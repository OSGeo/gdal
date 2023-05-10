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
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
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
                             int nBandCount, int *panBandMap,
                             GSpacing nPixelSpace, GSpacing nLineSpace,
                             GSpacing nBandSpace,
                             GDALRasterIOExtraArg *psExtraArg) override;
};

#endif  // GTIFFJPEGOVERVIEWDS_H_INCLUDED
