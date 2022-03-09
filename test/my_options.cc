// Copyright (c) 2020, Oracle and/or its affiliates. All rights reserved.
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

#include <string>
#include <vector>
#include <fstream>
#include <iostream>
#include <sstream>
#include <cstdio>
#include <map>
#include <vector>
#include <stdlib.h>

#include "odbctap.h"

#ifdef _WIN32
#pragma warning (disable : 4996)
// warning C4996: 'mkdir': The POSIX name for this item is deprecated. Instead, use the ISO C and C++ conformant name: _mkdir.
#include <direct.h>
#else
#include <sys/stat.h>
#endif

DECLARE_TEST(t_wl13883)
{

  if (strcmp((const char*)myserver, "localhost"))
    return OK;

  struct dataObject
  {
    std::string m_path;

    dataObject(std::string path) : m_path(path)
    {}

    const char *path() { return m_path.c_str(); }
  };

  struct dataDir : public dataObject
  {
    void cleanup()
    {
      std::stringstream cmd;
#if defined(_WIN32)
      cmd << "rd /s /q \"" << m_path << "\"";
#else
      cmd << "rm -rf " << m_path;
#endif
      std::cout << "CLEANING-UP..." << std::endl
        << "EXECUTING SYSTEM COMMAND: " << cmd.str() << std::endl;
      std::system(cmd.str().c_str());
    }

    dataDir(std::string path, bool is_hidden = false) : dataObject(path)
    {
      cleanup();
      std::cout << "CREATING A DIRECTORY: " << path << std::endl;
      try
      {
#if defined(_WIN32)
        if(mkdir(m_path.c_str()))
#else
        if(mkdir(m_path.c_str(), S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH))
#endif
        {
          throw "Destination directory could not be created";
        }

#if defined(_WIN32)
        if (is_hidden && !SetFileAttributes(m_path.c_str(), FILE_ATTRIBUTE_HIDDEN))
        {
          throw "Could not set the hidden attribute";
        }
#endif
      }
      catch(...)
      {
        cleanup();
        throw;
      }
    }

    ~dataDir()
    {
      cleanup();
    }

  };

  /*
    Create a data file to be used for LOAD DATA LOCAL INFILE
    It does not need to do a clean-up because the directory destructor will
    take care of it.
  */
  struct dataFile : public dataObject
  {
    dataFile(std::string dir, std::string file_name, bool write_only = false) :
      dataObject(dir + file_name)
    {
      std::ofstream infile;
      infile.open(m_path, std::ios::out|std::ios::binary);
      infile << "1,\"val1\"\n";
      infile << "2,\"val2\"\n";
      infile.close();

#ifndef _WIN32
      /*
        In windows all files are always readable; it is not possible to
        give write-only permission. For this reason the write-only test
        is not done in Windows
      */
      if (write_only)
      {
        std::stringstream sstr;
        sstr << "chmod -r " << m_path;
        if(system(sstr.str().c_str()))
        {
          throw "File permissions could not be set";
        }
      }
#endif
    }
  };

#ifndef _WIN32
  /*
    Windows can create symlinks, but the admin access is required.
    Or the machine must be in Developer Mode.
  */
  struct dataSymlink : public dataObject
  {
    dataSymlink(std::string fpath, std::string spath, bool directory = false)
      : dataObject(spath)
    {
      std::cout << "CREATING SYMLINK: " << spath << " -> " << fpath << std::endl;
      if (symlink(fpath.c_str(), spath.c_str()))
        throw "Error creating symbolic link";
    }
  };
#endif

struct connLocal
{
  SQLHENV m_henv;
  SQLHDBC m_hdbc;
  SQLHSTMT m_hstmt;

  connLocal(const char* opts)
  {
    alloc_basic_handles_with_opt(&m_henv, &m_hdbc, &m_hstmt, nullptr,
                                 nullptr, nullptr, nullptr, (SQLCHAR*)opts);
  }

  int execute(const char* query)
  {
    ok_stmt(m_hstmt, SQLExecDirect(m_hstmt, (SQLCHAR*)query, SQL_NTS));
    return OK;
  }

  ~connLocal()
  {
    free_basic_handles(&m_henv, &m_hdbc, &m_hstmt);
  }
};

struct testLocal
  {

#ifdef _WIN32
    enum class slash_type { MAKE_FORWARD, MAKE_BACKWARD, MAKE_DOUBLE_BACKWARD };

    static std::string slash_fix(std::string s, slash_type st)
    {
      std::string s_find = "\\";
      std::string s_repl = "/";

      switch(st)
      {
        case slash_type::MAKE_FORWARD:
          s_find = "\\";
          s_repl = "/";
        break;
        case slash_type::MAKE_BACKWARD:
          s_find = "/";
          s_repl = "\\";
        break;
        case slash_type::MAKE_DOUBLE_BACKWARD:
          s_find = "/";
          s_repl = "\\\\";
        break;
      }

      size_t pos = s.find(s_find);
      while (pos != std::string::npos)
      {
        s.replace(pos, 1, s_repl);
        pos = s.find(s_find);
      }
      return s;
    }
#endif


    connLocal m_conn_main;

    testLocal(std::string opts) : m_conn_main(opts.c_str())
    {
      m_conn_main.execute("DROP TABLE IF EXISTS test_local_infile");
      m_conn_main.execute("CREATE TABLE test_local_infile(id INT, value VARCHAR(20))");
    }

    ~testLocal()
    {
      m_conn_main.execute("DROP TABLE IF EXISTS test_local_infile");
      set_server_infile(false);
    }

    int set_server_infile(bool enabled)
    {
      std::string q = "SET GLOBAL local_infile=";
      q += enabled ? "1" : "0";
      return m_conn_main.execute(q.c_str());
    }


    /*
      Performs the following test:

      1. Opens new connection with LOCAL_INFILE and LOAD_DATA_LOCAL_DIR
         parameters set to client_local_infile and load_data_path, respectively.

      2. If set_after_connect is true, the LOAD_DATA_LOCAL_DIR parameter
         is set after making connection.

      3. Server's local_infile global variable is set to the
         value of server_local_infile.

      4. A LOAD DATA LOCAL INFILE command is executed for the
         file specified by file_path.

      5. For Windows the function can use forward and backward slashes in the
         directory and file paths depending on use_back_slash parameter

      6. If above succeeds, the data loaded from the file is examined.

      Returns true if test was successful, false otherwise.
      If parameter res is false, then it is expected that LOAD DATA LOCAL
      command in step 4 should fail with error.
    */

#define assert_str(a, b, c) \
do { \
  char *val_a= (char *)(a), *val_b= (char *)(b); \
  int val_len= (int)(c); \
  if (strncmp(val_a, val_b, val_len) != 0) { \
    printf("# %s ('%*s') != '%*s' in %s on line %d\n", \
           #a, val_len, val_a, val_len, val_b, __FILE__, __LINE__); \
    throw "Strings do not match"; \
  } \
} while (0);

#define assert_num(a, b) \
do { \
  long long a1= (a), a2= (b); \
  if (a1 != a2) { \
    printf("# %s (%lld) != %lld in %s on line %d\n", \
           #a, a1, a2, __FILE__, __LINE__); \
    throw "Numbers do not match"; \
  } \
} while (0)


    bool do_test(bool server_local_infile, bool client_local_infile,
                 const char *load_data_path, /* Uses forward slash if not null */
                 std::string file_path,
                 bool set_after_connect = false,
                 bool res = true, bool use_back_slash = false)
    {
      bool error_expected = !res;
      std::string t = load_data_path ? load_data_path : "nullptr";
      std::string file_path_query = file_path;

#ifdef _WIN32
      if (use_back_slash)
      {
        t = slash_fix(t, slash_type::MAKE_BACKWARD);
        file_path = slash_fix(file_path, slash_type::MAKE_BACKWARD);

        if (load_data_path && *load_data_path)
          load_data_path = t.c_str(); // use the back-slashed path
        /*
          The file path must be properly escaped for LOAD DATA LOCAL INFILE
          query. Therefore, it must use the double backslashes if needed.
        */
        file_path_query = slash_fix(file_path_query,
                                    slash_type::MAKE_DOUBLE_BACKWARD);
      }
#endif

      std::cout << " ENABLE_LOCAL_INFILE=" << (int)client_local_infile << std::endl <<
        " LOAD_DATA_LOCAL_DIR=" << t << std::endl <<
        " FILE_IN=" << file_path << std::endl;

      std::string opts = "ENABLE_LOCAL_INFILE=";
      opts += client_local_infile ? "1" : "0";

      if (load_data_path)
      {
        opts += ";LOAD_DATA_LOCAL_DIR=";
        opts += load_data_path;
      }

      set_server_infile(server_local_infile);

      try
      {
        connLocal conn(opts.c_str());
        SQLCHAR buf[512];

        std::stringstream sstr;
        sstr << "LOAD DATA LOCAL INFILE '" << file_path_query <<
            "' INTO TABLE test_local_infile FIELDS TERMINATED BY ',' "
            "OPTIONALLY ENCLOSED BY '\"' LINES TERMINATED BY '\n'";

        std::cout << " Result: ";
        SQLRETURN rc = conn.execute(sstr.str().c_str());
        if (rc != SQL_SUCCESS)
          assert_num(SQL_SUCCESS_WITH_INFO, rc);


        assert_num(SQL_SUCCESS, conn.execute("SELECT * FROM test_local_infile ORDER BY id ASC;"));

        assert_num(SQL_SUCCESS, SQLFetch(conn.m_hstmt));
        assert_num(1, my_fetch_int(conn.m_hstmt, 1));
        assert_str(my_fetch_str(conn.m_hstmt, buf, 2), "val1", 4);

        assert_num(SQL_SUCCESS, SQLFetch(conn.m_hstmt));
        assert_num(2, my_fetch_int(conn.m_hstmt, 1));
        assert_str(my_fetch_str(conn.m_hstmt, buf, 2), "val2", 4);

        assert_num(SQL_NO_DATA, SQLFetch(conn.m_hstmt));
      }
      catch(...)
      {
        std::cout << "Error (nothing is loaded)" << std::endl;

        if (error_expected)
          return true;

        return false;
      }

      std::cout << "Data is Loaded" << std::endl;
      if (error_expected)
        return false;

      return true;
    }
  };

  auto get_rand_name = []()
  {
    srand(time(NULL)); // use current time as seed for random generator
    int random_variable = rand();
    static const char hexdigit[17] = "0123456789abcdef";
    char buf[17];
    for(int i = 0; i < 16; ++i)
        buf[i] = hexdigit[rand() % 16];
      buf[16] = '\0';
    return std::string(buf);
  };


  try
  {
    {
      std::string temp_dir;
      std::string dir;
      std::string file_path;
      std::string test_dir = get_rand_name();

#ifdef _WIN32
      temp_dir = getenv("TEMP");

      // easier to deal with forward slash
      temp_dir = testLocal::slash_fix(temp_dir,
                                      testLocal::slash_type::MAKE_FORWARD);
      temp_dir.append("/");

#else
      temp_dir = "/tmp/";
#endif

      temp_dir.append(test_dir + "/");
      dir = temp_dir + "test/";
      file_path = dir + "infile.txt";

      dataDir dir_temp(temp_dir);
      dataDir dir_test(dir);
      dataDir dir_link(temp_dir + "test_link/");
      dataDir dir_subdir_link(temp_dir + "test_subdir_link/");

      dataFile infile(dir, "infile.txt");

#ifdef _WIN32
      dataDir hidden_dir(temp_dir + "HiddenTest/", true);
      dataFile file_in_hidden_dir(temp_dir + "HiddenTest/", "infile.txt");
#else
      dataFile infile_wo(dir, "infile_wo.txt", true);
      dataSymlink sl(file_path, temp_dir + "test_link/link_infile.txt");
      dataSymlink sld(dir, temp_dir + "test_subdir_link/subdir");
      std::string sld_file = sld.path();
      sld_file.append("/infile.txt");
#endif

      struct paramtest
      {
        int enable_local_infile;
        const char *load_data_local_dir;
        const char *file_in;
        bool expected_result;
      } params[] = {
        {0,"",                     infile.path(),                   false},
        {0,nullptr,                infile.path(),                   false},
        {0,dir_test.path(),        infile.path(),                   true},
        {1,dir_test.path(),        infile.path(),                   true},
        {0,"invalid_test/",        "invalid_test/infile.txt",       false},
#ifdef _WIN32
        {0,hidden_dir.path(),      file_in_hidden_dir.path(),       true}
#else
        {1,dir_test.path(),        sl.path(),                       true},
        {1,dir_test.path(),        sld_file.c_str(),                true},
        {0,dir_test.path(),        sld_file.c_str(),                true},
        {0,dir_test.path(),        sl.path(),                       true},
        {0,dir_link.path(),        sl.path(),                       false},
        {0,dir_subdir_link.path(), sld_file.c_str(),                false},
        {0,dir_test.path(),        infile_wo.path(),                false},
        {1,nullptr,                infile_wo.path(),                false}
#endif
      };

      for (int k = 0; k < 2; ++k)
      {
        bool server_local_infile = true;
        bool set_after_connect = false;
        std::cout << "---- TESTS SERIES: ";
        switch(k)
        {
          case 0:
            std::cout << "Normal scenario" << std::endl;
            break;

          case 1:
            std::cout << "Disable OPT_LOCAL_INFILE on the server" << std::endl;
            server_local_infile = false;
            break;
        }

        int test_num = 0;
        for (auto elem : params)
        {
          std::cout << "-- TEST" << (++test_num) << std::endl;

          for(bool use_backslash : {false, true})
          {
            testLocal test("");
            if (!test.do_test(server_local_infile,
                                (bool)elem.enable_local_infile,
                                elem.load_data_local_dir,
                                elem.file_in,
                                set_after_connect,
                                server_local_infile ? elem.expected_result : false,
                                use_backslash))
              return FAIL;
#ifndef _WIN32
            break;
#endif
          }
        }

      }

    }

  }
  catch (...)
  {
    printf ("Test failed in %s on line %d\n", __FILE__, __LINE__);
    return FAIL;
  }

  return OK;
}

/*
  Use mysql client library implementation of DNS+SRV
*/
DECLARE_TEST(t_wl14362)
{
  if (!mydns_srv)
  {
    SKIP_REASON = "Env variable TEST_DNS_SRV is not specified\n";
    return SKIP;
  }

  const int con_num = 30;
  std::string opts("ENABLE_DNS_SRV=1;SERVER=");
  opts.append((char*)mydns_srv);
  std::map<int, int> con_map;
  char buf[1024];

  for (int i = 0; i < con_num; ++i)
  {
    DECLARE_BASIC_HANDLES(henv1, hdbc1, hstmt1);

    is(OK == alloc_basic_handles_with_opt(&henv1, &hdbc1, &hstmt1,
       (SQLCHAR*)USE_DRIVER, myuid, mypwd, mydb, (SQLCHAR*)opts.c_str()));

    ok_sql(hstmt1, "SHOW VARIABLES LIKE 'server_id'");

    //my_fetch_str(hstmt1, (SQLCHAR*)buf, 2);
    while(SQLFetch(hstmt1) == SQL_SUCCESS)
    {
      SQLINTEGER server_id = my_fetch_int(hstmt1, 2);
      con_map[server_id]++;
    }
    free_basic_handles(&henv1, &hdbc1, &hstmt1);
  }

  for (auto el : con_map)
  {
    std::cout << "Server " << el.first << " : " << el.second <<
                 " connects" << std::endl;
    is(el.second > 1); // Each server should get at least two connections
  }
  return OK;
}

struct test_params
{
  int no_catalog;
  int no_schema;
  const char* catalog_name;
  const char* schema_name;
  SQLRETURN expected_res;
};


int run_func_tests(test_params &par)
{

  SQLRETURN rc = SQL_SUCCESS;
  SQLCHAR buf[4096];

  DECLARE_BASIC_HANDLES(henv1, hdbc1, hstmt1);

  auto check_results = [&](SQLRETURN ret, const char *func,
                           const char *col1 = nullptr,
                           const char *col2 = nullptr)
  {
    size_t expected_len1 = par.catalog_name ?
                                  strlen(par.catalog_name) : 0;
    size_t expected_len2 = par.schema_name ?
                                 strlen(par.schema_name) : 0;

    std::cout << func << std::endl;

    if (par.expected_res != SQL_ERROR)
      ok_stmt(hstmt1, ret);

    is_num(par.expected_res, ret);

    /* On expected error the rows do not need to be checked */
    if (par.expected_res == SQL_ERROR)
      return OK;

    int rows_result = 0;
    while(SQL_SUCCESS == SQLFetch(hstmt1))
    {
      const char *str1 = par.catalog_name;
      const char *str2 = par.schema_name;

      if (strcmp(func, "SQLSpecialColumns") == 0)
      {
        str1 = col1;
        str2 = col2;
        expected_len1 = 0;
        expected_len2 = str2 ? strlen(str2) : 0;
      }

      if (strcmp(func, "SQLForeignKeys") == 0)
      {
        is_str(str1, my_fetch_str(hstmt1, buf, 5),
               expected_len1);
        is_str(str2, my_fetch_str(hstmt1, buf, 6),
               expected_len2);
      }

      is_str(str1, my_fetch_str(hstmt1, buf, 1),
             expected_len1);
      is_str(str2, my_fetch_str(hstmt1, buf, 2),
             expected_len2);
      ++rows_result;
    }

    is(rows_result > 0);
    SQLFreeStmt(hstmt1, SQL_CLOSE);
    return OK;
  };

  for (int i = 0; i < 2; ++i)
  {
    std::string connstr = "NO_CATALOG=";
    connstr.append(par.no_catalog ? "1" : "0");
    connstr.append(";NO_SCHEMA=");
    connstr.append(par.no_schema ? "1" : "0");

    connstr.append( i ? ";NO_I_S=1" : ";NO_I_S=0");
    std::cout << "Connectin parameters [ " << connstr << " ]" << std::endl <<
                 "CATALOG: [ " << (par.catalog_name ? par.catalog_name : "NULL") <<
                 " ] SCHEMA: [ " << (par.schema_name ? par.schema_name : "NULL")
                 << " ]" << std::endl;

    alloc_basic_handles_with_opt(&henv1, &hdbc1, &hstmt1,
                                 nullptr, nullptr, nullptr, nullptr,
                                 (SQLCHAR*)connstr.c_str());

    rc = SQLTables(hstmt1, (SQLCHAR*)par.catalog_name, SQL_NTS,
                           (SQLCHAR*)par.schema_name, SQL_NTS,
                           (SQLCHAR*)"t_wl14490a", SQL_NTS,
                           (SQLCHAR*)"TABLE", SQL_NTS);
    is_num(OK, check_results(rc, "SQLTables"));

    rc = SQLColumns(hstmt1, (SQLCHAR*)par.catalog_name, SQL_NTS,
                            (SQLCHAR*)par.schema_name, SQL_NTS,
                            (SQLCHAR*)"t_wl14490a", SQL_NTS,
                            (SQLCHAR*)"a", SQL_NTS);
    is_num(OK, check_results(rc, "SQLColumns"));

    rc = SQLStatistics(hstmt1, (SQLCHAR*)par.catalog_name, SQL_NTS,
                               (SQLCHAR*)par.schema_name, SQL_NTS,
                               (SQLCHAR*)"t_wl14490a", SQL_NTS,
                               SQL_INDEX_ALL,SQL_QUICK);
    is_num(OK, check_results(rc, "SQLStatistics"));

    rc = SQLSpecialColumns(hstmt1, SQL_ROWVER,
                           (SQLCHAR*)par.catalog_name, SQL_NTS,
                           (SQLCHAR*)par.schema_name, SQL_NTS,
                           (SQLCHAR*)"t_wl14490a", SQL_NTS,
                           SQL_SCOPE_SESSION, SQL_NULLABLE);
    is_num(OK, check_results(rc, "SQLSpecialColumns", nullptr, "b_ts"));

    rc = SQLPrimaryKeys(hstmt1, (SQLCHAR*)par.catalog_name, SQL_NTS,
                        (SQLCHAR*)par.schema_name, SQL_NTS,
                        (SQLCHAR*)"t_wl14490a", SQL_NTS);
    is_num(OK, check_results(rc, "SQLPrimaryKeys"));

    rc = SQLForeignKeys(hstmt1, (SQLCHAR*)par.catalog_name, SQL_NTS,
                                (SQLCHAR*)par.schema_name, SQL_NTS,
                                NULL, 0, // All tables referenced by t_wl14490c
                                (SQLCHAR*)par.catalog_name, SQL_NTS,
                                (SQLCHAR*)par.schema_name, SQL_NTS,
                                (SQLCHAR *)"t_wl14490c", SQL_NTS);
    is_num(OK, check_results(rc, "SQLForeignKeys"));

    rc = SQLTablePrivileges(hstmt1, (SQLCHAR*)par.catalog_name, SQL_NTS,
                            (SQLCHAR*)par.schema_name, SQL_NTS,
                            (SQLCHAR*)"t_wl14490a", SQL_NTS);
    is_num(OK, check_results(rc, "SQLTablePrivileges"));

    rc = SQLColumnPrivileges(hstmt1, (SQLCHAR*)par.catalog_name, SQL_NTS,
                            (SQLCHAR*)par.schema_name, SQL_NTS,
                            (SQLCHAR*)"t_wl14490a", SQL_NTS,
                            (SQLCHAR*)"a", SQL_NTS);
    is_num(OK, check_results(rc, "SQLColumnPrivileges"));

    rc = SQLProcedures(hstmt1, (SQLCHAR*)par.catalog_name, SQL_NTS,
                            (SQLCHAR*)par.schema_name, SQL_NTS,
                            (SQLCHAR*)"procwl14490", SQL_NTS);
    is_num(OK, check_results(rc, "SQLProcedures"));

    rc = SQLProcedureColumns(hstmt1, (SQLCHAR*)par.catalog_name, SQL_NTS,
                            (SQLCHAR*)par.schema_name, SQL_NTS,
                            (SQLCHAR*)"procwl14490", SQL_NTS,
                            nullptr, 0);
    is_num(OK, check_results(rc, "SQLProcedureColumns"));

    {
      SQLUSMALLINT cat_vals[] = {
        SQL_MAX_CATALOG_NAME_LEN,
        SQL_MAX_QUALIFIER_NAME_LEN,
        SQL_CATALOG_NAME,
        SQL_CATALOG_NAME_SEPARATOR,
        SQL_CATALOG_TERM,
        SQL_CATALOG_USAGE
      };
      char info_buf[512];
      unsigned *pbuf = (unsigned*)info_buf;
      std::cout << "SQLGetInfo(CATALOG)" << std::endl;
      for(SQLUSMALLINT v : cat_vals)
      {
        *pbuf = 0;
        SQLSMALLINT str_len_ptr = sizeof(info_buf);
        ok_stmt(hdbc1, SQLGetInfo(hdbc1, v, info_buf, sizeof(info_buf),
                                  nullptr));
        std::cout << "[" << *pbuf << "]";
        if (par.no_catalog)
        {
          if(!(*pbuf == 0))
            std::cout << "(err) ";
        }
        else
        {
          if(!(*pbuf != 0))
            std::cout << "(err) ";
        }
      }
      std::cout << std::endl;
    }

    {
      SQLUSMALLINT schema_vals[] = {
        SQL_MAX_SCHEMA_NAME_LEN,
        SQL_MAX_OWNER_NAME_LEN,
        SQL_SCHEMA_TERM,
        SQL_SCHEMA_USAGE
      };
      char info_buf[512];
      unsigned *pbuf = (unsigned*)info_buf;
      std::cout << "SQLGetInfo(SCHEMA)" << std::endl;
      for(SQLUSMALLINT v : schema_vals)
      {
        *pbuf = 0;
        SQLSMALLINT str_len_ptr = sizeof(info_buf);
        ok_stmt(hdbc1, SQLGetInfo(hdbc1, v, info_buf, sizeof(info_buf),
                                  nullptr));
        std::cout << "[" << *pbuf << "]";
        if (par.no_schema)
        {
          if(!(*pbuf == 0))
            std::cout << "(err) ";
        }
        else
        {
          if(!(*pbuf != 0))
            std::cout << "(err) ";
        }
      }
      std::cout << std::endl;
    }

    free_basic_handles(&henv1, &hdbc1, &hstmt1);
  }

  return OK;
}

DECLARE_TEST(t_wl14490)
{
  char buf[256];
  ok_sql(hstmt, "DROP TABLE IF EXISTS t_wl14490c, t_wl14490b, t_wl14490a");
  ok_sql(hstmt, "CREATE TABLE t_wl14490a (a INT PRIMARY KEY, "
                "b_ts TIMESTAMP DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP) ENGINE=InnoDB");
  ok_sql(hstmt, "CREATE TABLE t_wl14490b (b INT PRIMARY KEY) ENGINE=InnoDB");
  ok_sql(hstmt, "CREATE TABLE t_wl14490c (a INT, b INT, UNIQUE(a), UNIQUE(b),"
                "CONSTRAINT `first_constraint1` FOREIGN KEY (`b`) REFERENCES `t_wl14490b` (`b`),"
                "CONSTRAINT `second_constraint1` FOREIGN KEY (`a`) REFERENCES `t_wl14490a` (`a`)"
                ") ENGINE=InnoDB");
  ok_sql(hstmt, "SELECT CURRENT_USER()");
  ok_stmt(hstmt, SQLFetch(hstmt));
  my_fetch_str(hstmt, (SQLCHAR*)buf, 1);

  //escape %
  std::string user;
  const char *pAt = strstr(buf, "@");
  user += std::string(buf, pAt - buf + 1);
  user += "'";
  user += pAt+1;
  user += "'";

  SQLFetch(hstmt);
  SQLFreeStmt(hstmt, SQL_CLOSE);

  std::string query = "GRANT ALL ON ";
  query.append((char *)mydb).append(".").append("t_wl14490a to ").append(user);
  ok_stmt(hstmt, SQLExecDirect(hstmt, (SQLCHAR*)query.c_str(), query.length()));

  query = "GRANT INSERT (a), SELECT (a), REFERENCES (a), UPDATE (a) ON ";
  query.append((char *)mydb).append(".").append("t_wl14490a to ").append(user);
  ok_stmt(hstmt, SQLExecDirect(hstmt, (SQLCHAR*)query.c_str(), query.length()));

  ok_sql(hstmt, "DROP PROCEDURE IF EXISTS procwl14490");
  ok_sql(hstmt, "CREATE PROCEDURE procwl14490(IN p1 INT, IN p2 INT) begin end;");

  test_params params[] = {
    {0, 0, (const char*)mydb, nullptr, SQL_SUCCESS},
    {0, 0, nullptr, (const char*)mydb, SQL_SUCCESS},
    {0, 0, (const char*)mydb, (const char*)mydb, SQL_ERROR},
    {1, 0, (const char*)mydb, nullptr, SQL_ERROR},
    {1, 0, nullptr, (const char*)mydb, SQL_SUCCESS},
    {0, 1, (const char*)mydb, nullptr, SQL_SUCCESS},
    {0, 1, nullptr, (const char*)mydb, SQL_ERROR},
    {1, 1, nullptr, (const char*)mydb, SQL_ERROR},
    {1, 1, (const char*)mydb, nullptr, SQL_ERROR},
    {1, 1, nullptr, nullptr, SQL_SUCCESS},
  };

  for (auto &elem : params)
  {
    if (run_func_tests(elem) != OK)
      return FAIL;
  }

  ok_sql(hstmt, "DROP TABLE t_wl14490c, t_wl14490b, t_wl14490a");
  ok_sql(hstmt, "DROP PROCEDURE procwl14490");

  return OK;
}

#define ok_dsn(C) if (!C) { \
  std::cout << "Call failed: " << #C << " in line " << __LINE__ << ":" << \
   __FILE__ << std::endl; \
  return FAIL; \
}
/*
  Deprecate NO_I_S connection option
*/
DECLARE_TEST(t_wl14586)
{
  SQLHDBC hdbc1;
  const char *dsn_name = "wl14586";
  SQLCHAR drv_info[1024];
  std::string drv_nobrackets;

  if (mydriver[0] == '{' && strlen((const char*)mydriver) > 2)
    drv_nobrackets = std::string((const char*)(mydriver + 1),
                                 strlen((const char*)mydriver) - 2);
  else
    drv_nobrackets = (const char*)mydriver;

  ok_env(henv, SQLAllocHandle(SQL_HANDLE_DBC, henv, &hdbc1));

  SQLCHAR *connstr = make_conn_str((SQLCHAR*)USE_DRIVER,
                                nullptr, nullptr, nullptr,
                                (const SQLCHAR*)"NO_I_S=1", 0);

  SQLRETURN rc = SQLDriverConnect(hdbc1, NULL, connstr, SQL_NTS, nullptr, 0,
                                  nullptr, SQL_DRIVER_NOPROMPT);
  // Driver must report the option deprecation
  is(rc == SQL_SUCCESS_WITH_INFO);
  print_diag(rc, SQL_HANDLE_DBC, hdbc1, "Expected Info inSQLDriverConnect():",
             __FILE__, __LINE__);
  ok_con(hdbc1, SQLDisconnect(hdbc1));

  ok_dsn(SQLGetPrivateProfileString((LPCSTR)drv_nobrackets.c_str(),
                                    (LPCSTR)"DRIVER", (LPCSTR)"",
                                    (LPSTR)drv_info, 1023, "odbcinst.ini"));

  SQLRemoveDSNFromIni((LPCSTR)dsn_name);

  if (!SQLWriteDSNToIni((LPCSTR)dsn_name, (LPCSTR)drv_nobrackets.c_str()))
  {
    std::cout << "Could not create a new DSN named " << dsn_name << std::endl;
    return FAIL;
  }

  char port[16] = { 0 };
  if (myport)
    snprintf(port, sizeof(port), "%d", myport);

  std::vector<std::vector<const char*>> params = {
    {"SERVER",      (const char*)myserver},
    {"USER",        (const char*)myuid   },
    {"PWD",         (const char*)mypwd   },
    {"DB",          (const char*)mydb    },
    {"SOCKET",      (const char*)mysock  },
    {"PORT",        (const char*)port    },
    {"NO_I_S",      (const char*)"1"     }
  };

  for (auto p : params)
  {
    if (p[1] && *p[1])
      ok_dsn(SQLWritePrivateProfileString((LPCSTR)dsn_name, p[0], p[1], "odbc.ini"));
  }

  rc = SQLConnect(hdbc1, (SQLCHAR*)dsn_name, SQL_NTS, nullptr, 0, nullptr, 0);
  // Driver must report the option deprecation
  is(rc == SQL_SUCCESS_WITH_INFO);
  print_diag(rc, SQL_HANDLE_DBC, hdbc1, "Expected Info in SQLConnect():",
             __FILE__, __LINE__);
  ok_con(hdbc1, SQLDisconnect(hdbc1));
  SQLRemoveDSNFromIni("wl14586");
  return OK;
}


BEGIN_TESTS
  ADD_TEST(t_wl13883)
  ADD_TEST(t_wl14490)
  ADD_TEST(t_wl14586)
  ADD_TEST(t_wl14362)
END_TESTS

RUN_TESTS

