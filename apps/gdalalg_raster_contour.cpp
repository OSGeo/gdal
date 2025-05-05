/******************************************************************************
 *
 * Project:  GDAL
 * Purpose:  gdal "raster contour" subcommand
 * Author:   Alessandro Pasotti <elpaso at itopen dot it>
 *
 ******************************************************************************
 * Copyright (c) 2025, Alessandro Pasotti <elpaso at itopen dot it>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include <cmath>

#include "gdalalg_raster_contour.h"

#include "cpl_conv.h"
#include "gdal_priv.h"
#include "gdal_utils.h"
#include "gdal_alg.h"
#include "gdal_utils_priv.h"

//! @cond Doxygen_Suppress

#ifndef _
#define _(x) (x)
#endif

/************************************************************************/
/*          GDALRasterContourAlgorithm::GDALRasterContourAlgorithm()    */
/************************************************************************/

GDALRasterContourAlgorithm::GDALRasterContourAlgorithm()
    : GDALAlgorithm(NAME, DESCRIPTION, HELP_URL), m_outputLayerName("contour"),
      m_elevAttributeName(""), m_amin(""), m_amax(""), m_levels{}
{

    AddProgressArg();
    AddOutputFormatArg(&m_outputFormat)
        .AddMetadataItem(GAAMDI_REQUIRED_CAPABILITIES,
                         {GDAL_DCAP_VECTOR, GDAL_DCAP_CREATE});
    AddOpenOptionsArg(&m_openOptions);
    AddInputFormatsArg(&m_inputFormats)
        .AddMetadataItem(GAAMDI_REQUIRED_CAPABILITIES, {GDAL_DCAP_RASTER});
    AddInputDatasetArg(&m_inputDataset, GDAL_OF_RASTER);
    AddOutputDatasetArg(&m_outputDataset, GDAL_OF_VECTOR);
    AddCreationOptionsArg(&m_creationOptions);
    AddLayerCreationOptionsArg(&m_layerCreationOptions);

    // gdal_contour specific options
    AddBandArg(&m_band).SetDefault(1);
    AddLayerNameArg(&m_outputLayerName).AddAlias("nln");
    AddArg("elevation-name", 0, _("Name of the elevation field"),
           &m_elevAttributeName);
    AddArg("min-name", 0, _("Name of the minimum elevation field"), &m_amin);
    AddArg("max-name", 0, _("Name of the maximum elevation field"), &m_amax);
    AddArg("3d", 0, _("Force production of 3D vectors instead of 2D"), &m_3d);

    AddArg("src-nodata", 0, _("Input pixel value to treat as 'nodata'"),
           &m_sNodata);
    AddArg("interval", 0, _("Elevation interval between contours"), &m_interval)
        .SetMutualExclusionGroup("levels")
        .SetMinValueExcluded(0);
    AddArg("levels", 0, _("List of contour levels"), &m_levels)
        .SetMutualExclusionGroup("levels");
    AddArg("exp-base", 'e', _("Base for exponential contour level generation"),
           &m_expBase)
        .SetMutualExclusionGroup("levels");
    AddArg("offset", 0, _("Offset to apply to contour levels"), &m_offset)
        .AddAlias("off");
    AddArg("polygonize", 'p', _("Create polygons instead of lines"),
           &m_polygonize);
    AddArg("group-transactions", 0,
           _("Group n features per transaction (default 100 000)"),
           &m_groupTransactions)
        .SetMinValueIncluded(0);
    AddOverwriteArg(&m_overwrite);
}

/************************************************************************/
/*                  GDALRasterContourAlgorithm::RunImpl()               */
/************************************************************************/

bool GDALRasterContourAlgorithm::RunImpl(GDALProgressFunc pfnProgress,
                                         void *pProgressData)
{

    CPLErrorReset();

    CPLAssert(m_inputDataset.GetDatasetRef());
    CPLAssert(!m_outputDataset.GetDatasetRef());

    CPLStringList aosOptions;
    if (!m_outputFormat.empty())
    {
        aosOptions.AddString("-of");
        aosOptions.AddString(m_outputFormat);
    }

    for (const auto &co : m_creationOptions)
    {
        aosOptions.AddString("-co");
        aosOptions.AddString(co);
    }

    for (const auto &co : m_layerCreationOptions)
    {
        aosOptions.AddString("-lco");
        aosOptions.AddString(co);
    }

    if (m_band > 0)
    {
        aosOptions.AddString("-b");
        aosOptions.AddString(CPLSPrintf("%d", m_band));
    }
    if (!m_elevAttributeName.empty())
    {
        aosOptions.AddString("-a");
        aosOptions.AddString(m_elevAttributeName);
    }
    if (!m_amin.empty())
    {
        aosOptions.AddString("-amin");
        aosOptions.AddString(m_amin);
    }
    if (!m_amax.empty())
    {
        aosOptions.AddString("-amax");
        aosOptions.AddString(m_amax);
    }
    if (m_3d)
    {
        aosOptions.AddString("-3d");
    }
    if (!std::isnan(m_sNodata))
    {
        aosOptions.AddString("-snodata");
        aosOptions.AddString(CPLSPrintf("%.16g", m_sNodata));
    }
    if (m_levels.size() > 0)
    {
        for (const auto &level : m_levels)
        {
            aosOptions.AddString("-fl");
            aosOptions.AddString(level);
        }
    }
    if (!std::isnan(m_interval))
    {
        aosOptions.AddString("-i");
        aosOptions.AddString(CPLSPrintf("%.16g", m_interval));
    }
    if (m_expBase > 0)
    {
        aosOptions.AddString("-e");
        aosOptions.AddString(CPLSPrintf("%d", m_expBase));
    }
    if (!std::isnan(m_offset))
    {
        aosOptions.AddString("-off");
        aosOptions.AddString(CPLSPrintf("%.16g", m_offset));
    }
    if (m_polygonize)
    {
        aosOptions.AddString("-p");
    }
    if (!m_outputLayerName.empty())
    {
        aosOptions.AddString("-nln");
        aosOptions.AddString(m_outputLayerName);
    }

    // Check that one of --interval, --levels, --exp-base is specified
    if (m_levels.size() == 0 && std::isnan(m_interval) && m_expBase == 0)
    {
        ReportError(
            CE_Failure, CPLE_AppDefined,
            "One of 'interval', 'levels', 'exp-base' must be specified.");
        return false;
    }

    // Check that interval is not negative
    if (!std::isnan(m_interval) && m_interval < 0)
    {
        ReportError(CE_Failure, CPLE_AppDefined,
                    "Interval must be a positive number.");
        return false;
    }

    aosOptions.AddString(m_inputDataset.GetName());
    aosOptions.AddString(m_outputDataset.GetName());

    GDALContourOptionsForBinary optionsForBinary;
    std::unique_ptr<GDALContourOptions, decltype(&GDALContourOptionsFree)>
        psOptions{GDALContourOptionsNew(aosOptions.List(), &optionsForBinary),
                  GDALContourOptionsFree};

    if (!psOptions)
    {
        return false;
    }

    GDALDatasetH hSrcDS{m_inputDataset.GetDatasetRef()};
    GDALRasterBandH hBand{nullptr};
    GDALDatasetH hDstDS{m_outputDataset.GetDatasetRef()};
    OGRLayerH hLayer{nullptr};
    char **papszStringOptions = nullptr;

    CPLErr eErr =
        GDALContourProcessOptions(psOptions.get(), &papszStringOptions, &hSrcDS,
                                  &hBand, &hDstDS, &hLayer);

    if (eErr == CE_None)
    {
        eErr = GDALContourGenerateEx(hBand, hLayer, papszStringOptions,
                                     pfnProgress, pProgressData);
    }

    CSLDestroy(papszStringOptions);

    auto poDstDS = GDALDataset::FromHandle(hDstDS);
    m_outputDataset.Set(std::unique_ptr<GDALDataset>(poDstDS));

    return eErr == CE_None;
}

//! @endcond
