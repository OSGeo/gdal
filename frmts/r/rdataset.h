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
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#ifndef RDATASET_H_INCLUDED
#define RDATASET_H_INCLUDED

#include <cstddef>
#include <cstdlib>
#include <cstring>
#include <string>
#if HAVE_FCNTL_H
#include <fcntl.h>
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

GDALDataset *RCreateCopy(const char *pszFilename, GDALDataset *poSrcDS,
                         int bStrict, char **papszOptions,
                         GDALProgressFunc pfnProgress, void *pProgressData);

/************************************************************************/
/* ==================================================================== */
/*                               RDataset                               */
/* ==================================================================== */
/************************************************************************/

class RDataset final : public GDALPamDataset
{
    friend class RRasterBand;
    VSILFILE *fp;
    int bASCII;
    CPLString osLastStringRead;

    vsi_l_offset nStartOfData;

    double *padfMatrixValues;

    const char *ASCIIFGets();
    int ReadInteger();
    double ReadFloat();
    const char *ReadString();
    bool ReadPair(CPLString &osItemName, int &nItemType);

  public:
    RDataset();
    ~RDataset();

    static GDALDataset *Open(GDALOpenInfo *);
    static int Identify(GDALOpenInfo *);
};

/************************************************************************/
/* ==================================================================== */
/*                            RRasterBand                               */
/* ==================================================================== */
/************************************************************************/

class RRasterBand final : public GDALPamRasterBand
{
    friend class RDataset;

    const double *padfMatrixValues;

  public:
    RRasterBand(RDataset *, int, const double *);

    virtual ~RRasterBand()
    {
    }

    virtual CPLErr IReadBlock(int, int, void *) override;
};

#endif /* RDATASET_H_INCLUDED */
