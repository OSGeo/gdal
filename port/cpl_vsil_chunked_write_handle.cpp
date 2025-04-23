/******************************************************************************
 *
 * Project:  CPL - Common Portability Library
 * Purpose:  Implement a write-only file handle using PUT chunked writing
 * Author:   Even Rouault, even.rouault at spatialys.com
 *
 ******************************************************************************
 * Copyright (c) 2024, Even Rouault <even.rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include "cpl_vsil_curl_class.h"

#ifdef HAVE_CURL

//! @cond Doxygen_Suppress

#define unchecked_curl_easy_setopt(handle, opt, param)                         \
    CPL_IGNORE_RET_VAL(curl_easy_setopt(handle, opt, param))

namespace cpl
{

/************************************************************************/
/*                        VSIChunkedWriteHandle()                       */
/************************************************************************/

VSIChunkedWriteHandle::VSIChunkedWriteHandle(
    IVSIS3LikeFSHandler *poFS, const char *pszFilename,
    IVSIS3LikeHandleHelper *poS3HandleHelper, CSLConstList papszOptions)
    : m_poFS(poFS), m_osFilename(pszFilename),
      m_poS3HandleHelper(poS3HandleHelper), m_aosOptions(papszOptions),
      m_aosHTTPOptions(CPLHTTPGetOptionsFromEnv(pszFilename)),
      m_oRetryParameters(m_aosHTTPOptions)
{
}

/************************************************************************/
/*                      ~VSIChunkedWriteHandle()                        */
/************************************************************************/

VSIChunkedWriteHandle::~VSIChunkedWriteHandle()
{
    VSIChunkedWriteHandle::Close();
    delete m_poS3HandleHelper;

    if (m_hCurlMulti)
    {
        if (m_hCurl)
        {
            curl_multi_remove_handle(m_hCurlMulti, m_hCurl);
            curl_easy_cleanup(m_hCurl);
        }
        VSICURLMultiCleanup(m_hCurlMulti);
    }
    CPLFree(m_sWriteFuncHeaderData.pBuffer);
}

/************************************************************************/
/*                                 Close()                              */
/************************************************************************/

int VSIChunkedWriteHandle::Close()
{
    int nRet = 0;
    if (!m_bClosed)
    {
        m_bClosed = true;
        if (m_hCurlMulti != nullptr)
        {
            nRet = FinishChunkedTransfer();
        }
        else
        {
            if (!m_bError && !DoEmptyPUT())
                nRet = -1;
        }
    }
    return nRet;
}

/************************************************************************/
/*                    InvalidateParentDirectory()                       */
/************************************************************************/

void VSIChunkedWriteHandle::InvalidateParentDirectory()
{
    m_poFS->InvalidateCachedData(m_poS3HandleHelper->GetURL().c_str());

    std::string osFilenameWithoutSlash(m_osFilename);
    if (!osFilenameWithoutSlash.empty() && osFilenameWithoutSlash.back() == '/')
        osFilenameWithoutSlash.pop_back();
    m_poFS->InvalidateDirContent(
        CPLGetDirnameSafe(osFilenameWithoutSlash.c_str()));
}

/************************************************************************/
/*                               Seek()                                 */
/************************************************************************/

int VSIChunkedWriteHandle::Seek(vsi_l_offset nOffset, int nWhence)
{
    if (!((nWhence == SEEK_SET && nOffset == m_nCurOffset) ||
          (nWhence == SEEK_CUR && nOffset == 0) ||
          (nWhence == SEEK_END && nOffset == 0)))
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "Seek not supported on writable %s files",
                 m_poFS->GetFSPrefix().c_str());
        m_bError = true;
        return -1;
    }
    return 0;
}

/************************************************************************/
/*                               Tell()                                 */
/************************************************************************/

vsi_l_offset VSIChunkedWriteHandle::Tell()
{
    return m_nCurOffset;
}

/************************************************************************/
/*                               Read()                                 */
/************************************************************************/

size_t VSIChunkedWriteHandle::Read(void * /* pBuffer */, size_t /* nSize */,
                                   size_t /* nMemb */)
{
    CPLError(CE_Failure, CPLE_NotSupported,
             "Read not supported on writable %s files",
             m_poFS->GetFSPrefix().c_str());
    m_bError = true;
    return 0;
}

/************************************************************************/
/*                      ReadCallBackBufferChunked()                     */
/************************************************************************/

size_t VSIChunkedWriteHandle::ReadCallBackBufferChunked(char *buffer,
                                                        size_t size,
                                                        size_t nitems,
                                                        void *instream)
{
    VSIChunkedWriteHandle *poThis =
        static_cast<VSIChunkedWriteHandle *>(instream);
    if (poThis->m_nChunkedBufferSize == 0)
    {
        // CPLDebug("VSIChunkedWriteHandle", "Writing 0 byte (finish)");
        return 0;
    }
    const size_t nSizeMax = size * nitems;
    size_t nSizeToWrite = nSizeMax;
    size_t nChunkedBufferRemainingSize =
        poThis->m_nChunkedBufferSize - poThis->m_nChunkedBufferOff;
    if (nChunkedBufferRemainingSize < nSizeToWrite)
        nSizeToWrite = nChunkedBufferRemainingSize;
    memcpy(buffer,
           static_cast<const GByte *>(poThis->m_pBuffer) +
               poThis->m_nChunkedBufferOff,
           nSizeToWrite);
    poThis->m_nChunkedBufferOff += nSizeToWrite;
    // CPLDebug("VSIChunkedWriteHandle", "Writing %d bytes", nSizeToWrite);
    return nSizeToWrite;
}

/************************************************************************/
/*                               Write()                                */
/************************************************************************/

size_t VSIChunkedWriteHandle::Write(const void *pBuffer, size_t nSize,
                                    size_t nMemb)
{
    if (m_bError)
        return 0;

    const size_t nBytesToWrite = nSize * nMemb;
    if (nBytesToWrite == 0)
        return 0;

    if (m_hCurlMulti == nullptr)
    {
        m_hCurlMulti = curl_multi_init();
    }

    WriteFuncStruct sWriteFuncData;
    CPLHTTPRetryContext oRetryContext(m_oRetryParameters);
    // We can only easily retry at the first chunk of a transfer
    bool bCanRetry = (m_hCurl == nullptr);
    bool bRetry;
    do
    {
        bRetry = false;
        struct curl_slist *headers = nullptr;
        if (m_hCurl == nullptr)
        {
            CURL *hCurlHandle = curl_easy_init();
            unchecked_curl_easy_setopt(hCurlHandle, CURLOPT_UPLOAD, 1L);
            unchecked_curl_easy_setopt(hCurlHandle, CURLOPT_READFUNCTION,
                                       ReadCallBackBufferChunked);
            unchecked_curl_easy_setopt(hCurlHandle, CURLOPT_READDATA, this);

            VSICURLInitWriteFuncStruct(&sWriteFuncData, nullptr, nullptr,
                                       nullptr);
            unchecked_curl_easy_setopt(hCurlHandle, CURLOPT_WRITEDATA,
                                       &sWriteFuncData);
            unchecked_curl_easy_setopt(hCurlHandle, CURLOPT_WRITEFUNCTION,
                                       VSICurlHandleWriteFunc);

            VSICURLInitWriteFuncStruct(&m_sWriteFuncHeaderData, nullptr,
                                       nullptr, nullptr);
            unchecked_curl_easy_setopt(hCurlHandle, CURLOPT_HEADERDATA,
                                       &m_sWriteFuncHeaderData);
            unchecked_curl_easy_setopt(hCurlHandle, CURLOPT_HEADERFUNCTION,
                                       VSICurlHandleWriteFunc);

            headers = static_cast<struct curl_slist *>(CPLHTTPSetOptions(
                hCurlHandle, m_poS3HandleHelper->GetURL().c_str(),
                m_aosHTTPOptions.List()));
            headers = VSICurlSetCreationHeadersFromOptions(
                headers, m_aosOptions.List(), m_osFilename.c_str());
            headers = VSICurlMergeHeaders(
                headers, m_poS3HandleHelper->GetCurlHeaders("PUT", headers));
            unchecked_curl_easy_setopt(hCurlHandle, CURLOPT_HTTPHEADER,
                                       headers);

            m_osCurlErrBuf.resize(CURL_ERROR_SIZE + 1);
            unchecked_curl_easy_setopt(hCurlHandle, CURLOPT_ERRORBUFFER,
                                       &m_osCurlErrBuf[0]);

            curl_multi_add_handle(m_hCurlMulti, hCurlHandle);
            m_hCurl = hCurlHandle;
        }

        m_pBuffer = pBuffer;
        m_nChunkedBufferOff = 0;
        m_nChunkedBufferSize = nBytesToWrite;

        int repeats = 0;
        // cppcheck-suppress knownConditionTrueFalse
        while (m_nChunkedBufferOff < m_nChunkedBufferSize && !bRetry)
        {
            int still_running;

            memset(&m_osCurlErrBuf[0], 0, m_osCurlErrBuf.size());

            while (curl_multi_perform(m_hCurlMulti, &still_running) ==
                       CURLM_CALL_MULTI_PERFORM &&
                   // cppcheck-suppress knownConditionTrueFalse
                   m_nChunkedBufferOff < m_nChunkedBufferSize)
            {
                // loop
            }
            // cppcheck-suppress knownConditionTrueFalse
            if (!still_running || m_nChunkedBufferOff == m_nChunkedBufferSize)
                break;

            CURLMsg *msg;
            do
            {
                int msgq = 0;
                msg = curl_multi_info_read(m_hCurlMulti, &msgq);
                if (msg && (msg->msg == CURLMSG_DONE))
                {
                    CURL *e = msg->easy_handle;
                    if (e == m_hCurl)
                    {
                        long response_code;
                        curl_easy_getinfo(m_hCurl, CURLINFO_RESPONSE_CODE,
                                          &response_code);
                        if (response_code != 200 && response_code != 201)
                        {
                            // Look if we should attempt a retry
                            if (bCanRetry &&
                                oRetryContext.CanRetry(
                                    static_cast<int>(response_code),
                                    m_sWriteFuncHeaderData.pBuffer,
                                    m_osCurlErrBuf.c_str()))
                            {
                                CPLError(CE_Warning, CPLE_AppDefined,
                                         "HTTP error code: %d - %s. "
                                         "Retrying again in %.1f secs",
                                         static_cast<int>(response_code),
                                         m_poS3HandleHelper->GetURL().c_str(),
                                         oRetryContext.GetCurrentDelay());
                                CPLSleep(oRetryContext.GetCurrentDelay());
                                bRetry = true;
                            }
                            else if (sWriteFuncData.pBuffer != nullptr &&
                                     m_poS3HandleHelper->CanRestartOnError(
                                         sWriteFuncData.pBuffer,
                                         m_sWriteFuncHeaderData.pBuffer, false))
                            {
                                bRetry = true;
                            }
                            else
                            {
                                CPLError(CE_Failure, CPLE_AppDefined,
                                         "Error %d: %s",
                                         static_cast<int>(response_code),
                                         m_osCurlErrBuf.c_str());

                                curl_slist_free_all(headers);
                                bRetry = false;
                            }

                            curl_multi_remove_handle(m_hCurlMulti, m_hCurl);
                            curl_easy_cleanup(m_hCurl);

                            CPLFree(sWriteFuncData.pBuffer);
                            CPLFree(m_sWriteFuncHeaderData.pBuffer);

                            m_hCurl = nullptr;
                            sWriteFuncData.pBuffer = nullptr;
                            m_sWriteFuncHeaderData.pBuffer = nullptr;
                            if (!bRetry)
                                return 0;
                        }
                    }
                }
            } while (msg);

            CPLMultiPerformWait(m_hCurlMulti, repeats);
        }

        m_nWrittenInPUT += nBytesToWrite;

        curl_slist_free_all(headers);

        m_pBuffer = nullptr;

        if (!bRetry)
        {
            long response_code;
            curl_easy_getinfo(m_hCurl, CURLINFO_RESPONSE_CODE, &response_code);
            if (response_code != 100)
            {
                // Look if we should attempt a retry
                if (bCanRetry &&
                    oRetryContext.CanRetry(static_cast<int>(response_code),
                                           m_sWriteFuncHeaderData.pBuffer,
                                           m_osCurlErrBuf.c_str()))
                {
                    CPLError(CE_Warning, CPLE_AppDefined,
                             "HTTP error code: %d - %s. "
                             "Retrying again in %.1f secs",
                             static_cast<int>(response_code),
                             m_poS3HandleHelper->GetURL().c_str(),
                             oRetryContext.GetCurrentDelay());
                    CPLSleep(oRetryContext.GetCurrentDelay());
                    bRetry = true;
                }
                else if (sWriteFuncData.pBuffer != nullptr &&
                         m_poS3HandleHelper->CanRestartOnError(
                             sWriteFuncData.pBuffer,
                             m_sWriteFuncHeaderData.pBuffer, false))
                {
                    bRetry = true;
                }
                else
                {
                    CPLError(CE_Failure, CPLE_AppDefined, "Error %d: %s",
                             static_cast<int>(response_code),
                             m_osCurlErrBuf.c_str());
                    bRetry = false;
                    nMemb = 0;
                }

                curl_multi_remove_handle(m_hCurlMulti, m_hCurl);
                curl_easy_cleanup(m_hCurl);

                CPLFree(sWriteFuncData.pBuffer);
                CPLFree(m_sWriteFuncHeaderData.pBuffer);

                m_hCurl = nullptr;
                sWriteFuncData.pBuffer = nullptr;
                m_sWriteFuncHeaderData.pBuffer = nullptr;
            }
        }
    } while (bRetry);

    m_nCurOffset += nBytesToWrite;

    return nMemb;
}

/************************************************************************/
/*                        FinishChunkedTransfer()                       */
/************************************************************************/

int VSIChunkedWriteHandle::FinishChunkedTransfer()
{
    if (m_hCurl == nullptr)
        return -1;

    NetworkStatisticsFileSystem oContextFS(m_poFS->GetFSPrefix().c_str());
    NetworkStatisticsFile oContextFile(m_osFilename.c_str());
    NetworkStatisticsAction oContextAction("Write");

    NetworkStatisticsLogger::LogPUT(m_nWrittenInPUT);
    m_nWrittenInPUT = 0;

    m_pBuffer = nullptr;
    m_nChunkedBufferOff = 0;
    m_nChunkedBufferSize = 0;

    VSICURLMultiPerform(m_hCurlMulti);

    long response_code;
    curl_easy_getinfo(m_hCurl, CURLINFO_RESPONSE_CODE, &response_code);
    if (response_code == 200 || response_code == 201)
    {
        InvalidateParentDirectory();
    }
    else
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Error %d: %s",
                 static_cast<int>(response_code), m_osCurlErrBuf.c_str());
        return -1;
    }
    return 0;
}

/************************************************************************/
/*                            DoEmptyPUT()                              */
/************************************************************************/

bool VSIChunkedWriteHandle::DoEmptyPUT()
{
    bool bSuccess = true;
    bool bRetry;
    CPLHTTPRetryContext oRetryContext(m_oRetryParameters);

    NetworkStatisticsFileSystem oContextFS(m_poFS->GetFSPrefix().c_str());
    NetworkStatisticsFile oContextFile(m_osFilename.c_str());
    NetworkStatisticsAction oContextAction("Write");

    do
    {
        bRetry = false;

        PutData putData;
        putData.pabyData = nullptr;
        putData.nOff = 0;
        putData.nTotalSize = 0;

        CURL *hCurlHandle = curl_easy_init();
        unchecked_curl_easy_setopt(hCurlHandle, CURLOPT_UPLOAD, 1L);
        unchecked_curl_easy_setopt(hCurlHandle, CURLOPT_READFUNCTION,
                                   PutData::ReadCallBackBuffer);
        unchecked_curl_easy_setopt(hCurlHandle, CURLOPT_READDATA, &putData);
        unchecked_curl_easy_setopt(hCurlHandle, CURLOPT_INFILESIZE, 0);

        struct curl_slist *headers = static_cast<struct curl_slist *>(
            CPLHTTPSetOptions(hCurlHandle, m_poS3HandleHelper->GetURL().c_str(),
                              m_aosHTTPOptions.List()));
        headers = VSICurlSetCreationHeadersFromOptions(
            headers, m_aosOptions.List(), m_osFilename.c_str());
        headers = VSICurlMergeHeaders(
            headers, m_poS3HandleHelper->GetCurlHeaders("PUT", headers, "", 0));
        headers = curl_slist_append(headers, "Expect: 100-continue");

        CurlRequestHelper requestHelper;
        const long response_code = requestHelper.perform(
            hCurlHandle, headers, m_poFS, m_poS3HandleHelper);

        NetworkStatisticsLogger::LogPUT(0);

        if (response_code != 200 && response_code != 201)
        {
            // Look if we should attempt a retry
            if (oRetryContext.CanRetry(
                    static_cast<int>(response_code),
                    requestHelper.sWriteFuncHeaderData.pBuffer,
                    requestHelper.szCurlErrBuf))
            {
                CPLError(CE_Warning, CPLE_AppDefined,
                         "HTTP error code: %d - %s. "
                         "Retrying again in %.1f secs",
                         static_cast<int>(response_code),
                         m_poS3HandleHelper->GetURL().c_str(),
                         oRetryContext.GetCurrentDelay());
                CPLSleep(oRetryContext.GetCurrentDelay());
                bRetry = true;
            }
            else if (requestHelper.sWriteFuncData.pBuffer != nullptr &&
                     m_poS3HandleHelper->CanRestartOnError(
                         requestHelper.sWriteFuncData.pBuffer,
                         requestHelper.sWriteFuncHeaderData.pBuffer, false))
            {
                bRetry = true;
            }
            else
            {
                CPLDebug("S3", "%s",
                         requestHelper.sWriteFuncData.pBuffer
                             ? requestHelper.sWriteFuncData.pBuffer
                             : "(null)");
                CPLError(CE_Failure, CPLE_AppDefined,
                         "DoSinglePartPUT of %s failed", m_osFilename.c_str());
                bSuccess = false;
            }
        }
        else
        {
            InvalidateParentDirectory();
        }

        if (requestHelper.sWriteFuncHeaderData.pBuffer != nullptr)
        {
            const char *pzETag =
                strstr(requestHelper.sWriteFuncHeaderData.pBuffer, "ETag: \"");
            if (pzETag)
            {
                pzETag += strlen("ETag: \"");
                const char *pszEndOfETag = strchr(pzETag, '"');
                if (pszEndOfETag)
                {
                    FileProp oFileProp;
                    oFileProp.eExists = EXIST_YES;
                    oFileProp.fileSize = m_nBufferOff;
                    oFileProp.bHasComputedFileSize = true;
                    oFileProp.ETag.assign(pzETag, pszEndOfETag - pzETag);
                    m_poFS->SetCachedFileProp(
                        m_poFS->GetURLFromFilename(m_osFilename.c_str())
                            .c_str(),
                        oFileProp);
                }
            }
        }

        curl_easy_cleanup(hCurlHandle);
    } while (bRetry);
    return bSuccess;
}

}  // namespace cpl

//! @endcond

#endif  // HAVE_CURL
