/******************************************************************************
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  Implements FileGDB OGR driver.
 * Author:   Ragi Yaser Burhum, ragi@burhum.com
 *
 ******************************************************************************
 * Copyright (c) 2010, Ragi Yaser Burhum
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 ****************************************************************************/

#include "ogr_fgdb.h"
#include "cpl_conv.h"
#include "FGdbUtils.h"

//g++ -Wall -g ogr/ogrsf_frmts/filegdb/*.c* -shared -o ogr_FileGDB.so -Iport -Igcore -Iogr -Iogr/ogrsf_frmts -Iogr/ogrsf_frmts/filegdb -L. -lgdal -I/home/even/filegdb/dist/include -L/home/even/filegdb/dist/lib  -I/home/even/filegdb/dist/src/FileGDBEngine/include/FileGDBLinux -lFileGDBAPI
extern "C" void RegisterOGRFileGDB();

/************************************************************************/
/*                            FGdbDriver()                            */
/************************************************************************/
FGdbDriver::FGdbDriver():
OGRSFDriver()
{
}

/************************************************************************/
/*                            ~FGdbDriver()                            */
/************************************************************************/
FGdbDriver::~FGdbDriver()

{
}


/************************************************************************/
/*                              GetName()                               */
/************************************************************************/

const char *FGdbDriver::GetName()

{
  return "FileGDB";
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

OGRDataSource *FGdbDriver::Open( const char* pszFilename,
                              int bUpdate )

{
  // First check if we have to do any work.
  if (!EQUAL(CPLGetExtension(pszFilename), "gdb"))
    return NULL;

  long hr;

  Geodatabase* pGeoDatabase = new Geodatabase;

  hr = ::OpenGeodatabase(StringToWString(pszFilename), *pGeoDatabase);

  if (FAILED(hr) || pGeoDatabase == NULL)
  {
    delete pGeoDatabase;

    GDBErr(hr, "Failed to open Geodatabase");
    return NULL;
  }

  FGdbDataSource* pDS;

  pDS = new FGdbDataSource();

  if(!pDS->Open( pGeoDatabase, pszFilename, bUpdate ) )
  {
    delete pDS;
    return NULL;
  }
  else
    return pDS;
}

/************************************************************************
*                     CreateDataSource()                                *
************************************************************************/

OGRDataSource* FGdbDriver::CreateDataSource( const char * pszName,
                                           char **papszOptions)
{
  return NULL;
}


void FGdbDriver::OpenGeodatabase(std::string conn, Geodatabase** ppGeodatabase)
{
  *ppGeodatabase = NULL;
  
  std::wstring wconn = StringToWString(conn); 

  long hr;

  Geodatabase* pGeoDatabase = new Geodatabase;

  if (S_OK != (hr = ::OpenGeodatabase(wconn, *pGeoDatabase)))
  {
    delete pGeoDatabase;

    return;
  }

  *ppGeodatabase = pGeoDatabase;

}

/************************************************************************
*                           TestCapability()                           *
************************************************************************/

int FGdbDriver::TestCapability( const char * pszCap )
{

  
  /*
  if (EQUAL(pszCap, ODsCCreateLayer) )
    return FALSE;
    */
  if (EQUAL(pszCap, ODsCDeleteLayer) )
    return TRUE;
  /*
  if (EQUAL(pszCap, ODrCCreateDataSource) )
    return FALSE;
  */

  return FALSE;
}

/************************************************************************
*                           RegisterOGRGdb()                                 *
************************************************************************/

void RegisterOGRfilegdb()

{
  if (! GDAL_CHECK_VERSION("OGR FGDB"))
    return;
  OGRSFDriverRegistrar::GetRegistrar()->RegisterDriver( new FGdbDriver );
}

