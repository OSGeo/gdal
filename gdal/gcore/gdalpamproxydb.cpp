/******************************************************************************
 *
 * Project:  GDAL Core
 * Purpose:  Implementation of the GDAL PAM Proxy database interface.
 *           The proxy db is used to associate .aux.xml files in a temp
 *           directory - used for files for which aux.xml files can't be
 *           created (i.e. read-only file systems).
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
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

#include "cpl_port.h"
#include "gdal_pam.h"

#include <cerrno>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#if HAVE_FCNTL_H
#  include <fcntl.h>
#endif

#include <memory>
#include <string>
#include <vector>

#include "cpl_conv.h"
#include "cpl_error.h"
#include "cpl_multiproc.h"
#include "cpl_string.h"
#include "cpl_vsi.h"
#include "gdal_pam.h"
#include "ogr_spatialref.h"

CPL_CVSID("$Id$")

/************************************************************************/
/* ==================================================================== */
/*                            GDALPamProxyDB                            */
/* ==================================================================== */
/************************************************************************/

class GDALPamProxyDB
{
  public:
    GDALPamProxyDB() { nUpdateCounter = -1; }

    CPLString   osProxyDBDir;

    int         nUpdateCounter;

    std::vector<CPLString> aosOriginalFiles;
    std::vector<CPLString> aosProxyFiles;

    void        CheckLoadDB();
    void        LoadDB();
    void        SaveDB();
};

static bool bProxyDBInitialized = FALSE;
static GDALPamProxyDB *poProxyDB = NULL;
static CPLMutex *hProxyDBLock = NULL;

/************************************************************************/
/*                            CheckLoadDB()                             */
/*                                                                      */
/*      Eventually we want to check if the file has changed, and if     */
/*      so, force it to be reloaded.  TODO:                             */
/************************************************************************/

void GDALPamProxyDB::CheckLoadDB()

{
    if( nUpdateCounter == -1 )
        LoadDB();
}

/************************************************************************/
/*                               LoadDB()                               */
/*                                                                      */
/*      It is assumed the caller already holds the lock.                */
/************************************************************************/

void GDALPamProxyDB::LoadDB()

{
/* -------------------------------------------------------------------- */
/*      Open the database relating original names to proxy .aux.xml     */
/*      file names.                                                     */
/* -------------------------------------------------------------------- */
    CPLString osDBName =
        CPLFormFilename( osProxyDBDir, "gdal_pam_proxy", "dat" );
    VSILFILE *fpDB = VSIFOpenL( osDBName, "r" );

    nUpdateCounter = 0;
    if( fpDB == NULL )
        return;

/* -------------------------------------------------------------------- */
/*      Read header, verify and extract update counter.                 */
/* -------------------------------------------------------------------- */
    const size_t nHeaderSize = 100;
    GByte abyHeader[nHeaderSize] = { '\0' };

    if( VSIFReadL( abyHeader, 1, nHeaderSize, fpDB ) != nHeaderSize
        || !STARTS_WITH(reinterpret_cast<char *>(abyHeader), "GDAL_PROXY") )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "Problem reading %s header - short or corrupt?",
                  osDBName.c_str() );
        CPL_IGNORE_RET_VAL(VSIFCloseL(fpDB));
        return;
    }

    nUpdateCounter = atoi(reinterpret_cast<char *>(abyHeader) + 10);

/* -------------------------------------------------------------------- */
/*      Read the file in one gulp.                                      */
/* -------------------------------------------------------------------- */
    if( VSIFSeekL( fpDB, 0, SEEK_END ) != 0 )
    {
        CPL_IGNORE_RET_VAL(VSIFCloseL(fpDB));
        return;
    }
    const int nBufLength = static_cast<int>(VSIFTellL(fpDB) - nHeaderSize);
    if( VSIFSeekL( fpDB, nHeaderSize, SEEK_SET ) != 0 )
    {
        CPL_IGNORE_RET_VAL(VSIFCloseL(fpDB));
        return;
    }
    char *pszDBData = static_cast<char *>( CPLCalloc(1,nBufLength+1) );
    if( VSIFReadL( pszDBData, 1, nBufLength, fpDB ) !=
        static_cast<size_t>(nBufLength) )
    {
        CPLFree(pszDBData);
        CPL_IGNORE_RET_VAL(VSIFCloseL(fpDB));
        return;
    }

    CPL_IGNORE_RET_VAL(VSIFCloseL( fpDB ));

/* -------------------------------------------------------------------- */
/*      Parse the list of in/out names.                                 */
/* -------------------------------------------------------------------- */
    int iNext = 0;

    while( iNext < nBufLength )
    {
        CPLString osOriginal;
        osOriginal.assign( pszDBData + iNext );

        for( ; iNext < nBufLength && pszDBData[iNext] != '\0'; iNext++ ) {}

        if( iNext == nBufLength )
            break;

        iNext++;

        CPLString osProxy = osProxyDBDir;
        osProxy += "/";
        osProxy += pszDBData + iNext;

        for( ; iNext < nBufLength && pszDBData[iNext] != '\0'; iNext++ ) {}
        iNext++;

        aosOriginalFiles.push_back( osOriginal );
        aosProxyFiles.push_back( osProxy );
    }

    CPLFree( pszDBData );
}

/************************************************************************/
/*                               SaveDB()                               */
/************************************************************************/

void GDALPamProxyDB::SaveDB()

{
/* -------------------------------------------------------------------- */
/*      Open the database relating original names to proxy .aux.xml     */
/*      file names.                                                     */
/* -------------------------------------------------------------------- */
    CPLString osDBName =
        CPLFormFilename( osProxyDBDir, "gdal_pam_proxy", "dat" );

    void *hLock = CPLLockFile( osDBName, 1.0 );

    // proceed even if lock fails - we need CPLBreakLockFile()!
    if( hLock == NULL )
    {
        CPLError( CE_Warning, CPLE_AppDefined,
                  "GDALPamProxyDB::SaveDB() - "
                  "Failed to lock %s file, proceeding anyways.",
                  osDBName.c_str() );
    }

    VSILFILE *fpDB = VSIFOpenL( osDBName, "w" );
    if( fpDB == NULL )
    {
        if( hLock )
            CPLUnlockFile( hLock );
        CPLError( CE_Failure, CPLE_AppDefined,
                  "Failed to save %s Pam Proxy DB.\n%s",
                  osDBName.c_str(),
                  VSIStrerror( errno ) );
        return;
    }

/* -------------------------------------------------------------------- */
/*      Write header.                                                   */
/* -------------------------------------------------------------------- */
    const size_t nHeaderSize = 100;
    GByte abyHeader[nHeaderSize] = { '\0' };

    memset( abyHeader, ' ', sizeof(abyHeader) );
    memcpy( reinterpret_cast<char *>(abyHeader), "GDAL_PROXY", 10 );
    snprintf( reinterpret_cast<char *>(abyHeader) + 10, sizeof(abyHeader) - 10,
              "%9d", nUpdateCounter );

    if( VSIFWriteL( abyHeader, 1, nHeaderSize, fpDB ) != nHeaderSize )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "Failed to write complete %s Pam Proxy DB.\n%s",
                  osDBName.c_str(),
                  VSIStrerror( errno ) );
        CPL_IGNORE_RET_VAL(VSIFCloseL( fpDB ));
        VSIUnlink( osDBName );
        if( hLock )
            CPLUnlockFile( hLock );
        return;
    }

/* -------------------------------------------------------------------- */
/*      Write names.                                                    */
/* -------------------------------------------------------------------- */
    for( unsigned int i = 0; i < aosOriginalFiles.size(); i++ )
    {
        size_t nCount = VSIFWriteL( aosOriginalFiles[i].c_str(),
                    strlen(aosOriginalFiles[i].c_str())+1, 1, fpDB );

        const char *pszProxyFile = CPLGetFilename(aosProxyFiles[i]);
        nCount += VSIFWriteL( pszProxyFile,
                                    strlen(pszProxyFile)+1, 1, fpDB );

        if( nCount != 2 )
        {
            CPLError( CE_Failure, CPLE_AppDefined,
                      "Failed to write complete %s Pam Proxy DB.\n%s",
                      osDBName.c_str(),
                      VSIStrerror( errno ) );
            CPL_IGNORE_RET_VAL(VSIFCloseL( fpDB ));
            VSIUnlink( osDBName );
            if( hLock )
                CPLUnlockFile( hLock );
            return;
        }
    }

    if( VSIFCloseL( fpDB ) != 0 )
    {
        CPLError(CE_Failure, CPLE_FileIO, "I/O error");
    }

    if( hLock )
        CPLUnlockFile( hLock );
}

/************************************************************************/
/*                            InitProxyDB()                             */
/*                                                                      */
/*      Initialize ProxyDB (if it isn't already initialized).           */
/************************************************************************/

static void InitProxyDB()

{
    if( !bProxyDBInitialized )
    {
        CPLMutexHolderD( &hProxyDBLock );

        if( !bProxyDBInitialized )
        {
            const char *pszProxyDir =
                CPLGetConfigOption( "GDAL_PAM_PROXY_DIR", NULL );

            if( pszProxyDir )
            {
                poProxyDB = new GDALPamProxyDB();
                poProxyDB->osProxyDBDir = pszProxyDir;
            }
        }

        bProxyDBInitialized = true;
    }
}

/************************************************************************/
/*                          PamCleanProxyDB()                           */
/************************************************************************/

void PamCleanProxyDB()

{
    {
        CPLMutexHolderD( &hProxyDBLock );

        bProxyDBInitialized = false;

        delete poProxyDB;
        poProxyDB = NULL;
    }

    CPLDestroyMutex( hProxyDBLock );
    hProxyDBLock = NULL;
}

/************************************************************************/
/*                            PamGetProxy()                             */
/************************************************************************/

const char *PamGetProxy( const char *pszOriginal )

{
    InitProxyDB();

    if( poProxyDB == NULL )
        return NULL;

    CPLMutexHolderD( &hProxyDBLock );

    poProxyDB->CheckLoadDB();

    for( unsigned int i = 0; i < poProxyDB->aosOriginalFiles.size(); i++ )
    {
        if( strcmp( poProxyDB->aosOriginalFiles[i], pszOriginal ) == 0 )
            return poProxyDB->aosProxyFiles[i];
    }

    return NULL;
}

/************************************************************************/
/*                          PamAllocateProxy()                          */
/************************************************************************/

const char *PamAllocateProxy( const char *pszOriginal )

{
    InitProxyDB();

    if( poProxyDB == NULL )
        return NULL;

    CPLMutexHolderD( &hProxyDBLock );

    poProxyDB->CheckLoadDB();

/* -------------------------------------------------------------------- */
/*      Form the proxy filename based on the original path if           */
/*      possible, but dummy out any questionable characters, path       */
/*      delimiters and such.  This is intended to make the proxy        */
/*      name be identifiable by folks digging around in the proxy       */
/*      database directory.                                             */
/*                                                                      */
/*      We also need to be careful about length.                        */
/* -------------------------------------------------------------------- */
    CPLString osRevProxyFile;

    int i = static_cast<int>(strlen(pszOriginal)) - 1;
    while( i >= 0 && osRevProxyFile.size() < 220 )
    {
        if( i > 6 && STARTS_WITH_CI(pszOriginal+i-5, ":::OVR") )
            i -= 6;

        // make some effort to break long names at path delimiters.
        if( (pszOriginal[i] == '/' || pszOriginal[i] == '\\')
            && osRevProxyFile.size() > 200 )
            break;

        if( (pszOriginal[i] >= 'A' && pszOriginal[i] <= 'Z')
            || (pszOriginal[i] >= 'a' && pszOriginal[i] <= 'z')
            || (pszOriginal[i] >= '0' && pszOriginal[i] <= '9')
            || pszOriginal[i] == '.' )
            osRevProxyFile += pszOriginal[i];
        else
            osRevProxyFile += '_';

        i--;
    }

    CPLString osOriginal = pszOriginal;
    CPLString osProxy = poProxyDB->osProxyDBDir + "/";

    CPLString osCounter;
    osCounter.Printf( "%06d_", poProxyDB->nUpdateCounter++ );
    osProxy += osCounter;

    for( i = static_cast<int>(osRevProxyFile.size())-1; i >= 0; i-- )
        osProxy += osRevProxyFile[i];

    if( osOriginal.find(":::OVR") != CPLString::npos )
        osProxy += ".ovr";
    else
        osProxy += ".aux.xml";

/* -------------------------------------------------------------------- */
/*      Add the proxy and the original to the proxy list and resave     */
/*      the database.                                                   */
/* -------------------------------------------------------------------- */
    poProxyDB->aosOriginalFiles.push_back( osOriginal );
    poProxyDB->aosProxyFiles.push_back( osProxy );

    poProxyDB->SaveDB();

    return PamGetProxy( pszOriginal );
}
