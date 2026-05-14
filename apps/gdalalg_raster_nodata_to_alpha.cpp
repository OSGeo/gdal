/******************************************************************************
 *
 * Project:  GDAL
 * Purpose:  "nodata-to-alpha" step of "raster pipeline"
 * Author:   Even Rouault <even dot rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2025, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include "gdalalg_raster_nodata_to_alpha.h"

#include "gdal_priv.h"
#include "gdal_utils.h"

//! @cond Doxygen_Suppress

#ifndef _
#define _(x) (x)
#endif

/************************************************************************/
/* GDALRasterNoDataToAlphaAlgorithm::GDALRasterNoDataToAlphaAlgorithm() */
/************************************************************************/

GDALRasterNoDataToAlphaAlgorithm::GDALRasterNoDataToAlphaAlgorithm(
    bool standaloneStep)
    : GDALRasterPipelineStepAlgorithm(NAME, DESCRIPTION, HELP_URL,
                                      standaloneStep)
{
    AddArg("nodata", 0,
           _("Override nodata value of input band(s) "
             "(numeric value, 'nan', 'inf', '-inf')"),
           &m_nodata);
}

/************************************************************************/
/*             GDALRasterNoDataToAlphaAlgorithm::RunStep()              */
/************************************************************************/

bool GDALRasterNoDataToAlphaAlgorithm::RunStep(GDALPipelineStepRunContext &)
{
    GDALDataset *poSrcDS = m_inputDataset[0].GetDatasetRef();
    CPLAssert(poSrcDS);
    CPLAssert(m_outputDataset.GetName().empty());
    CPLAssert(!m_outputDataset.GetDatasetRef());

    if (!m_nodata.empty())
    {
        CPLStringList aosOptions;
        aosOptions.AddString("-of");
        aosOptions.AddString("VRT");

        if (m_nodata.size() == 1)
        {
            aosOptions.AddString("-a_nodata");
            aosOptions.AddString(CPLSPrintf("%.17g", m_nodata[0]));
        }
        else
        {
            if (m_nodata.size() !=
                static_cast<size_t>(poSrcDS->GetRasterCount()))
            {
                ReportError(CE_Failure, CPLE_IllegalArg,
                            "There should be %d nodata values given the input "
                            "dataset has %d bands",
                            poSrcDS->GetRasterCount(),
                            poSrcDS->GetRasterCount());
                return false;
            }
            aosOptions.AddString("-mo");
            std::string nodataValues("NODATA_VALUES=");
            for (size_t i = 0; i < m_nodata.size(); ++i)
            {
                if (i > 0)
                    nodataValues += ' ';
                nodataValues += CPLSPrintf("%.17g", m_nodata[i]);
            }
            aosOptions.AddString(nodataValues.c_str());
        }

        GDALTranslateOptions *psOptions =
            GDALTranslateOptionsNew(aosOptions.List(), nullptr);
        if (psOptions)
        {
            m_tempDS.reset(GDALDataset::FromHandle(GDALTranslate(
                "", GDALDataset::ToHandle(poSrcDS), psOptions, nullptr)));
            GDALTranslateOptionsFree(psOptions);
        }
        poSrcDS = m_tempDS.get();
    }

    bool bRet = poSrcDS != nullptr;
    if (poSrcDS)
    {
        CPLStringList aosOptions;
        aosOptions.AddString("-of");
        aosOptions.AddString("VRT");

        if (poSrcDS->GetRasterCount() > 0 &&
            poSrcDS->GetRasterBand(1)->GetMaskFlags() != GMF_ALL_VALID &&
            poSrcDS->GetRasterBand(1)->GetMaskFlags() !=
                (GMF_ALPHA | GMF_PER_DATASET))
        {
            aosOptions.AddString("-a_nodata");
            aosOptions.AddString("none");

            for (int i = 1; i <= poSrcDS->GetRasterCount(); ++i)
            {
                aosOptions.AddString("-b");
                aosOptions.AddString(CPLSPrintf("%d", i));
            }
            aosOptions.AddString("-b");
            aosOptions.AddString("mask");

            aosOptions.AddString(
                CPLSPrintf("-colorinterp_%d", poSrcDS->GetRasterCount() + 1));
            aosOptions.AddString("alpha");
        }

        std::unique_ptr<GDALDataset> poOutDS;
        GDALTranslateOptions *psOptions =
            GDALTranslateOptionsNew(aosOptions.List(), nullptr);
        if (psOptions)
        {
            poOutDS.reset(GDALDataset::FromHandle(GDALTranslate(
                "", GDALDataset::ToHandle(poSrcDS), psOptions, nullptr)));
            GDALTranslateOptionsFree(psOptions);

            poOutDS->GetRasterBand(1)->GetMaskFlags();
        }
        bRet = poOutDS != nullptr;
        if (poOutDS)
        {
            m_outputDataset.Set(std::move(poOutDS));
        }
    }

    return bRet;
}

GDALRasterNoDataToAlphaAlgorithmStandalone::
    ~GDALRasterNoDataToAlphaAlgorithmStandalone() = default;

//! @endcond
