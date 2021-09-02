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
       "Usage: gdal_viewshed [-b <band>]\n"
       "                     [-a_nodata <value>] [-f <formatname>]\n"
       "                     [-oz <observer_height>] [-tz <target_height>] [-md <max_distance>]\n"
       "                     -ox <observer_x> -oy <observer_y>\n"
       "                     [-vv <visibility>] [-iv <invisibility>]\n"
       "                     [-ov <out_of_range>] [-cc <curvature_coef>]\n"
       "                     [[-co NAME=VALUE] ...]\n"
       "                     [-q] [-om <output mode>]\n"
       "                     <src_filename> <dst_filename>\n");

    if( pszErrorMsg != nullptr )
        fprintf(stderr, "\nFAILURE: %s\n", pszErrorMsg);

    exit( 1 );
}

static double CPLAtofTaintedSuppressed(const char* pszVal)
{
    // coverity[tainted_data]
    return CPLAtof(pszVal);
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
    bool bObserverXSpecified = false;
    double dfObserverX = 0.0;
    bool bObserverYSpecified = false;
    double dfObserverY = 0.0;
    double dfVisibleVal = 255.0;
    double dfInvisibleVal = 0.0;
    double dfOutOfRangeVal = 0.0;
    double dfNoDataVal = -1.0;
    // Value for standard atmospheric refraction. See doc/source/programs/gdal_viewshed.rst
    double dfCurvCoeff = 0.85714;
    const char *pszDriverName = nullptr;
    const char *pszSrcFilename = nullptr;
    const char *pszDstFilename = nullptr;
    bool bQuiet = false;
    GDALProgressFunc pfnProgress = nullptr;
    char** papszCreateOptions = nullptr;
    const char *pszOutputMode = nullptr;

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
        else if( EQUAL(argv[i],"-f") || EQUAL(argv[i],"-of") )
        {
            CHECK_HAS_ENOUGH_ADDITIONAL_ARGS(1);
            pszDriverName = argv[++i];
        }
        else if( EQUAL(argv[i],"-ox") )
        {
            CHECK_HAS_ENOUGH_ADDITIONAL_ARGS(1);
            bObserverXSpecified = true;
            dfObserverX = CPLAtofTaintedSuppressed(argv[++i]);
        }
        else if (EQUAL(argv[i], "-oy"))
        {
            CHECK_HAS_ENOUGH_ADDITIONAL_ARGS(1);
            bObserverYSpecified = true;
            dfObserverY = CPLAtofTaintedSuppressed(argv[++i]);
        }
        else if( EQUAL(argv[i],"-oz") )
        {
            CHECK_HAS_ENOUGH_ADDITIONAL_ARGS(1);
            dfObserverHeight = CPLAtofTaintedSuppressed(argv[++i]);
        }
        else if (EQUAL(argv[i], "-vv"))
        {
            CHECK_HAS_ENOUGH_ADDITIONAL_ARGS(1);
            dfVisibleVal = CPLAtofTaintedSuppressed(argv[++i]);
        }
        else if (EQUAL(argv[i], "-iv"))
        {
            CHECK_HAS_ENOUGH_ADDITIONAL_ARGS(1);
            dfInvisibleVal = CPLAtofTaintedSuppressed(argv[++i]);
        }
        else if (EQUAL(argv[i], "-ov"))
        {
            CHECK_HAS_ENOUGH_ADDITIONAL_ARGS(1);
            dfOutOfRangeVal = CPLAtofTaintedSuppressed(argv[++i]);
        }
        else if( EQUAL(argv[i],"-co"))
        {
            CHECK_HAS_ENOUGH_ADDITIONAL_ARGS(1);
            papszCreateOptions = CSLAddString( papszCreateOptions, argv[++i] );
        }
        else if (EQUAL(argv[i], "-a_nodata"))
        {
            CHECK_HAS_ENOUGH_ADDITIONAL_ARGS(1);
            dfNoDataVal = CPLAtofM(argv[++i]);;
        }
        else if (EQUAL(argv[i], "-tz"))
        {
            CHECK_HAS_ENOUGH_ADDITIONAL_ARGS(1);
            dfTargetHeight = CPLAtofTaintedSuppressed(argv[++i]);
        }
        else if (EQUAL(argv[i], "-md"))
        {
            CHECK_HAS_ENOUGH_ADDITIONAL_ARGS(1);
            dfMaxDistance = CPLAtofTaintedSuppressed(argv[++i]);
        }
        else if (EQUAL(argv[i], "-cc"))
        {
            CHECK_HAS_ENOUGH_ADDITIONAL_ARGS(1);
            dfCurvCoeff = CPLAtofTaintedSuppressed(argv[++i]);
        }
        else if( EQUAL(argv[i],"-b") )
        {
            CHECK_HAS_ENOUGH_ADDITIONAL_ARGS(1);
            nBandIn = atoi(argv[++i]);
        }
        else if( EQUAL(argv[i],"-om") )
        {
            CHECK_HAS_ENOUGH_ADDITIONAL_ARGS(1);
            pszOutputMode = argv[++i];
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

    if( !bObserverXSpecified )
    {
        Usage("Missing -ox.");
    }

    if( !bObserverYSpecified )
    {
        Usage("Missing -oy.");
    }

    if (!bQuiet)
        pfnProgress = GDALTermProgress;

    CPLString osFormat;
    if( pszDriverName == nullptr  )
    {
        osFormat = GetOutputDriverForRaster(pszDstFilename);
        if( osFormat.empty() )
        {
            exit( 2 );
        }
    }

    GDALViewshedOutputType outputMode = GVOT_NORMAL;
    if(pszOutputMode != nullptr)
    {
        if(EQUAL(pszOutputMode, "NORMAL"))
        {
        }
        else if(EQUAL(pszOutputMode, "DEM"))
        {
            outputMode = GVOT_MIN_TARGET_HEIGHT_FROM_DEM;
        }
        else if(EQUAL(pszOutputMode, "GROUND"))
        {
            outputMode = GVOT_MIN_TARGET_HEIGHT_FROM_GROUND;
        }
        else
        {
            Usage("-om must be either NORMAL, DEM or GROUND");
        }
    }

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
/*      Invoke.                                                         */
/* -------------------------------------------------------------------- */
    GDALDatasetH hDstDS = GDALViewshedGenerate( hBand,
                         pszDriverName ? pszDriverName : osFormat.c_str(),
                         pszDstFilename, papszCreateOptions,
                         dfObserverX, dfObserverY,
                         dfObserverHeight, dfTargetHeight,
                         dfVisibleVal, dfInvisibleVal,
                         dfOutOfRangeVal, dfNoDataVal, dfCurvCoeff,
                         GVM_Edge, dfMaxDistance,
                         pfnProgress, nullptr, outputMode, nullptr);
    bool bSuccess = hDstDS != nullptr;
    GDALClose( hSrcDS );
    GDALClose( hDstDS );

    CSLDestroy( argv );
    CSLDestroy( papszCreateOptions);
    GDALDestroyDriverManager();
    OGRCleanupAll();

    return bSuccess ? 0 : 1;
}
MAIN_END
