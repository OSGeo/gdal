/******************************************************************************
 * $Id$
 *
 * Project:  GDAL Utilities
 * Purpose:  Commandline application to list info about a file.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 * ****************************************************************************
 * Copyright (c) 1998, Frank Warmerdam
 * Copyright (c) 2007-2013, Even Rouault <even dot rouault at mines-paris dot org>
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

#include "gdal.h"
#include "gdal_alg.h"
#include "ogr_srs_api.h"
#include "cpl_string.h"
#include "cpl_conv.h"
#include "cpl_multiproc.h"
#include "commonutils.h"
#include "json.h"
#include "ogrgeojsonwriter.h"

CPL_CVSID("$Id$");

static int 
GDALInfoReportCorner( GDALDatasetH hDataset, 
                      OGRCoordinateTransformationH hTransform, OGRCoordinateTransformationH hTransformWGS84,
                      const char * corner_name,
                      double x, double y,
                      int bJson, json_object *poCornerCoordinates,
                      json_object *poWGS84ExtentCoordinates );

static void
GDALInfoReportMetadata( GDALMajorObjectH hObject,
                        int bListMDD,
                        int bShowMetadata,
                        char **papszExtraMDDomains,
                        int bIsBand,
                        int bJson,
                        json_object *poMetadata );



/************************************************************************/
/*                               Usage()                                */
/************************************************************************/

void Usage(const char* pszErrorMsg)

{
    printf( "Usage: gdalinfo [--help-general] [-json] [-mm] [-stats] [-hist] [-nogcp] [-nomd]\n"
            "                [-norat] [-noct] [-nofl] [-checksum] [-proj4]\n"
            "                [-listmdd] [-mdd domain|`all`]*\n"
            "                [-sd subdataset] [-oo NAME=VALUE]* datasetname\n" );

    if( pszErrorMsg != NULL )
        fprintf(stderr, "\nFAILURE: %s\n", pszErrorMsg);

    exit( 1 );
}

/************************************************************************/
/*                                main()                                */
/************************************************************************/

#define CHECK_HAS_ENOUGH_ADDITIONAL_ARGS(nExtraArg) \
    do { if (i + nExtraArg >= argc) \
        Usage(CPLSPrintf("%s option requires %d argument(s)", argv[i], nExtraArg)); } while(0)

int main( int argc, char ** argv ) 

{
    GDALDatasetH    hDataset = NULL;
    GDALRasterBandH hBand = NULL;
    int         i, iBand;
    double      adfGeoTransform[6];
    GDALDriverH     hDriver;
    int                 bComputeMinMax = FALSE, bSample = FALSE;
    int                 bShowGCPs = TRUE, bShowMetadata = TRUE, bShowRAT=TRUE;
    int                 bStats = FALSE, bApproxStats = TRUE;
    int                 bShowColorTable = TRUE, bComputeChecksum = FALSE;
    int                 bReportHistograms = FALSE;
    int                 bReportProj4 = FALSE;
    int                 nSubdataset = -1;
    const char          *pszFilename = NULL;
    char              **papszExtraMDDomains = NULL, **papszFileList;
    int                 bListMDD = FALSE;
    const char  *pszProjection = NULL;
    OGRCoordinateTransformationH hTransform = NULL, hTransformWGS84 = NULL;
    int             bShowFileList = TRUE;
    char              **papszOpenOptions = NULL;

    int bJson = FALSE;
    json_object *poJsonObject = NULL, *poBands = NULL, *poMetadata = NULL;

    /* Check that we are running against at least GDAL 1.5 */
    /* Note to developers : if we use newer API, please change the requirement */
    if (atoi(GDALVersionInfo("VERSION_NUM")) < 1500)
    {
        fprintf(stderr, "At least, GDAL >= 1.5.0 is required for this version of %s, "
                "which was compiled against GDAL %s\n", argv[0], GDAL_RELEASE_NAME);
        exit(1);
    }

    EarlySetConfigOptions(argc, argv);

    GDALAllRegister();

    argc = GDALGeneralCmdLineProcessor( argc, &argv, 0 );
    if( argc < 1 )
        exit( -argc );

/* -------------------------------------------------------------------- */
/*      Parse arguments.                                                */
/* -------------------------------------------------------------------- */
    for( i = 1; i < argc; i++ )
    {
        if( EQUAL(argv[i], "--utility_version") )
        {
            printf("%s was compiled against GDAL %s and is running against GDAL %s\n",
                   argv[0], GDAL_RELEASE_NAME, GDALVersionInfo("RELEASE_NAME"));
            return 0;
        }
        else if( EQUAL(argv[i],"--help") )
            Usage(NULL);
        else if( EQUAL(argv[i],"-json") )
            bJson = TRUE;
        else if( EQUAL(argv[i], "-mm") )
            bComputeMinMax = TRUE;
        else if( EQUAL(argv[i], "-hist") )
            bReportHistograms = TRUE;
        else if( EQUAL(argv[i], "-proj4") )
            bReportProj4 = TRUE;
        else if( EQUAL(argv[i], "-stats") )
        {
            bStats = TRUE;
            bApproxStats = FALSE;
        }
        else if( EQUAL(argv[i], "-approx_stats") )
        {
            bStats = TRUE;
            bApproxStats = TRUE;
        }
        else if( EQUAL(argv[i], "-sample") )
            bSample = TRUE;
        else if( EQUAL(argv[i], "-checksum") )
            bComputeChecksum = TRUE;
        else if( EQUAL(argv[i], "-nogcp") )
            bShowGCPs = FALSE;
        else if( EQUAL(argv[i], "-nomd") )
            bShowMetadata = FALSE;
        else if( EQUAL(argv[i], "-norat") )
            bShowRAT = FALSE;
        else if( EQUAL(argv[i], "-noct") )
            bShowColorTable = FALSE;
        else if( EQUAL(argv[i], "-listmdd") )
            bListMDD = TRUE;
        else if( EQUAL(argv[i], "-mdd") )
        {
            CHECK_HAS_ENOUGH_ADDITIONAL_ARGS(1);
            papszExtraMDDomains = CSLAddString( papszExtraMDDomains,
                                                argv[++i] );
        }
        else if( EQUAL(argv[i], "-oo") )
        {
            CHECK_HAS_ENOUGH_ADDITIONAL_ARGS(1);
            papszOpenOptions = CSLAddString( papszOpenOptions,
                                                argv[++i] );
        }
        else if( EQUAL(argv[i], "-nofl") )
            bShowFileList = FALSE;
        else if( EQUAL(argv[i], "-sd") )
        {
            CHECK_HAS_ENOUGH_ADDITIONAL_ARGS(1);
            nSubdataset = atoi(argv[++i]);
        }
        else if( argv[i][0] == '-' )
            Usage(CPLSPrintf("Unknown option name '%s'", argv[i]));
        else if( pszFilename == NULL )
            pszFilename = argv[i];
        else
            Usage("Too many command options.");
    }

    if( pszFilename == NULL )
        Usage("No datasource specified.");

/* -------------------------------------------------------------------- */
/*      Open dataset.                                                   */
/* -------------------------------------------------------------------- */
    hDataset = GDALOpenEx( pszFilename, GDAL_OF_READONLY | GDAL_OF_RASTER, NULL,
                           (const char* const* )papszOpenOptions, NULL );

    if( hDataset == NULL )
    {
        fprintf( stderr,
                 "gdalinfo failed - unable to open '%s'.\n",
                 pszFilename );

/* -------------------------------------------------------------------- */
/*      If argument is a VSIFILE, then print its contents               */
/* -------------------------------------------------------------------- */
        if ( strncmp( pszFilename, "/vsizip/", 8 ) == 0 || 
             strncmp( pszFilename, "/vsitar/", 8 ) == 0 ) 
        {
            papszFileList = VSIReadDirRecursive( pszFilename );
            if ( papszFileList )
            {
                int nCount = CSLCount( papszFileList );
                fprintf( stdout, 
                         "Unable to open source `%s' directly.\n"
                         "The archive contains %d files:\n", 
                         pszFilename, nCount );
                for ( i = 0; i < nCount; i++ )
                {
                    fprintf( stdout, "       %s/%s\n", pszFilename, papszFileList[i] );
                }
                CSLDestroy( papszFileList );
                papszFileList = NULL;
            }
        }

        CSLDestroy( argv );
        CSLDestroy( papszExtraMDDomains );
        CSLDestroy( papszOpenOptions );
    
        GDALDumpOpenDatasets( stderr );

        GDALDestroyDriverManager();

        CPLDumpSharedList( NULL );

        exit( 1 );
    }
    
/* -------------------------------------------------------------------- */
/*      Read specified subdataset if requested.                         */
/* -------------------------------------------------------------------- */
    if ( nSubdataset > 0 )
    {
        char **papszSubdatasets = GDALGetMetadata( hDataset, "SUBDATASETS" );
        int nSubdatasets = CSLCount( papszSubdatasets );

        if ( nSubdatasets > 0 && nSubdataset <= nSubdatasets )
        {
            char szKeyName[1024];
            char *pszSubdatasetName;

            snprintf( szKeyName, sizeof(szKeyName),
                      "SUBDATASET_%d_NAME", nSubdataset );
            szKeyName[sizeof(szKeyName) - 1] = '\0';
            pszSubdatasetName =
                CPLStrdup( CSLFetchNameValue( papszSubdatasets, szKeyName ) );
            GDALClose( hDataset );
            hDataset = GDALOpen( pszSubdatasetName, GA_ReadOnly );
            CPLFree( pszSubdatasetName );
        }
        else
        {
            fprintf( stderr,
                     "gdalinfo warning: subdataset %d of %d requested. "
                     "Reading the main dataset.\n",
                     nSubdataset, nSubdatasets );

        }
    }

/* -------------------------------------------------------------------- */
/*      Report general info.                                            */
/* -------------------------------------------------------------------- */
    hDriver = GDALGetDatasetDriver( hDataset );
    if(bJson)
    {
        json_object *poDescription = json_object_new_string(GDALGetDescription(hDataset));
        json_object *poDriverShortName = json_object_new_string(GDALGetDriverShortName(hDriver));
        json_object *poDriverLongName = json_object_new_string(GDALGetDriverLongName(hDriver));
        poJsonObject = json_object_new_object();
        poBands = json_object_new_array();
        poMetadata = json_object_new_object();

        json_object_object_add(poJsonObject, "description", poDescription);
        json_object_object_add(poJsonObject, "driverShortName", poDriverShortName);
        json_object_object_add(poJsonObject, "driverLongName", poDriverLongName);
    }
    else
    {
        printf( "Driver: %s/%s\n",
                GDALGetDriverShortName( hDriver ),
                GDALGetDriverLongName( hDriver ) );

    }

    papszFileList = GDALGetFileList( hDataset );

    if( CSLCount(papszFileList) == 0 )
    {   
        if(bJson)
        {
            json_object *poFiles = json_object_new_array();   
            json_object_object_add(poJsonObject, "files", poFiles);
        }
        else
            printf( "Files: none associated\n" );
    }
    else
    {
        if(bJson)
        {
            json_object *poFiles = json_object_new_array();

            for(i = 0; papszFileList[i] != NULL; i++)
            {
                json_object *poFile = json_object_new_string(papszFileList[i]);
                
                json_object_array_add(poFiles, poFile);
            }
            
            json_object_object_add(poJsonObject, "files", poFiles);
        }
        else
        {
            printf( "Files: %s\n", papszFileList[0] );
            if( bShowFileList )
            {
                for( i = 1; papszFileList[i] != NULL; i++ )
                    printf( "       %s\n", papszFileList[i] );
            }
        }

    }
    CSLDestroy( papszFileList );

    if(bJson)
    {
        json_object *poSize = json_object_new_array();
        json_object *poSizeX = json_object_new_int(GDALGetRasterXSize(hDataset));
        json_object *poSizeY = json_object_new_int(GDALGetRasterYSize(hDataset));
        
        json_object_array_add(poSize, poSizeX);
        json_object_array_add(poSize, poSizeY);
        json_object_object_add(poJsonObject, "size", poSize);
    }
    else
        printf( "Size is %d, %d\n",
            GDALGetRasterXSize( hDataset ),
            GDALGetRasterYSize( hDataset ) );

/* -------------------------------------------------------------------- */
/*      Report projection.                                              */
/* -------------------------------------------------------------------- */
    if( GDALGetProjectionRef( hDataset ) != NULL )
    {
        OGRSpatialReferenceH  hSRS;
        char              *pszProjection;
        json_object *poCoordinateSystem = NULL;

        if(bJson)
            poCoordinateSystem = json_object_new_object();

        pszProjection = (char *) GDALGetProjectionRef( hDataset );

        hSRS = OSRNewSpatialReference(NULL);
        if( OSRImportFromWkt( hSRS, &pszProjection ) == CE_None )
        {
            char    *pszPrettyWkt = NULL;

            OSRExportToPrettyWkt( hSRS, &pszPrettyWkt, FALSE );
            
            if(bJson)
            {
                json_object *poWkt = json_object_new_string(pszPrettyWkt);
                json_object_object_add(poCoordinateSystem, "wkt", poWkt);
            }
            else
                printf( "Coordinate System is:\n%s\n", pszPrettyWkt );
            CPLFree( pszPrettyWkt );
        }
        else
        {
            if(bJson)
            {
                json_object *poWkt = json_object_new_string(GDALGetProjectionRef(hDataset));
                json_object_object_add(poCoordinateSystem, "wkt", poWkt);
            }
            else
                printf( "Coordinate System is `%s'\n",
                    GDALGetProjectionRef( hDataset ) );

        }

        if ( bReportProj4 )
        {
            char *pszProj4 = NULL;
            OSRExportToProj4( hSRS, &pszProj4 );
            
            if(bJson)
            {
                json_object *proj4 = json_object_new_string(pszProj4);
                json_object_object_add(poCoordinateSystem, "proj4", proj4);
            }
            else
                printf("PROJ.4 string is:\n\'%s\'\n",pszProj4);
            CPLFree( pszProj4 );
        }

        if(bJson)
            json_object_object_add(poJsonObject, "coordinateSystem", poCoordinateSystem);

        OSRDestroySpatialReference( hSRS );
    }

/* -------------------------------------------------------------------- */
/*      Report Geotransform.                                            */
/* -------------------------------------------------------------------- */
    if( GDALGetGeoTransform( hDataset, adfGeoTransform ) == CE_None )
    {
        if(bJson)
        {
            json_object *poGeoTransform = json_object_new_array();
            
            for(i = 0; i < 6; i++)
            {
                json_object *poGeoTransformCoefficient = json_object_new_double_with_precision(adfGeoTransform[i], 16);
                json_object_array_add(poGeoTransform, poGeoTransformCoefficient);
            }

            json_object_object_add(poJsonObject, "geoTransform", poGeoTransform);
        }
        else
        {
            if( adfGeoTransform[2] == 0.0 && adfGeoTransform[4] == 0.0 )
            {
                CPLprintf( "Origin = (%.15f,%.15f)\n",
                        adfGeoTransform[0], adfGeoTransform[3] );

                CPLprintf( "Pixel Size = (%.15f,%.15f)\n",
                        adfGeoTransform[1], adfGeoTransform[5] );
            }
            else
                CPLprintf( "GeoTransform =\n"
                        "  %.16g, %.16g, %.16g\n"
                        "  %.16g, %.16g, %.16g\n",
                        adfGeoTransform[0],
                        adfGeoTransform[1],
                        adfGeoTransform[2],
                        adfGeoTransform[3],
                        adfGeoTransform[4],
                        adfGeoTransform[5] );
        }

    }

/* -------------------------------------------------------------------- */
/*      Report GCPs.                                                    */
/* -------------------------------------------------------------------- */
    if( bShowGCPs && GDALGetGCPCount( hDataset ) > 0 )
    {
        json_object *poGCPs = NULL, *poGCPList = NULL;

        if(bJson)
            poGCPs = json_object_new_object();
        
        if (GDALGetGCPProjection(hDataset) != NULL)
        {
            json_object *poGCPCoordinateSystem = NULL;
            OGRSpatialReferenceH  hSRS;
            char              *pszProjection;

            pszProjection = (char *) GDALGetGCPProjection( hDataset );

            hSRS = OSRNewSpatialReference(NULL);
            if( OSRImportFromWkt( hSRS, &pszProjection ) == CE_None )
            {
                char    *pszPrettyWkt = NULL;

                OSRExportToPrettyWkt( hSRS, &pszPrettyWkt, FALSE );
                if(bJson)
                {
                    json_object *poWkt = json_object_new_string(pszPrettyWkt);
                    poGCPCoordinateSystem = json_object_new_object();
                    
                    json_object_object_add(poGCPCoordinateSystem, "wkt", poWkt);
                }
                else
                    printf( "GCP Projection = \n%s\n", pszPrettyWkt );
                CPLFree( pszPrettyWkt );
            }
            else
            {
                if(bJson)
                {
                    json_object *poWkt = json_object_new_string(GDALGetGCPProjection(hDataset));
                    poGCPCoordinateSystem = json_object_new_object();
                    
                    json_object_object_add(poGCPCoordinateSystem, "wkt", poWkt);
                }
                else
                    printf( "GCP Projection = %s\n",
                        GDALGetGCPProjection( hDataset ) );

            }

            if(bJson)
                json_object_object_add(poGCPs, "coordinateSystem", poGCPCoordinateSystem);
            OSRDestroySpatialReference( hSRS );
        }

        if(bJson)
            poGCPList = json_object_new_array();

        for( i = 0; i < GDALGetGCPCount(hDataset); i++ )
        {
            const GDAL_GCP  *psGCP;

            psGCP = GDALGetGCPs( hDataset ) + i;
            if(bJson)
            {
                json_object *poGCP = json_object_new_object();
                json_object *poId = json_object_new_string(psGCP->pszId);
                json_object *poInfo = json_object_new_string(psGCP->pszInfo);
                json_object *poPixel = json_object_new_double_with_precision(psGCP->dfGCPPixel, 15);
                json_object *poLine = json_object_new_double_with_precision(psGCP->dfGCPLine, 15);
                json_object *poX = json_object_new_double_with_precision(psGCP->dfGCPX, 15);
                json_object *poY = json_object_new_double_with_precision(psGCP->dfGCPY, 15);
                json_object *poZ = json_object_new_double_with_precision(psGCP->dfGCPZ, 15);
                
                json_object_object_add(poGCP, "id", poId);
                json_object_object_add(poGCP, "info", poInfo);
                json_object_object_add(poGCP, "pixel", poPixel);
                json_object_object_add(poGCP, "line", poLine);
                json_object_object_add(poGCP, "x", poX);
                json_object_object_add(poGCP, "y", poY);
                json_object_object_add(poGCP, "z", poZ);
                json_object_array_add(poGCPList, poGCP);
            }
            else
                CPLprintf( "GCP[%3d]: Id=%s, Info=%s\n"
                    "          (%.15g,%.15g) -> (%.15g,%.15g,%.15g)\n",
                    i, psGCP->pszId, psGCP->pszInfo,
                    psGCP->dfGCPPixel, psGCP->dfGCPLine,
                    psGCP->dfGCPX, psGCP->dfGCPY, psGCP->dfGCPZ );
        }
        if(bJson)
        {
            json_object_object_add(poGCPs, "gcpList", poGCPList);
            json_object_object_add(poJsonObject, "gcps", poGCPs);
        }
    }

/* -------------------------------------------------------------------- */
/*      Report metadata.                                                */
/* -------------------------------------------------------------------- */

    GDALInfoReportMetadata( hDataset, bListMDD, bShowMetadata, papszExtraMDDomains, FALSE, bJson, poMetadata );
    if(bJson && bShowMetadata)
        json_object_object_add( poJsonObject, "metadata", poMetadata );

/* -------------------------------------------------------------------- */
/*      Setup projected to lat/long transform if appropriate.           */
/* -------------------------------------------------------------------- */
    if( GDALGetGeoTransform( hDataset, adfGeoTransform ) == CE_None )
        pszProjection = GDALGetProjectionRef(hDataset);

    if( pszProjection != NULL && strlen(pszProjection) > 0 )
    {
        OGRSpatialReferenceH hProj, hLatLong = NULL, hLatLongWGS84 = NULL;

        hProj = OSRNewSpatialReference( pszProjection );
        if( hProj != NULL )
        {
            hLatLong = OSRCloneGeogCS( hProj );

            if(bJson)
            {
                hLatLongWGS84 = OSRNewSpatialReference( NULL );
                OSRSetWellKnownGeogCS( hLatLongWGS84, "WGS84" );
            }
        }

        if( hLatLong != NULL )
        {
            CPLPushErrorHandler( CPLQuietErrorHandler );
            hTransform = OCTNewCoordinateTransformation( hProj, hLatLong );
            CPLPopErrorHandler();
            
            OSRDestroySpatialReference( hLatLong );
        }

        if( hLatLongWGS84 != NULL )
        {
            CPLPushErrorHandler( CPLQuietErrorHandler );
            hTransformWGS84 = OCTNewCoordinateTransformation( hProj, hLatLongWGS84 );
            CPLPopErrorHandler();
            
            OSRDestroySpatialReference( hLatLongWGS84 );
        }

        if( hProj != NULL )
            OSRDestroySpatialReference( hProj );
    }

/* -------------------------------------------------------------------- */
/*      Report corners.                                                 */
/* -------------------------------------------------------------------- */
    if(bJson)
    {
        json_object *poLinearRing = json_object_new_array();
        json_object *poCornerCoordinates = json_object_new_object();
        json_object *poWGS84Extent = json_object_new_object();
        json_object *poWGS84ExtentType = json_object_new_string("Polygon");
        json_object *poWGS84ExtentCoordinates = json_object_new_array();

        GDALInfoReportCorner( hDataset, hTransform, hTransformWGS84, "upperLeft",
                              0.0, 0.0, bJson, poCornerCoordinates, poWGS84ExtentCoordinates );
        GDALInfoReportCorner( hDataset, hTransform, hTransformWGS84, "lowerLeft",
                              0.0, GDALGetRasterYSize(hDataset), bJson, poCornerCoordinates, poWGS84ExtentCoordinates );
        GDALInfoReportCorner( hDataset, hTransform, hTransformWGS84, "upperRight",
                              GDALGetRasterXSize(hDataset), 0.0, bJson, poCornerCoordinates, poWGS84ExtentCoordinates );
        GDALInfoReportCorner( hDataset, hTransform, hTransformWGS84, "lowerRight",
                              GDALGetRasterXSize(hDataset), GDALGetRasterYSize(hDataset),
                              bJson, poCornerCoordinates, poWGS84ExtentCoordinates );
        GDALInfoReportCorner( hDataset, hTransform, hTransformWGS84, "center",
                              GDALGetRasterXSize(hDataset)/2.0, GDALGetRasterYSize(hDataset)/2.0,
                              bJson, poCornerCoordinates, poWGS84ExtentCoordinates );
        GDALInfoReportCorner( hDataset, hTransform, hTransformWGS84, "upperLeft",
                              0.0, 0.0, bJson, poCornerCoordinates, poWGS84ExtentCoordinates );
        
        json_object_object_add( poJsonObject, "cornerCoordinates", poCornerCoordinates );
        json_object_object_add( poWGS84Extent, "type", poWGS84ExtentType );
        json_object_array_add( poLinearRing, poWGS84ExtentCoordinates );
        json_object_object_add( poWGS84Extent, "coordinates", poLinearRing );
        json_object_object_add( poJsonObject, "wgs84Extent", poWGS84Extent );
    }
    else
    {
        printf( "Corner Coordinates:\n" );
        GDALInfoReportCorner( hDataset, hTransform, hTransformWGS84, "Upper Left",
                              0.0, 0.0, bJson, NULL, NULL );
        GDALInfoReportCorner( hDataset, hTransform, hTransformWGS84, "Lower Left",
                              0.0, GDALGetRasterYSize(hDataset), bJson, NULL, NULL );
        GDALInfoReportCorner( hDataset, hTransform, hTransformWGS84, "Upper Right",
                              GDALGetRasterXSize(hDataset), 0.0, bJson, NULL, NULL );
        GDALInfoReportCorner( hDataset, hTransform, hTransformWGS84, "Lower Right",
                              GDALGetRasterXSize(hDataset),
                              GDALGetRasterYSize(hDataset), bJson, NULL, NULL );
        GDALInfoReportCorner( hDataset, hTransform, hTransformWGS84, "Center",
                              GDALGetRasterXSize(hDataset)/2.0,
                              GDALGetRasterYSize(hDataset)/2.0, bJson, NULL, NULL );

    }

    if( hTransform != NULL )
    {
        OCTDestroyCoordinateTransformation( hTransform );
        hTransform = NULL;
    }

    if( hTransformWGS84 != NULL )
    {
        OCTDestroyCoordinateTransformation( hTransformWGS84 );
        hTransformWGS84 = NULL;
    }

/* ==================================================================== */
/*      Loop over bands.                                                */
/* ==================================================================== */
    for( iBand = 0; iBand < GDALGetRasterCount( hDataset ); iBand++ )
    {
        double      dfMin, dfMax, adfCMinMax[2], dfNoData;
        int         bGotMin, bGotMax, bGotNodata, bSuccess;
        int         nBlockXSize, nBlockYSize, nMaskFlags;
        double      dfMean, dfStdDev;
        GDALColorTableH hTable;
        CPLErr      eErr;
        json_object *poBand = NULL, *poBandMetadata = NULL;

        if(bJson)
        {
            poBand = json_object_new_object();
            poBandMetadata = json_object_new_object();
        }

        hBand = GDALGetRasterBand( hDataset, iBand+1 );

        if( bSample )
        {
            float afSample[10000];
            int   nCount;

            nCount = GDALGetRandomRasterSample( hBand, 10000, afSample );
            if(!bJson)
                printf( "Got %d samples.\n", nCount );
        }
        
        GDALGetBlockSize( hBand, &nBlockXSize, &nBlockYSize );
        if(bJson)
        {
            json_object *poBandNumber = json_object_new_int(iBand+1);
            json_object *poBlock = json_object_new_array();
            json_object *poType = json_object_new_string(GDALGetDataTypeName(GDALGetRasterDataType(hBand)));
            json_object *poColorInterp = json_object_new_string(GDALGetColorInterpretationName(
                GDALGetRasterColorInterpretation(hBand)));

            json_object_array_add(poBlock, json_object_new_int(nBlockXSize));
            json_object_array_add(poBlock, json_object_new_int(nBlockYSize));
            json_object_object_add(poBand, "band", poBandNumber);
            json_object_object_add(poBand, "block", poBlock);
            json_object_object_add(poBand, "type", poType);
            json_object_object_add(poBand, "colorInterpretation", poColorInterp);
        }
        else
            printf( "Band %d Block=%dx%d Type=%s, ColorInterp=%s\n", iBand+1,
                nBlockXSize, nBlockYSize,
                GDALGetDataTypeName(
                    GDALGetRasterDataType(hBand)),
                GDALGetColorInterpretationName(
                    GDALGetRasterColorInterpretation(hBand)) );

        if( GDALGetDescription( hBand ) != NULL 
            && strlen(GDALGetDescription( hBand )) > 0 )
        {
            if(bJson)
            {
                json_object *poBandDescription = json_object_new_string(GDALGetDescription(hBand));
                json_object_object_add(poBand, "description", poBandDescription);
            }
            else
                printf( "  Description = %s\n", GDALGetDescription(hBand) );
        }

        dfMin = GDALGetRasterMinimum( hBand, &bGotMin );
        dfMax = GDALGetRasterMaximum( hBand, &bGotMax );
        if( bGotMin || bGotMax || bComputeMinMax )
        {
            if(!bJson)
                printf( "  " );
            if( bGotMin )
            {
                if(bJson)
                {
                    json_object *poMin = json_object_new_double_with_precision(dfMin, 3);
                    json_object_object_add(poBand, "min", poMin);
                }
                else
                    CPLprintf( "Min=%.3f ", dfMin );
            }
            if( bGotMax )
            {
                if(bJson)
                {
                    json_object *poMax = json_object_new_double_with_precision(dfMax, 3);
                    json_object_object_add(poBand, "max", poMax);
                }
                else
                    CPLprintf( "Max=%.3f ", dfMax );
            }

            if( bComputeMinMax )
            {
                CPLErrorReset();
                GDALComputeRasterMinMax( hBand, FALSE, adfCMinMax );
                if (CPLGetLastErrorType() == CE_None)
                {
                    if(bJson)
                    {
                        json_object *poComputedMin = json_object_new_double_with_precision(adfCMinMax[0], 3);
                        json_object *poComputedMax = json_object_new_double_with_precision(adfCMinMax[1], 3);
                        json_object_object_add(poBand, "computedMin", poComputedMin);
                        json_object_object_add(poBand, "computedMax", poComputedMax);
                    }
                    else
                        CPLprintf( "  Computed Min/Max=%.3f,%.3f",
                          adfCMinMax[0], adfCMinMax[1] );
                }
            }
            if(!bJson)
                printf( "\n" );
        }

        eErr = GDALGetRasterStatistics( hBand, bApproxStats, bStats, 
                                        &dfMin, &dfMax, &dfMean, &dfStdDev );
        if( eErr == CE_None )
        {
            if(bJson)
            {
                json_object *poMinimum = json_object_new_double_with_precision(dfMin, 3);
                json_object *poMaximum = json_object_new_double_with_precision(dfMax, 3);
                json_object *poMean = json_object_new_double_with_precision(dfMean, 3);
                json_object *poStdDev = json_object_new_double_with_precision(dfStdDev, 3);
                
                json_object_object_add(poBand, "minimum", poMinimum);
                json_object_object_add(poBand, "maximum", poMaximum);
                json_object_object_add(poBand, "mean", poMean);
                json_object_object_add(poBand, "stdDev", poStdDev);
            }
            else
                CPLprintf( "  Minimum=%.3f, Maximum=%.3f, Mean=%.3f, StdDev=%.3f\n",
                    dfMin, dfMax, dfMean, dfStdDev );
        }

        if( bReportHistograms )
        {
            int nBucketCount;
            GUIntBig *panHistogram = NULL;

            if(bJson)
                eErr = GDALGetDefaultHistogramEx( hBand, &dfMin, &dfMax, 
                                            &nBucketCount, &panHistogram, 
                                            TRUE, GDALDummyProgress, NULL );
            else
                eErr = GDALGetDefaultHistogramEx( hBand, &dfMin, &dfMax, 
                                            &nBucketCount, &panHistogram, 
                                            TRUE, GDALTermProgress, NULL );
            if( eErr == CE_None )
            {
                int iBucket;
                json_object *poHistogram = NULL, *poBuckets = NULL;

                if(bJson)
                {
                    json_object *poCount = json_object_new_int(nBucketCount);
                    json_object *poMin = json_object_new_double(dfMin);
                    json_object *poMax = json_object_new_double(dfMax);
                    
                    poBuckets = json_object_new_array();
                    poHistogram = json_object_new_object();
                    json_object_object_add(poHistogram, "count", poCount);
                    json_object_object_add(poHistogram, "min", poMin);
                    json_object_object_add(poHistogram, "max", poMax);
                }
                else
                    printf( "  %d buckets from %g to %g:\n  ",
                        nBucketCount, dfMin, dfMax );

                for( iBucket = 0; iBucket < nBucketCount; iBucket++ )
                {
                    if(bJson)
                    {
                        json_object *poBucket = json_object_new_int64(panHistogram[iBucket]);
                        json_object_array_add(poBuckets, poBucket);
                    }
                    else
                        printf( CPL_FRMT_GUIB " ", panHistogram[iBucket] );
                }
                if(bJson)
                {
                    json_object_object_add(poHistogram, "buckets", poBuckets);
                    json_object_object_add(poBand, "histogram", poHistogram);
                }
                else
                    printf( "\n" );
                CPLFree( panHistogram );
            }
        }

        if ( bComputeChecksum)
        {
            int nBandChecksum = GDALChecksumImage(hBand, 0, 0,
                                      GDALGetRasterXSize(hDataset),
                                      GDALGetRasterYSize(hDataset));
            if(bJson)
            {
                json_object *poChecksum = json_object_new_int(nBandChecksum);
                json_object_object_add(poBand, "checksum", poChecksum);
            }
            else
                printf( "  Checksum=%d\n", nBandChecksum );
        }

        dfNoData = GDALGetRasterNoDataValue( hBand, &bGotNodata );
        if( bGotNodata )
        {
            if (CPLIsNan(dfNoData))
            {
                if(bJson)
                {
                    json_object *poNoDataValue = json_object_new_string("nan");
                    json_object_object_add(poBand, "noDataValue", poNoDataValue);
                }
                else
                    printf( "  NoData Value=nan\n" );
            }
            else
            {
                if(bJson)
                {
                    json_object *poNoDataValue = json_object_new_double_with_precision(dfNoData, 18);
                    json_object_object_add(poBand, "noDataValue", poNoDataValue);
                }
                else
                    CPLprintf( "  NoData Value=%.18g\n", dfNoData );
            }
        }

        if( GDALGetOverviewCount(hBand) > 0 )
        {
            int     iOverview;
            json_object *poOverviews = NULL;
            
            if(bJson)
                poOverviews = json_object_new_array();
            else
                printf( "  Overviews: " );

            for( iOverview = 0; 
                 iOverview < GDALGetOverviewCount(hBand);
                 iOverview++ )
            {
                GDALRasterBandH hOverview;
                const char *pszResampling = NULL;
                json_object *poOverview;

                if(!bJson)
                    if( iOverview != 0 )
                        printf( ", " );

                hOverview = GDALGetOverview( hBand, iOverview );
                if (hOverview != NULL)
                {
                    if(bJson)
                    {
                        json_object *poOverviewSize = json_object_new_array();
                        json_object *poOverviewSizeX = json_object_new_int( GDALGetRasterBandXSize( hOverview) );
                        json_object *poOverviewSizeY = json_object_new_int( GDALGetRasterBandYSize( hOverview) );
                        
                        poOverview = json_object_new_object();
                        json_object_array_add( poOverviewSize, poOverviewSizeX );
                        json_object_array_add( poOverviewSize, poOverviewSizeY );
                        json_object_object_add( poOverview, "size", poOverviewSize );

                        if(bComputeChecksum)
                        {
                            int nOverviewChecksum = GDALChecksumImage(hOverview, 0, 0,
                                        GDALGetRasterBandXSize(hOverview),
                                        GDALGetRasterBandYSize(hOverview));
                            json_object *poOverviewChecksum = json_object_new_int(nOverviewChecksum);
                            json_object_object_add(poOverview, "checksum", poOverviewChecksum);
                        }
                        json_object_array_add(poOverviews, poOverview);
                    }
                    else
                        printf( "%dx%d", 
                            GDALGetRasterBandXSize( hOverview ),
                            GDALGetRasterBandYSize( hOverview ) );

                    pszResampling = 
                        GDALGetMetadataItem( hOverview, "RESAMPLING", "" );

                    if( pszResampling != NULL && !bJson 
                        && EQUALN(pszResampling,"AVERAGE_BIT2",12) )
                        printf( "*" );
                }
                else
                    if(!bJson)
                        printf( "(null)" );
            }
            if(bJson)
                json_object_object_add(poBand, "overviews", poOverviews);
            else
                printf( "\n" );

            if ( bComputeChecksum && !bJson )
            {
                printf( "  Overviews checksum: " );
                
                for( iOverview = 0; 
                    iOverview < GDALGetOverviewCount(hBand);
                    iOverview++ )
                {
                    GDALRasterBandH hOverview;

                    if( iOverview != 0 )
                        printf( ", " );

                    hOverview = GDALGetOverview( hBand, iOverview );
                    if (hOverview)
                    {
                        printf( "%d",
                                GDALChecksumImage(hOverview, 0, 0,
                                        GDALGetRasterBandXSize(hOverview),
                                        GDALGetRasterBandYSize(hOverview)));
                    }
                    else
                    {
                        printf( "(null)" );
                    }
                }
                printf( "\n" );
            }
        }

        if( GDALHasArbitraryOverviews( hBand ) && !bJson )
        {
            printf( "  Overviews: arbitrary\n" );
        }
        
        nMaskFlags = GDALGetMaskFlags( hBand );
        if( (nMaskFlags & (GMF_NODATA|GMF_ALL_VALID)) == 0 )
        {
            GDALRasterBandH hMaskBand = GDALGetMaskBand(hBand) ;
            json_object *poMask = NULL, *poFlags = NULL, *poMaskOverviews = NULL;

            if(bJson)
            {
                poMask = json_object_new_object();
                poFlags = json_object_new_array(); 
            }
            else
                printf( "  Mask Flags: " );
            if( nMaskFlags & GMF_PER_DATASET )
            {
                if(bJson)
                {
                    json_object *poFlag = json_object_new_string( "PER_DATASET" );
                    json_object_array_add( poFlags, poFlag );
                }
                else
                    printf( "PER_DATASET " );
            }
            if( nMaskFlags & GMF_ALPHA )
            {
                if(bJson)
                {
                    json_object *poFlag = json_object_new_string( "ALPHA" );
                    json_object_array_add( poFlags, poFlag );
                }
                else
                    printf( "ALPHA " );
            }
            if( nMaskFlags & GMF_NODATA )
            {
                if(bJson)
                {
                    json_object *poFlag = json_object_new_string( "NODATA" );
                    json_object_array_add( poFlags, poFlag );
                }
                else
                    printf( "NODATA " );
            }
            if( nMaskFlags & GMF_ALL_VALID )
            {
                if(bJson)
                {
                    json_object *poFlag = json_object_new_string( "ALL_VALID" );
                    json_object_array_add( poFlags, poFlag );
                }
                else
                    printf( "ALL_VALID " );
            }
            if(bJson)
                json_object_object_add( poMask, "flags", poFlags );
            else
                printf( "\n" );

            if(bJson)
                poMaskOverviews = json_object_new_array();

            if( hMaskBand != NULL &&
                GDALGetOverviewCount(hMaskBand) > 0 )
            {
                int     iOverview;
                
                if(!bJson)
                    printf( "  Overviews of mask band: " );
                
                for( iOverview = 0; 
                     iOverview < GDALGetOverviewCount(hMaskBand);
                     iOverview++ )
                {
                    GDALRasterBandH hOverview;
                    json_object *poMaskOverviewSizeX, *poMaskOverviewSizeY,
                                *poMaskOverview = NULL, *poMaskOverviewSize = NULL;

                    if(bJson)
                    {
                        poMaskOverview = json_object_new_object();
                        poMaskOverviewSize = json_object_new_array();
                    }
                    else
                    {
                        if( iOverview != 0 )
                            printf( ", " );
                    }

                    hOverview = GDALGetOverview( hMaskBand, iOverview );
                    if(bJson)
                    {
                        poMaskOverviewSizeX = json_object_new_int(GDALGetRasterBandXSize(hOverview));
                        poMaskOverviewSizeY = json_object_new_int(GDALGetRasterBandYSize(hOverview));

                        json_object_array_add(poMaskOverviewSize, poMaskOverviewSizeX);
                        json_object_array_add(poMaskOverviewSize, poMaskOverviewSizeY);
                        json_object_object_add(poMaskOverview, "size", poMaskOverviewSize);
                        json_object_array_add(poMaskOverviews, poMaskOverview);
                    }
                    else
                        printf( "%dx%d", 
                            GDALGetRasterBandXSize( hOverview ),
                            GDALGetRasterBandYSize( hOverview ) );
                }
                if(!bJson)
                    printf( "\n" );
            }
            if(bJson)
            {
                json_object_object_add(poMask, "overviews", poMaskOverviews);
                json_object_object_add(poBand, "mask", poMask);
            }    
        }

        if( strlen(GDALGetRasterUnitType(hBand)) > 0 )
        {
            if(bJson)
            {
                json_object *poUnit = json_object_new_string(GDALGetRasterUnitType(hBand));
                json_object_object_add(poBand, "unit", poUnit);
            }
            else
                printf( "  Unit Type: %s\n", GDALGetRasterUnitType(hBand) );
        }

        if( GDALGetRasterCategoryNames(hBand) != NULL )
        {
            char **papszCategories = GDALGetRasterCategoryNames(hBand);
            int i;
            json_object *poCategories = NULL;

            if(bJson)
                poCategories = json_object_new_array();           
            else
                printf( "  Categories:\n" );
                
            for( i = 0; papszCategories[i] != NULL; i++ )
            {
                if(bJson)
                {
                    json_object *poCategoryName = json_object_new_string(papszCategories[i]);
                    json_object_array_add(poCategories, poCategoryName);
                }
                else
                    printf( "    %3d: %s\n", i, papszCategories[i] );
            }
            if(bJson)
                json_object_object_add(poBand, "categories", poCategories);
        }

        if( GDALGetRasterScale( hBand, &bSuccess ) != 1.0 
            || GDALGetRasterOffset( hBand, &bSuccess ) != 0.0 )
        {
            if(bJson)
            {
                json_object *poOffset = json_object_new_double_with_precision(GDALGetRasterOffset(hBand, &bSuccess), 15);
                json_object *poScale = json_object_new_double_with_precision(GDALGetRasterScale(hBand, &bSuccess), 15);
                json_object_object_add(poBand, "offset", poOffset);
                json_object_object_add(poBand, "scale", poScale);
            }
            else
                CPLprintf( "  Offset: %.15g,   Scale:%.15g\n",
                    GDALGetRasterOffset( hBand, &bSuccess ),
                    GDALGetRasterScale( hBand, &bSuccess ) );
        }
        
        GDALInfoReportMetadata( hBand, bListMDD, bShowMetadata, papszExtraMDDomains, TRUE, bJson, poBandMetadata );
        if(bJson && bShowMetadata)  
            json_object_object_add( poBand, "metadata", poBandMetadata );

        if( GDALGetRasterColorInterpretation(hBand) == GCI_PaletteIndex 
            && (hTable = GDALGetRasterColorTable( hBand )) != NULL )
        {
            int         i;
            json_object *poColorTable = NULL;
            
            if(bJson)
            {
                json_object *poPalette = json_object_new_string(GDALGetPaletteInterpretationName(
                    GDALGetPaletteInterpretation(hTable)));
                json_object *poCount = json_object_new_int(GDALGetColorEntryCount(hTable));
                poColorTable = json_object_new_object();

                json_object_object_add(poColorTable, "palette", poPalette);
                json_object_object_add(poColorTable, "count", poCount);
            }
            else
                printf( "  Color Table (%s with %d entries)\n",
                    GDALGetPaletteInterpretationName(
                        GDALGetPaletteInterpretation( hTable )), 
                    GDALGetColorEntryCount( hTable ) );

            if (bShowColorTable)
            {
                json_object *poEntries = NULL;
                
                if(bJson)
                    poEntries = json_object_new_array();

                for( i = 0; i < GDALGetColorEntryCount( hTable ); i++ )
                {
                    GDALColorEntry  sEntry;
    
                    GDALGetColorEntryAsRGB( hTable, i, &sEntry );

                    if(bJson)
                    {
                        json_object *poEntry = json_object_new_array();
                        json_object *poC1 = json_object_new_int(sEntry.c1);
                        json_object *poC2 = json_object_new_int(sEntry.c2);
                        json_object *poC3 = json_object_new_int(sEntry.c3);
                        json_object *poC4 = json_object_new_int(sEntry.c4);
                        
                        json_object_array_add(poEntry, poC1);
                        json_object_array_add(poEntry, poC2);
                        json_object_array_add(poEntry, poC3);
                        json_object_array_add(poEntry, poC4);
                        json_object_array_add(poEntries, poEntry);
                    }
                    else
                        printf( "  %3d: %d,%d,%d,%d\n",
                            i,
                            sEntry.c1,
                            sEntry.c2,
                            sEntry.c3,
                            sEntry.c4 );
                }
                if(bJson)
                {
                    json_object_object_add(poColorTable, "entries", poEntries);
                    json_object_object_add(poBand, "colorTable", poColorTable);
                }
            }   
        }

        if( bShowRAT && GDALGetDefaultRAT( hBand ) != NULL )
        {
            GDALRasterAttributeTableH hRAT = GDALGetDefaultRAT( hBand );
            
            if(bJson)
            {
                json_object *poRAT = (json_object*) GDALRATSerializeJSON( hRAT );
                json_object_object_add( poJsonObject, "rat", poRAT );
            }
            else
            {
                GDALRATDumpReadable( hRAT, NULL );
            }
        }
        if(bJson)
            json_object_array_add(poBands, poBand);
    }

    if(bJson)
    {
        json_object_object_add(poJsonObject, "bands", poBands);
        printf("%s\n", json_object_to_json_string_ext(poJsonObject, JSON_C_TO_STRING_PRETTY));
        json_object_put(poJsonObject);
    }
    
    GDALClose( hDataset );
    
    CSLDestroy( papszExtraMDDomains );
    CSLDestroy( papszOpenOptions );
    CSLDestroy( argv );
    
    GDALDumpOpenDatasets( stderr );

    GDALDestroyDriverManager();

    CPLDumpSharedList( NULL );
    CPLCleanupTLS();

    exit( 0 );
}

/************************************************************************/
/*                        GDALInfoReportCorner()                        */
/************************************************************************/

static int 
GDALInfoReportCorner( GDALDatasetH hDataset, 
                      OGRCoordinateTransformationH hTransform, OGRCoordinateTransformationH hTransformWGS84,
                      const char * corner_name,
                      double x, double y,
                      int bJson, json_object *poCornerCoordinates, json_object *poWGS84ExtentCoordinates )

{
    double  dfGeoX, dfGeoY;
    double  adfGeoTransform[6];
    json_object *poCorner, *poX, *poY;

    if(!bJson)
        printf( "%-11s ", corner_name );

/* -------------------------------------------------------------------- */
/*      Transform the point into georeferenced coordinates.             */
/* -------------------------------------------------------------------- */
    if( GDALGetGeoTransform( hDataset, adfGeoTransform ) == CE_None )
    {
        dfGeoX = adfGeoTransform[0] + adfGeoTransform[1] * x
            + adfGeoTransform[2] * y;
        dfGeoY = adfGeoTransform[3] + adfGeoTransform[4] * x
            + adfGeoTransform[5] * y;
    }

    else
    {
        if( bJson )
        {
            poCorner = json_object_new_array();
            poX = json_object_new_double_with_precision( x, 1 );
            poY = json_object_new_double_with_precision( y, 1 );
            json_object_array_add( poCorner, poX );
            json_object_array_add( poCorner, poY );
            json_object_object_add( poCornerCoordinates, corner_name, poCorner );
        }
        else
            CPLprintf( "(%7.1f,%7.1f)\n", x, y );
        return FALSE;
    }

/* -------------------------------------------------------------------- */
/*      Report the georeferenced coordinates.                           */
/* -------------------------------------------------------------------- */
    if( ABS(dfGeoX) < 181 && ABS(dfGeoY) < 91 )
    {
        if(bJson)
        {
            poCorner = json_object_new_array();
            poX = json_object_new_double_with_precision( dfGeoX, 7 );
            poY = json_object_new_double_with_precision( dfGeoY, 7 );
            json_object_array_add( poCorner, poX );
            json_object_array_add( poCorner, poY );
            json_object_object_add( poCornerCoordinates, corner_name, poCorner );
        }
        else
            CPLprintf( "(%12.7f,%12.7f) ", dfGeoX, dfGeoY );
    }
    else
    {
        if(bJson)
        {
            poCorner = json_object_new_array();
            poX = json_object_new_double_with_precision( dfGeoX, 3 );
            poY = json_object_new_double_with_precision( dfGeoY, 3 );
            json_object_array_add( poCorner, poX );
            json_object_array_add( poCorner, poY );
            json_object_object_add( poCornerCoordinates, corner_name, poCorner );
        }
        else
            CPLprintf( "(%12.3f,%12.3f) ", dfGeoX, dfGeoY );
    }

/* -------------------------------------------------------------------- */
/*      Transform to latlong and report.                                */
/* -------------------------------------------------------------------- */
    if(bJson)
    {
        if( hTransformWGS84 != NULL && !EQUAL( corner_name, "center" ) 
        && OCTTransform(hTransformWGS84,1,&dfGeoX,&dfGeoY,NULL) )
        {
            poCorner = json_object_new_array();
            poX = json_object_new_double_with_precision( dfGeoX, 7 );
            poY = json_object_new_double_with_precision( dfGeoY, 7 );
            json_object_array_add( poCorner, poX );
            json_object_array_add( poCorner, poY );
            json_object_array_add( poWGS84ExtentCoordinates , poCorner );
        }
    }
    else
    {
        if( hTransform != NULL 
        && OCTTransform(hTransform,1,&dfGeoX,&dfGeoY,NULL) )
        {
            printf( "(%s,", GDALDecToDMS( dfGeoX, "Long", 2 ) );
            printf( "%s)", GDALDecToDMS( dfGeoY, "Lat", 2 ) );
        }
        printf( "\n" );
    }
    
    return TRUE;
}


/************************************************************************/
/*                       GDALInfoPrintMetadata()                        */
/************************************************************************/
static void GDALInfoPrintMetadata( GDALMajorObjectH hObject,
                                   const char *pszDomain,
                                   const char *pszDisplayedname,
                                   const char *pszIndent,
                                   int bJson,
                                   json_object *poMetadata )
{
    int i;
    char **papszMetadata;
    int bIsxml = FALSE;
    char *pszKey = NULL;
    const char *pszValue;
    json_object *poDomain = NULL, *poValue = NULL;

    if (pszDomain != NULL && EQUALN(pszDomain, "xml:", 4))
        bIsxml = TRUE;

    papszMetadata = GDALGetMetadata( hObject, pszDomain );
    if( CSLCount(papszMetadata) > 0 )
    {
        if(bJson && !bIsxml)
            poDomain = json_object_new_object();
        
        if(!bJson)
            printf( "%s%s:\n", pszIndent, pszDisplayedname );
        
        for( i = 0; papszMetadata[i] != NULL; i++ )
        {
            if(bJson)
            {
                if(bIsxml)
                {
                    poValue = json_object_new_string( papszMetadata[i] );
                    break;
                }
                else
                {
                    pszKey = NULL;
                    pszValue = CPLParseNameValue( papszMetadata[i], &pszKey );
                    if( pszKey )
                    {
                        poValue = json_object_new_string( pszValue );
                        json_object_object_add( poDomain, pszKey, poValue );
                        CPLFree( pszKey );
                    }
                }
            }
            else
            {
                if (bIsxml)
                    printf( "%s%s\n", pszIndent, papszMetadata[i] );
                else
                    printf( "%s  %s\n", pszIndent, papszMetadata[i] );
            
            }
        }
        if(bJson)
        {
            if(bIsxml)
                json_object_object_add( poMetadata, pszDomain, poValue );
            else
            {
                if(pszDomain == NULL)
                    json_object_object_add( poMetadata, "", poDomain );
                else
                    json_object_object_add( poMetadata, pszDomain, poDomain );
            }
        }
    }
    
}

/************************************************************************/
/*                       GDALInfoReportMetadata()                       */
/************************************************************************/
static void GDALInfoReportMetadata( GDALMajorObjectH hObject,
                                    int bListMDD,
                                    int bShowMetadata,
                                    char **papszExtraMDDomains,
                                    int bIsBand,
                                    int bJson,
                                    json_object *poMetadata )
{
    const char* pszIndent = "";
    if( bIsBand )
        pszIndent = "  ";

    /* -------------------------------------------------------------------- */
    /*      Report list of Metadata domains                                 */
    /* -------------------------------------------------------------------- */
    if( bListMDD )
    {
        char** papszMDDList = GDALGetMetadataDomainList( hObject );
        char** papszIter = papszMDDList;
        json_object *poMDD = NULL;
        json_object *poListMDD = NULL;
        if( bJson )
            poListMDD = json_object_new_array();

        if( papszMDDList != NULL )
        {
            if(!bJson)
                printf( "%sMetadata domains:\n", pszIndent );
        }
        
        while( papszIter != NULL && *papszIter != NULL )
        {
            if( EQUAL(*papszIter, "") )
            {
                if(bJson)
                    poMDD = json_object_new_string( *papszIter );
                else
                    printf( "%s  (default)\n", pszIndent);
            }
            else
            {
                if(bJson)
                    poMDD = json_object_new_string( *papszIter );
                else
                    printf( "%s  %s\n", pszIndent, *papszIter );
            }
            if(bJson)
                json_object_array_add( poListMDD, poMDD );
            papszIter ++;
        }
        if(bJson)
            json_object_object_add( poMetadata, "metadataDomains", poListMDD );
        CSLDestroy(papszMDDList);
    }

    if (!bShowMetadata)
        return;

    /* -------------------------------------------------------------------- */
    /*      Report default Metadata domain.                                 */
    /* -------------------------------------------------------------------- */
    GDALInfoPrintMetadata( hObject, NULL, "Metadata", pszIndent, bJson, poMetadata );

    /* -------------------------------------------------------------------- */
    /*      Report extra Metadata domains                                   */
    /* -------------------------------------------------------------------- */
    if (papszExtraMDDomains != NULL) {
        char **papszExtraMDDomainsExpanded = NULL;
        int iMDD;

        if( EQUAL(papszExtraMDDomains[0], "all") &&
            papszExtraMDDomains[1] == NULL )
        {
            char** papszMDDList = GDALGetMetadataDomainList( hObject );
            char** papszIter = papszMDDList;

            while( papszIter != NULL && *papszIter != NULL )
            {
                if( !EQUAL(*papszIter, "") &&
                    !EQUAL(*papszIter, "IMAGE_STRUCTURE") &&
                    !EQUAL(*papszIter, "SUBDATASETS") &&
                    !EQUAL(*papszIter, "GEOLOCATION") &&
                    !EQUAL(*papszIter, "RPC") )
                {
                    papszExtraMDDomainsExpanded = CSLAddString(papszExtraMDDomainsExpanded, *papszIter);
                }
                papszIter ++;
            }
            CSLDestroy(papszMDDList);
        }
        else
        {
            papszExtraMDDomainsExpanded = CSLDuplicate(papszExtraMDDomains);
        }

        for( iMDD = 0; iMDD < CSLCount(papszExtraMDDomainsExpanded); iMDD++ )
        {
            char pszDisplayedname[256];
            snprintf(pszDisplayedname, 256, "Metadata (%s)", papszExtraMDDomainsExpanded[iMDD]);
            if(bJson)
                GDALInfoPrintMetadata( hObject, papszExtraMDDomainsExpanded[iMDD], papszExtraMDDomainsExpanded[iMDD], pszIndent, bJson, poMetadata );
            else
                GDALInfoPrintMetadata( hObject, papszExtraMDDomainsExpanded[iMDD], pszDisplayedname, pszIndent, bJson, poMetadata );
        }

        CSLDestroy(papszExtraMDDomainsExpanded);
    }

    /* -------------------------------------------------------------------- */
    /*      Report various named metadata domains.                          */
    /* -------------------------------------------------------------------- */
    GDALInfoPrintMetadata( hObject, "IMAGE_STRUCTURE", "Image Structure Metadata", pszIndent, bJson, poMetadata );

    if (!bIsBand)
    {
        GDALInfoPrintMetadata( hObject, "SUBDATASETS", "Subdatasets", pszIndent, bJson, poMetadata );
        GDALInfoPrintMetadata( hObject, "GEOLOCATION", "Geolocation", pszIndent, bJson, poMetadata );
        GDALInfoPrintMetadata( hObject, "RPC", "RPC Metadata", pszIndent, bJson, poMetadata );
    }

}
