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

#include "mylog.h"

#include <cstdarg>
#include <ctime>
#include <vector>

#include "driver.h"

#ifdef _WIN32
#include <process.h>
#else
#include <unistd.h>
#endif

void trace_print(FILE *file, unsigned long dbc_id, const char *fmt, ...) {
  if (file && fmt) {
    time_t now = time(nullptr);
    char time_buf[256];
    strftime(time_buf, sizeof(time_buf), "%Y/%m/%d %X ", localtime(&now));

    va_list args1;
    va_start(args1, fmt);
    va_list args2;
    va_copy(args2, args1);
    std::vector<char> buf(1 + vsnprintf(nullptr, 0, fmt, args1));
    va_end(args1);
    vsnprintf(buf.data(), buf.size(), fmt, args2);
    va_end(args2);

#ifdef _WIN32
    int pid;
    pid = _getpid();
#else
    pid_t pid;
    pid = getpid();
#endif

    fprintf(file, "%s - Process ID %ld -  DBC ID %lu - %s\n", time_buf, pid,
            dbc_id, buf.data());
    fflush(file);
  }
}

std::shared_ptr<FILE> init_log_file() {
  if (log_file) 
    return log_file;

  std::lock_guard<std::mutex> guard(log_file_mutex);
  if (log_file) 
    return log_file;

  FILE *file;
#ifdef _WIN32
  char filename[MAX_PATH];
  size_t buffsize;

  getenv_s(&buffsize, filename, sizeof(filename), "TEMP");

  if (buffsize) {
    snprintf(filename + buffsize - 1, sizeof(filename) - buffsize + 1, "\\%s", DRIVER_LOG_FILE);
  } else {
    snprintf(filename, sizeof(filename), "c:\\%s", DRIVER_LOG_FILE);
  }

  if (file = fopen(filename, "a+"))
#else
  if (file = fopen(DRIVER_LOG_FILE, "a+"))
#endif
  {
    fprintf(file, "-- Driver logging\n");
    fprintf(file, "--\n");
    fprintf(file, "--  Driver name: %s  Version: %s\n", DRIVER_NAME,
            DRIVER_VERSION);
#ifdef HAVE_LOCALTIME_R
    {
      time_t now = time(nullptr);
      struct tm start;
      localtime_r(&now, &start);

      fprintf(file, "-- Timestamp: %02d%02d%02d %2d:%02d:%02d\n",
              start.tm_year % 100, start.tm_mon + 1, start.tm_mday,
              start.tm_hour, start.tm_min, start.tm_sec);
    }
#endif /* HAVE_LOCALTIME_R */
    fprintf(file, "\n");
    log_file = std::shared_ptr<FILE>(file, FILEDeleter());
  }
  return log_file;
}

void end_log_file() {
  std::lock_guard<std::mutex> guard(log_file_mutex);
  if (log_file && log_file.use_count() == 1) { // static var
    log_file.reset();
  }
}
