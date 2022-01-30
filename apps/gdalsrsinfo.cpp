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
 * Copyright (c) 2011-2013, Even Rouault <even dot rouault at spatialys.com>
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

#include "cpl_string.h"
#include "gdal_version.h"
#include "gdal_priv.h"
#include "ogr_spatialref.h"
#include "ogr_api.h"
#include "ogrsf_frmts.h"
#include "commonutils.h"

#include "proj.h"

CPL_CVSID("$Id$")

bool FindSRS( const char *pszInput, OGRSpatialReference &oSRS );
CPLErr PrintSRS( const OGRSpatialReference &oSRS,
                 const char * pszOutputType,
                 bool bPretty, bool bPrintSep );
void PrintSRSOutputTypes( const OGRSpatialReference &oSRS,
                          const char * const * papszOutputTypes,
                          bool bPretty );

/************************************************************************/
/*                               Usage()                                */
/************************************************************************/

static void Usage(const char* pszErrorMsg = nullptr)

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
            "   [--single-line]        Print WKT on single line\n"
            "   [-V]                   Validate SRS\n"
            "   [-e]                   Search for EPSG number(s) corresponding to SRS\n"
            "   [-o out_type]          Output type { default, all, wkt_all,\n"
#if PROJ_VERSION_MAJOR > 6 || PROJ_VERSION_MINOR >= 2
            "                                        PROJJSON, proj4, epsg,\n"
#else
            "                                        proj4, epsg,\n"
#endif
            "                                        wkt1, wkt_simple, wkt_noct, wkt_esri,\n"
            "                                        wkt2, wkt2_2015, wkt2_2018, mapinfo, xml }\n\n" );

    if( pszErrorMsg != nullptr )
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

MAIN_START(argc, argv)

{
    bool bGotSRS = false;
    bool bPretty = true;
    bool bValidate = false;
    bool bFindEPSG = false;
    int            nEPSGCode = -1;
    const char     *pszInput = nullptr;
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
        else if( EQUAL(argv[i], "--single-line") )
            bPretty = false;
        else if( EQUAL(argv[i], "-V") )
            bValidate = true;
        else if( argv[i][0] == '-' )
        {
            Usage(CPLSPrintf("Unknown option name '%s'", argv[i]));
        }
        else
            pszInput = argv[i];
    }

    if ( pszInput == nullptr ) {
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
        exit(1);
    }

    else {
        int nEntries = 0;
        int* panConfidence = nullptr;
        OGRSpatialReferenceH* pahSRS = nullptr;

        /* Find EPSG code */
        if ( EQUAL(pszOutputType,"epsg") )
            bFindEPSG = true;

        if ( bFindEPSG ) {

            pahSRS =
                OSRFindMatches( reinterpret_cast<OGRSpatialReferenceH>(
                                    const_cast<OGRSpatialReference*>(&oSRS)),
                                nullptr,
                                &nEntries,
                                &panConfidence );
        }

        for( int i = 0; i < (nEntries ? nEntries : 1); i ++ )
        {
            if( nEntries )
            {
                oSRS = *reinterpret_cast<OGRSpatialReference*>(pahSRS[i]);
                if( panConfidence[i] != 100 )
                {
                    printf("Confidence in this match: %d %%\n",
                           panConfidence[i]);
                }

                const char* pszAuthorityCode =
                    oSRS.GetAuthorityCode(nullptr);
                if( pszAuthorityCode )
                    nEPSGCode = atoi(pszAuthorityCode);
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
                const char* papszOutputTypes[] =
                    { "proj4", "wkt2", nullptr };
                if ( bFindEPSG )
                    printf("\nEPSG:%d\n",nEPSGCode);
                PrintSRSOutputTypes( oSRS, papszOutputTypes, bPretty );
            }
            else if ( EQUAL("all", pszOutputType ) ) {
                if ( bFindEPSG )
                    printf("\nEPSG:%d\n\n",nEPSGCode);
                const char* papszOutputTypes[] =
                    {"proj4", "wkt1", "wkt2_2015", "wkt2_2018", "wkt_simple","wkt_noct","wkt_esri","mapinfo","xml",
#if PROJ_VERSION_MAJOR > 6 || PROJ_VERSION_MINOR >= 2
                     "PROJJSON",
#endif
                     nullptr};
                PrintSRSOutputTypes( oSRS, papszOutputTypes, bPretty );
            }
            else if ( EQUAL("wkt_all", pszOutputType ) ) {
                const char* papszOutputTypes[] =
                    { "wkt1", "wkt2_2015", "wkt2_2018", "wkt_simple", "wkt_noct", "wkt_esri", nullptr };
                PrintSRSOutputTypes( oSRS, papszOutputTypes, bPretty );
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

        OSRFreeSRSArray(pahSRS);
        CPLFree(panConfidence);
    }

    /* cleanup anything left */
    GDALDestroyDriverManager();
    OGRCleanupAll();
    CSLDestroy( argv );

    return 0;
}
MAIN_END

/************************************************************************/
/*                      FindSRS()                                       */
/*                                                                      */
/*      Search for SRS from pszInput, update oSRS.                      */
/************************************************************************/
bool FindSRS( const char *pszInput, OGRSpatialReference &oSRS )

{
    bool bGotSRS = false;
    GDALDataset *poGDALDS = nullptr;
    OGRLayer      *poLayer = nullptr;
    bool bIsFile = false;

    /* temporarily suppress error messages we may get from xOpen() */
    bool bDebug = CPLTestBool(CPLGetConfigOption("CPL_DEBUG", "OFF"));
    if( !bDebug )
        CPLPushErrorHandler ( CPLQuietErrorHandler );

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
        poGDALDS = static_cast<GDALDataset *>(GDALOpenEx( pszInput, 0, nullptr, nullptr, nullptr ));
    }
    if ( poGDALDS != nullptr ) {
        const OGRSpatialReference *poSRS = poGDALDS->GetSpatialRef( );
        if( poSRS )
        {
            oSRS = *poSRS;
            CPLDebug( "gdalsrsinfo", "got SRS from GDAL" );
            bGotSRS = true;
        }
        else if( poGDALDS->GetLayerCount() > 0 )
        {
            poLayer = poGDALDS->GetLayer( 0 );
            if ( poLayer != nullptr ) {
                poSRS = poLayer->GetSpatialRef( );
                if ( poSRS != nullptr ) {
                    CPLDebug( "gdalsrsinfo", "got SRS from OGR" );
                    bGotSRS = true;
                    oSRS = *poSRS;
                }
            }
        }
        GDALClose(poGDALDS);
        if ( ! bGotSRS )
            CPLDebug( "gdalsrsinfo", "did not open with GDAL" );
    }

    /* Try ESRI file */
    if ( ! bGotSRS && bIsFile && (strstr(pszInput,".prj") != nullptr) ) {
        CPLDebug( "gdalsrsinfo",
                  "trying to get SRS from ESRI .prj file [%s]", pszInput );

        char **pszTemp;
        if ( strstr(pszInput,"ESRI::") != nullptr )
            pszTemp = CSLLoad( pszInput+6 );
        else
            pszTemp = CSLLoad( pszInput );

        OGRErr eErr = OGRERR_UNSUPPORTED_SRS;
        if( pszTemp ) {
            eErr = oSRS.importFromESRI( pszTemp );
            CSLDestroy( pszTemp );
        }

        if( eErr != OGRERR_NONE ) {
            CPLDebug( "gdalsrsinfo", "did not get SRS from ESRI .prj file" );
        }
        else {
            CPLDebug( "gdalsrsinfo", "got SRS from ESRI .prj file" );
            bGotSRS = true;
        }
    }

    /* restore error messages */
    if( !bDebug )
        CPLPopErrorHandler();

    /* Last resort, try OSRSetFromUserInput() */
    if ( ! bGotSRS ) {
        CPLDebug( "gdalsrsinfo",
                  "trying to get SRS from user input [%s]", pszInput );

        if( CPLGetConfigOption("CPL_ALLOW_VSISTDIN", nullptr) == nullptr )
            CPLSetConfigOption("CPL_ALLOW_VSISTDIN", "YES");

        const OGRErr eErr = oSRS.SetFromUserInput( pszInput );

        if(  eErr != OGRERR_NONE ) {
            CPLDebug( "gdalsrsinfo", "did not get SRS from user input" );
        }
        else {
            CPLDebug( "gdalsrsinfo", "got SRS from user input" );
            bGotSRS = true;

            if( CPLGetConfigOption("OSR_USE_NON_DEPRECATED", nullptr) == nullptr )
            {
                const char* pszAuthName = oSRS.GetAuthorityName(nullptr);
                const char* pszAuthCode = oSRS.GetAuthorityCode(nullptr);

                CPLConfigOptionSetter oSetter("OSR_USE_NON_DEPRECATED", "NO", false);
                OGRSpatialReference oSRS2;
                oSRS2.SetFromUserInput( pszInput );
                const char* pszAuthCode2 = oSRS2.GetAuthorityCode(nullptr);
                if( pszAuthName && pszAuthCode &&
                    pszAuthCode2 &&
                    !EQUAL(pszAuthCode, pszAuthCode2) )
                {
                    printf("CRS %s is deprecated, and the following output "
                           "will use its non-deprecated replacement %s:%s.\n"
                           "To use the original CRS, set the OSR_USE_NON_DEPRECATED "
                           "configuration option to NO.\n",
                           pszInput, pszAuthName, pszAuthCode);
                }
            }
        }
    }

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

    char *pszOutput = nullptr;

    if ( EQUAL("proj4", pszOutputType ) ) {
        if ( bPrintSep ) printf( "PROJ.4 : ");
        oSRS.exportToProj4( &pszOutput );
        printf( "%s\n", pszOutput );
    }

    else if ( EQUAL("PROJJSON", pszOutputType ) ) {
        if ( bPrintSep ) printf( "PROJJSON :\n");
        const char* const apszOptions[] = {
            bPretty ? "MULTILINE=YES" : "MULTILINE=NO", nullptr };
        oSRS.exportToPROJJSON( &pszOutput, apszOptions );
        printf( "%s\n", pszOutput );
    }

    else if ( EQUAL("wkt1", pszOutputType ) ) {
        if ( bPrintSep ) printf("OGC WKT1 :\n");
        const char* const apszOptions[] = {
            "FORMAT=WKT1_GDAL", bPretty ? "MULTILINE=YES" : nullptr, nullptr };
        oSRS.exportToWkt(&pszOutput, apszOptions);
        printf("%s\n",pszOutput);
    }

    else if (  EQUAL("wkt_simple", pszOutputType ) ) {
        if ( bPrintSep ) printf("OGC WKT1 (simple) :\n");
        const char* const apszOptions[] = {
            "FORMAT=WKT1_SIMPLE", bPretty ? "MULTILINE=YES" : nullptr, nullptr };
        oSRS.exportToWkt(&pszOutput, apszOptions);
        printf("%s\n",pszOutput);
    }

    else if ( EQUAL("wkt_noct", pszOutputType ) ) {
        if (  bPrintSep ) printf("OGC WKT1 (no CT) :\n");
        const char* const apszOptions[] = {
            "FORMAT=SFSQL", bPretty ? "MULTILINE=YES" : nullptr, nullptr };
        oSRS.exportToWkt(&pszOutput, apszOptions);
        printf("%s\n",pszOutput);
    }

    else if ( EQUAL("wkt_esri", pszOutputType ) ) {
        if ( bPrintSep ) printf("ESRI WKT :\n");
        const char* const apszOptions[] = {
            "FORMAT=WKT1_ESRI", bPretty ? "MULTILINE=YES" : nullptr, nullptr };
        oSRS.exportToWkt(&pszOutput, apszOptions);
        printf("%s\n",pszOutput);
    }

    else if ( EQUAL("wkt2_2015", pszOutputType ) ) {
        if ( bPrintSep ) printf("OGC WKT2:2015 :\n");
        const char* const apszOptions[] = {
            "FORMAT=WKT2_2015", bPretty ? "MULTILINE=YES" : nullptr, nullptr };
        oSRS.exportToWkt(&pszOutput, apszOptions);
        printf("%s\n",pszOutput);
    }

    else if ( EQUAL("wkt", pszOutputType ) ||
              EQUAL("wkt2", pszOutputType ) ||
              EQUAL("wkt2_2018", pszOutputType ) ) {
        if ( bPrintSep ) printf("OGC WKT2:2018 :\n");
        const char* const apszOptions[] = {
            "FORMAT=WKT2_2018", bPretty ? "MULTILINE=YES" : nullptr, nullptr };
        oSRS.exportToWkt(&pszOutput, apszOptions);
        printf("%s\n",pszOutput);
    }

    else if ( EQUAL("mapinfo", pszOutputType ) ) {
        if ( bPrintSep ) printf("MAPINFO : ");
        oSRS.exportToMICoordSys( &pszOutput );
        printf("\'%s\'\n",pszOutput);
    }

    else if ( EQUAL("xml", pszOutputType ) ) {
        if ( bPrintSep ) printf("XML :\n");
        oSRS.exportToXML( &pszOutput, nullptr );
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
                          const char * const * papszOutputTypes,
                          bool bPretty )

{
    int nOutputTypes = CSLCount(papszOutputTypes);
    printf( "\n" );
    for ( int i=0; i<nOutputTypes; i++ ) {
        PrintSRS( oSRS, papszOutputTypes[i], bPretty, true );
        printf( "\n" );
    }
}
