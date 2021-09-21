// Copyright (c) 2010, 2018, Oracle and/or its affiliates. All rights reserved.
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

/**
  @file  catalog.h
  @brief some definitions required for catalog functions
*/


/**
   enums for resultsets returned by catalog functions - we don't want
   magic numbers
*/

/* SQLColumns */
enum myodbcColumns {mycTABLE_CAT= 0,      mycTABLE_SCHEM,      mycTABLE_NAME,
              /*3*/ mycCOLUMN_NAME,       mycDATA_TYPE,        mycTYPE_NAME,
              /*6*/ mycCOLUMN_SIZE,       mycBUFFER_LENGTH,    mycDECIMAL_DIGITS,
              /*9*/ mycNUM_PREC_RADIX,    mycNULLABLE,         mycREMARKS,
              /*12*/mycCOLUMN_DEF,        mycSQL_DATA_TYPE,    mycSQL_DATETIME_SUB,
              /*15*/mycCHAR_OCTET_LENGTH, mycORDINAL_POSITION, mycIS_NULLABLE };

/* SQLProcedureColumns */
enum myodbcProcColumns {mypcPROCEDURE_CAT= 0, mypcPROCEDURE_SCHEM,  mypcPROCEDURE_NAME,
                  /*3*/ mypcCOLUMN_NAME,      mypcCOLUMN_TYPE,      mypcDATA_TYPE,
                  /*6*/ mypcTYPE_NAME,        mypcCOLUMN_SIZE,      mypcBUFFER_LENGTH,
                  /*9*/ mypcDECIMAL_DIGITS,   mypcNUM_PREC_RADIX,   mypcNULLABLE,
                  /*12*/mypcREMARKS,          mypcCOLUMN_DEF,       mypcSQL_DATA_TYPE,
                  /*15*/mypcSQL_DATETIME_SUB, mypcCHAR_OCTET_LENGTH,mypcORDINAL_POSITION,
                  /*18*/mypcIS_NULLABLE };

/* SQLColumns */

static char SC_type[10],SC_typename[20],SC_precision[10],SC_length[10],SC_scale[10],
SC_nullable[10], SC_coldef[10], SC_sqltype[10],SC_octlen[10],
SC_pos[10],SC_isnull[10];

static char *SQLCOLUMNS_values[]= {
    "","",NullS,NullS,SC_type,SC_typename,
    SC_precision,
    SC_length,SC_scale,"10",SC_nullable,"MySQL column",
    SC_coldef,SC_sqltype,NullS,SC_octlen,NullS,SC_isnull
};

static MYSQL_FIELD SQLCOLUMNS_fields[]=
{
  MYODBC_FIELD_NAME("TABLE_CAT", 0),
  MYODBC_FIELD_NAME("TABLE_SCHEM", 0),
  MYODBC_FIELD_NAME("TABLE_NAME", NOT_NULL_FLAG),
  MYODBC_FIELD_NAME("COLUMN_NAME", NOT_NULL_FLAG),
  MYODBC_FIELD_SHORT("DATA_TYPE", NOT_NULL_FLAG),
  MYODBC_FIELD_STRING("TYPE_NAME", 20, NOT_NULL_FLAG),
  MYODBC_FIELD_LONG("COLUMN_SIZE", 0),
  MYODBC_FIELD_LONG("BUFFER_LENGTH", 0),
  MYODBC_FIELD_SHORT("DECIMAL_DIGITS", 0),
  MYODBC_FIELD_SHORT("NUM_PREC_RADIX", 0),
  MYODBC_FIELD_SHORT("NULLABLE", NOT_NULL_FLAG),
  MYODBC_FIELD_NAME("REMARKS", 0),
  MYODBC_FIELD_NAME("COLUMN_DEF", 0),
  MYODBC_FIELD_SHORT("SQL_DATA_TYPE", NOT_NULL_FLAG),
  MYODBC_FIELD_SHORT("SQL_DATETIME_SUB", 0),
  MYODBC_FIELD_LONG("CHAR_OCTET_LENGTH", 0),
  MYODBC_FIELD_LONG("ORDINAL_POSITION", NOT_NULL_FLAG),
  MYODBC_FIELD_STRING("IS_NULLABLE", 3, 0),
};

const uint SQLCOLUMNS_FIELDS= array_elements(SQLCOLUMNS_values);

/* Some common(for i_s/no_i_s) helper functions */
const char *my_next_token(const char *prev_token,
                          const char **token,
                                char *data,
                          const char chr);

SQLRETURN
create_empty_fake_resultset(STMT *stmt, MYSQL_ROW rowval, size_t rowsize,
                            MYSQL_FIELD *fields, uint fldcnt);

SQLRETURN
create_fake_resultset(STMT *stmt, MYSQL_ROW rowval, size_t rowsize,
                      my_ulonglong rowcnt, MYSQL_FIELD *fields, uint fldcnt,
                      bool copy_rowval);

my_bool server_has_i_s(DBC *dbc);


/* no_i_s functions */
SQLRETURN
columns_no_i_s(STMT *hstmt, SQLCHAR *szCatalog, SQLSMALLINT cbCatalog,
               SQLCHAR *szSchema,
               SQLSMALLINT cbSchema,
               SQLCHAR *szTable, SQLSMALLINT cbTable,
               SQLCHAR *szColumn, SQLSMALLINT cbColumn);

SQLRETURN
list_column_priv_no_i_s(SQLHSTMT hstmt,
                      SQLCHAR *catalog, SQLSMALLINT catalog_len,
                      SQLCHAR *schema,
                      SQLSMALLINT schema_len,
                      SQLCHAR *table, SQLSMALLINT table_len,
                      SQLCHAR *column, SQLSMALLINT column_len);

SQLRETURN
list_table_priv_no_i_s(SQLHSTMT hstmt,
                       SQLCHAR *catalog, SQLSMALLINT catalog_len,
                       SQLCHAR *schema,
                       SQLSMALLINT schema_len,
                       SQLCHAR *table, SQLSMALLINT table_len);

MYSQL_RES *db_status(STMT *stmt, std::string &db);

std::string get_database_name(STMT *stmt,
                              SQLCHAR *catalog, SQLSMALLINT catalog_len,
                              SQLCHAR *schema, SQLSMALLINT schema_len,
                              bool try_reget = true);


MYSQL_RES *table_status(STMT        *stmt,
                        SQLCHAR     *db,
                        SQLSMALLINT  db_length,
                        SQLCHAR     *table,
                        SQLSMALLINT  table_length,
                        my_bool      wildcard,
                        my_bool      show_tables,
                        my_bool      show_views);

MYSQL_RES *table_status_no_i_s(STMT        *stmt,
                               SQLCHAR     *db,
                               SQLSMALLINT  db_length,
                               SQLCHAR     *table,
                               SQLSMALLINT  table_length,
                               my_bool      wildcard);

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
                              SQLSMALLINT fk_table_len);


SQLRETURN
primary_keys_no_i_s(SQLHSTMT hstmt,
                    SQLCHAR *catalog, SQLSMALLINT catalog_len,
                    SQLCHAR *schema,
                    SQLSMALLINT schema_len,
                    SQLCHAR *table, SQLSMALLINT table_len);


SQLRETURN
procedure_columns_no_i_s(SQLHSTMT hstmt,
                         SQLCHAR *szCatalogName, SQLSMALLINT cbCatalogName,
                         SQLCHAR *szSchemaName,
                         SQLSMALLINT cbSchemaName,
                         SQLCHAR *szProcName, SQLSMALLINT cbProcName,
                         SQLCHAR *szColumnName, SQLSMALLINT cbColumnName);


SQLRETURN
special_columns_no_i_s(SQLHSTMT hstmt, SQLUSMALLINT fColType,
                       SQLCHAR *szTableQualifier, SQLSMALLINT cbTableQualifier,
                       SQLCHAR *szTableOwner,
                       SQLSMALLINT cbTableOwner,
                       SQLCHAR *szTableName, SQLSMALLINT cbTableName,
                       SQLUSMALLINT fScope,
                       SQLUSMALLINT fNullable);

/*
  @purpose : retrieves a list of statistics about a single table and the
       indexes associated with the table. The driver returns the
       information as a result set.
*/

SQLRETURN
statistics_no_i_s(SQLHSTMT hstmt,
                  SQLCHAR *catalog, SQLSMALLINT catalog_len,
                  SQLCHAR *schema,
                  SQLSMALLINT schema_len,
                  SQLCHAR *table, SQLSMALLINT table_len,
                  SQLUSMALLINT fUnique,
                  SQLUSMALLINT fAccuracy);

SQLRETURN
tables_no_i_s(SQLHSTMT hstmt,
              SQLCHAR *catalog, SQLSMALLINT catalog_len,
              SQLCHAR *schema, SQLSMALLINT schema_len,
              SQLCHAR *table, SQLSMALLINT table_len,
              SQLCHAR *type, SQLSMALLINT type_len);
