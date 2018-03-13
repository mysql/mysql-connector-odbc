# Copyright (c) 2006, 2018, Oracle and/or its affiliates. All rights reserved. 
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


SET_PROPERTY(DIRECTORY PROPERTY INCLUDE_DIRECTORIES ${CMAKE_SOURCE_DIR}/include/sys)

SET(STRINGS_SOURCES bchange.c ctype-big5.c ctype-bin.c ctype-cp932.c
                ctype-czech.c ctype-euc_kr.c ctype-eucjpms.c ctype-extra.c ctype-gb2312.c ctype-gbk.c ctype-gb18030.c
                ctype-latin1.c ctype-mb.c ctype-simple.c ctype-sjis.c ctype-tis620.c ctype-uca.c
                ctype-ucs2.c ctype-ujis.c ctype-utf8.c ctype-win1250ch.c ctype.c dtoa.c int2str.c
                is_prefix.c llstr.c longlong2str.c my_strtoll10.c my_vsnprintf.c
                str2int.c str_alloc.c strcend.c strend.c strfill.c strmake.c
                my_stpmov.c my_stpnmov.c
                strxmov.c strxnmov.c xml.c
		my_strchr.c strcont.c strappend.c)

IF(NOT HAVE_STRNLEN)
  # OSX does not have strnlen
  SET(STRINGS_SOURCES ${STRINGS_SOURCES} strnlen.c)
ENDIF()
# Avoid dependencies on perschema data defined in mysys
ADD_DEFINITIONS(-DDISABLE_MYSQL_THREAD_H)
ADD_LIBRARY(mysql_strings ${STRINGS_SOURCES})

IF(NOT WIN32)
  SET_TARGET_PROPERTIES(mysql_strings PROPERTIES COMPILE_FLAGS ${CMAKE_SHARED_LIBRARY_C_FLAGS})
ENDIF()