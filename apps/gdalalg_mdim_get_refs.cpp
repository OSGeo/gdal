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
#include "cpl_string.h"
#include "gdal_priv.h"
#include "ogrsf_frmts.h"
#include "gdalalg_mdim_get_refs_common.h"
#include <inttypes.h>

//! @cond Doxygen_Suppress

#ifndef _
#define _(x) (x)
#endif

/************************************************************************/
/*                      GDALMdimGetRefsAlgorithm()                      */
/************************************************************************/

GDALMdimGetRefsAlgorithm::GDALMdimGetRefsAlgorithm()
    : GDALAlgorithm(NAME, DESCRIPTION, HELP_URL)
{
    AddProgressArg();
    AddOutputFormatArg(&m_outputFormat, /* bStreamAllowed = */ false,
                       /* bGDALGAllowed = */ false)
        .AddMetadataItem(GAAMDI_REQUIRED_CAPABILITIES,
                         {GDAL_DCAP_VECTOR, GDAL_DCAP_CREATE});
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
    auto poSrcDS = m_inputDataset.GetDatasetRef();
    CPLAssert(poSrcDS);

    auto poRootGroup = poSrcDS->GetRootGroup();
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

    const std::vector<std::shared_ptr<GDALDimension>> &apoDims =
        poArray->GetDimensions();

    const auto anBlockSize = poArray->GetBlockSize();
    std::vector<uint64_t> anBlockSizeU64(anBlockSize.begin(),
                                         anBlockSize.end());
    CPLAssert(anBlockSize.size() == poArray->GetDimensionCount());
    for (size_t i = 0; i < anBlockSize.size(); ++i)
    {
        if (anBlockSize[i] == 0)
        {
            ReportError(
                CE_Failure, CPLE_AppDefined,
                "Array %s has no natural block size on dimension %" PRIu64 ";"
                "not chunk-enumerable",
                m_array.c_str(), static_cast<uint64_t>(i));
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

    std::vector<size_t> n_chunks;
    const uint64_t nTotalChunks =
        get_refs::ComputeChunkGrid(anDimSize, anBlockSizeU64, n_chunks);

    const std::string osOutputPath = m_outputDataset.GetName();

    if (m_outputFormat.empty())
    {
        const auto aosFormats =
            CPLStringList(GDALGetOutputDriversForDatasetName(
                m_outputDataset.GetName().c_str(), GDAL_OF_VECTOR,
                /* bSingleMatch = */ true,
                /* bWarn = */ true));
        if (aosFormats.size() != 1)
        {
            ReportError(CE_Failure, CPLE_AppDefined,
                        "Cannot guess driver for %s",
                        m_outputDataset.GetName().c_str());
            return false;
        }
        m_outputFormat = aosFormats[0];
    }

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
    // Per-dimension fields: dim_0, dim_1, ... with generic names
    // for row-filter, rather than an encoded position "0.0.0.."
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

    OGRFieldDefn oPresentField("present", OFTInteger);
    oPresentField.SetSubType(OFSTBoolean);
    if (poLayer->CreateField(&oPresentField) != OGRERR_NONE)
    {
        ReportError(CE_Failure, CPLE_AppDefined,
                    "Cannot create field 'present'");
        return false;
    }

    OGRFieldDefn oPathField("path", OFTString);

    if (poLayer->CreateField(&oPathField) != OGRERR_NONE)
    {
        ReportError(CE_Failure, CPLE_AppDefined, "Cannot create field 'path'");
        return false;
    }

    OGRFieldDefn oOffsetField("offset", OFTInteger64);

    if (poLayer->CreateField(&oOffsetField) != OGRERR_NONE)
    {
        ReportError(CE_Failure, CPLE_AppDefined,
                    "Cannot create field 'offset'");
        return false;
    }

    OGRFieldDefn oSizeField("size", OFTInteger64);
    if (poLayer->CreateField(&oSizeField) != OGRERR_NONE)
    {
        ReportError(CE_Failure, CPLE_AppDefined, "Cannot create field 'size'");
        return false;
    }

    OGRFieldDefn oInfoField("info", OFTString);
    if (poLayer->CreateField(&oInfoField) != OGRERR_NONE)
    {
        ReportError(CE_Failure, CPLE_AppDefined, "Cannot create field 'info'");
        return false;
    }

    poLayer->SetMetadataItem("ARRAY_NAME", m_array.c_str());
    poLayer->SetMetadataItem("DATA_TYPE", dt_name);
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

    std::vector<uint64_t> coords(apoDims.size());
    GDALMDArrayRawBlockInfo info;  // reused, .clear() per iteration

    // Loop over chunks
    const size_t nProgressInterval = std::max<size_t>(1, nTotalChunks / 100);
    bool bCodecToLayerMetadata = false;
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
        OGRFeature oFeature(poLayer->GetLayerDefn());
        // Per-dim coordinates: dim_0 .. dim_{n-1}
        for (size_t i = 0; i < coords.size(); ++i)
            oFeature.SetField(static_cast<int>(i),
                              static_cast<GIntBig>(coords[i]));
        const int iPresentField = static_cast<int>(coords.size());
        const int iPathField = iPresentField + 1;
        const int iOffsetField = iPresentField + 2;
        const int iSizeField = iPresentField + 3;
        const int iInfoField = iPresentField + 4;

        // Three-state classification
        if (info.pszFilename != nullptr)
        {
            // present (file-backed)
            oFeature.SetField(iPresentField, 1);
            oFeature.SetField(iPathField, info.pszFilename);
            oFeature.SetField(iOffsetField, static_cast<GIntBig>(info.nOffset));
            oFeature.SetField(iSizeField, static_cast<GIntBig>(info.nSize));
        }
        else if (info.pabyInlineData != nullptr)
        {
            // path and offset left null, and we don't yet inline data even though it's available
            oFeature.SetField(iPresentField, 1);
            oFeature.SetField(iSizeField, static_cast<GIntBig>(info.nSize));
        }
        else
        {
            // absent (sparse)
            oFeature.SetField(iPresentField, 0);
        }
        if (info.papszInfo != nullptr)
        {
            std::string osJoined;
            for (int i = 0; info.papszInfo[i] != nullptr; ++i)
            {
                if (i > 0)
                    osJoined += "; ";
                osJoined += info.papszInfo[i];
            }
            oFeature.SetField(iInfoField, osJoined.c_str());
        }

        // Codec layer metadata on the first successful file-backed chunk.
        if (!bCodecToLayerMetadata && info.papszInfo != nullptr &&
            info.pszFilename != nullptr)
        {
            const CPLStringList aosInfo(info.papszInfo, /* bAssign = */ false);
            for (const auto &[pszKey, pszValue] :
                 cpl::IterateNameValue(aosInfo))
            {
                poLayer->SetMetadataItem(
                    CPLString().Printf("CODEC_%s", pszKey).c_str(), pszValue);
            }
            bCodecToLayerMetadata = true;
        }

        if (poLayer->CreateFeature(&oFeature) != OGRERR_NONE)
        {
            ReportError(CE_Failure, CPLE_AppDefined,
                        "Cannot write feature for chunk %zu", iLinear);
            return false;
        }

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
