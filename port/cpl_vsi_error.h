/******************************************************************************
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
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#ifndef CPL_VSI_ERROR_H_INCLUDED
#define CPL_VSI_ERROR_H_INCLUDED

#include "cpl_port.h"
#include "cpl_error.h"

/* ====================================================================
        Filesystem error codes.
   ==================================================================== */

CPL_C_START

typedef int VSIErrorNum;

#define VSIE_None 0
#define VSIE_FileError 1
#define VSIE_HttpError 2

#define VSIE_ObjectStorageGenericError 5
#define VSIE_AccessDenied 6
#define VSIE_BucketNotFound 7
#define VSIE_ObjectNotFound 8
#define VSIE_InvalidCredentials 9
#define VSIE_SignatureDoesNotMatch 10

/** Deprecated alias for VSIE_ObjectStorageGenericError
 *
 * @deprecated since 3.12
 */
#define VSIE_AWSError VSIE_ObjectStorageGenericError

/** Deprecated alias for VSIE_AccessDenied
 *
 * @deprecated since 3.12
 */
#define VSIE_AWSAccessDenied VSIE_AccessDenied

/** Deprecated alias for VSIE_BucketNotFound
 *
 * @deprecated since 3.12
 */
#define VSIE_AWSBucketNotFound VSIE_BucketNotFound

/** Deprecated alias for VSIE_ObjectNotFound
 *
 * @deprecated since 3.12
 */
#define VSIE_AWSObjectNotFound VSIE_ObjectNotFound

/** Deprecated alias for VSIE_InvalidCredentials
 *
 * @deprecated since 3.12
 */
#define VSIE_AWSInvalidCredentials VSIE_InvalidCredentials

/** Deprecated alias for VSIE_SignatureDoesNotMatch
 *
 * @deprecated since 3.12
 */
#define VSIE_AWSSignatureDoesNotMatch VSIE_SignatureDoesNotMatch

void CPL_DLL VSIError(VSIErrorNum err_no, CPL_FORMAT_STRING(const char *fmt),
                      ...) CPL_PRINT_FUNC_FORMAT(2, 3);

void CPL_DLL CPL_STDCALL VSIErrorReset(void);
VSIErrorNum CPL_DLL CPL_STDCALL VSIGetLastErrorNo(void);
const char CPL_DLL *CPL_STDCALL VSIGetLastErrorMsg(void);

const char *VSIErrorNumToString(int eErr);
int CPL_DLL CPL_STDCALL VSIToCPLError(CPLErr eErrClass,
                                      CPLErrorNum eDefaultErrorNo);
void CPL_DLL VSIToCPLErrorWithMsg(CPLErr eErrClass, CPLErrorNum eDefaultErrorNo,
                                  const char *pszMsg);

CPL_C_END

#endif /* CPL_VSI_ERROR_H_INCLUDED */
