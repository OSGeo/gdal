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

CPL_C_END

#endif /* CPL_VSI_ERROR_H_INCLUDED */
