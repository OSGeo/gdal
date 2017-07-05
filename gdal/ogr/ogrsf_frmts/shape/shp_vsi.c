/******************************************************************************
 * $Id$
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  IO Redirection via VSI services for shp/dbf io.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2007,  Frank Warmerdam
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

#include "shp_vsi.h"
#include "cpl_error.h"
#include "cpl_conv.h"
#include "cpl_vsi_error.h"
#include <limits.h>

CPL_CVSID("$Id$")

typedef struct
{
    VSILFILE *fp;
    char     *pszFilename;
    int       bEnforce2GBLimit;
    int       bHasWarned2GB;
    SAOffset  nCurOffset;
} OGRSHPDBFFile;

/************************************************************************/
/*                         VSI_SHP_GetVSIL()                            */
/************************************************************************/

VSILFILE* VSI_SHP_GetVSIL( SAFile file )
{
    OGRSHPDBFFile* pFile = (OGRSHPDBFFile*) file;
    return pFile->fp;
}

/************************************************************************/
/*                        VSI_SHP_GetFilename()                         */
/************************************************************************/

const char* VSI_SHP_GetFilename( SAFile file )
{
    OGRSHPDBFFile* pFile = (OGRSHPDBFFile*) file;
    return pFile->pszFilename;
}

/************************************************************************/
/*                         VSI_SHP_OpenInternal()                       */
/************************************************************************/

static
SAFile VSI_SHP_OpenInternal( const char *pszFilename, const char *pszAccess,
                             int bEnforce2GBLimit )

{
    OGRSHPDBFFile* pFile;
    VSILFILE* fp = VSIFOpenExL( pszFilename, pszAccess, TRUE );
    if( fp == NULL )
        return NULL;
    pFile = (OGRSHPDBFFile* )CPLCalloc(1,sizeof(OGRSHPDBFFile));
    pFile->fp = fp;
    pFile->pszFilename = CPLStrdup(pszFilename);
    pFile->bEnforce2GBLimit = bEnforce2GBLimit;
    pFile->nCurOffset = 0;
    return (SAFile) pFile;
}

/************************************************************************/
/*                            VSI_SHP_Open()                            */
/************************************************************************/

static
SAFile VSI_SHP_Open( const char *pszFilename, const char *pszAccess )

{
    return VSI_SHP_OpenInternal(pszFilename, pszAccess, FALSE);
}

/************************************************************************/
/*                        VSI_SHP_Open2GBLimit()                        */
/************************************************************************/

static
SAFile VSI_SHP_Open2GBLimit( const char *pszFilename, const char *pszAccess )

{
    return VSI_SHP_OpenInternal(pszFilename, pszAccess, TRUE);
}

/************************************************************************/
/*                            VSI_SHP_Read()                            */
/************************************************************************/

static
SAOffset VSI_SHP_Read( void *p, SAOffset size, SAOffset nmemb, SAFile file )

{
    OGRSHPDBFFile* pFile = (OGRSHPDBFFile*) file;
    SAOffset ret = (SAOffset) VSIFReadL( p, (size_t) size, (size_t) nmemb,
                                 pFile->fp );
    pFile->nCurOffset += ret * size;
    return ret;
}

/************************************************************************/
/*                      VSI_SHP_WriteMoreDataOK()                       */
/************************************************************************/

int VSI_SHP_WriteMoreDataOK( SAFile file, SAOffset nExtraBytes )
{
    OGRSHPDBFFile* pFile = (OGRSHPDBFFile*) file;
    if( pFile->nCurOffset + nExtraBytes > INT_MAX )
    {
        if( pFile->bEnforce2GBLimit )
        {
            CPLError( CE_Failure, CPLE_AppDefined, "2GB file size limit reached for %s",
                      pFile->pszFilename );
            return FALSE;
        }
        else if( !pFile->bHasWarned2GB )
        {
            pFile->bHasWarned2GB = TRUE;
            CPLError( CE_Warning, CPLE_AppDefined, "2GB file size limit reached for %s. "
                      "Going on, but might cause compatibility issues with third party software",
                      pFile->pszFilename );
        }
    }

    return TRUE;
}

/************************************************************************/
/*                           VSI_SHP_Write()                            */
/************************************************************************/

static
SAOffset VSI_SHP_Write( void *p, SAOffset size, SAOffset nmemb, SAFile file )

{
    OGRSHPDBFFile* pFile = (OGRSHPDBFFile*) file;
    SAOffset ret;
    if( !VSI_SHP_WriteMoreDataOK( file, size * nmemb ) )
        return 0;
    ret = (SAOffset) VSIFWriteL( p, (size_t) size, (size_t) nmemb,
                                  pFile->fp );
    pFile->nCurOffset += ret * size;
    return ret;
}

/************************************************************************/
/*                            VSI_SHP_Seek()                            */
/************************************************************************/

static
SAOffset VSI_SHP_Seek( SAFile file, SAOffset offset, int whence )

{
    OGRSHPDBFFile* pFile = (OGRSHPDBFFile*) file;
    SAOffset ret = (SAOffset) VSIFSeekL( pFile->fp, (vsi_l_offset) offset, whence );
    if( whence == 0 && ret == 0)
        pFile->nCurOffset = offset;
    else
        pFile->nCurOffset = (SAOffset) VSIFTellL( pFile->fp );
    return ret;
}

/************************************************************************/
/*                            VSI_SHP_Tell()                            */
/************************************************************************/

static
SAOffset VSI_SHP_Tell( SAFile file )

{
    OGRSHPDBFFile* pFile = (OGRSHPDBFFile*) file;
    return (SAOffset) pFile->nCurOffset;
}

/************************************************************************/
/*                           VSI_SHP_Flush()                            */
/************************************************************************/

static
int VSI_SHP_Flush( SAFile file )

{
    OGRSHPDBFFile* pFile = (OGRSHPDBFFile*) file;
    return VSIFFlushL( pFile->fp );
}

/************************************************************************/
/*                           VSI_SHP_Close()                            */
/************************************************************************/

static
int VSI_SHP_Close( SAFile file )

{
    OGRSHPDBFFile* pFile = (OGRSHPDBFFile*) file;
    int ret = VSIFCloseL( pFile->fp );
    CPLFree(pFile->pszFilename);
    CPLFree(pFile);
    return ret;
}

/************************************************************************/
/*                              SADError()                              */
/************************************************************************/

static
void VSI_SHP_Error( const char *message )

{
    CPLError( CE_Failure, CPLE_AppDefined, "%s", message );
}

/************************************************************************/
/*                           VSI_SHP_Remove()                           */
/************************************************************************/

static
int VSI_SHP_Remove( const char *pszFilename )

{
    return VSIUnlink( pszFilename );
}

/************************************************************************/
/*                        SASetupDefaultHooks()                         */
/************************************************************************/

void SASetupDefaultHooks( SAHooks *psHooks )

{
    psHooks->FOpen   = VSI_SHP_Open;
    psHooks->FRead   = VSI_SHP_Read;
    psHooks->FWrite  = VSI_SHP_Write;
    psHooks->FSeek   = VSI_SHP_Seek;
    psHooks->FTell   = VSI_SHP_Tell;
    psHooks->FFlush  = VSI_SHP_Flush;
    psHooks->FClose  = VSI_SHP_Close;
    psHooks->Remove  = VSI_SHP_Remove;

    psHooks->Error   = VSI_SHP_Error;
    psHooks->Atof    = CPLAtof;
}

/************************************************************************/
/*                         VSI_SHP_GetHook()                            */
/************************************************************************/

static const SAHooks sOGRHook =
{
    VSI_SHP_Open,
    VSI_SHP_Read,
    VSI_SHP_Write,
    VSI_SHP_Seek,
    VSI_SHP_Tell,
    VSI_SHP_Flush,
    VSI_SHP_Close,
    VSI_SHP_Remove,
    VSI_SHP_Error,
    CPLAtof
};

static const SAHooks sOGRHook2GBLimit =
{
    VSI_SHP_Open2GBLimit,
    VSI_SHP_Read,
    VSI_SHP_Write,
    VSI_SHP_Seek,
    VSI_SHP_Tell,
    VSI_SHP_Flush,
    VSI_SHP_Close,
    VSI_SHP_Remove,
    VSI_SHP_Error,
    CPLAtof
};

const SAHooks* VSI_SHP_GetHook(int b2GBLimit)
{
    return (b2GBLimit) ? &sOGRHook2GBLimit : &sOGRHook;
}
