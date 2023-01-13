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
 * Function prototypes and structures for installer-wrapper functionality.
 */

#ifndef _INSTALLER_H
#define _INSTALLER_H

#include "../MYODBC_CONF.h"
#include "../MYODBC_ODBC.h"

#ifdef __cplusplus
extern "C" {
#endif

/* the different modes used when calling MYODBCSetupDataSourceConfig */
#define CONFIG_ADD 1
#define CONFIG_EDIT 2
#define CONFIG_VIEW 3
#define CONFIG_DRIVER_CONNECT 4

UWORD config_get();
UWORD config_set(UWORD mode);

typedef struct {
  SQLWCHAR *name;
  SQLWCHAR *lib;
  SQLWCHAR *setup_lib;

  SQLCHAR *name8;
  SQLCHAR *lib8;
  SQLCHAR *setup_lib8;
} Driver;

/* SQL_MAX_OPTION_STRING_LENGTH = 256, should be ok */
#define ODBCDRIVER_STRLEN SQL_MAX_OPTION_STRING_LENGTH
#define ODBCDATASOURCE_STRLEN SQL_MAX_OPTION_STRING_LENGTH

// Failover default settings
#define TOPOLOGY_REFRESH_RATE_MS 30000
#define FAILOVER_TOPOLOGY_REFRESH_RATE_MS 5000
#define FAILOVER_TIMEOUT_MS 60000
#define FAILOVER_READER_CONNECT_TIMEOUT_MS 30000
#define FAILOVER_WRITER_RECONNECT_INTERVAL_MS 5000

// Monitoring default settings
#define FAILURE_DETECTION_TIME_MS 30000
#define FAILURE_DETECTION_INTERVAL_MS 5000
#define FAILURE_DETECTION_COUNT 3
#define MONITOR_DISPOSAL_TIME_MS 60000
#define FAILURE_DETECTION_TIMEOUT_SECS 5

// Default timeout settings
#define DEFAULT_CONNECT_TIMEOUT_SECS 30
#define DEFAULT_NETWORK_TIMEOUT_SECS 30

unsigned int get_connect_timeout(unsigned int seconds);
unsigned int get_network_timeout(unsigned int seconds);

Driver *driver_new();
void driver_delete(Driver *driver);
int driver_lookup_name(Driver *driver);
int driver_lookup(Driver *driver);
int driver_from_kvpair_semicolon(Driver *driver, const SQLWCHAR *attrs);
int driver_to_kvpair_null(Driver *driver, SQLWCHAR *attrs, size_t attrslen);

typedef struct DataSource {
  SQLWCHAR *name;
  SQLWCHAR *driver; /* driver filename */
  SQLWCHAR *description;
  SQLWCHAR *server;
  SQLWCHAR *uid;
  SQLWCHAR *pwd;
#if MFA_ENABLED
  SQLWCHAR *pwd1;
  SQLWCHAR *pwd2;
  SQLWCHAR *pwd3;
#endif
  SQLWCHAR *database;
  SQLWCHAR *socket;
  SQLWCHAR *initstmt;
  SQLWCHAR *charset;
  SQLWCHAR *sslkey;
  SQLWCHAR *sslcert;
  SQLWCHAR *sslca;
  SQLWCHAR *sslcapath;
  SQLWCHAR *sslcipher;
  SQLWCHAR *sslmode;
  SQLWCHAR *rsakey;
  SQLWCHAR *savefile;
  SQLWCHAR *plugin_dir;
  SQLWCHAR *default_auth;
  SQLWCHAR *load_data_local_dir;
  SQLWCHAR *oci_config_file;
  SQLWCHAR *tls_versions;
  SQLWCHAR *ssl_crl;
  SQLWCHAR *ssl_crlpath;

  bool has_port;
  unsigned int port;
  unsigned int read_timeout;
  unsigned int write_timeout;
  unsigned int client_interactive;

  SQLCHAR *name8;
  SQLCHAR *driver8;
  SQLCHAR *description8;
  SQLCHAR *server8;
  SQLCHAR *uid8;
  SQLCHAR *pwd8;
#if MFA_ENABLED
  SQLCHAR *pwd18;
  SQLCHAR *pwd28;
  SQLCHAR *pwd38;
#endif
  SQLCHAR *database8;
  SQLCHAR *socket8;
  SQLCHAR *initstmt8;
  SQLCHAR *charset8;
  SQLCHAR *sslkey8;
  SQLCHAR *sslcert8;
  SQLCHAR *sslca8;
  SQLCHAR *sslcapath8;
  SQLCHAR *sslcipher8;
  SQLCHAR *sslmode8;
  SQLCHAR *rsakey8;
  SQLCHAR *savefile8;
  SQLCHAR *plugin_dir8;
  SQLCHAR *default_auth8;
  SQLCHAR *load_data_local_dir8;
  SQLCHAR *oci_config_file8;
  SQLCHAR *tls_versions8;
  SQLCHAR* ssl_crl8;
  SQLCHAR* ssl_crlpath8;

  /*  */
  BOOL return_matching_rows;
  BOOL allow_big_results;
  BOOL use_compressed_protocol;
  BOOL change_bigint_columns_to_int;
  BOOL safe;
  BOOL auto_reconnect;
  BOOL auto_increment_null_search;
  BOOL handle_binary_as_char;
  BOOL can_handle_exp_pwd;
  BOOL enable_cleartext_plugin;
  BOOL get_server_public_key;
  /*  */
  BOOL dont_prompt_upon_connect;
  BOOL dynamic_cursor;
  BOOL user_manager_cursor;
  BOOL dont_use_set_locale;
  BOOL pad_char_to_full_length;
  BOOL dont_cache_result;
  /*  */
  BOOL return_table_names_for_SqlDescribeCol;
  BOOL ignore_space_after_function_names;
  BOOL force_use_of_named_pipes;
  BOOL no_catalog;
  BOOL no_schema;
  BOOL read_options_from_mycnf;
  BOOL disable_transactions;
  BOOL force_use_of_forward_only_cursors;
  BOOL allow_multiple_statements;
  BOOL limit_column_size;

  BOOL min_date_to_zero;
  BOOL zero_date_to_min;
  BOOL default_bigint_bind_str;
  /* debug */
  BOOL save_queries;
  /* SSL */
  unsigned int sslverify;
  unsigned int cursor_prefetch_number;
  BOOL no_ssps;

  BOOL no_tls_1_2;
  BOOL no_tls_1_3;

  BOOL no_date_overflow;
  BOOL enable_local_infile;

  BOOL enable_dns_srv;
  BOOL multi_host;

  /* Failover */
  BOOL enable_cluster_failover;
  BOOL allow_reader_connections;
  BOOL gather_perf_metrics;
  BOOL gather_metrics_per_instance;
  SQLCHAR *host_pattern8;
  SQLCHAR *cluster_id8;
  SQLWCHAR *host_pattern;
  SQLWCHAR *cluster_id;
  unsigned int topology_refresh_rate;
  unsigned int failover_timeout;
  unsigned int failover_topology_refresh_rate;
  unsigned int failover_writer_reconnect_interval;
  unsigned int failover_reader_connect_timeout;
  unsigned int connect_timeout;
  unsigned int network_timeout;

  /* Monitoring */
  BOOL enable_failure_detection;
  unsigned int failure_detection_time;
  unsigned int failure_detection_interval;
  unsigned int failure_detection_count;
  unsigned int failure_detection_timeout;
  unsigned int monitor_disposal_time;

} DataSource;

/* perhaps that is a good idea to have const ds object with defaults */
extern const unsigned int default_cursor_prefetch;

typedef struct{
  SQLCHAR *type_name;
  SQLINTEGER name_length;
  SQLSMALLINT sql_type;
  SQLSMALLINT mysql_type;
  SQLUINTEGER type_length;
  BOOL binary;
}SQLTypeMap;

#define TYPE_MAP_SIZE 32

DataSource *ds_new();
void ds_delete(DataSource *ds);
int ds_set_strattr(SQLCHAR** attr, const SQLCHAR* val);
int ds_set_wstrattr(SQLWCHAR **attr, const SQLWCHAR *val);
int ds_set_strnattr(SQLCHAR** attr, const SQLCHAR* val, size_t charcount);
int ds_set_wstrnattr(SQLWCHAR **attr, const SQLWCHAR *val, size_t charcount);
int ds_lookup(DataSource *ds);
int ds_from_kvpair(DataSource *ds, const SQLWCHAR *attrs, SQLWCHAR delim);
int ds_add(DataSource *ds);
int ds_exists(SQLWCHAR *name);
char *ds_get_utf8attr(SQLWCHAR *attrw, SQLCHAR **attr8);
int ds_setattr_from_utf8(SQLWCHAR **attr, SQLCHAR *val8);
void ds_set_options(DataSource *ds, ulong options);
ulong ds_get_options(DataSource *ds);
void ds_copy(DataSource *ds, DataSource *ds_source);

extern const SQLWCHAR W_DRIVER_PARAM[];
extern const SQLWCHAR W_DRIVER_NAME[];
extern const SQLWCHAR W_INVALID_ATTR_STR[];

/*
 * Deprecated connection parameters
 */
#define FLAG_FOUND_ROWS		2   /* Access can't handle affected_rows */
#define FLAG_BIG_PACKETS	8   /* Allow BIG packets. */
#define FLAG_NO_PROMPT		16  /* Don't prompt on connection */
#define FLAG_DYNAMIC_CURSOR	32  /* Enables the dynamic cursor */
#define FLAG_NO_DEFAULT_CURSOR	128 /* No default cursor */
#define FLAG_NO_LOCALE		256  /* No locale specification */
#define FLAG_PAD_SPACE		512  /* Pad CHAR:s with space to max length */
#define FLAG_FULL_COLUMN_NAMES	1024 /* Extends SQLDescribeCol */
#define FLAG_COMPRESSED_PROTO	2048 /* Use compressed protocol */
#define FLAG_IGNORE_SPACE	4096 /* Ignore spaces after function names */
#define FLAG_NAMED_PIPE		8192 /* Force use of named pipes */
#define FLAG_NO_BIGINT		16384	/* Change BIGINT to INT */
#define FLAG_NO_CATALOG		32768	/* No catalog support */
#define FLAG_USE_MYCNF		65536L	/* Read my.cnf at start */
#define FLAG_SAFE		131072L /* Try to be as safe as possible */
#define FLAG_NO_TRANSACTIONS  (FLAG_SAFE << 1) /* Disable transactions */
#define FLAG_LOG_QUERY	      (FLAG_SAFE << 2) /* Query logging, debug */
#define FLAG_NO_CACHE	      (FLAG_SAFE << 3) /* Don't cache the resultset */
 /* Force use of forward-only cursors */
#define FLAG_FORWARD_CURSOR   (FLAG_SAFE << 4)
 /* Force auto-reconnect */
#define FLAG_AUTO_RECONNECT   (FLAG_SAFE << 5)
#define FLAG_AUTO_IS_NULL     (FLAG_SAFE << 6) /* 8388608 Enables SQL_AUTO_IS_NULL */
#define FLAG_ZERO_DATE_TO_MIN (1 << 24) /* Convert XXXX-00-00 date to ODBC min date on results */
#define FLAG_MIN_DATE_TO_ZERO (1 << 25) /* Convert ODBC min date to 0000-00-00 on query */
#define FLAG_MULTI_STATEMENTS (1 << 26) /* Allow multiple statements in a query */
#define FLAG_COLUMN_SIZE_S32 (1 << 27) /* Limit column size to a signed 32-bit value (automatically set for ADO) */
#define FLAG_NO_BINARY_RESULT (1 << 28) /* Disables charset 63 for columns with empty org_table */
/*
  When binding SQL_BIGINT as SQL_C_DEFAULT, treat it as a string
  (automatically set for MS Access) see bug#24535
*/
#define FLAG_DFLT_BIGINT_BIND_STR (1 << 29)
/*
  Use SHOW TABLE STATUS instead Information_Schema DB for table metadata
*/
#define ODBC_SSL_MODE_DISABLED           "DISABLED"
#define ODBC_SSL_MODE_PREFERRED          "PREFERRED"
#define ODBC_SSL_MODE_REQUIRED           "REQUIRED"
#define ODBC_SSL_MODE_VERIFY_CA          "VERIFY_CA"
#define ODBC_SSL_MODE_VERIFY_IDENTITY    "VERIFY_IDENTITY"

#define LPASTE(X) L ## X
#define LSTR(X) LPASTE(X)

#ifdef __cplusplus
}
#endif
typedef std::basic_string<SQLWCHAR, std::char_traits<SQLWCHAR>, std::allocator<SQLWCHAR>> SQLWSTRING;
int ds_to_kvpair(DataSource *ds, SQLWSTRING &attrs, SQLWCHAR delim);

#endif /* _INSTALLER_H */

