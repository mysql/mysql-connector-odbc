// Copyright (c) 2018, Oracle and/or its affiliates. All rights reserved.
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

DECLARE_TEST(my_local_infile)
{
  SQLRETURN rc = 0;
  SQLINTEGER num_rows = 0;
  FILE *csv_file = NULL;

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


BEGIN_TESTS
  ADD_TEST(my_local_infile)
END_TESTS


RUN_TESTS
