/******************************************************************************
 *
 * Project:  CEOS Translator
 * Purpose:  Public (C callable) interface for CEOS and related formats such
 *           as Spot CAP.   This stuff can be used independently of GDAL.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 1999, Frank Warmerdam
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#ifndef CEOSOPEN_H_INCLUDED
#define CEOSOPEN_H_INCLUDED

/* -------------------------------------------------------------------- */
/*      Include standard portability stuff.                             */
/* -------------------------------------------------------------------- */
#include "cpl_conv.h"
#include "cpl_string.h"

/* -------------------------------------------------------------------- */
/*      Base ``class'' for ceos records.                                */
/* -------------------------------------------------------------------- */

CPL_C_START

typedef struct
{
    int nRecordNum;
    GUInt32 nRecordType;
    int nLength;

    char *pachData;
} CEOSRecord;

/* well known record types */
#define CRT_IMAGE_FDR 0x3FC01212
#define CRT_IMAGE_DATA 0xEDED1212

/* -------------------------------------------------------------------- */
/*      Main CEOS info structure.                                       */
/* -------------------------------------------------------------------- */

typedef struct
{

    /* public information */
    int nPixels;
    int nLines;
    int nBands;

    int nBitsPerPixel;

    /* private information */
    VSILFILE *fpImage;

    int bLittleEndian;

    int nImageRecCount;
    int nImageRecLength;

    int nPrefixBytes;
    int nSuffixBytes;

    int *panDataStart;
    int nLineOffset;

} CEOSImage;

/* -------------------------------------------------------------------- */
/*      External Prototypes                                             */
/* -------------------------------------------------------------------- */

CEOSImage CPL_ODLL *CEOSOpen(const char *, const char *);
void CPL_ODLL CEOSClose(CEOSImage *);
CPLErr CPL_ODLL CEOSReadScanline(CEOSImage *psImage, int nBand, int nScanline,
                                 void *pData);

/* -------------------------------------------------------------------- */
/*      Internal prototypes.                                            */
/* -------------------------------------------------------------------- */
CEOSRecord CPL_ODLL *CEOSReadRecord(CEOSImage *);
void CPL_ODLL CEOSDestroyRecord(CEOSRecord *);

CPL_C_END

#endif /* ndef CEOSOPEN_H_INCLUDED */
