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

#include "cpl_port.h"

#include <cctype>
#include <cerrno>
#include <climits>
#include <cmath>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#if HAVE_FCNTL_H
#  include <fcntl.h>
#endif

#include <limits>
#include <memory>

#include "cpl_conv.h"
#include "cpl_error.h"
#include "cpl_progress.h"
#include "cpl_string.h"
#include "cpl_vsi.h"
#include "gdal.h"
#include "gdal_frmts.h"
#include "gdal_pam.h"
#include "gdal_priv.h"
#include "gdal_rat.h"
#include "ogr_core.h"
#include "ogr_spatialref.h"

class SIGDEMRasterBand;

#pragma pack(push, 1)
struct SIGDEMHeader {
    char acFileType[6];
    int16_t version;
    int32_t nCoordinateSystemId;
    double dfOffsetX;
    double dfScaleFactorX;
    double dfOffsetY;
    double dfScaleFactorY;
    double dfOffsetZ;
    double dfScaleFactorZ;
    double dfMinX;
    double dfMinY;
    double dfMinZ;
    double dfMaxX;
    double dfMaxY;
    double dfMaxZ;
    int32_t nCols;
    int32_t nRows;
    double dfXDim;
    double dfYDim;
};
#pragma pack(push, 1)

class SIGDEMDataset final: public GDALPamDataset {
    friend class SIGDEMRasterBand;

    VSILFILE *fpImage;  // image data file.

    bool bGotTransform { };
    double adfGeoTransform[6] { 0, 1, 0, 0, 0, 1 };
    char *pszProjection { };

    SIGDEMHeader sHeader;
    bool bHdrDirty = false;

    CPLErr RewritSIGDEM();

    CPL_DISALLOW_COPY_ASSIGN(SIGDEMDataset)

public:
    SIGDEMDataset(SIGDEMHeader& sHeaderIn);
    ~SIGDEMDataset() override;

    CPLErr GetGeoTransform(double *padfTransform) override;
    const char* GetProjectionRef(void) override;
    CPLErr SetGeoTransform(double *padfTransform) override;
    CPLErr SetProjection(const char* pszProjectionIn) override;

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
    int bDirty { };
    int bOwnsFP { };
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
