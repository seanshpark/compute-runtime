/*
 * Copyright (C) 2017-2018 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/os_interface/windows/os_context_win.h"
#include "unit_tests/os_interface/windows/os_interface_win_tests.h"
#include "unit_tests/os_interface/windows/wddm_fixture.h"

TEST_F(OsInterfaceTest, givenOsInterfaceWithoutWddmWhenGetHwContextIdIsCalledThenReturnsZero) {
    auto retVal = osInterface->getHwContextId();
    EXPECT_EQ(0u, retVal);
}

TEST_F(OsInterfaceTest, GivenWindowsWhenOsSupportFor64KBpagesIsBeingQueriedThenTrueIsReturned) {
    EXPECT_TRUE(OSInterface::are64kbPagesEnabled());
}

TEST_F(OsInterfaceTest, GivenWindowsWhenCreateEentIsCalledThenValidEventHandleIsReturned) {
    auto ev = osInterface->get()->createEvent(NULL, TRUE, FALSE, "DUMMY_EVENT_NAME");
    EXPECT_NE(nullptr, ev);
    auto ret = osInterface->get()->closeHandle(ev);
    EXPECT_EQ(TRUE, ret);
}

TEST(OsContextTest, givenWddmWhenCreateOsContextBeforeInitWddmThenOsContextIsNotInitialized) {
    auto wddm = new WddmMock;
    OSInterface osInterface;
    osInterface.get()->setWddm(wddm);
    EXPECT_THROW(auto osContext = std::make_unique<OsContext>(&osInterface, 0u), std::exception);
}

TEST(OsContextTest, givenWddmWhenCreateOsContextAfterInitWddmThenOsContextIsInitialized) {
    auto wddm = new WddmMock;
    OSInterface osInterface;
    osInterface.get()->setWddm(wddm);
    wddm->init();
    auto osContext = std::make_unique<OsContext>(&osInterface, 0u);
    EXPECT_NE(nullptr, osContext->get());
    EXPECT_TRUE(osContext->get()->isInitialized());
    EXPECT_EQ(osContext->get()->getWddm(), wddm);
}

TEST(OsContextTest, whenCreateOsContextWithoutOsInterfaceThenOsContextImplIsNotAvailable) {
    auto osContext = std::make_unique<OsContext>(nullptr, 0u);
    EXPECT_EQ(nullptr, osContext->get());
}
