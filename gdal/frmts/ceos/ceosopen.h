/******************************************************************************
 * $Id$
 *
 * Project:  CEOS Translator
 * Purpose:  Public (C callable) interface for CEOS and related formats such
 *           as Spot CAP.   This stuff can be used independently of GDAL.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 1999, Frank Warmerdam
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

typedef struct {
    int         nRecordNum;
    GUInt32     nRecordType;
    int         nLength;

    char        *pachData;
}CEOSRecord;

/* well known record types */
#define CRT_IMAGE_FDR   0x3FC01212
#define CRT_IMAGE_DATA  0xEDED1212

/* -------------------------------------------------------------------- */
/*      Main CEOS info structure.                                       */
/* -------------------------------------------------------------------- */

typedef struct {

    /* public information */
    int         nPixels;
    int         nLines;
    int         nBands;

    int         nBitsPerPixel;

    /* private information */
    VSILFILE    *fpImage;

    int         bLittleEndian;

    int         nImageRecCount;
    int         nImageRecLength;

    int         nPrefixBytes;
    int         nSuffixBytes;

    int         *panDataStart;
    int         nLineOffset;

} CEOSImage;

/* -------------------------------------------------------------------- */
/*      External Prototypes                                             */
/* -------------------------------------------------------------------- */

CEOSImage CPL_ODLL *CEOSOpen( const char *, const char * );
void CPL_ODLL       CEOSClose( CEOSImage * );
CPLErr CPL_ODLL     CEOSReadScanline( CEOSImage *psImage, int nBand,
                                      int nScanline, void * pData );

/* -------------------------------------------------------------------- */
/*      Internal prototypes.                                            */
/* -------------------------------------------------------------------- */
CEOSRecord CPL_ODLL *CEOSReadRecord( CEOSImage * );
void CPL_ODLL        CEOSDestroyRecord( CEOSRecord * );

CPL_C_END

#endif /* ndef CEOSOPEN_H_INCLUDED */

