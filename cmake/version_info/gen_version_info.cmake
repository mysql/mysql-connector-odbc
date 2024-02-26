# Copyright (c) 2024, Oracle and/or its affiliates.
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License, version 2.0, as
# published by the Free Software Foundation.
#
# This program is designed to work with certain software (including
# but not limited to OpenSSL) that is licensed under separate terms, as
# designated in a particular file or component or in included license
# documentation. The authors of MySQL hereby grant you an additional
# permission to link the program and your derivative works with the
# separately licensed software that they have either included with
# the program or referenced in the documentation.
#
# Without limiting anything contained in the foregoing, this file,
# which is part of Connector/C++, is also subject to the
# Universal FOSS Exception, version 1.0, a copy of which can be found at
# https://oss.oracle.com/licenses/universal-foss-exception.
#
# This program is distributed in the hope that it will be useful, but
# WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
# See the GNU General Public License, version 2.0, for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software Foundation, Inc.,
# 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA


message("Re-generating version info resource defintions: ${RC}")
message("For output file: ${OUTPUT}")
message("Of type: ${TYPE}")
message("Build configuration: ${CONFIG}")

include("${VERSION}")


set(PATH "${OUTPUT}")
get_filename_component(BASE "${OUTPUT}" NAME)
get_filename_component(BASE_WE "${OUTPUT}" NAME_WE)

set(ODBC_VERSION "${CONNECTOR_NUMERIC_VERSION}")
string(REPLACE "." "," ODBC_VERSION_RAW "${ODBC_VERSION}")

if("STATIC_LIBRARY" STREQUAL TYPE)
  set(FILETYPE "VFT_STATIC_LIB")
elseif(TYPE MATCHES "_LIBRARY")
  set(FILETYPE "VFT_DLL")
elseif("EXECUTABLE" STREQUAL TYPE)
  set(FILETYPE "VFT_APP")
else()
  set(FILETYPE 0)
endif()

set(FILEFLAGS 0)
if(CONFIG MATCHES "[Dd][Ee][Bb][Uu][Gg]")
  set(FILEFLAGS "VS_FF_DEBUG")
endif()


configure_file("${CMAKE_CURRENT_LIST_DIR}/version_info.rc.in" "${RC}" @ONLY)
