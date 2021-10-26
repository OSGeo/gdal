/******************************************************************************
 * $Id$
 *
 * Project:  RIEGL RDB 2 driver
 * Purpose:  Add support for reading *.mpx RDB 2 files.
 * Author:   RIEGL Laser Measurement Systems GmbH (support@riegl.com)
 *
 ******************************************************************************
 * Copyright (c) 2019, RIEGL Laser Measurement Systems GmbH (support@riegl.com)
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

#ifndef RDB_DATASET_INCLUDED
#define RDB_DATASET_INCLUDED

#include "../frmts/vrt/vrtdataset.h"
#include "gdal_pam.h"

#include <riegl/rdb.hpp>

#include <algorithm>
#include <memory>
#include <mutex>
#include <string>
namespace rdb
{
class RDBRasterBand;

struct RDBNode
{
    int nXBlockCoordinates = 0;
    int nYBlockCoordinates = 0;
    riegl::rdb::pointcloud::GraphNode::ID iID = 0;
    uint64_t nPointCount = 0;
};

struct RDBOverview
{
    double dfTileSize = 0;
    double dfPixelSize = 0;
    double adfMinimum[2] = {std::numeric_limits<double>::max(),
                            std::numeric_limits<double>::max()};
    double adfMaximum[2] = {std::numeric_limits<double>::lowest(),
                            std::numeric_limits<double>::lowest()};
    std::vector<RDBNode> aoRDBNodes;
    void addRDBNode(RDBNode &oRDBNode, double dfXMin, double dfYMin,
                    double dfXMax, double dfYMax);
    void setTileSize(double dfTileSizeIn);
};

template <typename T> struct RDBCoordinatesPlusData
{
    double adfCoordinates[2];
    T data;
};

class RDBDataset final: public GDALPamDataset
{
    friend class RDBRasterBand;
    template <typename T> friend class RDBRasterBandInternal;

    // is locking needed?
    // std::mutex oLock;
    FILE *fp = nullptr;
    riegl::rdb::Context oContext;
    riegl::rdb::Pointcloud oPointcloud;
    riegl::rdb::pointcloud::QueryStat oStatQuery;

    OGRSpatialReference oSpatialReference;

    double dfResolution = 0;
    int nChunkSize = 0;
    double dfSizeOfTile;
    double dfSizeOfPixel;
    CPLString osWktString;

    std::vector<RDBOverview> aoRDBOverviews;
    std::vector<std::unique_ptr<VRTDataset>> apoVRTDataset;

    double dfXMin;
    double dfYMin;

    double dfXMax;
    double dfYMax;

    double adfMinimumDs[2] = {};
    double adfMaximumDs[2] = {};

  public:
    explicit RDBDataset(GDALOpenInfo *poOpenInfo);
    ~RDBDataset();

    static GDALDataset *Open(GDALOpenInfo *poOpenInfo);
    static int Identify(GDALOpenInfo *poOpenInfo);

    CPLErr GetGeoTransform(double *padfTransform) override;
    const OGRSpatialReference *GetSpatialRef() const override;

  protected:
    static void SetBandInternal(
        RDBDataset *poDs, const std::string &osAttributeName,
        const riegl::rdb::pointcloud::PointAttribute &oPointAttribute,
        riegl::rdb::pointcloud::DataType eRDBDataType, int nLevel,
        int nNumberOfLevels, int &nBandIndex);
    void addRDBNode(const riegl::rdb::pointcloud::GraphNode &oNode,
                    double dfTileSize, std::size_t nLeve);
    double traverseRDBNodes(const riegl::rdb::pointcloud::GraphNode &oNode,
                            std::size_t nLevel = 0);

    void ReadGeoreferencing();
};

class RDBRasterBand CPL_NON_FINAL: public GDALPamRasterBand
{
  protected:
    CPLString osAttributeName;
    CPLString osDescription;
    riegl::rdb::pointcloud::PointAttribute oPointAttribute;
    int nLevel;

  public:
    RDBRasterBand(
        RDBDataset *poDSIn, const std::string &osAttributeName,
        const riegl::rdb::pointcloud::PointAttribute &oPointAttributeIn,
        int nBandIn, GDALDataType eDataTypeIn, int nLevelIn);

    virtual double GetNoDataValue(int *pbSuccess = nullptr) override;
    virtual const char *GetDescription() const override;
};
}  // namespace rdb
void GDALRegister_RDB();

#endif  // RDB_DATASET_INCLUDED