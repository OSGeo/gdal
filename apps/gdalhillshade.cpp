/******************************************************************************
 * $Id$
 *
 * Project:  GDAL Utilities
 * Purpose:  GDAL illshade program
 * Author:   Matthew Perry, perrygeo at gmail.com
 *
 ******************************************************************************
 * Copyright (c) 2006, 2009 Matthew Perry 
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


#include <stdlib.h>
#include <math.h>

#include "cpl_conv.h"
#include "cpl_string.h"
#include "gdal.h"


CPL_CVSID("$Id$");

/************************************************************************/
/*                               Usage()                                */
/************************************************************************/

static void Usage()

{
    printf( " \n Generates a shaded relief map from any GDAL-supported elevation raster\n"
            " Usage: \n"
            "   hillshade input_dem output_hillshade \n"
            "                 [-z ZFactor (default=1)] [-s scale* (default=1)] \n"
            "                 [-az Azimuth (default=315)] [-alt Altitude (default=45)] [-b Band (default=1)]\n\n"
            " Notes : \n"
            "   Scale for Feet:Latlong use scale=370400, for Meters:LatLong use scale=111120 \n\n");
    exit( 1 );
}

void Hillshade( GDALRasterBandH hSrcBand,
                GDALRasterBandH hDstBand,
                int nXSize,
                int nYSize,
                double* adfGeoTransform,
                double z,
                double scale,
                double alt,
                double az)
{
    int bContainsNull;
    const double radiansToDegrees = 180.0 / M_PI;
    const double degreesToRadians = M_PI / 180.0;
    float *pafWin;
    float *pafShadeBuf;

    double x, y, aspect, slope, cang;

    int bSrcHasNoData;
    double dfSrcNoDataValue = 0.0;

    double   nsres = adfGeoTransform[5];
    double   ewres = adfGeoTransform[1];

    pafShadeBuf = (float *) CPLMalloc(sizeof(float)*nXSize);
    pafWin  = (float *) CPLMalloc(sizeof(float)*9);

    dfSrcNoDataValue = GDALGetRasterNoDataValue(hSrcBand, &bSrcHasNoData);

    // Move a 3x3 pafWindow over each cell 
    // (where the cell in question is #4)
    // 
    //      0 1 2
    //      3 4 5
    //      6 7 8

    for (int  i = 0; i < nYSize; i++) {
        for (int j = 0; j < nXSize; j++) {
            bContainsNull = FALSE;

            // Exclude the edges
            if (i == 0 || j == 0 || i == nYSize-1 || j == nXSize-1 )
            {
                // We are at the edge so write nullValue and move on
                pafShadeBuf[j] = 0;
                continue;
            }


            // Read in 3x3 pafWindow
            GDALRasterIO(   hSrcBand,
                            GF_Read,
                            j-1, i-1,
                            3, 3,
                            pafWin,
                            3, 3,
                            GDT_Float32,
                            0, 0);

            // Check if pafWindow has null value
            for ( int n = 0; n <= 8; n++) {
                if(bSrcHasNoData && pafWin[n] == dfSrcNoDataValue) {
                    bContainsNull = TRUE;
                    break;
                }
            }

            if (bContainsNull) {
                // We have nulls so write nullValue and move on
                pafShadeBuf[j] = 0;
                continue;
            } else {
                // We have a valid 3x3 pafWindow.

                /* ---------------------------------------
                * Compute Hillshade
                */

                // First Slope ...
                x = ((z*pafWin[0] + z*pafWin[3] + z*pafWin[3] + z*pafWin[6]) -
                     (z*pafWin[2] + z*pafWin[5] + z*pafWin[5] + z*pafWin[8])) /
                    (8.0 * ewres * scale);

                y = ((z*pafWin[6] + z*pafWin[7] + z*pafWin[7] + z*pafWin[8]) -
                     (z*pafWin[0] + z*pafWin[1] + z*pafWin[1] + z*pafWin[2])) /
                    (8.0 * nsres * scale);

                slope = 90.0 - atan(sqrt(x*x + y*y))*radiansToDegrees;

                // ... then aspect...
                aspect = atan2(x,y);

                // ... then the shade value
                cang = sin(alt*degreesToRadians) * sin(slope*degreesToRadians) +
                       cos(alt*degreesToRadians) * cos(slope*degreesToRadians) *
                       cos((az-90.0)*degreesToRadians - aspect);

                if (cang <= 0.0) 
                    cang = 1.0;
                else
                    cang = 1.0 + (254.0 * cang);

                pafShadeBuf[j] = cang;

            }
        }

        /* -----------------------------------------
         * Write Line to Raster
         */
        GDALRasterIO(hDstBand,
                           GF_Write,
                           0, i, nXSize,
                           1, pafShadeBuf, nXSize, 1, GDT_Float32, 0, 0);
    }

    CPLFree(pafShadeBuf);
    CPLFree(pafWin);
}

/************************************************************************/
/*                                main()                                */
/************************************************************************/

int main( int argc, char ** argv )

{
    double z = 1.0;
    double scale = 1.0;
    double az = 315.0;
    double alt = 45.0;
    
    int nBand = 1;
    double  adfGeoTransform[6];

    const char *pszSrcFilename = NULL;
    const char *pszDstFilename = NULL;
    const char *pszFormat = "GTiff";
    char **papszOptions = NULL;
    
    GDALDatasetH hSrcDataset = NULL;
    GDALDatasetH hDstDataset = NULL;
    GDALRasterBandH hSrcBand = NULL;
    GDALRasterBandH hDstBand = NULL;
    GDALDriverH hGTiffDriver = NULL;
    
    int nXSize = 0;
    int nYSize = 0;
    
    /* Check that we are running against at least GDAL 1.4 */
    /* Note to developers : if we use newer API, please change the requirement */
    if (atoi(GDALVersionInfo("VERSION_NUM")) < 1400)
    {
        fprintf(stderr, "At least, GDAL >= 1.4.0 is required for this version of %s, "
                "which was compiled against GDAL %s\n", argv[0], GDAL_RELEASE_NAME);
        exit(1);
    }

    argc = GDALGeneralCmdLineProcessor( argc, &argv, 0 );
    if( argc < 1 )
        exit( 1 );

/* -------------------------------------------------------------------- */
/*      Parse arguments.                                                */
/* -------------------------------------------------------------------- */
    for(int i = 1; i < argc; i++ )
    {
        if( EQUAL(argv[i], "--utility_version") || EQUAL(argv[i], "--utility-version") )
        {
            printf("%s was compiled against GDAL %s and is running against GDAL %s\n",
                   argv[0], GDAL_RELEASE_NAME, GDALVersionInfo("RELEASE_NAME"));
            return 0;
        }

        if( i + 1 < argc && (EQUAL(argv[i], "--z") || EQUAL(argv[i], "-z")))
        {
            z = atof(argv[++i]);
        }
        if( i + 1 < argc &&
            (EQUAL(argv[i], "--s") || 
             EQUAL(argv[i], "-s") ||
             EQUAL(argv[i], "--scale") ||
             EQUAL(argv[i], "-scale"))
          )
        {
            scale = atof(argv[++i]);
        }
        if( i + 1 < argc &&
            (EQUAL(argv[i], "--az") || 
             EQUAL(argv[i], "-az") ||
             EQUAL(argv[i], "--azimuth") ||
             EQUAL(argv[i], "-azimuth"))
          )
        {
            az = atof(argv[++i]);
        }
        if( i + 1 < argc &&
            (EQUAL(argv[i], "--alt") || 
             EQUAL(argv[i], "-alt") ||
             EQUAL(argv[i], "--alt") ||
             EQUAL(argv[i], "-alt"))
          )
        {
            alt = atof(argv[++i]);
        }
        if( i + 1 < argc &&
            (EQUAL(argv[i], "--b") || 
             EQUAL(argv[i], "-b"))
          )
        {
            nBand = atof(argv[++i]);
        }
        else if( pszSrcFilename == NULL )
        {
            pszSrcFilename = argv[i];
        }
        else if( pszDstFilename == NULL )
        {
            pszDstFilename = argv[i];
        }
        else
            Usage();
    }

    if( pszSrcFilename == NULL || pszDstFilename == NULL )
    {
        fprintf( stderr, "Missing source or destination.\n\n" );
        Usage();
    }

    GDALAllRegister();

    // Open Dataset and get raster band
    hSrcDataset = GDALOpen( pszSrcFilename, GA_ReadOnly );
    
    if( hSrcDataset == NULL )
    {
        fprintf( stderr,
                 "GDALOpen failed - %d\n%s\n",
                 CPLGetLastErrorNo(), CPLGetLastErrorMsg() );
        GDALDestroyDriverManager();
        exit( 1 );
    }

    nXSize = GDALGetRasterXSize(hSrcDataset);
    nYSize = GDALGetRasterYSize(hSrcDataset);    

    hSrcBand = GDALGetRasterBand( hSrcDataset, nBand );

    if( hSrcBand == NULL )
    {
        fprintf( stderr,
                 "Unable to fetch band #%d- %d\n%s\n", nBand,
                 CPLGetLastErrorNo(), CPLGetLastErrorMsg() );
        GDALDestroyDriverManager();
        exit( 1 );
    }
    
    GDALGetGeoTransform(hSrcDataset, adfGeoTransform);
    hGTiffDriver = GDALGetDriverByName(pszFormat);

    hDstDataset = GDALCreate(   hGTiffDriver,
                                pszDstFilename,
                                nXSize,
                                nYSize,
                                1,
                                GDT_Byte,
                                papszOptions);

    if( hDstDataset == NULL )
    {
        fprintf( stderr,
                 "Unable to create dataset %s %d\n%s\n", pszDstFilename,
                 CPLGetLastErrorNo(), CPLGetLastErrorMsg() );
        GDALDestroyDriverManager();
        exit( 1 );
    }
    
    hDstBand = GDALGetRasterBand( hDstDataset, 1 );

    GDALSetGeoTransform(hDstDataset, adfGeoTransform);
    GDALSetProjection(hDstDataset, GDALGetProjectionRef(hSrcDataset));
    GDALSetRasterNoDataValue(hDstBand, 0);

    Hillshade(  hSrcBand, 
                hDstBand, 
                nXSize, 
                nYSize,
                adfGeoTransform,
                z,
                scale,
                alt,
                az);

    GDALClose(hSrcDataset);
    GDALClose(hDstDataset);

    GDALDestroyDriverManager();
    CSLDestroy( argv );

    return 0;
}

