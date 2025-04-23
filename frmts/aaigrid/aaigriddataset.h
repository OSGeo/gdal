/******************************************************************************
 *
 * Project:  GDAL
 * Purpose:  Implements Arc/Info ASCII Grid Format.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2001, Frank Warmerdam (warmerdam@pobox.com)
 * Copyright (c) 2007-2012, Even Rouault <even dot rouault at spatialys.com>
 * Copyright (c) 2014, Kyle Shannon <kyle at pobox dot com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#ifndef GDAL_FRMTS_AAIGRID_AAIGRIDDATASET_H_INCLUDED
#define GDAL_FRMTS_AAIGRID_AAIGRIDDATASET_H_INCLUDED

// We need cpl_port as first include to avoid VSIStatBufL being not
// defined on i586-mingw32msvc.
#include "cpl_port.h"
#include "gdal_frmts.h"

#include <cctype>
#include <climits>
#include <cmath>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#if HAVE_FCNTL_H
#include <fcntl.h>
#endif

#include <algorithm>
#include <limits>
#include <string>

#include "cpl_conv.h"
#include "cpl_error.h"
#include "cpl_progress.h"
#include "cpl_string.h"
#include "cpl_vsi.h"
#include "gdal.h"
#include "gdal_pam.h"
#include "gdal_priv.h"
#include "ogr_core.h"
#include "ogr_spatialref.h"

typedef enum
{
    FORMAT_AAIG,
    FORMAT_GRASSASCII,
    FORMAT_ISG,
} GridFormat;

/************************************************************************/
/* ==================================================================== */
/*                              AAIGDataset                             */
/* ==================================================================== */
/************************************************************************/

class AAIGRasterBand;

class AAIGDataset CPL_NON_FINAL : public GDALPamDataset
{
    friend class AAIGRasterBand;

    VSILFILE *fp;

    char **papszPrj;
    CPLString osPrjFilename;
    OGRSpatialReference m_oSRS{};

    unsigned char achReadBuf[256];
    GUIntBig nBufferOffset;
    int nOffsetInBuffer;

    char Getc();
    GUIntBig Tell() const;
    int Seek(GUIntBig nOffset);

  protected:
    GDALDataType eDataType;
    double adfGeoTransform[6];
    bool bNoDataSet;
    double dfNoDataValue;
    CPLString osUnits{};

    virtual int ParseHeader(const char *pszHeader, const char *pszDataType);

  public:
    AAIGDataset();
    ~AAIGDataset() override;

    char **GetFileList(void) override;

    static GDALDataset *CommonOpen(GDALOpenInfo *poOpenInfo,
                                   GridFormat eFormat);

    static GDALDataset *Open(GDALOpenInfo *);
    static int Identify(GDALOpenInfo *);
    static CPLErr Delete(const char *pszFilename);
    static CPLErr Remove(const char *pszFilename, int bRepError);
    static GDALDataset *CreateCopy(const char *pszFilename,
                                   GDALDataset *poSrcDS, int bStrict,
                                   char **papszOptions,
                                   GDALProgressFunc pfnProgress,
                                   void *pProgressData);

    CPLErr GetGeoTransform(double *) override;
    const OGRSpatialReference *GetSpatialRef() const override;
};

/************************************************************************/
/* ==================================================================== */
/*                        GRASSASCIIDataset                             */
/* ==================================================================== */
/************************************************************************/

class GRASSASCIIDataset final : public AAIGDataset
{
    int ParseHeader(const char *pszHeader, const char *pszDataType) override;

  public:
    GRASSASCIIDataset() : AAIGDataset()
    {
    }

    ~GRASSASCIIDataset() override
    {
    }

    static GDALDataset *Open(GDALOpenInfo *);
    static int Identify(GDALOpenInfo *);
};

/************************************************************************/
/* ==================================================================== */
/*                           ISGDataset                                 */
/* ==================================================================== */
/************************************************************************/

class ISGDataset final : public AAIGDataset
{
    int ParseHeader(const char *pszHeader, const char *pszDataType) override;

  public:
    ISGDataset() : AAIGDataset()
    {
    }

    static GDALDataset *Open(GDALOpenInfo *);
    static int Identify(GDALOpenInfo *);
};

/************************************************************************/
/* ==================================================================== */
/*                            AAIGRasterBand                             */
/* ==================================================================== */
/************************************************************************/

class AAIGRasterBand final : public GDALPamRasterBand
{
    friend class AAIGDataset;

    GUIntBig *panLineOffset;

  public:
    AAIGRasterBand(AAIGDataset *, int);
    ~AAIGRasterBand() override;

    double GetNoDataValue(int *) override;
    CPLErr SetNoDataValue(double) override;
    CPLErr IReadBlock(int, int, void *) override;
};

#endif  // GDAL_FRMTS_AAIGRID_AAIGRIDDATASET_H_INCLUDED
