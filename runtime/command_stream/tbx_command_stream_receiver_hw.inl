/*
 * Copyright (C) 2017-2018 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "hw_cmds.h"
#include "runtime/aub/aub_helper.h"
#include "runtime/aub_mem_dump/page_table_entry_bits.h"
#include "runtime/helpers/aligned_memory.h"
#include "runtime/helpers/debug_helpers.h"
#include "runtime/helpers/hw_helper.h"
#include "runtime/helpers/ptr_math.h"
#include "runtime/memory_manager/graphics_allocation.h"
#include "runtime/memory_manager/memory_banks.h"
#include "runtime/memory_manager/physical_address_allocator.h"
#include "runtime/command_stream/command_stream_receiver_with_aub_dump.h"
#include "runtime/os_interface/debug_settings_manager.h"
#include <cstring>

namespace OCLRT {

template <typename GfxFamily>
TbxCommandStreamReceiverHw<GfxFamily>::TbxCommandStreamReceiverHw(const HardwareInfo &hwInfoIn,
                                                                  ExecutionEnvironment &executionEnvironment)
    : BaseClass(hwInfoIn, executionEnvironment) {

    physicalAddressAllocator.reset(this->createPhysicalAddressAllocator());

    ppgtt = std::make_unique<std::conditional<is64bit, PML4, PDPE>::type>(physicalAddressAllocator.get());
    ggtt = std::make_unique<PDPE>(physicalAddressAllocator.get());

    for (auto &engineInfo : engineInfoTable) {
        engineInfo.pLRCA = nullptr;
        engineInfo.ggttLRCA = 0u;
        engineInfo.pGlobalHWStatusPage = nullptr;
        engineInfo.ggttHWSP = 0u;
        engineInfo.pRCS = nullptr;
        engineInfo.ggttRCS = 0u;
        engineInfo.sizeRCS = 0;
        engineInfo.tailRCS = 0;
    }
    auto debugDeviceId = DebugManager.flags.OverrideAubDeviceId.get();
    this->aubDeviceId = debugDeviceId == -1
                            ? hwInfoIn.capabilityTable.aubDeviceId
                            : static_cast<uint32_t>(debugDeviceId);
    this->stream = &tbxStream;
}

template <typename GfxFamily>
TbxCommandStreamReceiverHw<GfxFamily>::~TbxCommandStreamReceiverHw() {
    if (streamInitialized) {
        tbxStream.close();
    }

    for (auto &engineInfo : engineInfoTable) {
        alignedFree(engineInfo.pLRCA);
        gttRemap.unmap(engineInfo.pLRCA);
        engineInfo.pLRCA = nullptr;

        alignedFree(engineInfo.pGlobalHWStatusPage);
        gttRemap.unmap(engineInfo.pGlobalHWStatusPage);
        engineInfo.pGlobalHWStatusPage = nullptr;

        alignedFree(engineInfo.pRCS);
        gttRemap.unmap(engineInfo.pRCS);
        engineInfo.pRCS = nullptr;
    }
}

template <typename GfxFamily>
const AubMemDump::LrcaHelper &TbxCommandStreamReceiverHw<GfxFamily>::getCsTraits(EngineType engineType) {
    return *AUBFamilyMapper<GfxFamily>::csTraits[engineType];
}

template <typename GfxFamily>
void TbxCommandStreamReceiverHw<GfxFamily>::initEngineMMIO(EngineType engineType) {
    auto mmioList = AUBFamilyMapper<GfxFamily>::perEngineMMIO[engineType];

    DEBUG_BREAK_IF(!mmioList);
    for (auto &mmioPair : *mmioList) {
        tbxStream.writeMMIO(mmioPair.first, mmioPair.second);
    }
}

template <typename GfxFamily>
void TbxCommandStreamReceiverHw<GfxFamily>::initializeEngine(EngineType engineType) {
    auto mmioBase = getCsTraits(engineType).mmioBase;
    auto &engineInfo = engineInfoTable[engineType];

    this->initGlobalMMIO();
    initEngineMMIO(engineType);
    this->initAdditionalMMIO();

    // Global HW Status Page
    {
        const size_t sizeHWSP = 0x1000;
        const size_t alignHWSP = 0x1000;
        engineInfo.pGlobalHWStatusPage = alignedMalloc(sizeHWSP, alignHWSP);
        engineInfo.ggttHWSP = gttRemap.map(engineInfo.pGlobalHWStatusPage, sizeHWSP);
        auto physHWSP = ggtt->map(engineInfo.ggttHWSP, sizeHWSP, this->getGTTBits(), getMemoryBankForGtt());

        // Write our GHWSP
        AubGTTData data = {0};
        getGTTData(reinterpret_cast<void *>(physHWSP), data);
        AUB::reserveAddressGGTT(tbxStream, engineInfo.ggttHWSP, sizeHWSP, physHWSP, data);
        tbxStream.writeMMIO(mmioBase + 0x2080, engineInfo.ggttHWSP);
    }

    // Allocate the LRCA
    auto csTraits = getCsTraits(engineType);
    const size_t sizeLRCA = csTraits.sizeLRCA;
    const size_t alignLRCA = csTraits.alignLRCA;
    auto pLRCABase = alignedMalloc(sizeLRCA, alignLRCA);
    engineInfo.pLRCA = pLRCABase;

    // Initialize the LRCA to a known state
    csTraits.initialize(pLRCABase);

    // Reserve the RCS ring buffer
    engineInfo.sizeRCS = 0x4 * 0x1000;
    {
        const size_t alignRCS = 0x1000;
        engineInfo.pRCS = alignedMalloc(engineInfo.sizeRCS, alignRCS);
        engineInfo.ggttRCS = gttRemap.map(engineInfo.pRCS, engineInfo.sizeRCS);
        auto physRCS = ggtt->map(engineInfo.ggttRCS, engineInfo.sizeRCS, this->getGTTBits(), getMemoryBankForGtt());

        AubGTTData data = {0};
        getGTTData(reinterpret_cast<void *>(physRCS), data);
        AUB::reserveAddressGGTT(tbxStream, engineInfo.ggttRCS, engineInfo.sizeRCS, physRCS, data);
    }

    // Initialize the ring MMIO registers
    {
        uint32_t ringHead = 0x000;
        uint32_t ringTail = 0x000;
        auto ringBase = engineInfo.ggttRCS;
        auto ringCtrl = (uint32_t)((engineInfo.sizeRCS - 0x1000) | 1);
        csTraits.setRingHead(pLRCABase, ringHead);
        csTraits.setRingTail(pLRCABase, ringTail);
        csTraits.setRingBase(pLRCABase, ringBase);
        csTraits.setRingCtrl(pLRCABase, ringCtrl);
    }

    // Write our LRCA
    {
        engineInfo.ggttLRCA = gttRemap.map(engineInfo.pLRCA, sizeLRCA);
        auto lrcAddressPhys = ggtt->map(engineInfo.ggttLRCA, sizeLRCA, this->getGTTBits(), getMemoryBankForGtt());

        AubGTTData data = {0};
        getGTTData(reinterpret_cast<void *>(lrcAddressPhys), data);
        AUB::reserveAddressGGTT(tbxStream, engineInfo.ggttLRCA, sizeLRCA, lrcAddressPhys, data);
        AUB::addMemoryWrite(
            tbxStream,
            lrcAddressPhys,
            pLRCABase,
            sizeLRCA,
            this->getAddressSpace(csTraits.aubHintLRCA),
            csTraits.aubHintLRCA);
    }
}

template <typename GfxFamily>
CommandStreamReceiver *TbxCommandStreamReceiverHw<GfxFamily>::create(const HardwareInfo &hwInfoIn, bool withAubDump, ExecutionEnvironment &executionEnvironment) {
    TbxCommandStreamReceiverHw<GfxFamily> *csr;
    if (withAubDump) {
        csr = new CommandStreamReceiverWithAUBDump<TbxCommandStreamReceiverHw<GfxFamily>>(hwInfoIn, executionEnvironment);
    } else {
        csr = new TbxCommandStreamReceiverHw<GfxFamily>(hwInfoIn, executionEnvironment);
    }

    // Open our stream
    csr->stream->open(nullptr);

    // Add the file header.
    bool streamInitialized = csr->stream->init(AubMemDump::SteppingValues::A, csr->aubDeviceId);
    csr->streamInitialized = streamInitialized;

    return csr;
}

template <typename GfxFamily>
FlushStamp TbxCommandStreamReceiverHw<GfxFamily>::flush(BatchBuffer &batchBuffer, EngineType engineType, ResidencyContainer &allocationsForResidency, OsContext &osContext) {
    uint32_t mmioBase = getCsTraits(engineType).mmioBase;
    auto &engineInfo = engineInfoTable[engineType];

    if (!engineInfo.pLRCA) {
        initializeEngine(engineType);
        DEBUG_BREAK_IF(!engineInfo.pLRCA);
    }

    // Write our batch buffer
    auto pBatchBuffer = ptrOffset(batchBuffer.commandBufferAllocation->getUnderlyingBuffer(), batchBuffer.startOffset);
    auto currentOffset = batchBuffer.usedSize;
    DEBUG_BREAK_IF(currentOffset < batchBuffer.startOffset);
    auto sizeBatchBuffer = currentOffset - batchBuffer.startOffset;
    {
        auto physBatchBuffer = ppgtt->map(reinterpret_cast<uintptr_t>(pBatchBuffer), sizeBatchBuffer,
                                          getPPGTTAdditionalBits(batchBuffer.commandBufferAllocation),
                                          this->getMemoryBank(batchBuffer.commandBufferAllocation));

        AubHelperHw<GfxFamily> aubHelperHw(this->localMemoryEnabled);
        AUB::reserveAddressPPGTT(tbxStream, reinterpret_cast<uintptr_t>(pBatchBuffer), sizeBatchBuffer, physBatchBuffer,
                                 getPPGTTAdditionalBits(batchBuffer.commandBufferAllocation),
                                 aubHelperHw);

        AUB::addMemoryWrite(
            tbxStream,
            physBatchBuffer,
            pBatchBuffer,
            sizeBatchBuffer,
            this->getAddressSpace(AubMemDump::DataTypeHintValues::TraceBatchBufferPrimary),
            AubMemDump::DataTypeHintValues::TraceBatchBufferPrimary);
    }

    // Write allocations for residency
    processResidency(allocationsForResidency, osContext);

    // Add a batch buffer start to the RCS
    auto previousTail = engineInfo.tailRCS;
    {
        typedef typename GfxFamily::MI_LOAD_REGISTER_IMM MI_LOAD_REGISTER_IMM;
        typedef typename GfxFamily::MI_BATCH_BUFFER_START MI_BATCH_BUFFER_START;
        typedef typename GfxFamily::MI_NOOP MI_NOOP;

        auto pTail = ptrOffset(engineInfo.pRCS, engineInfo.tailRCS);
        auto ggttTail = ptrOffset(engineInfo.ggttRCS, engineInfo.tailRCS);

        auto sizeNeeded =
            sizeof(MI_BATCH_BUFFER_START) +
            sizeof(MI_NOOP) +
            sizeof(MI_LOAD_REGISTER_IMM);
        if (engineInfo.tailRCS + sizeNeeded >= engineInfo.sizeRCS) {
            // Pad the remaining ring with NOOPs
            auto sizeToWrap = engineInfo.sizeRCS - engineInfo.tailRCS;
            memset(pTail, 0, sizeToWrap);
            // write remaining ring
            auto physDumpStart = ggtt->map(ggttTail, sizeToWrap, this->getGTTBits(), getMemoryBankForGtt());
            AUB::addMemoryWrite(
                tbxStream,
                physDumpStart,
                pTail,
                sizeToWrap,
                this->getAddressSpace(AubMemDump::DataTypeHintValues::TraceCommandBuffer),
                AubMemDump::DataTypeHintValues::TraceCommandBuffer);
            previousTail = 0;
            engineInfo.tailRCS = 0;
            pTail = engineInfo.pRCS;
        } else if (engineInfo.tailRCS == 0) {
            // Add a LRI if this is our first submission
            auto lri = MI_LOAD_REGISTER_IMM::sInit();
            lri.setRegisterOffset(mmioBase + 0x2244);
            lri.setDataDword(0x00010000);
            *(MI_LOAD_REGISTER_IMM *)pTail = lri;
            pTail = ((MI_LOAD_REGISTER_IMM *)pTail) + 1;
        }

        // Add our BBS
        auto bbs = MI_BATCH_BUFFER_START::sInit();
        bbs.setBatchBufferStartAddressGraphicsaddress472(AUB::ptrToPPGTT(pBatchBuffer));
        bbs.setAddressSpaceIndicator(MI_BATCH_BUFFER_START::ADDRESS_SPACE_INDICATOR_PPGTT);
        *(MI_BATCH_BUFFER_START *)pTail = bbs;
        pTail = ((MI_BATCH_BUFFER_START *)pTail) + 1;

        // Add a NOOP as our tail needs to be aligned to a QWORD
        *(MI_NOOP *)pTail = MI_NOOP::sInit();
        pTail = ((MI_NOOP *)pTail) + 1;

        // Compute our new ring tail.
        engineInfo.tailRCS = (uint32_t)ptrDiff(pTail, engineInfo.pRCS);

        // Only dump the new commands
        auto ggttDumpStart = ptrOffset(engineInfo.ggttRCS, previousTail);
        auto dumpStart = ptrOffset(engineInfo.pRCS, previousTail);
        auto dumpLength = engineInfo.tailRCS - previousTail;

        // write RCS
        auto physDumpStart = ggtt->map(ggttDumpStart, dumpLength, this->getGTTBits(), getMemoryBankForGtt());
        AUB::addMemoryWrite(
            tbxStream,
            physDumpStart,
            dumpStart,
            dumpLength,
            this->getAddressSpace(AubMemDump::DataTypeHintValues::TraceCommandBuffer),
            AubMemDump::DataTypeHintValues::TraceCommandBuffer);

        // update the RCS mmio tail in the LRCA
        auto physLRCA = ggtt->map(engineInfo.ggttLRCA, sizeof(engineInfo.tailRCS), this->getGTTBits(), getMemoryBankForGtt());
        AUB::addMemoryWrite(
            tbxStream,
            physLRCA + 0x101c,
            &engineInfo.tailRCS,
            sizeof(engineInfo.tailRCS),
            this->getAddressSpace(AubMemDump::DataTypeHintValues::TraceNotype));

        DEBUG_BREAK_IF(engineInfo.tailRCS >= engineInfo.sizeRCS);
    }

    // Submit our execlist by submitting to the execlist submit ports
    {
        typename AUB::MiContextDescriptorReg contextDescriptor = {{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}};

        contextDescriptor.sData.Valid = true;
        contextDescriptor.sData.ForcePageDirRestore = false;
        contextDescriptor.sData.ForceRestore = false;
        contextDescriptor.sData.Legacy = true;
        contextDescriptor.sData.FaultSupport = 0;
        contextDescriptor.sData.PrivilegeAccessOrPPGTT = true;
        contextDescriptor.sData.ADor64bitSupport = AUB::Traits::addressingBits > 32;

        auto ggttLRCA = engineInfo.ggttLRCA;
        contextDescriptor.sData.LogicalRingCtxAddress = ggttLRCA / 4096;
        contextDescriptor.sData.ContextID = 0;

        submitLRCA(engineType, contextDescriptor);
    }

    pollForCompletion(engineType);
    return 0;
}

template <typename GfxFamily>
void TbxCommandStreamReceiverHw<GfxFamily>::submitLRCA(EngineType engineType, const MiContextDescriptorReg &contextDescriptor) {
    auto mmioBase = getCsTraits(engineType).mmioBase;
    tbxStream.writeMMIO(mmioBase + 0x2230, 0);
    tbxStream.writeMMIO(mmioBase + 0x2230, 0);
    tbxStream.writeMMIO(mmioBase + 0x2230, contextDescriptor.ulData[1]);
    tbxStream.writeMMIO(mmioBase + 0x2230, contextDescriptor.ulData[0]);
}

template <typename GfxFamily>
void TbxCommandStreamReceiverHw<GfxFamily>::pollForCompletion(EngineType engineType) {
    typedef typename AubMemDump::CmdServicesMemTraceRegisterPoll CmdServicesMemTraceRegisterPoll;

    auto mmioBase = getCsTraits(engineType).mmioBase;
    bool pollNotEqual = false;
    tbxStream.registerPoll(
        mmioBase + 0x2234, //EXECLIST_STATUS
        0x100,
        0x100,
        pollNotEqual,
        CmdServicesMemTraceRegisterPoll::TimeoutActionValues::Abort);
}

template <typename GfxFamily>
bool TbxCommandStreamReceiverHw<GfxFamily>::writeMemory(GraphicsAllocation &gfxAllocation) {
    auto cpuAddress = gfxAllocation.getUnderlyingBuffer();
    auto gpuAddress = gfxAllocation.getGpuAddress();
    auto size = gfxAllocation.getUnderlyingBufferSize();

    if (size == 0)
        return false;

    AubHelperHw<GfxFamily> aubHelperHw(this->localMemoryEnabled);

    PageWalker walker = [&](uint64_t physAddress, size_t size, size_t offset, uint64_t entryBits) {
        AUB::reserveAddressGGTTAndWriteMmeory(tbxStream, static_cast<uintptr_t>(gpuAddress), cpuAddress, physAddress, size, offset, getPPGTTAdditionalBits(&gfxAllocation),
                                              aubHelperHw);
    };

    ppgtt->pageWalk(static_cast<uintptr_t>(gpuAddress), size, 0, getPPGTTAdditionalBits(&gfxAllocation), walker, this->getMemoryBank(&gfxAllocation));
    return true;
}

template <typename GfxFamily>
void TbxCommandStreamReceiverHw<GfxFamily>::processResidency(ResidencyContainer &allocationsForResidency, OsContext &osContext) {
    for (auto &gfxAllocation : allocationsForResidency) {
        if (!writeMemory(*gfxAllocation)) {
            DEBUG_BREAK_IF(!(gfxAllocation->getUnderlyingBufferSize() == 0));
        }
        gfxAllocation->residencyTaskCount[this->deviceIndex] = this->taskCount + 1;
    }
}

template <typename GfxFamily>
void TbxCommandStreamReceiverHw<GfxFamily>::makeCoherent(GraphicsAllocation &gfxAllocation) {
    auto cpuAddress = gfxAllocation.getUnderlyingBuffer();
    auto gpuAddress = gfxAllocation.getGpuAddress();
    auto length = gfxAllocation.getUnderlyingBufferSize();

    if (length) {
        PageWalker walker = [&](uint64_t physAddress, size_t size, size_t offset, uint64_t entryBits) {
            DEBUG_BREAK_IF(offset > length);
            tbxStream.readMemory(physAddress, ptrOffset(cpuAddress, offset), size);
        };
        ppgtt->pageWalk(static_cast<uintptr_t>(gpuAddress), length, 0, 0, walker, this->getMemoryBank(&gfxAllocation));
    }
}

template <typename GfxFamily>
void TbxCommandStreamReceiverHw<GfxFamily>::waitBeforeMakingNonResidentWhenRequired() {
    auto allocation = this->getTagAllocation();
    UNRECOVERABLE_IF(allocation == nullptr);

    while (*this->getTagAddress() < this->latestFlushedTaskCount) {
        this->makeCoherent(*allocation);
    }
}

template <typename GfxFamily>
uint64_t TbxCommandStreamReceiverHw<GfxFamily>::getPPGTTAdditionalBits(GraphicsAllocation *gfxAllocation) {
    return BIT(PageTableEntry::presentBit) | BIT(PageTableEntry::writableBit) | BIT(PageTableEntry::userSupervisorBit);
}

template <typename GfxFamily>
void TbxCommandStreamReceiverHw<GfxFamily>::getGTTData(void *memory, AubGTTData &data) {
    data.present = true;
    data.localMemory = false;
}

template <typename GfxFamily>
uint32_t TbxCommandStreamReceiverHw<GfxFamily>::getMemoryBankForGtt() const {
    return MemoryBanks::getBank(this->deviceIndex);
}
} // namespace OCLRT
