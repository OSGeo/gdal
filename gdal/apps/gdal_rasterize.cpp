/******************************************************************************
 * $Id$
 *
 * Project:  GDAL Utilities
 * Purpose:  Rasterize OGR shapes into a GDAL raster.
 * Author:   Frank Warmerdam <warmerdam@pobox.com>
 *
 ******************************************************************************
 * Copyright (c) 2005, Frank Warmerdam <warmerdam@pobox.com>
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
#include "cpl_conv.h"
#include "ogr_api.h"
#include "ogr_srs_api.h"
#include "cpl_string.h"
#include <vector>

CPL_CVSID("$Id$");

/************************************************************************/
/*                               Usage()                                */
/************************************************************************/

static void Usage()

{
    printf( 
        "Usage: gdal_rasterize [-b band] [-i] [-at]\n"
        "       [-burn value] | [-a attribute_name] [-3d]\n"
//      "       [-of format_driver] [-co key=value]\n"       
//      "       [-te xmin ymin xmax ymax] [-tr xres yres] [-ts width height]\n"
        "       [-l layername]* [-where expression] [-sql select_statement]\n"
        "       <src_datasource> <dst_filename>\n" );
    exit( 1 );
}

/************************************************************************/
/*                          InvertGeometries()                          */
/************************************************************************/

static void InvertGeometries( GDALDatasetH hDstDS, 
                              std::vector<OGRGeometryH> &ahGeometries )

{
    OGRGeometryH hCollection = 
        OGR_G_CreateGeometry( wkbGeometryCollection );

/* -------------------------------------------------------------------- */
/*      Create a ring that is a bit outside the raster dataset.         */
/* -------------------------------------------------------------------- */
    OGRGeometryH hUniversePoly, hUniverseRing;
    double adfGeoTransform[6];
    int brx = GDALGetRasterXSize( hDstDS ) + 2;
    int bry = GDALGetRasterYSize( hDstDS ) + 2;

    GDALGetGeoTransform( hDstDS, adfGeoTransform );

    hUniverseRing = OGR_G_CreateGeometry( wkbLinearRing );
    
    OGR_G_AddPoint_2D( 
        hUniverseRing, 
        adfGeoTransform[0] + -2*adfGeoTransform[1] + -2*adfGeoTransform[2],
        adfGeoTransform[3] + -2*adfGeoTransform[4] + -2*adfGeoTransform[5] );
                       
    OGR_G_AddPoint_2D( 
        hUniverseRing, 
        adfGeoTransform[0] + brx*adfGeoTransform[1] + -2*adfGeoTransform[2],
        adfGeoTransform[3] + brx*adfGeoTransform[4] + -2*adfGeoTransform[5] );
                       
    OGR_G_AddPoint_2D( 
        hUniverseRing, 
        adfGeoTransform[0] + brx*adfGeoTransform[1] + bry*adfGeoTransform[2],
        adfGeoTransform[3] + brx*adfGeoTransform[4] + bry*adfGeoTransform[5] );
                       
    OGR_G_AddPoint_2D( 
        hUniverseRing, 
        adfGeoTransform[0] + -2*adfGeoTransform[1] + bry*adfGeoTransform[2],
        adfGeoTransform[3] + -2*adfGeoTransform[4] + bry*adfGeoTransform[5] );
                       
    OGR_G_AddPoint_2D( 
        hUniverseRing, 
        adfGeoTransform[0] + -2*adfGeoTransform[1] + -2*adfGeoTransform[2],
        adfGeoTransform[3] + -2*adfGeoTransform[4] + -2*adfGeoTransform[5] );
                       
    hUniversePoly = OGR_G_CreateGeometry( wkbPolygon );
    OGR_G_AddGeometryDirectly( hUniversePoly, hUniverseRing );

    OGR_G_AddGeometryDirectly( hCollection, hUniversePoly );
    
/* -------------------------------------------------------------------- */
/*      Add the rest of the geometries into our collection.             */
/* -------------------------------------------------------------------- */
    unsigned int iGeom;

    for( iGeom = 0; iGeom < ahGeometries.size(); iGeom++ )
        OGR_G_AddGeometryDirectly( hCollection, ahGeometries[iGeom] );

    ahGeometries.resize(1);
    ahGeometries[0] = hCollection;
}

/************************************************************************/
/*                            ProcessLayer()                            */
/*                                                                      */
/*      Process all the features in a layer selection, collecting       */
/*      geometries and burn values.                                     */
/************************************************************************/

static void ProcessLayer( 
    OGRLayerH hSrcLayer, 
    GDALDatasetH hDstDS, std::vector<int> anBandList,
    std::vector<double> &adfBurnValues, int b3D, int bInverse,
    const char *pszBurnAttribute, char **papszRasterizeOptions )

{
/* -------------------------------------------------------------------- */
/*      Checkout that SRS are the same.                                 */
/* -------------------------------------------------------------------- */
    OGRSpatialReferenceH  hDstSRS = NULL;
    if( GDALGetProjectionRef( hDstDS ) != NULL )
    {
        char *pszProjection;

        pszProjection = (char *) GDALGetProjectionRef( hDstDS );

        hDstSRS = OSRNewSpatialReference(NULL);
        if( OSRImportFromWkt( hDstSRS, &pszProjection ) != CE_None )
        {
            OSRDestroySpatialReference(hDstSRS);
            hDstSRS = NULL;
        }
    }

    OGRSpatialReferenceH hSrcSRS = OGR_L_GetSpatialRef(hSrcLayer);
    if( hDstSRS != NULL && hSrcSRS != NULL )
    {
        if( OSRIsSame(hSrcSRS, hDstSRS) == FALSE )
        {
            fprintf(stderr,
                    "Warning : the output raster dataset and the input vector layer do not have the same SRS.\n"
                    "Results might be incorrect (no on-the-fly reprojection of input data).\n");
        }
    }
    else if( hDstSRS != NULL && hSrcSRS == NULL )
    {
        fprintf(stderr,
                "Warning : the output raster dataset has a SRS, but the input vector layer SRS is unknown.\n"
                "Ensure input vector has the same SRS, otherwise results might be incorrect.\n");
    }
    else if( hDstSRS == NULL && hSrcLayer != NULL )
    {
        fprintf(stderr,
                "Warning : the input vector layer has a SRS, but the output raster dataset SRS is unknown.\n"
                "Ensure output raster dataset has the same SRS, otherwise results might be incorrect.\n");
    }

    if( hDstSRS != NULL )
    {
        OSRDestroySpatialReference(hDstSRS);
    }

/* -------------------------------------------------------------------- */
/*      Get field index, and check.                                     */
/* -------------------------------------------------------------------- */
    int iBurnField = -1;

    if( pszBurnAttribute )
    {
        iBurnField = OGR_FD_GetFieldIndex( OGR_L_GetLayerDefn( hSrcLayer ),
                                           pszBurnAttribute );
        if( iBurnField == -1 )
        {
            printf( "Failed to find field %s on layer %s, skipping.\n",
                    pszBurnAttribute, 
                    OGR_FD_GetName( OGR_L_GetLayerDefn( hSrcLayer ) ) );
            return;
        }
    }

/* -------------------------------------------------------------------- */
/*      Collect the geometries from this layer, and build list of       */
/*      burn values.                                                    */
/* -------------------------------------------------------------------- */
    OGRFeatureH hFeat;
    std::vector<OGRGeometryH> ahGeometries;
    std::vector<double> adfFullBurnValues;

    OGR_L_ResetReading( hSrcLayer );
    
    while( (hFeat = OGR_L_GetNextFeature( hSrcLayer )) != NULL )
    {
        OGRGeometryH hGeom;

        if( OGR_F_GetGeometryRef( hFeat ) == NULL )
        {
            OGR_F_Destroy( hFeat );
            continue;
        }

        hGeom = OGR_G_Clone( OGR_F_GetGeometryRef( hFeat ) );
        ahGeometries.push_back( hGeom );

        for( unsigned int iBand = 0; iBand < anBandList.size(); iBand++ )
        {
            if( adfBurnValues.size() > 0 )
                adfFullBurnValues.push_back( 
                    adfBurnValues[MIN(iBand,adfBurnValues.size()-1)] );
            else if( pszBurnAttribute )
            {
                adfFullBurnValues.push_back( OGR_F_GetFieldAsDouble( hFeat, iBurnField ) );
            }
            /* I have made the 3D option exclusive to other options since it
               can be used to modify the value from "-burn value" or
               "-a attribute_name" */
            if( b3D )
            {
                // TODO: get geometry "z" value
                /* Points and Lines will have their "z" values collected at the
                   point and line levels respectively. However filled polygons
                   (GDALdllImageFilledPolygon) can use some help by getting
                   their "z" values here. */
                adfFullBurnValues.push_back( 0.0 );
            }
        }
        
        OGR_F_Destroy( hFeat );
    }

/* -------------------------------------------------------------------- */
/*      If we are in inverse mode, we add one extra ring around the     */
/*      whole dataset to invert the concept of insideness and then      */
/*      merge everything into one geomtry collection.                   */
/* -------------------------------------------------------------------- */
    if( bInverse )
    {
        InvertGeometries( hDstDS, ahGeometries );
    }

/* -------------------------------------------------------------------- */
/*      Perform the burn.                                               */
/* -------------------------------------------------------------------- */
    GDALRasterizeGeometries( hDstDS, anBandList.size(), &(anBandList[0]), 
                             ahGeometries.size(), &(ahGeometries[0]), 
                             NULL, NULL, &(adfFullBurnValues[0]), 
                             papszRasterizeOptions,
                             GDALTermProgress, NULL );

/* -------------------------------------------------------------------- */
/*      Cleanup geometries.                                             */
/* -------------------------------------------------------------------- */
    int iGeom;

    for( iGeom = ahGeometries.size()-1; iGeom >= 0; iGeom-- )
        OGR_G_DestroyGeometry( ahGeometries[iGeom] );
}

/************************************************************************/
/*                                main()                                */
/************************************************************************/

int main( int argc, char ** argv )

{
    int i, b3D = FALSE;
    int bInverse = FALSE;
    const char *pszSrcFilename = NULL;
    const char *pszDstFilename = NULL;
    char **papszLayers = NULL;
    const char *pszSQL = NULL;
    const char *pszBurnAttribute = NULL;
    const char *pszWHERE = NULL;
    std::vector<int> anBandList;
    std::vector<double> adfBurnValues;
    char **papszRasterizeOptions = NULL;

    /* Check that we are running against at least GDAL 1.4 */
    /* Note to developers : if we use newer API, please change the requirement */
    if (atoi(GDALVersionInfo("VERSION_NUM")) < 1400)
    {
        fprintf(stderr, "At least, GDAL >= 1.4.0 is required for this version of %s, "
                "which was compiled against GDAL %s\n", argv[0], GDAL_RELEASE_NAME);
        exit(1);
    }

    GDALAllRegister();
    OGRRegisterAll();

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
        else if( EQUAL(argv[i],"-a") && i < argc-1 )
        {
            pszBurnAttribute = argv[++i];
        }
        else if( EQUAL(argv[i],"-b") && i < argc-1 )
        {
            anBandList.push_back( atoi(argv[++i]) );
        }
        else if( EQUAL(argv[i],"-3d")  )
        {
            b3D = TRUE;
            papszRasterizeOptions = 
                CSLSetNameValue( papszRasterizeOptions, "BURN_VALUE_FROM", "Z");
        }
        else if( EQUAL(argv[i],"-i")  )
        {
            bInverse = TRUE;
        }
        else if( EQUAL(argv[i],"-at")  )
        {
            papszRasterizeOptions = 
                CSLSetNameValue( papszRasterizeOptions, "ALL_TOUCHED", "TRUE" );
        }
        else if( EQUAL(argv[i],"-burn") && i < argc-1 )
        {
            adfBurnValues.push_back( atof(argv[++i]) );
        }
        else if( EQUAL(argv[i],"-where") && i < argc-1 )
        {
            pszWHERE = argv[++i];
        }
        else if( EQUAL(argv[i],"-l") && i < argc-1 )
        {
            papszLayers = CSLAddString( papszLayers, argv[++i] );
        }
        else if( EQUAL(argv[i],"-sql") && i < argc-1 )
        {
            pszSQL = argv[++i];
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
    
    if( pszSQL == NULL && papszLayers == NULL )
    {
        fprintf( stderr, "At least one of -l or -sql required.\n\n" );
        Usage();
    }

    if( adfBurnValues.size() == 0 && pszBurnAttribute == NULL && !b3D )
    {
        fprintf( stderr, "At least one of -3d, -burn or -a required.\n\n" );
        Usage();
    }

    if( anBandList.size() == 0 )
        anBandList.push_back( 1 );

/* -------------------------------------------------------------------- */
/*      Open source vector dataset.                                     */
/* -------------------------------------------------------------------- */
    OGRDataSourceH hSrcDS;

    hSrcDS = OGROpen( pszSrcFilename, FALSE, NULL );
    if( hSrcDS == NULL )
    {
        fprintf( stderr, "Failed to open feature source: %s\n", 
                 pszSrcFilename);
        exit( 1 );
    }

/* -------------------------------------------------------------------- */
/*      Open target raster file.  Eventually we will add optional       */
/*      creation.                                                       */
/* -------------------------------------------------------------------- */
    GDALDatasetH hDstDS;

    hDstDS = GDALOpen( pszDstFilename, GA_Update );
    if( hDstDS == NULL )
        exit( 2 );

/* -------------------------------------------------------------------- */
/*      Process SQL request.                                            */
/* -------------------------------------------------------------------- */
    if( pszSQL != NULL )
    {
        OGRLayerH hLayer;

        hLayer = OGR_DS_ExecuteSQL( hSrcDS, pszSQL, NULL, NULL ); 
        if( hLayer != NULL )
        {
            ProcessLayer( hLayer, hDstDS, anBandList, 
                          adfBurnValues, b3D, bInverse, pszBurnAttribute,
                          papszRasterizeOptions );
        }
    }

/* -------------------------------------------------------------------- */
/*      Process each layer.                                             */
/* -------------------------------------------------------------------- */
    int nLayerCount = CSLCount(papszLayers);
    for( i = 0; i < nLayerCount; i++ )
    {
        OGRLayerH hLayer = OGR_DS_GetLayerByName( hSrcDS, papszLayers[i] );
        if( hLayer == NULL )
        {
            fprintf( stderr, "Unable to find layer %s, skipping.\n", 
                      papszLayers[i] );
            continue;
        }

        if( pszWHERE )
        {
            if( OGR_L_SetAttributeFilter( hLayer, pszWHERE ) != OGRERR_NONE )
                break;
        }

        ProcessLayer( hLayer, hDstDS, anBandList, 
                      adfBurnValues, b3D, bInverse, pszBurnAttribute,
                      papszRasterizeOptions );
    }

/* -------------------------------------------------------------------- */
/*      Cleanup                                                         */
/* -------------------------------------------------------------------- */

    OGR_DS_Destroy( hSrcDS );
    GDALClose( hDstDS );

    CSLDestroy( argv );
    CSLDestroy( papszRasterizeOptions );
    CSLDestroy( papszLayers );
    
    GDALDestroyDriverManager();
    OGRCleanupAll();

    return 0;
}
