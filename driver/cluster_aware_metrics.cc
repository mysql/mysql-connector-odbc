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

#include "cluster_aware_metrics.h"

CLUSTER_AWARE_METRICS::CLUSTER_AWARE_METRICS() {}

CLUSTER_AWARE_METRICS::~CLUSTER_AWARE_METRICS() {}

void CLUSTER_AWARE_METRICS::register_failure_detection_time(long long time_ms) {
	failure_detection->register_query_execution_time(time_ms);
}

void CLUSTER_AWARE_METRICS::register_writer_failover_procedure_time(long long time_ms) {
	writer_failover_procedure->register_query_execution_time(time_ms);
}

void CLUSTER_AWARE_METRICS::register_reader_failover_procedure_time(long long time_ms) {
	reader_failover_procedure->register_query_execution_time(time_ms);
}

void CLUSTER_AWARE_METRICS::register_topology_query_time(long long time_ms) {
	topology_query->register_query_execution_time(time_ms);
}

void CLUSTER_AWARE_METRICS::register_failover_connects(bool is_hit) {
	failover_connects->register_metrics(is_hit);
}

void CLUSTER_AWARE_METRICS::register_invalid_initial_connection(bool is_hit) {
	invalid_initial_connection->register_metrics(is_hit);
}

void CLUSTER_AWARE_METRICS::register_use_cached_topology(bool is_hit) {
	use_cached_topology->register_metrics(is_hit);
}

std::string CLUSTER_AWARE_METRICS::report_metrics() {
	std::string log_message = "";
	log_message.append(failover_connects->report_metrics());
	log_message.append(failure_detection->report_metrics());
	log_message.append(writer_failover_procedure->report_metrics());
	log_message.append(reader_failover_procedure->report_metrics());
	log_message.append(topology_query->report_metrics());
	log_message.append(use_cached_topology->report_metrics());
	log_message.append(invalid_initial_connection->report_metrics());
	return log_message;
}
