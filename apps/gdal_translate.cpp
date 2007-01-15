/* ****************************************************************************
 * $Id$
 *
 * Project:  GDAL Utilities
 * Purpose:  GDAL Image Translator Program
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 * ****************************************************************************
 * Copyright (c) 1998, 2002, Frank Warmerdam
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
 ****************************************************************************/

#include "cpl_vsi.h"
#include "cpl_conv.h"
#include "cpl_string.h"
#include "gdal_priv.h"
#include "ogr_spatialref.h"
#include "vrt/vrtdataset.h"

CPL_CVSID("$Id$");

static int ArgIsNumeric( const char * );
static void AttachMetadata( GDALDatasetH, char ** );
static int bSubCall = FALSE;

/*  ******************************************************************* */
/*                               Usage()                                */
/* ******************************************************************** */

static void Usage()

{
    int	iDr;
        
    printf( "Usage: gdal_translate [--help-general]\n"
            "       [-ot {Byte/Int16/UInt16/UInt32/Int32/Float32/Float64/\n"
            "             CInt16/CInt32/CFloat32/CFloat64}] [-not_strict]\n"
            "       [-of format] [-b band] [-outsize xsize[%%] ysize[%%]]\n"
            "       [-scale [src_min src_max [dst_min dst_max]]]\n"
            "       [-srcwin xoff yoff xsize ysize] [-projwin ulx uly lrx lry]\n"
            "       [-a_srs srs_def] [-a_ullr ulx uly lrx lry] [-a_nodata value]\n"
            "       [-gcp pixel line easting northing [elevation]]*\n" 
            "       [-mo \"META-TAG=VALUE\"]* [-quiet] [-sds]\n"
            "       [-co \"NAME=VALUE\"]*\n"
            "       src_dataset dst_dataset\n\n" );

    printf( "%s\n\n", GDALVersionInfo( "--version" ) );
    printf( "The following format drivers are configured and support output:\n" );
    for( iDr = 0; iDr < GDALGetDriverCount(); iDr++ )
    {
        GDALDriverH hDriver = GDALGetDriver(iDr);
        
        if( GDALGetMetadataItem( hDriver, GDAL_DCAP_CREATE, NULL ) != NULL
            || GDALGetMetadataItem( hDriver, GDAL_DCAP_CREATECOPY,
                                    NULL ) != NULL )
        {
            printf( "  %s: %s\n",
                    GDALGetDriverShortName( hDriver ),
                    GDALGetDriverLongName( hDriver ) );
        }
    }
}

/************************************************************************/
/*                             ProxyMain()                              */
/************************************************************************/

static int ProxyMain( int argc, char ** argv )

{
    GDALDatasetH	hDataset, hOutDS;
    int			i;
    int			nRasterXSize, nRasterYSize;
    const char		*pszSource=NULL, *pszDest=NULL, *pszFormat = "GTiff";
    GDALDriverH		hDriver;
    int			*panBandList = NULL, nBandCount = 0, bDefBands = TRUE;
    double		adfGeoTransform[6];
    GDALDataType	eOutputType = GDT_Unknown;
    int			nOXSize = 0, nOYSize = 0;
    char		*pszOXSize=NULL, *pszOYSize=NULL;
    char                **papszCreateOptions = NULL;
    int                 anSrcWin[4], bStrict = TRUE;
    const char          *pszProjection;
    int                 bScale = FALSE, bHaveScaleSrc = FALSE;
    double	        dfScaleSrcMin=0.0, dfScaleSrcMax=255.0;
    double              dfScaleDstMin=0.0, dfScaleDstMax=255.0;
    double              dfULX, dfULY, dfLRX, dfLRY;
    char                **papszMetadataOptions = NULL;
    char                *pszOutputSRS = NULL;
    int                 bQuiet = FALSE, bGotBounds = FALSE;
    GDALProgressFunc    pfnProgress = GDALTermProgress;
    int                 nGCPCount = 0;
    GDAL_GCP            *pasGCPs = NULL;
    int                 iSrcFileArg = -1, iDstFileArg = -1;
    int                 bCopySubDatasets = FALSE;
    double              adfULLR[4] = { 0,0,0,0 };
    int                 bSetNoData = FALSE;
    double		dfNoDataReal = 0.0;


    anSrcWin[0] = 0;
    anSrcWin[1] = 0;
    anSrcWin[2] = 0;
    anSrcWin[3] = 0;

    dfULX = dfULY = dfLRX = dfLRY = 0.0;

/* -------------------------------------------------------------------- */
/*      Register standard GDAL drivers, and process generic GDAL        */
/*      command options.                                                */
/* -------------------------------------------------------------------- */
    GDALAllRegister();
    argc = GDALGeneralCmdLineProcessor( argc, &argv, 0 );
    if( argc < 1 )
        exit( -argc );

/* -------------------------------------------------------------------- */
/*      Handle command line arguments.                                  */
/* -------------------------------------------------------------------- */
    for( i = 1; i < argc; i++ )
    {
        if( EQUAL(argv[i],"-of") && i < argc-1 )
            pszFormat = argv[++i];

        else if( EQUAL(argv[i],"-quiet") )
        {
            bQuiet = TRUE;
            pfnProgress = GDALDummyProgress;
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
                GDALDestroyDriverManager();
                exit( 2 );
            }
            i++;
        }
        else if( EQUAL(argv[i],"-b") && i < argc-1 )
        {
            if( atoi(argv[i+1]) < 1 )
            {
                printf( "Unrecognizable band number (%s).\n", argv[i+1] );
                Usage();
                GDALDestroyDriverManager();
                exit( 2 );
            }

            nBandCount++;
            panBandList = (int *) 
                CPLRealloc(panBandList, sizeof(int) * nBandCount);
            panBandList[nBandCount-1] = atoi(argv[++i]);

            if( panBandList[nBandCount-1] != nBandCount )
                bDefBands = FALSE;
        }
        else if( EQUAL(argv[i],"-not_strict")  )
            bStrict = FALSE;
            
        else if( EQUAL(argv[i],"-sds")  )
            bCopySubDatasets = TRUE;
            
        else if( EQUAL(argv[i],"-gcp") && i < argc - 4 )
        {
            /* -gcp pixel line easting northing [elev] */

            nGCPCount++;
            pasGCPs = (GDAL_GCP *) 
                CPLRealloc( pasGCPs, sizeof(GDAL_GCP) * nGCPCount );
            GDALInitGCPs( 1, pasGCPs + nGCPCount - 1 );

            pasGCPs[nGCPCount-1].dfGCPPixel = atof(argv[++i]);
            pasGCPs[nGCPCount-1].dfGCPLine = atof(argv[++i]);
            pasGCPs[nGCPCount-1].dfGCPX = atof(argv[++i]);
            pasGCPs[nGCPCount-1].dfGCPY = atof(argv[++i]);
            if( argv[i+1] != NULL 
                && (atof(argv[i+1]) != 0.0 || argv[i+1][0] == '0') )
                pasGCPs[nGCPCount-1].dfGCPZ = atof(argv[++i]);

            /* should set id and info? */
        }   

        else if( EQUAL(argv[i],"-a_nodata") && i < argc - 1 )
        {
            bSetNoData = TRUE;
            dfNoDataReal = atof(argv[i+1]);
            i += 1;
        }   

        else if( EQUAL(argv[i],"-a_ullr") && i < argc - 4 )
        {
            adfULLR[0] = atof(argv[i+1]);
            adfULLR[1] = atof(argv[i+2]);
            adfULLR[2] = atof(argv[i+3]);
            adfULLR[3] = atof(argv[i+4]);

            bGotBounds = TRUE;
            
            i += 4;
        }   

        else if( EQUAL(argv[i],"-co") && i < argc-1 )
        {
            papszCreateOptions = CSLAddString( papszCreateOptions, argv[++i] );
        }   

        else if( EQUAL(argv[i],"-scale") )
        {
            bScale = TRUE;
            if( i < argc-2 && ArgIsNumeric(argv[i+1]) )
            {
                bHaveScaleSrc = TRUE;
                dfScaleSrcMin = atof(argv[i+1]);
                dfScaleSrcMax = atof(argv[i+2]);
                i += 2;
            }
            if( i < argc-2 && bHaveScaleSrc && ArgIsNumeric(argv[i+1]) )
            {
                dfScaleDstMin = atof(argv[i+1]);
                dfScaleDstMax = atof(argv[i+2]);
                i += 2;
            }
            else
            {
                dfScaleDstMin = 0.0;
                dfScaleDstMax = 255.999;
            }
        }   

        else if( EQUAL(argv[i],"-mo") && i < argc-1 )
        {
            papszMetadataOptions = CSLAddString( papszMetadataOptions,
                                                 argv[++i] );
        }

        else if( EQUAL(argv[i],"-outsize") && i < argc-2 )
        {
            pszOXSize = argv[++i];
            pszOYSize = argv[++i];
        }   

        else if( EQUAL(argv[i],"-srcwin") && i < argc-4 )
        {
            anSrcWin[0] = atoi(argv[++i]);
            anSrcWin[1] = atoi(argv[++i]);
            anSrcWin[2] = atoi(argv[++i]);
            anSrcWin[3] = atoi(argv[++i]);
        }   

        else if( EQUAL(argv[i],"-projwin") && i < argc-4 )
        {
            dfULX = atof(argv[++i]);
            dfULY = atof(argv[++i]);
            dfLRX = atof(argv[++i]);
            dfLRY = atof(argv[++i]);
        }   

        else if( EQUAL(argv[i],"-a_srs") && i < argc-1 )
        {
            OGRSpatialReference oOutputSRS;

            if( oOutputSRS.SetFromUserInput( argv[i+1] ) != OGRERR_NONE )
            {
                fprintf( stderr, "Failed to process SRS definition: %s\n", 
                         argv[i+1] );
                GDALDestroyDriverManager();
                exit( 1 );
            }

            oOutputSRS.exportToWkt( &pszOutputSRS );
            i++;
        }   

        else if( argv[i][0] == '-' )
        {
            printf( "Option %s incomplete, or not recognised.\n\n", 
                    argv[i] );
            Usage();
            GDALDestroyDriverManager();
            exit( 2 );
        }
        else if( pszSource == NULL )
        {
            iSrcFileArg = i;
            pszSource = argv[i];
        }
        else if( pszDest == NULL )
        {
            pszDest = argv[i];
            iDstFileArg = i;
        }

        else
        {
            printf( "Too many command options.\n\n" );
            Usage();
            GDALDestroyDriverManager();
            exit( 2 );
        }
    }

    if( pszDest == NULL )
    {
        Usage();
        GDALDestroyDriverManager();
        exit( 10 );
    }

/* -------------------------------------------------------------------- */
/*      Attempt to open source file.                                    */
/* -------------------------------------------------------------------- */

    hDataset = GDALOpenShared( pszSource, GA_ReadOnly );
    
    if( hDataset == NULL )
    {
        fprintf( stderr,
                 "GDALOpen failed - %d\n%s\n",
                 CPLGetLastErrorNo(), CPLGetLastErrorMsg() );
        GDALDestroyDriverManager();
        exit( 1 );
    }

/* -------------------------------------------------------------------- */
/*      Handle subdatasets.                                             */
/* -------------------------------------------------------------------- */
    if( CSLCount(GDALGetMetadata( hDataset, "SUBDATASETS" )) > 0 )
    {
        if( !bCopySubDatasets )
        {
            fprintf( stderr,
                     "Input file contains subdatasets. Please, select one of them for reading.\n" );
        }
        else
        {
            char **papszSubdatasets = GDALGetMetadata(hDataset,"SUBDATASETS");
            char *pszSubDest = (char *) CPLMalloc(strlen(pszDest)+32);
            int i;
            int bOldSubCall = bSubCall;

            argv[iDstFileArg] = pszSubDest;
            bSubCall = TRUE;
            for( i = 0; papszSubdatasets[i] != NULL; i += 2 )
            {
                argv[iSrcFileArg] = strstr(papszSubdatasets[i],"=")+1;
                sprintf( pszSubDest, "%s%d", pszDest, i/2 + 1 );
                if( ProxyMain( argc, argv ) != 0 )
                    break;
            }

            bSubCall = bOldSubCall;
            CPLFree( pszSubDest );
        }

        GDALClose( hDataset );

        if( !bSubCall )
        {
            GDALDumpOpenDatasets( stderr );
            GDALDestroyDriverManager();
        }
        return 1;
    }

/* -------------------------------------------------------------------- */
/*      Collect some information from the source file.                  */
/* -------------------------------------------------------------------- */
    nRasterXSize = GDALGetRasterXSize( hDataset );
    nRasterYSize = GDALGetRasterYSize( hDataset );

    if( !bQuiet )
        printf( "Input file size is %d, %d\n", nRasterXSize, nRasterYSize );

    if( anSrcWin[2] == 0 && anSrcWin[3] == 0 )
    {
        anSrcWin[2] = nRasterXSize;
        anSrcWin[3] = nRasterYSize;
    }

/* -------------------------------------------------------------------- */
/*	Build band list to translate					*/
/* -------------------------------------------------------------------- */
    if( nBandCount == 0 )
    {
        nBandCount = GDALGetRasterCount( hDataset );
        if( nBandCount == 0 )
        {
            fprintf( stderr, "Input file has no bands, and so cannot be translated.\n" );
            GDALDestroyDriverManager();
            exit(1 );
        }

        panBandList = (int *) CPLMalloc(sizeof(int)*nBandCount);
        for( i = 0; i < nBandCount; i++ )
            panBandList[i] = i+1;
    }
    else
    {
        for( i = 0; i < nBandCount; i++ )
        {
            if( panBandList[i] < 1 || panBandList[i] > GDALGetRasterCount(hDataset) )
            {
                fprintf( stderr, 
                         "Band %d requested, but only bands 1 to %d available.\n",
                         panBandList[i], GDALGetRasterCount(hDataset) );
                GDALDestroyDriverManager();
                exit( 2 );
            }
        }

        if( nBandCount != GDALGetRasterCount( hDataset ) )
            bDefBands = FALSE;
    }

/* -------------------------------------------------------------------- */
/*      Compute the source window from the projected source window      */
/*      if the projected coordinates were provided.  Note that the      */
/*      projected coordinates are in ulx, uly, lrx, lry format,         */
/*      while the anSrcWin is xoff, yoff, xsize, ysize with the         */
/*      xoff,yoff being the ulx, uly in pixel/line.                     */
/* -------------------------------------------------------------------- */
    if( dfULX != 0.0 || dfULY != 0.0 
        || dfLRX != 0.0 || dfLRY != 0.0 )
    {
        double	adfGeoTransform[6];

        GDALGetGeoTransform( hDataset, adfGeoTransform );

        if( adfGeoTransform[2] != 0.0 || adfGeoTransform[4] != 0.0 )
        {
            fprintf( stderr, 
                     "The -projwin option was used, but the geotransform is\n"
                     "rotated.  This configuration is not supported.\n" );
            GDALClose( hDataset );
            CPLFree( panBandList );
            GDALDestroyDriverManager();
            exit( 1 );
        }

        anSrcWin[0] = (int) 
            ((dfULX - adfGeoTransform[0]) / adfGeoTransform[1] + 0.001);
        anSrcWin[1] = (int) 
            ((dfULY - adfGeoTransform[3]) / adfGeoTransform[5] + 0.001);

        anSrcWin[2] = (int) ((dfLRX - dfULX) / adfGeoTransform[1] + 0.5);
        anSrcWin[3] = (int) ((dfLRY - dfULY) / adfGeoTransform[5] + 0.5);

        if( !bQuiet )
            fprintf( stdout, 
                     "Computed -srcwin %d %d %d %d from projected window.\n",
                     anSrcWin[0], 
                     anSrcWin[1], 
                     anSrcWin[2], 
                     anSrcWin[3] );
        
        if( anSrcWin[0] < 0 || anSrcWin[1] < 0 
            || anSrcWin[0] + anSrcWin[2] > GDALGetRasterXSize(hDataset) 
            || anSrcWin[1] + anSrcWin[3] > GDALGetRasterYSize(hDataset) )
        {
            fprintf( stderr, 
                     "Computed -srcwin falls outside raster size of %dx%d.\n",
                     GDALGetRasterXSize(hDataset), 
                     GDALGetRasterYSize(hDataset) );
            exit( 1 );
        }
    }

/* -------------------------------------------------------------------- */
/*      Verify source window.                                           */
/* -------------------------------------------------------------------- */
    if( anSrcWin[0] < 0 || anSrcWin[1] < 0 
        || anSrcWin[2] <= 0 || anSrcWin[3] <= 0
        || anSrcWin[0] + anSrcWin[2] > GDALGetRasterXSize(hDataset) 
        || anSrcWin[1] + anSrcWin[3] > GDALGetRasterYSize(hDataset) )
    {
        fprintf( stderr, 
                 "-srcwin %d %d %d %d falls outside raster size of %dx%d\n"
                 "or is otherwise illegal.\n",
                 anSrcWin[0],
                 anSrcWin[1],
                 anSrcWin[2],
                 anSrcWin[3],
                 GDALGetRasterXSize(hDataset), 
                 GDALGetRasterYSize(hDataset) );
        exit( 1 );
    }

/* -------------------------------------------------------------------- */
/*      Find the output driver.                                         */
/* -------------------------------------------------------------------- */
    hDriver = GDALGetDriverByName( pszFormat );
    if( hDriver == NULL )
    {
        int	iDr;
        
        printf( "Output driver `%s' not recognised.\n", pszFormat );
        printf( "The following format drivers are configured and support output:\n" );
        for( iDr = 0; iDr < GDALGetDriverCount(); iDr++ )
        {
            GDALDriverH hDriver = GDALGetDriver(iDr);

            if( GDALGetMetadataItem( hDriver, GDAL_DCAP_CREATE, NULL ) != NULL
                || GDALGetMetadataItem( hDriver, GDAL_DCAP_CREATECOPY,
                                        NULL ) != NULL )
            {
                printf( "  %s: %s\n",
                        GDALGetDriverShortName( hDriver  ),
                        GDALGetDriverLongName( hDriver ) );
            }
        }
        printf( "\n" );
        Usage();
        
        GDALClose( hDataset );
        CPLFree( panBandList );
        GDALDestroyDriverManager();
        CSLDestroy( argv );
        CSLDestroy( papszCreateOptions );
        exit( 1 );
    }

/* -------------------------------------------------------------------- */
/*      The short form is to CreateCopy().  We use this if the input    */
/*      matches the whole dataset.  Eventually we should rewrite        */
/*      this entire program to use virtual datasets to construct a      */
/*      virtual input source to copy from.                              */
/* -------------------------------------------------------------------- */
    if( eOutputType == GDT_Unknown 
        && !bScale && CSLCount(papszMetadataOptions) == 0 && bDefBands 
        && anSrcWin[0] == 0 && anSrcWin[1] == 0 
        && anSrcWin[2] == GDALGetRasterXSize(hDataset)
        && anSrcWin[3] == GDALGetRasterYSize(hDataset) 
        && pszOXSize == NULL && pszOYSize == NULL 
        && nGCPCount == 0 && !bGotBounds
        && pszOutputSRS == NULL && !bSetNoData )
    {
        
        hOutDS = GDALCreateCopy( hDriver, pszDest, hDataset, 
                                 bStrict, papszCreateOptions, 
                                 pfnProgress, NULL );

        if( hOutDS != NULL )
            GDALClose( hOutDS );
        
        GDALClose( hDataset );

        CPLFree( panBandList );

        if( !bSubCall )
        {
            GDALDumpOpenDatasets( stderr );
            GDALDestroyDriverManager();
        }

        CSLDestroy( argv );
        CSLDestroy( papszCreateOptions );

        return hOutDS == NULL;
    }

/* -------------------------------------------------------------------- */
/*      Establish some parameters.                                      */
/* -------------------------------------------------------------------- */
    if( pszOXSize == NULL )
    {
        nOXSize = anSrcWin[2];
        nOYSize = anSrcWin[3];
    }
    else
    {
        nOXSize = (int) ((pszOXSize[strlen(pszOXSize)-1]=='%' 
                          ? atof(pszOXSize)/100*anSrcWin[2] : atoi(pszOXSize)));
        nOYSize = (int) ((pszOYSize[strlen(pszOYSize)-1]=='%' 
                          ? atof(pszOYSize)/100*anSrcWin[3] : atoi(pszOYSize)));
    }
    
/* ==================================================================== */
/*      Create a virtual dataset.                                       */
/* ==================================================================== */
    VRTDataset *poVDS;
        
/* -------------------------------------------------------------------- */
/*      Make a virtual clone.                                           */
/* -------------------------------------------------------------------- */
    poVDS = new VRTDataset( nOXSize, nOYSize );

    if( nGCPCount == 0 )
    {
        if( pszOutputSRS != NULL )
        {
            poVDS->SetProjection( pszOutputSRS );
        }
        else
        {
            pszProjection = GDALGetProjectionRef( hDataset );
            if( pszProjection != NULL && strlen(pszProjection) > 0 )
                poVDS->SetProjection( pszProjection );
        }
    }

    if( bGotBounds )
    {
        adfGeoTransform[0] = adfULLR[0];
        adfGeoTransform[1] = (adfULLR[2] - adfULLR[0]) / nOXSize;
        adfGeoTransform[2] = 0.0;
        adfGeoTransform[3] = adfULLR[1];
        adfGeoTransform[4] = 0.0;
        adfGeoTransform[5] = (adfULLR[3] - adfULLR[1]) / nOYSize;

        poVDS->SetGeoTransform( adfGeoTransform );
    }

    else if( GDALGetGeoTransform( hDataset, adfGeoTransform ) == CE_None 
        && nGCPCount == 0 )
    {
        adfGeoTransform[0] += anSrcWin[0] * adfGeoTransform[1]
            + anSrcWin[1] * adfGeoTransform[2];
        adfGeoTransform[3] += anSrcWin[0] * adfGeoTransform[4]
            + anSrcWin[1] * adfGeoTransform[5];
        
        adfGeoTransform[1] *= anSrcWin[2] / (double) nOXSize;
        adfGeoTransform[2] *= anSrcWin[3] / (double) nOYSize;
        adfGeoTransform[4] *= anSrcWin[2] / (double) nOXSize;
        adfGeoTransform[5] *= anSrcWin[3] / (double) nOYSize;
        
        poVDS->SetGeoTransform( adfGeoTransform );
    }

    if( nGCPCount != 0 )
    {
        const char *pszGCPProjection = pszOutputSRS;

        if( pszGCPProjection == NULL )
            pszGCPProjection = GDALGetGCPProjection( hDataset );
        if( pszGCPProjection == NULL )
            pszGCPProjection = "";

        poVDS->SetGCPs( nGCPCount, pasGCPs, pszGCPProjection );

        GDALDeinitGCPs( nGCPCount, pasGCPs );
        CPLFree( pasGCPs );
    }

    else if( GDALGetGCPCount( hDataset ) > 0 )
    {
        GDAL_GCP *pasGCPs;
        int       nGCPs = GDALGetGCPCount( hDataset );

        pasGCPs = GDALDuplicateGCPs( nGCPs, GDALGetGCPs( hDataset ) );

        for( i = 0; i < nGCPs; i++ )
        {
            pasGCPs[i].dfGCPPixel -= anSrcWin[0];
            pasGCPs[i].dfGCPLine  -= anSrcWin[1];
            pasGCPs[i].dfGCPPixel *= (nOXSize / (double) anSrcWin[2] );
            pasGCPs[i].dfGCPLine  *= (nOYSize / (double) anSrcWin[3] );
        }
            
        poVDS->SetGCPs( nGCPs, pasGCPs,
                        GDALGetGCPProjection( hDataset ) );

        GDALDeinitGCPs( nGCPs, pasGCPs );
        CPLFree( pasGCPs );
    }

    poVDS->SetMetadata( ((GDALDataset*)hDataset)->GetMetadata() );
    AttachMetadata( (GDALDatasetH) poVDS, papszMetadataOptions );

    for( i = 0; i < nBandCount; i++ )
    {
        VRTSourcedRasterBand   *poVRTBand;
        GDALRasterBand  *poSrcBand;
        GDALDataType    eBandType;

        poSrcBand = ((GDALDataset *) 
                     hDataset)->GetRasterBand(panBandList[i]);

/* -------------------------------------------------------------------- */
/*      Select output data type to match source.                        */
/* -------------------------------------------------------------------- */
        if( eOutputType == GDT_Unknown )
            eBandType = poSrcBand->GetRasterDataType();
        else
            eBandType = eOutputType;

/* -------------------------------------------------------------------- */
/*      Create this band.                                               */
/* -------------------------------------------------------------------- */
        poVDS->AddBand( eBandType, NULL );
        poVRTBand = (VRTSourcedRasterBand *) poVDS->GetRasterBand( i+1 );
            
/* -------------------------------------------------------------------- */
/*      Do we need to collect scaling information?                      */
/* -------------------------------------------------------------------- */
        double dfScale=1.0, dfOffset=0.0;

        if( bScale && !bHaveScaleSrc )
        {
            double	adfCMinMax[2];
            GDALComputeRasterMinMax( poSrcBand, TRUE, adfCMinMax );
            dfScaleSrcMin = adfCMinMax[0];
            dfScaleSrcMax = adfCMinMax[1];
        }

        if( bScale )
        {
            if( dfScaleSrcMax == dfScaleSrcMin )
                dfScaleSrcMax += 0.1;
            if( dfScaleDstMax == dfScaleDstMin )
                dfScaleDstMax += 0.1;

            dfScale = (dfScaleDstMax - dfScaleDstMin) 
                / (dfScaleSrcMax - dfScaleSrcMin);
            dfOffset = -1 * dfScaleSrcMin * dfScale + dfScaleDstMin;
        }

/* -------------------------------------------------------------------- */
/*      Create a simple or complex data source depending on the         */
/*      translation type required.                                      */
/* -------------------------------------------------------------------- */
        if( bScale )
        {
            poVRTBand->AddComplexSource( poSrcBand,
                                         anSrcWin[0], anSrcWin[1], 
                                         anSrcWin[2], anSrcWin[3], 
                                         0, 0, nOXSize, nOYSize,
                                         dfOffset, dfScale );
        }
        else
            poVRTBand->AddSimpleSource( poSrcBand,
                                        anSrcWin[0], anSrcWin[1], 
                                        anSrcWin[2], anSrcWin[3], 
                                        0, 0, nOXSize, nOYSize );

/* -------------------------------------------------------------------- */
/*      copy over some other information of interest.                   */
/* -------------------------------------------------------------------- */
        poVRTBand->CopyCommonInfoFrom( poSrcBand );

/* -------------------------------------------------------------------- */
/*      Set a forcable nodata value?                                    */
/* -------------------------------------------------------------------- */
        if( bSetNoData )
            poVRTBand->SetNoDataValue( dfNoDataReal );
    }

/* -------------------------------------------------------------------- */
/*      Write to the output file using CopyCreate().                    */
/* -------------------------------------------------------------------- */
    hOutDS = GDALCreateCopy( hDriver, pszDest, (GDALDatasetH) poVDS,
                             bStrict, papszCreateOptions, 
                             pfnProgress, NULL );
    if( hOutDS != NULL )
    {
        GDALClose( hOutDS );
    }

    GDALClose( (GDALDatasetH) poVDS );
        
    GDALClose( hDataset );

    CPLFree( panBandList );

    if( !bSubCall )
    {
        GDALDumpOpenDatasets( stderr );
        GDALDestroyDriverManager();
    }

    CSLDestroy( argv );
    CSLDestroy( papszCreateOptions );
    
    return hOutDS == NULL;
}


/************************************************************************/
/*                            ArgIsNumeric()                            */
/************************************************************************/

int ArgIsNumeric( const char *pszArg )

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
/*                           AttachMetadata()                           */
/************************************************************************/

static void AttachMetadata( GDALDatasetH hDS, char **papszMetadataOptions )

{
    int nCount = CSLCount(papszMetadataOptions);
    int i;

    for( i = 0; i < nCount; i++ )
    {
        char    *pszKey = NULL;
        const char *pszValue;
        
        pszValue = CPLParseNameValue( papszMetadataOptions[i], &pszKey );
        GDALSetMetadataItem(hDS,pszKey,pszValue,NULL);
        CPLFree( pszKey );
    }

    CSLDestroy( papszMetadataOptions );
}

/************************************************************************/
/*                                main()                                */
/************************************************************************/

int main( int argc, char ** argv )

{
    return ProxyMain( argc, argv );
}


