/******************************************************************************
 * $Id$
 *
 * Project:  Arc/Info Binary Grid Translator
 * Purpose:  Grid file reading code.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 1999, Frank Warmerdam
 * Copyright (c) 2007-2010, Even Rouault <even dot rouault at mines-paris dot org>
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

#include "aigrid.h"

CPL_CVSID("$Id$");

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

    (void) nMin;
    if( nDataSize < nBlockXSize*nBlockYSize*4 )
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Block too small");
        return CE_Failure;
    }
    
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
/*                      AIGProcessIntConstBlock()                       */
/*                                                                      */
/*      Process a block using ``00'' constant 32bit integer format.     */
/************************************************************************/

static 
CPLErr AIGProcessIntConstBlock( GByte *pabyCur, int nDataSize, int nMin,
                                int nBlockXSize, int nBlockYSize,
                                GInt32 * panData )

{
    int		i;

    (void) pabyCur;
    (void) nDataSize;
    
/* -------------------------------------------------------------------- */
/*	Apply constant min value.					*/
/* -------------------------------------------------------------------- */
    for( i = 0; i < nBlockXSize * nBlockYSize; i++ )
        panData[i] = nMin;

    return( CE_None );
}

/************************************************************************/
/*                         AIGProcess32bitRawBlock()                    */
/*                                                                      */
/*      Process a block using ``20'' (thirtytwo bit) raw format.        */
/************************************************************************/

static 
CPLErr AIGProcessRaw32BitBlock( GByte *pabyCur, int nDataSize, int nMin,
                                int nBlockXSize, int nBlockYSize,
                                GInt32 * panData )

{
    int		i;

    if( nDataSize < nBlockXSize*nBlockYSize*4 )
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Block too small");
        return CE_Failure;
    }
    
/* -------------------------------------------------------------------- */
/*      Collect raw data.                                               */
/* -------------------------------------------------------------------- */
    for( i = 0; i < nBlockXSize * nBlockYSize; i++ )
    {
        panData[i] = pabyCur[0] * 256 * 256 * 256
            + pabyCur[1] * 256 * 256
            + pabyCur[2] * 256 
            + pabyCur[3] + nMin;
        pabyCur += 4;
    }

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
                                GInt32 * panData )

{
    int		i;

    if( nDataSize < nBlockXSize*nBlockYSize*2 )
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Block too small");
        return CE_Failure;
    }
    
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
                               GInt32 * panData )

{
    int		i;

    if( nDataSize < (nBlockXSize*nBlockYSize+1)/2 )
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Block too small");
        return CE_Failure;
    } 
    
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
/*                       AIGProcess1BitRawBlock()                       */
/*                                                                      */
/*      Process a block using ``0x01'' raw format.                      */
/************************************************************************/

static 
CPLErr AIGProcessRaw1BitBlock( GByte *pabyCur, int nDataSize, int nMin,
                               int nBlockXSize, int nBlockYSize,
                               GInt32 * panData )

{
    int		i;

    if( nDataSize < (nBlockXSize*nBlockYSize+7)/8 )
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Block too small");
        return CE_Failure;
    }
    
/* -------------------------------------------------------------------- */
/*      Collect raw data.                                               */
/* -------------------------------------------------------------------- */
    for( i = 0; i < nBlockXSize * nBlockYSize; i++ )
    {
        if( pabyCur[i>>3] & (0x80 >> (i&0x7)) )
            panData[i] = 1 + nMin;
        else
            panData[i] = 0 + nMin;
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
                        int nBlockXSize, int nBlockYSize, GInt32 * panData )

{
    int		i;

    if( nDataSize < nBlockXSize*nBlockYSize )
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Block too small");
        return CE_Failure;
    }
    
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
/*                         AIGProcessFFBlock()                          */
/*                                                                      */
/*      Process a type 0xFF (CCITT RLE) compressed block.               */
/************************************************************************/

static 
CPLErr AIGProcessFFBlock( GByte *pabyCur, int nDataSize, int nMin,
                          int nBlockXSize, int nBlockYSize,
                          GInt32 * panData )

{
/* -------------------------------------------------------------------- */
/*      Convert CCITT compress bitstream into 1bit raw data.            */
/* -------------------------------------------------------------------- */
    CPLErr eErr;
    int i, nDstBytes = (nBlockXSize * nBlockYSize + 7) / 8;
    unsigned char *pabyIntermediate;

    pabyIntermediate = (unsigned char *) VSIMalloc(nDstBytes);
    if (pabyIntermediate == NULL)
    {
        CPLError(CE_Failure, CPLE_OutOfMemory,
                 "Cannot allocate %d bytes", nDstBytes);
        return CE_Failure;
    }
    
    eErr = DecompressCCITTRLETile( pabyCur, nDataSize, 
                                   pabyIntermediate, nDstBytes,
                                   nBlockXSize, nBlockYSize );
    if( eErr != CE_None )
    {
        CPLFree(pabyIntermediate);
        return eErr;
    }

/* -------------------------------------------------------------------- */
/*      Convert the bit buffer into 32bit integers and account for      */
/*      nMin.                                                           */
/* -------------------------------------------------------------------- */
    for( i = 0; i < nBlockXSize * nBlockYSize; i++ )
    {
        if( pabyIntermediate[i>>3] & (0x80 >> (i&0x7)) )
            panData[i] = nMin+1;
        else
            panData[i] = nMin;
    }

    CPLFree( pabyIntermediate );

    return( CE_None );
}



/************************************************************************/
/*                          AIGProcessBlock()                           */
/*                                                                      */
/*      Process a block using ``D7'', ``E0'' or ``DF'' compression.     */
/************************************************************************/

static 
CPLErr AIGProcessBlock( GByte *pabyCur, int nDataSize, int nMin, int nMagic, 
                        int nBlockXSize, int nBlockYSize, GInt32 * panData )

{
    int		nTotPixels, nPixels;
    int		i;

/* ==================================================================== */
/*     Process runs till we are done.                                  */
/* ==================================================================== */
    nTotPixels = nBlockXSize * nBlockYSize;
    nPixels = 0;

    while( nPixels < nTotPixels && nDataSize > 0 )
    {
        int	nMarker = *(pabyCur++);

        nDataSize--;
        
/* -------------------------------------------------------------------- */
/*      Repeat data - four byte data block (0xE0)                       */
/* -------------------------------------------------------------------- */
        if( nMagic == 0xE0 )
        {
            GInt32	nValue;
            
            if( nMarker + nPixels > nTotPixels )
            {
                CPLError( CE_Failure, CPLE_AppDefined, 
                          "Run too long in AIGProcessBlock, needed %d values, got %d.", 
                          nTotPixels - nPixels, nMarker );
                return CE_Failure;
            }
        
            if( nDataSize < 4 )
            {
                CPLError(CE_Failure, CPLE_AppDefined, "Block too small");
                return CE_Failure;
            }

            nValue = 0;
            memcpy( &nValue, pabyCur, 4 );
            pabyCur += 4;
            nDataSize -= 4;

            nValue = CPL_MSBWORD32( nValue );

            nValue += nMin;
            for( i = 0; i < nMarker; i++ )
                panData[nPixels++] = nValue;
        }
        
/* -------------------------------------------------------------------- */
/*      Repeat data - two byte data block (0xF0)                        */
/* -------------------------------------------------------------------- */
        else if( nMagic == 0xF0 )
        {
            GInt32	nValue;
            
            if( nMarker + nPixels > nTotPixels )
            {
                CPLError( CE_Failure, CPLE_AppDefined, 
                          "Run too long in AIGProcessBlock, needed %d values, got %d.", 
                          nTotPixels - nPixels, nMarker );
                return CE_Failure;
            }

            if( nDataSize < 2 )
            {
                CPLError(CE_Failure, CPLE_AppDefined, "Block too small");
                return CE_Failure;
            }

            nValue = (pabyCur[0] * 256 + pabyCur[1]) + nMin;
            pabyCur += 2;
            nDataSize -= 2;

            for( i = 0; i < nMarker; i++ )
                panData[nPixels++] = nValue;
        }
        
/* -------------------------------------------------------------------- */
/*      Repeat data - one byte data block (0xFC)                        */
/* -------------------------------------------------------------------- */
        else if( nMagic == 0xFC || nMagic == 0xF8 )
        {
            GInt32	nValue;

            if( nMarker + nPixels > nTotPixels )
            {
                CPLError( CE_Failure, CPLE_AppDefined, 
                          "Run too long in AIGProcessBlock, needed %d values, got %d.", 
                          nTotPixels - nPixels, nMarker );
                return CE_Failure;
            }
            
            if( nDataSize < 1 )
            {
                CPLError(CE_Failure, CPLE_AppDefined, "Block too small");
                return CE_Failure;
            }
        
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
            if( nMarker + nPixels > nTotPixels )
            {
                CPLError( CE_Failure, CPLE_AppDefined, 
                          "Run too long in AIGProcessBlock, needed %d values, got %d.", 
                          nTotPixels - nPixels, nMarker );
                return CE_Failure;
            }
        
            for( i = 0; i < nMarker; i++ )
                panData[nPixels++] = nMin;
        }
        
/* -------------------------------------------------------------------- */
/*      Literal data (0xD7): 8bit values.                               */
/* -------------------------------------------------------------------- */
        else if( nMagic == 0xD7 && nMarker < 128 )
        {
            if( nMarker + nPixels > nTotPixels )
            {
                CPLError( CE_Failure, CPLE_AppDefined, 
                          "Run too long in AIGProcessBlock, needed %d values, got %d.", 
                          nTotPixels - nPixels, nMarker );
                return CE_Failure;
            }
        
            while( nMarker > 0 && nDataSize > 0 )
            {
                panData[nPixels++] = *(pabyCur++) + nMin;
                nMarker--;
                nDataSize--;
            }
        }

/* -------------------------------------------------------------------- */
/*      Literal data (0xCF): 16 bit values.                             */
/* -------------------------------------------------------------------- */
        else if( nMagic == 0xCF && nMarker < 128 )
        {
            GInt32	nValue;
            
            if( nMarker + nPixels > nTotPixels )
            {
                CPLError( CE_Failure, CPLE_AppDefined, 
                          "Run too long in AIGProcessBlock, needed %d values, got %d.", 
                          nTotPixels - nPixels, nMarker );
                return CE_Failure;
            }
        
            while( nMarker > 0 && nDataSize >= 2 )
            {
                nValue = pabyCur[0] * 256 + pabyCur[1] + nMin;
                panData[nPixels++] = nValue;
                pabyCur += 2;

                nMarker--;
                nDataSize -= 2;
            }
        }

/* -------------------------------------------------------------------- */
/*      Nodata repeat                                                   */
/* -------------------------------------------------------------------- */
        else if( nMarker > 128 )
        {
            nMarker = 256 - nMarker;

            if( nMarker + nPixels > nTotPixels )
            {
                CPLError( CE_Failure, CPLE_AppDefined, 
                          "Run too long in AIGProcessBlock, needed %d values, got %d.", 
                          nTotPixels - nPixels, nMarker );
                return CE_Failure;
            }
        
            while( nMarker > 0 )
            {
                panData[nPixels++] = ESRI_GRID_NO_DATA;
                nMarker--;
            }
        }

        else
        {
            return CE_Failure;
        }

    }

    if( nPixels < nTotPixels || nDataSize < 0 )
    {
        CPLError( CE_Failure, CPLE_AppDefined, 
                  "Ran out of data processing block with nMagic=%d.", 
                  nMagic );
        return CE_Failure;
    }
    
    return CE_None;
}

/************************************************************************/
/*                            AIGReadBlock()                            */
/*                                                                      */
/*      Read a single block of integer grid data.                       */
/************************************************************************/

CPLErr AIGReadBlock( VSILFILE * fp, GUInt32 nBlockOffset, int nBlockSize,
                     int nBlockXSize, int nBlockYSize,
                     GInt32 *panData, int nCellType, int bCompressed )

{
    GByte	*pabyRaw, *pabyCur;
    CPLErr	eErr;
    int		i, nMagic, nMinSize=0, nDataSize;
    GInt32 	nMin = 0;

/* -------------------------------------------------------------------- */
/*      If the block has zero size it is all dummies.                   */
/* -------------------------------------------------------------------- */
    if( nBlockSize == 0 )
    {
        for( i = 0; i < nBlockXSize * nBlockYSize; i++ )
            panData[i] = ESRI_GRID_NO_DATA;

        return( CE_None );
    }
    
/* -------------------------------------------------------------------- */
/*      Read the block into memory.                                     */
/* -------------------------------------------------------------------- */
    if (nBlockSize <= 0 || nBlockSize > 65535 * 2)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Invalid block size : %d", nBlockSize);
        return CE_Failure;
    }

    pabyRaw = (GByte *) VSIMalloc(nBlockSize+2);
    if (pabyRaw == NULL)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Cannot allocate memory for block");
        return CE_Failure;
    }

    if( VSIFSeekL( fp, nBlockOffset, SEEK_SET ) != 0 
        || VSIFReadL( pabyRaw, nBlockSize+2, 1, fp ) != 1 )
    {
        memset( panData, 0, nBlockXSize*nBlockYSize*4 );
        CPLError( CE_Failure, CPLE_AppDefined, 
                  "Read of %d bytes from offset %d for grid block failed.", 
                  nBlockSize+2, nBlockOffset );
        CPLFree( pabyRaw );
        return CE_Failure;
    }

/* -------------------------------------------------------------------- */
/*      Verify the block size.                                          */
/* -------------------------------------------------------------------- */
    if( nBlockSize != (pabyRaw[0]*256 + pabyRaw[1])*2 )
    {
        memset( panData, 0, nBlockXSize*nBlockYSize*4 );
        CPLError( CE_Failure, CPLE_AppDefined, 
                  "Block is corrupt, block size was %d, but expected to be %d.", 
                  (pabyRaw[0]*256 + pabyRaw[1])*2, nBlockSize );
        CPLFree( pabyRaw );
        return CE_Failure;
    }

    nDataSize = nBlockSize;
    
/* -------------------------------------------------------------------- */
/*      Handle float files and uncompressed integer files directly.     */
/* -------------------------------------------------------------------- */
    if( nCellType == AIG_CELLTYPE_FLOAT )
    {
        AIGProcessRaw32BitFloatBlock( pabyRaw + 2, nDataSize, 0, 
                                      nBlockXSize, nBlockYSize, 
                                      (float *) panData );
        CPLFree( pabyRaw );

        return CE_None;
    }

    if( nCellType == AIG_CELLTYPE_INT && !bCompressed  )
    {
        AIGProcessRaw32BitBlock( pabyRaw+2, nDataSize, nMin,
                                 nBlockXSize, nBlockYSize,
                                 panData );
        CPLFree( pabyRaw );
        return CE_None;
    }

/* -------------------------------------------------------------------- */
/*      Collect minimum value.                                          */
/* -------------------------------------------------------------------- */

    /* The first 2 bytes that give the block size are not included in nDataSize */
    /* and have already been safely read */
    pabyCur = pabyRaw + 2;

    /* Need at least 2 byte to read the nMinSize and the nMagic */
    if (nDataSize < 2)
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "Corrupt block. Need 2 bytes to read nMagic and nMinSize, only %d available",
                  nDataSize);
        CPLFree( pabyRaw );
        return CE_Failure;
    }
    nMagic = pabyCur[0];
    nMinSize = pabyCur[1];
    pabyCur += 2;
    nDataSize -= 2;

    /* Need at least nMinSize bytes to read the nMin value */
    if (nDataSize < nMinSize)
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "Corrupt block. Need %d bytes to read nMin. Only %d available",
                  nMinSize, nDataSize);
        CPLFree( pabyRaw );
        return CE_Failure;
    }

    if( nMinSize > 4 )
    {
        memset( panData, 0, nBlockXSize*nBlockYSize*4 );
        CPLError( CE_Failure, CPLE_AppDefined, 
                  "Corrupt 'minsize' of %d in block header.  Read aborted.", 
                  nMinSize );
        CPLFree( pabyRaw );
        return CE_Failure;
    }

    if( nMinSize == 4 )
    {
        memcpy( &nMin, pabyCur, 4 );
        nMin = CPL_MSBWORD32( nMin );
        pabyCur += 4;
    }
    else
    {
        nMin = 0;
        for( i = 0; i < nMinSize; i++ )
        {
            nMin = nMin * 256 + *pabyCur;
            pabyCur++;
        }

        /* If nMinSize = 0, then we might have only 4 bytes in pabyRaw */
        /* don't try to read the 5th one then */
        if( nMinSize != 0 && pabyRaw[4] > 127 )
        {
            if( nMinSize == 2 )
                nMin = nMin - 65536;
            else if( nMinSize == 1 )
                nMin = nMin - 256;
            else if( nMinSize == 3 )
                nMin = nMin - 256*256*256;
        }
    }
    
    nDataSize -= nMinSize;
    
/* -------------------------------------------------------------------- */
/*	Call an apppropriate handler depending on magic code.		*/
/* -------------------------------------------------------------------- */

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
    else if( nMagic == 0x01 )
    {
        AIGProcessRaw1BitBlock( pabyCur, nDataSize, nMin,
                                nBlockXSize, nBlockYSize,
                                panData );
    }
    else if( nMagic == 0x00 )
    {
        AIGProcessIntConstBlock( pabyCur, nDataSize, nMin,
                                 nBlockXSize, nBlockYSize, panData );
    }
    else if( nMagic == 0x10 )
    {
        AIGProcessRaw16BitBlock( pabyCur, nDataSize, nMin,
                                 nBlockXSize, nBlockYSize,
                                 panData );
    }
    else if( nMagic == 0x20 )
    {
        AIGProcessRaw32BitBlock( pabyCur, nDataSize, nMin,
                                 nBlockXSize, nBlockYSize,
                                 panData );
    }
    else if( nMagic == 0xFF )
    {
        AIGProcessFFBlock( pabyCur, nDataSize, nMin,
                           nBlockXSize, nBlockYSize,
                           panData );
    }
    else
    {
        eErr = AIGProcessBlock( pabyCur, nDataSize, nMin, nMagic,
                                nBlockXSize, nBlockYSize, panData );
        
        if( eErr == CE_Failure )
        {
            static int	bHasWarned = FALSE;
            
            for( i = 0; i < nBlockXSize * nBlockYSize; i++ )
                panData[i] = ESRI_GRID_NO_DATA;

            if( !bHasWarned )
            {
                CPLError( CE_Warning, CPLE_AppDefined,
                          "Unsupported Arc/Info Binary Grid tile of type 0x%X"
                          " encountered.\n"
                          "This and subsequent unsupported tile types set to"
                          " no data value.\n",
                          nMagic );
                bHasWarned = TRUE;
            }
        }
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
    VSILFILE	*fp;
    GByte	abyData[308];

/* -------------------------------------------------------------------- */
/*      Open the file hdr.adf file.                                     */
/* -------------------------------------------------------------------- */
    pszHDRFilename = (char *) CPLMalloc(strlen(pszCoverName)+30);
    sprintf( pszHDRFilename, "%s/hdr.adf", pszCoverName );

    fp = AIGLLOpen( pszHDRFilename, "rb" );
    
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

    VSIFReadL( abyData, 1, 308, fp );

    VSIFCloseL( fp );
    
/* -------------------------------------------------------------------- */
/*      Read the block size information.                                */
/* -------------------------------------------------------------------- */
    memcpy( &(psInfo->nCellType), abyData+16, 4 );
    memcpy( &(psInfo->bCompressed), abyData+20, 4 );
    memcpy( &(psInfo->nBlocksPerRow), abyData+288, 4 );
    memcpy( &(psInfo->nBlocksPerColumn), abyData+292, 4 );
    memcpy( &(psInfo->nBlockXSize), abyData+296, 4 );
    memcpy( &(psInfo->nBlockYSize), abyData+304, 4 );
    memcpy( &(psInfo->dfCellSizeX), abyData+256, 8 );
    memcpy( &(psInfo->dfCellSizeY), abyData+264, 8 );
    
#ifdef CPL_LSB
    psInfo->nCellType = CPL_SWAP32( psInfo->nCellType );
    psInfo->bCompressed = CPL_SWAP32( psInfo->bCompressed );
    psInfo->nBlocksPerRow = CPL_SWAP32( psInfo->nBlocksPerRow );
    psInfo->nBlocksPerColumn = CPL_SWAP32( psInfo->nBlocksPerColumn );
    psInfo->nBlockXSize = CPL_SWAP32( psInfo->nBlockXSize );
    psInfo->nBlockYSize = CPL_SWAP32( psInfo->nBlockYSize );
    CPL_SWAPDOUBLE( &(psInfo->dfCellSizeX) );
    CPL_SWAPDOUBLE( &(psInfo->dfCellSizeY) );
#endif

    psInfo->bCompressed = !psInfo->bCompressed;

    return( CE_None );
}

/************************************************************************/
/*                         AIGReadBlockIndex()                          */
/*                                                                      */
/*      Read the w001001x.adf file, and populate the given info         */
/*      structure with the block offsets, and sizes.                    */
/************************************************************************/

CPLErr AIGReadBlockIndex( AIGInfo_t * psInfo, AIGTileInfo *psTInfo, 
                          const char *pszBasename )

{
    char	*pszHDRFilename;
    VSILFILE	*fp;
    int		nLength, i;
    GInt32	nValue;
    GUInt32	*panIndex;
    GByte       abyHeader[8];

/* -------------------------------------------------------------------- */
/*      Open the file hdr.adf file.                                     */
/* -------------------------------------------------------------------- */
    pszHDRFilename = (char *) CPLMalloc(strlen(psInfo->pszCoverName)+40);
    sprintf( pszHDRFilename, "%s/%sx.adf", psInfo->pszCoverName, pszBasename );

    fp = AIGLLOpen( pszHDRFilename, "rb" );
    
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
/*      Verify the magic number.  This is often corrupted by CR/LF      */
/*      translation.                                                    */
/* -------------------------------------------------------------------- */
    VSIFReadL( abyHeader, 1, 8, fp );
    if( abyHeader[3] == 0x0D && abyHeader[4] == 0x0A )
    {
        CPLError( CE_Failure, CPLE_AppDefined, 
                  "w001001x.adf file header has been corrupted by unix to dos text conversion." );
        VSIFCloseL( fp );
        return CE_Failure;
    }

    if( abyHeader[0] != 0x00
        || abyHeader[1] != 0x00 
        || abyHeader[2] != 0x27
        || abyHeader[3] != 0x0A
        || abyHeader[4] != 0xFF
        || abyHeader[5] != 0xFF )
    {
        CPLError( CE_Failure, CPLE_AppDefined, 
                  "w001001x.adf file header magic number is corrupt." );
        VSIFCloseL( fp );
        return CE_Failure;
    }

/* -------------------------------------------------------------------- */
/*      Get the file length (in 2 byte shorts)                          */
/* -------------------------------------------------------------------- */
    VSIFSeekL( fp, 24, SEEK_SET );
    VSIFReadL( &nValue, 1, 4, fp );

    // FIXME? : risk of overflow in multiplication
    nLength = CPL_MSBWORD32(nValue) * 2;

/* -------------------------------------------------------------------- */
/*      Allocate buffer, and read the file (from beyond the header)     */
/*      into the buffer.                                                */
/* -------------------------------------------------------------------- */
    psTInfo->nBlocks = (nLength-100) / 8;
    panIndex = (GUInt32 *) VSIMalloc2(psTInfo->nBlocks, 8);
    if (panIndex == NULL)
    {
        CPLError(CE_Failure, CPLE_OutOfMemory,
                 "AIGReadBlockIndex: Out of memory. Probably due to corrupted w001001x.adf file");
        VSIFCloseL( fp );
        return CE_Failure;
    }
    VSIFSeekL( fp, 100, SEEK_SET );
    if ((int)VSIFReadL( panIndex, 8, psTInfo->nBlocks, fp ) != psTInfo->nBlocks)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "AIGReadBlockIndex: Cannot read block info");
        VSIFCloseL( fp );
        CPLFree( panIndex );
        return CE_Failure;
    }

    VSIFCloseL( fp );

/* -------------------------------------------------------------------- */
/*	Allocate AIGInfo block info arrays.				*/
/* -------------------------------------------------------------------- */
    psTInfo->panBlockOffset = (GUInt32 *) VSIMalloc2(4, psTInfo->nBlocks);
    psTInfo->panBlockSize = (int *) VSIMalloc2(4, psTInfo->nBlocks);
    if (psTInfo->panBlockOffset == NULL || 
        psTInfo->panBlockSize == NULL)
    {
        CPLError(CE_Failure, CPLE_OutOfMemory,
                 "AIGReadBlockIndex: Out of memory. Probably due to corrupted w001001x.adf file");
        CPLFree( psTInfo->panBlockOffset );
        CPLFree( psTInfo->panBlockSize );
        CPLFree( panIndex );
        return CE_Failure;
    }

/* -------------------------------------------------------------------- */
/*      Populate the block information.                                 */
/* -------------------------------------------------------------------- */
    for( i = 0; i < psTInfo->nBlocks; i++ )
    {
        psTInfo->panBlockOffset[i] = CPL_MSBWORD32(panIndex[i*2]) * 2;
        psTInfo->panBlockSize[i] = CPL_MSBWORD32(panIndex[i*2+1]) * 2;
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
    VSILFILE	*fp;
    double	adfBound[4];

/* -------------------------------------------------------------------- */
/*      Open the file dblbnd.adf file.                                  */
/* -------------------------------------------------------------------- */
    pszHDRFilename = (char *) CPLMalloc(strlen(pszCoverName)+40);
    sprintf( pszHDRFilename, "%s/dblbnd.adf", pszCoverName );

    fp = AIGLLOpen( pszHDRFilename, "rb" );
    
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
    VSIFReadL( adfBound, 1, 32, fp );

    VSIFCloseL( fp );

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
    VSILFILE	*fp;
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

    fp = AIGLLOpen( pszHDRFilename, "rb" );
    
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
    VSIFReadL( adfStats, 1, 32, fp );

    VSIFCloseL( fp );

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

