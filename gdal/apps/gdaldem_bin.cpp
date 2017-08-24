/******************************************************************************
 *
 * Project:  GDAL DEM Utilities
 * Purpose:
 * Authors:  Even Rouault, <even dot rouault at spatialys dot com>
 *
 ******************************************************************************
 * Copyright (c) 2015, Even Rouault <even dot rouault at spatialys dot com>
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

#include "cpl_vsi.h"
#include <stdlib.h>
#include <math.h>

#include "cpl_conv.h"
#include "cpl_string.h"
#include "gdal_utils_priv.h"
#include "gdal_priv.h"
#include "commonutils.h"

CPL_CVSID("$Id$")

/************************************************************************/
/*                               Usage()                                */
/************************************************************************/

static void Usage(const char* pszErrorMsg = NULL)

{
    printf( " Usage: \n"
            " - To generate a shaded relief map from any GDAL-supported elevation raster : \n\n"
            "     gdaldem hillshade input_dem output_hillshade \n"
            "                 [-z ZFactor (default=1)] [-s scale* (default=1)] \n"
            "                 [-az Azimuth (default=315)] [-alt Altitude (default=45)]\n"
            "                 [-alg ZevenbergenThorne] [-combined | -multidirectional]\n"
            "                 [-compute_edges] [-b Band (default=1)] [-of format] [-co \"NAME=VALUE\"]* [-q]\n"
            "\n"
            " - To generates a slope map from any GDAL-supported elevation raster :\n\n"
            "     gdaldem slope input_dem output_slope_map \n"
            "                 [-p use percent slope (default=degrees)] [-s scale* (default=1)]\n"
            "                 [-alg ZevenbergenThorne]\n"
            "                 [-compute_edges] [-b Band (default=1)] [-of format] [-co \"NAME=VALUE\"]* [-q]\n"
            "\n"
            " - To generate an aspect map from any GDAL-supported elevation raster\n"
            "   Outputs a 32-bit float tiff with pixel values from 0-360 indicating azimuth :\n\n"
            "     gdaldem aspect input_dem output_aspect_map \n"
            "                 [-trigonometric] [-zero_for_flat]\n"
            "                 [-alg ZevenbergenThorne]\n"
            "                 [-compute_edges] [-b Band (default=1)] [-of format] [-co \"NAME=VALUE\"]* [-q]\n"
            "\n"
            " - To generate a color relief map from any GDAL-supported elevation raster\n"
            "     gdaldem color-relief input_dem color_text_file output_color_relief_map\n"
            "                 [-alpha] [-exact_color_entry | -nearest_color_entry]\n"
            "                 [-b Band (default=1)] [-of format] [-co \"NAME=VALUE\"]* [-q]\n"
            "     where color_text_file contains lines of the format \"elevation_value red green blue\"\n"
            "\n"
            " - To generate a Terrain Ruggedness Index (TRI) map from any GDAL-supported elevation raster\n"
            "     gdaldem TRI input_dem output_TRI_map\n"
            "                 [-compute_edges] [-b Band (default=1)] [-of format] [-co \"NAME=VALUE\"]* [-q]\n"
            "\n"
            " - To generate a Topographic Position Index (TPI) map from any GDAL-supported elevation raster\n"
            "     gdaldem TPI input_dem output_TPI_map\n"
            "                 [-compute_edges] [-b Band (default=1)] [-of format] [-co \"NAME=VALUE\"]* [-q]\n"
            "\n"
            " - To generate a roughness map from any GDAL-supported elevation raster\n"
            "     gdaldem roughness input_dem output_roughness_map\n"
            "                 [-compute_edges] [-b Band (default=1)] [-of format] [-co \"NAME=VALUE\"]* [-q]\n"
            "\n"
            " Notes : \n"
            "   Scale is the ratio of vertical units to horizontal\n"
            "    for Feet:Latlong use scale=370400, for Meters:LatLong use scale=111120 \n\n");

    if( pszErrorMsg != NULL )
        fprintf(stderr, "\nFAILURE: %s\n", pszErrorMsg);

    exit( 1 );
}

/************************************************************************/
/*                       GDALDEMProcessingOptionsForBinaryNew()             */
/************************************************************************/

static GDALDEMProcessingOptionsForBinary *GDALDEMProcessingOptionsForBinaryNew(void)
{
    return static_cast<GDALDEMProcessingOptionsForBinary *>(
        CPLCalloc(1, sizeof(GDALDEMProcessingOptionsForBinary)));
}

/************************************************************************/
/*                       GDALDEMProcessingOptionsForBinaryFree()            */
/************************************************************************/

static void GDALDEMProcessingOptionsForBinaryFree(
    GDALDEMProcessingOptionsForBinary* psOptionsForBinary )
{
    if( psOptionsForBinary )
    {
        CPLFree(psOptionsForBinary->pszProcessing);
        CPLFree(psOptionsForBinary->pszSrcFilename);
        CPLFree(psOptionsForBinary->pszColorFilename);
        CPLFree(psOptionsForBinary->pszDstFilename);
        CPLFree(psOptionsForBinary->pszFormat);
        CPLFree(psOptionsForBinary);
    }
}
/************************************************************************/
/*                                main()                                */
/************************************************************************/

int main( int argc, char ** argv )

{
    /* Check strict compilation and runtime library version as we use C++ API */
    if( ! GDAL_CHECK_VERSION(argv[0]) )
        exit(1);

    EarlySetConfigOptions(argc, argv);

/* -------------------------------------------------------------------- */
/*      Register standard GDAL drivers, and process generic GDAL        */
/*      command options.                                                */
/* -------------------------------------------------------------------- */
    GDALAllRegister();
    argc = GDALGeneralCmdLineProcessor( argc, &argv, 0 );
    if( argc < 2 )
    {
        Usage("Not enough arguments.");
    }

    if( EQUAL(argv[1], "--utility_version") ||
        EQUAL(argv[1], "--utility-version") )
    {
        printf("%s was compiled against GDAL %s and is running against GDAL %s\n",
                argv[0], GDAL_RELEASE_NAME, GDALVersionInfo("RELEASE_NAME"));
        CSLDestroy( argv );
        return 0;
    }
    else if( EQUAL(argv[1],"--help") )
        Usage();

    GDALDEMProcessingOptionsForBinary* psOptionsForBinary =
        GDALDEMProcessingOptionsForBinaryNew();
    GDALDEMProcessingOptions *psOptions =
        GDALDEMProcessingOptionsNew(argv + 1, psOptionsForBinary);
    CSLDestroy( argv );

    if( psOptions == NULL )
    {
        Usage();
    }

    if( !(psOptionsForBinary->bQuiet) )
    {
        GDALDEMProcessingOptionsSetProgress(psOptions, GDALTermProgress, NULL);
    }

    if( psOptionsForBinary->pszSrcFilename == NULL )
    {
        Usage("Missing source.");
    }
    if ( EQUAL(psOptionsForBinary->pszProcessing, "color-relief") &&
         psOptionsForBinary->pszColorFilename == NULL )
    {
        Usage("Missing color file.");
    }
    if( psOptionsForBinary->pszDstFilename == NULL )
    {
        Usage("Missing destination.");
    }

    if( !psOptionsForBinary->bQuiet &&
        !psOptionsForBinary->bFormatExplicitlySet)
    {
        CheckExtensionConsistency(psOptionsForBinary->pszDstFilename,
                                  psOptionsForBinary->pszFormat);
    }

    // Open Dataset and get raster band.
    GDALDatasetH hSrcDataset =
        GDALOpen( psOptionsForBinary->pszSrcFilename, GA_ReadOnly );

    if( hSrcDataset == NULL )
    {
        fprintf( stderr,
                 "GDALOpen failed - %d\n%s\n",
                 CPLGetLastErrorNo(), CPLGetLastErrorMsg() );
        GDALDestroyDriverManager();
        exit( 1 );
    }

    int bUsageError = FALSE;
    GDALDatasetH hOutDS =
        GDALDEMProcessing(psOptionsForBinary->pszDstFilename, hSrcDataset,
                          psOptionsForBinary->pszProcessing,
                          psOptionsForBinary->pszColorFilename,
                          psOptions, &bUsageError);
    if( bUsageError )
        Usage();
    const int nRetCode = hOutDS ? 0 : 1;

    GDALClose(hSrcDataset);
    GDALClose(hOutDS);
    GDALDEMProcessingOptionsFree(psOptions);
    GDALDEMProcessingOptionsForBinaryFree(psOptionsForBinary);

    GDALDestroyDriverManager();

    return nRetCode;
}
