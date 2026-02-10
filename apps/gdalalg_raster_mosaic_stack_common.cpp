/******************************************************************************
 *
 * Project:  GDAL
 * Purpose:  Common code of "raster mosaic" and "raster stack"
 * Author:   Even Rouault <even dot rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2025, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include "gdalalg_raster_mosaic_stack_common.h"
#include "gdalalg_raster_write.h"

#include "cpl_conv.h"
#include "cpl_vsi_virtual.h"

#include "gdal_priv.h"
#include "gdal_utils.h"

//! @cond Doxygen_Suppress

#ifndef _
#define _(x) (x)
#endif

/************************************************************************/
/*                       GetConstructorOptions()                        */
/************************************************************************/

/* static */ GDALRasterMosaicStackCommonAlgorithm::ConstructorOptions
GDALRasterMosaicStackCommonAlgorithm::GetConstructorOptions(bool standaloneStep)
{
    ConstructorOptions opts;
    opts.SetStandaloneStep(standaloneStep);
    opts.SetAutoOpenInputDatasets(false);
    opts.SetInputDatasetHelpMsg(
        _("Input raster datasets (or specify a @<filename> to point to a "
          "file containing filenames)"));
    opts.SetAddDefaultArguments(false);
    opts.SetInputDatasetMaxCount(INT_MAX);
    return opts;
}

/************************************************************************/
/*                GDALRasterMosaicStackCommonAlgorithm()                */
/************************************************************************/

GDALRasterMosaicStackCommonAlgorithm::GDALRasterMosaicStackCommonAlgorithm(
    const std::string &name, const std::string &description,
    const std::string &helpURL, bool bStandalone)
    : GDALRasterPipelineStepAlgorithm(name, description, helpURL,
                                      GetConstructorOptions(bStandalone))
{
    AddRasterInputArgs(/* openForMixedRasterVector = */ false,
                       /* hiddenForCLI = */ false);
    if (bStandalone)
    {
        AddProgressArg();
        AddRasterOutputArgs(false);
    }

    AddBandArg(&m_bands);
    AddAbsolutePathArg(
        &m_writeAbsolutePaths,
        _("Whether the path to the input datasets should be stored as an "
          "absolute path"));

    auto &resArg =
        AddArg("resolution", 0,
               _("Target resolution (in destination CRS units)"), &m_resolution)
            .SetDefault("same")
            .SetMetaVar("<xres>,<yres>|same|average|common|highest|lowest");
    resArg.AddValidationAction(
        [this, &resArg]()
        {
            const std::string val = resArg.Get<std::string>();
            if (val != "average" && val != "highest" && val != "lowest" &&
                val != "same" && val != "common")
            {
                const auto aosTokens =
                    CPLStringList(CSLTokenizeString2(val.c_str(), ",", 0));
                if (aosTokens.size() != 2 ||
                    CPLGetValueType(aosTokens[0]) == CPL_VALUE_STRING ||
                    CPLGetValueType(aosTokens[1]) == CPL_VALUE_STRING ||
                    CPLAtof(aosTokens[0]) <= 0 || CPLAtof(aosTokens[1]) <= 0)
                {
                    ReportError(CE_Failure, CPLE_AppDefined,
                                "resolution: two comma separated positive "
                                "values should be provided, or 'same', "
                                "'average', 'common', 'highest' or 'lowest'");
                    return false;
                }
            }
            return true;
        });

    AddBBOXArg(&m_bbox, _("Target bounding box as xmin,ymin,xmax,ymax (in "
                          "destination CRS units)"));
    auto &tapArg = AddArg("target-aligned-pixels", 0,
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

    AddValidationAction(
        [this, &resArg, &tapArg]()
        {
            if (tapArg.IsExplicitlySet() && !resArg.IsExplicitlySet())
            {
                ReportError(
                    CE_Failure, CPLE_IllegalArg,
                    "Argument 'target-aligned-pixels' can only be specified if "
                    "argument 'resolution' is also specified.");
                return false;
            }
            return true;
        });
}

/************************************************************************/
/*     GDALRasterMosaicStackCommonAlgorithm::GetInputDatasetNames()     */
/************************************************************************/

bool GDALRasterMosaicStackCommonAlgorithm::GetInputDatasetNames(
    GDALPipelineStepRunContext &ctxt,
    std::vector<GDALDatasetH> &ahInputDatasets,
    CPLStringList &aosInputDatasetNames, bool &foundByName)
{
    bool foundByRef = false;
    for (auto &ds : m_inputDataset)
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
                                                 ctxt.m_pfnProgress,
                                                 ctxt.m_pProgressData));
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

    return true;
}

/************************************************************************/
/*      GDALRasterMosaicStackCommonAlgorithm::SetBuildVRTOptions()      */
/************************************************************************/

void GDALRasterMosaicStackCommonAlgorithm::SetBuildVRTOptions(
    CPLStringList &aosOptions)
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
    for (const int b : m_bands)
    {
        aosOptions.push_back("-b");
        aosOptions.push_back(CPLSPrintf("%d", b));
    }
    if (m_hideNoData)
    {
        aosOptions.push_back("-hidenodata");
    }
    if (m_writeAbsolutePaths)
    {
        aosOptions.push_back("-write_absolute_path");
    }
}

/************************************************************************/
/*           GDALRasterMosaicStackCommonAlgorithm::RunImpl()            */
/************************************************************************/

bool GDALRasterMosaicStackCommonAlgorithm::RunImpl(GDALProgressFunc pfnProgress,
                                                   void *pProgressData)
{
    if (m_standaloneStep)
    {
        GDALRasterWriteAlgorithm writeAlg;
        for (auto &arg : writeAlg.GetArgs())
        {
            if (!arg->IsHidden())
            {
                auto stepArg = GetArg(arg->GetName());
                if (stepArg && stepArg->IsExplicitlySet())
                {
                    arg->SetSkipIfAlreadySet(true);
                    arg->SetFrom(*stepArg);
                }
            }
        }

        // Already checked by GDALAlgorithm::Run()
        CPLAssert(!m_executionForStreamOutput ||
                  EQUAL(m_format.c_str(), "stream"));

        m_standaloneStep = false;
        bool ret = Run(pfnProgress, pProgressData);
        m_standaloneStep = true;
        if (ret)
        {
            if (m_format == "stream")
            {
                ret = true;
            }
            else
            {
                std::vector<GDALArgDatasetValue> inputDataset(1);
                inputDataset[0].Set(m_outputDataset.GetDatasetRef());
                auto inputArg = writeAlg.GetArg(GDAL_ARG_NAME_INPUT);
                CPLAssert(inputArg);
                inputArg->Set(std::move(inputDataset));
                if (writeAlg.Run(pfnProgress, pProgressData))
                {
                    m_outputDataset.Set(
                        writeAlg.m_outputDataset.GetDatasetRef());
                    ret = true;
                }
            }
        }

        return ret;
    }
    else
    {
        GDALPipelineStepRunContext stepCtxt;
        stepCtxt.m_pfnProgress = pfnProgress;
        stepCtxt.m_pProgressData = pProgressData;
        return RunStep(stepCtxt);
    }
}

//! @endcond
