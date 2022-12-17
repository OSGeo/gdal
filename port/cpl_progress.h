/******************************************************************************
 * $Id$
 *
 * Project:  CPL - Common Portability Library
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 * Purpose:  Prototypes and definitions for progress functions.
 *
 ******************************************************************************
 * Copyright (c) 2013, Frank Warmerdam
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

#ifndef CPL_PROGRESS_H_INCLUDED
#define CPL_PROGRESS_H_INCLUDED

#include "cpl_port.h"

CPL_C_START

typedef int(CPL_STDCALL *GDALProgressFunc)(double dfComplete,
                                           const char *pszMessage,
                                           void *pProgressArg);

int CPL_DLL CPL_STDCALL GDALDummyProgress(double, const char *, void *);
int CPL_DLL CPL_STDCALL GDALTermProgress(double, const char *, void *);
int CPL_DLL CPL_STDCALL GDALScaledProgress(double, const char *, void *);
void CPL_DLL *CPL_STDCALL GDALCreateScaledProgress(double, double,
                                                   GDALProgressFunc, void *);
void CPL_DLL CPL_STDCALL GDALDestroyScaledProgress(void *);
CPL_C_END

#endif /* ndef CPL_PROGRESS_H_INCLUDED */
