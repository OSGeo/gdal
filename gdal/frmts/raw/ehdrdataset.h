/******************************************************************************
 *
 * Project:  ESRI .hdr Driver
 * Purpose:  Implementation of EHdrDataset
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 1999, Frank Warmerdam <warmerdam@pobox.com>
 * Copyright (c) 2007-2013, Even Rouault <even dot rouault at spatialys.com>
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

#ifndef GDAL_FRMTS_RAW_EHDRDATASET_H_INCLUDED
#define GDAL_FRMTS_RAW_EHDRDATASET_H_INCLUDED

#include "cpl_port.h"
#include "rawdataset.h"

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

/************************************************************************/
/* ==================================================================== */
/*                       EHdrDataset                                    */
/* ==================================================================== */
/************************************************************************/

class EHdrRasterBand;

class EHdrDataset final: public RawDataset
{
    friend class EHdrRasterBand;

    VSILFILE   *fpImage;  // image data file.

    CPLString   osHeaderExt{};

    bool        bGotTransform{};
    double      adfGeoTransform[6]{0,1,0,0,0,1};
    char       *pszProjection{};

    bool        bHDRDirty{};
    char      **papszHDR{};

    bool        bCLRDirty{};
    std::shared_ptr<GDALColorTable> m_poColorTable{};
    std::shared_ptr<GDALRasterAttributeTable> m_poRAT{};


    CPLErr      ReadSTX() const;
    CPLErr      RewriteSTX() const;
    CPLErr      RewriteHDR();
    void        ResetKeyValue( const char *pszKey, const char *pszValue );
    const char *GetKeyValue( const char *pszKey, const char *pszDefault = "" );
    void        RewriteCLR(GDALRasterBand*) const;

    CPL_DISALLOW_COPY_ASSIGN(EHdrDataset)

  public:
    EHdrDataset();
    ~EHdrDataset() override;

    CPLErr GetGeoTransform( double *padfTransform ) override;
    CPLErr SetGeoTransform( double *padfTransform ) override;
    const char *_GetProjectionRef() override;
    CPLErr _SetProjection( const char * ) override;
    const OGRSpatialReference* GetSpatialRef() const override {
        return GetSpatialRefFromOldGetProjectionRef();
    }
    CPLErr SetSpatialRef(const OGRSpatialReference* poSRS) override {
        return OldSetProjectionFromSetSpatialRef(poSRS);
    }

    char **GetFileList() override;

    static GDALDataset *Open( GDALOpenInfo * );
    static GDALDataset *Open( GDALOpenInfo *, bool bFileSizeCheck );
    static GDALDataset *Create( const char *pszFilename,
                                int nXSize, int nYSize, int nBands,
                                GDALDataType eType, char **papszParamList );
    static GDALDataset *CreateCopy( const char *pszFilename,
                                    GDALDataset *poSrcDS,
                                    int bStrict, char **papszOptions,
                                    GDALProgressFunc pfnProgress,
                                    void *pProgressData );
    static CPLString GetImageRepFilename(const char *pszFilename);
};

/************************************************************************/
/* ==================================================================== */
/*                          EHdrRasterBand                              */
/* ==================================================================== */
/************************************************************************/

class EHdrRasterBand final: public RawRasterBand
{
   friend class EHdrDataset;

    std::shared_ptr<GDALColorTable> m_poColorTable{};
    std::shared_ptr<GDALRasterAttributeTable> m_poRAT{};

    int            nBits{};
    vsi_l_offset   nStartBit{};
    int            nPixelOffsetBits{};
    vsi_l_offset   nLineOffsetBits{};

    int            bNoDataSet{};  // TODO(schwehr): Convert to bool.
    double         dfNoData{};
    double         dfMin{};
    double         dfMax{};
    double         dfMean{};
    double         dfStdDev{};

    int            minmaxmeanstddev{};

    CPLErr IRasterIO( GDALRWFlag, int, int, int, int,
                      void *, int, int, GDALDataType,
                      GSpacing nPixelSpace,
                      GSpacing nLineSpace,
                      GDALRasterIOExtraArg *psExtraArg ) override;

    CPL_DISALLOW_COPY_ASSIGN(EHdrRasterBand)

  public:
    EHdrRasterBand( GDALDataset *poDS, int nBand, VSILFILE *fpRaw,
                    vsi_l_offset nImgOffset, int nPixelOffset,
                    int nLineOffset,
                    GDALDataType eDataType, int bNativeOrder,
                    int nBits);
    ~EHdrRasterBand() override;

    CPLErr IReadBlock( int, int, void * ) override;
    CPLErr IWriteBlock( int, int, void * ) override;

    double GetNoDataValue( int *pbSuccess = nullptr ) override;
    double GetMinimum( int *pbSuccess = nullptr ) override;
    double GetMaximum(int *pbSuccess = nullptr ) override;
    CPLErr GetStatistics( int bApproxOK, int bForce,
                          double *pdfMin, double *pdfMax,
                          double *pdfMean, double *pdfStdDev ) override;
    CPLErr SetStatistics( double dfMin, double dfMax,
                          double dfMean, double dfStdDev ) override;
    CPLErr SetColorTable( GDALColorTable *poNewCT ) override;
    GDALColorTable* GetColorTable() override;

    GDALRasterAttributeTable *GetDefaultRAT() override;
    CPLErr SetDefaultRAT( const GDALRasterAttributeTable * poRAT ) override;
};

#endif  // GDAL_FRMTS_RAW_EHDRDATASET_H_INCLUDED
