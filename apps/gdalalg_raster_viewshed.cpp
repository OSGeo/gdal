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
/*       GDALRasterViewshedAlgorithm::GDALRasterViewshedAlgorithm()     */
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
        .SetRequired()
        .SetRepeatedArgAllowed(false);
    AddArg("target-height", 0,
           _("Height of the target above the DEM surface in the height unit of "
             "the DEM."),
           &m_targetHeight)
        .SetDefault(m_targetHeight);
    AddArg("mode", 0, _("Sets what information the output contains."),
           &m_outputMode)
        .SetChoices("normal", "DEM", "ground", "cumulative")
        .SetDefault(m_outputMode);

    AddArg("max-distance", 0,
           _("Maximum distance from observer to compute visibility. It is also "
             "used to clamp the extent of the output raster."),
           &m_maxDistance)
        .SetMinValueIncluded(0);
    AddArg("curvature-coefficient", 0,
           _("Coefficient to consider the effect of the curvature and "
             "refraction."),
           &m_curveCoefficient)
        .SetMinValueIncluded(0);

    AddBandArg(&m_band).SetDefault(m_band);
    AddArg("visible-value", 0, _("Pixel value to set for visible areas"),
           &m_visibleVal)
        .SetDefault(m_visibleVal)
        .SetMinValueIncluded(0)
        .SetMaxValueIncluded(255);
    AddArg("invisible-value", 0, _("Pixel value to set for invisible areas"),
           &m_invisibleVal)
        .SetDefault(m_invisibleVal)
        .SetMinValueIncluded(0)
        .SetMaxValueIncluded(255);
    AddArg("out-of-range-value", 0,
           _("Pixel value to set for the cells that fall outside of the range "
             "specified by the observer location and the maximum distance"),
           &m_outOfRangeVal)
        .SetDefault(m_outOfRangeVal)
        .SetMinValueIncluded(0)
        .SetMaxValueIncluded(255);
    AddArg("dst-nodata", 0,
           _("The value to be set for the cells in the output raster that have "
             "no data."),
           &m_dstNoData)
        .SetMinValueIncluded(0)
        .SetMaxValueIncluded(255);
    AddArg("observer-spacing", 0, _("Cell Spacing between observers"),
           &m_observerSpacing)
        .SetDefault(m_observerSpacing)
        .SetMinValueIncluded(1);

    m_numThreadsStr = std::to_string(m_numThreads);
    AddNumThreadsArg(&m_numThreads, &m_numThreadsStr);
}

/************************************************************************/
/*                 GDALRasterViewshedAlgorithm::RunStep()               */
/************************************************************************/

bool GDALRasterViewshedAlgorithm::RunStep(GDALProgressFunc pfnProgress,
                                          void *pProgressData)
{
    auto poSrcDS = m_inputDataset.GetDatasetRef();
    CPLAssert(poSrcDS);
    CPLAssert(!m_outputDataset.GetDatasetRef());

    gdal::viewshed::Options opts;

    opts.observer.x = m_observerPos[0];
    opts.observer.y = m_observerPos[1];
    if (m_observerPos.size() == 3)
        opts.observer.z = m_observerPos[2];
    else
        opts.observer.z = 2;

    opts.targetHeight = m_targetHeight;

    opts.maxDistance = m_maxDistance;

    opts.curveCoeff = m_curveCoefficient;
    if (!GetArg("curvature-coefficient")->IsExplicitlySet())
    {
        opts.curveCoeff = gdal::viewshed::adjustCurveCoeff(
            opts.curveCoeff,
            GDALDataset::ToHandle(m_inputDataset.GetDatasetRef()));
    }

    opts.visibleVal = m_visibleVal;
    opts.invisibleVal = m_invisibleVal;
    opts.outOfRangeVal = m_outOfRangeVal;
    opts.nodataVal = m_dstNoData;

    if (m_outputMode == "normal")
        opts.outputMode = gdal::viewshed::OutputMode::Normal;
    else if (m_outputMode == "DEM")
        opts.outputMode = gdal::viewshed::OutputMode::DEM;
    else if (m_outputMode == "ground")
        opts.outputMode = gdal::viewshed::OutputMode::Ground;
    else if (m_outputMode == "cumulative")
        opts.outputMode = gdal::viewshed::OutputMode::Cumulative;

    opts.observerSpacing = m_observerSpacing;
    opts.numJobs = static_cast<uint8_t>(std::clamp(m_numThreads, 0, 255));

    opts.outputFilename =
        CPLGenerateTempFilenameSafe(
            CPLGetBasenameSafe(poSrcDS->GetDescription()).c_str()) +
        ".tif";
    opts.outputFormat = "GTiff";

    if (opts.outputMode == gdal::viewshed::OutputMode::Cumulative)
    {
        auto poSrcDriver = poSrcDS->GetDriver();
        if (EQUAL(poSrcDS->GetDescription(), "") || !poSrcDriver ||
            EQUAL(poSrcDriver->GetDescription(), "MEM"))
        {
            ReportError(
                CE_Failure, CPLE_AppDefined,
                "In cumulative mode, the input dataset must be opened by name");
            return false;
        }
        gdal::viewshed::Cumulative oViewshed(opts);
        const bool bSuccess = oViewshed.run(
            m_inputDataset.GetName().c_str(),
            pfnProgress ? pfnProgress : GDALDummyProgress, pProgressData);
        if (bSuccess)
        {
            m_outputDataset.Set(std::unique_ptr<GDALDataset>(
                GDALDataset::Open(opts.outputFilename.c_str(),
                                  GDAL_OF_RASTER | GDAL_OF_VERBOSE_ERROR,
                                  nullptr, nullptr, nullptr)));
        }
    }
    else
    {
        gdal::viewshed::Viewshed oViewshed(opts);
        const bool bSuccess = oViewshed.run(
            GDALRasterBand::ToHandle(poSrcDS->GetRasterBand(m_band)),
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

//! @endcond
