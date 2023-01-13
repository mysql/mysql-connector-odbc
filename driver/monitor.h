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

#ifndef __MONITOR_H__
#define __MONITOR_H__

#include "host_info.h"
#include "monitor_connection_context.h"

#include <atomic>
#include <list>

struct CONNECTION_STATUS {
    bool is_valid;
    std::chrono::milliseconds elapsed_time;
};

struct DataSource;
class MONITOR_SERVICE;
class MYSQL_MONITOR_PROXY;


namespace {
    const std::chrono::milliseconds thread_sleep_when_inactive = std::chrono::milliseconds(100);
    const unsigned int failure_detection_timeout_default = 5;
}

class MONITOR : public std::enable_shared_from_this<MONITOR> {
public:
    MONITOR(
        std::shared_ptr<HOST_INFO> host_info,
        std::chrono::seconds failure_detection_timeout,
        std::chrono::milliseconds monitor_disposal_time,
        DataSource* ds,
        bool enable_logging = false);
    MONITOR(
        std::shared_ptr<HOST_INFO> host_info,
        std::chrono::seconds failure_detection_timeout,
        std::chrono::milliseconds monitor_disposal_time,
        MYSQL_MONITOR_PROXY* proxy,
        bool enable_logging = false);
    virtual ~MONITOR();

    virtual void start_monitoring(std::shared_ptr<MONITOR_CONNECTION_CONTEXT> context);
    virtual void stop_monitoring(std::shared_ptr<MONITOR_CONNECTION_CONTEXT> context);
    virtual bool is_stopped();
    virtual void clear_contexts();
    virtual void run(std::shared_ptr<MONITOR_SERVICE> service);
    void stop();

private:
    std::atomic_bool stopped{true};
    std::shared_ptr<HOST_INFO> host;
    std::chrono::milliseconds connection_check_interval;
    std::chrono::seconds failure_detection_timeout;
    std::chrono::milliseconds disposal_time;
    std::list<std::shared_ptr<MONITOR_CONNECTION_CONTEXT>> contexts;
    std::chrono::steady_clock::time_point last_context_timestamp;
    MYSQL_MONITOR_PROXY* mysql_proxy = nullptr;
    std::shared_ptr<FILE> logger;
    std::mutex mutex_;

    std::chrono::milliseconds get_connection_check_interval();
    CONNECTION_STATUS check_connection_status();
    bool connect();
    std::chrono::milliseconds find_shortest_interval();
    virtual std::chrono::steady_clock::time_point get_current_time();

#ifdef UNIT_TEST_BUILD
    // Allows for testing private methods
    friend class TEST_UTILS;
#endif
};

#endif /* __MONITOR_H__ */
