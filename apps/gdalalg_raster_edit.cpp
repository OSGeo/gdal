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
        AddArg("dataset", 0, _("Dataset (in-place updated)"), &m_dataset,
               GDAL_OF_RASTER | GDAL_OF_UPDATE)
            .SetPositional()
            .SetRequired();
        m_standaloneStep = true;
    }

    AddArg("crs", 0, _("Override CRS (without reprojection)"), &m_overrideCrs)
        .AddHiddenAlias("a_srs")
        .SetIsCRSArg(/*noneAllowed=*/true);

    AddBBOXArg(&m_bbox);

    {
        auto &arg = AddArg("metadata", 0, _("Add/update dataset metadata item"),
                           &m_metadata)
                        .SetMetaVar("<KEY>=<VALUE>");
        arg.AddValidationAction([this, &arg]()
                                { return ValidateKeyValue(arg); });
        arg.AddHiddenAlias("mo");
    }

    AddArg("unset-metadata", 0, _("Remove dataset metadata item"),
           &m_unsetMetadata)
        .SetMetaVar("<KEY>");
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
        if (poDS->GetAccess() != GA_Update)
        {
            ReportError(CE_Failure, CPLE_AppDefined,
                        "Dataset should be opened in update mode");
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
                ReportError(
                    CE_Failure, CPLE_AppDefined,
                    "Cannot set extent because dataset has 0x0 dimension");
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

        return true;
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
