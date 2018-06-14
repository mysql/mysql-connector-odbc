# Copyright (c) 2007, 2018, Oracle and/or its affiliates. All rights reserved. 
# 
# This program is free software; you can redistribute it and/or modify 
# it under the terms of the GNU General Public License, version 2.0, as 
# published by the Free Software Foundation. 
#
# This program is also distributed with certain software (including 
# but not limited to OpenSSL) that is licensed under separate terms, 
# as designated in a particular file or component or in included license 
# documentation. The authors of MySQL hereby grant you an 
# additional permission to link the program and your derivative works 
# with the separately licensed software that they have included with 
# MySQL. 
# 
# Without limiting anything contained in the foregoing, this file, 
# which is part of MySQL Connector/ODBC, is also subject to the 
# Universal FOSS Exception, version 1.0, a copy of which can be found at 
# http://oss.oracle.com/licenses/universal-foss-exception. 
# 
# This program is distributed in the hope that it will be useful, but 
# WITHOUT ANY WARRANTY; without even the implied warranty of 
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. 
# See the GNU General Public License, version 2.0, for more details. 
# 
# You should have received a copy of the GNU General Public License 
# along with this program; if not, write to the Free Software Foundation, Inc., 
# 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA 

##########################################################################

ADD_DEFINITIONS(-DUNICODE -D_UNICODE)

IF (UNICODE OR NOT ANSI)
  ADD_DEFINITIONS(-DDRIVER_UNICODE)
ELSE (UNICODE OR NOT ANSI)
  ADD_DEFINITIONS(-DDRIVER_ANSI)
ENDIF (UNICODE OR NOT ANSI)


SET(SETUP_SRCS ConfigDSN.cc callbacks.cc setupgui.h utils.cc)

IF (WIN32)
  SET(SETUP_SRCS ${SETUP_SRCS}
                 myodbc8S.def
                 windows/main.cpp
                 windows/odbcdialogparams.cpp
                 windows/odbcdialogparams.h
                 windows/odbcdialogparams.rc
                 windows/resource.h
                 windows/TabCtrl.cpp
                 windows/TabCtrl.h
                 windows/tooltip.cpp
                 windows/connector_odbc_header.bmp)
  SET(PLATFORM_LIBS comctl32)
ELSE (WIN32)
  SET(SETUP_SRCS ${SETUP_SRCS}
                 gtk/odbcdialogparams.cc
                 gtk/ODBCINSTGetProperties.cc
  )
  INCLUDE (FindPkgConfig)
  PKG_CHECK_MODULES(GTK2 gtk+-2.0)
  IF (GTK2_INCLUDE_DIRS)
    INCLUDE_DIRECTORIES(${GTK2_INCLUDE_DIRS})
    LINK_DIRECTORIES(${GTK2_LIBRARY_DIRS})
  ELSE (GTK2_INCLUDE_DIRS)
    PKG_CHECK_MODULES(GTK3 gtk+-3.0)
    INCLUDE_DIRECTORIES(${GTK3_INCLUDE_DIRS})
    LINK_DIRECTORIES(${GTK3_LIBRARY_DIRS})
  ENDIF (GTK2_INCLUDE_DIRS)
ENDIF (WIN32)


IF (APPLE)
ADD_LIBRARY(myodbc8S MODULE ${SETUP_SRCS})
ELSE (APPLE)
ADD_LIBRARY(myodbc8S SHARED ${SETUP_SRCS})
ENDIF (APPLE)

INSTALL(TARGETS myodbc8S DESTINATION ${LIB_SUBDIR})

INCLUDE_DIRECTORIES(${CMAKE_SOURCE_DIR})
INCLUDE_DIRECTORIES(${CMAKE_SOURCE_DIR}/util)
INCLUDE_DIRECTORIES(${CMAKE_SOURCE_DIR}/driver)

IF (NOT WIN32)
  SET_TARGET_PROPERTIES(myodbc8S PROPERTIES COMPILE_FLAGS "${PLATFORM_CFLAGS}")
  SET(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} ${ODBC_LINK_FLAGS}")
  IF (GTK2_LIBRARIES)
    TARGET_LINK_LIBRARIES(myodbc8S ${GTK2_LIBRARIES})
  ELSE (GTK2_LIBRARIES)
    TARGET_LINK_LIBRARIES(myodbc8S ${GTK3_LIBRARIES})
  ENDIF (GTK2_LIBRARIES)
ENDIF (NOT WIN32)

IF(WIN32 AND MYSQL8)
  TARGET_LINK_LIBRARIES(myodbc8S
                      myodbc-util ${ODBCLIB} ${ODBCINSTLIB} ${MYSQL_CLIENT_LIBS}
                      ${PLATFORM_LIBS} legacy_stdio_definitions.lib)
ELSE(WIN32 AND MYSQL8)
  TARGET_LINK_LIBRARIES(myodbc8S
                      myodbc-util ${ODBCLIB} ${ODBCINSTLIB} ${MYSQL_CLIENT_LIBS}
                      ${PLATFORM_LIBS})
ENDIF(WIN32 AND MYSQL8)

IF (WIN32)
    IF (WITH_NODEFAULTLIB)
      SET_TARGET_PROPERTIES(myodbc8S PROPERTIES
        LINK_FLAGS_DEBUG "/NODEFAULTLIB:${WITH_NODEFAULTLIB}"
        LINK_FLAGS_RELWITHDEBINFO "/NODEFAULTLIB:${WITH_NODEFAULTLIB}"
        LINK_FLAGS_RELEASE "/NODEFAULTLIB:${WITH_NODEFAULTLIB}")
    ENDIF ()
ENDIF (WIN32)

IF(MYSQL_CXX_LINKAGE)
  SET_TARGET_PROPERTIES(myodbc8S PROPERTIES LINKER_LANGUAGE CXX)
ENDIF(MYSQL_CXX_LINKAGE)
