/******************************************************************************
 *
 * Project:  GDAL
 * Purpose:  gdal "mdim get-refs" subcommand
 * Author:   Michael Sumner <mdsumner at gmail.com>
 *
 ******************************************************************************
 * Copyright (c) 2026, Michael Sumner <mdsumner at gmail.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include "gdalalg_mdim_get_refs.h"
#include "cpl_conv.h"
#include "gdal_priv.h"
#include "ogrsf_frmts.h"
#include "gdalalg_mdim_get_refs_common.h"

//! @cond Doxygen_Suppress

#ifndef _
#define _(x) (x)
#endif

namespace
{

// Format a vector of integers as "[a, b, c]" for debug messages.
std::string FormatVec(const std::vector<uint64_t> &v)
{
    std::string s = "[";
    for (size_t i = 0; i < v.size(); ++i)
    {
        if (i > 0)
            s += ", ";
        s += std::to_string(v[i]);
    }
    s += "]";
    return s;
}

}  // namespace

/************************************************************************/
/*                        GDALMdimGetRefsAlgorithm()                  */
/************************************************************************/

GDALMdimGetRefsAlgorithm::GDALMdimGetRefsAlgorithm()
    : GDALAlgorithm(NAME, DESCRIPTION, HELP_URL)
{
    AddProgressArg();
    AddOutputFormatArg(&m_outputFormat, /* bStreamAllowed = */ false,
                       /* bGDALGAllowed = */ false)
        .AddMetadataItem(GAAMDI_REQUIRED_CAPABILITIES,
                         {GDAL_DCAP_VECTOR, GDAL_DCAP_CREATE})
        .SetRequired();
    AddOpenOptionsArg(&m_openOptions);
    AddInputFormatsArg(&m_inputFormats)
        .AddMetadataItem(GAAMDI_REQUIRED_CAPABILITIES,
                         {GDAL_DCAP_MULTIDIM_RASTER});
    AddInputDatasetArg(&m_inputDataset, GDAL_OF_MULTIDIM_RASTER)
        .AddAlias("dataset");
    AddOutputDatasetArg(&m_outputDataset, GDAL_OF_VECTOR)
        .SetDatasetInputFlags(GADV_NAME | GADV_OBJECT);
    AddArrayNameArg(&m_array, _("Name of the array, used to restrict the "
                                "output to the specified array."))
        .SetRequired();
    AddOverwriteArg(&m_overwrite);
    AddCreationOptionsArg(&m_creationOptions);
}

bool GDALMdimGetRefsAlgorithm::RunImpl(GDALProgressFunc pfnProgress,
                                       void *pProgressData)
{
    // ----------------------------------------------------------------------
    // STAGE A — resolve the input array
    // ----------------------------------------------------------------------
    // A1. Get the input GDALDataset from m_inputDataset (already opened by the
    //     framework as OF_MULTIDIM_RASTER — confirm footprint/convert rely on
    //     the framework open rather than re-opening). GetDatasetRef().

    auto poSrcDS = m_inputDataset.GetDatasetRef();
    CPLAssert(poSrcDS);

    // A2. GetRootGroup(). Null root => driver lacks mdim support => fail with a
    //     clear CPLError, return false.
    auto poRootGroup = poSrcDS->GetRootGroup();
    CPLDebug("MDIM-GET-REFS", "input: %s, root group: %s",
             poSrcDS->GetDescription(), poRootGroup ? "present" : "NULL");

    if (!poRootGroup)
    {
        ReportError(CE_Failure, CPLE_AppDefined,
                    "Dataset %s has no root group (not multidimensional?)",
                    poSrcDS->GetDescription());
        return false;
    }

    auto poArray = poRootGroup->OpenMDArrayFromFullname(m_array);
    if (!poArray)
    {
        ReportError(CE_Failure, CPLE_AppDefined,
                    "Cannot find array %s in dataset %s. \n"
                    "Use 'gdal mdim info %s' to list available arrays.",
                    m_array.c_str(), poSrcDS->GetDescription(),
                    poSrcDS->GetDescription());
        return false;
    }

    const std::vector<std::shared_ptr<GDALDimension>> apoDims =
        poArray->GetDimensions();

    const auto anBlockSize = poArray->GetBlockSize();
    std::vector<uint64_t> anBlockSizeU64(anBlockSize.begin(),
                                         anBlockSize.end());
    CPLAssert(anBlockSize.size() == poArray->GetDimensionCount());
    for (size_t i = 0; i < anBlockSizeU64.size(); ++i)
    {
        if (anBlockSize[i] == 0)
        {
            ReportError(CE_Failure, CPLE_AppDefined,
                        "Array %s has no natural block size on dimension %zu; "
                        "not chunk-enumerable",
                        m_array.c_str(), i);
            return false;
        }
    }

    const auto &dt = poArray->GetDataType();
    GDALDataType nDataType = dt.GetNumericDataType();
    if (nDataType == GDT_Unknown)
    {
        ReportError(
            CE_Failure, CPLE_AppDefined,
            "Array %s has non-numeric or unknown data type; not supported",
            m_array.c_str());
        return false;
    }
    const char *dt_name = GDALGetDataTypeName(nDataType);

    // Build the dim-size vector (needed by ComputeChunkGrid and for debug)
    std::vector<uint64_t> anDimSize(apoDims.size());
    for (size_t i = 0; i < apoDims.size(); ++i)
        anDimSize[i] = apoDims[i]->GetSize();

    // Stage C: chunk grid via the helper
    std::vector<size_t> n_chunks;
    const size_t nTotalChunks =
        get_refs::ComputeChunkGrid(anDimSize, anBlockSizeU64, n_chunks);

    CPLDebug(
        "MDIM-GET-REFS",
        "array %s: dims=[%s], blocks=[%s], chunks=[%s], total=%zu, dtype=%s",
        m_array.c_str(), FormatVec(anDimSize).c_str(),
        FormatVec(anBlockSizeU64).c_str(), FormatVec(n_chunks).c_str(),
        nTotalChunks, dt_name);

    const std::string osOutputPath = m_outputDataset.GetName();

    GDALDriver *poDriver =
        GetGDALDriverManager()->GetDriverByName(m_outputFormat.c_str());
    if (!poDriver)
    {
        ReportError(CE_Failure, CPLE_AppDefined,
                    "Cannot find vector driver '%s' for output dataset. "
                    "Use 'gdal --formats' to list available drivers.",
                    m_outputFormat.c_str());
        return false;
    }

    auto poDstDS = std::unique_ptr<GDALDataset>(
        poDriver->Create(osOutputPath.c_str(), 0, 0, 0, GDT_Unknown,
                         CPLStringList(m_creationOptions).List()));
    if (!poDstDS)
    {
        return false;
    }

    std::string osLayerName = m_array;
    const auto nLastSlash = osLayerName.rfind('/');
    if (nLastSlash != std::string::npos)
        osLayerName = osLayerName.substr(nLastSlash + 1);

    OGRLayer *poLayer =
        poDstDS->CreateLayer(osLayerName.c_str(), nullptr, wkbNone, nullptr);
    if (!poLayer)
    {
        ReportError(CE_Failure, CPLE_AppDefined,
                    "Cannot create layer '%s' in output dataset '%s'",
                    osLayerName.c_str(), osOutputPath.c_str());
        return false;
    }

    // D6. Per-dimension fields: dim_0, dim_1, ... as OFTInteger64
    //     (names live in layer metadata, not field names — keeps the schema
    //     identical-shape across arrays, sidesteps sanitization).
    for (size_t i = 0; i < apoDims.size(); ++i)
    {
        OGRFieldDefn oField(CPLSPrintf("dim_%zu", i), OFTInteger64);
        if (poLayer->CreateField(&oField) != OGRERR_NONE)
        {
            ReportError(CE_Failure, CPLE_AppDefined,
                        "Cannot create field 'dim_%zu'", i);
            return false;
        }
    }

    // D7. Generic fields per RFC Stage 1 schema.
    OGRFieldDefn oPresentField("present", OFTInteger);
    oPresentField.SetSubType(OFSTBoolean);
    if (poLayer->CreateField(&oPresentField) != OGRERR_NONE)
    {
        ReportError(CE_Failure, CPLE_AppDefined,
                    "Cannot create field 'present'");
        return false;
    }

    OGRFieldDefn oPathField("path", OFTString);
    oPathField.SetNullable(TRUE);
    if (poLayer->CreateField(&oPathField) != OGRERR_NONE)
    {
        ReportError(CE_Failure, CPLE_AppDefined, "Cannot create field 'path'");
        return false;
    }

    OGRFieldDefn oOffsetField("offset", OFTInteger64);
    oOffsetField.SetNullable(TRUE);
    if (poLayer->CreateField(&oOffsetField) != OGRERR_NONE)
    {
        ReportError(CE_Failure, CPLE_AppDefined,
                    "Cannot create field 'offset'");
        return false;
    }

    OGRFieldDefn oSizeField("size", OFTInteger64);
    oSizeField.SetNullable(TRUE);
    if (poLayer->CreateField(&oSizeField) != OGRERR_NONE)
    {
        ReportError(CE_Failure, CPLE_AppDefined, "Cannot create field 'size'");
        return false;
    }

    OGRFieldDefn oInfoField("info", OFTString);
    oInfoField.SetNullable(TRUE);
    if (poLayer->CreateField(&oInfoField) != OGRERR_NONE)
    {
        ReportError(CE_Failure, CPLE_AppDefined, "Cannot create field 'info'");
        return false;
    }

    // D8. Array-level metadata on the layer.
    poLayer->SetMetadataItem("ARRAY_NAME", m_array.c_str());
    poLayer->SetMetadataItem("DTYPE", dt_name);
    for (size_t i = 0; i < apoDims.size(); ++i)
    {
        poLayer->SetMetadataItem(CPLSPrintf("DIM_%zu_NAME", i),
                                 apoDims[i]->GetName().c_str());
        poLayer->SetMetadataItem(
            CPLSPrintf("DIM_%zu_SIZE", i),
            CPLSPrintf(CPL_FRMT_GUIB,
                       static_cast<GUIntBig>(apoDims[i]->GetSize())));
        poLayer->SetMetadataItem(
            CPLSPrintf("DIM_%zu_BLOCK", i),
            CPLSPrintf(CPL_FRMT_GUIB, static_cast<GUIntBig>(anBlockSize[i])));
        poLayer->SetMetadataItem(CPLSPrintf("DIM_%zu_CHUNKS", i),
                                 CPLSPrintf("%zu", n_chunks[i]));
    }

    CPLDebug("MDIM-GET-REFS",
             "created layer '%s' with %d fields, ready for %zu features",
             osLayerName.c_str(), poLayer->GetLayerDefn()->GetFieldCount(),
             nTotalChunks);

    std::vector<uint64_t> coords(
        apoDims.size());           // reused, inside LinearToCoords
    GDALMDArrayRawBlockInfo info;  // reused, .clear() per iteration

    // Loop over chunks
    const size_t nProgressInterval = std::max<size_t>(1, nTotalChunks / 100);
    bool bCodecHoisted = false;
    for (size_t iLinear = 0; iLinear < nTotalChunks; ++iLinear)
    {
        info.clear();
        get_refs::LinearToCoords(iLinear, n_chunks, coords);
        if (!poArray->GetRawBlockInfo(coords.data(), info))
        {
            ReportError(CE_Failure, CPLE_AppDefined,
                        "GetRawBlockInfo failed at linear index %zu", iLinear);
            return false;
        }
        if (iLinear < 3 || iLinear == nTotalChunks - 1)
        {
            CPLDebug("MDIM-GET-REFS",
                     "chunk %zu coords=[%s] path=%s offset=" CPL_FRMT_GUIB
                     " size=" CPL_FRMT_GUIB,
                     iLinear, FormatVec(coords).c_str(),
                     info.pszFilename ? info.pszFilename : "(null)",
                     static_cast<GUIntBig>(info.nOffset),
                     static_cast<GUIntBig>(info.nSize));
        }
        OGRFeature *poFeature =
            OGRFeature::CreateFeature(poLayer->GetLayerDefn());

        // Per-dim coordinates: dim_0 .. dim_{n-1}
        for (size_t i = 0; i < coords.size(); ++i)
            poFeature->SetField(static_cast<int>(i),
                                static_cast<GIntBig>(coords[i]));

        // Three-state classification — exactly the Commit 1 reconciliation
        const int iPresentField = static_cast<int>(coords.size());
        const int iPathField = iPresentField + 1;
        const int iOffsetField = iPresentField + 2;
        const int iSizeField = iPresentField + 3;
        const int iInfoField = iPresentField + 4;

        if (info.pszFilename != nullptr)
        {
            // present (file-backed)
            poFeature->SetField(iPresentField, 1);
            poFeature->SetField(iPathField, info.pszFilename);
            poFeature->SetField(iOffsetField,
                                static_cast<GIntBig>(info.nOffset));
            poFeature->SetField(iSizeField, static_cast<GIntBig>(info.nSize));
        }
        else if (info.pabyInlineData != nullptr)
        {
            // inline — Stage 1 reports size but not bytes
            // (note: classify by pabyInlineData, not nSize > 0 — Commit 1)
            poFeature->SetField(iPresentField, 1);
            // path and offset left null (default state)
            poFeature->SetField(iSizeField, static_cast<GIntBig>(info.nSize));
        }
        else
        {
            // absent (sparse)
            poFeature->SetField(iPresentField, 0);
            // path, offset, size all left null
        }

        // info (papszInfo joined) — applies to all three states when non-null
        if (info.papszInfo != nullptr)
        {
            CPLString osJoined;
            for (int i = 0; info.papszInfo[i] != nullptr; ++i)
            {
                if (i > 0)
                    osJoined += "; ";
                osJoined += info.papszInfo[i];
            }
            poFeature->SetField(iInfoField, osJoined.c_str());
        }

        // Codec hoist to layer metadata on the first successful file-backed chunk.
        if (!bCodecHoisted && info.papszInfo != nullptr &&
            info.pszFilename != nullptr)
        {
            for (int i = 0; info.papszInfo[i] != nullptr; ++i)
            {
                char *pszKey = nullptr;
                const char *pszValue =
                    CPLParseNameValue(info.papszInfo[i], &pszKey);
                if (pszKey && pszValue)
                    poLayer->SetMetadataItem(
                        CPLString().Printf("CODEC_%s", pszKey).c_str(),
                        pszValue);
                CPLFree(pszKey);
            }
            bCodecHoisted = true;
        }

        if (poLayer->CreateFeature(poFeature) != OGRERR_NONE)
        {
            ReportError(CE_Failure, CPLE_AppDefined,
                        "Cannot write feature for chunk %zu", iLinear);
            OGRFeature::DestroyFeature(poFeature);
            return false;
        }
        OGRFeature::DestroyFeature(poFeature);

        // progress
        if (pfnProgress && (iLinear % nProgressInterval == 0))
        {
            const double dfFraction = static_cast<double>(iLinear) /
                                      static_cast<double>(nTotalChunks);
            if (!pfnProgress(dfFraction, nullptr, pProgressData))
            {
                ReportError(CE_Failure, CPLE_UserInterrupt,
                            "User interrupted at chunk %zu of %zu", iLinear,
                            nTotalChunks);
                return false;
            }
        }
    }
    // Final progress tick
    if (pfnProgress)
        pfnProgress(1.0, nullptr, pProgressData);

    m_outputDataset.Set(std::move(poDstDS));

    return true;
}

//! @cond Doxygen_Suppress
