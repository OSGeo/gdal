/******************************************************************************
 *
 * Project:  GeoTIFF Driver
 * Purpose:  GDAL GeoTIFF support.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 1998, 2002, Frank Warmerdam <warmerdam@pobox.com>
 * Copyright (c) 2007-2015, Even Rouault <even dot rouault at spatialys dot com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#ifndef GTIFFJPEGOVERVIEWDS_H_INCLUDED
#define GTIFFJPEGOVERVIEWDS_H_INCLUDED

#include "gdal_priv.h"

class GTiffDataset;

/************************************************************************/
/* ==================================================================== */
/*                        GTiffJPEGOverviewDS                           */
/* ==================================================================== */
/************************************************************************/

class GTiffJPEGOverviewDS final : public GDALDataset
{
    CPL_DISALLOW_COPY_ASSIGN(GTiffJPEGOverviewDS)

    friend class GTiffJPEGOverviewBand;
    GTiffDataset *m_poParentDS = nullptr;
    int m_nOverviewLevel = 0;

    int m_nJPEGTableSize = 0;
    GByte *m_pabyJPEGTable = nullptr;
    CPLString m_osTmpFilenameJPEGTable{};

    CPLString m_osTmpFilename{};
    std::unique_ptr<GDALDataset> m_poJPEGDS{};
    // Valid block id of the parent DS that match poJPEGDS.
    int m_nBlockId = -1;

  public:
    GTiffJPEGOverviewDS(GTiffDataset *poParentDS, int nOverviewLevel,
                        const void *pJPEGTable, int nJPEGTableSize);
    virtual ~GTiffJPEGOverviewDS();

    virtual CPLErr IRasterIO(GDALRWFlag eRWFlag, int nXOff, int nYOff,
                             int nXSize, int nYSize, void *pData, int nBufXSize,
                             int nBufYSize, GDALDataType eBufType,
                             int nBandCount, BANDMAP_TYPE panBandMap,
                             GSpacing nPixelSpace, GSpacing nLineSpace,
                             GSpacing nBandSpace,
                             GDALRasterIOExtraArg *psExtraArg) override;
};

#endif  // GTIFFJPEGOVERVIEWDS_H_INCLUDED
