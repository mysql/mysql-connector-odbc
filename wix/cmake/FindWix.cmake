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


#-------------- FIND WIX_DIR ------------------
IF(DEFINED $ENV{WIX_DIR})
  SET(WIX_DIR "$ENV{WIX_DIR}")
ENDIF(DEFINED $ENV{WIX_DIR})

# Wix installer creates WIX environment variable
FIND_PATH(WIX_DIR candle.exe
	$ENV{WIX_DIR}/bin
	$ENV{WIX}/bin
	"$ENV{ProgramFiles}/wix/bin"
	"$ENV{ProgramFiles}/Windows Installer */bin")

#----------------- FIND MYSQL_LIB_DIR -------------------
IF (WIX_DIR)
	MESSAGE(STATUS "Wix found in ${WIX_DIR}")
ELSE (WIX_DIR)
	IF ($ENV{WIX_DIR})
		MESSAGE(FATAL_ERROR "Cannot find Wix in $ENV{WIX_DIR}")
	ELSE ($ENV{WIX_DIR})
		MESSAGE(FATAL_ERROR "Cannot find Wix. Please set environment variable WIX_DIR which points to the wix installation directory")
	ENDIF ($ENV{WIX_DIR})
ENDIF (WIX_DIR)

