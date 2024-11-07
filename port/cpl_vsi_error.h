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

#define VSIE_AWSError 5
#define VSIE_AWSAccessDenied 6
#define VSIE_AWSBucketNotFound 7
#define VSIE_AWSObjectNotFound 8
#define VSIE_AWSInvalidCredentials 9
#define VSIE_AWSSignatureDoesNotMatch 10

void CPL_DLL VSIError(VSIErrorNum err_no, CPL_FORMAT_STRING(const char *fmt),
                      ...) CPL_PRINT_FUNC_FORMAT(2, 3);

void CPL_DLL CPL_STDCALL VSIErrorReset(void);
VSIErrorNum CPL_DLL CPL_STDCALL VSIGetLastErrorNo(void);
const char CPL_DLL *CPL_STDCALL VSIGetLastErrorMsg(void);

int CPL_DLL CPL_STDCALL VSIToCPLError(CPLErr eErrClass,
                                      CPLErrorNum eDefaultErrorNo);
void CPL_DLL VSIToCPLErrorWithMsg(CPLErr eErrClass, CPLErrorNum eDefaultErrorNo,
                                  const char *pszMsg);

CPL_C_END

#endif /* CPL_VSI_ERROR_H_INCLUDED */
