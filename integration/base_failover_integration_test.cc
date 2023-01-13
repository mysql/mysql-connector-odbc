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

// Those classes need to be included first in Windows
#include <aws/rds/model/TargetState.h>
#include <aws/rds/model/TargetHealth.h>

#include <aws/rds/RDSClient.h>
#include <toxiproxy/toxiproxy_client.h>

#include <aws/core/Aws.h>
#include <aws/core/auth/AWSCredentialsProvider.h>
#include <aws/rds/model/DBCluster.h>
#include <aws/rds/model/DBClusterMember.h>
#include <aws/rds/model/DescribeDBClustersRequest.h>
#include <aws/rds/model/FailoverDBClusterRequest.h>

#include <gtest/gtest.h>
#include <cassert>
#include <climits>
#include <cstdlib>
#include <stdexcept>
#include <sql.h>
#include <sqlext.h>

#include "connection_string_builder.cc"

#define MAX_NAME_LEN 255
#define SQL_MAX_MESSAGE_LENGTH 512

#define AS_SQLCHAR(str) const_cast<SQLCHAR*>(reinterpret_cast<const SQLCHAR*>(str))

static int str_to_int(const char* str) {
    const long int x = strtol(str, nullptr, 10);
    assert(x <= INT_MAX);
    assert(x >= INT_MIN);
    return static_cast<int>(x);
}

static std::string DOWN_STREAM_STR = "DOWNSTREAM";
static std::string UP_STREAM_STR = "UPSTREAM";

static Aws::SDKOptions options;

class BaseFailoverIntegrationTest : public testing::Test {
protected:
  // Connection string parameters
  char* dsn = std::getenv("TEST_DSN");
  char* db = std::getenv("TEST_DATABASE");
  char* user = std::getenv("TEST_UID");
  char* pwd = std::getenv("TEST_PASSWORD");

  std::string MYSQL_INSTANCE_1_URL = std::getenv("MYSQL_INSTANCE_1_URL");
  std::string MYSQL_INSTANCE_2_URL = std::getenv("MYSQL_INSTANCE_2_URL");
  std::string MYSQL_INSTANCE_3_URL = std::getenv("MYSQL_INSTANCE_3_URL");
  std::string MYSQL_INSTANCE_4_URL = std::getenv("MYSQL_INSTANCE_4_URL");
  std::string MYSQL_INSTANCE_5_URL = std::getenv("MYSQL_INSTANCE_5_URL");
  std::string MYSQL_CLUSTER_URL = std::getenv("TEST_SERVER");
  std::string MYSQL_RO_CLUSTER_URL = std::getenv("TEST_RO_SERVER");

  std::string PROXIED_DOMAIN_NAME_SUFFIX = std::getenv("PROXIED_DOMAIN_NAME_SUFFIX");
  std::string PROXIED_CLUSTER_TEMPLATE = std::getenv("PROXIED_CLUSTER_TEMPLATE");
  std::string DB_CONN_STR_SUFFIX = std::getenv("DB_CONN_STR_SUFFIX");

  int MYSQL_PORT = str_to_int(std::getenv("MYSQL_PORT"));
  int MYSQL_PROXY_PORT = str_to_int(std::getenv("MYSQL_PROXY_PORT"));
  Aws::String cluster_id = MYSQL_CLUSTER_URL.substr(0, MYSQL_CLUSTER_URL.find('.'));

  static const int GLOBAL_FAILOVER_TIMEOUT = 120000;

  ConnectionStringBuilder builder;
  std::string connection_string;

  SQLCHAR conn_in[4096] = "\0", conn_out[4096] = "\0", sqlstate[6] = "\0", message[SQL_MAX_MESSAGE_LENGTH] = "\0";
  SQLINTEGER native_error = 0;
  SQLSMALLINT len = 0, length = 0;

  std::string TOXIPROXY_INSTANCE_1_NETWORK_ALIAS = std::getenv("TOXIPROXY_INSTANCE_1_NETWORK_ALIAS");
  std::string TOXIPROXY_INSTANCE_2_NETWORK_ALIAS = std::getenv("TOXIPROXY_INSTANCE_2_NETWORK_ALIAS");
  std::string TOXIPROXY_INSTANCE_3_NETWORK_ALIAS = std::getenv("TOXIPROXY_INSTANCE_3_NETWORK_ALIAS");
  std::string TOXIPROXY_INSTANCE_4_NETWORK_ALIAS = std::getenv("TOXIPROXY_INSTANCE_4_NETWORK_ALIAS");
  std::string TOXIPROXY_INSTANCE_5_NETWORK_ALIAS = std::getenv("TOXIPROXY_INSTANCE_5_NETWORK_ALIAS");
  std::string TOXIPROXY_CLUSTER_NETWORK_ALIAS = std::getenv("TOXIPROXY_CLUSTER_NETWORK_ALIAS");
  std::string TOXIPROXY_RO_CLUSTER_NETWORK_ALIAS = std::getenv("TOXIPROXY_RO_CLUSTER_NETWORK_ALIAS");

  TOXIPROXY::TOXIPROXY_CLIENT* toxiproxy_client_instance_1 = new TOXIPROXY::TOXIPROXY_CLIENT(TOXIPROXY_INSTANCE_1_NETWORK_ALIAS);
  TOXIPROXY::TOXIPROXY_CLIENT* toxiproxy_client_instance_2 = new TOXIPROXY::TOXIPROXY_CLIENT(TOXIPROXY_INSTANCE_2_NETWORK_ALIAS);
  TOXIPROXY::TOXIPROXY_CLIENT* toxiproxy_client_instance_3 = new TOXIPROXY::TOXIPROXY_CLIENT(TOXIPROXY_INSTANCE_3_NETWORK_ALIAS);
  TOXIPROXY::TOXIPROXY_CLIENT* toxiproxy_client_instance_4 = new TOXIPROXY::TOXIPROXY_CLIENT(TOXIPROXY_INSTANCE_4_NETWORK_ALIAS);
  TOXIPROXY::TOXIPROXY_CLIENT* toxiproxy_client_instance_5 = new TOXIPROXY::TOXIPROXY_CLIENT(TOXIPROXY_INSTANCE_5_NETWORK_ALIAS);
  TOXIPROXY::TOXIPROXY_CLIENT* toxiproxy_cluster = new TOXIPROXY::TOXIPROXY_CLIENT(TOXIPROXY_CLUSTER_NETWORK_ALIAS);
  TOXIPROXY::TOXIPROXY_CLIENT* toxiproxy_read_only_cluster = new TOXIPROXY::TOXIPROXY_CLIENT(TOXIPROXY_RO_CLUSTER_NETWORK_ALIAS);

  TOXIPROXY::PROXY* proxy_instance_1 = get_proxy(toxiproxy_client_instance_1, MYSQL_INSTANCE_1_URL, MYSQL_PORT);
  TOXIPROXY::PROXY* proxy_instance_2 = get_proxy(toxiproxy_client_instance_2, MYSQL_INSTANCE_2_URL, MYSQL_PORT);
  TOXIPROXY::PROXY* proxy_instance_3 = get_proxy(toxiproxy_client_instance_3, MYSQL_INSTANCE_3_URL, MYSQL_PORT);
  TOXIPROXY::PROXY* proxy_instance_4 = get_proxy(toxiproxy_client_instance_4, MYSQL_INSTANCE_4_URL, MYSQL_PORT);
  TOXIPROXY::PROXY* proxy_instance_5 = get_proxy(toxiproxy_client_instance_5, MYSQL_INSTANCE_5_URL, MYSQL_PORT);
  TOXIPROXY::PROXY* proxy_cluster = get_proxy(toxiproxy_cluster, MYSQL_CLUSTER_URL, MYSQL_PORT);
  TOXIPROXY::PROXY* proxy_read_only_cluster = get_proxy(toxiproxy_read_only_cluster, MYSQL_RO_CLUSTER_URL, MYSQL_PORT);

  std::map<std::string, TOXIPROXY::PROXY*> proxy_map = {
    {MYSQL_INSTANCE_1_URL.substr(0, MYSQL_INSTANCE_1_URL.find('.')), proxy_instance_1},
    {MYSQL_INSTANCE_2_URL.substr(0, MYSQL_INSTANCE_2_URL.find('.')), proxy_instance_2},
    {MYSQL_INSTANCE_3_URL.substr(0, MYSQL_INSTANCE_3_URL.find('.')), proxy_instance_3},
    {MYSQL_INSTANCE_4_URL.substr(0, MYSQL_INSTANCE_4_URL.find('.')), proxy_instance_4},
    {MYSQL_INSTANCE_5_URL.substr(0, MYSQL_INSTANCE_5_URL.find('.')), proxy_instance_5},
    {MYSQL_CLUSTER_URL, proxy_cluster},
    {MYSQL_RO_CLUSTER_URL, proxy_read_only_cluster}
  };

  std::vector<std::string> cluster_instances;
  std::string writer_id;
  std::string writer_endpoint;
  std::vector<std::string> readers;
  std::string reader_id;
  std::string reader_endpoint;

  // Queries
  SQLCHAR* SERVER_ID_QUERY = AS_SQLCHAR("SELECT @@aurora_server_id");

  // Error codes
  const std::string ERROR_COMM_LINK_FAILURE = "08S01";
  const std::string ERROR_COMM_LINK_CHANGED = "08S02";
  const std::string ERROR_CONN_FAILURE_DURING_TX = "08007";

  // Helper functions

  std::string get_endpoint(const std::string& instance_id) const {
    return instance_id + DB_CONN_STR_SUFFIX;
  }

  std::string get_proxied_endpoint(const std::string& instance_id) const {
    return instance_id + DB_CONN_STR_SUFFIX + PROXIED_DOMAIN_NAME_SUFFIX;
  }

  static std::string get_writer_id(std::vector<std::string> instances) {
    if (instances.empty()) {
      throw std::runtime_error("The input cluster topology is empty.");
    }
    return instances[0];
  }

  static std::vector<std::string> get_readers(std::vector<std::string> instances) {
    if (instances.size() < 2) {
      throw std::runtime_error("The input cluster topology does not contain a reader.");
    }
    const std::vector<std::string>::const_iterator first_reader = instances.begin() + 1;
    const std::vector<std::string>::const_iterator last_reader = instances.end();
    std::vector<std::string> readers_list(first_reader, last_reader);
    return readers_list;
  }

  static std::string get_first_reader_id(std::vector<std::string> instances) {
    if (instances.size() < 2) {
      throw std::runtime_error("The input cluster topology does not contain a reader.");
    }
    return instances[1];
  }

  void assert_query_succeeded(const SQLHDBC dbc, SQLCHAR* query) const {
    SQLHSTMT handle;
    EXPECT_EQ(SQL_SUCCESS, SQLAllocHandle(SQL_HANDLE_STMT, dbc, &handle));
    EXPECT_EQ(SQL_SUCCESS, SQLExecDirect(handle, query, SQL_NTS));
    EXPECT_EQ(SQL_SUCCESS, SQLFreeHandle(SQL_HANDLE_STMT, handle));
  }

  void assert_query_failed(const SQLHDBC dbc, SQLCHAR* query, const std::string& expected_error) const {
    SQLHSTMT handle;
    SQLSMALLINT stmt_length;
    SQLINTEGER native_err;
    SQLCHAR msg[SQL_MAX_MESSAGE_LENGTH] = "\0", state[6] = "\0";

    EXPECT_EQ(SQL_SUCCESS, SQLAllocHandle(SQL_HANDLE_STMT, dbc, &handle));
    EXPECT_EQ(SQL_ERROR, SQLExecDirect(handle, query, SQL_NTS));
    EXPECT_EQ(SQL_SUCCESS, SQLError(nullptr, nullptr, handle, state, &native_err, msg, SQL_MAX_MESSAGE_LENGTH - 1, &stmt_length));
    const std::string state_str = reinterpret_cast<char*>(state);
    EXPECT_EQ(expected_error, state_str);
    EXPECT_EQ(SQL_SUCCESS, SQLFreeHandle(SQL_HANDLE_STMT, handle));
  }

  std::string query_instance_id(const SQLHDBC dbc) const {
    SQLCHAR buf[255] = "\0";
    SQLLEN buflen;
    SQLHSTMT handle;
    EXPECT_EQ(SQL_SUCCESS, SQLAllocHandle(SQL_HANDLE_STMT, dbc, &handle));
    EXPECT_EQ(SQL_SUCCESS, SQLExecDirect(handle, SERVER_ID_QUERY, SQL_NTS));
    EXPECT_EQ(SQL_SUCCESS, SQLFetch(handle));
    EXPECT_EQ(SQL_SUCCESS, SQLGetData(handle, 1, SQL_CHAR, buf, sizeof(buf), &buflen));
    EXPECT_EQ(SQL_SUCCESS, SQLFreeHandle(SQL_HANDLE_STMT, handle));
    std::string id(reinterpret_cast<char*>(buf));
    return id;
  }

  // Helper functions from integration tests

  static std::vector<std::string> retrieve_topology_via_SDK(const Aws::RDS::RDSClient& client, const Aws::String& cluster_id) {
    std::vector<std::string> instances;

    std::string writer;
    std::vector<std::string> readers;

    Aws::RDS::Model::DescribeDBClustersRequest rds_req;
    rds_req.WithDBClusterIdentifier(cluster_id);
    auto outcome = client.DescribeDBClusters(rds_req);

    if (!outcome.IsSuccess()) {
      throw std::runtime_error("Unable to get cluster topology using SDK.");
    }

    const auto result = outcome.GetResult();
    const Aws::RDS::Model::DBCluster cluster = result.GetDBClusters()[0];

    for (const auto& instance : cluster.GetDBClusterMembers()) {
      std::string instance_id(instance.GetDBInstanceIdentifier());
      if (instance.GetIsClusterWriter()) {
        writer = instance_id;
      } else {
        readers.push_back(instance_id);
      }
    }

    instances.push_back(writer);
    for (const auto& reader : readers) {
      instances.push_back(reader);
    }
    return instances;
  }

  static Aws::RDS::Model::DBCluster get_DB_cluster(const Aws::RDS::RDSClient& client, const Aws::String& cluster_id) {
    Aws::RDS::Model::DescribeDBClustersRequest rds_req;
    rds_req.WithDBClusterIdentifier(cluster_id);
    auto outcome = client.DescribeDBClusters(rds_req);
    const auto result = outcome.GetResult();
    return result.GetDBClusters().at(0);
  }

  static void wait_until_cluster_has_right_state(const Aws::RDS::RDSClient& client, const Aws::String& cluster_id) {
    Aws::String status = get_DB_cluster(client, cluster_id).GetStatus();

    while (status != "available") {
      std::this_thread::sleep_for(std::chrono::seconds(1));
      status = get_DB_cluster(client, cluster_id).GetStatus();
    }
  }

  static Aws::RDS::Model::DBClusterMember get_DB_cluster_writer_instance(const Aws::RDS::RDSClient& client, const Aws::String& cluster_id) {
    Aws::RDS::Model::DBClusterMember instance;
    const Aws::RDS::Model::DBCluster cluster = get_DB_cluster(client, cluster_id);
    for (const auto& member : cluster.GetDBClusterMembers()) {
      if (member.GetIsClusterWriter()) {
        return member;
      }
    }
    return instance;
  }

  static Aws::String get_DB_cluster_writer_instance_id(const Aws::RDS::RDSClient& client, const Aws::String& cluster_id) {
    return get_DB_cluster_writer_instance(client, cluster_id).GetDBInstanceIdentifier();
  }

  static void wait_until_writer_instance_changed(const Aws::RDS::RDSClient& client, const Aws::String& cluster_id,
                      const Aws::String& initial_writer_instance_id) {
    Aws::String next_cluster_writer_id = get_DB_cluster_writer_instance_id(client, cluster_id);
    while (initial_writer_instance_id == next_cluster_writer_id) {
      std::this_thread::sleep_for(std::chrono::seconds(3));
      next_cluster_writer_id = get_DB_cluster_writer_instance_id(client, cluster_id);
    }
  }

  static void failover_cluster(const Aws::RDS::RDSClient& client, const Aws::String& cluster_id, const Aws::String& target_instance_id = "") {
    wait_until_cluster_has_right_state(client, cluster_id);
    Aws::RDS::Model::FailoverDBClusterRequest rds_req;
    rds_req.WithDBClusterIdentifier(cluster_id);
    if (!target_instance_id.empty()) {
      rds_req.WithTargetDBInstanceIdentifier(target_instance_id);
    }
    auto outcome = client.FailoverDBCluster(rds_req);
  }

  static void failover_cluster_and_wait_until_writer_changed(const Aws::RDS::RDSClient& client, const Aws::String& cluster_id,
                            const Aws::String& cluster_writer_id,
                            const Aws::String& target_writer_id = "") {
    failover_cluster(client, cluster_id, target_writer_id);
    wait_until_writer_instance_changed(client, cluster_id, cluster_writer_id);
  }

  static Aws::RDS::Model::DBClusterMember get_matched_DBClusterMember(const Aws::RDS::RDSClient& client, const Aws::String& cluster_id,
                                 const Aws::String& instance_id) {
    Aws::RDS::Model::DBClusterMember instance;
    const Aws::RDS::Model::DBCluster cluster = get_DB_cluster(client, cluster_id);
    for (const auto& member : cluster.GetDBClusterMembers()) {
      Aws::String member_id = member.GetDBInstanceIdentifier();
      if (member_id == instance_id) {
        return member;
      }
    }
    return instance;
  }

  static bool is_DB_instance_writer(const Aws::RDS::RDSClient& client, const Aws::String& cluster_id, const Aws::String& instance_id) {
    return get_matched_DBClusterMember(client, cluster_id, instance_id).GetIsClusterWriter();
  }

  static bool is_DB_instance_reader(const Aws::RDS::RDSClient& client, const Aws::String& cluster_id, const Aws::String& instance_id) {
    return !get_matched_DBClusterMember(client, cluster_id, instance_id).GetIsClusterWriter();
  }

  static int query_count_table_rows(const SQLHSTMT handle, const char* table_name, const int id = -1) {
    EXPECT_FALSE(table_name[0] == '\0');

    //TODO Investigate how to use Prepared Statements to protect against SQL injection
    char select_count_query[256];
    if (id == -1) {
      sprintf(select_count_query, "SELECT count(*) FROM %s", table_name);
    } else {
      sprintf(select_count_query, "SELECT count(*) FROM %s WHERE id = %d", table_name, id);
    }

    EXPECT_EQ(SQL_SUCCESS, SQLExecDirect(handle, AS_SQLCHAR(select_count_query), SQL_NTS));
    const auto rc = SQLFetch(handle);

    SQLINTEGER buf = -1;
    SQLLEN buflen;
    if (rc != SQL_NO_DATA_FOUND && rc != SQL_ERROR) {
      EXPECT_EQ(SQL_SUCCESS, SQLGetData(handle, 1, SQL_INTEGER, &buf, sizeof(buf), &buflen));
      SQLFetch(handle); // To get cursor in correct position
    }
    return buf;
  }

  // Helper functions from network integration tests

  static TOXIPROXY::PROXY* get_proxy(TOXIPROXY::TOXIPROXY_CLIENT* client, const std::string& host, const int port) {
    const std::string upstream = host + ":" + std::to_string(port);
    return client->get_proxy(upstream);
  }

  TOXIPROXY::PROXY* get_proxy_from_map(const std::string& url) {
    const auto it = proxy_map.find(url);
    if (it != proxy_map.end()) {
      return it->second;
    }
    return nullptr;
  }

  bool is_db_instance_writer(const std::string& instance) const {
    return writer_id == instance;
  }

  bool is_db_instance_reader(const std::string& instance) const {
    for (const auto& reader : readers) {
      if (reader == instance) {
        return true;
      }
    }
    return false;
  }

  void disable_instance(const std::string& instance) {
    TOXIPROXY::PROXY* new_instance = get_proxy_from_map(instance);
    if (new_instance) {
      disable_connectivity(new_instance);
    } else {
      FAIL() << instance << " does not have a proxy setup.";
    }
  }

  static void disable_connectivity(const TOXIPROXY::PROXY* proxy) {
    const auto toxics = proxy->get_toxics();
    if (toxics) {
      toxics->bandwidth(DOWN_STREAM_STR, TOXIPROXY::TOXIC_DIRECTION::DOWNSTREAM, 0);
      toxics->bandwidth(UP_STREAM_STR, TOXIPROXY::TOXIC_DIRECTION::UPSTREAM, 0);
    }
  }

  void enable_instance(const std::string& instance) {
    TOXIPROXY::PROXY* new_instance = get_proxy_from_map(instance);
    if (new_instance) {
      enable_connectivity(new_instance);
    } else {
      FAIL() << instance << " does not have a proxy setup.";
    }
  }

  static void enable_connectivity(const TOXIPROXY::PROXY* proxy) {
    TOXIPROXY::TOXIC_LIST* toxics = proxy->get_toxics();

    if (toxics) {
      TOXIPROXY::TOXIC* downstream = toxics->get(DOWN_STREAM_STR);
      TOXIPROXY::TOXIC* upstream = toxics->get(UP_STREAM_STR);

      if (downstream) {
        downstream->remove();
      }
      if (upstream) {
        upstream->remove();
      }
    }
  }

  std::string get_default_config(const int connect_timeout = 10, const int network_timeout = 10) const {
    char template_connection[4096];
    sprintf(template_connection, "DSN=%s;UID=%s;PWD=%s;LOG_QUERY=1;CONNECT_TIMEOUT=%d;NETWORK_TIMEOUT=%d;", dsn, user, pwd, connect_timeout, network_timeout);
    std::string config(template_connection);
    return config;
  }

  std::string get_default_proxied_config() const {
    char template_connection[4096];
    sprintf(template_connection, "%sHOST_PATTERN=%s;", get_default_config().c_str(), PROXIED_CLUSTER_TEMPLATE.c_str());
    std::string config(template_connection);
    return config;
  }

  void test_connection(const SQLHDBC dbc, const std::string& test_server, const int test_port) {
    sprintf(reinterpret_cast<char*>(conn_in), "%sSERVER=%s;PORT=%d;", get_default_proxied_config().c_str(), test_server.c_str(), test_port);
    EXPECT_EQ(SQL_SUCCESS, SQLDriverConnect(dbc, nullptr, conn_in, SQL_NTS, conn_out, MAX_NAME_LEN, &len, SQL_DRIVER_NOPROMPT));
    EXPECT_EQ(SQL_SUCCESS, SQLDisconnect(dbc));
  }

  static void assert_is_new_reader(const std::vector<std::string>& old_readers, const std::string& new_reader) {
    for (const auto& reader : old_readers) {
      EXPECT_NE(reader, new_reader);
    }
  }

public:
  ~BaseFailoverIntegrationTest() override {
    delete toxiproxy_client_instance_1;
    delete toxiproxy_client_instance_2;
    delete toxiproxy_client_instance_3;
    delete toxiproxy_client_instance_4;
    delete toxiproxy_client_instance_5;
    delete toxiproxy_cluster;
    delete toxiproxy_read_only_cluster;
    
    delete proxy_instance_1;
    delete proxy_instance_2;
    delete proxy_instance_3;
    delete proxy_instance_4;
    delete proxy_instance_5;
    delete proxy_cluster;
    delete proxy_read_only_cluster;
  }
};
