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
 * DEALINGS IN THE SOFTWARE.
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

    CPLErrorHandlerAccumulatorStruct() : type(CE_None), no(CPLE_None) {}
    CPLErrorHandlerAccumulatorStruct(CPLErr eErrIn, CPLErrorNum noIn, const char* msgIn) :
        type(eErrIn), no(noIn), msg(msgIn) {}
};

void CPL_DLL CPLInstallErrorHandlerAccumulator(std::vector<CPLErrorHandlerAccumulatorStruct>& aoErrors);
void CPL_DLL CPLUninstallErrorHandlerAccumulator();

#endif

#endif // CPL_ERROR_INTERNAL_H_INCLUDED
