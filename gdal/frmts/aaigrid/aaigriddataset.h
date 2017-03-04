/******************************************************************************
 * $Id$
 *
 * Project:  GDAL
 * Purpose:  Implements Arc/Info ASCII Grid Format.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2001, Frank Warmerdam (warmerdam@pobox.com)
 * Copyright (c) 2007-2012, Even Rouault <even dot rouault at mines-paris dot org>
 * Copyright (c) 2014, Kyle Shannon <kyle at pobox dot com>
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
#  include <fcntl.h>
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
    FORMAT_GRASSASCII
} GridFormat;

/************************************************************************/
/* ==================================================================== */
/*                              AAIGDataset                             */
/* ==================================================================== */
/************************************************************************/

class AAIGRasterBand;

class CPL_DLL AAIGDataset : public GDALPamDataset
{
    friend class AAIGRasterBand;

    VSILFILE   *fp;

    char        **papszPrj;
    CPLString   osPrjFilename;
    char        *pszProjection;

    unsigned char achReadBuf[256];
    GUIntBig    nBufferOffset;
    int         nOffsetInBuffer;

    char        Getc();
    GUIntBig    Tell();
    int         Seek( GUIntBig nOffset );

  protected:
    GDALDataType eDataType;
    double      adfGeoTransform[6];
    bool        bNoDataSet;
    double      dfNoDataValue;

    virtual int ParseHeader(const char* pszHeader, const char* pszDataType);

  public:
                AAIGDataset();
       virtual ~AAIGDataset();

    virtual char **GetFileList(void) override;

    static GDALDataset *CommonOpen( GDALOpenInfo * poOpenInfo,
                                    GridFormat eFormat );

    static GDALDataset *Open( GDALOpenInfo * );
    static int          Identify( GDALOpenInfo * );
    static CPLErr       Delete( const char *pszFilename );
    static CPLErr       Remove( const char *pszFilename, int bRepError );
    static GDALDataset *CreateCopy( const char * pszFilename,
                                    GDALDataset *poSrcDS,
                                    int bStrict, char ** papszOptions,
                                    GDALProgressFunc pfnProgress,
                                    void * pProgressData );

    virtual CPLErr GetGeoTransform( double * ) override;
    virtual const char *GetProjectionRef(void) override;
};

/************************************************************************/
/* ==================================================================== */
/*                        GRASSASCIIDataset                             */
/* ==================================================================== */
/************************************************************************/

class GRASSASCIIDataset : public AAIGDataset
{
    virtual int ParseHeader(const char* pszHeader, const char* pszDataType) override;

  public:
                GRASSASCIIDataset() : AAIGDataset() {}
       virtual ~GRASSASCIIDataset() {}

    static GDALDataset *Open( GDALOpenInfo * );
    static int          Identify( GDALOpenInfo * );
};

/************************************************************************/
/* ==================================================================== */
/*                            AAIGRasterBand                             */
/* ==================================================================== */
/************************************************************************/

class AAIGRasterBand : public GDALPamRasterBand
{
    friend class AAIGDataset;

    GUIntBig      *panLineOffset;

  public:

                   AAIGRasterBand( AAIGDataset *, int );
    virtual       ~AAIGRasterBand();

    virtual double GetNoDataValue( int * ) override;
    virtual CPLErr SetNoDataValue( double ) override;
    virtual CPLErr IReadBlock( int, int, void * ) override;
};

#endif  // GDAL_FRMTS_AAIGRID_AAIGRIDDATASET_H_INCLUDED
