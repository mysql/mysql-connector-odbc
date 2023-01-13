// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License, version 2.0
// (GPLv2), as published by the Free Software Foundation, with the
// following additional permissions:
//
// This program is distributed with certain software that is licensed
// under separate terms, as designated in a particular file or component
// or in the license documentation. Without limiting your rights under
// the GPLv2, the authors of this program hereby grant you an additional
// permission to link the program and your derivative works with the
// separately licensed software that they have included with the program.
//
// Without limiting the foregoing grant of rights under the GPLv2 and
// additional permission as to separately licensed software, this
// program is also subject to the Universal FOSS Exception, version 1.0,
// a copy of which can be found along with its FAQ at
// http://oss.oracle.com/licenses/universal-foss-exception.
//
// This program is distributed in the hope that it will be useful, but
// WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
// See the GNU General Public License, version 2.0, for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program. If not, see
// http://www.gnu.org/licenses/gpl-2.0.html.

#include "driver/monitor_thread_container.h"

#include "test_utils.h"
#include "mock_objects.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

using ::testing::_;
using ::testing::Return;

namespace {
    std::string node_key = "any.node.domain";
    std::set<std::string> node_keys = { node_key };
    const std::chrono::milliseconds failure_detection_time(10);
    const std::chrono::seconds failure_detection_timeout(5);
    const std::chrono::milliseconds failure_detection_interval(30);
    const int failure_detection_count = 3;
    const std::chrono::milliseconds monitor_disposal_time(200);
}

class MonitorThreadContainerTest : public testing::Test {
protected:
    std::shared_ptr<HOST_INFO> host;
    std::shared_ptr<MONITOR_THREAD_CONTAINER> thread_container;
    std::shared_ptr<MONITOR_SERVICE> service;

    static void SetUpTestSuite() {}

    static void TearDownTestSuite() {}

    void SetUp() override {
        host = std::make_shared<HOST_INFO>("host", 1234);
        thread_container = MONITOR_THREAD_CONTAINER::get_instance();
        service = std::make_shared<MONITOR_SERVICE>(thread_container);
    }

    void TearDown() override {
        service->release_resources();
    }
};

TEST_F(MonitorThreadContainerTest, GetInstance) {
    const auto thread_container_same = MONITOR_THREAD_CONTAINER::get_instance();
    EXPECT_TRUE(thread_container_same == thread_container);
}

TEST_F(MonitorThreadContainerTest, MultipleNodeKeys) {
    std::set<std::string> node_keys1 = { "nodeOne.domain", "nodeTwo.domain" };
    std::set<std::string> node_keys2 = { "nodeTwo.domain" };

    auto monitor1 = thread_container->get_or_create_monitor(
        node_keys1, host, failure_detection_timeout, monitor_disposal_time, nullptr);
    EXPECT_NE(nullptr, monitor1);

    // Should return the same monitor again because the first call to get_or_create_monitor()
    // mapped the monitor to both "nodeOne.domain" and "nodeTwo.domain".
    auto monitor2 = thread_container->get_or_create_monitor(
        node_keys2, host, failure_detection_timeout, monitor_disposal_time, nullptr);
    EXPECT_NE(nullptr, monitor2);

    EXPECT_TRUE(monitor1 == monitor2);
}

TEST_F(MonitorThreadContainerTest, DifferentNodeKeys) {
    std::set<std::string> keys = { "nodeNEW.domain" };

    auto monitor1 = thread_container->get_or_create_monitor(
        keys, host, failure_detection_timeout, monitor_disposal_time, nullptr);
    EXPECT_NE(nullptr, monitor1);

    auto monitor2 = thread_container->get_or_create_monitor(
        keys, host, failure_detection_timeout, monitor_disposal_time, nullptr);
    EXPECT_NE(nullptr, monitor2);

    // Monitors should be the same because both calls to get_or_create_monitor()
    // used the same node keys.
    EXPECT_TRUE(monitor1 == monitor2);

    auto monitor3 = thread_container->get_or_create_monitor(
        node_keys, host, failure_detection_timeout, monitor_disposal_time, nullptr);
    EXPECT_NE(nullptr, monitor3);

    // Last monitor should be different because it has a different node key.
    EXPECT_TRUE(monitor1 != monitor3);
    EXPECT_TRUE(monitor2 != monitor3);
}

TEST_F(MonitorThreadContainerTest, SameKeysInDifferentNodeKeys) {
    std::set<std::string> keys1 = { "nodeA" };
    std::set<std::string> keys2 = { "nodeA", "nodeB" };
    std::set<std::string> keys3 = { "nodeB" };

    auto monitor1 = thread_container->get_or_create_monitor(
        keys1, host, failure_detection_timeout, monitor_disposal_time, nullptr);
    EXPECT_NE(nullptr, monitor1);

    auto monitor2 = thread_container->get_or_create_monitor(
        keys2, host, failure_detection_timeout, monitor_disposal_time, nullptr);
    EXPECT_NE(nullptr, monitor2);

    // Monitors should be the same because both sets of keys have "nodeA".
    EXPECT_TRUE(monitor1 == monitor2);

    auto monitor3 = thread_container->get_or_create_monitor(
        keys3, host, failure_detection_timeout, monitor_disposal_time, nullptr);
    EXPECT_NE(nullptr, monitor3);

    // Last monitor should be also be the same because the 2nd call to get_or_create_monitor()
    // mapped the monitor to "nodeB" and the 3rd call used "nodeB".
    EXPECT_TRUE(monitor1 == monitor3);
    EXPECT_TRUE(monitor2 == monitor3);
}

TEST_F(MonitorThreadContainerTest, PopulateMonitorMap) {
    std::set<std::string> keys = { "nodeA", "nodeB", "nodeC", "nodeD" };
    auto monitor = thread_container->get_or_create_monitor(
        keys, host, failure_detection_timeout, monitor_disposal_time, nullptr);

    // Check that we now have mappings for all the keys.
    for (auto it = keys.begin(); it != keys.end(); ++it) {
        std::string node = *it;
        auto get_monitor = thread_container->get_monitor(node);
        EXPECT_NE(nullptr, get_monitor);

        EXPECT_TRUE(get_monitor == monitor);
    }
}

TEST_F(MonitorThreadContainerTest, RemoveMonitorMapping) {
    std::set<std::string> keys1 = { "nodeA", "nodeB", "nodeC", "nodeD" };
    auto monitor1 = thread_container->get_or_create_monitor(
        keys1, host, failure_detection_timeout, monitor_disposal_time, nullptr);

    std::set<std::string> keys2 = { "nodeE", "nodeF", "nodeG", "nodeH" };
    auto monitor2 = thread_container->get_or_create_monitor(
        keys2, host, failure_detection_timeout, std::chrono::milliseconds(100), nullptr);

    // This should remove the mappings for keys1 but not keys2.
    thread_container->reset_resource(monitor1);

    // Check that we no longer have any mappings for keys1.
    for (auto it = keys1.begin(); it != keys1.end(); ++it) {
        std::string node = *it;
        auto get_monitor = thread_container->get_monitor(node);
        EXPECT_THAT(get_monitor, nullptr);
    }

    // Check that we still have all the mapping for keys2.
    for (auto it = keys2.begin(); it != keys2.end(); ++it) {
        std::string node = *it;
        auto get_monitor = thread_container->get_monitor(node);
        EXPECT_NE(nullptr, get_monitor);

        EXPECT_TRUE(get_monitor == monitor2);
    }
}

TEST_F(MonitorThreadContainerTest, AvailableMonitorsQueue) {
    std::set<std::string> keys = { "nodeA", "nodeB", "nodeC", "nodeD" };
    auto mock_thread_container = std::make_shared<MOCK_MONITOR_THREAD_CONTAINER>();
    auto monitor_service = std::make_shared<MONITOR_SERVICE>(mock_thread_container);
    auto mock_monitor1 = std::make_shared<MOCK_MONITOR>(host, monitor_disposal_time, nullptr);
    auto mock_monitor2 = std::make_shared<MOCK_MONITOR>(host, monitor_disposal_time, nullptr);

    // While we have three get_or_create_monitor() calls, we only call create_monitor() twice.
    EXPECT_CALL(*mock_thread_container, create_monitor(_, _, _, _, _))
        .WillOnce(Return(mock_monitor1))
        .WillOnce(Return(mock_monitor2));

    EXPECT_CALL(*mock_monitor1, is_stopped())
        .WillOnce(Return(false))
        .WillOnce(Return(true));

    EXPECT_CALL(*mock_monitor1, run(_));
    
    // This first call should create the monitor.
    auto monitor1 = mock_thread_container->get_or_create_monitor(
        keys, host, failure_detection_timeout, monitor_disposal_time, nullptr);
    EXPECT_NE(nullptr, monitor1);

    mock_thread_container->add_task(monitor1, monitor_service);

    EXPECT_TRUE(TEST_UTILS::has_monitor(mock_thread_container, *keys.begin()));
    EXPECT_TRUE(TEST_UTILS::has_task(mock_thread_container, monitor1));

    // This should remove the node key mappings and add the monitor to the available monitors queue.
    mock_thread_container->reset_resource(monitor1);

    EXPECT_TRUE(TEST_UTILS::has_available_monitor(mock_thread_container));
    auto available_monitor = TEST_UTILS::get_available_monitor(mock_thread_container);
    EXPECT_NE(nullptr, available_monitor);

    EXPECT_TRUE(monitor1 == available_monitor);

    // This second call should get the monitor from the available monitors queue
    // instead of creating a new monitor.
    auto monitor2 = mock_thread_container->get_or_create_monitor(
        keys, host, failure_detection_timeout, monitor_disposal_time, nullptr);
    EXPECT_NE(nullptr, monitor2);

    EXPECT_TRUE(monitor2 == available_monitor);

    // Adding monitor back to available monitors queue.
    mock_thread_container->reset_resource(monitor2);

    // This call will discard the available monitor because it is now stopped
    // and create a new monitor.
    auto monitor3 = mock_thread_container->get_or_create_monitor(
        { node_key }, host, failure_detection_timeout, monitor_disposal_time, nullptr);
    EXPECT_NE(nullptr, monitor3);

    EXPECT_NE(monitor1, monitor3);

    EXPECT_FALSE(TEST_UTILS::has_any_tasks(mock_thread_container));

    monitor_service->release_resources();
}

TEST_F(MonitorThreadContainerTest, PopulateAndRemoveMappings) {
    auto context = service->start_monitoring(
        nullptr,
        nullptr,
        node_keys,
        host,
        failure_detection_time,
        failure_detection_timeout,
        failure_detection_interval,
        failure_detection_count,
        monitor_disposal_time);

    EXPECT_TRUE(TEST_UTILS::has_monitor(thread_container, node_key));
    EXPECT_TRUE(TEST_UTILS::has_task(thread_container, thread_container->get_monitor(node_key)));

    service->stop_monitoring(context);

    std::this_thread::sleep_for(std::chrono::milliseconds(1500));

    EXPECT_FALSE(TEST_UTILS::has_monitor(thread_container, node_key));
    EXPECT_FALSE(TEST_UTILS::has_any_tasks(thread_container));
}
