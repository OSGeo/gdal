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

    adfGeoTransform[GEOTRSFRM_TOPLEFT_X] = 0;
    adfGeoTransform[GEOTRSFRM_WE_RES] = 1;
    adfGeoTransform[GEOTRSFRM_ROTATION_PARAM1] = 0;
    adfGeoTransform[GEOTRSFRM_TOPLEFT_Y] = 0;
    adfGeoTransform[GEOTRSFRM_ROTATION_PARAM2] = 0;
    adfGeoTransform[GEOTRSFRM_NS_RES] = 1;
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
CPLErr PostGISRasterTileDataset::GetGeoTransform(double *padfTransform)
{
    // copy necessary values in supplied buffer
    padfTransform[0] = adfGeoTransform[0];
    padfTransform[1] = adfGeoTransform[1];
    padfTransform[2] = adfGeoTransform[2];
    padfTransform[3] = adfGeoTransform[3];
    padfTransform[4] = adfGeoTransform[4];
    padfTransform[5] = adfGeoTransform[5];

    return CE_None;
}

/********************************************************
 * \brief Return spatial extent of tile
 ********************************************************/
void PostGISRasterTileDataset::GetExtent(double *pdfMinX, double *pdfMinY,
                                         double *pdfMaxX, double *pdfMaxY) const
{
    // FIXME; incorrect in case of non 0 rotation terms

    double dfMinX = adfGeoTransform[GEOTRSFRM_TOPLEFT_X];
    double dfMaxY = adfGeoTransform[GEOTRSFRM_TOPLEFT_Y];

    double dfMaxX = adfGeoTransform[GEOTRSFRM_TOPLEFT_X] +
                    nRasterXSize * adfGeoTransform[GEOTRSFRM_WE_RES] +
                    nRasterYSize * adfGeoTransform[GEOTRSFRM_ROTATION_PARAM1];

    double dfMinY = adfGeoTransform[GEOTRSFRM_TOPLEFT_Y] +
                    nRasterXSize * adfGeoTransform[GEOTRSFRM_ROTATION_PARAM2] +
                    nRasterYSize * adfGeoTransform[GEOTRSFRM_NS_RES];

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
