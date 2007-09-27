/******************************************************************************
 * $Id$
 *
 * Project:  DTED Translator
 * Purpose:  Test mainline for DTED writer.
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
 *****************************************************************************/

#include "gdal.h"
#include "dted_api.h"

/************************************************************************/
/*                               Usage()                                */
/************************************************************************/

static void Usage()

{
    printf( "Usage: dted_test [-trim] [-fill n] [-level n] <in_file> [<out_level>]\n" );
    exit(0);
}

/************************************************************************/
/*                                main()                                */
/************************************************************************/
int main( int argc, char ** argv )

{
    GDALDatasetH hSrcDS;
    int         iY, iX, nOutLevel=0, nXSize, nYSize, iArg, nFillDist=0;
    void        *pStream;
    GInt16      *panData;
    const char  *pszFilename = NULL;
    GDALRasterBandH hSrcBand;
    double       adfGeoTransform[6];
    int          bEnableTrim = FALSE;
    GInt16       noDataValue = 0;
    int          bHasNoData;

/* -------------------------------------------------------------------- */
/*      Identify arguments.                                             */
/* -------------------------------------------------------------------- */

    for( iArg = 1; iArg < argc; iArg++ )
    {
        if( EQUAL(argv[iArg],"-trim") )
            bEnableTrim = TRUE;

        else if( EQUAL(argv[iArg],"-fill") )
            nFillDist = atoi(argv[++iArg]);

        else if( EQUAL(argv[iArg],"-level") )
            nOutLevel = atoi(argv[++iArg]);
        else
        {
            if( pszFilename != NULL )
                Usage();
            pszFilename = argv[iArg];
        }
    }		

    if( pszFilename == NULL )
        Usage();

/* -------------------------------------------------------------------- */
/*      Open input file.                                                */
/* -------------------------------------------------------------------- */
    GDALAllRegister();
    hSrcDS = GDALOpen( pszFilename, GA_ReadOnly );
    if( hSrcDS == NULL )
        exit(1);

    hSrcBand = GDALGetRasterBand( hSrcDS, 1 );

    noDataValue = (GInt16)GDALGetRasterNoDataValue(hSrcBand, &bHasNoData);

    nXSize = GDALGetRasterXSize( hSrcDS );
    nYSize = GDALGetRasterYSize( hSrcDS );

    GDALGetGeoTransform( hSrcDS, adfGeoTransform );

/* -------------------------------------------------------------------- */
/*      Create output stream.                                           */
/* -------------------------------------------------------------------- */
    pStream = DTEDCreatePtStream( ".", nOutLevel );

    if( pStream == NULL )
        exit( 1 );

/* -------------------------------------------------------------------- */
/*      Process all the profiles.                                       */
/* -------------------------------------------------------------------- */
    panData = (GInt16 *) malloc(sizeof(GInt16) * nXSize);

    for( iY = 0; iY < nYSize; iY++ )
    {
        GDALRasterIO( hSrcBand, GF_Read, 0, iY, nXSize, 1, 
                      panData, nXSize, 1, GDT_Int16, 0, 0 );

        if (bHasNoData)
        {
            for( iX = 0; iX < nXSize; iX++ )
            {
                if (panData[iX] == noDataValue)
                    panData[iX] = DTED_NODATA_VALUE;
            }
        }

        for( iX = 0; iX < nXSize; iX++ )
        {
            DTEDWritePt( pStream, 
                         adfGeoTransform[0] 
                         + adfGeoTransform[1] * (iX + 0.5)
                         + adfGeoTransform[2] * (iY + 0.5),
                         adfGeoTransform[3] 
                         + adfGeoTransform[4] * (iX + 0.5)
                         + adfGeoTransform[5] * (iY + 0.5),
                         panData[iX] );
        }
    }

    free( panData );

/* -------------------------------------------------------------------- */
/*      Cleanup.                                                        */
/* -------------------------------------------------------------------- */
    if( bEnableTrim )
        DTEDPtStreamTrimEdgeOnlyTiles( pStream );

    if( nFillDist > 0 )
        DTEDFillPtStream( pStream, nFillDist );

    DTEDClosePtStream( pStream );
    GDALClose( hSrcDS );

    exit( 0 );
}
