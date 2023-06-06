/*
 * Copyright (c) 2012, 2023, Oracle and/or its affiliates.
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


#ifndef _MYSQL_TELEMETRY_H_
#define _MYSQL_TELEMETRY_H_

#include <installer.h>  // ODBC_OTEL_MODE() macro

#ifdef TELEMETRY
#include <string>
#include <opentelemetry/trace/provider.h>
#endif

class STMT;
class DBC;

#define OTEL_ENUM_CONSTANT(N,V)  OTEL_##N = V,

enum OTEL_MODE
{
  ODBC_OTEL_MODE(OTEL_ENUM_CONSTANT)
};


namespace telemetry
{
    /*
      Note: If TELEMETRY flag is not enabled then defines phony classes
      Telemetry_base<X> and Telemetry<X> that do nothing (and should
      be optimized out by the compiler).
    */

#ifndef TELEMETRY

    template<class>
    struct Telemetry_base
    {};

#else

    namespace nostd      = opentelemetry::nostd;
    namespace trace      = opentelemetry::trace;

    using Span_ptr = nostd::shared_ptr<trace::Span>;

    template<class Obj> 
    struct Telemetry_base
    {
      bool disabled(Obj*) const;
    protected:
      Span_ptr mk_span(Obj*);
    };

    template<>
    struct Telemetry_base<DBC>
    {
      using Obj = DBC;

      bool disabled(Obj *) const
      {
        return OTEL_MODE::OTEL_DISABLED == mode;
      }

      enum OTEL_MODE mode = OTEL_MODE::OTEL_PREFERRED;
      void set_mode(OTEL_MODE m)
      {
        mode = m;
      }

    protected:

      Span_ptr mk_span(Obj*);
    };

#endif

    template<class Obj> 
    struct Telemetry
     : public Telemetry_base<Obj>
    {
#ifndef TELEMETRY

      static void span_start(Obj*) {}
      static void span_end(Obj*) {}
      static void set_error(Obj*, std::string) {}

#else
      using Base = Telemetry_base<Obj>;

      Span_ptr span;

      void span_start(Obj *obj)
      {
        if (Base::disabled(obj))
          return;
        span = Base::mk_span(obj);  
      }

      
      void span_end(Obj *obj)
      {
        if (!span)
          return;
        span->End();
        // Destroy span just in case
        Span_ptr sink;
        span.swap(sink);
      }


      void set_error(Obj *obj, std::string msg)
      {
        if (!span || Base::disabled(obj))
          return;
        span->SetStatus(trace::StatusCode::kError, msg);
        // TODO: explain why...
        Span_ptr sink;
        span.swap(sink);       
      }
#endif
    };

} /* namespace telemetry */

#endif /*_MYSQL_URI_H_*/
/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noet sw=4 ts=4 fdm=marker
 * vim<600: noet sw=4 ts=4
 */

