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

#include <chrono>
#include <thread>

#include "test_utils.h"
#include "mock_objects.h"

using ::testing::_;
using ::testing::AnyNumber;
using ::testing::Invoke;
using ::testing::Mock;
using ::testing::Return;

namespace {
    const std::string HOST_SUFFIX = ".xyz.us-east-2.rds.amazonaws.com";
}  // namespace

class FailoverReaderHandlerTest : public testing::Test {
protected:
    SQLHENV env;
    DBC* dbc;
    DataSource* ds;

    static std::shared_ptr<HOST_INFO> reader_a_host;
    static std::shared_ptr<HOST_INFO> reader_b_host;
    static std::shared_ptr<HOST_INFO> reader_c_host;
    static std::shared_ptr<HOST_INFO> writer_host;

    MOCK_MYSQL_PROXY* mock_reader_a_proxy;
    MOCK_MYSQL_PROXY* mock_reader_b_proxy;
    MOCK_MYSQL_PROXY* mock_writer_proxy;

    static std::shared_ptr<CLUSTER_TOPOLOGY_INFO> topology;
    
    std::shared_ptr<MOCK_TOPOLOGY_SERVICE> mock_ts;
    std::shared_ptr<MOCK_CONNECTION_HANDLER> mock_connection_handler;
    MOCK_FAILOVER_SYNC mock_sync;

    static void SetUpTestSuite() { 
        reader_a_host = std::make_shared<HOST_INFO>("reader-a-host" + HOST_SUFFIX, 1234, UP, false);
        reader_a_host->instance_name = "reader-a-host";
        
        reader_b_host = std::make_shared<HOST_INFO>("reader-b-host" + HOST_SUFFIX, 1234, UP, false);
        reader_b_host->instance_name = "reader-b-host";

        reader_c_host = std::make_shared<HOST_INFO>("reader-c-host" + HOST_SUFFIX, 1234, DOWN, false);
        reader_c_host->instance_name = "reader-c-host";
        
        writer_host = std::make_shared<HOST_INFO>("writer-host" + HOST_SUFFIX, 1234, UP, true);
        writer_host->instance_name = "writer-host";

        topology = std::make_shared<CLUSTER_TOPOLOGY_INFO>();
        topology->add_host(writer_host);
        topology->add_host(reader_a_host);
        topology->add_host(reader_b_host);
        topology->add_host(reader_c_host);
    }

    static void TearDownTestSuite() {}

    void SetUp() override {
        allocate_odbc_handles(env, dbc, ds);
        
        reader_a_host->set_host_state(UP);
        reader_b_host->set_host_state(UP);
        reader_c_host->set_host_state(DOWN);
        writer_host->set_host_state(UP);

        mock_ts = std::make_shared<MOCK_TOPOLOGY_SERVICE>();
        mock_connection_handler = std::make_shared<MOCK_CONNECTION_HANDLER>();
        EXPECT_CALL(mock_sync, is_completed()).WillRepeatedly(Return(false));
    }

    void TearDown() override {
        cleanup_odbc_handles(env, dbc, ds);
    }
};

std::shared_ptr<HOST_INFO> FailoverReaderHandlerTest::reader_a_host = nullptr;
std::shared_ptr<HOST_INFO> FailoverReaderHandlerTest::reader_b_host = nullptr;
std::shared_ptr<HOST_INFO> FailoverReaderHandlerTest::reader_c_host = nullptr;
std::shared_ptr<HOST_INFO> FailoverReaderHandlerTest::writer_host = nullptr;

std::shared_ptr<CLUSTER_TOPOLOGY_INFO> FailoverReaderHandlerTest::topology = nullptr;

// Helper function that generates a list of hosts.
// Parameters: Number of reader hosts UP, number of reader hosts DOWN, number of writer hosts.
CLUSTER_TOPOLOGY_INFO generate_topology(int readers_up, int readers_down, int writers) {
  CLUSTER_TOPOLOGY_INFO topology_info = CLUSTER_TOPOLOGY_INFO();
  int number = 0;
  for (int i = 0; i < readers_up; i++) {
    std::shared_ptr<HOST_INFO> reader_up = std::make_shared<HOST_INFO>("host" + number, 0123, UP, false);
    topology_info.add_host(reader_up);
    number++;
  }
  for (int i = 0; i < readers_down; i++) {
    std::shared_ptr<HOST_INFO> reader_down = std::make_shared<HOST_INFO>("host" + number, 0123, DOWN, false);
    topology_info.add_host(reader_down);
    number++;
  }
  for (int i = 0; i < writers; i++) {
    std::shared_ptr<HOST_INFO> writer = std::make_shared<HOST_INFO>("host" + number, 0123, UP, true);
    topology_info.add_host(writer);
    number++;
  }
  return topology_info;
}

// Unit tests for the function that generates the list of hosts based on number of hosts.
TEST_F(FailoverReaderHandlerTest, GenerateTopology) {
  CLUSTER_TOPOLOGY_INFO topology_info;

  topology_info = generate_topology(0, 0, 0);
  EXPECT_EQ(0, topology_info.total_hosts());

  topology_info = generate_topology(0, 0, 1);
  EXPECT_EQ(1, topology_info.total_hosts());

  topology_info = generate_topology(1, 0, 1);
  EXPECT_EQ(2, topology_info.total_hosts());

  topology_info = generate_topology(1, 1, 2);
  EXPECT_EQ(4, topology_info.total_hosts());
}

TEST_F(FailoverReaderHandlerTest, BuildHostsList) {
    FAILOVER_READER_HANDLER reader_handler(mock_ts, mock_connection_handler, 60000, 30000, 0);
    std::shared_ptr<CLUSTER_TOPOLOGY_INFO> topology_info;
    std::vector<std::shared_ptr<HOST_INFO>> hosts_list;

    //Case: 0 readers up, 0 readers down, 0 writers, writers included.
    topology_info = std::make_shared<CLUSTER_TOPOLOGY_INFO>(generate_topology(0, 0, 0));
    hosts_list = reader_handler.build_hosts_list(topology_info, true);
    EXPECT_EQ(0, hosts_list.size());

    //Case: 0 readers up, 0 readers down, 1 writers, writers included.
    topology_info = std::make_shared<CLUSTER_TOPOLOGY_INFO>(generate_topology(0, 0, 1));
    hosts_list = reader_handler.build_hosts_list(topology_info, true);
    EXPECT_EQ(1, hosts_list.size());
    EXPECT_TRUE(hosts_list[0]->is_host_writer());

    //Case: 0 readers up, 0 readers down, 1 writers, writers excluded.
    topology_info = std::make_shared<CLUSTER_TOPOLOGY_INFO>(generate_topology(0, 0, 1));
    hosts_list = reader_handler.build_hosts_list(topology_info, false);
    EXPECT_EQ(0, hosts_list.size());

    //Case: 1 readers up, 1 readers down, 0 writers, writers included.
    topology_info = std::make_shared<CLUSTER_TOPOLOGY_INFO>(generate_topology(0, 1, 0));
    hosts_list = reader_handler.build_hosts_list(topology_info, true);
    EXPECT_EQ(1, hosts_list.size());
    EXPECT_FALSE(hosts_list[0]->is_host_writer());
    EXPECT_FALSE(hosts_list[0]->is_host_up());

    //Case: 1 readers up, 0 readers down, 0 writers, writers included.
    topology_info = std::make_shared<CLUSTER_TOPOLOGY_INFO>(generate_topology(1, 0, 0));
    hosts_list = reader_handler.build_hosts_list(topology_info, true);
    EXPECT_EQ(1, hosts_list.size());
    EXPECT_FALSE(hosts_list[0]->is_host_writer());
    EXPECT_TRUE(hosts_list[0]->is_host_up());

    //Case: 1 readers up, 0 readers down, 1 writers, writers excluded.
    topology_info = std::make_shared<CLUSTER_TOPOLOGY_INFO>(generate_topology(1, 0, 1));
    hosts_list = reader_handler.build_hosts_list(topology_info, false);
    EXPECT_EQ(1, hosts_list.size());
    EXPECT_FALSE(hosts_list[0]->is_host_writer());

    //Case: 1 readers up, 1 readers down, 1 writers, writers excluded.
    topology_info = std::make_shared<CLUSTER_TOPOLOGY_INFO>(generate_topology(1, 1, 2));
    hosts_list = reader_handler.build_hosts_list(topology_info, false);
    EXPECT_EQ(2, hosts_list.size());
    EXPECT_FALSE(hosts_list[0]->is_host_writer());
    EXPECT_TRUE(hosts_list[0]->is_host_up());
    EXPECT_FALSE(hosts_list[1]->is_host_writer());
    EXPECT_TRUE(hosts_list[1]->is_host_down());

    //Case: 1 readers up, 1 readers down, 1 writers, writers included.
    topology_info = std::make_shared<CLUSTER_TOPOLOGY_INFO>(generate_topology(1, 1, 1));
    hosts_list = reader_handler.build_hosts_list(topology_info, true);
    EXPECT_EQ(3, hosts_list.size());
    EXPECT_FALSE(hosts_list[0]->is_host_writer());
    EXPECT_TRUE(hosts_list[0]->is_host_up());
    EXPECT_FALSE(hosts_list[1]->is_host_writer());
    EXPECT_TRUE(hosts_list[1]->is_host_down());
    EXPECT_TRUE(hosts_list[2]->is_host_writer());
}

// Verify that reader failover handler fails to connect to any reader node or writer node.
// Expected result: no new connection
TEST_F(FailoverReaderHandlerTest, GetConnectionFromHosts_Failure) {
    EXPECT_CALL(*mock_ts, get_topology(_, true)).WillRepeatedly(Return(topology));
    
    EXPECT_CALL(*mock_connection_handler, connect(_)).WillRepeatedly(Return(nullptr));

    EXPECT_CALL(*mock_ts, mark_host_down(reader_a_host)).Times(1);
    EXPECT_CALL(*mock_ts, mark_host_down(reader_b_host)).Times(1);
    EXPECT_CALL(*mock_ts, mark_host_down(reader_c_host)).Times(1);
    EXPECT_CALL(*mock_ts, mark_host_down(writer_host)).Times(1);

    FAILOVER_READER_HANDLER reader_handler(mock_ts, mock_connection_handler, 60000, 30000, 0);
    auto hosts_list = reader_handler.build_hosts_list(topology, true);
    READER_FAILOVER_RESULT result = reader_handler.get_connection_from_hosts(hosts_list, std::ref(mock_sync));

    EXPECT_FALSE(result.connected);
    EXPECT_THAT(result.new_connection, nullptr);
}

// Verify that reader failover handler connects to a reader node that is marked up.
// Expected result: new connection to reader A
TEST_F(FailoverReaderHandlerTest, GetConnectionFromHosts_Success_Reader) {
    mock_reader_a_proxy = new MOCK_MYSQL_PROXY(dbc, ds);

    EXPECT_CALL(*mock_ts, get_topology(_, true)).WillRepeatedly(Return(topology));

    EXPECT_CALL(*mock_reader_a_proxy, is_connected()).WillRepeatedly(Return(true));

    EXPECT_CALL(*mock_connection_handler, connect(_)).WillRepeatedly(Return(nullptr));    
    EXPECT_CALL(*mock_connection_handler, connect(reader_a_host)).WillRepeatedly(Return(mock_reader_a_proxy));

    // Reader C will not be used as it is put at the end. Will only try to connect to A and B
    EXPECT_CALL(*mock_ts, mark_host_up(reader_a_host)).Times(1);

    FAILOVER_READER_HANDLER reader_handler(mock_ts, mock_connection_handler, 60000, 30000, 0);
    auto hosts_list = reader_handler.build_hosts_list(topology, true);
    READER_FAILOVER_RESULT result = reader_handler.get_connection_from_hosts(hosts_list, std::ref(mock_sync));

    EXPECT_TRUE(result.connected);
    EXPECT_THAT(result.new_connection, mock_reader_a_proxy);
    EXPECT_FALSE(result.new_host->is_host_writer());

    // Explicit delete on reader A as it is returned as valid connection/result
    delete mock_reader_a_proxy;
}

// Verify that reader failover handler connects to a writer node.
// Expected result: new connection to writer
TEST_F(FailoverReaderHandlerTest, GetConnectionFromHosts_Success_Writer) {
    mock_writer_proxy = new MOCK_MYSQL_PROXY(dbc, ds);

    EXPECT_CALL(*mock_ts, get_topology(_, true)).WillRepeatedly(Return(topology));

    EXPECT_CALL(*mock_writer_proxy, is_connected()).WillRepeatedly(Return(true));

    EXPECT_CALL(*mock_connection_handler, connect(_)).WillRepeatedly(Return(nullptr));
    EXPECT_CALL(*mock_connection_handler, connect(writer_host)).WillRepeatedly(Return(mock_writer_proxy));

    EXPECT_CALL(*mock_ts, mark_host_up(writer_host)).Times(1);

    FAILOVER_READER_HANDLER reader_handler(mock_ts, mock_connection_handler, 60000, 30000, 0);
    auto hosts_list = reader_handler.build_hosts_list(topology, true);
    READER_FAILOVER_RESULT result = reader_handler.get_connection_from_hosts(hosts_list, std::ref(mock_sync));

    EXPECT_TRUE(result.connected);
    EXPECT_THAT(result.new_connection, mock_writer_proxy);
    EXPECT_TRUE(result.new_host->is_host_writer());

    // Explicit delete as it is returned as result & is not deconstructed during failover
    delete mock_writer_proxy; 
}

// Verify that reader failover handler connects to the fastest reader node available that is marked up.
// Expected result: new connection to reader A
TEST_F(FailoverReaderHandlerTest, GetConnectionFromHosts_FastestHost) {
    mock_reader_a_proxy = new MOCK_MYSQL_PROXY(dbc, ds);
    mock_reader_b_proxy = new MOCK_MYSQL_PROXY(dbc, ds); // Will be free'd during failover as it is slower

    // May not have actually connected during failover
    // Cannot delete at the end as it may cause double delete
    Mock::AllowLeak(mock_reader_b_proxy);
    Mock::AllowLeak(mock_reader_b_proxy->get_ds());

    EXPECT_CALL(*mock_ts, get_topology(_, true)).WillRepeatedly(Return(topology));

    EXPECT_CALL(*mock_ts, get_topology(_, true)).WillRepeatedly(Return(topology));
    // Reader C will not be used as it is put at the end. Will only try to connect to A and B
    EXPECT_CALL(*mock_reader_a_proxy, is_connected()).WillRepeatedly(Return(true));
    EXPECT_CALL(*mock_reader_b_proxy, is_connected()).WillRepeatedly(Return(true));

    EXPECT_CALL(*mock_connection_handler, connect(_)).WillRepeatedly(Return(nullptr));
    EXPECT_CALL(*mock_connection_handler, connect(reader_a_host)).WillRepeatedly(Return(mock_reader_a_proxy));
    EXPECT_CALL(*mock_connection_handler, connect(reader_b_host)).WillRepeatedly(Invoke([&]() {
            std::this_thread::sleep_for(std::chrono::milliseconds(5000));
            return mock_reader_b_proxy;
        }));

    FAILOVER_READER_HANDLER reader_handler(mock_ts, mock_connection_handler, 60000, 30000, 0);
    auto hosts_list = reader_handler.build_hosts_list(topology, true);
    READER_FAILOVER_RESULT result = reader_handler.get_connection_from_hosts(hosts_list, std::ref(mock_sync));

    EXPECT_TRUE(result.connected);
    EXPECT_THAT(result.new_connection, mock_reader_a_proxy);
    EXPECT_FALSE(result.new_host->is_host_writer());

    // Explicit delete on reader A as it is returned as a valid result
    delete mock_reader_a_proxy;
}

// Verify that reader failover handler fails to connect when a host fails to connect before timeout.
// Expected result: no connection
TEST_F(FailoverReaderHandlerTest, GetConnectionFromHosts_Timeout) {
    // Connections should automatically free inside failover
    mock_reader_a_proxy = new MOCK_MYSQL_PROXY(dbc, ds);
    mock_reader_b_proxy = new MOCK_MYSQL_PROXY(dbc, ds);

    EXPECT_CALL(*mock_ts, get_topology(_, true)).WillRepeatedly(Return(topology));

    EXPECT_CALL(*mock_reader_a_proxy, is_connected()).WillRepeatedly(Return(true));
    EXPECT_CALL(*mock_reader_a_proxy, mock_mysql_proxy_destructor());
    EXPECT_CALL(*mock_reader_b_proxy, is_connected()).WillRepeatedly(Return(true));
    EXPECT_CALL(*mock_reader_b_proxy, mock_mysql_proxy_destructor());
    
    EXPECT_CALL(*mock_connection_handler, connect(_)).WillRepeatedly(Return(nullptr));
    EXPECT_CALL(*mock_connection_handler, connect(reader_a_host)).WillRepeatedly(Invoke([&]() {
            std::this_thread::sleep_for(std::chrono::milliseconds(5000));
            return mock_reader_a_proxy;
        }));
    EXPECT_CALL(*mock_connection_handler, connect(reader_b_host)).WillRepeatedly(Invoke([&]() {
            std::this_thread::sleep_for(std::chrono::milliseconds(5000));
            return mock_reader_b_proxy;
        }));

    EXPECT_CALL(*mock_ts, mark_host_down(_)).Times(AnyNumber());
    EXPECT_CALL(*mock_ts, mark_host_down(writer_host)).Times(1);

    FAILOVER_READER_HANDLER reader_handler(mock_ts, mock_connection_handler, 60000, 1000, 0);
    auto hosts_list = reader_handler.build_hosts_list(topology, true);
    READER_FAILOVER_RESULT result = reader_handler.get_connection_from_hosts(hosts_list, std::ref(mock_sync));

    EXPECT_FALSE(result.connected);
    EXPECT_THAT(result.new_connection, nullptr);
}

// Verify that reader failover handler fails to connect to any reader node or
// writer node. Expected result: no new connection
TEST_F(FailoverReaderHandlerTest, Failover_Failure) {
    EXPECT_CALL(*mock_ts, get_topology(_, true)).WillRepeatedly(Return(topology));

    EXPECT_CALL(*mock_connection_handler, connect(_)).WillRepeatedly(Return(nullptr));

    EXPECT_CALL(*mock_ts, mark_host_down(reader_a_host)).Times(1);
    EXPECT_CALL(*mock_ts, mark_host_down(reader_b_host)).Times(1);
    EXPECT_CALL(*mock_ts, mark_host_down(reader_c_host)).Times(1);
    EXPECT_CALL(*mock_ts, mark_host_down(writer_host)).Times(1);

    FAILOVER_READER_HANDLER reader_handler(mock_ts, mock_connection_handler, 3000, 1000, 0);
    READER_FAILOVER_RESULT result = reader_handler.failover(topology);

    EXPECT_FALSE(result.connected);
    EXPECT_THAT(result.new_connection, nullptr);
}

// Verify that reader failover handler connects to a faster reader node.
// Expected result: new connection to reader A
TEST_F(FailoverReaderHandlerTest, Failover_Success_Reader) {
    mock_reader_a_proxy = new MOCK_MYSQL_PROXY(dbc, ds);
    mock_reader_b_proxy = new MOCK_MYSQL_PROXY(dbc, ds); // Will be free'd during failover to timeout / too slow

    // May not have actually connected during failover
    // Cannot delete at the end as it may cause double delete
    Mock::AllowLeak(mock_reader_b_proxy);
    Mock::AllowLeak(mock_reader_b_proxy->get_ds());

    auto current_topology = std::make_shared<CLUSTER_TOPOLOGY_INFO>();
    current_topology->add_host(writer_host);
    current_topology->add_host(reader_a_host);
    current_topology->add_host(reader_b_host);
    EXPECT_CALL(*mock_ts, get_topology(_, true)).WillRepeatedly(Return(current_topology));

    EXPECT_CALL(*mock_reader_a_proxy, is_connected()).WillRepeatedly(Return(true));

    EXPECT_CALL(*mock_reader_b_proxy, is_connected()).WillRepeatedly(Return(true));
    EXPECT_CALL(*mock_reader_b_proxy, mock_mysql_proxy_destructor());

    EXPECT_CALL(*mock_connection_handler, connect(_)).WillRepeatedly(Return(nullptr));
    EXPECT_CALL(*mock_connection_handler, connect(reader_a_host)).WillRepeatedly(
        Return(mock_reader_a_proxy));
    EXPECT_CALL(*mock_connection_handler, connect(reader_b_host)).WillRepeatedly(Invoke([&]() {
        std::this_thread::sleep_for(std::chrono::milliseconds(5000));
        return mock_reader_b_proxy;
    }));

    FAILOVER_READER_HANDLER reader_handler(mock_ts, mock_connection_handler, 60000, 30000, 0);
    READER_FAILOVER_RESULT result = reader_handler.failover(current_topology);

    EXPECT_TRUE(result.connected);
    EXPECT_THAT(result.new_connection, mock_reader_a_proxy);
    EXPECT_FALSE(result.new_host->is_host_writer());
    EXPECT_EQ("reader-a-host", result.new_host->instance_name);

    // Explicit delete on reader A as it's returned as a valid result
    delete mock_reader_a_proxy;
}
