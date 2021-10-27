/******************************************************************************
 * $Id$
 *
 * Project:  R Format Driver
 * Purpose:  Read/write R stats package object format.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2009, Frank Warmerdam <warmerdam@pobox.com>
 * Copyright (c) 2009-2010, Even Rouault <even dot rouault at spatialys.com>
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

#include <cstddef>
#include <cstdlib>
#include <cstring>
#include <string>
#if HAVE_FCNTL_H
#  include <fcntl.h>
#endif

#include "cpl_conv.h"
#include "cpl_error.h"
#include "cpl_port.h"
#include "cpl_progress.h"
#include "cpl_string.h"
#include "cpl_vsi.h"
#include "gdal.h"
#include "gdal_frmts.h"
#include "gdal_pam.h"
#include "gdal_priv.h"
#include "rawdataset.h"

GDALDataset *
RCreateCopy( const char * pszFilename, GDALDataset *poSrcDS,
             int bStrict, char ** papszOptions,
             GDALProgressFunc pfnProgress, void * pProgressData );

/************************************************************************/
/* ==================================================================== */
/*                               RDataset                               */
/* ==================================================================== */
/************************************************************************/

class RDataset final: public GDALPamDataset
{
    friend class RRasterBand;
    VSILFILE   *fp;
    int         bASCII;
    CPLString   osLastStringRead;

    vsi_l_offset nStartOfData;

    double     *padfMatrixValues;

    const char *ASCIIFGets();
    int         ReadInteger();
    double      ReadFloat();
    const char *ReadString();
    bool        ReadPair( CPLString &osItemName, int &nItemType );

  public:
                RDataset();
                ~RDataset();

    static GDALDataset  *Open( GDALOpenInfo * );
    static int          Identify( GDALOpenInfo * );
};

/************************************************************************/
/* ==================================================================== */
/*                            RRasterBand                               */
/* ==================================================================== */
/************************************************************************/

class RRasterBand final: public GDALPamRasterBand
{
    friend class RDataset;

    const double *padfMatrixValues;

  public:
                RRasterBand( RDataset *, int, const double * );
    virtual ~RRasterBand() {}

    virtual CPLErr          IReadBlock( int, int, void * ) override;
};
