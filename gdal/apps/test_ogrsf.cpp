/******************************************************************************
 * $Id$
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  Formal test harness for OGRLayer implementations.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 1999, Frank Warmerdam
 * Copyright (c) 2009-2014, Even Rouault <even dot rouault at mines-paris dot org>
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

#include <limits>

CPL_CVSID("$Id$");

int     bReadOnly = FALSE;
int     bVerbose = TRUE;
const char  *pszDataSource = NULL;
char** papszLayers = NULL;
const char  *pszSQLStatement = NULL;
const char  *pszDialect = NULL;
int nLoops = 1;
int     bFullSpatialFilter = FALSE;
char  **papszOpenOptions = NULL;
const char* pszDriver = NULL;
int bAllDrivers = FALSE;
const char* pszLogFilename = NULL;
char** papszDSCO = NULL;
char** papszLCO = NULL;

typedef struct
{
    CPLJoinableThread* hThread;
    int bRet;
} ThreadContext;

static void Usage();
static void ThreadFunction( void* user_data );
static void ThreadFunctionInternal( ThreadContext* psContext );
static int TestDataset( GDALDriver** ppoDriver );
static int TestCreate( GDALDriver* poDriver, int bFromAllDrivers );
static int TestOGRLayer( GDALDataset * poDS, OGRLayer * poLayer, int bIsSQLLayer );
static int TestInterleavedReading( const char* pszDataSource, char** papszLayers );
static int TestDSErrorConditions( GDALDataset * poDS );
static int TestVirtualIO( GDALDataset* poDS );
static const char* Log(const char* pszMsg, int nLineNumber);

static const char* Log(const char* pszMsg, int nLineNumber)
{
    if( pszLogFilename == NULL )
        return pszMsg;
    FILE* f = fopen(pszLogFilename, "at");
    if( f == NULL )
        return pszMsg;
    fprintf(f, "%d: %s\n", nLineNumber, pszMsg);
    fclose(f);
    return pszMsg;
}

#define LOG_STR(str) (Log((str), __LINE__))
#define LOG_ACTION(action) (Log(#action, __LINE__), (action))

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
            CSLDestroy(papszArgv);
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
        else if( EQUAL(papszArgv[iArg],"-fsf") )
            bFullSpatialFilter = TRUE;
        else if( EQUAL(papszArgv[iArg], "-oo") && iArg + 1 < nArgc)
        {
            papszOpenOptions = CSLAddString( papszOpenOptions,
                                                papszArgv[++iArg] );
        }
        else if( EQUAL(papszArgv[iArg], "-dsco") && iArg + 1 < nArgc)
        {
            papszDSCO = CSLAddString( papszDSCO,
                                                papszArgv[++iArg] );
        }
        else if( EQUAL(papszArgv[iArg], "-lco") && iArg + 1 < nArgc)
        {
            papszLCO = CSLAddString( papszLCO,
                                                papszArgv[++iArg] );
        }
        else if( EQUAL(papszArgv[iArg], "-log") && iArg + 1 < nArgc)
        {
            pszLogFilename = papszArgv[++iArg];
        }
        else if( EQUAL(papszArgv[iArg], "-driver") && iArg + 1 < nArgc)
        {
            pszDriver = papszArgv[++iArg];
        }
        else if( EQUAL(papszArgv[iArg],"-all_drivers") )
            bAllDrivers = TRUE;
        else if( papszArgv[iArg][0] == '-' )
        {
            Usage();
        }
        else if (pszDataSource == NULL)
            pszDataSource = papszArgv[iArg];
        else
            papszLayers = CSLAddString(papszLayers, papszArgv[iArg]);
    }

    if( pszDataSource == NULL && pszDriver == NULL && !bAllDrivers )
        Usage();
    if( nThreads > 1 && !bReadOnly && pszDataSource != NULL )
    {
        fprintf(stderr, "-threads must be used with -ro or -driver/-all_drivers option.\n");
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
    CSLDestroy(papszOpenOptions);
    CSLDestroy(papszDSCO);
    CSLDestroy(papszLCO);
    
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
#ifdef __AFL_HAVE_MANUAL_CONTROL
    while (__AFL_LOOP(1000)) {
#endif
    for( int iLoop = 0; psContext->bRet && iLoop < nLoops; iLoop ++ )
    {
        ThreadFunctionInternal(psContext);
    }
#ifdef __AFL_HAVE_MANUAL_CONTROL
    }
#endif
}

/************************************************************************/
/*                     ThreadFunctionInternal()                         */
/************************************************************************/

static void ThreadFunctionInternal( ThreadContext* psContext )

{
    int bRet = TRUE;

    GDALDriver         *poDriver = NULL;

    if( pszDataSource != NULL )
        bRet = TestDataset( &poDriver );
    else if( pszDriver != NULL )
    {
        poDriver = (GDALDriver*) GDALGetDriverByName(pszDriver);
        if( poDriver )
            bRet &= TestCreate( poDriver, FALSE );
        else
        {
            printf("ERROR: Cannot find driver %s\n", pszDriver);
            bRet = FALSE;
        }
    }
    else
    {
        int nCount = GDALGetDriverCount();
        for(int i=0;i<nCount;i++)
        {
            poDriver = (GDALDriver*) GDALGetDriver(i);
            if( poDriver->GetMetadataItem(GDAL_DCAP_VECTOR) != NULL )
                bRet &= TestCreate( poDriver, TRUE );
        }
    }

    psContext->bRet = bRet;
}

/************************************************************************/
/*                            TestDataset()                             */
/************************************************************************/

static int TestDataset( GDALDriver** ppoDriver )
{
    int bRet = TRUE;
    int bRetLocal;

/* -------------------------------------------------------------------- */
/*      Open data source.                                               */
/* -------------------------------------------------------------------- */
    GDALDataset        *poDS;
    GDALDriver         *poDriver = NULL;

    poDS = (GDALDataset*) GDALOpenEx( pszDataSource,
            (!bReadOnly ? GDAL_OF_UPDATE : GDAL_OF_READONLY) | GDAL_OF_VECTOR,
            NULL, papszOpenOptions, NULL );
    if( poDS == NULL && !bReadOnly )
    {
        poDS = (GDALDataset*) GDALOpenEx( pszDataSource, GDAL_OF_VECTOR,
                                          NULL, papszOpenOptions, NULL );
        if( poDS != NULL && bVerbose )
        {
            printf( "Had to open data source read-only.\n" );
            bReadOnly = TRUE;
        }
    }
    if( poDS != NULL )
        poDriver = poDS->GetDriver();
    *ppoDriver = poDriver;

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
            printf( "  -> %s\n", poR->GetDriver(iDriver)->GetDescription() );
        }

        return FALSE;
    }

/* -------------------------------------------------------------------- */
/*      Some information messages.                                      */
/* -------------------------------------------------------------------- */
    if( bVerbose )
        printf( "INFO: Open of `%s' using driver `%s' successful.\n",
                pszDataSource, poDriver->GetDescription() );

    if( bVerbose && !EQUAL(pszDataSource,poDS->GetDescription()) )
    {
        printf( "INFO: Internal data source name `%s'\n"
                "      different from user name `%s'.\n",
                poDS->GetDescription(), pszDataSource );
    }

/* -------------------------------------------------------------------- */
/*      Process optional SQL request.                                   */
/* -------------------------------------------------------------------- */
    if (pszSQLStatement != NULL)
    {
        OGRLayer *poResultSet
            = poDS->ExecuteSQL(pszSQLStatement, NULL, pszDialect);
        if (poResultSet == NULL)
        {
            GDALClose( (GDALDatasetH)poDS );
            return FALSE;
        }

        if( bVerbose )
        {
            printf( "INFO: Testing layer %s.\n",
                        poResultSet->GetName() );
        }
        bRet = TestOGRLayer( poDS, poResultSet, TRUE );

        poDS->ReleaseResultSet(poResultSet);

        bRetLocal = TestDSErrorConditions(poDS);
        bRet &= TestDSErrorConditions(poDS);

        bRetLocal = TestVirtualIO(poDS);
        bRet &= bRetLocal;
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
                GDALClose( (GDALDatasetH)poDS );
                return FALSE;
            }

            if( bVerbose )
            {
                printf( "INFO: Testing layer %s.\n",
                        poLayer->GetName() );
            }
            bRet &= TestOGRLayer( poDS, poLayer, FALSE );
        }

        bRetLocal = TestDSErrorConditions(poDS);
        bRet &= TestDSErrorConditions(poDS);

        bRetLocal = TestVirtualIO(poDS);
        bRet &= bRetLocal;

        if (poDS->GetLayerCount() >= 2)
        {
            GDALClose( (GDALDatasetH)poDS );
            poDS = NULL;
            bRetLocal = TestInterleavedReading( pszDataSource, NULL );
            bRet &= bRetLocal;
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
                GDALClose( (GDALDatasetH)poDS );
                return FALSE;
            }
            
            if( bVerbose )
            {
                printf( "INFO: Testing layer %s.\n",
                        poLayer->GetName() );
            }
            bRet &= TestOGRLayer( poDS, poLayer, FALSE );
            
            papszLayerIter ++;
        }

        bRetLocal = TestDSErrorConditions(poDS);
        bRet &= TestDSErrorConditions(poDS);

        bRetLocal = TestVirtualIO(poDS);
        bRet &= bRetLocal;

        if (CSLCount(papszLayers) >= 2)
        {
            GDALClose( (GDALDatasetH)poDS );
            poDS = NULL;
            bRetLocal = TestInterleavedReading( pszDataSource, papszLayers );
            bRet &= bRetLocal;
        }
    }

/* -------------------------------------------------------------------- */
/*      Close down.                                                     */
/* -------------------------------------------------------------------- */
    if( poDS != NULL )
        GDALClose( (GDALDatasetH)poDS );

    return bRet;
}

/************************************************************************/
/*                             GetWKT()                                 */
/************************************************************************/

static const char* GetWKT(OGRwkbGeometryType eGeomType)
{
    const char* pszWKT = NULL;
    if( eGeomType == wkbUnknown || eGeomType == wkbPoint )
        pszWKT = "POINT (0 0)";
    else if( eGeomType == wkbLineString )
        pszWKT = "LINESTRING (0 0,1 1)";
    else if( eGeomType == wkbPolygon )
        pszWKT = "POLYGON ((0 0,0 1,1 1,1 0,0 0))";
    else if( eGeomType == wkbMultiPoint )
        pszWKT = "MULTIPOINT (0 0)";
    else if( eGeomType == wkbMultiLineString )
        pszWKT = "MULTILINESTRING ((0 0,1 1))";
    else if( eGeomType == wkbMultiPolygon )
        pszWKT = "MULTIPOLYGON (((0 0,0 1,1 1,1 0,0 0)))";
    else if( eGeomType == wkbGeometryCollection )
        pszWKT = "GEOMETRYCOLLECTION (POINT (0 0),LINESTRING (0 0,1 1),POLYGON ((0 0,0 1,1 1,1 0,0 0)))";
    else if( eGeomType == wkbPoint25D )
        pszWKT = "POINT (0 0 10)";
    else if( eGeomType == wkbLineString25D )
        pszWKT = "LINESTRING (0 0 10,1 1 10)";
    else if( eGeomType == wkbPolygon25D )
        pszWKT = "POLYGON ((0 0 10,0 1 10,1 1 10,1 0 10,0 0 10))";
    else if( eGeomType == wkbMultiPoint25D )
        pszWKT = "MULTIPOINT (0 0 10)";
    else if( eGeomType == wkbMultiLineString25D )
        pszWKT = "MULTILINESTRING ((0 0 10,1 1 10))";
    else if( eGeomType == wkbMultiPolygon25D )
        pszWKT = "MULTIPOLYGON (((0 0 10,0 1 10,1 1 10,1 0 10,0 0 10)))";
    else if( eGeomType == wkbGeometryCollection25D )
        pszWKT = "GEOMETRYCOLLECTION (POINT (0 0 10),LINESTRING (0 0 10,1 1 10),POLYGON ((0 0 10,0 1 10,1 1 10,1 0 10,0 0 10)))";
    return pszWKT;
}

/************************************************************************/
/*                         TestCreateLayer()                            */
/************************************************************************/

static int TestCreateLayer( GDALDriver* poDriver, OGRwkbGeometryType eGeomType )
{
    int bRet = TRUE;
    const char* pszExt = poDriver->GetMetadataItem(GDAL_DMD_EXTENSION);

    static int nCounter = 0;
    CPLString osFilename = CPLFormFilename("/vsimem", CPLSPrintf("test%d", ++nCounter), pszExt);
    GDALDataset* poDS = LOG_ACTION(poDriver->Create(osFilename, 0, 0, 0, GDT_Unknown, papszDSCO));
    if( poDS == NULL )
    {
        if( bVerbose )
            printf("INFO: %s: Creation of %s failed.\n",
                   poDriver->GetDescription(), osFilename.c_str());
        return bRet;
    }
    CPLPushErrorHandler(CPLQuietErrorHandler);
    int bCreateLayerCap = LOG_ACTION(poDS->TestCapability(ODsCCreateLayer));
    OGRLayer* poLayer = LOG_ACTION(poDS->CreateLayer(CPLGetFilename(osFilename), NULL, eGeomType, papszLCO));
    CPLPopErrorHandler();
    CPLString osLayerNameToTest;
    OGRwkbGeometryType eExpectedGeomType = wkbUnknown;
    if( poLayer != NULL )
    {
        if( bCreateLayerCap == FALSE )
        {
            printf("ERROR: %s: TestCapability(ODsCCreateLayer) returns FALSE whereas layer creation was successful.\n",
                   poDriver->GetDescription());
            bRet = FALSE;
        }
        
        if( LOG_ACTION(poLayer->GetLayerDefn()) == NULL )
        {
            printf("ERROR: %s: GetLayerDefn() returns NUL just after layer creation.\n",
                   poDriver->GetDescription());
            bRet = FALSE;
        }
        
        // Create fields of various types
        int bCreateField = LOG_ACTION(poLayer->TestCapability(OLCCreateField));
        int iFieldStr = -1, iFieldInt = -1, iFieldReal = -1, iFieldDate = -1, iFieldDateTime = -1;
        int bStrFieldOK;
        {
            OGRFieldDefn oFieldStr("str", OFTString);
            CPLPushErrorHandler(CPLQuietErrorHandler);
            bStrFieldOK = (LOG_ACTION(poLayer->CreateField(&oFieldStr)) == OGRERR_NONE);
            CPLPopErrorHandler();
            if( bStrFieldOK && (iFieldStr = LOG_ACTION(poLayer->GetLayerDefn())->GetFieldIndex("str")) < 0 )
            {
                printf("ERROR: %s: CreateField(str) returned OK but field was not created.\n",
                    poDriver->GetDescription());
                bRet = FALSE;
            }
        }

        OGRFieldDefn oFieldInt("int", OFTInteger);
        CPLPushErrorHandler(CPLQuietErrorHandler);
        int bIntFieldOK = (LOG_ACTION(poLayer->CreateField(&oFieldInt)) == OGRERR_NONE);
        CPLPopErrorHandler();
        if( bIntFieldOK && (iFieldInt = poLayer->GetLayerDefn()->GetFieldIndex("int")) < 0 )
        {
            printf("ERROR: %s: CreateField(int) returned OK but field was not created.\n",
                   poDriver->GetDescription());
            bRet = FALSE;
        }

        OGRFieldDefn oFieldReal("real", OFTReal);
        CPLPushErrorHandler(CPLQuietErrorHandler);
        int bRealFieldOK = (LOG_ACTION(poLayer->CreateField(&oFieldReal)) == OGRERR_NONE);
        CPLPopErrorHandler();
        if( bRealFieldOK && (iFieldReal = poLayer->GetLayerDefn()->GetFieldIndex("real")) < 0 )
        {
            printf("ERROR: %s: CreateField(real) returned OK but field was not created.\n",
                   poDriver->GetDescription());
            bRet = FALSE;
        }

        OGRFieldDefn oFieldDate("date", OFTDate);
        CPLPushErrorHandler(CPLQuietErrorHandler);
        int bDateFieldOK = (LOG_ACTION(poLayer->CreateField(&oFieldDate)) == OGRERR_NONE);
        CPLPopErrorHandler();
        if( bDateFieldOK && (iFieldDate = poLayer->GetLayerDefn()->GetFieldIndex("date")) < 0 )
        {
            printf("ERROR: %s: CreateField(date) returned OK but field was not created.\n",
                   poDriver->GetDescription());
            bRet = FALSE;
        }

        OGRFieldDefn oFieldDateTime("datetime", OFTDateTime);
        CPLPushErrorHandler(CPLQuietErrorHandler);
        int bDateTimeFieldOK = (LOG_ACTION(poLayer->CreateField(&oFieldDateTime)) == OGRERR_NONE);
        CPLPopErrorHandler();
        if( bDateTimeFieldOK && (iFieldDateTime = poLayer->GetLayerDefn()->GetFieldIndex("datetime")) < 0 )
        {
            printf("ERROR: %s: CreateField(datetime) returned OK but field was not created.\n",
                   poDriver->GetDescription());
            bRet = FALSE;
        }

        if( bCreateField == FALSE &&
            (bStrFieldOK || bIntFieldOK || bRealFieldOK || bDateFieldOK || bDateTimeFieldOK) )
        {
            printf("ERROR: %s: TestCapability(OLCCreateField) returns FALSE.\n",
                   poDriver->GetDescription());
            bRet = FALSE;
        }

        if( LOG_ACTION(poLayer->TestCapability(OLCSequentialWrite)) == FALSE )
        {
            printf("ERROR: %s: TestCapability(OLCSequentialWrite) returns FALSE.\n",
                   poDriver->GetDescription());
            bRet = FALSE;
        }

        OGRFeature* poFeature;
        OGRErr eErr;

        /* Test creating empty feature */
        poFeature = new OGRFeature( poLayer->GetLayerDefn() );
        CPLErrorReset();
        CPLPushErrorHandler(CPLQuietErrorHandler);
        eErr = LOG_ACTION(poLayer->CreateFeature(poFeature));
        CPLPopErrorHandler();
        if( eErr != OGRERR_NONE && CPLGetLastErrorType() == 0 )
        {
            printf("INFO: %s: CreateFeature() at line %d failed but without explicit error.\n",
                   poDriver->GetDescription(), __LINE__);
        }
        if( eErr == OGRERR_NONE && poFeature->GetFID() < 0 && eGeomType == wkbUnknown )
        {
            printf("INFO: %s: CreateFeature() at line %d succeeded but failed to assign FID to feature.\n",
                   poDriver->GetDescription(), __LINE__);
        }
        delete poFeature;

        /* Test creating feature with all fields set */
        poFeature = new OGRFeature( poLayer->GetLayerDefn() );
        if( bStrFieldOK )
            poFeature->SetField(iFieldStr, "foo");
        if( bIntFieldOK )
            poFeature->SetField(iFieldInt, 123);
        if( bRealFieldOK )
            poFeature->SetField(iFieldReal, 1.23);
        if( bDateFieldOK )
            poFeature->SetField(iFieldDate, "2014/10/20");
        if( bDateTimeFieldOK )
            poFeature->SetField(iFieldDateTime, "2014/10/20 12:34:56");
        CPLErrorReset();
        CPLPushErrorHandler(CPLQuietErrorHandler);
        eErr = LOG_ACTION(poLayer->CreateFeature(poFeature));
        CPLPopErrorHandler();
        if( eErr != OGRERR_NONE && CPLGetLastErrorType() == 0 )
        {
            printf("INFO: %s: CreateFeature() at line %d failed but without explicit error.\n",
                   poDriver->GetDescription(), __LINE__);
        }
        delete poFeature;

        /* Test creating feature with all fields set as well as geometry */
        poFeature = new OGRFeature( poLayer->GetLayerDefn() );
        if( bStrFieldOK )
            poFeature->SetField(iFieldStr, "foo");
        if( bIntFieldOK )
            poFeature->SetField(iFieldInt, 123);
        if( bRealFieldOK )
            poFeature->SetField(iFieldReal, 1.23);
        if( bDateFieldOK )
            poFeature->SetField(iFieldDate, "2014/10/20");
        if( bDateTimeFieldOK )
            poFeature->SetField(iFieldDateTime, "2014/10/20 12:34:56");

        const char* pszWKT = GetWKT(eGeomType);
        if( pszWKT != NULL )
        {
            OGRGeometry* poGeom = NULL;
            OGRGeometryFactory::createFromWkt( (char**) &pszWKT, NULL, &poGeom);
            poFeature->SetGeometryDirectly(poGeom);
        }

        CPLErrorReset();
        CPLPushErrorHandler(CPLQuietErrorHandler);
        eErr = LOG_ACTION(poLayer->CreateFeature(poFeature));
        CPLPopErrorHandler();
        if( eErr != OGRERR_NONE && CPLGetLastErrorType() == 0 )
        {
            printf("INFO: %s: CreateFeature() at line %d failed but without explicit error.\n",
                   poDriver->GetDescription(), __LINE__);
        }
        delete poFeature;
        
        /* Test feature with incompatible geometry */
        poFeature = new OGRFeature( poLayer->GetLayerDefn() );
        if( bStrFieldOK )
            poFeature->SetField(iFieldStr, "foo");
        if( bIntFieldOK )
            poFeature->SetField(iFieldInt, 123);
        if( bRealFieldOK )
            poFeature->SetField(iFieldReal, 1.23);
        if( bDateFieldOK )
            poFeature->SetField(iFieldDate, "2014/10/20");
        if( bDateTimeFieldOK )
            poFeature->SetField(iFieldDateTime, "2014/10/20 12:34:56");

        OGRwkbGeometryType eOtherGeomType;
        if (eGeomType == wkbUnknown || eGeomType == wkbNone)
            eOtherGeomType = wkbLineString;
        else if( wkbFlatten(eGeomType) == eGeomType )
            eOtherGeomType = (OGRwkbGeometryType) ( ((int)eGeomType % 7) + 1 );
        else
            eOtherGeomType = wkbSetZ((OGRwkbGeometryType) ( (((int)wkbFlatten(eGeomType) % 7) + 1 )));
        pszWKT = GetWKT(eOtherGeomType);
        if( pszWKT != NULL )
        {
            OGRGeometry* poGeom = NULL;
            OGRGeometryFactory::createFromWkt( (char**) &pszWKT, NULL, &poGeom);
            poFeature->SetGeometryDirectly(poGeom);
        }

        CPLErrorReset();
        CPLPushErrorHandler(CPLQuietErrorHandler);
        eErr = LOG_ACTION(poLayer->CreateFeature(poFeature));
        CPLPopErrorHandler();
        if( eErr != OGRERR_NONE && CPLGetLastErrorType() == 0 )
        {
            printf("INFO: %s: CreateFeature() at line %d failed but without explicit error.\n",
                   poDriver->GetDescription(), __LINE__);
        }
        delete poFeature;

        /* Test reading a feature: write-only layers might not like this */
        CPLPushErrorHandler(CPLQuietErrorHandler);
        LOG_ACTION(poLayer->ResetReading());
        delete LOG_ACTION(poLayer->GetNextFeature());
        CPLPopErrorHandler();
        
        osLayerNameToTest = poLayer->GetName();
        eExpectedGeomType = poLayer->GetGeomType();

        /* Some drivers don't like more than one layer per dataset */
        CPLPushErrorHandler(CPLQuietErrorHandler);
        int bCreateLayerCap2 = LOG_ACTION(poDS->TestCapability(ODsCCreateLayer));
        OGRLayer* poLayer2 = LOG_ACTION(poDS->CreateLayer(CPLSPrintf("%s2",CPLGetFilename(osFilename)), NULL, eGeomType));
        CPLPopErrorHandler();
        if( poLayer2 == NULL && bCreateLayerCap2 )
        {
            printf("INFO: %s: Creation of second layer failed but TestCapability(ODsCCreateLayer) succeeded.\n",
                   poDriver->GetDescription());
        }
        else if( !EQUAL(poDriver->GetDescription(), "CSV") && poLayer2 != NULL )
        {
            OGRFieldDefn oFieldStr("str", OFTString);
            CPLPushErrorHandler(CPLQuietErrorHandler);
            LOG_ACTION(poLayer2->CreateField(&oFieldStr));
            CPLPopErrorHandler();

            poFeature = new OGRFeature( poLayer2->GetLayerDefn() );
            pszWKT = GetWKT(eGeomType);
            if( pszWKT != NULL )
            {
                OGRGeometry* poGeom = NULL;
                OGRGeometryFactory::createFromWkt( (char**) &pszWKT, NULL, &poGeom);
                poFeature->SetGeometryDirectly(poGeom);
            }
            CPLErrorReset();
            CPLPushErrorHandler(CPLQuietErrorHandler);
            eErr = LOG_ACTION(poLayer2->CreateFeature(poFeature));
            CPLPopErrorHandler();
            delete poFeature;

            if( eErr == OGRERR_NONE )
            {
                osLayerNameToTest = poLayer2->GetName();
                eExpectedGeomType = poLayer2->GetGeomType();
            }
        }

        /* Test deleting first layer */
        int bDeleteLayerCap = LOG_ACTION(poDS->TestCapability(ODsCDeleteLayer));
        CPLPushErrorHandler(CPLQuietErrorHandler);
        eErr = LOG_ACTION(poDS->DeleteLayer(0));
        CPLPopErrorHandler();
        if( eErr == OGRERR_NONE )
        {
            if( !bDeleteLayerCap )
            {
                printf("ERROR: %s: TestCapability(ODsCDeleteLayer) returns FALSE but layer deletion worked.\n",
                   poDriver->GetDescription());
                bRet = FALSE;
            }
            
            if( LOG_ACTION(poDS->GetLayerByName(CPLGetFilename(osFilename))) != NULL )
            {
                printf("ERROR: %s: DeleteLayer() declared success, but layer can still be fetched.\n",
                   poDriver->GetDescription());
                bRet = FALSE;
            }
        }
        else
        {
            if( bDeleteLayerCap )
            {
                printf("ERROR: %s: TestCapability(ODsCDeleteLayer) returns TRUE but layer deletion failed.\n",
                   poDriver->GetDescription());
                bRet = FALSE;
            }
        }
    }
    /*else
    {
        if( bVerbose )
            printf("INFO: %s: Creation of layer with geom_type = %s failed.\n",
                   poDriver->GetDescription(), OGRGeometryTypeToName(eGeomType));
    }*/
    LOG_ACTION(GDALClose(poDS));

    if( eExpectedGeomType != wkbUnknown &&
        /* Those drivers are expected not to store a layer geometry type */
        !EQUAL(poDriver->GetDescription(), "KML") &&
        !EQUAL(poDriver->GetDescription(), "LIBKML") &&
        !EQUAL(poDriver->GetDescription(), "PDF") )
    {
        /* Reopen dataset */
        poDS = LOG_ACTION((GDALDataset*)GDALOpenEx( osFilename,
                                                    GDAL_OF_VECTOR,
                                                    NULL, NULL, NULL ));
        if( poDS != NULL )
        {
            poLayer = LOG_ACTION(poDS->GetLayerByName(osLayerNameToTest));
            if( poLayer != NULL )
            {
                if( poLayer->GetGeomType() != eExpectedGeomType )
                {
                    printf( "ERROR: %s: GetGeomType() returns %d but %d "
                            "was expected (and %d originally set).\n",
                            poDriver->GetDescription(), poLayer->GetGeomType(),
                            eExpectedGeomType, eGeomType);
                    bRet = FALSE;
                }
            }
            LOG_ACTION(GDALClose(poDS));
        }
    }

    CPLPushErrorHandler(CPLQuietErrorHandler);
    LOG_ACTION(poDriver->Delete(osFilename));
    CPLPopErrorHandler();
    VSIUnlink(osFilename);

    if( poLayer != NULL )
    {
        /* Test creating empty layer */
        osFilename = CPLFormFilename("/vsimem", CPLSPrintf("test%d", ++nCounter), pszExt);
        poDS = LOG_ACTION(poDriver->Create(osFilename, 0, 0, 0, GDT_Unknown, NULL));
        if( poDS != NULL )
        {
            CPLPushErrorHandler(CPLQuietErrorHandler);
            poLayer = LOG_ACTION(poDS->CreateLayer(CPLGetFilename(osFilename), NULL, eGeomType));
            CPLPopErrorHandler();
            LOG_ACTION(GDALClose(poDS));
        
            CPLPushErrorHandler(CPLQuietErrorHandler);
            LOG_ACTION(poDriver->Delete(osFilename));
            CPLPopErrorHandler();
            VSIUnlink(osFilename);
        }
    }

    return bRet;
}

/************************************************************************/
/*                           TestCreate()                               */
/************************************************************************/

static int TestCreate( GDALDriver* poDriver, int bFromAllDrivers )
{
    int bRet = TRUE;
    int bVirtualIO =  poDriver->GetMetadataItem(GDAL_DCAP_VIRTUALIO) != NULL;
    if( poDriver->GetMetadataItem(GDAL_DCAP_CREATE) == NULL || !bVirtualIO)
    {
        if( bVerbose && !bFromAllDrivers )
            printf("INFO: %s: TestCreate skipped.\n", poDriver->GetDescription());
        return TRUE;
    }

    printf("%s\n", LOG_STR(CPLSPrintf("INFO: TestCreate(%s).", poDriver->GetDescription())));

    const char* pszExt = poDriver->GetMetadataItem(GDAL_DMD_EXTENSION);
    CPLString osFilename = CPLFormFilename("/foo", "test", pszExt);
    CPLPushErrorHandler(CPLQuietErrorHandler);
    GDALDataset* poDS = LOG_ACTION(poDriver->Create(osFilename, 0, 0, 0, GDT_Unknown, NULL));
    CPLPopErrorHandler();
    if( poDS != NULL )
    {
        /* Sometimes actual file creation is differed */
        CPLPushErrorHandler(CPLQuietErrorHandler);
        OGRLayer* poLayer = LOG_ACTION(poDS->CreateLayer("test", NULL, wkbPoint));
        CPLPopErrorHandler();

        /* Or sometimes writing is differed at dataset closing */
        CPLErrorReset();
        CPLPushErrorHandler(CPLQuietErrorHandler);
        LOG_ACTION(GDALClose(poDS));
        CPLPopErrorHandler();
        if( poLayer != NULL && CPLGetLastErrorType() == 0 )
        {
            printf("INFO: %s: Creation of %s should have failed.\n",
                poDriver->GetDescription(), osFilename.c_str());
        }
    }

    bRet &= LOG_ACTION(TestCreateLayer(poDriver, wkbUnknown));
    bRet &= LOG_ACTION(TestCreateLayer(poDriver, wkbNone));
    bRet &= LOG_ACTION(TestCreateLayer(poDriver, wkbPoint));
    bRet &= LOG_ACTION(TestCreateLayer(poDriver, wkbLineString));
    bRet &= LOG_ACTION(TestCreateLayer(poDriver, wkbPolygon));
    bRet &= LOG_ACTION(TestCreateLayer(poDriver, wkbMultiPoint));
    bRet &= LOG_ACTION(TestCreateLayer(poDriver, wkbMultiLineString));
    bRet &= LOG_ACTION(TestCreateLayer(poDriver, wkbMultiPolygon));
    bRet &= LOG_ACTION(TestCreateLayer(poDriver, wkbGeometryCollection));
    bRet &= LOG_ACTION(TestCreateLayer(poDriver, wkbPoint25D));
    bRet &= LOG_ACTION(TestCreateLayer(poDriver, wkbLineString25D));
    bRet &= LOG_ACTION(TestCreateLayer(poDriver, wkbPolygon25D));
    bRet &= LOG_ACTION(TestCreateLayer(poDriver, wkbMultiPoint25D));
    bRet &= LOG_ACTION(TestCreateLayer(poDriver, wkbMultiLineString25D));
    bRet &= LOG_ACTION(TestCreateLayer(poDriver, wkbMultiPolygon25D));
    bRet &= LOG_ACTION(TestCreateLayer(poDriver, wkbGeometryCollection25D));

    return bRet;
}

/************************************************************************/
/*                               Usage()                                */
/************************************************************************/

static void Usage()

{
    printf( "Usage: test_ogrsf [-ro] [-q] [-threads N] [-loops M] [-fsf]\n"
            "                  (datasource_name | [-driver driver_name] [[-dsco NAME=VALUE] ...] [[-lco NAME=VALUE] ...] | -all_drivers) \n"
            "                  [[layer1_name, layer2_name, ...] | [-sql statement] [-dialect dialect]]\n"
            "                   [[-oo NAME=VALUE] ...]\n");
    printf( "\n");
    printf( "-fsf : full spatial filter testing (slow)\n");
    exit( 1 );
}

/************************************************************************/
/*                           TestBasic()                                */
/************************************************************************/

static int TestBasic( OGRLayer *poLayer )
{
    int bRet = TRUE;

    const char* pszLayerName = LOG_ACTION(poLayer->GetName());
    OGRwkbGeometryType eGeomType = LOG_ACTION(poLayer->GetGeomType());
    OGRFeatureDefn* poFDefn = LOG_ACTION(poLayer->GetLayerDefn());

    if( strcmp(pszLayerName, LOG_ACTION(poFDefn->GetName())) != 0 )
    {
        bRet = FALSE;
        printf( "ERROR: poLayer->GetName() and poFDefn->GetName() differ.\n"
                "poLayer->GetName() = %s\n"
                "poFDefn->GetName() = %s\n",
                    pszLayerName, poFDefn->GetName());
    }

    if( strcmp(pszLayerName, LOG_ACTION(poLayer->GetDescription())) != 0 )
    {
        bRet = FALSE;
        printf( "ERROR: poLayer->GetName() and poLayer->GetDescription() differ.\n"
                "poLayer->GetName() = %s\n"
                "poLayer->GetDescription() = %s\n",
                    pszLayerName, poLayer->GetDescription());
    }
    
    if( eGeomType != LOG_ACTION(poFDefn->GetGeomType()) )
    {
        bRet = FALSE;
        printf( "ERROR: poLayer->GetGeomType() and poFDefn->GetGeomType() differ.\n"
                "poLayer->GetGeomType() = %d\n"
                "poFDefn->GetGeomType() = %d\n",
                    eGeomType, poFDefn->GetGeomType());
    }

    if( LOG_ACTION(poLayer->GetFIDColumn()) == NULL )
    {
        bRet = FALSE;
        printf( "ERROR: poLayer->GetFIDColumn() returned NULL.\n" );
    }

    if( LOG_ACTION(poLayer->GetGeometryColumn()) == NULL )
    {
        bRet = FALSE;
        printf( "ERROR: poLayer->GetGeometryColumn() returned NULL.\n" );
    }

    if( LOG_ACTION(poFDefn->GetGeomFieldCount()) > 0 )
    {
        if( eGeomType != LOG_ACTION(poFDefn->GetGeomFieldDefn(0))->GetType() )
        {
            bRet = FALSE;
            printf( "ERROR: poLayer->GetGeomType() and poFDefn->GetGeomFieldDefn(0)->GetType() differ.\n"
                    "poLayer->GetGeomType() = %d\n"
                    "poFDefn->GetGeomFieldDefn(0)->GetType() = %d\n",
                        eGeomType, poFDefn->GetGeomFieldDefn(0)->GetType());
        }

        if( !EQUAL(LOG_ACTION(poLayer->GetGeometryColumn()),
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

        if( LOG_ACTION(poLayer->GetSpatialRef()) !=
                   LOG_ACTION(poFDefn->GetGeomFieldDefn(0)->GetSpatialRef()) )
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
    OGRFeature* poFeat = NULL;

    CPLPushErrorHandler(CPLQuietErrorHandler);

    if (LOG_ACTION(poLyr->TestCapability("fake_capability")))
    {
        printf( "ERROR: poLyr->TestCapability(\"fake_capability\") should have returned FALSE\n" );
        bRet = FALSE;
        goto bye;
    }

    if (LOG_ACTION(poLyr->GetFeature(-10)) != NULL)
    {
        printf( "ERROR: GetFeature(-10) should have returned NULL\n" );
        bRet = FALSE;
        goto bye;
    }

    if (LOG_ACTION(poLyr->GetFeature(2000000000)) != NULL)
    {
        printf( "ERROR: GetFeature(2000000000) should have returned NULL\n" );
        bRet = FALSE;
        goto bye;
    }

    // This should detect int overflow
    if (LOG_ACTION(poLyr->GetFeature((GIntBig)INT_MAX + 1)) != NULL)
    {
        printf( "ERROR: GetFeature((GIntBig)INT_MAX + 1) should have returned NULL\n" );
        bRet = FALSE;
        goto bye;
    }

    poLyr->ResetReading();
    poFeat = poLyr->GetNextFeature();
    if( poFeat )
    {
        poFeat->SetFID(-10);
        if (poLyr->SetFeature(poFeat) == OGRERR_NONE)
        {
            printf( "ERROR: SetFeature(-10) should have returned an error\n" );
            delete poFeat;
            bRet = FALSE;
            goto bye;
        }
        delete poFeat;
    }

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

    if (LOG_ACTION(poLyr->SetNextByIndex(-10)) != OGRERR_FAILURE)
    {
        printf( "ERROR: SetNextByIndex(-10) should have returned OGRERR_FAILURE\n" );
        bRet = FALSE;
        goto bye;
    }

    if (LOG_ACTION(poLyr->SetNextByIndex(2000000000)) == OGRERR_NONE &&
        LOG_ACTION(poLyr->GetNextFeature()) != NULL)
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

static const char* GetLayerNameForSQL( GDALDataset* poDS, const char* pszLayerName )
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

    if (EQUAL(poDS->GetDriverName(), "MYSQL"))
        return CPLSPrintf("`%s`", pszLayerName);

    if (EQUAL(poDS->GetDriverName(), "PostgreSQL") &&
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

    if (EQUAL(poDS->GetDriverName(), "SQLAnywhere"))
        return pszLayerName;

    if (EQUAL(poDS->GetDriverName(), "DB2ODBC"))
        return pszLayerName;
        
    return CPLSPrintf("\"%s\"", pszLayerName);
}

/************************************************************************/
/*                      TestOGRLayerFeatureCount()                      */
/*                                                                      */
/*      Verify that the feature count matches the actual number of      */
/*      features returned during sequential reading.                    */
/************************************************************************/

static int TestOGRLayerFeatureCount( GDALDataset* poDS, OGRLayer *poLayer, int bIsSQLLayer )

{
    int bRet = TRUE;
    GIntBig         nFC = 0, nClaimedFC = LOG_ACTION(poLayer->GetFeatureCount());
    OGRFeature  *poFeature;
    int         bWarnAboutSRS = FALSE;
    OGRFeatureDefn* poLayerDefn = LOG_ACTION(poLayer->GetLayerDefn());
    int nGeomFieldCount = LOG_ACTION(poLayerDefn->GetGeomFieldCount());

    poLayer->ResetReading();
    CPLErrorReset();

    while( (poFeature = LOG_ACTION(poLayer->GetNextFeature())) != NULL )
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
        printf( "ERROR: Claimed feature count " CPL_FRMT_GIB " doesn't match actual, " CPL_FRMT_GIB ".\n",
                nClaimedFC, nFC );
    }
    else if( nFC != LOG_ACTION(poLayer->GetFeatureCount()) )
    {
        bRet = FALSE;
        printf( "ERROR: Feature count at end of layer, " CPL_FRMT_GIB ", differs "
                "from at start, " CPL_FRMT_GIB ".\n",
                poLayer->GetFeatureCount(), nFC );
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
                printf( "ERROR: Claimed feature count " CPL_FRMT_GIB " doesn't match '%s' one, " CPL_FRMT_GIB ".\n",
                        nClaimedFC, osSQL.c_str(), poFeatCount->GetFieldAsInteger64(0) );
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

    LOG_ACTION(poLayer->SetSpatialFilter( NULL ));
    
    if( LOG_ACTION(poLayer->GetFeatureCount()) < 5 )
    {
        if( bVerbose )
            printf( "INFO: Only " CPL_FRMT_GIB " features on layer,"
                    "skipping random read test.\n",
                    poLayer->GetFeatureCount() );
        
        return bRet;
    }

/* -------------------------------------------------------------------- */
/*      Fetch five features.                                            */
/* -------------------------------------------------------------------- */
    LOG_ACTION(poLayer->ResetReading());
    
    for( iFeature = 0; iFeature < 5; iFeature++ )
    {
        papoFeatures[iFeature] = NULL;
    }
    for( iFeature = 0; iFeature < 5; iFeature++ )
    {
        papoFeatures[iFeature] = LOG_ACTION(poLayer->GetNextFeature());
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
    poFeature = LOG_ACTION(poLayer->GetFeature( papoFeatures[1]->GetFID() ));
    if (poFeature == NULL)
    {
        printf( "ERROR: Cannot fetch feature " CPL_FRMT_GIB ".\n",
                 papoFeatures[1]->GetFID() );
        goto end;
    }

    if( !poFeature->Equal( papoFeatures[1] ) )
    {
        bRet = FALSE;
        printf( "ERROR: Attempt to randomly read feature " CPL_FRMT_GIB " appears to\n"
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
    poFeature = LOG_ACTION(poLayer->GetFeature( papoFeatures[4]->GetFID() ));
    if( poFeature == NULL || !poFeature->Equal( papoFeatures[4] ) )
    {
        bRet = FALSE;
        printf( "ERROR: Attempt to randomly read feature " CPL_FRMT_GIB " appears to\n"
                "       have returned a different feature than sequential\n"
                "       reading indicates should have happened.\n",
                papoFeatures[4]->GetFID() );

        goto end;
    }

    OGRFeature::DestroyFeature(poFeature);

/* -------------------------------------------------------------------- */
/*      Test feature 2 again                                            */
/* -------------------------------------------------------------------- */
    poFeature = LOG_ACTION(poLayer->GetFeature( papoFeatures[2]->GetFID() ));
    if( poFeature == NULL || !poFeature->Equal( papoFeatures[2] ) )
    {
        bRet = FALSE;
        printf( "ERROR: Attempt to randomly read feature " CPL_FRMT_GIB " appears to\n"
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

    LOG_ACTION(poLayer->SetSpatialFilter( NULL ));
    
    if( LOG_ACTION(poLayer->GetFeatureCount()) < 5 )
    {
        if( bVerbose )
            printf( "INFO: Only " CPL_FRMT_GIB " features on layer,"
                    "skipping SetNextByIndex test.\n",
                    poLayer->GetFeatureCount() );
        
        return bRet;
    }

/* -------------------------------------------------------------------- */
/*      Fetch five features.                                            */
/* -------------------------------------------------------------------- */
    LOG_ACTION(poLayer->ResetReading());
    
    for( iFeature = 0; iFeature < 5; iFeature++ )
    {
        papoFeatures[iFeature] = LOG_ACTION(poLayer->GetNextFeature());
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
    if (LOG_ACTION(poLayer->SetNextByIndex(1)) != OGRERR_NONE)
    {
        bRet = FALSE;
        printf( "ERROR: SetNextByIndex(%d) failed.\n", 1 );
        goto end;
    }
    
    poFeature = LOG_ACTION(poLayer->GetNextFeature());
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
    
    poFeature = LOG_ACTION(poLayer->GetNextFeature());
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
    if (LOG_ACTION(poLayer->SetNextByIndex(3)) != OGRERR_NONE)
    {
        bRet = FALSE;
        printf( "ERROR: SetNextByIndex(%d) failed.\n", 3 );
        goto end;
    }
    
    poFeature = LOG_ACTION(poLayer->GetNextFeature());
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
    
    poFeature = LOG_ACTION(poLayer->GetNextFeature());
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
    GIntBig     nFID2, nFID5;

    memset(papoFeatures, 0, sizeof(papoFeatures));

    LOG_ACTION(poLayer->SetSpatialFilter( NULL ));

    if( LOG_ACTION(poLayer->GetFeatureCount()) < 5 )
    {
        if( bVerbose )
            printf( "INFO: Only " CPL_FRMT_GIB " features on layer,"
                    "skipping random write test.\n",
                    poLayer->GetFeatureCount() );
        
        return bRet;
    }

    if( !LOG_ACTION(poLayer->TestCapability( OLCRandomRead )) )
    {
        if( bVerbose )
            printf( "INFO: Skipping random write test since this layer "
                    "doesn't support random read.\n" );
        return bRet;
    }

/* -------------------------------------------------------------------- */
/*      Fetch five features.                                            */
/* -------------------------------------------------------------------- */
    LOG_ACTION(poLayer->ResetReading());
    
    for( iFeature = 0; iFeature < 5; iFeature++ )
    {
        papoFeatures[iFeature] = LOG_ACTION(poLayer->GetNextFeature());
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
    if( LOG_ACTION(poLayer->SetFeature( papoFeatures[1] )) != OGRERR_NONE )
    {
        bRet = FALSE;
        printf( "ERROR: Attempt to SetFeature(1) failed.\n" );
        goto end;
    }
    if( LOG_ACTION(poLayer->SetFeature( papoFeatures[4] )) != OGRERR_NONE )
    {
        bRet = FALSE;
        printf( "ERROR: Attempt to SetFeature(4) failed.\n" );
        goto end;
    }

/* -------------------------------------------------------------------- */
/*      Now re-read feature 2 to verify the effect stuck.               */
/* -------------------------------------------------------------------- */
    poFeature = LOG_ACTION(poLayer->GetFeature( nFID5 ));
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

    if( LOG_ACTION(poLayer->SetFeature( papoFeatures[1] )) != OGRERR_NONE )
    {
        bRet = FALSE;
        printf( "ERROR: Attempt to restore SetFeature(1) failed.\n" );
    }
    if( LOG_ACTION(poLayer->SetFeature( papoFeatures[4] )) != OGRERR_NONE )
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
    GIntBig         nInclusiveCount;

/* -------------------------------------------------------------------- */
/*      Read the target feature.                                        */
/* -------------------------------------------------------------------- */
    LOG_ACTION(poLayer->ResetReading());
    poTargetFeature = LOG_ACTION(poLayer->GetNextFeature());

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

    OGREnvelope sLayerExtent;
    double epsilon = 10.0;
    if( LOG_ACTION(poLayer->TestCapability( OLCFastGetExtent )) &&
        LOG_ACTION(poLayer->GetExtent(iGeomField, &sLayerExtent)) == OGRERR_NONE &&
        sLayerExtent.MinX < sLayerExtent.MaxX &&
        sLayerExtent.MinY < sLayerExtent.MaxY )
    {
        epsilon = MIN( sLayerExtent.MaxX - sLayerExtent.MinX, sLayerExtent.MaxY - sLayerExtent.MinY ) / 10.0;
    }

/* -------------------------------------------------------------------- */
/*      Construct inclusive filter.                                     */
/* -------------------------------------------------------------------- */
    
    oRing.setPoint( 0, sEnvelope.MinX - 2 * epsilon, sEnvelope.MinY - 2 * epsilon );
    oRing.setPoint( 1, sEnvelope.MinX - 2 * epsilon, sEnvelope.MaxY + 1 * epsilon );
    oRing.setPoint( 2, sEnvelope.MaxX + 1 * epsilon, sEnvelope.MaxY + 1 * epsilon );
    oRing.setPoint( 3, sEnvelope.MaxX + 1 * epsilon, sEnvelope.MinY - 2 * epsilon );
    oRing.setPoint( 4, sEnvelope.MinX - 2 * epsilon, sEnvelope.MinY - 2 * epsilon );
    
    oInclusiveFilter.addRing( &oRing );

    LOG_ACTION(poLayer->SetSpatialFilter( iGeomField, &oInclusiveFilter ));

/* -------------------------------------------------------------------- */
/*      Verify that we can find the target feature.                     */
/* -------------------------------------------------------------------- */
    LOG_ACTION(poLayer->ResetReading());

    while( (poFeature = LOG_ACTION(poLayer->GetNextFeature())) != NULL )
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

    nInclusiveCount = LOG_ACTION(poLayer->GetFeatureCount());

/* -------------------------------------------------------------------- */
/*      Construct exclusive filter.                                     */
/* -------------------------------------------------------------------- */
    oRing.setPoint( 0, sEnvelope.MinX - 2 * epsilon, sEnvelope.MinY - 2 * epsilon );
    oRing.setPoint( 1, sEnvelope.MinX - 1 * epsilon, sEnvelope.MinY - 2 * epsilon );
    oRing.setPoint( 2, sEnvelope.MinX - 1 * epsilon, sEnvelope.MinY - 1 * epsilon );
    oRing.setPoint( 3, sEnvelope.MinX - 2 * epsilon, sEnvelope.MinY - 1 * epsilon );
    oRing.setPoint( 4, sEnvelope.MinX - 2 * epsilon, sEnvelope.MinY - 2 * epsilon );
    
    oExclusiveFilter.addRing( &oRing );

    LOG_ACTION(poLayer->SetSpatialFilter( iGeomField, &oExclusiveFilter ));

/* -------------------------------------------------------------------- */
/*      Verify that we can NOT find the target feature.                 */
/* -------------------------------------------------------------------- */
    LOG_ACTION(poLayer->ResetReading());

    while( (poFeature = LOG_ACTION(poLayer->GetNextFeature())) != NULL )
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
    else if( LOG_ACTION(poLayer->GetFeatureCount()) >= nInclusiveCount )
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
    poFeature = LOG_ACTION(poLayer->GetFeature( poTargetFeature->GetFID() ));
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
        LOG_ACTION(poLayer->ResetReading());
        while( (poFeature = LOG_ACTION(poLayer->GetNextFeature())) != NULL )
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

#define NEG_INF -std::numeric_limits<double>::infinity()
#define POS_INF std::numeric_limits<double>::infinity()

    oRing.setPoint( 0, NEG_INF, NEG_INF );
    oRing.setPoint( 1, NEG_INF, POS_INF );
    oRing.setPoint( 2, POS_INF, POS_INF );
    oRing.setPoint( 3, POS_INF, NEG_INF );
    oRing.setPoint( 4, NEG_INF, NEG_INF );

    OGRPolygon oInfinityFilter;
    oInfinityFilter.addRing( &oRing );

    LOG_ACTION(poLayer->SetSpatialFilter( iGeomField, &oInfinityFilter ));
    LOG_ACTION(poLayer->ResetReading());
    int nCountInf = 0;
    while( (poFeature = LOG_ACTION(poLayer->GetNextFeature())) != NULL )
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

    LOG_ACTION(poLayer->SetSpatialFilter( iGeomField, &oHugeFilter ));
    LOG_ACTION(poLayer->ResetReading());
    int nCountHuge = 0;
    while( (poFeature = LOG_ACTION(poLayer->GetNextFeature())) != NULL )
    {
        if( poFeature->GetGeomFieldRef(iGeomField) != NULL )
            nCountHuge ++;
        delete poFeature;
    }

/* -------------------------------------------------------------------- */
/*     Reset spatial filter                                             */
/* -------------------------------------------------------------------- */
    LOG_ACTION(poLayer->SetSpatialFilter( NULL ));

    int nExpected = 0;
    poLayer->ResetReading();
    while( (poFeature = LOG_ACTION(poLayer->GetNextFeature())) != NULL )
    {
        if( poFeature->GetGeomFieldRef(iGeomField) != NULL )
            nExpected ++;
        delete poFeature;
    }
    LOG_ACTION(poLayer->ResetReading());

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

static int TestFullSpatialFilter( OGRLayer *poLayer, int iGeomField )

{
    int bRet = TRUE;

    OGREnvelope sLayerExtent;
    double epsilon = 10.0;
    if( LOG_ACTION(poLayer->TestCapability( OLCFastGetExtent )) &&
        LOG_ACTION(poLayer->GetExtent(iGeomField, &sLayerExtent)) == OGRERR_NONE &&
        sLayerExtent.MinX < sLayerExtent.MaxX &&
        sLayerExtent.MinY < sLayerExtent.MaxY )
    {
        epsilon = MIN( sLayerExtent.MaxX - sLayerExtent.MinX, sLayerExtent.MaxY - sLayerExtent.MinY ) / 10.0;
    }

    GIntBig nTotalFeatureCount = LOG_ACTION(poLayer->GetFeatureCount());
    for(GIntBig i=0; i<nTotalFeatureCount;i++ )
    {
        OGRFeature  *poFeature, *poTargetFeature;
        OGRPolygon  oInclusiveFilter;
        OGRLinearRing oRing;
        OGREnvelope sEnvelope;

    /* -------------------------------------------------------------------- */
    /*      Read the target feature.                                        */
    /* -------------------------------------------------------------------- */
        LOG_ACTION(poLayer->SetSpatialFilter( NULL ));
        LOG_ACTION(poLayer->ResetReading());
        LOG_ACTION(poLayer->SetNextByIndex(i));
        poTargetFeature = LOG_ACTION(poLayer->GetNextFeature());

        if( poTargetFeature == NULL )
        {
            continue;
        }

        OGRGeometry* poGeom = poTargetFeature->GetGeomFieldRef(iGeomField);
        if( poGeom == NULL || poGeom->IsEmpty() )
        {
            OGRFeature::DestroyFeature(poTargetFeature);
            continue;
        }

        poGeom->getEnvelope( &sEnvelope );

/* -------------------------------------------------------------------- */
/*      Construct inclusive filter.                                     */
/* -------------------------------------------------------------------- */

        oRing.setPoint( 0, sEnvelope.MinX - 2 * epsilon, sEnvelope.MinY - 2 * epsilon );
        oRing.setPoint( 1, sEnvelope.MinX - 2 * epsilon, sEnvelope.MaxY + 1 * epsilon );
        oRing.setPoint( 2, sEnvelope.MaxX + 1 * epsilon, sEnvelope.MaxY + 1 * epsilon );
        oRing.setPoint( 3, sEnvelope.MaxX + 1 * epsilon, sEnvelope.MinY - 2 * epsilon );
        oRing.setPoint( 4, sEnvelope.MinX - 2 * epsilon, sEnvelope.MinY - 2 * epsilon );
        
        oInclusiveFilter.addRing( &oRing );

        LOG_ACTION(poLayer->SetSpatialFilter( iGeomField, &oInclusiveFilter ));

/* -------------------------------------------------------------------- */
/*      Verify that we can find the target feature.                     */
/* -------------------------------------------------------------------- */
        LOG_ACTION(poLayer->ResetReading());

        while( (poFeature = LOG_ACTION(poLayer->GetNextFeature())) != NULL )
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
            printf( "ERROR: Spatial filter (%d) eliminated feature " CPL_FRMT_GIB " unexpectedly!\n",
                    iGeomField, poTargetFeature->GetFID());
            OGRFeature::DestroyFeature(poTargetFeature);
            break;
        }

        OGRFeature::DestroyFeature(poTargetFeature);
    }

/* -------------------------------------------------------------------- */
/*     Reset spatial filter                                             */
/* -------------------------------------------------------------------- */
    LOG_ACTION(poLayer->SetSpatialFilter( NULL ));
    
    if( bRet && bVerbose )
    {
        printf( "INFO: Full spatial filter succeeded.\n");
    }

    return bRet;
}

static int TestSpatialFilter( OGRLayer *poLayer )
{
/* -------------------------------------------------------------------- */
/*      Read the target feature.                                        */
/* -------------------------------------------------------------------- */
    LOG_ACTION(poLayer->ResetReading());
    OGRFeature* poTargetFeature = LOG_ACTION(poLayer->GetNextFeature());

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

    int nGeomFieldCount = LOG_ACTION(poLayer->GetLayerDefn()->GetGeomFieldCount());
    if( nGeomFieldCount == 0 )
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
    for( int iGeom = 0; iGeom < nGeomFieldCount; iGeom ++ )
    {
        bRet &= TestSpatialFilter(poLayer, iGeom);
        
        if( bFullSpatialFilter )
            bRet &= TestFullSpatialFilter( poLayer, iGeom );
    }
    
    OGRPolygon oPolygon;
    CPLErrorReset();
    CPLPushErrorHandler(CPLQuietErrorHandler);
    LOG_ACTION(poLayer->SetSpatialFilter(-1, &oPolygon));
    CPLPopErrorHandler();
    if( CPLGetLastErrorType() == 0 )
        printf( "WARNING: poLayer->SetSpatialFilter(-1) should emit an error.\n" );

    CPLErrorReset();
    CPLPushErrorHandler(CPLQuietErrorHandler);
    LOG_ACTION(poLayer->SetSpatialFilter(nGeomFieldCount, &oPolygon));
    CPLPopErrorHandler();
    if( CPLGetLastErrorType() == 0 )
        printf( "WARNING: poLayer->SetSpatialFilter(nGeomFieldCount) should emit an error.\n" );

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

static int TestAttributeFilter( CPL_UNUSED GDALDataset* poDS, OGRLayer *poLayer )

{
    int bRet = TRUE;
    OGRFeature  *poFeature, *poFeature2, *poFeature3, *poTargetFeature;
    GIntBig        nInclusiveCount, nExclusiveCount, nTotalCount;
    CPLString osAttributeFilter;

/* -------------------------------------------------------------------- */
/*      Read the target feature.                                        */
/* -------------------------------------------------------------------- */
    LOG_ACTION(poLayer->ResetReading());
    poTargetFeature = LOG_ACTION(poLayer->GetNextFeature());

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

    if( strchr(pszFieldName, '_') || strchr(pszFieldName, ' ') )
    {
        osAttributeFilter = "\"";
        osAttributeFilter += pszFieldName;
        osAttributeFilter += "\"";
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
    LOG_ACTION(poLayer->SetAttributeFilter( osAttributeFilter ));

/* -------------------------------------------------------------------- */
/*      Verify that we can find the target feature.                     */
/* -------------------------------------------------------------------- */
    LOG_ACTION(poLayer->ResetReading());

    while( (poFeature = LOG_ACTION(poLayer->GetNextFeature())) != NULL )
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

    nInclusiveCount = LOG_ACTION(poLayer->GetFeatureCount());

/* -------------------------------------------------------------------- */
/*      Construct exclusive filter.                                     */
/* -------------------------------------------------------------------- */
    if( strchr(pszFieldName, '_') || strchr(pszFieldName, ' ') )
    {
        osAttributeFilter = "\"";
        osAttributeFilter += pszFieldName;
        osAttributeFilter += "\"";
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
    LOG_ACTION(poLayer->SetAttributeFilter( osAttributeFilter ));

/* -------------------------------------------------------------------- */
/*      Verify that we can find the target feature.                     */
/* -------------------------------------------------------------------- */
    LOG_ACTION(poLayer->ResetReading());

    GIntBig nExclusiveCountWhileIterating = 0;
    while( (poFeature = LOG_ACTION(poLayer->GetNextFeature())) != NULL )
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

    nExclusiveCount = LOG_ACTION(poLayer->GetFeatureCount());

    // Check that GetFeature() ignores the attribute filter
    poFeature2 = LOG_ACTION(poLayer->GetFeature( poTargetFeature->GetFID() ));

    poLayer->ResetReading();
    while( (poFeature3 = LOG_ACTION(poLayer->GetNextFeature())) != NULL )
    {
        if( poFeature3->Equal(poTargetFeature) )
        {
            OGRFeature::DestroyFeature(poFeature3);
            break;
        }
        else
            OGRFeature::DestroyFeature(poFeature3);
    }

    LOG_ACTION(poLayer->SetAttributeFilter( NULL ));

    nTotalCount = LOG_ACTION(poLayer->GetFeatureCount());

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
                "filter into account (nInclusiveCount = " CPL_FRMT_GIB ", nExclusiveCount = " CPL_FRMT_GIB ", nExclusiveCountWhileIterating = " CPL_FRMT_GIB ", nTotalCount = " CPL_FRMT_GIB ").\n",
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

    LOG_ACTION(poLayer->SetSpatialFilter( NULL ));
    LOG_ACTION(poLayer->SetAttributeFilter( NULL ));
    LOG_ACTION(poLayer->ResetReading());

    int bIsAdvertizedAsUTF8 = LOG_ACTION(poLayer->TestCapability( OLCStringsAsUTF8 ));
    int nFields = LOG_ACTION(poLayer->GetLayerDefn()->GetFieldCount());
    int bFoundString = FALSE;
    int bFoundNonASCII = FALSE;
    int bFoundUTF8 = FALSE;
    int bCanAdvertizeUTF8 = TRUE;

    OGRFeature* poFeature = NULL;
    while( bRet && (poFeature = LOG_ACTION(poLayer->GetNextFeature())) != NULL )
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
                            printf( "ERROR: Found non-UTF8 content at field %d of feature " CPL_FRMT_GIB ", but layer is advertized as UTF-8.\n",
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

    LOG_ACTION(poLayer->SetSpatialFilter( NULL ));
    LOG_ACTION(poLayer->SetAttributeFilter( NULL ));
    LOG_ACTION(poLayer->ResetReading());

    OGREnvelope sExtent;
    OGREnvelope sExtentSlow;

    OGRErr eErr = LOG_ACTION(poLayer->GetExtent(iGeomField, &sExtent, TRUE));
    OGRErr eErr2 = LOG_ACTION(poLayer->OGRLayer::GetExtent(iGeomField, &sExtentSlow, TRUE));

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
    int nGeomFieldCount = LOG_ACTION(poLayer->GetLayerDefn()->GetGeomFieldCount());
    for( int iGeom = 0; iGeom < nGeomFieldCount; iGeom ++ )
        bRet &= TestGetExtent(poLayer, iGeom);

    OGREnvelope sExtent;
    
    CPLPushErrorHandler(CPLQuietErrorHandler);
    OGRErr eErr = LOG_ACTION(poLayer->GetExtent(-1, &sExtent, TRUE));
    CPLPopErrorHandler();
    if( eErr != OGRERR_FAILURE )
    {
        printf("ERROR: poLayer->GetExtent(-1) should fail.\n");
        bRet = FALSE;
    }
    
    CPLPushErrorHandler(CPLQuietErrorHandler);
    eErr = LOG_ACTION(poLayer->GetExtent(nGeomFieldCount, &sExtent, TRUE));
    CPLPopErrorHandler();
    if( eErr != OGRERR_FAILURE )
    {
        printf("ERROR: poLayer->GetExtent(nGeomFieldCount) should fail.\n");
        bRet = FALSE;
    }

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
    GIntBig nFID;

    LOG_ACTION(poLayer->SetSpatialFilter( NULL ));
    
    if( !LOG_ACTION(poLayer->TestCapability( OLCRandomRead )) )
    {
        if( bVerbose )
            printf( "INFO: Skipping delete feature test since this layer "
                    "doesn't support random read.\n" );
        return bRet;
    }

    if( LOG_ACTION(poLayer->GetFeatureCount()) == 0 )
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
    LOG_ACTION(poLayer->ResetReading());

    LOG_ACTION(poLayer->SetNextByIndex(poLayer->GetFeatureCount() - 1));
    poFeature = LOG_ACTION(poLayer->GetNextFeature());
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
    if( LOG_ACTION(poLayer->DeleteFeature( nFID )) != OGRERR_NONE )
    {
        bRet = FALSE;
        printf( "ERROR: Attempt to DeleteFeature() failed.\n" );
        goto end;
    }
    
/* -------------------------------------------------------------------- */
/*      Now re-read the feature to verify the delete effect worked.     */
/* -------------------------------------------------------------------- */
    CPLPushErrorHandler(CPLQuietErrorHandler); /* silent legitimate error message */
    poFeatureTest = LOG_ACTION(poLayer->GetFeature( nFID ));
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
    if( LOG_ACTION(poLayer->CreateFeature( poFeature )) != OGRERR_NONE )
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
    poFeatureTest = LOG_ACTION(poLayer->GetFeature( nFID ));
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
    GIntBig nInitialFeatureCount = LOG_ACTION(poLayer->GetFeatureCount());

    OGRErr eErr = LOG_ACTION(poLayer->StartTransaction());
    if (eErr == OGRERR_NONE)
    {
        if (LOG_ACTION(poLayer->TestCapability(OLCTransactions)) == FALSE)
        {
            eErr = LOG_ACTION(poLayer->RollbackTransaction());
            if (eErr == OGRERR_UNSUPPORTED_OPERATION && LOG_ACTION(poLayer->TestCapability(OLCTransactions)) == FALSE)
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
        if (LOG_ACTION(poLayer->TestCapability(OLCTransactions)) == TRUE)
        {
            printf("ERROR: StartTransaction() failed, but TestCapability(OLCTransactions) returns TRUE.\n");
            return FALSE;
        }
        else
        {
            return TRUE;
        }
    }

    eErr = LOG_ACTION(poLayer->RollbackTransaction());
    if (eErr != OGRERR_NONE)
    {
        printf("ERROR: RollbackTransaction() failed after successful StartTransaction().\n");
        return FALSE;
    }

    /* ---------------- */

    eErr = LOG_ACTION(poLayer->StartTransaction());
    if (eErr != OGRERR_NONE)
    {
        printf("ERROR: StartTransaction() failed.\n");
        return FALSE;
    }

    eErr = LOG_ACTION(poLayer->CommitTransaction());
    if (eErr != OGRERR_NONE)
    {
        printf("ERROR: CommitTransaction() failed after successful StartTransaction().\n");
        return FALSE;
    }

    /* ---------------- */

    eErr = LOG_ACTION(poLayer->StartTransaction());
    if (eErr != OGRERR_NONE)
    {
        printf("ERROR: StartTransaction() failed.\n");
        return FALSE;
    }

    poFeature = new OGRFeature(poLayer->GetLayerDefn());
    if (poLayer->GetLayerDefn()->GetFieldCount() > 0)
        poFeature->SetField(0, "0");
    eErr = LOG_ACTION(poLayer->CreateFeature(poFeature));
    delete poFeature;
    poFeature = NULL;

    if (eErr == OGRERR_FAILURE)
    {
        if( bVerbose )
        {
            printf("INFO: CreateFeature() failed. Exiting this test now.\n");
        }
        LOG_ACTION(poLayer->RollbackTransaction());
        return TRUE;
    }

    eErr = LOG_ACTION(poLayer->RollbackTransaction());
    if (eErr != OGRERR_NONE)
    {
        printf("ERROR: RollbackTransaction() failed after successful StartTransaction().\n");
        return FALSE;
    }

    if (LOG_ACTION(poLayer->GetFeatureCount()) != nInitialFeatureCount)
    {
        printf("ERROR: GetFeatureCount() should have returned its initial value after RollbackTransaction().\n");
        return FALSE;
    }

    /* ---------------- */

    if( LOG_ACTION(poLayer->TestCapability( OLCDeleteFeature )) )
    {
        eErr = LOG_ACTION(poLayer->StartTransaction());
        if (eErr != OGRERR_NONE)
        {
            printf("ERROR: StartTransaction() failed.\n");
            return FALSE;
        }

        poFeature = new OGRFeature(poLayer->GetLayerDefn());
        if (poLayer->GetLayerDefn()->GetFieldCount() > 0)
            poFeature->SetField(0, "0");
        eErr = poLayer->CreateFeature(poFeature);
        GIntBig nFID = poFeature->GetFID();
        delete poFeature;
        poFeature = NULL;

        if (eErr == OGRERR_FAILURE)
        {
            printf("ERROR: CreateFeature() failed. Exiting this test now.\n");
            LOG_ACTION(poLayer->RollbackTransaction());
            return FALSE;
        }

        if( nFID < 0 )
        {
            printf("WARNING: CreateFeature() returned featured without FID.\n");
            LOG_ACTION(poLayer->RollbackTransaction());
            return FALSE;
        }

        eErr = LOG_ACTION(poLayer->CommitTransaction());
        if (eErr != OGRERR_NONE)
        {
            printf("ERROR: CommitTransaction() failed after successful StartTransaction().\n");
            return FALSE;
        }

        if (LOG_ACTION(poLayer->GetFeatureCount()) != nInitialFeatureCount + 1)
        {
            printf("ERROR: GetFeatureCount() should have returned its initial value + 1 after CommitTransaction().\n");
            return FALSE;
        }

        eErr = LOG_ACTION(poLayer->DeleteFeature(nFID));
        if (eErr != OGRERR_NONE)
        {
            printf("ERROR: DeleteFeature() failed.\n");
            return FALSE;
        }

        if (LOG_ACTION(poLayer->GetFeatureCount()) != nInitialFeatureCount)
        {
            printf("ERROR: GetFeatureCount() should have returned its initial value after DeleteFeature().\n");
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

    LOG_ACTION(poLayer->ResetReading());
    while( (poFeature = LOG_ACTION(poLayer->GetNextFeature())) != NULL )
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

    OGRErr eErr = LOG_ACTION(poLayer->SetIgnoredFields((const char**)papszIgnoredFields));
    CSLDestroy(papszIgnoredFields);

    if( eErr == OGRERR_FAILURE )
    {
        printf( "ERROR: SetIgnoredFields() failed.\n" );
        poLayer->SetIgnoredFields(NULL);
        return FALSE;
    }

    int bFoundNonEmpty2 = FALSE;

    LOG_ACTION(poLayer->ResetReading());
    while( (poFeature = LOG_ACTION(poLayer->GetNextFeature())) != NULL )
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

    LOG_ACTION(poLayer->SetIgnoredFields(NULL));

    if( bVerbose )
    {
        printf( "INFO: IgnoreFields test passed.\n" );
    }

    return TRUE;
}

/************************************************************************/
/*                            TestLayerSQL()                            */
/************************************************************************/

static int TestLayerSQL( GDALDataset* poDS, OGRLayer * poLayer )

{
    int bRet = TRUE;
    OGRLayer* poSQLLyr = NULL;
    OGRFeature* poLayerFeat = NULL;
    OGRFeature* poSQLFeat = NULL;
    int bGotFeature = FALSE;

    CPLString osSQL;

    /* Test consistency between result layer and traditional layer */
    LOG_ACTION(poLayer->ResetReading());
    poLayerFeat = LOG_ACTION(poLayer->GetNextFeature());

    /* Reset to avoid potentially a statement to be active which cause */
    /* issue in the transaction test of the second layer, when testing */
    /* multi-tables sqlite and gpkg databases */
    LOG_ACTION(poLayer->ResetReading());

    osSQL.Printf("SELECT * FROM %s", GetLayerNameForSQL(poDS, poLayer->GetName()));
    poSQLLyr = LOG_ACTION(poDS->ExecuteSQL(osSQL.c_str(), NULL, NULL));
    if( poSQLLyr == NULL )
    {
        printf( "ERROR: ExecuteSQL(%s) failed.\n", osSQL.c_str() );
        bRet = FALSE;
        return bRet;
    }
    else
    {
        poSQLFeat = LOG_ACTION(poSQLLyr->GetNextFeature());
        if( poSQLFeat != NULL )
            bGotFeature = TRUE;
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
        LOG_ACTION(poDS->ReleaseResultSet(poSQLLyr));
        poSQLLyr = NULL;
    }

    /* Try ResetReading(), GetNextFeature(), ResetReading(), GetNextFeature() */
    poSQLLyr = LOG_ACTION(poDS->ExecuteSQL(osSQL.c_str(), NULL, NULL));

    LOG_ACTION(poSQLLyr->ResetReading());

    poSQLFeat = LOG_ACTION(poSQLLyr->GetNextFeature());
    if( poSQLFeat == NULL && bGotFeature )
    {
        printf( "ERROR: Should have got feature (1)\n" );
        bRet = FALSE;
    }
    OGRFeature::DestroyFeature(poSQLFeat);
    poSQLFeat = NULL;

    LOG_ACTION(poSQLLyr->ResetReading());

    poSQLFeat = LOG_ACTION(poSQLLyr->GetNextFeature());
    if( poSQLFeat == NULL && bGotFeature )
    {
        printf( "ERROR: Should have got feature (2)\n" );
        bRet = FALSE;
    }
    OGRFeature::DestroyFeature(poSQLFeat);
    poSQLFeat = NULL;

    if( poSQLLyr )
    {
        LOG_ACTION(poDS->ReleaseResultSet(poSQLLyr));
        poSQLLyr = NULL;
    }

    /* Return an empty layer */
    osSQL.Printf("SELECT * FROM %s WHERE 0 = 1", GetLayerNameForSQL(poDS, poLayer->GetName()));

    poSQLLyr = LOG_ACTION(poDS->ExecuteSQL(osSQL.c_str(), NULL, NULL));
    if (poSQLLyr)
    {
        poSQLFeat = LOG_ACTION(poSQLLyr->GetNextFeature());
        if (poSQLFeat != NULL)
        {
            bRet = FALSE;
            printf( "ERROR: ExecuteSQL() should have returned a layer without features.\n" );
        }
        OGRFeature::DestroyFeature(poSQLFeat);
        LOG_ACTION(poDS->ReleaseResultSet(poSQLLyr));
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

static int TestOGRLayer( GDALDataset* poDS, OGRLayer * poLayer, int bIsSQLLayer )

{
    int bRet = TRUE;

/* -------------------------------------------------------------------- */
/*      Verify that there is no spatial filter in place by default.     */
/* -------------------------------------------------------------------- */
    if( LOG_ACTION(poLayer->GetSpatialFilter()) != NULL )
    {
        printf( "WARN: Spatial filter in place by default on layer %s.\n",
                poLayer->GetName() );
        LOG_ACTION(poLayer->SetSpatialFilter( NULL ));
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
    bRet &= TestOGRLayerRandomRead( poLayer );
    
/* -------------------------------------------------------------------- */
/*      Test SetNextByIndex.                                            */
/* -------------------------------------------------------------------- */
    bRet &= TestOGRLayerSetNextByIndex( poLayer );

/* -------------------------------------------------------------------- */
/*      Test delete feature.                                            */
/* -------------------------------------------------------------------- */
    if( LOG_ACTION(poLayer->TestCapability( OLCDeleteFeature )) )
    {
        bRet &= TestOGRLayerDeleteAndCreateFeature( poLayer );
    }
    
/* -------------------------------------------------------------------- */
/*      Test random writing.                                            */
/* -------------------------------------------------------------------- */
    if( LOG_ACTION(poLayer->TestCapability( OLCRandomWrite )) )
    {
        bRet &= TestOGRLayerRandomWrite( poLayer );
    }

/* -------------------------------------------------------------------- */
/*      Test OLCIgnoreFields.                                           */
/* -------------------------------------------------------------------- */
    if( LOG_ACTION(poLayer->TestCapability( OLCIgnoreFields )) )
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
    if( LOG_ACTION(poLayer->TestCapability( OLCSequentialWrite )) )
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

static int TestInterleavedReading( const char* pszDataSourceIn, char** papszLayersIn )
{
    int bRet = TRUE;
    GDALDataset* poDS = NULL;
    GDALDataset* poDS2 = NULL;
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
    poDS = LOG_ACTION((GDALDataset*) GDALOpenEx( pszDataSourceIn,
                            GDAL_OF_VECTOR, NULL, papszOpenOptions, NULL ));
    if (poDS == NULL)
    {
        if( bVerbose )
        {
            printf( "INFO: Skipping TestInterleavedReading(). Cannot reopen datasource\n" );
        }
        goto bye;
    }

    poLayer1 = LOG_ACTION(papszLayersIn ? poDS->GetLayerByName(papszLayersIn[0]) : poDS->GetLayer(0));
    poLayer2 = LOG_ACTION(papszLayersIn ? poDS->GetLayerByName(papszLayersIn[1]) : poDS->GetLayer(1));
    if (poLayer1 == NULL || poLayer2 == NULL ||
        LOG_ACTION(poLayer1->GetFeatureCount()) < 2 || LOG_ACTION(poLayer2->GetFeatureCount()) < 2)
    {
        if( bVerbose )
        {
            printf( "INFO: Skipping TestInterleavedReading(). Test conditions are not met\n" );
        }
        goto bye;
    }

    /* Test normal reading */
    LOG_ACTION(GDALClose( (GDALDatasetH)poDS ));
    poDS = LOG_ACTION((GDALDataset*) GDALOpenEx( pszDataSourceIn,
                                GDAL_OF_VECTOR, NULL, papszOpenOptions, NULL ));
    poDS2 = LOG_ACTION((GDALDataset*) GDALOpenEx( pszDataSourceIn,
                                GDAL_OF_VECTOR, NULL, papszOpenOptions, NULL ));
    if (poDS == NULL || poDS2 == NULL)
    {
        if( bVerbose )
        {
            printf( "INFO: Skipping TestInterleavedReading(). Cannot reopen datasource\n" );
        }
        goto bye;
    }

    poLayer1 = LOG_ACTION(papszLayersIn ? poDS->GetLayerByName(papszLayersIn[0]) : poDS->GetLayer(0));
    poLayer2 = LOG_ACTION(papszLayersIn ? poDS->GetLayerByName(papszLayersIn[1]) : poDS->GetLayer(1));
    if (poLayer1 == NULL || poLayer2 == NULL)
    {
        printf( "ERROR: Skipping TestInterleavedReading(). Test conditions are not met\n" );
        bRet = FALSE;
        goto bye;
    }

    poFeature11_Ref = LOG_ACTION(poLayer1->GetNextFeature());
    poFeature12_Ref = LOG_ACTION(poLayer1->GetNextFeature());
    poFeature21_Ref = LOG_ACTION(poLayer2->GetNextFeature());
    poFeature22_Ref = LOG_ACTION(poLayer2->GetNextFeature());
    if (poFeature11_Ref == NULL || poFeature12_Ref == NULL || poFeature21_Ref == NULL || poFeature22_Ref == NULL)
    {
        printf( "ERROR: TestInterleavedReading() failed: poFeature11_Ref=%p, poFeature12_Ref=%p, poFeature21_Ref=%p, poFeature22_Ref=%p\n",
                poFeature11_Ref, poFeature12_Ref, poFeature21_Ref, poFeature22_Ref);
        bRet = FALSE;
        goto bye;
    }

    /* Test interleaved reading */
    poLayer1 = LOG_ACTION(papszLayersIn ? poDS2->GetLayerByName(papszLayersIn[0]) : poDS2->GetLayer(0));
    poLayer2 = LOG_ACTION(papszLayersIn ? poDS2->GetLayerByName(papszLayersIn[1]) : poDS2->GetLayer(1));
    if (poLayer1 == NULL || poLayer2 == NULL)
    {
        printf( "ERROR: Skipping TestInterleavedReading(). Test conditions are not met\n" );
        bRet = FALSE;
        goto bye;
    }

    poFeature11 = LOG_ACTION(poLayer1->GetNextFeature());
    poFeature21 = LOG_ACTION(poLayer2->GetNextFeature());
    poFeature12 = LOG_ACTION(poLayer1->GetNextFeature());
    poFeature22 = LOG_ACTION(poLayer2->GetNextFeature());

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
        printf("INFO: TestInterleavedReading() successful.\n");
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
    if( poDS != NULL)
        LOG_ACTION(GDALClose( (GDALDatasetH)poDS ));
    if( poDS2 != NULL )
        LOG_ACTION(GDALClose( (GDALDatasetH)poDS2 ));
    return bRet;
}

/************************************************************************/
/*                          TestDSErrorConditions()                     */
/************************************************************************/

static int TestDSErrorConditions( GDALDataset * poDS )
{
    int bRet = TRUE;
    OGRLayer* poLyr;

    CPLPushErrorHandler(CPLQuietErrorHandler);

    if (LOG_ACTION(poDS->TestCapability("fake_capability")))
    {
        printf( "ERROR: TestCapability(\"fake_capability\") should have returned FALSE\n" );
        bRet = FALSE;
        goto bye;
    }

    if (LOG_ACTION(poDS->GetLayer(-1)) != NULL)
    {
        printf( "ERROR: GetLayer(-1) should have returned NULL\n" );
        bRet = FALSE;
        goto bye;
    }

    if (LOG_ACTION(poDS->GetLayer(poDS->GetLayerCount())) != NULL)
    {
        printf( "ERROR: GetLayer(poDS->GetLayerCount()) should have returned NULL\n" );
        bRet = FALSE;
        goto bye;
    }

    if (LOG_ACTION(poDS->GetLayerByName("non_existing_layer")) != NULL)
    {
        printf( "ERROR: GetLayerByName(\"non_existing_layer\") should have returned NULL\n" );
        bRet = FALSE;
        goto bye;
    }

    poLyr = LOG_ACTION(poDS->ExecuteSQL("a fake SQL command", NULL, NULL));
    if (poLyr != NULL)
    {
        LOG_ACTION(poDS->ReleaseResultSet(poLyr));
        printf( "ERROR: ExecuteSQL(\"a fake SQL command\") should have returned NULL\n" );
        bRet = FALSE;
        goto bye;
    }

bye:
    CPLPopErrorHandler();
    return bRet;
}

/************************************************************************/
/*                              TestVirtualIO()                         */
/************************************************************************/

static int TestVirtualIO( GDALDataset * poDS )
{
    int bRet = TRUE;

    if( STARTS_WITH(poDS->GetDescription(), "/vsimem/") )
        return TRUE;

    VSIStatBufL sStat;
    if( !(VSIStatL( poDS->GetDescription(), &sStat) == 0) )
        return TRUE;

    // Don't try with ODBC (will avoid a useless error message in ogr_odbc.py)
    if( poDS->GetDriver() != NULL &&
        EQUAL(poDS->GetDriver()->GetDescription(), "ODBC") )
    {
        return TRUE;
    }

    char** papszFileList = LOG_ACTION(poDS->GetFileList());
    char** papszIter = papszFileList;
    CPLString osPath;
    int bAllPathIdentical = TRUE;
    for( ; *papszIter != NULL; papszIter++ )
    {
        if( papszIter == papszFileList )
            osPath = CPLGetPath(*papszIter);
        else if( strcmp(osPath, CPLGetPath(*papszIter)) != 0 )
        {
            bAllPathIdentical = FALSE;
            break;
        }
    }
    CPLString osVirtPath;
    if( bAllPathIdentical && CSLCount(papszFileList) > 1 )
    {
        osVirtPath = CPLFormFilename("/vsimem", CPLGetFilename(osPath), NULL);
        VSIMkdir(osVirtPath, 0666);
    }
    else
        osVirtPath = "/vsimem";
    papszIter = papszFileList;
    for( ; *papszIter != NULL; papszIter++ )
    {
        const char* pszDestFile = CPLFormFilename(osVirtPath, CPLGetFilename(*papszIter), NULL);
        /* CPLDebug("test_ogrsf", "Copying %s to %s", *papszIter, pszDestFile); */
        CPLCopyFile( pszDestFile, *papszIter );
    }
    
    const char* pszVirtFile;
    if( VSI_ISREG(sStat.st_mode) )
        pszVirtFile = CPLFormFilename(osVirtPath, CPLGetFilename(poDS->GetDescription()), NULL);
    else
        pszVirtFile = osVirtPath;
    CPLDebug("test_ogrsf", "Trying to open %s", pszVirtFile);
    GDALDataset* poDS2 = LOG_ACTION((GDALDataset*)GDALOpenEx(
        pszVirtFile, GDAL_OF_VECTOR, NULL, NULL, NULL ));
    if( poDS2 != NULL )
    {
        if( poDS->GetDriver()->GetMetadataItem( GDAL_DCAP_VIRTUALIO ) == NULL )
        {
            printf("WARNING: %s driver apparently supports VirtualIO but does not declare it.\n",
                    poDS->GetDriver()->GetDescription() );
        }
        if( poDS2->GetLayerCount() != poDS->GetLayerCount() )
        {
            printf("WARNING: /vsimem dataset reports %d layers where as base dataset reports %d layers.\n",
                    poDS2->GetLayerCount(), poDS->GetLayerCount() );
        }
        GDALClose( (GDALDatasetH) poDS2 );

        if( bVerbose && bRet )
        {
            printf("INFO: TestVirtualIO successful.\n");
        }
    }
    else
    {
        if( poDS->GetDriver()->GetMetadataItem( GDAL_DCAP_VIRTUALIO ) != NULL )
        {
            printf("WARNING: %s driver declares supporting VirtualIO but "
                    "test with /vsimem does not work. It might be a sign that "
                    "GetFileList() is not properly implemented.\n",
                    poDS->GetDriver()->GetDescription() );
        }
    }

    papszIter = papszFileList;
    for( ; *papszIter != NULL; papszIter++ )
    {
        VSIUnlink( CPLFormFilename(osVirtPath, CPLGetFilename(*papszIter), NULL) );
    }
    CSLDestroy(papszFileList);

    return bRet;
}
