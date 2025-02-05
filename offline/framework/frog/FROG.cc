#include "FROG.h"

#include <phool/phool.h>

#include <odbc++/connection.h>
#include <odbc++/drivermanager.h>
#include <odbc++/resultset.h>
#include <odbc++/statement.h>  // for Statement
#include <odbc++/types.h>      // for SQLException

#include <boost/tokenizer.hpp>

#include <chrono>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <string>
#include <thread>

using namespace std;

const char *
FROG::location(const string &logical_name)
{
  pfn = logical_name;
  if (logical_name.empty() || logical_name.find("/") != string::npos)
  {
    if (Verbosity() > 0)
    {
      if (logical_name.empty())
      {
        cout << "FROG: empty string as filename" << endl;
      }
      else if (logical_name.find("/") != string::npos)
      {
        cout << "FROG: found / in filename, assuming it contains a full path" << endl;
      }
    }
    return pfn.c_str();
  }
  try
  {
    string gsearchpath(getenv("GSEARCHPATH"));
    if (Verbosity() > 0)
    {
      cout << "FROG: GSEARCHPATH: " << gsearchpath << endl;
    }
    boost::char_separator<char> sep(":");
    boost::tokenizer<boost::char_separator<char> > tok(gsearchpath, sep);
    for (auto &iter : tok)
    {
      if (iter == "PG")
      {
        if (Verbosity() > 1)
        {
          cout << "Searching FileCatalog for disk resident file "
               << logical_name << endl;
        }
        if (PGSearch(logical_name))
        {
          if (Verbosity() > 1)
          {
            cout << "Found " << logical_name << " in FileCatalog, returning "
                 << pfn << endl;
          }
          break;
        }
      }
      else if (iter == "DCACHE")
      {
        if (Verbosity() > 1)
        {
          cout << "Searching FileCatalog for dCache file "
               << logical_name << endl;
        }
        if (dCacheSearch(logical_name))
        {
          if (Verbosity() > 1)
          {
            cout << "Found " << logical_name << " in dCache, returning "
                 << pfn << endl;
          }
          break;
        }
      }
      else if (iter == "XROOTD")
      {
        if (Verbosity() > 1)
        {
          cout << "Searching FileCatalog for XRootD file "
               << logical_name << endl;
        }
        if (XRootDSearch(logical_name))
        {
          if (Verbosity() > 1)
          {
            cout << "Found " << logical_name << " in XRootD, returning "
                 << pfn << endl;
          }
          break;
        }
      }
      else  // assuming this is a file path
      {
        if (Verbosity() > 0)
        {
          cout << "Trying path " << iter << endl;
        }
        string fullfile = iter + "/" + logical_name;
        if (localSearch(fullfile))
        {
          break;
        }
      }
    }
  }
  catch (...)
  {
    if (Verbosity() > 0)
    {
      cout << "FROG: GSEARCHPATH not set " << endl;
    }
  }
  Disconnect();
  return pfn.c_str();
}

bool FROG::localSearch(const string &logical_name)
{
  if (std::ifstream(logical_name))
  {
    pfn = logical_name;
    return true;
  }
  return false;
}

bool FROG::GetConnection()
{
  if (m_OdbcConnection)
  {
    return true;
  }
  int icount = 0;
  do
  {
    try
    {
      m_OdbcConnection = odbc::DriverManager::getConnection("FileCatalog", "argouser", "Brass_Ring");
      return true;
    }
    catch (odbc::SQLException &e)
    {
      cout << PHWHERE
           << " Exception caught during DriverManager::getConnection" << endl;
      cout << "Message: " << e.getMessage() << endl;
    }
    icount++;
    std::this_thread::sleep_for(std::chrono::seconds(30));  // sleep 30 seconds before retry
  } while (icount < 5);
  return false;
}

void FROG::Disconnect()
{
  delete m_OdbcConnection;
  m_OdbcConnection = nullptr;
}

bool FROG::PGSearch(const string &lname)
{
  bool bret = false;
  if (!GetConnection())
  {
    return bret;
  }
  string sqlquery = "SELECT full_file_path from files where lfn='" + lname + "' and full_host_name <> 'hpss' and full_host_name <> 'dcache'";

  odbc::Statement *stmt = m_OdbcConnection->createStatement();
  odbc::ResultSet *rs = stmt->executeQuery(sqlquery);

  if (rs->next())
  {
    pfn = rs->getString(1);
    bret = true;
  }
  delete rs;
  delete stmt;
  return bret;
}

bool FROG::dCacheSearch(const string &lname)
{
  bool bret = false;
  if (!GetConnection())
  {
    return bret;
  }
  string sqlquery = "SELECT full_file_path from files where lfn='" + lname + "' and full_host_name = 'dcache'";

  odbc::Statement *stmt = m_OdbcConnection->createStatement();
  odbc::ResultSet *rs = stmt->executeQuery(sqlquery);

  if (rs->next())
  {
    string dcachefile = rs->getString(1);
    if (std::ifstream(dcachefile))
    {
      pfn = "dcache:" + dcachefile;
      bret = true;
    }
  }
  delete rs;
  delete stmt;
  return bret;
}

bool FROG::XRootDSearch(const string &lname)
{
  bool bret = false;
  if (!GetConnection())
  {
    return bret;
  }
  string sqlquery = "SELECT full_file_path from files where lfn='" + lname + "' and full_host_name = 'dcache'";

  odbc::Statement *stmt = m_OdbcConnection->createStatement();
  odbc::ResultSet *rs = stmt->executeQuery(sqlquery);

  if (rs->next())
  {
    string xrootdfile = rs->getString(1);
    if (std::ifstream(xrootdfile))
    {
      pfn = "root://dcsphdoor02.rcf.bnl.gov:1095" + xrootdfile;
      bret = true;
    }
  }
  delete rs;
  delete stmt;
  return bret;
}
