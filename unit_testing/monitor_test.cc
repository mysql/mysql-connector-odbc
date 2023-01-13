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

#include "driver/monitor.h"
#include "driver/driver.h"

#include "test_utils.h"
#include "mock_objects.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

using ::testing::_;
using ::testing::AtLeast;
using ::testing::Eq;
using ::testing::MatcherCast;
using ::testing::Pointee;
using ::testing::Return;
using ::testing::SafeMatcherCast;

namespace {
    const std::set<std::string> node_keys = { "any.node.domain" };
    const std::chrono::milliseconds failure_detection_time(10);
    const std::chrono::seconds failure_detection_timeout(5);
    const std::chrono::milliseconds short_interval(30);
    const std::chrono::milliseconds long_interval(300);
    const int failure_detection_count = 3;
    const std::chrono::milliseconds validation_interval(50);
    const std::chrono::milliseconds monitor_disposal_time(200);
    const std::chrono::steady_clock::time_point short_interval_time(short_interval);
}

class MonitorTest : public testing::Test {
protected:
    std::shared_ptr<HOST_INFO> host;
    std::shared_ptr<MONITOR> monitor;
    MOCK_MYSQL_MONITOR_PROXY* mock_proxy;
    std::shared_ptr<MOCK_MONITOR_CONNECTION_CONTEXT> mock_context_short_interval;
    std::shared_ptr<MOCK_MONITOR_CONNECTION_CONTEXT> mock_context_long_interval;

    static void SetUpTestSuite() {}

    static void TearDownTestSuite() {}

    void SetUp() override {
        host = std::make_shared<HOST_INFO>("host", 1234);
        mock_proxy = new MOCK_MYSQL_MONITOR_PROXY();
        monitor = std::make_shared<MONITOR>(host, failure_detection_timeout, monitor_disposal_time, mock_proxy);
        
        mock_context_short_interval = std::make_shared<MOCK_MONITOR_CONNECTION_CONTEXT>(
            node_keys,
            failure_detection_time,
            short_interval,
            failure_detection_count);

        mock_context_long_interval = std::make_shared<MOCK_MONITOR_CONNECTION_CONTEXT>(
            node_keys,
            failure_detection_time,
            long_interval,
            failure_detection_count);
    }

    void TearDown() override {}
};

TEST_F(MonitorTest, StartMonitoringWithDifferentContexts) {
    EXPECT_CALL(*mock_context_short_interval, set_start_monitor_time(_));
    EXPECT_CALL(*mock_context_long_interval, set_start_monitor_time(_));
    
    monitor->start_monitoring(mock_context_short_interval);
    monitor->start_monitoring(mock_context_long_interval);

    EXPECT_EQ(short_interval, TEST_UTILS::get_connection_check_interval(monitor));
}

TEST_F(MonitorTest, StopMonitoringWithContextRemaining) {
    EXPECT_CALL(*mock_context_short_interval, set_start_monitor_time(_));
    EXPECT_CALL(*mock_context_long_interval, set_start_monitor_time(_));
    
    monitor->start_monitoring(mock_context_short_interval);
    monitor->start_monitoring(mock_context_long_interval);

    monitor->stop_monitoring(mock_context_short_interval);

    EXPECT_EQ(long_interval, TEST_UTILS::get_connection_check_interval(monitor));
}

TEST_F(MonitorTest, StopMonitoringWithNoMatchingContexts) {
    monitor->stop_monitoring(mock_context_long_interval);

    EXPECT_EQ(std::chrono::milliseconds(0), TEST_UTILS::get_connection_check_interval(monitor));

    EXPECT_CALL(*mock_context_short_interval, set_start_monitor_time(_));
    
    monitor->start_monitoring(mock_context_short_interval);
    monitor->stop_monitoring(mock_context_long_interval);

    EXPECT_EQ(short_interval, TEST_UTILS::get_connection_check_interval(monitor));
}

TEST_F(MonitorTest, StopMonitoringTwiceWithSameContext) {
    EXPECT_CALL(*mock_context_long_interval, set_start_monitor_time(_));

    monitor->start_monitoring(mock_context_long_interval);

    monitor->stop_monitoring(mock_context_long_interval);
    monitor->stop_monitoring(mock_context_long_interval);

    EXPECT_EQ(std::chrono::milliseconds(0), TEST_UTILS::get_connection_check_interval(monitor));
}

TEST_F(MonitorTest, IsConnectionHealthyWithNoExistingConnection) {
    EXPECT_CALL(*mock_proxy, is_connected())
        .WillOnce(Return(false))
        .WillRepeatedly(Return(true));
    
    EXPECT_CALL(*mock_proxy, init()).Times(1);
    
    EXPECT_CALL(*mock_proxy, connect())
        .WillOnce(Return(true));

    EXPECT_CALL(*mock_proxy, ping()).Times(0);

    CONNECTION_STATUS status = TEST_UTILS::check_connection_status(monitor);
    EXPECT_TRUE(status.is_valid);
    EXPECT_TRUE(status.elapsed_time >= std::chrono::milliseconds(0));
}

TEST_F(MonitorTest, IsConnectionHealthyOrUnhealthy) {
    EXPECT_CALL(*mock_proxy, is_connected())
        .WillOnce(Return(false))
        .WillRepeatedly(Return(true));
    
    EXPECT_CALL(*mock_proxy, init()).Times(AtLeast(1));
    EXPECT_CALL(*mock_proxy, options(_, _)).Times(AtLeast(1));

    EXPECT_CALL(*mock_proxy, connect())
        .WillRepeatedly(Return(true));

    EXPECT_CALL(*mock_proxy, ping())
        .WillOnce(Return(0))
        .WillOnce(Return(1));

    CONNECTION_STATUS status1 = TEST_UTILS::check_connection_status(monitor);
    EXPECT_TRUE(status1.is_valid);

    CONNECTION_STATUS status2 = TEST_UTILS::check_connection_status(monitor);
    EXPECT_TRUE(status2.is_valid);

    CONNECTION_STATUS status3 = TEST_UTILS::check_connection_status(monitor);
    EXPECT_FALSE(status3.is_valid);
}

TEST_F(MonitorTest, IsConnectionHealthyAfterFailedConnection) {
    EXPECT_CALL(*mock_proxy, is_connected())
        .WillOnce(Return(false))
        .WillRepeatedly(Return(true));
    
    EXPECT_CALL(*mock_proxy, init()).Times(AtLeast(1));
    EXPECT_CALL(*mock_proxy, options(_, _)).Times(AtLeast(1));
    
    EXPECT_CALL(*mock_proxy, connect())
        .WillOnce(Return(true));

    EXPECT_CALL(*mock_proxy, ping())
        .WillOnce(Return(1));

    CONNECTION_STATUS first_status = TEST_UTILS::check_connection_status(monitor);
    EXPECT_TRUE(first_status.is_valid);
    EXPECT_TRUE(first_status.elapsed_time >= std::chrono::milliseconds(0));

    CONNECTION_STATUS second_status = TEST_UTILS::check_connection_status(monitor);
    EXPECT_FALSE(second_status.is_valid);
    EXPECT_TRUE(second_status.elapsed_time >= std::chrono::milliseconds(0));
}

TEST_F(MonitorTest, RunWithoutContext) {
    auto proxy = new MOCK_MYSQL_MONITOR_PROXY();
    std::shared_ptr<MONITOR_THREAD_CONTAINER> container = MONITOR_THREAD_CONTAINER::get_instance();
    auto monitor_service = std::make_shared<MONITOR_SERVICE>(container);

    auto mock_monitor = std::make_shared<MOCK_MONITOR3>(host, short_interval, proxy);

    EXPECT_CALL(*mock_monitor, get_current_time())
        .WillRepeatedly(Return(short_interval_time));

    // Put monitor into container maps
    std::string node_key = "monitorA";
    TEST_UTILS::populate_monitor_map(container, { node_key }, mock_monitor);
    TEST_UTILS::populate_task_map(container, mock_monitor);

    EXPECT_TRUE(TEST_UTILS::has_monitor(container, node_key));
    EXPECT_TRUE(TEST_UTILS::has_task(container, mock_monitor));

    // Run monitor without contexts. Should end by itself.
    mock_monitor->run(monitor_service);

    // After running with empty context, monitor should be out of the maps
    EXPECT_FALSE(TEST_UTILS::has_monitor(container, node_key));
    EXPECT_FALSE(TEST_UTILS::has_task(container, mock_monitor));
}

TEST_F(MonitorTest, RunWithContext) {
    auto proxy = new MOCK_MYSQL_MONITOR_PROXY();
    
    EXPECT_CALL(*proxy, is_connected())
        .WillOnce(Return(false))
        .WillRepeatedly(Return(true));

    EXPECT_CALL(*proxy, init()).Times(AtLeast(1));
    EXPECT_CALL(*proxy, options(_, _)).Times(AtLeast(1));

    EXPECT_CALL(*proxy, connect())
        .WillRepeatedly(Return(true));

    EXPECT_CALL(*proxy, ping())
        .WillRepeatedly(Return(0));

    std::shared_ptr<MONITOR> monitorA = 
        std::make_shared<MONITOR>(host, failure_detection_timeout, short_interval, proxy);

    auto container = MONITOR_THREAD_CONTAINER::get_instance();
    auto monitor_service = std::make_shared<MONITOR_SERVICE>(container);

    // Put monitor into container map
    std::string node_key = "monitorA";
    TEST_UTILS::populate_monitor_map(container, { node_key }, monitorA);
    TEST_UTILS::populate_task_map(container, monitorA);

    EXPECT_TRUE(TEST_UTILS::has_monitor(container, node_key));
    EXPECT_TRUE(TEST_UTILS::has_task(container, monitorA));

    auto context_short_interval = std::make_shared<MONITOR_CONNECTION_CONTEXT>(
        nullptr,
        node_keys,
        failure_detection_time,
        short_interval,
        failure_detection_count);

    // Put context
    monitorA->start_monitoring(context_short_interval);
    
    // Start thread to remove context from monitor
    std::thread thread(
        [&monitorA, &context_short_interval]() {
            std::this_thread::sleep_for(short_interval);
            monitorA->stop_monitoring(context_short_interval);
        });

    // Run monitor. Should end by itself after above thread stops monitoring.
    monitorA->run(monitor_service);

    thread.join();

    // After running, monitor should be out of the maps
    EXPECT_FALSE(TEST_UTILS::has_monitor(container, node_key));
    EXPECT_FALSE(TEST_UTILS::has_task(container, monitorA));
}

// Verify that if 0 timeout is passed in, we should set it to default value
TEST_F(MonitorTest, ZeroEFMTimeout) {
    auto proxy = new MOCK_MYSQL_MONITOR_PROXY();
    
    EXPECT_CALL(*proxy, is_connected())
        .WillOnce(Return(false))
        .WillRepeatedly(Return(true));

    EXPECT_CALL(*proxy, init()).Times(AtLeast(1));
    EXPECT_CALL(
        *proxy, 
        options(MYSQL_OPT_CONNECT_TIMEOUT,
                MatcherCast<const void*>(SafeMatcherCast<const unsigned int*>(
                    Pointee(Eq(failure_detection_timeout_default))))))
        .Times(1);
    EXPECT_CALL(
        *proxy,
        options(MYSQL_OPT_READ_TIMEOUT,
                MatcherCast<const void*>(SafeMatcherCast<const unsigned int*>(
                    Pointee(Eq(failure_detection_timeout_default))))))
        .Times(1);

    EXPECT_CALL(*proxy, connect()).WillOnce(Return(true));

    EXPECT_CALL(*proxy, ping()).WillRepeatedly(Return(0));

    std::chrono::seconds zero_timeout = std::chrono::seconds(0);

    std::shared_ptr<MONITOR> monitorA = 
        std::make_shared<MONITOR>(host, zero_timeout, short_interval, proxy);

    CONNECTION_STATUS status1 = TEST_UTILS::check_connection_status(monitorA);
    EXPECT_TRUE(status1.is_valid);
}

// Verify that if non-zero timeout is passed in, we should set it to that value
TEST_F(MonitorTest, NonZeroEFMTimeout) {
  auto proxy = new MOCK_MYSQL_MONITOR_PROXY();
  std::chrono::seconds timeout = std::chrono::seconds(1);

  EXPECT_CALL(*proxy, is_connected())
      .WillOnce(Return(false))
      .WillRepeatedly(Return(true));

  EXPECT_CALL(*proxy, init()).Times(AtLeast(1));
  EXPECT_CALL(
      *proxy,
      options(MYSQL_OPT_CONNECT_TIMEOUT,
              MatcherCast<const void*>(SafeMatcherCast<const unsigned int*>(Pointee(Eq(timeout.count()))))))
      .Times(1);
  EXPECT_CALL(
      *proxy,
      options(MYSQL_OPT_READ_TIMEOUT,
              MatcherCast<const void*>(SafeMatcherCast<const unsigned int*>(Pointee(Eq(timeout.count()))))))
      .Times(1);

  EXPECT_CALL(*proxy, connect()).WillOnce(Return(true));

  EXPECT_CALL(*proxy, ping()).WillRepeatedly(Return(0));


  std::shared_ptr<MONITOR> monitorA =
      std::make_shared<MONITOR>(host, timeout, short_interval, proxy);

  CONNECTION_STATUS status1 = TEST_UTILS::check_connection_status(monitorA);
  EXPECT_TRUE(status1.is_valid);
}
