# Note: Version 3.15 is required becausae RPM build instruction depend on cmake
# recognizing the --install command.

cmake_minimum_required(VERSION 3.15)

project(MySQL_ODBC_Packages)
include(../version.cmake)

find_program(RPM rpm)

if(NOT RPM)
  message(FATAL_ERROR "Could not find RPM tools")
endif()

# Get RPM version

execute_process(COMMAND ${RPM} "--version"
  RESULT_VARIABLE res
  OUTPUT_VARIABLE RPM_VERSION
  OUTPUT_STRIP_TRAILING_WHITESPACE
)

if(res)
  message(FATAL_ERROR "Could not determine RPM version (${RPM} --version command failed)")
endif()

# Set RPM_VER_NUM to version number in XYYZZ format

string(REGEX REPLACE "^RPM version *" "" RPM_VERSION ${RPM_VERSION})
string(REPLACE "." ";" ver ${RPM_VERSION})
list(GET ver 0 ver_major)
list(GET ver 1 ver_minor)
list(GET ver 2 ver_patch)
string(REGEX MATCH "..$" ver_minor "0${ver_minor}")
string(REGEX MATCH "..$" ver_patch "0${ver_patch}")
set(RPM_VER_NUM "${ver_major}${ver_minor}${ver_patch}")

message(STATUS "Using RPM tools version: ${RPM_VERSION} (${RPM_VER_NUM})")


# RPMs doesn't allow "-" in version, so we use like "x.x.0_alpha"
string(REPLACE "-" "_" CONNECTOR_NODASH_VERSION "${CONNECTOR_VERSION}")

if(UNIX)
  execute_process(COMMAND "date" "+%Y"
    OUTPUT_VARIABLE CURRENT_YEAR
    OUTPUT_STRIP_TRAILING_WHITESPACE
  )
endif()

# Generate RPM spec file

file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/SOURCES)
set(rpm_spec "${CMAKE_CURRENT_BINARY_DIR}/SPECS/mysql-connector-odbc.spec")

configure_file(mysql-connector-odbc.spec.in "${rpm_spec}" @ONLY)
