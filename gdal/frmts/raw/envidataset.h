/******************************************************************************
 * $Id$
 *
 * Project:  ENVI .hdr Driver
 * Purpose:  Implementation of ENVI .hdr labelled raw raster support.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 * Maintainer: Chris Padwick (cpadwick at ittvis.com)
 *
 ******************************************************************************
 * Copyright (c) 2002, Frank Warmerdam
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

#ifndef GDAL_FRMTS_RAW_ENVIDATASET_H_INCLUDED
#define GDAL_FRMTS_RAW_ENVIDATASET_H_INCLUDED

#include "cpl_port.h"
#include "rawdataset.h"

#include <climits>
#include <cmath>
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
#include "cpl_string.h"
#include "cpl_vsi.h"
#include "gdal.h"
#include "gdal_frmts.h"
#include "gdal_priv.h"
#include "ogr_core.h"
#include "ogr_spatialref.h"
#include "ogr_srs_api.h"

class ENVIRasterBand;

class ENVIDataset : public RawDataset
{
    friend class ENVIRasterBand;

    VSILFILE   *fpImage;  // Image data file.
    VSILFILE   *fp;  // Header file
    char       *pszHDRFilename;

    bool        bFoundMapinfo;
    bool        bHeaderDirty;
    bool        bFillFile;

    double      adfGeoTransform[6];

    char       *pszProjection;

    char        **papszHeader;

    CPLString   osStaFilename;

    bool        ReadHeader( VSILFILE * );
    bool        ProcessMapinfo( const char * );
    void        ProcessRPCinfo( const char *, int, int);
    void        ProcessStatsFile();
    static int         byteSwapInt(int);
    static float       byteSwapFloat(float);
    static double      byteSwapDouble(double);
    static void        SetENVIDatum( OGRSpatialReference *, const char * );
    static void        SetENVIEllipse( OGRSpatialReference *, char ** );
    void        WriteProjectionInfo();
    bool        ParseRpcCoeffsMetaDataString( const char *psName,
                                              char *papszVal[], int& idx );
    bool        WriteRpcInfo();
    bool        WritePseudoGcpInfo();

    void        SetFillFile() { bFillFile = true; }

    char        **SplitList( const char * );

    enum Interleave { BSQ, BIL, BIP } interleave;
    static int GetEnviType(GDALDataType eType);

  public:
            ENVIDataset();
    virtual ~ENVIDataset();

    virtual void    FlushCache() override;
    virtual CPLErr  GetGeoTransform( double *padfTransform ) override;
    virtual CPLErr  SetGeoTransform( double * ) override;
    virtual const char *GetProjectionRef(void) override;
    virtual CPLErr  SetProjection( const char * ) override;
    virtual char  **GetFileList(void) override;

    virtual void        SetDescription( const char * ) override;

    virtual CPLErr      SetMetadata( char **papszMetadata,
                                     const char *pszDomain = "" ) override;
    virtual CPLErr      SetMetadataItem( const char *pszName,
                                         const char *pszValue,
                                         const char *pszDomain = "" ) override;
    virtual CPLErr SetGCPs( int nGCPCount, const GDAL_GCP *pasGCPList,
                            const char *pszGCPProjection ) override;

    static GDALDataset *Open( GDALOpenInfo * );
    static GDALDataset *Create( const char *pszFilename,
                                int nXSize, int nYSize, int nBands,
                                GDALDataType eType, char ** papszOptions );
};

/************************************************************************/
/* ==================================================================== */
/*                            ENVIRasterBand                            */
/* ==================================================================== */
/************************************************************************/

class ENVIRasterBand : public RawRasterBand
{
    public:
                ENVIRasterBand( GDALDataset *poDS, int nBand, void *fpRaw,
                                vsi_l_offset nImgOffset, int nPixelOffset,
                                int nLineOffset,
                                GDALDataType eDataType, int bNativeOrder,
                                int bIsVSIL = FALSE, int bOwnsFP = FALSE );
    virtual ~ENVIRasterBand() {}

    virtual void        SetDescription( const char * ) override;

    virtual CPLErr SetCategoryNames( char ** ) override;
};

#endif  // GDAL_FRMTS_RAW_ENVIDATASET_H_INCLUDED
