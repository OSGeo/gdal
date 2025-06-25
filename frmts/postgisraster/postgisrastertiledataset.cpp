/***********************************************************************
 * File :    postgisrastertiledataset.cpp
 * Project:  PostGIS Raster driver
 * Purpose:  GDAL Dataset implementation for PostGIS Raster tile
 * Author:   Jorge Arevalo, jorge.arevalo@deimos-space.com
 *                          jorgearevalo@libregis.org
 *
 *
 ***********************************************************************
 * Copyright (c) 2013, Jorge Arevalo
 * Copyright (c) 2013, Even Rouault
 *
 * SPDX-License-Identifier: MIT
 ************************************************************************/
#include "postgisraster.h"

/************************
 * \brief Constructor
 ************************/
PostGISRasterTileDataset::PostGISRasterTileDataset(
    PostGISRasterDataset *poRDSIn, int nXSize, int nYSize)
    : poRDS(poRDSIn), pszPKID(nullptr)
{
    nRasterXSize = nXSize;
    nRasterYSize = nYSize;
}

/************************
 * \brief Destructor
 ************************/
PostGISRasterTileDataset::~PostGISRasterTileDataset()
{
    if (pszPKID)
    {
        CPLFree(pszPKID);
        pszPKID = nullptr;
    }
}

/********************************************************
 * \brief Get the affine transformation coefficients
 ********************************************************/
CPLErr PostGISRasterTileDataset::GetGeoTransform(GDALGeoTransform &gt) const
{
    gt = m_gt;
    return CE_None;
}

/********************************************************
 * \brief Return spatial extent of tile
 ********************************************************/
void PostGISRasterTileDataset::GetNativeExtent(double *pdfMinX, double *pdfMinY,
                                               double *pdfMaxX,
                                               double *pdfMaxY) const
{
    // FIXME; incorrect in case of non 0 rotation terms

    double dfMinX = m_gt[GEOTRSFRM_TOPLEFT_X];
    double dfMaxY = m_gt[GEOTRSFRM_TOPLEFT_Y];

    double dfMaxX = m_gt[GEOTRSFRM_TOPLEFT_X] +
                    nRasterXSize * m_gt[GEOTRSFRM_WE_RES] +
                    nRasterYSize * m_gt[GEOTRSFRM_ROTATION_PARAM1];

    double dfMinY = m_gt[GEOTRSFRM_TOPLEFT_Y] +
                    nRasterXSize * m_gt[GEOTRSFRM_ROTATION_PARAM2] +
                    nRasterYSize * m_gt[GEOTRSFRM_NS_RES];

    // In case yres > 0
    if (dfMinY > dfMaxY)
    {
        double dfTemp = dfMinY;
        dfMinY = dfMaxY;
        dfMaxY = dfTemp;
    }

    *pdfMinX = dfMinX;
    *pdfMinY = dfMinY;
    *pdfMaxX = dfMaxX;
    *pdfMaxY = dfMaxY;
}
