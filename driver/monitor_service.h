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

#ifndef __MONITORSERVICE_H__
#define __MONITORSERVICE_H__

#include "monitor_thread_container.h"

class MONITOR_SERVICE : public std::enable_shared_from_this<MONITOR_SERVICE> {
public:
    MONITOR_SERVICE(bool enable_logging = false);
    MONITOR_SERVICE(
        std::shared_ptr<MONITOR_THREAD_CONTAINER> monitor_thread_container,
        bool enable_logging = false);
    virtual ~MONITOR_SERVICE() = default;
    
    virtual std::shared_ptr<MONITOR_CONNECTION_CONTEXT> start_monitoring(
        DBC* dbc,
        DataSource* ds,
        std::set<std::string> node_keys,
        std::shared_ptr<HOST_INFO> host,
        std::chrono::milliseconds failure_detection_time,
        std::chrono::seconds failure_detection_timeout,
        std::chrono::milliseconds failure_detection_interval,
        int failure_detection_count,
        std::chrono::milliseconds disposal_time);
    virtual void stop_monitoring(std::shared_ptr<MONITOR_CONNECTION_CONTEXT> context);
    virtual void stop_monitoring_for_all_connections(std::set<std::string> node_keys);
    void notify_unused(const std::shared_ptr<MONITOR>& monitor) const;
    void release_resources();

private:
    std::shared_ptr<MONITOR_THREAD_CONTAINER> thread_container;
    std::shared_ptr<FILE> logger;
};

#endif /* __MONITORSERVICE_H__ */
