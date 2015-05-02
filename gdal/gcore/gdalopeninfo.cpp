/******************************************************************************
 * $Id$
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

#include "gdal_priv.h"
#include "cpl_conv.h"

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

CPL_CVSID("$Id$");

/************************************************************************/
/* ==================================================================== */
/*                             GDALOpenInfo                             */
/* ==================================================================== */
/************************************************************************/

/************************************************************************/
/*                            GDALOpenInfo()                            */
/************************************************************************/

GDALOpenInfo::GDALOpenInfo( const char * pszFilenameIn, int nOpenFlagsIn,
                            char **papszSiblingsIn )

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
        pszFilename = CPLStrdup( szAltPath );
    }
    else
#endif
        pszFilename = CPLStrdup( pszFilenameIn );

/* -------------------------------------------------------------------- */
/*      Initialize.                                                     */
/* -------------------------------------------------------------------- */

    nHeaderBytes = 0;
    nHeaderBytesTried = 0;
    pabyHeader = NULL;
    bIsDirectory = FALSE;
    bStatOK = FALSE;
    nOpenFlags = nOpenFlagsIn;
    eAccess = (nOpenFlags & GDAL_OF_UPDATE) ? GA_Update : GA_ReadOnly;
    fpL = NULL;
    papszOpenOptions = NULL;

#ifdef HAVE_READLINK
    int  bHasRetried = FALSE;
#endif

/* -------------------------------------------------------------------- */
/*      Collect information about the file.                             */
/* -------------------------------------------------------------------- */
    VSIStatBufL  sStat;

#ifdef HAVE_READLINK
retry:
#endif
    int bPotentialDirectory = FALSE;

    /* Check if the filename might be a directory of a special virtual file system */
    if( strncmp(pszFilename, "/vsizip/", strlen("/vsizip/")) == 0 ||
        strncmp(pszFilename, "/vsitar/", strlen("/vsitar/")) == 0 )
    {
        const char* pszExt = CPLGetExtension(pszFilename);
        if( EQUAL(pszExt, "zip") || EQUAL(pszExt, "tar") || EQUAL(pszExt, "gz") )
            bPotentialDirectory = TRUE;
    }
    else if( strncmp(pszFilename, "/vsicurl/", strlen("/vsicurl/")) == 0 )
    {
        bPotentialDirectory = TRUE;
    }

    if( bPotentialDirectory )
    {
        /* For those special files, opening them with VSIFOpenL() might result */
        /* in content, even if they should be considered as directories, so */
        /* use stat */
        if( VSIStatExL( pszFilename, &sStat,
                        VSI_STAT_EXISTS_FLAG | VSI_STAT_NATURE_FLAG ) == 0 )
        {
            bStatOK = TRUE;
            if( VSI_ISDIR( sStat.st_mode ) )
                bIsDirectory = TRUE;
        }
    }

    if( !bIsDirectory )
        fpL = VSIFOpenL( pszFilename, (eAccess == GA_Update) ? "r+b" : "rb" );
    if( fpL != NULL )
    {
        bStatOK = TRUE;
        pabyHeader = (GByte *) CPLCalloc(1025,1);
        nHeaderBytesTried = 1024;
        nHeaderBytes = (int) VSIFReadL( pabyHeader, 1, nHeaderBytesTried, fpL );
        VSIRewindL( fpL );

        /* If we cannot read anything, check if it is not a directory instead */
        if( nHeaderBytes == 0 &&
            VSIStatExL( pszFilename, &sStat,
                        VSI_STAT_EXISTS_FLAG | VSI_STAT_NATURE_FLAG ) == 0 &&
            VSI_ISDIR( sStat.st_mode ) )
        {
            VSIFCloseL(fpL);
            fpL = NULL;
            CPLFree(pabyHeader);
            pabyHeader = NULL;
            bIsDirectory = TRUE;
        }
    }
    else if( !bStatOK )
    {
        if( VSIStatExL( pszFilename, &sStat,
                        VSI_STAT_EXISTS_FLAG | VSI_STAT_NATURE_FLAG ) == 0 )
        {
            bStatOK = TRUE;
            if( VSI_ISDIR( sStat.st_mode ) )
                bIsDirectory = TRUE;
        }
#ifdef HAVE_READLINK
        else if ( !bHasRetried && strncmp(pszFilename, "/vsi", strlen("/vsi")) != 0 )
        {
            /* If someone creates a file with "ln -sf /vsicurl/http://download.osgeo.org/gdal/data/gtiff/utm.tif my_remote_utm.tif" */
            /* we will be able to open it by passing my_remote_utm.tif */
            /* This helps a lot for GDAL based readers that only provide file explorers to open datasets */
            char szPointerFilename[2048];
            int nBytes = readlink(pszFilename, szPointerFilename, sizeof(szPointerFilename));
            if (nBytes != -1)
            {
                szPointerFilename[MIN(nBytes, (int)sizeof(szPointerFilename)-1)] = 0;
                CPLFree(pszFilename);
                pszFilename = CPLStrdup(szPointerFilename);
                papszSiblingsIn = NULL;
                bHasRetried = TRUE;
                goto retry;
            }
        }
#endif
    }

/* -------------------------------------------------------------------- */
/*      Capture sibling list either from passed in values, or by        */
/*      scanning for them only if requested through GetSiblingFiles().  */
/* -------------------------------------------------------------------- */
    if( papszSiblingsIn != NULL )
    {
        papszSiblingFiles = CSLDuplicate( papszSiblingsIn );
        bHasGotSiblingFiles = TRUE;
    }
    else if( bStatOK && !bIsDirectory )
    {
        const char* pszOptionVal =
            CPLGetConfigOption( "GDAL_DISABLE_READDIR_ON_OPEN", "NO" );
        if (EQUAL(pszOptionVal, "EMPTY_DIR"))
        {
            papszSiblingFiles = CSLAddString( NULL, CPLGetFilename(pszFilename) );
            bHasGotSiblingFiles = TRUE;
        }
        else if( CSLTestBoolean(pszOptionVal) )
        {
            /* skip reading the directory */
            papszSiblingFiles = NULL;
            bHasGotSiblingFiles = TRUE;
        }
        else
        {
            /* will be lazy loaded */
            papszSiblingFiles = NULL;
            bHasGotSiblingFiles = FALSE;
        }
    }
    else
    {
        papszSiblingFiles = NULL;
        bHasGotSiblingFiles = TRUE;
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
        VSIFCloseL( fpL );
    CSLDestroy( papszSiblingFiles );
}

/************************************************************************/
/*                         GetSiblingFiles()                            */
/************************************************************************/

char** GDALOpenInfo::GetSiblingFiles()
{
    if( bHasGotSiblingFiles )
        return papszSiblingFiles;
    bHasGotSiblingFiles = TRUE;

    CPLString osDir = CPLGetDirname( pszFilename );
    papszSiblingFiles = VSIReadDir( osDir );

    /* Small optimization to avoid unnecessary stat'ing from PAux or ENVI */
    /* drivers. The MBTiles driver needs no companion file. */
    if( papszSiblingFiles == NULL &&
        strncmp(pszFilename, "/vsicurl/", 9) == 0 &&
        EQUAL(CPLGetExtension( pszFilename ),"mbtiles") )
    {
        papszSiblingFiles = CSLAddString( NULL, CPLGetFilename(pszFilename) );
    }

    return papszSiblingFiles;
}


/************************************************************************/
/*                           TryToIngest()                              */
/************************************************************************/

int GDALOpenInfo::TryToIngest(int nBytes)
{
    if( fpL == NULL )
        return FALSE;
    if( nHeaderBytes < nHeaderBytesTried )
        return TRUE;
    pabyHeader = (GByte*) CPLRealloc(pabyHeader, nBytes + 1);
    memset(pabyHeader, 0, nBytes + 1);
    VSIRewindL(fpL);
    nHeaderBytesTried = nBytes;
    nHeaderBytes = (int) VSIFReadL(pabyHeader, 1, nBytes, fpL);
    VSIRewindL(fpL);

    return TRUE;
}
