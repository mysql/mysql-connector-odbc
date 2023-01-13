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

#include "driver/query_parsing.h"

#include <gtest/gtest.h>

class QueryParsingTest : public testing::Test {};

TEST_F(QueryParsingTest, ParseMultiStatementQuery) {
    const char* query =
        "      \r\n  \t \n  start   \t transaction  ;"
        "select \t col\r\nfrom   \t table;"
        " select \"abcd;efgh\";"
        " \r\n\t   select \"abcd;efgh\"    \t 'ijkl;mnop' ;"
        " select \"abcd;efgh 'ijkl' \";"
        " select 'abcd;efgh';;;   \n\r\t ;"
        "select 'abcd;efgh'    \t \"ijkl;mnop\" ;"
        " select 'abcd;efgh \"ijkl\" ';"
        "select    \t  \n \"abcd   \n  \\\"efgh    ;  \t ijkl\\\"  \\\\\"   \t ;"
        "select   \r\n \'abcd   \n  \\\'efgh    ;  \t ijkl\\\'  \\\\\'";
    std::vector<std::string> expected {
        "start transaction",
        "select col from table",
        "select \"abcd;efgh\"",
        "select \"abcd;efgh\" 'ijkl;mnop'",
        "select \"abcd;efgh 'ijkl' \"",
        "select 'abcd;efgh'",
        "select 'abcd;efgh' \"ijkl;mnop\"",
        "select 'abcd;efgh \"ijkl\" '",
        "select \"abcd   \n  \\\"efgh    ;  \t ijkl\\\"  \\\\\"",
        "select \'abcd   \n  \\\'efgh    ;  \t ijkl\\\'  \\\\\'" };

    std::vector<std::string> actual = parse_query_into_statements(query);
    
    ASSERT_EQ(expected.size(), actual.size());
    for (int i = 0; i < actual.size(); i++)
    {
        EXPECT_EQ(expected[i], actual[i]);
    }
}
