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

/**
  @file  failover_connection_handler.c
  @brief Failover connection functions.
*/

#include "driver.h"
#include "failover.h"

#include <codecvt>
#include <locale>

#ifdef __linux__
    sqlwchar_string to_sqlwchar_string(const std::string& src) {
        return std::wstring_convert< std::codecvt_utf8_utf16< char16_t >,
                                     char16_t >{}
            .from_bytes(src);
    }
#else
    sqlwchar_string to_sqlwchar_string(const std::string& src) {
        return std::wstring_convert< std::codecvt_utf8< wchar_t >, wchar_t >{}
            .from_bytes(src);
    }
#endif

FAILOVER_CONNECTION_HANDLER::FAILOVER_CONNECTION_HANDLER(DBC* dbc) : dbc{dbc} {}

FAILOVER_CONNECTION_HANDLER::~FAILOVER_CONNECTION_HANDLER() {}

SQLRETURN FAILOVER_CONNECTION_HANDLER::do_connect(DBC* dbc_ptr, DataSource* ds, bool failover_enabled) {
    return dbc_ptr->connect(ds, failover_enabled);
}

MYSQL_PROXY* FAILOVER_CONNECTION_HANDLER::connect(const std::shared_ptr<HOST_INFO>& host_info) {

    if (dbc == nullptr || dbc->ds == nullptr || host_info == nullptr) {
        return nullptr;
    }

    const auto new_host = to_sqlwchar_string(host_info->get_host());

    DBC* dbc_clone = clone_dbc(dbc);
    ds_set_wstrnattr(&dbc_clone->ds->server, (SQLWCHAR*)new_host.c_str(), new_host.size());

    MYSQL_PROXY* new_connection = nullptr;
    CLEAR_DBC_ERROR(dbc_clone);
    const SQLRETURN rc = do_connect(dbc_clone, dbc_clone->ds, true);

    if (rc == SQL_SUCCESS || rc == SQL_SUCCESS_WITH_INFO) {
        new_connection = dbc_clone->mysql_proxy;
        dbc_clone->mysql_proxy = nullptr;
        dbc_clone->ds = nullptr;
    }

    my_SQLFreeConnect(dbc_clone);

    return new_connection;
}

void FAILOVER_CONNECTION_HANDLER::update_connection(
    MYSQL_PROXY* new_connection, const std::string& new_host_name) {

    if (new_connection->is_connected()) {
        dbc->close();
        dbc->mysql_proxy->set_connection(new_connection);
        
        CLEAR_DBC_ERROR(dbc);

        const sqlwchar_string new_host_name_wstr = to_sqlwchar_string(new_host_name);

        // Update original ds to reflect change in host/server.
        ds_set_wstrnattr(&dbc->ds->server, (SQLWCHAR*)new_host_name_wstr.c_str(), new_host_name_wstr.size());
        ds_set_strnattr(&dbc->ds->server8, (SQLCHAR*)new_host_name.c_str(), new_host_name.size());
    }
}

DBC* FAILOVER_CONNECTION_HANDLER::clone_dbc(DBC* source_dbc) {

    DBC* dbc_clone = nullptr;

    SQLRETURN status = SQL_ERROR;
    if (source_dbc && source_dbc->env) {
        SQLHDBC hdbc;
        SQLHENV henv = static_cast<SQLHANDLE>(source_dbc->env);

        status = my_SQLAllocConnect(henv, &hdbc);
        if (status == SQL_SUCCESS || status == SQL_SUCCESS_WITH_INFO) {
            dbc_clone = static_cast<DBC*>(hdbc);
            dbc_clone->ds = ds_new();
            ds_copy(dbc_clone->ds, source_dbc->ds);
            dbc_clone->mysql_proxy = new MYSQL_PROXY(dbc_clone, dbc_clone->ds);
        } else {
            const char* err = "Cannot allocate connection handle when cloning DBC in writer failover process";
            MYLOG_DBC_TRACE(dbc, err);
            throw std::runtime_error(err);
        }
    }
    return dbc_clone;
}
