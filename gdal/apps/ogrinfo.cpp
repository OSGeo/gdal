/******************************************************************************
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  Simple client for viewing OGR driver data.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 1999, Frank Warmerdam
 * Copyright (c) 2008-2013, Even Rouault <even dot rouault at mines-paris dot org>
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

#include "ogr_api.h"
#include "ogrsf_frmts.h"
#include "ogr_p.h"
#include "cpl_conv.h"
#include "cpl_string.h"
#include "cpl_multiproc.h"
#include "commonutils.h"

#include <set>

CPL_CVSID("$Id$")

bool bReadOnly = false;
bool bVerbose = true;
bool bSuperQuiet = false;
bool bSummaryOnly = false;
GIntBig nFetchFID = OGRNullFID;
char**  papszOptions = NULL;

static void Usage(const char* pszErrorMsg = NULL);

static void ReportOnLayer( OGRLayer *, const char *, const char* pszGeomField,
                           OGRGeometry *,
                           bool bListMDD,
                           bool bShowMetadata,
                           char** papszExtraMDDomains,
                           bool bFeatureCount,
                           bool bExtent );
static void GDALInfoReportMetadata( GDALMajorObjectH hObject,
                                    bool bListMDD,
                                    bool bShowMetadata,
                                    char **papszExtraMDDomains );

/************************************************************************/
/*                             RemoveBOM()                              */
/************************************************************************/

/* Remove potential UTF-8 BOM from data (must be NUL terminated) */
static void RemoveBOM(GByte* pabyData)
{
    if( pabyData[0] == 0xEF && pabyData[1] == 0xBB && pabyData[2] == 0xBF )
    {
        memmove(pabyData, pabyData + 3, strlen((const char*)pabyData + 3) + 1);
    }
}

/************************************************************************/
/*                                main()                                */
/************************************************************************/

#define CHECK_HAS_ENOUGH_ADDITIONAL_ARGS(nExtraArg) \
    do { if (iArg + nExtraArg >= nArgc) \
        Usage(CPLSPrintf("%s option requires %d argument(s)", \
                         papszArgv[iArg], nExtraArg)); } while( false )

int main( int nArgc, char ** papszArgv )

{
    char *pszWHERE = NULL;
    const char  *pszDataSource = NULL;
    char        **papszLayers = NULL;
    OGRGeometry *poSpatialFilter = NULL;
    int nRepeatCount = 1;
    bool bAllLayers = false;
    char  *pszSQLStatement = NULL;
    const char  *pszDialect = NULL;
    int          nRet = 0;
    const char* pszGeomField = NULL;
    char      **papszOpenOptions = NULL;
    char      **papszExtraMDDomains = NULL;
    bool bListMDD = false;
    bool bShowMetadata = true;
    bool bFeatureCount = true;
    bool bExtent = true;
    bool bDatasetGetNextFeature = false;

    /* Check strict compilation and runtime library version as we use C++ API */
    if (! GDAL_CHECK_VERSION(papszArgv[0]))
        exit(1);

    EarlySetConfigOptions(nArgc, papszArgv);

/* -------------------------------------------------------------------- */
/*      Register format(s).                                             */
/* -------------------------------------------------------------------- */
    OGRRegisterAll();

/* -------------------------------------------------------------------- */
/*      Processing command line arguments.                              */
/* -------------------------------------------------------------------- */
    nArgc = OGRGeneralCmdLineProcessor( nArgc, &papszArgv, 0 );

    if( nArgc < 1 )
        exit( -nArgc );

    for( int iArg = 1; iArg < nArgc; iArg++ )
    {
        if( EQUAL(papszArgv[iArg], "--utility_version") )
        {
            printf("%s was compiled against GDAL %s and is running against GDAL %s\n",
                   papszArgv[0], GDAL_RELEASE_NAME, GDALVersionInfo("RELEASE_NAME"));
            CSLDestroy( papszArgv );
            return 0;
        }
        else if( EQUAL(papszArgv[iArg],"--help") )
            Usage();
       else if( EQUAL(papszArgv[iArg],"-ro") )
            bReadOnly = true;
        else if( EQUAL(papszArgv[iArg],"-q") || EQUAL(papszArgv[iArg],"-quiet"))
            bVerbose = false;
        else if( EQUAL(papszArgv[iArg],"-qq") )
        {
            /* Undocumented: mainly only useful for AFL testing */
            bVerbose = false;
            bSuperQuiet = true;
        }
        else if( EQUAL(papszArgv[iArg],"-fid") )
        {
            CHECK_HAS_ENOUGH_ADDITIONAL_ARGS(1);
            nFetchFID = CPLAtoGIntBig(papszArgv[++iArg]);
        }
        else if( EQUAL(papszArgv[iArg],"-spat") )
        {
            CHECK_HAS_ENOUGH_ADDITIONAL_ARGS(4);
            OGRLinearRing  oRing;

            oRing.addPoint( CPLAtof(papszArgv[iArg+1]), CPLAtof(papszArgv[iArg+2]) );
            oRing.addPoint( CPLAtof(papszArgv[iArg+1]), CPLAtof(papszArgv[iArg+4]) );
            oRing.addPoint( CPLAtof(papszArgv[iArg+3]), CPLAtof(papszArgv[iArg+4]) );
            oRing.addPoint( CPLAtof(papszArgv[iArg+3]), CPLAtof(papszArgv[iArg+2]) );
            oRing.addPoint( CPLAtof(papszArgv[iArg+1]), CPLAtof(papszArgv[iArg+2]) );

            poSpatialFilter = new OGRPolygon();
            ((OGRPolygon *) poSpatialFilter)->addRing( &oRing );
            iArg += 4;
        }
        else if( EQUAL(papszArgv[iArg],"-geomfield") )
        {
            CHECK_HAS_ENOUGH_ADDITIONAL_ARGS(1);
            pszGeomField = papszArgv[++iArg];
        }
        else if( EQUAL(papszArgv[iArg],"-where") )
        {
            CHECK_HAS_ENOUGH_ADDITIONAL_ARGS(1);
            iArg++;
            CPLFree(pszWHERE);
            GByte* pabyRet = NULL;
            if( papszArgv[iArg][0] == '@' &&
                VSIIngestFile( NULL, papszArgv[iArg] + 1, &pabyRet, NULL, 1024*1024) )
            {
                RemoveBOM(pabyRet);
                pszWHERE = (char*)pabyRet;
            }
            else
            {
                pszWHERE = CPLStrdup(papszArgv[iArg]);
            }
        }
        else if( EQUAL(papszArgv[iArg],"-sql") )
        {
            CHECK_HAS_ENOUGH_ADDITIONAL_ARGS(1);
            iArg++;
            CPLFree(pszSQLStatement);
            GByte* pabyRet = NULL;
            if( papszArgv[iArg][0] == '@' &&
                VSIIngestFile( NULL, papszArgv[iArg] + 1, &pabyRet, NULL, 1024*1024) )
            {
                RemoveBOM(pabyRet);
                pszSQLStatement = (char*)pabyRet;
            }
            else
            {
                pszSQLStatement = CPLStrdup(papszArgv[iArg]);
            }
        }
        else if( EQUAL(papszArgv[iArg],"-dialect") )
        {
            CHECK_HAS_ENOUGH_ADDITIONAL_ARGS(1);
            pszDialect = papszArgv[++iArg];
        }
        else if( EQUAL(papszArgv[iArg],"-rc") )
        {
            CHECK_HAS_ENOUGH_ADDITIONAL_ARGS(1);
            nRepeatCount = atoi(papszArgv[++iArg]);
        }
        else if( EQUAL(papszArgv[iArg],"-al") )
        {
            bAllLayers = true;
        }
        else if( EQUAL(papszArgv[iArg],"-so")
                 || EQUAL(papszArgv[iArg],"-summary")  )
        {
            bSummaryOnly = true;
        }
        else if( STARTS_WITH_CI(papszArgv[iArg], "-fields=") )
        {
            char* pszTemp =
                static_cast<char *>(CPLMalloc(32 + strlen(papszArgv[iArg])));
            snprintf(pszTemp,
                    32 + strlen(papszArgv[iArg]),
                    "DISPLAY_FIELDS=%s", papszArgv[iArg] + strlen("-fields="));
            papszOptions = CSLAddString(papszOptions, pszTemp);
            CPLFree(pszTemp);
        }
        else if( STARTS_WITH_CI(papszArgv[iArg], "-geom=") )
        {
            char* pszTemp =
                static_cast<char *>(CPLMalloc(32 + strlen(papszArgv[iArg])));
            snprintf(pszTemp,
                    32 + strlen(papszArgv[iArg]),
                    "DISPLAY_GEOMETRY=%s", papszArgv[iArg] + strlen("-geom="));
            papszOptions = CSLAddString(papszOptions, pszTemp);
            CPLFree(pszTemp);
        }
        else if( EQUAL(papszArgv[iArg], "-oo") )
        {
            CHECK_HAS_ENOUGH_ADDITIONAL_ARGS(1);
            papszOpenOptions = CSLAddString( papszOpenOptions,
                                                papszArgv[++iArg] );
        }
        else if( EQUAL(papszArgv[iArg], "-nomd") )
            bShowMetadata = false;
        else if( EQUAL(papszArgv[iArg], "-listmdd") )
            bListMDD = true;
        else if( EQUAL(papszArgv[iArg], "-mdd") )
        {
            CHECK_HAS_ENOUGH_ADDITIONAL_ARGS(1);
            papszExtraMDDomains = CSLAddString( papszExtraMDDomains,
                                                papszArgv[++iArg] );
        }
        else if( EQUAL(papszArgv[iArg], "-nocount") )
            bFeatureCount = false;
        else if( EQUAL(papszArgv[iArg], "-noextent") )
            bExtent = false;
        else if( EQUAL(papszArgv[iArg],"-rl"))
            bDatasetGetNextFeature = true;
        else if( papszArgv[iArg][0] == '-' )
        {
            Usage(CPLSPrintf("Unknown option name '%s'", papszArgv[iArg]));
        }
        else if( pszDataSource == NULL )
            pszDataSource = papszArgv[iArg];
        else
        {
            papszLayers = CSLAddString( papszLayers, papszArgv[iArg] );
            bAllLayers = false;
        }
    }

    if( pszDataSource == NULL )
        Usage("No datasource specified.");

    if( pszDialect != NULL && pszWHERE != NULL && pszSQLStatement == NULL )
        printf("Warning: -dialect is ignored with -where. Use -sql instead");

    if( bDatasetGetNextFeature && pszSQLStatement )
    {
        Usage("-rl is incompatible with -sql");
    }

#ifdef __AFL_HAVE_MANUAL_CONTROL
    while (__AFL_LOOP(1000)) {
#endif
/* -------------------------------------------------------------------- */
/*      Open data source.                                               */
/* -------------------------------------------------------------------- */
    GDALDataset *poDS
        = (GDALDataset*) GDALOpenEx( pszDataSource,
            (!bReadOnly ? GDAL_OF_UPDATE : GDAL_OF_READONLY) | GDAL_OF_VECTOR,
            NULL, papszOpenOptions, NULL );
    if( poDS == NULL && !bReadOnly )
    {
        poDS = (GDALDataset*) GDALOpenEx( pszDataSource,
            GDAL_OF_READONLY | GDAL_OF_VECTOR, NULL, papszOpenOptions, NULL );
        if( poDS != NULL && bVerbose )
        {
            printf( "Had to open data source read-only.\n" );
            bReadOnly = true;
        }
    }

    GDALDriver *poDriver = NULL;
    if( poDS != NULL )
        poDriver = poDS->GetDriver();

/* -------------------------------------------------------------------- */
/*      Report failure                                                  */
/* -------------------------------------------------------------------- */
    if( poDS == NULL )
    {
        printf( "FAILURE:\n"
                "Unable to open datasource `%s' with the following drivers.\n",
                pszDataSource );
#ifdef __AFL_HAVE_MANUAL_CONTROL
        continue;
#else
        OGRSFDriverRegistrar    *poR = OGRSFDriverRegistrar::GetRegistrar();
        for( int iDriver = 0; iDriver < poR->GetDriverCount(); iDriver++ )
        {
            printf( "  -> %s\n", poR->GetDriver(iDriver)->GetDescription() );
        }

        nRet = 1;
        goto end;
#endif
    }

    CPLAssert( poDriver != NULL);

/* -------------------------------------------------------------------- */
/*      Some information messages.                                      */
/* -------------------------------------------------------------------- */
    if( bVerbose )
        printf( "INFO: Open of `%s'\n"
                "      using driver `%s' successful.\n",
                pszDataSource, poDriver->GetDescription() );

    if( bVerbose && !EQUAL(pszDataSource,poDS->GetDescription()) )
    {
        printf( "INFO: Internal data source name `%s'\n"
                "      different from user name `%s'.\n",
                poDS->GetDescription(), pszDataSource );
    }

    GDALInfoReportMetadata( (GDALMajorObjectH)poDS,
                            bListMDD,
                            bShowMetadata,
                            papszExtraMDDomains );

    if( bDatasetGetNextFeature )
    {
        nRepeatCount = 0;  // skip layer reporting.

/* -------------------------------------------------------------------- */
/*      Set filters if provided.                                        */
/* -------------------------------------------------------------------- */
        if( pszWHERE != NULL || poSpatialFilter != NULL )
        {
            for( int iLayer = 0; iLayer < poDS->GetLayerCount(); iLayer++ )
            {
                OGRLayer        *poLayer = poDS->GetLayer(iLayer);

                if( poLayer == NULL )
                {
                    printf( "FAILURE: Couldn't fetch advertised layer %d!\n",
                            iLayer );
                    exit( 1 );
                }

                if( pszWHERE != NULL )
                {
                    if (poLayer->SetAttributeFilter( pszWHERE ) != OGRERR_NONE )
                    {
                        printf( "WARNING: SetAttributeFilter(%s) failed on layer %s.\n",
                                pszWHERE, poLayer->GetName() );
                    }
                }

                if( poSpatialFilter != NULL )
                {
                    if( pszGeomField != NULL )
                    {
                        OGRFeatureDefn      *poDefn = poLayer->GetLayerDefn();
                        int iGeomField = poDefn->GetGeomFieldIndex(pszGeomField);
                        if( iGeomField >= 0 )
                            poLayer->SetSpatialFilter( iGeomField, poSpatialFilter );
                        else
                            printf("WARNING: Cannot find geometry field %s.\n",
                                pszGeomField);
                    }
                    else
                        poLayer->SetSpatialFilter( poSpatialFilter );
                }
            }
        }

        std::set<OGRLayer*> oSetLayers;
        while( true )
        {
            OGRLayer* poLayer = NULL;
            OGRFeature* poFeature = poDS->GetNextFeature( &poLayer, NULL,
                                                          NULL, NULL );
            if( poFeature == NULL )
                break;
            if( papszLayers == NULL || poLayer == NULL ||
                CSLFindString(papszLayers, poLayer->GetName()) >= 0 )
            {
                if( bVerbose && poLayer != NULL &&
                    oSetLayers.find(poLayer) == oSetLayers.end() )
                {
                    oSetLayers.insert(poLayer);
                    const bool bSummaryOnlyBackup = bSummaryOnly;
                    bSummaryOnly = true;
                    ReportOnLayer( poLayer, NULL, NULL, NULL,
                                   bListMDD, bShowMetadata,
                                   papszExtraMDDomains,
                                   bFeatureCount,
                                   bExtent );
                    bSummaryOnly = bSummaryOnlyBackup;
                }
                if( !bSuperQuiet && !bSummaryOnly )
                    poFeature->DumpReadable( NULL, papszOptions );
            }
            OGRFeature::DestroyFeature( poFeature );
        }
    }

/* -------------------------------------------------------------------- */
/*      Special case for -sql clause.  No source layers required.       */
/* -------------------------------------------------------------------- */
    else if( pszSQLStatement != NULL )
    {
        nRepeatCount = 0;  // skip layer reporting.

        if( CSLCount(papszLayers) > 0 )
            printf( "layer names ignored in combination with -sql.\n" );

        OGRLayer *poResultSet
            = poDS->ExecuteSQL( pszSQLStatement,
                                (pszGeomField == NULL) ? poSpatialFilter: NULL,
                                pszDialect );

        if( poResultSet != NULL )
        {
            if( pszWHERE != NULL )
            {
                if (poResultSet->SetAttributeFilter( pszWHERE ) != OGRERR_NONE )
                {
                    printf( "FAILURE: SetAttributeFilter(%s) failed.\n", pszWHERE );
                    exit(1);
                }
            }

            if( pszGeomField != NULL )
                ReportOnLayer( poResultSet, NULL, pszGeomField, poSpatialFilter,
                               bListMDD, bShowMetadata, papszExtraMDDomains,
                               bFeatureCount, bExtent );
            else
                ReportOnLayer( poResultSet, NULL, NULL, NULL,
                               bListMDD, bShowMetadata, papszExtraMDDomains,
                               bFeatureCount, bExtent );
            poDS->ReleaseResultSet( poResultSet );
        }
    }

    /* coverity[tainted_data] */
    for( int iRepeat = 0; iRepeat < nRepeatCount; iRepeat++ )
    {
        if ( papszLayers == NULL || *papszLayers == NULL )
        {
            if( iRepeat == 0 )
                CPLDebug( "OGR", "GetLayerCount() = %d\n", poDS->GetLayerCount() );

/* -------------------------------------------------------------------- */
/*      Process each data source layer.                                 */
/* -------------------------------------------------------------------- */
            for( int iLayer = 0; iLayer < poDS->GetLayerCount(); iLayer++ )
            {
                OGRLayer        *poLayer = poDS->GetLayer(iLayer);

                if( poLayer == NULL )
                {
                    printf( "FAILURE: Couldn't fetch advertised layer %d!\n",
                            iLayer );
                    exit( 1 );
                }

                if (!bAllLayers)
                {
                    printf( "%d: %s",
                            iLayer+1,
                            poLayer->GetName() );

                    int nGeomFieldCount =
                        poLayer->GetLayerDefn()->GetGeomFieldCount();
                    if( nGeomFieldCount > 1 )
                    {
                        printf( " (");
                        for(int iGeom = 0;iGeom < nGeomFieldCount; iGeom ++ )
                        {
                            if( iGeom > 0 )
                                printf(", ");
                            OGRGeomFieldDefn* poGFldDefn =
                                poLayer->GetLayerDefn()->GetGeomFieldDefn(iGeom);
                            printf( "%s",
                                OGRGeometryTypeToName(
                                    poGFldDefn->GetType() ) );
                        }
                        printf( ")");
                    }
                    else if( poLayer->GetGeomType() != wkbUnknown )
                        printf( " (%s)",
                                OGRGeometryTypeToName(
                                    poLayer->GetGeomType() ) );

                    printf( "\n" );
                }
                else
                {
                    if( iRepeat != 0 )
                        poLayer->ResetReading();

                    ReportOnLayer( poLayer, pszWHERE, pszGeomField, poSpatialFilter,
                                   bListMDD, bShowMetadata, papszExtraMDDomains,
                                   bFeatureCount, bExtent );
                }
            }
        }
        else
        {
/* -------------------------------------------------------------------- */
/*      Process specified data source layers.                           */
/* -------------------------------------------------------------------- */

            for( char** papszIter = papszLayers;
                 *papszIter != NULL;
                 ++papszIter )
            {
                OGRLayer        *poLayer = poDS->GetLayerByName(*papszIter);

                if( poLayer == NULL )
                {
                    printf( "FAILURE: Couldn't fetch requested layer %s!\n",
                            *papszIter );
                    exit( 1 );
                }

                if( iRepeat != 0 )
                    poLayer->ResetReading();

                ReportOnLayer( poLayer, pszWHERE, pszGeomField, poSpatialFilter,
                               bListMDD, bShowMetadata, papszExtraMDDomains,
                               bFeatureCount, bExtent );
            }
        }
    }

/* -------------------------------------------------------------------- */
/*      Close down.                                                     */
/* -------------------------------------------------------------------- */
    GDALClose(poDS);

#ifdef __AFL_HAVE_MANUAL_CONTROL
    }
#else
end:
#endif

    CSLDestroy( papszArgv );
    CSLDestroy( papszLayers );
    CSLDestroy( papszOptions );
    CSLDestroy( papszOpenOptions );
    CSLDestroy( papszExtraMDDomains );
    if (poSpatialFilter)
        OGRGeometryFactory::destroyGeometry( poSpatialFilter );
    CPLFree(pszSQLStatement);
    CPLFree(pszWHERE);

    OGRCleanupAll();

    return nRet;
}

/************************************************************************/
/*                               Usage()                                */
/************************************************************************/

static void Usage(const char* pszErrorMsg)

{
    printf( "Usage: ogrinfo [--help-general] [-ro] [-q] [-where restricted_where|@filename]\n"
            "               [-spat xmin ymin xmax ymax] [-geomfield field] [-fid fid]\n"
            "               [-sql statement|@filename] [-dialect sql_dialect] [-al] [-rl] [-so] [-fields={YES/NO}]\n"
            "               [-geom={YES/NO/SUMMARY}] [[-oo NAME=VALUE] ...]\n"
            "               [-nomd] [-listmdd] [-mdd domain|`all`]*\n"
            "               [-nocount] [-noextent]\n"
            "               datasource_name [layer [layer ...]]\n");

    if( pszErrorMsg != NULL )
        fprintf(stderr, "\nFAILURE: %s\n", pszErrorMsg);

    exit( 1 );
}

/************************************************************************/
/*                           ReportOnLayer()                            */
/************************************************************************/

static void ReportOnLayer( OGRLayer * poLayer, const char *pszWHERE,
                           const char* pszGeomField,
                           OGRGeometry *poSpatialFilter,
                           bool bListMDD,
                           bool bShowMetadata,
                           char** papszExtraMDDomains,
                           bool bFeatureCount,
                           bool bExtent )

{
    OGRFeatureDefn      *poDefn = poLayer->GetLayerDefn();

/* -------------------------------------------------------------------- */
/*      Set filters if provided.                                        */
/* -------------------------------------------------------------------- */
    if( pszWHERE != NULL )
    {
        if (poLayer->SetAttributeFilter( pszWHERE ) != OGRERR_NONE )
        {
            printf( "FAILURE: SetAttributeFilter(%s) failed.\n", pszWHERE );
            exit(1);
        }
    }

    if( poSpatialFilter != NULL )
    {
        if( pszGeomField != NULL )
        {
            int iGeomField = poDefn->GetGeomFieldIndex(pszGeomField);
            if( iGeomField >= 0 )
                poLayer->SetSpatialFilter( iGeomField, poSpatialFilter );
            else
                printf("WARNING: Cannot find geometry field %s.\n",
                       pszGeomField);
        }
        else
            poLayer->SetSpatialFilter( poSpatialFilter );
    }

/* -------------------------------------------------------------------- */
/*      Report various overall information.                             */
/* -------------------------------------------------------------------- */
    if( !bSuperQuiet )
    {
        printf( "\n" );

        printf( "Layer name: %s\n", poLayer->GetName() );
    }

    GDALInfoReportMetadata( (GDALMajorObjectH)poLayer,
                            bListMDD,
                            bShowMetadata,
                            papszExtraMDDomains );

    if( bVerbose )
    {
        int nGeomFieldCount =
            poLayer->GetLayerDefn()->GetGeomFieldCount();
        if( nGeomFieldCount > 1 )
        {
            for(int iGeom = 0;iGeom < nGeomFieldCount; iGeom ++ )
            {
                OGRGeomFieldDefn* poGFldDefn =
                    poLayer->GetLayerDefn()->GetGeomFieldDefn(iGeom);
                printf( "Geometry (%s): %s\n", poGFldDefn->GetNameRef(),
                    OGRGeometryTypeToName( poGFldDefn->GetType() ) );
            }
        }
        else
        {
            printf( "Geometry: %s\n",
                    OGRGeometryTypeToName( poLayer->GetGeomType() ) );
        }

        if( bFeatureCount )
            printf( "Feature Count: " CPL_FRMT_GIB "\n", poLayer->GetFeatureCount() );

        OGREnvelope oExt;
        if( bExtent && nGeomFieldCount > 1 )
        {
            for(int iGeom = 0;iGeom < nGeomFieldCount; iGeom ++ )
            {
                if (poLayer->GetExtent(iGeom, &oExt, TRUE) == OGRERR_NONE)
                {
                    OGRGeomFieldDefn* poGFldDefn =
                        poLayer->GetLayerDefn()->GetGeomFieldDefn(iGeom);
                    CPLprintf("Extent (%s): (%f, %f) - (%f, %f)\n",
                           poGFldDefn->GetNameRef(),
                           oExt.MinX, oExt.MinY, oExt.MaxX, oExt.MaxY);
                }
            }
        }
        else if ( bExtent && poLayer->GetExtent(&oExt, TRUE) == OGRERR_NONE)
        {
            CPLprintf("Extent: (%f, %f) - (%f, %f)\n",
                   oExt.MinX, oExt.MinY, oExt.MaxX, oExt.MaxY);
        }

        char *pszWKT = NULL;

        if( nGeomFieldCount > 1 )
        {
            for(int iGeom = 0;iGeom < nGeomFieldCount; iGeom ++ )
            {
                OGRGeomFieldDefn* poGFldDefn =
                    poLayer->GetLayerDefn()->GetGeomFieldDefn(iGeom);
                OGRSpatialReference* poSRS = poGFldDefn->GetSpatialRef();
                if( poSRS == NULL )
                    pszWKT = CPLStrdup( "(unknown)" );
                else
                {
                    poSRS->exportToPrettyWkt( &pszWKT );
                }

                printf( "SRS WKT (%s):\n%s\n",
                        poGFldDefn->GetNameRef(), pszWKT );
                CPLFree( pszWKT );
            }
        }
        else
        {
            if( poLayer->GetSpatialRef() == NULL )
                pszWKT = CPLStrdup( "(unknown)" );
            else
            {
                poLayer->GetSpatialRef()->exportToPrettyWkt( &pszWKT );
            }

            printf( "Layer SRS WKT:\n%s\n", pszWKT );
            CPLFree( pszWKT );
        }

        if( strlen(poLayer->GetFIDColumn()) > 0 )
            printf( "FID Column = %s\n",
                    poLayer->GetFIDColumn() );

        for(int iGeom = 0;iGeom < nGeomFieldCount; iGeom ++ )
        {
            OGRGeomFieldDefn* poGFldDefn =
                poLayer->GetLayerDefn()->GetGeomFieldDefn(iGeom);
            if( nGeomFieldCount == 1 &&
                EQUAL(poGFldDefn->GetNameRef(), "")  && poGFldDefn->IsNullable() )
                break;
            printf( "Geometry Column ");
            if( nGeomFieldCount > 1 )
                printf("%d ", iGeom + 1);
            if( !poGFldDefn->IsNullable() )
                printf("NOT NULL ");
            printf("= %s\n", poGFldDefn->GetNameRef() );
        }

        for( int iAttr = 0; iAttr < poDefn->GetFieldCount(); iAttr++ )
        {
            OGRFieldDefn    *poField = poDefn->GetFieldDefn( iAttr );
            const char* pszType = (poField->GetSubType() != OFSTNone) ?
                CPLSPrintf("%s(%s)",
                           poField->GetFieldTypeName( poField->GetType() ),
                           poField->GetFieldSubTypeName(poField->GetSubType())) :
                poField->GetFieldTypeName( poField->GetType() );
            printf( "%s: %s (%d.%d)",
                    poField->GetNameRef(),
                    pszType,
                    poField->GetWidth(),
                    poField->GetPrecision() );
            if( !poField->IsNullable() )
                printf(" NOT NULL");
            if( poField->GetDefault() != NULL )
                printf(" DEFAULT %s", poField->GetDefault() );
            printf( "\n" );
        }
    }

/* -------------------------------------------------------------------- */
/*      Read, and dump features.                                        */
/* -------------------------------------------------------------------- */
    OGRFeature  *poFeature = NULL;

    if( nFetchFID == OGRNullFID && !bSummaryOnly )
    {
        while( (poFeature = poLayer->GetNextFeature()) != NULL )
        {
            if( !bSuperQuiet )
                poFeature->DumpReadable( NULL, papszOptions );
            OGRFeature::DestroyFeature( poFeature );
        }
    }
    else if( nFetchFID != OGRNullFID )
    {
        poFeature = poLayer->GetFeature( nFetchFID );
        if( poFeature == NULL )
        {
            printf( "Unable to locate feature id " CPL_FRMT_GIB " on this layer.\n",
                    nFetchFID );
        }
        else
        {
            poFeature->DumpReadable( NULL, papszOptions );
            OGRFeature::DestroyFeature( poFeature );
        }
    }
}

/************************************************************************/
/*                       GDALInfoPrintMetadata()                        */
/************************************************************************/
static void GDALInfoPrintMetadata( GDALMajorObjectH hObject,
                                   const char *pszDomain,
                                   const char *pszDisplayedname,
                                   const char *pszIndent)
{
    bool bIsxml = false;

    if (pszDomain != NULL && STARTS_WITH_CI(pszDomain, "xml:"))
        bIsxml = true;

    char **papszMetadata = GDALGetMetadata( hObject, pszDomain );
    if( CSLCount(papszMetadata) > 0 )
    {
        printf( "%s%s:\n", pszIndent, pszDisplayedname );
        for( int i = 0; papszMetadata[i] != NULL; i++ )
        {
            if (bIsxml)
                printf( "%s%s\n", pszIndent, papszMetadata[i] );
            else
                printf( "%s  %s\n", pszIndent, papszMetadata[i] );
        }
    }
}

/************************************************************************/
/*                       GDALInfoReportMetadata()                       */
/************************************************************************/
static void GDALInfoReportMetadata( GDALMajorObjectH hObject,
                                    bool bListMDD,
                                    bool bShowMetadata,
                                    char **papszExtraMDDomains )
{
    const char* pszIndent = "";

    /* -------------------------------------------------------------------- */
    /*      Report list of Metadata domains                                 */
    /* -------------------------------------------------------------------- */
    if( bListMDD )
    {
        char** papszMDDList = GDALGetMetadataDomainList( hObject );
        char** papszIter = papszMDDList;

        if( papszMDDList != NULL )
            printf( "%sMetadata domains:\n", pszIndent );
        while( papszIter != NULL && *papszIter != NULL )
        {
            if( EQUAL(*papszIter, "") )
                printf( "%s  (default)\n", pszIndent);
            else
                printf( "%s  %s\n", pszIndent, *papszIter );
            papszIter ++;
        }
        CSLDestroy(papszMDDList);
    }

    if (!bShowMetadata)
        return;

    /* -------------------------------------------------------------------- */
    /*      Report default Metadata domain.                                 */
    /* -------------------------------------------------------------------- */
    GDALInfoPrintMetadata( hObject, NULL, "Metadata", pszIndent );

    /* -------------------------------------------------------------------- */
    /*      Report extra Metadata domains                                   */
    /* -------------------------------------------------------------------- */
    if (papszExtraMDDomains != NULL) {
        char **papszExtraMDDomainsExpanded = NULL;

        if( EQUAL(papszExtraMDDomains[0], "all") &&
            papszExtraMDDomains[1] == NULL )
        {
            char** papszMDDList = GDALGetMetadataDomainList( hObject );
            char** papszIter = papszMDDList;

            while( papszIter != NULL && *papszIter != NULL )
            {
                if( !EQUAL(*papszIter, "") )
                {
                    papszExtraMDDomainsExpanded = CSLAddString(papszExtraMDDomainsExpanded, *papszIter);
                }
                papszIter ++;
            }
            CSLDestroy(papszMDDList);
        }
        else
        {
            papszExtraMDDomainsExpanded = CSLDuplicate(papszExtraMDDomains);
        }

        for( int iMDD = 0; papszExtraMDDomainsExpanded != NULL &&
                           papszExtraMDDomainsExpanded[iMDD] != NULL; iMDD++ )
        {
            char pszDisplayedname[256];
            snprintf(pszDisplayedname, 256, "Metadata (%s)", papszExtraMDDomainsExpanded[iMDD]);
            GDALInfoPrintMetadata( hObject, papszExtraMDDomainsExpanded[iMDD], pszDisplayedname, pszIndent );
        }

        CSLDestroy(papszExtraMDDomainsExpanded);
    }
}
