/******************************************************************************
 * $Id$
 *
 * Name:     hfaopen.cpp
 * Project:  Erdas Imagine (.img) Translator
 * Purpose:  Supporting functions for HFA (.img) ... main (C callable) API
 *           that is not dependent on GDAL (just CPL).
 * Author:   Frank Warmerdam, warmerda@home.com
 *
 ******************************************************************************
 * Copyright (c) 1999, Intergraph Corporation
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
 * hfaopen.cpp
 *
 * Supporting routines for reading Erdas Imagine (.imf) Heirarchical
 * File Architecture files.  This is intended to be a library independent
 * of the GDAL core, but dependent on the Common Portability Library.
 *
 * $Log$
 * Revision 1.1  1999/01/04 05:28:13  warmerda
 * New
 *
 */

#include "hfa_p.h"
#include "cpl_conv.h"

/************************************************************************/
/*                          HFAGetDictionary()                          */
/************************************************************************/

static char * HFAGetDictionary( HFAHandle hHFA )

{
    char	*pszDictionary = (char *) CPLMalloc(100); 
    int		nDictMax = 100;
    int		nDictSize = 0;
    
    VSIFSeek( hHFA->fp, hHFA->nDictionaryPos, SEEK_SET );

    while( TRUE )
    {
        if( nDictSize >= nDictMax-1 )
        {
            nDictMax = nDictSize * 2 + 100;
            pszDictionary = (char *) CPLRealloc(pszDictionary, nDictMax );
        }

        if( VSIFRead( pszDictionary + nDictSize, 1, 1, hHFA->fp ) < 1
            || pszDictionary[nDictSize] == '\0' )
            break;

        nDictSize++;
    }

    pszDictionary[nDictSize] = '\0';

    return( pszDictionary );
}

/************************************************************************/
/*                              HFAOpen()                               */
/************************************************************************/

HFAHandle HFAOpen( const char * pszFilename, const char * pszAccess )

{
    FILE	*fp;
    char	szHeader[16];
    HFAInfo_t	*psInfo;
    GUInt32	nHeaderPos;

/* -------------------------------------------------------------------- */
/*      Open the file.                                                  */
/* -------------------------------------------------------------------- */
    if( EQUAL(pszAccess,"r") || EQUAL(pszAccess,"rb" ) )
        fp = VSIFOpen( pszFilename, "rb" );
    else
        fp = VSIFOpen( pszFilename, "r+b" );

    /* should this be changed to use some sort of CPLFOpen() which will
       set the error? */
    if( fp == NULL )
    {
        CPLError( CE_Failure, CPLE_OpenFailed,
                  "File open of %s failed.",
                  pszFilename );

        return NULL;
    }

/* -------------------------------------------------------------------- */
/*      Read and verify the header.                                     */
/* -------------------------------------------------------------------- */
    if( VSIFRead( szHeader, 16, 1, fp ) < 1 )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "Attempt to read 16 byte header failed for\n%s.",
                  pszFilename );

        return NULL;
    }

    if( !EQUALN(szHeader,"EHFA_HEADER_TAG",15) )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "File %s is not an Imagine HFA file ... header wrong.",
                  pszFilename );

        return NULL;
    }

/* -------------------------------------------------------------------- */
/*      Create the HFAInfo_t                                            */
/* -------------------------------------------------------------------- */
    psInfo = (HFAInfo_t *) CPLCalloc(sizeof(HFAInfo_t),1);

    psInfo->fp = fp;

/* -------------------------------------------------------------------- */
/*	Where is the header?						*/
/* -------------------------------------------------------------------- */
    VSIFRead( &nHeaderPos, sizeof(GInt32), 1, fp );
    HFAStandard( 4, &nHeaderPos );

/* -------------------------------------------------------------------- */
/*      Read the header.                                                */
/* -------------------------------------------------------------------- */
    VSIFSeek( fp, nHeaderPos, SEEK_SET );

    VSIFRead( &(psInfo->nVersion), sizeof(GInt32), 1, fp );
    HFAStandard( 4, &(psInfo->nVersion) );
    
    VSIFRead( szHeader, 4, 1, fp ); /* skip freeList */

    VSIFRead( &(psInfo->nRootPos), sizeof(GInt32), 1, fp );
    HFAStandard( 4, &(psInfo->nRootPos) );
    
    VSIFRead( &(psInfo->nEntryHeaderLength), sizeof(GInt16), 1, fp );
    HFAStandard( 2, &(psInfo->nEntryHeaderLength) );
    
    VSIFRead( &(psInfo->nDictionaryPos), sizeof(GInt32), 1, fp );
    HFAStandard( 4, &(psInfo->nDictionaryPos) );

/* -------------------------------------------------------------------- */
/*      Instantiate the root entry.                                     */
/* -------------------------------------------------------------------- */
    psInfo->poRoot = new HFAEntry( psInfo, psInfo->nRootPos, NULL, NULL );

/* -------------------------------------------------------------------- */
/*      Read the dictionary                                             */
/* -------------------------------------------------------------------- */
    psInfo->pszDictionary = HFAGetDictionary( psInfo );
    psInfo->poDictionary = new HFADictionary( psInfo->pszDictionary );

    return psInfo;
}

/************************************************************************/
/*                              HFAClose()                              */
/************************************************************************/

void HFAClose( HFAHandle hHFA )

{
    delete hHFA->poRoot;

    VSIFClose( hHFA->fp );

    CPLFree( hHFA->pszDictionary );
    CPLFree( hHFA );
}


