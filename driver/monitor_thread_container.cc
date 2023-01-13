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

#include "monitor_thread_container.h"

std::shared_ptr<MONITOR_THREAD_CONTAINER> MONITOR_THREAD_CONTAINER::get_instance() {
    if (singleton) {
        return singleton;
    }

    std::lock_guard<std::mutex> guard(thread_container_singleton_mutex);
    if (singleton) {
        return singleton;
    }

    singleton = std::shared_ptr<MONITOR_THREAD_CONTAINER>(new MONITOR_THREAD_CONTAINER);
    return singleton;
}

void MONITOR_THREAD_CONTAINER::release_instance() {
    if (!singleton) {
        return;
    }

    std::lock_guard<std::mutex> guard(thread_container_singleton_mutex);
    singleton->release_resources();
    singleton.reset();
}

std::string MONITOR_THREAD_CONTAINER::get_node(std::set<std::string> node_keys) {
    std::unique_lock<std::mutex> lock(monitor_map_mutex);
    if (!this->monitor_map.empty()) {
        for (auto it = node_keys.begin(); it != node_keys.end(); ++it) {
            std::string node = *it;
            if (this->monitor_map.count(node) > 0) {
                return node;
            }
        }
    }

    return std::string{};
}

std::shared_ptr<MONITOR> MONITOR_THREAD_CONTAINER::get_monitor(std::string node) {
    std::unique_lock<std::mutex> lock(monitor_map_mutex);
    return this->monitor_map.count(node) > 0 ? this->monitor_map.at(node) : nullptr;
}

std::shared_ptr<MONITOR> MONITOR_THREAD_CONTAINER::get_or_create_monitor(
    std::set<std::string> node_keys,
    std::shared_ptr<HOST_INFO> host,
    std::chrono::seconds failure_detection_timeout,
    std::chrono::milliseconds disposal_time,
    DataSource* ds,
    bool enable_logging) {

    std::shared_ptr<MONITOR> monitor;

    std::unique_lock<std::mutex> lock(mutex_);
    std::string node = this->get_node(node_keys);
    if (!node.empty()) {
        std::unique_lock<std::mutex> lock(monitor_map_mutex);
        monitor = this->monitor_map[node];
    }
    else {
        monitor = this->get_available_monitor();
        if (monitor == nullptr) {
            monitor = this->create_monitor(std::move(host), failure_detection_timeout, disposal_time, ds, enable_logging);
        }
    }

    this->populate_monitor_map(node_keys, monitor);

    return monitor;
}

void MONITOR_THREAD_CONTAINER::add_task(const std::shared_ptr<MONITOR>& monitor, const std::shared_ptr<MONITOR_SERVICE>& service) {
    if (monitor == nullptr || service == nullptr) {
        throw std::invalid_argument("Invalid parameters passed into MONITOR_THREAD_CONTAINER::add_task()");
    }

    std::unique_lock<std::mutex> lock(task_map_mutex);
    if (this->task_map.count(monitor) == 0) {
        this->thread_pool.resize(this->thread_pool.size() + 1);
        auto run_monitor = [monitor, service](int id) { monitor->run(service); };
        this->task_map[monitor] = this->thread_pool.push(run_monitor);
    }
}

void MONITOR_THREAD_CONTAINER::reset_resource(const std::shared_ptr<MONITOR>& monitor) {
    if (monitor == nullptr) {
        return;
    }

    this->remove_monitor_mapping(monitor);

    std::unique_lock<std::mutex> lock(available_monitors_mutex);
    this->available_monitors.push(monitor);
}

void MONITOR_THREAD_CONTAINER::release_resource(std::shared_ptr<MONITOR> monitor) {
    if (monitor == nullptr) {
        return;
    }

    this->remove_monitor_mapping(monitor);

    {
        std::unique_lock<std::mutex> lock(task_map_mutex);
        if (this->task_map.count(monitor) > 0) {
            this->task_map.erase(monitor);
        }
    }

    if (this->thread_pool.n_idle() > 0) {
        this->thread_pool.resize(this->thread_pool.size() - 1);
    }
}

void MONITOR_THREAD_CONTAINER::populate_monitor_map(
    std::set<std::string> node_keys, const std::shared_ptr<MONITOR>& monitor) {

    for (auto it = node_keys.begin(); it != node_keys.end(); ++it) {
        std::unique_lock<std::mutex> lock(monitor_map_mutex);
        this->monitor_map[*it] = monitor;
    }
}

void MONITOR_THREAD_CONTAINER::remove_monitor_mapping(const std::shared_ptr<MONITOR>& monitor) {
    std::unique_lock<std::mutex> lock(monitor_map_mutex);
    for (auto it = this->monitor_map.begin(); it != this->monitor_map.end();) {
        std::string node = (*it).first;
        if (this->monitor_map[node] == monitor) {
            it = this->monitor_map.erase(it);
        }
        else {
            ++it;
        }
    }
}

std::shared_ptr<MONITOR> MONITOR_THREAD_CONTAINER::get_available_monitor() {
    std::unique_lock<std::mutex> lock(available_monitors_mutex);
    if (!this->available_monitors.empty()) {
        std::shared_ptr<MONITOR> available_monitor = this->available_monitors.front();
        this->available_monitors.pop();
        
        if (!available_monitor->is_stopped()) {
            return available_monitor;
        }

        std::unique_lock<std::mutex> lock(task_map_mutex);
        if (this->task_map.count(available_monitor) > 0) {
            available_monitor->stop();
            this->task_map.erase(available_monitor);
        }
    }

    return nullptr;
}

std::shared_ptr<MONITOR> MONITOR_THREAD_CONTAINER::create_monitor(
    std::shared_ptr<HOST_INFO> host,
    std::chrono::seconds failure_detection_timeout,
    std::chrono::milliseconds disposal_time,
    DataSource* ds,
    bool enable_logging) {

    return std::make_shared<MONITOR>(host, failure_detection_timeout, disposal_time, ds, enable_logging);
}

void MONITOR_THREAD_CONTAINER::release_resources() {
    // Stop all monitors
    {
        std::unique_lock<std::mutex> lock(task_map_mutex);
        for (auto const& task_pair : task_map) {
            auto monitor = task_pair.first;
            monitor->stop();
        }
    }

    // Wait for monitor threads to finish
    this->thread_pool.stop(true);

    {
        std::unique_lock<std::mutex> lock(monitor_map_mutex);
        this->monitor_map.clear();
    }

    {
        std::unique_lock<std::mutex> lock(task_map_mutex);
        this->task_map.clear();
    }

    {
        std::unique_lock<std::mutex> lock(available_monitors_mutex);
        std::queue<std::shared_ptr<MONITOR>> empty;
        std::swap(available_monitors, empty);
    }
}
