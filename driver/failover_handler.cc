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

/**
  @file  failover_handler.c
  @brief Failover functions.
*/

#include <regex>
#include <sstream>

#include "driver.h"

namespace {
const std::regex AURORA_DNS_PATTERN(
    R"#((.+)\.(proxy-|cluster-|cluster-ro-|cluster-custom-)?([a-zA-Z0-9]+\.[a-zA-Z0-9\-]+\.rds\.amazonaws\.com))#",
    std::regex_constants::icase);
const std::regex AURORA_PROXY_DNS_PATTERN(
    R"#((.+)\.(proxy-[a-zA-Z0-9]+\.[a-zA-Z0-9\-]+\.rds\.amazonaws\.com))#",
    std::regex_constants::icase);
const std::regex AURORA_CUSTOM_CLUSTER_PATTERN(
    R"#((.+)\.(cluster-custom-[a-zA-Z0-9]+\.[a-zA-Z0-9\-]+\.rds\.amazonaws\.com))#",
    std::regex_constants::icase);
const std::regex IPV4_PATTERN(
    R"#(^(([1-9]|[1-9][0-9]|1[0-9]{2}|2[0-4][0-9]|25[0-5])\.){1}(([0-9]|[1-9][0-9]|1[0-9]{2}|2[0-4][0-9]|25[0-5])\.){2}([0-9]|[1-9][0-9]|1[0-9]{2}|2[0-4][0-9]|25[0-5])$)#");
const std::regex IPV6_PATTERN(R"#(^[0-9a-fA-F]{1,4}(:[0-9a-fA-F]{1,4}){7}$)#");
const std::regex IPV6_COMPRESSED_PATTERN(
    R"#(^(([0-9A-Fa-f]{1,4}(:[0-9A-Fa-f]{1,4}){0,5})?)::(([0-9A-Fa-f]{1,4}(:[0-9A-Fa-f]{1,4}){0,5})?)$)#");
}  // namespace

FAILOVER_HANDLER::FAILOVER_HANDLER(DBC* dbc, DataSource* ds)
    : FAILOVER_HANDLER(
          dbc, ds, std::make_shared<FAILOVER_CONNECTION_HANDLER>(dbc),
          std::make_shared<TOPOLOGY_SERVICE>(dbc ? dbc->id : 0, ds ? ds->save_queries : false),
          std::make_shared<CLUSTER_AWARE_METRICS_CONTAINER>(dbc, ds)) {}

FAILOVER_HANDLER::FAILOVER_HANDLER(DBC* dbc, DataSource* ds,
                                   std::shared_ptr<FAILOVER_CONNECTION_HANDLER> connection_handler,
                                   std::shared_ptr<TOPOLOGY_SERVICE> topology_service,
                                   std::shared_ptr<CLUSTER_AWARE_METRICS_CONTAINER> metrics_container) {
    if (!dbc) {
        throw std::runtime_error("DBC cannot be null.");
    }

    if (!ds) {
        throw std::runtime_error("DataSource cannot be null.");
    }

    this->dbc = dbc;
    this->ds = ds;
    this->topology_service = topology_service;
    this->topology_service->set_refresh_rate(ds->topology_refresh_rate);
    this->topology_service->set_gather_metric(ds->gather_perf_metrics);
    this->connection_handler = connection_handler;

    this->failover_reader_handler = std::make_shared<FAILOVER_READER_HANDLER>(
        this->topology_service, this->connection_handler, ds->failover_timeout,
        ds->failover_reader_connect_timeout, dbc->id, ds->save_queries);
    this->failover_writer_handler = std::make_shared<FAILOVER_WRITER_HANDLER>(
        this->topology_service, this->failover_reader_handler,
        this->connection_handler, ds->failover_timeout,
        ds->failover_topology_refresh_rate,
        ds->failover_writer_reconnect_interval, dbc->id, ds->save_queries);
    this->metrics_container = metrics_container;
}

FAILOVER_HANDLER::~FAILOVER_HANDLER() {}

SQLRETURN FAILOVER_HANDLER::init_cluster_info() {
    SQLRETURN rc = SQL_ERROR;
    if (initialized) {
        return rc;
    }
    
    if (!ds->enable_cluster_failover) {
        // Use a standard default connection - no further initialization required
        rc = connection_handler->do_connect(dbc, ds, false);
        initialized = true;
        return rc;
    }
    std::stringstream err;
    // Cluster-aware failover is enabled

    this->current_host = get_host_info_from_ds(ds);
    std::string main_host = this->current_host->get_host();
    unsigned int main_port = this->current_host->get_port();

    const char* hp =
        ds_get_utf8attr(ds->host_pattern, &ds->host_pattern8);
    std::string hp_str(hp ? hp : "");

    const char* clid =
        ds_get_utf8attr(ds->cluster_id, &ds->cluster_id8);
    std::string clid_str(clid ? clid : "");

    if (!hp_str.empty()) {
        unsigned int port = ds->port ? ds->port : MYSQL_PORT;
        std::vector<Srv_host_detail> host_patterns;

        try {
            host_patterns = parse_host_list(hp_str.c_str(), port);
        } catch (std::string&) {
            err << "Invalid host pattern: '" << hp_str << "' - the value could not be parsed";
            MYLOG_TRACE(dbc->log_file.get(), dbc->id, err.str().c_str());
            throw std::runtime_error(err.str());
        }

        if (host_patterns.size() == 0) {
            err << "Empty host pattern.";
            MYLOG_DBC_TRACE(dbc, err.str().c_str());
            throw std::runtime_error(err.str());
        }

        std::string host_pattern(host_patterns[0].name);
        unsigned int host_pattern_port = host_patterns[0].port;

        if (!is_dns_pattern_valid(host_pattern)) {
            err << "Invalid host pattern: '" << host_pattern
                << "' - the host pattern must contain a '?' character as a "
                    "placeholder for the DB instance identifiers  of the cluster "
                    "instances";
            MYLOG_DBC_TRACE(dbc, err.str().c_str());
            throw std::runtime_error(err.str());
        }

        auto host_template = std::make_shared<HOST_INFO>(host_pattern, host_pattern_port);
        topology_service->set_cluster_instance_template(host_template);

        m_is_rds = is_rds_dns(host_pattern);
        MYLOG_DBC_TRACE(dbc, "[FAILOVER_HANDLER] m_is_rds=%s", m_is_rds ? "true" : "false");
        m_is_rds_proxy = is_rds_proxy_dns(host_pattern);
        MYLOG_DBC_TRACE(dbc, "[FAILOVER_HANDLER] m_is_rds_proxy=%s", m_is_rds_proxy ? "true" : "false");
        m_is_rds_custom_cluster = is_rds_custom_cluster_dns(host_pattern);

        if (m_is_rds_proxy) {
            err << "RDS Proxy url can't be used as an instance pattern.";
            MYLOG_DBC_TRACE(dbc, err.str().c_str());
            throw std::runtime_error(err.str());
        }

        if (m_is_rds_custom_cluster) {
            err << "RDS Custom Cluster endpoint can't be used as an instance pattern.";
            MYLOG_DBC_TRACE(dbc, err.str().c_str());
            throw std::runtime_error(err.str());
        }

        if (!clid_str.empty()) {
            set_cluster_id(clid_str);

        } else if (m_is_rds) {
            // If it's a cluster endpoint, or a reader cluster endpoint, then
            // let's use as cluster identification
            std::string cluster_rds_host =
                get_rds_cluster_host_url(host_pattern);
            if (!cluster_rds_host.empty()) {
                set_cluster_id(cluster_rds_host, host_pattern_port);
            }
        }

        rc = create_connection_and_initialize_topology();
    } else if (is_ipv4(main_host) || is_ipv6(main_host)) {
        // TODO: do we need to setup host template in this case?
        // HOST_INFO* host_template = new HOST_INFO();
        // host_template->host.assign(main_host);
        // host_template->port = main_port;
        // ts->setClusterInstanceTemplate(host_template);

        if (!clid_str.empty()) {
            set_cluster_id(clid_str);
        }

        rc = create_connection_and_initialize_topology();

        if (m_is_cluster_topology_available) {
            err << "Host Pattern configuration setting is required when IP "
                    "address is used to connect to a cluster that provides topology "
                    "information. If you would instead like to connect without "
                    "failover functionality, set the 'Disable Cluster Failover' "
                    "configuration property to true.";
            MYLOG_DBC_TRACE(dbc, err.str().c_str());
            throw std::runtime_error(err.str());
        }

        m_is_rds = false;        // actually we don't know
        m_is_rds_proxy = false;  // actually we don't know

    } else {
        m_is_rds = is_rds_dns(main_host);
        MYLOG_DBC_TRACE(dbc, "[FAILOVER_HANDLER] m_is_rds=%s", m_is_rds ? "true" : "false");
        m_is_rds_proxy = is_rds_proxy_dns(main_host);
        MYLOG_DBC_TRACE(dbc, "[FAILOVER_HANDLER] m_is_rds_proxy=%s", m_is_rds_proxy ? "true" : "false");

        if (!m_is_rds) {
            // it's not RDS, maybe custom domain (CNAME)
            auto host_template =
                std::make_shared<HOST_INFO>(main_host, main_port);
            topology_service->set_cluster_instance_template(host_template);

            if (!clid_str.empty()) {
                set_cluster_id(clid_str);
            }

            rc = create_connection_and_initialize_topology();

            if (m_is_cluster_topology_available) {
              err << "The provided host appears to be a custom domain. The "
                     "driver requires the Host Pattern configuration setting "
                     "to be set for custom domains. If you would instead like "
                     "to connect without  failover functionality, set the "
                     "'Disable Cluster Failover' configuration property to true.";
                MYLOG_DBC_TRACE(dbc, err.str().c_str());
                throw std::runtime_error(err.str());
            }
        } else {
            // It's RDS

            std::string rds_instance_host = get_rds_instance_host_pattern(main_host);
            if (!rds_instance_host.empty()) {
                topology_service->set_cluster_instance_template(
                    std::make_shared<HOST_INFO>(rds_instance_host, main_port));
            } else {
                err << "The provided host does not appear to match an expected "
                       "Aurora DNS pattern. Please set the Host Pattern "
                       "configuration to specify the host pattern for the "
                       "cluster you are trying to connect to.";
                MYLOG_DBC_TRACE(dbc, err.str().c_str());
                throw std::runtime_error(err.str());
            }

            if (!clid_str.empty()) {
                set_cluster_id(clid_str);
            } else if (m_is_rds_proxy) {
                // Each proxy is associated with a single cluster so it's safe
                // to use RDS Proxy Url as cluster identification
                set_cluster_id(main_host, main_port);
            } else {
                // If it's cluster endpoint or reader cluster endpoint,
                // then let's use as cluster identification

                std::string cluster_rds_host = get_rds_cluster_host_url(main_host);
                if (!cluster_rds_host.empty()) {
                    set_cluster_id(cluster_rds_host, main_port);
                } else {
                    // Main host is an instance endpoint
                    set_cluster_id(main_host, main_port);
                }
            }

            rc = create_connection_and_initialize_topology();
        }
    }

    initialized = true;
    return rc;
}

void FAILOVER_HANDLER::set_cluster_id(std::string host, int port) {
    set_cluster_id(host + ":" + std::to_string(port));
}

void FAILOVER_HANDLER::set_cluster_id(std::string cid) {
    this->cluster_id = cid;
    topology_service->set_cluster_id(this->cluster_id);
    metrics_container->set_cluster_id(this->cluster_id);
}

bool FAILOVER_HANDLER::is_dns_pattern_valid(std::string host) {
    return (host.find("?") != std::string::npos);
}

bool FAILOVER_HANDLER::is_rds_dns(std::string host) {
    return std::regex_match(host, AURORA_DNS_PATTERN);
}

bool FAILOVER_HANDLER::is_rds_proxy_dns(std::string host) {
    return std::regex_match(host, AURORA_PROXY_DNS_PATTERN);
}

bool FAILOVER_HANDLER::is_rds_custom_cluster_dns(std::string host) {
    return std::regex_match(host, AURORA_CUSTOM_CLUSTER_PATTERN);
}

#if defined(__APPLE__) || defined(__linux__)
    #define strcmp_case_insensitive(str1, str2) strcasecmp(str1, str2)
#else
    #define strcmp_case_insensitive(str1, str2) strcmpi(str1, str2)
#endif

std::string FAILOVER_HANDLER::get_rds_cluster_host_url(std::string host) {
    std::smatch m;
    if (std::regex_search(host, m, AURORA_DNS_PATTERN) && m.size() > 1) {
        std::string gr1 = m.size() > 1 ? m.str(1) : std::string("");
        std::string gr2 = m.size() > 2 ? m.str(2) : std::string("");
        std::string gr3 = m.size() > 3 ? m.str(3) : std::string("");
        if (!gr1.empty() && !gr3.empty() &&
            (strcmp_case_insensitive(gr2.c_str(), "cluster-") == 0 || strcmp_case_insensitive(gr2.c_str(), "cluster-ro-") == 0)) {
            std::string result;
            result.assign(gr1);
            result.append(".cluster-");
            result.append(gr3);

            return result;
        }
    }
    return "";
}

std::string FAILOVER_HANDLER::get_rds_instance_host_pattern(std::string host) {
    std::smatch m;
    if (std::regex_search(host, m, AURORA_DNS_PATTERN) && m.size() > 3) {
        if (!m.str(3).empty()) {
            std::string result("?.");
            result.append(m.str(3));

            return result;
        }
    }
    return "";
}

bool FAILOVER_HANDLER::is_failover_enabled() {
    return (dbc != nullptr && ds != nullptr &&
            ds->enable_cluster_failover &&
            m_is_cluster_topology_available &&
            !m_is_rds_proxy &&
            !m_is_multi_writer_cluster);
}

bool FAILOVER_HANDLER::is_rds() { return m_is_rds; }

bool FAILOVER_HANDLER::is_rds_proxy() { return m_is_rds_proxy; }

bool FAILOVER_HANDLER::is_cluster_topology_available() {
    return m_is_cluster_topology_available;
}

SQLRETURN FAILOVER_HANDLER::create_connection_and_initialize_topology() {
    SQLRETURN rc = connection_handler->do_connect(dbc, ds, false);
    if (!SQL_SUCCEEDED(rc)) {
        metrics_container->register_invalid_initial_connection(true);
        return rc;
    }

    metrics_container->register_invalid_initial_connection(false);
    current_topology = topology_service->get_topology(dbc->mysql_proxy, false);
    if (current_topology) {
        m_is_multi_writer_cluster = current_topology->is_multi_writer_cluster;
        m_is_cluster_topology_available = current_topology->total_hosts() > 0;
        MYLOG_DBC_TRACE(dbc,
                    "[FAILOVER_HANDLER] m_is_cluster_topology_available=%s",
                    m_is_cluster_topology_available ? "true" : "false");

        // Since we can't determine whether failover should be enabled
        // before we connect, there is a possibility we need to reconnect
        // again with the correct connection settings for failover.
        const unsigned int connect_timeout = get_connect_timeout(ds->connect_timeout);
        const unsigned int network_timeout = get_network_timeout(ds->network_timeout);

        if (is_failover_enabled() && (connect_timeout != dbc->login_timeout ||
                                      network_timeout != ds->read_timeout ||
                                      network_timeout != ds->write_timeout)) {
            rc = reconnect(true);
        }
    }

    return rc;
}

SQLRETURN FAILOVER_HANDLER::reconnect(bool failover_enabled) {
    if (dbc->mysql_proxy != nullptr && dbc->mysql_proxy->is_connected()) {
        dbc->close();
    }
    return connection_handler->do_connect(dbc, ds, failover_enabled);
}

bool FAILOVER_HANDLER::is_ipv4(std::string host) {
    return std::regex_match(host, IPV4_PATTERN);
}

bool FAILOVER_HANDLER::is_ipv6(std::string host) {
    return std::regex_match(host, IPV6_PATTERN) ||
           std::regex_match(host, IPV6_COMPRESSED_PATTERN);
}

// return true if failover is triggered, false if not triggered
bool FAILOVER_HANDLER::trigger_failover_if_needed(const char* error_code,
                                                  const char*& new_error_code,
                                                  const char*& error_msg) {
    new_error_code = error_code;
    std::string ec(error_code ? error_code : "");

    if (!is_failover_enabled() || ec.empty()) {
        return false;
    }

    bool failover_success = false; // If failover happened & succeeded
    bool in_transaction = !autocommit_on(dbc) || dbc->transaction_open;

    if (ec.rfind("08", 0) == 0) {  // start with "08"

        // disable failure detection during failover
        auto failure_detection_old_state = ds->enable_failure_detection;
        ds->enable_failure_detection = false;

        // invalidate current connection
        current_host = nullptr;
        // close transaction if needed
        
        long long elasped_time_ms =
            std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - invoke_start_time_ms).count();
        metrics_container->register_failure_detection_time(elasped_time_ms);

        failover_start_time_ms = std::chrono::steady_clock::now();

        if (current_topology && current_topology->total_hosts() > 1 &&
            ds->allow_reader_connections) {  // there are readers in topology
            failover_success = failover_to_reader(new_error_code, error_msg);
            elasped_time_ms =
                std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - failover_start_time_ms).count();
            metrics_container->register_reader_failover_procedure_time(elasped_time_ms);
        } else {
            failover_success = failover_to_writer(new_error_code, error_msg);
            elasped_time_ms =
                std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - failover_start_time_ms).count();
            metrics_container->register_writer_failover_procedure_time(elasped_time_ms);

        }
        metrics_container->register_failover_connects(failover_success);

        if (failover_success && in_transaction) {
            new_error_code = "08007";
            error_msg = "Connection failure during transaction.";
        }

        ds->enable_failure_detection = failure_detection_old_state;

        return true;
    }

    return false;
}

bool FAILOVER_HANDLER::failover_to_reader(const char*& new_error_code, const char*& error_msg) {
    MYLOG_DBC_TRACE(dbc, "[FAILOVER_HANDLER] Starting reader failover procedure.");
    auto result = failover_reader_handler->failover(current_topology);

    if (result.connected) {
        current_host = result.new_host;
        connection_handler->update_connection(result.new_connection, current_host->get_host());
        new_error_code = "08S02";
        error_msg = "The active SQL connection has changed.";
        MYLOG_DBC_TRACE(dbc,
                    "[FAILOVER_HANDLER] The active SQL connection has changed "
                    "due to a connection failure. Please re-configure session "
                    "state if required.");
        return true;
    } else {
        MYLOG_DBC_TRACE(dbc, "[FAILOVER_HANDLER] Unable to establish SQL connection to reader node.");
        new_error_code = "08S01";
        error_msg = "The active SQL connection was lost.";
        return false;
    }
    return false;
}

bool FAILOVER_HANDLER::failover_to_writer(const char*& new_error_code, const char*& error_msg) {
    MYLOG_DBC_TRACE(dbc, "[FAILOVER_HANDLER] Starting writer failover procedure.");
    auto result = failover_writer_handler->failover(current_topology);

    if (!result.connected) {
        MYLOG_DBC_TRACE(dbc, "[FAILOVER_HANDLER] Unable to establish SQL connection to writer node.");
        new_error_code = "08S01";
        error_msg = "The active SQL connection was lost.";
        return false;
    }
    if (result.is_new_host) {
        // connected to a new writer host; take it over
        current_topology = result.new_topology;
        current_host = current_topology->get_writer();
    }

    connection_handler->update_connection(
        result.new_connection, result.new_topology->get_writer()->get_host());
    
    new_error_code = "08S02";
    error_msg = "The active SQL connection has changed.";
    MYLOG_DBC_TRACE(
        dbc,
        "[FAILOVER_HANDLER] The active SQL connection has changed due to a "
        "connection failure. Please re-configure session state if required.");
    return true;
}

void FAILOVER_HANDLER::invoke_start_time() {
    invoke_start_time_ms = std::chrono::steady_clock::now();
}
