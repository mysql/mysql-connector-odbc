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

#ifndef __TOPOLOGYSERVICE_H__
#define __TOPOLOGYSERVICE_H__

#include "cluster_aware_metrics_container.h"
#include "cluster_topology_info.h"
#include "mysql_proxy.h"
#include "mylog.h"

#include <map>
#include <mutex>
#include <cstring>
#include <chrono>
#include <ctime>

// TODO - consider - do we really need miliseconds for refresh? - the default numbers here are already 30 seconds.000;
#define DEFAULT_REFRESH_RATE_IN_MILLISECONDS 30000
#define WRITER_SESSION_ID "MASTER_SESSION_ID"
#define RETRIEVE_TOPOLOGY_SQL "SELECT SERVER_ID, SESSION_ID, LAST_UPDATE_TIMESTAMP, REPLICA_LAG_IN_MILLISECONDS \
    FROM information_schema.replica_host_status \
    WHERE time_to_sec(timediff(now(), LAST_UPDATE_TIMESTAMP)) <= 300 \
    ORDER BY LAST_UPDATE_TIMESTAMP DESC"

static std::map<std::string, std::shared_ptr<CLUSTER_TOPOLOGY_INFO>> topology_cache;
static std::mutex topology_cache_mutex;

class TOPOLOGY_SERVICE {
public:
    TOPOLOGY_SERVICE(unsigned long dbc_id, bool enable_logging = false);
    TOPOLOGY_SERVICE(const TOPOLOGY_SERVICE&);
    virtual ~TOPOLOGY_SERVICE();

    virtual void set_cluster_id(std::string cluster_id);
    virtual void set_cluster_instance_template(std::shared_ptr<HOST_INFO> host_template);  //is this equivalent to setcluster_instance_host

    virtual std::shared_ptr<CLUSTER_TOPOLOGY_INFO> get_topology(
        MYSQL_PROXY* connection, bool force_update = false);
    std::shared_ptr<CLUSTER_TOPOLOGY_INFO> get_cached_topology();

    std::shared_ptr<HOST_INFO> get_last_used_reader();
    void set_last_used_reader(std::shared_ptr<HOST_INFO> reader);
    std::set<std::string> get_down_hosts();
    virtual void mark_host_down(std::shared_ptr<HOST_INFO> host);
    virtual void mark_host_up(std::shared_ptr<HOST_INFO> host);
    void set_refresh_rate(int refresh_rate);
    void set_gather_metric(bool can_gather);
    void clear_all();
    void clear();

    // Property Keys
    const std::string SESSION_ID = "TOPOLOGY_SERVICE_SESSION_ID";
    const std::string LAST_UPDATED = "TOPOLOGY_SERVICE_LAST_UPDATE_TIMESTAMP";
    const std::string REPLICA_LAG = "TOPOLOGY_SERVICE_REPLICA_LAG_IN_MILLISECONDS";
    const std::string INSTANCE_NAME = "TOPOLOGY_SERVICE_SERVER_ID";

private:
    const int DEFAULT_CACHE_EXPIRE_MS = 5 * 60 * 1000; // 5 min

    const std::string GET_INSTANCE_NAME_SQL = "SELECT @@aurora_server_id";
    const std::string GET_INSTANCE_NAME_COL = "@@aurora_server_id";

    const std::string FIELD_SERVER_ID = "SERVER_ID";
    const std::string FIELD_SESSION_ID = "SESSION_ID";
    const std::string FIELD_LAST_UPDATED = "LAST_UPDATE_TIMESTAMP";
    const std::string FIELD_REPLICA_LAG = "REPLICA_LAG_IN_MILLISECONDS";

    std::shared_ptr<FILE> logger = nullptr;
    unsigned long dbc_id = 0;

protected:
    const int NO_CONNECTION_INDEX = -1;
    int refresh_rate_in_ms;

    std::string cluster_id;
    std::shared_ptr<HOST_INFO> cluster_instance_host;

    std::shared_ptr<CLUSTER_AWARE_METRICS_CONTAINER> metrics_container;

    bool refresh_needed(std::time_t last_updated);
    std::shared_ptr<CLUSTER_TOPOLOGY_INFO> query_for_topology(MYSQL_PROXY* connection);
    std::shared_ptr<HOST_INFO> create_host(MYSQL_ROW& row);
    std::string get_host_endpoint(const char* node_name);
    static bool does_instance_exist(
        std::map<std::string, std::shared_ptr<HOST_INFO>>& instances,
        std::shared_ptr<HOST_INFO> host_info);

    std::shared_ptr<CLUSTER_TOPOLOGY_INFO> get_from_cache();
    void put_to_cache(std::shared_ptr<CLUSTER_TOPOLOGY_INFO> topology_info);

    MYSQL_RES* try_execute_query(MYSQL_PROXY* mysql_proxy, const char* query);
};

#endif /* __TOPOLOGYSERVICE_H__ */
