/******************************************************************************
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  Implementation of PMTiles
 * Author:   Even Rouault <even.rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2026, Even Rouault <even.rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include "ogr_pmtiles.h"

/************************************************************************/
/*                       GDALPMTilesRasterBand()                        */
/************************************************************************/

GDALPMTilesRasterBand::GDALPMTilesRasterBand(OGRPMTilesDataset *poDSIn,
                                             int nBandIn, int nBlockSize)
{
    poDS = poDSIn;
    nBand = nBandIn;
    eDataType = GDT_Byte;
    nRasterXSize = poDS->GetRasterXSize();
    nRasterYSize = poDS->GetRasterYSize();
    nBlockXSize = nBlockSize;
    nBlockYSize = nBlockSize;
}

/************************************************************************/
/*                          GetOverviewCount()                          */
/************************************************************************/

int GDALPMTilesRasterBand::GetOverviewCount()
{
    auto poGDS = cpl::down_cast<OGRPMTilesDataset *>(poDS);
    return static_cast<int>(poGDS->m_apoOverviews.size());
}

/************************************************************************/
/*                            GetOverview()                             */
/************************************************************************/

GDALRasterBand *GDALPMTilesRasterBand::GetOverview(int idx)
{
    if (idx < 0 || idx >= GetOverviewCount())
        return nullptr;
    auto poGDS = cpl::down_cast<OGRPMTilesDataset *>(poDS);
    return poGDS->m_apoOverviews[idx]->GetRasterBand(nBand);
}

/************************************************************************/
/*                       GetColorInterpretation()                       */
/************************************************************************/

GDALColorInterp GDALPMTilesRasterBand::GetColorInterpretation()
{
    if (poDS->GetRasterCount() <= 2)
        return nBand == 1 ? GCI_GrayIndex : GCI_AlphaBand;
    return static_cast<GDALColorInterp>(GCI_RedBand + nBand - 1);
}

/************************************************************************/
/*                             IReadBlock()                             */
/************************************************************************/

CPLErr GDALPMTilesRasterBand::IReadBlock(int nBlockXOff, int nBlockYOff,
                                         void *pData)
{
    int nXSize, nYSize;
    GetActualBlockSize(nBlockXOff, nBlockYOff, &nXSize, &nYSize);
    const GSpacing nDTSize = GDALGetDataTypeSizeBytes(eDataType);
    GDALRasterIOExtraArg sExtraArg;
    INIT_RASTERIO_EXTRA_ARG(sExtraArg);
    return IRasterIO(GF_Read, nBlockXOff * nBlockXSize,
                     nBlockYOff * nBlockYSize, nXSize, nYSize, pData, nXSize,
                     nYSize, eDataType, nDTSize, nDTSize * nBlockXSize,
                     &sExtraArg);
}

/************************************************************************/
/*                             IRasterIO()                              */
/************************************************************************/

CPLErr GDALPMTilesRasterBand::IRasterIO(
    GDALRWFlag eRWFlag, int nXOff, int nYOff, int nXSize, int nYSize,
    void *pData, int nBufXSize, int nBufYSize, GDALDataType eBufType,
    GSpacing nPixelSpace, GSpacing nLineSpace, GDALRasterIOExtraArg *psExtraArg)
{
    // Re-route to dataset IRasterIO
    auto poGDS = cpl::down_cast<OGRPMTilesDataset *>(poDS);
    int anBand[] = {nBand};
    return poGDS->IRasterIO(eRWFlag, nXOff, nYOff, nXSize, nYSize, pData,
                            nBufXSize, nBufYSize, eBufType, 1, anBand,
                            nPixelSpace, nLineSpace, 0, psExtraArg);
}
