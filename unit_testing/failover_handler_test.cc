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

#include "test_utils.h"
#include "mock_objects.h"

using ::testing::_;
using ::testing::AtLeast;
using ::testing::Return;
using ::testing::StrEq;

class FailoverHandlerTest : public testing::Test {
   protected:
    static std::shared_ptr<HOST_INFO> writer_host;
    static std::shared_ptr<HOST_INFO> reader_host;
    static std::shared_ptr<CLUSTER_TOPOLOGY_INFO> topology;
    SQLHENV env;
    DBC* dbc;
    DataSource* ds;
    std::shared_ptr<MOCK_TOPOLOGY_SERVICE> mock_ts;
    std::shared_ptr<MOCK_CONNECTION_HANDLER> mock_connection_handler;
    std::shared_ptr<MOCK_CLUSTER_AWARE_METRICS_CONTAINER> mock_metrics;

    static void SetUpTestSuite() {
        writer_host = std::make_shared<HOST_INFO>("writer-host.com", 1234, UP, true);
        reader_host = std::make_shared<HOST_INFO>("reader-host.com", 1234, UP, false);
        topology = std::make_shared<CLUSTER_TOPOLOGY_INFO>();
        topology->add_host(writer_host);
        topology->add_host(reader_host);
    }

    static void TearDownTestSuite() {}

    void SetUp() override {
        allocate_odbc_handles(env, dbc, ds);
        ds->enable_cluster_failover = true;
        ds->gather_perf_metrics = false;

        mock_ts = std::make_shared<MOCK_TOPOLOGY_SERVICE>();
        mock_connection_handler = std::make_shared<MOCK_CONNECTION_HANDLER>();
        mock_metrics = std::make_shared<MOCK_CLUSTER_AWARE_METRICS_CONTAINER>();
    }

    void TearDown() override {
        cleanup_odbc_handles(env, dbc, ds);
    }
};

std::shared_ptr<HOST_INFO> FailoverHandlerTest::writer_host = nullptr;
std::shared_ptr<HOST_INFO> FailoverHandlerTest::reader_host = nullptr;
std::shared_ptr<CLUSTER_TOPOLOGY_INFO> FailoverHandlerTest::topology = nullptr;

TEST_F(FailoverHandlerTest, NullDBC) {
    EXPECT_THROW(FAILOVER_HANDLER failover_handler(nullptr, ds), std::runtime_error);
}

TEST_F(FailoverHandlerTest, NullDS) {
    EXPECT_THROW(FAILOVER_HANDLER failover_handler(dbc, nullptr), std::runtime_error);
}

TEST_F(FailoverHandlerTest, CustomDomain) {
    SQLCHAR server[] = "my-custom-domain.com";
    SQLCHAR host_pattern[] = "?.my-custom-domain.com";

    ds_setattr_from_utf8(&ds->server, server);
    ds_setattr_from_utf8(&ds->host_pattern, host_pattern);
    ds->port = 1234;

    EXPECT_CALL(*mock_ts, get_topology(_, false)).WillOnce(Return(topology));
    EXPECT_CALL(*mock_ts, set_cluster_instance_template(_))
        .Times(AtLeast(1));
    EXPECT_CALL(*mock_ts, set_cluster_id(_)).Times(0);

    EXPECT_CALL(*mock_connection_handler, do_connect(dbc, ds, false))
        .WillOnce(Return(SQL_SUCCESS));
    EXPECT_CALL(*mock_connection_handler, do_connect(dbc, ds, true))
        .WillOnce(Return(SQL_SUCCESS));

    FAILOVER_HANDLER failover_handler(dbc, ds, mock_connection_handler, mock_ts, mock_metrics);
    failover_handler.init_cluster_info();

    EXPECT_FALSE(failover_handler.is_rds());
    EXPECT_FALSE(failover_handler.is_rds_proxy());
    EXPECT_TRUE(failover_handler.is_cluster_topology_available());
    EXPECT_TRUE(failover_handler.is_failover_enabled());
}

TEST_F(FailoverHandlerTest, FailoverDisabled) {
    ds->enable_cluster_failover = false;

    EXPECT_CALL(*mock_connection_handler, do_connect(dbc, ds, false)).Times(1);

    FAILOVER_HANDLER failover_handler(dbc, ds, mock_connection_handler, mock_ts, mock_metrics);
    failover_handler.init_cluster_info();

    EXPECT_FALSE(failover_handler.is_failover_enabled());
}

TEST_F(FailoverHandlerTest, IP_TopologyAvailable_PatternRequired) {
    SQLCHAR server[] = "10.10.10.10";

    ds_setattr_from_utf8(&ds->server, server);

    EXPECT_CALL(*mock_connection_handler, do_connect(dbc, ds, false))
        .WillOnce(Return(SQL_SUCCESS));
    EXPECT_CALL(*mock_connection_handler, do_connect(dbc, ds, true))
        .WillOnce(Return(SQL_SUCCESS));

    EXPECT_CALL(*mock_ts, get_topology(_, false)).WillOnce(Return(topology));

    FAILOVER_HANDLER failover_handler(dbc, ds, mock_connection_handler, mock_ts, mock_metrics);
    EXPECT_THROW(failover_handler.init_cluster_info(), std::runtime_error);
}

TEST_F(FailoverHandlerTest, IP_TopologyNotAvailable) {
    SQLCHAR server[] = "10.10.10.10";

    ds_setattr_from_utf8(&ds->server, server);

    EXPECT_CALL(*mock_connection_handler, do_connect(dbc, ds, false))
        .WillOnce(Return(SQL_SUCCESS));

    EXPECT_CALL(*mock_ts, get_topology(_, false))
        .WillOnce(Return(std::make_shared<CLUSTER_TOPOLOGY_INFO>()));

    FAILOVER_HANDLER failover_handler(dbc, ds, mock_connection_handler, mock_ts, mock_metrics);
    failover_handler.init_cluster_info();

    EXPECT_FALSE(failover_handler.is_rds());
    EXPECT_FALSE(failover_handler.is_rds_proxy());
    EXPECT_FALSE(failover_handler.is_cluster_topology_available());
    EXPECT_FALSE(failover_handler.is_failover_enabled());
}

TEST_F(FailoverHandlerTest, IP_Cluster) {
    SQLCHAR server[] = "10.10.10.10";
    SQLCHAR host_pattern[] = "?.my-custom-domain.com";

    ds_setattr_from_utf8(&ds->server, server);
    ds_setattr_from_utf8(&ds->host_pattern, host_pattern);

    EXPECT_CALL(*mock_connection_handler, do_connect(dbc, ds, false))
        .WillOnce(Return(SQL_SUCCESS));
    EXPECT_CALL(*mock_connection_handler, do_connect(dbc, ds, true))
        .WillOnce(Return(SQL_SUCCESS));

    EXPECT_CALL(*mock_ts, get_topology(_, false)).WillOnce(Return(topology));
    EXPECT_CALL(*mock_ts, set_cluster_instance_template(_)).Times(AtLeast(1));
    EXPECT_CALL(*mock_ts, set_cluster_id(_)).Times(0);

    FAILOVER_HANDLER failover_handler(dbc, ds, mock_connection_handler, mock_ts, mock_metrics);
    failover_handler.init_cluster_info();

    EXPECT_FALSE(failover_handler.is_rds());
    EXPECT_FALSE(failover_handler.is_rds_proxy());
    EXPECT_TRUE(failover_handler.is_cluster_topology_available());
    EXPECT_TRUE(failover_handler.is_failover_enabled());
}

TEST_F(FailoverHandlerTest, IP_Cluster_ClusterID) {
    SQLCHAR server[] = "10.10.10.10";
    SQLCHAR host_pattern[] = "?.my-custom-domain.com";
    SQLCHAR cluster_id[] = "test-cluster-id";

    ds_setattr_from_utf8(&ds->server, server);
    ds_setattr_from_utf8(&ds->host_pattern, host_pattern);
    ds_setattr_from_utf8(&ds->cluster_id, cluster_id);

    EXPECT_CALL(*mock_connection_handler, do_connect(dbc, ds, false))
        .WillOnce(Return(SQL_SUCCESS));
    EXPECT_CALL(*mock_connection_handler, do_connect(dbc, ds, true))
        .WillOnce(Return(SQL_SUCCESS));

    EXPECT_CALL(*mock_ts, get_topology(_, false)).WillOnce(Return(topology));
    EXPECT_CALL(*mock_ts, set_cluster_instance_template(_)).Times(AtLeast(1));
    EXPECT_CALL(*mock_ts, set_cluster_id(StrEq(reinterpret_cast<const char*>(cluster_id)))).Times(AtLeast(1));

    FAILOVER_HANDLER failover_handler(dbc, ds, mock_connection_handler, mock_ts, mock_metrics);
    failover_handler.init_cluster_info();

    EXPECT_FALSE(failover_handler.is_rds());
    EXPECT_FALSE(failover_handler.is_rds_proxy());
    EXPECT_TRUE(failover_handler.is_cluster_topology_available());
    EXPECT_TRUE(failover_handler.is_failover_enabled());
}

TEST_F(FailoverHandlerTest, RDS_Cluster) {
    SQLCHAR server[] = "my-cluster-name.cluster-XYZ.us-east-2.rds.amazonaws.com";

    ds_setattr_from_utf8(&ds->server, server);
    ds->port = 1234;

    EXPECT_CALL(*mock_connection_handler, do_connect(dbc, ds, false))
        .WillOnce(Return(SQL_SUCCESS));
    EXPECT_CALL(*mock_connection_handler, do_connect(dbc, ds, true))
        .WillOnce(Return(SQL_SUCCESS));

    EXPECT_CALL(*mock_ts, get_topology(_, false)).WillOnce(Return(topology));
    EXPECT_CALL(*mock_ts, set_cluster_instance_template(_)).Times(AtLeast(1));
    EXPECT_CALL(*mock_ts, set_cluster_id(StrEq("my-cluster-name.cluster-XYZ.us-east-2.rds.amazonaws.com:1234"))).Times(AtLeast(1));

    FAILOVER_HANDLER failover_handler(dbc, ds, mock_connection_handler, mock_ts, mock_metrics);
    failover_handler.init_cluster_info();

    EXPECT_TRUE(failover_handler.is_rds());
    EXPECT_FALSE(failover_handler.is_rds_proxy());
    EXPECT_TRUE(failover_handler.is_cluster_topology_available());
    EXPECT_TRUE(failover_handler.is_failover_enabled());
}

TEST_F(FailoverHandlerTest, RDS_CustomCluster) {
    SQLCHAR server[] = "my-custom-cluster-name.cluster-custom-XYZ.us-east-2.rds.amazonaws.com";

    ds_setattr_from_utf8(&ds->server, server);

    EXPECT_CALL(*mock_connection_handler, do_connect(dbc, ds, false))
        .WillOnce(Return(SQL_SUCCESS));
    EXPECT_CALL(*mock_connection_handler, do_connect(dbc, ds, true))
        .WillOnce(Return(SQL_SUCCESS));

    EXPECT_CALL(*mock_ts, get_topology(_, false)).WillOnce(Return(topology));
    EXPECT_CALL(*mock_ts, set_cluster_instance_template(_)).Times(AtLeast(1));
    EXPECT_CALL(*mock_ts, set_cluster_id(_)).Times(1);

    FAILOVER_HANDLER failover_handler(dbc, ds, mock_connection_handler, mock_ts, mock_metrics);
    failover_handler.init_cluster_info();

    EXPECT_TRUE(failover_handler.is_rds());
    EXPECT_FALSE(failover_handler.is_rds_proxy());
    EXPECT_TRUE(failover_handler.is_cluster_topology_available());
    EXPECT_TRUE(failover_handler.is_failover_enabled());
}

TEST_F(FailoverHandlerTest, RDS_Instance) {
    SQLCHAR server[] = "my-instance-name.XYZ.us-east-2.rds.amazonaws.com";

    ds_setattr_from_utf8(&ds->server, server);

    EXPECT_CALL(*mock_connection_handler, do_connect(dbc, ds, false))
        .WillOnce(Return(SQL_SUCCESS));
    EXPECT_CALL(*mock_connection_handler, do_connect(dbc, ds, true))
        .WillOnce(Return(SQL_SUCCESS));

    EXPECT_CALL(*mock_ts, get_topology(_, false)).WillOnce(Return(topology));
    EXPECT_CALL(*mock_ts, set_cluster_instance_template(_)).Times(AtLeast(1));
    EXPECT_CALL(*mock_ts, set_cluster_id(_)).Times(1);

    FAILOVER_HANDLER failover_handler(dbc, ds, mock_connection_handler, mock_ts, mock_metrics);
    failover_handler.init_cluster_info();

    EXPECT_TRUE(failover_handler.is_rds());
    EXPECT_FALSE(failover_handler.is_rds_proxy());
    EXPECT_TRUE(failover_handler.is_cluster_topology_available());
    EXPECT_TRUE(failover_handler.is_failover_enabled());
}

TEST_F(FailoverHandlerTest, RDS_Proxy) {
    SQLCHAR server[] = "test-proxy.proxy-XYZ.us-east-2.rds.amazonaws.com";

    ds_setattr_from_utf8(&ds->server, server);
    ds->port = 1234;

    EXPECT_CALL(*mock_connection_handler, do_connect(dbc, ds, false))
        .WillOnce(Return(SQL_SUCCESS));

    EXPECT_CALL(*mock_ts, get_topology(_, false)).WillOnce(Return(topology));
    EXPECT_CALL(*mock_ts, set_cluster_instance_template(_)).Times(AtLeast(1));
    EXPECT_CALL(*mock_ts, set_cluster_id(StrEq("test-proxy.proxy-XYZ.us-east-2.rds.amazonaws.com:1234"))).Times(AtLeast(1));

    FAILOVER_HANDLER failover_handler(dbc, ds, mock_connection_handler, mock_ts, mock_metrics);
    failover_handler.init_cluster_info();

    EXPECT_TRUE(failover_handler.is_rds());
    EXPECT_TRUE(failover_handler.is_rds_proxy());
    EXPECT_TRUE(failover_handler.is_cluster_topology_available());
    EXPECT_FALSE(failover_handler.is_failover_enabled());
}

TEST_F(FailoverHandlerTest, RDS_ReaderCluster) {
    SQLCHAR server[] = "my-cluster-name.cluster-XYZ.us-east-2.rds.amazonaws.com";

    ds_setattr_from_utf8(&ds->server, server);
    ds->port = 1234;

    EXPECT_CALL(*mock_connection_handler, do_connect(dbc, ds, false))
        .WillOnce(Return(SQL_SUCCESS));
    EXPECT_CALL(*mock_connection_handler, do_connect(dbc, ds, true))
        .WillOnce(Return(SQL_SUCCESS));

    EXPECT_CALL(*mock_ts, get_topology(_, false)).WillOnce(Return(topology));
    EXPECT_CALL(*mock_ts, set_cluster_instance_template(_)).Times(AtLeast(1));
    EXPECT_CALL(*mock_ts,
                set_cluster_id(StrEq(
                    "my-cluster-name.cluster-XYZ.us-east-2.rds.amazonaws.com:1234")))
        .Times(AtLeast(1));

    FAILOVER_HANDLER failover_handler(dbc, ds, mock_connection_handler, mock_ts, mock_metrics);
    failover_handler.init_cluster_info();

    EXPECT_TRUE(failover_handler.is_rds());
    EXPECT_FALSE(failover_handler.is_rds_proxy());
    EXPECT_TRUE(failover_handler.is_cluster_topology_available());
    EXPECT_TRUE(failover_handler.is_failover_enabled());
}

TEST_F(FailoverHandlerTest, RDS_MultiWriterCluster) {
    SQLCHAR server[] = "my-cluster-name.cluster-XYZ.us-east-2.rds.amazonaws.com";

    ds_setattr_from_utf8(&ds->server, server);
    ds->port = 1234;
    ds->enable_cluster_failover = true;

    EXPECT_CALL(*mock_connection_handler, do_connect(dbc, ds, false)).WillOnce(Return(SQL_SUCCESS));

    auto multi_writer_topology = std::make_shared<CLUSTER_TOPOLOGY_INFO>();
    multi_writer_topology->add_host(writer_host);
    multi_writer_topology->add_host(reader_host);
    multi_writer_topology->is_multi_writer_cluster = true;

    EXPECT_CALL(*mock_ts, get_topology(_, false))
        .WillOnce(Return(multi_writer_topology));
    EXPECT_CALL(*mock_ts, set_cluster_instance_template(_)).Times(AtLeast(1));
    EXPECT_CALL(*mock_ts,
        set_cluster_id(StrEq(
            "my-cluster-name.cluster-XYZ.us-east-2.rds.amazonaws.com:1234")))
        .Times(AtLeast(1));

    FAILOVER_HANDLER failover_handler(dbc, ds, mock_connection_handler, mock_ts, mock_metrics);
    failover_handler.init_cluster_info();

    EXPECT_TRUE(failover_handler.is_rds());
    EXPECT_FALSE(failover_handler.is_rds_proxy());
    EXPECT_TRUE(failover_handler.is_cluster_topology_available());
    EXPECT_FALSE(failover_handler.is_failover_enabled());
}

TEST_F(FailoverHandlerTest, ReconnectWithFailoverSettings) {
    SQLCHAR server[] = "10.10.10.10";
    SQLCHAR host_pattern[] = "?.my-custom-domain.com";

    ds_setattr_from_utf8(&ds->server, server);
    ds_setattr_from_utf8(&ds->host_pattern, host_pattern);
    ds->connect_timeout = 100;
    ds->network_timeout = 100;

    EXPECT_CALL(*mock_ts, get_topology(_, false)).WillOnce(Return(topology));
    EXPECT_CALL(*mock_ts, set_cluster_instance_template(_))
        .Times(AtLeast(1));

    EXPECT_CALL(*mock_connection_handler, do_connect(dbc, ds, false))
        .WillOnce(Return(SQL_SUCCESS));

    EXPECT_CALL(*mock_connection_handler, do_connect(dbc, ds, true))
        .WillOnce(Return(SQL_SUCCESS));

    FAILOVER_HANDLER failover_handler(dbc, ds, mock_connection_handler, mock_ts, mock_metrics);
    failover_handler.init_cluster_info();

    EXPECT_TRUE(failover_handler.is_failover_enabled());
}
