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
 ******************************************************************************
 *
 * $Log$
 * Revision 1.10  2006/03/31 17:44:20  fwarmerdam
 * header updates
 *
 * Revision 1.9  2005/10/16 01:59:06  cfis
 * Added declaration for OGRGeneralCmdLineProcessor to ogr_p.h, and included it into ogr2ogr.  Also changed call to CPL_DLL from CPL_STDCALL
 *
 * Revision 1.8  2005/07/20 01:43:51  fwarmerdam
 * upgraded OGR geometry dimension handling
 *
 * Revision 1.7  2001/11/01 17:01:28  warmerda
 * pass output buffer into OGRMakeWktCoordinate
 *
 * Revision 1.6  1999/11/18 19:02:20  warmerda
 * expanded tabs
 *
 * Revision 1.5  1999/09/13 02:27:33  warmerda
 * incorporated limited 2.5d support
 *
 * Revision 1.4  1999/07/29 17:30:38  warmerda
 * avoid geometry dependent stuff if ogr_geometry.h not included
 *
 * Revision 1.3  1999/07/07 04:23:07  danmo
 * Fixed typo in  #define _OGR_..._H_INCLUDED  line
 *
 * Revision 1.2  1999/05/20 14:36:04  warmerda
 * added well known text parsing prototypes
 *
 * Revision 1.1  1999/03/29 21:21:10  warmerda
 * New
 *
 */

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


#endif /* ndef _OGR_P_H_INCLUDED */
