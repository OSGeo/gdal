/******************************************************************************
 *
 * Project:  GDAL TileDB Driver
 * Purpose:  Implement GDAL TileDB Support based on https://www.tiledb.io
 * Author:   TileDB, Inc
 *
 ******************************************************************************
 * Copyright (c) 2023, TileDB, Inc
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

#include "tiledbheaders.h"

#include "cpl_json.h"
#include "cpl_time.h"
#include "ogr_p.h"
#include "ogr_recordbatch.h"
#include "ogr_swq.h"

#include <algorithm>
#include <limits>

constexpr int SECONDS_PER_DAY = 3600 * 24;
constexpr const char *GEOMETRY_DATASET_TYPE = "geometry";

/************************************************************************/
/* ==================================================================== */
/*                            ProcessField                              */
/* ==================================================================== */
/************************************************************************/

namespace
{

template <tiledb_datatype_t> struct GetType
{
};

template <> struct GetType<TILEDB_INT32>
{
    using EltType = std::vector<int32_t>;
};

template <> struct GetType<TILEDB_INT16>
{
    using EltType = std::vector<int16_t>;
};

template <> struct GetType<TILEDB_UINT8>
{
    using EltType = std::vector<uint8_t>;
};

template <> struct GetType<TILEDB_UINT16>
{
    using EltType = std::vector<uint16_t>;
};

template <> struct GetType<TILEDB_BOOL>
{
    using EltType = VECTOR_OF_BOOL;
};

template <> struct GetType<TILEDB_INT64>
{
    using EltType = std::vector<int64_t>;
};

template <> struct GetType<TILEDB_FLOAT64>
{
    using EltType = std::vector<double>;
};

template <> struct GetType<TILEDB_FLOAT32>
{
    using EltType = std::vector<float>;
};

template <> struct GetType<TILEDB_STRING_ASCII>
{
    using EltType = std::string;
};

template <> struct GetType<TILEDB_STRING_UTF8>
{
    using EltType = std::string;
};

template <> struct GetType<TILEDB_BLOB>
{
    using EltType = std::vector<uint8_t>;
};

template <> struct GetType<TILEDB_DATETIME_DAY>
{
    using EltType = std::vector<int64_t>;
};

template <> struct GetType<TILEDB_DATETIME_MS>
{
    using EltType = std::vector<int64_t>;
};

template <> struct GetType<TILEDB_TIME_MS>
{
    using EltType = std::vector<int64_t>;
};

template <template <class> class Func> struct ProcessField
{
    static void exec(tiledb_datatype_t eType, OGRTileDBLayer::ArrayType &array)
    {
        switch (eType)
        {
            case TILEDB_INT32:
                Func<GetType<TILEDB_INT32>::EltType>::exec(array);
                break;
            case TILEDB_INT16:
                Func<GetType<TILEDB_INT16>::EltType>::exec(array);
                break;
            case TILEDB_UINT8:
                Func<GetType<TILEDB_UINT8>::EltType>::exec(array);
                break;
            case TILEDB_UINT16:
                Func<GetType<TILEDB_UINT16>::EltType>::exec(array);
                break;
            case TILEDB_BOOL:
                Func<GetType<TILEDB_BOOL>::EltType>::exec(array);
                break;
            case TILEDB_INT64:
                Func<GetType<TILEDB_INT64>::EltType>::exec(array);
                break;
            case TILEDB_FLOAT32:
                Func<GetType<TILEDB_FLOAT32>::EltType>::exec(array);
                break;
            case TILEDB_FLOAT64:
                Func<GetType<TILEDB_FLOAT64>::EltType>::exec(array);
                break;
            case TILEDB_STRING_ASCII:
                Func<GetType<TILEDB_STRING_ASCII>::EltType>::exec(array);
                break;
            case TILEDB_STRING_UTF8:
                Func<GetType<TILEDB_STRING_UTF8>::EltType>::exec(array);
                break;
            case TILEDB_BLOB:
                Func<GetType<TILEDB_BLOB>::EltType>::exec(array);
                break;
            case TILEDB_DATETIME_DAY:
                Func<GetType<TILEDB_DATETIME_DAY>::EltType>::exec(array);
                break;
            case TILEDB_DATETIME_MS:
                Func<GetType<TILEDB_DATETIME_MS>::EltType>::exec(array);
                break;
            case TILEDB_TIME_MS:
                Func<GetType<TILEDB_TIME_MS>::EltType>::exec(array);
                break;

            default:
            {
                CPLAssert(false);
                break;
            }
        }
    }
};

}  // namespace

/************************************************************************/
/* ==================================================================== */
/*                            OGRTileDBDataset                          */
/* ==================================================================== */
/************************************************************************/

/************************************************************************/
/*                         OGRTileDBDataset()                           */
/************************************************************************/

OGRTileDBDataset::OGRTileDBDataset()
{
}

/************************************************************************/
/*                         ~OGRTileDBDataset()                          */
/************************************************************************/

OGRTileDBDataset::~OGRTileDBDataset()
{
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

GDALDataset *OGRTileDBDataset::Open(GDALOpenInfo *poOpenInfo,
                                    tiledb::Object::Type objectType)

{
    auto poDS = std::make_unique<OGRTileDBDataset>();
    poDS->eAccess = poOpenInfo->eAccess;
    const char *pszConfig =
        CSLFetchNameValue(poOpenInfo->papszOpenOptions, "TILEDB_CONFIG");

    const char *pszTimestamp = CSLFetchNameValueDef(
        poOpenInfo->papszOpenOptions, "TILEDB_TIMESTAMP", "0");
    const uint64_t nTimestamp = std::strtoull(pszTimestamp, nullptr, 10);

    if (pszConfig != nullptr)
    {
        tiledb::Config cfg(pszConfig);
        poDS->m_ctx.reset(new tiledb::Context(cfg));
    }
    else
    {
        tiledb::Config cfg;
        cfg["sm.enable_signal_handlers"] = "false";
        poDS->m_ctx.reset(new tiledb::Context(cfg));
    }

    std::string osFilename(
        TileDBDataset::VSI_to_tiledb_uri(poOpenInfo->pszFilename));
    if (osFilename.back() == '/')
        osFilename.pop_back();

    const auto AddLayer = [&poDS, nTimestamp, poOpenInfo](
                              const std::string &osLayerFilename,
                              const std::optional<std::string> &osLayerName =
                                  std::optional<std::string>())
    {
        auto poLayer = std::make_unique<OGRTileDBLayer>(
            poDS.get(), osLayerFilename.c_str(),
            osLayerName.has_value() ? (*osLayerName).c_str()
                                    : CPLGetBasename(osLayerFilename.c_str()),
            wkbUnknown, nullptr);
        poLayer->m_bUpdatable = poOpenInfo->eAccess == GA_Update;
        if (!poLayer->InitFromStorage(poDS->m_ctx.get(), nTimestamp,
                                      poOpenInfo->papszOpenOptions))
        {
            poLayer->m_array.reset();
            return false;
        }

        int nBatchSize = atoi(CSLFetchNameValueDef(poOpenInfo->papszOpenOptions,
                                                   "BATCH_SIZE", "0"));
        poLayer->m_nBatchSize =
            nBatchSize <= 0 ? DEFAULT_BATCH_SIZE : nBatchSize;

        poLayer->m_bStats =
            CPLFetchBool(poOpenInfo->papszOpenOptions, "STATS", false);

        poDS->m_apoLayers.emplace_back(std::move(poLayer));
        return true;
    };

    CPL_IGNORE_RET_VAL(objectType);
    if (objectType == tiledb::Object::Type::Group)
    {
        poDS->m_osGroupName = osFilename;
        tiledb::Group group(*(poDS->m_ctx), osFilename.c_str(), TILEDB_READ);
        for (uint64_t i = 0; i < group.member_count(); ++i)
        {
            auto obj = group.member(i);
            if (obj.type() == tiledb::Object::Type::Array)
            {
                tiledb::ArraySchema schema(*(poDS->m_ctx), obj.uri());
                if (schema.array_type() == TILEDB_SPARSE)
                {
                    AddLayer(obj.uri(), obj.name());
                }
            }
        }
    }
    else
    {
        if (!AddLayer(osFilename))
            return nullptr;
    }

    return poDS.release();
}

/************************************************************************/
/*                           TestCapability()                           */
/************************************************************************/

int OGRTileDBDataset::TestCapability(const char *pszCap)
{
    if (EQUAL(pszCap, ODsCCreateLayer))
    {
        return eAccess == GA_Update &&
               (m_apoLayers.empty() || !m_osGroupName.empty());
    }
    if (EQUAL(pszCap, ODsCCurveGeometries) ||
        EQUAL(pszCap, ODsCMeasuredGeometries) || EQUAL(pszCap, ODsCZGeometries))
    {
        return TRUE;
    }
    return FALSE;
}

/***********************************************************************/
/*                            ExecuteSQL()                             */
/***********************************************************************/

OGRLayer *OGRTileDBDataset::ExecuteSQL(const char *pszSQLCommand,
                                       OGRGeometry *poSpatialFilter,
                                       const char *pszDialect)
{
    return GDALDataset::ExecuteSQL(pszSQLCommand, poSpatialFilter, pszDialect);
}

/***********************************************************************/
/*                           ICreateLayer()                            */
/***********************************************************************/
OGRLayer *
OGRTileDBDataset::ICreateLayer(const char *pszName,
                               const OGRGeomFieldDefn *poGeomFieldDefn,
                               CSLConstList papszOptions)
{
    if (eAccess != GA_Update)
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "CreateLayer() failed: dataset in read-only mode");
        return nullptr;
    }

    if (!m_osGroupName.empty() && strchr(pszName, '/'))
    {
        // Otherwise a layer name wit ha slash when groups are enabled causes
        // a "[TileDB::Array] Error: FragmentID: input URI is invalid. Provided URI does not contain a fragment name."
        // exception on re-opening starting with TileDB 2.21
        CPLError(CE_Failure, CPLE_NotSupported,
                 "Slash is not supported in layer name");
        return nullptr;
    }

    const auto eGType = poGeomFieldDefn ? poGeomFieldDefn->GetType() : wkbNone;
    const auto poSpatialRef =
        poGeomFieldDefn ? poGeomFieldDefn->GetSpatialRef() : nullptr;

    if (m_osGroupName.empty() && !m_apoLayers.empty())
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "CreateLayer() failed: no more than one layer per dataset "
                 "supported on a array object. Create a dataset with the "
                 "CREATE_GROUP=YES creation option or open such group "
                 "to enable multiple layer creation.");
        return nullptr;
    }

    if (eGType == wkbNone)
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "CreateLayer() failed: non-spatial layer not supported");
        return nullptr;
    }

    std::string osFilename = GetDescription();
    if (!m_osGroupName.empty())
    {
        osFilename = CPLFormFilename(m_osGroupName.c_str(), "layers", nullptr);
        if (!STARTS_WITH(m_osGroupName.c_str(), "s3://") &&
            !STARTS_WITH(m_osGroupName.c_str(), "gcs://"))
        {
            VSIStatBufL sStat;
            if (VSIStatL(osFilename.c_str(), &sStat) != 0)
                VSIMkdir(osFilename.c_str(), 0755);
        }
        osFilename = CPLFormFilename(osFilename.c_str(), pszName, nullptr);
    }
    auto poLayer = std::make_unique<OGRTileDBLayer>(
        this, osFilename.c_str(), pszName, eGType, poSpatialRef);
    poLayer->m_bUpdatable = true;
    poLayer->m_ctx.reset(new tiledb::Context(*m_ctx));
    poLayer->m_osGroupName = m_osGroupName;

    const char *pszBounds = CSLFetchNameValue(papszOptions, "BOUNDS");
    if (pszBounds)
    {
        CPLStringList aosBounds(CSLTokenizeString2(pszBounds, ",", 0));
        if (aosBounds.Count() == 4)
        {
            poLayer->m_dfXStart = CPLAtof(aosBounds[0]);
            poLayer->m_dfYStart = CPLAtof(aosBounds[1]);
            poLayer->m_dfXEnd = CPLAtof(aosBounds[2]);
            poLayer->m_dfYEnd = CPLAtof(aosBounds[3]);
        }
        else if (aosBounds.Count() == 6)
        {
            poLayer->m_dfXStart = CPLAtof(aosBounds[0]);
            poLayer->m_dfYStart = CPLAtof(aosBounds[1]);
            poLayer->m_dfZStart = CPLAtof(aosBounds[2]);
            poLayer->m_dfXEnd = CPLAtof(aosBounds[3]);
            poLayer->m_dfYEnd = CPLAtof(aosBounds[4]);
            poLayer->m_dfZEnd = CPLAtof(aosBounds[5]);
        }
        else
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Domain bounds specified as minx,miny,maxx,maxy or "
                     "minx,miny,minx,maxx,maxy,maxz are "
                     "required for array creation.");
            return nullptr;
        }
    }
    else if (poSpatialRef && poSpatialRef->IsGeographic())
    {
        poLayer->m_dfXStart = -360;
        poLayer->m_dfXEnd = 360;
        poLayer->m_dfYStart = -90;
        poLayer->m_dfYEnd = 90;
    }
    else if (poSpatialRef && poSpatialRef->IsProjected())
    {
        // Should hopefully be sufficiently large for most projections...
        // For example the eastings of Mercator go between [-PI * a, PI * a]
        // so we take a 2x margin here.
        const double dfBounds = 2 * M_PI * poSpatialRef->GetSemiMajor();
        poLayer->m_dfXStart = -dfBounds;
        poLayer->m_dfXEnd = dfBounds;
        poLayer->m_dfYStart = -dfBounds;
        poLayer->m_dfYEnd = dfBounds;
    }
    else
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Domain bounds must be specified with the BOUNDS layer "
                 "creation option.");
        return nullptr;
    }

    int nBatchSize =
        atoi(CSLFetchNameValueDef(papszOptions, "BATCH_SIZE", "0"));
    poLayer->m_nBatchSize = nBatchSize <= 0 ? DEFAULT_BATCH_SIZE : nBatchSize;

    int nTileCapacity =
        atoi(CSLFetchNameValueDef(papszOptions, "TILE_CAPACITY", "0"));
    poLayer->m_nTileCapacity =
        nTileCapacity <= 0 ? DEFAULT_TILE_CAPACITY : nTileCapacity;

    poLayer->m_bStats = CPLFetchBool(papszOptions, "STATS", false);

    poLayer->m_dfTileExtent =
        std::min(poLayer->m_dfYEnd - poLayer->m_dfYStart,
                 poLayer->m_dfXEnd - poLayer->m_dfXStart) /
        10;

    const char *pszTileExtent = CSLFetchNameValue(papszOptions, "TILE_EXTENT");
    if (pszTileExtent)
        poLayer->m_dfTileExtent = CPLAtof(pszTileExtent);

    if (wkbHasZ(eGType) || eGType == wkbUnknown)
    {
        poLayer->m_osZDim = "_Z";
        poLayer->m_dfZTileExtent =
            (poLayer->m_dfZEnd - poLayer->m_dfZStart) / 2;

        const char *pszZTileExtent =
            CSLFetchNameValue(papszOptions, "TILE_Z_EXTENT");
        if (pszZTileExtent)
            poLayer->m_dfZTileExtent = CPLAtof(pszZTileExtent);
    }

    const char *pszAddZDim = CSLFetchNameValue(papszOptions, "ADD_Z_DIM");
    if (pszAddZDim && !EQUAL(pszAddZDim, "AUTO") && !CPLTestBool(pszAddZDim))
        poLayer->m_osZDim.clear();

    const char *pszTimestamp =
        CSLFetchNameValueDef(papszOptions, "TILEDB_TIMESTAMP", "0");
    poLayer->m_nTimestamp = std::strtoull(pszTimestamp, nullptr, 10);

    const char *pszCompression = CSLFetchNameValue(papszOptions, "COMPRESSION");
    const char *pszCompressionLevel =
        CSLFetchNameValue(papszOptions, "COMPRESSION_LEVEL");

    poLayer->m_filterList.reset(new tiledb::FilterList(*poLayer->m_ctx));
    if (pszCompression != nullptr)
    {
        int nLevel = (pszCompressionLevel) ? atoi(pszCompressionLevel) : -1;
        TileDBDataset::AddFilter(*(poLayer->m_ctx.get()),
                                 *(poLayer->m_filterList.get()), pszCompression,
                                 nLevel);
    }

    poLayer->m_osFIDColumn = CSLFetchNameValueDef(papszOptions, "FID", "FID");

    const char *pszGeomColName =
        CSLFetchNameValueDef(papszOptions, "GEOMETRY_NAME", "wkb_geometry");
    if (EQUAL(pszGeomColName, "") && wkbFlatten(eGType) != wkbPoint)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "GEOMETRY_NAME must be defined to a non-empty string "
                 "for layers whose geometry type is not Point.");
        return nullptr;
    }
    poLayer->m_poFeatureDefn->GetGeomFieldDefn(0)->SetName(pszGeomColName);

    poLayer->m_eCurrentMode = OGRTileDBLayer::CurrentMode::WriteInProgress;

    const char *pszTileDBStringType =
        CSLFetchNameValue(papszOptions, "TILEDB_STRING_TYPE");
    if (pszTileDBStringType)
    {
        if (EQUAL(pszTileDBStringType, "ASCII"))
            poLayer->m_eTileDBStringType = TILEDB_STRING_ASCII;
        else if (EQUAL(pszTileDBStringType, "UTF8"))
            poLayer->m_eTileDBStringType = TILEDB_STRING_UTF8;
    }

    m_apoLayers.emplace_back(std::move(poLayer));

    return m_apoLayers.back().get();
}

/************************************************************************/
/*                          Create()                                    */
/************************************************************************/

GDALDataset *OGRTileDBDataset::Create(const char *pszFilename,
                                      CSLConstList papszOptions)
{
    auto poDS = std::make_unique<OGRTileDBDataset>();
    poDS->SetDescription(TileDBDataset::VSI_to_tiledb_uri(pszFilename));
    poDS->eAccess = GA_Update;

    const char *pszConfig = CSLFetchNameValue(papszOptions, "TILEDB_CONFIG");
    if (pszConfig != nullptr)
    {
        tiledb::Config cfg(pszConfig);
        poDS->m_ctx.reset(new tiledb::Context(cfg));
    }
    else
    {
        poDS->m_ctx.reset(new tiledb::Context());
    }

    if (CPLTestBool(CSLFetchNameValueDef(papszOptions, "CREATE_GROUP", "NO")))
    {
        try
        {
            tiledb::create_group(*(poDS->m_ctx.get()), poDS->GetDescription());
        }
        catch (const std::exception &e)
        {
            CPLError(CE_Failure, CPLE_AppDefined, "%s", e.what());
            return nullptr;
        }
        poDS->m_osGroupName = poDS->GetDescription();
    }

    return poDS.release();
}

/************************************************************************/
/* ==================================================================== */
/*                            OGRTileDBLayer                            */
/* ==================================================================== */
/************************************************************************/

/************************************************************************/
/*                         OGRTileDBLayer()                             */
/************************************************************************/

OGRTileDBLayer::OGRTileDBLayer(GDALDataset *poDS, const char *pszFilename,
                               const char *pszLayerName,
                               const OGRwkbGeometryType eGType,
                               const OGRSpatialReference *poSRS)
    : m_poDS(poDS), m_osFilename(pszFilename),
      m_poFeatureDefn(new OGRFeatureDefn(pszLayerName)),
      m_pbLayerStillAlive(std::make_shared<bool>(true)),
      m_anFIDs(std::make_shared<std::vector<int64_t>>()),
      m_adfXs(std::make_shared<std::vector<double>>()),
      m_adfYs(std::make_shared<std::vector<double>>()),
      m_adfZs(std::make_shared<std::vector<double>>()),
      m_abyGeometries(std::make_shared<std::vector<unsigned char>>()),
      m_anGeometryOffsets(std::make_shared<std::vector<uint64_t>>())
{
    m_poFeatureDefn->SetGeomType(eGType);

    if (poSRS)
    {
        auto poSRSClone = poSRS->Clone();
        m_poFeatureDefn->GetGeomFieldDefn(0)->SetSpatialRef(poSRSClone);
        poSRSClone->Release();
    }

    m_poFeatureDefn->Reference();

    SetDescription(pszLayerName);
}

/************************************************************************/
/*                         ~OGRTileDBLayer()                            */
/************************************************************************/

OGRTileDBLayer::~OGRTileDBLayer()
{
    *m_pbLayerStillAlive = false;

    try
    {
        if (m_bUpdatable && !m_bInitializationAttempted && m_filterList)
        {
            InitializeSchemaAndArray();
        }
        if (m_array && m_bUpdatable)
        {
            SwitchToWritingMode();
        }
        if (m_array)
        {
            if (m_bUpdatable)
            {
                if (m_bInitialized && !m_adfXs->empty())
                {
                    FlushArrays();
                }

                // write the pad metadata
                m_array->put_metadata("PAD_X", TILEDB_FLOAT64, 1, &m_dfPadX);
                m_array->put_metadata("PAD_Y", TILEDB_FLOAT64, 1, &m_dfPadY);
                if (m_dfPadZ != 0)
                    m_array->put_metadata("PAD_Z", TILEDB_FLOAT64, 1,
                                          &m_dfPadZ);

                if (m_nTotalFeatureCount >= 0)
                    m_array->put_metadata("FEATURE_COUNT", TILEDB_INT64, 1,
                                          &m_nTotalFeatureCount);

                if (m_oLayerExtent.IsInit())
                {
                    m_array->put_metadata("LAYER_EXTENT_MINX", TILEDB_FLOAT64,
                                          1, &m_oLayerExtent.MinX);
                    m_array->put_metadata("LAYER_EXTENT_MINY", TILEDB_FLOAT64,
                                          1, &m_oLayerExtent.MinY);
                    m_array->put_metadata("LAYER_EXTENT_MAXX", TILEDB_FLOAT64,
                                          1, &m_oLayerExtent.MaxX);
                    m_array->put_metadata("LAYER_EXTENT_MAXY", TILEDB_FLOAT64,
                                          1, &m_oLayerExtent.MaxY);
                }
            }

            m_array->close();
        }
    }
    catch (const std::exception &e)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "%s", e.what());
    }

    if (m_poFeatureDefn)
        m_poFeatureDefn->Release();
}

/************************************************************************/
/*                         InitFromStorage()                            */
/************************************************************************/

bool OGRTileDBLayer::InitFromStorage(tiledb::Context *poCtx,
                                     uint64_t nTimestamp,
                                     CSLConstList papszOpenOptions)
{
    m_bInitialized = true;
    m_bInitializationAttempted = true;
    m_ctx.reset(new tiledb::Context(*poCtx));
    m_schema.reset(new tiledb::ArraySchema(*m_ctx, m_osFilename));
    m_nTimestamp = nTimestamp;

    m_filterList.reset(new tiledb::FilterList(*m_ctx));

    CPLJSONObject oJson;
    CPLJSONObject oSchema;
    oJson.Add("schema", oSchema);

    {
        const auto filters = m_schema->coords_filter_list();
        CPLJSONArray oCoordsFilterList;
        for (uint32_t j = 0; j < filters.nfilters(); ++j)
        {
            const auto filter = filters.filter(j);
            oCoordsFilterList.Add(tiledb::Filter::to_str(filter.filter_type()));
        }
        oSchema.Add("coords_filter_list", oCoordsFilterList);
    }

    if (m_nTimestamp)
        m_array.reset(new tiledb::Array(
            *m_ctx, m_osFilename, TILEDB_READ,
            tiledb::TemporalPolicy(tiledb::TimeTravel, m_nTimestamp)));
    else
        m_array.reset(new tiledb::Array(*m_ctx, m_osFilename, TILEDB_READ));

    const auto domain = m_schema->domain();
    if (domain.ndim() < 2)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Domain should have at least 2 dimensions");
        return false;
    }

    const auto CreateField = [this](const std::string &osName,
                                    tiledb_datatype_t type, bool bIsSingle,
                                    bool bIsNullable)
    {
        bool bOK = true;
        OGRFieldType eType = OFTString;
        OGRFieldSubType eSubType = OFSTNone;
        auto &fieldValues = m_aFieldValues;
        switch (type)
        {
            case TILEDB_UINT16:
                eType = bIsSingle ? OFTInteger : OFTIntegerList;
                fieldValues.push_back(
                    std::make_shared<std::vector<uint16_t>>());
                break;
            case TILEDB_INT32:
                eType = bIsSingle ? OFTInteger : OFTIntegerList;
                fieldValues.push_back(std::make_shared<std::vector<int32_t>>());
                break;
            case TILEDB_INT64:
                eType = bIsSingle ? OFTInteger64 : OFTInteger64List;
                fieldValues.push_back(std::make_shared<std::vector<int64_t>>());
                break;
            case TILEDB_FLOAT32:
                eType = bIsSingle ? OFTReal : OFTRealList;
                eSubType = OFSTFloat32;
                fieldValues.push_back(std::make_shared<std::vector<float>>());
                break;
            case TILEDB_FLOAT64:
                eType = bIsSingle ? OFTReal : OFTRealList;
                fieldValues.push_back(std::make_shared<std::vector<double>>());
                break;
            case TILEDB_INT16:
                eType = bIsSingle ? OFTInteger : OFTIntegerList;
                eSubType = OFSTInt16;
                fieldValues.push_back(std::make_shared<std::vector<int16_t>>());
                break;
            case TILEDB_STRING_ASCII:
            case TILEDB_STRING_UTF8:
                eType = OFTString;
                fieldValues.push_back(std::make_shared<std::string>());
                break;
            case TILEDB_BOOL:
                eType = bIsSingle ? OFTInteger : OFTIntegerList;
                eSubType = OFSTBoolean;
                fieldValues.push_back(std::make_shared<VECTOR_OF_BOOL>());
                break;
            case TILEDB_DATETIME_DAY:
                eType = OFTDate;
                fieldValues.push_back(std::make_shared<std::vector<int64_t>>());
                break;
            case TILEDB_DATETIME_MS:
                eType = OFTDateTime;
                fieldValues.push_back(std::make_shared<std::vector<int64_t>>());
                break;
            case TILEDB_TIME_MS:
                eType = OFTTime;
                fieldValues.push_back(std::make_shared<std::vector<int64_t>>());
                break;
            case TILEDB_UINT8:
                eType = bIsSingle ? OFTInteger : OFTBinary;
                fieldValues.push_back(std::make_shared<std::vector<uint8_t>>());
                break;
            case TILEDB_BLOB:
            {
                if (bIsSingle)
                {
                    bOK = false;
                    const char *pszTypeName = "";
                    tiledb_datatype_to_str(type, &pszTypeName);
                    CPLError(
                        CE_Warning, CPLE_AppDefined,
                        "Ignoring attribute %s of type %s, as only "
                        "variable length is supported, but it has a fixed size",
                        osName.c_str(), pszTypeName);
                }
                else
                {
                    eType = OFTBinary;
                    fieldValues.push_back(
                        std::make_shared<std::vector<uint8_t>>());
                }
                break;
            }
            case TILEDB_CHAR:
            case TILEDB_INT8:
            case TILEDB_UINT32:
            case TILEDB_UINT64:
            case TILEDB_STRING_UTF16:
            case TILEDB_STRING_UTF32:
            case TILEDB_STRING_UCS2:
            case TILEDB_STRING_UCS4:
            case TILEDB_DATETIME_YEAR:
            case TILEDB_DATETIME_MONTH:
            case TILEDB_DATETIME_WEEK:
            case TILEDB_DATETIME_HR:
            case TILEDB_DATETIME_MIN:
            case TILEDB_DATETIME_SEC:
            case TILEDB_DATETIME_US:
            case TILEDB_DATETIME_NS:
            case TILEDB_DATETIME_PS:
            case TILEDB_DATETIME_FS:
            case TILEDB_DATETIME_AS:
            case TILEDB_TIME_HR:
            case TILEDB_TIME_MIN:
            case TILEDB_TIME_SEC:
            case TILEDB_TIME_US:
            case TILEDB_TIME_NS:
            case TILEDB_TIME_PS:
            case TILEDB_TIME_FS:
            case TILEDB_TIME_AS:
            case TILEDB_ANY:
#ifdef HAS_TILEDB_GEOM_WKB_WKT
            case TILEDB_GEOM_WKB:  // TODO: take that into account
            case TILEDB_GEOM_WKT:
#endif
            {
                // TODO ?
                const char *pszTypeName = "";
                tiledb_datatype_to_str(type, &pszTypeName);
                CPLError(CE_Warning, CPLE_AppDefined,
                         "Ignoring attribute %s as its type %s is unsupported",
                         osName.c_str(), pszTypeName);
                bOK = false;
                break;
            }
        }
        if (bOK)
        {
            m_aeFieldTypes.push_back(type);
            OGRFieldDefn oFieldDefn(osName.c_str(), eType);
            oFieldDefn.SetSubType(eSubType);
            oFieldDefn.SetNullable(bIsNullable);
            m_poFeatureDefn->AddFieldDefn(&oFieldDefn);
        }
    };

    // Figure out dimensions
    m_osXDim.clear();
    m_osYDim.clear();

    // to improve interoperability with PDAL generated datasets
    const bool bDefaultDimNameWithoutUnderscore =
        !CSLFetchNameValue(papszOpenOptions, "DIM_X") &&
        !CSLFetchNameValue(papszOpenOptions, "DIM_Y") &&
        !CSLFetchNameValue(papszOpenOptions, "DIM_Z") &&
        !domain.has_dimension("_X") && !domain.has_dimension("_Y") &&
        domain.has_dimension("X") && domain.has_dimension("Y");

    const std::string osXDim =
        CSLFetchNameValueDef(papszOpenOptions, "DIM_X",
                             bDefaultDimNameWithoutUnderscore ? "X" : "_X");
    const std::string osYDim =
        CSLFetchNameValueDef(papszOpenOptions, "DIM_Y",
                             bDefaultDimNameWithoutUnderscore ? "Y" : "_Y");
    const std::string osZDim =
        CSLFetchNameValueDef(papszOpenOptions, "DIM_Z",
                             bDefaultDimNameWithoutUnderscore ? "Z" : "_Z");
    for (unsigned int i = 0; i < domain.ndim(); ++i)
    {
        auto dim = domain.dimension(i);
        if (dim.name() == osXDim)
        {
            m_osXDim = dim.name();
            if (dim.type() != TILEDB_FLOAT64)
            {
                const char *pszTypeName = "";
                tiledb_datatype_to_str(dim.type(), &pszTypeName);
                CPLError(CE_Failure, CPLE_AppDefined,
                         "Did not get expected type for %s dimension of "
                         "domain. Got %s, expected FLOAT64",
                         dim.name().c_str(), pszTypeName);
                return false;
            }
            const auto dimdomain = dim.domain<double>();
            m_dfXStart = dimdomain.first;
            m_dfXEnd = dimdomain.second;
        }
        else if (dim.name() == osYDim)
        {
            m_osYDim = dim.name();
            if (dim.type() != TILEDB_FLOAT64)
            {
                const char *pszTypeName = "";
                tiledb_datatype_to_str(dim.type(), &pszTypeName);
                CPLError(CE_Failure, CPLE_AppDefined,
                         "Did not get expected type for %s dimension of "
                         "domain. Got %s, expected FLOAT64",
                         dim.name().c_str(), pszTypeName);
                return false;
            }
            const auto dimdomain = dim.domain<double>();
            m_dfYStart = dimdomain.first;
            m_dfYEnd = dimdomain.second;
        }
        else if (dim.name() == osZDim)
        {
            m_osZDim = dim.name();
            if (dim.type() != TILEDB_FLOAT64)
            {
                const char *pszTypeName = "";
                tiledb_datatype_to_str(dim.type(), &pszTypeName);
                CPLError(CE_Failure, CPLE_AppDefined,
                         "Did not get expected type for %s dimension of "
                         "domain. Got %s, expected FLOAT64",
                         dim.name().c_str(), pszTypeName);
                return false;
            }
            const auto dimdomain = dim.domain<double>();
            m_dfZStart = dimdomain.first;
            m_dfZEnd = dimdomain.second;
        }
        else
        {
            CreateField(dim.name(), dim.type(), /*bIsSingle=*/true,
                        /*bIsNullable=*/false);
        }
    }
    if (m_osXDim.empty())
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Did not get expected _X dimension of domain");
        return false;
    }
    if (m_osYDim.empty())
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Did not get expected _Y dimension of domain");
        return false;
    }

    tiledb_datatype_t v_type = TILEDB_FLOAT64;
    const void *v_r = nullptr;
    uint32_t v_num = 0;
    std::string osFIDColumn = "FID";
    m_array->get_metadata("FID_ATTRIBUTE_NAME", &v_type, &v_num, &v_r);
    if (v_r && (v_type == TILEDB_UINT8 || v_type == TILEDB_CHAR ||
                v_type == TILEDB_STRING_ASCII || v_type == TILEDB_STRING_UTF8))
    {
        osFIDColumn.assign(static_cast<const char *>(v_r), v_num);
    }

    std::string osGeomColumn = "wkb_geometry";
    m_array->get_metadata("GEOMETRY_ATTRIBUTE_NAME", &v_type, &v_num, &v_r);
    if (v_r && (v_type == TILEDB_UINT8 || v_type == TILEDB_CHAR ||
                v_type == TILEDB_STRING_ASCII || v_type == TILEDB_STRING_UTF8))
    {
        osGeomColumn.assign(static_cast<const char *>(v_r), v_num);
    }

    bool bFoundWkbGeometry = false;
    CPLJSONArray oAttributes;
    oSchema.Add("attributes", oAttributes);
    for (unsigned i = 0; i < m_schema->attribute_num(); ++i)
    {
        auto attr = m_schema->attribute(i);

        // Export attribute in json:TILEDB metadata domain, mostly for unit
        // testing purposes
        {
            CPLJSONObject oAttribute;
            oAttributes.Add(oAttribute);
            oAttribute.Set("name", attr.name());
            const char *pszTypeName = "";
            tiledb_datatype_to_str(attr.type(), &pszTypeName);
            oAttribute.Set("type", pszTypeName);
            if (attr.cell_val_num() == TILEDB_VAR_NUM)
                oAttribute.Set("cell_val_num", "variable");
            else
                oAttribute.Set("cell_val_num",
                               static_cast<GIntBig>(attr.cell_val_num()));
            oAttribute.Set("nullable", attr.nullable());

            const auto filters = attr.filter_list();
            CPLJSONArray oFilterList;
            for (uint32_t j = 0; j < filters.nfilters(); ++j)
            {
                const auto filter = filters.filter(j);
                oFilterList.Add(tiledb::Filter::to_str(filter.filter_type()));
            }
            oAttribute.Add("filter_list", oFilterList);
        }

        if (attr.name() == osFIDColumn && attr.type() == TILEDB_INT64)
        {
            m_osFIDColumn = attr.name();
            continue;
        }
        if (attr.name() == osGeomColumn &&
            (attr.type() == TILEDB_UINT8 || attr.type() == TILEDB_BLOB) &&
            attr.cell_val_num() == TILEDB_VAR_NUM)
        {
            bFoundWkbGeometry = true;
            continue;
        }
        const bool bIsSingle = attr.cell_val_num() == 1;
        if (attr.cell_val_num() > 1 && attr.cell_val_num() != TILEDB_VAR_NUM)
        {
            CPLError(CE_Warning, CPLE_NotSupported,
                     "Ignoring attribute %s as it has a number of values per "
                     "cell that is not 1 neither variable size",
                     attr.name().c_str());
            continue;
        }
        CreateField(attr.name(), attr.type(), bIsSingle, attr.nullable());
    }

    if (bFoundWkbGeometry)
        m_poFeatureDefn->GetGeomFieldDefn(0)->SetName(osGeomColumn.c_str());

    for (int i = 0; i < m_poFeatureDefn->GetFieldCount(); ++i)
        m_aFieldValueOffsets.push_back(
            std::make_shared<std::vector<uint64_t>>());
    m_aFieldValidity.resize(m_poFeatureDefn->GetFieldCount());

    m_array->get_metadata("PAD_X", &v_type, &v_num, &v_r);
    if (v_r && v_type == TILEDB_FLOAT64 && v_num == 1)
    {
        m_dfPadX = *static_cast<const double *>(v_r);
    }

    m_array->get_metadata("PAD_Y", &v_type, &v_num, &v_r);
    if (v_r && v_type == TILEDB_FLOAT64 && v_num == 1)
    {
        m_dfPadY = *static_cast<const double *>(v_r);
    }

    m_array->get_metadata("PAD_Z", &v_type, &v_num, &v_r);
    if (v_r && v_type == TILEDB_FLOAT64 && v_num == 1)
    {
        m_dfPadZ = *static_cast<const double *>(v_r);
    }

    m_array->get_metadata("FEATURE_COUNT", &v_type, &v_num, &v_r);
    if (v_r && v_type == TILEDB_INT64 && v_num == 1)
    {
        m_nTotalFeatureCount = *static_cast<const int64_t *>(v_r);
    }

    m_array->get_metadata("LAYER_EXTENT_MINX", &v_type, &v_num, &v_r);
    if (v_r && v_type == TILEDB_FLOAT64 && v_num == 1)
    {
        m_oLayerExtent.MinX = *static_cast<const double *>(v_r);
    }

    m_array->get_metadata("LAYER_EXTENT_MINY", &v_type, &v_num, &v_r);
    if (v_r && v_type == TILEDB_FLOAT64 && v_num == 1)
    {
        m_oLayerExtent.MinY = *static_cast<const double *>(v_r);
    }

    m_array->get_metadata("LAYER_EXTENT_MAXX", &v_type, &v_num, &v_r);
    if (v_r && v_type == TILEDB_FLOAT64 && v_num == 1)
    {
        m_oLayerExtent.MaxX = *static_cast<const double *>(v_r);
    }

    m_array->get_metadata("LAYER_EXTENT_MAXY", &v_type, &v_num, &v_r);
    if (v_r && v_type == TILEDB_FLOAT64 && v_num == 1)
    {
        m_oLayerExtent.MaxY = *static_cast<const double *>(v_r);
    }

    m_array->get_metadata("CRS", &v_type, &v_num, &v_r);
    if (v_r && (v_type == TILEDB_UINT8 || v_type == TILEDB_CHAR ||
                v_type == TILEDB_STRING_ASCII || v_type == TILEDB_STRING_UTF8))
    {
        std::string osStr;
        osStr.assign(static_cast<const char *>(v_r), v_num);
        OGRSpatialReference *poSRS = new OGRSpatialReference();
        poSRS->SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);
        if (poSRS->SetFromUserInput(
                osStr.c_str(),
                OGRSpatialReference::SET_FROM_USER_INPUT_LIMITATIONS_get()) !=
            OGRERR_NONE)
        {
            poSRS->Release();
            poSRS = nullptr;
        }
        if (poSRS)
        {
            m_poFeatureDefn->GetGeomFieldDefn(0)->SetSpatialRef(poSRS);
            poSRS->Release();
        }
    }

    m_array->get_metadata("GeometryType", &v_type, &v_num, &v_r);
    if (v_r && (v_type == TILEDB_UINT8 || v_type == TILEDB_CHAR ||
                v_type == TILEDB_STRING_ASCII || v_type == TILEDB_STRING_UTF8))
    {
        std::string osStr;
        osStr.assign(static_cast<const char *>(v_r), v_num);
        OGRwkbGeometryType eGeomType = wkbUnknown;
        OGRReadWKTGeometryType(osStr.c_str(), &eGeomType);
        m_poFeatureDefn->GetGeomFieldDefn(0)->SetType(eGeomType);
    }
    else if (!bFoundWkbGeometry)
    {
        m_poFeatureDefn->GetGeomFieldDefn(0)->SetType(
            m_osZDim.empty() ? wkbPoint : wkbPoint25D);
    }

    // Export array metadata in json:TILEDB metadata domain, mostly for
    // unit testing purposes
    CPLJSONObject oArray;
    oJson.Add("array", oArray);
    CPLJSONObject oMetadata;
    oArray.Add("metadata", oMetadata);
    for (uint64_t i = 0; i < m_array->metadata_num(); ++i)
    {
        std::string osKey;
        m_array->get_metadata_from_index(i, &osKey, &v_type, &v_num, &v_r);
        CPLJSONObject oMDItem;
        oMetadata.Add(osKey, oMDItem);

        const char *pszTypeName = "";
        tiledb_datatype_to_str(v_type, &pszTypeName);
        oMDItem.Set("type", pszTypeName);

        switch (v_type)
        {
            case TILEDB_INT32:
                if (v_num == 1)
                    oMDItem.Set("value", *static_cast<const int32_t *>(v_r));
                break;
            case TILEDB_INT64:
                if (v_num == 1)
                    oMDItem.Set("value",
                                static_cast<GIntBig>(
                                    *static_cast<const int64_t *>(v_r)));
                break;
            case TILEDB_FLOAT64:
                if (v_num == 1)
                    oMDItem.Set("value", *static_cast<const double *>(v_r));
                break;
            case TILEDB_STRING_ASCII:
            case TILEDB_STRING_UTF8:
            {
                std::string osStr;
                osStr.append(static_cast<const char *>(v_r), v_num);
                if (osStr.find("$schema") != std::string::npos)
                {
                    // PROJJSON typically
                    CPLJSONDocument oDoc;
                    if (oDoc.LoadMemory(osStr))
                    {
                        oMDItem.Add("value", oDoc.GetRoot());
                    }
                    else
                    {
                        oMDItem.Set("value", osStr);
                    }
                }
                else
                {
                    oMDItem.Set("value", osStr);
                }
                break;
            }
            default:
                // other types unhandled for now
                break;
        }
    }

    char *apszMD[] = {nullptr, nullptr};
    std::string osJsonMD = oJson.Format(CPLJSONObject::PrettyFormat::Plain);
    apszMD[0] = osJsonMD.data();
    SetMetadata(apszMD, "json:TILEDB");

    return true;
}

/************************************************************************/
/*                     GetDatabaseGeomColName()                         */
/************************************************************************/

const char *OGRTileDBLayer::GetDatabaseGeomColName()
{
    const char *pszGeomColName = GetGeometryColumn();
    if (pszGeomColName && pszGeomColName[0] == 0)
        pszGeomColName = nullptr;
    return pszGeomColName;
}

/************************************************************************/
/*                        SetReadBuffers()                              */
/************************************************************************/

void OGRTileDBLayer::SetReadBuffers(bool bGrowVariableSizeArrays)
{
    const auto GetValueSize =
        [this, bGrowVariableSizeArrays](const std::string & /*osColName*/,
                                        size_t nCapacity, size_t nMulFactor = 1)
    {
        if (bGrowVariableSizeArrays)
        {
            CPLAssert(nCapacity > 0);
            return 2 * nCapacity;
        }
        return std::max(m_nBatchSize * nMulFactor, nCapacity);
    };

    m_anFIDs->resize(m_nBatchSize);
    if (!m_osFIDColumn.empty())
    {
        m_query->set_data_buffer(m_osFIDColumn, *(m_anFIDs));
    }

    if (!m_poFeatureDefn->GetGeomFieldDefn(0)->IsIgnored())
    {
        const char *pszGeomColName = GetDatabaseGeomColName();
        if (pszGeomColName)
        {
            m_anGeometryOffsets->resize(m_nBatchSize);
            m_abyGeometries->resize(GetValueSize(pszGeomColName,
                                                 m_nGeometriesCapacity,
                                                 m_nEstimatedWkbSizePerRow));
            m_nGeometriesCapacity = m_abyGeometries->capacity();
            const auto colType = m_schema->attribute(pszGeomColName).type();
            if (colType == TILEDB_UINT8)
            {
                m_query->set_data_buffer(pszGeomColName, *m_abyGeometries);
                m_query->set_offsets_buffer(pszGeomColName,
                                            *m_anGeometryOffsets);
            }
            else if (colType == TILEDB_BLOB)
            {
                m_query->set_data_buffer(
                    pszGeomColName,
                    reinterpret_cast<std::byte *>(m_abyGeometries->data()),
                    m_abyGeometries->size());
                m_query->set_offsets_buffer(pszGeomColName,
                                            m_anGeometryOffsets->data(),
                                            m_anGeometryOffsets->size());
            }
            else
            {
                CPLAssert(false);
            }
        }
        else
        {
            m_adfXs->resize(m_nBatchSize);
            m_query->set_data_buffer(m_osXDim, *m_adfXs);

            m_adfYs->resize(m_nBatchSize);
            m_query->set_data_buffer(m_osYDim, *m_adfYs);

            if (!m_osZDim.empty())
            {
                m_adfZs->resize(m_nBatchSize);
                m_query->set_data_buffer(m_osZDim, *m_adfZs);
            }
        }
    }

    if (m_anFieldValuesCapacity.empty())
        m_anFieldValuesCapacity.resize(m_poFeatureDefn->GetFieldCount());

    for (int i = 0; i < m_poFeatureDefn->GetFieldCount(); ++i)
    {
        const OGRFieldDefn *poFieldDefn = m_poFeatureDefn->GetFieldDefn(i);
        if (poFieldDefn->IsIgnored())
            continue;
        const char *pszFieldName = poFieldDefn->GetNameRef();
        auto &anOffsets = *(m_aFieldValueOffsets[i]);
        if (poFieldDefn->IsNullable())
        {
            m_aFieldValidity[i].resize(m_nBatchSize);
            m_query->set_validity_buffer(pszFieldName, m_aFieldValidity[i]);
        }
        auto &fieldValues = m_aFieldValues[i];
        switch (poFieldDefn->GetType())
        {
            case OFTInteger:
            {
                if (m_aeFieldTypes[i] == TILEDB_BOOL)
                {
                    auto &v = *(
                        std::get<std::shared_ptr<VECTOR_OF_BOOL>>(fieldValues));
                    v.resize(m_nBatchSize);
#ifdef VECTOR_OF_BOOL_IS_NOT_UINT8_T
                    m_query->set_data_buffer(pszFieldName, v.data(), v.size());
#else
                    m_query->set_data_buffer(pszFieldName, v);
#endif
                }
                else if (m_aeFieldTypes[i] == TILEDB_INT16)
                {
                    auto &v = *(std::get<std::shared_ptr<std::vector<int16_t>>>(
                        fieldValues));
                    v.resize(m_nBatchSize);
                    m_query->set_data_buffer(pszFieldName, v);
                }
                else if (m_aeFieldTypes[i] == TILEDB_INT32)
                {
                    auto &v = *(std::get<std::shared_ptr<std::vector<int32_t>>>(
                        fieldValues));
                    v.resize(m_nBatchSize);
                    m_query->set_data_buffer(pszFieldName, v);
                }
                else if (m_aeFieldTypes[i] == TILEDB_UINT8)
                {
                    auto &v = *(std::get<std::shared_ptr<std::vector<uint8_t>>>(
                        fieldValues));
                    v.resize(m_nBatchSize);
                    m_query->set_data_buffer(pszFieldName, v);
                }
                else if (m_aeFieldTypes[i] == TILEDB_UINT16)
                {
                    auto &v =
                        *(std::get<std::shared_ptr<std::vector<uint16_t>>>(
                            fieldValues));
                    v.resize(m_nBatchSize);
                    m_query->set_data_buffer(pszFieldName, v);
                }
                else
                {
                    CPLAssert(false);
                }
                break;
            }

            case OFTIntegerList:
            {
                auto iter = m_oMapEstimatedSizePerRow.find(pszFieldName);
                const int nMulFactor =
                    iter != m_oMapEstimatedSizePerRow.end()
                        ? static_cast<int>(
                              std::min<uint64_t>(1000, iter->second))
                        : 8;
                if (m_aeFieldTypes[i] == TILEDB_BOOL)
                {
                    auto &v = *(
                        std::get<std::shared_ptr<VECTOR_OF_BOOL>>(fieldValues));
                    v.resize(GetValueSize(
                        pszFieldName, m_anFieldValuesCapacity[i], nMulFactor));
                    m_anFieldValuesCapacity[i] = v.capacity();
                    anOffsets.resize(m_nBatchSize);
                    m_query->set_offsets_buffer(pszFieldName, anOffsets);
#ifdef VECTOR_OF_BOOL_IS_NOT_UINT8_T
                    m_query->set_data_buffer(pszFieldName, v.data(), v.size());
#else
                    m_query->set_data_buffer(pszFieldName, v);
#endif
                }
                else if (m_aeFieldTypes[i] == TILEDB_INT16)
                {
                    auto &v = *(std::get<std::shared_ptr<std::vector<int16_t>>>(
                        fieldValues));
                    v.resize(GetValueSize(
                        pszFieldName, m_anFieldValuesCapacity[i], nMulFactor));
                    m_anFieldValuesCapacity[i] = v.capacity();
                    anOffsets.resize(m_nBatchSize);
                    m_query->set_data_buffer(pszFieldName, v);
                    m_query->set_offsets_buffer(pszFieldName, anOffsets);
                }
                else if (m_aeFieldTypes[i] == TILEDB_INT32)
                {
                    auto &v = *(std::get<std::shared_ptr<std::vector<int32_t>>>(
                        fieldValues));
                    v.resize(GetValueSize(
                        pszFieldName, m_anFieldValuesCapacity[i], nMulFactor));
                    m_anFieldValuesCapacity[i] = v.capacity();
                    anOffsets.resize(m_nBatchSize);
                    m_query->set_data_buffer(pszFieldName, v);
                    m_query->set_offsets_buffer(pszFieldName, anOffsets);
                }
                else if (m_aeFieldTypes[i] == TILEDB_UINT8)
                {
                    auto &v = *(std::get<std::shared_ptr<std::vector<uint8_t>>>(
                        fieldValues));
                    v.resize(GetValueSize(
                        pszFieldName, m_anFieldValuesCapacity[i], nMulFactor));
                    m_anFieldValuesCapacity[i] = v.capacity();
                    anOffsets.resize(m_nBatchSize);
                    m_query->set_data_buffer(pszFieldName, v);
                    m_query->set_offsets_buffer(pszFieldName, anOffsets);
                }
                else if (m_aeFieldTypes[i] == TILEDB_UINT16)
                {
                    auto &v =
                        *(std::get<std::shared_ptr<std::vector<uint16_t>>>(
                            fieldValues));
                    v.resize(GetValueSize(
                        pszFieldName, m_anFieldValuesCapacity[i], nMulFactor));
                    m_anFieldValuesCapacity[i] = v.capacity();
                    anOffsets.resize(m_nBatchSize);
                    m_query->set_data_buffer(pszFieldName, v);
                    m_query->set_offsets_buffer(pszFieldName, anOffsets);
                }
                else
                {
                    CPLAssert(false);
                }
                break;
            }

            case OFTInteger64:
            case OFTDate:
            case OFTDateTime:
            case OFTTime:
            {
                auto &v = *(std::get<std::shared_ptr<std::vector<int64_t>>>(
                    fieldValues));
                v.resize(m_nBatchSize);
                m_query->set_data_buffer(pszFieldName, v);
                break;
            }

            case OFTInteger64List:
            {
                auto iter = m_oMapEstimatedSizePerRow.find(pszFieldName);
                const int nMulFactor =
                    iter != m_oMapEstimatedSizePerRow.end()
                        ? static_cast<int>(
                              std::min<uint64_t>(1000, iter->second))
                        : 8;
                auto &v = *(std::get<std::shared_ptr<std::vector<int64_t>>>(
                    fieldValues));
                v.resize(GetValueSize(pszFieldName, m_anFieldValuesCapacity[i],
                                      nMulFactor));
                m_anFieldValuesCapacity[i] = v.capacity();
                anOffsets.resize(m_nBatchSize);
                m_query->set_data_buffer(pszFieldName, v);
                m_query->set_offsets_buffer(pszFieldName, anOffsets);
                break;
            }

            case OFTReal:
            {
                if (poFieldDefn->GetSubType() == OFSTFloat32)
                {
                    auto &v = *(std::get<std::shared_ptr<std::vector<float>>>(
                        fieldValues));
                    v.resize(m_nBatchSize);
                    m_query->set_data_buffer(pszFieldName, v);
                }
                else
                {
                    auto &v = *(std::get<std::shared_ptr<std::vector<double>>>(
                        fieldValues));
                    v.resize(m_nBatchSize);
                    m_query->set_data_buffer(pszFieldName, v);
                }
                break;
            }

            case OFTRealList:
            {
                auto iter = m_oMapEstimatedSizePerRow.find(pszFieldName);
                const int nMulFactor =
                    iter != m_oMapEstimatedSizePerRow.end()
                        ? static_cast<int>(
                              std::min<uint64_t>(1000, iter->second))
                        : 8;
                anOffsets.resize(m_nBatchSize);
                if (poFieldDefn->GetSubType() == OFSTFloat32)
                {
                    auto &v = *(std::get<std::shared_ptr<std::vector<float>>>(
                        fieldValues));
                    v.resize(GetValueSize(
                        pszFieldName, m_anFieldValuesCapacity[i], nMulFactor));
                    m_anFieldValuesCapacity[i] = v.capacity();
                    m_query->set_data_buffer(pszFieldName, v);
                    m_query->set_offsets_buffer(pszFieldName, anOffsets);
                }
                else
                {
                    auto &v = *(std::get<std::shared_ptr<std::vector<double>>>(
                        fieldValues));
                    v.resize(GetValueSize(
                        pszFieldName, m_anFieldValuesCapacity[i], nMulFactor));
                    m_anFieldValuesCapacity[i] = v.capacity();
                    m_query->set_data_buffer(pszFieldName, v);
                    m_query->set_offsets_buffer(pszFieldName, anOffsets);
                }
                break;
            }

            case OFTString:
            {
                auto &v =
                    *(std::get<std::shared_ptr<std::string>>(fieldValues));
                auto iter = m_oMapEstimatedSizePerRow.find(pszFieldName);
                v.resize(GetValueSize(pszFieldName, m_anFieldValuesCapacity[i],
                                      iter != m_oMapEstimatedSizePerRow.end()
                                          ? iter->second
                                          : 8));
                m_anFieldValuesCapacity[i] = v.capacity();
                anOffsets.resize(m_nBatchSize);
                m_query->set_data_buffer(pszFieldName, v);
                m_query->set_offsets_buffer(pszFieldName, anOffsets);
                break;
            }

            case OFTBinary:
            {
                const auto eType = m_schema->attribute(pszFieldName).type();
                auto &v = *(std::get<std::shared_ptr<std::vector<uint8_t>>>(
                    fieldValues));
                auto iter = m_oMapEstimatedSizePerRow.find(pszFieldName);
                v.resize(GetValueSize(pszFieldName, m_anFieldValuesCapacity[i],
                                      iter != m_oMapEstimatedSizePerRow.end()
                                          ? iter->second
                                          : 8));
                m_anFieldValuesCapacity[i] = v.capacity();
                anOffsets.resize(m_nBatchSize);
                if (eType == TILEDB_UINT8)
                {
                    m_query->set_data_buffer(pszFieldName, v);
                    m_query->set_offsets_buffer(pszFieldName, anOffsets);
                }
                else if (eType == TILEDB_BLOB)
                {
                    m_query->set_data_buffer(
                        pszFieldName, reinterpret_cast<std::byte *>(v.data()),
                        v.size());
                    m_query->set_offsets_buffer(pszFieldName, anOffsets.data(),
                                                anOffsets.size());
                }
                else
                {
                    CPLAssert(false);
                }
                break;
            }

            default:
            {
                CPLAssert(false);
                break;
            }
        }
    }
}

/************************************************************************/
/*                           SetupQuery()                               */
/************************************************************************/

namespace
{
template <class T> struct ResetArray
{
    static void exec(OGRTileDBLayer::ArrayType &array)
    {
        array = std::make_shared<T>();
    }
};
}  // namespace

void OGRTileDBLayer::AllocateNewBuffers()
{
    m_anFIDs = std::make_shared<std::vector<int64_t>>();
    m_adfXs = std::make_shared<std::vector<double>>();
    m_adfYs = std::make_shared<std::vector<double>>();
    m_adfZs = std::make_shared<std::vector<double>>();
    m_abyGeometries = std::make_shared<std::vector<unsigned char>>();
    m_anGeometryOffsets = std::make_shared<std::vector<uint64_t>>();

    for (int i = 0; i < m_poFeatureDefn->GetFieldCount(); i++)
    {
        ProcessField<ResetArray>::exec(m_aeFieldTypes[i], m_aFieldValues[i]);

        m_aFieldValueOffsets[i] = std::make_shared<std::vector<uint64_t>>();
    }
}

bool OGRTileDBLayer::SetupQuery(tiledb::QueryCondition *queryCondition)
{
    if (!m_bArrowBatchReleased)
    {
        AllocateNewBuffers();
    }

    m_anFIDs->clear();
    m_adfXs->clear();
    m_anGeometryOffsets->clear();
    m_nOffsetInResultSet = 0;
    m_nRowCountInResultSet = 0;
    if (m_bAttributeFilterAlwaysFalse)
        return false;

    const char *pszGeomColName = GetDatabaseGeomColName();

    // FIXME: remove this
    const bool bHitBug = CPLTestBool(CPLGetConfigOption("TILEDB_BUG", "NO"));
    if (bHitBug)
    {
        m_nBatchSize = 1;
        m_nEstimatedWkbSizePerRow = 10;
    }

    try
    {
        if (!m_query)
        {
            m_query = std::make_unique<tiledb::Query>(*m_ctx, *m_array);
            m_query->set_layout(TILEDB_UNORDERED);
            if (queryCondition)
                m_query->set_condition(*queryCondition);
            else if (m_poQueryCondition)
                m_query->set_condition(*(m_poQueryCondition.get()));

            if (m_nEstimatedWkbSizePerRow == 0)
            {
                for (int i = 0; i < m_poFeatureDefn->GetFieldCount(); ++i)
                {
                    const OGRFieldDefn *poFieldDefn =
                        m_poFeatureDefn->GetFieldDefn(i);
                    const char *pszFieldName = poFieldDefn->GetNameRef();
                    switch (poFieldDefn->GetType())
                    {
                        case OFTString:
                        case OFTBinary:
                        case OFTIntegerList:
                        case OFTInteger64List:
                        case OFTRealList:
                        {
                            uint64_t nEstRows;
                            uint64_t nEstBytes;
                            if (poFieldDefn->IsNullable())
                            {
                                const auto estimation =
                                    m_query->est_result_size_var_nullable(
                                        pszFieldName);
                                nEstRows = estimation[0] / sizeof(uint64_t);
                                nEstBytes = estimation[1];
                            }
                            else
                            {
                                const auto estimation =
                                    m_query->est_result_size_var(pszFieldName);
                                nEstRows = estimation[0] / sizeof(uint64_t);
                                nEstBytes = estimation[1];
                            }
                            if (nEstRows)
                            {
                                m_oMapEstimatedSizePerRow[pszFieldName] =
                                    std::max<size_t>(1,
                                                     static_cast<size_t>(
                                                         nEstBytes / nEstRows) *
                                                         4 / 3);
                                CPLDebug("TILEDB", "Average %s size: %u bytes",
                                         pszFieldName,
                                         static_cast<unsigned>(
                                             m_oMapEstimatedSizePerRow
                                                 [pszFieldName]));
                            }
                            break;
                        }

                        default:
                            break;
                    }
                }

                m_nEstimatedWkbSizePerRow = 9;  // Size of 2D point WKB
                if (pszGeomColName)
                {
                    const auto estimation =
                        m_query->est_result_size_var(pszGeomColName);
                    const uint64_t nEstRows = estimation[0] / sizeof(uint64_t);
                    const uint64_t nEstBytes = estimation[1];
                    if (nEstRows)
                    {
                        m_nEstimatedWkbSizePerRow = std::max(
                            m_nEstimatedWkbSizePerRow,
                            static_cast<size_t>(nEstBytes / nEstRows) * 4 / 3);
                        CPLDebug(
                            "TILEDB", "Average WKB size: %u bytes",
                            static_cast<unsigned>(m_nEstimatedWkbSizePerRow));
                    }
                }
            }

            if (m_poFilterGeom && queryCondition == nullptr)
            {
                tiledb::Subarray subarray(*m_ctx, *m_array);

                const double dfMinX =
                    std::max(m_dfXStart, m_sFilterEnvelope.MinX - m_dfPadX);
                const double dfMaxX =
                    std::min(m_dfXEnd, m_sFilterEnvelope.MaxX + m_dfPadX);
                const double dfMinY =
                    std::max(m_dfYStart, m_sFilterEnvelope.MinY - m_dfPadY);
                const double dfMaxY =
                    std::min(m_dfYEnd, m_sFilterEnvelope.MaxY + m_dfPadY);

                if (dfMaxX < dfMinX || dfMaxY < dfMinY)
                {
                    m_bQueryComplete = true;
                    return false;
                }

                subarray.add_range(m_osXDim, dfMinX, dfMaxX);
                subarray.add_range(m_osYDim, dfMinY, dfMaxY);
                m_query->set_subarray(subarray);
            }
        }

        SetReadBuffers(m_bGrowBuffers);
        m_bGrowBuffers = false;

        // Create a loop
        tiledb::Query::Status status;
        uint64_t nRowCount = 0;
        while (true)
        {
            // Submit query and get status
            if (m_bStats)
                tiledb::Stats::enable();

            m_query->submit();

            if (m_bStats)
            {
                tiledb::Stats::dump(stdout);
                tiledb::Stats::disable();
            }

            status = m_query->query_status();
            if (status == tiledb::Query::Status::FAILED)
            {
                CPLError(CE_Failure, CPLE_AppDefined, "Query failed");
                m_bQueryComplete = true;
                return false;
            }

            const auto result_buffer_elements =
                m_query->result_buffer_elements();
            if (m_osFIDColumn.empty())
            {
                auto oIter = result_buffer_elements.begin();
                if (oIter != result_buffer_elements.end())
                    nRowCount = oIter->second.second;
            }
            else
            {
                auto oIter = result_buffer_elements.find(m_osFIDColumn);
                if (oIter != result_buffer_elements.end())
                    nRowCount = oIter->second.second;
            }
            if (!m_poFeatureDefn->GetGeomFieldDefn(0)->IsIgnored() &&
                pszGeomColName)
            {
                auto oIter = result_buffer_elements.find(pszGeomColName);
                if (oIter != result_buffer_elements.end())
                {
                    const auto &result = oIter->second;
                    nRowCount = std::min(nRowCount, result.first);
                    // For some reason, result.first can be 1, and result.second 0
                    if (!bHitBug && result.second == 0)
                        nRowCount = 0;
                }
                else
                {
                    CPLAssert(false);
                }
            }
            for (int i = 0; i < m_poFeatureDefn->GetFieldCount(); ++i)
            {
                const OGRFieldDefn *poFieldDefn =
                    m_poFeatureDefn->GetFieldDefn(i);
                if (!poFieldDefn->IsIgnored())
                {
                    const char *pszFieldName = poFieldDefn->GetNameRef();
                    auto oIter = result_buffer_elements.find(pszFieldName);
                    if (oIter != result_buffer_elements.end())
                    {
                        const auto &result = oIter->second;
                        if (result.first == 0)
                        {
                            nRowCount = std::min(nRowCount, result.second);
                        }
                        else
                            nRowCount = std::min(nRowCount, result.first);
                    }
                    else
                    {
                        CPLAssert(false);
                    }
                }
            }

            if (status != tiledb::Query::Status::INCOMPLETE)
                break;

            if (bHitBug)
            {
                if (nRowCount > 0)
                    break;
                SetReadBuffers(true);
            }
            else if (nRowCount < m_nBatchSize)
            {
                if (nRowCount > 0)
                {
                    m_bGrowBuffers = true;
                    break;
                }
                CPLDebug("TILEDB", "Got 0 rows. Grow buffers");
                SetReadBuffers(true);
            }
            else
                break;
        }

        m_bQueryComplete = (status == tiledb::Query::Status::COMPLETE);
        m_nRowCountInResultSet = nRowCount;

        if (nRowCount == 0)
        {
            m_bQueryComplete = true;
            return false;
        }
        //CPLDebug("TILEDB", "Read %d rows", int(nRowCount));

        const auto result_buffer_elements = m_query->result_buffer_elements();
        m_anFIDs->resize(nRowCount);
        if (m_osFIDColumn.empty())
        {
            for (uint64_t i = 0; i < nRowCount; ++i)
            {
                (*m_anFIDs)[i] = m_nNextFID;
                m_nNextFID++;
            }
        }

        if (!m_poFeatureDefn->GetGeomFieldDefn(0)->IsIgnored())
        {
            if (pszGeomColName)
            {
                auto oIter = result_buffer_elements.find(pszGeomColName);
                if (oIter != result_buffer_elements.end())
                {
                    const auto &result = oIter->second;
                    if (nRowCount < result.first)
                    {
                        m_abyGeometries->resize(
                            (*m_anGeometryOffsets)[nRowCount]);
                    }
                    else
                    {
                        m_abyGeometries->resize(
                            static_cast<size_t>(result.second));
                    }
                    m_anGeometryOffsets->resize(nRowCount);
                }
            }
            else
            {
                m_adfXs->resize(nRowCount);
                m_adfYs->resize(nRowCount);
                if (!m_osZDim.empty())
                    m_adfZs->resize(nRowCount);
            }
        }

        for (int i = 0; i < m_poFeatureDefn->GetFieldCount(); ++i)
        {
            const OGRFieldDefn *poFieldDefn = m_poFeatureDefn->GetFieldDefn(i);
            if (poFieldDefn->IsIgnored())
                continue;
            const char *pszFieldName = poFieldDefn->GetNameRef();
            auto &anOffsets = *(m_aFieldValueOffsets[i]);
            auto oIter = result_buffer_elements.find(pszFieldName);
            if (oIter == result_buffer_elements.end())
            {
                CPLAssert(false);
                continue;
            }
            const auto &result = oIter->second;
            if (poFieldDefn->IsNullable())
                m_aFieldValidity[i].resize(nRowCount);
            auto &fieldValues = m_aFieldValues[i];
            switch (poFieldDefn->GetType())
            {
                case OFTInteger:
                {
                    if (m_aeFieldTypes[i] == TILEDB_BOOL)
                    {
                        auto &v = *(std::get<std::shared_ptr<VECTOR_OF_BOOL>>(
                            fieldValues));
                        v.resize(result.second);
                    }
                    else if (m_aeFieldTypes[i] == TILEDB_INT16)
                    {
                        auto &v =
                            *(std::get<std::shared_ptr<std::vector<int16_t>>>(
                                fieldValues));
                        v.resize(result.second);
                    }
                    else if (m_aeFieldTypes[i] == TILEDB_INT32)
                    {
                        auto &v =
                            *(std::get<std::shared_ptr<std::vector<int32_t>>>(
                                fieldValues));
                        v.resize(result.second);
                    }
                    else if (m_aeFieldTypes[i] == TILEDB_UINT8)
                    {
                        auto &v =
                            *(std::get<std::shared_ptr<std::vector<uint8_t>>>(
                                fieldValues));
                        v.resize(result.second);
                    }
                    else if (m_aeFieldTypes[i] == TILEDB_UINT16)
                    {
                        auto &v =
                            *(std::get<std::shared_ptr<std::vector<uint16_t>>>(
                                fieldValues));
                        v.resize(result.second);
                    }
                    else
                    {
                        CPLAssert(false);
                    }
                    break;
                }

                case OFTIntegerList:
                {
                    if (m_aeFieldTypes[i] == TILEDB_BOOL)
                    {
                        auto &v = *(std::get<std::shared_ptr<VECTOR_OF_BOOL>>(
                            fieldValues));
                        if (nRowCount < result.first)
                        {
                            v.resize(anOffsets[nRowCount] / sizeof(v[0]));
                        }
                        else
                        {
                            v.resize(static_cast<size_t>(result.second));
                        }
                    }
                    else if (m_aeFieldTypes[i] == TILEDB_INT16)
                    {
                        auto &v =
                            *(std::get<std::shared_ptr<std::vector<int16_t>>>(
                                fieldValues));
                        if (nRowCount < result.first)
                        {
                            v.resize(anOffsets[nRowCount] / sizeof(v[0]));
                        }
                        else
                        {
                            v.resize(static_cast<size_t>(result.second));
                        }
                    }
                    else if (m_aeFieldTypes[i] == TILEDB_INT32)
                    {
                        auto &v =
                            *(std::get<std::shared_ptr<std::vector<int32_t>>>(
                                fieldValues));
                        if (nRowCount < result.first)
                        {
                            v.resize(anOffsets[nRowCount] / sizeof(v[0]));
                        }
                        else
                        {
                            v.resize(static_cast<size_t>(result.second));
                        }
                    }
                    else if (m_aeFieldTypes[i] == TILEDB_UINT8)
                    {
                        auto &v =
                            *(std::get<std::shared_ptr<std::vector<uint8_t>>>(
                                fieldValues));
                        if (nRowCount < result.first)
                        {
                            v.resize(anOffsets[nRowCount] / sizeof(v[0]));
                        }
                        else
                        {
                            v.resize(static_cast<size_t>(result.second));
                        }
                    }
                    else if (m_aeFieldTypes[i] == TILEDB_UINT16)
                    {
                        auto &v =
                            *(std::get<std::shared_ptr<std::vector<uint16_t>>>(
                                fieldValues));
                        if (nRowCount < result.first)
                        {
                            v.resize(anOffsets[nRowCount] / sizeof(v[0]));
                        }
                        else
                        {
                            v.resize(static_cast<size_t>(result.second));
                        }
                    }
                    else
                    {
                        CPLAssert(false);
                    }
                    anOffsets.resize(nRowCount);
                    break;
                }

                case OFTInteger64:
                case OFTDate:
                case OFTDateTime:
                case OFTTime:
                {
                    auto &v = *(std::get<std::shared_ptr<std::vector<int64_t>>>(
                        fieldValues));
                    v.resize(result.second);
                    break;
                }

                case OFTInteger64List:
                {
                    auto &v = *(std::get<std::shared_ptr<std::vector<int64_t>>>(
                        fieldValues));
                    if (nRowCount < result.first)
                    {
                        v.resize(anOffsets[nRowCount] / sizeof(v[0]));
                    }
                    else
                    {
                        v.resize(static_cast<size_t>(result.second));
                    }
                    anOffsets.resize(nRowCount);
                    break;
                }

                case OFTReal:
                {
                    if (poFieldDefn->GetSubType() == OFSTFloat32)
                    {
                        auto &v =
                            *(std::get<std::shared_ptr<std::vector<float>>>(
                                fieldValues));
                        v.resize(result.second);
                    }
                    else
                    {
                        auto &v =
                            *(std::get<std::shared_ptr<std::vector<double>>>(
                                fieldValues));
                        v.resize(result.second);
                    }
                    break;
                }

                case OFTRealList:
                {
                    if (poFieldDefn->GetSubType() == OFSTFloat32)
                    {
                        auto &v =
                            *(std::get<std::shared_ptr<std::vector<float>>>(
                                fieldValues));
                        if (nRowCount < result.first)
                        {
                            v.resize(anOffsets[nRowCount] / sizeof(v[0]));
                        }
                        else
                        {
                            v.resize(static_cast<size_t>(result.second));
                        }
                    }
                    else
                    {
                        auto &v =
                            *(std::get<std::shared_ptr<std::vector<double>>>(
                                fieldValues));
                        if (nRowCount < result.first)
                        {
                            v.resize(anOffsets[nRowCount] / sizeof(v[0]));
                        }
                        else
                        {
                            v.resize(static_cast<size_t>(result.second));
                        }
                    }
                    anOffsets.resize(nRowCount);
                    break;
                }

                case OFTString:
                {
                    auto &v =
                        *(std::get<std::shared_ptr<std::string>>(fieldValues));
                    if (nRowCount < result.first)
                    {
                        v.resize(anOffsets[nRowCount] / sizeof(v[0]));
                    }
                    else
                    {
                        v.resize(static_cast<size_t>(result.second));
                    }
                    anOffsets.resize(nRowCount);
                    break;
                }

                case OFTBinary:
                {
                    auto &v = *(std::get<std::shared_ptr<std::vector<uint8_t>>>(
                        fieldValues));
                    if (nRowCount < result.first)
                    {
                        v.resize(anOffsets[nRowCount] / sizeof(v[0]));
                    }
                    else
                    {
                        v.resize(static_cast<size_t>(result.second));
                    }
                    anOffsets.resize(nRowCount);
                    break;
                }

                default:
                {
                    CPLAssert(false);
                    break;
                }
            }
        }
    }
    catch (const std::exception &e)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "%s", e.what());
        m_bQueryComplete = true;
        return false;
    }

    return true;
}

/************************************************************************/
/*                       SwitchToReadingMode()                          */
/************************************************************************/

void OGRTileDBLayer::SwitchToReadingMode()
{
    if (m_eCurrentMode == CurrentMode::WriteInProgress)
    {
        m_eCurrentMode = CurrentMode::None;
        try
        {
            if (m_array)
            {
                if (!m_adfXs->empty())
                {
                    FlushArrays();
                }
                m_array->close();
                m_array.reset();
            }
        }
        catch (const std::exception &e)
        {
            CPLError(CE_Failure, CPLE_AppDefined, "%s", e.what());
            m_array.reset();
            return;
        }

        try
        {
            if (m_nTimestamp)
                m_array.reset(new tiledb::Array(
                    *m_ctx, m_osFilename, TILEDB_READ,
                    tiledb::TemporalPolicy(tiledb::TimeTravel, m_nTimestamp)));
            else
                m_array.reset(
                    new tiledb::Array(*m_ctx, m_osFilename, TILEDB_READ));
        }
        catch (const std::exception &e)
        {
            CPLError(CE_Failure, CPLE_AppDefined, "%s", e.what());
            return;
        }
    }
    m_eCurrentMode = CurrentMode::ReadInProgress;
}

/************************************************************************/
/*                       GetNextRawFeature()                            */
/************************************************************************/

OGRFeature *OGRTileDBLayer::GetNextRawFeature()
{
    if (m_eCurrentMode == CurrentMode::WriteInProgress)
    {
        ResetReading();
    }
    if (!m_array)
        return nullptr;

    if (m_nOffsetInResultSet >= m_nRowCountInResultSet)
    {
        if (m_bQueryComplete)
            return nullptr;

        if (!SetupQuery(nullptr))
            return nullptr;
    }

    return TranslateCurrentFeature();
}

/***********************************************************************/
/*                        GetColumnSubNode()                           */
/***********************************************************************/

static const swq_expr_node *GetColumnSubNode(const swq_expr_node *poNode)
{
    if (poNode->eNodeType == SNT_OPERATION && poNode->nSubExprCount == 2)
    {
        if (poNode->papoSubExpr[0]->eNodeType == SNT_COLUMN)
            return poNode->papoSubExpr[0];
        if (poNode->papoSubExpr[1]->eNodeType == SNT_COLUMN)
            return poNode->papoSubExpr[1];
    }
    return nullptr;
}

/***********************************************************************/
/*                        GetConstantSubNode()                         */
/***********************************************************************/

static const swq_expr_node *GetConstantSubNode(const swq_expr_node *poNode)
{
    if (poNode->eNodeType == SNT_OPERATION && poNode->nSubExprCount == 2)
    {
        if (poNode->papoSubExpr[1]->eNodeType == SNT_CONSTANT)
            return poNode->papoSubExpr[1];
        if (poNode->papoSubExpr[0]->eNodeType == SNT_CONSTANT)
            return poNode->papoSubExpr[0];
    }
    return nullptr;
}

/***********************************************************************/
/*                           IsComparisonOp()                          */
/***********************************************************************/

static bool IsComparisonOp(int op)
{
    return (op == SWQ_EQ || op == SWQ_NE || op == SWQ_LT || op == SWQ_LE ||
            op == SWQ_GT || op == SWQ_GE);
}

/***********************************************************************/
/*                       OGRFieldToTimeMS()                            */
/***********************************************************************/

static int64_t OGRFieldToTimeMS(const OGRField &sField)
{
    GIntBig nVal = sField.Date.Hour * 3600 + sField.Date.Minute * 60;
    return static_cast<int64_t>(
        (static_cast<double>(nVal) + sField.Date.Second) * 1000 + 0.5);
}

/***********************************************************************/
/*                       OGRFieldToDateDay()                           */
/***********************************************************************/

static int64_t OGRFieldToDateDay(const OGRField &sField)
{
    struct tm brokenDown;
    memset(&brokenDown, 0, sizeof(brokenDown));
    brokenDown.tm_year = sField.Date.Year - 1900;
    brokenDown.tm_mon = sField.Date.Month - 1;
    brokenDown.tm_mday = sField.Date.Day;
    brokenDown.tm_hour = 0;
    brokenDown.tm_min = 0;
    brokenDown.tm_sec = 0;
    GIntBig nVal = CPLYMDHMSToUnixTime(&brokenDown);
    return static_cast<int64_t>(nVal / SECONDS_PER_DAY);
}

/***********************************************************************/
/*                       OGRFieldToDateTimeMS()                        */
/***********************************************************************/

static int64_t OGRFieldToDateTimeMS(const OGRField &sField)
{
    struct tm brokenDown;
    memset(&brokenDown, 0, sizeof(brokenDown));
    brokenDown.tm_year = sField.Date.Year - 1900;
    brokenDown.tm_mon = sField.Date.Month - 1;
    brokenDown.tm_mday = sField.Date.Day;
    brokenDown.tm_hour = sField.Date.Hour;
    brokenDown.tm_min = sField.Date.Minute;
    brokenDown.tm_sec = 0;
    GIntBig nVal = CPLYMDHMSToUnixTime(&brokenDown);
    if (sField.Date.TZFlag != 0 && sField.Date.TZFlag != 1)
    {
        nVal -= (sField.Date.TZFlag - 100) * 15 * 60;
    }
    return static_cast<int64_t>(
        (static_cast<double>(nVal) + sField.Date.Second) * 1000 + 0.5);
}

/***********************************************************************/
/*                  CreateQueryConditionForIntType()                   */
/***********************************************************************/

template <typename T>
static std::unique_ptr<tiledb::QueryCondition>
CreateQueryConditionForIntType(tiledb::Context &ctx,
                               const OGRFieldDefn *poFieldDefn, int nVal,
                               tiledb_query_condition_op_t tiledb_op,
                               bool &bAlwaysTrue, bool &bAlwaysFalse)
{
    if (nVal >= static_cast<int>(std::numeric_limits<T>::min()) &&
        nVal <= static_cast<int>(std::numeric_limits<T>::max()))
    {
        return std::make_unique<tiledb::QueryCondition>(
            tiledb::QueryCondition::create(ctx, poFieldDefn->GetNameRef(),
                                           static_cast<T>(nVal), tiledb_op));
    }
    else if (tiledb_op == TILEDB_EQ)
    {
        bAlwaysFalse = true;
    }
    else if (tiledb_op == TILEDB_NE)
    {
        bAlwaysTrue = true;
    }
    else if (nVal > static_cast<int>(std::numeric_limits<T>::max()))
    {
        bAlwaysTrue = (tiledb_op == TILEDB_LE || tiledb_op == TILEDB_LT);
        bAlwaysFalse = (tiledb_op == TILEDB_GE || tiledb_op == TILEDB_GT);
    }
    else if (nVal < static_cast<int>(std::numeric_limits<T>::min()))
    {
        bAlwaysTrue = (tiledb_op == TILEDB_GE || tiledb_op == TILEDB_GT);
        bAlwaysFalse = (tiledb_op == TILEDB_LE || tiledb_op == TILEDB_LT);
    }
    return nullptr;
}

/***********************************************************************/
/*                     CreateQueryCondition()                          */
/***********************************************************************/

std::unique_ptr<tiledb::QueryCondition> OGRTileDBLayer::CreateQueryCondition(
    int nOperation, bool bColumnIsLeft, const swq_expr_node *poColumn,
    const swq_expr_node *poValue, bool &bAlwaysTrue, bool &bAlwaysFalse)
{
    bAlwaysTrue = false;
    bAlwaysFalse = false;

    if (poColumn != nullptr && poValue != nullptr &&
        poColumn->field_index < m_poFeatureDefn->GetFieldCount())
    {
        const OGRFieldDefn *poFieldDefn =
            m_poFeatureDefn->GetFieldDefn(poColumn->field_index);

        if (!bColumnIsLeft)
        {
            /* If "constant op column", then we must reverse */
            /* the operator for LE, LT, GE, GT */
            switch (nOperation)
            {
                case SWQ_LE:
                    nOperation = SWQ_GE;
                    break;
                case SWQ_LT:
                    nOperation = SWQ_GT;
                    break;
                case SWQ_NE: /* do nothing */;
                    break;
                case SWQ_EQ: /* do nothing */;
                    break;
                case SWQ_GE:
                    nOperation = SWQ_LE;
                    break;
                case SWQ_GT:
                    nOperation = SWQ_LT;
                    break;
                default:
                    CPLAssert(false);
                    break;
            }
        }

        tiledb_query_condition_op_t tiledb_op = TILEDB_EQ;
        switch (nOperation)
        {
            case SWQ_LE:
                tiledb_op = TILEDB_LE;
                break;
            case SWQ_LT:
                tiledb_op = TILEDB_LT;
                break;
            case SWQ_NE:
                tiledb_op = TILEDB_NE;
                break;
            case SWQ_EQ:
                tiledb_op = TILEDB_EQ;
                break;
            case SWQ_GE:
                tiledb_op = TILEDB_GE;
                break;
            case SWQ_GT:
                tiledb_op = TILEDB_GT;
                break;
            default:
                CPLAssert(false);
                break;
        }

        switch (poFieldDefn->GetType())
        {
            case OFTInteger:
            {
                int nVal;
                if (poValue->field_type == SWQ_FLOAT)
                    nVal = static_cast<int>(poValue->float_value);
                else if (SWQ_IS_INTEGER(poValue->field_type))
                    nVal = static_cast<int>(poValue->int_value);
                else
                {
                    CPLDebug("TILEDB",
                             "Unexpected field_type in SQL expression");
                    CPLAssert(false);
                    return nullptr;
                }

                if (m_aeFieldTypes[poColumn->field_index] == TILEDB_BOOL)
                {
                    if (nVal == 0 || nVal == 1)
                    {
                        return std::make_unique<tiledb::QueryCondition>(
                            tiledb::QueryCondition::create(
                                *(m_ctx.get()), poFieldDefn->GetNameRef(),
                                static_cast<uint8_t>(nVal), tiledb_op));
                    }
                    else if (tiledb_op == TILEDB_EQ)
                    {
                        bAlwaysFalse = true;
                        return nullptr;
                    }
                    else if (tiledb_op == TILEDB_NE)
                    {
                        bAlwaysTrue = true;
                        return nullptr;
                    }
                }
                else if (m_aeFieldTypes[poColumn->field_index] == TILEDB_INT16)
                {
                    return CreateQueryConditionForIntType<int16_t>(
                        *(m_ctx.get()), poFieldDefn, nVal, tiledb_op,
                        bAlwaysTrue, bAlwaysFalse);
                }
                else if (m_aeFieldTypes[poColumn->field_index] == TILEDB_UINT8)
                {
                    return CreateQueryConditionForIntType<uint8_t>(
                        *(m_ctx.get()), poFieldDefn, nVal, tiledb_op,
                        bAlwaysTrue, bAlwaysFalse);
                }
                else if (m_aeFieldTypes[poColumn->field_index] == TILEDB_UINT16)
                {
                    return CreateQueryConditionForIntType<uint16_t>(
                        *(m_ctx.get()), poFieldDefn, nVal, tiledb_op,
                        bAlwaysTrue, bAlwaysFalse);
                }
                else
                {
                    return std::make_unique<tiledb::QueryCondition>(
                        tiledb::QueryCondition::create(
                            *(m_ctx.get()), poFieldDefn->GetNameRef(), nVal,
                            tiledb_op));
                }
                break;
            }

            case OFTInteger64:
            {
                int64_t nVal;
                if (poValue->field_type == SWQ_FLOAT)
                    nVal = static_cast<int64_t>(poValue->float_value);
                else if (SWQ_IS_INTEGER(poValue->field_type))
                    nVal = static_cast<int64_t>(poValue->int_value);
                else
                {
                    CPLDebug("TILEDB",
                             "Unexpected field_type in SQL expression");
                    CPLAssert(false);
                    return nullptr;
                }
                return std::make_unique<tiledb::QueryCondition>(
                    tiledb::QueryCondition::create(*(m_ctx.get()),
                                                   poFieldDefn->GetNameRef(),
                                                   nVal, tiledb_op));
            }

            case OFTReal:
            {
                if (poValue->field_type != SWQ_FLOAT)
                {
                    CPLDebug("TILEDB",
                             "Unexpected field_type in SQL expression");
                    CPLAssert(false);
                    return nullptr;
                }
                if (poFieldDefn->GetSubType() == OFSTFloat32)
                {
                    return std::make_unique<tiledb::QueryCondition>(
                        tiledb::QueryCondition::create(
                            *(m_ctx.get()), poFieldDefn->GetNameRef(),
                            static_cast<float>(poValue->float_value),
                            tiledb_op));
                }
                return std::make_unique<tiledb::QueryCondition>(
                    tiledb::QueryCondition::create(
                        *(m_ctx.get()), poFieldDefn->GetNameRef(),
                        poValue->float_value, tiledb_op));
            }

            case OFTString:
            {
                if (poValue->field_type != SWQ_STRING)
                {
                    CPLDebug("TILEDB",
                             "Unexpected field_type in SQL expression");
                    CPLAssert(false);
                    return nullptr;
                }
                return std::make_unique<tiledb::QueryCondition>(
                    tiledb::QueryCondition::create(
                        *(m_ctx.get()), poFieldDefn->GetNameRef(),
                        std::string(poValue->string_value), tiledb_op));
            }

            case OFTDateTime:
            {
                if (poValue->field_type == SWQ_TIMESTAMP ||
                    poValue->field_type == SWQ_DATE ||
                    poValue->field_type == SWQ_TIME)
                {
                    OGRField sField;
                    if (OGRParseDate(poValue->string_value, &sField, 0))
                    {
                        return std::make_unique<tiledb::QueryCondition>(
                            tiledb::QueryCondition::create(
                                *(m_ctx.get()), poFieldDefn->GetNameRef(),
                                OGRFieldToDateTimeMS(sField), tiledb_op));
                    }
                    else
                    {
                        CPLError(CE_Failure, CPLE_AppDefined,
                                 "Failed to parse %s as a date time",
                                 poValue->string_value);
                    }
                }
                break;
            }

            case OFTDate:
            {
                if (poValue->field_type == SWQ_TIMESTAMP ||
                    poValue->field_type == SWQ_DATE ||
                    poValue->field_type == SWQ_TIME)
                {
                    OGRField sField;
                    if (OGRParseDate(poValue->string_value, &sField, 0))
                    {
                        return std::make_unique<tiledb::QueryCondition>(
                            tiledb::QueryCondition::create(
                                *(m_ctx.get()), poFieldDefn->GetNameRef(),
                                OGRFieldToDateDay(sField), tiledb_op));
                    }
                    else
                    {
                        CPLError(CE_Failure, CPLE_AppDefined,
                                 "Failed to parse %s as a date",
                                 poValue->string_value);
                    }
                }
                break;
            }

#ifdef not_supported_by_tiledb
            // throws the following error:
            // C API: TileDB Internal, std::exception; Cannot perform query comparison; Unsupported query conditional type on
            case OFTTime:
            {
                if (poValue->field_type == SWQ_TIMESTAMP ||
                    poValue->field_type == SWQ_DATE ||
                    poValue->field_type == SWQ_TIME)
                {
                    OGRField sField;
                    if (OGRParseDate(poValue->string_value, &sField, 0))
                    {
                        return std::make_unique<tiledb::QueryCondition>(
                            tiledb::QueryCondition::create(
                                *(m_ctx.get()), poFieldDefn->GetNameRef(),
                                OGRFieldToTimeMS(sField), tiledb_op));
                    }
                }
                break;
            }
#endif

            default:
                break;
        }
    }
    return nullptr;
}

/***********************************************************************/
/*                     CreateQueryCondition()                          */
/***********************************************************************/

std::unique_ptr<tiledb::QueryCondition>
OGRTileDBLayer::CreateQueryCondition(const swq_expr_node *poNode,
                                     bool &bAlwaysTrue, bool &bAlwaysFalse)
{
    bAlwaysTrue = false;
    bAlwaysFalse = false;

    // A AND B
    if (poNode->eNodeType == SNT_OPERATION && poNode->nOperation == SWQ_AND &&
        poNode->nSubExprCount == 2)
    {
        bool bAlwaysTrueLeft, bAlwaysFalseLeft, bAlwaysTrueRight,
            bAlwaysFalseRight;
        auto left = CreateQueryCondition(poNode->papoSubExpr[0],
                                         bAlwaysTrueLeft, bAlwaysFalseLeft);
        auto right = CreateQueryCondition(poNode->papoSubExpr[1],
                                          bAlwaysTrueRight, bAlwaysFalseRight);
        if (bAlwaysFalseLeft || bAlwaysFalseRight)
        {
            bAlwaysFalse = true;
            return nullptr;
        }
        if (bAlwaysTrueLeft)
        {
            if (bAlwaysTrueRight)
            {
                bAlwaysTrue = true;
                return nullptr;
            }
            return right;
        }
        if (bAlwaysTrueRight)
        {
            return left;
        }
        if (left && right)
        {
            return std::make_unique<tiledb::QueryCondition>(
                left->combine(*(right.get()), TILEDB_AND));
        }
        // Returning only left or right member is OK for a AND
        m_bAttributeFilterPartiallyTranslated = true;
        if (left)
            return left;
        return right;
    }

    // A OR B
    else if (poNode->eNodeType == SNT_OPERATION &&
             poNode->nOperation == SWQ_OR && poNode->nSubExprCount == 2)
    {
        bool bAlwaysTrueLeft, bAlwaysFalseLeft, bAlwaysTrueRight,
            bAlwaysFalseRight;
        auto left = CreateQueryCondition(poNode->papoSubExpr[0],
                                         bAlwaysTrueLeft, bAlwaysFalseLeft);
        auto right = CreateQueryCondition(poNode->papoSubExpr[1],
                                          bAlwaysTrueRight, bAlwaysFalseRight);
        if (bAlwaysTrueLeft || bAlwaysTrueRight)
        {
            bAlwaysTrue = true;
            return nullptr;
        }
        if (bAlwaysFalseLeft)
        {
            if (bAlwaysFalseRight)
            {
                bAlwaysFalse = true;
                return nullptr;
            }
            return right;
        }
        if (bAlwaysFalseRight)
        {
            return left;
        }
        if (left && right)
        {
            return std::make_unique<tiledb::QueryCondition>(
                left->combine(*(right.get()), TILEDB_OR));
        }
        m_bAttributeFilterPartiallyTranslated = true;
        return nullptr;
    }

    // field_name IN (constant, ..., constant)
    else if (poNode->eNodeType == SNT_OPERATION &&
             poNode->nOperation == SWQ_IN && poNode->nSubExprCount >= 2 &&
             poNode->papoSubExpr[0]->eNodeType == SNT_COLUMN &&
             poNode->papoSubExpr[0]->field_index <
                 m_poFeatureDefn->GetFieldCount())
    {
        std::unique_ptr<tiledb::QueryCondition> cond;
        for (int i = 1; i < poNode->nSubExprCount; ++i)
        {
            if (poNode->papoSubExpr[i]->eNodeType == SNT_CONSTANT)
            {
                bool bAlwaysTrueTmp;
                bool bAlwaysFalseTmp;
                auto newCond = CreateQueryCondition(
                    SWQ_EQ, true, poNode->papoSubExpr[0],
                    poNode->papoSubExpr[i], bAlwaysTrueTmp, bAlwaysFalseTmp);
                if (bAlwaysFalseTmp)
                    continue;
                if (!newCond)
                {
                    m_bAttributeFilterPartiallyTranslated = true;
                    return nullptr;
                }
                if (!cond)
                {
                    cond = std::move(newCond);
                }
                else
                {
                    cond = std::make_unique<tiledb::QueryCondition>(
                        cond->combine(*(newCond.get()), TILEDB_OR));
                }
            }
            else
            {
                m_bAttributeFilterPartiallyTranslated = true;
                return nullptr;
            }
        }
        if (!cond)
            bAlwaysFalse = true;
        return cond;
    }

    // field_name =/<>/</>/<=/>= constant (or the reverse)
    else if (poNode->eNodeType == SNT_OPERATION &&
             IsComparisonOp(poNode->nOperation) && poNode->nSubExprCount == 2)
    {
        const swq_expr_node *poColumn = GetColumnSubNode(poNode);
        const swq_expr_node *poValue = GetConstantSubNode(poNode);
        return CreateQueryCondition(
            poNode->nOperation, poColumn == poNode->papoSubExpr[0], poColumn,
            poValue, bAlwaysTrue, bAlwaysFalse);
    }

    // field_name IS NULL
    else if (poNode->eNodeType == SNT_OPERATION &&
             poNode->nOperation == SWQ_ISNULL && poNode->nSubExprCount == 1 &&
             poNode->papoSubExpr[0]->eNodeType == SNT_COLUMN &&
             poNode->papoSubExpr[0]->field_index <
                 m_poFeatureDefn->GetFieldCount())
    {
        const OGRFieldDefn *poFieldDefn =
            m_poFeatureDefn->GetFieldDefn(poNode->papoSubExpr[0]->field_index);
        if (!poFieldDefn->IsNullable())
        {
            bAlwaysFalse = true;
            return nullptr;
        }
        auto qc = std::make_unique<tiledb::QueryCondition>(*(m_ctx.get()));
        qc->init(poFieldDefn->GetNameRef(), nullptr, 0, TILEDB_EQ);
        return qc;
    }

    // field_name IS NOT NULL
    else if (poNode->eNodeType == SNT_OPERATION &&
             poNode->nOperation == SWQ_NOT && poNode->nSubExprCount == 1 &&
             poNode->papoSubExpr[0]->nOperation == SWQ_ISNULL &&
             poNode->papoSubExpr[0]->nSubExprCount == 1 &&
             poNode->papoSubExpr[0]->papoSubExpr[0]->eNodeType == SNT_COLUMN &&
             poNode->papoSubExpr[0]->papoSubExpr[0]->field_index <
                 m_poFeatureDefn->GetFieldCount())
    {
        const OGRFieldDefn *poFieldDefn = m_poFeatureDefn->GetFieldDefn(
            poNode->papoSubExpr[0]->papoSubExpr[0]->field_index);
        if (!poFieldDefn->IsNullable())
        {
            bAlwaysTrue = true;
            return nullptr;
        }
        auto qc = std::make_unique<tiledb::QueryCondition>(*(m_ctx.get()));
        qc->init(poFieldDefn->GetNameRef(), nullptr, 0, TILEDB_NE);
        return qc;
    }

    m_bAttributeFilterPartiallyTranslated = true;
    return nullptr;
}

/***********************************************************************/
/*                         SetAttributeFilter()                        */
/***********************************************************************/

OGRErr OGRTileDBLayer::SetAttributeFilter(const char *pszFilter)
{
    m_bAttributeFilterPartiallyTranslated = false;
    m_poQueryCondition.reset();
    m_bAttributeFilterAlwaysFalse = false;
    m_bAttributeFilterAlwaysTrue = false;
    OGRErr eErr = OGRLayer::SetAttributeFilter(pszFilter);
    if (eErr != OGRERR_NONE)
        return eErr;

    if (m_poAttrQuery != nullptr)
    {
        if (m_nUseOptimizedAttributeFilter < 0)
        {
            m_nUseOptimizedAttributeFilter = CPLTestBool(CPLGetConfigOption(
                "OGR_TILEDB_OPTIMIZED_ATTRIBUTE_FILTER", "YES"));
        }
        if (m_nUseOptimizedAttributeFilter)
        {
            swq_expr_node *poNode =
                static_cast<swq_expr_node *>(m_poAttrQuery->GetSWQExpr());
            poNode->ReplaceBetweenByGEAndLERecurse();
            poNode->PushNotOperationDownToStack();
            bool bAlwaysTrue, bAlwaysFalse;
            CPLErrorReset();
            try
            {
                m_poQueryCondition =
                    CreateQueryCondition(poNode, bAlwaysTrue, bAlwaysFalse);
            }
            catch (const std::exception &e)
            {
                CPLError(CE_Failure, CPLE_AppDefined, "%s", e.what());
                return OGRERR_FAILURE;
            }
            if (CPLGetLastErrorType() == CE_Failure)
                return OGRERR_FAILURE;
            if (m_poQueryCondition && m_bAttributeFilterPartiallyTranslated)
            {
                CPLDebug("TILEDB", "Attribute filter partially translated to "
                                   "libtiledb query condition");
            }
            else if (!m_poQueryCondition)
            {
                CPLDebug("TILEDB", "Attribute filter could not be translated "
                                   "to libtiledb query condition");
            }
            m_bAttributeFilterAlwaysTrue = bAlwaysTrue;
            m_bAttributeFilterAlwaysFalse = bAlwaysFalse;
        }
    }

    return OGRERR_NONE;
}

/************************************************************************/
/*                        GetMetadataItem()                             */
/************************************************************************/

const char *OGRTileDBLayer::GetMetadataItem(const char *pszName,
                                            const char *pszDomain)
{
    if (pszDomain && EQUAL(pszDomain, "_DEBUG_"))
    {
        if (EQUAL(pszName, "ATTRIBUTE_FILTER_TRANSLATION"))
        {
            if (!m_poQueryCondition && !m_bAttributeFilterAlwaysFalse &&
                !m_bAttributeFilterAlwaysTrue)
                return "NONE";
            if (m_bAttributeFilterPartiallyTranslated)
                return "PARTIAL";
            return "WHOLE";
        }
    }
    return OGRLayer::GetMetadataItem(pszName, pszDomain);
}

/************************************************************************/
/*                    TranslateCurrentFeature()                         */
/************************************************************************/

OGRFeature *OGRTileDBLayer::TranslateCurrentFeature()
{
    auto poFeature = new OGRFeature(m_poFeatureDefn);

    poFeature->SetFID((*m_anFIDs)[m_nOffsetInResultSet]);

    // For a variable size attribute (list type), return the number of elements
    // for the feature of index m_nOffsetInResultSet.
    const auto GetEltCount = [this](const std::vector<uint64_t> &anOffsets,
                                    size_t nEltSizeInBytes,
                                    size_t nTotalSizeInBytes)
    {
        uint64_t nSize;
        if (static_cast<size_t>(m_nOffsetInResultSet + 1) < anOffsets.size())
        {
            nSize = anOffsets[m_nOffsetInResultSet + 1] -
                    anOffsets[m_nOffsetInResultSet];
        }
        else
        {
            nSize = nTotalSizeInBytes - anOffsets[m_nOffsetInResultSet];
        }
        return static_cast<size_t>(nSize / nEltSizeInBytes);
    };

    if (!m_poFeatureDefn->GetGeomFieldDefn(0)->IsIgnored())
    {
        const char *pszGeomColName = GetDatabaseGeomColName();
        if (pszGeomColName)
        {
            const size_t nWKBSize =
                GetEltCount(*m_anGeometryOffsets, 1, m_abyGeometries->size());
            OGRGeometry *poGeom = nullptr;
            OGRGeometryFactory::createFromWkb(
                m_abyGeometries->data() +
                    static_cast<size_t>(
                        (*m_anGeometryOffsets)[m_nOffsetInResultSet]),
                GetSpatialRef(), &poGeom, nWKBSize);
            poFeature->SetGeometryDirectly(poGeom);
        }
        else
        {
            OGRPoint *poPoint =
                m_adfZs->empty()
                    ? new OGRPoint((*m_adfXs)[m_nOffsetInResultSet],
                                   (*m_adfYs)[m_nOffsetInResultSet])
                    : new OGRPoint((*m_adfXs)[m_nOffsetInResultSet],
                                   (*m_adfYs)[m_nOffsetInResultSet],
                                   (*m_adfZs)[m_nOffsetInResultSet]);
            poPoint->assignSpatialReference(GetSpatialRef());
            poFeature->SetGeometryDirectly(poPoint);
        }
    }

    const int nFieldCount = m_poFeatureDefn->GetFieldCount();
    for (int i = 0; i < nFieldCount; ++i)
    {
        const OGRFieldDefn *poFieldDefn =
            m_poFeatureDefn->GetFieldDefnUnsafe(i);
        if (poFieldDefn->IsIgnored())
        {
            continue;
        }
        if (poFieldDefn->IsNullable())
        {
            if (!m_aFieldValidity[i][m_nOffsetInResultSet])
            {
                poFeature->SetFieldNull(i);
                continue;
            }
        }

        const auto &anOffsets = *(m_aFieldValueOffsets[i]);
        auto &fieldValues = m_aFieldValues[i];
        switch (poFieldDefn->GetType())
        {
            case OFTInteger:
            {
                if (m_aeFieldTypes[i] == TILEDB_BOOL)
                {
                    const auto &v = *(
                        std::get<std::shared_ptr<VECTOR_OF_BOOL>>(fieldValues));
                    poFeature->SetFieldSameTypeUnsafe(i,
                                                      v[m_nOffsetInResultSet]);
                }
                else if (m_aeFieldTypes[i] == TILEDB_INT16)
                {
                    const auto &v =
                        *(std::get<std::shared_ptr<std::vector<int16_t>>>(
                            fieldValues));
                    poFeature->SetFieldSameTypeUnsafe(i,
                                                      v[m_nOffsetInResultSet]);
                }
                else if (m_aeFieldTypes[i] == TILEDB_INT32)
                {
                    const auto &v =
                        *(std::get<std::shared_ptr<std::vector<int32_t>>>(
                            fieldValues));
                    poFeature->SetFieldSameTypeUnsafe(i,
                                                      v[m_nOffsetInResultSet]);
                }
                else if (m_aeFieldTypes[i] == TILEDB_UINT8)
                {
                    const auto &v =
                        *(std::get<std::shared_ptr<std::vector<uint8_t>>>(
                            fieldValues));
                    poFeature->SetFieldSameTypeUnsafe(i,
                                                      v[m_nOffsetInResultSet]);
                }
                else if (m_aeFieldTypes[i] == TILEDB_UINT16)
                {
                    const auto &v =
                        *(std::get<std::shared_ptr<std::vector<uint16_t>>>(
                            fieldValues));
                    poFeature->SetFieldSameTypeUnsafe(i,
                                                      v[m_nOffsetInResultSet]);
                }
                else
                {
                    CPLAssert(false);
                }
                break;
            }

            case OFTIntegerList:
            {
                if (m_aeFieldTypes[i] == TILEDB_BOOL)
                {
                    const auto &v = *(
                        std::get<std::shared_ptr<VECTOR_OF_BOOL>>(fieldValues));
                    const size_t nEltCount = GetEltCount(
                        anOffsets, sizeof(v[0]), v.size() * sizeof(v[0]));
                    std::vector<int32_t> tmp;
                    const auto *inPtr =
                        v.data() +
                        static_cast<size_t>(anOffsets[m_nOffsetInResultSet] /
                                            sizeof(v[0]));
                    for (size_t j = 0; j < nEltCount; ++j)
                        tmp.push_back(inPtr[j]);
                    poFeature->SetField(i, static_cast<int>(nEltCount),
                                        tmp.data());
                }
                else if (m_aeFieldTypes[i] == TILEDB_INT16)
                {
                    const auto &v =
                        *(std::get<std::shared_ptr<std::vector<int16_t>>>(
                            fieldValues));
                    const size_t nEltCount = GetEltCount(
                        anOffsets, sizeof(v[0]), v.size() * sizeof(v[0]));
                    std::vector<int32_t> tmp;
                    const auto *inPtr =
                        v.data() +
                        static_cast<size_t>(anOffsets[m_nOffsetInResultSet] /
                                            sizeof(v[0]));
                    for (size_t j = 0; j < nEltCount; ++j)
                        tmp.push_back(inPtr[j]);
                    poFeature->SetField(i, static_cast<int>(nEltCount),
                                        tmp.data());
                }
                else if (m_aeFieldTypes[i] == TILEDB_INT32)
                {
                    const auto &v =
                        *(std::get<std::shared_ptr<std::vector<int32_t>>>(
                            fieldValues));
                    const size_t nEltCount = GetEltCount(
                        anOffsets, sizeof(v[0]), v.size() * sizeof(v[0]));
                    poFeature->SetField(
                        i, static_cast<int>(nEltCount),
                        v.data() + static_cast<size_t>(
                                       anOffsets[m_nOffsetInResultSet] /
                                       sizeof(v[0])));
                }
                else if (m_aeFieldTypes[i] == TILEDB_UINT8)
                {
                    const auto &v =
                        *(std::get<std::shared_ptr<std::vector<uint8_t>>>(
                            fieldValues));
                    const size_t nEltCount = GetEltCount(
                        anOffsets, sizeof(v[0]), v.size() * sizeof(v[0]));
                    std::vector<int32_t> tmp;
                    const auto *inPtr =
                        v.data() +
                        static_cast<size_t>(anOffsets[m_nOffsetInResultSet] /
                                            sizeof(v[0]));
                    for (size_t j = 0; j < nEltCount; ++j)
                        tmp.push_back(inPtr[j]);
                    poFeature->SetField(i, static_cast<int>(nEltCount),
                                        tmp.data());
                }
                else if (m_aeFieldTypes[i] == TILEDB_UINT16)
                {
                    const auto &v =
                        *(std::get<std::shared_ptr<std::vector<uint16_t>>>(
                            fieldValues));
                    const size_t nEltCount = GetEltCount(
                        anOffsets, sizeof(v[0]), v.size() * sizeof(v[0]));
                    poFeature->SetField(
                        i, static_cast<int>(nEltCount),
                        v.data() + static_cast<size_t>(
                                       anOffsets[m_nOffsetInResultSet] /
                                       sizeof(v[0])));
                }
                else
                {
                    CPLAssert(false);
                }
                break;
            }

            case OFTInteger64:
            {
                const auto &v =
                    *(std::get<std::shared_ptr<std::vector<int64_t>>>(
                        fieldValues));
                poFeature->SetFieldSameTypeUnsafe(
                    i, static_cast<GIntBig>(v[m_nOffsetInResultSet]));
                break;
            }

            case OFTInteger64List:
            {
                const auto &v =
                    *(std::get<std::shared_ptr<std::vector<int64_t>>>(
                        fieldValues));
                const size_t nEltCount = GetEltCount(anOffsets, sizeof(v[0]),
                                                     v.size() * sizeof(v[0]));
                poFeature->SetField(
                    i, static_cast<int>(nEltCount),
                    v.data() +
                        static_cast<size_t>(anOffsets[m_nOffsetInResultSet] /
                                            sizeof(v[0])));
                break;
            }

            case OFTReal:
            {
                if (poFieldDefn->GetSubType() == OFSTFloat32)
                {
                    const auto &v =
                        *(std::get<std::shared_ptr<std::vector<float>>>(
                            fieldValues));
                    poFeature->SetFieldSameTypeUnsafe(i,
                                                      v[m_nOffsetInResultSet]);
                }
                else
                {
                    const auto &v =
                        *(std::get<std::shared_ptr<std::vector<double>>>(
                            fieldValues));
                    poFeature->SetFieldSameTypeUnsafe(i,
                                                      v[m_nOffsetInResultSet]);
                }
                break;
            }

            case OFTRealList:
            {
                if (poFieldDefn->GetSubType() == OFSTFloat32)
                {
                    const auto &v =
                        *(std::get<std::shared_ptr<std::vector<float>>>(
                            fieldValues));
                    const size_t nEltCount = GetEltCount(
                        anOffsets, sizeof(v[0]), v.size() * sizeof(v[0]));
                    std::vector<double> tmp;
                    const auto *inPtr =
                        v.data() +
                        static_cast<size_t>(anOffsets[m_nOffsetInResultSet] /
                                            sizeof(v[0]));
                    for (size_t j = 0; j < nEltCount; ++j)
                        tmp.push_back(inPtr[j]);
                    poFeature->SetField(i, static_cast<int>(nEltCount),
                                        tmp.data());
                }
                else
                {
                    const auto &v =
                        *(std::get<std::shared_ptr<std::vector<double>>>(
                            fieldValues));
                    const size_t nEltCount = GetEltCount(
                        anOffsets, sizeof(v[0]), v.size() * sizeof(v[0]));
                    poFeature->SetField(
                        i, static_cast<int>(nEltCount),
                        v.data() + static_cast<size_t>(
                                       anOffsets[m_nOffsetInResultSet] /
                                       sizeof(v[0])));
                }
                break;
            }

            case OFTString:
            {
                auto &v =
                    *(std::get<std::shared_ptr<std::string>>(fieldValues));
                const size_t nEltCount = GetEltCount(anOffsets, 1, v.size());
                if (static_cast<size_t>(m_nOffsetInResultSet + 1) <
                    anOffsets.size())
                {
                    char &chSavedRef =
                        v[static_cast<size_t>(anOffsets[m_nOffsetInResultSet]) +
                          nEltCount];
                    const char chSavedBackup = chSavedRef;
                    chSavedRef = 0;
                    poFeature->SetField(
                        i, v.data() + static_cast<size_t>(
                                          anOffsets[m_nOffsetInResultSet]));
                    chSavedRef = chSavedBackup;
                }
                else
                {
                    poFeature->SetField(
                        i, v.data() + static_cast<size_t>(
                                          anOffsets[m_nOffsetInResultSet]));
                }
                break;
            }

            case OFTBinary:
            {
                auto &v = *(std::get<std::shared_ptr<std::vector<uint8_t>>>(
                    fieldValues));
                const size_t nEltCount = GetEltCount(anOffsets, 1, v.size());
                poFeature->SetField(
                    i, static_cast<int>(nEltCount),
                    static_cast<const void *>(
                        v.data() +
                        static_cast<size_t>(anOffsets[m_nOffsetInResultSet])));
                break;
            }

            case OFTDate:
            {
                const auto &v =
                    *(std::get<std::shared_ptr<std::vector<int64_t>>>(
                        fieldValues));
                auto psField = poFeature->GetRawFieldRef(i);
                psField->Set.nMarker1 = OGRUnsetMarker;
                psField->Set.nMarker2 = OGRUnsetMarker;
                psField->Set.nMarker3 = OGRUnsetMarker;
                constexpr int DAYS_IN_YEAR_APPROX = 365;
                // Avoid overflow in the x SECONDS_PER_DAY muliplication
                if (v[m_nOffsetInResultSet] > DAYS_IN_YEAR_APPROX * 100000 ||
                    v[m_nOffsetInResultSet] < -DAYS_IN_YEAR_APPROX * 100000)
                {
                    CPLError(CE_Failure, CPLE_AppDefined, "Invalid date value");
                }
                else
                {
                    GIntBig timestamp =
                        static_cast<GIntBig>(v[m_nOffsetInResultSet]) *
                        SECONDS_PER_DAY;
                    struct tm dt;
                    CPLUnixTimeToYMDHMS(timestamp, &dt);

                    psField->Date.Year = static_cast<GInt16>(dt.tm_year + 1900);
                    psField->Date.Month = static_cast<GByte>(dt.tm_mon + 1);
                    psField->Date.Day = static_cast<GByte>(dt.tm_mday);
                    psField->Date.Hour = 0;
                    psField->Date.Minute = 0;
                    psField->Date.Second = 0;
                    psField->Date.TZFlag = 0;
                }
                break;
            }

            case OFTDateTime:
            {
                const auto &v =
                    *(std::get<std::shared_ptr<std::vector<int64_t>>>(
                        fieldValues));
                GIntBig timestamp =
                    static_cast<GIntBig>(v[m_nOffsetInResultSet]);
                double floatingPart = (timestamp % 1000) / 1e3;
                timestamp /= 1000;
                struct tm dt;
                CPLUnixTimeToYMDHMS(timestamp, &dt);
                auto psField = poFeature->GetRawFieldRef(i);
                psField->Set.nMarker1 = OGRUnsetMarker;
                psField->Set.nMarker2 = OGRUnsetMarker;
                psField->Set.nMarker3 = OGRUnsetMarker;
                psField->Date.Year = static_cast<GInt16>(dt.tm_year + 1900);
                psField->Date.Month = static_cast<GByte>(dt.tm_mon + 1);
                psField->Date.Day = static_cast<GByte>(dt.tm_mday);
                psField->Date.Hour = static_cast<GByte>(dt.tm_hour);
                psField->Date.Minute = static_cast<GByte>(dt.tm_min);
                psField->Date.Second =
                    static_cast<float>(dt.tm_sec + floatingPart);
                psField->Date.TZFlag = static_cast<GByte>(100);
                break;
            }

            case OFTTime:
            {
                const auto &v =
                    *(std::get<std::shared_ptr<std::vector<int64_t>>>(
                        fieldValues));
                GIntBig value = static_cast<GIntBig>(v[m_nOffsetInResultSet]);
                double floatingPart = (value % 1000) / 1e3;
                value /= 1000;
                auto psField = poFeature->GetRawFieldRef(i);
                psField->Set.nMarker1 = OGRUnsetMarker;
                psField->Set.nMarker2 = OGRUnsetMarker;
                psField->Set.nMarker3 = OGRUnsetMarker;
                psField->Date.Year = 0;
                psField->Date.Month = 0;
                psField->Date.Day = 0;
                const int nHour = static_cast<int>(value / 3600);
                const int nMinute = static_cast<int>((value / 60) % 60);
                const int nSecond = static_cast<int>(value % 60);
                psField->Date.Hour = static_cast<GByte>(nHour);
                psField->Date.Minute = static_cast<GByte>(nMinute);
                psField->Date.Second =
                    static_cast<float>(nSecond + floatingPart);
                psField->Date.TZFlag = 0;
                break;
            }

            default:
            {
                CPLAssert(false);
                break;
            }
        }
    }
    m_nOffsetInResultSet++;

    return poFeature;
}

/************************************************************************/
/*                         GetFeature()                                 */
/************************************************************************/

OGRFeature *OGRTileDBLayer::GetFeature(GIntBig nFID)
{
    if (m_osFIDColumn.empty())
        return OGRLayer::GetFeature(nFID);

    tiledb::QueryCondition qc(*(m_ctx.get()));
    qc.init(m_osFIDColumn, &nFID, sizeof(nFID), TILEDB_EQ);
    ResetReading();
    if (!SetupQuery(&qc))
        return nullptr;
    auto poFeat = TranslateCurrentFeature();
    ResetReading();
    return poFeat;
}

/************************************************************************/
/*                      GetFeatureCount()                               */
/************************************************************************/

GIntBig OGRTileDBLayer::GetFeatureCount(int bForce)
{
    if (!m_poAttrQuery && !m_poFilterGeom && m_nTotalFeatureCount >= 0)
        return m_nTotalFeatureCount;
    GIntBig nRet = OGRLayer::GetFeatureCount(bForce);
    if (nRet >= 0 && !m_poAttrQuery && !m_poFilterGeom)
        m_nTotalFeatureCount = nRet;
    return nRet;
}

/************************************************************************/
/*                          GetExtent()                                 */
/************************************************************************/

OGRErr OGRTileDBLayer::GetExtent(OGREnvelope *psExtent, int bForce)
{
    if (m_oLayerExtent.IsInit())
    {
        *psExtent = m_oLayerExtent;
        return OGRERR_NONE;
    }
    return OGRLayer::GetExtent(psExtent, bForce);
}

/************************************************************************/
/*                         ResetReading()                               */
/************************************************************************/

void OGRTileDBLayer::ResetReading()
{
    if (m_eCurrentMode == CurrentMode::WriteInProgress && !m_array)
        return;

    SwitchToReadingMode();
    ResetBuffers();
    m_nNextFID = 1;
    m_nOffsetInResultSet = 0;
    m_nRowCountInResultSet = 0;
    m_query.reset();
    m_bQueryComplete = false;
}

/************************************************************************/
/*                         CreateField()                                */
/************************************************************************/

OGRErr OGRTileDBLayer::CreateField(const OGRFieldDefn *poField,
                                   int /* bApproxOK*/)
{
    if (!m_bUpdatable)
        return OGRERR_FAILURE;
    if (m_schema)
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "Cannot add field after schema has been initialized");
        return OGRERR_FAILURE;
    }
    if (poField->GetType() == OFTStringList)
    {
        CPLError(CE_Failure, CPLE_NotSupported, "Unsupported field type");
        return OGRERR_FAILURE;
    }
    const char *pszFieldName = poField->GetNameRef();
    if (m_poFeatureDefn->GetFieldIndex(pszFieldName) >= 0 ||
        pszFieldName == m_osFIDColumn ||
        strcmp(pszFieldName, GetGeometryColumn()) == 0 ||
        pszFieldName == m_osXDim || pszFieldName == m_osYDim ||
        pszFieldName == m_osZDim)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "A field or dimension of same name (%s) already exists",
                 pszFieldName);
        return OGRERR_FAILURE;
    }
    OGRFieldDefn oFieldDefn(poField);
    m_poFeatureDefn->AddFieldDefn(&oFieldDefn);
    m_aeFieldTypesInCreateField.push_back(-1);
    if (poField->GetType() == OFTInteger ||
        poField->GetType() == OFTIntegerList)
    {
        const char *pszTileDBIntType =
            CPLGetConfigOption("TILEDB_INT_TYPE", "INT32");
        if (EQUAL(pszTileDBIntType, "UINT8"))
        {
            m_aeFieldTypesInCreateField.back() = TILEDB_UINT8;
        }
        else if (EQUAL(pszTileDBIntType, "UINT16"))
        {
            m_aeFieldTypesInCreateField.back() = TILEDB_UINT16;
        }
    }
    return OGRERR_NONE;
}

/************************************************************************/
/*                      InitializeSchemaAndArray()                      */
/************************************************************************/

void OGRTileDBLayer::InitializeSchemaAndArray()
{
    m_bInitializationAttempted = true;

    try
    {
        // create the tiledb schema
        // dimensions will be _x and _y, we can also add _z (2.5d)
        // set dimensions and attribute type for schema
        // we will use row-major for now but we could use hilbert indexing
        m_schema.reset(new tiledb::ArraySchema(*m_ctx, TILEDB_SPARSE));
        m_schema->set_tile_order(TILEDB_ROW_MAJOR);
        m_schema->set_cell_order(TILEDB_ROW_MAJOR);

        m_schema->set_coords_filter_list(*m_filterList);
        m_schema->set_offsets_filter_list(*m_filterList);

        tiledb::Domain domain(*m_ctx);

        auto xdim = tiledb::Dimension::create<double>(
            *m_ctx, m_osXDim, {m_dfXStart, m_dfXEnd}, m_dfTileExtent);
        auto ydim = tiledb::Dimension::create<double>(
            *m_ctx, m_osYDim, {m_dfYStart, m_dfYEnd}, m_dfTileExtent);
        if (!m_osZDim.empty())
        {
            auto zdim = tiledb::Dimension::create<double>(
                *m_ctx, m_osZDim, {m_dfZStart, m_dfZEnd}, m_dfZTileExtent);
            domain.add_dimensions(std::move(xdim), std::move(ydim),
                                  std::move(zdim));
        }
        else
        {
            domain.add_dimensions(std::move(xdim), std::move(ydim));
        }

        m_schema->set_domain(domain);

        m_schema->set_capacity(m_nTileCapacity);

        // allow geometries with same _X, _Y
        m_schema->set_allows_dups(true);

        // add FID attribute
        if (!m_osFIDColumn.empty())
        {
            m_schema->add_attribute(tiledb::Attribute::create<int64_t>(
                *m_ctx, m_osFIDColumn, *m_filterList));
        }

        // add geometry attribute
        const char *pszGeomColName = GetDatabaseGeomColName();
        if (pszGeomColName)
        {
            const char *pszWkbBlobType =
                CPLGetConfigOption("TILEDB_WKB_GEOMETRY_TYPE", "BLOB");
            auto wkbGeometryAttr = tiledb::Attribute::create(
                *m_ctx, pszGeomColName,
                EQUAL(pszWkbBlobType, "UINT8") ? TILEDB_UINT8 : TILEDB_BLOB);
            wkbGeometryAttr.set_filter_list(*m_filterList);
            wkbGeometryAttr.set_cell_val_num(TILEDB_VAR_NUM);
            m_schema->add_attribute(wkbGeometryAttr);
        }

        auto &aFieldValues = m_aFieldValues;
        CPLAssert(static_cast<int>(m_aeFieldTypesInCreateField.size()) ==
                  m_poFeatureDefn->GetFieldCount());
        for (int i = 0; i < m_poFeatureDefn->GetFieldCount(); i++)
        {
            const OGRFieldDefn *poFieldDefn = m_poFeatureDefn->GetFieldDefn(i);
            const bool bIsNullable = poFieldDefn->IsNullable();

            const auto CreateAttr =
                [this, poFieldDefn, bIsNullable](tiledb_datatype_t type,
                                                 bool bIsVariableSize = false)
            {
                m_aeFieldTypes.push_back(type);
                auto attr = tiledb::Attribute::create(
                    *m_ctx, poFieldDefn->GetNameRef(), m_aeFieldTypes.back());
                attr.set_filter_list(*m_filterList);
                attr.set_nullable(bIsNullable);
                if (bIsVariableSize)
                    attr.set_cell_val_num(TILEDB_VAR_NUM);
                m_schema->add_attribute(attr);
            };

            const auto eType = poFieldDefn->GetType();
            switch (eType)
            {
                case OFTInteger:
                case OFTIntegerList:
                {
                    if (poFieldDefn->GetSubType() == OFSTBoolean)
                    {
                        CreateAttr(TILEDB_BOOL, eType == OFTIntegerList);
                        aFieldValues.push_back(
                            std::make_shared<VECTOR_OF_BOOL>());
                    }
                    else if (poFieldDefn->GetSubType() == OFSTInt16)
                    {
                        CreateAttr(TILEDB_INT16, eType == OFTIntegerList);
                        aFieldValues.push_back(
                            std::make_shared<std::vector<int16_t>>());
                    }
                    else if (m_aeFieldTypesInCreateField[i] >= 0)
                    {
                        if (m_aeFieldTypesInCreateField[i] == TILEDB_UINT8)
                        {
                            CreateAttr(TILEDB_UINT8, eType == OFTIntegerList);
                            aFieldValues.push_back(
                                std::make_shared<std::vector<uint8_t>>());
                        }
                        else if (m_aeFieldTypesInCreateField[i] ==
                                 TILEDB_UINT16)
                        {
                            CreateAttr(TILEDB_UINT16, eType == OFTIntegerList);
                            aFieldValues.push_back(
                                std::make_shared<std::vector<uint16_t>>());
                        }
                        else
                        {
                            CPLAssert(false);
                        }
                    }
                    else
                    {
                        const char *pszTileDBIntType =
                            CPLGetConfigOption("TILEDB_INT_TYPE", "INT32");
                        if (EQUAL(pszTileDBIntType, "UINT8"))
                        {
                            CreateAttr(TILEDB_UINT8, eType == OFTIntegerList);
                            aFieldValues.push_back(
                                std::make_shared<std::vector<uint8_t>>());
                        }
                        else if (EQUAL(pszTileDBIntType, "UINT16"))
                        {
                            CreateAttr(TILEDB_UINT16, eType == OFTIntegerList);
                            aFieldValues.push_back(
                                std::make_shared<std::vector<uint16_t>>());
                        }
                        else
                        {
                            CreateAttr(TILEDB_INT32, eType == OFTIntegerList);
                            aFieldValues.push_back(
                                std::make_shared<std::vector<int32_t>>());
                        }
                    }
                    break;
                }

                case OFTInteger64:
                {
                    CreateAttr(TILEDB_INT64);
                    aFieldValues.push_back(
                        std::make_shared<std::vector<int64_t>>());
                    break;
                }

                case OFTInteger64List:
                {
                    CreateAttr(TILEDB_INT64, true);
                    aFieldValues.push_back(
                        std::make_shared<std::vector<int64_t>>());
                    break;
                }

                case OFTReal:
                case OFTRealList:
                {
                    if (poFieldDefn->GetSubType() == OFSTFloat32)
                    {
                        CreateAttr(TILEDB_FLOAT32, eType == OFTRealList);
                        aFieldValues.push_back(
                            std::make_shared<std::vector<float>>());
                    }
                    else
                    {
                        CreateAttr(TILEDB_FLOAT64, eType == OFTRealList);
                        aFieldValues.push_back(
                            std::make_shared<std::vector<double>>());
                    }
                    break;
                }

                case OFTString:
                {
                    CreateAttr(m_eTileDBStringType, true);
                    aFieldValues.push_back(std::make_shared<std::string>());
                    break;
                }

                case OFTDate:
                {
                    CreateAttr(TILEDB_DATETIME_DAY);
                    aFieldValues.push_back(
                        std::make_shared<std::vector<int64_t>>());
                    break;
                }

                case OFTDateTime:
                {
                    CreateAttr(TILEDB_DATETIME_MS);
                    aFieldValues.push_back(
                        std::make_shared<std::vector<int64_t>>());
                    break;
                }

                case OFTTime:
                {
                    CreateAttr(TILEDB_TIME_MS);
                    aFieldValues.push_back(
                        std::make_shared<std::vector<int64_t>>());
                    break;
                }

                case OFTBinary:
                {
                    const char *pszBlobType =
                        CPLGetConfigOption("TILEDB_BINARY_TYPE", "BLOB");
                    CreateAttr(EQUAL(pszBlobType, "UINT8") ? TILEDB_UINT8
                                                           : TILEDB_BLOB,
                               true);
                    aFieldValues.push_back(
                        std::make_shared<std::vector<uint8_t>>());
                    break;
                }

                default:
                {
                    CPLError(CE_Failure, CPLE_NoWriteAccess,
                             "Unsupported attribute definition.\n");
                    return;
                }
            }
        }

        for (int i = 0; i < m_poFeatureDefn->GetFieldCount(); i++)
        {
            m_aFieldValueOffsets.push_back(
                std::make_shared<std::vector<uint64_t>>());
        }
        m_aFieldValidity.resize(m_poFeatureDefn->GetFieldCount());

        tiledb::Array::create(m_osFilename, *m_schema);

        if (!m_osGroupName.empty())
        {
            tiledb::Group group(*m_ctx, m_osGroupName, TILEDB_WRITE);
            group.add_member(m_osFilename, false, GetDescription());
        }

        if (m_nTimestamp)
            m_array.reset(new tiledb::Array(
                *m_ctx, m_osFilename, TILEDB_WRITE,
                tiledb::TemporalPolicy(tiledb::TimeTravel, m_nTimestamp)));
        else
            m_array.reset(
                new tiledb::Array(*m_ctx, m_osFilename, TILEDB_WRITE));

        if (!m_osFIDColumn.empty())
        {
            m_array->put_metadata("FID_ATTRIBUTE_NAME", TILEDB_STRING_UTF8,
                                  static_cast<int>(m_osFIDColumn.size()),
                                  m_osFIDColumn.c_str());
        }

        if (pszGeomColName)
        {
            m_array->put_metadata("GEOMETRY_ATTRIBUTE_NAME", TILEDB_STRING_UTF8,
                                  static_cast<int>(strlen(pszGeomColName)),
                                  pszGeomColName);
        }

        m_array->put_metadata("dataset_type", TILEDB_STRING_UTF8,
                              static_cast<int>(strlen(GEOMETRY_DATASET_TYPE)),
                              GEOMETRY_DATASET_TYPE);

        auto poSRS = GetSpatialRef();
        if (poSRS)
        {
            char *pszStr = nullptr;
            poSRS->exportToPROJJSON(&pszStr, nullptr);
            if (!pszStr)
                poSRS->exportToWkt(&pszStr, nullptr);
            if (pszStr)
            {
                m_array->put_metadata("CRS", TILEDB_STRING_UTF8,
                                      static_cast<int>(strlen(pszStr)), pszStr);
            }
            CPLFree(pszStr);
        }

        const auto GetStringGeometryType = [](OGRwkbGeometryType eType)
        {
            const auto eFlattenType = wkbFlatten(eType);
            std::string osType = "Unknown";
            if (wkbPoint == eFlattenType)
                osType = "Point";
            else if (wkbLineString == eFlattenType)
                osType = "LineString";
            else if (wkbPolygon == eFlattenType)
                osType = "Polygon";
            else if (wkbMultiPoint == eFlattenType)
                osType = "MultiPoint";
            else if (wkbMultiLineString == eFlattenType)
                osType = "MultiLineString";
            else if (wkbMultiPolygon == eFlattenType)
                osType = "MultiPolygon";
            else if (wkbGeometryCollection == eFlattenType)
                osType = "GeometryCollection";
            else if (wkbCircularString == eFlattenType)
                osType = "CircularString";
            else if (wkbCompoundCurve == eFlattenType)
                osType = "CompoundCurve";
            else if (wkbCurvePolygon == eFlattenType)
                osType = "CurvePolygon";
            else if (wkbMultiCurve == eFlattenType)
                osType = "MultiCurve";
            else if (wkbMultiSurface == eFlattenType)
                osType = "MultiSurface";
            else if (wkbPolyhedralSurface == eFlattenType)
                osType = "PolyhedralSurface";
            else if (wkbTIN == eFlattenType)
                osType = "TIN";

            if (OGR_GT_HasZ(eType) && OGR_GT_HasM(eType))
                osType += " ZM";
            else if (OGR_GT_HasZ(eType))
                osType += " Z";
            else if (OGR_GT_HasM(eType))
                osType += " M";

            return osType;
        };
        const auto eGeomType = GetGeomType();
        const std::string osGeometryType = GetStringGeometryType(eGeomType);
        m_array->put_metadata("GeometryType", TILEDB_STRING_ASCII,
                              static_cast<int>(osGeometryType.size()),
                              osGeometryType.data());

        m_bInitialized = true;
    }
    catch (const tiledb::TileDBError &e)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "InitializeSchemaAndArray() failed: %s", e.what());
    }
}

/************************************************************************/
/*                       SwitchToWritingMode()                          */
/************************************************************************/

void OGRTileDBLayer::SwitchToWritingMode()
{
    if (m_eCurrentMode != CurrentMode::WriteInProgress)
    {
        m_nNextFID = GetFeatureCount(true) + 1;
        if (m_eCurrentMode == CurrentMode::ReadInProgress)
        {
            m_eCurrentMode = CurrentMode::None;
            ResetBuffers();
        }

        m_query.reset();
        m_array.reset();

        try
        {
            if (m_nTimestamp)
                m_array.reset(new tiledb::Array(
                    *m_ctx, m_osFilename, TILEDB_WRITE,
                    tiledb::TemporalPolicy(tiledb::TimeTravel, m_nTimestamp)));
            else
                m_array.reset(
                    new tiledb::Array(*m_ctx, m_osFilename, TILEDB_WRITE));
        }
        catch (const std::exception &e)
        {
            CPLError(CE_Failure, CPLE_AppDefined, "%s", e.what());
            return;
        }
    }
    m_eCurrentMode = CurrentMode::WriteInProgress;
}

/************************************************************************/
/*                         ICreateFeature()                             */
/************************************************************************/

OGRErr OGRTileDBLayer::ICreateFeature(OGRFeature *poFeature)
{
    if (!m_bUpdatable)
        return OGRERR_FAILURE;

    SwitchToWritingMode();

    if (!m_bInitializationAttempted)
    {
        InitializeSchemaAndArray();
    }
    if (!m_bInitialized)
        return OGRERR_FAILURE;

    if (!m_array)
        return OGRERR_FAILURE;

    const OGRGeometry *poGeom = poFeature->GetGeometryRef();
    if (!poGeom || poGeom->IsEmpty())
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "Features without geometry (or with empty geometry) are not "
                 "supported");
        return OGRERR_FAILURE;
    }

    if (GetDatabaseGeomColName())
    {
        const size_t nWkbSize = poGeom->WkbSize();
        std::vector<unsigned char> aGeometry(nWkbSize);
        poGeom->exportToWkb(wkbNDR, aGeometry.data(), wkbVariantIso);
        m_abyGeometries->insert(m_abyGeometries->end(), aGeometry.begin(),
                                aGeometry.end());
        if (m_anGeometryOffsets->empty())
            m_anGeometryOffsets->push_back(0);
        m_anGeometryOffsets->push_back(m_anGeometryOffsets->back() + nWkbSize);
    }
    else if (wkbFlatten(poGeom->getGeometryType()) != wkbPoint)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Cannot write non-Point geometry in a layer without a "
                 "geometry attribute");
        return OGRERR_FAILURE;
    }

    int64_t nFID = poFeature->GetFID();
    if (nFID < 0)
    {
        nFID = m_nNextFID++;
        poFeature->SetFID(nFID);
    }
    if (!m_osFIDColumn.empty())
        m_anFIDs->push_back(nFID);

    const int nFieldCount = m_poFeatureDefn->GetFieldCountUnsafe();
    for (int i = 0; i < nFieldCount; i++)
    {
        const OGRFieldDefn *poFieldDefn = m_poFeatureDefn->GetFieldDefn(i);
        const bool bFieldIsValid = poFeature->IsFieldSetAndNotNull(i);
        auto &anOffsets = *(m_aFieldValueOffsets[i]);
        if (poFieldDefn->IsNullable())
        {
            m_aFieldValidity[i].push_back(bFieldIsValid);
        }
        else
        {
            if (!bFieldIsValid)
            {
                CPLError(CE_Warning, CPLE_AppDefined,
                         "Field %d of feature " CPL_FRMT_GIB
                         " is null or unset, "
                         "but field is declared as not nullable. Readers "
                         "will see an incorrect value",
                         i, static_cast<GIntBig>(nFID));
            }
        }
        auto &fieldValues = m_aFieldValues[i];

        switch (poFieldDefn->GetType())
        {
            case OFTInteger:
            {
                const int nVal =
                    bFieldIsValid ? poFeature->GetFieldAsIntegerUnsafe(i) : 0;
                if (m_aeFieldTypes[i] == TILEDB_BOOL)
                    std::get<std::shared_ptr<VECTOR_OF_BOOL>>(fieldValues)
                        ->push_back(static_cast<uint8_t>(nVal));
                else if (m_aeFieldTypes[i] == TILEDB_INT16)
                    std::get<std::shared_ptr<std::vector<int16_t>>>(fieldValues)
                        ->push_back(static_cast<int16_t>(nVal));
                else if (m_aeFieldTypes[i] == TILEDB_INT32)
                    std::get<std::shared_ptr<std::vector<int32_t>>>(fieldValues)
                        ->push_back(nVal);
                else if (m_aeFieldTypes[i] == TILEDB_UINT8)
                    std::get<std::shared_ptr<std::vector<uint8_t>>>(fieldValues)
                        ->push_back(static_cast<uint8_t>(nVal));
                else if (m_aeFieldTypes[i] == TILEDB_UINT16)
                    std::get<std::shared_ptr<std::vector<uint16_t>>>(
                        fieldValues)
                        ->push_back(static_cast<uint16_t>(nVal));
                else
                {
                    CPLAssert(false);
                }
                break;
            }

            case OFTIntegerList:
            {
                int nCount = 0;
                const int *panVal =
                    poFeature->GetFieldAsIntegerList(i, &nCount);
                if (anOffsets.empty())
                    anOffsets.push_back(0);
                if (m_aeFieldTypes[i] == TILEDB_BOOL)
                {
                    auto &v = *(
                        std::get<std::shared_ptr<VECTOR_OF_BOOL>>(fieldValues));
                    for (int j = 0; j < nCount; ++j)
                        v.push_back(static_cast<uint8_t>(panVal[j]));
                    anOffsets.push_back(anOffsets.back() +
                                        nCount * sizeof(v[0]));
                }
                else if (m_aeFieldTypes[i] == TILEDB_INT16)
                {
                    auto &v = *(std::get<std::shared_ptr<std::vector<int16_t>>>(
                        fieldValues));
                    for (int j = 0; j < nCount; ++j)
                        v.push_back(static_cast<int16_t>(panVal[j]));
                    anOffsets.push_back(anOffsets.back() +
                                        nCount * sizeof(v[0]));
                }
                else if (m_aeFieldTypes[i] == TILEDB_INT32)
                {
                    auto &v = *(std::get<std::shared_ptr<std::vector<int32_t>>>(
                        fieldValues));
                    v.insert(v.end(), panVal, panVal + nCount);
                    anOffsets.push_back(anOffsets.back() +
                                        nCount * sizeof(v[0]));
                }
                else if (m_aeFieldTypes[i] == TILEDB_UINT8)
                {
                    auto &v = *(std::get<std::shared_ptr<std::vector<uint8_t>>>(
                        fieldValues));
                    for (int j = 0; j < nCount; ++j)
                        v.push_back(static_cast<uint8_t>(panVal[j]));
                    anOffsets.push_back(anOffsets.back() +
                                        nCount * sizeof(v[0]));
                }
                else if (m_aeFieldTypes[i] == TILEDB_UINT16)
                {
                    auto &v =
                        *(std::get<std::shared_ptr<std::vector<uint16_t>>>(
                            fieldValues));
                    for (int j = 0; j < nCount; ++j)
                        v.push_back(static_cast<uint16_t>(panVal[j]));
                    anOffsets.push_back(anOffsets.back() +
                                        nCount * sizeof(v[0]));
                }
                else
                {
                    CPLAssert(false);
                }
                break;
            }

            case OFTInteger64:
            {
                std::get<std::shared_ptr<std::vector<int64_t>>>(fieldValues)
                    ->push_back(bFieldIsValid
                                    ? poFeature->GetFieldAsInteger64Unsafe(i)
                                    : 0);
                break;
            }

            case OFTInteger64List:
            {
                int nCount = 0;
                const int64_t *panVal = reinterpret_cast<const int64_t *>(
                    poFeature->GetFieldAsInteger64List(i, &nCount));
                if (anOffsets.empty())
                    anOffsets.push_back(0);
                auto &v = *(std::get<std::shared_ptr<std::vector<int64_t>>>(
                    fieldValues));
                v.insert(v.end(), panVal, panVal + nCount);
                anOffsets.push_back(anOffsets.back() + nCount * sizeof(v[0]));
                break;
            }

            case OFTReal:
            {
                const double dfVal =
                    bFieldIsValid ? poFeature->GetFieldAsDoubleUnsafe(i)
                                  : std::numeric_limits<double>::quiet_NaN();
                if (poFieldDefn->GetSubType() == OFSTFloat32)
                    std::get<std::shared_ptr<std::vector<float>>>(fieldValues)
                        ->push_back(static_cast<float>(dfVal));
                else
                    std::get<std::shared_ptr<std::vector<double>>>(fieldValues)
                        ->push_back(dfVal);
                break;
            }

            case OFTRealList:
            {
                int nCount = 0;
                const double *padfVal =
                    poFeature->GetFieldAsDoubleList(i, &nCount);
                if (anOffsets.empty())
                    anOffsets.push_back(0);
                if (poFieldDefn->GetSubType() == OFSTFloat32)
                {
                    auto &v = *(std::get<std::shared_ptr<std::vector<float>>>(
                        fieldValues));
                    for (int j = 0; j < nCount; ++j)
                        v.push_back(static_cast<float>(padfVal[j]));
                    anOffsets.push_back(anOffsets.back() +
                                        nCount * sizeof(v[0]));
                }
                else
                {
                    auto &v = *(std::get<std::shared_ptr<std::vector<double>>>(
                        fieldValues));
                    v.insert(v.end(), padfVal, padfVal + nCount);
                    anOffsets.push_back(anOffsets.back() +
                                        nCount * sizeof(v[0]));
                }
                break;
            }

            case OFTString:
            {
                const char *pszValue =
                    bFieldIsValid ? poFeature->GetFieldAsStringUnsafe(i)
                                  : nullptr;
                const size_t nValueLen = pszValue ? strlen(pszValue) : 0;
                if (pszValue)
                {
                    auto &v =
                        *(std::get<std::shared_ptr<std::string>>(fieldValues));
                    v.insert(v.end(), pszValue, pszValue + nValueLen);
                }
                if (anOffsets.empty())
                    anOffsets.push_back(0);
                anOffsets.push_back(anOffsets.back() + nValueLen);
                break;
            }

            case OFTBinary:
            {
                int nCount = 0;
                const GByte *pabyValue =
                    poFeature->GetFieldAsBinary(i, &nCount);
                auto &v = *(std::get<std::shared_ptr<std::vector<uint8_t>>>(
                    fieldValues));
                v.insert(v.end(), pabyValue, pabyValue + nCount);
                if (anOffsets.empty())
                    anOffsets.push_back(0);
                anOffsets.push_back(anOffsets.back() + nCount);
                break;
            }

            case OFTDate:
            {
                auto &v = *(std::get<std::shared_ptr<std::vector<int64_t>>>(
                    fieldValues));
                if (bFieldIsValid)
                {
                    const auto poRawField = poFeature->GetRawFieldRef(i);
                    v.push_back(OGRFieldToDateDay(*poRawField));
                }
                else
                {
                    v.push_back(0);
                }
                break;
            }

            case OFTDateTime:
            {
                auto &v = *(std::get<std::shared_ptr<std::vector<int64_t>>>(
                    fieldValues));
                if (bFieldIsValid)
                {
                    const auto poRawField = poFeature->GetRawFieldRef(i);
                    v.push_back(OGRFieldToDateTimeMS(*poRawField));
                }
                else
                {
                    v.push_back(0);
                }
                break;
            }

            case OFTTime:
            {
                auto &v = *(std::get<std::shared_ptr<std::vector<int64_t>>>(
                    fieldValues));
                if (bFieldIsValid)
                {
                    const auto poRawField = poFeature->GetRawFieldRef(i);
                    v.push_back(OGRFieldToTimeMS(*poRawField));
                }
                else
                {
                    v.push_back(0);
                }
                break;
            }

            default:
            {
                CPLError(CE_Failure, CPLE_NoWriteAccess,
                         "Unsupported attribute definition.\n");
                return OGRERR_FAILURE;
            }
        }
    }

    OGREnvelope sEnvelope;
    OGREnvelope3D sEnvelope3D;
    if (!m_osZDim.empty())
    {
        poGeom->getEnvelope(&sEnvelope3D);
        sEnvelope = sEnvelope3D;
    }
    else
    {
        poGeom->getEnvelope(&sEnvelope);
    }

    m_oLayerExtent.Merge(sEnvelope);

    // use mid point of envelope
    m_adfXs->push_back(sEnvelope.MinX +
                       ((sEnvelope.MaxX - sEnvelope.MinX) / 2.0));
    m_adfYs->push_back(sEnvelope.MinY +
                       ((sEnvelope.MaxY - sEnvelope.MinY) / 2.0));

    // Compute maximum "radius" of a geometry around its mid point,
    // for later spatial requests
    m_dfPadX = std::max(m_dfPadX, (sEnvelope.MaxX - sEnvelope.MinX) / 2);
    m_dfPadY = std::max(m_dfPadY, (sEnvelope.MaxY - sEnvelope.MinY) / 2);

    if (!m_osZDim.empty())
    {
        m_adfZs->push_back(sEnvelope3D.MinZ +
                           ((sEnvelope3D.MaxZ - sEnvelope3D.MinZ) / 2.0));
        m_dfPadZ =
            std::max(m_dfPadZ, (sEnvelope3D.MaxZ - sEnvelope3D.MinZ) / 2);
    }

    if (m_nTotalFeatureCount < 0)
        m_nTotalFeatureCount = 1;
    else
        ++m_nTotalFeatureCount;

    if (m_adfXs->size() == m_nBatchSize)
    {
        try
        {
            FlushArrays();
        }
        catch (const std::exception &e)
        {
            CPLError(CE_Failure, CPLE_AppDefined, "%s", e.what());
            return OGRERR_FAILURE;
        }
    }

    return OGRERR_NONE;
}

/************************************************************************/
/*                          FlushArrays()                               */
/************************************************************************/

void OGRTileDBLayer::FlushArrays()
{
    CPLDebug("TILEDB", "Flush %d records", static_cast<int>(m_adfXs->size()));

    try
    {
        tiledb::Query query(*m_ctx, *m_array);
        query.set_layout(TILEDB_UNORDERED);
        if (!m_osFIDColumn.empty())
            query.set_data_buffer(m_osFIDColumn, *m_anFIDs);
        query.set_data_buffer(m_osXDim, *m_adfXs);
        query.set_data_buffer(m_osYDim, *m_adfYs);
        if (!m_osZDim.empty())
            query.set_data_buffer(m_osZDim, *m_adfZs);

        const char *pszGeomColName = GetDatabaseGeomColName();
        if (pszGeomColName)
        {
            m_anGeometryOffsets->pop_back();
            if (m_schema->attribute(pszGeomColName).type() == TILEDB_UINT8)
            {
                query.set_data_buffer(pszGeomColName, *m_abyGeometries);
                query.set_offsets_buffer(pszGeomColName, *m_anGeometryOffsets);
            }
            else if (m_schema->attribute(pszGeomColName).type() == TILEDB_BLOB)
            {
                query.set_data_buffer(
                    pszGeomColName,
                    reinterpret_cast<std::byte *>(m_abyGeometries->data()),
                    m_abyGeometries->size());
                query.set_offsets_buffer(pszGeomColName,
                                         m_anGeometryOffsets->data(),
                                         m_anGeometryOffsets->size());
            }
            else
            {
                CPLAssert(false);
            }
        }

        for (int i = 0; i < m_poFeatureDefn->GetFieldCount(); i++)
        {
            const OGRFieldDefn *poFieldDefn = m_poFeatureDefn->GetFieldDefn(i);
            const char *pszFieldName = poFieldDefn->GetNameRef();
            auto &anOffsets = *(m_aFieldValueOffsets[i]);
            auto &fieldValues = m_aFieldValues[i];

            if (poFieldDefn->IsNullable())
                query.set_validity_buffer(pszFieldName, m_aFieldValidity[i]);

            const auto eType = poFieldDefn->GetType();
            switch (eType)
            {
                case OFTInteger:
                case OFTIntegerList:
                {
                    if (eType == OFTIntegerList)
                    {
                        anOffsets.pop_back();
                        query.set_offsets_buffer(pszFieldName, anOffsets);
                    }

                    if (m_aeFieldTypes[i] == TILEDB_BOOL)
                    {
                        auto &v = *(std::get<std::shared_ptr<VECTOR_OF_BOOL>>(
                            fieldValues));
#ifdef VECTOR_OF_BOOL_IS_NOT_UINT8_T
                        query.set_data_buffer(pszFieldName, v.data(), v.size());
#else
                        query.set_data_buffer(pszFieldName, v);
#endif
                    }
                    else if (m_aeFieldTypes[i] == TILEDB_INT16)
                    {
                        auto &v =
                            *(std::get<std::shared_ptr<std::vector<int16_t>>>(
                                fieldValues));
                        query.set_data_buffer(pszFieldName, v);
                    }
                    else if (m_aeFieldTypes[i] == TILEDB_INT32)
                    {
                        auto &v =
                            *(std::get<std::shared_ptr<std::vector<int32_t>>>(
                                fieldValues));
                        query.set_data_buffer(pszFieldName, v);
                    }
                    else if (m_aeFieldTypes[i] == TILEDB_UINT8)
                    {
                        auto &v =
                            *(std::get<std::shared_ptr<std::vector<uint8_t>>>(
                                fieldValues));
                        query.set_data_buffer(pszFieldName, v);
                    }
                    else if (m_aeFieldTypes[i] == TILEDB_UINT16)
                    {
                        auto &v =
                            *(std::get<std::shared_ptr<std::vector<uint16_t>>>(
                                fieldValues));
                        query.set_data_buffer(pszFieldName, v);
                    }
                    break;
                }

                case OFTInteger64:
                {
                    query.set_data_buffer(
                        pszFieldName,
                        *std::get<std::shared_ptr<std::vector<int64_t>>>(
                            fieldValues));
                    break;
                }

                case OFTInteger64List:
                {
                    anOffsets.pop_back();
                    query.set_data_buffer(
                        pszFieldName,
                        *std::get<std::shared_ptr<std::vector<int64_t>>>(
                            fieldValues));
                    query.set_offsets_buffer(pszFieldName, anOffsets);
                    break;
                }

                case OFTReal:
                {
                    if (poFieldDefn->GetSubType() == OFSTFloat32)
                    {
                        query.set_data_buffer(
                            pszFieldName,
                            *std::get<std::shared_ptr<std::vector<float>>>(
                                fieldValues));
                    }
                    else
                    {
                        query.set_data_buffer(
                            pszFieldName,
                            *std::get<std::shared_ptr<std::vector<double>>>(
                                fieldValues));
                    }
                    break;
                }

                case OFTRealList:
                {
                    anOffsets.pop_back();
                    if (poFieldDefn->GetSubType() == OFSTFloat32)
                    {
                        query.set_data_buffer(
                            pszFieldName,
                            *std::get<std::shared_ptr<std::vector<float>>>(
                                fieldValues));
                        query.set_offsets_buffer(pszFieldName, anOffsets);
                    }
                    else
                    {
                        query.set_data_buffer(
                            pszFieldName,
                            *std::get<std::shared_ptr<std::vector<double>>>(
                                fieldValues));
                        query.set_offsets_buffer(pszFieldName, anOffsets);
                    }
                    break;
                }

                case OFTString:
                {
                    anOffsets.pop_back();
                    query.set_data_buffer(
                        pszFieldName,
                        *std::get<std::shared_ptr<std::string>>(fieldValues));
                    query.set_offsets_buffer(pszFieldName, anOffsets);
                    break;
                }

                case OFTBinary:
                {
                    anOffsets.pop_back();
                    auto &v = *(std::get<std::shared_ptr<std::vector<uint8_t>>>(
                        fieldValues));
                    if (m_aeFieldTypes[i] == TILEDB_UINT8)
                    {
                        query.set_data_buffer(pszFieldName, v);
                        query.set_offsets_buffer(pszFieldName, anOffsets);
                    }
                    else if (m_aeFieldTypes[i] == TILEDB_BLOB)
                    {
                        query.set_data_buffer(
                            pszFieldName,
                            reinterpret_cast<std::byte *>(v.data()), v.size());
                        query.set_offsets_buffer(pszFieldName, anOffsets.data(),
                                                 anOffsets.size());
                    }
                    else
                    {
                        CPLAssert(false);
                    }
                    break;
                }

                case OFTDate:
                case OFTDateTime:
                case OFTTime:
                {
                    query.set_data_buffer(
                        pszFieldName,
                        *std::get<std::shared_ptr<std::vector<int64_t>>>(
                            fieldValues));
                    break;
                }

                default:
                {
                    CPLAssert(false);
                    break;
                }
            }
        }

        if (m_bStats)
            tiledb::Stats::enable();

        query.submit();

        if (m_bStats)
        {
            tiledb::Stats::dump(stdout);
            tiledb::Stats::disable();
        }
    }
    catch (const std::exception &)
    {
        ResetBuffers();
        throw;
    }
    ResetBuffers();
}

/************************************************************************/
/*                          ResetBuffers()                              */
/************************************************************************/

namespace
{
template <class T> struct ClearArray
{
    static void exec(OGRTileDBLayer::ArrayType &array)
    {
        std::get<std::shared_ptr<T>>(array)->clear();
    }
};
}  // namespace

void OGRTileDBLayer::ResetBuffers()
{
    if (!m_bArrowBatchReleased)
    {
        AllocateNewBuffers();
    }
    else
    {
        // Reset buffers
        m_anFIDs->clear();
        m_adfXs->clear();
        m_adfYs->clear();
        m_adfZs->clear();
        m_abyGeometries->clear();
        m_anGeometryOffsets->clear();
        for (int i = 0; i < m_poFeatureDefn->GetFieldCount(); i++)
        {
            m_aFieldValueOffsets[i]->clear();
            m_aFieldValidity[i].clear();
            ProcessField<ClearArray>::exec(m_aeFieldTypes[i],
                                           m_aFieldValues[i]);
        }
    }
}

/************************************************************************/
/*                         TestCapability()                             */
/************************************************************************/

int OGRTileDBLayer::TestCapability(const char *pszCap)
{
    if (EQUAL(pszCap, OLCCreateField))
        return m_bUpdatable && m_schema == nullptr;

    if (EQUAL(pszCap, OLCSequentialWrite))
        return m_bUpdatable;

    if (EQUAL(pszCap, OLCFastFeatureCount))
        return (!m_poAttrQuery && !m_poFilterGeom && m_nTotalFeatureCount >= 0);

    if (EQUAL(pszCap, OLCFastGetExtent))
        return m_oLayerExtent.IsInit();

    if (EQUAL(pszCap, OLCStringsAsUTF8))
        return true;

    if (EQUAL(pszCap, OLCCurveGeometries))
        return true;

    if (EQUAL(pszCap, OLCMeasuredGeometries))
        return true;

    if (EQUAL(pszCap, OLCFastSpatialFilter))
        return true;

    if (EQUAL(pszCap, OLCIgnoreFields))
        return true;

    if (EQUAL(pszCap, OLCFastGetArrowStream))
        return true;

    return false;
}

/************************************************************************/
/*                         GetArrowSchema()                             */
/************************************************************************/

int OGRTileDBLayer::GetArrowSchema(struct ArrowArrayStream *out_stream,
                                   struct ArrowSchema *out_schema)
{
    int ret = OGRLayer::GetArrowSchema(out_stream, out_schema);
    if (ret != 0)
        return ret;

    // Patch integer fields
    const bool bIncludeFID = CPLTestBool(
        m_aosArrowArrayStreamOptions.FetchNameValueDef("INCLUDE_FID", "YES"));
    const int nFieldCount = m_poFeatureDefn->GetFieldCount();
    int iSchemaChild = bIncludeFID ? 1 : 0;
    for (int i = 0; i < nFieldCount; ++i)
    {
        const auto poFieldDefn = m_poFeatureDefn->GetFieldDefn(i);
        if (poFieldDefn->IsIgnored())
        {
            continue;
        }
        const auto eType = poFieldDefn->GetType();
        if (eType == OFTInteger || eType == OFTIntegerList)
        {
            const char *&format =
                eType == OFTInteger
                    ? out_schema->children[iSchemaChild]->format
                    : out_schema->children[iSchemaChild]->children[0]->format;
            if (m_aeFieldTypes[i] == TILEDB_BOOL)
                format = "b";
            else if (m_aeFieldTypes[i] == TILEDB_INT16)
                format = "s";
            else if (m_aeFieldTypes[i] == TILEDB_INT32)
                format = "i";
            else if (m_aeFieldTypes[i] == TILEDB_UINT8)
                format = "C";
            else if (m_aeFieldTypes[i] == TILEDB_UINT16)
                format = "S";
            else
            {
                CPLAssert(false);
            }
        }
        ++iSchemaChild;
    }

    // Patch other fields
    for (int64_t i = 0; i < out_schema->n_children; ++i)
    {
        const char *&format = out_schema->children[i]->format;
        if (strcmp(format, "+l") == 0)
        {
            // 32-bit list to 64-bit list
            format = "+L";
        }
        else if (strcmp(format, "u") == 0)
        {
            // 32-bit string to 64-bit string
            format = "U";
        }
        else if (strcmp(format, "z") == 0)
        {
            // 32-bit binary to 64-bit binary
            format = "Z";
        }
    }
    return 0;
}

/************************************************************************/
/*                        ReleaseArrowArray()                           */
/************************************************************************/

void OGRTileDBLayer::ReleaseArrowArray(struct ArrowArray *array)
{
    for (int i = 0; i < static_cast<int>(array->n_children); ++i)
    {
        if (array->children[i] && array->children[i]->release)
        {
            array->children[i]->release(array->children[i]);
            CPLFree(array->children[i]);
        }
    }
    CPLFree(array->children);
    CPLFree(array->buffers);

    OGRTileDBArrowArrayPrivateData *psPrivateData =
        static_cast<OGRTileDBArrowArrayPrivateData *>(array->private_data);
    if (psPrivateData->m_pbLayerStillAlive &&
        *psPrivateData->m_pbLayerStillAlive)
    {
        psPrivateData->m_poLayer->m_bArrowBatchReleased = true;
    }
    delete psPrivateData;
    array->private_data = nullptr;
    array->release = nullptr;
}

/************************************************************************/
/*                            SetNullBuffer()                           */
/************************************************************************/

void OGRTileDBLayer::SetNullBuffer(
    struct ArrowArray *psChild, int iField,
    const std::vector<bool> &abyValidityFromFilters)
{
    if (m_poFeatureDefn->GetFieldDefn(iField)->IsNullable())
    {
        // TileDB used a std::vector<uint8_t> with 1 element per byte
        // whereas Arrow uses a ~ std::vector<bool> with 8 elements per byte
        OGRTileDBArrowArrayPrivateData *psPrivateData =
            static_cast<OGRTileDBArrowArrayPrivateData *>(
                psChild->private_data);
        const auto &v_validity = m_aFieldValidity[iField];
        uint8_t *pabyNull = nullptr;
        const size_t nSrcSize = static_cast<size_t>(m_nRowCountInResultSet);
        if (abyValidityFromFilters.empty())
        {
            for (size_t i = 0; i < nSrcSize; ++i)
            {
                if (!v_validity[i])
                {
                    ++psChild->null_count;
                    if (pabyNull == nullptr)
                    {
                        psPrivateData->nullHolder =
                            std::make_shared<std::vector<uint8_t>>(
                                (nSrcSize + 7) / 8, static_cast<uint8_t>(0xFF));
                        pabyNull = psPrivateData->nullHolder->data();
                        psChild->buffers[0] = pabyNull;
                    }
                    pabyNull[i / 8] &= static_cast<uint8_t>(~(1 << (i % 8)));
                }
            }
        }
        else
        {
            for (size_t i = 0, j = 0; i < nSrcSize; ++i)
            {
                if (abyValidityFromFilters[i])
                {
                    if (!v_validity[i])
                    {
                        ++psChild->null_count;
                        if (pabyNull == nullptr)
                        {
                            const size_t nDstSize =
                                static_cast<size_t>(psChild->length);
                            psPrivateData->nullHolder =
                                std::make_shared<std::vector<uint8_t>>(
                                    (nDstSize + 7) / 8,
                                    static_cast<uint8_t>(0xFF));
                            pabyNull = psPrivateData->nullHolder->data();
                            psChild->buffers[0] = pabyNull;
                        }
                        pabyNull[j / 8] &=
                            static_cast<uint8_t>(~(1 << (j % 8)));
                    }
                    ++j;
                }
            }
        }
    }
}

/************************************************************************/
/*                           FillBoolArray()                            */
/************************************************************************/

void OGRTileDBLayer::FillBoolArray(
    struct ArrowArray *psChild, int iField,
    const std::vector<bool> &abyValidityFromFilters)
{
    OGRTileDBArrowArrayPrivateData *psPrivateData =
        new OGRTileDBArrowArrayPrivateData;
    psChild->private_data = psPrivateData;

    psChild->n_buffers = 2;
    psChild->buffers = static_cast<const void **>(CPLCalloc(2, sizeof(void *)));
    // TileDB used a std::vector<uint8_t> with 1 element per byte
    // whereas Arrow uses a ~ std::vector<bool> with 8 elements per byte
    const auto &v_source =
        *(std::get<std::shared_ptr<VECTOR_OF_BOOL>>(m_aFieldValues[iField]));
    const size_t nDstSize = abyValidityFromFilters.empty()
                                ? v_source.size()
                                : static_cast<size_t>(psChild->length);
    auto arrayValues =
        std::make_shared<std::vector<uint8_t>>((nDstSize + 7) / 8);
    psPrivateData->valueHolder = arrayValues;
    auto panValues = arrayValues->data();
    psChild->buffers[1] = panValues;
    if (abyValidityFromFilters.empty())
    {
        for (size_t i = 0; i < v_source.size(); ++i)
        {
            if (v_source[i])
            {
                panValues[i / 8] |= static_cast<uint8_t>(1 << (i % 8));
            }
        }
    }
    else
    {
        for (size_t i = 0, j = 0; i < v_source.size(); ++i)
        {
            if (abyValidityFromFilters[i])
            {
                if (v_source[i])
                {
                    panValues[j / 8] |= static_cast<uint8_t>(1 << (j % 8));
                }
                ++j;
            }
        }
    }

    SetNullBuffer(psChild, iField, abyValidityFromFilters);
}

/************************************************************************/
/*                        FillPrimitiveArray()                          */
/************************************************************************/

template <typename T>
void OGRTileDBLayer::FillPrimitiveArray(
    struct ArrowArray *psChild, int iField,
    const std::vector<bool> &abyValidityFromFilters)
{
    OGRTileDBArrowArrayPrivateData *psPrivateData =
        new OGRTileDBArrowArrayPrivateData;
    psChild->private_data = psPrivateData;

    psChild->n_buffers = 2;
    psChild->buffers = static_cast<const void **>(CPLCalloc(2, sizeof(void *)));
    auto &v_source =
        std::get<std::shared_ptr<std::vector<T>>>(m_aFieldValues[iField]);
    psPrivateData->valueHolder = v_source;
    psChild->buffers[1] = v_source->data();

    if (!abyValidityFromFilters.empty())
    {
        const size_t nSrcSize = static_cast<size_t>(m_nRowCountInResultSet);
        for (size_t i = 0, j = 0; i < nSrcSize; ++i)
        {
            if (abyValidityFromFilters[i])
            {
                (*v_source)[j] = (*v_source)[i];
                ++j;
            }
        }
    }

    SetNullBuffer(psChild, iField, abyValidityFromFilters);
}

/************************************************************************/
/*                      FillStringOrBinaryArray()                       */
/************************************************************************/

template <typename T>
void OGRTileDBLayer::FillStringOrBinaryArray(
    struct ArrowArray *psChild, int iField,
    const std::vector<bool> &abyValidityFromFilters)
{
    OGRTileDBArrowArrayPrivateData *psPrivateData =
        new OGRTileDBArrowArrayPrivateData;
    psChild->private_data = psPrivateData;

    psChild->n_buffers = 3;
    psChild->buffers = static_cast<const void **>(CPLCalloc(3, sizeof(void *)));

    auto &v_source = std::get<std::shared_ptr<T>>(m_aFieldValues[iField]);

    psPrivateData->offsetHolder = m_aFieldValueOffsets[iField];
    // Add back extra offset
    if (!psPrivateData->offsetHolder->empty())
        psPrivateData->offsetHolder->push_back(v_source->size());
    psChild->buffers[1] = psPrivateData->offsetHolder->data();

    psPrivateData->valueHolder = v_source;
    psChild->buffers[2] = v_source->data();

    if (!abyValidityFromFilters.empty())
    {
        const size_t nSrcSize = static_cast<size_t>(m_nRowCountInResultSet);
        size_t nAccLen = 0;
        for (size_t i = 0, j = 0; i < nSrcSize; ++i)
        {
            if (abyValidityFromFilters[i])
            {
                const size_t nSrcOffset =
                    static_cast<size_t>((*psPrivateData->offsetHolder)[i]);
                const size_t nNextOffset =
                    static_cast<size_t>((*psPrivateData->offsetHolder)[i + 1]);
                const size_t nItemLen = nNextOffset - nSrcOffset;
                (*psPrivateData->offsetHolder)[j] = nAccLen;
                if (nItemLen && nAccLen < nSrcOffset)
                {
                    memmove(v_source->data() + nAccLen,
                            v_source->data() + nSrcOffset, nItemLen);
                }
                nAccLen += nItemLen;
                ++j;
            }
        }
        (*psPrivateData->offsetHolder)[static_cast<size_t>(psChild->length)] =
            nAccLen;
    }

    SetNullBuffer(psChild, iField, abyValidityFromFilters);
}

/************************************************************************/
/*                      FillTimeOrDateArray()                           */
/************************************************************************/

void OGRTileDBLayer::FillTimeOrDateArray(
    struct ArrowArray *psChild, int iField,
    const std::vector<bool> &abyValidityFromFilters)
{
    // TileDB uses 64-bit for time[ms], whereas Arrow uses 32-bit
    // Idem for date[day]
    OGRTileDBArrowArrayPrivateData *psPrivateData =
        new OGRTileDBArrowArrayPrivateData;
    psChild->private_data = psPrivateData;

    psChild->n_buffers = 2;
    psChild->buffers = static_cast<const void **>(CPLCalloc(2, sizeof(void *)));

    const auto &v_source = *(std::get<std::shared_ptr<std::vector<int64_t>>>(
        m_aFieldValues[iField]));
    const size_t nDstSize = abyValidityFromFilters.empty()
                                ? v_source.size()
                                : static_cast<size_t>(psChild->length);
    auto newValuesPtr = std::make_shared<std::vector<int32_t>>(nDstSize);
    psPrivateData->valueHolder = newValuesPtr;
    auto &newValues = *newValuesPtr;

    if (abyValidityFromFilters.empty())
    {
        for (size_t i = 0; i < v_source.size(); ++i)
        {
            newValues[i] = static_cast<int32_t>(v_source[i]);
        }
    }
    else
    {
        for (size_t i = 0, j = 0; i < v_source.size(); ++i)
        {
            if (abyValidityFromFilters[i])
            {
                newValues[j] = static_cast<int32_t>(v_source[i]);
                ++j;
            }
        }
    }
    psChild->buffers[1] = newValuesPtr->data();

    SetNullBuffer(psChild, iField, abyValidityFromFilters);
}

/************************************************************************/
/*                      FillPrimitiveListArray()                        */
/************************************************************************/

template <typename T>
void OGRTileDBLayer::FillPrimitiveListArray(
    struct ArrowArray *psChild, int iField,
    const std::vector<bool> &abyValidityFromFilters)
{
    OGRTileDBArrowArrayPrivateData *psPrivateData =
        new OGRTileDBArrowArrayPrivateData;
    psChild->private_data = psPrivateData;

    // We cannot direct use m_aFieldValueOffsets as it uses offsets in
    // bytes whereas Arrow uses offsets in number of elements
    psChild->n_buffers = 2;
    psChild->buffers = static_cast<const void **>(CPLCalloc(2, sizeof(void *)));
    auto offsetsPtr = std::make_shared<std::vector<uint64_t>>();
    const auto &offsetsSrc = *(m_aFieldValueOffsets[iField]);
    const size_t nSrcVals = offsetsSrc.size();
    if (abyValidityFromFilters.empty())
    {
        offsetsPtr->reserve(nSrcVals + 1);
    }
    else
    {
        offsetsPtr->reserve(static_cast<size_t>(psChild->length) + 1);
    }
    psPrivateData->offsetHolder = offsetsPtr;
    auto &offsets = *offsetsPtr;
    auto &v_source =
        std::get<std::shared_ptr<std::vector<T>>>(m_aFieldValues[iField]);

    if (abyValidityFromFilters.empty())
    {
        for (size_t i = 0; i < nSrcVals; ++i)
            offsets.push_back(offsetsSrc[i] / sizeof(T));
        offsets.push_back(v_source->size());
    }
    else
    {
        size_t nAccLen = 0;
        for (size_t i = 0; i < nSrcVals; ++i)
        {
            if (abyValidityFromFilters[i])
            {
                const auto nSrcOffset =
                    static_cast<size_t>(offsetsSrc[i] / sizeof(T));
                const auto nNextOffset = i + 1 < nSrcVals
                                             ? offsetsSrc[i + 1] / sizeof(T)
                                             : v_source->size();
                const size_t nItemLen =
                    static_cast<size_t>(nNextOffset - nSrcOffset);
                offsets.push_back(nAccLen);
                if (nItemLen && nAccLen < nSrcOffset)
                {
                    memmove(v_source->data() + nAccLen,
                            v_source->data() + nSrcOffset,
                            nItemLen * sizeof(T));
                }
                nAccLen += nItemLen;
            }
        }
        offsets.push_back(nAccLen);
    }

    psChild->buffers[1] = offsetsPtr->data();

    SetNullBuffer(psChild, iField, abyValidityFromFilters);

    psChild->n_children = 1;
    psChild->children = static_cast<struct ArrowArray **>(
        CPLCalloc(1, sizeof(struct ArrowArray *)));
    psChild->children[0] = static_cast<struct ArrowArray *>(
        CPLCalloc(1, sizeof(struct ArrowArray)));
    auto psValueChild = psChild->children[0];

    psValueChild->release = psChild->release;

    psValueChild->n_buffers = 2;
    psValueChild->buffers =
        static_cast<const void **>(CPLCalloc(2, sizeof(void *)));
    psValueChild->length = offsets.back();

    psPrivateData = new OGRTileDBArrowArrayPrivateData;
    psValueChild->private_data = psPrivateData;
    psPrivateData->valueHolder = v_source;
    psValueChild->buffers[1] = v_source->data();
}

/************************************************************************/
/*                        FillBoolListArray()                           */
/************************************************************************/

void OGRTileDBLayer::FillBoolListArray(
    struct ArrowArray *psChild, int iField,
    const std::vector<bool> &abyValidityFromFilters)
{
    OGRTileDBArrowArrayPrivateData *psPrivateData =
        new OGRTileDBArrowArrayPrivateData;
    psChild->private_data = psPrivateData;

    psChild->n_buffers = 2;
    psChild->buffers = static_cast<const void **>(CPLCalloc(2, sizeof(void *)));
    auto &offsetsPtr = m_aFieldValueOffsets[iField];
    psPrivateData->offsetHolder = offsetsPtr;
    auto &v_source =
        *(std::get<std::shared_ptr<VECTOR_OF_BOOL>>(m_aFieldValues[iField]));

    psChild->n_children = 1;
    psChild->children = static_cast<struct ArrowArray **>(
        CPLCalloc(1, sizeof(struct ArrowArray *)));
    psChild->children[0] = static_cast<struct ArrowArray *>(
        CPLCalloc(1, sizeof(struct ArrowArray)));
    auto psValueChild = psChild->children[0];

    psValueChild->release = psChild->release;

    psValueChild->n_buffers = 2;
    psValueChild->buffers =
        static_cast<const void **>(CPLCalloc(2, sizeof(void *)));

    psPrivateData = new OGRTileDBArrowArrayPrivateData;
    psValueChild->private_data = psPrivateData;

    // TileDB used a std::vector<uint8_t> with 1 element per byte
    // whereas Arrow uses a ~ std::vector<bool> with 8 elements per byte
    auto arrayValues =
        std::make_shared<std::vector<uint8_t>>((v_source.size() + 7) / 8);
    psPrivateData->valueHolder = arrayValues;
    auto panValues = arrayValues->data();
    psValueChild->buffers[1] = panValues;

    if (abyValidityFromFilters.empty())
    {
        offsetsPtr->push_back(v_source.size());

        for (size_t iFeat = 0; iFeat < v_source.size(); ++iFeat)
        {
            if (v_source[iFeat])
                panValues[iFeat / 8] |= static_cast<uint8_t>(1 << (iFeat % 8));
        }

        psValueChild->length = v_source.size();
    }
    else
    {
        CPLAssert(offsetsPtr->size() > static_cast<size_t>(psChild->length));

        auto &offsets = *offsetsPtr;
        const size_t nSrcVals = offsets.size();
        size_t nAccLen = 0;
        for (size_t i = 0, j = 0; i < nSrcVals; ++i)
        {
            if (abyValidityFromFilters[i])
            {
                const auto nSrcOffset = static_cast<size_t>(offsets[i]);
                const auto nNextOffset =
                    i + 1 < nSrcVals ? offsets[i + 1] : v_source.size();
                const size_t nItemLen =
                    static_cast<size_t>(nNextOffset - nSrcOffset);
                offsets[j] = nAccLen;
                for (size_t k = 0; k < nItemLen; ++k)
                {
                    if (v_source[nSrcOffset + k])
                    {
                        panValues[(nAccLen + k) / 8] |=
                            static_cast<uint8_t>(1 << ((nAccLen + k) % 8));
                    }
                }
                ++j;
                nAccLen += nItemLen;
            }
        }
        offsets[static_cast<size_t>(psChild->length)] = nAccLen;

        psValueChild->length = nAccLen;
    }

    psChild->buffers[1] = offsetsPtr->data();

    SetNullBuffer(psChild, iField, abyValidityFromFilters);
}

/************************************************************************/
/*                        GetNextArrowArray()                           */
/************************************************************************/

int OGRTileDBLayer::GetNextArrowArray(struct ArrowArrayStream *stream,
                                      struct ArrowArray *out_array)
{
    memset(out_array, 0, sizeof(*out_array));

    if (m_eCurrentMode == CurrentMode::WriteInProgress)
    {
        ResetReading();
    }
    if (!m_array)
        return 0;
    if (m_bQueryComplete)
        return 0;

    const size_t nBatchSizeBackup = m_nBatchSize;
    const char *pszBatchSize =
        m_aosArrowArrayStreamOptions.FetchNameValue("MAX_FEATURES_IN_BATCH");
    if (pszBatchSize)
        m_nBatchSize = atoi(pszBatchSize);
    if (m_nBatchSize > INT_MAX - 1)
        m_nBatchSize = INT_MAX - 1;
    const bool bSetupOK = SetupQuery(nullptr);
    m_nBatchSize = nBatchSizeBackup;
    if (!bSetupOK)
        return 0;

    const bool bIncludeFID = CPLTestBool(
        m_aosArrowArrayStreamOptions.FetchNameValueDef("INCLUDE_FID", "YES"));

    int nChildren = 0;
    if (bIncludeFID)
    {
        nChildren++;
    }
    const int nFieldCount = m_poFeatureDefn->GetFieldCount();
    for (int i = 0; i < nFieldCount; i++)
    {
        const auto poFieldDefn = m_poFeatureDefn->GetFieldDefn(i);
        if (!poFieldDefn->IsIgnored())
        {
            nChildren++;
        }
    }
    for (int i = 0; i < m_poFeatureDefn->GetGeomFieldCount(); i++)
    {
        if (!m_poFeatureDefn->GetGeomFieldDefn(i)->IsIgnored())
        {
            nChildren++;
        }
    }
    out_array->length = m_nRowCountInResultSet;
    out_array->n_children = nChildren;
    out_array->children =
        (struct ArrowArray **)CPLCalloc(sizeof(struct ArrowArray *), nChildren);

    // Allocate list of parent buffers: no nulls, null bitmap can be omitted
    out_array->n_buffers = 1;
    out_array->buffers =
        static_cast<const void **>(CPLCalloc(1, sizeof(void *)));

    {
        OGRTileDBArrowArrayPrivateData *psPrivateData =
            new OGRTileDBArrowArrayPrivateData;
        if (m_bArrowBatchReleased)
        {
            psPrivateData->m_poLayer = this;
            psPrivateData->m_pbLayerStillAlive = m_pbLayerStillAlive;
        }
        out_array->private_data = psPrivateData;
    }
    out_array->release = OGRTileDBLayer::ReleaseArrowArray;

    std::vector<bool> abyValidityFromFilters;
    size_t nCountIntersecting = 0;
    if (!m_anGeometryOffsets->empty())
    {
        // Add back extra offset
        m_anGeometryOffsets->push_back(m_abyGeometries->size());

        // Given that the TileDB filtering is based only on the center point
        // of geometries, we need to refine it a bit from the actual WKB we get
        if (m_poFilterGeom && (m_dfPadX > 0 || m_dfPadY > 0))
        {
            const size_t nSrcVals = static_cast<size_t>(m_nRowCountInResultSet);
            abyValidityFromFilters.resize(nSrcVals);
            OGREnvelope sEnvelope;
            size_t nAccLen = 0;
            for (size_t i = 0; i < nSrcVals; ++i)
            {
                const auto nSrcOffset =
                    static_cast<size_t>((*m_anGeometryOffsets)[i]);
                const auto nNextOffset =
                    static_cast<size_t>((*m_anGeometryOffsets)[i + 1]);
                const auto nItemLen = nNextOffset - nSrcOffset;
                const GByte *pabyWKB = m_abyGeometries->data() + nSrcOffset;
                const size_t nWKBSize = nItemLen;
                if (FilterWKBGeometry(pabyWKB, nWKBSize,
                                      /* bEnvelopeAlreadySet=*/false,
                                      sEnvelope))
                {
                    abyValidityFromFilters[i] = true;
                    (*m_anGeometryOffsets)[nCountIntersecting] = nAccLen;
                    if (nItemLen && nAccLen < nSrcOffset)
                    {
                        memmove(m_abyGeometries->data() + nAccLen,
                                m_abyGeometries->data() + nSrcOffset, nItemLen);
                    }
                    nAccLen += nItemLen;
                    nCountIntersecting++;
                }
            }
            (*m_anGeometryOffsets)[nCountIntersecting] = nAccLen;

            if (nCountIntersecting == m_nRowCountInResultSet)
            {
                abyValidityFromFilters.clear();
            }
            else
            {
                out_array->length = nCountIntersecting;
            }
        }
    }

    int iSchemaChild = 0;
    if (bIncludeFID)
    {
        out_array->children[iSchemaChild] = static_cast<struct ArrowArray *>(
            CPLCalloc(1, sizeof(struct ArrowArray)));
        auto psChild = out_array->children[iSchemaChild];
        ++iSchemaChild;
        OGRTileDBArrowArrayPrivateData *psPrivateData =
            new OGRTileDBArrowArrayPrivateData;
        psPrivateData->valueHolder = m_anFIDs;
        psChild->private_data = psPrivateData;
        psChild->release = OGRTileDBLayer::ReleaseArrowArray;
        psChild->length = out_array->length;
        psChild->n_buffers = 2;
        psChild->buffers =
            static_cast<const void **>(CPLCalloc(2, sizeof(void *)));
        if (!abyValidityFromFilters.empty())
        {
            for (size_t i = 0, j = 0; i < m_nRowCountInResultSet; ++i)
            {
                if (abyValidityFromFilters[i])
                {
                    (*m_anFIDs)[j] = (*m_anFIDs)[i];
                    ++j;
                }
            }
        }
        psChild->buffers[1] = m_anFIDs->data();
    }

    try
    {
        for (int i = 0; i < nFieldCount; ++i)
        {
            const auto poFieldDefn = m_poFeatureDefn->GetFieldDefn(i);
            if (poFieldDefn->IsIgnored())
            {
                continue;
            }

            out_array->children[iSchemaChild] =
                static_cast<struct ArrowArray *>(
                    CPLCalloc(1, sizeof(struct ArrowArray)));
            auto psChild = out_array->children[iSchemaChild];
            ++iSchemaChild;
            psChild->release = OGRTileDBLayer::ReleaseArrowArray;
            psChild->length = out_array->length;
            const auto eSubType = poFieldDefn->GetSubType();
            switch (poFieldDefn->GetType())
            {
                case OFTInteger:
                {
                    if (m_aeFieldTypes[i] == TILEDB_BOOL)
                    {
                        FillBoolArray(psChild, i, abyValidityFromFilters);
                    }
                    else if (m_aeFieldTypes[i] == TILEDB_INT16)
                    {
                        FillPrimitiveArray<int16_t>(psChild, i,
                                                    abyValidityFromFilters);
                    }
                    else if (m_aeFieldTypes[i] == TILEDB_INT32)
                    {
                        FillPrimitiveArray<int32_t>(psChild, i,
                                                    abyValidityFromFilters);
                    }
                    else if (m_aeFieldTypes[i] == TILEDB_UINT8)
                    {
                        FillPrimitiveArray<uint8_t>(psChild, i,
                                                    abyValidityFromFilters);
                    }
                    else if (m_aeFieldTypes[i] == TILEDB_UINT16)
                    {
                        FillPrimitiveArray<uint16_t>(psChild, i,
                                                     abyValidityFromFilters);
                    }
                    else
                    {
                        CPLAssert(false);
                    }
                    break;
                }

                case OFTIntegerList:
                {
                    if (m_aeFieldTypes[i] == TILEDB_BOOL)
                    {
                        FillBoolListArray(psChild, i, abyValidityFromFilters);
                    }
                    else if (m_aeFieldTypes[i] == TILEDB_INT16)
                    {
                        FillPrimitiveListArray<int16_t>(psChild, i,
                                                        abyValidityFromFilters);
                    }
                    else if (m_aeFieldTypes[i] == TILEDB_INT32)
                    {
                        FillPrimitiveListArray<int32_t>(psChild, i,
                                                        abyValidityFromFilters);
                    }
                    else if (m_aeFieldTypes[i] == TILEDB_UINT8)
                    {
                        FillPrimitiveListArray<uint8_t>(psChild, i,
                                                        abyValidityFromFilters);
                    }
                    else if (m_aeFieldTypes[i] == TILEDB_UINT16)
                    {
                        FillPrimitiveListArray<uint16_t>(
                            psChild, i, abyValidityFromFilters);
                    }
                    else
                    {
                        CPLAssert(false);
                    }
                    break;
                }

                case OFTInteger64:
                case OFTDateTime:
                {
                    FillPrimitiveArray<int64_t>(psChild, i,
                                                abyValidityFromFilters);
                    break;
                }

                case OFTInteger64List:
                {
                    FillPrimitiveListArray<int64_t>(psChild, i,
                                                    abyValidityFromFilters);
                    break;
                }

                case OFTReal:
                {
                    if (eSubType == OFSTFloat32)
                    {
                        FillPrimitiveArray<float>(psChild, i,
                                                  abyValidityFromFilters);
                    }
                    else
                    {
                        FillPrimitiveArray<double>(psChild, i,
                                                   abyValidityFromFilters);
                    }
                    break;
                }

                case OFTRealList:
                {
                    if (eSubType == OFSTFloat32)
                    {
                        FillPrimitiveListArray<float>(psChild, i,
                                                      abyValidityFromFilters);
                    }
                    else
                    {
                        FillPrimitiveListArray<double>(psChild, i,
                                                       abyValidityFromFilters);
                    }
                    break;
                }

                case OFTString:
                {
                    FillStringOrBinaryArray<std::string>(
                        psChild, i, abyValidityFromFilters);
                    break;
                }

                case OFTBinary:
                {
                    FillStringOrBinaryArray<std::vector<uint8_t>>(
                        psChild, i, abyValidityFromFilters);
                    break;
                }

                case OFTTime:
                case OFTDate:
                {
                    FillTimeOrDateArray(psChild, i, abyValidityFromFilters);
                    break;
                }

                case OFTStringList:
                case OFTWideString:
                case OFTWideStringList:
                    break;
            }
        }

        if (!m_poFeatureDefn->GetGeomFieldDefn(0)->IsIgnored())
        {
            out_array->children[iSchemaChild] =
                static_cast<struct ArrowArray *>(
                    CPLCalloc(1, sizeof(struct ArrowArray)));
            auto psChild = out_array->children[iSchemaChild];
            ++iSchemaChild;
            psChild->release = OGRTileDBLayer::ReleaseArrowArray;
            psChild->length = out_array->length;

            OGRTileDBArrowArrayPrivateData *psPrivateData =
                new OGRTileDBArrowArrayPrivateData;
            psChild->private_data = psPrivateData;

            psChild->n_buffers = 3;
            psChild->buffers =
                static_cast<const void **>(CPLCalloc(3, sizeof(void *)));

            if (!m_anGeometryOffsets->empty() || m_adfXs->empty())
            {
                psPrivateData->offsetHolder = m_anGeometryOffsets;
                psChild->buffers[1] = m_anGeometryOffsets->data();

                psPrivateData->valueHolder = m_abyGeometries;
                psChild->buffers[2] = m_abyGeometries->data();
            }
            else
            {
                // Build Point WKB from X/Y/Z arrays

                const int nDims = m_osZDim.empty() ? 2 : 3;
                const size_t nPointWKBSize = 5 + nDims * sizeof(double);

                auto offsets = std::make_shared<std::vector<uint64_t>>();
                psPrivateData->offsetHolder = offsets;
                offsets->reserve(m_adfXs->size());

                auto pabyWKB = std::make_shared<std::vector<unsigned char>>();
                pabyWKB->reserve(nPointWKBSize * m_adfXs->size());
                psPrivateData->valueHolder = pabyWKB;

                unsigned char wkbHeader[5];
                wkbHeader[0] = static_cast<unsigned char>(wkbNDR);
                uint32_t wkbType = wkbPoint + ((nDims == 3) ? 1000 : 0);
                CPL_LSBPTR32(&wkbType);
                memcpy(wkbHeader + 1, &wkbType, sizeof(uint32_t));
                double *padfX = m_adfXs->data();
                double *padfY = m_adfYs->data();
                double *padfZ = m_adfZs->data();
                uint64_t nOffset = 0;
                for (size_t i = 0; i < m_adfXs->size(); ++i)
                {
                    pabyWKB->insert(pabyWKB->end(), wkbHeader,
                                    wkbHeader + sizeof(wkbHeader));
                    unsigned char *x =
                        reinterpret_cast<unsigned char *>(&padfX[i]);
                    CPL_LSBPTR64(x);
                    pabyWKB->insert(pabyWKB->end(), x, x + sizeof(double));
                    CPL_LSBPTR64(x);
                    unsigned char *y =
                        reinterpret_cast<unsigned char *>(&padfY[i]);
                    CPL_LSBPTR64(y);
                    pabyWKB->insert(pabyWKB->end(), y, y + sizeof(double));
                    CPL_LSBPTR64(y);
                    if (nDims == 3)
                    {
                        unsigned char *z =
                            reinterpret_cast<unsigned char *>(&padfZ[i]);
                        CPL_LSBPTR64(z);
                        pabyWKB->insert(pabyWKB->end(), z, z + sizeof(double));
                        CPL_LSBPTR64(z);
                    }
                    offsets->push_back(nOffset);
                    nOffset += nPointWKBSize;
                }
                offsets->push_back(nOffset);

                psChild->buffers[1] = offsets->data();
                psChild->buffers[2] = pabyWKB->data();
            }
        }
        CPL_IGNORE_RET_VAL(iSchemaChild);

        if (m_poAttrQuery &&
            (!m_poQueryCondition || m_bAttributeFilterPartiallyTranslated))
        {
            struct ArrowSchema schema;
            stream->get_schema(stream, &schema);
            CPLAssert(schema.release != nullptr);
            CPLAssert(schema.n_children == out_array->n_children);
            // Spatial filter already evaluated
            auto poFilterGeomBackup = m_poFilterGeom;
            m_poFilterGeom = nullptr;
            if (CanPostFilterArrowArray(&schema))
                PostFilterArrowArray(&schema, out_array, nullptr);
            schema.release(&schema);
            m_poFilterGeom = poFilterGeomBackup;
        }
    }
    catch (const std::exception &e)
    {
        CPLError(CE_Failure, CPLE_OutOfMemory, "%s", e.what());
        out_array->release(out_array);
        memset(out_array, 0, sizeof(*out_array));
        return ENOMEM;
    }

    m_bArrowBatchReleased = false;

    return 0;
}
