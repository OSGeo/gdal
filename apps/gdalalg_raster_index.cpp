/******************************************************************************
 *
 * Project:  GDAL
 * Purpose:  gdal "raster index" subcommand
 * Author:   Even Rouault <even dot rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2025, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include "gdalalg_raster_index.h"

#include "cpl_conv.h"
#include "gdal_priv.h"
#include "gdal_utils_priv.h"
#include "ogrsf_frmts.h"

//! @cond Doxygen_Suppress

#ifndef _
#define _(x) (x)
#endif

/************************************************************************/
/*          GDALRasterIndexAlgorithm::GDALRasterIndexAlgorithm()        */
/************************************************************************/

GDALRasterIndexAlgorithm::GDALRasterIndexAlgorithm()
    : GDALVectorOutputAbstractAlgorithm(NAME, DESCRIPTION, HELP_URL)
{
    AddProgressArg();
    AddInputDatasetArg(&m_inputDatasets, GDAL_OF_RASTER)
        .SetAutoOpenDataset(false);
    GDALVectorOutputAbstractAlgorithm::AddAllOutputArgs();

    AddCommonOptions();

    AddArg("source-crs-field-name", 0,
           _("Name of the field to store the CRS of each dataset"),
           &m_sourceCrsName)
        .SetMinCharCount(1);
    AddArg("source-crs-format", 0,
           _("Format in which the CRS of each dataset must be written"),
           &m_sourceCrsFormat)
        .SetMinCharCount(1)
        .SetDefault(m_sourceCrsFormat)
        .SetChoices("auto", "WKT", "EPSG", "PROJ");
}

/************************************************************************/
/*          GDALRasterIndexAlgorithm::GDALRasterIndexAlgorithm()        */
/************************************************************************/

GDALRasterIndexAlgorithm::GDALRasterIndexAlgorithm(
    const std::string &name, const std::string &description,
    const std::string &helpURL)
    : GDALVectorOutputAbstractAlgorithm(name, description, helpURL)
{
}

/************************************************************************/
/*              GDALRasterIndexAlgorithm::AddCommonOptions()            */
/************************************************************************/

void GDALRasterIndexAlgorithm::AddCommonOptions()
{
    AddArg("recursive", 0,
           _("Whether input directories should be explored recursively."),
           &m_recursive);
    AddArg("filename-filter", 0,
           _("Pattern that the filenames in input directories should follow "
             "('*' and '?' wildcard)"),
           &m_filenameFilter);
    AddArg("min-pixel-size", 0,
           _("Minimum pixel size in term of geospatial extent per pixel "
             "(resolution) that a raster should have to be selected."),
           &m_minPixelSize)
        .SetMinValueExcluded(0);
    AddArg("max-pixel-size", 0,
           _("Maximum pixel size in term of geospatial extent per pixel "
             "(resolution) that a raster should have to be selected."),
           &m_maxPixelSize)
        .SetMinValueExcluded(0);
    AddArg("location-name", 0, _("Name of the field with the raster path"),
           &m_locationName)
        .SetDefault(m_locationName)
        .SetMinCharCount(1);
    AddArg("absolute-path", 0,
           _("Whether the path to the input datasets should be stored as an "
             "absolute path"),
           &m_writeAbsolutePaths);
    AddArg("dst-crs", 0, _("Destination CRS"), &m_crs)
        .SetIsCRSArg()
        .AddHiddenAlias("t_srs");

    {
        auto &arg =
            AddArg("metadata", 0, _("Add dataset metadata item"), &m_metadata)
                .SetMetaVar("<KEY>=<VALUE>")
                .SetPackedValuesAllowed(false);
        arg.AddValidationAction([this, &arg]()
                                { return ParseAndValidateKeyValue(arg); });
        arg.AddHiddenAlias("mo");
    }
}

/************************************************************************/
/*                   GDALRasterIndexAlgorithm::RunImpl()                */
/************************************************************************/

bool GDALRasterIndexAlgorithm::RunImpl(GDALProgressFunc pfnProgress,
                                       void *pProgressData)
{
    CPLStringList aosSources;
    for (auto &srcDS : m_inputDatasets)
    {
        if (srcDS.GetDatasetRef())
        {
            ReportError(
                CE_Failure, CPLE_IllegalArg,
                "Input datasets must be provided by name, not as object");
            return false;
        }
        aosSources.push_back(srcDS.GetName());
    }

    auto setupRet = SetupOutputDataset();
    if (!setupRet.outDS)
        return false;

    if (!SetDefaultOutputLayerNameIfNeeded(setupRet.outDS))
        return false;

    CPLStringList aosOptions;
    if (m_recursive)
    {
        aosOptions.push_back("-recursive");
    }
    for (const std::string &s : m_filenameFilter)
    {
        aosOptions.push_back("-filename_filter");
        aosOptions.push_back(s);
    }
    if (m_minPixelSize > 0)
    {
        aosOptions.push_back("-min_pixel_size");
        aosOptions.push_back(CPLSPrintf("%.17g", m_minPixelSize));
    }
    if (m_maxPixelSize > 0)
    {
        aosOptions.push_back("-max_pixel_size");
        aosOptions.push_back(CPLSPrintf("%.17g", m_maxPixelSize));
    }

    if (!m_outputLayerName.empty())
    {
        aosOptions.push_back("-lyr_name");
        aosOptions.push_back(m_outputLayerName);
    }

    aosOptions.push_back("-tileindex");
    aosOptions.push_back(m_locationName);

    if (m_writeAbsolutePaths)
    {
        aosOptions.push_back("-write_absolute_path");
    }
    if (m_crs.empty())
    {
        if (m_sourceCrsName.empty())
            aosOptions.push_back("-skip_different_projection");
    }
    else
    {
        aosOptions.push_back("-t_srs");
        aosOptions.push_back(m_crs);
    }
    if (!m_sourceCrsName.empty())
    {
        aosOptions.push_back("-src_srs_name");
        aosOptions.push_back(m_sourceCrsName);

        aosOptions.push_back("-src_srs_format");
        aosOptions.push_back(CPLString(m_sourceCrsFormat).toupper());
    }

    for (const std::string &s : m_metadata)
    {
        aosOptions.push_back("-mo");
        aosOptions.push_back(s);
    }

    if (!AddExtraOptions(aosOptions))
        return false;

    std::unique_ptr<GDALTileIndexOptions, decltype(&GDALTileIndexOptionsFree)>
        options(GDALTileIndexOptionsNew(aosOptions.List(), nullptr),
                GDALTileIndexOptionsFree);

    if (options)
    {
        GDALTileIndexOptionsSetProgress(options.get(), pfnProgress,
                                        pProgressData);
    }

    const bool ret =
        options && GDALTileIndexInternal(m_outputDataset.GetName().c_str(),
                                         GDALDataset::ToHandle(setupRet.outDS),
                                         OGRLayer::ToHandle(setupRet.layer),
                                         aosSources.size(), aosSources.List(),
                                         options.get(), nullptr) != nullptr;

    if (ret && setupRet.newDS)
    {
        m_outputDataset.Set(std::move(setupRet.newDS));
    }

    return ret;
}

//! @endcond
