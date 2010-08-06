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
#include "ogr_api.h"

CPL_CVSID("$Id$");

int     bReadOnly = FALSE;
int     bVerbose = TRUE;

static void Usage();
static int TestOGRLayer( OGRDataSource * poDS, OGRLayer * poLayer, int bIsSQLLayer );

/************************************************************************/
/*                                main()                                */
/************************************************************************/

int main( int nArgc, char ** papszArgv )

{
    const char  *pszDataSource = NULL;
    char** papszLayers = NULL;
    const char  *pszSQLStatement = NULL;
    int bRet = TRUE;
    
/* -------------------------------------------------------------------- */
/*      Register format(s).                                             */
/* -------------------------------------------------------------------- */
    OGRRegisterAll();
    
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

        exit( 1 );
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
        OGRLayer  *poResultSet = poDS->ExecuteSQL(pszSQLStatement, NULL, NULL);
        if (poResultSet == NULL)
            exit(1);
            
        printf( "INFO: Testing layer %s.\n",
                    poResultSet->GetLayerDefn()->GetName() );
        bRet = TestOGRLayer( poDS, poResultSet, TRUE );
        
        poDS->ReleaseResultSet(poResultSet);
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
                exit( 1 );
            }

            printf( "INFO: Testing layer %s.\n",
                    poLayer->GetLayerDefn()->GetName() );
            bRet &= TestOGRLayer( poDS, poLayer, FALSE );
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
                exit( 1 );
            }
            
            printf( "INFO: Testing layer %s.\n",
                    poLayer->GetLayerDefn()->GetName() );
            bRet &= TestOGRLayer( poDS, poLayer, FALSE );
            
            papszLayerIter ++;
        }
    }

/* -------------------------------------------------------------------- */
/*      Close down.                                                     */
/* -------------------------------------------------------------------- */
    OGRDataSource::DestroyDataSource(poDS);

    OGRCleanupAll();

    CSLDestroy(papszLayers);
    
#ifdef DBMALLOC
    malloc_dump(1);
#endif
    
    return (bRet) ? 0 : 1;
}

/************************************************************************/
/*                               Usage()                                */
/************************************************************************/

static void Usage()

{
    printf( "Usage: test_ogrsf [-ro] [-q] datasource_name [[layer1_name, layer2_name, ...] | [-sql statement]]\n" );
    exit( 1 );
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
    OGRSpatialReference * poSRS = poLayer->GetSpatialRef();
    int         bWarnAboutSRS = FALSE;

    poLayer->ResetReading();

    while( (poFeature = poLayer->GetNextFeature()) != NULL )
    {
        nFC++;

        if( poFeature->GetGeometryRef() != NULL
            && poFeature->GetGeometryRef()->getSpatialReference() != poSRS
            && !bWarnAboutSRS )
        {
            char        *pszLayerSRSWKT, *pszFeatureSRSWKT;
            
            bWarnAboutSRS = TRUE;

            if( poSRS != NULL )
                poSRS->exportToWkt( &pszLayerSRSWKT );
            else
                pszLayerSRSWKT = CPLStrdup("(NULL)");

            if( poFeature->GetGeometryRef()->getSpatialReference() != NULL )
                poFeature->GetGeometryRef()->
                    getSpatialReference()->exportToWkt( &pszFeatureSRSWKT );
            else
                pszFeatureSRSWKT = CPLStrdup("(NULL)");

            bRet = FALSE;
            printf( "ERROR: Feature SRS differs from layer SRS.\n"
                    "Feature SRS = %s\n"
                    "Layer SRS = %s\n",
                    pszFeatureSRSWKT, pszLayerSRSWKT );
            CPLFree( pszLayerSRSWKT );
            CPLFree( pszFeatureSRSWKT );
        }
        
        OGRFeature::DestroyFeature(poFeature);
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
        osSQL.Printf("SELECT COUNT(*) FROM \"%s\"", poLayer->GetLayerDefn()->GetName());
        OGRLayer* poSQLLyr = poDS->ExecuteSQL(osSQL.c_str(), NULL, NULL);
        if (poSQLLyr)
        {
            OGRFeature* poFeatCount = poSQLLyr->GetNextFeature();
            if (nClaimedFC != poFeatCount->GetFieldAsInteger(0))
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
    OGRFeature  *papoFeatures[5], *poFeature;
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
        papoFeatures[iFeature] = poLayer->GetNextFeature();
        CPLAssert( papoFeatures[iFeature] != NULL );
    }

/* -------------------------------------------------------------------- */
/*      Test feature 2.                                                 */
/* -------------------------------------------------------------------- */
    poFeature = poLayer->GetFeature( papoFeatures[1]->GetFID() );
    if( !poFeature->Equal( papoFeatures[1] ) )
    {
        bRet = FALSE;
        printf( "ERROR: Attempt to randomly read feature %ld appears to\n"
                "       have returned a different feature than sequential\n"
                "       reading indicates should have happened.\n",
                papoFeatures[1]->GetFID() );

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
        CPLAssert( papoFeatures[iFeature] != NULL );
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
    if( !poFeature->Equal( papoFeatures[1] ) )
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
    if( !poFeature->Equal( papoFeatures[2] ) )
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
        CPLAssert( papoFeatures[iFeature] != NULL );
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
    if( !poFeature->Equal(papoFeatures[1]) )
    {
        bRet = FALSE;
        printf( "ERROR: Written feature didn't seem to retain value.\n" );
    }
    else
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

/************************************************************************/
/*                         TestSpatialFilter()                          */
/*                                                                      */
/*      This is intended to be a simple test of the spatial             */
/*      filting.  We read the first feature.  Then construct a          */
/*      spatial filter geometry which includes it, install and          */
/*      verify that we get the feature.  Next install a spatial         */
/*      filter that doesn't include this feature, and test again.       */
/************************************************************************/

static int TestSpatialFilter( OGRLayer *poLayer )

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
        printf( "INFO: Skipping Spatial Filter test for %s.\n"
                "      No features in layer.\n",
                poLayer->GetLayerDefn()->GetName() );
        return bRet;
    }

    if( poTargetFeature->GetGeometryRef() == NULL )
    {
        printf( "INFO: Skipping Spatial Filter test for %s,\n"
                "      target feature has no geometry.\n",
                poTargetFeature->GetDefnRef()->GetName() );
        OGRFeature::DestroyFeature(poTargetFeature);
        return bRet;
    }

    poTargetFeature->GetGeometryRef()->getEnvelope( &sEnvelope );

/* -------------------------------------------------------------------- */
/*      Construct inclusive filter.                                     */
/* -------------------------------------------------------------------- */
    
    oRing.setPoint( 0, sEnvelope.MinX - 20.0, sEnvelope.MinY - 20.0 );
    oRing.setPoint( 1, sEnvelope.MinX - 20.0, sEnvelope.MaxY + 10.0 );
    oRing.setPoint( 2, sEnvelope.MaxX + 10.0, sEnvelope.MaxY + 10.0 );
    oRing.setPoint( 3, sEnvelope.MaxX + 10.0, sEnvelope.MinY - 20.0 );
    oRing.setPoint( 4, sEnvelope.MinX - 20.0, sEnvelope.MinY - 20.0 );
    
    oInclusiveFilter.addRing( &oRing );

    poLayer->SetSpatialFilter( &oInclusiveFilter );

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
        printf( "ERROR: Spatial filter eliminated a feature unexpectedly!\n");
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

    poLayer->SetSpatialFilter( &oExclusiveFilter );

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
        printf( "ERROR: Spatial filter failed to eliminate"
                "a feature unexpectedly!\n");
    }
    else if( poLayer->GetFeatureCount() >= nInclusiveCount )
    {
        bRet = FALSE;
        printf( "ERROR: GetFeatureCount() may not be taking spatial "
                "filter into account.\n" );
    }
    else if( bVerbose )
    {
        printf( "INFO: Spatial filter exclusion seems to work.\n" );
    }

    OGRFeature::DestroyFeature(poTargetFeature);

    poLayer->SetSpatialFilter( NULL );

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
                poLayer->GetLayerDefn()->GetName() );
        poLayer->SetSpatialFilter( NULL );
    }

/* -------------------------------------------------------------------- */
/*      Test feature count accuracy.                                    */
/* -------------------------------------------------------------------- */
    bRet &= TestOGRLayerFeatureCount( poDS, poLayer, bIsSQLLayer );

/* -------------------------------------------------------------------- */
/*      Test spatial filtering                                          */
/* -------------------------------------------------------------------- */
    bRet &= TestSpatialFilter( poLayer );

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
/*      Test random writing.                                            */
/* -------------------------------------------------------------------- */
    if( poLayer->TestCapability( OLCRandomWrite ) )
    {
        bRet &= TestOGRLayerRandomWrite( poLayer );
    }

    return bRet;
}
