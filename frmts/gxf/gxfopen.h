/******************************************************************************
 * Copyright (c) 1998, Global Geomatics
 * Copyright (c) 1998, Frank Warmerdam
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
 ******************************************************************************
 *
 * gxfopen.h: Includes for underlying GXF reading code.
 *
 * $Log$
 * Revision 1.3  1998/12/14 04:51:30  warmerda
 * Added some functions, clarified raw vs. not raw.
 *
 * Revision 1.2  1998/12/06 02:54:26  warmerda
 * new functions
 *
 * Revision 1.1  1998/12/02 19:37:04  warmerda
 * New
 *
 */

#ifndef _GXFOPEN_H_INCLUDED
#define _GXFOPEN_H_INCLUDED

/* -------------------------------------------------------------------- */
/*      Include standard portability stuff.                             */
/* -------------------------------------------------------------------- */
#include "cpl_conv.h"
#include "cpl_string.h"

typedef void *GXFHandle;

CPL_C_START

GXFHandle GXFOpen( const char * pszFilename );

CPLErr   GXFGetRawInfo( GXFHandle hGXF, int *pnXSize, int *pnYSize,
                        int *pnSense );
CPLErr   GXFGetInfo( GXFHandle hGXF, int *pnXSize, int *pnYSize );

CPLErr   GXFGetRawScanline( GXFHandle, int iScanline, double * padfLineBuf );

char	**GXFGetMapProjection( GXFHandle );
char	**GXFGetMapDatumTransform( GXFHandle );
char	*GXFGetMapProjectionAsPROJ4( GXFHandle );

CPLErr  GXFGetRawPosition( GXFHandle, double *, double *, double *, double *,
                           double * );

void     GXFClose( GXFHandle hGXF );

#define GXFS_LL_UP	-1
#define GXFS_LL_RIGHT	1
#define GXFS_UL_RIGHT	-2
#define GXFS_UL_DOWN	2
#define GXFS_UR_DOWN	-3
#define GXFS_UR_LEFT	3
#define GXFS_LR_LEFT	-4
#define GXFS_LR_UP	4

CPL_C_END

#endif /* ndef _GXFOPEN_H_INCLUDED */
