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
 * DEALINGS IN THE SOFTWARE.
 ****************************************************************************/

#ifndef CPL_ERROR_H_INCLUDED
#define CPL_ERROR_H_INCLUDED

#include "cpl_port.h"

#include <stdarg.h>
#include <stddef.h>

/*=====================================================================
                   Error handling functions (cpl_error.c)
 =====================================================================*/

/**
 * \file cpl_error.h
 *
 * CPL error handling services.
 */

CPL_C_START

/** Error category */
typedef enum
{
    CE_None = 0,
    CE_Debug = 1,
    CE_Warning = 2,
    CE_Failure = 3,
    CE_Fatal = 4
} CPLErr;

/* ==================================================================== */
/*      Well known error codes.                                         */
/* ==================================================================== */

#ifdef STRICT_CPLERRORNUM_TYPE

/* This is not appropriate for the general case, as there are parts */
/* of GDAL which use custom error codes, but this can help diagnose confusions
 */
/* between CPLErr and CPLErrorNum */
typedef enum
{
    CPLE_None,
    CPLE_AppDefined,
    CPLE_OutOfMemory,
    CPLE_FileIO,
    CPLE_OpenFailed,
    CPLE_IllegalArg,
    CPLE_NotSupported,
    CPLE_AssertionFailed,
    CPLE_NoWriteAccess,
    CPLE_UserInterrupt,
    CPLE_ObjectNull,
    CPLE_HttpResponse,
    CPLE_AWSBucketNotFound,
    CPLE_AWSObjectNotFound,
    CPLE_AWSAccessDenied,
    CPLE_AWSInvalidCredentials,
    CPLE_AWSSignatureDoesNotMatch,
} CPLErrorNum;

#else

/** Error number */
typedef int CPLErrorNum;

/** No error */
#define CPLE_None 0
/** Application defined error */
#define CPLE_AppDefined 1
/** Out of memory error */
#define CPLE_OutOfMemory 2
/** File I/O error */
#define CPLE_FileIO 3
/** Open failed */
#define CPLE_OpenFailed 4
/** Illegal argument */
#define CPLE_IllegalArg 5
/** Not supported */
#define CPLE_NotSupported 6
/** Assertion failed */
#define CPLE_AssertionFailed 7
/** No write access */
#define CPLE_NoWriteAccess 8
/** User interrupted */
#define CPLE_UserInterrupt 9
/** NULL object */
#define CPLE_ObjectNull 10

/*
 * Filesystem-specific errors
 */
/** HTTP response */
#define CPLE_HttpResponse 11
/** AWSBucketNotFound */
#define CPLE_AWSBucketNotFound 12
/** AWSObjectNotFound */
#define CPLE_AWSObjectNotFound 13
/** AWSAccessDenied */
#define CPLE_AWSAccessDenied 14
/** AWSInvalidCredentials */
#define CPLE_AWSInvalidCredentials 15
/** AWSSignatureDoesNotMatch */
#define CPLE_AWSSignatureDoesNotMatch 16
/** VSIE_AWSError */
#define CPLE_AWSError 17

/* 100 - 299 reserved for GDAL */

#endif

void CPL_DLL CPLError(CPLErr eErrClass, CPLErrorNum err_no,
                      CPL_FORMAT_STRING(const char *fmt), ...)
    CPL_PRINT_FUNC_FORMAT(3, 4);
void CPL_DLL CPLErrorV(CPLErr, CPLErrorNum, const char *, va_list);
void CPL_DLL CPLEmergencyError(const char *) CPL_NO_RETURN;
void CPL_DLL CPL_STDCALL CPLErrorReset(void);
CPLErrorNum CPL_DLL CPL_STDCALL CPLGetLastErrorNo(void);
CPLErr CPL_DLL CPL_STDCALL CPLGetLastErrorType(void);
const char CPL_DLL *CPL_STDCALL CPLGetLastErrorMsg(void);
GUInt32 CPL_DLL CPL_STDCALL CPLGetErrorCounter(void);
void CPL_DLL *CPL_STDCALL CPLGetErrorHandlerUserData(void);
void CPL_DLL CPLErrorSetState(CPLErr eErrClass, CPLErrorNum err_no,
                              const char *pszMsg);
void CPL_DLL CPLCallPreviousHandler(CPLErr eErrClass, CPLErrorNum err_no,
                                    const char *pszMsg);
/*! @cond Doxygen_Suppress */
void CPL_DLL CPLCleanupErrorMutex(void);
/*! @endcond */

/** Callback for a custom error handler */
typedef void(CPL_STDCALL *CPLErrorHandler)(CPLErr, CPLErrorNum, const char *);

void CPL_DLL CPL_STDCALL CPLLoggingErrorHandler(CPLErr, CPLErrorNum,
                                                const char *);
void CPL_DLL CPL_STDCALL CPLDefaultErrorHandler(CPLErr, CPLErrorNum,
                                                const char *);
void CPL_DLL CPL_STDCALL CPLQuietErrorHandler(CPLErr, CPLErrorNum,
                                              const char *);
void CPLTurnFailureIntoWarning(int bOn);

CPLErrorHandler CPL_DLL CPLGetErrorHandler(void **ppUserData);

CPLErrorHandler CPL_DLL CPL_STDCALL CPLSetErrorHandler(CPLErrorHandler);
CPLErrorHandler CPL_DLL CPL_STDCALL CPLSetErrorHandlerEx(CPLErrorHandler,
                                                         void *);
void CPL_DLL CPL_STDCALL CPLPushErrorHandler(CPLErrorHandler);
void CPL_DLL CPL_STDCALL CPLPushErrorHandlerEx(CPLErrorHandler, void *);
void CPL_DLL CPL_STDCALL CPLSetCurrentErrorHandlerCatchDebug(int bCatchDebug);
void CPL_DLL CPL_STDCALL CPLPopErrorHandler(void);

#ifdef WITHOUT_CPLDEBUG
#define CPLDebug(...) /* Eat all CPLDebug calls. */
#else
void CPL_DLL CPLDebug(const char *, CPL_FORMAT_STRING(const char *), ...)
    CPL_PRINT_FUNC_FORMAT(2, 3);
#endif

#ifdef DEBUG
/** Same as CPLDebug(), but expands to nothing for non-DEBUG builds.
 * @since GDAL 3.1
 */
#define CPLDebugOnly(...) CPLDebug(__VA_ARGS__)
#else
/** Same as CPLDebug(), but expands to nothing for non-DEBUG builds.
 * @since GDAL 3.1
 */
#define CPLDebugOnly(...)
#endif

void CPL_DLL CPL_STDCALL _CPLAssert(const char *, const char *,
                                    int) CPL_NO_RETURN;

#if defined(DEBUG) && !defined(CPPCHECK)
/** Assert on an expression. Only enabled in DEBUG mode */
#define CPLAssert(expr)                                                        \
    ((expr) ? (void)(0) : _CPLAssert(#expr, __FILE__, __LINE__))
/** Assert on an expression in DEBUG mode. Evaluate it also in non-DEBUG mode
 * (useful to 'consume' a error return variable) */
#define CPLAssertAlwaysEval(expr) CPLAssert(expr)
#else
/** Assert on an expression. Only enabled in DEBUG mode */
#define CPLAssert(expr)
#ifdef __cplusplus
/** Assert on an expression in DEBUG mode. Evaluate it also in non-DEBUG mode
 * (useful to 'consume' a error return variable) */
#define CPLAssertAlwaysEval(expr) CPL_IGNORE_RET_VAL(expr)
#else
/** Assert on an expression in DEBUG mode. Evaluate it also in non-DEBUG mode
 * (useful to 'consume' a error return variable) */
#define CPLAssertAlwaysEval(expr) (void)(expr)
#endif
#endif

CPL_C_END

/*! @cond Doxygen_Suppress */
/*
 * Helper macros used for input parameters validation.
 */
#ifdef DEBUG
#define VALIDATE_POINTER_ERR CE_Fatal
#else
#define VALIDATE_POINTER_ERR CE_Failure
#endif

#if defined(__cplusplus) && !defined(CPL_SUPRESS_CPLUSPLUS) &&                 \
    !defined(DOXYGEN_SKIP)

extern "C++"
{
    template <class T> T *CPLAssertNotNull(T *x) CPL_RETURNS_NONNULL;
    template <class T> T *CPLAssertNotNull(T *x)
    {
        CPLAssert(x);
        return x;
    }

#include <string>

    class CPLErrorHandlerPusher
    {
      public:
        explicit CPLErrorHandlerPusher(CPLErrorHandler hHandler)
        {
            CPLPushErrorHandler(hHandler);
        }

        CPLErrorHandlerPusher(CPLErrorHandler hHandler, void *user_data)
        {
            CPLPushErrorHandlerEx(hHandler, user_data);
        }

        ~CPLErrorHandlerPusher()
        {
            CPLPopErrorHandler();
        }
    };

    class CPLErrorStateBackuper
    {
        CPLErrorNum m_nLastErrorNum;
        CPLErr m_nLastErrorType;
        std::string m_osLastErrorMsg;

      public:
        CPLErrorStateBackuper()
            : m_nLastErrorNum(CPLGetLastErrorNo()),
              m_nLastErrorType(CPLGetLastErrorType()),
              m_osLastErrorMsg(CPLGetLastErrorMsg())
        {
        }

        ~CPLErrorStateBackuper()
        {
            CPLErrorSetState(m_nLastErrorType, m_nLastErrorNum,
                             m_osLastErrorMsg.c_str());
        }
    };
}

#ifdef GDAL_COMPILATION
// internal only
bool CPLIsDefaultErrorHandlerAndCatchDebug();
#endif

#endif

/*! @endcond */

/** Validate that a pointer is not NULL */
#define VALIDATE_POINTER0(ptr, func)                                           \
    do                                                                         \
    {                                                                          \
        if (CPL_NULLPTR == ptr)                                                \
        {                                                                      \
            CPLErr const ret = VALIDATE_POINTER_ERR;                           \
            CPLError(ret, CPLE_ObjectNull,                                     \
                     "Pointer \'%s\' is NULL in \'%s\'.\n", #ptr, (func));     \
            return;                                                            \
        }                                                                      \
    } while (0)

/** Validate that a pointer is not NULL, and return rc if it is NULL */
#define VALIDATE_POINTER1(ptr, func, rc)                                       \
    do                                                                         \
    {                                                                          \
        if (CPL_NULLPTR == ptr)                                                \
        {                                                                      \
            CPLErr const ret = VALIDATE_POINTER_ERR;                           \
            CPLError(ret, CPLE_ObjectNull,                                     \
                     "Pointer \'%s\' is NULL in \'%s\'.\n", #ptr, (func));     \
            return (rc);                                                       \
        }                                                                      \
    } while (0)

#endif /* CPL_ERROR_H_INCLUDED */
