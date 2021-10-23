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
#include "cpl_http.h"

struct WMSHTTPRequest {
    WMSHTTPRequest()
        :options(nullptr), nStatus(0), pabyData(nullptr), nDataLen(0), nDataAlloc(0), m_curl_handle(nullptr), m_headers(nullptr), x(0), y(0) {}
    ~WMSHTTPRequest();

    /* Input */
    CPLString URL;
    // Not owned, do not release
    const char *const *options;
    CPLString Range;

    /* Output */
    CPLString ContentType;
    CPLString Error;

    int nStatus;  /* 200 = success, 404 = not found, 0 = no response / error */
    GByte *pabyData;
    size_t nDataLen;
    size_t nDataAlloc;

    /* curl internal stuff */
    CURL *m_curl_handle;
    struct curl_slist* m_headers;
    // Which tile is being requested
    int x, y;

    // Space for error message, doesn't seem to be used by the multi-request interface
    std::vector<char> m_curl_error;
};

// Not public, only for use within WMS
void WMSHTTPInitializeRequest(WMSHTTPRequest *psRequest);
CPLErr WMSHTTPFetchMulti(WMSHTTPRequest *psRequest, int nRequestCount = 1);

#endif /*  GDALHTTP_H */
