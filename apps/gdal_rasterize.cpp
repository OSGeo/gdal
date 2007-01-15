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
 ******************************************************************************
 *
 * $Log$
 * Revision 1.4  2006/02/21 20:26:59  fwarmerdam
 * Fixed last fix.
 *
 * Revision 1.3  2006/02/21 15:14:28  fwarmerdam
 * Don't crash if less -burn options provided than -b options.
 *
 * Revision 1.2  2005/10/28 18:31:09  fwarmerdam
 * added -where and progress func
 *
 * Revision 1.1  2005/10/28 17:47:50  fwarmerdam
 * New
 *
 */

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
        "Usage: gdal_rasterize [-b band]\n"
        "       [-burn value] | [-a attribute_name] | [-3d]\n"
//      "       [-of format_driver] [-co key=value]\n"       
//      "       [-te xmin ymin xmax ymax] [-tr xres yres] [-ts width height]\n"
        "       [-l layername]* [-where expression] [-sql select_statement]\n"
        "       <src_datasource> <dst_filename>\n" );
    exit( 1 );
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
    std::vector<double> &adfBurnValues, int b3D, const char *pszBurnAttribute )

{
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

        hGeom = OGR_G_Clone( OGR_F_GetGeometryRef( hFeat ) );
        ahGeometries.push_back( hGeom );

        for( unsigned int iBand = 0; iBand < anBandList.size(); iBand++ )
        {
            if( adfBurnValues.size() > 0 )
                adfFullBurnValues.push_back( 
                    adfBurnValues[MIN(iBand,adfBurnValues.size()-1)] );
            else if( b3D )
            {
                // TODO: get geometry "z" value 
                adfFullBurnValues.push_back( 0.0 );
            }
            else if( pszBurnAttribute )
            {
                adfFullBurnValues.push_back( OGR_F_GetFieldAsDouble( hFeat, iBurnField ) );
            }
        }
        
        OGR_F_Destroy( hFeat );
    }

/* -------------------------------------------------------------------- */
/*      Perform the burn.                                               */
/* -------------------------------------------------------------------- */
    GDALRasterizeGeometries( hDstDS, anBandList.size(), &(anBandList[0]), 
                             ahGeometries.size(), &(ahGeometries[0]), 
                             NULL, NULL, &(adfFullBurnValues[0]), NULL,
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
    const char *pszSrcFilename = NULL;
    const char *pszDstFilename = NULL;
    char **papszLayers = NULL;
    const char *pszSQL = NULL;
    const char *pszBurnAttribute = NULL;
    const char *pszWHERE = NULL;
    std::vector<int> anBandList;
    std::vector<double> adfBurnValues;

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
        if( EQUAL(argv[i],"-a") && i < argc-1 )
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

    if( pszSrcFilename == NULL || pszDstFilename == NULL 
        || (pszSQL == NULL && papszLayers == NULL) )
    {
        Usage();
    }

    if( adfBurnValues.size() == 0 && pszBurnAttribute == NULL && !b3D )
    {
        fprintf( stdout, "To many of -3d, -burn and -a specified.\n" );
        exit( 1 );
    }

    if( anBandList.size() == 0 )
        anBandList.push_back( 1 );

/* -------------------------------------------------------------------- */
/*      Open source vector dataset.                                     */
/* -------------------------------------------------------------------- */
    OGRDataSourceH hSrcDS;

    hSrcDS = OGROpen( pszSrcFilename, FALSE, NULL );
    if( hSrcDS == NULL )
        exit( 1 );

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
                          adfBurnValues, b3D, pszBurnAttribute );
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
                      adfBurnValues, b3D, pszBurnAttribute );
    }

/* -------------------------------------------------------------------- */
/*      Cleanup                                                         */
/* -------------------------------------------------------------------- */
    OGR_DS_Destroy( hSrcDS );
    GDALClose( hDstDS );

    CSLDestroy( argv );
    CSLDestroy( papszLayers );
    
    GDALDestroyDriverManager();
    OGRCleanupAll();

    return 0;
}
