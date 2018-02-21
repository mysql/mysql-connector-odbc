/*
  Copyright (c) 2007 MySQL AB, 2010 Sun Microsystems, Inc.
  Use is subject to license terms.

  The MySQL Connector/ODBC is licensed under the terms of the GPLv2
  <http://www.gnu.org/licenses/old-licenses/gpl-2.0.html>, like most
  MySQL Connectors. There are special exceptions to the terms and
  conditions of the GPLv2 as it is applied to this software, see the
  FLOSS License Exception
  <http://www.mysql.com/about/legal/licensing/foss-exception.html>.
  
  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published
  by the Free Software Foundation; version 2 of the License.
  
  This program is distributed in the hope that it will be useful, but
  WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
  or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License
  for more details.
  
  You should have received a copy of the GNU General Public License along
  with this program; if not, write to the Free Software Foundation, Inc.,
  51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

#ifndef _UNICODE_TRANSCODE_H
#define _UNICODE_TRANSCODE_H

/* Unicode transcoding */
typedef unsigned int UTF32;
typedef unsigned short UTF16;
typedef unsigned char UTF8;

#ifndef ODBCTAP
# include "stringutil.h"
#endif

int utf16toutf32(UTF16 *i, UTF32 *u);
int utf32toutf16(UTF32 i, UTF16 *u);
int utf8toutf32(UTF8 *i, UTF32 *u);
int utf32toutf8(UTF32 i, UTF8 *c);

#endif
