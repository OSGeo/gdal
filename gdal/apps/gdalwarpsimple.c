/******************************************************************************
 * $Id$
 *
 * Project:  Mapinfo Image Warper
 * Purpose:  Commandline program for doing a variety of image warps, including
 *           image reprojection.
 * Author:   Frank Warmerdam <warmerdam@pobox.com>
 *
 ******************************************************************************
 * Copyright (c) 2002, i3 - information integration and imaging 
 *                          Fort Collin, CO
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

#include "gdal_alg.h"
#include "cpl_string.h"
#include "ogr_srs_api.h"

CPL_CVSID("$Id$");

static GDALDatasetH 
GDALWarpCreateOutput( GDALDatasetH hSrcDS, const char *pszFilename, 
                      const char *pszFormat, const char *pszSourceSRS, 
                      const char *pszTargetSRS, int nOrder, 
                      char **papszCreateOptions );

static double	       dfMinX=0.0, dfMinY=0.0, dfMaxX=0.0, dfMaxY=0.0;
static double	       dfXRes=0.0, dfYRes=0.0;
static int             nForcePixels=0, nForceLines=0;

/************************************************************************/
/*                               Usage()                                */
/************************************************************************/

static void Usage()

{
    printf( 
        "Usage: gdalwarpsimple [--version] [--formats]\n"
        "    [-s_srs srs_def] [-t_srs srs_def] [-order n] [-et err_threshold]\n"
        "    [-te xmin ymin xmax ymax] [-tr xres yres] [-ts width height]\n"
        "    [-of format] [-co \"NAME=VALUE\"]* srcfile dstfile\n" );
    exit( 1 );
}

/************************************************************************/
/*                             SanitizeSRS                              */
/************************************************************************/

char *SanitizeSRS( const char *pszUserInput )

{
    OGRSpatialReferenceH hSRS;
    char *pszResult = NULL;

    CPLErrorReset();
    
    hSRS = OSRNewSpatialReference( NULL );
    if( OSRSetFromUserInput( hSRS, pszUserInput ) == OGRERR_NONE )
        OSRExportToWkt( hSRS, &pszResult );
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

int main( int argc, char ** argv )

{
    GDALDatasetH	hSrcDS, hDstDS;
    const char         *pszFormat = "GTiff";
    char               *pszTargetSRS = NULL;
    char               *pszSourceSRS = NULL;
    const char         *pszSrcFilename = NULL, *pszDstFilename = NULL;
    int                 bCreateOutput = FALSE, i, nOrder = 0;
    void               *hTransformArg, *hGenImgProjArg=NULL, *hApproxArg=NULL;
    char               **papszWarpOptions = NULL;
    double             dfErrorThreshold = 0.125;
    GDALTransformerFunc pfnTransformer = NULL;
    char                **papszCreateOptions = NULL;

    GDALAllRegister();

/* -------------------------------------------------------------------- */
/*      Parse arguments.                                                */
/* -------------------------------------------------------------------- */
    for( i = 1; i < argc; i++ )
    {
        if( EQUAL(argv[i],"--version") )
        {
            printf( "%s\n", GDALVersionInfo( "--version" ) );
            exit( 0 );
        }
        else if( EQUAL(argv[i],"--formats") )
        {
            int iDr;

            printf( "Supported Formats:\n" );
            for( iDr = 0; iDr < GDALGetDriverCount(); iDr++ )
            {
                GDALDriverH hDriver = GDALGetDriver(iDr);
                
                printf( "  %s: %s\n",
                        GDALGetDriverShortName( hDriver ),
                        GDALGetDriverLongName( hDriver ) );
            }

            exit( 0 );
        }
        else if( EQUAL(argv[i],"-co") && i < argc-1 )
        {
            papszCreateOptions = CSLAddString( papszCreateOptions, argv[++i] );
            bCreateOutput = TRUE;
        }   
        else if( EQUAL(argv[i],"-of") && i < argc-1 )
        {
            pszFormat = argv[++i];
            bCreateOutput = TRUE;
        }
        else if( EQUAL(argv[i],"-t_srs") && i < argc-1 )
        {
            pszTargetSRS = SanitizeSRS(argv[++i]);
        }
        else if( EQUAL(argv[i],"-s_srs") && i < argc-1 )
        {
            pszSourceSRS = SanitizeSRS(argv[++i]);
        }
        else if( EQUAL(argv[i],"-order") && i < argc-1 )
        {
            nOrder = atoi(argv[++i]);
        }
        else if( EQUAL(argv[i],"-et") && i < argc-1 )
        {
            dfErrorThreshold = CPLAtof(argv[++i]);
        }
        else if( EQUAL(argv[i],"-tr") && i < argc-2 )
        {
            dfXRes = CPLAtof(argv[++i]);
            dfYRes = fabs(CPLAtof(argv[++i]));
            bCreateOutput = TRUE;
        }
        else if( EQUAL(argv[i],"-ts") && i < argc-2 )
        {
            nForcePixels = atoi(argv[++i]);
            nForceLines = atoi(argv[++i]);
            bCreateOutput = TRUE;
        }
        else if( EQUAL(argv[i],"-te") && i < argc-4 )
        {
            dfMinX = CPLAtof(argv[++i]);
            dfMinY = CPLAtof(argv[++i]);
            dfMaxX = CPLAtof(argv[++i]);
            dfMaxY = CPLAtof(argv[++i]);
            bCreateOutput = TRUE;
        }
        else if( argv[i][0] == '-' )
            Usage();
        else if( pszSrcFilename == NULL )
            pszSrcFilename = argv[i];
        else if( pszDstFilename == NULL )
            pszDstFilename = argv[i];
        else
            Usage();
    }

    if( pszDstFilename == NULL )
        Usage();

/* -------------------------------------------------------------------- */
/*      Open source dataset.                                            */
/* -------------------------------------------------------------------- */
    hSrcDS = GDALOpen( pszSrcFilename, GA_ReadOnly );
    
    if( hSrcDS == NULL )
        exit( 2 );

/* -------------------------------------------------------------------- */
/*      Check that there's at least one raster band                     */
/* -------------------------------------------------------------------- */
    if ( GDALGetRasterCount(hSrcDS) == 0 )
    {
        fprintf(stderr, "Input file %s has no raster bands.\n", pszSrcFilename );
        exit( 2 );
    }

    if( pszSourceSRS == NULL )
    {
        if( GDALGetProjectionRef( hSrcDS ) != NULL 
            && strlen(GDALGetProjectionRef( hSrcDS )) > 0 )
            pszSourceSRS = CPLStrdup(GDALGetProjectionRef( hSrcDS ));

        else if( GDALGetGCPProjection( hSrcDS ) != NULL
                 && strlen(GDALGetGCPProjection(hSrcDS)) > 0 
                 && GDALGetGCPCount( hSrcDS ) > 1 )
            pszSourceSRS = CPLStrdup(GDALGetGCPProjection( hSrcDS ));
        else
            pszSourceSRS = CPLStrdup("");
    }

    if( pszTargetSRS == NULL )
        pszTargetSRS = CPLStrdup(pszSourceSRS);

/* -------------------------------------------------------------------- */
/*      Does the output dataset already exist?                          */
/* -------------------------------------------------------------------- */
    CPLPushErrorHandler( CPLQuietErrorHandler );
    hDstDS = GDALOpen( pszDstFilename, GA_Update );
    CPLPopErrorHandler();

    if( hDstDS != NULL && bCreateOutput )
    {
        fprintf( stderr, 
                 "Output dataset %s exists,\n"
                 "but some commandline options were provided indicating a new dataset\n"
                 "should be created.  Please delete existing dataset and run again.",
                 pszDstFilename );
        exit( 1 );
    }

/* -------------------------------------------------------------------- */
/*      If not, we need to create it.                                   */
/* -------------------------------------------------------------------- */
    if( hDstDS == NULL )
    {
        hDstDS = GDALWarpCreateOutput( hSrcDS, pszDstFilename, pszFormat, 
                                       pszSourceSRS, pszTargetSRS, nOrder,
                                       papszCreateOptions );
        papszWarpOptions = CSLSetNameValue( papszWarpOptions, "INIT", "0" );
        CSLDestroy( papszCreateOptions );
        papszCreateOptions = NULL;
    }

    if( hDstDS == NULL )
        exit( 1 );

/* -------------------------------------------------------------------- */
/*      Create a transformation object from the source to               */
/*      destination coordinate system.                                  */
/* -------------------------------------------------------------------- */
    hTransformArg = hGenImgProjArg = 
        GDALCreateGenImgProjTransformer( hSrcDS, pszSourceSRS, 
                                         hDstDS, pszTargetSRS, 
                                         TRUE, 1000.0, nOrder );

    if( hTransformArg == NULL )
        exit( 1 );

    pfnTransformer = GDALGenImgProjTransform;

/* -------------------------------------------------------------------- */
/*      Warp the transformer with a linear approximator unless the      */
/*      acceptable error is zero.                                       */
/* -------------------------------------------------------------------- */
    if( dfErrorThreshold != 0.0 )
    {
        hTransformArg = hApproxArg = 
            GDALCreateApproxTransformer( GDALGenImgProjTransform, 
                                         hGenImgProjArg, dfErrorThreshold );
        pfnTransformer = GDALApproxTransform;
    }

/* -------------------------------------------------------------------- */
/*      Now actually invoke the warper to do the work.                  */
/* -------------------------------------------------------------------- */
    GDALSimpleImageWarp( hSrcDS, hDstDS, 0, NULL, 
                         pfnTransformer, hTransformArg,
                         GDALTermProgress, NULL, papszWarpOptions );

    CSLDestroy( papszWarpOptions );

    if( hApproxArg != NULL )
        GDALDestroyApproxTransformer( hApproxArg );

    if( hGenImgProjArg != NULL )
        GDALDestroyGenImgProjTransformer( hGenImgProjArg );

/* -------------------------------------------------------------------- */
/*      Cleanup.                                                        */
/* -------------------------------------------------------------------- */
    GDALClose( hDstDS );
    GDALClose( hSrcDS );

    GDALDumpOpenDatasets( stderr );

    GDALDestroyDriverManager();
    
    exit( 0 );
}

/************************************************************************/
/*                        GDALWarpCreateOutput()                        */
/*                                                                      */
/*      Create the output file based on various commandline options,    */
/*      and the input file.                                             */
/************************************************************************/

static GDALDatasetH 
GDALWarpCreateOutput( GDALDatasetH hSrcDS, const char *pszFilename, 
                      const char *pszFormat, const char *pszSourceSRS, 
                      const char *pszTargetSRS, int nOrder,
                      char **papszCreateOptions )

{
    GDALDriverH hDriver;
    GDALDatasetH hDstDS;
    void *hTransformArg;
    double adfDstGeoTransform[6];
    int nPixels=0, nLines=0;
    GDALColorTableH hCT;

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

/* -------------------------------------------------------------------- */
/*      Create a transformation object from the source to               */
/*      destination coordinate system.                                  */
/* -------------------------------------------------------------------- */
    hTransformArg = 
        GDALCreateGenImgProjTransformer( hSrcDS, pszSourceSRS, 
                                         NULL, pszTargetSRS, 
                                         TRUE, 1000.0, nOrder );

    if( hTransformArg == NULL )
        return NULL;

/* -------------------------------------------------------------------- */
/*      Get approximate output definition.                              */
/* -------------------------------------------------------------------- */
    if( GDALSuggestedWarpOutput( hSrcDS, 
                                 GDALGenImgProjTransform, hTransformArg, 
                                 adfDstGeoTransform, &nPixels, &nLines )
        != CE_None )
        return NULL;

    GDALDestroyGenImgProjTransformer( hTransformArg );

/* -------------------------------------------------------------------- */
/*      Did the user override some parameters?                          */
/* -------------------------------------------------------------------- */
    if( dfXRes != 0.0 && dfYRes != 0.0 )
    {
        CPLAssert( nPixels == 0 && nLines == 0 );
        if( dfMinX == 0.0 && dfMinY == 0.0 && dfMaxX == 0.0 && dfMaxY == 0.0 )
        {
            dfMinX = adfDstGeoTransform[0];
            dfMaxX = adfDstGeoTransform[0] + adfDstGeoTransform[1] * nPixels;
            dfMaxY = adfDstGeoTransform[3];
            dfMinY = adfDstGeoTransform[3] + adfDstGeoTransform[5] * nLines;
        }

        nPixels = (int) ((dfMaxX - dfMinX + (dfXRes/2.0)) / dfXRes);
        nLines = (int) ((dfMaxY - dfMinY + (dfYRes/2.0)) / dfYRes);
        adfDstGeoTransform[0] = dfMinX;
        adfDstGeoTransform[3] = dfMaxY;
        adfDstGeoTransform[1] = dfXRes;
        adfDstGeoTransform[5] = -dfYRes;
    }

    else if( nForcePixels != 0 && nForceLines != 0 )
    {
        if( dfMinX == 0.0 && dfMinY == 0.0 && dfMaxX == 0.0 && dfMaxY == 0.0 )
        {
            dfMinX = adfDstGeoTransform[0];
            dfMaxX = adfDstGeoTransform[0] + adfDstGeoTransform[1] * nPixels;
            dfMaxY = adfDstGeoTransform[3];
            dfMinY = adfDstGeoTransform[3] + adfDstGeoTransform[5] * nLines;
        }

        dfXRes = (dfMaxX - dfMinX) / nForcePixels;
        dfYRes = (dfMaxY - dfMinY) / nForceLines;

        adfDstGeoTransform[0] = dfMinX;
        adfDstGeoTransform[3] = dfMaxY;
        adfDstGeoTransform[1] = dfXRes;
        adfDstGeoTransform[5] = -dfYRes;

        nPixels = nForcePixels;
        nLines = nForceLines;
    }

    else if( dfMinX != 0.0 || dfMinY != 0.0 || dfMaxX != 0.0 || dfMaxY != 0.0 )
    {
        dfXRes = adfDstGeoTransform[1];
        dfYRes = fabs(adfDstGeoTransform[5]);

        nPixels = (int) ((dfMaxX - dfMinX + (dfXRes/2.0)) / dfXRes);
        nLines = (int) ((dfMaxY - dfMinY + (dfYRes/2.0)) / dfYRes);

        adfDstGeoTransform[0] = dfMinX;
        adfDstGeoTransform[3] = dfMaxY;
    }

/* -------------------------------------------------------------------- */
/*      Create the output file.                                         */
/* -------------------------------------------------------------------- */
    printf( "Creating output file is that %dP x %dL.\n", nPixels, nLines );

    hDstDS = GDALCreate( hDriver, pszFilename, nPixels, nLines, 
                         GDALGetRasterCount(hSrcDS),
                         GDALGetRasterDataType(GDALGetRasterBand(hSrcDS,1)),
                         papszCreateOptions );

    if( hDstDS == NULL )
        return NULL;

/* -------------------------------------------------------------------- */
/*      Write out the projection definition.                            */
/* -------------------------------------------------------------------- */
    GDALSetProjection( hDstDS, pszTargetSRS );
    GDALSetGeoTransform( hDstDS, adfDstGeoTransform );

/* -------------------------------------------------------------------- */
/*      Copy the color table, if required.                              */
/* -------------------------------------------------------------------- */
    hCT = GDALGetRasterColorTable( GDALGetRasterBand(hSrcDS,1) );
    if( hCT != NULL )
        GDALSetRasterColorTable( GDALGetRasterBand(hDstDS,1), hCT );

    return hDstDS;
}
    
