/******************************************************************************
 *
 * Project:  GDAL
 * Purpose:  Common code for gdalalg_raster_clip and gdalalg_vector_clip
 * Author:   Even Rouault <even dot rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2025, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include "gdalalg_clip_common.h"

#include "ogrsf_frmts.h"

//! @cond Doxygen_Suppress

#ifndef _
#define _(x) (x)
#endif

/************************************************************************/
/*                           ~GDALClipCommon()                          */
/************************************************************************/

GDALClipCommon::~GDALClipCommon() = default;

/************************************************************************/
/*                           LoadGeometry()                             */
/************************************************************************/

std::pair<std::unique_ptr<OGRGeometry>, std::string>
GDALClipCommon::LoadGeometry()
{
    auto poDS = m_likeDataset.GetDatasetRef();
    OGRLayer *poLyr = nullptr;
    if (!m_likeSQL.empty())
        poLyr = poDS->ExecuteSQL(m_likeSQL.c_str(), nullptr, nullptr);
    else if (!m_likeLayer.empty())
        poLyr = poDS->GetLayerByName(m_likeLayer.c_str());
    else
        poLyr = poDS->GetLayer(0);

    if (poLyr == nullptr)
    {
        return {nullptr,
                "Failed to identify source layer from clipping dataset."};
    }

    if (!m_likeWhere.empty())
        poLyr->SetAttributeFilter(m_likeWhere.c_str());

    OGRGeometryCollection oGC;
    oGC.assignSpatialReference(poLyr->GetSpatialRef());

    for (auto &poFeat : poLyr)
    {
        auto poSrcGeom = std::unique_ptr<OGRGeometry>(poFeat->StealGeometry());
        if (poSrcGeom)
        {
            // Only take into account areal geometries.
            if (poSrcGeom->getDimension() == 2)
            {
                if (!poSrcGeom->IsValid())
                {
                    return {
                        nullptr,
                        CPLSPrintf("Geometry of feature " CPL_FRMT_GIB " of %s "
                                   "is invalid. You may be able to correct it "
                                   "with 'gdal vector geom make-valid'.",
                                   poFeat->GetFID(), poDS->GetDescription())};
                }
                else
                {
                    oGC.addGeometry(std::move(poSrcGeom));
                }
            }
            else
            {
                CPLErrorOnce(CE_Warning, CPLE_AppDefined,
                             "Non-polygonal geometry encountered in clipping "
                             "dataset will be ignored.");
            }
        }
    }

    if (!m_likeSQL.empty())
        poDS->ReleaseResultSet(poLyr);

    if (oGC.IsEmpty())
    {
        return {nullptr, "No clipping geometry found"};
    }

    return {std::unique_ptr<OGRGeometry>(oGC.UnaryUnion()), std::string()};
}

/************************************************************************/
/*                           GetClipGeometry()                          */
/************************************************************************/

std::pair<std::unique_ptr<OGRGeometry>, std::string>
GDALClipCommon::GetClipGeometry()
{

    std::unique_ptr<OGRGeometry> poClipGeom;

    if (!m_bbox.empty())
    {
        poClipGeom = std::make_unique<OGRPolygon>(m_bbox[0], m_bbox[1],
                                                  m_bbox[2], m_bbox[3]);

        if (!m_bboxCrs.empty())
        {
            auto poSRS = new OGRSpatialReference();
            poSRS->SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);
            CPL_IGNORE_RET_VAL(poSRS->SetFromUserInput(m_bboxCrs.c_str()));
            poClipGeom->assignSpatialReference(poSRS);
            poSRS->Release();
        }
    }
    else if (!m_geometry.empty())
    {
        {
            CPLErrorStateBackuper oErrorStateBackuper(CPLQuietErrorHandler);
            auto [poGeom, eErr] =
                OGRGeometryFactory::createFromWkt(m_geometry.c_str());
            if (eErr == OGRERR_NONE)
            {
                poClipGeom = std::move(poGeom);
            }
            else
            {
                poClipGeom.reset(
                    OGRGeometryFactory::createFromGeoJson(m_geometry.c_str()));
                if (poClipGeom && poClipGeom->getSpatialReference() == nullptr)
                {
                    auto poSRS = new OGRSpatialReference();
                    poSRS->SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);
                    CPL_IGNORE_RET_VAL(poSRS->SetFromUserInput("WGS84"));
                    poClipGeom->assignSpatialReference(poSRS);
                    poSRS->Release();
                }
            }
        }
        if (!poClipGeom)
        {
            return {
                nullptr,
                "Clipping geometry is neither a valid WKT or GeoJSON geometry"};
        }

        if (!m_geometryCrs.empty())
        {
            auto poSRS = new OGRSpatialReference();
            poSRS->SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);
            // Validity of CRS already checked by GDALAlgorithm
            CPL_IGNORE_RET_VAL(poSRS->SetFromUserInput(m_geometryCrs.c_str()));
            poClipGeom->assignSpatialReference(poSRS);
            poSRS->Release();
        }
    }
    else if (auto poLikeDS = m_likeDataset.GetDatasetRef())
    {
        if (poLikeDS->GetLayerCount() > 1 && m_likeLayer.empty() &&
            m_likeSQL.empty())
        {
            return {
                nullptr,
                "Only single layer dataset can be specified with --like when "
                "neither --like-layer or --like-sql have been specified"};
        }
        else if (poLikeDS->GetLayerCount() > 0)
        {
            std::string errMsg;
            std::tie(poClipGeom, errMsg) = LoadGeometry();
            if (!poClipGeom)
                return {nullptr, errMsg};
        }
        else if (poLikeDS->GetRasterCount() > 0)
        {
            double adfGT[6];
            if (poLikeDS->GetGeoTransform(adfGT) != CE_None)
            {
                return {
                    nullptr,
                    CPLSPrintf(
                        "Dataset '%s' has no geotransform matrix. Its bounds "
                        "cannot be established.",
                        poLikeDS->GetDescription())};
            }
            auto poLikeSRS = poLikeDS->GetSpatialRef();
            const double dfTLX = adfGT[0];
            const double dfTLY = adfGT[3];

            double dfTRX = 0;
            double dfTRY = 0;
            GDALApplyGeoTransform(adfGT, poLikeDS->GetRasterXSize(), 0, &dfTRX,
                                  &dfTRY);

            double dfBLX = 0;
            double dfBLY = 0;
            GDALApplyGeoTransform(adfGT, 0, poLikeDS->GetRasterYSize(), &dfBLX,
                                  &dfBLY);

            double dfBRX = 0;
            double dfBRY = 0;
            GDALApplyGeoTransform(adfGT, poLikeDS->GetRasterXSize(),
                                  poLikeDS->GetRasterYSize(), &dfBRX, &dfBRY);

            auto poPoly = std::make_unique<OGRPolygon>();
            auto poLR = std::make_unique<OGRLinearRing>();
            poLR->addPoint(dfTLX, dfTLY);
            poLR->addPoint(dfTRX, dfTRY);
            poLR->addPoint(dfBRX, dfBRY);
            poLR->addPoint(dfBLX, dfBLY);
            poLR->addPoint(dfTLX, dfTLY);
            poPoly->addRingDirectly(poLR.release());
            poPoly->assignSpatialReference(poLikeSRS);
            poClipGeom = std::move(poPoly);
        }
        else
        {
            return {nullptr, "Cannot get extent from clip dataset"};
        }
    }
    else
    {
        return {nullptr, "--bbox, --geometry or --like must be specified"};
    }

    return {std::move(poClipGeom), std::string()};
}

//! @endcond
