/******************************************************************************
 *
 * Project:  GDAL
 * Purpose:  Command line point transformer.
 * Author:   Frank Warmerdam <warmerdam@pobox.com>
 *
 ******************************************************************************
 * Copyright (c) 2007, Frank Warmerdam <warmerdam@pobox.com>
 * Copyright (c) 2008-2013, Even Rouault <even dot rouault at mines-paris dot org>
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

#include "cpl_port.h"

#include <cstdio>
#include <cstdlib>

#include "cpl_conv.h"
#include "cpl_error.h"
#include "cpl_string.h"
#include "gdal_alg.h"
#include "gdalwarper.h"
#include "gdal.h"
#include "gdal_version.h"
#include "ogr_api.h"
#include "ogr_core.h"
#include "ogr_spatialref.h"
#include "ogr_srs_api.h"
#include "commonutils.h"

CPL_CVSID("$Id$")

/************************************************************************/
/*                               Usage()                                */
/************************************************************************/

static void Usage(const char* pszErrorMsg = nullptr)

{
    printf(
        "Usage: gdaltransform [--help-general]\n"
        "    [-i] [-s_srs srs_def] [-t_srs srs_def] [-to \"NAME=VALUE\"]\n"
        "    [-order n] [-tps] [-rpc] [-geoloc] \n"
        "    [-gcp pixel line easting northing [elevation]]* [-output_xy]\n"
        "    [srcfile [dstfile]]\n"
        "\n" );

    if( pszErrorMsg != nullptr )
        fprintf(stderr, "\nFAILURE: %s\n", pszErrorMsg);

    exit( 1 );
}

/************************************************************************/
/*                             SanitizeSRS                              */
/************************************************************************/

static char *SanitizeSRS( const char *pszUserInput )

{
    CPLErrorReset();

    char *pszResult = nullptr;
    OGRSpatialReferenceH hSRS = OSRNewSpatialReference(nullptr);
    if( OSRSetFromUserInput( hSRS, pszUserInput ) == OGRERR_NONE )
    {
        OSRExportToWkt( hSRS, &pszResult );
    }
    else
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "Translating source or target SRS failed:\n%s",
                  pszUserInput );
        exit( 1 );
    }

    OSRDestroySpatialReference( hSRS );

    return pszResult;
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
    // Check that we are running against at least GDAL 1.5.
    // Note to developers: if we use newer API, please change the requirement.
    if (atoi(GDALVersionInfo("VERSION_NUM")) < 1500)
    {
        fprintf(stderr,
                "At least, GDAL >= 1.5.0 is required for this version of %s, "
                "which was compiled against GDAL %s\n",
                argv[0], GDAL_RELEASE_NAME);
        exit(1);
    }

    GDALAllRegister();
    argc = GDALGeneralCmdLineProcessor( argc, &argv, 0 );
    if( argc < 1 )
        exit( -argc );

    const char         *pszSrcFilename = nullptr;
    const char         *pszDstFilename = nullptr;
    int                 nOrder = 0;
    void               *hTransformArg;
    GDALTransformerFunc pfnTransformer = nullptr;
    int                 nGCPCount = 0;
    GDAL_GCP            *pasGCPs = nullptr;
    int                 bInverse = FALSE;
    char              **papszTO = nullptr;
    int                 bOutputXY = FALSE;
    double              dfX = 0.0;
    double              dfY = 0.0;
    double              dfZ = 0.0;
    bool                bCoordOnCommandLine = false;

/* -------------------------------------------------------------------- */
/*      Parse arguments.                                                */
/* -------------------------------------------------------------------- */
    for( int i = 1; i < argc && argv[i] != nullptr; i++ )
    {
        if( EQUAL(argv[i], "--utility_version") )
        {
            printf("%s was compiled against GDAL %s and "
                   "is running against GDAL %s\n",
                   argv[0], GDAL_RELEASE_NAME, GDALVersionInfo("RELEASE_NAME"));
            CSLDestroy(argv);
            return 0;
        }
        else if( EQUAL(argv[i],"--help") )
        {
            Usage();
        }
        else if( EQUAL(argv[i],"-t_srs") )
        {
            CHECK_HAS_ENOUGH_ADDITIONAL_ARGS(1);
            char *pszSRS = SanitizeSRS(argv[++i]);
            papszTO = CSLSetNameValue( papszTO, "DST_SRS", pszSRS );
            CPLFree( pszSRS );
        }
        else if( EQUAL(argv[i],"-s_srs") )
        {
            CHECK_HAS_ENOUGH_ADDITIONAL_ARGS(1);
            char *pszSRS = SanitizeSRS(argv[++i]);
            papszTO = CSLSetNameValue( papszTO, "SRC_SRS", pszSRS );
            CPLFree( pszSRS );
        }
        else if( EQUAL(argv[i],"-order") )
        {
            CHECK_HAS_ENOUGH_ADDITIONAL_ARGS(1);
            nOrder = atoi(argv[++i]);
            papszTO = CSLSetNameValue( papszTO, "MAX_GCP_ORDER", argv[i] );
        }
        else if( EQUAL(argv[i],"-tps") )
        {
            papszTO = CSLSetNameValue( papszTO, "METHOD", "GCP_TPS" );
            nOrder = -1;
        }
        else if( EQUAL(argv[i],"-rpc") )
        {
            papszTO = CSLSetNameValue( papszTO, "METHOD", "RPC" );
        }
        else if( EQUAL(argv[i],"-geoloc") )
        {
            papszTO = CSLSetNameValue( papszTO, "METHOD", "GEOLOC_ARRAY" );
        }
        else if( EQUAL(argv[i],"-i") )
        {
            bInverse = TRUE;
        }
        else if( EQUAL(argv[i],"-to")  )
        {
            CHECK_HAS_ENOUGH_ADDITIONAL_ARGS(1);
            papszTO = CSLAddString( papszTO, argv[++i] );
        }
        else if( EQUAL(argv[i],"-gcp") )
        {
            CHECK_HAS_ENOUGH_ADDITIONAL_ARGS(4);
            char* endptr = nullptr;
            /* -gcp pixel line easting northing [elev] */

            nGCPCount++;
            pasGCPs = static_cast<GDAL_GCP *>(
                CPLRealloc(pasGCPs, sizeof(GDAL_GCP) * nGCPCount));
            GDALInitGCPs( 1, pasGCPs + nGCPCount - 1 );

            pasGCPs[nGCPCount-1].dfGCPPixel = CPLAtof(argv[++i]);
            pasGCPs[nGCPCount-1].dfGCPLine = CPLAtof(argv[++i]);
            pasGCPs[nGCPCount-1].dfGCPX = CPLAtof(argv[++i]);
            pasGCPs[nGCPCount-1].dfGCPY = CPLAtof(argv[++i]);
            if( argv[i+1] != nullptr &&
                (CPLStrtod(argv[i+1], &endptr) != 0.0 || argv[i+1][0] == '0') )
            {
                // Check that last argument is really a number and not a
                // filename looking like a number (see ticket #863).
                if (endptr && *endptr == 0)
                    pasGCPs[nGCPCount-1].dfGCPZ = CPLAtof(argv[++i]);
            }

            /* should set id and info? */
        }
        else if( EQUAL(argv[i],"-output_xy") )
        {
            bOutputXY = TRUE;
        }
        else if( EQUAL(argv[i],"-coord")  && i + 2 < argc)
        {
            bCoordOnCommandLine = true;
            dfX = CPLAtof(argv[++i]);
            dfY = CPLAtof(argv[++i]);
            if( i + 1 < argc && CPLGetValueType(argv[i+1]) != CPL_VALUE_STRING )
                dfZ = CPLAtof(argv[++i]);
            bOutputXY = TRUE;
        }
        else if( argv[i][0] == '-' )
        {
            Usage(CPLSPrintf("Unknown option name '%s'", argv[i]));
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
        {
            Usage("Too many command options.");
        }
    }

/* -------------------------------------------------------------------- */
/*      Open src and destination file, if appropriate.                  */
/* -------------------------------------------------------------------- */
    GDALDatasetH hSrcDS = nullptr;
    if( pszSrcFilename != nullptr )
    {
        hSrcDS = GDALOpen( pszSrcFilename, GA_ReadOnly );
        if( hSrcDS == nullptr )
            exit( 1 );
    }

    GDALDatasetH hDstDS = nullptr;
    if( pszDstFilename != nullptr )
    {
        hDstDS = GDALOpen( pszDstFilename, GA_ReadOnly );
        if( hDstDS == nullptr )
            exit( 1 );
    }

    if( hSrcDS != nullptr && nGCPCount > 0 )
    {
        fprintf(stderr,
                "Command line GCPs and input file specified, "
                "specify one or the other.\n");
        exit( 1 );
    }

/* -------------------------------------------------------------------- */
/*      Create a transformation object from the source to               */
/*      destination coordinate system.                                  */
/* -------------------------------------------------------------------- */
    if( nGCPCount != 0 && nOrder == -1 )
    {
        pfnTransformer = GDALTPSTransform;
        hTransformArg =
            GDALCreateTPSTransformer( nGCPCount, pasGCPs, FALSE );
    }
    else if( nGCPCount != 0 )
    {
        pfnTransformer = GDALGCPTransform;
        hTransformArg =
            GDALCreateGCPTransformer( nGCPCount, pasGCPs, nOrder, FALSE );
    }
    else
    {
        pfnTransformer = GDALGenImgProjTransform;
        hTransformArg =
            GDALCreateGenImgProjTransformer2( hSrcDS, hDstDS, papszTO );
    }

    CSLDestroy( papszTO );

    if( hTransformArg == nullptr )
    {
        exit( 1 );
    }

/* -------------------------------------------------------------------- */
/*      Read points from stdin, transform and write to stdout.          */
/* -------------------------------------------------------------------- */
    while( bCoordOnCommandLine || !feof(stdin) )
    {
        if( !bCoordOnCommandLine )
        {
            char szLine[1024];

            if( fgets( szLine, sizeof(szLine)-1, stdin ) == nullptr )
                break;

            char **papszTokens = CSLTokenizeString(szLine);

            if( CSLCount(papszTokens) < 2 )
            {
                CSLDestroy(papszTokens);
                continue;
            }

            dfX = CPLAtof(papszTokens[0]);
            dfY = CPLAtof(papszTokens[1]);
            if( CSLCount(papszTokens) >= 3 )
                dfZ = CPLAtof(papszTokens[2]);
            else
                dfZ = 0.0;
            CSLDestroy(papszTokens);
        }

        int bSuccess = TRUE;
        if( pfnTransformer( hTransformArg, bInverse, 1,
                            &dfX, &dfY, &dfZ, &bSuccess )
            && bSuccess )
        {
            if( bOutputXY )
                CPLprintf( "%.15g %.15g\n", dfX, dfY );
            else
                CPLprintf( "%.15g %.15g %.15g\n", dfX, dfY, dfZ );
        }
        else
        {
            printf( "transformation failed.\n" );
        }

        if( bCoordOnCommandLine )
            break;
    }

    if( nGCPCount != 0 && nOrder == -1 )
    {
        GDALDestroyTPSTransformer(hTransformArg);
    }
    else if( nGCPCount != 0 )
    {
        GDALDestroyGCPTransformer(hTransformArg);
    }
    else
    {
        GDALDestroyGenImgProjTransformer(hTransformArg);
    }

    if (nGCPCount)
    {
        GDALDeinitGCPs( nGCPCount, pasGCPs );
        CPLFree( pasGCPs );
    }

    if (hSrcDS)
        GDALClose(hSrcDS);

    if (hDstDS)
        GDALClose(hDstDS);

    GDALDumpOpenDatasets( stderr );
    GDALDestroyDriverManager();

    CSLDestroy( argv );

    return 0;
}
MAIN_END
