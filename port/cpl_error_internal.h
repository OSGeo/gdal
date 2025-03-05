/**********************************************************************
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

#include <mutex>
#include <vector>

/************************************************************************/
/*                CPLErrorHandlerAccumulatorStruct                      */
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

/************************************************************************/
/*                       CPLErrorAccumulator                            */
/************************************************************************/

/** Class typically used by a worker thread to store errors emitted by their
 * worker functions, and replay them in the main thread.
 *
 * An instance of CPLErrorAccumulator can be shared by several
 * threads. Each thread calls InstallForCurrentScope() in its processing
 * function. The main thread may invoke ReplayErrors() to replay errors (and
 * warnings).
 *
 * @since 3.11
 */
class CPL_DLL CPLErrorAccumulator
{
  public:
    /** Constructor */
    CPLErrorAccumulator() = default;

    struct CPL_DLL Context
    {
        ~Context();
    };

    /** Install a temporary error handler that will store errors and warnings.
     */
    Context InstallForCurrentScope() CPL_WARN_UNUSED_RESULT;

    /** Return error list. */
    const std::vector<CPLErrorHandlerAccumulatorStruct> &GetErrors() const
    {
        return errors;
    }

    /** Replay stored errors. */
    void ReplayErrors();

  private:
    std::mutex mutex{};
    std::vector<CPLErrorHandlerAccumulatorStruct> errors{};

    static void CPL_STDCALL Accumulator(CPLErr eErr, CPLErrorNum no,
                                        const char *msg);
};

#endif

#endif  // CPL_ERROR_INTERNAL_H_INCLUDED
