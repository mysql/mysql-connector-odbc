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

#include "monitor.h"
#include "monitor_service.h"
#include "mylog.h"
#include "mysql_proxy.h"

MONITOR::MONITOR(
    std::shared_ptr<HOST_INFO> host_info,
    std::chrono::seconds failure_detection_timeout,
    std::chrono::milliseconds monitor_disposal_time,
    DataSource* ds,
    bool enable_logging)
    : MONITOR(
        std::move(host_info),
        failure_detection_timeout,
        monitor_disposal_time,
        new MYSQL_MONITOR_PROXY(ds),
        enable_logging) {};

MONITOR::MONITOR(
    std::shared_ptr<HOST_INFO> host_info,
    std::chrono::seconds failure_detection_timeout,
    std::chrono::milliseconds monitor_disposal_time,
    MYSQL_MONITOR_PROXY* proxy,
    bool enable_logging) {

    this->host = std::move(host_info);
    this->failure_detection_timeout = failure_detection_timeout;
    this->disposal_time = monitor_disposal_time;
    this->mysql_proxy = proxy;
    this->connection_check_interval = (std::chrono::milliseconds::max)();
    if (enable_logging)
        this->logger = init_log_file();
}

MONITOR::~MONITOR() {
    if (this->mysql_proxy) {
        delete this->mysql_proxy;
        this->mysql_proxy = nullptr;
    }
}

void MONITOR::start_monitoring(std::shared_ptr<MONITOR_CONNECTION_CONTEXT> context) {
    std::chrono::milliseconds detection_interval = context->get_failure_detection_interval();
    if (detection_interval < this->connection_check_interval) {
        this->connection_check_interval = detection_interval;
    }

    auto current_time = get_current_time();
    context->set_start_monitor_time(current_time);
    this->last_context_timestamp = current_time;
    
    {
        std::unique_lock<std::mutex> lock(mutex_);
        this->contexts.push_back(context);
    }
}

void MONITOR::stop_monitoring(std::shared_ptr<MONITOR_CONNECTION_CONTEXT> context) {
    if (context == nullptr) {
        MYLOG_TRACE(
            this->logger.get(), 0,
            "[MONITOR] Invalid context passed into stop_monitoring()");
        return;
    }

    context->invalidate();

    {
        std::unique_lock<std::mutex> lock(mutex_);
        this->contexts.remove(context);
    }

    this->connection_check_interval = this->find_shortest_interval();
}

bool MONITOR::is_stopped() {
    return this->stopped.load(); 
}

void MONITOR::stop() {
    this->stopped.store(true);
}

void MONITOR::clear_contexts() {
    {
        std::unique_lock<std::mutex> lock(mutex_);
        this->contexts.clear();
    }

    this->connection_check_interval = (std::chrono::milliseconds::max)();
}

// Periodically ping the server and update the contexts' connection status.
void MONITOR::run(std::shared_ptr<MONITOR_SERVICE> service) {
    this->stopped = false;
    while (!this->stopped) {
        bool have_contexts;
        {
            std::unique_lock<std::mutex> lock(mutex_);
            have_contexts = !this->contexts.empty();
        }
        if (have_contexts) {
            auto status_check_start_time = this->get_current_time();
            this->last_context_timestamp = status_check_start_time;

            CONNECTION_STATUS status = this->check_connection_status();

            {
                std::unique_lock<std::mutex> lock(mutex_);
                for (auto it = this->contexts.begin(); it != this->contexts.end(); ++it) {
                    std::shared_ptr<MONITOR_CONNECTION_CONTEXT> context = *it;
                    context->update_connection_status(
                        status_check_start_time,
                        status_check_start_time + status.elapsed_time,
                        status.is_valid);
                }
            }

            std::chrono::milliseconds check_interval = this->get_connection_check_interval();
            auto sleep_time = check_interval - status.elapsed_time;
            if (sleep_time > std::chrono::milliseconds(0)) {
                std::this_thread::sleep_for(sleep_time);
            }
        }
        else {
            auto time_inactive = std::chrono::duration_cast<std::chrono::milliseconds>(this->get_current_time() - this->last_context_timestamp);
            if (time_inactive >= this->disposal_time) {
                break;
            }
            std::this_thread::sleep_for(thread_sleep_when_inactive);
        }
    }

    service->notify_unused(shared_from_this());

    this->stopped = true;
}

std::chrono::milliseconds MONITOR::get_connection_check_interval() {
    std::unique_lock<std::mutex> lock(mutex_);
    if (this->contexts.empty()) {
        return std::chrono::milliseconds(0);
    }

    return this->connection_check_interval;
}

CONNECTION_STATUS MONITOR::check_connection_status() {
    if (this->mysql_proxy == nullptr || !this->mysql_proxy->is_connected()) {
        const auto start = this->get_current_time();
        bool connected = this->connect();
        return CONNECTION_STATUS{
            connected,
            std::chrono::duration_cast<std::chrono::milliseconds>(this->get_current_time() - start)
        };
    }

    auto start = this->get_current_time();
    bool is_connection_active = this->mysql_proxy->ping() == 0;
    auto duration = this->get_current_time() - start;
    
    return CONNECTION_STATUS{
        is_connection_active,
        std::chrono::duration_cast<std::chrono::milliseconds>(duration)
    };
}

bool MONITOR::connect() {
    this->mysql_proxy->close();
    this->mysql_proxy->init();

    // Timeout shouldn't be 0 by now, but double check just in case
    unsigned int timeout_sec = this->failure_detection_timeout.count() == 0 ? failure_detection_timeout_default : this->failure_detection_timeout.count();
    this->mysql_proxy->options(MYSQL_OPT_CONNECT_TIMEOUT, &timeout_sec);
    this->mysql_proxy->options(MYSQL_OPT_READ_TIMEOUT, &timeout_sec);

    if (!this->mysql_proxy->connect()) {
        MYLOG_TRACE(this->logger.get(), 0, this->mysql_proxy->error());
        this->mysql_proxy->close();
        return false;
    }

    return true;
}

std::chrono::milliseconds MONITOR::find_shortest_interval() {
    auto min = (std::chrono::milliseconds::max)();

    {
        std::unique_lock<std::mutex> lock(mutex_);
        if (this->contexts.empty()) {
            return min;
        }

        for (auto it = this->contexts.begin(); it != this->contexts.end(); ++it) {
            auto failure_detection_interval = (*it)->get_failure_detection_interval();
            if (failure_detection_interval < min) {
                min = failure_detection_interval;
            }
        }
    }

    return min;
}

std::chrono::steady_clock::time_point MONITOR::get_current_time() {
    return std::chrono::steady_clock::now();
}
