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
REM \brief  Create binary distribution (without installer).
REM
REM         Creates binary distribution - without installer.
REM         This may be useful for those wishing to incorporate
REM         myodbc into their own installer.
REM 
REM \note   To do this you are going to need pkzipc (the command-line
REM         version of pkzip) somewhere in your path.
REM
REM \sa     CreateBinaryMsi.bat
REM         CreateSourceZip.bat
REM
REM #########################################################

IF "%1"=="" GOTO doSyntax

ECHO Building...
call Build.bat

ECHO Clean any existing stage area...
IF EXIST .\mysql-connector-odbc-noinstall-%1-win32.zip ( 
    del mysql-connector-odbc-noinstall-%1-win32.zip 
)
IF EXIST .\mysql-connector-odbc-noinstall-%1-win32 ( 
    rmdir /S /Q mysql-connector-odbc-noinstall-%1-win32 
)

IF EXIST .\mysql-connector-odbc-commercial-noinstall-%1-win32.zip ( 
    del mysql-connector-odbc-commercial-noinstall-%1-win32.zip 
)
IF EXIST .\mysql-connector-odbc-commercial-noinstall-%1-win32 ( 
    rmdir /S /Q mysql-connector-odbc-commercial-noinstall-%1-win32 
)

ECHO GPL: Create stage area and populate...
mkdir mysql-connector-odbc-noinstall-%1-win32 
mkdir mysql-connector-odbc-noinstall-%1-win32\bin
mkdir mysql-connector-odbc-noinstall-%1-win32\lib
xcopy /E /Y bin\* mysql-connector-odbc-noinstall-%1-win32\bin
xcopy /E /Y lib\* mysql-connector-odbc-noinstall-%1-win32\lib
copy Install.bat mysql-connector-odbc-noinstall-%1-win32
copy Uninstall.bat mysql-connector-odbc-noinstall-%1-win32
copy ChangeLog mysql-connector-odbc-noinstall-%1-win32\ChangeLog.rtf
copy README.txt mysql-connector-odbc-noinstall-%1-win32\README.rtf
copy LICENSE.txt mysql-connector-odbc-noinstall-%1-win32
copy INFO_BIN mysql-connector-odbc-noinstall-%1-win32
copy INFO_SRC mysql-connector-odbc-noinstall-%1-win32


ECHO Zipping...
pkzipc -add -maximum -recurse -path=current mysql-connector-odbc-noinstall-%1-win32.zip mysql-connector-odbc-noinstall-%1-win32\*.*

ECHO COMMERCIAL: Create stage area and populate...
move mysql-connector-odbc-noinstall-%1-win32 mysql-connector-odbc-commercial-noinstall-%1-win32 
copy LICENSE.txt mysql-connector-odbc-commercial-noinstall-%1-win32\LICENSE.rtf

ECHO Zipping...
pkzipc -add -maximum -recurse -path=current mysql-connector-odbc-commercial-noinstall-%1-win32.zip mysql-connector-odbc-commercial-noinstall-%1-win32/*.*
IF EXIST .\mysql-connector-odbc-commercial-noinstall-%1-win32 ( 
    rmdir /S /Q mysql-connector-odbc-commercial-noinstall-%1-win32 
)

EXIT /B 0

:doSyntax
ECHO "+-----------------------------------------------------+"
ECHO "| CreateBinaryZip.bat                                 |"
ECHO "+-----------------------------------------------------+"
ECHO "|                                                     |"
ECHO "| DESCRIPTION                                         |"
ECHO "|                                                     |"
ECHO "| Use this to build sources and create a ZIP based    |"
ECHO "| noinstaller distribution for commercial and GPL.    |"
ECHO "|                                                     |"
ECHO "| SYNTAX                                              |"
ECHO "|                                                     |"
ECHO "| CreateBinaryZip <version>                           |"
ECHO "|                                                     |"
ECHO "| <version>   must be a 3 number version              |"
ECHO "|                                                     |"
ECHO "| EXAMPLE                                             |"
ECHO "|                                                     |"
ECHO "| CreateBinaryZip 8.0.26                              |"
ECHO "|                                                     |"
ECHO "+-----------------------------------------------------+"

