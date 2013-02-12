/******************************************************************************
 * $Id$
 *
 * Project:  Contour Generator
 * Purpose:  Contour Generator mainline.
 * Author:   Frank Warmerdam <warmerdam@pobox.com>
 *
 ******************************************************************************
 * Copyright (c) 2003, Applied Coherent Technology (www.actgate.com). 
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
#include "cpl_string.h"
#include "ogr_api.h"
#include "ogr_srs_api.h"

CPL_CVSID("$Id$");

/************************************************************************/
/*                            ArgIsNumeric()                            */
/************************************************************************/

static int ArgIsNumeric( const char *pszArg )

{
    return CPLGetValueType(pszArg) != CPL_VALUE_STRING;
}

/************************************************************************/
/*                               Usage()                                */
/************************************************************************/

static void Usage(const char* pszErrorMsg = NULL)

{
    printf( 
        "Usage: gdal_contour [-b <band>] [-a <attribute_name>] [-3d] [-inodata]\n"
        "                    [-snodata n] [-f <formatname>] [-i <interval>]\n"
        "                    [-f <formatname>] [[-dsco NAME=VALUE] ...] [[-lco NAME=VALUE] ...]\n"   
        "                    [-off <offset>] [-fl <level> <level>...]\n" 
        "                    [-nln <outlayername>] [-q]\n"
        "                    <src_filename> <dst_filename>\n" );

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
    GDALDatasetH	hSrcDS;
    int i, b3D = FALSE, bNoDataSet = FALSE, bIgnoreNoData = FALSE;
    int nBandIn = 1;
    double dfInterval = 0.0, dfNoData = 0.0, dfOffset = 0.0;
    const char *pszSrcFilename = NULL;
    const char *pszDstFilename = NULL;
    const char *pszElevAttrib = NULL;
    const char *pszFormat = "ESRI Shapefile";
    char        **papszDSCO = NULL, **papszLCO = NULL;
    double adfFixedLevels[1000];
    int    nFixedLevelCount = 0;
    const char *pszNewLayerName = "contour";
    int bQuiet = FALSE;
    GDALProgressFunc pfnProgress = NULL;

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
        else if( EQUAL(argv[i], "--help") )
            Usage();
        else if( EQUAL(argv[i],"-a") )
        {
            CHECK_HAS_ENOUGH_ADDITIONAL_ARGS(1);
            pszElevAttrib = argv[++i];
        }
        else if( EQUAL(argv[i],"-off") )
        {
            CHECK_HAS_ENOUGH_ADDITIONAL_ARGS(1);
            dfOffset = atof(argv[++i]);
        }
        else if( EQUAL(argv[i],"-i") )
        {
            CHECK_HAS_ENOUGH_ADDITIONAL_ARGS(1);
            dfInterval = atof(argv[++i]);
        }
        else if( EQUAL(argv[i],"-fl") )
        {
            if( i >= argc-1 )
                Usage(CPLSPrintf("%s option requires at least 1 argument", argv[i]));
            while( i < argc-1 
                   && nFixedLevelCount 
                             < (int)(sizeof(adfFixedLevels)/sizeof(double))
                   && ArgIsNumeric(argv[i+1]) )
                adfFixedLevels[nFixedLevelCount++] = atof(argv[++i]);
        }
        else if( EQUAL(argv[i],"-b") )
        {
            CHECK_HAS_ENOUGH_ADDITIONAL_ARGS(1);
            nBandIn = atoi(argv[++i]);
        }
        else if( EQUAL(argv[i],"-f") )
        {
            CHECK_HAS_ENOUGH_ADDITIONAL_ARGS(1);
            pszFormat = argv[++i];
        }
        else if( EQUAL(argv[i],"-dsco") )
        {
            CHECK_HAS_ENOUGH_ADDITIONAL_ARGS(1);
            papszDSCO = CSLAddString(papszDSCO, argv[++i] );
        }
        else if( EQUAL(argv[i],"-lco") )
        {
            CHECK_HAS_ENOUGH_ADDITIONAL_ARGS(1);
            papszLCO = CSLAddString(papszLCO, argv[++i] );
        }
        else if( EQUAL(argv[i],"-3d")  )
        {
            b3D = TRUE;
        }
        else if( EQUAL(argv[i],"-snodata") )
        {
            CHECK_HAS_ENOUGH_ADDITIONAL_ARGS(1);
            bNoDataSet = TRUE;
            dfNoData = atof(argv[++i]);
        }
        else if( EQUAL(argv[i],"-nln") )
        {
            CHECK_HAS_ENOUGH_ADDITIONAL_ARGS(1);
            pszNewLayerName = argv[++i];
        }
        else if( EQUAL(argv[i],"-inodata") )
        {
            bIgnoreNoData = TRUE;
        }
        else if ( EQUAL(argv[i],"-q") || EQUAL(argv[i],"-quiet") )
        {
            bQuiet = TRUE;
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
            Usage("Too many command options.");
    }

    if( dfInterval == 0.0 && nFixedLevelCount == 0 )
    {
        Usage("Neither -i nor -fl are specified.");
    }

    if (pszSrcFilename == NULL)
    {
        Usage("Missing source filename.");
    }

    if (pszDstFilename == NULL)
    {
        Usage("Missing destination filename.");
    }
    
    if (!bQuiet)
        pfnProgress = GDALTermProgress;

/* -------------------------------------------------------------------- */
/*      Open source raster file.                                        */
/* -------------------------------------------------------------------- */
    GDALRasterBandH hBand;

    hSrcDS = GDALOpen( pszSrcFilename, GA_ReadOnly );
    if( hSrcDS == NULL )
        exit( 2 );

    hBand = GDALGetRasterBand( hSrcDS, nBandIn );
    if( hBand == NULL )
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
    OGRSpatialReferenceH hSRS = NULL;

    const char *pszWKT = GDALGetProjectionRef( hSrcDS );

    if( pszWKT != NULL && strlen(pszWKT) != 0 )
        hSRS = OSRNewSpatialReference( pszWKT );

/* -------------------------------------------------------------------- */
/*      Create the outputfile.                                          */
/* -------------------------------------------------------------------- */
    OGRDataSourceH hDS;
    OGRSFDriverH hDriver = OGRGetDriverByName( pszFormat );
    OGRFieldDefnH hFld;
    OGRLayerH hLayer;

    if( hDriver == NULL )
    {
        fprintf( stderr, "Unable to find format driver named %s.\n", 
                 pszFormat );
        exit( 10 );
    }

    hDS = OGR_Dr_CreateDataSource( hDriver, pszDstFilename, papszDSCO );
    if( hDS == NULL )
        exit( 1 );

    hLayer = OGR_DS_CreateLayer( hDS, pszNewLayerName, hSRS, 
                                 b3D ? wkbLineString25D : wkbLineString,
                                 papszLCO );
    if( hLayer == NULL )
        exit( 1 );

    hFld = OGR_Fld_Create( "ID", OFTInteger );
    OGR_Fld_SetWidth( hFld, 8 );
    OGR_L_CreateField( hLayer, hFld, FALSE );
    OGR_Fld_Destroy( hFld );

    if( pszElevAttrib )
    {
        hFld = OGR_Fld_Create( pszElevAttrib, OFTReal );
        OGR_Fld_SetWidth( hFld, 12 );
        OGR_Fld_SetPrecision( hFld, 3 );
        OGR_L_CreateField( hLayer, hFld, FALSE );
        OGR_Fld_Destroy( hFld );
    }

/* -------------------------------------------------------------------- */
/*      Invoke.                                                         */
/* -------------------------------------------------------------------- */
    CPLErr eErr;
    
    eErr = GDALContourGenerate( hBand, dfInterval, dfOffset, 
                         nFixedLevelCount, adfFixedLevels,
                         bNoDataSet, dfNoData, hLayer, 
                         OGR_FD_GetFieldIndex( OGR_L_GetLayerDefn( hLayer ), 
                                               "ID" ), 
                         (pszElevAttrib == NULL) ? -1 :
                                 OGR_FD_GetFieldIndex( OGR_L_GetLayerDefn( hLayer ), 
                                                       pszElevAttrib ), 
                         pfnProgress, NULL );

    OGR_DS_Destroy( hDS );
    GDALClose( hSrcDS );

    if (hSRS)
        OSRDestroySpatialReference( hSRS );

    CSLDestroy( argv );
    CSLDestroy( papszDSCO );
    CSLDestroy( papszLCO );
    GDALDestroyDriverManager();
    OGRCleanupAll();

    return 0;
}
