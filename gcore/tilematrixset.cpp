/******************************************************************************
 *
 * Project:  GDAL
 * Purpose:  Class to handle TileMatrixSet
 * Author:   Even Rouault, <even dot rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2020, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include "cpl_json.h"
#include "ogr_spatialref.h"

#include <algorithm>
#include <cmath>
#include <cfloat>
#include <limits>

#include "tilematrixset.hpp"

//! @cond Doxygen_Suppress

namespace gdal
{

/************************************************************************/
/*                   listPredefinedTileMatrixSets()                     */
/************************************************************************/

std::vector<std::string> TileMatrixSet::listPredefinedTileMatrixSets()
{
    std::vector<std::string> l{"GoogleMapsCompatible", "WorldCRS84Quad",
                               "WorldMercatorWGS84Quad", "GoogleCRS84Quad",
                               "PseudoTMS_GlobalMercator"};
    const char *pszSomeFile = CPLFindFile("gdal", "tms_NZTM2000.json");
    if (pszSomeFile)
    {
        std::set<std::string> set;
        CPLStringList aosList(
            VSIReadDir(CPLGetDirnameSafe(pszSomeFile).c_str()));
        for (int i = 0; i < aosList.size(); i++)
        {
            const size_t nLen = strlen(aosList[i]);
            if (nLen > strlen("tms_") + strlen(".json") &&
                STARTS_WITH(aosList[i], "tms_") &&
                EQUAL(aosList[i] + nLen - strlen(".json"), ".json"))
            {
                std::string id(aosList[i] + strlen("tms_"),
                               nLen - (strlen("tms_") + strlen(".json")));
                set.insert(id);
            }
        }
        for (const std::string &id : set)
            l.push_back(id);
    }
    return l;
}

/************************************************************************/
/*                              parse()                                 */
/************************************************************************/

std::unique_ptr<TileMatrixSet> TileMatrixSet::parse(const char *fileOrDef)
{
    CPLJSONDocument oDoc;
    std::unique_ptr<TileMatrixSet> poTMS(new TileMatrixSet());

    constexpr double HALF_CIRCUMFERENCE = 6378137 * M_PI;

    if (EQUAL(fileOrDef, "GoogleMapsCompatible") ||
        EQUAL(fileOrDef, "WebMercatorQuad") ||
        EQUAL(
            fileOrDef,
            "http://www.opengis.net/def/tilematrixset/OGC/1.0/WebMercatorQuad"))
    {
        /* See http://portal.opengeospatial.org/files/?artifact_id=35326
         * (WMTS 1.0), Annex E.4 */
        // or https://docs.ogc.org/is/17-083r4/17-083r4.html#toc49
        poTMS->mTitle = "GoogleMapsCompatible";
        poTMS->mIdentifier = "GoogleMapsCompatible";
        poTMS->mCrs = "http://www.opengis.net/def/crs/EPSG/0/3857";
        poTMS->mBbox.mCrs = poTMS->mCrs;
        poTMS->mBbox.mLowerCornerX = -HALF_CIRCUMFERENCE;
        poTMS->mBbox.mLowerCornerY = -HALF_CIRCUMFERENCE;
        poTMS->mBbox.mUpperCornerX = HALF_CIRCUMFERENCE;
        poTMS->mBbox.mUpperCornerY = HALF_CIRCUMFERENCE;
        poTMS->mWellKnownScaleSet =
            "http://www.opengis.net/def/wkss/OGC/1.0/GoogleMapsCompatible";
        for (int i = 0; i <= 30; i++)
        {
            TileMatrix tm;
            tm.mId = CPLSPrintf("%d", i);
            tm.mResX = 2 * HALF_CIRCUMFERENCE / 256 / (1 << i);
            tm.mResY = tm.mResX;
            tm.mScaleDenominator = tm.mResX / 0.28e-3;
            tm.mTopLeftX = -HALF_CIRCUMFERENCE;
            tm.mTopLeftY = HALF_CIRCUMFERENCE;
            tm.mTileWidth = 256;
            tm.mTileHeight = 256;
            tm.mMatrixWidth = 1 << i;
            tm.mMatrixHeight = 1 << i;
            poTMS->mTileMatrixList.emplace_back(std::move(tm));
        }
        return poTMS;
    }

    if (EQUAL(fileOrDef, "WorldMercatorWGS84Quad") ||
        EQUAL(fileOrDef, "http://www.opengis.net/def/tilematrixset/OGC/1.0/"
                         "WorldMercatorWGS84Quad"))
    {
        // See https://docs.ogc.org/is/17-083r4/17-083r4.html#toc51
        poTMS->mTitle = "WorldMercatorWGS84Quad";
        poTMS->mIdentifier = "WorldMercatorWGS84Quad";
        poTMS->mCrs = "http://www.opengis.net/def/crs/EPSG/0/3395";
        poTMS->mBbox.mCrs = poTMS->mCrs;
        poTMS->mBbox.mLowerCornerX = -HALF_CIRCUMFERENCE;
        poTMS->mBbox.mLowerCornerY = -HALF_CIRCUMFERENCE;
        poTMS->mBbox.mUpperCornerX = HALF_CIRCUMFERENCE;
        poTMS->mBbox.mUpperCornerY = HALF_CIRCUMFERENCE;
        poTMS->mWellKnownScaleSet =
            "http://www.opengis.net/def/wkss/OGC/1.0/WorldMercatorWGS84Quad";
        for (int i = 0; i <= 30; i++)
        {
            TileMatrix tm;
            tm.mId = CPLSPrintf("%d", i);
            tm.mResX = 2 * HALF_CIRCUMFERENCE / 256 / (1 << i);
            tm.mResY = tm.mResX;
            tm.mScaleDenominator = tm.mResX / 0.28e-3;
            tm.mTopLeftX = -HALF_CIRCUMFERENCE;
            tm.mTopLeftY = HALF_CIRCUMFERENCE;
            tm.mTileWidth = 256;
            tm.mTileHeight = 256;
            tm.mMatrixWidth = 1 << i;
            tm.mMatrixHeight = 1 << i;
            poTMS->mTileMatrixList.emplace_back(std::move(tm));
        }
        return poTMS;
    }

    if (EQUAL(fileOrDef, "PseudoTMS_GlobalMercator"))
    {
        /* See global-mercator at
           http://wiki.osgeo.org/wiki/Tile_Map_Service_Specification */
        poTMS->mTitle = "PseudoTMS_GlobalMercator";
        poTMS->mIdentifier = "PseudoTMS_GlobalMercator";
        poTMS->mCrs = "http://www.opengis.net/def/crs/EPSG/0/3857";
        poTMS->mBbox.mCrs = poTMS->mCrs;
        poTMS->mBbox.mLowerCornerX = -HALF_CIRCUMFERENCE;
        poTMS->mBbox.mLowerCornerY = -HALF_CIRCUMFERENCE;
        poTMS->mBbox.mUpperCornerX = HALF_CIRCUMFERENCE;
        poTMS->mBbox.mUpperCornerY = HALF_CIRCUMFERENCE;
        for (int i = 0; i <= 29; i++)
        {
            TileMatrix tm;
            tm.mId = CPLSPrintf("%d", i);
            tm.mResX = HALF_CIRCUMFERENCE / 256 / (1 << i);
            tm.mResY = tm.mResX;
            tm.mScaleDenominator = tm.mResX / 0.28e-3;
            tm.mTopLeftX = -HALF_CIRCUMFERENCE;
            tm.mTopLeftY = HALF_CIRCUMFERENCE;
            tm.mTileWidth = 256;
            tm.mTileHeight = 256;
            tm.mMatrixWidth = 2 * (1 << i);
            tm.mMatrixHeight = 2 * (1 << i);
            poTMS->mTileMatrixList.emplace_back(std::move(tm));
        }
        return poTMS;
    }

    if (EQUAL(fileOrDef, "InspireCRS84Quad") ||
        EQUAL(fileOrDef, "PseudoTMS_GlobalGeodetic") ||
        EQUAL(fileOrDef, "WorldCRS84Quad") ||
        EQUAL(
            fileOrDef,
            "http://www.opengis.net/def/tilematrixset/OGC/1.0/WorldCRS84Quad"))
    {
        /* See InspireCRS84Quad at
         * http://inspire.ec.europa.eu/documents/Network_Services/TechnicalGuidance_ViewServices_v3.0.pdf
         */
        /* This is exactly the same as PseudoTMS_GlobalGeodetic */
        /* See global-geodetic at
         * http://wiki.osgeo.org/wiki/Tile_Map_Service_Specification */
        // See also http://docs.opengeospatial.org/is/17-083r2/17-083r2.html#76
        poTMS->mTitle = "WorldCRS84Quad";
        poTMS->mIdentifier = "WorldCRS84Quad";
        poTMS->mCrs = "http://www.opengis.net/def/crs/OGC/1.3/CRS84";
        poTMS->mBbox.mCrs = poTMS->mCrs;
        poTMS->mBbox.mLowerCornerX = -180;
        poTMS->mBbox.mLowerCornerY = -90;
        poTMS->mBbox.mUpperCornerX = 180;
        poTMS->mBbox.mUpperCornerY = 90;
        poTMS->mWellKnownScaleSet =
            "http://www.opengis.net/def/wkss/OGC/1.0/GoogleCRS84Quad";
        // Limit to zoom level 29, because at that zoom level nMatrixWidth = 2 * (1 << 29) = 1073741824
        // and at 30 it would overflow int32.
        for (int i = 0; i <= 29; i++)
        {
            TileMatrix tm;
            tm.mId = CPLSPrintf("%d", i);
            tm.mResX = 180. / 256 / (1 << i);
            tm.mResY = tm.mResX;
            tm.mScaleDenominator =
                tm.mResX * (HALF_CIRCUMFERENCE / 180) / 0.28e-3;
            tm.mTopLeftX = -180;
            tm.mTopLeftY = 90;
            tm.mTileWidth = 256;
            tm.mTileHeight = 256;
            tm.mMatrixWidth = 2 * (1 << i);
            tm.mMatrixHeight = 1 << i;
            poTMS->mTileMatrixList.emplace_back(std::move(tm));
        }
        return poTMS;
    }

    if (EQUAL(fileOrDef, "GoogleCRS84Quad") ||
        EQUAL(fileOrDef,
              "http://www.opengis.net/def/wkss/OGC/1.0/GoogleCRS84Quad"))
    {
        /* See http://portal.opengeospatial.org/files/?artifact_id=35326 (WMTS 1.0),
               Annex E.3 */
        poTMS->mTitle = "GoogleCRS84Quad";
        poTMS->mIdentifier = "GoogleCRS84Quad";
        poTMS->mCrs = "http://www.opengis.net/def/crs/OGC/1.3/CRS84";
        poTMS->mBbox.mCrs = poTMS->mCrs;
        poTMS->mBbox.mLowerCornerX = -180;
        poTMS->mBbox.mLowerCornerY = -90;
        poTMS->mBbox.mUpperCornerX = 180;
        poTMS->mBbox.mUpperCornerY = 90;
        poTMS->mWellKnownScaleSet =
            "http://www.opengis.net/def/wkss/OGC/1.0/GoogleCRS84Quad";
        for (int i = 0; i <= 30; i++)
        {
            TileMatrix tm;
            tm.mId = CPLSPrintf("%d", i);
            tm.mResX = 360. / 256 / (1 << i);
            tm.mResY = tm.mResX;
            tm.mScaleDenominator =
                tm.mResX * (HALF_CIRCUMFERENCE / 180) / 0.28e-3;
            tm.mTopLeftX = -180;
            tm.mTopLeftY = 180;
            tm.mTileWidth = 256;
            tm.mTileHeight = 256;
            tm.mMatrixWidth = 1 << i;
            tm.mMatrixHeight = 1 << i;
            poTMS->mTileMatrixList.emplace_back(std::move(tm));
        }
        return poTMS;
    }

    bool loadOk = false;
    if (  // TMS 2.0 spec
        (strstr(fileOrDef, "\"crs\"") &&
         strstr(fileOrDef, "\"tileMatrices\"")) ||
        // TMS 1.0 spec
        (strstr(fileOrDef, "\"type\"") &&
         strstr(fileOrDef, "\"TileMatrixSetType\"")) ||
        (strstr(fileOrDef, "\"identifier\"") &&
         strstr(fileOrDef, "\"boundingBox\"") &&
         strstr(fileOrDef, "\"tileMatrix\"")))
    {
        loadOk = oDoc.LoadMemory(fileOrDef);
    }
    else if (STARTS_WITH_CI(fileOrDef, "http://") ||
             STARTS_WITH_CI(fileOrDef, "https://"))
    {
        const char *const apszOptions[] = {"MAX_FILE_SIZE=1000000", nullptr};
        loadOk = oDoc.LoadUrl(fileOrDef, apszOptions);
    }
    else
    {
        VSIStatBufL sStat;
        if (VSIStatL(fileOrDef, &sStat) == 0)
        {
            loadOk = oDoc.Load(fileOrDef);
        }
        else
        {
            const char *pszFilename = CPLFindFile(
                "gdal", (std::string("tms_") + fileOrDef + ".json").c_str());
            if (pszFilename)
            {
                loadOk = oDoc.Load(pszFilename);
            }
            else
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "Invalid tiling matrix set name");
            }
        }
    }
    if (!loadOk)
    {
        return nullptr;
    }

    auto oRoot = oDoc.GetRoot();
    const bool bIsTMSv2 =
        oRoot.GetObj("crs").IsValid() && oRoot.GetObj("tileMatrices").IsValid();

    if (!bIsTMSv2 && oRoot.GetString("type") != "TileMatrixSetType" &&
        !oRoot.GetObj("tileMatrix").IsValid())
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Expected type = TileMatrixSetType");
        return nullptr;
    }

    const auto GetCRS = [](const CPLJSONObject &j)
    {
        if (j.IsValid())
        {
            if (j.GetType() == CPLJSONObject::Type::String)
                return j.ToString();

            else if (j.GetType() == CPLJSONObject::Type::Object)
            {
                const std::string osURI = j.GetString("uri");
                if (!osURI.empty())
                    return osURI;

                // Quite a bit of confusion around wkt.
                // See https://github.com/opengeospatial/ogcapi-tiles/issues/170
                const auto jWKT = j.GetObj("wkt");
                if (jWKT.GetType() == CPLJSONObject::Type::String)
                {
                    const std::string osWKT = jWKT.ToString();
                    if (!osWKT.empty())
                        return osWKT;
                }
                else if (jWKT.GetType() == CPLJSONObject::Type::Object)
                {
                    const std::string osWKT = jWKT.ToString();
                    if (!osWKT.empty())
                        return osWKT;
                }
            }
        }
        return std::string();
    };

    poTMS->mIdentifier = oRoot.GetString(bIsTMSv2 ? "id" : "identifier");
    poTMS->mTitle = oRoot.GetString("title");
    poTMS->mAbstract = oRoot.GetString(bIsTMSv2 ? "description" : "abstract");
    const auto oBbox = oRoot.GetObj("boundingBox");
    if (oBbox.IsValid())
    {
        poTMS->mBbox.mCrs = GetCRS(oBbox.GetObj("crs"));
        const auto oLowerCorner = oBbox.GetArray("lowerCorner");
        if (oLowerCorner.IsValid() && oLowerCorner.Size() == 2)
        {
            poTMS->mBbox.mLowerCornerX = oLowerCorner[0].ToDouble(NaN);
            poTMS->mBbox.mLowerCornerY = oLowerCorner[1].ToDouble(NaN);
        }
        const auto oUpperCorner = oBbox.GetArray("upperCorner");
        if (oUpperCorner.IsValid() && oUpperCorner.Size() == 2)
        {
            poTMS->mBbox.mUpperCornerX = oUpperCorner[0].ToDouble(NaN);
            poTMS->mBbox.mUpperCornerY = oUpperCorner[1].ToDouble(NaN);
        }
    }
    poTMS->mCrs = GetCRS(oRoot.GetObj(bIsTMSv2 ? "crs" : "supportedCRS"));
    poTMS->mWellKnownScaleSet = oRoot.GetString("wellKnownScaleSet");

    OGRSpatialReference oCrs;
    if (oCrs.SetFromUserInput(
            poTMS->mCrs.c_str(),
            OGRSpatialReference::SET_FROM_USER_INPUT_LIMITATIONS) !=
        OGRERR_NONE)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Cannot parse CRS %s",
                 poTMS->mCrs.c_str());
        return nullptr;
    }
    double dfMetersPerUnit = 1.0;
    if (oCrs.IsProjected())
    {
        dfMetersPerUnit = oCrs.GetLinearUnits();
    }
    else if (oCrs.IsGeographic())
    {
        dfMetersPerUnit = oCrs.GetSemiMajor() * M_PI / 180;
    }

    const auto oTileMatrices =
        oRoot.GetArray(bIsTMSv2 ? "tileMatrices" : "tileMatrix");
    if (oTileMatrices.IsValid())
    {
        double dfLastScaleDenominator = std::numeric_limits<double>::max();
        for (const auto &oTM : oTileMatrices)
        {
            TileMatrix tm;
            tm.mId = oTM.GetString(bIsTMSv2 ? "id" : "identifier");
            tm.mScaleDenominator = oTM.GetDouble("scaleDenominator");
            if (tm.mScaleDenominator >= dfLastScaleDenominator ||
                tm.mScaleDenominator <= 0)
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "Invalid scale denominator or non-decreasing series "
                         "of scale denominators");
                return nullptr;
            }
            dfLastScaleDenominator = tm.mScaleDenominator;
            // See note g of Table 2 of
            // http://docs.opengeospatial.org/is/17-083r2/17-083r2.html
            tm.mResX = tm.mScaleDenominator * 0.28e-3 / dfMetersPerUnit;
            tm.mResY = tm.mResX;
            if (bIsTMSv2)
            {
                const auto osCornerOfOrigin = oTM.GetString("cornerOfOrigin");
                if (!osCornerOfOrigin.empty() && osCornerOfOrigin != "topLeft")
                {
                    CPLError(CE_Warning, CPLE_AppDefined,
                             "cornerOfOrigin = %s not supported",
                             osCornerOfOrigin.c_str());
                }
            }
            const auto oTopLeftCorner =
                oTM.GetArray(bIsTMSv2 ? "pointOfOrigin" : "topLeftCorner");
            if (oTopLeftCorner.IsValid() && oTopLeftCorner.Size() == 2)
            {
                tm.mTopLeftX = oTopLeftCorner[0].ToDouble(NaN);
                tm.mTopLeftY = oTopLeftCorner[1].ToDouble(NaN);
            }
            tm.mTileWidth = oTM.GetInteger("tileWidth");
            if (tm.mTileWidth <= 0)
            {
                CPLError(CE_Failure, CPLE_AppDefined, "Invalid tileWidth: %d",
                         tm.mTileWidth);
                return nullptr;
            }
            tm.mTileHeight = oTM.GetInteger("tileHeight");
            if (tm.mTileHeight <= 0)
            {
                CPLError(CE_Failure, CPLE_AppDefined, "Invalid tileHeight: %d",
                         tm.mTileHeight);
                return nullptr;
            }
            if (tm.mTileWidth > INT_MAX / tm.mTileHeight)
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "tileWidth(%d) x tileHeight(%d) larger than "
                         "INT_MAX",
                         tm.mTileWidth, tm.mTileHeight);
                return nullptr;
            }
            tm.mMatrixWidth = oTM.GetInteger("matrixWidth");
            if (tm.mMatrixWidth <= 0)
            {
                CPLError(CE_Failure, CPLE_AppDefined, "Invalid matrixWidth: %d",
                         tm.mMatrixWidth);
                return nullptr;
            }
            tm.mMatrixHeight = oTM.GetInteger("matrixHeight");
            if (tm.mMatrixHeight <= 0)
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "Invalid matrixHeight: %d", tm.mMatrixHeight);
                return nullptr;
            }

            const auto oVariableMatrixWidths = oTM.GetArray(
                bIsTMSv2 ? "variableMatrixWidths" : "variableMatrixWidth");
            if (oVariableMatrixWidths.IsValid())
            {
                for (const auto &oVMW : oVariableMatrixWidths)
                {
                    TileMatrix::VariableMatrixWidth vmw;
                    vmw.mCoalesce = oVMW.GetInteger("coalesce");
                    vmw.mMinTileRow = oVMW.GetInteger("minTileRow");
                    vmw.mMaxTileRow = oVMW.GetInteger("maxTileRow");
                    tm.mVariableMatrixWidthList.emplace_back(std::move(vmw));
                }
            }

            poTMS->mTileMatrixList.emplace_back(std::move(tm));
        }
    }
    if (poTMS->mTileMatrixList.empty())
    {
        CPLError(CE_Failure, CPLE_AppDefined, "No tileMatrix defined");
        return nullptr;
    }

    return poTMS;
}

/************************************************************************/
/*                       haveAllLevelsSameTopLeft()                     */
/************************************************************************/

bool TileMatrixSet::haveAllLevelsSameTopLeft() const
{
    for (const auto &oTM : mTileMatrixList)
    {
        if (oTM.mTopLeftX != mTileMatrixList[0].mTopLeftX ||
            oTM.mTopLeftY != mTileMatrixList[0].mTopLeftY)
        {
            return false;
        }
    }
    return true;
}

/************************************************************************/
/*                      haveAllLevelsSameTileSize()                     */
/************************************************************************/

bool TileMatrixSet::haveAllLevelsSameTileSize() const
{
    for (const auto &oTM : mTileMatrixList)
    {
        if (oTM.mTileWidth != mTileMatrixList[0].mTileWidth ||
            oTM.mTileHeight != mTileMatrixList[0].mTileHeight)
        {
            return false;
        }
    }
    return true;
}

/************************************************************************/
/*                    hasOnlyPowerOfTwoVaryingScales()                  */
/************************************************************************/

bool TileMatrixSet::hasOnlyPowerOfTwoVaryingScales() const
{
    for (size_t i = 1; i < mTileMatrixList.size(); i++)
    {
        if (mTileMatrixList[i].mScaleDenominator == 0 ||
            std::fabs(mTileMatrixList[i - 1].mScaleDenominator /
                          mTileMatrixList[i].mScaleDenominator -
                      2) > 1e-10)
        {
            return false;
        }
    }
    return true;
}

/************************************************************************/
/*                        hasVariableMatrixWidth()                      */
/************************************************************************/

bool TileMatrixSet::hasVariableMatrixWidth() const
{
    for (const auto &oTM : mTileMatrixList)
    {
        if (!oTM.mVariableMatrixWidthList.empty())
        {
            return true;
        }
    }
    return false;
}

/************************************************************************/
/*                            createRaster()                            */
/************************************************************************/

/* static */
std::unique_ptr<TileMatrixSet>
TileMatrixSet::createRaster(int width, int height, int tileSize,
                            int zoomLevelCount, double dfTopLeftX,
                            double dfTopLeftY, double dfResXFull,
                            double dfResYFull, const std::string &crs)
{
    CPLAssert(width > 0);
    CPLAssert(height > 0);
    CPLAssert(tileSize > 0);
    CPLAssert(zoomLevelCount > 0);
    std::unique_ptr<TileMatrixSet> poTMS(new TileMatrixSet());
    poTMS->mTitle = "raster";
    poTMS->mIdentifier = "raster";
    poTMS->mCrs = crs;
    poTMS->mBbox.mCrs = poTMS->mCrs;
    poTMS->mBbox.mLowerCornerX = dfTopLeftX;
    poTMS->mBbox.mLowerCornerY = dfTopLeftY - height * dfResYFull;
    poTMS->mBbox.mUpperCornerX = dfTopLeftX + width * dfResYFull;
    poTMS->mBbox.mUpperCornerY = dfTopLeftY;
    for (int i = 0; i < zoomLevelCount; i++)
    {
        TileMatrix tm;
        tm.mId = CPLSPrintf("%d", i);
        tm.mResX = dfResXFull * (1 << (zoomLevelCount - 1 - i));
        tm.mResY = dfResYFull * (1 << (zoomLevelCount - 1 - i));
        tm.mScaleDenominator = tm.mResX / 0.28e-3;
        tm.mTopLeftX = poTMS->mBbox.mLowerCornerX;
        tm.mTopLeftY = poTMS->mBbox.mUpperCornerY;
        tm.mTileWidth = tileSize;
        tm.mTileHeight = tileSize;
        tm.mMatrixWidth = std::max(
            1, ((width >> (zoomLevelCount - 1 - i)) + tileSize - 1) / tileSize);
        tm.mMatrixHeight =
            std::max(1, ((height >> (zoomLevelCount - 1 - i)) + tileSize - 1) /
                            tileSize);
        poTMS->mTileMatrixList.emplace_back(std::move(tm));
    }
    return poTMS;
}

}  // namespace gdal

//! @endcond
