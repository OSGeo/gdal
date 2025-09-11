/******************************************************************************
 *
 * Name:     gdal_maskbands.h
 * Project:  GDAL Core
 * Purpose:  Declaration of mask related subclasses of GDALRasterBand
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 1998, Frank Warmerdam
 * Copyright (c) 2007-2014, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#ifndef GDALMASKBANDS_H_INCLUDED
#define GDALMASKBANDS_H_INCLUDED

#include "cpl_port.h"
#include "gdal_rasterband.h"

#include <cstddef>

//! @cond Doxygen_Suppress
/* ******************************************************************** */
/*                         GDALAllValidMaskBand                         */
/* ******************************************************************** */

class CPL_DLL GDALAllValidMaskBand final : public GDALRasterBand
{
  protected:
    CPLErr IReadBlock(int, int, void *) override;

    CPLErr IRasterIO(GDALRWFlag eRWFlag, int nXOff, int nYOff, int nXSize,
                     int nYSize, void *pData, int nBufXSize, int nBufYSize,
                     GDALDataType eBufType, GSpacing nPixelSpace,
                     GSpacing nLineSpace,
                     GDALRasterIOExtraArg *psExtraArg) override;

    bool
    EmitErrorMessageIfWriteNotSupported(const char *pszCaller) const override;

    CPL_DISALLOW_COPY_ASSIGN(GDALAllValidMaskBand)

  public:
    explicit GDALAllValidMaskBand(GDALRasterBand *);
    ~GDALAllValidMaskBand() override;

    GDALRasterBand *GetMaskBand() override;
    int GetMaskFlags() override;

    bool IsMaskBand() const override
    {
        return true;
    }

    GDALMaskValueRange GetMaskValueRange() const override
    {
        return GMVR_0_AND_255_ONLY;
    }

    CPLErr ComputeStatistics(int bApproxOK, double *pdfMin, double *pdfMax,
                             double *pdfMean, double *pdfStdDev,
                             GDALProgressFunc, void *pProgressData) override;
};

/* ******************************************************************** */
/*                         GDALNoDataMaskBand                           */
/* ******************************************************************** */

class CPL_DLL GDALNoDataMaskBand final : public GDALRasterBand
{
    friend class GDALRasterBand;
    double m_dfNoDataValue = 0;
    int64_t m_nNoDataValueInt64 = 0;
    uint64_t m_nNoDataValueUInt64 = 0;
    GDALRasterBand *m_poParent = nullptr;

    CPL_DISALLOW_COPY_ASSIGN(GDALNoDataMaskBand)

  protected:
    CPLErr IReadBlock(int, int, void *) override;
    CPLErr IRasterIO(GDALRWFlag, int, int, int, int, void *, int, int,
                     GDALDataType, GSpacing, GSpacing,
                     GDALRasterIOExtraArg *psExtraArg) override;

    bool
    EmitErrorMessageIfWriteNotSupported(const char *pszCaller) const override;

  public:
    explicit GDALNoDataMaskBand(GDALRasterBand *);
    explicit GDALNoDataMaskBand(GDALRasterBand *, double dfNoDataValue);
    ~GDALNoDataMaskBand() override;

    bool IsMaskBand() const override
    {
        return true;
    }

    GDALMaskValueRange GetMaskValueRange() const override
    {
        return GMVR_0_AND_255_ONLY;
    }

    static bool IsNoDataInRange(double dfNoDataValue, GDALDataType eDataType);
};

/* ******************************************************************** */
/*                  GDALNoDataValuesMaskBand                            */
/* ******************************************************************** */

class CPL_DLL GDALNoDataValuesMaskBand final : public GDALRasterBand
{
    double *padfNodataValues;

    CPL_DISALLOW_COPY_ASSIGN(GDALNoDataValuesMaskBand)

  protected:
    CPLErr IReadBlock(int, int, void *) override;

    bool
    EmitErrorMessageIfWriteNotSupported(const char *pszCaller) const override;

  public:
    explicit GDALNoDataValuesMaskBand(GDALDataset *);
    ~GDALNoDataValuesMaskBand() override;

    bool IsMaskBand() const override
    {
        return true;
    }

    GDALMaskValueRange GetMaskValueRange() const override
    {
        return GMVR_0_AND_255_ONLY;
    }
};

/* ******************************************************************** */
/*                         GDALRescaledAlphaBand                        */
/* ******************************************************************** */

class GDALRescaledAlphaBand final : public GDALRasterBand
{
    GDALRasterBand *poParent;
    void *pTemp;

    CPL_DISALLOW_COPY_ASSIGN(GDALRescaledAlphaBand)

  protected:
    CPLErr IReadBlock(int, int, void *) override;
    CPLErr IRasterIO(GDALRWFlag, int, int, int, int, void *, int, int,
                     GDALDataType, GSpacing, GSpacing,
                     GDALRasterIOExtraArg *psExtraArg) override;

    bool
    EmitErrorMessageIfWriteNotSupported(const char *pszCaller) const override;

  public:
    explicit GDALRescaledAlphaBand(GDALRasterBand *);
    ~GDALRescaledAlphaBand() override;

    bool IsMaskBand() const override
    {
        return true;
    }
};

//! @endcond

#endif
