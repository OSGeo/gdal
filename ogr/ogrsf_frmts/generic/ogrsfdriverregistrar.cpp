/******************************************************************************
 * $Id$
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  The OGRSFDriverRegistrar class implementation.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 1999,  Les Technologies SoftMap Inc.
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
 ******************************************************************************
 *
 * $Log$
 * Revision 1.25  2006/10/30 16:06:54  hobu
 * switch off for now until we figure out a GetHome
 *
 * Revision 1.24  2006/10/30 15:14:46  hobu
 * turn on the $HOME finding stuff in AutoLoadDrivers and
 * make sure it looks for lib/gdalplugins.  (bug 1299)
 *
 * Revision 1.23  2005/10/28 16:47:02  fwarmerdam
 * Added OGRCleanupAll()
 *
 * Revision 1.22  2005/10/25 19:59:15  fwarmerdam
 * added driver tracking on datasource
 *
 * Revision 1.21  2005/08/10 13:49:43  fwarmerdam
 * fix logic for dll entry point.
 *
 * Revision 1.20  2005/06/10 15:38:10  fwarmerdam
 * avoid use of MAX_PATH
 *
 * Revision 1.19  2005/06/10 14:58:46  fwarmerdam
 * Added option to search under executable directory for plugins.
 *
 * Revision 1.18  2005/06/08 21:11:24  fwarmerdam
 * bug 865: fixed "ogr_" name test.
 *
 * Revision 1.17  2005/01/19 20:28:18  fwarmerdam
 * added untested autoloaddrivers.
 *
 * Revision 1.16  2004/10/17 04:04:05  fwarmerdam
 * fixed bug with cleanup order in ReleaseDataSource, issue with vrt
 *
 * Revision 1.15  2003/10/10 19:12:06  warmerda
 * Ensure registrar is created before trying to use it.
 *
 * Revision 1.14  2003/05/28 19:18:04  warmerda
 * fixup argument names for docs
 *
 * Revision 1.13  2003/05/20 19:07:03  warmerda
 * added GDAL_DATA support, use CPLGetConfigOption
 *
 * Revision 1.12  2003/04/08 21:23:33  warmerda
 * added OGRGetDriverByName
 *
 * Revision 1.11  2003/03/20 19:12:15  warmerda
 * added debug messages
 *
 * Revision 1.10  2003/03/19 20:37:09  warmerda
 * added OpenShared() mechanism
 *
 * Revision 1.9  2003/03/14 02:28:07  danmo
 * Prevent crash if poRegistrar==NULL in OGRGetDriverCount() and OGROpen()
 *
 * Revision 1.8  2002/09/26 18:16:19  warmerda
 * added C entry points
 *
 * Revision 1.7  2002/05/14 21:38:00  warmerda
 * make INST_DATA overidable with binary patch
 *
 * Revision 1.6  2001/07/18 04:55:16  warmerda
 * added CPL_CSVID
 *
 * Revision 1.5  2001/01/22 22:36:05  warmerda
 * expanded tabs
 *
 * Revision 1.4  2000/12/05 23:07:43  warmerda
 * Check for CE_Failure, not just an error being set.
 *
 * Revision 1.3  2000/08/30 09:13:34  warmerda
 * Set INST_DATA as FinderLocation
 *
 * Revision 1.2  1999/07/27 00:51:08  warmerda
 * added arg to get driver out of Open()
 *
 * Revision 1.1  1999/07/05 18:58:32  warmerda
 * New
 *
 */

#include "ogrsf_frmts.h"
#include "ogr_api.h"
#include "ogr_p.h"
#include "cpl_multiproc.h"

CPL_CVSID("$Id$");

static OGRSFDriverRegistrar *poRegistrar = NULL;

static char *pszUpdatableINST_DATA = 
"__INST_DATA_TARGET:                                                                                                                                      ";
/************************************************************************/
/*                         OGRSFDriverRegistrar                         */
/************************************************************************/

OGRSFDriverRegistrar::OGRSFDriverRegistrar()

{
    CPLAssert( poRegistrar == NULL );
    nDrivers = 0;
    papoDrivers = NULL;

    nOpenDSCount = 0;
    papszOpenDSRawName = NULL;
    papoOpenDS = NULL;
    papoOpenDSDriver = NULL;

/* -------------------------------------------------------------------- */
/*      We want to push a location to search for data files             */
/*      supporting GDAL/OGR such as EPSG csv files, S-57 definition     */
/*      files, and so forth.  The static pszUpdateableINST_DATA         */
/*      string can be updated within the shared library or              */
/*      executable during an install to point installed data            */
/*      directory.  If it isn't burned in here then we use the          */
/*      INST_DATA macro (setup at configure time) if                    */
/*      available. Otherwise we don't push anything and we hope         */
/*      other mechanisms such as environment variables will have        */
/*      been employed.                                                  */
/* -------------------------------------------------------------------- */
    if( CPLGetConfigOption( "GDAL_DATA", NULL ) != NULL )
    {
        CPLPushFinderLocation( CPLGetConfigOption( "GDAL_DATA", NULL ) );
    }
    else if( pszUpdatableINST_DATA[19] != ' ' )
    {
        CPLPushFinderLocation( pszUpdatableINST_DATA + 19 );
    }
    else
    {
#ifdef INST_DATA
        CPLPushFinderLocation( INST_DATA );
#endif
    }
}

/************************************************************************/
/*                       ~OGRSFDriverRegistrar()                        */
/************************************************************************/

OGRSFDriverRegistrar::~OGRSFDriverRegistrar()

{
    for( int i = 0; i < nDrivers; i++ )
    {
        delete papoDrivers[i];
    }

    CPLFree( papoDrivers );
    papoDrivers = NULL;

    poRegistrar = NULL;
}

/************************************************************************/
/*                           OGRCleanupAll()                            */
/************************************************************************/

void OGRCleanupAll()

{
    if( poRegistrar != NULL )
        delete poRegistrar;
    OSRCleanup();
    CPLFinderClean();
    VSICleanupFileManager();
    CPLFreeConfig();
    CPLCleanupTLS();
}


/************************************************************************/
/*                            GetRegistrar()                            */
/************************************************************************/

OGRSFDriverRegistrar *OGRSFDriverRegistrar::GetRegistrar()

{
    if( poRegistrar == NULL )
        poRegistrar = new OGRSFDriverRegistrar();

    return poRegistrar;
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

OGRDataSource *OGRSFDriverRegistrar::Open( const char * pszName,
                                           int bUpdate,
                                           OGRSFDriver ** ppoDriver )

{
    OGRDataSource       *poDS;

    if( ppoDriver != NULL )
        *ppoDriver = NULL;

    GetRegistrar();
    
    CPLErrorReset();

    for( int iDriver = 0; iDriver < poRegistrar->nDrivers; iDriver++ )
    {
        poDS = poRegistrar->papoDrivers[iDriver]->Open( pszName, bUpdate );
        if( poDS != NULL )
        {
            if( ppoDriver != NULL )
                *ppoDriver = poRegistrar->papoDrivers[iDriver];

            poDS->Reference();
            if( poDS->GetDriver() == NULL )
                poDS->m_poDriver = poRegistrar->papoDrivers[iDriver];

            CPLDebug( "OGR", "OGROpen(%s/%p) succeeded as %s.", 
                      pszName, poDS, poDS->GetDriver()->GetName() );
            
            return poDS;
        }

        if( CPLGetLastErrorType() == CE_Failure )
            return NULL;
    }

    CPLDebug( "OGR", "OGROpen(%s) failed.", pszName );
            
    return NULL;
}

/************************************************************************/
/*                              OGROpen()                               */
/************************************************************************/

OGRDataSourceH OGROpen( const char *pszName, int bUpdate,
                        OGRSFDriverH *pahDriverList )

{
    if (poRegistrar)
        return poRegistrar->Open( pszName, bUpdate, 
                                  (OGRSFDriver **) pahDriverList );

    return NULL;
}

/************************************************************************/
/*                             OpenShared()                             */
/************************************************************************/

OGRDataSource *
OGRSFDriverRegistrar::OpenShared( const char * pszName, int bUpdate,
                                  OGRSFDriver ** ppoDriver )

{
    OGRDataSource       *poDS;

    if( ppoDriver != NULL )
        *ppoDriver = NULL;

    CPLErrorReset();

/* -------------------------------------------------------------------- */
/*      First try finding an existing open dataset matching exactly     */
/*      on the original datasource raw name used to open the            */
/*      datasource.                                                     */
/*                                                                      */
/*      NOTE: It is an error, but currently we ignore the bUpdate,      */
/*      and return whatever is open even if it is read-only and the     */
/*      application requested update access.                            */
/* -------------------------------------------------------------------- */
    int iDS;

    for( iDS = 0; iDS < nOpenDSCount; iDS++ )
    {
        poDS = papoOpenDS[iDS];

        if( strcmp( pszName, papszOpenDSRawName[iDS]) == 0 )
        {
            poDS->Reference();

            if( ppoDriver != NULL )
                *ppoDriver = papoOpenDSDriver[iDS];
            return poDS;
        }
    }

/* -------------------------------------------------------------------- */
/*      If that doesn't match, try matching on the name returned by     */
/*      the datasource itself.                                          */
/* -------------------------------------------------------------------- */
    for( iDS = 0; iDS < nOpenDSCount; iDS++ )
    {
        poDS = papoOpenDS[iDS];

        if( strcmp( pszName, poDS->GetName()) == 0 )
        {
            poDS->Reference();

            if( ppoDriver != NULL )
                *ppoDriver = papoOpenDSDriver[iDS];
            return poDS;
        }
    }

/* -------------------------------------------------------------------- */
/*      We don't have the datasource.  Open it normally.                */
/* -------------------------------------------------------------------- */
    OGRSFDriver *poTempDriver = NULL;

    poDS = Open( pszName, bUpdate, &poTempDriver );

    if( poDS == NULL )
        return poDS;

/* -------------------------------------------------------------------- */
/*      We don't have this datasource already.  Grow our list to        */
/*      hold the new datasource.                                        */
/* -------------------------------------------------------------------- */
    papszOpenDSRawName = (char **) 
        CPLRealloc( papszOpenDSRawName, sizeof(char*) * (nOpenDSCount+1) );
    
    papoOpenDS = (OGRDataSource **) 
        CPLRealloc( papoOpenDS, sizeof(char*) * (nOpenDSCount+1) );
    
    papoOpenDSDriver = (OGRSFDriver **) 
        CPLRealloc( papoOpenDSDriver, sizeof(char*) * (nOpenDSCount+1) );

    papszOpenDSRawName[nOpenDSCount] = CPLStrdup( pszName );
    papoOpenDS[nOpenDSCount] = poDS;
    papoOpenDSDriver[nOpenDSCount] = poTempDriver;

    nOpenDSCount++;

    if( ppoDriver != NULL )
        *ppoDriver = poTempDriver;

    return poDS;
}

/************************************************************************/
/*                           OGROpenShared()                            */
/************************************************************************/

OGRDataSourceH OGROpenShared( const char *pszName, int bUpdate,
                              OGRSFDriverH *pahDriverList )

{
    OGRSFDriverRegistrar::GetRegistrar();
    return poRegistrar->OpenShared( pszName, bUpdate, 
                                    (OGRSFDriver **) pahDriverList );
}

/************************************************************************/
/*                         ReleaseDataSource()                          */
/************************************************************************/

OGRErr OGRSFDriverRegistrar::ReleaseDataSource( OGRDataSource * poDS )

{
    int iDS;

    for( iDS = 0; iDS < nOpenDSCount; iDS++ )
    {
        if( poDS == papoOpenDS[iDS] )
            break;
    }

    if( iDS == nOpenDSCount )
    {
        CPLDebug( "OGR", 
                  "ReleaseDataSource(%s/%p) on unshared datasource!\n"
                  "Deleting directly.", 
                  poDS->GetName(), poDS );
        delete poDS;
        return OGRERR_FAILURE;
    }

    if( poDS->GetRefCount() > 0 )
        poDS->Dereference();

    if( poDS->GetRefCount() > 0 )
    {
        CPLDebug( "OGR", 
                  "ReleaseDataSource(%s/%p) ... just dereferencing.",
                  poDS->GetName(), poDS );
        return OGRERR_NONE;
    }

    if( poDS->GetSummaryRefCount() > 0 )
    {
        CPLDebug( "OGR", 
                  "OGRSFDriverRegistrar::ReleaseDataSource(%s)\n"
                  "Datasource reference count is now zero, but some layers\n"
                  "are still referenced ... not closing datasource.",
                  poDS->GetName() );
        return OGRERR_FAILURE;
    }

/* -------------------------------------------------------------------- */
/*      We really want to close this file, and remove it from the       */
/*      shared list.                                                    */
/* -------------------------------------------------------------------- */
    CPLDebug( "OGR", 
              "ReleaseDataSource(%s/%p) dereferenced and now destroying.",
              poDS->GetName(), poDS );

    CPLFree( papszOpenDSRawName[iDS] );
    memmove( papszOpenDSRawName + iDS, papszOpenDSRawName + iDS + 1, 
             sizeof(char *) * (nOpenDSCount - iDS - 1) );
    memmove( papoOpenDS + iDS, papoOpenDS + iDS + 1, 
             sizeof(char *) * (nOpenDSCount - iDS - 1) );
    memmove( papoOpenDSDriver + iDS, papoOpenDSDriver + iDS + 1, 
             sizeof(char *) * (nOpenDSCount - iDS - 1) );

    nOpenDSCount--;

    if( nOpenDSCount == 0 )
    {
        CPLFree( papszOpenDSRawName );
        papszOpenDSRawName = NULL;
        CPLFree( papoOpenDS );
        papoOpenDS = NULL;
        CPLFree( papoOpenDSDriver );
        papoOpenDSDriver = NULL;
    }

    // We are careful to only do the delete poDS after adjusting the
    // table, as if it is a virtual dataset, other removals may happen
    // in the meantime.
    delete poDS;

    return OGRERR_NONE;
}

/************************************************************************/
/*                        OGRReleaseDataSource()                        */
/************************************************************************/

OGRErr OGRReleaseDataSource( OGRDataSourceH hDS )

{
    OGRSFDriverRegistrar::GetRegistrar();
    return poRegistrar->ReleaseDataSource((OGRDataSource *) hDS);
}

/************************************************************************/
/*                         OGRGetOpenDSCount()                          */
/************************************************************************/

int OGRGetOpenDSCount()

{
    OGRSFDriverRegistrar::GetRegistrar();
    return poRegistrar->GetOpenDSCount();
}

/************************************************************************/
/*                             GetOpenDS()                              */
/************************************************************************/

OGRDataSource *OGRSFDriverRegistrar::GetOpenDS( int iDS )

{
    if( iDS < 0 || iDS >= nOpenDSCount )
        return NULL;
    else
        return papoOpenDS[iDS];
}

/************************************************************************/
/*                            OGRGetOpenDS()                            */
/************************************************************************/

OGRDataSourceH OGRGetOpenDS( int iDS )

{
    OGRSFDriverRegistrar::GetRegistrar();
    return (OGRDataSourceH) poRegistrar->GetOpenDS( iDS );
}

/************************************************************************/
/*                           RegisterDriver()                           */
/************************************************************************/

void OGRSFDriverRegistrar::RegisterDriver( OGRSFDriver * poDriver )

{
    int         iDriver;

/* -------------------------------------------------------------------- */
/*      It has no effect to register a driver more than once.           */
/* -------------------------------------------------------------------- */
    for( iDriver = 0; iDriver < nDrivers; iDriver++ )
    {
        if( poDriver == papoDrivers[iDriver] )
            return;
    }                                                   

/* -------------------------------------------------------------------- */
/*      Add to the end of the driver list.                              */
/* -------------------------------------------------------------------- */
    papoDrivers = (OGRSFDriver **)
        CPLRealloc( papoDrivers, (nDrivers+1) * sizeof(void*) );

    papoDrivers[nDrivers++] = poDriver;
}

/************************************************************************/
/*                         OGRRegisterDriver()                          */
/************************************************************************/

void OGRRegisterDriver( OGRSFDriverH hDriver )

{
    OGRSFDriverRegistrar::GetRegistrar()->RegisterDriver( 
        (OGRSFDriver *) hDriver );
}

/************************************************************************/
/*                           GetDriverCount()                           */
/************************************************************************/

int OGRSFDriverRegistrar::GetDriverCount()

{
    return nDrivers;
}

/************************************************************************/
/*                         OGRGetDriverCount()                          */
/************************************************************************/

int OGRGetDriverCount()

{
    if (poRegistrar)
        return poRegistrar->GetDriverCount();

    return 0;
}

/************************************************************************/
/*                             GetDriver()                              */
/************************************************************************/

OGRSFDriver *OGRSFDriverRegistrar::GetDriver( int iDriver )

{
    if( iDriver < 0 || iDriver >= nDrivers )
        return NULL;
    else
        return papoDrivers[iDriver];
}

/************************************************************************/
/*                            OGRGetDriver()                            */
/************************************************************************/

OGRSFDriverH OGRGetDriver( int iDriver )

{
    return (OGRSFDriverH) poRegistrar->GetDriver( iDriver );
}

/************************************************************************/
/*                          GetDriverByName()                           */
/************************************************************************/

OGRSFDriver *OGRSFDriverRegistrar::GetDriverByName( const char * pszName )

{
    for( int i = 0; i < nDrivers; i++ )
    {
        if( papoDrivers[i] != NULL 
            && EQUAL(papoDrivers[i]->GetName(),pszName) )
            return papoDrivers[i];
    }

    return NULL;
}

/************************************************************************/
/*                         OGRGetDriverByName()                         */
/************************************************************************/

OGRSFDriverH OGRGetDriverByName( const char *pszName )

{
    ;
    return (OGRSFDriverH) 
        OGRSFDriverRegistrar::GetRegistrar()->GetDriverByName( pszName );
}

/************************************************************************/
/*                          AutoLoadDrivers()                           */
/************************************************************************/

/**
 * Auto-load GDAL drivers from shared libraries.
 *
 * This function will automatically load drivers from shared libraries.  It
 * searches the "driver path" for .so (or .dll) files that start with the
 * prefix "gdal_X.so".  It then tries to load them and then tries to call
 * a function within them called GDALRegister_X() where the 'X' is the same 
 * as the remainder of the shared library basename, or failing that to 
 * call GDALRegisterMe().  
 *
 * There are a few rules for the driver path.  If the GDAL_DRIVER_PATH
 * environment variable it set, it is taken to be a list of directories
 * to search separated by colons on unix, or semi-colons on Windows.  Otherwise
 * the /usr/local/lib/gdalplugins directory, and (if known) the lib/gdalplugins
 * subdirectory of the gdal home directory are searched. 
 */

void OGRSFDriverRegistrar::AutoLoadDrivers()

{
    char     **papszSearchPath = NULL;
    const char *pszGDAL_DRIVER_PATH = 
        CPLGetConfigOption( "OGR_DRIVER_PATH", NULL );

    if( pszGDAL_DRIVER_PATH == NULL )
        pszGDAL_DRIVER_PATH = 
            CPLGetConfigOption( "GDAL_DRIVER_PATH", NULL );

/* -------------------------------------------------------------------- */
/*      Where should we look for stuff?                                 */
/* -------------------------------------------------------------------- */
    if( pszGDAL_DRIVER_PATH != NULL )
    {
#ifdef WIN32
        papszSearchPath = 
            CSLTokenizeStringComplex( pszGDAL_DRIVER_PATH, ";", TRUE, FALSE );
#else
        papszSearchPath = 
            CSLTokenizeStringComplex( pszGDAL_DRIVER_PATH, ":", TRUE, FALSE );
#endif
    }
    else
    {
#ifdef GDAL_PREFIX
        papszSearchPath = CSLAddString( papszSearchPath, 
                                        GDAL_PREFIX "/lib/gdalplugins" );
#else
        char szExecPath[1024];

        if( CPLGetExecPath( szExecPath, sizeof(szExecPath) ) )
        {
            char szPluginDir[sizeof(szExecPath)+50];
            strcpy( szPluginDir, CPLGetDirname( szExecPath ) );
            strcat( szPluginDir, "\\gdalplugins\\" );
            papszSearchPath = CSLAddString( papszSearchPath, szPluginDir );
        }
        else
        {
            papszSearchPath = CSLAddString( papszSearchPath, 
                                            "/usr/local/lib/gdalplugins" );
        }
#endif

#ifdef notdef
        if( strlen(GetHome()) > 0 )
        {
            papszSearchPath = CSLAddString( papszSearchPath, 
                                  CPLFormFilename( GetHome(), "lib/gdalplugins", NULL ) );
        }
#endif
    }

/* -------------------------------------------------------------------- */
/*      Scan each directory looking for files starting with gdal_       */
/* -------------------------------------------------------------------- */
    for( int iDir = 0; iDir < CSLCount(papszSearchPath); iDir++ )
    {
        char  **papszFiles = CPLReadDir( papszSearchPath[iDir] );

        for( int iFile = 0; iFile < CSLCount(papszFiles); iFile++ )
        {
            char   *pszFuncName;
            const char *pszFilename;
            const char *pszExtension = CPLGetExtension( papszFiles[iFile] );
            void   *pRegister;

            if( !EQUALN(papszFiles[iFile],"ogr_",4) )
                continue;

            if( !EQUAL(pszExtension,"dll") 
                && !EQUAL(pszExtension,"so") 
                && !EQUAL(pszExtension,"dylib") )
                continue;

            pszFuncName = (char *) CPLCalloc(strlen(papszFiles[iFile])+20,1);
            sprintf( pszFuncName, "RegisterOGR%s", 
                     CPLGetBasename(papszFiles[iFile]) + 4 );
            
            pszFilename = 
                CPLFormFilename( papszSearchPath[iDir], 
                                 papszFiles[iFile], NULL );

            pRegister = CPLGetSymbol( pszFilename, pszFuncName );
            if( pRegister == NULL )
            {
                strcpy( pszFuncName, "GDALRegisterMe" );
                pRegister = CPLGetSymbol( pszFilename, pszFuncName );
            }
            
            if( pRegister != NULL )
            {
                CPLDebug( "OGR", "Auto register %s using %s.", 
                          pszFilename, pszFuncName );

                ((void (*)()) pRegister)();
            }

            CPLFree( pszFuncName );
        }

        CSLDestroy( papszFiles );
    }

    CSLDestroy( papszSearchPath );
}
