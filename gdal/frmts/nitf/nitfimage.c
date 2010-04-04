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
 ****************************************************************************/

#include "gdal.h"
#include "nitflib.h"
#include "mgrs.h"
#include "cpl_vsi.h"
#include "cpl_conv.h"
#include "cpl_string.h"

CPL_CVSID("$Id$");

static char *NITFTrimWhite( char * );
#ifdef CPL_LSB
static void NITFSwapWords( NITFImage *psImage, void *pData, int nWordCount );
#endif

static void NITFLoadLocationTable( NITFImage *psImage );
static void NITFLoadColormapSubSection( NITFImage *psImage );
static void NITFLoadSubframeMaskTable( NITFImage *psImage );
static int NITFLoadVQTables( NITFImage *psImage );
static int NITFReadGEOLOB( NITFImage *psImage );
static void NITFLoadAttributeSection( NITFImage *psImage );

void NITFGetGCP ( const char* pachCoord, double *pdfXYs, int iCoord );
int NITFReadBLOCKA_GCPs ( NITFImage *psImage );


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
    int        nNICOM;
    const char* pszIID1;
    
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
    if (psSegInfo->nSegmentHeaderSize < 370 + 1)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                    "Image header too small");
        return NULL;
    }

    pachHeader = (char*) VSIMalloc(psSegInfo->nSegmentHeaderSize);
    if (pachHeader == NULL)
    {
        CPLError(CE_Failure, CPLE_OutOfMemory, "Cannot allocate memory for segment header");
        return NULL;
    }

    if( VSIFSeekL( psFile->fp, psSegInfo->nSegmentHeaderStart, 
                  SEEK_SET ) != 0 
        || VSIFReadL( pachHeader, 1, psSegInfo->nSegmentHeaderSize, 
                     psFile->fp ) != psSegInfo->nSegmentHeaderSize )
    {
        CPLError( CE_Failure, CPLE_FileIO, 
                  "Failed to read %u byte image subheader from " CPL_FRMT_GUIB ".",
                  psSegInfo->nSegmentHeaderSize,
                  psSegInfo->nSegmentHeaderStart );
        CPLFree(pachHeader);
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
/*      Collect a variety of information as metadata.                   */
/* -------------------------------------------------------------------- */
#define GetMD( target, hdr, start, length, name )              \
    NITFExtractMetadata( &(target->papszMetadata), hdr,       \
                         start, length,                        \
                         "NITF_" #name );
       
    if( EQUAL(psFile->szVersion,"NITF02.10") 
        || EQUAL(psFile->szVersion,"NSIF01.00") )
    {
        GetMD( psImage, pachHeader,   2,  10, IID1   );
        GetMD( psImage, pachHeader,  12,  14, IDATIM );
        GetMD( psImage, pachHeader,  26,  17, TGTID  );
        GetMD( psImage, pachHeader,  43,  80, IID2   );
        GetMD( psImage, pachHeader, 123,   1, ISCLAS );
        GetMD( psImage, pachHeader, 124,   2, ISCLSY );
        GetMD( psImage, pachHeader, 126,  11, ISCODE );
        GetMD( psImage, pachHeader, 137,   2, ISCTLH );
        GetMD( psImage, pachHeader, 139,  20, ISREL  );
        GetMD( psImage, pachHeader, 159,   2, ISDCTP );
        GetMD( psImage, pachHeader, 161,   8, ISDCDT );
        GetMD( psImage, pachHeader, 169,   4, ISDCXM );
        GetMD( psImage, pachHeader, 173,   1, ISDG   );
        GetMD( psImage, pachHeader, 174,   8, ISDGDT );
        GetMD( psImage, pachHeader, 182,  43, ISCLTX );
        GetMD( psImage, pachHeader, 225,   1, ISCATP );
        GetMD( psImage, pachHeader, 226,  40, ISCAUT );
        GetMD( psImage, pachHeader, 266,   1, ISCRSN );
        GetMD( psImage, pachHeader, 267,   8, ISSRDT );
        GetMD( psImage, pachHeader, 275,  15, ISCTLN );
        /* skip ENCRYPT - 1 character */
        GetMD( psImage, pachHeader, 291,  42, ISORCE );
        /* skip NROWS (8), and NCOLS (8) */
        GetMD( psImage, pachHeader, 349,   3, PVTYPE );
        GetMD( psImage, pachHeader, 352,   8, IREP   );
        GetMD( psImage, pachHeader, 360,   8, ICAT   );
        GetMD( psImage, pachHeader, 368,   2, ABPP   );
        GetMD( psImage, pachHeader, 370,   1, PJUST  );
    }
    else if( EQUAL(psFile->szVersion,"NITF02.00") )
    {
        int nOffset = 0;
        GetMD( psImage, pachHeader,   2,  10, IID1   );
        GetMD( psImage, pachHeader,  12,  14, IDATIM );
        GetMD( psImage, pachHeader,  26,  17, TGTID  );
        GetMD( psImage, pachHeader,  43,  80, ITITLE );
        GetMD( psImage, pachHeader, 123,   1, ISCLAS );
        GetMD( psImage, pachHeader, 124,  40, ISCODE );
        GetMD( psImage, pachHeader, 164,  40, ISCTLH );
        GetMD( psImage, pachHeader, 204,  40, ISREL  );
        GetMD( psImage, pachHeader, 244,  20, ISCAUT );
        GetMD( psImage, pachHeader, 264,  20, ISCTLN );
        GetMD( psImage, pachHeader, 284,   6, ISDWNG );
        
        if( EQUALN(pachHeader+284,"999998",6) )
        {
            if (psSegInfo->nSegmentHeaderSize < 370 + 40 + 1)
                goto header_too_small;
            GetMD( psImage, pachHeader, 290,  40, ISDEVT );
            nOffset += 40;
        }

        /* skip ENCRYPT - 1 character */
        GetMD( psImage, pachHeader, 291+nOffset,  42, ISORCE );
        /* skip NROWS (8), and NCOLS (8) */
        GetMD( psImage, pachHeader, 349+nOffset,   3, PVTYPE );
        GetMD( psImage, pachHeader, 352+nOffset,   8, IREP   );
        GetMD( psImage, pachHeader, 360+nOffset,   8, ICAT   );
        GetMD( psImage, pachHeader, 368+nOffset,   2, ABPP   );
        GetMD( psImage, pachHeader, 370+nOffset,   1, PJUST  );
    }

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
        if ( (int)psSegInfo->nSegmentHeaderSize < nOffset + 35+2)
            goto header_too_small;

        psImage->nRows = atoi(NITFGetField(szTemp,pachHeader,nOffset,8));
        psImage->nCols = atoi(NITFGetField(szTemp,pachHeader,nOffset+8,8));
        
        NITFTrimWhite( NITFGetField( psImage->szPVType, pachHeader, 
                                     nOffset+16, 3) );
        NITFTrimWhite( NITFGetField( psImage->szIREP, pachHeader, 
                                     nOffset+19, 8) );
        NITFTrimWhite( NITFGetField( psImage->szICAT, pachHeader, 
                                     nOffset+27, 8) );
        psImage->nABPP = atoi(NITFGetField(szTemp,pachHeader,nOffset+35,2));
    }

    nOffset += 38;

/* -------------------------------------------------------------------- */
/*      Do we have IGEOLO information?  In NITF 2.0 (and 1.x) 'N' means */
/*      no information, while in 2.1 this is indicated as ' ', and 'N'  */
/*      means UTM (north).  So for 2.0 products we change 'N' to ' '    */
/*      to conform to 2.1 conventions.                                  */
/* -------------------------------------------------------------------- */
    if ( (int)psSegInfo->nSegmentHeaderSize < nOffset + 1)
        goto header_too_small;

    GetMD( psImage, pachHeader, nOffset, 1, ICORDS );

    psImage->chICORDS = pachHeader[nOffset++];
    psImage->bHaveIGEOLO = FALSE;

    if( (EQUALN(psFile->szVersion,"NITF02.0",8)
         || EQUALN(psFile->szVersion,"NITF01.",7))
        && psImage->chICORDS == 'N' )
        psImage->chICORDS = ' ';

/* -------------------------------------------------------------------- */
/*      Read the image bounds.                                          */
/* -------------------------------------------------------------------- */
    if( psImage->chICORDS != ' ' )
    {
        int iCoord;

        psImage->bHaveIGEOLO = TRUE;
        if ( (int)psSegInfo->nSegmentHeaderSize < nOffset + 4 * 15)
            goto header_too_small;

        GetMD( psImage, pachHeader, nOffset, 60, IGEOLO );

        psImage->bIsBoxCenterOfPixel = TRUE;
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
            else if( psImage->chICORDS == 'G' || psImage->chICORDS == 'C' )
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
            else if( psImage->chICORDS == 'D' )
            {  /* 'D' is Decimal Degrees */
                pdfXY[1] = atof(NITFGetField( szTemp, pszCoordPair, 0, 7 ));
                pdfXY[0] = atof(NITFGetField( szTemp, pszCoordPair, 7, 8 ));
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
                                  "Some IGEOLO points are in different UTM\n"
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
        if ( (int)psSegInfo->nSegmentHeaderSize < nOffset + 1 )
            goto header_too_small;

        nNICOM = atoi(NITFGetField( szTemp, pachHeader, nOffset++, 1));
        if ( (int)psSegInfo->nSegmentHeaderSize < nOffset + 80 * nNICOM )
            goto header_too_small;

        psImage->pszComments = (char *) CPLMalloc(nNICOM*80+1);
        NITFGetField( psImage->pszComments, pachHeader,
                      nOffset, 80 * nNICOM );
        nOffset += nNICOM * 80;
    }
    
/* -------------------------------------------------------------------- */
/*      Read more stuff.                                                */
/* -------------------------------------------------------------------- */
    if ( (int)psSegInfo->nSegmentHeaderSize < nOffset + 2 )
        goto header_too_small;

    NITFGetField( psImage->szIC, pachHeader, nOffset, 2 );
    nOffset += 2;

    if( psImage->szIC[0] != 'N' )
    {
        if ( (int)psSegInfo->nSegmentHeaderSize < nOffset + 4 )
            goto header_too_small;

        NITFGetField( psImage->szCOMRAT, pachHeader, nOffset, 4 );
        nOffset += 4;
    }

    /* NBANDS */
    if ( (int)psSegInfo->nSegmentHeaderSize < nOffset + 1 )
        goto header_too_small;
    psImage->nBands = atoi(NITFGetField(szTemp,pachHeader,nOffset,1));
    nOffset++;

    /* XBANDS */
    if( psImage->nBands == 0 )
    {
        if ( (int)psSegInfo->nSegmentHeaderSize < nOffset + 5 )
            goto header_too_small;
        psImage->nBands = atoi(NITFGetField(szTemp,pachHeader,nOffset,5));
        nOffset += 5;
    }

    if (psImage->nBands <= 0)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Invalid band number");
        NITFImageDeaccess(psImage);
        return NULL;
    }

/* -------------------------------------------------------------------- */
/*      Read per-band information.                                      */
/* -------------------------------------------------------------------- */
    psImage->pasBandInfo = (NITFBandInfo *) 
        VSICalloc(sizeof(NITFBandInfo),psImage->nBands);
    if (psImage->pasBandInfo == NULL)
    {
        CPLError(CE_Failure, CPLE_OutOfMemory, "Cannot allocate memory for band info");
        NITFImageDeaccess(psImage);
        return NULL;
    }

    for( iBand = 0; iBand < psImage->nBands; iBand++ )
    {
        NITFBandInfo *psBandInfo = psImage->pasBandInfo + iBand;
        int nLUTS;

        if ( (int)psSegInfo->nSegmentHeaderSize < nOffset + 2 + 6 + 4 + 1 + 5)
            goto header_too_small;

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

        if (psBandInfo->nSignificantLUTEntries < 0 ||
            psBandInfo->nSignificantLUTEntries > 256)
        {
            CPLError( CE_Warning, CPLE_AppDefined,
                      "LUT for band %d is corrupted : nSignificantLUTEntries=%d. Truncating to 256",
                      iBand + 1, psBandInfo->nSignificantLUTEntries);
            psBandInfo->nSignificantLUTEntries = 256;
        }

        psBandInfo->nLUTLocation = nOffset +
                                   (int)psSegInfo->nSegmentHeaderStart;

        psBandInfo->pabyLUT = (unsigned char *) CPLCalloc(768,1);
        if ( (int)psSegInfo->nSegmentHeaderSize <
             nOffset + nLUTS * psBandInfo->nSignificantLUTEntries )
            goto header_too_small;

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
/*      Some files (ie NSIF datasets) have truncated image              */
/*      headers.  This has been observed with jpeg compressed           */
/*      files.  In this case guess reasonable values for these          */
/*      fields.                                                         */
/* -------------------------------------------------------------------- */
    if( nOffset + 40 > (int)psSegInfo->nSegmentHeaderSize )
    {
        psImage->chIMODE = 'B';
        psImage->nBlocksPerRow = 1;
        psImage->nBlocksPerColumn = 1;
        psImage->nBlockWidth = psImage->nCols;
        psImage->nBlockHeight = psImage->nRows;
        psImage->nBitsPerSample = psImage->nABPP;
        psImage->nIDLVL = 0;
        psImage->nIALVL = 0;
        psImage->nILOCRow = 0;
        psImage->nILOCColumn = 0;
        psImage->szIMAG[0] = '\0';

        nOffset += 40;
    }

/* -------------------------------------------------------------------- */
/*      Read more header fields.                                        */
/* -------------------------------------------------------------------- */
    else
    {
        psImage->chIMODE = pachHeader[nOffset + 1];
        
        psImage->nBlocksPerRow = 
            atoi(NITFGetField(szTemp, pachHeader, nOffset+2, 4));
        psImage->nBlocksPerColumn = 
            atoi(NITFGetField(szTemp, pachHeader, nOffset+6, 4));
        psImage->nBlockWidth = 
            atoi(NITFGetField(szTemp, pachHeader, nOffset+10, 4));
        psImage->nBlockHeight = 
            atoi(NITFGetField(szTemp, pachHeader, nOffset+14, 4));
            
        /* See MIL-STD-2500-C, paragraph 5.4.2.2-d (#3263) */
        if (EQUAL(psImage->szIC, "NC") &&
            (psImage->nCols > 8192 || psImage->nRows > 8192))
        {
            if (psImage->nBlocksPerRow == 1 &&
                psImage->nBlockWidth == 0)
            {
                psImage->nBlockWidth = psImage->nCols;
            }

            if (psImage->nBlocksPerColumn == 1 &&
                psImage->nBlockHeight == 0)
            {
                psImage->nBlockHeight = psImage->nRows;
            }
        }

        psImage->nBitsPerSample = 
            atoi(NITFGetField(szTemp, pachHeader, nOffset+18, 2));
        
        if( psImage->nABPP == 0 )
            psImage->nABPP = psImage->nBitsPerSample;

        nOffset += 20;

        /* capture image inset information */

        psImage->nIDLVL = atoi(NITFGetField(szTemp,pachHeader, nOffset+0, 3));
        psImage->nIALVL = atoi(NITFGetField(szTemp,pachHeader, nOffset+3, 3));
        psImage->nILOCRow = atoi(NITFGetField(szTemp,pachHeader,nOffset+6,5));
        psImage->nILOCColumn = 
            atoi(NITFGetField(szTemp,pachHeader, nOffset+11,5));

        memcpy( psImage->szIMAG, pachHeader+nOffset+16, 4 );
        psImage->szIMAG[4] = '\0';
        
        nOffset += 3;                   /* IDLVL */
        nOffset += 3;                   /* IALVL */
        nOffset += 10;                  /* ILOC */
        nOffset += 4;                   /* IMAG */
    }

    if (psImage->nBitsPerSample <= 0 ||
        psImage->nBlocksPerRow <= 0 ||
        psImage->nBlocksPerColumn <= 0 ||
        psImage->nBlockWidth <= 0 ||
        psImage->nBlockHeight <= 0 ||
        psImage->nBlocksPerRow > INT_MAX / psImage->nBlockWidth ||
        psImage->nBlocksPerColumn > INT_MAX / psImage->nBlockHeight ||
        psImage->nCols > psImage->nBlocksPerRow * psImage->nBlockWidth ||
        psImage->nRows > psImage->nBlocksPerColumn * psImage->nBlockHeight ||
        psImage->nBlocksPerRow > INT_MAX / psImage->nBlocksPerColumn ||
        psImage->nBlocksPerRow * psImage->nBlocksPerColumn > INT_MAX / psImage->nBands)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Invalid values for block dimension/number");
        NITFImageDeaccess(psImage);
        return NULL;
    }

/* -------------------------------------------------------------------- */
/*      Override nCols and nRows for NITF 1.1 (not sure why!)           */
/* -------------------------------------------------------------------- */
    if( EQUALN(psFile->szVersion,"NITF01.",7) )
    {
        psImage->nCols = psImage->nBlocksPerRow * psImage->nBlockWidth;
        psImage->nRows = psImage->nBlocksPerColumn * psImage->nBlockHeight;
    }

/* -------------------------------------------------------------------- */
/*      Read TREs if we have them.                                      */
/* -------------------------------------------------------------------- */
    else if( nOffset+10 <= (int)psSegInfo->nSegmentHeaderSize )
    {
        int nUserTREBytes, nExtendedTREBytes;
        
/* -------------------------------------------------------------------- */
/*      Are there user TRE bytes to skip?                               */
/* -------------------------------------------------------------------- */
        nUserTREBytes = atoi(NITFGetField( szTemp, pachHeader, nOffset, 5 ));
        nOffset += 5;

        if( nUserTREBytes > 3 )
        {
            if( (int)psSegInfo->nSegmentHeaderSize < nOffset + nUserTREBytes )
                goto header_too_small;

            psImage->nTREBytes = nUserTREBytes - 3;
            psImage->pachTRE = (char *) CPLMalloc(psImage->nTREBytes);
            memcpy( psImage->pachTRE, pachHeader + nOffset + 3,
                    psImage->nTREBytes );

            nOffset += nUserTREBytes;
        }
        else
        {
            psImage->nTREBytes = 0;
            psImage->pachTRE = NULL;
        }

/* -------------------------------------------------------------------- */
/*      Are there managed TRE bytes to recognise?                       */
/* -------------------------------------------------------------------- */
        if ( (int)psSegInfo->nSegmentHeaderSize < nOffset + 5 )
            goto header_too_small;
        nExtendedTREBytes = atoi(NITFGetField(szTemp,pachHeader,nOffset,5));
        nOffset += 5;

        if( nExtendedTREBytes > 3 )
        {
            if( (int)psSegInfo->nSegmentHeaderSize < 
                            nOffset + nExtendedTREBytes )
                goto header_too_small;

            psImage->pachTRE = (char *) 
                CPLRealloc( psImage->pachTRE, 
                            psImage->nTREBytes + nExtendedTREBytes - 3 );
            memcpy( psImage->pachTRE + psImage->nTREBytes, 
                    pachHeader + nOffset + 3, 
                    nExtendedTREBytes - 3 );

            psImage->nTREBytes += (nExtendedTREBytes - 3);
            nOffset += nExtendedTREBytes;
        }
    }

/* -------------------------------------------------------------------- */
/*      Is there a location table to load?                              */
/* -------------------------------------------------------------------- */
    NITFLoadLocationTable( psImage );
    
    /* Fix bug #1744 */
    if (psImage->nBands == 1)
        NITFLoadColormapSubSection ( psImage );

/* -------------------------------------------------------------------- */
/*      Setup some image access values.  Some of these may not apply    */
/*      for compressed images, or band interleaved by block images.     */
/* -------------------------------------------------------------------- */
    psImage->nWordSize = psImage->nBitsPerSample / 8;
    if( psImage->chIMODE == 'S' )
    {
        psImage->nPixelOffset = psImage->nWordSize;
        psImage->nLineOffset = 
            ((GIntBig) psImage->nBlockWidth * psImage->nBitsPerSample) / 8;
        psImage->nBlockOffset = psImage->nLineOffset * psImage->nBlockHeight;
        psImage->nBandOffset = psImage->nBlockOffset * psImage->nBlocksPerRow 
            * psImage->nBlocksPerColumn;
    }
    else if( psImage->chIMODE == 'P' )
    {
        psImage->nPixelOffset = psImage->nWordSize * psImage->nBands;
        psImage->nLineOffset = 
            ((GIntBig) psImage->nBlockWidth * psImage->nBitsPerSample * psImage->nBands) / 8;
        psImage->nBandOffset = psImage->nWordSize;
        psImage->nBlockOffset = psImage->nLineOffset * psImage->nBlockHeight;
    }
    else if( psImage->chIMODE == 'R' )
    {
        psImage->nPixelOffset = psImage->nWordSize;
        psImage->nBandOffset = 
            ((GIntBig) psImage->nBlockWidth * psImage->nBitsPerSample) / 8;
        psImage->nLineOffset = psImage->nBandOffset * psImage->nBands;
        psImage->nBlockOffset = psImage->nLineOffset * psImage->nBlockHeight;
    }
    else if( psImage->chIMODE == 'B' )
    {
        psImage->nPixelOffset = psImage->nWordSize;
        psImage->nLineOffset = 
            ((GIntBig) psImage->nBlockWidth * psImage->nBitsPerSample) / 8;
        psImage->nBandOffset = psImage->nBlockHeight * psImage->nLineOffset;
        psImage->nBlockOffset = psImage->nBandOffset * psImage->nBands;
    }
    else
    {
        psImage->nPixelOffset = psImage->nWordSize;
        psImage->nLineOffset = 
            ((GIntBig) psImage->nBlockWidth * psImage->nBitsPerSample) / 8;
        psImage->nBandOffset = psImage->nBlockHeight * psImage->nLineOffset;
        psImage->nBlockOffset = psImage->nBandOffset * psImage->nBands;
    }

/* -------------------------------------------------------------------- */
/*      Setup block map.                                                */
/* -------------------------------------------------------------------- */

    /* Int overflow already checked above */
    psImage->panBlockStart = (GUIntBig *) 
        VSICalloc( psImage->nBlocksPerRow * psImage->nBlocksPerColumn 
                   * psImage->nBands, sizeof(GUIntBig) );
    if (psImage->panBlockStart == NULL)
    {
        CPLError(CE_Failure, CPLE_OutOfMemory, "Cannot allocate block map");
        NITFImageDeaccess(psImage);
        return NULL;
    }

/* -------------------------------------------------------------------- */
/*      Offsets to VQ compressed tiles are based on a fixed block       */
/*      size, and are offset from the spatial data location kept in     */
/*      the location table ... which is generally not the beginning     */
/*      of the image data segment.                                      */
/* -------------------------------------------------------------------- */
    if( EQUAL(psImage->szIC,"C4") )
    {
        GUIntBig  nLocBase = psSegInfo->nSegmentStart;

        for( i = 0; i < psImage->nLocCount; i++ )
        {
            if( psImage->pasLocations[i].nLocId == LID_SpatialDataSubsection )
                nLocBase = psImage->pasLocations[i].nLocOffset;
        }

        if( nLocBase == psSegInfo->nSegmentStart )
            CPLError( CE_Warning, CPLE_AppDefined, 
                      "Failed to find spatial data location, guessing." );

        for( i=0; i < psImage->nBlocksPerRow * psImage->nBlocksPerColumn; i++ )
            psImage->panBlockStart[i] = nLocBase + 6144 * i;
        
        /* Fix bug #913 */
        NITFLoadSubframeMaskTable ( psImage );
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

        VSIFSeekL( psFile->fp, psSegInfo->nSegmentStart, SEEK_SET );
        VSIFReadL( &nIMDATOFF, 1, 4, psFile->fp );
        VSIFReadL( &nBMRLNTH, 1, 2, psFile->fp );
        VSIFReadL( &nTMRLNTH, 1, 2, psFile->fp );
        VSIFReadL( &nTPXCDLNTH, 1, 2, psFile->fp );

        CPL_MSBPTR32( &nIMDATOFF );
        CPL_MSBPTR16( &nBMRLNTH );
        CPL_MSBPTR16( &nTMRLNTH );
        CPL_MSBPTR16( &nTPXCDLNTH );

        if( nTPXCDLNTH == 8 )
        {
            GByte byNodata;

            psImage->bNoDataSet = TRUE;
            VSIFReadL( &byNodata, 1, 1, psFile->fp );
            psImage->nNoDataValue = byNodata;
        }
        else
            VSIFSeekL( psFile->fp, (nTPXCDLNTH+7)/8, SEEK_CUR );

        if( nBMRLNTH == 4 && psImage->chIMODE == 'P' )
        {
            int nStoredBlocks = psImage->nBlocksPerRow 
                * psImage->nBlocksPerColumn; 
            int iBand;

            for( i = 0; i < nStoredBlocks; i++ )
            {
                GUInt32 nOffset;
                VSIFReadL( &nOffset, 4, 1, psFile->fp );
                CPL_MSBPTR32( &nOffset );
                psImage->panBlockStart[i] = nOffset;
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
            int isM4 = EQUAL(psImage->szIC,"M4");
            for( i=0; i < nBlockCount; i++ )
            {
                GUInt32 nOffset;
                VSIFReadL( &nOffset, 4, 1, psFile->fp );
                CPL_MSBPTR32( &nOffset );
                psImage->panBlockStart[i] = nOffset;
                if( psImage->panBlockStart[i] != 0xffffffff )
                {
                    if (isM4 && (psImage->panBlockStart[i] % 6144) != 0)
                    {
                        break;
                    }
                    psImage->panBlockStart[i] 
                        += psSegInfo->nSegmentStart + nIMDATOFF;
                }
            }
            /* This is a fix for a problem with rpf/cjga/cjgaz01/0105f033.ja1 and */
            /* rpf/cjga/cjgaz03/0034t0b3.ja3 CADRG products (bug 1754). */
            /* These products have the strange particularity that their block start table begins */
            /* one byte after its theoretical beginning, for an unknown reason */
            /* We detect this situation when the block start offset is not a multiple of 6144 */
            /* Hopefully there's something in the NITF/CADRG standard that can account for it,  */
            /* but I've not found it */
            if (isM4 && i != nBlockCount)
            {
                CPLError( CE_Warning, CPLE_AppDefined,
                          "Block start for block %d is wrong. Retrying with one extra byte shift...", i);
                VSIFSeekL( psFile->fp, psSegInfo->nSegmentStart +
                                       4 + /* nIMDATOFF */
                                       2 + /* nBMRLNTH */
                                       2 + /* nTMRLNTH */
                                       2 + /* nTPXCDLNTH */
                                       (nTPXCDLNTH+7)/8 +
                                       1, /* MAGIC here ! One byte shift... */
                            SEEK_SET );

                for( i=0; i < nBlockCount; i++ )
                {
                    GUInt32 nOffset;
                    VSIFReadL( &nOffset, 4, 1, psFile->fp );
                    CPL_MSBPTR32( &nOffset );
                    psImage->panBlockStart[i] = nOffset;
                    if( psImage->panBlockStart[i] != 0xffffffff )
                    {
                        if ((psImage->panBlockStart[i] % 6144) != 0)
                        {
                            CPLError( CE_Warning, CPLE_AppDefined, 
                                      "Block start for block %d is still wrong. Display will be wrong.", i );
                            break;
                        }
                        psImage->panBlockStart[i] 
                            += psSegInfo->nSegmentStart + nIMDATOFF;
                    }
                }
            }
        }
        else
        {
            if( EQUAL(psImage->szIC,"M4") )
            {
                for( i=0; i < nBlockCount; i++ )
                        psImage->panBlockStart[i] = 6144 * i
                            + psSegInfo->nSegmentStart + nIMDATOFF;
            }
            else if( EQUAL(psImage->szIC,"NM") )
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
                                psSegInfo->nSegmentStart + nIMDATOFF
                                + ((iBlockX + iBlockY * psImage->nBlocksPerRow) 
                                * psImage->nBlockOffset)
                                + (iBand * psImage->nBandOffset );
                        }
                    }
                }
            }
            else
            {
                CPLError( CE_Warning, CPLE_AppDefined, 
                          "Unsupported IC value '%s', image access will likely fail.",
                          psImage->szIC );
            }
        }
    }

/* -------------------------------------------------------------------- */
/*      Bug #1751: Add a transparent color if there are none. Absent    */
/*      subblocks will be then transparent.                             */
/* -------------------------------------------------------------------- */
    if( !psImage->bNoDataSet
        && psImage->nBands == 1 
        && psImage->nBitsPerSample == 8 )
    {
        NITFBandInfo *psBandInfo = psImage->pasBandInfo;
        if (psBandInfo->nSignificantLUTEntries < 256-1
            && psBandInfo->pabyLUT != NULL )
        {
            psBandInfo->pabyLUT[0+psBandInfo->nSignificantLUTEntries] = 0;
            psBandInfo->pabyLUT[256+psBandInfo->nSignificantLUTEntries] = 0;
            psBandInfo->pabyLUT[512+psBandInfo->nSignificantLUTEntries] = 0;
            psImage->bNoDataSet = TRUE;
            psImage->nNoDataValue = psBandInfo->nSignificantLUTEntries;
        }
    }

/* -------------------------------------------------------------------- */
/*  We override the coordinates found in IGEOLO in case a BLOCKA is     */
/*  present. According to the BLOCKA specification, it repeats earth    */
/*  coordinates image corner locations described by IGEOLO in the NITF  */
/*  image subheader, but provide higher precision.                      */
/* -------------------------------------------------------------------- */

    NITFReadBLOCKA_GCPs( psImage );

/* -------------------------------------------------------------------- */
/*      We override the coordinates found in IGEOLO in case a GEOLOB is */
/*      present.  It provides higher precision lat/long values.         */
/* -------------------------------------------------------------------- */
    NITFReadGEOLOB( psImage );

/* -------------------------------------------------------------------- */
/*      If we have an RPF CoverageSectionSubheader, read the more       */
/*      precise bounds from it.                                         */
/* -------------------------------------------------------------------- */
    for( i = 0; i < psImage->nLocCount; i++ )
    {
        if( psImage->pasLocations[i].nLocId == LID_CoverageSectionSubheader )
        {
            double adfTarget[8];

            VSIFSeekL( psFile->fp, psImage->pasLocations[i].nLocOffset,
                      SEEK_SET );
            VSIFReadL( adfTarget, 8, 8, psFile->fp );
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

            psImage->bIsBoxCenterOfPixel = FALSE; // edge of pixel

            CPLDebug( "NITF", "Got spatial info from CoverageSection" );
            break;
        }
    }

    /* Bug #1750, #2135 and #3383 */
    /* Fix CADRG products like cjnc/cjncz01/000k1023.jn1 (and similar) from NIMA GNCJNCN CDROM: */
    /* this product is crossing meridian 180deg and the upper and lower right longitudes are negative  */
    /* while the upper and lower left longitudes are positive which causes problems in OpenEV, etc... */
    /* So we are adjusting the upper and lower right longitudes by setting them above +180 */
    /* Make this test only CADRG specific are there are other NITF profiles where non north-up imagery */
    /* is valid */
    pszIID1 = CSLFetchNameValue(psImage->papszMetadata, "NITF_IID1");
    if( (psImage->chICORDS == 'G' || psImage->chICORDS == 'D') &&
         pszIID1 != NULL && EQUAL(pszIID1, "CADRG") &&
        (psImage->dfULX > psImage->dfURX && psImage->dfLLX > psImage->dfLRX &&
         psImage->dfULY > psImage->dfLLY && psImage->dfURY > psImage->dfLRY) )
    {
        psImage->dfURX += 360;
        psImage->dfLRX += 360;
    }

/* -------------------------------------------------------------------- */
/*      Load RPF attribute metadata if we have it.                      */
/* -------------------------------------------------------------------- */
    NITFLoadAttributeSection( psImage );

/* -------------------------------------------------------------------- */
/*      Are the VQ tables to load up?                                   */
/* -------------------------------------------------------------------- */
    NITFLoadVQTables( psImage );

    return psImage;


header_too_small:

    CPLError(CE_Failure, CPLE_AppDefined, "Image header too small");
    NITFImageDeaccess(psImage);
    return NULL;
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

    if ( psImage->pasBandInfo)
    {
        for( iBand = 0; iBand < psImage->nBands; iBand++ )
            CPLFree( psImage->pasBandInfo[iBand].pabyLUT );
    }
    CPLFree( psImage->pasBandInfo );
    CPLFree( psImage->panBlockStart );
    CPLFree( psImage->pszComments );
    CPLFree( psImage->pachHeader );
    CPLFree( psImage->pachTRE );
    CSLDestroy( psImage->papszMetadata );

    CPLFree( psImage->pasLocations );
    for( iBand = 0; iBand < 4; iBand++ )
        CPLFree( psImage->apanVQLUT[iBand] );

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
                
                memcpy( pabyTarget, psImage->apanVQLUT[t] + val1, 4 );
                memcpy( pabyTarget+4, psImage->apanVQLUT[t] + val2, 4);
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

/* -------------------------------------------------------------------- */
/*      Special exit conditions.                                        */
/* -------------------------------------------------------------------- */
    if( nBand == 0 )
        return BLKREAD_FAIL;

    if( psImage->panBlockStart[iFullBlock] == 0xffffffff )
        return BLKREAD_NULL;

/* -------------------------------------------------------------------- */
/*      Special case for 1 bit data.  NITFRasterBand::IReadBlock()      */
/*      already knows how to promote to byte.                           */
/* -------------------------------------------------------------------- */
    if ((EQUAL(psImage->szIC, "NC") || EQUAL(psImage->szIC, "NM")) && psImage->nBitsPerSample == 1)
    {
        if (nBlockX != 0 || nBlockY != 0)
        {
            CPLError( CE_Failure, CPLE_AppDefined, 
                      "assert nBlockX == 0 && nBlockY == 0 failed\n");
            return BLKREAD_FAIL;
        }
        VSIFSeekL( psImage->psFile->fp,
                   psImage->panBlockStart[0] + 
                    (psImage->nBlockWidth * psImage->nBlockHeight + 7) / 8 * (nBand-1),
                   SEEK_SET );
        VSIFReadL( pData, 1, (psImage->nBlockWidth * psImage->nBlockHeight + 7) / 8, psImage->psFile->fp );
        return BLKREAD_OK;
    }

/* -------------------------------------------------------------------- */
/*      Figure out how big the working buffer will need to be.          */
/* -------------------------------------------------------------------- */
    if( psImage->nBitsPerSample != psImage->nWordSize * 8 )
        nWrkBufSize = (int)psImage->nLineOffset * (psImage->nBlockHeight-1)
            + (psImage->nBitsPerSample * (psImage->nBlockWidth) + 7) / 8;
    else
        nWrkBufSize = (int)psImage->nLineOffset * (psImage->nBlockHeight-1)
            + (int)psImage->nPixelOffset * (psImage->nBlockWidth - 1)
            + psImage->nWordSize;

    if (nWrkBufSize == 0)
      nWrkBufSize = (psImage->nBlockWidth*psImage->nBlockHeight*psImage->nBitsPerSample+7)/8;

/* -------------------------------------------------------------------- */
/*      Can we do a direct read into our buffer?                        */
/* -------------------------------------------------------------------- */
    if( psImage->nWordSize == psImage->nPixelOffset
        && (psImage->nBitsPerSample * psImage->nBlockWidth + 7) / 8
           == psImage->nLineOffset 
        && psImage->szIC[0] != 'C' && psImage->szIC[0] != 'M'
        && psImage->chIMODE != 'P' )
    {
        if( VSIFSeekL( psImage->psFile->fp, 
                      psImage->panBlockStart[iFullBlock], 
                      SEEK_SET ) != 0 
            || (int) VSIFReadL( pData, 1, nWrkBufSize,
                               psImage->psFile->fp ) != nWrkBufSize )
        {
            CPLError( CE_Failure, CPLE_FileIO, 
                      "Unable to read %d byte block from " CPL_FRMT_GUIB ".", 
                      nWrkBufSize, psImage->panBlockStart[iFullBlock] );
            return BLKREAD_FAIL;
        }
        else
        {
#ifdef CPL_LSB
            if( psImage->nWordSize * 8 == psImage->nBitsPerSample )
            {
                NITFSwapWords( psImage, pData,
                            psImage->nBlockWidth * psImage->nBlockHeight);
            }
#endif

            return BLKREAD_OK;
        }
    }

    if( psImage->szIC[0] == 'N' )
    {
        /* read all the data needed to get our requested band-block */
        if( psImage->nBitsPerSample != psImage->nWordSize * 8 )
        {
            if( psImage->chIMODE == 'S' )
            {
                nWrkBufSize = ((psImage->nBlockWidth * psImage->nBlockHeight * psImage->nBitsPerSample) + 7) / 8;
                if( VSIFSeekL( psImage->psFile->fp, psImage->panBlockStart[iFullBlock], SEEK_SET ) != 0 
                  || (int) VSIFReadL( pData, 1, nWrkBufSize, psImage->psFile->fp ) != nWrkBufSize )
                {
                    CPLError( CE_Failure, CPLE_FileIO, 
                              "Unable to read %d byte block from %d.", 
                              (int) nWrkBufSize, 
                              (int) psImage->panBlockStart[iFullBlock] );
                    return BLKREAD_FAIL;
                }

                return BLKREAD_OK;
            }
        }
    }

/* -------------------------------------------------------------------- */
/*      Read the requested information into a temporary buffer and      */
/*      pull out what we want.                                          */
/* -------------------------------------------------------------------- */
    if( psImage->szIC[0] == 'N' )
    {
        GByte *pabyWrkBuf = (GByte *) VSIMalloc(nWrkBufSize);
        int   iPixel, iLine;

        if (pabyWrkBuf == NULL)
        {
            CPLError( CE_Failure, CPLE_OutOfMemory, 
                      "Cannot allocate working buffer" );
            return BLKREAD_FAIL;
        }

        /* read all the data needed to get our requested band-block */
        if( VSIFSeekL( psImage->psFile->fp, psImage->panBlockStart[iFullBlock], 
                      SEEK_SET ) != 0 
            || (int) VSIFReadL( pabyWrkBuf, 1, nWrkBufSize,
                               psImage->psFile->fp ) != nWrkBufSize )
        {
            CPLError( CE_Failure, CPLE_FileIO, 
                      "Unable to read %d byte block from " CPL_FRMT_GUIB ".", 
                      nWrkBufSize, psImage->panBlockStart[iFullBlock] );
            CPLFree( pabyWrkBuf );
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
        NITFSwapWords( psImage, pData,
                       psImage->nBlockWidth * psImage->nBlockHeight);
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

        if( psImage->apanVQLUT[0] == NULL )
        {
            CPLError( CE_Failure, CPLE_NotSupported, 
                      "File lacks VQ LUTs, unable to decode imagery." );
            return BLKREAD_FAIL;
        }

        /* Read the codewords */
        if( VSIFSeekL(psImage->psFile->fp, psImage->panBlockStart[iFullBlock], 
                      SEEK_SET ) != 0 
            || VSIFReadL(abyVQCoded, 1, sizeof(abyVQCoded),
                         psImage->psFile->fp ) != sizeof(abyVQCoded) )
        {
            CPLError( CE_Failure, CPLE_FileIO, 
                      "Unable to read %d byte block from " CPL_FRMT_GUIB ".", 
                      (int) sizeof(abyVQCoded), 
                      psImage->panBlockStart[iFullBlock] );
            return BLKREAD_FAIL;
        }
        
        NITFUncompressVQTile( psImage, abyVQCoded, pData );

        return BLKREAD_OK;
    }

/* -------------------------------------------------------------------- */
/*      Handle ARIDPCM compression.                                     */
/* -------------------------------------------------------------------- */
    else if( EQUAL(psImage->szIC,"C2") || EQUAL(psImage->szIC,"M2") )
    {
        size_t nRawBytes;
        NITFSegmentInfo *psSegInfo;
        int success;
        GByte *pabyRawData;

        if (psImage->nBitsPerSample != 8)
        {
            CPLError( CE_Failure, CPLE_AppDefined, 
                      "Unsupported bits per sample value (%d) for C2/M2 compression",
                      psImage->nBitsPerSample);
            return BLKREAD_FAIL;
        }

        if( iFullBlock < psImage->nBlocksPerRow * psImage->nBlocksPerColumn-1 )
            nRawBytes = (size_t)( psImage->panBlockStart[iFullBlock+1] 
                - psImage->panBlockStart[iFullBlock] );
        else
        {
            psSegInfo = psImage->psFile->pasSegmentInfo + psImage->iSegment;
            nRawBytes = (size_t)(psSegInfo->nSegmentStart 
                                + psSegInfo->nSegmentSize 
                                - psImage->panBlockStart[iFullBlock]);
        }

        pabyRawData = (GByte *) VSIMalloc( nRawBytes );
        if (pabyRawData == NULL)
        {
            CPLError( CE_Failure, CPLE_OutOfMemory, 
                      "Cannot allocate working buffer" );
            return BLKREAD_FAIL;
        }

        /* Read the codewords */
        if( VSIFSeekL(psImage->psFile->fp, psImage->panBlockStart[iFullBlock], 
                      SEEK_SET ) != 0 
            || VSIFReadL(pabyRawData, 1, nRawBytes, psImage->psFile->fp ) !=  
            nRawBytes )
        {
            CPLError( CE_Failure, CPLE_FileIO, 
                      "Unable to read %d byte block from " CPL_FRMT_GUIB ".", 
                      (int) nRawBytes, psImage->panBlockStart[iFullBlock] );
            CPLFree( pabyRawData );
            return BLKREAD_FAIL;
        }
        
        success = NITFUncompressARIDPCM( psImage, pabyRawData, nRawBytes, pData );
        
        CPLFree( pabyRawData );

        if( success )
            return BLKREAD_OK;
        else
            return BLKREAD_FAIL;
    }

/* -------------------------------------------------------------------- */
/*      Handle BILEVEL (C1) compression.                                */
/* -------------------------------------------------------------------- */
    else if( EQUAL(psImage->szIC,"C1") || EQUAL(psImage->szIC,"M1") )
    {
        size_t nRawBytes;
        NITFSegmentInfo *psSegInfo;
        int success;
        GByte *pabyRawData;

        if (psImage->nBitsPerSample != 1)
        {
            CPLError( CE_Failure, CPLE_AppDefined, 
                      "Invalid bits per sample value (%d) for C1/M1 compression",
                      psImage->nBitsPerSample);
            return BLKREAD_FAIL;
        }

        if( iFullBlock < psImage->nBlocksPerRow * psImage->nBlocksPerColumn-1 )
            nRawBytes = (size_t)( psImage->panBlockStart[iFullBlock+1]
                                  - psImage->panBlockStart[iFullBlock] );
        else
        {
            psSegInfo = psImage->psFile->pasSegmentInfo + psImage->iSegment;
            nRawBytes = (size_t)( psSegInfo->nSegmentStart 
                            + psSegInfo->nSegmentSize
                            - psImage->panBlockStart[iFullBlock] );
        }

        pabyRawData = (GByte *) VSIMalloc( nRawBytes );
        if (pabyRawData == NULL)
        {
            CPLError( CE_Failure, CPLE_OutOfMemory, 
                      "Cannot allocate working buffer" );
            return BLKREAD_FAIL;
        }

        /* Read the codewords */
        if( VSIFSeekL(psImage->psFile->fp, psImage->panBlockStart[iFullBlock], 
                      SEEK_SET ) != 0 
            || VSIFReadL(pabyRawData, 1, nRawBytes, psImage->psFile->fp ) !=  
            nRawBytes )
        {
            CPLError( CE_Failure, CPLE_FileIO, 
                      "Unable to read %d byte block from " CPL_FRMT_GUIB ".", 
                      (int) nRawBytes, psImage->panBlockStart[iFullBlock] );
            return BLKREAD_FAIL;
        }
        
        success = NITFUncompressBILEVEL( psImage, pabyRawData, (int)nRawBytes, 
                                         pData );
        
        CPLFree( pabyRawData );

        if( success )
            return BLKREAD_OK;
        else
            return BLKREAD_FAIL;
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
    GUIntBig   nWrkBufSize;
    int   iBaseBlock = nBlockX + nBlockY * psImage->nBlocksPerRow;
    int   iFullBlock = iBaseBlock 
        + (nBand-1) * psImage->nBlocksPerRow * psImage->nBlocksPerColumn;

    if( nBand == 0 )
        return BLKREAD_FAIL;

    nWrkBufSize = psImage->nLineOffset * (psImage->nBlockHeight-1)
        + psImage->nPixelOffset * (psImage->nBlockWidth-1)
        + psImage->nWordSize;

    if (nWrkBufSize == 0)
      nWrkBufSize = (psImage->nBlockWidth*psImage->nBlockHeight*psImage->nBitsPerSample+7)/8;

/* -------------------------------------------------------------------- */
/*      Can we do a direct read into our buffer?                        */
/* -------------------------------------------------------------------- */
    if( psImage->nWordSize == psImage->nPixelOffset
        && psImage->nWordSize * psImage->nBlockWidth == psImage->nLineOffset 
        && psImage->szIC[0] != 'C' && psImage->szIC[0] != 'M' )
    {
#ifdef CPL_LSB
        NITFSwapWords( psImage, pData,
                       psImage->nBlockWidth * psImage->nBlockHeight);
#endif

        if( VSIFSeekL( psImage->psFile->fp, psImage->panBlockStart[iFullBlock], 
                      SEEK_SET ) != 0 
            || (GUIntBig) VSIFWriteL( pData, 1, (size_t)nWrkBufSize,
                                psImage->psFile->fp ) != nWrkBufSize )
        {
            CPLError( CE_Failure, CPLE_FileIO, 
                      "Unable to write " CPL_FRMT_GUIB " byte block from " CPL_FRMT_GUIB ".", 
                      nWrkBufSize, psImage->panBlockStart[iFullBlock] );
            return BLKREAD_FAIL;
        }
        else
        {
#ifdef CPL_LSB
            /* restore byte order to original */
            NITFSwapWords( psImage, pData,
                       psImage->nBlockWidth * psImage->nBlockHeight);
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
    GUIntBig   nLineOffsetInFile;
    size_t        nLineSize;
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

    nLineSize = (size_t)psImage->nPixelOffset * (psImage->nCols - 1) 
        + psImage->nWordSize;

    if (nLineSize == 0 || psImage->nWordSize * 8 != psImage->nBitsPerSample)
      nLineSize = (psImage->nBlockWidth*psImage->nBitsPerSample+7)/8;

    VSIFSeekL( psImage->psFile->fp, nLineOffsetInFile, SEEK_SET );

/* -------------------------------------------------------------------- */
/*      Can we do a direct read into our buffer.                        */
/* -------------------------------------------------------------------- */
    if( (psImage->nBitsPerSample % 8) != 0 ||
        (psImage->nWordSize == psImage->nPixelOffset
        && psImage->nWordSize * psImage->nBlockWidth == psImage->nLineOffset) )
    {
        if( VSIFReadL( pData, 1, nLineSize, psImage->psFile->fp ) !=  
            nLineSize )
        {
            CPLError( CE_Failure, CPLE_FileIO, 
                      "Unable to read %d bytes for line %d.", (int) nLineSize, nLine );
            return BLKREAD_FAIL;
        }

#ifdef CPL_LSB
        NITFSwapWords( psImage, pData, psImage->nBlockWidth);
#endif

        return BLKREAD_OK;
    }

/* -------------------------------------------------------------------- */
/*      Allocate a buffer for all the interleaved data, and read        */
/*      it.                                                             */
/* -------------------------------------------------------------------- */
    pabyLineBuf = (unsigned char *) VSIMalloc(nLineSize);
    if (pabyLineBuf == NULL)
    {
        CPLError( CE_Failure, CPLE_OutOfMemory, 
                "Cannot allocate working buffer" );
        return BLKREAD_FAIL;
    }

    if( VSIFReadL( pData, 1, nLineSize, psImage->psFile->fp ) !=  
        nLineSize )
    {
        CPLError( CE_Failure, CPLE_FileIO, 
                    "Unable to read %d bytes for line %d.", (int) nLineSize, nLine );
        CPLFree(pabyLineBuf);
        return BLKREAD_FAIL;
    }

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
        NITFSwapWords(  psImage, pabyDst, psImage->nBlockWidth);
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
    GUIntBig   nLineOffsetInFile;
    size_t        nLineSize;
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

    nLineSize = (size_t)psImage->nPixelOffset * (psImage->nCols - 1) 
        + psImage->nWordSize;

    VSIFSeekL( psImage->psFile->fp, nLineOffsetInFile, SEEK_SET );

/* -------------------------------------------------------------------- */
/*      Can we do a direct write into our buffer.                       */
/* -------------------------------------------------------------------- */
    if( psImage->nWordSize == psImage->nPixelOffset
        && psImage->nWordSize * psImage->nBlockWidth == psImage->nLineOffset )
    {
#ifdef CPL_LSB
        NITFSwapWords( psImage, pData, psImage->nCols );
#endif

        VSIFWriteL( pData, 1, nLineSize, psImage->psFile->fp );

#ifdef CPL_LSB
        NITFSwapWords( psImage, pData, psImage->nCols );
#endif

        return BLKREAD_OK;
    }

/* -------------------------------------------------------------------- */
/*      Allocate a buffer for all the interleaved data, and read        */
/*      it.                                                             */
/* -------------------------------------------------------------------- */
    pabyLineBuf = (unsigned char *) VSIMalloc(nLineSize);
    if (pabyLineBuf == NULL)
    {
        CPLError( CE_Failure, CPLE_OutOfMemory, 
                "Cannot allocate working buffer" );
        return BLKREAD_FAIL;
    }

    VSIFReadL( pabyLineBuf, 1, nLineSize, psImage->psFile->fp );

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
            NITFSwapWords( psImage, pabyDst + iPixel * psImage->nWordSize, 1 );
#endif
        }
    }

/* -------------------------------------------------------------------- */
/*      Write the results back out.                                     */
/* -------------------------------------------------------------------- */
    VSIFSeekL( psImage->psFile->fp, nLineOffsetInFile, SEEK_SET );
    VSIFWriteL( pabyLineBuf, 1, nLineSize, psImage->psFile->fp );
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

/* -------------------------------------------------------------------- */
/*      Do careful rounding on seconds so that 59.9->60 is properly     */
/*      rolled into minutes and degrees.                                */
/* -------------------------------------------------------------------- */
    nSeconds = (int) (dfValue + 0.5);
    if (nSeconds == 60) 
    {
        nSeconds = 0;
        nMinutes += 1;
        if (nMinutes == 60) 
        {
            nMinutes = 0;
            nDegrees += 1;
        }
    }

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

/* Check that easting can be represented as a 6 character string */
#define CHECK_IGEOLO_UTM_X(name, x) \
    if ((int) floor((x)+0.5) <= -100000 || (int) floor((x)+0.5) >= 1000000) \
    { \
        CPLError( CE_Failure, CPLE_AppDefined, \
                  "Attempt to write UTM easting %s=%d which is outside of valid range.", name, (int) floor((x)+0.5) ); \
        return FALSE; \
    }

/* Check that northing can be represented as a 7 character string */
#define CHECK_IGEOLO_UTM_Y(name, y) \
    if ((int) floor((y)+0.5) <= -1000000 || (int) floor((y)+0.5) >= 10000000) \
    { \
        CPLError( CE_Failure, CPLE_AppDefined, \
                  "Attempt to write UTM northing %s=%d which is outside of valid range.", name, (int) floor((y)+0.5) ); \
        return FALSE; \
    }

int NITFWriteIGEOLO( NITFImage *psImage, char chICORDS,
                     int nZone, 
                     double dfULX, double dfULY,
                     double dfURX, double dfURY,
                     double dfLRX, double dfLRY,
                     double dfLLX, double dfLLY )

{
    char szIGEOLO[61];

/* -------------------------------------------------------------------- */
/*      Do some checking.                                               */
/* -------------------------------------------------------------------- */
    if( psImage->chICORDS == ' ' )
    {
        CPLError(CE_Failure, CPLE_NotSupported, 
                 "Apparently no space reserved for IGEOLO info in NITF file.\n"
                 "NITFWriteIGEOGLO() fails." );
        return FALSE;
    }

    if( chICORDS != 'G' && chICORDS != 'N' && chICORDS != 'S' && chICORDS != 'D')
    {
        CPLError( CE_Failure, CPLE_NotSupported, 
                  "Invalid ICOORDS value (%c) for NITFWriteIGEOLO().", chICORDS );
        return FALSE;
    }

/* -------------------------------------------------------------------- */
/*      Format geographic coordinates in DMS                            */
/* -------------------------------------------------------------------- */
    if( chICORDS == 'G' )
    {
        if( fabs(dfULX) > 180 || fabs(dfURX) > 180 
            || fabs(dfLRX) > 180 || fabs(dfLLX) > 180 
            || fabs(dfULY) >  90 || fabs(dfURY) >  90
            || fabs(dfLRY) >  90 || fabs(dfLLY) >  90 )
        {
            CPLError( CE_Failure, CPLE_AppDefined, 
                      "Attempt to write geographic bound outside of legal range." );
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
    }
/* -------------------------------------------------------------------- */
/*      Format geographic coordinates in decimal degrees                */
/* -------------------------------------------------------------------- */
    else if( chICORDS == 'D' )
    {
        if( fabs(dfULX) > 180 || fabs(dfURX) > 180 
            || fabs(dfLRX) > 180 || fabs(dfLLX) > 180 
            || fabs(dfULY) >  90 || fabs(dfURY) >  90
            || fabs(dfLRY) >  90 || fabs(dfLLY) >  90 )
        {
            CPLError( CE_Failure, CPLE_AppDefined, 
                      "Attempt to write geographic bound outside of legal range." );
            return FALSE;
        }

        sprintf(szIGEOLO + 0, "%+#07.3f%+#08.3f", dfULY, dfULX);
        sprintf(szIGEOLO + 15, "%+#07.3f%+#08.3f", dfURY, dfURX);
        sprintf(szIGEOLO + 30, "%+#07.3f%+#08.3f", dfLRY, dfLRX);
        sprintf(szIGEOLO + 45, "%+#07.3f%+#08.3f", dfLLY, dfLLX);
    }

/* -------------------------------------------------------------------- */
/*      Format UTM coordinates.                                         */
/* -------------------------------------------------------------------- */
    else if( chICORDS == 'N' || chICORDS == 'S' )
    {
        CHECK_IGEOLO_UTM_X("dfULX", dfULX);
        CHECK_IGEOLO_UTM_Y("dfULY", dfULY);
        CHECK_IGEOLO_UTM_X("dfURX", dfURX);
        CHECK_IGEOLO_UTM_Y("dfURY", dfURY);
        CHECK_IGEOLO_UTM_X("dfLRX", dfLRX);
        CHECK_IGEOLO_UTM_Y("dfLRY", dfLRY);
        CHECK_IGEOLO_UTM_X("dfLLX", dfLLX);
        CHECK_IGEOLO_UTM_Y("dfLLY", dfLLY);
        sprintf( szIGEOLO + 0, "%02d%06d%07d",
                 nZone, (int) floor(dfULX+0.5), (int) floor(dfULY+0.5) );
        sprintf( szIGEOLO + 15, "%02d%06d%07d",
                 nZone, (int) floor(dfURX+0.5), (int) floor(dfURY+0.5) );
        sprintf( szIGEOLO + 30, "%02d%06d%07d",
                 nZone, (int) floor(dfLRX+0.5), (int) floor(dfLRY+0.5) );
        sprintf( szIGEOLO + 45, "%02d%06d%07d",
                 nZone, (int) floor(dfLLX+0.5), (int) floor(dfLLY+0.5) );
    }

/* -------------------------------------------------------------------- */
/*      Write IGEOLO data to disk.                                      */
/* -------------------------------------------------------------------- */
    if( VSIFSeekL( psImage->psFile->fp, 
                  psImage->psFile->pasSegmentInfo[psImage->iSegment].nSegmentHeaderStart + 372, SEEK_SET ) == 0
        && VSIFWriteL( szIGEOLO, 1, 60, psImage->psFile->fp ) == 60 )
    {
        return TRUE;
    }
    else
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "I/O Error writing IGEOLO segment.\n%s",
                  VSIStrerror( errno ) );
        return FALSE;
    }
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

    VSIFSeekL( psImage->psFile->fp, psBandInfo->nLUTLocation, SEEK_SET );
    VSIFWriteL( pabyLUT, 1, nColors, psImage->psFile->fp );
    VSIFSeekL( psImage->psFile->fp, 
              psBandInfo->nLUTLocation + psBandInfo->nSignificantLUTEntries, 
              SEEK_SET );
    VSIFWriteL( pabyLUT+256, 1, nColors, psImage->psFile->fp );
    VSIFSeekL( psImage->psFile->fp, 
              psBandInfo->nLUTLocation + 2*psBandInfo->nSignificantLUTEntries, 
              SEEK_SET );
    VSIFWriteL( pabyLUT+512, 1, nColors, psImage->psFile->fp );

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
/*                           NITFSwapWords()                            */
/************************************************************************/

#ifdef CPL_LSB

static void NITFSwapWordsInternal( void *pData, int nWordSize, int nWordCount,
                                   int nWordSkip )

{
    int         i;
    GByte       *pabyData = (GByte *) pData;

    switch( nWordSize )
    {
      case 1:
        break;

      case 2:
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
        break;
    }
}

/* Swap real or complex types */
static void NITFSwapWords( NITFImage *psImage, void *pData, int nWordCount )

{
    if( EQUAL(psImage->szPVType,"C") )
    {
        /* According to http://jitc.fhu.disa.mil/nitf/tag_reg/imagesubheader/pvtype.html */
        /* "C values shall be represented with the Real and Imaginary parts, each represented */
        /* in IEEE 32 or 64-bit floating point representation (IEEE 754) and appearing in */
        /* adjacent four or eight-byte blocks, first Real, then Imaginary" */
        NITFSwapWordsInternal(  pData,
                                psImage->nWordSize / 2,
                                2 * nWordCount,
                                psImage->nWordSize / 2 );
    }
    else
    {
        NITFSwapWordsInternal( pData,
                               psImage->nWordSize,
                               nWordCount, 
                               psImage->nWordSize );
    }
}

#endif /* def CPL_LSB */

/************************************************************************/
/*                           NITFReadRPC00B()                           */
/*                                                                      */
/*      Read an RPC00A or RPC00B structure if the TRE is available.     */
/*      RPC00A is remapped into RPC00B organization.                    */
/************************************************************************/

int NITFReadRPC00B( NITFImage *psImage, NITFRPC00BInfo *psRPC )

{
    static const int anRPC00AMap[] = /* See ticket #2040 */
    {0, 1, 2, 3, 4, 5, 6 , 10, 7, 8, 9, 11, 14, 17, 12, 15, 18, 13, 16, 19};

    const char *pachTRE;
    char szTemp[100];
    int  i;
    int  bRPC00A = FALSE;
    int  nTRESize;

    psRPC->SUCCESS = 0;

/* -------------------------------------------------------------------- */
/*      Do we have the TRE?                                             */
/* -------------------------------------------------------------------- */
    pachTRE = NITFFindTRE( psImage->pachTRE, psImage->nTREBytes, 
                           "RPC00B", &nTRESize );

    if( pachTRE == NULL )
    {
        pachTRE = NITFFindTRE( psImage->pachTRE, psImage->nTREBytes,
                               "RPC00A", &nTRESize );
        if( pachTRE )
            bRPC00A = TRUE;
    }

    if( pachTRE == NULL )
    {
        return FALSE;
    }

    if (nTRESize < 801 + 19*12 + 12)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Cannot read RPC00A/RPC00B TRE. Not enough bytes");
        return FALSE;
    }

/* -------------------------------------------------------------------- */
/*      Parse out field values.                                         */
/* -------------------------------------------------------------------- */
    psRPC->SUCCESS = atoi(NITFGetField(szTemp, pachTRE, 0, 1 ));
	
    if ( !psRPC->SUCCESS )
	fprintf( stdout, "RPC Extension not Populated!\n");

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
        int iSrcCoef = i;

        if( bRPC00A )
            iSrcCoef = anRPC00AMap[i];

        psRPC->LINE_NUM_COEFF[i] = 
            atof(NITFGetField(szTemp, pachTRE, 81+iSrcCoef*12, 12));
        psRPC->LINE_DEN_COEFF[i] = 
            atof(NITFGetField(szTemp, pachTRE, 321+iSrcCoef*12, 12));
        psRPC->SAMP_NUM_COEFF[i] = 
            atof(NITFGetField(szTemp, pachTRE, 561+iSrcCoef*12, 12));
        psRPC->SAMP_DEN_COEFF[i] = 
            atof(NITFGetField(szTemp, pachTRE, 801+iSrcCoef*12, 12));
    }

    return TRUE;
}

/************************************************************************/
/*                           NITFReadICHIPB()                           */
/*                                                                      */
/*      Read an ICHIPB structure if the TRE is available.               */
/************************************************************************/

int NITFReadICHIPB( NITFImage *psImage, NITFICHIPBInfo *psICHIP )

{
    const char *pachTRE;
    char szTemp[32];
    int nTRESize;

/* -------------------------------------------------------------------- */
/*      Do we have the TRE?                                             */
/* -------------------------------------------------------------------- */
    pachTRE = NITFFindTRE( psImage->pachTRE, psImage->nTREBytes, 
                           "ICHIPB", &nTRESize );

    if( pachTRE == NULL )
    {
        pachTRE = NITFFindTRE( psImage->pachTRE, psImage->nTREBytes,
                               "ICHIPA", &nTRESize );
    }

    if( pachTRE == NULL )
    {
        return FALSE;
    }

    if (nTRESize < 2)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Cannot read ICHIPA/ICHIPB TRE. Not enough bytes");
        return FALSE;
    }
/* -------------------------------------------------------------------- */
/*      Parse out field values.                                         */
/* -------------------------------------------------------------------- */
    psICHIP->XFRM_FLAG = atoi(NITFGetField(szTemp, pachTRE, 0, 2 ));

    if ( psICHIP->XFRM_FLAG == 0 )
    {
        if (nTRESize < 216 + 8)
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                    "Cannot read ICHIPA/ICHIPB TRE. Not enough bytes");
            return FALSE;
        }

        psICHIP->SCALE_FACTOR = atof(NITFGetField(szTemp, pachTRE, 2, 10 ));
        psICHIP->ANAMORPH_CORR = atoi(NITFGetField(szTemp, pachTRE, 12, 2 ));
        psICHIP->SCANBLK_NUM = atoi(NITFGetField(szTemp, pachTRE, 14, 2 ));

        psICHIP->OP_ROW_11 = atof(NITFGetField(szTemp, pachTRE, 16, 12 ));
        psICHIP->OP_COL_11 = atof(NITFGetField(szTemp, pachTRE, 28, 12 ));

        psICHIP->OP_ROW_12 = atof(NITFGetField(szTemp, pachTRE, 40, 12 ));
        psICHIP->OP_COL_12 = atof(NITFGetField(szTemp, pachTRE, 52, 12 ));

        psICHIP->OP_ROW_21 = atof(NITFGetField(szTemp, pachTRE, 64, 12 ));
        psICHIP->OP_COL_21 = atof(NITFGetField(szTemp, pachTRE, 76, 12 ));

        psICHIP->OP_ROW_22 = atof(NITFGetField(szTemp, pachTRE, 88, 12 ));
        psICHIP->OP_COL_22 = atof(NITFGetField(szTemp, pachTRE, 100, 12 ));

        psICHIP->FI_ROW_11 = atof(NITFGetField(szTemp, pachTRE, 112, 12 ));
        psICHIP->FI_COL_11 = atof(NITFGetField(szTemp, pachTRE, 124, 12 ));

        psICHIP->FI_ROW_12 = atof(NITFGetField(szTemp, pachTRE, 136, 12 ));
        psICHIP->FI_COL_12 = atof(NITFGetField(szTemp, pachTRE, 148, 12 ));

        psICHIP->FI_ROW_21 = atof(NITFGetField(szTemp, pachTRE, 160, 12 ));
        psICHIP->FI_COL_21 = atof(NITFGetField(szTemp, pachTRE, 172, 12 ));

        psICHIP->FI_ROW_22 = atof(NITFGetField(szTemp, pachTRE, 184, 12 ));
        psICHIP->FI_COL_22 = atof(NITFGetField(szTemp, pachTRE, 196, 12 ));

        psICHIP->FI_ROW = atoi(NITFGetField(szTemp, pachTRE, 208, 8 ));
        psICHIP->FI_COL = atoi(NITFGetField(szTemp, pachTRE, 216, 8 ));
    }
    else
    {
        fprintf( stdout, "Chip is already de-warpped?\n" );
    }

    return TRUE;
}

/************************************************************************/
/*                           NITFReadUSE00A()                           */
/*                                                                      */
/*      Read a USE00A TRE and return contents as metadata strings.      */
/************************************************************************/

char **NITFReadUSE00A( NITFImage *psImage )

{
    const char *pachTRE;
    int  nTRESize;
    char **papszMD = NULL;
    int nRemainingBytes;


/* -------------------------------------------------------------------- */
/*      Do we have the TRE?                                             */
/* -------------------------------------------------------------------- */
    pachTRE = NITFFindTRE( psImage->pachTRE, psImage->nTREBytes, 
                           "USE00A", &nTRESize );

    if( pachTRE == NULL )
        return NULL;

    if( nTRESize != 107 )
    {
        CPLError( CE_Warning, CPLE_AppDefined, 
                  "USE00A TRE wrong size, ignoring." );
        return NULL;
    }

    nRemainingBytes = psImage->nTREBytes - (pachTRE - psImage->pachTRE);
    if (nRemainingBytes < 107)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                "Cannot read USE00A TRE. Not enough bytes");
        return FALSE;
    }
/* -------------------------------------------------------------------- */
/*      Parse out field values.                                         */
/* -------------------------------------------------------------------- */
    NITFExtractMetadata( &papszMD, pachTRE,   0,   3, 
                         "NITF_USE00A_ANGLE_TO_NORTH" );
    NITFExtractMetadata( &papszMD, pachTRE,   3,   5, 
                         "NITF_USE00A_MEAN_GSD" );
    /* reserved: 1 */
    NITFExtractMetadata( &papszMD, pachTRE,   9,   5, 
                         "NITF_USE00A_DYNAMIC_RANGE" );
    /* reserved: 3+1+3 */
    NITFExtractMetadata( &papszMD, pachTRE,  21,   5, 
                         "NITF_USE00A_OBL_ANG" );
    NITFExtractMetadata( &papszMD, pachTRE,  26,   6, 
                         "NITF_USE00A_ROLL_ANG" );
    /* reserved: 12+15+4+1+3+1+1 = 37 */
    NITFExtractMetadata( &papszMD, pachTRE,  69,   2, 
                         "NITF_USE00A_N_REF" );
    NITFExtractMetadata( &papszMD, pachTRE,  71,   5, 
                         "NITF_USE00A_REV_NUM" );
    NITFExtractMetadata( &papszMD, pachTRE,  76,   3, 
                         "NITF_USE00A_N_SEG" );
    NITFExtractMetadata( &papszMD, pachTRE,  79,   6, 
                         "NITF_USE00A_MAX_LP_SEG" );
    /* reserved: 6+6 */
    NITFExtractMetadata( &papszMD, pachTRE,  97,   5, 
                         "NITF_USE00A_SUN_EL" );
    NITFExtractMetadata( &papszMD, pachTRE, 102,   5, 
                         "NITF_USE00A_SUN_AZ" );

    return papszMD;
}

/************************************************************************/
/*                           NITFReadBLOCKA()                           */
/*                                                                      */
/*      Read a BLOCKA SDE and return contents as metadata strings.      */
/************************************************************************/

char **NITFReadBLOCKA( NITFImage *psImage )

{
    const char *pachTRE;
    int  nTRESize;
    char **papszMD = NULL;
    int nBlockaCount = 0;
    char szTemp[128];

    while ( TRUE )
    {
/* -------------------------------------------------------------------- */
/*      Do we have the TRE?                                             */
/* -------------------------------------------------------------------- */
        pachTRE = NITFFindTREByIndex( psImage->pachTRE, psImage->nTREBytes, 
                                      "BLOCKA", nBlockaCount,
                                      &nTRESize );

        if( pachTRE == NULL )
            break;

        if( nTRESize != 123 )
        {
            CPLError( CE_Warning, CPLE_AppDefined, 
                      "BLOCKA TRE wrong size, ignoring." );
            break;
        }

        nBlockaCount++;

/* -------------------------------------------------------------------- */
/*      Parse out field values.                                         */
/* -------------------------------------------------------------------- */
        sprintf( szTemp, "NITF_BLOCKA_BLOCK_INSTANCE_%02d", nBlockaCount );
        NITFExtractMetadata( &papszMD, pachTRE,   0,   2, szTemp );
        sprintf( szTemp, "NITF_BLOCKA_N_GRAY_%02d", nBlockaCount );
        NITFExtractMetadata( &papszMD, pachTRE,   2,   5, szTemp );
        sprintf( szTemp, "NITF_BLOCKA_L_LINES_%02d",      nBlockaCount );
        NITFExtractMetadata( &papszMD, pachTRE,   7,   5, szTemp );
        sprintf( szTemp, "NITF_BLOCKA_LAYOVER_ANGLE_%02d",nBlockaCount );
        NITFExtractMetadata( &papszMD, pachTRE,  12,   3, szTemp );
        sprintf( szTemp, "NITF_BLOCKA_SHADOW_ANGLE_%02d", nBlockaCount );
        NITFExtractMetadata( &papszMD, pachTRE,  15,   3, szTemp );
        /* reserved: 16 */
        sprintf( szTemp, "NITF_BLOCKA_FRLC_LOC_%02d",     nBlockaCount );
        NITFExtractMetadata( &papszMD, pachTRE,  34,  21, szTemp );
        sprintf( szTemp, "NITF_BLOCKA_LRLC_LOC_%02d",     nBlockaCount );
        NITFExtractMetadata( &papszMD, pachTRE,  55,  21, szTemp );
        sprintf( szTemp, "NITF_BLOCKA_LRFC_LOC_%02d",     nBlockaCount );
        NITFExtractMetadata( &papszMD, pachTRE,  76,  21, szTemp );
        sprintf( szTemp, "NITF_BLOCKA_FRFC_LOC_%02d",     nBlockaCount );
        NITFExtractMetadata( &papszMD, pachTRE,  97,  21, szTemp );
        /* reserved: 5 -> 97 + 21 + 5 = 123 -> OK */
    }

    if ( nBlockaCount > 0 )
    {
        sprintf( szTemp, "%02d", nBlockaCount );
        papszMD = CSLSetNameValue( papszMD, "NITF_BLOCKA_BLOCK_COUNT", szTemp );
    }

    return papszMD;
}


/************************************************************************/
/*                           NITFGetGCP()                               */
/*                                                                      */
/* Reads a geographical coordinate (lat, long) from the provided        */
/* buffer.                                                              */
/************************************************************************/

void NITFGetGCP ( const char* pachCoord, double *pdfXYs, int iCoord )
{
    char szTemp[128];

    // offset to selected coordinate.
    pdfXYs += 2 * iCoord;

    if( pachCoord[0] == 'N' || pachCoord[0] == 'n' || 
        pachCoord[0] == 'S' || pachCoord[0] == 's' )
    {	
        /* ------------------------------------------------------------ */
        /*                             0....+....1....+....2            */
        /* Coordinates are in the form Xddmmss.ssYdddmmss.ss:           */
        /* The format Xddmmss.cc represents degrees (00 to 89), minutes */
        /* (00 to 59), seconds (00 to 59), and hundredths of seconds    */
        /* (00 to 99) of latitude, with X = N for north or S for south, */
        /* and Ydddmmss.cc represents degrees (000 to 179), minutes     */
        /* (00 to 59), seconds (00 to 59), and hundredths of seconds    */
        /* (00 to 99) of longitude, with Y = E for east or W for west.  */
        /* ------------------------------------------------------------ */

        pdfXYs[1] = 
            atof(NITFGetField( szTemp, pachCoord, 1, 2 )) 
          + atof(NITFGetField( szTemp, pachCoord, 3, 2 )) / 60.0
          + atof(NITFGetField( szTemp, pachCoord, 5, 5 )) / 3600.0;

        if( pachCoord[0] == 's' || pachCoord[0] == 'S' )
            pdfXYs[1] *= -1;

        pdfXYs[0] = 
            atof(NITFGetField( szTemp, pachCoord,11, 3 )) 
          + atof(NITFGetField( szTemp, pachCoord,14, 2 )) / 60.0
          + atof(NITFGetField( szTemp, pachCoord,16, 5 )) / 3600.0;

        if( pachCoord[10] == 'w' || pachCoord[10] == 'W' )
            pdfXYs[0] *= -1;
    }
    else
    {
        /* ------------------------------------------------------------ */
        /*                             0....+....1....+....2            */
        /* Coordinates are in the form ±dd.dddddd±ddd.dddddd:           */
        /* The format ±dd.dddddd indicates degrees of latitude (north   */
        /* is positive), and ±ddd.dddddd represents degrees of          */
        /* longitude (east is positive).                                */
        /* ------------------------------------------------------------ */

        pdfXYs[1] = atof(NITFGetField( szTemp, pachCoord, 0, 10 ));
        pdfXYs[0] = atof(NITFGetField( szTemp, pachCoord,10, 11 ));
    }
}

/************************************************************************/
/*                           NITFReadBLOCKA_GCPs()                      */
/*                                                                      */
/* The BLOCKA repeat earth coordinates image corner locations described */
/* by IGEOLO in the NITF image subheader, but provide higher precision. */
/************************************************************************/

int NITFReadBLOCKA_GCPs( NITFImage *psImage )
{
    const char *pachTRE;
    int        nTRESize;
    int        nBlockaLines;
    char       szTemp[128];
    
/* -------------------------------------------------------------------- */
/*      Do we have the TRE?                                             */
/* -------------------------------------------------------------------- */
    pachTRE = NITFFindTRE( psImage->pachTRE, psImage->nTREBytes, 
                           "BLOCKA", &nTRESize );

    if( pachTRE == NULL )
        return FALSE;

    if( nTRESize != 123 )
    {
        return FALSE;
    }

/* -------------------------------------------------------------------- */
/*      Parse out field values.                                         */
/* -------------------------------------------------------------------- */

    /* ---------------------------------------------------------------- */
    /* Make sure the BLOCKA geo coordinates are set. Spaces indicate    */
    /* the value of a coordinate is unavailable or inapplicable.        */
    /* ---------------------------------------------------------------- */
    if( pachTRE[34] == ' ' || pachTRE[55] == ' ' || 
        pachTRE[76] == ' ' || pachTRE[97] == ' ' )
    {
        return FALSE;
    }

    /* ---------------------------------------------------------------- */
    /* Extract the L_LINES field of BLOCKA and see if this instance     */
    /* covers the whole image. This is the case if L_LINES is equal to  */
    /* the no of rows of this image.                                    */
    /* We use the BLOCKA only in that case!                             */
    /* ---------------------------------------------------------------- */
    nBlockaLines = atoi(NITFGetField( szTemp, pachTRE, 7, 5 ));
    if( psImage->nRows != nBlockaLines )
    {
        return FALSE;
    }

    /* ---------------------------------------------------------------- */
    /* Note that the order of these coordinates is different from       */
    /* IGEOLO/NITFImage.                                                */
    /*                   IGEOLO            BLOCKA                       */
    /*                   0, 0              0, MaxCol                    */
    /*                   0, MaxCol         MaxRow, MaxCol               */
    /*                   MaxRow, MaxCol    MaxRow, 0                    */
    /*                   MaxRow, 0         0, 0                         */
    /* ---------------------------------------------------------------- */
    {
        double *pdfXYs = &(psImage->dfULX);
    
        NITFGetGCP ( pachTRE + 34, pdfXYs, 1 );
        NITFGetGCP ( pachTRE + 55, pdfXYs, 2 );
        NITFGetGCP ( pachTRE + 76, pdfXYs, 3 );
        NITFGetGCP ( pachTRE + 97, pdfXYs, 0 );
        
        psImage->bIsBoxCenterOfPixel = TRUE;
    }
    
    /* ---------------------------------------------------------------- */
    /* Regardless of the former value of ICORDS, the values are now in  */
    /* decimal degrees.                                                 */
    /* ---------------------------------------------------------------- */

    psImage->chICORDS = 'D';

    return TRUE;
}

/************************************************************************/
/*                        NITFReadGEOLOB()                              */
/*                                                                      */
/*      The GEOLOB contains high precision lat/long geotransform        */
/*      values.                                                         */
/************************************************************************/

static int NITFReadGEOLOB( NITFImage *psImage )
{
    const char *pachTRE;
    int        nTRESize;
    char       szTemp[128];

/* -------------------------------------------------------------------- */
/*      Do we have the TRE?                                             */
/* -------------------------------------------------------------------- */
    pachTRE = NITFFindTRE( psImage->pachTRE, psImage->nTREBytes, 
                           "GEOLOB", &nTRESize );

    if( pachTRE == NULL )
        return FALSE;

    if( !CSLTestBoolean(CPLGetConfigOption( "NITF_USEGEOLOB", "YES" )) )
    {
        CPLDebug( "NITF", "GEOLOB available, but ignored by request." );
        return FALSE;
    }

    if( nTRESize != 48 )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                "Cannot read GEOLOB TRE. Wrong size.");
        return FALSE;
    }

/* -------------------------------------------------------------------- */
/*      Parse out field values.                                         */
/* -------------------------------------------------------------------- */
    {
        double dfARV = atoi(NITFGetField( szTemp, pachTRE, 0, 9 ));
        double dfBRV = atoi(NITFGetField( szTemp, pachTRE, 9, 9 ));
        
        double dfLSO = atof(NITFGetField( szTemp, pachTRE, 18, 15 ));
        double dfPSO = atof(NITFGetField( szTemp, pachTRE, 33, 15 ));
        
        double dfPixelWidth  = 360.0 / dfARV;
        double dfPixelHeight = 360.0 / dfBRV;
        
        psImage->dfULX = dfLSO;
        psImage->dfURX = psImage->dfULX + psImage->nCols * dfPixelWidth;
        psImage->dfLLX = psImage->dfULX;
        psImage->dfLRX = psImage->dfURX;
        
        psImage->dfULY = dfPSO;
        psImage->dfURY = psImage->dfULY;
        psImage->dfLLY = psImage->dfULY - psImage->nRows * dfPixelHeight;
        psImage->dfLRY = psImage->dfLLY;

        psImage->bIsBoxCenterOfPixel = FALSE; // GEOLOB is edge of pixel.
        psImage->chICORDS = 'D';
        
        CPLDebug( "NITF", "IGEOLO bounds overridden by GEOLOB TRE." );
    }

    return TRUE;
}

/************************************************************************/
/*                         NITFFetchAttribute()                         */
/*                                                                      */
/*      Load one attribute given the attribute id, and the parameter    */
/*      id and the number of bytes to fetch.                            */
/************************************************************************/

static int NITFFetchAttribute( GByte *pabyAttributeSubsection, 
                               int nASSSize, int nAttrCount,
                               int nAttrID, int nParamID, int nBytesToFetch,
                               GByte *pabyBuffer )

{
    int i;
    GUInt32 nAttrOffset = 0;

/* -------------------------------------------------------------------- */
/*      Scan the attribute offset table                                 */
/* -------------------------------------------------------------------- */
    for( i = 0; i < nAttrCount; i++ )
    {
        GByte *pabyOffsetRec = i*8 + pabyAttributeSubsection;

        if( (pabyOffsetRec[0] * 256 + pabyOffsetRec[1]) == nAttrID
            && pabyOffsetRec[2] == nParamID )
        {
            memcpy( &nAttrOffset, pabyOffsetRec+4, 4 );
            CPL_MSBPTR32( &nAttrOffset );
            break;
        }
    }

/* -------------------------------------------------------------------- */
/*      Extract the attribute value.                                    */
/* -------------------------------------------------------------------- */
    if( nAttrOffset == 0 )
        return FALSE;

    if( nAttrOffset + nBytesToFetch > nASSSize )
        return FALSE;

    memcpy( pabyBuffer, pabyAttributeSubsection + nAttrOffset, nBytesToFetch );
    return TRUE;
}

/************************************************************************/
/*                      NITFLoadAttributeSection()                      */
/*                                                                      */
/*      Load metadata items from selected attributes in the RPF         */
/*      attributes subsection.  The items are defined in                */
/*      MIL-STD-2411-1 section 5.3.2.                                   */
/************************************************************************/

static void NITFLoadAttributeSection( NITFImage *psImage )

{
    int i;
    int nASHOffset=0, nASHSize=0, nASSOffset=0, nASSSize=0, nNextOffset=0;
    GInt16 nAttrCount;
    GByte *pabyAttributeSubsection;
    GByte abyBuffer[128];

    for( i = 0; (int)i < psImage->nLocCount; i++ )
    {
        if( psImage->pasLocations[i].nLocId == LID_AttributeSectionSubheader )
        {
            nASHOffset = psImage->pasLocations[i].nLocOffset;
            nASHSize = psImage->pasLocations[i].nLocSize;
        }
        else if( psImage->pasLocations[i].nLocId == LID_AttributeSubsection )
        {
            nASSOffset = psImage->pasLocations[i].nLocOffset;
            nASSSize = psImage->pasLocations[i].nLocSize;
            if (i + 1 < psImage->nLocCount)
                nNextOffset = psImage->pasLocations[i + 1].nLocOffset;
        }
    }

    if( nASSOffset == 0 || nASHOffset == 0 )
        return;

/* -------------------------------------------------------------------- */
/*      How many attribute records do we have?                          */
/* -------------------------------------------------------------------- */
    VSIFSeekL( psImage->psFile->fp, nASHOffset, SEEK_SET );
    VSIFReadL( &nAttrCount, 2, 1, psImage->psFile->fp );

    CPL_MSBPTR16( &nAttrCount );

    
    /* OK, now, as often with RPF/CADRG, here is the necessary dirty hack */
    /* -- Begin of lengthy explanation -- */
    /* A lot of CADRG files have a nASSSize value that reports a size */
    /* smaller than the genuine size of the attribute subsection in the */
    /* file, so if we trust the nASSSize value, we'll reject existing */
    /* attributes. This is for example the case for */
    /* http://download.osgeo.org/gdal/data/nitf/0000M033.GN3 */
    /* where nASSSize is reported to be 302 bytes for 52 attributes (which */
    /* is odd since 52 * 8 < 302), but a binary inspection of the attribute */
    /* subsection shows that the actual size is 608 bytes, which is also confirmed*/
    /* by the fact that the next subsection (quite often LID_ExplicitArealCoverageTable but not always) */
    /* begins right after. So if this next subsection is found and that the */
    /* difference in offset is larger than the original nASSSize, use it. */
    /* I have observed that nowhere in the NITF driver we make use of the .nLocSize field */
    /* -- End of lengthy explanation -- */

    if (nNextOffset > 0 && nNextOffset - nASSOffset > nASSSize)
        nASSSize = nNextOffset - nASSOffset;

    /* Be sure that the attribute subsection is large enough to hold the */
    /* offset table (otherwise NITFFetchAttribute coud read out of the buffer) */
    if (nASSSize < 8 * nAttrCount)
    {
        CPLError( CE_Warning, CPLE_AppDefined,
                  "Attribute subsection not large enough (%d bytes) to contain %d attributes.",
                  nASSSize, nAttrCount );
        return;
    }

/* -------------------------------------------------------------------- */
/*      Load the attribute table.                                       */
/* -------------------------------------------------------------------- */
    pabyAttributeSubsection = (GByte *) VSIMalloc(nASSSize);
    if( pabyAttributeSubsection == NULL )
    {
        CPLError( CE_Warning, CPLE_AppDefined,
                  "Out of memory failure reading %d bytes of attribute subsection. ",
                  nASSSize );
        return;
    }

    VSIFSeekL( psImage->psFile->fp, nASSOffset, SEEK_SET );
    VSIFReadL( pabyAttributeSubsection, nASSSize, 1, psImage->psFile->fp );

/* -------------------------------------------------------------------- */
/*      Scan for some particular attributes we would like.              */
/* -------------------------------------------------------------------- */
    if( NITFFetchAttribute( pabyAttributeSubsection, nASSSize, nAttrCount,
                            1, 1, 8, abyBuffer ) )
        NITFExtractMetadata( &(psImage->papszMetadata), (char*)abyBuffer, 0, 8, 
                             "NITF_RPF_CurrencyDate" );
    if( NITFFetchAttribute( pabyAttributeSubsection, nASSSize, nAttrCount,
                            2, 1, 8, abyBuffer ) )
        NITFExtractMetadata( &(psImage->papszMetadata), (char*)abyBuffer, 0, 8, 
                             "NITF_RPF_ProductionDate" );
    if( NITFFetchAttribute( pabyAttributeSubsection, nASSSize, nAttrCount,
                            3, 1, 8, abyBuffer ) )
        NITFExtractMetadata( &(psImage->papszMetadata), (char*)abyBuffer, 0, 8, 
                             "NITF_RPF_SignificantDate" );

    CPLFree( pabyAttributeSubsection );
}

/************************************************************************/
/*                       NITFLoadColormapSubSection()                   */
/************************************************************************/

/* This function is directly inspired by function parse_clut coming from ogdi/driver/rpf/utils.c
   and placed under the following copyright */

/*
 ******************************************************************************
 * Copyright (C) 1995 Logiciels et Applications Scientifiques (L.A.S.) Inc
 * Permission to use, copy, modify and distribute this software and
 * its documentation for any purpose and without fee is hereby granted,
 * provided that the above copyright notice appear in all copies, that
 * both the copyright notice and this permission notice appear in
 * supporting documentation, and that the name of L.A.S. Inc not be used 
 * in advertising or publicity pertaining to distribution of the software 
 * without specific, written prior permission. L.A.S. Inc. makes no
 * representations about the suitability of this software for any purpose.
 * It is provided "as is" without express or implied warranty.
 ******************************************************************************
 */


static void NITFLoadColormapSubSection( NITFImage *psImage )
{
    int nLocBaseColorGrayscaleSection = 0;
    int nLocBaseColormapSubSection = 0;
    int colorGrayscaleSectionSize = 0;
    int colormapSubSectionSize = 0;
    NITFFile *psFile = psImage->psFile;
    unsigned int i, j;
    unsigned char nOffsetRecs;
    NITFColormapRecord* colormapRecords;
    unsigned int colormapOffsetTableOffset;
    unsigned short offsetRecLen;
    
    NITFBandInfo *psBandInfo = psImage->pasBandInfo;
  
    for( i = 0; (int)i < psImage->nLocCount; i++ )
    {
        if( psImage->pasLocations[i].nLocId == LID_ColorGrayscaleSectionSubheader )
        {
            nLocBaseColorGrayscaleSection = psImage->pasLocations[i].nLocOffset;
            colorGrayscaleSectionSize = psImage->pasLocations[i].nLocSize;
        }
        else if( psImage->pasLocations[i].nLocId == LID_ColormapSubsection )
        {
            nLocBaseColormapSubSection = psImage->pasLocations[i].nLocOffset;
            colormapSubSectionSize = psImage->pasLocations[i].nLocSize;
        }
    }
    if (nLocBaseColorGrayscaleSection == 0)
    {
        //fprintf(stderr, "nLocBaseColorGrayscaleSection == 0\n");
        return;
    }
    if (nLocBaseColormapSubSection == 0)
    {
        //fprintf(stderr, "nLocBaseColormapSubSection == 0\n");
        return;
    }
    
    if( VSIFSeekL( psFile->fp, nLocBaseColorGrayscaleSection, 
                  SEEK_SET ) != 0 )
    {
        CPLError( CE_Failure, CPLE_FileIO, 
                  "Failed to seek to %d.",
                  nLocBaseColorGrayscaleSection );
        return;
    }
    
    
    VSIFReadL( &nOffsetRecs, 1, 1, psFile->fp );
    
    if( VSIFSeekL( psFile->fp, nLocBaseColormapSubSection, 
                  SEEK_SET ) != 0  )
    {
        CPLError( CE_Failure, CPLE_FileIO, 
                  "Failed to seek to %d.",
                  nLocBaseColormapSubSection );
        return;
    }
    
    colormapRecords = (NITFColormapRecord*)CPLMalloc(nOffsetRecs * sizeof(NITFColormapRecord));

     /* colormap offset table offset length */
    VSIFReadL( &colormapOffsetTableOffset, 1, sizeof(colormapOffsetTableOffset),  psFile->fp );
    CPL_MSBPTR32( &colormapOffsetTableOffset );

     /* offset record length */
    VSIFReadL( &offsetRecLen, 1, sizeof(offsetRecLen),  psFile->fp );
    CPL_MSBPTR16( &offsetRecLen );
    
    for (i = 0; i < nOffsetRecs; i++)
    {
        VSIFReadL( &colormapRecords[i].tableId, 1, sizeof(colormapRecords[i].tableId),  psFile->fp );
        CPL_MSBPTR16( &colormapRecords[i].tableId );
        
        VSIFReadL( &colormapRecords[i].nRecords, 1, sizeof(colormapRecords[i].nRecords),  psFile->fp );
        CPL_MSBPTR32( &colormapRecords[i].nRecords );
        
        VSIFReadL( &colormapRecords[i].elementLength, 1, sizeof(colormapRecords[i].elementLength),  psFile->fp );
    
        VSIFReadL( &colormapRecords[i].histogramRecordLength, 1, sizeof(colormapRecords[i].histogramRecordLength),  psFile->fp );
        CPL_MSBPTR16( &colormapRecords[i].histogramRecordLength );
    
        VSIFReadL( &colormapRecords[i].colorTableOffset, 1, sizeof(colormapRecords[i].colorTableOffset),  psFile->fp );
        CPL_MSBPTR32( &colormapRecords[i].colorTableOffset );
    
        VSIFReadL( &colormapRecords[i].histogramTableOffset, 1, sizeof(colormapRecords[i].histogramTableOffset),  psFile->fp );
        CPL_MSBPTR32( &colormapRecords[i].histogramTableOffset );
    }
    
    for (i=0; i<nOffsetRecs; i++)
    {
        if( VSIFSeekL( psFile->fp, nLocBaseColormapSubSection + colormapRecords[i].colorTableOffset, 
                    SEEK_SET ) != 0  )
        {
            CPLError( CE_Failure, CPLE_FileIO, 
                    "Failed to seek to %d.",
                    nLocBaseColormapSubSection + colormapRecords[i].colorTableOffset );
            CPLFree(colormapRecords);
            return;
        }
        
        /* This test is very CADRG specific. See MIL-C-89038, paragraph 3.12.5.a */
        if (i == 0 &&
            colormapRecords[i].tableId == 2 &&
            colormapRecords[i].elementLength == 4 &&
            colormapRecords[i].nRecords == 216)   /* read, use colortable */
        {
            GByte* rgbm = (GByte*)CPLMalloc(colormapRecords[i].nRecords * 4);
            if (VSIFReadL(rgbm, 1, colormapRecords[i].nRecords * 4, 
                     psFile->fp ) != colormapRecords[i].nRecords * 4 )
            {
                CPLError( CE_Failure, CPLE_FileIO, 
                          "Failed to read %d byte rgbm.",
                           colormapRecords[i].nRecords * 4);
                CPLFree(rgbm);
                CPLFree(colormapRecords);
                return;
            }
            for (j = 0; j < colormapRecords[i].nRecords; j++)
            {
                psBandInfo->pabyLUT[j] = rgbm[4*j];
                psBandInfo->pabyLUT[j+256] = rgbm[4*j+1];
                psBandInfo->pabyLUT[j+512] = rgbm[4*j+2];
            }
            CPLFree(rgbm);
        }
    } 

    CPLFree(colormapRecords);
}


/************************************************************************/
/*                       NITFLoadSubframeMaskTable()                        */
/************************************************************************/

/* Fixes bug #913 */
static void NITFLoadSubframeMaskTable( NITFImage *psImage )
{
    int i;
    NITFFile *psFile = psImage->psFile;
    NITFSegmentInfo *psSegInfo = psFile->pasSegmentInfo + psImage->iSegment;
    GUIntBig  nLocBaseSpatialDataSubsection = psSegInfo->nSegmentStart;
    GUInt32  nLocBaseMaskSubsection = 0;
    GUInt16 subframeSequenceRecordLength, transparencySequenceRecordLength, transparencyOutputPixelCodeLength;

    for( i = 0; i < psImage->nLocCount; i++ )
    {
        if( psImage->pasLocations[i].nLocId == LID_SpatialDataSubsection )
        {
            nLocBaseSpatialDataSubsection = psImage->pasLocations[i].nLocOffset;
        }
        else if( psImage->pasLocations[i].nLocId == LID_MaskSubsection )
        {
            nLocBaseMaskSubsection = psImage->pasLocations[i].nLocOffset;
        }
    }
    if (nLocBaseMaskSubsection == 0)
    {
        //fprintf(stderr, "nLocBase(LID_MaskSubsection) == 0\n");
        return;
    }
    
    //fprintf(stderr, "nLocBaseMaskSubsection = %d\n", nLocBaseMaskSubsection);
    if( VSIFSeekL( psFile->fp, nLocBaseMaskSubsection, 
                    SEEK_SET ) != 0  )
    {
        CPLError( CE_Failure, CPLE_FileIO, 
                "Failed to seek to %d.",
                nLocBaseMaskSubsection );
        return;
    }
    
    VSIFReadL( &subframeSequenceRecordLength, 1, sizeof(subframeSequenceRecordLength),  psFile->fp );
    CPL_MSBPTR16( &subframeSequenceRecordLength );
    
    VSIFReadL( &transparencySequenceRecordLength, 1, sizeof(transparencySequenceRecordLength),  psFile->fp );
    CPL_MSBPTR16( &transparencySequenceRecordLength );
    
    /* in bits */
    VSIFReadL( &transparencyOutputPixelCodeLength, 1, sizeof(transparencyOutputPixelCodeLength),  psFile->fp );
    CPL_MSBPTR16( &transparencyOutputPixelCodeLength );

    //fprintf(stderr, "transparencyOutputPixelCodeLength=%d\n", transparencyOutputPixelCodeLength);

    if( transparencyOutputPixelCodeLength == 8 )
    {
      GByte byNodata;

      psImage->bNoDataSet = TRUE;
      VSIFReadL( &byNodata, 1, 1, psFile->fp );
      psImage->nNoDataValue = byNodata;
    }
    else
    {
      VSIFSeekL( psFile->fp, (transparencyOutputPixelCodeLength+7)/8, SEEK_CUR );
    }

    /* Fix for rpf/cjnc/cjncz01/0001f023.jn1 */
    if (subframeSequenceRecordLength != 4)
    {
      //fprintf(stderr, "subframeSequenceRecordLength=%d\n", subframeSequenceRecordLength);
      return;
    }

    for( i=0; i < psImage->nBlocksPerRow * psImage->nBlocksPerColumn; i++ )
    {
        unsigned int offset;
        VSIFReadL( &offset, 1, sizeof(offset),  psFile->fp );
        CPL_MSBPTR32( &offset );
        //fprintf(stderr, "%d : %d\n", i, offset);
        if (offset == 0xffffffff)
            psImage->panBlockStart[i] = 0xffffffff;
        else
            psImage->panBlockStart[i] = nLocBaseSpatialDataSubsection + offset;
    }
}


/************************************************************************/
/*                       NITFLoadLocationTable()                        */
/************************************************************************/

static void NITFLoadLocationTable( NITFImage *psImage )

{
/* -------------------------------------------------------------------- */
/*      Get the location table out of the RPFIMG TRE on the image.      */
/* -------------------------------------------------------------------- */
    GUInt16  nLocCount;
    int      iLoc;
    const char *pszTRE;
    int nHeaderOffset = 0;
    int i;
    int nRemainingBytes;

    pszTRE = NITFFindTRE(psImage->pachTRE, psImage->nTREBytes, "RPFIMG", &nRemainingBytes);
    if( pszTRE == NULL )
        return;

    pszTRE += 6;
    nRemainingBytes -= 6;

    if (nRemainingBytes < 2)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Cannot read location table. Not enough bytes");
        return;
    }

    memcpy( &nLocCount, pszTRE, 2 );
    nLocCount = CPL_MSBWORD16( nLocCount );

    psImage->nLocCount = nLocCount;
    if (psImage->nLocCount == 0)
        return;

    psImage->pasLocations = (NITFLocation *) 
        VSICalloc(sizeof(NITFLocation), nLocCount);
    if (psImage->pasLocations == NULL)
    {
        CPLError(CE_Failure, CPLE_OutOfMemory,
                 "Cannot allocate memory for location table");
        psImage->nLocCount = 0;
        return;
    }

    pszTRE += 8;
    nRemainingBytes -= 8;

    if (nRemainingBytes < nLocCount * 10)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Cannot read location table. Not enough bytes");
        CPLFree(psImage->pasLocations);
        psImage->pasLocations = NULL;
        psImage->nLocCount = 0;
        return;
    }

/* -------------------------------------------------------------------- */
/*      Process the locations.                                          */
/* -------------------------------------------------------------------- */
    
    for( iLoc = 0; iLoc < nLocCount; iLoc++ )
    {
        unsigned char *pabyEntry = (unsigned char *) pszTRE;
        pszTRE += 10;
        nRemainingBytes -= 10;

        psImage->pasLocations[iLoc].nLocId = pabyEntry[0] * 256 + pabyEntry[1];

        CPL_MSBPTR32( pabyEntry + 2 );
        memcpy( &(psImage->pasLocations[iLoc].nLocSize), pabyEntry + 2, 4 );

        CPL_MSBPTR32( pabyEntry + 6 );
        memcpy( &(psImage->pasLocations[iLoc].nLocOffset), pabyEntry + 6, 4 );
    }

/* -------------------------------------------------------------------- */
/*      It seems that sometimes (at least for bug #1313 and #1714)      */
/*      the RPF headers are improperly placed.  We check by looking     */
/*      to see if the RPFHDR is where it should be.  If not, we         */
/*      disregard the location table.                                   */
/*                                                                      */
/*      The NITF21_CGM_ANNO_Uncompressed_unmasked.ntf sample data       */
/*      file (see gdal data downloads) is an example of this.           */
/* -------------------------------------------------------------------- */
    for( i = 0; i < psImage->nLocCount; i++ )
    {
        if( psImage->pasLocations[i].nLocId == LID_HeaderComponent )
        {
            nHeaderOffset = psImage->pasLocations[i].nLocOffset;
            break;
        }
    }

    if( nHeaderOffset != 0 )
    {
        char achHeaderChunk[1000];

        VSIFSeekL( psImage->psFile->fp, nHeaderOffset - 11, SEEK_SET );
        VSIFReadL( achHeaderChunk, 1, sizeof(achHeaderChunk), 
                   psImage->psFile->fp );

        if( !EQUALN(achHeaderChunk,"RPFHDR",6) )
        {
            CPLError( CE_Warning, CPLE_AppDefined,
                      "Ignoring NITF RPF Location table since it seems to be corrupt." );
            CPLFree( psImage->pasLocations );
            psImage->pasLocations = NULL;
            psImage->nLocCount = 0;
        }
    }
}

/************************************************************************/
/*                          NITFLoadVQTables()                          */
/************************************************************************/

static int NITFLoadVQTables( NITFImage *psImage )

{
    int     i, nVQOffset=0, nVQSize=0;
    GByte abyTestChunk[1000];
    GByte abySignature[6];

/* -------------------------------------------------------------------- */
/*      Do we already have the VQ tables?                               */
/* -------------------------------------------------------------------- */
    if( psImage->apanVQLUT[0] != NULL )
        return TRUE;

/* -------------------------------------------------------------------- */
/*      Do we have the location information?                            */
/* -------------------------------------------------------------------- */
    for( i = 0; i < psImage->nLocCount; i++ )
    {
        if( psImage->pasLocations[i].nLocId == LID_CompressionLookupSubsection)
        {
            nVQOffset = psImage->pasLocations[i].nLocOffset;
            nVQSize = psImage->pasLocations[i].nLocSize;
        }
    }

    if( nVQOffset == 0 )
        return FALSE;

/* -------------------------------------------------------------------- */
/*      Does it look like we have the tables properly identified?       */
/* -------------------------------------------------------------------- */
    abySignature[0] = 0x00;
    abySignature[1] = 0x00;
    abySignature[2] = 0x00;
    abySignature[3] = 0x06;
    abySignature[4] = 0x00;
    abySignature[5] = 0x0E;

    VSIFSeekL( psImage->psFile->fp, nVQOffset, SEEK_SET );
    VSIFReadL( abyTestChunk, 1, sizeof(abyTestChunk), psImage->psFile->fp );

    if( memcmp(abyTestChunk,abySignature,sizeof(abySignature)) != 0 )
    {
        for( i = 0; i < sizeof(abyTestChunk) - sizeof(abySignature); i++ )
        {
            if( memcmp(abyTestChunk+i,abySignature,sizeof(abySignature)) == 0 )
            {
                nVQOffset += i; 
                CPLDebug( "NITF", 
                          "VQ CompressionLookupSubsection offsets off by %d bytes, adjusting accordingly.",
                          i );
                break;
            }
        }
    }
    
/* -------------------------------------------------------------------- */
/*      Load the tables.                                                */
/* -------------------------------------------------------------------- */
    for( i = 0; i < 4; i++ )
    {
        GUInt32 nVQVector;

        psImage->apanVQLUT[i] = (GUInt32 *) CPLCalloc(4096,sizeof(GUInt32));

        VSIFSeekL( psImage->psFile->fp, nVQOffset + 6 + i*14 + 10, SEEK_SET );
        VSIFReadL( &nVQVector, 1, 4, psImage->psFile->fp );
        nVQVector = CPL_MSBWORD32( nVQVector );
        
        VSIFSeekL( psImage->psFile->fp, nVQOffset + nVQVector, SEEK_SET );
        VSIFReadL( psImage->apanVQLUT[i], 4, 4096, psImage->psFile->fp );
    }

    return TRUE;
}

/************************************************************************/
/*                           NITFReadSTDIDC()                           */
/*                                                                      */
/*      Read a STDIDC TRE and return contents as metadata strings.      */
/************************************************************************/

char **NITFReadSTDIDC( NITFImage *psImage )

{
    const char *pachTRE;
    int  nTRESize;
    char **papszMD = NULL;

/* -------------------------------------------------------------------- */
/*      Do we have the TRE?                                             */
/* -------------------------------------------------------------------- */
    pachTRE = NITFFindTRE( psImage->pachTRE, psImage->nTREBytes, 
                           "STDIDC", &nTRESize );

    if( pachTRE == NULL )
        return NULL;

    if( nTRESize != 89 )
    {
        CPLError( CE_Warning, CPLE_AppDefined, 
                  "STDIDC TRE wrong size, ignoring." );
        return NULL;
    }

/* -------------------------------------------------------------------- */
/*      Parse out field values.                                         */
/* -------------------------------------------------------------------- */
    NITFExtractMetadata( &papszMD, pachTRE,   0,  14, 
                         "NITF_STDIDC_ACQUISITION_DATE" );
    NITFExtractMetadata( &papszMD, pachTRE,  14,  14, 
                         "NITF_STDIDC_MISSION" );
    NITFExtractMetadata( &papszMD, pachTRE,  28,   2, 
                         "NITF_STDIDC_PASS" );
    NITFExtractMetadata( &papszMD, pachTRE,  30,   3, 
                         "NITF_STDIDC_OP_NUM" );
    NITFExtractMetadata( &papszMD, pachTRE,  33,   2, 
                         "NITF_STDIDC_START_SEGMENT" );
    NITFExtractMetadata( &papszMD, pachTRE,  35,   2, 
                         "NITF_STDIDC_REPRO_NUM" );
    NITFExtractMetadata( &papszMD, pachTRE,  37,   3, 
                         "NITF_STDIDC_REPLAY_REGEN" );
    /* reserved: 1 */
    NITFExtractMetadata( &papszMD, pachTRE,  41,   3, 
                         "NITF_STDIDC_START_COLUMN" );
    NITFExtractMetadata( &papszMD, pachTRE,  44,   5, 
                         "NITF_STDIDC_START_ROW" );
    NITFExtractMetadata( &papszMD, pachTRE,  49,   2, 
                         "NITF_STDIDC_END_SEGMENT" );
    NITFExtractMetadata( &papszMD, pachTRE,  51,   3, 
                         "NITF_STDIDC_END_COLUMN" );
    NITFExtractMetadata( &papszMD, pachTRE,  54,   5, 
                         "NITF_STDIDC_END_ROW" );
    NITFExtractMetadata( &papszMD, pachTRE,  59,   2, 
                         "NITF_STDIDC_COUNTRY" );
    NITFExtractMetadata( &papszMD, pachTRE,  61,   4, 
                         "NITF_STDIDC_WAC" );
    NITFExtractMetadata( &papszMD, pachTRE,  65,  11, 
                         "NITF_STDIDC_LOCATION" );
    /* reserved: 5+8 */

    return papszMD;
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

/************************************************************************/
/*                         NITFIHFieldOffset()                          */
/*                                                                      */
/*      Find the file offset for the beginning of a particular field    */
/*      in this image header.  Only implemented for selected fields.    */
/************************************************************************/

GUIntBig NITFIHFieldOffset( NITFImage *psImage, const char *pszFieldName )

{
    char szTemp[128];
    int nNICOM;
    GUIntBig nWrkOffset;
    GUIntBig nIMOffset =
        psImage->psFile->pasSegmentInfo[psImage->iSegment].nSegmentHeaderStart;

    // We only support files we created.
    if( !EQUALN(psImage->psFile->szVersion,"NITF02.1",8) )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "NITFIHFieldOffset() only works with NITF 2.1 images");
        return 0;
    }

    if( EQUAL(pszFieldName,"IM") )
        return nIMOffset;

    if( EQUAL(pszFieldName,"PJUST") )
        return nIMOffset + 370;

    if( EQUAL(pszFieldName,"ICORDS") )
        return nIMOffset + 371;

    if( EQUAL(pszFieldName,"IGEOLO") )
    {
        if( !psImage->bHaveIGEOLO )
            return 0;
        else
            return nIMOffset + 372;
    }

/* -------------------------------------------------------------------- */
/*      Keep working offset from here on in since everything else is    */
/*      variable.                                                       */
/* -------------------------------------------------------------------- */
    nWrkOffset = 372 + nIMOffset;

    if( psImage->bHaveIGEOLO )
        nWrkOffset += 60;

/* -------------------------------------------------------------------- */
/*      Comments.                                                       */
/* -------------------------------------------------------------------- */
    nNICOM = atoi(NITFGetField(szTemp,psImage->pachHeader,
                               (int)(nWrkOffset - nIMOffset),1));
        
    if( EQUAL(pszFieldName,"NICOM") )
        return nWrkOffset;
    
    nWrkOffset++;

    if( EQUAL(pszFieldName,"ICOM") )
        return nWrkOffset;

    nWrkOffset += 80 * nNICOM;

/* -------------------------------------------------------------------- */
/*      IC                                                              */
/* -------------------------------------------------------------------- */
    if( EQUAL(pszFieldName,"IC") )
        return nWrkOffset;

    nWrkOffset += 2;

/* -------------------------------------------------------------------- */
/*      COMRAT                                                          */
/* -------------------------------------------------------------------- */

    if( psImage->szIC[0] != 'N' )
    {
        if( EQUAL(pszFieldName,"COMRAT") )
            return nWrkOffset;
        nWrkOffset += 4;
    }

/* -------------------------------------------------------------------- */
/*      NBANDS                                                          */
/* -------------------------------------------------------------------- */
    if( EQUAL(pszFieldName,"NBANDS") )
        return nWrkOffset;

    nWrkOffset += 1;

/* -------------------------------------------------------------------- */
/*      XBANDS                                                          */
/* -------------------------------------------------------------------- */
    if( EQUAL(pszFieldName,"XBANDS") )
        return nWrkOffset;

    if( psImage->nBands > 9 )
        nWrkOffset += 5;

/* -------------------------------------------------------------------- */
/*      IREPBAND                                                        */
/* -------------------------------------------------------------------- */
    if( EQUAL(pszFieldName,"IREPBAND") )
        return nWrkOffset;

//    nWrkOffset += 2 * psImage->nBands;

    return 0;
}
