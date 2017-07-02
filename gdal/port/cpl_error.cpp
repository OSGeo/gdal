/**********************************************************************
 *
 * Name:     cpl_error.cpp
 * Project:  CPL - Common Portability Library
 * Purpose:  Error handling functions.
 * Author:   Daniel Morissette, danmo@videotron.ca
 *
 **********************************************************************
 * Copyright (c) 1998, Daniel Morissette
 * Copyright (c) 2007-2013, Even Rouault <even dot rouault at mines-paris dot org>
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

#include "cpl_error.h"

#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include <algorithm>

#include "cpl_config.h"
#include "cpl_conv.h"
#include "cpl_multiproc.h"
#include "cpl_string.h"
#include "cpl_vsi.h"

#if !defined(va_copy) && defined(__va_copy)
#define va_copy __va_copy
#endif

#define TIMESTAMP_DEBUG
// #define MEMORY_DEBUG

CPL_CVSID("$Id$")

static CPLMutex *hErrorMutex = NULL;
static void *pErrorHandlerUserData = NULL;
static CPLErrorHandler pfnErrorHandler = CPLDefaultErrorHandler;
static bool gbCatchDebug = true;

static const int DEFAULT_LAST_ERR_MSG_SIZE =
#if !defined(HAVE_VSNPRINTF)
    20000
#else
    500
#endif
    ;

typedef struct errHandler
{
    struct errHandler   *psNext;
    void                *pUserData;
    CPLErrorHandler     pfnHandler;
    bool                bCatchDebug;
} CPLErrorHandlerNode;

typedef struct {
    CPLErrorNum nLastErrNo;
    CPLErr  eLastErrType;
    CPLErrorHandlerNode *psHandlerStack;
    int     nLastErrMsgMax;
    int     nFailureIntoWarning;
    GUInt32 nErrorCounter;
    char    szLastErrMsg[DEFAULT_LAST_ERR_MSG_SIZE];
    // Do not add anything here. szLastErrMsg must be the last field.
    // See CPLRealloc() below.
} CPLErrorContext;

static const CPLErrorContext sNoErrorContext =
{
    0,
    CE_None,
    NULL,
    0,
    0,
    0,
    ""
};

static const CPLErrorContext sWarningContext =
{
    0,
    CE_Warning,
    NULL,
    0,
    0,
    0,
    "A warning was emitted"
};

static const CPLErrorContext sFailureContext =
{
    0,
    CE_Warning,
    NULL,
    0,
    0,
    0,
    "A failure was emitted"
};

#define IS_PREFEFINED_ERROR_CTX(psCtxt) ( psCtx == &sNoErrorContext || \
                                          psCtx == &sWarningContext || \
                                          psCtxt == &sFailureContext )


/************************************************************************/
/*                     CPLErrorContextGetString()                       */
/************************************************************************/

// Makes clang -fsanitize=undefined happy since it doesn't like
// dereferencing szLastErrMsg[i>=DEFAULT_LAST_ERR_MSG_SIZE]

static char* CPLErrorContextGetString(CPLErrorContext* psCtxt)
{
    return psCtxt->szLastErrMsg;
}

/************************************************************************/
/*                         CPLGetErrorContext()                         */
/************************************************************************/

static CPLErrorContext *CPLGetErrorContext()

{
    int bError = FALSE;
    CPLErrorContext *psCtx =
        reinterpret_cast<CPLErrorContext *>(
            CPLGetTLSEx( CTLS_ERRORCONTEXT, &bError ) );
    if( bError )
        return NULL;

    if( psCtx == NULL )
    {
        psCtx = static_cast<CPLErrorContext *>(
            VSICalloc( sizeof(CPLErrorContext), 1) );
        if( psCtx == NULL )
        {
            fprintf(stderr, "Out of memory attempting to report error.\n");
            return NULL;
        }
        psCtx->eLastErrType = CE_None;
        psCtx->nLastErrMsgMax = sizeof(psCtx->szLastErrMsg);
        CPLSetTLS( CTLS_ERRORCONTEXT, psCtx, TRUE );
    }

    return psCtx;
}

/************************************************************************/
/*                         CPLGetErrorHandlerUserData()                 */
/************************************************************************/

/**
 * Fetch the user data for the error context
 *
 * Fetches the user data for the current error context.  You can
 * set the user data for the error context when you add your handler by
 * issuing CPLSetErrorHandlerEx() and CPLPushErrorHandlerEx().  Note that
 * user data is primarily intended for providing context within error handlers
 * themselves, but they could potentially be abused in other useful ways with
 * the usual caveat emptor understanding.
 *
 * @return the user data pointer for the error context
 */

void* CPL_STDCALL CPLGetErrorHandlerUserData(void)
{
    CPLErrorContext *psCtx = CPLGetErrorContext();
    if( psCtx == NULL || IS_PREFEFINED_ERROR_CTX(psCtx) )
        abort();
    return reinterpret_cast<void*>(
        psCtx->psHandlerStack ?
        psCtx->psHandlerStack->pUserData : pErrorHandlerUserData );
}

/**********************************************************************
 *                          CPLError()
 **********************************************************************/

/**
 * Report an error.
 *
 * This function reports an error in a manner that can be hooked
 * and reported appropriate by different applications.
 *
 * The effect of this function can be altered by applications by installing
 * a custom error handling using CPLSetErrorHandler().
 *
 * The eErrClass argument can have the value CE_Warning indicating that the
 * message is an informational warning, CE_Failure indicating that the
 * action failed, but that normal recover mechanisms will be used or
 * CE_Fatal meaning that a fatal error has occurred, and that CPLError()
 * should not return.
 *
 * The default behaviour of CPLError() is to report errors to stderr,
 * and to abort() after reporting a CE_Fatal error.  It is expected that
 * some applications will want to suppress error reporting, and will want to
 * install a C++ exception, or longjmp() approach to no local fatal error
 * recovery.
 *
 * Regardless of how application error handlers or the default error
 * handler choose to handle an error, the error number, and message will
 * be stored for recovery with CPLGetLastErrorNo() and CPLGetLastErrorMsg().
 *
 * @param eErrClass one of CE_Warning, CE_Failure or CE_Fatal.
 * @param err_no the error number (CPLE_*) from cpl_error.h.
 * @param fmt a printf() style format string.  Any additional arguments
 * will be treated as arguments to fill in this format in a manner
 * similar to printf().
 */

void CPLError( CPLErr eErrClass, CPLErrorNum err_no,
               CPL_FORMAT_STRING(const char *fmt), ... )
{
    va_list args;

    // Expand the error message.
    va_start(args, fmt);
    CPLErrorV( eErrClass, err_no, fmt, args );
    va_end(args);
}

/************************************************************************/
/*                             CPLErrorV()                              */
/************************************************************************/

/** Same as CPLError() but with a va_list */
void CPLErrorV( CPLErr eErrClass, CPLErrorNum err_no, const char *fmt,
                va_list args )
{
    CPLErrorContext *psCtx = CPLGetErrorContext();
    if( psCtx == NULL || IS_PREFEFINED_ERROR_CTX(psCtx) )
    {
        int bMemoryError = FALSE;
        if( eErrClass == CE_Warning )
        {
            CPLSetTLSWithFreeFuncEx(
                CTLS_ERRORCONTEXT,
                reinterpret_cast<void*>(
                    const_cast<CPLErrorContext *>( &sWarningContext ) ),
                NULL, &bMemoryError );
        }
        else if( eErrClass == CE_Failure )
        {
            CPLSetTLSWithFreeFuncEx(
                CTLS_ERRORCONTEXT,
                reinterpret_cast<void*>(
                    const_cast<CPLErrorContext *>( &sFailureContext ) ),
                NULL, &bMemoryError );
        }

        // TODO: Is it possible to move the entire szShortMessage under the if
        // pfnErrorHandler?
        char szShortMessage[80] = {};
        CPLvsnprintf( szShortMessage, sizeof(szShortMessage), fmt, args );

        CPLMutexHolderD( &hErrorMutex );
        if( pfnErrorHandler != NULL )
            pfnErrorHandler(eErrClass, err_no, szShortMessage);
        return;
    }

    if( psCtx->nFailureIntoWarning > 0 && eErrClass == CE_Failure )
        eErrClass = CE_Warning;

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

/* -------------------------------------------------------------------- */
/*      If CPL_ACCUM_ERROR_MSG=ON accumulate the error messages,        */
/*      rather than just replacing the last error message.              */
/* -------------------------------------------------------------------- */
        int nPreviousSize = 0;
        if( psCtx->psHandlerStack != NULL &&
            EQUAL(CPLGetConfigOption( "CPL_ACCUM_ERROR_MSG", "" ), "ON"))
        {
            nPreviousSize = static_cast<int>(strlen(psCtx->szLastErrMsg));
            if( nPreviousSize )
            {
                if( nPreviousSize + 1 + 1 >= psCtx->nLastErrMsgMax )
                {
                    psCtx->nLastErrMsgMax *= 3;
                    psCtx = static_cast<CPLErrorContext *> (
                        CPLRealloc(psCtx,
                                   sizeof(CPLErrorContext)
                                   - DEFAULT_LAST_ERR_MSG_SIZE
                                   + psCtx->nLastErrMsgMax + 1));
                    CPLSetTLS( CTLS_ERRORCONTEXT, psCtx, TRUE );
                }
                char* pszLastErrMsg = CPLErrorContextGetString(psCtx);
                pszLastErrMsg[nPreviousSize] = '\n';
                pszLastErrMsg[nPreviousSize+1] = '\0';
                nPreviousSize++;
            }
        }

        int nPR = 0;
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
            psCtx = static_cast<CPLErrorContext *> (
                CPLRealloc(psCtx,
                           sizeof(CPLErrorContext)
                           - DEFAULT_LAST_ERR_MSG_SIZE
                           + psCtx->nLastErrMsgMax + 1));
            CPLSetTLS( CTLS_ERRORCONTEXT, psCtx, TRUE );
        }

        va_end( wrk_args );
    }
#else
    // !HAVE_VSNPRINTF
    CPLvsnprintf( psCtx->szLastErrMsg, psCtx->nLastErrMsgMax, fmt, args);
#endif

/* -------------------------------------------------------------------- */
/*      Obfuscate any password in error message                         */
/* -------------------------------------------------------------------- */
    char* pszPassword = strstr(psCtx->szLastErrMsg, "password=");
    if( pszPassword != NULL )
    {
        char* pszIter = pszPassword + strlen("password=");
        while( *pszIter != ' ' && *pszIter != '\0' )
        {
            *pszIter = 'X';
            pszIter++;
        }
    }

/* -------------------------------------------------------------------- */
/*      If the user provided an handling function, then                 */
/*      call it, otherwise print the error to stderr and return.        */
/* -------------------------------------------------------------------- */
    psCtx->nLastErrNo = err_no;
    psCtx->eLastErrType = eErrClass;
    if( psCtx->nErrorCounter == ~(0U) )
        psCtx->nErrorCounter = 0;
    else
        psCtx->nErrorCounter ++;

    if( CPLGetConfigOption("CPL_LOG_ERRORS", NULL) != NULL )
        CPLDebug( "CPLError", "%s", psCtx->szLastErrMsg );

/* -------------------------------------------------------------------- */
/*      Invoke the current error handler.                               */
/* -------------------------------------------------------------------- */
    if( psCtx->psHandlerStack != NULL )
    {
        psCtx->psHandlerStack->pfnHandler(eErrClass, err_no,
                                          psCtx->szLastErrMsg);
    }
    else
    {
        CPLMutexHolderD( &hErrorMutex );
        if( pfnErrorHandler != NULL )
            pfnErrorHandler(eErrClass, err_no, psCtx->szLastErrMsg);
    }

    if( eErrClass == CE_Fatal )
        abort();
}

/************************************************************************/
/*                         CPLEmergencyError()                          */
/************************************************************************/

/**
 * Fatal error when things are bad.
 *
 * This function should be called in an emergency situation where
 * it is unlikely that a regular error report would work.  This would
 * include in the case of heap exhaustion for even small allocations,
 * or any failure in the process of reporting an error (such as TLS
 * allocations).
 *
 * This function should never return.  After the error message has been
 * reported as best possible, the application will abort() similarly to how
 * CPLError() aborts on CE_Fatal class errors.
 *
 * @param pszMessage the error message to report.
 */

void CPLEmergencyError( const char *pszMessage )
{
    static bool bInEmergencyError = false;

    // If we are already in emergency error then one of the
    // following failed, so avoid them the second time through.
    if( !bInEmergencyError )
    {
        bInEmergencyError = true;
        CPLErrorContext *psCtx =
            static_cast<CPLErrorContext *>(CPLGetTLS( CTLS_ERRORCONTEXT ));

        if( psCtx != NULL && psCtx->psHandlerStack != NULL )
        {
            psCtx->psHandlerStack->pfnHandler( CE_Fatal, CPLE_AppDefined,
                                               pszMessage );
        }
        else if( pfnErrorHandler != NULL )
        {
            pfnErrorHandler( CE_Fatal, CPLE_AppDefined, pszMessage );
        }
    }

    // Ultimate fallback.
    fprintf( stderr, "FATAL: %s\n", pszMessage );

    abort();
}

/************************************************************************/
/*                    CPLGetProcessMemorySize()                         */
/************************************************************************/

#ifdef MEMORY_DEBUG

#ifdef __linux
static int CPLGetProcessMemorySize()
{
    FILE* fp = fopen("/proc/self/status", "r");
    if( fp == NULL )
        return -1;
    int nRet = -1;
    char szLine[128] = {};
    while( fgets(szLine, sizeof(szLine), fp) != NULL )
    {
        if( STARTS_WITH(szLine, "VmSize:") )
        {
            const char* pszPtr = szLine;
            while( !(*pszPtr == '\0' || (*pszPtr >= '0' && *pszPtr <= '9')) )
                 pszPtr++;
            nRet = atoi(pszPtr);
            break;
        }
    }
    fclose(fp);
    return nRet;
}
#else
#error CPLGetProcessMemorySize() unimplemented for this OS
#endif

#endif // def MEMORY_DEBUG

/************************************************************************/
/*                              CPLDebug()                              */
/************************************************************************/

/**
 * Display a debugging message.
 *
 * The category argument is used in conjunction with the CPL_DEBUG
 * environment variable to establish if the message should be displayed.
 * If the CPL_DEBUG environment variable is not set, no debug messages
 * are emitted (use CPLError(CE_Warning, ...) to ensure messages are displayed).
 * If CPL_DEBUG is set, but is an empty string or the word "ON" then all
 * debug messages are shown.  Otherwise only messages whose category appears
 * somewhere within the CPL_DEBUG value are displayed (as determined by
 * strstr()).
 *
 * Categories are usually an identifier for the subsystem producing the
 * error.  For instance "GDAL" might be used for the GDAL core, and "TIFF"
 * for messages from the TIFF translator.
 *
 * @param pszCategory name of the debugging message category.
 * @param pszFormat printf() style format string for message to display.
 *        Remaining arguments are assumed to be for format.
 */

#ifdef WITHOUT_CPLDEBUG
// Do not include CPLDebug.  Only available in custom builds.
#else
void CPLDebug( const char * pszCategory,
               CPL_FORMAT_STRING(const char * pszFormat), ... )

{
    CPLErrorContext *psCtx = CPLGetErrorContext();
    if( psCtx == NULL || IS_PREFEFINED_ERROR_CTX(psCtx) )
        return;
    const char *pszDebug = CPLGetConfigOption("CPL_DEBUG", NULL);

/* -------------------------------------------------------------------- */
/*      Does this message pass our current criteria?                    */
/* -------------------------------------------------------------------- */
    if( pszDebug == NULL )
        return;

    if( !EQUAL(pszDebug, "ON") && !EQUAL(pszDebug, "") )
    {
        const size_t nLen = strlen(pszCategory);

        size_t i = 0;
        for( i = 0; pszDebug[i] != '\0'; i++ )
        {
            if( EQUALN(pszCategory, pszDebug+i, nLen) )
                break;
        }

        if( pszDebug[i] == '\0' )
            return;
    }

/* -------------------------------------------------------------------- */
/*    Allocate a block for the error.                                   */
/* -------------------------------------------------------------------- */
    const int ERROR_MAX = 25000;
    char *pszMessage = static_cast<char *>( VSIMalloc( ERROR_MAX ) );
    if( pszMessage == NULL )
        return;

/* -------------------------------------------------------------------- */
/*      Dal -- always log a timestamp as the first part of the line     */
/*      to ensure one is looking at what one should be looking at!      */
/* -------------------------------------------------------------------- */

    pszMessage[0] = '\0';
#ifdef TIMESTAMP_DEBUG
    if( CPLGetConfigOption( "CPL_TIMESTAMP", NULL ) != NULL )
    {
        strcpy( pszMessage, VSICTime( VSITime(NULL) ) );

        // On windows anyway, ctime puts a \n at the end, but I'm not
        // convinced this is standard behaviour, so we'll get rid of it
        // carefully

        if( pszMessage[strlen(pszMessage) -1 ] == '\n' )
        {
            pszMessage[strlen(pszMessage) - 1] = 0; // blow it out
        }
        strcat( pszMessage, ": " );
    }
#endif

/* -------------------------------------------------------------------- */
/*      Add the process memory size.                                    */
/* -------------------------------------------------------------------- */
#ifdef MEMORY_DEBUG
    char szVmSize[32] = {};
    CPLsprintf( szVmSize, "[VmSize: %d] ", CPLGetProcessMemorySize());
    strcat( pszMessage, szVmSize );
#endif

/* -------------------------------------------------------------------- */
/*      Add the category.                                               */
/* -------------------------------------------------------------------- */
    strcat( pszMessage, pszCategory );
    strcat( pszMessage, ": " );

/* -------------------------------------------------------------------- */
/*      Format the application provided portion of the debug message.   */
/* -------------------------------------------------------------------- */
    va_list args;
    va_start(args, pszFormat);

    CPLvsnprintf(pszMessage+strlen(pszMessage), ERROR_MAX - strlen(pszMessage),
                 pszFormat, args);

    va_end(args);

/* -------------------------------------------------------------------- */
/*      Obfuscate any password in error message                         */
/* -------------------------------------------------------------------- */

    char* pszPassword = strstr(pszMessage, "password=");
    if( pszPassword != NULL )
    {
        char* pszIter = pszPassword + strlen("password=");
        while( *pszIter != ' ' && *pszIter != '\0' )
        {
            *pszIter = 'X';
            pszIter++;
        }
    }

/* -------------------------------------------------------------------- */
/*      Invoke the current error handler.                               */
/* -------------------------------------------------------------------- */
    if( psCtx->psHandlerStack != NULL )
    {
        if( psCtx->psHandlerStack->bCatchDebug )
        {
            psCtx->psHandlerStack->pfnHandler( CE_Debug, CPLE_None,
                                               pszMessage );
        }
        else
        {
            CPLErrorHandlerNode *psNode = psCtx->psHandlerStack->psNext;
            while( psNode != NULL )
            {
                if( psNode->bCatchDebug )
                {
                    psNode->pfnHandler( CE_Debug, CPLE_None, pszMessage );
                    break;
                }
                psNode = psNode->psNext;
            }
            if( psNode == NULL )
            {
                CPLMutexHolderD( &hErrorMutex );
                if( gbCatchDebug )
                    pfnErrorHandler( CE_Debug, CPLE_None, pszMessage );
                else
                    CPLDefaultErrorHandler( CE_Debug, CPLE_None, pszMessage );
            }
        }
    }
    else
    {
        CPLMutexHolderD( &hErrorMutex );
        if( pfnErrorHandler != NULL )
        {
            if( gbCatchDebug )
                pfnErrorHandler( CE_Debug, CPLE_None, pszMessage );
            else
                CPLDefaultErrorHandler( CE_Debug, CPLE_None, pszMessage );
        }
    }

    VSIFree( pszMessage );
}
#endif  // !WITHOUT_CPLDEBUG

/**********************************************************************
 *                          CPLErrorReset()
 **********************************************************************/

/**
 * Erase any traces of previous errors.
 *
 * This is normally used to ensure that an error which has been recovered
 * from does not appear to be still in play with high level functions.
 */

void CPL_STDCALL CPLErrorReset()
{
    CPLErrorContext *psCtx = CPLGetErrorContext();
    if( psCtx == NULL )
        return;
    if( IS_PREFEFINED_ERROR_CTX(psCtx) )
    {
        int bMemoryError = FALSE;
        CPLSetTLSWithFreeFuncEx(
            CTLS_ERRORCONTEXT,
            reinterpret_cast<void*>(
                const_cast<CPLErrorContext *>( &sNoErrorContext ) ),
            NULL, &bMemoryError );
        return;
    }

    psCtx->nLastErrNo = CPLE_None;
    psCtx->szLastErrMsg[0] = '\0';
    psCtx->eLastErrType = CE_None;
    psCtx->nErrorCounter = 0;
}

/**********************************************************************
 *                       CPLErrorSetState()
 **********************************************************************/

/**
 * Restore an error state, without emitting an error.
 *
 * Can be useful if a routine might call CPLErrorReset() and one wants to
 * preserve the previous error state.
 *
 * @since GDAL 2.0
 */

void CPL_DLL CPLErrorSetState( CPLErr eErrClass, CPLErrorNum err_no,
                               const char* pszMsg )
{
    CPLErrorContext *psCtx = CPLGetErrorContext();
    if( psCtx == NULL )
        return;
    if( IS_PREFEFINED_ERROR_CTX(psCtx) )
    {
        int bMemoryError = FALSE;
        if( eErrClass == CE_None )
            CPLSetTLSWithFreeFuncEx(
                CTLS_ERRORCONTEXT,
                reinterpret_cast<void*>(
                    const_cast<CPLErrorContext *>( &sNoErrorContext ) ),
                NULL, &bMemoryError );
        else if( eErrClass == CE_Warning )
            CPLSetTLSWithFreeFuncEx(
                CTLS_ERRORCONTEXT,
                reinterpret_cast<void*>(
                    const_cast<CPLErrorContext *>( &sWarningContext ) ),
                NULL, &bMemoryError );
        else if( eErrClass == CE_Failure )
            CPLSetTLSWithFreeFuncEx(
                CTLS_ERRORCONTEXT,
                reinterpret_cast<void*>(
                    const_cast<CPLErrorContext *>( &sFailureContext ) ),
                NULL, &bMemoryError );
        return;
    }

    psCtx->nLastErrNo = err_no;
    const size_t size = std::min(
        static_cast<size_t>(psCtx->nLastErrMsgMax-1), strlen(pszMsg) );
    char* pszLastErrMsg = CPLErrorContextGetString(psCtx);
    strncpy( pszLastErrMsg, pszMsg, size );
    pszLastErrMsg[size] = '\0';
    psCtx->eLastErrType = eErrClass;
}

/**********************************************************************
 *                          CPLGetLastErrorNo()
 **********************************************************************/

/**
 * Fetch the last error number.
 *
 * Fetches the last error number posted with CPLError(), that hasn't
 * been cleared by CPLErrorReset().  This is the error number, not the error
 * class.
 *
 * @return the error number of the last error to occur, or CPLE_None (0)
 * if there are no posted errors.
 */

CPLErrorNum CPL_STDCALL CPLGetLastErrorNo()
{
    CPLErrorContext *psCtx = CPLGetErrorContext();
    if( psCtx == NULL )
        return 0;

    return psCtx->nLastErrNo;
}

/**********************************************************************
 *                          CPLGetLastErrorType()
 **********************************************************************/

/**
 * Fetch the last error type.
 *
 * Fetches the last error type posted with CPLError(), that hasn't
 * been cleared by CPLErrorReset().  This is the error class, not the error
 * number.
 *
 * @return the error type of the last error to occur, or CE_None (0)
 * if there are no posted errors.
 */

CPLErr CPL_STDCALL CPLGetLastErrorType()
{
    CPLErrorContext *psCtx = CPLGetErrorContext();
    if( psCtx == NULL )
        return CE_None;

    return psCtx->eLastErrType;
}

/**********************************************************************
 *                          CPLGetLastErrorMsg()
 **********************************************************************/

/**
 * Get the last error message.
 *
 * Fetches the last error message posted with CPLError(), that hasn't
 * been cleared by CPLErrorReset().  The returned pointer is to an internal
 * string that should not be altered or freed.
 *
 * @return the last error message, or NULL if there is no posted error
 * message.
 */

const char* CPL_STDCALL CPLGetLastErrorMsg()
{
    CPLErrorContext *psCtx = CPLGetErrorContext();
    if( psCtx == NULL )
        return "";

    return psCtx->szLastErrMsg;
}

/**********************************************************************
 *                          CPLGetErrorCounter()
 **********************************************************************/

/**
 * Get the error counter
 *
 * Fetches the number of errors emitted in the current error context,
 * since the last call to CPLErrorReset()
 *
 * @return the error counter.
 * @since GDAL 2.3
 */

GUInt32 CPL_STDCALL CPLGetErrorCounter()
{
    CPLErrorContext *psCtx = CPLGetErrorContext();
    if( psCtx == NULL )
        return 0;

    return psCtx->nErrorCounter;
}

/************************************************************************/
/*                       CPLDefaultErrorHandler()                       */
/************************************************************************/

/** Default error handler. */
void CPL_STDCALL CPLDefaultErrorHandler( CPLErr eErrClass, CPLErrorNum nError,
                                         const char * pszErrorMsg )

{
    static int nCount = 0;
    static int nMaxErrors = -1;

    if( eErrClass != CE_Debug )
    {
        if( nMaxErrors == -1 )
        {
            nMaxErrors =
                atoi(CPLGetConfigOption( "CPL_MAX_ERROR_REPORTS", "1000" ));
        }

        nCount++;
        if( nCount > nMaxErrors && nMaxErrors > 0 )
            return;
    }

    static FILE *fpLog = stderr;

    static bool bLogInit = false;
    if( !bLogInit )
    {
        bLogInit = true;

        fpLog = stderr;
        if( CPLGetConfigOption( "CPL_LOG", NULL ) != NULL )
        {
            const char* pszAccess = "wt";
            if( CPLGetConfigOption( "CPL_LOG_APPEND", NULL ) != NULL )
                pszAccess = "at";
            fpLog = fopen( CPLGetConfigOption("CPL_LOG", ""), pszAccess );
            if( fpLog == NULL )
                fpLog = stderr;
        }
    }

    if( eErrClass == CE_Debug )
        fprintf( fpLog, "%s\n", pszErrorMsg );
    else if( eErrClass == CE_Warning )
        fprintf( fpLog, "Warning %d: %s\n", nError, pszErrorMsg );
    else
        fprintf( fpLog, "ERROR %d: %s\n", nError, pszErrorMsg );

    if( eErrClass != CE_Debug
        && nMaxErrors > 0
        && nCount == nMaxErrors )
    {
        fprintf( fpLog,
                 "More than %d errors or warnings have been reported. "
                 "No more will be reported from now.\n",
                 nMaxErrors );
    }

    fflush( fpLog );
}

/************************************************************************/
/*                        CPLQuietErrorHandler()                        */
/************************************************************************/

/** Error handler that does not do anything, except for debug messages. */
void CPL_STDCALL CPLQuietErrorHandler( CPLErr eErrClass , CPLErrorNum nError,
                                       const char * pszErrorMsg )

{
    if( eErrClass == CE_Debug )
        CPLDefaultErrorHandler( eErrClass, nError, pszErrorMsg );
}

/************************************************************************/
/*                       CPLLoggingErrorHandler()                       */
/************************************************************************/

/** Error handler that logs into the file defined by the CPL_LOG configuration
 * option, or stderr otherwise.
 */
void CPL_STDCALL CPLLoggingErrorHandler( CPLErr eErrClass, CPLErrorNum nError,
                                         const char * pszErrorMsg )

{
    static bool bLogInit = false;
    static FILE *fpLog = stderr;

    if( !bLogInit )
    {
        bLogInit = true;

        CPLSetConfigOption( "CPL_TIMESTAMP", "ON" );

        const char *cpl_log = CPLGetConfigOption("CPL_LOG", NULL );

        fpLog = stderr;
        if( cpl_log != NULL && EQUAL(cpl_log, "OFF") )
        {
            fpLog = NULL;
        }
        else if( cpl_log != NULL )
        {
            size_t nPathLen = strlen(cpl_log) + 20;
            char* pszPath = static_cast<char *>(CPLMalloc(nPathLen));
            strcpy(pszPath, cpl_log);

            int i = 0;
            while( (fpLog = fopen( pszPath, "rt" )) != NULL )
            {
                fclose( fpLog );

                // Generate sequenced log file names, inserting # before ext.
                if( strrchr(cpl_log, '.') == NULL )
                {
                    snprintf( pszPath, nPathLen, "%s_%d%s", cpl_log, i++,
                             ".log" );
                }
                else
                {
                    size_t pos = 0;
                    char *cpl_log_base = CPLStrdup(cpl_log);
                    pos = strcspn(cpl_log_base, ".");
                    if( pos > 0 )
                    {
                        cpl_log_base[pos] = '\0';
                    }
                    snprintf( pszPath, nPathLen, "%s_%d%s", cpl_log_base,
                             i++, ".log" );
                    CPLFree(cpl_log_base);
                }
            }

            fpLog = fopen( pszPath, "wt" );
            CPLFree(pszPath);
        }
    }

    if( fpLog == NULL )
        return;

    if( eErrClass == CE_Debug )
        fprintf( fpLog, "%s\n", pszErrorMsg );
    else if( eErrClass == CE_Warning )
        fprintf( fpLog, "Warning %d: %s\n", nError, pszErrorMsg );
    else
        fprintf( fpLog, "ERROR %d: %s\n", nError, pszErrorMsg );

    fflush( fpLog );
}

/**********************************************************************
 *                      CPLTurnFailureIntoWarning()                   *
 **********************************************************************/

/** Whether failures should be turned into warnings.
 */
void CPLTurnFailureIntoWarning( int bOn )
{
    CPLErrorContext *psCtx = CPLGetErrorContext();
    if( psCtx == NULL || IS_PREFEFINED_ERROR_CTX(psCtx) )
    {
        fprintf(stderr, "CPLTurnFailureIntoWarning() failed.\n");
        return;
    }
    psCtx->nFailureIntoWarning += (bOn) ? 1 : -1;
    if( psCtx->nFailureIntoWarning < 0 )
    {
        CPLDebug( "CPL", "Wrong nesting of CPLTurnFailureIntoWarning(TRUE) / "
                  "CPLTurnFailureIntoWarning(FALSE)" );
    }
}

/**********************************************************************
 *                          CPLSetErrorHandlerEx()                    *
 **********************************************************************/

/**
 * Install custom error handle with user's data. This method is
 * essentially CPLSetErrorHandler with an added pointer to pUserData.
 * The pUserData is not returned in the CPLErrorHandler, however, and
 * must be fetched via CPLGetErrorHandlerUserData.
 *
 * @param pfnErrorHandlerNew new error handler function.
 * @param pUserData User data to carry along with the error context.
 * @return returns the previously installed error handler.
 */

CPLErrorHandler CPL_STDCALL
CPLSetErrorHandlerEx( CPLErrorHandler pfnErrorHandlerNew, void* pUserData )
{
    CPLErrorContext *psCtx = CPLGetErrorContext();
    if( psCtx == NULL || IS_PREFEFINED_ERROR_CTX(psCtx) )
    {
        fprintf(stderr, "CPLSetErrorHandlerEx() failed.\n");
        return NULL;
    }

    if( psCtx->psHandlerStack != NULL )
    {
        CPLDebug( "CPL",
                  "CPLSetErrorHandler() called with an error handler on "
                  "the local stack.  New error handler will not be used "
                  "immediately." );
    }

    CPLErrorHandler pfnOldHandler = NULL;
    {
        CPLMutexHolderD( &hErrorMutex );

        pfnOldHandler = pfnErrorHandler;

        if( pfnErrorHandler == NULL )
            pfnErrorHandler = CPLDefaultErrorHandler;
        else
            pfnErrorHandler = pfnErrorHandlerNew;

        pErrorHandlerUserData = pUserData;
    }

    return pfnOldHandler;
}

/**********************************************************************
 *                          CPLSetErrorHandler()                      *
 **********************************************************************/

/**
 * Install custom error handler.
 *
 * Allow the library's user to specify an error handler function.
 * A valid error handler is a C function with the following prototype:
 *
 * <pre>
 *     void MyErrorHandler(CPLErr eErrClass, int err_no, const char *msg)
 * </pre>
 *
 * Pass NULL to come back to the default behavior.  The default behaviour
 * (CPLDefaultErrorHandler()) is to write the message to stderr.
 *
 * The msg will be a partially formatted error message not containing the
 * "ERROR %d:" portion emitted by the default handler.  Message formatting
 * is handled by CPLError() before calling the handler.  If the error
 * handler function is passed a CE_Fatal class error and returns, then
 * CPLError() will call abort(). Applications wanting to interrupt this
 * fatal behaviour will have to use longjmp(), or a C++ exception to
 * indirectly exit the function.
 *
 * Another standard error handler is CPLQuietErrorHandler() which doesn't
 * make any attempt to report the passed error or warning messages but
 * will process debug messages via CPLDefaultErrorHandler.
 *
 * Note that error handlers set with CPLSetErrorHandler() apply to all
 * threads in an application, while error handlers set with CPLPushErrorHandler
 * are thread-local.  However, any error handlers pushed with
 * CPLPushErrorHandler (and not removed with CPLPopErrorHandler) take
 * precedence over the global error handlers set with CPLSetErrorHandler().
 * Generally speaking CPLSetErrorHandler() would be used to set a desired
 * global error handler, while CPLPushErrorHandler() would be used to install
 * a temporary local error handler, such as CPLQuietErrorHandler() to suppress
 * error reporting in a limited segment of code.
 *
 * @param pfnErrorHandlerNew new error handler function.
 * @return returns the previously installed error handler.
 */
CPLErrorHandler CPL_STDCALL
CPLSetErrorHandler( CPLErrorHandler pfnErrorHandlerNew )
{
    return CPLSetErrorHandlerEx(pfnErrorHandlerNew, NULL);
}

/************************************************************************/
/*                        CPLPushErrorHandler()                         */
/************************************************************************/

/**
 * Push a new CPLError handler.
 *
 * This pushes a new error handler on the thread-local error handler
 * stack.  This handler will be used until removed with CPLPopErrorHandler().
 *
 * The CPLSetErrorHandler() docs have further information on how
 * CPLError handlers work.
 *
 * @param pfnErrorHandlerNew new error handler function.
 */

void CPL_STDCALL CPLPushErrorHandler( CPLErrorHandler pfnErrorHandlerNew )

{
    CPLPushErrorHandlerEx(pfnErrorHandlerNew, NULL);
}

/************************************************************************/
/*                        CPLPushErrorHandlerEx()                       */
/************************************************************************/

/**
 * Push a new CPLError handler with user data on the error context.
 *
 * This pushes a new error handler on the thread-local error handler
 * stack.  This handler will be used until removed with CPLPopErrorHandler().
 * Obtain the user data back by using CPLGetErrorContext().
 *
 * The CPLSetErrorHandler() docs have further information on how
 * CPLError handlers work.
 *
 * @param pfnErrorHandlerNew new error handler function.
 * @param pUserData User data to put on the error context.
 */
void CPL_STDCALL CPLPushErrorHandlerEx( CPLErrorHandler pfnErrorHandlerNew,
                                        void* pUserData )

{
    CPLErrorContext *psCtx = CPLGetErrorContext();

    if( psCtx == NULL || IS_PREFEFINED_ERROR_CTX(psCtx) )
    {
        fprintf(stderr, "CPLPushErrorHandlerEx() failed.\n");
        return;
    }

    CPLErrorHandlerNode *psNode = static_cast<CPLErrorHandlerNode *>(
        CPLMalloc( sizeof(CPLErrorHandlerNode) ) );
    psNode->psNext = psCtx->psHandlerStack;
    psNode->pfnHandler = pfnErrorHandlerNew;
    psNode->pUserData = pUserData;
    psNode->bCatchDebug = true;
    psCtx->psHandlerStack = psNode;
}

/************************************************************************/
/*                         CPLPopErrorHandler()                         */
/************************************************************************/

/**
 * Pop error handler off stack.
 *
 * Discards the current error handler on the error handler stack, and restores
 * the one in use before the last CPLPushErrorHandler() call.  This method
 * has no effect if there are no error handlers on the current threads error
 * handler stack.
 */

void CPL_STDCALL CPLPopErrorHandler()

{
    CPLErrorContext *psCtx = CPLGetErrorContext();

    if( psCtx == NULL || IS_PREFEFINED_ERROR_CTX(psCtx) )
    {
        fprintf(stderr, "CPLPopErrorHandler() failed.\n");
        return;
    }

    if( psCtx->psHandlerStack != NULL )
    {
        CPLErrorHandlerNode     *psNode = psCtx->psHandlerStack;

        psCtx->psHandlerStack = psNode->psNext;
        VSIFree( psNode );
    }
}

/************************************************************************/
/*                 CPLSetCurrentErrorHandlerCatchDebug()                */
/************************************************************************/

/**
 * Set if the current error handler should intercept debug messages, or if
 * they should be processed by the previous handler.
 *
 * By default when installing a custom error handler, this one intercepts
 * debug messages. In some cases, this might not be desirable and the user
 * would prefer that the previous installed handler (or the default one if no
 * previous installed handler exists in the stack) deal with it. In which
 * case, this function should be called with bCatchDebug.
 *
 * @param bCatchDebug FALSE if the current error handler should not intercept
 * debug messages
 * @since GDAL 2.1
 */

void CPL_STDCALL CPLSetCurrentErrorHandlerCatchDebug( int bCatchDebug )
{
    CPLErrorContext *psCtx = CPLGetErrorContext();

    if( psCtx == NULL || IS_PREFEFINED_ERROR_CTX(psCtx) )
    {
        fprintf(stderr, "CPLSetCurrentErrorHandlerCatchDebug() failed.\n");
        return;
    }

    if( psCtx->psHandlerStack != NULL )
        psCtx->psHandlerStack->bCatchDebug = CPL_TO_BOOL(bCatchDebug);
    else
        gbCatchDebug = CPL_TO_BOOL(bCatchDebug);
}

/************************************************************************/
/*                             _CPLAssert()                             */
/*                                                                      */
/*      This function is called only when an assertion fails.           */
/************************************************************************/

/**
 * Report failure of a logical assertion.
 *
 * Applications would normally use the CPLAssert() macro which expands
 * into code calling _CPLAssert() only if the condition fails.  _CPLAssert()
 * will generate a CE_Fatal error call to CPLError(), indicating the file
 * name, and line number of the failed assertion, as well as containing
 * the assertion itself.
 *
 * There is no reason for application code to call _CPLAssert() directly.
 */

void CPL_STDCALL _CPLAssert( const char * pszExpression, const char * pszFile,
                             int iLine )

{
    CPLError( CE_Fatal, CPLE_AssertionFailed,
              "Assertion `%s' failed "
              "in file `%s', line %d",
              pszExpression, pszFile, iLine );

    // Just to please compiler so it is aware the function does not return.
    abort();
}

/************************************************************************/
/*                       CPLCleanupErrorMutex()                         */
/************************************************************************/

void CPLCleanupErrorMutex()
{
    if( hErrorMutex != NULL )
    {
        CPLDestroyMutex(hErrorMutex);
        hErrorMutex = NULL;
    }
}
