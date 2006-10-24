/******************************************************************************
 * $Id$
 *
 * Project:  NITF Read/Write Library
 * Purpose:  Simple test mainline to dump info about NITF file. 
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
 * Revision 1.10  2006/10/24 02:18:06  fwarmerdam
 * added image attachment metadata
 *
 * Revision 1.9  2006/10/13 02:53:48  fwarmerdam
 * various improvements to TRE and VQ LUT support for bug 1313
 *
 * Revision 1.8  2006/06/06 17:09:27  fwarmerdam
 * added various extra reporting
 *
 * Revision 1.7  2004/05/06 14:58:06  warmerda
 * added USE00A and STDIDC parsing and reporting as metadata
 *
 * Revision 1.6  2003/05/29 19:50:10  warmerda
 * added RPC (and more general TRE) reporting
 *
 * Revision 1.5  2002/12/18 21:18:38  warmerda
 * report corners more sensibly
 *
 * Revision 1.4  2002/12/17 05:26:26  warmerda
 * implement basic write support
 *
 * Revision 1.3  2002/12/03 18:07:40  warmerda
 * added VQLUT reporting
 *
 * Revision 1.2  2002/12/03 04:43:54  warmerda
 * lots of work
 *
 * Revision 1.1  2002/12/02 06:09:29  warmerda
 * New
 *
 */

#include "nitflib.h"
#include "cpl_string.h"

CPL_CVSID("$Id$");

static void DumpRPC( NITFImage *psImage, NITFRPC00BInfo *psRPC );
static void DumpMetadata( const char *, const char *, char ** );

/************************************************************************/
/*                                main()                                */
/************************************************************************/

int main( int nArgc, char ** papszArgv )

{
    NITFFile	*psFile;
    int          iSegment, iFile;
    char         szTemp[100];

    if( nArgc < 2 )
    {
        printf( "Usage: nitfdump <nitf_filename>*\n" );
        exit( 1 );
    }

/* ==================================================================== */
/*      Loop over all files.                                            */
/* ==================================================================== */
    for( iFile = 1; iFile < nArgc; iFile++ )
    {
/* -------------------------------------------------------------------- */
/*      Open the file.                                                  */
/* -------------------------------------------------------------------- */
        psFile = NITFOpen( papszArgv[iFile], FALSE );
        if( psFile == NULL )
            exit( 2 );

        printf( "Dump for %s\n", papszArgv[iFile] );

/* -------------------------------------------------------------------- */
/*      Dump first TRE list.                                            */
/* -------------------------------------------------------------------- */
        if( psFile->pachTRE != NULL )
        {
            int nTREBytes = psFile->nTREBytes;
            const char *pszTREData = psFile->pachTRE;


            printf( "File TREs:" );

            while( nTREBytes > 10 )
            {
                int nThisTRESize = atoi(NITFGetField(szTemp, pszTREData, 6, 5 ));

                printf( " %6.6s(%d)", pszTREData, nThisTRESize );
                pszTREData += nThisTRESize + 11;
                nTREBytes -= (nThisTRESize + 11);
            }
            printf( "\n" );
        }

/* -------------------------------------------------------------------- */
/*      Dump Metadata                                                   */
/* -------------------------------------------------------------------- */
        DumpMetadata( "File Metadata:", "  ", psFile->papszMetadata );

/* -------------------------------------------------------------------- */
/*      Dump general info about segments.                               */
/* -------------------------------------------------------------------- */
        for( iSegment = 0; iSegment < psFile->nSegmentCount; iSegment++ )
        {
            NITFSegmentInfo *psSegInfo = psFile->pasSegmentInfo + iSegment;

            printf( "Segment %d (Type=%s):\n", 
                    iSegment + 1, psSegInfo->szSegmentType );

            printf( "  HeaderStart=%d, HeaderSize=%d, DataStart=%d, DataSize=%d\n",
                    psSegInfo->nSegmentHeaderStart,
                    psSegInfo->nSegmentHeaderSize, 
                    psSegInfo->nSegmentStart,
                    psSegInfo->nSegmentSize );
            printf( "\n" );
        }

/* -------------------------------------------------------------------- */
/*      Report details of images.                                       */
/* -------------------------------------------------------------------- */
        for( iSegment = 0; iSegment < psFile->nSegmentCount; iSegment++ )
        {
            NITFSegmentInfo *psSegInfo = psFile->pasSegmentInfo + iSegment;
            NITFImage *psImage;
            NITFRPC00BInfo sRPCInfo;
            int iBand;
            char **papszMD;

            if( !EQUAL(psSegInfo->szSegmentType,"IM") )
                continue;
        
            psImage = NITFImageAccess( psFile, iSegment );
            if( psImage == NULL )
            {
                printf( "NITFAccessImage(%d) failed!\n", iSegment );
                continue;
            }

            printf( "Image Segment %d, %dPx%dLx%dB x %dbits:\n", 
                    iSegment, psImage->nRows, psImage->nCols, psImage->nBands,
                    psImage->nBitsPerSample );
            printf( "  PVTYPE=%s, IREP=%s, ICAT=%s, IMODE=%c, IC=%s, COMRAT=%s, ICORDS=%c\n", 
                    psImage->szPVType, psImage->szIREP, psImage->szICAT,
                    psImage->chIMODE, psImage->szIC, psImage->szCOMRAT,
                    psImage->chICORDS );
            if( psImage->chICORDS != ' ' )
            {
                printf( "  UL=(%g,%g), UR=(%g,%g)\n  LL=(%g,%g), LR=(%g,%g)\n", 
                        psImage->dfULX, psImage->dfULY,
                        psImage->dfURX, psImage->dfURY,
                        psImage->dfLLX, psImage->dfLLY,
                        psImage->dfLRX, psImage->dfLRY );
            }
            if( psImage->nILOCRow != 0 )
            {
                printf( "  IDLVL=%d, IALVL=%d, ILOC R=%d,C=%d, IMAG=%s\n",
                        psImage->nIDLVL, psImage->nIALVL, 
                        psImage->nILOCRow, psImage->nILOCColumn, 
                        psImage->szIMAG );
            }

            printf( "  %d x %d blocks of size %d x %d\n",
                    psImage->nBlocksPerRow, psImage->nBlocksPerColumn,
                    psImage->nBlockWidth, psImage->nBlockHeight );
        
            if( psImage->pachTRE != NULL )
            {
                int nTREBytes = psImage->nTREBytes;
                const char *pszTREData = psImage->pachTRE;
            
            
                printf( "  Image TREs:" );
            
                while( nTREBytes > 10 )
                {
                    int nThisTRESize = atoi(NITFGetField(szTemp, pszTREData, 6, 5 ));
                
                    printf( " %6.6s(%d)", pszTREData, nThisTRESize );
                    pszTREData += nThisTRESize + 11;
                    nTREBytes -= (nThisTRESize + 11);
                }
                printf( "\n" );
            }

            /* Report info from location table, if found.                  */
            if( psImage->nLocCount > 0 )
            {
                int i;
                printf( "  Location Table\n" );
                for( i = 0; i < psImage->nLocCount; i++ )
                {
                    printf( "    LocId=%d, Offset=%d, Size=%d\n", 
                            psImage->pasLocations[i].nLocId,
                            psImage->pasLocations[i].nLocOffset,
                            psImage->pasLocations[i].nLocSize );
                }
                printf( "\n" );
            }

            if( strlen(psImage->pszComments) > 0 )
                printf( "  Comments:\n%s\n", psImage->pszComments );

            for( iBand = 0; iBand < psImage->nBands; iBand++ )
            {
                NITFBandInfo *psBandInfo = psImage->pasBandInfo + iBand;

                printf( "  Band %d: IREPBAND=%s, ISUBCAT=%s, %d LUT entries.\n",
                        iBand + 1, psBandInfo->szIREPBAND, psBandInfo->szISUBCAT,
                        psBandInfo->nSignificantLUTEntries );
            }

            if( NITFReadRPC00B( psImage, &sRPCInfo ) )
            {
                DumpRPC( psImage, &sRPCInfo );
            }

            papszMD = NITFReadUSE00A( psImage );
            if( papszMD != NULL )
            {
                DumpMetadata( "  USE00A TRE:", "    ", papszMD );
                CSLDestroy( papszMD );
            }

            papszMD = NITFReadSTDIDC( psImage );
            if( papszMD != NULL )
            {
                DumpMetadata( "  STDIDC TRE:", "    ", papszMD );
                CSLDestroy( papszMD );
            }

            DumpMetadata( "  Image Metadata:", "    ", psImage->papszMetadata );
        }

/* -------------------------------------------------------------------- */
/*      Close.                                                          */
/* -------------------------------------------------------------------- */
        NITFClose( psFile );
    }

    exit( 0 );
}

/************************************************************************/
/*                            DumpMetadata()                            */
/************************************************************************/

static void DumpMetadata( const char *pszTitle, const char *pszPrefix, 
                          char ** papszMD )
{
    int i;

    if( papszMD == NULL )
        return;

    printf( "%s\n", pszTitle );

    for( i = 0; papszMD[i] != NULL; i++ )
        printf( "%s%s\n", pszPrefix, papszMD[i] );
}

/************************************************************************/
/*                              DumpRPC()                               */
/************************************************************************/

static void DumpRPC( NITFImage *psImage, NITFRPC00BInfo *psRPC )

{
    int  i;

    printf( "  RPC00B:\n" );
    printf( "    SUCCESS=%d\n", psRPC->SUCCESS );
    printf( "    ERR_BIAS=%.16g\n", psRPC->ERR_BIAS );
    printf( "    ERR_RAND=%.16g\n", psRPC->ERR_RAND );

    printf( "    LINE_OFF=%.16g\n", psRPC->LINE_OFF );
    printf( "    SAMP_OFF=%.16g\n", psRPC->SAMP_OFF );
    printf( "    LAT_OFF =%.16g\n", psRPC->LAT_OFF );
    printf( "    LONG_OFF=%.16g\n", psRPC->LONG_OFF );
    printf( "    HEIGHT_OFF=%.16g\n", psRPC->HEIGHT_OFF );

    printf( "    LINE_SCALE=%.16g\n", psRPC->LINE_SCALE );
    printf( "    SAMP_SCALE=%.16g\n", psRPC->SAMP_SCALE );
    printf( "    LAT_SCALE =%.16g\n", psRPC->LAT_SCALE );
    printf( "    LONG_SCALE=%.16g\n", psRPC->LONG_SCALE );
    printf( "    HEIGHT_SCALE=%.16g\n", psRPC->HEIGHT_SCALE );

    printf( "    LINE_NUM_COEFF = " );
    for( i=0; i < 20; i++ )
    {
        printf( "%.12g ", psRPC->LINE_NUM_COEFF[i] );

        if( i == 19 )
            printf( "\n" );
        else if( (i%5) == 4  )
            printf( "\n                     " );
    }
    
    printf( "    LINE_DEN_COEFF = " );
    for( i=0; i < 20; i++ )
    {
        printf( "%.12g ", psRPC->LINE_DEN_COEFF[i] );

        if( i == 19 )
            printf( "\n" );
        else if( (i%5) == 4  )
            printf( "\n                     " );
    }
    
    printf( "    SAMP_NUM_COEFF = " );
    for( i=0; i < 20; i++ )
    {
        printf( "%.12g ", psRPC->SAMP_NUM_COEFF[i] );

        if( i == 19 )
            printf( "\n" );
        else if( (i%5) == 4  )
            printf( "\n                     " );
    }
    
    printf( "    SAMP_DEN_COEFF = " );
    for( i=0; i < 20; i++ )
    {
        printf( "%.12g ", psRPC->SAMP_DEN_COEFF[i] );

        if( i == 19 )
            printf( "\n" );
        else if( (i%5) == 4  )
            printf( "\n                     " );
    }

/* -------------------------------------------------------------------- */
/*      Dump some known locations.                                      */
/* -------------------------------------------------------------------- */
    {
        double adfLong[] = { psImage->dfULX, psImage->dfURX, 
                             psImage->dfLLX, psImage->dfLRX, 
                             (psImage->dfULX + psImage->dfLRX) / 2,
                             (psImage->dfULX + psImage->dfLRX) / 2 };
        double adfLat[] = { psImage->dfULY, psImage->dfURY, 
                            psImage->dfLLY, psImage->dfLRY, 
                            (psImage->dfULY + psImage->dfLRY) / 2,
                            (psImage->dfULY + psImage->dfLRY) / 2 };
        double adfHeight[] = { 0.0, 0.0, 0.0, 0.0, 0.0, 300.0 };
        double dfPixel, dfLine;
        
        for( i = 0; i < sizeof(adfLong) / sizeof(double); i++ )
        {
            NITFRPCGeoToImage( psRPC, adfLong[i], adfLat[i], adfHeight[i], 
                               &dfPixel, &dfLine );
            
            printf( "    RPC Transform (%.12g,%.12g,%g) -> (%g,%g)\n", 
                    adfLong[i], adfLat[i], adfHeight[i], dfPixel, dfLine );
        }
    }
}
