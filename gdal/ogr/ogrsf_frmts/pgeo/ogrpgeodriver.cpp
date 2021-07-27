/******************************************************************************
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  Implements Personal Geodatabase driver.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2005, Frank Warmerdam <warmerdam@pobox.com>
 * Copyright (c) 2008-2013, Even Rouault <even dot rouault at spatialys.com>
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

#include "ogr_pgeo.h"
#include "cpl_conv.h"

CPL_CVSID("$Id$")

/************************************************************************/
/*                            ~OGRODBCDriver()                            */
/************************************************************************/

OGRPGeoDriver::~OGRPGeoDriver()

{
}

/************************************************************************/
/*                              GetName()                               */
/************************************************************************/

const char *OGRPGeoDriver::GetName()

{
    return "PGeo";
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

OGRDataSource *OGRPGeoDriver::Open( const char * pszFilename,
                                    int bUpdate )

{
    if( STARTS_WITH_CI(pszFilename, "WALK:") )
        return nullptr;

    if( STARTS_WITH_CI(pszFilename, "GEOMEDIA:") )
        return nullptr;

    if( !STARTS_WITH_CI(pszFilename, "PGEO:")
        && !EQUAL(CPLGetExtension(pszFilename),"mdb") )
        return nullptr;

    // Disabling the attempt to guess if a MDB file is a PGeo database
    // or not. The mention to GDB_GeomColumns might be quite far in
    // the/ file, which can cause misdetection.  See
    // http://trac.osgeo.org/gdal/ticket/4498 This was initially meant
    // to know if a MDB should be opened by the PGeo or the Geomedia
    // driver.
#if 0
    if( !STARTS_WITH_CI(pszFilename, "PGEO:") &&
        EQUAL(CPLGetExtension(pszFilename),"mdb") )
    {
        VSILFILE* fp = VSIFOpenL(pszFilename, "rb");
        if (!fp)
            return NULL;
        GByte* pabyHeader = (GByte*) CPLMalloc(100000);
        VSIFReadL(pabyHeader, 100000, 1, fp);
        VSIFCloseL(fp);

        /* Look for GDB_GeomColumns table */
        const GByte pabyNeedle[] = {
            'G', 0, 'D', 0, 'B', 0, '_', 0, 'G', 0, 'e', 0, 'o', 0, 'm', 0,
            'C', 0, 'o', 0, 'l', 0, 'u', 0, 'm', 0, 'n', 0, 's' };
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
        CPLDebug( "PGeo", "MDB Tools driver installed successfully!");

#endif /* ndef WIN32 */

    // Open data source
    OGRPGeoDataSource *poDS = new OGRPGeoDataSource();

    if( !poDS->Open( pszFilename, bUpdate, TRUE ) )
    {
        delete poDS;
        return nullptr;
    }
    else
        return poDS;
}

/************************************************************************/
/*                           TestCapability()                           */
/************************************************************************/

int OGRPGeoDriver::TestCapability( CPL_UNUSED const char * pszCap )
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

bool OGRODBCMDBDriver::InstallMdbDriver()
{
    if ( !FindDriverLib() )
    {
        return false;
    }
    else
    {
        CPLAssert( !osDriverFile.empty() );
        CPLDebug( GetName(), "MDB Tools driver: %s", osDriverFile.c_str() );

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

        if ( !dri.InstallDriver(driver.c_str(), nullptr, ODBC_INSTALL_COMPLETE) )
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

bool OGRODBCMDBDriver::FindDriverLib()
{
    // Default name and path of driver library
    const char* aszDefaultLibName[] = {
        "libmdbodbc.so",
        "libmdbodbc.so.0" /* for Ubuntu 8.04 support */
    };
    const int nLibNames = sizeof(aszDefaultLibName) / sizeof(aszDefaultLibName[0]);
    const char* libPath[] = {
        "/usr/lib/x86_64-linux-gnu/odbc", /* ubuntu 20.04 */
        "/usr/lib64",
        "/usr/local/lib64",
        "/usr/lib",
        "/usr/local/lib"
    };
    const int nLibPaths = sizeof(libPath) / sizeof(libPath[0]);

    const char* pszDrvCfg = CPLGetConfigOption("MDBDRIVER_PATH", nullptr);
    if ( nullptr != pszDrvCfg )
    {
        // Directory or file path
        CPLString strLibPath(pszDrvCfg);

        VSIStatBuf sStatBuf;
        if ( VSIStat( pszDrvCfg, &sStatBuf ) == 0
             && VSI_ISDIR( sStatBuf.st_mode ) )
        {
            // Find default library in custom directory
            const char* pszDriverFile = CPLFormFilename( pszDrvCfg, aszDefaultLibName[0], nullptr );
            CPLAssert( nullptr != pszDriverFile );

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
            const char* pszDriverFile = CPLFormFilename( libPath[i], aszDefaultLibName[j], nullptr );
            CPLAssert( nullptr != pszDriverFile );

            if ( LibraryExists( pszDriverFile ) )
            {
                // Save default driver path
                osDriverFile = pszDriverFile;
                return true;
            }
        }
    }

    CPLError(CE_Failure, CPLE_AppDefined, "%s: MDB Tools driver not found!\n", GetName());
    // Driver not found!
    return false;
}

/************************************************************************/
/*                           LibraryExists()                            */
/************************************************************************/

bool OGRODBCMDBDriver::LibraryExists(const char* pszLibPath)
{
    CPLAssert( nullptr != pszLibPath );

    VSIStatBuf stb;

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

void RegisterOGRPGeo()

{
    OGRSFDriver* poDriver = new OGRPGeoDriver;

    poDriver->SetMetadataItem( GDAL_DMD_LONGNAME, "ESRI Personal GeoDatabase" );
    poDriver->SetMetadataItem( GDAL_DMD_EXTENSION, "mdb" );
    poDriver->SetMetadataItem( GDAL_DMD_HELPTOPIC, "drivers/vector/pgeo.html" );
    poDriver->SetMetadataItem( GDAL_DCAP_MULTIPLE_VECTOR_LAYERS, "YES" );

    OGRSFDriverRegistrar::GetRegistrar()->RegisterDriver( poDriver );
}
