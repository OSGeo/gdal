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
#include "commonutils.h"
#include <vector>

CPL_CVSID("$Id$");

/************************************************************************/
/*                            ArgIsNumeric()                            */
/************************************************************************/

static int ArgIsNumeric( const char *pszArg )

{
    if( pszArg[0] == '-' )
        pszArg++;

    if( *pszArg == '\0' )
        return FALSE;

    while( *pszArg != '\0' )
    {
        if( (*pszArg < '0' || *pszArg > '9') && *pszArg != '.' )
            return FALSE;
        pszArg++;
    }
        
    return TRUE;
}

/************************************************************************/
/*                               Usage()                                */
/************************************************************************/

static void Usage()

{
    printf( 
        "Usage: gdal_rasterize [-b band]* [-i] [-at]\n"
        "       [-burn value]* | [-a attribute_name] [-3d]\n"
        "       [-l layername]* [-where expression] [-sql select_statement]\n"
        "       [-of format] [-a_srs srs_def] [-co \"NAME=VALUE\"]*\n"
        "       [-a_nodata value] [-init value]*\n"
        "       [-te xmin ymin xmax ymax] [-tr xres yres] [-tap] [-ts width height]\n"
        "       [-ot {Byte/Int16/UInt16/UInt32/Int32/Float32/Float64/\n"
        "             CInt16/CInt32/CFloat32/CFloat64}] [-q]\n"
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
    OGRLayerH hSrcLayer, int bSRSIsSet, 
    GDALDatasetH hDstDS, std::vector<int> anBandList,
    std::vector<double> &adfBurnValues, int b3D, int bInverse,
    const char *pszBurnAttribute, char **papszRasterizeOptions,
    GDALProgressFunc pfnProgress, void* pProgressData )

{
/* -------------------------------------------------------------------- */
/*      Checkout that SRS are the same.                                 */
/*      If -a_srs is specified, skip the test                           */
/* -------------------------------------------------------------------- */
    if (!bSRSIsSet)
    {
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
        else if( hDstSRS == NULL && hSrcSRS != NULL )
        {
            fprintf(stderr,
                    "Warning : the input vector layer has a SRS, but the output raster dataset SRS is unknown.\n"
                    "Ensure output raster dataset has the same SRS, otherwise results might be incorrect.\n");
        }
    
        if( hDstSRS != NULL )
        {
            OSRDestroySpatialReference(hDstSRS);
        }
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
/*      merge everything into one geometry collection.                  */
/* -------------------------------------------------------------------- */
    if( bInverse )
    {
        if( ahGeometries.size() == 0 )
        {
            for( unsigned int iBand = 0; iBand < anBandList.size(); iBand++ )
            {
                if( adfBurnValues.size() > 0 )
                    adfFullBurnValues.push_back(
                        adfBurnValues[MIN(iBand,adfBurnValues.size()-1)] );
                else /* FIXME? Not sure what to do exactly in the else case, but we must insert a value */
                    adfFullBurnValues.push_back( 0.0 );
            }
        }

        InvertGeometries( hDstDS, ahGeometries );
    }

/* -------------------------------------------------------------------- */
/*      Perform the burn.                                               */
/* -------------------------------------------------------------------- */
    GDALRasterizeGeometries( hDstDS, anBandList.size(), &(anBandList[0]), 
                             ahGeometries.size(), &(ahGeometries[0]), 
                             NULL, NULL, &(adfFullBurnValues[0]), 
                             papszRasterizeOptions,
                             pfnProgress, pProgressData );

/* -------------------------------------------------------------------- */
/*      Cleanup geometries.                                             */
/* -------------------------------------------------------------------- */
    int iGeom;

    for( iGeom = ahGeometries.size()-1; iGeom >= 0; iGeom-- )
        OGR_G_DestroyGeometry( ahGeometries[iGeom] );
}

/************************************************************************/
/*                  CreateOutputDataset()                               */
/************************************************************************/

static
GDALDatasetH CreateOutputDataset(std::vector<OGRLayerH> ahLayers,
                                 OGRSpatialReferenceH hSRS,
                                 int bGotBounds, OGREnvelope sEnvelop,
                                 GDALDriverH hDriver, const char* pszDstFilename,
                                 int nXSize, int nYSize, double dfXRes, double dfYRes,
                                 int bTargetAlignedPixels,
                                 int nBandCount, GDALDataType eOutputType,
                                 char** papszCreateOptions, std::vector<double> adfInitVals,
                                 int bNoDataSet, double dfNoData)
{
    int bFirstLayer = TRUE;
    char* pszWKT = NULL;
    GDALDatasetH hDstDS = NULL;
    unsigned int i;

    for( i = 0; i < ahLayers.size(); i++ )
    {
        OGRLayerH hLayer = ahLayers[i];

        if (!bGotBounds)
        {
            OGREnvelope sLayerEnvelop;

            if (OGR_L_GetExtent(hLayer, &sLayerEnvelop, TRUE) != OGRERR_NONE)
            {
                fprintf(stderr, "Cannot get layer extent\n");
                exit(2);
            }

            /* When rasterizing point layers and that the bounds have */
            /* not been explicitely set, voluntary increase the extent by */
            /* a half-pixel size to avoid missing points on the border */
            if (wkbFlatten(OGR_L_GetGeomType(hLayer)) == wkbPoint &&
                !bTargetAlignedPixels && dfXRes != 0 && dfYRes != 0)
            {
                sLayerEnvelop.MinX -= dfXRes / 2;
                sLayerEnvelop.MaxX += dfXRes / 2;
                sLayerEnvelop.MinY -= dfYRes / 2;
                sLayerEnvelop.MaxY += dfYRes / 2;
            }

            if (bFirstLayer)
            {
                sEnvelop.MinX = sLayerEnvelop.MinX;
                sEnvelop.MinY = sLayerEnvelop.MinY;
                sEnvelop.MaxX = sLayerEnvelop.MaxX;
                sEnvelop.MaxY = sLayerEnvelop.MaxY;

                if (hSRS == NULL)
                    hSRS = OGR_L_GetSpatialRef(hLayer);

                bFirstLayer = FALSE;
            }
            else
            {
                sEnvelop.MinX = MIN(sEnvelop.MinX, sLayerEnvelop.MinX);
                sEnvelop.MinY = MIN(sEnvelop.MinY, sLayerEnvelop.MinY);
                sEnvelop.MaxX = MAX(sEnvelop.MaxX, sLayerEnvelop.MaxX);
                sEnvelop.MaxY = MAX(sEnvelop.MaxY, sLayerEnvelop.MaxY);
            }
        }
        else
        {
            if (bFirstLayer)
            {
                if (hSRS == NULL)
                    hSRS = OGR_L_GetSpatialRef(hLayer);

                bFirstLayer = FALSE;
            }
        }
    }

    if (dfXRes == 0 && dfYRes == 0)
    {
        dfXRes = (sEnvelop.MaxX - sEnvelop.MinX) / nXSize;
        dfYRes = (sEnvelop.MaxY - sEnvelop.MinY) / nYSize;
    }
    else if (bTargetAlignedPixels && dfXRes != 0 && dfYRes != 0)
    {
        sEnvelop.MinX = floor(sEnvelop.MinX / dfXRes) * dfXRes;
        sEnvelop.MaxX = ceil(sEnvelop.MaxX / dfXRes) * dfXRes;
        sEnvelop.MinY = floor(sEnvelop.MinY / dfYRes) * dfYRes;
        sEnvelop.MaxY = ceil(sEnvelop.MaxY / dfYRes) * dfYRes;
    }

    double adfProjection[6];
    adfProjection[0] = sEnvelop.MinX;
    adfProjection[1] = dfXRes;
    adfProjection[2] = 0;
    adfProjection[3] = sEnvelop.MaxY;
    adfProjection[4] = 0;
    adfProjection[5] = -dfYRes;

    if (nXSize == 0 && nYSize == 0)
    {
        nXSize = (int)(0.5 + (sEnvelop.MaxX - sEnvelop.MinX) / dfXRes);
        nYSize = (int)(0.5 + (sEnvelop.MaxY - sEnvelop.MinY) / dfYRes);
    }

    hDstDS = GDALCreate(hDriver, pszDstFilename, nXSize, nYSize,
                        nBandCount, eOutputType, papszCreateOptions);
    if (hDstDS == NULL)
    {
        fprintf(stderr, "Cannot create %s\n", pszDstFilename);
        exit(2);
    }

    GDALSetGeoTransform(hDstDS, adfProjection);

    if (hSRS)
        OSRExportToWkt(hSRS, &pszWKT);
    if (pszWKT)
        GDALSetProjection(hDstDS, pszWKT);
    CPLFree(pszWKT);

    int iBand;
    /*if( nBandCount == 3 || nBandCount == 4 )
    {
        for(iBand = 0; iBand < nBandCount; iBand++)
        {
            GDALRasterBandH hBand = GDALGetRasterBand(hDstDS, iBand + 1);
            GDALSetRasterColorInterpretation(hBand, (GDALColorInterp)(GCI_RedBand + iBand));
        }
    }*/

    if (bNoDataSet)
    {
        for(iBand = 0; iBand < nBandCount; iBand++)
        {
            GDALRasterBandH hBand = GDALGetRasterBand(hDstDS, iBand + 1);
            GDALSetRasterNoDataValue(hBand, dfNoData);
        }
    }

    if (adfInitVals.size() != 0)
    {
        for(iBand = 0; iBand < MIN(nBandCount,(int)adfInitVals.size()); iBand++)
        {
            GDALRasterBandH hBand = GDALGetRasterBand(hDstDS, iBand + 1);
            GDALFillRaster(hBand, adfInitVals[iBand], 0);
        }
    }

    return hDstDS;
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
    double dfXRes = 0, dfYRes = 0;
    int bCreateOutput = FALSE;
    const char* pszFormat = "GTiff";
    int bFormatExplicitelySet = FALSE;
    char **papszCreateOptions = NULL;
    GDALDriverH hDriver = NULL;
    GDALDataType eOutputType = GDT_Float64;
    std::vector<double> adfInitVals;
    int bNoDataSet = FALSE;
    double dfNoData = 0;
    OGREnvelope sEnvelop;
    int bGotBounds = FALSE;
    int nXSize = 0, nYSize = 0;
    int bQuiet = FALSE;
    GDALProgressFunc pfnProgress = GDALTermProgress;
    OGRSpatialReferenceH hSRS = NULL;
    int bTargetAlignedPixels = FALSE;
    

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
        else if( EQUAL(argv[i],"-q") || EQUAL(argv[i],"-quiet") )
        {
            bQuiet = TRUE;
            pfnProgress = GDALDummyProgress;
        }
        else if( EQUAL(argv[i],"-a") && i < argc-1 )
        {
            pszBurnAttribute = argv[++i];
        }
        else if( EQUAL(argv[i],"-b") && i < argc-1 )
        {
            if (strchr(argv[i+1], ' '))
            {
                char** papszTokens = CSLTokenizeString( argv[i+1] );
                char** papszIter = papszTokens;
                while(papszIter && *papszIter)
                {
                    anBandList.push_back(atoi(*papszIter));
                    papszIter ++;
                }
                CSLDestroy(papszTokens);
                i += 1;
            }
            else
            {
                while(i < argc-1 && ArgIsNumeric(argv[i+1]))
                {
                    anBandList.push_back(atoi(argv[i+1]));
                    i += 1;
                }
            }
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
            if (strchr(argv[i+1], ' '))
            {
                char** papszTokens = CSLTokenizeString( argv[i+1] );
                char** papszIter = papszTokens;
                while(papszIter && *papszIter)
                {
                    adfBurnValues.push_back(atof(*papszIter));
                    papszIter ++;
                }
                CSLDestroy(papszTokens);
                i += 1;
            }
            else
            {
                while(i < argc-1 && ArgIsNumeric(argv[i+1]))
                {
                    adfBurnValues.push_back(atof(argv[i+1]));
                    i += 1;
                }
            }
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
        else if( EQUAL(argv[i],"-of") && i < argc-1 )
        {
            pszFormat = argv[++i];
            bFormatExplicitelySet = TRUE;
            bCreateOutput = TRUE;
        }
        else if( EQUAL(argv[i],"-init") && i < argc - 1 )
        {
            if (strchr(argv[i+1], ' '))
            {
                char** papszTokens = CSLTokenizeString( argv[i+1] );
                char** papszIter = papszTokens;
                while(papszIter && *papszIter)
                {
                    adfInitVals.push_back(atof(*papszIter));
                    papszIter ++;
                }
                CSLDestroy(papszTokens);
                i += 1;
            }
            else
            {
                while(i < argc-1 && ArgIsNumeric(argv[i+1]))
                {
                    adfInitVals.push_back(atof(argv[i+1]));
                    i += 1;
                }
            }
            bCreateOutput = TRUE;
        }
        else if( EQUAL(argv[i],"-a_nodata") && i < argc - 1 )
        {
            dfNoData = atof(argv[i+1]);
            bNoDataSet = TRUE;
            i += 1;
            bCreateOutput = TRUE;
        }
        else if( EQUAL(argv[i],"-a_srs") && i < argc-1 )
        {
            hSRS = OSRNewSpatialReference( NULL );

            if( OSRSetFromUserInput(hSRS, argv[i+1]) != OGRERR_NONE )
            {
                fprintf( stderr, "Failed to process SRS definition: %s\n", 
                         argv[i+1] );
                exit( 1 );
            }

            i++;
            bCreateOutput = TRUE;
        }   

        else if( EQUAL(argv[i],"-te") && i < argc - 4 )
        {
            sEnvelop.MinX = atof(argv[++i]);
            sEnvelop.MinY = atof(argv[++i]);
            sEnvelop.MaxX = atof(argv[++i]);
            sEnvelop.MaxY = atof(argv[++i]);
            bGotBounds = TRUE;
            bCreateOutput = TRUE;
        }
        else if( EQUAL(argv[i],"-a_ullr") && i < argc - 4 )
        {
            sEnvelop.MinX = atof(argv[++i]);
            sEnvelop.MaxY = atof(argv[++i]);
            sEnvelop.MaxX = atof(argv[++i]);
            sEnvelop.MinY = atof(argv[++i]);
            bGotBounds = TRUE;
            bCreateOutput = TRUE;
        }
        else if( EQUAL(argv[i],"-co") && i < argc-1 )
        {
            papszCreateOptions = CSLAddString( papszCreateOptions, argv[++i] );
            bCreateOutput = TRUE;
        }
        else if( EQUAL(argv[i],"-ot") && i < argc-1 )
        {
            int	iType;
            
            for( iType = 1; iType < GDT_TypeCount; iType++ )
            {
                if( GDALGetDataTypeName((GDALDataType)iType) != NULL
                    && EQUAL(GDALGetDataTypeName((GDALDataType)iType),
                             argv[i+1]) )
                {
                    eOutputType = (GDALDataType) iType;
                }
            }

            if( eOutputType == GDT_Unknown )
            {
                printf( "Unknown output pixel type: %s\n", argv[i+1] );
                Usage();
            }
            i++;
            bCreateOutput = TRUE;
        }
        else if( (EQUAL(argv[i],"-ts") || EQUAL(argv[i],"-outsize")) && i < argc-2 )
        {
            nXSize = atoi(argv[++i]);
            nYSize = atoi(argv[++i]);
            if (nXSize <= 0 || nYSize <= 0)
            {
                printf( "Wrong value for -outsize parameters\n");
                Usage();
            }
            bCreateOutput = TRUE;
        }
        else if( EQUAL(argv[i],"-tr") && i < argc-2 )
        {
            dfXRes = atof(argv[++i]);
            dfYRes = fabs(atof(argv[++i]));
            if( dfXRes == 0 || dfYRes == 0 )
            {
                printf( "Wrong value for -tr parameters\n");
                Usage();
            }
            bCreateOutput = TRUE;
        }
        else if( EQUAL(argv[i],"-tap") )
        {
            bTargetAlignedPixels = TRUE;
            bCreateOutput = TRUE;
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

    if( adfBurnValues.size() == 0 && pszBurnAttribute == NULL && !b3D )
    {
        fprintf( stderr, "At least one of -3d, -burn or -a required.\n\n" );
        Usage();
    }

    if( bCreateOutput )
    {
        if( dfXRes == 0 && dfYRes == 0 && nXSize == 0 && nYSize == 0 )
        {
            fprintf( stderr, "'-tr xres yes' or '-ts xsize ysize' is required.\n\n" );
            Usage();
        }
    
        if (bTargetAlignedPixels && dfXRes == 0 && dfYRes == 0)
        {
            fprintf( stderr, "-tap option cannot be used without using -tr\n");
            Usage();
        }

        if( anBandList.size() != 0 )
        {
            fprintf( stderr, "-b option cannot be used when creating a GDAL dataset.\n\n" );
            Usage();
        }

        int nBandCount = 1;

        if (adfBurnValues.size() != 0)
            nBandCount = adfBurnValues.size();

        if ((int)adfInitVals.size() > nBandCount)
            nBandCount = adfInitVals.size();

        if (adfInitVals.size() == 1)
        {
            for(i=1;i<=nBandCount - 1;i++)
                adfInitVals.push_back( adfInitVals[0] );
        }

        int i;
        for(i=1;i<=nBandCount;i++)
            anBandList.push_back( i );
    }
    else
    {
        if( anBandList.size() == 0 )
            anBandList.push_back( 1 );
    }

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

    if( pszSQL == NULL && papszLayers == NULL )
    {
        if( OGR_DS_GetLayerCount(hSrcDS) == 1 )
        {
            papszLayers = CSLAddString(NULL, OGR_L_GetName(OGR_DS_GetLayer(hSrcDS, 0)));
        }
        else
        {
            fprintf( stderr, "At least one of -l or -sql required.\n\n" );
            Usage();
        }
    }

/* -------------------------------------------------------------------- */
/*      Open target raster file.  Eventually we will add optional       */
/*      creation.                                                       */
/* -------------------------------------------------------------------- */
    GDALDatasetH hDstDS = NULL;

    if (bCreateOutput)
    {
/* -------------------------------------------------------------------- */
/*      Find the output driver.                                         */
/* -------------------------------------------------------------------- */
        hDriver = GDALGetDriverByName( pszFormat );
        if( hDriver == NULL 
            || GDALGetMetadataItem( hDriver, GDAL_DCAP_CREATE, NULL ) == NULL )
        {
            int	iDr;

            printf( "Output driver `%s' not recognised or does not support\n", 
                    pszFormat );
            printf( "direct output file creation.  The following format drivers are configured\n"
                    "and support direct output:\n" );

            for( iDr = 0; iDr < GDALGetDriverCount(); iDr++ )
            {
                GDALDriverH hDriver = GDALGetDriver(iDr);

                if( GDALGetMetadataItem( hDriver, GDAL_DCAP_CREATE, NULL) != NULL )
                {
                    printf( "  %s: %s\n",
                            GDALGetDriverShortName( hDriver  ),
                            GDALGetDriverLongName( hDriver ) );
                }
            }
            printf( "\n" );
            exit( 1 );
        }

        if (!bQuiet && !bFormatExplicitelySet)
            CheckExtensionConsistency(pszDstFilename, pszFormat);
    }
    else
    {
        hDstDS = GDALOpen( pszDstFilename, GA_Update );
        if( hDstDS == NULL )
            exit( 2 );
    }

/* -------------------------------------------------------------------- */
/*      Process SQL request.                                            */
/* -------------------------------------------------------------------- */
    if( pszSQL != NULL )
    {
        OGRLayerH hLayer;

        hLayer = OGR_DS_ExecuteSQL( hSrcDS, pszSQL, NULL, NULL ); 
        if( hLayer != NULL )
        {
            if (bCreateOutput)
            {
                std::vector<OGRLayerH> ahLayers;
                ahLayers.push_back(hLayer);

                hDstDS = CreateOutputDataset(ahLayers, hSRS,
                                 bGotBounds, sEnvelop,
                                 hDriver, pszDstFilename,
                                 nXSize, nYSize, dfXRes, dfYRes,
                                 bTargetAlignedPixels,
                                 anBandList.size(), eOutputType,
                                 papszCreateOptions, adfInitVals,
                                 bNoDataSet, dfNoData);
            }

            ProcessLayer( hLayer, hSRS != NULL, hDstDS, anBandList, 
                          adfBurnValues, b3D, bInverse, pszBurnAttribute,
                          papszRasterizeOptions, pfnProgress, NULL );

            OGR_DS_ReleaseResultSet( hSrcDS, hLayer );
        }
    }

/* -------------------------------------------------------------------- */
/*      Create output file if necessary.                                */
/* -------------------------------------------------------------------- */
    int nLayerCount = CSLCount(papszLayers);

    if (bCreateOutput && hDstDS == NULL)
    {
        std::vector<OGRLayerH> ahLayers;

        for( i = 0; i < nLayerCount; i++ )
        {
            OGRLayerH hLayer = OGR_DS_GetLayerByName( hSrcDS, papszLayers[i] );
            if( hLayer == NULL )
            {
                continue;
            }
            ahLayers.push_back(hLayer);
        }

        hDstDS = CreateOutputDataset(ahLayers, hSRS,
                                bGotBounds, sEnvelop,
                                hDriver, pszDstFilename,
                                nXSize, nYSize, dfXRes, dfYRes,
                                bTargetAlignedPixels,
                                anBandList.size(), eOutputType,
                                papszCreateOptions, adfInitVals,
                                bNoDataSet, dfNoData);
    }

/* -------------------------------------------------------------------- */
/*      Process each layer.                                             */
/* -------------------------------------------------------------------- */

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

        void *pScaledProgress;
        pScaledProgress =
            GDALCreateScaledProgress( 0.0, 1.0 * (i + 1) / nLayerCount,
                                      pfnProgress, NULL );

        ProcessLayer( hLayer, hSRS != NULL, hDstDS, anBandList, 
                      adfBurnValues, b3D, bInverse, pszBurnAttribute,
                      papszRasterizeOptions, GDALScaledProgress, pScaledProgress );

        GDALDestroyScaledProgress( pScaledProgress );
    }

/* -------------------------------------------------------------------- */
/*      Cleanup                                                         */
/* -------------------------------------------------------------------- */

    OGR_DS_Destroy( hSrcDS );
    GDALClose( hDstDS );

    OSRDestroySpatialReference(hSRS);

    CSLDestroy( argv );
    CSLDestroy( papszRasterizeOptions );
    CSLDestroy( papszLayers );
    CSLDestroy( papszCreateOptions );
    
    GDALDestroyDriverManager();
    OGRCleanupAll();

    return 0;
}
