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
        const std::string& identifier() const { return mIdentifier; }
        const std::string& title() const { return mTitle; }
        const std::string& abstract() const { return mAbstract; }

        struct CPL_DLL BoundingBox
        {
            std::string mCrs{};
            double mLowerCornerX{NaN};
            double mLowerCornerY{NaN};
            double mUpperCornerX{NaN};
            double mUpperCornerY{NaN};
        };
        const BoundingBox& bbox() const { return mBbox; }
        const std::string& crs() const { return mCrs; }
        const std::string& wellKnownScaleSet() const { return mWellKnownScaleSet; }

        struct CPL_DLL TileMatrix
        {
            std::string mId{};
            double mScaleDenominator{NaN};
            double mResX{NaN}; // computed from mScaleDenominator and CRS definition
            double mResY{NaN}; // computed from mScaleDenominator and CRS definition
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
        const std::vector<TileMatrix>& tileMatrixList() const { return mTileMatrixList; }

        /** Parse a TileMatrixSet definition, passed inline or by filename,
         * corresponding to the JSON encoding of the OGC Two Dimensional Tile Matrix Set:
         * http://docs.opengeospatial.org/is/17-083r2/17-083r2.html */
        static std::unique_ptr<TileMatrixSet> parse(const char* fileOrDef);

        /** Return hardcoded tile matrix set names (such as GoogleMapsCompatible), as
         * well as XXX for each tms_XXXX.json in GDAL data directory */
        static std::set<std::string> listPredefinedTileMatrixSets();

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

} // namespace gdal

//! @endcond

#endif // TILEMATRIXSET_HPP_INCLUDED
