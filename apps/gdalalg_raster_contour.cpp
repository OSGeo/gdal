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
/*       GDALRasterContourAlgorithm::GDALRasterContourAlgorithm()       */
/************************************************************************/

GDALRasterContourAlgorithm::GDALRasterContourAlgorithm(bool standaloneStep)
    : GDALPipelineStepAlgorithm(
          NAME, DESCRIPTION, HELP_URL,
          ConstructorOptions()
              .SetStandaloneStep(standaloneStep)
              .SetAddAppendLayerArgument(false)
              .SetAddOverwriteLayerArgument(false)
              .SetAddUpdateArgument(false)
              .SetAddUpsertArgument(false)
              .SetAddSkipErrorsArgument(false)
              .SetOutputFormatCreateCapability(GDAL_DCAP_CREATE))
{
    m_outputLayerName = "contour";

    AddProgressArg();
    if (standaloneStep)
    {
        AddRasterInputArgs(false, false);
        AddVectorOutputArgs(false, false);
    }
    else
    {
        AddRasterHiddenInputDatasetArg();
        AddOutputLayerNameArg(/* hiddenForCLI = */ false,
                              /* shortNameOutputLayerAllowed = */ false);
    }

    // gdal_contour specific options
    AddBandArg(&m_band).SetDefault(1);

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
}

/************************************************************************/
/*                GDALRasterContourAlgorithm::RunImpl()                 */
/************************************************************************/

bool GDALRasterContourAlgorithm::RunImpl(GDALProgressFunc pfnProgress,
                                         void *pProgressData)
{
    GDALPipelineStepRunContext stepCtxt;
    stepCtxt.m_pfnProgress = pfnProgress;
    stepCtxt.m_pProgressData = pProgressData;
    return RunPreStepPipelineValidations() && RunStep(stepCtxt);
}

/************************************************************************/
/*                GDALRasterContourAlgorithm::RunStep()                 */
/************************************************************************/

bool GDALRasterContourAlgorithm::RunStep(GDALPipelineStepRunContext &ctxt)
{
    CPLErrorReset();

    auto poSrcDS = m_inputDataset[0].GetDatasetRef();
    CPLAssert(poSrcDS);

    CPLAssert(!m_outputDataset.GetDatasetRef());

    CPLStringList aosOptions;

    std::string outputFilename;
    if (m_standaloneStep)
    {
        outputFilename = m_outputDataset.GetName();
        if (!m_format.empty())
        {
            aosOptions.AddString("-of");
            aosOptions.AddString(m_format.c_str());
        }

        for (const auto &co : m_creationOptions)
        {
            aosOptions.push_back("-co");
            aosOptions.push_back(co.c_str());
        }

        for (const auto &co : m_layerCreationOptions)
        {
            aosOptions.push_back("-lco");
            aosOptions.push_back(co.c_str());
        }
    }
    else
    {
        if (GetGDALDriverManager()->GetDriverByName("GPKG"))
        {
            aosOptions.AddString("-of");
            aosOptions.AddString("GPKG");

            outputFilename = CPLGenerateTempFilenameSafe("_contour") + ".gpkg";
        }
        else
        {
            aosOptions.AddString("-of");
            aosOptions.AddString("MEM");
        }
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

    aosOptions.AddString(m_inputDataset[0].GetName());
    aosOptions.AddString(outputFilename);

    bool bRet = false;
    GDALContourOptionsForBinary optionsForBinary;
    std::unique_ptr<GDALContourOptions, decltype(&GDALContourOptionsFree)>
        psOptions{GDALContourOptionsNew(aosOptions.List(), &optionsForBinary),
                  GDALContourOptionsFree};
    if (psOptions)
    {
        GDALDatasetH hSrcDS{poSrcDS};
        GDALRasterBandH hBand{nullptr};
        GDALDatasetH hDstDS{m_outputDataset.GetDatasetRef()};
        OGRLayerH hLayer{nullptr};
        char **papszStringOptions = nullptr;

        bRet = GDALContourProcessOptions(psOptions.get(), &papszStringOptions,
                                         &hSrcDS, &hBand, &hDstDS,
                                         &hLayer) == CE_None;

        if (bRet)
        {
            bRet = GDALContourGenerateEx(hBand, hLayer, papszStringOptions,
                                         ctxt.m_pfnProgress,
                                         ctxt.m_pProgressData) == CE_None;
        }

        CSLDestroy(papszStringOptions);

        auto poDstDS = GDALDataset::FromHandle(hDstDS);
        if (bRet)
        {
            bRet = poDstDS != nullptr;
        }
        if (poDstDS && !m_standaloneStep && !outputFilename.empty())
        {
            poDstDS->MarkSuppressOnClose();
            if (bRet)
                bRet = poDstDS->FlushCache() == CE_None;
#if !defined(__APPLE__)
            // For some unknown reason, unlinking the file on MacOSX
            // leads to later "disk I/O error". See https://github.com/OSGeo/gdal/issues/13794
            VSIUnlink(outputFilename.c_str());
#endif
        }
        m_outputDataset.Set(std::unique_ptr<GDALDataset>(poDstDS));
    }

    return bRet;
}

GDALRasterContourAlgorithmStandalone::~GDALRasterContourAlgorithmStandalone() =
    default;

//! @endcond
