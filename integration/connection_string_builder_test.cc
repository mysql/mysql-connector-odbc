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

#include "connection_string_builder.cc"

class ConnectionStringBuilderTest : public testing::Test {
};

// No fields are set in the builder. Error expected.
TEST_F(ConnectionStringBuilderTest, test_empty_builder) {
  ConnectionStringBuilder builder = ConnectionStringBuilder();
  EXPECT_THROW(builder.build(), std::runtime_error);
}

// More than one required field is not set in the builder. Error expected.
TEST_F(ConnectionStringBuilderTest, test_missing_fields) {
  ConnectionStringBuilder builder = ConnectionStringBuilder();
  EXPECT_THROW(builder.withServer("testServer").build(), std::runtime_error);
}

// Required field DSN is not set in the builder. Error expected.
TEST_F(ConnectionStringBuilderTest, test_missing_dsn) {
  ConnectionStringBuilder builder = ConnectionStringBuilder();
  EXPECT_THROW(builder.withServer("testServer").withPort(3306).build(), std::runtime_error);
}

// Required field Server is not set in the builder. Error expected.
TEST_F(ConnectionStringBuilderTest, test_missing_server) {
  ConnectionStringBuilder builder = ConnectionStringBuilder();
  EXPECT_THROW(builder.withDSN("testDSN").withPort(3306).build(), std::runtime_error);
}

// Required field Port is not set in the builder. Error expected.
TEST_F(ConnectionStringBuilderTest, test_missing_port) {
  ConnectionStringBuilder builder = ConnectionStringBuilder();
  EXPECT_THROW(builder.withDSN("testDSN").withServer("testServer").build(), std::runtime_error);
}

// All connection string fields are set in the builder.
TEST_F(ConnectionStringBuilderTest, test_complete_string) {
  ConnectionStringBuilder builder = ConnectionStringBuilder();
  const std::string connection_string = builder.withServer("testServer")
                                   .withUID("testUser")
                                   .withPWD("testPwd")
                                   .withLogQuery(false)
                                   .withAllowReaderConnections(true)
                                   .withMultiStatements(false)
                                   .withDSN("testDSN")
                                   .withFailoverTimeout(120000)
                                   .withPort(3306)
                                   .withDatabase("testDb")
                                   .withConnectTimeout(20)
                                   .withNetworkTimeout(20)
                                   .withHostPattern("?.testDomain")
                                   .withEnableFailureDetection(true)
                                   .withFailureDetectionTime(10000)
                                   .withFailureDetectionInterval(100)
                                   .withFailureDetectionCount(4)
                                   .withMonitorDisposalTime(300)
                                   .withEnableClusterFailover(true)
                                   .build();

  const std::string expected = "DSN=testDSN;SERVER=testServer;PORT=3306;UID=testUser;PWD=testPwd;DATABASE=testDb;LOG_QUERY=0;ALLOW_READER_CONNECTIONS=1;MULTI_STATEMENTS=0;ENABLE_CLUSTER_FAILOVER=1;FAILOVER_TIMEOUT=120000;CONNECT_TIMEOUT=20;NETWORK_TIMEOUT=20;HOST_PATTERN=?.testDomain;ENABLE_FAILURE_DETECTION=1;FAILURE_DETECTION_TIME=10000;FAILURE_DETECTION_INTERVAL=100;FAILURE_DETECTION_COUNT=4;MONITOR_DISPOSAL_TIME=300;";
  EXPECT_EQ(0, expected.compare(connection_string));
}

// No optional fields are set in the builder. Build will succeed. Connection string with required fields.
TEST_F(ConnectionStringBuilderTest, test_only_required_fields) {
  ConnectionStringBuilder builder = ConnectionStringBuilder();
  const std::string connection_string = builder.withDSN("testDSN")
                                   .withServer("testServer")
                                   .withPort(3306)
                                   .build();

  const std::string expected = "DSN=testDSN;SERVER=testServer;PORT=3306;";
  EXPECT_EQ(0, expected.compare(connection_string));
}

// Some optional fields are set and others not set in the builder. Build will succeed.
// Connection string with required fields and ONLY the fields that were set.
TEST_F(ConnectionStringBuilderTest, test_some_optional) {
  ConnectionStringBuilder builder = ConnectionStringBuilder();
  const std::string connection_string = builder.withDSN("testDSN")
                                   .withServer("testServer")
                                   .withPort(3306)
                                   .withUID("testUser")
                                   .withPWD("testPwd")
                                   .build();

  const std::string expected("DSN=testDSN;SERVER=testServer;PORT=3306;UID=testUser;PWD=testPwd;");
  EXPECT_EQ(0, expected.compare(connection_string));
}

// Boolean values are set in the builder. Build will succeed. True will be marked as 1 in the string, false 0.
TEST_F(ConnectionStringBuilderTest, test_setting_boolean_fields) {
  ConnectionStringBuilder builder = ConnectionStringBuilder();
  const std::string connection_string = builder.withDSN("testDSN")
                                   .withServer("testServer")
                                   .withPort(3306)
                                   .withUID("testUser")
                                   .withPWD("testPwd")
                                   .withLogQuery(false)
                                   .withAllowReaderConnections(true)
                                   .withMultiStatements(true)
                                   .withEnableClusterFailover(false)
                                   .withEnableFailureDetection(true)
                                   .build();

  const std::string expected("DSN=testDSN;SERVER=testServer;PORT=3306;UID=testUser;PWD=testPwd;LOG_QUERY=0;ALLOW_READER_CONNECTIONS=1;MULTI_STATEMENTS=1;ENABLE_CLUSTER_FAILOVER=0;ENABLE_FAILURE_DETECTION=1;");
  EXPECT_EQ(0, expected.compare(connection_string));
}

// Create a builder with required values. Then append other properties to the builder. Then build the connection string. Build will succeed.
TEST_F(ConnectionStringBuilderTest, test_setting_multiple_steps_1) {
  ConnectionStringBuilder builder = ConnectionStringBuilder();
  builder.withDSN("testDSN").withServer("testServer").withPort(3306);

  builder.withUID("testUser").withPWD("testPwd").withLogQuery(true);
  const std::string connection_string = builder.build();

  const std::string expected("DSN=testDSN;SERVER=testServer;PORT=3306;UID=testUser;PWD=testPwd;LOG_QUERY=1;");
  EXPECT_EQ(0, expected.compare(connection_string));
}

// Create a builder initially without all required values. Then append other properties to the builder.
// After second round of values, all required values are set. Build the connection string. Build will succeed.
TEST_F(ConnectionStringBuilderTest, test_setting_multiple_steps_2) {
  ConnectionStringBuilder builder = ConnectionStringBuilder();
  builder.withDSN("testDSN").withServer("testServer").withUID("testUser").withPWD("testPwd");

  builder.withPort(3306).withLogQuery(true);
  const std::string connection_string = builder.build();

  const std::string expected("DSN=testDSN;SERVER=testServer;PORT=3306;UID=testUser;PWD=testPwd;LOG_QUERY=1;");
  EXPECT_EQ(0, expected.compare(connection_string));
}

// Create a builder with some values. Then append more values to the builder, but leaving required values unset.
// Then build the connection string. Error expected.
TEST_F(ConnectionStringBuilderTest, test_multiple_steps_without_required) {
  ConnectionStringBuilder builder = ConnectionStringBuilder();
  builder.withServer("testServer").withPort(3306);
  EXPECT_THROW(builder.withUID("testUser").withPWD("testPwd").withLogQuery(true).build(), std::runtime_error);
}
