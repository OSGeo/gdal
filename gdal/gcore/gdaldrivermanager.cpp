/******************************************************************************
 *
 * Project:  GDAL Core
 * Purpose:  Implementation of GDALDriverManager class.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 1998, Frank Warmerdam
 * Copyright (c) 2009-2013, Even Rouault <even dot rouault at mines-paris dot org>
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

#include "cpl_port.h"
#include "gdal_priv.h"

#include <cstring>
#include <map>

#include "cpl_conv.h"
#include "cpl_error.h"
#include "cpl_multiproc.h"
#include "cpl_port.h"
#include "cpl_string.h"
#include "cpl_vsi.h"
#include "gdal_alg.h"
#include "gdal_alg_priv.h"
#include "gdal.h"
#include "gdal_pam.h"
#include "ogr_srs_api.h"
#include "ograpispy.h"
#ifdef HAVE_XERCES
#  include "ogr_xerces.h"
#endif  // HAVE_XERCES

#ifdef _MSC_VER
#  ifdef MSVC_USE_VLD
#    include <wchar.h>
#    include <vld.h>
#  endif
#endif

// FIXME: Disabled following code as it crashed on OSX CI test.
//#if HAVE_CXX11
//#include <mutex>
//#endif

CPL_CVSID("$Id$")

/************************************************************************/
/* ==================================================================== */
/*                           GDALDriverManager                          */
/* ==================================================================== */
/************************************************************************/

static volatile GDALDriverManager *poDM = NULL;
static CPLMutex *hDMMutex = NULL;

// FIXME: Disabled following code as it crashed on OSX CI test.
//#if HAVE_CXX11
//static std::mutex oDeleteMutex;
//#endif

CPLMutex** GDALGetphDMMutex() { return &hDMMutex; }

/************************************************************************/
/*                        GetGDALDriverManager()                        */
/*                                                                      */
/*      A freestanding function to get the only instance of the         */
/*      GDALDriverManager.                                              */
/************************************************************************/

/**
 * \brief Fetch the global GDAL driver manager.
 *
 * This function fetches the pointer to the singleton global driver manager.
 * If the driver manager doesn't exist it is automatically created.
 *
 * @return pointer to the global driver manager.  This should not be able
 * to fail.
 */

GDALDriverManager * GetGDALDriverManager()

{
    if( poDM == NULL )
    {
        CPLMutexHolderD( &hDMMutex );

        if( poDM == NULL )
            poDM = new GDALDriverManager();
    }

    CPLAssert( NULL != poDM );

    return const_cast<GDALDriverManager *>( poDM );
}

/************************************************************************/
/*                         GDALDriverManager()                          */
/************************************************************************/

GDALDriverManager::GDALDriverManager() :
    nDrivers(0),
    papoDrivers(NULL)
{
    CPLAssert( poDM == NULL );

/* -------------------------------------------------------------------- */
/*      We want to push a location to search for data files             */
/*      supporting GDAL/OGR such as EPSG csv files, S-57 definition     */
/*      files, and so forth.  Use the INST_DATA macro (setup at         */
/*      configure time) if available. Otherwise we don't push anything  */
/*      and we hope other mechanisms such as environment variables will */
/*      have been employed.                                             */
/* -------------------------------------------------------------------- */
#ifdef INST_DATA
    if( CPLGetConfigOption( "GDAL_DATA", NULL ) != NULL )
    {
        // This one is picked up automatically by finder initialization.
    }
    else
    {
        CPLPushFinderLocation( INST_DATA );
    }
#endif
}

/************************************************************************/
/*                         ~GDALDriverManager()                         */
/************************************************************************/

// Keep these two in sync with gdalproxypool.cpp.
void GDALDatasetPoolPreventDestroy();
void GDALDatasetPoolForceDestroy();

GDALDriverManager::~GDALDriverManager()

{
/* -------------------------------------------------------------------- */
/*      Cleanup any open datasets.                                      */
/* -------------------------------------------------------------------- */

    // We have to prevent the destroying of the dataset pool during this first
    // phase, otherwise it cause crashes with a VRT B referencing a VRT A, and
    // if CloseDependentDatasets() is called first on VRT A.
    // If we didn't do this nasty trick, due to the refCountOfDisableRefCount
    // mechanism that cheats the real refcount of the dataset pool, we might
    // destroy the dataset pool too early, leading the VRT A to
    // destroy itself indirectly ... Ok, I am aware this explanation does
    // not make any sense unless you try it under a debugger ...
    // When people just manipulate "top-level" dataset handles, we luckily
    // don't need this horrible hack, but GetOpenDatasets() expose "low-level"
    // datasets, which defeat some "design" of the proxy pool.
    GDALDatasetPoolPreventDestroy();

    // First begin by requesting each remaining dataset to drop any reference
    // to other datasets.
    bool bHasDroppedRef = false;

    do
    {
        int nDSCount = 0;
        GDALDataset **papoDSList = GDALDataset::GetOpenDatasets(&nDSCount);

        // If a dataset has dropped a reference, the list might have become
        // invalid, so go out of the loop and try again with the new valid
        // list.
        bHasDroppedRef = false;
        for( int i = 0; i < nDSCount && !bHasDroppedRef; ++i )
        {
#if DEBUG_VERBOSE
            CPLDebug( "GDAL", "Call CloseDependentDatasets() on %s",
                      papoDSList[i]->GetDescription() );
#endif  // DEBUG_VERBOSE
            bHasDroppedRef =
                CPL_TO_BOOL(papoDSList[i]->CloseDependentDatasets());
        }
    } while(bHasDroppedRef);

    // Now let's destroy the dataset pool. Nobody should use it afterwards
    // if people have well released their dependent datasets above.
    GDALDatasetPoolForceDestroy();

    // Now close the stand-alone datasets.
    int nDSCount = 0;
    GDALDataset **papoDSList = GDALDataset::GetOpenDatasets(&nDSCount);
    for( int i = 0; i < nDSCount; ++i )
    {
        CPLDebug( "GDAL",
                  "Force close of %s (%p) in GDALDriverManager cleanup.",
                  papoDSList[i]->GetDescription(), papoDSList[i] );
        // Destroy with delete operator rather than GDALClose() to force
        // deletion of datasets with multiple reference count.
        // We could also iterate while GetOpenDatasets() returns a non NULL
        // list.
        delete papoDSList[i];
    }

/* -------------------------------------------------------------------- */
/*      Destroy the existing drivers.                                   */
/* -------------------------------------------------------------------- */
    while( GetDriverCount() > 0 )
    {
        GDALDriver *poDriver = GetDriver(0);

        DeregisterDriver(poDriver);
        delete poDriver;
    }

    delete GDALGetAPIPROXYDriver();

/* -------------------------------------------------------------------- */
/*      Cleanup local memory.                                           */
/* -------------------------------------------------------------------- */
    VSIFree( papoDrivers );

/* -------------------------------------------------------------------- */
/*      Cleanup any Proxy related memory.                               */
/* -------------------------------------------------------------------- */
    PamCleanProxyDB();

/* -------------------------------------------------------------------- */
/*      Blow away all the finder hints paths.  We really should not     */
/*      be doing all of them, but it is currently hard to keep track    */
/*      of those that actually belong to us.                            */
/* -------------------------------------------------------------------- */
    CPLFinderClean();
    CPLFreeConfig();
    CPLCleanupSharedFileMutex();

/* -------------------------------------------------------------------- */
/*      Cleanup any memory allocated by the OGRSpatialReference         */
/*      related subsystem.                                              */
/* -------------------------------------------------------------------- */
    OSRCleanup();

#ifdef HAVE_XERCES
    OGRCleanupXercesMutex();
#endif

#ifdef OGRAPISPY_ENABLED
    OGRAPISpyDestroyMutex();
#endif

/* -------------------------------------------------------------------- */
/*      Cleanup VSIFileManager.                                         */
/* -------------------------------------------------------------------- */
    VSICleanupFileManager();

/* -------------------------------------------------------------------- */
/*      Cleanup thread local storage ... I hope the program is all      */
/*      done with GDAL/OGR!                                             */
/* -------------------------------------------------------------------- */
    CPLCleanupTLS();

/* -------------------------------------------------------------------- */
/*      Cleanup our mutex.                                              */
/* -------------------------------------------------------------------- */
    if( hDMMutex )
    {
        CPLDestroyMutex( hDMMutex );
        hDMMutex = NULL;
    }

/* -------------------------------------------------------------------- */
/*      Cleanup dataset list mutex.                                     */
/* -------------------------------------------------------------------- */
    if( *GDALGetphDLMutex() != NULL )
    {
        CPLDestroyMutex( *GDALGetphDLMutex() );
        *GDALGetphDLMutex() = NULL;
    }

/* -------------------------------------------------------------------- */
/*      Cleanup raster block mutex.                                     */
/* -------------------------------------------------------------------- */
    GDALRasterBlock::DestroyRBMutex();

/* -------------------------------------------------------------------- */
/*      Cleanup gdaltransformer.cpp mutex.                              */
/* -------------------------------------------------------------------- */
    GDALCleanupTransformDeserializerMutex();

/* -------------------------------------------------------------------- */
/*      Cleanup cpl_error.cpp mutex.                                    */
/* -------------------------------------------------------------------- */
    CPLCleanupErrorMutex();

/* -------------------------------------------------------------------- */
/*      Cleanup CPLsetlocale mutex.                                     */
/* -------------------------------------------------------------------- */
    CPLCleanupSetlocaleMutex();

/* -------------------------------------------------------------------- */
/*      Cleanup QHull mutex.                                            */
/* -------------------------------------------------------------------- */
    GDALTriangulationTerminate();

/* -------------------------------------------------------------------- */
/*      Cleanup the master CPL mutex, which governs the creation        */
/*      of all other mutexes.                                           */
/* -------------------------------------------------------------------- */
    CPLCleanupMasterMutex();

/* -------------------------------------------------------------------- */
/*      Ensure the global driver manager pointer is NULLed out.         */
/* -------------------------------------------------------------------- */
    if( poDM == this )
        poDM = NULL;
}

/************************************************************************/
/*                           GetDriverCount()                           */
/************************************************************************/

/**
 * \brief Fetch the number of registered drivers.
 *
 * This C analog to this is GDALGetDriverCount().
 *
 * @return the number of registered drivers.
 */

int GDALDriverManager::GetDriverCount() const

{
    return nDrivers;
}

/************************************************************************/
/*                         GDALGetDriverCount()                         */
/************************************************************************/

/**
 * \brief Fetch the number of registered drivers.
 *
 * @see GDALDriverManager::GetDriverCount()
 */

int CPL_STDCALL GDALGetDriverCount()

{
    return GetGDALDriverManager()->GetDriverCount();
}

/************************************************************************/
/*                             GetDriver()                              */
/************************************************************************/

/**
 * \brief Fetch driver by index.
 *
 * This C analog to this is GDALGetDriver().
 *
 * @param iDriver the driver index from 0 to GetDriverCount()-1.
 *
 * @return the driver identified by the index or NULL if the index is invalid
 */

GDALDriver * GDALDriverManager::GetDriver( int iDriver )

{
    CPLMutexHolderD( &hDMMutex );

    return GetDriver_unlocked(iDriver);
}

/************************************************************************/
/*                           GDALGetDriver()                            */
/************************************************************************/

/**
 * \brief Fetch driver by index.
 *
 * @see GDALDriverManager::GetDriver()
 */

GDALDriverH CPL_STDCALL GDALGetDriver( int iDriver )

{
    return /* (GDALDriverH) */ GetGDALDriverManager()->GetDriver(iDriver);
}

/************************************************************************/
/*                           RegisterDriver()                           */
/************************************************************************/

/**
 * \brief Register a driver for use.
 *
 * The C analog is GDALRegisterDriver().
 *
 * Normally this method is used by format specific C callable registration
 * entry points such as GDALRegister_GTiff() rather than being called
 * directly by application level code.
 *
 * If this driver (based on the object pointer, not short name) is already
 * registered, then no change is made, and the index of the existing driver
 * is returned.  Otherwise the driver list is extended, and the new driver
 * is added at the end.
 *
 * @param poDriver the driver to register.
 *
 * @return the index of the new installed driver.
 */

int GDALDriverManager::RegisterDriver( GDALDriver * poDriver )

{
    CPLMutexHolderD( &hDMMutex );

/* -------------------------------------------------------------------- */
/*      If it is already registered, just return the existing           */
/*      index.                                                          */
/* -------------------------------------------------------------------- */
    if( GetDriverByName_unlocked( poDriver->GetDescription() ) != NULL )
    {
        for( int i = 0; i < nDrivers; ++i )
        {
            if( papoDrivers[i] == poDriver )
            {
                return i;
            }
        }

        CPLAssert( false );
    }

/* -------------------------------------------------------------------- */
/*      Otherwise grow the list to hold the new entry.                  */
/* -------------------------------------------------------------------- */
    GDALDriver** papoNewDrivers = static_cast<GDALDriver **>(
        VSI_REALLOC_VERBOSE(papoDrivers, sizeof(GDALDriver *) * (nDrivers+1)) );
    if( papoNewDrivers == NULL )
        return -1;
    papoDrivers = papoNewDrivers;

    papoDrivers[nDrivers] = poDriver;
    ++nDrivers;

    if( poDriver->pfnOpen != NULL ||
        poDriver->pfnOpenWithDriverArg != NULL )
        poDriver->SetMetadataItem( GDAL_DCAP_OPEN, "YES" );

    if( poDriver->pfnCreate != NULL )
        poDriver->SetMetadataItem( GDAL_DCAP_CREATE, "YES" );

    if( poDriver->pfnCreateCopy != NULL )
        poDriver->SetMetadataItem( GDAL_DCAP_CREATECOPY, "YES" );

    // Backward compatibility for GDAL raster out-of-tree drivers:
    // If a driver hasn't explicitly set a vector capability, assume it is
    // a raster-only driver (legacy OGR drivers will have DCAP_VECTOR set before
    // calling RegisterDriver()).
    if( poDriver->GetMetadataItem( GDAL_DCAP_RASTER ) == NULL &&
        poDriver->GetMetadataItem( GDAL_DCAP_VECTOR ) == NULL &&
        poDriver->GetMetadataItem( GDAL_DCAP_GNM ) == NULL )
    {
        CPLDebug( "GDAL", "Assuming DCAP_RASTER for driver %s. Please fix it.",
                  poDriver->GetDescription() );
        poDriver->SetMetadataItem( GDAL_DCAP_RASTER, "YES" );
    }

    if( poDriver->GetMetadataItem( GDAL_DMD_OPENOPTIONLIST ) != NULL &&
        poDriver->pfnIdentify == NULL &&
        !STARTS_WITH_CI(poDriver->GetDescription(), "Interlis") )
    {
        CPLDebug( "GDAL",
                  "Driver %s that defines GDAL_DMD_OPENOPTIONLIST must also "
                  "implement Identify(), so that it can be used",
                  poDriver->GetDescription() );
    }

    oMapNameToDrivers[CPLString(poDriver->GetDescription()).toupper()] =
        poDriver;

    int iResult = nDrivers - 1;

    return iResult;
}

/************************************************************************/
/*                         GDALRegisterDriver()                         */
/************************************************************************/

/**
 * \brief Register a driver for use.
 *
 * @see GDALDriverManager::GetRegisterDriver()
 */

int CPL_STDCALL GDALRegisterDriver( GDALDriverH hDriver )

{
    VALIDATE_POINTER1( hDriver, "GDALRegisterDriver", 0 );

    return GetGDALDriverManager()->
        RegisterDriver( static_cast<GDALDriver *>( hDriver ) );
}

/************************************************************************/
/*                          DeregisterDriver()                          */
/************************************************************************/

/**
 * \brief Deregister the passed driver.
 *
 * If the driver isn't found no change is made.
 *
 * The C analog is GDALDeregisterDriver().
 *
 * @param poDriver the driver to deregister.
 */

void GDALDriverManager::DeregisterDriver( GDALDriver * poDriver )

{
    CPLMutexHolderD( &hDMMutex );

    int i = 0;  // Used after for.
    for( ; i < nDrivers; ++i )
    {
        if( papoDrivers[i] == poDriver )
            break;
    }

    if( i == nDrivers )
        return;

    oMapNameToDrivers.erase(CPLString(poDriver->GetDescription()).toupper());
    --nDrivers;
    // Move all following drivers down by one to pack the list.
    while( i < nDrivers )
    {
        papoDrivers[i] = papoDrivers[i+1];
        ++i;
    }
}

/************************************************************************/
/*                        GDALDeregisterDriver()                        */
/************************************************************************/

/**
 * \brief Deregister the passed driver.
 *
 * @see GDALDriverManager::GetDeregisterDriver()
 */

void CPL_STDCALL GDALDeregisterDriver( GDALDriverH hDriver )

{
    VALIDATE_POINTER0( hDriver, "GDALDeregisterDriver" );

    GetGDALDriverManager()->DeregisterDriver( (GDALDriver *) hDriver );
}

/************************************************************************/
/*                          GetDriverByName()                           */
/************************************************************************/

/**
 * \brief Fetch a driver based on the short name.
 *
 * The C analog is the GDALGetDriverByName() function.
 *
 * @param pszName the short name, such as GTiff, being searched for.
 *
 * @return the identified driver, or NULL if no match is found.
 */

GDALDriver * GDALDriverManager::GetDriverByName( const char * pszName )

{
    CPLMutexHolderD( &hDMMutex );

    // Alias old name to new name
    if( EQUAL(pszName, "CartoDB") )
        pszName = "Carto";

    return oMapNameToDrivers[CPLString(pszName).toupper()];
}

/************************************************************************/
/*                        GDALGetDriverByName()                         */
/************************************************************************/

/**
 * \brief Fetch a driver based on the short name.
 *
 * @see GDALDriverManager::GetDriverByName()
 */

GDALDriverH CPL_STDCALL GDALGetDriverByName( const char * pszName )

{
    VALIDATE_POINTER1( pszName, "GDALGetDriverByName", NULL );

    return GetGDALDriverManager()->GetDriverByName( pszName );
}

/************************************************************************/
/*                          AutoSkipDrivers()                           */
/************************************************************************/

/**
 * \brief This method unload undesirable drivers.
 *
 * All drivers specified in the comma delimited list in the GDAL_SKIP
 * environment variable) will be deregistered and destroyed.  This method
 * should normally be called after registration of standard drivers to allow
 * the user a way of unloading undesired drivers.  The GDALAllRegister()
 * function already invokes AutoSkipDrivers() at the end, so if that functions
 * is called, it should not be necessary to call this method from application
 * code.
 *
 * Note: space separator is also accepted for backward compatibility, but some
 * vector formats have spaces in their names, so it is encouraged to use comma
 * to avoid issues.
 */

void GDALDriverManager::AutoSkipDrivers()

{
    char **apapszList[2] = { NULL, NULL };
    const char* pszGDAL_SKIP = CPLGetConfigOption( "GDAL_SKIP", NULL );
    if( pszGDAL_SKIP != NULL )
    {
        // Favor comma as a separator. If not found, then use space.
        const char* pszSep = (strchr(pszGDAL_SKIP, ',') != NULL) ? "," : " ";
        apapszList[0] =
            CSLTokenizeStringComplex( pszGDAL_SKIP, pszSep, FALSE, FALSE );
    }
    const char* pszOGR_SKIP = CPLGetConfigOption( "OGR_SKIP", NULL );
    if( pszOGR_SKIP != NULL )
    {
        // OGR has always used comma as a separator.
        apapszList[1] = CSLTokenizeStringComplex(pszOGR_SKIP, ",", FALSE, FALSE);
    }

    for( int j = 0; j < 2; ++j )
    {
        for( int i = 0; apapszList[j] != NULL && apapszList[j][i] != NULL; ++i )
        {
            GDALDriver * const poDriver = GetDriverByName( apapszList[j][i] );

            if( poDriver == NULL )
            {
                CPLError( CE_Warning, CPLE_AppDefined,
                          "Unable to find driver %s to unload from GDAL_SKIP "
                          "environment variable.",
                        apapszList[j][i] );
            }
            else
            {
                CPLDebug( "GDAL", "AutoSkipDriver(%s)", apapszList[j][i] );
                DeregisterDriver( poDriver );
                delete poDriver;
            }
        }
    }

    CSLDestroy( apapszList[0] );
    CSLDestroy( apapszList[1] );
}

/************************************************************************/
/*                          AutoLoadDrivers()                           */
/************************************************************************/

/**
 * \brief Auto-load GDAL drivers from shared libraries.
 *
 * This function will automatically load drivers from shared libraries.  It
 * searches the "driver path" for .so (or .dll) files that start with the
 * prefix "gdal_X.so".  It then tries to load them and then tries to call a
 * function within them called GDALRegister_X() where the 'X' is the same as
 * the remainder of the shared library basename ('X' is case sensitive), or
 * failing that to call GDALRegisterMe().
 *
 * There are a few rules for the driver path.  If the GDAL_DRIVER_PATH
 * environment variable it set, it is taken to be a list of directories to
 * search separated by colons on UNIX, or semi-colons on Windows.  Otherwise
 * the /usr/local/lib/gdalplugins directory, and (if known) the
 * lib/gdalplugins subdirectory of the gdal home directory are searched on
 * UNIX and $(BINDIR)\\gdalplugins on Windows.
 *
 * Auto loading can be completely disabled by setting the GDAL_DRIVER_PATH
 * config option to "disable".
 */

void GDALDriverManager::AutoLoadDrivers()

{
#ifdef GDAL_NO_AUTOLOAD
    CPLDebug( "GDAL", "GDALDriverManager::AutoLoadDrivers() not compiled in." );
#else
    const char *pszGDAL_DRIVER_PATH =
        CPLGetConfigOption( "GDAL_DRIVER_PATH", NULL );
    if( pszGDAL_DRIVER_PATH == NULL )
        pszGDAL_DRIVER_PATH = CPLGetConfigOption( "OGR_DRIVER_PATH", NULL );

/* -------------------------------------------------------------------- */
/*      Allow applications to completely disable this search by         */
/*      setting the driver path to the special string "disable".        */
/* -------------------------------------------------------------------- */
    if( pszGDAL_DRIVER_PATH != NULL && EQUAL(pszGDAL_DRIVER_PATH,"disable"))
    {
        CPLDebug( "GDAL", "GDALDriverManager::AutoLoadDrivers() disabled." );
        return;
    }

/* -------------------------------------------------------------------- */
/*      Where should we look for stuff?                                 */
/* -------------------------------------------------------------------- */
    char **papszSearchPath = NULL;

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
    #ifdef MACOSX_FRAMEWORK
                                        GDAL_PREFIX "/PlugIns");
    #else
                                        GDAL_PREFIX "/lib/gdalplugins" );
    #endif
#else
        char szExecPath[1024];

        if( CPLGetExecPath( szExecPath, sizeof(szExecPath) ) )
        {
            char szPluginDir[sizeof(szExecPath)+50];
            strcpy( szPluginDir, CPLGetDirname( szExecPath ) );
            strcat( szPluginDir, "\\gdalplugins" );
            papszSearchPath = CSLAddString( papszSearchPath, szPluginDir );
        }
        else
        {
            papszSearchPath = CSLAddString( papszSearchPath,
                                            "/usr/local/lib/gdalplugins" );
        }
#endif

   #ifdef MACOSX_FRAMEWORK
   #define num2str(x) str(x)
   #define str(x) #x
     papszSearchPath = CSLAddString( papszSearchPath,
                                     "/Library/Application Support/GDAL/"
                                     num2str(GDAL_VERSION_MAJOR) "."
                                     num2str(GDAL_VERSION_MINOR) "/PlugIns" );
#endif
    }

/* -------------------------------------------------------------------- */
/*      Format the ABI version specific subdirectory to look in.        */
/* -------------------------------------------------------------------- */
    CPLString osABIVersion;

    osABIVersion.Printf( "%d.%d", GDAL_VERSION_MAJOR, GDAL_VERSION_MINOR );

/* -------------------------------------------------------------------- */
/*      Scan each directory looking for files starting with gdal_       */
/* -------------------------------------------------------------------- */
    for( int iDir = 0; iDir < CSLCount(papszSearchPath); ++iDir )
    {
        CPLString osABISpecificDir =
            CPLFormFilename( papszSearchPath[iDir], osABIVersion, NULL );

        VSIStatBufL sStatBuf;
        if( VSIStatL( osABISpecificDir, &sStatBuf ) != 0 )
            osABISpecificDir = papszSearchPath[iDir];

        char **papszFiles = VSIReadDir( osABISpecificDir );
        const int nFileCount = CSLCount(papszFiles);

        for( int iFile = 0; iFile < nFileCount; ++iFile )
        {
            const char *pszExtension = CPLGetExtension( papszFiles[iFile] );

            if( !EQUAL(pszExtension,"dll")
                && !EQUAL(pszExtension,"so")
                && !EQUAL(pszExtension,"dylib") )
                continue;

            char *pszFuncName;
            if( STARTS_WITH_CI(papszFiles[iFile], "gdal_") )
            {
                pszFuncName = (char *) CPLCalloc(strlen(papszFiles[iFile])+20,1);
                snprintf( pszFuncName,
                          strlen(papszFiles[iFile])+20,
                          "GDALRegister_%s",
                        CPLGetBasename(papszFiles[iFile]) + strlen("gdal_") );
            }
            else if( STARTS_WITH_CI(papszFiles[iFile], "ogr_") )
            {
                pszFuncName = (char *) CPLCalloc(strlen(papszFiles[iFile])+20,1);
                snprintf( pszFuncName,
                         strlen(papszFiles[iFile])+20,
                         "RegisterOGR%s",
                         CPLGetBasename(papszFiles[iFile]) + strlen("ogr_") );
            }
            else
                continue;

            const char *pszFilename
                = CPLFormFilename( osABISpecificDir,
                                   papszFiles[iFile], NULL );

            CPLErrorReset();
            CPLPushErrorHandler(CPLQuietErrorHandler);
            void *pRegister = CPLGetSymbol( pszFilename, pszFuncName );
            CPLPopErrorHandler();
            if( pRegister == NULL )
            {
                CPLString osLastErrorMsg(CPLGetLastErrorMsg());
                strcpy( pszFuncName, "GDALRegisterMe" );
                pRegister = CPLGetSymbol( pszFilename, pszFuncName );
                if( pRegister == NULL )
                {
                    CPLError( CE_Failure, CPLE_AppDefined,
                              "%s", osLastErrorMsg.c_str() );
                }
            }

            if( pRegister != NULL )
            {
                CPLDebug( "GDAL", "Auto register %s using %s.",
                          pszFilename, pszFuncName );

                ((void (*)()) pRegister)();
            }

            CPLFree( pszFuncName );
        }

        CSLDestroy( papszFiles );
    }

    CSLDestroy( papszSearchPath );

#endif  // GDAL_NO_AUTOLOAD
}

/************************************************************************/
/*                      GDALDestroyDriverManager()                      */
/************************************************************************/

/**
 * \brief Destroy the driver manager.
 *
 * Incidentally unloads all managed drivers.
 *
 * NOTE: This function is not thread safe.  It should not be called while
 * other threads are actively using GDAL.
 */

void CPL_STDCALL GDALDestroyDriverManager( void )

{
    // THREADSAFETY: We would like to lock the mutex here, but it
    // needs to be reacquired within the destructor during driver
    // deregistration.

// FIXME: Disable following code as it crashed on OSX CI test.
//#if HAVE_CXX11
//    std::lock_guard<std::mutex> oLock(oDeleteMutex);
//#endif

    if( poDM != NULL )
    {
        delete poDM;
        poDM = NULL;
    }
}
