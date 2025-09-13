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
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include "wmsdriver.h"
#include <algorithm>
#include <vector>

static size_t WriteFunc(void *buffer, size_t count, size_t nmemb, void *req)
{
    WMSHTTPRequest *psRequest = reinterpret_cast<WMSHTTPRequest *>(req);
    size_t size = count * nmemb;

    if (size == 0)
        return 0;

    const size_t required_size = psRequest->nDataLen + size + 1;
    if (required_size > psRequest->nDataAlloc)
    {
        size_t new_size = required_size * 2;
        if (new_size < 512)
            new_size = 512;
        psRequest->nDataAlloc = new_size;
        GByte *pabyNewData = reinterpret_cast<GByte *>(
            VSIRealloc(psRequest->pabyData, new_size));
        if (pabyNewData == nullptr)
        {
            VSIFree(psRequest->pabyData);
            psRequest->pabyData = nullptr;
            psRequest->Error.Printf(
                "Out of memory allocating %u bytes for HTTP data buffer.",
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
static void ProcessCurlErrors(CURLMsg *msg, WMSHTTPRequest *psRequest)
{
    CPLAssert(msg != nullptr);
    CPLAssert(msg->msg == CURLMSG_DONE);

    // in case of local file error: update status code
    if (msg->data.result == CURLE_FILE_COULDNT_READ_FILE)
    {
        // sanity check for local files
        if (STARTS_WITH(psRequest->URL.c_str(), "file://"))
        {
            psRequest->nStatus = 404;
        }
    }
}

// Builds a curl request
static void WMSHTTPInitializeRequest(WMSHTTPRequest *psRequest, CURL *curl)
{
    psRequest->nStatus = 0;
    psRequest->pabyData = nullptr;
    psRequest->nDataLen = 0;
    psRequest->nDataAlloc = 0;
    psRequest->retry = 3;

    psRequest->m_curl_handle = curl;

    if (!psRequest->Range.empty())
    {
        CPL_IGNORE_RET_VAL(curl_easy_setopt(
            psRequest->m_curl_handle, CURLOPT_RANGE, psRequest->Range.c_str()));
    }

    CPL_IGNORE_RET_VAL(curl_easy_setopt(psRequest->m_curl_handle,
                                        CURLOPT_WRITEDATA, psRequest));
    CPL_IGNORE_RET_VAL(curl_easy_setopt(psRequest->m_curl_handle,
                                        CURLOPT_WRITEFUNCTION, WriteFunc));

    psRequest->m_curl_error.resize(CURL_ERROR_SIZE + 1);
    CPL_IGNORE_RET_VAL(curl_easy_setopt(psRequest->m_curl_handle,
                                        CURLOPT_ERRORBUFFER,
                                        &psRequest->m_curl_error[0]));

    psRequest->m_headers = static_cast<struct curl_slist *>(CPLHTTPSetOptions(
        psRequest->m_curl_handle, psRequest->URL.URLEncode().c_str(),
        psRequest->options));
    CPL_IGNORE_RET_VAL(curl_easy_setopt(
        psRequest->m_curl_handle, CURLOPT_HTTPHEADER, psRequest->m_headers));
    curl_easy_setopt(curl, CURLOPT_PRIVATE, psRequest);
}

WMSHTTPRequest::~WMSHTTPRequest()
{
    if (m_headers != nullptr)
        curl_slist_free_all(m_headers);
    if (pabyData != nullptr)
        CPLFree(pabyData);
}

static CURL *new_curl_handle(void)
{
    CURL *curl = curl_easy_init();
    if (curl == nullptr)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "CPLHTTPInitializeRequest(): Unable to create CURL handle.");
        return curl;
    }
    curl_easy_setopt(curl, CURLOPT_BUFFERSIZE, CURL_MAX_READ_SIZE);
    return curl;
}

static void process(WMSHTTPRequest *psRequest)
{
    long response_code;
    curl_easy_getinfo(psRequest->m_curl_handle, CURLINFO_RESPONSE_CODE,
                      &response_code);
    // for local files, don't update the status code if one is already set
    if (!(psRequest->nStatus != 0 &&
          STARTS_WITH(psRequest->URL.c_str(), "file://")))
        psRequest->nStatus = static_cast<int>(response_code);

    char *content_type = nullptr;
    curl_easy_getinfo(psRequest->m_curl_handle, CURLINFO_CONTENT_TYPE,
                      &content_type);
    psRequest->ContentType = content_type ? content_type : "";

    if (psRequest->Error.empty())
        psRequest->Error = &psRequest->m_curl_error[0];

    /* In the case of a file:// URL, curl will return a status == 0, so if
        * there's no */
    /* error returned, patch the status code to be 200, as it would be for
        * http:// */
    if (psRequest->nStatus == 0 && psRequest->Error.empty() &&
        STARTS_WITH(psRequest->URL.c_str(), "file://"))
        psRequest->nStatus = 200;

    // If there is an error with no error message, use the content if it is
    // text
    if (psRequest->Error.empty() && psRequest->nStatus != 0 &&
        psRequest->nStatus != 200 && strstr(psRequest->ContentType, "text") &&
        psRequest->pabyData != nullptr)
        psRequest->Error = reinterpret_cast<const char *>(psRequest->pabyData);

    CPLDebug("HTTP", "Request %s : status = %d, type = %s, error = %s",
             psRequest->URL.c_str(), psRequest->nStatus,
             !psRequest->ContentType.empty() ? psRequest->ContentType.c_str()
                                             : "(null)",
             !psRequest->Error.empty() ? psRequest->Error.c_str() : "(null)");
}

//
// Like CPLHTTPFetch, but multiple requests in parallel
// By default it uses 5 connections
//
CPLErr WMSHTTPFetchMulti(WMSHTTPRequest *pasRequest, int nRequestCount)
{
    CPLErr ret = CE_None;
    CURLM *curl_multi = nullptr;
    int max_conn;
    int conn_i;
    std::vector<CURL *> handles;

    CPLAssert(nRequestCount >= 0);
    if (nRequestCount == 0)
        return CE_None;

    const char *max_conn_opt =
        CSLFetchNameValue(const_cast<char **>(pasRequest->options), "MAXCONN");
    max_conn =
        (max_conn_opt == nullptr) ? 5 : MAX(1, MIN(atoi(max_conn_opt), 1000));

    // If the first url starts with vsimem, assume all do and defer to
    // CPLHTTPFetch
    if (STARTS_WITH(pasRequest[0].URL.c_str(), "/vsimem/") &&
        /* Disabled by default for potential security issues */
        CPLTestBool(CPLGetConfigOption("CPL_CURL_ENABLE_VSIMEM", "FALSE")))
    {
        for (int i = 0; i < nRequestCount; i++)
        {
            CPLHTTPResult *psResult =
                CPLHTTPFetch(pasRequest[i].URL.c_str(),
                             const_cast<char **>(pasRequest[i].options));
            pasRequest[i].pabyData = psResult->pabyData;
            pasRequest[i].nDataLen = psResult->nDataLen;
            pasRequest[i].Error =
                psResult->pszErrBuf ? psResult->pszErrBuf : "";
            // Conventions are different between this module and cpl_http...
            if (psResult->pszErrBuf != nullptr &&
                strcmp(psResult->pszErrBuf, "HTTP error code : 404") == 0)
                pasRequest[i].nStatus = 404;
            else
                pasRequest[i].nStatus = 200;
            pasRequest[i].ContentType =
                psResult->pszContentType ? psResult->pszContentType : "";
            // took ownership of content, we're done with the rest
            psResult->pabyData = nullptr;
            psResult->nDataLen = 0;
            CPLHTTPDestroyResult(psResult);
        }
        return CE_None;
    }

    curl_multi = curl_multi_init();
    if (curl_multi == nullptr)
    {
        CPLError(CE_Fatal, CPLE_AppDefined,
                 "CPLHTTPFetchMulti(): Unable to create CURL multi-handle.");
    }

    curl_multi_setopt(curl_multi, CURLMOPT_MAX_HOST_CONNECTIONS,
                      static_cast<long>(max_conn));
    curl_multi_setopt(curl_multi, CURLMOPT_MAXCONNECTS,
                      static_cast<long>(max_conn));
    curl_multi_setopt(curl_multi, CURLMOPT_MAX_TOTAL_CONNECTIONS,
                      static_cast<long>(max_conn));

    // add at most max_conn requests
    int torun = std::min(nRequestCount, max_conn);
    handles.reserve(torun);
    for (int i = 0; i < torun; i++)
    {
        CURL *curl = new_curl_handle();
        if (!curl)
        {
            CPLError(CE_Fatal, CPLE_AppDefined,
                     "CPLHTTPFetchMulti(): Unable to create CURL handle.");
        }
        handles.push_back(curl);
    }

    for (conn_i = 0; conn_i < torun; ++conn_i)
    {
        WMSHTTPRequest *const psRequest = &pasRequest[conn_i];
        WMSHTTPInitializeRequest(psRequest, handles[conn_i]);
        CPLDebug("HTTP", "Requesting [%d/%d] %s", conn_i + 1, nRequestCount,
                 pasRequest[conn_i].URL.c_str());
        curl_multi_add_handle(curl_multi, psRequest->m_curl_handle);
    }

    void *old_handler = CPLHTTPIgnoreSigPipe();
    int still_running;
    do
    {
        CURLMcode mc;
        mc = curl_multi_perform(curl_multi, &still_running);
        if (mc != CURLM_OK)
        {
            CPLError(CE_Fatal, CPLE_AppDefined, "curl_multi failed, code %d.\n",
                     mc);
        }

        if (still_running)
        {
            mc = curl_multi_poll(curl_multi, nullptr, 0, 1000, nullptr);
            if (mc != CURLM_OK)
            {
                CPLError(CE_Fatal, CPLE_AppDefined,
                         "curl_multi_poll failed, code %d.\n", mc);
            }
        }
        // Pick up messages, clean up the completed ones, add more
        int msgs_in_queue = 0;
        while (CURLMsg *m = curl_multi_info_read(curl_multi, &msgs_in_queue))
        {
            if (m->msg == CURLMSG_DONE)
            {
                auto handle = m->easy_handle;
                CURLcode result = m->data.result;
                WMSHTTPRequest *psRequest;
                curl_easy_getinfo(handle, CURLINFO_PRIVATE, &psRequest);

                curl_multi_remove_handle(curl_multi, handle);
                ProcessCurlErrors(m, psRequest);

                if (result != CURLE_OK)
                {
                    CPLError(CE_Failure, CPLE_AppDefined,
                             "CURL: Transfer failed: %s\n",
                             curl_easy_strerror(result));
                    curl_off_t wait = 0;
                    curl_easy_getinfo(handle, CURLINFO_RETRY_AFTER, &wait);
                    if (wait)
                    {
                        sleep(1);
                    }
                    if (--psRequest->retry > 0)
                    {
                        curl_multi_add_handle(curl_multi, handle);
                        still_running = 1;  // Still have request pending
                        continue;
                    }
                    ret = CE_Failure;
                }
                process(psRequest);

                if (conn_i < nRequestCount)
                {
                    psRequest = &pasRequest[conn_i];
                    WMSHTTPInitializeRequest(psRequest, handle);
                    CPLDebug("HTTP", "Requesting [%d/%d] %s", conn_i + 1,
                             nRequestCount, pasRequest[conn_i].URL.c_str());
                    curl_multi_add_handle(curl_multi, psRequest->m_curl_handle);
                    ++conn_i;
                    still_running = 1;  // Still have request pending
                }
            }
        }
    } while (still_running || conn_i != nRequestCount);

    CPLHTTPRestoreSigPipeHandler(old_handler);

    if (conn_i != nRequestCount)
    {  // something gone really really wrong
        // oddly built libcurl or perhaps absence of network interface
        CPLError(CE_Failure, CPLE_AppDefined,
                 "WMSHTTPFetchMulti(): conn_i != nRequestCount, this should "
                 "never happen ...");
        nRequestCount = conn_i;
        ret = CE_Failure;
    }

    curl_multi_cleanup(curl_multi);
    for (auto handle : handles)
    {
        curl_easy_cleanup(handle);
    }

    return ret;
}
