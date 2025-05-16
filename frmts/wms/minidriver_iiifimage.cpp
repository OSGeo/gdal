/******************************************************************************
 *
 * Project:  WMS Client Driver
 * Purpose:  Mini driver for International Image Interoperability Framework
 *           Image API (IIIFImage)
 * Author:   Even Rouault <even.rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2025, Even Rouault <even.rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include "wmsdriver.h"
#include "minidriver_iiifimage.h"

#include <algorithm>

// Implements https://iiif.io/api/image/3.0/ "Image API 3.0"

WMSMiniDriver_IIIFImage::WMSMiniDriver_IIIFImage()
{
}

WMSMiniDriver_IIIFImage::~WMSMiniDriver_IIIFImage()
{
}

CPLErr WMSMiniDriver_IIIFImage::Initialize(CPLXMLNode *config,
                                           CPL_UNUSED char **papszOpenOptions)
{
    m_base_url = CPLGetXMLValue(config, "ServerURL", "");
    if (m_base_url.empty())
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "GDALWMS, IIIFImage mini-driver: ServerURL missing.");
        return CE_Failure;
    }

    const char *pszImageFormat =
        CPLGetXMLValue(config, "ImageFormat", "image/jpeg");
    if (EQUAL(pszImageFormat, "image/jpeg"))
        m_imageExtension = "jpg";
    else if (EQUAL(pszImageFormat, "image/png"))
        m_imageExtension = "png";
    else if (EQUAL(pszImageFormat, "image/webp"))
        m_imageExtension = "webp";

    return CE_None;
}

void WMSMiniDriver_IIIFImage::GetCapabilities(WMSMiniDriverCapabilities *caps)
{
    caps->m_overview_dim_computation_method = OVERVIEW_FLOOR;
    caps->m_has_geotransform = false;
}

CPLErr WMSMiniDriver_IIIFImage::TiledImageRequest(
    WMSHTTPRequest &request, const GDALWMSImageRequestInfo & /* iri */,
    const GDALWMSTiledImageRequestInfo &tiri)
{
    CPLString &url = request.URL;
    url = m_base_url;
    if (!url.empty() && url.back() != '/')
        url += '/';

    int nBlockWidth = 0;
    int nBlockHeight = 0;
    m_parent_dataset->GetRasterBand(1)->GetBlockSize(&nBlockWidth,
                                                     &nBlockHeight);

    const int iShift =
        m_parent_dataset->GetRasterBand(1)->GetOverviewCount() - tiri.m_level;

    GDALRasterBand *poOvrBand =
        iShift == 0
            ? m_parent_dataset->GetRasterBand(1)
            : m_parent_dataset->GetRasterBand(1)->GetOverview(
                  m_parent_dataset->GetRasterBand(1)->GetOverviewCount() - 1 -
                  tiri.m_level);

    const int nXOffFullRes = (tiri.m_x * nBlockWidth) << iShift;
    const int nYOffFullRes = (tiri.m_y * nBlockHeight) << iShift;
    url += CPLSPrintf(
        "%d,%d,%d,%d/%d,%d/0/default.%s", nXOffFullRes, nYOffFullRes,
        std::min(nBlockWidth << iShift,
                 m_parent_dataset->GetRasterXSize() - nXOffFullRes),
        std::min(nBlockHeight << iShift,
                 m_parent_dataset->GetRasterYSize() - nYOffFullRes),
        std::min(nBlockWidth, poOvrBand->GetXSize() - tiri.m_x * nBlockWidth),
        std::min(nBlockHeight, poOvrBand->GetYSize() - tiri.m_y * nBlockHeight),
        m_imageExtension.c_str());

    return CE_None;
}
