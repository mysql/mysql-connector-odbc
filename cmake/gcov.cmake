# Copyright (c) 2015, 2024, Oracle and/or its affiliates.
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


if (NOT DEFINED WITH_COVERAGE)
  # use ENABLE_GCOV here to make it compatible with server conventions
  option(WITH_COVERAGE "Enable gcov (debug, Linux builds only)" ${ENABLE_GCOV})
endif()



macro(ADD_COVERAGE target)

  if(CMAKE_COMPILER_IS_GNUCXX AND WITH_COVERAGE)

    message(STATUS "Enabling gcc coverage support for target: ${target}")

    # Note: we use set_property(... APPEND ...) to not override other sttings
    # for the target.

#    set_property(TARGET ${target} APPEND
#      PROPERTY COMPILE_OPTIONS -fprofile-arcs -ftest-coverage
#    )

    set_property(TARGET ${target} APPEND
      PROPERTY COMPILE_OPTIONS
        -O0;-fprofile-arcs;-ftest-coverage
    )

    set_property(TARGET ${target} APPEND
      PROPERTY INTERFACE_LINK_LIBRARIES gcov)

  endif()

endmacro(ADD_COVERAGE)
