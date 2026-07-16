/******************************************************************************
 *
 * Project:  GDAL
 * Purpose:  "write" step of "raster pipeline"
 * Author:   Even Rouault <even dot rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2024, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include "gdalalg_raster_write.h"

#include "cpl_string.h"
#include "gdal_utils.h"
#include "gdal_priv.h"

//! @cond Doxygen_Suppress

/************************************************************************/
/*         GDALRasterWriteAlgorithm::GDALRasterWriteAlgorithm()         */
/************************************************************************/

GDALRasterWriteAlgorithm::GDALRasterWriteAlgorithm()
    : GDALRasterPipelineStepAlgorithm(NAME, DESCRIPTION, HELP_URL,
                                      /* standaloneStep =*/false)
{
    AddRasterOutputArgs(/* hiddenForCLI = */ false);
}

/************************************************************************/
/*                 GDALRasterWriteAlgorithm::RunStep()                  */
/************************************************************************/

bool GDALRasterWriteAlgorithm::RunStep(GDALPipelineStepRunContext &ctxt)
{
    auto pfnProgress = ctxt.m_pfnProgress;
    auto pProgressData = ctxt.m_pProgressData;
    auto poSrcDS = m_inputDataset[0].GetDatasetRef();
    CPLAssert(poSrcDS);
    CPLAssert(!m_outputDataset.GetDatasetRef());

    if (m_format == "stream")
    {
        m_outputDataset.Set(poSrcDS);
        return true;
    }

    CPLStringList aosOptions;
    if (!m_overwrite)
    {
        aosOptions.AddString("--no-overwrite");
    }
    if (m_appendRaster)
    {
        aosOptions.AddString("-co");
        aosOptions.AddString("APPEND_SUBDATASET=YES");
    }
    if (!m_format.empty())
    {
        aosOptions.AddString("-of");
        aosOptions.AddString(m_format.c_str());
    }
    for (const auto &co : m_creationOptions)
    {
        aosOptions.AddString("-co");
        aosOptions.AddString(co.c_str());
    }

    GDALTranslateOptions *psOptions =
        GDALTranslateOptionsNew(aosOptions.List(), nullptr);
    GDALTranslateOptionsSetProgress(psOptions, pfnProgress, pProgressData);

    // Backup error state since GDALTranslate() resets it multiple times
    const auto nLastErrorNum = CPLGetLastErrorNo();
    const auto nLastErrorType = CPLGetLastErrorType();
    const std::string osLastErrorMsg = CPLGetLastErrorMsg();
    const auto nLastErrorCounter = CPLGetErrorCounter();

    GDALDatasetH hSrcDS = GDALDataset::ToHandle(poSrcDS);
    auto poRetDS = GDALDataset::FromHandle(GDALTranslate(
        m_outputDataset.GetName().c_str(), hSrcDS, psOptions, nullptr));
    GDALTranslateOptionsFree(psOptions);

    if (nLastErrorCounter > 0 && CPLGetErrorCounter() == 0)
    {
        CPLErrorSetState(nLastErrorType, nLastErrorNum, osLastErrorMsg.c_str(),
                         &nLastErrorCounter);
    }

    if (!poRetDS)
        return false;

    m_outputDataset.Set(std::unique_ptr<GDALDataset>(poRetDS));

    return true;
}

//! @endcond
