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
    AddArg("src-crs", 's', _("Source CRS"), &m_srsCrs)
        .SetIsCRSArg()
        .AddHiddenAlias("s_srs");
    AddArg("dst-crs", 'd', _("Destination CRS"), &m_dstCrs)
        .SetIsCRSArg()
        .AddHiddenAlias("t_srs");
    AddArg("resampling", 'r', _("Resampling method"), &m_resampling)
        .SetChoices("nearest", "bilinear", "cubic", "cubicspline", "lanczos",
                    "average", "rms", "mode", "min", "max", "med", "q1", "q3",
                    "sum")
        .SetDefault("nearest")
        .SetHiddenChoices("near");

    auto &resArg =
        AddArg("resolution", 0,
               _("Target resolution (in destination CRS units)"), &m_resolution)
            .SetMinCount(2)
            .SetMaxCount(2)
            .SetRepeatedArgAllowed(false)
            .SetDisplayHintAboutRepetition(false)
            .SetMetaVar("<xres>,<yres>")
            .SetMutualExclusionGroup("resolution-size");
    resArg.AddValidationAction(
        [&resArg]()
        {
            const auto &val = resArg.Get<std::vector<double>>();
            CPLAssert(val.size() == 2);
            if (!(val[0] > 0 && val[1] > 0))
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "Target resolution should be strictly positive.");
                return false;
            }
            return true;
        });

    auto &sizeArg = AddArg("size", 0, _("Target size in pixels"), &m_size)
                        .SetMinCount(2)
                        .SetMaxCount(2)
                        .SetRepeatedArgAllowed(false)
                        .SetDisplayHintAboutRepetition(false)
                        .SetMetaVar("<width>,<height>")
                        .SetMutualExclusionGroup("resolution-size");
    sizeArg.AddValidationAction(
        [&sizeArg]()
        {
            const auto &val = sizeArg.Get<std::vector<int>>();
            CPLAssert(val.size() == 2);
            if (!(val[0] >= 0 && val[1] >= 0))
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "Target size should be positive or 0.");
                return false;
            }
            return true;
        });

    AddBBOXArg(&m_bbox, _("Target bounding box (in destination CRS units)"));
    AddArg("bbox-crs", 0, _("CRS of target bounding box"), &m_bboxCrs)
        .SetIsCRSArg()
        .AddHiddenAlias("bbox_srs");

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
    if (!m_size.empty())
    {
        aosOptions.AddString("-ts");
        aosOptions.AddString(CPLSPrintf("%d", m_size[0]));
        aosOptions.AddString(CPLSPrintf("%d", m_size[1]));
    }
    if (!m_bbox.empty())
    {
        aosOptions.AddString("-te");
        aosOptions.AddString(CPLSPrintf("%.17g", m_bbox[0]));
        aosOptions.AddString(CPLSPrintf("%.17g", m_bbox[1]));
        aosOptions.AddString(CPLSPrintf("%.17g", m_bbox[2]));
        aosOptions.AddString(CPLSPrintf("%.17g", m_bbox[3]));
    }
    if (!m_bboxCrs.empty())
    {
        aosOptions.AddString("-te_srs");
        aosOptions.AddString(m_bboxCrs.c_str());
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
