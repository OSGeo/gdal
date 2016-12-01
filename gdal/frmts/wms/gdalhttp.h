/*****************************************************************************
 * $Id$
 *
 * Project:  WMS Client Driver
 * Purpose:  Implementation of Dataset and RasterBand classes for WMS
 *           and other similar services.
 * Author:   Adam Nowacki, nowak@xpam.de
 *
 ******************************************************************************
 * Copyright (c) 2007, Adam Nowacki
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

#ifndef GDALHTTP_H
#define GDALHTTP_H

#include "cpl_port.h"

typedef struct {
    /* Input */
    char *pszURL;
    char **papszOptions;

    /* Output */
    int nStatus;  /* 200 = success, 404 = not found, 0 = no response / error */
    char *pszContentType;
    char *pszError;

    GByte *pabyData;
    size_t nDataLen;
    size_t nDataAlloc;

    //    int nMimePartCount;
    //    CPLMimePart *pasMimePart;

    /* Internal stuff */
    CURL *m_curl_handle;
    struct curl_slist *m_headers;
    char *m_curl_error;
} CPLHTTPRequest;

void CPL_DLL CPLHTTPInitializeRequest(CPLHTTPRequest *psRequest, const char *pszURL = NULL, const char *const *papszOptions = NULL);
void CPL_DLL CPLHTTPCleanupRequest(CPLHTTPRequest *psRequest);
CPLErr CPL_DLL CPLHTTPFetchMulti(CPLHTTPRequest *pasRequest, int nRequestCount = 1, const char *const *papszOptions = NULL);

#endif /*  GDALHTTP_H */
