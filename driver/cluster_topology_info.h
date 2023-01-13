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

#ifndef __CLUSTERTOPOLOGYINFO_H__
#define __CLUSTERTOPOLOGYINFO_H__

#include "host_info.h"

#include <ctime>
#include <set>
#include <vector>

// This class holds topology information for one cluster.
// Cluster topology consists of an instance endpoint, a set of nodes in the cluster,
// the type of each node in the cluster, and the status of each node in the cluster.
class CLUSTER_TOPOLOGY_INFO {
public:
    CLUSTER_TOPOLOGY_INFO();
    CLUSTER_TOPOLOGY_INFO(const CLUSTER_TOPOLOGY_INFO& src_info); //copy constructor
    virtual ~CLUSTER_TOPOLOGY_INFO();

    void add_host(std::shared_ptr<HOST_INFO> host_info);
    size_t total_hosts();
    size_t num_readers(); // return number of readers in the cluster
    std::time_t time_last_updated();

    std::shared_ptr<HOST_INFO> get_writer();
    std::shared_ptr<HOST_INFO> get_next_reader();
    // TODO - Ponder if the get_reader below is needed. In general user of this should not need to deal with indexes.
    // One case that comes to mind, if we were to try to do a random shuffle of readers or hosts in general like JDBC driver
    // we could do random shuffle of host indices and call the get_reader for specific index in order we wanted.
    std::shared_ptr<HOST_INFO> get_reader(int i);
    std::vector<std::shared_ptr<HOST_INFO>> get_writers();
    std::vector<std::shared_ptr<HOST_INFO>> get_readers();
    bool is_multi_writer_cluster = false;

private:
    int current_reader = -1;
    std::time_t last_updated;
    std::set<std::string> down_hosts; // maybe not needed, HOST_INFO has is_host_down() method
    //std::vector<HOST_INFO*> hosts;
    std::shared_ptr<HOST_INFO> last_used_reader;  // TODO perhaps this overlaps with current_reader and is not needed

    // TODO - can we do without pointers -
    // perhaps ok for now, we are using copies CLUSTER_TOPOLOGY_INFO returned by get_topology and get_cached_topology from TOPOLOGY_SERVICE.
    // However, perhaps smart shared pointers could be used.
    std::vector<std::shared_ptr<HOST_INFO>> writers;
    std::vector<std::shared_ptr<HOST_INFO>> readers;

    std::shared_ptr<HOST_INFO> get_last_used_reader();
    void set_last_used_reader(std::shared_ptr<HOST_INFO> reader);
    void mark_host_down(std::shared_ptr<HOST_INFO> host);
    void mark_host_up(std::shared_ptr<HOST_INFO> host);
    std::set<std::string> get_down_hosts();
    void update_time();

    friend class TOPOLOGY_SERVICE;
};

#endif /* __CLUSTERTOPOLOGYINFO_H__ */
