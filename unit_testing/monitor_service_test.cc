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

#include "driver/monitor_service.h"

#include "mock_objects.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

using ::testing::_;
using ::testing::Return;

namespace {
    const std::set<std::string> node_keys = { "any.node.domain" };
    const std::chrono::milliseconds failure_detection_time(10);
    const std::chrono::seconds failure_detection_timeout(5);
    const std::chrono::milliseconds failure_detection_interval(100);
    const int failure_detection_count = 3;
    const std::chrono::milliseconds monitor_disposal_time(200);
}

class MonitorServiceTest : public testing::Test {
protected:
    std::shared_ptr<HOST_INFO> host;
    std::shared_ptr<MOCK_MONITOR> mock_monitor;
    std::shared_ptr<MOCK_MONITOR_THREAD_CONTAINER> mock_thread_container;
    std::shared_ptr<MONITOR_SERVICE> monitor_service;
    
    static void SetUpTestSuite() {}

    static void TearDownTestSuite() {}

    void SetUp() override {
        host = std::make_shared<HOST_INFO>("host", 1234);
        mock_thread_container = std::make_shared<MOCK_MONITOR_THREAD_CONTAINER>();
        monitor_service = std::make_shared<MONITOR_SERVICE>(mock_thread_container);
        mock_monitor = std::make_shared<MOCK_MONITOR>(host, monitor_disposal_time, nullptr);
    }

    void TearDown() override {
        monitor_service->release_resources();
        mock_thread_container->release_resources();
    }
};

TEST_F(MonitorServiceTest, StartMonitoring) {
    EXPECT_CALL(*mock_thread_container, create_monitor(_, _, _, _, _))
        .WillOnce(Return(mock_monitor));

    EXPECT_CALL(*mock_monitor, start_monitoring(_)).Times(1);
    EXPECT_CALL(*mock_monitor, run(_)).Times(1);

    auto context = monitor_service->start_monitoring(
        nullptr,
        nullptr,
        node_keys,
        host,
        failure_detection_time,
        failure_detection_timeout,
        failure_detection_interval,
        failure_detection_count,
        monitor_disposal_time);
    EXPECT_NE(nullptr, context);
}

TEST_F(MonitorServiceTest, StartMonitoringCalledMultipleTimes) {
    EXPECT_CALL(*mock_thread_container, create_monitor(_, _, _, _, _))
        .WillOnce(Return(mock_monitor));

    const int runs = 5;

    EXPECT_CALL(*mock_monitor, start_monitoring(_)).Times(runs);
    EXPECT_CALL(*mock_monitor, run(_)).Times(1);

    for (int i = 0; i < runs; i++) {
        auto context = monitor_service->start_monitoring(
            nullptr,
            nullptr,
            node_keys,
            host,
            failure_detection_time,
            failure_detection_timeout,
            failure_detection_interval,
            failure_detection_count,
            monitor_disposal_time);
        EXPECT_NE(nullptr, context);
    }
}

TEST_F(MonitorServiceTest, StopMonitoring) {
    EXPECT_CALL(*mock_thread_container, create_monitor(_, _, _, _, _))
        .WillOnce(Return(mock_monitor));

    EXPECT_CALL(*mock_monitor, start_monitoring(_)).Times(1);
    EXPECT_CALL(*mock_monitor, run(_)).Times(1);

    auto context = monitor_service->start_monitoring(
        nullptr,
        nullptr,
        node_keys,
        host,
        failure_detection_time,
        failure_detection_timeout,
        failure_detection_interval,
        failure_detection_count,
        monitor_disposal_time);
    EXPECT_NE(nullptr, context);

    EXPECT_CALL(*mock_monitor, stop_monitoring(context)).Times(1);

    monitor_service->stop_monitoring(context);
}

TEST_F(MonitorServiceTest, StopMonitoringCalledTwice) {
    EXPECT_CALL(*mock_thread_container, create_monitor(_, _, _, _, _))
        .WillOnce(Return(mock_monitor));

    EXPECT_CALL(*mock_monitor, start_monitoring(_)).Times(1);
    EXPECT_CALL(*mock_monitor, run(_)).Times(1);

    auto context = monitor_service->start_monitoring(
        nullptr,
        nullptr,
        node_keys,
        host,
        failure_detection_time,
        failure_detection_timeout,
        failure_detection_interval,
        failure_detection_count,
        monitor_disposal_time);
    EXPECT_NE(nullptr, context);

    EXPECT_CALL(*mock_monitor, stop_monitoring(context)).Times(2);

    monitor_service->stop_monitoring(context);
    monitor_service->stop_monitoring(context);
}

TEST_F(MonitorServiceTest, EmptyNodeKeys) {
    std::set<std::string> keys = { };

    EXPECT_THROW(
        monitor_service->start_monitoring(
            nullptr,
            nullptr,
            keys,
            host,
            failure_detection_time,
            failure_detection_timeout,
            failure_detection_interval,
            failure_detection_count,
            monitor_disposal_time),
        std::invalid_argument);
}
