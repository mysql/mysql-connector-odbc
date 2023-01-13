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

#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <iostream>
#include <fstream>
#include <stdio.h>

#include "test_utils.h"
#include "mock_objects.h"

using ::testing::_;
using ::testing::AtLeast;
using ::testing::Return;
using ::testing::StrEq;

class ClusterAwareMetricsContainerTest : public testing::Test {
protected:
    SQLHENV env;
    DBC* dbc;
    DataSource* ds;

    std::shared_ptr<MOCK_CLUSTER_AWARE_METRICS_CONTAINER> metrics_container;

    std::string cluster_id = "test-cluster-id";
    std::string instance_url = "test-instance-url.domain.com:12345";

    static void SetUpTestSuite() {}

    static void TearDownTestSuite() {}

    void SetUp() override {
        allocate_odbc_handles(env, dbc, ds);

        metrics_container = std::make_shared<MOCK_CLUSTER_AWARE_METRICS_CONTAINER>(dbc, ds);
    }

    void TearDown() override {
        cleanup_odbc_handles(env, dbc, ds, true);
    }
};

TEST_F(ClusterAwareMetricsContainerTest, isEnabled) {
    std::shared_ptr<MOCK_CLUSTER_AWARE_METRICS_CONTAINER> metrics
        = std::make_shared<MOCK_CLUSTER_AWARE_METRICS_CONTAINER>();
    EXPECT_FALSE(metrics->is_enabled());
    EXPECT_FALSE(metrics->is_instance_metrics_enabled());
    metrics->set_gather_metric(true);
    EXPECT_TRUE(metrics->is_enabled());
    EXPECT_TRUE(metrics->is_instance_metrics_enabled());

    ds->gather_perf_metrics = true;
    ds->gather_metrics_per_instance = true;
    EXPECT_TRUE(metrics_container->is_enabled());
    EXPECT_TRUE(metrics_container->is_instance_metrics_enabled());

    ds->gather_perf_metrics = false;
    ds->gather_metrics_per_instance = false;
    EXPECT_FALSE(metrics_container->is_enabled());
    EXPECT_FALSE(metrics_container->is_instance_metrics_enabled());

    ds->gather_perf_metrics = true;
    ds->gather_metrics_per_instance = false;
    EXPECT_TRUE(metrics_container->is_enabled());
    EXPECT_FALSE(metrics_container->is_instance_metrics_enabled());

    ds->gather_perf_metrics = false;
    ds->gather_metrics_per_instance = true;
    EXPECT_FALSE(metrics_container->is_enabled());
    EXPECT_TRUE(metrics_container->is_instance_metrics_enabled());
}

TEST_F(ClusterAwareMetricsContainerTest, collectClusterOnly) {
    ds->gather_perf_metrics = true;
    ds->gather_metrics_per_instance = false;

    EXPECT_CALL(*metrics_container, get_curr_conn_url())
        .Times(0);

    metrics_container->set_cluster_id(cluster_id);

    metrics_container->register_failure_detection_time(1234);
    metrics_container->register_writer_failover_procedure_time(1234);
    metrics_container->register_reader_failover_procedure_time(1234);
    metrics_container->register_failover_connects(true);
    metrics_container->register_invalid_initial_connection(true);
    metrics_container->register_use_cached_topology(true);
    metrics_container->register_topology_query_execution_time(1234);

    std::string cluster_logs = metrics_container->report_metrics(cluster_id, false);
    std::string instance_logs = metrics_container->report_metrics(cluster_id, true);

    auto cluster_length = std::count(cluster_logs.begin(), cluster_logs.end(), '\n');
    auto instance_length = std::count(instance_logs.begin(), instance_logs.end(), '\n');

    EXPECT_TRUE(cluster_length > instance_length);
}

TEST_F(ClusterAwareMetricsContainerTest, collectInstance) {
    ds->gather_perf_metrics = true;
    ds->gather_metrics_per_instance = true;

    EXPECT_CALL(*metrics_container, get_curr_conn_url())
        .Times(7)
        .WillRepeatedly(Return(instance_url));

    metrics_container->set_cluster_id(cluster_id);

    metrics_container->register_failure_detection_time(1234);
    metrics_container->register_writer_failover_procedure_time(1234);
    metrics_container->register_reader_failover_procedure_time(1234);
    metrics_container->register_failover_connects(true);
    metrics_container->register_invalid_initial_connection(true);
    metrics_container->register_use_cached_topology(true);
    metrics_container->register_topology_query_execution_time(1234);

    std::string cluster_logs = metrics_container->report_metrics(cluster_id, false);
    std::string instance_logs = metrics_container->report_metrics(instance_url, true);

    auto cluster_length = std::count(cluster_logs.begin(), cluster_logs.end(), '\n');
    auto instance_length = std::count(instance_logs.begin(), instance_logs.end(), '\n');

    EXPECT_TRUE(cluster_length == instance_length);
}
