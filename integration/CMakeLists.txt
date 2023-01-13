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

project(integration)

# GoogleTest requires at least C++11
set(CMAKE_CXX_STANDARD 17)

#-------------- unixodbc/iodbc/win -------------------
if(WIN32)
  set(ODBCLIB odbc32)
else(WIN32)
  if(WITH_UNIXODBC)
    set(ODBCLIB odbc)
  else(WITH_UNIXODBC)
    set(ODBCLIB iodbc)
  endif(WITH_UNIXODBC)
endif(WIN32)

include(FetchContent)
FetchContent_Declare(
  googletest
  URL https://github.com/google/googletest/archive/refs/tags/release-1.11.0.zip
)
FetchContent_Declare(
  awssdk
  GIT_REPOSITORY https://github.com/aws/aws-sdk-cpp
)
FetchContent_Declare(
  toxiproxy
  GIT_REPOSITORY https://github.com/Bit-Quill/toxiproxy-cpp
  GIT_TAG main
)
FetchContent_Declare(
  openxlsx
  GIT_REPOSITORY https://github.com/troldal/OpenXLSX
)

# For Windows: Prevent overriding the parent project's compiler/linker settings
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)

# Only builds the RDS SDK
set(BUILD_ONLY "rds" CACHE INTERNAL "")
set(ENABLE_TESTING OFF CACHE BOOL "" FORCE)

# OpenXLSX
set(OPENXLSX_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(OPENXLSX_BUILD_BENCHMARKS OFF CACHE BOOL "" FORCE)
set(OPENXLSX_BUILD_SAMPLES OFF CACHE BOOL "" FORCE)
set(OPENXLSX_CREATE_DOCS OFF CACHE BOOL "" FORCE)

set(CMAKE_EXE_LINKER_FLAGS_DEBUGOPT
  ""
  CACHE STRING "Flags used for linking binaries during coverage builds."
  FORCE)
set(CMAKE_SHARED_LINKER_FLAGS_DEBUGOPT
  ""
  CACHE STRING "Flags used by the shared libraries linker during coverage builds."
  FORCE)
mark_as_advanced(
  CMAKE_CXX_FLAGS_DEBUGOPT
  CMAKE_C_FLAGS_DEBUGOPT
  CMAKE_EXE_LINKER_FLAGS_DEBUGOPT
  CMAKE_SHARED_LINKER_FLAGS_DEBUGOPT)
set(AWS_SDK_DEPENDENCIES aws-c-auth aws-c-cal aws-c-common aws-c-compression
  aws-c-event-stream aws-checksums aws-c-http aws-c-io aws-c-mqtt
  aws-crt-cpp aws-c-s3 aws-cpp-sdk-core aws-cpp-sdk-rds)

FetchContent_MakeAvailable(googletest toxiproxy awssdk)

if(ENABLE_PERFORMANCE_TESTS)
  FetchContent_MakeAvailable(openxlsx)
endif()

enable_testing()

set(TEST_SOURCES connection_string_builder.cc base_failover_integration_test.cc connection_string_builder_test.cc)
set(INTEGRATION_TESTS network_failover_integration_test.cc failover_integration_test.cc)

if(NOT ENABLE_PERFORMANCE_TESTS)
  set(TEST_SOURCES ${TEST_SOURCES} ${INTEGRATION_TESTS})
else()
  set(TEST_SOURCES ${TEST_SOURCES} failover_performance_test.cc)
endif()

add_executable(
  integration
  ${TEST_SOURCES}
)

include(GoogleTest)

gtest_discover_tests(integration)

target_include_directories(integration PUBLIC "${httplib_SOURCE_DIR}")

target_link_libraries(
  integration
  nlohmann_json::nlohmann_json
  ${ODBCLIB}
  gtest_main
  toxiproxy
  ${AWS_SDK_DEPENDENCIES}
)

if(ENABLE_PERFORMANCE_TESTS)
  target_link_libraries(integration OpenXLSX::OpenXLSX)
endif()

set_target_properties(integration PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR}/bin)
