/******************************************************************************
 *
 * Project:  MapServer
 * Purpose:  Commandline App to build tile index for raster files.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2001, Frank Warmerdam, DM Solutions Group Inc
 * Copyright (c) 2007-2013, Even Rouault <even dot rouault at spatialys.com>
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
#include "cpl_conv.h"
#include "cpl_string.h"
#include "gdal_version.h"
#include "gdal.h"
#include "ogr_api.h"
#include "ogr_srs_api.h"
#include "commonutils.h"

#include <cmath>

CPL_CVSID("$Id$")

/************************************************************************/
/*                               Usage()                                */
/************************************************************************/

static void Usage(const char* pszErrorMsg)

{
    fprintf(stdout, "%s",
            "\n"
            "Usage: gdaltindex [-f format] [-tileindex field_name] [-write_absolute_path] \n"
            "                  [-skip_different_projection] [-t_srs target_srs]\n"
            "                  [-src_srs_name field_name] [-src_srs_format [AUTO|WKT|EPSG|PROJ]\n"
            "                  [-lyr_name name] index_file [gdal_file]*\n"
            "\n"
            "e.g.\n"
            "  % gdaltindex doq_index.shp doq/*.tif\n"
            "\n"
            "NOTES:\n"
            "  o The index will be created if it doesn't already exist.\n"
            "  o The default tile index field is 'location'.\n"
            "  o Raster filenames will be put in the file exactly as they are specified\n"
            "    on the commandline unless the option -write_absolute_path is used.\n"
            "  o If -skip_different_projection is specified, only files with same projection ref\n"
            "    as files already inserted in the tileindex will be inserted (unless t_srs is specified).\n"
            "  o If -t_srs is specified, geometries of input files will be transformed to the desired\n"
            "    target coordinate reference system.\n"
            "    Note that using this option generates files that are NOT compatible with MapServer < 6.4.\n"
            "  o Simple rectangular polygons are generated in the same coordinate reference system\n"
            "    as the rasters, or in target reference system if the -t_srs option is used.\n");

    if( pszErrorMsg != nullptr )
        fprintf(stderr, "\nFAILURE: %s\n", pszErrorMsg);

    exit(1);
}

/************************************************************************/
/*                                main()                                */
/************************************************************************/

#define CHECK_HAS_ENOUGH_ADDITIONAL_ARGS(nExtraArg) \
    do { if (iArg + nExtraArg >= argc) \
        Usage(CPLSPrintf("%s option requires %d argument(s)", \
                         argv[iArg], nExtraArg)); } while( false )

typedef enum
{
    FORMAT_AUTO,
    FORMAT_WKT,
    FORMAT_EPSG,
    FORMAT_PROJ
} SrcSRSFormat;

MAIN_START(argc, argv)
{
    // Check that we are running against at least GDAL 1.4.
    // Note to developers: if we use newer API, please change the requirement.
    if( atoi(GDALVersionInfo("VERSION_NUM")) < 1400 )
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
    if( argc < 1 )
        exit( -argc );

/* -------------------------------------------------------------------- */
/*      Get commandline arguments other than the GDAL raster filenames. */
/* -------------------------------------------------------------------- */
    const char* pszIndexLayerName = nullptr;
    const char *index_filename = nullptr;
    const char *tile_index = "location";
    const char* pszDriverName = nullptr;
    size_t nMaxFieldSize = 254;
    bool write_absolute_path = false;
    char* current_path = nullptr;
    bool skip_different_projection = false;
    const char *pszTargetSRS = "";
    bool bSetTargetSRS = false;
    const char* pszSrcSRSName = nullptr;
    int i_SrcSRSName = -1;
    bool bSrcSRSFormatSpecified = false;
    SrcSRSFormat eSrcSRSFormat = FORMAT_AUTO;

    int iArg = 1;  // Used after for.
    for( ; iArg < argc; iArg++ )
    {
        if( EQUAL(argv[iArg], "--utility_version") )
        {
            printf("%s was compiled against GDAL %s and is running against "
                   "GDAL %s\n",
                   argv[0], GDAL_RELEASE_NAME, GDALVersionInfo("RELEASE_NAME"));
            CSLDestroy( argv );
            return 0;
        }
        else if( EQUAL(argv[iArg],"--help") )
            Usage(nullptr);
        else if( (strcmp(argv[iArg],"-f") == 0 || strcmp(argv[iArg],"-of") == 0) )
        {
            CHECK_HAS_ENOUGH_ADDITIONAL_ARGS(1);
            pszDriverName = argv[++iArg];
        }
        else if( strcmp(argv[iArg],"-lyr_name") == 0 )
        {
            CHECK_HAS_ENOUGH_ADDITIONAL_ARGS(1);
            pszIndexLayerName = argv[++iArg];
        }
        else if( strcmp(argv[iArg],"-tileindex") == 0 )
        {
            CHECK_HAS_ENOUGH_ADDITIONAL_ARGS(1);
            tile_index = argv[++iArg];
        }
        else if( strcmp(argv[iArg],"-t_srs") == 0 )
        {
            CHECK_HAS_ENOUGH_ADDITIONAL_ARGS(1);
            pszTargetSRS = argv[++iArg];
            bSetTargetSRS = true;
        }
        else if ( strcmp(argv[iArg],"-write_absolute_path") == 0 )
        {
            write_absolute_path = true;
        }
        else if ( strcmp(argv[iArg],"-skip_different_projection") == 0 )
        {
            skip_different_projection = true;
        }
        else if( strcmp(argv[iArg], "-src_srs_name") == 0 )
        {
            CHECK_HAS_ENOUGH_ADDITIONAL_ARGS(1);
            pszSrcSRSName = argv[++iArg];
        }
        else if( strcmp(argv[iArg], "-src_srs_format") == 0 )
        {
            const char* pszFormat;
            bSrcSRSFormatSpecified = true;
            CHECK_HAS_ENOUGH_ADDITIONAL_ARGS(1);
            pszFormat = argv[++iArg];
            if( EQUAL(pszFormat, "AUTO") )
                eSrcSRSFormat = FORMAT_AUTO;
            else if( EQUAL(pszFormat, "WKT") )
                eSrcSRSFormat = FORMAT_WKT;
            else if( EQUAL(pszFormat, "EPSG") )
                eSrcSRSFormat = FORMAT_EPSG;
            else if( EQUAL(pszFormat, "PROJ") )
                eSrcSRSFormat = FORMAT_PROJ;
        }
        else if( argv[iArg][0] == '-' )
            Usage(CPLSPrintf("Unknown option name '%s'", argv[iArg]));
        else if( index_filename == nullptr )
        {
            index_filename = argv[iArg];
            iArg++;
            break;
        }
    }

    if( index_filename == nullptr )
        Usage("No index filename specified.");
    if( iArg == argc )
        Usage("No file to index specified.");
    if( bSrcSRSFormatSpecified && pszSrcSRSName == nullptr )
        Usage("-src_srs_name must be specified when -src_srs_format is "
              "specified.");

/* -------------------------------------------------------------------- */
/*      Create and validate target SRS if given.                        */
/* -------------------------------------------------------------------- */
    OGRSpatialReferenceH hTargetSRS = nullptr;
    if( bSetTargetSRS )
    {
        if( skip_different_projection )
        {
            fprintf( stderr,
                     "Warning : -skip_different_projection does not apply "
                     "when -t_srs is requested.\n" );
        }
        hTargetSRS = OSRNewSpatialReference("");
        OSRSetAxisMappingStrategy(hTargetSRS, OAMS_TRADITIONAL_GIS_ORDER);
        // coverity[tainted_data]
        if( OSRSetFromUserInput( hTargetSRS, pszTargetSRS ) != CE_None )
        {
            OSRDestroySpatialReference( hTargetSRS );
            fprintf( stderr, "Invalid target SRS `%s'.\n",
                     pszTargetSRS );
            exit(1);
        }
    }

/* -------------------------------------------------------------------- */
/*      Open or create the target datasource                            */
/* -------------------------------------------------------------------- */
    GDALDatasetH hTileIndexDS = GDALOpenEx(
        index_filename, GDAL_OF_VECTOR | GDAL_OF_UPDATE, nullptr, nullptr, nullptr );
    OGRLayerH hLayer = nullptr;
    CPLString osFormat;
    if( hTileIndexDS != nullptr )
    {
        GDALDriverH hDriver = GDALGetDatasetDriver(hTileIndexDS);
        if( hDriver )
            osFormat = GDALGetDriverShortName(hDriver);

        if( GDALDatasetGetLayerCount(hTileIndexDS) == 1 )
        {
            hLayer = GDALDatasetGetLayer(hTileIndexDS, 0);
        }
        else
        {
            if( pszIndexLayerName == nullptr )
            {
                printf( "-lyr_name must be specified.\n" );
                exit( 1 );
            }
            CPLPushErrorHandler(CPLQuietErrorHandler);
            hLayer = GDALDatasetGetLayerByName(hTileIndexDS, pszIndexLayerName);
            CPLPopErrorHandler();
        }
    }
    else
    {
        printf( "Creating new index file...\n" );
        if( pszDriverName == nullptr )
        {
            std::vector<CPLString> aoDrivers =
                GetOutputDriversFor(index_filename, GDAL_OF_VECTOR);
            if( aoDrivers.empty() )
            {
                CPLError( CE_Failure, CPLE_AppDefined,
                        "Cannot guess driver for %s", index_filename);
                exit( 10 );
            }
            else
            {
                if( aoDrivers.size() > 1 )
                {
                    CPLError( CE_Warning, CPLE_AppDefined,
                            "Several drivers matching %s extension. Using %s",
                            CPLGetExtension(index_filename), aoDrivers[0].c_str() );
                }
                osFormat = aoDrivers[0];
            }
        }
        else
        {
            osFormat = pszDriverName;
        }
        if( !EQUAL(osFormat, "ESRI Shapefile") )
            nMaxFieldSize = 0;


        GDALDriverH hDriver = GDALGetDriverByName( osFormat.c_str() );
        if( hDriver == nullptr )
        {
            printf( "%s driver not available.\n", osFormat.c_str() );
            exit( 1 );
        }

        hTileIndexDS = 
            GDALCreate( hDriver, index_filename, 0, 0, 0, GDT_Unknown, nullptr );
    }

    if( hTileIndexDS != nullptr && hLayer == nullptr )
    {
        OGRSpatialReferenceH hSpatialRef = nullptr;
        char* pszLayerName = nullptr;
        if( pszIndexLayerName == nullptr )
        {
            VSIStatBuf sStat;
            if( EQUAL(osFormat, "ESRI Shapefile") ||
                VSIStat(index_filename, &sStat) == 0 )
            {
                pszLayerName = CPLStrdup(CPLGetBasename(index_filename));
            }
            else
            {
                printf( "-lyr_name must be specified.\n" );
                exit( 1 );
            }
        }
        else
        {
            pszLayerName = CPLStrdup(pszIndexLayerName);
        }

        /* get spatial reference for output file from target SRS (if set) */
        /* or from first input file */
        if( bSetTargetSRS )
        {
            hSpatialRef = OSRClone( hTargetSRS );
        }
        else
        {
            GDALDatasetH hDS = GDALOpen( argv[iArg], GA_ReadOnly );
            if( hDS )
            {
                const char* pszWKT = GDALGetProjectionRef(hDS);
                if (pszWKT != nullptr && pszWKT[0] != '\0')
                {
                    hSpatialRef = OSRNewSpatialReference(pszWKT);
                    OSRSetAxisMappingStrategy(hSpatialRef, OAMS_TRADITIONAL_GIS_ORDER);
                }
                GDALClose(hDS);
            }
        }

        hLayer =
            GDALDatasetCreateLayer( hTileIndexDS, pszLayerName, hSpatialRef,
                                wkbPolygon, nullptr );
        CPLFree(pszLayerName);
        if( hSpatialRef )
            OSRRelease(hSpatialRef);

        if( hLayer )
        {
            OGRFieldDefnH hFieldDefn = OGR_Fld_Create( tile_index, OFTString );
            if( nMaxFieldSize )
                OGR_Fld_SetWidth( hFieldDefn, static_cast<int>(nMaxFieldSize));
            OGR_L_CreateField( hLayer, hFieldDefn, TRUE );
            OGR_Fld_Destroy(hFieldDefn);
            if( pszSrcSRSName != nullptr )
            {
                hFieldDefn = OGR_Fld_Create( pszSrcSRSName, OFTString );
                if( nMaxFieldSize )
                    OGR_Fld_SetWidth(hFieldDefn,
                                     static_cast<int>(nMaxFieldSize));
                OGR_L_CreateField( hLayer, hFieldDefn, TRUE );
                OGR_Fld_Destroy(hFieldDefn);
            }
        }
    }

    if( hTileIndexDS == nullptr || hLayer == nullptr )
    {
        fprintf( stderr, "Unable to open/create shapefile `%s'.\n",
                 index_filename );
        exit(2);
    }

    OGRFeatureDefnH hFDefn = OGR_L_GetLayerDefn(hLayer);

    const int ti_field = OGR_FD_GetFieldIndex( hFDefn, tile_index );
    if( ti_field < 0 )
    {
        fprintf( stderr, "Unable to find field `%s' in file `%s'.\n",
                 tile_index, index_filename );
        exit(2);
    }

    if( pszSrcSRSName != nullptr )
        i_SrcSRSName = OGR_FD_GetFieldIndex( hFDefn, pszSrcSRSName );

    // Load in memory existing file names in SHP.
    int nExistingFiles = static_cast<int>(OGR_L_GetFeatureCount(hLayer, FALSE));
    if( nExistingFiles < 0)
        nExistingFiles = 0;

    char** existingFilesTab = nullptr;
    bool alreadyExistingProjectionRefValid = false;
    char* alreadyExistingProjectionRef = nullptr;
    if( nExistingFiles > 0 )
    {
        OGRFeatureH hFeature = nullptr;
        existingFilesTab = static_cast<char **>(
            CPLMalloc(nExistingFiles * sizeof(char*)));
        for( int i = 0; i < nExistingFiles; i++ )
        {
            hFeature = OGR_L_GetNextFeature(hLayer);
            existingFilesTab[i] =
                CPLStrdup(OGR_F_GetFieldAsString( hFeature, ti_field ));
            if( i == 0 )
            {
                GDALDatasetH hDS = GDALOpen(existingFilesTab[i], GA_ReadOnly );
                if( hDS )
                {
                    alreadyExistingProjectionRefValid = true;
                    alreadyExistingProjectionRef =
                        CPLStrdup(GDALGetProjectionRef(hDS));
                    GDALClose(hDS);
                }
            }
            OGR_F_Destroy( hFeature );
        }
    }

    if( write_absolute_path )
    {
        current_path = CPLGetCurrentDir();
        if (current_path == nullptr)
        {
            fprintf( stderr,
                     "This system does not support the CPLGetCurrentDir call. "
                     "The option -write_absolute_path will have no effect\n" );
            write_absolute_path = FALSE;
        }
    }

/* -------------------------------------------------------------------- */
/*      loop over GDAL files, processing.                               */
/* -------------------------------------------------------------------- */
    for( ; iArg < argc; iArg++ )
    {
        char *fileNameToWrite = nullptr;
        VSIStatBuf sStatBuf;

        // Make sure it is a file before building absolute path name.
        if( write_absolute_path && CPLIsFilenameRelative( argv[iArg] ) &&
            VSIStat( argv[iArg], &sStatBuf ) == 0 )
        {
            fileNameToWrite =
                CPLStrdup(CPLProjectRelativeFilename(current_path, argv[iArg]));
        }
        else
        {
            fileNameToWrite = CPLStrdup(argv[iArg]);
        }

        // Checks that file is not already in tileindex.
        {
            int i = 0;  // Used after for.
            for( ; i < nExistingFiles; i++ )
            {
                if (EQUAL(fileNameToWrite, existingFilesTab[i]))
                {
                    fprintf(stderr,
                            "File %s is already in tileindex. Skipping it.\n",
                            fileNameToWrite);
                    break;
                }
            }
            if (i != nExistingFiles)
            {
                CPLFree(fileNameToWrite);
                continue;
            }
        }

        GDALDatasetH hDS = GDALOpen( argv[iArg], GA_ReadOnly );
        if( hDS == nullptr )
        {
            fprintf( stderr, "Unable to open %s, skipping.\n",
                     argv[iArg] );
            CPLFree(fileNameToWrite);
            continue;
        }

        double adfGeoTransform[6] = { 0.0, 0.0, 0.0, 0.0, 0.0, 0.0 };
        GDALGetGeoTransform( hDS, adfGeoTransform );
        if( adfGeoTransform[0] == 0.0
            && adfGeoTransform[1] == 1.0
            && adfGeoTransform[3] == 0.0
            && std::abs(adfGeoTransform[5]) == 1.0 )
        {
            fprintf( stderr,
                     "It appears no georeferencing is available for\n"
                     "`%s', skipping.\n",
                     argv[iArg] );
            GDALClose( hDS );
            CPLFree(fileNameToWrite);
            continue;
        }

        const char *projectionRef = GDALGetProjectionRef(hDS);

        // If not set target srs, test that the current file uses same
        // projection as others.
        if( !bSetTargetSRS )
        {
            if( alreadyExistingProjectionRefValid )
            {
                int projectionRefNotNull, alreadyExistingProjectionRefNotNull;
                projectionRefNotNull = projectionRef && projectionRef[0];
                alreadyExistingProjectionRefNotNull =
                    alreadyExistingProjectionRef &&
                    alreadyExistingProjectionRef[0];
                if ((projectionRefNotNull &&
                     alreadyExistingProjectionRefNotNull &&
                     EQUAL(projectionRef, alreadyExistingProjectionRef) == 0) ||
                    (projectionRefNotNull != alreadyExistingProjectionRefNotNull))
                {
                    fprintf(
                        stderr,
                        "Warning : %s is not using the same projection system "
                        "as other files in the tileindex.\n"
                        "This may cause problems when using it in MapServer "
                        "for example.\n"
                        "Use -t_srs option to set target projection system "
                        "(not supported by MapServer).\n"
                        "%s\n", argv[iArg],
                        skip_different_projection ? "Skipping this file." : "");
                    if( skip_different_projection )
                    {
                        CPLFree(fileNameToWrite);
                        GDALClose( hDS );
                        continue;
                    }
                }
            }
            else
            {
                alreadyExistingProjectionRefValid = true;
                alreadyExistingProjectionRef = CPLStrdup(projectionRef);
            }
        }

        const int nXSize = GDALGetRasterXSize( hDS );
        const int nYSize = GDALGetRasterYSize( hDS );

        double adfX[5] = { 0.0, 0.0, 0.0, 0.0, 0.0 };
        double adfY[5] = { 0.0, 0.0, 0.0, 0.0, 0.0 };
        adfX[0] = adfGeoTransform[0]
            + 0 * adfGeoTransform[1]
            + 0 * adfGeoTransform[2];
        adfY[0] = adfGeoTransform[3]
            + 0 * adfGeoTransform[4]
            + 0 * adfGeoTransform[5];

        adfX[1] = adfGeoTransform[0]
            + nXSize * adfGeoTransform[1]
            + 0 * adfGeoTransform[2];
        adfY[1] = adfGeoTransform[3]
            + nXSize * adfGeoTransform[4]
            + 0 * adfGeoTransform[5];

        adfX[2] = adfGeoTransform[0]
            + nXSize * adfGeoTransform[1]
            + nYSize * adfGeoTransform[2];
        adfY[2] = adfGeoTransform[3]
            + nXSize * adfGeoTransform[4]
            + nYSize * adfGeoTransform[5];

        adfX[3] = adfGeoTransform[0]
            + 0 * adfGeoTransform[1]
            + nYSize * adfGeoTransform[2];
        adfY[3] = adfGeoTransform[3]
            + 0 * adfGeoTransform[4]
            + nYSize * adfGeoTransform[5];

        adfX[4] = adfGeoTransform[0]
            + 0 * adfGeoTransform[1]
            + 0 * adfGeoTransform[2];
        adfY[4] = adfGeoTransform[3]
            + 0 * adfGeoTransform[4]
            + 0 * adfGeoTransform[5];

        OGRSpatialReferenceH hSourceSRS = nullptr;
        if( (bSetTargetSRS || i_SrcSRSName >= 0) &&
            projectionRef != nullptr &&
            projectionRef[0] != '\0' )
        {
            hSourceSRS = OSRNewSpatialReference( projectionRef );
            OSRSetAxisMappingStrategy(hSourceSRS, OAMS_TRADITIONAL_GIS_ORDER);
        }

        // If set target srs, do the forward transformation of all points.
        if( bSetTargetSRS && projectionRef != nullptr && projectionRef[0] != '\0' )
        {
            OGRCoordinateTransformationH hCT = nullptr;
            if( hSourceSRS && !OSRIsSame( hSourceSRS, hTargetSRS ) )
            {
                hCT = OCTNewCoordinateTransformation( hSourceSRS, hTargetSRS );
                if( hCT == nullptr || !OCTTransform( hCT, 5, adfX, adfY, nullptr ) )
                {
                    fprintf(
                        stderr,
                        "Warning : unable to transform points from source "
                        "SRS `%s' to target SRS `%s'\n"
                        "for file `%s' - file skipped\n",
                        projectionRef, pszTargetSRS, fileNameToWrite );
                    if( hCT )
                        OCTDestroyCoordinateTransformation( hCT );
                    OSRDestroySpatialReference( hSourceSRS );
                    continue;
                }
                OCTDestroyCoordinateTransformation( hCT );
            }
        }

        OGRFeatureH hFeature = OGR_F_Create( OGR_L_GetLayerDefn( hLayer ) );
        OGR_F_SetFieldString( hFeature, ti_field, fileNameToWrite );

        if( i_SrcSRSName >= 0 && hSourceSRS != nullptr )
        {
            const char* pszAuthorityCode =
                OSRGetAuthorityCode(hSourceSRS, nullptr);
            const char* pszAuthorityName =
                OSRGetAuthorityName(hSourceSRS, nullptr);
            if( eSrcSRSFormat == FORMAT_AUTO )
            {
                if( pszAuthorityName != nullptr && pszAuthorityCode != nullptr )
                {
                    OGR_F_SetFieldString(
                        hFeature, i_SrcSRSName,
                        CPLSPrintf("%s:%s",
                                   pszAuthorityName, pszAuthorityCode) );
                }
                else if( nMaxFieldSize == 0 ||
                         strlen(projectionRef) <= nMaxFieldSize )
                {
                    OGR_F_SetFieldString(hFeature, i_SrcSRSName, projectionRef);
                }
                else
                {
                    char* pszProj4 = nullptr;
                    if( OSRExportToProj4(hSourceSRS, &pszProj4) == OGRERR_NONE )
                    {
                        OGR_F_SetFieldString( hFeature, i_SrcSRSName,
                                              pszProj4 );
                        CPLFree(pszProj4);
                    }
                    else
                    {
                        OGR_F_SetFieldString( hFeature, i_SrcSRSName,
                                              projectionRef );
                    }
                }
            }
            else if( eSrcSRSFormat == FORMAT_WKT )
            {
                if( nMaxFieldSize == 0 ||
                    strlen(projectionRef) <= nMaxFieldSize )
                {
                    OGR_F_SetFieldString( hFeature, i_SrcSRSName,
                                          projectionRef );
                }
                else
                {
                    fprintf(stderr,
                            "Cannot write WKT for file %s as it is too long!\n",
                            fileNameToWrite);
                }
            }
            else if( eSrcSRSFormat == FORMAT_PROJ )
            {
                char* pszProj4 = nullptr;
                if( OSRExportToProj4(hSourceSRS, &pszProj4) == OGRERR_NONE )
                {
                    OGR_F_SetFieldString( hFeature, i_SrcSRSName, pszProj4 );
                    CPLFree(pszProj4);
                }
            }
            else if( eSrcSRSFormat == FORMAT_EPSG )
            {
                if( pszAuthorityName != nullptr && pszAuthorityCode != nullptr )
                    OGR_F_SetFieldString(
                        hFeature, i_SrcSRSName,
                        CPLSPrintf("%s:%s",
                                   pszAuthorityName, pszAuthorityCode) );
            }
        }
        if( hSourceSRS )
            OSRDestroySpatialReference( hSourceSRS );

        OGRGeometryH hPoly = OGR_G_CreateGeometry(wkbPolygon);
        OGRGeometryH hRing = OGR_G_CreateGeometry(wkbLinearRing);
        for( int k = 0; k < 5; k++ )
            OGR_G_SetPoint_2D(hRing, k, adfX[k], adfY[k]);
        OGR_G_AddGeometryDirectly( hPoly, hRing );
        OGR_F_SetGeometryDirectly( hFeature, hPoly );

        if( OGR_L_CreateFeature( hLayer, hFeature ) != OGRERR_NONE )
        {
           printf( "Failed to create feature in shapefile.\n" );
           break;
        }

        OGR_F_Destroy( hFeature );

        CPLFree(fileNameToWrite);

        GDALClose( hDS );
    }

    CPLFree(current_path);

    if (nExistingFiles)
    {
        for( int i = 0; i < nExistingFiles; i++ )
        {
            CPLFree(existingFilesTab[i]);
        }
        CPLFree(existingFilesTab);
    }
    CPLFree(alreadyExistingProjectionRef);

    if ( hTargetSRS )
        OSRDestroySpatialReference( hTargetSRS );

    GDALClose( hTileIndexDS );

    GDALDestroyDriverManager();
    OGRCleanupAll();
    CSLDestroy(argv);

    exit( 0 );
}
MAIN_END
