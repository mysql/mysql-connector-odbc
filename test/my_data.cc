// Copyright (c) 2018, 2026, Oracle and/or its affiliates.
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License, version 2.0, as
// published by the Free Software Foundation.
//
// This program is designed to work with certain software (including
// but not limited to OpenSSL) that is licensed under separate terms, as
// designated in a particular file or component or in included license
// documentation. The authors of MySQL hereby grant you an additional
// permission to link the program and your derivative works with the
// separately licensed software that they have either included with
// the program or referenced in the documentation.
//
// Without limiting anything contained in the foregoing, this file,
// which is part of Connector/ODBC, is also subject to the
// Universal FOSS Exception, version 1.0, a copy of which can be found at
// https://oss.oracle.com/licenses/universal-foss-exception.
//
// This program is distributed in the hope that it will be useful, but
// WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
// See the GNU General Public License, version 2.0, for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software Foundation, Inc.,
// 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA

#include <algorithm>
#include <string>
#include <vector>
#include <chrono>
#include <locale>
#include <sstream>
#include <iomanip>


#include "odbc_util.h"
#include "../VersionInfo.h"

DECLARE_TEST(my_json)
{
  SQLINTEGER id_value = 1;
  SQLLEN out_bytes[3] = {0, 0, 0};
  char buf[255], qbuf[255];
  DECLARE_BASIC_HANDLES(henv2, hdbc2, hstmt2);
  alloc_basic_handles_with_opt(&henv2, &hdbc2, &hstmt2, NULL, NULL, NULL, NULL,
                               "CHARSET=utf8mb4");

  const char *json = "{\"key1\": \"value1\", \"key2\": \"value2\"}";
  const char *json2 = "{\"key1\": \"value10\", \"key2\": \"value20\"}";

  ok_sql(hstmt2, "DROP TABLE IF EXISTS t_bug_json");
  ok_sql(hstmt2, "CREATE TABLE t_bug_json(id INT PRIMARY KEY, jdoc JSON)");

  snprintf(qbuf, 255, "INSERT INTO t_bug_json VALUES (1, '%s')", json);
  ok_stmt(hstmt2, SQLExecDirect(hstmt2, SC_NTS(qbuf)));

  ok_stmt(hstmt2, SQLColumns(hstmt2, SC_NTS(mydb), NULL, 0,
                            SC_NTS("t_bug_json"), NULL, 0));

  ok_stmt(hstmt2, SQLFetch(hstmt2)); // Fetch 1st column and ignore data
  ok_stmt(hstmt2, SQLFetch(hstmt2)); // Fetch 2nd column

  int exp_type = unicode_driver ? SQL_WLONGVARCHAR : SQL_LONGVARCHAR;
  is_num(my_fetch_int(hstmt2, 5), exp_type);       // DATA_TYPE
  is_str(my_fetch_str(hstmt2, buf, 6), "json", 4); // TYPE_NAME

  expect_stmt(hstmt2, SQLFetch(hstmt2), SQL_NO_DATA);
  ok_stmt(hstmt2, SQLFreeStmt(hstmt2, SQL_CLOSE));
  ok_stmt(hstmt2, SQLPrepare(hstmt2, SC_NTS("SELECT id, jdoc FROM t_bug_json WHERE id=?")));
  ok_stmt(hstmt2, SQLBindParameter(hstmt2, 1, SQL_PARAM_INPUT, SQL_C_DEFAULT,
                                  SQL_INTEGER, 10, 0, &id_value, 0, &out_bytes[0]));
  ok_stmt(hstmt2, SQLExecute(hstmt2));
  ok_stmt(hstmt2, SQLFetch(hstmt2));
  is_num(my_fetch_int(hstmt2, 1), 1);
  is_str(my_fetch_str(hstmt2, buf, 2), json, strlen(json));
  expect_stmt(hstmt2, SQLFetch(hstmt2), SQL_NO_DATA);

  ok_stmt(hstmt2, SQLFreeStmt(hstmt2, SQL_CLOSE));
  ok_stmt(hstmt2, SQLPrepare(hstmt2, SC_NTS("UPDATE t_bug_json SET jdoc = ? "\
    "WHERE id = ?")));

  out_bytes[0] = strlen(json2);
  ok_stmt(hstmt2, SQLBindParameter(hstmt2, 1, SQL_PARAM_INPUT, SQL_C_CHAR,
                                  SQL_LONGVARCHAR, 0, 0, (SQLPOINTER)json2, strlen(json2),
                                  &out_bytes[0]));

  ok_stmt(hstmt2, SQLBindParameter(hstmt2, 2, SQL_PARAM_INPUT, SQL_C_DEFAULT,
                                  SQL_INTEGER, 10, 0, &id_value, 0, &out_bytes[1]));

  ok_stmt(hstmt2, SQLExecute(hstmt2));

  ok_stmt(hstmt2, SQLPrepare(hstmt2, SC_NTS("SELECT id, jdoc FROM t_bug_json WHERE id=?")));
  out_bytes[0] = 0;
  ok_stmt(hstmt2, SQLBindParameter(hstmt2, 1, SQL_PARAM_INPUT, SQL_C_DEFAULT,
                                  SQL_INTEGER, 10, 0, &id_value, 0, &out_bytes[0]));
  ok_stmt(hstmt2, SQLExecute(hstmt2));
  ok_stmt(hstmt2, SQLFetch(hstmt2));
  is_num(my_fetch_int(hstmt2, 1), 1);
  is_str(my_fetch_str(hstmt2, buf, 2), json2, strlen(json2));
  expect_stmt(hstmt2, SQLFetch(hstmt2), SQL_NO_DATA);

  ok_stmt(hstmt2, SQLFreeStmt(hstmt2, SQL_CLOSE));

  odbc::stmt_reset(hstmt2);
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
  rc = SQLExecDirect(hstmt, SC_NTS("LOAD DATA LOCAL INFILE 'test_local_infile.csv' " \
                    "INTO TABLE test_local_infile " \
                    "FIELDS TERMINATED BY ',' " \
                    "LINES TERMINATED BY '\\n'"));
  if (rc != SQL_ERROR)  // First attempt must fail
  {
    rc = SQL_ERROR;
    goto END_TEST;
  }

  rc = alloc_basic_handles_with_opt(&henv1, &hdbc1, &hstmt1,
                                    NULL, NULL, NULL, NULL,
                                    "ENABLE_LOCAL_INFILE=1");
  if (rc != SQL_SUCCESS) goto END_TEST;

  rc = SQLExecDirect(hstmt1, SC_NTS(
                     "LOAD DATA LOCAL INFILE 'test_local_infile.csv' " \
                     "INTO TABLE test_local_infile " \
                     "FIELDS TERMINATED BY ',' " \
                     "LINES TERMINATED BY '\\n'"));

  if (rc != SQL_SUCCESS)
  {
      print_diag(rc, SQL_HANDLE_STMT, hstmt1, "LOAD DATA LOCAL INFILE FAILED", __FILE__, __LINE__);
      goto END_TEST;
  }

  rc = SQLExecDirect(hstmt, SC_NTS("SELECT count(*) FROM test_local_infile"));
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
  ok_stmt(hstmt1, SQLPrepare(hstmt1, SC_NTS("SELECT * FROM bug106683")));
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
      ok_stmt(hstmt1, SQLBindCol(hstmt1, i + 1, SQL_C_CHAR, databuf[i], 32, &lenbuf[0]));
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
  std::unique_ptr<T[]> row_data[3];

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

    is((size_t)cb_buf[i] >= buf_size * char_size);
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
    odbc::stmt_reset(hstmt);

    odbc::sql(hstmt, "SELECT value FROM tab_bug33401384");
    ok_stmt(hstmt, SQLFetch(hstmt));
    char buf[10];
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


DECLARE_TEST(t_wl15423_json)
{
  try
  {
    odbc::table tab(hstmt, "wl15423", "jdoc JSON, txt LONGTEXT");
    odbc::xstring json = "{\"key1\": \"value1\", \"key2\": \"value2\"}";
    tab.insert("('" + json + "', 'ABC')");

    long long exp_type = unicode_driver ? SQL_WLONGVARCHAR : SQL_LONGVARCHAR;

    // Type information.

    {
      bool found = false;

      ok_stmt(hstmt, SQLGetTypeInfo(hstmt, (SQLSMALLINT)exp_type));

      while (SQL_SUCCESS == SQLFetch(hstmt))
      {
        odbc::xbuf buf(32);
        my_fetch_str(hstmt, buf, 1);

        if (buf.get_str().compare("json"))
          continue;

        found = true;

        auto data_type = my_fetch_int(hstmt, 2);
        is_num(data_type, exp_type);

        auto sql_data_type = my_fetch_int(hstmt, 16);
        is_num(sql_data_type, exp_type);

        auto column_size = my_fetch_int(hstmt, 3);
        is_num(column_size, UINT32_MAX/4);

        auto case_sensitive = my_fetch_int(hstmt, 8);
        is_num(case_sensitive, SQL_TRUE);

        auto searchable = my_fetch_int(hstmt, 9);
        is_num(searchable, SQL_PRED_CHAR);

        break;
      }

      odbc::stmt_close(hstmt);
      is_num(found, true);
    }

    // Table column info.

    {
      ok_stmt(hstmt, SQLColumns(hstmt, nullptr, 0, nullptr, 0, tab.table_name,
        SQL_NTS, nullptr, 0));

      // Fetch information oabut first column (JSON).

      ok_stmt(hstmt, SQLFetch(hstmt));

      odbc::xbuf type_name(16);
      my_fetch_str(hstmt, type_name, 6);
      is_str(type_name, "json", 4);

      auto data_type = my_fetch_int(hstmt, 5);
      is_num(data_type, exp_type);

      auto sql_data_type = my_fetch_int(hstmt, 14);
      is_num(sql_data_type, exp_type);

      auto column_size = my_fetch_uint(hstmt, 7);
      is_num(column_size, UINT32_MAX/4);

      auto buffer_length = my_fetch_uint(hstmt, 8);
      is_num(buffer_length, UINT32_MAX);

      auto char_octet_length = my_fetch_uint(hstmt, 16);
      is_num(char_octet_length, UINT32_MAX);

      odbc::stmt_close(hstmt);
    }

    // query result meta-data.

    {
      odbc::sql(hstmt, "SELECT * FROM " + tab.table_name);

      // Get info about 1st column in the result.

      char col_name[20];
      SQLSMALLINT data_type = 0;
      SQLULEN col_size = 0;

      ok_stmt(hstmt, SQLDescribeCol(hstmt, 1,
        SC(col_name), 20, nullptr, &data_type, &col_size,
        nullptr, nullptr));

      is_num(data_type, exp_type);
      is_num(col_size, UINT32_MAX / 4);

      while (SQL_SUCCESS == SQLFetch(hstmt))
      {
        odbc::xbuf buf(128);
        my_fetch_data(hstmt, buf, 1);
        is_num(0, json.compare(buf));
      }

      // check descriptors for the first column.

      char str_attr[20];
      SQLLEN num_attr;

      auto get_attr = [&str_attr, &num_attr, &hstmt](auto idx) {
        num_attr = 0;
        str_attr[0] = 0;
        ok_stmt(hstmt, SQLColAttribute(
          hstmt, 1, idx, str_attr, 20, nullptr, &num_attr
        ));
      };

      get_attr(SQL_DESC_TYPE_NAME);
      is_str(str_attr,"json",4);

      get_attr(SQL_DESC_LOCAL_TYPE_NAME);
      is_str(str_attr,"",0);

      get_attr(SQL_DESC_TYPE);
      is_num(num_attr, exp_type);

      get_attr(SQL_DESC_CONCISE_TYPE);
      is_num(num_attr, exp_type);

      get_attr(SQL_DESC_LENGTH);
      is_num(num_attr, UINT32_MAX/4);

      get_attr(SQL_DESC_DISPLAY_SIZE);
      is_num(num_attr, UINT32_MAX/4);

      get_attr(SQL_DESC_CASE_SENSITIVE);
      is_num(num_attr, SQL_TRUE);

      get_attr(SQL_DESC_SEARCHABLE);
      is_num(num_attr, SQL_PRED_CHAR);
    }
  }
  ENDCATCH;
}

// Bug #33353465: ODBC JSON utf8mb4 support
DECLARE_TEST(t_bug33353465_json_utf8mb4) {
  try {
    odbc::connection con(nullptr, nullptr, nullptr, nullptr, "CHARSET=utf8mb4");
    odbc::HSTMT hstmt(con);
    odbc::table tab(hstmt, mydb, "json_utf8mb4",
      "jcol JSON, tcol TEXT", "DEFAULT CHARSET=UTF8MB4");
    odbc::xbuf data(255);

    // This is a JSON string containing UTF8MB4 characters.
    const char *insert_val =
        "{\"entry_id\": 1, "
        "\"entry_title\": \"Za\xC5\xBC\xC3\xB3\xC5\x82\xC4\x87 "
        "g\xC4\x99\xC5\x9Bl\xC4\x85 ja\xC5\xBA\xC5\x84\"}";

    // Do inserts as binary parameters to avoid possibility of conversion.
    odbc::stmt_prepare(hstmt, "INSERT INTO json_utf8mb4 VALUES (?, ?)");
    for (int i = 1; i < 3; ++i) {
      ok_stmt(hstmt, SQLBindParameter(hstmt, i,
        SQL_PARAM_INPUT, SQL_C_BINARY,
        SQL_BINARY, 0, 0, (SQLPOINTER)insert_val,
        strlen(insert_val), nullptr));
    }
    odbc::stmt_execute(hstmt);
    odbc::stmt_reset(hstmt);
    // Check the JSON and TEXT data inserted in the table.
    // Both should be identical.
    odbc::sql(hstmt, "SELECT * FROM " + tab.table_name);
    while (SQL_SUCCESS == SQLFetch(hstmt)) {
      for (int i = 1; i < 3; ++i)
      {
        is_str(insert_val, my_fetch_str(hstmt, data, i), SQL_NTS);
      }
    }
  }
  ENDCATCH;
}

// Bug 34350417 - ODBC Massive Performance hit
DECLARE_TEST(t_bug_34350417_performance) {
  // Save the original locale.
  char *default_locale = setlocale(LC_NUMERIC, nullptr);
  int res = OK;
  struct lconv *tmp;

  // Set locale with comma as decimal point.
  setlocale(LC_NUMERIC, "pl_PL");

  tmp = localeconv();
  odbc::xstring decimal_point = tmp->decimal_point;
  std::cout << "Decimal point: [" << decimal_point << "]\n";

  try {
    odbc::table tab(hstmt, "perf_tab",
      "c1 VARCHAR(32), c2 DOUBLE");

    odbc::stmt_prepare(hstmt, "INSERT INTO " + tab.table_name +
      "(c1,c2)VALUES(?,?)");

    const char *c_val = "4.2345678902345678e+20";
    double d_val = 4.2345678902345678e+20;
    char c_res[64];
    double d_res = 0;

    // Make driver convert DOUBLE into CHAR.
    ok_stmt(hstmt, SQLBindParameter(hstmt, 1, SQL_PARAM_INPUT, SQL_C_DOUBLE,
      SQL_CHAR, 10, 4, &d_val, 0, nullptr));

    // Make driver convert CHAR into DOUBLE.
    ok_stmt(hstmt, SQLBindParameter(hstmt, 2, SQL_PARAM_INPUT, SQL_C_CHAR,
      SQL_DOUBLE, 0, 0, (SQLPOINTER)c_val, strlen(c_val), nullptr));

    odbc::stmt_execute(hstmt);
    odbc::stmt_close(hstmt);
    odbc::stmt_reset(hstmt);

    odbc::sql(hstmt, "SELECT * FROM " + tab.table_name);
    while (SQL_SUCCESS == SQLFetch(hstmt)) {
      for (int i = 1; i < 3; ++i) {
        // Check CHAR conversion
        ok_stmt(hstmt, SQLGetData(hstmt, i, SQL_C_CHAR, c_res,
          sizeof(c_res), nullptr));
        // Skip comparing last digits in fraction that might be inexact.
        is_str(c_val, c_res, 17);
        // Check that the power is correct.
        char *power = c_res + strlen(c_res) - 2;
        is_str(power, "20", 2);
      }
    }
    odbc::stmt_close(hstmt);

    // Need to run another query to get data as DOUBLE
    // because SQLGetData() can only be called once per column per row.
    odbc::sql(hstmt, "SELECT * FROM " + tab.table_name);
    while (SQL_SUCCESS == SQLFetch(hstmt)) {
      for (int i = 1; i < 3; ++i) {
        // Check DOUBLE conversion.
        ok_stmt(hstmt, SQLGetData(hstmt, i, SQL_C_DOUBLE, &d_res,
          sizeof(d_res), nullptr));
        is(d_val == d_res);
      }
    }
    odbc::stmt_close(hstmt);

  } catch (odbc::Exception &ex) {
    std::cout << "Test failed: " << ex.msg << std::endl;
    res = FAIL;
  } catch (...) {
    res = FAIL;
  }
  setlocale(LC_NUMERIC, "");
  setlocale(LC_NUMERIC, default_locale);
  return res;
}

// Bug#35075941 - Client side prepared statements cause
// "illegal mix of collations" error.
DECLARE_TEST(t_utf8mb4_param) {
  try {
    if (!mysql_min_version(hdbc, "8.0.30", 6))
      skip("server does not support the additional collations");

    odbc::xstring opts = "NO_SSPS=1;";

    // For ANSI driver the charset must be utf8mb4
    if (!unicode_driver)
      opts.append("CHARSET=utf8mb4");

    odbc::connection con(nullptr, nullptr, nullptr, nullptr, opts);
    odbc::HSTMT hstmt1(con);

    odbc::sql(hstmt1, "set collation_connection=utf8mb4_0900_as_cs;");
    odbc::sql(hstmt1, "set default_collation_for_utf8mb4=utf8mb4_general_ci;");

    odbc::sql(hstmt1, "show variables like '%collation%'");
    my_print_non_format_result(hstmt1);

    // The table collation and collation_connection
    // should be the same (case sensitive).
    // The default_collation_for_utf8mb4 should be case insensitive
    // to allow checking for "illegal mix of collations" error.
    odbc::table tab(hstmt1, nullptr, "test_illegal_mix", "vc varchar(32)",
                    "COLLATE=utf8mb4_0900_as_cs");
    tab.insert("('aud')");

    SQLWCHAR *param = W(L"AUD");

    // Without use of variable the bug could not be reproduced.
    // For example, if parameter was directly inserted into
    // query like this ".. WHERE vc = ?"
    odbc::stmt_prepare(hstmt1, "SET @Currency = ?");
    ok_stmt(hstmt1, SQLBindParameter(hstmt1, 1, SQL_PARAM_INPUT, SQL_C_WCHAR,
                                     SQL_WCHAR, 0, 0, (SQLPOINTER)param,
                                     6 * 2, nullptr));

    odbc::stmt_execute(hstmt1);
    odbc::stmt_reset(hstmt1);

    // @Currency should use `collation_connection` collation
    // which is case sensitive and therefore 0 rows should be returned
    // when executing SELECT query.
    // Before bug#35075941 `default_collation_for_utf8mb4` was used
    // which is case insensitive and would return 1 row
    // (but even before that server errors out because of bad mix
    // of collations).
    odbc::sql(hstmt1, "SELECT * FROM " + tab.table_name +
              " WHERE vc = @Currency");

    is_num(0, my_print_non_format_result(hstmt1));
  }
  ENDCATCH;
}

// Bug#35520983 - ANSI driver and charset=sjis hangs program
DECLARE_TEST(t_bug35520983_sjis) {
  try {
    if (unicode_driver != 0)
      skip("This is only supported by ANSI driver");

    odbc::xstring opts = "CHARSET=sjis";
    odbc::connection con(nullptr, nullptr, nullptr, nullptr, opts);
    odbc::HSTMT hstmt1(con);

    odbc::table tab(hstmt1, nullptr, "t_bug35520983_sjis",
    "col1 TIMESTAMP, col2 varchar(10)",
                  "DEFAULT CHARSET=sjis");
    tab.insert("(NOW(), '\x82\xA0')");
    tab.insert("(NOW(), '\x87\x8A')");
    odbc::stmt_close(hstmt1);
    odbc::sql(hstmt1, "SELECT * FROM " + tab.table_name +
      " ORDER BY col1 ASC");

    const char* expected[] = { "\x82\xA0", "\x87\x8A" };
    int rnum = 0;
    while (SQL_SUCCESS == SQLFetch(hstmt1)) {
      char buf[10] = {0};
      SQLLEN len = 0;
      ok_stmt(hstmt, SQLGetData(hstmt1, 2, SQL_C_CHAR, buf,
        sizeof(buf), &len));
      is_num(expected[rnum][0], buf[0]);
      is_num(expected[rnum][1], buf[1]);
      is_num(2, len);
      ++rnum;
    }
    is_num(2, rnum);
    odbc::stmt_close(hstmt1);
  }
  ENDCATCH;
}

/*
  Bug #26474471/61991 - SQLGetData return shorter than expected
  string when multiple rows are fetched.

  NOTE: The issue happens with multiple rows fetch when the fetch
        positions change.
*/
DECLARE_TEST(t_bug26474471)
{
  try {
    odbc::connection conn;
    SQLHSTMT hstmt = conn.hstmt;
    odbc::table tab(hstmt, "t_bug26474471", "names VARCHAR(32)");
    odbc::xstring data[] = {"02 Str 0123456789ABCDEF", "01 Str 0123456789"};

    for(auto &d : data)
      tab.insert("('" + d + "')");

    odbc::sql(hstmt, "SELECT * FROM " + tab.table_name + " ORDER BY names DESC");

    SQLULEN rows_fetched = 0;
    ok_stmt(hstmt, SQLSetStmtAttr(hstmt, SQL_ATTR_ROWS_FETCHED_PTR,
                                  &rows_fetched, 0));

    ok_stmt(hstmt, SQLSetStmtAttr(hstmt, SQL_ATTR_ROW_ARRAY_SIZE,
                                  (SQLPOINTER)100, 0));

    ok_stmt(hstmt, SQLFetchScroll(hstmt, SQL_FETCH_NEXT, 0));
    // Two rows should be fetched at this point.
    is_num(2, rows_fetched);

    // To ensure correctness of SQLGetData() the row positions
    // will increase and decrease over iterations.
    int row_positions[] = {1, 2, 1};
    SQLLEN len = 0;
    odbc::xbuf buf(65536);

    // Check that setting the position changes the data and the length
    for (auto pos : row_positions) {
      ok_stmt(hstmt, SQLSetPos(hstmt, pos, SQL_POSITION, SQL_LOCK_NO_CHANGE));

      ok_stmt(hstmt, SQLGetData(hstmt, 1, SQL_C_CHAR, buf, buf.size, &len));

      is_num(data[pos - 1].length(), (size_t)len);
      is_str((const char *)data[pos - 1], (const char*)buf, len);
    }

  }
  ENDCATCH;
}

DECLARE_TEST(t_wl16171_vector)
{
  // Keep the original data
  std::vector<float> float_buf_orig = {
    1.2345, 2.3456E+10, 3.4567, -93.23, 0, -2, 5433.34, 3.1415,
    2.7183, 2.3456E-10, 123.987, 232, 0.000001, 3.33333333333E+0,
    10.0000009, 9.9999998
  };

  is_num(16, float_buf_orig.size());

  char char_buf[64];

  try
  {
    // Detect vector support by creating a table with VECTOR column.
    odbc::table tab_check(hstmt, "support_vector", "c1 VECTOR(16)");
  }
  catch(const odbc::Exception& e)
  {
    skip("Skipping because VECTOR support is not detected");
  }

  odbc::table tab(hstmt, "test_vector", "vector_col VECTOR(16) comment 'some comment'");

  // Check 1: Insert VECTOR value into the table.
  auto check_insert_data = [&tab](SQLHSTMT hstmt, std::vector<float> &float_buf)
  {
    odbc::stmt_close(hstmt);
    odbc::stmt_reset(hstmt);

    odbc::stmt_prepare(hstmt, "INSERT INTO " + tab.table_name + " VALUES (?)");
    size_t NN = float_buf.size();
    SQLLEN len = NN * sizeof(float);
    ok_stmt(hstmt, SQLBindParameter(hstmt, 1, SQL_PARAM_INPUT, SQL_C_BINARY,
                                    SQL_BINARY, len, 0,
                                    float_buf.data(), len,
                                    &len));
    odbc::stmt_execute(hstmt);
  };

    // Check 2: Read VECTOR value as raw bytes.
  auto check_select_data = [](SQLHSTMT hstmt, std::vector<float> &float_buf)
  {
  // For testing purposes we will not need more than 16 elements.
    float res_buf[16];
    size_t NN = float_buf.size();
    SQLLEN len = NN * sizeof(float);

    memset(res_buf, 0, sizeof(res_buf));

    odbc::stmt_close(hstmt);
    odbc::stmt_reset(hstmt);

    // Depending on NO_SSPS it will run as a query or PS
    odbc::stmt_prepare(hstmt, "SELECT * from test_vector WHERE 100=?");
    SQLINTEGER par = 100;
    ok_stmt(hstmt, SQLBindParameter(hstmt, 1, SQL_PARAM_INPUT, SQL_C_LONG,
                                    SQL_INTEGER, 0, 0,
                                    &par, 0, nullptr));

    odbc::stmt_execute(hstmt);

    ok_stmt(hstmt, SQLBindCol(hstmt, 1, SQL_C_BINARY, res_buf, NN * sizeof(float), &len));
    ok_stmt(hstmt, SQLFetch(hstmt));
    std::cout << "Float buf size: " << NN << "\n";
    for (int i = 0; i < NN; ++i)
    {
      std::cout << "[" << float_buf[i] << "," << res_buf[i] << "]\n";
      is(float_buf[i] == res_buf[i]);
    }
  };
  auto check_metadata = [&char_buf](SQLHSTMT hstmt, size_t col_cnt)
  {
    char col_name[32];
    SQLSMALLINT name_len = 0, data_type = 0, dec_digits = 0, nullable = 0;
    SQLULEN col_size = 0;

    // Check SQLDescribeCol()
    ok_stmt(hstmt, SQLDescribeCol(hstmt, 1, SC(col_name), sizeof(col_name),
        &name_len, &data_type, &col_size, &dec_digits, &nullable));

    is_num(10, name_len);
    is_str("vector_col", col_name, 10);
    is_num(0, dec_digits);
    is_num(1, nullable);
    is_num(SQL_VARBINARY, data_type);
    is_num(col_cnt, col_size);

    // Check SQLColAttribute()
    SQLLEN num_attr = 0;
    SQLSMALLINT attr_len = 0;

    auto get_attr = [&char_buf, &num_attr, &hstmt](auto idx) {
      num_attr = 0;
      char_buf[0] = 0;
      ok_stmt(hstmt, SQLColAttribute(
        hstmt, 1, idx, char_buf, sizeof(char_buf), nullptr, &num_attr
      ));
    };

    get_attr(SQL_DESC_TYPE);
    is_num(SQL_VARBINARY, num_attr);

    get_attr(SQL_DESC_CONCISE_TYPE);
    is_num(SQL_VARBINARY, num_attr);

    get_attr(SQL_DESC_TYPE_NAME);
    is_str("vector", char_buf, SQL_NTS);

    get_attr(SQL_DESC_LENGTH);
    is_num(col_cnt, num_attr);

    get_attr(SQL_DESC_OCTET_LENGTH);
    is_num(col_cnt * sizeof(float), num_attr);

    get_attr(SQL_DESC_DISPLAY_SIZE);
    is_num((col_cnt * 15) + 1, num_attr);

    get_attr(SQL_DESC_LITERAL_PREFIX);
    is(char_buf[0] == '\0');
    is_num(0, attr_len);

    get_attr(SQL_DESC_LITERAL_SUFFIX);
    is(char_buf[0] == '\0');
    is_num(0, attr_len);

    is_no_data(SQLFetch(hstmt));
    odbc::stmt_close(hstmt);
  };

  // Check SQLColumns()
  auto check_table_columns = [](SQLHSTMT hstmt, size_t col_cnt)
  {
    char char_buf[64];
    ok_stmt(hstmt, SQLColumns(hstmt, nullptr, 0, nullptr, 0,
      SC_NTS("test_vector"),
      SC_NTS("vector_col")));
    ok_stmt(hstmt, SQLFetch(hstmt));

    // TABLE_CAT
    is_str(mydb, my_fetch_str(hstmt, char_buf, 1), SQL_NTS);
    // TABLE_NAME
    is_str("test_vector", my_fetch_str(hstmt, char_buf, 3), SQL_NTS);
    // COLUMN_NAME
    is_str("vector_col", my_fetch_str(hstmt, char_buf, 4), SQL_NTS);
    // DATA_TYPE
    is_num(SQL_VARBINARY, my_fetch_int(hstmt, 5));
    // TYPE_NAME
    // NOTE: For SQLColumns() the type name is without parentheses:
    // https://learn.microsoft.com/en-us/sql/odbc/reference/syntax/sqlcolumns-function?view=sql-server-ver16
    is_str("vector", my_fetch_str(hstmt, char_buf, 6), SQL_NTS);
    // COLUMN_SIZE
    is_num(col_cnt, my_fetch_int(hstmt, 7));
    // BUFFER_LENGTH
    is_num(col_cnt * sizeof(float), my_fetch_int(hstmt, 8));
    // DECIMAL_DIGITS
    is_num(0, my_fetch_int(hstmt, 9));
    // NUM_PREC_RADIX
    is_num(0, my_fetch_int(hstmt, 10));
    // NULLABLE
    is_num(SQL_NULLABLE, my_fetch_int(hstmt, 11));
    // COMMENT
    is_str("some comment", my_fetch_str(hstmt, char_buf, 12), SQL_NTS);
    // DEFAULT VALUE
    // Note: if NULL is a default value the function must report the word
    //       NULL not enclosed in quotation marks.
    is_str("NULL", my_fetch_str(hstmt, char_buf, 13), SQL_NTS);
    // SQL_DATA_TYPE
    is_num(SQL_VARBINARY, my_fetch_int(hstmt, 14));
    // SQL_DATETIME_SUB
    is_num(0, my_fetch_int(hstmt, 15));
    // CHAR_OCTET_LENGTH
    is_num(col_cnt * sizeof(float), my_fetch_int(hstmt, 16));
    // ORDINAL_POSITION
    is_num(1, my_fetch_int(hstmt, 17));
    // IS_NULLABLE
    is_str("YES", my_fetch_str(hstmt, char_buf, 18), SQL_NTS);

    is_no_data(SQLFetch(hstmt));
    odbc::stmt_close(hstmt);
  };


  // Check 3: SQLGetTypeInfo()
  auto check_type_info = [](SQLHSTMT hstmt)
  {
    char char_buf[64];
    for (auto opt : { SQL_VARBINARY, SQL_ALL_TYPES })
    {
      bool vector_detected = false;
      ok_stmt(hstmt, SQLGetTypeInfo(hstmt, opt));
      while(SQL_SUCCESS == SQLFetch(hstmt))
      {
        std::string type_name = my_fetch_str(hstmt, char_buf, 1);
        if (type_name != "vector()")
          continue;

        // DATA_TYPE
        is_num(SQL_VARBINARY, my_fetch_int(hstmt, 2));
        // COLUMN_SIZE
        is_num(16382, my_fetch_int(hstmt, 3));
        // LOCAL_TYPE_NAME
        is_str("vector", my_fetch_str(hstmt, char_buf, 13), SQL_NTS);
        // LITERAL_PREFIX
        // Note: for NULL values my_fetch_str() returns "(Null)" string
        is_str("(Null)", my_fetch_str(hstmt, char_buf, 4), SQL_NTS);
        // LITERAL_SUFIX
        is_str("(Null)", my_fetch_str(hstmt, char_buf, 5), SQL_NTS);

        vector_detected = true;
      }
      is(vector_detected);
      odbc::stmt_close(hstmt);

    }
  };

  std::vector<const char*> opts = { "NO_SSPS=0", "NO_SSPS=1" };

  try
  {
    for (auto opt : opts)
    {
      odbc::connection con(nullptr, nullptr, nullptr, nullptr, opt);
      odbc::HSTMT hstmt1(con);

      std::vector <float> float_buf = float_buf_orig;
      check_insert_data(hstmt1, float_buf);
      check_select_data(hstmt1, float_buf);
      check_metadata(hstmt1, float_buf.size());
      check_table_columns(hstmt1, float_buf.size());
      check_type_info(hstmt1);

      // Check 4: Working with vector sizes M <> N
      // Check when data is longer than the column, add one more element.
      float_buf.emplace_back(543.1231f);
      try
      {
        check_insert_data(hstmt1, float_buf);
        return FAIL;
      }
      catch(odbc::SimpleException &e)
      {
        std::cout << "Expected error\n";
      }

      // Check when data is shorter than the column
      odbc::stmt_close(hstmt1);
      odbc::stmt_reset(hstmt1);
      odbc::sql(hstmt1, "TRUNCATE test_vector");

      float_buf.resize(8);
      check_insert_data(hstmt1, float_buf);
      check_select_data(hstmt1, float_buf);
      odbc::stmt_close(hstmt1);
      odbc::stmt_reset(hstmt1);
      odbc::sql(hstmt1, "TRUNCATE test_vector");
    }
  }
  ENDCATCH;
}


/*
  Bug #21115726 - INCORRECT RESULT FETCHED FROM BIGINT COLUMN.
*/
DECLARE_TEST(t_bug21115726)
{
  try
  {
    odbc::table tab(hstmt, "bigint21115726",
      "C1 BIGINT, C2 BIGINT UNSIGNED, C3 BIGINT(7)");

    tab.insert("(-9223372036854775808,18446744073709551615,1234567123)");

    SQLLEN c1 = 0, len1 = 0, len2 = 0, len3 = 0, c3 = 0;
    SQLULEN c2 = 0;

    auto res_check = [&c1, &c2, &c3, &len1, &len2, &len3]()
    {
      is_num(INT64_MIN, c1);
      is_num(UINT64_MAX, c2);
      is_num(1234567123LL, c3);
      is_num(8, len1);
      is_num(8, len2);
      is_num(8, len3);

      std::cout << "C1=" << c1 << " (len:" << len1 << ")\n" <<
                    "C2=" << c2 << " (len:" << len2 << ")\n" <<
                    "C3=" << c3 << " (len:" << len3 << ")\n";
    };

    auto set_zero = [&c1, &c2, &c3, &len1, &len2, &len3]()
    {
      c1 = 0; c2 = 0; c3 = 0;
      len1 = len2 = len3 = 0;
    };

    for (int i = 0; i < 2; ++i)
    {
      if (i == 0)
      {
        // Execute the query
        odbc::sql(hstmt, "SELECT * FROM bigint21115726");
      }
      else
      {
        SQLINTEGER int_val = 1;
        // Do SSPS
        odbc::stmt_prepare(hstmt, "SELECT * FROM bigint21115726 WHERE 1 = ?");
        ok_stmt(hstmt, SQLBindParameter(hstmt, 1, SQL_PARAM_INPUT, SQL_C_LONG,
          SQL_INTEGER, 10, 0, &int_val, 0, nullptr));
        odbc::stmt_execute(hstmt);
      }

      ok_stmt(hstmt, SQLBindCol(hstmt, 1, SQL_C_SBIGINT, &c1, 0, &len1));
      ok_stmt(hstmt, SQLBindCol(hstmt, 2, SQL_C_UBIGINT, &c2, 0, &len2));
      ok_stmt(hstmt, SQLBindCol(hstmt, 3, SQL_C_SBIGINT, &c3, 0, &len3));
      ok_stmt(hstmt, SQLFetch(hstmt));

      res_check();
      set_zero();

      ok_stmt(hstmt, SQLGetData(hstmt, 1, SQL_C_SBIGINT, &c1, 0, &len1));
      ok_stmt(hstmt, SQLGetData(hstmt, 2, SQL_C_UBIGINT, &c2, 0, &len2));
      ok_stmt(hstmt, SQLGetData(hstmt, 3, SQL_C_SBIGINT, &c3, 0, &len3));

      res_check();

      is_no_data(SQLFetch(hstmt));
      odbc::stmt_close(hstmt);
    }
  }
  ENDCATCH;
}

/*
  Bug#37286526/116657 - Retrieving list of records that contain an
  empty blob causes undefined behaviour.
*/
DECLARE_TEST(t_bug37286526_empty_blob)
{
  try
  {
    const size_t buflen = 32;

    odbc::xbuf data_char(buflen);
    odbc::xbuf data_bin(buflen);

    odbc::table tab(hstmt, "tab37286526", "C1 varchar(32), C2 BLOB");

    tab.insert("('A', 'ABCDE'), ('B', '')");

    enum { BIND_COL, GET_DATA };

    SQLLEN expected_len[][2] = {{1, 5}, {1, 0}};
    const char* expected_data[][2] = {{"A", "ABCDE"}, {"B", ""}};

    for (auto v : { BIND_COL, GET_DATA })
    {
      SQLLEN len_bin = 0;
      SQLLEN len_char = 0;

      odbc::stmt_prepare(hstmt, "SELECT * FROM tab37286526");

      // For BIND_COL we bind buffers before executing of the statement.
      if (v == BIND_COL)
      {
        ok_stmt(hstmt, SQLBindCol(hstmt, 1, SQL_C_CHAR, data_char, buflen, &len_char));
        ok_stmt(hstmt, SQLBindCol(hstmt, 2, SQL_C_BINARY, data_bin, buflen, &len_bin));
      }
      odbc::stmt_execute(hstmt);

      int row_num = 0;

      while(SQL_SUCCESS == SQLFetch(hstmt))
      {
        //  For GET_DATA case getting of data is done after SQLFetch() call.
        if (v == GET_DATA)
        {
          ok_stmt(hstmt, SQLGetData(hstmt, 1, SQL_C_CHAR, data_char, buflen, &len_char));
          ok_stmt(hstmt, SQLGetData(hstmt, 2, SQL_C_BINARY, data_bin, buflen, &len_bin));
        }

        is_num(expected_len[row_num][0], len_char);
        is_num(expected_len[row_num][1], len_bin);

        is_num(0, memcmp(expected_data[row_num][0], data_char, len_char));

        /*
          No need to check contents of the row with empty blob data.
          Buffers are not modified.
        */

        if (len_bin)
          is_num(0, memcmp(expected_data[row_num][1], data_bin, len_bin));

        ++row_num;
      }

      // Expect to get 2 rows
      is_num(2, row_num);

      odbc::stmt_close(hstmt);
    }

  }
  ENDCATCH;
}

/*
  Bug#37298936/114470 - returned data that does not match expected
  data length for column
*/

DECLARE_TEST(t_bug37298936_pad_spaces)
{
  try
  {
    const size_t pad_width = 32;
    const size_t buflen = pad_width * 5;
    SQLSMALLINT target_type = unicode_driver ? SQL_C_WCHAR : SQL_C_CHAR;

    // For Unicode driver the expected length is more because of SQLWCHAR
    size_t char_size = unicode_driver ? sizeof(SQLWCHAR) : sizeof(SQLCHAR);

    /*
      Each entry in `test_data` is an utf8 string and the number of characters
      in that string.
    */
    std::vector<std::pair<std::string, size_t>> test_data =
    {
      {"ABC", 3}, {"", 0}, {"\xD0\xA4", 1}
    };

    // Create table. The table has one column per each test string above.

    std::stringstream sql;

    for (size_t i=0; i < test_data.size(); ++i)
    {
      if (i) sql << " ,";
      sql << "C" << (i+1) << " CHAR(" << pad_width << ")";
    };

    odbc::table tab(hstmt, "tab37298936", sql.str());

    /*
      Insert data.

      Note: Converstion to utf8 is done using expressions like this one
      ```
        convert(UNHEX('d0a4') using utf8)
      ```
    */

    sql.str("");

    sql << "INSERT INTO tab37298936 VALUES (";
    for (size_t i=0; i < test_data.size(); ++i)
    {
      if (i) sql << " ,";
      sql << "convert(UNHEX('";
      for (char c : test_data[i].first)
        sql << std::hex << std::setw(2) << std::setfill('0')
            << (int)(unsigned char)c;
      sql << "') using utf8)";
    };
    sql << ")";

    odbc::sql(hstmt, sql.str());

    /*
      Select data with or without padding and check that returned data
      is as expected.
    */

    for (bool pad : {false, true})
    {
      sql.str("");
      sql << "PAD_SPACE=" << (pad ? "1" : "0");

      // Make sure the ANSI driver uses the right charset.
      if (!unicode_driver)
        sql << ";CHARSET=utf8mb4";

      odbc::connection con(nullptr, nullptr, nullptr, nullptr, sql.str());
      odbc::HSTMT hstmt1(con);

      for (bool bind : {false, true})
      {
        std::vector<SQLLEN> len(test_data.size());
        std::vector<odbc::xbuf> buf;

        /*
          Depending on `bind` flag we either bind buffers for prepared
          statement or fetch data into buffers after execution.
        */

        auto attach_buffers = [&](bool bind)
        {
          buf.clear();

          for (size_t i=0; i < test_data.size(); ++i)
          {
            buf.emplace_back(buflen);

            if (bind)
              ok_stmt(hstmt1, SQLBindCol(
                hstmt1, i+1, target_type, (SQLPOINTER)buf[i], buflen, &len[i]
              ));
            else
              ok_stmt(hstmt1, SQLGetData(
                hstmt1, i+1, target_type, (SQLPOINTER)buf[i], buflen, &len[i]
              ));
          }
        };

        odbc::stmt_prepare(hstmt1, "SELECT * FROM tab37298936");

        if (bind)
          attach_buffers(true);

        odbc::stmt_execute(hstmt1);

        is(SQL_SUCCESS == SQLFetch(hstmt1));

        if (!bind)
          attach_buffers(false);

        // Check received data

        for (size_t i=0; i < test_data.size(); ++i)
        {
          /*
            Note: d.first is a utf8 string, d.second is the number of chars
            in that string.
          */

          auto &d = test_data[i];

          /*
            Expected length.

            The length `str_len` is measured in characters for Unicode driver
            and in bytes for ANSI one (in the latter case we have utf8 strings).

            `pad_len` is the number of spaces that need to be added to pad
            to `pad_width` (0 if no padding). This is computed using the number
            of *characters* in the test data as given by `d.second`.
          */

          SQLLEN str_len = unicode_driver ? d.second : d.first.length();
          SQLLEN pad_len = pad ? pad_width - d.second : 0;
          SQLLEN expected_len = str_len + pad_len;

          is_num(expected_len * char_size, len[i]);

          // Check that correct number of spaces is added after the string.

          for (size_t k = 0; k < pad_len; ++k)
          {
            if (unicode_driver)
            {
              is((SQLWCHAR)' ' == ((SQLWCHAR*)(buf[i]))[str_len + k]);
            }
            else
            {
              is((char)' ' == ((char*)(buf[i]))[str_len + k]);
            }
          }
        }
        odbc::stmt_close(hstmt1);
      }
    }
  }
  ENDCATCH;
}


/*
  Bug#18641803 - ASSERT FAILURE IN SQLBINDCOL WHEN CALLED WITHOUT
  SETTING SQL_ATTR_USE_BOOKMARKS
*/
DECLARE_TEST(t_bug18641803_bind_col)
{
  try
  {
    odbc::stmt_prepare(hstmt, "SELECT 1234");

    ok_stmt(hstmt, SQLBindCol(hstmt, 0, SQL_C_VARBOOKMARK, NULL, 0, NULL));
    ok_stmt(hstmt, SQLBindCol(hstmt, 0, SQL_C_VARBOOKMARK, NULL, 0, NULL));

    odbc::stmt_execute(hstmt);
    int rnum = 0;
    odbc::xbuf buf(16);

    while (SQL_SUCCESS == SQLFetch(hstmt))
    {
      const char *s = odbc::my_fetch_str(hstmt, buf, 1);
      is_str("1234", s, SQL_NTS);
      ++rnum;
    }
    is_num(1, rnum);
  }
  ENDCATCH;
}


/*
  Bug#37071646 - Time type not formatted correctly in SSPS mode
*/
DECLARE_TEST(t_bug37071646_time_format)
{
  try {

    /*
      Checking the min and max time values and also the fractional
      seconds close to min and max values.
    */
    odbc::xstring vals[][2] = {
      {"-838:59:59", "-838:59:58.987654"},
      {"838:59:59", "838:59:58.987654"}
    };

    odbc::table tab(hstmt, "tab37071646", "typetime1 TIME(0), typetime2 TIME(6)");
    odbc::xstring ins = "('";

    ins.append(vals[0][0]).append("','").append(vals[0][1]).append("'),('").
        append(vals[1][0]).append("','").append(vals[1][1]).append("')");
    tab.insert(ins);

    // It should work with both SSPS and and client-side statements
    for (odbc::xstring opt : {"NO_SSPS=0", "NO_SSPS=1"}) {
      odbc::connection con(nullptr, nullptr, nullptr, nullptr, opt);
      SQLHSTMT hstmt1 = con.hstmt;

      odbc::stmt_prepare(hstmt1,
        "SELECT * FROM tab37071646 order by typetime1 ASC");

      char buf[2][128];
      SQLLEN len[] = {0, 0};
      ok_stmt(hstmt1,
        SQLBindCol(hstmt1, 1, SQL_C_CHAR, &buf[0], 128, &len[0]));
      ok_stmt(hstmt1,
        SQLBindCol(hstmt1, 2, SQL_C_CHAR, &buf[1], 128, &len[1]));

      odbc::stmt_execute(hstmt1);

      for (auto rv : vals) {
        ok_stmt(hstmt1, SQLFetch(hstmt1));
        // Check returned data lengths
        is_num(rv[0].length(), len[0]);
        is_num(rv[1].length(), len[1]);
        // Check the data
        is_str(rv[0], buf[0], rv[0].length());
        is_str(rv[1], buf[1], rv[1].length());
      }

      is(SQL_NO_DATA == SQLFetch(hstmt1));
    }
  }
  ENDCATCH;
}

DECLARE_TEST(t_float_text_invalid_length) {
  try {
    for (const char *option : {"NO_SSPS=1", "NO_SSPS=0"}) {
      odbc::connection con(nullptr, nullptr, nullptr, nullptr, option);
      SQLHSTMT stmt = con.hstmt;
      odbc::stmt_prepare(stmt, "SELECT ?");
      for (SQLSMALLINT sql_type : {SQL_FLOAT, SQL_REAL, SQL_DOUBLE}) {
        for (SQLSMALLINT c_type : {SQL_C_CHAR, SQL_C_BINARY}) {
          for (SQLLEN invalid_length : {-10, -99}) {
            char value[] = "1";
            SQLLEN length = sizeof(value) - 1;
            ok_stmt(stmt,
                    SQLBindParameter(stmt, 1, SQL_PARAM_INPUT, c_type, sql_type,
                                     0, 0, value, sizeof(value), &length));
            // Change the length after binding so it is checked at execution.
            // Neither value is a special ODBC length indicator.
            length = invalid_length;
            expect_stmt(stmt, SQLExecute(stmt), SQL_ERROR);
            is_num(check_sqlstate_ex(stmt, SQL_HANDLE_STMT, "HY090"), OK);
            odbc::stmt_close(stmt);
          }
        }
      }
    }
  }
  ENDCATCH;
}

/* Text bound to floating-point SQL types must remain parameter data. */
DECLARE_TEST(t_float_text_parameters) {
  try {
    const auto hex_dump = [](const void *data, size_t size) {
      const auto *bytes = static_cast<const unsigned char *>(data);
      std::ostringstream out;
      out << std::hex << std::setfill('0');
      for (size_t i = 0; i < (std::min)(size, size_t{64}); ++i) {
        if (i) out << ' ';
        out << std::setw(2) << static_cast<unsigned int>(bytes[i]);
      }
      if (size > 64) out << " ... (first 64 bytes)";
      return out.str();
    };
    for (const char *option :
         {"NO_SSPS=1;NO_LOCALE=0", "NO_SSPS=0;NO_LOCALE=0"}) {
      odbc::connection con(nullptr, nullptr, nullptr, nullptr, option);
      SQLHSTMT stmt = con.hstmt;
      bool iodbc_wide_nul_limitation = false;
#if defined(USE_IODBC) && !defined(NO_DRIVERMANAGER)
      SQLCHAR dm_version[32] = {};
      iodbc_wide_nul_limitation =
          SQL_SUCCEEDED(SQLGetInfo(con.hdbc, SQL_DM_VER, dm_version,
                                   sizeof(dm_version), nullptr)) &&
          strncmp(reinterpret_cast<const char *>(dm_version), "03.52.15", 8) ==
              0;
      if (iodbc_wide_nul_limitation)
        printMessage(
            "%s: skipping SQL_C_WCHAR embedded NUL: iODBC 3.52.15 "
            "does not preserve it; SQL_C_CHAR/SQL_C_BINARY remain tested",
            option);
#endif
      for (const char *mode : {"", "NO_BACKSLASH_ESCAPES"}) {
        odbc::sql(stmt, std::string("SET SESSION sql_mode='") + mode + "'");
        odbc::stmt_prepare(stmt, "SELECT ?, 7");
        for (SQLSMALLINT sql_type : {SQL_FLOAT, SQL_REAL, SQL_DOUBLE}) {
          for (SQLSMALLINT c_type : {SQL_C_CHAR, SQL_C_BINARY, SQL_C_WCHAR}) {
            SQLSMALLINT driver_c_type = c_type;
#if !defined(NO_DRIVERMANAGER) && (defined(_WIN32) || defined(USE_IODBC))
            // Windows and iODBC map wide parameters to ANSI for ANSI drivers.
            if (c_type == SQL_C_WCHAR && unicode_driver == 0)
              driver_c_type = SQL_C_CHAR;
#endif
            const std::string values[] = {
                "1+1",   "1 /* parameter */",      "1'\\\n",
                "-12.5", "4.2345678902345678e+20", "1,234.5",
                "",      std::string("1\0+1", 4),  std::string(512, '1')};
            for (const std::string &value : values) {
              // iODBC 3.52.15 uses NUL-terminated copies for wide parameters.
              if (iodbc_wide_nul_limitation && c_type == SQL_C_WCHAR &&
                  value.find('\0') != std::string::npos)
                continue;
              // Exercise explicit lengths, SQL_NTS and streamed data.
              for (int length_mode : {0, 1, 2}) {
                if (c_type != SQL_C_CHAR && length_mode != 0) continue;
                std::vector<char> bytes(value.begin(), value.end());
                if (bytes.empty() || length_mode == 1) bytes.push_back(0);
                std::vector<SQLWCHAR> wide(value.begin(), value.end());
                wide.push_back(0);
                SQLPOINTER data = c_type == SQL_C_WCHAR
                                      ? static_cast<SQLPOINTER>(wide.data())
                                      : static_cast<SQLPOINTER>(bytes.data());
                SQLLEN size = value.size() *
                              (c_type == SQL_C_WCHAR ? sizeof(SQLWCHAR) : 1);
                SQLLEN length = length_mode == 1 ? SQL_NTS : size;
                if (length_mode == 2) length = SQL_LEN_DATA_AT_EXEC(size);
                const size_t buffer_bytes = c_type == SQL_C_WCHAR
                                                ? wide.size() * sizeof(SQLWCHAR)
                                                : bytes.size();
                const SQLLEN length_before_bind = length;
                const std::string input_before_bind =
                    hex_dump(data, buffer_bytes);
                // Include the terminator in the buffer size, not the data
                // length.
                ok_stmt(stmt,
                        SQLBindParameter(
                            stmt, 1, SQL_PARAM_INPUT, c_type, sql_type, 0, 0,
                            data, static_cast<SQLLEN>(buffer_bytes), &length));
                const SQLLEN length_after_bind = length;
                if (length_mode == 2) {
                  expect_stmt(stmt, SQLExecute(stmt), SQL_NEED_DATA);
                  SQLPOINTER token;
                  expect_stmt(stmt, SQLParamData(stmt, &token), SQL_NEED_DATA);
                  is(token == data);
                  ok_stmt(stmt, SQLPutData(stmt, data, size / 2));
                  ok_stmt(stmt,
                          SQLPutData(stmt, static_cast<char *>(data) + size / 2,
                                     size - size / 2));
                  ok_stmt(stmt, SQLParamData(stmt, &token));
                } else {
                  ok_stmt(stmt, SQLExecute(stmt));
                }
                const SQLLEN length_after_execute = length;
                const std::string input_after_execute =
                    hex_dump(data, buffer_bytes);
                ok_stmt(stmt, SQLFetch(stmt));
                std::string expected =
                    length_mode == 1 ? std::string(value.c_str()) : value;
                if (driver_c_type != SQL_C_WCHAR) {
                  // Keep the existing locale normalization and empty -> 0.
                  expected.erase(
                      std::remove(expected.begin(), expected.end(), ','),
                      expected.end());
                  if (expected.empty()) expected = "0";
                }
                std::vector<char> result(expected.size() + 1);
                SQLLEN result_length;
                ok_stmt(stmt, SQLGetData(stmt, 1, SQL_C_CHAR, result.data(),
                                         result.size(), &result_length));
                if (result_length != static_cast<SQLLEN>(expected.size()) ||
                    memcmp(result.data(), expected.data(), expected.size()) !=
                        0) {
                  printMessage(
                      "Float parameter mismatch: %s, sql_mode='%s', "
                      "sql_type=%d, "
                      "c_type=%d, assumed_driver_c_type=%d, value_bytes=%zu, "
                      "length_mode=%d",
                      option, mode, sql_type, c_type, driver_c_type,
                      value.size(), length_mode);
                  printMessage(
                      "sizeof(SQLWCHAR)=%zu, unicode_driver=%d, "
                      "BufferLength=%zu, data_bytes=%lld",
                      sizeof(SQLWCHAR), unicode_driver, buffer_bytes,
                      static_cast<long long>(size));
                  printMessage(
                      "Indicator: before_bind=%lld, after_bind=%lld, "
                      "after_execute=%lld; result_length=%lld, "
                      "expected_length=%zu",
                      static_cast<long long>(length_before_bind),
                      static_cast<long long>(length_after_bind),
                      static_cast<long long>(length_after_execute),
                      static_cast<long long>(result_length), expected.size());
                  printMessage("Input buffer before bind (hex): [%s]",
                               input_before_bind.c_str());
                  printMessage("Input buffer after execute (hex): [%s]",
                               input_after_execute.c_str());
                  printMessage(
                      "Expected data (hex): [%s]",
                      hex_dump(expected.data(), expected.size()).c_str());
                  printMessage("Result buffer, %zu bytes (hex): [%s]",
                               result.size(),
                               hex_dump(result.data(), result.size()).c_str());
                  const SQLUSMALLINT info_types[] = {
                      SQL_DRIVER_NAME, SQL_DRIVER_VER, SQL_DM_VER};
                  const char *info_names[] = {"SQL_DRIVER_NAME",
                                              "SQL_DRIVER_VER", "SQL_DM_VER"};
                  for (size_t i = 0; i < 3; ++i) {
                    SQLCHAR info[256] = {};
                    SQLRETURN rc = SQLGetInfo(con.hdbc, info_types[i], info,
                                              sizeof(info), nullptr);
                    printMessage("%s: rc=%d, value='%s'", info_names[i], rc,
                                 SQL_SUCCEEDED(rc)
                                     ? reinterpret_cast<const char *>(info)
                                     : "unavailable");
                  }
                }
                is_num(result_length, expected.size());
                is(memcmp(result.data(), expected.data(), expected.size()) ==
                   0);
                is_num(my_fetch_int(stmt, 2), 7);
                expect_stmt(stmt, SQLFetch(stmt), SQL_NO_DATA);
                odbc::stmt_close(stmt);
              }
            }
          }
        }
      }
      // Normalized numeric text must still work in a numeric expression.
      odbc::stmt_prepare(stmt, "SELECT ? + 0");
      for (SQLSMALLINT sql_type : {SQL_FLOAT, SQL_REAL, SQL_DOUBLE}) {
        char value[] = "1,234.5";
        ok_stmt(stmt,
                SQLBindParameter(stmt, 1, SQL_PARAM_INPUT, SQL_C_CHAR, sql_type,
                                 0, 0, value, sizeof(value), nullptr));
        ok_stmt(stmt, SQLExecute(stmt));
        ok_stmt(stmt, SQLFetch(stmt));
        double result;
        ok_stmt(stmt, SQLGetData(stmt, 1, SQL_C_DOUBLE, &result, sizeof(result),
                                 nullptr));
        is(result == 1234.5);
        odbc::stmt_close(stmt);
      }
    }
  }
  ENDCATCH;
}

BEGIN_TESTS
  ADD_TEST(t_float_text_invalid_length)
  ADD_TEST(t_float_text_parameters)
  ADD_TEST(t_bug37071646_time_format)
  ADD_TEST(t_bug18641803_bind_col)
  ADD_TEST(t_bug37298936_pad_spaces)
  ADD_TEST(t_bug37286526_empty_blob)
  ADD_TEST(t_wl16171_vector)
  ADD_TEST(t_bug21115726)
  ADD_TEST(t_bug26474471)
  ADD_TEST(t_bug35520983_sjis)
  ADD_TEST(t_utf8mb4_param)
  ADD_TEST(t_bug_34350417_performance)
  ADD_TEST(t_bug33353465_json_utf8mb4)
  ADD_TEST(t_wl15423_json)
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
