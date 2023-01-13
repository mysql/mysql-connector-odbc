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
#include "topology_service.h"

TOPOLOGY_SERVICE::TOPOLOGY_SERVICE(unsigned long dbc_id, bool enable_logging)
    : dbc_id{dbc_id},
      cluster_instance_host{nullptr},
      refresh_rate_in_ms{DEFAULT_REFRESH_RATE_IN_MILLISECONDS},
      metrics_container{std::make_shared<CLUSTER_AWARE_METRICS_CONTAINER>()}{

  // TODO get better initial cluster id
  time_t now = time(0);
  cluster_id = std::to_string(now) + ctime(&now);
  if (enable_logging)
    logger = init_log_file();
}

TOPOLOGY_SERVICE::TOPOLOGY_SERVICE(const TOPOLOGY_SERVICE& ts) {
    refresh_rate_in_ms = ts.refresh_rate_in_ms;
    cluster_id = ts.cluster_id;
    cluster_instance_host = ts.cluster_instance_host;
    logger = ts.logger;
    dbc_id = ts.dbc_id;
    metrics_container = ts.metrics_container;
}

TOPOLOGY_SERVICE::~TOPOLOGY_SERVICE() {
    if (cluster_instance_host) 
        cluster_instance_host.reset();
}

void TOPOLOGY_SERVICE::set_cluster_id(std::string cid) {
    MYLOG_TRACE(logger.get(), dbc_id, "[TOPOLOGY_SERVICE] cluster ID=%s", cid.c_str());
    this->cluster_id = cid;
    metrics_container->set_cluster_id(this->cluster_id);
}

void TOPOLOGY_SERVICE::set_cluster_instance_template(std::shared_ptr<HOST_INFO> host_template) {

    // NOTE, this may not have to be a pointer. Copy the information passed to this function.
    // Alernatively the create host function should be part of the Topologyservice, even protected or private one
    // and host information passed as separate parameters.
    if (cluster_instance_host)
        cluster_instance_host.reset();

    MYLOG_TRACE(logger.get(), dbc_id,
                "[TOPOLOGY_SERVICE] cluster instance host=%s, port=%d",
                host_template->get_host().c_str(), host_template->get_port());
    cluster_instance_host = host_template;
}

void TOPOLOGY_SERVICE::set_refresh_rate(int refresh_rate) {
    refresh_rate_in_ms = refresh_rate;
}

std::shared_ptr<HOST_INFO> TOPOLOGY_SERVICE::get_last_used_reader() {
    auto topology_info = get_from_cache();
    if (!topology_info || refresh_needed(topology_info->time_last_updated())) {
        return nullptr;
    }

    return topology_info->get_last_used_reader();
}

void TOPOLOGY_SERVICE::set_last_used_reader(std::shared_ptr<HOST_INFO> reader) {
    if (reader) {
        std::unique_lock<std::mutex> lock(topology_cache_mutex);
        auto topology_info = get_from_cache();
        if (topology_info) {
            topology_info->set_last_used_reader(reader);
        }
        lock.unlock();
    }
}

std::set<std::string> TOPOLOGY_SERVICE::get_down_hosts() {
    std::set<std::string> down_hosts;

    std::unique_lock<std::mutex> lock(topology_cache_mutex);
    auto topology_info = get_from_cache();
    if (topology_info) {
        down_hosts = topology_info->get_down_hosts();
    }
    lock.unlock();

    return down_hosts;
}

void TOPOLOGY_SERVICE::mark_host_down(std::shared_ptr<HOST_INFO> host) {
    if (!host) {
        return;
    }

    std::unique_lock<std::mutex> lock(topology_cache_mutex);

    auto topology_info = get_from_cache();
    if (topology_info) {
        topology_info->mark_host_down(host);
    }

    lock.unlock();
}

void TOPOLOGY_SERVICE::mark_host_up(std::shared_ptr<HOST_INFO> host) {
    if (!host) {
        return;
    }

    std::unique_lock<std::mutex> lock(topology_cache_mutex);

    auto topology_info = get_from_cache();
    if (topology_info) {
        topology_info->mark_host_up(host);
    }

    lock.unlock();
}

void TOPOLOGY_SERVICE::set_gather_metric(bool can_gather) {
    this->metrics_container->set_gather_metric(can_gather);
}

void TOPOLOGY_SERVICE::clear_all() {
    std::unique_lock<std::mutex> lock(topology_cache_mutex);
    topology_cache.clear();
    lock.unlock();
}

void TOPOLOGY_SERVICE::clear() {
    std::unique_lock<std::mutex> lock(topology_cache_mutex);
    topology_cache.erase(cluster_id);
    lock.unlock();
}

std::shared_ptr<CLUSTER_TOPOLOGY_INFO> TOPOLOGY_SERVICE::get_cached_topology() {
    return get_from_cache();
}

//TODO consider the return value
//Note to determine whether or not force_update succeeded one would compare
// CLUSTER_TOPOLOGY_INFO->time_last_updated() prior and after the call if non-null information was given prior.
std::shared_ptr<CLUSTER_TOPOLOGY_INFO> TOPOLOGY_SERVICE::get_topology(MYSQL_PROXY* connection, bool force_update)
{
    //TODO reconsider using this cache. It appears that we only store information for the current cluster Id.
    // therefore instead of a map we can just keep CLUSTER_TOPOLOGY_INFO* topology_info member variable.
    auto cached_topology = get_from_cache();
    if (!cached_topology
        || force_update
        || refresh_needed(cached_topology->time_last_updated()))
    {
        auto latest_topology = query_for_topology(connection);
        if (latest_topology) {
            put_to_cache(latest_topology);
            return latest_topology;
        }
    }

    return cached_topology;
}

// TODO consider thread safety and usage of pointers
std::shared_ptr<CLUSTER_TOPOLOGY_INFO> TOPOLOGY_SERVICE::get_from_cache() {
    if (topology_cache.empty()) {
        metrics_container->register_use_cached_topology(false);
        return nullptr;
    }
        
    auto result = topology_cache.find(cluster_id);
    if (result == topology_cache.end()) {
        metrics_container->register_use_cached_topology(false);
        return nullptr;
    }
    
    metrics_container->register_use_cached_topology(true);
    return result->second;
}

// TODO consider thread safety and usage of pointers
void TOPOLOGY_SERVICE::put_to_cache(std::shared_ptr<CLUSTER_TOPOLOGY_INFO> topology_info) {
    if (!topology_cache.empty())
    {
        auto result = topology_cache.find(cluster_id);
        if (result != topology_cache.end()) {
            result->second.reset();
            result->second = topology_info;
            return;
        }
    }
    std::unique_lock<std::mutex> lock(topology_cache_mutex);
    topology_cache[cluster_id] = topology_info;
    lock.unlock();
}

MYSQL_RES* TOPOLOGY_SERVICE::try_execute_query(MYSQL_PROXY* mysql_proxy, const char* query) {
    if (mysql_proxy != nullptr && mysql_proxy->query(query) == 0) {
        return mysql_proxy->store_result();
    }

    return nullptr;
}

// TODO harmonize time function across objects so the times are comparable
bool TOPOLOGY_SERVICE::refresh_needed(std::time_t last_updated) {

    return  time(0) - last_updated > (refresh_rate_in_ms / 1000);
}

std::shared_ptr<HOST_INFO> TOPOLOGY_SERVICE::create_host(MYSQL_ROW& row) {

    //TEMP and TODO figure out how to fetch values from row by name, not by ordinal for now this enum is matching
    // order of columns in the query
    enum COLUMNS {
        SERVER_ID,
        SESSION,
        LAST_UPDATE_TIMESTAMP,
        REPLICA_LAG_MILLISECONDS
    };

    if (row[SERVER_ID] == NULL) {
        return nullptr;  // will not be able to generate host endpoint so no point. TODO: log this condition?
    }

    std::string host_endpoint = get_host_endpoint(row[SERVER_ID]);

    //TODO check cluster_instance_host for NULL, or decide what is needed out of it
    std::shared_ptr<HOST_INFO> host_info = std::make_shared<HOST_INFO>(
        host_endpoint, cluster_instance_host->get_port());

    //TODO do case-insensitive comparison
    // side note: how stable this is on the server side? If it changes there we will not detect a writer.
    if (strcmp(row[SESSION], WRITER_SESSION_ID) == 0)
    {
        host_info->mark_as_writer(true);
    }

    host_info->instance_name = row[SERVER_ID] ? row[SERVER_ID] : "";
    host_info->session_id = row[SESSION] ? row[SESSION] : "";
    host_info->last_updated = row[LAST_UPDATE_TIMESTAMP] ? row[LAST_UPDATE_TIMESTAMP] : "";
    host_info->replica_lag = row[REPLICA_LAG_MILLISECONDS] ? row[REPLICA_LAG_MILLISECONDS] : "";

    return host_info;
}

// If no host information retrieved return NULL
std::shared_ptr<CLUSTER_TOPOLOGY_INFO> TOPOLOGY_SERVICE::query_for_topology(MYSQL_PROXY* mysql_proxy) {

    std::shared_ptr<CLUSTER_TOPOLOGY_INFO> topology_info = nullptr;

    std::chrono::steady_clock::time_point start_time_ms = std::chrono::steady_clock::now();
    if (MYSQL_RES* result = try_execute_query(mysql_proxy, RETRIEVE_TOPOLOGY_SQL)) {
        topology_info = std::make_shared<CLUSTER_TOPOLOGY_INFO>();
        std::map<std::string, std::shared_ptr<HOST_INFO>> instances;
        MYSQL_ROW row;
        int writer_count = 0;
        while ((row = mysql_proxy->fetch_row(result))) {
            std::shared_ptr<HOST_INFO> host_info = create_host(row);
            if (host_info) {
                // Only mark the first/latest writer as true writer
                if (host_info->is_host_writer()) {
                    if (writer_count > 0) {
                        host_info->mark_as_writer(false);
                    }
                    writer_count++;
                }
                // Add to topology if instance not seen before
                if (!TOPOLOGY_SERVICE::does_instance_exist(instances, host_info)) {
                    topology_info->add_host(host_info);
                }
            }
        }
        mysql_proxy->free_result(result);

        topology_info->is_multi_writer_cluster = writer_count > 1;
        if (writer_count == 0) {
            MYLOG_TRACE(logger.get(), dbc_id,
                        "[TOPOLOGY_SERVICE] The topology query returned an "
                        "invalid topology - no writer instance detected");
        }
    }

    std::chrono::steady_clock::time_point end_time_ms = std::chrono::steady_clock::now();
    long long elasped_time_ms = std::chrono::duration_cast<std::chrono::milliseconds>(end_time_ms - start_time_ms).count();
    metrics_container->register_topology_query_execution_time(elasped_time_ms);
    return topology_info;
}

bool TOPOLOGY_SERVICE::does_instance_exist(
    std::map<std::string, std::shared_ptr<HOST_INFO>>& instances,
    std::shared_ptr<HOST_INFO> host_info) {
    
    auto duplicate = instances.find(host_info->instance_name);
    if (duplicate != instances.end()) {
        return true;
    } else {
        instances.insert(std::pair<std::string, std::shared_ptr<HOST_INFO>>(
            host_info->instance_name, host_info));
        return false;
    }
}

std::string TOPOLOGY_SERVICE::get_host_endpoint(const char* node_name) {
    std::string host = cluster_instance_host->get_host();
    size_t position = host.find("?");
    if (position != std::string::npos) {
        host.replace(position, 1, node_name);
    }
    return host;
}
