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
 * Revision 1.1  1999/02/03 14:12:56  warmerda
 * New
 *
 */


#ifndef _AIGRID_H_INCLUDED
#define _AIGRID_H_INCLUDED

#include "cpl_conv.h"

#define GRID_NO_DATA 65536

typedef struct {
    int		nBlockXSize;
    int		nBlockYSize;
    
    int		nBlocksPerRow;
    int		nBlocksPerColumn;

    int		nBlocks;
    int		*panBlockOffset;
    int		*panBlockSize;

    FILE	*fpGrid;	/* the w001001.adf file */

} AIGInfo_t;

CPLErr AIGReadBlock( FILE * fp, int nBlockOffset, int nBlockSize,
                     int nBlockXSize, int nBlockYSize, GUInt32 * panData );

CPLErr AIGReadHeader( const char *, AIGInfo_t * );
CPLErr AIGReadBlockIndex( const char *, AIGInfo_t * );

/************************************************************************/
/*                              Public API                              */
/************************************************************************/

AIGInfo_t	*AIGOpen( const char *, const char * );

void		AIGClose( AIGInfo_t * );

#endif /* ndef _AIGRID_H_INCLUDED */
