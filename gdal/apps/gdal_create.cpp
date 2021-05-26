/******************************************************************************
 *
 * Project:  GDAL Utilities
 * Purpose:  GDAL Raster creation utility
 * Author:   Even Rouault <even dot rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2020, Even Rouault <even dot rouault at spatialys.com>
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

#include "cpl_string.h"
#include "gdal_version.h"
#include "gdal_priv.h"
#include "gdal.h"
#include "commonutils.h"
#include "ogr_spatialref.h"

#include <memory>
#include <vector>

CPL_CVSID("$Id$")

static void Usage()

{
    printf( "Usage: gdal_create [--help-general]\n"
            "       [-of format]\n"
            "       [-outsize xsize ysize]\n"
            "       [-bands count]\n"
            "       [-burn value]*\n"
            "       [-ot {Byte/Int16/UInt16/UInt32/Int32/Float32/Float64/\n"
            "             CInt16/CInt32/CFloat32/CFloat64}] [-strict]\n"
            "       [-a_srs srs_def] [-a_ullr ulx uly lrx lry] [-a_nodata value]\n"
            "       [-mo \"META-TAG=VALUE\"]* [-q]\n"
            "       [-co \"NAME=VALUE\"]*\n"
            "       [-if input_dataset]\n"
            "       out_dataset\n" );

    exit(1);
}

/************************************************************************/
/*                            ArgIsNumeric()                            */
/************************************************************************/

static bool ArgIsNumeric( const char *pszArg )

{
    char* pszEnd = nullptr;
    CPLStrtod(pszArg, &pszEnd);
    return pszEnd != nullptr && pszEnd[0] == '\0';
}

/************************************************************************/
/*                                main()                                */
/************************************************************************/

MAIN_START(argc, argv)

{
    /* Check strict compilation and runtime library version as we use C++ API */
    if (! GDAL_CHECK_VERSION(argv[0]))
        exit(1);

    EarlySetConfigOptions(argc, argv);

/* -------------------------------------------------------------------- */
/*      Register standard GDAL drivers, and process generic GDAL        */
/*      command options.                                                */
/* -------------------------------------------------------------------- */
    GDALAllRegister();
    argc = GDALGeneralCmdLineProcessor( argc, &argv, 0 );
    if( argc < 1 )
        exit( -argc );

    const char* pszFormat = nullptr;
    const char* pszFilename = nullptr;
    CPLStringList aosCreateOptions;
    int nPixels = 0;
    int nLines = 0;
    int nBandCount = -1;
    GDALDataType eDT = GDT_Unknown;
    double dfULX = 0;
    double dfULY = 0;
    double dfLRX = 0;
    double dfLRY = 0;
    bool bGeoTransform = false;
    const char* pszOutputSRS = nullptr;
    CPLStringList aosMetadata;
    std::vector<double> adfBurnValues;
    bool bQuiet = false;
    int bSetNoData = false;
    double dfNoDataValue = 0;
    const char* pszInputFile = nullptr;
    for( int i = 1; argv != nullptr && argv[i] != nullptr; i++ )
    {
        if( EQUAL(argv[i], "--utility_version") )
        {
            printf("%s was compiled against GDAL %s and is running against GDAL %s\n",
                   argv[0], GDAL_RELEASE_NAME, GDALVersionInfo("RELEASE_NAME"));
            CSLDestroy( argv );
            return 0;
        }
        else if( EQUAL(argv[i],"--help") )
        {
            Usage();
        }
        else if( i < argc-1 && (EQUAL(argv[i],"-of") || EQUAL(argv[i],"-f")) )
        {
            ++i;
            pszFormat = argv[i];
        }
        else if( i < argc-1 && EQUAL(argv[i],"-co"))
        {
            ++i;
            aosCreateOptions.AddString(argv[i]);
        }
        else if( i < argc-1 && EQUAL(argv[i],"-mo"))
        {
            ++i;
            aosMetadata.AddString(argv[i]);
        }
        else if( i < argc-1 && EQUAL(argv[i],"-bands"))
        {
            ++i;
            nBandCount = atoi(argv[i]);
        }
        else if( i+2 < argc && EQUAL(argv[i],"-outsize") )
        {
            ++i;
            nPixels = atoi(argv[i]);
            ++i;
            nLines = atoi(argv[i]);
        }
        else if( i < argc-1 && EQUAL(argv[i],"-ot") )
        {
            ++i;
            for( int iType = 1; iType < GDT_TypeCount; iType++ )
            {
                if( GDALGetDataTypeName(static_cast<GDALDataType>(iType)) != nullptr
                    && EQUAL(GDALGetDataTypeName(static_cast<GDALDataType>(iType)),
                             argv[i]) )
                {
                    eDT = static_cast<GDALDataType>(iType);
                }
            }

            if( eDT == GDT_Unknown )
            {
                CPLError(CE_Failure, CPLE_NotSupported,
                         "Unknown output pixel type: %s.", argv[i] );
                CSLDestroy( argv );
                exit(1);
            }
        }
        else if( i+4 < argc && EQUAL(argv[i],"-a_ullr") )
        {
            bGeoTransform = true;
            // coverity[tainted_data]
            dfULX = CPLAtofM(argv[++i]);
            // coverity[tainted_data]
            dfULY = CPLAtofM(argv[++i]);
            // coverity[tainted_data]
            dfLRX = CPLAtofM(argv[++i]);
            // coverity[tainted_data]
            dfLRY = CPLAtofM(argv[++i]);
        }
        else if( i < argc-1 && EQUAL(argv[i],"-a_srs") )
        {
            ++i;
            pszOutputSRS = argv[i];
        }
        else if( i < argc-1 && EQUAL(argv[i],"-a_nodata") )
        {
            bSetNoData = true;
            ++i;
            // coverity[tainted_data]
            dfNoDataValue = CPLAtofM(argv[i]);
        }

        else if( i < argc-1 && EQUAL(argv[i],"-burn") )
        {
            if (strchr(argv[i+1], ' '))
            {
                ++i;
                CPLStringList aosTokens( CSLTokenizeString( argv[i] ) );
                for( int j = 0; j < aosTokens.size(); j++)
                {
                    adfBurnValues.push_back(CPLAtof(aosTokens[j]));
                }
            }
            else
            {
                // coverity[tainted_data]
                while(i < argc-1 && ArgIsNumeric(argv[i+1]))
                {
                    ++i;
                    // coverity[tainted_data]
                    adfBurnValues.push_back(CPLAtof(argv[i]));
                }
            }
        }
        else if( i < argc-1 && EQUAL(argv[i],"-if") )
        {
            ++i;
            pszInputFile = argv[i];
        }
        else if( EQUAL(argv[i], "-q") )
        {
            bQuiet = true;
        }
        else if( argv[i][0] == '-' )
        {
            CPLError(CE_Failure, CPLE_NotSupported,
                     "Unknown option name '%s'", argv[i]);
            CSLDestroy( argv );
            Usage();
        }
        else if( pszFilename == nullptr )
        {
            pszFilename = argv[i];
        }
        else
        {
            CPLError(CE_Failure, CPLE_NotSupported,
                     "Too many command options '%s'", argv[i]);
            CSLDestroy( argv );
            Usage();
        }
    }
    if( pszFilename == nullptr )
    {
        CSLDestroy( argv );
        Usage();
    }

    double adfGeoTransform[6] = {0, 1, 0, 0, 0, 1};
    if( bGeoTransform && nPixels > 0 && nLines > 0 )
    {
        adfGeoTransform[0] = dfULX;
        adfGeoTransform[1] = (dfLRX - dfULX) / nPixels;
        adfGeoTransform[2] = 0;
        adfGeoTransform[3] = dfULY;
        adfGeoTransform[4] = 0;
        adfGeoTransform[5] = (dfLRY - dfULY) / nLines;
    }

    std::unique_ptr<GDALDataset> poInputDS;
    if( pszInputFile )
    {
        poInputDS.reset(GDALDataset::Open(pszInputFile, GDAL_OF_RASTER | GDAL_OF_VERBOSE_ERROR));
        if( poInputDS == nullptr )
        {
            CSLDestroy( argv );
            GDALDestroyDriverManager();
            exit( 1 );
        }
        if( nPixels == 0 )
        {
            nPixels = poInputDS->GetRasterXSize();
            nLines = poInputDS->GetRasterYSize();
        }
        if( nBandCount < 0 )
        {
            nBandCount = poInputDS->GetRasterCount();
        }
        if( eDT == GDT_Unknown && poInputDS->GetRasterCount() > 0 )
        {
            eDT = poInputDS->GetRasterBand(1)->GetRasterDataType();
        }
        if( pszOutputSRS == nullptr )
        {
            pszOutputSRS = poInputDS->GetProjectionRef();
        }
        if( ! (bGeoTransform && nPixels > 0 && nLines > 0) )
        {
            if( poInputDS->GetGeoTransform(adfGeoTransform) == CE_None )
            {
                bGeoTransform = true;
            }
        }
        if( !bSetNoData && poInputDS->GetRasterCount() > 0 )
        {
            dfNoDataValue = poInputDS->GetRasterBand(1)->GetNoDataValue(&bSetNoData);
        }
    }

    GDALDriverH hDriver = GDALGetDriverByName(
        pszFormat ? pszFormat : GetOutputDriverForRaster( pszFilename ).c_str());
    if ( hDriver == nullptr )
    {
        fprintf( stderr, "Output driver not found.\n");
        CSLDestroy( argv );
        GDALDestroyDriverManager();
        exit( 1 );
    }
    const bool bHasCreate =
        GDALGetMetadataItem( hDriver, GDAL_DCAP_CREATE, nullptr ) != nullptr;
    if( !bHasCreate &&
        GDALGetMetadataItem( hDriver, GDAL_DCAP_CREATECOPY, nullptr ) == nullptr)
    {
        fprintf( stderr, "This driver has no creation capabilities.\n");
        CSLDestroy( argv );
        GDALDestroyDriverManager();
        exit( 1 );
    }
    GDALDriverH hTmpDriver = GDALGetDriverByName("MEM");
    if( !bHasCreate && hTmpDriver == nullptr )
    {
        fprintf( stderr, "MEM driver not available.\n");
        CSLDestroy( argv );
        GDALDestroyDriverManager();
        exit( 1 );
    }

    if( nPixels != 0 && eDT == GDT_Unknown )
    {
        eDT = GDT_Byte;
    }
    if( nBandCount < 0 )
    {
        nBandCount = eDT == GDT_Unknown ? 0 : 1;
    }
    GDALDatasetH hDS = GDALCreate( bHasCreate ? hDriver : hTmpDriver,
                                   pszFilename, nPixels, nLines,
                                   nBandCount, eDT,
                                   bHasCreate ? aosCreateOptions.List() : nullptr );

    if( hDS == nullptr )
    {
        GDALDestroyDriverManager();
        CSLDestroy( argv );
        exit( 1 );
    }

    if( pszOutputSRS && pszOutputSRS[0] != '\0' && !EQUAL(pszOutputSRS, "NONE") )
    {
        OGRSpatialReference oSRS;
        oSRS.SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);

        if( oSRS.SetFromUserInput( pszOutputSRS ) != OGRERR_NONE )
        {
            CPLError( CE_Failure, CPLE_AppDefined, "Failed to process SRS definition: %s",
                      pszOutputSRS );
            CSLDestroy( argv );
            GDALDestroyDriverManager();
            exit( 1 );
        }

        char* pszSRS = nullptr;
        oSRS.exportToWkt( &pszSRS );

        if( GDALSetProjection(hDS, pszSRS) != CE_None )
        {
            CPLFree(pszSRS);
            GDALClose(hDS);
            CSLDestroy( argv );
            GDALDestroyDriverManager();
            exit( 1 );
        }
        CPLFree(pszSRS);
    }
    if( bGeoTransform )
    {
        if( nPixels == 0 )
        {
            fprintf( stderr, "-outsize must be specified when -a_ullr is used.\n");
            GDALClose(hDS);
            GDALDestroyDriverManager();
            exit( 1 );
        }
        if( GDALSetGeoTransform(hDS, adfGeoTransform) != CE_None )
        {
            GDALClose(hDS);
            CSLDestroy( argv );
            GDALDestroyDriverManager();
            exit( 1 );
        }
    }
    if( !aosMetadata.empty() )
    {
        GDALSetMetadata(hDS, aosMetadata.List(), nullptr );
    }
    const int nBands = GDALGetRasterCount(hDS);
    if( bSetNoData )
    {
        for( int i = 0; i < nBands; i++ )
        {
            GDALSetRasterNoDataValue(GDALGetRasterBand(hDS, i+1),
                                     dfNoDataValue);
        }
    }
    if( !adfBurnValues.empty() )
    {
        for( int i = 0; i < nBands; i++ )
        {
            GDALFillRaster(GDALGetRasterBand(hDS, i+1),
                           i < static_cast<int>(adfBurnValues.size()) ?
                                adfBurnValues[i] : adfBurnValues.back(),
                           0);
        }
    }

    bool bHasGotErr = false;
    if( !bHasCreate )
    {
        GDALDatasetH hOutDS = GDALCreateCopy(
            hDriver, pszFilename, hDS, false,
            aosCreateOptions.List(),
            bQuiet ? GDALDummyProgress : GDALTermProgress, nullptr );
        if( hOutDS == nullptr )
        {
            GDALClose(hDS);
            CSLDestroy( argv );
            GDALDestroyDriverManager();
            exit( 1 );
        }
        const bool bWasFailureBefore = (CPLGetLastErrorType() == CE_Failure);
        GDALFlushCache( hOutDS );
        if (!bWasFailureBefore && CPLGetLastErrorType() == CE_Failure)
        {
            bHasGotErr = true;
        }
        GDALClose(hOutDS);
    }

    const bool bWasFailureBefore = (CPLGetLastErrorType() == CE_Failure);
    GDALClose(hDS);
    if (!bWasFailureBefore && CPLGetLastErrorType() == CE_Failure)
    {
        bHasGotErr = true;
    }

    CSLDestroy( argv );
    return bHasGotErr ? 1 : 0;
}
MAIN_END
