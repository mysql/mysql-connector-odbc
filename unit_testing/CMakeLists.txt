# Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License, version 2.0
# (GPLv2), as published by the Free Software Foundation, with the
# following additional permissions:
#
# This program is distributed with certain software that is licensed
# under separate terms, as designated in a particular file or component
# or in the license documentation. Without limiting your rights under
# the GPLv2, the authors of this program hereby grant you an additional
# permission to link the program and your derivative works with the
# separately licensed software that they have included with the program.
#
# Without limiting the foregoing grant of rights under the GPLv2 and
# additional permission as to separately licensed software, this
# program is also subject to the Universal FOSS Exception, version 1.0,
# a copy of which can be found along with its FAQ at
# http://oss.oracle.com/licenses/universal-foss-exception.
#
# This program is distributed in the hope that it will be useful, but
# WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
# See the GNU General Public License, version 2.0, for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program. If not, see 
# http://www.gnu.org/licenses/gpl-2.0.html.

cmake_minimum_required(VERSION 3.14)

project(testing)

set(CMAKE_CXX_STANDARD 14)

include(FetchContent)
FetchContent_Declare(
  googletest
  URL https://github.com/google/googletest/archive/refs/tags/release-1.11.0.zip
)

# For Windows: Prevent overriding the parent project's compiler/linker settings
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)

FetchContent_MakeAvailable(googletest)

enable_testing()

add_executable(
  unit_testing

  mock_objects.h
  test_utils.h
  test_utils.cc

  cluster_aware_metrics_test.cc
  failover_handler_test.cc
  failover_reader_handler_test.cc
  failover_writer_handler_test.cc
  monitor_connection_context_test.cc
  monitor_service_test.cc
  monitor_test.cc
  monitor_thread_container_test.cc
  multi_threaded_monitor_service_test.cc
  mysql_proxy_test.cc
  query_parsing_test.cc
  main.cc
  topology_service_test.cc
)

target_link_libraries(
  unit_testing
  gtest_main
  gmock_main
  myodbc8a
)

include(GoogleTest)
gtest_discover_tests(unit_testing PROPERTIES DISCOVERY_TIMEOUT 30)

set_target_properties(unit_testing PROPERTIES
                      RUNTIME_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR}/bin)
