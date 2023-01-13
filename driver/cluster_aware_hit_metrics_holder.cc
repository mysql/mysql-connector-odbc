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

#include "cluster_aware_hit_metrics_holder.h"

CLUSTER_AWARE_HIT_METRICS_HOLDER::CLUSTER_AWARE_HIT_METRICS_HOLDER(std::string metric_name):metric_name{metric_name} {}

CLUSTER_AWARE_HIT_METRICS_HOLDER::~CLUSTER_AWARE_HIT_METRICS_HOLDER() {}

void CLUSTER_AWARE_HIT_METRICS_HOLDER::register_metrics(bool is_hit) {
    number_of_reports++;
    if (is_hit) {
        number_of_hits++;
    }
}

std::string CLUSTER_AWARE_HIT_METRICS_HOLDER::report_metrics() {
    std::string log_message = "";

    log_message.append("\n\n** Performance Metrics Report for '");
    log_message.append(metric_name);
    log_message.append("' **");
    log_message.append("\nNumber of reports: ").append(std::to_string(number_of_reports));
    if (number_of_reports > 0) {
      log_message.append("\nNumber of hits: ").append(std::to_string(number_of_hits));
      log_message.append("\nRatio : ").append(std::to_string(number_of_hits * 100.0 / number_of_reports)).append(" %");
    }

    return log_message;
}
