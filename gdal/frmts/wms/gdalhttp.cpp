/******************************************************************************
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

#include "stdinc.h"

void CPLHTTPSetOptions(CURL *http_handle, char** papszOptions);

/* CURLINFO_RESPONSE_CODE was known as CURLINFO_HTTP_CODE in libcurl 7.10.7 and earlier */
#if LIBCURL_VERSION_NUM < 0x070a07
#define CURLINFO_RESPONSE_CODE CURLINFO_HTTP_CODE
#endif

static size_t CPLHTTPWriteFunc(void *buffer, size_t count, size_t nmemb, void *req) {
    CPLHTTPRequest *psRequest = reinterpret_cast<CPLHTTPRequest *>(req);
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
            psRequest->pszError = CPLStrdup(CPLString().Printf("Out of memory allocating %u bytes for HTTP data buffer.", static_cast<int>(new_size)));
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

void CPLHTTPInitializeRequest(CPLHTTPRequest *psRequest, const char *pszURL, const char *const *papszOptions) {
    psRequest->pszURL = CPLStrdup(pszURL);
    psRequest->papszOptions = CSLDuplicate(const_cast<char **>(papszOptions));
    psRequest->nStatus = 0;
    psRequest->pszContentType = 0;
    psRequest->pszError = 0;
    psRequest->pabyData = 0;
    psRequest->nDataLen = 0;
    psRequest->nDataAlloc = 0;
    psRequest->m_curl_handle = 0;
    psRequest->m_headers = 0;
    psRequest->m_curl_error = 0;

    psRequest->m_curl_handle = curl_easy_init();
    if (psRequest->m_curl_handle == NULL) {
        CPLError(CE_Fatal, CPLE_AppDefined, "CPLHTTPInitializeRequest(): Unable to create CURL handle.");
    }

    char** papszOptionsDup = CSLDuplicate(const_cast<char **>(psRequest->papszOptions));

    /* Set User-Agent */
    const char *pszUserAgent = CSLFetchNameValue(papszOptionsDup, "USERAGENT");
    if (pszUserAgent == NULL)
        papszOptionsDup = CSLAddNameValue(papszOptionsDup, "USERAGENT",
                                          "GDAL WMS driver (http://www.gdal.org/frmt_wms.html)");

    /* Set URL */
    curl_easy_setopt(psRequest->m_curl_handle, CURLOPT_URL, psRequest->pszURL);

    /* Set Headers (copied&pasted from cpl_http.cpp, but unused by callers of CPLHTTPInitializeRequest) .*/
    const char *headers = CSLFetchNameValue(const_cast<char **>(psRequest->papszOptions), "HEADERS");
    if (headers != NULL) {
        psRequest->m_headers = curl_slist_append(psRequest->m_headers, headers);
        curl_easy_setopt(psRequest->m_curl_handle, CURLOPT_HTTPHEADER, psRequest->m_headers);
    }

    curl_easy_setopt(psRequest->m_curl_handle, CURLOPT_WRITEDATA, psRequest);
    curl_easy_setopt(psRequest->m_curl_handle, CURLOPT_WRITEFUNCTION, CPLHTTPWriteFunc);

    psRequest->m_curl_error = reinterpret_cast<char *>(CPLMalloc(CURL_ERROR_SIZE + 1));
    psRequest->m_curl_error[0] = '\0';
    curl_easy_setopt(psRequest->m_curl_handle, CURLOPT_ERRORBUFFER, psRequest->m_curl_error);
    
    CPLHTTPSetOptions(psRequest->m_curl_handle, papszOptionsDup);

    CSLDestroy(papszOptionsDup);
}

void CPLHTTPCleanupRequest(CPLHTTPRequest *psRequest) {
    if (psRequest->m_curl_handle) {
        curl_easy_cleanup(psRequest->m_curl_handle);
        psRequest->m_curl_handle = 0;
    }
    if (psRequest->m_headers) {
        curl_slist_free_all(psRequest->m_headers);
        psRequest->m_headers = 0;
    }
    if (psRequest->m_curl_error) {
        CPLFree(psRequest->m_curl_error);
        psRequest->m_curl_error = 0;
    }

    if (psRequest->pszContentType) {
        CPLFree(psRequest->pszContentType);
        psRequest->pszContentType = 0;
    }
    if (psRequest->pszError) {
        CPLFree(psRequest->pszError);
        psRequest->pszError = 0;
    }
    if (psRequest->pabyData) {
        CPLFree(psRequest->pabyData);
        psRequest->pabyData = 0;
        psRequest->nDataLen = 0;
        psRequest->nDataAlloc = 0;
    }
    if (psRequest->papszOptions) {
        CSLDestroy(psRequest->papszOptions);
        psRequest->papszOptions = 0;
    }
    if (psRequest->pszURL) {
        CPLFree(psRequest->pszURL);
        psRequest->pszURL = 0;
    }
}

CPLErr CPLHTTPFetchMulti(CPLHTTPRequest *pasRequest, int nRequestCount, const char *const *papszOptions) {
    CPLErr ret = CE_None;
    CURLM *curl_multi = 0;
    int still_running;
    int max_conn;
    int i, conn_i;

    const char *max_conn_opt = CSLFetchNameValue(const_cast<char **>(papszOptions), "MAXCONN");
    if (max_conn_opt && (max_conn_opt[0] != '\0')) {
        max_conn = MAX(1, MIN(atoi(max_conn_opt), 1000));
    } else {
        max_conn = 5;
    }

    curl_multi = curl_multi_init();
    if (curl_multi == NULL) {
        CPLError(CE_Fatal, CPLE_AppDefined, "CPLHTTPFetchMulti(): Unable to create CURL multi-handle.");
    }

    // add at most max_conn requests
    for (conn_i = 0; conn_i < MIN(nRequestCount, max_conn); ++conn_i) {
        CPLHTTPRequest *const psRequest = &pasRequest[conn_i];
        CPLDebug("HTTP", "Requesting [%d/%d] %s", conn_i + 1, nRequestCount, pasRequest[conn_i].pszURL);
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
                if (msg->msg == CURLMSG_DONE) { // transfer completed, check if we have more waiting and add them
                    if (conn_i < nRequestCount) {
                        CPLHTTPRequest *const psRequest = &pasRequest[conn_i];
                        CPLDebug("HTTP", "Requesting [%d/%d] %s", conn_i + 1, nRequestCount, pasRequest[conn_i].pszURL);
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
        timeout.tv_sec = 0;
        timeout.tv_usec = 100000;
        select(maxfd + 1, &fdread, &fdwrite, &fdexcep, &timeout);
        while (curl_multi_perform(curl_multi, &still_running) == CURLM_CALL_MULTI_PERFORM);
    }

    if (conn_i != nRequestCount) { // something gone really really wrong
        CPLError(CE_Fatal, CPLE_AppDefined, "CPLHTTPFetchMulti(): conn_i != nRequestCount, this should never happen ...");
    }
    for (i = 0; i < nRequestCount; ++i) {
        CPLHTTPRequest *const psRequest = &pasRequest[i];

        long response_code = 0;
        curl_easy_getinfo(psRequest->m_curl_handle, CURLINFO_RESPONSE_CODE, &response_code);
        psRequest->nStatus = response_code;

        char *content_type = 0;
        curl_easy_getinfo(psRequest->m_curl_handle, CURLINFO_CONTENT_TYPE, &content_type);
        if (content_type) psRequest->pszContentType = CPLStrdup(content_type);

        if ((psRequest->pszError == NULL) && (psRequest->m_curl_error != NULL) && (psRequest->m_curl_error[0] != '\0')) {
            psRequest->pszError = CPLStrdup(psRequest->m_curl_error);
        }

        /* In the case of a file:// URL, curl will return a status == 0, so if there's no */
        /* error returned, patch the status code to be 200, as it would be for http:// */
        if (strncmp(psRequest->pszURL, "file://", 7) == 0 && psRequest->nStatus == 0 &&
            psRequest->pszError == NULL)
        {
            psRequest->nStatus = 200;
        }

        CPLDebug("HTTP", "Request [%d] %s : status = %d, content type = %s, error = %s",
                 i, psRequest->pszURL, psRequest->nStatus,
                 (psRequest->pszContentType) ? psRequest->pszContentType : "(null)",
                 (psRequest->pszError) ? psRequest->pszError : "(null)");

        curl_multi_remove_handle(curl_multi, pasRequest[i].m_curl_handle);
    }
    curl_multi_cleanup(curl_multi);

    return ret;
}
