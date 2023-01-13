// Modifications Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
//
// Copyright (c) 2007, 2018, Oracle and/or its affiliates. All rights reserved.
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

/*
 * Installer wrapper implementations.
 *
 * How to add a data-source parameter:
 *    Search for DS_PARAM, and follow the code around it
 *    Add an extra field to the DataSource struct (installer.h)
 *    Add a default value in ds_new()
 *    Initialize/destroy them in ds_new/ds_delete
 *    Print the field in myodbc3i.c
 *    Add to the configuration GUIs
 *
 */

#include <vector>
#include "stringutil.h"
#include "installer.h"


/*
   SQLGetPrivateProfileStringW is buggy in all releases of unixODBC
   as of 2007-12-03, so always use our replacement.
*/
#if USE_UNIXODBC
# define SQLGetPrivateProfileStringW MySQLGetPrivateProfileStringW
int INSTAPI
MySQLGetPrivateProfileStringW(const MyODBC_LPCWSTR lpszSection, const MyODBC_LPCWSTR lpszEntry,
                              const MyODBC_LPCWSTR lpszDefault, LPWSTR lpszRetBuffer,
                              int cbRetBuffer, const MyODBC_LPCWSTR lpszFilename);

#endif


/*
   Most of the installer API functions in iODBC incorrectly reset the
   config mode, so we need to save and restore it whenever we call those
   functions. These macros reduce the clutter a little bit.
*/
#if USE_IODBC
# define SAVE_MODE() UWORD config_mode= config_get()
# define RESTORE_MODE() config_set(config_mode)
#else
# define SAVE_MODE()
# define RESTORE_MODE()
#endif


/* ODBC Installer Config Wrapper */

/* a few constants */
static SQLWCHAR W_EMPTY[]= {0};
static SQLWCHAR W_ODBCINST_INI[]=
  {'O', 'D', 'B', 'C', 'I', 'N', 'S', 'T', '.', 'I', 'N', 'I', 0};
static SQLWCHAR W_ODBC_INI[]= {'O', 'D', 'B', 'C', '.', 'I', 'N', 'I', 0};
static SQLWCHAR W_CANNOT_FIND_DRIVER[]= {'C', 'a', 'n', 'n', 'o', 't', ' ',
                                         'f', 'i', 'n', 'd', ' ',
                                         'd', 'r', 'i', 'v', 'e', 'r', 0};

static SQLWCHAR W_DSN[]= {'D', 'S', 'N', 0};
static SQLWCHAR W_DRIVER[]= {'D', 'r', 'i', 'v', 'e', 'r', 0};
static SQLWCHAR W_DESCRIPTION[]=
  {'D', 'E', 'S', 'C', 'R', 'I', 'P', 'T', 'I', 'O', 'N', 0};
static SQLWCHAR W_SERVER[]= {'S', 'E', 'R', 'V', 'E', 'R', 0};
static SQLWCHAR W_UID[]= {'U', 'I', 'D', 0};
static SQLWCHAR W_USER[]= {'U', 'S', 'E', 'R', 0};
static SQLWCHAR W_PWD[]= {'P', 'W', 'D', 0};
static SQLWCHAR W_PASSWORD[]= {'P', 'A', 'S', 'S', 'W', 'O', 'R', 'D', 0};
#if MFA_ENABLED
static SQLWCHAR W_PWD1[]= {'P', 'W', 'D', '1', 0};
static SQLWCHAR W_PASSWORD1[]= {'P', 'A', 'S', 'S', 'W', 'O', 'R', 'D', '1', 0};
static SQLWCHAR W_PWD2[]= {'P', 'W', 'D', '2', 0};
static SQLWCHAR W_PASSWORD2[]= {'P', 'A', 'S', 'S', 'W', 'O', 'R', 'D', '2', 0};
static SQLWCHAR W_PWD3[]= {'P', 'W', 'D', '3', 0};
static SQLWCHAR W_PASSWORD3[]= {'P', 'A', 'S', 'S', 'W', 'O', 'R', 'D', '3', 0};
#endif
static SQLWCHAR W_DB[]= {'D', 'B', 0};
static SQLWCHAR W_DATABASE[]= {'D', 'A', 'T', 'A', 'B', 'A', 'S', 'E', 0};
static SQLWCHAR W_SOCKET[]= {'S', 'O', 'C', 'K', 'E', 'T', 0};
static SQLWCHAR W_INITSTMT[]= {'I', 'N', 'I', 'T', 'S', 'T', 'M', 'T', 0};
static SQLWCHAR W_OPTION[]= {'O', 'P', 'T', 'I', 'O', 'N', 0};
static SQLWCHAR W_CHARSET[]= {'C', 'H', 'A', 'R', 'S', 'E', 'T', 0};
static SQLWCHAR W_SSLKEY[]= {'S', 'S', 'L', 'K', 'E', 'Y', 0}; //Old One Replace by:
static SQLWCHAR W_SSL_KEY[]= {'S', 'S', 'L', '-', 'K', 'E', 'Y', 0};
static SQLWCHAR W_SSLCERT[]= {'S', 'S', 'L', 'C', 'E', 'R', 'T', 0}; //Old One Replace by:
static SQLWCHAR W_SSL_CERT[]= {'S', 'S', 'L', '-', 'C', 'E', 'R', 'T', 0};
static SQLWCHAR W_SSLCA[]= {'S', 'S', 'L', 'C', 'A', 0}; //Old One Replace by:
static SQLWCHAR W_SSL_CA[]= {'S', 'S', 'L', '-', 'C', 'A', 0};
static SQLWCHAR W_SSLCAPATH[]=
  {'S', 'S', 'L', 'C', 'A', 'P', 'A', 'T', 'H', 0}; //Old One Replace by:
static SQLWCHAR W_SSL_CAPATH[]=
  {'S', 'S', 'L', '-', 'C', 'A', 'P', 'A', 'T', 'H', 0};
static SQLWCHAR W_SSLCIPHER[]=
  {'S', 'S', 'L', 'C', 'I', 'P', 'H', 'E', 'R', 0}; //Old One Replace by:
static SQLWCHAR W_SSL_CIPHER[]=
  {'S', 'S', 'L', '-', 'C', 'I', 'P', 'H', 'E', 'R', 0};
static SQLWCHAR W_SSLVERIFY[]=
  {'S', 'S', 'L', 'V', 'E', 'R', 'I', 'F', 'Y', 0};
static SQLWCHAR W_RSAKEY[]= {'R', 'S', 'A', 'K', 'E', 'Y', 0};
static SQLWCHAR W_PORT[]= {'P', 'O', 'R', 'T', 0};
static SQLWCHAR W_SETUP[]= {'S', 'E', 'T', 'U', 'P', 0};
static SQLWCHAR W_READTIMEOUT[]=
  {'R','E','A','D','T','I','M','E','O','U','T',0};
static SQLWCHAR W_WRITETIMEOUT[]=
  {'W','R','I','T','E','T','I','M','E','O','U','T',0};
static SQLWCHAR W_FOUND_ROWS[]=
  {'F','O','U','N','D','_','R','O','W','S',0};
static SQLWCHAR W_BIG_PACKETS[]=
  {'B','I','G','_','P','A','C','K','E','T','S',0};
static SQLWCHAR W_NO_PROMPT[]=
  {'N','O','_','P','R','O','M','P','T',0};
static SQLWCHAR W_DYNAMIC_CURSOR[]=
  {'D','Y','N','A','M','I','C','_','C','U','R','S','O','R',0};
static SQLWCHAR W_NO_DEFAULT_CURSOR[]=
  {'N','O','_','D','E','F','A','U','L','T','_','C','U','R','S','O','R',0};
static SQLWCHAR W_NO_LOCALE[]=
  {'N','O','_','L','O','C','A','L','E',0};
static SQLWCHAR W_PAD_SPACE[]=
  {'P','A','D','_','S','P','A','C','E',0};
static SQLWCHAR W_FULL_COLUMN_NAMES[]=
  {'F','U','L','L','_','C','O','L','U','M','N','_','N','A','M','E','S',0};
static SQLWCHAR W_COMPRESSED_PROTO[]=
  {'C','O','M','P','R','E','S','S','E','D','_','P','R','O','T','O',0};
static SQLWCHAR W_IGNORE_SPACE[]=
  {'I','G','N','O','R','E','_','S','P','A','C','E',0};
static SQLWCHAR W_NAMED_PIPE[]=
  {'N','A','M','E','D','_','P','I','P','E',0};
static SQLWCHAR W_NO_BIGINT[]=
  {'N','O','_','B','I','G','I','N','T',0};
static SQLWCHAR W_NO_CATALOG[]=
  {'N','O','_','C','A','T','A','L','O','G',0};
static SQLWCHAR W_NO_SCHEMA[]=
  {'N','O','_','S','C','H','E','M','A',0};
static SQLWCHAR W_USE_MYCNF[]=
  {'U','S','E','_','M','Y','C','N','F',0};
static SQLWCHAR W_SAFE[]=
  {'S','A','F','E',0};
static SQLWCHAR W_NO_TRANSACTIONS[]=
  {'N','O','_','T','R','A','N','S','A','C','T','I','O','N','S',0};
static SQLWCHAR W_LOG_QUERY[]=
  {'L','O','G','_','Q','U','E','R','Y',0};
static SQLWCHAR W_NO_CACHE[]=
  {'N','O','_','C','A','C','H','E',0};
static SQLWCHAR W_FORWARD_CURSOR[]=
  {'F','O','R','W','A','R','D','_','C','U','R','S','O','R',0};
static SQLWCHAR W_AUTO_RECONNECT[]=
  {'A','U','T','O','_','R','E','C','O','N','N','E','C','T',0};
static SQLWCHAR W_ENABLE_DNS_SRV[]=
  {'E','N','A','B','L','E','_','D','N','S','_','S','R','V',0};
static SQLWCHAR W_MULTI_HOST[]=
  {'M','U','L','T','I','_','H','O','S','T',0};
static SQLWCHAR W_AUTO_IS_NULL[]=
  {'A','U','T','O','_','I','S','_','N','U','L','L',0};
static SQLWCHAR W_ZERO_DATE_TO_MIN[]=
  {'Z','E','R','O','_','D','A','T','E','_','T','O','_','M','I','N',0};
static SQLWCHAR W_MIN_DATE_TO_ZERO[]=
  {'M','I','N','_','D','A','T','E','_','T','O','_','Z','E','R','O',0};
static SQLWCHAR W_MULTI_STATEMENTS[]=
  {'M','U','L','T','I','_','S','T','A','T','E','M','E','N','T','S',0};
static SQLWCHAR W_COLUMN_SIZE_S32[]=
  {'C','O','L','U','M','N','_','S','I','Z','E','_','S','3','2',0};
static SQLWCHAR W_NO_BINARY_RESULT[]=
  {'N','O','_','B','I','N','A','R','Y','_','R','E','S','U','L','T',0};
static SQLWCHAR W_DFLT_BIGINT_BIND_STR[]=
  {'D','F','L','T','_','B','I','G','I','N','T','_','B','I','N','D','_','S','T','R',0};
static SQLWCHAR W_CLIENT_INTERACTIVE[]=
  {'I','N','T','E','R','A','C','T','I','V','E',0};
static SQLWCHAR W_PREFETCH[]= {'P','R','E','F','E','T','C','H',0};
static SQLWCHAR W_NO_SSPS[]= {'N','O','_','S','S','P','S',0};
static SQLWCHAR W_CAN_HANDLE_EXP_PWD[]=
  {'C','A','N','_','H','A','N','D','L','E','_','E','X','P','_','P','W','D',0};
static SQLWCHAR W_ENABLE_CLEARTEXT_PLUGIN[]=
  {'E','N','A','B','L','E','_','C','L','E','A', 'R', 'T', 'E', 'X', 'T', '_','P','L','U','G','I','N',0};
static SQLWCHAR W_GET_SERVER_PUBLIC_KEY[] =
{ 'G','E','T','_','S','E','R','V', 'E', 'R','_','P','U','B','L', 'I', 'C','_','K','E','Y',0 };
static SQLWCHAR W_SAVEFILE[]=  {'S','A','V','E','F','I','L','E',0};
static SQLWCHAR W_PLUGIN_DIR[]=
  {'P','L','U','G','I','N','_','D','I','R',0};
static SQLWCHAR W_DEFAULT_AUTH[]=
  {'D','E','F','A','U','L','T','_','A','U','T', 'H',0};
static SQLWCHAR W_NO_TLS_1_2[] =
{ 'N', 'O', '_', 'T', 'L', 'S', '_', '1', '_', '2', 0 };
static SQLWCHAR W_NO_TLS_1_3[] =
{ 'N', 'O', '_', 'T', 'L', 'S', '_', '1', '_', '3', 0 };
static SQLWCHAR W_SSLMODE[] =
{ 'S', 'S', 'L', 'M', 'O', 'D', 'E', 0 }; //Old One Replace by:
static SQLWCHAR W_SSL_MODE[] =
{ 'S', 'S', 'L', '-' ,'M', 'O', 'D', 'E', 0 };
static SQLWCHAR W_NO_DATE_OVERFLOW[] =
{ 'N', 'O', '_', 'D', 'A', 'T', 'E', '_', 'O', 'V', 'E', 'R', 'F', 'L', 'O', 'W', 0 };
static SQLWCHAR W_ENABLE_LOCAL_INFILE[] =
{ 'E', 'N', 'A', 'B', 'L', 'E', '_', 'L', 'O', 'C', 'A', 'L', '_', 'I', 'N', 'F', 'I', 'L', 'E', 0 };
static SQLWCHAR W_LOAD_DATA_LOCAL_DIR[] =
{ 'L', 'O', 'A', 'D', '_', 'D', 'A', 'T', 'A', '_', 'L', 'O', 'C', 'A', 'L', '_', 'D', 'I', 'R', 0 };
static SQLWCHAR W_OCI_CONFIG_FILE[] =
{ 'O', 'C', 'I', '_', 'C', 'O', 'N', 'F', 'I', 'G', '_', 'F', 'I', 'L', 'E', 0 };
static SQLWCHAR W_TLS_VERSIONS[] =
{ 'T', 'L', 'S', '-', 'V', 'E', 'R', 'S', 'I', 'O', 'N', 'S', 0 };
static SQLWCHAR W_SSL_CRL[] =
{ 'S', 'S', 'L', '-', 'C', 'R', 'L', 0 };
static SQLWCHAR W_SSL_CRLPATH[] =
{ 'S', 'S', 'L', '-', 'C', 'R', 'L', 'P', 'A', 'T', 'H', 0};

/* Failover */
static SQLWCHAR W_ENABLE_CLUSTER_FAILOVER[] = { 'E', 'N', 'A', 'B', 'L', 'E', '_', 'C', 'L', 'U', 'S', 'T', 'E', 'R', '_', 'F', 'A', 'I', 'L', 'O', 'V', 'E', 'R', 0 };
static SQLWCHAR W_ALLOW_READER_CONNECTIONS[] = { 'A', 'L', 'L', 'O', 'W', '_', 'R', 'E', 'A', 'D', 'E', 'R', '_', 'C', 'O', 'N', 'N', 'E', 'C', 'T', 'I', 'O', 'N', 'S', 0 };
static SQLWCHAR W_GATHER_PERF_METRICS[] = { 'G', 'A', 'T', 'H', 'E', 'R', '_', 'P', 'E', 'R', 'F', '_', 'M', 'E', 'T', 'R', 'I', 'C', 'S', 0 };
static SQLWCHAR W_GATHER_PERF_METRICS_PER_INSTANCE[] = { 'G', 'A', 'T', 'H', 'E', 'R', '_', 'P', 'E', 'R', 'F', '_', 'M', 'E', 'T', 'R', 'I', 'C', 'S', '_','P','E','R','_','I','N','S','T','A','N', 'C', 'E', 0 };
static SQLWCHAR W_HOST_PATTERN[] = { 'H', 'O', 'S', 'T', '_', 'P', 'A', 'T', 'T', 'E', 'R', 'N', 0 };
static SQLWCHAR W_CLUSTER_ID[] = { 'C', 'L', 'U', 'S', 'T', 'E', 'R', '_', 'I', 'D', 0 };
static SQLWCHAR W_TOPOLOGY_REFRESH_RATE[] = { 'T', 'O', 'P', 'O', 'L', 'O', 'G', 'Y', '_', 'R', 'E', 'F', 'R', 'E', 'S', 'H', '_', 'R', 'A', 'T', 'E', 0 };
static SQLWCHAR W_FAILOVER_TIMEOUT[] = { 'F', 'A', 'I', 'L', 'O', 'V', 'E', 'R', '_', 'T', 'I', 'M', 'E', 'O', 'U', 'T', 0 };
static SQLWCHAR W_FAILOVER_TOPOLOGY_REFRESH_RATE[] = { 'F', 'A', 'I', 'L', 'O', 'V', 'E', 'R', '_', 'T', 'O', 'P', 'O', 'L', 'O', 'G', 'Y', '_', 'R', 'E', 'F', 'R', 'E', 'S', 'H', '_', 'R', 'A', 'T', 'E', 0 };
static SQLWCHAR W_FAILOVER_WRITER_RECONNECT_INTERVAL[] = { 'F', 'A', 'I', 'L', 'O', 'V', 'E', 'R', '_', 'W', 'R', 'I', 'T', 'E', 'R', '_', 'R', 'E', 'C', 'O', 'N', 'N', 'E', 'C', 'T', '_', 'I', 'N', 'T', 'E', 'R', 'V', 'A', 'L', 0 };
static SQLWCHAR W_FAILOVER_READER_CONNECT_TIMEOUT[] = { 'F', 'A', 'I', 'L', 'O', 'V', 'E', 'R', '_', 'R', 'E', 'A', 'D', 'E', 'R', '_', 'C', 'O', 'N', 'N', 'E', 'C', 'T', '_', 'T', 'I', 'M', 'E', 'O', 'U', 'T', 0 };
static SQLWCHAR W_CONNECT_TIMEOUT[] = { 'C', 'O', 'N', 'N', 'E', 'C', 'T', '_', 'T', 'I', 'M', 'E', 'O', 'U', 'T', 0 };
static SQLWCHAR W_NETWORK_TIMEOUT[] = { 'N', 'E', 'T', 'W', 'O', 'R', 'K', '_', 'T', 'I', 'M', 'E', 'O', 'U', 'T', 0 };

/* Monitoring */
static SQLWCHAR W_ENABLE_FAILURE_DETECTION[] = { 'E', 'N', 'A', 'B', 'L', 'E', '_', 'F', 'A', 'I', 'L', 'U', 'R', 'E', '_', 'D', 'E', 'T', 'E', 'C', 'T', 'I', 'O', 'N', 0 };
static SQLWCHAR W_FAILURE_DETECTION_TIME[] = { 'F', 'A', 'I', 'L', 'U', 'R', 'E', '_', 'D', 'E', 'T', 'E', 'C', 'T', 'I', 'O', 'N', '_', 'T', 'I', 'M', 'E', 0 };
static SQLWCHAR W_FAILURE_DETECTION_INTERVAL[] = { 'F', 'A', 'I', 'L', 'U', 'R', 'E', '_', 'D', 'E', 'T', 'E', 'C', 'T', 'I', 'O', 'N', '_', 'I', 'N', 'T', 'E', 'R', 'V', 'A', 'L', 0 };
static SQLWCHAR W_FAILURE_DETECTION_COUNT[] = { 'F', 'A', 'I', 'L', 'U', 'R', 'E', '_', 'D', 'E', 'T', 'E', 'C', 'T', 'I', 'O', 'N', '_', 'C', 'O', 'U', 'N', 'T', 0 };
static SQLWCHAR W_MONITOR_DISPOSAL_TIME[] = { 'M', 'O', 'N', 'I', 'T', 'O', 'R', '_', 'D', 'I', 'S', 'P', 'O', 'S', 'A', 'L', '_', 'T', 'I', 'M', 'E', 0 };
static SQLWCHAR W_FAILURE_DETECTION_TIMEOUT[] = { 'F', 'A', 'I', 'L', 'U', 'R', 'E', '_', 'D', 'E', 'T', 'E', 'C', 'T', 'I', 'O', 'N', '_', 'T', 'I', 'M', 'E', 'O', 'U', 'T', 0 };

/* DS_PARAM */
/* externally used strings */
const SQLWCHAR W_DRIVER_PARAM[]= {';', 'D', 'R', 'I', 'V', 'E', 'R', '=', 0};
const SQLWCHAR W_DRIVER_NAME[]= {'M', 'y', 'S', 'Q', 'L', ' ',
                                 'O', 'D', 'B', 'C', ' ', '5', '.', '3', ' ',
                                 'D', 'r', 'i', 'v', 'e', 'r', 0};
const SQLWCHAR W_INVALID_ATTR_STR[]= {'I', 'n', 'v', 'a', 'l', 'i', 'd', ' ',
                                      'a', 't', 't', 'r', 'i', 'b', 'u', 't', 'e', ' ',
                                      's', 't', 'r', 'i', 'n', 'g', 0};

/* List of all DSN params, used when serializing to string */
static const
SQLWCHAR *dsnparams[]= {W_DSN, W_DRIVER, W_DESCRIPTION, W_SERVER,
                        W_UID, W_PWD,
#if MFA_ENABLED
                        W_PWD1, W_PWD2, W_PWD3,
#endif
                        W_DATABASE, W_SOCKET, W_INITSTMT,
                        W_PORT, W_OPTION, W_CHARSET, W_SSLKEY, W_SSL_KEY,
                        W_SSLCERT, W_SSLCERT, W_SSLCA, W_SSL_CA,
                        W_SSLCAPATH,W_SSL_CAPATH, W_SSLCIPHER, W_SSL_CIPHER,
                        W_SSLVERIFY, W_READTIMEOUT, W_WRITETIMEOUT,
                        W_FOUND_ROWS, W_BIG_PACKETS, W_NO_PROMPT,
                        W_DYNAMIC_CURSOR, W_NO_DEFAULT_CURSOR,
                        W_NO_LOCALE, W_PAD_SPACE, W_FULL_COLUMN_NAMES,
                        W_COMPRESSED_PROTO, W_IGNORE_SPACE, W_NAMED_PIPE,
                        W_NO_BIGINT, W_NO_CATALOG, W_USE_MYCNF, W_SAFE,
                        W_NO_TRANSACTIONS, W_LOG_QUERY, W_NO_CACHE,
                        W_FORWARD_CURSOR, W_AUTO_RECONNECT, W_AUTO_IS_NULL,
                        W_ZERO_DATE_TO_MIN, W_MIN_DATE_TO_ZERO,
                        W_MULTI_STATEMENTS, W_COLUMN_SIZE_S32,
                        W_NO_BINARY_RESULT, W_DFLT_BIGINT_BIND_STR,
                        W_CLIENT_INTERACTIVE, W_PREFETCH, W_NO_SSPS,
                        W_CAN_HANDLE_EXP_PWD, W_ENABLE_CLEARTEXT_PLUGIN,
                        W_GET_SERVER_PUBLIC_KEY, W_ENABLE_DNS_SRV, W_MULTI_HOST,
                        W_SAVEFILE, W_RSAKEY, W_PLUGIN_DIR, W_DEFAULT_AUTH,
                        W_NO_TLS_1_2, W_NO_TLS_1_3,
                        W_SSLMODE, W_NO_DATE_OVERFLOW, W_LOAD_DATA_LOCAL_DIR,
                        W_OCI_CONFIG_FILE, W_TLS_VERSIONS,
                        W_SSL_CRL, W_SSL_CRLPATH,
                        /* Failover */
                        W_ENABLE_CLUSTER_FAILOVER, W_ALLOW_READER_CONNECTIONS, 
                        W_GATHER_PERF_METRICS, W_GATHER_PERF_METRICS_PER_INSTANCE,
                        W_HOST_PATTERN, W_CLUSTER_ID, W_TOPOLOGY_REFRESH_RATE,
                        W_FAILOVER_TIMEOUT, W_FAILOVER_TOPOLOGY_REFRESH_RATE,
                        W_FAILOVER_WRITER_RECONNECT_INTERVAL, 
                        W_FAILOVER_READER_CONNECT_TIMEOUT, W_CONNECT_TIMEOUT,
                        W_NETWORK_TIMEOUT,
                        /* Monitoring */
                        W_ENABLE_FAILURE_DETECTION, W_FAILURE_DETECTION_TIME,
                        W_FAILURE_DETECTION_INTERVAL, W_FAILURE_DETECTION_COUNT,
                        W_MONITOR_DISPOSAL_TIME, W_FAILURE_DETECTION_TIMEOUT};
static const
int dsnparamcnt= sizeof(dsnparams) / sizeof(SQLWCHAR *);
/* DS_PARAM */

const unsigned int default_cursor_prefetch= 100;

/* convenience macro to append a single character */
#define APPEND_SQLWCHAR(buf, ctr, c) {\
  if (ctr) { \
    *((buf)++)= (c); \
    if (--(ctr)) \
        *(buf)= 0; \
  } \
}


/*
 * Check whether a parameter value needs escaping.
 */
static int value_needs_escaped(SQLWCHAR *str)
{
  SQLWCHAR c;
  while (str && (c= *str++))
  {
    if (c >= '0' && c <= '9')
      continue;
    else if (c >= 'a' && c <= 'z')
      continue;
    else if (c >= 'A' && c <= 'Z')
      continue;
    /* other non-alphanumeric characters that don't need escaping */
    switch (c)
    {
    case '_':
    case ' ':
    case '.':
      continue;
    }
    return 1;
  }
  return 0;
}


/*
 * Convenience function to get the current config mode.
 */
UWORD config_get()
{
  UWORD mode;
  SQLGetConfigMode(&mode);
  return mode;
}


/*
 * Convenience function to set the current config mode. Returns the
 * mode in use when the function was called.
 */
UWORD config_set(UWORD mode)
{
  UWORD current= config_get();
  SQLSetConfigMode(mode);
  return current;
}

unsigned int get_connect_timeout(unsigned int seconds) {
  if (seconds && seconds > 0) {
    return seconds;
  }

  return DEFAULT_CONNECT_TIMEOUT_SECS;
}

unsigned int get_network_timeout(unsigned int seconds) {
  if (seconds && seconds > 0) {
    return seconds;
  }

  return DEFAULT_NETWORK_TIMEOUT_SECS;
}

/* ODBC Installer Driver Wrapper */

/*
 * Create a new driver object. All string data is pre-allocated.
 */
Driver *driver_new()
{
  Driver *driver= (Driver *)myodbc_malloc(sizeof(Driver), MYF(0));
  if (!driver)
    return NULL;
  driver->name= (SQLWCHAR *)myodbc_malloc(ODBCDRIVER_STRLEN * sizeof(SQLWCHAR),
                                      MYF(0));
  if (!driver->name)
  {
    x_free(driver);
    return NULL;
  }
  driver->lib= (SQLWCHAR *)myodbc_malloc(ODBCDRIVER_STRLEN * sizeof(SQLWCHAR),
                                     MYF(0));
  if (!driver->lib)
  {
    x_free(driver->name);
    x_free(driver);
    return NULL;
  }
  driver->setup_lib= (SQLWCHAR *)myodbc_malloc(ODBCDRIVER_STRLEN *
                                           sizeof(SQLWCHAR), MYF(0));
  if (!driver->setup_lib)
  {
    x_free(driver->name);
    x_free(driver->lib);
    x_free(driver);
    return NULL;
  }
  /* init to empty strings */
  driver->name[0]= 0;
  driver->lib[0]= 0;
  driver->setup_lib[0]= 0;
  driver->name8= NULL;
  driver->lib8= NULL;
  driver->setup_lib8= NULL;
  return driver;
}


/*
 * Delete an existing driver object.
 */
void driver_delete(Driver *driver)
{
  x_free(driver->name);
  x_free(driver->lib);
  x_free(driver->setup_lib);
  x_free(driver->name8);
  x_free(driver->lib8);
  x_free(driver->setup_lib8);
  x_free(driver);
}


#ifdef _WIN32
/*
 * Utility function to duplicate path and remove "(x86)" chars.
 */
SQLWCHAR *remove_x86(SQLWCHAR *path, SQLWCHAR *loc)
{
  /* Program Files (x86)
   *           13^   19^
   */
  size_t chars = sqlwcharlen(loc) - 18 /* need +1 for sqlwcharncat2() */;
  SQLWCHAR *news= sqlwchardup(path, SQL_NTS);
  news[(loc-path)+13] = 0;
  sqlwcharncat2(news, loc + 19, &chars);
  return news;
}


/*
 * Compare two library paths taking into account different
 * locations of "Program Files" for 32 and 64 bit applications
 * on Windows. This is done by removing the "(x86)" from "Program
 * Files" if present in either path.
 *
 * Note: wcs* functions are used as this only needs to support
 *       Windows where SQLWCHAR is wchar_t.
 */
int Win64CompareLibs(SQLWCHAR *lib1, SQLWCHAR *lib2)
{
  int free1= 0, free2= 0;
  int rc;
  SQLWCHAR *llib1, *llib2;

  /* perform necessary transformations */
  if (llib1= wcsstr(lib1, L"Program Files (x86)"))
  {
    llib1= remove_x86(lib1, llib1);
    free1= 1;
  }
  else
    llib1= lib1;
  if (llib2= wcsstr(lib2, L"Program Files (x86)"))
  {
    llib2= remove_x86(lib2, llib2);
    free2= 1;
  }
  else
    llib2= lib2;

  /* perform the comparison */
  rc= sqlwcharcasecmp(llib1, llib2);

  if (free1)
    x_free(llib1);
  if (free2)
    x_free(llib2);
  return rc;
}
#endif /* _WIN32 */


/*
 * Lookup a driver given only the filename of the driver. This is used:
 *
 * 1. When prompting for additional DSN info upon connect when the
 *    driver uses an external setup library.
 *
 * 2. When testing a connection when adding/editing a DSN.
 */
int driver_lookup_name(Driver *driver)
{
  SQLWCHAR drivers[16384];
  SQLWCHAR *pdrv= drivers;
  SQLWCHAR driverinfo[1024];
  int len;
  WORD slen; /* WORD needed for windows */
  SAVE_MODE();

  /* get list of drivers */
#ifdef _WIN32
  if (!SQLGetInstalledDriversW(pdrv, 16383, &slen) || !(len= slen))
#else
  if (!(len = SQLGetPrivateProfileStringW(NULL, NULL, W_EMPTY, pdrv, 16383,
                                          W_ODBCINST_INI)))
#endif
    return -1;

  RESTORE_MODE();

  /* check the lib of each driver for one that matches the given lib name */
  while (len > 0)
  {
    if (SQLGetPrivateProfileStringW(pdrv, W_DRIVER, W_EMPTY, driverinfo,
                                    1023, W_ODBCINST_INI))
    {
      RESTORE_MODE();

#ifdef _WIN32
      if (!Win64CompareLibs(driverinfo, driver->lib))
#else
      /* Trying to match the driver lib or section name in odbcinst.ini */
      if (!sqlwcharcasecmp(driverinfo, driver->lib) ||
          !sqlwcharcasecmp(pdrv, driver->lib))
#endif
      {
        sqlwcharncpy(driver->name, pdrv, ODBCDRIVER_STRLEN);
        return 0;
      }
    }

    RESTORE_MODE();

    len -= sqlwcharlen(pdrv) + 1;
    pdrv += sqlwcharlen(pdrv) + 1;
  }

  return -1;
}


/*
 * Lookup a driver in the system. The driver name is read from the given
 * object. If greater-than zero is returned, additional information
 * can be obtained from SQLInstallerError(). A less-than zero return code
 * indicates that the driver could not be found.
 */
int driver_lookup(Driver *driver)
{
  SQLWCHAR buf[4096];
  SQLWCHAR *entries= buf;
  SQLWCHAR *dest;
  SAVE_MODE();

  /* if only the filename is given, we must get the driver's name */
  if (!*driver->name && *driver->lib)
  {
    if (driver_lookup_name(driver))
      return -1;
  }

  /* get entries and make sure the driver exists */
  if (SQLGetPrivateProfileStringW(driver->name, NULL, W_EMPTY, buf, 4096,
                                  W_ODBCINST_INI) < 1)
  {
    SQLPostInstallerErrorW(ODBC_ERROR_INVALID_NAME, W_CANNOT_FIND_DRIVER);
    return -1;
  }

  RESTORE_MODE();

  /* read the needed driver attributes */
  while (*entries)
  {
    dest= NULL;
    if (!sqlwcharcasecmp(W_DRIVER, entries))
      dest= driver->lib;
    else if (!sqlwcharcasecmp(W_SETUP, entries))
      dest= driver->setup_lib;
    else
      { /* unknown/unused entry */ }

    /* get the value if it's one we're looking for */
    if (dest && SQLGetPrivateProfileStringW(driver->name, entries, W_EMPTY,
                                            dest, ODBCDRIVER_STRLEN,
                                            W_ODBCINST_INI) < 1)
    {
      RESTORE_MODE();
      return 1;
    }

    RESTORE_MODE();

    entries += sqlwcharlen(entries) + 1;
  }

  return 0;
}


/*
 * Read the semi-colon delimited key-value pairs the attributes
 * necessary to popular the driver object.
 */
int driver_from_kvpair_semicolon(Driver *driver, const SQLWCHAR *attrs)
{
  const SQLWCHAR *split;
  const SQLWCHAR *end;
  SQLWCHAR attribute[100];
  SQLWCHAR *dest;

  while (*attrs)
  {
    dest= NULL;

    /* invalid key-value pair if no equals */
    if ((split= sqlwcharchr(attrs, '=')) == NULL)
      return 1;

    /* get end of key-value pair */
    if ((end= sqlwcharchr(attrs, ';')) == NULL)
      end= attrs + sqlwcharlen(attrs);

    /* check the attribute length (must not be longer than the buffer size) */
    if (split - attrs >= 100)
    {
      return 1;
    }

    /* pull out the attribute name */
    memcpy(attribute, attrs, (split - attrs) * sizeof(SQLWCHAR));
    attribute[split - attrs]= 0; /* add null term */
    ++split;

    /* if its one we want, copy it over */
    if (!sqlwcharcasecmp(W_DRIVER, attribute))
      dest= driver->lib;
    else if (!sqlwcharcasecmp(W_SETUP, attribute))
      dest= driver->setup_lib;
    else
      { /* unknown/unused attribute */ }

    if (dest)
    {
      if (end - split >= ODBCDRIVER_STRLEN)
      {
        /*
          The value is longer than the allocated buffer length
          for driver->lib or driver->setup_lib
        */
        return 1;
      }
      memcpy(dest, split, (end - split) * sizeof(SQLWCHAR));
      dest[end - split]= 0; /* add null term */
    }

    /* advanced to next attribute */
    attrs= end;
    if (*end)
      ++attrs;
  }

  return 0;
}


/*
 * Write the attributes of the driver object into key-value pairs
 * separated by single NULL chars.
 */
int driver_to_kvpair_null(Driver *driver, SQLWCHAR *attrs, size_t attrslen)
{
  *attrs= 0;
  attrs+= sqlwcharncat2(attrs, driver->name, &attrslen);

  /* append NULL-separator */
  APPEND_SQLWCHAR(attrs, attrslen, 0);

  attrs+= sqlwcharncat2(attrs, W_DRIVER, &attrslen);
  APPEND_SQLWCHAR(attrs, attrslen, '=');
  attrs+= sqlwcharncat2(attrs, driver->lib, &attrslen);

  /* append NULL-separator */
  APPEND_SQLWCHAR(attrs, attrslen, 0);

  if (*driver->setup_lib)
  {
    attrs+= sqlwcharncat2(attrs, W_SETUP, &attrslen);
    APPEND_SQLWCHAR(attrs, attrslen, '=');
    attrs+= sqlwcharncat2(attrs, driver->setup_lib, &attrslen);

    /* append NULL-separator */
    APPEND_SQLWCHAR(attrs, attrslen, 0);
  }
  if (attrslen--)
    *attrs= 0;
  return !(attrslen > 0);
}


/* ODBC Installer Data Source Wrapper */


/*
 * Create a new data source object.
 */
DataSource *ds_new()
{
  DataSource *ds= (DataSource *)myodbc_malloc(sizeof(DataSource), MYF(0));
  if (!ds)
    return NULL;
  memset(ds, 0, sizeof(DataSource));

  /* non-zero DataSource defaults here */
  ds->port = 3306;
  ds->has_port = false;
  ds->no_schema = 1;
  ds->enable_cluster_failover = true;
  ds->allow_reader_connections = false;
  ds->gather_perf_metrics = false;
  ds->topology_refresh_rate = TOPOLOGY_REFRESH_RATE_MS;
  ds->failover_timeout = FAILOVER_TIMEOUT_MS;
  ds->failover_reader_connect_timeout = FAILOVER_READER_CONNECT_TIMEOUT_MS;
  ds->failover_topology_refresh_rate = FAILOVER_TOPOLOGY_REFRESH_RATE_MS;
  ds->failover_writer_reconnect_interval = FAILOVER_WRITER_RECONNECT_INTERVAL_MS;
  ds->connect_timeout = DEFAULT_CONNECT_TIMEOUT_SECS;
  ds->network_timeout = DEFAULT_NETWORK_TIMEOUT_SECS;

  ds->enable_failure_detection = ds->enable_cluster_failover;
  ds->failure_detection_time = FAILURE_DETECTION_TIME_MS;
  ds->failure_detection_interval = FAILURE_DETECTION_INTERVAL_MS;
  ds->failure_detection_count = FAILURE_DETECTION_COUNT;
  ds->monitor_disposal_time = MONITOR_DISPOSAL_TIME_MS;
  ds->failure_detection_timeout = FAILURE_DETECTION_TIMEOUT_SECS;

  /* DS_PARAM */

  return ds;
}


/*
 * Delete an existing data source object.
 */
void ds_delete(DataSource *ds)
{
  x_free(ds->name);
  x_free(ds->driver);
  x_free(ds->description);
  x_free(ds->server);
  x_free(ds->uid);
  x_free(ds->pwd);
#if MFA_ENABLED
  x_free(ds->pwd1);
  x_free(ds->pwd2);
  x_free(ds->pwd3);
#endif
  x_free(ds->database);
  x_free(ds->socket);
  x_free(ds->initstmt);
  x_free(ds->charset);
  x_free(ds->sslkey);
  x_free(ds->sslcert);
  x_free(ds->sslca);
  x_free(ds->sslcapath);
  x_free(ds->sslcipher);
  x_free(ds->sslmode);
  x_free(ds->rsakey);
  x_free(ds->savefile);
  x_free(ds->plugin_dir);
  x_free(ds->default_auth);
  x_free(ds->oci_config_file);
  x_free(ds->tls_versions);
  x_free(ds->ssl_crl);
  x_free(ds->ssl_crlpath);
  x_free(ds->load_data_local_dir);

  x_free(ds->name8);
  x_free(ds->driver8);
  x_free(ds->description8);
  x_free(ds->server8);
  x_free(ds->uid8);
  x_free(ds->pwd8);
#if MFA_ENABLED
  x_free(ds->pwd18);
  x_free(ds->pwd28);
  x_free(ds->pwd38);
#endif
  x_free(ds->database8);
  x_free(ds->socket8);
  x_free(ds->initstmt8);
  x_free(ds->charset8);
  x_free(ds->sslkey8);
  x_free(ds->sslcert8);
  x_free(ds->sslca8);
  x_free(ds->sslcapath8);
  x_free(ds->sslcipher8);
  x_free(ds->sslmode8);
  x_free(ds->rsakey8);
  x_free(ds->savefile8);
  x_free(ds->plugin_dir8);
  x_free(ds->default_auth8);
  x_free(ds->oci_config_file8);
  x_free(ds->tls_versions8);
  x_free(ds->ssl_crl8);
  x_free(ds->ssl_crlpath8);
  x_free(ds->load_data_local_dir8);

  x_free(ds->host_pattern);
  x_free(ds->cluster_id);
  x_free(ds->host_pattern8);
  x_free(ds->cluster_id8);

  x_free(ds);
}

/*
 * Set a string attribute of a given data source object. The string
 * will be copied into the object.
 */
int ds_set_strattr(SQLCHAR** attr, const SQLCHAR* val)
{
    x_free(*attr);
    if (val && *val)
        *attr = sqlchardup(val, SQL_NTS);
    else
        *attr = NULL;
    return *attr || 0;
}

/*
 * Set a string attribute of a given data source object. The string
 * will be copied into the object.
 */
int ds_set_wstrattr(SQLWCHAR **attr, const SQLWCHAR *val)
{
  x_free(*attr);
  if (val && *val)
    *attr= sqlwchardup(val, SQL_NTS);
  else
    *attr= NULL;
  return *attr || 0;
}

/*
 * Same as ds_set_strattr, but allows truncating the given string. If
 * charcount is 0 or SQL_NTS, it will act the same as ds_set_strattr.
 */
int ds_set_strnattr(SQLCHAR** attr, const SQLCHAR* val, size_t charcount)
{
    x_free(*attr);

    if (charcount == SQL_NTS)
        charcount = strlen((char*)val);

    if (!charcount)
    {
        *attr = NULL;
        return 1;
    }

    if (val && *val)
        *attr = sqlchardup(val, charcount);
    else
        *attr = NULL;
    return *attr || 0;
}

/*
 * Same as ds_set_wstrattr, but allows truncating the given string. If
 * charcount is 0 or SQL_NTS, it will act the same as ds_set_wstrattr.
 */
int ds_set_wstrnattr(SQLWCHAR **attr, const SQLWCHAR *val, size_t charcount)
{
  x_free(*attr);

  if (charcount == SQL_NTS)
    charcount= sqlwcharlen(val);

  if (!charcount)
  {
    *attr= NULL;
    return 1;
  }

  if (val && *val)
  {
    SQLWCHAR *temp = sqlwchardup(val, charcount);
    SQLWCHAR *pos = temp;
    while (charcount)
    {
      *pos = *val;
      if (charcount > 1 && ((*val == '}' && *(val + 1) == '}')))
      {
        ++val;
        --charcount;
      }

      ++pos;
      ++val;
      --charcount;
    }
    *pos = 0; // Terminate the string
    *attr= temp;
  }
  else
    *attr= NULL;
  return *attr || 0;
}


/*
 * Internal function to map a parameter name of the data source object
 * to the pointer needed to set the parameter. Only one of strdest or
 * intdest will be set. strdest and intdest will be set for populating
 * string (SQLWCHAR *) or int parameters.
 */
void ds_map_param(DataSource *ds, const SQLWCHAR *param,
                  SQLWCHAR ***strdest, unsigned int **intdest,
                  BOOL **booldest)
{
  *strdest= NULL;
  *intdest= NULL;
  *booldest= NULL;
  /* parameter aliases can be used here, see W_UID, W_USER */
  if (!sqlwcharcasecmp(W_DSN, param))
    *strdest= &ds->name;
  else if (!sqlwcharcasecmp(W_DRIVER, param))
    *strdest= &ds->driver;
  else if (!sqlwcharcasecmp(W_DESCRIPTION, param))
    *strdest= &ds->description;
  else if (!sqlwcharcasecmp(W_SERVER, param))
    *strdest= &ds->server;
  else if (!sqlwcharcasecmp(W_UID, param))
    *strdest= &ds->uid;
  else if (!sqlwcharcasecmp(W_USER, param))
    *strdest= &ds->uid;
  else if (!sqlwcharcasecmp(W_PWD, param))
    *strdest= &ds->pwd;
  else if (!sqlwcharcasecmp(W_PASSWORD, param))
    *strdest= &ds->pwd;
#if MFA_ENABLED
  else if (!sqlwcharcasecmp(W_PWD1, param))
    *strdest= &ds->pwd1;
  else if (!sqlwcharcasecmp(W_PASSWORD1, param))
    *strdest= &ds->pwd1;
  else if (!sqlwcharcasecmp(W_PWD2, param))
    *strdest= &ds->pwd2;
  else if (!sqlwcharcasecmp(W_PASSWORD2, param))
    *strdest= &ds->pwd2;
  else if (!sqlwcharcasecmp(W_PWD3, param))
    *strdest= &ds->pwd3;
  else if (!sqlwcharcasecmp(W_PASSWORD3, param))
    *strdest= &ds->pwd3;
#endif
  else if (!sqlwcharcasecmp(W_DB, param))
    *strdest= &ds->database;
  else if (!sqlwcharcasecmp(W_DATABASE, param))
    *strdest= &ds->database;
  else if (!sqlwcharcasecmp(W_SOCKET, param))
    *strdest= &ds->socket;
  else if (!sqlwcharcasecmp(W_INITSTMT, param))
    *strdest= &ds->initstmt;
  else if (!sqlwcharcasecmp(W_CHARSET, param))
    *strdest= &ds->charset;
  else if (!sqlwcharcasecmp(W_SSLKEY, param))
    *strdest= &ds->sslkey;
  else if (!sqlwcharcasecmp(W_SSL_KEY, param))
    *strdest= &ds->sslkey;
  else if (!sqlwcharcasecmp(W_SSLCERT, param))
    *strdest= &ds->sslcert;
  else if (!sqlwcharcasecmp(W_SSL_CERT, param))
    *strdest= &ds->sslcert;
  else if (!sqlwcharcasecmp(W_SSLCA, param))
    *strdest= &ds->sslca;
  else if (!sqlwcharcasecmp(W_SSL_CA, param))
    *strdest= &ds->sslca;
  else if (!sqlwcharcasecmp(W_SSLCAPATH, param))
    *strdest= &ds->sslcapath;
  else if (!sqlwcharcasecmp(W_SSL_CAPATH, param))
    *strdest= &ds->sslcapath;
  else if (!sqlwcharcasecmp(W_SSLCIPHER, param))
    *strdest= &ds->sslcipher;
  else if (!sqlwcharcasecmp(W_SSL_CIPHER, param))
    *strdest= &ds->sslcipher;
  else if (!sqlwcharcasecmp(W_SSLMODE, param))
    *strdest = &ds->sslmode;
  else if (!sqlwcharcasecmp(W_SSL_MODE, param))
    *strdest = &ds->sslmode;
  else if (!sqlwcharcasecmp(W_SAVEFILE, param))
    *strdest= &ds->savefile;
  else if (!sqlwcharcasecmp(W_RSAKEY, param))
    *strdest= &ds->rsakey;
  else if (!sqlwcharcasecmp(W_PORT, param))
  {
    ds->has_port = true;
    *intdest= &ds->port;
  }
  else if (!sqlwcharcasecmp(W_SSLVERIFY, param))
    *intdest= &ds->sslverify;
  else if (!sqlwcharcasecmp(W_READTIMEOUT, param))
    *intdest= &ds->read_timeout;
  else if (!sqlwcharcasecmp(W_WRITETIMEOUT, param))
    *intdest= &ds->write_timeout;
  else if (!sqlwcharcasecmp(W_CLIENT_INTERACTIVE, param))
    *intdest= &ds->client_interactive;
  else if (!sqlwcharcasecmp(W_PREFETCH, param))
    *intdest= &ds->cursor_prefetch_number;
  else if (!sqlwcharcasecmp(W_FOUND_ROWS, param))
    *booldest= &ds->return_matching_rows;
  else if (!sqlwcharcasecmp(W_BIG_PACKETS, param))
    *booldest= &ds->allow_big_results;
  else if (!sqlwcharcasecmp(W_NO_PROMPT, param))
    *booldest= &ds->dont_prompt_upon_connect;
  else if (!sqlwcharcasecmp(W_DYNAMIC_CURSOR, param))
    *booldest= &ds->dynamic_cursor;
  else if (!sqlwcharcasecmp(W_NO_DEFAULT_CURSOR, param))
    *booldest= &ds->user_manager_cursor;
  else if (!sqlwcharcasecmp(W_NO_LOCALE, param))
    *booldest= &ds->dont_use_set_locale;
  else if (!sqlwcharcasecmp(W_PAD_SPACE, param))
    *booldest= &ds->pad_char_to_full_length;
  else if (!sqlwcharcasecmp(W_FULL_COLUMN_NAMES, param))
    *booldest= &ds->return_table_names_for_SqlDescribeCol;
  else if (!sqlwcharcasecmp(W_COMPRESSED_PROTO, param))
    *booldest= &ds->use_compressed_protocol;
  else if (!sqlwcharcasecmp(W_IGNORE_SPACE, param))
    *booldest= &ds->ignore_space_after_function_names;
  else if (!sqlwcharcasecmp(W_NAMED_PIPE, param))
    *booldest= &ds->force_use_of_named_pipes;
  else if (!sqlwcharcasecmp(W_NO_BIGINT, param))
    *booldest= &ds->change_bigint_columns_to_int;
  else if (!sqlwcharcasecmp(W_NO_CATALOG, param))
    *booldest= &ds->no_catalog;
  else if (!sqlwcharcasecmp(W_NO_SCHEMA, param))
    *booldest= &ds->no_schema;
  else if (!sqlwcharcasecmp(W_USE_MYCNF, param))
    *booldest= &ds->read_options_from_mycnf;
  else if (!sqlwcharcasecmp(W_SAFE, param))
    *booldest= &ds->safe;
  else if (!sqlwcharcasecmp(W_NO_TRANSACTIONS, param))
    *booldest= &ds->disable_transactions;
  else if (!sqlwcharcasecmp(W_LOG_QUERY, param))
    *booldest= &ds->save_queries;
  else if (!sqlwcharcasecmp(W_NO_CACHE, param))
    *booldest= &ds->dont_cache_result;
  else if (!sqlwcharcasecmp(W_FORWARD_CURSOR, param))
    *booldest= &ds->force_use_of_forward_only_cursors;
  else if (!sqlwcharcasecmp(W_AUTO_RECONNECT, param))
    *booldest= &ds->auto_reconnect;
  else if (!sqlwcharcasecmp(W_AUTO_IS_NULL, param))
    *booldest= &ds->auto_increment_null_search;
  else if (!sqlwcharcasecmp(W_ZERO_DATE_TO_MIN, param))
    *booldest= &ds->zero_date_to_min;
  else if (!sqlwcharcasecmp(W_MIN_DATE_TO_ZERO, param))
    *booldest= &ds->min_date_to_zero;
  else if (!sqlwcharcasecmp(W_MULTI_STATEMENTS, param))
    *booldest= &ds->allow_multiple_statements;
  else if (!sqlwcharcasecmp(W_COLUMN_SIZE_S32, param))
    *booldest= &ds->limit_column_size;
  else if (!sqlwcharcasecmp(W_NO_BINARY_RESULT, param))
    *booldest= &ds->handle_binary_as_char;
  else if (!sqlwcharcasecmp(W_DFLT_BIGINT_BIND_STR, param))
    *booldest= &ds->default_bigint_bind_str;
  else if (!sqlwcharcasecmp(W_NO_SSPS, param))
    *booldest= &ds->no_ssps;
  else if (!sqlwcharcasecmp(W_CAN_HANDLE_EXP_PWD, param))
    *booldest= &ds->can_handle_exp_pwd;
  else if (!sqlwcharcasecmp(W_ENABLE_CLEARTEXT_PLUGIN, param))
    *booldest= &ds->enable_cleartext_plugin;
  else if (!sqlwcharcasecmp(W_GET_SERVER_PUBLIC_KEY, param))
    *booldest = &ds->get_server_public_key;
  else if (!sqlwcharcasecmp(W_ENABLE_DNS_SRV, param))
    *booldest = &ds->enable_dns_srv;
  else if (!sqlwcharcasecmp(W_MULTI_HOST, param))
    *booldest = &ds->multi_host;
  else if (!sqlwcharcasecmp(W_PLUGIN_DIR, param))
    *strdest= &ds->plugin_dir;
  else if (!sqlwcharcasecmp(W_DEFAULT_AUTH, param))
    *strdest= &ds->default_auth;
  else if (!sqlwcharcasecmp(W_NO_TLS_1_2, param))
    *booldest = &ds->no_tls_1_2;
  else if (!sqlwcharcasecmp(W_NO_TLS_1_3, param))
    *booldest = &ds->no_tls_1_3;
  else if (!sqlwcharcasecmp(W_NO_DATE_OVERFLOW, param))
    *booldest = &ds->no_date_overflow;
  else if (!sqlwcharcasecmp(W_ENABLE_LOCAL_INFILE, param))
    *booldest = &ds->enable_local_infile;
  else if (!sqlwcharcasecmp(W_LOAD_DATA_LOCAL_DIR, param))
    *strdest= &ds->load_data_local_dir;
  else if (!sqlwcharcasecmp(W_OCI_CONFIG_FILE, param))
    *strdest= &ds->oci_config_file;
  else if (!sqlwcharcasecmp(W_TLS_VERSIONS, param))
    *strdest= &ds->tls_versions;
  else if (!sqlwcharcasecmp(W_SSL_CRL, param))
  *strdest = &ds->ssl_crl;
  else if (!sqlwcharcasecmp(W_SSL_CRLPATH, param))
  *strdest = &ds->ssl_crlpath;
  /* Failover*/
  else if (!sqlwcharcasecmp(W_ENABLE_CLUSTER_FAILOVER, param))
    *booldest = &ds->enable_cluster_failover;
  else if (!sqlwcharcasecmp(W_ALLOW_READER_CONNECTIONS, param))
    *booldest = &ds->allow_reader_connections;  
  else if (!sqlwcharcasecmp(W_GATHER_PERF_METRICS, param))
    *booldest = &ds->gather_perf_metrics;
  else if (!sqlwcharcasecmp(W_GATHER_PERF_METRICS_PER_INSTANCE, param))
    *booldest = &ds->gather_metrics_per_instance;
  else if (!sqlwcharcasecmp(W_HOST_PATTERN, param))
    *strdest = &ds->host_pattern;
  else if (!sqlwcharcasecmp(W_CLUSTER_ID, param))
    *strdest = &ds->cluster_id;
  else if (!sqlwcharcasecmp(W_TOPOLOGY_REFRESH_RATE, param))
    *intdest = &ds->topology_refresh_rate;
  else if (!sqlwcharcasecmp(W_FAILOVER_TIMEOUT, param))
    *intdest = &ds->failover_timeout;
  else if (!sqlwcharcasecmp(W_FAILOVER_TOPOLOGY_REFRESH_RATE, param))
    *intdest = &ds->failover_topology_refresh_rate;
  else if (!sqlwcharcasecmp(W_FAILOVER_WRITER_RECONNECT_INTERVAL, param))
    *intdest = &ds->failover_writer_reconnect_interval;
  else if (!sqlwcharcasecmp(W_FAILOVER_READER_CONNECT_TIMEOUT, param))
    *intdest = &ds->failover_reader_connect_timeout;
  else if (!sqlwcharcasecmp(W_CONNECT_TIMEOUT, param))
    *intdest = &ds->connect_timeout;
  else if (!sqlwcharcasecmp(W_NETWORK_TIMEOUT, param))
    *intdest = &ds->network_timeout;

  /* Monitoring */
  else if (!sqlwcharcasecmp(W_ENABLE_FAILURE_DETECTION, param))
    *booldest = &ds->enable_failure_detection;
  else if (!sqlwcharcasecmp(W_FAILURE_DETECTION_TIME, param))
    *intdest = &ds->failure_detection_time;
  else if (!sqlwcharcasecmp(W_FAILURE_DETECTION_INTERVAL, param))
    *intdest = &ds->failure_detection_interval;
  else if (!sqlwcharcasecmp(W_FAILURE_DETECTION_COUNT, param))
    *intdest = &ds->failure_detection_count;
  else if (!sqlwcharcasecmp(W_MONITOR_DISPOSAL_TIME, param))
    *intdest = &ds->monitor_disposal_time;
  else if (!sqlwcharcasecmp(W_FAILURE_DETECTION_TIMEOUT, param))
    *intdest = &ds->failure_detection_timeout;

  /* DS_PARAM */
}


/*
 * Lookup a data source in the system. The name will be read from
 * the object and the rest of the details will be populated.
 *
 * If greater-than zero is returned, additional information
 * can be obtained from SQLInstallerError(). A less-than zero return code
 * indicates that the driver could not be found.
 */
int ds_lookup(DataSource *ds)
{
#define DS_BUF_LEN 8192
  SQLWCHAR buf[DS_BUF_LEN];
  SQLWCHAR *entries= buf;
  SQLWCHAR **dest;
  SQLWCHAR val[256];
  int size, used;
  int rc= 0;
  UWORD config_mode= config_get();
  unsigned int *intdest;
  BOOL *booldest;
  /* No need for SAVE_MODE() because we always call config_get() above. */

  memset(buf, 0xff, sizeof(buf));

  /* get entries and check if data source exists */
  if ((size= SQLGetPrivateProfileStringW(ds->name, NULL, W_EMPTY,
                                         buf, DS_BUF_LEN, W_ODBC_INI)) < 1)
  {
    rc= -1;
    goto end;
  }

  RESTORE_MODE();

  /* Debug code to print the entries returned, char by char */
#ifdef DEBUG_MYODBC_DS_LOOKUP
  {
  int i;
  char dbuf[100];
  OutputDebugString("Dumping SQLGetPrivateProfileStringW result");
  for (i= 0; i < size; ++i)
  {
    snprintf(dbuf, sizeof(dbuf), "[%d] = %wc - 0x%x\n", i,
             (entries[i] < 0x7f && isalpha(entries[i]) ? entries[i] : 'X'),
             entries[i]);
    OutputDebugString(dbuf);
  }
  }
#endif

#ifdef _WIN32
  /*
   * In Windows XP, there is a bug in SQLGetPrivateProfileString
   * when mode is ODBC_BOTH_DSN and we are looking for a system
   * DSN. In this case SQLGetPrivateProfileString will find the
   * system dsn but return a corrupt list of attributes.
   *
   * See debug code above to print the exact data returned.
   * See also: http://support.microsoft.com/kb/909122/
   */
  if (config_mode == ODBC_BOTH_DSN &&
      /* two null chars or a null and some bogus character */
      *(entries + sqlwcharlen(entries)) == 0 &&
      (*(entries + sqlwcharlen(entries) + 1) > 0x7f ||
       *(entries + sqlwcharlen(entries) + 1) == 0))
  {
    /* revert to system mode and try again */
    config_set(ODBC_SYSTEM_DSN);
    if ((size= SQLGetPrivateProfileStringW(ds->name, NULL, W_EMPTY,
                                           buf, DS_BUF_LEN, W_ODBC_INI)) < 1)
    {
      rc= -1;
      goto end;
    }
  }
#endif

  for (used= 0;
       used < DS_BUF_LEN && entries[0];
       used += sqlwcharlen(entries) + 1,
       entries += sqlwcharlen(entries) + 1)
  {
    int valsize;
    ds_map_param(ds, entries, &dest, &intdest, &booldest);

    if ((valsize= SQLGetPrivateProfileStringW(ds->name, entries, W_EMPTY,
                                              val, ODBCDATASOURCE_STRLEN,
                                              W_ODBC_INI)) < 0)
    {
      rc= 1;
      goto end;
    }
    else if (!valsize)
      /* skip blanks */;
    else if (dest && !*dest)
      ds_set_wstrnattr(dest, val, valsize);
    else if (intdest)
      *intdest= sqlwchartoul(val, NULL);
    else if (booldest)
      *booldest= sqlwchartoul(val, NULL) > 0;
    else if (!sqlwcharcasecmp(W_OPTION, entries))
      ds_set_options(ds, ds_get_options(ds) | sqlwchartoul(val, NULL));

    RESTORE_MODE();
  }

end:
  config_set(config_mode);
  return rc;
}


/*
 * Read an attribute list (key/value pairs) into a data source
 * object. Delimiter should probably be 0 or ';'.
 */
int ds_from_kvpair(DataSource *ds, const SQLWCHAR *attrs, SQLWCHAR delim)
{
  const SQLWCHAR *split;
  const SQLWCHAR *end;
  SQLWCHAR **dest;
  SQLWCHAR attribute[100];
  int len;
  unsigned int *intdest;
  BOOL *booldest;

  while (*attrs)
  {
    if ((split= sqlwcharchr(attrs, (SQLWCHAR)'=')) == NULL)
      return 1;

    /* remove leading spaces on attribute */
    while (*attrs == ' ')
      ++attrs;
    len = split - attrs;

    /* the attribute length must not be longer than the buffer size */
    if (len >= 100)
    {
      return 1;
    }

    memcpy(attribute, attrs, len * sizeof(SQLWCHAR));
    attribute[len]= 0;
    /* remove trailing spaces on attribute */
    --len;
    while (attribute[len] == ' ')
    {
      attribute[len] = 0;
      --len;
    }

    /* remove leading and trailing spaces on value */
    while (*(++split) == ' ');

    auto find_bracket_end = [](const SQLWCHAR *wstr) {
      const SQLWCHAR *e = wstr;
      do
      {
        e = sqlwcharchr(e, '}');
        if (e == nullptr || *(e + 1) == 0)
          break;

        // Found escape }}, skip to the next bracket
        if (*(e + 1) == '}')
          e += 2;
        else
          break;

      } while (*e);
      return e;
    };

    /* check for an "escaped" value */
    if ((*split == '{' && (end = find_bracket_end(attrs)) == NULL) ||
        /* or a delimited value */
        (*split != '{' && (end= sqlwcharchr(attrs, delim)) == NULL))
      /* otherwise, take the rest of the string */
      end= attrs + sqlwcharlen(attrs);

    /* remove trailing spaces on value (not escaped part) */
    len = end - split - 1;
    while (end > split && split[len] == ' ' && split[len+1] != '}')
    {
      --len;
      --end;
    }

    /* handle deprecated options as an exception */
    if (!sqlwcharcasecmp(W_OPTION, attribute))
    {
      ds_set_options(ds, sqlwchartoul(split, NULL));
    }
    else
    {
      ds_map_param(ds, attribute, &dest, &intdest, &booldest);

      if (dest)
      {
        if (*split == '{' && *end == '}')
        {
          ds_set_wstrnattr(dest, split + 1, end - split - 1);
          ++end;
        }
        else
          ds_set_wstrnattr(dest, split, end - split);
      }
      else if (intdest)
      {
        /* we know we have a ; or NULL at the end so we just let it go */
        *intdest= sqlwchartoul(split, NULL);
      }
      else if (booldest)
      {
        *booldest= sqlwchartoul(split, NULL) > 0;
      }
    }

    attrs= end;
    /* If delim is NULL then double-NULL is the end of key-value pairs list */
    while ((delim && *attrs == delim) ||
           (!delim && !*attrs && *(attrs+1)) ||
           *attrs == ' ')
      ++attrs;
  }

  return 0;
}


/*
 * Copy data source details into an attribute string. Use attrslen
 * to limit the number of characters placed into the string.
 *
 * Return -1 for an error or truncation, otherwise the number of
 * characters written.
 */
int ds_to_kvpair(DataSource *ds, SQLWCHAR *attrs, size_t attrslen,
                 SQLWCHAR delim)
{
  int i;
  SQLWCHAR **strval;
  unsigned int *intval;
  BOOL *boolval;
  int origchars= attrslen;
  SQLWCHAR numbuf[21];

  if (!attrslen)
    return -1;

  *attrs= 0;

  for (i= 0; i < dsnparamcnt; ++i)
  {
    ds_map_param(ds, dsnparams[i], &strval, &intval, &boolval);

    /* We skip the driver if dsn name is given */
    if (!sqlwcharcasecmp(W_DRIVER, dsnparams[i]) && ds->name && *ds->name)
      continue;

    if (strval && *strval && **strval)
    {
      attrs+= sqlwcharncat2(attrs, dsnparams[i], &attrslen);
      APPEND_SQLWCHAR(attrs, attrslen, '=');
      if (value_needs_escaped(*strval))
      {
        APPEND_SQLWCHAR(attrs, attrslen, '{');
        attrs+= sqlwcharncat2(attrs, *strval, &attrslen);
        APPEND_SQLWCHAR(attrs, attrslen, '}');
      }
      else
        attrs+= sqlwcharncat2(attrs, *strval, &attrslen);
      APPEND_SQLWCHAR(attrs, attrslen, delim);
    }
    /* only write out int values if they're non-zero */
    else if (intval && *intval)
    {
      attrs+= sqlwcharncat2(attrs, dsnparams[i], &attrslen);
      APPEND_SQLWCHAR(attrs, attrslen, '=');
      sqlwcharfromul(numbuf, *intval);
      attrs+= sqlwcharncat2(attrs, numbuf, &attrslen);
      APPEND_SQLWCHAR(attrs, attrslen, delim);
    }
    else if (boolval && *boolval)
    {
      attrs+= sqlwcharncat2(attrs, dsnparams[i], &attrslen);
      APPEND_SQLWCHAR(attrs, attrslen, '=');
      APPEND_SQLWCHAR(attrs, attrslen, '1');
      APPEND_SQLWCHAR(attrs, attrslen, delim);
    }

    if (!attrslen)
      /* we don't have enough room */
      return -1;
  }

  /* always ends in delimiter, so overwrite it */
  *(attrs - 1)= 0;

  return origchars - attrslen;
}


/*
 * Copy data source details into an attribute string. Use attrslen
 * to limit the number of characters placed into the string.
 *
 * Return -1 for an error or truncation, otherwise the number of
 * characters written.
 */
int ds_to_kvpair(DataSource *ds, SQLWSTRING &attrs, SQLWCHAR delim)
{
  int i;
  SQLWCHAR **strval;
  unsigned int *intval;
  BOOL *boolval;
  SQLWCHAR numbuf[21];

  attrs.clear();

  for (i= 0; i < dsnparamcnt; ++i)
  {
    ds_map_param(ds, dsnparams[i], &strval, &intval, &boolval);

    /* We skip the driver if dsn name is given */
    if (!sqlwcharcasecmp(W_DRIVER, dsnparams[i]) && ds->name && *ds->name)
      continue;

    if (strval && *strval && **strval)
    {
      attrs.append(dsnparams[i]);
      attrs.append({(SQLWCHAR)'='});
      if (value_needs_escaped(*strval))
      {
        attrs.append({(SQLWCHAR)'{'});
        attrs.append(escape_brackets(*strval, false));
        attrs.append({(SQLWCHAR)'}'});
      }
      else
        attrs.append(escape_brackets(*strval, false));

      attrs.append({delim});
    }
    /* only write out int values if they're non-zero */
    else if (intval && *intval)
    {
      attrs.append(dsnparams[i]);
      attrs.append({(SQLWCHAR)'='});
      sqlwcharfromul(numbuf, *intval);
      attrs.append(escape_brackets(numbuf, false));
      attrs.append({delim});
    }
    else if (boolval && *boolval)
    {
      attrs.append(dsnparams[i]);
      attrs.append({(SQLWCHAR)'=', (SQLWCHAR)'1'});
      attrs.append({delim});
    }
  }

  return attrs.length();
}


/*
 * Utility method for ds_add() to add a single string
 * property via the installer api.
 */
int ds_add_strprop(const SQLWCHAR *name, const SQLWCHAR *propname,
                   const SQLWCHAR *propval)
{
  /* don't write if its null or empty string */
  if (propval && *propval)
  {
    SAVE_MODE();
    if (SQLWritePrivateProfileStringW(name, propname, propval, W_ODBC_INI))
    {
        RESTORE_MODE();
        return 0;
    }
    return 1;
  }

  return 0;
}


/*
 * Utility method for ds_add() to add a single integer
 * property via the installer api.
 */
int ds_add_intprop(const SQLWCHAR *name, const SQLWCHAR *propname, int propval,
  bool write_zero = false)
{
  SQLWCHAR buf[21];
  sqlwcharfromul(buf, propval);
  if (!propval && write_zero)
  {
    buf[0] = (SQLWCHAR)'0';
    buf[1] = 0;
  }
  return ds_add_strprop(name, propname, buf);
}


/*
 * Add the given datasource to system. Call SQLInstallerError() to get
 * further error details if non-zero is returned. ds->driver should be
 * the driver name.
 */
int ds_add(DataSource *ds)
{
  Driver *driver= NULL;
  int rc= 1;
  SAVE_MODE();

  /* Validate data source name */
  if (!SQLValidDSNW(ds->name))
    goto error;

  RESTORE_MODE();

  /* remove if exists, FYI SQLRemoveDSNFromIni returns true
   * even if the dsn isnt found, false only if there is a failure */
  if (!SQLRemoveDSNFromIniW(ds->name))
    goto error;

  RESTORE_MODE();

  /* Get the actual driver info (not just name) */
  driver= driver_new();
  memcpy(driver->name, ds->driver,
         (sqlwcharlen(ds->driver) + 1) * sizeof(SQLWCHAR));
  if (driver_lookup(driver))
  {
    SQLPostInstallerErrorW(ODBC_ERROR_INVALID_KEYWORD_VALUE,
                           W_CANNOT_FIND_DRIVER);
    goto error;
  }

  /* "Create" section for data source */
  if (!SQLWriteDSNToIniW(ds->name, driver->name))
    goto error;

  RESTORE_MODE();

#ifdef _WIN32
  /* Windows driver manager allows writing lib into the DRIVER parameter */
  if (ds_add_strprop(ds->name, W_DRIVER     , driver->lib    )) goto error;
#else
  /*
   If we write driver->lib into the DRIVER parameter with iODBC/UnixODBC
   the next time GUI will not load because it loses the relation to
   odbcinst.ini
   */
  if (ds_add_strprop(ds->name, W_DRIVER     , driver->name    )) goto error;
#endif

  /* write all fields (util method takes care of skipping blank fields) */
  if (ds_add_strprop(ds->name, W_DESCRIPTION, ds->description)) goto error;
  if (ds_add_strprop(ds->name, W_SERVER     , ds->server     )) goto error;
  if (ds_add_strprop(ds->name, W_UID        , ds->uid        )) goto error;
  if (ds_add_strprop(ds->name, W_PWD        ,
    ds->pwd ? escape_brackets(ds->pwd, false).c_str() : nullptr)) goto error;
#if MFA_ENABLED
  if (ds_add_strprop(ds->name, W_PWD1       , ds->pwd1       )) goto error;
  if (ds_add_strprop(ds->name, W_PWD2       , ds->pwd2       )) goto error;
  if (ds_add_strprop(ds->name, W_PWD3       , ds->pwd3       )) goto error;
#endif
  if (ds_add_strprop(ds->name, W_DATABASE   , ds->database   )) goto error;
  if (ds_add_strprop(ds->name, W_SOCKET     , ds->socket     )) goto error;
  if (ds_add_strprop(ds->name, W_INITSTMT   , ds->initstmt   )) goto error;
  if (ds_add_strprop(ds->name, W_CHARSET    , ds->charset    )) goto error;
  if (ds_add_strprop(ds->name, W_SSL_KEY    , ds->sslkey     )) goto error;
  if (ds_add_strprop(ds->name, W_SSL_CERT   , ds->sslcert    )) goto error;
  if (ds_add_strprop(ds->name, W_SSL_CA     , ds->sslca      )) goto error;
  if (ds_add_strprop(ds->name, W_SSL_CAPATH , ds->sslcapath  )) goto error;
  if (ds_add_strprop(ds->name, W_SSL_CIPHER , ds->sslcipher  )) goto error;
  if (ds_add_strprop(ds->name, W_SSL_MODE   , ds->sslmode    )) goto error;
  if (ds_add_strprop(ds->name, W_RSAKEY, ds->rsakey          )) goto error;
  if (ds_add_strprop(ds->name, W_SAVEFILE   , ds->savefile   )) goto error;

  if (ds_add_intprop(ds->name, W_SSLVERIFY  , ds->sslverify  )) goto error;
  if(ds->has_port)
    if (ds_add_intprop(ds->name, W_PORT       , ds->port       )) goto error;
  if (ds_add_intprop(ds->name, W_READTIMEOUT, ds->read_timeout)) goto error;
  if (ds_add_intprop(ds->name, W_WRITETIMEOUT, ds->write_timeout)) goto error;
  if (ds_add_intprop(ds->name, W_CLIENT_INTERACTIVE, ds->client_interactive)) goto error;
  if (ds_add_intprop(ds->name, W_PREFETCH   , ds->cursor_prefetch_number)) goto error;

  if (ds_add_intprop(ds->name, W_FOUND_ROWS, ds->return_matching_rows)) goto error;
  if (ds_add_intprop(ds->name, W_BIG_PACKETS, ds->allow_big_results)) goto error;
  if (ds_add_intprop(ds->name, W_NO_PROMPT, ds->dont_prompt_upon_connect)) goto error;
  if (ds_add_intprop(ds->name, W_DYNAMIC_CURSOR, ds->dynamic_cursor)) goto error;
  if (ds_add_intprop(ds->name, W_NO_DEFAULT_CURSOR, ds->user_manager_cursor)) goto error;
  if (ds_add_intprop(ds->name, W_NO_LOCALE, ds->dont_use_set_locale)) goto error;
  if (ds_add_intprop(ds->name, W_PAD_SPACE, ds->pad_char_to_full_length)) goto error;
  if (ds_add_intprop(ds->name, W_FULL_COLUMN_NAMES, ds->return_table_names_for_SqlDescribeCol)) goto error;
  if (ds_add_intprop(ds->name, W_COMPRESSED_PROTO, ds->use_compressed_protocol)) goto error;
  if (ds_add_intprop(ds->name, W_IGNORE_SPACE, ds->ignore_space_after_function_names)) goto error;
  if (ds_add_intprop(ds->name, W_NAMED_PIPE, ds->force_use_of_named_pipes)) goto error;
  if (ds_add_intprop(ds->name, W_NO_BIGINT, ds->change_bigint_columns_to_int)) goto error;
  if (ds_add_intprop(ds->name, W_NO_CATALOG, ds->no_catalog)) goto error;
  if (ds_add_intprop(ds->name, W_NO_SCHEMA, ds->no_schema, true)) goto error;
  if (ds_add_intprop(ds->name, W_USE_MYCNF, ds->read_options_from_mycnf)) goto error;
  if (ds_add_intprop(ds->name, W_SAFE, ds->safe)) goto error;
  if (ds_add_intprop(ds->name, W_NO_TRANSACTIONS, ds->disable_transactions)) goto error;
  if (ds_add_intprop(ds->name, W_LOG_QUERY, ds->save_queries)) goto error;
  if (ds_add_intprop(ds->name, W_NO_CACHE, ds->dont_cache_result)) goto error;
  if (ds_add_intprop(ds->name, W_FORWARD_CURSOR, ds->force_use_of_forward_only_cursors)) goto error;
  if (ds_add_intprop(ds->name, W_AUTO_RECONNECT, ds->auto_reconnect)) goto error;
  if (ds_add_intprop(ds->name, W_AUTO_IS_NULL, ds->auto_increment_null_search)) goto error;
  if (ds_add_intprop(ds->name, W_ZERO_DATE_TO_MIN, ds->zero_date_to_min)) goto error;
  if (ds_add_intprop(ds->name, W_MIN_DATE_TO_ZERO, ds->min_date_to_zero)) goto error;
  if (ds_add_intprop(ds->name, W_MULTI_STATEMENTS, ds->allow_multiple_statements)) goto error;
  if (ds_add_intprop(ds->name, W_COLUMN_SIZE_S32, ds->limit_column_size)) goto error;
  if (ds_add_intprop(ds->name, W_NO_BINARY_RESULT, ds->handle_binary_as_char)) goto error;
  if (ds_add_intprop(ds->name, W_DFLT_BIGINT_BIND_STR, ds->default_bigint_bind_str)) goto error;
  if (ds_add_intprop(ds->name, W_NO_SSPS, ds->no_ssps)) goto error;
  if (ds_add_intprop(ds->name, W_CAN_HANDLE_EXP_PWD, ds->can_handle_exp_pwd)) goto error;
  if (ds_add_intprop(ds->name, W_ENABLE_CLEARTEXT_PLUGIN, ds->enable_cleartext_plugin)) goto error;
  if (ds_add_intprop(ds->name, W_GET_SERVER_PUBLIC_KEY, ds->get_server_public_key)) goto error;
  if (ds_add_intprop(ds->name, W_ENABLE_DNS_SRV, ds->enable_dns_srv)) goto error;
  if (ds_add_intprop(ds->name, W_MULTI_HOST, ds->multi_host)) goto error;
  if (ds_add_strprop(ds->name, W_PLUGIN_DIR  , ds->plugin_dir  )) goto error;
  if (ds_add_strprop(ds->name, W_DEFAULT_AUTH, ds->default_auth)) goto error;
  if (ds_add_intprop(ds->name, W_NO_TLS_1_2, ds->no_tls_1_2)) goto error;
  if (ds_add_intprop(ds->name, W_NO_TLS_1_3, ds->no_tls_1_3)) goto error;
  if (ds_add_intprop(ds->name, W_NO_DATE_OVERFLOW, ds->no_date_overflow)) goto error;
  if (ds_add_intprop(ds->name, W_ENABLE_LOCAL_INFILE, ds->enable_local_infile)) goto error;
  if (ds_add_strprop(ds->name, W_LOAD_DATA_LOCAL_DIR, ds->load_data_local_dir)) goto error;
  if (ds_add_strprop(ds->name, W_OCI_CONFIG_FILE, ds->oci_config_file)) goto error;
  if (ds_add_strprop(ds->name, W_TLS_VERSIONS, ds->tls_versions)) goto error;
  if (ds_add_strprop(ds->name, W_SSL_CRL, ds->ssl_crl)) goto error;
  if (ds_add_strprop(ds->name, W_SSL_CRLPATH, ds->ssl_crlpath)) goto error;
  /* Failover */
  if (ds_add_intprop(ds->name, W_ENABLE_CLUSTER_FAILOVER, ds->enable_cluster_failover, true)) goto error;
  if (ds_add_intprop(ds->name, W_ALLOW_READER_CONNECTIONS, ds->allow_reader_connections)) goto error;
  if (ds_add_intprop(ds->name, W_GATHER_PERF_METRICS, ds->gather_perf_metrics)) goto error;
  if (ds_add_intprop(ds->name, W_GATHER_PERF_METRICS_PER_INSTANCE, ds->gather_metrics_per_instance)) goto error;
  if (ds_add_strprop(ds->name, W_HOST_PATTERN, ds->host_pattern)) goto error;
  if (ds_add_strprop(ds->name, W_CLUSTER_ID, ds->cluster_id)) goto error;
  if (ds_add_intprop(ds->name, W_TOPOLOGY_REFRESH_RATE, ds->topology_refresh_rate)) goto error;
  if (ds_add_intprop(ds->name, W_FAILOVER_TIMEOUT, ds->failover_timeout)) goto error;
  if (ds_add_intprop(ds->name, W_FAILOVER_TOPOLOGY_REFRESH_RATE, ds->failover_topology_refresh_rate)) goto error;
  if (ds_add_intprop(ds->name, W_FAILOVER_WRITER_RECONNECT_INTERVAL, ds->failover_writer_reconnect_interval)) goto error;
  if (ds_add_intprop(ds->name, W_FAILOVER_READER_CONNECT_TIMEOUT, ds->failover_reader_connect_timeout)) goto error;
  if (ds_add_intprop(ds->name, W_CONNECT_TIMEOUT, ds->connect_timeout)) goto error;
  if (ds_add_intprop(ds->name, W_NETWORK_TIMEOUT, ds->network_timeout)) goto error;

  /* Monitoring */
  if (ds_add_intprop(ds->name, W_ENABLE_FAILURE_DETECTION, ds->enable_failure_detection, true)) goto error;
  if (ds_add_intprop(ds->name, W_FAILURE_DETECTION_TIME, ds->failure_detection_time)) goto error;
  if (ds_add_intprop(ds->name, W_FAILURE_DETECTION_INTERVAL, ds->failure_detection_interval)) goto error;
  if (ds_add_intprop(ds->name, W_FAILURE_DETECTION_COUNT, ds->failure_detection_count)) goto error;
  if (ds_add_intprop(ds->name, W_MONITOR_DISPOSAL_TIME, ds->monitor_disposal_time)) goto error;
  if (ds_add_intprop(ds->name, W_FAILURE_DETECTION_TIMEOUT, ds->failure_detection_timeout)) goto error;

 /* DS_PARAM */

  rc= 0;

error:
  if (driver)
    driver_delete(driver);
  return rc;
}


/*
 * Convenience method to check if data source exists. Set the dsn
 * scope before calling this to narrow the check.
 */
int ds_exists(SQLWCHAR *name)
{
  SQLWCHAR buf[100];
  SAVE_MODE();

  /* get entries and check if data source exists */
  if (SQLGetPrivateProfileStringW(name, NULL, W_EMPTY, buf, 100, W_ODBC_INI))
    return 0;

  RESTORE_MODE();

  return 1;
}


/*
 * Get a copy of an attribute in UTF-8. You can use the attr8 style
 * pointer and it will be freed when the data source is deleted.
 *
 * ex. char *username= ds_get_utf8attr(ds->uid, &ds->uid8);
 */
char *ds_get_utf8attr(SQLWCHAR *attrw, SQLCHAR **attr8)
{
  SQLINTEGER len= SQL_NTS;
  x_free(*attr8);
  *attr8= sqlwchar_as_utf8(attrw, &len);
  return (char *)*attr8;
}


/*
 * Assign a data source attribute from a UTF-8 string.
 */
int ds_setattr_from_utf8(SQLWCHAR **attr, SQLCHAR *val8)
{
  size_t len= strlen((char *)val8);
  x_free(*attr);
  if (!(*attr= (SQLWCHAR *)myodbc_malloc((len + 1) * sizeof(SQLWCHAR), MYF(0))))
    return -1;
  utf8_as_sqlwchar(*attr, len, val8, len);
  return 0;
}


/*
 * Set DataSource member flags from deprecated options value.
 */
void ds_set_options(DataSource *ds, ulong options)
{
  ds->return_matching_rows=                 (options & FLAG_FOUND_ROWS) > 0;
  ds->allow_big_results=                    (options & FLAG_BIG_PACKETS) > 0;
  ds->dont_prompt_upon_connect=             (options & FLAG_NO_PROMPT) > 0;
  ds->dynamic_cursor=                       (options & FLAG_DYNAMIC_CURSOR) > 0;
  ds->user_manager_cursor=                  (options & FLAG_NO_DEFAULT_CURSOR) > 0;
  ds->dont_use_set_locale=                  (options & FLAG_NO_LOCALE) > 0;
  ds->pad_char_to_full_length=              (options & FLAG_PAD_SPACE) > 0;
  ds->return_table_names_for_SqlDescribeCol=(options & FLAG_FULL_COLUMN_NAMES) > 0;
  ds->use_compressed_protocol=              (options & FLAG_COMPRESSED_PROTO) > 0;
  ds->ignore_space_after_function_names=    (options & FLAG_IGNORE_SPACE) > 0;
  ds->force_use_of_named_pipes=             (options & FLAG_NAMED_PIPE) > 0;
  ds->change_bigint_columns_to_int=         (options & FLAG_NO_BIGINT) > 0;
  ds->no_catalog=                           (options & FLAG_NO_CATALOG) > 0;
  ds->read_options_from_mycnf=              (options & FLAG_USE_MYCNF) > 0;
  ds->safe=                                 (options & FLAG_SAFE) > 0;
  ds->disable_transactions=                 (options & FLAG_NO_TRANSACTIONS) > 0;
  ds->save_queries=                         (options & FLAG_LOG_QUERY) > 0;
  ds->dont_cache_result=                    (options & FLAG_NO_CACHE) > 0;
  ds->force_use_of_forward_only_cursors=    (options & FLAG_FORWARD_CURSOR) > 0;
  ds->auto_reconnect=                       (options & FLAG_AUTO_RECONNECT) > 0;
  ds->auto_increment_null_search=           (options & FLAG_AUTO_IS_NULL) > 0;
  ds->zero_date_to_min=                     (options & FLAG_ZERO_DATE_TO_MIN) > 0;
  ds->min_date_to_zero=                     (options & FLAG_MIN_DATE_TO_ZERO) > 0;
  ds->allow_multiple_statements=            (options & FLAG_MULTI_STATEMENTS) > 0;
  ds->limit_column_size=                    (options & FLAG_COLUMN_SIZE_S32) > 0;
  ds->handle_binary_as_char=                (options & FLAG_NO_BINARY_RESULT) > 0;
  ds->default_bigint_bind_str=              (options & FLAG_DFLT_BIGINT_BIND_STR) > 0;
}


/*
 * Get deprecated options value from DataSource member flags.
 */
ulong ds_get_options(DataSource *ds)
{
  ulong options= 0;

  if (ds->return_matching_rows)
    options|= FLAG_FOUND_ROWS;
  if (ds->allow_big_results)
    options|= FLAG_BIG_PACKETS;
  if (ds->dont_prompt_upon_connect)
    options|= FLAG_NO_PROMPT;
  if (ds->dynamic_cursor)
    options|= FLAG_DYNAMIC_CURSOR;
  if (ds->user_manager_cursor)
    options|= FLAG_NO_DEFAULT_CURSOR;
  if (ds->dont_use_set_locale)
    options|= FLAG_NO_LOCALE;
  if (ds->pad_char_to_full_length)
    options|= FLAG_PAD_SPACE;
  if (ds->return_table_names_for_SqlDescribeCol)
    options|= FLAG_FULL_COLUMN_NAMES;
  if (ds->use_compressed_protocol)
    options|= FLAG_COMPRESSED_PROTO;
  if (ds->ignore_space_after_function_names)
    options|= FLAG_IGNORE_SPACE;
  if (ds->force_use_of_named_pipes)
    options|= FLAG_NAMED_PIPE;
  if (ds->change_bigint_columns_to_int)
    options|= FLAG_NO_BIGINT;
  if (ds->no_catalog)
    options|= FLAG_NO_CATALOG;
  if (ds->read_options_from_mycnf)
    options|= FLAG_USE_MYCNF;
  if (ds->safe)
    options|= FLAG_SAFE;
  if (ds->disable_transactions)
    options|= FLAG_NO_TRANSACTIONS;
  if (ds->save_queries)
    options|= FLAG_LOG_QUERY;
  if (ds->dont_cache_result)
    options|= FLAG_NO_CACHE;
  if (ds->force_use_of_forward_only_cursors)
    options|= FLAG_FORWARD_CURSOR;
  if (ds->auto_reconnect)
    options|= FLAG_AUTO_RECONNECT;
  if (ds->auto_increment_null_search)
    options|= FLAG_AUTO_IS_NULL;
  if (ds->zero_date_to_min)
    options|= FLAG_ZERO_DATE_TO_MIN;
  if (ds->min_date_to_zero)
    options|= FLAG_MIN_DATE_TO_ZERO;
  if (ds->allow_multiple_statements)
    options|= FLAG_MULTI_STATEMENTS;
  if (ds->limit_column_size)
    options|= FLAG_COLUMN_SIZE_S32;
  if (ds->handle_binary_as_char)
    options|= FLAG_NO_BINARY_RESULT;
  if (ds->default_bigint_bind_str)
    options|= FLAG_DFLT_BIGINT_BIND_STR;

  return options;
}

/*
 *  copy values from ds_source to ds
 */
void ds_copy(DataSource *ds, DataSource *ds_source) {
    if (ds == NULL || ds_source == NULL) {
        return;
    }

    if (ds_source->name != nullptr) {
        ds_set_wstrnattr(&ds->name, ds_source->name,
                         sqlwcharlen(ds_source->name));

        // probably don't need to set the '8' variables, they seem to be set as
        // needed based on the non - '8', but it would look like this
        if (ds_source->name8 != nullptr) {
            ds_get_utf8attr(ds->name, &ds->name8);
        }
    }
    if (ds_source->driver != nullptr) {
        ds_set_wstrnattr(&ds->driver, ds_source->driver,
                         sqlwcharlen(ds_source->driver));
    }
    if (ds_source->description != nullptr) {
        ds_set_wstrnattr(&ds->description, ds_source->description,
                         sqlwcharlen(ds_source->description));
    }
    if (ds_source->server != nullptr) {
        ds_set_wstrnattr(&ds->server, ds_source->server,
                         sqlwcharlen(ds_source->server));
    }
    if (ds_source->uid != nullptr) {
        ds_set_wstrnattr(&ds->uid, ds_source->uid, sqlwcharlen(ds_source->uid));
    }
    if (ds_source->pwd != nullptr) {
        ds_set_wstrnattr(&ds->pwd, ds_source->pwd, sqlwcharlen(ds_source->pwd));
    }
    if (ds_source->database != nullptr) {
        ds_set_wstrnattr(&ds->database, ds_source->database,
                         sqlwcharlen(ds_source->database));
    }
    if (ds_source->socket != nullptr) {
        ds_set_wstrnattr(&ds->socket, ds_source->socket,
                         sqlwcharlen(ds_source->socket));
    }
    if (ds_source->initstmt != nullptr) {
        ds_set_wstrnattr(&ds->initstmt, ds_source->initstmt,
                         sqlwcharlen(ds_source->initstmt));
    }
    if (ds_source->charset != nullptr) {
        ds_set_wstrnattr(&ds->charset, ds_source->charset,
                         sqlwcharlen(ds_source->charset));
    }
    if (ds_source->sslkey != nullptr) {
        ds_set_wstrnattr(&ds->sslkey, ds_source->sslkey,
                         sqlwcharlen(ds_source->sslkey));
    }
    if (ds_source->sslcert != nullptr) {
        ds_set_wstrnattr(&ds->sslcert, ds_source->sslcert,
                         sqlwcharlen(ds_source->sslcert));
    }
    if (ds_source->sslca != nullptr) {
        ds_set_wstrnattr(&ds->sslca, ds_source->sslca,
                         sqlwcharlen(ds_source->sslca));
    }
    if (ds_source->sslcapath != nullptr) {
        ds_set_wstrnattr(&ds->sslcapath, ds_source->sslcapath,
                         sqlwcharlen(ds_source->sslcapath));
    }
    if (ds_source->sslcipher != nullptr) {
        ds_set_wstrnattr(&ds->sslcipher, ds_source->sslcipher,
                         sqlwcharlen(ds_source->sslcipher));
    }
    if (ds_source->sslmode != nullptr) {
        ds_set_wstrnattr(&ds->sslmode, ds_source->sslmode,
                         sqlwcharlen(ds_source->sslmode));
    }
    if (ds_source->rsakey != nullptr) {
        ds_set_wstrnattr(&ds->rsakey, ds_source->rsakey,
                         sqlwcharlen(ds_source->rsakey));
    }
    if (ds_source->savefile != nullptr) {
        ds_set_wstrnattr(&ds->savefile, ds_source->savefile,
                         sqlwcharlen(ds_source->savefile));
    }
    if (ds_source->plugin_dir != nullptr) {
        ds_set_wstrnattr(&ds->plugin_dir, ds_source->plugin_dir,
                         sqlwcharlen(ds_source->plugin_dir));
    }
    if (ds_source->default_auth != nullptr) {
        ds_set_wstrnattr(&ds->default_auth, ds_source->default_auth,
                         sqlwcharlen(ds_source->default_auth));
    }
    if (ds_source->load_data_local_dir != nullptr) {
        ds_set_wstrnattr(&ds->load_data_local_dir,
                         ds_source->load_data_local_dir,
                         sqlwcharlen(ds_source->load_data_local_dir));
    }

    ds->has_port = ds_source->has_port;
    ds->port = ds_source->port;
    ds->read_timeout = ds_source->read_timeout;
    ds->write_timeout = ds_source->write_timeout;
    ds->client_interactive = ds_source->client_interactive;

    /*  */
    ds->return_matching_rows = ds_source->return_matching_rows;
    ds->allow_big_results = ds_source->allow_big_results;
    ds->use_compressed_protocol = ds_source->use_compressed_protocol;
    ds->change_bigint_columns_to_int = ds_source->change_bigint_columns_to_int;
    ds->safe = ds_source->safe;
    ds->auto_reconnect = ds_source->auto_reconnect;
    ds->auto_increment_null_search = ds_source->auto_increment_null_search;
    ds->handle_binary_as_char = ds_source->handle_binary_as_char;
    ds->can_handle_exp_pwd = ds_source->can_handle_exp_pwd;
    ds->enable_cleartext_plugin = ds_source->enable_cleartext_plugin;
    ds->get_server_public_key = ds_source->get_server_public_key;
    /*  */
    ds->dont_prompt_upon_connect = ds_source->dont_prompt_upon_connect;
    ds->dynamic_cursor = ds_source->dynamic_cursor;
    ds->user_manager_cursor = ds_source->user_manager_cursor;
    ds->dont_use_set_locale = ds_source->dont_use_set_locale;
    ds->pad_char_to_full_length = ds_source->pad_char_to_full_length;
    ds->dont_cache_result = ds_source->dont_cache_result;
    /*  */
    ds->return_table_names_for_SqlDescribeCol =
        ds_source->return_table_names_for_SqlDescribeCol;
    ds->ignore_space_after_function_names =
        ds_source->ignore_space_after_function_names;
    ds->force_use_of_named_pipes = ds_source->force_use_of_named_pipes;
    ds->no_catalog = ds_source->no_catalog;
    ds->read_options_from_mycnf = ds_source->read_options_from_mycnf;
    ds->disable_transactions = ds_source->disable_transactions;
    ds->force_use_of_forward_only_cursors =
        ds_source->force_use_of_forward_only_cursors;
    ds->allow_multiple_statements = ds_source->allow_multiple_statements;
    ds->limit_column_size = ds_source->limit_column_size;

    ds->min_date_to_zero = ds_source->min_date_to_zero;
    ds->zero_date_to_min = ds_source->zero_date_to_min;
    ds->default_bigint_bind_str = ds_source->default_bigint_bind_str;
    /* debug */
    ds->save_queries = ds_source->save_queries;
    /* SSL */
    ds->sslverify = ds_source->sslverify;
    ds->cursor_prefetch_number = ds_source->cursor_prefetch_number;
    ds->no_ssps = ds_source->no_ssps;

    ds->no_tls_1_2 = ds_source->no_tls_1_2;
    ds->no_tls_1_3 = ds_source->no_tls_1_3;

    ds->no_date_overflow = ds_source->no_date_overflow;
    ds->enable_local_infile = ds_source->enable_local_infile;

    ds->enable_dns_srv = ds_source->enable_dns_srv;
    ds->multi_host = ds_source->multi_host;

    /* Failover */
    if (ds_source->host_pattern != nullptr) {
        ds_set_wstrnattr(&ds->host_pattern, ds_source->host_pattern,
                         sqlwcharlen(ds_source->host_pattern));
    }
    if (ds_source->cluster_id != nullptr) {
        ds_set_wstrnattr(&ds->cluster_id, ds_source->cluster_id,
                         sqlwcharlen(ds_source->cluster_id));
    }

    ds->enable_cluster_failover = ds_source->enable_cluster_failover;
    ds->allow_reader_connections = ds_source->allow_reader_connections;
    ds->gather_perf_metrics = ds_source->gather_perf_metrics;
    ds->gather_metrics_per_instance = ds_source->gather_metrics_per_instance;
    ds->topology_refresh_rate = ds_source->topology_refresh_rate;
    ds->failover_timeout = ds_source->failover_timeout;
    ds->failover_topology_refresh_rate =
        ds_source->failover_topology_refresh_rate;
    ds->failover_writer_reconnect_interval =
        ds_source->failover_writer_reconnect_interval;
    ds->failover_reader_connect_timeout =
        ds_source->failover_reader_connect_timeout;
    ds->connect_timeout = ds_source->connect_timeout;
    ds->network_timeout = ds_source->network_timeout;
    ds->enable_failure_detection = ds_source->enable_failure_detection;
    ds->failure_detection_time = ds_source->failure_detection_time;
    ds->failure_detection_interval = ds_source->failure_detection_interval;
    ds->failure_detection_count = ds_source->failure_detection_count;
    ds->monitor_disposal_time = ds_source->monitor_disposal_time;
    ds->failure_detection_timeout = ds_source->failure_detection_timeout;
}
