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
 * Revision 1.5  1999/08/09 16:30:44  warmerda
 * Modified 0x00 integer blocks to be treated as a constant min valued block.
 *
 * Revision 1.4  1999/04/21 16:51:30  warmerda
 * fixed up floating point support
 *
 * Revision 1.3  1999/03/02 21:10:28  warmerda
 * added some floating point support
 *
 * Revision 1.2  1999/02/04 22:15:33  warmerda
 * fleshed out implementation
 *
 * Revision 1.1  1999/02/03 14:12:56  warmerda
 * New
 *
 */

#include "aigrid.h"

/************************************************************************/
/*                    AIGProcessRaw32bitFloatBlock()                    */
/*                                                                      */
/*      Process a block using ``00'' (32 bit) raw format.               */
/************************************************************************/

static 
CPLErr AIGProcessRaw32BitFloatBlock( GByte *pabyCur, int nDataSize, int nMin,
                                     int nBlockXSize, int nBlockYSize,
                                     float * pafData )

{
    int		i;

    CPLAssert( nDataSize >= nBlockXSize*nBlockYSize*4 );
    
/* -------------------------------------------------------------------- */
/*      Collect raw data.                                               */
/* -------------------------------------------------------------------- */
    for( i = 0; i < nBlockXSize * nBlockYSize; i++ )
    {
        float	fWork;

#ifdef CPL_LSB
        ((GByte *) &fWork)[3] = *(pabyCur++);
        ((GByte *) &fWork)[2] = *(pabyCur++);
        ((GByte *) &fWork)[1] = *(pabyCur++);
        ((GByte *) &fWork)[0] = *(pabyCur++);
#else
        ((GByte *) &fWork)[0] = *(pabyCur++);
        ((GByte *) &fWork)[1] = *(pabyCur++);
        ((GByte *) &fWork)[2] = *(pabyCur++);
        ((GByte *) &fWork)[3] = *(pabyCur++);
#endif
        
        pafData[i] = fWork;
    }

    return( CE_None );
}

/************************************************************************/
/*                      AIGProcessRaw32bitBlock()                       */
/*                                                                      */
/*      Process a block using ``0x43'' (32 bit) raw format.             */
/************************************************************************/

static 
CPLErr AIGProcessRaw32BitBlock( GByte *pabyCur, int nDataSize, int nMin,
                                int nBlockXSize, int nBlockYSize,
                                GUInt32 * panData )

{
    int		i;

    CPLAssert( nDataSize >= nBlockXSize*nBlockYSize*4 );
    
/* -------------------------------------------------------------------- */
/*      Collect raw data.                                               */
/* -------------------------------------------------------------------- */
    for( i = 0; i < nBlockXSize * nBlockYSize; i++ )
    {
        panData[i] = pabyCur[0]*256*256*256 + pabyCur[1]*256*256
                   + pabyCur[2]*256 + pabyCur[3] + nMin;
        pabyCur += 4;
    }

    return( CE_None );
}

/************************************************************************/
/*                      AIGProcessIntConstBlock()                       */
/*                                                                      */
/*      Process a block using ``00'' constant 32bit integer format.     */
/************************************************************************/

static 
CPLErr AIGProcessIntConstBlock( GByte *pabyCur, int nDataSize, int nMin,
                                int nBlockXSize, int nBlockYSize,
                                GUInt32 * panData )

{
    int		i;

    CPLAssert( nDataSize <= 8 );
    
/* -------------------------------------------------------------------- */
/*	Apply constant min value.					*/
/* -------------------------------------------------------------------- */
    for( i = 0; i < nBlockXSize * nBlockYSize; i++ )
        panData[i] = nMin;

    return( CE_None );
}

/************************************************************************/
/*                         AIGProcess16bitRawBlock()                    */
/*                                                                      */
/*      Process a block using ``10'' (sixteen bit) raw format.          */
/************************************************************************/

static 
CPLErr AIGProcessRaw16BitBlock( GByte *pabyCur, int nDataSize, int nMin,
                                int nBlockXSize, int nBlockYSize,
                                GUInt32 * panData )

{
    int		i;

    CPLAssert( nDataSize >= nBlockXSize*nBlockYSize*2 );
    
/* -------------------------------------------------------------------- */
/*      Collect raw data.                                               */
/* -------------------------------------------------------------------- */
    for( i = 0; i < nBlockXSize * nBlockYSize; i++ )
    {
        panData[i] = pabyCur[0] * 256 + pabyCur[1] + nMin;
        pabyCur += 2;
    }

    return( CE_None );
}

/************************************************************************/
/*                         AIGProcess4BitRawBlock()                     */
/*                                                                      */
/*      Process a block using ``08'' raw format.                        */
/************************************************************************/

static 
CPLErr AIGProcessRaw4BitBlock( GByte *pabyCur, int nDataSize, int nMin,
                               int nBlockXSize, int nBlockYSize,
                               GUInt32 * panData )

{
    int		i;

    CPLAssert( nDataSize >= (nBlockXSize*nBlockYSize+1)/2 );
    
/* -------------------------------------------------------------------- */
/*      Collect raw data.                                               */
/* -------------------------------------------------------------------- */
    for( i = 0; i < nBlockXSize * nBlockYSize; i++ )
    {
        if( i % 2 == 0 )
            panData[i] = ((*(pabyCur) & 0xf0) >> 4) + nMin;
        else
            panData[i] = (*(pabyCur++) & 0xf) + nMin;
    }

    return( CE_None );
}

/************************************************************************/
/*                         AIGProcessRawBlock()                         */
/*                                                                      */
/*      Process a block using ``08'' raw format.                        */
/************************************************************************/

static 
CPLErr AIGProcessRawBlock( GByte *pabyCur, int nDataSize, int nMin,
                        int nBlockXSize, int nBlockYSize, GUInt32 * panData )

{
    int		i;

    CPLAssert( nDataSize >= nBlockXSize*nBlockYSize );
    
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
/*      Process a block using ``D7'', ``E0'' or ``DF'' compression.     */
/************************************************************************/

static 
CPLErr AIGProcessBlock( GByte *pabyCur, int nDataSize, int nMin, int nMagic, 
                        int nBlockXSize, int nBlockYSize, GUInt32 * panData )

{
    int		nTotPixels, nPixels;
    int		i;

/* ==================================================================== */
/*     Process runs till we are done.                                  */
/* ==================================================================== */
    nTotPixels = nBlockXSize * nBlockYSize;
    nPixels = 0;

    while( nPixels < nTotPixels )
    {
        int	nMarker = *(pabyCur++);

        nDataSize--;

/* -------------------------------------------------------------------- */
/*      Repeat data - four byte data block (0xE0)                       */
/* -------------------------------------------------------------------- */
        if( nMagic == 0xE0 )
        {
            GUInt32	nValue;
            int		bNoData = FALSE;
            
            nValue = 0;
            
            for( i = 0; i < 4; i++ )
            {
                nValue = nValue * 256 + *(pabyCur++);
                nDataSize--;
                
                if( i == 0 )
                {
                    if( nValue & 0x80 )
                        nValue &= 0x7f;
                    else
                        bNoData = TRUE;
                }
            }

            if( bNoData )
                nValue = GRID_NO_DATA;
            else
                nValue += nMin;

            for( i = 0; i < nMarker; i++ )
                panData[nPixels++] = nValue;
        }
        
/* -------------------------------------------------------------------- */
/*      Repeat data - one byte data block (0xFC)                        */
/* -------------------------------------------------------------------- */
        else if( nMagic == 0xFC || nMagic == 0xF8 )
        {
            GUInt32	nValue;

            nValue = *(pabyCur++) + nMin;
            nDataSize--;
            
            for( i = 0; i < nMarker; i++ )
                panData[nPixels++] = nValue;
        }
        
/* -------------------------------------------------------------------- */
/*      Repeat data - no actual data, just assign minimum (0xDF)        */
/* -------------------------------------------------------------------- */
        else if( nMagic == 0xDF && nMarker < 128 )
        {
            for( i = 0; i < nMarker; i++ )
                panData[nPixels++] += nMin;
        }
        
/* -------------------------------------------------------------------- */
/*      Literal data (0xD7)                                             */
/* -------------------------------------------------------------------- */
        else if( nMagic == 0xD7 && nMarker < 128 )
        {
            while( nMarker > 0 )
            {
                panData[nPixels++] = *(pabyCur++) + nMin;
                nMarker--;
                nDataSize--;
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

        CPLAssert( nDataSize >= 0 );
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
                     int nBlockXSize, int nBlockYSize,
                     GUInt32 *panData, int nCellType )

{
    GByte	*pabyRaw, *pabyCur;
    CPLErr	eErr;
    int		i, nMagic, nMin=0, nMinSize=0, nDataSize;

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

    nDataSize = nBlockSize;
    
/* -------------------------------------------------------------------- */
/*      Collect minimum value.                                          */
/* -------------------------------------------------------------------- */
    pabyCur = pabyRaw + 2;
    if( (nCellType == AIG_CELLTYPE_INT || pabyCur[0] != 0x00)
        && pabyCur[0] != 0x43 )
    {
        nMinSize = pabyCur[1];
        pabyCur += 2;
        
        nMin = 0;
        for( i = 0; i < nMinSize; i++ )
        {
            nMin = nMin * 256 + *pabyCur;
            
            if( i == 0 )
                nMin &= 0x7f;
            
            pabyCur++;
        }

        nDataSize -= 2+nMinSize;
    }
    
/* -------------------------------------------------------------------- */
/*	Call an apppropriate handler depending on magic code.		*/
/* -------------------------------------------------------------------- */
    nMagic = pabyRaw[2];

    if( nMagic == 0x08 )
    {
        AIGProcessRawBlock( pabyCur, nDataSize, nMin,
                            nBlockXSize, nBlockYSize,
                            panData );
    }
    else if( nMagic == 0x04 )
    {
        AIGProcessRaw4BitBlock( pabyCur, nDataSize, nMin,
                                nBlockXSize, nBlockYSize,
                                panData );
    }
    else if( nCellType == AIG_CELLTYPE_INT && nMagic == 0x00 )
    {
        AIGProcessIntConstBlock( pabyCur, nDataSize, nMin,
                                 nBlockXSize, nBlockYSize, panData );
    }
    else if( nMagic == 0x00 || nMagic == 0x43 )
    {
        if( nCellType == AIG_CELLTYPE_FLOAT )
            AIGProcessRaw32BitFloatBlock( pabyCur, nDataSize, 0,
                                          nBlockXSize, nBlockYSize,
                                          (float *) panData );
        else
            AIGProcessRaw32BitBlock( pabyCur, nDataSize, nMin,
                                     nBlockXSize, nBlockYSize,
                                     panData );
    }
    else if( nMagic == 0x10 )
    {
        AIGProcessRaw16BitBlock( pabyCur, nDataSize, nMin,
                                 nBlockXSize, nBlockYSize,
                                 panData );
    }
    else if( nMagic == 0xFF )
    {
        /* just fill with no data value ... I can't figure this one out */
        for( i = 0; i < nBlockXSize * nBlockYSize; i++ )
            panData[i] = GRID_NO_DATA;
    }
    else
    {
        eErr = AIGProcessBlock( pabyCur, nDataSize, nMin, nMagic,
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
    memcpy( &(psInfo->nCellType), abyData+16, 4 );
    memcpy( &(psInfo->nBlocksPerRow), abyData+288, 4 );
    memcpy( &(psInfo->nBlocksPerColumn), abyData+292, 4 );
    memcpy( &(psInfo->nBlockXSize), abyData+296, 4 );
    memcpy( &(psInfo->nBlockYSize), abyData+304, 4 );
    memcpy( &(psInfo->dfCellSizeX), abyData+256, 8 );
    memcpy( &(psInfo->dfCellSizeY), abyData+264, 8 );
    
#ifdef CPL_LSB
    psInfo->nCellType = CPL_SWAP32( psInfo->nCellType );
    psInfo->nBlocksPerRow = CPL_SWAP32( psInfo->nBlocksPerRow );
    psInfo->nBlocksPerColumn = CPL_SWAP32( psInfo->nBlocksPerColumn );
    psInfo->nBlockXSize = CPL_SWAP32( psInfo->nBlockXSize );
    psInfo->nBlockYSize = CPL_SWAP32( psInfo->nBlockYSize );
    CPL_SWAPDOUBLE( &(psInfo->dfCellSizeX) );
    CPL_SWAPDOUBLE( &(psInfo->dfCellSizeY) );
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

/************************************************************************/
/*                           AIGReadBounds()                            */
/*                                                                      */
/*      Read the dblbnd.adf file for the georeferenced bounds.          */
/************************************************************************/

CPLErr AIGReadBounds( const char * pszCoverName, AIGInfo_t * psInfo )

{
    char	*pszHDRFilename;
    FILE	*fp;
    double	adfBound[4];

/* -------------------------------------------------------------------- */
/*      Open the file dblbnd.adf file.                                  */
/* -------------------------------------------------------------------- */
    pszHDRFilename = (char *) CPLMalloc(strlen(pszCoverName)+40);
    sprintf( pszHDRFilename, "%s/dblbnd.adf", pszCoverName );

    fp = VSIFOpen( pszHDRFilename, "rb" );
    
    if( fp == NULL )
    {
        CPLError( CE_Failure, CPLE_OpenFailed,
                  "Failed to open grid bounds file:\n%s\n",
                  pszHDRFilename );

        CPLFree( pszHDRFilename );
        return( CE_Failure );
    }

    CPLFree( pszHDRFilename );

/* -------------------------------------------------------------------- */
/*      Get the contents - four doubles.                                */
/* -------------------------------------------------------------------- */
    VSIFRead( adfBound, 1, 32, fp );

    VSIFClose( fp );

#ifdef CPL_LSB
    CPL_SWAPDOUBLE(adfBound+0);
    CPL_SWAPDOUBLE(adfBound+1);
    CPL_SWAPDOUBLE(adfBound+2);
    CPL_SWAPDOUBLE(adfBound+3);
#endif    
    
    psInfo->dfLLX = adfBound[0];
    psInfo->dfLLY = adfBound[1];
    psInfo->dfURX = adfBound[2];
    psInfo->dfURY = adfBound[3];

    return( CE_None );
}

/************************************************************************/
/*                         AIGReadStatistics()                          */
/*                                                                      */
/*      Read the sta.adf file for the layer statistics.                 */
/************************************************************************/

CPLErr AIGReadStatistics( const char * pszCoverName, AIGInfo_t * psInfo )

{
    char	*pszHDRFilename;
    FILE	*fp;
    double	adfStats[4];

    psInfo->dfMin = 0.0;
    psInfo->dfMax = 0.0;
    psInfo->dfMean = 0.0;
    psInfo->dfStdDev = 0.0;

/* -------------------------------------------------------------------- */
/*      Open the file sta.adf file.                                     */
/* -------------------------------------------------------------------- */
    pszHDRFilename = (char *) CPLMalloc(strlen(pszCoverName)+40);
    sprintf( pszHDRFilename, "%s/sta.adf", pszCoverName );

    fp = VSIFOpen( pszHDRFilename, "rb" );
    
    if( fp == NULL )
    {
        CPLError( CE_Failure, CPLE_OpenFailed,
                  "Failed to open grid statistics file:\n%s\n",
                  pszHDRFilename );

        CPLFree( pszHDRFilename );
        return( CE_Failure );
    }

    CPLFree( pszHDRFilename );

/* -------------------------------------------------------------------- */
/*      Get the contents - four doubles.                                */
/* -------------------------------------------------------------------- */
    VSIFRead( adfStats, 1, 32, fp );

    VSIFClose( fp );

#ifdef CPL_LSB
    CPL_SWAPDOUBLE(adfStats+0);
    CPL_SWAPDOUBLE(adfStats+1);
    CPL_SWAPDOUBLE(adfStats+2);
    CPL_SWAPDOUBLE(adfStats+3);
#endif    
    
    psInfo->dfMin = adfStats[0];
    psInfo->dfMax = adfStats[1];
    psInfo->dfMean = adfStats[2];
    psInfo->dfStdDev = adfStats[3];

    return( CE_None );
}





