// Copyright (c) 2018, 2020 Oracle and/or its affiliates. All rights reserved.
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License, version 2.0, as
// published by the Free Software Foundation.
//
// This program is also distributed with certain software (including
// but not limited to OpenSSL) that is licensed under separate terms,
// as designated in a particular file or component or in included license
// documentation. The authors of MySQL hereby grant you an
// additional permission to link the program and your derivative works
// with the separately licensed software that they have included with
// MySQL.
//
// Without limiting anything contained in the foregoing, this file,
// which is part of <MySQL Product>, is also subject to the
// Universal FOSS Exception, version 1.0, a copy of which can be found at
// http://oss.oracle.com/licenses/universal-foss-exception.
//
// This program is distributed in the hope that it will be useful, but
// WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
// See the GNU General Public License, version 2.0, for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software Foundation, Inc.,
// 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA

#include <string>
#include <vector>

#include "odbc_util.h"


DECLARE_TEST(t_bug34355094_sqlcolumns_wchar)
{
  try
  {
    std::string tab_name = "t_bug34355094";
    std::string fields = "id int primary key,"
      "col1 varchar(100) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci NOT NULL,"
      "col2 char(16) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci NOT NULL,"
      "col3 LONGTEXT CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci NOT NULL"
      ;
    odbc::table tab(hstmt, nullptr, tab_name, fields);

    ok_stmt(hstmt, SQLColumns(hstmt, nullptr, 0, nullptr, 0,
      (SQLCHAR*)tab_name.c_str(), SQL_NTS,
      (SQLCHAR*)"col%", SQL_NTS));

    SQLINTEGER expected_type[2][3] = {
      {SQL_VARCHAR, SQL_CHAR, SQL_LONGVARCHAR},
      {SQL_WVARCHAR, SQL_WCHAR, SQL_WLONGVARCHAR},
    };
    int rnum = 0;
    while (SQL_SUCCESS == SQLFetch(hstmt))
    {
      SQLINTEGER data_type = my_fetch_int(hstmt, 5);
      // The value of unicode_driver is either 0 or 1 and
      // therefore it can be used as an index in the array
      is_num(expected_type[unicode_driver][rnum], data_type);
      ++rnum;
    }
    is_num(3, rnum);
  }
  ENDCATCH;
}


BEGIN_TESTS
  ADD_TEST(t_bug34355094_sqlcolumns_wchar)
END_TESTS


RUN_TESTS
