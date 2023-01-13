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

#ifndef __MONITORTHREADCONTAINER_H__
#define __MONITORTHREADCONTAINER_H__

#include "monitor.h"

#include <ctpl_stl.h>
#include <future>
#include <map>
#include <queue>

class MONITOR_THREAD_CONTAINER {
public:
    MONITOR_THREAD_CONTAINER(MONITOR_THREAD_CONTAINER const&) = delete;
    MONITOR_THREAD_CONTAINER& operator=(MONITOR_THREAD_CONTAINER const&) = delete;
    virtual ~MONITOR_THREAD_CONTAINER() = default;
    std::string get_node(std::set<std::string> node_keys);
    std::shared_ptr<MONITOR> get_monitor(std::string node);
    std::shared_ptr<MONITOR> get_or_create_monitor(
        std::set<std::string> node_keys,
        std::shared_ptr<HOST_INFO> host,
        std::chrono::seconds failure_detection_timeout,
        std::chrono::milliseconds disposal_time,
        DataSource* ds,
        bool enable_logging = false);
    virtual void add_task(const std::shared_ptr<MONITOR>& monitor, const std::shared_ptr<MONITOR_SERVICE>& service);
    void reset_resource(const std::shared_ptr<MONITOR>& monitor);
    void release_resource(std::shared_ptr<MONITOR> monitor);

    static std::shared_ptr<MONITOR_THREAD_CONTAINER> get_instance();
    static void release_instance();

protected:
    MONITOR_THREAD_CONTAINER() = default;
    void populate_monitor_map(std::set<std::string> node_keys, const std::shared_ptr<MONITOR>& monitor);
    void remove_monitor_mapping(const std::shared_ptr<MONITOR>& monitor);
    std::shared_ptr<MONITOR> get_available_monitor();
    virtual std::shared_ptr<MONITOR> create_monitor(
        std::shared_ptr<HOST_INFO> host,
        std::chrono::seconds failure_detection_timeout,
        std::chrono::milliseconds disposal_time,
        DataSource* ds,
        bool enable_logging = false);
    void release_resources();

    std::map<std::string, std::shared_ptr<MONITOR>> monitor_map;
    std::map<std::shared_ptr<MONITOR>, std::future<void>> task_map;
    std::queue<std::shared_ptr<MONITOR>> available_monitors;
    std::mutex monitor_map_mutex;
    std::mutex task_map_mutex;
    std::mutex available_monitors_mutex;
    ctpl::thread_pool thread_pool;
    std::mutex mutex_;

#ifdef UNIT_TEST_BUILD
    // Allows for testing private methods
    friend class TEST_UTILS;
#endif
};

static std::shared_ptr<MONITOR_THREAD_CONTAINER> singleton;
static std::mutex thread_container_singleton_mutex;

#endif /* __MONITORTHREADCONTAINER_H__ */
