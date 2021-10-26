/******************************************************************************
 *
 * Project:  Scaled Integer Gridded DEM .sigdem Driver
 * Purpose:  Implementation of Scaled Integer Gridded DEM
 * Author:   Paul Austin, paul.austin@revolsys.com
 *
 ******************************************************************************
 * Copyright (c) 2018, Paul Austin <paul.austin@revolsys.com>
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

#ifndef GDAL_FRMTS_SIGDEMDATASET_H_INCLUDED
#define GDAL_FRMTS_SIGDEMDATASET_H_INCLUDED

#include <cctype>
#include <cerrno>
#include <climits>
#include <cmath>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <memory>

#include "gdal_pam.h"

class SIGDEMRasterBand;

class SIGDEMHeader {
public:
    int16_t version = 1;
    int32_t nCoordinateSystemId = 0;
    double dfOffsetX = 0;
    double dfScaleFactorX = 1000;
    double dfOffsetY = 0;
    double dfScaleFactorY = 1000;
    double dfOffsetZ = 0;
    double dfScaleFactorZ = 1000;
    double dfMinX = -std::numeric_limits<double>::max();
    double dfMinY = -std::numeric_limits<double>::max();
    double dfMinZ = -std::numeric_limits<double>::max();
    double dfMaxX = std::numeric_limits<double>::max();
    double dfMaxY = std::numeric_limits<double>::max();
    double dfMaxZ = std::numeric_limits<double>::max();
    int32_t nCols = 0;
    int32_t nRows = 0;
    double dfXDim = 1;
    double dfYDim = 1;

    SIGDEMHeader();

    bool Read(const GByte* pabyHeader);

    bool Write(VSILFILE *fp);
};

class SIGDEMDataset final: public GDALPamDataset {
    friend class SIGDEMRasterBand;

    VSILFILE *fpImage;  // image data file.

    double adfGeoTransform[6] { 0, 1, 0, 0, 0, 1 };
    char *pszProjection { };

    SIGDEMHeader sHeader;

    CPLErr RewritSIGDEM();

    CPL_DISALLOW_COPY_ASSIGN(SIGDEMDataset)

public:
    explicit SIGDEMDataset(const SIGDEMHeader& sHeaderIn);
    ~SIGDEMDataset() override;

    CPLErr GetGeoTransform(double *padfTransform) override;
    const char* _GetProjectionRef(void) override;
    const OGRSpatialReference* GetSpatialRef() const override {
        return GetSpatialRefFromOldGetProjectionRef();
    }

    static GDALDataset *CreateCopy(
        const char *pszFilename,
        GDALDataset *poSrcDS,
        int bStrict,
        char **papszOptions,
        GDALProgressFunc pfnProgress,
        void *pProgressData);
    static int Identify(GDALOpenInfo *);
    static GDALDataset *Open(GDALOpenInfo *);
};

class SIGDEMRasterBand final: public GDALPamRasterBand {
    friend class SIGDEMDataset;

    CPL_DISALLOW_COPY_ASSIGN(SIGDEMRasterBand)

private:
    double dfOffsetZ { };
    double dfScaleFactorZ { };
    VSILFILE* fpRawL { };
    int nBlockSizeBytes { };
    int nLoadedBlockIndex = -1;
    int32_t* pBlockBuffer { };

public:
    SIGDEMRasterBand(
        SIGDEMDataset *poDS,
        VSILFILE *fpRaw,
        double dfMinZ,
        double dfMaxZ);
    ~SIGDEMRasterBand() override;

    CPLErr IReadBlock(int nBlockXOff, int nBlockYOff, void *pImage) override;

    CPLErr IWriteBlock( CPL_UNUSED int nBlockXOff, int nBlockYOff, void *pImage)
            override;
};

#endif  // GDAL_FRMTS_SIGDEMDATASET_H_INCLUDED
