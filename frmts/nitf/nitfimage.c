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
 * Revision 1.21  2004/02/09 05:18:07  warmerda
 * fixed up north/south MGRS support
 *
 * Revision 1.20  2004/02/09 05:03:42  warmerda
 * added ICORDS=U (MGRS) support
 *
 * Revision 1.19  2003/09/11 19:51:55  warmerda
 * avoid type casting warnings
 *
 * Revision 1.18  2003/08/21 19:27:07  warmerda
 * fixed byte swaping issue
 *
 * Revision 1.17  2003/06/06 16:52:32  warmerda
 * changes based on better understanding of conditional FSDEVT field
 *
 * Revision 1.16  2003/06/06 15:07:53  warmerda
 * fixed security area sizing for NITF 2.0 images, its like NITF 1.1.
 *
 * Revision 1.15  2003/06/06 13:48:13  warmerda
 * Corrected problem with pixel interleaved (IMODE=P) images that include
 * block maps like the v_3301f.ntf "stdset" file.
 *
 * Revision 1.14  2003/05/29 19:50:57  warmerda
 * added TRE in image, and RPC00B support
 *
 * Revision 1.13  2003/05/05 17:57:54  warmerda
 * added blocked writing support
 *
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
#include "mgrs.h"
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
/*      Does this header have the FSDEVT field?                         */
/* -------------------------------------------------------------------- */
    nOffset = 333;

    if( EQUALN(psFile->szVersion,"NITF01.",7) 
        || EQUALN(pachHeader+284,"999998",6) )
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
            else if( psImage->chICORDS == 'U' )
            {
                int err;
                long nZone;
                char chHemisphere;
                NITFGetField( szTemp, pszCoordPair, 0, 15 );
                
                CPLDebug( "NITF", "IGEOLO = %15.15s", pszCoordPair );
                err = Convert_MGRS_To_UTM( szTemp, &nZone, &chHemisphere,
                                           pdfXY+0, pdfXY+1 );

                if( chHemisphere == 'S' )
                    nZone = -1 * nZone;

                if( psImage->nZone != 0 && psImage->nZone != -100 )
                {
                    if( nZone != psImage->nZone )
                    {
                        CPLError( CE_Warning, CPLE_AppDefined,
                                  "Some IGEOGLO points are in different UTM\n"
                                  "zones, but this configuration isn't currently\n"
                                  "supported by GDAL, ignoring IGEOLO." );
                        psImage->nZone = -100;
                    }
                }
                else if( psImage->nZone == 0 )
                {
                    psImage->nZone = nZone;
                }
            }
        }

        if( psImage->nZone == -100 )
            psImage->nZone = 0;

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
/*      Skip some unused fields.                                        */
/* -------------------------------------------------------------------- */
    else
    {
        int nUserTREBytes;

        nOffset += 3;                   /* IDLVL */
        nOffset += 3;                   /* IALVL */
        nOffset += 10;                  /* ILOC */
        nOffset += 4;                   /* IMAG */
        
/* -------------------------------------------------------------------- */
/*      Are there user TRE bytes to skip?                               */
/* -------------------------------------------------------------------- */
        nUserTREBytes = atoi(NITFGetField( szTemp, pachHeader, nOffset, 5 ));
        nOffset += 5;

        if( nUserTREBytes > 0 )
            nOffset += nUserTREBytes;

/* -------------------------------------------------------------------- */
/*      Are there managed TRE bytes to recognise?                       */
/* -------------------------------------------------------------------- */
        psImage->nTREBytes = atoi(NITFGetField(szTemp,pachHeader,nOffset,5));
        nOffset += 5;

        if( psImage->nTREBytes != 0 )
        {
            nOffset += 3;
            psImage->pachTRE = pachHeader + nOffset;
            psImage->nTREBytes -= 3;

            nOffset += psImage->nTREBytes;
        }
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
        int nBlockCount;

        nBlockCount = psImage->nBlocksPerRow * psImage->nBlocksPerColumn
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

        if( nBMRLNTH == 4 && psImage->chIMODE == 'P' )
        {
            int nStoredBlocks = psImage->nBlocksPerRow 
                * psImage->nBlocksPerColumn; 
            int iBand;

            VSIFRead( psImage->panBlockStart, 4, nStoredBlocks, psFile->fp );

            for( i = 0; i < nStoredBlocks; i++ )
            {
                CPL_MSBPTR32( psImage->panBlockStart + i );
                if( psImage->panBlockStart[i] != 0xffffffff )
                {
                    psImage->panBlockStart[i] 
                        += psSegInfo->nSegmentStart + nIMDATOFF;

                    for( iBand = 1; iBand < psImage->nBands; iBand++ )
                    {
                        psImage->panBlockStart[i + iBand * nStoredBlocks] = 
                            psImage->panBlockStart[i] 
                            + iBand * psImage->nBandOffset;
                    }
                }
                else
                {
                    for( iBand = 1; iBand < psImage->nBands; iBand++ )
                        psImage->panBlockStart[i + iBand * nStoredBlocks] = 
                            0xffffffff;
                }
            }
        }
        else if( nBMRLNTH == 4 )
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

    if( psImage->panBlockStart[iFullBlock] == 0xffffffff )
        return BLKREAD_NULL;

/* -------------------------------------------------------------------- */
/*      Can we do a direct read into our buffer?                        */
/* -------------------------------------------------------------------- */
    if( psImage->nWordSize == psImage->nPixelOffset
        && psImage->nWordSize * psImage->nBlockWidth == psImage->nLineOffset 
        && psImage->szIC[0] != 'C' && psImage->szIC[0] != 'M'
        && psImage->chIMODE != 'P' )
    {
        if( VSIFSeek( psImage->psFile->fp, 
                      psImage->panBlockStart[iFullBlock], 
                      SEEK_SET ) != 0 
            || (int) VSIFRead( pData, 1, nWrkBufSize,
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
            || (int) VSIFRead( pabyWrkBuf, 1, nWrkBufSize,
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

/* -------------------------------------------------------------------- */
/*      Report unsupported compression scheme(s).                       */
/* -------------------------------------------------------------------- */
    else if( atoi(psImage->szIC + 1) > 0 )
    {
        CPLError( CE_Failure, CPLE_NotSupported, 
                  "Unsupported imagery compression format %s in NITF library.",
                  psImage->szIC );
        return BLKREAD_FAIL;
    }

    return BLKREAD_FAIL;
}

/************************************************************************/
/*                        NITFWriteImageBlock()                         */
/************************************************************************/

int NITFWriteImageBlock( NITFImage *psImage, int nBlockX, int nBlockY, 
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
#ifdef CPL_LSB
        NITFSwapWords( pData, psImage->nWordSize, 
                       psImage->nBlockWidth * psImage->nBlockHeight, 
                       psImage->nWordSize );
#endif

        if( VSIFSeek( psImage->psFile->fp, psImage->panBlockStart[iFullBlock], 
                      SEEK_SET ) != 0 
            || (int) VSIFWrite( pData, 1, nWrkBufSize,
                                psImage->psFile->fp ) != nWrkBufSize )
        {
            CPLError( CE_Failure, CPLE_FileIO, 
                      "Unable to write %d byte block from %d.", 
                      nWrkBufSize, psImage->panBlockStart[iFullBlock] );
            return BLKREAD_FAIL;
        }
        else
        {
#ifdef CPL_LSB
            /* restore byte order to original */
            NITFSwapWords( pData, psImage->nWordSize, 
                           psImage->nBlockWidth * psImage->nBlockHeight, 
                           psImage->nWordSize );
#endif

            return BLKREAD_OK;
        }
    }

/* -------------------------------------------------------------------- */
/*      Other forms not supported at this time.                         */
/* -------------------------------------------------------------------- */
    CPLError( CE_Failure, CPLE_NotSupported, 
              "Mapped, interleaved and compressed NITF forms not supported\n"
              "for writing at this time." );

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
#ifdef CPL_LSB
        NITFSwapWords( (void *) pData, psImage->nWordSize, 
                       psImage->nCols, psImage->nWordSize );
#endif

        VSIFWrite( pData, 1, nLineSize, psImage->psFile->fp );

#ifdef CPL_LSB
        NITFSwapWords( (void *) pData, psImage->nWordSize, 
                       psImage->nCols, psImage->nWordSize );
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
/*      Copy the desired data into the interleaved buffer.              */
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
#ifdef CPL_LSB
        NITFSwapWords( pabyDst + iPixel * psImage->nWordSize, 
                       psImage->nWordSize, 1, psImage->nWordSize );
#endif
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

/************************************************************************/
/*                           NITFReadRPC00B()                           */
/*                                                                      */
/*      Read an RPC00B structure if the TRE is available.               */
/************************************************************************/

int NITFReadRPC00B( NITFImage *psImage, NITFRPC00BInfo *psRPC )

{
    const char *pachTRE;
    char szTemp[100];
    int  i;

    psRPC->SUCCESS = 0;

/* -------------------------------------------------------------------- */
/*      Do we have the TRE?                                             */
/* -------------------------------------------------------------------- */
    pachTRE = NITFFindTRE( psImage->pachTRE, psImage->nTREBytes, 
                           "RPC00B", NULL );

    if( pachTRE == NULL )
        return FALSE;

/* -------------------------------------------------------------------- */
/*      Parse out field values.                                         */
/* -------------------------------------------------------------------- */
    psRPC->SUCCESS = atoi(NITFGetField(szTemp, pachTRE, 0, 1 ));

    psRPC->ERR_BIAS = atof(NITFGetField(szTemp, pachTRE, 1, 7 ));
    psRPC->ERR_RAND = atof(NITFGetField(szTemp, pachTRE, 8, 7 ));

    psRPC->LINE_OFF = atof(NITFGetField(szTemp, pachTRE, 15, 6 ));
    psRPC->SAMP_OFF = atof(NITFGetField(szTemp, pachTRE, 21, 5 ));
    psRPC->LAT_OFF = atof(NITFGetField(szTemp, pachTRE, 26, 8 ));
    psRPC->LONG_OFF = atof(NITFGetField(szTemp, pachTRE, 34, 9 ));
    psRPC->HEIGHT_OFF = atof(NITFGetField(szTemp, pachTRE, 43, 5 ));

    psRPC->LINE_SCALE = atof(NITFGetField(szTemp, pachTRE, 48, 6 ));
    psRPC->SAMP_SCALE = atof(NITFGetField(szTemp, pachTRE, 54, 5 ));
    psRPC->LAT_SCALE = atof(NITFGetField(szTemp, pachTRE, 59, 8 ));
    psRPC->LONG_SCALE = atof(NITFGetField(szTemp, pachTRE, 67, 9 ));
    psRPC->HEIGHT_SCALE = atof(NITFGetField(szTemp, pachTRE, 76, 5 ));

/* -------------------------------------------------------------------- */
/*      Parse out coefficients.                                         */
/* -------------------------------------------------------------------- */
    for( i = 0; i < 20; i++ )
    {
        psRPC->LINE_NUM_COEFF[i] = 
            atof(NITFGetField(szTemp, pachTRE, 81+i*12, 12));
        psRPC->LINE_DEN_COEFF[i] = 
            atof(NITFGetField(szTemp, pachTRE, 321+i*12, 12));
        psRPC->SAMP_NUM_COEFF[i] = 
            atof(NITFGetField(szTemp, pachTRE, 561+i*12, 12));
        psRPC->SAMP_DEN_COEFF[i] = 
            atof(NITFGetField(szTemp, pachTRE, 801+i*12, 12));
    }

    return TRUE;
}

/************************************************************************/
/*                         NITFRPCGeoToImage()                          */
/************************************************************************/

int NITFRPCGeoToImage( NITFRPC00BInfo *psRPC, 
                       double dfLong, double dfLat, double dfHeight, 
                       double *pdfPixel, double *pdfLine )

{
    double dfLineNumerator, dfLineDenominator, 
        dfPixelNumerator, dfPixelDenominator;
    double dfPolyTerm[20];
    int i;

/* -------------------------------------------------------------------- */
/*      Normalize Lat/Long position.                                    */
/* -------------------------------------------------------------------- */
    dfLong = (dfLong - psRPC->LONG_OFF) / psRPC->LONG_SCALE;
    dfLat  = (dfLat - psRPC->LAT_OFF) / psRPC->LAT_SCALE;
    dfHeight = (dfHeight - psRPC->HEIGHT_OFF) / psRPC->HEIGHT_SCALE;

/* -------------------------------------------------------------------- */
/*      Compute the 20 terms.                                           */
/* -------------------------------------------------------------------- */

    dfPolyTerm[0] = 1.0;
    dfPolyTerm[1] = dfLong;
    dfPolyTerm[2] = dfLat;
    dfPolyTerm[3] = dfHeight;
    dfPolyTerm[4] = dfLong * dfLat;
    dfPolyTerm[5] = dfLong * dfHeight;
    dfPolyTerm[6] = dfLat * dfHeight;
    dfPolyTerm[7] = dfLong * dfLong;
    dfPolyTerm[8] = dfLat * dfLat;
    dfPolyTerm[9] = dfHeight * dfHeight;

    dfPolyTerm[10] = dfLong * dfLat * dfHeight;
    dfPolyTerm[11] = dfLong * dfLong * dfLong;
    dfPolyTerm[12] = dfLong * dfLat * dfLat;
    dfPolyTerm[13] = dfLong * dfHeight * dfHeight;
    dfPolyTerm[14] = dfLong * dfLong * dfLat;
    dfPolyTerm[15] = dfLat * dfLat * dfLat;
    dfPolyTerm[16] = dfLat * dfHeight * dfHeight;
    dfPolyTerm[17] = dfLong * dfLong * dfHeight;
    dfPolyTerm[18] = dfLat * dfLat * dfHeight;
    dfPolyTerm[19] = dfHeight * dfHeight * dfHeight;
    

/* -------------------------------------------------------------------- */
/*      Compute numerator and denominator sums.                         */
/* -------------------------------------------------------------------- */
    dfPixelNumerator = 0.0;
    dfPixelDenominator = 0.0;
    dfLineNumerator = 0.0;
    dfLineDenominator = 0.0;

    for( i = 0; i < 20; i++ )
    {
        dfPixelNumerator += psRPC->SAMP_NUM_COEFF[i] * dfPolyTerm[i];
        dfPixelDenominator += psRPC->SAMP_DEN_COEFF[i] * dfPolyTerm[i];
        dfLineNumerator += psRPC->LINE_NUM_COEFF[i] * dfPolyTerm[i];
        dfLineDenominator += psRPC->LINE_DEN_COEFF[i] * dfPolyTerm[i];
    }
        
/* -------------------------------------------------------------------- */
/*      Compute normalized pixel and line values.                       */
/* -------------------------------------------------------------------- */
    *pdfPixel = dfPixelNumerator / dfPixelDenominator;
    *pdfLine = dfLineNumerator / dfLineDenominator;

/* -------------------------------------------------------------------- */
/*      Denormalize.                                                    */
/* -------------------------------------------------------------------- */
    *pdfPixel = *pdfPixel * psRPC->SAMP_SCALE + psRPC->SAMP_OFF;
    *pdfLine  = *pdfLine  * psRPC->LINE_SCALE + psRPC->LINE_OFF;

    return TRUE;
}
