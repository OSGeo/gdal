/******************************************************************************
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  Extension SQL functions used by both SQLite dialect and GPKG driver
 * Author:   Even Rouault, even dot rouault at spatialys.com
 *
 ******************************************************************************
 * Copyright (c) 2012-2022, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

/* WARNING: VERY IMPORTANT NOTE: This file MUST not be directly compiled as */
/* a standalone object. It must be included from ogrsqlitevirtualogr.cpp */
#ifndef COMPILATION_ALLOWED
#error See comment in file
#endif

#include "gdal_priv.h"
#include "ogr_geocoding.h"

#include "ogrsqliteregexp.cpp" /* yes the .cpp file, to make it work on Windows with load_extension('gdalXX.dll') */

#include <algorithm>
#include <cassert>
#include <cmath>
#include <map>

#include "ogr_swq.h"

namespace
{
class OGRSQLiteExtensionData
{
#ifdef DEBUG
    void *pDummy = nullptr; /* to track memory leaks */
#endif

    std::map<std::pair<int, int>, std::unique_ptr<OGRCoordinateTransformation>>
        // cppcheck-suppress unusedStructMember
        oCachedTransformsMap{};
    std::map<std::string, std::unique_ptr<GDALDataset>> oCachedDS{};

    void *hRegExpCache = nullptr;

    OGRGeocodingSessionH hGeocodingSession = nullptr;

    bool bCaseSensitiveLike = false;

    OGRSQLiteExtensionData(const OGRSQLiteExtensionData &) = delete;
    OGRSQLiteExtensionData &operator=(const OGRSQLiteExtensionData &) = delete;

  public:
    explicit OGRSQLiteExtensionData(sqlite3 *hDB);
    ~OGRSQLiteExtensionData();

#ifdef DEFINE_OGRSQLiteExtensionData_GetTransform
    OGRCoordinateTransformation *GetTransform(int nSrcSRSId, int nDstSRSId);
#endif

    GDALDataset *GetDataset(const char *pszDSName);

    OGRGeocodingSessionH GetGeocodingSession()
    {
        return hGeocodingSession;
    }

    void SetGeocodingSession(OGRGeocodingSessionH hGeocodingSessionIn)
    {
        hGeocodingSession = hGeocodingSessionIn;
    }

    void SetRegExpCache(void *hRegExpCacheIn)
    {
        hRegExpCache = hRegExpCacheIn;
    }

    void SetCaseSensitiveLike(bool b)
    {
        bCaseSensitiveLike = b;
    }

    bool GetCaseSensitiveLike() const
    {
        return bCaseSensitiveLike;
    }
};

/************************************************************************/
/*                     OGRSQLiteExtensionData()                         */
/************************************************************************/

OGRSQLiteExtensionData::OGRSQLiteExtensionData(CPL_UNUSED sqlite3 *hDB)
    :
#ifdef DEBUG
      pDummy(CPLMalloc(1)),
#endif
      hRegExpCache(nullptr), hGeocodingSession(nullptr)
{
}

/************************************************************************/
/*                       ~OGRSQLiteExtensionData()                      */
/************************************************************************/

OGRSQLiteExtensionData::~OGRSQLiteExtensionData()
{
#ifdef DEBUG
    CPLFree(pDummy);
#endif

    OGRSQLiteFreeRegExpCache(hRegExpCache);

    OGRGeocodeDestroySession(hGeocodingSession);
}

/************************************************************************/
/*                          GetTransform()                              */
/************************************************************************/

#ifdef DEFINE_OGRSQLiteExtensionData_GetTransform
OGRCoordinateTransformation *OGRSQLiteExtensionData::GetTransform(int nSrcSRSId,
                                                                  int nDstSRSId)
{
    auto oIter = oCachedTransformsMap.find(std::pair(nSrcSRSId, nDstSRSId));
    if (oIter == oCachedTransformsMap.end())
    {
        std::unique_ptr<OGRCoordinateTransformation> poCT;
        OGRSpatialReference oSrcSRS, oDstSRS;
        oSrcSRS.SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);
        oDstSRS.SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);
        if (oSrcSRS.importFromEPSG(nSrcSRSId) == OGRERR_NONE &&
            oDstSRS.importFromEPSG(nDstSRSId) == OGRERR_NONE)
        {
            poCT.reset(OGRCreateCoordinateTransformation(&oSrcSRS, &oDstSRS));
        }
        oIter = oCachedTransformsMap
                    .insert({std::pair(nSrcSRSId, nDstSRSId), std::move(poCT)})
                    .first;
    }
    return oIter->second.get();
}
#endif

/************************************************************************/
/*                          GetDataset()                                */
/************************************************************************/

GDALDataset *OGRSQLiteExtensionData::GetDataset(const char *pszDSName)
{
    auto oIter = oCachedDS.find(pszDSName);
    if (oIter != oCachedDS.end())
        return oIter->second.get();

    auto poDS = std::unique_ptr<GDALDataset>(
        GDALDataset::Open(pszDSName, GDAL_OF_RASTER | GDAL_OF_VERBOSE_ERROR));
    if (!poDS)
    {
        return nullptr;
    }
    oIter = oCachedDS.insert({pszDSName, std::move(poDS)}).first;
    return oIter->second.get();
}

}  // namespace

/************************************************************************/
/*                    OGRSQLITE_gdal_get_pixel_value()                  */
/************************************************************************/

static void OGRSQLITE_gdal_get_pixel_value(sqlite3_context *pContext, int argc,
                                           sqlite3_value **argv)
{
    if (!CPLTestBool(
            CPLGetConfigOption("OGR_SQLITE_ALLOW_EXTERNAL_ACCESS", "NO")))
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "gdal_get_pixel_value() SQL function not available "
                 "if OGR_SQLITE_ALLOW_EXTERNAL_ACCESS configuration option "
                 "is not set");
        sqlite3_result_null(pContext);
        return;
    }

    if (sqlite3_value_type(argv[0]) != SQLITE_TEXT)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Invalid arguments to gdal_get_layer_pixel_value()");
        sqlite3_result_null(pContext);
        return;
    }
    const char *pszDSName =
        reinterpret_cast<const char *>(sqlite3_value_text(argv[0]));

    OGRSQLiteExtensionData *poModule =
        static_cast<OGRSQLiteExtensionData *>(sqlite3_user_data(pContext));
    auto poDS = poModule->GetDataset(pszDSName);
    if (!poDS)
    {
        sqlite3_result_null(pContext);
        return;
    }

    OGRSQLite_gdal_get_pixel_value_common("gdal_get_layer_pixel_value",
                                          pContext, argc, argv, poDS);
}

/************************************************************************/
/*                             OGRSQLITE_LIKE()                         */
/************************************************************************/

static void OGRSQLITE_LIKE(sqlite3_context *pContext, int argc,
                           sqlite3_value **argv)
{
    OGRSQLiteExtensionData *poModule =
        static_cast<OGRSQLiteExtensionData *>(sqlite3_user_data(pContext));

    // A LIKE B is implemented as like(B, A)
    // A LIKE B ESCAPE C is implemented as like(B, A, C)
    const char *pattern =
        reinterpret_cast<const char *>(sqlite3_value_text(argv[0]));
    const char *input =
        reinterpret_cast<const char *>(sqlite3_value_text(argv[1]));
    if (!input || !pattern)
    {
        sqlite3_result_null(pContext);
        return;
    }
    char chEscape = '\\';
    if (argc == 3)
    {
        const char *escape =
            reinterpret_cast<const char *>(sqlite3_value_text(argv[2]));
        if (!escape || escape[1] != 0)
        {
            sqlite3_result_null(pContext);
            return;
        }
        chEscape = escape[0];
    }

    const bool insensitive = !poModule->GetCaseSensitiveLike();
    constexpr bool bUTF8Strings = true;
    sqlite3_result_int(pContext, swq_test_like(input, pattern, chEscape,
                                               insensitive, bUTF8Strings));
}

/************************************************************************/
/*                       OGRSQLITE_STDDEV_Step()                        */
/************************************************************************/

// Welford's online algorithm for variance:
// https://en.wikipedia.org/wiki/Algorithms_for_calculating_variance#Welford's_online_algorithm
struct OGRSQLITE_STDDEV_Context
{
    int64_t nValues;
    double dfMean;
    double dfM2;  // Accumulator for squared distance from the mean
};

static void OGRSQLITE_STDDEV_Step(sqlite3_context *pContext, int /* argc*/,
                                  sqlite3_value **argv)
{
    auto pAggCtxt =
        static_cast<OGRSQLITE_STDDEV_Context *>(sqlite3_aggregate_context(
            pContext, static_cast<int>(sizeof(OGRSQLITE_STDDEV_Context))));
    const auto eType = sqlite3_value_type(argv[0]);
    if (eType != SQLITE_INTEGER && eType != SQLITE_FLOAT)
        return;

    const double dfValue = sqlite3_value_double(argv[0]);
    pAggCtxt->nValues++;
    const double dfDelta = dfValue - pAggCtxt->dfMean;
    pAggCtxt->dfMean += dfDelta / pAggCtxt->nValues;
    const double dfDelta2 = dfValue - pAggCtxt->dfMean;
    pAggCtxt->dfM2 += dfDelta * dfDelta2;
}

/************************************************************************/
/*                    OGRSQLITE_STDDEV_POP_Finalize()                   */
/************************************************************************/

static void OGRSQLITE_STDDEV_POP_Finalize(sqlite3_context *pContext)
{
    auto pAggCtxt =
        static_cast<OGRSQLITE_STDDEV_Context *>(sqlite3_aggregate_context(
            pContext, static_cast<int>(sizeof(OGRSQLITE_STDDEV_Context))));
    if (pAggCtxt->nValues > 0)
    {
        sqlite3_result_double(pContext,
                              sqrt(pAggCtxt->dfM2 / pAggCtxt->nValues));
    }
}

/************************************************************************/
/*                    OGRSQLITE_STDDEV_SAMP_Finalize()                  */
/************************************************************************/

static void OGRSQLITE_STDDEV_SAMP_Finalize(sqlite3_context *pContext)
{
    auto pAggCtxt =
        static_cast<OGRSQLITE_STDDEV_Context *>(sqlite3_aggregate_context(
            pContext, static_cast<int>(sizeof(OGRSQLITE_STDDEV_Context))));
    if (pAggCtxt->nValues > 1)
    {
        sqlite3_result_double(pContext,
                              sqrt(pAggCtxt->dfM2 / (pAggCtxt->nValues - 1)));
    }
}

/************************************************************************/
/*                     OGRSQLITE_Percentile_Step()                      */
/************************************************************************/

// Percentile related code inspired from https://sqlite.org/src/file/ext/misc/percentile.c
// of https://www.sqlite.org/draft/percentile.html

// Constant added to Percentile::rPct, since rPct is initialized to 0 when unset.
constexpr double PERCENT_ADD_CONSTANT = 1;

namespace
{
struct Percentile
{
    double rPct; /* PERCENT_ADD_CONSTANT more than the value for P */
    std::vector<double> *values; /* Array of Y values */
};
}  // namespace

/*
** The "step" function for percentile(Y,P) is called once for each
** input row.
*/
static void OGRSQLITE_Percentile_Step(sqlite3_context *pCtx, int argc,
                                      sqlite3_value **argv)
{
    assert(argc == 2 || argc == 1);

    double rPct;

    if (argc == 1)
    {
        /* Requirement 13:  median(Y) is the same as percentile(Y,50). */
        rPct = 50.0;
    }
    else if (sqlite3_user_data(pCtx) == nullptr)
    {
        /* Requirement 3:  P must be a number between 0 and 100 */
        const int eType = sqlite3_value_numeric_type(argv[1]);
        rPct = sqlite3_value_double(argv[1]);
        if ((eType != SQLITE_INTEGER && eType != SQLITE_FLOAT) || rPct < 0.0 ||
            rPct > 100.0)
        {
            sqlite3_result_error(pCtx,
                                 "2nd argument to percentile() is not "
                                 "a number between 0.0 and 100.0",
                                 -1);
            return;
        }
    }
    else
    {
        /* Requirement 3:  P must be a number between 0 and 1 */
        const int eType = sqlite3_value_numeric_type(argv[1]);
        rPct = sqlite3_value_double(argv[1]);
        if ((eType != SQLITE_INTEGER && eType != SQLITE_FLOAT) || rPct < 0.0 ||
            rPct > 1.0)
        {
            sqlite3_result_error(pCtx,
                                 "2nd argument to percentile_cont() is not "
                                 "a number between 0.0 and 1.0",
                                 -1);
            return;
        }
        rPct *= 100.0;
    }

    /* Allocate the session context. */
    auto p = static_cast<Percentile *>(
        sqlite3_aggregate_context(pCtx, sizeof(Percentile)));
    if (!p)
        return;

    /* Remember the P value.  Throw an error if the P value is different
  ** from any prior row, per Requirement (2). */
    if (p->rPct == 0.0)
    {
        p->rPct = rPct + PERCENT_ADD_CONSTANT;
    }
    else if (p->rPct != rPct + PERCENT_ADD_CONSTANT)
    {
        sqlite3_result_error(pCtx,
                             "2nd argument to percentile() is not the "
                             "same for all input rows",
                             -1);
        return;
    }

    /* Ignore rows for which the value is NULL */
    const int eType = sqlite3_value_type(argv[0]);
    if (eType == SQLITE_NULL)
        return;

    /* If not NULL, then Y must be numeric.  Otherwise throw an error.
  ** Requirement 4 */
    if (eType != SQLITE_INTEGER && eType != SQLITE_FLOAT)
    {
        sqlite3_result_error(pCtx,
                             "1st argument to percentile() is not "
                             "numeric",
                             -1);
        return;
    }

    /* Ignore rows for which the value is  NaN */
    const double v = sqlite3_value_double(argv[0]);
    if (std::isnan(v))
    {
        return;
    }

    if (!p->values)
        p->values = new std::vector<double>();
    try
    {
        p->values->push_back(v);
    }
    catch (const std::exception &)
    {
        delete p->values;
        memset(p, 0, sizeof(*p));
        sqlite3_result_error_nomem(pCtx);
        return;
    }
}

/************************************************************************/
/*                   OGRSQLITE_Percentile_Finalize()                    */
/************************************************************************/

/*
** Called to compute the final output of percentile() and to clean
** up all allocated memory.
*/
static void OGRSQLITE_Percentile_Finalize(sqlite3_context *pCtx)
{
    auto p = static_cast<Percentile *>(sqlite3_aggregate_context(pCtx, 0));
    if (!p)
        return;
    if (!p->values)
        return;
    if (!p->values->empty())
    {
        std::sort(p->values->begin(), p->values->end());
        const double ix = (p->rPct - PERCENT_ADD_CONSTANT) *
                          static_cast<double>(p->values->size() - 1) * 0.01;
        const size_t i1 = static_cast<size_t>(ix);
        const size_t i2 =
            ix == static_cast<double>(i1) || i1 == p->values->size() - 1
                ? i1
                : i1 + 1;
        const double v1 = (*p->values)[i1];
        const double v2 = (*p->values)[i2];
        const double vx = v1 + (v2 - v1) * static_cast<double>(ix - i1);
        sqlite3_result_double(pCtx, vx);
    }
    delete p->values;
    memset(p, 0, sizeof(*p));
}

/************************************************************************/
/*                         OGRSQLITE_Mode_Step()                        */
/************************************************************************/

namespace
{
struct Mode
{
    std::map<double, uint64_t> *numericValues;
    std::map<std::string, uint64_t> *stringValues;
    double mostFrequentNumValue;
    std::string *mostFrequentStr;
    uint64_t mostFrequentValueCount;
    bool mostFrequentValueIsStr;
};
}  // namespace

static void OGRSQLITE_Mode_Step(sqlite3_context *pCtx, int /*argc*/,
                                sqlite3_value **argv)
{
    const int eType = sqlite3_value_type(argv[0]);
    if (eType == SQLITE_NULL)
        return;

    if (eType == SQLITE_BLOB)
    {
        sqlite3_result_error(pCtx, "BLOB argument not supported for mode()",
                             -1);
        return;
    }

    /* Allocate the session context. */
    auto p = static_cast<Mode *>(sqlite3_aggregate_context(pCtx, sizeof(Mode)));
    if (!p)
        return;

    try
    {
        if (eType == SQLITE_TEXT)
        {
            const char *pszStr =
                reinterpret_cast<const char *>(sqlite3_value_text(argv[0]));
            if (!p->stringValues)
            {
                p->stringValues = new std::map<std::string, uint64_t>();
                p->mostFrequentStr = new std::string();
            }
            const uint64_t count = ++(*p->stringValues)[pszStr];
            if (count > p->mostFrequentValueCount)
            {
                p->mostFrequentValueCount = count;
                p->mostFrequentValueIsStr = true;
                *(p->mostFrequentStr) = pszStr;
            }
        }
        else
        {
            const double v = sqlite3_value_double(argv[0]);
            if (std::isnan(v))
                return;
            if (!p->numericValues)
                p->numericValues = new std::map<double, uint64_t>();
            const uint64_t count = ++(*p->numericValues)[v];
            if (count > p->mostFrequentValueCount)
            {
                p->mostFrequentValueCount = count;
                p->mostFrequentValueIsStr = false;
                p->mostFrequentNumValue = v;
            }
        }
    }
    catch (const std::exception &)
    {
        delete p->stringValues;
        delete p->numericValues;
        delete p->mostFrequentStr;
        memset(p, 0, sizeof(*p));
        sqlite3_result_error_nomem(pCtx);
        return;
    }
}

/************************************************************************/
/*                       OGRSQLITE_Mode_Finalize()                      */
/************************************************************************/

static void OGRSQLITE_Mode_Finalize(sqlite3_context *pCtx)
{
    auto p = static_cast<Mode *>(sqlite3_aggregate_context(pCtx, 0));
    if (!p)
        return;

    if (p->mostFrequentValueCount)
    {
        if (p->mostFrequentValueIsStr)
        {
            sqlite3_result_text(pCtx, p->mostFrequentStr->c_str(), -1,
                                SQLITE_TRANSIENT);
        }
        else
        {
            sqlite3_result_double(pCtx, p->mostFrequentNumValue);
        }
    }

    delete p->stringValues;
    delete p->numericValues;
    delete p->mostFrequentStr;
    memset(p, 0, sizeof(*p));
}

/************************************************************************/
/*                 OGRSQLiteRegisterSQLFunctionsCommon()                */
/************************************************************************/

#ifndef SQLITE_DETERMINISTIC
#define SQLITE_DETERMINISTIC 0
#endif

#ifndef SQLITE_INNOCUOUS
#define SQLITE_INNOCUOUS 0
#endif

#define UTF8_INNOCUOUS (SQLITE_UTF8 | SQLITE_DETERMINISTIC | SQLITE_INNOCUOUS)

static OGRSQLiteExtensionData *OGRSQLiteRegisterSQLFunctionsCommon(sqlite3 *hDB)
{
    OGRSQLiteExtensionData *pData = new OGRSQLiteExtensionData(hDB);

    sqlite3_create_function(hDB, "gdal_get_pixel_value", 5, SQLITE_UTF8, pData,
                            OGRSQLITE_gdal_get_pixel_value, nullptr, nullptr);
    sqlite3_create_function(hDB, "gdal_get_pixel_value", 6, SQLITE_UTF8, pData,
                            OGRSQLITE_gdal_get_pixel_value, nullptr, nullptr);

    if (CPLTestBool(CPLGetConfigOption("OGR_SQLITE_USE_CUSTOM_LIKE", "YES")))
    {
        sqlite3_create_function(hDB, "LIKE", 2, UTF8_INNOCUOUS, pData,
                                OGRSQLITE_LIKE, nullptr, nullptr);
        sqlite3_create_function(hDB, "LIKE", 3, UTF8_INNOCUOUS, pData,
                                OGRSQLITE_LIKE, nullptr, nullptr);
    }

    sqlite3_create_function(hDB, "STDDEV_POP", 1, UTF8_INNOCUOUS, nullptr,
                            nullptr, OGRSQLITE_STDDEV_Step,
                            OGRSQLITE_STDDEV_POP_Finalize);

    sqlite3_create_function(hDB, "STDDEV_SAMP", 1, UTF8_INNOCUOUS, nullptr,
                            nullptr, OGRSQLITE_STDDEV_Step,
                            OGRSQLITE_STDDEV_SAMP_Finalize);

    sqlite3_create_function(hDB, "median", 1, UTF8_INNOCUOUS, nullptr, nullptr,
                            OGRSQLITE_Percentile_Step,
                            OGRSQLITE_Percentile_Finalize);

    sqlite3_create_function(hDB, "percentile", 2, UTF8_INNOCUOUS, nullptr,
                            nullptr, OGRSQLITE_Percentile_Step,
                            OGRSQLITE_Percentile_Finalize);

    sqlite3_create_function(
        hDB, "percentile_cont", 2, UTF8_INNOCUOUS,
        const_cast<char *>("percentile_cont"),  // any non-null ptr
        nullptr, OGRSQLITE_Percentile_Step, OGRSQLITE_Percentile_Finalize);

    sqlite3_create_function(hDB, "mode", 1, UTF8_INNOCUOUS, nullptr, nullptr,
                            OGRSQLITE_Mode_Step, OGRSQLITE_Mode_Finalize);

    pData->SetRegExpCache(OGRSQLiteRegisterRegExpFunction(hDB));

    return pData;
}

/************************************************************************/
/*                   OGRSQLiteUnregisterSQLFunctions()                  */
/************************************************************************/

static void OGRSQLiteUnregisterSQLFunctions(void *hHandle)
{
    OGRSQLiteExtensionData *pData =
        static_cast<OGRSQLiteExtensionData *>(hHandle);
    delete pData;
}

#ifdef DEFINE_OGRSQLiteSQLFunctionsSetCaseSensitiveLike
/************************************************************************/
/*                OGRSQLiteSQLFunctionsSetCaseSensitiveLike()           */
/************************************************************************/

static void OGRSQLiteSQLFunctionsSetCaseSensitiveLike(void *hHandle, bool b)
{
    OGRSQLiteExtensionData *pData =
        static_cast<OGRSQLiteExtensionData *>(hHandle);
    pData->SetCaseSensitiveLike(b);
}
#endif
