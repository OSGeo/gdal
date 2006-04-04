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
 * Revision 1.43  2006/04/04 15:37:19  fwarmerdam
 * Removed long double strtod functions.
 *
 * Revision 1.42  2006/03/18 16:36:15  dron
 * Added ASCII to floating point number conversion functions.
 *
 * Revision 1.41  2006/03/04 00:46:43  mloskot
 * [WCE] CPLLocaleC class excluded from compilation for Windows CE
 *
 * Revision 1.40  2006/03/03 19:40:53  fwarmerdam
 * added CPLLocaleC class
 *
 * Revision 1.39  2005/09/11 21:08:21  fwarmerdam
 * added CPLReadLineL
 *
 * Revision 1.38  2005/08/25 18:06:48  fwarmerdam
 * void in empty arg lists.
 *
 * Revision 1.37  2005/06/10 15:00:00  fwarmerdam
 * added cpl_getexecpath.cpp
 *
 * Revision 1.36  2005/04/04 15:23:31  fwarmerdam
 * some functions now CPL_STDCALL
 *
 * Revision 1.35  2004/11/17 22:57:21  fwarmerdam
 * added CPLScanPointer() and CPLPrintPointer()
 *
 * Revision 1.34  2004/08/16 20:24:07  warmerda
 * added CPLUnlinkTree
 *
 * Revision 1.33  2004/08/11 18:41:46  warmerda
 * added CPLExtractRelativePath
 *
 * Revision 1.32  2004/07/31 04:51:36  warmerda
 * added shared file open support
 *
 * Revision 1.31  2004/03/28 16:22:02  warmerda
 * const correctness changes in scan functions
 *
 * Revision 1.30  2004/03/24 09:01:17  dron
 * Added CPLPrintUIntBig().
 *
 * Revision 1.29  2004/02/07 14:03:30  dron
 * CPLDecToPackedDMS() added.
 *
 * Revision 1.28  2004/02/01 08:37:55  dron
 * Added CPLPackedDMSToDec().
 *
 * Revision 1.27  2003/12/28 17:24:43  warmerda
 * added CPLFreeConfig
 *
 * Revision 1.26  2003/10/17 07:06:06  dron
 * Added locale selection option to CPLScanDouble() and CPLPrintDOuble().
 *
 * Revision 1.25  2003/09/28 14:14:16  dron
 * Added CPLScanString().
 *
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

void CPL_DLL CPLVerifyConfiguration(void);

const char CPL_DLL * CPL_STDCALL
CPLGetConfigOption( const char *, const char * );
void CPL_DLL CPL_STDCALL CPLSetConfigOption( const char *, const char * );
void CPL_DLL CPL_STDCALL CPLFreeConfig(void);

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
const char CPL_DLL *CPLReadLineL( FILE * );

/* -------------------------------------------------------------------- */
/*      Convert ASCII string to floationg point number                  */
/*      (THESE FUNCTIONS ARE NOT LOCALE AWARE!).                        */
/* -------------------------------------------------------------------- */
double CPL_DLL CPLAtof(const char *);
double CPL_DLL CPLAtofDelim(const char *, char);
double CPL_DLL CPLStrtod(const char *, char **);
double CPL_DLL CPLStrtodDelim(const char *, char **, char);
float CPL_DLL CPLStrtof(const char *, char **);
float CPL_DLL CPLStrtofDelim(const char *, char **, char);

/* -------------------------------------------------------------------- */
/*      Read a numeric value from an ASCII character string.            */
/* -------------------------------------------------------------------- */
char CPL_DLL *CPLScanString( const char *, int, int, int );
double CPL_DLL CPLScanDouble( const char *, int, char * );
long CPL_DLL CPLScanLong( const char *, int );
GUIntBig CPL_DLL CPLScanUIntBig( const char *, int );
void CPL_DLL *CPLScanPointer( const char *, int );

/* -------------------------------------------------------------------- */
/*      Print a value to an ASCII character string.                     */
/* -------------------------------------------------------------------- */
int CPL_DLL CPLPrintString( char *, const char *, int );
int CPL_DLL CPLPrintStringFill( char *, const char *, int );
int CPL_DLL CPLPrintInt32( char *, GInt32 , int );
int CPL_DLL CPLPrintUIntBig( char *, GUIntBig , int );
int CPL_DLL CPLPrintDouble( char *, const char *, double, char * );
int CPL_DLL CPLPrintTime( char *, int , const char *, const struct tm *,
                          char * );
int CPL_DLL CPLPrintPointer( char *, void *, int );

/* -------------------------------------------------------------------- */
/*      Fetch a function from DLL / so.                                 */
/* -------------------------------------------------------------------- */

void CPL_DLL *CPLGetSymbol( const char *, const char * );

/* -------------------------------------------------------------------- */
/*      Read a directory  (cpl_dir.c)                                   */
/* -------------------------------------------------------------------- */
char CPL_DLL  **CPLReadDir( const char *pszPath );

/* -------------------------------------------------------------------- */
/*      Fetch executable path.                                          */
/* -------------------------------------------------------------------- */
int CPL_DLL CPLGetExecPath( char *pszPathBuf, int nMaxLength );

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
const char CPL_DLL *CPLExtractRelativePath(const char *, const char *, int *);

/* -------------------------------------------------------------------- */
/*      Find File Function                                              */
/* -------------------------------------------------------------------- */
typedef const char *(*CPLFileFinder)(const char *, const char *);

const char    CPL_DLL *CPLFindFile(const char *pszClass, 
                                   const char *pszBasename);
const char    CPL_DLL *CPLDefaultFindFile(const char *pszClass, 
                                          const char *pszBasename);
void          CPL_DLL CPLPushFileFinder( CPLFileFinder pfnFinder );
CPLFileFinder CPL_DLL CPLPopFileFinder(void);
void          CPL_DLL CPLPushFinderLocation( const char * );
void          CPL_DLL CPLPopFinderLocation(void);
void          CPL_DLL CPLFinderClean(void);

/* -------------------------------------------------------------------- */
/*      Safe version of stat() that works properly on stuff like "C:".  */
/* -------------------------------------------------------------------- */
int CPL_DLL     CPLStat( const char *, VSIStatBuf * );

/* -------------------------------------------------------------------- */
/*      Reference counted file handle manager.  Makes sharing file      */
/*      handles more practical.                                         */
/* -------------------------------------------------------------------- */
typedef struct {
    FILE *fp;
    int   nRefCount;
    int   bLarge;
    char  *pszFilename;
    char  *pszAccess;
} CPLSharedFileInfo;

FILE CPL_DLL    *CPLOpenShared( const char *, const char *, int );
void CPL_DLL     CPLCloseShared( FILE * );
CPLSharedFileInfo CPL_DLL *CPLGetSharedList( int * );
void CPL_DLL     CPLDumpSharedList( FILE * );

/* -------------------------------------------------------------------- */
/*      DMS to Dec to DMS conversion.                                   */
/* -------------------------------------------------------------------- */
double CPL_DLL CPLDMSToDec( const char *is );
const char CPL_DLL *CPLDecToDMS( double dfAngle, const char * pszAxis,
                                 int nPrecision );
double CPL_DLL CPLPackedDMSToDec( double );
double CPL_DLL CPLDecToPackedDMS( double dfDec );

void CPL_DLL CPLStringToComplex( const char *pszString, 
                                 double *pdfReal, double *pdfImag );

/* -------------------------------------------------------------------- */
/*      Misc other functions.                                           */
/* -------------------------------------------------------------------- */
int CPL_DLL CPLUnlinkTree( const char * );
CPL_C_END

/* -------------------------------------------------------------------- */
/*      C++ object for temporariliy forcing a LC_NUMERIC locale to "C". */
/* -------------------------------------------------------------------- */
#ifndef WIN32CE

#ifdef __cplusplus

class CPLLocaleC
{
  private:
    char *pszOldLocale;

  public:
    CPLLocaleC();
    ~CPLLocaleC();
};

#endif /* def __cplusplus */

#endif /* ndef WIN32CE */

#endif /* ndef CPL_CONV_H_INCLUDED */
