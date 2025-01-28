/******************************************************************************
 *
 * Project:  GDAL
 * Purpose:  "clip" step of "raster pipeline", or "gdal raster clip" standalone
 * Author:   Even Rouault <even dot rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2025, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include "gdalalg_raster_clip.h"

#include "gdal_priv.h"
#include "gdal_utils.h"

#include <algorithm>

//! @cond Doxygen_Suppress

#ifndef _
#define _(x) (x)
#endif

/************************************************************************/
/*          GDALRasterClipAlgorithm::GDALRasterClipAlgorithm()          */
/************************************************************************/

GDALRasterClipAlgorithm::GDALRasterClipAlgorithm(bool standaloneStep)
    : GDALRasterPipelineStepAlgorithm(NAME, DESCRIPTION, HELP_URL,
                                      standaloneStep)
{
    AddBBOXArg(&m_bbox, _("Clipping bounding box as xmin,ymin,xmax,ymax"))
        .SetMutualExclusionGroup("exclusion-group");
    AddArg("bbox-crs", 0, _("CRS of clipping bounding box"), &m_bboxCrs)
        .SetIsCRSArg()
        .AddHiddenAlias("bbox_srs");
    AddArg("like", 0, _("Raster dataset to use as a template for bounds"),
           &m_likeDataset, GDAL_OF_RASTER)
        .SetMetaVar("DATASET")
        .SetMutualExclusionGroup("exclusion-group");
}

/************************************************************************/
/*                 GDALRasterClipAlgorithm::RunStep()                   */
/************************************************************************/

bool GDALRasterClipAlgorithm::RunStep(GDALProgressFunc, void *)
{
    CPLAssert(m_inputDataset.GetDatasetRef());
    CPLAssert(m_outputDataset.GetName().empty());
    CPLAssert(!m_outputDataset.GetDatasetRef());

    CPLStringList aosOptions;
    aosOptions.AddString("-of");
    aosOptions.AddString("VRT");
    if (!m_bbox.empty())
    {
        aosOptions.AddString("-projwin");
        aosOptions.AddString(CPLSPrintf("%.17g", m_bbox[0]));  // minx
        aosOptions.AddString(CPLSPrintf("%.17g", m_bbox[3]));  // maxy
        aosOptions.AddString(CPLSPrintf("%.17g", m_bbox[2]));  // maxx
        aosOptions.AddString(CPLSPrintf("%.17g", m_bbox[1]));  // miny

        if (!m_bboxCrs.empty())
        {
            aosOptions.AddString("-projwin_srs");
            aosOptions.AddString(m_bboxCrs.c_str());
        }
    }
    else if (auto poLikeDS = m_likeDataset.GetDatasetRef())
    {
        double adfGT[6];
        if (poLikeDS->GetGeoTransform(adfGT) != CE_None)
        {
            ReportError(CE_Failure, CPLE_AppDefined,
                        "Dataset '%s' has no geotransform matrix. Its bounds "
                        "cannot be established.",
                        poLikeDS->GetDescription());
            return false;
        }
        auto poLikeSRS = poLikeDS->GetSpatialRef();
        if (!poLikeSRS)
        {
            ReportError(CE_Failure, CPLE_AppDefined,
                        "Dataset '%s' has no SRS. Its bounds cannot be used.",
                        poLikeDS->GetDescription());
            return false;
        }
        const double dfTLX = adfGT[0];
        const double dfTLY = adfGT[3];
        const double dfTRX = adfGT[0] + poLikeDS->GetRasterXSize() * adfGT[1];
        const double dfTRY = adfGT[3] + poLikeDS->GetRasterXSize() * adfGT[4];
        const double dfBLX = adfGT[0] + poLikeDS->GetRasterYSize() * adfGT[2];
        const double dfBLY = adfGT[3] + poLikeDS->GetRasterYSize() * adfGT[5];
        const double dfBRX = adfGT[0] + poLikeDS->GetRasterXSize() * adfGT[1] +
                             poLikeDS->GetRasterYSize() * adfGT[2];
        const double dfBRY = adfGT[3] + poLikeDS->GetRasterXSize() * adfGT[4] +
                             poLikeDS->GetRasterYSize() * adfGT[5];
        const double dfMinX =
            std::min(std::min(dfTLX, dfTRX), std::min(dfBLX, dfBRX));
        const double dfMinY =
            std::min(std::min(dfTLY, dfTRY), std::min(dfBLY, dfBRY));
        const double dfMaxX =
            std::max(std::max(dfTLX, dfTRX), std::max(dfBLX, dfBRX));
        const double dfMaxY =
            std::max(std::max(dfTLY, dfTRY), std::max(dfBLY, dfBRY));

        aosOptions.AddString("-projwin");
        aosOptions.AddString(CPLSPrintf("%.17g", dfMinX));
        aosOptions.AddString(CPLSPrintf("%.17g", dfMaxY));
        aosOptions.AddString(CPLSPrintf("%.17g", dfMaxX));
        aosOptions.AddString(CPLSPrintf("%.17g", dfMinY));

        const char *const apszOptions[] = {"FORMAT=WKT2", nullptr};
        const std::string osWKT = poLikeSRS->exportToWkt(apszOptions);
        aosOptions.AddString("-projwin_srs");
        aosOptions.AddString(osWKT.c_str());
    }
    else
    {
        ReportError(CE_Failure, CPLE_AppDefined,
                    "Either --bbox or --like must be specified");
        return false;
    }

    GDALTranslateOptions *psOptions =
        GDALTranslateOptionsNew(aosOptions.List(), nullptr);

    GDALDatasetH hSrcDS = GDALDataset::ToHandle(m_inputDataset.GetDatasetRef());
    auto poRetDS =
        GDALDataset::FromHandle(GDALTranslate("", hSrcDS, psOptions, nullptr));
    GDALTranslateOptionsFree(psOptions);

    const bool bOK = poRetDS != nullptr;
    if (bOK)
    {
        m_outputDataset.Set(std::unique_ptr<GDALDataset>(poRetDS));
    }

    return bOK;
}

//! @endcond
