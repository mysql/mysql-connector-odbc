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

#ifndef __TESTUTILS_H__
#define __TESTUTILS_H__

#include "driver/driver.h"

void allocate_odbc_handles(SQLHENV& env, DBC*& dbc, DataSource*& ds);
void cleanup_odbc_handles(SQLHENV& env, DBC*& dbc, DataSource*& ds, bool call_myodbc_end = false);

class TEST_UTILS {
public:
    static std::chrono::milliseconds get_connection_check_interval(std::shared_ptr<MONITOR> monitor);
    static CONNECTION_STATUS check_connection_status(std::shared_ptr<MONITOR> monitor);
    static void populate_monitor_map(std::shared_ptr<MONITOR_THREAD_CONTAINER> container,
        std::set<std::string> node_keys, std::shared_ptr<MONITOR> monitor);
    static void populate_task_map(std::shared_ptr<MONITOR_THREAD_CONTAINER> container,
        std::shared_ptr<MONITOR> monitor);
    static bool has_monitor(std::shared_ptr<MONITOR_THREAD_CONTAINER> container, std::string node_key);
    static bool has_task(std::shared_ptr<MONITOR_THREAD_CONTAINER> container, std::shared_ptr<MONITOR> monitor);
    static bool has_any_tasks(std::shared_ptr<MONITOR_THREAD_CONTAINER> container);
    static bool has_available_monitor(std::shared_ptr<MONITOR_THREAD_CONTAINER> container);
    static std::shared_ptr<MONITOR> get_available_monitor(std::shared_ptr<MONITOR_THREAD_CONTAINER> container);
    static size_t get_map_size(std::shared_ptr<MONITOR_THREAD_CONTAINER> container);
    static std::list<std::shared_ptr<MONITOR_CONNECTION_CONTEXT>> get_contexts(std::shared_ptr<MONITOR> monitor);
};

#endif /* __TESTUTILS_H__ */
