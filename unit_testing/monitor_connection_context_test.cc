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

#include "driver/monitor_connection_context.h"
#include "driver/driver.h"

#include "test_utils.h"
#include "mock_objects.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

using ::testing::_;
using ::testing::Return;

namespace {
    const std::set<std::string> node_keys = { "any.node.domain" };
    const std::chrono::milliseconds failure_detection_time(10);
    const std::chrono::milliseconds failure_detection_interval(100);
    const int failure_detection_count = 3;
    const std::chrono::milliseconds validation_interval(50);
}

class MonitorConnectionContextTest : public testing::Test {
 protected:
    MONITOR_CONNECTION_CONTEXT* context;
    DBC* connection_to_abort;
    SQLHENV env;
    DataSource* ds;

    static void SetUpTestSuite() {}

    static void TearDownTestSuite() {}

    void SetUp() override {
        allocate_odbc_handles(env, connection_to_abort, ds);
        connection_to_abort->mysql_proxy = new MOCK_MYSQL_PROXY(connection_to_abort, ds);
        context = new MONITOR_CONNECTION_CONTEXT(connection_to_abort,
                                                 node_keys,
                                                 failure_detection_time,
                                                 failure_detection_interval,
                                                 failure_detection_count);
    }

    void TearDown() override {
        cleanup_odbc_handles(env, connection_to_abort, ds);
        delete context;
    }
};

TEST_F(MonitorConnectionContextTest, IsNodeUnhealthyWithConnection_ReturnFalse) {
    std::chrono::steady_clock::time_point current_time = std::chrono::steady_clock::now();
    context->set_connection_valid(true, current_time, current_time);
    EXPECT_FALSE(context->is_node_unhealthy());
    EXPECT_EQ(0, context->get_failure_count());
}

TEST_F(MonitorConnectionContextTest, IsNodeUnhealthyWithInvalidConnection_ReturnFalse) {
    std::chrono::steady_clock::time_point current_time = std::chrono::steady_clock::now();
    context->set_connection_valid(false, current_time, current_time);
    EXPECT_FALSE(context->is_node_unhealthy());
    EXPECT_EQ(1, context->get_failure_count());
}

TEST_F(MonitorConnectionContextTest, IsNodeUnhealthyExceedsFailureDetectionCount_ReturnTrue) {
    const int expected_failure_count = failure_detection_count + 1;
    context->set_failure_count(failure_detection_count);
    context->reset_invalid_node_start_time();

    std::chrono::steady_clock::time_point current_time = std::chrono::steady_clock::now();
    context->set_connection_valid(false, current_time, current_time);

    EXPECT_FALSE(context->is_node_unhealthy());
    EXPECT_EQ(expected_failure_count, context->get_failure_count());
    EXPECT_TRUE(context->is_invalid_node_start_time_defined());
}

TEST_F(MonitorConnectionContextTest, IsNodeUnhealthyExceedsFailureDetectionCount) {
    std::chrono::steady_clock::time_point current_time = std::chrono::steady_clock::now();
    context->set_failure_count(0);
    context->reset_invalid_node_start_time();

    // Simulate monitor loop that reports invalid connection for 5 times with interval 50 msec to wait 250 msec in total
    for (int i = 0; i < 5; i++) {
      std::chrono::steady_clock::time_point status_check_start_time = current_time;
      std::chrono::steady_clock::time_point status_check_end_time = current_time + validation_interval;

      context->set_connection_valid(false, status_check_start_time, status_check_end_time);
      EXPECT_FALSE(context->is_node_unhealthy());

      current_time += validation_interval;
    }

    // Simulate waiting another 50 msec that makes total waiting time to 300 msec
    // Expected max waiting time for this context is 300 msec (interval 100 msec, count 3)
    // So it's expected that this run turns node status to "unhealthy" since we reached max allowed waiting time.

    std::chrono::steady_clock::time_point status_check_start_time = current_time;
    std::chrono::steady_clock::time_point status_check_end_time = current_time + validation_interval;

    context->set_connection_valid(false, status_check_start_time, status_check_end_time);
    EXPECT_TRUE(context->is_node_unhealthy());
}
