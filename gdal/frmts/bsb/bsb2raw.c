/******************************************************************************
 * $Id$
 *
 * Project:  BSB Reader
 * Purpose:  Test program for dumping BSB to PPM raster format.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2001, Frank Warmerdam
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

#include "cpl_error.h"
#include "cpl_vsi.h"
#include "cpl_conv.h"
#include "bsb_read.h"

CPL_CVSID("$Id$")

/************************************************************************/
/*                                main()                                */
/************************************************************************/

int main( int nArgc, char **papszArgv )

{
    BSBInfo	*psInfo;
    int		iLine, i;
    GByte	*pabyScanline;
    FILE	*fp;
    int         nError = 0;

    if( nArgc < 3 )
    {
        fprintf( stderr, "Usage: bsb2raw src_file dst_file\n" );
        exit( 1 );
    }

    psInfo = BSBOpen( papszArgv[1] );
    if( psInfo == NULL )
        exit( 1 );

    fp = VSIFOpen( papszArgv[2], "wb" );
    if( fp == NULL )
    {
        perror( "open" );
        exit( 1 );
    }

    pabyScanline = (GByte *) CPLMalloc(psInfo->nXSize);
    for( iLine = 0; iLine < psInfo->nYSize; iLine++ )
    {
        if( !BSBReadScanline( psInfo, iLine, pabyScanline ) )
            nError++;

        VSIFWrite( pabyScanline, 1, psInfo->nXSize, fp );
    }

    VSIFClose( fp );

    if( nError > 0 )
        fprintf( stderr, "Read failed for %d scanlines out of %d.\n",
                 nError, psInfo->nYSize );

/* -------------------------------------------------------------------- */
/*      Write .aux file.                                                */
/* -------------------------------------------------------------------- */
    fp = VSIFOpen( CPLResetExtension( papszArgv[2], "aux" ), "wt" );

    fprintf( fp, "AuxilaryTarget: %s\n",
             CPLGetFilename(papszArgv[2]) );

    fprintf( fp, "RawDefinition: %d %d 1\n",
             psInfo->nXSize, psInfo->nYSize );

    fprintf( fp, "ChanDefinition-1: 8U 0 1 %d Swapped\n",
             psInfo->nXSize );

    for( i = 0; i < psInfo->nPCTSize; i++ )
        fprintf( fp, "METADATA_IMG_1_Class_%d_Color: (RGB:%d %d %d)\n",
                 i,
                 psInfo->pabyPCT[i*3 + 0],
                 psInfo->pabyPCT[i*3 + 1],
                 psInfo->pabyPCT[i*3 + 2] );

    VSIFClose( fp );

    exit( 0 );
}
