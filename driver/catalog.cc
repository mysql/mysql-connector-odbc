// Copyright (c) 2000, 2022, Oracle and/or its affiliates. All rights reserved.
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
// which is part of MySQL Connector/ODBC, is also subject to the
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

/**
  @file  catalog.c
  @brief Catalog functions.
*/

#include "driver.h"
#include "catalog.h"


size_t ROW_STORAGE::set_size(size_t rnum, size_t cnum)
{
  size_t new_size = rnum * cnum;
  m_rnum = rnum;
  m_cnum = cnum;

  if (new_size)
  {
    m_data.resize(new_size, "");
    m_pdata.resize(new_size, nullptr);

    // Move the current row back if the array had shrunk
    if (m_cur_row >= rnum)
      m_cur_row = rnum - 1;
  }
  else
  {
    // Clear if the size is zero
    m_data.clear();
    m_pdata.clear();
    m_cur_row = 0;
  }

  return new_size;
}

bool ROW_STORAGE::next_row()
{
  ++m_cur_row;

  if (m_cur_row < m_rnum - 1)
  {
    return true;
  }
  // if not enough rows - add one more
  set_size(m_rnum + 1, m_cnum);
  return false;
}

const xstring & ROW_STORAGE::operator=(const xstring &val)
{
  size_t offs = m_cur_row * m_cnum + m_cur_col;
  m_data[offs] = val;
  m_pdata[offs] = m_data[offs].c_str();
  return m_data[offs];
}

xstring& ROW_STORAGE::operator[](size_t idx)
{
  if (idx >= m_cnum)
    throw ("Column number is out of bounds");

  m_cur_col = idx;
  return m_data[m_cur_row * m_cnum + m_cur_col];
}

const char** ROW_STORAGE::data()
{
  auto m_data_it = m_data.begin();
  auto m_pdata_it = m_pdata.begin();

  while(m_data_it != m_data.end())
  {
    *m_pdata_it = m_data_it->c_str();
    ++m_pdata_it;
    ++m_data_it;
  }
  return m_pdata.size() ? m_pdata.data() : nullptr;
}

/*
  @type    : internal
  @purpose : returns the next token
*/

const char *my_next_token(const char *prev_token,
                          const char **token,
                                char *data,
                          const char chr)
{
    const char *cur_token;

    if ( (cur_token= strchr(*token, chr)) )
    {
        if ( prev_token )
        {
            uint len= (uint)(cur_token-prev_token);
            strncpy(data,prev_token, len);
            data[len]= 0;
        }
        *token= (char *)cur_token+1;
        prev_token= cur_token;
        return cur_token+1;
    }
    return 0;
}


/**
  Create a fake result set in the current statement

  @param[in] stmt           Handle to statement
  @param[in] rowval         Row array
  @param[in] rowsize        sizeof(row array)
  @param[in] rowcnt         Number of rows
  @param[in] fields         Field array
  @param[in] fldcnt         Number of fields

  @return SQL_SUCCESS or SQL_ERROR (and diag is set)
*/
SQLRETURN
create_fake_resultset(STMT *stmt, MYSQL_ROW rowval, size_t rowsize,
                      my_ulonglong rowcnt, MYSQL_FIELD *fields, uint fldcnt,
                      bool copy_rowval = true)
{
  if (stmt->fake_result)
  {
    x_free(stmt->result);
  }
  else
  {
    if (stmt->result)
      mysql_free_result(stmt->result);
  }

  /* Free if result data was not in row storage */
  if (!stmt->m_row_storage.is_valid())
    stmt->result_array.reset();

  stmt->result= (MYSQL_RES*) myodbc_malloc(sizeof(MYSQL_RES), MYF(MY_ZEROFILL));
  if (!stmt->result) {
    set_mem_error(stmt->dbc->mysql);
    return handle_connection_error(stmt);
  }
  stmt->fake_result = 1;

  if (copy_rowval)
  {
    stmt->result_array.set(rowval, rowsize);
  }

  set_row_count(stmt, rowcnt);

  myodbc_link_fields(stmt, fields, fldcnt);

  return SQL_SUCCESS;
}


/**
  Create an empty fake result set in the current statement

  @param[in] stmt           Handle to statement
  @param[in] rowval         Row array
  @param[in] rowsize        sizeof(row array)
  @param[in] fields         Field array
  @param[in] fldcnt         Number of fields

  @return SQL_SUCCESS or SQL_ERROR (and diag is set)
*/
SQLRETURN
create_empty_fake_resultset(STMT *stmt, MYSQL_ROW rowval, size_t rowsize,
                            MYSQL_FIELD *fields, uint fldcnt)
{
  return create_fake_resultset(stmt, rowval, rowsize, 0 /* rowcnt */,
                               fields, fldcnt);
}

/**
  Get the DB information using Information_Schema DB.
  Lengths may not be SQL_NTS.

  @param[in] stmt           Handle to statement
  @param[in] db_name        Database of table, @c NULL for current
  @param[in] db_len         Length of database name
  @param[in] wildcard       Whether the table name is a wildcard

  @return Result of SELECT ... FROM I_S.SCHEMATA or NULL if there is an error
          or empty result (check mysql_errno(stmt->dbc->mysql) != 0)
          It contains the same set of result columns as table_status_i_s()
*/
MYSQL_RES *db_status(STMT *stmt, std::string &db)
{
  MYSQL *mysql= stmt->dbc->mysql;
  /** the buffer size should count possible escapes */
  std::string query;
  char tmpbuff[1024];
  size_t cnt = 0;;
  query.reserve(1024);
  query = "SELECT NULL, NULL, NULL, SCHEMA_NAME " \
          "FROM INFORMATION_SCHEMA.SCHEMATA WHERE ";

  if (db.length())
  {
    query.append("SCHEMA_NAME LIKE '");
    cnt = myodbc_escape_string(stmt, tmpbuff, sizeof(tmpbuff),
                              (char *)db.c_str(), (ulong)db.length(), 1);
    query.append(tmpbuff, cnt);
    query.append("' ");
  }
  else
  {
    query.append("SCHEMA_NAME=DATABASE() ");
  }

  query.append(" ORDER BY SCHEMA_NAME");

  MYLOG_QUERY(stmt, query.c_str());

  if (exec_stmt_query(stmt, query.c_str(), query.length(), FALSE))
  {
    return NULL;
  }

  return mysql_store_result(mysql);
}



/**
  Get the table status for a table or tables using Information_Schema DB.
  Lengths may not be SQL_NTS.

  @param[in] stmt           Handle to statement
  @param[in] db_name        Database of table, @c NULL for current
  @param[in] db_len         Length of database name
  @param[in] table_name     Name of table
  @param[in] table_len      Length of table name
  @param[in] wildcard       Whether the table name is a wildcard

  @return Result of SELECT ... FROM I_S.TABLES, or NULL if there is an error
          or empty result (check mysql_errno(stmt->dbc->mysql) != 0)
*/
static MYSQL_RES *table_status_i_s(STMT    *stmt,
                                   SQLCHAR     *db_name,
                                   SQLSMALLINT  db_len,
                                   SQLCHAR     *table_name,
                                   SQLSMALLINT  table_len,
                                   my_bool      wildcard,
                                   my_bool      show_tables,
                                   my_bool      show_views)
{
  MYSQL *mysql= stmt->dbc->mysql;
  /** the buffer size should count possible escapes */
  std::string query;
  char tmpbuff[1024];
  size_t cnt = 0;;
  query.reserve(1024);
  query = "SELECT TABLE_NAME,TABLE_COMMENT," \
          "IF(TABLE_TYPE='BASE TABLE', 'TABLE', TABLE_TYPE)," \
          "TABLE_SCHEMA " \
          "FROM INFORMATION_SCHEMA.TABLES WHERE ";

  if (db_name && *db_name)
  {
    query.append("TABLE_SCHEMA LIKE '");
    cnt = myodbc_escape_string(stmt, tmpbuff, sizeof(tmpbuff),
                              (char *)db_name, db_len, 1);
    query.append(tmpbuff, cnt);
    query.append("' ");
  }
  else
  {
    query.append("TABLE_SCHEMA=DATABASE() ");
  }

  if (show_tables)
  {
    query.append("AND ");
    if (show_views)
      query.append("( ");
    query.append("TABLE_TYPE='BASE TABLE' ");
  }

  if (show_views)
  {
    if (show_tables)
      query.append("OR ");
    else
      query.append("AND ");

    query.append("TABLE_TYPE='VIEW' ");
    if (show_tables)
      query.append(") ");
  }

  /*
    As a pattern-value argument, an empty string needs to be treated
    literally. (It's not the same as NULL, which is the same as '%'.)
    But it will never match anything, so bail out now.
  */
  if (table_name && wildcard && !*table_name)
    return NULL;

  if (table_name && *table_name)
  {
    query.append("AND TABLE_NAME LIKE '");
    if (wildcard)
    {
      cnt = mysql_real_escape_string(mysql, tmpbuff, (char *)table_name, table_len);
      query.append(tmpbuff, cnt);
    }
    else
    {
      cnt = myodbc_escape_string(stmt, tmpbuff, sizeof(tmpbuff),
                                (char *)table_name, table_len, 0);
      query.append(tmpbuff, cnt);
    }
    query.append("'");
  }

  query.append(" ORDER BY TABLE_SCHEMA, TABLE_NAME");

  MYLOG_QUERY(stmt, query.c_str());

  if (exec_stmt_query(stmt, query.c_str(), query.length(), FALSE))
  {
    return NULL;
  }

  return mysql_store_result(mysql);
}


/**
  Get the table status for a table or tables. Lengths may not be SQL_NTS.

  @param[in] stmt           Handle to statement
  @param[in] db_name        Database of table, @c NULL for current
  @param[in] db_len    Length of catalog name
  @param[in] table_name     Name of table
  @param[in] table_len      Length of table name
  @param[in] wildcard       Whether the table name is a wildcard

  @return Result of SHOW TABLE STATUS, or NULL if there is an error
          or empty result (check mysql_errno(stmt->dbc->mysql) != 0)
*/
MYSQL_RES *table_status(STMT        *stmt,
                        SQLCHAR     *db_name,
                        SQLSMALLINT  db_len,
                        SQLCHAR     *table_name,
                        SQLSMALLINT  table_len,
                        my_bool      wildcard,
                        my_bool      show_tables,
                        my_bool      show_views)
{
    return table_status_i_s(stmt, db_name, db_len, table_name, table_len,
                            wildcard, show_tables, show_views);
}


/*
@type    :  internal
@purpose :  Adding name condition for arguments what depending on SQL_ATTR_METADATA_ID
            are either ordinary argument(oa) or identifier string(id)
            NULL _default parameter means that parameter is mandatory and error is generated
@returns :  1 if required parameter is NULL, 0 otherwise
*/
int add_name_condition_oa_id(HSTMT hstmt, std::string &query, SQLCHAR * name,
                              SQLSMALLINT name_len, const char * _default)
{
  SQLUINTEGER metadata_id;

  /* this shouldn't be very expensive, so no need to complicate things with additional parameter etc */
  MySQLGetStmtAttr(hstmt, SQL_ATTR_METADATA_ID, (SQLPOINTER)&metadata_id, 0, NULL);

  /* we can't rely here that column was checked and is not null */
  if (name)
  {
    STMT *stmt= (STMT *) hstmt;

    if (metadata_id)
    {
      query.append("=");
      /* Need also code to remove trailing blanks */
    }
    else
      query.append("= BINARY ");

    query.append("'");
    char tmpbuff[1024];
    size_t cnt = mysql_real_escape_string(stmt->dbc->mysql, tmpbuff, (char *)name, name_len);
    query.append(tmpbuff, cnt);
    query.append("' ");
  }
  else
  {
    /* According to http://msdn.microsoft.com/en-us/library/ms714579%28VS.85%29.aspx
    identifier argument cannot be NULL with one exception not actual for mysql) */
    if (!metadata_id && _default)
      query.append(_default);
    else
      return 1;
  }

  return 0;
}


/*
@type    :  internal
@purpose :  Adding name condition for arguments what depending on SQL_ATTR_METADATA_ID
are either pattern value(oa) or identifier string(id)
@returns :  1 if required parameter is NULL, 0 otherwise
*/
int add_name_condition_pv_id(HSTMT hstmt, std::string &query, SQLCHAR * name,
                                       SQLSMALLINT name_len, const char * _default)
{
  SQLUINTEGER metadata_id;
  /* this shouldn't be very expensive, so no need to complicate things with additional parameter etc */
  MySQLGetStmtAttr(hstmt, SQL_ATTR_METADATA_ID, (SQLPOINTER)&metadata_id, 0, NULL);

  /* we can't rely here that column was checked and is not null */
  if (name)
  {
    STMT *stmt= (STMT *) hstmt;

    if (metadata_id)
    {
      query.append("=");
      /* Need also code to remove trailing blanks */
    }
    else
      query.append(" LIKE BINARY ");

    query.append("'");
    char tmpbuff[1024];
    size_t cnt = mysql_real_escape_string(stmt->dbc->mysql, tmpbuff,
                                          (char *)name, name_len);
    query.append(tmpbuff, cnt);
    query.append("' ");
  }
  else
  {
    /* According to http://msdn.microsoft.com/en-us/library/ms714579%28VS.85%29.aspx
       identifier argument cannot be NULL with one exception not actual for mysql) */
    if (!metadata_id && _default)
      query.append(_default);
    else
      return 1;
  }

  return 0;
}

/*
  Check if no_catalog is set, but catalog is specified:
  CN is non-NULL pointer AND *CN is not an empty string AND
  CL the length is not zero

  Check if no_schema is set, but schema is specified:
  SN is non-NULL pointer AND *SN is not an empty string AND
  SL the length is not zero

  Check if catalog and schema were specified together and
  return an error if they were.
*/
#define CHECK_CATALOG_SCHEMA(ST, CN, CL, SN, SL) \
  if (ST->dbc->ds.opt_NO_CATALOG && CN && *CN && CL) \
    return ST->set_error("HY000", "Support for catalogs is disabled by " \
           "NO_CATALOG option, but non-empty catalog is specified.", 0); \
  if (ST->dbc->ds.opt_NO_SCHEMA && SN && *SN && SL) \
    return ST->set_error("HY000", "Support for schemas is disabled by " \
           "NO_SCHEMA option, but non-empty schema is specified.", 0); \
  if (CN && *CN && CL && SN && *SN && SL) \
    return ST->set_error("HY000", "Catalog and schema cannot be specified " \
           "together in the same function call.", 0);


/*
****************************************************************************
SQLTables
****************************************************************************
*/

SQLRETURN
tables_i_s(SQLHSTMT hstmt,
           SQLCHAR *catalog_name, SQLSMALLINT catalog_len,
           SQLCHAR *schema_name, SQLSMALLINT schema_len,
           SQLCHAR *table_name, SQLSMALLINT table_len,
           SQLCHAR *type_name, SQLSMALLINT type_len)
{
  /* The function is just a stub. We call non-I_S version of the function before implementing the I_S one */
  return tables_no_i_s(hstmt, catalog_name, catalog_len, schema_name, schema_len,
                       table_name, table_len, type_name, type_len);
}


SQLRETURN SQL_API
MySQLTables(SQLHSTMT hstmt,
            SQLCHAR *catalog_name, SQLSMALLINT catalog_len,
            SQLCHAR *schema_name, SQLSMALLINT schema_len,
            SQLCHAR *table_name, SQLSMALLINT table_len,
            SQLCHAR *type_name, SQLSMALLINT type_len)
{
  STMT *stmt= (STMT *)hstmt;

  CLEAR_STMT_ERROR(hstmt);
  my_SQLFreeStmt(hstmt, FREE_STMT_RESET);

  GET_NAME_LEN(stmt, catalog_name, catalog_len);
  GET_NAME_LEN(stmt, schema_name, schema_len);
  GET_NAME_LEN(stmt, table_name, table_len);
  GET_NAME_LEN(stmt, type_name, type_len);

  CHECK_CATALOG_SCHEMA(stmt, catalog_name, catalog_len,
                       schema_name, schema_len);

  return tables_i_s(hstmt, catalog_name, catalog_len, schema_name, schema_len,
                    table_name, table_len, type_name, type_len);
}


/*
****************************************************************************
SQLColumns
****************************************************************************
*/
typedef std::vector<MYSQL_BIND> vec_bind;

/**
  Compute the buffer length for SQLColumns.
  Mostly used for numeric values when I_S.COLUMNS.CHARACTER_OCTET_LENGTH is
  NULL.
*/
SQLLEN get_buffer_length(const char *type_name, const char *ch_size,
  const char *ch_buflen, SQLSMALLINT sqltype, size_t col_size,
  bool is_null)
{
  size_t decimal_digits = 0;
  bool is_unsigned = false;
  // Check the type name
  if (type_name)
    is_unsigned = (bool)strstr(type_name, "unsigned");

  switch (sqltype)
  {
    case SQL_DECIMAL:
      decimal_digits = std::strtoll(ch_size, nullptr, 10);
      return decimal_digits + (is_unsigned ? 1 : 2);

    case SQL_TINYINT:
      return 1;

    case SQL_SMALLINT:
      return 2;

    case SQL_INTEGER:
      return 4;

    case SQL_REAL:
      return 4;

    case SQL_DOUBLE:
      return 8;

    case SQL_BIGINT:
      return 20;

    case SQL_DATE:
      return sizeof(SQL_DATE_STRUCT);

    case SQL_TIME:
      return sizeof(SQL_TIME_STRUCT);

    case SQL_TIMESTAMP:
      return sizeof(SQL_TIMESTAMP_STRUCT);

    case SQL_BIT:
      return col_size;
  }

  if (is_null)
    return 0;

  return (SQLULEN)std::strtoll(ch_buflen, nullptr, 10);
}


/**
  Get information about the columns in one or more tables.

  @param[in] hstmt           Handle of statement
  @param[in] catalog    Name of catalog (database)
  @param[in] catalog_len     Length of catalog
  @param[in] schema     Name of schema (unused)
  @param[in] schema_len      Length of schema name
  @param[in] table      Pattern of table names to match
  @param[in] table_len       Length of table pattern
  @param[in] column     Pattern of column names to match
  @param[in] column_len      Length of column pattern
*/
SQLRETURN
columns_i_s(SQLHSTMT hstmt, SQLCHAR *catalog, unsigned long catalog_len,
            SQLCHAR *schema, unsigned long schema_len,
            SQLCHAR *table, unsigned long table_len,
            SQLCHAR *column, unsigned long column_len)
{
  STMT *stmt = (STMT*)hstmt;
  vec_bind params, ssps_res;
  const size_t ccount = 19;
  std::vector<tempBuf> bufs;
  bufs.reserve(ccount);
  params.reserve(4);
  ssps_res.reserve(ccount);
  unsigned long lengths[ccount];
  bool is_null[ccount];
  tempBuf temp(1024);
  MYSQL_RES *mysql_res = nullptr;
  MYSQL_ROW mysql_row = nullptr;
  // Use statically allocated array of pointers for STMT result processing
  char *mysql_stmt_row[ccount];
  bool no_ssps = stmt->dbc->ds.opt_NO_SSPS;

  std::string query;
  query.reserve(2048);
  query = "select "
    "TABLE_SCHEMA as TABLE_CATALOG,"
    "TABLE_SCHEMA,"
    "TABLE_NAME,"
    "COLUMN_NAME,"
    "DATA_TYPE,"
    "COLUMN_TYPE as TYPE_NAME,"
    "IF(ISNULL(CHARACTER_MAXIMUM_LENGTH),"
    "  CASE DATA_TYPE"
    "  WHEN 'bit' THEN CAST((NUMERIC_PRECISION + 7) / 8 AS UNSIGNED)"
    "  WHEN 'json' THEN 1073741823"
    "  ELSE NUMERIC_PRECISION"
    "  END, CHARACTER_MAXIMUM_LENGTH) as "
    "COLUMN_SIZE,"
    "CASE DATA_TYPE"
    "  WHEN 'json' THEN 4294967295"
    "  ELSE CHARACTER_OCTET_LENGTH"
    "  END as "
    "BUFFER_LENGTH,"
    "NUMERIC_SCALE as DECIMAL_DIGITS,"
    "IF(ISNULL(NUMERIC_PRECISION), NULL, 10) as NUM_PREC_RADIX,"
    "IF(EXTRA LIKE \"%auto_increment%\", \"YES\", IS_NULLABLE) as NULLABLE,"
    "COLUMN_COMMENT as REMARKS,"
    "IF(ISNULL(COLUMN_DEFAULT), \"NULL\", IF(ISNULL(NUMERIC_PRECISION), CONCAT(\"\'\", COLUMN_DEFAULT, \"\'\"),COLUMN_DEFAULT)) as COLUMN_DEF,"
    "0 as SQL_DATA_TYPE,"
    "NULL as SQL_DATA_TYPE_SUB,"
    "CASE DATA_TYPE"
    "  WHEN 'json' THEN 4294967295"
    "  ELSE CHARACTER_OCTET_LENGTH"
    "  END as "
    "CHAR_OCTET_LENGTH,"
    "ORDINAL_POSITION,"
    "IF(EXTRA LIKE \"%auto_increment%\", \"YES\", IS_NULLABLE) AS IS_NULLABLE,"
    "IF(DATA_TYPE like 'json', 4, MAXLEN) as CHAR_SIZE "
    "FROM information_schema.COLUMNS c "
    "LEFT JOIN information_schema.CHARACTER_SETS cs ON c.CHARACTER_SET_NAME = cs.CHARACTER_SET_NAME "
    "WHERE 1=1";

  auto do_bind = [&query, &stmt, &temp, &no_ssps]
    (vec_bind &par, SQLCHAR *data,
    enum_field_types buf_type, unsigned long &len,
    bool *isnull = nullptr)
  {
    if (no_ssps)
    {
      query.append("'");
      auto cnt = mysql_real_escape_string(stmt->dbc->mysql,
        temp.buf, (char*)data, len);
      query.append(temp.buf, cnt);
      query.append("'");
    }
    else
    {
      par.emplace_back();
      MYSQL_BIND &b = par.back();
      memset(&b, 0, sizeof(b));
      b.buffer_type = buf_type;
      b.buffer = data;
      b.length = &len;
      b.buffer_length = len;

      if (isnull != nullptr)
      {
        b.is_null = isnull;
      }
      else
      {
        // Append "?" only for param, but not for result
        query.append("?");
      }
    }
  };

  if (catalog && catalog_len)
  {
    query.append(" AND c.TABLE_SCHEMA LIKE ");
    do_bind(params, catalog, MYSQL_TYPE_STRING, catalog_len);
  }
  else if (schema && schema_len)
  {
    query.append(" AND c.TABLE_SCHEMA LIKE ");
    do_bind(params, schema, MYSQL_TYPE_STRING, schema_len);
  }
  else
  {
    query.append(" AND c.TABLE_SCHEMA=DATABASE()");
  }

  if (table && table_len)
  {
    query.append(" AND c.TABLE_NAME LIKE ");
    do_bind(params, table, MYSQL_TYPE_STRING, table_len);
  }

  if (column && column_len)
  {
    query.append(" AND c.COLUMN_NAME LIKE ");
    do_bind(params, column, MYSQL_TYPE_STRING, column_len);
  }
  query.append(" ORDER BY ORDINAL_POSITION");
  ODBC_STMT local_stmt(stmt->dbc->mysql);

  if (!no_ssps)
  {
    // Bind results only for SSPS.
    for (size_t i = 0; i < ccount; i++)
    {
      bufs.emplace_back(tempBuf(1024));
      auto &buf = bufs.back();
      buf.buf[0] = 0;
      lengths[i] = (unsigned long)buf.buf_len;
      do_bind(ssps_res, (SQLCHAR*)buf.buf, MYSQL_TYPE_STRING,
        lengths[i], &is_null[i]);
    }
  }

  // Get DB name before running query or executing stmt.
  std::string db = get_database_name(stmt, catalog, catalog_len,
    schema, schema_len, false);

  try
  {
    if(set_sql_select_limit(stmt->dbc, stmt->stmt_options.max_rows, false))
    {
      stmt->set_error("HY000");
      throw stmt->error;
    }

    MYLOG_QUERY(stmt, query.c_str());
    if (!no_ssps)
    {
      // In case of SSPS the call will throw an error.
      stmt->dbc->execute_prep_stmt(local_stmt, query,
        params.data(), ssps_res.data());
    }
    else
    {
      if (SQL_SUCCESS != stmt->dbc->execute_query(query.c_str(),
        query.length(), true))
      {
        // Throwing the error for NO_SSPS case.
        throw stmt->dbc->error;
      }
      mysql_res = mysql_store_result(stmt->dbc->mysql);
    }
  }
  catch (const MYERROR &e)
  {
    return e.retcode;
  }
  if (!stmt->m_row_storage.is_valid())
    stmt->result_array.reset();

  bool is_access = false;
#ifdef _WIN32
  if (GetModuleHandle("msaccess.exe") != NULL)
    is_access = true;
#endif

  size_t rows = no_ssps ? mysql_num_rows(mysql_res) :
    mysql_stmt_num_rows(local_stmt);
  stmt->m_row_storage.set_size(rows, SQLCOLUMNS_FIELDS);
  if (rows == 0)
  {
    return create_empty_fake_resultset(stmt, SQLCOLUMNS_values,
     sizeof(SQLCOLUMNS_values), SQLCOLUMNS_fields, SQLCOLUMNS_FIELDS);
  }

  auto &data = stmt->m_row_storage;
  data.first_row();
  size_t rnum = 1;
  // Used for NO_SSPS only.
  unsigned long* result_lengths = nullptr;

  auto _is_null = [&mysql_row, &is_null, &no_ssps, &result_lengths](int colnum)
  {
    return (bool)(no_ssps ?
      (result_lengths[colnum] == 0 && mysql_row[colnum] == nullptr) :
      (is_null[colnum])
      );
  };

  auto _fetch_row = [&no_ssps, &mysql_res, &local_stmt, &ccount,
    &mysql_stmt_row, &ssps_res]()
  {
    if (no_ssps == false)
    {
      if (mysql_stmt_fetch(local_stmt))
        return (MYSQL_ROW)nullptr;
      for (size_t i = 0; i < ccount; ++i)
        mysql_stmt_row[i] = (char*)ssps_res[i].buffer;
      return (MYSQL_ROW)mysql_stmt_row;
    }
    return mysql_fetch_row(mysql_res);
  };

  while((mysql_row = _fetch_row()))
  {
    if (no_ssps)
      result_lengths = mysql_fetch_lengths(mysql_res);

    CAT_SCHEMA_SET(data[0], data[1], db);
    /* TABLE_NAME */
    data[2] = mysql_row[2];
    /* COLUMN_NAME */
    data[3] = mysql_row[3];
    /* DATA_TYPE */
    size_t col_size = _is_null(6) ? 0 :
      get_column_size_from_str(stmt, mysql_row[6]);

    SQLSMALLINT odbc_sql_type =
      get_sql_data_type_from_str(mysql_row[4]);

    const char *char_size = mysql_row[18];
    SQLSMALLINT sqltype =
      compute_sql_data_type(stmt, odbc_sql_type,
      (char_size ? *char_size : '1'), col_size);

    data[4] = sqltype;
    /* TYPE_NAME */
    data[5] = mysql_row[4];
    /* COLUMN_SIZE */
#if _WIN32 && !_WIN64
#define COL_SIZE_VAL (int)col_size
#else
#define COL_SIZE_VAL col_size
#endif
    if (_is_null(6))
      data[6] = nullptr;
    else
      data[6] = COL_SIZE_VAL;
    /* BUFFER_LENGTH */
    data[7] = get_buffer_length(mysql_row[5], mysql_row[6],
      mysql_row[7], odbc_sql_type, col_size, _is_null(7));
    /* DECIMAL_DIGITS */
    data[8] = (char*)(_is_null(8) ? nullptr : mysql_row[8]);
    /* NUM_PREC_RADIX */
    data[9] = (char*)(_is_null(9) ? nullptr : mysql_row[9]);
    /* NULLABLE */
    SQLSMALLINT nullable = (SQLSMALLINT)((*mysql_row[10] == 'Y') ||
      (sqltype == SQL_TIMESTAMP) || (sqltype == SQL_TYPE_TIMESTAMP) ? SQL_NULLABLE : SQL_NO_NULLS);

    if (is_access && nullable == SQL_NO_NULLS)
      nullable = SQL_NULLABLE_UNKNOWN;
    data[10] = nullable;
    /* REMARKS */
    if (_is_null(11))
      data[11] = nullptr;
    else
      data[11] = (!no_ssps && lengths[11]) ||
        (result_lengths && result_lengths[11]) ?
        mysql_row[11] : "";
    /* COLUMN_DEF */
    data[12] = mysql_row[12];

    if (sqltype == SQL_TYPE_DATE || sqltype == SQL_TYPE_TIME ||
        sqltype == SQL_TYPE_TIMESTAMP)
    {
      /* SQL_DATA_TYPE */
      data[13] = SQL_DATETIME;
      /* SQL_DATETIME_SUB */
      data[14] = sqltype;
    }
    else
    {
      /* SQL_DATA_TYPE */
      data[13] = sqltype;
      /* SQL_DATETIME_SUB */
      data[14] = (sqltype == SQL_DATETIME ? SQL_TYPE_TIMESTAMP : 0);
    }

    /* CHAR_OCTET_LENGTH */
    if (_is_null(15))
    {
      if (is_char_sql_type(sqltype) || is_wchar_sql_type(sqltype) ||
          is_binary_sql_type(sqltype))
        data[15] = COL_SIZE_VAL;
      else
        data[15] = nullptr;
    }
    else
    {
      data[15] = mysql_row[15];
    }

    /* ORDINAL_POSITION */
    data[16] = (long long)rnum;
    /* IS_NULLABLE */
    data[17] = nullable ? "YES" : mysql_row[17];
    if(rnum < rows)
      data.next_row();
    ++rnum;
  }

  if (mysql_res)
    mysql_free_result(mysql_res);

  if (rows)
  {
    /*
      With the non-empty result the function should return from this block
    */
    stmt->result_array = (MYSQL_ROW)data.data();
    create_fake_resultset(stmt, stmt->result_array, SQLCOLUMNS_FIELDS, rows,
      SQLCOLUMNS_fields, SQLCOLUMNS_FIELDS, false);
    myodbc_link_fields(stmt, SQLCOLUMNS_fields, SQLCOLUMNS_FIELDS);
    return SQL_SUCCESS;
  }

  create_empty_fake_resultset(stmt, SQLCOLUMNS_values,
                                     SQLCOLUMNS_FIELDS,
                                     SQLCOLUMNS_fields,
                                     SQLCOLUMNS_FIELDS);
  return SQL_SUCCESS;
}


/**
  Get information about the columns in one or more tables.

  @param[in] hstmt            Handle of statement
  @param[in] catalog_name     Name of catalog (database)
  @param[in] catalog_len      Length of catalog
  @param[in] schema_name      Name of schema (unused)
  @param[in] schema_len       Length of schema name
  @param[in] table_name       Pattern of table names to match
  @param[in] table_len        Length of table pattern
  @param[in] column_name      Pattern of column names to match
  @param[in] column_len       Length of column pattern
*/
SQLRETURN SQL_API
MySQLColumns(SQLHSTMT hstmt, SQLCHAR *catalog_name, SQLSMALLINT catalog_len,
             SQLCHAR *schema_name,
             SQLSMALLINT schema_len,
             SQLCHAR *table_name, SQLSMALLINT table_len,
             SQLCHAR *column_name, SQLSMALLINT column_len)

{
  STMT *stmt= (STMT *)hstmt;

  CLEAR_STMT_ERROR(hstmt);
  my_SQLFreeStmt(hstmt, FREE_STMT_RESET);

  GET_NAME_LEN(stmt, catalog_name, catalog_len);
  GET_NAME_LEN(stmt, schema_name, schema_len);
  GET_NAME_LEN(stmt, table_name, table_len);
  GET_NAME_LEN(stmt, column_name, column_len);
  CHECK_CATALOG_SCHEMA(stmt, catalog_name, catalog_len,
                       schema_name, schema_len);

  return columns_i_s(hstmt, catalog_name, catalog_len,schema_name, schema_len,
                     table_name, table_len, column_name, column_len);
}


/*
****************************************************************************
SQLStatistics
****************************************************************************
*/

/*
  @purpose : retrieves a list of statistics about a single table and the
       indexes associated with the table. The driver returns the
       information as a result set.
*/

SQLRETURN
statistics_i_s(SQLHSTMT hstmt,
               SQLCHAR *catalog_name, SQLSMALLINT catalog_len,
               SQLCHAR *schema_name,
               SQLSMALLINT schema_len,
               SQLCHAR *table_name, SQLSMALLINT table_len,
               SQLUSMALLINT fUnique,
               SQLUSMALLINT fAccuracy)
{
  /* The function is just a stub. We call non-I_S version of the function before implementing the I_S one */
  return statistics_no_i_s(hstmt, catalog_name, catalog_len, schema_name, schema_len,
                           table_name, table_len, fUnique, fAccuracy);
}


/*
  @type    : ODBC 1.0 API
  @purpose : retrieves a list of statistics about a single table and the
       indexes associated with the table. The driver returns the
       information as a result set.
*/

SQLRETURN SQL_API
MySQLStatistics(SQLHSTMT hstmt,
                SQLCHAR *catalog_name, SQLSMALLINT catalog_len,
                SQLCHAR *schema_name,
                SQLSMALLINT schema_len,
                SQLCHAR *table_name, SQLSMALLINT table_len,
                SQLUSMALLINT fUnique,
                SQLUSMALLINT fAccuracy)
{
  STMT *stmt= (STMT *)hstmt;

  CLEAR_STMT_ERROR(hstmt);
  my_SQLFreeStmt(hstmt, FREE_STMT_RESET);

  GET_NAME_LEN(stmt, catalog_name, catalog_len);
  GET_NAME_LEN(stmt, schema_name, schema_len);
  GET_NAME_LEN(stmt, table_name, table_len);
  CHECK_CATALOG_SCHEMA(stmt, catalog_name, catalog_len,
                       schema_name, schema_len);

  return statistics_i_s(hstmt, catalog_name, catalog_len, schema_name, schema_len,
                        table_name, table_len, fUnique, fAccuracy);
}

/*
****************************************************************************
SQLTablePrivileges
****************************************************************************
*/
/*
  @type    : internal
  @purpose : fetches data from I_S table_privileges. returns SQLRETURN of the operation
*/
SQLRETURN list_table_priv_i_s(SQLHSTMT    hstmt,
                              SQLCHAR *   catalog_name,
                              SQLSMALLINT catalog_len,
                              SQLCHAR *   schema_name,
                              SQLSMALLINT schema_len,
                              SQLCHAR *   table_name,
                              SQLSMALLINT table_len)
{
  STMT *stmt=(STMT *) hstmt;
  SQLRETURN rc;
  std::string query;
  query.reserve(1024);

  /* Db,User,Table_name,"NULL" as Grantor,Table_priv*/
  if (!schema_len)
    query = "SELECT TABLE_SCHEMA as TABLE_CAT, NULL as TABLE_SCHEM,";
  else
    query = "SELECT NULL as TABLE_CAT, TABLE_SCHEMA as TABLE_SCHEM,";


  query.append("TABLE_NAME, NULL as GRANTOR,GRANTEE,"
          "PRIVILEGE_TYPE as PRIVILEGE,IS_GRANTABLE "
          "FROM INFORMATION_SCHEMA.TABLE_PRIVILEGES "
          "WHERE TABLE_NAME");

  add_name_condition_pv_id(hstmt, query, table_name, table_len, " LIKE '%'" );

  query.append(" AND TABLE_SCHEMA");
  add_name_condition_oa_id(hstmt, query, catalog_name, catalog_len, "=DATABASE()");

  /* TABLE_CAT is always NULL in mysql I_S */
  query.append(" ORDER BY TABLE_CAT, TABLE_SCHEM, TABLE_NAME, PRIVILEGE, GRANTEE");

  if( !SQL_SUCCEEDED(rc= MySQLPrepare(hstmt, (SQLCHAR *)query.c_str(),
                          (SQLINTEGER)(query.length()), true, false)))
    return rc;

  return my_SQLExecute(stmt);
}


/*
  @type    : ODBC 1.0 API
  @purpose : returns a list of tables and the privileges associated with
             each table. The driver returns the information as a result
             set on the specified statement.
*/
SQLRETURN SQL_API
MySQLTablePrivileges(SQLHSTMT hstmt,
                     SQLCHAR *catalog_name, SQLSMALLINT catalog_len,
                     SQLCHAR *schema_name,
                     SQLSMALLINT schema_len,
                     SQLCHAR *table_name, SQLSMALLINT table_len)
{
    STMT     *stmt= (STMT *)hstmt;

    CLEAR_STMT_ERROR(hstmt);
    my_SQLFreeStmt(hstmt, FREE_STMT_RESET);

    GET_NAME_LEN(stmt, catalog_name, catalog_len);
    GET_NAME_LEN(stmt, schema_name, schema_len);
    GET_NAME_LEN(stmt, table_name, table_len);
    CHECK_CATALOG_SCHEMA(stmt, catalog_name, catalog_len,
                         schema_name, schema_len);

      /* Since mysql is also the name of the system db, using here i_s prefix to
         distinct functions */
    return list_table_priv_i_s(hstmt, catalog_name, catalog_len, schema_name, schema_len,
                               table_name, table_len);
}


/*
  @type    : internal
  @purpose : returns a column privileges result, NULL on error
*/
static SQLRETURN list_column_priv_i_s(HSTMT       hstmt,
                                      SQLCHAR *   catalog_name,
                                      SQLSMALLINT catalog_len,
                                      SQLCHAR *   schema_name,
                                      SQLSMALLINT schema_len,
                                      SQLCHAR *   table_name,
                                      SQLSMALLINT table_len,
                                      SQLCHAR *   column_name,
                                      SQLSMALLINT column_len)
{
  STMT *stmt=(STMT *) hstmt;
  /* 3 names theorethically can have all their characters escaped - thus 6*NAME_LEN  */
  SQLRETURN rc;
  std::string query;
  query.reserve(1024);

  /* Db,User,Table_name,"NULL" as Grantor,Table_priv*/
  if (!schema_len)
    query = "SELECT TABLE_SCHEMA as TABLE_CAT, NULL as TABLE_SCHEM,";
  else
    query = "SELECT NULL as TABLE_CAT, TABLE_SCHEMA as TABLE_SCHEM,";

  query.append("TABLE_NAME, COLUMN_NAME, NULL as GRANTOR, GRANTEE,"
          "PRIVILEGE_TYPE as PRIVILEGE, IS_GRANTABLE "
          "FROM INFORMATION_SCHEMA.COLUMN_PRIVILEGES "
          "WHERE TABLE_NAME");

  if(add_name_condition_oa_id(hstmt, query, table_name, table_len, NULL))
    return stmt->set_error("HY009", "Invalid use of NULL pointer(table is required parameter)", 0);

  query.append(" AND TABLE_SCHEMA");
  add_name_condition_oa_id(hstmt, query, catalog_name, catalog_len, "=DATABASE()");

  query.append(" AND COLUMN_NAME");
  add_name_condition_pv_id(hstmt, query, column_name, column_len, " LIKE '%'");

  /* TABLE_CAT is always NULL in mysql I_S */
  query.append(" ORDER BY TABLE_CAT, TABLE_SCHEM, TABLE_NAME, COLUMN_NAME, PRIVILEGE");

  if( !SQL_SUCCEEDED(rc= MySQLPrepare(hstmt, (SQLCHAR *)query.c_str(), SQL_NTS,
                                      true, false)))
    return rc;

  return my_SQLExecute(stmt);
}


SQLRETURN SQL_API
MySQLColumnPrivileges(SQLHSTMT hstmt,
                      SQLCHAR *catalog_name, SQLSMALLINT catalog_len,
                      SQLCHAR *schema_name,
                      SQLSMALLINT schema_len,
                      SQLCHAR *table_name, SQLSMALLINT table_len,
                      SQLCHAR *column_name, SQLSMALLINT column_len)
{
  STMT     *stmt= (STMT *)hstmt;

  CLEAR_STMT_ERROR(hstmt);
  my_SQLFreeStmt(hstmt, FREE_STMT_RESET);

  GET_NAME_LEN(stmt, catalog_name, catalog_len);
  GET_NAME_LEN(stmt, schema_name, schema_len);
  GET_NAME_LEN(stmt, table_name, table_len);
  GET_NAME_LEN(stmt, column_name, column_len);
  CHECK_CATALOG_SCHEMA(stmt, catalog_name, catalog_len,
                       schema_name, schema_len);

  /* Since mysql is also the name of the system db, using here i_s prefix to
  distinct functions */
  return list_column_priv_i_s(hstmt, catalog_name, catalog_len, schema_name, schema_len,
    table_name, table_len, column_name, column_len);
}


/*
****************************************************************************
SQLSpecialColumns
****************************************************************************
*/
SQLRETURN
special_columns_i_s(SQLHSTMT hstmt, SQLUSMALLINT fColType,
                    SQLCHAR *table_qualifier, SQLSMALLINT table_qualifier_len,
                    SQLCHAR *table_owner,
                    SQLSMALLINT table_owner_len,
                    SQLCHAR *table_name, SQLSMALLINT table_len,
                    SQLUSMALLINT fScope,
                    SQLUSMALLINT fNullable)
{
  /* The function is just a stub. We call non-I_S version of the function before implementing the I_S one */
  return special_columns_no_i_s(hstmt, fColType, table_qualifier,
                                table_qualifier_len, table_owner, table_owner_len,
                                table_name, table_len, fScope, fNullable);
}


/*
  @type    : ODBC 1.0 API
  @purpose : retrieves the following information about columns within a
       specified table:
       - The optimal set of columns that uniquely identifies a row
         in the table.
       - Columns that are automatically updated when any value in the
         row is updated by a transaction
*/

SQLRETURN SQL_API
MySQLSpecialColumns(SQLHSTMT hstmt, SQLUSMALLINT fColType,
                    SQLCHAR *catalog, SQLSMALLINT catalog_len,
                    SQLCHAR *schema, SQLSMALLINT schema_len,
                    SQLCHAR *table_name, SQLSMALLINT table_len,
                    SQLUSMALLINT fScope,
                    SQLUSMALLINT fNullable)
{
  STMT        *stmt=(STMT *) hstmt;

  CLEAR_STMT_ERROR(hstmt);
  my_SQLFreeStmt(hstmt, FREE_STMT_RESET);

  GET_NAME_LEN(stmt, catalog, catalog_len);
  GET_NAME_LEN(stmt, schema, schema_len);
  GET_NAME_LEN(stmt, table_name, table_len);
  CHECK_CATALOG_SCHEMA(stmt, catalog, catalog_len,
                       schema, schema_len);

  return special_columns_i_s(hstmt, fColType, catalog,
                             catalog_len, schema, schema_len,
                             table_name, table_len, fScope, fNullable);
}


/*
****************************************************************************
SQLPrimaryKeys
****************************************************************************
*/
SQLRETURN
primary_keys_i_s(SQLHSTMT hstmt,
                 SQLCHAR *catalog_name, SQLSMALLINT catalog_len,
                 SQLCHAR *schema_name,
                 SQLSMALLINT schema_len,
                 SQLCHAR *table_name, SQLSMALLINT table_len)
{
  /* The function is just a stub. We call non-I_S version of the function before implementing the I_S one */
  return primary_keys_no_i_s(hstmt, catalog_name, catalog_len, schema_name, schema_len,
                             table_name, table_len);
}


/*
  @type    : ODBC 1.0 API
  @purpose : returns the column names that make up the primary key for a table.
       The driver returns the information as a result set. This function
       does not support returning primary keys from multiple tables in
       a single call
*/

SQLRETURN SQL_API
MySQLPrimaryKeys(SQLHSTMT hstmt,
                 SQLCHAR *catalog_name, SQLSMALLINT catalog_len,
                 SQLCHAR *schema_name,
                 SQLSMALLINT schema_len,
                 SQLCHAR *table_name, SQLSMALLINT table_len)
{
  STMT *stmt= (STMT *) hstmt;

  CLEAR_STMT_ERROR(hstmt);
  my_SQLFreeStmt(hstmt, FREE_STMT_RESET);

  GET_NAME_LEN(stmt, catalog_name, catalog_len);
  GET_NAME_LEN(stmt, schema_name, schema_len);
  GET_NAME_LEN(stmt, table_name, table_len);
  CHECK_CATALOG_SCHEMA(stmt, catalog_name, catalog_len,
                       schema_name, schema_len);

  return primary_keys_i_s(hstmt, catalog_name, catalog_len, schema_name, schema_len,
                          table_name, table_len);
}


/*
****************************************************************************
SQLForeignKeys
****************************************************************************
*/
SQLRETURN foreign_keys_i_s(SQLHSTMT hstmt,
                           SQLCHAR    *pk_catalog,
                           SQLSMALLINT pk_catalog_len,
                           SQLCHAR    *pk_schema,
                           SQLSMALLINT pk_schema_len,
                           SQLCHAR    *pk_table,
                           SQLSMALLINT pk_table_len,
                           SQLCHAR    *fk_catalog,
                           SQLSMALLINT fk_catalog_len,
                           SQLCHAR    *fk_schema,
                           SQLSMALLINT fk_schema_len,
                           SQLCHAR    *fk_table,
                           SQLSMALLINT fk_table_len)
{
  STMT *stmt=(STMT *) hstmt;
  MYSQL *mysql= stmt->dbc->mysql;
  char tmpbuff[1024]; /* This should be big enough. */
  const char *update_rule, *delete_rule, *ref_constraints_join;
  SQLRETURN rc;
  std::string query, pk_db, fk_db, order_by;
  query.reserve(4096);
  size_t cnt = 0;

  pk_db = get_database_name(stmt, pk_catalog, pk_catalog_len,
                            pk_schema, pk_schema_len, false);
  fk_db = get_database_name(stmt, fk_catalog, fk_catalog_len,
                            fk_schema, fk_schema_len, false);

  /*
     With 5.1, we can use REFERENTIAL_CONSTRAINTS to get even more info.
  */
  if (is_minimum_version(stmt->dbc->mysql->server_version, "5.1"))
  {
    update_rule= "CASE"
                 " WHEN R.UPDATE_RULE = 'CASCADE' THEN 0"
                 " WHEN R.UPDATE_RULE = 'SET NULL' THEN 2"
                 " WHEN R.UPDATE_RULE = 'SET DEFAULT' THEN 4"
                 " WHEN R.UPDATE_RULE = 'SET RESTRICT' THEN 1"
                 " WHEN R.UPDATE_RULE = 'SET NO ACTION' THEN 3"
                 " ELSE 3"
                 " END";
    delete_rule= "CASE"
                 " WHEN R.DELETE_RULE = 'CASCADE' THEN 0"
                 " WHEN R.DELETE_RULE = 'SET NULL' THEN 2"
                 " WHEN R.DELETE_RULE = 'SET DEFAULT' THEN 4"
                 " WHEN R.DELETE_RULE = 'SET RESTRICT' THEN 1"
                 " WHEN R.DELETE_RULE = 'SET NO ACTION' THEN 3"
                 " ELSE 3"
                 " END";

    ref_constraints_join=
      " JOIN INFORMATION_SCHEMA.REFERENTIAL_CONSTRAINTS R"
      " ON (R.CONSTRAINT_NAME = A.CONSTRAINT_NAME"
      " AND R.TABLE_NAME = A.TABLE_NAME"
      " AND R.CONSTRAINT_SCHEMA = A.TABLE_SCHEMA)";
  }
  else
  {
    /* Just return '1' to be compatible with pre-I_S version. */
    update_rule= delete_rule= "1";
    ref_constraints_join= "";
  }

  /* This is a big, ugly query. But it works! */
  if(!pk_schema_len)
    query = "SELECT A.REFERENCED_TABLE_SCHEMA AS PKTABLE_CAT,"
            "NULL AS PKTABLE_SCHEM,";
  else
    query = "SELECT NULL AS PKTABLE_CAT,"
            "A.REFERENCED_TABLE_SCHEMA AS PKTABLE_SCHEM,";

  query.append("A.REFERENCED_TABLE_NAME AS PKTABLE_NAME,"
          "A.REFERENCED_COLUMN_NAME AS PKCOLUMN_NAME,");

  if(!pk_schema_len)
    query.append("A.TABLE_SCHEMA AS FKTABLE_CAT, NULL AS FKTABLE_SCHEM,");
  else
    query.append("NULL AS FKTABLE_CAT, A.TABLE_SCHEMA AS FKTABLE_SCHEM,");

  query.append("A.TABLE_NAME AS FKTABLE_NAME,"
          "A.COLUMN_NAME AS FKCOLUMN_NAME,"
          "A.ORDINAL_POSITION AS KEY_SEQ,");
  query.append(update_rule).append(" AS UPDATE_RULE,").append(delete_rule);
  query.append(" AS DELETE_RULE,"
                "A.CONSTRAINT_NAME AS FK_NAME,"
                "'PRIMARY' AS PK_NAME,"
                "7 AS DEFERRABILITY"
                " FROM INFORMATION_SCHEMA.KEY_COLUMN_USAGE A"
                " JOIN INFORMATION_SCHEMA.KEY_COLUMN_USAGE D"
                " ON (D.TABLE_SCHEMA=A.REFERENCED_TABLE_SCHEMA AND D.TABLE_NAME=A.REFERENCED_TABLE_NAME"
                " AND D.COLUMN_NAME=A.REFERENCED_COLUMN_NAME)");
  query.append(ref_constraints_join).append(" WHERE D.CONSTRAINT_NAME");
  query.append(" IS NOT NULL ");

  if (pk_table && pk_table[0])
  {
    query.append("AND A.REFERENCED_TABLE_SCHEMA = ");
    if (!pk_db.empty())
    {
      query.append("'");
      cnt = mysql_real_escape_string(mysql, tmpbuff, pk_db.c_str(),
                                     (unsigned long)pk_db.length());
      query.append(tmpbuff, cnt);
      query.append("' ");
    }
    else
    {
      query.append("DATABASE() ");
    }

    query.append("AND A.REFERENCED_TABLE_NAME = '");

    cnt = mysql_real_escape_string(mysql, tmpbuff, (char *)pk_table,
                                    pk_table_len);
    query.append(tmpbuff, cnt);
    query.append("' ");

    order_by = " ORDER BY PKTABLE_CAT, PKTABLE_SCHEM, PKTABLE_NAME, KEY_SEQ, FKTABLE_NAME, PKCOLUMN_NAME";
  }

  if (fk_table && fk_table[0])
  {
    query.append(" AND A.TABLE_SCHEMA = ");

    if (!fk_db.empty())
    {
      query.append("'");
      cnt = mysql_real_escape_string(mysql, tmpbuff, fk_db.c_str(),
                                     (unsigned long)fk_db.length());
      query.append(tmpbuff, cnt);
      query.append("' ");
    }
    else
    {
      query.append("DATABASE() ");
    }

    query.append("AND A.TABLE_NAME = '");

    cnt = mysql_real_escape_string(mysql, tmpbuff, (char *)fk_table,
                                    fk_table_len);
    query.append(tmpbuff, cnt);
    query.append("' ");

    order_by = " ORDER BY FKTABLE_CAT, FKTABLE_NAME, KEY_SEQ, PKTABLE_NAME, FKCOLUMN_NAME";
  }

  query.append(order_by);
  rc= MySQLPrepare(hstmt, (SQLCHAR *)query.c_str(), (SQLINTEGER)(query.length()),
                   true, false);

  if (!SQL_SUCCEEDED(rc))
    return rc;

  return my_SQLExecute((STMT*)hstmt);
}
/**
  Retrieve either a list of foreign keys in a specified table, or the list
  of foreign keys in other tables that refer to the primary key in the
  specified table. (We currently only support the former, not the latter.)

  @param[in] hstmt           Handle of statement
  @param[in] pk_catalog Catalog (database) of table with primary key that
                             we want to see foreign keys for
  @param[in] pk_catalog_len  Length of @a pk_catalog
  @param[in] pk_schema  Schema of table with primary key that we want to
                             see foreign keys for (unused)
  @param[in] pk_schema_len   Length of @a pk_schema
  @param[in] pk_table   Table with primary key that we want to see foreign
                             keys for
  @param[in] pk_table_len    Length of @a pk_table
  @param[in] fk_catalog Catalog (database) of table with foreign keys we
                             are interested in
  @param[in] fk_catalog_len  Length of @a fk_catalog
  @param[in] fk_schema  Schema of table with foreign keys we are
                             interested in
  @param[in] fk_schema_len   Length of fk_schema
  @param[in] fk_table   Table with foreign keys we are interested in
  @param[in] fk_table_len    Length of @a fk_table

  @return SQL_SUCCESS

  @since ODBC 1.0
*/
SQLRETURN SQL_API
MySQLForeignKeys(SQLHSTMT hstmt,
                 SQLCHAR *pk_catalog_name,
                 SQLSMALLINT pk_catalog_len,
                 SQLCHAR *pk_schema_name,
                 SQLSMALLINT pk_schema_len,
                 SQLCHAR *pk_table_name, SQLSMALLINT pk_table_len,
                 SQLCHAR *fk_catalog_name, SQLSMALLINT fk_catalog_len,
                 SQLCHAR *fk_schema_name,
                 SQLSMALLINT fk_schema_len,
                 SQLCHAR *fk_table_name, SQLSMALLINT fk_table_len)
{
  STMT *stmt=(STMT *) hstmt;

  CLEAR_STMT_ERROR(hstmt);
  my_SQLFreeStmt(hstmt, FREE_STMT_RESET);

  GET_NAME_LEN(stmt, pk_catalog_name, pk_catalog_len);
  GET_NAME_LEN(stmt, fk_catalog_name, fk_catalog_len);
  GET_NAME_LEN(stmt, pk_schema_name, pk_schema_len);
  GET_NAME_LEN(stmt, fk_schema_name, fk_schema_len);
  GET_NAME_LEN(stmt, pk_table_name, pk_table_len);
  GET_NAME_LEN(stmt, fk_table_name, fk_table_len);

  CHECK_CATALOG_SCHEMA(stmt, pk_catalog_name, pk_catalog_len,
                        pk_schema_name, pk_schema_len);
  CHECK_CATALOG_SCHEMA(stmt, fk_catalog_name, fk_catalog_len,
                        fk_schema_name, fk_schema_len);

  return foreign_keys_i_s(hstmt, pk_catalog_name, pk_catalog_len, pk_schema_name,
                          pk_schema_len, pk_table_name, pk_table_len, fk_catalog_name,
                          fk_catalog_len, fk_schema_name, fk_schema_len,
                          fk_table_name, fk_table_len);
}

/*
****************************************************************************
SQLProcedures and SQLProcedureColumns
****************************************************************************
*/

/**
  Get the list of procedures stored in a catalog (database). This is done by
  generating the appropriate query against INFORMATION_SCHEMA. If no
  database is specified, the current database is used.

  @param[in] hstmt            Handle of statement
  @param[in] catalog_name     Name of catalog (database)
  @param[in] catalog_len      Length of catalog
  @param[in] schema_name      Pattern of schema (unused)
  @param[in] schema_len       Length of schema name
  @param[in] proc_name        Pattern of procedure names to fetch
  @param[in] proc_len         Length of procedure name
*/
SQLRETURN SQL_API
MySQLProcedures(SQLHSTMT hstmt,
                SQLCHAR *catalog_name, SQLSMALLINT catalog_len,
                SQLCHAR *schema_name,
                SQLSMALLINT schema_len,
                SQLCHAR *proc_name, SQLSMALLINT proc_len)
{
  SQLRETURN rc;
  STMT *stmt= (STMT *)hstmt;

  CLEAR_STMT_ERROR(hstmt);
  my_SQLFreeStmt(hstmt, FREE_STMT_RESET);

  GET_NAME_LEN(stmt, catalog_name, catalog_len);
  GET_NAME_LEN(stmt, schema_name, schema_len);
  GET_NAME_LEN(stmt, proc_name, proc_len);
  CHECK_CATALOG_SCHEMA(stmt, catalog_name, catalog_len,
                       schema_name, schema_len);

  std::string query;
  if (!schema_len)
    query = "SELECT ROUTINE_SCHEMA AS PROCEDURE_CAT, NULL AS PROCEDURE_SCHEM,";
  else
    query = "SELECT NULL AS PROCEDURE_CAT, ROUTINE_SCHEMA AS PROCEDURE_SCHEM,";

  /*
    If a catalog (database) was specified, we use that, otherwise we
    look up procedures from the current database. (This is not standard
    behavior, but seems useful.)
  */
  if (catalog_name && proc_name)
    query.append("ROUTINE_NAME AS PROCEDURE_NAME,"
                 "NULL AS NUM_INPUT_PARAMS,"
                 "NULL AS NUM_OUTPUT_PARAMS,"
                 "NULL AS NUM_RESULT_SETS,"
                 "ROUTINE_COMMENT AS REMARKS,"
                 "IF(ROUTINE_TYPE = 'FUNCTION', 2,"
                 "IF(ROUTINE_TYPE= 'PROCEDURE', 1, 0)) AS PROCEDURE_TYPE"
                 "  FROM INFORMATION_SCHEMA.ROUTINES"
                 " WHERE ROUTINE_NAME LIKE ? AND ROUTINE_SCHEMA = ?");
  else if (proc_name)
    query.append("ROUTINE_NAME AS PROCEDURE_NAME,"
                 "NULL AS NUM_INPUT_PARAMS,"
                 "NULL AS NUM_OUTPUT_PARAMS,"
                 "NULL AS NUM_RESULT_SETS,"
                 "ROUTINE_COMMENT AS REMARKS,"
                 "IF(ROUTINE_TYPE = 'FUNCTION', 2,"
                 "IF(ROUTINE_TYPE= 'PROCEDURE', 1, 0)) AS PROCEDURE_TYPE"
                 "  FROM INFORMATION_SCHEMA.ROUTINES"
                 " WHERE ROUTINE_NAME LIKE ?"
                 " AND ROUTINE_SCHEMA = DATABASE()");
  else
    query.append("ROUTINE_NAME AS PROCEDURE_NAME,"
                 "NULL AS NUM_INPUT_PARAMS,"
                 "NULL AS NUM_OUTPUT_PARAMS,"
                 "NULL AS NUM_RESULT_SETS,"
                 "ROUTINE_COMMENT AS REMARKS,"
                 "IF(ROUTINE_TYPE = 'FUNCTION', 2,"
                 "IF(ROUTINE_TYPE= 'PROCEDURE', 1, 0)) AS PROCEDURE_TYPE"
                 " FROM INFORMATION_SCHEMA.ROUTINES"
                 " WHERE ROUTINE_SCHEMA = DATABASE()");

  rc= MySQLPrepare(hstmt, (SQLCHAR*)query.c_str(), SQL_NTS,
                   true, false);

  if (!SQL_SUCCEEDED(rc))
    return rc;

  if (proc_name)
  {
    rc= my_SQLBindParameter(hstmt, 1, SQL_PARAM_INPUT, SQL_C_CHAR, SQL_C_CHAR,
                            0, 0, proc_name, proc_len, NULL);
    if (!SQL_SUCCEEDED(rc))
      return rc;
  }

  if (catalog_name)
  {
    rc= my_SQLBindParameter(hstmt, 2, SQL_PARAM_INPUT, SQL_C_CHAR,
                            SQL_C_CHAR, 0, 0, catalog_name, catalog_len,
                            NULL);
    if (!SQL_SUCCEEDED(rc))
      return rc;
  }

  return my_SQLExecute((STMT*)hstmt);
}


/*
****************************************************************************
SQLProcedure Columns
****************************************************************************
*/

/*
  @purpose : returns the list of input and output parameters, as well as
  the columns that make up the result set for the specified
  procedures. The driver returns the information as a result
  set on the specified statement
*/
SQLRETURN
procedure_columns_i_s(SQLHSTMT hstmt,
                      SQLCHAR *catalog_name, SQLSMALLINT catalog_len,
                      SQLCHAR *schema_name,
                      SQLSMALLINT schema_len,
                      SQLCHAR *proc_name, SQLSMALLINT proc_len,
                      SQLCHAR *column_name, SQLSMALLINT column_len)
{
  /* The function is just a stub. We call non-I_S version of the function before implementing the I_S one */
  return procedure_columns_no_i_s(hstmt, catalog_name, catalog_len, schema_name,
                                  schema_len, proc_name, proc_len, column_name,
                                  column_len);
}


/*
  @type    : ODBC 1.0 API
  @purpose : returns the list of input and output parameters, as well as
  the columns that make up the result set for the specified
  procedures. The driver returns the information as a result
  set on the specified statement
*/

SQLRETURN SQL_API
MySQLProcedureColumns(SQLHSTMT hstmt,
                    SQLCHAR *catalog_name, SQLSMALLINT catalog_len,
                    SQLCHAR *schema_name,
                    SQLSMALLINT schema_len,
                    SQLCHAR *proc_name, SQLSMALLINT proc_len,
                    SQLCHAR *column_name, SQLSMALLINT column_len)
{
  STMT *stmt= (STMT *)hstmt;

  CLEAR_STMT_ERROR(hstmt);
  my_SQLFreeStmt(hstmt, FREE_STMT_RESET);

  GET_NAME_LEN(stmt, catalog_name, catalog_len);
  GET_NAME_LEN(stmt, schema_name, schema_len);
  GET_NAME_LEN(stmt, proc_name, proc_len);
  GET_NAME_LEN(stmt, column_name, column_len);
  CHECK_CATALOG_SCHEMA(stmt, catalog_name, catalog_len,
                       schema_name, schema_len);

  return procedure_columns_i_s(hstmt, catalog_name, catalog_len, schema_name,
                               schema_len, proc_name, proc_len, column_name,
                               column_len);
}
