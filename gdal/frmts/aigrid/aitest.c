/******************************************************************************
 * $Id$
 *
 * Project:  Arc/Info Binary Grid Translator
 * Purpose:  Test mainline for examining AIGrid files.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 1999, Frank Warmerdam
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
 *****************************************************************************/

#include "aigrid.h"

CPL_CVSID("$Id$")

/************************************************************************/
/*                             DumpMagic()                              */
/*                                                                      */
/*      Dump the magic ``block type byte'' for each existing block.     */
/************************************************************************/

static void DumpMagic( AIGInfo_t * psInfo, int bVerbose )

{
    int		i;
    AIGTileInfo *psTInfo = psInfo->pasTileInfo + 0;

    for( i = 0; i < psTInfo->nBlocks; i++ )
    {
        GByte	byMagic;
        int	bReport = bVerbose;
        unsigned char abyBlockSize[2];
        const char *pszMessage = "";

        if( psTInfo->panBlockSize[i] == 0 )
            continue;

        VSIFSeekL( psTInfo->fpGrid, psTInfo->panBlockOffset[i], SEEK_SET );
        VSIFReadL( abyBlockSize, 2, 1, psTInfo->fpGrid );

        if( psInfo->nCellType == AIG_CELLTYPE_INT && psInfo->bCompressed )
        {
            VSIFReadL( &byMagic, 1, 1, psTInfo->fpGrid );

            if( byMagic != 0 && byMagic != 0x43 && byMagic != 0x04
                && byMagic != 0x08 && byMagic != 0x10 && byMagic != 0xd7
                && byMagic != 0xdf && byMagic != 0xe0 && byMagic != 0xfc
                && byMagic != 0xf8 && byMagic != 0xff && byMagic != 0x41
                && byMagic != 0x40 && byMagic != 0x42 && byMagic != 0xf0
                && byMagic != 0xcf && byMagic != 0x01 )
            {
                pszMessage = "(unhandled magic number)";
                bReport = TRUE;
            }

            if( byMagic == 0 && psTInfo->panBlockSize[i] > 8 )
            {
                pszMessage = "(wrong size for 0x00 block, should be 8 bytes)";
                bReport = TRUE;
            }

            if( (abyBlockSize[0] * 256 + abyBlockSize[1])*2 !=
                psTInfo->panBlockSize[i] )
            {
                pszMessage = "(block size in data doesn't match index)";
                bReport = TRUE;
            }
        }
        else
        {
            if( psTInfo->panBlockSize[i] !=
                psInfo->nBlockXSize*psInfo->nBlockYSize*sizeof(float) )
            {
                pszMessage = "(floating point block size is wrong)";
                bReport = TRUE;
            }
        }

        if( bReport )
        {
            printf( " %02x %5d %5d @ %u %s\n", byMagic, i,
                    psTInfo->panBlockSize[i],
                    psTInfo->panBlockOffset[i],
                    pszMessage );
        }
    }
}


/************************************************************************/
/*                                main()                                */
/************************************************************************/

int main( int argc, char ** argv )

{
    AIGInfo_t	*psInfo;
    GInt32 	*panRaster;
    int		i, j;
    int		bMagic = FALSE, bSuppressMagic = FALSE;
    int         iTestTileX = 0, iTestTileY = 0;

/* -------------------------------------------------------------------- */
/*      Process arguments.                                              */
/* -------------------------------------------------------------------- */
    while( argc > 1 && argv[1][0] == '-' )
    {
        if( EQUAL(argv[1],"-magic") )
            bMagic = TRUE;

        else if( EQUAL(argv[1],"-nomagic") )
            bSuppressMagic = TRUE;

        else if( EQUAL(argv[1],"-t") && argc > 2 )
        {
            iTestTileX = atoi(argv[2]);
            iTestTileY = atoi(argv[3]);
            argc -= 2;
            argv += 2;
        }

        argc--;
        argv++;
    }

    if( argc < 2 ) {
        printf( "Usage: aitest [-magic] coverage [block numbers...]\n" );
        exit( 1 );
    }

/* -------------------------------------------------------------------- */
/*      Open dataset.                                                   */
/* -------------------------------------------------------------------- */
    psInfo = AIGOpen( argv[1], "r" );
    if( psInfo == NULL )
        exit( 1 );

    AIGAccessTile( psInfo, iTestTileX, iTestTileY );

/* -------------------------------------------------------------------- */
/*      Dump general information                                        */
/* -------------------------------------------------------------------- */
    printf( "%d pixels x %d lines.\n", psInfo->nPixels, psInfo->nLines );
    printf( "Lower Left = (%f,%f)   Upper Right = (%f,%f)\n",
            psInfo->dfLLX,
            psInfo->dfLLY,
            psInfo->dfURX,
            psInfo->dfURY );

    if( psInfo->nCellType == AIG_CELLTYPE_INT )
        printf( "%s Integer coverage, %dx%d blocks.\n",
                psInfo->bCompressed ? "Compressed" : "Uncompressed",
                psInfo->nBlockXSize, psInfo->nBlockYSize );
    else
        printf( "%s Floating point coverage, %dx%d blocks.\n",
                psInfo->bCompressed ? "Compressed" : "Uncompressed",
                psInfo->nBlockXSize, psInfo->nBlockYSize );

    printf( "Stats - Min=%f, Max=%f, Mean=%f, StdDev=%f\n",
            psInfo->dfMin,
            psInfo->dfMax,
            psInfo->dfMean,
            psInfo->dfStdDev );

/* -------------------------------------------------------------------- */
/*      Do we want a dump of all the ``magic'' numbers for              */
/*      instantiated blocks?                                            */
/* -------------------------------------------------------------------- */
    if( !bSuppressMagic )
        DumpMagic( psInfo, bMagic );

/* -------------------------------------------------------------------- */
/*      Read a block, and report its contents.                          */
/* -------------------------------------------------------------------- */
    panRaster = (GInt32 *)
        CPLMalloc(psInfo->nBlockXSize * psInfo->nBlockYSize * 4);

    while( argc > 2 && (atoi(argv[2]) > 0 || argv[2][0] == '0') )
    {
        int	nBlock = atoi(argv[2]);
        CPLErr  eErr;
        AIGTileInfo *psTInfo = psInfo->pasTileInfo + 0;

        argv++;
        argc--;

        eErr = AIGReadBlock( psTInfo->fpGrid,
                             psTInfo->panBlockOffset[nBlock],
                             psTInfo->panBlockSize[nBlock],
                             psInfo->nBlockXSize, psInfo->nBlockYSize,
                             panRaster, psInfo->nCellType, psInfo->bCompressed);

        printf( "\nBlock %d:\n", nBlock );

        if( eErr != CE_None )
        {
            printf( "  Error! Skipping block.\n" );
            continue;
        }

        for( j = 0; j < psInfo->nBlockYSize; j++ )
        {
            for( i = 0; i < psInfo->nBlockXSize; i++ )
            {
                if( i > 18 )
                {
                    printf( "..." );
                    break;
                }

                if( panRaster[i+j*psInfo->nBlockXSize] == ESRI_GRID_NO_DATA )
                    printf( "-*- " );
                else if( psInfo->nCellType == AIG_CELLTYPE_FLOAT )
                    printf( "%f ",
                            ((float *) panRaster)[i+j*psInfo->nBlockXSize] );
                else
                    printf( "%3d ", panRaster[i+j*psInfo->nBlockXSize] );
            }
            printf( "\n" );
        }
    }

    CPLFree( panRaster );

    AIGClose( psInfo );

    exit( 0 );
}
