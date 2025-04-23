/******************************************************************************
 *
 * Project:  Parquet Translator
 * Purpose:  Implements OGRParquetDriver.
 * Author:   Even Rouault, <even.rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2022-2024, Planet Labs
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include "ogrsf_frmts.h"

#include <algorithm>
#include <cassert>
#include <map>
#include <set>
#include <utility>

#include "cpl_time.h"
#include "ogr_api.h"

#include "ogr_parquet.h"

#include "../arrow_common/ograrrowlayer.hpp"
#include "../arrow_common/ograrrowdataset.hpp"

#if PARQUET_VERSION_MAJOR >= 13
// Using field indices for FieldRef is only supported since
// https://github.com/apache/arrow/commit/10eedbe63c71f4cf8f0621f3a2304ab3168a2ae5
#define SUPPORTS_INDICES_IN_FIELD_REF
#endif

namespace cp = ::arrow::compute;

/************************************************************************/
/*                        OGRParquetLayer()                             */
/************************************************************************/

OGRParquetDatasetLayer::OGRParquetDatasetLayer(
    OGRParquetDataset *poDS, const char *pszLayerName, bool bIsVSI,
    const std::shared_ptr<arrow::dataset::Dataset> &dataset,
    CSLConstList papszOpenOptions)
    : OGRParquetLayerBase(poDS, pszLayerName, papszOpenOptions),
      m_bIsVSI(bIsVSI), m_poDataset(dataset)
{
    m_poSchema = m_poDataset->schema();
    EstablishFeatureDefn();
    CPLAssert(static_cast<int>(m_aeGeomEncoding.size()) ==
              m_poFeatureDefn->GetGeomFieldCount());
}

/************************************************************************/
/*                  ProcessGeometryColumnCovering()                     */
/************************************************************************/

/** Process GeoParquet JSON geometry field object to extract information about
 * its bounding box column, and appropriately fill m_oMapGeomFieldIndexToGeomColBBOX
 * member with information on that bounding box column.
 */
void OGRParquetDatasetLayer::ProcessGeometryColumnCovering(
    const std::shared_ptr<arrow::Field> &field,
    const CPLJSONObject &oJSONGeometryColumn)
{
    std::string osBBOXColumn;
    std::string osXMin, osYMin, osXMax, osYMax;
    if (ParseGeometryColumnCovering(oJSONGeometryColumn, osBBOXColumn, osXMin,
                                    osYMin, osXMax, osYMax))
    {
        OGRArrowLayer::GeomColBBOX sDesc;
        sDesc.iArrowCol = m_poSchema->GetFieldIndex(osBBOXColumn);
        const auto fieldBBOX = m_poSchema->GetFieldByName(osBBOXColumn);
        if (sDesc.iArrowCol >= 0 && fieldBBOX &&
            fieldBBOX->type()->id() == arrow::Type::STRUCT)
        {
            const auto fieldBBOXStruct =
                std::static_pointer_cast<arrow::StructType>(fieldBBOX->type());
            const auto fieldXMin = fieldBBOXStruct->GetFieldByName(osXMin);
            const auto fieldYMin = fieldBBOXStruct->GetFieldByName(osYMin);
            const auto fieldXMax = fieldBBOXStruct->GetFieldByName(osXMax);
            const auto fieldYMax = fieldBBOXStruct->GetFieldByName(osYMax);
            const int nXMinIdx = fieldBBOXStruct->GetFieldIndex(osXMin);
            const int nYMinIdx = fieldBBOXStruct->GetFieldIndex(osYMin);
            const int nXMaxIdx = fieldBBOXStruct->GetFieldIndex(osXMax);
            const int nYMaxIdx = fieldBBOXStruct->GetFieldIndex(osYMax);
            if (nXMinIdx >= 0 && nYMinIdx >= 0 && nXMaxIdx >= 0 &&
                nYMaxIdx >= 0 && fieldXMin && fieldYMin && fieldXMax &&
                fieldYMax &&
                (fieldXMin->type()->id() == arrow::Type::FLOAT ||
                 fieldXMin->type()->id() == arrow::Type::DOUBLE) &&
                fieldXMin->type()->id() == fieldYMin->type()->id() &&
                fieldXMin->type()->id() == fieldXMax->type()->id() &&
                fieldXMin->type()->id() == fieldYMax->type()->id())
            {
                CPLDebug("PARQUET",
                         "Bounding box column '%s' detected for "
                         "geometry column '%s'",
                         osBBOXColumn.c_str(), field->name().c_str());
                sDesc.iArrowSubfieldXMin = nXMinIdx;
                sDesc.iArrowSubfieldYMin = nYMinIdx;
                sDesc.iArrowSubfieldXMax = nXMaxIdx;
                sDesc.iArrowSubfieldYMax = nYMaxIdx;
                sDesc.bIsFloat =
                    (fieldXMin->type()->id() == arrow::Type::FLOAT);

                m_oMapGeomFieldIndexToGeomColBBOX
                    [m_poFeatureDefn->GetGeomFieldCount() - 1] =
                        std::move(sDesc);
            }
        }
    }
}

/************************************************************************/
/*                        EstablishFeatureDefn()                        */
/************************************************************************/

void OGRParquetDatasetLayer::EstablishFeatureDefn()
{
    const auto &kv_metadata = m_poSchema->metadata();

    LoadGeoMetadata(kv_metadata);
    const auto oMapFieldNameToGDALSchemaFieldDefn =
        LoadGDALSchema(kv_metadata.get());

    LoadGDALMetadata(kv_metadata.get());

    const bool bUseBBOX =
        CPLTestBool(CPLGetConfigOption("OGR_PARQUET_USE_BBOX", "YES"));

    // Keep track of declared bounding box columns in GeoParquet JSON metadata,
    // in order not to expose them as regular fields.
    std::set<std::string> oSetBBOXColumns;
    if (bUseBBOX)
    {
        for (const auto &iter : m_oMapGeometryColumns)
        {
            std::string osBBOXColumn;
            std::string osXMin, osYMin, osXMax, osYMax;
            if (ParseGeometryColumnCovering(iter.second, osBBOXColumn, osXMin,
                                            osYMin, osXMax, osYMax))
            {
                oSetBBOXColumns.insert(osBBOXColumn);
            }
        }
    }

    const auto &fields = m_poSchema->fields();

    // Overture Maps 2024-04-16-beta.0 almost follows GeoParquet 1.1, except
    // they don't declare the "covering" element in the GeoParquet JSON metadata
    if (m_oMapGeometryColumns.find("geometry") != m_oMapGeometryColumns.end() &&
        bUseBBOX &&
        !m_oMapGeometryColumns["geometry"].GetObj("covering").IsValid() &&
        m_oMapGeometryColumns["geometry"].GetString("encoding") == "WKB")
    {
        for (int i = 0; i < m_poSchema->num_fields(); ++i)
        {
            const auto &field = fields[i];
            if (field->name() == "bbox" &&
                field->type()->id() == arrow::Type::STRUCT)
            {
                bool bXMin = false;
                bool bXMax = false;
                bool bYMin = false;
                bool bYMax = false;
                const auto subfields = field->Flatten();
                if (subfields.size() == 4)
                {
                    for (int j = 0; j < static_cast<int>(subfields.size()); j++)
                    {
                        const auto &subfield = subfields[j];
                        if (subfield->name() == "bbox.xmin")
                            bXMin = true;
                        else if (subfield->name() == "bbox.xmax")
                            bXMax = true;
                        else if (subfield->name() == "bbox.ymin")
                            bYMin = true;
                        else if (subfield->name() == "bbox.ymax")
                            bYMax = true;
                    }
                }
                if (bXMin && bXMax && bYMin && bYMax)
                {
                    CPLJSONObject oDef = m_oMapGeometryColumns["geometry"];
                    CPLJSONObject oCovering;
                    oDef.Add("covering", oCovering);
                    CPLJSONObject oBBOX;
                    oCovering.Add("bbox", oBBOX);
                    {
                        CPLJSONArray oArray;
                        oArray.Add("bbox");
                        oArray.Add("xmin");
                        oBBOX.Add("xmin", oArray);
                    }
                    {
                        CPLJSONArray oArray;
                        oArray.Add("bbox");
                        oArray.Add("ymin");
                        oBBOX.Add("ymin", oArray);
                    }
                    {
                        CPLJSONArray oArray;
                        oArray.Add("bbox");
                        oArray.Add("xmax");
                        oBBOX.Add("xmax", oArray);
                    }
                    {
                        CPLJSONArray oArray;
                        oArray.Add("bbox");
                        oArray.Add("ymax");
                        oBBOX.Add("ymax", oArray);
                    }
                    oSetBBOXColumns.insert("bbox");
                    m_oMapGeometryColumns["geometry"] = std::move(oDef);
                }
                break;
            }
        }
    }

    for (int i = 0; i < m_poSchema->num_fields(); ++i)
    {
        const auto &field = fields[i];

        if (!m_osFIDColumn.empty() && field->name() == m_osFIDColumn)
        {
            m_iFIDArrowColumn = i;
            continue;
        }

        if (oSetBBOXColumns.find(field->name()) != oSetBBOXColumns.end())
        {
            m_oSetBBoxArrowColumns.insert(i);
            continue;
        }

        const bool bGeometryField =
            DealWithGeometryColumn(i, field, []() { return wkbUnknown; });
        if (bGeometryField)
        {
            const auto oIter = m_oMapGeometryColumns.find(field->name());
            if (bUseBBOX && oIter != m_oMapGeometryColumns.end())
            {
                ProcessGeometryColumnCovering(field, oIter->second);
            }
        }
        else
        {
            CreateFieldFromSchema(field, {i},
                                  oMapFieldNameToGDALSchemaFieldDefn);
        }
    }

    CPLAssert(static_cast<int>(m_anMapFieldIndexToArrowColumn.size()) ==
              m_poFeatureDefn->GetFieldCount());
    CPLAssert(static_cast<int>(m_anMapGeomFieldIndexToArrowColumn.size()) ==
              m_poFeatureDefn->GetGeomFieldCount());
}

namespace
{

/************************************************************************/
/*                        WKBGeometryOptionsType                        */
/************************************************************************/

class WKBGeometryOptions;

class WKBGeometryOptionsType : public cp::FunctionOptionsType
{
    WKBGeometryOptionsType() = default;

    static const WKBGeometryOptions &Cast(const cp::FunctionOptions &opts);

  public:
    const char *type_name() const override
    {
        return "WKBGeometryOptionsType";
    }

    std::string Stringify(const cp::FunctionOptions &) const override;
    bool Compare(const cp::FunctionOptions &,
                 const cp::FunctionOptions &) const override;
    std::unique_ptr<cp::FunctionOptions>
    Copy(const cp::FunctionOptions &) const override;

    static WKBGeometryOptionsType *GetSingleton()
    {
        static WKBGeometryOptionsType instance;
        return &instance;
    }
};

/************************************************************************/
/*                         WKBGeometryOptions                           */
/************************************************************************/

class WKBGeometryOptions : public cp::FunctionOptions
{

  public:
    explicit WKBGeometryOptions(
        const std::vector<GByte> &abyFilterGeomWkbIn = std::vector<GByte>())
        : cp::FunctionOptions(WKBGeometryOptionsType::GetSingleton()),
          abyFilterGeomWkb(abyFilterGeomWkbIn)
    {
    }

    bool operator==(const WKBGeometryOptions &other) const
    {
        return abyFilterGeomWkb == other.abyFilterGeomWkb;
    }

    std::vector<GByte> abyFilterGeomWkb;
};

const WKBGeometryOptions &
WKBGeometryOptionsType::Cast(const cp::FunctionOptions &opts)
{
    return *cpl::down_cast<const WKBGeometryOptions *>(&opts);
}

bool WKBGeometryOptionsType::Compare(const cp::FunctionOptions &optsA,
                                     const cp::FunctionOptions &optsB) const
{
    return Cast(optsA) == Cast(optsB);
}

std::string
WKBGeometryOptionsType::Stringify(const cp::FunctionOptions &opts) const
{
    const auto &bboxOptions = Cast(opts);
    std::string osRet(type_name());
    osRet += '-';
    for (GByte byVal : bboxOptions.abyFilterGeomWkb)
        osRet += CPLSPrintf("%02X", byVal);
    return osRet;
}

std::unique_ptr<cp::FunctionOptions>
WKBGeometryOptionsType::Copy(const cp::FunctionOptions &opts) const
{
    return std::make_unique<WKBGeometryOptions>(Cast(opts));
}

/************************************************************************/
/*                            OptionsWrapper                            */
/************************************************************************/

/// KernelState adapter for the common case of kernels whose only
/// state is an instance of a subclass of FunctionOptions.
template <typename OptionsType> struct OptionsWrapper : public cp::KernelState
{
    explicit OptionsWrapper(OptionsType optionsIn)
        : options(std::move(optionsIn))
    {
    }

    static arrow::Result<std::unique_ptr<cp::KernelState>>
    Init(cp::KernelContext *, const cp::KernelInitArgs &args)
    {
        auto options = cpl::down_cast<const OptionsType *>(args.options);
        CPLAssert(options);
        return std::make_unique<OptionsWrapper>(*options);
    }

    static const OptionsType &Get(cp::KernelContext *ctx)
    {
        return cpl::down_cast<const OptionsWrapper *>(ctx->state())->options;
    }

    OptionsType options;
};
}  // namespace

/************************************************************************/
/*                       ExecOGRWKBIntersects()                         */
/************************************************************************/

static arrow::Status ExecOGRWKBIntersects(cp::KernelContext *ctx,
                                          const cp::ExecSpan &batch,
                                          cp::ExecResult *out)
{
    // Get filter geometry
    const auto &opts = OptionsWrapper<WKBGeometryOptions>::Get(ctx);
    OGRGeometry *poGeomTmp = nullptr;
    OGRErr eErr = OGRGeometryFactory::createFromWkb(
        opts.abyFilterGeomWkb.data(), nullptr, &poGeomTmp,
        opts.abyFilterGeomWkb.size());
    CPL_IGNORE_RET_VAL(eErr);
    CPLAssert(eErr == OGRERR_NONE);
    CPLAssert(poGeomTmp != nullptr);
    std::unique_ptr<OGRGeometry> poFilterGeom(poGeomTmp);
    OGREnvelope sFilterEnvelope;
    poFilterGeom->getEnvelope(&sFilterEnvelope);
    const bool bFilterIsEnvelope = poFilterGeom->IsRectangle();

    // Deal with input array
    CPLAssert(batch.num_values() == 1);
    const arrow::ArraySpan &input = batch[0].array;
    CPLAssert(input.type->id() == arrow::Type::BINARY);
    // Packed array of bits
    const auto pabyInputValidity = input.buffers[0].data;
    const auto nInputOffsets = input.offset;
    const auto panWkbOffsets = input.GetValues<int32_t>(1);
    const auto pabyWkbArray = input.buffers[2].data;

    // Deal with output array
    CPLAssert(out->type()->id() == arrow::Type::BOOL);
    auto out_span = out->array_span();
    // Below array holds 8 bits per uint8_t
    uint8_t *pabitsOutValues = out_span->buffers[1].data;
    const auto nOutOffset = out_span->offset;

    // Iterate over WKB geometries
    OGRPreparedGeometry *pPreparedFilterGeom = nullptr;
    OGREnvelope sEnvelope;
    for (int64_t i = 0; i < batch.length; ++i)
    {
        const bool bInputIsNull =
            (pabyInputValidity &&
             arrow::bit_util::GetBit(pabyInputValidity, i + nInputOffsets) ==
                 0);
        bool bOutputVal = false;
        if (!bInputIsNull)
        {
            const GByte *pabyWkb = pabyWkbArray + panWkbOffsets[i];
            const size_t nWkbSize = panWkbOffsets[i + 1] - panWkbOffsets[i];
            bOutputVal = OGRLayer::FilterWKBGeometry(
                pabyWkb, nWkbSize,
                /* bEnvelopeAlreadySet = */ false, sEnvelope,
                poFilterGeom.get(), bFilterIsEnvelope, sFilterEnvelope,
                pPreparedFilterGeom);
        }
        if (bOutputVal)
            arrow::bit_util::SetBit(pabitsOutValues, i + nOutOffset);
        else
            arrow::bit_util::ClearBit(pabitsOutValues, i + nOutOffset);
    }

    // Cleanup
    if (pPreparedFilterGeom)
        OGRDestroyPreparedGeometry(pPreparedFilterGeom);

    return arrow::Status::OK();
}

/************************************************************************/
/*                    RegisterOGRWKBIntersectsIfNeeded()                */
/************************************************************************/

static bool RegisterOGRWKBIntersectsIfNeeded()
{
    auto registry = cp::GetFunctionRegistry();
    bool bRet =
        registry->GetFunction("OGRWKBIntersects").ValueOr(nullptr) != nullptr;
    if (!bRet)
    {
        static const WKBGeometryOptions defaultOpts;

        // Below assert is completely useless but helps improve test coverage
        CPLAssert(WKBGeometryOptionsType::GetSingleton()->Compare(
            defaultOpts, *(WKBGeometryOptionsType::GetSingleton()
                               ->Copy(defaultOpts)
                               .get())));

        auto func = std::make_shared<cp::ScalarFunction>(
            "OGRWKBIntersects", cp::Arity::Unary(), cp::FunctionDoc(),
            &defaultOpts);
        cp::ScalarKernel kernel({arrow::binary()}, arrow::boolean(),
                                ExecOGRWKBIntersects,
                                OptionsWrapper<WKBGeometryOptions>::Init);
        kernel.null_handling = cp::NullHandling::OUTPUT_NOT_NULL;
        bRet = func->AddKernel(std::move(kernel)).ok() &&
               registry->AddFunction(std::move(func)).ok();
    }
    return bRet;
}

/************************************************************************/
/*                              BuildScanner()                          */
/************************************************************************/

void OGRParquetDatasetLayer::BuildScanner()
{
    m_bRebuildScanner = false;
    m_bSkipFilterGeometry = false;
    m_bBaseArrowIgnoreSpatialFilterRect = false;
    m_bBaseArrowIgnoreSpatialFilter = false;
    m_bBaseArrowIgnoreAttributeFilter = false;

    try
    {
        std::shared_ptr<arrow::dataset::ScannerBuilder> scannerBuilder;
        PARQUET_ASSIGN_OR_THROW(scannerBuilder, m_poDataset->NewScan());
        assert(scannerBuilder);

        // We cannot use the shared memory pool. Otherwise we get random
        // crashes in multi-threaded arrow code (apparently some cleanup code),
        // that may used the memory pool after it has been destroyed.
        // At least this was true with some older libarrow version
        // PARQUET_THROW_NOT_OK(scannerBuilder->Pool(m_poMemoryPool));

        if (m_bIsVSI)
        {
            const int nFragmentReadAhead = atoi(
                CPLGetConfigOption("OGR_PARQUET_FRAGMENT_READ_AHEAD", "2"));
            PARQUET_THROW_NOT_OK(
                scannerBuilder->FragmentReadahead(nFragmentReadAhead));
        }

        const char *pszBatchSize =
            CPLGetConfigOption("OGR_PARQUET_BATCH_SIZE", nullptr);
        if (pszBatchSize)
        {
            PARQUET_THROW_NOT_OK(
                scannerBuilder->BatchSize(CPLAtoGIntBig(pszBatchSize)));
        }

        const int nNumCPUs = GetNumCPUs();
        const char *pszUseThreads =
            CPLGetConfigOption("OGR_PARQUET_USE_THREADS", nullptr);
        if (!pszUseThreads && nNumCPUs > 1)
        {
            pszUseThreads = "YES";
        }
        if (pszUseThreads && CPLTestBool(pszUseThreads))
        {
            PARQUET_THROW_NOT_OK(scannerBuilder->UseThreads(true));
        }

#if PARQUET_VERSION_MAJOR >= 10
        const char *pszBatchReadAhead =
            CPLGetConfigOption("OGR_PARQUET_BATCH_READ_AHEAD", nullptr);
        if (pszBatchReadAhead)
        {
            PARQUET_THROW_NOT_OK(
                scannerBuilder->BatchReadahead(atoi(pszBatchReadAhead)));
        }
#endif

        cp::Expression expression;
        if (m_poFilterGeom && !m_poFilterGeom->IsEmpty() &&
            CPLTestBool(CPLGetConfigOption(
                "OGR_PARQUET_OPTIMIZED_SPATIAL_FILTER", "YES")))
        {
            const auto oIter =
                m_oMapGeomFieldIndexToGeomColBBOX.find(m_iGeomFieldFilter);
            if (oIter != m_oMapGeomFieldIndexToGeomColBBOX.end())
            {
                // This actually requires Arrow >= 15 (https://github.com/apache/arrow/issues/39064)
                // to be more efficient.
#ifdef SUPPORTS_INDICES_IN_FIELD_REF
                const auto &oBBOXDef = oIter->second;
                expression = cp::and_(
                    {cp::less_equal(
                         cp::field_ref(arrow::FieldRef(
                             oBBOXDef.iArrowCol, oBBOXDef.iArrowSubfieldXMin)),
                         cp::literal(m_sFilterEnvelope.MaxX)),
                     cp::less_equal(
                         cp::field_ref(arrow::FieldRef(
                             oBBOXDef.iArrowCol, oBBOXDef.iArrowSubfieldYMin)),
                         cp::literal(m_sFilterEnvelope.MaxY)),
                     cp::greater_equal(
                         cp::field_ref(arrow::FieldRef(
                             oBBOXDef.iArrowCol, oBBOXDef.iArrowSubfieldXMax)),
                         cp::literal(m_sFilterEnvelope.MinX)),
                     cp::greater_equal(
                         cp::field_ref(arrow::FieldRef(
                             oBBOXDef.iArrowCol, oBBOXDef.iArrowSubfieldYMax)),
                         cp::literal(m_sFilterEnvelope.MinY))});
#else
                const auto oIter2 = m_oMapGeometryColumns.find(
                    m_poFeatureDefn->GetGeomFieldDefn(m_iGeomFieldFilter)
                        ->GetNameRef());
                std::string osBBOXColumn;
                std::string osXMin, osYMin, osXMax, osYMax;
                if (ParseGeometryColumnCovering(oIter2->second, osBBOXColumn,
                                                osXMin, osYMin, osXMax, osYMax))
                {
                    expression = cp::and_(
                        {cp::less_equal(cp::field_ref(arrow::FieldRef(
                                            osBBOXColumn, osXMin)),
                                        cp::literal(m_sFilterEnvelope.MaxX)),
                         cp::less_equal(cp::field_ref(arrow::FieldRef(
                                            osBBOXColumn, osYMin)),
                                        cp::literal(m_sFilterEnvelope.MaxY)),
                         cp::greater_equal(cp::field_ref(arrow::FieldRef(
                                               osBBOXColumn, osXMax)),
                                           cp::literal(m_sFilterEnvelope.MinX)),
                         cp::greater_equal(
                             cp::field_ref(
                                 arrow::FieldRef(osBBOXColumn, osYMax)),
                             cp::literal(m_sFilterEnvelope.MinY))});
                }
#endif
            }
            else if (m_iGeomFieldFilter >= 0 &&
                     m_iGeomFieldFilter <
                         static_cast<int>(m_aeGeomEncoding.size()) &&
                     m_aeGeomEncoding[m_iGeomFieldFilter] ==
                         OGRArrowGeomEncoding::GEOARROW_STRUCT_POINT)
            {
                const int iCol =
                    m_anMapGeomFieldIndexToArrowColumn[m_iGeomFieldFilter];
                const auto &field = m_poSchema->fields()[iCol];
                auto type = field->type();
                std::vector<arrow::FieldRef> fieldRefs;
#ifdef SUPPORTS_INDICES_IN_FIELD_REF
                fieldRefs.emplace_back(iCol);
#else
                fieldRefs.emplace_back(field->name());
#endif
                if (type->id() == arrow::Type::STRUCT)
                {
                    const auto fieldStruct =
                        std::static_pointer_cast<arrow::StructType>(type);
                    const auto fieldX = fieldStruct->GetFieldByName("x");
                    const auto fieldY = fieldStruct->GetFieldByName("y");
                    if (fieldX && fieldY)
                    {
                        auto fieldRefX(fieldRefs);
                        fieldRefX.emplace_back("x");
                        auto fieldRefY(std::move(fieldRefs));
                        fieldRefY.emplace_back("y");
                        expression = cp::and_(
                            {cp::less_equal(
                                 cp::field_ref(arrow::FieldRef(fieldRefX)),
                                 cp::literal(m_sFilterEnvelope.MaxX)),
                             cp::less_equal(
                                 cp::field_ref(arrow::FieldRef(fieldRefY)),
                                 cp::literal(m_sFilterEnvelope.MaxY)),
                             cp::greater_equal(
                                 cp::field_ref(arrow::FieldRef(fieldRefX)),
                                 cp::literal(m_sFilterEnvelope.MinX)),
                             cp::greater_equal(
                                 cp::field_ref(arrow::FieldRef(fieldRefY)),
                                 cp::literal(m_sFilterEnvelope.MinY))});
                    }
                }
            }
            else if (m_iGeomFieldFilter >= 0 &&
                     m_iGeomFieldFilter <
                         static_cast<int>(m_aeGeomEncoding.size()) &&
                     m_aeGeomEncoding[m_iGeomFieldFilter] ==
                         OGRArrowGeomEncoding::WKB)
            {
                const int iCol =
                    m_anMapGeomFieldIndexToArrowColumn[m_iGeomFieldFilter];
                const auto &field = m_poSchema->fields()[iCol];
                if (field->type()->id() == arrow::Type::BINARY &&
                    RegisterOGRWKBIntersectsIfNeeded())
                {
#ifdef SUPPORTS_INDICES_IN_FIELD_REF
                    auto oFieldRef = arrow::FieldRef(iCol);
#else
                    auto oFieldRef = arrow::FieldRef(field->name());
#endif
                    std::vector<GByte> abyFilterGeomWkb;
                    abyFilterGeomWkb.resize(m_poFilterGeom->WkbSize());
                    m_poFilterGeom->exportToWkb(wkbNDR, abyFilterGeomWkb.data(),
                                                wkbVariantIso);
                    // Silence 'Using uninitialized value oFieldRef. Field oFieldRef.impl_._M_u is uninitialized when calling FieldRef.'
                    // coverity[uninit_use_in_call]
                    expression = cp::call("OGRWKBIntersects",
                                          {cp::field_ref(std::move(oFieldRef))},
                                          WKBGeometryOptions(abyFilterGeomWkb));

                    if (expression.is_valid())
                    {
                        m_bBaseArrowIgnoreSpatialFilterRect = true;
                        m_bBaseArrowIgnoreSpatialFilter = true;
                        m_bSkipFilterGeometry = true;
                    }
                }
            }

            if (expression.is_valid() && !m_bSkipFilterGeometry)
            {
                m_bBaseArrowIgnoreSpatialFilterRect = true;

                const bool bIsPoint =
                    wkbFlatten(
                        m_poFeatureDefn->GetGeomFieldDefn(m_iGeomFieldFilter)
                            ->GetType()) == wkbPoint;
                m_bBaseArrowIgnoreSpatialFilter =
                    m_bFilterIsEnvelope && bIsPoint;

                m_bSkipFilterGeometry =
                    m_bFilterIsEnvelope &&
                    (bIsPoint ||
                     m_poFeatureDefn->GetGeomFieldDefn(m_iGeomFieldFilter)
                         ->IsIgnored());
            }
        }

        if (m_poAttrQuery &&
            CPLTestBool(CPLGetConfigOption(
                "OGR_PARQUET_OPTIMIZED_ATTRIBUTE_FILTER", "YES")))
        {
            const swq_expr_node *poNode =
                static_cast<swq_expr_node *>(m_poAttrQuery->GetSWQExpr());
            bool bFullyTranslated = true;
            auto expressionFilter = BuildArrowFilter(poNode, bFullyTranslated);
            if (expressionFilter.is_valid())
            {
                if (bFullyTranslated)
                {
                    CPLDebugOnly("PARQUET",
                                 "Attribute filter fully translated to Arrow");
                    m_asAttributeFilterConstraints.clear();
                    m_bBaseArrowIgnoreAttributeFilter = true;
                }

                if (expression.is_valid())
                    expression =
                        cp::and_(expression, std::move(expressionFilter));
                else
                    expression = std::move(expressionFilter);
            }
        }

        if (expression.is_valid())
        {
            PARQUET_THROW_NOT_OK(scannerBuilder->Filter(expression));
        }

        if (m_bIgnoredFields)
        {
#ifdef DEBUG
            std::string osFields;
            for (const std::string &osField : m_aosProjectedFields)
            {
                if (!osFields.empty())
                    osFields += ',';
                osFields += osField;
            }
            CPLDebug("PARQUET", "Projected fields: %s", osFields.c_str());
#endif
            PARQUET_THROW_NOT_OK(scannerBuilder->Project(m_aosProjectedFields));
        }

        PARQUET_ASSIGN_OR_THROW(m_poScanner, scannerBuilder->Finish());
    }
    catch (const std::exception &e)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Arrow/Parquet exception: %s",
                 e.what());
    }
}

/************************************************************************/
/*                           BuildArrowFilter()                         */
/************************************************************************/

cp::Expression
OGRParquetDatasetLayer::BuildArrowFilter(const swq_expr_node *poNode,
                                         bool &bFullyTranslated)
{
    if (poNode->eNodeType == SNT_OPERATION && poNode->nOperation == SWQ_AND &&
        poNode->nSubExprCount == 2)
    {
        const auto sLeft =
            BuildArrowFilter(poNode->papoSubExpr[0], bFullyTranslated);
        const auto sRight =
            BuildArrowFilter(poNode->papoSubExpr[1], bFullyTranslated);
        if (sLeft.is_valid() && sRight.is_valid())
            return cp::and_(sLeft, sRight);
        if (sLeft.is_valid())
            return sLeft;
        if (sRight.is_valid())
            return sRight;
    }

    else if (poNode->eNodeType == SNT_OPERATION &&
             poNode->nOperation == SWQ_OR && poNode->nSubExprCount == 2)
    {
        const auto sLeft =
            BuildArrowFilter(poNode->papoSubExpr[0], bFullyTranslated);
        const auto sRight =
            BuildArrowFilter(poNode->papoSubExpr[1], bFullyTranslated);
        if (sLeft.is_valid() && sRight.is_valid())
            return cp::or_(sLeft, sRight);
    }

    else if (poNode->eNodeType == SNT_OPERATION &&
             poNode->nOperation == SWQ_NOT && poNode->nSubExprCount == 1)
    {
        const auto expr =
            BuildArrowFilter(poNode->papoSubExpr[0], bFullyTranslated);
        if (expr.is_valid())
            return cp::not_(expr);
    }

    else if (poNode->eNodeType == SNT_COLUMN)
    {
        if (poNode->field_index >= 0 &&
            poNode->field_index < m_poFeatureDefn->GetFieldCount())
        {
            std::vector<arrow::FieldRef> fieldRefs;
#ifdef SUPPORTS_INDICES_IN_FIELD_REF
            for (int idx : m_anMapFieldIndexToArrowColumn[poNode->field_index])
                fieldRefs.emplace_back(idx);
#else
            std::shared_ptr<arrow::Field> field;
            for (int idx : m_anMapFieldIndexToArrowColumn[poNode->field_index])
            {
                if (!field)
                {
                    field = m_poSchema->fields()[idx];
                }
                else
                {
                    CPLAssert(field->type()->id() == arrow::Type::STRUCT);
                    const auto fieldStruct =
                        std::static_pointer_cast<arrow::StructType>(
                            field->type());
                    field = fieldStruct->fields()[idx];
                }
                fieldRefs.emplace_back(field->name());
            }
#endif
            auto expr = cp::field_ref(arrow::FieldRef(std::move(fieldRefs)));

            // Comparing a boolean column to 0 or 1 fails without explicit cast
            if (m_poFeatureDefn->GetFieldDefn(poNode->field_index)
                    ->GetSubType() == OFSTBoolean)
            {
                expr = cp::call("cast", {expr},
                                cp::CastOptions::Safe(arrow::uint8()));
            }
            return expr;
        }
        else if (poNode->field_index ==
                     m_poFeatureDefn->GetFieldCount() + SPF_FID &&
                 m_iFIDArrowColumn >= 0)
        {
#ifdef SUPPORTS_INDICES_IN_FIELD_REF
            return cp::field_ref(arrow::FieldRef(m_iFIDArrowColumn));
#else
            return cp::field_ref(arrow::FieldRef(
                m_poSchema->fields()[m_iFIDArrowColumn]->name()));
#endif
        }
    }

    else if (poNode->eNodeType == SNT_CONSTANT)
    {
        switch (poNode->field_type)
        {
            case SWQ_INTEGER:
            case SWQ_INTEGER64:
                return cp::literal(static_cast<int64_t>(poNode->int_value));

            case SWQ_FLOAT:
                return cp::literal(poNode->float_value);

            case SWQ_STRING:
                return cp::literal(poNode->string_value);

            case SWQ_TIMESTAMP:
            {
                OGRField sField;
                if (OGRParseDate(poNode->string_value, &sField, 0))
                {
                    struct tm brokenDown;
                    brokenDown.tm_year = sField.Date.Year - 1900;
                    brokenDown.tm_mon = sField.Date.Month - 1;
                    brokenDown.tm_mday = sField.Date.Day;
                    brokenDown.tm_hour = sField.Date.Hour;
                    brokenDown.tm_min = sField.Date.Minute;
                    brokenDown.tm_sec = static_cast<int>(sField.Date.Second);
                    int64_t nVal =
                        CPLYMDHMSToUnixTime(&brokenDown) * 1000 +
                        (static_cast<int>(sField.Date.Second * 1000 + 0.5) %
                         1000);
                    if (sField.Date.TZFlag > OGR_TZFLAG_MIXED_TZ)
                    {
                        // Convert for sField.Date.TZFlag to UTC
                        const int TZOffset =
                            (sField.Date.TZFlag - OGR_TZFLAG_UTC) * 15;
                        const int TZOffsetMS = TZOffset * 60 * 1000;
                        nVal -= TZOffsetMS;
                        return cp::literal(arrow::TimestampScalar(
                            nVal, arrow::TimeUnit::MILLI, "UTC"));
                    }
                    else
                    {
                        return cp::literal(arrow::TimestampScalar(
                            nVal, arrow::TimeUnit::MILLI));
                    }
                }
            }

            default:
                break;
        }
    }

    else if (poNode->eNodeType == SNT_OPERATION && poNode->nSubExprCount == 2 &&
             IsComparisonOp(poNode->nOperation))
    {
        const auto sLeft =
            BuildArrowFilter(poNode->papoSubExpr[0], bFullyTranslated);
        const auto sRight =
            BuildArrowFilter(poNode->papoSubExpr[1], bFullyTranslated);
        if (sLeft.is_valid() && sRight.is_valid())
        {
            if (poNode->nOperation == SWQ_EQ)
                return cp::equal(sLeft, sRight);
            if (poNode->nOperation == SWQ_LT)
                return cp::less(sLeft, sRight);
            if (poNode->nOperation == SWQ_LE)
                return cp::less_equal(sLeft, sRight);
            if (poNode->nOperation == SWQ_GT)
                return cp::greater(sLeft, sRight);
            if (poNode->nOperation == SWQ_GE)
                return cp::greater_equal(sLeft, sRight);
            if (poNode->nOperation == SWQ_NE)
                return cp::not_equal(sLeft, sRight);
        }
    }

    else if (poNode->eNodeType == SNT_OPERATION && poNode->nSubExprCount == 2 &&
             (poNode->nOperation == SWQ_LIKE ||
              poNode->nOperation == SWQ_ILIKE) &&
             poNode->papoSubExpr[1]->eNodeType == SNT_CONSTANT &&
             poNode->papoSubExpr[1]->field_type == SWQ_STRING)
    {
        const auto sLeft =
            BuildArrowFilter(poNode->papoSubExpr[0], bFullyTranslated);
        if (sLeft.is_valid())
        {
            if (cp::GetFunctionRegistry()
                    ->GetFunction("match_like")
                    .ValueOr(nullptr))
            {
                // match_like is only available is Arrow built against RE2.
                return cp::call(
                    "match_like", {sLeft},
                    cp::MatchSubstringOptions(
                        poNode->papoSubExpr[1]->string_value,
                        /* ignore_case=*/poNode->nOperation == SWQ_ILIKE));
            }
        }
    }

    else if (poNode->eNodeType == SNT_OPERATION &&
             poNode->nOperation == SWQ_ISNULL && poNode->nSubExprCount == 1)
    {
        const auto expr =
            BuildArrowFilter(poNode->papoSubExpr[0], bFullyTranslated);
        if (expr.is_valid())
            return cp::is_null(expr);
    }

    bFullyTranslated = false;
    return {};
}

/************************************************************************/
/*                           ReadNextBatch()                            */
/************************************************************************/

bool OGRParquetDatasetLayer::ReadNextBatch()
{
    if (m_bRebuildScanner)
        BuildScanner();

    m_nIdxInBatch = 0;

    if (m_poRecordBatchReader == nullptr)
    {
        if (!m_poScanner)
            return false;
        auto result = m_poScanner->ToRecordBatchReader();
        if (!result.ok())
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "ToRecordBatchReader() failed: %s",
                     result.status().message().c_str());
            return false;
        }
        m_poRecordBatchReader = *result;
        if (m_poRecordBatchReader == nullptr)
            return false;
    }

    std::shared_ptr<arrow::RecordBatch> poNextBatch;
    do
    {
        ++m_iRecordBatch;

        poNextBatch.reset();
        auto status = m_poRecordBatchReader->ReadNext(&poNextBatch);
        if (!status.ok())
        {
            CPLError(CE_Failure, CPLE_AppDefined, "ReadNext() failed: %s",
                     status.message().c_str());
            poNextBatch.reset();
        }
        if (poNextBatch == nullptr)
        {
            m_poBatch.reset();
            return false;
        }
    } while (poNextBatch->num_rows() == 0);

    // CPLDebug("PARQUET", "Current batch has %d rows", int(poNextBatch->num_rows()));

    SetBatch(poNextBatch);

    return true;
}

/************************************************************************/
/*                        GetNextFeature()                              */
/************************************************************************/

OGRFeature *OGRParquetDatasetLayer::GetNextFeature()
{
    while (true)
    {
        OGRFeature *poFeature = GetNextRawFeature();
        if (poFeature == nullptr)
            return nullptr;

        if ((m_poFilterGeom == nullptr || m_bSkipFilterGeometry ||
             FilterGeometry(poFeature->GetGeometryRef())) &&
            (m_poAttrQuery == nullptr || m_bBaseArrowIgnoreAttributeFilter ||
             m_poAttrQuery->Evaluate(poFeature)))
        {
            return poFeature;
        }
        else
            delete poFeature;
    }
}

/************************************************************************/
/*                        GetFeatureCount()                             */
/************************************************************************/

GIntBig OGRParquetDatasetLayer::GetFeatureCount(int bForce)
{
    if (m_poAttrQuery == nullptr && m_poFilterGeom == nullptr)
    {
        if (m_bRebuildScanner)
            BuildScanner();
        if (!m_poScanner)
            return -1;
        auto status = m_poScanner->CountRows();
        if (status.ok())
            return *status;
    }
    return OGRLayer::GetFeatureCount(bForce);
}

/************************************************************************/
/*                         FastGetExtent()                              */
/************************************************************************/

bool OGRParquetDatasetLayer::FastGetExtent(int iGeomField,
                                           OGREnvelope *psExtent) const
{
    const auto oIter = m_oMapExtents.find(iGeomField);
    if (oIter != m_oMapExtents.end())
    {
        *psExtent = oIter->second;
        return true;
    }

    return false;
}

/************************************************************************/
/*                           IGetExtent()                               */
/************************************************************************/

OGRErr OGRParquetDatasetLayer::IGetExtent(int iGeomField, OGREnvelope *psExtent,
                                          bool bForce)
{
    if (FastGetExtent(iGeomField, psExtent))
    {
        return OGRERR_NONE;
    }

    // bbox in general m_oMapGeometryColumns can not be trusted (at least at
    // time of writing), so we have to iterate over each fragment.
    const char *pszGeomFieldName =
        m_poFeatureDefn->GetGeomFieldDefn(iGeomField)->GetNameRef();
    auto oIter = m_oMapGeometryColumns.find(pszGeomFieldName);
    if (oIter != m_oMapGeometryColumns.end())
    {
        auto statusFragments = m_poDataset->GetFragments();
        if (statusFragments.ok())
        {
            *psExtent = OGREnvelope();
            int nFragmentCount = 0;
            int nBBoxFragmentCount = 0;
            for (const auto &oFragmentStatus : *statusFragments)
            {
                if (oFragmentStatus.ok())
                {
                    auto statusSchema =
                        (*oFragmentStatus)->ReadPhysicalSchema();
                    if (statusSchema.ok())
                    {
                        nFragmentCount++;
                        const auto &kv_metadata = (*statusSchema)->metadata();
                        if (kv_metadata && kv_metadata->Contains("geo"))
                        {
                            auto geo = kv_metadata->Get("geo");
                            CPLJSONDocument oDoc;
                            if (geo.ok() && oDoc.LoadMemory(*geo))
                            {
                                auto oRoot = oDoc.GetRoot();
                                auto oColumns = oRoot.GetObj("columns");
                                auto oCol = oColumns.GetObj(pszGeomFieldName);
                                OGREnvelope3D sFragmentExtent;
                                if (oCol.IsValid() &&
                                    GetExtentFromMetadata(
                                        oCol, &sFragmentExtent) == OGRERR_NONE)
                                {
                                    nBBoxFragmentCount++;
                                    psExtent->Merge(sFragmentExtent);
                                }
                            }
                        }
                        if (nFragmentCount != nBBoxFragmentCount)
                            break;
                    }
                }
            }
            if (nFragmentCount == nBBoxFragmentCount)
            {
                m_oMapExtents[iGeomField] = *psExtent;
                return OGRERR_NONE;
            }
        }
    }

    return OGRParquetLayerBase::IGetExtent(iGeomField, psExtent, bForce);
}

/************************************************************************/
/*                        ISetSpatialFilter()                           */
/************************************************************************/

OGRErr OGRParquetDatasetLayer::ISetSpatialFilter(int iGeomField,
                                                 const OGRGeometry *poGeomIn)

{
    const OGRErr eErr =
        OGRParquetLayerBase::ISetSpatialFilter(iGeomField, poGeomIn);
    m_bRebuildScanner = true;

    // Full invalidation
    InvalidateCachedBatches();
    return eErr;
}

/************************************************************************/
/*                        SetIgnoredFields()                            */
/************************************************************************/

OGRErr OGRParquetDatasetLayer::SetIgnoredFields(CSLConstList papszFields)
{
    m_bRebuildScanner = true;
    m_aosProjectedFields.clear();
    m_bIgnoredFields = false;
    m_anMapFieldIndexToArrayIndex.clear();
    m_anMapGeomFieldIndexToArrayIndex.clear();
    m_nRequestedFIDColumn = -1;
    OGRErr eErr = OGRParquetLayerBase::SetIgnoredFields(papszFields);
    if (eErr == OGRERR_NONE)
    {
        m_bIgnoredFields = papszFields != nullptr && papszFields[0] != nullptr;
        if (m_bIgnoredFields)
        {
            if (m_iFIDArrowColumn >= 0)
            {
                m_nRequestedFIDColumn =
                    static_cast<int>(m_aosProjectedFields.size());
                m_aosProjectedFields.emplace_back(GetFIDColumn());
            }

            const auto &fields = m_poSchema->fields();
            for (int i = 0; i < m_poFeatureDefn->GetFieldCount(); ++i)
            {
                const auto &field =
                    fields[m_anMapFieldIndexToArrowColumn[i][0]];
                const auto eArrowType = field->type()->id();
                if (eArrowType == arrow::Type::STRUCT)
                {
                    // For a struct, for the sake of simplicity in
                    // GetNextRawFeature(), as soon as one of the member if
                    // requested, request the struct field, so that the Arrow
                    // type doesn't change
                    bool bFoundNotIgnored = false;
                    for (int j = i; j < m_poFeatureDefn->GetFieldCount() &&
                                    m_anMapFieldIndexToArrowColumn[i][0] ==
                                        m_anMapFieldIndexToArrowColumn[j][0];
                         ++j)
                    {
                        if (!m_poFeatureDefn->GetFieldDefn(j)->IsIgnored())
                        {
                            bFoundNotIgnored = true;
                            break;
                        }
                    }
                    if (bFoundNotIgnored)
                    {
                        int j;
                        for (j = i; j < m_poFeatureDefn->GetFieldCount() &&
                                    m_anMapFieldIndexToArrowColumn[i][0] ==
                                        m_anMapFieldIndexToArrowColumn[j][0];
                             ++j)
                        {
                            if (!m_poFeatureDefn->GetFieldDefn(j)->IsIgnored())
                            {
                                m_anMapFieldIndexToArrayIndex.push_back(
                                    static_cast<int>(
                                        m_aosProjectedFields.size()));
                            }
                            else
                            {
                                m_anMapFieldIndexToArrayIndex.push_back(-1);
                            }
                        }
                        i = j - 1;

                        m_aosProjectedFields.emplace_back(field->name());
                    }
                    else
                    {
                        int j;
                        for (j = i; j < m_poFeatureDefn->GetFieldCount() &&
                                    m_anMapFieldIndexToArrowColumn[i][0] ==
                                        m_anMapFieldIndexToArrowColumn[j][0];
                             ++j)
                        {
                            m_anMapFieldIndexToArrayIndex.push_back(-1);
                        }
                        i = j - 1;
                    }
                }
                else if (!m_poFeatureDefn->GetFieldDefn(i)->IsIgnored())
                {
                    m_anMapFieldIndexToArrayIndex.push_back(
                        static_cast<int>(m_aosProjectedFields.size()));
                    m_aosProjectedFields.emplace_back(field->name());
                }
                else
                {
                    m_anMapFieldIndexToArrayIndex.push_back(-1);
                }
            }

            for (int i = 0; i < m_poFeatureDefn->GetGeomFieldCount(); ++i)
            {
                const auto &field =
                    fields[m_anMapGeomFieldIndexToArrowColumn[i]];
                if (!m_poFeatureDefn->GetGeomFieldDefn(i)->IsIgnored())
                {
                    m_anMapGeomFieldIndexToArrayIndex.push_back(
                        static_cast<int>(m_aosProjectedFields.size()));
                    m_aosProjectedFields.emplace_back(field->name());
                }
                else
                {
                    m_anMapGeomFieldIndexToArrayIndex.push_back(-1);
                }
            }
        }
    }

    m_nExpectedBatchColumns =
        m_bIgnoredFields ? static_cast<int>(m_aosProjectedFields.size()) : -1;

    // Full invalidation
    InvalidateCachedBatches();

    return eErr;
}

/************************************************************************/
/*                         TestCapability()                             */
/************************************************************************/

int OGRParquetDatasetLayer::TestCapability(const char *pszCap)
{
    if (EQUAL(pszCap, OLCIgnoreFields))
        return true;

    if (EQUAL(pszCap, OLCFastSpatialFilter))
    {
        if (m_iGeomFieldFilter >= 0 &&
            m_iGeomFieldFilter < static_cast<int>(m_aeGeomEncoding.size()) &&
            m_aeGeomEncoding[m_iGeomFieldFilter] ==
                OGRArrowGeomEncoding::GEOARROW_STRUCT_POINT)
        {
            return true;
        }
        // fallback to base method
    }

    return OGRParquetLayerBase::TestCapability(pszCap);
}

/***********************************************************************/
/*                         SetAttributeFilter()                        */
/***********************************************************************/

OGRErr OGRParquetDatasetLayer::SetAttributeFilter(const char *pszFilter)
{
    m_bRebuildScanner = true;
    return OGRParquetLayerBase::SetAttributeFilter(pszFilter);
}
