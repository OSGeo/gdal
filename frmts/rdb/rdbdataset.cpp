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

#include "ogrgeojsonreader.h"

#include "rdbdataset.hpp"

#include <cmath>
#include <sstream>

namespace rdb
{
void RDBOverview::addRDBNode(RDBNode &oRDBNode, double dfXMin, double dfYMin,
                             double dfXMax, double dfYMax)
{
    adfMinimum[0] = std::min(adfMinimum[0], dfXMin);
    adfMaximum[0] = std::max(adfMaximum[0], dfXMax);

    adfMinimum[1] = std::min(adfMinimum[1], dfYMin);
    adfMaximum[1] = std::max(adfMaximum[1], dfYMax);

    aoRDBNodes.push_back(oRDBNode);
}
void RDBOverview::setTileSize(double dfTileSizeIn)
{
    dfTileSize = dfTileSizeIn;
    dfPixelSize = dfTileSize / 256.0;
}

template <typename T> struct CPLMallocGuard
{
    T *const pData = nullptr;
    explicit CPLMallocGuard(std::size_t count)
        : pData(static_cast<T *>(CPLMalloc(sizeof(T) * count)))
    {
    }
    ~CPLMallocGuard() { CPLFree(pData); }
};

template <typename T> class RDBRasterBandInternal;

template <typename T> class RDBRasterBandInternal final: public RDBRasterBand
{
    std::vector<std::unique_ptr<RDBRasterBandInternal<T>>> aoOverviewBands;
    std::vector<VRTSourcedRasterBand *> aoVRTRasterBand;

  public:
    RDBRasterBandInternal(
        RDBDataset *poDSIn, const std::string &osAttributeNameIn,
        const riegl::rdb::pointcloud::PointAttribute &oPointAttributeIn,
        int nBandIn, GDALDataType eDataTypeIn, int nLevelIn)
        : RDBRasterBand(poDSIn, osAttributeNameIn, oPointAttributeIn, nBandIn,
                        eDataTypeIn, nLevelIn)
    {
        poDS = poDSIn;
        nBand = nBandIn;

        eDataType = eDataTypeIn;
        eAccess = poDSIn->eAccess;

        auto &oRDBOverview = poDSIn->aoRDBOverviews[nLevelIn];

        nRasterXSize = static_cast<int>(
            (oRDBOverview.adfMaximum[0] - oRDBOverview.adfMinimum[0]) * 256);
        nRasterYSize = static_cast<int>(
            (oRDBOverview.adfMaximum[1] - oRDBOverview.adfMinimum[1]) * 256);

        nBlockXSize = 256;
        nBlockYSize = 256;
    }

    ~RDBRasterBandInternal() {}
    RDBRasterBandInternal(
        RDBDataset *poDSIn, const std::string &osAttributeNameIn,
        const riegl::rdb::pointcloud::PointAttribute &oPointAttributeIn,
        int nBandIn, GDALDataType eDataTypeIn, int nLevelIn, int nNumberOfLayers)
        : RDBRasterBandInternal(poDSIn, osAttributeNameIn, oPointAttributeIn,
                                nBandIn, eDataTypeIn, nLevelIn)
    {
        aoOverviewBands.resize(nNumberOfLayers);
        poDSIn->apoVRTDataset.resize(nNumberOfLayers);

        for(int i = nNumberOfLayers - 2; i >= 0; i--)
        {
            aoOverviewBands[i].reset(new RDBRasterBandInternal<T>(
                poDSIn, osAttributeNameIn, oPointAttributeIn, nBandIn,
                eDataTypeIn, i));
            RDBOverview &oRDBOverview = poDSIn->aoRDBOverviews[i];

            int nDatasetXSize = static_cast<int>(std::round(
                (poDSIn->dfXMax - poDSIn->dfXMin) / oRDBOverview.dfPixelSize));

            int nDatasetYSize = static_cast<int>(std::round(
                (poDSIn->dfYMax - poDSIn->dfYMin) / oRDBOverview.dfPixelSize));

            if(!poDSIn->apoVRTDataset[i])
            {
                poDSIn->apoVRTDataset[i].reset(
                    new VRTDataset(nDatasetXSize, nDatasetYSize));
            }

            VRTAddBand(poDSIn->apoVRTDataset[i].get(), eDataType, nullptr);

            VRTSourcedRasterBand *hVRTBand(dynamic_cast<VRTSourcedRasterBand *>(
                poDSIn->apoVRTDataset[i]->GetRasterBand(nBandIn)));

            int bSuccess = FALSE;
            double dfNoDataValue =
                RDBRasterBandInternal::GetNoDataValue(&bSuccess);
            if(bSuccess == FALSE)
            {
                dfNoDataValue = VRT_NODATA_UNSET;
            }

            hVRTBand->AddSimpleSource(
                aoOverviewBands[i].get(),
                (poDSIn->dfXMin -
                 oRDBOverview.adfMinimum[0] * oRDBOverview.dfTileSize) /
                    (oRDBOverview.dfPixelSize),
                (poDSIn->dfYMin -
                 oRDBOverview.adfMinimum[1] * oRDBOverview.dfTileSize) /
                    (oRDBOverview.dfPixelSize),
                nDatasetXSize, nDatasetYSize, 0, 0, nDatasetXSize,
                nDatasetYSize, "average", dfNoDataValue);

            aoVRTRasterBand.push_back(hVRTBand);
        }
        poDS = poDSIn;
        nBand = nBandIn;

        eDataType = eDataTypeIn;
        eAccess = poDSIn->eAccess;
        nRasterXSize = poDSIn->nRasterXSize;
        nRasterYSize = poDSIn->nRasterYSize;
        nBlockXSize = 256;
        nBlockYSize = 256;
    }

    double GetNoDataValue(int *pbSuccess) override
    {
        double dfInvalidValue = RDBRasterBand::GetNoDataValue(pbSuccess);

        if(pbSuccess != nullptr && *pbSuccess == TRUE)
        {
            return dfInvalidValue;
        }
        else
        {
            if(oPointAttribute.maximumValue < std::numeric_limits<T>::max())
            {
                if(pbSuccess != nullptr)
                {
                    *pbSuccess = TRUE;
                }
                return std::numeric_limits<T>::max();
            }
            else if(oPointAttribute.minimumValue >
                    std::numeric_limits<T>::lowest())
            {
                if(pbSuccess != nullptr)
                {
                    *pbSuccess = TRUE;
                }
                return std::numeric_limits<T>::lowest();
            }
            // Another no data value could be any value that is actually not in
            // the data but in the range of specified rdb attribute. However,
            // this could maybe be a problem when combining multiple files.

            // Using always the maximum or minimum value in such cases might be
            // at least consistent across multiple files.
            if(pbSuccess != nullptr)
            {
                *pbSuccess = FALSE;
            }
            return 0.0;
        }
    }
    virtual CPLErr IReadBlock(int nBlockXOff, int nBlockYOff,
                              void *pImageIn) override
    {
        T *pImage = reinterpret_cast<T *>(pImageIn);

        constexpr std::size_t nTileSize = 256 * 256;
        if(std::isnan(oPointAttribute.invalidValue))
        {
            memset(pImageIn, 0, sizeof(T) * nTileSize);
        }
        else
        {
            for(std::size_t i = 0; i < nTileSize; i++)
            {
                pImage[i] = static_cast<T>(oPointAttribute.invalidValue);
            }
        }

        try
        {
            RDBDataset *poRDBDs = dynamic_cast<RDBDataset *>(poDS);
            if(poRDBDs != nullptr)
            {
                auto &oRDBOverview = poRDBDs->aoRDBOverviews[nLevel];
                auto &aoRDBNodes = oRDBOverview.aoRDBNodes;

                auto pIt = std::find_if(
                    aoRDBNodes.begin(), aoRDBNodes.end(),
                    [&](const RDBNode &poRDBNode) {
                        return poRDBNode.nXBlockCoordinates == nBlockXOff &&
                               poRDBNode.nYBlockCoordinates == nBlockYOff;
                    });

                if(pIt != aoRDBNodes.end())
                {
                    using type = RDBCoordinatesPlusData<T>;
                    CPLMallocGuard<type> oData(pIt->nPointCount);

                    uint32_t nPointsReturned = 0;
                    {
                        // is locking needed?
                        // std::lock_guard<std::mutex>
                        // oGuard(poRDBDs->oLock);
                        auto oSelectQuery =
                            poRDBDs->oPointcloud.select(pIt->iID);
                        oSelectQuery.bindBuffer(
                            poRDBDs->oPointcloud.pointAttribute()
                                .primaryAttributeName(),
                            oData.pData[0].adfCoordinates[0],
                            static_cast<int32_t>(sizeof(type)));

                        oSelectQuery.bindBuffer(
                            osAttributeName, oData.pData[0].data,
                            static_cast<int32_t>(sizeof(type)));

                        nPointsReturned = oSelectQuery.next(pIt->nPointCount);
                    }

                    if(nPointsReturned > 0)
                    {
                        double dfHalvePixel = oRDBOverview.dfPixelSize * 0.5;

                        double dfTileMinX =
                            (std::floor((poRDBDs->dfXMin + dfHalvePixel) /
                                        oRDBOverview.dfTileSize) +
                             nBlockXOff) *
                            oRDBOverview.dfTileSize;
                        double dfTileMinY =
                            (std::floor((poRDBDs->dfYMin + dfHalvePixel) /
                                        oRDBOverview.dfTileSize) +
                             nBlockYOff) *
                            oRDBOverview.dfTileSize;

                        for(uint32_t i = 0; i < nPointsReturned; i++)
                        {
                            int dfPixelX = static_cast<int>(
                                std::floor((oData.pData[i].adfCoordinates[0] +
                                            dfHalvePixel - dfTileMinX) /
                                           oRDBOverview.dfPixelSize));

                            int dfPixelY = static_cast<int>(
                                std::floor((oData.pData[i].adfCoordinates[1] +
                                            dfHalvePixel - dfTileMinY) /
                                           oRDBOverview.dfPixelSize));

                            pImage[dfPixelY * 256 + dfPixelX] =
                                oData.pData[i].data;
                        }
                    }
                }
            }
        }
        catch(const riegl::rdb::Error &oException)
        {
            CPLError(CE_Failure, CPLE_AppDefined, "RDB error: %s, %s",
                     oException.what(), oException.details());
            return CE_Failure;
        }
        catch(const std::exception &oException)
        {
            CPLError(CE_Failure, CPLE_AppDefined, "Error: %s",
                     oException.what());
            return CE_Failure;
        }
        catch(...)
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Unknown error in IReadBlock.");
            return CE_Failure;
        }
        return CE_None;
    }

    virtual int GetOverviewCount() override
    {
        RDBDataset *poRDBDs = dynamic_cast<RDBDataset *>(poDS);
        if(poRDBDs == nullptr)
        {
            return 0;
        }
        return static_cast<int>(aoVRTRasterBand.size());
    }
    virtual GDALRasterBand *GetOverview(int i) override
    {
        return aoVRTRasterBand[i];
    }
};

RDBDataset::~RDBDataset() {}

void RDBDataset::SetBandInternal(
    RDBDataset *poDs, const std::string &osAttributeName,
    const riegl::rdb::pointcloud::PointAttribute &oPointAttribute,
    riegl::rdb::pointcloud::DataType eRDBDataType, int nLevel,
    int nNumberOfLayers, int &nBandIndex)
{
    RDBRasterBand *poBand = nullptr;
    // map riegl rdb datatype to gdal data type
    switch(eRDBDataType)
    {
    case riegl::rdb::pointcloud::DataType::UINT8:
    // Should I do ignore the other type?
    case riegl::rdb::pointcloud::DataType::INT8:
        poBand = new RDBRasterBandInternal<std::uint8_t>(
            poDs, osAttributeName, oPointAttribute, nBandIndex, GDT_Byte,
            nLevel, nNumberOfLayers);
        break;
    case riegl::rdb::pointcloud::DataType::UINT16:
        poBand = new RDBRasterBandInternal<std::uint16_t>(
            poDs, osAttributeName, oPointAttribute, nBandIndex, GDT_UInt16,
            nLevel, nNumberOfLayers);
        break;
    case riegl::rdb::pointcloud::DataType::INT16:
        poBand = new RDBRasterBandInternal<std::int16_t>(
            poDs, osAttributeName, oPointAttribute, nBandIndex, GDT_Int16,
            nLevel, nNumberOfLayers);
        break;
    case riegl::rdb::pointcloud::DataType::UINT32:
        poBand = new RDBRasterBandInternal<std::uint32_t>(
            poDs, osAttributeName, oPointAttribute, nBandIndex, GDT_UInt32,
            nLevel, nNumberOfLayers);
        break;
    case riegl::rdb::pointcloud::DataType::INT32:
        poBand = new RDBRasterBandInternal<std::int32_t>(
            poDs, osAttributeName, oPointAttribute, nBandIndex, GDT_Int32,
            nLevel, nNumberOfLayers);
        break;
    case riegl::rdb::pointcloud::DataType::FLOAT32:
        poBand = new RDBRasterBandInternal<float>(
            poDs, osAttributeName, oPointAttribute, nBandIndex, GDT_Float32,
            nLevel, nNumberOfLayers);
        break;
    case riegl::rdb::pointcloud::DataType::FLOAT64:
        poBand = new RDBRasterBandInternal<double>(
            poDs, osAttributeName, oPointAttribute, nBandIndex, GDT_Float64,
            nLevel, nNumberOfLayers);
        break;
    default:
        // for all reamining use double. e.g. u/int64_t
        // an alternate option would be to check the data in the rdb and use the
        // minimum required data type. could be a problem when working with
        // multiple files.
        poBand = new RDBRasterBandInternal<double>(
            poDs, osAttributeName, oPointAttribute, nBandIndex, GDT_Float64,
            nLevel, nNumberOfLayers);
        break;
    }

    poDs->SetBand(nBandIndex, poBand);
    nBandIndex++;
}

void RDBDataset::addRDBNode(const riegl::rdb::pointcloud::GraphNode &oNode,
                            double dfTileSize, std::size_t nLevel)
{
    double adfNodeMinimum[2];
    double adfNodeMaximum[2];

    oStatQuery.minimum(oNode.id,
                       oPointcloud.pointAttribute().primaryAttributeName(),
                       adfNodeMinimum[0]);
    oStatQuery.maximum(oNode.id,
                       oPointcloud.pointAttribute().primaryAttributeName(),
                       adfNodeMaximum[0]);

    int dfXNodeMin = static_cast<int>(
        std::floor((adfNodeMinimum[0] + dfSizeOfPixel * 0.5) / dfTileSize));
    int dfYNodeMin = static_cast<int>(
        std::floor((adfNodeMinimum[1] + dfSizeOfPixel * 0.5) / dfTileSize));

    int dfXNodeMax = static_cast<int>(
        std::ceil((adfNodeMaximum[0] + dfSizeOfPixel * 0.5) / dfTileSize));
    int dfYNodeMax = static_cast<int>(
        std::ceil((adfNodeMaximum[1] + dfSizeOfPixel * 0.5) / dfTileSize));

    RDBNode oRDBNode;

    oRDBNode.iID = oNode.id;
    oRDBNode.nPointCount = oNode.pointCountNode;
    oRDBNode.nXBlockCoordinates =
        dfXNodeMin - static_cast<int>(std::floor(
                         (dfXMin + dfSizeOfPixel * 0.5) / dfTileSize));
    oRDBNode.nYBlockCoordinates =
        dfYNodeMin - static_cast<int>(std::floor(
                         (dfYMin + dfSizeOfPixel * 0.5) / dfTileSize));

    if(aoRDBOverviews.size() <= nLevel)
    {
        aoRDBOverviews.resize(nLevel + 1);
    }
    aoRDBOverviews[nLevel].setTileSize(dfTileSize);

    aoRDBOverviews[nLevel].addRDBNode(oRDBNode, dfXNodeMin, dfYNodeMin,
                                      dfXNodeMax, dfYNodeMax);
}

double
RDBDataset::traverseRDBNodes(const riegl::rdb::pointcloud::GraphNode &oNode,
                             std::size_t nLevel)
{
    if(oNode.children.size() == 0)
    {
        addRDBNode(oNode, dfSizeOfTile, nLevel);
        return dfSizeOfTile;
    }
    else
    {
        double dfSizeOfChildTile = 0.0;
        for(auto &&oChild : oNode.children)
        {
            dfSizeOfChildTile = traverseRDBNodes(oChild, nLevel + 1);
        }
        if(dfSizeOfChildTile >= dfSizeOfTile && oNode.pointCountNode > 0)
        {
            double dfTileSizeCurrentLevel = dfSizeOfChildTile * 2.0;

            addRDBNode(oNode, dfTileSizeCurrentLevel, nLevel);

            return dfTileSizeCurrentLevel;
        }
        return 0.0;
    }
}

RDBDataset::RDBDataset(GDALOpenInfo *poOpenInfo) : oPointcloud(oContext)
{
    int nBandIndex = 1;

    riegl::rdb::pointcloud::OpenSettings oSettings(oContext);
    oPointcloud.open(poOpenInfo->pszFilename, oSettings);

    oStatQuery = oPointcloud.stat();

    std::string oPrimaryAttribute =
        oPointcloud.pointAttribute().primaryAttributeName();

    oStatQuery.minimum(1, oPrimaryAttribute, adfMinimumDs[0]);
    oStatQuery.maximum(1, oPrimaryAttribute, adfMaximumDs[0]);

    dfResolution =
        oPointcloud.pointAttribute().get(oPrimaryAttribute).resolution;

    nChunkSize = oPointcloud.management().getChunkSizeLOD();

    std::string oPixelInfo = oPointcloud.metaData().get("riegl.pixel_info");

    json_object *poObj = nullptr;
    if(!OGRJSonParse(oPixelInfo.c_str(), &poObj, true))
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "riegl.pixel_info is invalid JSon: %s", oPixelInfo.c_str());
    }
    JsonObjectUniquePtr oObj(poObj);

    json_object *poPixelSize = CPL_json_object_object_get(poObj, "size");

    if(poPixelSize != nullptr)
    {
        json_object *poSize0 = json_object_array_get_idx(poPixelSize, 0);
        if(poSize0 != nullptr)
        {
            dfSizeOfPixel = json_object_get_double(poSize0);
        }
    }

    ReadGeoreferencing();

    dfSizeOfTile = dfSizeOfPixel * 256;  // 2^8

    double dfHalvePixel = dfSizeOfPixel * 0.5;
    dfXMin = std::floor((adfMinimumDs[0] + dfHalvePixel) / dfSizeOfTile) *
             dfSizeOfTile;
    dfYMin = std::floor((adfMinimumDs[1] + dfHalvePixel) / dfSizeOfTile) *
             dfSizeOfTile;

    dfXMax = std::ceil((adfMaximumDs[0] + dfHalvePixel) / dfSizeOfTile) *
             dfSizeOfTile;
    dfYMax = std::ceil((adfMaximumDs[1] + dfHalvePixel) / dfSizeOfTile) *
             dfSizeOfTile;

    nRasterXSize = static_cast<int>((dfXMax - dfXMin) / dfSizeOfPixel);
    nRasterYSize = static_cast<int>((dfYMax - dfYMin) / dfSizeOfPixel);

    traverseRDBNodes(oStatQuery.index());

    aoRDBOverviews.erase(
        std::remove_if(aoRDBOverviews.begin(), aoRDBOverviews.end(),
                       [](const RDBOverview &oRDBOverView) {
                           return oRDBOverView.aoRDBNodes.empty();
                       }),
        aoRDBOverviews.end());

    double dfLevelFactor = std::pow(2, aoRDBOverviews.size());
    nRasterXSize = static_cast<int>(std::ceil(nRasterXSize / dfLevelFactor) *
                                    dfLevelFactor);
    nRasterYSize = static_cast<int>(std::ceil(nRasterYSize / dfLevelFactor) *
                                    dfLevelFactor);

    dfXMax = dfXMin + nRasterXSize * dfSizeOfPixel;
    dfYMax = dfYMin + nRasterYSize * dfSizeOfPixel;

    riegl::rdb::pointcloud::PointAttributes &oPointAttribute =
        oPointcloud.pointAttribute();

    int nNumberOfLevels = static_cast<int>(aoRDBOverviews.size());
    std::vector<std::string> aoExistingPointAttributes = oPointAttribute.list();

    for(auto &&osAttributeName : aoExistingPointAttributes)
    {
        riegl::rdb::pointcloud::PointAttribute oAttribute =
            oPointAttribute.get(osAttributeName);

        if(osAttributeName == oPointAttribute.primaryAttributeName())
        {
            continue;
        }
        if(oAttribute.length == 1)
        {
            SetBandInternal(this, osAttributeName, oAttribute,
                            oAttribute.dataType(), nNumberOfLevels - 1,
                            nNumberOfLevels, nBandIndex);
        }
        else
        {
            for(uint32_t i = 0; i < oAttribute.length; i++)
            {
                std::ostringstream oOss;
                oOss << osAttributeName << '[' << i << ']';
                SetBandInternal(this, oOss.str(), oAttribute,
                                oAttribute.dataType(), nNumberOfLevels - 1,
                                nNumberOfLevels, nBandIndex);
            }
        }
    }
}

GDALDataset *RDBDataset::Open(GDALOpenInfo *poOpenInfo)
{
    if(!Identify(poOpenInfo))
    {
        return nullptr;
    }

    if(poOpenInfo->eAccess == GA_Update)
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "The RDB driver does not support update access to existing "
                 "datasets.");
        return nullptr;
    }

    if(poOpenInfo->fpL == nullptr)
    {
        return nullptr;
    }
    try
    {
        std::unique_ptr<RDBDataset> poDS(new RDBDataset(poOpenInfo));
        // std::swap(poDS->fp, poOpenInfo->fpL);
        // Initialize any PAM information.
        poDS->SetDescription(poOpenInfo->pszFilename);
        poDS->TryLoadXML();

        return poDS.release();
    }
    catch(const riegl::rdb::Error &oException)
    {
        CPLError(CE_Failure, CPLE_OpenFailed, "RDB error: %s, %s",
                 oException.what(), oException.details());
        return nullptr;
    }

    catch(const std::exception &oException)
    {
        CPLError(CE_Failure, CPLE_OpenFailed, "Error: %s", oException.what());
        return nullptr;
    }
    catch(...)
    {
        CPLError(CE_Failure, CPLE_OpenFailed, "Unknown error in Open.");
        return nullptr;
    }

    return nullptr;
}
int RDBDataset::Identify(GDALOpenInfo *poOpenInfo)
{
    const char *psHeader = reinterpret_cast<char *>(poOpenInfo->pabyHeader);
    if(poOpenInfo->nHeaderBytes < 32)
    {
        return FALSE;
    }

    constexpr int kSizeOfRDBHeaderIdentifier = 32;
    constexpr char szRDBHeaderIdentifier[kSizeOfRDBHeaderIdentifier] =
        "RIEGL LMS RDB 2 POINTCLOUD FILE";

    if(strncmp(psHeader, szRDBHeaderIdentifier, kSizeOfRDBHeaderIdentifier))
    {
        // A more comprehensive test could be done by the library.
        // Should file -> library incompatibilities handled in Identify or
        // in the Open function?
        return FALSE;
    }
    return TRUE;
}

CPLErr RDBDataset::GetGeoTransform(double *padfTransform)
{
    padfTransform[0] = dfXMin;
    padfTransform[1] = dfSizeOfPixel;
    padfTransform[2] = 0;

    padfTransform[3] = dfYMin;
    padfTransform[4] = 0;
    padfTransform[5] = dfSizeOfPixel;

    return CE_None;
}

const OGRSpatialReference *RDBDataset::GetSpatialRef() const
{
    return &oSpatialReference;
}

void RDBDataset::ReadGeoreferencing()
{
    if(osWktString.empty())
    {
        try
        {
            std::string oPixelInfo =
                oPointcloud.metaData().get("riegl.geo_tag");

            json_object *poObj = nullptr;
            if(!OGRJSonParse(oPixelInfo.c_str(), &poObj, true))
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "riegl.geo_tag is invalid JSon: %s",
                         oPixelInfo.c_str());
            }
            JsonObjectUniquePtr oObj(poObj);

            json_object *poCrs = CPL_json_object_object_get(poObj, "crs");

            if(poCrs != nullptr)
            {
                json_object *poWkt = CPL_json_object_object_get(poCrs, "wkt");

                if(poWkt != nullptr)
                {
                    osWktString = json_object_get_string(poWkt);
                    oSpatialReference.importFromWkt(osWktString.c_str());
                    oSpatialReference.SetAxisMappingStrategy(
                        OAMS_TRADITIONAL_GIS_ORDER);
                }
            }
        }
        catch(const riegl::rdb::Error &oException)
        {
            CPLError(CE_Failure, CPLE_AppDefined, "RDB error: %s, %s",
                     oException.what(), oException.details());
        }
        catch(const std::exception &oException)
        {
            CPLError(CE_Failure, CPLE_AppDefined, "Error: %s",
                     oException.what());
        }
        catch(...)
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Unknown error in IReadBlock.");
        }
    }
}

RDBRasterBand::RDBRasterBand(
    RDBDataset *poDSIn, const std::string &osAttributeNameIn,
    const riegl::rdb::pointcloud::PointAttribute &oPointAttributeIn,
    int nBandIn, GDALDataType eDataTypeIn, int nLevelIn)
    : osAttributeName(osAttributeNameIn), oPointAttribute(oPointAttributeIn),
      nLevel(nLevelIn)
{

    osDescription.Printf("%s (%s)", oPointAttribute.title.c_str(),
                         osAttributeName.c_str());

    poDS = poDSIn;
    nBand = nBandIn;

    eDataType = eDataTypeIn;
    eAccess = poDSIn->eAccess;
    nRasterXSize = poDSIn->nRasterXSize;
    nRasterYSize = poDSIn->nRasterYSize;
    nBlockXSize = 256;
    nBlockYSize = 256;
}

double RDBRasterBand::GetNoDataValue(int *pbSuccess)
{
    if(!std::isnan(oPointAttribute.invalidValue))
    {
        if(pbSuccess != nullptr)
        {
            *pbSuccess = TRUE;
        }
        return oPointAttribute.invalidValue;
    }
    else
    {
        if(pbSuccess != nullptr)
        {
            *pbSuccess = FALSE;
        }
        return 0.0;
    }
}

const char *RDBRasterBand::GetDescription() const
{
    return osDescription.c_str();
}

}  // namespace rdb
void GDALRegister_RDB()
{
    if(!GDAL_CHECK_VERSION("RDB"))
        return;
    if(GDALGetDriverByName("RDB") != NULL)
        return;
    GDALDriver *poDriver = new GDALDriver();
    poDriver->SetDescription("RDB");
    poDriver->SetMetadataItem(GDAL_DCAP_RASTER, "YES");
    poDriver->SetMetadataItem(GDAL_DMD_LONGNAME, "RIEGL RDB Map Pixel (.mpx)");
    poDriver->SetMetadataItem(GDAL_DMD_HELPTOPIC,
                              "drivers/raster/rdb.html");
    poDriver->SetMetadataItem(GDAL_DMD_EXTENSION, "mpx");
    poDriver->pfnOpen = rdb::RDBDataset::Open;
    poDriver->pfnIdentify = rdb::RDBDataset::Identify;
    GetGDALDriverManager()->RegisterDriver(poDriver);
}

// includes the cpp wrapper of the rdb library
#include <riegl/rdb.cpp>
