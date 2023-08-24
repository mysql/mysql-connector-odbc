/*
 * Copyright (c) 2008, 2023, Oracle and/or its affiliates. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License, version 2.0, as
 * published by the Free Software Foundation.
 *
 * This program is also distributed with certain software (including
 * but not limited to OpenSSL) that is licensed under separate terms,
 * as designated in a particular file or component or in included license
 * documentation.  The authors of MySQL hereby grant you an
 * additional permission to link the program and your derivative works
 * with the separately licensed software that they have included with
 * MySQL.
 *
 * Without limiting anything contained in the foregoing, this file,
 * which is part of MySQL Connector/C++, is also subject to the
 * Universal FOSS Exception, version 1.0, a copy of which can be found at
 * http://oss.oracle.com/licenses/universal-foss-exception.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License, version 2.0, for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
 */

#include "telemetry.h"
#include <VersionInfo.h>
#include "driver.h"

#include <iostream>
#include <vector>
#include <optional>

#ifdef _WIN32
#include <windows.h>
#else
#include <pthread.h>
#endif

namespace telemetry
{
  Span_ptr mk_span(
    std::string name,
    std::optional<trace::SpanContext> link = {}
  )
  {
    auto tracer = trace::Provider::GetTracerProvider()->GetTracer(
      "MySQL Connector/ODBC " MYODBC_STRDRIVERTYPE, MYODBC_CONN_ATTR_VER
    );

    trace::StartSpanOptions opts;
    opts.kind = trace::SpanKind::kClient;

    auto span
    = link ? tracer->StartSpan(name, {}, {{*link, {}}},  opts)
           : tracer->StartSpan(name, opts);

    span->SetAttribute("db.system", "mysql");
    return span;
  }


  Span_ptr
  Telemetry_base<DBC>::mk_span(DBC *conn, const char*)
  {
    auto span = telemetry::mk_span("connection");

    return span;
  }


  void
  Telemetry_base<DBC>::set_attribs(DBC *dbc, DataSource *ds)
  {
    if (disabled(dbc) || !span || !ds)
      return;

    // NOTE: There is no possibility in ODBC for "other" transport
    std::string transport = "other";

    if(ds->opt_SOCKET)
    {
      transport = "pipe";
#ifndef _WIN32
      span->SetAttribute("net.sock.family", "unix");
#endif
    } else {
      transport = "ip_tcp";
    }

    span->SetAttribute("net.transport", transport);
    if (ds->opt_SERVER.is_set())
    {
      span->SetAttribute("net.peer.name", (const char*)ds->opt_SERVER);
    }
    if (ds->opt_PORT.is_set())
    {
      span->SetAttribute("net.peer.port", ds->opt_PORT);
    }
  }


  template<>
  bool
  Telemetry_base<STMT>::disabled(STMT *stmt) const
  {
    return stmt->conn_telemetry().disabled(stmt->dbc);
  }


  /*
    Creating statement span: we link it to the connection span and we also
    set "traceparent" attribute unless user already set it.
  */

  template<>
  Span_ptr
  Telemetry_base<STMT>::mk_span(STMT *stmt, const char* name)
  {
    Span_ptr local_span;
    if (!name)
    {
      /*
        Creating statement span: we link it to the connection span and we also
        set "traceparent" attribute unless user already set it.

        If `name` is not given then this span corresponds to a plain (not prepared)
        statement. Otherwise this is a span for prepared statement prepare or execute
        operation and the name should indicate which operation it is.
      */
      local_span = telemetry::mk_span("SQL statement",
        stmt->conn_telemetry().span->GetContext()
      );

      if (!stmt->query_attr_exists("traceparent"))
      {
        char buf[trace::TraceId::kSize * 2];
        auto ctx = local_span->GetContext();

        ctx.trace_id().ToLowerBase16(buf);
        std::string trace_id{buf, sizeof(buf)};

        ctx.span_id().ToLowerBase16({buf, trace::SpanId::kSize * 2});
        std::string span_id{buf, trace::SpanId::kSize * 2};

        stmt->add_query_attr(
          "traceparent", "00-" + trace_id + "-" + span_id + "-00"
        );
      }
    }
    else
    {
      // For SSPS we give a name indicating whether it is
      // Prepare or Execute.
      local_span = telemetry::mk_span(name,
        stmt->conn_telemetry().span->GetContext()
      );
    }

    local_span->SetAttribute("db.user", (const char*)stmt->dbc->ds.opt_UID);

    return local_span;
  }

}
