/******************************************************************************
 * $Id$
 *
 * Project:  Arc/Info Binary Grid Translator
 * Purpose:  Grid file access include file.
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
 * Revision 1.8  2000/11/09 06:22:40  warmerda
 * save cover name
 *
 * Revision 1.7  2000/07/18 13:58:58  warmerda
 * no RTileType for float tiles
 *
 * Revision 1.6  2000/04/20 14:05:02  warmerda
 * added more raw float magic codes
 *
 * Revision 1.5  2000/02/18 05:04:50  warmerda
 * added bHasWarned
 *
 * Revision 1.4  1999/08/12 19:10:54  warmerda
 * added ESRI_GRID_NO_DATA
 *
 * Revision 1.3  1999/04/21 16:51:30  warmerda
 * fixed up floating point support
 *
 * Revision 1.2  1999/02/04 22:15:33  warmerda
 * fleshed out implementation
 *
 * Revision 1.1  1999/02/03 14:12:56  warmerda
 * New
 *
 */

#ifndef _AIGRID_H_INCLUDED
#define _AIGRID_H_INCLUDED

#include "cpl_conv.h"

CPL_C_START

#define GRID_NO_DATA 65536

#define ESRI_GRID_NO_DATA -2147483647

/* ==================================================================== */
/*      Grid Instance                                                   */
/* ==================================================================== */
typedef struct {
    /* Private information */
    
    int		nBlocks;
    int		*panBlockOffset;
    int		*panBlockSize;

    FILE	*fpGrid;	/* the w001001.adf file */

    int		bHasWarned;

    /* public information */

    char	*pszCoverName; /* path of coverage directory */

    int		nCellType;

#define AIG_CELLTYPE_INT		1
#define AIG_CELLTYPE_FLOAT		2    
    
    int		nBlockXSize;
    int		nBlockYSize;
    
    int		nBlocksPerRow;
    int		nBlocksPerColumn;

    double	dfLLX;
    double	dfLLY;
    double	dfURX;
    double	dfURY;

    double	dfCellSizeX;
    double	dfCellSizeY;

    int		nPixels;
    int		nLines;

    double	dfMin;
    double	dfMax;
    double	dfMean;
    double	dfStdDev;

} AIGInfo_t;

/* ==================================================================== */
/*      Private APIs                                                    */
/* ==================================================================== */

CPLErr AIGReadBlock( FILE * fp, int nBlockOffset, int nBlockSize,
                     int nBlockXSize, int nBlockYSize, GUInt32 * panData,
                     int nCellType );

CPLErr AIGReadHeader( const char *, AIGInfo_t * );
CPLErr AIGReadBlockIndex( const char *, AIGInfo_t * );
CPLErr AIGReadBounds( const char *, AIGInfo_t * );
CPLErr AIGReadStatistics( const char *, AIGInfo_t * );

/* ==================================================================== */
/*      Public APIs                                                     */
/* ==================================================================== */

AIGInfo_t	*AIGOpen( const char *, const char * );

CPLErr 		AIGReadTile( AIGInfo_t *, int, int, GUInt32 * );
CPLErr 		AIGReadFloatTile( AIGInfo_t *, int, int, float * );

void		AIGClose( AIGInfo_t * );

CPL_C_END

#endif /* ndef _AIGRID_H_INCLUDED */
