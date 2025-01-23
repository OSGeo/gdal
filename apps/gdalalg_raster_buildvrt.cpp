/******************************************************************************
 *
 * Project:  GDAL
 * Purpose:  gdal "raster buildvrt" subcommand
 * Author:   Even Rouault <even dot rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2024, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include "gdalalg_raster_buildvrt.h"

#include "cpl_conv.h"
#include "cpl_vsi_virtual.h"

#include "gdal_priv.h"
#include "gdal_utils.h"

//! @cond Doxygen_Suppress

#ifndef _
#define _(x) (x)
#endif

/************************************************************************/
/*        GDALRasterBuildVRTAlgorithm::GDALRasterBuildVRTAlgorithm()    */
/************************************************************************/

GDALRasterBuildVRTAlgorithm::GDALRasterBuildVRTAlgorithm()
    : GDALAlgorithm(NAME, DESCRIPTION, HELP_URL)
{
    AddProgressArg();
    AddArg(GDAL_ARG_NAME_INPUT, 'i',
           _("Input raster datasets (or specify a @<filename> to point to a "
             "file containing filenames)"),
           &m_inputDatasets, GDAL_OF_RASTER)
        .SetPositional()
        .SetMinCount(1)
        .SetAutoOpenDataset(false)
        .SetMetaVar("INPUTS");
    AddOutputDatasetArg(&m_outputDataset, GDAL_OF_RASTER);
    AddCreationOptionsArg(&m_creationOptions);
    AddArg("band", 'b', _("Specify input band(s) number."), &m_bands);
    AddArg("separate", 0, _("Place each input file into a separate band."),
           &m_separate);
    AddOverwriteArg(&m_overwrite);
    {
        auto &arg = AddArg("resolution", 0,
                           _("Target resolution (in destination CRS units)"),
                           &m_resolution)
                        .SetMetaVar("<xres>,<yres>|average|highest|lowest");
        arg.AddValidationAction(
            [this, &arg]()
            {
                const std::string val = arg.Get<std::string>();
                if (val != "average" && val != "highest" && val != "lowest")
                {
                    const auto aosTokens =
                        CPLStringList(CSLTokenizeString2(val.c_str(), ",", 0));
                    if (aosTokens.size() != 2 ||
                        CPLGetValueType(aosTokens[0]) == CPL_VALUE_STRING ||
                        CPLGetValueType(aosTokens[1]) == CPL_VALUE_STRING ||
                        CPLAtof(aosTokens[0]) <= 0 ||
                        CPLAtof(aosTokens[1]) <= 0)
                    {
                        ReportError(CE_Failure, CPLE_AppDefined,
                                    "resolution: two comma separated positive "
                                    "values should be provided, or 'average', "
                                    "'highest' or 'lowest'");
                        return false;
                    }
                }
                return true;
            });
    }
    AddBBOXArg(&m_bbox, _("Target bounding box as xmin,ymin,xmax,ymax (in "
                          "destination CRS units)"));
    AddArg("target-aligned-pixels", 0,
           _("Round target extent to target resolution"),
           &m_targetAlignedPixels)
        .AddHiddenAlias("tap");
    AddArg("srcnodata", 0, _("Set nodata values for input bands."),
           &m_srcNoData)
        .SetMinCount(1)
        .SetRepeatedArgAllowed(false);
    AddArg("vrtnodata", 0, _("Set nodata values at the VRT band level."),
           &m_vrtNoData)
        .SetMinCount(1)
        .SetRepeatedArgAllowed(false);
    AddArg("hidenodata", 0, _("Makes the VRT band not report the NoData."),
           &m_hideNoData);
    AddArg("addalpha", 0,
           _("Adds an alpha mask band to the VRT when the source raster have "
             "none."),
           &m_addAlpha);
}

/************************************************************************/
/*                  GDALRasterBuildVRTAlgorithm::RunImpl()              */
/************************************************************************/

bool GDALRasterBuildVRTAlgorithm::RunImpl(GDALProgressFunc pfnProgress,
                                          void *pProgressData)
{
    if (m_outputDataset.GetDatasetRef())
    {
        ReportError(CE_Failure, CPLE_NotSupported,
                    "gdal raster buildvrt does not support outputting to an "
                    "already opened output dataset");
        return false;
    }

    std::vector<GDALDatasetH> ahInputDatasets;
    CPLStringList aosInputDatasetNames;
    bool foundByRef = false;
    bool foundByName = false;
    for (auto &ds : m_inputDatasets)
    {
        if (ds.GetDatasetRef())
        {
            foundByRef = true;
            ahInputDatasets.push_back(
                GDALDataset::ToHandle(ds.GetDatasetRef()));
        }
        else if (!ds.GetName().empty())
        {
            foundByName = true;
            if (ds.GetName()[0] == '@')
            {
                auto f = VSIVirtualHandleUniquePtr(
                    VSIFOpenL(ds.GetName().c_str() + 1, "r"));
                if (!f)
                {
                    ReportError(CE_Failure, CPLE_FileIO, "Cannot open %s",
                                ds.GetName().c_str() + 1);
                    return false;
                }
                while (const char *filename = CPLReadLineL(f.get()))
                {
                    aosInputDatasetNames.push_back(filename);
                }
            }
            else if (ds.GetName().find_first_of("*?[") != std::string::npos)
            {
                CPLStringList aosMatches(VSIGlob(ds.GetName().c_str(), nullptr,
                                                 pfnProgress, pProgressData));
                for (const char *pszStr : aosMatches)
                {
                    aosInputDatasetNames.push_back(pszStr);
                }
            }
            else
            {
                aosInputDatasetNames.push_back(ds.GetName().c_str());
            }
        }
    }
    if (foundByName && foundByRef)
    {
        ReportError(CE_Failure, CPLE_NotSupported,
                    "Input datasets should be provided either all by reference "
                    "or all by name");
        return false;
    }

    VSIStatBufL sStat;
    if (!m_overwrite && !m_outputDataset.GetName().empty() &&
        (VSIStatL(m_outputDataset.GetName().c_str(), &sStat) == 0 ||
         std::unique_ptr<GDALDataset>(
             GDALDataset::Open(m_outputDataset.GetName().c_str()))))
    {
        ReportError(CE_Failure, CPLE_AppDefined,
                    "File '%s' already exists. Specify the --overwrite "
                    "option to overwrite it.",
                    m_outputDataset.GetName().c_str());
        return false;
    }

    CPLStringList aosOptions;
    if (!m_resolution.empty())
    {
        const auto aosTokens =
            CPLStringList(CSLTokenizeString2(m_resolution.c_str(), ",", 0));
        if (aosTokens.size() == 2)
        {
            aosOptions.push_back("-tr");
            aosOptions.push_back(aosTokens[0]);
            aosOptions.push_back(aosTokens[1]);
        }
        else
        {
            aosOptions.push_back("-resolution");
            aosOptions.push_back(m_resolution);
        }
    }
    if (!m_bbox.empty())
    {
        aosOptions.push_back("-te");
        aosOptions.push_back(CPLSPrintf("%.17g", m_bbox[0]));
        aosOptions.push_back(CPLSPrintf("%.17g", m_bbox[1]));
        aosOptions.push_back(CPLSPrintf("%.17g", m_bbox[2]));
        aosOptions.push_back(CPLSPrintf("%.17g", m_bbox[3]));
    }
    if (m_targetAlignedPixels)
    {
        aosOptions.push_back("-tap");
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
    if (!m_vrtNoData.empty())
    {
        aosOptions.push_back("-vrtnodata");
        std::string s;
        for (double v : m_vrtNoData)
        {
            if (!s.empty())
                s += " ";
            s += CPLSPrintf("%.17g", v);
        }
        aosOptions.push_back(s);
    }
    if (m_separate)
    {
        aosOptions.push_back("-separate");
    }
    for (const auto &co : m_creationOptions)
    {
        aosOptions.push_back("-co");
        aosOptions.push_back(co);
    }
    for (const int b : m_bands)
    {
        aosOptions.push_back("-b");
        aosOptions.push_back(CPLSPrintf("%d", b));
    }
    if (m_addAlpha)
    {
        aosOptions.push_back("-addalpha");
    }
    if (m_hideNoData)
    {
        aosOptions.push_back("-hidenodata");
    }

    GDALBuildVRTOptions *psOptions =
        GDALBuildVRTOptionsNew(aosOptions.List(), nullptr);
    GDALBuildVRTOptionsSetProgress(psOptions, pfnProgress, pProgressData);

    auto poOutDS = std::unique_ptr<GDALDataset>(GDALDataset::FromHandle(
        GDALBuildVRT(m_outputDataset.GetName().c_str(),
                     foundByName ? aosInputDatasetNames.size()
                                 : static_cast<int>(m_inputDatasets.size()),
                     ahInputDatasets.empty() ? nullptr : ahInputDatasets.data(),
                     aosInputDatasetNames.List(), psOptions, nullptr)));
    GDALBuildVRTOptionsFree(psOptions);
    const bool bOK = poOutDS != nullptr;
    if (bOK)
    {
        m_outputDataset.Set(std::move(poOutDS));
    }

    return bOK;
}

//! @endcond
