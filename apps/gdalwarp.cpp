/******************************************************************************
 * $Id$
 *
 * Project:  High Performance Image Reprojector
 * Purpose:  Test program for high performance warper API.
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
 ******************************************************************************
 *
 * $Log$
 * Revision 1.13  2004/11/14 04:57:04  fwarmerdam
 * added -srcalpha switch, and automatic alpha detection
 *
 * Revision 1.12  2004/11/05 06:15:08  fwarmerdam
 * Don't double free the warpoptions array.
 *
 * Revision 1.11  2004/11/05 05:53:43  fwarmerdam
 * Avoid various memory leaks.
 *
 * Revision 1.10  2004/10/07 15:53:42  fwarmerdam
 * added preliminary alpha band support
 *
 * Revision 1.9  2004/08/31 19:58:57  warmerda
 * Added error check if dst srs given but no source srs available.
 * http://208.24.120.44/show_bug.cgi?id=603
 *
 * Revision 1.8  2004/08/11 21:10:29  warmerda
 * Removed extra dumpopendatasets call.
 *
 * Revision 1.7  2004/08/11 20:11:24  warmerda
 * Added special VRT mode
 *
 * Revision 1.6  2004/07/28 17:56:00  warmerda
 * use return instead of exit() to avoid lame warnings on windows
 *
 * Revision 1.5  2004/04/02 17:33:22  warmerda
 * added GDALGeneralCmdLineProcessor()
 *
 * Revision 1.4  2004/04/01 19:51:18  warmerda
 * Added the -dstnodata commandline switch.
 *
 * Revision 1.3  2004/03/17 05:49:26  warmerda
 * Fixed assert check in GDALWarpCreateOutput().
 *
 * Revision 1.2  2003/09/19 17:52:21  warmerda
 * removed planned -gcp option
 *
 * Revision 1.1  2003/09/19 17:40:46  warmerda
 * Renamed from gdalwarptest.cpp to gdalwarp.cpp, replacing old gdalwarp. 
 *
 * Revision 1.12  2003/07/04 11:53:14  dron
 * Added `-rcs' option to select bicubic B-spline resampling.
 *
 * Revision 1.11  2003/06/05 16:55:42  warmerda
 * enable INIT_DEST=0 by default when creating new files
 *
 * Revision 1.10  2003/05/28 18:18:43  warmerda
 * added -q (quiet) flag
 *
 * Revision 1.9  2003/05/21 14:40:40  warmerda
 * fix error message
 *
 * Revision 1.8  2003/05/20 18:35:38  warmerda
 * added error reporting if SRS import fails
 *
 * Revision 1.7  2003/05/06 18:11:29  warmerda
 * added -multi in usage
 *
 * Revision 1.6  2003/04/23 05:18:02  warmerda
 * added -multi switch
 *
 * Revision 1.5  2003/04/21 17:21:04  warmerda
 * Fixed -wm switch.
 *
 * Revision 1.4  2003/03/28 17:43:04  warmerda
 * added -wm option to control warp memory
 *
 * Revision 1.3  2003/03/18 17:37:44  warmerda
 * add color table copying
 *
 * Revision 1.2  2003/03/02 05:24:02  warmerda
 * added -srcnodata option
 *
 * Revision 1.1  2003/02/22 02:03:41  warmerda
 * New
 *
 */

#include "gdalwarper.h"
#include "cpl_string.h"
#include "ogr_srs_api.h"

CPL_CVSID("$Id$");

static GDALDatasetH 
GDALWarpCreateOutput( GDALDatasetH hSrcDS, const char *pszFilename, 
                      const char *pszFormat, const char *pszSourceSRS, 
                      const char *pszTargetSRS, int nOrder, 
                      char **papszCreateOptions, GDALDataType eDT );

static double	       dfMinX=0.0, dfMinY=0.0, dfMaxX=0.0, dfMaxY=0.0;
static double	       dfXRes=0.0, dfYRes=0.0;
static int             nForcePixels=0, nForceLines=0, bQuiet = FALSE;
static int             bEnableDstAlpha = FALSE, bEnableSrcAlpha = FALSE;

static int             bVRT = FALSE;

/************************************************************************/
/*                               Usage()                                */
/************************************************************************/

static void Usage()

{
    printf( 
        "Usage: gdalwarp [--help-general] [--formats]\n"
        "    [-s_srs srs_def] [-t_srs srs_def] [-order n] [-et err_threshold]\n"
        "    [-te xmin ymin xmax ymax] [-tr xres yres] [-ts width height]\n"
        "    [-wo \"NAME=VALUE\"] [-ot Byte/Int16/...] [-wt Byte/Int16]\n"
        "    [-srcnodata value [value...]] [-dstnodata value [value...]] -dstalpha\n" 
        "    [-rn] [-rb] [-rc] [-rcs] [-wm memory_in_mb] [-multi] [-q]\n"
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
    double             dfWarpMemoryLimit = 0.0;
    GDALTransformerFunc pfnTransformer = NULL;
    char                **papszCreateOptions = NULL;
    GDALDataType        eOutputType = GDT_Unknown, eWorkingType = GDT_Unknown; 
    GDALResampleAlg     eResampleAlg = GRA_NearestNeighbour;
    const char          *pszSrcNodata = NULL;
    const char          *pszDstNodata = NULL;
    int                 bMulti = FALSE;

    GDALAllRegister();
    argc = GDALGeneralCmdLineProcessor( argc, &argv, 0 );
    if( argc < 1 )
        exit( -argc );

/* -------------------------------------------------------------------- */
/*      Parse arguments.                                                */
/* -------------------------------------------------------------------- */
    for( i = 1; i < argc; i++ )
    {
        if( EQUAL(argv[i],"-co") && i < argc-1 )
        {
            papszCreateOptions = CSLAddString( papszCreateOptions, argv[++i] );
            bCreateOutput = TRUE;
        }   
        else if( EQUAL(argv[i],"-wo") && i < argc-1 )
        {
            papszWarpOptions = CSLAddString( papszWarpOptions, argv[++i] );
        }   
        else if( EQUAL(argv[i],"-multi") )
        {
            bMulti = TRUE;
        }   
        else if( EQUAL(argv[i],"-q") )
        {
            bQuiet = TRUE;
        }   
        else if( EQUAL(argv[i],"-dstalpha") )
        {
            bEnableDstAlpha = TRUE;
        }
        else if( EQUAL(argv[i],"-srcalpha") )
        {
            bEnableSrcAlpha = TRUE;
        }
        else if( EQUAL(argv[i],"-of") && i < argc-1 )
        {
            pszFormat = argv[++i];
            bCreateOutput = TRUE;
            if( EQUAL(pszFormat,"VRT") )
                bVRT = TRUE;
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
            dfErrorThreshold = atof(argv[++i]);
        }
        else if( EQUAL(argv[i],"-wm") && i < argc-1 )
        {
            if( atof(argv[i+1]) < 4000 )
                dfWarpMemoryLimit = atof(argv[i+1]) * 1024 * 1024;
            else
                dfWarpMemoryLimit = atof(argv[i+1]);
            i++;
        }
        else if( EQUAL(argv[i],"-srcnodata") && i < argc-1 )
        {
            pszSrcNodata = argv[++i];
        }
        else if( EQUAL(argv[i],"-dstnodata") && i < argc-1 )
        {
            pszDstNodata = argv[++i];
        }
        else if( EQUAL(argv[i],"-tr") && i < argc-2 )
        {
            dfXRes = atof(argv[++i]);
            dfYRes = fabs(atof(argv[++i]));
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
                exit( 2 );
            }
            i++;
            bCreateOutput = TRUE;
        }
        else if( EQUAL(argv[i],"-wt") && i < argc-1 )
        {
            int	iType;
            
            for( iType = 1; iType < GDT_TypeCount; iType++ )
            {
                if( GDALGetDataTypeName((GDALDataType)iType) != NULL
                    && EQUAL(GDALGetDataTypeName((GDALDataType)iType),
                             argv[i+1]) )
                {
                    eWorkingType = (GDALDataType) iType;
                }
            }

            if( eWorkingType == GDT_Unknown )
            {
                printf( "Unknown output pixel type: %s\n", argv[i+1] );
                Usage();
                exit( 2 );
            }
            i++;
        }
        else if( EQUAL(argv[i],"-ts") && i < argc-2 )
        {
            nForcePixels = atoi(argv[++i]);
            nForceLines = atoi(argv[++i]);
            bCreateOutput = TRUE;
        }
        else if( EQUAL(argv[i],"-te") && i < argc-4 )
        {
            dfMinX = atof(argv[++i]);
            dfMinY = atof(argv[++i]);
            dfMaxX = atof(argv[++i]);
            dfMaxY = atof(argv[++i]);
            bCreateOutput = TRUE;
        }
        else if( EQUAL(argv[i],"-rn") )
            eResampleAlg = GRA_NearestNeighbour;

        else if( EQUAL(argv[i],"-rb") )
            eResampleAlg = GRA_Bilinear;

        else if( EQUAL(argv[i],"-rc") )
            eResampleAlg = GRA_Cubic;

        else if( EQUAL(argv[i],"-rcs") )
            eResampleAlg = GRA_CubicSpline;

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

    if( pszTargetSRS != NULL && strlen(pszSourceSRS) == 0 )
    {
        fprintf( stderr, "A target coordinate system was specified, but there is no source coordinate\nsystem.  Consider using -s_srs option to provide a source coordinate system.\nOperation terminated.\n" );
        exit( 1 );
    }

    if( pszTargetSRS == NULL )
        pszTargetSRS = CPLStrdup(pszSourceSRS);

    if( GDALGetRasterColorInterpretation( 
            GDALGetRasterBand(hSrcDS,GDALGetRasterCount(hSrcDS)) ) 
        == GCI_AlphaBand 
        && !bEnableSrcAlpha )
    {
        bEnableSrcAlpha = TRUE;
        printf( "Using band %d of source image as alpha.\n", 
                GDALGetRasterCount(hSrcDS) );
    }
        

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
                 "should be created.  Please delete existing dataset and run again.\n",
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
                                       papszCreateOptions, eOutputType );
        bCreateOutput = TRUE;

        if( CSLFetchNameValue( papszWarpOptions, "INIT_DEST" ) == NULL 
            && pszDstNodata == NULL )
        {
            papszWarpOptions = CSLSetNameValue(papszWarpOptions,
                                               "INIT_DEST", "0");
        }
        else if( CSLFetchNameValue( papszWarpOptions, "INIT_DEST" ) == NULL )
        {
            papszWarpOptions = CSLSetNameValue(papszWarpOptions,
                                               "INIT_DEST", "NO_DATA" );
        }

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

    CPLFree( pszSourceSRS );
    pszSourceSRS = NULL;

    CPLFree( pszTargetSRS );
    pszTargetSRS = NULL;

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
/*      Setup warp options.                                             */
/* -------------------------------------------------------------------- */
    GDALWarpOptions *psWO = GDALCreateWarpOptions();

    psWO->papszWarpOptions = papszWarpOptions;
    psWO->eWorkingDataType = eWorkingType;
    psWO->eResampleAlg = eResampleAlg;

    psWO->hSrcDS = hSrcDS;
    psWO->hDstDS = hDstDS;

    psWO->pfnTransformer = pfnTransformer;
    psWO->pTransformerArg = hTransformArg;

    if( !bQuiet )
        psWO->pfnProgress = GDALTermProgress;

    if( dfWarpMemoryLimit != 0.0 )
        psWO->dfWarpMemoryLimit = dfWarpMemoryLimit;

/* -------------------------------------------------------------------- */
/*      Setup band mapping.                                             */
/* -------------------------------------------------------------------- */
    if( bEnableSrcAlpha )
        psWO->nBandCount = GDALGetRasterCount(hSrcDS) - 1;
    else
        psWO->nBandCount = GDALGetRasterCount(hSrcDS);

    psWO->panSrcBands = (int *) CPLMalloc(psWO->nBandCount*sizeof(int));
    psWO->panDstBands = (int *) CPLMalloc(psWO->nBandCount*sizeof(int));

    for( i = 0; i < psWO->nBandCount; i++ )
    {
        psWO->panSrcBands[i] = i+1;
        psWO->panDstBands[i] = i+1;
    }

/* -------------------------------------------------------------------- */
/*      Setup alpha bands used if any.                                  */
/* -------------------------------------------------------------------- */
    if( bEnableSrcAlpha )
        psWO->nSrcAlphaBand = GDALGetRasterCount(hSrcDS);

    if( !bEnableDstAlpha 
        && GDALGetRasterCount(hDstDS) == psWO->nBandCount+1 
        && GDALGetRasterColorInterpretation( 
            GDALGetRasterBand(hDstDS,GDALGetRasterCount(hDstDS))) 
        == GCI_AlphaBand )
    {
        printf( "Using band %d of destination image as alpha.\n", 
                GDALGetRasterCount(hDstDS) );
                
        bEnableDstAlpha = TRUE;
    }

    if( bEnableDstAlpha )
        psWO->nDstAlphaBand = GDALGetRasterCount(hDstDS);

/* -------------------------------------------------------------------- */
/*      Setup NODATA options.                                           */
/* -------------------------------------------------------------------- */
    if( pszSrcNodata != NULL )
    {
        char **papszTokens = CSLTokenizeString( pszSrcNodata );
        int  nTokenCount = CSLCount(papszTokens);

        psWO->padfSrcNoDataReal = (double *) 
            CPLMalloc(psWO->nBandCount*sizeof(double));
        psWO->padfSrcNoDataImag = (double *) 
            CPLMalloc(psWO->nBandCount*sizeof(double));

        for( i = 0; i < psWO->nBandCount; i++ )
        {
            if( i < nTokenCount )
            {
                CPLStringToComplex( papszTokens[i], 
                                    psWO->padfSrcNoDataReal + i,
                                    psWO->padfSrcNoDataImag + i );
            }
            else
            {
                psWO->padfSrcNoDataReal[i] = psWO->padfSrcNoDataReal[i-1];
                psWO->padfSrcNoDataImag[i] = psWO->padfSrcNoDataImag[i-1];
            }
        }

        CSLDestroy( papszTokens );
    }

/* -------------------------------------------------------------------- */
/*      If the output dataset was created, and we have a destination    */
/*      nodata value, go through marking the bands with the information.*/
/* -------------------------------------------------------------------- */
    if( pszDstNodata != NULL && bCreateOutput )
    {
        char **papszTokens = CSLTokenizeString( pszDstNodata );
        int  nTokenCount = CSLCount(papszTokens);

        psWO->padfDstNoDataReal = (double *) 
            CPLMalloc(psWO->nBandCount*sizeof(double));
        psWO->padfDstNoDataImag = (double *) 
            CPLMalloc(psWO->nBandCount*sizeof(double));

        for( i = 0; i < psWO->nBandCount; i++ )
        {
            if( i < nTokenCount )
            {
                CPLStringToComplex( papszTokens[i], 
                                    psWO->padfDstNoDataReal + i,
                                    psWO->padfDstNoDataImag + i );
            }
            else
            {
                psWO->padfDstNoDataReal[i] = psWO->padfDstNoDataReal[i-1];
                psWO->padfDstNoDataImag[i] = psWO->padfDstNoDataImag[i-1];
            }

            if( bCreateOutput )
            {
                GDALSetRasterNoDataValue( 
                    GDALGetRasterBand( hDstDS, psWO->panDstBands[i] ), 
                    psWO->padfDstNoDataReal[i] );
            }
        }

        CSLDestroy( papszTokens );
    }

/* -------------------------------------------------------------------- */
/*      If we are producing VRT output, then just initialize it with    */
/*      the warp options and write out now rather than proceeding       */
/*      with the operations.                                            */
/* -------------------------------------------------------------------- */
    if( bVRT )
    {
        if( GDALInitializeWarpedVRT( hDstDS, psWO ) != CE_None )
            exit( 1 );

        GDALClose( hDstDS );
        GDALClose( hSrcDS );
        
        CSLDestroy( argv );
    
        GDALDumpOpenDatasets( stderr );
        
        GDALDestroyDriverManager();
        
        return 0;
    }

/* -------------------------------------------------------------------- */
/*      Initialize and execute the warp.                                */
/* -------------------------------------------------------------------- */
    GDALWarpOperation oWO;

    if( oWO.Initialize( psWO ) == CE_None )
    {
        if( bMulti )
            oWO.ChunkAndWarpMulti( 0, 0, 
                                   GDALGetRasterXSize( hDstDS ),
                                   GDALGetRasterYSize( hDstDS ) );
        else
            oWO.ChunkAndWarpImage( 0, 0, 
                                   GDALGetRasterXSize( hDstDS ),
                                   GDALGetRasterYSize( hDstDS ) );
    }

/* -------------------------------------------------------------------- */
/*      Cleanup                                                         */
/* -------------------------------------------------------------------- */
    if( hApproxArg != NULL )
        GDALDestroyApproxTransformer( hApproxArg );

    if( hGenImgProjArg != NULL )
        GDALDestroyGenImgProjTransformer( hGenImgProjArg );

/* -------------------------------------------------------------------- */
/*      Cleanup.                                                        */
/* -------------------------------------------------------------------- */
    GDALClose( hDstDS );
    GDALClose( hSrcDS );

    GDALDestroyWarpOptions( psWO );

    CSLDestroy( argv );

    GDALDumpOpenDatasets( stderr );

    GDALDestroyDriverManager();
    
    return 0;
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
                      char **papszCreateOptions, GDALDataType eDT )


{
    GDALDriverH hDriver;
    GDALDatasetH hDstDS;
    void *hTransformArg;
    double adfDstGeoTransform[6];
    int nPixels=0, nLines=0;

    if( eDT == GDT_Unknown )
        eDT = GDALGetRasterDataType(GDALGetRasterBand(hSrcDS,1));

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
/*      For virtual output files, we have to set a special subclass     */
/*      of dataset to create.                                           */
/* -------------------------------------------------------------------- */
    if( bVRT )
        papszCreateOptions = 
            CSLSetNameValue( papszCreateOptions, "SUBCLASS", 
                             "VRTWarpedDataset" );

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
        CPLAssert( nForcePixels == 0 && nForceLines == 0 );
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
/*      Do we want to generate an alpha band in the output file?        */
/* -------------------------------------------------------------------- */
    int nDstBandCount = GDALGetRasterCount(hSrcDS);

    if( bEnableSrcAlpha )
        nDstBandCount--;

    if( bEnableDstAlpha )
        nDstBandCount++;

/* -------------------------------------------------------------------- */
/*      Create the output file.                                         */
/* -------------------------------------------------------------------- */
    if( !bQuiet )
        printf( "Creating output file is that %dP x %dL.\n", nPixels, nLines );

    hDstDS = GDALCreate( hDriver, pszFilename, nPixels, nLines, 
                         nDstBandCount, eDT, papszCreateOptions );
    
    if( hDstDS == NULL )
        return NULL;

/* -------------------------------------------------------------------- */
/*      Write out the projection definition.                            */
/* -------------------------------------------------------------------- */
    GDALSetProjection( hDstDS, pszTargetSRS );
    GDALSetGeoTransform( hDstDS, adfDstGeoTransform );

/* -------------------------------------------------------------------- */
/*      Try to set color interpretation of output file alpha band.      */
/*      TODO: We should likely try to copy the other bands too.         */
/* -------------------------------------------------------------------- */
    if( bEnableDstAlpha )
    {
        GDALSetRasterColorInterpretation( 
            GDALGetRasterBand( hDstDS, nDstBandCount ), 
            GCI_AlphaBand );
    }

/* -------------------------------------------------------------------- */
/*      Copy the color table, if required.                              */
/* -------------------------------------------------------------------- */
    GDALColorTableH hCT;

    hCT = GDALGetRasterColorTable( GDALGetRasterBand(hSrcDS,1) );
    if( hCT != NULL )
        GDALSetRasterColorTable( GDALGetRasterBand(hDstDS,1), hCT );

    return hDstDS;
}
    
