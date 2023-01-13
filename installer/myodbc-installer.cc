// Modifications Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
//
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

/*!
  @brief  This program will aid installers when installing/uninstalling
          MyODBC.

          This program can register/deregister a myodbc driver and create
          a sample dsn. The key thing here is that it does this with
          cross-platform code - thanks to the ODBC installer API. This is
          most useful to those creating installers (apps using myodbc or
          myodbc itself).

          For example, this program is used in the postinstall script
          of the MyODBC for Mac OS X installer package.
*/

/*
 * Installer utility application. Handles installing/removing/listing, etc
 * driver and data source entries in the system. See usage text for details.
 */

#include "MYODBC_MYSQL.h"
#include "installer.h"
#include "stringutil.h"

#include <stdio.h>
#include <stdlib.h>

const char usage[] =
"+---                                                                   \n"
"| myodbc-installer v" MYODBC_VERSION "                                 \n"
"+---                                                                   \n"
"|                                                                      \n"
"| Description                                                          \n"
"|                                                                      \n"
"|    This program is for managing MySQL Connector/ODBC configuration   \n"
"|    options at the command prompt. It can be used to register or      \n"
"|    unregister an ODBC driver, and to create, edit, or remove DSN.    \n"
"|                                                                      \n"
"|    The program has been designed to work across all platforms        \n"
"|    supported by Connector/ODBC, including MS Windows and             \n"
"|    various Unix-like systems.                                        \n"
"|                                                                      \n"
#ifndef _WIN32
#ifndef USE_IODBC
"|    The program requires that unixODBC version 2.2.14 or newer        \n"
"|    has already been installed on the system.                         \n"
"|    UnixODBC can be downloaded from here:                             \n"
"|    http://www.unixodbc.org/                                          \n"
"|                                                                      \n"
#else
"|    The program requires that iODBC version 3.52.12 or newer          \n"
"|    has already been installed on the system.                         \n"
"|    iODBC can be downloaded from here:                                \n"
"|    http://www.iodbc.org/                                             \n"
"|                                                                      \n"
#endif
#endif
"| Syntax                                                               \n"
"|                                                                      \n"
"|    myodbc-installer <Object> <Action> [Options]                      \n"
"|                                                                      \n"
"| Object                                                               \n"
"|                                                                      \n"
"|     -d driver                                                        \n"
"|     -s datasource                                                    \n"
"|                                                                      \n"
"| Action                                                               \n"
"|                                                                      \n"
"|     -l list                                                          \n"
"|     -a add (add/update for data source)                              \n"
"|     -r remove                                                        \n"
"|     -h display this help page and exit                               \n"
"|                                                                      \n"
"| Options                                                              \n"
"|                                                                      \n"
"|     -n <name>                                                        \n"
"|     -t <attribute string>                                            \n"
"|        if used for handling data source names the <attribute string> \n"
"|        can contain ODBC options for establishing connections to MySQL\n"
"|        Server. The full list of ODBC options can be obtained from    \n"
"|        http://dev.mysql.com/doc/connector-odbc/en/                   \n"
/* Note: These scope values line up with the SQLSetConfigMode constants */
"|     -c0 add as both a user and a system data source                  \n"
"|     -c1 add as a user data source                                    \n"
"|     -c2 add as a system data source (default)                        \n"
"|                                                                      \n"
"| Examples                                                             \n"
"|                                                                      \n"
"|    List drivers                                                      \n"
"|    shell> myodbc-installer -d -l                                     \n"
"|                                                                      \n"
#ifndef _WIN32
"|    Register a Unicode driver (UNIX example)                          \n"
"|    shell> myodbc-installer -d -a -n \"MySQL ODBC " MYODBC_STRSERIES " Unicode Driver\" \\ \n"
"|              -t \"DRIVER=/path/to/driver/libmyodbc8w.so;SETUP=/path/to/gui/myodbc8S.so\"\n"
"|                                                                      \n"
"|      Note                                                            \n"
"|         * The /path/to/driver is /usr/lib for 32-bit systems and     \n"
"|           some 64-bit systems, and /usr/lib64 for most 64-bit systems\n"
"|                                                                      \n"
"|         * driver_name is libmyodbc8a.so for the ANSI version and     \n"
"|           libmyodbc8w.so for the Unicode version of MySQL ODBC Driver\n"
"|                                                                      \n"
"|         * The SETUP parameter is optional; it provides location of   \n"
"|           the GUI module (libmyodbc8S.so) for DSN setup, which       \n"
"|           is not supported on Solaris and Mac OSX systems            \n"
"|                                                                      \n"
#else
"|    Register a Unicode driver (Windows example)                       \n"
"|    shell> myodbc-installer -d -a -n \"MySQL ODBC " MYODBC_STRSERIES " Unicode Driver\" \\ \n"
"|              -t \"DRIVER=myodbc8w.dll;SETUP=myodbc8S.dll\"\n"
"|                                                                      \n"
"|      Note                                                            \n"
"|         * driver_name is myodbc8a.dll for the ANSI version and       \n"
"|           myodbc8w.dll for the Unicode version of MySQL ODBC Driver  \n"
"|                                                                      \n"
#endif
"|    Add a new system data source name for Unicode driver              \n"
"|    shell> myodbc-installer -s -a -c2 -n \"test\" \\                  \n"
"|              -t \"DRIVER=MySQL ODBC " MYODBC_STRSERIES " Unicode Driver;SERVER=localhost;DATABASE=test;UID=myid;PWD=mypwd\"\n"
"|                                                                      \n"
"|    List data source name attributes for 'test'                       \n"
"|    shell> myodbc-installer -s -l -c2 -n \"test\"                    \n"
"+---                                                                   \n";

/* command line args */
#define OBJ_DRIVER 'd'
#define OBJ_DATASOURCE 's'

#define ACTION_LIST 'l'
#define ACTION_ADD 'a'
#define ACTION_REMOVE 'r'
#define ACTION_HELP 'h'

#define OPT_NAME 'n'
#define OPT_ATTR 't'
#define OPT_SCOPE 'c'

static char obj= 0;
static char action= 0;
static UWORD scope= ODBC_SYSTEM_DSN; /* default = system */

/* these two are 'paired' and the char data is converted to the wchar buf.
 * we check the char * to see if the arg was given then use the wchar in the
 * installer api. */
static char *name= NULL;
static SQLWCHAR *wname;

static char *attrstr= NULL;
static SQLWCHAR *wattrs;


void object_usage()
{
  fprintf(stderr,
          "[ERROR] Object must be either driver (d) or data source (s)\n");
}


void action_usage()
{
  fprintf(stderr, "[ERROR] One action must be specified\n");
}


void main_usage(FILE *out_file)
{
  fprintf(out_file, usage);
}


/*
 * Print an error retrieved from SQLInstallerError.
 */
void print_installer_error()
{
  int msgno;
  DWORD errcode;
  char errmsg[256];
  for(msgno= 1; msgno < 9; ++msgno)
  {
    if (SQLInstallerError(msgno, &errcode, errmsg, 256, NULL) != SQL_SUCCESS)
      return;
    fprintf(stderr, "[ERROR] SQLInstaller error %d: %s\n", errcode, errmsg);
  }
}


/*
 * Print a general ODBC error (non-odbcinst).
 */
void print_odbc_error(SQLHANDLE hnd, SQLSMALLINT type)
{
  SQLCHAR sqlstate[20];
  SQLCHAR errmsg[1000];
  SQLINTEGER nativeerr;

  fprintf(stderr, "[ERROR] ODBC error");

  if(SQL_SUCCEEDED(SQLGetDiagRec(type, hnd, 1, sqlstate, &nativeerr,
                                 errmsg, 1000, NULL)))
    printf(": [%s] %d -> %s\n", sqlstate, nativeerr, errmsg);
  printf("\n");
}


/*
 * Handler for "list driver" command (-d -l -n drivername)
 */
int list_driver_details(Driver *driver)
{
  int rc;

  /* lookup the driver */
  if ((rc= driver_lookup(driver)) < 0)
  {
    fprintf(stderr, "[ERROR] Driver not found '%s'\n",
            ds_get_utf8attr(driver->name, &driver->name8));
    return 1;
  }
  else if (rc > 0)
  {
    print_installer_error();
    return 1;
  }

  /* print driver details */
  printf("FriendlyName: %s\n", ds_get_utf8attr(driver->name, &driver->name8));
  printf("DRIVER      : %s\n", ds_get_utf8attr(driver->lib, &driver->lib8));
  printf("SETUP       : %s\n", ds_get_utf8attr(driver->setup_lib,
                                               &driver->setup_lib8));

  return 0;
}


/*
 * Handler for "list drivers" command (-d -l)
 */
int list_drivers()
{
  char buf[50000];
  char *drivers= buf;

  if (!SQLGetInstalledDrivers(buf, 50000, NULL))
  {
    print_installer_error();
    return 1;
  }

  while (*drivers)
  {
    printf("%s\n", drivers);
    drivers += strlen(drivers) + 1;
  }

  return 0;
}


#if !defined(HAVE_SQLGETPRIVATEPROFILESTRINGW) && !defined(_WIN32)
/**
  The version of iODBC that shipped with Mac OS X 10.4 does not implement
  the Unicode versions of various installer API functions, so we have to
  do it ourselves. This function is only needed in this module, so we do
  not define it on the global level to avoid conflicts
*/

BOOL INSTAPI
SQLInstallDriverExW(const MyODBC_LPCWSTR lpszDriver, const MyODBC_LPCWSTR lpszPathIn,
                    LPWSTR lpszPathOut, WORD cbPathOutMax, WORD *pcbPathOut,
                    WORD fRequest, LPDWORD lpdwUsageCount)
{
  const SQLWCHAR *pos;
  SQLINTEGER len;
  BOOL rc;
  char *driver, *pathin, *pathout;
  WORD out;

  if (!pcbPathOut)
    pcbPathOut= &out;

  /* Calculate length of double-\0 terminated string */
  pos= lpszDriver;
  while (*pos)
    pos+= sqlwcharlen(pos) + 1;
  len= pos - lpszDriver + 1;
  driver= (char *)sqlwchar_as_utf8(lpszDriver, &len);

  len= SQL_NTS;
  pathin= (char *)sqlwchar_as_utf8(lpszPathIn, &len);

  if (cbPathOutMax > 0)
    pathout= (char *)myodbc_malloc(cbPathOutMax * 4 + 1, MYF(0)); /* 4 = max utf8 charlen */

  rc= SQLInstallDriverEx(driver, pathin, pathout, cbPathOutMax * 4,
                         pcbPathOut, fRequest, lpdwUsageCount);

  if (rc == TRUE && cbPathOutMax)
    *pcbPathOut= utf8_as_sqlwchar(lpszPathOut, cbPathOutMax,
                                  (SQLCHAR *)pathout, *pcbPathOut);

  x_free(driver);
  x_free(pathin);
  x_free(pathout);

  return rc;
}


BOOL INSTAPI
SQLRemoveDriverW(const MyODBC_LPCWSTR lpszDriver, BOOL fRemoveDSN,
                 LPDWORD lpdwUsageCount)
{
  BOOL ret;
  SQLINTEGER len= SQL_NTS;
  char *driver= (char *)sqlwchar_as_utf8(lpszDriver, &len);

  ret= SQLRemoveDriver(driver, fRemoveDSN, lpdwUsageCount);

  x_free(driver);

  return ret;
}
#endif


/*
 * Handler for "add driver" command (-d -a -n drivername -t attrs)
 */
int add_driver(Driver *driver, const SQLWCHAR *attrs)
{
  DWORD usage_count;
  SQLWCHAR attrs_null[4096]; /* null-delimited pairs */
  SQLWCHAR prevpath[256];

  /* read driver attributes into object */
  if (driver_from_kvpair_semicolon(driver, attrs))
  {
    fprintf(stderr,
            "[ERROR] Could not parse key-value pair attribute string\n");
    return 1;
  }

  /* convert to null-delimited for installer API */
  if (driver_to_kvpair_null(driver, attrs_null, 4096))
  {
    fprintf(stderr,
            "[ERROR] Could not create new key-value pair attribute string\n");
    return 1;
  }

  if (SQLInstallDriverExW(attrs_null, NULL, prevpath, 256, NULL,
                          ODBC_INSTALL_COMPLETE, &usage_count) != TRUE)
  {
    print_installer_error();
    return 1;
  }

  printf("Success: Usage count is %d\n", usage_count);

  return 0;
}


/*
 * Handler for "remove driver" command (-d -a -n drivername)
 */
int remove_driver(Driver *driver)
{
  DWORD usage_count;

  if (SQLRemoveDriverW(driver->name, FALSE, &usage_count) != TRUE)
  {
    print_installer_error();
    return 1;
  }

  printf("Success: Usage count is %d\n", usage_count);

  return 0;
}


/*
 * Handle driver actions. We setup a driver object to be used
 * for all actions.
 */
int handle_driver_action()
{
  int rc= 0;
  UWORD config_mode= config_set(scope);
  Driver *driver= driver_new();

  /* check name is given if needed */
  switch (action)
  {
  case ACTION_ADD:
  case ACTION_REMOVE:
    if (!name)
    {
      fprintf(stderr, "[ERROR] Name missing to add/remove driver\n");
      rc= 1;
      goto end;
    }
  }

  /* set the driver name */
  if (name)
    sqlwcharncpy(driver->name, wname, ODBCDRIVER_STRLEN);

  /* perform given action */
  switch (action)
  {
  case ACTION_LIST:
    if (name)
      rc= list_driver_details(driver);
    else
      rc= list_drivers();
    break;
  case ACTION_ADD:
    if (attrstr)
      rc= add_driver(driver, wattrs);
    else
    {
      fprintf(stderr, "[ERROR] Attribute string missing to add driver\n");
      rc= 1;
    }
    break;
  case ACTION_REMOVE:
    rc= remove_driver(driver);
    break;
  }

end:
  driver_delete(driver);
  config_set(config_mode);
  return rc;
}


/*
 * Handler for "list data source" command (-s -l -n drivername)
 */
int list_datasource_details(DataSource *ds)
{
  int rc;

  if ((rc= ds_lookup(ds)) < 0)
  {
    fprintf(stderr, "[ERROR] Data source not found '%s'\n",
            ds_get_utf8attr(ds->name, &ds->name8));
    return 1;
  }
  else if (rc > 0)
  {
    print_installer_error();
    return 1;
  }

  /* print the data source fields */
                       printf("Name:                %s\n", ds_get_utf8attr(ds->name, &ds->name8));
  if (ds->driver     ) printf("Driver:              %s\n", ds_get_utf8attr(ds->driver, &ds->driver8));
  if (ds->description) printf("Description:         %s\n", ds_get_utf8attr(ds->description, &ds->description8));
  if (ds->server     ) printf("Server:              %s\n", ds_get_utf8attr(ds->server, &ds->server8));
  if (ds->uid        ) printf("Uid:                 %s\n", ds_get_utf8attr(ds->uid, &ds->uid8));
  if (ds->pwd        ) printf("Pwd:                 %s\n", ds_get_utf8attr(ds->pwd, &ds->pwd8));
#if MFA_ENABLED
  if (ds->pwd1       ) printf("Pwd1:                %s\n", ds_get_utf8attr(ds->pwd1, &ds->pwd18));
  if (ds->pwd2       ) printf("Pwd2:                %s\n", ds_get_utf8attr(ds->pwd2, &ds->pwd28));
  if (ds->pwd3       ) printf("Pwd3:                %s\n", ds_get_utf8attr(ds->pwd3, &ds->pwd38));
#endif
  if (ds->database   ) printf("Database:            %s\n", ds_get_utf8attr(ds->database, &ds->database8));
  if (ds->socket     ) printf("Socket:              %s\n", ds_get_utf8attr(ds->socket, &ds->socket8));
  if (ds->initstmt   ) printf("Initial statement:   %s\n", ds_get_utf8attr(ds->initstmt, &ds->initstmt8));
  if (ds->charset    ) printf("Character set:       %s\n", ds_get_utf8attr(ds->charset, &ds->charset8));
  if (ds->sslkey     ) printf("SSL key:             %s\n", ds_get_utf8attr(ds->sslkey, &ds->sslkey8));
  if (ds->sslcert    ) printf("SSL cert:            %s\n", ds_get_utf8attr(ds->sslcert, &ds->sslcert8));
  if (ds->sslca      ) printf("SSL CA:              %s\n", ds_get_utf8attr(ds->sslca, &ds->sslca8));
  if (ds->sslcapath  ) printf("SSL CA path:         %s\n", ds_get_utf8attr(ds->sslcapath, &ds->sslcapath8));
  if (ds->sslcipher  ) printf("SSL cipher:          %s\n", ds_get_utf8attr(ds->sslcipher, &ds->sslcipher8));
  if (ds->sslmode   ) printf("SSL Mode:             %s\n", ds_get_utf8attr(ds->sslmode, &ds->sslmode8));
  if (ds->sslverify) printf("Verify SSL cert:      yes\n");
  if (ds->rsakey)    printf("RSA public key:       %s\n", ds_get_utf8attr(ds->rsakey, &ds->rsakey8));
  if (ds->tls_versions) printf("TLS Versions:        %s\n", ds_get_utf8attr(ds->tls_versions, &ds->tls_versions8));
  if (ds->ssl_crl   ) printf("SSL CRL:             %s\n", ds_get_utf8attr(ds->ssl_crl, &ds->ssl_crl8));
  if (ds->ssl_crlpath) printf("SSL CRL path:        %s\n", ds_get_utf8attr(ds->ssl_crlpath, &ds->ssl_crlpath8));
  if (ds->port && ds->has_port) printf("Port:                %d\n", ds->port);
  if (ds->plugin_dir  ) printf("Plugin directory:    %s\n", ds_get_utf8attr(ds->plugin_dir, &ds->plugin_dir8));
  if (ds->default_auth) printf("Default Authentication Library: %s\n", ds_get_utf8attr(ds->default_auth, &ds->default_auth8));
  if (ds->oci_config_file) printf("OCI Config File: %s\n", ds_get_utf8attr(ds->oci_config_file, &ds->oci_config_file8));
  /* Failover */
  if (ds->host_pattern) printf("Failover Instance Host pattern:   %s\n", ds_get_utf8attr(ds->host_pattern, &ds->host_pattern8));
  if (ds->cluster_id) printf("Failover Cluster ID:   %s\n", ds_get_utf8attr(ds->cluster_id, &ds->cluster_id8));

  printf("Options:\n");
  if (ds->return_matching_rows) printf("\tFOUND_ROWS\n");
  if (ds->allow_big_results) printf("\tBIG_PACKETS\n");
  if (ds->dont_prompt_upon_connect) printf("\tNO_PROMPT\n");
  if (ds->dynamic_cursor) printf("\tDYNAMIC_CURSOR\n");
  if (ds->user_manager_cursor) printf("\tNO_DEFAULT_CURSOR\n");
  if (ds->dont_use_set_locale) printf("\tNO_LOCALE\n");
  if (ds->pad_char_to_full_length) printf("\tPAD_SPACE\n");
  if (ds->return_table_names_for_SqlDescribeCol) printf("\tFULL_COLUMN_NAMES\n");
  if (ds->use_compressed_protocol) printf("\tCOMPRESSED_PROTO\n");
  if (ds->ignore_space_after_function_names) printf("\tIGNORE_SPACE\n");
  if (ds->force_use_of_named_pipes) printf("\tNAMED_PIPE\n");
  if (ds->change_bigint_columns_to_int) printf("\tNO_BIGINT\n");
  if (ds->no_catalog) printf("\tNO_CATALOG\n");
  if (ds->no_schema) printf("\tNO_SCHEMA\n");
  if (ds->read_options_from_mycnf) printf("\tUSE_MYCNF\n");
  if (ds->safe) printf("\tSAFE\n");
  if (ds->disable_transactions) printf("\tNO_TRANSACTIONS\n");
  if (ds->save_queries) printf("\tLOG_QUERY\n");
  if (ds->dont_cache_result) printf("\tNO_CACHE\n");
  if (ds->force_use_of_forward_only_cursors) printf("\tFORWARD_CURSOR\n");
  if (ds->auto_reconnect) printf("\tAUTO_RECONNECT\n");
  if (ds->client_interactive) printf("\tINTERACTIVE\n");
  if (ds->auto_increment_null_search) printf("\tAUTO_IS_NULL\n");
  if (ds->zero_date_to_min) printf("\tZERO_DATE_TO_MIN\n");
  if (ds->min_date_to_zero) printf("\tMIN_DATE_TO_ZERO\n");
  if (ds->allow_multiple_statements) printf("\tMULTI_STATEMENTS\n");
  if (ds->limit_column_size) printf("\tCOLUMN_SIZE_S32\n");
  if (ds->handle_binary_as_char) printf("\tNO_BINARY_RESULT\n");
  if (ds->default_bigint_bind_str) printf("\tDFLT_BIGINT_BIND_STR\n");
  if (ds->no_date_overflow) printf("\tNO_DATE_OVERFLOW\n");
  if (ds->enable_local_infile) printf("\tENABLE_LOCAL_INFILE\n");
  if (ds->no_tls_1_2) printf("\tNO_TLS_1_2\n");
  if (ds->no_tls_1_3) printf("\tNO_TLS_1_3\n");
  if (ds->no_ssps) printf("\tNO_SSPS\n");
  if (ds->cursor_prefetch_number) printf("\tPREFETCH=%d\n", ds->cursor_prefetch_number);
  if (ds->read_timeout) printf("\tREAD_TIMEOUT=%d\n", ds->read_timeout);
  if (ds->write_timeout) printf("\tWRITE_TIMEOUT=%d\n", ds->write_timeout);
  if (ds->can_handle_exp_pwd) printf("\tCAN_HANDLE_EXP_PWD\n");
  if (ds->enable_cleartext_plugin) printf("\tENABLE_CLEARTEXT_PLUGIN\n");
  if (ds->get_server_public_key) printf("\tGET_SERVER_PUBLIC_KEY\n");
  if (ds->enable_dns_srv) printf("\tENABLE_DNS_SRV\n");
  if (ds->multi_host) printf("\tMULTI_HOST\n");
  /* Failover */
  if (ds->enable_cluster_failover) printf("\tENABLE_CLUSTER_FAILOVER\n");
  if (ds->allow_reader_connections) printf("\tALLOW_READER_CONNECTIONS\n");
  if (ds->gather_perf_metrics) printf("\tGATHER_PERF_METRICS\n");
  if (ds->gather_metrics_per_instance) printf("\tGATHER_METRICS_PER_INSTANCE\n");
  if (ds->topology_refresh_rate) printf("\tTOPOLOGY_REFRESH_RATE=%d\n", ds->topology_refresh_rate);
  if (ds->failover_timeout) printf("\tFAILOVER_TIMEOUT=%d\n", ds->failover_timeout);
  if (ds->failover_topology_refresh_rate) printf("\tFAILOVER_TOPOLOGY_REFRESH_RATE=%d\n", ds->failover_topology_refresh_rate);
  if (ds->failover_writer_reconnect_interval) printf("\tFAILOVER_WRITER_RECONNECT_INTERVAL=%d\n", ds->failover_writer_reconnect_interval);
  if (ds->failover_reader_connect_timeout) printf("\tFAILOVER_READER_CONNECT_TIMEOUT=%d\n", ds->failover_reader_connect_timeout);
  if (ds->connect_timeout) printf("\tCONNECT_TIMEOUT=%d\n", ds->connect_timeout);
  if (ds->network_timeout) printf("\tNETWORK_TIMEOUT=%d\n", ds->network_timeout);
  /* Monitoring */
  if (ds->enable_failure_detection) printf("\tENABLE_FAILURE_DETECTION\n");
  if (ds->failure_detection_time) printf("\tFAILURE_DETECTION_TIME=%d\n", ds->failure_detection_time);
  if (ds->failure_detection_timeout) printf("\tFAILURE_DETECTION_TIMEOUT=%d\n", ds->failure_detection_timeout);
  if (ds->failure_detection_interval) printf("\tFAILURE_DETECTION_INTERVAL=%d\n", ds->failure_detection_interval);
  if (ds->failure_detection_count) printf("\tFAILURE_DETECTION_COUNT=%d\n", ds->failure_detection_count);
  if (ds->monitor_disposal_time) printf("\tMONITOR_DISPOSAL_TIME=%d\n", ds->monitor_disposal_time);

  return 0;
}


/*
 * Handler for "list data sources" command (-s -l)
 */
int list_datasources()
{
  SQLHANDLE env;
  SQLRETURN rc;
  SQLUSMALLINT dir= 0; /* SQLDataSources fetch direction */
  SQLCHAR server_name[256];
  SQLCHAR description[256];

  /* determine 'direction' to pass to SQLDataSources */
  switch (scope)
  {
  case ODBC_BOTH_DSN:
    dir= SQL_FETCH_FIRST;
    break;
  case ODBC_USER_DSN:
    dir= SQL_FETCH_FIRST_USER;
    break;
  case ODBC_SYSTEM_DSN:
    dir= SQL_FETCH_FIRST_SYSTEM;
    break;
  }

  /* allocate handle and set ODBC API v3 */
  if ((rc= SQLAllocHandle(SQL_HANDLE_ENV, SQL_NULL_HANDLE,
                          &env)) != SQL_SUCCESS)
  {
    fprintf(stderr, "[ERROR] Failed to allocate env handle\n");
    return 1;
  }

  if ((rc= SQLSetEnvAttr(env, SQL_ATTR_ODBC_VERSION, (SQLPOINTER)SQL_OV_ODBC3,
                         SQL_IS_INTEGER)) != SQL_SUCCESS)
  {
    print_odbc_error(env, SQL_HANDLE_ENV);
    SQLFreeHandle(SQL_HANDLE_ENV, env);
    return 1;
  }

  /* retrieve and print data source */
  while ((rc= SQLDataSources(env, dir, server_name, 256, NULL, description,
                             256, NULL)) == SQL_SUCCESS)
  {
    printf("%-20s - %s\n", server_name, description);
    dir= SQL_FETCH_NEXT;
  }

  if (rc != SQL_NO_DATA)
  {
    print_odbc_error(env, SQL_HANDLE_ENV);
    SQLFreeHandle(SQL_HANDLE_ENV, env);
    return 1;
  }

  SQLFreeHandle(SQL_HANDLE_ENV, env);

  return 0;
}


/*
 * Handler for "add data source" command (-s -a -n drivername -t attrs)
 */
int add_datasource(DataSource *ds, const SQLWCHAR *attrs)
{
  /* read datasource object from attributes */
  if (ds_from_kvpair(ds, attrs, ';'))
  {
    fprintf(stderr,
            "[ERROR] Could not parse key-value pair attribute string\n");
    return 1;
  }

  /* validate */
  if (!ds->driver)
  {
    fprintf(stderr, "[ERROR] Driver must be specified for a data source\n");
    return 1;
  }

  /* Add it */
  if (ds_add(ds))
  {
    print_installer_error();
    fprintf(stderr,
            "[ERROR] Data source entry failed, remove or try again\n");
    return 1;
  }

  printf("Success\n");
  return 0;
}


/*
 * Handler for "remove data source" command (-s -r -n drivername)
 */
int remove_datasource(DataSource *ds)
{
  /*
     First check that it exists, because SQLRemoveDSNFromIni
     returns true, even if it doesn't.
   */
  if (ds_exists(ds->name))
  {
    fprintf(stderr, "[ERROR] Data source doesn't exist\n");
    return 1;
  }

  if (SQLRemoveDSNFromIniW(ds->name) != TRUE)
  {
    print_installer_error();
    return 1;
  }

  printf("Success\n");

  return 0;
}


/*
 * Handle driver actions. We setup a data source object to be used
 * for all actions. Config/scope set+restore is wrapped around the
 * action.
 */
int handle_datasource_action()
{
  int rc= 0;
  UWORD config_mode= config_set(scope);
  DataSource *ds= ds_new();

  /* check name is given if needed */
  switch (action)
  {
  case ACTION_ADD:
  case ACTION_REMOVE:
    if (!name)
    {
      fprintf(stderr, "[ERROR] Name missing to add/remove data source\n");
      rc= 1;
      goto end;
    }
  }

  /* set name if given */
  if (name)
    ds_set_wstrattr(&ds->name, wname);

  /* perform given action */
  switch (action)
  {
  case ACTION_LIST:
    if (name)
      rc= list_datasource_details(ds);
    else
      rc= list_datasources();
    break;
  case ACTION_ADD:
    if (scope == ODBC_BOTH_DSN)
    {
      fprintf(stderr, "[ERROR] Adding data source must be either "
                      "user or system scope (not both)\n");
      rc= 1;
    }
    else if (!attrstr)
    {
      fprintf(stderr, "[ERROR] Attribute string missing to add data source\n");
      rc= 1;
    }
    else
    {
      rc= add_datasource(ds, wattrs);
    }
    break;
  case ACTION_REMOVE:
    rc= remove_datasource(ds);
    break;
  }

end:
  ds_delete(ds);
  config_set(config_mode);
  return rc;
}


/*
 * Entry point, parse args and do first set of validation.
 */
int main(int argc, char **argv)
{
  char *arg;
  int i;
  SQLINTEGER convlen;

  /* minimum number of args to do anything useful */
  if (argc < 3)
  {
    if (argc == 2)
    {
      arg= argv[1]; /* check for help option */
      if (*arg == '-' && *(arg + 1) == ACTION_HELP)
      {
        main_usage(stdout);
        return 1;
      }
    }

    fprintf(stderr, "[ERROR] Not enough arguments given\n");
    main_usage(stderr);
    return 1;
  }

  /* parse args */
  for(i= 1; i < argc; ++i)
  {
    arg= argv[i];
    if (*arg == '-')
      arg++;

    /* we should be left with a single character option (except scope)
     * all strings are skipped by the respective option below */
    if (*arg != OPT_SCOPE && *(arg + 1))
    {
      fprintf(stderr, "[ERROR] Invalid command line option: %s\n", arg);
      return 1;
    }

    switch (*arg)
    {
    case OBJ_DRIVER:
    case OBJ_DATASOURCE:
      /* make sure we haven't already assigned the object */
      if (obj)
      {
        object_usage();
        return 1;
      }
      obj= *arg;
      break;
    case ACTION_LIST:
    case ACTION_ADD:
    case ACTION_REMOVE:
      if (action)
      {
        action_usage();
        return 1;
      }
      action= *arg;
      break;
    case OPT_NAME:
      if (i + 1 == argc || *argv[i + 1] == '-')
      {
        fprintf(stderr, "[ERROR] Missing name\n");
        return 1;
      }
      name= argv[++i];
      break;
    case OPT_ATTR:
      if (i + 1 == argc || *argv[i + 1] == '-')
      {
        fprintf(stderr, "[ERROR] Missing attribute string\n");
        return 1;
      }
      attrstr= argv[++i];
      break;
    case OPT_SCOPE:
      /* convert to integer */
      scope= *(++arg) - '0';
      if (scope > 2 || *(arg + 1) /* another char exists */)
      {
        fprintf(stderr, "[ERROR] Invalid scope: %s\n", arg);
        return 1;
      }
      break;
    case ACTION_HELP:
      /* print usage if -h is given anywhere */
      main_usage(stdout);
      return 1;
    default:
      fprintf(stderr, "[ERROR] Invalid command line option: %s\n", arg);
      return 1;
    }
  }

  if (!action)
  {
    action_usage();
    return 1;
  }

  /* init libmysqlclient */
  my_sys_init();
  utf8_charset_info= get_charset_by_csname(transport_charset, MYF(MY_CS_PRIMARY),
                                           MYF(0));

  /* convert to SQLWCHAR for installer API */
  convlen= SQL_NTS;
  if (name && !(wname= sqlchar_as_sqlwchar(default_charset_info,
                                           (SQLCHAR *)name, &convlen, NULL)))
  {
    fprintf(stderr, "[ERROR] Name is invalid\n");
    return 1;
  }

  convlen= SQL_NTS;
  if (attrstr && !(wattrs= sqlchar_as_sqlwchar(default_charset_info,
                                               (SQLCHAR *)attrstr, &convlen,
                                               NULL)))
  {
    fprintf(stderr, "[ERROR] Attribute string is invalid\n");
    return 1;
  }

  /* handle appropriate action */
  switch (obj)
  {
  case OBJ_DRIVER:
    return handle_driver_action();
  case OBJ_DATASOURCE:
    return handle_datasource_action();
  default:
    object_usage();
    return 1;
  }
}

