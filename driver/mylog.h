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

#ifndef __MYLOG_H__
#define __MYLOG_H__

#include <cstdio>
#include <memory>
#include <mutex>
#include <string>

#define MYLOG_STMT_TRACE(A, B)                                              \
  {                                                                         \
    if ((A)->dbc->ds->save_queries)                                         \
      trace_print((A)->dbc->log_file.get(), (A)->dbc->id, (const char *)B); \
  }

#define MYLOG_DBC_TRACE(A, ...) \
  { trace_print((A)->log_file.get(), (A)->id, __VA_ARGS__); }

#define MYLOG_TRACE(A, B, ...)                            \
  {                                                       \
    if ((A) != nullptr) trace_print((A), B, __VA_ARGS__); \
  }

// stateless functor object for deleting FILE handle
struct FILEDeleter {
  void operator()(FILE *file) {
    if (file) {
      fclose(file);
      file = nullptr;
    }
  }
};

static std::shared_ptr<FILE> log_file;
static std::mutex log_file_mutex;

/* Functions used when debugging */
std::shared_ptr<FILE> init_log_file();
void end_log_file();
void trace_print(FILE *file, unsigned long dbc_id, const char *fmt, ...);

#endif /* __MYLOG_H__ */
