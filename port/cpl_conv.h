/******************************************************************************
 * $Id$
 *
 * Project:  CPL - Common Portability Library
 * Purpose:  Convenience functions declarations.
 *           This is intended to remain light weight.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
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
 * $Log$
 * Revision 1.24  2003/09/08 11:09:53  dron
 * Added CPLPrintDouble() and CPLPrintTime().
 *
 * Revision 1.23  2003/09/07 14:38:43  dron
 * Added CPLPrintString(), CPLPrintStringFill(), CPLPrintInt32(), CPLPrintUIntBig().
 *
 * Revision 1.22  2003/08/31 14:48:05  dron
 * Added CPLScanLong() and CPLScanDouble().
 *
 * Revision 1.21  2003/08/25 20:01:58  dron
 * Added CPLFGets() helper function.
 *
 * Revision 1.20  2003/05/08 21:51:14  warmerda
 * added CPL{G,S}etConfigOption() usage
 *
 * Revision 1.19  2003/03/02 04:44:38  warmerda
 * added CPLStringToComplex
 *
 * Revision 1.18  2002/12/13 06:00:54  warmerda
 * added CPLProjectRelativeFilename() and CPLIsFilenameRelative()
 *
 * Revision 1.17  2002/12/09 18:52:51  warmerda
 * added DMS conversion
 *
 * Revision 1.16  2002/12/03 04:42:02  warmerda
 * improved finder cleanup support
 *
 * Revision 1.15  2002/08/15 09:23:24  dron
 * Added CPLGetDirname() function
 *
 * Revision 1.14  2002/02/01 20:39:50  warmerda
 * ensure CPLReadLine() is exported from DLL
 *
 * Revision 1.13  2001/12/12 17:06:57  warmerda
 * added CPLStat
 *
 * Revision 1.12  2001/03/16 22:15:08  warmerda
 * added CPLResetExtension
 *
 * Revision 1.1  1998/10/18 06:15:11  warmerda
 * Initial implementation.
 *
 */

#ifndef CPL_CONV_H_INCLUDED
#define CPL_CONV_H_INCLUDED

#include "cpl_port.h"
#include "cpl_vsi.h"
#include "cpl_error.h"

/**
 * \file cpl_conv.h
 *
 * Various convenience functions for CPL.
 *
 */

/* -------------------------------------------------------------------- */
/*      Runtime check of various configuration items.                   */
/* -------------------------------------------------------------------- */
CPL_C_START

void CPL_DLL CPLVerifyConfiguration();

const char CPL_DLL *CPLGetConfigOption( const char *, const char * );
void CPL_DLL        CPLSetConfigOption( const char *, const char * );

/* -------------------------------------------------------------------- */
/*      Safe malloc() API.  Thin cover over VSI functions with fatal    */
/*      error reporting if memory allocation fails.                     */
/* -------------------------------------------------------------------- */
void CPL_DLL *CPLMalloc( size_t );
void CPL_DLL *CPLCalloc( size_t, size_t );
void CPL_DLL *CPLRealloc( void *, size_t );
char CPL_DLL *CPLStrdup( const char * );

#define CPLFree VSIFree

/* -------------------------------------------------------------------- */
/*      Read a line from a text file, and strip of CR/LF.               */
/* -------------------------------------------------------------------- */
char CPL_DLL *CPLFGets( char *, int, FILE *);
const char CPL_DLL *CPLReadLine( FILE * );

/* -------------------------------------------------------------------- */
/*      Read a numeric value from an ASCII character string.            */
/* -------------------------------------------------------------------- */
double CPL_DLL CPLScanDouble( char *, int );
long CPL_DLL CPLScanLong( char *, int );

/* -------------------------------------------------------------------- */
/*      Print a value to an ASCII character string.                     */
/* -------------------------------------------------------------------- */
char CPL_DLL *CPLPrintString( char *, const char *, int );
char CPL_DLL *CPLPrintStringFill( char *, const char *, int );
char CPL_DLL *CPLPrintInt32( char *, GInt32 , int );
char CPL_DLL *CPLPrintUIntBig( char *, GUIntBig , int );
char CPL_DLL *CPLPrintDouble( char *, const char *, double );
char CPL_DLL *CPLPrintTime( char *, int , const char *,
                            const struct tm *, char * );

/* -------------------------------------------------------------------- */
/*      Fetch a function from DLL / so.                                 */
/* -------------------------------------------------------------------- */

void CPL_DLL *CPLGetSymbol( const char *, const char * );

/* -------------------------------------------------------------------- */
/*      Read a directory  (cpl_dir.c)                                   */
/* -------------------------------------------------------------------- */
char CPL_DLL  **CPLReadDir( const char *pszPath );

/* -------------------------------------------------------------------- */
/*      Filename handling functions.                                    */
/* -------------------------------------------------------------------- */
const char CPL_DLL *CPLGetPath( const char * );
const char CPL_DLL *CPLGetDirname( const char * );
const char CPL_DLL *CPLGetFilename( const char * );
const char CPL_DLL *CPLGetBasename( const char * );
const char CPL_DLL *CPLGetExtension( const char * );
const char CPL_DLL *CPLFormFilename( const char *pszPath,
                                     const char *pszBasename,
                                     const char *pszExtension );
const char CPL_DLL *CPLFormCIFilename( const char *pszPath,
                                       const char *pszBasename,
                                       const char *pszExtension );
const char CPL_DLL *CPLResetExtension( const char *, const char * );
const char CPL_DLL *CPLProjectRelativeFilename( const char *pszProjectDir, 
                                            const char *pszSecondaryFilename );
int CPL_DLL CPLIsFilenameRelative( const char *pszFilename );

/* -------------------------------------------------------------------- */
/*      Find File Function                                              */
/* -------------------------------------------------------------------- */
typedef const char *(*CPLFileFinder)(const char *, const char *);

const char    CPL_DLL *CPLFindFile(const char *pszClass, 
                                   const char *pszBasename);
const char    CPL_DLL *CPLDefaultFindFile(const char *pszClass, 
                                          const char *pszBasename);
void          CPL_DLL CPLPushFileFinder( CPLFileFinder pfnFinder );
CPLFileFinder CPL_DLL CPLPopFileFinder();
void          CPL_DLL CPLPushFinderLocation( const char * );
void          CPL_DLL CPLPopFinderLocation();
void          CPL_DLL CPLFinderClean();

/* -------------------------------------------------------------------- */
/*      Safe version of stat() that works properly on stuff like "C:".  */
/* -------------------------------------------------------------------- */
int CPL_DLL     CPLStat( const char *, VSIStatBuf * );

/* -------------------------------------------------------------------- */
/*      DMS to Dec to DMS conversion.                                   */
/* -------------------------------------------------------------------- */
double CPL_DLL CPLDMSToDec( const char *is );
const char CPL_DLL *CPLDecToDMS( double dfAngle, const char * pszAxis,
                                 int nPrecision );

void CPL_DLL CPLStringToComplex( const char *pszString, 
                                 double *pdfReal, double *pdfImag );

CPL_C_END

#endif /* ndef CPL_CONV_H_INCLUDED */
