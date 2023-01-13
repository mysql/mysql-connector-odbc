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

#include "monitor_service.h"

#include "driver.h"

MONITOR_SERVICE::MONITOR_SERVICE(bool enable_logging) {
    this->thread_container = MONITOR_THREAD_CONTAINER::get_instance();
    if (enable_logging)
        this->logger = init_log_file();
}

MONITOR_SERVICE::MONITOR_SERVICE(
    std::shared_ptr<MONITOR_THREAD_CONTAINER> monitor_thread_container, bool enable_logging)
    : thread_container{std::move(monitor_thread_container)} {
 
    if (enable_logging)
        this->logger = init_log_file();
}

std::shared_ptr<MONITOR_CONNECTION_CONTEXT> MONITOR_SERVICE::start_monitoring(
    DBC* dbc,
    DataSource* ds,
    std::set<std::string> node_keys,
    std::shared_ptr<HOST_INFO> host,
    std::chrono::milliseconds failure_detection_time,
    std::chrono::seconds failure_detection_timeout,
    std::chrono::milliseconds failure_detection_interval,
    int failure_detection_count,
    std::chrono::milliseconds disposal_time) {

    if (node_keys.empty()) {
        auto msg = "[MONITOR_SERVICE] Parameter node_keys cannot be empty";
        MYLOG_TRACE(this->logger.get(), dbc ? dbc->id : 0, msg);
        throw std::invalid_argument(msg);
    }

    bool enable_logging = ds && ds->save_queries;

    std::shared_ptr<MONITOR> monitor = this->thread_container->get_or_create_monitor(
        node_keys,
        std::move(host),
        failure_detection_timeout,
        disposal_time,
        ds,
        enable_logging);

    auto context = std::make_shared<MONITOR_CONNECTION_CONTEXT>(
        dbc,
        node_keys,
        failure_detection_time,
        failure_detection_interval,
        failure_detection_count,
        enable_logging);

    monitor->start_monitoring(context);
    this->thread_container->add_task(monitor, shared_from_this());

    return context;
}

void MONITOR_SERVICE::stop_monitoring(std::shared_ptr<MONITOR_CONNECTION_CONTEXT> context) {
    if (context == nullptr) {
        MYLOG_TRACE(
            this->logger.get(), 0,
            "[MONITOR_SERVICE] Invalid context passed into stop_monitoring()");
        return;
    }

    context->invalidate();

    std::string node = this->thread_container->get_node(context->get_node_keys());
    if (node.empty()) {
        MYLOG_TRACE(
            this->logger.get(), context->get_dbc_id(),
            "[MONITOR_SERVICE] Can not find node key from context");
        return;
    }

    auto monitor = this->thread_container->get_monitor(node);
    if (monitor != nullptr) {
        monitor->stop_monitoring(context);
    }
}

void MONITOR_SERVICE::stop_monitoring_for_all_connections(std::set<std::string> node_keys) {
    std::string node = this->thread_container->get_node(node_keys);
    if (node.empty()) {
        MYLOG_TRACE(
            this->logger.get(), 0,
            "[MONITOR_SERVICE] Invalid node keys passed into stop_monitoring_for_all_connections(). "
            "No existing monitor for the given set of node keys");
        return;
    }

    auto monitor = this->thread_container->get_monitor(node);
    if (monitor != nullptr) {
        monitor->clear_contexts();
        this->thread_container->reset_resource(monitor);
    }
}

void MONITOR_SERVICE::notify_unused(const std::shared_ptr<MONITOR>& monitor) const {
    if (monitor == nullptr) {
        MYLOG_TRACE(
            this->logger.get(), 0,
            "[MONITOR_SERVICE] Invalid monitor passed into notify_unused()");
        return;
    }

    // Remove monitor from the maps
    this->thread_container->release_resource(monitor);
}

void MONITOR_SERVICE::release_resources() {
    this->thread_container = nullptr;
    MONITOR_THREAD_CONTAINER::release_instance();
}
