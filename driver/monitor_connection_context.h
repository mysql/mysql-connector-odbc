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

#ifndef __MONITORCONNECTIONCONTEXT_H__
#define __MONITORCONNECTIONCONTEXT_H__

#include <atomic>
#include <chrono>
#include <memory>
#include <mutex>
#include <set>
#include <string>

struct DBC;

// Monitoring context for each connection. This contains each connection's criteria for
// whether a server should be considered unhealthy.
class MONITOR_CONNECTION_CONTEXT {
public:
    MONITOR_CONNECTION_CONTEXT(DBC* connection_to_abort,
                               std::set<std::string> node_keys,
                               std::chrono::milliseconds failure_detection_time,
                               std::chrono::milliseconds failure_detection_interval,
                               int failure_detection_count,
                               bool enable_logging = false);
    virtual ~MONITOR_CONNECTION_CONTEXT();

    std::chrono::steady_clock::time_point get_start_monitor_time();
    virtual void set_start_monitor_time(std::chrono::steady_clock::time_point time);
    std::set<std::string> get_node_keys();
    std::chrono::milliseconds get_failure_detection_time();
    std::chrono::milliseconds get_failure_detection_interval();
    int get_failure_detection_count();
    int get_failure_count();
    void set_failure_count(int count);
    void increment_failure_count();
    void set_invalid_node_start_time(std::chrono::steady_clock::time_point time);
    void reset_invalid_node_start_time();
    bool is_invalid_node_start_time_defined();
    std::chrono::steady_clock::time_point get_invalid_node_start_time();
    bool is_node_unhealthy();
    void set_node_unhealthy(bool node);
    bool is_active_context();
    void invalidate();
    DBC* get_connection_to_abort();
    unsigned long get_dbc_id();

    void update_connection_status(
        std::chrono::steady_clock::time_point status_check_start_time,
        std::chrono::steady_clock::time_point current_time,
        bool is_valid);
    void set_connection_valid(
        bool connection_valid,
        std::chrono::steady_clock::time_point status_check_start_time,
        std::chrono::steady_clock::time_point current_time);
    void abort_connection();

private:
    std::mutex mutex_;
    
    std::chrono::milliseconds failure_detection_time;
    std::chrono::milliseconds failure_detection_interval;
    int failure_detection_count;

    std::set<std::string> node_keys;
    DBC* connection_to_abort;

    std::chrono::steady_clock::time_point start_monitor_time;
    std::chrono::steady_clock::time_point invalid_node_start_time;
    int failure_count;
    bool node_unhealthy;
    std::atomic_bool active_context{ true };
    std::shared_ptr<FILE> logger;

    std::string build_node_keys_str();
};

#endif /* __MONITORCONNECTIONCONTEXT_H__ */
