/******************************************************************************
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
 * cpl_serv.h
 *
 * This include file derived and simplified from the GDAL Common Portability
 * Library.
 */

#ifndef CPL_SERV_H_INCLUDED
#define CPL_SERV_H_INCLUDED

/* ==================================================================== */
/*	Standard include files.						*/
/* ==================================================================== */

#include "geo_config.h"
#include <stdio.h>

#include <math.h>

#ifdef HAVE_STRING_H
#  include <string.h>
#endif
#if defined(HAVE_STRINGS_H) && !defined(HAVE_STRING_H)
#  include <strings.h>
#endif
#ifdef HAVE_STDLIB_H
#  include <stdlib.h>
#endif

/**********************************************************************
 * Do we want to build as a DLL on windows?
 **********************************************************************/
#if !defined(CPL_DLL)
#  if defined(_WIN32) && defined(BUILD_AS_DLL)
#    define CPL_DLL     __declspec(dllexport)
#  else
#    define CPL_DLL
#  endif
#endif

/* ==================================================================== */
/*      Other standard services.                                        */
/* ==================================================================== */
#ifdef __cplusplus
#  define CPL_C_START		extern "C" {
#  define CPL_C_END		}
#else
#  define CPL_C_START
#  define CPL_C_END
#endif

#ifndef NULL
#  define NULL	0
#endif

#ifndef FALSE
#  define FALSE	0
#endif

#ifndef TRUE
#  define TRUE	1
#endif

#ifndef MAX
#  define MIN(a,b)      ((a<b) ? a : b)
#  define MAX(a,b)      ((a>b) ? a : b)
#endif

#ifndef NULL
#define NULL 0
#endif

#ifndef ABS
#  define ABS(x)        ((x<0) ? (-1*(x)) : x)
#endif

#ifndef EQUAL
#if defined(_WIN32) && !defined(__CYGWIN__)
#  define EQUALN(a,b,n)           (strnicmp(a,b,n)==0)
#  define EQUAL(a,b)              (stricmp(a,b)==0)
#else
#  define EQUALN(a,b,n)           (strncasecmp(a,b,n)==0)
#  define EQUAL(a,b)              (strcasecmp(a,b)==0)
#endif
#endif

/* ==================================================================== */
/*      VSI Services (just map directly onto Standard C services.       */
/* ==================================================================== */

#define VSIFOpen	fopen
#define VSIFClose	fclose
#define VSIFEof		feof
#define VSIFPrintf	fprintf
#define VSIFPuts	fputs
#define VSIFPutc	fputc
#define VSIFGets	fgets
#define VSIRewind	rewind

#define VSICalloc(x,y)	_GTIFcalloc(x*y)
#define VSIMalloc	_GTIFcalloc
#define VSIFree	        _GTIFFree
#define VSIRealloc      _GTIFrealloc

/* -------------------------------------------------------------------- */
/*      Safe malloc() API.  Thin cover over VSI functions with fatal    */
/*      error reporting if memory allocation fails.                     */
/* -------------------------------------------------------------------- */
CPL_C_START
void  *CPLMalloc( int );
void  *CPLCalloc( int, int );
void  *CPLRealloc( void *, int );
char  *CPLStrdup( const char * );

#define CPLFree	VSIFree

/* -------------------------------------------------------------------- */
/*      Read a line from a text file, and strip of CR/LF.               */
/* -------------------------------------------------------------------- */
const char *CPLReadLine( FILE * );

/*=====================================================================
                   Error handling functions (cpl_error.c)
 =====================================================================*/

typedef enum
{
    CE_None = 0,
    CE_Log = 1,
    CE_Warning = 2,
    CE_Failure = 3,
    CE_Fatal = 4
  
} CPLErr;

void  CPLError(CPLErr eErrClass, int err_no, const char *fmt, ...);
void  CPLErrorReset();
int  CPLGetLastErrorNo();
const char  * CPLGetLastErrorMsg();
void  CPLSetErrorHandler(void(*pfnErrorHandler)(CPLErr,int,
                                                       const char *));
void  _CPLAssert( const char *, const char *, int );

#ifdef DEBUG
#  define CPLAssert(expr)  ((expr) ? (void)(0) : _CPLAssert(#expr,__FILE__,__LINE__))
#else
#  define CPLAssert(expr)
#endif

CPL_C_END

/* ==================================================================== */
/*      Well known error codes.                                         */
/* ==================================================================== */

#define CPLE_AppDefined			1
#define CPLE_OutOfMemory		2
#define CPLE_FileIO			3
#define CPLE_OpenFailed			4
#define CPLE_IllegalArg			5
#define CPLE_NotSupported		6
#define CPLE_AssertionFailed		7
#define CPLE_NoWriteAccess		8

/*=====================================================================
                   Stringlist functions (strlist.c)
 =====================================================================*/
CPL_C_START

char    **CSLAddString(char **papszStrList, const char *pszNewString);
int     CSLCount(char **papszStrList);
const char *CSLGetField( char **, int );
void    CSLDestroy(char **papszStrList);
char    **CSLDuplicate(char **papszStrList);

char    **CSLTokenizeString(const char *pszString );
char    **CSLTokenizeStringComplex(const char *pszString,
                                   const char *pszDelimiter,
                                   int bHonourStrings, int bAllowEmptyTokens );

int     CSLPrint(char **papszStrList, FILE *fpOut);
char    **CSLLoad(const char *pszFname);
int     CSLSave(char **papszStrList, const char *pszFname);

char  **CSLInsertStrings(char **papszStrList, int nInsertAtLineNo, 
                         char **papszNewLines);
char  **CSLInsertString(char **papszStrList, int nInsertAtLineNo, 
                        char *pszNewLine);
char  **CSLRemoveStrings(char **papszStrList, int nFirstLineToDelete,
                         int nNumToRemove, char ***ppapszRetStrings);

const char *CPLSPrintf(char *fmt, ...);
char  **CSLAppendPrintf(char **papszStrList, char *fmt, ...);

const char *CSLFetchNameValue(char **papszStrList, const char *pszName);
char  **CSLFetchNameValueMultiple(char **papszStrList, const char *pszName);
char  **CSLAddNameValue(char **papszStrList, 
                        const char *pszName, const char *pszValue);
char  **CSLSetNameValue(char **papszStrList, 
                        const char *pszName, const char *pszValue);

CPL_C_END

#endif /* ndef CPL_SERV_H_INCLUDED */
