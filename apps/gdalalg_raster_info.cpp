/******************************************************************************
 *
 * Project:  GDAL
 * Purpose:  gdal "raster info" subcommand
 * Author:   Even Rouault <even dot rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2024, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include "gdalalg_raster_info.h"

#include "cpl_conv.h"
#include "gdal_priv.h"
#include "gdal_utils.h"

//! @cond Doxygen_Suppress

#ifndef _
#define _(x) (x)
#endif

/************************************************************************/
/*            GDALRasterInfoAlgorithm::GDALRasterInfoAlgorithm()        */
/************************************************************************/

GDALRasterInfoAlgorithm::GDALRasterInfoAlgorithm(bool openForMixedRasterVector)
    : GDALAlgorithm(NAME, DESCRIPTION, HELP_URL)
{
    AddOutputFormatArg(&m_format).SetDefault("json").SetChoices("json", "text");
    AddArg("min-max", 0, _("Compute minimum and maximum value"), &m_minMax)
        .AddAlias("mm");
    AddArg("stats", 0, _("Retrieve or compute statistics, using all pixels"),
           &m_stats)
        .SetMutualExclusionGroup("stats");
    AddArg("approx-stats", 0,
           _("Retrieve or compute statistics, using a subset of pixels"),
           &m_approxStats)
        .SetMutualExclusionGroup("stats");
    AddArg("hist", 0, _("Retrieve or compute histogram"), &m_hist);

    AddOpenOptionsArg(&m_openOptions);
    AddInputFormatsArg(&m_inputFormats)
        .AddMetadataItem(GAAMDI_REQUIRED_CAPABILITIES, {GDAL_DCAP_RASTER});
    AddArg("no-gcp", 0, _("Suppress ground control points list printing"),
           &m_noGCP)
        .SetCategory(GAAC_ADVANCED);
    AddArg("no-md", 0, _("Suppress metadata printing"), &m_noMD)
        .SetCategory(GAAC_ADVANCED);
    AddArg("no-ct", 0, _("Suppress color table printing"), &m_noCT)
        .SetCategory(GAAC_ADVANCED);
    AddArg("no-fl", 0, _("Suppress file list printing"), &m_noFL)
        .SetCategory(GAAC_ADVANCED);
    AddArg("checksum", 0, _("Compute pixel checksum"), &m_checksum)
        .SetCategory(GAAC_ADVANCED);
    AddArg("list-mdd", 0,
           _("List all metadata domains available for the dataset"), &m_listMDD)
        .AddAlias("list-metadata-domains")
        .SetCategory(GAAC_ADVANCED);
    AddArg("metadata-domain", 0,
           _("Report metadata for the specified domain. 'all' can be used to "
             "report metadata in all domains"),
           &m_mdd)
        .AddAlias("mdd")
        .SetCategory(GAAC_ADVANCED);

    AddArg("no-nodata", 0, _("Suppress retrieving nodata value"), &m_noNodata)
        .SetCategory(GAAC_ESOTERIC);
    AddArg("no-mask", 0, _("Suppress mask band information"), &m_noMask)
        .SetCategory(GAAC_ESOTERIC);
    AddArg("subdataset", 0,
           _("Use subdataset of specified index (starting at 1), instead of "
             "the source dataset itself"),
           &m_subDS)
        .SetCategory(GAAC_ESOTERIC)
        .SetMinValueIncluded(1);

    AddInputDatasetArg(&m_dataset, openForMixedRasterVector
                                       ? GDAL_OF_RASTER | GDAL_OF_VECTOR
                                       : GDAL_OF_RASTER)
        .AddAlias("dataset");

    AddOutputStringArg(&m_output);
    AddArg("stdout", 0,
           _("Directly output on stdout (format=text mode only). If enabled, "
             "output-string will be empty"),
           &m_stdout)
        .SetHiddenForCLI();
}

/************************************************************************/
/*                  GDALRasterInfoAlgorithm::RunImpl()                  */
/************************************************************************/

bool GDALRasterInfoAlgorithm::RunImpl(GDALProgressFunc, void *)
{
    CPLAssert(m_dataset.GetDatasetRef());

    CPLStringList aosOptions;
    if (m_format == "json")
        aosOptions.AddString("-json");
    if (m_minMax)
        aosOptions.AddString("-mm");
    if (m_stats)
        aosOptions.AddString("-stats");
    if (m_approxStats)
        aosOptions.AddString("-approx_stats");
    if (m_hist)
        aosOptions.AddString("-hist");
    if (m_noGCP)
        aosOptions.AddString("-nogcp");
    if (m_noMD)
        aosOptions.AddString("-nomd");
    if (m_noCT)
        aosOptions.AddString("-noct");
    if (m_noFL)
        aosOptions.AddString("-nofl");
    if (m_noMask)
        aosOptions.AddString("-nomask");
    if (m_noNodata)
        aosOptions.AddString("-nonodata");
    if (m_checksum)
        aosOptions.AddString("-checksum");
    if (m_listMDD)
        aosOptions.AddString("-listmdd");
    if (!m_mdd.empty())
    {
        aosOptions.AddString("-mdd");
        aosOptions.AddString(m_mdd.c_str());
    }

    GDALDatasetH hDS = GDALDataset::ToHandle(m_dataset.GetDatasetRef());
    std::unique_ptr<GDALDataset> poSubDataset;

    if (m_subDS > 0)
    {
        char **papszSubdatasets = GDALGetMetadata(hDS, "SUBDATASETS");
        const int nSubdatasets = CSLCount(papszSubdatasets) / 2;
        if (m_subDS > nSubdatasets)
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Invalid value for 'subdataset' argument. Should be "
                     "between 1 and %d",
                     nSubdatasets);
            return false;
        }

        char szKeyName[64];
        snprintf(szKeyName, sizeof(szKeyName), "SUBDATASET_%d_NAME", m_subDS);
        const std::string osSubDSName =
            CSLFetchNameValueDef(papszSubdatasets, szKeyName, "");

        poSubDataset.reset(GDALDataset::Open(
            osSubDSName.c_str(), GDAL_OF_RASTER | GDAL_OF_VERBOSE_ERROR,
            nullptr, nullptr, nullptr));
        if (!poSubDataset)
            return false;
        hDS = GDALDataset::ToHandle(poSubDataset.get());
    }

    if (m_stdout)
    {
        aosOptions.AddString("-stdout");
    }

    GDALInfoOptions *psOptions = GDALInfoOptionsNew(aosOptions.List(), nullptr);
    char *ret = GDALInfo(hDS, psOptions);
    GDALInfoOptionsFree(psOptions);
    const bool bOK = ret != nullptr;
    if (ret)
    {
        m_output = ret;
    }
    CPLFree(ret);

    return bOK;
}

//! @endcond
