/******************************************************************************
 *
 * Project:  GDAL
 * Purpose:  "write" step of "vector pipeline"
 * Author:   Even Rouault <even dot rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2024, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include "gdalalg_vector_write.h"
#include "cpl_string.h"
#include "gdal_utils.h"
#include "gdal_priv.h"

#ifndef _
#define _(x) (x)
#endif

//! @cond Doxygen_Suppress

/************************************************************************/
/*         GDALVectorWriteAlgorithm::GDALVectorWriteAlgorithm()         */
/************************************************************************/

GDALVectorWriteAlgorithm::GDALVectorWriteAlgorithm()
    : GDALVectorPipelineStepAlgorithm(NAME, DESCRIPTION, HELP_URL,
                                      /* standaloneStep =*/false)
{
    AddVectorOutputArgs(/* hiddenForCLI = */ false,
                        /* shortNameOutputLayerAllowed=*/true);
}

/************************************************************************/
/*                 GDALVectorWriteAlgorithm::RunStep()                  */
/************************************************************************/

bool GDALVectorWriteAlgorithm::RunStep(GDALPipelineStepRunContext &ctxt)
{
    auto pfnProgress = ctxt.m_pfnProgress;
    auto pProgressData = ctxt.m_pProgressData;
    auto poSrcDS = m_inputDataset[0].GetDatasetRef();
    CPLAssert(poSrcDS);

    if (m_format == "stream")
    {
        m_outputDataset.Set(poSrcDS);
        return true;
    }

    CPLStringList aosOptions;
    aosOptions.AddString("--invoked-from-gdal-algorithm");
    if (!m_overwrite)
    {
        aosOptions.AddString("--no-overwrite");
    }
    if (m_overwriteLayer)
    {
        aosOptions.AddString("-overwrite");
    }
    if (m_appendLayer)
    {
        aosOptions.AddString("-append");
    }
    if (m_upsert)
    {
        aosOptions.AddString("-upsert");
    }
    if (!m_format.empty())
    {
        aosOptions.AddString("-of");
        aosOptions.AddString(m_format.c_str());
    }
    for (const auto &co : m_creationOptions)
    {
        aosOptions.AddString("-dsco");
        aosOptions.AddString(co.c_str());
    }
    for (const auto &co : m_layerCreationOptions)
    {
        aosOptions.AddString("-lco");
        aosOptions.AddString(co.c_str());
    }
    if (!m_outputLayerName.empty())
    {
        aosOptions.AddString("-nln");
        aosOptions.AddString(m_outputLayerName.c_str());
    }
    if (pfnProgress && pfnProgress != GDALDummyProgress)
    {
        aosOptions.AddString("-progress");
    }
    if (m_skipErrors)
    {
        aosOptions.AddString("-skipfailures");
    }

    GDALDataset *poRetDS = nullptr;
    GDALDatasetH hOutDS =
        GDALDataset::ToHandle(m_outputDataset.GetDatasetRef());
    GDALVectorTranslateOptions *psOptions =
        GDALVectorTranslateOptionsNew(aosOptions.List(), nullptr);
    if (psOptions)
    {
        GDALVectorTranslateOptionsSetProgress(psOptions, pfnProgress,
                                              pProgressData);

        GDALDatasetH hSrcDS = GDALDataset::ToHandle(poSrcDS);
        poRetDS = GDALDataset::FromHandle(
            GDALVectorTranslate(m_outputDataset.GetName().c_str(), hOutDS, 1,
                                &hSrcDS, psOptions, nullptr));
        GDALVectorTranslateOptionsFree(psOptions);
    }

    if (!poRetDS)
    {
        return false;
    }

    if (!hOutDS)
    {
        m_outputDataset.Set(std::unique_ptr<GDALDataset>(poRetDS));
    }

    return true;
}

//! @endcond
