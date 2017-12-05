/******************************************************************************
 *
 * Project:  ESRI .hdr Driver
 * Purpose:  Implementation of EHdrDataset
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 1999, Frank Warmerdam <warmerdam@pobox.com>
 * Copyright (c) 2007-2013, Even Rouault <even dot rouault at mines-paris dot org>
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

#include "cpl_conv.h"
#include "cpl_error.h"
#include "cpl_progress.h"
#include "cpl_string.h"
#include "cpl_vsi.h"
#include "gdal.h"
#include "gdal_frmts.h"
#include "gdal_pam.h"
#include "gdal_priv.h"
#include "ogr_core.h"
#include "ogr_spatialref.h"

/************************************************************************/
/* ==================================================================== */
/*                       EHdrDataset                                    */
/* ==================================================================== */
/************************************************************************/

class EHdrRasterBand;

class EHdrDataset : public RawDataset
{
    friend class EHdrRasterBand;

    VSILFILE   *fpImage;  // image data file.

    CPLString   osHeaderExt;

    bool        bGotTransform;
    double      adfGeoTransform[6];
    char       *pszProjection;

    bool        bHDRDirty;
    char      **papszHDR;

    bool        bCLRDirty;

    CPLErr      ReadSTX();
    CPLErr      RewriteSTX();
    CPLErr      RewriteHDR();
    void        ResetKeyValue( const char *pszKey, const char *pszValue );
    const char *GetKeyValue( const char *pszKey, const char *pszDefault = "" );
    void        RewriteColorTable( GDALColorTable * );

  public:
    EHdrDataset();
    virtual ~EHdrDataset();

    virtual CPLErr GetGeoTransform( double *padfTransform ) override;
    virtual CPLErr SetGeoTransform( double *padfTransform ) override;
    virtual const char *GetProjectionRef(void) override;
    virtual CPLErr SetProjection( const char * ) override;

    virtual char **GetFileList() override;

    static GDALDataset *Open( GDALOpenInfo * );
    static GDALDataset *Create( const char *pszFilename,
                                int nXSize, int nYSize, int nBands,
                                GDALDataType eType, char **papszParmList );
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

class EHdrRasterBand : public RawRasterBand
{
   friend class EHdrDataset;

    int            nBits;
    vsi_l_offset   nStartBit;
    int            nPixelOffsetBits;
    vsi_l_offset   nLineOffsetBits;

    int            bNoDataSet;  // TODO(schwehr): Convert to bool.
    double         dfNoData;
    double         dfMin;
    double         dfMax;
    double         dfMean;
    double         dfStdDev;

    int            minmaxmeanstddev;

    virtual CPLErr IRasterIO( GDALRWFlag, int, int, int, int,
                              void *, int, int, GDALDataType,
                              GSpacing nPixelSpace,
                              GSpacing nLineSpace,
                              GDALRasterIOExtraArg *psExtraArg ) override;

  public:
    EHdrRasterBand( GDALDataset *poDS, int nBand, VSILFILE *fpRaw,
                    vsi_l_offset nImgOffset, int nPixelOffset,
                    int nLineOffset,
                    GDALDataType eDataType, int bNativeOrder,
                    int nBits);
    virtual ~EHdrRasterBand() {}

    virtual CPLErr IReadBlock( int, int, void * ) override;
    virtual CPLErr IWriteBlock( int, int, void * ) override;

    virtual double GetNoDataValue( int *pbSuccess = NULL ) override;
    virtual double GetMinimum( int *pbSuccess = NULL ) override;
    virtual double GetMaximum(int *pbSuccess = NULL ) override;
    virtual CPLErr GetStatistics( int bApproxOK, int bForce,
                                  double *pdfMin, double *pdfMax,
                                  double *pdfMean, double *pdfStdDev ) override;
    virtual CPLErr SetStatistics( double dfMin, double dfMax,
                                  double dfMean, double dfStdDev ) override;
    virtual CPLErr SetColorTable( GDALColorTable *poNewCT ) override;
};

#endif  // GDAL_FRMTS_RAW_EHDRDATASET_H_INCLUDED
