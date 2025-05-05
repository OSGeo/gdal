/******************************************************************************
 *
 * Project:  GDAL
 * Purpose:  gdal "raster footprint" subcommand
 * Author:   Even Rouault <even dot rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2025, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include "gdalalg_raster_footprint.h"

#include "cpl_conv.h"
#include "cpl_vsi_virtual.h"

#include "gdal_priv.h"
#include "gdal_utils.h"

//! @cond Doxygen_Suppress

#ifndef _
#define _(x) (x)
#endif

/************************************************************************/
/*      GDALRasterFootprintAlgorithm::GDALRasterFootprintAlgorithm()    */
/************************************************************************/

GDALRasterFootprintAlgorithm::GDALRasterFootprintAlgorithm()
    : GDALAlgorithm(NAME, DESCRIPTION, HELP_URL)
{
    AddProgressArg();

    AddOpenOptionsArg(&m_openOptions);
    AddInputFormatsArg(&m_inputFormats)
        .AddMetadataItem(GAAMDI_REQUIRED_CAPABILITIES, {GDAL_DCAP_RASTER});
    AddInputDatasetArg(&m_inputDataset, GDAL_OF_RASTER);

    AddOutputDatasetArg(&m_outputDataset, GDAL_OF_VECTOR);
    AddOutputFormatArg(&m_format, /* bStreamAllowed = */ false,
                       /* bGDALGAllowed = */ false)
        .AddMetadataItem(GAAMDI_REQUIRED_CAPABILITIES,
                         {GDAL_DCAP_VECTOR, GDAL_DCAP_CREATE});
    AddArg("output-layer", 0, _("Output layer name"), &m_outputLayerName)
        .SetDefault(m_outputLayerName);
    AddCreationOptionsArg(&m_creationOptions);
    AddLayerCreationOptionsArg(&m_layerCreationOptions);
    AddAppendUpdateArg(&m_append);
    AddOverwriteArg(&m_overwrite);

    AddBandArg(&m_bands);
    AddArg("combine-bands", 0,
           _("Defines how the mask bands of the selected bands are combined to "
             "generate a single mask band, before being vectorized."),
           &m_combineBands)
        .SetChoices("union", "intersection")
        .SetDefault(m_combineBands);
    AddArg("overview", 0, _("Which overview level of source file must be used"),
           &m_overview)
        .SetMutualExclusionGroup("overview-srcnodata")
        .SetMinValueIncluded(0);
    AddArg("src-nodata", 0, _("Set nodata values for input bands."),
           &m_srcNoData)
        .SetMinCount(1)
        .SetRepeatedArgAllowed(false)
        .SetMutualExclusionGroup("overview-srcnodata");
    AddArg("coordinate-system", 0, _("Target coordinate system"),
           &m_coordinateSystem)
        .SetChoices("georeferenced", "pixel");
    AddArg("dst-crs", 0, _("Destination CRS"), &m_dstCrs)
        .SetIsCRSArg()
        .AddHiddenAlias("t_srs");
    AddArg("split-multipolygons", 0,
           _("Whether to split multipolygons as several features each with one "
             "single polygon"),
           &m_splitMultiPolygons);
    AddArg("convex-hull", 0,
           _("Whether to compute the convex hull of the footprint"),
           &m_convexHull);
    AddArg("densify-distance", 0,
           _("Maximum distance between 2 consecutive points of the output "
             "geometry."),
           &m_densifyVal)
        .SetMinValueExcluded(0);
    AddArg(
        "simplify-tolerance", 0,
        _("Tolerance used to merge consecutive points of the output geometry."),
        &m_simplifyVal)
        .SetMinValueExcluded(0);
    AddArg("min-ring-area", 0, _("Minimum value for the area of a ring"),
           &m_minRingArea)
        .SetMinValueIncluded(0);
    AddArg("max-points", 0,
           _("Maximum number of points of each output geometry"), &m_maxPoints)
        .SetDefault(m_maxPoints)
        .AddValidationAction(
            [this]()
            {
                if (m_maxPoints != "unlimited")
                {
                    char *endptr = nullptr;
                    const auto nVal =
                        std::strtoll(m_maxPoints.c_str(), &endptr, 10);
                    if (nVal < 4 ||
                        endptr != m_maxPoints.c_str() + m_maxPoints.size())
                    {
                        ReportError(
                            CE_Failure, CPLE_IllegalArg,
                            "Value of 'max-points' should be a positive "
                            "integer greater or equal to 4, or 'unlimited'");
                        return false;
                    }
                }
                return true;
            });
    AddArg("location-field", 0,
           _("Name of the field where the path of the input dataset will be "
             "stored."),
           &m_locationField)
        .SetDefault(m_locationField)
        .SetMutualExclusionGroup("location");
    AddArg("no-location-field", 0,
           _("Disable creating a field with the path of the input dataset"),
           &m_noLocation)
        .SetMutualExclusionGroup("location");
    AddArg("absolute-path", 0,
           _("Whether the path to the input dataset should be stored as an "
             "absolute path"),
           &m_writeAbsolutePaths);

    AddValidationAction(
        [this]
        {
            if (auto poSrcDS = m_inputDataset.GetDatasetRef())
            {
                const int nOvrCount =
                    poSrcDS->GetRasterBand(1)->GetOverviewCount();
                if (m_overview >= 0 && poSrcDS->GetRasterCount() > 0 &&
                    m_overview >= nOvrCount)
                {
                    if (nOvrCount == 0)
                    {
                        ReportError(
                            CE_Failure, CPLE_IllegalArg,
                            "Source dataset has no overviews. "
                            "Argument 'overview' should not be specified.");
                    }
                    else
                    {
                        ReportError(
                            CE_Failure, CPLE_IllegalArg,
                            "Source dataset has only %d overview levels. "
                            "'overview' "
                            "value should be strictly lower than this number.",
                            nOvrCount);
                    }
                    return false;
                }
            }
            return true;
        });
}

/************************************************************************/
/*                 GDALRasterFootprintAlgorithm::RunImpl()              */
/************************************************************************/

bool GDALRasterFootprintAlgorithm::RunImpl(GDALProgressFunc pfnProgress,
                                           void *pProgressData)
{
    GDALDatasetH hDstDS =
        GDALDataset::ToHandle(m_outputDataset.GetDatasetRef());

    const bool dstDSWasNull{!hDstDS};

    if (!hDstDS && !m_outputDataset.GetName().empty())
    {
        VSIStatBufL sStat;
        bool fileExists{VSIStatL(m_outputDataset.GetName().c_str(), &sStat) ==
                        0};

        {
            CPLErrorStateBackuper oCPLErrorHandlerPusher(CPLQuietErrorHandler);
            hDstDS = GDALOpenEx(m_outputDataset.GetName().c_str(),
                                GDAL_OF_VECTOR | GDAL_OF_VERBOSE_ERROR |
                                    GDAL_OF_UPDATE,
                                nullptr, nullptr, nullptr);
            CPLErrorReset();
        }

        if ((hDstDS || fileExists) && !m_overwrite && !m_append)
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Dataset '%s' already exists. Specify the --overwrite "
                     "option to overwrite it or the --append option to "
                     "append to it.",
                     m_outputDataset.GetName().c_str());
            GDALClose(hDstDS);
            return false;
        }

        if (hDstDS && fileExists && m_overwrite)
        {
            // Delete the existing file
            GDALClose(hDstDS);
            hDstDS = nullptr;
            if (VSIUnlink(m_outputDataset.GetName().c_str()) != 0)
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "Failed to delete existing dataset '%s'.",
                         m_outputDataset.GetName().c_str());
                return false;
            }
        }
    }

    CPLStringList aosOptions;
    for (int band : m_bands)
    {
        aosOptions.push_back("-b");
        aosOptions.push_back(CPLSPrintf("%d", band));
    }

    aosOptions.push_back("-combine_bands");
    aosOptions.push_back(m_combineBands);

    if (m_overview >= 0)
    {
        aosOptions.push_back("-ovr");
        aosOptions.push_back(CPLSPrintf("%d", m_overview));
    }

    if (!m_srcNoData.empty())
    {
        aosOptions.push_back("-srcnodata");
        std::string s;
        for (double v : m_srcNoData)
        {
            if (!s.empty())
                s += " ";
            s += CPLSPrintf("%.17g", v);
        }
        aosOptions.push_back(s);
    }

    if (m_coordinateSystem == "pixel")
    {
        aosOptions.push_back("-t_cs");
        aosOptions.push_back("pixel");
    }
    else if (m_coordinateSystem == "georeferenced")
    {
        aosOptions.push_back("-t_cs");
        aosOptions.push_back("georef");
    }

    if (!m_dstCrs.empty())
    {
        aosOptions.push_back("-t_srs");
        aosOptions.push_back(m_dstCrs);
    }

    if (!m_format.empty())
    {
        aosOptions.push_back("-of");
        aosOptions.push_back(m_format.c_str());
    }

    for (const auto &co : m_creationOptions)
    {
        aosOptions.push_back("-dsco");
        aosOptions.push_back(co.c_str());
    }

    for (const auto &co : m_layerCreationOptions)
    {
        aosOptions.push_back("-lco");
        aosOptions.push_back(co.c_str());
    }

    if (GetArg("output-layer")->IsExplicitlySet())
    {
        aosOptions.push_back("-lyr_name");
        aosOptions.push_back(m_outputLayerName.c_str());
    }

    if (m_splitMultiPolygons)
        aosOptions.push_back("-split_polys");

    if (m_convexHull)
        aosOptions.push_back("-convex_hull");

    if (m_densifyVal > 0)
    {
        aosOptions.push_back("-densify");
        aosOptions.push_back(CPLSPrintf("%.17g", m_densifyVal));
    }

    if (m_simplifyVal > 0)
    {
        aosOptions.push_back("-simplify");
        aosOptions.push_back(CPLSPrintf("%.17g", m_simplifyVal));
    }

    aosOptions.push_back("-min_ring_area");
    aosOptions.push_back(CPLSPrintf("%.17g", m_minRingArea));

    aosOptions.push_back("-max_points");
    aosOptions.push_back(m_maxPoints);

    if (m_noLocation)
    {
        aosOptions.push_back("-no_location");
    }
    else
    {
        aosOptions.push_back("-location_field_name");
        aosOptions.push_back(m_locationField);

        if (m_writeAbsolutePaths)
            aosOptions.push_back("-write_absolute_path");
    }

    std::unique_ptr<GDALFootprintOptions, decltype(&GDALFootprintOptionsFree)>
        psOptions{GDALFootprintOptionsNew(aosOptions.List(), nullptr),
                  GDALFootprintOptionsFree};
    if (!psOptions)
        return false;

    GDALFootprintOptionsSetProgress(psOptions.get(), pfnProgress,
                                    pProgressData);

    GDALDatasetH hSrcDS = GDALDataset::ToHandle(m_inputDataset.GetDatasetRef());
    auto poRetDS = GDALDataset::FromHandle(
        GDALFootprint(m_outputDataset.GetName().c_str(), hDstDS, hSrcDS,
                      psOptions.get(), nullptr));
    if (!poRetDS)
        return false;

    if (!hDstDS)
    {
        m_outputDataset.Set(std::unique_ptr<GDALDataset>(poRetDS));
    }
    else if (dstDSWasNull)
    {
        if (GDALClose(hDstDS) != CE_None)
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Failed to close output dataset");
            return false;
        }
    }

    return true;
}

//! @endcond
