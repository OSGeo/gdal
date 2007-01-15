/**********************************************************************
 * $Id$
 *
 * Name:     cpl_error.h
 * Project:  CPL - Common Portability Library
 * Purpose:  CPL Error handling
 * Author:   Daniel Morissette, danmo@videotron.ca
 *
 **********************************************************************
 * Copyright (c) 1998, Daniel Morissette
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
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER 
 ****************************************************************************/

#ifndef _CPL_ERROR_H_INCLUDED_
#define _CPL_ERROR_H_INCLUDED_

#include "cpl_port.h"

/*=====================================================================
                   Error handling functions (cpl_error.c)
 =====================================================================*/

/**
 * \file cpl_error.h
 *
 * CPL error handling services.
 */
  
CPL_C_START

typedef enum
{
    CE_None = 0,
    CE_Debug = 1,
    CE_Warning = 2,
    CE_Failure = 3,
    CE_Fatal = 4
} CPLErr;

void CPL_DLL CPLError(CPLErr eErrClass, int err_no, const char *fmt, ...);
void CPL_DLL CPLErrorV(CPLErr, int, const char *, va_list );
void CPL_DLL CPL_STDCALL CPLErrorReset( void );
int CPL_DLL CPL_STDCALL CPLGetLastErrorNo( void );
CPLErr CPL_DLL CPL_STDCALL CPLGetLastErrorType( void );
const char CPL_DLL * CPL_STDCALL CPLGetLastErrorMsg( void );

typedef void (CPL_STDCALL *CPLErrorHandler)(CPLErr, int, const char*);

void CPL_DLL CPL_STDCALL CPLLoggingErrorHandler( CPLErr, int, const char * );
void CPL_DLL CPL_STDCALL CPLDefaultErrorHandler( CPLErr, int, const char * );
void CPL_DLL CPL_STDCALL CPLQuietErrorHandler( CPLErr, int, const char * );

CPLErrorHandler CPL_DLL CPL_STDCALL CPLSetErrorHandler(CPLErrorHandler);
void CPL_DLL CPL_STDCALL CPLPushErrorHandler( CPLErrorHandler );
void CPL_DLL CPL_STDCALL CPLPopErrorHandler(void);

void CPL_DLL CPL_STDCALL CPLDebug( const char *, const char *, ... );
void CPL_DLL CPL_STDCALL _CPLAssert( const char *, const char *, int );

#ifdef DEBUG
#  define CPLAssert(expr)  ((expr) ? (void)(0) : _CPLAssert(#expr,__FILE__,__LINE__))
#else
#  define CPLAssert(expr)
#endif

CPL_C_END

/* ==================================================================== */
/*      Well known error codes.                                         */
/* ==================================================================== */

#define CPLE_None                       0
#define CPLE_AppDefined                 1
#define CPLE_OutOfMemory                2
#define CPLE_FileIO                     3
#define CPLE_OpenFailed                 4
#define CPLE_IllegalArg                 5
#define CPLE_NotSupported               6
#define CPLE_AssertionFailed            7
#define CPLE_NoWriteAccess              8
#define CPLE_UserInterrupt              9

/* 100 - 299 reserved for GDAL */

#endif /* _CPL_ERROR_H_INCLUDED_ */
