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

#ifndef __CLUSTERAWAREMETRICSCONTAINER_H__
#define __CLUSTERAWAREMETRICSCONTAINER_H__

#include <unordered_map>
#include <functional>

#include "cluster_aware_time_metrics_holder.h"
#include "cluster_aware_metrics.h"
#include "mylog.h"

#define DEFAULT_CLUSTER_ID "no_id"

struct DBC;
struct DataSource;

class CLUSTER_AWARE_METRICS_CONTAINER {
public:
    CLUSTER_AWARE_METRICS_CONTAINER();
    CLUSTER_AWARE_METRICS_CONTAINER(DBC* dbc, DataSource* ds);

    ~CLUSTER_AWARE_METRICS_CONTAINER();

    void set_cluster_id(std::string cluster_id);
    void register_failure_detection_time(long long time_ms);
    void register_writer_failover_procedure_time(long long time_ms);
    void register_reader_failover_procedure_time(long long time_ms);
    void register_failover_connects(bool is_hit);
    void register_invalid_initial_connection(bool is_hit);
    void register_use_cached_topology(bool is_hit);
    void register_topology_query_execution_time(long long time_ms);
    
    void set_gather_metric(bool gather);

    static void report_metrics(std::string conn_url, bool for_instances, FILE* log, unsigned long dbc_id);
    static std::string report_metrics(std::string conn_url, bool for_instances);
    static void reset_metrics();

private:
    // ClusterID, Metrics
    static std::unordered_map<std::string, std::shared_ptr<CLUSTER_AWARE_METRICS>> cluster_metrics;
    // Instance URL, Metrics
    static std::unordered_map<std::string, std::shared_ptr<CLUSTER_AWARE_METRICS>> instance_metrics;

    bool can_gather = false;    
    DBC* dbc = nullptr;
    DataSource* ds = nullptr;
    std::string cluster_id = DEFAULT_CLUSTER_ID;

protected:
    bool is_enabled();
    bool is_instance_metrics_enabled();

    std::shared_ptr<CLUSTER_AWARE_METRICS> get_cluster_metrics(std::string key);
    std::shared_ptr<CLUSTER_AWARE_METRICS> get_instance_metrics(std::string key);
    virtual std::string get_curr_conn_url();

    void register_metrics(std::function<void(std::shared_ptr<CLUSTER_AWARE_METRICS>)> lambda);
};

#endif /* __CLUSTERAWAREMETRICSCONTAINER_H__ */
