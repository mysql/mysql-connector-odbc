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

#include <stdexcept>
#include <string>
#include <memory>

class ConnectionStringBuilder;

class ConnectionString {
  public:
    // friend class so the builder can access ConnectionString private attributes
    friend class ConnectionStringBuilder;

    ConnectionString() : m_dsn(""), m_server(""), m_port(-1),
                         m_uid(""), m_pwd(""), m_db(""), m_log_query(true), m_allow_reader_connections(false),
                         m_multi_statements(false), m_enable_cluster_failover(true), m_failover_timeout(-1),
                         m_connect_timeout(-1), m_network_timeout(-1), m_host_pattern(""),
                         m_enable_failure_detection(true), m_failure_detection_time(-1), m_failure_detection_timeout(-1),
                         m_failure_detection_interval(-1), m_failure_detection_count(-1), m_monitor_disposal_time(-1),
                         m_read_timeout(-1), m_write_timeout(-1), 
                         
                         is_set_uid(false), is_set_pwd(false), is_set_db(false), is_set_log_query(false),
                         is_set_allow_reader_connections(false), is_set_multi_statements(false), is_set_enable_cluster_failover(false),
                         is_set_failover_timeout(false), is_set_connect_timeout(false), is_set_network_timeout(false), is_set_host_pattern(false),
                         is_set_enable_failure_detection(false), is_set_failure_detection_time(false), is_set_failure_detection_timeout(false),
                         is_set_failure_detection_interval(false), is_set_failure_detection_count(false), is_set_monitor_disposal_time(false),
                         is_set_read_timeout(false), is_set_write_timeout(false) {};

    std::string get_connection_string() const {
      char conn_in[4096] = "\0";
      int length = 0;
      length += sprintf(conn_in, "DSN=%s;SERVER=%s;PORT=%d;", m_dsn.c_str(), m_server.c_str(), m_port);

      if (is_set_uid) {
        length += sprintf(conn_in + length, "UID=%s;", m_uid.c_str()); 
      }
      if (is_set_pwd) {
        length += sprintf(conn_in + length, "PWD=%s;", m_pwd.c_str()); 
      }
      if (is_set_db) {
        length += sprintf(conn_in + length, "DATABASE=%s;", m_db.c_str()); 
      }
      if (is_set_log_query) {
        length += sprintf(conn_in + length, "LOG_QUERY=%d;", m_log_query ? 1 : 0); 
      }
      if (is_set_allow_reader_connections) {
        length += sprintf(conn_in + length, "ALLOW_READER_CONNECTIONS=%d;", m_allow_reader_connections ? 1 : 0); 
      }
      if (is_set_multi_statements) {
        length += sprintf(conn_in + length, "MULTI_STATEMENTS=%d;", m_multi_statements ? 1 : 0);
      }
      if (is_set_enable_cluster_failover) {
        length += sprintf(conn_in + length, "ENABLE_CLUSTER_FAILOVER=%d;", m_enable_cluster_failover ? 1 : 0);
      }
      if (is_set_failover_timeout) {
        length += sprintf(conn_in + length, "FAILOVER_TIMEOUT=%d;", m_failover_timeout); 
      }
      if (is_set_connect_timeout) {
        length += sprintf(conn_in + length, "CONNECT_TIMEOUT=%d;", m_connect_timeout); 
      }
      if (is_set_network_timeout) {
        length += sprintf(conn_in + length, "NETWORK_TIMEOUT=%d;", m_network_timeout); 
      }
      if (is_set_host_pattern) {
        length += sprintf(conn_in + length, "HOST_PATTERN=%s;", m_host_pattern.c_str()); 
      }
      if (is_set_enable_failure_detection) {
        length += sprintf(conn_in + length, "ENABLE_FAILURE_DETECTION=%d;", m_enable_failure_detection ? 1 : 0); 
      }
      if (is_set_failure_detection_time) {
        length += sprintf(conn_in + length, "FAILURE_DETECTION_TIME=%d;", m_failure_detection_time);
      }
      if (is_set_failure_detection_timeout) {
        length += sprintf(conn_in + length, "FAILURE_DETECTION_TIMEOUT=%d;", m_failure_detection_timeout);
      }
      if (is_set_failure_detection_interval) {
        length += sprintf(conn_in + length, "FAILURE_DETECTION_INTERVAL=%d;", m_failure_detection_interval);
      }
      if (is_set_failure_detection_count) {
        length += sprintf(conn_in + length, "FAILURE_DETECTION_COUNT=%d;", m_failure_detection_count);
      }
      if (is_set_monitor_disposal_time) {
        length += sprintf(conn_in + length, "MONITOR_DISPOSAL_TIME=%d;", m_monitor_disposal_time);
      }
      if (is_set_read_timeout) {
        length += sprintf(conn_in + length, "READTIMEOUT=%d;", m_read_timeout);
      }
      if (is_set_write_timeout) {
        length += sprintf(conn_in + length, "WRITETIMEOUT=%d;", m_write_timeout);
      }
      snprintf(conn_in + length, sizeof(conn_in) - length, "\0");

      std::string connection_string(conn_in);
      return connection_string;
    }
    
  private:
    // Required fields
    std::string m_dsn, m_server;
    int m_port;

    // Optional fields
    std::string m_uid, m_pwd, m_db;
    bool m_log_query, m_allow_reader_connections, m_multi_statements, m_enable_cluster_failover;
    int m_failover_timeout, m_connect_timeout, m_network_timeout;
    std::string m_host_pattern;
    bool m_enable_failure_detection;
    int m_failure_detection_time, m_failure_detection_timeout, m_failure_detection_interval, m_failure_detection_count, m_monitor_disposal_time, m_read_timeout, m_write_timeout;

    bool is_set_uid, is_set_pwd, is_set_db;
    bool is_set_log_query, is_set_allow_reader_connections, is_set_multi_statements;
    bool is_set_enable_cluster_failover;
    bool is_set_failover_timeout, is_set_connect_timeout, is_set_network_timeout;
    bool is_set_host_pattern;
    bool is_set_enable_failure_detection;
    bool is_set_failure_detection_time, is_set_failure_detection_timeout, is_set_failure_detection_interval, is_set_failure_detection_count;
    bool is_set_monitor_disposal_time;
    bool is_set_read_timeout, is_set_write_timeout;

    void set_dsn(const std::string& dsn) {
      m_dsn = dsn;
    }

    void set_server(const std::string& server) {
      m_server = server;
    }

    void set_port(const int& port) {
      m_port = port;
    }

    void set_uid(const std::string& uid) {
      m_uid = uid;
      is_set_uid = true;
    }

    void set_pwd(const std::string& pwd) {
      m_pwd = pwd;
      is_set_pwd = true;
    }

    void set_db(const std::string& db) {
      m_db = db;
      is_set_db = true;
    }

    void set_log_query(const bool& log_query) {
      m_log_query = log_query;
      is_set_log_query = true;
    }

    void set_allow_reader_connections(const bool& allow_reader_connections) {
      m_allow_reader_connections = allow_reader_connections;
      is_set_allow_reader_connections = true;
    }

    void set_multi_statements(const bool& multi_statements) {
      m_multi_statements = multi_statements;
      is_set_multi_statements = true;
    }

    void set_enable_cluster_failover(const bool& enable_cluster_failover) {
      m_enable_cluster_failover = enable_cluster_failover;
      is_set_enable_cluster_failover = true;
    }

    void set_failover_timeout(const int& failover_timeout) {
      m_failover_timeout = failover_timeout;
      is_set_failover_timeout = true;
    }

    void set_connect_timeout(const int& connect_timeout) {
      m_connect_timeout = connect_timeout;
      is_set_connect_timeout = true;
    }

    void set_network_timeout(const int& network_timeout) {
      m_network_timeout = network_timeout;
      is_set_network_timeout = true;
    }

    void set_host_pattern(const std::string& host_pattern) {
      m_host_pattern = host_pattern;
      is_set_host_pattern = true;
    }

    void set_enable_failure_detection(const bool& enable_failure_detection) {
      m_enable_failure_detection = enable_failure_detection;
      is_set_enable_failure_detection = true;
    }

    void set_failure_detection_time(const int& failure_detection_time) {
      m_failure_detection_time = failure_detection_time;
      is_set_failure_detection_time = true;
    }

    void set_failure_detection_timeout(const int& failure_detection_timeout) {
      m_failure_detection_timeout = failure_detection_timeout;
      is_set_failure_detection_timeout = true;
    }

    void set_failure_detection_interval(const int& failure_detection_interval) {
      m_failure_detection_interval = failure_detection_interval;
      is_set_failure_detection_interval = true;
    }

    void set_failure_detection_count(const int& failure_detection_count) {
      m_failure_detection_count = failure_detection_count;
      is_set_failure_detection_count = true;
    }

    void set_monitor_disposal_time(const int& monitor_disposal_time) {
      m_monitor_disposal_time = monitor_disposal_time;
      is_set_monitor_disposal_time = true;
    }

    void set_read_timeout(const int& read_timeout) {
      m_read_timeout = read_timeout;
      is_set_read_timeout = true;
    }

    void set_write_timeout(const int& write_timeout) {
      m_write_timeout = write_timeout;
      is_set_write_timeout = true;
    }
};

class ConnectionStringBuilder {
  public:
    ConnectionStringBuilder() {
      connection_string.reset(new ConnectionString());
    }
    
    ConnectionStringBuilder& withDSN(const std::string& dsn) {
      connection_string->set_dsn(dsn);
      return *this;
    }

    ConnectionStringBuilder& withServer(const std::string& server) {
      connection_string->set_server(server);
      return *this;
    }

    ConnectionStringBuilder& withPort(const int& port) {
      connection_string->set_port(port);
      return *this;
    }
    
    ConnectionStringBuilder& withUID(const std::string& uid) {
      connection_string->set_uid(uid);
      return *this;
    }

    ConnectionStringBuilder& withPWD(const std::string& pwd) {
      connection_string->set_pwd(pwd);
      return *this;
    }

    ConnectionStringBuilder& withDatabase(const std::string& db) {
      connection_string->set_db(db);
      return *this;
    }

    ConnectionStringBuilder& withLogQuery(const bool& log_query) {
      connection_string->set_log_query(log_query);
      return *this;
    }

    ConnectionStringBuilder& withAllowReaderConnections(const bool& allow_reader_connections) {
      connection_string->set_allow_reader_connections(allow_reader_connections);
      return *this;
    }

    ConnectionStringBuilder& withMultiStatements(const bool& multi_statements) {
      connection_string->set_multi_statements(multi_statements);
      return *this;
    }

    ConnectionStringBuilder& withEnableClusterFailover(const bool& enable_cluster_failover) {
      connection_string->set_enable_cluster_failover(enable_cluster_failover);
      return *this;
    }

    ConnectionStringBuilder& withFailoverTimeout(const int& failover_t) {
      connection_string->set_failover_timeout(failover_t);
      return *this;
    }

    ConnectionStringBuilder& withConnectTimeout(const int& connect_timeout) {
      connection_string->set_connect_timeout(connect_timeout);
      return *this;
    }

    ConnectionStringBuilder& withNetworkTimeout(const int& network_timeout) {
      connection_string->set_network_timeout(network_timeout);
      return *this;
    }

    ConnectionStringBuilder& withHostPattern(const std::string& host_pattern) {
      connection_string->set_host_pattern(host_pattern);
      return *this;
    }

    ConnectionStringBuilder& withEnableFailureDetection(const bool& enable_failure_detection) {
      connection_string->set_enable_failure_detection(enable_failure_detection);
      return *this;
    }

    ConnectionStringBuilder& withFailureDetectionTime(const int& failure_detection_time) {
      connection_string->set_failure_detection_time(failure_detection_time);
      return *this;
    }

    ConnectionStringBuilder& withFailureDetectionTimeout(const int& failure_detection_timeout) {
      connection_string->set_failure_detection_timeout(failure_detection_timeout);
      return *this;
    }

    ConnectionStringBuilder& withFailureDetectionInterval(const int& failure_detection_interval) {
      connection_string->set_failure_detection_interval(failure_detection_interval);
      return *this;
    }

    ConnectionStringBuilder& withFailureDetectionCount(const int& failure_detection_count) {
      connection_string->set_failure_detection_count(failure_detection_count);
      return *this;
    }

    ConnectionStringBuilder& withMonitorDisposalTime(const int& monitor_disposal_time) {
      connection_string->set_monitor_disposal_time(monitor_disposal_time);
      return *this;
    }

    ConnectionStringBuilder& withReadTimeout(const int& read_timeout) {
      connection_string->set_read_timeout(read_timeout);
      return *this;
    }
    
    ConnectionStringBuilder& withWriteTimeout(const int& write_timeout) {
      connection_string->set_write_timeout(write_timeout);
      return *this;
    }

    std::string build() const {
      if (connection_string->m_dsn.empty()) {
        throw std::runtime_error("DSN is a required field in a connection string.");
      }
      if (connection_string->m_server.empty()) {
        throw std::runtime_error("Server is a required field in a connection string.");
      }
      if (connection_string->m_port < 1) {
        throw std::runtime_error("Port is a required field in a connection string.");
      }
      return connection_string->get_connection_string();
    }
    
  private:
    std::unique_ptr<ConnectionString> connection_string;
};
