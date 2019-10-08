/******************************************************************************
 *
 * Project:  Viewshed Generator
 * Purpose:  Viewshed Generator mainline.
 * Author:   Tamas Szekeres <szekerest@gmail.com>
 *
 ******************************************************************************
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

#include "cpl_conv.h"
#include "cpl_string.h"
#include "gdal_version.h"
#include "gdal.h"
#include "gdal_alg.h"
#include "ogr_api.h"
#include "ogr_srs_api.h"
#include "commonutils.h"

CPL_CVSID("$Id$")

/************************************************************************/
/*                               Usage()                                */
/************************************************************************/

static void Usage(const char* pszErrorMsg = nullptr)

{
    printf(
        "Usage: gdal_viewshed [-b <band>] [-a <attribute_name>] [-3d] [-inodata]\n"
        "                    [-snodata n] [-f <formatname>] [-tr <target_raster_filename>]\n"
        "                    [-oz <observer_height>] [-tz <target_height>] [-md <max_distance>]\n"
        "                    [-ox <observer_x> -oy <observer_y>]\n"
        "                    [[-dsco NAME=VALUE] ...] [[-lco NAME=VALUE] ...]\n"
        "                    [-nln <outlayername>] [-q]\n"
        "                    <src_filename> <dst_filename>\n" );

    if( pszErrorMsg != nullptr )
        fprintf(stderr, "\nFAILURE: %s\n", pszErrorMsg);

    exit( 1 );
}

/************************************************************************/
/*                                main()                                */
/************************************************************************/

#define CHECK_HAS_ENOUGH_ADDITIONAL_ARGS(nExtraArg) \
    do { if (i + nExtraArg >= argc) \
        Usage(CPLSPrintf("%s option requires %d argument(s)", \
                         argv[i], nExtraArg)); } while( false )

MAIN_START(argc, argv)

{
    int nBandIn = 1;
    double dfObserverHeight = 2.0;
    double dfTargetHeight = 0.0;
    double dfMaxDistance = 0.0;
    double dfObserverX = 0.0;
    double dfObserverY = 0.0;
    double dfVisibleVal = 255.0;
    double dfInvisibleVal = 0.0;
    double dfOutOfRangeVal = -1.0;
    double dfNoDataVal = 0.0;
    double dfCurvCoeff = 0.0;
    const char *pszSrcFilename = nullptr;
    const char *pszDstFilename = nullptr;
    bool bQuiet = false;
    GDALProgressFunc pfnProgress = nullptr;

    // Check that we are running against at least GDAL 1.4.
    // Note to developers: if we use newer API, please change the requirement.
    if (atoi(GDALVersionInfo("VERSION_NUM")) < 1400)
    {
        fprintf(stderr,
                "At least, GDAL >= 1.4.0 is required for this version of %s, "
                "which was compiled against GDAL %s\n",
                argv[0], GDAL_RELEASE_NAME);
        exit(1);
    }

    GDALAllRegister();

    argc = GDALGeneralCmdLineProcessor( argc, &argv, 0 );

/* -------------------------------------------------------------------- */
/*      Parse arguments.                                                */
/* -------------------------------------------------------------------- */
    for( int i = 1; i < argc; i++ )
    {
        if( EQUAL(argv[i], "--utility_version") )
        {
            printf("%s was compiled against GDAL %s and "
                   "is running against GDAL %s\n",
                   argv[0], GDAL_RELEASE_NAME, GDALVersionInfo("RELEASE_NAME"));
            CSLDestroy( argv );
            return 0;
        }
        else if( EQUAL(argv[i], "--help") )
            Usage();
        else if( EQUAL(argv[i],"-ox") )
        {
            CHECK_HAS_ENOUGH_ADDITIONAL_ARGS(1);
            dfObserverX = CPLAtof(argv[++i]);
        }
        else if (EQUAL(argv[i], "-oy"))
        {
            CHECK_HAS_ENOUGH_ADDITIONAL_ARGS(1);
            dfObserverY = CPLAtof(argv[++i]);
        }
        else if( EQUAL(argv[i],"-oz") )
        {
            CHECK_HAS_ENOUGH_ADDITIONAL_ARGS(1);
            dfObserverHeight = CPLAtof(argv[++i]);
        }
        else if (EQUAL(argv[i], "-vv"))
        {
            CHECK_HAS_ENOUGH_ADDITIONAL_ARGS(1);
            dfVisibleVal = CPLAtof(argv[++i]);
        }
        else if (EQUAL(argv[i], "-iv"))
        {
            CHECK_HAS_ENOUGH_ADDITIONAL_ARGS(1);
            dfInvisibleVal = CPLAtof(argv[++i]);
        }
        else if (EQUAL(argv[i], "-ov"))
        {
            CHECK_HAS_ENOUGH_ADDITIONAL_ARGS(1);
            dfOutOfRangeVal = CPLAtof(argv[++i]);
        }
        else if (EQUAL(argv[i], "-nv"))
        {
            CHECK_HAS_ENOUGH_ADDITIONAL_ARGS(1);
            dfNoDataVal = CPLAtof(argv[++i]);
        }
        else if (EQUAL(argv[i], "-tz"))
        {
            CHECK_HAS_ENOUGH_ADDITIONAL_ARGS(1);
            dfTargetHeight = CPLAtof(argv[++i]);
        }
        else if (EQUAL(argv[i], "-md"))
        {
            CHECK_HAS_ENOUGH_ADDITIONAL_ARGS(1);
            dfMaxDistance = CPLAtof(argv[++i]);
        }
        else if (EQUAL(argv[i], "-cc"))
        {
            CHECK_HAS_ENOUGH_ADDITIONAL_ARGS(1);
            dfCurvCoeff = CPLAtof(argv[++i]);
        }
        else if( EQUAL(argv[i],"-b") )
        {
            CHECK_HAS_ENOUGH_ADDITIONAL_ARGS(1);
            nBandIn = atoi(argv[++i]);
        }
        else if ( EQUAL(argv[i],"-q") || EQUAL(argv[i],"-quiet") )
        {
            bQuiet = TRUE;
        }
        else if( pszSrcFilename == nullptr )
        {
            pszSrcFilename = argv[i];
        }
        else if( pszDstFilename == nullptr )
        {
            pszDstFilename = argv[i];
        }
        else
            Usage("Too many command options.");
    }

    if (pszSrcFilename == nullptr)
    {
        Usage("Missing source filename.");
    }

    if (pszDstFilename == nullptr)
    {
        Usage("Missing destination filename.");
    }

    if (!bQuiet)
        pfnProgress = GDALTermProgress;

/* -------------------------------------------------------------------- */
/*      Open source raster file.                                        */
/* -------------------------------------------------------------------- */
    GDALDatasetH hSrcDS = GDALOpen(pszSrcFilename, GA_ReadOnly);
    if( hSrcDS == nullptr )
        exit( 2 );

    GDALRasterBandH hBand = GDALGetRasterBand( hSrcDS, nBandIn );
    if( hBand == nullptr )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "Band %d does not exist on dataset.",
                  nBandIn );
        exit(2);
    }

/* -------------------------------------------------------------------- */
/*      Try to get a coordinate system from the raster.                 */
/* -------------------------------------------------------------------- */
    OGRSpatialReferenceH hSRS = nullptr;

    const char *pszWKT = GDALGetProjectionRef( hSrcDS );

    if( pszWKT != nullptr && strlen(pszWKT) != 0 )
        hSRS = OSRNewSpatialReference( pszWKT );

/* -------------------------------------------------------------------- */
/*      Invoke.                                                         */
/* -------------------------------------------------------------------- */
    CPLErr eErr = GDALViewshedGenerate( hBand, pszDstFilename,
                         dfObserverX, dfObserverY, 
                         dfObserverHeight, dfTargetHeight,
                         dfVisibleVal, dfInvisibleVal, 
                         dfOutOfRangeVal, dfNoDataVal, dfCurvCoeff,
                         GVM_Edge, dfMaxDistance,
                         pfnProgress, nullptr );

    GDALClose( hSrcDS );

    if (hSRS)
        OSRDestroySpatialReference( hSRS );

    CSLDestroy( argv );
    GDALDestroyDriverManager();
    OGRCleanupAll();

    return (eErr == CE_None) ? 0 : 1;
}
MAIN_END
