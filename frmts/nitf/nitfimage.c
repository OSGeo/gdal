/******************************************************************************
 * $Id$
 *
 * Project:  NITF Read/Write Library
 * Purpose:  Module responsible for implementation of most NITFImage 
 *           implementation.
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
 * Revision 1.1  2002/12/03 04:43:41  warmerda
 * New
 *
 */

#include "nitflib.h"
#include "cpl_vsi.h"
#include "cpl_conv.h"

CPL_CVSID("$Id$");

static char *NITFTrimWhite( char * );

/************************************************************************/
/*                          NITFImageAccess()                           */
/************************************************************************/

NITFImage *NITFImageAccess( NITFFile *psFile, int iSegment )

{
    NITFImage *psImage;
    char      *pachHeader;
    NITFSegmentInfo *psSegInfo;
    char       szTemp[128];
    int        nOffset, iBand;

/* -------------------------------------------------------------------- */
/*      Verify segment, and return existing image accessor if there     */
/*      is one.                                                         */
/* -------------------------------------------------------------------- */
    if( iSegment < 0 || iSegment >= psFile->nSegmentCount )
        return NULL;

    psSegInfo = psFile->pasSegmentInfo + iSegment;

    if( !EQUAL(psSegInfo->szSegmentType,"IM") )
        return NULL;

    if( psSegInfo->hAccess != NULL )
        return (NITFImage *) psSegInfo->hAccess;

/* -------------------------------------------------------------------- */
/*      Read the image subheader.                                       */
/* -------------------------------------------------------------------- */
    pachHeader = (char*) CPLMalloc(psSegInfo->nSegmentHeaderSize);
    if( VSIFSeek( psFile->fp, psSegInfo->nSegmentHeaderStart, 
                  SEEK_SET ) != 0 
        || VSIFRead( pachHeader, 1, psSegInfo->nSegmentHeaderSize, 
                     psFile->fp ) != psSegInfo->nSegmentHeaderSize )
    {
        CPLError( CE_Failure, CPLE_FileIO, 
                  "Failed to read %d byte image subheader from %d.",
                  psSegInfo->nSegmentHeaderSize,
                  psSegInfo->nSegmentHeaderStart );
        return NULL;
    }

/* -------------------------------------------------------------------- */
/*      Initialize image object.                                        */
/* -------------------------------------------------------------------- */
    psImage = (NITFImage *) CPLCalloc(sizeof(NITFImage),1);

    psImage->psFile = psFile;
    psImage->iSegment = iSegment;
    psImage->pachHeader = pachHeader;

    psSegInfo->hAccess = psImage;

/* -------------------------------------------------------------------- */
/*      Read lots of header fields.                                     */
/* -------------------------------------------------------------------- */
    psImage->nRows = atoi(NITFGetField(szTemp,pachHeader,333,8));
    psImage->nCols = atoi(NITFGetField(szTemp,pachHeader,341,8));
    
    NITFTrimWhite( NITFGetField( psImage->szPVType, pachHeader, 349, 3 ) );
    NITFTrimWhite( NITFGetField( psImage->szIREP, pachHeader, 352, 8 ) );
    NITFTrimWhite( NITFGetField( psImage->szICAT, pachHeader, 360, 8 ) );

/* -------------------------------------------------------------------- */
/*      Read the image bounds.                                          */
/* -------------------------------------------------------------------- */
    nOffset = 371;
    psImage->chICORDS = pachHeader[nOffset++];

    if( psImage->chICORDS != ' ' && psImage->chICORDS != 'N' )
        nOffset += 60;

/* -------------------------------------------------------------------- */
/*      Read the image comments.                                        */
/* -------------------------------------------------------------------- */
    {
        int nNICOM;

        nNICOM = atoi(NITFGetField( szTemp, pachHeader, nOffset++, 1));

        psImage->pszComments = (char *) CPLMalloc(nNICOM*80+1);
        NITFGetField( psImage->pszComments, pachHeader,
                      nOffset, 80 * nNICOM );
        nOffset += nNICOM * 80;
    }
    
/* -------------------------------------------------------------------- */
/*      Read more stuff.                                                */
/* -------------------------------------------------------------------- */
    NITFGetField( psImage->szIC, pachHeader, nOffset, 2 );
    nOffset += 2;

    if( psImage->szIC[1] == '1' 
        || psImage->szIC[1] == '3' 
        || psImage->szIC[1] == '4' 
        || psImage->szIC[1] == '5' 
        || psImage->szIC[1] == '3' )
    {
        NITFGetField( psImage->szCOMRAT, pachHeader, nOffset, 4 );
        nOffset += 4;
    }

    /* NBANDS */
    psImage->nBands = atoi(NITFGetField(szTemp,pachHeader,nOffset,1));
    nOffset++;

    /* XBANDS */
    if( psImage->nBands == 0 )
    {
        psImage->nBands = atoi(NITFGetField(szTemp,pachHeader,nOffset,5));
        nOffset += 5;
    }

/* -------------------------------------------------------------------- */
/*      Read per-band information.                                      */
/* -------------------------------------------------------------------- */
    psImage->pasBandInfo = (NITFBandInfo *) 
        CPLCalloc(sizeof(NITFBandInfo),psImage->nBands);
    
    for( iBand = 0; iBand < psImage->nBands; iBand++ )
    {
        NITFBandInfo *psBandInfo = psImage->pasBandInfo + iBand;
        int nLUTS;

        NITFTrimWhite(
            NITFGetField( psBandInfo->szIREPBAND, pachHeader, nOffset, 2 ) );
        nOffset += 2;

        NITFTrimWhite(
            NITFGetField( psBandInfo->szISUBCAT, pachHeader, nOffset, 6 ) );
        nOffset += 6;

        nOffset += 4; /* Skip IFCn and IMFLTn */

        nLUTS = atoi(NITFGetField( szTemp, pachHeader, nOffset, 1 ));
        nOffset += 1;

        if( nLUTS == 0 )
            continue;

        psBandInfo->nSignificantLUTEntries = 
            atoi(NITFGetField( szTemp, pachHeader, nOffset, 5 ));
        nOffset += 5;

        psBandInfo->pabyLUT = (unsigned char *) CPLCalloc(768,1);
        
        memcpy( psBandInfo->pabyLUT, pachHeader + nOffset, 
                psBandInfo->nSignificantLUTEntries );
        nOffset += psBandInfo->nSignificantLUTEntries;

        if( nLUTS == 3 )
        {
            memcpy( psBandInfo->pabyLUT+256, pachHeader + nOffset, 
                    psBandInfo->nSignificantLUTEntries );
            nOffset += psBandInfo->nSignificantLUTEntries;
            
            memcpy( psBandInfo->pabyLUT+512, pachHeader + nOffset, 
                    psBandInfo->nSignificantLUTEntries );
            nOffset += psBandInfo->nSignificantLUTEntries;
        }
        else 
        {
            /* morph greyscale lut into RGB LUT. */
            memcpy( psBandInfo->pabyLUT+256, psBandInfo->pabyLUT, 256 );
            memcpy( psBandInfo->pabyLUT+512, psBandInfo->pabyLUT, 256 );
        }
    }								

/* -------------------------------------------------------------------- */
/*      Read more header fields.                                        */
/* -------------------------------------------------------------------- */
    psImage->chIMODE = pachHeader[nOffset + 1];

    psImage->nBlocksPerRow = 
        atoi(NITFGetField(szTemp, pachHeader, nOffset+2, 4));
    psImage->nBlocksPerColumn = 
        atoi(NITFGetField(szTemp, pachHeader, nOffset+6, 4));
    psImage->nBlockWidth = 
        atoi(NITFGetField(szTemp, pachHeader, nOffset+10, 4));
    psImage->nBlockHeight = 
        atoi(NITFGetField(szTemp, pachHeader, nOffset+14, 4));
    psImage->nBitsPerSample = 
        atoi(NITFGetField(szTemp, pachHeader, nOffset+18, 2));

    nOffset += 20;

/* -------------------------------------------------------------------- */
/*      Setup some image access values.  Some of these may not apply    */
/*      for compressed images, or band interleaved by block images.     */
/* -------------------------------------------------------------------- */
    psImage->nWordSize = psImage->nBitsPerSample / 8;
    if( psImage->chIMODE == 'S' )
    {
        psImage->nPixelOffset = psImage->nWordSize;
        psImage->nLineOffset = psImage->nBlockWidth * psImage->nPixelOffset;
        psImage->nBlockOffset = psImage->nLineOffset * psImage->nBlockHeight;
        psImage->nBandOffset = psImage->nBlockOffset * psImage->nBlocksPerRow 
            * psImage->nBlocksPerColumn;
    }
    else if( psImage->chIMODE == 'P' )
    {
        psImage->nPixelOffset = psImage->nWordSize * psImage->nBands;
        psImage->nLineOffset = psImage->nBlockWidth * psImage->nPixelOffset;
        psImage->nBandOffset = psImage->nWordSize;
        psImage->nBlockOffset = psImage->nLineOffset * psImage->nBlockHeight;
    }
    else if( psImage->chIMODE == 'R' )
    {
        psImage->nPixelOffset = psImage->nWordSize;
        psImage->nBandOffset = psImage->nBlockWidth * psImage->nPixelOffset;
        psImage->nLineOffset = psImage->nBandOffset * psImage->nBands;
        psImage->nBlockOffset = psImage->nLineOffset * psImage->nBlockHeight;
    }
    else if( psImage->chIMODE == 'B' )
    {
        psImage->nPixelOffset = psImage->nWordSize;
        psImage->nLineOffset = psImage->nBlockWidth * psImage->nPixelOffset;
        psImage->nBandOffset = psImage->nBlockHeight * psImage->nLineOffset;
        psImage->nBlockOffset = psImage->nBandOffset * psImage->nBands;
    }
    else
    {
//        CPLAssert( FALSE );
        psImage->nPixelOffset = psImage->nWordSize;
        psImage->nLineOffset = psImage->nBlockWidth * psImage->nPixelOffset;
        psImage->nBandOffset = psImage->nBlockHeight * psImage->nLineOffset;
        psImage->nBlockOffset = psImage->nBandOffset * psImage->nBands;
    }

/* -------------------------------------------------------------------- */
/*      Setup block map.                                                */
/* -------------------------------------------------------------------- */
    psImage->panBlockStart = (GUInt32 *) 
        CPLCalloc( psImage->nBlocksPerRow * psImage->nBlocksPerColumn 
                   * psImage->nBands, sizeof(GUInt32) );

/* -------------------------------------------------------------------- */
/*      If there is no block map, just compute directly assuming the    */
/*      blocks start at the beginning of the image segment, and are     */
/*      packed tightly with the IMODE organization.                     */
/* -------------------------------------------------------------------- */
    if( psImage->szIC[0] != 'M' && psImage->szIC[1] != 'M' )
    {
        int iBlockX, iBlockY, iBand;

        for( iBlockY = 0; iBlockY < psImage->nBlocksPerColumn; iBlockY++ )
        {
            for( iBlockX = 0; iBlockX < psImage->nBlocksPerRow; iBlockX++ )
            {
                for( iBand = 0; iBand < psImage->nBands; iBand++ )
                {
                    int iBlock;

                    iBlock = iBlockX + iBlockY * psImage->nBlocksPerRow
                        + iBand * psImage->nBlocksPerRow 
                        * psImage->nBlocksPerColumn;
                    
                    psImage->panBlockStart[iBlock] = 
                        psSegInfo->nSegmentStart
                        + ((iBlockX + iBlockY * psImage->nBlocksPerRow) 
                           * psImage->nBlockOffset)
                        + (iBand * psImage->nBandOffset );
                }
            }
        }
    }

/* -------------------------------------------------------------------- */
/*      Otherwise we need to read the block map from the beginning      */
/*      of the image segment.                                           */
/* -------------------------------------------------------------------- */
    else
    {
        
    }

    return psImage;
}

/************************************************************************/
/*                         NITFImageDeaccess()                          */
/************************************************************************/

void NITFImageDeaccess( NITFImage *psImage )

{
    int  iBand;

    CPLAssert( psImage->psFile->pasSegmentInfo[psImage->iSegment].hAccess
               == psImage );

    psImage->psFile->pasSegmentInfo[psImage->iSegment].hAccess = NULL;

    for( iBand = 0; iBand < psImage->nBands; iBand++ )
        CPLFree( psImage->pasBandInfo[iBand].pabyLUT );
    CPLFree( psImage->pasBandInfo );
    CPLFree( psImage->panBlockStart );
    CPLFree( psImage->pszComments );
    CPLFree( psImage->pachHeader );

    CPLFree( psImage );
}

/************************************************************************/
/*                         NITFReadImageBlock()                         */
/************************************************************************/

int NITFReadImageBlock( NITFImage *psImage, int nBlockX, int nBlockY, 
                        int nBand, void *pData )

{
    int   nWrkBufSize;
    int   iBaseBlock = nBlockX + nBlockY * psImage->nBlocksPerRow;
    int   iFullBlock = iBaseBlock 
        + (nBand-1) * psImage->nBlocksPerRow * psImage->nBlocksPerColumn;

    if( nBand == 0 )
        return BLKREAD_FAIL;

    nWrkBufSize = psImage->nLineOffset * (psImage->nBlockHeight-1)
        + psImage->nPixelOffset * (psImage->nBlockWidth-1)
        + psImage->nWordSize;

/* -------------------------------------------------------------------- */
/*      Can we do a direct read into our buffer?                        */
/* -------------------------------------------------------------------- */
    if( psImage->nWordSize == psImage->nPixelOffset
        && psImage->nWordSize * psImage->nBlockWidth == psImage->nLineOffset 
        && psImage->szIC[0] != 'C' )
    {

        if( VSIFSeek( psImage->psFile->fp, psImage->panBlockStart[iFullBlock], 
                      SEEK_SET ) != 0 
            || VSIFRead( pData, 1, nWrkBufSize,
                         psImage->psFile->fp ) != nWrkBufSize )
        {
            CPLError( CE_Failure, CPLE_FileIO, 
                      "Unable to read %d byte block from %d.", 
                      nWrkBufSize, psImage->panBlockStart[iFullBlock] );
            return BLKREAD_FAIL;
        }
        else
            return BLKREAD_OK;
    }

/* -------------------------------------------------------------------- */
/*      Read the requested information into a temporary buffer.         */
/* -------------------------------------------------------------------- */
    if( psImage->szIC[0] != 'C' )
    {
        GByte *pabyWrkBuf = (GByte *) CPLMalloc(nWrkBufSize);
        int   iPixel, iLine;

        /* read all the data needed to get our requested band-block */
        if( VSIFSeek( psImage->psFile->fp, psImage->panBlockStart[iFullBlock], 
                      SEEK_SET ) != 0 
            || VSIFRead( pabyWrkBuf, 1, nWrkBufSize,
                         psImage->psFile->fp ) != nWrkBufSize )
        {
            CPLError( CE_Failure, CPLE_FileIO, 
                      "Unable to read %d byte block from %d.", 
                      nWrkBufSize, psImage->panBlockStart[iFullBlock] );
            return BLKREAD_FAIL;
        }

        for( iLine = 0; iLine < psImage->nBlockHeight; iLine++ )
        {
            GByte *pabySrc, *pabyDst;

            pabySrc = pabyWrkBuf + iLine * psImage->nLineOffset;
            pabyDst = ((GByte *) pData) 
                + iLine * (psImage->nWordSize * psImage->nBlockWidth);

            for( iPixel = 0; iPixel < psImage->nBlockWidth; iPixel++ )
            {
                memcpy( pabyDst + iPixel * psImage->nWordSize, 
                        pabySrc + iPixel * psImage->nPixelOffset,
                        psImage->nWordSize );
            }
        }

        CPLFree( pabyWrkBuf );

        return BLKREAD_OK;
    }

    return BLKREAD_FAIL;
}

/************************************************************************/
/*                           NITFTrimWhite()                            */
/*                                                                      */
/*      Trim any white space off the white of the passed string in      */
/*      place.                                                          */
/************************************************************************/

char *NITFTrimWhite( char *pszTarget )

{
    int i;

    i = strlen(pszTarget)-1;
    while( i >= 0 && pszTarget[i] == ' ' )
        pszTarget[i--] = '\0';

    return pszTarget;
}
