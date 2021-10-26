/******************************************************************************
 * $Id$
 *
 * Project:  Arc/Info Binary Grid Translator
 * Purpose:  Grid file access include file.
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
 ****************************************************************************/

#ifndef AIGRID_H_INCLUDED
#define AIGRID_H_INCLUDED

#include "cpl_conv.h"

CPL_C_START

#define ESRI_GRID_NO_DATA -2147483647
/*#define ESRI_GRID_FLOAT_NO_DATA -340282306073709652508363335590014353408.0 */
#define ESRI_GRID_FLOAT_NO_DATA -340282346638528859811704183484516925440.0

/* ==================================================================== */
/*      Grid Instance                                                   */
/* ==================================================================== */

typedef struct {
    int         nBlocks;
    GUInt32     *panBlockOffset;
    int         *panBlockSize;

    VSILFILE    *fpGrid;  // The w001001.adf file.
    int         bTriedToLoad;
} AIGTileInfo;

typedef struct {
    /* Private information */

    AIGTileInfo *pasTileInfo;

    int         bHasWarned;
    int         nFailedOpenings;

    /* public information */

    char        *pszCoverName; // Path of coverage directory.

    GInt32      nCellType;
    GInt32      bCompressed;

#define AIG_CELLTYPE_INT                1
#define AIG_CELLTYPE_FLOAT              2

    GInt32      nBlockXSize;
    GInt32      nBlockYSize;

    GInt32      nBlocksPerRow;
    GInt32      nBlocksPerColumn;

    int         nTileXSize;
    int         nTileYSize;

    int         nTilesPerRow;
    int         nTilesPerColumn;

    double      dfLLX;
    double      dfLLY;
    double      dfURX;
    double      dfURY;

    double      dfCellSizeX;
    double      dfCellSizeY;

    int         nPixels;
    int         nLines;

    double      dfMin;
    double      dfMax;
    double      dfMean;
    double      dfStdDev;

} AIGInfo_t;

/* ==================================================================== */
/*      Private APIs                                                    */
/* ==================================================================== */

CPLErr AIGAccessTile( AIGInfo_t *psInfo, int iTileX, int iTileY );
CPLErr AIGReadBlock( VSILFILE * fp, GUInt32 nBlockOffset, int nBlockSize,
                     int nBlockXSize, int nBlockYSize, GInt32 * panData,
                     int nCellType, int bCompressed );

CPLErr AIGReadHeader( const char *, AIGInfo_t * );
CPLErr AIGReadBlockIndex( AIGInfo_t *, AIGTileInfo *,
                          const char *pszBasename );
CPLErr AIGReadBounds( const char *, AIGInfo_t * );
CPLErr AIGReadStatistics( const char *, AIGInfo_t * );

CPLErr DecompressCCITTRLETile( unsigned char *pabySrcData, int nSrcBytes,
                               unsigned char *pabyDstData, int nDstBytes,
                               int nBlockXSize, int nBlockYSize );

/* ==================================================================== */
/*      Public APIs                                                     */
/* ==================================================================== */

AIGInfo_t       *AIGOpen( const char *, const char * );

CPLErr          AIGReadTile( AIGInfo_t *, int, int, GInt32 * );
CPLErr          AIGReadFloatTile( AIGInfo_t *, int, int, float * );

void            AIGClose( AIGInfo_t * );

VSILFILE           *AIGLLOpen( const char *, const char * );

CPL_C_END

#endif /* ndef AIGRID_H_INCLUDED */
