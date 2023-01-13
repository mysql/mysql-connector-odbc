// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License, version 2.0
// (GPLv2), as published by the Free Software Foundation, with the
// following additional permissions:
//
// This program is distributed with certain software that is licensed
// under separate terms, as designated in a particular file or component
// or in the license documentation. Without limiting your rights under
// the GPLv2, the authors of this program hereby grant you an additional
// permission to link the program and your derivative works with the
// separately licensed software that they have included with the program.
//
// Without limiting the foregoing grant of rights under the GPLv2 and
// additional permission as to separately licensed software, this
// program is also subject to the Universal FOSS Exception, version 1.0,
// a copy of which can be found along with its FAQ at
// http://oss.oracle.com/licenses/universal-foss-exception.
//
// This program is distributed in the hope that it will be useful, but
// WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
// See the GNU General Public License, version 2.0, for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program. If not, see
// http://www.gnu.org/licenses/gpl-2.0.html.

#ifndef __MYSQL_PROXY__
#define __MYSQL_PROXY__

#include <mysql.h>

#include "monitor_service.h"

struct DBC;
struct DataSource;

class MYSQL_PROXY {
public:
    MYSQL_PROXY(DBC* dbc, DataSource* ds);
    MYSQL_PROXY(DBC* dbc, DataSource* ds, std::shared_ptr<MONITOR_SERVICE> monitor_service);
    virtual ~MYSQL_PROXY();

    void delete_ds();
    uint64_t num_rows(MYSQL_RES* res);
    unsigned int num_fields(MYSQL_RES* res);
    MYSQL_FIELD* fetch_field_direct(MYSQL_RES* res, unsigned int fieldnr);
    MYSQL_ROW_OFFSET row_tell(MYSQL_RES* res);

    unsigned int field_count();
    uint64_t affected_rows();
    unsigned int error_code();
    const char* error();
    const char* sqlstate();
    unsigned long thread_id();
    int set_character_set(const char* csname);

    void init();
    bool ssl_set(const char* key, const char* cert, const char* ca,
                 const char* capath, const char* cipher);
    bool change_user(const char* user, const char* passwd,
                     const char* db);
    bool real_connect(const char* host, const char* user,
                      const char* passwd, const char* db, unsigned int port,
                      const char* unix_socket, unsigned long clientflag);
    int select_db(const char* db);
    virtual int query(const char* q);
    int real_query(const char* q, unsigned long length);
    virtual MYSQL_RES* store_result();
    MYSQL_RES* use_result();
    struct CHARSET_INFO* get_character_set() const;
    void get_character_set_info(MY_CHARSET_INFO* charset);

    virtual int ping();
    static unsigned long get_client_version(void);
    virtual int options(enum mysql_option option, const void* arg);
    int options4(enum mysql_option option, const void* arg1,
                 const void* arg2);
    int get_option(enum mysql_option option, const void* arg);
    virtual void free_result(MYSQL_RES* result);
    void data_seek(MYSQL_RES* result, uint64_t offset);
    MYSQL_ROW_OFFSET row_seek(MYSQL_RES* result, MYSQL_ROW_OFFSET offset);
    MYSQL_FIELD_OFFSET field_seek(MYSQL_RES* result, MYSQL_FIELD_OFFSET offset);
    virtual MYSQL_ROW fetch_row(MYSQL_RES* result);

    unsigned long* fetch_lengths(MYSQL_RES* result);
    MYSQL_FIELD* fetch_field(MYSQL_RES* result);
    MYSQL_RES* list_fields(const char* table, const char* wild);
    unsigned long real_escape_string(char* to, const char* from,
                                     unsigned long length);

    bool bind_param(unsigned n_params, MYSQL_BIND* binds,
                    const char** names);

    MYSQL_STMT* stmt_init();
    int stmt_prepare(MYSQL_STMT* stmt, const char* query, unsigned long length);
    int stmt_execute(MYSQL_STMT* stmt);
    int stmt_fetch(MYSQL_STMT* stmt);
    int stmt_fetch_column(MYSQL_STMT* stmt, MYSQL_BIND* bind_arg,
                          unsigned int column, unsigned long offset);
    int stmt_store_result(MYSQL_STMT* stmt);
    unsigned long stmt_param_count(MYSQL_STMT* stmt);
    bool stmt_bind_param(MYSQL_STMT* stmt, MYSQL_BIND* bnd);
    bool stmt_bind_result(MYSQL_STMT* stmt, MYSQL_BIND* bnd);
    bool stmt_close(MYSQL_STMT* stmt);
    bool stmt_reset(MYSQL_STMT* stmt);
    bool stmt_free_result(MYSQL_STMT* stmt);
    bool stmt_send_long_data(MYSQL_STMT* stmt, unsigned int param_number,
                             const char* data, unsigned long length);
    MYSQL_RES* stmt_result_metadata(MYSQL_STMT* stmt);
    unsigned int stmt_errno(MYSQL_STMT* stmt);
    const char* stmt_error(MYSQL_STMT* stmt);
    MYSQL_ROW_OFFSET stmt_row_seek(MYSQL_STMT* stmt, MYSQL_ROW_OFFSET offset);
    MYSQL_ROW_OFFSET stmt_row_tell(MYSQL_STMT* stmt);
    void stmt_data_seek(MYSQL_STMT* stmt, uint64_t offset);
    uint64_t stmt_num_rows(MYSQL_STMT* stmt);
    uint64_t stmt_affected_rows(MYSQL_STMT* stmt);
    unsigned int stmt_field_count(MYSQL_STMT* stmt);

    bool autocommit(bool auto_mode);
    int next_result();
    int stmt_next_result(MYSQL_STMT* stmt);
    void close();

    bool real_connect_dns_srv(const char* dns_srv_name,
                              const char* user, const char* passwd,
                              const char* db, unsigned long client_flag);
    struct st_mysql_client_plugin* client_find_plugin(
        const char* name, int type);

    virtual bool is_connected();

    void set_last_error_code(unsigned int error_code);

    char* get_last_error() const;

    unsigned int get_last_error_code() const;

    char* get_sqlstate() const;

    char* get_server_version() const;

    uint64_t get_affected_rows() const;

    void set_affected_rows(uint64_t num_rows);

    char* get_host_info() const;

    std::string get_host();

    unsigned int get_port();

    unsigned long get_max_packet() const;

    unsigned long get_server_capabilities() const;

    unsigned int get_server_status() const;

    void set_connection(MYSQL_PROXY* mysql_proxy);

    virtual void close_socket();

protected:
    DBC* dbc = nullptr;
    DataSource* ds = nullptr;
    MYSQL* mysql = nullptr;
    std::shared_ptr<MONITOR_SERVICE> monitor_service = nullptr;
    std::shared_ptr<HOST_INFO> host = nullptr;
    std::set<std::string> node_keys;

    std::shared_ptr<MONITOR_CONNECTION_CONTEXT> start_monitoring();
    void stop_monitoring(std::shared_ptr<MONITOR_CONNECTION_CONTEXT> context);
    void generate_node_keys();
};

class MYSQL_MONITOR_PROXY {
public:
    MYSQL_MONITOR_PROXY(DataSource* ds);
    virtual ~MYSQL_MONITOR_PROXY();

    virtual void init();
    virtual int ping();
    virtual int options(enum mysql_option option, const void* arg);
    virtual bool connect();
    virtual bool is_connected();
    virtual const char* error();
    virtual void close();

private:
    DataSource* ds = nullptr;
    MYSQL* mysql = nullptr;
};

#endif /* __MYSQL_PROXY__ */
