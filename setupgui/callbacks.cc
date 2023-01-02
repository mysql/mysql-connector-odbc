// Copyright (c) 2007, 2022, Oracle and/or its affiliates. All rights reserved.
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


/* TODO no L"" */
#include "setupgui.h"
#include "stringutil.h"
#include "windows/resource.h"
#include <driver.h>
#include <error.h>

#include <codecvt>
#include <locale>

SQLWCHAR **errorMsgs= NULL;

SQLHDBC hDBC= SQL_NULL_HDBC;


/*
  Tests if it is possible to establish connection using given DS
  returns message text. user is responsible to free that text after use.
*/
SQLWSTRING mytest(HWND hwnd, DataSource *params)
{
  SQLWSTRING msg;
  SQLWCHAR tmpbuf[1024];

  SQLHENV hEnv = nullptr;
  SQLHDBC hDbc = nullptr;
  /*
    In case of file data source we do not want it to be created
    when clicking the Test button
  */
  myodbc::HENV henv;
  SQLWCHAR *preservedSavefile= params->savefile;
  params->savefile= 0;

  try
  {
    myodbc::HDBC hdbc(henv, params);
    msg = _W(L"Connection successful");
  }
  catch(MYERROR &e)
  {
    auto s = L"Connection failed with the following error:\n" +
      std::wstring_convert<std::codecvt_utf8<wchar_t>>().from_bytes(e.message) +
      L"[" + std::wstring_convert<std::codecvt_utf8<wchar_t>>().from_bytes(e.sqlstate) +
      L"]";
    auto *w = s.c_str();
    msg = _W(w);
  }

  /* Restore savefile parameter */
  params->savefile= preservedSavefile;
  return msg;
}


BOOL mytestaccept(HWND hwnd, DataSource* params)
{
  /* TODO validation */
  return TRUE;
}


std::vector<SQLWSTRING> mygetdatabases(HWND hwnd, DataSource* params)
{
  SQLRETURN   ret;
  SQLWCHAR    catalog[MYODBC_DB_NAME_MAX];
  SQLLEN      n_catalog;
  SQLWCHAR    *preserved_database = params->database;
  BOOL        preserved_no_catalog = params->no_catalog;
  std::vector<SQLWSTRING> result;
  result.reserve(20);

  /*
    In case of file data source we do not want it to be created
    when clicking the Test button
  */
  SQLWCHAR *preserved_savefile= params->savefile;
  params->savefile = NULL;

  params->database = NULL;
  params->no_catalog = FALSE;

  myodbc::HENV henv;
  myodbc::HDBC hdbc(henv, params);

  params->savefile = preserved_savefile;
  params->database = preserved_database;
  params->no_catalog = preserved_no_catalog;

  myodbc::HSTMT hstmt(hdbc);

  ret = SQLTablesW(hstmt, (SQLWCHAR*)SQL_ALL_CATALOGS, SQL_NTS,
                      (SQLWCHAR*)L"", SQL_NTS, (SQLWCHAR*)L"", 0,
                      (SQLWCHAR*)L"", 0);
  if (!SQL_SUCCEEDED(ret))
    return result;

  ret = SQLBindCol(hstmt, 1, SQL_C_WCHAR, catalog, MYODBC_DB_NAME_MAX,
                      &n_catalog);
  if (!SQL_SUCCEEDED(ret))
    return result;

  while (true)
  {
    if (result.size() % 20)
      result.reserve(result.size() + 20);

    if (SQL_SUCCEEDED(SQLFetch(hstmt)))
      result.emplace_back(catalog);
    else
      break;
  }

  return result;
}


std::vector<SQLWSTRING> mygetcharsets(HWND hwnd, DataSource* params)
{
  SQLRETURN   ret;
  SQLWCHAR    charset[MYODBC_DB_NAME_MAX];
  SQLLEN      n_charset;
  SQLWCHAR    *preserved_database= params->database;
  BOOL        preserved_no_catalog= params->no_catalog;
  SQLWCHAR tmpbuf[1024];
  std::vector<SQLWSTRING> csl;
  csl.reserve(20);

  /*
    In case of file data source we do not want it to be created
    when clicking the Test button
  */
  SQLWCHAR *preserved_savefile= params->savefile;
  params->savefile= NULL;

  params->database= NULL;
  params->no_catalog= FALSE;

  myodbc::HENV henv;
  myodbc::HDBC hdbc(henv, params);

  params->savefile = preserved_savefile;
  params->database = preserved_database;
  params->no_catalog = preserved_no_catalog;

  myodbc::HSTMT hstmt(hdbc);


#ifdef DRIVER_ANSI
  /* Skip undesired charsets */
  nReturn = SQLExecDirectW( hStmt, _W(L"SHOW CHARACTER SET WHERE " \
                                       "charset <> 'utf16' AND " \
                                       "charset <> 'utf32' AND " \
                                       "charset <> 'ucs2'" ), SQL_NTS);
#else
  ret = SQLExecDirectW(hstmt, _W(L"SHOW CHARACTER SET"), SQL_NTS);
#endif

  if (!SQL_SUCCEEDED(ret))
    return csl;

  ret = SQLBindCol(hstmt, 1, SQL_C_WCHAR, charset, MYODBC_DB_NAME_MAX,
                      &n_charset);
  if (!SQL_SUCCEEDED(ret))
    return csl;

  while (true)
  {
    if (csl.size() % 20)
      csl.reserve(csl.size() + 20);

    if (SQL_SUCCEEDED(SQLFetch(hstmt)))
      csl.emplace_back(charset);
    else
      break;
}

  return csl;
}


/* Init DataSource from the main dialog controls */
void syncData(HWND hwnd, DataSource *params)
{
  GET_STRING(name);
  GET_STRING(description);
  GET_STRING(server);
  GET_STRING(socket);
  GET_UNSIGNED(port);
  GET_STRING(uid);
  GET_STRING(pwd);
  GET_COMBO(database);

#ifdef _WIN32
  /* use this flag exclusively for Windows */
  params->force_use_of_named_pipes = READ_BOOL(hwnd, IDC_RADIO_pipe);
#endif
}


/* Set the main dialog controls using DataSource */
void syncForm(HWND hwnd, DataSource *params)
{
  SET_STRING(name);
  SET_STRING(description);
  SET_STRING(server);
  SET_UNSIGNED(port);
  SET_STRING(uid);
  SET_STRING(pwd);
  SET_STRING(socket);
  SET_COMBO(database);

#ifdef _WIN32
  if (params->force_use_of_named_pipes)
  {
    SET_RADIO(hwnd, IDC_RADIO_pipe, TRUE);
  }
  else
  {
    SET_RADIO(hwnd, IDC_RADIO_tcp, TRUE);
  }
  SwitchTcpOrPipe(hwnd, params->force_use_of_named_pipes);
#else
  if (params->socket)
  {
    /* this flag means the socket file in Linux */
    SET_CHECKED(__UNUSED, use_socket_file, TRUE);
    SET_SENSITIVE(server, FALSE);
    SET_SENSITIVE(socket, TRUE);
  }
  else
  {
    SET_CHECKED(__UNUSED, use_tcp_ip_server, TRUE);
    SET_SENSITIVE(server, TRUE);
    SET_SENSITIVE(socket, FALSE);
  }
#endif
}

/*
 Sets the DataSource fields from the dialog inputs
*/
void syncTabsData(HWND hwnd, DataSource *params)
{  /* 1 - Connection */
  GET_BOOL_TAB(CONNECTION_TAB, allow_big_results);
  GET_BOOL_TAB(CONNECTION_TAB, use_compressed_protocol);
  GET_BOOL_TAB(CONNECTION_TAB, dont_prompt_upon_connect);
  GET_BOOL_TAB(CONNECTION_TAB, auto_reconnect);
  GET_BOOL_TAB(CONNECTION_TAB, allow_multiple_statements);
  GET_BOOL_TAB(CONNECTION_TAB, clientinteractive);
  GET_BOOL_TAB(CONNECTION_TAB, can_handle_exp_pwd);
  GET_BOOL_TAB(CONNECTION_TAB, get_server_public_key);
  GET_BOOL_TAB(CONNECTION_TAB, enable_dns_srv);

  params->has_port = !params->enable_dns_srv;

  GET_BOOL_TAB(CONNECTION_TAB, multi_host);

  GET_COMBO_TAB(CONNECTION_TAB, charset);
  GET_STRING_TAB(CONNECTION_TAB, initstmt);
  GET_STRING_TAB(CONNECTION_TAB, plugin_dir);

  /* 2 - Authentication */
  GET_BOOL_TAB(AUTH_TAB, enable_cleartext_plugin);
#ifdef _WIN32
  GET_STRING_TAB(AUTH_TAB, authentication_kerberos_mode);
#endif
  GET_STRING_TAB(AUTH_TAB, default_auth);
#if MFA_ENABLED
  GET_STRING_TAB(AUTH_TAB, pwd2);
  GET_STRING_TAB(AUTH_TAB, pwd3);
#endif
  GET_STRING_TAB(AUTH_TAB, oci_config_file);
  GET_STRING_TAB(AUTH_TAB, oci_config_profile);

  /* 3 - Metadata*/
  GET_BOOL_TAB(METADATA_TAB, change_bigint_columns_to_int);
  GET_BOOL_TAB(METADATA_TAB, handle_binary_as_char);
  GET_BOOL_TAB(METADATA_TAB, return_table_names_for_SqlDescribeCol);
  GET_BOOL_TAB(METADATA_TAB, no_catalog);
  GET_BOOL_TAB(METADATA_TAB, no_schema);
  GET_BOOL_TAB(METADATA_TAB, limit_column_size);

  /* 4 - Cursors/Results */
  GET_BOOL_TAB(CURSORS_TAB, return_matching_rows);
  GET_BOOL_TAB(CURSORS_TAB, auto_increment_null_search);
  GET_BOOL_TAB(CURSORS_TAB, dynamic_cursor);
  GET_BOOL_TAB(CURSORS_TAB, user_manager_cursor);
  GET_BOOL_TAB(CURSORS_TAB, pad_char_to_full_length);
  GET_BOOL_TAB(CURSORS_TAB, dont_cache_result);
  GET_BOOL_TAB(CURSORS_TAB, force_use_of_forward_only_cursors);
  GET_BOOL_TAB(CURSORS_TAB, zero_date_to_min);

  if (READ_BOOL_TAB(CURSORS_TAB, cursor_prefetch_active))
  {
    GET_UNSIGNED_TAB(CURSORS_TAB, cursor_prefetch_number);
  }
  else
  {
    params->cursor_prefetch_number= 0;
  }
  /* 5 - debug*/
  GET_BOOL_TAB(DEBUG_TAB,save_queries);

  /* 6 - ssl related */
  GET_STRING_TAB(SSL_TAB, sslkey);
  GET_STRING_TAB(SSL_TAB, sslcert);
  GET_STRING_TAB(SSL_TAB, sslca);
  GET_STRING_TAB(SSL_TAB, sslcapath);
  GET_STRING_TAB(SSL_TAB, sslcipher);
  GET_COMBO_TAB(SSL_TAB, sslmode);

  GET_STRING_TAB(SSL_TAB, rsakey);
  GET_BOOL_TAB(SSL_TAB, no_tls_1_2);
  GET_BOOL_TAB(SSL_TAB, no_tls_1_3);
  GET_STRING_TAB(SSL_TAB, tls_versions);
  GET_STRING_TAB(SSL_TAB, ssl_crl);
  GET_STRING_TAB(SSL_TAB, ssl_crlpath);

  /* 7 - Misc*/
  GET_BOOL_TAB(MISC_TAB, safe);
  GET_BOOL_TAB(MISC_TAB, dont_use_set_locale);
  GET_BOOL_TAB(MISC_TAB, ignore_space_after_function_names);
  GET_BOOL_TAB(MISC_TAB, read_options_from_mycnf);
  GET_BOOL_TAB(MISC_TAB, disable_transactions);
  GET_BOOL_TAB(MISC_TAB, min_date_to_zero);
  GET_BOOL_TAB(MISC_TAB, no_ssps);
  GET_BOOL_TAB(MISC_TAB, default_bigint_bind_str);
  GET_BOOL_TAB(MISC_TAB, no_date_overflow);
  GET_BOOL_TAB(MISC_TAB, enable_local_infile);
  GET_STRING_TAB(MISC_TAB, load_data_local_dir);
}

/*
 Sets the options in the dialog tabs using DataSource
*/
void syncTabs(HWND hwnd, DataSource *params)
{
  /* 1 - Connection */
  SET_BOOL_TAB(CONNECTION_TAB, allow_big_results);
  SET_BOOL_TAB(CONNECTION_TAB, use_compressed_protocol);
  SET_BOOL_TAB(CONNECTION_TAB, dont_prompt_upon_connect);
  SET_BOOL_TAB(CONNECTION_TAB, auto_reconnect);
  SET_BOOL_TAB(CONNECTION_TAB, enable_dns_srv);
  SET_BOOL_TAB(CONNECTION_TAB, allow_multiple_statements);
  SET_BOOL_TAB(CONNECTION_TAB, clientinteractive);
  SET_BOOL_TAB(CONNECTION_TAB, can_handle_exp_pwd);
  SET_BOOL_TAB(CONNECTION_TAB, get_server_public_key);
  SET_BOOL_TAB(CONNECTION_TAB, enable_dns_srv);
  SET_BOOL_TAB(CONNECTION_TAB, multi_host);

#ifdef _WIN32
  if ( getTabCtrlTabPages(CONNECTION_TAB-1))
#endif
  {
    SET_COMBO_TAB(CONNECTION_TAB, charset);
    SET_STRING_TAB(CONNECTION_TAB, initstmt);
    SET_STRING_TAB(CONNECTION_TAB, plugin_dir);
  }

  /* 2 - Authentication */
  SET_BOOL_TAB(AUTH_TAB, enable_cleartext_plugin);
#ifdef _WIN32
  SET_STRING_TAB(AUTH_TAB, authentication_kerberos_mode);
#endif
  SET_STRING_TAB(AUTH_TAB, default_auth);
#if MFA_ENABLED
  SET_STRING_TAB(AUTH_TAB, pwd2);
  SET_STRING_TAB(AUTH_TAB, pwd3);
#endif
  SET_STRING_TAB(AUTH_TAB, oci_config_file);
  SET_STRING_TAB(AUTH_TAB, oci_config_profile);

  /* 3 - Metadata*/
  SET_BOOL_TAB(METADATA_TAB, change_bigint_columns_to_int);
  SET_BOOL_TAB(METADATA_TAB, handle_binary_as_char);
  SET_BOOL_TAB(METADATA_TAB, return_table_names_for_SqlDescribeCol);
  SET_BOOL_TAB(METADATA_TAB, no_catalog);
  SET_BOOL_TAB(METADATA_TAB, no_schema);
  SET_BOOL_TAB(METADATA_TAB, limit_column_size);

  /* 4 - Cursors/Results */
  SET_BOOL_TAB(CURSORS_TAB, return_matching_rows);
  SET_BOOL_TAB(CURSORS_TAB, auto_increment_null_search);
  SET_BOOL_TAB(CURSORS_TAB, dynamic_cursor);
  SET_BOOL_TAB(CURSORS_TAB, user_manager_cursor);
  SET_BOOL_TAB(CURSORS_TAB, pad_char_to_full_length);
  SET_BOOL_TAB(CURSORS_TAB, dont_cache_result);
  SET_BOOL_TAB(CURSORS_TAB, force_use_of_forward_only_cursors);
  SET_BOOL_TAB(CURSORS_TAB, zero_date_to_min);

  if(params->cursor_prefetch_number > 0)
  {
#ifdef _WIN32
    SET_ENABLED(CURSORS_TAB, IDC_EDIT_cursor_prefetch_number, TRUE);
#endif
    SET_CHECKED_TAB(CURSORS_TAB, cursor_prefetch_active, TRUE);
    SET_UNSIGNED_TAB(CURSORS_TAB, cursor_prefetch_number);
  }

  /* 5 - debug*/
  SET_BOOL_TAB(DEBUG_TAB,save_queries);

  /* 6 - ssl related */
#ifdef _WIN32
  if ( getTabCtrlTabPages(SSL_TAB-1) )
#endif
  {
    if(params->sslkey)
      SET_STRING_TAB(SSL_TAB, sslkey);

    if(params->sslcert)
      SET_STRING_TAB(SSL_TAB, sslcert);

    if(params->sslca)
      SET_STRING_TAB(SSL_TAB, sslca);

    if(params->sslcapath)
      SET_STRING_TAB(SSL_TAB, sslcapath);

    if(params->sslcipher)
      SET_STRING_TAB(SSL_TAB, sslcipher);

    if (params->sslmode)
      SET_COMBO_TAB(SSL_TAB, sslmode);

    if(params->rsakey)
      SET_STRING_TAB(SSL_TAB, rsakey);

    if (params->ssl_crl)
      SET_STRING_TAB(SSL_TAB, ssl_crl);

    if (params->ssl_crlpath)
      SET_STRING_TAB(SSL_TAB, ssl_crlpath);

    SET_BOOL_TAB(SSL_TAB, no_tls_1_2);
    SET_BOOL_TAB(SSL_TAB, no_tls_1_3);

    SET_STRING_TAB(SSL_TAB, tls_versions);
  }

  /* 7 - Misc*/
  SET_BOOL_TAB(MISC_TAB, safe);
  SET_BOOL_TAB(MISC_TAB, dont_use_set_locale);
  SET_BOOL_TAB(MISC_TAB, ignore_space_after_function_names);
  SET_BOOL_TAB(MISC_TAB, read_options_from_mycnf);
  SET_BOOL_TAB(MISC_TAB, disable_transactions);
  SET_BOOL_TAB(MISC_TAB, min_date_to_zero);
  SET_BOOL_TAB(MISC_TAB, no_ssps);
  SET_BOOL_TAB(MISC_TAB, default_bigint_bind_str);
  SET_BOOL_TAB(MISC_TAB, no_date_overflow);
  SET_BOOL_TAB(MISC_TAB, enable_local_infile);
  SET_STRING_TAB(MISC_TAB, load_data_local_dir);
}

void FillParameters(HWND hwnd, DataSource *params)
{
  syncData(hwnd, params );
#ifdef _WIN32
  if(getTabCtrlTab())
#endif
    syncTabsData(hwnd, params);
}
