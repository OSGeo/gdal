/******************************************************************************
 * $Id$
 *
 * Project:  Arc/Info Binary Grid Translator
 * Purpose:  Grid file reading code.
 * Author:   Frank Warmerdam, warmerda@home.com
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
 ******************************************************************************
 *
 * $Log$
 * Revision 1.1  1999/02/03 14:12:56  warmerda
 * New
 *
 */

#include "aigrid.h"

/************************************************************************/
/*                       AIGProcessRawBlock()                           */
/*                                                                      */
/*      Process a block using ``08'' raw format.			*/
/************************************************************************/

static 
CPLErr AIGProcessRawBlock( GByte *pabyRaw, int nBlockSize,
                        int nBlockXSize, int nBlockYSize, GUInt32 * panData )

{
    GByte	*pabyCur;
    int		nMinSize, nMin, i;

/* -------------------------------------------------------------------- */
/*      Collect minimum value.                                          */
/* -------------------------------------------------------------------- */
    pabyCur = pabyRaw + 2;
    nMinSize = pabyCur[1];
    pabyCur += 2;

    nMin = 0;
    for( i = 0; i < nMinSize; i++ )
    {
        nMin = nMin * 256 + *pabyCur;
        pabyCur++;
    }
    
    CPLAssert( nBlockSize >= nBlockXSize*nBlockYSize + 4 + nMinSize );
    
/* -------------------------------------------------------------------- */
/*      Collect raw data.                                               */
/* -------------------------------------------------------------------- */
    for( i = 0; i < nBlockXSize * nBlockYSize; i++ )
    {
        panData[i] = *(pabyCur++) + nMin;
    }

    return( CE_None );
}

/************************************************************************/
/*                         AIGProcessD7Block()                          */
/*                                                                      */
/*      Process a block using ``D7'' compression.                       */
/************************************************************************/

static 
CPLErr AIGProcessBlock( GByte *pabyRaw, int nBlockSize,
                        int nBlockXSize, int nBlockYSize, GUInt32 * panData )

{
    GByte	*pabyCur;
    int		nTotPixels, nPixels;
    int		nMinSize, nMin, i;
    int		nTypeFlag = pabyRaw[2];

/* -------------------------------------------------------------------- */
/*      Collect block header info.                                      */
/* -------------------------------------------------------------------- */
    pabyCur = pabyRaw + 2;
    nMinSize = pabyCur[1];
    pabyCur += 2;

    nMin = 0;
    for( i = 0; i < nMinSize; i++ )
    {
        nMin = nMin * 256 + *pabyCur;
        if( i == 0 && nTypeFlag == 0xE0 )
            nMin &= 0x7f;

        pabyCur++;
    }
    
/* ==================================================================== */
/*     Process runs till we are done.                                  */
/* ==================================================================== */
    nTotPixels = nBlockXSize * nBlockYSize;
    nPixels = 0;

    while( nPixels < nTotPixels )
    {
        int	nMarker = *(pabyCur++);

/* -------------------------------------------------------------------- */
/*      Repeat data - four byte data block (0xE0)                       */
/* -------------------------------------------------------------------- */
        if( nTypeFlag == 0xE0 )
        {
            GUInt32	nValue;
            
            nValue = 0;
            
            for( i = 0; i < 4; i++ )
            {
                nValue = nValue * 256 + (*pabyCur++);
                
                if( i == 0 )
                    nValue &= 0x7f;
            }

            nValue += nMin;

            for( i = 0; i < nMarker; i++ )
                panData[nPixels++] += nValue;
        }
        
/* -------------------------------------------------------------------- */
/*      Repeat data - no actual data, just assign minimum (0xDF)        */
/* -------------------------------------------------------------------- */
        else if( nTypeFlag == 0xDF )
        {
            for( i = 0; i < nMarker; i++ )
                panData[nPixels++] += nMin;
        }
        
/* -------------------------------------------------------------------- */
/*      Literal data (0xD7)                                             */
/* -------------------------------------------------------------------- */
        else if( nMarker < 128 && nTypeFlag == 0xD7 )
        {
            while( nMarker > 0 )
            {
                panData[nPixels++] = *(pabyCur++) + nMin;
                nMarker--;
            }
        }

/* -------------------------------------------------------------------- */
/*      Nodata repeat                                                   */
/* -------------------------------------------------------------------- */
        else if( nMarker > 128 )
        {
            nMarker = 256 - nMarker;

            while( nMarker > 0 )
            {
                panData[nPixels++] = GRID_NO_DATA;
                nMarker--;
            }
        }

        else
        {
            CPLAssert( FALSE );
        }

        CPLAssert( (pabyCur - pabyRaw) <= (nBlockSize + 2) );
    }

    CPLAssert( nPixels <= nTotPixels );
    
    return CE_None;
}

/************************************************************************/
/*                            AIGReadBlock()                            */
/*                                                                      */
/*      Read a single block of integer grid data.                       */
/************************************************************************/

CPLErr AIGReadBlock( FILE * fp, int nBlockOffset, int nBlockSize,
                     int nBlockXSize, int nBlockYSize, GUInt32 *panData )

{
    GByte	*pabyRaw;
    CPLErr	eErr;
    int		i, nMagic;

/* -------------------------------------------------------------------- */
/*      If the block has zero size it is all dummies.                   */
/* -------------------------------------------------------------------- */
    if( nBlockSize == 0 )
    {
        for( i = 0; i < nBlockXSize * nBlockYSize; i++ )
            panData[i] = GRID_NO_DATA;

        return( CE_None );
    }
    
/* -------------------------------------------------------------------- */
/*      Read the block into memory.                                     */
/* -------------------------------------------------------------------- */
    pabyRaw = (GByte *) CPLMalloc(nBlockSize+2);
    VSIFSeek( fp, nBlockOffset, SEEK_SET );
    VSIFRead( pabyRaw, nBlockSize+2, 1, fp );

/* -------------------------------------------------------------------- */
/*      Verify the block size.                                          */
/* -------------------------------------------------------------------- */
    CPLAssert( nBlockSize == (pabyRaw[0]*256 + pabyRaw[1])*2 );

/* -------------------------------------------------------------------- */
/*      Is this a 0xD7 or 0xE0 block?                                   */
/* -------------------------------------------------------------------- */
    nMagic = pabyRaw[2];

    if( nMagic == 0x08 )
    {
        AIGProcessRawBlock( pabyRaw, nBlockSize, nBlockXSize, nBlockYSize,
                            panData );
    }
    else
    {
        eErr = AIGProcessBlock( pabyRaw, nBlockSize, 
                                nBlockXSize, nBlockYSize, panData );
    }

    
    CPLFree( pabyRaw );
    
    return CE_None;
}

/************************************************************************/
/*                           AIGReadHeader()                            */
/*                                                                      */
/*      Read the hdr.adf file, and populate the given info structure    */
/*      appropriately.                                                  */
/************************************************************************/

CPLErr AIGReadHeader( const char * pszCoverName, AIGInfo_t * psInfo )

{
    char	*pszHDRFilename;
    FILE	*fp;
    GByte	abyData[308];

/* -------------------------------------------------------------------- */
/*      Open the file hdr.adf file.                                     */
/* -------------------------------------------------------------------- */
    pszHDRFilename = (char *) CPLMalloc(strlen(pszCoverName)+30);
    sprintf( pszHDRFilename, "%s/hdr.adf", pszCoverName );

    fp = VSIFOpen( pszHDRFilename, "rb" );
    
    if( fp == NULL )
    {
        CPLError( CE_Failure, CPLE_OpenFailed,
                  "Failed to open grid header file:\n%s\n", pszHDRFilename );

        CPLFree( pszHDRFilename );
        return( CE_Failure );
    }

    CPLFree( pszHDRFilename );

/* -------------------------------------------------------------------- */
/*      Read the whole file (we expect it to always be 308 bytes        */
/*      long.                                                           */
/* -------------------------------------------------------------------- */

    VSIFRead( abyData, 1, 308, fp );

    VSIFClose( fp );
    
/* -------------------------------------------------------------------- */
/*      Read the block size information.                                */
/* -------------------------------------------------------------------- */
    memcpy( &(psInfo->nBlocksPerRow), abyData+288, 4 );
    memcpy( &(psInfo->nBlocksPerColumn), abyData+292, 4 );
    memcpy( &(psInfo->nBlockXSize), abyData+296, 4 );
    memcpy( &(psInfo->nBlockYSize), abyData+304, 4 );
    
#ifdef CPL_LSB
    psInfo->nBlocksPerRow = CPL_SWAP32( psInfo->nBlocksPerRow );
    psInfo->nBlocksPerColumn = CPL_SWAP32( psInfo->nBlocksPerColumn );
    psInfo->nBlockXSize = CPL_SWAP32( psInfo->nBlockXSize );
    psInfo->nBlockYSize = CPL_SWAP32( psInfo->nBlockYSize );
#endif

    return( CE_None );
}

/************************************************************************/
/*                         AIGReadBlockIndex()                          */
/*                                                                      */
/*      Read the w001001x.adf file, and populate the given info         */
/*      structure with the block offsets, and sizes.                    */
/************************************************************************/

CPLErr AIGReadBlockIndex( const char * pszCoverName, AIGInfo_t * psInfo )

{
    char	*pszHDRFilename;
    FILE	*fp;
    int		nLength, i;
    GUInt32	nValue;
    GUInt32	*panIndex;

/* -------------------------------------------------------------------- */
/*      Open the file hdr.adf file.                                     */
/* -------------------------------------------------------------------- */
    pszHDRFilename = (char *) CPLMalloc(strlen(pszCoverName)+40);
    sprintf( pszHDRFilename, "%s/w001001x.adf", pszCoverName );

    fp = VSIFOpen( pszHDRFilename, "rb" );
    
    if( fp == NULL )
    {
        CPLError( CE_Failure, CPLE_OpenFailed,
                  "Failed to open grid block index file:\n%s\n",
                  pszHDRFilename );

        CPLFree( pszHDRFilename );
        return( CE_Failure );
    }

    CPLFree( pszHDRFilename );

/* -------------------------------------------------------------------- */
/*      Get the file length (in 2 byte shorts)                          */
/* -------------------------------------------------------------------- */
    VSIFSeek( fp, 24, SEEK_SET );
    VSIFRead( &nValue, 1, 4, fp );

    nLength = CPL_MSBWORD32(nValue) * 2;

/* -------------------------------------------------------------------- */
/*      Allocate buffer, and read the file (from beyond the header)     */
/*      into the buffer.                                                */
/* -------------------------------------------------------------------- */
    psInfo->nBlocks = (nLength-100) / 8;
    panIndex = (GUInt32 *) CPLMalloc(psInfo->nBlocks * 8);
    VSIFSeek( fp, 100, SEEK_SET );
    VSIFRead( panIndex, 8, psInfo->nBlocks, fp );

    VSIFClose( fp );

/* -------------------------------------------------------------------- */
/*	Allocate AIGInfo block info arrays.				*/
/* -------------------------------------------------------------------- */
    psInfo->panBlockOffset = (int *) CPLMalloc(4 * psInfo->nBlocks);
    psInfo->panBlockSize = (int *) CPLMalloc(4 * psInfo->nBlocks);

/* -------------------------------------------------------------------- */
/*      Populate the block information.                                 */
/* -------------------------------------------------------------------- */
    for( i = 0; i < psInfo->nBlocks; i++ )
    {
        psInfo->panBlockOffset[i] = CPL_MSBWORD32(panIndex[i*2]) * 2;
        psInfo->panBlockSize[i] = CPL_MSBWORD32(panIndex[i*2+1]) * 2;
    }

    CPLFree( panIndex );

    return( CE_None );
}
