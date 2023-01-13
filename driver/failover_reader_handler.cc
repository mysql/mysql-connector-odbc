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

#include "failover.h"

#include <algorithm>
#include <chrono>
#include <future>
#include <random>
#include <thread>
#include <vector>

FAILOVER_READER_HANDLER::FAILOVER_READER_HANDLER(
    std::shared_ptr<TOPOLOGY_SERVICE> topology_service,
    std::shared_ptr<FAILOVER_CONNECTION_HANDLER> connection_handler,
    int failover_timeout_ms, int failover_reader_connect_timeout,
    unsigned long dbc_id, bool enable_logging)
    : topology_service{topology_service},
      connection_handler{connection_handler},
      max_failover_timeout_ms{failover_timeout_ms},
      reader_connect_timeout_ms{failover_reader_connect_timeout},
      dbc_id{dbc_id} {

    if (enable_logging)
        logger = init_log_file();
}

FAILOVER_READER_HANDLER::~FAILOVER_READER_HANDLER() {}

// Function called to start the Reader Failover process.
// This process will generate a list of available hosts: First readers that are up, then readers marked as down, then writers.
// If it goes through the list and does not succeed to connect, it tries again, endlessly.
READER_FAILOVER_RESULT FAILOVER_READER_HANDLER::failover(
    std::shared_ptr<CLUSTER_TOPOLOGY_INFO> current_topology) {

    READER_FAILOVER_RESULT reader_result(false, nullptr, nullptr);
    if (!current_topology || current_topology->total_hosts() == 0) {
        return reader_result;
    }

    FAILOVER_SYNC global_sync(1);

    auto reader_result_future = std::async(std::launch::async, [=, &global_sync, &reader_result]() {
        std::vector<std::shared_ptr<HOST_INFO>> hosts_list;
        while (!global_sync.is_completed()) {
            hosts_list = build_hosts_list(current_topology, true);
            reader_result = get_connection_from_hosts(hosts_list, global_sync);
            if (reader_result.connected) {
                global_sync.mark_as_complete(true);
                return;
            }
            // TODO Think of changes to the strategy if it went
            // through all the hosts and did not connect.
            std::this_thread::sleep_for(std::chrono::seconds(READER_CONNECT_INTERVAL_SEC));
        }
    });

    global_sync.wait_and_complete(max_failover_timeout_ms);

    if (reader_result_future.wait_for(std::chrono::seconds(0)) == std::future_status::ready) {
        reader_result_future.get();
    }
    
    return reader_result;
}

// Function to connect to a reader host. Often used to query/update the topology.
// If it goes through the list of readers and fails to connect, it tries again, endlessly.
// This function only tries to connect to reader hosts.
READER_FAILOVER_RESULT FAILOVER_READER_HANDLER::get_reader_connection(
    std::shared_ptr<CLUSTER_TOPOLOGY_INFO> topology_info,
    FAILOVER_SYNC& f_sync) {

    // We build a list of all readers, up then down, without writers.
    auto hosts = build_hosts_list(topology_info, false);

    while (!f_sync.is_completed()) {
        auto reader_result = get_connection_from_hosts(hosts, f_sync);
        // TODO Think of changes to the strategy if it went through all the readers and did not connect.
        if (reader_result.connected) {
            return reader_result;
        }
    }
    // Return a false result if the connection request has been cancelled.
    return READER_FAILOVER_RESULT(false, nullptr, nullptr);
}

// Function that reads the topology and builds a list of hosts to connect to, in order of priority.
// boolean include_writers indicate whether one wants to append the writers to the end of the list or not.
std::vector<std::shared_ptr<HOST_INFO>> FAILOVER_READER_HANDLER::build_hosts_list(
    const std::shared_ptr<CLUSTER_TOPOLOGY_INFO>& topology_info,
    bool include_writers) {

    std::vector<std::shared_ptr<HOST_INFO>> hosts_list;

    // We split reader hosts that are marked up from those marked down.
    std::vector<std::shared_ptr<HOST_INFO>> readers_up;
    std::vector<std::shared_ptr<HOST_INFO>> readers_down;

    auto readers = topology_info->get_readers();

    for (auto reader : readers) {
        if (reader->is_host_down()) {
            readers_down.push_back(reader);
        } else {
            readers_up.push_back(reader);
        }
    }

    // Both lists of readers up and down are shuffled.
    auto rng = std::default_random_engine{};
    std::shuffle(std::begin(readers_up), std::end(readers_up), rng);
    std::shuffle(std::begin(readers_down), std::end(readers_down), rng);

    // Readers that are marked up go first, readers marked down go after.
    hosts_list.insert(hosts_list.end(), readers_up.begin(), readers_up.end());
    hosts_list.insert(hosts_list.end(), readers_down.begin(), readers_down.end());

    if (include_writers) {
        auto writers = topology_info->get_writers();
        std::shuffle(std::begin(writers), std::end(writers), rng);
        hosts_list.insert(hosts_list.end(), writers.begin(), writers.end());
    }

    return hosts_list;
}

READER_FAILOVER_RESULT FAILOVER_READER_HANDLER::get_connection_from_hosts(
    std::vector<std::shared_ptr<HOST_INFO>> hosts_list, FAILOVER_SYNC& global_sync) {

    size_t total_hosts = hosts_list.size();
    size_t i = 0;

    // This loop should end once it reaches the end of the list without a successful connection.
    // The function calling it already has a neverending loop looking for a connection.
    // Ending this loop will allow the calling function to update the list or change strategy if this failed.
    while (!global_sync.is_completed() && i < total_hosts) {
        // This boolean verifies if the next host in the list is also the last, meaning there's no host for the second thread.
        bool odd_hosts_number = (i + 1 == total_hosts);

        FAILOVER_SYNC local_sync(1);
        if (!odd_hosts_number) {
            local_sync.increment_task();
        }

        CONNECT_TO_READER_HANDLER first_connection_handler(connection_handler, topology_service, dbc_id, logger != nullptr);
        std::future<void> first_connection_future;
        READER_FAILOVER_RESULT first_connection_result(false, nullptr, nullptr);

        CONNECT_TO_READER_HANDLER second_connection_handler(connection_handler, topology_service, dbc_id, logger != nullptr);
        std::future<void> second_connection_future;
        READER_FAILOVER_RESULT second_connection_result(false, nullptr, nullptr);
        
        std::shared_ptr<HOST_INFO> first_reader_host = hosts_list.at(i);
        first_connection_future = std::async(std::launch::async, std::ref(first_connection_handler),
                                             std::ref(first_reader_host), std::ref(local_sync),
                                             std::ref(first_connection_result));

        if (!odd_hosts_number) {
            std::shared_ptr<HOST_INFO> second_reader_host = hosts_list.at(i + 1);
            second_connection_future = std::async(std::launch::async, std::ref(second_connection_handler),
                                                  std::ref(second_reader_host), std::ref(local_sync),
                                                  std::ref(second_connection_result));
        }

        local_sync.wait_and_complete(reader_connect_timeout_ms);

        if (first_connection_future.wait_for(std::chrono::seconds(0)) == std::future_status::ready) {
            first_connection_future.get();
        }
        if (!odd_hosts_number && 
            second_connection_future.wait_for(std::chrono::seconds(0)) == std::future_status::ready) {

            second_connection_future.get();
        }

        if (first_connection_result.connected) {
            MYLOG_TRACE(logger.get(), dbc_id,
                        "[FAILOVER_READER_HANDLER] Connected to reader: %s",
                        first_connection_result.new_host->get_host_port_pair().c_str());
            return first_connection_result;
        } else if (!odd_hosts_number && second_connection_result.connected) {
            MYLOG_TRACE(logger.get(), dbc_id,
                        "[FAILOVER_READER_HANDLER] Connected to reader: %s",
                        second_connection_result.new_host->get_host_port_pair().c_str());
            return second_connection_result;
        }
        // None has connected. We move on and try new hosts.
        i += 2;
        std::this_thread::sleep_for(std::chrono::seconds(READER_CONNECT_INTERVAL_SEC));
    }

    // The operation was either cancelled either reached the end of the list without connecting.
    return READER_FAILOVER_RESULT(false, nullptr, nullptr);
}

// *** CONNECT_TO_READER_HANDLER
// Handler to connect to a reader host.
CONNECT_TO_READER_HANDLER::CONNECT_TO_READER_HANDLER(
    std::shared_ptr<FAILOVER_CONNECTION_HANDLER> connection_handler,
    std::shared_ptr<TOPOLOGY_SERVICE> topology_service,
    unsigned long dbc_id, bool enable_logging)
    : FAILOVER{connection_handler, topology_service, dbc_id, enable_logging} {}

CONNECT_TO_READER_HANDLER::~CONNECT_TO_READER_HANDLER() {}

void CONNECT_TO_READER_HANDLER::operator()(
    std::shared_ptr<HOST_INFO> reader,
    FAILOVER_SYNC& f_sync,
    READER_FAILOVER_RESULT& result) {
    
    if (reader && !f_sync.is_completed()) {

        MYLOG_TRACE(logger.get(), dbc_id,
                    "[CONNECT_TO_READER_HANDLER] Trying to connect to reader: %s",
                    reader->get_host_port_pair().c_str());

        if (connect(reader)) {
            topology_service->mark_host_up(reader);
            if (f_sync.is_completed()) {
                // If another thread finishes first, or both timeout, this thread is canceled.
                release_new_connection();
            } else {
                result = READER_FAILOVER_RESULT(true, reader, std::move(this->new_connection));
                f_sync.mark_as_complete(true);
                MYLOG_TRACE(
                    logger.get(), dbc_id,
                    "[CONNECT_TO_READER_HANDLER] Connected to reader: %s",
                    reader->get_host_port_pair().c_str());
                return;
            }
        } else {
            topology_service->mark_host_down(reader);
            MYLOG_TRACE(
                logger.get(), dbc_id,
                "[CONNECT_TO_READER_HANDLER] Failed to connect to reader: %s",
                reader->get_host_port_pair().c_str());
            if (!f_sync.is_completed()) {
                f_sync.mark_as_complete(false);
            }
        }
    }

    release_new_connection();
}
