#ifndef RDB_DATASET_INCLUDED
#define RDB_DATASET_INCLUDED

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

template <typename T> struct RDBCoordinatesPlusData
{
    double adfCoordinates[2];
    T data;
};
class RDBDataset : public GDALPamDataset
{
    friend class RDBRasterBand;
    template <typename T> friend class RDBRasterBandInternal;

    // is locking needed?
    // std::mutex oLock;
    FILE *fp = nullptr;
    riegl::rdb::Context oContext;
    riegl::rdb::Pointcloud oPointcloud;
    riegl::rdb::pointcloud::QueryStat oStatQuery;

    double dfResolution = 0;
    int nChunkSize = 0;
    double dfSizeOfTile;
    double dfSizeOfPixel;
    CPLString osWktString;

    std::vector<RDBNode> aoRDBNodes;

    double dfXMin;
    double dfYMin;

    double dfXMax;
    double dfYMax;

    double adfMinimum[2] = {};
    double adfMaximum[2] = {};

  public:
    explicit RDBDataset(GDALOpenInfo *poOpenInfo);
    ~RDBDataset();

    static GDALDataset *Open(GDALOpenInfo *poOpenInfo);
    static int Identify(GDALOpenInfo *poOpenInfo);

    CPLErr GetGeoTransform(double *padfTransform) override;
    const char *_GetProjectionRef() override;

  protected:
    static void SetBandInternal(
        RDBDataset *poDs, const std::string &osAttributeName,
        const riegl::rdb::pointcloud::PointAttribute &oPointAttribute,
        riegl::rdb::pointcloud::DataType eRdbDataType, int &nBandIndex);
    void traverseRdbNodes(const riegl::rdb::pointcloud::GraphNode &oNode);

    void ReadGeoreferencing();
};

class RDBRasterBand : public GDALPamRasterBand
{
  protected:
    CPLString osAttributeName;
    CPLString osDescription;
    riegl::rdb::pointcloud::PointAttribute oPointAttribute;

  public:
    RDBRasterBand(
        RDBDataset *poDSIn, const std::string &osAttributeName,
        const riegl::rdb::pointcloud::PointAttribute &oPointAttributeIn,
        int nBandIn, GDALDataType eDataTypeIn);
    virtual double GetNoDataValue(int *pbSuccess = nullptr) override;
    virtual const char *GetDescription() const override;
};
}  // namespace rdb
void GDALRegister_RDB();

#endif  // RDB_DATASET_INCLUDED