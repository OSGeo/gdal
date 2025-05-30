/******************************************************************************
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  Formal test harness for OGRLayer implementations.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 1999, Frank Warmerdam
 * Copyright (c) 2009-2014, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include "cpl_conv.h"
#include "cpl_multiproc.h"
#include "gdal_version.h"
#include "ogr_api.h"
#include "ogr_p.h"
#include "ogrsf_frmts.h"
#include "ogr_swq.h"
#include "ogr_recordbatch.h"
#include "commonutils.h"

#include <cinttypes>
#include <algorithm>
#include <limits>

bool bReadOnly = false;
bool bVerbose = true;
const char *pszDataSource = nullptr;
char **papszLayers = nullptr;
const char *pszSQLStatement = nullptr;
const char *pszDialect = nullptr;
int nLoops = 1;
bool bFullSpatialFilter = false;
char **papszOpenOptions = nullptr;
const char *pszDriver = nullptr;
bool bAllDrivers = false;
const char *pszLogFilename = nullptr;
char **papszDSCO = nullptr;
char **papszLCO = nullptr;

typedef struct
{
    CPLJoinableThread *hThread;
    int bRet;
} ThreadContext;

static void Usage();
static void ThreadFunction(void *user_data);
static void ThreadFunctionInternal(ThreadContext *psContext);
static int TestDataset(GDALDriver **ppoDriver);
static int TestCreate(GDALDriver *poDriver, int bFromAllDrivers);
static int TestOGRLayer(GDALDataset *poDS, OGRLayer *poLayer, int bIsSQLLayer);
static int TestInterleavedReading(const char *pszDataSource,
                                  char **papszLayers);
static int TestDSErrorConditions(GDALDataset *poDS);
static int TestVirtualIO(GDALDataset *poDS);

static const char *Log(const char *pszMsg, int nLineNumber)
{
    if (pszLogFilename == nullptr)
        return pszMsg;
    FILE *f = fopen(pszLogFilename, "at");
    if (f == nullptr)
        return pszMsg;
    fprintf(f, "%d: %s\n", nLineNumber, pszMsg);
    fclose(f);
    return pszMsg;
}

#define LOG_STR(str) (Log((str), __LINE__))
#define LOG_ACTION(action) ((void)Log(#action, __LINE__), (action))

/************************************************************************/
/*                      DestroyFeatureAndNullify()                      */
/************************************************************************/

static void DestroyFeatureAndNullify(OGRFeature *&poFeature)
{
    OGRFeature::DestroyFeature(poFeature);
    poFeature = nullptr;
}

/************************************************************************/
/*                                main()                                */
/************************************************************************/

MAIN_START(nArgc, papszArgv)

{
    EarlySetConfigOptions(nArgc, papszArgv);

    OGRRegisterAll();

    /* -------------------------------------------------------------------- */
    /*      Processing command line arguments.                              */
    /* -------------------------------------------------------------------- */
    nArgc = OGRGeneralCmdLineProcessor(nArgc, &papszArgv, 0);

    if (nArgc < 1)
        exit(-nArgc);

    int bRet = TRUE;
    int nThreads = 1;

    if (EQUAL(CPLGetConfigOption("DRIVER_WISHED", ""), "FileGDB"))
    {
        auto poFileGDB = GetGDALDriverManager()->GetDriverByName("FileGDB");
        auto poOpenFileGDB =
            GetGDALDriverManager()->GetDriverByName("OpenFileGDB");
        if (poFileGDB && poOpenFileGDB)
        {
            GetGDALDriverManager()->DeregisterDriver(poFileGDB);
            GetGDALDriverManager()->DeregisterDriver(poOpenFileGDB);
            GetGDALDriverManager()->RegisterDriver(poFileGDB);
            GetGDALDriverManager()->RegisterDriver(poOpenFileGDB);
        }
    }

    /* -------------------------------------------------------------------- */
    /*      Processing command line arguments.                              */
    /* -------------------------------------------------------------------- */
    for (int iArg = 1; iArg < nArgc; iArg++)
    {
        if (EQUAL(papszArgv[iArg], "--utility_version"))
        {
            printf("%s was compiled against GDAL %s and "
                   "is running against GDAL %s\n",
                   papszArgv[0], GDAL_RELEASE_NAME,
                   GDALVersionInfo("RELEASE_NAME"));
            CSLDestroy(papszArgv);
            return 0;
        }
        else if (EQUAL(papszArgv[iArg], "-ro"))
        {
            bReadOnly = true;
        }
        else if (EQUAL(papszArgv[iArg], "-q") ||
                 EQUAL(papszArgv[iArg], "-quiet"))
        {
            bVerbose = false;
        }
        else if (EQUAL(papszArgv[iArg], "-sql") && iArg + 1 < nArgc)
        {
            pszSQLStatement = papszArgv[++iArg];
        }
        else if (EQUAL(papszArgv[iArg], "-dialect") && iArg + 1 < nArgc)
        {
            pszDialect = papszArgv[++iArg];
        }
        else if (EQUAL(papszArgv[iArg], "-threads") && iArg + 1 < nArgc)
        {
            nThreads = atoi(papszArgv[++iArg]);
        }
        else if (EQUAL(papszArgv[iArg], "-loops") && iArg + 1 < nArgc)
        {
            nLoops = atoi(papszArgv[++iArg]);
        }
        else if (EQUAL(papszArgv[iArg], "-fsf"))
        {
            bFullSpatialFilter = true;
        }
        else if (EQUAL(papszArgv[iArg], "-oo") && iArg + 1 < nArgc)
        {
            papszOpenOptions =
                CSLAddString(papszOpenOptions, papszArgv[++iArg]);
        }
        else if (EQUAL(papszArgv[iArg], "-dsco") && iArg + 1 < nArgc)
        {
            papszDSCO = CSLAddString(papszDSCO, papszArgv[++iArg]);
        }
        else if (EQUAL(papszArgv[iArg], "-lco") && iArg + 1 < nArgc)
        {
            papszLCO = CSLAddString(papszLCO, papszArgv[++iArg]);
        }
        else if (EQUAL(papszArgv[iArg], "-log") && iArg + 1 < nArgc)
        {
            pszLogFilename = papszArgv[++iArg];
        }
        else if (EQUAL(papszArgv[iArg], "-driver") && iArg + 1 < nArgc)
        {
            pszDriver = papszArgv[++iArg];
        }
        else if (EQUAL(papszArgv[iArg], "-all_drivers"))
        {
            bAllDrivers = true;
        }
        else if (papszArgv[iArg][0] == '-')
        {
            Usage();
        }
        else if (pszDataSource == nullptr)
        {
            pszDataSource = papszArgv[iArg];
        }
        else
        {
            papszLayers = CSLAddString(papszLayers, papszArgv[iArg]);
        }
    }

    if (pszDataSource == nullptr && pszDriver == nullptr && !bAllDrivers)
        Usage();

    if (nThreads > 1 && !bReadOnly && pszDataSource != nullptr)
    {
        fprintf(
            stderr,
            "-threads must be used with -ro or -driver/-all_drivers option.\n");
        exit(1);
    }

    if (nThreads == 1)
    {
        ThreadContext sContext;
        ThreadFunction(&sContext);
        bRet = sContext.bRet;
    }
    else if (nThreads > 1)
    {
        ThreadContext *pasContext = new ThreadContext[nThreads];
        for (int i = 0; i < nThreads; i++)
        {
            pasContext[i].hThread =
                CPLCreateJoinableThread(ThreadFunction, &(pasContext[i]));
        }
        for (int i = 0; i < nThreads; i++)
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

MAIN_END

/************************************************************************/
/*                        ThreadFunction()                              */
/************************************************************************/

static void ThreadFunction(void *user_data)

{
    ThreadContext *psContext = static_cast<ThreadContext *>(user_data);
    psContext->bRet = TRUE;
#ifdef __AFL_HAVE_MANUAL_CONTROL
    while (__AFL_LOOP(1000))
    {
#endif
        for (int iLoop = 0; psContext->bRet && iLoop < nLoops; iLoop++)
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

static void ThreadFunctionInternal(ThreadContext *psContext)

{
    int bRet = TRUE;

    GDALDriver *poDriver = nullptr;

    if (pszDataSource != nullptr)
    {
        bRet = TestDataset(&poDriver);
    }
    else if (pszDriver != nullptr)
    {
        poDriver = static_cast<GDALDriver *>(GDALGetDriverByName(pszDriver));
        if (poDriver)
        {
            bRet &= TestCreate(poDriver, FALSE);
        }
        else
        {
            printf("ERROR: Cannot find driver %s\n", pszDriver);
            bRet = FALSE;
        }
    }
    else
    {
        const int nCount = GDALGetDriverCount();
        for (int i = 0; i < nCount; i++)
        {
            poDriver = static_cast<GDALDriver *>(GDALGetDriver(i));
            if (poDriver->GetMetadataItem(GDAL_DCAP_VECTOR) != nullptr)
                bRet &= TestCreate(poDriver, TRUE);
        }
    }

    psContext->bRet = bRet;
}

/************************************************************************/
/*                            TestDataset()                             */
/************************************************************************/

static int TestDataset(GDALDriver **ppoDriver)
{
    int bRet = TRUE;
    int bRetLocal;

    /* -------------------------------------------------------------------- */
    /*      Open data source.                                               */
    /* -------------------------------------------------------------------- */

    GDALDataset *poDS = static_cast<GDALDataset *>(GDALOpenEx(
        pszDataSource,
        (!bReadOnly ? GDAL_OF_UPDATE : GDAL_OF_READONLY) | GDAL_OF_VECTOR,
        nullptr, papszOpenOptions, nullptr));

    if (poDS == nullptr && !bReadOnly)
    {
        poDS = static_cast<GDALDataset *>(GDALOpenEx(
            pszDataSource, GDAL_OF_VECTOR, nullptr, papszOpenOptions, nullptr));
        if (poDS != nullptr && bVerbose)
        {
            printf("Had to open data source read-only.\n");
            bReadOnly = true;
        }
    }

    GDALDriver *poDriver = nullptr;
    if (poDS != nullptr)
        poDriver = poDS->GetDriver();
    *ppoDriver = poDriver;

    /* -------------------------------------------------------------------- */
    /*      Report failure                                                  */
    /* -------------------------------------------------------------------- */
    if (poDS == nullptr)
    {
        OGRSFDriverRegistrar *poR = OGRSFDriverRegistrar::GetRegistrar();

        printf("FAILURE:\n"
               "Unable to open datasource `%s' with the following drivers.\n",
               pszDataSource);

        for (int iDriver = 0; iDriver < poR->GetDriverCount(); iDriver++)
        {
            printf("  -> %s\n", poR->GetDriver(iDriver)->GetDescription());
        }

        return FALSE;
    }

    /* -------------------------------------------------------------------- */
    /*      Some information messages.                                      */
    /* -------------------------------------------------------------------- */
    if (bVerbose)
        printf("INFO: Open of `%s' using driver `%s' successful.\n",
               pszDataSource, poDriver->GetDescription());

    if (bVerbose && !EQUAL(pszDataSource, poDS->GetDescription()))
    {
        printf("INFO: Internal data source name `%s'\n"
               "      different from user name `%s'.\n",
               poDS->GetDescription(), pszDataSource);
    }

    // Check that pszDomain == nullptr doesn't crash
    poDS->GetMetadata(nullptr);
    poDS->GetMetadataItem("", nullptr);

    if (!poDriver->GetMetadataItem(GDAL_DCAP_VECTOR))
    {
        printf("FAILURE: Driver does not advertise GDAL_DCAP_VECTOR!\n");
        bRet = false;
    }

    // Test consistency of datasource capabilities and driver metadata
    if (poDS->TestCapability(ODsCCreateLayer) &&
        !poDriver->GetMetadataItem(GDAL_DCAP_CREATE_LAYER))
    {
        printf("FAILURE: Dataset advertises ODsCCreateLayer capability but "
               "driver metadata does not advertise GDAL_DCAP_CREATE_LAYER!\n");
        bRet = false;
    }
    if (!bReadOnly &&
        poDriver->GetMetadataItem(GDAL_DCAP_MULTIPLE_VECTOR_LAYERS) &&
        poDriver->GetMetadataItem(GDAL_DCAP_CREATE_LAYER) &&
        !poDS->TestCapability(ODsCCreateLayer))
    {
        printf("FAILURE: Driver advertises GDAL_DCAP_CREATE_LAYER and "
               "GDAL_DCAP_MULTIPLE_VECTOR_LAYERS capability but dataset does "
               "not advertise ODsCCreateLayer!\n");
        bRet = false;
    }

    if (poDS->TestCapability(ODsCDeleteLayer) &&
        !poDriver->GetMetadataItem(GDAL_DCAP_DELETE_LAYER))
    {
        printf("FAILURE: Dataset advertises ODsCDeleteLayer capability but "
               "driver metadata does not advertise GDAL_DCAP_DELETE_LAYER!\n");
        bRet = false;
    }

    if (poDriver->GetMetadataItem(GDAL_DCAP_CREATE_FIELD) &&
        !poDriver->GetMetadataItem(GDAL_DMD_CREATIONFIELDDATATYPES))
    {
        bRet = FALSE;
        printf("FAILURE: Driver metadata advertises GDAL_DCAP_CREATE_FIELD but "
               "does not include GDAL_DMD_CREATIONFIELDDATATYPES!\n");
    }
    if (poDriver->GetMetadataItem(GDAL_DMD_CREATIONFIELDDATATYPES) &&
        !poDriver->GetMetadataItem(GDAL_DCAP_CREATE_FIELD))
    {
        bRet = FALSE;
        printf(
            "FAILURE: Driver metadata includes GDAL_DMD_CREATIONFIELDDATATYPES "
            "but does not advertise GDAL_DCAP_CREATE_FIELD!\n");
    }
    if (poDriver->GetMetadataItem(GDAL_DMD_CREATIONFIELDDATASUBTYPES) &&
        !poDriver->GetMetadataItem(GDAL_DMD_CREATIONFIELDDATATYPES))
    {
        bRet = FALSE;
        printf("FAILURE: Driver metadata includes "
               "GDAL_DMD_CREATIONFIELDDATASUBTYPES but does not include "
               "GDAL_DMD_CREATIONFIELDDATATYPES!\n");
    }

    /* -------------------------------------------------------------------- */
    /*      Process optional SQL request.                                   */
    /* -------------------------------------------------------------------- */
    if (pszSQLStatement != nullptr)
    {
        OGRLayer *poResultSet =
            poDS->ExecuteSQL(pszSQLStatement, nullptr, pszDialect);
        if (poResultSet == nullptr)
        {
            GDALClose(poDS);
            return FALSE;
        }

        if (bVerbose)
        {
            printf("INFO: Testing layer %s.\n", poResultSet->GetName());
        }
        bRet = TestOGRLayer(poDS, poResultSet, TRUE);

        poDS->ReleaseResultSet(poResultSet);

        bRetLocal = TestDSErrorConditions(poDS);
        bRet &= bRetLocal;

        bRetLocal = TestVirtualIO(poDS);
        bRet &= bRetLocal;
    }
    /* -------------------------------------------------------------------- */
    /*      Process each data source layer.                                 */
    /* -------------------------------------------------------------------- */
    else if (papszLayers == nullptr)
    {
        for (int iLayer = 0; iLayer < poDS->GetLayerCount(); iLayer++)
        {
            OGRLayer *poLayer = poDS->GetLayer(iLayer);

            if (poLayer == nullptr)
            {
                printf("FAILURE: Couldn't fetch advertised layer %d!\n",
                       iLayer);
                GDALClose(poDS);
                return FALSE;
            }

            if (bVerbose)
            {
                printf("INFO: Testing layer %s.\n", poLayer->GetName());
            }
            bRet &= TestOGRLayer(poDS, poLayer, FALSE);
        }

        bRetLocal = TestDSErrorConditions(poDS);
        bRet &= bRetLocal;

        bRetLocal = TestVirtualIO(poDS);
        bRet &= bRetLocal;

        if (poDS->GetLayerCount() >= 2)
        {
            GDALClose(poDS);
            poDS = nullptr;
            bRetLocal = TestInterleavedReading(pszDataSource, nullptr);
            bRet &= bRetLocal;
        }
    }
    else
    {
        /* --------------------------------------------------------------------
         */
        /*      Or process layers specified by the user */
        /* --------------------------------------------------------------------
         */
        char **papszLayerIter = papszLayers;
        while (*papszLayerIter)
        {
            OGRLayer *poLayer = poDS->GetLayerByName(*papszLayerIter);

            if (poLayer == nullptr)
            {
                printf("FAILURE: Couldn't fetch requested layer %s!\n",
                       *papszLayerIter);
                GDALClose(poDS);
                return FALSE;
            }

            if (bVerbose)
            {
                printf("INFO: Testing layer %s.\n", poLayer->GetName());
            }
            bRet &= TestOGRLayer(poDS, poLayer, FALSE);

            papszLayerIter++;
        }

        bRetLocal = TestDSErrorConditions(poDS);
        bRet &= bRetLocal;

        bRetLocal = TestVirtualIO(poDS);
        bRet &= bRetLocal;

        if (CSLCount(papszLayers) >= 2)
        {
            GDALClose(poDS);
            poDS = nullptr;
            bRetLocal = TestInterleavedReading(pszDataSource, papszLayers);
            bRet &= bRetLocal;
        }
    }

    /* -------------------------------------------------------------------- */
    /*      Close down.                                                     */
    /* -------------------------------------------------------------------- */
    if (poDS != nullptr)
        GDALClose(poDS);

    return bRet;
}

/************************************************************************/
/*                             GetWKT()                                 */
/************************************************************************/

static const char *GetWKT(OGRwkbGeometryType eGeomType)
{
    const char *pszWKT = nullptr;
    if (eGeomType == wkbUnknown || eGeomType == wkbPoint)
        pszWKT = "POINT (0 0)";
    else if (eGeomType == wkbLineString)
        pszWKT = "LINESTRING (0 0,1 1)";
    else if (eGeomType == wkbPolygon)
        pszWKT = "POLYGON ((0 0,0 1,1 1,1 0,0 0))";
    else if (eGeomType == wkbMultiPoint)
        pszWKT = "MULTIPOINT (0 0)";
    else if (eGeomType == wkbMultiLineString)
        pszWKT = "MULTILINESTRING ((0 0,1 1))";
    else if (eGeomType == wkbMultiPolygon)
        pszWKT = "MULTIPOLYGON (((0 0,0 1,1 1,1 0,0 0)))";
    else if (eGeomType == wkbGeometryCollection)
        pszWKT = "GEOMETRYCOLLECTION (POINT (0 0),LINESTRING (0 0,1 1),"
                 "POLYGON ((0 0,0 1,1 1,1 0,0 0)))";
    else if (eGeomType == wkbPoint25D)
        pszWKT = "POINT (0 0 10)";
    else if (eGeomType == wkbLineString25D)
        pszWKT = "LINESTRING (0 0 10,1 1 10)";
    else if (eGeomType == wkbPolygon25D)
        pszWKT = "POLYGON ((0 0 10,0 1 10,1 1 10,1 0 10,0 0 10))";
    else if (eGeomType == wkbMultiPoint25D)
        pszWKT = "MULTIPOINT (0 0 10)";
    else if (eGeomType == wkbMultiLineString25D)
        pszWKT = "MULTILINESTRING ((0 0 10,1 1 10))";
    else if (eGeomType == wkbMultiPolygon25D)
        pszWKT = "MULTIPOLYGON (((0 0 10,0 1 10,1 1 10,1 0 10,0 0 10)))";
    else if (eGeomType == wkbGeometryCollection25D)
        pszWKT =
            "GEOMETRYCOLLECTION (POINT (0 0 10),LINESTRING (0 0 10,1 1 10),"
            "POLYGON ((0 0 10,0 1 10,1 1 10,1 0 10,0 0 10)))";
    return pszWKT;
}

/************************************************************************/
/*                         TestCreateLayer()                            */
/************************************************************************/

static int TestCreateLayer(GDALDriver *poDriver, OGRwkbGeometryType eGeomType)
{
    int bRet = TRUE;
    const char *pszExt = poDriver->GetMetadataItem(GDAL_DMD_EXTENSION);

    static int nCounter = 0;
    CPLString osFilename = CPLFormFilenameSafe(
        "/vsimem", CPLSPrintf("test%d", ++nCounter), pszExt);
    GDALDataset *poDS = LOG_ACTION(
        poDriver->Create(osFilename, 0, 0, 0, GDT_Unknown, papszDSCO));
    if (poDS == nullptr)
    {
        if (bVerbose)
            printf("INFO: %s: Creation of %s failed.\n",
                   poDriver->GetDescription(), osFilename.c_str());
        return bRet;
    }
    CPLPushErrorHandler(CPLQuietErrorHandler);
    int bCreateLayerCap = LOG_ACTION(poDS->TestCapability(ODsCCreateLayer));
    OGRLayer *poLayer = LOG_ACTION(poDS->CreateLayer(
        CPLGetFilename(osFilename), nullptr, eGeomType, papszLCO));
    CPLPopErrorHandler();
    CPLString osLayerNameToTest;
    OGRwkbGeometryType eExpectedGeomType = wkbUnknown;
    if (poLayer != nullptr)
    {
        if (bCreateLayerCap == FALSE)
        {
            printf("ERROR: %s: TestCapability(ODsCCreateLayer) returns FALSE "
                   "whereas layer creation was successful.\n",
                   poDriver->GetDescription());
            bRet = FALSE;
        }

        if (LOG_ACTION(poLayer->GetLayerDefn()) == nullptr)
        {
            printf("ERROR: %s: GetLayerDefn() returns NUL just after layer "
                   "creation.\n",
                   poDriver->GetDescription());
            bRet = FALSE;
        }

        auto poLyrDS = LOG_ACTION(poLayer->GetDataset());
        if (!poLyrDS)
        {
            printf("ERROR: %s: GetDataset() returns NUL just after layer "
                   "creation.\n",
                   poDriver->GetDescription());
            bRet = FALSE;
        }
        else if (poLyrDS != poDS)
        {
            printf("WARNING: %s: GetDataset()->GetDescription() (=%s) != %s"
                   "creation.\n",
                   poDriver->GetDescription(), poLyrDS->GetDescription(),
                   poDS->GetDescription());
        }

        // Create fields of various types
        int bCreateField = LOG_ACTION(poLayer->TestCapability(OLCCreateField));
        int iFieldStr = -1;
        int iFieldInt = -1;
        int iFieldReal = -1;
        int iFieldDate = -1;
        int iFieldDateTime = -1;
        int bStrFieldOK;
        {
            OGRFieldDefn oFieldStr("str", OFTString);
            CPLPushErrorHandler(CPLQuietErrorHandler);
            bStrFieldOK =
                LOG_ACTION(poLayer->CreateField(&oFieldStr)) == OGRERR_NONE;
            CPLPopErrorHandler();
            if (bStrFieldOK && (iFieldStr = LOG_ACTION(poLayer->GetLayerDefn())
                                                ->GetFieldIndex("str")) < 0)
            {
                printf("ERROR: %s: CreateField(str) returned OK "
                       "but field was not created.\n",
                       poDriver->GetDescription());
                bRet = FALSE;
            }
        }

        OGRFieldDefn oFieldInt("int", OFTInteger);
        CPLPushErrorHandler(CPLQuietErrorHandler);
        const bool bIntFieldOK =
            LOG_ACTION(poLayer->CreateField(&oFieldInt)) == OGRERR_NONE;
        CPLPopErrorHandler();
        if (bIntFieldOK &&
            (iFieldInt = poLayer->GetLayerDefn()->GetFieldIndex("int")) < 0)
        {
            printf("ERROR: %s: CreateField(int) returned OK "
                   "but field was not created.\n",
                   poDriver->GetDescription());
            bRet = FALSE;
        }

        OGRFieldDefn oFieldReal("real", OFTReal);
        CPLPushErrorHandler(CPLQuietErrorHandler);
        const bool bRealFieldOK =
            LOG_ACTION(poLayer->CreateField(&oFieldReal)) == OGRERR_NONE;
        CPLPopErrorHandler();
        if (bRealFieldOK &&
            (iFieldReal = poLayer->GetLayerDefn()->GetFieldIndex("real")) < 0)
        {
            printf("ERROR: %s: CreateField(real) returned OK but field was not "
                   "created.\n",
                   poDriver->GetDescription());
            bRet = FALSE;
        }

        OGRFieldDefn oFieldDate("mydate", OFTDate);
        CPLPushErrorHandler(CPLQuietErrorHandler);
        const bool bDateFieldOK =
            LOG_ACTION(poLayer->CreateField(&oFieldDate)) == OGRERR_NONE;
        CPLPopErrorHandler();
        if (bDateFieldOK &&
            (iFieldDate = poLayer->GetLayerDefn()->GetFieldIndex("mydate")) < 0)
        {
            printf("ERROR: %s: CreateField(mydate) returned OK but field was "
                   "not created.\n",
                   poDriver->GetDescription());
            bRet = FALSE;
        }

        OGRFieldDefn oFieldDateTime("datetime", OFTDateTime);
        CPLPushErrorHandler(CPLQuietErrorHandler);
        const bool bDateTimeFieldOK =
            LOG_ACTION(poLayer->CreateField(&oFieldDateTime)) == OGRERR_NONE;
        CPLPopErrorHandler();
        if (bDateTimeFieldOK &&
            (iFieldDateTime =
                 poLayer->GetLayerDefn()->GetFieldIndex("datetime")) < 0)
        {
            printf("ERROR: %s: CreateField(datetime) returned OK but field was "
                   "not created.\n",
                   poDriver->GetDescription());
            bRet = FALSE;
        }

        if (bCreateField == FALSE &&
            (bStrFieldOK || bIntFieldOK || bRealFieldOK || bDateFieldOK ||
             bDateTimeFieldOK))
        {
            printf("ERROR: %s: TestCapability(OLCCreateField) returns FALSE.\n",
                   poDriver->GetDescription());
            bRet = FALSE;
        }

        if (LOG_ACTION(poLayer->TestCapability(OLCSequentialWrite)) == FALSE)
        {
            printf("ERROR: %s: TestCapability(OLCSequentialWrite) returns "
                   "FALSE.\n",
                   poDriver->GetDescription());
            bRet = FALSE;
        }

        /* Test creating empty feature */
        OGRFeature *poFeature = new OGRFeature(poLayer->GetLayerDefn());
        CPLErrorReset();
        CPLPushErrorHandler(CPLQuietErrorHandler);
        OGRErr eErr = LOG_ACTION(poLayer->CreateFeature(poFeature));
        CPLPopErrorHandler();
        if (eErr != OGRERR_NONE && CPLGetLastErrorType() == 0)
        {
            printf("INFO: %s: CreateFeature() at line %d failed but without "
                   "explicit error.\n",
                   poDriver->GetDescription(), __LINE__);
        }
        if (eErr == OGRERR_NONE && poFeature->GetFID() < 0 &&
            eGeomType == wkbUnknown)
        {
            printf("INFO: %s: CreateFeature() at line %d succeeded "
                   "but failed to assign FID to feature.\n",
                   poDriver->GetDescription(), __LINE__);
        }
        delete poFeature;

        /* Test creating feature with all fields set */
        poFeature = new OGRFeature(poLayer->GetLayerDefn());
        if (bStrFieldOK)
            poFeature->SetField(iFieldStr, "foo");
        if (bIntFieldOK)
            poFeature->SetField(iFieldInt, 123);
        if (bRealFieldOK)
            poFeature->SetField(iFieldReal, 1.23);
        if (bDateFieldOK)
            poFeature->SetField(iFieldDate, "2014/10/20");
        if (bDateTimeFieldOK)
            poFeature->SetField(iFieldDateTime, "2014/10/20 12:34:56");
        CPLErrorReset();
        CPLPushErrorHandler(CPLQuietErrorHandler);
        eErr = LOG_ACTION(poLayer->CreateFeature(poFeature));
        CPLPopErrorHandler();
        if (eErr != OGRERR_NONE && CPLGetLastErrorType() == 0)
        {
            printf("INFO: %s: CreateFeature() at line %d failed "
                   "but without explicit error.\n",
                   poDriver->GetDescription(), __LINE__);
        }
        delete poFeature;

        /* Test creating feature with all fields set as well as geometry */
        poFeature = new OGRFeature(poLayer->GetLayerDefn());
        if (bStrFieldOK)
            poFeature->SetField(iFieldStr, "foo");
        if (bIntFieldOK)
            poFeature->SetField(iFieldInt, 123);
        if (bRealFieldOK)
            poFeature->SetField(iFieldReal, 1.23);
        if (bDateFieldOK)
            poFeature->SetField(iFieldDate, "2014/10/20");
        if (bDateTimeFieldOK)
            poFeature->SetField(iFieldDateTime, "2014/10/20 12:34:56");

        const char *pszWKT = GetWKT(eGeomType);
        if (pszWKT != nullptr)
        {
            auto [poGeom, _] = OGRGeometryFactory::createFromWkt(pszWKT);
            poFeature->SetGeometry(std::move(poGeom));
        }

        CPLErrorReset();
        CPLPushErrorHandler(CPLQuietErrorHandler);
        eErr = LOG_ACTION(poLayer->CreateFeature(poFeature));
        CPLPopErrorHandler();
        if (eErr != OGRERR_NONE && CPLGetLastErrorType() == 0)
        {
            printf("INFO: %s: CreateFeature() at line %d failed "
                   "but without explicit error.\n",
                   poDriver->GetDescription(), __LINE__);
        }
        delete poFeature;

        /* Test feature with incompatible geometry */
        poFeature = new OGRFeature(poLayer->GetLayerDefn());
        if (bStrFieldOK)
            poFeature->SetField(iFieldStr, "foo");
        if (bIntFieldOK)
            poFeature->SetField(iFieldInt, 123);
        if (bRealFieldOK)
            poFeature->SetField(iFieldReal, 1.23);
        if (bDateFieldOK)
            poFeature->SetField(iFieldDate, "2014/10/20");
        if (bDateTimeFieldOK)
            poFeature->SetField(iFieldDateTime, "2014/10/20 12:34:56");

        OGRwkbGeometryType eOtherGeomType;
        if (eGeomType == wkbUnknown || eGeomType == wkbNone)
            eOtherGeomType = wkbLineString;
        else if (wkbFlatten(eGeomType) == eGeomType)
            eOtherGeomType = static_cast<OGRwkbGeometryType>(
                (static_cast<int>(eGeomType) % 7) + 1);
        else
            eOtherGeomType = wkbSetZ(static_cast<OGRwkbGeometryType>(
                ((static_cast<int>(wkbFlatten(eGeomType)) % 7) + 1)));
        pszWKT = GetWKT(eOtherGeomType);
        if (pszWKT != nullptr)
        {
            auto [poGeom, _] = OGRGeometryFactory::createFromWkt(pszWKT);
            poFeature->SetGeometry(std::move(poGeom));
        }

        CPLErrorReset();
        CPLPushErrorHandler(CPLQuietErrorHandler);
        eErr = LOG_ACTION(poLayer->CreateFeature(poFeature));
        CPLPopErrorHandler();
        if (eErr != OGRERR_NONE && CPLGetLastErrorType() == 0)
        {
            printf("INFO: %s: CreateFeature() at line %d failed "
                   "but without explicit error.\n",
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
        const int bCreateLayerCap2 =
            LOG_ACTION(poDS->TestCapability(ODsCCreateLayer));
        OGRLayer *poLayer2 = LOG_ACTION(poDS->CreateLayer(
            CPLSPrintf("%s2", CPLGetFilename(osFilename)), nullptr, eGeomType));
        CPLPopErrorHandler();
        if (poLayer2 == nullptr && bCreateLayerCap2)
        {
            printf("INFO: %s: Creation of second layer failed but "
                   "TestCapability(ODsCCreateLayer) succeeded.\n",
                   poDriver->GetDescription());
        }
        else if (!EQUAL(poDriver->GetDescription(), "CSV") &&
                 poLayer2 != nullptr)
        {
            OGRFieldDefn oFieldStr("str", OFTString);
            CPLPushErrorHandler(CPLQuietErrorHandler);
            LOG_ACTION(poLayer2->CreateField(&oFieldStr));
            CPLPopErrorHandler();

            poFeature = new OGRFeature(poLayer2->GetLayerDefn());
            pszWKT = GetWKT(eGeomType);
            if (pszWKT != nullptr)
            {
                auto [poGeom, _] = OGRGeometryFactory::createFromWkt(pszWKT);
                poFeature->SetGeometry(std::move(poGeom));
            }
            CPLErrorReset();
            CPLPushErrorHandler(CPLQuietErrorHandler);
            eErr = LOG_ACTION(poLayer2->CreateFeature(poFeature));
            CPLPopErrorHandler();
            delete poFeature;

            if (eErr == OGRERR_NONE)
            {
                osLayerNameToTest = poLayer2->GetName();
                eExpectedGeomType = poLayer2->GetGeomType();
            }
        }

        /* Test deleting first layer */
        const int bDeleteLayerCap =
            LOG_ACTION(poDS->TestCapability(ODsCDeleteLayer));
        CPLPushErrorHandler(CPLQuietErrorHandler);
        eErr = LOG_ACTION(poDS->DeleteLayer(0));
        CPLPopErrorHandler();
        if (eErr == OGRERR_NONE)
        {
            if (!bDeleteLayerCap)
            {
                printf("ERROR: %s: TestCapability(ODsCDeleteLayer) "
                       "returns FALSE but layer deletion worked.\n",
                       poDriver->GetDescription());
                bRet = FALSE;
            }

            if (LOG_ACTION(poDS->GetLayerByName(CPLGetFilename(osFilename))) !=
                nullptr)
            {
                printf("ERROR: %s: DeleteLayer() declared success, "
                       "but layer can still be fetched.\n",
                       poDriver->GetDescription());
                bRet = FALSE;
            }
        }
        else
        {
            if (bDeleteLayerCap)
            {
                printf("ERROR: %s: TestCapability(ODsCDeleteLayer) "
                       "returns TRUE but layer deletion failed.\n",
                       poDriver->GetDescription());
                bRet = FALSE;
            }
        }
    }
    /*else
    {
        if( bVerbose )
            printf("INFO: %s: Creation of layer with geom_type = %s failed.\n",
                   poDriver->GetDescription(),
    OGRGeometryTypeToName(eGeomType));
    }*/
    LOG_ACTION(GDALClose(poDS));

    if (eExpectedGeomType != wkbUnknown &&
        /* Those drivers are expected not to store a layer geometry type */
        !EQUAL(poDriver->GetDescription(), "KML") &&
        !EQUAL(poDriver->GetDescription(), "LIBKML") &&
        !EQUAL(poDriver->GetDescription(), "PDF") &&
        !EQUAL(poDriver->GetDescription(), "GeoJSON") &&
        !EQUAL(poDriver->GetDescription(), "JSONFG") &&
        !EQUAL(poDriver->GetDescription(), "OGR_GMT") &&
        !EQUAL(poDriver->GetDescription(), "PDS4") &&
        !EQUAL(poDriver->GetDescription(), "FlatGeobuf"))
    {
        /* Reopen dataset */
        poDS = LOG_ACTION(static_cast<GDALDataset *>(
            GDALOpenEx(osFilename, GDAL_OF_VECTOR, nullptr, nullptr, nullptr)));
        if (poDS != nullptr)
        {
            poLayer = LOG_ACTION(poDS->GetLayerByName(osLayerNameToTest));
            if (poLayer != nullptr)
            {
                if (poLayer->GetGeomType() != eExpectedGeomType &&
                    !(eGeomType == wkbGeometryCollection25D &&
                      EQUAL(poDriver->GetDescription(), "OpenFileGDB")))
                {
                    printf("ERROR: %s: GetGeomType() returns %d but %d "
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

    if (poLayer != nullptr)
    {
        /* Test creating empty layer */
        osFilename = CPLFormFilenameSafe(
            "/vsimem", CPLSPrintf("test%d", ++nCounter), pszExt);
        poDS = LOG_ACTION(
            poDriver->Create(osFilename, 0, 0, 0, GDT_Unknown, nullptr));
        if (poDS != nullptr)
        {
            CPLPushErrorHandler(CPLQuietErrorHandler);
            LOG_ACTION(poDS->CreateLayer(CPLGetFilename(osFilename), nullptr,
                                         eGeomType));
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

static int TestCreate(GDALDriver *poDriver, int bFromAllDrivers)
{
    int bRet = TRUE;
    const bool bVirtualIO =
        poDriver->GetMetadataItem(GDAL_DCAP_VIRTUALIO) != nullptr;
    if (poDriver->GetMetadataItem(GDAL_DCAP_CREATE) == nullptr || !bVirtualIO)
    {
        if (bVerbose && !bFromAllDrivers)
            printf("INFO: %s: TestCreate skipped.\n",
                   poDriver->GetDescription());
        return TRUE;
    }

    printf("%s\n", LOG_STR(CPLSPrintf("INFO: TestCreate(%s).",
                                      poDriver->GetDescription())));

    const char *pszExt = poDriver->GetMetadataItem(GDAL_DMD_EXTENSION);
    CPLString osFilename = CPLFormFilenameSafe("/foo", "test", pszExt);
    CPLPushErrorHandler(CPLQuietErrorHandler);
    GDALDataset *poDS =
        LOG_ACTION(poDriver->Create(osFilename, 0, 0, 0, GDT_Unknown, nullptr));
    CPLPopErrorHandler();
    if (poDS != nullptr)
    {
        /* Sometimes actual file creation is deferred */
        CPLPushErrorHandler(CPLQuietErrorHandler);
        OGRLayer *poLayer =
            LOG_ACTION(poDS->CreateLayer("test", nullptr, wkbPoint));
        CPLPopErrorHandler();

        /* Or sometimes writing is deferred at dataset closing */
        CPLErrorReset();
        CPLPushErrorHandler(CPLQuietErrorHandler);
        LOG_ACTION(GDALClose(poDS));
        CPLPopErrorHandler();
        if (poLayer != nullptr && CPLGetLastErrorType() == 0)
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
    printf("Usage: test_ogrsf [-ro] [-q] [-threads N] [-loops M] [-fsf]\n"
           "                  (datasource_name | [-driver driver_name] [[-dsco "
           "NAME=VALUE] ...] [[-lco NAME=VALUE] ...] | -all_drivers) \n"
           "                  [[layer1_name, layer2_name, ...] | [-sql "
           "statement] [-dialect dialect]]\n"
           "                   [[-oo NAME=VALUE] ...]\n");
    printf("\n");
    printf("-fsf : full spatial filter testing (slow)\n");
    exit(1);
}

/************************************************************************/
/*                           TestBasic()                                */
/************************************************************************/

static int TestBasic(GDALDataset *poDS, OGRLayer *poLayer)
{
    int bRet = TRUE;

    const char *pszLayerName = LOG_ACTION(poLayer->GetName());
    const OGRwkbGeometryType eGeomType = LOG_ACTION(poLayer->GetGeomType());
    OGRFeatureDefn *poFDefn = LOG_ACTION(poLayer->GetLayerDefn());

    if (strcmp(pszLayerName, LOG_ACTION(poFDefn->GetName())) != 0)
    {
        bRet = FALSE;
        printf("ERROR: poLayer->GetName() and poFDefn->GetName() differ.\n"
               "poLayer->GetName() = %s\n"
               "poFDefn->GetName() = %s\n",
               pszLayerName, poFDefn->GetName());
    }

    if (strcmp(pszLayerName, LOG_ACTION(poLayer->GetDescription())) != 0)
    {
        bRet = FALSE;
        printf(
            "ERROR: poLayer->GetName() and poLayer->GetDescription() differ.\n"
            "poLayer->GetName() = %s\n"
            "poLayer->GetDescription() = %s\n",
            pszLayerName, poLayer->GetDescription());
    }

    if (eGeomType != LOG_ACTION(poFDefn->GetGeomType()))
    {
        bRet = FALSE;
        printf(
            "ERROR: poLayer->GetGeomType() and poFDefn->GetGeomType() differ.\n"
            "poLayer->GetGeomType() = %d\n"
            "poFDefn->GetGeomType() = %d\n",
            eGeomType, poFDefn->GetGeomType());
    }

    if (LOG_ACTION(poLayer->GetFIDColumn()) == nullptr)
    {
        bRet = FALSE;
        printf("ERROR: poLayer->GetFIDColumn() returned NULL.\n");
    }

    if (LOG_ACTION(poLayer->GetGeometryColumn()) == nullptr)
    {
        bRet = FALSE;
        printf("ERROR: poLayer->GetGeometryColumn() returned NULL.\n");
    }

    if (LOG_ACTION(poFDefn->GetGeomFieldCount()) > 0)
    {
        if (eGeomType != LOG_ACTION(poFDefn->GetGeomFieldDefn(0))->GetType())
        {
            bRet = FALSE;
            printf("ERROR: poLayer->GetGeomType() and "
                   "poFDefn->GetGeomFieldDefn(0)->GetType() differ.\n"
                   "poLayer->GetGeomType() = %d\n"
                   "poFDefn->GetGeomFieldDefn(0)->GetType() = %d\n",
                   eGeomType, poFDefn->GetGeomFieldDefn(0)->GetType());
        }

        if (!EQUAL(LOG_ACTION(poLayer->GetGeometryColumn()),
                   poFDefn->GetGeomFieldDefn(0)->GetNameRef()))
        {
            if (poFDefn->GetGeomFieldCount() > 1)
                bRet = FALSE;
            printf("%s: poLayer->GetGeometryColumn() and "
                   "poFDefn->GetGeomFieldDefn(0)->GetNameRef() differ.\n"
                   "poLayer->GetGeometryColumn() = %s\n"
                   "poFDefn->GetGeomFieldDefn(0)->GetNameRef() = %s\n",
                   (poFDefn->GetGeomFieldCount() == 1) ? "WARNING" : "ERROR",
                   poLayer->GetGeometryColumn(),
                   poFDefn->GetGeomFieldDefn(0)->GetNameRef());
        }

        if (LOG_ACTION(poLayer->GetSpatialRef()) !=
            LOG_ACTION(poFDefn->GetGeomFieldDefn(0)->GetSpatialRef()))
        {
            if (poFDefn->GetGeomFieldCount() > 1)
                bRet = FALSE;
            printf("%s: poLayer->GetSpatialRef() and "
                   "poFDefn->GetGeomFieldDefn(0)->GetSpatialRef() differ.\n"
                   "poLayer->GetSpatialRef() = %p\n"
                   "poFDefn->GetGeomFieldDefn(0)->GetSpatialRef() = %p\n",
                   (poFDefn->GetGeomFieldCount() == 1) ? "WARNING" : "ERROR",
                   poLayer->GetSpatialRef(),
                   poFDefn->GetGeomFieldDefn(0)->GetSpatialRef());
        }
    }

    // Test consistency of layer capabilities and driver metadata
    if (poDS)
    {
        GDALDriver *poDriver = poDS->GetDriver();

        const bool bLayerShouldHaveEditCapabilities =
            !bReadOnly && !pszSQLStatement;

        // metadata measure tests
        if (poLayer->TestCapability(OLCMeasuredGeometries) &&
            !poDriver->GetMetadataItem(GDAL_DCAP_MEASURED_GEOMETRIES))
        {
            bRet = FALSE;
            printf("FAILURE: Layer advertises OLCMeasuredGeometries capability "
                   "but driver metadata does not advertise "
                   "GDAL_DCAP_MEASURED_GEOMETRIES!\n");
        }
        if (poDS->TestCapability(ODsCMeasuredGeometries) &&
            !poDriver->GetMetadataItem(GDAL_DCAP_MEASURED_GEOMETRIES))
        {
            bRet = FALSE;
            printf("FAILURE: Datasource advertises ODsCMeasuredGeometries "
                   "capability but driver metadata does not advertise "
                   "GDAL_DCAP_MEASURED_GEOMETRIES!\n");
        }
        if (poLayer->TestCapability(OLCMeasuredGeometries) &&
            !poDS->TestCapability(ODsCMeasuredGeometries))
        {
            bRet = FALSE;
            printf(
                "FAILURE: Layer advertises OLCMeasuredGeometries capability "
                "but datasource does not advertise ODsCMeasuredGeometries!\n");
        }

        // metadata curve tests
        if (poLayer->TestCapability(OLCCurveGeometries) &&
            !poDriver->GetMetadataItem(GDAL_DCAP_CURVE_GEOMETRIES))
        {
            bRet = FALSE;
            printf("FAILURE: Layer advertises OLCCurveGeometries capability "
                   "but driver metadata does not advertise "
                   "GDAL_DCAP_CURVE_GEOMETRIES!\n");
        }
        if (poDS->TestCapability(ODsCCurveGeometries) &&
            !poDriver->GetMetadataItem(GDAL_DCAP_CURVE_GEOMETRIES))
        {
            bRet = FALSE;
            printf("FAILURE: Datasource advertises ODsCCurveGeometries "
                   "capability but driver metadata does not advertise "
                   "GDAL_DCAP_CURVE_GEOMETRIES!\n");
        }
        if (poLayer->TestCapability(OLCCurveGeometries) &&
            !poDS->TestCapability(ODsCCurveGeometries))
        {
            bRet = FALSE;
            printf("FAILURE: Layer advertises OLCCurveGeometries capability "
                   "but datasource does not advertise ODsCCurveGeometries!\n");
        }

        // metadata z dimension tests
        if (poLayer->TestCapability(OLCZGeometries) &&
            !poDriver->GetMetadataItem(GDAL_DCAP_Z_GEOMETRIES))
        {
            bRet = FALSE;
            printf(
                "FAILURE: Layer advertises OLCZGeometries capability but "
                "driver metadata does not advertise GDAL_DCAP_Z_GEOMETRIES!\n");
        }
        if (poDS->TestCapability(ODsCZGeometries) &&
            !poDriver->GetMetadataItem(GDAL_DCAP_Z_GEOMETRIES))
        {
            bRet = FALSE;
            printf(
                "FAILURE: Datasource advertises ODsCZGeometries capability but "
                "driver metadata does not advertise GDAL_DCAP_Z_GEOMETRIES!\n");
        }
        if (poLayer->TestCapability(OLCZGeometries) &&
            !poDS->TestCapability(ODsCZGeometries))
        {
            bRet = FALSE;
            printf("FAILURE: Layer advertises OLCZGeometries capability but "
                   "datasource does not advertise ODsCZGeometries!\n");
        }

        // note -- it's not safe to test the reverse case for these next two
        // situations as some drivers only support CreateField() on newly
        // created layers before the first feature is written
        if (poLayer->TestCapability(OLCCreateField) &&
            !poDriver->GetMetadataItem(GDAL_DCAP_CREATE_FIELD))
        {
            bRet = FALSE;
            printf(
                "FAILURE: Layer advertises OLCCreateField capability but "
                "driver metadata does not advertise GDAL_DCAP_CREATE_FIELD!\n");
        }
        if (poLayer->TestCapability(OLCCreateField) &&
            !poDriver->GetMetadataItem(GDAL_DMD_CREATIONFIELDDATATYPES))
        {
            bRet = FALSE;
            printf("FAILURE: Layer advertises OLCCreateField capability but "
                   "driver metadata does not include "
                   "GDAL_DMD_CREATIONFIELDDATATYPES!\n");
        }

        if (poLayer->TestCapability(OLCDeleteField) &&
            !poDriver->GetMetadataItem(GDAL_DCAP_DELETE_FIELD))
        {
            bRet = FALSE;
            printf(
                "FAILURE: Layer advertises OLCDeleteField capability but "
                "driver metadata does not advertise GDAL_DCAP_DELETE_FIELD!\n");
        }
        if (bLayerShouldHaveEditCapabilities &&
            poDriver->GetMetadataItem(GDAL_DCAP_DELETE_FIELD) &&
            !poLayer->TestCapability(OLCDeleteField))
        {
            bRet = FALSE;
            printf("FAILURE: Driver metadata advertises GDAL_DCAP_DELETE_FIELD "
                   "but layer capability does not advertise OLCDeleteField!\n");
        }

        if (poLayer->TestCapability(OLCReorderFields) &&
            !poDriver->GetMetadataItem(GDAL_DCAP_REORDER_FIELDS))
        {
            bRet = FALSE;
            printf("FAILURE: Layer advertises OLCReorderFields capability but "
                   "driver metadata does not advertise "
                   "GDAL_DCAP_REORDER_FIELDS!\n");
        }
        if (bLayerShouldHaveEditCapabilities &&
            poDriver->GetMetadataItem(GDAL_DCAP_REORDER_FIELDS) &&
            !poLayer->TestCapability(OLCReorderFields))
        {
            bRet = FALSE;
            printf(
                "FAILURE: Driver metadata advertises GDAL_DCAP_REORDER_FIELDS "
                "but layer capability does not advertise OLCReorderFields!\n");
        }

        if (poLayer->TestCapability(OLCAlterFieldDefn) &&
            !poDriver->GetMetadataItem(GDAL_DMD_ALTER_FIELD_DEFN_FLAGS))
        {
            bRet = FALSE;
            printf("FAILURE: Layer advertises OLCAlterFieldDefn capability but "
                   "driver metadata does not include "
                   "GDAL_DMD_ALTER_FIELD_DEFN_FLAGS!\n");
        }
        if (bLayerShouldHaveEditCapabilities &&
            poDriver->GetMetadataItem(GDAL_DMD_ALTER_FIELD_DEFN_FLAGS) &&
            !poLayer->TestCapability(OLCAlterFieldDefn))
        {
            bRet = FALSE;
            printf("FAILURE: Driver metadata advertises "
                   "GDAL_DMD_ALTER_FIELD_DEFN_FLAGS but layer capability does "
                   "not advertise OLCAlterFieldDefn!\n");
        }
    }

    return bRet;
}

/************************************************************************/
/*                      TestLayerErrorConditions()                      */
/************************************************************************/

static int TestLayerErrorConditions(OGRLayer *poLyr)
{
    int bRet = TRUE;
    OGRFeature *poFeat = nullptr;

    CPLPushErrorHandler(CPLQuietErrorHandler);

    if (LOG_ACTION(poLyr->TestCapability("fake_capability")))
    {
        printf("ERROR: poLyr->TestCapability(\"fake_capability\") "
               "should have returned FALSE\n");
        bRet = FALSE;
        goto bye;
    }

    if (LOG_ACTION(poLyr->GetFeature(-10)) != nullptr)
    {
        printf("ERROR: GetFeature(-10) should have returned NULL\n");
        bRet = FALSE;
        goto bye;
    }

    if (LOG_ACTION(poLyr->GetFeature(2000000000)) != nullptr)
    {
        printf("ERROR: GetFeature(2000000000) should have returned NULL\n");
        bRet = FALSE;
        goto bye;
    }

    // This should detect int overflow
    if (LOG_ACTION(poLyr->GetFeature(
            static_cast<GIntBig>(std::numeric_limits<int>::max()) + 1)) !=
        nullptr)
    {
        printf("ERROR: GetFeature((GIntBig)INT_MAX + 1) "
               "should have returned NULL\n");
        bRet = FALSE;
        goto bye;
    }

    poLyr->ResetReading();
    poFeat = poLyr->GetNextFeature();
    if (poFeat)
    {
        poFeat->SetFID(-10);
        if (poLyr->SetFeature(poFeat) == OGRERR_NONE)
        {
            printf("ERROR: SetFeature(-10) should have returned an error\n");
            delete poFeat;
            bRet = FALSE;
            goto bye;
        }
        delete poFeat;
    }

    if (poLyr->DeleteFeature(-10) == OGRERR_NONE)
    {
        printf("ERROR: DeleteFeature(-10) should have returned an error\n");
        bRet = FALSE;
        goto bye;
    }

    if (poLyr->DeleteFeature(2000000000) == OGRERR_NONE)
    {
        printf("ERROR: DeleteFeature(2000000000) should have "
               "returned an error\n");
        bRet = FALSE;
        goto bye;
    }

    if (LOG_ACTION(poLyr->SetNextByIndex(-10)) != OGRERR_FAILURE)
    {
        printf("ERROR: SetNextByIndex(-10) should have "
               "returned OGRERR_FAILURE\n");
        bRet = FALSE;
        goto bye;
    }

    if (LOG_ACTION(poLyr->SetNextByIndex(2000000000)) == OGRERR_NONE &&
        LOG_ACTION(poLyr->GetNextFeature()) != nullptr)
    {
        printf("ERROR: SetNextByIndex(2000000000) and then GetNextFeature() "
               "should have returned NULL\n");
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

static const char *GetLayerNameForSQL(GDALDataset *poDS,
                                      const char *pszLayerName)
{
    /* Only quote if needed. Quoting conventions depend on the driver... */
    if (!EQUAL(pszLayerName, "SELECT") && !EQUAL(pszLayerName, "AS") &&
        !EQUAL(pszLayerName, "CAST") && !EQUAL(pszLayerName, "FROM") &&
        !EQUAL(pszLayerName, "JOIN") && !EQUAL(pszLayerName, "WHERE") &&
        !EQUAL(pszLayerName, "ON") && !EQUAL(pszLayerName, "USING") &&
        !EQUAL(pszLayerName, "ORDER") && !EQUAL(pszLayerName, "BY") &&
        !EQUAL(pszLayerName, "ASC") && !EQUAL(pszLayerName, "DESC") &&
        !EQUAL(pszLayerName, "GROUP") && !EQUAL(pszLayerName, "LIMIT") &&
        !EQUAL(pszLayerName, "OFFSET"))
    {
        char ch;
        for (int i = 0; (ch = pszLayerName[i]) != 0; i++)
        {
            if (ch >= '0' && ch <= '9')
            {
                if (i == 0)
                    break;
            }
            else if (!((ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z')))
                break;
        }
        if (ch == 0)
            return pszLayerName;
    }

    if (EQUAL(poDS->GetDriverName(), "MYSQL"))
        return CPLSPrintf("`%s`", pszLayerName);

    if (EQUAL(poDS->GetDriverName(), "PostgreSQL") && strchr(pszLayerName, '.'))
    {
        const char *pszRet = nullptr;
        char **papszTokens = CSLTokenizeStringComplex(pszLayerName, ".", 0, 0);
        if (CSLCount(papszTokens) == 2)
            pszRet =
                CPLSPrintf("\"%s\".\"%s\"", papszTokens[0], papszTokens[1]);
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

static int TestOGRLayerFeatureCount(GDALDataset *poDS, OGRLayer *poLayer,
                                    int bIsSQLLayer)

{
    int bRet = TRUE;
    GIntBig nFC = 0;
    GIntBig nClaimedFC = LOG_ACTION(poLayer->GetFeatureCount());
    bool bWarnAboutSRS = false;
    OGRFeatureDefn *poLayerDefn = LOG_ACTION(poLayer->GetLayerDefn());
    int nGeomFieldCount = LOG_ACTION(poLayerDefn->GetGeomFieldCount());

    const bool bLayerHasMeasuredGeometriesCapability =
        poLayer->TestCapability(ODsCMeasuredGeometries);
    const bool bLayerHasCurveGeometriesCapability =
        poLayer->TestCapability(OLCCurveGeometries);
    const bool bLayerHasZGeometriesCapability =
        poLayer->TestCapability(OLCZGeometries);

    CPLErrorReset();

    for (auto &&poFeature : poLayer)
    {
        nFC++;

        if (poFeature->GetDefnRef() != poLayerDefn)
        {
            bRet = FALSE;
            printf("ERROR: Feature defn differs from layer defn.\n"
                   "Feature defn = %p\n"
                   "Layer defn = %p\n",
                   poFeature->GetDefnRef(), poLayerDefn);
        }

        for (int iGeom = 0; iGeom < nGeomFieldCount; iGeom++)
        {
            OGRGeometry *poGeom = poFeature->GetGeomFieldRef(iGeom);

            if (poGeom && poGeom->IsMeasured() &&
                !bLayerHasMeasuredGeometriesCapability)
            {
                bRet = FALSE;
                printf("FAILURE: Layer has a feature with measured geometries "
                       "but no ODsCMeasuredGeometries capability!\n");
            }
            if (poGeom && poGeom->hasCurveGeometry() &&
                !bLayerHasCurveGeometriesCapability)
            {
                bRet = FALSE;
                printf("FAILURE: Layer has a feature with curved geometries "
                       "but no OLCCurveGeometries capability!\n");
            }
            if (poGeom && poGeom->Is3D() && !bLayerHasZGeometriesCapability)
            {
                bRet = FALSE;
                printf("FAILURE: Layer has a feature with 3D geometries but no "
                       "OLCZGeometries capability!\n");
            }

            const OGRSpatialReference *poGFldSRS =
                poLayerDefn->GetGeomFieldDefn(iGeom)->GetSpatialRef();

            // Compatibility with old drivers anterior to RFC 41
            if (iGeom == 0 && nGeomFieldCount == 1 && poGFldSRS == nullptr)
                poGFldSRS = poLayer->GetSpatialRef();

            if (poGeom != nullptr &&
                poGeom->getSpatialReference() != poGFldSRS && !bWarnAboutSRS)
            {
                bWarnAboutSRS = true;

                char *pszLayerSRSWKT = nullptr;
                if (poGFldSRS != nullptr)
                    poGFldSRS->exportToWkt(&pszLayerSRSWKT);
                else
                    pszLayerSRSWKT = CPLStrdup("(NULL)");

                char *pszFeatureSRSWKT = nullptr;
                if (poGeom->getSpatialReference() != nullptr)
                    poGeom->getSpatialReference()->exportToWkt(
                        &pszFeatureSRSWKT);
                else
                    pszFeatureSRSWKT = CPLStrdup("(NULL)");

                bRet = FALSE;
                printf("ERROR: Feature SRS differs from layer SRS.\n"
                       "Feature SRS = %s (%p)\n"
                       "Layer SRS = %s (%p)\n",
                       pszFeatureSRSWKT, poGeom->getSpatialReference(),
                       pszLayerSRSWKT, poGFldSRS);
                CPLFree(pszLayerSRSWKT);
                CPLFree(pszFeatureSRSWKT);
            }
        }
    }

    /* mapogr.cpp doesn't like errors after GetNextFeature() */
    if (CPLGetLastErrorType() == CE_Failure)
    {
        bRet = FALSE;
        printf("ERROR: An error was reported : %s\n", CPLGetLastErrorMsg());
    }

    // Drivers might or might not emit errors when attempting to iterate
    // after EOF
    CPLPushErrorHandler(CPLQuietErrorHandler);
    auto poFeat = LOG_ACTION(poLayer->GetNextFeature());
    CPLPopErrorHandler();
    if (poFeat != nullptr)
    {
        bRet = FALSE;
        printf("ERROR: GetNextFeature() returned non NULL feature after end of "
               "iteration.\n");
    }
    delete poFeat;

    const auto nFCEndOfIter = LOG_ACTION(poLayer->GetFeatureCount());
    if (nFC != nClaimedFC)
    {
        bRet = FALSE;
        printf("ERROR: Claimed feature count " CPL_FRMT_GIB
               " doesn't match actual, " CPL_FRMT_GIB ".\n",
               nClaimedFC, nFC);
    }
    else if (nFC != nFCEndOfIter)
    {
        bRet = FALSE;
        printf("ERROR: Feature count at end of layer, " CPL_FRMT_GIB
               ", differs from at start, " CPL_FRMT_GIB ".\n",
               nFCEndOfIter, nFC);
    }
    else if (bVerbose)
        printf("INFO: Feature count verified.\n");

    if (!bIsSQLLayer)
    {
        CPLString osSQL;

        osSQL.Printf("SELECT COUNT(*) FROM %s",
                     GetLayerNameForSQL(poDS, poLayer->GetName()));

        OGRLayer *poSQLLyr = poDS->ExecuteSQL(osSQL.c_str(), nullptr, nullptr);
        if (poSQLLyr)
        {
            OGRFeature *poFeatCount = poSQLLyr->GetNextFeature();
            if (poFeatCount == nullptr)
            {
                bRet = FALSE;
                printf("ERROR: '%s' failed.\n", osSQL.c_str());
            }
            else if (nClaimedFC != poFeatCount->GetFieldAsInteger(0))
            {
                bRet = FALSE;
                printf("ERROR: Claimed feature count " CPL_FRMT_GIB
                       " doesn't match '%s' one, " CPL_FRMT_GIB ".\n",
                       nClaimedFC, osSQL.c_str(),
                       poFeatCount->GetFieldAsInteger64(0));
            }
            DestroyFeatureAndNullify(poFeatCount);
            poDS->ReleaseResultSet(poSQLLyr);
        }
    }

    if (bVerbose && !bWarnAboutSRS)
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

static int TestOGRLayerRandomRead(OGRLayer *poLayer)

{
    int bRet = TRUE;
    OGRFeature *papoFeatures[5], *poFeature = nullptr;

    LOG_ACTION(poLayer->SetSpatialFilter(nullptr));

    if (LOG_ACTION(poLayer->GetFeatureCount()) < 5)
    {
        if (bVerbose)
            printf("INFO: Only " CPL_FRMT_GIB " features on layer,"
                   "skipping random read test.\n",
                   poLayer->GetFeatureCount());

        return bRet;
    }

    /* -------------------------------------------------------------------- */
    /*      Fetch five features.                                            */
    /* -------------------------------------------------------------------- */
    LOG_ACTION(poLayer->ResetReading());

    for (int iFeature = 0; iFeature < 5; iFeature++)
    {
        papoFeatures[iFeature] = nullptr;
    }
    for (int iFeature = 0; iFeature < 5; iFeature++)
    {
        papoFeatures[iFeature] = LOG_ACTION(poLayer->GetNextFeature());
        if (papoFeatures[iFeature] == nullptr)
        {
            if (bVerbose)
                printf("INFO: Only %d features on layer,"
                       "skipping random read test.\n",
                       iFeature);
            goto end;
        }
    }

    /* -------------------------------------------------------------------- */
    /*      Test feature 2.                                                 */
    /* -------------------------------------------------------------------- */
    poFeature = LOG_ACTION(poLayer->GetFeature(papoFeatures[1]->GetFID()));
    if (poFeature == nullptr)
    {
        printf("ERROR: Cannot fetch feature " CPL_FRMT_GIB ".\n",
               papoFeatures[1]->GetFID());
        goto end;
    }

    if (!poFeature->Equal(papoFeatures[1]))
    {
        bRet = FALSE;
        printf("ERROR: Attempt to randomly read feature " CPL_FRMT_GIB
               " appears to\n"
               "       have returned a different feature than sequential\n"
               "       reading indicates should have happened.\n",
               papoFeatures[1]->GetFID());
        poFeature->DumpReadable(stdout);
        papoFeatures[1]->DumpReadable(stdout);

        goto end;
    }

    DestroyFeatureAndNullify(poFeature);

    /* -------------------------------------------------------------------- */
    /*      Test feature 5.                                                 */
    /* -------------------------------------------------------------------- */
    poFeature = LOG_ACTION(poLayer->GetFeature(papoFeatures[4]->GetFID()));
    if (poFeature == nullptr)
    {
        printf("ERROR: Cannot fetch feature " CPL_FRMT_GIB ".\n",
               papoFeatures[4]->GetFID());
        goto end;
    }

    if (!poFeature->Equal(papoFeatures[4]))
    {
        bRet = FALSE;
        printf("ERROR: Attempt to randomly read feature " CPL_FRMT_GIB
               " appears to\n"
               "       have returned a different feature than sequential\n"
               "       reading indicates should have happened.\n",
               papoFeatures[4]->GetFID());
        poFeature->DumpReadable(stdout);
        papoFeatures[4]->DumpReadable(stdout);

        goto end;
    }

    DestroyFeatureAndNullify(poFeature);

    /* -------------------------------------------------------------------- */
    /*      Test feature 2 again                                            */
    /* -------------------------------------------------------------------- */
    poFeature = LOG_ACTION(poLayer->GetFeature(papoFeatures[2]->GetFID()));
    if (poFeature == nullptr)
    {
        printf("ERROR: Cannot fetch feature " CPL_FRMT_GIB ".\n",
               papoFeatures[2]->GetFID());
        goto end;
    }

    if (!poFeature->Equal(papoFeatures[2]))
    {
        bRet = FALSE;
        printf("ERROR: Attempt to randomly read feature " CPL_FRMT_GIB
               " appears to\n"
               "       have returned a different feature than sequential\n"
               "       reading indicates should have happened.\n",
               papoFeatures[2]->GetFID());
        poFeature->DumpReadable(stdout);
        papoFeatures[2]->DumpReadable(stdout);

        goto end;
    }

    if (bVerbose)
        printf("INFO: Random read test passed.\n");

end:
    DestroyFeatureAndNullify(poFeature);

    /* -------------------------------------------------------------------- */
    /*      Cleanup.                                                        */
    /* -------------------------------------------------------------------- */
    for (int iFeature = 0; iFeature < 5; iFeature++)
        DestroyFeatureAndNullify(papoFeatures[iFeature]);

    return bRet;
}

/************************************************************************/
/*                       TestOGRLayerSetNextByIndex()                   */
/*                                                                      */
/************************************************************************/

static int TestOGRLayerSetNextByIndex(OGRLayer *poLayer)

{
    int bRet = TRUE;
    OGRFeature *poFeature = nullptr;
    OGRFeature *papoFeatures[5];

    memset(papoFeatures, 0, sizeof(papoFeatures));

    LOG_ACTION(poLayer->SetSpatialFilter(nullptr));

    if (LOG_ACTION(poLayer->GetFeatureCount()) < 5)
    {
        if (bVerbose)
            printf("INFO: Only " CPL_FRMT_GIB " features on layer,"
                   "skipping SetNextByIndex test.\n",
                   poLayer->GetFeatureCount());

        return bRet;
    }

    /* -------------------------------------------------------------------- */
    /*      Fetch five features.                                            */
    /* -------------------------------------------------------------------- */
    LOG_ACTION(poLayer->ResetReading());

    for (int iFeature = 0; iFeature < 5; iFeature++)
    {
        papoFeatures[iFeature] = LOG_ACTION(poLayer->GetNextFeature());
        if (papoFeatures[iFeature] == nullptr)
        {
            bRet = FALSE;
            printf("ERROR: Cannot get feature %d.\n", iFeature);
            goto end;
        }
    }

    /* -------------------------------------------------------------------- */
    /*      Test feature at index 1.                                        */
    /* -------------------------------------------------------------------- */
    if (LOG_ACTION(poLayer->SetNextByIndex(1)) != OGRERR_NONE)
    {
        bRet = FALSE;
        printf("ERROR: SetNextByIndex(%d) failed.\n", 1);
        goto end;
    }

    poFeature = LOG_ACTION(poLayer->GetNextFeature());
    if (poFeature == nullptr || !poFeature->Equal(papoFeatures[1]))
    {
        bRet = FALSE;
        printf("ERROR: Attempt to read feature at index %d appears to\n"
               "       have returned a different feature than sequential\n"
               "       reading indicates should have happened.\n",
               1);

        goto end;
    }

    DestroyFeatureAndNullify(poFeature);

    poFeature = LOG_ACTION(poLayer->GetNextFeature());
    if (poFeature == nullptr || !poFeature->Equal(papoFeatures[2]))
    {
        bRet = FALSE;
        printf("ERROR: Attempt to read feature after feature at index %d "
               "appears to\n"
               "       have returned a different feature than sequential\n"
               "       reading indicates should have happened.\n",
               1);

        goto end;
    }

    DestroyFeatureAndNullify(poFeature);

    /* -------------------------------------------------------------------- */
    /*      Test feature at index 3.                                        */
    /* -------------------------------------------------------------------- */
    if (LOG_ACTION(poLayer->SetNextByIndex(3)) != OGRERR_NONE)
    {
        bRet = FALSE;
        printf("ERROR: SetNextByIndex(%d) failed.\n", 3);
        goto end;
    }

    poFeature = LOG_ACTION(poLayer->GetNextFeature());
    if (!poFeature->Equal(papoFeatures[3]))
    {
        bRet = FALSE;
        printf("ERROR: Attempt to read feature at index %d appears to\n"
               "       have returned a different feature than sequential\n"
               "       reading indicates should have happened.\n",
               3);

        goto end;
    }

    DestroyFeatureAndNullify(poFeature);

    poFeature = LOG_ACTION(poLayer->GetNextFeature());
    if (!poFeature->Equal(papoFeatures[4]))
    {
        bRet = FALSE;
        printf("ERROR: Attempt to read feature after feature at index %d "
               "appears to\n"
               "       have returned a different feature than sequential\n"
               "       reading indicates should have happened.\n",
               3);

        goto end;
    }

    if (bVerbose)
        printf("INFO: SetNextByIndex() read test passed.\n");

end:
    DestroyFeatureAndNullify(poFeature);

    /* -------------------------------------------------------------------- */
    /*      Cleanup.                                                        */
    /* -------------------------------------------------------------------- */
    for (int iFeature = 0; iFeature < 5; iFeature++)
        DestroyFeatureAndNullify(papoFeatures[iFeature]);

    return bRet;
}

/************************************************************************/
/*                      TestOGRLayerRandomWrite()                       */
/*                                                                      */
/*      Test random writing by trying to switch the 2nd and 5th         */
/*      features.                                                       */
/************************************************************************/

static int TestOGRLayerRandomWrite(OGRLayer *poLayer)

{
    int bRet = TRUE;
    OGRFeature *papoFeatures[5];

    memset(papoFeatures, 0, sizeof(papoFeatures));

    LOG_ACTION(poLayer->SetSpatialFilter(nullptr));

    if (LOG_ACTION(poLayer->GetFeatureCount()) < 5)
    {
        if (bVerbose)
            printf("INFO: Only " CPL_FRMT_GIB " features on layer,"
                   "skipping random write test.\n",
                   poLayer->GetFeatureCount());

        return bRet;
    }

    if (!LOG_ACTION(poLayer->TestCapability(OLCRandomRead)))
    {
        if (bVerbose)
            printf("INFO: Skipping random write test since this layer "
                   "doesn't support random read.\n");
        return bRet;
    }

    GIntBig nFID2;
    GIntBig nFID5;

    CPLString os_Id2;
    CPLString os_Id5;

    const bool bHas_Id = poLayer->GetLayerDefn()->GetFieldIndex("_id") == 0 ||
                         poLayer->GetLayerDefn()->GetFieldIndex("id") == 0;

    /* -------------------------------------------------------------------- */
    /*      Fetch five features.                                            */
    /* -------------------------------------------------------------------- */
    LOG_ACTION(poLayer->ResetReading());

    for (int iFeature = 0; iFeature < 5; iFeature++)
    {
        papoFeatures[iFeature] = LOG_ACTION(poLayer->GetNextFeature());
        if (papoFeatures[iFeature] == nullptr)
        {
            bRet = FALSE;
            printf("ERROR: Cannot get feature %d.\n", iFeature);
            goto end;
        }
    }

    /* -------------------------------------------------------------------- */
    /*      Switch feature ids of feature 2 and 5.                          */
    /* -------------------------------------------------------------------- */
    nFID2 = papoFeatures[1]->GetFID();
    nFID5 = papoFeatures[4]->GetFID();

    papoFeatures[1]->SetFID(nFID5);
    papoFeatures[4]->SetFID(nFID2);

    if (bHas_Id)
    {
        os_Id2 = papoFeatures[1]->GetFieldAsString(0);
        os_Id5 = papoFeatures[4]->GetFieldAsString(0);

        papoFeatures[1]->SetField(0, os_Id5);
        papoFeatures[4]->SetField(0, os_Id2);
    }

    /* -------------------------------------------------------------------- */
    /*      Rewrite them.                                                   */
    /* -------------------------------------------------------------------- */
    if (LOG_ACTION(poLayer->SetFeature(papoFeatures[1])) != OGRERR_NONE)
    {
        bRet = FALSE;
        printf("ERROR: Attempt to SetFeature(1) failed.\n");
        goto end;
    }
    if (LOG_ACTION(poLayer->SetFeature(papoFeatures[4])) != OGRERR_NONE)
    {
        bRet = FALSE;
        printf("ERROR: Attempt to SetFeature(4) failed.\n");
        goto end;
    }

    /* -------------------------------------------------------------------- */
    /*      Now re-read feature 2 to verify the effect stuck.               */
    /* -------------------------------------------------------------------- */
    {
        auto poFeature =
            std::unique_ptr<OGRFeature>(LOG_ACTION(poLayer->GetFeature(nFID5)));
        if (poFeature == nullptr)
        {
            bRet = FALSE;
            printf("ERROR: Attempt to GetFeature( nFID5 ) failed.\n");
            goto end;
        }
        if (!poFeature->Equal(papoFeatures[1]))
        {
            bRet = FALSE;
            poFeature->DumpReadable(stderr);
            papoFeatures[1]->DumpReadable(stderr);
            printf("ERROR: Written feature didn't seem to retain value.\n");
        }
        else if (bVerbose)
        {
            printf("INFO: Random write test passed.\n");
        }
    }

    /* -------------------------------------------------------------------- */
    /*      Re-invert the features to restore to original state             */
    /* -------------------------------------------------------------------- */

    papoFeatures[1]->SetFID(nFID2);
    papoFeatures[4]->SetFID(nFID5);

    if (bHas_Id)
    {
        papoFeatures[1]->SetField(0, os_Id2);
        papoFeatures[4]->SetField(0, os_Id5);
    }

    if (LOG_ACTION(poLayer->SetFeature(papoFeatures[1])) != OGRERR_NONE)
    {
        bRet = FALSE;
        printf("ERROR: Attempt to restore SetFeature(1) failed.\n");
    }
    if (LOG_ACTION(poLayer->SetFeature(papoFeatures[4])) != OGRERR_NONE)
    {
        bRet = FALSE;
        printf("ERROR: Attempt to restore SetFeature(4) failed.\n");
    }

    /* -------------------------------------------------------------------- */
    /*      Test UpdateFeature()                                            */
    /* -------------------------------------------------------------------- */

    if (bRet)
    {
        int nOldVal = 0;
        std::string osOldVal;
        int iUpdatedFeature = -1;
        int iUpdatedField = -1;
        std::unique_ptr<OGRFeature> poUpdatedFeature;

        for (int iFeature = 0; iUpdatedFeature < 0 && iFeature < 5; iFeature++)
        {
            for (int iField = 0;
                 iField < poLayer->GetLayerDefn()->GetFieldCount(); ++iField)
            {
                if (papoFeatures[iFeature]->IsFieldSetAndNotNull(iField))
                {
                    if (poLayer->GetLayerDefn()
                            ->GetFieldDefn(iField)
                            ->GetType() == OFTInteger)
                    {
                        iUpdatedFeature = iFeature;
                        iUpdatedField = iField;
                        nOldVal =
                            papoFeatures[iFeature]->GetFieldAsInteger(iField);
                        poUpdatedFeature.reset(papoFeatures[iFeature]->Clone());
                        poUpdatedFeature->SetField(iField, 0xBEEF);
                        break;
                    }
                    else if (poLayer->GetLayerDefn()
                                 ->GetFieldDefn(iField)
                                 ->GetType() == OFTString)
                    {
                        iUpdatedFeature = iFeature;
                        iUpdatedField = iField;
                        osOldVal =
                            papoFeatures[iFeature]->GetFieldAsString(iField);
                        poUpdatedFeature.reset(papoFeatures[iFeature]->Clone());
                        poUpdatedFeature->SetField(iField, "0xBEEF");
                        break;
                    }
                }
            }
        }

        if (poUpdatedFeature)
        {
            if (LOG_ACTION(poLayer->UpdateFeature(poUpdatedFeature.get(), 1,
                                                  &iUpdatedField, 0, nullptr,
                                                  false)) != OGRERR_NONE)
            {
                bRet = FALSE;
                printf("ERROR: UpdateFeature() failed.\n");
            }
            if (bRet)
            {
                LOG_ACTION(poLayer->ResetReading());
                for (int iFeature = 0; iFeature < 5; iFeature++)
                {
                    auto poFeature = std::unique_ptr<OGRFeature>(LOG_ACTION(
                        poLayer->GetFeature(papoFeatures[iFeature]->GetFID())));
                    if (iFeature != iUpdatedFeature)
                    {
                        if (!poFeature ||
                            !poFeature->Equal(papoFeatures[iFeature]))
                        {
                            bRet = false;
                            printf("ERROR: UpdateFeature() test: "
                                   "!poFeature->Equals(papoFeatures[iFeature]) "
                                   "unexpected.\n");
                            if (poFeature)
                                poFeature->DumpReadable(stdout);
                            papoFeatures[iFeature]->DumpReadable(stdout);
                        }
                    }
                }

                auto poFeature =
                    std::unique_ptr<OGRFeature>(LOG_ACTION(poLayer->GetFeature(
                        papoFeatures[iUpdatedFeature]->GetFID())));
                if (!poFeature)
                {
                    bRet = FALSE;
                    printf("ERROR: at line %d", __LINE__);
                    goto end;
                }
                if (poLayer->GetLayerDefn()
                        ->GetFieldDefn(iUpdatedField)
                        ->GetType() == OFTInteger)
                {
                    if (poFeature->GetFieldAsInteger(iUpdatedField) != 0xBEEF)
                    {
                        bRet = FALSE;
                        printf("ERROR: Did not get expected field value after "
                               "UpdateFeature().\n");
                    }
                    poFeature->SetField(iUpdatedField, nOldVal);
                }
                else if (poLayer->GetLayerDefn()
                             ->GetFieldDefn(iUpdatedField)
                             ->GetType() == OFTString)
                {
                    if (!EQUAL(poFeature->GetFieldAsString(iUpdatedField),
                               "0xBEEF"))
                    {
                        bRet = FALSE;
                        printf("ERROR: Did not get expected field value after "
                               "UpdateFeature().\n");
                    }
                    poFeature->SetField(iUpdatedField, osOldVal.c_str());
                }
                else
                {
                    CPLAssert(false);
                }

                if (LOG_ACTION(poLayer->UpdateFeature(
                        poFeature.get(), 1, &iUpdatedField, 0, nullptr,
                        false)) != OGRERR_NONE)
                {
                    bRet = FALSE;
                    printf("ERROR: UpdateFeature() failed.\n");
                }

                poFeature.reset(LOG_ACTION(poLayer->GetFeature(
                    papoFeatures[iUpdatedFeature]->GetFID())));
                if (!poFeature)
                {
                    bRet = FALSE;
                    printf("ERROR: at line %d", __LINE__);
                    goto end;
                }
                if (!poFeature->Equal(papoFeatures[iUpdatedFeature]))
                {
                    bRet = false;
                    printf("ERROR: UpdateFeature() test: "
                           "!poFeature->Equals(papoFeatures[iUpdatedFeature]) "
                           "unexpected.\n");
                }
            }
        }
        else
        {
            if (bVerbose)
                printf("INFO: Could not test UpdateFeature().\n");
        }
    }

end:
    /* -------------------------------------------------------------------- */
    /*      Cleanup.                                                        */
    /* -------------------------------------------------------------------- */

    for (int iFeature = 0; iFeature < 5; iFeature++)
        DestroyFeatureAndNullify(papoFeatures[iFeature]);

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

static int TestSpatialFilter(OGRLayer *poLayer, int iGeomField)

{
    int bRet = TRUE;

    /* -------------------------------------------------------------------- */
    /*      Read the target feature.                                        */
    /* -------------------------------------------------------------------- */
    LOG_ACTION(poLayer->ResetReading());
    OGRFeature *poTargetFeature = LOG_ACTION(poLayer->GetNextFeature());

    if (poTargetFeature == nullptr)
    {
        if (bVerbose)
        {
            printf("INFO: Skipping Spatial Filter test for %s.\n"
                   "      No features in layer.\n",
                   poLayer->GetName());
        }
        return bRet;
    }

    OGRGeometry *poGeom = poTargetFeature->GetGeomFieldRef(iGeomField);
    if (poGeom == nullptr || poGeom->IsEmpty())
    {
        if (bVerbose)
        {
            printf("INFO: Skipping Spatial Filter test for %s,\n"
                   "      target feature has no geometry.\n",
                   poTargetFeature->GetDefnRef()->GetName());
        }
        DestroyFeatureAndNullify(poTargetFeature);
        return bRet;
    }

    OGREnvelope sEnvelope;
    poGeom->getEnvelope(&sEnvelope);

    OGREnvelope sLayerExtent;
    double epsilon = 10.0;
    if (LOG_ACTION(poLayer->TestCapability(OLCFastGetExtent)) &&
        LOG_ACTION(poLayer->GetExtent(iGeomField, &sLayerExtent)) ==
            OGRERR_NONE &&
        sLayerExtent.MinX < sLayerExtent.MaxX &&
        sLayerExtent.MinY < sLayerExtent.MaxY)
    {
        epsilon = std::min(sLayerExtent.MaxX - sLayerExtent.MinX,
                           sLayerExtent.MaxY - sLayerExtent.MinY) /
                  10.0;
    }

    /* -------------------------------------------------------------------- */
    /*      Construct inclusive filter.                                     */
    /* -------------------------------------------------------------------- */

    OGRLinearRing oRing;
    oRing.setPoint(0, sEnvelope.MinX - 2 * epsilon,
                   sEnvelope.MinY - 2 * epsilon);
    oRing.setPoint(1, sEnvelope.MinX - 2 * epsilon,
                   sEnvelope.MaxY + 1 * epsilon);
    oRing.setPoint(2, sEnvelope.MaxX + 1 * epsilon,
                   sEnvelope.MaxY + 1 * epsilon);
    oRing.setPoint(3, sEnvelope.MaxX + 1 * epsilon,
                   sEnvelope.MinY - 2 * epsilon);
    oRing.setPoint(4, sEnvelope.MinX - 2 * epsilon,
                   sEnvelope.MinY - 2 * epsilon);

    OGRPolygon oInclusiveFilter;
    oInclusiveFilter.addRing(&oRing);

    LOG_ACTION(poLayer->SetSpatialFilter(iGeomField, &oInclusiveFilter));

    /* -------------------------------------------------------------------- */
    /*      Verify that we can find the target feature.                     */
    /* -------------------------------------------------------------------- */
    bool bFound = false;
    GIntBig nIterCount = 0;
    for (auto &&poFeature : poLayer)
    {
        if (poFeature->Equal(poTargetFeature))
        {
            bFound = true;
        }
        nIterCount++;
    }

    if (!bFound)
    {
        bRet = FALSE;
        printf(
            "ERROR: Spatial filter (%d) eliminated a feature unexpectedly!\n",
            iGeomField);
    }
    else if (bVerbose)
    {
        printf("INFO: Spatial filter inclusion seems to work.\n");
    }

    GIntBig nInclusiveCount = LOG_ACTION(poLayer->GetFeatureCount());

    // Identity check doesn't always work depending on feature geometries
    if (nIterCount > nInclusiveCount)
    {
        bRet = FALSE;
        printf("ERROR: GetFeatureCount() with spatial filter smaller (%d) than "
               "count while iterating over features (%d).\n",
               static_cast<int>(nInclusiveCount), static_cast<int>(nIterCount));
    }

    LOG_ACTION(poLayer->SetAttributeFilter("1=1"));
    GIntBig nShouldBeSame = LOG_ACTION(poLayer->GetFeatureCount());
    LOG_ACTION(poLayer->SetAttributeFilter(nullptr));
    if (nShouldBeSame != nInclusiveCount)
    {
        bRet = FALSE;
        printf("ERROR: Attribute filter seems to be make spatial "
               "filter fail with GetFeatureCount().\n");
    }

    LOG_ACTION(poLayer->SetAttributeFilter("1=0"));
    GIntBig nShouldBeZero = LOG_ACTION(poLayer->GetFeatureCount());
    LOG_ACTION(poLayer->SetAttributeFilter(nullptr));
    if (nShouldBeZero != 0)
    {
        bRet = FALSE;
        printf("ERROR: Attribute filter seems to be ignored in "
               "GetFeatureCount() when spatial filter is set.\n");
    }

    /* -------------------------------------------------------------------- */
    /*      Construct exclusive filter.                                     */
    /* -------------------------------------------------------------------- */
    oRing.setPoint(0, sEnvelope.MinX - 2 * epsilon,
                   sEnvelope.MinY - 2 * epsilon);
    oRing.setPoint(1, sEnvelope.MinX - 1 * epsilon,
                   sEnvelope.MinY - 2 * epsilon);
    oRing.setPoint(2, sEnvelope.MinX - 1 * epsilon,
                   sEnvelope.MinY - 1 * epsilon);
    oRing.setPoint(3, sEnvelope.MinX - 2 * epsilon,
                   sEnvelope.MinY - 1 * epsilon);
    oRing.setPoint(4, sEnvelope.MinX - 2 * epsilon,
                   sEnvelope.MinY - 2 * epsilon);

    OGRPolygon oExclusiveFilter;
    oExclusiveFilter.addRing(&oRing);

    LOG_ACTION(poLayer->SetSpatialFilter(iGeomField, &oExclusiveFilter));

    /* -------------------------------------------------------------------- */
    /*      Verify that we can NOT find the target feature.                 */
    /* -------------------------------------------------------------------- */
    OGRFeatureUniquePtr poUniquePtrFeature;
    for (auto &&poFeatureIter : poLayer)
    {
        if (poFeatureIter->Equal(poTargetFeature))
        {
            poUniquePtrFeature.swap(poFeatureIter);
            break;
        }
    }

    if (poUniquePtrFeature != nullptr)
    {
        bRet = FALSE;
        printf("ERROR: Spatial filter (%d) failed to eliminate "
               "a feature unexpectedly!\n",
               iGeomField);
    }
    else if (LOG_ACTION(poLayer->GetFeatureCount()) >= nInclusiveCount)
    {
        bRet = FALSE;
        printf("ERROR: GetFeatureCount() may not be taking spatial "
               "filter (%d) into account.\n",
               iGeomField);
    }
    else if (bVerbose)
    {
        printf("INFO: Spatial filter exclusion seems to work.\n");
    }

    // Check that GetFeature() ignores the spatial filter
    poUniquePtrFeature.reset(
        LOG_ACTION(poLayer->GetFeature(poTargetFeature->GetFID())));
    if (poUniquePtrFeature == nullptr ||
        !poUniquePtrFeature->Equal(poTargetFeature))
    {
        bRet = FALSE;
        printf("ERROR: Spatial filter has been taken into account "
               "by GetFeature()\n");
    }
    else if (bVerbose)
    {
        printf("INFO: Spatial filter is ignored by GetFeature() "
               "as expected.\n");
    }

    if (bRet)
    {
        poUniquePtrFeature.reset();
        for (auto &&poFeatureIter : poLayer)
        {
            if (poFeatureIter->Equal(poTargetFeature))
            {
                poUniquePtrFeature.swap(poFeatureIter);
                break;
            }
        }
        if (poUniquePtrFeature != nullptr)
        {
            bRet = FALSE;
            printf("ERROR: Spatial filter has not been restored correctly "
                   "after GetFeature()\n");
        }
    }

    DestroyFeatureAndNullify(poTargetFeature);

    /* -------------------------------------------------------------------- */
    /*     Test infinity envelope                                           */
    /* -------------------------------------------------------------------- */

    constexpr double NEG_INF = -std::numeric_limits<double>::infinity();
    constexpr double POS_INF = std::numeric_limits<double>::infinity();

    oRing.setPoint(0, NEG_INF, NEG_INF);
    oRing.setPoint(1, NEG_INF, POS_INF);
    oRing.setPoint(2, POS_INF, POS_INF);
    oRing.setPoint(3, POS_INF, NEG_INF);
    oRing.setPoint(4, NEG_INF, NEG_INF);

    OGRPolygon oInfinityFilter;
    oInfinityFilter.addRing(&oRing);

    LOG_ACTION(poLayer->SetSpatialFilter(iGeomField, &oInfinityFilter));
    int nCountInf = 0;
    for (auto &&poFeatureIter : poLayer)
    {
        auto poGeomIter = poFeatureIter->GetGeomFieldRef(iGeomField);
        if (poGeomIter != nullptr)
            nCountInf++;
    }

    /* -------------------------------------------------------------------- */
    /*     Test envelope with huge coords                                   */
    /* -------------------------------------------------------------------- */

    constexpr double HUGE_COORDS = 1.0e300;

    oRing.setPoint(0, -HUGE_COORDS, -HUGE_COORDS);
    oRing.setPoint(1, -HUGE_COORDS, HUGE_COORDS);
    oRing.setPoint(2, HUGE_COORDS, HUGE_COORDS);
    oRing.setPoint(3, HUGE_COORDS, -HUGE_COORDS);
    oRing.setPoint(4, -HUGE_COORDS, -HUGE_COORDS);

    OGRPolygon oHugeFilter;
    oHugeFilter.addRing(&oRing);

    LOG_ACTION(poLayer->SetSpatialFilter(iGeomField, &oHugeFilter));
    int nCountHuge = 0;
    for (auto &&poFeatureIter : poLayer)
    {
        auto poGeomIter = poFeatureIter->GetGeomFieldRef(iGeomField);
        if (poGeomIter != nullptr)
            nCountHuge++;
    }

    /* -------------------------------------------------------------------- */
    /*     Reset spatial filter                                             */
    /* -------------------------------------------------------------------- */
    LOG_ACTION(poLayer->SetSpatialFilter(nullptr));

    int nExpected = 0;
    for (auto &&poFeatureIter : poLayer)
    {
        auto poGeomIter = poFeatureIter->GetGeomFieldRef(iGeomField);
        if (poGeomIter != nullptr && !poGeomIter->IsEmpty())
            nExpected++;
    }
    LOG_ACTION(poLayer->ResetReading());

    if (nCountInf != nExpected)
    {
        /*bRet = FALSE; */
        printf("WARNING: Infinity spatial filter returned %d features "
               "instead of %d\n",
               nCountInf, nExpected);
    }
    else if (bVerbose)
    {
        printf("INFO: Infinity spatial filter works as expected.\n");
    }

    if (nCountHuge != nExpected)
    {
        /* bRet = FALSE; */
        printf("WARNING: Huge coords spatial filter returned %d features "
               "instead of %d\n",
               nCountHuge, nExpected);
    }
    else if (bVerbose)
    {
        printf("INFO: Huge coords spatial filter works as expected.\n");
    }

    return bRet;
}

static int TestFullSpatialFilter(OGRLayer *poLayer, int iGeomField)

{
    int bRet = TRUE;

    OGREnvelope sLayerExtent;
    double epsilon = 10.0;
    if (LOG_ACTION(poLayer->TestCapability(OLCFastGetExtent)) &&
        LOG_ACTION(poLayer->GetExtent(iGeomField, &sLayerExtent)) ==
            OGRERR_NONE &&
        sLayerExtent.MinX < sLayerExtent.MaxX &&
        sLayerExtent.MinY < sLayerExtent.MaxY)
    {
        epsilon = std::min(sLayerExtent.MaxX - sLayerExtent.MinX,
                           sLayerExtent.MaxY - sLayerExtent.MinY) /
                  10.0;
    }

    const GIntBig nTotalFeatureCount = LOG_ACTION(poLayer->GetFeatureCount());
    for (GIntBig i = 0; i < nTotalFeatureCount; i++)
    {
        /* --------------------------------------------------------------------
         */
        /*      Read the target feature. */
        /* --------------------------------------------------------------------
         */
        LOG_ACTION(poLayer->SetSpatialFilter(nullptr));
        LOG_ACTION(poLayer->ResetReading());
        LOG_ACTION(poLayer->SetNextByIndex(i));
        OGRFeature *poTargetFeature = LOG_ACTION(poLayer->GetNextFeature());

        if (poTargetFeature == nullptr)
        {
            continue;
        }

        OGRGeometry *poGeom = poTargetFeature->GetGeomFieldRef(iGeomField);
        if (poGeom == nullptr || poGeom->IsEmpty())
        {
            DestroyFeatureAndNullify(poTargetFeature);
            continue;
        }

        OGREnvelope sEnvelope;
        poGeom->getEnvelope(&sEnvelope);

        /* --------------------------------------------------------------------
         */
        /*      Construct inclusive filter. */
        /* --------------------------------------------------------------------
         */

        OGRLinearRing oRing;
        oRing.setPoint(0, sEnvelope.MinX - 2 * epsilon,
                       sEnvelope.MinY - 2 * epsilon);
        oRing.setPoint(1, sEnvelope.MinX - 2 * epsilon,
                       sEnvelope.MaxY + 1 * epsilon);
        oRing.setPoint(2, sEnvelope.MaxX + 1 * epsilon,
                       sEnvelope.MaxY + 1 * epsilon);
        oRing.setPoint(3, sEnvelope.MaxX + 1 * epsilon,
                       sEnvelope.MinY - 2 * epsilon);
        oRing.setPoint(4, sEnvelope.MinX - 2 * epsilon,
                       sEnvelope.MinY - 2 * epsilon);

        OGRPolygon oInclusiveFilter;
        oInclusiveFilter.addRing(&oRing);

        LOG_ACTION(poLayer->SetSpatialFilter(iGeomField, &oInclusiveFilter));

        /* --------------------------------------------------------------------
         */
        /*      Verify that we can find the target feature. */
        /* --------------------------------------------------------------------
         */
        LOG_ACTION(poLayer->ResetReading());

        bool bFound = false;
        OGRFeature *poFeature = nullptr;
        while ((poFeature = LOG_ACTION(poLayer->GetNextFeature())) != nullptr)
        {
            if (poFeature->Equal(poTargetFeature))
            {
                bFound = true;
                DestroyFeatureAndNullify(poFeature);
                break;
            }
            else
                DestroyFeatureAndNullify(poFeature);
        }

        if (!bFound)
        {
            bRet = FALSE;
            printf("ERROR: Spatial filter (%d) eliminated feature " CPL_FRMT_GIB
                   " unexpectedly!\n",
                   iGeomField, poTargetFeature->GetFID());
            DestroyFeatureAndNullify(poTargetFeature);
            break;
        }

        DestroyFeatureAndNullify(poTargetFeature);
    }

    /* -------------------------------------------------------------------- */
    /*     Reset spatial filter                                             */
    /* -------------------------------------------------------------------- */
    LOG_ACTION(poLayer->SetSpatialFilter(nullptr));

    if (bRet && bVerbose)
    {
        printf("INFO: Full spatial filter succeeded.\n");
    }

    return bRet;
}

static int TestSpatialFilter(OGRLayer *poLayer)
{
    /* -------------------------------------------------------------------- */
    /*      Read the target feature.                                        */
    /* -------------------------------------------------------------------- */
    LOG_ACTION(poLayer->ResetReading());
    OGRFeature *poTargetFeature = LOG_ACTION(poLayer->GetNextFeature());

    if (poTargetFeature == nullptr)
    {
        if (bVerbose)
        {
            printf("INFO: Skipping Spatial Filter test for %s.\n"
                   "      No features in layer.\n",
                   poLayer->GetName());
        }
        return TRUE;
    }
    DestroyFeatureAndNullify(poTargetFeature);

    const int nGeomFieldCount =
        LOG_ACTION(poLayer->GetLayerDefn()->GetGeomFieldCount());
    if (nGeomFieldCount == 0)
    {
        if (bVerbose)
        {
            printf("INFO: Skipping Spatial Filter test for %s,\n"
                   "      target feature has no geometry.\n",
                   poLayer->GetName());
        }
        return TRUE;
    }

    int bRet = TRUE;
    for (int iGeom = 0; iGeom < nGeomFieldCount; iGeom++)
    {
        bRet &= TestSpatialFilter(poLayer, iGeom);

        if (bFullSpatialFilter)
            bRet &= TestFullSpatialFilter(poLayer, iGeom);
    }

    CPLErrorReset();
    CPLPushErrorHandler(CPLQuietErrorHandler);
    OGRPolygon oPolygon;
    LOG_ACTION(poLayer->SetSpatialFilter(-1, &oPolygon));
    CPLPopErrorHandler();
    if (CPLGetLastErrorType() == 0)
        printf("WARNING: poLayer->SetSpatialFilter(-1) "
               "should emit an error.\n");

    CPLErrorReset();
    CPLPushErrorHandler(CPLQuietErrorHandler);
    LOG_ACTION(poLayer->SetSpatialFilter(nGeomFieldCount, &oPolygon));
    CPLPopErrorHandler();
    if (CPLGetLastErrorType() == 0)
        printf("WARNING: poLayer->SetSpatialFilter(nGeomFieldCount) "
               "should emit an error.\n");

    return bRet;
}

/************************************************************************/
/*                  GetQuotedIfNeededIdentifier()                       */
/************************************************************************/

static std::string GetQuotedIfNeededIdentifier(const char *pszFieldName)
{
    std::string osIdentifier;
    const bool bMustQuoteAttrName =
        pszFieldName[0] == '\0' || strchr(pszFieldName, '_') ||
        strchr(pszFieldName, ' ') || swq_is_reserved_keyword(pszFieldName);
    if (bMustQuoteAttrName)
    {
        osIdentifier = "\"";
        osIdentifier += pszFieldName;
        osIdentifier += "\"";
    }
    else
    {
        osIdentifier = pszFieldName;
    }
    return osIdentifier;
}

/************************************************************************/
/*                       GetAttributeFilters()                         */
/************************************************************************/

static bool GetAttributeFilters(OGRLayer *poLayer,
                                std::unique_ptr<OGRFeature> &poTargetFeature,
                                std::string &osInclusiveFilter,
                                std::string &osExclusiveFilter)
{

    /* -------------------------------------------------------------------- */
    /*      Read the target feature.                                        */
    /* -------------------------------------------------------------------- */
    LOG_ACTION(poLayer->ResetReading());
    poTargetFeature.reset(LOG_ACTION(poLayer->GetNextFeature()));

    if (poTargetFeature == nullptr)
    {
        if (bVerbose)
        {
            printf("INFO: Skipping Attribute Filter test for %s.\n"
                   "      No features in layer.\n",
                   poLayer->GetName());
        }
        return false;
    }

    int i = 0;
    OGRFieldType eType = OFTString;
    for (i = 0; i < poTargetFeature->GetFieldCount(); i++)
    {
        eType = poTargetFeature->GetFieldDefnRef(i)->GetType();
        if (poTargetFeature->IsFieldSetAndNotNull(i) &&
            (eType == OFTString || eType == OFTInteger || eType == OFTReal))
        {
            break;
        }
    }
    if (i == poTargetFeature->GetFieldCount())
    {
        if (bVerbose)
        {
            printf("INFO: Skipping Attribute Filter test for %s.\n"
                   "      Could not find non NULL field.\n",
                   poLayer->GetName());
        }
        return false;
    }

    const std::string osFieldName =
        poTargetFeature->GetFieldDefnRef(i)->GetNameRef();
    CPLString osValue = poTargetFeature->GetFieldAsString(i);
    if (eType == OFTReal)
    {
        int nWidth = poTargetFeature->GetFieldDefnRef(i)->GetWidth();
        int nPrecision = poTargetFeature->GetFieldDefnRef(i)->GetPrecision();
        if (nWidth > 0)
        {
            char szFormat[32];
            snprintf(szFormat, sizeof(szFormat), "%%%d.%df", nWidth,
                     nPrecision);
            osValue.Printf(szFormat, poTargetFeature->GetFieldAsDouble(i));
        }
        else
            osValue.Printf("%.18g", poTargetFeature->GetFieldAsDouble(i));
    }

    /* -------------------------------------------------------------------- */
    /*      Construct inclusive filter.                                     */
    /* -------------------------------------------------------------------- */
    osInclusiveFilter = GetQuotedIfNeededIdentifier(osFieldName.c_str());
    osInclusiveFilter += " = ";
    if (eType == OFTString)
        osInclusiveFilter += "'";
    osInclusiveFilter += osValue;
    if (eType == OFTString)
        osInclusiveFilter += "'";
    /* Make sure that the literal will be recognized as a float value */
    /* to avoid int underflow/overflow */
    else if (eType == OFTReal && strchr(osValue, '.') == nullptr)
        osInclusiveFilter += ".";

    /* -------------------------------------------------------------------- */
    /*      Construct exclusive filter.                                     */
    /* -------------------------------------------------------------------- */
    osExclusiveFilter = GetQuotedIfNeededIdentifier(osFieldName.c_str());
    osExclusiveFilter += " <> ";
    if (eType == OFTString)
        osExclusiveFilter += "'";
    osExclusiveFilter += osValue;
    if (eType == OFTString)
        osExclusiveFilter += "'";
    /* Make sure that the literal will be recognized as a float value */
    /* to avoid int underflow/overflow */
    else if (eType == OFTReal && strchr(osValue, '.') == nullptr)
        osExclusiveFilter += ".";

    return true;
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

static int TestAttributeFilter(CPL_UNUSED GDALDataset *poDS, OGRLayer *poLayer)

{
    int bRet = TRUE;

    std::unique_ptr<OGRFeature> poTargetFeature;
    std::string osInclusiveFilter;
    std::string osExclusiveFilter;
    if (!GetAttributeFilters(poLayer, poTargetFeature, osInclusiveFilter,
                             osExclusiveFilter))
    {
        return true;
    }

    /* -------------------------------------------------------------------- */
    /*      Apply inclusive filter.                                         */
    /* -------------------------------------------------------------------- */
    LOG_ACTION(poLayer->SetAttributeFilter(osInclusiveFilter.c_str()));

    /* -------------------------------------------------------------------- */
    /*      Verify that we can find the target feature.                     */
    /* -------------------------------------------------------------------- */
    LOG_ACTION(poLayer->ResetReading());

    bool bFoundFeature = false;
    OGRFeature *poFeature = nullptr;
    while ((poFeature = LOG_ACTION(poLayer->GetNextFeature())) != nullptr)
    {
        if (poFeature->Equal(poTargetFeature.get()))
        {
            bFoundFeature = true;
            DestroyFeatureAndNullify(poFeature);
            break;
        }
        else
        {
            DestroyFeatureAndNullify(poFeature);
        }
    }

    if (!bFoundFeature)
    {
        bRet = FALSE;
        printf("ERROR: Attribute filter eliminated a feature unexpectedly!\n");
    }
    else if (bVerbose)
    {
        printf("INFO: Attribute filter inclusion seems to work.\n");
    }

    const GIntBig nInclusiveCount = LOG_ACTION(poLayer->GetFeatureCount());

    /* -------------------------------------------------------------------- */
    /*      Apply exclusive filter.                                         */
    /* -------------------------------------------------------------------- */
    LOG_ACTION(poLayer->SetAttributeFilter(osExclusiveFilter.c_str()));

    /* -------------------------------------------------------------------- */
    /*      Verify that we can find the target feature.                     */
    /* -------------------------------------------------------------------- */
    LOG_ACTION(poLayer->ResetReading());

    GIntBig nExclusiveCountWhileIterating = 0;
    while ((poFeature = LOG_ACTION(poLayer->GetNextFeature())) != nullptr)
    {
        if (poFeature->Equal(poTargetFeature.get()))
        {
            DestroyFeatureAndNullify(poFeature);
            break;
        }
        else
        {
            DestroyFeatureAndNullify(poFeature);
        }
        nExclusiveCountWhileIterating++;
    }

    const GIntBig nExclusiveCount = LOG_ACTION(poLayer->GetFeatureCount());

    // Check that GetFeature() ignores the attribute filter
    OGRFeature *poFeature2 =
        LOG_ACTION(poLayer->GetFeature(poTargetFeature.get()->GetFID()));

    poLayer->ResetReading();
    OGRFeature *poFeature3 = nullptr;
    while ((poFeature3 = LOG_ACTION(poLayer->GetNextFeature())) != nullptr)
    {
        if (poFeature3->Equal(poTargetFeature.get()))
        {
            DestroyFeatureAndNullify(poFeature3);
            break;
        }
        else
            DestroyFeatureAndNullify(poFeature3);
    }

    LOG_ACTION(poLayer->SetAttributeFilter(nullptr));

    const GIntBig nTotalCount = LOG_ACTION(poLayer->GetFeatureCount());

    if (poFeature != nullptr)
    {
        bRet = FALSE;
        printf("ERROR: Attribute filter failed to eliminate "
               "a feature unexpectedly!\n");
    }
    else if (nExclusiveCountWhileIterating != nExclusiveCount ||
             nExclusiveCount >= nTotalCount || nInclusiveCount > nTotalCount ||
             (nInclusiveCount == nTotalCount && nExclusiveCount != 0))
    {
        bRet = FALSE;
        printf("ERROR: GetFeatureCount() may not be taking attribute "
               "filter into account (nInclusiveCount = " CPL_FRMT_GIB
               ", nExclusiveCount = " CPL_FRMT_GIB
               ", nExclusiveCountWhileIterating = " CPL_FRMT_GIB
               ", nTotalCount = " CPL_FRMT_GIB ").\n",
               nInclusiveCount, nExclusiveCount, nExclusiveCountWhileIterating,
               nTotalCount);
    }
    else if (bVerbose)
    {
        printf("INFO: Attribute filter exclusion seems to work.\n");
    }

    if (poFeature2 == nullptr || !poFeature2->Equal(poTargetFeature.get()))
    {
        bRet = FALSE;
        printf("ERROR: Attribute filter has been taken into account "
               "by GetFeature()\n");
    }
    else if (bVerbose)
    {
        printf("INFO: Attribute filter is ignored by GetFeature() "
               "as expected.\n");
    }

    if (poFeature3 != nullptr)
    {
        bRet = FALSE;
        printf("ERROR: Attribute filter has not been restored correctly "
               "after GetFeature()\n");
    }

    if (poFeature2 != nullptr)
        DestroyFeatureAndNullify(poFeature2);

    return bRet;
}

/************************************************************************/
/*                         TestOGRLayerUTF8()                           */
/************************************************************************/

static int TestOGRLayerUTF8(OGRLayer *poLayer)
{
    int bRet = TRUE;

    LOG_ACTION(poLayer->SetSpatialFilter(nullptr));
    LOG_ACTION(poLayer->SetAttributeFilter(nullptr));
    LOG_ACTION(poLayer->ResetReading());

    const int bIsAdvertizedAsUTF8 =
        LOG_ACTION(poLayer->TestCapability(OLCStringsAsUTF8));
    const int nFields = LOG_ACTION(poLayer->GetLayerDefn()->GetFieldCount());
    bool bFoundString = false;
    bool bFoundNonASCII = false;
    bool bFoundUTF8 = false;
    bool bCanAdvertiseUTF8 = true;

    OGRFeature *poFeature = nullptr;
    while (bRet &&
           (poFeature = LOG_ACTION(poLayer->GetNextFeature())) != nullptr)
    {
        for (int i = 0; i < nFields; i++)
        {
            if (!poFeature->IsFieldSet(i))
                continue;
            if (poFeature->GetFieldDefnRef(i)->GetType() == OFTString)
            {
                const char *pszVal = poFeature->GetFieldAsString(i);
                if (pszVal[0] != 0)
                {
                    bFoundString = true;
                    const GByte *pszIter =
                        reinterpret_cast<const GByte *>(pszVal);
                    bool bIsASCII = true;
                    while (*pszIter)
                    {
                        if (*pszIter >= 128)
                        {
                            bFoundNonASCII = true;
                            bIsASCII = false;
                            break;
                        }
                        pszIter++;
                    }
                    int bIsUTF8 = CPLIsUTF8(pszVal, -1);
                    if (bIsUTF8 && !bIsASCII)
                        bFoundUTF8 = true;
                    if (bIsAdvertizedAsUTF8)
                    {
                        if (!bIsUTF8)
                        {
                            printf("ERROR: Found non-UTF8 content at field %d "
                                   "of feature " CPL_FRMT_GIB
                                   ", but layer is advertized as UTF-8.\n",
                                   i, poFeature->GetFID());
                            bRet = FALSE;
                            break;
                        }
                    }
                    else
                    {
                        if (!bIsUTF8)
                            bCanAdvertiseUTF8 = false;
                    }
                }
            }
        }
        DestroyFeatureAndNullify(poFeature);
    }

    if (!bFoundString)
    {
    }
    else if (bCanAdvertiseUTF8 && bVerbose)
    {
        if (bIsAdvertizedAsUTF8)
        {
            if (bFoundUTF8)
            {
                printf("INFO: Layer has UTF-8 content and is consistently "
                       "declared as having UTF-8 content.\n");
            }
            else if (!bFoundNonASCII)
            {
                printf("INFO: Layer has ASCII only content and is "
                       "consistently declared as having UTF-8 content.\n");
            }
        }
        else
        {
            if (bFoundUTF8)
            {
                printf("INFO: Layer could perhaps be advertized as UTF-8 "
                       "compatible (and it has non-ASCII UTF-8 content).\n");
            }
            else if (!bFoundNonASCII)
            {
                printf("INFO: Layer could perhaps be advertized as UTF-8 "
                       "compatible (it has only ASCII content).\n");
            }
        }
    }
    else if (bVerbose)
    {
        printf("INFO: Layer has non UTF-8 content (and is consistently "
               "declared as not being UTF-8 compatible).\n");
    }

    return bRet;
}

/************************************************************************/
/*                         TestGetExtent()                              */
/************************************************************************/

static int TestGetExtent(OGRLayer *poLayer, int iGeomField)
{
    int bRet = TRUE;

    LOG_ACTION(poLayer->SetSpatialFilter(nullptr));
    LOG_ACTION(poLayer->SetAttributeFilter(nullptr));
    LOG_ACTION(poLayer->ResetReading());

    OGREnvelope sExtent;
    OGREnvelope sExtentSlow;

    OGRErr eErr = LOG_ACTION(poLayer->GetExtent(iGeomField, &sExtent, TRUE));
    OGRErr eErr2 = LOG_ACTION(
        poLayer->OGRLayer::GetExtent(iGeomField, &sExtentSlow, TRUE));

    if (eErr != eErr2)
    {
        if (eErr == OGRERR_NONE && eErr2 != OGRERR_NONE)
        {
            // With the LIBKML driver and test_ogrsf:
            // ../autotest/ogr/data/samples.kml "Styles and Markup"
            if (bVerbose)
            {
                printf("INFO: GetExtent() succeeded but OGRLayer::GetExtent() "
                       "failed.\n");
            }
        }
        else
        {
            bRet = FALSE;
            if (bVerbose)
            {
                printf("ERROR: GetExtent() failed but OGRLayer::GetExtent() "
                       "succeeded.\n");
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
                printf("INFO: unknown relationship between sExtent and "
                       "sExtentSlow.\n");
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

static int TestGetExtent(OGRLayer *poLayer)
{
    int bRet = TRUE;
    const int nGeomFieldCount =
        LOG_ACTION(poLayer->GetLayerDefn()->GetGeomFieldCount());
    for (int iGeom = 0; iGeom < nGeomFieldCount; iGeom++)
        bRet &= TestGetExtent(poLayer, iGeom);

    OGREnvelope sExtent;
    CPLPushErrorHandler(CPLQuietErrorHandler);
    OGRErr eErr = LOG_ACTION(poLayer->GetExtent(-1, &sExtent, TRUE));
    CPLPopErrorHandler();
    if (eErr != OGRERR_FAILURE)
    {
        printf("ERROR: poLayer->GetExtent(-1) should fail.\n");
        bRet = FALSE;
    }

    CPLPushErrorHandler(CPLQuietErrorHandler);
    eErr = LOG_ACTION(poLayer->GetExtent(nGeomFieldCount, &sExtent, TRUE));
    CPLPopErrorHandler();
    if (eErr != OGRERR_FAILURE)
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

static int TestOGRLayerDeleteAndCreateFeature(OGRLayer *poLayer)

{
    int bRet = TRUE;

    LOG_ACTION(poLayer->SetSpatialFilter(nullptr));

    if (!LOG_ACTION(poLayer->TestCapability(OLCRandomRead)))
    {
        if (bVerbose)
            printf("INFO: Skipping delete feature test since this layer "
                   "doesn't support random read.\n");
        return bRet;
    }

    if (LOG_ACTION(poLayer->GetFeatureCount()) == 0)
    {
        if (bVerbose)
            printf("INFO: No feature available on layer '%s',"
                   "skipping delete/create feature test.\n",
                   poLayer->GetName());

        return bRet;
    }
    /* -------------------------------------------------------------------- */
    /*      Fetch the last feature                                          */
    /* -------------------------------------------------------------------- */
    OGRFeature *poFeatureTest = nullptr;
    GIntBig nFID = 0;

    LOG_ACTION(poLayer->ResetReading());

    LOG_ACTION(poLayer->SetNextByIndex(poLayer->GetFeatureCount() - 1));
    OGRFeature *poFeature = LOG_ACTION(poLayer->GetNextFeature());
    if (poFeature == nullptr)
    {
        bRet = FALSE;
        printf("ERROR: Could not get last feature of layer.\n");
        goto end;
    }

    /* -------------------------------------------------------------------- */
    /*      Get the feature ID of the last feature                          */
    /* -------------------------------------------------------------------- */
    nFID = poFeature->GetFID();

    /* -------------------------------------------------------------------- */
    /*      Delete the feature.                                             */
    /* -------------------------------------------------------------------- */

    if (LOG_ACTION(poLayer->DeleteFeature(nFID)) != OGRERR_NONE)
    {
        bRet = FALSE;
        printf("ERROR: Attempt to DeleteFeature() failed.\n");
        goto end;
    }

    /* -------------------------------------------------------------------- */
    /*      Now re-read the feature to verify the delete effect worked.     */
    /* -------------------------------------------------------------------- */
    // Silent legitimate error message.
    CPLPushErrorHandler(CPLQuietErrorHandler);
    poFeatureTest = LOG_ACTION(poLayer->GetFeature(nFID));
    CPLPopErrorHandler();
    if (poFeatureTest != nullptr)
    {
        bRet = FALSE;
        printf("ERROR: The feature was not deleted.\n");
    }
    else if (bVerbose)
    {
        printf("INFO: Delete Feature test passed.\n");
    }
    DestroyFeatureAndNullify(poFeatureTest);

    /* -------------------------------------------------------------------- */
    /*      Re-insert the features to restore to original state             */
    /* -------------------------------------------------------------------- */
    if (LOG_ACTION(poLayer->CreateFeature(poFeature)) != OGRERR_NONE)
    {
        bRet = FALSE;
        printf("ERROR: Attempt to restore feature failed.\n");
    }

    if (poFeature->GetFID() != nFID)
    {
        /* Case of shapefile driver for example that will not try to */
        /* reuse the existing FID, but will assign a new one */
        if (bVerbose)
        {
            printf("INFO: Feature was created, "
                   "but with not its original FID.\n");
        }
        nFID = poFeature->GetFID();
    }

    /* -------------------------------------------------------------------- */
    /*      Now re-read the feature to verify the create effect worked.     */
    /* -------------------------------------------------------------------- */
    poFeatureTest = LOG_ACTION(poLayer->GetFeature(nFID));
    if (poFeatureTest == nullptr)
    {
        bRet = FALSE;
        printf("ERROR: The feature was not created.\n");
    }
    else if (bVerbose)
    {
        printf("INFO: Create Feature test passed.\n");
    }
    DestroyFeatureAndNullify(poFeatureTest);

end:
    /* -------------------------------------------------------------------- */
    /*      Cleanup.                                                        */
    /* -------------------------------------------------------------------- */

    DestroyFeatureAndNullify(poFeature);

    return bRet;
}

/*************************************************************************/
/*                         TestTransactions()                            */
/*************************************************************************/

static int TestTransactions(OGRLayer *poLayer)

{
    GIntBig nInitialFeatureCount = LOG_ACTION(poLayer->GetFeatureCount());

    OGRErr eErr = LOG_ACTION(poLayer->StartTransaction());
    if (eErr == OGRERR_NONE)
    {
        if (LOG_ACTION(poLayer->TestCapability(OLCTransactions)) == FALSE)
        {
            eErr = LOG_ACTION(poLayer->RollbackTransaction());
            if (eErr == OGRERR_UNSUPPORTED_OPERATION &&
                LOG_ACTION(poLayer->TestCapability(OLCTransactions)) == FALSE)
            {
                // The default implementation has a dummy
                // StartTransaction(), but RollbackTransaction()
                // returns OGRERR_UNSUPPORTED_OPERATION
                if (bVerbose)
                {
                    printf("INFO: Transactions test skipped due to lack of "
                           "transaction support.\n");
                }
                return TRUE;
            }
            else
            {
                printf("WARN: StartTransaction() is supported, but "
                       "TestCapability(OLCTransactions) returns FALSE.\n");
            }
        }
    }
    else if (eErr == OGRERR_FAILURE)
    {
        if (LOG_ACTION(poLayer->TestCapability(OLCTransactions)) == TRUE)
        {
            printf("ERROR: StartTransaction() failed, but "
                   "TestCapability(OLCTransactions) returns TRUE.\n");
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
        printf("ERROR: RollbackTransaction() failed after successful "
               "StartTransaction().\n");
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
        printf("ERROR: CommitTransaction() failed after successful "
               "StartTransaction().\n");
        return FALSE;
    }

    /* ---------------- */

    eErr = LOG_ACTION(poLayer->StartTransaction());
    if (eErr != OGRERR_NONE)
    {
        printf("ERROR: StartTransaction() failed.\n");
        return FALSE;
    }

    OGRFeature *poFeature = new OGRFeature(poLayer->GetLayerDefn());
    if (poLayer->GetLayerDefn()->GetFieldCount() > 0)
        poFeature->SetField(0, "0");
    eErr = LOG_ACTION(poLayer->CreateFeature(poFeature));
    delete poFeature;
    poFeature = nullptr;

    if (eErr == OGRERR_FAILURE)
    {
        if (bVerbose)
        {
            printf("INFO: CreateFeature() failed. Exiting this test now.\n");
        }
        LOG_ACTION(poLayer->RollbackTransaction());
        return TRUE;
    }

    eErr = LOG_ACTION(poLayer->RollbackTransaction());
    if (eErr != OGRERR_NONE)
    {
        printf("ERROR: RollbackTransaction() failed after successful "
               "StartTransaction().\n");
        return FALSE;
    }

    if (LOG_ACTION(poLayer->GetFeatureCount()) != nInitialFeatureCount)
    {
        printf("ERROR: GetFeatureCount() should have returned its initial "
               "value after RollbackTransaction().\n");
        return FALSE;
    }

    /* ---------------- */

    if (LOG_ACTION(poLayer->TestCapability(OLCDeleteFeature)))
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
        poFeature = nullptr;

        if (eErr == OGRERR_FAILURE)
        {
            printf("ERROR: CreateFeature() failed. Exiting this test now.\n");
            LOG_ACTION(poLayer->RollbackTransaction());
            return FALSE;
        }

        if (nFID < 0)
        {
            printf("WARNING: CreateFeature() returned featured without FID.\n");
            LOG_ACTION(poLayer->RollbackTransaction());
            return FALSE;
        }

        eErr = LOG_ACTION(poLayer->CommitTransaction());
        if (eErr != OGRERR_NONE)
        {
            printf("ERROR: CommitTransaction() failed after successful "
                   "StartTransaction().\n");
            return FALSE;
        }

        if (LOG_ACTION(poLayer->GetFeatureCount()) != nInitialFeatureCount + 1)
        {
            printf("ERROR: GetFeatureCount() should have returned its initial "
                   "value + 1 after CommitTransaction().\n");
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
            printf("ERROR: GetFeatureCount() should have returned its initial "
                   "value after DeleteFeature().\n");
            return FALSE;
        }
    }

    /* ---------------- */

    if (bVerbose)
    {
        printf("INFO: Transactions test passed.\n");
    }

    return TRUE;
}

/************************************************************************/
/*                     TestOGRLayerIgnoreFields()                       */
/************************************************************************/

static int TestOGRLayerIgnoreFields(OGRLayer *poLayer)
{
    int iFieldNonEmpty = -1;
    int iFieldNonEmpty2 = -1;
    bool bGeomNonEmpty = false;

    LOG_ACTION(poLayer->ResetReading());
    OGRFeature *poFeature = nullptr;
    while ((poFeature = LOG_ACTION(poLayer->GetNextFeature())) != nullptr)
    {
        if (iFieldNonEmpty < 0)
        {
            for (int i = 0; i < poFeature->GetFieldCount(); i++)
            {
                if (poFeature->IsFieldSetAndNotNull(i))
                {
                    iFieldNonEmpty = i;
                    break;
                }
            }
        }
        else if (iFieldNonEmpty2 < 0)
        {
            for (int i = 0; i < poFeature->GetFieldCount(); i++)
            {
                if (i != iFieldNonEmpty && poFeature->IsFieldSetAndNotNull(i))
                {
                    iFieldNonEmpty2 = i;
                    break;
                }
            }
        }

        if (!bGeomNonEmpty && poFeature->GetGeometryRef() != nullptr)
            bGeomNonEmpty = true;

        delete poFeature;
    }

    if (iFieldNonEmpty < 0 && !bGeomNonEmpty)
    {
        if (bVerbose)
        {
            printf("INFO: IgnoreFields test skipped.\n");
        }
        return TRUE;
    }

    char **papszIgnoredFields = nullptr;
    if (iFieldNonEmpty >= 0)
        papszIgnoredFields =
            CSLAddString(papszIgnoredFields, poLayer->GetLayerDefn()
                                                 ->GetFieldDefn(iFieldNonEmpty)
                                                 ->GetNameRef());

    if (bGeomNonEmpty)
        papszIgnoredFields = CSLAddString(papszIgnoredFields, "OGR_GEOMETRY");

    OGRErr eErr = LOG_ACTION(poLayer->SetIgnoredFields(
        const_cast<const char **>(papszIgnoredFields)));
    CSLDestroy(papszIgnoredFields);

    if (eErr == OGRERR_FAILURE)
    {
        printf("ERROR: SetIgnoredFields() failed.\n");
        poLayer->SetIgnoredFields(nullptr);
        return FALSE;
    }

    bool bFoundNonEmpty2 = false;

    LOG_ACTION(poLayer->ResetReading());
    while ((poFeature = LOG_ACTION(poLayer->GetNextFeature())) != nullptr)
    {
        if (iFieldNonEmpty >= 0 &&
            poFeature->IsFieldSetAndNotNull(iFieldNonEmpty))
        {
            delete poFeature;
            printf("ERROR: After SetIgnoredFields(), "
                   "found a non empty field that should have been ignored.\n");
            poLayer->SetIgnoredFields(nullptr);
            return FALSE;
        }

        if (iFieldNonEmpty2 >= 0 &&
            poFeature->IsFieldSetAndNotNull(iFieldNonEmpty2))
            bFoundNonEmpty2 = true;

        if (bGeomNonEmpty && poFeature->GetGeometryRef() != nullptr)
        {
            delete poFeature;
            printf(
                "ERROR: After SetIgnoredFields(), "
                "found a non empty geometry that should have been ignored.\n");
            poLayer->SetIgnoredFields(nullptr);
            return FALSE;
        }

        delete poFeature;
    }

    if (iFieldNonEmpty2 >= 0 && !bFoundNonEmpty2)
    {
        printf("ERROR: SetIgnoredFields() discarded fields that it "
               "should not have discarded.\n");
        poLayer->SetIgnoredFields(nullptr);
        return FALSE;
    }

    LOG_ACTION(poLayer->SetIgnoredFields(nullptr));

    if (bVerbose)
    {
        printf("INFO: IgnoreFields test passed.\n");
    }

    return TRUE;
}

/************************************************************************/
/*                            TestLayerSQL()                            */
/************************************************************************/

static int TestLayerSQL(GDALDataset *poDS, OGRLayer *poLayer)

{
    int bRet = TRUE;
    bool bGotFeature = false;

    /* Test consistency between result layer and traditional layer */
    LOG_ACTION(poLayer->ResetReading());
    OGRFeature *poLayerFeat = LOG_ACTION(poLayer->GetNextFeature());

    /* Reset to avoid potentially a statement to be active which cause */
    /* issue in the transaction test of the second layer, when testing */
    /* multi-tables sqlite and gpkg databases */
    LOG_ACTION(poLayer->ResetReading());

    CPLString osSQL;
    osSQL.Printf("SELECT * FROM %s",
                 GetLayerNameForSQL(poDS, poLayer->GetName()));
    OGRLayer *poSQLLyr =
        LOG_ACTION(poDS->ExecuteSQL(osSQL.c_str(), nullptr, nullptr));
    OGRFeature *poSQLFeat = nullptr;
    if (poSQLLyr == nullptr)
    {
        printf("ERROR: ExecuteSQL(%s) failed.\n", osSQL.c_str());
        bRet = FALSE;
        return bRet;
    }
    else
    {
        poSQLFeat = LOG_ACTION(poSQLLyr->GetNextFeature());
        if (poSQLFeat != nullptr)
            bGotFeature = TRUE;
        if (poLayerFeat == nullptr && poSQLFeat != nullptr)
        {
            printf("ERROR: poLayerFeat == NULL && poSQLFeat != NULL.\n");
            bRet = FALSE;
        }
        else if (poLayerFeat != nullptr && poSQLFeat == nullptr)
        {
            printf("ERROR: poLayerFeat != NULL && poSQLFeat == NULL.\n");
            bRet = FALSE;
        }
        else if (poLayerFeat != nullptr && poSQLFeat != nullptr)
        {
            if (poLayer->GetLayerDefn()->GetGeomFieldCount() !=
                poSQLLyr->GetLayerDefn()->GetGeomFieldCount())
            {
                printf("ERROR: poLayer->GetLayerDefn()->GetGeomFieldCount() != "
                       "poSQLLyr->GetLayerDefn()->GetGeomFieldCount().\n");
                bRet = FALSE;
            }
            else
            {
                int nGeomFieldCount =
                    poLayer->GetLayerDefn()->GetGeomFieldCount();
                for (int i = 0; i < nGeomFieldCount; i++)
                {
                    int iOtherI;
                    if (nGeomFieldCount != 1)
                    {
                        OGRGeomFieldDefn *poGFldDefn =
                            poLayer->GetLayerDefn()->GetGeomFieldDefn(i);
                        iOtherI = poSQLLyr->GetLayerDefn()->GetGeomFieldIndex(
                            poGFldDefn->GetNameRef());
                        if (iOtherI == -1)
                        {
                            printf("ERROR: Cannot find geom field in SQL "
                                   "matching %s.\n",
                                   poGFldDefn->GetNameRef());
                            break;
                        }
                    }
                    else
                        iOtherI = 0;
                    OGRGeometry *poLayerFeatGeom =
                        poLayerFeat->GetGeomFieldRef(i);
                    OGRGeometry *poSQLFeatGeom =
                        poSQLFeat->GetGeomFieldRef(iOtherI);
                    if (poLayerFeatGeom == nullptr && poSQLFeatGeom != nullptr)
                    {
                        printf("ERROR: poLayerFeatGeom[%d] == NULL && "
                               "poSQLFeatGeom[%d] != NULL.\n",
                               i, iOtherI);
                        bRet = FALSE;
                    }
                    else if (poLayerFeatGeom != nullptr &&
                             poSQLFeatGeom == nullptr)
                    {
                        printf("ERROR: poLayerFeatGeom[%d] != NULL && "
                               "poSQLFeatGeom[%d] == NULL.\n",
                               i, iOtherI);
                        bRet = FALSE;
                    }
                    else if (poLayerFeatGeom != nullptr &&
                             poSQLFeatGeom != nullptr)
                    {
                        const OGRSpatialReference *poLayerFeatSRS =
                            poLayerFeatGeom->getSpatialReference();
                        const OGRSpatialReference *poSQLFeatSRS =
                            poSQLFeatGeom->getSpatialReference();
                        if (poLayerFeatSRS == nullptr &&
                            poSQLFeatSRS != nullptr)
                        {
                            printf("ERROR: poLayerFeatSRS == NULL && "
                                   "poSQLFeatSRS != NULL.\n");
                            bRet = FALSE;
                        }
                        else if (poLayerFeatSRS != nullptr &&
                                 poSQLFeatSRS == nullptr)
                        {
                            printf("ERROR: poLayerFeatSRS != NULL && "
                                   "poSQLFeatSRS == NULL.\n");
                            bRet = FALSE;
                        }
                        else if (poLayerFeatSRS != nullptr &&
                                 poSQLFeatSRS != nullptr)
                        {
                            if (!(poLayerFeatSRS->IsSame(poSQLFeatSRS)))
                            {
                                printf("ERROR: !(poLayerFeatSRS->IsSame("
                                       "poSQLFeatSRS)).\n");
                                bRet = FALSE;
                            }
                        }
                    }
                }
            }
        }
    }

    DestroyFeatureAndNullify(poLayerFeat);
    DestroyFeatureAndNullify(poSQLFeat);

    LOG_ACTION(poDS->ReleaseResultSet(poSQLLyr));

    /* Try ResetReading(), GetNextFeature(), ResetReading(), GetNextFeature() */
    poSQLLyr = LOG_ACTION(poDS->ExecuteSQL(osSQL.c_str(), nullptr, nullptr));
    if (poSQLLyr == nullptr)
    {
        printf("ERROR: ExecuteSQL(%s) failed at line %d "
               "(but succeeded before).\n",
               osSQL.c_str(), __LINE__);
        bRet = FALSE;
        return bRet;
    }
    LOG_ACTION(poSQLLyr->ResetReading());

    poSQLFeat = LOG_ACTION(poSQLLyr->GetNextFeature());
    if (poSQLFeat == nullptr && bGotFeature)
    {
        printf("ERROR: Should have got feature (1)\n");
        bRet = FALSE;
    }
    DestroyFeatureAndNullify(poSQLFeat);

    LOG_ACTION(poSQLLyr->ResetReading());

    poSQLFeat = LOG_ACTION(poSQLLyr->GetNextFeature());
    if (poSQLFeat == nullptr && bGotFeature)
    {
        printf("ERROR: Should have got feature (2)\n");
        bRet = FALSE;
    }
    DestroyFeatureAndNullify(poSQLFeat);

    LOG_ACTION(poDS->ReleaseResultSet(poSQLLyr));

    /* Return an empty layer */
    osSQL.Printf("SELECT * FROM %s WHERE 0 = 1",
                 GetLayerNameForSQL(poDS, poLayer->GetName()));

    poSQLLyr = LOG_ACTION(poDS->ExecuteSQL(osSQL.c_str(), nullptr, nullptr));
    if (poSQLLyr)
    {
        poSQLFeat = LOG_ACTION(poSQLLyr->GetNextFeature());
        if (poSQLFeat != nullptr)
        {
            bRet = FALSE;
            printf("ERROR: ExecuteSQL() should have returned "
                   "a layer without features.\n");
        }
        DestroyFeatureAndNullify(poSQLFeat);

        LOG_ACTION(poDS->ReleaseResultSet(poSQLLyr));
    }
    else
    {
        printf("ERROR: ExecuteSQL() should have returned a non-NULL result.\n");
        bRet = FALSE;
    }

    // Test that installing a spatial filter on an empty layer at ExecuteSQL()
    // does not raise an error
    osSQL.Printf("SELECT * FROM %s WHERE 0 = 1",
                 GetLayerNameForSQL(poDS, poLayer->GetName()));

    OGRLinearRing oRing;
    oRing.setPoint(0, 0, 0);
    oRing.setPoint(1, 0, 1);
    oRing.setPoint(2, 1, 1);
    oRing.setPoint(3, 1, 0);
    oRing.setPoint(4, 0, 0);

    OGRPolygon oPoly;
    oPoly.addRing(&oRing);

    CPLErrorReset();
    if (poLayer->GetLayerDefn()->GetGeomFieldCount() == 0)
    {
        CPLPushErrorHandler(CPLQuietErrorHandler);
        poSQLLyr = LOG_ACTION(poDS->ExecuteSQL(osSQL.c_str(), &oPoly, nullptr));
        CPLPopErrorHandler();
        if (poSQLLyr)
        {
            printf("WARNING: ExecuteSQL() with a spatial filter on a "
                   "non-spatial layer should have triggered an error.\n");
        }
    }
    else
    {
        poSQLLyr = LOG_ACTION(poDS->ExecuteSQL(osSQL.c_str(), &oPoly, nullptr));
        if (CPLGetLastErrorType() == CE_Failure &&
            poLayer->GetLayerDefn()->GetGeomFieldCount() > 0)
        {
            bRet = FALSE;
            printf("ERROR: ExecuteSQL() triggered an unexpected error.\n");
        }
        if (!poSQLLyr)
        {
            printf("ERROR: ExecuteSQL() should have returned a non-NULL "
                   "result.\n");
            bRet = FALSE;
        }
    }
    if (poSQLLyr)
    {
        CPLErrorReset();
        poSQLFeat = LOG_ACTION(poSQLLyr->GetNextFeature());
        if (CPLGetLastErrorType() == CE_Failure)
        {
            bRet = FALSE;
            printf("ERROR: GetNextFeature() triggered an unexpected error.\n");
        }
        if (poSQLFeat != nullptr)
        {
            bRet = FALSE;
            printf("ERROR: ExecuteSQL() should have returned "
                   "a layer without features.\n");
        }
        DestroyFeatureAndNullify(poSQLFeat);
        LOG_ACTION(poDS->ReleaseResultSet(poSQLLyr));
    }

    if (bRet && bVerbose)
        printf("INFO: TestLayerSQL passed.\n");

    return bRet;
}

/************************************************************************/
/*                  CountFeaturesUsingArrowStream()                     */
/************************************************************************/

static int64_t CountFeaturesUsingArrowStream(OGRLayer *poLayer,
                                             int64_t nExpectedFID,
                                             int64_t nUnexpectedFID, bool &bOK)
{
    struct ArrowArrayStream stream;
    if (!LOG_ACTION(poLayer->GetArrowStream(&stream)))
    {
        printf("ERROR: GetArrowStream() failed\n");
        return -1;
    }
    struct ArrowSchema schema;
    if (stream.get_schema(&stream, &schema) != 0)
    {
        printf("ERROR: stream.get_schema() failed\n");
        stream.release(&stream);
        return -1;
    }
    int iFIDColumn = -1;
    if (schema.n_children > 0 &&
        (strcmp(schema.children[0]->name, "OGC_FID") == 0 ||
         strcmp(schema.children[0]->name, poLayer->GetFIDColumn()) == 0) &&
        strcmp(schema.children[0]->format, "l") == 0)
    {
        iFIDColumn = 0;
    }
    schema.release(&schema);
    int64_t nFeatureCountFiltered = 0;
    bool bExpectedFIDFound = false;
    bool bUnexpectedFIDFound = false;
    while (true)
    {
        struct ArrowArray array;
        if (stream.get_next(&stream, &array) != 0)
        {
            printf("ERROR: stream.get_next() is NULL\n");
            stream.release(&stream);
            return -1;
        }
        if (!array.release)
            break;
        if (iFIDColumn >= 0 && (nExpectedFID >= 0 || nUnexpectedFID >= 0))
        {
            const int64_t *panIds =
                static_cast<const int64_t *>(
                    array.children[iFIDColumn]->buffers[1]) +
                array.children[iFIDColumn]->offset;
            for (int64_t i = 0; i < array.length; ++i)
            {
                if (nExpectedFID >= 0 && panIds[i] == nExpectedFID)
                    bExpectedFIDFound = true;
                if (nUnexpectedFID >= 0 && panIds[i] == nUnexpectedFID)
                    bUnexpectedFIDFound = true;
            }
        }
        nFeatureCountFiltered += array.length;
        array.release(&array);
    }
    if (iFIDColumn >= 0)
    {
        if (nExpectedFID >= 0 && !bExpectedFIDFound)
        {
            bOK = false;
            printf("ERROR: CountFeaturesUsingArrowStream() :"
                   "expected to find feature of id %" PRId64
                   ", but did not get it\n",
                   nExpectedFID);
        }
        if (nUnexpectedFID >= 0 && bUnexpectedFIDFound)
        {
            bOK = false;
            printf("ERROR: CountFeaturesUsingArrowStream(): "
                   "expected *not* to find feature of id %" PRId64
                   ", but did get it\n",
                   nUnexpectedFID);
        }
    }
    stream.release(&stream);
    return nFeatureCountFiltered;
}

/************************************************************************/
/*                   TestLayerGetArrowStream()                          */
/************************************************************************/

static int TestLayerGetArrowStream(OGRLayer *poLayer)
{
    LOG_ACTION(poLayer->SetSpatialFilter(nullptr));
    LOG_ACTION(poLayer->SetAttributeFilter(nullptr));
    LOG_ACTION(poLayer->ResetReading());

    struct ArrowArrayStream stream;
    if (!LOG_ACTION(poLayer->GetArrowStream(&stream)))
    {
        printf("ERROR: GetArrowStream() failed\n");
        return false;
    }

    if (!stream.release)
    {
        printf("ERROR: stream.release is NULL\n");
        return false;
    }

    struct ArrowSchema schema;
    if (stream.get_schema(&stream, &schema) != 0)
    {
        printf("ERROR: stream.get_schema() failed\n");
        stream.release(&stream);
        return false;
    }

    if (!schema.release)
    {
        printf("ERROR: schema.release is NULL\n");
        stream.release(&stream);
        return false;
    }

    if (strcmp(schema.format, "+s") != 0)
    {
        printf("ERROR: expected schema.format to be '+s'. Got '%s'\n",
               schema.format);
        schema.release(&schema);
        stream.release(&stream);
        return false;
    }

    int64_t nFeatureCount = 0;
    while (true)
    {
        struct ArrowArray array;
        if (stream.get_next(&stream, &array) != 0)
        {
            printf("ERROR: stream.get_next() is NULL\n");
            schema.release(&schema);
            stream.release(&stream);
            return false;
        }
        if (array.release == nullptr)
        {
            break;
        }

        if (array.n_children != schema.n_children)
        {
            printf("ERROR: expected array.n_children (=%d) to be "
                   "schema.n_children (=%d)\n",
                   int(array.n_children), int(schema.n_children));
            array.release(&array);
            schema.release(&schema);
            stream.release(&stream);
            return false;
        }

        int bRet = true;
        for (int i = 0; i < array.n_children; ++i)
        {
            if (array.children[i]->length != array.length)
            {
                bRet = false;
                printf("ERROR: expected array.children[i]->length (=%d) to be "
                       "array.length (=%d)\n",
                       int(array.children[i]->length), int(array.length));
            }
        }
        if (!bRet)
        {
            array.release(&array);
            schema.release(&schema);
            stream.release(&stream);
            return false;
        }

        nFeatureCount += array.length;

        array.release(&array);

        if (array.release)
        {
            printf("ERROR: array.release should be NULL after release\n");
            schema.release(&schema);
            stream.release(&stream);
            return false;
        }
    }

    bool bRet = true;
    // We are a bit in non-specified behavior below by calling get_next()
    // after end of iteration.
    {
        struct ArrowArray array;
        if (stream.get_next(&stream, &array) == 0)
        {
            if (array.length != 0)
            {
                printf("WARNING: get_next() return an array with length != 0 "
                       "after end of iteration\n");
            }
            if (array.release)
                array.release(&array);
        }
    }

    schema.release(&schema);
    if (schema.release)
    {
        printf("ERROR: schema.release should be NULL after release\n");
        stream.release(&stream);
        return false;
    }

    stream.release(&stream);
    if (stream.release)
    {
        printf("ERROR: stream.release should be NULL after release\n");
        return false;
    }

    const int64_t nFCClassic = poLayer->GetFeatureCount(true);
    if (nFeatureCount != nFCClassic)
    {
        printf("ERROR: GetArrowStream() returned %" PRId64
               " features, whereas GetFeatureCount() returned %" PRId64 "\n",
               nFeatureCount, nFCClassic);
        bRet = false;
    }

    {
        LOG_ACTION(poLayer->SetAttributeFilter("1=1"));
        const auto nFeatureCountFiltered =
            CountFeaturesUsingArrowStream(poLayer, -1, -1, bRet);
        LOG_ACTION(poLayer->SetAttributeFilter(nullptr));
        if (nFeatureCount != nFeatureCountFiltered)
        {
            printf("ERROR: GetArrowStream() with 1=1 filter returned %" PRId64
                   " features, whereas %" PRId64 " expected\n",
                   nFeatureCountFiltered, nFeatureCount);
            bRet = false;
        }
    }

    {
        LOG_ACTION(poLayer->SetAttributeFilter("1=0"));
        const auto nFeatureCountFiltered =
            CountFeaturesUsingArrowStream(poLayer, -1, -1, bRet);
        LOG_ACTION(poLayer->SetAttributeFilter(nullptr));
        if (nFeatureCountFiltered != 0)
        {
            printf("ERROR: GetArrowStream() with 1=0 filter returned %" PRId64
                   " features, whereas 0 expected\n",
                   nFeatureCountFiltered);
            bRet = false;
        }
    }

    std::unique_ptr<OGRFeature> poTargetFeature;
    std::string osInclusiveFilter;
    std::string osExclusiveFilter;
    if (GetAttributeFilters(poLayer, poTargetFeature, osInclusiveFilter,
                            osExclusiveFilter))
    {
        {
            LOG_ACTION(poLayer->SetAttributeFilter(osInclusiveFilter.c_str()));
            const auto nFeatureCountFiltered = CountFeaturesUsingArrowStream(
                poLayer, poTargetFeature->GetFID(), -1, bRet);
            LOG_ACTION(poLayer->SetAttributeFilter(nullptr));
            if (nFeatureCountFiltered == 0)
            {
                printf(
                    "ERROR: GetArrowStream() with %s filter returned %" PRId64
                    " features, whereas at least one expected\n",
                    osInclusiveFilter.c_str(), nFeatureCountFiltered);
                bRet = false;
            }
            else if (bVerbose)
            {
                printf("INFO: Attribute filter inclusion with GetArrowStream "
                       "seems to work.\n");
            }
        }

        {
            LOG_ACTION(poLayer->SetAttributeFilter(osExclusiveFilter.c_str()));
            const auto nFeatureCountFiltered =
                CountFeaturesUsingArrowStream(poLayer, -1, -1, bRet);
            LOG_ACTION(poLayer->SetAttributeFilter(nullptr));
            if (nFeatureCountFiltered >= nFCClassic)
            {
                printf(
                    "ERROR: GetArrowStream() with %s filter returned %" PRId64
                    " features, whereas less than %" PRId64 " expected\n",
                    osExclusiveFilter.c_str(), nFeatureCountFiltered,
                    nFCClassic);
                bRet = false;
            }
            else if (bVerbose)
            {
                printf("INFO: Attribute filter exclusion with GetArrowStream "
                       "seems to work.\n");
            }
        }

        auto poGeom = poTargetFeature->GetGeometryRef();
        if (poGeom && !poGeom->IsEmpty())
        {
            OGREnvelope sEnvelope;
            poGeom->getEnvelope(&sEnvelope);

            OGREnvelope sLayerExtent;
            double epsilon = 10.0;
            if (LOG_ACTION(poLayer->TestCapability(OLCFastGetExtent)) &&
                LOG_ACTION(poLayer->GetExtent(&sLayerExtent)) == OGRERR_NONE &&
                sLayerExtent.MinX < sLayerExtent.MaxX &&
                sLayerExtent.MinY < sLayerExtent.MaxY)
            {
                epsilon = std::min(sLayerExtent.MaxX - sLayerExtent.MinX,
                                   sLayerExtent.MaxY - sLayerExtent.MinY) /
                          10.0;
            }

            /* -------------------------------------------------------------------- */
            /*      Construct inclusive filter.                                     */
            /* -------------------------------------------------------------------- */

            OGRLinearRing oRing;
            oRing.setPoint(0, sEnvelope.MinX - 2 * epsilon,
                           sEnvelope.MinY - 2 * epsilon);
            oRing.setPoint(1, sEnvelope.MinX - 2 * epsilon,
                           sEnvelope.MaxY + 1 * epsilon);
            oRing.setPoint(2, sEnvelope.MaxX + 1 * epsilon,
                           sEnvelope.MaxY + 1 * epsilon);
            oRing.setPoint(3, sEnvelope.MaxX + 1 * epsilon,
                           sEnvelope.MinY - 2 * epsilon);
            oRing.setPoint(4, sEnvelope.MinX - 2 * epsilon,
                           sEnvelope.MinY - 2 * epsilon);

            OGRPolygon oInclusiveFilter;
            oInclusiveFilter.addRing(&oRing);

            LOG_ACTION(poLayer->SetSpatialFilter(&oInclusiveFilter));
            const auto nFeatureCountFiltered = CountFeaturesUsingArrowStream(
                poLayer, poTargetFeature->GetFID(), -1, bRet);
            LOG_ACTION(poLayer->SetSpatialFilter(nullptr));
            if (nFeatureCountFiltered == 0)
            {
                printf("ERROR: GetArrowStream() with inclusive spatial filter "
                       "returned %" PRId64
                       " features, whereas at least 1 expected\n",
                       nFeatureCountFiltered);
                bRet = false;
            }
            else if (bVerbose)
            {
                printf("INFO: Spatial filter inclusion with GetArrowStream "
                       "seems to work.\n");
            }
        }
    }

    if (bRet && bVerbose)
        printf("INFO: TestLayerGetArrowStream passed.\n");

    return bRet;
}

/************************************************************************/
/*                            TestOGRLayer()                            */
/************************************************************************/

static int TestOGRLayer(GDALDataset *poDS, OGRLayer *poLayer, int bIsSQLLayer)

{
    int bRet = TRUE;

    // Check that pszDomain == nullptr doesn't crash
    poLayer->GetMetadata(nullptr);
    poLayer->GetMetadataItem("", nullptr);

    /* -------------------------------------------------------------------- */
    /*      Verify that there is no spatial filter in place by default.     */
    /* -------------------------------------------------------------------- */
    if (LOG_ACTION(poLayer->GetSpatialFilter()) != nullptr)
    {
        printf("WARN: Spatial filter in place by default on layer %s.\n",
               poLayer->GetName());
        LOG_ACTION(poLayer->SetSpatialFilter(nullptr));
    }

    /* -------------------------------------------------------------------- */
    /*      Basic tests.                                                   */
    /* -------------------------------------------------------------------- */
    bRet &= TestBasic(poDS, poLayer);

    /* -------------------------------------------------------------------- */
    /*      Test feature count accuracy.                                    */
    /* -------------------------------------------------------------------- */
    bRet &= TestOGRLayerFeatureCount(poDS, poLayer, bIsSQLLayer);

    /* -------------------------------------------------------------------- */
    /*      Test spatial filtering                                          */
    /* -------------------------------------------------------------------- */
    bRet &= TestSpatialFilter(poLayer);

    /* -------------------------------------------------------------------- */
    /*      Test attribute filtering                                        */
    /* -------------------------------------------------------------------- */
    bRet &= TestAttributeFilter(poDS, poLayer);

    /* -------------------------------------------------------------------- */
    /*      Test GetExtent()                                                */
    /* -------------------------------------------------------------------- */
    bRet &= TestGetExtent(poLayer);

    /* -------------------------------------------------------------------- */
    /*      Test GetArrowStream() interface                                 */
    /* -------------------------------------------------------------------- */
    bRet &= TestLayerGetArrowStream(poLayer);

    /* -------------------------------------------------------------------- */
    /*      Test random reading.                                            */
    /* -------------------------------------------------------------------- */
    bRet &= TestOGRLayerRandomRead(poLayer);

    /* -------------------------------------------------------------------- */
    /*      Test SetNextByIndex.                                            */
    /* -------------------------------------------------------------------- */
    bRet &= TestOGRLayerSetNextByIndex(poLayer);

    /* -------------------------------------------------------------------- */
    /*      Test delete feature.                                            */
    /* -------------------------------------------------------------------- */
    if (LOG_ACTION(poLayer->TestCapability(OLCDeleteFeature)))
    {
        bRet &= TestOGRLayerDeleteAndCreateFeature(poLayer);
    }

    /* -------------------------------------------------------------------- */
    /*      Test random writing.                                            */
    /* -------------------------------------------------------------------- */
    if (LOG_ACTION(poLayer->TestCapability(OLCRandomWrite)))
    {
        if (!poDS->GetDriver()->GetMetadataItem(GDAL_DCAP_UPDATE))
        {
            printf("ERROR: Driver %s does not declare GDAL_DCAP_UPDATE\n",
                   poDS->GetDriver()->GetDescription());
            bRet = false;
        }
        else
        {
            const char *pszItems =
                poDS->GetDriver()->GetMetadataItem(GDAL_DMD_UPDATE_ITEMS);
            if (!pszItems || !strstr(pszItems, "Features"))
            {
                printf("ERROR: Driver %s does not declare Features in "
                       "GDAL_DMD_UPDATE_ITEMS\n",
                       poDS->GetDriver()->GetDescription());
                bRet = false;
            }
        }

        bRet &= TestOGRLayerRandomWrite(poLayer);
    }

    /* -------------------------------------------------------------------- */
    /*      Test OLCIgnoreFields.                                           */
    /* -------------------------------------------------------------------- */
    if (LOG_ACTION(poLayer->TestCapability(OLCIgnoreFields)))
    {
        bRet &= TestOGRLayerIgnoreFields(poLayer);
    }

    /* -------------------------------------------------------------------- */
    /*      Test UTF-8 reporting                                            */
    /* -------------------------------------------------------------------- */
    bRet &= TestOGRLayerUTF8(poLayer);

    /* -------------------------------------------------------------------- */
    /*      Test TestTransactions()                                         */
    /* -------------------------------------------------------------------- */
    if (LOG_ACTION(poLayer->TestCapability(OLCSequentialWrite)))
    {
        bRet &= TestTransactions(poLayer);
    }

    /* -------------------------------------------------------------------- */
    /*      Test error conditions.                                          */
    /* -------------------------------------------------------------------- */
    bRet &= TestLayerErrorConditions(poLayer);

    /* -------------------------------------------------------------------- */
    /*      Test some SQL.                                                  */
    /* -------------------------------------------------------------------- */
    if (!bIsSQLLayer)
        bRet &= TestLayerSQL(poDS, poLayer);

    return bRet;
}

/************************************************************************/
/*                        TestInterleavedReading()                      */
/************************************************************************/

static int TestInterleavedReading(const char *pszDataSourceIn,
                                  char **papszLayersIn)
{
    int bRet = TRUE;
    GDALDataset *poDS2 = nullptr;
    OGRLayer *poLayer1 = nullptr;
    OGRLayer *poLayer2 = nullptr;
    OGRFeature *poFeature11_Ref = nullptr;
    OGRFeature *poFeature12_Ref = nullptr;
    OGRFeature *poFeature21_Ref = nullptr;
    OGRFeature *poFeature22_Ref = nullptr;
    OGRFeature *poFeature11 = nullptr;
    OGRFeature *poFeature12 = nullptr;
    OGRFeature *poFeature21 = nullptr;
    OGRFeature *poFeature22 = nullptr;

    /* Check that we have 2 layers with at least 2 features */
    GDALDataset *poDS = LOG_ACTION(static_cast<GDALDataset *>(GDALOpenEx(
        pszDataSourceIn, GDAL_OF_VECTOR, nullptr, papszOpenOptions, nullptr)));
    if (poDS == nullptr)
    {
        if (bVerbose)
        {
            printf("INFO: Skipping TestInterleavedReading(). "
                   "Cannot reopen datasource\n");
        }
        goto bye;
    }

    poLayer1 = LOG_ACTION(papszLayersIn ? poDS->GetLayerByName(papszLayersIn[0])
                                        : poDS->GetLayer(0));
    poLayer2 = LOG_ACTION(papszLayersIn ? poDS->GetLayerByName(papszLayersIn[1])
                                        : poDS->GetLayer(1));
    if (poLayer1 == nullptr || poLayer2 == nullptr ||
        LOG_ACTION(poLayer1->GetFeatureCount()) < 2 ||
        LOG_ACTION(poLayer2->GetFeatureCount()) < 2)
    {
        if (bVerbose)
        {
            printf("INFO: Skipping TestInterleavedReading(). "
                   "Test conditions are not met\n");
        }
        goto bye;
    }

    /* Test normal reading */
    LOG_ACTION(GDALClose(poDS));
    poDS = LOG_ACTION(static_cast<GDALDataset *>(GDALOpenEx(
        pszDataSourceIn, GDAL_OF_VECTOR, nullptr, papszOpenOptions, nullptr)));
    poDS2 = LOG_ACTION(static_cast<GDALDataset *>(GDALOpenEx(
        pszDataSourceIn, GDAL_OF_VECTOR, nullptr, papszOpenOptions, nullptr)));
    if (poDS == nullptr || poDS2 == nullptr)
    {
        if (bVerbose)
        {
            printf("INFO: Skipping TestInterleavedReading(). "
                   "Cannot reopen datasource\n");
        }
        goto bye;
    }

    poLayer1 = LOG_ACTION(papszLayersIn ? poDS->GetLayerByName(papszLayersIn[0])
                                        : poDS->GetLayer(0));
    poLayer2 = LOG_ACTION(papszLayersIn ? poDS->GetLayerByName(papszLayersIn[1])
                                        : poDS->GetLayer(1));
    if (poLayer1 == nullptr || poLayer2 == nullptr)
    {
        printf("ERROR: Skipping TestInterleavedReading(). "
               "Test conditions are not met\n");
        bRet = FALSE;
        goto bye;
    }

    poFeature11_Ref = LOG_ACTION(poLayer1->GetNextFeature());
    poFeature12_Ref = LOG_ACTION(poLayer1->GetNextFeature());
    poFeature21_Ref = LOG_ACTION(poLayer2->GetNextFeature());
    poFeature22_Ref = LOG_ACTION(poLayer2->GetNextFeature());
    if (poFeature11_Ref == nullptr || poFeature12_Ref == nullptr ||
        poFeature21_Ref == nullptr || poFeature22_Ref == nullptr)
    {
        printf("ERROR: TestInterleavedReading() failed: poFeature11_Ref=%p, "
               "poFeature12_Ref=%p, poFeature21_Ref=%p, poFeature22_Ref=%p\n",
               poFeature11_Ref, poFeature12_Ref, poFeature21_Ref,
               poFeature22_Ref);
        bRet = FALSE;
        goto bye;
    }

    /* Test interleaved reading */
    poLayer1 =
        LOG_ACTION(papszLayersIn ? poDS2->GetLayerByName(papszLayersIn[0])
                                 : poDS2->GetLayer(0));
    poLayer2 =
        LOG_ACTION(papszLayersIn ? poDS2->GetLayerByName(papszLayersIn[1])
                                 : poDS2->GetLayer(1));
    if (poLayer1 == nullptr || poLayer2 == nullptr)
    {
        printf("ERROR: Skipping TestInterleavedReading(). "
               "Test conditions are not met\n");
        bRet = FALSE;
        goto bye;
    }

    poFeature11 = LOG_ACTION(poLayer1->GetNextFeature());
    poFeature21 = LOG_ACTION(poLayer2->GetNextFeature());
    poFeature12 = LOG_ACTION(poLayer1->GetNextFeature());
    poFeature22 = LOG_ACTION(poLayer2->GetNextFeature());

    if (poFeature11 == nullptr || poFeature21 == nullptr ||
        poFeature12 == nullptr || poFeature22 == nullptr)
    {
        printf("ERROR: TestInterleavedReading() failed: poFeature11=%p, "
               "poFeature21=%p, poFeature12=%p, poFeature22=%p\n",
               poFeature11, poFeature21, poFeature12, poFeature22);
        bRet = FALSE;
        goto bye;
    }

    if (poFeature12->Equal(poFeature11))
    {
        printf("WARN: TestInterleavedReading() failed: poFeature12 == "
               "poFeature11. "
               "The datasource resets the layer reading when interleaved "
               "layer reading pattern is detected. Acceptable but could be "
               "improved\n");
        goto bye;
    }

    /* We cannot directly compare the feature as they don't share */
    /* the same (pointer) layer definition, so just compare FIDs */
    if (poFeature12_Ref->GetFID() != poFeature12->GetFID())
    {
        printf("ERROR: TestInterleavedReading() failed: "
               "poFeature12_Ref != poFeature12\n");
        poFeature12_Ref->DumpReadable(stdout, nullptr);
        poFeature12->DumpReadable(stdout, nullptr);
        bRet = FALSE;
        goto bye;
    }

    if (bVerbose)
    {
        printf("INFO: TestInterleavedReading() successful.\n");
    }

bye:
    DestroyFeatureAndNullify(poFeature11_Ref);
    DestroyFeatureAndNullify(poFeature12_Ref);
    DestroyFeatureAndNullify(poFeature21_Ref);
    DestroyFeatureAndNullify(poFeature22_Ref);
    DestroyFeatureAndNullify(poFeature11);
    DestroyFeatureAndNullify(poFeature21);
    DestroyFeatureAndNullify(poFeature12);
    DestroyFeatureAndNullify(poFeature22);
    if (poDS != nullptr)
        LOG_ACTION(GDALClose(poDS));
    if (poDS2 != nullptr)
        LOG_ACTION(GDALClose(poDS2));
    return bRet;
}

/************************************************************************/
/*                          TestDSErrorConditions()                     */
/************************************************************************/

static int TestDSErrorConditions(GDALDataset *poDS)
{
    int bRet = TRUE;
    OGRLayer *poLyr;

    CPLPushErrorHandler(CPLQuietErrorHandler);

    if (LOG_ACTION(poDS->TestCapability("fake_capability")))
    {
        printf("ERROR: TestCapability(\"fake_capability\") "
               "should have returned FALSE\n");
        bRet = FALSE;
        goto bye;
    }

    if (LOG_ACTION(poDS->GetLayer(-1)) != nullptr)
    {
        printf("ERROR: GetLayer(-1) should have returned NULL\n");
        bRet = FALSE;
        goto bye;
    }

    if (LOG_ACTION(poDS->GetLayer(poDS->GetLayerCount())) != nullptr)
    {
        printf("ERROR: GetLayer(poDS->GetLayerCount()) should have "
               "returned NULL\n");
        bRet = FALSE;
        goto bye;
    }

    if (LOG_ACTION(poDS->GetLayerByName("non_existing_layer")) != nullptr)
    {
        printf("ERROR: GetLayerByName(\"non_existing_layer\") should have "
               "returned NULL\n");
        bRet = FALSE;
        goto bye;
    }

    poLyr =
        LOG_ACTION(poDS->ExecuteSQL("a fake SQL command", nullptr, nullptr));
    if (poLyr != nullptr)
    {
        LOG_ACTION(poDS->ReleaseResultSet(poLyr));
        printf("ERROR: ExecuteSQL(\"a fake SQL command\") should have "
               "returned NULL\n");
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

static int TestVirtualIO(GDALDataset *poDS)
{
    int bRet = TRUE;

    if (STARTS_WITH(poDS->GetDescription(), "/vsimem/"))
        return TRUE;

    VSIStatBufL sStat;
    if (!(VSIStatL(poDS->GetDescription(), &sStat) == 0))
        return TRUE;

    // Don't try with ODBC (will avoid a useless error message in ogr_odbc.py)
    if (poDS->GetDriver() != nullptr &&
        EQUAL(poDS->GetDriver()->GetDescription(), "ODBC"))
    {
        return TRUE;
    }

    const CPLStringList aosFileList(LOG_ACTION(poDS->GetFileList()));
    CPLString osPath;
    int bAllPathIdentical = TRUE;
    for (const char *pszFilename : aosFileList)
    {
        if (pszFilename == aosFileList[0])
            osPath = CPLGetPathSafe(pszFilename);
        else if (osPath != CPLGetPathSafe(pszFilename))
        {
            bAllPathIdentical = FALSE;
            break;
        }
    }
    CPLString osVirtPath;
    if (bAllPathIdentical && aosFileList.size() > 1)
    {
        osVirtPath =
            CPLFormFilenameSafe("/vsimem", CPLGetFilename(osPath), nullptr);
        VSIMkdir(osVirtPath, 0666);
    }
    else
        osVirtPath = "/vsimem";

    for (const char *pszFilename : aosFileList)
    {
        const std::string osDestFile = CPLFormFilenameSafe(
            osVirtPath, CPLGetFilename(pszFilename), nullptr);
        /* CPLDebug("test_ogrsf", "Copying %s to %s", pszFilename, osDestFile.c_str());
         */
        CPLCopyFile(osDestFile.c_str(), pszFilename);
    }

    std::string osVirtFile;
    if (VSI_ISREG(sStat.st_mode))
        osVirtFile = CPLFormFilenameSafe(
            osVirtPath, CPLGetFilename(poDS->GetDescription()), nullptr);
    else
        osVirtFile = osVirtPath;
    CPLDebug("test_ogrsf", "Trying to open %s", osVirtFile.c_str());
    GDALDataset *poDS2 = LOG_ACTION(static_cast<GDALDataset *>(GDALOpenEx(
        osVirtFile.c_str(), GDAL_OF_VECTOR, nullptr, nullptr, nullptr)));
    if (poDS2 != nullptr)
    {
        if (poDS->GetDriver()->GetMetadataItem(GDAL_DCAP_VIRTUALIO) == nullptr)
        {
            printf("WARNING: %s driver apparently supports VirtualIO "
                   "but does not declare it.\n",
                   poDS->GetDriver()->GetDescription());
        }
        if (poDS2->GetLayerCount() != poDS->GetLayerCount())
        {
            printf("WARNING: /vsimem dataset reports %d layers where as base "
                   "dataset reports %d layers.\n",
                   poDS2->GetLayerCount(), poDS->GetLayerCount());
        }
        GDALClose(poDS2);

        if (bVerbose && bRet)
        {
            printf("INFO: TestVirtualIO successful.\n");
        }
    }
    else
    {
        if (poDS->GetDriver()->GetMetadataItem(GDAL_DCAP_VIRTUALIO) != nullptr)
        {
            printf("WARNING: %s driver declares supporting VirtualIO but "
                   "test with /vsimem does not work. It might be a sign that "
                   "GetFileList() is not properly implemented.\n",
                   poDS->GetDriver()->GetDescription());
        }
    }

    for (const char *pszFilename : aosFileList)
    {
        VSIUnlink(CPLFormFilenameSafe(osVirtPath, CPLGetFilename(pszFilename),
                                      nullptr)
                      .c_str());
    }

    return bRet;
}
