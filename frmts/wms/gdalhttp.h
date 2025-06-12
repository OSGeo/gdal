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
    WMSHTTPRequest() = default;

    ~WMSHTTPRequest();
    WMSHTTPRequest(const WMSHTTPRequest &) = delete;
    WMSHTTPRequest &operator=(const WMSHTTPRequest &) = delete;
    WMSHTTPRequest(WMSHTTPRequest &&) = delete;
    WMSHTTPRequest &operator=(WMSHTTPRequest &&) = delete;

    /* Input */
    CPLString URL{};
    // Not owned, do not release
    const char *const *options{nullptr};
    CPLString Range{};

    /* Output */
    CPLString ContentType{};
    CPLString Error{};

    int nStatus =
        0; /* 200 = success, 404 = not found, 0 = no response / error */
    GByte *pabyData{nullptr};
    size_t nDataLen{};
    size_t nDataAlloc{};

    /* curl internal stuff */
    CURL *m_curl_handle{nullptr};
    struct curl_slist *m_headers{nullptr};
    // Which tile is being requested
    int x{}, y{};

    // Space for error message, doesn't seem to be used by the multi-request
    // interface
    std::vector<char> m_curl_error{};
};

// Not public, only for use within WMS
void WMSHTTPInitializeRequest(WMSHTTPRequest *psRequest);
CPLErr WMSHTTPFetchMulti(WMSHTTPRequest *psRequest, int nRequestCount = 1);

#endif /*  GDALHTTP_H */
