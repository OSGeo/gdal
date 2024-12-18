/******************************************************************************
 *
 * Project:  GDAL
 * Purpose:  "reproject" step of "raster pipeline"
 * Author:   Even Rouault <even dot rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2024, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include "gdalalg_raster_reproject.h"

#include "gdal_priv.h"
#include "gdal_utils.h"

//! @cond Doxygen_Suppress

#ifndef _
#define _(x) (x)
#endif

/************************************************************************/
/*      GDALRasterReprojectAlgorithm::GDALRasterReprojectAlgorithm()    */
/************************************************************************/

GDALRasterReprojectAlgorithm::GDALRasterReprojectAlgorithm(bool standaloneStep)
    : GDALRasterPipelineStepAlgorithm(NAME, DESCRIPTION, HELP_URL,
                                      standaloneStep)
{
    AddArg("src-crs", 's', _("Source CRS"), &m_srsCrs).AddHiddenAlias("s_srs");
    AddArg("dst-crs", 'd', _("Destination CRS"), &m_dstCrs)
        .AddHiddenAlias("t_srs");
    AddArg("resampling", 'r', _("Resampling method"), &m_resampling)
        .SetChoices("near", "bilinear", "cubic", "cubicspline", "lanczos",
                    "average", "rms", "mode", "min", "max", "med", "q1", "q3",
                    "sum");

    auto &resArg =
        AddArg("resolution", 0,
               _("Target resolution (in destination CRS units)"), &m_resolution)
            .SetMinCount(2)
            .SetMaxCount(2)
            .SetRepeatedArgAllowed(false)
            .SetDisplayHintAboutRepetition(false)
            .SetMetaVar("<xres>,<yres>");
    resArg.AddValidationAction(
        [&resArg]()
        {
            const auto &val = resArg.Get<std::vector<double>>();
            CPLAssert(val.size() == 2);
            if (!(val[0] >= 0 && val[1] >= 0))
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "Target resolution should be strictly positive.");
                return false;
            }
            return true;
        });

    auto &extentArg =
        AddArg("extent", 0, _("Target extent (in destination CRS units)"),
               &m_extent)
            .SetMinCount(4)
            .SetMaxCount(4)
            .SetRepeatedArgAllowed(false)
            .SetDisplayHintAboutRepetition(false)
            .SetMetaVar("<xmin>,<ymin>,<xmax>,<ymax>");
    extentArg.AddValidationAction(
        [&extentArg]()
        {
            const auto &val = extentArg.Get<std::vector<double>>();
            CPLAssert(val.size() == 4);
            if (!(val[0] <= val[2]) || !(val[1] <= val[3]))
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "Value of 'extent' should be xmin,ymin,xmax,ymax with "
                         "xmin <= xmax and ymin <= ymax");
                return false;
            }
            return true;
        });

    AddArg("target-aligned-pixels", 0,
           _("Round target extent to target resolution"),
           &m_targetAlignedPixels)
        .AddHiddenAlias("tap");
}

/************************************************************************/
/*            GDALRasterReprojectAlgorithm::RunStep()                   */
/************************************************************************/

bool GDALRasterReprojectAlgorithm::RunStep(GDALProgressFunc, void *)
{
    CPLAssert(m_inputDataset.GetDatasetRef());
    CPLAssert(m_outputDataset.GetName().empty());
    CPLAssert(!m_outputDataset.GetDatasetRef());

    if (!m_srsCrs.empty())
    {
        OGRSpatialReference oSRS;
        if (oSRS.SetFromUserInput(m_srsCrs.c_str()) != OGRERR_NONE)
        {
            ReportError(CE_Failure, CPLE_AppDefined,
                        "Invalid value for '--src-crs'");
            return false;
        }
    }

    if (!m_dstCrs.empty())
    {
        OGRSpatialReference oSRS;
        if (oSRS.SetFromUserInput(m_dstCrs.c_str()) != OGRERR_NONE)
        {
            ReportError(CE_Failure, CPLE_AppDefined,
                        "Invalid value for '--dst-crs'");
            return false;
        }
    }

    CPLStringList aosOptions;
    aosOptions.AddString("-of");
    aosOptions.AddString("VRT");
    if (!m_srsCrs.empty())
    {
        aosOptions.AddString("-s_srs");
        aosOptions.AddString(m_srsCrs.c_str());
    }
    if (!m_dstCrs.empty())
    {
        aosOptions.AddString("-t_srs");
        aosOptions.AddString(m_dstCrs.c_str());
    }
    if (!m_resampling.empty())
    {
        aosOptions.AddString("-r");
        aosOptions.AddString(m_resampling.c_str());
    }
    if (!m_resolution.empty())
    {
        aosOptions.AddString("-tr");
        aosOptions.AddString(CPLSPrintf("%.17g", m_resolution[0]));
        aosOptions.AddString(CPLSPrintf("%.17g", m_resolution[1]));
    }
    if (!m_extent.empty())
    {
        aosOptions.AddString("-te");
        aosOptions.AddString(CPLSPrintf("%.17g", m_extent[0]));
        aosOptions.AddString(CPLSPrintf("%.17g", m_extent[1]));
        aosOptions.AddString(CPLSPrintf("%.17g", m_extent[2]));
        aosOptions.AddString(CPLSPrintf("%.17g", m_extent[3]));
    }
    if (m_targetAlignedPixels)
    {
        aosOptions.AddString("-tap");
    }
    GDALWarpAppOptions *psOptions =
        GDALWarpAppOptionsNew(aosOptions.List(), nullptr);

    GDALDatasetH hSrcDS = GDALDataset::ToHandle(m_inputDataset.GetDatasetRef());
    auto poRetDS = GDALDataset::FromHandle(
        GDALWarp("", nullptr, 1, &hSrcDS, psOptions, nullptr));
    GDALWarpAppOptionsFree(psOptions);
    if (!poRetDS)
        return false;

    m_outputDataset.Set(std::unique_ptr<GDALDataset>(poRetDS));

    return true;
}

//! @endcond
