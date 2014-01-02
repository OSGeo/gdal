/******************************************************************************
 * $Id$
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  Formal test harnass for OGRLayer implementations.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 1999, Frank Warmerdam
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

#include "ogrsf_frmts.h"
#include "cpl_conv.h"
#include "cpl_multiproc.h"
#include "ogr_api.h"
#include "ogr_p.h"
#include "commonutils.h"

CPL_CVSID("$Id$");

int     bReadOnly = FALSE;
int     bVerbose = TRUE;
const char  *pszDataSource = NULL;
char** papszLayers = NULL;
const char  *pszSQLStatement = NULL;
const char  *pszDialect = NULL;
int nLoops = 1;

typedef struct
{
    void* hThread;
    int bRet;
} ThreadContext;

static void Usage();
static void ThreadFunction( void* user_data );
static void ThreadFunctionInternal( ThreadContext* psContext );
static int TestOGRLayer( OGRDataSource * poDS, OGRLayer * poLayer, int bIsSQLLayer );
static int TestInterleavedReading( const char* pszDataSource, char** papszLayers );
static int TestDSErrorConditions( OGRDataSource * poDS );


/************************************************************************/
/*                                main()                                */
/************************************************************************/

int main( int nArgc, char ** papszArgv )

{
    int bRet = TRUE;
    int nThreads = 1;

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

/* -------------------------------------------------------------------- */
/*      Processing command line arguments.                              */
/* -------------------------------------------------------------------- */
    for( int iArg = 1; iArg < nArgc; iArg++ )
    {
        if( EQUAL(papszArgv[iArg], "--utility_version") )
        {
            printf("%s was compiled against GDAL %s and is running against GDAL %s\n",
                   papszArgv[0], GDAL_RELEASE_NAME, GDALVersionInfo("RELEASE_NAME"));
            return 0;
        }
        else if( EQUAL(papszArgv[iArg],"-ro") )
            bReadOnly = TRUE;
        else if( EQUAL(papszArgv[iArg],"-q") || EQUAL(papszArgv[iArg],"-quiet"))
            bVerbose = FALSE;
        else if( EQUAL(papszArgv[iArg],"-sql") && iArg + 1 < nArgc)
            pszSQLStatement = papszArgv[++iArg];
        else if( EQUAL(papszArgv[iArg],"-dialect") && papszArgv[iArg+1] != NULL )
        {
            pszDialect = papszArgv[++iArg];
        }
        else if( EQUAL(papszArgv[iArg],"-threads") && iArg + 1 < nArgc)
        {
            nThreads = atoi(papszArgv[++iArg]);
        }
        else if( EQUAL(papszArgv[iArg],"-loops") && iArg + 1 < nArgc)
        {
            nLoops = atoi(papszArgv[++iArg]);
        }
        else if( papszArgv[iArg][0] == '-' )
        {
            Usage();
        }
        else if (pszDataSource == NULL)
            pszDataSource = papszArgv[iArg];
        else
            papszLayers = CSLAddString(papszLayers, papszArgv[iArg]);
    }

    if( pszDataSource == NULL )
        Usage();
    if( nThreads > 1 && !bReadOnly )
    {
        fprintf(stderr, "-theads must be used with -ro option.\n");
        exit(1);
    }

    if( nThreads == 1 )
    {
        ThreadContext sContext;
        ThreadFunction(&sContext);
        bRet = sContext.bRet;
    }
    else if( nThreads > 1 )
    {
        int i;
        ThreadContext* pasContext = new ThreadContext[nThreads];
        for(i = 0; i < nThreads; i ++ )
        {
            pasContext[i].hThread = CPLCreateJoinableThread(
                ThreadFunction, &(pasContext[i]));
        }
        for(i = 0; i < nThreads; i ++ )
        {
            CPLJoinThread(pasContext[i].hThread);
            bRet &= pasContext[i].bRet;
        }
        delete[] pasContext;
    }

    OGRCleanupAll();

    CSLDestroy(papszLayers);
    CSLDestroy(papszArgv);
    
#ifdef DBMALLOC
    malloc_dump(1);
#endif
    
    return (bRet) ? 0 : 1;
}

/************************************************************************/
/*                        ThreadFunction()                              */
/************************************************************************/

static void ThreadFunction( void* user_data )

{
    ThreadContext* psContext = (ThreadContext* )user_data;
    psContext->bRet = TRUE;
    for( int iLoop = 0; psContext->bRet && iLoop < nLoops; iLoop ++ )
    {
        ThreadFunctionInternal(psContext);
    }
}

/************************************************************************/
/*                     ThreadFunctionInternal()                         */
/************************************************************************/

static void ThreadFunctionInternal( ThreadContext* psContext )

{
    int bRet = TRUE;

/* -------------------------------------------------------------------- */
/*      Open data source.                                               */
/* -------------------------------------------------------------------- */
    OGRDataSource       *poDS;
    OGRSFDriver         *poDriver;

    poDS = OGRSFDriverRegistrar::Open( pszDataSource, !bReadOnly, &poDriver );
    if( poDS == NULL && !bReadOnly )
    {
        poDS = OGRSFDriverRegistrar::Open( pszDataSource, FALSE, &poDriver );
        if( poDS != NULL && bVerbose )
        {
            printf( "Had to open data source read-only.\n" );
            bReadOnly = TRUE;
        }
    }

/* -------------------------------------------------------------------- */
/*      Report failure                                                  */
/* -------------------------------------------------------------------- */
    if( poDS == NULL )
    {
        OGRSFDriverRegistrar    *poR = OGRSFDriverRegistrar::GetRegistrar();
        
        printf( "FAILURE:\n"
                "Unable to open datasource `%s' with the following drivers.\n",
                pszDataSource );

        for( int iDriver = 0; iDriver < poR->GetDriverCount(); iDriver++ )
        {
            printf( "  -> %s\n", poR->GetDriver(iDriver)->GetName() );
        }

        psContext->bRet = FALSE;
        return;
    }

/* -------------------------------------------------------------------- */
/*      Some information messages.                                      */
/* -------------------------------------------------------------------- */
    if( bVerbose )
        printf( "INFO: Open of `%s' using driver `%s' successful.\n",
                pszDataSource, poDriver->GetName() );

    if( bVerbose && !EQUAL(pszDataSource,poDS->GetName()) )
    {
        printf( "INFO: Internal data source name `%s'\n"
                "      different from user name `%s'.\n",
                poDS->GetName(), pszDataSource );
    }
    
/* -------------------------------------------------------------------- */
/*      Process optionnal SQL request.                                  */
/* -------------------------------------------------------------------- */
    if (pszSQLStatement != NULL)
    {
        OGRLayer  *poResultSet = poDS->ExecuteSQL(pszSQLStatement, NULL, pszDialect);
        if (poResultSet == NULL)
        {
            OGRDataSource::DestroyDataSource(poDS);
            psContext->bRet = FALSE;
            return;
        }

        if( bVerbose )
        {
            printf( "INFO: Testing layer %s.\n",
                        poResultSet->GetName() );
        }
        bRet = TestOGRLayer( poDS, poResultSet, TRUE );
        
        poDS->ReleaseResultSet(poResultSet);

        bRet &= TestDSErrorConditions(poDS);
    }
/* -------------------------------------------------------------------- */
/*      Process each data source layer.                                 */
/* -------------------------------------------------------------------- */
    else if (papszLayers == NULL)
    {
        for( int iLayer = 0; iLayer < poDS->GetLayerCount(); iLayer++ )
        {
            OGRLayer        *poLayer = poDS->GetLayer(iLayer);

            if( poLayer == NULL )
            {
                printf( "FAILURE: Couldn't fetch advertised layer %d!\n",
                        iLayer );
                OGRDataSource::DestroyDataSource(poDS);
                psContext->bRet = FALSE;
                return;
            }

            if( bVerbose )
            {
                printf( "INFO: Testing layer %s.\n",
                        poLayer->GetName() );
            }
            bRet &= TestOGRLayer( poDS, poLayer, FALSE );
        }

        bRet &= TestDSErrorConditions(poDS);

        if (poDS->GetLayerCount() >= 2)
        {
            OGRDataSource::DestroyDataSource(poDS);
            poDS = NULL;
            bRet &= TestInterleavedReading( pszDataSource, NULL );
        }
    }
    else
    {
/* -------------------------------------------------------------------- */
/*      Or process layers specified by the user                         */
/* -------------------------------------------------------------------- */
        char** papszLayerIter = papszLayers;
        while (*papszLayerIter)
        {
            OGRLayer        *poLayer = poDS->GetLayerByName(*papszLayerIter);

            if( poLayer == NULL )
            {
                printf( "FAILURE: Couldn't fetch requested layer %s!\n",
                        *papszLayerIter );
                OGRDataSource::DestroyDataSource(poDS);
                psContext->bRet = FALSE;
                return;
            }
            
            if( bVerbose )
            {
                printf( "INFO: Testing layer %s.\n",
                        poLayer->GetName() );
            }
            bRet &= TestOGRLayer( poDS, poLayer, FALSE );
            
            papszLayerIter ++;
        }

        bRet &= TestDSErrorConditions(poDS);

        if (CSLCount(papszLayers) >= 2)
        {
            OGRDataSource::DestroyDataSource(poDS);
            poDS = NULL;
            bRet &= TestInterleavedReading( pszDataSource, papszLayers );
        }
    }

/* -------------------------------------------------------------------- */
/*      Close down.                                                     */
/* -------------------------------------------------------------------- */
    OGRDataSource::DestroyDataSource(poDS);

    psContext->bRet = bRet;
}

/************************************************************************/
/*                               Usage()                                */
/************************************************************************/

static void Usage()

{
    printf( "Usage: test_ogrsf [-ro] [-q] [-threads N] [-loops M] datasource_name \n"
            "                  [[layer1_name, layer2_name, ...] | [-sql statement] [-dialect dialect]]\n" );
    exit( 1 );
}

/************************************************************************/
/*                           TestBasic()                                */
/************************************************************************/

static int TestBasic( OGRLayer *poLayer )
{
    int bRet = TRUE;

    const char* pszLayerName = poLayer->GetName();
    OGRwkbGeometryType eGeomType = poLayer->GetGeomType();
    OGRFeatureDefn* poFDefn = poLayer->GetLayerDefn();

    if( strcmp(pszLayerName, poFDefn->GetName()) != 0 )
    {
        bRet = FALSE;
        printf( "ERROR: poLayer->GetName() and poFDefn>GetName() differ.\n"
                "poLayer->GetName() = %s\n"
                "poFDefn->GetName() = %s\n",
                    pszLayerName, poFDefn->GetName());
    }

    if( eGeomType != poFDefn->GetGeomType() )
    {
        bRet = FALSE;
        printf( "ERROR: poLayer->GetGeomType() and poFDefn->GetGeomType() differ.\n"
                "poLayer->GetGeomType() = %d\n"
                "poFDefn->GetGeomType() = %d\n",
                    eGeomType, poFDefn->GetGeomType());
    }

    if( poLayer->GetFIDColumn() == NULL )
    {
        bRet = FALSE;
        printf( "ERROR: poLayer->GetFIDColumn() returned NULL.\n" );
    }

    if( poLayer->GetGeometryColumn() == NULL )
    {
        bRet = FALSE;
        printf( "ERROR: poLayer->GetGeometryColumn() returned NULL.\n" );
    }

    if( poFDefn->GetGeomFieldCount() > 0 )
    {
        if( eGeomType != poFDefn->GetGeomFieldDefn(0)->GetType() )
        {
            bRet = FALSE;
            printf( "ERROR: poLayer->GetGeomType() and poFDefn->GetGeomFieldDefn(0)->GetType() differ.\n"
                    "poLayer->GetGeomType() = %d\n"
                    "poFDefn->GetGeomFieldDefn(0)->GetType() = %d\n",
                        eGeomType, poFDefn->GetGeomFieldDefn(0)->GetType());
        }

        if( !EQUAL(poLayer->GetGeometryColumn(),
                   poFDefn->GetGeomFieldDefn(0)->GetNameRef()) )
        {
            if( poFDefn->GetGeomFieldCount() > 1 )
                bRet = FALSE;
            printf( "%s: poLayer->GetGeometryColumn() and poFDefn->GetGeomFieldDefn(0)->GetNameRef() differ.\n"
                    "poLayer->GetGeometryColumn() = %s\n"
                    "poFDefn->GetGeomFieldDefn(0)->GetNameRef() = %s\n",
                     ( poFDefn->GetGeomFieldCount() == 1 ) ? "WARNING" : "ERROR",
                    poLayer->GetGeometryColumn(),
                     poFDefn->GetGeomFieldDefn(0)->GetNameRef());
        }

        if( poLayer->GetSpatialRef() !=
                   poFDefn->GetGeomFieldDefn(0)->GetSpatialRef() )
        {
            if( poFDefn->GetGeomFieldCount() > 1 )
                bRet = FALSE;
            printf( "%s: poLayer->GetSpatialRef() and poFDefn->GetGeomFieldDefn(0)->GetSpatialRef() differ.\n"
                    "poLayer->GetSpatialRef() = %p\n"
                    "poFDefn->GetGeomFieldDefn(0)->GetSpatialRef() = %p\n",
                     ( poFDefn->GetGeomFieldCount() == 1 ) ? "WARNING" : "ERROR",
                     poLayer->GetSpatialRef(),
                     poFDefn->GetGeomFieldDefn(0)->GetSpatialRef());
        }
    }

    return bRet;
}

/************************************************************************/
/*                      TestLayerErrorConditions()                      */
/************************************************************************/

static int TestLayerErrorConditions( OGRLayer* poLyr )
{
    int bRet = TRUE;

    CPLPushErrorHandler(CPLQuietErrorHandler);

    if (poLyr->TestCapability("fake_capability"))
    {
        printf( "ERROR: poLyr->TestCapability(\"fake_capability\") should have returned FALSE\n" );
        bRet = FALSE;
        goto bye;
    }

    if (poLyr->GetFeature(-10) != NULL)
    {
        printf( "ERROR: GetFeature(-10) should have returned NULL\n" );
        bRet = FALSE;
        goto bye;
    }

    if (poLyr->GetFeature(2000000000) != NULL)
    {
        printf( "ERROR: GetFeature(2000000000) should have returned NULL\n" );
        bRet = FALSE;
        goto bye;
    }

#if 0
    /* PG driver doesn't issue errors when the feature doesn't exist */
    /* So, not sure if emitting error is expected or not */

    if (poLyr->DeleteFeature(-10) == OGRERR_NONE)
    {
        printf( "ERROR: DeleteFeature(-10) should have returned an error\n" );
        bRet = FALSE;
        goto bye;
    }

    if (poLyr->DeleteFeature(2000000000) == OGRERR_NONE)
    {
        printf( "ERROR: DeleteFeature(2000000000) should have returned an error\n" );
        bRet = FALSE;
        goto bye;
    }
#endif

    if (poLyr->SetNextByIndex(-10) != OGRERR_FAILURE)
    {
        printf( "ERROR: SetNextByIndex(-10) should have returned OGRERR_FAILURE\n" );
        bRet = FALSE;
        goto bye;
    }

    if (poLyr->SetNextByIndex(2000000000) == OGRERR_NONE &&
        poLyr->GetNextFeature() != NULL)
    {
        printf( "ERROR: SetNextByIndex(2000000000) and then GetNextFeature() should have returned NULL\n" );
        bRet = FALSE;
        goto bye;
    }

bye:
    CPLPopErrorHandler();
    return bRet;
}

/************************************************************************/
/*                          GetLayerNameForSQL()                        */
/************************************************************************/

const char* GetLayerNameForSQL( OGRDataSource* poDS, const char* pszLayerName )
{
    int i;
    char ch;
    for(i=0;(ch = pszLayerName[i]) != 0;i++)
    {
        if (ch >= '0' && ch <= '9')
        {
            if (i == 0)
                break;
        }
        else if (!((ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z')))
            break;
    }
    /* Only quote if needed. Quoting conventions depend on the driver... */
    if (ch == 0)
        return pszLayerName;

    if (EQUAL(poDS->GetDriver()->GetName(), "MYSQL"))
        return CPLSPrintf("`%s`", pszLayerName);

    if (EQUAL(poDS->GetDriver()->GetName(), "PostgreSQL") &&
                strchr(pszLayerName, '.'))
    {
        const char* pszRet;
        char** papszTokens = CSLTokenizeStringComplex(pszLayerName, ".", 0, 0);
        if (CSLCount(papszTokens) == 2)
            pszRet = CPLSPrintf("\"%s\".\"%s\"", papszTokens[0], papszTokens[1]);
        else
            pszRet = CPLSPrintf("\"%s\"", pszLayerName);
        CSLDestroy(papszTokens);
        return pszRet;
    }

    if (EQUAL(poDS->GetDriver()->GetName(), "SQLAnywhere"))
        return pszLayerName;

    return CPLSPrintf("\"%s\"", pszLayerName);
}

/************************************************************************/
/*                      TestOGRLayerFeatureCount()                      */
/*                                                                      */
/*      Verify that the feature count matches the actual number of      */
/*      features returned during sequential reading.                    */
/************************************************************************/

static int TestOGRLayerFeatureCount( OGRDataSource* poDS, OGRLayer *poLayer, int bIsSQLLayer )

{
    int bRet = TRUE;
    int         nFC = 0, nClaimedFC = poLayer->GetFeatureCount();
    OGRFeature  *poFeature;
    int         bWarnAboutSRS = FALSE;
    OGRFeatureDefn* poLayerDefn = poLayer->GetLayerDefn();
    int nGeomFieldCount = poLayerDefn->GetGeomFieldCount();

    poLayer->ResetReading();
    CPLErrorReset();

    while( (poFeature = poLayer->GetNextFeature()) != NULL )
    {
        nFC++;

        if (poFeature->GetDefnRef() != poLayerDefn)
        {
            bRet = FALSE;
            printf( "ERROR: Feature defn differs from layer defn.\n"
                    "Feature defn = %p\n"
                    "Layer defn = %p\n",
                     poFeature->GetDefnRef(), poLayerDefn);
        }

        for( int iGeom = 0; iGeom < nGeomFieldCount; iGeom ++ )
        {
            OGRGeometry* poGeom = poFeature->GetGeomFieldRef(iGeom);
            OGRSpatialReference * poGFldSRS =
                poLayerDefn->GetGeomFieldDefn(iGeom)->GetSpatialRef();

            // Compatibility with old drivers anterior to RFC 41
            if( iGeom == 0 && nGeomFieldCount == 1 && poGFldSRS == NULL )
                poGFldSRS = poLayer->GetSpatialRef();

            if( poGeom != NULL
                && poGeom->getSpatialReference() != poGFldSRS
                && !bWarnAboutSRS )
            {
                char        *pszLayerSRSWKT, *pszFeatureSRSWKT;
                
                bWarnAboutSRS = TRUE;

                if( poGFldSRS != NULL )
                    poGFldSRS->exportToWkt( &pszLayerSRSWKT );
                else
                    pszLayerSRSWKT = CPLStrdup("(NULL)");

                if( poGeom->getSpatialReference() != NULL )
                    poGeom->
                        getSpatialReference()->exportToWkt( &pszFeatureSRSWKT );
                else
                    pszFeatureSRSWKT = CPLStrdup("(NULL)");

                bRet = FALSE;
                printf( "ERROR: Feature SRS differs from layer SRS.\n"
                        "Feature SRS = %s (%p)\n"
                        "Layer SRS = %s (%p)\n",
                        pszFeatureSRSWKT, poGeom->getSpatialReference(),
                        pszLayerSRSWKT, poGFldSRS );
                CPLFree( pszLayerSRSWKT );
                CPLFree( pszFeatureSRSWKT );
            }
        }
        
        OGRFeature::DestroyFeature(poFeature);
    }

    /* mapogr.cpp doesn't like errors after GetNextFeature() */
    if (CPLGetLastErrorType() != CE_None )
    {
        bRet = FALSE;
        printf( "ERROR: An error was reported : %s\n",
                CPLGetLastErrorMsg());
    }

    if( nFC != nClaimedFC )
    {
        bRet = FALSE;
        printf( "ERROR: Claimed feature count %d doesn't match actual, %d.\n",
                nClaimedFC, nFC );
    }
    else if( nFC != poLayer->GetFeatureCount() )
    {
        bRet = FALSE;
        printf( "ERROR: Feature count at end of layer %d differs "
                "from at start, %d.\n",
                nFC, poLayer->GetFeatureCount() );
    }
    else if( bVerbose )
        printf( "INFO: Feature count verified.\n" );
        
    if (!bIsSQLLayer)
    {
        CPLString osSQL;

        osSQL.Printf("SELECT COUNT(*) FROM %s", GetLayerNameForSQL(poDS, poLayer->GetName()));

        OGRLayer* poSQLLyr = poDS->ExecuteSQL(osSQL.c_str(), NULL, NULL);
        if (poSQLLyr)
        {
            OGRFeature* poFeatCount = poSQLLyr->GetNextFeature();
            if (poFeatCount == NULL)
            {
                bRet = FALSE;
                printf( "ERROR: '%s' failed.\n", osSQL.c_str() );
            }
            else if (nClaimedFC != poFeatCount->GetFieldAsInteger(0))
            {
                bRet = FALSE;
                printf( "ERROR: Claimed feature count %d doesn't match '%s' one, %d.\n",
                        nClaimedFC, osSQL.c_str(), poFeatCount->GetFieldAsInteger(0) );
            }
            OGRFeature::DestroyFeature(poFeatCount);
            poDS->ReleaseResultSet(poSQLLyr);
        }
    }

    if( bVerbose && !bWarnAboutSRS )
    {
        printf("INFO: Feature/layer spatial ref. consistency verified.\n");
    }

    return bRet;
}

/************************************************************************/
/*                       TestOGRLayerRandomRead()                       */
/*                                                                      */
/*      Read the first 5 features, and then try to use random           */
/*      reading to reread 2 and 5 and verify that this works OK.        */
/*      Don't attempt if there aren't at least 5 features.              */
/************************************************************************/

static int TestOGRLayerRandomRead( OGRLayer *poLayer )

{
    int bRet = TRUE;
    OGRFeature  *papoFeatures[5], *poFeature = NULL;
    int         iFeature;

    poLayer->SetSpatialFilter( NULL );
    
    if( poLayer->GetFeatureCount() < 5 )
    {
        if( bVerbose )
            printf( "INFO: Only %d features on layer,"
                    "skipping random read test.\n",
                    poLayer->GetFeatureCount() );
        
        return bRet;
    }

/* -------------------------------------------------------------------- */
/*      Fetch five features.                                            */
/* -------------------------------------------------------------------- */
    poLayer->ResetReading();
    
    for( iFeature = 0; iFeature < 5; iFeature++ )
    {
        papoFeatures[iFeature] = NULL;
    }
    for( iFeature = 0; iFeature < 5; iFeature++ )
    {
        papoFeatures[iFeature] = poLayer->GetNextFeature();
        if( papoFeatures[iFeature] == NULL )
        {
            if( bVerbose )
                printf( "INFO: Only %d features on layer,"
                        "skipping random read test.\n",
                        iFeature );
            goto end;
        }
    }

/* -------------------------------------------------------------------- */
/*      Test feature 2.                                                 */
/* -------------------------------------------------------------------- */
    poFeature = poLayer->GetFeature( papoFeatures[1]->GetFID() );
    if (poFeature == NULL)
    {
        printf( "ERROR: Cannot fetch feature %ld.\n",
                 papoFeatures[1]->GetFID() );
        goto end;
    }

    if( !poFeature->Equal( papoFeatures[1] ) )
    {
        bRet = FALSE;
        printf( "ERROR: Attempt to randomly read feature %ld appears to\n"
                "       have returned a different feature than sequential\n"
                "       reading indicates should have happened.\n",
                papoFeatures[1]->GetFID() );
        poFeature->DumpReadable(stdout);
        papoFeatures[1]->DumpReadable(stdout);

        goto end;
    }

    OGRFeature::DestroyFeature(poFeature);

/* -------------------------------------------------------------------- */
/*      Test feature 5.                                                 */
/* -------------------------------------------------------------------- */
    poFeature = poLayer->GetFeature( papoFeatures[4]->GetFID() );
    if( !poFeature->Equal( papoFeatures[4] ) )
    {
        bRet = FALSE;
        printf( "ERROR: Attempt to randomly read feature %ld appears to\n"
                "       have returned a different feature than sequential\n"
                "       reading indicates should have happened.\n",
                papoFeatures[4]->GetFID() );

        goto end;
    }

    if( bVerbose )
        printf( "INFO: Random read test passed.\n" );

end:
    OGRFeature::DestroyFeature(poFeature);

/* -------------------------------------------------------------------- */
/*      Cleanup.                                                        */
/* -------------------------------------------------------------------- */
    for( iFeature = 0; iFeature < 5; iFeature++ )
        OGRFeature::DestroyFeature(papoFeatures[iFeature]);

    return bRet;
}


/************************************************************************/
/*                       TestOGRLayerSetNextByIndex()                   */
/*                                                                      */
/************************************************************************/

static int TestOGRLayerSetNextByIndex( OGRLayer *poLayer )

{
    int bRet = TRUE;
    OGRFeature  *papoFeatures[5], *poFeature = NULL;
    int         iFeature;

    memset(papoFeatures, 0, sizeof(papoFeatures));

    poLayer->SetSpatialFilter( NULL );
    
    if( poLayer->GetFeatureCount() < 5 )
    {
        if( bVerbose )
            printf( "INFO: Only %d features on layer,"
                    "skipping SetNextByIndex test.\n",
                    poLayer->GetFeatureCount() );
        
        return bRet;
    }

/* -------------------------------------------------------------------- */
/*      Fetch five features.                                            */
/* -------------------------------------------------------------------- */
    poLayer->ResetReading();
    
    for( iFeature = 0; iFeature < 5; iFeature++ )
    {
        papoFeatures[iFeature] = poLayer->GetNextFeature();
        if( papoFeatures[iFeature] == NULL )
        {
            bRet = FALSE;
            printf( "ERROR: Cannot get feature %d.\n", iFeature );
            goto end;
        }
    }

/* -------------------------------------------------------------------- */
/*      Test feature at index 1.                                        */
/* -------------------------------------------------------------------- */
    if (poLayer->SetNextByIndex(1) != OGRERR_NONE)
    {
        bRet = FALSE;
        printf( "ERROR: SetNextByIndex(%d) failed.\n", 1 );
        goto end;
    }
    
    poFeature = poLayer->GetNextFeature();
    if( poFeature == NULL || !poFeature->Equal( papoFeatures[1] ) )
    {
        bRet = FALSE;
        printf( "ERROR: Attempt to read feature at index %d appears to\n"
                "       have returned a different feature than sequential\n"
                "       reading indicates should have happened.\n",
                1 );

        goto end;
    }

    OGRFeature::DestroyFeature(poFeature);
    
    poFeature = poLayer->GetNextFeature();
    if( poFeature == NULL || !poFeature->Equal( papoFeatures[2] ) )
    {
        bRet = FALSE;
        printf( "ERROR: Attempt to read feature after feature at index %d appears to\n"
                "       have returned a different feature than sequential\n"
                "       reading indicates should have happened.\n",
                1 );

        goto end;
    }

    OGRFeature::DestroyFeature(poFeature);
    poFeature = NULL;
    
/* -------------------------------------------------------------------- */
/*      Test feature at index 3.                                        */
/* -------------------------------------------------------------------- */
    if (poLayer->SetNextByIndex(3) != OGRERR_NONE)
    {
        bRet = FALSE;
        printf( "ERROR: SetNextByIndex(%d) failed.\n", 3 );
        goto end;
    }
    
    poFeature = poLayer->GetNextFeature();
    if( !poFeature->Equal( papoFeatures[3] ) )
    {
        bRet = FALSE;
        printf( "ERROR: Attempt to read feature at index %d appears to\n"
                "       have returned a different feature than sequential\n"
                "       reading indicates should have happened.\n",
                3 );

        goto end;
    }

    OGRFeature::DestroyFeature(poFeature);
    
    poFeature = poLayer->GetNextFeature();
    if( !poFeature->Equal( papoFeatures[4] ) )
    {
        bRet = FALSE;
        printf( "ERROR: Attempt to read feature after feature at index %d appears to\n"
                "       have returned a different feature than sequential\n"
                "       reading indicates should have happened.\n",
                3 );

        goto end;
    }


    if( bVerbose )
        printf( "INFO: SetNextByIndex() read test passed.\n" );

end:
    OGRFeature::DestroyFeature(poFeature);

/* -------------------------------------------------------------------- */
/*      Cleanup.                                                        */
/* -------------------------------------------------------------------- */
    for( iFeature = 0; iFeature < 5; iFeature++ )
        OGRFeature::DestroyFeature(papoFeatures[iFeature]);

    return bRet;
}

/************************************************************************/
/*                      TestOGRLayerRandomWrite()                       */
/*                                                                      */
/*      Test random writing by trying to switch the 2nd and 5th         */
/*      features.                                                       */
/************************************************************************/

static int TestOGRLayerRandomWrite( OGRLayer *poLayer )

{
    int bRet = TRUE;
    OGRFeature  *papoFeatures[5], *poFeature;
    int         iFeature;
    long        nFID2, nFID5;

    memset(papoFeatures, 0, sizeof(papoFeatures));

    poLayer->SetSpatialFilter( NULL );

    if( poLayer->GetFeatureCount() < 5 )
    {
        if( bVerbose )
            printf( "INFO: Only %d features on layer,"
                    "skipping random write test.\n",
                    poLayer->GetFeatureCount() );
        
        return bRet;
    }

    if( !poLayer->TestCapability( OLCRandomRead ) )
    {
        if( bVerbose )
            printf( "INFO: Skipping random write test since this layer "
                    "doesn't support random read.\n" );
        return bRet;
    }

/* -------------------------------------------------------------------- */
/*      Fetch five features.                                            */
/* -------------------------------------------------------------------- */
    poLayer->ResetReading();
    
    for( iFeature = 0; iFeature < 5; iFeature++ )
    {
        papoFeatures[iFeature] = poLayer->GetNextFeature();
        if( papoFeatures[iFeature] == NULL )
        {
            bRet = FALSE;
            printf( "ERROR: Cannot get feature %d.\n", iFeature );
            goto end;
        }
    }

/* -------------------------------------------------------------------- */
/*      Switch feature ids of feature 2 and 5.                          */
/* -------------------------------------------------------------------- */
    nFID2 = papoFeatures[1]->GetFID();
    nFID5 = papoFeatures[4]->GetFID();

    papoFeatures[1]->SetFID( nFID5 );
    papoFeatures[4]->SetFID( nFID2 );

/* -------------------------------------------------------------------- */
/*      Rewrite them.                                                   */
/* -------------------------------------------------------------------- */
    if( poLayer->SetFeature( papoFeatures[1] ) != OGRERR_NONE )
    {
        bRet = FALSE;
        printf( "ERROR: Attempt to SetFeature(1) failed.\n" );
        goto end;
    }
    if( poLayer->SetFeature( papoFeatures[4] ) != OGRERR_NONE )
    {
        bRet = FALSE;
        printf( "ERROR: Attempt to SetFeature(4) failed.\n" );
        goto end;
    }

/* -------------------------------------------------------------------- */
/*      Now re-read feature 2 to verify the effect stuck.               */
/* -------------------------------------------------------------------- */
    poFeature = poLayer->GetFeature( nFID5 );
    if(poFeature == NULL)
    {
        bRet = FALSE;
        printf( "ERROR: Attempt to GetFeature( nFID5 ) failed.\n" );
        goto end;
    }
    if( !poFeature->Equal(papoFeatures[1]) )
    {
        bRet = FALSE;
        poFeature->DumpReadable(stderr);
        papoFeatures[1]->DumpReadable(stderr);
        printf( "ERROR: Written feature didn't seem to retain value.\n" );
    }
    else if( bVerbose )
    {
        printf( "INFO: Random write test passed.\n" );
    }
    OGRFeature::DestroyFeature(poFeature);

/* -------------------------------------------------------------------- */
/*      Re-invert the features to restore to original state             */
/* -------------------------------------------------------------------- */

    papoFeatures[1]->SetFID( nFID2 );
    papoFeatures[4]->SetFID( nFID5 );

    if( poLayer->SetFeature( papoFeatures[1] ) != OGRERR_NONE )
    {
        bRet = FALSE;
        printf( "ERROR: Attempt to restore SetFeature(1) failed.\n" );
    }
    if( poLayer->SetFeature( papoFeatures[4] ) != OGRERR_NONE )
    {
        bRet = FALSE;
        printf( "ERROR: Attempt to restore SetFeature(4) failed.\n" );
    }

end:
/* -------------------------------------------------------------------- */
/*      Cleanup.                                                        */
/* -------------------------------------------------------------------- */

    for( iFeature = 0; iFeature < 5; iFeature++ )
        OGRFeature::DestroyFeature(papoFeatures[iFeature]);

    return bRet;
}

#ifndef INFINITY
    static CPL_INLINE double CPLInfinity(void)
    {
        static double ZERO = 0;
        return 1.0 / ZERO; /* MSVC doesn't like 1.0 / 0.0 */
    }
    #define INFINITY CPLInfinity()
#endif

/************************************************************************/
/*                         TestSpatialFilter()                          */
/*                                                                      */
/*      This is intended to be a simple test of the spatial             */
/*      filtering.  We read the first feature.  Then construct a        */
/*      spatial filter geometry which includes it, install and          */
/*      verify that we get the feature.  Next install a spatial         */
/*      filter that doesn't include this feature, and test again.       */
/************************************************************************/

static int TestSpatialFilter( OGRLayer *poLayer, int iGeomField )

{
    int bRet = TRUE;
    OGRFeature  *poFeature, *poTargetFeature;
    OGRPolygon  oInclusiveFilter, oExclusiveFilter;
    OGRLinearRing oRing;
    OGREnvelope sEnvelope;
    int         nInclusiveCount;

/* -------------------------------------------------------------------- */
/*      Read the target feature.                                        */
/* -------------------------------------------------------------------- */
    poLayer->ResetReading();
    poTargetFeature = poLayer->GetNextFeature();

    if( poTargetFeature == NULL )
    {
        if( bVerbose )
        {
            printf( "INFO: Skipping Spatial Filter test for %s.\n"
                    "      No features in layer.\n",
                    poLayer->GetName() );
        }
        return bRet;
    }

    OGRGeometry* poGeom = poTargetFeature->GetGeomFieldRef(iGeomField);
    if( poGeom == NULL || poGeom->IsEmpty() )
    {
        if( bVerbose )
        {
            printf( "INFO: Skipping Spatial Filter test for %s,\n"
                    "      target feature has no geometry.\n",
                    poTargetFeature->GetDefnRef()->GetName() );
        }
        OGRFeature::DestroyFeature(poTargetFeature);
        return bRet;
    }

    poGeom->getEnvelope( &sEnvelope );

/* -------------------------------------------------------------------- */
/*      Construct inclusive filter.                                     */
/* -------------------------------------------------------------------- */
    
    oRing.setPoint( 0, sEnvelope.MinX - 20.0, sEnvelope.MinY - 20.0 );
    oRing.setPoint( 1, sEnvelope.MinX - 20.0, sEnvelope.MaxY + 10.0 );
    oRing.setPoint( 2, sEnvelope.MaxX + 10.0, sEnvelope.MaxY + 10.0 );
    oRing.setPoint( 3, sEnvelope.MaxX + 10.0, sEnvelope.MinY - 20.0 );
    oRing.setPoint( 4, sEnvelope.MinX - 20.0, sEnvelope.MinY - 20.0 );
    
    oInclusiveFilter.addRing( &oRing );

    poLayer->SetSpatialFilter( iGeomField, &oInclusiveFilter );

/* -------------------------------------------------------------------- */
/*      Verify that we can find the target feature.                     */
/* -------------------------------------------------------------------- */
    poLayer->ResetReading();

    while( (poFeature = poLayer->GetNextFeature()) != NULL )
    {
        if( poFeature->Equal(poTargetFeature) )
        {
            OGRFeature::DestroyFeature(poFeature);
            break;
        }
        else
            OGRFeature::DestroyFeature(poFeature);
    }

    if( poFeature == NULL )
    {
        bRet = FALSE;
        printf( "ERROR: Spatial filter (%d) eliminated a feature unexpectedly!\n",
                iGeomField);
    }
    else if( bVerbose )
    {
        printf( "INFO: Spatial filter inclusion seems to work.\n" );
    }

    nInclusiveCount = poLayer->GetFeatureCount();

/* -------------------------------------------------------------------- */
/*      Construct exclusive filter.                                     */
/* -------------------------------------------------------------------- */
    oRing.setPoint( 0, sEnvelope.MinX - 20.0, sEnvelope.MinY - 20.0 );
    oRing.setPoint( 1, sEnvelope.MinX - 10.0, sEnvelope.MinY - 20.0 );
    oRing.setPoint( 2, sEnvelope.MinX - 10.0, sEnvelope.MinY - 10.0 );
    oRing.setPoint( 3, sEnvelope.MinX - 20.0, sEnvelope.MinY - 10.0 );
    oRing.setPoint( 4, sEnvelope.MinX - 20.0, sEnvelope.MinY - 20.0 );
    
    oExclusiveFilter.addRing( &oRing );

    poLayer->SetSpatialFilter( iGeomField, &oExclusiveFilter );

/* -------------------------------------------------------------------- */
/*      Verify that we can find the target feature.                     */
/* -------------------------------------------------------------------- */
    poLayer->ResetReading();

    while( (poFeature = poLayer->GetNextFeature()) != NULL )
    {
        if( poFeature->Equal(poTargetFeature) )
        {
            OGRFeature::DestroyFeature(poFeature);
            break;
        }
        else
            OGRFeature::DestroyFeature(poFeature);
    }

    if( poFeature != NULL )
    {
        bRet = FALSE;
        printf( "ERROR: Spatial filter (%d) failed to eliminate"
                "a feature unexpectedly!\n",
                iGeomField);
    }
    else if( poLayer->GetFeatureCount() >= nInclusiveCount )
    {
        bRet = FALSE;
        printf( "ERROR: GetFeatureCount() may not be taking spatial "
                "filter (%d) into account.\n" ,
                iGeomField);
    }
    else if( bVerbose )
    {
        printf( "INFO: Spatial filter exclusion seems to work.\n" );
    }

    // Check that GetFeature() ignores the spatial filter
    poFeature = poLayer->GetFeature( poTargetFeature->GetFID() );
    if( poFeature == NULL || !poFeature->Equal(poTargetFeature) )
    {
        bRet = FALSE;
        printf( "ERROR: Spatial filter has been taken into account by GetFeature()\n");
    }
    else if( bVerbose )
    {
        printf( "INFO: Spatial filter is ignored by GetFeature() as expected.\n");
    }
    if( poFeature != NULL )
        OGRFeature::DestroyFeature(poFeature);

    if( bRet )
    {
        poLayer->ResetReading();
        while( (poFeature = poLayer->GetNextFeature()) != NULL )
        {
            if( poFeature->Equal(poTargetFeature) )
            {
                OGRFeature::DestroyFeature(poFeature);
                break;
            }
            else
                OGRFeature::DestroyFeature(poFeature);
        }
        if( poFeature != NULL )
        {
            bRet = FALSE;
            printf( "ERROR: Spatial filter has not been restored correctly after GetFeature()\n");
        }
    }

    OGRFeature::DestroyFeature(poTargetFeature);

/* -------------------------------------------------------------------- */
/*     Test infinity envelope                                           */
/* -------------------------------------------------------------------- */

#define NEG_INF -INFINITY
#define POS_INF INFINITY

    oRing.setPoint( 0, NEG_INF, NEG_INF );
    oRing.setPoint( 1, NEG_INF, POS_INF );
    oRing.setPoint( 2, POS_INF, POS_INF );
    oRing.setPoint( 3, POS_INF, NEG_INF );
    oRing.setPoint( 4, NEG_INF, NEG_INF );

    OGRPolygon oInfinityFilter;
    oInfinityFilter.addRing( &oRing );

    poLayer->SetSpatialFilter( iGeomField, &oInfinityFilter );
    poLayer->ResetReading();
    int nCountInf = 0;
    while( (poFeature = poLayer->GetNextFeature()) != NULL )
    {
        if( poFeature->GetGeomFieldRef(iGeomField) != NULL )
            nCountInf ++;
        delete poFeature;
    }

/* -------------------------------------------------------------------- */
/*     Test envelope with huge coords                                   */
/* -------------------------------------------------------------------- */

#define HUGE_COORDS (1e300)

    oRing.setPoint( 0, -HUGE_COORDS, -HUGE_COORDS );
    oRing.setPoint( 1, -HUGE_COORDS, HUGE_COORDS );
    oRing.setPoint( 2, HUGE_COORDS, HUGE_COORDS );
    oRing.setPoint( 3, HUGE_COORDS, -HUGE_COORDS );
    oRing.setPoint( 4, -HUGE_COORDS, -HUGE_COORDS );

    OGRPolygon oHugeFilter;
    oHugeFilter.addRing( &oRing );

    poLayer->SetSpatialFilter( iGeomField, &oHugeFilter );
    poLayer->ResetReading();
    int nCountHuge = 0;
    while( (poFeature = poLayer->GetNextFeature()) != NULL )
    {
        if( poFeature->GetGeomFieldRef(iGeomField) != NULL )
            nCountHuge ++;
        delete poFeature;
    }

/* -------------------------------------------------------------------- */
/*     Reset spatial filter                                             */
/* -------------------------------------------------------------------- */
    poLayer->SetSpatialFilter( NULL );

    int nExpected = 0;
    poLayer->ResetReading();
    while( (poFeature = poLayer->GetNextFeature()) != NULL )
    {
        if( poFeature->GetGeomFieldRef(iGeomField) != NULL )
            nExpected ++;
        delete poFeature;
    }
    poLayer->ResetReading();

    if( nCountInf != nExpected )
    {
        /*bRet = FALSE; */
        printf( "WARNING: Infinity spatial filter returned %d features instead of %d\n",
                nCountInf, nExpected );
    }
    else if( bVerbose )
    {
        printf( "INFO: Infinity spatial filter works as expected.\n");
    }

    if( nCountHuge != nExpected )
    {
        /* bRet = FALSE; */
        printf( "WARNING: Huge coords spatial filter returned %d features instead of %d\n",
                nCountHuge, nExpected );
    }
    else if( bVerbose )
    {
        printf( "INFO: Huge coords spatial filter works as expected.\n");
    }

    return bRet;
}

static int TestSpatialFilter( OGRLayer *poLayer )
{
/* -------------------------------------------------------------------- */
/*      Read the target feature.                                        */
/* -------------------------------------------------------------------- */
    poLayer->ResetReading();
    OGRFeature* poTargetFeature = poLayer->GetNextFeature();

    if( poTargetFeature == NULL )
    {
        if( bVerbose )
        {
            printf( "INFO: Skipping Spatial Filter test for %s.\n"
                    "      No features in layer.\n",
                    poLayer->GetName() );
        }
        return TRUE;
    }
    OGRFeature::DestroyFeature(poTargetFeature);

    if( poLayer->GetLayerDefn()->GetGeomFieldCount() == 0 )
    {
        if( bVerbose )
        {
            printf( "INFO: Skipping Spatial Filter test for %s,\n"
                    "      target feature has no geometry.\n",
                    poLayer->GetName() );
        }
        return TRUE;
    }

    int bRet = TRUE;
    int nGeomFieldCount = poLayer->GetLayerDefn()->GetGeomFieldCount();
    for( int iGeom = 0; iGeom < nGeomFieldCount; iGeom ++ )
        bRet &= TestSpatialFilter(poLayer, iGeom);
    return bRet;
}


/************************************************************************/
/*                      TestAttributeFilter()                           */
/*                                                                      */
/*      This is intended to be a simple test of the attribute           */
/*      filtering.  We read the first feature.  Then construct a        */
/*      attribute filter which includes it, install and                 */
/*      verify that we get the feature.  Next install a attribute       */
/*      filter that doesn't include this feature, and test again.       */
/************************************************************************/

static int TestAttributeFilter( OGRDataSource* poDS, OGRLayer *poLayer )

{
    int bRet = TRUE;
    OGRFeature  *poFeature, *poFeature2, *poFeature3, *poTargetFeature;
    int         nInclusiveCount, nExclusiveCount, nTotalCount;
    CPLString osAttributeFilter;

/* -------------------------------------------------------------------- */
/*      Read the target feature.                                        */
/* -------------------------------------------------------------------- */
    poLayer->ResetReading();
    poTargetFeature = poLayer->GetNextFeature();

    if( poTargetFeature == NULL )
    {
        if( bVerbose )
        {
            printf( "INFO: Skipping Attribute Filter test for %s.\n"
                    "      No features in layer.\n",
                    poLayer->GetName() );
        }
        return bRet;
    }

    int i;
    OGRFieldType eType = OFTString;
    for(i=0;i<poTargetFeature->GetFieldCount();i++)
    {
        eType = poTargetFeature->GetFieldDefnRef(i)->GetType();
        if (poTargetFeature->IsFieldSet(i) &&
            (eType == OFTString || eType == OFTInteger || eType == OFTReal))
        {
            break;
        }
    }
    if( i == poTargetFeature->GetFieldCount() )
    {
        if( bVerbose )
        {
            printf( "INFO: Skipping Attribute Filter test for %s.\n"
                    "      Could not find non NULL field.\n",
                    poLayer->GetName() );
        }
        OGRFeature::DestroyFeature(poTargetFeature);
        return bRet;
    }

    const char* pszFieldName = poTargetFeature->GetFieldDefnRef(i)->GetNameRef();
    CPLString osValue = poTargetFeature->GetFieldAsString(i);
    if( eType == OFTReal )
        osValue.Printf("%.18g", poTargetFeature->GetFieldAsDouble(i));

/* -------------------------------------------------------------------- */
/*      Construct inclusive filter.                                     */
/* -------------------------------------------------------------------- */

    if (EQUAL(poDS->GetDriver()->GetName(), "PostgreSQL") &&
        (strchr(pszFieldName, '_') || strchr(pszFieldName, ' ')))
    {
        osAttributeFilter = "\"";
        osAttributeFilter += pszFieldName;
        osAttributeFilter += "\"";
    }
    else if (strchr(pszFieldName, ' ') || pszFieldName[0] == '_')
    {
        osAttributeFilter = "'";
        osAttributeFilter += pszFieldName;
        osAttributeFilter += "'";
    }
    else
        osAttributeFilter = pszFieldName;
    osAttributeFilter += " = ";
    if (eType == OFTString)
        osAttributeFilter += "'";
    osAttributeFilter += osValue;
    if (eType == OFTString)
        osAttributeFilter += "'";
    /* Make sure that the literal will be recognized as a float value */
    /* to avoid int underflow/overflow */
    else if (eType == OFTReal && strchr(osValue, '.') == NULL)
        osAttributeFilter += ".";
    poLayer->SetAttributeFilter( osAttributeFilter );

/* -------------------------------------------------------------------- */
/*      Verify that we can find the target feature.                     */
/* -------------------------------------------------------------------- */
    poLayer->ResetReading();

    while( (poFeature = poLayer->GetNextFeature()) != NULL )
    {
        if( poFeature->Equal(poTargetFeature) )
        {
            OGRFeature::DestroyFeature(poFeature);
            break;
        }
        else
            OGRFeature::DestroyFeature(poFeature);
    }

    if( poFeature == NULL )
    {
        bRet = FALSE;
        printf( "ERROR: Attribute filter eliminated a feature unexpectedly!\n");
    }
    else if( bVerbose )
    {
        printf( "INFO: Attribute filter inclusion seems to work.\n" );
    }

    nInclusiveCount = poLayer->GetFeatureCount();

/* -------------------------------------------------------------------- */
/*      Construct exclusive filter.                                     */
/* -------------------------------------------------------------------- */
    if (EQUAL(poDS->GetDriver()->GetName(), "PostgreSQL") &&
        (strchr(pszFieldName, '_') || strchr(pszFieldName, ' ')))
    {
        osAttributeFilter = "\"";
        osAttributeFilter += pszFieldName;
        osAttributeFilter += "\"";
    }
    else if (strchr(pszFieldName, ' ') || pszFieldName[0] == '_')
    {
        osAttributeFilter = "'";
        osAttributeFilter += pszFieldName;
        osAttributeFilter += "'";
    }
    else
        osAttributeFilter = pszFieldName;
    osAttributeFilter += " <> ";
    if (eType == OFTString)
        osAttributeFilter += "'";
    osAttributeFilter += osValue;
    if (eType == OFTString)
        osAttributeFilter += "'";
    /* Make sure that the literal will be recognized as a float value */
    /* to avoid int underflow/overflow */
    else if (eType == OFTReal && strchr(osValue, '.') == NULL)
        osAttributeFilter += ".";
    poLayer->SetAttributeFilter( osAttributeFilter );

/* -------------------------------------------------------------------- */
/*      Verify that we can find the target feature.                     */
/* -------------------------------------------------------------------- */
    poLayer->ResetReading();

    int nExclusiveCountWhileIterating = 0;
    while( (poFeature = poLayer->GetNextFeature()) != NULL )
    {
        if( poFeature->Equal(poTargetFeature) )
        {
            OGRFeature::DestroyFeature(poFeature);
            break;
        }
        else
            OGRFeature::DestroyFeature(poFeature);
        nExclusiveCountWhileIterating ++;
    }

    nExclusiveCount = poLayer->GetFeatureCount();

    // Check that GetFeature() ignores the attribute filter
    poFeature2 = poLayer->GetFeature( poTargetFeature->GetFID() );

    poLayer->ResetReading();
    while( (poFeature3 = poLayer->GetNextFeature()) != NULL )
    {
        if( poFeature3->Equal(poTargetFeature) )
        {
            OGRFeature::DestroyFeature(poFeature3);
            break;
        }
        else
            OGRFeature::DestroyFeature(poFeature3);
    }

    poLayer->SetAttributeFilter( NULL );

    nTotalCount = poLayer->GetFeatureCount();

    if( poFeature != NULL )
    {
        bRet = FALSE;
        printf( "ERROR: Attribute filter failed to eliminate "
                "a feature unexpectedly!\n");
    }
    else if( nExclusiveCountWhileIterating != nExclusiveCount ||
             nExclusiveCount >= nTotalCount ||
             nInclusiveCount > nTotalCount ||
             (nInclusiveCount == nTotalCount && nExclusiveCount != 0))
    {
        bRet = FALSE;
        printf( "ERROR: GetFeatureCount() may not be taking attribute "
                "filter into account (nInclusiveCount = %d, nExclusiveCount = %d, nExclusiveCountWhileIterating = %d, nTotalCount = %d).\n",
                 nInclusiveCount, nExclusiveCount, nExclusiveCountWhileIterating, nTotalCount);
    }
    else if( bVerbose )
    {
        printf( "INFO: Attribute filter exclusion seems to work.\n" );
    }
    
    if( poFeature2 == NULL || !poFeature2->Equal(poTargetFeature) )
    {
        bRet = FALSE;
        printf( "ERROR: Attribute filter has been taken into account by GetFeature()\n");
    }
    else if( bVerbose )
    {
        printf( "INFO: Attribute filter is ignored by GetFeature() as expected.\n");
    }

    if( poFeature3 != NULL )
    {
        bRet = FALSE;
        printf( "ERROR: Attribute filter has not been restored correctly after GetFeature()\n");
    }

    if( poFeature2 != NULL )
        OGRFeature::DestroyFeature(poFeature2);

    OGRFeature::DestroyFeature(poTargetFeature);

    return bRet;
}

/************************************************************************/
/*                         TestOGRLayerUTF8()                           */
/************************************************************************/

static int TestOGRLayerUTF8 ( OGRLayer *poLayer )
{
    int bRet = TRUE;

    poLayer->SetSpatialFilter( NULL );
    poLayer->SetAttributeFilter( NULL );
    poLayer->ResetReading();

    int bIsAdvertizedAsUTF8 = poLayer->TestCapability( OLCStringsAsUTF8 );
    int nFields = poLayer->GetLayerDefn()->GetFieldCount();
    int bFoundString = FALSE;
    int bFoundNonASCII = FALSE;
    int bFoundUTF8 = FALSE;
    int bCanAdvertizeUTF8 = TRUE;

    OGRFeature* poFeature = NULL;
    while( bRet && (poFeature = poLayer->GetNextFeature()) != NULL )
    {
        for(int i = 0; i<nFields; i++)
        {
            if (!poFeature->IsFieldSet(i))
                continue;
            if (poFeature->GetFieldDefnRef(i)->GetType() == OFTString)
            {
                const char* pszVal = poFeature->GetFieldAsString(i);
                if (pszVal[0] != 0)
                {
                    bFoundString = TRUE;
                    const GByte* pszIter = (const GByte*) pszVal;
                    int bIsASCII = TRUE;
                    while(*pszIter)
                    {
                        if (*pszIter >= 128)
                        {
                            bFoundNonASCII = TRUE;
                            bIsASCII = FALSE;
                            break;
                        }
                        pszIter ++;
                    }
                    int bIsUTF8 = CPLIsUTF8(pszVal, -1);
                    if (bIsUTF8 && !bIsASCII)
                        bFoundUTF8 = TRUE;
                    if (bIsAdvertizedAsUTF8)
                    {
                        if (!bIsUTF8)
                        {
                            printf( "ERROR: Found non-UTF8 content at field %d of feature %ld, but layer is advertized as UTF-8.\n",
                                    i, poFeature->GetFID() );
                            bRet = FALSE;
                            break;
                        }
                    }
                    else
                    {
                        if (!bIsUTF8)
                            bCanAdvertizeUTF8 = FALSE;
                    }
                }
            }
        }
        OGRFeature::DestroyFeature(poFeature);
    }

    if (!bFoundString)
    {
    }
    else if (bCanAdvertizeUTF8 && bVerbose)
    {
        if (bIsAdvertizedAsUTF8)
        {
            if (bFoundUTF8)
            {
                printf( "INFO: Layer has UTF-8 content and is consistently declared as having UTF-8 content.\n" );
            }
            else if (!bFoundNonASCII)
            {
                printf( "INFO: Layer has ASCII only content and is consistently declared as having UTF-8 content.\n" );
            }
        }
        else
        {
            if (bFoundUTF8)
            {
                printf( "INFO: Layer could perhaps be advertized as UTF-8 compatible (and it has non-ASCII UTF-8 content).\n" );
            }
            else if (!bFoundNonASCII)
            {
                printf( "INFO: Layer could perhaps be advertized as UTF-8 compatible (it has only ASCII content).\n" );
            }
        }
    }
    else if( bVerbose )
    {
        printf( "INFO: Layer has non UTF-8 content (and is consistently declared as not being UTF-8 compatible).\n" );
    }

    return bRet;
}

/************************************************************************/
/*                         TestGetExtent()                              */
/************************************************************************/

static int TestGetExtent ( OGRLayer *poLayer, int iGeomField )
{
    int bRet = TRUE;

    poLayer->SetSpatialFilter( NULL );
    poLayer->SetAttributeFilter( NULL );
    poLayer->ResetReading();

    OGREnvelope sExtent;
    OGREnvelope sExtentSlow;

    OGRErr eErr = poLayer->GetExtent(iGeomField, &sExtent, TRUE);
    OGRErr eErr2 = poLayer->OGRLayer::GetExtent(iGeomField, &sExtentSlow, TRUE);

    if (eErr != eErr2)
    {
        if (eErr == OGRERR_NONE && eErr2 != OGRERR_NONE)
        {
            /* with the LIBKML driver and test_ogrsf ../autotest/ogr/data/samples.kml "Styles and Markup" */
            if( bVerbose )
            {
                printf("INFO: GetExtent() succeeded but OGRLayer::GetExtent() failed.\n");
            }
        }
        else
        {
            bRet = FALSE;
            if( bVerbose )
            {
                printf("ERROR: GetExtent() failed but OGRLayer::GetExtent() succeeded.\n");
            }
        }
    }
    else if (eErr == OGRERR_NONE && bVerbose)
    {
        if (fabs(sExtentSlow.MinX - sExtent.MinX) < 1e-10 &&
            fabs(sExtentSlow.MinY - sExtent.MinY) < 1e-10 &&
            fabs(sExtentSlow.MaxX - sExtent.MaxX) < 1e-10 &&
            fabs(sExtentSlow.MaxY - sExtent.MaxY) < 1e-10)
        {
            printf("INFO: GetExtent() test passed.\n");
        }
        else
        {
            if (sExtentSlow.Contains(sExtent))
            {
                printf("INFO: sExtentSlow.Contains(sExtent)\n");
            }
            else if (sExtent.Contains(sExtentSlow))
            {
                printf("INFO: sExtent.Contains(sExtentSlow)\n");
            }
            else
            {
                printf("INFO: unknown relationship between sExtent and sExentSlow.\n");
            }
            printf("INFO: sExtentSlow.MinX = %.15f\n", sExtentSlow.MinX);
            printf("INFO: sExtentSlow.MinY = %.15f\n", sExtentSlow.MinY);
            printf("INFO: sExtentSlow.MaxX = %.15f\n", sExtentSlow.MaxX);
            printf("INFO: sExtentSlow.MaxY = %.15f\n", sExtentSlow.MaxY);
            printf("INFO: sExtent.MinX = %.15f\n", sExtent.MinX);
            printf("INFO: sExtent.MinY = %.15f\n", sExtent.MinY);
            printf("INFO: sExtent.MaxX = %.15f\n", sExtent.MaxX);
            printf("INFO: sExtent.MaxY = %.15f\n", sExtent.MaxY);
        }
    }

    return bRet;
}

static int TestGetExtent ( OGRLayer *poLayer )
{
    int bRet = TRUE;
    int nGeomFieldCount = poLayer->GetLayerDefn()->GetGeomFieldCount();
    for( int iGeom = 0; iGeom < nGeomFieldCount; iGeom ++ )
        bRet &= TestGetExtent(poLayer, iGeom);
    return bRet;
}

/*************************************************************************/
/*             TestOGRLayerDeleteAndCreateFeature()                      */
/*                                                                       */
/*      Test delete feature by trying to delete the last feature and     */
/*      recreate it.                                                     */
/*************************************************************************/

static int TestOGRLayerDeleteAndCreateFeature( OGRLayer *poLayer )

{
    int bRet = TRUE;
    OGRFeature  * poFeature = NULL;
    OGRFeature  * poFeatureTest = NULL;
    long        nFID;

    poLayer->SetSpatialFilter( NULL );
    
    if( !poLayer->TestCapability( OLCRandomRead ) )
    {
        if( bVerbose )
            printf( "INFO: Skipping delete feature test since this layer "
                    "doesn't support random read.\n" );
        return bRet;
    }

    if( poLayer->GetFeatureCount() == 0 )
    {
        if( bVerbose )
            printf( "INFO: No feature available on layer '%s',"
                    "skipping delete/create feature test.\n",
                    poLayer->GetName() );
        
        return bRet;
    }
/* -------------------------------------------------------------------- */
/*      Fetch the last feature                                          */
/* -------------------------------------------------------------------- */
    poLayer->ResetReading();

    poLayer->SetNextByIndex(poLayer->GetFeatureCount() - 1);
    poFeature = poLayer->GetNextFeature();
    if (poFeature == NULL)
    {
        bRet = FALSE;
        printf( "ERROR: Could not get last feature of layer.\n" );
        goto end;
    }

/* -------------------------------------------------------------------- */
/*      Get the feature ID of the last feature                          */
/* -------------------------------------------------------------------- */
    nFID = poFeature->GetFID();

/* -------------------------------------------------------------------- */
/*      Delete the feature.                                             */
/* -------------------------------------------------------------------- */
    if( poLayer->DeleteFeature( nFID ) != OGRERR_NONE )
    {
        bRet = FALSE;
        printf( "ERROR: Attempt to DeleteFeature() failed.\n" );
        goto end;
    }
    
/* -------------------------------------------------------------------- */
/*      Now re-read the feature to verify the delete effect worked.     */
/* -------------------------------------------------------------------- */
    CPLPushErrorHandler(CPLQuietErrorHandler); /* silent legitimate error message */
    poFeatureTest = poLayer->GetFeature( nFID );
    CPLPopErrorHandler();
    if( poFeatureTest != NULL)
    {
        bRet = FALSE;
        printf( "ERROR: The feature was not deleted.\n" );
    }
    else if( bVerbose )
    {
        printf( "INFO: Delete Feature test passed.\n" );
    }
    OGRFeature::DestroyFeature(poFeatureTest);

/* -------------------------------------------------------------------- */
/*      Re-insert the features to restore to original state             */
/* -------------------------------------------------------------------- */
    if( poLayer->CreateFeature( poFeature ) != OGRERR_NONE )
    {
        bRet = FALSE;
        printf( "ERROR: Attempt to restore feature failed.\n" );
    }

    if( poFeature->GetFID() != nFID )
    {
        /* Case of shapefile driver for example that will not try to */
        /* reuse the existing FID, but will assign a new one */
        if( bVerbose )
        {
            printf( "INFO: Feature was created, but with not its original FID.\n" );
        }
        nFID = poFeature->GetFID();
    }

/* -------------------------------------------------------------------- */
/*      Now re-read the feature to verify the create effect worked.     */
/* -------------------------------------------------------------------- */
    poFeatureTest = poLayer->GetFeature( nFID );
    if( poFeatureTest == NULL)
    {
        bRet = FALSE;
        printf( "ERROR: The feature was not created.\n" );
    }
    else if( bVerbose )
    {
        printf( "INFO: Create Feature test passed.\n" );
    }
    OGRFeature::DestroyFeature(poFeatureTest);
    
end:
/* -------------------------------------------------------------------- */
/*      Cleanup.                                                        */
/* -------------------------------------------------------------------- */

    OGRFeature::DestroyFeature(poFeature);

    return bRet;
}

/*************************************************************************/
/*                         TestTransactions()                            */
/*************************************************************************/

static int TestTransactions( OGRLayer *poLayer )

{
    OGRFeature* poFeature = NULL;
    int nInitialFeatureCount = poLayer->GetFeatureCount();

    OGRErr eErr = poLayer->StartTransaction();
    if (eErr == OGRERR_NONE)
    {
        if (poLayer->TestCapability(OLCTransactions) == FALSE)
        {
            eErr = poLayer->RollbackTransaction();
            if (eErr == OGRERR_UNSUPPORTED_OPERATION && poLayer->TestCapability(OLCTransactions) == FALSE)
            {
                /* The default implementation has a dummy StartTransaction(), but RollbackTransaction() returns */
                /* OGRERR_UNSUPPORTED_OPERATION */
                if( bVerbose )
                {
                    printf( "INFO: Transactions test skipped due to lack of transaction support.\n" );
                }
                return TRUE;
            }
            else
            {
                printf("WARN: StartTransaction() is supported, but TestCapability(OLCTransactions) returns FALSE.\n");
            }
        }
    }
    else if (eErr == OGRERR_FAILURE)
    {
        if (poLayer->TestCapability(OLCTransactions) == TRUE)
        {
            printf("ERROR: StartTransaction() failed, but TestCapability(OLCTransactions) returns TRUE.\n");
            return FALSE;
        }
        else
        {
            return TRUE;
        }
    }

    eErr = poLayer->RollbackTransaction();
    if (eErr != OGRERR_NONE)
    {
        printf("ERROR: RollbackTransaction() failed after successfull StartTransaction().\n");
        return FALSE;
    }

    /* ---------------- */

    eErr = poLayer->StartTransaction();
    if (eErr != OGRERR_NONE)
    {
        printf("ERROR: StartTransaction() failed.\n");
        return FALSE;
    }

    eErr = poLayer->CommitTransaction();
    if (eErr != OGRERR_NONE)
    {
        printf("ERROR: CommitTransaction() failed after successfull StartTransaction().\n");
        return FALSE;
    }

    /* ---------------- */

    eErr = poLayer->StartTransaction();
    if (eErr != OGRERR_NONE)
    {
        printf("ERROR: StartTransaction() failed.\n");
        return FALSE;
    }

    poFeature = new OGRFeature(poLayer->GetLayerDefn());
    if (poLayer->GetLayerDefn()->GetFieldCount() > 0)
        poFeature->SetField(0, "0");
    eErr = poLayer->CreateFeature(poFeature);
    delete poFeature;
    poFeature = NULL;

    if (eErr == OGRERR_FAILURE)
    {
        if( bVerbose )
        {
            printf("INFO: CreateFeature() failed. Exiting this test now.\n");
        }
        poLayer->RollbackTransaction();
        return TRUE;
    }

    eErr = poLayer->RollbackTransaction();
    if (eErr != OGRERR_NONE)
    {
        printf("ERROR: RollbackTransaction() failed after successfull StartTransaction().\n");
        return FALSE;
    }

    if (poLayer->GetFeatureCount() != nInitialFeatureCount)
    {
        printf("ERROR: GetFeatureCount() should have returned its initial value after RollbackTransaction().\n");
        poLayer->RollbackTransaction();
        return FALSE;
    }

    /* ---------------- */

    if( poLayer->TestCapability( OLCDeleteFeature ) )
    {
        eErr = poLayer->StartTransaction();
        if (eErr != OGRERR_NONE)
        {
            printf("ERROR: StartTransaction() failed.\n");
            return FALSE;
        }

        poFeature = new OGRFeature(poLayer->GetLayerDefn());
        if (poLayer->GetLayerDefn()->GetFieldCount() > 0)
            poFeature->SetField(0, "0");
        eErr = poLayer->CreateFeature(poFeature);
        int nFID = poFeature->GetFID();
        delete poFeature;
        poFeature = NULL;

        if (eErr == OGRERR_FAILURE)
        {
            printf("ERROR: CreateFeature() failed. Exiting this test now.\n");
            poLayer->RollbackTransaction();
            return FALSE;
        }

        eErr = poLayer->CommitTransaction();
        if (eErr != OGRERR_NONE)
        {
            printf("ERROR: CommitTransaction() failed after successfull StartTransaction().\n");
            return FALSE;
        }

        if (poLayer->GetFeatureCount() != nInitialFeatureCount + 1)
        {
            printf("ERROR: GetFeatureCount() should have returned its initial value + 1 after CommitTransaction().\n");
            poLayer->RollbackTransaction();
            return FALSE;
        }

        eErr = poLayer->DeleteFeature(nFID);
        if (eErr != OGRERR_NONE)
        {
            printf("ERROR: DeleteFeature() failed.\n");
            return FALSE;
        }

        if (poLayer->GetFeatureCount() != nInitialFeatureCount)
        {
            printf("ERROR: GetFeatureCount() should have returned its initial value after DeleteFeature().\n");
            poLayer->RollbackTransaction();
            return FALSE;
        }
    }

    /* ---------------- */

    if( bVerbose )
    {
        printf( "INFO: Transactions test passed.\n" );
    }

    return TRUE;
}

/************************************************************************/
/*                     TestOGRLayerIgnoreFields()                       */
/************************************************************************/

static int TestOGRLayerIgnoreFields( OGRLayer* poLayer )
{
    int iFieldNonEmpty = -1;
    int iFieldNonEmpty2 = -1;
    int bGeomNonEmpty = FALSE;
    OGRFeature* poFeature;

    poLayer->ResetReading();
    while( (poFeature = poLayer->GetNextFeature()) != NULL )
    {
        if( iFieldNonEmpty < 0 )
        {
            for(int i=0;i<poFeature->GetFieldCount();i++)
            {
                if( poFeature->IsFieldSet(i) )
                {
                    iFieldNonEmpty = i;
                    break;
                }
            }
        }
        else if ( iFieldNonEmpty2 < 0 )
        {
            for(int i=0;i<poFeature->GetFieldCount();i++)
            {
                if( i != iFieldNonEmpty && poFeature->IsFieldSet(i) )
                {
                    iFieldNonEmpty2 = i;
                    break;
                }
            }
        }

        if( !bGeomNonEmpty && poFeature->GetGeometryRef() != NULL)
            bGeomNonEmpty = TRUE;

        delete poFeature;
    }

    if( iFieldNonEmpty < 0 && bGeomNonEmpty == FALSE )
    {
        if( bVerbose )
        {
            printf( "INFO: IgnoreFields test skipped.\n" );
        }
        return TRUE;
    }

    char** papszIgnoredFields = NULL;
    if( iFieldNonEmpty >= 0 )
        papszIgnoredFields = CSLAddString(papszIgnoredFields,
            poLayer->GetLayerDefn()->GetFieldDefn(iFieldNonEmpty)->GetNameRef());

    if( bGeomNonEmpty )
        papszIgnoredFields = CSLAddString(papszIgnoredFields, "OGR_GEOMETRY");

    OGRErr eErr = poLayer->SetIgnoredFields((const char**)papszIgnoredFields);
    CSLDestroy(papszIgnoredFields);

    if( eErr == OGRERR_FAILURE )
    {
        printf( "ERROR: SetIgnoredFields() failed.\n" );
        poLayer->SetIgnoredFields(NULL);
        return FALSE;
    }

    int bFoundNonEmpty2 = FALSE;

    poLayer->ResetReading();
    while( (poFeature = poLayer->GetNextFeature()) != NULL )
    {
        if( iFieldNonEmpty >= 0 && poFeature->IsFieldSet(iFieldNonEmpty) )
        {
            delete poFeature;
            printf( "ERROR: After SetIgnoredFields(), found a non empty field that should have been ignored.\n" );
            poLayer->SetIgnoredFields(NULL);
            return FALSE;
        }

        if( iFieldNonEmpty2 >= 0 && poFeature->IsFieldSet(iFieldNonEmpty2) )
            bFoundNonEmpty2 = TRUE;

        if( bGeomNonEmpty && poFeature->GetGeometryRef() != NULL)
        {
            delete poFeature;
            printf( "ERROR: After SetIgnoredFields(), found a non empty geometry that should have been ignored.\n" );
            poLayer->SetIgnoredFields(NULL);
            return FALSE;
        }

        delete poFeature;
    }

    if( iFieldNonEmpty2 >= 0 && !bFoundNonEmpty2)
    {
        printf( "ERROR: SetIgnoredFields() discarded fields that it should not have discarded.\n" );
        poLayer->SetIgnoredFields(NULL);
        return FALSE;
    }

    poLayer->SetIgnoredFields(NULL);

    if( bVerbose )
    {
        printf( "INFO: IgnoreFields test passed.\n" );
    }

    return TRUE;
}

/************************************************************************/
/*                            TestLayerSQL()                            */
/************************************************************************/

static int TestLayerSQL( OGRDataSource* poDS, OGRLayer * poLayer )

{
    int bRet = TRUE;
    OGRLayer* poSQLLyr = NULL;
    OGRFeature* poLayerFeat = NULL;
    OGRFeature* poSQLFeat = NULL;

    CPLString osSQL;

    /* Test consistency between result layer and traditionnal layer */
    poLayer->ResetReading();
    poLayerFeat = poLayer->GetNextFeature();

    osSQL.Printf("SELECT * FROM %s", GetLayerNameForSQL(poDS, poLayer->GetName()));
    poSQLLyr = poDS->ExecuteSQL(osSQL.c_str(), NULL, NULL);
    if( poSQLLyr == NULL )
    {
        printf( "ERROR: ExecuteSQL(%s) failed.\n", osSQL.c_str() );
        bRet = FALSE;
    }
    else
    {
        poSQLFeat = poSQLLyr->GetNextFeature();
        if( poLayerFeat == NULL && poSQLFeat != NULL )
        {
            printf( "ERROR: poLayerFeat == NULL && poSQLFeat != NULL.\n" );
            bRet = FALSE;
        }
        else if( poLayerFeat != NULL && poSQLFeat == NULL )
        {
            printf( "ERROR: poLayerFeat != NULL && poSQLFeat == NULL.\n" );
            bRet = FALSE;
        }
        else if( poLayerFeat != NULL && poSQLFeat != NULL )
        {
            if( poLayer->GetLayerDefn()->GetGeomFieldCount() !=
                poSQLLyr->GetLayerDefn()->GetGeomFieldCount() )
            {
                printf( "ERROR: poLayer->GetLayerDefn()->GetGeomFieldCount() != poSQLLyr->GetLayerDefn()->GetGeomFieldCount().\n" );
                bRet = FALSE;
            }
            else
            {
                int nGeomFieldCount = poLayer->GetLayerDefn()->GetGeomFieldCount();
                for(int i = 0; i < nGeomFieldCount; i++ )
                {
                    int iOtherI;
                    if( nGeomFieldCount != 1 )
                    {
                        OGRGeomFieldDefn* poGFldDefn =
                            poLayer->GetLayerDefn()->GetGeomFieldDefn(i);
                        iOtherI = poSQLLyr->GetLayerDefn()->
                            GetGeomFieldIndex(poGFldDefn->GetNameRef());
                        if( iOtherI == -1 )
                        {
                            printf( "ERROR: Cannot find geom field in SQL matching %s.\n",
                                    poGFldDefn->GetNameRef() );
                            break;
                        }
                    }
                    else
                        iOtherI = 0;
                    OGRGeometry* poLayerFeatGeom = poLayerFeat->GetGeomFieldRef(i);
                    OGRGeometry* poSQLFeatGeom = poSQLFeat->GetGeomFieldRef(iOtherI);
                    if( poLayerFeatGeom == NULL && poSQLFeatGeom != NULL )
                    {
                        printf( "ERROR: poLayerFeatGeom[%d] == NULL && poSQLFeatGeom[%d] != NULL.\n",
                                i, iOtherI );
                        bRet = FALSE;
                    }
                    else if( poLayerFeatGeom != NULL && poSQLFeatGeom == NULL )
                    {
                        printf( "ERROR: poLayerFeatGeom[%d] != NULL && poSQLFeatGeom[%d] == NULL.\n",
                                i, iOtherI );
                        bRet = FALSE;
                    }
                    else if( poLayerFeatGeom != NULL && poSQLFeatGeom != NULL )
                    {
                        OGRSpatialReference* poLayerFeatSRS = poLayerFeatGeom->getSpatialReference();
                        OGRSpatialReference* poSQLFeatSRS = poSQLFeatGeom->getSpatialReference();
                        if( poLayerFeatSRS == NULL && poSQLFeatSRS != NULL )
                        {
                            printf( "ERROR: poLayerFeatSRS == NULL && poSQLFeatSRS != NULL.\n" );
                            bRet = FALSE;
                        }
                        else if( poLayerFeatSRS != NULL && poSQLFeatSRS == NULL )
                        {
                            printf( "ERROR: poLayerFeatSRS != NULL && poSQLFeatSRS == NULL.\n" );
                            bRet = FALSE;
                        }
                        else if( poLayerFeatSRS != NULL && poSQLFeatSRS != NULL )
                        {
                            if( !(poLayerFeatSRS->IsSame(poSQLFeatSRS)) )
                            {
                                printf( "ERROR: !(poLayerFeatSRS->IsSame(poSQLFeatSRS)).\n" );
                                bRet = FALSE;
                            }
                        }
                    }
                }
            }
        }
    }

    OGRFeature::DestroyFeature(poLayerFeat);
    poLayerFeat = NULL;
    OGRFeature::DestroyFeature(poSQLFeat);
    poSQLFeat = NULL;
    if( poSQLLyr )
    {
        poDS->ReleaseResultSet(poSQLLyr);
        poSQLLyr = NULL;
    }

    /* Return an empty layer */
    osSQL.Printf("SELECT * FROM %s WHERE 0 = 1", GetLayerNameForSQL(poDS, poLayer->GetName()));

    poSQLLyr = poDS->ExecuteSQL(osSQL.c_str(), NULL, NULL);
    if (poSQLLyr)
    {
        poSQLFeat = poSQLLyr->GetNextFeature();
        if (poSQLFeat != NULL)
        {
            bRet = FALSE;
            printf( "ERROR: ExecuteSQL() should have returned a layer without features.\n" );
        }
        OGRFeature::DestroyFeature(poSQLFeat);
        poDS->ReleaseResultSet(poSQLLyr);
    }
    else
    {
        printf( "ERROR: ExecuteSQL() should have returned a non-NULL result.\n");
        bRet = FALSE;
    }
    
    if( bRet && bVerbose )
        printf("INFO: TestLayerSQL passed.\n");

    return bRet;
}

/************************************************************************/
/*                            TestOGRLayer()                            */
/************************************************************************/

static int TestOGRLayer( OGRDataSource* poDS, OGRLayer * poLayer, int bIsSQLLayer )

{
    int bRet = TRUE;

/* -------------------------------------------------------------------- */
/*      Verify that there is no spatial filter in place by default.     */
/* -------------------------------------------------------------------- */
    if( poLayer->GetSpatialFilter() != NULL )
    {
        printf( "WARN: Spatial filter in place by default on layer %s.\n",
                poLayer->GetName() );
        poLayer->SetSpatialFilter( NULL );
    }

/* -------------------------------------------------------------------- */
/*      Basic tests.                                                   */
/* -------------------------------------------------------------------- */
    bRet &= TestBasic( poLayer );
    
/* -------------------------------------------------------------------- */
/*      Test feature count accuracy.                                    */
/* -------------------------------------------------------------------- */
    bRet &= TestOGRLayerFeatureCount( poDS, poLayer, bIsSQLLayer );

/* -------------------------------------------------------------------- */
/*      Test spatial filtering                                          */
/* -------------------------------------------------------------------- */
    bRet &= TestSpatialFilter( poLayer );

/* -------------------------------------------------------------------- */
/*      Test attribute filtering                                        */
/* -------------------------------------------------------------------- */
    bRet &= TestAttributeFilter( poDS, poLayer );

/* -------------------------------------------------------------------- */
/*      Test GetExtent()                                                */
/* -------------------------------------------------------------------- */
    bRet &= TestGetExtent( poLayer );

/* -------------------------------------------------------------------- */
/*      Test random reading.                                            */
/* -------------------------------------------------------------------- */
    if( poLayer->TestCapability( OLCRandomRead ) )
    {
        bRet &= TestOGRLayerRandomRead( poLayer );
    }
    
/* -------------------------------------------------------------------- */
/*      Test SetNextByIndex.                                            */
/* -------------------------------------------------------------------- */
    if( poLayer->TestCapability( OLCFastSetNextByIndex ) )
    {
        bRet &= TestOGRLayerSetNextByIndex( poLayer );
    }
    
/* -------------------------------------------------------------------- */
/*      Test delete feature.                                            */
/* -------------------------------------------------------------------- */
    if( poLayer->TestCapability( OLCDeleteFeature ) )
    {
        bRet &= TestOGRLayerDeleteAndCreateFeature( poLayer );
    }
    
/* -------------------------------------------------------------------- */
/*      Test random writing.                                            */
/* -------------------------------------------------------------------- */
    if( poLayer->TestCapability( OLCRandomWrite ) )
    {
        bRet &= TestOGRLayerRandomWrite( poLayer );
    }

/* -------------------------------------------------------------------- */
/*      Test OLCIgnoreFields.                                           */
/* -------------------------------------------------------------------- */
    if( poLayer->TestCapability( OLCIgnoreFields ) )
    {
        bRet &= TestOGRLayerIgnoreFields( poLayer );
    }

/* -------------------------------------------------------------------- */
/*      Test UTF-8 reporting                                            */
/* -------------------------------------------------------------------- */
    bRet &= TestOGRLayerUTF8( poLayer );

/* -------------------------------------------------------------------- */
/*      Test TestTransactions()                                         */
/* -------------------------------------------------------------------- */
    if( poLayer->TestCapability( OLCSequentialWrite ) )
    {
        bRet &= TestTransactions( poLayer );
    }

/* -------------------------------------------------------------------- */
/*      Test error conditions.                                          */
/* -------------------------------------------------------------------- */
    bRet &= TestLayerErrorConditions( poLayer );


/* -------------------------------------------------------------------- */
/*      Test some SQL.                                                  */
/* -------------------------------------------------------------------- */
    if( !bIsSQLLayer )
        bRet &= TestLayerSQL( poDS, poLayer );

    return bRet;
}

/************************************************************************/
/*                        TestInterleavedReading()                      */
/************************************************************************/

static int TestInterleavedReading( const char* pszDataSource, char** papszLayers )
{
    int bRet = TRUE;
    OGRDataSource* poDS = NULL;
    OGRDataSource* poDS2 = NULL;
    OGRLayer* poLayer1 = NULL;
    OGRLayer* poLayer2 = NULL;
    OGRFeature* poFeature11_Ref = NULL;
    OGRFeature* poFeature12_Ref = NULL;
    OGRFeature* poFeature21_Ref = NULL;
    OGRFeature* poFeature22_Ref = NULL;
    OGRFeature* poFeature11 = NULL;
    OGRFeature* poFeature12 = NULL;
    OGRFeature* poFeature21 = NULL;
    OGRFeature* poFeature22 = NULL;

    /* Check that we have 2 layers with at least 2 features */
    poDS = OGRSFDriverRegistrar::Open( pszDataSource, FALSE, NULL );
    if (poDS == NULL)
    {
        if( bVerbose )
        {
            printf( "INFO: Skipping TestInterleavedReading(). Cannot reopen datasource\n" );
        }
        goto bye;
    }

    poLayer1 = papszLayers ? poDS->GetLayerByName(papszLayers[0]) : poDS->GetLayer(0);
    poLayer2 = papszLayers ? poDS->GetLayerByName(papszLayers[1]) : poDS->GetLayer(1);
    if (poLayer1 == NULL || poLayer2 == NULL ||
        poLayer1->GetFeatureCount() < 2 || poLayer2->GetFeatureCount() < 2)
    {
        if( bVerbose )
        {
            printf( "INFO: Skipping TestInterleavedReading(). Test conditions are not met\n" );
        }
        goto bye;
    }

    /* Test normal reading */
    OGRDataSource::DestroyDataSource(poDS);
    poDS = OGRSFDriverRegistrar::Open( pszDataSource, FALSE, NULL );
    poDS2 = OGRSFDriverRegistrar::Open( pszDataSource, FALSE, NULL );
    if (poDS == NULL || poDS2 == NULL)
    {
        if( bVerbose )
        {
            printf( "INFO: Skipping TestInterleavedReading(). Cannot reopen datasource\n" );
        }
        goto bye;
    }

    poLayer1 = papszLayers ? poDS->GetLayerByName(papszLayers[0]) : poDS->GetLayer(0);
    poLayer2 = papszLayers ? poDS->GetLayerByName(papszLayers[1]) : poDS->GetLayer(1);
    if (poLayer1 == NULL || poLayer2 == NULL)
    {
        printf( "ERROR: Skipping TestInterleavedReading(). Test conditions are not met\n" );
        bRet = FALSE;
        goto bye;
    }

    poFeature11_Ref = poLayer1->GetNextFeature();
    poFeature12_Ref = poLayer1->GetNextFeature();
    poFeature21_Ref = poLayer2->GetNextFeature();
    poFeature22_Ref = poLayer2->GetNextFeature();
    if (poFeature11_Ref == NULL || poFeature12_Ref == NULL || poFeature21_Ref == NULL || poFeature22_Ref == NULL)
    {
        printf( "ERROR: TestInterleavedReading() failed: poFeature11_Ref=%p, poFeature12_Ref=%p, poFeature21_Ref=%p, poFeature22_Ref=%p\n",
                poFeature11_Ref, poFeature12_Ref, poFeature21_Ref, poFeature22_Ref);
        bRet = FALSE;
        goto bye;
    }

    /* Test interleaved reading */
    poLayer1 = papszLayers ? poDS2->GetLayerByName(papszLayers[0]) : poDS2->GetLayer(0);
    poLayer2 = papszLayers ? poDS2->GetLayerByName(papszLayers[1]) : poDS2->GetLayer(1);
    if (poLayer1 == NULL || poLayer2 == NULL)
    {
        printf( "ERROR: Skipping TestInterleavedReading(). Test conditions are not met\n" );
        bRet = FALSE;
        goto bye;
    }

    poFeature11 = poLayer1->GetNextFeature();
    poFeature21 = poLayer2->GetNextFeature();
    poFeature12 = poLayer1->GetNextFeature();
    poFeature22 = poLayer2->GetNextFeature();

    if (poFeature11 == NULL || poFeature21 == NULL || poFeature12 == NULL || poFeature22 == NULL)
    {
        printf( "ERROR: TestInterleavedReading() failed: poFeature11=%p, poFeature21=%p, poFeature12=%p, poFeature22=%p\n",
                poFeature11, poFeature21, poFeature12, poFeature22);
        bRet = FALSE;
        goto bye;
    }

    if (poFeature12->Equal(poFeature11))
    {
        printf( "WARN: TestInterleavedReading() failed: poFeature12 == poFeature11. "
                "The datasource resets the layer reading when interleaved layer reading pattern is detected. Acceptable but could be improved\n" );
        goto bye;
    }

    /* We cannot directly compare the feature as they don't share */
    /* the same (pointer) layer definition, so just compare FIDs */
    if (poFeature12_Ref->GetFID() != poFeature12->GetFID())
    {
        printf( "ERROR: TestInterleavedReading() failed: poFeature12_Ref != poFeature12\n" );
        poFeature12_Ref->DumpReadable(stdout, NULL);
        poFeature12->DumpReadable(stdout, NULL);
        bRet = FALSE;
        goto bye;
    }

    if( bVerbose )
    {
        printf("INFO: TestInterleavedReading() successfull.\n");
    }

bye:
    OGRFeature::DestroyFeature(poFeature11_Ref);
    OGRFeature::DestroyFeature(poFeature12_Ref);
    OGRFeature::DestroyFeature(poFeature21_Ref);
    OGRFeature::DestroyFeature(poFeature22_Ref);
    OGRFeature::DestroyFeature(poFeature11);
    OGRFeature::DestroyFeature(poFeature21);
    OGRFeature::DestroyFeature(poFeature12);
    OGRFeature::DestroyFeature(poFeature22);
    OGRDataSource::DestroyDataSource(poDS);
    OGRDataSource::DestroyDataSource(poDS2);
    return bRet;
}

/************************************************************************/
/*                          TestDSErrorConditions()                     */
/************************************************************************/

static int TestDSErrorConditions( OGRDataSource * poDS )
{
    int bRet = TRUE;
    OGRLayer* poLyr;

    CPLPushErrorHandler(CPLQuietErrorHandler);

    if (poDS->TestCapability("fake_capability"))
    {
        printf( "ERROR: TestCapability(\"fake_capability\") should have returned FALSE\n" );
        bRet = FALSE;
        goto bye;
    }

    if (poDS->GetLayer(-1) != NULL)
    {
        printf( "ERROR: GetLayer(-1) should have returned NULL\n" );
        bRet = FALSE;
        goto bye;
    }

    if (poDS->GetLayer(poDS->GetLayerCount()) != NULL)
    {
        printf( "ERROR: GetLayer(poDS->GetLayerCount()) should have returned NULL\n" );
        bRet = FALSE;
        goto bye;
    }

    if (poDS->GetLayerByName("non_existing_layer") != NULL)
    {
        printf( "ERROR: GetLayerByName(\"non_existing_layer\") should have returned NULL\n" );
        bRet = FALSE;
        goto bye;
    }

    poLyr = poDS->ExecuteSQL("a fake SQL command", NULL, NULL);
    if (poLyr != NULL)
    {
        poDS->ReleaseResultSet(poLyr);
        printf( "ERROR: ExecuteSQL(\"a fake SQL command\") should have returned NULL\n" );
        bRet = FALSE;
        goto bye;
    }

bye:
    CPLPopErrorHandler();
    return bRet;
}
