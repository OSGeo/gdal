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
#include "gdalalg_raster_write.h"

#include "gdal_alg.h"
#include "gdal_priv.h"
#include "gdal_utils.h"
#include "gdalwarper.h"

#include <cmath>

//! @cond Doxygen_Suppress

#ifndef _
#define _(x) (x)
#endif

/************************************************************************/
/*     GDALRasterReprojectAlgorithm::GDALRasterReprojectAlgorithm()     */
/************************************************************************/

GDALRasterReprojectAlgorithm::GDALRasterReprojectAlgorithm(bool standaloneStep)
    : GDALRasterPipelineStepAlgorithm(NAME, DESCRIPTION, HELP_URL,
                                      standaloneStep)
{

    AddArg("src-crs", 's', _("Source CRS"), &m_srsCrs)
        .SetIsCRSArg()
        .AddHiddenAlias("s_srs");

    AddArg("like", 0,
           _("Dataset to use as a template for target bounds, CRS, size and "
             "nodata"),
           &m_likeDataset, GDAL_OF_RASTER)
        .SetMetaVar("DATASET");

    AddArg("dst-crs", 'd', _("Destination CRS"), &m_dstCrs)
        .SetIsCRSArg()
        .AddHiddenAlias("t_srs");

    GDALRasterReprojectUtils::AddResamplingArg(this, m_resampling);

    AddArg("resolution", 0, _("Target resolution (in destination CRS units)"),
           &m_resolution)
        .SetMinCount(2)
        .SetMaxCount(2)
        .SetMinValueExcluded(0)
        .SetRepeatedArgAllowed(false)
        .SetDisplayHintAboutRepetition(false)
        .SetMetaVar("<xres>,<yres>")
        .SetMutualExclusionGroup("resolution-size");

    AddArg("size", 0, _("Target size in pixels"), &m_size)
        .SetMinCount(2)
        .SetMaxCount(2)
        .SetMinValueIncluded(0)
        .SetRepeatedArgAllowed(false)
        .SetDisplayHintAboutRepetition(false)
        .SetMetaVar("<width>,<height>")
        .SetMutualExclusionGroup("resolution-size");

    auto &arg = AddBBOXArg(&m_bbox,
                           _("Target bounding box (in destination CRS units)"));

    arg.AddValidationAction(
        [this, &arg]()
        {
            // Validate it's not empty
            const std::vector<double> &bbox = arg.Get<std::vector<double>>();
            if ((bbox[0] >= bbox[2]) || (bbox[1] >= bbox[3]))
            {
                ReportError(CE_Failure, CPLE_AppDefined,
                            "Invalid bounding box specified");
                return false;
            }
            else
            {
                return true;
            }
        });

    AddArg("bbox-crs", 0, _("CRS of target bounding box"), &m_bboxCrs)
        .SetIsCRSArg()
        .AddHiddenAlias("bbox_srs");

    AddArg("target-aligned-pixels", 0,
           _("Round target extent to target resolution"),
           &m_targetAlignedPixels)
        .AddHiddenAlias("tap")
        .SetCategory(GAAC_ADVANCED);
    AddArg("src-nodata", 0,
           _("Set nodata values for input bands ('None' to unset)."),
           &m_srcNoData)
        .SetMinCount(1)
        .SetRepeatedArgAllowed(false)
        .SetCategory(GAAC_ADVANCED);
    AddArg("dst-nodata", 0,
           _("Set nodata values for output bands ('None' to unset)."),
           &m_dstNoData)
        .SetMinCount(1)
        .SetRepeatedArgAllowed(false)
        .SetCategory(GAAC_ADVANCED);
    AddArg("add-alpha", 0,
           _("Adds an alpha mask band to the destination when the source "
             "raster have none."),
           &m_addAlpha)
        .SetCategory(GAAC_ADVANCED);

    GDALRasterReprojectUtils::AddWarpOptTransformOptErrorThresholdArg(
        this, m_warpOptions, m_transformOptions, m_errorThreshold);

    AddNumThreadsArg(&m_numThreads, &m_numThreadsStr);
}

/************************************************************************/
/*             GDALRasterReprojectUtils::AddResamplingArg()             */
/************************************************************************/

/*static */ void
GDALRasterReprojectUtils::AddResamplingArg(GDALAlgorithm *alg,
                                           std::string &resampling)
{
    alg->AddArg("resampling", 'r', _("Resampling method"), &resampling)
        .SetChoices("nearest", "bilinear", "cubic", "cubicspline", "lanczos",
                    "average", "rms", "mode", "min", "max", "med", "q1", "q3",
                    "sum")
        .SetDefault("nearest")
        .SetHiddenChoices("near");
}

/************************************************************************/
/*              AddWarpOptTransformOptErrorThresholdArg()               */
/************************************************************************/

/* static */
void GDALRasterReprojectUtils::AddWarpOptTransformOptErrorThresholdArg(
    GDALAlgorithm *alg, std::vector<std::string> &warpOptions,
    std::vector<std::string> &transformOptions, double &errorThreshold)
{
    {
        auto &arg =
            alg->AddArg("warp-option", 0, _("Warping option(s)"), &warpOptions)
                .AddAlias("wo")
                .SetMetaVar("<NAME>=<VALUE>")
                .SetCategory(GAAC_ADVANCED)
                .SetPackedValuesAllowed(false);
        arg.AddValidationAction([alg, &arg]()
                                { return alg->ParseAndValidateKeyValue(arg); });
        arg.SetAutoCompleteFunction(
            [](const std::string &currentValue)
            {
                std::vector<std::string> ret;
                GDALAlgorithm::AddOptionsSuggestions(GDALWarpGetOptionList(), 0,
                                                     currentValue, ret);
                return ret;
            });
    }
    {
        auto &arg = alg->AddArg("transform-option", 0, _("Transform option(s)"),
                                &transformOptions)
                        .AddAlias("to")
                        .SetMetaVar("<NAME>=<VALUE>")
                        .SetCategory(GAAC_ADVANCED)
                        .SetPackedValuesAllowed(false);
        arg.AddValidationAction([alg, &arg]()
                                { return alg->ParseAndValidateKeyValue(arg); });
        arg.SetAutoCompleteFunction(
            [](const std::string &currentValue)
            {
                std::vector<std::string> ret;
                GDALAlgorithm::AddOptionsSuggestions(
                    GDALGetGenImgProjTranformerOptionList(), 0, currentValue,
                    ret);
                return ret;
            });
    }
    alg->AddArg("error-threshold", 0, _("Error threshold"), &errorThreshold)
        .AddAlias("et")
        .SetMinValueIncluded(0)
        .SetCategory(GAAC_ADVANCED);
}

/************************************************************************/
/*          GDALRasterReprojectAlgorithm::CanHandleNextStep()           */
/************************************************************************/

bool GDALRasterReprojectAlgorithm::CanHandleNextStep(
    GDALPipelineStepAlgorithm *poNextStep) const
{
    return poNextStep->GetName() == GDALRasterWriteAlgorithm::NAME &&
           poNextStep->GetOutputFormat() != "stream";
}

/************************************************************************/
/*               GDALRasterReprojectAlgorithm::RunStep()                */
/************************************************************************/

bool GDALRasterReprojectAlgorithm::RunStep(GDALPipelineStepRunContext &ctxt)
{
    auto poSrcDS = m_inputDataset[0].GetDatasetRef();
    CPLAssert(poSrcDS);
    CPLAssert(m_outputDataset.GetName().empty());
    CPLAssert(!m_outputDataset.GetDatasetRef());

    CPLStringList aosOptions;
    std::string outputFilename;

    // --like provide defaults: override if not explicitly set
    if (auto poLikeDS = m_likeDataset.GetDatasetRef())
    {
        const auto poSpatialRef = poLikeDS->GetSpatialRef();
        if (poSpatialRef)
        {
            char *pszWKT = nullptr;
            poSpatialRef->exportToWkt(&pszWKT);
            m_dstCrs = pszWKT;
            CPLFree(pszWKT);
            GDALGeoTransform gt;
            if (poLikeDS->GetGeoTransform(gt) == CE_None)
            {
                if (gt.IsAxisAligned())
                {
                    if (m_resolution.empty())
                    {
                        m_resolution = {std::abs(gt[1]), std::abs(gt[5])};
                    }
                    const int nXSize = poLikeDS->GetRasterXSize();
                    const int nYSize = poLikeDS->GetRasterYSize();
                    if (m_size.empty())
                    {
                        m_size = {nXSize, nYSize};
                    }
                    if (m_bbox.empty())
                    {
                        double minX = gt.xorig;
                        double maxY = gt.yorig;
                        double maxX =
                            gt.xorig + nXSize * gt.xscale + nYSize * gt.xrot;
                        double minY =
                            gt.yorig + nXSize * gt.yrot + nYSize * gt.yscale;
                        if (minY > maxY)
                            std::swap(minY, maxY);
                        m_bbox = {minX, minY, maxX, maxY};
                        m_bboxCrs = m_dstCrs;
                    }
                }
                else
                {
                    ReportError(
                        CE_Warning, CPLE_AppDefined,
                        "Dataset provided with --like has a geotransform "
                        "with rotation. Ignoring it");
                }
            }
        }
    }

    if (ctxt.m_poNextUsableStep)
    {
        CPLAssert(CanHandleNextStep(ctxt.m_poNextUsableStep));
        outputFilename = ctxt.m_poNextUsableStep->GetOutputDataset().GetName();
        const auto &format = ctxt.m_poNextUsableStep->GetOutputFormat();
        if (!format.empty())
        {
            aosOptions.AddString("-of");
            aosOptions.AddString(format.c_str());
        }

        bool bFoundNumThreads = false;
        for (const std::string &co :
             ctxt.m_poNextUsableStep->GetCreationOptions())
        {
            aosOptions.AddString("-co");
            if (STARTS_WITH_CI(co.c_str(), "NUM_THREADS="))
                bFoundNumThreads = true;
            aosOptions.AddString(co.c_str());
        }

        // Forward m_numThreads to GeoTIFF driver if --co NUM_THREADS not
        // specified
        if (!bFoundNumThreads && m_numThreads > 1 &&
            (EQUAL(format.c_str(), "GTIFF") || EQUAL(format.c_str(), "COG") ||
             (format.empty() &&
              EQUAL(CPLGetExtensionSafe(outputFilename.c_str()).c_str(),
                    "tif"))))
        {
            aosOptions.AddString("-co");
            aosOptions.AddString(CPLSPrintf("NUM_THREADS=%d", m_numThreads));
        }
    }
    else
    {
        aosOptions.AddString("-of");
        aosOptions.AddString("VRT");
    }
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
    if (!m_srcNoData.empty())
    {
        aosOptions.push_back("-srcnodata");
        std::string s;
        for (const std::string &v : m_srcNoData)
        {
            if (!s.empty())
                s += " ";
            s += v;
        }
        aosOptions.push_back(s);
    }
    if (!m_dstNoData.empty())
    {
        aosOptions.push_back("-dstnodata");
        std::string s;
        for (const std::string &v : m_dstNoData)
        {
            if (!s.empty())
                s += " ";
            s += v;
        }
        aosOptions.push_back(s);
    }
    if (m_addAlpha)
    {
        aosOptions.AddString("-dstalpha");
    }

    bool bFoundNumThreads = false;
    for (const std::string &opt : m_warpOptions)
    {
        aosOptions.AddString("-wo");
        if (STARTS_WITH_CI(opt.c_str(), "NUM_THREADS="))
            bFoundNumThreads = true;
        aosOptions.AddString(opt.c_str());
    }
    if (bFoundNumThreads)
    {
        if (GetArg(GDAL_ARG_NAME_NUM_THREADS)->IsExplicitlySet())
        {
            ReportError(CE_Failure, CPLE_AppDefined,
                        "--num-threads argument and NUM_THREADS warp options "
                        "are mutually exclusive.");
            return false;
        }
    }
    else
    {
        aosOptions.AddString("-wo");
        aosOptions.AddString(CPLSPrintf("NUM_THREADS=%d", m_numThreads));
    }

    for (const std::string &opt : m_transformOptions)
    {
        aosOptions.AddString("-to");
        aosOptions.AddString(opt.c_str());
    }
    if (std::isfinite(m_errorThreshold))
    {
        aosOptions.AddString("-et");
        aosOptions.AddString(CPLSPrintf("%.17g", m_errorThreshold));
    }

    bool bOK = false;
    GDALWarpAppOptions *psOptions =
        GDALWarpAppOptionsNew(aosOptions.List(), nullptr);
    if (psOptions)
    {
        if (ctxt.m_poNextUsableStep)
        {
            GDALWarpAppOptionsSetProgress(psOptions, ctxt.m_pfnProgress,
                                          ctxt.m_pProgressData);
        }
        GDALDatasetH hSrcDS = GDALDataset::ToHandle(poSrcDS);
        auto poRetDS = GDALDataset::FromHandle(GDALWarp(
            outputFilename.c_str(), nullptr, 1, &hSrcDS, psOptions, nullptr));
        GDALWarpAppOptionsFree(psOptions);
        bOK = poRetDS != nullptr;
        if (bOK)
        {
            m_outputDataset.Set(std::unique_ptr<GDALDataset>(poRetDS));
        }
    }
    return bOK;
}

GDALRasterReprojectAlgorithmStandalone::
    ~GDALRasterReprojectAlgorithmStandalone() = default;

//! @endcond
