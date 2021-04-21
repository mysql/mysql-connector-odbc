# Copyright (c) 2007, 2020, Oracle and/or its affiliates. All rights reserved. 
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


IF (WIN32)
  SET(SETUP_SRCS ConfigDSN.cc
                 callbacks.cc
                 setupgui.h
                 utils.cc
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

  # Sources for the main setup module
  SET(SETUP_SRCS gtk/setup_loader.cc)
  
  # Sources for the GTK setup module
  SET(SETUP_SRCS_GTK ConfigDSN.cc
                     callbacks.cc
                     setupgui.h
                     utils.cc
                     gtk/odbcdialogparams.cc
                     gtk/ODBCINSTGetProperties.cc
  )

ENDIF (WIN32)


IF (APPLE)
  ADD_LIBRARY(myodbc8S MODULE ${SETUP_SRCS})
ELSE (APPLE)

  # myodbc8S is common for Windows and GTK
  
  ADD_LIBRARY(myodbc8S SHARED ${SETUP_SRCS})
  
  IF(NOT WIN32)
    INCLUDE (FindPkgConfig)
    PKG_CHECK_MODULES(GTK3 gtk+-3.0)
  
    IF (GTK3_INCLUDE_DIRS)
      TRY_COMPILE(RESULT_VAR ${PROJECT_BINARY_DIR} SOURCES ${PROJECT_SOURCE_DIR}/setupgui/gtk/try.cc
        CMAKE_FLAGS -DLINK_DIRECTORIES=${GTK3_LIBRARY_DIRS}
        LINK_LIBRARIES ${GTK3_LIBRARIES})

      IF(RESULT_VAR)
        ADD_LIBRARY(myodbc8S-gtk3 SHARED ${SETUP_SRCS_GTK})
        TARGET_INCLUDE_DIRECTORIES(myodbc8S-gtk3 PUBLIC ${GTK3_INCLUDE_DIRS})
      
        IF(GTK3_LIBRARY_DIRS)
          TARGET_LINK_DIRECTORIES(myodbc8S-gtk3 PUBLIC ${GTK3_LIBRARY_DIRS})
        ENDIF(GTK3_LIBRARY_DIRS)
      
        INSTALL(TARGETS myodbc8S-gtk3 DESTINATION ${LIB_SUBDIR})
        SET_TARGET_PROPERTIES(myodbc8S-gtk3 PROPERTIES COMPILE_FLAGS "${PLATFORM_CFLAGS}")
      ENDIF(RESULT_VAR)
    ENDIF(GTK3_INCLUDE_DIRS)

    PKG_CHECK_MODULES(GTK2 gtk+-2.0)
    
    IF (GTK2_INCLUDE_DIRS)

      TRY_COMPILE(RESULT_VAR ${PROJECT_BINARY_DIR} SOURCES ${PROJECT_SOURCE_DIR}/setupgui/gtk/try.cc
      CMAKE_FLAGS -DLINK_DIRECTORIES=${GTK2_LIBRARY_DIRS}
      LINK_LIBRARIES ${GTK2_LIBRARIES})

      IF(RESULT_VAR)
        ADD_LIBRARY(myodbc8S-gtk2 SHARED ${SETUP_SRCS_GTK})
        TARGET_INCLUDE_DIRECTORIES(myodbc8S-gtk2 PUBLIC ${GTK2_INCLUDE_DIRS})
      
        IF(GTK2_LIBRARY_DIRS)
          TARGET_LINK_DIRECTORIES(myodbc8S-gtk2 PUBLIC ${GTK2_LIBRARY_DIRS})
        ENDIF(GTK2_LIBRARY_DIRS)
        INSTALL(TARGETS myodbc8S-gtk2 DESTINATION ${LIB_SUBDIR})
        SET_TARGET_PROPERTIES(myodbc8S-gtk2 PROPERTIES COMPILE_FLAGS "${PLATFORM_CFLAGS}")      
      ENDIF(RESULT_VAR)
    ENDIF (GTK2_INCLUDE_DIRS)
  
    SET_TARGET_PROPERTIES(myodbc8S PROPERTIES COMPILE_FLAGS "${PLATFORM_CFLAGS}")
  ENDIF(NOT WIN32)
ENDIF (APPLE)


INSTALL(TARGETS myodbc8S DESTINATION ${LIB_SUBDIR})

INCLUDE_DIRECTORIES(${CMAKE_SOURCE_DIR})
INCLUDE_DIRECTORIES(${CMAKE_SOURCE_DIR}/util)
INCLUDE_DIRECTORIES(${CMAKE_SOURCE_DIR}/driver)

IF (NOT WIN32)
  SET(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} ${ODBC_LINK_FLAGS}")
  
  TARGET_LINK_LIBRARIES(myodbc8S
                      myodbc-util ${ODBCLIB} ${ODBCINSTLIB} dl
                      ${PLATFORM_LIBS})
  
  IF (TARGET myodbc8S-gtk3)
    TARGET_LINK_LIBRARIES(myodbc8S-gtk3 ${GTK3_LIBRARIES}
                          myodbc-util ${ODBCLIB} ${ODBCINSTLIB}
                           ${PLATFORM_LIBS} ${MYSQL_CLIENT_LIBS})
  ENDIF (TARGET myodbc8S-gtk3)
  
  IF (TARGET myodbc8S-gtk2)
    TARGET_LINK_LIBRARIES(myodbc8S-gtk2 ${GTK2_LIBRARIES}
                          myodbc-util ${ODBCLIB} ${ODBCINSTLIB}
                          ${PLATFORM_LIBS} ${MYSQL_CLIENT_LIBS})
  ENDIF (TARGET myodbc8S-gtk2)
  
ELSE (NOT WIN32)
  TARGET_LINK_LIBRARIES(myodbc8S
                      myodbc-util ${ODBCLIB} ${ODBCINSTLIB} ${MYSQL_CLIENT_LIBS}
                      ${PLATFORM_LIBS} legacy_stdio_definitions.lib)
  IF (WITH_NODEFAULTLIB)
    SET_TARGET_PROPERTIES(myodbc8S PROPERTIES
      LINK_FLAGS_DEBUG "/NODEFAULTLIB:${WITH_NODEFAULTLIB}"
      LINK_FLAGS_RELWITHDEBINFO "/NODEFAULTLIB:${WITH_NODEFAULTLIB}"
      LINK_FLAGS_RELEASE "/NODEFAULTLIB:${WITH_NODEFAULTLIB}")
  ENDIF ()

ENDIF (NOT WIN32)


IF(MYSQL_CXX_LINKAGE)
  SET_TARGET_PROPERTIES(myodbc8S PROPERTIES LINKER_LANGUAGE CXX)
  set_property(TARGET myodbc8S APPEND PROPERTY INSTALL_RPATH "$ORIGIN")

  IF (TARGET myodbc8S-gtk3)
    SET_TARGET_PROPERTIES(myodbc8S-gtk3 PROPERTIES LINKER_LANGUAGE CXX)  
  ENDIF (TARGET myodbc8S-gtk3)  
  
  IF (TARGET myodbc8S-gtk2)
    SET_TARGET_PROPERTIES(myodbc8S-gtk2 PROPERTIES LINKER_LANGUAGE CXX)  
  ENDIF (TARGET myodbc8S-gtk2)  
  
ENDIF(MYSQL_CXX_LINKAGE)
