/******************************************************************************
 * $Id$
 *
 * Project:  RIEGL RDB 2 driver
 * Purpose:  Add support for reading *.mpx RDB 2 files.
 * Author:   RIEGL Laser Measurement Systems GmbH (support@riegl.com)
 *
 ******************************************************************************
 * Copyright (c) 2018, RIEGL Laser Measurement Systems GmbH (support@riegl.com)
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
template <typename T> struct CPLMallocGuard
{
    T *const pData;
    CPLMallocGuard(std::size_t count)
        : pData(static_cast<T *>(CPLMalloc(sizeof(T) * count)))
    {
    }
    ~CPLMallocGuard() { CPLFree(pData); }
};

template <typename T> class RDBRasterBandInternal : public RDBRasterBand
{
  public:
    RDBRasterBandInternal(
        RDBDataset *poDSIn, const std::string &osAttributeNameIn,
        const riegl::rdb::pointcloud::PointAttribute &oPointAttributeIn,
        int nBandIn, GDALDataType eDataTypeIn)
        : RDBRasterBand(poDSIn, osAttributeNameIn, oPointAttributeIn, nBandIn,
                        eDataTypeIn)
    {
    }

    double GetNoDataValue(int *pbSuccess) override
    {
        double dfInvalidValue = RDBRasterBand::GetNoDataValue(pbSuccess);
        if(pbSuccess == nullptr)
        {
            return dfInvalidValue;
        }

        if(*pbSuccess == TRUE)
        {
            return dfInvalidValue;
        }
        else
        {
            if(oPointAttribute.maximumValue < std::numeric_limits<T>::max())
            {
                *pbSuccess = TRUE;
                return std::numeric_limits<T>::max();
            }
            else if(oPointAttribute.minimumValue >
                    std::numeric_limits<T>::lowest())
            {
                *pbSuccess = TRUE;
                return std::numeric_limits<T>::lowest();
            }
            // Another no data value could be any value that is actually not in
            // the data but in the range of specified rdb attribute. However,
            // this could maybe be a problem when combining multiple files.

            // Using always the maximum or minimum value in such cases might be
            // at least consistend across multiple files.

            *pbSuccess = FALSE;
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
            RDBDataset *poRdbDs = dynamic_cast<RDBDataset *>(poDS);
            if(poRdbDs != nullptr)
            {
                auto pIt = std::find_if(
                    poRdbDs->aoRDBNodes.begin(), poRdbDs->aoRDBNodes.end(),
                    [&](const RDBNode &poRdbNode) {
                        return poRdbNode.nXBlockCoordinates == nBlockXOff &&
                               poRdbNode.nYBlockCoordinates == nBlockYOff;
                    });

                if(pIt != poRdbDs->aoRDBNodes.end())
                {

                    using type = RDBCoordinatesPlusData<T>;
                    CPLMallocGuard<type> oData(pIt->nPointCount);

                    uint32_t nPointsReturned = 0;
                    {
                        // is locking needed?
                        // std::lock_guard<std::mutex>
                        // oGuard(poRdbDs->oLock);

                        auto oSelectQuery =
                            poRdbDs->oPointcloud.select(pIt->iID);
                        oSelectQuery.bindBuffer(
                            poRdbDs->oPointcloud.pointAttribute()
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
                        double dfTileMinX = poRdbDs->dfXMin +
                                            nBlockXOff * poRdbDs->dfSizeOfTile;
                        double dfTileMinY = poRdbDs->dfYMin +
                                            nBlockYOff * poRdbDs->dfSizeOfTile;

                        double dfHalveResolution = poRdbDs->dfResolution / 2;

                        for(uint32_t i = 0; i < nPointsReturned; i++)
                        {
                            int dfPixelX = static_cast<int>(
                                std::floor((oData.pData[i].adfCoordinates[0] +
                                            dfHalveResolution - dfTileMinX) /
                                           poRdbDs->dfSizeOfPixel));

                            int dfPixelY = static_cast<int>(
                                std::floor((oData.pData[i].adfCoordinates[1] +
                                            dfHalveResolution - dfTileMinY) /
                                           poRdbDs->dfSizeOfPixel));

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
};

RDBDataset::~RDBDataset() {}

void RDBDataset::SetBandInternal(
    RDBDataset *poDs, const std::string &osAttributeName,
    const riegl::rdb::pointcloud::PointAttribute &oPointAttribute,
    riegl::rdb::pointcloud::DataType eRdbDataType, int &nBandIndex)
{

    RDBRasterBand *poBand = nullptr;
    switch(eRdbDataType)
    {
    case riegl::rdb::pointcloud::DataType::UINT8:
    // Should I do ignore the other type?
    case riegl::rdb::pointcloud::DataType::INT8:
        poBand = new RDBRasterBandInternal<std::uint8_t>(
            poDs, osAttributeName, oPointAttribute, nBandIndex, GDT_Byte);
        break;
    case riegl::rdb::pointcloud::DataType::UINT16:
        poBand = new RDBRasterBandInternal<std::uint16_t>(
            poDs, osAttributeName, oPointAttribute, nBandIndex, GDT_UInt16);
        break;
    case riegl::rdb::pointcloud::DataType::INT16:
        poBand = new RDBRasterBandInternal<std::int16_t>(
            poDs, osAttributeName, oPointAttribute, nBandIndex, GDT_Int16);
        break;
    case riegl::rdb::pointcloud::DataType::UINT32:
        poBand = new RDBRasterBandInternal<std::uint32_t>(
            poDs, osAttributeName, oPointAttribute, nBandIndex, GDT_UInt32);
        break;
    case riegl::rdb::pointcloud::DataType::INT32:
        poBand = new RDBRasterBandInternal<std::int32_t>(
            poDs, osAttributeName, oPointAttribute, nBandIndex, GDT_Int32);
        break;
    case riegl::rdb::pointcloud::DataType::FLOAT32:
        poBand = new RDBRasterBandInternal<float>(
            poDs, osAttributeName, oPointAttribute, nBandIndex, GDT_Float32);
        break;
    case riegl::rdb::pointcloud::DataType::FLOAT64:
        poBand = new RDBRasterBandInternal<double>(
            poDs, osAttributeName, oPointAttribute, nBandIndex, GDT_Float64);
        break;
    default:
        // for all reamining use double. e.g. u/int64_t
        // an alternate option would be to check the data in the rdb and use the
        // minimum required data type. could be a problem when working with
        // multiple files.
        poBand = new RDBRasterBandInternal<double>(
            poDs, osAttributeName, oPointAttribute, nBandIndex, GDT_Float64);
        break;
    }

    poDs->SetBand(nBandIndex, poBand);
    nBandIndex++;
}

void RDBDataset::traverseRdbNodes(
    const riegl::rdb::pointcloud::GraphNode &oNode)
{
    if(oNode.children.size() == 0)
    {
        double adfNodeMinimum[2];
        oStatQuery.minimum(oNode.id,
                           oPointcloud.pointAttribute().primaryAttributeName(),
                           adfNodeMinimum[0]);

        int dfXNodeMin = static_cast<int>(
            std::floor((adfNodeMinimum[0] + dfSizeOfPixel / 2) / dfSizeOfTile));
        int dfYNodeMin = static_cast<int>(
            std::floor((adfNodeMinimum[1] + dfSizeOfPixel / 2) / dfSizeOfTile));

        RDBNode oRdbNode;

        oRdbNode.iID = oNode.id;
        oRdbNode.nPointCount = oNode.pointCountNode;
        oRdbNode.nXBlockCoordinates =
            dfXNodeMin - static_cast<int>(std::floor(dfXMin / dfSizeOfTile));
        oRdbNode.nYBlockCoordinates =
            dfYNodeMin - static_cast<int>(std::floor(dfYMin / dfSizeOfTile));

        aoRDBNodes.push_back(oRdbNode);
    }
    else
    {
        for(auto &&oChild : oNode.children)
        {
            traverseRdbNodes(oChild);
        }
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

    oStatQuery.minimum(1, oPrimaryAttribute, adfMinimum[0]);
    oStatQuery.maximum(1, oPrimaryAttribute, adfMaximum[0]);

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

    dfXMin = std::floor((adfMinimum[0] + dfSizeOfPixel / 2) / dfSizeOfTile) *
             dfSizeOfTile;
    dfYMin = std::floor((adfMinimum[1] + dfSizeOfPixel / 2) / dfSizeOfTile) *
             dfSizeOfTile;

    dfXMax = std::ceil((adfMaximum[0] - dfSizeOfPixel / 2) / dfSizeOfTile) *
             dfSizeOfTile;
    dfYMax = std::ceil((adfMaximum[1] - dfSizeOfPixel / 2) / dfSizeOfTile) *
             dfSizeOfTile;

    nRasterXSize = static_cast<int>((dfXMax - dfXMin) / dfSizeOfPixel);
    nRasterYSize = static_cast<int>((dfYMax - dfYMin) / dfSizeOfPixel);

    traverseRdbNodes(oStatQuery.index());

    riegl::rdb::pointcloud::PointAttributes &oPointAttribute =
        oPointcloud.pointAttribute();

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
                            oAttribute.dataType(), nBandIndex);
        }
        else
        {
            for(uint32_t i = 0; i < oAttribute.length; i++)
            {
                std::ostringstream oOss;
                oOss << osAttributeName << '[' << i << ']';
                SetBandInternal(this, oOss.str(), oAttribute,
                                oAttribute.dataType(), nBandIndex);
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
        RDBDataset *poDS = new RDBDataset(poOpenInfo);
        std::swap(poDS->fp, poOpenInfo->fpL);
        // Initialize any PAM information.
        poDS->SetDescription(poOpenInfo->pszFilename);
        poDS->TryLoadXML();

        return poDS;
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

    constexpr int kSizeOfRdbHeaderIdentifier = 32;
    constexpr char szRdbHeaderIdentifier[kSizeOfRdbHeaderIdentifier] =
        "RIEGL LMS RDB 2 POINTCLOUD FILE";

    if(strncmp(psHeader, szRdbHeaderIdentifier, kSizeOfRdbHeaderIdentifier))
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
const char *RDBDataset::_GetProjectionRef()
{
    ReadGeoreferencing();
    return osWktString.c_str();
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
    int nBandIn, GDALDataType eDataTypeIn)
    : osAttributeName(osAttributeNameIn), oPointAttribute(oPointAttributeIn)
{
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
    if(pbSuccess == nullptr)
    {
        return 0.0;
    }
    if(!std::isnan(oPointAttribute.invalidValue))
    {
        *pbSuccess = TRUE;
        return oPointAttribute.invalidValue;
    }
    else
    {
        *pbSuccess = FALSE;
        return 0;
    }
}
const char *RDBRasterBand::GetDescription() const
{
    return osAttributeName.c_str();
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
    poDriver->SetMetadataItem(GDAL_DMD_LONGNAME, "RIEGL RDB (.mpx)");
    // TODO:
    // poDriver->SetMetadataItem(GDAL_DMD_HELPTOPIC,
    //                          "");P
    poDriver->SetMetadataItem(GDAL_DMD_EXTENSION, "mpx");
    poDriver->pfnOpen = rdb::RDBDataset::Open;
    poDriver->pfnIdentify = rdb::RDBDataset::Identify;
    GetGDALDriverManager()->RegisterDriver(poDriver);
}

// includes the cpp wrapper of the rdb library
#include <riegl/rdb.cpp>
