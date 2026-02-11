/******************************************************************************
 *
 * Project:  GDAL
 * Purpose:  gdal "materialize" pipeline step
 * Author:   Even Rouault <even dot rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2025, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include "gdalalg_materialize.h"
#include "gdal_utils.h"
#include "gdal_priv.h"
#include "ogrsf_frmts.h"

//! @cond Doxygen_Suppress

#ifndef _
#define _(x) (x)
#endif

/************************************************************************/
/*                   GDALMaterializeRasterAlgorithm()                   */
/************************************************************************/

GDALMaterializeRasterAlgorithm::GDALMaterializeRasterAlgorithm()
    : GDALMaterializeStepAlgorithm<GDALRasterPipelineStepAlgorithm,
                                   GDAL_OF_RASTER>(HELP_URL)
{
    AddRasterHiddenInputDatasetArg();

    AddOutputDatasetArg(&m_outputDataset, GDAL_OF_RASTER,
                        /* positionalAndRequired = */ false,
                        _("Materialized dataset name"))
        .SetDatasetInputFlags(GADV_NAME);

    AddOutputFormatArg(&m_format)
        .AddMetadataItem(GAAMDI_REQUIRED_CAPABILITIES,
                         {GDAL_DCAP_RASTER, GDAL_DCAP_CREATECOPY,
                          GDAL_DCAP_OPEN, GDAL_DMD_EXTENSIONS})
        .AddMetadataItem(GAAMDI_ALLOWED_FORMATS, {"MEM", "COG"})
        .AddMetadataItem(GAAMDI_EXCLUDED_FORMATS, {"VRT"});

    AddCreationOptionsArg(&m_creationOptions);
    AddOverwriteArg(&m_overwrite);
}

/************************************************************************/
/*              GDALMaterializeRasterAlgorithm::RunStep()               */
/************************************************************************/

bool GDALMaterializeRasterAlgorithm::RunStep(GDALPipelineStepRunContext &ctxt)
{
    auto pfnProgress = ctxt.m_pfnProgress;
    auto pProgressData = ctxt.m_pProgressData;

    auto poSrcDS = m_inputDataset[0].GetDatasetRef();
    CPLAssert(poSrcDS);
    CPLAssert(!m_outputDataset.GetDatasetRef());

    if (m_format.empty())
        m_format = "GTiff";

    auto poDrv = GetGDALDriverManager()->GetDriverByName(m_format.c_str());
    if (!poDrv)
    {
        ReportError(CE_Failure, CPLE_AppDefined, "Driver %s does not exist",
                    m_format.c_str());
        return false;
    }

    std::string filename = m_outputDataset.GetName();
    const bool autoDeleteFile =
        filename.empty() && !EQUAL(m_format.c_str(), "MEM");
    if (autoDeleteFile)
    {
        filename = CPLGenerateTempFilenameSafe(nullptr);

        const char *pszExt = poDrv->GetMetadataItem(GDAL_DMD_EXTENSIONS);
        if (pszExt)
        {
            filename += '.';
            filename += CPLStringList(CSLTokenizeString(pszExt))[0];
        }
    }

    CPLStringList aosOptions(m_creationOptions);
    if (EQUAL(m_format.c_str(), "GTiff"))
    {
        if (aosOptions.FetchNameValue("TILED") == nullptr)
        {
            aosOptions.SetNameValue("TILED", "YES");
        }
        if (aosOptions.FetchNameValue("COPY_SRC_OVERVIEWS") == nullptr)
        {
            aosOptions.SetNameValue("COPY_SRC_OVERVIEWS", "YES");
        }
        if (aosOptions.FetchNameValue("COMPRESS") == nullptr)
        {
            const char *pszCOList =
                poDrv->GetMetadataItem(GDAL_DMD_CREATIONOPTIONLIST);
            aosOptions.SetNameValue(
                "COMPRESS",
                pszCOList && strstr(pszCOList, "ZSTD") ? "ZSTD" : "DEFLATE");
        }
    }

    if (autoDeleteFile)
    {
        aosOptions.SetNameValue("@SUPPRESS_ASAP", "YES");
    }

    auto poOutDS = std::unique_ptr<GDALDataset>(
        poDrv->CreateCopy(filename.c_str(), poSrcDS, false, aosOptions.List(),
                          pfnProgress, pProgressData));
    bool ok = poOutDS != nullptr && poOutDS->FlushCache() == CE_None;
    if (poOutDS)
    {
        if (poDrv->GetMetadataItem(GDAL_DCAP_REOPEN_AFTER_WRITE_REQUIRED))
        {
            ok = poOutDS->Close() == CE_None;
            poOutDS.reset();
            if (ok)
            {
                const char *const apszAllowedDrivers[] = {m_format.c_str(),
                                                          nullptr};
                poOutDS.reset(GDALDataset::Open(
                    filename.c_str(), GDAL_OF_RASTER | GDAL_OF_VERBOSE_ERROR,
                    apszAllowedDrivers));
                ok = poOutDS != nullptr;
            }
        }
        if (ok)
        {
            if (autoDeleteFile)
            {
#if !defined(_WIN32)
                if (poDrv->GetMetadataItem(GDAL_DCAP_CAN_READ_AFTER_DELETE))
                {
                    CPLErrorStateBackuper oBackuper(CPLQuietErrorHandler);
                    poDrv->Delete(poOutDS.get(),
                                  CPLStringList(poOutDS->GetFileList()).List());
                }
#endif
                poOutDS->MarkSuppressOnClose();
            }

            m_outputDataset.Set(std::move(poOutDS));
        }
    }
    return ok;
}

/************************************************************************/
/*                   GDALMaterializeVectorAlgorithm()                   */
/************************************************************************/

GDALMaterializeVectorAlgorithm::GDALMaterializeVectorAlgorithm()
    : GDALMaterializeStepAlgorithm<GDALVectorPipelineStepAlgorithm,
                                   GDAL_OF_VECTOR>(HELP_URL)
{
    AddVectorHiddenInputDatasetArg();

    AddOutputDatasetArg(&m_outputDataset, GDAL_OF_VECTOR,
                        /* positionalAndRequired = */ false,
                        _("Materialized dataset name"))
        .SetDatasetInputFlags(GADV_NAME);

    AddOutputFormatArg(&m_format)
        .AddMetadataItem(GAAMDI_REQUIRED_CAPABILITIES,
                         {GDAL_DCAP_VECTOR, GDAL_DCAP_CREATE, GDAL_DCAP_OPEN,
                          GDAL_DMD_EXTENSIONS})
        .AddMetadataItem(GAAMDI_ALLOWED_FORMATS, {"MEM"})
        .AddMetadataItem(GAAMDI_EXCLUDED_FORMATS,
                         {"MBTiles", "MVT", "PMTiles", "JP2ECW"});

    AddCreationOptionsArg(&m_creationOptions);
    AddLayerCreationOptionsArg(&m_layerCreationOptions);
    AddOverwriteArg(&m_overwrite);
}

/************************************************************************/
/*              GDALMaterializeVectorAlgorithm::RunStep()               */
/************************************************************************/

bool GDALMaterializeVectorAlgorithm::RunStep(GDALPipelineStepRunContext &ctxt)
{
    auto pfnProgress = ctxt.m_pfnProgress;
    auto pProgressData = ctxt.m_pProgressData;

    auto poSrcDS = m_inputDataset[0].GetDatasetRef();
    CPLAssert(poSrcDS);
    CPLAssert(!m_outputDataset.GetDatasetRef());

    if (m_format.empty())
    {
        bool bSeveralGeomFields = false;
        for (const auto *poLayer : poSrcDS->GetLayers())
        {
            if (!bSeveralGeomFields)
                bSeveralGeomFields =
                    poLayer->GetLayerDefn()->GetGeomFieldCount() > 1;
            if (!bSeveralGeomFields &&
                poLayer->GetLayerDefn()->GetGeomFieldCount() > 0)
            {
                for (const auto *poFieldDefn :
                     poLayer->GetLayerDefn()->GetFields())
                {
                    const auto eType = poFieldDefn->GetType();
                    if (eType == OFTStringList || eType == OFTIntegerList ||
                        eType == OFTRealList || eType == OFTInteger64List)
                    {
                        bSeveralGeomFields = true;
                    }
                }
            }
        }
        m_format = bSeveralGeomFields ? "SQLite" : "GPKG";
    }

    auto poDrv = GetGDALDriverManager()->GetDriverByName(m_format.c_str());
    if (!poDrv)
    {
        ReportError(CE_Failure, CPLE_AppDefined, "Driver %s does not exist",
                    m_format.c_str());
        return false;
    }

    std::string filename = m_outputDataset.GetName();
    const bool autoDeleteFile =
        filename.empty() && !EQUAL(m_format.c_str(), "MEM");
    if (autoDeleteFile)
    {
        filename = CPLGenerateTempFilenameSafe(nullptr);

        const char *pszExt = poDrv->GetMetadataItem(GDAL_DMD_EXTENSIONS);
        if (pszExt)
        {
            filename += '.';
            filename += CPLStringList(CSLTokenizeString(pszExt))[0];
        }
    }

    CPLStringList aosOptions;
    aosOptions.AddString("--invoked-from-gdal-algorithm");
    if (!m_overwrite)
    {
        aosOptions.AddString("--no-overwrite");
    }

    aosOptions.AddString("-of");
    aosOptions.AddString(m_format.c_str());
    for (const auto &co : m_creationOptions)
    {
        aosOptions.AddString("-dsco");
        aosOptions.AddString(co.c_str());
    }
    if (EQUAL(m_format.c_str(), "SQLite"))
    {
        const char *pszCOList =
            poDrv->GetMetadataItem(GDAL_DMD_CREATIONOPTIONLIST);
        if (pszCOList && strstr(pszCOList, "SPATIALITE") &&
            CPLStringList(m_creationOptions).FetchNameValue("SPATIALITE") ==
                nullptr)
        {
            aosOptions.AddString("-dsco");
            aosOptions.AddString("SPATIALITE=YES");
        }
    }
    for (const auto &co : m_layerCreationOptions)
    {
        aosOptions.AddString("-lco");
        aosOptions.AddString(co.c_str());
    }
    if (pfnProgress && pfnProgress != GDALDummyProgress)
    {
        aosOptions.AddString("-progress");
    }

    if (autoDeleteFile)
    {
        aosOptions.AddString("-dsco");
        aosOptions.AddString("@SUPPRESS_ASAP=YES");
    }

    GDALVectorTranslateOptions *psOptions =
        GDALVectorTranslateOptionsNew(aosOptions.List(), nullptr);
    GDALVectorTranslateOptionsSetProgress(psOptions, pfnProgress,
                                          pProgressData);

    GDALDatasetH hSrcDS = GDALDataset::ToHandle(poSrcDS);
    auto poOutDS = std::unique_ptr<GDALDataset>(
        GDALDataset::FromHandle(GDALVectorTranslate(
            filename.c_str(), nullptr, 1, &hSrcDS, psOptions, nullptr)));
    GDALVectorTranslateOptionsFree(psOptions);

    bool ok = poOutDS != nullptr && poOutDS->FlushCache() == CE_None;
    if (poOutDS)
    {
        if (poDrv->GetMetadataItem(GDAL_DCAP_REOPEN_AFTER_WRITE_REQUIRED))
        {
            ok = poOutDS->Close() == CE_None;
            poOutDS.reset();
            if (ok)
            {
                const char *const apszAllowedDrivers[] = {m_format.c_str(),
                                                          nullptr};
                poOutDS.reset(GDALDataset::Open(
                    filename.c_str(), GDAL_OF_VECTOR | GDAL_OF_VERBOSE_ERROR,
                    apszAllowedDrivers));
                ok = poOutDS != nullptr;
            }
        }
        if (ok)
        {
            if (autoDeleteFile)
            {
#if !defined(_WIN32)
                if (poDrv->GetMetadataItem(GDAL_DCAP_CAN_READ_AFTER_DELETE))
                {
                    CPLErrorStateBackuper oBackuper(CPLQuietErrorHandler);
                    poDrv->Delete(poOutDS.get(),
                                  CPLStringList(poOutDS->GetFileList()).List());
                }
#endif
                poOutDS->MarkSuppressOnClose();
            }

            m_outputDataset.Set(std::move(poOutDS));
        }
    }
    return ok;
}

//! @endcond
