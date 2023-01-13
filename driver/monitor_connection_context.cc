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

#include "monitor_connection_context.h"
#include "driver.h"

#include <algorithm>
#include <mutex>

MONITOR_CONNECTION_CONTEXT::MONITOR_CONNECTION_CONTEXT(DBC* connection_to_abort,
    std::set<std::string> node_keys,
    std::chrono::milliseconds failure_detection_time,
    std::chrono::milliseconds failure_detection_interval,
    int failure_detection_count,
    bool enable_logging) : connection_to_abort{connection_to_abort},
                                   node_keys{node_keys},
                                   failure_detection_time{failure_detection_time},
                                   failure_detection_interval{failure_detection_interval},
                                   failure_detection_count{failure_detection_count},
                                   failure_count{0},
                                   node_unhealthy{false} {
    if (enable_logging)
        this->logger = init_log_file();
}

MONITOR_CONNECTION_CONTEXT::~MONITOR_CONNECTION_CONTEXT() {}

std::chrono::steady_clock::time_point MONITOR_CONNECTION_CONTEXT::get_start_monitor_time() {
    return start_monitor_time;
}

void MONITOR_CONNECTION_CONTEXT::set_start_monitor_time(std::chrono::steady_clock::time_point time) {
    start_monitor_time = time;
}

std::set<std::string> MONITOR_CONNECTION_CONTEXT::get_node_keys() {
    return node_keys;
}

std::chrono::milliseconds MONITOR_CONNECTION_CONTEXT::get_failure_detection_time() {
    return failure_detection_time;
}

std::chrono::milliseconds MONITOR_CONNECTION_CONTEXT::get_failure_detection_interval() {
    return failure_detection_interval;
}

int MONITOR_CONNECTION_CONTEXT::get_failure_detection_count() {
    return failure_detection_count;
}

int MONITOR_CONNECTION_CONTEXT::get_failure_count() {
    return failure_count;
}

void MONITOR_CONNECTION_CONTEXT::set_failure_count(int count) {
    failure_count = count;
}

void MONITOR_CONNECTION_CONTEXT::increment_failure_count() {
    failure_count++;
}

void MONITOR_CONNECTION_CONTEXT::set_invalid_node_start_time(std::chrono::steady_clock::time_point time) {
    invalid_node_start_time = time;
}

void MONITOR_CONNECTION_CONTEXT::reset_invalid_node_start_time() {
    std::chrono::steady_clock::time_point timestamp_zero{};
    invalid_node_start_time = timestamp_zero;
}

bool MONITOR_CONNECTION_CONTEXT::is_invalid_node_start_time_defined() {
    std::chrono::steady_clock::time_point timestamp_zero{};
    return invalid_node_start_time > timestamp_zero;
}

std::chrono::steady_clock::time_point MONITOR_CONNECTION_CONTEXT::get_invalid_node_start_time() {
    return invalid_node_start_time;
}

bool MONITOR_CONNECTION_CONTEXT::is_node_unhealthy() {
    return node_unhealthy;
}

void MONITOR_CONNECTION_CONTEXT::set_node_unhealthy(bool node) {
    node_unhealthy = node;
}

bool MONITOR_CONNECTION_CONTEXT::is_active_context() {
    return active_context.load();
}

void MONITOR_CONNECTION_CONTEXT::invalidate() {
    active_context.store(false);
}

DBC* MONITOR_CONNECTION_CONTEXT::get_connection_to_abort() {
    return connection_to_abort;
}

unsigned long MONITOR_CONNECTION_CONTEXT::get_dbc_id() {
    return connection_to_abort ? connection_to_abort->id : 0;
}

// Update whether the connection is still valid if the total elapsed time has passed the grace period.
void MONITOR_CONNECTION_CONTEXT::update_connection_status(
    std::chrono::steady_clock::time_point status_check_start_time,
    std::chrono::steady_clock::time_point current_time,
    bool is_valid) {
    
    if (!is_active_context()) {
      return;
    }

    auto total_elapsed_time = current_time - get_start_monitor_time();

    if (total_elapsed_time > get_failure_detection_time()) {
      set_connection_valid(is_valid, status_check_start_time, current_time);
    }
}

// Set whether the connection to the server is still valid based on the monitoring settings.
void MONITOR_CONNECTION_CONTEXT::set_connection_valid(
    bool connection_valid,
    std::chrono::steady_clock::time_point status_check_start_time,
    std::chrono::steady_clock::time_point current_time) {

    const auto node_keys_str = build_node_keys_str();
    if (!connection_valid) {
        increment_failure_count();

        if (!is_invalid_node_start_time_defined()) {
            set_invalid_node_start_time(status_check_start_time);
        }

        const auto invalid_node_duration_ms = std::chrono::duration_cast<std::chrono::milliseconds>(current_time - get_invalid_node_start_time());

        const auto max_invalid_node_duration = get_failure_detection_interval() * (std::max)(0, get_failure_detection_count());

        if (invalid_node_duration_ms >= max_invalid_node_duration) {
            MYLOG_TRACE(logger.get(), get_dbc_id(), "[MONITOR_CONNECTION_CONTEXT] Node '%s' is *dead*.", node_keys_str.c_str());
            set_node_unhealthy(true);
            abort_connection();
            return;
        }

        MYLOG_TRACE(
            logger.get(), get_dbc_id(),
            "[MONITOR_CONNECTION_CONTEXT] Node '%s' is *not responding* (%d).", node_keys_str.c_str(), get_failure_count());
        return;
    }

    set_failure_count(0);
    reset_invalid_node_start_time();
    set_node_unhealthy(false);
    MYLOG_TRACE(logger.get(), get_dbc_id(), "[MONITOR_CONNECTION_CONTEXT] Node '%s' is *alive*.", node_keys_str.c_str());
}

void MONITOR_CONNECTION_CONTEXT::abort_connection() {
    std::lock_guard<std::mutex> lock(mutex_);
    if ((!get_connection_to_abort()) || (!is_active_context())) {
        return;
    }
    connection_to_abort->mysql_proxy->close_socket();
}

std::string MONITOR_CONNECTION_CONTEXT::build_node_keys_str() {
    if (node_keys.empty()) {
        return "[]";
    }

    auto it = node_keys.begin();
    std::string node_keys_str = '[' +(*it);
    ++it;
    while (it != node_keys.end()) {
        node_keys_str += ", " + *it;
        ++it;
    }
    node_keys_str += ']';

    return node_keys_str;
}
