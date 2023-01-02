// Copyright (c) 2018, 2022 Oracle and/or its affiliates. All rights reserved.
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
#include "../VersionInfo.h"

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

#if !MYSQLCLIENT_STATIC_LINKING
  skip("Enable when bug#34814863 is fixed!");
#endif

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
      databuf.emplace_back(odbc::xbuf(32));
      lenbuf.emplace_back(32);
      ok_stmt(hstmt1, SQLBindCol(hstmt1, i + 1, SQL_C_CHAR, (char*)databuf[i], 32, &lenbuf[0]));
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

/*
  Template function to test how SQLGetData() works with
  partial fetches on ANSI and WIDE string data.

  The following scenarios are assumed:

  - Insert the same long character string into 2 columns of
    `t_sqlgetdata` table: blob column `mb_data` and text
    column `tx_data`.

  - Get 3 values from table `t_sqlgetdata`:
   using

   1 value of `mb_data`
   2 value of `tx_data`
   3 value of `mb_data` converted to hex string using SQL
   `HEX()` function

  - Fetch the values using SQLGetData() and requesting
    conversion to SQL_C_(W)CHAR.

  - In the first call to SQLGetData() use buffer which
    is too small to hold values. Verify that:

    -- reported total data length is as expected
    -- the truncated data in the output buffer is null-terminated

  - Call SQLGetData() again to fetch remaining data - use:
    buffer big enough to hold all of it. Verify that:

    -- second call to SQLGetData() reports all data fetched
    -- the data for 2nd value (`tx_data`) equals the original string
    -- data for values 1 and 3 are identical.
*/
template <typename T>
int check_sqlgetdata_buf(SQLHSTMT hstmt, std::string &src_str)
{
  odbc::table tab(hstmt, "t_sqlgetdata",
    "id int, mb_data mediumblob, tx_data mediumtext");

  tab.insert("(1, '" + src_str + "', '" + src_str + "')");

  SQLSMALLINT result_type = SQL_C_CHAR;
  size_t char_size = sizeof(T);

  if (unicode_driver == 1)
  {
    result_type = SQL_C_WCHAR;
  }

  const size_t buf_size = 128 + 1;

  // Get a buffer, which is too small for all data
  T buf[3][buf_size];
  SQLLEN   cb_buf[3] = { SQL_NTS, SQL_NTS, SQL_NTS };
  std::unique_ptr<T> row_data[3];

  odbc::sql(hstmt, "SELECT mb_data, tx_data, "
    "CAST(HEX(mb_data) AS CHAR) hex_data FROM t_sqlgetdata");

  ok_stmt(hstmt, SQLFetch(hstmt));

  for (int i = 0; i < 3; ++i)
  {
    SQLRETURN retcode = SQLGetData(hstmt, i + 1, result_type, buf[i],
      buf_size * char_size, &cb_buf[i]);
    is_num(SQL_SUCCESS_WITH_INFO, retcode);

    if (i == 1)
      // This column has the data as text
      is_num(src_str.length() * char_size, cb_buf[i]);
    else
      // Everything else has data as hex codes
      is_num(src_str.length() * char_size * 2, cb_buf[i]);

    size_t row_data_len = (cb_buf[i] / char_size) + 1;
    row_data[i].reset(new T[row_data_len]);
    T* data_ptr = row_data[i].get();

    is(cb_buf[i] >= buf_size * char_size);
    size_t row_data_offset = buf_size;

    memcpy(data_ptr, buf[i], buf_size * char_size);
    // Even the partial data read must have the null-termination
    is_num(0, data_ptr[buf_size - 1]);

    T* target = data_ptr + row_data_offset - 1;
    SQLULEN buflen = (row_data_len - row_data_offset + 1) * char_size;
    retcode = SQLGetData(hstmt, i + 1, result_type, target, buflen, nullptr);
    is_num(SQL_SUCCESS, retcode);
  }

  T* mb_data = row_data[0].get();
  T* tx_data = row_data[1].get();
  T* hex_data = row_data[2].get();

  is_num(0, memcmp(mb_data, hex_data, cb_buf[0]));

  if (unicode_driver == 1)
  {
    std::wstring wsrc_str(src_str.begin(), src_str.end());
    SQLWCHAR *w = WL((wchar_t*)wsrc_str.c_str(), wsrc_str.length() + 1);
    is_num(0, memcmp(w, tx_data, cb_buf[1]));
  }
  else
  {
    std::string str_tx_data((char*)tx_data);
    is(str_tx_data == src_str);
  }
  return OK;
}

/*
  Testing partial data fetch in SQLGetData().
  The data can be fetched in chunks if the user app buffer
  is too small to hold all data. In this case each new call
  to SQLGetData() returns a new chunk.

  Two scenarios have to be tested: ANSI with SQLCHAR and
  UNICODE with SQLWCHAR. That is when the Driver Manager does
  not need to convert data.

  In case when conversion is required such as ANSI driver and
  SQLWCHAR data the Driver Manager does not allow fetching all
  data in chunks (looks like it is implementation specific).
  Such scenarios are not tested because the ODBC driver has no
  control over the conversion flow.
*/
DECLARE_TEST(t_sqlgetdata_buf)
{
  std::string src_str;

  const int max_repeat = 50;
  src_str.reserve(26 * max_repeat);
  for (int i = 0; i < max_repeat; ++i)
    src_str.append("abcdefghijklmnopqrstuvwxyz");

  if (unicode_driver == 1)
  {
    is_num(SQL_SUCCESS, check_sqlgetdata_buf<SQLWCHAR>(hstmt, src_str));
  }
  else
  {
    is_num(SQL_SUCCESS, check_sqlgetdata_buf<SQLCHAR>(hstmt, src_str));
  }

  return OK;
}

/*
  Bug #33401384 - ODBC parameter not properly replaced when
  put into JOIN/VALUES statement.
*/
DECLARE_TEST(t_bug33401384_JSON_param)
{
  if (!mysql_min_version(hdbc, "8.0.31", 6))
    skip("Server does not have the fix for the problem");

  try
  {
    odbc::table tab(hstmt, "tab_bug33401384",
      "id INT NOT NULL, value VARCHAR(100) NULL");
    tab.insert("(1, 'A')");

    const char* upd_str = "UPDATE tab_bug33401384 ut " \
      "INNER JOIN ( VALUES ROW( ? , ? ) ) vt (id,value) " \
      "ON (ut.id = vt.id) " \
      "SET ut.value = vt.value";

    const char* par[] = { "1", "B" };
    SQLLEN out_bytes[] = { 1, 1 };

    odbc::stmt_prepare(hstmt, upd_str);

    ok_stmt(hstmt, SQLBindParameter(hstmt, 1, SQL_PARAM_INPUT, SQL_C_CHAR,
      SQL_VARCHAR, 0, 0, (SQLPOINTER)par[0], 1, &out_bytes[0]));

    ok_stmt(hstmt, SQLBindParameter(hstmt, 2, SQL_PARAM_INPUT, SQL_C_CHAR,
      SQL_VARCHAR, 0, 0, (SQLPOINTER)par[1], 1, &out_bytes[1]));

    odbc::stmt_execute(hstmt);
    odbc::stmt_close(hstmt);

    odbc::sql(hstmt, "SELECT value FROM tab_bug33401384");
    ok_stmt(hstmt, SQLFetch(hstmt));
    SQLCHAR buf[10];
    my_fetch_str(hstmt, buf, 1);
    is_str(buf, "B", 2);
    is_no_data(SQLFetch(hstmt));
    odbc::stmt_close(hstmt);
  }
  ENDCATCH;
}

DECLARE_TEST(t_conn_attr_data)
{
  try
  {
    odbc::xbuf buf(64);
    odbc::select_one_str(hstmt, "SELECT CONNECTION_ID()", buf, 1);
    odbc::xstring query = "SELECT ATTR_NAME, ATTR_VALUE FROM "
      "performance_schema.session_connect_attrs WHERE "
      "(ATTR_NAME='_connector_license' OR "
      "ATTR_NAME='_connector_name' OR "
      "ATTR_NAME='_connector_type' OR "
      "ATTR_NAME='_connector_version') AND "
      "PROCESSLIST_ID=" + buf.get_str() +
      " ORDER BY ATTR_NAME";
    odbc::sql(hstmt, query);

    std::string attr_list[][2] = {
      {"_connector_license", MYODBC_LICENSE},
      {"_connector_name", "mysql-connector-odbc"},
      {"_connector_type", unicode_driver == 0 ? "ANSI" : "Unicode"},
      {"_connector_version", MYODBC_CONN_ATTR_VER}
    };

    int idx = 0;
    while (SQL_SUCCESS == SQLFetch(hstmt))
    {
      // Compare ATTR_NAME
      my_fetch_str(hstmt, buf, 1);
      is_num(0, attr_list[idx][0].compare(buf));
      // Compare ATTR_VALUE
      my_fetch_str(hstmt, buf, 2);
      is_num(0, attr_list[idx][1].compare(buf));
      ++idx;
    }
    is_num(4, idx);
  }
  ENDCATCH;
}

BEGIN_TESTS
  ADD_TEST(t_conn_attr_data)
  ADD_TEST(t_bug33401384_JSON_param)
  ADD_TEST(t_sqlgetdata_buf)
  ADD_TEST(t_bug34397870_ubigint)
  ADD_TEST(t_bug34109678_collations)
  ADD_TEST(t_bug106683)
  ADD_TEST(my_json)
  ADD_TEST(my_local_infile)
END_TESTS


RUN_TESTS
