// Copyright (c) 2021, Oracle and/or its affiliates. All rights reserved.
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
#include <fstream>
#include <iostream>
#include <sstream>
#include <cstdio>
#include <map>
#include <thread>
#include <stdlib.h>

#include "odbc_util.h"

#ifdef _WIN32
#pragma warning (disable : 4996)
// warning C4996: 'mkdir': The POSIX name for this item is deprecated. Instead, use the ISO C and C++ conformant name: _mkdir.

#pragma warning (disable : 4566)
// warning C4566: character represented by universal-character-name
// '\u....' cannot be represented in the current code page

#include <direct.h>
#else
#include <sys/stat.h>
#endif

/*
  Bug 32552965
  MYSQL ODBC CONNECTOR *8.0.23* 'MALFORMED COMMUNICATION PACKET' WITH ADO.NET DATA
*/
DECLARE_TEST(t_bug32552965)
{
  try
  {
    odbc::table tab(hstmt, "t_bug32552965",
      "col1 MEDIUMBLOB NOT NULL, col2 TINYINT DEFAULT NULL");
    odbc::stmt_prepare(hstmt, "INSERT INTO " + tab.table_name + " VALUES(?,?)");

    odbc::xbuf buff(20000);
    buff.setval('Q', buff.size - 1);
    SQLLEN len1 = buff.size - 1;
    SQLLEN len2 = -1; // INSERT NULL !!!
    SQLINTEGER int_data = 0;

    ok_stmt(hstmt, SQLBindParameter(hstmt, 1, SQL_PARAM_INPUT,
          SQL_C_CHAR, SQL_LONGVARCHAR, 0, 0, buff, buff.size, &len1));
    ok_stmt(hstmt, SQLBindParameter(hstmt, 2, SQL_PARAM_INPUT,
          SQL_C_LONG, SQL_INTEGER, 0, 0, &int_data, 0, &len2));
    ok_stmt(hstmt, SQLExecute(hstmt));
  }
  ENDCATCH;
}


/*
  Bug 27499789
  ODBC PARAMETER NO_CACHE LEAD TO FUNCTION CALL ERROR NOT RETURN
*/
DECLARE_TEST(t_bug27499789)
{
  try
  {
    odbc::connection con(nullptr, nullptr, nullptr, nullptr, ";NO_CACHE=1");
    SQLHSTMT hstmt = con.hstmt;
    odbc::stmt_prepare(hstmt, "SELECT 1 + 9223372036854775807;");
    ok_stmt(hstmt, SQLExecute(hstmt));
    err_stmt(hstmt, SQLFetch(hstmt));

    SQLBIGINT val = 0;
    SQLLEN len = 0;
    err_stmt(hstmt, SQLGetData(hstmt, 1, SQL_C_SBIGINT, &val, 0, &len));
  }
  ENDCATCH;
}


/*
  Bug #32763378
  SQLSETPOS(SQL_DELETE) DOES NOT PRODUCE DIAGNOSTIC FOR DUPLICATE KEY
*/
DECLARE_TEST(t_bug32763378)
{
  try
  {
    odbc::table tab(hstmt, "bug32763378", "A VARCHAR(10) UNIQUE, B VARCHAR(10)");
    odbc::stmt_prepare(hstmt, "INSERT INTO " + tab.table_name + " VALUES(?,?)");

    SQLLEN rows_fetched = 0;
    SQLCHAR buffers[2][10];
    SQLLEN lengths[2] = { 0, 0 };
    memset(buffers, 0, sizeof(buffers));
    odbc::xbuf buff(SQL_MAX_MESSAGE_LENGTH);
    buff.setval("A");
    ok_stmt(hstmt, SQLBindParameter(hstmt, 1, SQL_PARAM_INPUT, SQL_C_CHAR,
	    SQL_VARCHAR, 0, 0, buff, buff.size, NULL));
    ok_stmt(hstmt, SQLBindParameter(hstmt, 2, SQL_PARAM_INPUT, SQL_C_CHAR,
	    SQL_VARCHAR, 0, 0, buff, buff.size, NULL));
    odbc::stmt_execute(hstmt);
    buff.setval("B");
    odbc::stmt_execute(hstmt);
    odbc::stmt_reset(hstmt);
    odbc::stmt_close(hstmt);

    odbc::stmt_prepare(hstmt, "SELECT A, B FROM " + tab.table_name + " WHERE A='A' FOR UPDATE");
    ok_stmt(hstmt, SQLBindCol(hstmt, 1, SQL_C_CHAR, buffers[0], 10, &lengths[0]));
    ok_stmt(hstmt, SQLBindCol(hstmt, 2, SQL_C_CHAR, buffers[1], 10, &lengths[1]));
    odbc::stmt_execute(hstmt);
    ok_stmt(hstmt, SQLFetchScroll(hstmt, SQL_FETCH_NEXT, 0));
    buffers[0][0] = 'B';
    is_err(SQLSetPos(hstmt, 1, SQL_UPDATE, SQL_LOCK_NO_CHANGE));

    SQLCHAR sqlstate[6] = { 0, 0, 0, 0, 0, 0 };
    SQLINTEGER  native_error = 0;
    SQLSMALLINT length = 0;
    ok_stmt(hstmt, SQLGetDiagRec(SQL_HANDLE_STMT, hstmt, 1, sqlstate, &native_error,
                     buff, buff.size - 1, &length));
    std::cout << "Expected error message: " << buff.get_str() << std::endl;
  }
  ENDCATCH;
}


BEGIN_TESTS
  ADD_TEST(t_bug32763378)
  ADD_TEST(t_bug27499789)
  ADD_TEST(t_bug32552965)

  END_TESTS

RUN_TESTS
