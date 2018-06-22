/*
 * Copyright (c) 2017 - 2018, Intel Corporation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 */

#include "hw_cmds.h"
#include "hw_info_kbl.h"
#include "runtime/aub_mem_dump/aub_services.h"
#include "runtime/helpers/engine_node.h"
#include "runtime/memory_manager/memory_constants.h"

namespace OCLRT {

const char *HwMapper<IGFX_KABYLAKE>::abbreviation = "kbl";

bool isSimulationKBL(unsigned short deviceId) {
    return false;
};

const PLATFORM KBL::platform = {
    IGFX_KABYLAKE,
    PCH_UNKNOWN,
    IGFX_GEN9_CORE,
    IGFX_GEN9_CORE,
    PLATFORM_NONE, // default init
    0,             // usDeviceID
    0,             // usRevId. 0 sets the stepping to A0
    0,             // usDeviceID_PCH
    0,             // usRevId_PCH
    GTTYPE_UNDEFINED};

const RuntimeCapabilityTable KBL::capabilityTable{
    0,
    83.333,
    21,
    true,
    true,
    true,
    true,
    true,  // ftrSupportsVmeAvcTextureSampler
    false, // ftrSupportsVmeAvcPreemption
    false, // ftrRenderCompressedBuffers
    false, // ftrRenderCompressedImages
    PreemptionMode::MidThread,
    {true, false},
    &isSimulationKBL,
    true,
    true,                           // forceStatelessCompilationFor32Bit
    {false, 0, false, 0, false, 0}, // KmdNotifyProperties
    true,                           // ftr64KBpages
    EngineType::ENGINE_RCS,         // defaultEngineType
    MemoryConstants::pageSize,      //requiredPreemptionSurfaceSize
    true,                           // isCore
    true,                           // sourceLevelDebuggerSupported
    CmdServicesMemTraceVersion::DeviceValues::Kbl,
    0}; // extraQuantityThreadsPerEU

const HardwareInfo KBL_1x2x6::hwInfo = {
    &KBL::platform,
    &emptySkuTable,
    &emptyWaTable,
    &KBL_1x2x6::gtSystemInfo,
    KBL::capabilityTable,
};
GT_SYSTEM_INFO KBL_1x2x6::gtSystemInfo = {0};
void KBL_1x2x6::setupGtSystemInfo(GT_SYSTEM_INFO *gtSysInfo) {
    gtSysInfo->EUCount = 11;
    gtSysInfo->ThreadCount = 11 * KBL::threadsPerEu;
    gtSysInfo->SliceCount = 1;
    gtSysInfo->SubSliceCount = 2;
    gtSysInfo->L3CacheSizeInKb = 384;
    gtSysInfo->L3BankCount = 2;
    gtSysInfo->MaxFillRate = 8;
    gtSysInfo->TotalVsThreads = 336;
    gtSysInfo->TotalHsThreads = 336;
    gtSysInfo->TotalDsThreads = 336;
    gtSysInfo->TotalGsThreads = 336;
    gtSysInfo->TotalPsThreadsWindowerRange = 64;
    gtSysInfo->CsrSizeInMb = 8;
    gtSysInfo->MaxEuPerSubSlice = KBL::maxEuPerSubslice;
    gtSysInfo->MaxSlicesSupported = KBL::maxSlicesSupported;
    gtSysInfo->MaxSubSlicesSupported = KBL::maxSubslicesSupported;
    gtSysInfo->IsL3HashModeEnabled = false;
    gtSysInfo->IsDynamicallyPopulated = false;
};

const HardwareInfo KBL_1x3x6::hwInfo = {
    &KBL::platform,
    &emptySkuTable,
    &emptyWaTable,
    &KBL_1x3x6::gtSystemInfo,
    KBL::capabilityTable,
};
GT_SYSTEM_INFO KBL_1x3x6::gtSystemInfo = {0};
void KBL_1x3x6::setupGtSystemInfo(GT_SYSTEM_INFO *gtSysInfo) {
    gtSysInfo->EUCount = 17;
    gtSysInfo->ThreadCount = 17 * KBL::threadsPerEu;
    gtSysInfo->SliceCount = 1;
    gtSysInfo->SubSliceCount = 3;
    gtSysInfo->L3CacheSizeInKb = 768;
    gtSysInfo->L3BankCount = 4;
    gtSysInfo->MaxFillRate = 8;
    gtSysInfo->TotalVsThreads = 336;
    gtSysInfo->TotalHsThreads = 336;
    gtSysInfo->TotalDsThreads = 336;
    gtSysInfo->TotalGsThreads = 336;
    gtSysInfo->TotalPsThreadsWindowerRange = 64;
    gtSysInfo->CsrSizeInMb = 8;
    gtSysInfo->MaxEuPerSubSlice = KBL::maxEuPerSubslice;
    gtSysInfo->MaxSlicesSupported = KBL::maxSlicesSupported;
    gtSysInfo->MaxSubSlicesSupported = KBL::maxSubslicesSupported;
    gtSysInfo->IsL3HashModeEnabled = false;
    gtSysInfo->IsDynamicallyPopulated = false;
};

const HardwareInfo KBL_1x3x8::hwInfo = {
    &KBL::platform,
    &emptySkuTable,
    &emptyWaTable,
    &KBL_1x3x8::gtSystemInfo,
    KBL::capabilityTable,
};
GT_SYSTEM_INFO KBL_1x3x8::gtSystemInfo = {0};
void KBL_1x3x8::setupGtSystemInfo(GT_SYSTEM_INFO *gtSysInfo) {
    gtSysInfo->EUCount = 23;
    gtSysInfo->ThreadCount = 23 * KBL::threadsPerEu;
    gtSysInfo->SliceCount = 1;
    gtSysInfo->SubSliceCount = 3;
    gtSysInfo->L3CacheSizeInKb = 768;
    gtSysInfo->L3BankCount = 4;
    gtSysInfo->MaxFillRate = 8;
    gtSysInfo->TotalVsThreads = 336;
    gtSysInfo->TotalHsThreads = 336;
    gtSysInfo->TotalDsThreads = 336;
    gtSysInfo->TotalGsThreads = 336;
    gtSysInfo->TotalPsThreadsWindowerRange = 64;
    gtSysInfo->CsrSizeInMb = 8;
    gtSysInfo->MaxEuPerSubSlice = KBL::maxEuPerSubslice;
    gtSysInfo->MaxSlicesSupported = KBL::maxSlicesSupported;
    gtSysInfo->MaxSubSlicesSupported = KBL::maxSubslicesSupported;
    gtSysInfo->IsL3HashModeEnabled = false;
    gtSysInfo->IsDynamicallyPopulated = false;
};

const HardwareInfo KBL_2x3x8::hwInfo = {
    &KBL::platform,
    &emptySkuTable,
    &emptyWaTable,
    &KBL_2x3x8::gtSystemInfo,
    KBL::capabilityTable,
};
GT_SYSTEM_INFO KBL_2x3x8::gtSystemInfo = {0};
void KBL_2x3x8::setupGtSystemInfo(GT_SYSTEM_INFO *gtSysInfo) {
    gtSysInfo->EUCount = 47;
    gtSysInfo->ThreadCount = 47 * KBL::threadsPerEu;
    gtSysInfo->SliceCount = 2;
    gtSysInfo->SubSliceCount = 6;
    gtSysInfo->L3CacheSizeInKb = 1536;
    gtSysInfo->L3BankCount = 8;
    gtSysInfo->MaxFillRate = 16;
    gtSysInfo->TotalVsThreads = 336;
    gtSysInfo->TotalHsThreads = 336;
    gtSysInfo->TotalDsThreads = 336;
    gtSysInfo->TotalGsThreads = 336;
    gtSysInfo->TotalPsThreadsWindowerRange = 64;
    gtSysInfo->CsrSizeInMb = 8;
    gtSysInfo->MaxEuPerSubSlice = KBL::maxEuPerSubslice;
    gtSysInfo->MaxSlicesSupported = KBL::maxSlicesSupported;
    gtSysInfo->MaxSubSlicesSupported = KBL::maxSubslicesSupported;
    gtSysInfo->IsL3HashModeEnabled = false;
    gtSysInfo->IsDynamicallyPopulated = false;
};

const HardwareInfo KBL_3x3x8::hwInfo = {
    &KBL::platform,
    &emptySkuTable,
    &emptyWaTable,
    &KBL_3x3x8::gtSystemInfo,
    KBL::capabilityTable,
};
GT_SYSTEM_INFO KBL_3x3x8::gtSystemInfo = {0};
void KBL_3x3x8::setupGtSystemInfo(GT_SYSTEM_INFO *gtSysInfo) {
    gtSysInfo->EUCount = 71;
    gtSysInfo->ThreadCount = 71 * KBL::threadsPerEu;
    gtSysInfo->SliceCount = 3;
    gtSysInfo->SubSliceCount = 9;
    gtSysInfo->L3CacheSizeInKb = 2304;
    gtSysInfo->L3BankCount = 12;
    gtSysInfo->MaxFillRate = 23;
    gtSysInfo->TotalVsThreads = 336;
    gtSysInfo->TotalHsThreads = 336;
    gtSysInfo->TotalDsThreads = 336;
    gtSysInfo->TotalGsThreads = 336;
    gtSysInfo->TotalPsThreadsWindowerRange = 64;
    gtSysInfo->CsrSizeInMb = 8;
    gtSysInfo->MaxEuPerSubSlice = KBL::maxEuPerSubslice;
    gtSysInfo->MaxSlicesSupported = KBL::maxSlicesSupported;
    gtSysInfo->MaxSubSlicesSupported = KBL::maxSubslicesSupported;
    gtSysInfo->IsL3HashModeEnabled = false;
    gtSysInfo->IsDynamicallyPopulated = false;
};

const HardwareInfo KBL::hwInfo = KBL_1x3x6::hwInfo;
void (*KBL::setupGtSystemInfo)(GT_SYSTEM_INFO *) = KBL_1x3x6::setupGtSystemInfo;
} // namespace OCLRT
