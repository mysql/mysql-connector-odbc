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

DECLARE_TEST(my_json)
{
  SQLINTEGER id_value = 1;
  SQLLEN out_bytes[3] = {0, 0, 0};
  SQLCHAR buf[255], qbuf[255];
  DECLARE_BASIC_HANDLES(henv2, hdbc2, hstmt2);
  alloc_basic_handles_with_opt(&henv2, &hdbc2, &hstmt2, NULL, NULL, NULL, NULL,
                               (SQLCHAR*)"CHARSET=utf8mb4");

  const char *json = "{\"key1\": \"value1\", \"key2\": \"value2\"}";
  const char *json2 = "{\"key1\": \"value10\", \"key2\": \"value20\"}";

  ok_sql(hstmt2, "DROP TABLE IF EXISTS t_bug_json");
  ok_sql(hstmt2, "CREATE TABLE t_bug_json(id INT PRIMARY KEY, jdoc JSON)");

  snprintf((char*)qbuf, 255, "INSERT INTO t_bug_json VALUES (1, '%s')", json);
  ok_stmt(hstmt2, SQLExecDirect(hstmt2, qbuf, SQL_NTS));

  ok_stmt(hstmt2, SQLColumns(hstmt2, mydb, SQL_NTS, NULL, 0,
                            (SQLCHAR *)"t_bug_json", SQL_NTS, NULL, 0));

  ok_stmt(hstmt2, SQLFetch(hstmt2)); // Fetch 1st column and ignore data
  ok_stmt(hstmt2, SQLFetch(hstmt2)); // Fetch 2nd column

  is_num(my_fetch_int(hstmt2, 5), -1);             // DATA_TYPE
  is_str(my_fetch_str(hstmt2, buf, 6), "json", 4); // TYPE_NAME

  expect_stmt(hstmt2, SQLFetch(hstmt2), SQL_NO_DATA);
  ok_stmt(hstmt2, SQLFreeStmt(hstmt2, SQL_CLOSE));
  ok_stmt(hstmt2, SQLPrepare(hstmt2, (SQLCHAR*)"SELECT id, jdoc FROM t_bug_json WHERE id=?",
                            SQL_NTS));
  ok_stmt(hstmt2, SQLBindParameter(hstmt2, 1, SQL_PARAM_INPUT, SQL_C_DEFAULT,
                                  SQL_INTEGER, 10, 0, &id_value, 0, &out_bytes[0]));
  ok_stmt(hstmt2, SQLExecute(hstmt2));
  ok_stmt(hstmt2, SQLFetch(hstmt2));
  is_num(my_fetch_int(hstmt2, 1), 1);
  is_str(my_fetch_str(hstmt2, buf, 2), json, strlen(json));
  expect_stmt(hstmt2, SQLFetch(hstmt2), SQL_NO_DATA);

  ok_stmt(hstmt2, SQLFreeStmt(hstmt2, SQL_CLOSE));
  ok_stmt(hstmt2, SQLPrepare(hstmt2, (SQLCHAR*)"UPDATE t_bug_json SET jdoc = ? "\
                                   "WHERE id = ?", SQL_NTS));

  out_bytes[0] = strlen(json2);
  ok_stmt(hstmt2, SQLBindParameter(hstmt2, 1, SQL_PARAM_INPUT, SQL_C_CHAR,
                                  SQL_LONGVARCHAR, 0, 0, (SQLPOINTER)json2, strlen(json2),
                                  &out_bytes[0]));

  ok_stmt(hstmt2, SQLBindParameter(hstmt2, 2, SQL_PARAM_INPUT, SQL_C_DEFAULT,
                                  SQL_INTEGER, 10, 0, &id_value, 0, &out_bytes[1]));

  ok_stmt(hstmt2, SQLExecute(hstmt2));

  ok_stmt(hstmt2, SQLPrepare(hstmt2, (SQLCHAR*)"SELECT id, jdoc FROM t_bug_json WHERE id=?",
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

  if (strcmp((char*)myserver, "localhost"))
    return OK;

  DECLARE_BASIC_HANDLES(henv1, hdbc1, hstmt1);

  ok_sql(hstmt, "DROP TABLE IF EXISTS test_local_infile");
  ok_sql(hstmt, "CREATE TABLE test_local_infile (id int, vc varchar(32))");

  csv_file = fopen("test_local_infile.csv", "w");
  if (csv_file == NULL) goto END_TEST;

  fputs("1,abc\n2,def\n3,ghi",csv_file);
  fclose(csv_file);

  ok_sql(hstmt, "SET GLOBAL local_infile=true");
  rc = SQLExecDirect(hstmt, (SQLCHAR*)"LOAD DATA LOCAL INFILE 'test_local_infile.csv' " \
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
                                    (SQLCHAR*)"ENABLE_LOCAL_INFILE=1");
  if (rc != SQL_SUCCESS) goto END_TEST;

  rc = SQLExecDirect(hstmt1, (SQLCHAR*)
                     "LOAD DATA LOCAL INFILE 'test_local_infile.csv' " \
                     "INTO TABLE test_local_infile " \
                     "FIELDS TERMINATED BY ',' " \
                     "LINES TERMINATED BY '\\n'", SQL_NTS);

  if (rc != SQL_SUCCESS)
  {
      print_diag(rc, SQL_HANDLE_STMT, hstmt1, "LOAD DATA LOCAL INFILE FAILED", __FILE__, __LINE__);
      goto END_TEST;
  }

  rc = SQLExecDirect(hstmt, (SQLCHAR*)"SELECT count(*) FROM test_local_infile", SQL_NTS);
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
                   (SQLCHAR*)"NO_CACHE=1"));

  ok_stmt(hstmt1, SQLSetStmtAttr(hstmt1, SQL_ATTR_CURSOR_TYPE,
    (SQLPOINTER)SQL_CURSOR_FORWARD_ONLY, SQL_IS_UINTEGER));
  ok_stmt(hstmt1, SQLPrepare(hstmt1, (SQLCHAR*)"SELECT * FROM bug106683", SQL_NTS));
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


DECLARE_TEST(t_bug34109678_collations)
{
  if (!mysql_min_version(hdbc, "8.0.30", 6))
    skip("server does not support the additional collations");

  try
  {
    std::vector<std::string> collations = {
      "utf8mb4_nb_0900_ai_ci",
      "utf8mb4_nb_0900_as_cs",
      "utf8mb4_nn_0900_ai_ci",
      "utf8mb4_nn_0900_as_cs",
      "utf8mb4_sr_latn_0900_ai_ci",
      "utf8mb4_sr_latn_0900_as_cs",
      "utf8mb4_bs_0900_ai_ci",
      "utf8mb4_bs_0900_as_cs",
      "utf8mb4_bg_0900_ai_ci",
      "utf8mb4_bg_0900_as_cs",
      "utf8mb4_gl_0900_ai_ci",
      "utf8mb4_gl_0900_as_cs",
      "utf8mb4_mn_cyrl_0900_ai_ci",
      "utf8mb4_mn_cyrl_0900_as_cs",
      "utf8mb3_unicode_ci",
      "utf8mb3_icelandic_ci",
      "utf8mb3_latvian_ci",
      "utf8mb3_romanian_ci",
      "utf8mb3_slovenian_ci",
      "utf8mb3_polish_ci",
      "utf8mb3_estonian_ci",
      "utf8mb3_spanish_ci",
      "utf8mb3_swedish_ci",
      "utf8mb3_turkish_ci",
      "utf8mb3_czech_ci",
      "utf8mb3_danish_ci",
      "utf8mb3_lithuanian_ci",
      "utf8mb3_slovak_ci",
      "utf8mb3_spanish2_ci",
      "utf8mb3_roman_ci",
      "utf8mb3_persian_ci",
      "utf8mb3_esperanto_ci",
      "utf8mb3_hungarian_ci",
      "utf8mb3_sinhala_ci",
      "utf8mb3_german2_ci",
      "utf8mb3_croatian_ci",
      "utf8mb3_unicode_520_ci",
      "utf8mb3_vietnamese_ci",
      "utf8mb3_general_mysql500_ci"
    };

    odbc::connection conn(nullptr, nullptr, nullptr, nullptr, "PAD_SPACE=1;");
    SQLHSTMT hstmt1 = conn.hstmt;

    std::string tab_name = "t_bug34109678";
    std::string fields = "id int primary key auto_increment";
    std::string ins_query = "(NULL";
    for (auto s : collations)
    {
      fields.append(",col_" + s + " CHAR(3) COLLATE " + s);
      ins_query.append(",'abc'");
    }
    ins_query.append(")");
    odbc::table tab(hstmt, tab_name, fields);
    tab.insert(ins_query);
    odbc::stmt_prepare(hstmt1, "SELECT * FROM " + tab_name);
    odbc::stmt_execute(hstmt1);

    size_t elem_num = collations.size() + 1;
    std::vector<odbc::xbuf> databuf;
    std::vector<SQLLEN> lenbuf;
    databuf.reserve(elem_num);
    lenbuf.reserve(elem_num);
    for (int i = 0; i < collations.size() + 1; ++i)
    {
      databuf.emplace_back(odbc::xbuf(16));
      lenbuf.emplace_back(16);
      ok_stmt(hstmt1, SQLBindCol(hstmt1, i + 1, SQL_C_CHAR, (char*)databuf[i], 16, &lenbuf[0]));
    }

    is(SQLFetch(hstmt1) == SQL_SUCCESS);
    for (int i = 1; i < elem_num; ++i)
    {
      is_xstr(databuf[i].get_str(), "abc");
    }
    is_no_data(SQLFetch(hstmt1));
    odbc::stmt_reset(hstmt1);
  }
  ENDCATCH;
}

DECLARE_TEST(t_bug34397870_ubigint)
{
  try
  {
    odbc::table tab(hstmt, "t_bug34397870", "col1 BIGINT UNSIGNED");
    SQLUBIGINT vals[] = { 0xEFFFFFFFFFFFFFFFU, 0xFFFFFFFFFFFFFFFFU };
    SQLUBIGINT val = 0, res = 0;
    SQLLEN len = 0;
    odbc::stmt_prepare(hstmt, "INSERT INTO t_bug34397870 VALUES (?)");
    ok_stmt(hstmt, SQLBindParameter(hstmt, 1, SQL_PARAM_INPUT, SQL_C_UBIGINT,
      SQL_BIGINT, 20, 0, &val, 20, NULL));
    for (auto v : vals)
    {
      val = v;
      odbc::stmt_execute(hstmt);
    }
    odbc::stmt_close(hstmt);
    odbc::stmt_reset(hstmt);

    odbc::stmt_prepare(hstmt, "SELECT * FROM t_bug34397870");
    odbc::stmt_execute(hstmt);
    ok_stmt(hstmt, SQLBindCol(hstmt, 1, SQL_C_UBIGINT, &res, sizeof(res), &len));
    int rownum = 0;
    while (SQL_SUCCESS == SQLFetch(hstmt))
    {
      is_num(vals[rownum], res);
      ++rownum;
    }
    is_num(2, rownum);
    odbc::stmt_close(hstmt);

    odbc::sql(hstmt, "SELECT * FROM t_bug34397870");
    rownum = 0;
    while (SQL_SUCCESS == SQLFetch(hstmt))
    {
      ok_stmt(hstmt, SQLGetData(hstmt, 1, SQL_C_UBIGINT, &res, sizeof(res), &len));
      is_num(vals[rownum], res);
      ++rownum;
    }
    is_num(2, rownum);

    odbc::stmt_close(hstmt);
  }
  ENDCATCH;
}

BEGIN_TESTS
  ADD_TEST(t_bug34397870_ubigint)
  ADD_TEST(t_bug34109678_collations)
  ADD_TEST(t_bug106683)
  ADD_TEST(my_json)
  ADD_TEST(my_local_infile)
END_TESTS


RUN_TESTS
