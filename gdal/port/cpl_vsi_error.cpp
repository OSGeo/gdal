/******************************************************************************
 * $Id$
 *
 * Project:  VSI Virtual File System
 * Purpose:  Implement an error system for reporting file system errors.
 *           Filesystem errors need to be handled separately from the
 *           CPLError architecture because they are potentially ignored.
 * Author:   Rob Emanuele, rdemanuele at gmail.com
 *
 ******************************************************************************
 * Copyright (c) 2016, Rob Emanuele <rdemanuele at gmail.com>
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

#include "cpl_vsi_error.h"
#include "cpl_error.h"
#include "cpl_string.h"
#include "cpl_vsi.h"
#include "cpl_conv.h"
#include "cpl_multiproc.h"

#if !defined(WIN32)
#include <string.h>
#endif


#define TIMESTAMP_DEBUG
//#define MEMORY_DEBUG

CPL_CVSID("$Id$");

static const int DEFAULT_LAST_ERR_MSG_SIZE =
#if !defined(HAVE_VSNPRINTF)
  20000
#else
  500
#endif
  ;

typedef struct {
    VSIErrorNum nLastErrNo;
    int     nLastErrMsgMax;
    char    szLastErrMsg[DEFAULT_LAST_ERR_MSG_SIZE];
    /* Do not add anything here. szLastErrMsg must be the last field. See CPLRealloc() below */
} VSIErrorContext;

static void VSIErrorV(VSIErrorNum, const char *, va_list );

/************************************************************************/
/*                         CPLGetErrorContext()                         */
/************************************************************************/

static VSIErrorContext *VSIGetErrorContext()

{
    int bError;
    VSIErrorContext *psCtx =
        reinterpret_cast<VSIErrorContext *>(
            CPLGetTLSEx( CTLS_VSIERRORCONTEXT, &bError ) );
    if( bError )
        return NULL;

    if( psCtx == NULL )
    {
      psCtx = reinterpret_cast<VSIErrorContext *>(
          VSICalloc( sizeof(VSIErrorContext), 1) );
        if (psCtx == NULL) {
            fprintf(stderr, "Out of memory attempting to record a VSI error.\n");
            return NULL;
        }
        psCtx->nLastErrNo = VSIE_None;
        psCtx->nLastErrMsgMax = sizeof(psCtx->szLastErrMsg);
        CPLSetTLS( CTLS_VSIERRORCONTEXT, psCtx, TRUE );
    }

    return psCtx;
}

/**********************************************************************
 *                          VSIError()
 **********************************************************************/

/**
 * Report an VSI filesystem error.
 *
 * This function records an error in the filesystem that may or may not be
 * used in the future, for example converted into a CPLError. This allows
 * filesystem errors to be available to error handling functionality, but
 * reported only when necessary.
 *
 * @param err_no the error number (VSIE_*) from cpl_vsi_error.h.
 * @param fmt a printf() style format string.  Any additional arguments
 * will be treated as arguments to fill in this format in a manner
 * similar to printf().
 */

void    VSIError(VSIErrorNum err_no, const char *fmt, ...)
{
    va_list args;

    // Expand the error message
    va_start(args, fmt);
    VSIErrorV( err_no, fmt, args );
    va_end(args);
}

/************************************************************************/
/*                             VSIErrorV()                              */
/************************************************************************/

void    VSIErrorV( VSIErrorNum err_no, const char *fmt, va_list args )
{
    VSIErrorContext *psCtx = VSIGetErrorContext();
    if( psCtx == NULL )
      return;

/* -------------------------------------------------------------------- */
/*      Expand the error message                                        */
/* -------------------------------------------------------------------- */
#if defined(HAVE_VSNPRINTF)
    {
        va_list wrk_args;

#ifdef va_copy
        va_copy( wrk_args, args );
#else
        wrk_args = args;
#endif

        int nPreviousSize = 0;
        int nPR;
        while( ((nPR = CPLvsnprintf(
                     psCtx->szLastErrMsg+nPreviousSize,
                     psCtx->nLastErrMsgMax-nPreviousSize, fmt, wrk_args )) == -1
                || nPR >= psCtx->nLastErrMsgMax-nPreviousSize-1)
               && psCtx->nLastErrMsgMax < 1000000 )
        {
#ifdef va_copy
            va_end( wrk_args );
            va_copy( wrk_args, args );
#else
            wrk_args = args;
#endif
            psCtx->nLastErrMsgMax *= 3;
            psCtx = static_cast<VSIErrorContext *> (
                CPLRealloc( psCtx,
                            sizeof(VSIErrorContext)
                            - DEFAULT_LAST_ERR_MSG_SIZE
                            + psCtx->nLastErrMsgMax + 1) );
            CPLSetTLS( CTLS_ERRORCONTEXT, psCtx, TRUE );
        }

        va_end( wrk_args );
    }
#else
    // !HAVE_VSNPRINTF
    CPLvsnprintf( psCtx->szLastErrMsg, psCtx->nLastErrMsgMax, fmt, args);
#endif

    psCtx->nLastErrNo = err_no;
}

/**********************************************************************
 *                          VSIErrorReset()
 **********************************************************************/

/**
 * Erase any traces of previous errors.
 *
 * This is used to clear out the latest file system error when it is either translated
 * into a CPLError call or when it is determined to be ignorable.
 */

void CPL_STDCALL VSIErrorReset()
{
    VSIErrorContext *psCtx = VSIGetErrorContext();
    if( psCtx == NULL )
        return;

    psCtx->nLastErrNo = VSIE_None;
    psCtx->szLastErrMsg[0] = '\0';
}

/**********************************************************************
 *                          VSIGetLastErrorNo()
 **********************************************************************/

/**
 * Fetch the last error number.
 *
 * Fetches the last error number posted with VSIError(), that hasn't
 * been cleared by VSIErrorReset().  This is the error number, not the error
 * class.
 *
 * @return the error number of the last error to occur, or VSIE_None (0)
 * if there are no posted errors.
 */

VSIErrorNum CPL_STDCALL VSIGetLastErrorNo()
{
    VSIErrorContext *psCtx = VSIGetErrorContext();
    if( psCtx == NULL )
        return 0;

    return psCtx->nLastErrNo;
}

/**********************************************************************
 *                          VSIGetLastErrorMsg()
 **********************************************************************/

/**
 * Get the last error message.
 *
 * Fetches the last error message posted with VSIError(), that hasn't
 * been cleared by VSIErrorReset().  The returned pointer is to an internal
 * string that should not be altered or freed.
 *
 * @return the last error message, or NULL if there is no posted error
 * message.
 */

const char* CPL_STDCALL VSIGetLastErrorMsg()
{
    VSIErrorContext *psCtx = VSIGetErrorContext();
    if( psCtx == NULL )
        return "";

    return psCtx->szLastErrMsg;
}

/**********************************************************************
 *                          VSItoCPLError()
 **********************************************************************/

/**
 * Translate the VSI error into a CPLError call
 *
 * If there is a VSIError that is set, translate it to a CPLError call
 * with the given CPLErr error class, and either an appropriate CPLErrorNum
 * given the VSIErrorNum, or the given defualt CPLErrorNum.
 *
 * @return TRUE if a CPLError was issued, or FALSE if not.
 */

int CPL_DLL CPL_STDCALL VSIToCPLError(CPLErr eErrClass, CPLErrorNum eDefaultErrorNo) {
    int err = VSIGetLastErrorNo();
    switch(err) {
        case VSIE_None:
            return FALSE;
        case VSIE_FileError:
            CPLError(eErrClass, eDefaultErrorNo, "%s", VSIGetLastErrorMsg());
            break;
        case VSIE_HttpError:
            CPLError(eErrClass, CPLE_HttpResponse, "%s", VSIGetLastErrorMsg());
            break;
        case VSIE_AWSAccessDenied:
            CPLError(eErrClass, CPLE_AWSAccessDenied, "%s", VSIGetLastErrorMsg());
            break;
        case VSIE_AWSBucketNotFound:
            CPLError(eErrClass, CPLE_AWSBucketNotFound, "%s", VSIGetLastErrorMsg());
            break;
        case VSIE_AWSObjectNotFound:
            CPLError(eErrClass, CPLE_AWSObjectNotFound, "%s", VSIGetLastErrorMsg());
            break;
        case VSIE_AWSInvalidCredentials:
            CPLError(eErrClass, CPLE_AWSInvalidCredentials, "%s", VSIGetLastErrorMsg());
            break;
        case VSIE_AWSSignatureDoesNotMatch:
            CPLError(eErrClass, CPLE_AWSSignaturDoesNotMatch, "%s", VSIGetLastErrorMsg());
            break;
        default:
            CPLError(eErrClass, CPLE_HttpResponse, "A filesystem error with code %d occurred", err);
            break;
    }

    return TRUE;
}
