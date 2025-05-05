/******************************************************************************
 *
 * Project:  GDAL
 * Purpose:  gdal "raster stack" subcommand
 * Author:   Even Rouault <even dot rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2024, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include "gdalalg_raster_stack.h"

#include "cpl_conv.h"
#include "cpl_vsi_virtual.h"

#include "gdal_priv.h"
#include "gdal_utils.h"

//! @cond Doxygen_Suppress

#ifndef _
#define _(x) (x)
#endif

/************************************************************************/
/*        GDALRasterStackAlgorithm::GDALRasterStackAlgorithm()    */
/************************************************************************/

GDALRasterStackAlgorithm::GDALRasterStackAlgorithm()
    : GDALAlgorithm(NAME, DESCRIPTION, HELP_URL)
{
    m_supportsStreamedOutput = true;

    AddProgressArg();
    AddOutputFormatArg(&m_format, /* bStreamAllowed = */ true,
                       /* bGDALGAllowed = */ true);
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
    AddBandArg(&m_bands);
    AddOverwriteArg(&m_overwrite);
    {
        auto &arg =
            AddArg("resolution", 0,
                   _("Target resolution (in destination CRS units)"),
                   &m_resolution)
                .SetDefault("same")
                .SetMetaVar("<xres>,<yres>|same|average|common|highest|lowest");
        arg.AddValidationAction(
            [this, &arg]()
            {
                const std::string val = arg.Get<std::string>();
                if (val != "average" && val != "highest" && val != "lowest" &&
                    val != "same" && val != "common")
                {
                    const auto aosTokens =
                        CPLStringList(CSLTokenizeString2(val.c_str(), ",", 0));
                    if (aosTokens.size() != 2 ||
                        CPLGetValueType(aosTokens[0]) == CPL_VALUE_STRING ||
                        CPLGetValueType(aosTokens[1]) == CPL_VALUE_STRING ||
                        CPLAtof(aosTokens[0]) <= 0 ||
                        CPLAtof(aosTokens[1]) <= 0)
                    {
                        ReportError(
                            CE_Failure, CPLE_AppDefined,
                            "resolution: two comma-separated positive "
                            "values should be provided, or 'same', "
                            "'average', 'common', 'highest' or 'lowest'");
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
    AddArg("src-nodata", 0, _("Set nodata values for input bands."),
           &m_srcNoData)
        .SetMinCount(1)
        .SetRepeatedArgAllowed(false);
    AddArg("dst-nodata", 0,
           _("Set nodata values at the destination band level."), &m_dstNoData)
        .SetMinCount(1)
        .SetRepeatedArgAllowed(false);
    AddArg("hide-nodata", 0,
           _("Makes the destination band not report the NoData."),
           &m_hideNoData);
}

/************************************************************************/
/*                   GDALRasterStackAlgorithm::RunImpl()               */
/************************************************************************/

bool GDALRasterStackAlgorithm::RunImpl(GDALProgressFunc pfnProgress,
                                       void *pProgressData)
{
    CPLAssert(!m_outputDataset.GetDatasetRef());

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
                std::string osDatasetName = ds.GetName();
                if (!GetReferencePathForRelativePaths().empty())
                {
                    osDatasetName = GDALDataset::BuildFilename(
                        osDatasetName.c_str(),
                        GetReferencePathForRelativePaths().c_str(), true);
                }
                aosInputDatasetNames.push_back(osDatasetName.c_str());
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

    const bool bVRTOutput =
        m_outputDataset.GetName().empty() || EQUAL(m_format.c_str(), "VRT") ||
        EQUAL(m_format.c_str(), "stream") ||
        EQUAL(CPLGetExtensionSafe(m_outputDataset.GetName().c_str()).c_str(),
              "VRT");

    CPLStringList aosOptions;

    aosOptions.push_back("-strict");

    aosOptions.push_back("-program_name");
    aosOptions.push_back("gdal raster stack");

    aosOptions.push_back("-separate");

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
    if (!m_dstNoData.empty())
    {
        aosOptions.push_back("-vrtnodata");
        std::string s;
        for (double v : m_dstNoData)
        {
            if (!s.empty())
                s += " ";
            s += CPLSPrintf("%.17g", v);
        }
        aosOptions.push_back(s);
    }
    if (bVRTOutput)
    {
        for (const auto &co : m_creationOptions)
        {
            aosOptions.push_back("-co");
            aosOptions.push_back(co);
        }
    }
    for (const int b : m_bands)
    {
        aosOptions.push_back("-b");
        aosOptions.push_back(CPLSPrintf("%d", b));
    }
    if (m_hideNoData)
    {
        aosOptions.push_back("-hidenodata");
    }

    GDALBuildVRTOptions *psOptions =
        GDALBuildVRTOptionsNew(aosOptions.List(), nullptr);
    if (bVRTOutput)
    {
        GDALBuildVRTOptionsSetProgress(psOptions, pfnProgress, pProgressData);
    }

    auto poOutDS = std::unique_ptr<GDALDataset>(GDALDataset::FromHandle(
        GDALBuildVRT(EQUAL(m_format.c_str(), "stream") ? ""
                     : bVRTOutput ? m_outputDataset.GetName().c_str()
                                  : "",
                     foundByName ? aosInputDatasetNames.size()
                                 : static_cast<int>(m_inputDatasets.size()),
                     ahInputDatasets.empty() ? nullptr : ahInputDatasets.data(),
                     aosInputDatasetNames.List(), psOptions, nullptr)));
    GDALBuildVRTOptionsFree(psOptions);
    bool bOK = poOutDS != nullptr;
    if (bOK)
    {
        if (bVRTOutput)
        {
            m_outputDataset.Set(std::move(poOutDS));
        }
        else
        {
            CPLStringList aosTranslateOptions;
            if (!m_format.empty())
            {
                aosTranslateOptions.AddString("-of");
                aosTranslateOptions.AddString(m_format.c_str());
            }
            for (const auto &co : m_creationOptions)
            {
                aosTranslateOptions.AddString("-co");
                aosTranslateOptions.AddString(co.c_str());
            }

            GDALTranslateOptions *psTranslateOptions =
                GDALTranslateOptionsNew(aosTranslateOptions.List(), nullptr);
            GDALTranslateOptionsSetProgress(psTranslateOptions, pfnProgress,
                                            pProgressData);

            auto poFinalDS =
                std::unique_ptr<GDALDataset>(GDALDataset::FromHandle(
                    GDALTranslate(m_outputDataset.GetName().c_str(),
                                  GDALDataset::ToHandle(poOutDS.get()),
                                  psTranslateOptions, nullptr)));
            GDALTranslateOptionsFree(psTranslateOptions);

            bOK = poFinalDS != nullptr;
            if (bOK)
            {
                m_outputDataset.Set(std::move(poFinalDS));
            }
        }
    }

    return bOK;
}

//! @endcond
