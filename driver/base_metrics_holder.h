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

#ifndef __BASEMETRICSHOLDER_H__
#define __BASEMETRICSHOLDER_H__

#include <climits>
#include <cmath>
#include <cstring>
#include <vector>

#include "mylog.h"

class BASE_METRICS_HOLDER {
public:
    BASE_METRICS_HOLDER();
    ~BASE_METRICS_HOLDER();

    virtual void register_query_execution_time(long long query_time_ms);
    virtual std::string report_metrics();

private:
    void create_initial_histogram(long long* breakpoints, long long lower_bound, long long upper_bound);
    void add_to_histogram(
        int* histogram_counts,
        long long* histogram_breakpoints,
        long long value,
        int number_of_times,
        long long current_lower_bound,
        long long current_upper_bound);
    void add_to_performance_histogram(long long value, int number_of_times);
    void check_and_create_performance_histogram();
    void repartition_histogram(
        int* hist_counts,
        long long* hist_breakpoints,
        long long current_lower_bound,
        long long current_upper_bound);
    void repartition_performance_histogram();
    
protected:
    const static int HISTOGRAM_BUCKETS = 20;
    long long longest_query_time_ms = 0;
    long number_of_queries_issued = 0;
    long* old_hist_breakpoints = nullptr;
    int* old_hist_counts = nullptr;
    long long shortest_query_time_ms = LONG_MAX;
    double total_query_time_ms = 0;
    long long* perf_metrics_hist_breakpoints = nullptr;
    int* perf_metrics_hist_counts = nullptr;
};

#endif /* __BASEMETRICSHOLDER_H__ */
