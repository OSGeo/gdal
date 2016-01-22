/***********************************************************************
 * File :    postgisrastertiledataset.cpp
 * Project:  PostGIS Raster driver
 * Purpose:  GDAL Dataset implementation for PostGIS Raster tile
 * Author:   Jorge Arevalo, jorge.arevalo@deimos-space.com
 *                          jorgearevalo@libregis.org
 *
 * Last changes: $Id: $
 *
 ***********************************************************************
 * Copyright (c) 2013, Jorge Arevalo
 * Copyright (c) 2013, Even Rouault
 *
 * Permission is hereby granted, free of charge, to any person obtaining 
 * a copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice shall be 
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, 
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND 
 * NONINFRINGEMENT.IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE 
 * LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN 
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN 
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE 
 * SOFTWARE.
 ************************************************************************/
#include "postgisraster.h"

/************************
 * \brief Constructor
 ************************/
PostGISRasterTileDataset::PostGISRasterTileDataset(PostGISRasterDataset* poRDS,
                                                   int nXSize, 
                                                   int nYSize)
{
    this->poRDS = poRDS;
    this->pszPKID = NULL;
    this->nRasterXSize = nXSize;
    this->nRasterYSize = nYSize;

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
    if (pszPKID) {
        CPLFree(pszPKID);
        pszPKID = NULL;
    }
}

/********************************************************
 * \brief Get the affine transformation coefficients
 ********************************************************/
CPLErr PostGISRasterTileDataset::GetGeoTransform(double * padfTransform) {
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
void PostGISRasterTileDataset::GetExtent(double* pdfMinX, double* pdfMinY,
                                         double* pdfMaxX, double* pdfMaxY)
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
    if( dfMinY > dfMaxY )
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
