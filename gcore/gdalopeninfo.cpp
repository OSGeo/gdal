/******************************************************************************
 * $Id$
 *
 * Project:  GDAL Core
 * Purpose:  Implementation of GDALOpenInfo class.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 **********************************************************************
 * Copyright (c) 2002, Frank Warmerdam
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

CPL_CVSID("$Id$");

/************************************************************************/
/* ==================================================================== */
/*                             GDALOpenInfo                             */
/* ==================================================================== */
/************************************************************************/

/************************************************************************/
/*                            GDALOpenInfo()                            */
/************************************************************************/

GDALOpenInfo::GDALOpenInfo( const char * pszFilenameIn, GDALAccess eAccessIn,
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
    pabyHeader = NULL;
    bIsDirectory = FALSE;
    bStatOK = FALSE;
    eAccess = eAccessIn;
    fp = NULL;
    
/* -------------------------------------------------------------------- */
/*      Collect information about the file.                             */
/* -------------------------------------------------------------------- */
    VSIStatBufL  sStat;

    if( VSIStatL( pszFilename, &sStat ) == 0 )
    {
        bStatOK = TRUE;

        if( VSI_ISREG( sStat.st_mode ) )
        {
            pabyHeader = (GByte *) CPLCalloc(1025,1);

            fp = VSIFOpen( pszFilename, "rb" );

            if( fp != NULL )
            {
                nHeaderBytes = (int) VSIFRead( pabyHeader, 1, 1024, fp );

                VSIRewind( fp );
            }
            /* XXX: ENOENT is used to catch the case of virtual filesystem
             * when we do not have a real file with such a name. Under some
             * circumstances EINVAL reported instead of ENOENT in Windows
             * (for filenames containing colon, e.g. "smth://name"). 
             * See also: #2437 */
            else if( errno == 27 /* "File to large" */ 
                     || errno == ENOENT || errno == EINVAL
#ifdef EOVERFLOW
                     || errno == EOVERFLOW
#else
                     || errno == 75 /* Linux EOVERFLOW */
                     || errno == 79 /* Solaris EOVERFLOW */ 
#endif
                     )
            {
                fp = VSIFOpenL( pszFilename, "rb" );
                if( fp != NULL )
                {
                    nHeaderBytes = (int) VSIFReadL( pabyHeader, 1, 1024, fp );
                    VSIFCloseL( fp );
                    fp = NULL;
                }
            }
        }
        else if( VSI_ISDIR( sStat.st_mode ) )
            bIsDirectory = TRUE;
    }

/* -------------------------------------------------------------------- */
/*      Capture sibling list either from passed in values, or by        */
/*      scanning for them.                                              */
/* -------------------------------------------------------------------- */
    if( papszSiblingsIn != NULL )
    {
        papszSiblingFiles = CSLDuplicate( papszSiblingsIn );
    }
    else if( bStatOK && !bIsDirectory )
    {
        if( CSLTestBoolean( 
                CPLGetConfigOption( "GDAL_DISABLE_READDIR_ON_OPEN", "NO" )) )
        {
            /* skip reading the directory */
            papszSiblingFiles = NULL;
        }
        else
        {
            CPLString osDir = CPLGetDirname( pszFilename );
            papszSiblingFiles = VSIReadDir( osDir );
        }
    }
    else
        papszSiblingFiles = NULL;
}

/************************************************************************/
/*                           ~GDALOpenInfo()                            */
/************************************************************************/

GDALOpenInfo::~GDALOpenInfo()

{
    VSIFree( pabyHeader );
    CPLFree( pszFilename );

    if( fp != NULL )
        VSIFClose( fp );
    CSLDestroy( papszSiblingFiles );
}

