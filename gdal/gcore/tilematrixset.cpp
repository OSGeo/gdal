/******************************************************************************
 *
 * Project:  GDAL
 * Purpose:  Class to handle TileMatrixSet
 * Author:   Even Rouault, <even dot rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2020, Even Rouault <even dot rouault at spatialys.com>
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

#include "cpl_json.h"
#include "ogr_spatialref.h"

#include <cmath>
#include <cfloat>

#include "tilematrixset.hpp"

//! @cond Doxygen_Suppress

namespace gdal
{

/************************************************************************/
/*                   listPredefinedTileMatrixSets()                     */
/************************************************************************/

std::set<std::string> TileMatrixSet::listPredefinedTileMatrixSets()
{
    std::set<std::string> l{ "GoogleMapsCompatible",
                             "InspireCRS84Quad" };
    const char* pszSomeFile = CPLFindFile("gdal", "tms_NZTM2000.json");
    if( pszSomeFile )
    {
        CPLStringList aosList(VSIReadDir(CPLGetDirname(pszSomeFile)));
        for( int i = 0; i < aosList.size(); i++ )
        {
            const size_t nLen = strlen(aosList[i]);
            if( nLen > strlen("tms_") + strlen(".json") &&
                STARTS_WITH(aosList[i], "tms_") &&
                EQUAL(aosList[i] + nLen - strlen(".json"), ".json") )
            {
                std::string id(aosList[i] + strlen("tms_"),
                               nLen - (strlen("tms_") + strlen(".json")));
                l.insert(id);
            }
        }
    }
    return l;
}

/************************************************************************/
/*                              parse()                                 */
/************************************************************************/

std::unique_ptr<TileMatrixSet> TileMatrixSet::parse(const char* fileOrDef)
{
    CPLJSONDocument oDoc;
    std::unique_ptr<TileMatrixSet> poTMS(new TileMatrixSet());

    constexpr double HALF_CIRCUMFERENCE = 6378137 * M_PI;
    if( EQUAL(fileOrDef, "GoogleMapsCompatible") )
    {
        /* See http://portal.opengeospatial.org/files/?artifact_id=35326 (WMTS 1.0), Annex E.4 */
        poTMS->mTitle = "GoogleMapsCompatible";
        poTMS->mIdentifier = "GoogleMapsCompatible";
        poTMS->mCrs = "http://www.opengis.net/def/crs/EPSG/0/3857";
        poTMS->mBbox.mCrs = poTMS->mCrs;
        poTMS->mBbox.mLowerCornerX = -HALF_CIRCUMFERENCE;
        poTMS->mBbox.mLowerCornerY = -HALF_CIRCUMFERENCE;
        poTMS->mBbox.mUpperCornerX = HALF_CIRCUMFERENCE;
        poTMS->mBbox.mUpperCornerY = HALF_CIRCUMFERENCE;
        poTMS->mWellKnownScaleSet = "http://www.opengis.net/def/wkss/OGC/1.0/GoogleMapsCompatible";
        for( int i = 0; i < 25; i++ )
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

    if( EQUAL(fileOrDef, "InspireCRS84Quad") )
    {
        /* See InspireCRS84Quad at http://inspire.ec.europa.eu/documents/Network_Services/TechnicalGuidance_ViewServices_v3.0.pdf */
        /* This is exactly the same as PseudoTMS_GlobalGeodetic */
        /* See global-geodetic at http://wiki.osgeo.org/wiki/Tile_Map_Service_Specification */
        // See also http://docs.opengeospatial.org/is/17-083r2/17-083r2.html#76
        poTMS->mTitle = "InspireCRS84Quad";
        poTMS->mIdentifier = "InspireCRS84Quad";
        poTMS->mCrs = "http://www.opengis.net/def/crs/OGC/1.3/CRS84";
        poTMS->mBbox.mCrs = poTMS->mCrs;
        poTMS->mBbox.mLowerCornerX = -180;
        poTMS->mBbox.mLowerCornerY = -90;
        poTMS->mBbox.mUpperCornerX = 180;
        poTMS->mBbox.mUpperCornerY = 90;
        poTMS->mWellKnownScaleSet = "http://www.opengis.net/def/wkss/OGC/1.0/GoogleCRS84Quad";
        for( int i = 0; i < 18; i++ )
        {
            TileMatrix tm;
            tm.mId = CPLSPrintf("%d", i);
            tm.mResX = 180. / 256 / (1 << i);
            tm.mResY = tm.mResX;
            tm.mScaleDenominator = tm.mResX * (HALF_CIRCUMFERENCE / 180) / 0.28e-3;
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

    bool loadOk = false;
    if( (strstr(fileOrDef, "\"type\"") != nullptr &&
         strstr(fileOrDef, "\"TileMatrixSetType\"") != nullptr) ||
        (strstr(fileOrDef, "\"identifier\"") != nullptr &&
         strstr(fileOrDef, "\"boundingBox\"") != nullptr &&
         (strstr(fileOrDef, "\"tileMatrix\"") != nullptr ||
          strstr(fileOrDef, "\"tileMatrices\"") != nullptr)) )
    {
        loadOk = oDoc.LoadMemory(fileOrDef);
    }
    else if( STARTS_WITH_CI(fileOrDef, "http://") ||
             STARTS_WITH_CI(fileOrDef, "https://") )
    {
        const char* const apszOptions[] = { "MAX_FILE_SIZE=1000000", nullptr };
        loadOk = oDoc.LoadUrl(fileOrDef, apszOptions);
    }
    else
    {
        VSIStatBufL sStat;
        if( VSIStatL( fileOrDef, &sStat ) == 0 )
        {
            loadOk = oDoc.Load(fileOrDef);
        }
        else
        {
            const char* pszFilename = CPLFindFile( "gdal",
                (std::string("tms_") + fileOrDef + ".json").c_str() );
            if( pszFilename )
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
    if( !loadOk )
    {
        return nullptr;
    }

    auto oRoot = oDoc.GetRoot();
    if( oRoot.GetString("type") != "TileMatrixSetType" &&
        !oRoot.GetObj("tileMatrix").IsValid() &&
        !oRoot.GetObj("tileMatrices").IsValid() )
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Expected type = TileMatrixSetType");
        return nullptr;
    }

    poTMS->mIdentifier = oRoot.GetString("identifier");
    poTMS->mTitle = oRoot.GetString("title");
    poTMS->mAbstract = oRoot.GetString("abstract");
    const auto oBbox = oRoot.GetObj("boundingBox");
    if( oBbox.IsValid() )
    {
        poTMS->mBbox.mCrs = oBbox.GetString("crs");
        const auto oLowerCorner = oBbox.GetArray("lowerCorner");
        if( oLowerCorner.IsValid() && oLowerCorner.Size() == 2 )
        {
            poTMS->mBbox.mLowerCornerX = oLowerCorner[0].ToDouble(NaN);
            poTMS->mBbox.mLowerCornerY = oLowerCorner[1].ToDouble(NaN);
        }
        const auto oUpperCorner = oBbox.GetArray("upperCorner");
        if( oUpperCorner.IsValid() && oUpperCorner.Size() == 2 )
        {
            poTMS->mBbox.mUpperCornerX = oUpperCorner[0].ToDouble(NaN);
            poTMS->mBbox.mUpperCornerY = oUpperCorner[1].ToDouble(NaN);
        }
    }
    poTMS->mCrs = oRoot.GetString("supportedCRS");
    poTMS->mWellKnownScaleSet = oRoot.GetString("wellKnownScaleSet");

    OGRSpatialReference oCrs;
    if( oCrs.SetFromUserInput(poTMS->mCrs.c_str(),
                              OGRSpatialReference::SET_FROM_USER_INPUT_LIMITATIONS) != OGRERR_NONE )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Cannot parse CRS %s", poTMS->mCrs.c_str());
        return nullptr;
    }
    double dfMetersPerUnit = 1.0;
    if( oCrs.IsProjected() )
    {
        dfMetersPerUnit = oCrs.GetLinearUnits();
    }
    else if( oCrs.IsGeographic() )
    {
        dfMetersPerUnit = oCrs.GetSemiMajor() * M_PI / 180;
    }

    const auto oTileMatrix = oRoot.GetObj("tileMatrix").IsValid() ?
        oRoot.GetArray("tileMatrix") :
        oRoot.GetArray("tileMatrices");
    if( oTileMatrix.IsValid() )
    {
        double dfLastScaleDenominator = std::numeric_limits<double>::max();
        for( const auto& oTM: oTileMatrix )
        {
            TileMatrix tm;
            tm.mId = oTM.GetString("identifier");
            tm.mScaleDenominator = oTM.GetDouble("scaleDenominator");
            if( tm.mScaleDenominator >= dfLastScaleDenominator ||
                tm.mScaleDenominator <= 0 )
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                        "Invalid scale denominator or non-decreasing series "
                        "of scale denominators");
                return nullptr;
            }
            dfLastScaleDenominator = tm.mScaleDenominator;
            // See note g of Table 2 of http://docs.opengeospatial.org/is/17-083r2/17-083r2.html
            tm.mResX = tm.mScaleDenominator * 0.28e-3 / dfMetersPerUnit;
            tm.mResY = tm.mResX;
            const auto oTopLeftCorner = oTM.GetArray("topLeftCorner");
            if( oTopLeftCorner.IsValid() && oTopLeftCorner.Size() == 2 )
            {
                tm.mTopLeftX = oTopLeftCorner[0].ToDouble(NaN);
                tm.mTopLeftY = oTopLeftCorner[1].ToDouble(NaN);
            }
            tm.mTileWidth = oTM.GetInteger("tileWidth");
            tm.mTileHeight = oTM.GetInteger("tileHeight");
            tm.mMatrixWidth = oTM.GetInteger("matrixWidth");
            tm.mMatrixHeight = oTM.GetInteger("matrixHeight");

            const auto oVariableMatrixWidth = oTM.GetObj("variableMatrixWidth").IsValid() ?
                oTM.GetArray("variableMatrixWidth") :
                oTM.GetArray("variableMatrixWidths");
            if( oVariableMatrixWidth.IsValid() )
            {
                for( const auto& oVMW: oVariableMatrixWidth )
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
    if( poTMS->mTileMatrixList.empty() )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "No tileMatrix defined");
        return nullptr;
    }

    return poTMS;
}

/************************************************************************/
/*                       haveAllLevelsSameTopLeft()                     */
/************************************************************************/

bool TileMatrixSet::haveAllLevelsSameTopLeft() const
{
    for( const auto& oTM: mTileMatrixList )
    {
        if( oTM.mTopLeftX != mTileMatrixList[0].mTopLeftX ||
            oTM.mTopLeftY != mTileMatrixList[0].mTopLeftY )
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
    for( const auto& oTM: mTileMatrixList )
    {
        if( oTM.mTileWidth != mTileMatrixList[0].mTileWidth ||
            oTM.mTileHeight != mTileMatrixList[0].mTileHeight )
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
    for( size_t i = 1; i < mTileMatrixList.size(); i++ )
    {
        if( mTileMatrixList[i].mScaleDenominator == 0 ||
            std::fabs(mTileMatrixList[i-1].mScaleDenominator /
                        mTileMatrixList[i].mScaleDenominator - 2) > 1e-10 )
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
    for( const auto& oTM: mTileMatrixList )
    {
        if( !oTM.mVariableMatrixWidthList.empty() )
        {
            return true;
        }
    }
    return false;
}

} // namespace gdal

//! @endcond
