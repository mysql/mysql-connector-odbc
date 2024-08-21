cmake_minimum_required(VERSION 3.5)

project(MySQL_ODBC_Packages)
include(../version.cmake)

# RPMs doesn't allow "-" in version, so we use like "x.x.0_alpha"
string(REPLACE "-" "_" CONNECTOR_NODASH_VERSION "${CONNECTOR_VERSION}")

if(UNIX)
  execute_process(COMMAND "date" "+%Y"
    OUTPUT_VARIABLE CURRENT_YEAR
    OUTPUT_STRIP_TRAILING_WHITESPACE
  )
endif()

file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/SOURCES)
set(rpm_spec "${CMAKE_CURRENT_BINARY_DIR}/SPECS/mysql-connector-odbc.spec")

configure_file(mysql-connector-odbc.spec.in "${rpm_spec}" @ONLY)
