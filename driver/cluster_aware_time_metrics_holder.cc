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

#include "cluster_aware_time_metrics_holder.h"

CLUSTER_AWARE_TIME_METRICS_HOLDER::CLUSTER_AWARE_TIME_METRICS_HOLDER(std::string metric_name):metric_name{metric_name} {}

CLUSTER_AWARE_TIME_METRICS_HOLDER::~CLUSTER_AWARE_TIME_METRICS_HOLDER() {}

std::string CLUSTER_AWARE_TIME_METRICS_HOLDER::report_metrics() {
    std::string log_message = "";

    log_message.append("\n\n** Performance Metrics Report for '").append(metric_name).append("' **");
    if (number_of_queries_issued > 0) {
      log_message.append("\nLongest reported time: ").append(std::to_string(longest_query_time_ms)).append(" ms");
      log_message.append("\nShortest reported time: ").append(std::to_string(shortest_query_time_ms)).append(" ms");
      double avg_time = total_query_time_ms / number_of_queries_issued;
      log_message.append("\nAverage query execution time: ").append(std::to_string(avg_time)).append(" ms");
    }
    log_message.append("\nNumber of reports: ").append(std::to_string(number_of_queries_issued));

    if (number_of_queries_issued > 0 && perf_metrics_hist_breakpoints) {
      log_message.append("\n\n\tTiming Histogram:\n");
      int max_num_points = 20;
      int highest_count = INT_MIN;

      for (int i = 0; i < (HISTOGRAM_BUCKETS); i++) {
        if (perf_metrics_hist_counts[i] > highest_count) {
          highest_count = perf_metrics_hist_counts[i];
        }
      }

      if (highest_count == 0) {
        highest_count = 1; // avoid DIV/0
      }

      for (int i = 0; i < (HISTOGRAM_BUCKETS - 1); i++) {

        if (i == 0) {
          log_message.append("\n\tless than ")
                  .append(std::to_string(perf_metrics_hist_breakpoints[i + 1]))
                  .append(" ms: \t")
                  .append(std::to_string(perf_metrics_hist_counts[i]));
        } else {
          log_message.append("\n\tbetween ")
                  .append(std::to_string(perf_metrics_hist_breakpoints[i]))
                  .append(" and ")
                  .append(std::to_string(perf_metrics_hist_breakpoints[i + 1]))
                  .append(" ms: \t")
                  .append(std::to_string(perf_metrics_hist_counts[i]));
        }

        log_message.append("\t");

        int numPointsToGraph =
            (int) (max_num_points * ((double) perf_metrics_hist_counts[i] / highest_count));

        for (int j = 0; j < numPointsToGraph; j++) {
          log_message.append("*");
        }

        if (longest_query_time_ms < perf_metrics_hist_counts[i + 1]) {
          break;
        }
      }

      if (perf_metrics_hist_breakpoints[HISTOGRAM_BUCKETS - 2] < longest_query_time_ms) {
        log_message.append("\n\tbetween ");
        log_message.append(std::to_string(perf_metrics_hist_breakpoints[HISTOGRAM_BUCKETS - 2]));
        log_message.append(" and ");
        log_message.append(std::to_string(perf_metrics_hist_breakpoints[HISTOGRAM_BUCKETS - 1]));
        log_message.append(" ms: \t");
        log_message.append(std::to_string(perf_metrics_hist_counts[HISTOGRAM_BUCKETS - 1]));
      }
    }

    return log_message;
}

void CLUSTER_AWARE_TIME_METRICS_HOLDER::register_query_execution_time(long long query_time_ms) {
	BASE_METRICS_HOLDER::register_query_execution_time(query_time_ms);
}
