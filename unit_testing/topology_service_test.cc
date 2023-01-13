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

#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <thread>

#include "test_utils.h"
#include "mock_objects.h"

using ::testing::_;
using ::testing::DeleteArg;
using ::testing::Return;
using ::testing::ReturnNew;
using ::testing::StrEq;

namespace {
    MOCK_MYSQL_PROXY* mock_proxy;
    TOPOLOGY_SERVICE* ts;
    std::shared_ptr<HOST_INFO> cluster_instance;

    std::string cluster_id("eac7829c-bb6d-4d1a-82fa-a8c2470910f2");

    char* reader1[4] = { "replica-instance-1", "Replica", "2020-09-15 17:51:53.0", "13.5" };
    char* reader2[4] = { "replica-instance-2", "Replica", "2020-09-15 17:51:53.0", "13.5" };
    char* writer[4] = { "writer-instance", WRITER_SESSION_ID, "2020-09-15 17:51:53.0", "13.5" };
    char* writer1[4] = { "writer-instance-1", WRITER_SESSION_ID, "2020-09-15 17:51:53.0", "13.5" };
    char* writer2[4] = { "writer-instance-2", WRITER_SESSION_ID, "2020-09-15 17:51:53.0", "13.5" };
    char* writer3[4] = { "writer-instance-3", WRITER_SESSION_ID, "2020-09-15 17:51:53.0", "13.5" };
}  // namespace

class TopologyServiceTest : public testing::Test {
protected:
    SQLHENV env;
    DBC* dbc;
    DataSource* ds;
    
    static void SetUpTestSuite() {
        ts = new TOPOLOGY_SERVICE(0);
        cluster_instance = std::make_shared<HOST_INFO>("?.XYZ.us-east-2.rds.amazonaws.com", 1234);
        ts->set_cluster_instance_template(cluster_instance);
        ts->set_cluster_id(cluster_id);
    }

    static void TearDownTestSuite() {
        delete ts;
    }

    void SetUp() override {
        allocate_odbc_handles(env, dbc, ds);
        
        mock_proxy = new MOCK_MYSQL_PROXY(dbc, ds);
        EXPECT_CALL(*mock_proxy, store_result()).WillRepeatedly(ReturnNew<MYSQL_RES>());
        EXPECT_CALL(*mock_proxy, free_result(_)).WillRepeatedly(DeleteArg<0>());
        ts->set_refresh_rate(DEFAULT_REFRESH_RATE_IN_MILLISECONDS);
    }

    void TearDown() override {
        cleanup_odbc_handles(env, dbc, ds);
        
        ts->clear_all();
        delete mock_proxy;
    }
};

TEST_F(TopologyServiceTest, TopologyQuery) {
    EXPECT_CALL(*mock_proxy, query(StrEq(RETRIEVE_TOPOLOGY_SQL)))
        .WillRepeatedly(Return(0));
    EXPECT_CALL(*mock_proxy, fetch_row(_))
        .WillOnce(Return(reader1))
        .WillOnce(Return(writer))
        .WillOnce(Return(reader2))
        .WillRepeatedly(Return(MYSQL_ROW{}));

    std::shared_ptr<CLUSTER_TOPOLOGY_INFO> topology = ts->get_topology(mock_proxy);
    ASSERT_NE(nullptr, topology);

    EXPECT_FALSE(topology->is_multi_writer_cluster);
    EXPECT_EQ(3, topology->total_hosts());
    EXPECT_EQ(2, topology->num_readers());

    std::shared_ptr<HOST_INFO> writer_host = topology->get_writer();
    ASSERT_NE(nullptr, writer_host);
	
    EXPECT_EQ("writer-instance.XYZ.us-east-2.rds.amazonaws.com", writer_host->get_host());
    EXPECT_EQ(1234, writer_host->get_port());
    EXPECT_EQ("writer-instance", writer_host->instance_name);
    EXPECT_EQ(WRITER_SESSION_ID, writer_host->session_id);
    EXPECT_EQ("2020-09-15 17:51:53.0", writer_host->last_updated);
    EXPECT_EQ("13.5", writer_host->replica_lag);
}

TEST_F(TopologyServiceTest, MultiWriter) {
    EXPECT_CALL(*mock_proxy, query(StrEq(RETRIEVE_TOPOLOGY_SQL)))
        .WillRepeatedly(Return(0));
    EXPECT_CALL(*mock_proxy, fetch_row(_))
        .WillOnce(Return(writer1))
        .WillOnce(Return(writer2))
        .WillOnce(Return(writer3))
        .WillRepeatedly(Return(MYSQL_ROW{}));

    std::shared_ptr<CLUSTER_TOPOLOGY_INFO> topology = ts->get_topology(mock_proxy);
    ASSERT_NE(nullptr, topology);

    EXPECT_TRUE(topology->is_multi_writer_cluster);
    EXPECT_EQ(3, topology->total_hosts());
    EXPECT_EQ(2, topology->num_readers()); // 2 writers are marked as readers

    std::shared_ptr<HOST_INFO> writer_host = topology->get_writer();
    ASSERT_NE(nullptr, writer_host);

    EXPECT_EQ("writer-instance-1.XYZ.us-east-2.rds.amazonaws.com", writer_host->get_host());
    EXPECT_EQ(1234, writer_host->get_port());
    EXPECT_EQ("writer-instance-1", writer_host->instance_name);
    EXPECT_EQ(WRITER_SESSION_ID, writer_host->session_id);
    EXPECT_EQ("2020-09-15 17:51:53.0", writer_host->last_updated);
    EXPECT_EQ("13.5", writer_host->replica_lag);
}

TEST_F(TopologyServiceTest, DuplicateInstances) {
  EXPECT_CALL(*mock_proxy, query(StrEq(RETRIEVE_TOPOLOGY_SQL)))
      .WillRepeatedly(Return(0));
  EXPECT_CALL(*mock_proxy, fetch_row(_))
      .WillOnce(Return(writer1))
      .WillOnce(Return(writer1))
      .WillOnce(Return(writer1))
      .WillRepeatedly(Return(MYSQL_ROW{}));

  std::shared_ptr<CLUSTER_TOPOLOGY_INFO> topology = ts->get_topology(mock_proxy);
  ASSERT_NE(nullptr, topology);

  EXPECT_TRUE(topology->is_multi_writer_cluster);
  EXPECT_EQ(1, topology->total_hosts());
  EXPECT_EQ(0, topology->num_readers());

  std::shared_ptr<HOST_INFO> writer_host = topology->get_writer();
  ASSERT_NE(nullptr, writer_host);

  EXPECT_EQ("writer-instance-1.XYZ.us-east-2.rds.amazonaws.com",
            writer_host->get_host());
  EXPECT_EQ(1234, writer_host->get_port());
  EXPECT_EQ("writer-instance-1", writer_host->instance_name);
  EXPECT_EQ(WRITER_SESSION_ID, writer_host->session_id);
  EXPECT_EQ("2020-09-15 17:51:53.0", writer_host->last_updated);
  EXPECT_EQ("13.5", writer_host->replica_lag);
}

TEST_F(TopologyServiceTest, CachedTopology) {
  EXPECT_CALL(*mock_proxy, query(StrEq(RETRIEVE_TOPOLOGY_SQL)))
        .Times(1)
        .WillOnce(Return(0));
  EXPECT_CALL(*mock_proxy, fetch_row(_))
        .Times(4)
        .WillOnce(Return(reader1))
        .WillOnce(Return(writer))
        .WillOnce(Return(reader2))
        .WillOnce(Return(MYSQL_ROW{}));

    ts->get_topology(mock_proxy);

    // 2nd call to get_topology() should retrieve from cache instead of executing another query
    // which is why we expect try_execute_query to be called only once
    ts->get_topology(mock_proxy);
}

TEST_F(TopologyServiceTest, QueryFailure) {
    EXPECT_CALL(*mock_proxy, query(StrEq(RETRIEVE_TOPOLOGY_SQL)))
        .WillOnce(Return(1));

    std::shared_ptr<CLUSTER_TOPOLOGY_INFO> topology = ts->get_topology(mock_proxy);

    EXPECT_EQ(nullptr, topology);
}

TEST_F(TopologyServiceTest, StaleTopology) {
    EXPECT_CALL(*mock_proxy, query(StrEq(RETRIEVE_TOPOLOGY_SQL)))
        .Times(2)
        .WillOnce(Return(0))
        .WillOnce(Return(1));
    EXPECT_CALL(*mock_proxy, fetch_row(_))
        .WillOnce(Return(reader1))
        .WillOnce(Return(writer))
        .WillOnce(Return(reader2))
        .WillRepeatedly(Return(MYSQL_ROW{}));

    ts->set_refresh_rate(1);

    std::shared_ptr<CLUSTER_TOPOLOGY_INFO> hosts = ts->get_topology(mock_proxy);
    std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    std::shared_ptr<CLUSTER_TOPOLOGY_INFO> stale_hosts = ts->get_topology(mock_proxy);

    EXPECT_EQ(3, stale_hosts->total_hosts());
    EXPECT_EQ(hosts, stale_hosts);
}

TEST_F(TopologyServiceTest, RefreshTopology) {
    EXPECT_CALL(*mock_proxy, query(StrEq(RETRIEVE_TOPOLOGY_SQL)))
        .Times(2)
        .WillRepeatedly(Return(0));
    EXPECT_CALL(*mock_proxy, fetch_row(_))
        .WillOnce(Return(reader1))
        .WillOnce(Return(writer))
        .WillOnce(Return(reader2))
        .WillRepeatedly(Return(MYSQL_ROW{}));

    ts->set_refresh_rate(1);

    ts->get_topology(mock_proxy);
    std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    ts->get_topology(mock_proxy);
}

TEST_F(TopologyServiceTest, SharedTopology) {
    EXPECT_CALL(*mock_proxy, query(StrEq(RETRIEVE_TOPOLOGY_SQL)))
        .WillOnce(Return(0))
        .WillRepeatedly(Return(false));
    EXPECT_CALL(*mock_proxy, fetch_row(_))
        .WillOnce(Return(reader1))
        .WillOnce(Return(writer))
        .WillOnce(Return(reader2))
        .WillRepeatedly(Return(MYSQL_ROW{}));

    TOPOLOGY_SERVICE* ts2 = new TOPOLOGY_SERVICE(0);
    ts2->set_cluster_id(cluster_id);

    auto topology1 = ts->get_topology(mock_proxy);
    auto topology2 = ts2->get_cached_topology();
    
    // Both topologies should come from 
    // the same underlying shared topology.
    EXPECT_NE(nullptr, topology1);
    EXPECT_NE(nullptr, topology2);
    EXPECT_EQ(topology1, topology2);

    ts->clear();

    topology1 = ts->get_cached_topology();
    topology2 = ts2->get_cached_topology();

    // Both topologies should be null because
    // the underlying shared topology got cleared.
    EXPECT_EQ(nullptr, topology1);
    EXPECT_EQ(nullptr, topology2);

    delete ts2;
}

TEST_F(TopologyServiceTest, ClearCache) {
    EXPECT_CALL(*mock_proxy, query(StrEq(RETRIEVE_TOPOLOGY_SQL)))
        .WillOnce(Return(0))
        .WillRepeatedly(Return(1));
    EXPECT_CALL(*mock_proxy, fetch_row(_))
        .WillOnce(Return(reader1))
        .WillOnce(Return(writer))
        .WillOnce(Return(reader2))
        .WillRepeatedly(Return(MYSQL_ROW{}));

    auto topology = ts->get_topology(mock_proxy);
    EXPECT_NE(nullptr, topology);

    ts->clear_all();

    // topology should now be null after above clear_all()
    topology = ts->get_topology(mock_proxy);
    EXPECT_EQ(nullptr, topology);
}
