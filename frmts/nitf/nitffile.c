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
 * Revision 1.3  2002/12/03 18:07:59  warmerda
 * load VQLUTs
 *
 * Revision 1.2  2002/12/03 04:43:54  warmerda
 * lots of work
 *
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
static void NITFLoadLocationTable( NITFFile *psFile );
static int NITFLoadVQTables( NITFFile *psFile );

/************************************************************************/
/*                              NITFOpen()                              */
/************************************************************************/

NITFFile *NITFOpen( const char *pszFilename, int bUpdatable )

{
    FILE	*fp;
    char        *pachHeader;
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
/*      Check file type.                                                */
/* -------------------------------------------------------------------- */
    VSIFRead( szTemp, 1, 4, fp );

    if( !EQUALN(szTemp,"NITF",4) && !EQUALN(szTemp,"NSIF",4) )
    {
        CPLError( CE_Failure, CPLE_AppDefined, 
                  "The file %s is not an NITF file.", 
                  pszFilename );
        return NULL;
    }

/* -------------------------------------------------------------------- */
/*      Get header length.                                              */
/* -------------------------------------------------------------------- */
    VSIFSeek( fp, 354, SEEK_SET );
    VSIFRead( szTemp, 1, 6, fp );
    szTemp[6] = '\0';

    nHeaderLen = atoi(szTemp);

/* -------------------------------------------------------------------- */
/*      Read the whole file header.                                     */
/* -------------------------------------------------------------------- */
    pachHeader = (char *) CPLMalloc(nHeaderLen);
    VSIFSeek( fp, 0, SEEK_SET );
    VSIFRead( pachHeader, 1, nHeaderLen, fp );

/* -------------------------------------------------------------------- */
/*      Create and initialize info structure about file.                */
/* -------------------------------------------------------------------- */
    psFile = (NITFFile *) CPLCalloc(sizeof(NITFFile),1);
    psFile->fp = fp;
    psFile->pachHeader = pachHeader;

/* -------------------------------------------------------------------- */
/*      Get version.                                                    */
/* -------------------------------------------------------------------- */
    NITFGetField( psFile->szVersion, pachHeader, 4, 5 );

/* -------------------------------------------------------------------- */
/*      Collect segment info for the types we care about.               */
/* -------------------------------------------------------------------- */
    nNextData = nHeaderLen;

    nOffset = NITFCollectSegmentInfo( psFile, 360, "IM", 6, 10, &nNextData );

    nOffset = NITFCollectSegmentInfo( psFile, nOffset, "GR", 4, 6, &nNextData);

    nOffset += 3; /* NUMX reserved field */

    nOffset = NITFCollectSegmentInfo( psFile, nOffset, "TX", 4, 5, &nNextData);

    nOffset = NITFCollectSegmentInfo( psFile, nOffset, "DE", 4, 9, &nNextData);

    nOffset = NITFCollectSegmentInfo( psFile, nOffset, "RE", 4, 7, &nNextData);

/* -------------------------------------------------------------------- */
/*      Is there a TRE to suck up?                                      */
/* -------------------------------------------------------------------- */
    psFile->nTREBytes = 
        atoi(NITFGetField( szTemp, pachHeader, nOffset, 5 ));
    nOffset += 5;

    nOffset += 3; /* UDHOFL */

    if( psFile->nTREBytes != 0 )
    {
        psFile->pachTRE = pachHeader + nOffset;
        psFile->nTREBytes -= 3;
    }

/* -------------------------------------------------------------------- */
/*      Are the VQ tables to load up?                                   */
/* -------------------------------------------------------------------- */
    NITFLoadLocationTable( psFile );
    NITFLoadVQTables( psFile );

    return psFile;
}

/************************************************************************/
/*                             NITFClose()                              */
/************************************************************************/

void NITFClose( NITFFile *psFile )

{
    int  iSegment, i;

    for( iSegment = 0; iSegment < psFile->nSegmentCount; iSegment++ )
    {
        NITFSegmentInfo *psSegInfo = psFile->pasSegmentInfo + iSegment;

        if( psSegInfo->hAccess == NULL )
            continue;

        if( EQUAL(psSegInfo->szSegmentType,"IM"))
            NITFImageDeaccess( (NITFImage *) psSegInfo->hAccess );
        else
        {
            CPLAssert( FALSE );
        }
    }

    CPLFree( psFile->pasSegmentInfo );
    CPLFree( psFile->pasLocations );
    for( i = 0; i < 4; i++ )
        CPLFree( psFile->apanVQLUT[i] );

    if( psFile->fp != NULL )
        VSIFClose( psFile->fp );
    CPLFree( psFile->pachHeader );
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

        *pnNextData += (psInfo->nSegmentHeaderSize+psInfo->nSegmentSize);
        psFile->nSegmentCount++;
    }

    CPLFree( pachSegDef );

    return nOffset + nSegDefSize + 3;
}

/************************************************************************/
/*                       NITFLoadLocationTable()                        */
/************************************************************************/

static void NITFLoadLocationTable( NITFFile *psFile )

{
    GUInt32  nLocTableOffset;
    GUInt16  nLocCount;
    int      iLoc;

/* -------------------------------------------------------------------- */
/*      Get the location table position from within the RPFHDR TRE      */
/*      structure.                                                      */
/* -------------------------------------------------------------------- */
    if( psFile->pachTRE == NULL || !EQUALN(psFile->pachTRE,"RPFHDR",6) )
        return;

    memcpy( &nLocTableOffset, psFile->pachTRE + 55, 4 );
    nLocTableOffset = CPL_MSBWORD32( nLocTableOffset );
    
    if( nLocTableOffset == 0 )
        return;

/* -------------------------------------------------------------------- */
/*      Read the count of entries in the location table.                */
/* -------------------------------------------------------------------- */
    VSIFSeek( psFile->fp, nLocTableOffset + 6, SEEK_SET );
    VSIFRead( &nLocCount, 1, 2, psFile->fp );
    nLocCount = CPL_MSBWORD16( nLocCount );
    psFile->nLocCount = nLocCount;

    psFile->pasLocations = (NITFLocation *) 
        CPLCalloc(sizeof(NITFLocation), nLocCount);
    
/* -------------------------------------------------------------------- */
/*      Process the locations.                                          */
/* -------------------------------------------------------------------- */
    VSIFSeek( psFile->fp, 6, SEEK_CUR );
    for( iLoc = 0; iLoc < nLocCount; iLoc++ )
    {
        unsigned char abyEntry[10];
        
        VSIFRead( abyEntry, 1, 10, psFile->fp );
        
        psFile->pasLocations[iLoc].nLocId = abyEntry[0] * 256 + abyEntry[1];

        CPL_MSBPTR32( abyEntry + 2 );
        memcpy( &(psFile->pasLocations[iLoc].nLocSize), abyEntry + 2, 4 );

        CPL_MSBPTR32( abyEntry + 6 );
        memcpy( &(psFile->pasLocations[iLoc].nLocOffset), abyEntry + 6, 4 );
    }
}

/************************************************************************/
/*                          NITFLoadVQTables()                          */
/************************************************************************/

static int NITFLoadVQTables( NITFFile *psFile )

{
    int     i, nVQOffset=0, nVQSize=0;

/* -------------------------------------------------------------------- */
/*      Do we already have the VQ tables?                               */
/* -------------------------------------------------------------------- */
    if( psFile->apanVQLUT[0] != NULL )
        return TRUE;

/* -------------------------------------------------------------------- */
/*      Do we have the location information?                            */
/* -------------------------------------------------------------------- */
    for( i = 0; i < psFile->nLocCount; i++ )
    {
        if( psFile->pasLocations[i].nLocId == 132 )
        {
            nVQOffset = psFile->pasLocations[i].nLocOffset;
            nVQSize = psFile->pasLocations[i].nLocSize;
        }
    }

    if( nVQOffset == 0 )
        return FALSE;

/* -------------------------------------------------------------------- */
/*      Load the tables.                                                */
/* -------------------------------------------------------------------- */
    for( i = 0; i < 4; i++ )
    {
        GUInt32 nVQVector;

        psFile->apanVQLUT[i] = (GUInt32 *) CPLCalloc(4096,sizeof(GUInt32));

        VSIFSeek( psFile->fp, nVQOffset + 6 + i*14 + 10, SEEK_SET );
        VSIFRead( &nVQVector, 1, 4, psFile->fp );
        nVQVector = CPL_MSBWORD32( nVQVector );
        
        VSIFSeek( psFile->fp, nVQOffset + nVQVector, SEEK_SET );
        VSIFRead( psFile->apanVQLUT[i], 4, 4096, psFile->fp );
    }

    return TRUE;
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
