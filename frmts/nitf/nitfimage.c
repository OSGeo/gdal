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
 * Revision 1.12  2003/04/09 07:10:47  warmerda
 * Added byte swapping for non-eight bit data on little endian platforms.
 * Multi-byte NITF image data is always bigendian.
 * http://bugzilla.remotesensing.org/show_bug.cgi?id=316
 *
 * Revision 1.11  2003/03/28 13:57:12  warmerda
 * fixed C++ comment (per bug 311)
 *
 * Revision 1.10  2002/12/18 21:18:11  warmerda
 * read RPF CoverageSection for more exact geotransform
 *
 * Revision 1.9  2002/12/18 20:16:04  warmerda
 * support writing IGEOLO
 *
 * Revision 1.8  2002/12/18 06:49:21  warmerda
 * implement support for mapped files without a map (BMRLNTH==0)
 *
 * Revision 1.7  2002/12/18 06:35:15  warmerda
 * implement nodata support for mapped data
 *
 * Revision 1.6  2002/12/17 22:01:27  warmerda
 * implemented support for IC=M4 overview.ovr
 *
 * Revision 1.5  2002/12/17 21:23:15  warmerda
 * implement LUT reading and writing
 *
 * Revision 1.4  2002/12/17 20:03:08  warmerda
 * added rudimentary NITF 1.1 support
 *
 * Revision 1.3  2002/12/17 05:26:26  warmerda
 * implement basic write support
 *
 * Revision 1.2  2002/12/03 18:08:16  warmerda
 * implemented nitf VQ decompression
 *
 * Revision 1.1  2002/12/03 04:43:41  warmerda
 * New
 *
 */

#include "nitflib.h"
#include "cpl_vsi.h"
#include "cpl_conv.h"

CPL_CVSID("$Id$");

static char *NITFTrimWhite( char * );
static int NITFIsAllDigits( const char *, int );
static void NITFSwapWords( void *pData, int nWordSize, int nWordCount,
                           int nWordSkip );

/************************************************************************/
/*                          NITFImageAccess()                           */
/************************************************************************/

NITFImage *NITFImageAccess( NITFFile *psFile, int iSegment )

{
    NITFImage *psImage;
    char      *pachHeader;
    NITFSegmentInfo *psSegInfo;
    char       szTemp[128];
    int        nOffset, iBand, i;

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
/*      Is this an old NITF 1.1 image?  If so, there seems to be an     */
/*      extra 40 bytes of info in the security area.                    */
/* -------------------------------------------------------------------- */
    nOffset = 333;

    if( EQUALN(psFile->szVersion,"NITF01.",7) )
        nOffset += 40;

/* -------------------------------------------------------------------- */
/*      Read lots of header fields.                                     */
/* -------------------------------------------------------------------- */
    if( !EQUALN(psFile->szVersion,"NITF01.",7) )
    {
        psImage->nRows = atoi(NITFGetField(szTemp,pachHeader,nOffset,8));
        psImage->nCols = atoi(NITFGetField(szTemp,pachHeader,nOffset+8,8));
        
        NITFTrimWhite( NITFGetField( psImage->szPVType, pachHeader, 
                                     nOffset+16, 3) );
        NITFTrimWhite( NITFGetField( psImage->szIREP, pachHeader, 
                                     nOffset+19, 8) );
        NITFTrimWhite( NITFGetField( psImage->szICAT, pachHeader, 
                                     nOffset+27, 8) );
    }

    nOffset += 38;

/* -------------------------------------------------------------------- */
/*      Read the image bounds.  According to the specification the      */
/*      60 character IGEOGLO field should occur unless the ICORDS is    */
/*      ' '; however, some datasets (ie. an ADRG OVERVIEW.OVR file)     */
/*      have 'N' in the ICORDS but still no IGEOGLO.  To detect this    */
/*      we verify that the IGEOGLO value seems valid before             */
/*      accepting that it must be there.                                */
/* -------------------------------------------------------------------- */
    psImage->chICORDS = pachHeader[nOffset++];

    if( psImage->chICORDS != ' ' 
        && (psImage->chICORDS != 'N' 
            || NITFIsAllDigits( pachHeader+nOffset, 60)) )
    {
        int iCoord;

        for( iCoord = 0; iCoord < 4; iCoord++ )
        {
            const char *pszCoordPair = pachHeader + nOffset + iCoord*15;
            double *pdfXY = &(psImage->dfULX) + iCoord*2;

            if( psImage->chICORDS == 'N' || psImage->chICORDS == 'S' )
            {
                psImage->nZone = 
                    atoi(NITFGetField( szTemp, pszCoordPair, 0, 2 ));

                pdfXY[0] = atof(NITFGetField( szTemp, pszCoordPair, 2, 6 ));
                pdfXY[1] = atof(NITFGetField( szTemp, pszCoordPair, 8, 7 ));
            }
            else if( psImage->chICORDS == 'G' )
            {
                pdfXY[1] = 
                    atof(NITFGetField( szTemp, pszCoordPair, 0, 2 )) 
                  + atof(NITFGetField( szTemp, pszCoordPair, 2, 2 )) / 60.0
                  + atof(NITFGetField( szTemp, pszCoordPair, 4, 2 )) / 3600.0;
                if( pszCoordPair[6] == 's' || pszCoordPair[6] == 'S' )
                    pdfXY[1] *= -1;

                pdfXY[0] = 
                    atof(NITFGetField( szTemp, pszCoordPair, 7, 3 )) 
                  + atof(NITFGetField( szTemp, pszCoordPair,10, 2 )) / 60.0
                  + atof(NITFGetField( szTemp, pszCoordPair,12, 2 )) / 3600.0;

                if( pszCoordPair[14] == 'w' || pszCoordPair[14] == 'W' )
                    pdfXY[0] *= -1;
            }
        }

        nOffset += 60;
    }

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

    if( psImage->szIC[0] != 'N' )
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

        psBandInfo->nLUTLocation = nOffset + psSegInfo->nSegmentHeaderStart;

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

    if( EQUALN(psFile->szVersion,"NITF01.",7) )
    {
        psImage->nCols = psImage->nBlocksPerRow * psImage->nBlockWidth;
        psImage->nRows = psImage->nBlocksPerColumn * psImage->nBlockHeight;
    }

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
/*      Offsets to VQ compressed tiles are based on a fixed block       */
/*      size, and are offset from the spatial data location kept in     */
/*      the location table ... which is generally not the beginning     */
/*      of the image data segment.                                      */
/* -------------------------------------------------------------------- */
    if( EQUAL(psImage->szIC,"C4") )
    {
        GUInt32  nLocBase = psSegInfo->nSegmentStart;

        for( i = 0; i < psFile->nLocCount; i++ )
        {
            if( psFile->pasLocations[i].nLocId == 140 )
                nLocBase = psFile->pasLocations[i].nLocOffset;
        }

        if( nLocBase == psSegInfo->nSegmentStart )
            CPLError( CE_Warning, CPLE_AppDefined, 
                      "Failed to find spatial data location, guessing." );

        for( i=0; i < psImage->nBlocksPerRow * psImage->nBlocksPerColumn; i++ )
            psImage->panBlockStart[i] = nLocBase + 6144 * i;
    }

/* -------------------------------------------------------------------- */
/*      If there is no block map, just compute directly assuming the    */
/*      blocks start at the beginning of the image segment, and are     */
/*      packed tightly with the IMODE organization.                     */
/* -------------------------------------------------------------------- */
    else if( psImage->szIC[0] != 'M' && psImage->szIC[1] != 'M' )
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
        GUInt32  nIMDATOFF;
        GUInt16  nBMRLNTH, nTMRLNTH, nTPXCDLNTH;
        int nBlockCount = psImage->nBlocksPerRow * psImage->nBlocksPerColumn
            * psImage->nBands;

        CPLAssert( psImage->szIC[0] == 'M' || psImage->szIC[1] == 'M' );

        VSIFSeek( psFile->fp, psSegInfo->nSegmentStart, SEEK_SET );
        VSIFRead( &nIMDATOFF, 1, 4, psFile->fp );
        VSIFRead( &nBMRLNTH, 1, 2, psFile->fp );
        VSIFRead( &nTMRLNTH, 1, 2, psFile->fp );
        VSIFRead( &nTPXCDLNTH, 1, 2, psFile->fp );

        CPL_MSBPTR32( &nIMDATOFF );
        CPL_MSBPTR16( &nBMRLNTH );
        CPL_MSBPTR16( &nTMRLNTH );
        CPL_MSBPTR16( &nTPXCDLNTH );

        if( nTPXCDLNTH == 8 )
        {
            GByte byNodata;

            psImage->bNoDataSet = TRUE;
            VSIFRead( &byNodata, 1, 1, psFile->fp );
            psImage->nNoDataValue = byNodata;
        }
        else
            VSIFSeek( psFile->fp, (nTPXCDLNTH+7)/8, SEEK_CUR );

        if( nBMRLNTH == 4 )
        {
            VSIFRead( psImage->panBlockStart, 4, nBlockCount, psFile->fp );
            for( i=0; i < nBlockCount; i++ )
            {
                CPL_MSBPTR32( psImage->panBlockStart + i );
                if( psImage->panBlockStart[i] != 0xffffffff )
                    psImage->panBlockStart[i] 
                        += psSegInfo->nSegmentStart + nIMDATOFF;
            }
        }
        else
        {
            for( i=0; i < nBlockCount; i++ )
            {
                if( EQUAL(psImage->szIC,"M4") )
                    psImage->panBlockStart[i] = 6144 * i
                        + psSegInfo->nSegmentStart + nIMDATOFF;
                else if( EQUAL(psImage->szIC,"NM") )
                    psImage->panBlockStart[i] = 
                        psImage->nBlockOffset * i
                        + psSegInfo->nSegmentStart + nIMDATOFF;
            }
        }
    }

/* -------------------------------------------------------------------- */
/*      If we have an RPF CoverageSectionSubheader, read the more       */
/*      precise bounds from it.                                         */
/* -------------------------------------------------------------------- */
    for( i = 0; i < psFile->nLocCount; i++ )
    {
        if( psFile->pasLocations[i].nLocId == LID_CoverageSectionSubheader )
        {
            double adfTarget[8];

            VSIFSeek( psFile->fp, psFile->pasLocations[i].nLocOffset,
                      SEEK_SET );
            VSIFRead( adfTarget, 8, 8, psFile->fp );
            for( i = 0; i < 8; i++ )
                CPL_MSBPTR64( (adfTarget + i) );

            psImage->dfULX = adfTarget[1];
            psImage->dfULY = adfTarget[0];
            psImage->dfLLX = adfTarget[3];
            psImage->dfLLY = adfTarget[2];
            psImage->dfURX = adfTarget[5];
            psImage->dfURY = adfTarget[4];
            psImage->dfLRX = adfTarget[7];
            psImage->dfLRY = adfTarget[6];

            CPLDebug( "NITF", "Got spatial info from CoverageSection" );
            break;
        }
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
/*                        NITFUncompressVQTile()                        */
/*                                                                      */
/*      This code was derived from OSSIM which in turn derived it       */
/*      from OpenMap ... open source means sharing!                     */
/************************************************************************/

static void NITFUncompressVQTile( NITFImage *psImage, 
                                  GByte *pabyVQBuf,
                                  GByte *pabyResult )

{
    int   i, j, t, iSrcByte = 0;

    for (i = 0; i < 256; i += 4)
    {
        for (j = 0; j < 256; j += 8)
        {
            GUInt16 firstByte  = pabyVQBuf[iSrcByte++];
            GUInt16 secondByte = pabyVQBuf[iSrcByte++];
            GUInt16 thirdByte  = pabyVQBuf[iSrcByte++];

            /*
             * because dealing with half-bytes is hard, we
             * uncompress two 4x4 tiles at the same time. (a
             * 4x4 tile compressed is 12 bits )
             * this little code was grabbed from openmap software.
             */
                  
            /* Get first 12-bit value as index into VQ table */

            GUInt16 val1 = (firstByte << 4) | (secondByte >> 4);
                  
            /* Get second 12-bit value as index into VQ table*/

            GUInt16 val2 = ((secondByte & 0x000F) << 8) | thirdByte;
                  
            for ( t = 0; t < 4; ++t)
            {
                GByte *pabyTarget = pabyResult + (i+t) * 256 + j;
                
                memcpy( pabyTarget, psImage->psFile->apanVQLUT[t] + val1, 4 );
                memcpy( pabyTarget+4, psImage->psFile->apanVQLUT[t] + val2, 4);
            }
        }  /* for j */
    } /* for i */
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
        && psImage->szIC[0] != 'C' && psImage->szIC[0] != 'M' )
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
        {
#ifdef CPL_LSB
            NITFSwapWords( pData, psImage->nWordSize, 
                           psImage->nBlockWidth * psImage->nBlockHeight, 
                           psImage->nWordSize );
#endif

            return BLKREAD_OK;
        }
    }

/* -------------------------------------------------------------------- */
/*      Read the requested information into a temporary buffer and      */
/*      pull out what we want.                                          */
/* -------------------------------------------------------------------- */
    if( psImage->szIC[0] == 'N' )
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

#ifdef CPL_LSB
        NITFSwapWords( pData, psImage->nWordSize, 
                       psImage->nBlockWidth * psImage->nBlockHeight, 
                       psImage->nWordSize );
#endif

        CPLFree( pabyWrkBuf );

        return BLKREAD_OK;
    }

/* -------------------------------------------------------------------- */
/*      Handle VQ compression.  The VQ compression basically keeps a    */
/*      64x64 array of 12bit code words.  Each code word expands to     */
/*      a predefined 4x4 8 bit per pixel pattern.                       */
/* -------------------------------------------------------------------- */
    else if( EQUAL(psImage->szIC,"C4") || EQUAL(psImage->szIC,"M4") )
    {
        GByte abyVQCoded[6144];

        if( psImage->panBlockStart[iFullBlock] == 0xffffffff )
            return BLKREAD_NULL;

        /* Read the codewords */
        if( VSIFSeek( psImage->psFile->fp, psImage->panBlockStart[iFullBlock], 
                      SEEK_SET ) != 0 
            || VSIFRead( abyVQCoded, 1, sizeof(abyVQCoded),
                         psImage->psFile->fp ) != sizeof(abyVQCoded) )
        {
            CPLError( CE_Failure, CPLE_FileIO, 
                      "Unable to read %d byte block from %d.", 
                      sizeof(abyVQCoded), psImage->panBlockStart[iFullBlock] );
            return BLKREAD_FAIL;
        }
        
        NITFUncompressVQTile( psImage, abyVQCoded, pData );

        return BLKREAD_OK;
    }

    return BLKREAD_FAIL;
}

/************************************************************************/
/*                        NITFWriteImageBlock()                         */
/************************************************************************/

int NITFWriteImageBlock( NITFImage *psImage, int nBlockX, int nBlockY, 
                         int nBand, void *pData )

{
    CPLError( CE_Failure, CPLE_AppDefined,
              "NITFWriteImageBlock() not implemented at this time." );
    return BLKREAD_FAIL;
}

/************************************************************************/
/*                         NITFReadImageLine()                          */
/************************************************************************/

int NITFReadImageLine( NITFImage *psImage, int nLine, int nBand, void *pData )

{
    int   nLineOffsetInFile, nLineSize;
    unsigned char *pabyLineBuf;

    if( nBand == 0 )
        return BLKREAD_FAIL;

    if( psImage->nBlocksPerRow != 1 || psImage->nBlocksPerColumn != 1 )
    {
        CPLError( CE_Failure, CPLE_AppDefined, 
                  "Scanline access not supported on tiled NITF files." );
        return BLKREAD_FAIL;
    }

    if( !EQUAL(psImage->szIC,"NC") )
    {
        CPLError( CE_Failure, CPLE_AppDefined, 
                  "Scanline access not supported on compressed NITF files." );
        return BLKREAD_FAIL;
    }

/* -------------------------------------------------------------------- */
/*      Workout location and size of data in file.                      */
/* -------------------------------------------------------------------- */
    nLineOffsetInFile = psImage->panBlockStart[0]
        + psImage->nLineOffset * nLine
        + psImage->nBandOffset * (nBand-1);

    nLineSize = psImage->nPixelOffset * (psImage->nCols - 1) 
        + psImage->nWordSize;

    VSIFSeek( psImage->psFile->fp, nLineOffsetInFile, SEEK_SET );

/* -------------------------------------------------------------------- */
/*      Can we do a direct read into our buffer.                        */
/* -------------------------------------------------------------------- */
    if( psImage->nWordSize == psImage->nPixelOffset
        && psImage->nWordSize * psImage->nBlockWidth == psImage->nLineOffset )
    {
        VSIFRead( pData, 1, nLineSize, psImage->psFile->fp );

#ifdef CPL_LSB
        NITFSwapWords( pData, psImage->nWordSize, 
                       psImage->nBlockWidth, psImage->nWordSize );
#endif

        return BLKREAD_OK;
    }

/* -------------------------------------------------------------------- */
/*      Allocate a buffer for all the interleaved data, and read        */
/*      it.                                                             */
/* -------------------------------------------------------------------- */
    pabyLineBuf = (unsigned char *) CPLMalloc(nLineSize);
    VSIFRead( pabyLineBuf, 1, nLineSize, psImage->psFile->fp );

/* -------------------------------------------------------------------- */
/*      Copy the desired data out of the interleaved buffer.            */
/* -------------------------------------------------------------------- */
    {
        GByte *pabySrc, *pabyDst;
        int iPixel;
        
        pabySrc = pabyLineBuf;
        pabyDst = ((GByte *) pData);
        
        for( iPixel = 0; iPixel < psImage->nBlockWidth; iPixel++ )
        {
            memcpy( pabyDst + iPixel * psImage->nWordSize, 
                    pabySrc + iPixel * psImage->nPixelOffset,
                    psImage->nWordSize );
        }

#ifdef CPL_LSB
        NITFSwapWords( (void *) pabyDst, psImage->nWordSize, 
                       psImage->nBlockWidth, psImage->nWordSize );
#endif
    }

    CPLFree( pabyLineBuf );

    return BLKREAD_OK;
}

/************************************************************************/
/*                         NITFWriteImageLine()                         */
/************************************************************************/

int NITFWriteImageLine( NITFImage *psImage, int nLine, int nBand, void *pData )

{
    int   nLineOffsetInFile, nLineSize;
    unsigned char *pabyLineBuf;

    if( nBand == 0 )
        return BLKREAD_FAIL;

    if( psImage->nBlocksPerRow != 1 || psImage->nBlocksPerColumn != 1 )
    {
        CPLError( CE_Failure, CPLE_AppDefined, 
                  "Scanline access not supported on tiled NITF files." );
        return BLKREAD_FAIL;
    }

    if( !EQUAL(psImage->szIC,"NC") )
    {
        CPLError( CE_Failure, CPLE_AppDefined, 
                  "Scanline access not supported on compressed NITF files." );
        return BLKREAD_FAIL;
    }

/* -------------------------------------------------------------------- */
/*      Workout location and size of data in file.                      */
/* -------------------------------------------------------------------- */
    nLineOffsetInFile = psImage->panBlockStart[0]
        + psImage->nLineOffset * nLine
        + psImage->nBandOffset * (nBand-1);

    nLineSize = psImage->nPixelOffset * (psImage->nCols - 1) 
        + psImage->nWordSize;

    VSIFSeek( psImage->psFile->fp, nLineOffsetInFile, SEEK_SET );

/* -------------------------------------------------------------------- */
/*      Can we do a direct write into our buffer.                       */
/* -------------------------------------------------------------------- */
    if( psImage->nWordSize == psImage->nPixelOffset
        && psImage->nWordSize * psImage->nBlockWidth == psImage->nLineOffset )
    {
        VSIFWrite( pData, 1, nLineSize, psImage->psFile->fp );

        return BLKREAD_OK;
    }

/* -------------------------------------------------------------------- */
/*      Allocate a buffer for all the interleaved data, and read        */
/*      it.                                                             */
/* -------------------------------------------------------------------- */
    pabyLineBuf = (unsigned char *) CPLMalloc(nLineSize);
    VSIFRead( pabyLineBuf, 1, nLineSize, psImage->psFile->fp );

/* -------------------------------------------------------------------- */
/*      Copy the desired data out of the interleaved buffer.            */
/* -------------------------------------------------------------------- */
    {
        GByte *pabySrc, *pabyDst;
        int iPixel;
        
        pabySrc = pabyLineBuf;
        pabyDst = ((GByte *) pData);
        
        for( iPixel = 0; iPixel < psImage->nBlockWidth; iPixel++ )
        {
            memcpy( pabySrc + iPixel * psImage->nPixelOffset,
                    pabyDst + iPixel * psImage->nWordSize, 
                    psImage->nWordSize );
        }
    }

/* -------------------------------------------------------------------- */
/*      Write the results back out.                                     */
/* -------------------------------------------------------------------- */
    VSIFSeek( psImage->psFile->fp, nLineOffsetInFile, SEEK_SET );
    VSIFWrite( pabyLineBuf, 1, nLineSize, psImage->psFile->fp );
    CPLFree( pabyLineBuf );

    return BLKREAD_OK;
}

/************************************************************************/
/*                          NITFEncodeDMSLoc()                          */
/************************************************************************/

static void NITFEncodeDMSLoc( char *pszTarget, double dfValue, 
                              const char *pszAxis )

{
    char chHemisphere;
    int  nDegrees, nMinutes, nSeconds;

    if( EQUAL(pszAxis,"Lat") )
    {
        if( dfValue < 0.0 )
            chHemisphere = 'S';
        else
            chHemisphere = 'N';
    }
    else
    {
        if( dfValue < 0.0 )
            chHemisphere = 'W';
        else
            chHemisphere = 'E';
    }

    dfValue = fabs(dfValue);

    nDegrees = (int) dfValue;
    dfValue = (dfValue-nDegrees) * 60.0;

    nMinutes = (int) dfValue;
    dfValue = (dfValue-nMinutes) * 60.0;

    nSeconds = (int) dfValue;

    if( EQUAL(pszAxis,"Lat") )
        sprintf( pszTarget, "%02d%02d%02d%c", 
                 nDegrees, nMinutes, nSeconds, chHemisphere );
    else
        sprintf( pszTarget, "%03d%02d%02d%c", 
                 nDegrees, nMinutes, nSeconds, chHemisphere );
}

/************************************************************************/
/*                          NITFWriteIGEOLO()                           */
/************************************************************************/

int NITFWriteIGEOLO( NITFImage *psImage, char chICORDS,
                     double dfULX, double dfULY,
                     double dfURX, double dfURY,
                     double dfLRX, double dfLRY,
                     double dfLLX, double dfLLY )

{
    char szIGEOLO[61];

    if( chICORDS != 'G' )
    {
        CPLError( CE_Failure, CPLE_NotSupported, 
                  "Currently NITFWriteIGEOLO() only supports writing ICORDS=G style." );
        return FALSE;
    }

    if( psImage->chICORDS == ' ' )
    {
        CPLError(CE_Failure, CPLE_NotSupported, 
                 "Apparently no space reserved for IGEOLO info in NITF file.\n"
                 "NITFWriteIGEOGLO() fails." );
        return FALSE;
    }

    NITFEncodeDMSLoc( szIGEOLO +  0, dfULY, "Lat" );
    NITFEncodeDMSLoc( szIGEOLO +  7, dfULX, "Long" );
    NITFEncodeDMSLoc( szIGEOLO + 15, dfURY, "Lat" );
    NITFEncodeDMSLoc( szIGEOLO + 22, dfURX, "Long" );
    NITFEncodeDMSLoc( szIGEOLO + 30, dfLRY, "Lat" );
    NITFEncodeDMSLoc( szIGEOLO + 37, dfLRX, "Long" );
    NITFEncodeDMSLoc( szIGEOLO + 45, dfLLY, "Lat" );
    NITFEncodeDMSLoc( szIGEOLO + 52, dfLLX, "Long" );

    VSIFSeek( psImage->psFile->fp, 
              psImage->psFile->pasSegmentInfo[psImage->iSegment].nSegmentHeaderStart
              + 372, SEEK_SET );
    VSIFWrite( szIGEOLO, 1, 60, psImage->psFile->fp );

    return TRUE;
}

/************************************************************************/
/*                            NITFWriteLUT()                            */
/************************************************************************/

int NITFWriteLUT( NITFImage *psImage, int nBand, int nColors, 
                  unsigned char *pabyLUT )

{
    NITFBandInfo *psBandInfo;
    int           bSuccess = TRUE;

    if( nBand < 1 || nBand > psImage->nBands )
        return FALSE;

    psBandInfo = psImage->pasBandInfo + (nBand-1);

    if( nColors > psBandInfo->nSignificantLUTEntries )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "Unable to write all %d LUT entries, only able to write %d.",
                  nColors, psBandInfo->nSignificantLUTEntries );
        nColors = psBandInfo->nSignificantLUTEntries;
        bSuccess = FALSE;
    }

    VSIFSeek( psImage->psFile->fp, psBandInfo->nLUTLocation, SEEK_SET );
    VSIFWrite( pabyLUT, 1, nColors, psImage->psFile->fp );
    VSIFSeek( psImage->psFile->fp, 
              psBandInfo->nLUTLocation + psBandInfo->nSignificantLUTEntries, 
              SEEK_SET );
    VSIFWrite( pabyLUT+256, 1, nColors, psImage->psFile->fp );
    VSIFSeek( psImage->psFile->fp, 
              psBandInfo->nLUTLocation + 2*psBandInfo->nSignificantLUTEntries, 
              SEEK_SET );
    VSIFWrite( pabyLUT+512, 1, nColors, psImage->psFile->fp );

    return bSuccess;
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

/************************************************************************/
/*                          NITFIsAllDigits()                           */
/*                                                                      */
/*      This is used in verifying that the IGEOLO value is actually     */
/*      present for ICORDS='N'.   We also allow for spaces.             */
/************************************************************************/

static int NITFIsAllDigits( const char *pachBuffer, int nCharCount )

{
    int i;

    for( i = 0; i < nCharCount; i++ )
    {
        if( pachBuffer[i] != ' '
            && (pachBuffer[i] < '1' || pachBuffer[i] > '0' ) )
            return FALSE;
    }
    return TRUE;
}

/************************************************************************/
/*                           NITFSwapWords()                            */
/************************************************************************/

static void NITFSwapWords( void *pData, int nWordSize, int nWordCount,
                           int nWordSkip )

{
    int         i;
    GByte       *pabyData = (GByte *) pData;

    switch( nWordSize )
    {
      case 1:
        break;

      case 2:
        CPLAssert( nWordSize >= 2 );
        for( i = 0; i < nWordCount; i++ )
        {
            GByte       byTemp;

            byTemp = pabyData[0];
            pabyData[0] = pabyData[1];
            pabyData[1] = byTemp;

            pabyData += nWordSkip;
        }
        break;
        
      case 4:
        CPLAssert( nWordSize >= 4 );
        for( i = 0; i < nWordCount; i++ )
        {
            GByte       byTemp;

            byTemp = pabyData[0];
            pabyData[0] = pabyData[3];
            pabyData[3] = byTemp;

            byTemp = pabyData[1];
            pabyData[1] = pabyData[2];
            pabyData[2] = byTemp;

            pabyData += nWordSkip;
        }
        break;

      case 8:
        CPLAssert( nWordSize >= 8 );
        for( i = 0; i < nWordCount; i++ )
        {
            GByte       byTemp;

            byTemp = pabyData[0];
            pabyData[0] = pabyData[7];
            pabyData[7] = byTemp;

            byTemp = pabyData[1];
            pabyData[1] = pabyData[6];
            pabyData[6] = byTemp;

            byTemp = pabyData[2];
            pabyData[2] = pabyData[5];
            pabyData[5] = byTemp;

            byTemp = pabyData[3];
            pabyData[3] = pabyData[4];
            pabyData[4] = byTemp;

            pabyData += nWordSkip;
        }
        break;

      default:
        CPLAssert( FALSE );
    }
}

