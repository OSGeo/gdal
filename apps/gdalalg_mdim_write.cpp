/******************************************************************************
 *
 * Project:  GDAL
 * Purpose:  "write" step of "mdim pipeline"
 * Author:   Even Rouault <even dot rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2026, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include "gdalalg_mdim_write.h"

#include "cpl_string.h"
#include "gdal_utils.h"
#include "gdal_priv.h"

//! @cond Doxygen_Suppress

/************************************************************************/
/*           GDALMdimWriteAlgorithm::GDALMdimWriteAlgorithm()           */
/************************************************************************/

GDALMdimWriteAlgorithm::GDALMdimWriteAlgorithm()
    : GDALMdimPipelineStepAlgorithm(NAME, DESCRIPTION, HELP_URL,
                                    ConstructorOptions())
{
    AddMdimOutputArgs(/* hiddenForCLI = */ false);
}

/************************************************************************/
/*                  GDALMdimWriteAlgorithm::RunStep()                   */
/************************************************************************/

bool GDALMdimWriteAlgorithm::RunStep(GDALPipelineStepRunContext &ctxt)
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

    GDALMultiDimTranslateOptions *psOptions =
        GDALMultiDimTranslateOptionsNew(aosOptions.List(), nullptr);
    GDALMultiDimTranslateOptionsSetProgress(psOptions, pfnProgress,
                                            pProgressData);

    // Backup error state since GDALMultiDimTranslate() resets it multiple times
    const auto nLastErrorNum = CPLGetLastErrorNo();
    const auto nLastErrorType = CPLGetLastErrorType();
    const std::string osLastErrorMsg = CPLGetLastErrorMsg();
    const auto nLastErrorCounter = CPLGetErrorCounter();

    GDALDatasetH hSrcDS = GDALDataset::ToHandle(poSrcDS);
    auto poRetDS = GDALDataset::FromHandle(
        GDALMultiDimTranslate(m_outputDataset.GetName().c_str(), nullptr, 1,
                              &hSrcDS, psOptions, nullptr));
    GDALMultiDimTranslateOptionsFree(psOptions);

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
