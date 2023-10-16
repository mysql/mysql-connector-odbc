# Copyright (c) 2007, 2023, Oracle and/or its affiliates. All rights reserved.
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License, version 2.0, as
# published by the Free Software Foundation.
#
# This program is also distributed with certain software (including
# but not limited to OpenSSL) that is licensed under separate terms,
# as designated in a particular file or component or in included license
# documentation. The authors of MySQL hereby grant you an
# additional permission to link the program and your derivative works
# with the separately licensed software that they have included with
# MySQL.
#
# Without limiting anything contained in the foregoing, this file,
# which is part of MySQL Connector/ODBC, is also subject to the
# Universal FOSS Exception, version 1.0, a copy of which can be found at
# http://oss.oracle.com/licenses/universal-foss-exception.
#
# This program is distributed in the hope that it will be useful, but
# WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
# See the GNU General Public License, version 2.0, for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software Foundation, Inc.,
# 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA

##########################################################################

CONFIGURE_FILE(${CMAKE_SOURCE_DIR}/scripts/macosx/postflight.in ${CMAKE_SOURCE_DIR}/scripts/macosx/postflight @ONLY)

SET(LICENSE_FILE "${CMAKE_SOURCE_DIR}/LICENSE.txt")
SET(LICENSE_HTML  "${CMAKE_SOURCE_DIR}/scripts/macosx/License.html.in")
SET(HTML_HEADER "<!DOCTYPE html PUBLIC \"-//w3c//dtd html 4.0 transitional//en\">
<html>
<head>
  <meta http-equiv=\"Content-Type\" content=\"text/html; charset=iso-8859-1\">
  <title>Connector/ODBC - License</title>
</head>
<body>
<font face=\"Helvetica\">
<pre class=\"programlisting\">
<h2>Licensing Information User Manual</h2>
<h3>MySQL Connector/ODBC ${CONNECTOR_MAJOR}.${CONNECTOR_MINOR}</h3>
<hr>
")

FILE(READ ${LICENSE_FILE} CONTENTS)
FILE(WRITE "${LICENSE_HTML}" ${HTML_HEADER})
FILE(APPEND "${LICENSE_HTML}" "${CONTENTS}")
FILE(APPEND "${LICENSE_HTML}" "</pre></body></html>")

