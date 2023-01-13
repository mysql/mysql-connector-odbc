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

#include "base_failover_integration_test.cc"

class NetworkFailoverIntegrationTest : public BaseFailoverIntegrationTest {
protected:
  std::string ACCESS_KEY = std::getenv("AWS_ACCESS_KEY_ID");
  std::string SECRET_ACCESS_KEY = std::getenv("AWS_SECRET_ACCESS_KEY");
  std::string SESSION_TOKEN = std::getenv("AWS_SESSION_TOKEN");
  Aws::Auth::AWSCredentials credentials = Aws::Auth::AWSCredentials(Aws::String(ACCESS_KEY),
                                                                    Aws::String(SECRET_ACCESS_KEY),
                                                                    Aws::String(SESSION_TOKEN));
  Aws::Client::ClientConfiguration client_config;
  Aws::RDS::RDSClient rds_client;
  SQLHENV env = nullptr;
  SQLHDBC dbc = nullptr;

  static void SetUpTestSuite() {
    Aws::InitAPI(options);
  }

  static void TearDownTestSuite() {
    Aws::ShutdownAPI(options);
  }

  void SetUp() override {
    SQLAllocHandle(SQL_HANDLE_ENV, nullptr, &env);
    SQLSetEnvAttr(env, SQL_ATTR_ODBC_VERSION, reinterpret_cast<SQLPOINTER>(SQL_OV_ODBC3), 0);
    SQLAllocHandle(SQL_HANDLE_DBC, env, &dbc);

    client_config.region = "us-east-2";
    rds_client = Aws::RDS::RDSClient(credentials, client_config);

    for (const auto& x : proxy_map) {
      enable_connectivity(x.second);
    }

    cluster_instances = retrieve_topology_via_SDK(rds_client, cluster_id);
    writer_id = get_writer_id(cluster_instances);
    writer_endpoint = get_proxied_endpoint(writer_id);
    readers = get_readers(cluster_instances);
    reader_id = get_first_reader_id(cluster_instances);
    reader_endpoint = get_proxied_endpoint(reader_id);

    builder = ConnectionStringBuilder();
    builder.withDSN(dsn)
        .withUID(user)
        .withPWD(pwd)
        .withConnectTimeout(10)
        .withNetworkTimeout(10);
    builder.withPort(MYSQL_PROXY_PORT)
        .withHostPattern(PROXIED_CLUSTER_TEMPLATE)
        .withLogQuery(true)
        .withEnableFailureDetection(true);
  }

  void TearDown() override {
    if (nullptr != dbc) {
      SQLFreeHandle(SQL_HANDLE_DBC, dbc);
    }
    if (nullptr != env) {
      SQLFreeHandle(SQL_HANDLE_ENV, env);
    }
  }
};

TEST_F(NetworkFailoverIntegrationTest, connection_test) {
  test_connection(dbc, MYSQL_INSTANCE_1_URL, MYSQL_PORT);
  test_connection(dbc, MYSQL_INSTANCE_1_URL + PROXIED_DOMAIN_NAME_SUFFIX, MYSQL_PROXY_PORT);
  test_connection(dbc, MYSQL_CLUSTER_URL, MYSQL_PORT);
  test_connection(dbc, MYSQL_CLUSTER_URL + PROXIED_DOMAIN_NAME_SUFFIX, MYSQL_PROXY_PORT);
  test_connection(dbc, MYSQL_RO_CLUSTER_URL, MYSQL_PORT);
  test_connection(dbc, MYSQL_RO_CLUSTER_URL + PROXIED_DOMAIN_NAME_SUFFIX, MYSQL_PROXY_PORT);
}

TEST_F(NetworkFailoverIntegrationTest, lost_connection_to_writer) {
  const std::string server = get_proxied_endpoint(writer_id);
  connection_string = builder.withServer(server).withFailoverTimeout(GLOBAL_FAILOVER_TIMEOUT).build();
  EXPECT_EQ(SQL_SUCCESS, SQLDriverConnect(dbc, nullptr, AS_SQLCHAR(connection_string.c_str()), SQL_NTS, conn_out, MAX_NAME_LEN, &len, SQL_DRIVER_NOPROMPT));

  assert_query_succeeded(dbc, SERVER_ID_QUERY);

  const auto writer_proxy = get_proxy_from_map(writer_id);
  if (writer_proxy) {
    disable_connectivity(writer_proxy);
  }
  disable_connectivity(proxy_cluster);

  assert_query_failed(dbc, SERVER_ID_QUERY, ERROR_COMM_LINK_FAILURE);

  enable_connectivity(writer_proxy);
  enable_connectivity(proxy_cluster);

  EXPECT_EQ(SQL_SUCCESS, SQLDisconnect(dbc));
}

TEST_F(NetworkFailoverIntegrationTest, use_same_connection_after_failing_failover) {
  const std::string server = get_proxied_endpoint(writer_id);
  connection_string = builder.withServer(server).withFailoverTimeout(GLOBAL_FAILOVER_TIMEOUT).build();
  EXPECT_EQ(SQL_SUCCESS, SQLDriverConnect(dbc, nullptr, AS_SQLCHAR(connection_string.c_str()), SQL_NTS, conn_out, MAX_NAME_LEN, &len, SQL_DRIVER_NOPROMPT));
  SQLHSTMT handle;
  EXPECT_EQ(SQL_SUCCESS, SQLAllocHandle(SQL_HANDLE_STMT, dbc, &handle));

  assert_query_succeeded(dbc, SERVER_ID_QUERY);

  const auto writer_proxy = get_proxy_from_map(writer_id);
  if (writer_proxy) {
    disable_connectivity(writer_proxy);
  }
  disable_connectivity(proxy_cluster);

  // failover fails
  assert_query_failed(dbc, SERVER_ID_QUERY, ERROR_COMM_LINK_FAILURE);

  enable_connectivity(writer_proxy);
  enable_connectivity(proxy_cluster);

  // Reuse same connection after failing failover
  assert_query_failed(dbc, SERVER_ID_QUERY, ERROR_COMM_LINK_FAILURE);
  
  EXPECT_EQ(SQL_SUCCESS, SQLDisconnect(dbc));
}

TEST_F(NetworkFailoverIntegrationTest, lost_connection_to_all_readers) {
  connection_string = builder.withServer(reader_endpoint).build();
  EXPECT_EQ(SQL_SUCCESS, SQLDriverConnect(dbc, nullptr, AS_SQLCHAR(connection_string.c_str()), SQL_NTS, conn_out, MAX_NAME_LEN, &len, SQL_DRIVER_NOPROMPT));

  for (const auto& x : proxy_map) {
    if (x.first != writer_id) {
      disable_connectivity(x.second);
    }
  }

  assert_query_failed(dbc, SERVER_ID_QUERY, ERROR_COMM_LINK_CHANGED);

  const std::string new_reader_id = query_instance_id(dbc);
  EXPECT_EQ(writer_id, new_reader_id);
  EXPECT_EQ(SQL_SUCCESS, SQLDisconnect(dbc));
}

TEST_F(NetworkFailoverIntegrationTest, lost_connection_to_reader_instance) {
  connection_string = builder.withServer(reader_endpoint).build();
  EXPECT_EQ(SQL_SUCCESS, SQLDriverConnect(dbc, nullptr, AS_SQLCHAR(connection_string.c_str()), SQL_NTS, conn_out, MAX_NAME_LEN, &len, SQL_DRIVER_NOPROMPT));

  disable_instance(reader_id);
  assert_query_failed(dbc, SERVER_ID_QUERY, ERROR_COMM_LINK_CHANGED);

  const std::string new_instance = query_instance_id(dbc);
  EXPECT_EQ(writer_id, new_instance);
  EXPECT_EQ(SQL_SUCCESS, SQLDisconnect(dbc));
}

TEST_F(NetworkFailoverIntegrationTest, lost_connection_read_only) {
  connection_string = builder.withServer(reader_endpoint).withAllowReaderConnections(true).build();
  EXPECT_EQ(SQL_SUCCESS, SQLDriverConnect(dbc, nullptr, AS_SQLCHAR(connection_string.c_str()), SQL_NTS, conn_out, MAX_NAME_LEN, &len, SQL_DRIVER_NOPROMPT));

  disable_instance(reader_id);
  assert_query_failed(dbc, SERVER_ID_QUERY, ERROR_COMM_LINK_CHANGED);

  const std::string new_reader_id = query_instance_id(dbc);
  EXPECT_NE(writer_id, new_reader_id);
  EXPECT_EQ(SQL_SUCCESS, SQLDisconnect(dbc));
}

TEST_F(NetworkFailoverIntegrationTest, writer_connection_fails_due_to_no_reader) {
  const char* writer_char_id = writer_id.c_str();
  const std::string server = MYSQL_INSTANCE_1_URL + PROXIED_DOMAIN_NAME_SUFFIX;

  connection_string = builder.withServer(server).withFailoverTimeout(GLOBAL_FAILOVER_TIMEOUT).build();
  EXPECT_EQ(SQL_SUCCESS, SQLDriverConnect(dbc, nullptr, AS_SQLCHAR(connection_string.c_str()), SQL_NTS, conn_out, MAX_NAME_LEN, &len, SQL_DRIVER_NOPROMPT));

  // Put all but writer down first
  for (const auto& x : proxy_map) {
    if (x.first != writer_char_id) {
      disable_connectivity(x.second);
    }
  }

  // Crash the writer now
  const auto writer_proxy = get_proxy_from_map(writer_id);
  if (writer_proxy) {
    disable_connectivity(writer_proxy);
  }

  assert_query_failed(dbc, SERVER_ID_QUERY, ERROR_COMM_LINK_FAILURE);
  EXPECT_EQ(SQL_SUCCESS, SQLDisconnect(dbc));
}

TEST_F(NetworkFailoverIntegrationTest, fail_from_reader_to_reader_with_some_readers_are_down) {
  // Assert there are at least 2 readers in the cluster.
  EXPECT_LE(2, readers.size());

  connection_string = builder.withServer(reader_endpoint).withFailoverTimeout(GLOBAL_FAILOVER_TIMEOUT).withAllowReaderConnections(true).build();
  EXPECT_EQ(SQL_SUCCESS, SQLDriverConnect(dbc, nullptr, AS_SQLCHAR(connection_string.c_str()), SQL_NTS, conn_out, MAX_NAME_LEN, &len, SQL_DRIVER_NOPROMPT));

  for (size_t index = 0; index < readers.size() - 1; ++index) {
    disable_instance(readers[index]);
  }

  assert_query_failed(dbc, SERVER_ID_QUERY, ERROR_COMM_LINK_CHANGED);

  const std::string current_connection = query_instance_id(dbc);
  const std::string last_reader = readers.back();

  // Assert that new instance is either the last reader instance or the writer instance.
  EXPECT_TRUE(current_connection == last_reader || current_connection == writer_id);
  EXPECT_EQ(SQL_SUCCESS, SQLDisconnect(dbc));
}

TEST_F(NetworkFailoverIntegrationTest, failover_back_to_the_previously_down_reader) {
  // Assert there are at least 4 readers in the cluster.
  EXPECT_LE(4, readers.size());

  std::vector<std::string> previous_readers;

  const std::string first_reader = reader_id;
  const std::string server = get_proxied_endpoint(first_reader);
  previous_readers.push_back(first_reader);

  connection_string = builder.withServer(server).withFailoverTimeout(GLOBAL_FAILOVER_TIMEOUT).withAllowReaderConnections(true).build();
  EXPECT_EQ(SQL_SUCCESS, SQLDriverConnect(dbc, nullptr, AS_SQLCHAR(connection_string.c_str()), SQL_NTS, conn_out, MAX_NAME_LEN, &len, SQL_DRIVER_NOPROMPT));

  disable_instance(first_reader);
  assert_query_failed(dbc, SERVER_ID_QUERY, ERROR_COMM_LINK_CHANGED);

  const std::string second_reader = query_instance_id(dbc);
  EXPECT_TRUE(is_db_instance_reader(second_reader));
  assert_is_new_reader(previous_readers, second_reader);
  previous_readers.push_back(second_reader);

  disable_instance(second_reader);
  assert_query_failed(dbc, SERVER_ID_QUERY, ERROR_COMM_LINK_CHANGED);

  const std::string third_reader = query_instance_id(dbc);
  EXPECT_TRUE(is_db_instance_reader(third_reader));
  assert_is_new_reader(previous_readers, third_reader);
  previous_readers.push_back(third_reader);

  // Find the fourth reader instance
  std::string last_reader;
  for (const auto& reader : readers) {
    const std::string reader_id = reader;
    bool is_same = false;

    for (const auto& used_reader : previous_readers) {
      if (used_reader == reader_id) {
        is_same = true;
        break;
      }
    }

    if (is_same) {
      continue;
    }

    last_reader = reader_id;
  }

  assert_is_new_reader(previous_readers, last_reader);

  // Crash the fourth reader instance.
  disable_instance(last_reader);

  // Stop crashing the first and second.
  enable_instance(previous_readers[0]);
  enable_instance(previous_readers[1]);

  const std::string current_instance_id = query_instance_id(dbc);
  EXPECT_EQ(third_reader, current_instance_id);

  // Start crashing the third instance.
  disable_instance(third_reader);

  assert_query_failed(dbc, SERVER_ID_QUERY, ERROR_COMM_LINK_CHANGED);

  const std::string last_instance_id = query_instance_id(dbc);

  // Assert that the last instance is either the first reader instance or the second reader instance.
  EXPECT_TRUE(last_instance_id == first_reader || last_instance_id == second_reader);
  EXPECT_EQ(SQL_SUCCESS, SQLDisconnect(dbc));
}
