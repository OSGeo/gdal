/******************************************************************************
 *
 * Project:  GDAL Utilities
 * Purpose:  Command line application to list info about a given CRS.
 *           Outputs a number of formats (WKT, PROJ.4, etc.).
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *           Etienne Tourigny, etourigny.dev-at-gmail-dot-com
 *
 * ****************************************************************************
 * Copyright (c) 1998, Frank Warmerdam
 * Copyright (c) 2011-2013, Even Rouault <even dot rouault at mines-paris dot org>
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

#include "gdal_priv.h"
#include "cpl_string.h"
#include "ogr_spatialref.h"
#include "ogr_api.h"
#include "ogrsf_frmts.h"
#include "commonutils.h"

CPL_CVSID("$Id$");

bool FindSRS( const char *pszInput, OGRSpatialReference &oSRS );
CPLErr PrintSRS( const OGRSpatialReference &oSRS,
                 const char * pszOutputType,
                 bool bPretty, bool bPrintSep );
void PrintSRSOutputTypes( const OGRSpatialReference &oSRS,
                          const char ** papszOutputTypes );
int FindEPSG( const OGRSpatialReference &oSRS );
int SearchCSVForWKT( const char *pszFileCSV, const char *pszTarget );

/************************************************************************/
/*                               Usage()                                */
/************************************************************************/

static void Usage(const char* pszErrorMsg = NULL)

{
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
            "   [-V]                   Validate SRS\n"
            "   [-e]                   Search for EPSG number corresponding to SRS (experimental)\n"
            "   [-o out_type]          Output type { default, all, wkt_all,\n"
            "                                        proj4, epsg,\n"
            "                                        wkt, wkt_simple, wkt_noct, wkt_esri,\n"
            "                                        mapinfo, xml }\n\n" );

    if( pszErrorMsg != NULL )
        fprintf(stderr, "\nFAILURE: %s\n", pszErrorMsg);

    exit( 1 );
}

/************************************************************************/
/*                                main()                                */
/************************************************************************/

#define CHECK_HAS_ENOUGH_ADDITIONAL_ARGS(nExtraArg) \
    do { if (i + nExtraArg >= argc) \
        Usage(CPLSPrintf("%s option requires %d argument(s)", \
                         argv[i], nExtraArg)); } while( false )

int main( int argc, char ** argv )

{
    bool bGotSRS = false;
    bool bPretty = false;
    bool bValidate = false;
    bool bFindEPSG = false;
    int            nEPSGCode = -1;
    const char     *pszInput = NULL;
    const char     *pszOutputType = "default";
    OGRSpatialReference  oSRS;

    /* Check strict compilation and runtime library version as we use C++ API */
    if (! GDAL_CHECK_VERSION(argv[0]))
        exit(1);

    EarlySetConfigOptions(argc, argv);

/* -------------------------------------------------------------------- */
/*      Register standard GDAL and OGR drivers.                         */
/* -------------------------------------------------------------------- */
    GDALAllRegister();

/* -------------------------------------------------------------------- */
/*      Register standard GDAL drivers, and process generic GDAL        */
/*      command options.                                                */
/* -------------------------------------------------------------------- */
    argc = GDALGeneralCmdLineProcessor( argc, &argv, 0 );
    if( argc < 1 )
        exit( -argc );

/* -------------------------------------------------------------------- */
/*      Parse arguments.                                                */
/* -------------------------------------------------------------------- */
    for( int i = 1; i < argc; i++ )
    {
        CPLDebug( "gdalsrsinfo", "got arg #%d : [%s]", i, argv[i] );

        if( EQUAL(argv[i], "--utility_version") )
        {
            printf("%s was compiled against GDAL %s and is running against GDAL %s\n",
                   argv[0], GDAL_RELEASE_NAME, GDALVersionInfo("RELEASE_NAME"));
            CSLDestroy( argv );
            return 0;
        }
        else if( EQUAL(argv[i], "-h") || EQUAL(argv[i], "--help") )
            Usage();
        else if( EQUAL(argv[i], "-e") )
            bFindEPSG = true;
        else if( EQUAL(argv[i], "-o") )
        {
            CHECK_HAS_ENOUGH_ADDITIONAL_ARGS(1);
            pszOutputType = argv[++i];
        }
        else if( EQUAL(argv[i], "-p") )
            bPretty = true;
        else if( EQUAL(argv[i], "-V") )
            bValidate = true;
        else if( argv[i][0] == '-' )
        {
            Usage(CPLSPrintf("Unknown option name '%s'", argv[i]));
        }
        else
            pszInput = argv[i];
    }

    if ( pszInput == NULL ) {
        CSLDestroy( argv );
        Usage("No input specified.");
    }

    /* Search for SRS */
    /* coverity[tainted_data] */
    bGotSRS = FindSRS( pszInput, oSRS ) == TRUE;

    CPLDebug( "gdalsrsinfo",
              "bGotSRS: %d bValidate: %d pszOutputType: %s bPretty: %d",
              static_cast<int>(bGotSRS),
              static_cast<int>(bValidate),
              pszOutputType,
              static_cast<int>(bPretty) );

    /* Make sure we got a SRS */
    if ( ! bGotSRS ) {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "ERROR - failed to load SRS definition from %s",
                  pszInput );
    }

    else {

        /* Find EPSG code - experimental */
        if ( EQUAL(pszOutputType,"epsg") )
            bFindEPSG = true;
        if ( bFindEPSG ) {
            CPLError( CE_Warning, CPLE_AppDefined,
                      "EPSG detection is experimental and requires new data files (see bug #4345)" );
            nEPSGCode = FindEPSG( oSRS );
            /* If found, replace oSRS based on EPSG code */
            if(nEPSGCode != -1) {
                CPLDebug( "gdalsrsinfo",
                          "Found EPSG code %d", nEPSGCode );
                OGRSpatialReference oSRS2;
                if ( oSRS2.importFromEPSG( nEPSGCode ) == OGRERR_NONE )
                    oSRS = oSRS2;
            }
        }
        /* Validate - not well tested!*/
        if ( bValidate ) {
            OGRErr eErr = oSRS.Validate( );
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
        if ( EQUAL("default", pszOutputType ) ) {
            /* does this work in MSVC? */
            const char* papszOutputTypes[] =
                { "proj4", "wkt", NULL };
            if ( bFindEPSG )
                printf("\nEPSG:%d\n",nEPSGCode);
            PrintSRSOutputTypes( oSRS, papszOutputTypes );
        }
        else if ( EQUAL("all", pszOutputType ) ) {
            if ( bFindEPSG )
                printf("\nEPSG:%d\n\n",nEPSGCode);
            const char* papszOutputTypes[] =
                {"proj4","wkt","wkt_simple","wkt_noct","wkt_esri","mapinfo","xml",NULL};
            PrintSRSOutputTypes( oSRS, papszOutputTypes );
        }
        else if ( EQUAL("wkt_all", pszOutputType ) ) {
            const char* papszOutputTypes[] =
                { "wkt", "wkt_simple", "wkt_noct", "wkt_esri", NULL };
            PrintSRSOutputTypes( oSRS, papszOutputTypes );
        }
        else {
            if ( bPretty )
                printf( "\n" );
            if ( EQUAL(pszOutputType,"epsg") )
                printf("EPSG:%d\n",nEPSGCode);
            else
                PrintSRS( oSRS, pszOutputType, bPretty, FALSE );
            if ( bPretty )
                printf( "\n" );
        }
    }

    /* cleanup anything left */
    GDALDestroyDriverManager();
    OGRCleanupAll();
    CSLDestroy( argv );

    return 0;
}

/************************************************************************/
/*                      FindSRS()                                       */
/*                                                                      */
/*      Search for SRS from pszInput, update oSRS.                      */
/************************************************************************/
bool FindSRS( const char *pszInput, OGRSpatialReference &oSRS )

{
    bool bGotSRS = false;
    GDALDataset *poGDALDS = NULL;
    OGRLayer      *poLayer = NULL;
    const char    *pszProjection = NULL;
    CPLErrorHandler oErrorHandler = NULL;
    bool bIsFile = false;
    OGRErr eErr = OGRERR_NONE;

    /* temporarily suppress error messages we may get from xOpen() */
    bool bDebug = CPLTestBool(CPLGetConfigOption("CPL_DEBUG", "OFF"));
    if( !bDebug )
        oErrorHandler = CPLSetErrorHandler ( CPLQuietErrorHandler );

    /* Test if argument is a file */
    VSILFILE *fp = VSIFOpenL( pszInput, "r" );
    if ( fp )  {
        bIsFile = true;
        VSIFCloseL( fp );
        CPLDebug( "gdalsrsinfo", "argument is a file" );
    }

    /* try to open with GDAL */
    if( !STARTS_WITH(pszInput, "http://spatialreference.org/") )    {
        CPLDebug( "gdalsrsinfo", "trying to open with GDAL" );
        poGDALDS = (GDALDataset *) GDALOpenEx( pszInput, 0, NULL, NULL, NULL );
    }
    if ( poGDALDS != NULL ) {
        pszProjection = poGDALDS->GetProjectionRef( );
        if( pszProjection != NULL && pszProjection[0] != '\0' )
        {
            char* pszProjectionTmp = (char*) pszProjection;
            if( oSRS.importFromWkt( &pszProjectionTmp ) == OGRERR_NONE ) {
                CPLDebug( "gdalsrsinfo", "got SRS from GDAL" );
                bGotSRS = true;
            }
        }
        else if( poGDALDS->GetLayerCount() > 0 )
        {
            poLayer = poGDALDS->GetLayer( 0 );
            if ( poLayer != NULL ) {
                OGRSpatialReference *poSRS = poLayer->GetSpatialRef( );
                if ( poSRS != NULL ) {
                    CPLDebug( "gdalsrsinfo", "got SRS from OGR" );
                    bGotSRS = true;
                    OGRSpatialReference* poSRSClone = poSRS->Clone();
                    oSRS = *poSRSClone;
                    OGRSpatialReference::DestroySpatialReference( poSRSClone );
                }
            }
        }
        GDALClose( (GDALDatasetH) poGDALDS );
        if ( ! bGotSRS )
            CPLDebug( "gdalsrsinfo", "did not open with GDAL" );
    }

    /* Try ESRI file */
    if ( ! bGotSRS && bIsFile && (strstr(pszInput,".prj") != NULL) ) {
        CPLDebug( "gdalsrsinfo",
                  "trying to get SRS from ESRI .prj file [%s]", pszInput );

        char **pszTemp;
        if ( strstr(pszInput,"ESRI::") != NULL )
            pszTemp = CSLLoad( pszInput+6 );
        else
            pszTemp = CSLLoad( pszInput );

        if( pszTemp ) {
            eErr = oSRS.importFromESRI( pszTemp );
            CSLDestroy( pszTemp );
        }
        else
            eErr = OGRERR_UNSUPPORTED_SRS;

        if( eErr != OGRERR_NONE ) {
            CPLDebug( "gdalsrsinfo", "did not get SRS from ESRI .prj file" );
        }
        else {
            CPLDebug( "gdalsrsinfo", "got SRS from ESRI .prj file" );
            bGotSRS = true;
        }
    }

    /* Last resort, try OSRSetFromUserInput() */
    if ( ! bGotSRS ) {
        CPLDebug( "gdalsrsinfo",
                  "trying to get SRS from user input [%s]", pszInput );

        if( CPLGetConfigOption("CPL_ALLOW_VSISTDIN", NULL) == NULL )
            CPLSetConfigOption("CPL_ALLOW_VSISTDIN", "YES");

        eErr = oSRS.SetFromUserInput( pszInput );

       if(  eErr != OGRERR_NONE ) {
            CPLDebug( "gdalsrsinfo", "did not get SRS from user input" );
        }
        else {
            CPLDebug( "gdalsrsinfo", "got SRS from user input" );
            bGotSRS = true;
        }
    }

    /* restore error messages */
    if( !bDebug )
        CPLSetErrorHandler ( oErrorHandler );

    return bGotSRS;
}

/************************************************************************/
/*                      PrintSRS()                                      */
/*                                                                      */
/*      Print spatial reference in specified format.                    */
/************************************************************************/
CPLErr PrintSRS( const OGRSpatialReference &oSRS,
                 const char * pszOutputType,
                 bool bPretty, bool bPrintSep )

{
    if ( ! pszOutputType || EQUAL(pszOutputType,""))
        return CE_None;

    CPLDebug( "gdalsrsinfo", "PrintSRS( oSRS, %s, %d, %d )\n",
              pszOutputType, static_cast<int>(bPretty),
              static_cast<int>(bPrintSep) );

    char *pszOutput = NULL;

    if ( EQUAL("proj4", pszOutputType ) ) {
        if ( bPrintSep ) printf( "PROJ.4 : ");
        oSRS.exportToProj4( &pszOutput );
        printf( "%s\n", pszOutput );
    }

    else if ( EQUAL("wkt", pszOutputType ) ) {
        if ( bPrintSep ) printf("OGC WKT :\n");
        if ( bPretty )
            oSRS.exportToPrettyWkt( &pszOutput, FALSE );
        else
            oSRS.exportToWkt( &pszOutput );
        printf("%s\n",pszOutput);
    }

    else if (  EQUAL("wkt_simple", pszOutputType ) ) {
        if ( bPrintSep ) printf("OGC WKT (simple) :\n");
        oSRS.exportToPrettyWkt( &pszOutput, TRUE );
        printf("%s\n",pszOutput);
    }

    else if ( EQUAL("wkt_noct", pszOutputType ) ) {
        if (  bPrintSep ) printf("OGC WKT (no CT) :\n");
        OGRSpatialReference *poSRS = oSRS.Clone();
        poSRS->StripCTParms( );
        if ( bPretty )
            poSRS->exportToPrettyWkt( &pszOutput, FALSE );
        else
            poSRS->exportToWkt( &pszOutput );
        OGRSpatialReference::DestroySpatialReference( poSRS );
        printf("%s\n",pszOutput);
    }

    else if ( EQUAL("wkt_esri", pszOutputType ) ) {
        if ( bPrintSep ) printf("ESRI WKT :\n");
        OGRSpatialReference *poSRS = oSRS.Clone();
        poSRS->morphToESRI( );
        if ( bPretty )
            poSRS->exportToPrettyWkt( &pszOutput, FALSE );
        else
            poSRS->exportToWkt( &pszOutput );
        OGRSpatialReference::DestroySpatialReference( poSRS );
        printf("%s\n",pszOutput);
    }

    else if ( EQUAL("mapinfo", pszOutputType ) ) {
        if ( bPrintSep ) printf("MAPINFO : ");
        oSRS.exportToMICoordSys( &pszOutput );
        printf("\'%s\'\n",pszOutput);
    }

    else if ( EQUAL("xml", pszOutputType ) ) {
        if ( bPrintSep ) printf("XML :\n");
        oSRS.exportToXML( &pszOutput, NULL );
        printf("%s\n",pszOutput);
    }

    else {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "ERROR - %s output not supported",
                  pszOutputType );
        return CE_Failure;
    }

    CPLFree( pszOutput );

    return CE_None;
}

/************************************************************************/
/*                      PrintSRSOutputTypes()                           */
/*                                                                      */
/*      Print spatial reference in specified formats.                   */
/************************************************************************/
void PrintSRSOutputTypes( const OGRSpatialReference &oSRS,
                          const char ** papszOutputTypes )

{
    int nOutputTypes = CSLCount((char**)papszOutputTypes);
    printf( "\n" );
    for ( int i=0; i<nOutputTypes; i++ ) {
        PrintSRS( oSRS, papszOutputTypes[i], true, true );
        printf( "\n" );
    }
}

/************************************************************************/
/*                      SearchCSVForWKT()                               */
/*                                                                      */
/*      Search CSV file for target WKT, return EPSG code (or -1).       */
/*      For saving space, the file can be compressed (gz)               */
/*      If CSV file is absent are absent the function silently exits    */
/************************************************************************/
int SearchCSVForWKT( const char *pszFileCSV, const char *pszTarget )
{
    const char *pszFilename = NULL;
    const char *pszWKT = NULL;
    char szTemp[1024];
    int nPos = 0;
    const char *pszTemp = NULL;

    VSILFILE *fp = NULL;
    OGRSpatialReference oSRS;
    int nCode = 0;
    int nFound = -1;

    CPLDebug( "gdalsrsinfo",
              "SearchCSVForWKT()\nfile=%s\nWKT=%s\n",
              pszFileCSV, pszTarget);

/* -------------------------------------------------------------------- */
/*      Find and open file.                                             */
/* -------------------------------------------------------------------- */
    // pszFilename = pszFileCSV;
    pszFilename = CPLFindFile( "gdal", pszFileCSV );
    if( pszFilename == NULL )
    {
        CPLDebug( "gdalsrsinfo", "could not find support file %s",
                   pszFileCSV );
        // return OGRERR_UNSUPPORTED_SRS;
        return -1;
    }

    /* support gzipped file */
    if ( strstr( pszFileCSV,".gz") != NULL )
        snprintf( szTemp, sizeof(szTemp), "/vsigzip/%s", pszFilename);
    else
        snprintf( szTemp, sizeof(szTemp), "%s", pszFilename);

    CPLDebug( "gdalsrsinfo", "SearchCSVForWKT() using file %s",
              szTemp );

    fp = VSIFOpenL( szTemp, "r" );
    if( fp == NULL )
    {
        CPLDebug( "gdalsrsinfo", "could not open support file %s",
                  pszFilename );

        // return OGRERR_UNSUPPORTED_SRS;
        return -1;
    }

/* -------------------------------------------------------------------- */
/*      Process lines.                                                  */
/* -------------------------------------------------------------------- */
    const char *pszLine;

    while( (pszLine = CPLReadLine2L(fp,-1,NULL)) != NULL )

    {
        // CPLDebug( "gdalsrsinfo", "read line %s", pszLine );

        if( pszLine[0] == '#' )
            continue;
            /* do nothing */;

        // else if( STARTS_WITH_CI(pszLine, "include ") )
        // {
        //     eErr = importFromDict( pszLine + 8, pszCode );
        //     if( eErr != OGRERR_UNSUPPORTED_SRS )
        //         break;
        // }

        // else if( strstr(pszLine,",") == NULL )
        //     /* do nothing */;

        pszTemp = strstr(pszLine,",");
        if (pszTemp)
        {
            nPos = static_cast<int>(pszTemp - pszLine);

            if ( nPos == 0 )
                continue;

            strncpy( szTemp, pszLine, nPos );
            szTemp[nPos] = '\0';
            nCode = atoi(szTemp);

            pszWKT = (char *) pszLine + nPos +1;

            // CPLDebug( "gdalsrsinfo",
            //           "code=%d\nWKT=\n[%s]\ntarget=\n[%s]\n",
            //           nCode,pszWKT, pszTarget );

            if ( EQUAL(pszTarget,pszWKT) )
            {
                nFound = nCode;
                CPLDebug( "gdalsrsinfo", "found EPSG:%d\n"
                          "current=%s\ntarget= %s\n",
                          nCode, pszWKT, pszTarget );
                break;
            }
        }
    }

    VSIFCloseL( fp );

    return nFound;
}

/* TODO
   - search for well-known values (AutoIdentifyEPSG())

   - should we search .override.csv files?

   - fix precision differences (namely in degree: 17 vs 15) so we can use epsg_ogc_simple
target:
 orig: GEOGCS["SAD69",DATUM["D_South_American_1969",SPHEROID["GRS_1967_Modified",6378160,298.25]],PRIMEM["Greenwich",0],UNIT["Degree",0.017453292519943295]]
 ESRI: GEOGCS["SAD69",DATUM["D_South_American_1969",SPHEROID["GRS_1967_Truncated",6378160,298.25]],PRIMEM["Greenwich",0],UNIT["Degree",0.017453292519943295]]
 OGC:  GEOGCS["SAD69",DATUM["South_American_Datum_1969",SPHEROID["GRS_1967_Modified",6378160,298.25]],PRIMEM["Greenwich",0],UNIT["Degree",0.017453292519943295]]
database:
 ESRI: GEOGCS["SAD69",DATUM["D_South_American_1969",SPHEROID["GRS_1967_Truncated",6378160,298.25]],PRIMEM["Greenwich",0],UNIT["Degree",0.017453292519943295]]
 OGC:  GEOGCS["SAD69",DATUM["South_American_Datum_1969",SPHEROID["GRS 1967 Modified",6378160,298.25]],PRIMEM["Greenwich",0],UNIT["degree",0.0174532925199433]]
*/

/************************************************************************/
/*                      FindEPSG()                                      */
/*                                                                      */
/*      Return EPSG code corresponding to spatial reference (or -1)     */
/************************************************************************/
int FindEPSG( const OGRSpatialReference &oSRS )
{
    char *pszWKT = NULL;
    char *pszESRI = NULL;
    OGRSpatialReference *poSRS = NULL;

    poSRS = oSRS.Clone();
    poSRS->StripCTParms();
    poSRS->exportToWkt( &pszWKT );
    OGRSpatialReference::DestroySpatialReference( poSRS );

    poSRS = oSRS.Clone();
    poSRS->morphToESRI( );
    poSRS->exportToWkt( &pszESRI );
    OGRSpatialReference::DestroySpatialReference( poSRS );

    CPLDebug( "gdalsrsinfo", "FindEPSG()\nWKT (OGC)= %s\nWKT (ESRI)=%s",
              pszWKT,pszESRI );

    /* search for EPSG code in epsg_*.wkt.gz files */
    /* using ESRI WKT for now, as it seems to work best */
    int nFound = SearchCSVForWKT( "epsg_esri.wkt.gz", pszESRI );
    if ( nFound == -1 )
        nFound = SearchCSVForWKT( "epsg_ogc_simple.wkt.gz", pszESRI );
    if ( nFound == -1 )
        nFound = SearchCSVForWKT( "epsg_ogc.wkt.gz", pszESRI );

    CPLFree( pszWKT );
    CPLFree( pszESRI );

    return nFound;
}
