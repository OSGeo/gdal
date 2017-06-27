/******************************************************************************
 *
 * Project:  WCS Client Driver
 * Purpose:  Implementation of an HTTP fetching driver.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2007, Frank Warmerdam
 * Copyright (c) 2008-2013, Even Rouault <even dot rouault at mines-paris dot org>
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

#include "cpl_string.h"
#include "cpl_http.h"
#include "cpl_atomic_ops.h"
#include "gdal_frmts.h"
#include "gdal_pam.h"

CPL_CVSID("$Id$")

/************************************************************************/
/*               HTTPFetchContentDispositionFilename()                 */
/************************************************************************/

static const char* HTTPFetchContentDispositionFilename(char** papszHeaders)
{
    char** papszIter = papszHeaders;
    while(papszIter && *papszIter)
    {
        /* For multipart, we have in raw format, but without end-of-line characters */
        if (STARTS_WITH(*papszIter, "Content-Disposition: attachment; filename="))
        {
            return *papszIter + 42;
        }
        /* For single part, the headers are in KEY=VAL format, but with e-o-l ... */
        else if (STARTS_WITH(*papszIter, "Content-Disposition=attachment; filename="))
        {
            char* pszVal = (char*)(*papszIter + 41);
            char* pszEOL = strchr(pszVal, '\r');
            if (pszEOL) *pszEOL = 0;
            pszEOL = strchr(pszVal, '\n');
            if (pszEOL) *pszEOL = 0;
            return pszVal;
        }
        papszIter ++;
    }
    return NULL;
}

/************************************************************************/
/*                              HTTPOpen()                              */
/************************************************************************/

static GDALDataset *HTTPOpen( GDALOpenInfo * poOpenInfo )

{
    static volatile int nCounter = 0;

    if( poOpenInfo->nHeaderBytes != 0 )
        return NULL;

    if( !STARTS_WITH_CI(poOpenInfo->pszFilename, "http:")
        && !STARTS_WITH_CI(poOpenInfo->pszFilename, "https:")
        && !STARTS_WITH_CI(poOpenInfo->pszFilename, "ftp:") )
        return NULL;

/* -------------------------------------------------------------------- */
/*      Fetch the result.                                               */
/* -------------------------------------------------------------------- */
    CPLErrorReset();

    CPLHTTPResult *psResult = CPLHTTPFetch( poOpenInfo->pszFilename, NULL );

/* -------------------------------------------------------------------- */
/*      Try to handle errors.                                           */
/* -------------------------------------------------------------------- */
    if( psResult == NULL || psResult->nDataLen == 0
        || CPLGetLastErrorNo() != 0 )
    {
        CPLHTTPDestroyResult( psResult );
        return NULL;
    }

/* -------------------------------------------------------------------- */
/*      Create a memory file from the result.                           */
/* -------------------------------------------------------------------- */
    CPLString osResultFilename;

    int nNewCounter = CPLAtomicInc(&nCounter);

    const char* pszFilename = HTTPFetchContentDispositionFilename(psResult->papszHeaders);
    if (pszFilename == NULL)
    {
        pszFilename = CPLGetFilename(poOpenInfo->pszFilename);
        /* If we have special characters, let's default to a fixed name */
        if (strchr(pszFilename, '?') || strchr(pszFilename, '&'))
            pszFilename = "file.dat";
    }

    osResultFilename.Printf( "/vsimem/http_%d/%s",
                             nNewCounter, pszFilename );

    VSILFILE *fp = VSIFileFromMemBuffer( osResultFilename,
                                     psResult->pabyData,
                                     psResult->nDataLen,
                                     TRUE );

    if( fp == NULL )
        return NULL;

    VSIFCloseL( fp );

/* -------------------------------------------------------------------- */
/*      Steal the memory buffer from HTTP result before destroying      */
/*      it.                                                             */
/* -------------------------------------------------------------------- */
    psResult->pabyData = NULL;
    psResult->nDataLen = 0;
    psResult->nDataAlloc = 0;

    CPLHTTPDestroyResult( psResult );

/* -------------------------------------------------------------------- */
/*      Try opening this result as a gdaldataset.                       */
/* -------------------------------------------------------------------- */
    /* suppress errors as not all drivers support /vsimem */
    CPLPushErrorHandler( CPLQuietErrorHandler );
    GDALDataset *poDS = (GDALDataset *)
        GDALOpenEx( osResultFilename, poOpenInfo->nOpenFlags,
                    poOpenInfo->papszAllowedDrivers,
                    poOpenInfo->papszOpenOptions, NULL);
    CPLPopErrorHandler();

/* -------------------------------------------------------------------- */
/*      If opening it in memory didn't work, perhaps we need to         */
/*      write to a temp file on disk?                                   */
/* -------------------------------------------------------------------- */
    if( poDS == NULL )
    {
        CPLString osTempFilename;

#ifdef WIN32
        const char* pszPath = CPLGetPath(CPLGenerateTempFilename(NULL));
#else
        const char* pszPath = "/tmp";
#endif
        osTempFilename = CPLFormFilename(pszPath, CPLGetFilename(osResultFilename), NULL );
        if( CPLCopyFile( osTempFilename, osResultFilename ) != 0 )
        {
            CPLError( CE_Failure, CPLE_OpenFailed,
                      "Failed to create temporary file:%s",
                      osTempFilename.c_str() );
        }
        else
        {
            poDS =  (GDALDataset *)
                GDALOpenEx( osTempFilename, poOpenInfo->nOpenFlags,
                            poOpenInfo->papszAllowedDrivers,
                            poOpenInfo->papszOpenOptions, NULL );
            if( VSIUnlink( osTempFilename ) != 0 && poDS != NULL )
                poDS->MarkSuppressOnClose(); /* VSIUnlink() may not work on windows */
            if( poDS && strcmp(poDS->GetDescription(), osTempFilename) == 0 )
                poDS->SetDescription(poOpenInfo->pszFilename);
        }
    }
    else if( strcmp(poDS->GetDescription(), osResultFilename) == 0 )
        poDS->SetDescription(poOpenInfo->pszFilename);

/* -------------------------------------------------------------------- */
/*      Release our hold on the vsi memory file, though if it is        */
/*      held open by a dataset it will continue to exist till that      */
/*      lets it go.                                                     */
/* -------------------------------------------------------------------- */
    VSIUnlink( osResultFilename );

    return poDS;
}

/************************************************************************/
/*                         GDALRegister_HTTP()                          */
/************************************************************************/

void GDALRegister_HTTP()

{
    if( GDALGetDriverByName( "HTTP" ) != NULL )
        return;

    GDALDriver *poDriver = new GDALDriver();

    poDriver->SetDescription( "HTTP" );
    poDriver->SetMetadataItem( GDAL_DCAP_RASTER, "YES" );
    poDriver->SetMetadataItem( GDAL_DCAP_VECTOR, "YES" );
    poDriver->SetMetadataItem( GDAL_DMD_LONGNAME, "HTTP Fetching Wrapper" );

    poDriver->pfnOpen = HTTPOpen;

    GetGDALDriverManager()->RegisterDriver( poDriver );
}
