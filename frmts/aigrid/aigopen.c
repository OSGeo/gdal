/******************************************************************************
 * $Id$
 *
 * Project:  Arc/Info Binary Grid Translator
 * Purpose:  Grid file access cover API for non-GDAL use.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 1999, Frank Warmerdam
 * Copyright (c) 2009-2010, Even Rouault <even dot rouault at mines-paris dot org>
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
/*                              AIGOpen()                               */
/************************************************************************/

AIGInfo_t *AIGOpen( const char * pszInputName, const char * pszAccess )

{
    AIGInfo_t	*psInfo;
    char        *pszCoverName;

    (void) pszAccess;

/* -------------------------------------------------------------------- */
/*      If the pass name ends in .adf assume a file within the          */
/*      coverage has been selected, and strip that off the coverage     */
/*      name.                                                           */
/* -------------------------------------------------------------------- */
    pszCoverName = CPLStrdup( pszInputName );
    if( EQUAL(pszCoverName+strlen(pszCoverName)-4, ".adf") )
    {
        int      i;

        for( i = strlen(pszCoverName)-1; i > 0; i-- )
        {
            if( pszCoverName[i] == '\\' || pszCoverName[i] == '/' )
            {
                pszCoverName[i] = '\0';
                break;
            }
        }

        if( i == 0 )
            strcpy(pszCoverName,".");
    }

/* -------------------------------------------------------------------- */
/*      Allocate info structure.                                        */
/* -------------------------------------------------------------------- */
    psInfo = (AIGInfo_t *) CPLCalloc(sizeof(AIGInfo_t),1);
    psInfo->bHasWarned = FALSE;
    psInfo->pszCoverName = pszCoverName;

/* -------------------------------------------------------------------- */
/*      Read the header file.                                           */
/* -------------------------------------------------------------------- */
    if( AIGReadHeader( pszCoverName, psInfo ) != CE_None )
    {
        CPLFree( pszCoverName );
        CPLFree( psInfo );
        return NULL;
    }

/* -------------------------------------------------------------------- */
/*      Read the extents.                                               */
/* -------------------------------------------------------------------- */
    if( AIGReadBounds( pszCoverName, psInfo ) != CE_None )
    {
        AIGClose( psInfo );
        return NULL;
    }

/* -------------------------------------------------------------------- */
/*      Compute the number of pixels and lines, and the number of       */
/*      tile files.                                                     */
/* -------------------------------------------------------------------- */
    if (psInfo->dfCellSizeX <= 0 || psInfo->dfCellSizeY <= 0)
    {
        CPLError( CE_Failure, CPLE_AppDefined, 
                  "Illegal cell size : %f x %f",
                  psInfo->dfCellSizeX, psInfo->dfCellSizeY );
        AIGClose( psInfo );
        return NULL;
    }

    psInfo->nPixels = (int)
        ((psInfo->dfURX - psInfo->dfLLX + 0.5 * psInfo->dfCellSizeX) 
		/ psInfo->dfCellSizeX);
    psInfo->nLines = (int)
        ((psInfo->dfURY - psInfo->dfLLY + 0.5 * psInfo->dfCellSizeY) 
		/ psInfo->dfCellSizeY);
    
    if (psInfo->nPixels <= 0 || psInfo->nLines <= 0)
    {
        CPLError( CE_Failure, CPLE_AppDefined, 
                  "Invalid raster dimensions : %d x %d",
                  psInfo->nPixels, psInfo->nLines );
        AIGClose( psInfo );
        return NULL;
    }

    if (psInfo->nBlockXSize <= 0 || psInfo->nBlockYSize <= 0 ||
        psInfo->nBlocksPerRow <= 0 || psInfo->nBlocksPerColumn <= 0 ||
        psInfo->nBlockXSize > INT_MAX / psInfo->nBlocksPerRow ||
        psInfo->nBlockYSize > INT_MAX / psInfo->nBlocksPerColumn)
    {
        CPLError( CE_Failure, CPLE_AppDefined, 
                  "Invalid block characteristics: nBlockXSize=%d, "
                  "nBlockYSize=%d, nBlocksPerRow=%d, nBlocksPerColumn=%d",
                  psInfo->nBlockXSize, psInfo->nBlockYSize,
                  psInfo->nBlocksPerRow, psInfo->nBlocksPerColumn);
        AIGClose( psInfo );
        return NULL;
    }

    psInfo->nTileXSize = psInfo->nBlockXSize * psInfo->nBlocksPerRow;
    psInfo->nTileYSize = psInfo->nBlockYSize * psInfo->nBlocksPerColumn;

    psInfo->nTilesPerRow = (psInfo->nPixels-1) / psInfo->nTileXSize + 1;
    psInfo->nTilesPerColumn = (psInfo->nLines-1) / psInfo->nTileYSize + 1;

    if (psInfo->nTilesPerRow > INT_MAX / psInfo->nTilesPerColumn)
    {
        CPLError(CE_Failure, CPLE_OutOfMemory, "Too many tiles");
        AIGClose( psInfo );
        return NULL;
    }

/* -------------------------------------------------------------------- */
/*      Setup tile infos, but defer reading of tile data.               */
/* -------------------------------------------------------------------- */
    psInfo->pasTileInfo = (AIGTileInfo *) 
        VSICalloc(sizeof(AIGTileInfo),
                  psInfo->nTilesPerRow * psInfo->nTilesPerColumn);
    if (psInfo->pasTileInfo == NULL)
    {
        CPLError(CE_Failure, CPLE_OutOfMemory, "Cannot allocate tile info array");
        AIGClose( psInfo );
        return NULL;
    }

/* -------------------------------------------------------------------- */
/*      Read the statistics.                                            */
/* -------------------------------------------------------------------- */
    if( AIGReadStatistics( pszCoverName, psInfo ) != CE_None )
    {
        AIGClose( psInfo );
        return NULL;
    }

    return( psInfo );
}

/************************************************************************/
/*                           AIGAccessTile()                            */
/************************************************************************/

CPLErr AIGAccessTile( AIGInfo_t *psInfo, int iTileX, int iTileY )

{
    char szBasename[20];
    char *pszFilename;
    AIGTileInfo *psTInfo;

/* -------------------------------------------------------------------- */
/*      Identify our tile.                                              */
/* -------------------------------------------------------------------- */
    if( iTileX < 0 || iTileX >= psInfo->nTilesPerRow
        || iTileY < 0 || iTileY >= psInfo->nTilesPerColumn )
    {
        CPLAssert( FALSE );
        return CE_Failure;
    }

    psTInfo = psInfo->pasTileInfo + iTileX + iTileY * psInfo->nTilesPerRow;

    if( psTInfo->fpGrid != NULL || psTInfo->bTriedToLoad )
        return CE_None;

/* -------------------------------------------------------------------- */
/*      Compute the basename.                                           */
/* -------------------------------------------------------------------- */
    if( iTileY == 0 )
        sprintf( szBasename, "w%03d001", iTileX + 1 );
    else if( iTileY == 1 )
        sprintf( szBasename, "w%03d000", iTileX + 1 );
    else
        sprintf( szBasename, "z%03d%03d", iTileX + 1, iTileY - 1 );
    
/* -------------------------------------------------------------------- */
/*      Open the file w001001.adf file itself.                          */
/* -------------------------------------------------------------------- */
    pszFilename = (char *) CPLMalloc(strlen(psInfo->pszCoverName)+40);
    sprintf( pszFilename, "%s/%s.adf", psInfo->pszCoverName, szBasename );

    psTInfo->fpGrid = AIGLLOpen( pszFilename, "rb" );
    psTInfo->bTriedToLoad = TRUE;
    
    if( psTInfo->fpGrid == NULL )
    {
        CPLError( CE_Warning, CPLE_OpenFailed,
                  "Failed to open grid file, assuming region is nodata:\n%s\n",
                  pszFilename );
        CPLFree( pszFilename );
        return CE_Warning;
    }

    CPLFree( pszFilename );
    pszFilename = NULL;
    
/* -------------------------------------------------------------------- */
/*      Read the block index file.                                      */
/* -------------------------------------------------------------------- */
    return AIGReadBlockIndex( psInfo, psTInfo, szBasename );
}

/************************************************************************/
/*                            AIGReadTile()                             */
/************************************************************************/

CPLErr AIGReadTile( AIGInfo_t * psInfo, int nBlockXOff, int nBlockYOff,
                    GInt32 *panData )

{
    int		nBlockID;
    CPLErr	eErr;
    int         iTileX, iTileY;
    AIGTileInfo *psTInfo;

/* -------------------------------------------------------------------- */
/*      Compute our tile, and ensure it is accessable (open).  Then     */
/*      reduce block x/y values to be the block within that tile.       */
/* -------------------------------------------------------------------- */
    iTileX = nBlockXOff / psInfo->nBlocksPerRow;
    iTileY = nBlockYOff / psInfo->nBlocksPerColumn;

    eErr = AIGAccessTile( psInfo, iTileX, iTileY );
    if( eErr == CE_Failure )
        return eErr;

    psTInfo = psInfo->pasTileInfo + iTileX + iTileY * psInfo->nTilesPerRow;

    nBlockXOff -= iTileX * psInfo->nBlocksPerRow;
    nBlockYOff -= iTileY * psInfo->nBlocksPerColumn;

/* -------------------------------------------------------------------- */
/*      Request for tile from a file which does not exist - treat as    */
/*      all nodata.                                                     */
/* -------------------------------------------------------------------- */
    if( psTInfo->fpGrid == NULL )
    {
        int i;
        for( i = psInfo->nBlockXSize * psInfo->nBlockYSize - 1; i >= 0; i-- )
            panData[i] = ESRI_GRID_NO_DATA;
        return CE_None;
    }
    
/* -------------------------------------------------------------------- */
/*      validate block id.                                              */
/* -------------------------------------------------------------------- */
    nBlockID = nBlockXOff + nBlockYOff * psInfo->nBlocksPerRow;
    if( nBlockID < 0 
        || nBlockID >= psInfo->nBlocksPerRow * psInfo->nBlocksPerColumn )
    {
        CPLError( CE_Failure, CPLE_AppDefined, 
                  "Illegal block requested." );
        return CE_Failure;
    }

    if( nBlockID >= psTInfo->nBlocks )
    {
        int i;
        CPLDebug( "AIG", 
                  "Request legal block, but from beyond end of block map.\n"
                  "Assuming all nodata." );
        for( i = psInfo->nBlockXSize * psInfo->nBlockYSize - 1; i >= 0; i-- )
            panData[i] = ESRI_GRID_NO_DATA;
        return CE_None;
    }
    
/* -------------------------------------------------------------------- */
/*      Read block.                                                     */
/* -------------------------------------------------------------------- */
    eErr = AIGReadBlock( psTInfo->fpGrid,
                         psTInfo->panBlockOffset[nBlockID],
                         psTInfo->panBlockSize[nBlockID],
                         psInfo->nBlockXSize, psInfo->nBlockYSize,
                         panData, psInfo->nCellType, psInfo->bCompressed );

/* -------------------------------------------------------------------- */
/*      Apply floating point post-processing.                           */
/* -------------------------------------------------------------------- */
    if( eErr == CE_None && psInfo->nCellType == AIG_CELLTYPE_FLOAT )
    {
        float	*pafData = (float *) panData;
        int	i, nPixels = psInfo->nBlockXSize * psInfo->nBlockYSize;

        for( i = 0; i < nPixels; i++ )
        {
            panData[i] = (int) pafData[i];
        }
    }

    return( eErr );
}

/************************************************************************/
/*                          AIGReadFloatTile()                          */
/************************************************************************/

CPLErr AIGReadFloatTile( AIGInfo_t * psInfo, int nBlockXOff, int nBlockYOff,
                         float *pafData )

{
    int		nBlockID;
    CPLErr	eErr;
    int         iTileX, iTileY;
    AIGTileInfo *psTInfo;

/* -------------------------------------------------------------------- */
/*      Compute our tile, and ensure it is accessable (open).  Then     */
/*      reduce block x/y values to be the block within that tile.       */
/* -------------------------------------------------------------------- */
    iTileX = nBlockXOff / psInfo->nBlocksPerRow;
    iTileY = nBlockYOff / psInfo->nBlocksPerColumn;

    eErr = AIGAccessTile( psInfo, iTileX, iTileY );
    if( eErr == CE_Failure )
        return eErr;

    psTInfo = psInfo->pasTileInfo + iTileX + iTileY * psInfo->nTilesPerRow;

    nBlockXOff -= iTileX * psInfo->nBlocksPerRow;
    nBlockYOff -= iTileY * psInfo->nBlocksPerColumn;

/* -------------------------------------------------------------------- */
/*      Request for tile from a file which does not exist - treat as    */
/*      all nodata.                                                     */
/* -------------------------------------------------------------------- */
    if( psTInfo->fpGrid == NULL )
    {
        int i;
        for( i = psInfo->nBlockXSize * psInfo->nBlockYSize - 1; i >= 0; i-- )
            pafData[i] = ESRI_GRID_FLOAT_NO_DATA;
        return CE_None;
    }
    
/* -------------------------------------------------------------------- */
/*      validate block id.                                              */
/* -------------------------------------------------------------------- */
    nBlockID = nBlockXOff + nBlockYOff * psInfo->nBlocksPerRow;
    if( nBlockID < 0 
        || nBlockID >= psInfo->nBlocksPerRow * psInfo->nBlocksPerColumn )
    {
        CPLError( CE_Failure, CPLE_AppDefined, 
                  "Illegal block requested." );
        return CE_Failure;
    }

    if( nBlockID >= psTInfo->nBlocks )
    {
        int i;
        CPLDebug( "AIG", 
                  "Request legal block, but from beyond end of block map.\n"
                  "Assuming all nodata." );
        for( i = psInfo->nBlockXSize * psInfo->nBlockYSize - 1; i >= 0; i-- )
            pafData[i] = ESRI_GRID_FLOAT_NO_DATA;
        return CE_None;
    }
    
/* -------------------------------------------------------------------- */
/*      Read block.                                                     */
/* -------------------------------------------------------------------- */
    eErr = AIGReadBlock( psTInfo->fpGrid,
                         psTInfo->panBlockOffset[nBlockID],
                         psTInfo->panBlockSize[nBlockID],
                         psInfo->nBlockXSize, psInfo->nBlockYSize,
                         (GInt32 *) pafData, psInfo->nCellType, 
                         psInfo->bCompressed );

/* -------------------------------------------------------------------- */
/*      Perform integer post processing.                                */
/* -------------------------------------------------------------------- */
    if( eErr == CE_None && psInfo->nCellType == AIG_CELLTYPE_INT )
    {
        GUInt32	*panData = (GUInt32 *) pafData;
        int	i, nPixels = psInfo->nBlockXSize * psInfo->nBlockYSize;

        for( i = 0; i < nPixels; i++ )
        {
            pafData[i] = (float) panData[i];
        }
    }

    return( eErr );
}

/************************************************************************/
/*                              AIGClose()                              */
/************************************************************************/

void AIGClose( AIGInfo_t * psInfo )

{
    int nTileCount = psInfo->nTilesPerRow * psInfo->nTilesPerColumn;
    int iTile;

    for( iTile = 0; iTile < nTileCount; iTile++ )
    {
        if( psInfo->pasTileInfo[iTile].fpGrid )
        {
            VSIFCloseL( psInfo->pasTileInfo[iTile].fpGrid );

            CPLFree( psInfo->pasTileInfo[iTile].panBlockOffset );
            CPLFree( psInfo->pasTileInfo[iTile].panBlockSize );
        }
    }

    CPLFree( psInfo->pasTileInfo );
    CPLFree( psInfo->pszCoverName );
    CPLFree( psInfo );
}

/************************************************************************/
/*                             AIGLLOpen()                              */
/*                                                                      */
/*      Low level fopen() replacement that will try provided, and       */
/*      upper cased versions of file names.                             */
/************************************************************************/

VSILFILE *AIGLLOpen( const char *pszFilename, const char *pszAccess )

{
    VSILFILE	*fp;

    fp = VSIFOpenL( pszFilename, pszAccess );
    if( fp == NULL )
    {
        char *pszUCFilename = CPLStrdup(pszFilename);
        int  i;

        for( i = strlen(pszUCFilename)-1; 
             pszUCFilename[i] != '/' && pszUCFilename[i] != '\\';
             i-- )
        {
            pszUCFilename[i] = (char) toupper(pszUCFilename[i]);
        }
        
        fp = VSIFOpenL( pszUCFilename, pszAccess );

        CPLFree( pszUCFilename );
    }

    return fp;
}

