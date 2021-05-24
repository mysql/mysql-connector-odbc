@ECHO OFF

REM Copyright (c) 2006, 2018, Oracle and/or its affiliates. All rights reserved. 
REM 
REM This program is free software; you can redistribute it and/or modify 
REM it under the terms of the GNU General Public License, version 2.0, as 
REM published by the Free Software Foundation. 
REM
REM This program is also distributed with certain software (including 
REM but not limited to OpenSSL) that is licensed under separate terms, 
REM as designated in a particular file or component or in included license 
REM documentation. The authors of MySQL hereby grant you an 
REM additional permission to link the program and your derivative works 
REM with the separately licensed software that they have included with 
REM MySQL. 
REM 
REM Without limiting anything contained in the foregoing, this file, 
REM which is part of MySQL Server, is also subject to the 
REM Universal FOSS Exception, version 1.0, a copy of which can be found at 
REM http://oss.oracle.com/licenses/universal-foss-exception. 
REM 
REM This program is distributed in the hope that it will be useful, but 
REM WITHOUT ANY WARRANTY; without even the implied warranty of 
REM MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. 
REM See the GNU General Public License, version 2.0, for more details. 
REM 
REM You should have received a copy of the GNU General Public License 
REM along with this program; if not, write to the Free Software Foundation, Inc., 
REM 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA 

REM #########################################################
REM 
REM \brief  Create binary distribution (with installer).
REM
REM         Creates binary distributions with installer. It will
REM         create a GPL and a Commercial version.
REM         1) MSI
REM         2) setup.exe (zipped)
REM 
REM \sa     CreateBinaryZip.bat
REM         CreateSourceZip.bat
REM
REM #########################################################

IF "%1"=="" GOTO doSyntax
IF "%2"=="" GOTO doSyntax

REM Building...
call Build.bat

REM Copying files to wix stage area...
IF NOT EXIST ..\wix-installer\bin\mysql-connector-odbc-%1-win32 (
    mkdir ..\wix-installer\bin\mysql-connector-odbc-%1-win32
)
IF NOT EXIST ..\wix-installer\bin\mysql-connector-odbc-%1-win32\Windows (
    mkdir ..\wix-installer\bin\mysql-connector-odbc-%1-win32\Windows
)
IF NOT EXIST ..\wix-installer\bin\mysql-connector-odbc-%1-win32\Windows\System32 (
    mkdir ..\wix-installer\bin\mysql-connector-odbc-%1-win32\Windows\System32
)
copy bin\* ..\wix-installer\bin\mysql-connector-odbc-%1-win32\Windows\System32
copy lib\* ..\wix-installer\bin\mysql-connector-odbc-%1-win32\Windows\System32
copy LICENSE.txt ..\wix-installer\bin\mysql-connector-odbc-%1-win32\Windows\System32
copy INFO_BIN ..\wix-installer\bin\mysql-connector-odbc-%1-win32\Windows\System32
copy INFO_SRC ..\wix-installer\bin\mysql-connector-odbc-%1-win32\Windows\System32

REM Creating Commercial msi...
cd ..\wix-installer
copy bin\mysql-connector-odbc-%1-win32\Windows\System32\LICENSE.txt bin\mysql-connector-odbc-%1-win32\Windows\System32\myodbc8-license.rtf
copy bin\mysql-connector-odbc-%1-win32\Windows\System32\LICENSE.txt resources\License.rtf
call OdbcMakeSetup.bat %1 %2 commercial

REM Creating GPL msi...
move /Y bin\dist\mysql-connector-odbc-%1-win32.msi bin\dist\mysql-connector-odbc-commercial-%1-win32.msi
move /Y bin\dist\mysql-connector-odbc-%1-win32.msi bin\dist\mysql-connector-odbc-commercial-%1-win32.msi
move /Y bin\dist\mysql-connector-odbc-%1-win32.zip bin\dist\mysql-connector-odbc-commercial-%1-win32.zip
move /Y bin\dist\mysql-connector-odbc-%1-win32.msi.md5 bin\dist\mysql-connector-odbc-commercial-%1-win32.msi.md5
move /Y bin\dist\mysql-connector-odbc-%1-win32.zip.md5 bin\dist\mysql-connector-odbc-commercial-%1-win32.zip.md5
copy bin\mysql-connector-odbc-%1-win32\Windows\System32\LICENSE.txt bin\mysql-connector-odbc-%1-win32\Windows\System32\myodbc8-license.rtf
call OdbcMakeSetup.bat %1 %2 gpl

cd ..\*odbc3

EXIT /B 0

:doSyntax
ECHO "+-----------------------------------------------------+"
ECHO "| CreateBinaryMsi.bat                                 |"
ECHO "+-----------------------------------------------------+"
ECHO "|                                                     |"
ECHO "| DESCRIPTION                                         |"
ECHO "|                                                     |"
ECHO "| Use this to build sources and create a MSI based    |"
ECHO "| installers for commercial and GPL.                  |"
ECHO "|                                                     |"
ECHO "| SYNTAX                                              |"
ECHO "|                                                     |"
ECHO "| CreateBinaryMsi <version> <build-type>              |"
ECHO "|                                                     |"
ECHO "| <version>   must be a 3 number version              |"
ECHO "|                                                     |"
ECHO "| <built-type> must be;                               |"
ECHO "|              a - alpha                              |"
ECHO "|              b - beta                               |"
ECHO "|              r - release candidate                  |"
ECHO "|              p - production                         |"
ECHO "|              i - internal                           |"
ECHO "|                                                     |"
ECHO "| EXAMPLE                                             |"
ECHO "|                                                     |"
ECHO "| CreateBinaryMsi 8.0.26                              |"
ECHO "|                                                     |"
ECHO "+-----------------------------------------------------+"

