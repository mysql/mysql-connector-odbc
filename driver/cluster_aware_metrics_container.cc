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

#include "cluster_aware_metrics_container.h"
#include "driver.h"
#include "installer.h"

std::unordered_map<std::string, std::shared_ptr<CLUSTER_AWARE_METRICS>> CLUSTER_AWARE_METRICS_CONTAINER::cluster_metrics = {};
std::unordered_map<std::string, std::shared_ptr<CLUSTER_AWARE_METRICS>> CLUSTER_AWARE_METRICS_CONTAINER::instance_metrics = {};

CLUSTER_AWARE_METRICS_CONTAINER::CLUSTER_AWARE_METRICS_CONTAINER() {}

CLUSTER_AWARE_METRICS_CONTAINER::CLUSTER_AWARE_METRICS_CONTAINER(DBC* dbc, DataSource* ds) {
    this->dbc = dbc;
    this->ds = ds;
}

CLUSTER_AWARE_METRICS_CONTAINER::~CLUSTER_AWARE_METRICS_CONTAINER() {}

void CLUSTER_AWARE_METRICS_CONTAINER::set_cluster_id(std::string id) {
    this->cluster_id = id;
}

void CLUSTER_AWARE_METRICS_CONTAINER::register_failure_detection_time(long long time_ms) {
    CLUSTER_AWARE_METRICS_CONTAINER::register_metrics([time_ms](std::shared_ptr<CLUSTER_AWARE_METRICS> metrics){metrics->register_failure_detection_time(time_ms);});
}

void CLUSTER_AWARE_METRICS_CONTAINER::register_writer_failover_procedure_time(long long time_ms) {
    CLUSTER_AWARE_METRICS_CONTAINER::register_metrics([time_ms](std::shared_ptr<CLUSTER_AWARE_METRICS> metrics){metrics->register_writer_failover_procedure_time(time_ms);});
}

void CLUSTER_AWARE_METRICS_CONTAINER::register_reader_failover_procedure_time(long long time_ms) {
    CLUSTER_AWARE_METRICS_CONTAINER::register_metrics([time_ms](std::shared_ptr<CLUSTER_AWARE_METRICS> metrics){metrics->register_reader_failover_procedure_time(time_ms);});
}

void CLUSTER_AWARE_METRICS_CONTAINER::register_failover_connects(bool is_hit) {
    CLUSTER_AWARE_METRICS_CONTAINER::register_metrics([is_hit](std::shared_ptr<CLUSTER_AWARE_METRICS> metrics){metrics->register_failover_connects(is_hit);});
}

void CLUSTER_AWARE_METRICS_CONTAINER::register_invalid_initial_connection(bool is_hit) {
    CLUSTER_AWARE_METRICS_CONTAINER::register_metrics([is_hit](std::shared_ptr<CLUSTER_AWARE_METRICS> metrics){metrics->register_invalid_initial_connection(is_hit);});
}

void CLUSTER_AWARE_METRICS_CONTAINER::register_use_cached_topology(bool is_hit) {
    CLUSTER_AWARE_METRICS_CONTAINER::register_metrics([is_hit](std::shared_ptr<CLUSTER_AWARE_METRICS> metrics){metrics->register_use_cached_topology(is_hit);});
}

void CLUSTER_AWARE_METRICS_CONTAINER::register_topology_query_execution_time(long long time_ms) {
    CLUSTER_AWARE_METRICS_CONTAINER::register_metrics([time_ms](std::shared_ptr<CLUSTER_AWARE_METRICS> metrics){metrics->register_topology_query_time(time_ms);});
}

void CLUSTER_AWARE_METRICS_CONTAINER::set_gather_metric(bool gather) {
    this->can_gather = gather;
}

void CLUSTER_AWARE_METRICS_CONTAINER::report_metrics(std::string conn_url, bool for_instances, FILE* log, unsigned long dbc_id) {
    if (!log) {
        return;
    }
    
    MYLOG_TRACE(log, dbc_id, report_metrics(conn_url, for_instances).c_str());
}

std::string CLUSTER_AWARE_METRICS_CONTAINER::report_metrics(std::string conn_url, bool for_instances) {
    std::string log_message = "\n";

    std::unordered_map<std::string, std::shared_ptr<CLUSTER_AWARE_METRICS>>::const_iterator has_metrics = for_instances ? instance_metrics.find(conn_url) : cluster_metrics.find(conn_url);
    if ((for_instances ? instance_metrics.end() : cluster_metrics.end()) == has_metrics) {
        log_message.append("** No metrics collected for '")
            .append(conn_url)
            .append("' **\n");

        return log_message;
    }

    std::shared_ptr<CLUSTER_AWARE_METRICS> metrics = has_metrics->second;

    log_message.append("** Performance Metrics Report for '")
        .append(conn_url)
        .append("' **\n");

    log_message.append(metrics->report_metrics())
        .append("\n");

    return log_message;
}

void CLUSTER_AWARE_METRICS_CONTAINER::reset_metrics() {
    cluster_metrics.clear();
    instance_metrics.clear();
}

bool CLUSTER_AWARE_METRICS_CONTAINER::is_enabled() {
    if (ds) {
        return ds->gather_perf_metrics;
    }
    return can_gather;
}

bool CLUSTER_AWARE_METRICS_CONTAINER::is_instance_metrics_enabled() {
    if (ds) {
        return ds->gather_metrics_per_instance;
    }
    return can_gather;
}

std::shared_ptr<CLUSTER_AWARE_METRICS> CLUSTER_AWARE_METRICS_CONTAINER::get_cluster_metrics(std::string key) {
    cluster_metrics.emplace(key, std::make_shared<CLUSTER_AWARE_METRICS>());
    return cluster_metrics.at(key);
}

std::shared_ptr<CLUSTER_AWARE_METRICS> CLUSTER_AWARE_METRICS_CONTAINER::get_instance_metrics(std::string key) {
    instance_metrics.emplace(key, std::make_shared<CLUSTER_AWARE_METRICS>());
    return instance_metrics.at(key);
}

std::string CLUSTER_AWARE_METRICS_CONTAINER::get_curr_conn_url() {
    std::string curr_url = "[Unknown Url]";
    if (dbc && dbc->mysql_proxy) {
        curr_url = dbc->mysql_proxy->get_host();
        curr_url.append(":").append(std::to_string(dbc->mysql_proxy->get_port()));
    }
    return curr_url;
}

void CLUSTER_AWARE_METRICS_CONTAINER::register_metrics(std::function<void(std::shared_ptr<CLUSTER_AWARE_METRICS>)> lambda) {
    if (!is_enabled()) {
        return;
    }

    lambda(get_cluster_metrics(this->cluster_id));

    if (is_instance_metrics_enabled()) {
        lambda(get_instance_metrics(get_curr_conn_url()));
    }
}
