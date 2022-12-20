/******************************************************************************
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  Extension SQL functions used by both SQLite dialect and GPKG driver
 * Author:   Even Rouault, even dot rouault at spatialys.com
 *
 ******************************************************************************
 * Copyright (c) 2012-2022, Even Rouault <even dot rouault at spatialys.com>
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

/* WARNING: VERY IMPORTANT NOTE: This file MUST not be directly compiled as */
/* a standalone object. It must be included from ogrsqlitevirtualogr.cpp */
#ifndef COMPILATION_ALLOWED
#error See comment in file
#endif

#include "gdal_priv.h"
#include "ogr_geocoding.h"

#include "ogrsqliteregexp.cpp" /* yes the .cpp file, to make it work on Windows with load_extension('gdalXX.dll') */

#include <map>

namespace
{
class OGRSQLiteExtensionData
{
#ifdef DEBUG
    void *pDummy = nullptr; /* to track memory leaks */
#endif
    std::map<std::pair<int, int>, OGRCoordinateTransformation *>
        oCachedTransformsMap{};
    std::map<std::string, std::unique_ptr<GDALDataset>> oCachedDS{};

    void *hRegExpCache = nullptr;

    OGRGeocodingSessionH hGeocodingSession = nullptr;

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

    std::map<std::pair<int, int>, OGRCoordinateTransformation *>::iterator
        oIter = oCachedTransformsMap.begin();
    for (; oIter != oCachedTransformsMap.end(); ++oIter)
        delete oIter->second;

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
    std::map<std::pair<int, int>, OGRCoordinateTransformation *>::iterator
        oIter = oCachedTransformsMap.find(
            std::pair<int, int>(nSrcSRSId, nDstSRSId));
    if (oIter == oCachedTransformsMap.end())
    {
        OGRCoordinateTransformation *poCT = nullptr;
        OGRSpatialReference oSrcSRS, oDstSRS;
        oSrcSRS.SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);
        oDstSRS.SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);
        if (oSrcSRS.importFromEPSG(nSrcSRSId) == OGRERR_NONE &&
            oDstSRS.importFromEPSG(nDstSRSId) == OGRERR_NONE)
        {
            poCT = OGRCreateCoordinateTransformation(&oSrcSRS, &oDstSRS);
        }
        oCachedTransformsMap[std::pair<int, int>(nSrcSRSId, nDstSRSId)] = poCT;
        return poCT;
    }
    else
        return oIter->second;
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
    oCachedDS[pszDSName] = std::move(poDS);
    return oCachedDS[pszDSName].get();
}

}  // namespace

/************************************************************************/
/*                    OGRSQLITE_gdal_get_pixel_value()                  */
/************************************************************************/

static void OGRSQLITE_gdal_get_pixel_value(sqlite3_context *pContext,
                                           CPL_UNUSED int argc,
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

    if (sqlite3_value_type(argv[0]) != SQLITE_TEXT ||
        sqlite3_value_type(argv[1]) != SQLITE_INTEGER ||
        sqlite3_value_type(argv[2]) != SQLITE_TEXT ||
        (sqlite3_value_type(argv[3]) != SQLITE_INTEGER &&
         sqlite3_value_type(argv[3]) != SQLITE_FLOAT) ||
        (sqlite3_value_type(argv[4]) != SQLITE_INTEGER &&
         sqlite3_value_type(argv[4]) != SQLITE_FLOAT))
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Invalid arguments to gdal_get_pixel_value()");
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

    const int nBand = sqlite3_value_int(argv[1]);
    auto poBand = poDS->GetRasterBand(nBand);
    if (!poBand)
    {
        sqlite3_result_null(pContext);
        return;
    }

    const char *pszCoordType =
        reinterpret_cast<const char *>(sqlite3_value_text(argv[2]));
    int x, y;
    if (EQUAL(pszCoordType, "georef"))
    {
        const double X = sqlite3_value_double(argv[3]);
        const double Y = sqlite3_value_double(argv[4]);
        double adfGeoTransform[6];
        if (poDS->GetGeoTransform(adfGeoTransform) != CE_None)
        {
            sqlite3_result_null(pContext);
            return;
        }
        double adfInvGT[6];
        if (!GDALInvGeoTransform(adfGeoTransform, adfInvGT))
        {
            sqlite3_result_null(pContext);
            return;
        }
        x = static_cast<int>(adfInvGT[0] + X * adfInvGT[1] + Y * adfInvGT[2]);
        y = static_cast<int>(adfInvGT[3] + X * adfInvGT[4] + Y * adfInvGT[5]);
    }
    else if (EQUAL(pszCoordType, "pixel"))
    {
        x = sqlite3_value_int(argv[3]);
        y = sqlite3_value_int(argv[4]);
    }
    else
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Invalid value for 3rd argument of gdal_get_pixel_value(): "
                 "only 'georef' or 'pixel' are supported");
        sqlite3_result_null(pContext);
        return;
    }
    if (x < 0 || x >= poDS->GetRasterXSize() || y < 0 ||
        y >= poDS->GetRasterYSize())
    {
        sqlite3_result_null(pContext);
        return;
    }
    const auto eDT = poBand->GetRasterDataType();
    if (eDT != GDT_UInt64 && GDALDataTypeIsInteger(eDT))
    {
        int64_t nValue = 0;
        if (poBand->RasterIO(GF_Read, x, y, 1, 1, &nValue, 1, 1, GDT_Int64, 0,
                             0, nullptr) != CE_None)
        {
            sqlite3_result_null(pContext);
            return;
        }
        return sqlite3_result_int64(pContext, nValue);
    }
    else
    {
        double dfValue = 0;
        if (poBand->RasterIO(GF_Read, x, y, 1, 1, &dfValue, 1, 1, GDT_Float64,
                             0, 0, nullptr) != CE_None)
        {
            sqlite3_result_null(pContext);
            return;
        }
        return sqlite3_result_double(pContext, dfValue);
    }
}

/************************************************************************/
/*                 OGRSQLiteRegisterSQLFunctionsCommon()                */
/************************************************************************/

#ifndef SQLITE_DETERMINISTIC
#define SQLITE_DETERMINISTIC 0
#endif

static OGRSQLiteExtensionData *OGRSQLiteRegisterSQLFunctionsCommon(sqlite3 *hDB)
{
    OGRSQLiteExtensionData *pData = new OGRSQLiteExtensionData(hDB);

    sqlite3_create_function(hDB, "gdal_get_pixel_value", 5, SQLITE_UTF8, pData,
                            OGRSQLITE_gdal_get_pixel_value, nullptr, nullptr);

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
