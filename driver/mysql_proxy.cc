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

#include "driver.h"

#include <sstream>

namespace {
    const char* RETRIEVE_HOST_PORT_SQL = "SELECT CONCAT(@@hostname, ':', @@port)";
    const auto SOCKET_CLOSE_DELAY = std::chrono::milliseconds(100);
}

MYSQL_PROXY::MYSQL_PROXY(DBC* dbc, DataSource* ds)
    : MYSQL_PROXY(dbc, ds, std::make_shared<MONITOR_SERVICE>(ds && ds->save_queries)) {}

MYSQL_PROXY::MYSQL_PROXY(DBC* dbc, DataSource* ds, std::shared_ptr<MONITOR_SERVICE> monitor_service) : dbc{dbc}, ds{ds} {
    if (!this->dbc) {
        throw std::runtime_error("DBC cannot be null.");
    }

    if (!this->ds) {
        throw std::runtime_error("DataSource cannot be null.");
    }

    if (this->ds->enable_failure_detection) {
        this->monitor_service = std::move(monitor_service);
    }

    this->host = get_host_info_from_ds(ds);
    generate_node_keys();
}

MYSQL_PROXY::~MYSQL_PROXY() {
    if (this->mysql) {
        close();
    }
}

void MYSQL_PROXY::delete_ds() {
    if (ds) {
        ds_delete(ds);
        ds = nullptr;
    }
}

uint64_t MYSQL_PROXY::num_rows(MYSQL_RES* res) {
    return mysql_num_rows(res);
}

unsigned int MYSQL_PROXY::num_fields(MYSQL_RES* res) {
    return mysql_num_fields(res);
}

MYSQL_FIELD* MYSQL_PROXY::fetch_field_direct(MYSQL_RES* res, unsigned int fieldnr) {
    return mysql_fetch_field_direct(res, fieldnr);
}

MYSQL_ROW_OFFSET MYSQL_PROXY::row_tell(MYSQL_RES* res) {
    return mysql_row_tell(res);
}

unsigned int MYSQL_PROXY::field_count() {
    return mysql_field_count(mysql);
}

uint64_t MYSQL_PROXY::affected_rows() {
    return mysql_affected_rows(mysql);
}

unsigned int MYSQL_PROXY::error_code() {
    return mysql_errno(mysql);
}

const char* MYSQL_PROXY::error() {
    return mysql_error(mysql);
}

const char* MYSQL_PROXY::sqlstate() {
    return mysql_sqlstate(mysql);
}

unsigned long MYSQL_PROXY::thread_id() {
    return mysql_thread_id(mysql);
}

int MYSQL_PROXY::set_character_set(const char* csname) {
    const auto context = start_monitoring();
    const int ret = mysql_set_character_set(mysql, csname);
    stop_monitoring(context);
    return ret;
}

void MYSQL_PROXY::init() {
    const auto context = start_monitoring();
    this->mysql = mysql_init(nullptr);
    stop_monitoring(context);
}

bool MYSQL_PROXY::ssl_set(const char* key, const char* cert, const char* ca, const char* capath, const char* cipher) {
    const auto context = start_monitoring();
    const bool ret = mysql_ssl_set(mysql, key, cert, ca, capath, cipher);
    stop_monitoring(context);
    return ret;
}

bool MYSQL_PROXY::change_user(const char* user, const char* passwd, const char* db) {
    const auto context = start_monitoring();
    const bool ret = mysql_change_user(mysql, user, passwd, db);
    stop_monitoring(context);
    return ret;
}

bool MYSQL_PROXY::real_connect(
    const char* host, const char* user, const char* passwd,
    const char* db, unsigned int port, const char* unix_socket,
    unsigned long clientflag) {

    const auto context = start_monitoring();
    const MYSQL* new_mysql = mysql_real_connect(mysql, host, user, passwd, db, port, unix_socket, clientflag);
    stop_monitoring(context);
    return new_mysql != nullptr;
}

int MYSQL_PROXY::select_db(const char* db) {
    const auto context = start_monitoring();
    const int ret = mysql_select_db(mysql, db);
    stop_monitoring(context);
    return ret;
}

int MYSQL_PROXY::query(const char* q) {
    const auto context = start_monitoring();
    const int ret = mysql_query(mysql, q);
    stop_monitoring(context);
    return ret;
}

int MYSQL_PROXY::real_query(const char* q, unsigned long length) {
    const auto context = start_monitoring();
    const int ret = mysql_real_query(mysql, q, length);
    stop_monitoring(context);
    return ret;
}

MYSQL_RES* MYSQL_PROXY::store_result() {
    const auto context = start_monitoring();
    MYSQL_RES* ret = mysql_store_result(mysql);
    stop_monitoring(context);
    return ret;
}

MYSQL_RES* MYSQL_PROXY::use_result() {
    const auto context = start_monitoring();
    MYSQL_RES* ret = mysql_use_result(mysql);
    stop_monitoring(context);
    return ret;
}

struct CHARSET_INFO* MYSQL_PROXY::get_character_set() const {
    return this->mysql->charset;
}

void MYSQL_PROXY::get_character_set_info(MY_CHARSET_INFO* charset) {
    mysql_get_character_set_info(mysql, charset);
}

bool MYSQL_PROXY::autocommit(bool auto_mode) {
    const auto context = start_monitoring();
    const bool ret = mysql_autocommit(mysql, auto_mode);
    stop_monitoring(context);
    return ret;
}

int MYSQL_PROXY::next_result() {
    const auto context = start_monitoring();
    const int ret = mysql_next_result(mysql);
    stop_monitoring(context);
    return ret;
}

int MYSQL_PROXY::stmt_next_result(MYSQL_STMT* stmt) {
    const auto context = start_monitoring();
    const int ret = mysql_stmt_next_result(stmt);
    stop_monitoring(context);
    return ret;
}

void MYSQL_PROXY::close() {
    mysql_close(mysql);
    mysql = nullptr;
}

bool MYSQL_PROXY::real_connect_dns_srv(
    const char* dns_srv_name, const char* user,
    const char* passwd, const char* db, unsigned long client_flag) {

    const auto context = start_monitoring();
    const MYSQL* new_mysql = mysql_real_connect_dns_srv(mysql, dns_srv_name, user, passwd, db, client_flag);
    stop_monitoring(context);
    return new_mysql != nullptr;
}

int MYSQL_PROXY::ping() {
    const auto context = start_monitoring();
    const int ret = mysql_ping(mysql);
    stop_monitoring(context);
    return ret;
}

unsigned long MYSQL_PROXY::get_client_version(void) {
    return mysql_get_client_version();
}

int MYSQL_PROXY::get_option(mysql_option option, const void* arg) {
    return mysql_get_option(mysql, option, arg);
}

int MYSQL_PROXY::options4(mysql_option option, const void* arg1, const void* arg2) {
    const auto context = start_monitoring();
    const int ret = mysql_options4(mysql, option, arg1, arg2);
    stop_monitoring(context);
    return ret;
}

int MYSQL_PROXY::options(mysql_option option, const void* arg) {
    const auto context = start_monitoring();
    const int ret = mysql_options(mysql, option, arg);
    stop_monitoring(context);
    return ret;
}

void MYSQL_PROXY::free_result(MYSQL_RES* result) {
    const auto context = start_monitoring();
    mysql_free_result(result);
    stop_monitoring(context);
}

void MYSQL_PROXY::data_seek(MYSQL_RES* result, uint64_t offset) {
    mysql_data_seek(result, offset);
}

MYSQL_ROW_OFFSET MYSQL_PROXY::row_seek(MYSQL_RES* result, MYSQL_ROW_OFFSET offset) {
    return mysql_row_seek(result, offset);
}

MYSQL_FIELD_OFFSET MYSQL_PROXY::field_seek(MYSQL_RES* result, MYSQL_FIELD_OFFSET offset) {
    return mysql_field_seek(result, offset);
}

MYSQL_ROW MYSQL_PROXY::fetch_row(MYSQL_RES* result) {
    const auto context = start_monitoring();
    const MYSQL_ROW ret = mysql_fetch_row(result);
    stop_monitoring(context);
    return ret;
}

unsigned long* MYSQL_PROXY::fetch_lengths(MYSQL_RES* result) {
    return mysql_fetch_lengths(result);
}

MYSQL_FIELD* MYSQL_PROXY::fetch_field(MYSQL_RES* result) {
    return mysql_fetch_field(result);
}

MYSQL_RES* MYSQL_PROXY::list_fields(const char* table, const char* wild) {
    const auto context = start_monitoring();
    MYSQL_RES* ret = mysql_list_fields(mysql, table, wild);
    stop_monitoring(context);
    return ret;
}

unsigned long MYSQL_PROXY::real_escape_string(char* to, const char* from, unsigned long length) {
    const auto context = start_monitoring();
    const unsigned long ret = mysql_real_escape_string(mysql, to, from, length);
    stop_monitoring(context);
    return ret;
}

bool MYSQL_PROXY::bind_param(unsigned n_params, MYSQL_BIND* binds, const char** names) {
    const auto context = start_monitoring();
    const bool ret = mysql_bind_param(mysql, n_params, binds, names);
    stop_monitoring(context);
    return ret;
}

MYSQL_STMT* MYSQL_PROXY::stmt_init() {
    const auto context = start_monitoring();
    MYSQL_STMT* ret = mysql_stmt_init(mysql);
    stop_monitoring(context);
    return ret;
}

int MYSQL_PROXY::stmt_prepare(MYSQL_STMT* stmt, const char* query, unsigned long length) {
    const auto context = start_monitoring();
    const int ret = mysql_stmt_prepare(stmt, query, length);
    stop_monitoring(context);
    return ret;
}

int MYSQL_PROXY::stmt_execute(MYSQL_STMT* stmt) {
    const auto context = start_monitoring();
    const int ret = mysql_stmt_execute(stmt);
    stop_monitoring(context);
    return ret;
}

int MYSQL_PROXY::stmt_fetch(MYSQL_STMT* stmt) {
    const auto context = start_monitoring();
    const int ret = mysql_stmt_fetch(stmt);
    stop_monitoring(context);
    return ret;
}

int MYSQL_PROXY::stmt_fetch_column(MYSQL_STMT* stmt, MYSQL_BIND* bind_arg, unsigned int column, unsigned long offset) {
    const auto context = start_monitoring();
    const int ret = mysql_stmt_fetch_column(stmt, bind_arg, column, offset);
    stop_monitoring(context);
    return ret;
}

int MYSQL_PROXY::stmt_store_result(MYSQL_STMT* stmt) {
    const auto context = start_monitoring();
    const int ret = mysql_stmt_store_result(stmt);
    stop_monitoring(context);
    return ret;
}

bool MYSQL_PROXY::stmt_bind_param(MYSQL_STMT* stmt, MYSQL_BIND* bnd) {
    const auto context = start_monitoring();
    const bool ret = mysql_stmt_bind_param(stmt, bnd);
    stop_monitoring(context);
    return ret;
}

bool MYSQL_PROXY::stmt_bind_result(MYSQL_STMT* stmt, MYSQL_BIND* bnd) {
    const auto context = start_monitoring();
    const bool ret = mysql_stmt_bind_result(stmt, bnd);
    stop_monitoring(context);
    return ret;
}

bool MYSQL_PROXY::stmt_close(MYSQL_STMT* stmt) {
    const auto context = start_monitoring();
    const bool ret = mysql_stmt_close(stmt);
    stop_monitoring(context);
    return ret;
}

bool MYSQL_PROXY::stmt_reset(MYSQL_STMT* stmt) {
    const auto context = start_monitoring();
    const bool ret = mysql_stmt_reset(stmt);
    stop_monitoring(context);
    return ret;
}

bool MYSQL_PROXY::stmt_free_result(MYSQL_STMT* stmt) {
    const auto context = start_monitoring();
    const bool ret = mysql_stmt_free_result(stmt);
    stop_monitoring(context);
    return ret;
}

bool MYSQL_PROXY::stmt_send_long_data(MYSQL_STMT* stmt, unsigned int param_number, const char* data,
                                      unsigned long length) {

    const auto context = start_monitoring();
    const bool ret = mysql_stmt_send_long_data(stmt, param_number, data, length);
    stop_monitoring(context);
    return ret;
}

MYSQL_RES* MYSQL_PROXY::stmt_result_metadata(MYSQL_STMT* stmt) {
    const auto context = start_monitoring();
    MYSQL_RES* ret = mysql_stmt_result_metadata(stmt);
    stop_monitoring(context);
    return ret;
}

unsigned int MYSQL_PROXY::stmt_errno(MYSQL_STMT* stmt) {
    return mysql_stmt_errno(stmt);
}

const char* MYSQL_PROXY::stmt_error(MYSQL_STMT* stmt) {
    return mysql_stmt_error(stmt);
}

MYSQL_ROW_OFFSET MYSQL_PROXY::stmt_row_seek(MYSQL_STMT* stmt, MYSQL_ROW_OFFSET offset) {
    return mysql_stmt_row_seek(stmt, offset);
}

MYSQL_ROW_OFFSET MYSQL_PROXY::stmt_row_tell(MYSQL_STMT* stmt) {
    return mysql_stmt_row_tell(stmt);
}

void MYSQL_PROXY::stmt_data_seek(MYSQL_STMT* stmt, uint64_t offset) {
    mysql_stmt_data_seek(stmt, offset);
}

uint64_t MYSQL_PROXY::stmt_num_rows(MYSQL_STMT* stmt) {
    return mysql_stmt_num_rows(stmt);
}

uint64_t MYSQL_PROXY::stmt_affected_rows(MYSQL_STMT* stmt) {
    return mysql_stmt_affected_rows(stmt);
}

unsigned int MYSQL_PROXY::stmt_field_count(MYSQL_STMT* stmt) {
    return mysql_stmt_field_count(stmt);
}

st_mysql_client_plugin* MYSQL_PROXY::client_find_plugin(const char* name, int type) {
    const auto context = start_monitoring();
    st_mysql_client_plugin* ret = mysql_client_find_plugin(mysql, name, type);
    stop_monitoring(context);
    return ret;
}

bool MYSQL_PROXY::is_connected() {
    return this->mysql != nullptr && this->mysql->net.vio;
}

void MYSQL_PROXY::set_last_error_code(unsigned int error_code) {
    this->mysql->net.last_errno = error_code;
}

char* MYSQL_PROXY::get_last_error() const {
    return this->mysql->net.last_error;
}

unsigned int MYSQL_PROXY::get_last_error_code() const {
    return this->mysql->net.last_errno;
}

char* MYSQL_PROXY::get_sqlstate() const {
    return this->mysql->net.sqlstate;
}

char* MYSQL_PROXY::get_server_version() const {
    return this->mysql->server_version;
}

uint64_t MYSQL_PROXY::get_affected_rows() const {
    return this->mysql->affected_rows;
}

void MYSQL_PROXY::set_affected_rows(uint64_t num_rows) {
    this->mysql->affected_rows = num_rows;
}

char* MYSQL_PROXY::get_host_info() const {
    return this->mysql->host_info;
}

std::string MYSQL_PROXY::get_host() {
    return (this->mysql && this->mysql->host) ? this->mysql->host : this->host->get_host();
}

unsigned int MYSQL_PROXY::get_port() {
    return (this->mysql) ? this->mysql->port : this->host->get_port();
}

unsigned long MYSQL_PROXY::get_max_packet() const {
    return this->mysql->net.max_packet;
}

unsigned long MYSQL_PROXY::get_server_capabilities() const {
    return this->mysql->server_capabilities;
}

unsigned int MYSQL_PROXY::get_server_status() const {
    return this->mysql->server_status;
}

void MYSQL_PROXY::set_connection(MYSQL_PROXY* mysql_proxy) {
    close();
    this->mysql = mysql_proxy->mysql;
    mysql_proxy->mysql = nullptr;

    ds_delete(mysql_proxy->ds);
    delete mysql_proxy;

    if (monitor_service != nullptr && !node_keys.empty()) {
        monitor_service->stop_monitoring_for_all_connections(node_keys);
    }
    generate_node_keys();
}

void MYSQL_PROXY::close_socket() {
    MYLOG_DBC_TRACE(dbc, "Closing socket");
    int ret = 0;
    if (mysql->net.fd != INVALID_SOCKET && (ret = shutdown(mysql->net.fd, SHUT_RDWR))) {
        MYLOG_DBC_TRACE(dbc, "shutdown() with return code: %d, error message: %s,", ret, strerror(socket_errno));
    }
    // Yield to main thread to handle socket shutdown
    std::this_thread::sleep_for(SOCKET_CLOSE_DELAY);
    if (mysql->net.fd != INVALID_SOCKET && (ret = ::closesocket(mysql->net.fd))) {
        MYLOG_DBC_TRACE(dbc, "closesocket() with return code: %d, error message: %s,", ret, strerror(socket_errno));
    }
}

std::shared_ptr<MONITOR_CONNECTION_CONTEXT> MYSQL_PROXY::start_monitoring() {
    if (!ds || !ds->enable_failure_detection) {
        return nullptr;
    }

    
    auto failure_detection_timeout = ds->failure_detection_timeout;
    // Use network timeout defined if failure detection timeout is not set
    if (failure_detection_timeout == 0) {
        failure_detection_timeout = ds->network_timeout == 0 ? failure_detection_timeout_default : ds->network_timeout;
    }
    
    return monitor_service->start_monitoring(
        dbc,
        ds,
        node_keys,
        std::make_shared<HOST_INFO>(get_host(), get_port()),
        std::chrono::milliseconds{ds->failure_detection_time},
        std::chrono::seconds{failure_detection_timeout},
        std::chrono::milliseconds{ds->failure_detection_interval},
        ds->failure_detection_count,
        std::chrono::milliseconds{ds->monitor_disposal_time});
}

void MYSQL_PROXY::stop_monitoring(std::shared_ptr<MONITOR_CONNECTION_CONTEXT> context) {
    if (!ds ||!ds->enable_failure_detection || context == nullptr) {
        return;
    }
    monitor_service->stop_monitoring(context);
    if (context->is_node_unhealthy() && is_connected()) {
        close_socket();
    }
}

void MYSQL_PROXY::generate_node_keys() {
    node_keys.clear();
    node_keys.insert(std::string(get_host()) + ":" + std::to_string(get_port()));

    if (is_connected()) {
        // Temporarily turn off failure detection if on
        const auto failure_detection_old_state = ds->enable_failure_detection;
        ds->enable_failure_detection = false;

        const auto error = query(RETRIEVE_HOST_PORT_SQL);
        if (error == 0) {
            MYSQL_RES* result = store_result();
            MYSQL_ROW row;
            while ((row = fetch_row(result))) {
                node_keys.insert(std::string(row[0]));
            }
            free_result(result);
        }

        ds->enable_failure_detection = failure_detection_old_state;
    }
}

MYSQL_MONITOR_PROXY::MYSQL_MONITOR_PROXY(DataSource* ds) {
    this->ds = ds_new();
    ds_copy(this->ds, ds);
}

MYSQL_MONITOR_PROXY::~MYSQL_MONITOR_PROXY() {
    if (this->mysql) {
        mysql_close(this->mysql);
    }
    if (this->ds) {
        ds_delete(this->ds);
    }
}

void MYSQL_MONITOR_PROXY::init() {
    this->mysql = mysql_init(nullptr);
}

int MYSQL_MONITOR_PROXY::ping() {
    return mysql_ping(mysql);
}

int MYSQL_MONITOR_PROXY::options(enum mysql_option option, const void* arg) {
    return mysql_options(mysql, option, arg);
}

bool MYSQL_MONITOR_PROXY::connect() {
    if (!ds)
        return false;

    const auto host = get_host_info_from_ds(ds);

    return mysql_real_connect(mysql, host->get_host().c_str(), ds_get_utf8attr(ds->uid, &ds->uid8),
                              ds_get_utf8attr(ds->pwd, &ds->pwd8), nullptr, host->get_port(),
                              ds_get_utf8attr(ds->socket, &ds->socket8), 0) != nullptr;
}

bool MYSQL_MONITOR_PROXY::is_connected() {
    return this->mysql != nullptr && this->mysql->net.vio;
}

const char* MYSQL_MONITOR_PROXY::error() {
    return mysql_error(mysql); }

void MYSQL_MONITOR_PROXY::close() {
    mysql_close(mysql);
    mysql= nullptr;
}
