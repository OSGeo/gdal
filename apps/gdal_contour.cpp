/******************************************************************************
 *
 * Project:  Contour Generator
 * Purpose:  Contour Generator mainline.
 * Author:   Frank Warmerdam <warmerdam@pobox.com>
 *
 ******************************************************************************
 * Copyright (c) 2003, Applied Coherent Technology (www.actgate.com).
 * Copyright (c) 2008-2013, Even Rouault <even dot rouault at spatialys.com>
 * Copyright (c) 2018, Oslandia <infos at oslandia dot com>
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
/*                            ArgIsNumeric()                            */
/************************************************************************/

static bool ArgIsNumeric( const char *pszArg )

{
    return CPLGetValueType(pszArg) != CPL_VALUE_STRING;
}

/************************************************************************/
/*                               Usage()                                */
/************************************************************************/

static void Usage(const char* pszErrorMsg = nullptr)

{
    printf(
        "Usage: gdal_contour [-b <band>] [-a <attribute_name>] [-amin <attribute_name>] [-amax <attribute_name>]\n"
        "                    [-3d] [-inodata] [-snodata n] [-f <formatname>] [-i <interval>]\n"
        "                    [[-dsco NAME=VALUE] ...] [[-lco NAME=VALUE] ...]\n"
        "                    [-off <offset>] [-fl <level> <level>...] [-e <exp_base>]\n"
        "                    [-nln <outlayername>] [-q] [-p]\n"
        "                    <src_filename> <dst_filename>\n" );

    if( pszErrorMsg != nullptr )
        fprintf(stderr, "\nFAILURE: %s\n", pszErrorMsg);

    exit( 1 );
}

static void CreateElevAttrib(const char* pszElevAttrib, OGRLayerH hLayer)
{
    OGRFieldDefnH hFld = OGR_Fld_Create( pszElevAttrib, OFTReal );
    OGRErr eErr = OGR_L_CreateField( hLayer, hFld, FALSE );
    OGR_Fld_Destroy( hFld );
    if( eErr == OGRERR_FAILURE )
    {
      exit( 1 );
    }
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
    bool b3D = false;
    int bNoDataSet = FALSE;
    bool bIgnoreNoData = false;
    int nBandIn = 1;
    double dfInterval = 0.0;
    double dfNoData = 0.0;
    double dfOffset = 0.0;
    double dfExpBase = 0.0;
    const char *pszSrcFilename = nullptr;
    const char *pszDstFilename = nullptr;
    const char *pszElevAttrib = nullptr;
    const char *pszElevAttribMin = nullptr;
    const char *pszElevAttribMax = nullptr;
    const char *pszFormat = nullptr;
    char **papszDSCO = nullptr;
    char **papszLCO = nullptr;
    double adfFixedLevels[1000];
    int nFixedLevelCount = 0;
    const char *pszNewLayerName = "contour";
    bool bQuiet = false;
    GDALProgressFunc pfnProgress = nullptr;
    bool bPolygonize = false;

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
    OGRRegisterAll();

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
        else if( EQUAL(argv[i],"-a") )
        {
            CHECK_HAS_ENOUGH_ADDITIONAL_ARGS(1);
            // coverity[tainted_data]
            pszElevAttrib = argv[++i];
        }
        else if( EQUAL(argv[i],"-amin") )
        {
            CHECK_HAS_ENOUGH_ADDITIONAL_ARGS(1);
            // coverity[tainted_data]
            pszElevAttribMin = argv[++i];
        }
        else if( EQUAL(argv[i],"-amax") )
        {
            CHECK_HAS_ENOUGH_ADDITIONAL_ARGS(1);
            // coverity[tainted_data]
            pszElevAttribMax = argv[++i];
        }
        else if( EQUAL(argv[i],"-off") )
        {
            CHECK_HAS_ENOUGH_ADDITIONAL_ARGS(1);
            // coverity[tainted_data]
            dfOffset = CPLAtof(argv[++i]);
        }
        else if( EQUAL(argv[i],"-i") )
        {
            CHECK_HAS_ENOUGH_ADDITIONAL_ARGS(1);
            // coverity[tainted_data]
            dfInterval = CPLAtof(argv[++i]);
        }
        else if( EQUAL(argv[i],"-e") )
        {
            CHECK_HAS_ENOUGH_ADDITIONAL_ARGS(1);
            // coverity[tainted_data]
            dfExpBase = CPLAtof(argv[++i]);
        }
        else if( EQUAL(argv[i],"-p") )
        {
            bPolygonize = true;
        }
        else if( EQUAL(argv[i],"-fl") )
        {
            if( i >= argc-1 )
                Usage(CPLSPrintf("%s option requires at least 1 argument",
                                 argv[i]));
            while( i < argc-1
                   && nFixedLevelCount
                   < static_cast<int>(sizeof(adfFixedLevels)/sizeof(double))
                   && ArgIsNumeric(argv[i+1]) )
                // coverity[tainted_data]
                adfFixedLevels[nFixedLevelCount++] = CPLAtof(argv[++i]);
        }
        else if( EQUAL(argv[i],"-b") )
        {
            CHECK_HAS_ENOUGH_ADDITIONAL_ARGS(1);
            // coverity[tainted_data]
            nBandIn = atoi(argv[++i]);
        }
        else if( EQUAL(argv[i],"-f") || EQUAL(argv[i],"-of") )
        {
            CHECK_HAS_ENOUGH_ADDITIONAL_ARGS(1);
            // coverity[tainted_data]
            pszFormat = argv[++i];
        }
        else if( EQUAL(argv[i],"-dsco") )
        {
            CHECK_HAS_ENOUGH_ADDITIONAL_ARGS(1);
            // coverity[tainted_data]
            papszDSCO = CSLAddString(papszDSCO, argv[++i] );
        }
        else if( EQUAL(argv[i],"-lco") )
        {
            CHECK_HAS_ENOUGH_ADDITIONAL_ARGS(1);
            // coverity[tainted_data]
            papszLCO = CSLAddString(papszLCO, argv[++i] );
        }
        else if( EQUAL(argv[i],"-3d")  )
        {
            b3D = true;
        }
        else if( EQUAL(argv[i],"-snodata") )
        {
            CHECK_HAS_ENOUGH_ADDITIONAL_ARGS(1);
            bNoDataSet = TRUE;
            // coverity[tainted_data]
            dfNoData = CPLAtof(argv[++i]);
        }
        else if( EQUAL(argv[i],"-nln") )
        {
            CHECK_HAS_ENOUGH_ADDITIONAL_ARGS(1);
            // coverity[tainted_data]
            pszNewLayerName = argv[++i];
        }
        else if( EQUAL(argv[i],"-inodata") )
        {
            bIgnoreNoData = true;
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

    if( dfInterval == 0.0 && nFixedLevelCount == 0 && dfExpBase == 0.0 )
    {
        Usage("Neither -i nor -fl nor -e are specified.");
    }

    if (pszSrcFilename == nullptr)
    {
        Usage("Missing source filename.");
    }

    if (pszDstFilename == nullptr)
    {
        Usage("Missing destination filename.");
    }

    if( strcmp(pszDstFilename, "/vsistdout/") == 0 ||
        strcmp(pszDstFilename, "/dev/stdout") == 0 )
    {
        bQuiet = true;
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

    if( !bNoDataSet && !bIgnoreNoData )
        dfNoData = GDALGetRasterNoDataValue( hBand, &bNoDataSet );

/* -------------------------------------------------------------------- */
/*      Try to get a coordinate system from the raster.                 */
/* -------------------------------------------------------------------- */
    OGRSpatialReferenceH hSRS = GDALGetSpatialRef( hSrcDS );

/* -------------------------------------------------------------------- */
/*      Create the output file.                                         */
/* -------------------------------------------------------------------- */
    CPLString osFormat;
    if( pszFormat == nullptr )
    {
        std::vector<CPLString> aoDrivers =
            GetOutputDriversFor(pszDstFilename, GDAL_OF_VECTOR);
        if( aoDrivers.empty() )
        {
            CPLError( CE_Failure, CPLE_AppDefined,
                    "Cannot guess driver for %s", pszDstFilename);
            exit( 10 );
        }
        else
        {
            if( aoDrivers.size() > 1 )
            {
                CPLError( CE_Warning, CPLE_AppDefined,
                        "Several drivers matching %s extension. Using %s",
                        CPLGetExtension(pszDstFilename), aoDrivers[0].c_str() );
            }
            osFormat = aoDrivers[0];
        }
    }
    else
    {
        osFormat = pszFormat;
    }

    OGRSFDriverH hDriver = OGRGetDriverByName( osFormat.c_str() );

    if( hDriver == nullptr )
    {
        fprintf( stderr, "Unable to find format driver named %s.\n",
                 osFormat.c_str() );
        exit( 10 );
    }

    OGRDataSourceH hDS =
        OGR_Dr_CreateDataSource(hDriver, pszDstFilename, papszDSCO);
    if( hDS == nullptr )
        exit(1);

    OGRLayerH hLayer =
        OGR_DS_CreateLayer(hDS, pszNewLayerName, hSRS,
                           bPolygonize ? (b3D ? wkbMultiPolygon25D : wkbMultiPolygon )
                           : (b3D ? wkbLineString25D : wkbLineString),
                           papszLCO);
    if( hLayer == nullptr )
        exit( 1 );

    OGRFieldDefnH hFld = OGR_Fld_Create("ID", OFTInteger);
    OGR_Fld_SetWidth( hFld, 8 );
    OGR_L_CreateField( hLayer, hFld, FALSE );
    OGR_Fld_Destroy( hFld );

    if( bPolygonize )
    {
        if( pszElevAttrib )
        {
            pszElevAttrib = nullptr;
            CPLError(CE_Warning, CPLE_NotSupported,
                     "-a is ignored in polygonal contouring mode. "
                     "Use -amin and/or -amax instead");
        }
    }
    else
    {
        if( pszElevAttribMin != nullptr || pszElevAttribMax != nullptr )
        {
            pszElevAttribMin = nullptr;
            pszElevAttribMax = nullptr;
            CPLError(CE_Warning, CPLE_NotSupported,
                     "-amin and/or -amax are ignored in line contouring mode. "
                     "Use -a instead");
        }
    }

    if( pszElevAttrib )
    {
      CreateElevAttrib( pszElevAttrib, hLayer );
    }

    if( pszElevAttribMin )
    {
      CreateElevAttrib( pszElevAttribMin, hLayer );
    }

    if( pszElevAttribMax )
    {
      CreateElevAttrib( pszElevAttribMax, hLayer );
    }

/* -------------------------------------------------------------------- */
/*      Invoke.                                                         */
/* -------------------------------------------------------------------- */
    int iIDField = OGR_FD_GetFieldIndex( OGR_L_GetLayerDefn( hLayer ), "ID" );
    int iElevField = (pszElevAttrib == nullptr) ? -1 :
        OGR_FD_GetFieldIndex( OGR_L_GetLayerDefn( hLayer ),
                              pszElevAttrib );

    int iElevFieldMin = (pszElevAttribMin == nullptr) ? -1 :
        OGR_FD_GetFieldIndex( OGR_L_GetLayerDefn( hLayer ),
                              pszElevAttribMin );

    int iElevFieldMax = (pszElevAttribMax == nullptr) ? -1 :
        OGR_FD_GetFieldIndex( OGR_L_GetLayerDefn( hLayer ),
                              pszElevAttribMax );

    char** options = nullptr;
    if ( nFixedLevelCount > 0 ) {
        std::string values = "FIXED_LEVELS=";
        for ( int i = 0; i < nFixedLevelCount; i++ ) {
            const int sz = 32;
            char* newValue = new char[sz+1];
            if ( i == nFixedLevelCount - 1 ) {
                CPLsnprintf( newValue, sz+1, "%f", adfFixedLevels[i] );
            }
            else {
                CPLsnprintf( newValue, sz+1, "%f,", adfFixedLevels[i] );
            }
            values = values + std::string( newValue );
            delete[] newValue;
        }
        options = CSLAddString( options, values.c_str() );
    }
    else if ( dfExpBase != 0.0 ) {
        options = CSLAppendPrintf( options, "LEVEL_EXP_BASE=%f", dfExpBase );
    }
    else if ( dfInterval != 0.0 ) {
        options = CSLAppendPrintf( options, "LEVEL_INTERVAL=%f", dfInterval );
    }

    if ( dfOffset != 0.0 ) {
        options = CSLAppendPrintf( options, "LEVEL_BASE=%f", dfOffset );
    }

    if ( bNoDataSet ) {
        options = CSLAppendPrintf( options, "NODATA=%.19g", dfNoData );
    }
    if ( iIDField != -1 ) {
        options = CSLAppendPrintf( options, "ID_FIELD=%d", iIDField );
    }
    if ( iElevField != -1 ) {
        options = CSLAppendPrintf( options, "ELEV_FIELD=%d", iElevField );
    }
    if ( iElevFieldMin != -1 ) {
        options = CSLAppendPrintf( options, "ELEV_FIELD_MIN=%d", iElevFieldMin );
    }
    if ( iElevFieldMax != -1 ) {
        options = CSLAppendPrintf( options, "ELEV_FIELD_MAX=%d", iElevFieldMax );
    }
    if ( bPolygonize ) {
        options = CSLAppendPrintf( options, "POLYGONIZE=YES" );
    }

    CPLErr eErr = GDALContourGenerateEx( hBand, hLayer, options, pfnProgress, nullptr );

    CSLDestroy( options );
    OGR_DS_Destroy( hDS );
    GDALClose( hSrcDS );

    CSLDestroy( argv );
    CSLDestroy( papszDSCO );
    CSLDestroy( papszLCO );
    GDALDestroyDriverManager();
    OGRCleanupAll();

    return (eErr == CE_None) ? 0 : 1;
}
MAIN_END
