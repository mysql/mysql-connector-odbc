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

#include "cluster_topology_info.h"

#include <stdexcept>

/**
  Initialize and return random number.

  Returns random number.
 */
int get_random_number() {
    std::srand((unsigned int)time(nullptr));
    return rand();
}

CLUSTER_TOPOLOGY_INFO::CLUSTER_TOPOLOGY_INFO() {
    update_time();
}

// copy constructor
CLUSTER_TOPOLOGY_INFO::CLUSTER_TOPOLOGY_INFO(const CLUSTER_TOPOLOGY_INFO& src_info)
    : current_reader{src_info.current_reader},
      last_updated{src_info.last_updated},
      last_used_reader{src_info.last_used_reader},
      is_multi_writer_cluster{src_info.is_multi_writer_cluster} {
    for (auto host_info_source : src_info.writers) {
        writers.push_back(std::make_shared<HOST_INFO>(*host_info_source)); //default copy
    }
    for (auto host_info_source : src_info.readers) {
        readers.push_back(std::make_shared<HOST_INFO>(*host_info_source)); //default copy
    }
}

CLUSTER_TOPOLOGY_INFO::~CLUSTER_TOPOLOGY_INFO() {
    for (auto p : writers) {
        p.reset();
    }
    writers.clear();

    for (auto p : readers) {
        p.reset();
    }
    readers.clear();
}

void CLUSTER_TOPOLOGY_INFO::add_host(std::shared_ptr<HOST_INFO> host_info) {
    host_info->is_host_writer() ? writers.push_back(host_info) : readers.push_back(host_info);
    update_time();
}

size_t CLUSTER_TOPOLOGY_INFO::total_hosts() {
    return writers.size() + readers.size();
}

size_t CLUSTER_TOPOLOGY_INFO::num_readers() {
    return readers.size();
}

std::time_t CLUSTER_TOPOLOGY_INFO::time_last_updated() {
    return last_updated;
}

// TODO harmonize time function across objects so the times are comparable
void CLUSTER_TOPOLOGY_INFO::update_time() {
    last_updated = time(0);
}

std::shared_ptr<HOST_INFO> CLUSTER_TOPOLOGY_INFO::get_writer() {
    if (writers.size() <= 0) {
        throw std::runtime_error("No writer available");
    }

    return writers[0];
}

std::shared_ptr<HOST_INFO> CLUSTER_TOPOLOGY_INFO::get_next_reader() {
    size_t num_readers = readers.size();
    if (num_readers <= 0) {
        throw std::runtime_error("No reader available");
    }

    if (current_reader == -1) { // initialize for the first time
        current_reader = get_random_number() % num_readers;
    }
    else if (current_reader >= num_readers) {
        // adjust current reader in case topology was refreshed.
        current_reader = (current_reader) % num_readers;
    }
    else {
        current_reader = (current_reader + 1) % num_readers;
    }

    return readers[current_reader];
}

std::shared_ptr<HOST_INFO> CLUSTER_TOPOLOGY_INFO::get_reader(int i) {
    if (i < 0 || i >= readers.size()) {
        throw std::runtime_error("No reader available at index " + i);
    }

    return readers[i];
}

std::vector<std::shared_ptr<HOST_INFO>> CLUSTER_TOPOLOGY_INFO::get_readers() {
    return readers;
}

std::vector<std::shared_ptr<HOST_INFO>> CLUSTER_TOPOLOGY_INFO::get_writers() {
    return writers;
}

std::shared_ptr<HOST_INFO> CLUSTER_TOPOLOGY_INFO::get_last_used_reader() {
    return last_used_reader;
}

void CLUSTER_TOPOLOGY_INFO::set_last_used_reader(std::shared_ptr<HOST_INFO> reader) {
    last_used_reader = reader;
}

void CLUSTER_TOPOLOGY_INFO::mark_host_down(std::shared_ptr<HOST_INFO> host) {
    host->set_host_state(DOWN);
    down_hosts.insert(host->get_host_port_pair());
}

void CLUSTER_TOPOLOGY_INFO::mark_host_up(std::shared_ptr<HOST_INFO> host) {
    host->set_host_state(UP);
    down_hosts.erase(host->get_host_port_pair());
}

std::set<std::string> CLUSTER_TOPOLOGY_INFO::get_down_hosts() {
    return down_hosts;
}
