/******************************************************************************
 *
 * Project:  GDAL Core
 * Purpose:  Implementation of GDALOpenInfo class.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 **********************************************************************
 * Copyright (c) 2002, Frank Warmerdam
 * Copyright (c) 2008-2012, Even Rouault <even dot rouault at mines-paris dot org>
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
#include <vector>

#include "cpl_config.h"
#include "cpl_conv.h"
#include "cpl_error.h"
#include "cpl_string.h"
#include "cpl_vsi.h"
#include "gdal.h"

CPL_CVSID("$Id$")

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
                            char **papszSiblingsIn ) :
    bHasGotSiblingFiles(false),
    papszSiblingFiles(NULL),
    nHeaderBytesTried(0),
    pszFilename(CPLStrdup(pszFilenameIn)),
    papszOpenOptions(NULL),
    eAccess(nOpenFlagsIn & GDAL_OF_UPDATE ? GA_Update : GA_ReadOnly),
    nOpenFlags(nOpenFlagsIn),
    bStatOK(FALSE),
    bIsDirectory(FALSE),
    fpL(NULL),
    nHeaderBytes(0),
    pabyHeader(NULL),
    papszAllowedDrivers(NULL)
{

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

#ifdef __FreeBSD__
    /* FreeBSD 8 oddity: fopen(a_directory, "rb") returns non NULL */
    bool bPotentialDirectory = (eAccess == GA_ReadOnly);
#else
    bool bPotentialDirectory = false;
#endif  // __FreeBDS__

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

    if( !bIsDirectory ) {
        fpL = VSIFOpenExL( pszFilename, (eAccess == GA_Update) ? "r+b" : "rb", (nOpenFlagsIn & GDAL_OF_VERBOSE_ERROR) > 0);
    }
    if( fpL != NULL )
    {
        bStatOK = TRUE;
        const int nBufSize = 1025;
        pabyHeader = static_cast<GByte *>( CPLCalloc(nBufSize, 1) );
        nHeaderBytesTried = nBufSize - 1;
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
            fpL = NULL;
            CPLFree(pabyHeader);
            pabyHeader = NULL;
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
                papszSiblingsIn = NULL;
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
    if( papszSiblingsIn != NULL )
    {
        papszSiblingFiles = CSLDuplicate( papszSiblingsIn );
        bHasGotSiblingFiles = true;
    }
    else if( bStatOK && !bIsDirectory )
    {
        const char* pszOptionVal =
            CPLGetConfigOption( "GDAL_DISABLE_READDIR_ON_OPEN", "NO" );
        if (EQUAL(pszOptionVal, "EMPTY_DIR"))
        {
            papszSiblingFiles =
                CSLAddString( NULL, CPLGetFilename(pszFilename) );
            bHasGotSiblingFiles = true;
        }
        else if( CPLTestBool(pszOptionVal) )
        {
            /* skip reading the directory */
            papszSiblingFiles = NULL;
            bHasGotSiblingFiles = true;
        }
        else
        {
            /* will be lazy loaded */
            papszSiblingFiles = NULL;
            bHasGotSiblingFiles = false;
        }
    }
    else
    {
        papszSiblingFiles = NULL;
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

    if( fpL != NULL )
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

    CPLString osDir = CPLGetDirname( pszFilename );
    const int nMaxFiles =
        atoi(CPLGetConfigOption("GDAL_READDIR_LIMIT_ON_OPEN", "1000"));
    papszSiblingFiles = VSIReadDirEx( osDir, nMaxFiles );
    if( nMaxFiles > 0 && CSLCount(papszSiblingFiles) > nMaxFiles )
    {
        CPLDebug("GDAL", "GDAL_READDIR_LIMIT_ON_OPEN reached on %s",
                 osDir.c_str());
        CSLDestroy(papszSiblingFiles);
        papszSiblingFiles = NULL;
    }

    /* Small optimization to avoid unnecessary stat'ing from PAux or ENVI */
    /* drivers. The MBTiles driver needs no companion file. */
    if( papszSiblingFiles == NULL &&
        STARTS_WITH(pszFilename, "/vsicurl/") &&
        EQUAL(CPLGetExtension( pszFilename ),"mbtiles") )
    {
        papszSiblingFiles = CSLAddString( NULL, CPLGetFilename(pszFilename) );
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
    papszSiblingFiles = NULL;
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
    if( fpL == NULL )
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
