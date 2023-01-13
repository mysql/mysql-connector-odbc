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
using ::testing::AtLeast;
using ::testing::DoAll;
using ::testing::Invoke;
using ::testing::Mock;
using ::testing::Return;
using ::testing::Sequence;
using ::testing::Unused;

namespace {
    const std::string HOST_SUFFIX = ".xyz.us-east-2.rds.amazonaws.com";
}  // namespace

class FailoverWriterHandlerTest : public testing::Test {
 protected:
    SQLHENV env;
    DBC* dbc;
    DataSource* ds;
    std::string writer_instance_name;
    std::string new_writer_instance_name;
    std::shared_ptr<HOST_INFO> writer_host;
    std::shared_ptr<HOST_INFO> new_writer_host;
    std::shared_ptr<HOST_INFO> reader_a_host;
    std::shared_ptr<HOST_INFO> reader_b_host;
    std::shared_ptr<CLUSTER_TOPOLOGY_INFO> current_topology;
    std::shared_ptr<MOCK_TOPOLOGY_SERVICE> mock_ts;
    std::shared_ptr<MOCK_READER_HANDLER> mock_reader_handler;
    std::shared_ptr<MOCK_CONNECTION_HANDLER> mock_connection_handler;
    MOCK_MYSQL_PROXY* mock_reader_a_proxy;
    MOCK_MYSQL_PROXY* mock_reader_b_proxy;
    MOCK_MYSQL_PROXY* mock_writer_proxy;
    MOCK_MYSQL_PROXY* mock_new_writer_proxy;

    static void SetUpTestSuite() {}

    static void TearDownTestSuite() {}

    void SetUp() override {
        allocate_odbc_handles(env, dbc, ds);
        
        writer_instance_name = "writer-host";
        new_writer_instance_name = "new-writer-host";

        writer_host = std::make_shared<HOST_INFO>(writer_instance_name + HOST_SUFFIX, 1234, DOWN, true);
        writer_host->instance_name = writer_instance_name;

        new_writer_host = std::make_shared<HOST_INFO>(new_writer_instance_name + HOST_SUFFIX, 1234, UP, true);
        new_writer_host->instance_name = new_writer_instance_name;

        reader_a_host = std::make_shared<HOST_INFO>("reader-a-host" + HOST_SUFFIX, 1234);
        reader_a_host->instance_name = "reader-a-host";

        reader_b_host = std::make_shared<HOST_INFO>("reader-b-host" + HOST_SUFFIX, 1234);
        reader_b_host->instance_name = "reader-b-host";

        current_topology = std::make_shared<CLUSTER_TOPOLOGY_INFO>();
        current_topology->add_host(writer_host);
        current_topology->add_host(reader_a_host);
        current_topology->add_host(reader_b_host);

        mock_ts = std::make_shared<MOCK_TOPOLOGY_SERVICE>();
        mock_reader_handler = std::make_shared<MOCK_READER_HANDLER>();
        mock_connection_handler = std::make_shared<MOCK_CONNECTION_HANDLER>();
    }

    void TearDown() override {
        cleanup_odbc_handles(env, dbc, ds);
    }
};

// Verify that writer failover handler can re-connect to a current writer node.
// topology: no changes
// taskA: successfully re-connect to writer; return new connection
// taskB: fail to connect to any reader due to exception
// expected test result: new connection by taskA
TEST_F(FailoverWriterHandlerTest, ReconnectToWriter_TaskBEmptyReaderResult) {
    mock_writer_proxy = new MOCK_MYSQL_PROXY(dbc, ds);
    EXPECT_CALL(*mock_writer_proxy, is_connected()).WillRepeatedly(Return(true));

    EXPECT_CALL(*mock_ts, get_topology(_, true))
        .WillRepeatedly(Return(current_topology));

    Sequence s;
    EXPECT_CALL(*mock_ts, mark_host_down(writer_host)).InSequence(s);
    EXPECT_CALL(*mock_ts, mark_host_up(writer_host)).InSequence(s);

    EXPECT_CALL(*mock_reader_handler, get_reader_connection(_, _))
        .WillRepeatedly(Return(READER_FAILOVER_RESULT(false, nullptr, nullptr)));

    EXPECT_CALL(*mock_connection_handler, connect(writer_host))
        .WillRepeatedly(Return(mock_writer_proxy));
    EXPECT_CALL(*mock_connection_handler, connect(reader_a_host))
        .WillRepeatedly(Return(nullptr));
    EXPECT_CALL(*mock_connection_handler, connect(reader_b_host))
        .WillRepeatedly(Return(nullptr));

    FAILOVER_WRITER_HANDLER writer_handler(
        mock_ts, mock_reader_handler, mock_connection_handler, 5000, 2000, 2000, 0);
    auto result = writer_handler.failover(current_topology);

    EXPECT_TRUE(result.connected);
    EXPECT_FALSE(result.is_new_host);
    EXPECT_THAT(result.new_connection, mock_writer_proxy);

    // Explicit delete on writer connection as it's returned as a valid result
    delete mock_writer_proxy;
}

// Verify that writer failover handler can re-connect to a current writer node.
// topology: no changes seen by task A, changes to [new-writer, reader-a,
// reader-b] for taskB
// taskA: successfully re-connect to initial writer; return new connection
// taskB: successfully connect to reader-a and then new writer but it takes more
// time than taskA
// expected test result: new connection by taskA
TEST_F(FailoverWriterHandlerTest, ReconnectToWriter_SlowReaderA) {
    mock_writer_proxy = new MOCK_MYSQL_PROXY(dbc, ds);
    mock_reader_a_proxy = new MOCK_MYSQL_PROXY(dbc, ds);

    // May not have actually connected during failover
    // Cannot delete at the end as it may cause double delete
    Mock::AllowLeak(mock_reader_a_proxy);
    Mock::AllowLeak(mock_reader_a_proxy->get_ds());

    const auto new_topology = std::make_shared<CLUSTER_TOPOLOGY_INFO>();
    new_topology->add_host(new_writer_host);
    new_topology->add_host(reader_a_host);
    new_topology->add_host(reader_b_host);

    EXPECT_CALL(*mock_writer_proxy, is_connected()).WillRepeatedly(Return(true));

    EXPECT_CALL(*mock_reader_a_proxy, is_connected()).WillRepeatedly(Return(true));

    EXPECT_CALL(*mock_connection_handler, connect(writer_host))
        .WillRepeatedly(Return(mock_writer_proxy));
    EXPECT_CALL(*mock_connection_handler, connect(reader_b_host))
        .WillRepeatedly(Return(nullptr));

    EXPECT_CALL(*mock_ts, get_topology(mock_writer_proxy, true))
        .WillRepeatedly(Return(current_topology));
    EXPECT_CALL(*mock_ts, get_topology(mock_reader_a_proxy, true))
        .WillRepeatedly(Return(new_topology));

    const Sequence s;
    EXPECT_CALL(*mock_ts, mark_host_down(writer_host)).InSequence(s);
    EXPECT_CALL(*mock_ts, mark_host_up(writer_host)).InSequence(s);

    EXPECT_CALL(*mock_reader_handler, get_reader_connection(_, _))
        .WillRepeatedly(DoAll(
            Invoke([](Unused, FAILOVER_SYNC& f_sync) {
                for (int i = 0; i <= 50 && !f_sync.is_completed(); i++) {
                    std::this_thread::sleep_for(std::chrono::milliseconds(100));
                }
            }),
            Return(READER_FAILOVER_RESULT(true, reader_a_host,
                                          mock_reader_a_proxy))));

    FAILOVER_WRITER_HANDLER writer_handler(
        mock_ts, mock_reader_handler, mock_connection_handler, 60000, 5000, 5000, 0);
    const auto result = writer_handler.failover(current_topology);

    EXPECT_TRUE(result.connected);
    EXPECT_FALSE(result.is_new_host);
    EXPECT_THAT(result.new_connection, mock_writer_proxy);

    // Explicit delete on writer connection as it's returned as a valid result
    delete mock_writer_proxy;
}

// Verify that writer failover handler can re-connect to a current writer node.
// topology: no changes
// taskA: successfully re-connect to writer; return new connection
// taskB: successfully connect to readerA and retrieve topology, but latest
// writer is not new (defer to taskA)
// expected test result: new connection by taskA
TEST_F(FailoverWriterHandlerTest, ReconnectToWriter_TaskBDefers) {
    mock_writer_proxy = new MOCK_MYSQL_PROXY(dbc, ds);
    mock_reader_a_proxy = new MOCK_MYSQL_PROXY(dbc, ds);
    EXPECT_CALL(*mock_writer_proxy, is_connected()).WillRepeatedly(Return(true));

    EXPECT_CALL(*mock_reader_a_proxy, is_connected()).WillRepeatedly(Return(true));
    EXPECT_CALL(*mock_reader_a_proxy, mock_mysql_proxy_destructor());

    EXPECT_CALL(*mock_connection_handler, connect(writer_host))
        .WillRepeatedly(Invoke([&]() {
            std::this_thread::sleep_for(std::chrono::milliseconds(5000));
            return mock_writer_proxy;
        }));
    EXPECT_CALL(*mock_connection_handler, connect(reader_b_host))
        .WillRepeatedly(Return(nullptr));

    EXPECT_CALL(*mock_ts, get_topology(_, true))
        .WillRepeatedly(Return(current_topology));

    Sequence s;
    EXPECT_CALL(*mock_ts, mark_host_down(writer_host)).InSequence(s);
    EXPECT_CALL(*mock_ts, mark_host_up(writer_host)).InSequence(s);

    EXPECT_CALL(*mock_reader_handler, get_reader_connection(_, _))
        .WillRepeatedly(Return(READER_FAILOVER_RESULT(true, reader_a_host,
                                                    mock_reader_a_proxy)));

    FAILOVER_WRITER_HANDLER writer_handler(
        mock_ts, mock_reader_handler, mock_connection_handler, 60000, 2000, 2000, 0);
    auto result = writer_handler.failover(current_topology);

    EXPECT_TRUE(result.connected);
    EXPECT_FALSE(result.is_new_host);
    EXPECT_THAT(result.new_connection, mock_writer_proxy);

    // Explicit delete on writer connection as it's returned as a valid result
    delete mock_writer_proxy;
}

// Verify that writer failover handler can re-connect to a new writer node.
// topology: changes to [new-writer, reader-A, reader-B] for taskB, taskA sees
// no changes
// taskA: successfully re-connect to writer; return connection to initial writer
// but it takes more time than taskB
// taskB: successfully connect to readerA and then to new-writer
// expected test result: new connection to writer by taskB
TEST_F(FailoverWriterHandlerTest, ConnectToReaderA_SlowWriter) {
    mock_writer_proxy = new MOCK_MYSQL_PROXY(dbc, ds);
    mock_new_writer_proxy = new MOCK_MYSQL_PROXY(dbc, ds);
    mock_reader_a_proxy = new MOCK_MYSQL_PROXY(dbc, ds);

    // May not have actually connected during failover
    // Cannot delete at the end as it may cause double delete
    Mock::AllowLeak(mock_writer_proxy);
    Mock::AllowLeak(mock_writer_proxy->get_ds());

    const auto new_topology = std::make_shared<CLUSTER_TOPOLOGY_INFO>();
    new_topology->add_host(new_writer_host);
    new_topology->add_host(reader_a_host);
    new_topology->add_host(reader_b_host);

    EXPECT_CALL(*mock_writer_proxy, is_connected()).WillRepeatedly(Return(true));
    EXPECT_CALL(*mock_writer_proxy, mock_mysql_proxy_destructor());

    EXPECT_CALL(*mock_new_writer_proxy, is_connected()).WillRepeatedly(Return(true));

    EXPECT_CALL(*mock_reader_a_proxy, is_connected()).WillRepeatedly(Return(true));
    EXPECT_CALL(*mock_reader_a_proxy, mock_mysql_proxy_destructor());

    EXPECT_CALL(*mock_connection_handler, connect(writer_host))
        .WillRepeatedly(Invoke([&]() {
            std::this_thread::sleep_for(std::chrono::milliseconds(5000));
            return mock_writer_proxy;
        }));

    EXPECT_CALL(*mock_connection_handler, connect(new_writer_host))
        .WillRepeatedly(Return(mock_new_writer_proxy));

    EXPECT_CALL(*mock_ts, get_topology(mock_writer_proxy, true))
        .WillRepeatedly(Return(current_topology));
    EXPECT_CALL(*mock_ts, get_topology(mock_reader_a_proxy, true))
        .WillRepeatedly(Return(new_topology));
    EXPECT_CALL(*mock_ts, mark_host_down(writer_host)).Times(1);
    EXPECT_CALL(*mock_ts, mark_host_up(_)).Times(AnyNumber());
    EXPECT_CALL(*mock_ts, mark_host_up(new_writer_host)).Times(1);

    EXPECT_CALL(*mock_reader_handler, get_reader_connection(_, _))
        .WillRepeatedly(Return(READER_FAILOVER_RESULT(true, reader_a_host,
                                                    mock_reader_a_proxy)));

    FAILOVER_WRITER_HANDLER writer_handler(
        mock_ts, mock_reader_handler, mock_connection_handler, 60000, 5000, 5000, 0);
    auto result = writer_handler.failover(current_topology);

    EXPECT_TRUE(result.connected);
    EXPECT_TRUE(result.is_new_host);
    EXPECT_THAT(result.new_connection, mock_new_writer_proxy);
    EXPECT_EQ(3, result.new_topology->total_hosts());
    EXPECT_EQ(new_writer_instance_name,
            result.new_topology->get_writer()->instance_name);

    // Explicit delete on new writer connection as it's returned as a valid result
    delete mock_new_writer_proxy;
}

// Verify that writer failover handler can re-connect to a new writer node.
// topology: changes to [new-writer, initial-writer, reader-A, reader-B] 
// taskA: successfully reconnect, but initial-writer is now a reader (defer to taskB) 
// taskB: successfully connect to readerA and then to new-writer 
// expected test result: new connection to writer by taskB
TEST_F(FailoverWriterHandlerTest, ConnectToReaderA_TaskADefers) {
    mock_writer_proxy = new MOCK_MYSQL_PROXY(dbc, ds);
    mock_new_writer_proxy = new MOCK_MYSQL_PROXY(dbc, ds);
    mock_reader_a_proxy = new MOCK_MYSQL_PROXY(dbc, ds);

    auto new_topology = std::make_shared<CLUSTER_TOPOLOGY_INFO>();
    new_topology->add_host(new_writer_host);
    new_topology->add_host(writer_host);
    new_topology->add_host(reader_a_host);
    new_topology->add_host(reader_b_host);

    EXPECT_CALL(*mock_writer_proxy, is_connected()).WillOnce(Return(true));
    EXPECT_CALL(*mock_writer_proxy, mock_mysql_proxy_destructor());

    EXPECT_CALL(*mock_new_writer_proxy, is_connected()).WillRepeatedly(Return(true));

    EXPECT_CALL(*mock_reader_a_proxy, is_connected()).WillRepeatedly(Return(true));
    EXPECT_CALL(*mock_reader_a_proxy, mock_mysql_proxy_destructor());

    EXPECT_CALL(*mock_connection_handler, connect(writer_host))
        .WillOnce(Return(mock_writer_proxy))
        .WillRepeatedly(Return(nullptr)); // Connection is deleted after first connect
    EXPECT_CALL(*mock_connection_handler, connect(reader_a_host))
        .WillRepeatedly(Return(mock_reader_a_proxy));
    EXPECT_CALL(*mock_connection_handler, connect(new_writer_host))
        .WillRepeatedly(Invoke([&]() {
            std::this_thread::sleep_for(std::chrono::milliseconds(5000));
            return mock_new_writer_proxy;
        }));

    EXPECT_CALL(*mock_ts, get_topology(_, true))
        .WillRepeatedly(Return(new_topology));
    EXPECT_CALL(*mock_ts, mark_host_down(writer_host)).Times(1);
    EXPECT_CALL(*mock_ts, mark_host_up(new_writer_host)).Times(1);

    EXPECT_CALL(*mock_reader_handler, get_reader_connection(_, _))
        .WillRepeatedly(Return(READER_FAILOVER_RESULT(true, reader_a_host,
                                                    mock_reader_a_proxy)));

    FAILOVER_WRITER_HANDLER writer_handler(
        mock_ts, mock_reader_handler, mock_connection_handler, 60000, 5000, 5000, 0);
    auto result = writer_handler.failover(current_topology);

    EXPECT_TRUE(result.connected);
    EXPECT_TRUE(result.is_new_host);
    EXPECT_THAT(result.new_connection, mock_new_writer_proxy);
    EXPECT_EQ(4, result.new_topology->total_hosts());
    EXPECT_EQ(new_writer_instance_name,
            result.new_topology->get_writer()->instance_name);
    
    // Explicit delete on new writer connection as it's returned as a valid result
    delete mock_new_writer_proxy;
}

// Verify that writer failover handler fails to re-connect to any writer node.
// topology: no changes seen by task A, changes to [new-writer, reader-A, reader-B] for taskB
// taskA: fail to re-connect to writer due to failover timeout
// taskB: successfully connect to readerA and then fail to connect to writer due to failover timeout
// expected test result: no connection
TEST_F(FailoverWriterHandlerTest, FailedToConnect_FailoverTimeout) {
    mock_writer_proxy = new MOCK_MYSQL_PROXY(dbc, ds);
    mock_new_writer_proxy = new MOCK_MYSQL_PROXY(dbc, ds);
    mock_reader_a_proxy = new MOCK_MYSQL_PROXY(dbc, ds);
    mock_reader_b_proxy = new MOCK_MYSQL_PROXY(dbc, ds);

    auto new_topology = std::make_shared<CLUSTER_TOPOLOGY_INFO>();
    new_topology->add_host(new_writer_host);
    new_topology->add_host(reader_a_host);
    new_topology->add_host(reader_b_host);

    EXPECT_CALL(*mock_writer_proxy, is_connected()).WillRepeatedly(Return(true));
    EXPECT_CALL(*mock_writer_proxy, mock_mysql_proxy_destructor());

    EXPECT_CALL(*mock_new_writer_proxy, is_connected()).WillRepeatedly(Return(true));
    EXPECT_CALL(*mock_new_writer_proxy, mock_mysql_proxy_destructor());

    EXPECT_CALL(*mock_reader_a_proxy, is_connected()).WillRepeatedly(Return(true));
    EXPECT_CALL(*mock_reader_a_proxy, mock_mysql_proxy_destructor());

    EXPECT_CALL(*mock_reader_b_proxy, is_connected()).WillRepeatedly(Return(true));

    EXPECT_CALL(*mock_connection_handler, connect(writer_host))
        .WillRepeatedly(Invoke([&]() {
            std::this_thread::sleep_for(std::chrono::milliseconds(5000));
            return mock_writer_proxy;
        }));
    EXPECT_CALL(*mock_connection_handler, connect(reader_a_host))
        .WillRepeatedly(Return(mock_reader_a_proxy));
    EXPECT_CALL(*mock_connection_handler, connect(reader_b_host))
        .WillRepeatedly(Return(mock_reader_b_proxy));
    EXPECT_CALL(*mock_connection_handler, connect(new_writer_host))
        .WillRepeatedly(Invoke([&]() {
            std::this_thread::sleep_for(std::chrono::milliseconds(5000));
            return mock_new_writer_proxy;
        }));

    EXPECT_CALL(*mock_ts, get_topology(mock_writer_proxy, _))
        .WillRepeatedly(Return(current_topology));
    EXPECT_CALL(*mock_ts, get_topology(mock_reader_a_proxy, _))
        .WillRepeatedly(Return(new_topology));
    EXPECT_CALL(*mock_ts, mark_host_down(writer_host)).Times(1);
    EXPECT_CALL(*mock_ts, mark_host_up(writer_host)).Times(1);
    EXPECT_CALL(*mock_ts, mark_host_up(new_writer_host)).Times(1);

    EXPECT_CALL(*mock_reader_handler, get_reader_connection(_, _))
        .WillRepeatedly(Return(READER_FAILOVER_RESULT(true, reader_a_host,
                                                    mock_reader_a_proxy)));

    FAILOVER_WRITER_HANDLER writer_handler(
        mock_ts, mock_reader_handler, mock_connection_handler, 1000, 2000, 2000, 0);
    auto result = writer_handler.failover(current_topology);

    EXPECT_FALSE(result.connected);
    EXPECT_FALSE(result.is_new_host);
    EXPECT_THAT(result.new_connection, nullptr);

    // delete reader b explicitly, since get_reader_connection() is mocked
    delete mock_reader_b_proxy;
}

// Verify that writer failover handler fails to re-connect to any writer node.
// topology: changes to [new-writer, reader-A, reader-B] for taskB
// taskA: fail to re-connect to writer
// taskB: successfully connect to readerA and then fail to connect to writer
// expected test result: no connection
TEST_F(FailoverWriterHandlerTest, FailedToConnect_TaskAFailed_TaskBWriterFailed) {
    mock_reader_a_proxy = new MOCK_MYSQL_PROXY(dbc, ds);
    mock_reader_b_proxy = new MOCK_MYSQL_PROXY(dbc, ds);

    auto new_topology = std::make_shared<CLUSTER_TOPOLOGY_INFO>();
    new_topology->add_host(new_writer_host);
    new_topology->add_host(reader_a_host);
    new_topology->add_host(reader_b_host);

    EXPECT_CALL(*mock_reader_a_proxy, is_connected()).WillRepeatedly(Return(true));
    EXPECT_CALL(*mock_reader_a_proxy, mock_mysql_proxy_destructor());

    EXPECT_CALL(*mock_reader_b_proxy, is_connected()).WillRepeatedly(Return(true));

    EXPECT_CALL(*mock_connection_handler, connect(writer_host))
        .WillRepeatedly(Return(nullptr));
    EXPECT_CALL(*mock_connection_handler, connect(reader_a_host))
        .WillRepeatedly(Return(mock_reader_a_proxy));
    EXPECT_CALL(*mock_connection_handler, connect(reader_b_host))
        .WillRepeatedly(Return(mock_reader_b_proxy));
    EXPECT_CALL(*mock_connection_handler, connect(new_writer_host))
        .WillRepeatedly(Return(nullptr));

    EXPECT_CALL(*mock_ts, get_topology(_, _))
        .WillRepeatedly(Return(new_topology));
    EXPECT_CALL(*mock_ts, mark_host_down(writer_host)).Times(1);
    EXPECT_CALL(*mock_ts, mark_host_down(new_writer_host)).Times(AtLeast(1));

    EXPECT_CALL(*mock_reader_handler, get_reader_connection(_, _))
        .WillRepeatedly(Return(READER_FAILOVER_RESULT(true, reader_a_host,
                                                    mock_reader_a_proxy)));

    FAILOVER_WRITER_HANDLER writer_handler(
        mock_ts, mock_reader_handler, mock_connection_handler, 5000, 2000, 2000, 0);
    auto result = writer_handler.failover(current_topology);

    EXPECT_FALSE(result.connected);
    EXPECT_FALSE(result.is_new_host);
    EXPECT_THAT(result.new_connection, nullptr);

    // delete reader b explicitly, since get_reader_connection() is mocked
    delete mock_reader_b_proxy;
}
