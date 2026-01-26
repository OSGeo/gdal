/******************************************************************************
 *
 * Project:  GDAL
 * Purpose:  gdal "raster viewshed" subcommand
 * Author:   Even Rouault <even dot rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2025, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include "gdalalg_raster_viewshed.h"

#include "cpl_conv.h"
#include "cpl_vsi_virtual.h"

#include "commonutils.h"
#include "gdal_priv.h"

#include "viewshed/cumulative.h"
#include "viewshed/viewshed.h"

#include <algorithm>

//! @cond Doxygen_Suppress

#ifndef _
#define _(x) (x)
#endif

/************************************************************************/
/*      GDALRasterViewshedAlgorithm::GDALRasterViewshedAlgorithm()      */
/************************************************************************/

GDALRasterViewshedAlgorithm::GDALRasterViewshedAlgorithm(bool standaloneStep)
    : GDALRasterPipelineNonNativelyStreamingAlgorithm(NAME, DESCRIPTION,
                                                      HELP_URL, standaloneStep)
{
    AddArg("position", 'p', _("Observer position"), &m_observerPos)
        .AddAlias("pos")
        .SetMetaVar("<X,Y> or <X,Y,H>")
        .SetMinCount(2)
        .SetMaxCount(3)
        .SetRepeatedArgAllowed(false);
    AddArg("height", 'z', _("Observer height"), &m_opts.observer.z);

    auto &sdFilenameArg =
        AddArg("sd-filename", 0, _("Filename of standard-deviation raster"),
               &m_sdFilename, GDAL_OF_RASTER);
    SetAutoCompleteFunctionForFilename(sdFilenameArg, GDAL_OF_RASTER);

    AddArg("target-height", 0,
           _("Height of the target above the DEM surface in the height unit of "
             "the DEM."),
           &m_opts.targetHeight)
        .SetDefault(m_opts.targetHeight);
    AddArg("mode", 0, _("Sets what information the output contains."),
           &m_outputMode)
        .SetChoices("normal", "DEM", "ground", "cumulative")
        .SetDefault(m_outputMode);

    AddArg("max-distance", 0,
           _("Maximum distance from observer to compute visibility. It is also "
             "used to clamp the extent of the output raster."),
           &m_opts.maxDistance)
        .SetMinValueIncluded(0);
    AddArg("min-distance", 0,
           _("Mask all cells less than this distance from the observer. Must "
             "be less "
             "than 'max-distance'."),
           &m_opts.minDistance)
        .SetMinValueIncluded(0);

    AddArg("start-angle", 0,
           _("Mask all cells outside of the arc ('start-angle', 'end-angle'). "
             "Clockwise degrees "
             "from north. Also used to clamp the extent of the output raster."),
           &m_opts.startAngle)
        .SetMinValueIncluded(0)
        .SetMaxValueExcluded(360);
    AddArg("end-angle", 0,
           _("Mask all cells outside of the arc ('start-angle', 'end-angle'). "
             "Clockwise degrees "
             "from north. Also used to clamp the extent of the output raster."),
           &m_opts.endAngle)
        .SetMinValueIncluded(0)
        .SetMaxValueExcluded(360);

    AddArg("high-pitch", 0,
           _("Mark all cells out-of-range where the observable height would be "
             "higher than the "
             "'high-pitch' angle from the observer. Degrees from horizontal - "
             "positive is up. "
             "Must be greater than 'low-pitch'."),
           &m_opts.highPitch)
        .SetMaxValueIncluded(90)
        .SetMinValueExcluded(-90);
    AddArg("low-pitch", 0,
           _("Bound observable height to be no lower than the 'low-pitch' "
             "angle from the observer. "
             "Degrees from horizontal - positive is up. Must be less than "
             "'high-pitch'."),
           &m_opts.lowPitch)
        .SetMaxValueExcluded(90)
        .SetMinValueIncluded(-90);

    AddArg("curvature-coefficient", 0,
           _("Coefficient to consider the effect of the curvature and "
             "refraction."),
           &m_opts.curveCoeff)
        .SetMinValueIncluded(0);

    AddBandArg(&m_band).SetDefault(m_band);
    AddArg("visible-value", 0, _("Pixel value to set for visible areas"),
           &m_opts.visibleVal)
        .SetDefault(m_opts.visibleVal)
        .SetMinValueIncluded(0)
        .SetMaxValueIncluded(255);
    AddArg("invisible-value", 0, _("Pixel value to set for invisible areas"),
           &m_opts.invisibleVal)
        .SetDefault(m_opts.invisibleVal)
        .SetMinValueIncluded(0)
        .SetMaxValueIncluded(255);
    AddArg("maybe-visible-value", 0,
           _("Pixel value to set for potentially visible areas"),
           &m_opts.maybeVisibleVal)
        .SetDefault(m_opts.maybeVisibleVal)
        .SetMinValueIncluded(0)
        .SetMaxValueIncluded(255);
    AddArg("out-of-range-value", 0,
           _("Pixel value to set for the cells that fall outside of the range "
             "specified by the observer location and the maximum distance"),
           &m_opts.outOfRangeVal)
        .SetDefault(m_opts.outOfRangeVal)
        .SetMinValueIncluded(0)
        .SetMaxValueIncluded(255);
    AddArg("dst-nodata", 0,
           _("The value to be set for the cells in the output raster that have "
             "no data."),
           &m_opts.nodataVal)
        .SetMinValueIncluded(0)
        .SetMaxValueIncluded(255);
    AddArg("observer-spacing", 0, _("Cell Spacing between observers"),
           &m_opts.observerSpacing)
        .SetDefault(m_opts.observerSpacing)
        .SetMinValueIncluded(1);

    m_numThreadsStr = std::to_string(m_numThreads);
    AddNumThreadsArg(&m_numThreads, &m_numThreadsStr);
}

/************************************************************************/
/*                GDALRasterViewshedAlgorithm::RunStep()                */
/************************************************************************/

bool GDALRasterViewshedAlgorithm::RunStep(GDALPipelineStepRunContext &ctxt)
{
    auto pfnProgress = ctxt.m_pfnProgress;
    auto pProgressData = ctxt.m_pProgressData;
    auto poSrcDS = m_inputDataset[0].GetDatasetRef();
    CPLAssert(poSrcDS);
    CPLAssert(!m_outputDataset.GetDatasetRef());

    GDALRasterBandH sdBand = nullptr;
    if (auto sdDataset = m_sdFilename.GetDatasetRef())
    {
        if (sdDataset->GetRasterCount() == 0)
        {
            ReportError(
                CE_Failure, CPLE_AppDefined,
                "The standard deviation dataset must have one raster band");
            return false;
        }
        sdBand = GDALRasterBand::FromHandle(sdDataset->GetRasterBand(1));
    }

    if (GetArg("height")->IsExplicitlySet())
    {
        if (m_observerPos.size() == 3)
        {
            ReportError(CE_Failure, CPLE_AppDefined,
                        "Height can't be specified in both 'position' and "
                        "'height' arguments");
            return false;
        }
    }

    if (m_observerPos.size())
    {
        m_opts.observer.x = m_observerPos[0];
        m_opts.observer.y = m_observerPos[1];
        if (m_observerPos.size() == 3)
            m_opts.observer.z = m_observerPos[2];
        else
            m_opts.observer.z = 2;
    }

    if (!GetArg("curvature-coefficient")->IsExplicitlySet())
    {
        m_opts.curveCoeff = gdal::viewshed::adjustCurveCoeff(
            m_opts.curveCoeff, GDALDataset::ToHandle(poSrcDS));
    }

    if (m_outputMode == "normal")
        m_opts.outputMode = gdal::viewshed::OutputMode::Normal;
    else if (m_outputMode == "DEM")
        m_opts.outputMode = gdal::viewshed::OutputMode::DEM;
    else if (m_outputMode == "ground")
        m_opts.outputMode = gdal::viewshed::OutputMode::Ground;
    else if (m_outputMode == "cumulative")
        m_opts.outputMode = gdal::viewshed::OutputMode::Cumulative;

    m_opts.numJobs = static_cast<uint8_t>(std::clamp(m_numThreads, 0, 255));

    m_opts.outputFilename =
        CPLGenerateTempFilenameSafe(
            CPLGetBasenameSafe(poSrcDS->GetDescription()).c_str()) +
        ".tif";
    m_opts.outputFormat = "GTiff";

    if (m_opts.outputMode == gdal::viewshed::OutputMode::Cumulative)
    {
        static const std::vector<std::string> badArgs{
            "visible-value", "invisible-value", "max-distance",
            "min-distance",  "start-angle",     "end-angle",
            "low-pitch",     "high-pitch",      "position"};

        for (const auto &arg : badArgs)
            if (GetArg(arg)->IsExplicitlySet())
            {
                std::string err =
                    "Option '" + arg + "' can't be used in cumulative mode.";
                ReportError(CE_Failure, CPLE_AppDefined, "%s", err.c_str());
                return false;
            }

        auto poSrcDriver = poSrcDS->GetDriver();
        if (EQUAL(poSrcDS->GetDescription(), "") || !poSrcDriver ||
            EQUAL(poSrcDriver->GetDescription(), "MEM"))
        {
            ReportError(
                CE_Failure, CPLE_AppDefined,
                "In cumulative mode, the input dataset must be opened by name");
            return false;
        }
        gdal::viewshed::Cumulative oViewshed(m_opts);
        const bool bSuccess = oViewshed.run(
            m_inputDataset[0].GetName().c_str(),
            pfnProgress ? pfnProgress : GDALDummyProgress, pProgressData);
        if (bSuccess)
        {
            m_outputDataset.Set(std::unique_ptr<GDALDataset>(
                GDALDataset::Open(m_opts.outputFilename.c_str(),
                                  GDAL_OF_RASTER | GDAL_OF_VERBOSE_ERROR,
                                  nullptr, nullptr, nullptr)));
        }
    }
    else
    {
        static const std::vector<std::string> badArgs{
            "observer-spacing", GDAL_ARG_NAME_NUM_THREADS};
        for (const auto &arg : badArgs)
            if (GetArg(arg)->IsExplicitlySet())
            {
                std::string err =
                    "Option '" + arg + "' can't be used in standard mode.";
                ReportError(CE_Failure, CPLE_AppDefined, "%s", err.c_str());
                return false;
            }
        static const std::vector<std::string> goodArgs{"position"};
        for (const auto &arg : goodArgs)
            if (!GetArg(arg)->IsExplicitlySet())
            {
                std::string err =
                    "Option '" + arg + "' must be specified in standard mode.";
                ReportError(CE_Failure, CPLE_AppDefined, "%s", err.c_str());
                return false;
            }

        gdal::viewshed::Viewshed oViewshed(m_opts);
        const bool bSuccess = oViewshed.run(
            GDALRasterBand::ToHandle(poSrcDS->GetRasterBand(m_band)), sdBand,
            pfnProgress ? pfnProgress : GDALDummyProgress, pProgressData);
        if (bSuccess)
        {
            m_outputDataset.Set(oViewshed.output());
        }
    }

    auto poOutDS = m_outputDataset.GetDatasetRef();
    if (poOutDS && poOutDS->GetDescription()[0])
    {
        // In file systems that allow it (all but Windows...), we want to
        // delete the temporary file as soon as soon as possible after
        // having open it, so that if someone kills the process there are
        // no temp files left over. If that unlink() doesn't succeed
        // (on Windows), then the file will eventually be deleted when
        // poTmpDS is cleaned due to MarkSuppressOnClose().
        VSIUnlink(poOutDS->GetDescription());
        poOutDS->MarkSuppressOnClose();
    }

    return poOutDS != nullptr;
}

GDALRasterViewshedAlgorithmStandalone::
    ~GDALRasterViewshedAlgorithmStandalone() = default;

//! @endcond
