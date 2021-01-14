/******************************************************************************
 *
 * Project:  WMS Client Driver
 * Purpose:  Implementation of Dataset and RasterBand classes for WMS
 *           and other similar services.
 * Author:   Adam Nowacki, nowak@xpam.de
 *
 ******************************************************************************
 * Copyright (c) 2007, Adam Nowacki
 * Copyright (c) 2007-2013, Even Rouault <even dot rouault at spatialys.com>
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

#if LIBCURL_VERSION_NUM < 0x071c00
// Needed for curl_multi_wait()
#error Need libcurl version 7.28.0 or newer
// 7.28 was released in Oct 2012
#endif

static size_t WriteFunc(void *buffer, size_t count, size_t nmemb, void *req) {
    WMSHTTPRequest *psRequest = reinterpret_cast<WMSHTTPRequest *>(req);
    size_t size = count * nmemb;

    if (size == 0) return 0;

    const size_t required_size = psRequest->nDataLen + size + 1;
    if (required_size > psRequest->nDataAlloc) {
        size_t new_size = required_size * 2;
        if (new_size < 512) new_size = 512;
        psRequest->nDataAlloc = new_size;
        GByte * pabyNewData = reinterpret_cast<GByte *>(VSIRealloc(psRequest->pabyData, new_size));
        if (pabyNewData == nullptr) {
            VSIFree(psRequest->pabyData);
            psRequest->pabyData = nullptr;
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

// Process curl errors
static void ProcessCurlErrors(CURLMsg* msg, WMSHTTPRequest* pasRequest, int nRequestCount)
{
    CPLAssert(msg != nullptr);
    CPLAssert(msg->msg == CURLMSG_DONE);

    // in case of local file error: update status code
    if (msg->data.result == CURLE_FILE_COULDNT_READ_FILE) {
        // identify current request
        for (int current_req_i = 0; current_req_i < nRequestCount; ++current_req_i) {
            WMSHTTPRequest* const psRequest = &pasRequest[current_req_i];
            if (psRequest->m_curl_handle != msg->easy_handle)
                continue;

            // sanity check for local files
            if (STARTS_WITH(psRequest->URL.c_str(), "file://")) {
                psRequest->nStatus = 404;
                break;
            }
        }
    }
}

// Builds a curl request
void WMSHTTPInitializeRequest(WMSHTTPRequest *psRequest) {
    psRequest->nStatus = 0;
    psRequest->pabyData = nullptr;
    psRequest->nDataLen = 0;
    psRequest->nDataAlloc = 0;

    psRequest->m_curl_handle = curl_easy_init();
    if (psRequest->m_curl_handle == nullptr) {
        CPLError(CE_Fatal, CPLE_AppDefined, "CPLHTTPInitializeRequest(): Unable to create CURL handle.");
        // This should return somehow?
    }

    if (!psRequest->Range.empty())
        curl_easy_setopt(psRequest->m_curl_handle, CURLOPT_RANGE, psRequest->Range.c_str());

    curl_easy_setopt(psRequest->m_curl_handle, CURLOPT_WRITEDATA, psRequest);
    curl_easy_setopt(psRequest->m_curl_handle, CURLOPT_WRITEFUNCTION, WriteFunc);

    psRequest->m_curl_error.resize(CURL_ERROR_SIZE + 1);
    curl_easy_setopt(psRequest->m_curl_handle, CURLOPT_ERRORBUFFER, &psRequest->m_curl_error[0]);

    psRequest->m_headers = static_cast<struct curl_slist*>(
            CPLHTTPSetOptions(psRequest->m_curl_handle, psRequest->URL.c_str(), psRequest->options));
    const char* pszAccept = CSLFetchNameValue(psRequest->options, "ACCEPT");
    if( pszAccept )
    {
        psRequest->m_headers = curl_slist_append(psRequest->m_headers,
                                        CPLSPrintf("Accept: %s", pszAccept));
    }
    if( psRequest->m_headers != nullptr )
        curl_easy_setopt(psRequest->m_curl_handle, CURLOPT_HTTPHEADER,
                         psRequest->m_headers);

}

WMSHTTPRequest::~WMSHTTPRequest() {
    if (m_curl_handle != nullptr)
        curl_easy_cleanup(m_curl_handle);
    if( m_headers != nullptr )
        curl_slist_free_all(m_headers);
    if (pabyData != nullptr)
        CPLFree(pabyData);
}

//
// Like CPLHTTPFetch, but multiple requests in parallel
// By default it uses 5 connections
//
CPLErr WMSHTTPFetchMulti(WMSHTTPRequest *pasRequest, int nRequestCount) {
    CPLErr ret = CE_None;
    CURLM *curl_multi = nullptr;
    int max_conn;
    int i, conn_i;

    CPLAssert(nRequestCount >= 0);
    if (nRequestCount == 0)
        return CE_None;

    const char *max_conn_opt = CSLFetchNameValue(const_cast<char **>(pasRequest->options), "MAXCONN");
    max_conn = (max_conn_opt == nullptr) ? 5 : MAX(1, MIN(atoi(max_conn_opt), 1000));

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
            if( psResult->pszErrBuf != nullptr &&
                strcmp(psResult->pszErrBuf, "HTTP error code : 404") == 0 )
                pasRequest[i].nStatus = 404;
            else
                pasRequest[i].nStatus = 200;
            pasRequest[i].ContentType = psResult->pszContentType ? psResult->pszContentType : "";
            // took ownership of content, we're done with the rest
            psResult->pabyData = nullptr;
            psResult->nDataLen = 0;
            CPLHTTPDestroyResult(psResult);
        }
        return CE_None;
    }

    curl_multi = curl_multi_init();
    if (curl_multi == nullptr) {
        CPLError(CE_Fatal, CPLE_AppDefined, "CPLHTTPFetchMulti(): Unable to create CURL multi-handle.");
    }

    // add at most max_conn requests
    int torun = std::min(nRequestCount, max_conn);
    for (conn_i = 0; conn_i < torun; ++conn_i) {
        WMSHTTPRequest *const psRequest = &pasRequest[conn_i];
        CPLDebug("HTTP", "Requesting [%d/%d] %s", conn_i + 1, nRequestCount,
            pasRequest[conn_i].URL.c_str());
        curl_multi_add_handle(curl_multi, psRequest->m_curl_handle);
    }

    void* old_handler = CPLHTTPIgnoreSigPipe();
    int still_running;
    do {
        CURLMcode mc;
        do {
            mc = curl_multi_perform(curl_multi, &still_running);
        } while (CURLM_CALL_MULTI_PERFORM == mc);

        // Pick up messages, clean up the completed ones, add more
        int msgs_in_queue = 0;
        do {
            CURLMsg *m = curl_multi_info_read(curl_multi, &msgs_in_queue);
            if (m && (m->msg == CURLMSG_DONE)) {
                ProcessCurlErrors(m, pasRequest, nRequestCount);

                curl_multi_remove_handle(curl_multi, m->easy_handle);
                if (conn_i < nRequestCount) {
                    auto psRequest = &pasRequest[conn_i];
                    CPLDebug("HTTP", "Requesting [%d/%d] %s", conn_i + 1,
                        nRequestCount, pasRequest[conn_i].URL.c_str());
                    curl_multi_add_handle(curl_multi, psRequest->m_curl_handle);
                    ++conn_i;
                    still_running = 1; // Still have request pending
                }
            }
        } while (msgs_in_queue);

        if (CURLM_OK == mc) {
            int numfds;
            curl_multi_wait(curl_multi, nullptr, 0, 100, &numfds);
        }
    } while (still_running || conn_i != nRequestCount);

    // process any message still in queue
    CURLMsg* msg;
    int msgs_in_queue;
    do {
        msg = curl_multi_info_read(curl_multi, &msgs_in_queue);
        if (msg != nullptr) {
            if (msg->msg == CURLMSG_DONE) {
                ProcessCurlErrors(msg, pasRequest, nRequestCount);
            }
        }
    } while (msg != nullptr);

    CPLHTTPRestoreSigPipeHandler(old_handler);

    if (conn_i != nRequestCount) { // something gone really really wrong
        // oddly built libcurl or perhaps absence of network interface
        CPLError(CE_Failure, CPLE_AppDefined,
                 "WMSHTTPFetchMulti(): conn_i != nRequestCount, this should never happen ...");
        nRequestCount = conn_i;
        ret = CE_Failure;
    }

    for (i = 0; i < nRequestCount; ++i) {
        WMSHTTPRequest *const psRequest = &pasRequest[i];

        long response_code;
        curl_easy_getinfo(psRequest->m_curl_handle, CURLINFO_RESPONSE_CODE, &response_code);
        // for local files, don't update the status code if one is already set
        if(!(psRequest->nStatus != 0 && STARTS_WITH(psRequest->URL.c_str(), "file://")))
            psRequest->nStatus = static_cast<int>(response_code);

        char *content_type = nullptr;
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
            && psRequest->pabyData != nullptr )
            psRequest->Error = reinterpret_cast<const char *>(psRequest->pabyData);

        CPLDebug("HTTP", "Request [%d] %s : status = %d, type = %s, error = %s",
                 i, psRequest->URL.c_str(), psRequest->nStatus,
                 !psRequest->ContentType.empty() ? psRequest->ContentType.c_str() : "(null)",
                 !psRequest->Error.empty() ? psRequest->Error.c_str() : "(null)");

        curl_multi_remove_handle(curl_multi, pasRequest->m_curl_handle);
    }

    curl_multi_cleanup(curl_multi);

    return ret;
}
