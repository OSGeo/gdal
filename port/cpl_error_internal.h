/**********************************************************************
 * $Id$
 *
 * Name:     cpl_error_internal.h
 * Project:  CPL - Common Portability Library
 * Purpose:  CPL Error handling
 * Author:   Even Rouault, <even.rouault at spatialys.com>
 *
 **********************************************************************
 * Copyright (c) 2019, Even Rouault, <even.rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#ifndef CPL_ERROR_INTERNAL_H_INCLUDED
#define CPL_ERROR_INTERNAL_H_INCLUDED

#ifdef GDAL_COMPILATION
// internal only

#include "cpl_error.h"
#include "cpl_string.h"
#include <vector>

/************************************************************************/
/*                      ErrorHandlerAccumulator()                       */
/************************************************************************/

class CPL_DLL CPLErrorHandlerAccumulatorStruct
{
  public:
    CPLErr type;
    CPLErrorNum no;
    CPLString msg{};

    CPLErrorHandlerAccumulatorStruct() : type(CE_None), no(CPLE_None)
    {
    }

    CPLErrorHandlerAccumulatorStruct(CPLErr eErrIn, CPLErrorNum noIn,
                                     const char *msgIn)
        : type(eErrIn), no(noIn), msg(msgIn)
    {
    }
};

void CPL_DLL CPLInstallErrorHandlerAccumulator(
    std::vector<CPLErrorHandlerAccumulatorStruct> &aoErrors);
void CPL_DLL CPLUninstallErrorHandlerAccumulator();

#endif

#endif  // CPL_ERROR_INTERNAL_H_INCLUDED
