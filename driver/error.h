// Copyright (c) 2001, 2013, Oracle and/or its affiliates. All rights reserved.
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

/***************************************************************************
 * ERROR.H								   *
 *									   *
 * @description: Definitions for error handling				   *
 *									   *
 * @author     : MySQL AB(monty@mysql.com, venu@mysql.com)		   *
 * @date       : 2001-Aug-15						   *
 * @product    : myodbc3						   *
 *									   *
****************************************************************************/

#ifndef __ERROR_H__
#define __ERROR_H__

/* Including driver version definitions */
#include "../MYODBC_CONF.h"
/*
  myodbc internal error constants
*/
#define ER_INVALID_CURSOR_NAME	 514
#define ER_ALL_COLUMNS_IGNORED	 537

/*
  myodbc8 error prefix
*/
#define MYODBC_ERROR_PREFIX	 "[MySQL][ODBC " MYODBC_STRDRIVERID " Driver]"
#define MYODBC_ERROR_CODE_START  500

#define CLEAR_ENV_ERROR(env)   (((ENV *)env)->error.clear())
#define CLEAR_DBC_ERROR(dbc)   (((DBC *)dbc)->error.clear())
#define CLEAR_STMT_ERROR(stmt) (((STMT *)stmt)->error.clear())
#define CLEAR_DESC_ERROR(desc) (((DESC *)desc)->error.clear())

#define NEXT_ERROR(error) \
  (error.current ? 2 : (error.current= 1))

#define NEXT_ENV_ERROR(env)   NEXT_ERROR(((ENV *)env)->error)
#define NEXT_DBC_ERROR(dbc)   NEXT_ERROR(((DBC *)dbc)->error)
#define NEXT_STMT_ERROR(stmt) NEXT_ERROR(((STMT *)stmt)->error)
#define NEXT_DESC_ERROR(desc) NEXT_ERROR(((DESC *)desc)->error)

/*
  list of MyODBC3 error codes
*/
typedef enum myodbc_errid
{
    MYERR_01000 = 0,
    MYERR_01004,
    MYERR_01S02,
    MYERR_01S03,
    MYERR_01S04,
    MYERR_01S06,
    MYERR_07001,
    MYERR_07005,
    MYERR_07006,
    MYERR_07009,
    MYERR_08002,
    MYERR_08003,
    MYERR_24000,
    MYERR_25000,
    MYERR_25S01,
    MYERR_34000,
    MYERR_HYT00,
    MYERR_S1000,
    MYERR_S1001,
    MYERR_S1002,
    MYERR_S1003,
    MYERR_S1004,
    MYERR_S1007,
    MYERR_S1009,
    MYERR_S1010,
    MYERR_S1011,
    MYERR_S1012,
    MYERR_S1013,
    MYERR_S1015,
    MYERR_S1016,
    MYERR_S1017,
    MYERR_S1024,
    MYERR_S1090,
    MYERR_S1091,
    MYERR_S1092,
    MYERR_S1093,
    MYERR_S1095,
    MYERR_S1106,
    MYERR_S1107,
    MYERR_S1109,
    MYERR_S1C00,

    MYERR_21S01,
    MYERR_23000,
    MYERR_42000,
    MYERR_42S01,
    MYERR_42S02,
    MYERR_42S12,
    MYERR_42S21,
    MYERR_42S22,
    MYERR_08S01,
    /* Please add new errors to the end of enum, and not in alphabet order */
    MYERR_08004,
} myodbc_errid;

/*
  error handler structure
*/
struct MYERROR
{
  SQLRETURN   retcode = 0;
  char	      sqlstate[6] = {'\0', '\0', '\0', '\0', '\0', '\0'};
  char        current = 0;
  char	      message[SQL_MAX_MESSAGE_LENGTH + 1] = { '\0' };
  SQLINTEGER  native_error = 0;

  MYERROR()
  {}

  MYERROR(SQLRETURN rc) : MYERROR()
  {
    retcode = rc;
  }

  MYERROR(myodbc_errid errid, const char *errtext, SQLINTEGER errcode,
    const char *prefix);

  MYERROR(const char *sqlstate, const char *errtext, SQLINTEGER errcode,
    const char *prefix);

  operator bool() const
  {
    return native_error != 0 || retcode != 0;
  }

  void clear()
  {
    retcode = 0;
    message[0] = '\0';
    current = 0;
    native_error = 0;
    sqlstate[0] = '\0';
  }
};

/*
  error handler-predefined structure
  odbc2 state, odbc3 state, message and return code
*/
typedef struct myodbc3_err_str {
  char	   sqlstate[6];  /* ODBC3 STATE, if SQL_OV_ODBC2, then ODBC2 STATE */
  char	   message[SQL_MAX_MESSAGE_LENGTH+1];/* ERROR MSG */
  SQLRETURN   retcode;	    /* RETURN CODE */
} MYODBC3_ERR_STR;

#endif /* __ERROR_H__ */
