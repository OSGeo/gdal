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
    auto poSrcDS = m_inputDataset.GetDatasetRef();
    CPLAssert(poSrcDS);
    CPLAssert(m_outputDataset.GetName().empty());
    CPLAssert(!m_outputDataset.GetDatasetRef());

    CPLStringList aosOptions;
    aosOptions.AddString("-of");
    aosOptions.AddString("VRT");
    aosOptions.AddString("-unscale");
    aosOptions.AddString("-ot");
    if (!m_type.empty())
    {
        aosOptions.AddString(m_type.c_str());
    }
    else
    {
        const auto eSrcDT = poSrcDS->GetRasterCount() > 0
                                ? poSrcDS->GetRasterBand(1)->GetRasterDataType()
                                : GDT_Unknown;
        if (GDALGetNonComplexDataType(eSrcDT) != GDT_Float64)
        {
            aosOptions.AddString(GDALDataTypeIsComplex(eSrcDT) ? "CFloat32"
                                                               : "Float32");
        }
        else
        {
            aosOptions.AddString(GDALDataTypeIsComplex(eSrcDT) ? "CFloat64"
                                                               : "Float64");
        }
    }

    GDALTranslateOptions *psOptions =
        GDALTranslateOptionsNew(aosOptions.List(), nullptr);

    auto poOutDS = std::unique_ptr<GDALDataset>(GDALDataset::FromHandle(
        GDALTranslate("", GDALDataset::ToHandle(poSrcDS), psOptions, nullptr)));
    GDALTranslateOptionsFree(psOptions);
    const bool bRet = poOutDS != nullptr;
    if (poOutDS)
    {
        m_outputDataset.Set(std::move(poOutDS));
    }

    return bRet;
}

//! @endcond
