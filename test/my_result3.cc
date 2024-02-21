// Copyright (c) 2023, 2024, Oracle and/or its affiliates.
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License, version 2.0, as
// published by the Free Software Foundation.
//
// This program is designed to work with certain software (including
// but not limited to OpenSSL) that is licensed under separate terms, as
// designated in a particular file or component or in included license
// documentation. The authors of MySQL hereby grant you an additional
// permission to link the program and your derivative works with the
// separately licensed software that they have either included with
// the program or referenced in the documentation.
//
// Without limiting anything contained in the foregoing, this file,
// which is part of Connector/ODBC, is also subject to the
// Universal FOSS Exception, version 1.0, a copy of which can be found at
// https://oss.oracle.com/licenses/universal-foss-exception.
//
// This program is distributed in the hope that it will be useful, but
// WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
// See the GNU General Public License, version 2.0, for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software Foundation, Inc.,
// 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA

#include <string>
#include <vector>

#include "odbc_util.h"
#include "../VersionInfo.h"

// Bug #16590994 - ADO updates fail when recordset is adForwardOnly
// and recordset contains bit data.
//
// NOTE: This bug is repeatable VB6 logic when it generates the
//       UPDATE query based on searchability of the column in a corresponding
//       SELECT. The driver converts BIT(N>1) to _binary and the comparison
//       like " WHERE bit_col = _binary'....'" fails the query with the error
//       message: Truncated incorrect DOUBLE value: "xxxxx".
//
//       Marking BIT(N>1) column as not searcheable will exclude it from
//       being added in WHERE clause by ADO/VB6.
//       To check if column is searchable the user needs to call
//       SQLColAttribute(hstmt, col_number, SQL_DESC_SEARCHABLE, ...);
DECLARE_TEST(t_bug16590994_bit_searchable) {
  try
  {
    odbc::table tab(hstmt, "bit_searchable",
                    "id int, ch char(10), b1 bit(1), b16 bit(16)");
    tab.insert("(3, '1234567890', 1, 0xFF00)");
    odbc::sql(hstmt, "SELECT * FROM " + tab.table_name);
    SQLSMALLINT col_count = 0;
    ok_stmt(hstmt, SQLNumResultCols(hstmt, &col_count));
    is_num(4, col_count);

    SQLLEN expected[] = {
      SQL_PRED_SEARCHABLE, // INT column is searchable
      SQL_PRED_SEARCHABLE, // CHAR column is searchable
      SQL_PRED_SEARCHABLE, // BIT(1) column is searchable
      SQL_PRED_NONE        // BIT(16) column is not searchable
    };

    SQLINTEGER expected_val[] = {
      3, 1234567890, 1, 0xFF00
    };

    ok_stmt(hstmt, SQLFetch(hstmt));

    for (int i = 0; i < col_count; ++i) {

      // Checking searchablility
      SQLLEN searchable = -1;
      ok_stmt(hstmt, SQLColAttribute(hstmt, i + 1, SQL_DESC_SEARCHABLE,
        nullptr, 0, nullptr, &searchable));
      is_num(expected[i], searchable);

      // Checking the data
      SQLINTEGER val = 0;
      ok_stmt(hstmt, SQLGetData(hstmt, i + 1, SQL_C_LONG, &val, 0, nullptr));
      is_num(expected_val[i], val);
    }

    is(SQL_NO_DATA == SQLFetch(hstmt));
  }
  ENDCATCH;
}

BEGIN_TESTS
  ADD_TEST(t_bug16590994_bit_searchable)
END_TESTS


RUN_TESTS
