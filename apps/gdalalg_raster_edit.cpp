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
#include "ogrsf_frmts.h"

//! @cond Doxygen_Suppress

#ifndef _
#define _(x) (x)
#endif

/************************************************************************/
/*                          GetGCPFilename()                            */
/************************************************************************/

static std::string GetGCPFilename(const std::vector<std::string> &gcps)
{
    if (gcps.size() == 1 && !gcps[0].empty() && gcps[0][0] == '@')
    {
        return gcps[0].substr(1);
    }
    return std::string();
}

/************************************************************************/
/*          GDALRasterEditAlgorithm::GDALRasterEditAlgorithm()          */
/************************************************************************/

GDALRasterEditAlgorithm::GDALRasterEditAlgorithm(bool standaloneStep)
    : GDALRasterPipelineStepAlgorithm(
          NAME, DESCRIPTION, HELP_URL,
          ConstructorOptions().SetAddDefaultArguments(false))
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
    }

    AddArg("crs", 0, _("Override CRS (without reprojection)"), &m_overrideCrs)
        .AddHiddenAlias("a_srs")
        .AddHiddenAlias("srs")
        .SetIsCRSArg(/*noneAllowed=*/true);

    AddBBOXArg(&m_bbox);

    AddNodataArg(&m_nodata, /* noneAllowed = */ true);

    {
        auto &arg = AddArg("metadata", 0, _("Add/update dataset metadata item"),
                           &m_metadata)
                        .SetMetaVar("<KEY>=<VALUE>")
                        .SetPackedValuesAllowed(false);
        arg.AddValidationAction([this, &arg]()
                                { return ParseAndValidateKeyValue(arg); });
        arg.AddHiddenAlias("mo");
    }

    AddArg("unset-metadata", 0, _("Remove dataset metadata item(s)"),
           &m_unsetMetadata)
        .SetMetaVar("<KEY>");

    AddArg("unset-metadata-domain", 0, _("Remove dataset metadata domain(s)"),
           &m_unsetMetadataDomain)
        .SetMetaVar("<DOMAIN>");

    AddArg("gcp", 0,
           _("Add ground control point, formatted as "
             "pixel,line,easting,northing[,elevation], or @filename"),
           &m_gcps)
        .SetPackedValuesAllowed(false)
        .AddValidationAction(
            [this]()
            {
                if (GetGCPFilename(m_gcps).empty())
                {
                    for (const std::string &gcp : m_gcps)
                    {
                        const CPLStringList aosTokens(
                            CSLTokenizeString2(gcp.c_str(), ",", 0));
                        if (aosTokens.size() != 4 && aosTokens.size() != 5)
                        {
                            ReportError(CE_Failure, CPLE_IllegalArg,
                                        "Bad format for %s", gcp.c_str());
                            return false;
                        }
                        for (int i = 0; i < aosTokens.size(); ++i)
                        {
                            if (CPLGetValueType(aosTokens[i]) ==
                                CPL_VALUE_STRING)
                            {
                                ReportError(CE_Failure, CPLE_IllegalArg,
                                            "Bad format for %s", gcp.c_str());
                                return false;
                            }
                        }
                    }
                }
                return true;
            });

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
/*           GDALRasterEditAlgorithm::~GDALRasterEditAlgorithm()        */
/************************************************************************/

GDALRasterEditAlgorithm::~GDALRasterEditAlgorithm() = default;

/************************************************************************/
/*                              ParseGCPs()                             */
/************************************************************************/

std::vector<gdal::GCP> GDALRasterEditAlgorithm::ParseGCPs() const
{
    std::vector<gdal::GCP> ret;
    const std::string osGCPFilename = GetGCPFilename(m_gcps);
    if (!osGCPFilename.empty())
    {
        auto poDS = std::unique_ptr<GDALDataset>(GDALDataset::Open(
            osGCPFilename.c_str(), GDAL_OF_VECTOR | GDAL_OF_VERBOSE_ERROR));
        if (!poDS)
            return ret;
        if (poDS->GetLayerCount() != 1)
        {
            ReportError(CE_Failure, CPLE_AppDefined,
                        "GCPs can only be specified for single-layer datasets");
            return ret;
        }
        auto poLayer = poDS->GetLayer(0);
        const auto poLayerDefn = poLayer->GetLayerDefn();
        int nIdIdx = -1, nInfoIdx = -1, nColIdx = -1, nLineIdx = -1, nXIdx = -1,
            nYIdx = -1, nZIdx = -1;

        const struct
        {
            int &idx;
            const char *name;
            bool required;
        } aFields[] = {
            {nIdIdx, "id", false},     {nInfoIdx, "info", false},
            {nColIdx, "column", true}, {nLineIdx, "line", true},
            {nXIdx, "x", true},        {nYIdx, "y", true},
            {nZIdx, "z", false},
        };

        for (auto &field : aFields)
        {
            field.idx = poLayerDefn->GetFieldIndex(field.name);
            if (field.idx < 0 && field.required)
            {
                ReportError(CE_Failure, CPLE_AppDefined,
                            "Field '%s' cannot be found in '%s'", field.name,
                            poDS->GetDescription());
                return ret;
            }
        }
        for (auto &&poFeature : poLayer)
        {
            gdal::GCP gcp;
            if (nIdIdx >= 0)
                gcp.SetId(poFeature->GetFieldAsString(nIdIdx));
            if (nInfoIdx >= 0)
                gcp.SetInfo(poFeature->GetFieldAsString(nInfoIdx));
            gcp.Pixel() = poFeature->GetFieldAsDouble(nColIdx);
            gcp.Line() = poFeature->GetFieldAsDouble(nLineIdx);
            gcp.X() = poFeature->GetFieldAsDouble(nXIdx);
            gcp.Y() = poFeature->GetFieldAsDouble(nYIdx);
            if (nZIdx >= 0 && poFeature->IsFieldSetAndNotNull(nZIdx))
                gcp.Z() = poFeature->GetFieldAsDouble(nZIdx);
            ret.push_back(std::move(gcp));
        }
    }
    else
    {
        for (const std::string &gcpStr : m_gcps)
        {
            const CPLStringList aosTokens(
                CSLTokenizeString2(gcpStr.c_str(), ",", 0));
            // Verified by validation action
            CPLAssert(aosTokens.size() == 4 || aosTokens.size() == 5);
            gdal::GCP gcp;
            gcp.Pixel() = CPLAtof(aosTokens[0]);
            gcp.Line() = CPLAtof(aosTokens[1]);
            gcp.X() = CPLAtof(aosTokens[2]);
            gcp.Y() = CPLAtof(aosTokens[3]);
            if (aosTokens.size() == 5)
                gcp.Z() = CPLAtof(aosTokens[4]);
            ret.push_back(std::move(gcp));
        }
    }
    return ret;
}

/************************************************************************/
/*                GDALRasterEditAlgorithm::RunStep()                    */
/************************************************************************/

bool GDALRasterEditAlgorithm::RunStep(GDALPipelineStepRunContext &ctxt)
{
    GDALDataset *poDS = m_dataset.GetDatasetRef();
    if (poDS)
    {
        if (poDS->GetAccess() != GA_Update && !m_readOnly)
        {
            ReportError(CE_Failure, CPLE_AppDefined,
                        "Dataset should be opened in update mode unless "
                        "--auxiliary is set");
            return false;
        }
    }
    else
    {
        const auto poSrcDS = m_inputDataset[0].GetDatasetRef();
        CPLAssert(poSrcDS);
        CPLAssert(m_outputDataset.GetName().empty());
        CPLAssert(!m_outputDataset.GetDatasetRef());

        CPLStringList aosOptions;
        aosOptions.push_back("-of");
        aosOptions.push_back("VRT");
        GDALTranslateOptions *psOptions =
            GDALTranslateOptionsNew(aosOptions.List(), nullptr);
        GDALDatasetH hSrcDS = GDALDataset::ToHandle(poSrcDS);
        auto poRetDS = GDALDataset::FromHandle(
            GDALTranslate("", hSrcDS, psOptions, nullptr));
        GDALTranslateOptionsFree(psOptions);
        m_outputDataset.Set(std::unique_ptr<GDALDataset>(poRetDS));
        poDS = m_outputDataset.GetDatasetRef();
    }

    bool ret = poDS != nullptr;

    if (poDS)
    {
        if (m_overrideCrs == "null" || m_overrideCrs == "none")
        {
            if (poDS->SetSpatialRef(nullptr) != CE_None)
            {
                ReportError(CE_Failure, CPLE_AppDefined,
                            "SetSpatialRef(%s) failed", m_overrideCrs.c_str());
                return false;
            }
        }
        else if (!m_overrideCrs.empty() && m_gcps.empty())
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
            GDALGeoTransform gt;
            gt[0] = m_bbox[0];
            gt[1] = (m_bbox[2] - m_bbox[0]) / poDS->GetRasterXSize();
            gt[2] = 0;
            gt[3] = m_bbox[3];
            gt[4] = 0;
            gt[5] = -(m_bbox[3] - m_bbox[1]) / poDS->GetRasterYSize();
            if (poDS->SetGeoTransform(gt) != CE_None)
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

        for (const std::string &domain : m_unsetMetadataDomain)
        {
            if (poDS->SetMetadata(nullptr, domain.c_str()) != CE_None)
            {
                ReportError(CE_Failure, CPLE_AppDefined,
                            "SetMetadata(NULL, '%s') failed", domain.c_str());
                return false;
            }
        }

        if (!m_gcps.empty())
        {
            const auto gcps = ParseGCPs();
            if (gcps.empty())
                return false;  // error already emitted by ParseGCPs()

            OGRSpatialReference oSRS;
            if (!m_overrideCrs.empty())
            {
                oSRS.SetFromUserInput(m_overrideCrs.c_str());
                oSRS.SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);
            }

            if (poDS->SetGCPs(static_cast<int>(gcps.size()), gcps[0].c_ptr(),
                              oSRS.IsEmpty() ? nullptr : &oSRS) != CE_None)
            {
                ReportError(CE_Failure, CPLE_AppDefined, "Setting GCPs failed");
                return false;
            }
        }

        const int nBands = poDS->GetRasterCount();
        int nCurProgress = 0;
        const double dfTotalProgress =
            ((m_stats || m_approxStats) ? nBands : 0) + (m_hist ? nBands : 0);
        if (m_stats || m_approxStats)
        {
            for (int i = 0; (i < nBands) && ret; ++i)
            {
                void *pScaledProgress = GDALCreateScaledProgress(
                    nCurProgress / dfTotalProgress,
                    (nCurProgress + 1) / dfTotalProgress, ctxt.m_pfnProgress,
                    ctxt.m_pProgressData);
                ++nCurProgress;
                double dfMin = 0.0;
                double dfMax = 0.0;
                double dfMean = 0.0;
                double dfStdDev = 0.0;
                ret = poDS->GetRasterBand(i + 1)->ComputeStatistics(
                          m_approxStats, &dfMin, &dfMax, &dfMean, &dfStdDev,
                          pScaledProgress ? GDALScaledProgress : nullptr,
                          pScaledProgress) == CE_None;
                GDALDestroyScaledProgress(pScaledProgress);
            }
        }

        if (m_hist)
        {
            for (int i = 0; (i < nBands) && ret; ++i)
            {
                void *pScaledProgress = GDALCreateScaledProgress(
                    nCurProgress / dfTotalProgress,
                    (nCurProgress + 1) / dfTotalProgress, ctxt.m_pfnProgress,
                    ctxt.m_pProgressData);
                ++nCurProgress;
                double dfMin = 0.0;
                double dfMax = 0.0;
                int nBucketCount = 0;
                GUIntBig *panHistogram = nullptr;
                ret = poDS->GetRasterBand(i + 1)->GetDefaultHistogram(
                          &dfMin, &dfMax, &nBucketCount, &panHistogram, TRUE,
                          pScaledProgress ? GDALScaledProgress : nullptr,
                          pScaledProgress) == CE_None;
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
    }

    return poDS != nullptr;
}

GDALRasterEditAlgorithmStandalone::~GDALRasterEditAlgorithmStandalone() =
    default;

//! @endcond
