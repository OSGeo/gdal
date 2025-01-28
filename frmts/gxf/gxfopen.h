/******************************************************************************
 *
 * Project:  GXF Reader
 * Purpose:  GXF-3 access function declarations.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 1998, Global Geomatics
 * Copyright (c) 1998, Frank Warmerdam
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#ifndef GXFOPEN_H_INCLUDED
#define GXFOPEN_H_INCLUDED

/**
 * \file gxfopen.h
 *
 * Public GXF-3 function definitions.
 */

/* -------------------------------------------------------------------- */
/*      Include standard portability stuff.                             */
/* -------------------------------------------------------------------- */
#include "cpl_conv.h"
#include "cpl_string.h"

/* -------------------------------------------------------------------- */
/*      This is consider to be a private structure.                     */
/* -------------------------------------------------------------------- */
struct GXFInfo_t
{
    VSILFILE *fp;

    int nRawXSize;
    int nRawYSize;
    int nSense; /* GXFS_ codes */
    int nGType; /* 0 is uncompressed */

    double dfXPixelSize;
    double dfYPixelSize;
    double dfRotation;
    double dfXOrigin; /* lower left corner */
    double dfYOrigin; /* lower left corner */

    char szDummy[64];
    double dfSetDummyTo;

    char *pszTitle;

    double dfTransformScale;
    double dfTransformOffset;
    char *pszTransformName;

    char **papszMapProjection;
    char **papszMapDatumTransform;

    char *pszUnitName;
    double dfUnitToMeter;

    double dfZMaximum;
    double dfZMinimum;

    vsi_l_offset *panRawLineOffset;
};
typedef struct GXFInfo_t GXFInfo_t;

CPL_C_START

typedef struct GXFInfo_t *GXFHandle;

GXFHandle GXFOpen(const char *pszFilename);

CPLErr GXFGetRawInfo(GXFHandle hGXF, int *pnXSize, int *pnYSize, int *pnSense,
                     double *pdfZMin, double *pdfZMax, double *pdfDummy);
CPLErr GXFGetInfo(GXFHandle hGXF, int *pnXSize, int *pnYSize);

CPLErr GXFGetRawScanline(GXFHandle, int iScanline, double *padfLineBuf);
CPLErr GXFGetScanline(GXFHandle, int iScanline, double *padfLineBuf);

char **GXFGetMapProjection(GXFHandle);
char **GXFGetMapDatumTransform(GXFHandle);
char *GXFGetMapProjectionAsPROJ4(GXFHandle);
char *GXFGetMapProjectionAsOGCWKT(GXFHandle);

CPLErr GXFGetRawPosition(GXFHandle, double *, double *, double *, double *,
                         double *);
CPLErr GXFGetPosition(GXFHandle, double *, double *, double *, double *,
                      double *);

CPLErr GXFGetPROJ4Position(GXFHandle, double *, double *, double *, double *,
                           double *);

void GXFClose(GXFHandle hGXF);

#define GXFS_LL_UP -1
#define GXFS_LL_RIGHT 1
#define GXFS_UL_RIGHT -2
#define GXFS_UL_DOWN 2
#define GXFS_UR_DOWN -3
#define GXFS_UR_LEFT 3
#define GXFS_LR_LEFT -4
#define GXFS_LR_UP 4

CPL_C_END

#endif /* ndef GXFOPEN_H_INCLUDED */
