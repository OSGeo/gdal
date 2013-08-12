/******************************************************************************
 * $Id$
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  Implements FileGDB OGR driver.
 * Author:   Ragi Yaser Burhum, ragi@burhum.com
 *           Paul Ramsey, pramsey at cleverelephant.ca
 *
 ******************************************************************************
 * Copyright (c) 2010, Ragi Yaser Burhum
 * Copyright (c) 2011, Paul Ramsey <pramsey at cleverelephant.ca>
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

CPL_CVSID("$Id$");

extern "C" void RegisterOGRFileGDB();

/************************************************************************/
/*                            FGdbDriver()                              */
/************************************************************************/
FGdbDriver::FGdbDriver():
OGRSFDriver()
{
}

/************************************************************************/
/*                            ~FGdbDriver()                             */
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

OGRDataSource *FGdbDriver::Open( const char* pszFilename, int bUpdate )

{
    // First check if we have to do any work.
    int nLen = strlen(pszFilename);
    if(! ((nLen >= 4 && EQUAL(pszFilename + nLen - 4, ".gdb")) ||
          (nLen >= 5 && EQUAL(pszFilename + nLen - 5, ".gdb/"))) )
        return NULL;

    long hr;

    /* Check that the filename is really a directory, to avoid confusion with */
    /* Garmin MapSource - gdb format which can be a problem when the FileGDB */
    /* driver is loaded as a plugin, and loaded before the GPSBabel driver */
    /* (http://trac.osgeo.org/osgeo4w/ticket/245) */
    VSIStatBuf stat;
    if( CPLStat( pszFilename, &stat ) != 0 || !VSI_ISDIR(stat.st_mode) )
    {
        return NULL;
    }

    Geodatabase* pGeoDatabase = NULL;

    FGdbDatabaseConnection* pConnection = oMapConnections[pszFilename];
    if( pConnection != NULL )
    {
        pGeoDatabase = pConnection->m_pGeodatabase;
        pConnection->m_nRefCount ++;
        CPLDebug("FileGDB", "ref_count of %s = %d now", pszFilename,
                 pConnection->m_nRefCount);
    }
    else
    {
        pGeoDatabase = new Geodatabase;
        hr = ::OpenGeodatabase(StringToWString(pszFilename), *pGeoDatabase);

        if (FAILED(hr) || pGeoDatabase == NULL)
        {
            delete pGeoDatabase;

            GDBErr(hr, "Failed to open Geodatabase");
            return NULL;
        }

        CPLDebug("FileGDB", "Really opening %s", pszFilename);
        oMapConnections[pszFilename] = new FGdbDatabaseConnection(pGeoDatabase);
    }

    FGdbDataSource* pDS;

    pDS = new FGdbDataSource(this);

    if(!pDS->Open( pGeoDatabase, pszFilename, bUpdate ) )
    {
        delete pDS;
        return NULL;
    }
    else
        return pDS;
}

/***********************************************************************/
/*                     CreateDataSource()                              */
/***********************************************************************/

OGRDataSource* FGdbDriver::CreateDataSource( const char * conn,
                                           char **papszOptions)
{
    long hr;
    Geodatabase *pGeodatabase;
    std::wstring wconn = StringToWString(conn);
    int bUpdate = TRUE; // If we're creating, we must be writing.
    VSIStatBuf stat;

    /* We don't support options yet, so warn if they send us some */
    if ( papszOptions )
    {
        /* TODO: warning, ignoring options */
    }

    /* Only accept names of form "filename.gdb" and */
    /* also .gdb.zip to be able to return FGDB with MapServer OGR output (#4199) */
    const char* pszExt = CPLGetExtension(conn);
    if ( !(EQUAL(pszExt,"gdb") || EQUAL(pszExt, "zip")) )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "FGDB data source name must use 'gdb' extension.\n" );
        return NULL;
    }

    /* Don't try to create on top of something already there */
    if( CPLStat( conn, &stat ) == 0 )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "%s already exists.\n", conn );
        return NULL;
    }

    /* Try to create the geodatabase */
    pGeodatabase = new Geodatabase; // Create on heap so we can store it in the Datasource
    hr = CreateGeodatabase(wconn, *pGeodatabase);

    /* Handle creation errors */
    if ( S_OK != hr )
    {
        const char *errstr = "Error creating geodatabase (%s).\n";
        if ( hr == -2147220653 )
            errstr = "File already exists (%s).\n";
        delete pGeodatabase;
        CPLError( CE_Failure, CPLE_AppDefined, errstr, conn );
        return NULL;
    }

    oMapConnections[conn] = new FGdbDatabaseConnection(pGeodatabase);

    /* Ready to embed the Geodatabase in an OGR Datasource */
    FGdbDataSource* pDS = new FGdbDataSource(this);
    if ( ! pDS->Open(pGeodatabase, conn, bUpdate) )
    {
        delete pDS;
        return NULL;
    }
    else
        return pDS;
}

/***********************************************************************/
/*                            Release()                                */
/***********************************************************************/

void FGdbDriver::Release(const char* pszName)
{
    FGdbDatabaseConnection* pConnection = oMapConnections[pszName];
    if( pConnection != NULL )
    {
        pConnection->m_nRefCount --;
        CPLDebug("FileGDB", "ref_count of %s = %d now", pszName,
                 pConnection->m_nRefCount);
        if( pConnection->m_nRefCount == 0 )
        {
            CPLDebug("FileGDB", "Really closing %s now", pszName);
            ::CloseGeodatabase(*(pConnection->m_pGeodatabase));
            delete pConnection->m_pGeodatabase;
            pConnection->m_pGeodatabase = NULL;
            delete pConnection;
            oMapConnections.erase(pszName);
        }
    }
}

/***********************************************************************/
/*                         TestCapability()                            */
/***********************************************************************/

int FGdbDriver::TestCapability( const char * pszCap )
{
    if (EQUAL(pszCap, ODrCCreateDataSource) )
        return TRUE;

    else if (EQUAL(pszCap, ODrCDeleteDataSource) )
        return TRUE;

    return FALSE;
}
/************************************************************************/
/*                          DeleteDataSource()                          */
/************************************************************************/

OGRErr FGdbDriver::DeleteDataSource( const char *pszDataSource )
{

    std::wstring wstr = StringToWString(pszDataSource);

    long hr;

    if (S_OK != (hr = ::DeleteGeodatabase(wstr)))
    {
        GDBErr(hr, "Failed to delete Geodatabase");
        return OGRERR_FAILURE;
    }

    return OGRERR_NONE;
}

/***********************************************************************/
/*                       RegisterOGRFileGDB()                          */
/***********************************************************************/

void RegisterOGRFileGDB()

{
    if (! GDAL_CHECK_VERSION("OGR FGDB"))
        return;
    OGRSFDriverRegistrar::GetRegistrar()->RegisterDriver( new FGdbDriver );
}

