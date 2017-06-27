/******************************************************************************
 *
 * Project:  WMS Client Driver
 * Purpose:  Implementation of Dataset and RasterBand classes for WMS
 *           and other similar services.
 * Author:   Adam Nowacki, nowak@xpam.de
 *
 ******************************************************************************
 * Copyright (c) 2007, Adam Nowacki
 * Copyright (c) 2007-2013, Even Rouault <even dot rouault at mines-paris dot org>
 * Copyright (c) 2016, Lucian Plesea
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

#include "wmsdriver.h"
#include <algorithm>

CPL_CVSID("$Id$")

/* CURLINFO_RESPONSE_CODE was known as CURLINFO_HTTP_CODE in libcurl 7.10.7 and earlier */
#if LIBCURL_VERSION_NUM < 0x070a07
#define CURLINFO_RESPONSE_CODE CURLINFO_HTTP_CODE
#endif

static size_t CPLHTTPWriteFunc(void *buffer, size_t count, size_t nmemb, void *req) {
    WMSHTTPRequest *psRequest = reinterpret_cast<WMSHTTPRequest *>(req);
    size_t size = count * nmemb;

    if (size == 0) return 0;

    const size_t required_size = psRequest->nDataLen + size + 1;
    if (required_size > psRequest->nDataAlloc) {
        size_t new_size = required_size * 2;
        if (new_size < 512) new_size = 512;
        psRequest->nDataAlloc = new_size;
        GByte * pabyNewData = reinterpret_cast<GByte *>(VSIRealloc(psRequest->pabyData, new_size));
        if (pabyNewData == NULL) {
            VSIFree(psRequest->pabyData);
            psRequest->pabyData = NULL;
            psRequest->Error.Printf("Out of memory allocating %u bytes for HTTP data buffer.",
                static_cast<unsigned int>(new_size));
            psRequest->nDataAlloc = 0;
            psRequest->nDataLen = 0;
            return 0;
        }
        psRequest->pabyData = pabyNewData;
    }
    memcpy(psRequest->pabyData + psRequest->nDataLen, buffer, size);
    psRequest->nDataLen += size;
    psRequest->pabyData[psRequest->nDataLen] = 0;
    return nmemb;
}

// Builds a curl request
void WMSHTTPInitializeRequest(WMSHTTPRequest *psRequest) {
    psRequest->nStatus = 0;
    psRequest->pabyData = NULL;
    psRequest->nDataLen = 0;
    psRequest->nDataAlloc = 0;

    psRequest->m_curl_handle = curl_easy_init();
    if (psRequest->m_curl_handle == NULL) {
        CPLError(CE_Fatal, CPLE_AppDefined, "CPLHTTPInitializeRequest(): Unable to create CURL handle.");
        // This should return somehow?
    }

    if (!psRequest->Range.empty())
        curl_easy_setopt(psRequest->m_curl_handle, CURLOPT_RANGE, psRequest->Range.c_str());

    curl_easy_setopt(psRequest->m_curl_handle, CURLOPT_URL, psRequest->URL.c_str());
    curl_easy_setopt(psRequest->m_curl_handle, CURLOPT_WRITEDATA, psRequest);
    curl_easy_setopt(psRequest->m_curl_handle, CURLOPT_WRITEFUNCTION, CPLHTTPWriteFunc);

    psRequest->m_curl_error.resize(CURL_ERROR_SIZE + 1);
    curl_easy_setopt(psRequest->m_curl_handle, CURLOPT_ERRORBUFFER, &psRequest->m_curl_error[0]);

    psRequest->m_headers = static_cast<struct curl_slist*>(
            CPLHTTPSetOptions(psRequest->m_curl_handle, psRequest->options));
    if( psRequest->m_headers != NULL )
        curl_easy_setopt(psRequest->m_curl_handle, CURLOPT_HTTPHEADER,
                         psRequest->m_headers);

}

WMSHTTPRequest::~WMSHTTPRequest() {
    if (m_curl_handle != NULL)
        curl_easy_cleanup(m_curl_handle);
    if( m_headers != NULL )
        curl_slist_free_all(m_headers);
    if (pabyData != NULL)
        CPLFree(pabyData);
}

//
// Like CPLHTTPFetch, but multiple requests in parallel
// By default it uses 5 connections
//
CPLErr WMSHTTPFetchMulti(WMSHTTPRequest *pasRequest, int nRequestCount) {
    CPLErr ret = CE_None;
    CURLM *curl_multi = NULL;
    int still_running;
    int max_conn;
    int i, conn_i;

    CPLAssert(nRequestCount >= 0);
    if (nRequestCount == 0)
        return CE_None;

    const char *max_conn_opt = CSLFetchNameValue(const_cast<char **>(pasRequest->options), "MAXCONN");
    max_conn = (max_conn_opt == NULL) ? 5 : MAX(1, MIN(atoi(max_conn_opt), 1000));

    // If the first url starts with vsimem, assume all do and defer to CPLHTTPFetch
    if( STARTS_WITH(pasRequest[0].URL.c_str(), "/vsimem/") &&
        /* Disabled by default for potential security issues */
        CPLTestBool(CPLGetConfigOption("CPL_CURL_ENABLE_VSIMEM", "FALSE")) )
    {
        for(i = 0; i< nRequestCount;i++)
        {
            CPLHTTPResult* psResult = CPLHTTPFetch(pasRequest[i].URL.c_str(),
                                                    const_cast<char**>(pasRequest[i].options));
            pasRequest[i].pabyData = psResult->pabyData;
            pasRequest[i].nDataLen = psResult->nDataLen;
            pasRequest[i].Error = psResult->pszErrBuf ? psResult->pszErrBuf : "";
            // Conventions are different between this module and cpl_http...
            if( psResult->pszErrBuf != NULL &&
                strcmp(psResult->pszErrBuf, "HTTP error code : 404") == 0 )
                pasRequest[i].nStatus = 404;
            else
                pasRequest[i].nStatus = 200;
            pasRequest[i].ContentType = psResult->pszContentType ? psResult->pszContentType : "";
            // took ownership of content, we're done with the rest
            psResult->pabyData = NULL;
            psResult->nDataLen = 0;
            CPLHTTPDestroyResult(psResult);
        }
        return CE_None;
    }

    curl_multi = curl_multi_init();
    if (curl_multi == NULL) {
        CPLError(CE_Fatal, CPLE_AppDefined, "CPLHTTPFetchMulti(): Unable to create CURL multi-handle.");
    }

    // add at most max_conn requests
    for (conn_i = 0; conn_i < std::min(nRequestCount, max_conn); ++conn_i) {
        WMSHTTPRequest *const psRequest = &pasRequest[conn_i];
        CPLDebug("HTTP", "Requesting [%d/%d] %s", conn_i + 1, nRequestCount,
            pasRequest[conn_i].URL.c_str());
        curl_multi_add_handle(curl_multi, psRequest->m_curl_handle);
    }

    while (curl_multi_perform(curl_multi, &still_running) == CURLM_CALL_MULTI_PERFORM);

    while (still_running || (conn_i != nRequestCount)) {
        struct timeval timeout;
        fd_set fdread, fdwrite, fdexcep;
        int maxfd;
        CURLMsg *msg;
        int msgs_in_queue;

        do {
            msg = curl_multi_info_read(curl_multi, &msgs_in_queue);
            if (msg != NULL) {
                if (msg->msg == CURLMSG_DONE) {
                    // transfer completed, add more handles if available
                    if (conn_i < nRequestCount) {
                        WMSHTTPRequest *const psRequest = &pasRequest[conn_i];
                        CPLDebug("HTTP", "Requesting [%d/%d] %s", conn_i + 1,
                                    nRequestCount, pasRequest[conn_i].URL.c_str());
                        curl_multi_add_handle(curl_multi, psRequest->m_curl_handle);
                        ++conn_i;
                    }
                }
            }
        } while (msg != NULL);

        FD_ZERO(&fdread);
        FD_ZERO(&fdwrite);
        FD_ZERO(&fdexcep);
        curl_multi_fdset(curl_multi, &fdread, &fdwrite, &fdexcep, &maxfd);
        if( maxfd >= 0 )
        {
            timeout.tv_sec = 0;
            timeout.tv_usec = 100000;
            if( select(maxfd + 1, &fdread, &fdwrite, &fdexcep, &timeout) < 0 )
            {
                CPLError(CE_Failure, CPLE_AppDefined, "select() failed");
                break;
            }
        }

#ifdef FUZZING_BUILD_MODE_UNSAFE_FOR_PRODUCTION
        if( CPLTestBool(CPLGetConfigOption("GDAL_WMS_ABORT_CURL_REQUEST",  "NO")) )
        {
            // oss-fuzz has no network interface and apparently this causes
            // endless loop here. There might be a better/more general way of
            // detecting this, and avoid this oss-fuzz specific trick, but
            // for now that's good enough.
            break;
        }
#endif

        while (curl_multi_perform(curl_multi, &still_running) == CURLM_CALL_MULTI_PERFORM);
    }

    if (conn_i != nRequestCount) { // something gone really really wrong
        // oddly built libcurl or perhaps absence of network interface
        CPLError(CE_Failure, CPLE_AppDefined,
                 "CPLHTTPFetchMulti(): conn_i != nRequestCount, this should never happen ...");
        nRequestCount = conn_i;
        ret = CE_Failure;
    }

    for (i = 0; i < nRequestCount; ++i) {
        WMSHTTPRequest *const psRequest = &pasRequest[i];

        long response_code;
        curl_easy_getinfo(psRequest->m_curl_handle, CURLINFO_RESPONSE_CODE, &response_code);
        psRequest->nStatus = static_cast<int>(response_code);

        char *content_type = NULL;
        curl_easy_getinfo(psRequest->m_curl_handle, CURLINFO_CONTENT_TYPE, &content_type);
        psRequest->ContentType = content_type ? content_type : "";

        if (psRequest->Error.empty())
            psRequest->Error = &psRequest->m_curl_error[0];

        /* In the case of a file:// URL, curl will return a status == 0, so if there's no */
        /* error returned, patch the status code to be 200, as it would be for http:// */
        if (psRequest->nStatus == 0 && psRequest->Error.empty() && STARTS_WITH(psRequest->URL.c_str(), "file://"))
            psRequest->nStatus = 200;

        // If there is an error with no error message, use the content if it is text
        if (psRequest->Error.empty()
            && psRequest->nStatus != 0
            && psRequest->nStatus != 200
            && strstr(psRequest->ContentType, "text")
            && psRequest->pabyData != NULL )
            psRequest->Error = reinterpret_cast<const char *>(psRequest->pabyData);

        CPLDebug("HTTP", "Request [%d] %s : status = %d, content type = %s, error = %s",
                 i, psRequest->URL.c_str(), psRequest->nStatus,
                 !psRequest->ContentType.empty() ? psRequest->ContentType.c_str() : "(null)",
                 !psRequest->Error.empty() ? psRequest->Error.c_str() : "(null)");

        curl_multi_remove_handle(curl_multi, pasRequest->m_curl_handle);
    }

    curl_multi_cleanup(curl_multi);

    return ret;
}
