/******************************************************************************
 * $Id$
 *
 * Project:  GDAL Utilities
 * Purpose:  GDAL Image Translator Program
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
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
 * DEALINGS IN THE SOFTWARE.
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
            "             CInt16/CInt32/CFloat32/CFloat64}] [-strict]\n"
            "       [-of format] [-b band] [-expand {gray|rgb|rgba}]\n"
            "       [-outsize xsize[%%] ysize[%%]]\n"
            "       [-unscale] [-scale [src_min src_max [dst_min dst_max]]]\n"
            "       [-srcwin xoff yoff xsize ysize] [-projwin ulx uly lrx lry]\n"
            "       [-a_srs srs_def] [-a_ullr ulx uly lrx lry] [-a_nodata value]\n"
            "       [-gcp pixel line easting northing [elevation]]*\n" 
            "       [-mo \"META-TAG=VALUE\"]* [-q] [-sds]\n"
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
    int                 anSrcWin[4], bStrict = FALSE;
    const char          *pszProjection;
    int                 bScale = FALSE, bHaveScaleSrc = FALSE, bUnscale=FALSE;
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
    int                 nRGBExpand = 0;


    anSrcWin[0] = 0;
    anSrcWin[1] = 0;
    anSrcWin[2] = 0;
    anSrcWin[3] = 0;

    dfULX = dfULY = dfLRX = dfLRY = 0.0;
    
    /* Check strict compilation and runtime library version as we use C++ API */
    if (! GDAL_CHECK_VERSION(argv[0]))
        exit(1);

    /* Must process GDAL_SKIP before GDALAllRegister(), but we can't call */
    /* GDALGeneralCmdLineProcessor before it needs the drivers to be registered */
    /* for the --format or --formats options */
    for( i = 1; i < argc; i++ )
    {
        if( EQUAL(argv[i],"--config") && i + 2 < argc && EQUAL(argv[i + 1], "GDAL_SKIP") )
        {
            CPLSetConfigOption( argv[i+1], argv[i+2] );

            i += 2;
        }
    }

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
        if( EQUAL(argv[i], "--utility_version") )
        {
            printf("%s was compiled against GDAL %s and is running against GDAL %s\n",
                   argv[0], GDAL_RELEASE_NAME, GDALVersionInfo("RELEASE_NAME"));
            return 0;
        }
        else if( EQUAL(argv[i],"-of") && i < argc-1 )
            pszFormat = argv[++i];

        else if( EQUAL(argv[i],"-q") || EQUAL(argv[i],"-quiet") )
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
            
        else if( EQUAL(argv[i],"-strict")  )
            bStrict = TRUE;
            
        else if( EQUAL(argv[i],"-sds")  )
            bCopySubDatasets = TRUE;
            
        else if( EQUAL(argv[i],"-gcp") && i < argc - 4 )
        {
            char* endptr = NULL;
            /* -gcp pixel line easting northing [elev] */

            nGCPCount++;
            pasGCPs = (GDAL_GCP *) 
                CPLRealloc( pasGCPs, sizeof(GDAL_GCP) * nGCPCount );
            GDALInitGCPs( 1, pasGCPs + nGCPCount - 1 );

            pasGCPs[nGCPCount-1].dfGCPPixel = CPLAtofM(argv[++i]);
            pasGCPs[nGCPCount-1].dfGCPLine = CPLAtofM(argv[++i]);
            pasGCPs[nGCPCount-1].dfGCPX = CPLAtofM(argv[++i]);
            pasGCPs[nGCPCount-1].dfGCPY = CPLAtofM(argv[++i]);
            if( argv[i+1] != NULL 
                && (CPLStrtod(argv[i+1], &endptr) != 0.0 || argv[i+1][0] == '0') )
            {
                /* Check that last argument is really a number and not a filename */
                /* looking like a number (see ticket #863) */
                if (endptr && *endptr == 0)
                    pasGCPs[nGCPCount-1].dfGCPZ = CPLAtofM(argv[++i]);
            }

            /* should set id and info? */
        }   

        else if( EQUAL(argv[i],"-a_nodata") && i < argc - 1 )
        {
            bSetNoData = TRUE;
            dfNoDataReal = CPLAtofM(argv[i+1]);
            i += 1;
        }   

        else if( EQUAL(argv[i],"-a_ullr") && i < argc - 4 )
        {
            adfULLR[0] = CPLAtofM(argv[i+1]);
            adfULLR[1] = CPLAtofM(argv[i+2]);
            adfULLR[2] = CPLAtofM(argv[i+3]);
            adfULLR[3] = CPLAtofM(argv[i+4]);

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
                dfScaleSrcMin = CPLAtofM(argv[i+1]);
                dfScaleSrcMax = CPLAtofM(argv[i+2]);
                i += 2;
            }
            if( i < argc-2 && bHaveScaleSrc && ArgIsNumeric(argv[i+1]) )
            {
                dfScaleDstMin = CPLAtofM(argv[i+1]);
                dfScaleDstMax = CPLAtofM(argv[i+2]);
                i += 2;
            }
            else
            {
                dfScaleDstMin = 0.0;
                dfScaleDstMax = 255.999;
            }
        }   

        else if( EQUAL(argv[i], "-unscale") )
        {
            bUnscale = TRUE;
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
            dfULX = CPLAtofM(argv[++i]);
            dfULY = CPLAtofM(argv[++i]);
            dfLRX = CPLAtofM(argv[++i]);
            dfLRY = CPLAtofM(argv[++i]);
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

        else if( EQUAL(argv[i],"-expand") && i < argc-1 )
        {
            if (EQUAL(argv[i+1], "gray"))
                nRGBExpand = 1;
            else if (EQUAL(argv[i+1], "rgb"))
                nRGBExpand = 3;
            else if (EQUAL(argv[i+1], "rgba"))
                nRGBExpand = 4;
            else
            {
                printf( "Value %s unsupported. Only gray, rgb or rgba are supported.\n\n", 
                    argv[i] );
                Usage();
                GDALDestroyDriverManager();
                exit( 2 );
            }
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

    if ( strcmp(pszSource, pszDest) == 0)
    {
        fprintf(stderr, "Source and destination datasets must be different.\n");
        GDALDestroyDriverManager();
        exit( 1 );
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
    if( !bCopySubDatasets 
        && CSLCount(GDALGetMetadata( hDataset, "SUBDATASETS" )) > 0 
        && GDALGetRasterCount(hDataset) == 0 )
    {
        fprintf( stderr,
                 "Input file contains subdatasets. Please, select one of them for reading.\n" );
        GDALClose( hDataset );
        GDALDestroyDriverManager();
        exit( 1 );
    }

    if( CSLCount(GDALGetMetadata( hDataset, "SUBDATASETS" )) > 0 
        && bCopySubDatasets )
    {
        char **papszSubdatasets = GDALGetMetadata(hDataset,"SUBDATASETS");
        char *pszSubDest = (char *) CPLMalloc(strlen(pszDest)+32);
        int i;
        int bOldSubCall = bSubCall;
        char** papszDupArgv = CSLDuplicate(argv);
        int nRet = 0;

        CPLFree(papszDupArgv[iDstFileArg]);
        papszDupArgv[iDstFileArg] = pszSubDest;
        bSubCall = TRUE;
        for( i = 0; papszSubdatasets[i] != NULL; i += 2 )
        {
            CPLFree(papszDupArgv[iSrcFileArg]);
            papszDupArgv[iSrcFileArg] = CPLStrdup(strstr(papszSubdatasets[i],"=")+1);
            sprintf( pszSubDest, "%s%d", pszDest, i/2 + 1 );
            nRet = ProxyMain( argc, papszDupArgv );
            if (nRet != 0)
                break;
        }
        CSLDestroy(papszDupArgv);
        
        bSubCall = bOldSubCall;
        CSLDestroy(argv);

        GDALClose( hDataset );

        if( !bSubCall )
        {
            GDALDumpOpenDatasets( stderr );
            GDALDestroyDriverManager();
        }
        return nRet;
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
        && !bScale && !bUnscale
        && CSLCount(papszMetadataOptions) == 0 && bDefBands 
        && anSrcWin[0] == 0 && anSrcWin[1] == 0 
        && anSrcWin[2] == GDALGetRasterXSize(hDataset)
        && anSrcWin[3] == GDALGetRasterYSize(hDataset) 
        && pszOXSize == NULL && pszOYSize == NULL 
        && nGCPCount == 0 && !bGotBounds
        && pszOutputSRS == NULL && !bSetNoData
        && nRGBExpand == 0)
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
                          ? CPLAtofM(pszOXSize)/100*anSrcWin[2] : atoi(pszOXSize)));
        nOYSize = (int) ((pszOYSize[strlen(pszOYSize)-1]=='%' 
                          ? CPLAtofM(pszOYSize)/100*anSrcWin[3] : atoi(pszOYSize)));
    }
    
/* ==================================================================== */
/*      Create a virtual dataset.                                       */
/* ==================================================================== */
    VRTDataset *poVDS;
        
/* -------------------------------------------------------------------- */
/*      Make a virtual clone.                                           */
/* -------------------------------------------------------------------- */
    poVDS = (VRTDataset *) VRTCreate( nOXSize, nOYSize );

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

/* -------------------------------------------------------------------- */
/*      Transfer generally applicable metadata.                         */
/* -------------------------------------------------------------------- */
    poVDS->SetMetadata( ((GDALDataset*)hDataset)->GetMetadata() );
    AttachMetadata( (GDALDatasetH) poVDS, papszMetadataOptions );

/* -------------------------------------------------------------------- */
/*      Transfer metadata that remains valid if the spatial             */
/*      arrangement of the data is unaltered.                           */
/* -------------------------------------------------------------------- */
    if( anSrcWin[0] == 0 && anSrcWin[1] == 0 
        && anSrcWin[2] == GDALGetRasterXSize(hDataset)
        && anSrcWin[3] == GDALGetRasterYSize(hDataset) 
        && pszOXSize == NULL && pszOYSize == NULL )
    {
        char **papszMD;

        papszMD = ((GDALDataset*)hDataset)->GetMetadata("RPC");
        if( papszMD != NULL )
            poVDS->SetMetadata( papszMD, "RPC" );

        papszMD = ((GDALDataset*)hDataset)->GetMetadata("GEOLOCATION");
        if( papszMD != NULL )
            poVDS->SetMetadata( papszMD, "GEOLOCATION" );
    }

    int nSrcBandCount = nBandCount;

    if (nRGBExpand != 0)
    {
        GDALRasterBand  *poSrcBand;
        poSrcBand = ((GDALDataset *) 
                     hDataset)->GetRasterBand(panBandList[0]);
        GDALColorTable* poColorTable = poSrcBand->GetColorTable();
        if (poColorTable == NULL)
        {
            fprintf(stderr, "Error : band %d has no color table\n", panBandList[0]);
            GDALClose( hDataset );
            CPLFree( panBandList );
            GDALDestroyDriverManager();
            CSLDestroy( argv );
            CSLDestroy( papszCreateOptions );
            exit( 1 );
        }
        
        /* Check that the color table only contains gray levels */
        /* when using -expand gray */
        if (nRGBExpand == 1)
        {
            int nColorCount = poColorTable->GetColorEntryCount();
            int nColor;
            for( nColor = 0; nColor < nColorCount; nColor++ )
            {
                const GDALColorEntry* poEntry = poColorTable->GetColorEntry(nColor);
                if (poEntry->c1 != poEntry->c2 || poEntry->c1 != poEntry->c2)
                {
                    fprintf(stderr, "Warning : color table contains non gray levels colors\n");
                    break;
                }
            }
        }

        if (nBandCount == 1)
            nBandCount = nRGBExpand;
        else if (nBandCount == 2 && (nRGBExpand == 3 || nRGBExpand == 4))
            nBandCount = nRGBExpand;
        else
        {
            fprintf(stderr, "Error : invalid use of -expand option.\n");
            exit( 1 );
        }
    }

/* ==================================================================== */
/*      Process all bands.                                              */
/* ==================================================================== */
    for( i = 0; i < nBandCount; i++ )
    {
        VRTSourcedRasterBand   *poVRTBand;
        GDALRasterBand  *poSrcBand;
        GDALDataType    eBandType;
        int             nComponent = 0;

        if (nRGBExpand != 0)
        {
            if (nSrcBandCount == 2 && nRGBExpand == 4 && i == 3)
                poSrcBand = ((GDALDataset *) 
                        hDataset)->GetRasterBand(panBandList[1]);
            else
            {
                poSrcBand = ((GDALDataset *) 
                        hDataset)->GetRasterBand(panBandList[0]);
                nComponent = i + 1;
            }
        }
        else
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

        if( bUnscale )
        {
            dfScale = poSrcBand->GetScale();
            dfOffset = poSrcBand->GetOffset();
        }

/* -------------------------------------------------------------------- */
/*      Create a simple or complex data source depending on the         */
/*      translation type required.                                      */
/* -------------------------------------------------------------------- */
        if( bUnscale || bScale || (nRGBExpand != 0 && i < nRGBExpand) )
        {
            poVRTBand->AddComplexSource( poSrcBand,
                                         anSrcWin[0], anSrcWin[1], 
                                         anSrcWin[2], anSrcWin[3], 
                                         0, 0, nOXSize, nOYSize,
                                         dfOffset, dfScale,
                                         VRT_NODATA_UNSET,
                                         nComponent );
        }
        else
            poVRTBand->AddSimpleSource( poSrcBand,
                                        anSrcWin[0], anSrcWin[1], 
                                        anSrcWin[2], anSrcWin[3], 
                                        0, 0, nOXSize, nOYSize );

/* -------------------------------------------------------------------- */
/*      In case of color table translate, we only set the color         */
/*      interpretation other info copied by CopyCommonInfoFrom are      */
/*      not relevant in RGB expansion.                                  */
/* -------------------------------------------------------------------- */
        if (nRGBExpand == 1)
        {
            poVRTBand->SetColorInterpretation( GCI_GrayIndex );
        }
        else if (nRGBExpand != 0 && i < nRGBExpand)
        {
            poVRTBand->SetColorInterpretation( (GDALColorInterp) (GCI_RedBand + i) );
        }

/* -------------------------------------------------------------------- */
/*      copy over some other information of interest.                   */
/* -------------------------------------------------------------------- */
        else
        {
            poVRTBand->CopyCommonInfoFrom( poSrcBand );

            if( bUnscale )
            {
                poVRTBand->SetOffset( 0.0 );
                poVRTBand->SetScale( 1.0 );
            }
        }

/* -------------------------------------------------------------------- */
/*      Set a forcable nodata value?                                    */
/* -------------------------------------------------------------------- */
        if( bSetNoData )
        {
            double dfVal = dfNoDataReal;
            int bClamped = FALSE, bRounded = FALSE;

#define CLAMP(val,type,minval,maxval) \
    do { if (val < minval) { bClamped = TRUE; val = minval; } \
    else if (val > maxval) { bClamped = TRUE; val = maxval; } \
    else if (val != (type)val) { bRounded = TRUE; val = (type)(val + 0.5); } } \
    while(0)

            switch(eBandType)
            {
                case GDT_Byte:
                    CLAMP(dfVal, GByte, 0.0, 255.0);
                    break;
                case GDT_Int16:
                    CLAMP(dfVal, GInt16, -32768.0, 32767.0);
                    break;
                case GDT_UInt16:
                    CLAMP(dfVal, GUInt16, 0.0, 65535.0);
                    break;
                case GDT_Int32:
                    CLAMP(dfVal, GInt32, -2147483648.0, 2147483647.0);
                    break;
                case GDT_UInt32:
                    CLAMP(dfVal, GUInt32, 0.0, 4294967295.0);
                    break;
                default:
                    break;
            }
                
            if (bClamped)
            {
                printf( "for band %d, nodata value has been clamped "
                       "to %.0f, the original value being out of range.\n",
                       i + 1, dfVal);
            }
            else if(bRounded)
            {
                printf("for band %d, nodata value has been rounded "
                       "to %.0f, %s being an integer datatype.\n",
                       i + 1, dfVal,
                       GDALGetDataTypeName(eBandType));
            }
            
            poVRTBand->SetNoDataValue( dfVal );
        }
    }

/* -------------------------------------------------------------------- */
/*      Write to the output file using CopyCreate().                    */
/* -------------------------------------------------------------------- */
    hOutDS = GDALCreateCopy( hDriver, pszDest, (GDALDatasetH) poVDS,
                             bStrict, papszCreateOptions, 
                             pfnProgress, NULL );
    if( hOutDS != NULL )
    {
        int bHasGotErr = FALSE;
        CPLErrorReset();
        GDALFlushCache( hOutDS );
        if (CPLGetLastErrorType() != CE_None)
            bHasGotErr = TRUE;
        GDALClose( hOutDS );
        if (bHasGotErr)
            hOutDS = NULL;
    }
    
    GDALClose( (GDALDatasetH) poVDS );
        
    GDALClose( hDataset );

    CPLFree( panBandList );
    
    CPLFree( pszOutputSRS );

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


