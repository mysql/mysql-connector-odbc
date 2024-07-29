# Copyright (c) 2007, 2024, Oracle and/or its affiliates.
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
# which is part of Connector/ODBC, is also subject to the
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

SET(CONNECTOR_MAJOR "9")
SET(CONNECTOR_MINOR "1")
SET(CONNECTOR_PATCH "0")

SET(CONNECTOR_MAJOR_PREV "9")
SET(CONNECTOR_MINOR_PREV "0")
SET(CONNECTOR_PATCH_PREV "0")


SET(CONNECTOR_LEVEL "")
SET(CONNECTOR_QUALITY "GA")

# Note: To be used in copyright notes of generated files

set(COPYRIGHT_YEAR 2024)


# ----------------------------------------------------------------------
# Set other variables that are about the version
# ----------------------------------------------------------------------

IF(CONNECTOR_MINOR LESS 10)
  SET(CONNECTOR_MINOR_PADDED "0${CONNECTOR_MINOR}")
ELSE(CONNECTOR_MINOR LESS 10)
  SET(CONNECTOR_MINOR_PADDED "${CONNECTOR_MINOR}")
ENDIF(CONNECTOR_MINOR LESS 10)

# If driver survives 100th patch this has to be changed
IF(CONNECTOR_PATCH LESS 10)
  SET(CONNECTOR_PATCH_PADDED "000${CONNECTOR_PATCH}")
ELSE(CONNECTOR_PATCH LESS 10)
  SET(CONNECTOR_PATCH_PADDED "00${CONNECTOR_PATCH}")
ENDIF(CONNECTOR_PATCH LESS 10)

SET(CONNECTOR_BASE_VERSION    "${CONNECTOR_MAJOR}.${CONNECTOR_MINOR}")
SET(CONNECTOR_BASE_PREVIOUS   "${CONNECTOR_MAJOR_PREV}.${CONNECTOR_MINOR_PREV}")
SET(CONNECTOR_NUMERIC_VERSION "${CONNECTOR_BASE_VERSION}.${CONNECTOR_PATCH}")
SET(CONNECTOR_VERSION         "${CONNECTOR_NUMERIC_VERSION}${EXTRA_VERSION_SUFFIX}${CONNECTOR_LEVEL}")

#
#  Generate version info
#

if(EXTRA_NAME_SUFFIX STREQUAL "-commercial")
  SET(CONNECTOR_LICENSE "COMMERCIAL" CACHE INTERNAL "license info")
else()
  SET(CONNECTOR_LICENSE "GPL-2.0" CACHE INTERNAL "license info")
endif()

# Define the driver library file names
SET(MYODBC_ANSI_NAME myodbc${CONNECTOR_MAJOR}a)
SET(MYODBC_UNICODE_NAME myodbc${CONNECTOR_MAJOR}w)
SET(MYODBC_SETUP_NAME myodbc${CONNECTOR_MAJOR}S)
