/******************************************************************************
 * $Id$
 *
 * Name:     hfa.h
 * Project:  Erdas Imagine (.img) Translator
 * Purpose:  Public (C callable) interface for the Erdas Imagine reading
 *           code.  This include files, and it's implementing code depends
 *           on CPL, but not GDAL. 
 * Author:   Frank Warmerdam, warmerda@home.com
 *
 ******************************************************************************
 * Copyright (c) 1999, Intergraph Corporation
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
 * Revision 1.2  1999/01/04 22:52:47  warmerda
 * field access working
 *
 * Revision 1.1  1999/01/04 05:28:13  warmerda
 * New
 *
 */

#ifndef _HFAOPEN_H_INCLUDED
#define _HFAOPEN_H_INCLUDED

/* -------------------------------------------------------------------- */
/*      Include standard portability stuff.                             */
/* -------------------------------------------------------------------- */
#include "cpl_conv.h"
#include "cpl_string.h"

#ifdef HFA_PRIVATE
typedef HFAInfo_t *HFAHandle;
#else
typedef void *HFAHandle;
#endif

CPL_C_START

HFAHandle HFAOpen( const char * pszFilename, const char * pszMode );
void	HFAClose( HFAHandle );

void	HFAGetRasterInfo( HFAHandle hHFA, int *pnXSize, int *pnYSize,
                          int *pnBands, int * pnDataType );

#define EPT_u1	0
#define EPT_u2	1
#define EPT_u4	2
#define EPT_u8	3
#define EPT_s8	4
#define EPT_u16	5
#define EPT_s16	6
#define EPT_u32	7
#define EPT_s32	8
#define EPT_f32	9
#define EPT_f64	10
#define EPT_c64	11
#define EPT_c128 12

CPLErr HFAReadBlock( HFAHandle, int nBand, int nXTileOff, int nYTileOff,
                     int nReqDataType, void * pImageData );

GInt32	HFAGetIntField( const char *, CPLErr * peErr );
double	HFAGetDoubleField( const char *, CPLErr * peErr );
const char *HFAGetStringField( const char *, CPLErr * peErr );
CPL_C_END

#endif /* ndef _HFAOPEN_H_INCLUDED */
