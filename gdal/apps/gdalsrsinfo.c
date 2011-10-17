/******************************************************************************
 * $Id$
 *
 * Project:  GDAL Utilities
 * Purpose:  Commandline application to list info about a given CRS.
 *           Outputs a number of formats (WKT, PROJ.4, etc.).
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *           Etienne Tourigny, etourigny.dev-at-gmail-dot-com       
 *
 * ****************************************************************************
 * Copyright (c) 1998, Frank Warmerdam
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
#include "ogr_srs_api.h"
#include "cpl_string.h"
#include "ogr_api.h"

CPL_CVSID("$Id$");

/************************************************************************/
/*                               Usage()                                */
/************************************************************************/

void Usage()

{
    /* printf( "Usage: gdalsrsinfo [options] srs_def[--help-general] [-h]\n" */
    /*         "       [-p] [-s] [-o {all,proj4,wkt,esri,mapinfo,xml}]\n"  */
    /*         "       srs_def\n"); */
    printf( "\nUsage: gdalsrsinfo [options] srs_def\n"
            "\n"
            "srs_def may be the filename of a dataset supported by GDAL/OGR "
            "from which to extract SRS information\n"
            "OR any of the usual GDAL/OGR forms "
            "(complete WKT, PROJ.4, EPSG:n or a file containing the SRS)\n"
            "\n"          
            "Options: \n"
            "   [--help-general] [-h]  Show help and exit\n"
            "   [-p]                   Pretty-print where applicable (e.g. WKT)\n"
            /* "   [-s]                   Be as silent as possible\n" */
            "   [-v]                   Validate SRS\n"
            "   [-o out_type]          Output type, default all\n"
            "                          {all,proj4,wkt,wkt_simple,wkt_old,esri,mapinfo,xml}\n\n" ); 

    exit( 1 );
}

/************************************************************************/
/*                                main()                                */
/************************************************************************/

int main( int argc, char ** argv ) 

{
    int i;
    int bGotSRS = FALSE;
    int bPretty = FALSE;
    int bOutputAll = FALSE;
    /* int bSilent = FALSE; */
    int bValidate = FALSE;

    char  *pszOutput = NULL;
    const char  *pszOutputType = "all";
    const char  *pszInput = NULL;

    OGRSpatialReferenceH hSRS = OSRNewSpatialReference( NULL );
    GDALDatasetH	hGDALDS = NULL; 
    OGRDataSourceH hOGRDS = NULL;
    OGRLayerH hLayer = NULL;

    /* Check that we are running against at least GDAL 1.5 */
    /* Note to developers : if we use newer API, please change the requirement */
    if (atoi(GDALVersionInfo("VERSION_NUM")) < 1500)
    {
        fprintf(stderr, "At least, GDAL >= 1.5.0 is required for this version of %s, "
                "which was compiled against GDAL %s\n", argv[0], GDAL_RELEASE_NAME);
        exit(1);
    }

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
        else if( EQUAL(argv[i], "-h") )
            Usage();
        else if( EQUAL(argv[i], "-o") )
            pszOutputType =  argv[++i];
        else if( EQUAL(argv[i], "-p") )
            bPretty = TRUE;
        /* else if( EQUAL(argv[i], "-s") ) */
        /*     bSilent = TRUE; */
        else if( EQUAL(argv[i], "-v") )
            bValidate = TRUE;
        else if( ! bGotSRS ) {

            int bGotSRSDef = FALSE;
            FILE *file;
            char  *pszProjection = NULL;
            int nTmp;

            pszInput = argv[i];
       
            /* If argument is a file, try to open it with GDALOpen() and get the projection */
            file = fopen(pszInput, "r");
            if ( file )  {
                fclose(file);
                /* supress error messages temporarily */
                CPLSetErrorHandler ( CPLQuietErrorHandler );
                /* try to open with GDAL */
                hGDALDS = GDALOpen( pszInput, GA_ReadOnly );
                if ( hGDALDS != NULL && GDALGetProjectionRef( hGDALDS ) != NULL ) {
                    pszProjection = (char *) GDALGetProjectionRef( hGDALDS ); 
                    if( OSRImportFromWkt( hSRS, &pszProjection ) == CE_None )
                        bGotSRSDef = TRUE;
                    GDALClose( hGDALDS );
                }
                /* if unsuccessful, try to open with OGR */
                if ( ! bGotSRSDef ) {
                    hOGRDS = OGROpen( pszInput, FALSE, NULL );
                    if( hOGRDS != NULL ) {
                        hLayer = OGR_DS_GetLayer( hOGRDS,0 );
                        if ( hLayer != NULL ) {
                            nTmp = OGR_L_GetFeatureCount(hLayer,TRUE);
                            hSRS = OGR_L_GetSpatialRef( hLayer );
                            if ( hSRS != NULL ) {
                                bGotSRSDef = TRUE;
                            }
                        }
                    } 
                    /* OGR_DS_Destroy( hOGRDS ); */
                }
                
                GDALDestroyDriverManager();
                CPLSetErrorHandler ( NULL );	
            }

            /* If didn't get the projection from the file, try OSRSetFromUserInput() */
            if ( ! bGotSRSDef ) {
                if( OSRSetFromUserInput( hSRS, pszInput ) != OGRERR_NONE ) {
                    fprintf( stderr, "Failed to process SRS definition: %s\n",
                             pszInput );
                    exit( 1 );
                }
            }

        }
        else
            Usage();
    }

    /* Validate - not well tested!*/
    if ( bValidate ) {
        OGRErr eErr = OSRValidate( hSRS );
        if ( eErr != OGRERR_NONE ) {
            printf( "\nValidate Fails" );
            if ( eErr == OGRERR_CORRUPT_DATA )
                printf( " - SRS is not well formed");
            else if ( eErr == OGRERR_UNSUPPORTED_SRS )
                printf(" - contains non-standard PROJECTION[] values");
            printf("\n");
        }
        else
            printf( "\nValidate Succeeds\n" );
    }
    
    /* Output */
    if ( EQUAL("all", pszOutputType ) ) {
        bOutputAll = TRUE;
        bPretty = TRUE;
    }

    printf("\n");

    if ( bOutputAll || EQUAL("proj4", pszOutputType ) ) {
        if ( bOutputAll ) printf("PROJ.4 : ");
        OSRExportToProj4( hSRS, &pszOutput );
        printf("\'%s\'\n\n",pszOutput);
    }

    if ( bOutputAll || EQUAL("wkt", pszOutputType ) ) {
        if ( bOutputAll ) printf("OGC WKT :\n");
        if ( bPretty ) 
            OSRExportToPrettyWkt( hSRS, &pszOutput, FALSE );
        else
            OSRExportToWkt( hSRS, &pszOutput );
        printf("%s\n\n",pszOutput);
    }

    if ( EQUAL("wkt_simple", pszOutputType ) ) {
        if ( bOutputAll ) printf("OGC WKT (simple):\n");
        OSRExportToPrettyWkt( hSRS, &pszOutput, TRUE );
        printf("%s\n\n",pszOutput);
    }

    if ( EQUAL("wkt_old", pszOutputType ) ) {
        if (  bOutputAll ) printf("OGC WKT (old):\n");
        OSRStripCTParms( hSRS );
        OSRExportToPrettyWkt( hSRS, &pszOutput, TRUE );
        printf("%s\n\n",pszOutput);
    }

    if ( bOutputAll || EQUAL("esri", pszOutputType ) ) {
        if ( bOutputAll ) printf("ESRI WKT :\n");
        OSRMorphToESRI( hSRS );
        if ( bPretty ) 
            OSRExportToPrettyWkt( hSRS, &pszOutput, FALSE );
        else
            OSRExportToWkt( hSRS, &pszOutput );
        printf("%s\n\n",pszOutput);
    }

    if ( bOutputAll || EQUAL("mapinfo", pszOutputType ) ) {
        if ( bOutputAll ) printf("MAPINFO : ");
        OSRExportToMICoordSys( hSRS, &pszOutput );
        printf("\'%s\'\n\n",pszOutput);
    }

    /* if ( bOutputAll || EQUAL("xml", pszOutputType ) ) { */
    if ( EQUAL("xml", pszOutputType ) ) {
        if ( bOutputAll ) printf("XML :\n");
        OSRExportToXML( hSRS, &pszOutput, NULL );
        printf("%s\n\n",pszOutput);
    }


    CSLDestroy( argv );
    // TODO: adding this causes segfault - why? */
    /* if ( hOGRDS )  OGR_DS_Destroy( hOGRDS ); */
    OSRDestroySpatialReference( hSRS );

    exit( 0 );

}
