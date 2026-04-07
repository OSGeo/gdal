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

#include <optional>

//! @cond Doxygen_Suppress

#ifndef _
#define _(x) (x)
#endif

/************************************************************************/
/*                           GetGCPFilename()                           */
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
        AddOpenOptionsArg(&m_openOptions);
        AddArg("auxiliary", 0,
               _("Ask for an auxiliary .aux.xml file to be edited"),
               &m_readOnly)
            .AddHiddenAlias("ro")
            .AddHiddenAlias(GDAL_ARG_NAME_READ_ONLY);
    }
    else
    {
        AddRasterHiddenInputDatasetArg();
    }

    AddArg("crs", 0, _("Override CRS (without reprojection)"), &m_overrideCrs)
        .AddHiddenAlias("a_srs")
        .AddHiddenAlias("srs")
        .SetIsCRSArg(/*noneAllowed=*/true);

    AddBBOXArg(&m_bbox);

    AddNodataArg(&m_nodata, /* noneAllowed = */ true);

    AddArg("color-interpretation", 0, _("Set band color interpretation"),
           &m_colorInterpretation)
        .SetMetaVar("[all|<BAND>=]<COLOR-INTEPRETATION>")
        .SetAutoCompleteFunction(
            [this](const std::string &s)
            {
                std::vector<std::string> ret;
                int nValues = 0;
                const auto paeVals = GDALGetColorInterpretationList(&nValues);
                if (s.find('=') == std::string::npos)
                {
                    ret.push_back("all=");
                    if (auto poDS = m_dataset.GetDatasetRef())
                    {
                        for (int i = 0; i < poDS->GetRasterCount(); ++i)
                            ret.push_back(std::to_string(i + 1).append("="));
                    }
                    for (int i = 0; i < nValues; ++i)
                        ret.push_back(
                            GDALGetColorInterpretationName(paeVals[i]));
                }
                else
                {
                    for (int i = 0; i < nValues; ++i)
                        ret.push_back(
                            GDALGetColorInterpretationName(paeVals[i]));
                }
                return ret;
            });

    const auto ValidationActionScaleOffset =
        [this](const char *argName, const std::vector<std::string> &values)
    {
        for (const std::string &s : values)
        {
            bool valid = true;
            const auto nPos = s.find('=');
            if (nPos != std::string::npos)
            {
                if (CPLGetValueType(s.substr(0, nPos).c_str()) !=
                        CPL_VALUE_INTEGER ||
                    CPLGetValueType(s.substr(nPos + 1).c_str()) ==
                        CPL_VALUE_STRING)
                {
                    valid = false;
                }
            }
            else if (CPLGetValueType(s.c_str()) == CPL_VALUE_STRING)
            {
                valid = false;
            }
            if (!valid)
            {
                ReportError(CE_Failure, CPLE_IllegalArg,
                            "Invalid value '%s' for '%s'", s.c_str(), argName);
                return false;
            }
        }
        return true;
    };

    AddArg("scale", 0, _("Override band scale factor"), &m_scale)
        .SetMetaVar("[<BAND>=]<SCALE>")
        .AddValidationAction(
            [this, ValidationActionScaleOffset]()
            { return ValidationActionScaleOffset("scale", m_scale); });

    AddArg("offset", 0, _("Override band offset constant"), &m_offset)
        .SetMetaVar("[<BAND>=]<OFFSET>")
        .AddValidationAction(
            [this, ValidationActionScaleOffset]()
            { return ValidationActionScaleOffset("offset", m_offset); });

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
/*         GDALRasterEditAlgorithm::~GDALRasterEditAlgorithm()          */
/************************************************************************/

GDALRasterEditAlgorithm::~GDALRasterEditAlgorithm() = default;

/************************************************************************/
/*                             ParseGCPs()                              */
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
/*                  GDALRasterEditAlgorithm::RunStep()                  */
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
            gt.xorig = m_bbox[0];
            gt.xscale = (m_bbox[2] - m_bbox[0]) / poDS->GetRasterXSize();
            gt.xrot = 0;
            gt.yorig = m_bbox[3];
            gt.yrot = 0;
            gt.yscale = -(m_bbox[3] - m_bbox[1]) / poDS->GetRasterYSize();
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

        if (!m_colorInterpretation.empty())
        {
            const auto GetColorInterp =
                [this](const char *pszStr) -> std::optional<GDALColorInterp>
            {
                if (EQUAL(pszStr, "undefined"))
                    return GCI_Undefined;
                const GDALColorInterp eInterp =
                    GDALGetColorInterpretationByName(pszStr);
                if (eInterp != GCI_Undefined)
                    return eInterp;
                ReportError(CE_Failure, CPLE_NotSupported,
                            "Unsupported color interpretation: %s", pszStr);
                return {};
            };

            if (m_colorInterpretation.size() == 1 &&
                poDS->GetRasterCount() > 1 &&
                !cpl::starts_with(m_colorInterpretation[0], "all="))
            {
                ReportError(
                    CE_Failure, CPLE_NotSupported,
                    "With several bands, specify as many color interpretation "
                    "as bands, one or many values of the form "
                    "<band_number>=<color> or a single value all=<color>");
                return false;
            }
            else
            {
                int nBandIter = 0;
                bool bSyntaxAll = false;
                bool bSyntaxExplicitBand = false;
                bool bSyntaxImplicitBand = false;
                for (const std::string &token : m_colorInterpretation)
                {
                    const CPLStringList aosTokens(
                        CSLTokenizeString2(token.c_str(), "=", 0));
                    if (aosTokens.size() == 2 && EQUAL(aosTokens[0], "all"))
                    {
                        bSyntaxAll = true;
                        const auto eColorInterp = GetColorInterp(aosTokens[1]);
                        if (!eColorInterp)
                        {
                            return false;
                        }
                        else
                        {
                            for (int i = 0; i < poDS->GetRasterCount(); ++i)
                            {
                                if (poDS->GetRasterBand(i + 1)
                                        ->SetColorInterpretation(
                                            *eColorInterp) != CE_None)
                                    return false;
                            }
                        }
                    }
                    else if (aosTokens.size() == 2)
                    {
                        bSyntaxExplicitBand = true;
                        const int nBand = atoi(aosTokens[0]);
                        if (nBand <= 0 || nBand > poDS->GetRasterCount())
                        {
                            ReportError(CE_Failure, CPLE_NotSupported,
                                        "Invalid band number '%s' in '%s'",
                                        aosTokens[0], token.c_str());
                            return false;
                        }
                        const auto eColorInterp = GetColorInterp(aosTokens[1]);
                        if (!eColorInterp)
                        {
                            return false;
                        }
                        else if (poDS->GetRasterBand(nBand)
                                     ->SetColorInterpretation(*eColorInterp) !=
                                 CE_None)
                        {
                            return false;
                        }
                    }
                    else
                    {
                        bSyntaxImplicitBand = true;
                        ++nBandIter;
                        if (nBandIter > poDS->GetRasterCount())
                        {
                            ReportError(CE_Failure, CPLE_IllegalArg,
                                        "More color interpretation values "
                                        "specified than bands in the dataset");
                            return false;
                        }
                        const auto eColorInterp = GetColorInterp(token.c_str());
                        if (!eColorInterp)
                        {
                            return false;
                        }
                        else if (poDS->GetRasterBand(nBandIter)
                                     ->SetColorInterpretation(*eColorInterp) !=
                                 CE_None)
                        {
                            return false;
                        }
                    }
                }
                if ((bSyntaxAll ? 1 : 0) + (bSyntaxExplicitBand ? 1 : 0) +
                        (bSyntaxImplicitBand ? 1 : 0) !=
                    1)
                {
                    ReportError(CE_Failure, CPLE_IllegalArg,
                                "Mix of different syntaxes to specify color "
                                "interpretation");
                    return false;
                }
                if (bSyntaxImplicitBand && nBandIter != poDS->GetRasterCount())
                {
                    ReportError(CE_Failure, CPLE_IllegalArg,
                                "Less color interpretation values specified "
                                "than bands in the dataset");
                    return false;
                }
            }
        }

        const auto ScaleOffsetSetterLambda =
            [this, poDS](const char *argName,
                         const std::vector<std::string> &values,
                         CPLErr (GDALRasterBand::*Setter)(double))
        {
            if (values.size() == 1 && values[0].find('=') == std::string::npos)
            {
                const double dfScale = CPLAtof(values[0].c_str());
                for (int i = 0; i < poDS->GetRasterCount(); ++i)
                {
                    if ((poDS->GetRasterBand(i + 1)->*Setter)(dfScale) !=
                        CE_None)
                        return false;
                }
            }
            else
            {
                int nBandIter = 0;
                bool bSyntaxExplicitBand = false;
                bool bSyntaxImplicitBand = false;
                for (const std::string &token : values)
                {
                    const CPLStringList aosTokens(
                        CSLTokenizeString2(token.c_str(), "=", 0));
                    if (aosTokens.size() == 2)
                    {
                        bSyntaxExplicitBand = true;
                        const int nBand = atoi(aosTokens[0]);
                        if (nBand <= 0 || nBand > poDS->GetRasterCount())
                        {
                            ReportError(CE_Failure, CPLE_NotSupported,
                                        "Invalid band number '%s' in '%s'",
                                        aosTokens[0], token.c_str());
                            return false;
                        }
                        const double dfScale = CPLAtof(aosTokens[1]);
                        if ((poDS->GetRasterBand(nBand)->*Setter)(dfScale) !=
                            CE_None)
                        {
                            return false;
                        }
                    }
                    else
                    {
                        bSyntaxImplicitBand = true;
                        ++nBandIter;
                        if (nBandIter > poDS->GetRasterCount())
                        {
                            ReportError(CE_Failure, CPLE_IllegalArg,
                                        "More %s values "
                                        "specified than bands in the dataset",
                                        argName);
                            return false;
                        }
                        const double dfScale = CPLAtof(token.c_str());
                        if ((poDS->GetRasterBand(nBandIter)->*Setter)(
                                dfScale) != CE_None)
                        {
                            return false;
                        }
                    }
                }
                if (((bSyntaxExplicitBand ? 1 : 0) +
                     (bSyntaxImplicitBand ? 1 : 0)) != 1)
                {
                    ReportError(CE_Failure, CPLE_IllegalArg,
                                "Mix of different syntaxes to specify %s",
                                argName);
                    return false;
                }
                if (bSyntaxImplicitBand && nBandIter != poDS->GetRasterCount())
                {
                    ReportError(CE_Failure, CPLE_IllegalArg,
                                "Less %s values specified "
                                "than bands in the dataset",
                                argName);
                    return false;
                }
            }

            return true;
        };

        if (!m_scale.empty())
        {
            if (!ScaleOffsetSetterLambda("scale", m_scale,
                                         &GDALRasterBand::SetScale))
                return false;
        }

        if (!m_offset.empty())
        {
            if (!ScaleOffsetSetterLambda("offset", m_offset,
                                         &GDALRasterBand::SetOffset))
                return false;
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
