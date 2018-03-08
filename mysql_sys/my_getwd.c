// Copyright (c) 2000, 2015, Oracle and/or its affiliates. All rights reserved. 
// 
// This program is free software; you can redistribute it and/or modify 
// it under the terms of the GNU General Public License, version 2.0, as 
// published by the Free Software Foundation. 
// 
// This program is also distributed with certain software (including 
// but not limited to OpenSSL) that is licensed under separate terms, 
// as designated in a particular file or component or in included license 
// documentation. The authors of MySQL hereby grant you an 
// additional permission to link the program and your derivative works 
// with the separately licensed software that they have included with 
// MySQL. 
// 
// Without limiting anything contained in the foregoing, this file, 
// which is part of <MySQL Product>, is also subject to the 
// Universal FOSS Exception, version 1.0, a copy of which can be found at 
// http://oss.oracle.com/licenses/universal-foss-exception. 
// 
// This program is distributed in the hope that it will be useful, but 
// WITHOUT ANY WARRANTY; without even the implied warranty of 
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. 
// See the GNU General Public License, version 2.0, for more details. 
// 
// You should have received a copy of the GNU General Public License 
// along with this program; if not, write to the Free Software Foundation, Inc., 
// 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA 

/* my_setwd() and my_getwd() works with intern_filenames !! */

#include "mysys_priv.h"
#include "my_sys.h"
#include <m_string.h>
#include "mysys_err.h"
#include "my_thread_local.h"
#if defined(_WIN32)
#include <m_ctype.h>
#include <dos.h>
#include <direct.h>
#endif


	/* Test if hard pathname */
	/* Returns 1 if dirname is a hard path */

int sys_test_if_hard_path(const char *dir_name)
{
  if (dir_name[0] == FN_HOMELIB && dir_name[1] == FN_LIBCHAR)
    return (sys_home_dir != NullS && sys_test_if_hard_path(sys_home_dir));
  if (dir_name[0] == FN_LIBCHAR)
    return (TRUE);
#ifdef FN_DEVCHAR
  return (strchr(dir_name,FN_DEVCHAR) != 0);
#else
  return FALSE;
#endif
} /* sys_test_if_hard_path */


/*
  Test if a name contains an (absolute or relative) path.

  SYNOPSIS
    sys_has_path()
    name                The name to test.

  RETURN
    TRUE        name contains a path.
    FALSE       name does not contain a path.
*/

my_bool sys_has_path(const char *name)
{
  return MY_TEST(strchr(name, FN_LIBCHAR))
#if FN_LIBCHAR != '/'
    || MY_TEST(strchr(name,'/'))
#endif
#ifdef FN_DEVCHAR
    || MY_TEST(strchr(name, FN_DEVCHAR))
#endif
    ;
}
