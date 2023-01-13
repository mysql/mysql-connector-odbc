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

#ifndef __CLUSTERAWAREMETRICS_H__
#define __CLUSTERAWAREMETRICS_H__

#include <memory>

#include "cluster_aware_hit_metrics_holder.h"
#include "cluster_aware_time_metrics_holder.h"

class CLUSTER_AWARE_METRICS {
public:
	CLUSTER_AWARE_METRICS();
	~CLUSTER_AWARE_METRICS();
	void register_failure_detection_time(long long time_ms);
	void register_writer_failover_procedure_time(long long time_ms);
	void register_reader_failover_procedure_time(long long time_ms);
	void register_topology_query_time(long long time_ms);
	void register_failover_connects(bool is_hit);
	void register_invalid_initial_connection(bool is_hit);
	void register_use_cached_topology(bool is_hit);	

 	std::string report_metrics();

private:
	std::shared_ptr<CLUSTER_AWARE_TIME_METRICS_HOLDER> failure_detection = std::make_shared<CLUSTER_AWARE_TIME_METRICS_HOLDER>("Failover Detection");
	std::shared_ptr<CLUSTER_AWARE_TIME_METRICS_HOLDER> writer_failover_procedure = std::make_shared<CLUSTER_AWARE_TIME_METRICS_HOLDER>("Writer Failover Procedure");
	std::shared_ptr<CLUSTER_AWARE_TIME_METRICS_HOLDER> reader_failover_procedure = std::make_shared<CLUSTER_AWARE_TIME_METRICS_HOLDER>("Reader Failover Procedure");
	std::shared_ptr<CLUSTER_AWARE_TIME_METRICS_HOLDER> topology_query = std::make_shared<CLUSTER_AWARE_TIME_METRICS_HOLDER>("Topology Query");
	std::shared_ptr<CLUSTER_AWARE_HIT_METRICS_HOLDER> failover_connects = std::make_shared<CLUSTER_AWARE_HIT_METRICS_HOLDER>("Successful Failover Reconnects");
	std::shared_ptr<CLUSTER_AWARE_HIT_METRICS_HOLDER> invalid_initial_connection = std::make_shared<CLUSTER_AWARE_HIT_METRICS_HOLDER>("Invalid Initial Connection");
	std::shared_ptr<CLUSTER_AWARE_HIT_METRICS_HOLDER> use_cached_topology = std::make_shared<CLUSTER_AWARE_HIT_METRICS_HOLDER>("Used Cached Topology");
};

#endif /* __CLUSTERAWAREMETRICS_H__ */
