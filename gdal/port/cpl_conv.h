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
 ****************************************************************************/

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
char CPL_DLL *CPLStrlwr( char *);

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
/*      Convert number to string.  This function is locale agnostic     */
/*      (ie. it will support "," or "." regardless of current locale)   */
/* -------------------------------------------------------------------- */
double CPL_DLL CPLAtofM(const char *);

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
char       CPL_DLL *CPLGetCurrentDir(void);
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
const char CPL_DLL *CPLCleanTrailingSlash( const char * );

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
int CPL_DLL CPLCopyFile( const char *pszNewPath, const char *pszOldPath );

CPL_C_END

/* -------------------------------------------------------------------- */
/*      C++ object for temporariliy forcing a LC_NUMERIC locale to "C". */
/* -------------------------------------------------------------------- */

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


#endif /* ndef CPL_CONV_H_INCLUDED */
