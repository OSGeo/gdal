/******************************************************************************
 * $Id$
 *
 * Project:  CPL - Common Portability Library
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 * Purpose:  Progress function implementations.
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

#include "cpl_progress.h"
#include "cpl_conv.h"

CPL_CVSID("$Id: gdal_misc.cpp 25494 2013-01-13 12:55:17Z etourigny $");

/************************************************************************/
/*                         GDALDummyProgress()                          */
/************************************************************************/

/**
 * \brief Stub progress function.
 *
 * This is a stub (does nothing) implementation of the GDALProgressFunc()
 * semantics.  It is primarily useful for passing to functions that take
 * a GDALProgressFunc() argument but for which the application does not want
 * to use one of the other progress functions that actually do something.
 */

int CPL_STDCALL GDALDummyProgress( double dfComplete, const char *pszMessage,
                                   void *pData )

{
    return TRUE;
}

/************************************************************************/
/*                         GDALScaledProgress()                         */
/************************************************************************/
typedef struct {
    GDALProgressFunc pfnProgress;
    void *pData;
    double dfMin;
    double dfMax;
} GDALScaledProgressInfo;

/**
 * \brief Scaled progress transformer.
 *
 * This is the progress function that should be passed along with the
 * callback data returned by GDALCreateScaledProgress().
 */

int CPL_STDCALL GDALScaledProgress( double dfComplete, const char *pszMessage,
                                    void *pData )

{
    GDALScaledProgressInfo *psInfo = (GDALScaledProgressInfo *) pData;

    return psInfo->pfnProgress( dfComplete * (psInfo->dfMax - psInfo->dfMin)
                                + psInfo->dfMin,
                                pszMessage, psInfo->pData );
}

/************************************************************************/
/*                      GDALCreateScaledProgress()                      */
/************************************************************************/

/**
 * \brief Create scaled progress transformer.
 *
 * Sometimes when an operations wants to report progress it actually
 * invokes several subprocesses which also take GDALProgressFunc()s,
 * and it is desirable to map the progress of each sub operation into
 * a portion of 0.0 to 1.0 progress of the overall process.  The scaled
 * progress function can be used for this.
 *
 * For each subsection a scaled progress function is created and
 * instead of passing the overall progress func down to the sub functions,
 * the GDALScaledProgress() function is passed instead.
 *
 * @param dfMin the value to which 0.0 in the sub operation is mapped.
 * @param dfMax the value to which 1.0 is the sub operation is mapped.
 * @param pfnProgress the overall progress function.
 * @param pData the overall progress function callback data.
 *
 * @return pointer to pass as pProgressArg to sub functions.  Should be freed
 * with GDALDestroyScaledProgress().
 *
 * Example:
 *
 * \code
 *   int MyOperation( ..., GDALProgressFunc pfnProgress, void *pProgressData );
 *
 *   {
 *       void *pScaledProgress;
 *
 *       pScaledProgress = GDALCreateScaledProgress( 0.0, 0.5, pfnProgress,
 *                                                   pProgressData );
 *       GDALDoLongSlowOperation( ..., GDALScaledProgress, pScaledProgress );
 *       GDALDestroyScaledProgress( pScaledProgress );
 *
 *       pScaledProgress = GDALCreateScaledProgress( 0.5, 1.0, pfnProgress,
 *                                                   pProgressData );
 *       GDALDoAnotherOperation( ..., GDALScaledProgress, pScaledProgress );
 *       GDALDestroyScaledProgress( pScaledProgress );
 *
 *       return ...;
 *   }
 * \endcode
 */

void * CPL_STDCALL GDALCreateScaledProgress( double dfMin, double dfMax,
                                GDALProgressFunc pfnProgress,
                                void * pData )

{
    GDALScaledProgressInfo *psInfo;

    psInfo = (GDALScaledProgressInfo *)
        CPLCalloc(sizeof(GDALScaledProgressInfo),1);

    if( ABS(dfMin-dfMax) < 0.0000001 )
        dfMax = dfMin + 0.01;

    psInfo->pData = pData;
    psInfo->pfnProgress = pfnProgress;
    psInfo->dfMin = dfMin;
    psInfo->dfMax = dfMax;

    return (void *) psInfo;
}

/************************************************************************/
/*                     GDALDestroyScaledProgress()                      */
/************************************************************************/

/**
 * \brief Cleanup scaled progress handle.
 *
 * This function cleans up the data associated with a scaled progress function
 * as returned by GADLCreateScaledProgress().
 *
 * @param pData scaled progress handle returned by GDALCreateScaledProgress().
 */

void CPL_STDCALL GDALDestroyScaledProgress( void * pData )

{
    CPLFree( pData );
}

/************************************************************************/
/*                          GDALTermProgress()                          */
/************************************************************************/

/**
 * \brief Simple progress report to terminal.
 *
 * This progress reporter prints simple progress report to the
 * terminal window.  The progress report generally looks something like
 * this:

\verbatim
0...10...20...30...40...50...60...70...80...90...100 - done.
\endverbatim

 * Every 2.5% of progress another number or period is emitted.  Note that
 * GDALTermProgress() uses internal static data to keep track of the last
 * percentage reported and will get confused if two terminal based progress
 * reportings are active at the same time.
 *
 * The GDALTermProgress() function maintains an internal memory of the
 * last percentage complete reported in a static variable, and this makes
 * it unsuitable to have multiple GDALTermProgress()'s active eithin a
 * single thread or across multiple threads.
 *
 * @param dfComplete completion ratio from 0.0 to 1.0.
 * @param pszMessage optional message.
 * @param pProgressArg ignored callback data argument.
 *
 * @return Always returns TRUE indicating the process should continue.
 */

int CPL_STDCALL GDALTermProgress( double dfComplete, const char *pszMessage,
                      void * pProgressArg )

{
    static int nLastTick = -1;
    int nThisTick = (int) (dfComplete * 40.0);

    (void) pProgressArg;

    nThisTick = MIN(40,MAX(0,nThisTick));

    // Have we started a new progress run?
    if( nThisTick < nLastTick && nLastTick >= 39 )
        nLastTick = -1;

    if( nThisTick <= nLastTick )
        return TRUE;

    while( nThisTick > nLastTick )
    {
        nLastTick++;
        if( nLastTick % 4 == 0 )
            fprintf( stdout, "%d", (nLastTick / 4) * 10 );
        else
            fprintf( stdout, "." );
    }

    if( nThisTick == 40 )
        fprintf( stdout, " - done.\n" );
    else
        fflush( stdout );

    return TRUE;
}
