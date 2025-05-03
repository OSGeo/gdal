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

#ifndef TILEMATRIXSET_HPP_INCLUDED
#define TILEMATRIXSET_HPP_INCLUDED

#include "cpl_port.h"

#include <memory>
#include <limits>
#include <set>
#include <string>
#include <vector>

//! @cond Doxygen_Suppress

namespace gdal
{

class CPL_DLL TileMatrixSet
{
    static constexpr double NaN = std::numeric_limits<double>::quiet_NaN();

  public:
    const std::string &identifier() const
    {
        return mIdentifier;
    }

    const std::string &title() const
    {
        return mTitle;
    }

    //! "abstract" in TMS v1 / "description" in TMS v2
    const std::string &abstract() const
    {
        return mAbstract;
    }

    struct CPL_DLL BoundingBox
    {
        std::string mCrs{};  //! Can be a URL, a URI, a WKT or PROJJSON string.
        double mLowerCornerX{NaN};
        double mLowerCornerY{NaN};
        double mUpperCornerX{NaN};
        double mUpperCornerY{NaN};
    };

    const BoundingBox &bbox() const
    {
        return mBbox;
    }

    //! Can be a URL, a URI, a WKT or PROJJSON string.
    const std::string &crs() const
    {
        return mCrs;
    }

    const std::string &wellKnownScaleSet() const
    {
        return mWellKnownScaleSet;
    }

    struct CPL_DLL TileMatrix
    {
        std::string mId{};
        double mScaleDenominator{NaN};
        double mResX{
            NaN};  // computed from mScaleDenominator and CRS definition
        double mResY{
            NaN};  // computed from mScaleDenominator and CRS definition
        double mTopLeftX{NaN};
        double mTopLeftY{NaN};
        int mTileWidth{};
        int mTileHeight{};
        int mMatrixWidth{};
        int mMatrixHeight{};

        struct CPL_DLL VariableMatrixWidth
        {
            int mCoalesce{};
            int mMinTileRow{};
            int mMaxTileRow{};
        };

        std::vector<VariableMatrixWidth> mVariableMatrixWidthList{};
    };

    const std::vector<TileMatrix> &tileMatrixList() const
    {
        return mTileMatrixList;
    }

    /** Parse a TileMatrixSet definition, passed inline or by filename,
     * corresponding to the JSON encoding of the OGC Two Dimensional Tile Matrix
     * Set: http://docs.opengeospatial.org/is/17-083r2/17-083r2.html */
    static std::unique_ptr<TileMatrixSet> parse(const char *fileOrDef);

    /** Create a raster tiling scheme */
    static std::unique_ptr<TileMatrixSet>
    createRaster(int width, int height, int tileSize, int zoomLevelCount,
                 double dfTopLeftX = 0.0, double dfTopLeftY = 0.0,
                 double dfResXFull = 1.0, double dfResYFull = 1.0,
                 const std::string &mCRS = std::string());

    /** Return hardcoded tile matrix set names (such as GoogleMapsCompatible),
     * as well as XXX for each tms_XXXX.json in GDAL data directory */
    static std::vector<std::string> listPredefinedTileMatrixSets();

    bool haveAllLevelsSameTopLeft() const;

    bool haveAllLevelsSameTileSize() const;

    bool hasOnlyPowerOfTwoVaryingScales() const;

    bool hasVariableMatrixWidth() const;

  private:
    TileMatrixSet() = default;

    std::string mIdentifier{};
    std::string mTitle{};
    std::string mAbstract{};
    BoundingBox mBbox{};
    std::string mCrs{};
    std::string mWellKnownScaleSet{};
    std::vector<TileMatrix> mTileMatrixList{};
};

}  // namespace gdal

//! @endcond

#endif  // TILEMATRIXSET_HPP_INCLUDED
