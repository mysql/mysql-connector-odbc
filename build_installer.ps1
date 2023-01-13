<#
Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License, version 2.0
(GPLv2), as published by the Free Software Foundation, with the
following additional permissions:

This program is distributed with certain software that is licensed
under separate terms, as designated in a particular file or component
or in the license documentation. Without limiting your rights under
the GPLv2, the authors of this program hereby grant you an additional
permission to link the program and your derivative works with the
separately licensed software that they have included with the program.

Without limiting the foregoing grant of rights under the GPLv2 and
additional permission as to separately licensed software, this
program is also subject to the Universal FOSS Exception, version 1.0,
a copy of which can be found along with its FAQ at
http://oss.oracle.com/licenses/universal-foss-exception.

This program is distributed in the hope that it will be useful, but
WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
See the GNU General Public License, version 2.0, for more details.

You should have received a copy of the GNU General Public License
along with this program. If not, see 
http://www.gnu.org/licenses/gpl-2.0.html.
/#>

<#
This script is to assist with building the driver and creating the installer. There are two optional arguments:
- $CONFIGURATION: determines what configuration you would like to build the driver in (Debug, Release, RelWithDebInfo)
- $MYSQL_DIR: determines where your MySQL installation is located

The current default behaviour is to build the driver and installer without unit or integration tests on the Release configuration, 
with "C:\Program Files\MySQL\MySQL Server 8.0" set as the location of the MySQL directory

Note that building the installer requires the following:
- Wix 3.0 or above (https://wixtoolset.org/)
- Microsoft Visual Studio environment
- CMake 2.4.6 (http://www.cmake.org)
#>

$ARCHITECTURE = $args[0]
$CONFIGURATION = $args[1]
$MYSQL_DIR = $args[2]

# Set default values
if ($null -eq $CONFIGURATION) {
    $CONFIGURATION = "Release"
}
if ($null -eq $MYSQL_DIR) {
    $MYSQL_DIR = "C:\Program Files\MySQL\MySQL Server 8.0"
}

# BUILD DRIVER
cmake -S . -G "Visual Studio 16 2019" -DMYSQL_DIR="$MYSQL_DIR" -DMYSQLCLIENT_STATIC_LINKING=TRUE -DCMAKE_BUILD_TYPE="$CONFIGURATION" -DBUNDLE_DEPENDENCIES=TRUE
cmake --build . --config "$CONFIGURATION"

# CREATE INSTALLER
# Copy dll, installer, and info files to wix folder
New-Item -Path .\Wix\x64 -ItemType Directory -Force
New-Item -Path .\Wix\x86 -ItemType Directory -Force
New-Item -Path .\Wix\doc -ItemType Directory -Force
Copy-Item .\lib\$CONFIGURATION\myodbc8*.dll .\Wix\x64
Copy-Item .\lib\$CONFIGURATION\myodbc8*.lib .\Wix\x64
Copy-Item .\lib\$CONFIGURATION\myodbc8*.dll .\Wix\x86
Copy-Item .\lib\$CONFIGURATION\myodbc8*.lib .\Wix\x86
Copy-Item .\bin\$CONFIGURATION\myodbc-installer.exe .\Wix\x64
Copy-Item .\bin\$CONFIGURATION\myodbc-installer.exe .\Wix\x86
Copy-Item .\INFO_BIN .\Wix\doc
Copy-Item .\INFO_SRC .\Wix\doc
Copy-Item .\ChangeLog .\Wix\doc
Copy-Item .\README.md .\Wix\doc
Copy-Item .\LICENSE.txt .\Wix\doc

Set-Location .\Wix
if ($ARCHITECTURE -eq "x64") {
    cmake -DMSI_64=1 -G "NMake Makefiles"
}
else {
    cmake -DMSI_64=0 -G "NMake Makefiles"
}
nmake

Set-Location ..\
