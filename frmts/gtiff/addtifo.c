/******************************************************************************
 * $Id$
 *
 * Project:  GeoTIFF Overview Builder
 * Purpose:  Mainline for building overviews in a TIFF file.
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
 * Revision 1.2  1999/02/11 22:27:12  warmerda
 * Added multi-sample support
 *
 * Revision 1.1  1999/02/11 18:12:30  warmerda
 * New
 */

#include <stdio.h>
#include <stdlib.h>
#include "tiffio.h"

void TIFF_BuildOverviews( const char *, int, int * );

/************************************************************************/
/*                                main()                                */
/************************************************************************/

int main( int argc, char ** argv )

{
    int		anOverviews[100];
    int		nOverviewCount = 0;

/* -------------------------------------------------------------------- */
/*      Usage:                                                          */
/* -------------------------------------------------------------------- */
    if( argc < 2 )
    {
        printf( "Usage: addtifo tiff_filename [resolution_reductions]\n" );
        printf( "\n" );
        printf( "Example:\n" );
        printf( " %% addtifo abc.tif 2 4 8 16\n" );
        exit( 1 );
    }

/* -------------------------------------------------------------------- */
/*      Collect the user requested reduction factors.                   */
/* -------------------------------------------------------------------- */
    while( nOverviewCount < argc - 2 && nOverviewCount < 100 )
    {
        anOverviews[nOverviewCount] = atoi(argv[nOverviewCount+2]);
        nOverviewCount++;
    }

/* -------------------------------------------------------------------- */
/*      Default to four overview levels.  It would be nicer if it       */
/*      defaulted based on the size of the source image.                */
/* -------------------------------------------------------------------- */
    if( nOverviewCount == 0 )
    {
        nOverviewCount = 4;
        
        anOverviews[0] = 2;
        anOverviews[1] = 4;
        anOverviews[2] = 8;
        anOverviews[3] = 16;
    }

/* -------------------------------------------------------------------- */
/*      Build the overview.                                             */
/* -------------------------------------------------------------------- */
    TIFF_BuildOverviews( argv[1], nOverviewCount, anOverviews );
    
/* -------------------------------------------------------------------- */
/*      Optionally test for memory leaks.                               */
/* -------------------------------------------------------------------- */
#ifdef DBMALLOC
    malloc_dump(1);
#endif
    
    exit( 0 );
}
