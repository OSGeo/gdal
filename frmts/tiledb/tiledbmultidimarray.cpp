/******************************************************************************
 *
 * Project:  GDAL TileDB Driver
 * Purpose:  Implement GDAL TileDB multidimensional support based on https://www.tiledb.io
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

#include "tiledbmultidim.h"

#include <algorithm>
#include <limits>

/************************************************************************/
/*                   TileDBArray::TileDBArray()                         */
/************************************************************************/

TileDBArray::TileDBArray(
    const std::shared_ptr<TileDBSharedResource> &poSharedResource,
    const std::string &osParentName, const std::string &osName,
    const std::vector<std::shared_ptr<GDALDimension>> &aoDims,
    const GDALExtendedDataType &oType, const std::string &osPath)
    : GDALAbstractMDArray(osParentName, osName),
      GDALMDArray(osParentName, osName), m_poSharedResource(poSharedResource),
      m_aoDims(aoDims), m_oType(oType), m_osPath(osPath),
      m_bStats(poSharedResource->GetDumpStats())
{
}

/************************************************************************/
/*                   TileDBArray::Create()                              */
/************************************************************************/

/*static*/ std::shared_ptr<TileDBArray> TileDBArray::Create(
    const std::shared_ptr<TileDBSharedResource> &poSharedResource,
    const std::string &osParentName, const std::string &osName,
    const std::vector<std::shared_ptr<GDALDimension>> &aoDims,
    const GDALExtendedDataType &oType, const std::string &osPath)
{
    auto poArray = std::shared_ptr<TileDBArray>(new TileDBArray(
        poSharedResource, osParentName, osName, aoDims, oType, osPath));
    poArray->SetSelf(poArray);
    return poArray;
}

/************************************************************************/
/*                   TileDBArray::~TileDBArray()                        */
/************************************************************************/

TileDBArray::~TileDBArray()
{
    if (!m_bFinalized)
        Finalize();
}

/************************************************************************/
/*                   BuildDimensionLabelName()                          */
/************************************************************************/

static std::string
BuildDimensionLabelName(const std::shared_ptr<GDALDimension> &poDim)
{
    return poDim->GetName() + "_label";
}

/************************************************************************/
/*                   TileDBDataTypeToGDALDataType()                     */
/************************************************************************/

/*static*/ GDALDataType
TileDBArray::TileDBDataTypeToGDALDataType(tiledb_datatype_t tiledb_dt)
{
    GDALDataType eDT = GDT_Unknown;
    switch (tiledb_dt)
    {
        case TILEDB_UINT8:
            eDT = GDT_Byte;
            break;

        case TILEDB_INT8:
            eDT = GDT_Int8;
            break;

        case TILEDB_UINT16:
            eDT = GDT_UInt16;
            break;

        case TILEDB_INT16:
            eDT = GDT_Int16;
            break;

        case TILEDB_UINT32:
            eDT = GDT_UInt32;
            break;

        case TILEDB_INT32:
            eDT = GDT_Int32;
            break;

        case TILEDB_UINT64:
            eDT = GDT_UInt64;
            break;

        case TILEDB_INT64:
            eDT = GDT_Int64;
            break;

        case TILEDB_FLOAT32:
            eDT = GDT_Float32;
            break;

        case TILEDB_FLOAT64:
            eDT = GDT_Float64;
            break;

        case TILEDB_CHAR:
        case TILEDB_STRING_ASCII:
        case TILEDB_STRING_UTF8:
        case TILEDB_STRING_UTF16:
        case TILEDB_STRING_UTF32:
        case TILEDB_STRING_UCS2:
        case TILEDB_STRING_UCS4:
        case TILEDB_ANY:
        case TILEDB_DATETIME_YEAR:
        case TILEDB_DATETIME_MONTH:
        case TILEDB_DATETIME_WEEK:
        case TILEDB_DATETIME_DAY:
        case TILEDB_DATETIME_HR:
        case TILEDB_DATETIME_MIN:
        case TILEDB_DATETIME_SEC:
        case TILEDB_DATETIME_MS:
        case TILEDB_DATETIME_US:
        case TILEDB_DATETIME_NS:
        case TILEDB_DATETIME_PS:
        case TILEDB_DATETIME_FS:
        case TILEDB_DATETIME_AS:
        case TILEDB_TIME_HR:
        case TILEDB_TIME_MIN:
        case TILEDB_TIME_SEC:
        case TILEDB_TIME_MS:
        case TILEDB_TIME_US:
        case TILEDB_TIME_NS:
        case TILEDB_TIME_PS:
        case TILEDB_TIME_FS:
        case TILEDB_TIME_AS:
        case TILEDB_BLOB:
        case TILEDB_BOOL:
#ifdef HAS_TILEDB_GEOM_WKB_WKT
        case TILEDB_GEOM_WKB:
        case TILEDB_GEOM_WKT:
#endif
        {
            break;
        }
    }
    return eDT;
}

/************************************************************************/
/*                   TileDBArray::Finalize()                            */
/************************************************************************/

bool TileDBArray::Finalize() const
{
    if (m_bFinalized)
        return m_poTileDBArray != nullptr;

    m_bFinalized = true;

    CPLAssert(m_poSchema);
    CPLAssert(m_poAttr);

    try
    {
        // TODO: set nodata as fill_value

        m_poSchema->add_attribute(*(m_poAttr.get()));

        tiledb::Array::create(m_osPath, *m_poSchema);

        bool bAdded = false;
        auto poGroup = m_poParent.lock();
        if (!poGroup)
        {
            // Temporarily instantiate a TileDBGroup to call AddMember() on it
            poGroup = TileDBGroup::OpenFromDisk(
                m_poSharedResource,
                /* osParentName = */ std::string(),
                CPLGetFilename(m_osParentPath.c_str()), m_osParentPath);
        }
        if (poGroup)
        {
            bAdded = poGroup->AddMember(m_osPath, m_osName);
        }
        if (!bAdded)
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Could not add array %s as a member of group %s",
                     m_osName.c_str(), m_osParentPath.c_str());
        }

        auto &ctx = m_poSharedResource->GetCtx();
        m_poTileDBArray =
            std::make_unique<tiledb::Array>(ctx, m_osPath, TILEDB_READ);
        if (m_nTimestamp > 0)
            m_poTileDBArray->set_open_timestamp_end(m_nTimestamp);
        m_poSchema =
            std::make_unique<tiledb::ArraySchema>(m_poTileDBArray->schema());
        m_poAttr.reset();

        // Write dimension label values
        for (const auto &poDim : m_aoDims)
        {
            auto poVar = poDim->GetIndexingVariable();
            if (poVar)
            {
                const std::string osLabelName(BuildDimensionLabelName(poDim));
                if (tiledb::ArraySchemaExperimental::has_dimension_label(
                        ctx, *(m_poSchema.get()), osLabelName))
                {
                    auto label =
                        tiledb::ArraySchemaExperimental::dimension_label(
                            ctx, *(m_poSchema.get()), osLabelName);
                    tiledb::Array labelArray(ctx, label.uri(), TILEDB_WRITE);
                    auto label_attr = labelArray.schema().attribute(0);
                    const auto eDT =
                        TileDBDataTypeToGDALDataType(label_attr.type());
                    if (eDT != GDT_Unknown)
                    {
                        std::vector<GByte> abyVals;
                        abyVals.resize(static_cast<size_t>(
                            poVar->GetDimensions()[0]->GetSize() *
                            GDALGetDataTypeSizeBytes(eDT)));
                        GUInt64 anStart[1] = {0};
                        size_t anCount[1] = {static_cast<size_t>(
                            poVar->GetDimensions()[0]->GetSize())};
                        if (poVar->Read(anStart, anCount, nullptr, nullptr,
                                        GDALExtendedDataType::Create(eDT),
                                        abyVals.data()))
                        {
                            tiledb::Query query(ctx, labelArray);
                            query.set_data_buffer(
                                label_attr.name(),
                                static_cast<void *>(abyVals.data()),
                                anCount[0]);
                            if (query.submit() !=
                                tiledb::Query::Status::COMPLETE)
                            {
                                CPLError(CE_Failure, CPLE_AppDefined,
                                         "Could not write values for dimension "
                                         "label %s",
                                         osLabelName.c_str());
                            }

                            if (!poDim->GetType().empty())
                            {
                                labelArray.put_metadata(
                                    DIM_TYPE_ATTRIBUTE_NAME, TILEDB_STRING_UTF8,
                                    static_cast<uint32_t>(
                                        poDim->GetType().size()),
                                    poDim->GetType().c_str());
                            }

                            if (!poDim->GetDirection().empty())
                            {
                                labelArray.put_metadata(
                                    DIM_DIRECTION_ATTRIBUTE_NAME,
                                    TILEDB_STRING_UTF8,
                                    static_cast<uint32_t>(
                                        poDim->GetDirection().size()),
                                    poDim->GetDirection().c_str());
                            }
                        }
                    }
                }
            }
        }
    }
    catch (const std::exception &e)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Array %s creation failed with: %s", m_osName.c_str(),
                 e.what());
        return false;
    }

    return true;
}

/************************************************************************/
/*                   TileDBArray::OpenFromDisk()                        */
/************************************************************************/

/* static */
std::shared_ptr<TileDBArray> TileDBArray::OpenFromDisk(
    const std::shared_ptr<TileDBSharedResource> &poSharedResource,
    const std::shared_ptr<GDALGroup> &poParent, const std::string &osParentName,
    const std::string &osName, const std::string &osAttributeName,
    const std::string &osPath, CSLConstList papszOptions)
{
    try
    {
        auto &ctx = poSharedResource->GetCtx();
        uint64_t nTimestamp = poSharedResource->GetTimestamp();
        const char *pszTimestamp =
            CSLFetchNameValue(papszOptions, "TILEDB_TIMESTAMP");
        if (pszTimestamp)
            nTimestamp = std::strtoull(pszTimestamp, nullptr, 10);

        auto poTileDBArray =
            std::make_unique<tiledb::Array>(ctx, osPath, TILEDB_READ);
        if (nTimestamp > 0)
            poTileDBArray->set_open_timestamp_end(nTimestamp);

        auto schema = poTileDBArray->schema();

        if (schema.attribute_num() != 1 && osAttributeName.empty())
        {
            CPLError(CE_Failure, CPLE_NotSupported,
                     "Array %s has %u attributes. "
                     "osAttributeName must be specified",
                     osName.c_str(), schema.attribute_num());
            return nullptr;
        }

        const auto &attr = osAttributeName.empty()
                               ? schema.attribute(0)
                               : schema.attribute(osAttributeName);
        GDALDataType eDT = TileDBDataTypeToGDALDataType(attr.type());
        if (attr.type() == TILEDB_CHAR)
            eDT = GDT_Byte;
        if (eDT == GDT_Unknown)
        {
            const char *pszTypeName = "";
            tiledb_datatype_to_str(attr.type(), &pszTypeName);
            CPLError(CE_Failure, CPLE_NotSupported,
                     "Array %s has type %s, which is unsupported",
                     osName.c_str(), pszTypeName);
            return nullptr;
        }

        if (attr.variable_sized())
        {
            CPLError(CE_Failure, CPLE_NotSupported,
                     "Variable sized attribute not supported");
            return nullptr;
        }
        if (attr.cell_val_num() == 2)
        {
            if (attr.type() == TILEDB_INT16)
                eDT = GDT_CInt16;
            else if (attr.type() == TILEDB_INT32)
                eDT = GDT_CInt32;
            else if (attr.type() == TILEDB_FLOAT32)
                eDT = GDT_CFloat32;
            else if (attr.type() == TILEDB_FLOAT64)
                eDT = GDT_CFloat64;
            else
            {
                const char *pszTypeName = "";
                tiledb_datatype_to_str(attr.type(), &pszTypeName);
                CPLError(CE_Failure, CPLE_NotSupported,
                         "Attribute with number of values per cell = %u not "
                         "supported for type %s",
                         attr.cell_val_num(), pszTypeName);
                return nullptr;
            }
        }
        else if (attr.cell_val_num() != 1)
        {
            CPLError(
                CE_Failure, CPLE_NotSupported,
                "Attribute with number of values per cell = %u not supported",
                attr.cell_val_num());
            return nullptr;
        }

        // Compatibility with the 2D raster side: extract X_SIZE, Y_SIZE, SRS
        // and geotransform
        int nXSize = 0;
        int nYSize = 0;
        std::shared_ptr<OGRSpatialReference> poSRS;
        double adfGeoTransform[6] = {0};
        bool bHasGeoTransform = false;
        {
            tiledb_datatype_t value_type = TILEDB_ANY;
            uint32_t value_num = 0;
            const void *value = nullptr;
            poTileDBArray->get_metadata(GDAL_ATTRIBUTE_NAME, &value_type,
                                        &value_num, &value);
            if (value && value_num && value_type == TILEDB_UINT8 &&
                CPLIsUTF8(static_cast<const char *>(value), value_num))
            {
                std::string osXML;
                osXML.assign(static_cast<const char *>(value), value_num);
                CPLXMLNode *psRoot = CPLParseXMLString(osXML.c_str());
                if (psRoot)
                {
                    const CPLXMLNode *psDataset =
                        CPLGetXMLNode(psRoot, "=PAMDataset");
                    if (psDataset)
                    {
                        for (const CPLXMLNode *psIter = psDataset->psChild;
                             psIter; psIter = psIter->psNext)
                        {
                            if (psIter->eType == CXT_Element &&
                                strcmp(psIter->pszValue, "Metadata") == 0 &&
                                strcmp(CPLGetXMLValue(psIter, "domain", ""),
                                       "IMAGE_STRUCTURE") == 0)
                            {
                                for (const CPLXMLNode *psIter2 =
                                         psIter->psChild;
                                     psIter2; psIter2 = psIter2->psNext)
                                {
                                    if (psIter2->eType == CXT_Element &&
                                        strcmp(psIter2->pszValue, "MDI") == 0 &&
                                        strcmp(
                                            CPLGetXMLValue(psIter2, "key", ""),
                                            "X_SIZE") == 0)
                                    {
                                        nXSize = atoi(CPLGetXMLValue(
                                            psIter2, nullptr, "0"));
                                    }
                                    else if (psIter2->eType == CXT_Element &&
                                             strcmp(psIter2->pszValue, "MDI") ==
                                                 0 &&
                                             strcmp(CPLGetXMLValue(psIter2,
                                                                   "key", ""),
                                                    "Y_SIZE") == 0)
                                    {
                                        nYSize = atoi(CPLGetXMLValue(
                                            psIter2, nullptr, "0"));
                                    }
                                }
                            }
                        }

                        const char *pszSRS =
                            CPLGetXMLValue(psDataset, "SRS", nullptr);
                        if (pszSRS)
                        {
                            poSRS = std::make_shared<OGRSpatialReference>();
                            poSRS->SetAxisMappingStrategy(
                                OAMS_TRADITIONAL_GIS_ORDER);
                            if (poSRS->importFromWkt(pszSRS) != OGRERR_NONE)
                            {
                                poSRS.reset();
                            }
                        }

                        const char *pszGeoTransform =
                            CPLGetXMLValue(psDataset, "GeoTransform", nullptr);
                        if (pszGeoTransform)
                        {
                            const CPLStringList aosTokens(
                                CSLTokenizeString2(pszGeoTransform, ", ", 0));
                            if (aosTokens.size() == 6)
                            {
                                bHasGeoTransform = true;
                                for (int i = 0; i < 6; ++i)
                                    adfGeoTransform[i] = CPLAtof(aosTokens[i]);
                            }
                        }
                    }

                    CPLDestroyXMLNode(psRoot);
                }
            }
        }

        // Read CRS from _CRS attribute otherwise
        if (!poSRS)
        {
            tiledb_datatype_t value_type = TILEDB_ANY;
            uint32_t value_num = 0;
            const void *value = nullptr;
            poTileDBArray->get_metadata(CRS_ATTRIBUTE_NAME, &value_type,
                                        &value_num, &value);
            if (value && value_num &&
                (value_type == TILEDB_STRING_ASCII ||
                 value_type == TILEDB_STRING_UTF8))
            {
                std::string osStr;
                osStr.assign(static_cast<const char *>(value), value_num);
                poSRS = std::make_shared<OGRSpatialReference>();
                poSRS->SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);
                if (poSRS->SetFromUserInput(
                        osStr.c_str(),
                        OGRSpatialReference::
                            SET_FROM_USER_INPUT_LIMITATIONS_get()) !=
                    OGRERR_NONE)
                {
                    poSRS.reset();
                }
            }
        }

        // Read unit
        std::string osUnit;
        {
            tiledb_datatype_t value_type = TILEDB_ANY;
            uint32_t value_num = 0;
            const void *value = nullptr;
            poTileDBArray->get_metadata(UNIT_ATTRIBUTE_NAME, &value_type,
                                        &value_num, &value);
            if (value && value_num &&
                (value_type == TILEDB_STRING_ASCII ||
                 value_type == TILEDB_STRING_UTF8))
            {
                osUnit.assign(static_cast<const char *>(value), value_num);
            }
        }

        // Read dimensions
        std::vector<std::shared_ptr<GDALDimension>> aoDims;
        const auto dims = schema.domain().dimensions();
        std::vector<GUInt64> anBlockSize;
        std::vector<uint64_t> anStartDimOffset;
        const std::string osArrayFullName(
            (osParentName == "/" ? std::string() : osParentName) + "/" +
            osName);
        for (size_t i = 0; i < dims.size(); ++i)
        {
            const auto &dim = dims[i];
            if (dim.type() != TILEDB_UINT64)
            {
                const char *pszTypeName = "";
                tiledb_datatype_to_str(dim.type(), &pszTypeName);
                CPLError(CE_Failure, CPLE_NotSupported,
                         "Dimension %s of array %s has type %s, which is "
                         "unsupported. Only UInt64 is supported",
                         dim.name().c_str(), osName.c_str(), pszTypeName);
                return nullptr;
            }
            const auto domain = dim.domain<uint64_t>();
            anStartDimOffset.push_back(domain.first);
            const uint64_t nSize = (i + 2 == dims.size() && nYSize > 0) ? nYSize
                                   : (i + 1 == dims.size() && nXSize > 0)
                                       ? nXSize
                                       : domain.second - domain.first + 1;
            std::string osType;
            std::string osDirection;
            auto poDim = std::make_shared<TileDBDimension>(
                osArrayFullName, dim.name(), osType, osDirection, nSize);

            const std::string osLabelName(BuildDimensionLabelName(poDim));
            if (tiledb::ArraySchemaExperimental::has_dimension_label(
                    ctx, schema, osLabelName))
            {
                auto label = tiledb::ArraySchemaExperimental::dimension_label(
                    ctx, schema, osLabelName);
                auto poIndexingVar = OpenFromDisk(
                    poSharedResource, nullptr, osArrayFullName,
                    poDim->GetName(),
                    /* osAttributeName = */ std::string(), label.uri(),
                    /* papszOptions= */ nullptr);
                if (poIndexingVar)
                {
                    auto poAttr =
                        poIndexingVar->GetAttribute(DIM_TYPE_ATTRIBUTE_NAME);
                    if (poAttr &&
                        poAttr->GetDataType().GetClass() == GEDTC_STRING)
                    {
                        const char *pszVal = poAttr->ReadAsString();
                        if (pszVal)
                            osType = pszVal;
                    }

                    poAttr = poIndexingVar->GetAttribute(
                        DIM_DIRECTION_ATTRIBUTE_NAME);
                    if (poAttr &&
                        poAttr->GetDataType().GetClass() == GEDTC_STRING)
                    {
                        const char *pszVal = poAttr->ReadAsString();
                        if (pszVal)
                            osDirection = pszVal;
                    }

                    if (!osType.empty() || !osDirection.empty())
                    {
                        // Recreate dimension with type and/or direction info
                        poDim = std::make_shared<TileDBDimension>(
                            osArrayFullName, dim.name(), osType, osDirection,
                            nSize);
                    }

                    poDim->SetIndexingVariableOneTime(poIndexingVar);
                }
            }
            if (bHasGeoTransform && !poDim->GetIndexingVariable() &&
                i + 2 >= dims.size() && adfGeoTransform[2] == 0 &&
                adfGeoTransform[4] == 0)
            {
                // Recreate dimension with type and/or direction info
                if (i + 2 == dims.size())
                {
                    osType = GDAL_DIM_TYPE_HORIZONTAL_Y;
                    osDirection = "NORTH";
                }
                else /* if( i + 1 == dims.size()) */
                {
                    osType = GDAL_DIM_TYPE_HORIZONTAL_X;
                    osDirection = "EAST";
                }
                poDim = std::make_shared<TileDBDimension>(
                    osArrayFullName, dim.name(), osType, osDirection, nSize);
                // Do not create indexing variable with poDim, otherwise
                // both dimension and indexing variable will have a shared_ptr
                // to each other, causing memory leak
                auto poDimTmp = std::make_shared<GDALDimension>(
                    std::string(), dim.name(), /* osType = */ std::string(),
                    /* osDirection = */ std::string(), nSize);
                const double dfStart =
                    (i + 2 == dims.size())
                        ? adfGeoTransform[3] + adfGeoTransform[5] / 2
                        : adfGeoTransform[0] + adfGeoTransform[1] / 2;
                const double dfStep = (i + 2 == dims.size())
                                          ? adfGeoTransform[5]
                                          : adfGeoTransform[1];
                poDim->SetIndexingVariableOneTime(
                    GDALMDArrayRegularlySpaced::Create(
                        osArrayFullName, poDim->GetName(), poDimTmp, dfStart,
                        dfStep, 0));
            }

            if (poParent && dims.size() >= 2)
            {
                for (const auto &osOtherArray : poParent->GetMDArrayNames())
                {
                    if (osOtherArray != osName)
                    {
                        auto poOtherArray = poParent->OpenMDArray(osOtherArray);
                        if (poOtherArray &&
                            poOtherArray->GetDimensionCount() == 1 &&
                            poOtherArray->GetDataType().GetClass() ==
                                GEDTC_NUMERIC &&
                            poOtherArray->GetAttribute(
                                std::string("__tiledb_attr.")
                                    .append(poDim->GetName())
                                    .append(".data.standard_name")))
                        {
                            if (dim.name() == "x")
                            {
                                osType = GDAL_DIM_TYPE_HORIZONTAL_X;
                                osDirection = "EAST";
                            }
                            else if (dim.name() == "y")
                            {
                                osType = GDAL_DIM_TYPE_HORIZONTAL_Y;
                                osDirection = "NORTH";
                            }
                            if (!osType.empty())
                            {
                                poDim = std::make_shared<TileDBDimension>(
                                    osArrayFullName, dim.name(), osType,
                                    osDirection, nSize);
                            }
                            poDim->SetIndexingVariableOneTime(poOtherArray);
                            break;
                        }
                    }
                }
            }

            aoDims.emplace_back(std::move(poDim));
            anBlockSize.push_back(dim.tile_extent<uint64_t>());
        }

        GDALExtendedDataType oType = GDALExtendedDataType::Create(eDT);
        auto poArray = Create(poSharedResource, osParentName, osName, aoDims,
                              oType, osPath);
        poArray->m_poTileDBArray = std::move(poTileDBArray);
        poArray->m_poSchema = std::make_unique<tiledb::ArraySchema>(
            poArray->m_poTileDBArray->schema());
        poArray->m_anBlockSize = std::move(anBlockSize);
        poArray->m_anStartDimOffset = std::move(anStartDimOffset);
        poArray->m_osAttrName = attr.name();
        poArray->m_osUnit = std::move(osUnit);
        poArray->m_nTimestamp = nTimestamp;

        // Try to get SRS from CF-1 conventions, if dataset has been generated
        // with https://github.com/TileDB-Inc/TileDB-CF-Py
        if (poParent && !poSRS)
        {
            const auto ENDS_WITH_CI =
                [](const char *pszStr, const char *pszNeedle)
            {
                const size_t nLenStr = strlen(pszStr);
                const size_t nLenNeedle = strlen(pszNeedle);
                return nLenStr >= nLenNeedle &&
                       memcmp(pszStr + (nLenStr - nLenNeedle), pszNeedle,
                              nLenNeedle) == 0;
            };

            const auto GetSRSFromGridMappingArray =
                [](const std::shared_ptr<GDALMDArray> &poOtherArray,
                   const std::string &osGMPrefix)
            {
                CPLStringList aosGridMappingKeyValues;
                for (const auto &poGMAttr : poOtherArray->GetAttributes())
                {
                    if (STARTS_WITH(poGMAttr->GetName().c_str(),
                                    osGMPrefix.c_str()))
                    {
                        const std::string osKey =
                            poGMAttr->GetName().c_str() + osGMPrefix.size();
                        if (poGMAttr->GetDataType().GetClass() == GEDTC_STRING)
                        {
                            const char *pszValue = poGMAttr->ReadAsString();
                            if (pszValue)
                                aosGridMappingKeyValues.AddNameValue(
                                    osKey.c_str(), pszValue);
                        }
                        else if (poGMAttr->GetDataType().GetClass() ==
                                 GEDTC_NUMERIC)
                        {
                            const auto aosValues =
                                poGMAttr->ReadAsDoubleArray();
                            std::string osVal;
                            for (double dfVal : aosValues)
                            {
                                if (!osVal.empty())
                                    osVal += ',';
                                osVal += CPLSPrintf("%.18g", dfVal);
                            }
                            aosGridMappingKeyValues.AddNameValue(osKey.c_str(),
                                                                 osVal.c_str());
                        }
                    }
                }
                auto l_poSRS = std::make_shared<OGRSpatialReference>();
                l_poSRS->SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);
                if (l_poSRS->importFromCF1(aosGridMappingKeyValues.List(),
                                           nullptr) != OGRERR_NONE)
                {
                    l_poSRS.reset();
                }
                return l_poSRS;
            };

            const auto poAttributes = poArray->GetAttributes();
            for (const auto &poAttr : poAttributes)
            {
                if (poAttr->GetDataType().GetClass() == GEDTC_STRING &&
                    STARTS_WITH_CI(poAttr->GetName().c_str(),
                                   "__tiledb_attr.") &&
                    ENDS_WITH_CI(poAttr->GetName().c_str(), ".grid_mapping"))
                {
                    const char *pszGridMapping = poAttr->ReadAsString();
                    if (pszGridMapping)
                    {
                        for (const auto &osOtherArray :
                             poParent->GetMDArrayNames())
                        {
                            if (osOtherArray != osName)
                            {
                                auto poOtherArray =
                                    poParent->OpenMDArray(osOtherArray);
                                if (poOtherArray)
                                {
                                    const std::string osGMPrefix =
                                        std::string("__tiledb_attr.")
                                            .append(pszGridMapping)
                                            .append(".");
                                    auto poGridMappingNameAttr =
                                        poOtherArray->GetAttribute(
                                            std::string(osGMPrefix)
                                                .append("grid_mapping_name")
                                                .c_str());
                                    if (poGridMappingNameAttr)
                                    {
                                        poSRS = GetSRSFromGridMappingArray(
                                            poOtherArray, osGMPrefix);
                                        break;
                                    }
                                }
                            }
                        }
                    }
                    break;
                }
            }
        }

        // Set SRS DataAxisToSRSAxisMapping
        if (poSRS)
        {
            int iDimX = 0;
            int iDimY = 0;
            int iCount = 1;
            for (const auto &poDim : aoDims)
            {
                if (poDim->GetType() == GDAL_DIM_TYPE_HORIZONTAL_X)
                    iDimX = iCount;
                else if (poDim->GetType() == GDAL_DIM_TYPE_HORIZONTAL_Y)
                    iDimY = iCount;
                iCount++;
            }
            if ((iDimX == 0 || iDimY == 0) && aoDims.size() >= 2)
            {
                iDimX = static_cast<int>(aoDims.size());
                iDimY = iDimX - 1;
            }
            if (iDimX > 0 && iDimY > 0)
            {
                if (poSRS->GetDataAxisToSRSAxisMapping() ==
                    std::vector<int>{2, 1})
                    poSRS->SetDataAxisToSRSAxisMapping({iDimY, iDimX});
                else if (poSRS->GetDataAxisToSRSAxisMapping() ==
                         std::vector<int>{1, 2})
                    poSRS->SetDataAxisToSRSAxisMapping({iDimX, iDimY});
            }
        }

        poArray->m_poSRS = std::move(poSRS);

        const auto filters = attr.filter_list();
        std::string osFilters;
        for (uint32_t j = 0; j < filters.nfilters(); ++j)
        {
            const auto filter = filters.filter(j);
            if (j > 0)
                osFilters += ',';
            osFilters += tiledb::Filter::to_str(filter.filter_type());
        }
        if (!osFilters.empty())
        {
            poArray->m_aosStructuralInfo.SetNameValue("FILTER_LIST",
                                                      osFilters.c_str());
        }

        return poArray;
    }
    catch (const std::exception &e)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "OpenFromDisk() failed with: %s",
                 e.what());
        return nullptr;
    }
}

/************************************************************************/
/*                   TileDBArray::EnsureOpenAs()                        */
/************************************************************************/

bool TileDBArray::EnsureOpenAs(tiledb_query_type_t mode) const
{
    if (!m_bFinalized && !Finalize())
        return false;
    if (!m_poTileDBArray)
        return false;
    if (m_poTileDBArray->query_type() == mode && m_poTileDBArray->is_open())
        return true;
    try
    {
        m_poTileDBArray->close();
        m_poTileDBArray->open(mode);
    }
    catch (const std::exception &e)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "%s", e.what());
        m_poTileDBArray.reset();
        return false;
    }
    return true;
}

/************************************************************************/
/*                          TileDBArray::IRead()                        */
/************************************************************************/

bool TileDBArray::IRead(const GUInt64 *arrayStartIdx, const size_t *count,
                        const GInt64 *arrayStep, const GPtrDiff_t *bufferStride,
                        const GDALExtendedDataType &bufferDataType,
                        void *pDstBuffer) const
{
    if (!EnsureOpenAs(TILEDB_READ))
        return false;

    if (!IsStepOneContiguousRowMajorOrderedSameDataType(
            count, arrayStep, bufferStride, bufferDataType))
    {
        return ReadUsingContiguousIRead(arrayStartIdx, count, arrayStep,
                                        bufferStride, bufferDataType,
                                        pDstBuffer);
    }
    else
    {
        std::vector<uint64_t> anSubArray;
        const auto nDims = m_aoDims.size();
        anSubArray.reserve(2 * nDims);
        size_t nBufferSize =
            GDALDataTypeIsComplex(m_oType.GetNumericDataType()) ? 2 : 1;
        for (size_t i = 0; i < nDims; ++i)
        {
            anSubArray.push_back(m_anStartDimOffset[i] + arrayStartIdx[i]);
            anSubArray.push_back(m_anStartDimOffset[i] + arrayStartIdx[i] +
                                 count[i] - 1);
            nBufferSize *= count[i];
        }
        try
        {
            tiledb::Query query(m_poSharedResource->GetCtx(),
                                *(m_poTileDBArray.get()));
            tiledb::Subarray subarray(m_poSharedResource->GetCtx(),
                                      *(m_poTileDBArray.get()));
            subarray.set_subarray(anSubArray);
            query.set_subarray(subarray);
            query.set_data_buffer(m_osAttrName, pDstBuffer, nBufferSize);

            if (m_bStats)
                tiledb::Stats::enable();

            const auto ret = query.submit();

            if (m_bStats)
            {
                tiledb::Stats::dump(stdout);
                tiledb::Stats::disable();
            }

            return ret == tiledb::Query::Status::COMPLETE;
        }
        catch (const std::exception &e)
        {
            CPLError(CE_Failure, CPLE_AppDefined, "Read() failed with %s",
                     e.what());
        }
    }

    return false;
}

/************************************************************************/
/*                          TileDBArray::IWrite()                       */
/************************************************************************/

bool TileDBArray::IWrite(const GUInt64 *arrayStartIdx, const size_t *count,
                         const GInt64 *arrayStep,
                         const GPtrDiff_t *bufferStride,
                         const GDALExtendedDataType &bufferDataType,
                         const void *pSrcBuffer)
{
    if (!IsWritable())
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "Dataset not open in update mode");
        return false;
    }

    if (!EnsureOpenAs(TILEDB_WRITE))
        return false;

    if (!IsStepOneContiguousRowMajorOrderedSameDataType(
            count, arrayStep, bufferStride, bufferDataType))
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "Write parameters not supported");
        return false;
    }
    else
    {
        std::vector<uint64_t> anSubArray;
        const auto nDims = m_aoDims.size();
        anSubArray.reserve(2 * nDims);
        size_t nBufferSize =
            GDALDataTypeIsComplex(m_oType.GetNumericDataType()) ? 2 : 1;
        for (size_t i = 0; i < nDims; ++i)
        {
            anSubArray.push_back(m_anStartDimOffset[i] + arrayStartIdx[i]);
            anSubArray.push_back(m_anStartDimOffset[i] + arrayStartIdx[i] +
                                 count[i] - 1);
            nBufferSize *= count[i];
        }
        try
        {
            tiledb::Query query(m_poSharedResource->GetCtx(),
                                *(m_poTileDBArray.get()));
            tiledb::Subarray subarray(m_poSharedResource->GetCtx(),
                                      *(m_poTileDBArray.get()));
            subarray.set_subarray(anSubArray);
            query.set_subarray(subarray);
            query.set_data_buffer(m_osAttrName, const_cast<void *>(pSrcBuffer),
                                  nBufferSize);

            return query.submit() == tiledb::Query::Status::COMPLETE;
        }
        catch (const std::exception &e)
        {
            CPLError(CE_Failure, CPLE_AppDefined, "Write() failed with %s",
                     e.what());
        }
    }

    return false;
}

/************************************************************************/
/*                  TileDBArray::GetRawNoDataValue()                    */
/************************************************************************/

const void *TileDBArray::GetRawNoDataValue() const
{
    if (!m_bFinalized)
        return nullptr;

    if (m_abyNoData.empty())
    {
        const void *value = nullptr;
        uint64_t size = 0;
        // Caution: 2 below statements must not be combined in a single one,
        // as the lifetime of value is linked to the return value of
        // attribute()
        auto attr = m_poSchema->attribute(m_osAttrName);
        attr.get_fill_value(&value, &size);
        if (size == m_oType.GetSize())
        {
            m_abyNoData.resize(size);
            memcpy(m_abyNoData.data(), value, size);
        }
    }

    return m_abyNoData.empty() ? nullptr : m_abyNoData.data();
}

/************************************************************************/
/*                  TileDBArray::SetRawNoDataValue()                    */
/************************************************************************/

bool TileDBArray::SetRawNoDataValue(const void *pRawNoData)
{
    if (m_bFinalized)
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "SetRawNoDataValue() not supported after array has been "
                 "finalized.");
        return false;
    }

    if (pRawNoData)
    {
        CPLAssert(m_poAttr);
        m_poAttr->set_fill_value(pRawNoData, m_oType.GetSize());
        m_abyNoData.resize(m_oType.GetSize());
        memcpy(m_abyNoData.data(), pRawNoData, m_oType.GetSize());
    }

    Finalize();

    return true;
}

/************************************************************************/
/*                  TileDBArray::CreateAttribute()                      */
/************************************************************************/

std::shared_ptr<GDALAttribute> TileDBArray::CreateAttribute(
    const std::string &osName, const std::vector<GUInt64> &anDimensions,
    const GDALExtendedDataType &oDataType, CSLConstList papszOptions)
{
    return CreateAttributeImpl(osName, anDimensions, oDataType, papszOptions);
}

/************************************************************************/
/*                   TileDBArray::GetAttribute()                        */
/************************************************************************/

std::shared_ptr<GDALAttribute>
TileDBArray::GetAttribute(const std::string &osName) const
{
    return GetAttributeImpl(osName);
}

/************************************************************************/
/*                   TileDBArray::GetAttributes()                       */
/************************************************************************/

std::vector<std::shared_ptr<GDALAttribute>>
TileDBArray::GetAttributes(CSLConstList papszOptions) const
{
    return GetAttributesImpl(papszOptions);
}

/************************************************************************/
/*                   TileDBArray::DeleteAttribute()                     */
/************************************************************************/

bool TileDBArray::DeleteAttribute(const std::string &osName,
                                  CSLConstList papszOptions)
{
    return DeleteAttributeImpl(osName, papszOptions);
}

/************************************************************************/
/*                   TileDBArray::SetSpatialRef()                       */
/************************************************************************/

bool TileDBArray::SetSpatialRef(const OGRSpatialReference *poSRS)
{
    if (!IsWritable())
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "Dataset not open in update mode");
        return false;
    }

    if (!EnsureOpenAs(TILEDB_WRITE))
        return false;

    try
    {
        if (m_poSRS && !poSRS)
            m_poTileDBArray->delete_metadata(CRS_ATTRIBUTE_NAME);

        m_poSRS.reset();
        if (poSRS)
        {
            m_poSRS.reset(poSRS->Clone());

            char *pszPROJJSON = nullptr;
            if (m_poSRS->exportToPROJJSON(&pszPROJJSON, nullptr) ==
                    OGRERR_NONE &&
                pszPROJJSON != nullptr)
            {
                m_poTileDBArray->put_metadata(
                    CRS_ATTRIBUTE_NAME, TILEDB_STRING_UTF8,
                    static_cast<uint32_t>(strlen(pszPROJJSON)), pszPROJJSON);
                CPLFree(pszPROJJSON);
            }
            else
            {
                CPLFree(pszPROJJSON);
                return false;
            }
        }
        return true;
    }
    catch (const std::exception &e)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "SetSpatialRef() failed with: %s",
                 e.what());
        return false;
    }
}

/************************************************************************/
/*                       TileDBArray::SetUnit()                         */
/************************************************************************/

bool TileDBArray::SetUnit(const std::string &osUnit)
{
    if (!IsWritable())
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "Dataset not open in update mode");
        return false;
    }

    if (!EnsureOpenAs(TILEDB_WRITE))
        return false;

    try
    {
        if (!m_osUnit.empty() && osUnit.empty())
            m_poTileDBArray->delete_metadata(UNIT_ATTRIBUTE_NAME);

        m_osUnit = osUnit;
        if (!osUnit.empty())
        {
            m_poTileDBArray->put_metadata(
                UNIT_ATTRIBUTE_NAME, TILEDB_STRING_UTF8,
                static_cast<uint32_t>(osUnit.size()), osUnit.data());
        }
        return true;
    }
    catch (const std::exception &e)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "SetUnit() failed with: %s",
                 e.what());
        return false;
    }
}

/************************************************************************/
/*                          FillBlockSize()                             */
/************************************************************************/

static bool
FillBlockSize(const std::vector<std::shared_ptr<GDALDimension>> &aoDimensions,
              const GDALExtendedDataType &oDataType,
              std::vector<GUInt64> &anBlockSize, CSLConstList papszOptions)
{
    const auto nDims = aoDimensions.size();
    anBlockSize.resize(nDims);
    for (size_t i = 0; i < nDims; ++i)
        anBlockSize[i] = 1;
    if (nDims >= 2)
    {
        anBlockSize[nDims - 2] =
            std::min(std::max<GUInt64>(1, aoDimensions[nDims - 2]->GetSize()),
                     static_cast<GUInt64>(256));
        anBlockSize[nDims - 1] =
            std::min(std::max<GUInt64>(1, aoDimensions[nDims - 1]->GetSize()),
                     static_cast<GUInt64>(256));
    }
    else if (nDims == 1)
    {
        anBlockSize[0] = std::max<GUInt64>(1, aoDimensions[0]->GetSize());
    }

    const char *pszBlockSize = CSLFetchNameValue(papszOptions, "BLOCKSIZE");
    if (pszBlockSize)
    {
        const auto aszTokens(
            CPLStringList(CSLTokenizeString2(pszBlockSize, ",", 0)));
        if (static_cast<size_t>(aszTokens.size()) != nDims)
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Invalid number of values in BLOCKSIZE");
            return false;
        }
        size_t nBlockSize = oDataType.GetSize();
        for (size_t i = 0; i < nDims; ++i)
        {
            anBlockSize[i] = static_cast<GUInt64>(CPLAtoGIntBig(aszTokens[i]));
            if (anBlockSize[i] == 0)
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "Values in BLOCKSIZE should be > 0");
                return false;
            }
            if (anBlockSize[i] >
                std::numeric_limits<size_t>::max() / nBlockSize)
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "Too large values in BLOCKSIZE");
                return false;
            }
            nBlockSize *= static_cast<size_t>(anBlockSize[i]);
        }
    }
    return true;
}

/************************************************************************/
/*                 TileDBArray::GDALDataTypeToTileDB()                  */
/************************************************************************/

/*static*/ bool TileDBArray::GDALDataTypeToTileDB(GDALDataType dt,
                                                  tiledb_datatype_t &tiledb_dt)
{
    switch (dt)
    {
        case GDT_Byte:
            tiledb_dt = TILEDB_UINT8;
            break;
        case GDT_Int8:
            tiledb_dt = TILEDB_INT8;
            break;
        case GDT_UInt16:
            tiledb_dt = TILEDB_UINT16;
            break;
        case GDT_CInt16:
        case GDT_Int16:
            tiledb_dt = TILEDB_INT16;
            break;
        case GDT_UInt32:
            tiledb_dt = TILEDB_UINT32;
            break;
        case GDT_CInt32:
        case GDT_Int32:
            tiledb_dt = TILEDB_INT32;
            break;
        case GDT_UInt64:
            tiledb_dt = TILEDB_UINT64;
            break;
        case GDT_Int64:
            tiledb_dt = TILEDB_INT64;
            break;
        case GDT_CFloat32:
        case GDT_Float32:
            tiledb_dt = TILEDB_FLOAT32;
            break;
        case GDT_CFloat64:
        case GDT_Float64:
            tiledb_dt = TILEDB_FLOAT64;
            break;

        case GDT_Unknown:
        case GDT_TypeCount:
        {
            CPLError(CE_Failure, CPLE_NotSupported, "Unsupported data type: %s",
                     GDALGetDataTypeName(dt));
            return false;
        }
    }
    return true;
}

/************************************************************************/
/*                 IsIncreasingOrDecreasing1DVar()                      */
/************************************************************************/

static void
IsIncreasingOrDecreasing1DVar(const std::shared_ptr<GDALMDArray> &poVar,
                              bool &bIncreasing, bool &bDecreasing)
{
    bIncreasing = false;
    bDecreasing = false;
    std::vector<double> adfVals;
    try
    {
        adfVals.resize(
            static_cast<size_t>(poVar->GetDimensions()[0]->GetSize()));
    }
    catch (const std::exception &)
    {
    }
    if (adfVals.size() > 1)
    {
        GUInt64 anStart[1] = {0};
        size_t anCount[1] = {adfVals.size()};
        if (poVar->Read(anStart, anCount, nullptr, nullptr,
                        GDALExtendedDataType::Create(GDT_Float64),
                        adfVals.data()))
        {
            if (adfVals[1] > adfVals[0])
                bIncreasing = true;
            else if (adfVals[1] < adfVals[0])
                bDecreasing = true;
            if (bIncreasing || bDecreasing)
            {
                for (size_t i = 2; i < adfVals.size(); ++i)
                {
                    if (bIncreasing)
                    {
                        if (!(adfVals[i] > adfVals[i - 1]))
                        {
                            bIncreasing = false;
                            break;
                        }
                    }
                    else
                    {
                        if (!(adfVals[i] < adfVals[i - 1]))
                        {
                            bDecreasing = false;
                            break;
                        }
                    }
                }
            }
        }
    }
}

/************************************************************************/
/*                   TileDBArray::CreateOnDisk()                        */
/************************************************************************/

/* static */
std::shared_ptr<TileDBArray> TileDBArray::CreateOnDisk(
    const std::shared_ptr<TileDBSharedResource> &poSharedResource,
    const std::shared_ptr<TileDBGroup> &poParent, const std::string &osName,
    const std::vector<std::shared_ptr<GDALDimension>> &aoDimensions,
    const GDALExtendedDataType &oDataType, CSLConstList papszOptions)
{
    if (aoDimensions.empty())
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "Zero-dimensions arrays are not supported by TileDB");
        return nullptr;
    }

    tiledb_datatype_t tiledb_dt = TILEDB_ANY;
    if (oDataType.GetClass() != GEDTC_NUMERIC)
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "Only numeric data types are supported");
        return nullptr;
    }
    if (!GDALDataTypeToTileDB(oDataType.GetNumericDataType(), tiledb_dt))
        return nullptr;

    try
    {
        const auto osSanitizedName =
            TileDBSharedResource::SanitizeNameForPath(osName);
        if (osSanitizedName.empty() || STARTS_WITH(osName.c_str(), "./") ||
            STARTS_WITH(osName.c_str(), "../") ||
            STARTS_WITH(osName.c_str(), ".\\") ||
            STARTS_WITH(osName.c_str(), "..\\"))
        {
            CPLError(CE_Failure, CPLE_AppDefined, "Invalid array name");
            return nullptr;
        }
        std::string osArrayPath = poParent->GetPath() + "/" + osSanitizedName;
        const char *pszURI = CSLFetchNameValue(papszOptions, "URI");
        if (pszURI)
            osArrayPath = pszURI;

        auto &ctx = poSharedResource->GetCtx();
        tiledb::VFS vfs(ctx);
        if (vfs.is_dir(osArrayPath))
        {
            CPLError(CE_Failure, CPLE_AppDefined, "Path %s already exists",
                     osArrayPath.c_str());
            return nullptr;
        }

        std::vector<GUInt64> anBlockSize;
#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wnull-dereference"
#endif
        if (!FillBlockSize(aoDimensions, oDataType, anBlockSize, papszOptions))
            return nullptr;
#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif
        auto poSchema =
            std::make_unique<tiledb::ArraySchema>(ctx, TILEDB_DENSE);
        poSchema->set_tile_order(TILEDB_ROW_MAJOR);
        poSchema->set_cell_order(TILEDB_ROW_MAJOR);

        tiledb::FilterList filterList(ctx);
        const char *pszCompression =
            CSLFetchNameValue(papszOptions, "COMPRESSION");
        const char *pszCompressionLevel =
            CSLFetchNameValue(papszOptions, "COMPRESSION_LEVEL");

        if (pszCompression != nullptr)
        {
            int nLevel = (pszCompressionLevel) ? atoi(pszCompressionLevel) : -1;
            if (TileDBDataset::AddFilter(ctx, filterList, pszCompression,
                                         nLevel) != CE_None)
            {
                return nullptr;
            }
        }
        poSchema->set_coords_filter_list(filterList);

        tiledb::Domain domain(ctx);
        for (size_t i = 0; i < aoDimensions.size(); ++i)
        {
            const auto &poDim = aoDimensions[i];
            if (poDim->GetSize() == 0)
            {
                CPLError(CE_Failure, CPLE_AppDefined, "Invalid dim size: 0");
                return nullptr;
            }
            std::string osDimName(poDim->GetName());
            if (poDim->GetName() == osName)
                osDimName += "_dim";
            auto dim = tiledb::Dimension::create<uint64_t>(
                ctx, osDimName, {0, poDim->GetSize() - 1}, anBlockSize[i]);
            domain.add_dimension(std::move(dim));
        }

        poSchema->set_domain(domain);

        std::vector<std::shared_ptr<GDALMDArray>> apoIndexingVariables;
        for (size_t i = 0; i < aoDimensions.size(); ++i)
        {
            const auto &poDim = aoDimensions[i];
            auto poIndexingVar = poDim->GetIndexingVariable();
            bool bDimLabelCreated = false;
            tiledb_datatype_t dim_label_tiledb_dt = TILEDB_ANY;
            if (poIndexingVar && poIndexingVar->GetDimensionCount() == 1 &&
                poIndexingVar->GetDataType().GetClass() == GEDTC_NUMERIC &&
                poIndexingVar->GetDimensions()[0]->GetName() ==
                    poDim->GetName() &&
                poIndexingVar->GetDimensions()[0]->GetSize() <
                    10 * 1024 * 1024 &&
                !GDALDataTypeIsComplex(
                    poIndexingVar->GetDataType().GetNumericDataType()) &&
                GDALDataTypeToTileDB(
                    poIndexingVar->GetDataType().GetNumericDataType(),
                    dim_label_tiledb_dt))
            {
                bool bIncreasing = false;
                bool bDecreasing = false;
                IsIncreasingOrDecreasing1DVar(poIndexingVar, bIncreasing,
                                              bDecreasing);
                if (bIncreasing || bDecreasing)
                {
                    bDimLabelCreated = true;
                    apoIndexingVariables.push_back(poIndexingVar);
                    tiledb::ArraySchemaExperimental::add_dimension_label(
                        ctx, *(poSchema.get()), static_cast<uint32_t>(i),
                        BuildDimensionLabelName(poDim),
                        bIncreasing ? TILEDB_INCREASING_DATA
                                    : TILEDB_DECREASING_DATA,
                        dim_label_tiledb_dt,
                        std::optional<tiledb::FilterList>(filterList));
                }
            }
            if (poIndexingVar && !bDimLabelCreated)
            {
                CPLDebug("TILEDB",
                         "Dimension %s has indexing variable %s, "
                         "but not compatible of a dimension label",
                         poDim->GetName().c_str(),
                         poIndexingVar->GetName().c_str());
            }
        }

        auto attr = std::make_unique<tiledb::Attribute>(
            tiledb::Attribute::create(ctx, osName, tiledb_dt));
        if (GDALDataTypeIsComplex(oDataType.GetNumericDataType()))
            attr->set_cell_val_num(2);
        attr->set_filter_list(filterList);

        // Implement a deferred TileDB array creation given that we might
        // need to set the fill value of the attribute from the nodata value
        auto poArray = Create(poSharedResource, poParent->GetFullName(), osName,
                              aoDimensions, oDataType, osArrayPath);
        poArray->m_bFinalized = false;
        poArray->m_poParent = poParent;
        poArray->m_osParentPath = poParent->GetPath();
        poArray->m_poSchema = std::move(poSchema);
        poArray->m_osAttrName = attr->name();
        poArray->m_poAttr = std::move(attr);
        poArray->m_anBlockSize = std::move(anBlockSize);
        poArray->m_anStartDimOffset.resize(aoDimensions.size());
        // To keep a reference on the indexing variables, so they are still
        // alive at Finalize() time
        poArray->m_apoIndexingVariables = std::move(apoIndexingVariables);
        if (CPLTestBool(CSLFetchNameValueDef(papszOptions, "STATS", "FALSE")))
            poArray->m_bStats = true;

        uint64_t nTimestamp = poSharedResource->GetTimestamp();
        const char *pszTimestamp =
            CSLFetchNameValue(papszOptions, "TILEDB_TIMESTAMP");
        if (pszTimestamp)
            nTimestamp = std::strtoull(pszTimestamp, nullptr, 10);
        poArray->m_nTimestamp = nTimestamp;

        return poArray;
    }
    catch (const std::exception &e)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "CreateMDArray() failed with: %s",
                 e.what());
        return nullptr;
    }
}

/************************************************************************/
/*                    TileDBArray::GetStructuralInfo()                  */
/************************************************************************/

CSLConstList TileDBArray::GetStructuralInfo() const
{
    return m_aosStructuralInfo.List();
}
