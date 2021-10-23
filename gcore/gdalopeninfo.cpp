/******************************************************************************
 *
 * Project:  GDAL Core
 * Purpose:  Implementation of GDALOpenInfo class.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 **********************************************************************
 * Copyright (c) 2002, Frank Warmerdam
 * Copyright (c) 2008-2012, Even Rouault <even dot rouault at spatialys.com>
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

#include "gdal_priv.h"  // Must be included first for mingw VSIStatBufL.
#include "cpl_port.h"

#include <cstdlib>
#include <cstring>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#include <algorithm>
#include <map>
#include <mutex>
#include <vector>

#include "cpl_config.h"
#include "cpl_conv.h"
#include "cpl_error.h"
#include "cpl_string.h"
#include "cpl_vsi.h"
#include "gdal.h"

CPL_CVSID("$Id$")

// Keep in sync prototype of those 2 functions between gdalopeninfo.cpp,
// ogrsqlitedatasource.cpp and ogrgeopackagedatasource.cpp
void GDALOpenInfoDeclareFileNotToOpen(const char* pszFilename,
                                       const GByte* pabyHeader,
                                       int nHeaderBytes);
void GDALOpenInfoUnDeclareFileNotToOpen(const char* pszFilename);

/************************************************************************/

/* This whole section helps for SQLite/GPKG, especially with write-ahead
 * log enabled. The issue is that sqlite3 relies on POSIX advisory locks to
 * properly work and decide when to create/delete the wal related files.
 * One issue with POSIX advisory locks is that if within the same process
 * you do
 * f1 = open('somefile')
 * set locks on f1
 * f2 = open('somefile')
 * close(f2)
 * The close(f2) will cancel the locks set on f1. The work on f1 is done by
 * libsqlite3 whereas the work on f2 is done by GDALOpenInfo.
 * So as soon as sqlite3 has opened a file we should make sure not to re-open
 * it (actually close it) ourselves.
 */

namespace {
struct FileNotToOpen
{
    CPLString osFilename{};
    int       nRefCount{};
    GByte    *pabyHeader{nullptr};
    int       nHeaderBytes{0};
};
}

static std::mutex sFNTOMutex;
static std::map<CPLString, FileNotToOpen>* pMapFNTO = nullptr;

void GDALOpenInfoDeclareFileNotToOpen(const char* pszFilename,
                                       const GByte* pabyHeader,
                                       int nHeaderBytes)
{
    std::lock_guard<std::mutex> oLock(sFNTOMutex);
    if( pMapFNTO == nullptr )
        pMapFNTO = new std::map<CPLString, FileNotToOpen>();
    auto oIter = pMapFNTO->find(pszFilename);
    if( oIter != pMapFNTO->end() )
    {
        oIter->second.nRefCount ++;
    }
    else
    {
        FileNotToOpen fnto;
        fnto.osFilename = pszFilename;
        fnto.nRefCount = 1;
        fnto.pabyHeader = static_cast<GByte*>(CPLMalloc(nHeaderBytes + 1));
        memcpy(fnto.pabyHeader, pabyHeader, nHeaderBytes);
        fnto.pabyHeader[nHeaderBytes] = 0;
        fnto.nHeaderBytes = nHeaderBytes;
        (*pMapFNTO)[pszFilename] = fnto;
    }
}

void GDALOpenInfoUnDeclareFileNotToOpen(const char* pszFilename)
{
    std::lock_guard<std::mutex> oLock(sFNTOMutex);
    CPLAssert(pMapFNTO);
    auto oIter = pMapFNTO->find(pszFilename);
    CPLAssert( oIter != pMapFNTO->end() );
    oIter->second.nRefCount --;
    if( oIter->second.nRefCount == 0 )
    {
        CPLFree(oIter->second.pabyHeader);
        pMapFNTO->erase(oIter);
    }
    if( pMapFNTO->empty() )
    {
        delete pMapFNTO;
        pMapFNTO = nullptr;
    }
}

static GByte* GDALOpenInfoGetFileNotToOpen(const char* pszFilename,
                                           int* pnHeaderBytes)
{
    std::lock_guard<std::mutex> oLock(sFNTOMutex);
    *pnHeaderBytes = 0;
    if( pMapFNTO == nullptr )
    {
        return nullptr;
    }
    auto oIter = pMapFNTO->find(pszFilename);
    if( oIter == pMapFNTO->end() )
    {
        return nullptr;
    }
    *pnHeaderBytes = oIter->second.nHeaderBytes;
    GByte* pabyHeader = static_cast<GByte*>(CPLMalloc(*pnHeaderBytes + 1));
    memcpy(pabyHeader, oIter->second.pabyHeader, *pnHeaderBytes);
    pabyHeader[*pnHeaderBytes] = 0;
    return pabyHeader;
}

/************************************************************************/
/* ==================================================================== */
/*                             GDALOpenInfo                             */
/* ==================================================================== */
/************************************************************************/

/************************************************************************/
/*                            GDALOpenInfo()                            */
/************************************************************************/

/** Constructor/
 * @param pszFilenameIn filename
 * @param nOpenFlagsIn open flags
 * @param papszSiblingsIn list of sibling files, or NULL.
 */
GDALOpenInfo::GDALOpenInfo( const char * pszFilenameIn, int nOpenFlagsIn,
                            const char * const * papszSiblingsIn ) :
    bHasGotSiblingFiles(false),
    papszSiblingFiles(nullptr),
    nHeaderBytesTried(0),
    pszFilename(CPLStrdup(pszFilenameIn)),
    papszOpenOptions(nullptr),
    eAccess(nOpenFlagsIn & GDAL_OF_UPDATE ? GA_Update : GA_ReadOnly),
    nOpenFlags(nOpenFlagsIn),
    bStatOK(FALSE),
    bIsDirectory(FALSE),
    fpL(nullptr),
    nHeaderBytes(0),
    pabyHeader(nullptr),
    papszAllowedDrivers(nullptr)
{
    if( STARTS_WITH(pszFilename, "MVT:/vsi") )
        return;

/* -------------------------------------------------------------------- */
/*      Ensure that C: is treated as C:\ so we can stat it on           */
/*      Windows.  Similar to what is done in CPLStat().                 */
/* -------------------------------------------------------------------- */
#ifdef WIN32
    if( strlen(pszFilenameIn) == 2 && pszFilenameIn[1] == ':' )
    {
        char    szAltPath[10];

        strcpy( szAltPath, pszFilenameIn );
        strcat( szAltPath, "\\" );
        CPLFree( pszFilename );
        pszFilename = CPLStrdup( szAltPath );
    }
#endif  // WIN32

/* -------------------------------------------------------------------- */
/*      Collect information about the file.                             */
/* -------------------------------------------------------------------- */

#ifdef HAVE_READLINK
    bool bHasRetried = false;

retry:  // TODO(schwehr): Stop using goto.

#endif  // HAVE_READLINK

#if !(defined(_WIN32) || defined(__linux__) || defined(__ANDROID__) || (defined(__MACH__) && defined(__APPLE__)))
    /* On BSDs, fread() on a directory returns non zero, so we have to */
    /* do a stat() before to check the nature of pszFilename. */
    bool bPotentialDirectory = (eAccess == GA_ReadOnly);
#else
    bool bPotentialDirectory = false;
#endif

    /* Check if the filename might be a directory of a special virtual file system */
    if( STARTS_WITH(pszFilename, "/vsizip/") ||
        STARTS_WITH(pszFilename, "/vsitar/") )
    {
        const char* pszExt = CPLGetExtension(pszFilename);
        if( EQUAL(pszExt, "zip") || EQUAL(pszExt, "tar") || EQUAL(pszExt, "gz")
            || pszFilename[strlen(pszFilename)-1] == '}'
#ifdef DEBUG
            // For AFL, so that .cur_input is detected as the archive filename.
            || EQUAL( CPLGetFilename(pszFilename), ".cur_input" )
#endif  // DEBUG
          )
        {
            bPotentialDirectory = true;
        }
    }
    else if( STARTS_WITH(pszFilename, "/vsicurl/") )
    {
        bPotentialDirectory = true;
    }

    if( bPotentialDirectory )
    {
        int nStatFlags = VSI_STAT_EXISTS_FLAG | VSI_STAT_NATURE_FLAG;
        if(nOpenFlagsIn & GDAL_OF_VERBOSE_ERROR)
            nStatFlags |= VSI_STAT_SET_ERROR_FLAG;

        // For those special files, opening them with VSIFOpenL() might result
        // in content, even if they should be considered as directories, so
        // use stat.
        VSIStatBufL sStat;

        if(VSIStatExL( pszFilename, &sStat, nStatFlags) == 0) {
            bStatOK = TRUE;
            if( VSI_ISDIR( sStat.st_mode ) )
                bIsDirectory = TRUE;
        }
    }

    pabyHeader = GDALOpenInfoGetFileNotToOpen(pszFilename, &nHeaderBytes);

    if( !bIsDirectory && pabyHeader == nullptr ) {
        fpL = VSIFOpenExL( pszFilename, (eAccess == GA_Update) ? "r+b" : "rb", (nOpenFlagsIn & GDAL_OF_VERBOSE_ERROR) > 0);
    }
    if( pabyHeader )
    {
        bStatOK = TRUE;
        nHeaderBytesTried = nHeaderBytes;
    }
    else if( fpL != nullptr )
    {
        bStatOK = TRUE;
        int nBufSize =
            atoi(CPLGetConfigOption("GDAL_INGESTED_BYTES_AT_OPEN", "1024"));
        if( nBufSize < 1024 )
            nBufSize = 1024;
        else if( nBufSize > 10 * 1024 * 1024)
            nBufSize = 10 * 1024 * 1024;
        pabyHeader = static_cast<GByte *>( CPLCalloc(nBufSize+1, 1) );
        nHeaderBytesTried = nBufSize;
        nHeaderBytes = static_cast<int>(
            VSIFReadL( pabyHeader, 1, nHeaderBytesTried, fpL ) );
        VSIRewindL( fpL );

        /* If we cannot read anything, check if it is not a directory instead */
        VSIStatBufL sStat;
        if( nHeaderBytes == 0 &&
            VSIStatExL( pszFilename, &sStat,
                        VSI_STAT_EXISTS_FLAG | VSI_STAT_NATURE_FLAG ) == 0 &&
            VSI_ISDIR( sStat.st_mode ) )
        {
            CPL_IGNORE_RET_VAL(VSIFCloseL(fpL));
            fpL = nullptr;
            CPLFree(pabyHeader);
            pabyHeader = nullptr;
            bIsDirectory = TRUE;
        }
    }
    else if( !bStatOK )
    {
        VSIStatBufL sStat;
        if( !bPotentialDirectory && VSIStatExL( pszFilename, &sStat,
                        VSI_STAT_EXISTS_FLAG | VSI_STAT_NATURE_FLAG ) == 0 )
        {
            bStatOK = TRUE;
            if( VSI_ISDIR( sStat.st_mode ) )
                bIsDirectory = TRUE;
        }
#ifdef HAVE_READLINK
        else if ( !bHasRetried && !STARTS_WITH(pszFilename, "/vsi") )
        {
            // If someone creates a file with "ln -sf
            // /vsicurl/http://download.osgeo.org/gdal/data/gtiff/utm.tif
            // my_remote_utm.tif" we will be able to open it by passing
            // my_remote_utm.tif.  This helps a lot for GDAL based readers that
            // only provide file explorers to open datasets.
            const int nBufSize = 2048;
            std::vector<char> oFilename(nBufSize);
            char *szPointerFilename = &oFilename[0];
            int nBytes = static_cast<int>(
                readlink( pszFilename, szPointerFilename, nBufSize ) );
            if (nBytes != -1)
            {
                szPointerFilename[std::min(nBytes, nBufSize - 1)] = 0;
                CPLFree(pszFilename);
                pszFilename = CPLStrdup(szPointerFilename);
                papszSiblingsIn = nullptr;
                bHasRetried = true;
                goto retry;
            }
        }
#endif  // HAVE_READLINK
    }

/* -------------------------------------------------------------------- */
/*      Capture sibling list either from passed in values, or by        */
/*      scanning for them only if requested through GetSiblingFiles().  */
/* -------------------------------------------------------------------- */
    if( papszSiblingsIn != nullptr )
    {
        papszSiblingFiles = CSLDuplicate( papszSiblingsIn );
        bHasGotSiblingFiles = true;
    }
    else if( bStatOK && !bIsDirectory )
    {
        papszSiblingFiles = VSISiblingFiles(pszFilename);
        if (papszSiblingFiles != nullptr)
        {
            bHasGotSiblingFiles = true;
        }
        else
        {
            const char* pszOptionVal =
                CPLGetConfigOption( "GDAL_DISABLE_READDIR_ON_OPEN", "NO" );
            if (EQUAL(pszOptionVal, "EMPTY_DIR"))
            {
                papszSiblingFiles =
                    CSLAddString( nullptr, CPLGetFilename(pszFilename) );
                bHasGotSiblingFiles = true;
            }
            else if( CPLTestBool(pszOptionVal) )
            {
                /* skip reading the directory */
                papszSiblingFiles = nullptr;
                bHasGotSiblingFiles = true;
            }
            else
            {
                /* will be lazy loaded */
                papszSiblingFiles = nullptr;
                bHasGotSiblingFiles = false;
            }
        }
    }
    else
    {
        papszSiblingFiles = nullptr;
        bHasGotSiblingFiles = true;
    }
}

/************************************************************************/
/*                           ~GDALOpenInfo()                            */
/************************************************************************/

GDALOpenInfo::~GDALOpenInfo()

{
    VSIFree( pabyHeader );
    CPLFree( pszFilename );

    if( fpL != nullptr )
        CPL_IGNORE_RET_VAL(VSIFCloseL( fpL ));
    CSLDestroy( papszSiblingFiles );
}

/************************************************************************/
/*                         GetSiblingFiles()                            */
/************************************************************************/

/** Return sibling files.
 * @return sibling files. Ownership below to the object.
 */
char** GDALOpenInfo::GetSiblingFiles()
{
    if( bHasGotSiblingFiles )
        return papszSiblingFiles;
    bHasGotSiblingFiles = true;

    papszSiblingFiles = VSISiblingFiles( pszFilename );
    if ( papszSiblingFiles != nullptr ) {
        return papszSiblingFiles;
    }

    CPLString osDir = CPLGetDirname( pszFilename );
    const int nMaxFiles =
        atoi(CPLGetConfigOption("GDAL_READDIR_LIMIT_ON_OPEN", "1000"));
    papszSiblingFiles = VSIReadDirEx( osDir, nMaxFiles );
    if( nMaxFiles > 0 && CSLCount(papszSiblingFiles) > nMaxFiles )
    {
        CPLDebug("GDAL", "GDAL_READDIR_LIMIT_ON_OPEN reached on %s",
                 osDir.c_str());
        CSLDestroy(papszSiblingFiles);
        papszSiblingFiles = nullptr;
    }


    return papszSiblingFiles;
}

/************************************************************************/
/*                         StealSiblingFiles()                          */
/*                                                                      */
/*      Same as GetSiblingFiles() except that the list is stealed       */
/*      (ie ownership transferred to the caller) and the associated     */
/*      member variable is set to NULL.                                 */
/************************************************************************/

/** Return sibling files and steal reference
 * @return sibling files. Ownership below to the caller (must be freed with CSLDestroy)
 */
char** GDALOpenInfo::StealSiblingFiles()
{
    char** papszRet = GetSiblingFiles();
    papszSiblingFiles = nullptr;
    return papszRet;
}

/************************************************************************/
/*                        AreSiblingFilesLoaded()                       */
/************************************************************************/

/** Return whether sibling files have been loaded.
 * @return true or false.
 */
bool GDALOpenInfo::AreSiblingFilesLoaded() const
{
    return bHasGotSiblingFiles;
}

/************************************************************************/
/*                           TryToIngest()                              */
/************************************************************************/

/** Ingest bytes from the file.
 * @param nBytes number of bytes to ingest.
 * @return TRUE if successful
 */
int GDALOpenInfo::TryToIngest(int nBytes)
{
    if( fpL == nullptr )
        return FALSE;
    if( nHeaderBytes < nHeaderBytesTried )
        return TRUE;
    pabyHeader = static_cast<GByte *>( CPLRealloc(pabyHeader, nBytes + 1) );
    memset(pabyHeader, 0, nBytes + 1);
    VSIRewindL(fpL);
    nHeaderBytesTried = nBytes;
    nHeaderBytes = static_cast<int>( VSIFReadL(pabyHeader, 1, nBytes, fpL) );
    VSIRewindL(fpL);

    return TRUE;
}
