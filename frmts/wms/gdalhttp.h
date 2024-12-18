/*****************************************************************************
 *
 * Project:  WMS Client Driver
 * Purpose:  Implementation of Dataset and RasterBand classes for WMS
 *           and other similar services.
 * Author:   Adam Nowacki, nowak@xpam.de
 *
 ******************************************************************************
 * Copyright (c) 2007, Adam Nowacki
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#ifndef GDALHTTP_H
#define GDALHTTP_H

#include "cpl_port.h"
#include "cpl_http.h"

struct WMSHTTPRequest
{
    WMSHTTPRequest()
        : options(nullptr), nStatus(0), pabyData(nullptr), nDataLen(0),
          nDataAlloc(0), m_curl_handle(nullptr), m_headers(nullptr), x(0), y(0)
    {
    }

    ~WMSHTTPRequest();

    /* Input */
    CPLString URL;
    // Not owned, do not release
    const char *const *options;
    CPLString Range;

    /* Output */
    CPLString ContentType;
    CPLString Error;

    int nStatus; /* 200 = success, 404 = not found, 0 = no response / error */
    GByte *pabyData;
    size_t nDataLen;
    size_t nDataAlloc;

    /* curl internal stuff */
    CURL *m_curl_handle;
    struct curl_slist *m_headers;
    // Which tile is being requested
    int x, y;

    // Space for error message, doesn't seem to be used by the multi-request
    // interface
    std::vector<char> m_curl_error;
};

// Not public, only for use within WMS
void WMSHTTPInitializeRequest(WMSHTTPRequest *psRequest);
CPLErr WMSHTTPFetchMulti(WMSHTTPRequest *psRequest, int nRequestCount = 1);

#endif /*  GDALHTTP_H */
