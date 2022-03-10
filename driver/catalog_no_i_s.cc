// Copyright (c) 2000, 2018, Oracle and/or its affiliates. All rights reserved.
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
  @file   catalog_no_i_s.c
  @brief  Catalog functions not using I_S.
  @remark All functions suppose that parameters specifying other parameters lenthes can't SQL_NTS.
          caller should take care of that.
*/

#include "driver.h"
#include "catalog.h"


/*
  Helper function to deduce the MySQL database name from catalog,
  schema or current database taking into account NO_CATALOG and NO_SCHEMA
  options.
*/
std::string get_database_name(STMT *stmt,
                              SQLCHAR *catalog, SQLSMALLINT catalog_len,
                              SQLCHAR *schema, SQLSMALLINT schema_len,
                              bool try_reget)
{
  std::string db;
  if (!stmt->dbc->ds->no_catalog && catalog && catalog_len)
  {
    // Catalog parameter can be used
    db = std::string((char*)catalog, catalog_len);
  }
  else if(!stmt->dbc->ds->no_schema && schema && schema_len)
  {
    // Schema parameter can be used
    db = std::string((char*)schema, schema_len);
  }
  else if (!stmt->dbc->ds->no_catalog || !stmt->dbc->ds->no_schema)
  {
    if (!try_reget)
      return db;

    reget_current_catalog(stmt->dbc);

    db = !stmt->dbc->database.empty() ? stmt->dbc->database : "null";
  }
  return db;
}

/*
  @type    : internal
  @purpose : validate for give table type from the list
*/
static my_bool check_table_type(const SQLCHAR *TableType,
                                const char *req_type,
                                uint       len)
{
    char    req_type_quoted[NAME_LEN+2], req_type_quoted1[NAME_LEN+2];
    char    *type, *table_type= (char *)TableType;
    my_bool found= 0;

    if ( !TableType || !TableType[0] )
        return found;

    /*
      Check and return only 'user' tables from current DB and
      don't return any tables when its called with
      SYSTEM TABLES.

      Expected Types:
        "TABLE", "VIEW", "SYSTEM TABLE", "GLOBAL TEMPORARY",
        "LOCAL TEMPORARY", "ALIAS", "SYNONYM",
    */

    type= strstr(table_type,",");
    sprintf(req_type_quoted,"'%s'",req_type);
    sprintf(req_type_quoted1,"`%s`",req_type);
    while ( type )
    {
        while ( isspace(*(table_type)) ) ++table_type;
        if ( !myodbc_casecmp(table_type,req_type,len) ||
             !myodbc_casecmp(table_type,req_type_quoted,len+2) ||
             !myodbc_casecmp(table_type,req_type_quoted1,len+2) )
        {
            found= 1;
            break;
        }
        table_type= ++type;
        type= strstr(table_type,",");
    }
    if ( !found )
    {
        while ( isspace(*(table_type)) ) ++table_type;
        if ( !myodbc_casecmp(table_type,req_type,len) ||
             !myodbc_casecmp(table_type,req_type_quoted,len+2) ||
             !myodbc_casecmp(table_type,req_type_quoted1,len+2) )
            found= 1;
    }
    return found;
}


static MYSQL_ROW fix_fields_copy(STMT *stmt,MYSQL_ROW row)
{
    uint i;
    for ( i=0 ; i < stmt->order_count; ++i )
        stmt->array[stmt->order[i]]= row[i];
    return stmt->array;
}


/*
  @type    : internal
  @purpose : returns columns from a particular table, NULL on error
*/
static MYSQL_RES *server_list_dbkeys(STMT *stmt,
                                     SQLCHAR *catalog,
                                     SQLSMALLINT catalog_len,
                                     SQLCHAR *table,
                                     SQLSMALLINT table_len)
{
    DBC   *dbc = stmt->dbc;
    MYSQL *mysql= dbc->mysql;
    char  tmpbuff[1024];
    std::string query;
    query.reserve(1024);
    size_t cnt = 0;

    query = "SHOW KEYS FROM `";

    if (catalog_len)
    {
      cnt = myodbc_escape_string(stmt, tmpbuff, (ulong)sizeof(tmpbuff),
                                (char *)catalog, catalog_len, 1);
      query.append(tmpbuff, cnt);
      query.append("`.`");
    }

    cnt = myodbc_escape_string(stmt, tmpbuff, (ulong)sizeof(tmpbuff),
                              (char *)table, table_len, 1);
    query.append(tmpbuff, cnt);
    query.append("`");

    MYLOG_DBC_QUERY(dbc, query.c_str());
    if (exec_stmt_query(stmt, query.c_str(), query.length(), FALSE))
        return NULL;
    return mysql_store_result(mysql);
}


/**
  Get the list of columns in a table matching a wildcard.

  @param[in] stmt             Statement
  @param[in] szCatalog        Name of catalog (database)
  @param[in] catalog_len        Length of catalog
  @param[in] szTable          Name of table
  @param[in] table_len          Length of table
  @param[in] column         Pattern of column names to match
  @param[in] column_len         Length of column pattern

  @return Result of mysql_list_fields, or NULL if there is an error
*/
static MYSQL_RES *
server_list_dbcolumns(STMT *stmt,
                      SQLCHAR *szCatalog, SQLSMALLINT cbCatalog,
                      SQLCHAR *szTable, SQLSMALLINT cbTable,
                      SQLCHAR *szColumn, SQLSMALLINT cbColumn)
{
  DBC *dbc= stmt->dbc;
  MYSQL *mysql= dbc->mysql;
  MYSQL_RES *result;
  char buff[NAME_LEN * 2 + 64], column_buff[NAME_LEN * 2 + 64];

  LOCK_STMT(stmt);

  /* If a catalog was specified, we have to change working catalog
     to be able to use mysql_list_fields. */
  if (cbCatalog)
  {
    if (reget_current_catalog(dbc))
      return NULL;


    strncpy(buff, (const char*)szCatalog, cbCatalog);
    buff[cbCatalog]= '\0';

    if (mysql_select_db(mysql, buff))
    {
      return NULL;
    }
  }

  strncpy(buff, (const char*)szTable, cbTable);
  buff[cbTable]= '\0';
  strncpy(column_buff, (const char*)szColumn, cbColumn);
  column_buff[cbColumn]= '\0';

  result= mysql_list_fields(mysql, buff, column_buff);

  /* If before this call no database were selected - we cannot revert that */
  if (cbCatalog && !dbc->database.empty())
  {
    if (mysql_select_db( mysql, dbc->database.c_str()))
    {
      /* Well, probably have to return error here */
      mysql_free_result(result);
      return NULL;
    }
  }

  return result;
}


/**
  Get information about the columns in one or more tables.

  @param[in] hstmt            Handle of statement
  @param[in] catalog          Name of catalog (database)
  @param[in] catalog_len      Length of catalog
  @param[in] schema           Name of schema (unused)
  @param[in] schema_len       Length of schema name
  @param[in] table            Pattern of table names to match
  @param[in] table_len        Length of table pattern
  @param[in] column           Pattern of column names to match
  @param[in] column_len       Length of column pattern
*/
SQLRETURN
columns_no_i_s(STMT * stmt, SQLCHAR *catalog, SQLSMALLINT catalog_len,
               SQLCHAR *schema, SQLSMALLINT schema_len,
               SQLCHAR *table, SQLSMALLINT table_len,
               SQLCHAR *column, SQLSMALLINT column_len)

{
  MYSQL_RES *res;
  MEM_ROOT *alloc;
  MYSQL_ROW table_row;
  unsigned long rows= 0, next_row= 0, *lengths;
  std::string db;
  BOOL is_access= FALSE;

  if (column_len > NAME_LEN || table_len > NAME_LEN || catalog_len > NAME_LEN)
  {
    return stmt->set_error("HY090", "Invalid string or buffer length", 4001);
  }

  db = get_database_name(stmt, catalog, catalog_len, schema, schema_len, false);

  /* Get the list of tables that match szCatalog and table */
  LOCK_STMT(stmt);
  res= table_status(stmt, (SQLCHAR*)db.c_str(), db.length(),
                    table, table_len, TRUE, TRUE, TRUE);

  if (!res && mysql_errno(stmt->dbc->mysql))
  {
    SQLRETURN rc= handle_connection_error(stmt);
    return rc;
  }
  else if (!res)
  {
    goto empty_set;
  }

#ifdef _WIN32
  if (GetModuleHandle("msaccess.exe") != NULL)
    is_access= TRUE;
#endif

  stmt->result= res;
  alloc= &stmt->alloc_root;

  while ((table_row= mysql_fetch_row(res)))
  {
    MYSQL_FIELD *field;
    MYSQL_RES *table_res;
    int count= 0;

    /* Get list of columns matching column for each table. */
    lengths= mysql_fetch_lengths(res);
    table_res= server_list_dbcolumns(stmt, (SQLCHAR*)db.c_str(), db.length(),
                                     (SQLCHAR *)table_row[0],
                                     (SQLSMALLINT)lengths[0],
                                     column, column_len);

    if (!table_res)
    {
      return handle_connection_error(stmt);
    }

    rows+= mysql_num_fields(table_res);

    if (!stmt->m_row_storage.is_valid())
      x_free(stmt->result_array);

    // We will use the ROW_STORAGE here
    stmt->m_row_storage.set_size(rows, SQLCOLUMNS_FIELDS);
    auto &data = stmt->m_row_storage;

    while (field = mysql_fetch_field(table_res))
    {
      SQLSMALLINT type;
      char buff[255]; /* @todo justify the size of this buffer */

      CAT_SCHEMA_SET(data[0], data[1], db);

      /* TABLE_NAME */
      data[2] = field->table;

      /* COLUMN_NAME */
      data[3] = field->name;

      /* TYPE_NAME */
      type= get_sql_data_type(stmt, field, buff);
      data[5] = buff;

      /* DATA_TYPE */
      data[4] = type;

      if (type == SQL_TYPE_DATE || type == SQL_TYPE_TIME ||
          type == SQL_TYPE_TIMESTAMP)
      {
        /* SQL_DATETIME_SUB */
        data[14] = type;
        /* SQL_DATA_TYPE */
        data[13] = SQL_DATETIME;
      }
      else
      {
        /* SQL_DATA_TYPE */
        data[13] = type;
        /* SQL_DATETIME_SUB */
        data[14] = nullptr;
      }

      /* COLUMN_SIZE */
      fill_column_size_buff(buff, stmt, field);
      data[6]= buff;

      /* BUFFER_LENGTH */
      SQLLEN buf_len  = get_transfer_octet_length(stmt, field);
      data[7] = buf_len;

      /* CHAR_OCTET_LENGTH */
      if (is_char_sql_type(type) || is_wchar_sql_type(type) ||
          is_binary_sql_type(type))
        data[15] = buf_len;
      else
        data[15] = nullptr;

      {
        SQLSMALLINT digits= get_decimal_digits(stmt, field);
        if (digits != SQL_NO_TOTAL)
        {
          /* DECIMAL_DIGITS */
          data[8] = digits;
          /* NUM_PREC_RADIX */
          data[9] = "10";
        }
        else
        {
          /* DECIMAL_DIGITS */
          data[8] = nullptr;
          /* NUM_PREC_RADIX */
          data[9] = nullptr;
        }
      }

      /*
        If a field is a TIMESTAMP, NULL can be stored to it (although it gets turned into
        something else).

        The same logic applies to fields with AUTO_INCREMENT_FLAG set.
      */
      if ((field->flags & NOT_NULL_FLAG) && !(field->type == MYSQL_TYPE_TIMESTAMP) &&
          !(field->flags & AUTO_INCREMENT_FLAG))
      {
        /* Bug#31067. Access seems to try to put NULL value when not null field
           is cleared. And that contradicts with its knowledge of that the field
           is not nullable, and it yields an error. Here is a little trick for
           such case - we don't tell Access the whole truth we know, and
           return for such field SQL_NULLABLE_UNKNOWN instead*/

        /* NULLABLE */
        if (is_access)
          data[10] = SQL_NULLABLE_UNKNOWN;
        else
          data[10] = (long long)SQL_NO_NULLS;
        /* IS_NULLABLE */
        data[17] = "NO";
      }
      else
      {
        /* NULLABLE */
        data[10] = SQL_NULLABLE;
        /* IS_NULLABLE */
        data[17]= "YES";
      }
      /* REMARKS */
      data[11] = "";

      /*
        The default value of the column. The value in this column should be
        interpreted as a string if it is enclosed in quotation marks.

        if NULL was specified as the default value, then this column is the
        word NULL, not enclosed in quotation marks. If the default value
        cannot be represented without truncation, then this column contains
        TRUNCATED, with no enclosing single quotation marks. If no default
        value was specified, then this column is NULL.

        The value of COLUMN_DEF can be used in generating a new column
        definition, except when it contains the value TRUNCATED
      */

      if (!field->def)
        /* COLUMN_DEF */
        data[12] = nullptr;
      else
      {
        if (field->type == MYSQL_TYPE_TIMESTAMP &&
            !strcmp(field->def,"0000-00-00 00:00:00"))
        {
          /* COLUMN_DEF */
          data[12] = nullptr;
        }
        else
        {
          std::string def;
          if (is_numeric_mysql_type(field))
          {
            def = field->def;
          }
          else
          {
            def.append("'").append(field->def).append("'");
          }
          /* COLUMN_DEF */
          data[12] = def;
        }
      }

      /* ORDINAL_POSITION */
      data[16] = ++count;
      data.next_row();
    }
    stmt->result_array = (MYSQL_ROW)data.data();

    mysql_free_result(table_res);
  }

  set_row_count(stmt, rows);
  myodbc_link_fields(stmt, SQLCOLUMNS_fields, SQLCOLUMNS_FIELDS);

  return SQL_SUCCESS;

empty_set:
  return create_empty_fake_resultset(stmt, SQLCOLUMNS_values,
                                     sizeof(SQLCOLUMNS_values),
                                     SQLCOLUMNS_fields,
                                     SQLCOLUMNS_FIELDS);
}


/****************************************************************************
SQLTablePrivileges
****************************************************************************
*/
/*
  @type    : internal
  @purpose : checks for the grantability
*/
static my_bool is_grantable(char *grant_list)
{
    char *grant=dupp_str(grant_list,SQL_NTS);;
    if ( grant_list && grant_list[0] )
    {
        char seps[]   = ",";
        char *token;
        token = strtok( grant, seps );
        while ( token != NULL )
        {
            if ( !strcmp(token,"Grant") )
            {
                x_free(grant);
                return(1);
            }
            token = strtok( NULL, seps );
        }
    }
    x_free(grant);
    return(0);
}


/*
@type    : internal
@purpose : returns a table privileges result, NULL on error. Uses mysql pk_db tables
*/
static MYSQL_RES *table_privs_raw_data( STMT *      stmt,
                                        SQLCHAR *   catalog,
                                        SQLSMALLINT catalog_len,
                                        SQLCHAR *   table,
                                        SQLSMALLINT table_len)
{
  DBC *dbc= stmt->dbc;
  MYSQL *mysql= dbc->mysql;
  char   tmpbuff[1024];
  std::string query;
  size_t cnt = 0;

  query.reserve(1024);
  query = "SELECT Db,User,Table_name,Grantor,Table_priv "
          "FROM mysql.tables_priv WHERE Table_name LIKE '";

  cnt = mysql_real_escape_string(mysql, tmpbuff, (char *)table, table_len);
  query.append(tmpbuff, cnt);

  query.append("' AND Db = ");

  if (catalog_len)
  {
    query.append("'");
    cnt = mysql_real_escape_string(mysql, tmpbuff, (char *)catalog, catalog_len);
    query.append(tmpbuff, cnt);
    query.append("'");
  }
  else
    query.append("DATABASE()");

  query.append(" ORDER BY Db, Table_name, Table_priv, User");

  MYLOG_DBC_QUERY(dbc, query.c_str());
  if (exec_stmt_query(stmt, query.c_str(), query.length(), FALSE))
    return NULL;

  return mysql_store_result(mysql);
}


#define MY_MAX_TABPRIV_COUNT 21
#define MY_MAX_COLPRIV_COUNT 3

char *SQLTABLES_priv_values[]=
{
    NULL,"",NULL,NULL,NULL,NULL,NULL
};

MYSQL_FIELD SQLTABLES_priv_fields[]=
{
  MYODBC_FIELD_NAME("TABLE_CAT", 0),
  MYODBC_FIELD_NAME("TABLE_SCHEM", 0),
  MYODBC_FIELD_NAME("TABLE_NAME", NOT_NULL_FLAG),
  MYODBC_FIELD_NAME("GRANTOR", 0),
  MYODBC_FIELD_NAME("GRANTEE", NOT_NULL_FLAG),
  MYODBC_FIELD_NAME("PRIVILEGE", NOT_NULL_FLAG),
  MYODBC_FIELD_NAME("IS_GRANTABLE", 0),
};

const uint SQLTABLES_PRIV_FIELDS= array_elements(SQLTABLES_priv_values);

/*
  @type    : ODBC 1.0 API
  @purpose : returns a list of tables and the privileges associated with
             each table. The driver returns the information as a result
             set on the specified statement.
*/

SQLRETURN
list_table_priv_no_i_s(SQLHSTMT hstmt,
                       SQLCHAR *catalog, SQLSMALLINT catalog_len,
                       SQLCHAR *schema, SQLSMALLINT schema_len,
                       SQLCHAR *table, SQLSMALLINT table_len)
{
    STMT     *stmt= (STMT *)hstmt;
    char     **row;
    uint     row_count;
    std::string db;

    LOCK_STMT(stmt);

    db = get_database_name(stmt, catalog, catalog_len,
                           schema, schema_len, false);

    stmt->result= table_privs_raw_data(stmt, (SQLCHAR*)db.c_str(), db.length(),
      table, table_len);

    if (!stmt->result)
    {
      SQLRETURN rc= handle_connection_error(stmt);
      return rc;
    }

    // Free if result data was not in row storage
    if (!stmt->m_row_storage.is_valid())
      x_free(stmt->result_array);

    // We will use the ROW_STORAGE here
    stmt->m_row_storage.set_size((ulong)stmt->result->row_count *
      MY_MAX_TABPRIV_COUNT, SQLTABLES_PRIV_FIELDS);
    auto &data = stmt->m_row_storage;

    row_count= 0;

    while ( (row= mysql_fetch_row(stmt->result)) )
    {
      const char  *grants= row[4];
      char  token[NAME_LEN+1];
      const char *grant= (const char *)grants;

      while(true)
      {
        /* TABLE_CAT and TABLE_SCHEMA */
        CAT_SCHEMA_SET(data[0], data[1], row[0]);

        /* TABLE_NAME */
        data[2] = row[2];
        /* GRANTOR */
        data[3] = row[3];
        /* GRANTEE */
        data[4] = row[1];
        /* IS_GRANTABLE */
        data[6] = (char *)(is_grantable(row[4]) ? "YES" : "NO");
        ++row_count;

        if ( !(grant= my_next_token(grant,&grants,token,',')) )
        {
          /* End of grants .. */
          /* PRIVILEGE */
          data[5]= grants;
          data.next_row();
          break;
        }
        /* PRIVILEGE */
        data[5] = token;
        data.next_row();
      }
    }

    stmt->result_array = (MYSQL_ROW)data.data();
    set_row_count(stmt, row_count);
    myodbc_link_fields(stmt,SQLTABLES_priv_fields,SQLTABLES_PRIV_FIELDS);

    return SQL_SUCCESS;
}


/*
****************************************************************************
SQLColumnPrivileges
****************************************************************************
*/
/*
@type    : internal
@purpose : returns a column privileges result, NULL on error
*/
static MYSQL_RES *column_privs_raw_data(STMT *      stmt,
                                        SQLCHAR *   catalog,
                                        SQLSMALLINT catalog_len,
                                        SQLCHAR *   table,
                                        SQLSMALLINT table_len,
                                        SQLCHAR *   column,
                                        SQLSMALLINT column_len)
{
  DBC   *dbc = stmt->dbc;
  MYSQL *mysql = dbc->mysql;

  char tmpbuff[1024];
  std::string query;
  size_t cnt = 0;

  query.reserve(1024);

  query = "SELECT c.Db, c.User, c.Table_name, c.Column_name,"
          "t.Grantor, c.Column_priv, t.Table_priv "
          "FROM mysql.columns_priv AS c, mysql.tables_priv AS t "
          "WHERE c.Table_name = '";

  cnt = mysql_real_escape_string(mysql, tmpbuff, (char *)table, table_len);
  query.append(tmpbuff, cnt);

  query.append("' AND c.Db = ");
  if (catalog_len)
  {
    query.append("'");
    cnt = mysql_real_escape_string(mysql, tmpbuff, (char *)catalog, catalog_len);
    query.append(tmpbuff, cnt);
    query.append("'");
  }
  else
    query.append("DATABASE()");

  query.append("AND c.Column_name LIKE '");
  cnt = mysql_real_escape_string(mysql, tmpbuff, (char *)column, column_len);
  query.append(tmpbuff, cnt);

  query.append("' AND c.Table_name = t.Table_name "
               "ORDER BY c.Db, c.Table_name, c.Column_name, c.Column_priv");

  if (exec_stmt_query(stmt, query.c_str(), query.length(), FALSE))
    return NULL;

  return mysql_store_result(mysql);
}


char *SQLCOLUMNS_priv_values[]=
{
  NULL,"",NULL,NULL,NULL,NULL,NULL,NULL
};

MYSQL_FIELD SQLCOLUMNS_priv_fields[]=
{
  MYODBC_FIELD_NAME("TABLE_CAT", 0),
  MYODBC_FIELD_NAME("TABLE_SCHEM", 0),
  MYODBC_FIELD_NAME("TABLE_NAME", NOT_NULL_FLAG),
  MYODBC_FIELD_NAME("COLUMN_NAME", NOT_NULL_FLAG),
  MYODBC_FIELD_NAME("GRANTOR", 0),
  MYODBC_FIELD_NAME("GRANTEE", NOT_NULL_FLAG),
  MYODBC_FIELD_NAME("PRIVILEGE", NOT_NULL_FLAG),
  MYODBC_FIELD_NAME("IS_GRANTABLE", 0),
};


const uint SQLCOLUMNS_PRIV_FIELDS= array_elements(SQLCOLUMNS_priv_values);


SQLRETURN
list_column_priv_no_i_s(SQLHSTMT hstmt,
                        SQLCHAR *catalog, SQLSMALLINT catalog_len,
                        SQLCHAR *schema, SQLSMALLINT schema_len,
                        SQLCHAR *table, SQLSMALLINT table_len,
                        SQLCHAR *column, SQLSMALLINT column_len)
{
  STMT *stmt=(STMT *) hstmt;
  char     **row;
  uint     row_count;
  std::string db;

  CLEAR_STMT_ERROR(hstmt);
  my_SQLFreeStmt(hstmt,MYSQL_RESET);

  LOCK_STMT(stmt);
  db = get_database_name(stmt, catalog, catalog_len,
                         schema, schema_len, false);

  stmt->result= column_privs_raw_data(stmt,
    (SQLCHAR*)db.c_str(), db.length(),
    table, table_len,
    column, column_len);
  if (!stmt->result)
  {
    SQLRETURN rc= handle_connection_error(stmt);
    return rc;
  }

  // Free if result data was not in row storage
  if (!stmt->m_row_storage.is_valid())
    x_free(stmt->result_array);

  row_count = stmt->result->row_count ?
              (ulong)stmt->result->row_count : 1;

  // We will use the ROW_STORAGE here
  stmt->m_row_storage.set_size(row_count * MY_MAX_COLPRIV_COUNT,
                               SQLCOLUMNS_PRIV_FIELDS);
  auto &data = stmt->m_row_storage;

  row_count= 0;
  while ( (row= mysql_fetch_row(stmt->result)) )
  {
    const char  *grants = row[5];
    char  token[NAME_LEN+1];
    const char *grant= (const char *)grants;

    while(true)
    {
      /* TABLE_CAT and TABLE_SCHEMA */
      CAT_SCHEMA_SET(data[0], data[1], row[0]);

      /* TABLE_NAME */
      data[2] = row[2];
      /* COLUMN_NAME */
      data[3] = row[3];
      /* GRANTOR */
      data[4] = row[4];
      /* GRANTEE */
      data[5] = row[1];
      /* IS_GRANTABLE */
      data[7] = (char*)(is_grantable(row[6]) ? "YES":"NO");
            ++row_count;

      if ( !(grant= my_next_token(grant,&grants,token,',')) )
      {
        /* End of grants .. */
        data[6] = grants;
        data.next_row();
        break;
      }
      data[6] = token;
      data.next_row();
    }
  }

  stmt->result_array = (MYSQL_ROW)data.data();
  set_row_count(stmt, row_count);
  myodbc_link_fields(stmt,SQLCOLUMNS_priv_fields,SQLCOLUMNS_PRIV_FIELDS);
  return SQL_SUCCESS;
}


/**
Get the table status for a table or tables using SHOW TABLE STATUS.
Lengths may not be SQL_NTS.

@param[in] stmt           Handle to statement
@param[in] catalog        Catalog (database) of table, @c NULL for current
@param[in] catalog_length Length of catalog name
@param[in] table          Name of table
@param[in] table_length   Length of table name
@param[in] wildcard       Whether the table name is a wildcard

@return Result of SHOW TABLE STATUS, or NULL if there is an error
or empty result (check mysql_errno(stmt->dbc->mysql) != 0)
*/
MYSQL_RES *table_status_no_i_s(STMT        *stmt,
                               SQLCHAR     *catalog,
                               SQLSMALLINT  catalog_length,
                               SQLCHAR     *table,
                               SQLSMALLINT  table_length,
                               my_bool      wildcard)
{
	MYSQL *mysql= stmt->dbc->mysql;

	char tmpbuff[1024];
  std::string query;
  size_t cnt = 0;

  query.reserve(1024);

	query = "SHOW TABLE STATUS ";
	if (catalog && *catalog)
	{
		query.append("FROM `");
		cnt = myodbc_escape_string(stmt, tmpbuff, (ulong)sizeof(tmpbuff),
			(char *)catalog, catalog_length, 1);
    query.append(tmpbuff, cnt);
		query.append("` ");
	}

	/*
	As a pattern-value argument, an empty string needs to be treated
	literally. (It's not the same as NULL, which is the same as '%'.)
	But it will never match anything, so bail out now.
	*/
	if (table && wildcard && !*table)
		return NULL;

	if (table && *table)
	{
		query.append("LIKE '");
		if (wildcard)
			cnt = mysql_real_escape_string(mysql, tmpbuff, (char *)table, table_length);
		else
			cnt = myodbc_escape_string(stmt, tmpbuff, (ulong)sizeof(tmpbuff),
			(char *)table, table_length, 0);
    query.append(tmpbuff, cnt);
		query.append("'");
	}

  MYLOG_QUERY(stmt, query.c_str());

  if (exec_stmt_query(stmt, query.c_str(), (unsigned long)query.length(), FALSE))
  {
    return NULL;
  }

  return mysql_store_result(mysql);
}


/**
Get the CREATE TABLE statement for the given table.
Lengths may not be SQL_NTS.

@param[in] stmt           Handle to statement
@param[in] catalog        Catalog (database) of table, @c NULL for current
@param[in] catalog_length Length of catalog name
@param[in] table          Name of table
@param[in] table_length   Length of table name

@return Result of SHOW CREATE TABLE , or NULL if there is an error
or empty result (check mysql_errno(stmt->dbc->mysql) != 0)
*/
MYSQL_RES *server_show_create_table(STMT        *stmt,
                                    SQLCHAR     *catalog,
                                    SQLSMALLINT  catalog_length,
                                    SQLCHAR     *table,
                                    SQLSMALLINT  table_length)
{
  MYSQL *mysql= stmt->dbc->mysql;
	char tmpbuff[1024];
  std::string query;
  size_t cnt = 0;

  query.reserve(1024);

  query = "SHOW CREATE TABLE ";
  if (catalog && *catalog)
  {
    query.append(" `").append((char *)catalog).append("`.");
  }

  /* Empty string won't match anything. */
  if (!*table)
    return NULL;

  if (table && *table)
  {
    query.append(" `").append((char *)table).append("`");
  }

  MYLOG_QUERY(stmt, query.c_str());


  if (mysql_real_query(mysql, query.c_str(),(unsigned long)query.length()))
  {
    return NULL;
  }

  return mysql_store_result(mysql);
}


/*
****************************************************************************
SQLForeignKeys
****************************************************************************
*/
MYSQL_FIELD SQLFORE_KEYS_fields[]=
{
  MYODBC_FIELD_NAME("PKTABLE_CAT", 0),
  MYODBC_FIELD_NAME("PKTABLE_SCHEM", 0),
  MYODBC_FIELD_NAME("PKTABLE_NAME", NOT_NULL_FLAG),
  MYODBC_FIELD_NAME("PKCOLUMN_NAME", NOT_NULL_FLAG),
  MYODBC_FIELD_NAME("FKTABLE_CAT", 0),
  MYODBC_FIELD_NAME("FKTABLE_SCHEM", 0),
  MYODBC_FIELD_NAME("FKTABLE_NAME", NOT_NULL_FLAG),
  MYODBC_FIELD_NAME("FKCOLUMN_NAME", NOT_NULL_FLAG),
  MYODBC_FIELD_SHORT("KEY_SEQ", NOT_NULL_FLAG),
  MYODBC_FIELD_SHORT("UPDATE_RULE", 0),
  MYODBC_FIELD_SHORT("DELETE_RULE", 0),
  MYODBC_FIELD_NAME("FK_NAME", 0),
  MYODBC_FIELD_NAME("PK_NAME", 0),
  MYODBC_FIELD_SHORT("DEFERRABILITY", 0),
};

const uint SQLFORE_KEYS_FIELDS= array_elements(SQLFORE_KEYS_fields);

/* Multiple array of Struct to store and sort SQLForeignKeys field */
typedef struct SQL_FOREIGN_KEY_FIELD
{
  char PKTABLE_CAT[NAME_LEN + 1];
  char PKTABLE_SCHEM[NAME_LEN + 1];
  char PKTABLE_NAME[NAME_LEN + 1];
  char PKCOLUMN_NAME[NAME_LEN + 1];
  char FKTABLE_CAT[NAME_LEN + 1];
  char FKTABLE_SCHEM[NAME_LEN + 1];
  char FKTABLE_NAME[NAME_LEN + 1];
  char FKCOLUMN_NAME[NAME_LEN + 1];
  int  KEY_SEQ;
  int  UPDATE_RULE;
  int  DELETE_RULE;
  char FK_NAME[NAME_LEN + 1];
  char PK_NAME[NAME_LEN + 1];
  int  DEFERRABILITY;
} MY_FOREIGN_KEY_FIELD;

char *SQLFORE_KEYS_values[]= {
    NULL,"",NULL,NULL,
    NULL,"",NULL,NULL,
    0,0,0,NULL,NULL,0
};


/*
 * Get a record from the array if exists otherwise allocate a new
 * record and return.
 *
 * @param records MY_FOREIGN_KEY_FIELD record
 * @param recnum  0-based record number
 *
 * @return The requested record or NULL if it doesn't exist
 *         (and isn't created).
 */
MY_FOREIGN_KEY_FIELD *fk_get_rec(DYNAMIC_ARRAY *records, unsigned int recnum)
{
  MY_FOREIGN_KEY_FIELD *rec= NULL;
  if (recnum < records->elements)
  {
    rec= ((MY_FOREIGN_KEY_FIELD *)records->buffer) + recnum;
  }
  else
  {
    rec= (MY_FOREIGN_KEY_FIELD *) alloc_dynamic(records);
    if (!rec)
      return NULL;
    memset(rec, 0, sizeof(MY_FOREIGN_KEY_FIELD));
  }
  return rec;
}


/*
  If the foreign keys associated with a primary key are requested, the
  result set is ordered by FKTABLE_CAT, FKTABLE_NAME, KEY_SEQ, PKTABLE_NAME
*/
static int sql_fk_sort(const void *var1, const void *var2)
{
  int ret;
  if ((ret= strcmp(((MY_FOREIGN_KEY_FIELD *) var1)->FKTABLE_CAT,
               ((MY_FOREIGN_KEY_FIELD *) var2)->FKTABLE_CAT)) == 0)
  {
    if ((ret= strcmp(((MY_FOREIGN_KEY_FIELD *) var1)->FKTABLE_NAME,
                  ((MY_FOREIGN_KEY_FIELD *) var2)->FKTABLE_NAME)) == 0)
    {
      if ((ret= ((MY_FOREIGN_KEY_FIELD *) var1)->KEY_SEQ -
                  ((MY_FOREIGN_KEY_FIELD *) var2)->KEY_SEQ) == 0)
      {
        if ((ret= strcmp(((MY_FOREIGN_KEY_FIELD *) var1)->PKTABLE_NAME,
                  ((MY_FOREIGN_KEY_FIELD *) var2)->PKTABLE_NAME)) == 0)
        {
          return 0;
        }
      }
    }
  }
  return ret;
}


/*
  If the primary keys associated with a foreign key are requested, the
  result set is ordered by PKTABLE_CAT, PKTABLE_NAME, KEY_SEQ, FKTABLE_NAME
*/
static int sql_pk_sort(const void *var1, const void *var2)
{
  int ret;
  if ((ret= strcmp(((MY_FOREIGN_KEY_FIELD *) var1)->PKTABLE_CAT,
               ((MY_FOREIGN_KEY_FIELD *) var2)->PKTABLE_CAT)) == 0)
  {
    if ((ret= strcmp(((MY_FOREIGN_KEY_FIELD *) var1)->PKTABLE_NAME,
                  ((MY_FOREIGN_KEY_FIELD *) var2)->PKTABLE_NAME)) == 0)
    {
      if ((ret= ((MY_FOREIGN_KEY_FIELD *) var1)->KEY_SEQ -
                  ((MY_FOREIGN_KEY_FIELD *) var2)->KEY_SEQ) == 0)
      {
        if ((ret= strcmp(((MY_FOREIGN_KEY_FIELD *) var1)->FKTABLE_NAME,
                  ((MY_FOREIGN_KEY_FIELD *) var2)->FKTABLE_NAME)) == 0)
        {
          return 0;
        }
      }
    }
  }
  return ret;
}


SQLRETURN foreign_keys_no_i_s(SQLHSTMT hstmt,
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
                              SQLSMALLINT fk_table_name)
{
  STMT *stmt=(STMT *) hstmt;
  uint row_count= 0;

  MEM_ROOT  *alloc;
  MYSQL_ROW row, table_row;
  MYSQL_RES *local_res;
  /* We need this array for the cases if key count is greater than 18 */
  char      buffer[NAME_LEN + 1];
  unsigned int index= 0;
  DYNAMIC_ARRAY records;
  MY_FOREIGN_KEY_FIELD *fkRows= NULL;
  unsigned long *lengths;
  SQLRETURN rc= SQL_SUCCESS;
  std::string pk_db, fk_db;

  myodbc_init_dynamic_array(&records, sizeof(MY_FOREIGN_KEY_FIELD), 0, 0);

  /* Get the list of tables that match szCatalog and szTable */
  LOCK_STMT(stmt);

  try
  {
  pk_db = get_database_name(stmt, pk_catalog, pk_catalog_len,
                            pk_schema, pk_schema_len, false);
  fk_db = get_database_name(stmt, fk_catalog, fk_catalog_len,
                            fk_schema, fk_schema_len, false);

  local_res= table_status(stmt, (SQLCHAR*)pk_db.c_str(), pk_db.length(),
                    fk_table, fk_table_name, FALSE, TRUE, TRUE);
  if (!local_res && mysql_errno(stmt->dbc->mysql))
  {
    rc= handle_connection_error(stmt);
    throw ODBCEXCEPTION(EXCEPTION_TYPE::CONN_ERR);
  }
  else if (!local_res)
  {
    throw ODBCEXCEPTION(EXCEPTION_TYPE::EMPTY_SET);
  }
  free_internal_result_buffers(stmt);

  while ((table_row = mysql_fetch_row(local_res)))
  {
    lengths = mysql_fetch_lengths(local_res);
    if (stmt->result)
      mysql_free_result(stmt->result);
    stmt->result= server_show_create_table(stmt,
                                           (SQLCHAR*)pk_db.c_str(), pk_db.length(),
                                           (SQLCHAR*)table_row[0],
                                           (SQLSMALLINT)lengths[0]);

    if (!stmt->result)
    {
      if (mysql_errno(stmt->dbc->mysql))
      {
        rc= handle_connection_error(stmt);
        throw ODBCEXCEPTION(EXCEPTION_TYPE::CONN_ERR);
      }
      throw ODBCEXCEPTION(EXCEPTION_TYPE::EMPTY_SET);
    }

    /* Convert mysql fields to data that odbc wants */
    alloc= &stmt->alloc_root;

    while (row= mysql_fetch_row(stmt->result))
    {
      lengths= mysql_fetch_lengths(stmt->result);
      if (lengths[1])
      {
        const char Fk_keywords[2][12]= {"FOREIGN KEY", "REFERENCES"};
        const char Fk_ref_action[2][12]= {"ON UPDATE", "ON DELETE"};
        const char *pos, *end_pos, *bracket_end, *comma_pos;
        char       table_name[NAME_LEN+1], constraint_name[NAME_LEN+1];
        char       quote_char;
        unsigned int last_index= 0, quote_char_length= 1, key_seq, key_search;
        const char *end= row[1] + lengths[1];
        const char *token= row[1];

        quote_char= get_identifier_quote(stmt);
        while ((token= find_first_token(stmt->dbc->ansi_charset_info,
                              token, end, "CONSTRAINT")) != NULL)
        {
          pos= token;
          last_index= index;
          key_seq= 0;

          /* get constraint name */
          pos= my_next_token(NULL, &pos, NULL,
                 quote_char ? quote_char : ' ');
          end_pos= my_next_token(pos, &pos, constraint_name,
                     quote_char ? quote_char : ' ');
          token= end_pos;

          for (key_search= 0; key_search < 2; ++key_search)
          {
            /* get [FOREIGN KEY | REFERENCES] position */
            token= find_first_token(stmt->dbc->ansi_charset_info, token - 1,
                                      end, Fk_keywords[key_search]);
            /* When tables don't have FOREIGN KEY, don't search more */
            if(!token)
            {
              token = end_pos;
              break;
            }
            token += strlen(Fk_keywords[key_search]);
            token= skip_leading_spaces(token);
            *table_name= 0;

            /* if '(' not present get primary key table name */
            if (*token != '(')
            {
              pos= token;
              pos= my_next_token(NULL, &pos, NULL,
                     quote_char ? quote_char : ' ');
              end_pos= my_next_token(pos, &pos, table_name,
                         quote_char ? quote_char : ' ');
              token= end_pos;
            }

            token= skip_leading_spaces(token);
            /*
               get foreign key and primary column name
               in loop 1 and 2 respectively
            */
            if (*token == '(')
            {
              bracket_end= pos= token + 1;

              /* Get past opening quote */
              bracket_end= my_next_token(NULL, &bracket_end, NULL,
                                         quote_char ? quote_char : ' ');

              /* Get past closing quote */
              bracket_end= my_next_token(NULL, &bracket_end, NULL,
                                         quote_char ? quote_char : ' ');

              /* Only now it is safe to look for closing parenthese */
              bracket_end= my_next_token(NULL, &bracket_end, NULL, ')');
              /*
                index position need to be maintained for both PK column
                and FK column to fetch proper record
              */
              if (key_search == 0)
                last_index= index;
              else
                index= last_index;
              do
              {
                fkRows= fk_get_rec(&records, index);
                if (!fkRows)
                {
                  throw ODBCEXCEPTION(EXCEPTION_TYPE::EMPTY_SET);
                }

                comma_pos= pos;
                comma_pos= my_next_token(NULL, &comma_pos, NULL, ',');

                if (comma_pos > bracket_end || comma_pos == NULL)
                {
                  /*
                    TODO: make the length calculation more efficient and simple.
                          Add checking for negative values.
                  */
                  memcpy(buffer, pos + quote_char_length,
                           bracket_end - pos - quote_char_length * 2 - 1);
                  buffer[bracket_end - pos - quote_char_length * 2 - 1]= '\0';
                  if (key_search == 0)
                  {
                    myodbc_stpmov(fkRows->FKCOLUMN_NAME, buffer);
                  }
                  else
                  {
                    myodbc_stpmov(fkRows->PKCOLUMN_NAME, buffer);
                    myodbc_stpmov(fkRows->PKTABLE_NAME, table_name);
                    myodbc_stpmov(fkRows->FK_NAME, constraint_name);
                    myodbc_stpmov(fkRows->FKTABLE_NAME, row[0]);
                    myodbc_stpmov(fkRows->FKTABLE_CAT, (!fk_db.empty() ?
                            strdup_root(alloc, fk_db.c_str()) :
                            strdup_root(alloc, !stmt->dbc->database.empty() ?
                            stmt->dbc->database.c_str() : "null")));
                    myodbc_stpmov(fkRows->PKTABLE_CAT, (!pk_db.empty() ?
                            strdup_root(alloc, pk_db.c_str()) :
                            strdup_root(alloc, !stmt->dbc->database.empty() ?
                            stmt->dbc->database.c_str() : "null")));
                    /* key_seq incremented once for each PK column */
                    fkRows->KEY_SEQ= ++key_seq;
                  }
                  ++index;
                  break;
                }
                else
                {
                  memcpy(buffer, pos + quote_char_length,
                           comma_pos - pos - quote_char_length * 2 - 1);
                  buffer[comma_pos - pos - quote_char_length * 2 - 1]= '\0';
                  if (key_search == 0)
                  {
                    myodbc_stpmov(fkRows->FKCOLUMN_NAME, buffer);
                  }
                  else
                  {
                    myodbc_stpmov(fkRows->PKCOLUMN_NAME, buffer);
                    myodbc_stpmov(fkRows->PKTABLE_NAME, table_name);
                    myodbc_stpmov(fkRows->FK_NAME, constraint_name);
                    myodbc_stpmov(fkRows->FKTABLE_NAME, row[0]);
                    myodbc_stpmov(fkRows->FKTABLE_CAT, (!fk_db.empty() ?
                            strdup_root(alloc, fk_db.c_str()) :
                            strdup_root(alloc, !stmt->dbc->database.empty() ?
                            stmt->dbc->database.c_str() : "null")));
                    myodbc_stpmov(fkRows->PKTABLE_CAT, (!pk_db.empty() ?
                            strdup_root(alloc, pk_db.c_str()) :
                            strdup_root(alloc, !stmt->dbc->database.empty() ?
                            stmt->dbc->database.c_str() : "null")));
                    /* key_seq incremented once for each PK column */
                    fkRows->KEY_SEQ= ++key_seq;
                  }
                  pos= comma_pos + 1;
                  ++index;
                }
              } while (1);
              token= bracket_end + 1;
            }
          }

          /* OPTIONAL (UPDATE|DELETE) operations */
          for (key_search= 0; key_search < 2; ++key_search)
          {
            unsigned int curr_index;
            int action= SQL_NO_ACTION;
            bracket_end= comma_pos= pos= token;
            bracket_end= my_next_token(NULL, &bracket_end, NULL, ')');
            comma_pos= my_next_token(NULL, &comma_pos, NULL, ',');
            pos= find_first_token(stmt->dbc->ansi_charset_info, pos - 1,
              comma_pos ?
               ((comma_pos < bracket_end) ? comma_pos : bracket_end)
               : bracket_end,
              Fk_ref_action[key_search]);
            if (pos)
            {
              pos += strlen(Fk_ref_action[key_search]);
              pos= skip_leading_spaces(pos);
              if (*pos == 'R')  /* RESTRICT */
              {
                action= SQL_NO_ACTION;
              }
              else if (*pos == 'C')  /* CASCADE */
              {
                action= SQL_CASCADE;
              }
              else if (*pos == 'S')  /* SET NULL */
              {
                action= SQL_SET_NULL;
              }
              else if (*pos == 'N')  /* NO ACTION */
              {
                action= SQL_NO_ACTION;
              }
            }

            for (curr_index= last_index; curr_index < index; ++curr_index)
            {
              fkRows= fk_get_rec(&records, curr_index);
              if (!fkRows)
              {
                throw ODBCEXCEPTION(EXCEPTION_TYPE::EMPTY_SET);
              }

              if (key_search == 0)
                fkRows->UPDATE_RULE= action;
              else
                fkRows->DELETE_RULE= action;
            }
          }
        }
      }
    }
  }

  if (!records.elements)
  {
    throw ODBCEXCEPTION(EXCEPTION_TYPE::EMPTY_SET);
  }

  /*
    If the primary keys associated with a foreign key are requested, then
    sort order is PKTABLE_CAT, PKTABLE_NAME, KEY_SEQ, FKTABLE_NAME
    Sort order used same as present in no_i_s case, but it is different from
    http://msdn.microsoft.com/en-us/library/windows/desktop/ms709315(v=vs.85).aspx
  */
  if (pk_table && pk_table[0])
  {
    sort_dynamic(&records, sql_pk_sort);
  }

  /*
    if foreign keys associated with a primary key are requested
    then sort order is FKTABLE_CAT, FKTABLE_NAME, KEY_SEQ, PKTABLE_NAME
    Sort order used same as present in no_i_s case, but it is different from
    http://msdn.microsoft.com/en-us/library/windows/desktop/ms709315(v=vs.85).aspx
  */
  if (fk_table && fk_table[0])
  {
    sort_dynamic(&records, sql_fk_sort);
  }

  if (!stmt->m_row_storage.is_valid())
    x_free(stmt->result_array);

  // We will use the ROW_STORAGE here
  stmt->m_row_storage.set_size(records.elements, SQLFORE_KEYS_FIELDS);
  auto &data = stmt->m_row_storage;

  fkRows= (MY_FOREIGN_KEY_FIELD *) records.buffer;
  index= 0;
  while (index < records.elements)
  {
    if (pk_table && pk_table[0])
    {
      if (myodbc_strcasecmp((const char*)pk_table, (const char*)fkRows[index].PKTABLE_NAME))
      {
        ++index;
        continue;
      }
    }

    // PK Table Catalog & Schema
    CAT_SCHEMA_SET_FULL(stmt, data[0], data[1], fkRows[index].PKTABLE_CAT,
      pk_catalog, pk_schema, pk_catalog_len, pk_schema_len);

    /* PKTABLE_NAME */
    data[2] = fkRows[index].PKTABLE_NAME;
    /* PKCOLUMN_NAME */
    data[3]= fkRows[index].PKCOLUMN_NAME;

    // FK Table Catalog & Schema
    CAT_SCHEMA_SET_FULL(stmt, data[4], data[5], fkRows[index].FKTABLE_CAT,
      fk_catalog, fk_schema, fk_catalog_len, fk_schema_len);

    /* FKTABLE_NAME */
    data[6] = fkRows[index].FKTABLE_NAME;
     /* FKCOLUMN_NAME */
    data[7] = fkRows[index].FKCOLUMN_NAME;
    /* KEY_SEQ */
    data[8] = fkRows[index].KEY_SEQ;
    /* UPDATE_RULE */
    data[9] = fkRows[index].UPDATE_RULE;
    /* DELETE_RULE */
    data[10] = fkRows[index].DELETE_RULE;
    /* FK_NAME */
    data[11] = fkRows[index].FK_NAME;
    /* PK_NAME */
    data[12] = "PRIMARY";
    /* DEFERRABILITY */
    data[13]= "7"; /*SQL_NOT_DEFERRABLE*/

    data.next_row();
    ++row_count;
    ++index;
  }
  delete_dynamic(&records);
  mysql_free_result(local_res);

  stmt->result_array = (MYSQL_ROW)data.data();

  if (!stmt->result_array)
  {
    set_mem_error(stmt->dbc->mysql);
    return handle_connection_error(stmt);
  }

  set_row_count(stmt, row_count);
  myodbc_link_fields(stmt,SQLFORE_KEYS_fields,SQLFORE_KEYS_FIELDS);
  return SQL_SUCCESS;

  }
  catch(ODBCEXCEPTION &ex)
  {
    switch(ex.m_type)
    {
      case EXCEPTION_TYPE::EMPTY_SET:
      delete_dynamic(&records);
      mysql_free_result(local_res);
      free_internal_result_buffers(stmt);
      if (stmt->result)
      {
        mysql_free_result(stmt->result);
        stmt->result = nullptr;
      }
      return create_empty_fake_resultset(stmt, SQLFORE_KEYS_values,
                                         sizeof(SQLFORE_KEYS_values),
                                         SQLFORE_KEYS_fields,
                                         SQLFORE_KEYS_FIELDS);

    case EXCEPTION_TYPE::CONN_ERR:
      mysql_free_result(local_res);
      local_res = nullptr;
      delete_dynamic(&records);
      free_internal_result_buffers(stmt);
      if (stmt->result)
      {
        mysql_free_result(stmt->result);
        stmt->result = nullptr;
      }
    }
  }

  if (local_res)
    mysql_free_result(local_res);

  return rc;
}


/*
****************************************************************************
SQLPrimaryKeys
****************************************************************************
*/

MYSQL_FIELD SQLPRIM_KEYS_fields[]=
{
  MYODBC_FIELD_NAME("TABLE_CAT", 0),
  MYODBC_FIELD_NAME("TABLE_SCHEM", 0),
  MYODBC_FIELD_NAME("TABLE_NAME", NOT_NULL_FLAG),
  MYODBC_FIELD_NAME("COLUMN_NAME", NOT_NULL_FLAG),
  MYODBC_FIELD_SHORT("KEY_SEQ", NOT_NULL_FLAG),
  MYODBC_FIELD_STRING("PK_NAME", 128, 0),
};

const uint SQLPRIM_KEYS_FIELDS= array_elements(SQLPRIM_KEYS_fields);

const long SQLPRIM_LENGTHS[]= {0, 0, 1, 5, 4, -7};

char *SQLPRIM_KEYS_values[]= {
    NULL,"",NULL,NULL,0,NULL
};

/*
  @purpose : returns the column names that make up the primary key for a table.
       The driver returns the information as a result set. This function
       does not support returning primary keys from multiple tables in
       a single call
*/

SQLRETURN
primary_keys_no_i_s(SQLHSTMT hstmt,
                    SQLCHAR *catalog, SQLSMALLINT catalog_len,
                    SQLCHAR *schema, SQLSMALLINT schema_len,
                    SQLCHAR *table, SQLSMALLINT table_len)
{
    STMT *stmt= (STMT *) hstmt;
    MYSQL_ROW row;
    uint      row_count;

    LOCK_STMT(stmt);

    auto db = get_database_name(stmt, catalog, catalog_len,
                                schema, schema_len);
    if (!(stmt->result= server_list_dbkeys(stmt, (SQLCHAR*)db.c_str(), db.length(),
                                           table, table_len)))
    {
      SQLRETURN rc= handle_connection_error(stmt);
      return rc;
    }

    /*
    x_free(stmt->result_array);
    stmt->result_array= (char**) myodbc_malloc(sizeof(char*)*SQLPRIM_KEYS_FIELDS*
                                            (ulong) stmt->result->row_count,
                                            MYF(MY_ZEROFILL));
    if (!stmt->result_array)
    {
      set_mem_error(stmt->dbc->mysql);
      return handle_connection_error(stmt);
    }
    */

    if (!stmt->m_row_storage.is_valid())
      x_free(stmt->result_array);

    // We will use the ROW_STORAGE here
    stmt->m_row_storage.set_size(stmt->result->row_count,
                                 SQLPRIM_KEYS_FIELDS);
    auto &data = stmt->m_row_storage;

    stmt->alloc_lengths(SQLPRIM_KEYS_FIELDS*(ulong) stmt->result->row_count);

    if (!stmt->lengths)
    {
      set_mem_error(stmt->dbc->mysql);
      return handle_connection_error(stmt);
    }

    row_count= 0;
    while ( (row= mysql_fetch_row(stmt->result)) )
    {
        if ( row[1][0] == '0' )     /* If unique index */
        {
            if ( row_count && !strcmp(row[3],"1") )
                break;    /* Already found unique key */

            fix_row_lengths(stmt, SQLPRIM_LENGTHS, row_count, SQLPRIM_KEYS_FIELDS);

            ++row_count;
            /* TABLE_CAT and TABLE_SCHEMA */
            CAT_SCHEMA_SET(data[0], data[1], db);
            /* TABLE_NAME */
            data[2] = row[0];

            /* COLUMN_NAME */
            data[3] = row[4];

            /* KEY_SEQ  */
            data[4] = row[3];

            /* PK_NAME */
            data[5] = "PRIMARY";

            data.next_row();
        }
    }

    stmt->result_array = (MYSQL_ROW)data.data();
    set_row_count(stmt, row_count);
    myodbc_link_fields(stmt,SQLPRIM_KEYS_fields,SQLPRIM_KEYS_FIELDS);

    return SQL_SUCCESS;
}


/*
****************************************************************************
SQLProcedure Columns
****************************************************************************
*/

char *SQLPROCEDURECOLUMNS_values[]= {
       "", "", NullS, NullS, "", "", "",
       "", "", "", "10", "",
       "MySQL column", "", "", NullS, "",
       NullS, ""
};

/* TODO make LONGLONG fields just LONG if SQLLEN is 4 bytes */
MYSQL_FIELD SQLPROCEDURECOLUMNS_fields[]=
{
  MYODBC_FIELD_NAME("PROCEDURE_CAT",     0),
  MYODBC_FIELD_NAME("PROCEDURE_SCHEM",   0),
  MYODBC_FIELD_NAME("PROCEDURE_NAME",    NOT_NULL_FLAG),
  MYODBC_FIELD_NAME("COLUMN_NAME",       NOT_NULL_FLAG),
  MYODBC_FIELD_SHORT ("COLUMN_TYPE",       NOT_NULL_FLAG),
  MYODBC_FIELD_SHORT ("DATA_TYPE",         NOT_NULL_FLAG),
  MYODBC_FIELD_STRING("TYPE_NAME",         20,       NOT_NULL_FLAG),
  MYODBC_FIELD_LONGLONG("COLUMN_SIZE",       0),
  MYODBC_FIELD_LONGLONG("BUFFER_LENGTH",     0),
  MYODBC_FIELD_SHORT ("DECIMAL_DIGITS",    0),
  MYODBC_FIELD_SHORT ("NUM_PREC_RADIX",    0),
  MYODBC_FIELD_SHORT ("NULLABLE",          NOT_NULL_FLAG),
  MYODBC_FIELD_NAME("REMARKS",           0),
  MYODBC_FIELD_NAME("COLUMN_DEF",        0),
  MYODBC_FIELD_SHORT ("SQL_DATA_TYPE",     NOT_NULL_FLAG),
  MYODBC_FIELD_SHORT ("SQL_DATETIME_SUB",  0),
  MYODBC_FIELD_LONGLONG("CHAR_OCTET_LENGTH", 0),
  MYODBC_FIELD_LONG  ("ORDINAL_POSITION",  NOT_NULL_FLAG),
  MYODBC_FIELD_STRING("IS_NULLABLE",       3,        0),
};

const uint SQLPROCEDURECOLUMNS_FIELDS=
             array_elements(SQLPROCEDURECOLUMNS_fields);


/*
  @type    : internal
  @purpose : returns procedure params as resultset
*/
static MYSQL_RES *server_list_proc_params(STMT *stmt,
                                          SQLCHAR *catalog,
                                          SQLSMALLINT catalog_len,
                                          SQLCHAR *proc_name,
                                          SQLSMALLINT proc_name_len,
                                          SQLCHAR *par_name,
                                          SQLSMALLINT par_name_len)
{
  DBC   *dbc = stmt->dbc;
  MYSQL *mysql= dbc->mysql;
  char   tmpbuf[1024];
  std::string qbuff;
  qbuff.reserve(2048);

  auto append_escaped_string = [&mysql, &tmpbuf](std::string &outstr,
                                        SQLCHAR* str,
                                        SQLSMALLINT len)
  {
    tmpbuf[0] = '\0';
    outstr.append("'");
    mysql_real_escape_string(mysql, tmpbuf, (char *)str, len);
    outstr.append(tmpbuf).append("'");
  };


  if((is_minimum_version(dbc->mysql->server_version, "5.7")))
  {
    qbuff = "select SPECIFIC_NAME, (IF(ISNULL(PARAMETER_NAME), "
            "concat('OUT RETURN_VALUE ', DTD_IDENTIFIER), "
            "concat(PARAMETER_MODE, ' `', PARAMETER_NAME, '` ', DTD_IDENTIFIER)) "
            ") as PARAMS_LIST, SPECIFIC_SCHEMA, ROUTINE_TYPE, ORDINAL_POSITION "
            "FROM information_schema.parameters "
            "WHERE SPECIFIC_SCHEMA LIKE ";

    if (catalog_len)
      append_escaped_string(qbuff, catalog, catalog_len);
    else
      qbuff.append("DATABASE()");

    if (proc_name_len)
    {
      qbuff.append(" AND SPECIFIC_NAME LIKE ");
      append_escaped_string(qbuff, proc_name, proc_name_len);
    }

    if (par_name_len)
    {
      qbuff.append(" AND (PARAMETER_NAME LIKE ");
      append_escaped_string(qbuff, par_name, par_name_len);
      qbuff.append(" OR ISNULL(PARAMETER_NAME))");
    }

    qbuff.append(" ORDER BY SPECIFIC_SCHEMA, SPECIFIC_NAME, ORDINAL_POSITION ASC");
  }
  else
  {
    qbuff = "SELECT name, CONCAT(IF(length(returns)>0, "
            "CONCAT('RETURN_VALUE ', returns, if(length(param_list)>0, ',', '')),''), param_list),"
            "db, type FROM mysql.proc WHERE Db=";

    if (catalog_len)
      append_escaped_string(qbuff, catalog, catalog_len);
    else
      qbuff.append("DATABASE()");

    if (proc_name_len)
    {
      qbuff.append(" AND name LIKE ");
      append_escaped_string(qbuff, proc_name, proc_name_len);
    }

    qbuff.append(" ORDER BY Db, name");
  }

  MYLOG_DBC_QUERY(dbc, qbuff.c_str());
  if (exec_stmt_query(stmt, qbuff.c_str(), qbuff.length(), FALSE))
    return NULL;

  return mysql_store_result(mysql);
}


/*
  @type    : ODBC 1.0 API
  @purpose : returns the list of input and output parameters, as well as
  the columns that make up the result set for the specified
  procedures. The driver returns the information as a result
  set on the specified statement
*/
SQLRETURN
procedure_columns_no_i_s(SQLHSTMT hstmt,
                         SQLCHAR *catalog, SQLSMALLINT catalog_len,
                         SQLCHAR *schema, SQLSMALLINT schema_len,
                         SQLCHAR *proc, SQLSMALLINT proc_len,
                         SQLCHAR *column, SQLSMALLINT column_len)
{
  STMT *stmt= (STMT *)hstmt;
  SQLRETURN nReturn= SQL_SUCCESS;
  MYSQL_ROW row;
  MYSQL_RES *proc_list_res;
  int params_num= 0, return_params_num= 0;
  unsigned int i, j, total_params_num= 0;
  std::string db;

  /* get procedures list */
  LOCK_STMT(stmt);
  try
  {
    db = get_database_name(stmt, catalog, catalog_len,
                            schema, schema_len, false);

  if (!(proc_list_res= server_list_proc_params(stmt,
      (SQLCHAR*)db.c_str(), db.length(), proc, proc_len,
      column, column_len)))
  {
    nReturn= stmt->set_error(MYERR_S1000);
    throw ODBCEXCEPTION();
  }

  if (!stmt->m_row_storage.is_valid())
    x_free(stmt->result_array);

  // We will use the ROW_STORAGE here
  stmt->m_row_storage.set_size(proc_list_res->row_count,
                               SQLPROCEDURECOLUMNS_FIELDS);
  auto &data = stmt->m_row_storage;

  while ((row= mysql_fetch_row(proc_list_res)))
  {
    char *token;
    char *param_str;
    char *param_str_end;

    param_str= row[1];
    if(!param_str[0])
      continue;

    param_str_end= param_str + strlen(param_str);

    token = proc_param_tokenize(param_str, &params_num);

    if (params_num == 0)
    {
      throw ODBCEXCEPTION(EXCEPTION_TYPE::EMPTY_SET);
    }

    while (token != NULL)
    {
      SQLSMALLINT  ptype= 0;
      int sql_type_index;
      unsigned int flags= 0;
      SQLCHAR param_name[NAME_LEN]= "\0";
      SQLCHAR param_dbtype[1024]= "\0";
      SQLCHAR param_type[4]= "\0";
      SQLCHAR param_sql_type[6]= "\0";
      SQLCHAR param_size_buf[21]= "\0";
      SQLCHAR param_buffer_len[21]= "\0";
      SQLCHAR param_decimal[6]= "\0";
      SQLCHAR param_desc_type[6]= "\0";
      SQLCHAR param_pos[6]= "\0";

      SQLTypeMap *type_map;
      SQLSMALLINT dec;
      SQLULEN param_size= 0;
      /* temp variables for debugging */
      SQLUINTEGER dec_int= 0;
      SQLINTEGER sql_type_int= 0;

      token= proc_get_param_type(token, (int)strlen(token), &ptype);
      token= proc_get_param_name(token, (int)strlen(token), (char*)param_name);
      token= proc_get_param_dbtype(token, (int)strlen(token), (char*)param_dbtype);

      /* param_dbtype is lowercased in the proc_get_param_dbtype */
      if (strstr((const char*)param_dbtype, "unsigned"))
        flags |= UNSIGNED_FLAG;

      sql_type_index= proc_get_param_sql_type_index((const char*)param_dbtype, (int)strlen((const char*)param_dbtype));
      type_map= proc_get_param_map_by_index(sql_type_index);

      param_size= proc_get_param_size(param_dbtype, (int)strlen((const char*)param_dbtype), sql_type_index, &dec);

      proc_get_param_octet_len(stmt, sql_type_index, param_size, dec, flags, (char*)param_buffer_len);

      /* PROCEDURE_CAT and PROCEDURE_SCHEMA */
      CAT_SCHEMA_SET(data[mypcPROCEDURE_CAT], data[mypcPROCEDURE_SCHEM], row[2]);

      data[mypcPROCEDURE_NAME] = row[0];
      data[mypcCOLUMN_NAME] = (const char*)param_name;

      /* The ordinal position can start with "0" only for return values */
      if (row[4][0] == '0')
      {
        ptype= SQL_RETURN_VALUE;
      }

      data[mypcCOLUMN_TYPE] = ptype;

      if (!myodbc_strcasecmp((const char*)type_map->type_name, "bit") && param_size > 1)
        data[mypcDATA_TYPE] = SQL_BINARY;
      else
        data[mypcDATA_TYPE] = type_map->sql_type;

      if (!myodbc_strcasecmp((const char*)type_map->type_name, "set") ||
         !myodbc_strcasecmp((const char*)type_map->type_name, "enum"))
      {
        data[mypcTYPE_NAME] = "char";
      }
      else
      {
        data[mypcTYPE_NAME]= (const char*)type_map->type_name;
      }

      proc_get_param_col_len(stmt, sql_type_index, param_size, dec, flags, (char*)param_size_buf);
      data[mypcCOLUMN_SIZE] = (const char*)param_size_buf;
      data[mypcBUFFER_LENGTH] = (const char*)param_buffer_len;

      if (dec != SQL_NO_TOTAL)
      {
        data[mypcDECIMAL_DIGITS] = dec;
        data[mypcNUM_PREC_RADIX] = "10";
      }
      else
      {
        data[mypcDECIMAL_DIGITS] = nullptr;
        data[mypcNUM_PREC_RADIX] = nullptr;
      }
      data[mypcNULLABLE] = "1";  /* NULLABLE */
      data[mypcREMARKS] = "";   /* REMARKS */
      data[mypcCOLUMN_DEF] = nullptr; /* COLUMN_DEF */

      if(type_map->sql_type == SQL_TYPE_DATE ||
         type_map->sql_type == SQL_TYPE_TIME ||
         type_map->sql_type == SQL_TYPE_TIMESTAMP)
      {
        data[mypcSQL_DATA_TYPE] = SQL_DATETIME;
        data[mypcSQL_DATETIME_SUB] = data[mypcDATA_TYPE];
      }
      else
      {
        data[mypcSQL_DATA_TYPE] = data[mypcDATA_TYPE];
        data[mypcSQL_DATETIME_SUB] = nullptr;
      }

      if (is_char_sql_type(type_map->sql_type) || is_wchar_sql_type(type_map->sql_type) ||
          is_binary_sql_type(type_map->sql_type))
      {
        data[mypcCHAR_OCTET_LENGTH] = data[mypcBUFFER_LENGTH];
      }
      else
      {
        data[mypcCHAR_OCTET_LENGTH] = nullptr;
      }

      data[mypcORDINAL_POSITION] = row[4];

      data[mypcIS_NULLABLE] = "YES"; /* IS_NULLABLE */
      ++total_params_num;
      data.next_row();
      token = proc_param_next_token(token, param_str_end);
    }
  }

  stmt->result_array = data.is_valid() ? (MYSQL_ROW)data.data() : nullptr;
  return_params_num = total_params_num;

  stmt->result= proc_list_res;

  set_row_count(stmt, return_params_num);

  myodbc_link_fields(stmt, SQLPROCEDURECOLUMNS_fields, SQLPROCEDURECOLUMNS_FIELDS);

  }
  catch(ODBCEXCEPTION &ex)
  {
    switch(ex.m_type)
    {
      case EXCEPTION_TYPE::EMPTY_SET:
        nReturn = create_empty_fake_resultset(
            (STMT*)hstmt, SQLPROCEDURECOLUMNS_values,
            sizeof(SQLPROCEDURECOLUMNS_values),
            SQLPROCEDURECOLUMNS_fields,
            SQLPROCEDURECOLUMNS_FIELDS);
        free_internal_result_buffers(stmt);
        mysql_free_result(proc_list_res);
      case EXCEPTION_TYPE::GENERAL:
        break;
    }
  }

  return nReturn;
}


/*
****************************************************************************
SQLSpecialColumns
****************************************************************************
*/

MYSQL_FIELD SQLSPECIALCOLUMNS_fields[]=
{
  MYODBC_FIELD_SHORT("SCOPE", 0),
  MYODBC_FIELD_NAME("COLUMN_NAME", NOT_NULL_FLAG),
  MYODBC_FIELD_SHORT("DATA_TYPE", NOT_NULL_FLAG),
  MYODBC_FIELD_STRING("TYPE_NAME", 20, NOT_NULL_FLAG),
  MYODBC_FIELD_LONG("COLUMN_SIZE", 0),
  MYODBC_FIELD_LONG("BUFFER_LENGTH", 0),
  MYODBC_FIELD_LONG("DECIMAL_DIGITS", 0),
  MYODBC_FIELD_SHORT("PSEUDO_COLUMN", 0),
};

char *SQLSPECIALCOLUMNS_values[]= {
    0,NULL,0,NULL,0,0,0,0
};

const uint SQLSPECIALCOLUMNS_FIELDS= array_elements(SQLSPECIALCOLUMNS_fields);


/*
  @type    : ODBC 1.0 API
  @purpose : retrieves the following information about columns within a
       specified table:
       - The optimal set of columns that uniquely identifies a row
         in the table.
       - Columns that are automatically updated when any value in the
         row is updated by a transaction
*/

SQLRETURN
special_columns_no_i_s(SQLHSTMT hstmt, SQLUSMALLINT fColType,
                       SQLCHAR *catalog, SQLSMALLINT catalog_len,
                       SQLCHAR *schema, SQLSMALLINT schema_len,
                       SQLCHAR *szTableName, SQLSMALLINT cbTableName,
                       SQLUSMALLINT fScope __attribute__((unused)),
                       SQLUSMALLINT fNullable __attribute__((unused)))
{
    STMT        *stmt=(STMT *) hstmt;
    char        buff[80];
    MYSQL_FIELD *field;
    MYSQL_RES   *result;
    my_bool     primary_key;
    std::string db;

    /* Reset the statement in order to avoid memory leaks when working with ADODB */
    my_SQLFreeStmt(hstmt, MYSQL_RESET);

    db = get_database_name(stmt, catalog, catalog_len, schema, schema_len,
                           false);

    stmt->result= server_list_dbcolumns(stmt, (SQLCHAR*)db.c_str(), db.length(),
                                        szTableName, cbTableName, NULL, 0);
    if (!(result= stmt->result))
    {
      return handle_connection_error(stmt);
    }

    if (!stmt->m_row_storage.is_valid())
      x_free(stmt->result_array);

    // We will use the ROW_STORAGE here
    stmt->m_row_storage.set_size(result->field_count,
                                  SQLSPECIALCOLUMNS_FIELDS);
    auto &data = stmt->m_row_storage;

    ////////////////////////////////////////////////////////////////////////
    auto lambda_fill_data = [&result, &field, &data, &stmt, &buff, &primary_key]
                            (SQLSMALLINT colType)
    {
      uint f_count = 0;
      mysql_field_seek(result,0);
      while(field = mysql_fetch_field(result))
      {
        if(colType == SQL_ROWVER)
        {
          if ((field->type != MYSQL_TYPE_TIMESTAMP))
            continue;
          if (!(field->flags & ON_UPDATE_NOW_FLAG))
            continue;
          /* SCOPE */
          data[0] = nullptr;
        }
        else
        {
          if ( primary_key && !(field->flags & PRI_KEY_FLAG) )
              continue;
  #ifndef SQLSPECIALCOLUMNS_RETURN_ALL_COLUMNS
          /* The ODBC 'standard' doesn't want us to return all columns if there is
             no primary or unique key */
          if ( !primary_key )
              continue;
  #endif
          /* SCOPE */
          data[0] = SQL_SCOPE_SESSION;
        }

        /* COLUMN_NAME */
        data[1] = field->name;

        /* DATA_TYPE */
        data[2] = get_sql_data_type(stmt, field, buff);;

        /* TYPE_NAME */
        data[3] = buff;

        /* COLUMN_SIZE */
        fill_column_size_buff(buff, stmt, field);
        data[4] = buff;

        /* BUFFER_LENGTH */
        data[5] = get_transfer_octet_length(stmt, field);
        {
          SQLSMALLINT digits= get_decimal_digits(stmt, field);
          /* DECIMAL_DIGITS */
          if (digits != SQL_NO_TOTAL)
            data[6] = digits;
          else
            data[6] = nullptr;
        }
        data[7] = SQL_PC_NOT_PSEUDO;
        ++f_count;

        data.next_row();
      }

      stmt->result_array = (MYSQL_ROW)data.data();
      result->row_count= f_count;
      myodbc_link_fields(stmt,SQLSPECIALCOLUMNS_fields,SQLSPECIALCOLUMNS_FIELDS);

      return f_count;
    };
    ////////////////////////////////////////////////////////////////////////

    if ( fColType == SQL_ROWVER )
    {
        /* Convert mysql fields to data that odbc wants */
        lambda_fill_data(fColType);
        return SQL_SUCCESS;
    }

    if ( fColType != SQL_BEST_ROWID )
    {
        return stmt->set_error( MYERR_S1000,
                         "Unsupported argument to SQLSpecialColumns", 4000);
    }

    /*
     * The optimal set of columns for identifing a row is either
     * the primary key, or if there is no primary key, then
     * all the fields.
     */

    /* Check if there is a primary (unique) key */
    primary_key= 0;
    while ( (field= mysql_fetch_field(result)) )
    {
        if ( field->flags & PRI_KEY_FLAG )
        {
            primary_key=1;
            break;
        }
    }
    lambda_fill_data(!SQL_ROWVER);
    return SQL_SUCCESS;
}


/*
****************************************************************************
SQLStatistics
****************************************************************************
*/

char SS_type[10];
uint SQLSTAT_order[]={2,3,5,7,8,9,10};
char *SQLSTAT_values[]={NullS,NullS,"","",NullS,"",SS_type,"","","","",NullS,NullS};

MYSQL_FIELD SQLSTAT_fields[]=
{
  MYODBC_FIELD_NAME("TABLE_CAT", 0),
  MYODBC_FIELD_NAME("TABLE_SCHEM", 0),
  MYODBC_FIELD_NAME("TABLE_NAME", NOT_NULL_FLAG),
  MYODBC_FIELD_SHORT("NON_UNIQUE", 0),
  MYODBC_FIELD_NAME("INDEX_QUALIFIER", 0),
  MYODBC_FIELD_NAME("INDEX_NAME", 0),
  MYODBC_FIELD_SHORT("TYPE", NOT_NULL_FLAG),
  MYODBC_FIELD_SHORT("ORDINAL_POSITION", 0),
  MYODBC_FIELD_NAME("COLUMN_NAME", 0),
  MYODBC_FIELD_STRING("ASC_OR_DESC", 1, 0),
  MYODBC_FIELD_SHORT("CARDINALITY", 0),
  MYODBC_FIELD_SHORT("PAGES", 0),
  MYODBC_FIELD_STRING("FILTER_CONDITION", 10, 0),
};

const uint SQLSTAT_FIELDS= array_elements(SQLSTAT_fields);

/*
  @purpose : retrieves a list of statistics about a single table and the
       indexes associated with the table. The driver returns the
       information as a result set.
*/

SQLRETURN
statistics_no_i_s(SQLHSTMT hstmt,
                  SQLCHAR *catalog, SQLSMALLINT catalog_len,
                  SQLCHAR *schema, SQLSMALLINT schema_len,
                  SQLCHAR *table, SQLSMALLINT table_len,
                  SQLUSMALLINT fUnique,
                  SQLUSMALLINT fAccuracy)
{
    STMT *stmt= (STMT *)hstmt;
    MYSQL *mysql= stmt->dbc->mysql;
    DBC *dbc= stmt->dbc;
    char *db_val = nullptr;
    std::string db;

    LOCK_STMT(stmt);

    if (!table_len)
        goto empty_set;

    db = get_database_name(stmt, catalog, catalog_len, schema, schema_len, false);

    stmt->result= server_list_dbkeys(stmt, (SQLCHAR*)db.c_str(), db.length(),
                                     table, table_len);
    if (!stmt->result)
    {
      SQLRETURN rc= handle_connection_error(stmt);
      return rc;
    }
    my_int2str(SQL_INDEX_OTHER,SS_type,10,0);
    stmt->order=       SQLSTAT_order;
    stmt->order_count= array_elements(SQLSTAT_order);
    stmt->fix_fields=  fix_fields_copy;
    stmt->array= (MYSQL_ROW) myodbc_memdup((char *)SQLSTAT_values,
                                       sizeof(SQLSTAT_values),MYF(0));
    if (!stmt->array)
    {
      set_mem_error(stmt->dbc->mysql);
      return handle_connection_error(stmt);
    }

    db_val = strmake_root(&stmt->alloc_root, db.c_str(), db.length());
    CAT_SCHEMA_SET(stmt->array[0], stmt->array[1], db_val);

    if ( fUnique == SQL_INDEX_UNIQUE )
    {
        /* This is too low level... */
        MYSQL_ROWS **prev,*pos;
        prev= &stmt->result->data->data;
        for ( pos= *prev; pos; pos= pos->next )
        {
            if ( pos->data[1][0] == '0' ) /* Unlink nonunique index */
            {
                (*prev)=pos;
                prev= &pos->next;
            }
            else
            {
                --stmt->result->row_count;
            }
        }
        (*prev)= 0;
        mysql_data_seek(stmt->result,0);  /* Restore pointer */
    }

    set_row_count(stmt, stmt->result->row_count);
    myodbc_link_fields(stmt,SQLSTAT_fields,SQLSTAT_FIELDS);
    return SQL_SUCCESS;

empty_set:
  return create_empty_fake_resultset(stmt, SQLSTAT_values,
                                     sizeof(SQLSTAT_values),
                                     SQLSTAT_fields, SQLSTAT_FIELDS);
}


/*
****************************************************************************
SQLTables
****************************************************************************
*/

uint SQLTABLES_qualifier_order[]= {0};
char *SQLTABLES_values[]= {"","",NULL,"TABLE","MySQL table"};
char *SQLTABLES_qualifier_values[]= {"",NULL,NULL,NULL,NULL};
char *SQLTABLES_owner_values[]= {NULL,"",NULL,NULL,NULL};
char *SQLTABLES_type_values[3][5]=
{
    {NULL,NULL,NULL,"TABLE",NULL},
    {NULL,NULL,NULL,"SYSTEM TABLE",NULL},
    {NULL,NULL,NULL,"VIEW",NULL},
};

MYSQL_FIELD SQLTABLES_fields[]=
{
  MYODBC_FIELD_NAME("TABLE_CAT",   0),
  MYODBC_FIELD_NAME("TABLE_SCHEM", 0),
  MYODBC_FIELD_NAME("TABLE_NAME",  0),
  MYODBC_FIELD_NAME("TABLE_TYPE",  0),
/*
  Table remark length is 80 characters
*/
  MYODBC_FIELD_STRING("REMARKS",     80, 0),
};


const uint SQLTABLES_FIELDS= array_elements(SQLTABLES_values);

SQLRETURN
tables_no_i_s(SQLHSTMT hstmt,
              SQLCHAR *catalog, SQLSMALLINT catalog_len,
              SQLCHAR *schema, SQLSMALLINT schema_len,
              SQLCHAR *table, SQLSMALLINT table_len,
              SQLCHAR *type, SQLSMALLINT type_len)
{
    STMT *stmt= (STMT *)hstmt;
    my_bool user_tables, views;

    my_ulonglong row_count= 0;
    MYSQL_ROW db_row = nullptr;
    unsigned long count= 0;
    bool is_info_schema= 0;
    SQLRETURN rc = SQL_SUCCESS;

    try
    {
      ODBC_RESULTSET db_res;
      LOCK_STMT(stmt);
      stmt->result = nullptr;
      is_info_schema = server_has_i_s(stmt->dbc) &&
                       !stmt->dbc->ds->no_information_schema;

      bool all_dbs =
          ((catalog_len && !schema_len) ||
           (schema_len && !catalog_len)) &&
           !table_len && table && !type_len;

      if (// Empty catalog, empty schema, empty table, type is "%"
                   !catalog_len && catalog && !schema_len && schema &&
                   !table_len && table && type && !strncmp((char *)type, "%", 2))
      {
        /* Return set of TableType qualifiers */
        rc = create_fake_resultset(stmt, (MYSQL_ROW)SQLTABLES_type_values,
                                    sizeof(SQLTABLES_type_values),
                                    sizeof(SQLTABLES_type_values) /
                                    sizeof(SQLTABLES_type_values[0]),
                                    SQLTABLES_fields, SQLTABLES_FIELDS,
                                    true);
        return rc;
      }

      /*
        Get all databases if NO_I_S
        OR
        Empty (but non-NULL) schema and table returns catalog list.
        Empty (but non-NULL) catalog and table returns schema list.
      */
      if (!is_info_schema || all_dbs)
      {
        /* Determine the database */
        std::string db = get_database_name(stmt, catalog, catalog_len,
                                           schema, schema_len, false);
        db_res = db_status(stmt, db);
        if (!db_res)
        {
          return handle_connection_error(stmt);
        }

      }
      else if (!catalog_len && catalog && // Empty catalog, schema and table
               !schema_len && schema &&
               !table_len && table &&
               !type_len && type)
      {
        rc = create_fake_resultset(stmt, SQLTABLES_owner_values,
                                   sizeof(SQLTABLES_owner_values),
                                   1, SQLTABLES_fields, SQLTABLES_FIELDS,
                                   true);
        return rc;
      }

      /* any other use of catalog="" or schema="" returns an empty result */
      if ((catalog || schema) && !catalog_len && !schema)
      {
        throw ODBCEXCEPTION(EXCEPTION_TYPE::EMPTY_SET);
      }

      user_tables = check_table_type(type, "TABLE", 5);
      views = check_table_type(type, "VIEW", 4);

      /* If no types specified, we want tables and views. */
      if (!user_tables && !views)
      {
        if (!type_len)
          user_tables = views = 1;
      }

      /* Return empty set if unknown TableType */
      if (type_len && !views && !user_tables)
      {
        throw ODBCEXCEPTION(EXCEPTION_TYPE::EMPTY_SET);
      }

      /* Free if result data was not in row storage */
      if (!stmt->m_row_storage.is_valid())
        x_free(stmt->result_array);

      auto &data = stmt->m_row_storage;

      /*
        If database result set (catalog_res) was produced loop
        through all database to fetch table list inside database
      */
      while (is_info_schema ||
             (db_res && (db_row = mysql_fetch_row(db_res))))
      {

        if ((is_info_schema || all_dbs) && db_res)
          stmt->result = db_res.release();
        else
        {
          /*
            Determine the database either from the result (NO_I_S)
            or from parameters (I_S)
          */
          std::string db = db_row ? db_row[3] :
                                    get_database_name(stmt, catalog, catalog_len,
                                                      schema, schema_len, false);
          stmt->result = table_status(stmt, (SQLCHAR*)db.c_str(), db.length(),
                                      table, table_len, TRUE,
                                      user_tables, views);
        }

        if (!stmt->result && mysql_errno(stmt->dbc->mysql))
        {
          /* unknown DB will return empty set from SQLTables */
          switch (mysql_errno(stmt->dbc->mysql))
          {
          case ER_BAD_DB_ERROR:
            throw ODBCEXCEPTION(EXCEPTION_TYPE::EMPTY_SET);
          default:
            return handle_connection_error(stmt);
          }
        }

        if (!stmt->result)
          throw ODBCEXCEPTION(EXCEPTION_TYPE::EMPTY_SET);

        /* assemble final result set */
        {
          MYSQL_ROW    row;
          std::string  db= "";
          row_count += stmt->result->row_count;

          if (!stmt->result->row_count)
          {
            free_internal_result_buffers(stmt);
            if (stmt->result)
            {
              mysql_free_result(stmt->result);
              stmt->result = nullptr;
            }

            if (is_info_schema)
              break;
            else
              continue;
          }

          // We will use the ROW_STORAGE here
          stmt->m_row_storage.set_size(row_count, SQLTABLES_FIELDS);

          int name_index = 0;
          while ((row= mysql_fetch_row(stmt->result)))
          {
            int type_index = 2;
            int comment_index = 1;
            int cat_index= 3;
            my_bool view;

            /* If if did not use I_S */
            if (!is_info_schema)
            {
              /* If the result is not databases only */
              if (stmt->result->field_count > 5)
                type_index= comment_index= (stmt->result->field_count == 18) ? 17 : 15;

              db = db_row[cat_index] ? std::string(db_row[cat_index]) : "";
            }
            else
              db = row[cat_index];

            view= (myodbc_casecmp(row[type_index], "VIEW", 4) == 0);

            if ((view && !views) || (!view && !user_tables))
            {
              --row_count;
              continue;
            }

            // Table Catalog & Schema
            CAT_SCHEMA_SET(data[0], data[1], db);

            // Table Name
            data[2] = row[name_index];

            // Table Type
            data[3] = row[type_index];

            // Comment
            data[4] = row[comment_index];
            data.next_row();
          }

        }
        /*
          If i_s then loop only once to fetch database name,
          as mysql_table_status_i_s handles SQL_ALL_CATALOGS (%)
          functionality and all databases with tables are returned
          in one result set and so no further loops are required.
        */
        if (is_info_schema)
          break;
      }

      stmt->result_array = (MYSQL_ROW)data.data();

      if (!row_count)
      {
        throw ODBCEXCEPTION(EXCEPTION_TYPE::EMPTY_SET);
      }
    }
    catch(ODBCEXCEPTION &ex)
    {
      switch(ex.m_type)
      {
        case EXCEPTION_TYPE::EMPTY_SET:
          return create_empty_fake_resultset(stmt, SQLTABLES_values,
                                             sizeof(SQLTABLES_values),
                                             SQLTABLES_fields,
                                             SQLTABLES_FIELDS);
      }
    }

  set_row_count(stmt, row_count);

  myodbc_link_fields(stmt, SQLTABLES_fields, SQLTABLES_FIELDS);
  return SQL_SUCCESS;
}
