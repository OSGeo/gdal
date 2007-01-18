/******************************************************************************
 * $Id$
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  Some private helper functions and stuff for OGR implementation.
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

#ifndef _OGR_P_H_INCLUDED
#define _OGR_P_H_INCLUDED

/* -------------------------------------------------------------------- */
/*      Include the common portability library ... lets us do lots      */
/*      of stuff easily.                                                */
/* -------------------------------------------------------------------- */

#include "cpl_string.h"
#include "cpl_conv.h"

#ifdef CPL_MSB 
#  define OGR_SWAP(x)   (x == wkbNDR)
#else
#  define OGR_SWAP(x)   (x == wkbXDR)
#endif

/* -------------------------------------------------------------------- */
/*      helper function for parsing well known text format vector objects.*/
/* -------------------------------------------------------------------- */

#ifdef _OGR_GEOMETRY_H_INCLUDED
#define OGR_WKT_TOKEN_MAX       64

const char CPL_DLL * OGRWktReadToken( const char * pszInput, char * pszToken );

const char CPL_DLL * OGRWktReadPoints( const char * pszInput,
                                       OGRRawPoint **ppaoPoints, 
                                       double **ppadfZ,
                                       int * pnMaxPoints,
                                       int * pnReadPoints );

void CPL_DLL OGRMakeWktCoordinate( char *, double, double, double, int );
#endif


/* General utility option processing. */
int CPL_DLL OGRGeneralCmdLineProcessor( int nArgc, char ***ppapszArgv, int nOptions );

/************************************************************************/
/*     Support for special attributes (feature query and selection)     */
/************************************************************************/
CPL_C_START
#include "swq.h"
CPL_C_END

#define SPF_FID 0
#define SPF_OGR_GEOMETRY 1
#define SPF_OGR_STYLE 2
#define SPF_OGR_GEOM_WKT 3
#define SPECIAL_FIELD_COUNT 4

extern char* SpecialFieldNames[SPECIAL_FIELD_COUNT];
extern swq_field_type SpecialFieldTypes[SPECIAL_FIELD_COUNT];

#endif /* ndef _OGR_P_H_INCLUDED */
