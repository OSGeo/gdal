/******************************************************************************
 *
 * Project:  CPL - Common Portability Library
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 * Purpose:  Prototypes and definitions for progress functions.
 *
 ******************************************************************************
 * Copyright (c) 2013, Frank Warmerdam
 *
 * SPDX-License-Identifier: MIT
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

#if defined(__cplusplus) && defined(GDAL_COMPILATION)
extern "C++"
{
    /*! @cond Doxygen_Suppress */
    struct CPL_DLL GDALScaledProgressReleaser
    {
        void operator()(void *p) const
        {
            GDALDestroyScaledProgress(p);
        }
    };

    /*! @endcond */
}
#endif

#endif /* ndef CPL_PROGRESS_H_INCLUDED */
