/******************************************************************************
 * Project:  Geography Network utility
 * Purpose:  To manage GNM networks
 * Authors:  Mikhail Gusev, gusevmihs at gmail dot com
 *           Dmitry Baryshnikov, polimax@mail.ru
 *
 ******************************************************************************
 * Copyright (c) 2014, Mikhail Gusev
 * Copyright (c) 2014-2015, NextGIS <info@nextgis.com>
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

#include "commonutils.h"
#include "cpl_string.h"
#include "gdal_version.h"
#include "gdal.h"
#include "gnm.h"
#include "gnm_priv.h"

//#include "ogr_p.h"
//#include "gnm.h"
//#include "gnm_api.h"

CPL_CVSID("$Id$")

enum operation
{
    op_unknown = 0, /** no operation */
    op_info,        /** print information about network */
    op_create,      /** create a new network */
    op_import,      /** add a OGR layer to the network */
    op_connect,     /** connect features from layers added to the network */
    op_disconnect,  /** disconnect features from layers added to the network */
    op_rule,        /** add connect rule */
    op_autoconnect, /** try to connect features base on their tolerance */
    op_delete,      /** delete network */
    op_change_st    /** change vertex or edge blocking state */
};

/************************************************************************/
/*                               Usage()                                */
/************************************************************************/

static void Usage(const char* pszAdditionalMsg, int bShort = TRUE) CPL_NO_RETURN;

static void Usage(const char* pszAdditionalMsg, int bShort)
{
    printf("Usage: gnmmanage [--help][-q][-quiet][--long-usage]\n"
           "                 [info]\n"
           "                 [create [-f format_name] [-t_srs srs_name] [-dsco NAME=VALUE]... ]\n"
           "                 [import src_dataset_name] [-l layer_name]\n"
           "                 [connect gfid_src gfid_tgt gfid_con [-c cost] [-ic inv_cost] [-dir dir]]\n"
           "                 [disconnect gfid_src gfid_tgt gfid_con]\n"
           "                 [rule rule_str]\n"
           "                 [autoconnect tolerance]\n"
           "                 [delete]\n"
           "                 [change [-bl gfid][-unbl gfid][-unblall]]\n"
           "                 gnm_name [layer [layer ...]]\n");

    if (bShort)
    {
        printf("\nNote: gnmmanage --long-usage for full help.\n");
        if (pszAdditionalMsg)
            fprintf(stderr, "\nFAILURE: %s\n", pszAdditionalMsg);
        exit(1);
    }

    printf("\n   info: different information about network: system and class "
           "layers, network metadata, network spatial reference\n"
           "   create: create network\n"
           "      -f format_name: output file format name, possible values are:\n");

    int nGNMDriverCounter = 1;
    for(int iDr = 0; iDr < GDALGetDriverCount(); iDr++ )
    {
        GDALDriverH hDriver = GDALGetDriver(iDr);

        const char *pszRFlag = "", *pszWFlag, *pszVirtualIO, *pszSubdatasets;
        char** papszMD = GDALGetMetadata( hDriver, nullptr );

        if( CPLFetchBool( papszMD, GDAL_DCAP_RASTER, false ) )
            continue;
        if( CPLFetchBool( papszMD, GDAL_DCAP_VECTOR, false ) )
            continue;

        if( CPLFetchBool( papszMD, GDAL_DCAP_OPEN, false ) )
            pszRFlag = "r";

        if( CPLFetchBool( papszMD, GDAL_DCAP_CREATE, false ) )
            pszWFlag = "w+";
        else if( CPLFetchBool( papszMD, GDAL_DCAP_CREATECOPY, false ) )
            pszWFlag = "w";
        else
            pszWFlag = "o";

        if( CPLFetchBool( papszMD, GDAL_DCAP_VIRTUALIO, false ) )
            pszVirtualIO = "v";
        else
            pszVirtualIO = "";

        if( CPLFetchBool( papszMD, GDAL_DMD_SUBDATASETS, false ) )
            pszSubdatasets = "s";
        else
            pszSubdatasets = "";

        printf( "          %d. %s (%s%s%s%s): %s\n",
                nGNMDriverCounter++,
                GDALGetDriverShortName( hDriver ),
                pszRFlag, pszWFlag, pszVirtualIO, pszSubdatasets,
                GDALGetDriverLongName( hDriver ) );
    }

    printf("      -t_srs srs_name: spatial reference input\n"
           "      -dsco NAME=VALUE: network creation option set as pair=value\n"
           "   import src_dataset_name: import external layer where src_dataset_name is a dataset name to copy from\n"
           "      -l layer_name: layer name in dataset. If unset, 0 layer is copied\n"
           "   connect gfid_src gfid_tgt gfid_con: make a topological connection, where the gfid_src and gfid_tgt are vertices and gfid_con is edge (gfid_con can be -1, so the virtual connection will be created)\n"
           "      -c cost -ic inv_cost -dir dir: manually assign the following values: the cost (weight), inverse cost and direction of the edge (optional)\n"
           "   disconnect gfid_src gfid_tgt gfid_con: removes the connection from the graph\n"
           "   rule rule_str: creates a rule in the network by the given rule_str string\n"
           "   autoconnect tolerance: create topology automatically with the given double tolerance\n"
           "   delete: delete network\n"
           "   change: modify blocking state of vertices and edges ans save them in the network"
           "   -bl gfid: block feature before the main operation. Blocking features are saved in the special layer\n"
           "   -unbl gfid: unblock feature before the main operation\n"
           "   -unblall: unblock all blocked features before the main operation\n"
           "   gnm_name: the network to work with (path and name)\n"
           );

    if (pszAdditionalMsg)
        fprintf(stderr, "\nFAILURE: %s\n", pszAdditionalMsg);

    exit(1);
}

static void Usage(int bShort = TRUE)
{
    Usage(nullptr, bShort);
}

/************************************************************************/
/*                                main()                                */
/************************************************************************/

#define CHECK_HAS_ENOUGH_ADDITIONAL_ARGS(nExtraArg) \
    do { if (iArg + nExtraArg >= nArgc) \
        Usage(CPLSPrintf("%s option requires %d argument(s)", \
                         papszArgv[iArg], nExtraArg)); } while( false )

MAIN_START(nArgc, papszArgv)

{
    int bQuiet = FALSE;
    const char *pszFormat = nullptr;
    const char *pszSRS = nullptr;
    GNMGFID nSrcFID = -1;
    GNMGFID nTgtFID = -1;
    GNMGFID nConFID = -1;
    double dfDirCost = 1.0;
    double dfInvCost = 1.0;
    GNMDirection eDir = GNM_EDGE_DIR_BOTH;
    const char *pszRuleStr = "";
    const char *pszDataSource = nullptr;
    char **papszDSCO = nullptr;
    const char *pszInputDataset = nullptr;
    const char *pszInputLayer = nullptr;
    double dfTolerance = 0.0001;
    operation stOper = op_unknown;
    char **papszLayers = nullptr;
    GNMNetwork *poDS = nullptr;
    std::vector<GNMGFID> anFIDsToBlock;
    std::vector<GNMGFID> anFIDsToUnblock;
    bool bUnblockAll = false;
    int          nRet = 0;

    // Check strict compilation and runtime library version as we use C++ API
    if (! GDAL_CHECK_VERSION(papszArgv[0]))
        exit(1);

    EarlySetConfigOptions(nArgc, papszArgv);

/* -------------------------------------------------------------------- */
/*      Register format(s).                                             */
/* -------------------------------------------------------------------- */
    GDALAllRegister();

/* -------------------------------------------------------------------- */
/*      Processing command line arguments.                              */
/* -------------------------------------------------------------------- */
    nArgc = GDALGeneralCmdLineProcessor( nArgc, &papszArgv, GDAL_OF_GNM );

    if( nArgc < 1 )
    {
        exit( -nArgc );
    }

    for( int iArg = 1; iArg < nArgc; iArg++ )
    {
        if( EQUAL(papszArgv[1], "--utility_version") )
        {
            printf("%s was compiled against GDAL %s and is running against GDAL %s\n",
                    papszArgv[0], GDAL_RELEASE_NAME, GDALVersionInfo("RELEASE_NAME"));
            CSLDestroy( papszArgv );
            return 0;
        }

        else if( EQUAL(papszArgv[iArg],"--help") )
        {
            Usage();
        }

        else if ( EQUAL(papszArgv[iArg], "--long-usage") )
        {
            Usage(FALSE);
        }

        else if( EQUAL(papszArgv[iArg],"-q") || EQUAL(papszArgv[iArg],"-quiet") )
        {
            bQuiet = TRUE;
        }

        else if ( EQUAL(papszArgv[iArg],"info") )
        {
            stOper = op_info;
        }

        else if( EQUAL(papszArgv[iArg],"-f") || EQUAL(papszArgv[iArg],"-of") )
        {
            CHECK_HAS_ENOUGH_ADDITIONAL_ARGS(1);
            // coverity[tainted_data]
            pszFormat = papszArgv[++iArg];
        }

        else if( EQUAL(papszArgv[iArg],"-dsco") )
        {
            CHECK_HAS_ENOUGH_ADDITIONAL_ARGS(1);
            // coverity[tainted_data]
            papszDSCO = CSLAddString(papszDSCO, papszArgv[++iArg] );
        }

        else if( EQUAL(papszArgv[iArg],"create") )
        {
            stOper = op_create;
        }

        else if( EQUAL(papszArgv[iArg],"-t_srs") )
        {
            CHECK_HAS_ENOUGH_ADDITIONAL_ARGS(1);
            // coverity[tainted_data]
            pszSRS = papszArgv[++iArg];
        }

        else if( EQUAL(papszArgv[iArg],"import") )
        {
            stOper = op_import;
            CHECK_HAS_ENOUGH_ADDITIONAL_ARGS(1);
            // coverity[tainted_data]
            pszInputDataset = papszArgv[++iArg];
        }

        else if( EQUAL(papszArgv[iArg],"-l") )
        {
            CHECK_HAS_ENOUGH_ADDITIONAL_ARGS(1);
            // coverity[tainted_data]
            pszInputLayer = papszArgv[++iArg];
        }

        else if( EQUAL(papszArgv[iArg],"connect") )
        {
            CHECK_HAS_ENOUGH_ADDITIONAL_ARGS(3);
            stOper = op_connect;
            // coverity[tainted_data]
            nSrcFID = atoi(papszArgv[++iArg]);
            // coverity[tainted_data]
            nTgtFID = atoi(papszArgv[++iArg]);
            // coverity[tainted_data]
            nConFID = atoi(papszArgv[++iArg]);
        }

        else if( EQUAL(papszArgv[iArg],"-c") )
        {
            CHECK_HAS_ENOUGH_ADDITIONAL_ARGS(1);
            // coverity[tainted_data]
            dfDirCost = CPLAtofM(papszArgv[++iArg]);
        }
        else if( EQUAL(papszArgv[iArg],"-ic") )
        {
            CHECK_HAS_ENOUGH_ADDITIONAL_ARGS(1);
            // coverity[tainted_data]
            dfInvCost = CPLAtofM(papszArgv[++iArg]);
        }
        else if( EQUAL(papszArgv[iArg],"-dir") )
        {
            CHECK_HAS_ENOUGH_ADDITIONAL_ARGS(1);
            // coverity[tainted_data]
            eDir = atoi(papszArgv[++iArg]);
        }

        else if( EQUAL(papszArgv[iArg],"disconnect") )
        {
            CHECK_HAS_ENOUGH_ADDITIONAL_ARGS(3);
            stOper = op_disconnect;
            // coverity[tainted_data]
            nSrcFID = atoi(papszArgv[++iArg]);
            // coverity[tainted_data]
            nTgtFID = atoi(papszArgv[++iArg]);
            // coverity[tainted_data]
            nConFID = atoi(papszArgv[++iArg]);
        }

        else if ( EQUAL(papszArgv[iArg],"autoconnect") )
        {
            CHECK_HAS_ENOUGH_ADDITIONAL_ARGS(1);
            stOper = op_autoconnect;
            // coverity[tainted_data]
            dfTolerance = CPLAtofM(papszArgv[++iArg]);
        }

        else if ( EQUAL(papszArgv[iArg],"rule") )
        {
            CHECK_HAS_ENOUGH_ADDITIONAL_ARGS(1);
            stOper = op_rule;
            // coverity[tainted_data]
            pszRuleStr = papszArgv[++iArg];
        }

        else if ( EQUAL(papszArgv[iArg],"delete") )
        {
            stOper = op_delete;
        }

        else if( EQUAL(papszArgv[iArg],"change") )
        {
            stOper = op_change_st;
        }

        else if( EQUAL(papszArgv[iArg],"-bl") )
        {
            CHECK_HAS_ENOUGH_ADDITIONAL_ARGS(1);
            // coverity[tainted_data]
            anFIDsToBlock.push_back(atoi(papszArgv[++iArg]));
        }

        else if( EQUAL(papszArgv[iArg],"-unbl") )
        {
            CHECK_HAS_ENOUGH_ADDITIONAL_ARGS(1);
            // coverity[tainted_data]
            anFIDsToUnblock.push_back(atoi(papszArgv[++iArg]));
        }

        else if( EQUAL(papszArgv[iArg],"-unblall") )
        {
            bUnblockAll = true;
        }

        else if( papszArgv[iArg][0] == '-' )
        {
            Usage(CPLSPrintf("Unknown option name '%s'", papszArgv[iArg]));
        }

        else if( pszDataSource == nullptr )
            pszDataSource = papszArgv[iArg];
        else
            papszLayers = CSLAddString( papszLayers, papszArgv[iArg] );
    }

// do the work ////////////////////////////////////////////////////////////////

    if(stOper == op_info)
    {
        if(pszDataSource == nullptr)
            Usage("No network dataset provided");

        //TODO for output:
        // stats about graph and blocked features

        // open

        poDS = cpl::down_cast<GNMNetwork*>(static_cast<GDALDataset*>(
            GDALOpenEx( pszDataSource, GDAL_OF_READONLY |
                                    GDAL_OF_GNM, nullptr, nullptr, nullptr )));

        GDALDriver         *poDriver = nullptr;
        if( poDS != nullptr )
            poDriver = poDS->GetDriver();

        if( poDS == nullptr )
        {
            fprintf(stderr, "FAILURE:\nUnable to open datasource `%s'.\n",
                    pszDataSource);
            exit(1);
        }

        if( poDriver == nullptr )
        {
            CPLAssert( false );
            exit(1);
        }

        printf( "INFO: Open of `%s'\n      using driver `%s' successful.\n",
                    pszDataSource, poDriver->GetDescription() );

        // Report projection.
        int nMajor = poDS->GetVersion() / 100;
        printf( "Network version: %d.%d.\n", nMajor,
                                            poDS->GetVersion() - nMajor * 100 );
        const char* pszName = poDS->GetName();
        if(nullptr != pszName)
            printf( "Network name: %s.\n", pszName );
        const char* pszDescript = poDS->GetDescription();
        if(nullptr != pszDescript)
            printf( "Network description: %s.\n", pszDescript );

        char *pszProjection = const_cast<char*>(poDS->GetProjectionRef());
        OGRSpatialReferenceH hSRS = OSRNewSpatialReference(nullptr);
        if( OSRImportFromWkt( hSRS, &pszProjection ) == CE_None )
        {
            char *pszPrettyWkt = nullptr;
            OSRExportToPrettyWkt( hSRS, &pszPrettyWkt, FALSE );

            printf( "Coordinate System is:\n%s\n", pszPrettyWkt );
            CPLFree( pszPrettyWkt );
        }
        else
        {
            printf( "Coordinate System is '%s'\n", pszProjection );
        }
        OSRDestroySpatialReference(hSRS);

        // report layers
        if(poDS->GetLayerCount() > 0)
        {
            printf("\nNetwork\'s layers: \n");
            for (int iLayer = 0; iLayer < poDS->GetLayerCount(); iLayer++)
            {
                OGRLayer *poLayer = poDS->GetLayer(iLayer);

                if (poLayer != nullptr)
                {
                    printf("  %d: %s", iLayer + 1, poLayer->GetName());

                    int nGeomFieldCount = poLayer->GetLayerDefn()->GetGeomFieldCount();
                    if (nGeomFieldCount > 1)
                    {
                        printf(" (");
                        for (int iGeom = 0; iGeom < nGeomFieldCount; iGeom++)
                        {
                            if (iGeom > 0)
                                printf(", ");
                            OGRGeomFieldDefn* poGFldDefn =
                                poLayer->GetLayerDefn()->GetGeomFieldDefn(iGeom);
                            printf("%s", OGRGeometryTypeToName( poGFldDefn->GetType()) );
                        }
                        printf(")");
                    }
                    else if (poLayer->GetGeomType() != wkbUnknown)
                        printf(" (%s)", OGRGeometryTypeToName( poLayer->GetGeomType()) );

                    printf("\n");
                }
            }
        }

        // report rules
        GNMGenericNetwork* poGenericNetwork =
                                         dynamic_cast<GNMGenericNetwork*>(poDS);

        if(nullptr != poGenericNetwork)
        {
            CPLStringList oList(poGenericNetwork->GetRules());
            if(oList.Count() > 0)
            {
                printf("\nNetwork\'s rules: \n");
                for(int iRule = 0; iRule < oList.Count(); ++iRule)
                {
                    printf("  %d: %s\n", iRule + 1, oList[iRule]);
                }
            }
        }
    }
    else if(stOper == op_create)
    {
        const char* pszPath;
        const char* pszNetworkName = CSLFetchNameValue(papszDSCO, GNM_MD_NAME);

        if(pszDataSource == nullptr)
            Usage("No network dataset provided");

        //the DSCO have priority on input keys
        if(nullptr == pszNetworkName)
        {
            pszPath = CPLGetPath(pszDataSource);
            pszNetworkName = CPLGetBasename(pszDataSource);
            papszDSCO = CSLAddNameValue(papszDSCO, GNM_MD_NAME, pszNetworkName);
        }
        else
        {
            pszPath = pszDataSource;
        }

        if( pszNetworkName == nullptr)
            Usage("No dataset name provided");

        const char* pszFinalSRS = CSLFetchNameValue(papszDSCO, GNM_MD_SRS);
        if(nullptr == pszFinalSRS)
        {
            pszFinalSRS = pszSRS;
            papszDSCO = CSLAddNameValue(papszDSCO, GNM_MD_SRS, pszSRS);
        }

        if(nullptr == pszFinalSRS)
            Usage("No spatial reference provided");
        if( pszFormat == nullptr )
            Usage("No output format provided");

        GDALDriver *poDriver = GetGDALDriverManager()->GetDriverByName(pszFormat);
        if( poDriver == nullptr )
        {
            Usage( CPLSPrintf("%s driver not available", pszFormat) );
        }
        else
        {
            char** papszMD = poDriver->GetMetadata();

            if( !CPLFetchBool( papszMD, GDAL_DCAP_GNM, false ) )
                Usage("not a GNM driver");

            poDS = cpl::down_cast<GNMNetwork*>(poDriver->Create( pszPath, 0, 0, 0, GDT_Unknown,
                                                papszDSCO ));

            if (nullptr == poDS)
            {
                fprintf(stderr, "\nFAILURE: Failed to create network in a new dataset at "
                        "%s and with driver %s\n", CPLFormFilename(pszPath,
                        pszNetworkName, nullptr) ,pszFormat);
                nRet = 1;
            }
            else
            {
                if (bQuiet == FALSE)
                    printf("\nNetwork created successfully in a "
                    "new dataset at %s\n", CPLFormFilename(pszPath,
                        pszNetworkName, nullptr));
            }
        }
    }
    else if(stOper == op_import)
    {
        if(pszDataSource == nullptr)
            Usage("No network dataset provided");

        if(pszInputDataset == nullptr)
            Usage("No input dataset name provided");

        // open
        poDS = cpl::down_cast<GNMNetwork*>(static_cast<GDALDataset*>(GDALOpenEx( pszDataSource,
                             GDAL_OF_READONLY | GDAL_OF_GNM, nullptr, nullptr, nullptr )));

        if(nullptr == poDS)
        {
            printf("\nFailed to open network at %s\n",pszDataSource);
            goto exit;
        }

        GDALDataset *poSrcDS = static_cast<GDALDataset*>(GDALOpenEx(pszInputDataset,
                          GDAL_OF_VECTOR | GDAL_OF_READONLY, nullptr, nullptr, nullptr ));
        if(nullptr == poSrcDS)
        {
            fprintf(stderr, "\nFAILURE: Can not open dataset at %s\n",
                            pszInputDataset);

            nRet = 1;
            goto exit;
        }

        OGRLayer *poSrcLayer;
        if (pszInputLayer != nullptr)
            poSrcLayer = poSrcDS->GetLayerByName(pszInputLayer);
        else
            poSrcLayer = poSrcDS->GetLayer(0);

        if (nullptr == poSrcLayer)
        {
            if (pszInputLayer != nullptr)
                fprintf(stderr, "\nFAILURE: Can not open layer %s in %s\n",
                    pszInputLayer,pszInputDataset);
            else
                fprintf(stderr, "\nFAILURE: Can not open layer in %s\n",
                pszInputDataset);

            GDALClose(poSrcDS);

            nRet = 1;
            goto exit;
        }

        OGRLayer * poLayer = poDS->CopyLayer(poSrcLayer, poSrcLayer->GetName());
        if (nullptr == poLayer)
        {
            if (pszInputLayer != nullptr)
                fprintf(stderr, "\nFAILURE: Can not copy layer %s from %s\n",
                    pszInputLayer,pszInputDataset);
            else
                fprintf(stderr, "\nFAILURE: Can not copy layer from %s\n",
                pszInputDataset);
            GDALClose(poSrcDS);

            nRet = 1;
            goto exit;
        }

        if (bQuiet == FALSE)
        {
            if (pszInputLayer != nullptr)
                printf("\nLayer %s successfully copied from %s and added to the network at %s\n",
                pszInputLayer, pszInputDataset, pszDataSource);
            else
                printf("\nLayer successfully copied from %s and added to the network at %s\n",
                pszInputDataset, pszDataSource);
        }

        GDALClose(poSrcDS);
    }
    else if (stOper == op_connect)
    {
        if(pszDataSource == nullptr)
            Usage("No network dataset provided");

        // open
        poDS = cpl::down_cast<GNMNetwork*>(static_cast<GDALDataset*>(GDALOpenEx( pszDataSource,
                             GDAL_OF_UPDATE | GDAL_OF_GNM, nullptr, nullptr, nullptr )));

        if(nullptr == poDS)
        {
            fprintf( stderr, "\nFailed to open network at %s\n", pszDataSource);
            nRet = 1;
            goto exit;
        }

        GNMGenericNetwork* poGenericNetwork =
                                         dynamic_cast<GNMGenericNetwork*>(poDS);

        if(nullptr == poGenericNetwork)
        {
            fprintf( stderr, "\nUnsupported datasource type for this operation\n");
            nRet = 1;
            goto exit;
        }

        if(poGenericNetwork->ConnectFeatures(nSrcFID, nTgtFID, nConFID,
                            dfDirCost, dfInvCost, eDir) != CE_None )
        {
            fprintf( stderr, "Failed to connect features\n" );
            nRet = 1;
            goto exit;
        }

        if (bQuiet == FALSE)
        {
            printf("Features connected successfully\n");
        }
    }
    else if (stOper == op_disconnect)
    {
        if(pszDataSource == nullptr)
            Usage("No network dataset provided");

        // open
        poDS = cpl::down_cast<GNMNetwork*>(static_cast<GDALDataset*>(GDALOpenEx( pszDataSource,
                             GDAL_OF_UPDATE | GDAL_OF_GNM, nullptr, nullptr, nullptr )));

        if(nullptr == poDS)
        {
            fprintf( stderr, "\nFailed to open network at %s\n", pszDataSource);
            nRet = 1;
            goto exit;
        }

        GNMGenericNetwork* poGenericNetwork =
                                         dynamic_cast<GNMGenericNetwork*>(poDS);

        if(nullptr == poGenericNetwork)
        {
            fprintf( stderr, "\nUnsupported datasource type for this operation\n");
            nRet = 1;
            goto exit;
        }

        if(poGenericNetwork->DisconnectFeatures(nSrcFID, nTgtFID, nConFID)
                != CE_None )
        {
            fprintf( stderr, "Failed to disconnect features\n" );
            nRet = 1;
            goto exit;
        }

        if (bQuiet == FALSE)
        {
            printf("Features disconnected successfully\n");
        }
    }
    else if (stOper == op_rule)
    {
        if(pszDataSource == nullptr)
            Usage("No network dataset provided");

        // open
        poDS = cpl::down_cast<GNMNetwork*>(static_cast<GDALDataset*>(GDALOpenEx( pszDataSource,
                             GDAL_OF_UPDATE | GDAL_OF_GNM, nullptr, nullptr, nullptr )));

        if(nullptr == poDS)
        {
            fprintf( stderr, "\nFailed to open network at %s\n", pszDataSource);
            nRet = 1;
            goto exit;
        }

        GNMGenericNetwork* poGenericNetwork =
                                         dynamic_cast<GNMGenericNetwork*>(poDS);

        if(nullptr == poGenericNetwork)
        {
            fprintf( stderr, "\nUnsupported datasource type for this operation\n");
            nRet = 1;
            goto exit;
        }

        if(poGenericNetwork->CreateRule(pszRuleStr) != CE_None )
        {
            fprintf( stderr, "Failed to create rule %s\n", pszRuleStr );
            nRet = 1;
            goto exit;
        }

        if (bQuiet == FALSE)
        {
            printf("Create rule '%s' successfully\n", pszRuleStr);
        }
    }
    else if (stOper == op_autoconnect)
    {
        if(pszDataSource == nullptr)
            Usage("No network dataset provided");

        // open
        poDS = cpl::down_cast<GNMNetwork*>(static_cast<GDALDataset*>(GDALOpenEx( pszDataSource,
                             GDAL_OF_UPDATE | GDAL_OF_GNM, nullptr, nullptr, nullptr )));

        if(nullptr == poDS)
        {
            fprintf( stderr, "\nFailed to open network at %s\n", pszDataSource);
            nRet = 1;
            goto exit;
        }

        GNMGenericNetwork* poGenericNetwork =
                                         dynamic_cast<GNMGenericNetwork*>(poDS);

        if(nullptr == poGenericNetwork)
        {
            fprintf( stderr, "\nUnsupported datasource type for this operation\n");
            nRet = 1;
            goto exit;
        }

        if(CSLCount(papszLayers) == 0 && poDS->GetLayerCount() > 1)
        {
            if(bQuiet == FALSE)
            {
                printf("No layers provided. Use all layers of network:\n");
            }

            for(int i = 0; i < poDS->GetLayerCount(); ++i)
            {
                OGRLayer* poLayer = poDS->GetLayer(i);
                if(bQuiet == FALSE)
                {
                    printf("%d. %s\n", i + 1, poLayer->GetName());
                }
                papszLayers = CSLAddString(papszLayers, poLayer->GetName() );
            }
        }

        if(poGenericNetwork->ConnectPointsByLines(papszLayers, dfTolerance,
                                        dfDirCost, dfInvCost, eDir) != CE_None )
        {
            fprintf( stderr, "Failed to autoconnect features\n" );
            nRet = 1;
            goto exit;
        }

        if (bQuiet == FALSE)
        {
            printf("Features connected successfully\n");
        }
    }
    else if(stOper == op_delete)
    {
        if(pszDataSource == nullptr)
            Usage("No network dataset provided");

        // open
        poDS = cpl::down_cast<GNMNetwork*>(static_cast<GDALDataset*>(GDALOpenEx( pszDataSource,
                             GDAL_OF_UPDATE | GDAL_OF_GNM, nullptr, nullptr, nullptr )));

        if(nullptr == poDS)
        {
            fprintf( stderr, "\nFailed to open network at %s\n", pszDataSource);
            nRet = 1;
            goto exit;
        }

        if( poDS->Delete() != CE_None )
        {
            fprintf( stderr, "Delete failed.\n" );
            nRet = 1;
            goto exit;
        }

        if (bQuiet == FALSE)
        {
            printf("Delete successfully\n");
        }

       /** if hDriver == NULL this code delete everything in folder,
        *  not only GNM files

        GDALDriverH hDriver = NULL;
        if( pszFormat != NULL )
        {
            hDriver = GDALGetDriverByName( pszFormat );
            if( hDriver == NULL )
            {
                fprintf( stderr, "Unable to find driver named '%s'.\n",
                         pszFormat );
                exit( 1 );
            }
        }
        GDALDeleteDataset( hDriver, pszDataSource );
        */
    }
    else if(stOper == op_change_st)
    {
        if(pszDataSource == nullptr)
            Usage("No dataset in input");

        // open
        poDS = cpl::down_cast<GNMNetwork*>(static_cast<GDALDataset*>(GDALOpenEx( pszDataSource,
                             GDAL_OF_UPDATE | GDAL_OF_GNM, nullptr, nullptr, nullptr )));

        if(nullptr == poDS)
        {
            fprintf( stderr, "\nFailed to open network at %s\n", pszDataSource);
            nRet = 1;
            goto exit;
        }

        GNMGenericNetwork* poGenericNetwork =
                                         dynamic_cast<GNMGenericNetwork*>(poDS);

        if(nullptr == poGenericNetwork)
        {
            fprintf( stderr, "\nUnsupported datasource type for this operation\n");
            nRet = 1;
            goto exit;
        }

        if(bUnblockAll)
        {
            if(poGenericNetwork->ChangeAllBlockState(false) != CE_None)
            {
                fprintf( stderr, "\nChange all block state failed\n");
                nRet = 1;
                goto exit;
            }
        }
        else
        {
            size_t i;
            for(i = 0; i < anFIDsToBlock.size(); ++i)
            {
                if(poGenericNetwork->ChangeBlockState(anFIDsToBlock[i], true)
                        != CE_None)
                {
                    fprintf( stderr, "\nChange block state of id "
                                     GNMGFIDFormat " failed\n", anFIDsToBlock[i]);
                    nRet = 1;
                    goto exit;
                }
            }

            for(i = 0; i < anFIDsToUnblock.size(); ++i)
            {
                if(poGenericNetwork->ChangeBlockState(anFIDsToUnblock[i], false)
                        != CE_None)
                {
                    fprintf( stderr, "\nChange block state of id "
                                     GNMGFIDFormat " failed\n", anFIDsToBlock[i]);
                    nRet = 1;
                    goto exit;
                }
            }
        }

        if (bQuiet == FALSE)
        {
            printf("Change block state successfully\n");
        }
    }
    else
    {
        printf("\nNeed an operation. See help what you can do with gnmmanage:\n");
        Usage();
    }

exit:
    CSLDestroy( papszArgv );
    CSLDestroy( papszDSCO );
    CSLDestroy( papszLayers );

    if( poDS != nullptr )
        GDALClose(poDS);

    GDALDestroyDriverManager();

    return nRet;
}
MAIN_END
