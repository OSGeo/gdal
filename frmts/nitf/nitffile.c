/******************************************************************************
 * $Id$
 *
 * Project:  NITF Read/Write Library
 * Purpose:  Module responsible for opening NITF file, populating NITFFile
 *           structure, and instantiating segment specific access objects.
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
 ******************************************************************************
 *
 * $Log$
 * Revision 1.1  2002/12/02 06:09:29  warmerda
 * New
 *
 */

#include "nitflib.h"
#include "cpl_vsi.h"
#include "cpl_conv.h"

CPL_CVSID("$Id$");

static int 
NITFCollectSegmentInfo( NITFFile *psFile, int nOffset, char *pszType,
                        int nHeaderLenSize, int nDataLenSize, 
                        int *pnNextData );

/************************************************************************/
/*                              NITFOpen()                              */
/************************************************************************/

NITFFile *NITFOpen( const char *pszFilename, int bUpdatable )

{
    FILE	*fp;
    char        szHeader[500];
    NITFFile    *psFile;
    int         nHeaderLen, nOffset, nNextData;
    char        szTemp[128];

/* -------------------------------------------------------------------- */
/*      Open the file.                                                  */
/* -------------------------------------------------------------------- */
    if( bUpdatable )
        fp = VSIFOpen( pszFilename, "r+b" );
    else
        fp = VSIFOpen( pszFilename, "rb" );

    if( fp == NULL )
    {
        CPLError( CE_Failure, CPLE_OpenFailed, 
                  "Failed to open file %s.", 
                  pszFilename );
        return NULL;
    }

/* -------------------------------------------------------------------- */
/*      Read non-varying portion of file header.                        */
/* -------------------------------------------------------------------- */

    VSIFRead( szHeader, 1, 500, fp );

    if( !EQUALN(szHeader,"NITF",4) )
    {
        CPLError( CE_Failure, CPLE_AppDefined, 
                  "The file %s is not an NITF file.", 
                  pszFilename );
        return NULL;
    }

/* -------------------------------------------------------------------- */
/*      Create and initialize info structure about file.                */
/* -------------------------------------------------------------------- */
    psFile = (NITFFile *) CPLCalloc(sizeof(NITFFile),1);
    psFile->fp = fp;

/* -------------------------------------------------------------------- */
/*      Get version.                                                    */
/* -------------------------------------------------------------------- */
    NITFGetField( psFile->szVersion, szHeader, 4, 5 );

/* -------------------------------------------------------------------- */
/*      Get header length.                                              */
/* -------------------------------------------------------------------- */
    nHeaderLen = atoi(NITFGetField( szTemp, szHeader, 354, 6 ));       

/* -------------------------------------------------------------------- */
/*      Collect segment info for the types we care about.               */
/* -------------------------------------------------------------------- */
    nNextData = nHeaderLen;

    nOffset = NITFCollectSegmentInfo( psFile, 360, "IM", 6, 10, &nNextData );

    nOffset = NITFCollectSegmentInfo( psFile, nOffset, "GR", 4, 6, &nNextData);

    nOffset = NITFCollectSegmentInfo( psFile, nOffset, "TX", 4, 5, &nNextData);

    return psFile;
}

/************************************************************************/
/*                             NITFClose()                              */
/************************************************************************/

void NITFClose( NITFFile *psFile )

{
    CPLFree( psFile->pasSegmentInfo );
    CPLFree( psFile );
}

/************************************************************************/
/*                       NITFCollectSegmentInfo()                       */
/*                                                                      */
/*      Collect the information about a set of segments of a            */
/*      particular type from the NITF file header, and add them to      */
/*      the segment list in the NITFFile object.                        */
/************************************************************************/

static int 
NITFCollectSegmentInfo( NITFFile *psFile, int nOffset, char *pszType,
                        int nHeaderLenSize, int nDataLenSize, int *pnNextData )

{
    char szTemp[12];
    char *pachSegDef;
    int  nCount, nSegDefSize, iSegment;

/* -------------------------------------------------------------------- */
/*      Get the segment count, and grow the segmentinfo array           */
/*      accordingly.                                                    */
/* -------------------------------------------------------------------- */
    VSIFSeek( psFile->fp, nOffset, SEEK_SET );
    VSIFRead( szTemp, 1, 3, psFile->fp );
    szTemp[3] = '\0';

    nCount = atoi(szTemp);

    if( nCount == 0 )
        return nOffset + 3;

    if( psFile->pasSegmentInfo == NULL )
        psFile->pasSegmentInfo = (NITFSegmentInfo *)
            CPLMalloc( sizeof(NITFSegmentInfo) * nCount );
    else
        psFile->pasSegmentInfo = (NITFSegmentInfo *)
            CPLRealloc( psFile->pasSegmentInfo, 
                        sizeof(NITFSegmentInfo)
                        * (psFile->nSegmentCount+nCount) );

/* -------------------------------------------------------------------- */
/*      Read the detailed information about the segments.               */
/* -------------------------------------------------------------------- */
    nSegDefSize = nCount * (nHeaderLenSize + nDataLenSize);
    pachSegDef = (char *) CPLMalloc(nCount * (nHeaderLenSize + nDataLenSize));
    
    VSIFRead( pachSegDef, 1, nSegDefSize, psFile->fp );

/* -------------------------------------------------------------------- */
/*      Collect detailed about segment.                                 */
/* -------------------------------------------------------------------- */
    for( iSegment = 0; iSegment < nCount; iSegment++ )
    {
        NITFSegmentInfo *psInfo = psFile->pasSegmentInfo+psFile->nSegmentCount;
        
        psInfo->hAccess = NULL;
        strcpy( psInfo->szSegmentType, pszType );
        
        psInfo->nSegmentHeaderSize = 
            atoi(NITFGetField(szTemp,pachSegDef, 
                              iSegment * nHeaderLenSize, 
                              nHeaderLenSize));
        psInfo->nSegmentSize = 
            atoi(NITFGetField(szTemp,pachSegDef, 
                              iSegment * nDataLenSize + nCount*nHeaderLenSize, 
                              nDataLenSize));

        psInfo->nSegmentHeaderStart = *pnNextData;
        psInfo->nSegmentStart = *pnNextData + psInfo->nSegmentHeaderSize;

        *pnNextData += (psInfo->nSegmentHeaderStart+psInfo->nSegmentSize);
        psFile->nSegmentCount++;
    }

    CPLFree( pachSegDef );

    return nOffset + nSegDefSize + 3;
}

/************************************************************************/
/*                            NITFGetField()                            */
/*                                                                      */
/*      Copy a field from a passed in header buffer into a temporary    */
/*      buffer and zero terminate it.                                   */
/************************************************************************/

char *NITFGetField( char *pszTarget, const char *pszSource, 
                    int nStart, int nLength )

{
    memcpy( pszTarget, pszSource + nStart, nLength );
    pszTarget[nLength] = '\0';

    return pszTarget;
}
