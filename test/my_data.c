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

#include "odbctap.h"

DECLARE_TEST(my_json)
{
  SQLINTEGER id_value = 1;
  SQLLEN out_bytes[3] = {0, 0, 0};
  SQLCHAR buf[255], qbuf[255];
  DECLARE_BASIC_HANDLES(henv2, hdbc2, hstmt2);
  alloc_basic_handles_with_opt(&henv2, &hdbc2, &hstmt2, NULL, NULL, NULL, NULL,
                               "CHARSET=utf8");

  const char *json = "{\"key1\": \"value1\", \"key2\": \"value2\"}";
  const char *json2 = "{\"key1\": \"value10\", \"key2\": \"value20\"}";

  ok_sql(hstmt2, "DROP TABLE IF EXISTS t_bug_json");
  ok_sql(hstmt2, "CREATE TABLE t_bug_json(id INT PRIMARY KEY, jdoc JSON)");

  snprintf(qbuf, 255, "INSERT INTO t_bug_json VALUES (1, '%s')", json);
  ok_stmt(hstmt2, SQLExecDirect(hstmt2, qbuf, SQL_NTS));

  ok_stmt(hstmt2, SQLColumns(hstmt2, mydb, SQL_NTS, NULL, 0,
                            (SQLCHAR *)"t_bug_json", SQL_NTS, NULL, 0));

  ok_stmt(hstmt2, SQLFetch(hstmt2)); // Fetch 1st column and ignore data
  ok_stmt(hstmt2, SQLFetch(hstmt2)); // Fetch 2nd column

  is_num(my_fetch_int(hstmt2, 5), -1);             // DATA_TYPE
  is_str(my_fetch_str(hstmt2, buf, 6), "json", 4); // TYPE_NAME

  expect_stmt(hstmt2, SQLFetch(hstmt2), SQL_NO_DATA);
  ok_stmt(hstmt2, SQLFreeStmt(hstmt2, SQL_CLOSE));
  ok_stmt(hstmt2, SQLPrepare(hstmt2, "SELECT id, jdoc FROM t_bug_json WHERE id=?",
                            SQL_NTS));
  ok_stmt(hstmt2, SQLBindParameter(hstmt2, 1, SQL_PARAM_INPUT, SQL_C_DEFAULT,
                                  SQL_INTEGER, 10, 0, &id_value, 0, &out_bytes[0]));
  ok_stmt(hstmt2, SQLExecute(hstmt2));
  ok_stmt(hstmt2, SQLFetch(hstmt2));
  is_num(my_fetch_int(hstmt2, 1), 1);
  is_str(my_fetch_str(hstmt2, buf, 2), json, strlen(json));
  expect_stmt(hstmt2, SQLFetch(hstmt2), SQL_NO_DATA);

  ok_stmt(hstmt2, SQLFreeStmt(hstmt2, SQL_CLOSE));
  ok_stmt(hstmt2, SQLPrepare(hstmt2, "UPDATE t_bug_json SET jdoc = ? "\
                                   "WHERE id = ?", SQL_NTS));

  out_bytes[0] = strlen(json2);
  ok_stmt(hstmt2, SQLBindParameter(hstmt2, 1, SQL_PARAM_INPUT, SQL_C_CHAR,
                                  SQL_LONGVARCHAR, 0, 0, (SQLPOINTER)json2, strlen(json2),
                                  &out_bytes[0]));

  ok_stmt(hstmt2, SQLBindParameter(hstmt2, 2, SQL_PARAM_INPUT, SQL_C_DEFAULT,
                                  SQL_INTEGER, 10, 0, &id_value, 0, &out_bytes[1]));

  ok_stmt(hstmt2, SQLExecute(hstmt2));

  ok_stmt(hstmt2, SQLPrepare(hstmt2, "SELECT id, jdoc FROM t_bug_json WHERE id=?",
                            SQL_NTS));
  out_bytes[0] = 0;
  ok_stmt(hstmt2, SQLBindParameter(hstmt2, 1, SQL_PARAM_INPUT, SQL_C_DEFAULT,
                                  SQL_INTEGER, 10, 0, &id_value, 0, &out_bytes[0]));
  ok_stmt(hstmt2, SQLExecute(hstmt2));
  ok_stmt(hstmt2, SQLFetch(hstmt2));
  is_num(my_fetch_int(hstmt2, 1), 1);
  is_str(my_fetch_str(hstmt2, buf, 2), json2, strlen(json2));
  expect_stmt(hstmt2, SQLFetch(hstmt2), SQL_NO_DATA);

  ok_stmt(hstmt2, SQLFreeStmt(hstmt2, SQL_CLOSE));

  ok_sql(hstmt2, "DROP TABLE t_bug_json");
  free_basic_handles(&henv2, &hdbc2, &hstmt2);
  return OK;
}

DECLARE_TEST(my_local_infile)
{
  SQLRETURN rc = 0;
  SQLINTEGER num_rows = 0;
  FILE *csv_file = NULL;

  if (strcmp(myserver, "localhost"))
    return OK;

  DECLARE_BASIC_HANDLES(henv1, hdbc1, hstmt1);

  ok_sql(hstmt, "DROP TABLE IF EXISTS test_local_infile");
  ok_sql(hstmt, "CREATE TABLE test_local_infile (id int, vc varchar(32))");

  csv_file = fopen("test_local_infile.csv", "w");
  if (csv_file == NULL) goto END_TEST;

  fputs("1,abc\n2,def\n3,ghi",csv_file);
  fclose(csv_file);

  ok_sql(hstmt, "SET GLOBAL local_infile=true");
  rc = SQLExecDirect(hstmt, "LOAD DATA LOCAL INFILE 'test_local_infile.csv' " \
                    "INTO TABLE test_local_infile " \
                    "FIELDS TERMINATED BY ',' " \
                    "LINES TERMINATED BY '\\n'", SQL_NTS);
  if (rc != SQL_ERROR)  // First attempt must fail
  {
    rc = SQL_ERROR;
    goto END_TEST;
  }

  rc = alloc_basic_handles_with_opt(&henv1, &hdbc1, &hstmt1,
                                    NULL, NULL, NULL, NULL,
                                    "ENABLE_LOCAL_INFILE=1");
  if (rc != SQL_SUCCESS) goto END_TEST;

  rc = SQLExecDirect(hstmt1,
                     "LOAD DATA LOCAL INFILE 'test_local_infile.csv' " \
                     "INTO TABLE test_local_infile " \
                     "FIELDS TERMINATED BY ',' " \
                     "LINES TERMINATED BY '\\n'", SQL_NTS);

  if (rc != SQL_SUCCESS)
  {
      print_diag(rc, SQL_HANDLE_STMT, hstmt1, "LOAD DATA LOCAL INFILE FAILED", __FILE__, __LINE__);
      goto END_TEST;
  }

  rc = SQLExecDirect(hstmt, "SELECT count(*) FROM test_local_infile", SQL_NTS);
  if (rc != SQL_SUCCESS) goto END_TEST;
  rc = SQLFetch(hstmt);
  if (rc != SQL_SUCCESS) goto END_TEST;
  num_rows = my_fetch_int(hstmt, 1);
  if (num_rows != 3)
  {
    rc = SQL_ERROR;
    goto END_TEST;
  }


END_TEST:
  SQLFreeStmt(hstmt, SQL_CLOSE);
  ok_sql(hstmt, "SET GLOBAL local_infile=false");
  free_basic_handles(&henv1, &hdbc1, &hstmt1);
  ok_sql(hstmt, "DROP TABLE IF EXISTS test_local_infile");

  return rc == SQL_SUCCESS ? OK : FAIL;
}


DECLARE_TEST(t_bug106683)
{
  DECLARE_BASIC_HANDLES(henv1, hdbc1, hstmt1);

  ok_sql(hstmt, "DROP TABLE IF EXISTS bug106683");
  ok_sql(hstmt, "CREATE TABLE bug106683(datetime0 varchar(32))");
  ok_sql(hstmt, "INSERT INTO bug106683 VALUES ('2010-06-01 14:56:00'),(NULL),('2022-01-01 14:56:00')");

  is(SQL_SUCCESS == alloc_basic_handles_with_opt(&henv1, &hdbc1, &hstmt1,
                   NULL, NULL, NULL, NULL,
                   "NO_CACHE=1"));

  ok_stmt(hstmt1, SQLSetStmtAttr(hstmt1, SQL_ATTR_CURSOR_TYPE,
    (SQLPOINTER)SQL_CURSOR_FORWARD_ONLY, SQL_IS_UINTEGER));
  ok_stmt(hstmt1, SQLPrepare(hstmt1, "SELECT * FROM bug106683", SQL_NTS));
  ok_stmt(hstmt1, SQLExecute(hstmt1));

  int rnum = 1;
  while(SQL_SUCCESS == SQLFetch(hstmt1))
  {
    char buf[64];
    memset(buf, 0, sizeof(buf));
    SQLLEN len = 0;
    ok_stmt(hstmt1, SQLGetData(hstmt1, 1, SQL_C_CHAR, buf, sizeof(buf), &len));
    switch(rnum)
    {
      case 1:
      case 3:
        is_num(19, len);
        printf("Result [%s] ", buf);
        break;
      case 2:
      default:
        printf("Result [NULL] ");
        is_num(-1, len);
    }
    ++rnum;
    printf(" Length is %d\n", (int)len);
  }

  free_basic_handles(&henv1, &hdbc1, &hstmt1);
  ok_sql(hstmt, "DROP TABLE IF EXISTS bug106683");

  return OK;
}


BEGIN_TESTS
  ADD_TEST(t_bug106683)
  ADD_TEST(my_json)
  ADD_TEST(my_local_infile)
END_TESTS


RUN_TESTS
