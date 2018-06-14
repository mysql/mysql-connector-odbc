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
                      my_ulonglong rowcnt, MYSQL_FIELD *fields, uint fldcnt);

my_bool server_has_i_s(DBC *dbc);


/* no_i_s functions */
SQLRETURN
columns_no_i_s(STMT *hstmt, SQLCHAR *szCatalog, SQLSMALLINT cbCatalog,
               SQLCHAR *szSchema __attribute__((unused)),
               SQLSMALLINT cbSchema __attribute__((unused)),
               SQLCHAR *szTable, SQLSMALLINT cbTable,
               SQLCHAR *szColumn, SQLSMALLINT cbColumn);

SQLRETURN 
list_column_priv_no_i_s(SQLHSTMT hstmt,
                      SQLCHAR *catalog, SQLSMALLINT catalog_len,
                      SQLCHAR *schema __attribute__((unused)),
                      SQLSMALLINT schema_len __attribute__((unused)),
                      SQLCHAR *table, SQLSMALLINT table_len,
                      SQLCHAR *column, SQLSMALLINT column_len);

SQLRETURN
list_table_priv_no_i_s(SQLHSTMT hstmt,
                       SQLCHAR *catalog, SQLSMALLINT catalog_len,
                       SQLCHAR *schema __attribute__((unused)),
                       SQLSMALLINT schema_len __attribute__((unused)),
                       SQLCHAR *table, SQLSMALLINT table_len);

MYSQL_RES *table_status(STMT        *stmt,
                        SQLCHAR     *catalog,
                        SQLSMALLINT  catalog_length,
                        SQLCHAR     *table,
                        SQLSMALLINT  table_length,
                        my_bool      wildcard,
                        my_bool      show_tables,
                        my_bool      show_views);

MYSQL_RES *table_status_no_i_s(STMT        *stmt,
                               SQLCHAR     *catalog,
                               SQLSMALLINT  catalog_length,
                               SQLCHAR     *table,
                               SQLSMALLINT  table_length,
                               my_bool      wildcard);

SQLRETURN foreign_keys_no_i_s(SQLHSTMT hstmt,
                              SQLCHAR    *szPkCatalogName __attribute__((unused)),
                              SQLSMALLINT cbPkCatalogName __attribute__((unused)),
                              SQLCHAR    *szPkSchemaName __attribute__((unused)),
                              SQLSMALLINT cbPkSchemaName __attribute__((unused)),
                              SQLCHAR    *szPkTableName,
                              SQLSMALLINT cbPkTableName,
                              SQLCHAR    *szFkCatalogName,
                              SQLSMALLINT cbFkCatalogName,
                              SQLCHAR    *szFkSchemaName __attribute__((unused)),
                              SQLSMALLINT cbFkSchemaName __attribute__((unused)),
                              SQLCHAR    *szFkTableName,
                              SQLSMALLINT cbFkTableName);


SQLRETURN
primary_keys_no_i_s(SQLHSTMT hstmt,
                    SQLCHAR *catalog, SQLSMALLINT catalog_len,
                    SQLCHAR *schema __attribute__((unused)),
                    SQLSMALLINT schema_len __attribute__((unused)),
                    SQLCHAR *table, SQLSMALLINT table_len);


SQLRETURN
procedure_columns_no_i_s(SQLHSTMT hstmt,
                         SQLCHAR *szCatalogName, SQLSMALLINT cbCatalogName,
                         SQLCHAR *szSchemaName __attribute__((unused)),
                         SQLSMALLINT cbSchemaName __attribute__((unused)),
                         SQLCHAR *szProcName, SQLSMALLINT cbProcName,
                         SQLCHAR *szColumnName, SQLSMALLINT cbColumnName);


SQLRETURN
special_columns_no_i_s(SQLHSTMT hstmt, SQLUSMALLINT fColType,
                       SQLCHAR *szTableQualifier, SQLSMALLINT cbTableQualifier,
                       SQLCHAR *szTableOwner __attribute__((unused)),
                       SQLSMALLINT cbTableOwner __attribute__((unused)),
                       SQLCHAR *szTableName, SQLSMALLINT cbTableName,
                       SQLUSMALLINT fScope __attribute__((unused)),
                       SQLUSMALLINT fNullable __attribute__((unused)));

/*
  @purpose : retrieves a list of statistics about a single table and the
       indexes associated with the table. The driver returns the
       information as a result set.
*/

SQLRETURN
statistics_no_i_s(SQLHSTMT hstmt,
                  SQLCHAR *catalog, SQLSMALLINT catalog_len,
                  SQLCHAR *schema __attribute__((unused)),
                  SQLSMALLINT schema_len __attribute__((unused)),
                  SQLCHAR *table, SQLSMALLINT table_len,
                  SQLUSMALLINT fUnique,
                  SQLUSMALLINT fAccuracy __attribute__((unused)));

SQLRETURN
tables_no_i_s(SQLHSTMT hstmt,
              SQLCHAR *catalog, SQLSMALLINT catalog_len,
              SQLCHAR *schema, SQLSMALLINT schema_len,
              SQLCHAR *table, SQLSMALLINT table_len,
              SQLCHAR *type, SQLSMALLINT type_len);
