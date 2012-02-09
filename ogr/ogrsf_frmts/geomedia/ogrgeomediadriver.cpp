/******************************************************************************
 * $Id$
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  Implements Personal Geodatabase driver.
 * Author:   Even Rouault, <even dot rouault at mines dash paris dot org>
 *
 ******************************************************************************
 * Copyright (c) 2011, Even Rouault, <even dot rouault at mines dash paris dot org>
 * Copyright (c) 2005, Frank Warmerdam <warmerdam@pobox.com>
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

#include "ogr_geomedia.h"
#include "cpl_conv.h"

CPL_CVSID("$Id$");

/************************************************************************/
/*                          ~OGRODBCDriver()                            */
/************************************************************************/

OGRGeomediaDriver::~OGRGeomediaDriver()

{
}

/************************************************************************/
/*                              GetName()                               */
/************************************************************************/

const char *OGRGeomediaDriver::GetName()

{
    return "Geomedia";
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

OGRDataSource *OGRGeomediaDriver::Open( const char * pszFilename,
                                    int bUpdate )

{
    OGRGeomediaDataSource     *poDS;

    if( !EQUALN(pszFilename,"GEOMEDIA:",9) 
        && !EQUAL(CPLGetExtension(pszFilename),"mdb") )
        return NULL;

    /* Disabling the attempt to guess if a MDB file is a Geomedia database */
    /* or not. See similar fix in PGeo driver for rationale. */
#if 0
    if( !EQUALN(pszFilename,"GEOMEDIA:",9) &&
        EQUAL(CPLGetExtension(pszFilename),"mdb") )
    {
        VSILFILE* fp = VSIFOpenL(pszFilename, "rb");
        if (!fp)
            return NULL;
        GByte* pabyHeader = (GByte*) CPLMalloc(100000);
        VSIFReadL(pabyHeader, 100000, 1, fp);
        VSIFCloseL(fp);

        /* Look for GAliasTable table */
        const GByte pabyNeedle[] = { 'G', 0, 'A', 0, 'l', 0, 'i', 0, 'a', 0, 's', 0, 'T', 0, 'a', 0, 'b', 0, 'l', 0, 'e'};
        int bFound = FALSE;
        for(int i=0;i<100000 - (int)sizeof(pabyNeedle);i++)
        {
            if (memcmp(pabyHeader + i, pabyNeedle, sizeof(pabyNeedle)) == 0)
            {
                bFound = TRUE;
                break;
            }
        }
        CPLFree(pabyHeader);
        if (!bFound)
            return NULL;
    }
#endif

#ifndef WIN32
    // Try to register MDB Tools driver
    //
    // ODBCINST.INI NOTE:
    // This operation requires write access to odbcinst.ini file
    // located in directory pointed by ODBCINISYS variable.
    // Usually, it points to /etc, so non-root users can overwrite this
    // setting ODBCINISYS with location they have write access to, e.g.:
    // $ export ODBCINISYS=$HOME/etc
    // $ touch $ODBCINISYS/odbcinst.ini
    //
    // See: http://www.unixodbc.org/internals.html
    //
    if ( !InstallMdbDriver() )
    {
        CPLError( CE_Warning, CPLE_AppDefined, 
                  "Unable to install MDB driver for ODBC, MDB access may not supported.\n" );
    }
    else
        CPLDebug( "Geomedia", "MDB Tools driver installed successfully!");

#endif /* ndef WIN32 */

    // Open data source
    poDS = new OGRGeomediaDataSource();

    if( !poDS->Open( pszFilename, bUpdate, TRUE ) )
    {
        delete poDS;
        return NULL;
    }
    else
        return poDS;
}

/************************************************************************/
/*                           TestCapability()                           */
/************************************************************************/

int OGRGeomediaDriver::TestCapability( const char * pszCap )

{
    return FALSE;
}


/*
 * START OF UNIX-only features.
 */
#ifndef WIN32

/************************************************************************/
/*                           InstallMdbDriver()                         */
/************************************************************************/

bool OGRGeomediaDriver::InstallMdbDriver()
{
    if ( !FindDriverLib() )
    {
        return false;
    }
    else
    {
        CPLAssert( !osDriverFile.empty() );
        CPLDebug( "Geomedia", "MDB Tools driver: %s", osDriverFile.c_str() );

        CPLString driverName("Microsoft Access Driver (*.mdb)");
        CPLString driver(driverName);
        driver += '\0';
        driver += "Driver=";
        driver += osDriverFile; // Found by FindDriverLib()
        driver += '\0';
        driver += "FileUsage=1";
        driver += '\0';
        driver += '\0';

        // Create installer and register driver
        CPLODBCDriverInstaller dri;

        if ( !dri.InstallDriver(driver.c_str(), 0, ODBC_INSTALL_COMPLETE) )
        {
            // Report ODBC error
            CPLError( CE_Failure, CPLE_AppDefined, "ODBC: %s", dri.GetLastError() );
            return false;
        }
    }

    return true;
}

/************************************************************************/
/*                           FindDriverLib()                            */
/************************************************************************/

bool OGRGeomediaDriver::FindDriverLib()
{
    // Default name and path of driver library
    const char* aszDefaultLibName[] = {
        "libmdbodbc.so",
        "libmdbodbc.so.0" /* for Ubuntu 8.04 support */
    };
    const int nLibNames = sizeof(aszDefaultLibName) / sizeof(aszDefaultLibName[0]);
    const char* libPath[] = { 
        "/usr/lib",
        "/usr/local/lib"
    };
    const int nLibPaths = sizeof(libPath) / sizeof(libPath[0]);

    CPLString strLibPath("");

    const char* pszDrvCfg = CPLGetConfigOption("MDBDRIVER_PATH", NULL);
    if ( NULL != pszDrvCfg )
    {
        // Directory or file path
        strLibPath = pszDrvCfg;

        VSIStatBuf sStatBuf = { 0 };
        if ( VSIStat( pszDrvCfg, &sStatBuf ) == 0
             && VSI_ISDIR( sStatBuf.st_mode ) ) 
        {
            // Find default library in custom directory
            const char* pszDriverFile = CPLFormFilename( pszDrvCfg, aszDefaultLibName[0], NULL );
            CPLAssert( 0 != pszDriverFile );
        
            strLibPath = pszDriverFile;
        }

        if ( LibraryExists( strLibPath.c_str() ) )
        {
            // Save custom driver path
            osDriverFile = strLibPath;
            return true;
        }
    }

    // Try to find library in default path
    for ( int i = 0; i < nLibPaths; i++ )
    {
        for ( int j = 0; j < nLibNames; j++ )
        {
            const char* pszDriverFile = CPLFormFilename( libPath[i], aszDefaultLibName[j], NULL );
            CPLAssert( 0 != pszDriverFile );

            if ( LibraryExists( pszDriverFile ) )
            {
                // Save default driver path
                osDriverFile = pszDriverFile;
                return true;
            }
        }
    }

    CPLError(CE_Failure, CPLE_AppDefined, "Geomedia: MDB Tools driver not found!\n");
    // Driver not found!
    return false;
}

/************************************************************************/
/*                           LibraryExists()                            */
/************************************************************************/

bool OGRGeomediaDriver::LibraryExists(const char* pszLibPath)
{
    CPLAssert( 0 != pszLibPath );

    VSIStatBuf stb = { 0 } ;

    if ( 0 == VSIStat( pszLibPath, &stb ) )
    {
        if (VSI_ISREG( stb.st_mode ) || VSI_ISLNK(stb.st_mode))
        {
            return true;
        }
    }

    return false;
}

#endif /* ndef WIN32 */
/*
 * END OF UNIX-only features
 */

/************************************************************************/
/*                           RegisterOGRODBC()                          */
/************************************************************************/

void RegisterOGRGeomedia()

{
    OGRSFDriverRegistrar::GetRegistrar()->RegisterDriver( new OGRGeomediaDriver );
}

