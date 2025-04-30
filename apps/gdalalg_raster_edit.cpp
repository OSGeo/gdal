/******************************************************************************
 *
 * Project:  GDAL
 * Purpose:  "edit" step of "raster pipeline"
 * Author:   Even Rouault <even dot rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2024, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include "gdalalg_raster_edit.h"

#include "gdal_priv.h"
#include "gdal_utils.h"

//! @cond Doxygen_Suppress

#ifndef _
#define _(x) (x)
#endif

/************************************************************************/
/*          GDALRasterEditAlgorithm::GDALRasterEditAlgorithm()          */
/************************************************************************/

GDALRasterEditAlgorithm::GDALRasterEditAlgorithm(bool standaloneStep)
    : GDALRasterPipelineStepAlgorithm(
          NAME, DESCRIPTION, HELP_URL,
          // Avoid automatic addition of input/output arguments
          /*standaloneStep = */ false)
{
    if (standaloneStep)
    {
        AddProgressArg();

        AddArg("dataset", 0,
               _("Dataset (to be updated in-place, unless --auxiliary)"),
               &m_dataset, GDAL_OF_RASTER | GDAL_OF_UPDATE)
            .SetPositional()
            .SetRequired();
        AddArg("auxiliary", 0,
               _("Ask for an auxiliary .aux.xml file to be edited"),
               &m_readOnly)
            .AddHiddenAlias("ro")
            .AddHiddenAlias(GDAL_ARG_NAME_READ_ONLY);

        m_standaloneStep = true;
    }

    AddArg("crs", 0, _("Override CRS (without reprojection)"), &m_overrideCrs)
        .AddHiddenAlias("a_srs")
        .SetIsCRSArg(/*noneAllowed=*/true);

    AddBBOXArg(&m_bbox);

    AddNodataDataTypeArg(&m_nodata, /* noneAllowed = */ true);

    {
        auto &arg = AddArg("metadata", 0, _("Add/update dataset metadata item"),
                           &m_metadata)
                        .SetMetaVar("<KEY>=<VALUE>")
                        .SetPackedValuesAllowed(false);
        arg.AddValidationAction([this, &arg]()
                                { return ParseAndValidateKeyValue(arg); });
        arg.AddHiddenAlias("mo");
    }

    AddArg("unset-metadata", 0, _("Remove dataset metadata item"),
           &m_unsetMetadata)
        .SetMetaVar("<KEY>");

    if (standaloneStep)
    {
        AddArg("stats", 0, _("Compute statistics, using all pixels"), &m_stats)
            .SetMutualExclusionGroup("stats");
        AddArg("approx-stats", 0,
               _("Compute statistics, using a subset of pixels"),
               &m_approxStats)
            .SetMutualExclusionGroup("stats");
        AddArg("hist", 0, _("Compute histogram"), &m_hist);
    }
}

/************************************************************************/
/*                GDALRasterEditAlgorithm::RunImpl()                    */
/************************************************************************/

bool GDALRasterEditAlgorithm::RunImpl(GDALProgressFunc pfnProgress,
                                      void *pProgressData)
{
    if (m_standaloneStep)
    {
        auto poDS = m_dataset.GetDatasetRef();
        CPLAssert(poDS);
        if (poDS->GetAccess() != GA_Update && !m_readOnly)
        {
            ReportError(CE_Failure, CPLE_AppDefined,
                        "Dataset should be opened in update mode unless "
                        "--auxiliary is set");
            return false;
        }

        if (m_overrideCrs == "null" || m_overrideCrs == "none")
        {
            if (poDS->SetSpatialRef(nullptr) != CE_None)
            {
                ReportError(CE_Failure, CPLE_AppDefined,
                            "SetSpatialRef(%s) failed", m_overrideCrs.c_str());
                return false;
            }
        }
        else if (!m_overrideCrs.empty())
        {
            OGRSpatialReference oSRS;
            oSRS.SetFromUserInput(m_overrideCrs.c_str());
            oSRS.SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);
            if (poDS->SetSpatialRef(&oSRS) != CE_None)
            {
                ReportError(CE_Failure, CPLE_AppDefined,
                            "SetSpatialRef(%s) failed", m_overrideCrs.c_str());
                return false;
            }
        }

        if (!m_bbox.empty())
        {
            if (poDS->GetRasterXSize() == 0 || poDS->GetRasterYSize() == 0)
            {
                ReportError(CE_Failure, CPLE_AppDefined,
                            "Cannot set extent because one of dataset height "
                            "or width is null");
                return false;
            }
            double adfGT[6];
            adfGT[0] = m_bbox[0];
            adfGT[1] = (m_bbox[2] - m_bbox[0]) / poDS->GetRasterXSize();
            adfGT[2] = 0;
            adfGT[3] = m_bbox[3];
            adfGT[4] = 0;
            adfGT[5] = -(m_bbox[3] - m_bbox[1]) / poDS->GetRasterYSize();
            if (poDS->SetGeoTransform(adfGT) != CE_None)
            {
                ReportError(CE_Failure, CPLE_AppDefined,
                            "Setting extent failed");
                return false;
            }
        }

        if (!m_nodata.empty())
        {
            for (int i = 0; i < poDS->GetRasterCount(); ++i)
            {
                if (EQUAL(m_nodata.c_str(), "none"))
                    poDS->GetRasterBand(i + 1)->DeleteNoDataValue();
                else
                    poDS->GetRasterBand(i + 1)->SetNoDataValue(
                        CPLAtof(m_nodata.c_str()));
            }
        }

        const CPLStringList aosMD(m_metadata);
        for (const auto &[key, value] : cpl::IterateNameValue(aosMD))
        {
            if (poDS->SetMetadataItem(key, value) != CE_None)
            {
                ReportError(CE_Failure, CPLE_AppDefined,
                            "SetMetadataItem('%s', '%s') failed", key, value);
                return false;
            }
        }

        for (const std::string &key : m_unsetMetadata)
        {
            if (poDS->SetMetadataItem(key.c_str(), nullptr) != CE_None)
            {
                ReportError(CE_Failure, CPLE_AppDefined,
                            "SetMetadataItem('%s', NULL) failed", key.c_str());
                return false;
            }
        }

        const int nBands = poDS->GetRasterCount();
        int nCurProgress = 0;
        const double dfTotalProgress =
            ((m_stats || m_approxStats) ? nBands : 0) + (m_hist ? nBands : 0);
        bool ret = true;
        if (m_stats || m_approxStats)
        {
            for (int i = 0; (i < nBands) && ret; ++i)
            {
                void *pScaledProgress = GDALCreateScaledProgress(
                    nCurProgress / dfTotalProgress,
                    (nCurProgress + 1) / dfTotalProgress, pfnProgress,
                    pProgressData);
                ++nCurProgress;
                double dfMin = 0.0;
                double dfMax = 0.0;
                double dfMean = 0.0;
                double dfStdDev = 0.0;
                ret = poDS->GetRasterBand(i + 1)->ComputeStatistics(
                          m_approxStats, &dfMin, &dfMax, &dfMean, &dfStdDev,
                          GDALScaledProgress, pScaledProgress) == CE_None;
                GDALDestroyScaledProgress(pScaledProgress);
            }
        }
        if (m_hist)
        {
            for (int i = 0; (i < nBands) && ret; ++i)
            {
                void *pScaledProgress = GDALCreateScaledProgress(
                    nCurProgress / dfTotalProgress,
                    (nCurProgress + 1) / dfTotalProgress, pfnProgress,
                    pProgressData);
                ++nCurProgress;
                double dfMin = 0.0;
                double dfMax = 0.0;
                int nBucketCount = 0;
                GUIntBig *panHistogram = nullptr;
                ret = poDS->GetRasterBand(i + 1)->GetDefaultHistogram(
                          &dfMin, &dfMax, &nBucketCount, &panHistogram, TRUE,
                          GDALScaledProgress, pScaledProgress) == CE_None;
                if (ret)
                {
                    ret = poDS->GetRasterBand(i + 1)->SetDefaultHistogram(
                              dfMin, dfMax, nBucketCount, panHistogram) ==
                          CE_None;
                }
                CPLFree(panHistogram);
                GDALDestroyScaledProgress(pScaledProgress);
            }
        }

        return ret;
    }
    else
    {
        return RunStep(pfnProgress, pProgressData);
    }
}

/************************************************************************/
/*                GDALRasterEditAlgorithm::RunStep()                    */
/************************************************************************/

bool GDALRasterEditAlgorithm::RunStep(GDALProgressFunc, void *)
{
    CPLAssert(m_inputDataset.GetDatasetRef());
    CPLAssert(m_outputDataset.GetName().empty());
    CPLAssert(!m_outputDataset.GetDatasetRef());

    CPLStringList aosOptions;
    aosOptions.AddString("-of");
    aosOptions.AddString("VRT");
    if (!m_overrideCrs.empty())
    {
        aosOptions.AddString("-a_srs");
        aosOptions.AddString(m_overrideCrs.c_str());
    }
    if (!m_bbox.empty())
    {
        aosOptions.AddString("-a_ullr");
        aosOptions.AddString(CPLSPrintf("%.17g", m_bbox[0]));  // upper-left X
        aosOptions.AddString(CPLSPrintf("%.17g", m_bbox[3]));  // upper-left Y
        aosOptions.AddString(CPLSPrintf("%.17g", m_bbox[2]));  // lower-right X
        aosOptions.AddString(CPLSPrintf("%.17g", m_bbox[1]));  // lower-right Y
    }

    for (const auto &val : m_metadata)
    {
        aosOptions.AddString("-mo");
        aosOptions.AddString(val.c_str());
    }

    for (const std::string &key : m_unsetMetadata)
    {
        aosOptions.AddString("-mo");
        aosOptions.AddString((key + "=").c_str());
    }

    if (!m_nodata.empty())
    {
        aosOptions.AddString("-a_nodata");
        aosOptions.AddString(m_nodata);
    }

    GDALTranslateOptions *psOptions =
        GDALTranslateOptionsNew(aosOptions.List(), nullptr);

    GDALDatasetH hSrcDS = GDALDataset::ToHandle(m_inputDataset.GetDatasetRef());
    auto poRetDS =
        GDALDataset::FromHandle(GDALTranslate("", hSrcDS, psOptions, nullptr));
    GDALTranslateOptionsFree(psOptions);
    const bool ok = poRetDS != nullptr;
    if (ok)
        m_outputDataset.Set(std::unique_ptr<GDALDataset>(poRetDS));

    return ok;
}

//! @endcond
