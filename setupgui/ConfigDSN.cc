// Copyright (c) 2007, 2020, Oracle and/or its affiliates. All rights reserved. 
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
// which is part of MySQL Connector/ODBC, is also subject to the 
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

#ifdef _WIN32
#include "setupgui.h"
#else
#include "MYODBC_MYSQL.h"
#include "installer.h"
#include "unicode_transcode.h"
#include <sql.h>

#endif

#include "stringutil.h"

int (*odbc_dialog_show)(DataSource*, HWND, BOOL) = nullptr;

#ifndef _WIN32
#include <link.h>

int check_major_gtk_version()
{
  
  struct link_map* lmap = nullptr;
  
  void *dlhandle = dlopen(nullptr, RTLD_NOW);
  int res = dlinfo(dlhandle, RTLD_DI_LINKMAP, &lmap);
  
  if (res != 0)
    return 3;  // If something is not right try using GTK+3
  
  while(NULL != lmap)
  {
    printf("-- %s\n", lmap->l_name);
    if (strstr(lmap->l_name, "gtk-2.0") ||
        strstr(lmap->l_name, "gtk-x11-2")
    )
      return 2;
    lmap = lmap->l_next;
  }
  return 3;
}

#endif


bool setup_dialog_func()
{
#ifdef _WIN32
  odbc_dialog_show = &ShowOdbcParamsDialog;
#else
  if (odbc_dialog_show == nullptr)
  {
    void *dlhandle = nullptr;
    
    switch(check_major_gtk_version())
    {
      case 2: 
        dlhandle = dlopen("libmyodbc8S-gtk2.so", RTLD_NOW);
        break;
      case 3:
        dlhandle = dlopen("libmyodbc8S-gtk3.so", RTLD_NOW);
        break;
    }
    
    if (!dlhandle)
      return false;
    
    odbc_dialog_show = (int(*)(DataSource*, HWND, BOOL))dlsym(dlhandle,
                                                        "ShowOdbcParamsDialog");
  }
#endif
  return true;
}

/*
   Entry point for GUI prompting from SQLDriverConnect().
*/
BOOL Driver_Prompt(HWND hWnd, SQLWCHAR *instr, SQLUSMALLINT completion,
                   SQLWCHAR *outstr, SQLSMALLINT outmax, SQLSMALLINT *outlen)
{
  DataSource *ds= ds_new();
  BOOL rc= FALSE;

  /*
     parse the attr string, dsn lookup will have already been
     done in the driver
  */
  if (instr && *instr)
  {
    if (ds_from_kvpair(ds, instr, (SQLWCHAR)';'))
      goto exit;
  }

  
  if (!setup_dialog_func())
    goto exit;
  
  /* Show the dialog and handle result */
  if (odbc_dialog_show(ds, hWnd, TRUE) == 1)
  {
    int len;
    /* serialize to outstr */
    if ((len= ds_to_kvpair(ds, outstr, outmax, (SQLWCHAR)';')) == -1)
    {
      /* truncated, up to caller to see outmax < *outlen */
      if (outlen)
      {
        *outlen= ds_to_kvpair_len(ds);
      }
      
      /* Prevent access violation if outstr is NULL */
      if (outstr)
      {
        outstr[outmax]= 0;
      }
    }
    else if (outlen)
      *outlen= len;
    rc= TRUE;
  }

exit:
  ds_delete(ds);
  return rc;
}


/*
   Add, edit, or remove a Data Source Name (DSN). This function is
   called by "Data Source Administrator" on Windows, or similar
   application on Unix.
*/
BOOL INSTAPI ConfigDSNW(HWND hWnd, WORD nRequest, LPCWSTR pszDriver,
                        LPCWSTR pszAttributes)
{
  DataSource *ds= ds_new();
  BOOL rc= TRUE;
  Driver *driver= NULL;
  SQLWCHAR *origdsn= NULL;

  if (pszAttributes && *pszAttributes)
  {
    SQLWCHAR delim= ';';

#ifdef _WIN32
    /* 
      if there's no ;, then it's most likely null-delimited

     NOTE: the double null-terminated strings are not working
     *      with UnixODBC-GUI-Qt (posted a bug ) 
    */
    if (!sqlwcharchr(pszAttributes, delim))
      delim= 0;
#endif

    if (ds_from_kvpair(ds, pszAttributes, delim))
    {
      SQLPostInstallerError(ODBC_ERROR_INVALID_KEYWORD_VALUE,
                            W_INVALID_ATTR_STR);
      rc= FALSE;
      goto exitConfigDSN;
    }
    if (ds_lookup(ds) && nRequest != ODBC_ADD_DSN)
    {
      /* ds_lookup() will already set SQLInstallerError */
      rc= FALSE;
      goto exitConfigDSN;
    }
    origdsn= sqlwchardup(ds->name, SQL_NTS);
  }

  switch (nRequest)
  {
  case ODBC_ADD_DSN:
    driver= driver_new();
    memcpy(driver->name, pszDriver,
           (sqlwcharlen(pszDriver) + 1) * sizeof(SQLWCHAR));
    if (driver_lookup(driver))
    {
      rc= FALSE;
      break;
    }
    if (hWnd)
    {
      /*
        hWnd means we will at least try to prompt, at which point
        the driver lib will be replaced by the name
      */
      ds_set_strattr(&ds->driver, driver->lib);
    }
    else
    {
      /*
        no hWnd is a likely a call from an app w/no prompting so
        we put the driver name immediately
      */
      ds_set_strattr(&ds->driver, driver->name);
    }
  case ODBC_CONFIG_DSN:
    
    if (!setup_dialog_func())
    {
      rc = FALSE;
      break;
    }
#ifdef _WIN32
    /*
      for windows, if hWnd is NULL, we try to add the dsn
      with what information was given
    */
    if (!hWnd || odbc_dialog_show(ds, hWnd, FALSE) == 1)
#else
    if (odbc_dialog_show(ds, hWnd, FALSE) == 1)
#endif
    {
      /* save datasource */
      if (ds_add(ds))
        rc= FALSE;
      /* if the name is changed, remove the old dsn */
      if (origdsn && memcmp(origdsn, ds->name,
                            (sqlwcharlen(origdsn) + 1) * sizeof(SQLWCHAR)))
        SQLRemoveDSNFromIni(origdsn);
    }
    break;
  case ODBC_REMOVE_DSN:
    if (SQLRemoveDSNFromIni(ds->name) != TRUE)
      rc= FALSE;
    break;
  }

exitConfigDSN:
  x_free(origdsn);
  ds_delete(ds);
  if (driver)
    driver_delete(driver);
  return rc;
}

#ifdef USE_IODBC
BOOL INSTAPI ConfigDSN(HWND hWnd, WORD nRequest, LPCSTR pszDriverA,
                       LPCSTR pszAttributesA)
{
  BOOL rc;

  size_t lenDriver = strlen(pszDriverA);
  size_t lenAttrib = strlen(pszAttributesA);

  /* We will assume using one-byte Latin string as a subset of UTF-8 */
  SQLWCHAR *pszDriverW= (SQLWCHAR *) myodbc_malloc((lenDriver + 1) * 
                                                sizeof(SQLWCHAR), MYF(0));
  SQLWCHAR *pszAttributesW= (SQLWCHAR *)myodbc_malloc((lenAttrib + 1) * 
                                                  sizeof(SQLWCHAR), MYF(0));

  utf8_as_sqlwchar(pszDriverW, lenDriver, (SQLCHAR* )pszDriverA, lenDriver);
  utf8_as_sqlwchar(pszAttributesW, lenAttrib, (SQLCHAR* )pszAttributesA, 
                   lenAttrib);

  rc= ConfigDSNW(hWnd, nRequest, pszDriverW, pszAttributesW);

  x_free(pszDriverW);
  x_free(pszAttributesW);                 
  return rc;
}
#endif
