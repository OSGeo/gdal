/******************************************************************************
 *
 * Project:  GDAL
 * Purpose:  "unscale" step of "raster pipeline"
 * Author:   Even Rouault <even dot rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2025, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include "gdalalg_raster_unscale.h"

#include "gdal_priv.h"
#include "gdal_utils.h"

//! @cond Doxygen_Suppress

#ifndef _
#define _(x) (x)
#endif

/************************************************************************/
/*        GDALRasterUnscaleAlgorithm::GDALRasterUnscaleAlgorithm()      */
/************************************************************************/

GDALRasterUnscaleAlgorithm::GDALRasterUnscaleAlgorithm(bool standaloneStep)
    : GDALRasterPipelineStepAlgorithm(NAME, DESCRIPTION, HELP_URL,
                                      standaloneStep)
{
    AddOutputDataTypeArg(&m_type);
}

/************************************************************************/
/*               GDALRasterUnscaleAlgorithm::RunStep()                  */
/************************************************************************/

bool GDALRasterUnscaleAlgorithm::RunStep(GDALProgressFunc, void *)
{
    CPLAssert(m_inputDataset.GetDatasetRef());
    CPLAssert(m_outputDataset.GetName().empty());
    CPLAssert(!m_outputDataset.GetDatasetRef());

    CPLStringList aosOptions;
    aosOptions.AddString("-of");
    aosOptions.AddString("VRT");
    aosOptions.AddString("-unscale");
    if (!m_type.empty())
    {
        aosOptions.AddString("-ot");
        aosOptions.AddString(m_type.c_str());
    }

    GDALTranslateOptions *psOptions =
        GDALTranslateOptionsNew(aosOptions.List(), nullptr);

    auto poOutDS = std::unique_ptr<GDALDataset>(GDALDataset::FromHandle(
        GDALTranslate("", GDALDataset::ToHandle(m_inputDataset.GetDatasetRef()),
                      psOptions, nullptr)));
    GDALTranslateOptionsFree(psOptions);
    const bool bRet = poOutDS != nullptr;
    if (poOutDS)
    {
        m_outputDataset.Set(std::move(poOutDS));
    }

    return bRet;
}

//! @endcond
