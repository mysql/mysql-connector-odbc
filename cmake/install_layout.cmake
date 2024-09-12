# Copyright (c) 2010, 2024, Oracle and/or its affiliates.
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

# Originally part of MySQL Server and Adapted for Connector/ODBC

# The purpose of this file is to set the default installation layout.
#
# - INSTALL_BINDIR          (directory with myodbc-install)
# - INSTALL_BINDIR_DEBUG
# - INSTALL_LIBDIR          (directory with ODBC drivers)
# - INSTALL_LIBDIR_DEBUG
# - INSTALL_DOCDIR          (readme and similar)
# - INSTALL_TESTDIR         (test modules)
# - INSTALL_TESTDIR_DEBUG
#

set(DEFAULT_INSTALL_BINDIR     "bin")
set(DEFAULT_INSTALL_LIBDIR     "lib")
set(DEFAULT_INSTALL_PLUGINDIR   "lib/plugin")
set(DEFAULT_INSTALL_DOCDIR     ".")
set(DEFAULT_INSTALL_TESTDIR    "test")


# Define cache entries which describe install layout.

foreach(var BIN LIB PLUGIN DOC TEST)

  set(
    INSTALL_${var}DIR  ${DEFAULT_INSTALL_${var}DIR}
    CACHE STRING "${var} installation directory"
  )
  mark_as_advanced(INSTALL_${var}DIR)
  message("Install layout ${var}: ${INSTALL_${var}DIR}")

  if(var MATCHES "DOC|PLUGIN")
    continue()
  endif()

  # Define _DEBUG cache entries. They differ from plain ones only on Windows
  # where the same project can be built either in debug or in release mode.
  # On other platforms the debug/release mode is choosen at the project
  # configuration time and regardless of the choice the artifacts are placed
  # in the same location.
  #
  # Note that the same DEFAULT_INSTALL_${var}DIR variable is re-used to set
  # the default debug paths.

  if(WIN32)
    set(DEFAULT_INSTALL_${var}DIR "${INSTALL_${var}DIR}/debug")
  else()
    set(DEFAULT_INSTALL_${var}DIR "${INSTALL_${var}DIR}")
  endif()

  set(
    INSTALL_${var}DIR_DEBUG  ${DEFAULT_INSTALL_${var}DIR}
    CACHE STRING "${var} installation directory (debug)"
  )
  mark_as_advanced(INSTALL_${var}DIR_DEBUG)
  message("Install layout ${var} (debug): ${INSTALL_${var}DIR_DEBUG}")

endforeach()


# Functions that should be used to install components.


function(install_lib COMP TARGET)

  install(
    TARGETS ${TARGET}
    CONFIGURATIONS Release RelWithDebInfo
    DESTINATION "${INSTALL_LIBDIR}"
    COMPONENT ${COMP}
  )

  install(
    TARGETS ${TARGET}
    CONFIGURATIONS Debug
    DESTINATION "${INSTALL_LIBDIR_DEBUG}"
    COMPONENT ${COMP}
  )

  if(WIN32)

    install(
      FILES "$<TARGET_PDB_FILE:${TARGET}>"
      CONFIGURATIONS RelWithDebInfo
      DESTINATION "${INSTALL_LIBDIR}"
      COMPONENT ${COMP}
    )
    install(
      FILES "$<TARGET_PDB_FILE:${TARGET}>"
      CONFIGURATIONS Debug
      DESTINATION "${INSTALL_LIBDIR_DEBUG}"
      COMPONENT ${COMP}
    )

  else()

    install(
      TARGETS ${TARGET}
      DESTINATION "${INSTALL_LIBDIR}"
      COMPONENT ${COMP}
    )

  endif()

endfunction()


function(install_bin COMP TARGET)

  install(
    TARGETS ${TARGET}
    CONFIGURATIONS Release RelWithDebInfo
    DESTINATION "${INSTALL_BINDIR}"
    COMPONENT ${COMP}
  )

  install(
    TARGETS ${TARGET}
    CONFIGURATIONS Debug
    DESTINATION "${INSTALL_BINDIR_DEBUG}"
    COMPONENT ${COMP}
  )

  if(WIN32)

    install(
      FILES "$<TARGET_PDB_FILE:${TARGET}>"
      CONFIGURATIONS RelWithDebInfo
      DESTINATION "${INSTALL_BINDIR}"
      COMPONENT ${COMP}
    )
    install(
      FILES "$<TARGET_PDB_FILE:${TARGET}>"
      CONFIGURATIONS Debug
      DESTINATION "${INSTALL_BINDIR_DEBUG}"
      COMPONENT ${COMP}
    )

  else()

    install(
      TARGETS ${TARGET}
      DESTINATION "${INSTALL_BINDIR}"
      COMPONENT ${COMP}
    )

  endif()

endfunction()


function(install_test TARGET)

  install(
    TARGETS ${TARGET}
    CONFIGURATIONS Release RelWithDebInfo
    DESTINATION "${INSTALL_TESTDIR}"
    COMPONENT tests
  )

  install(
    TARGETS ${TARGET}
    CONFIGURATIONS Debug
    DESTINATION "${INSTALL_TESTDIR_DEBUG}"
    COMPONENT tests
  )

  if(WIN32)

    install(
      FILES "$<TARGET_PDB_FILE:${TARGET}>"
      CONFIGURATIONS RelWithDebInfo
      DESTINATION "${INSTALL_TESTDIR}"
      COMPONENT tests
    )
    install(
      FILES "$<TARGET_PDB_FILE:${TARGET}>"
      CONFIGURATIONS Debug
      DESTINATION "${INSTALL_TESTDIR_DEBUG}"
      COMPONENT tests
    )

  else()

    install(
      TARGETS ${TARGET}
      DESTINATION "${INSTALL_TESTDIR}"
      COMPONENT tests
    )

  endif()

endfunction()

function(install_test_files)

  # message(STATUS "Installing test files: ${ARGN}")

  install(
    FILES ${ARGN}
    CONFIGURATIONS Release RelWithDebInfo
    DESTINATION "${INSTALL_TESTDIR}"
    COMPONENT tests
  )

  install(
    FILES ${ARGN}
    CONFIGURATIONS Debug
    DESTINATION "${INSTALL_TESTDIR_DEBUG}"
    COMPONENT tests
  )

  if(NOT WIN32)
    install(
      FILES ${ARGN}
      DESTINATION "${INSTALL_TESTDIR}"
      COMPONENT tests
    )
  endif()

endfunction()


function(install_doc)

  install(
    FILES ${ARGN}
    DESTINATION "${INSTALL_DOCDIR}"
    COMPONENT readme
  )

endfunction()
