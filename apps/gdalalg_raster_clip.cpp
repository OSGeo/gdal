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
#include <cmath>

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
        .SetMutualExclusionGroup("bbox-geometry-like");
    AddArg("bbox-crs", 0, _("CRS of clipping bounding box"), &m_bboxCrs)
        .SetIsCRSArg()
        .AddHiddenAlias("bbox_srs");
    AddArg("geometry", 0, _("Clipping geometry (WKT or GeoJSON)"), &m_geometry)
        .SetMutualExclusionGroup("bbox-geometry-like");
    AddArg("geometry-crs", 0, _("CRS of clipping geometry"), &m_geometryCrs)
        .SetIsCRSArg()
        .AddHiddenAlias("geometry_srs");
    AddArg("like", 0, _("Dataset to use as a template for bounds"),
           &m_likeDataset, GDAL_OF_RASTER | GDAL_OF_VECTOR)
        .SetMetaVar("DATASET")
        .SetMutualExclusionGroup("bbox-geometry-like");
    AddArg("like-sql", 0, ("SELECT statement to run on the 'like' dataset"),
           &m_likeSQL)
        .SetMetaVar("SELECT-STATEMENT")
        .SetMutualExclusionGroup("sql-where");
    AddArg("like-layer", 0, ("Name of the layer of the 'like' dataset"),
           &m_likeLayer)
        .SetMetaVar("LAYER-NAME");
    AddArg("like-where", 0, ("WHERE SQL clause to run on the 'like' dataset"),
           &m_likeWhere)
        .SetMetaVar("WHERE-EXPRESSION")
        .SetMutualExclusionGroup("sql-where");
    AddArg("only-bbox", 0,
           _("For 'geometry' and 'like', only consider their bounding box"),
           &m_onlyBBOX);
    AddArg("allow-bbox-outside-source", 0,
           _("Allow clipping box to include pixels outside input dataset"),
           &m_allowExtentOutsideSource);
    AddArg("add-alpha", 0,
           _("Adds an alpha mask band to the destination when the source "
             "raster have none."),
           &m_addAlpha);
}

/************************************************************************/
/*                 GDALRasterClipAlgorithm::RunStep()                   */
/************************************************************************/

bool GDALRasterClipAlgorithm::RunStep(GDALProgressFunc, void *)
{
    auto poSrcDS = m_inputDataset.GetDatasetRef();
    CPLAssert(poSrcDS);
    CPLAssert(m_outputDataset.GetName().empty());
    CPLAssert(!m_outputDataset.GetDatasetRef());

    double adfGT[6];
    if (poSrcDS->GetGeoTransform(adfGT) != CE_None)
    {
        ReportError(
            CE_Failure, CPLE_NotSupported,
            "Clipping is not supported on a raster without a geotransform");
        return false;
    }
    if (adfGT[2] != 0 && adfGT[4] != 0)
    {
        ReportError(CE_Failure, CPLE_NotSupported,
                    "Clipping is not supported on a raster whose geotransform "
                    "has rotation terms");
        return false;
    }

    auto [poClipGeom, errMsg] = GetClipGeometry();
    if (!poClipGeom)
    {
        ReportError(CE_Failure, CPLE_AppDefined, "%s", errMsg.c_str());
        return false;
    }

    auto poLikeDS = m_likeDataset.GetDatasetRef();
    if (!poClipGeom->getSpatialReference() && poLikeDS &&
        poLikeDS->GetLayerCount() == 0)
    {
        ReportError(CE_Failure, CPLE_AppDefined,
                    "Dataset '%s' has no CRS. Its bounds cannot be used.",
                    poLikeDS->GetDescription());
        return false;
    }

    CPLStringList aosOptions;
    aosOptions.AddString("-of");
    aosOptions.AddString("VRT");

    OGREnvelope env;
    poClipGeom->getEnvelope(&env);

    if (m_onlyBBOX)
    {
        auto poPoly = std::make_unique<OGRPolygon>(env);
        poPoly->assignSpatialReference(poClipGeom->getSpatialReference());
        poClipGeom = std::move(poPoly);
    }

    const bool bBottomUpRaster = adfGT[5] > 0;

    if (poClipGeom->IsRectangle() && !m_addAlpha && !bBottomUpRaster)
    {
        aosOptions.AddString("-projwin");
        aosOptions.AddString(CPLSPrintf("%.17g", env.MinX));
        aosOptions.AddString(CPLSPrintf("%.17g", env.MaxY));
        aosOptions.AddString(CPLSPrintf("%.17g", env.MaxX));
        aosOptions.AddString(CPLSPrintf("%.17g", env.MinY));

        auto poClipGeomSRS = poClipGeom->getSpatialReference();
        if (poClipGeomSRS)
        {
            const char *const apszOptions[] = {"FORMAT=WKT2", nullptr};
            const std::string osWKT = poClipGeomSRS->exportToWkt(apszOptions);
            aosOptions.AddString("-projwin_srs");
            aosOptions.AddString(osWKT.c_str());
        }

        if (!m_allowExtentOutsideSource)
        {
            // Unless we've specifically allowed the bounding box to extend beyond
            // the source raster, raise an error.
            aosOptions.AddString("-epo");
        }

        GDALTranslateOptions *psOptions =
            GDALTranslateOptionsNew(aosOptions.List(), nullptr);

        GDALDatasetH hSrcDS = GDALDataset::ToHandle(poSrcDS);
        auto poRetDS = GDALDataset::FromHandle(
            GDALTranslate("", hSrcDS, psOptions, nullptr));
        GDALTranslateOptionsFree(psOptions);

        const bool bOK = poRetDS != nullptr;
        if (bOK)
        {
            m_outputDataset.Set(std::unique_ptr<GDALDataset>(poRetDS));
        }

        return bOK;
    }
    else
    {
        if (bBottomUpRaster)
        {
            adfGT[3] += adfGT[5] * poSrcDS->GetRasterYSize();
            adfGT[5] = -adfGT[5];
        }

        {
            auto poClipGeomInSrcSRS =
                std::unique_ptr<OGRGeometry>(poClipGeom->clone());
            if (poClipGeom->getSpatialReference() && poSrcDS->GetSpatialRef())
                poClipGeomInSrcSRS->transformTo(poSrcDS->GetSpatialRef());
            poClipGeomInSrcSRS->getEnvelope(&env);
        }

        if (!m_allowExtentOutsideSource &&
            !(env.MinX >= adfGT[0] &&
              env.MaxX <= adfGT[0] + adfGT[1] * poSrcDS->GetRasterXSize() &&
              env.MaxY >= adfGT[3] &&
              env.MinY <= adfGT[3] + adfGT[5] * poSrcDS->GetRasterYSize()))
        {
            ReportError(CE_Failure, CPLE_AppDefined,
                        "Clipping geometry is partially or totally outside the "
                        "extent of the raster. You can set the "
                        "'allow-bbox-outside-source' argument to proceed.");
            return false;
        }

        if (m_addAlpha)
        {
            aosOptions.AddString("-dstalpha");
        }

        aosOptions.AddString("-cutline");
        aosOptions.AddString(poClipGeom->exportToWkt());

        aosOptions.AddString("-wo");
        aosOptions.AddString("CUTLINE_ALL_TOUCHED=YES");

        auto poClipGeomSRS = poClipGeom->getSpatialReference();
        if (poClipGeomSRS)
        {
            const char *const apszOptions[] = {"FORMAT=WKT2", nullptr};
            const std::string osWKT = poClipGeomSRS->exportToWkt(apszOptions);
            aosOptions.AddString("-cutline_srs");
            aosOptions.AddString(osWKT.c_str());
        }

        constexpr double REL_EPS_PIXEL = 1e-3;
        const double dfMinX =
            adfGT[0] +
            floor((env.MinX - adfGT[0]) / adfGT[1] + REL_EPS_PIXEL) * adfGT[1];
        const double dfMinY =
            adfGT[3] +
            ceil((env.MinY - adfGT[3]) / adfGT[5] - REL_EPS_PIXEL) * adfGT[5];
        const double dfMaxX =
            adfGT[0] +
            ceil((env.MaxX - adfGT[0]) / adfGT[1] - REL_EPS_PIXEL) * adfGT[1];
        const double dfMaxY =
            adfGT[3] +
            floor((env.MaxY - adfGT[3]) / adfGT[5] + REL_EPS_PIXEL) * adfGT[5];

        aosOptions.AddString("-te");
        aosOptions.AddString(CPLSPrintf("%.17g", dfMinX));
        aosOptions.AddString(
            CPLSPrintf("%.17g", bBottomUpRaster ? dfMaxY : dfMinY));
        aosOptions.AddString(CPLSPrintf("%.17g", dfMaxX));
        aosOptions.AddString(
            CPLSPrintf("%.17g", bBottomUpRaster ? dfMinY : dfMaxY));

        aosOptions.AddString("-tr");
        aosOptions.AddString(CPLSPrintf("%.17g", adfGT[1]));
        aosOptions.AddString(CPLSPrintf("%.17g", std::fabs(adfGT[5])));

        GDALWarpAppOptions *psOptions =
            GDALWarpAppOptionsNew(aosOptions.List(), nullptr);

        GDALDatasetH hSrcDS = GDALDataset::ToHandle(poSrcDS);
        auto poRetDS = GDALDataset::FromHandle(
            GDALWarp("", nullptr, 1, &hSrcDS, psOptions, nullptr));
        GDALWarpAppOptionsFree(psOptions);

        const bool bOK = poRetDS != nullptr;
        if (bOK)
        {
            m_outputDataset.Set(std::unique_ptr<GDALDataset>(poRetDS));
        }

        return bOK;
    }
}

//! @endcond
