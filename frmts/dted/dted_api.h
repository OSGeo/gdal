/******************************************************************************
 * $Id$
 *
 * Project:  DTED Translator
 * Purpose:  Public (C callable) interface for DTED/CDED reading.
 * Author:   Frank Warmerdam, warmerda@home.com
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
 ******************************************************************************
 *
 * $Log$
 * Revision 1.1  1999/12/07 18:01:28  warmerda
 * New
 *
 */

#ifndef _DTED_API_H_INCLUDED
#define _DTED_API_H_INCLUDED

/* -------------------------------------------------------------------- */
/*      To avoid dependence on CPL, just define AVOID_CPL when          */
/*      compiling.                                                      */
/* -------------------------------------------------------------------- */
#ifndef AVOID_CPL
#  include "cpl_conv.h"
#else

#undef CPL_C_START
#undef CPL_C_END

#define VSIFTell ftell
#define CPLMalloc malloc
#define CPLCalloc calloc
#define CPLFree free
#define GInt16	short
#define GByte   unsigned char
#define VSIFSeek fseek

#endif

/* -------------------------------------------------------------------- */
/*      DTED information structure.  All of this can be considered      */
/*      public information.                                             */
/* -------------------------------------------------------------------- */
CPL_C_START

#define DTED_UHL_SIZE 80
#define DTED_DSI_SIZE 648
#define DTED_ACC_SIZE 2700

typedef struct {
  FILE		*fp;

  int		nXSize;
  int		nYSize;

  double	dfULCornerX;		/* in long/lat degrees */
  double        dfULCornerY;
  double	dfPixelSizeX;
  double        dfPixelSizeY;

  char		*pachUHLRecord;
  char          *pachDSIRecord;
  char          *pachACCRecord;

  int		nDataOffset;
  
} DTEDInfo;

/* -------------------------------------------------------------------- */
/*      DTED access API.  Get info directly from the structure          */
/*      (DTEDInfo).                                                     */
/* -------------------------------------------------------------------- */
DTEDInfo *DTEDOpen( const char * pszFilename, const char * pszAccess,
                    int bTestOpen );

int DTEDReadProfile( DTEDInfo * psDInfo, int nColumnOffset,
                     GInt16 * panData );

void DTEDClose( DTEDInfo * );

CPL_C_END

#endif /* ndef _DTED_API_H_INCLUDED */


