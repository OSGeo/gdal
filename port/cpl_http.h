/******************************************************************************
 * $Id$
 *
 * Project:  Common Portability Library
 * Purpose:  Function wrapper for libcurl HTTP access.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2006, Frank Warmerdam
 * Copyright (c) 2009, Even Rouault <even dot rouault at spatialys.com>
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

#ifndef CPL_HTTP_H_INCLUDED
#define CPL_HTTP_H_INCLUDED

#include "cpl_conv.h"
#include "cpl_string.h"
#include "cpl_progress.h"
#include "cpl_vsi.h"

/**
 * \file cpl_http.h
 *
 * Interface for downloading HTTP, FTP documents
 */

/*! @cond Doxygen_Suppress */
#ifndef CPL_HTTP_MAX_RETRY
#define CPL_HTTP_MAX_RETRY 0
#endif

#ifndef CPL_HTTP_RETRY_DELAY
#define CPL_HTTP_RETRY_DELAY 30.0
#endif
/*! @endcond */

CPL_C_START

/*! Describe a part of a multipart message */
typedef struct
{
    /*! NULL terminated array of headers */ char **papszHeaders;

    /*! Buffer with data of the part     */ GByte *pabyData;
    /*! Buffer length                    */ int nDataLen;
} CPLMimePart;

/*! Describe the result of a CPLHTTPFetch() call */
typedef struct
{
    /*! cURL error code : 0=success, non-zero if request failed */
    int nStatus;

    /*! Content-Type of the response */
    char *pszContentType;

    /*! Error message from curl, or NULL */
    char *pszErrBuf;

    /*! Length of the pabyData buffer */
    int nDataLen;
    /*! Allocated size of the pabyData buffer */
    int nDataAlloc;

    /*! Buffer with downloaded data */
    GByte *pabyData;

    /*! Headers returned */
    char **papszHeaders;

    /*! Number of parts in a multipart message */
    int nMimePartCount;

    /*! Array of parts (resolved by CPLHTTPParseMultipartMime()) */
    CPLMimePart *pasMimePart;

} CPLHTTPResult;

/*! @cond Doxygen_Suppress */
typedef size_t (*CPLHTTPFetchWriteFunc)(void *pBuffer, size_t nSize,
                                        size_t nMemb, void *pWriteArg);
/*! @endcond */

int CPL_DLL CPLHTTPEnabled(void);
CPLHTTPResult CPL_DLL *CPLHTTPFetch(const char *pszURL,
                                    CSLConstList papszOptions);
CPLHTTPResult CPL_DLL *
CPLHTTPFetchEx(const char *pszURL, CSLConstList papszOptions,
               GDALProgressFunc pfnProgress, void *pProgressArg,
               CPLHTTPFetchWriteFunc pfnWrite, void *pWriteArg);
CPLHTTPResult CPL_DLL **CPLHTTPMultiFetch(const char *const *papszURL,
                                          int nURLCount, int nMaxSimultaneous,
                                          CSLConstList papszOptions);

void CPL_DLL CPLHTTPCleanup(void);
void CPL_DLL CPLHTTPDestroyResult(CPLHTTPResult *psResult);
void CPL_DLL CPLHTTPDestroyMultiResult(CPLHTTPResult **papsResults, int nCount);
int CPL_DLL CPLHTTPParseMultipartMime(CPLHTTPResult *psResult);

void CPL_DLL CPLHTTPSetDefaultUserAgent(const char *pszUserAgent);

/* -------------------------------------------------------------------- */
/* To install an alternate network layer to the default Curl one        */
/* -------------------------------------------------------------------- */
/** Callback function to process network requests.
 *
 * If CLOSE_PERSISTENT is found in papszOptions, no network request should be
 * issued, but a dummy non-null CPLHTTPResult* should be returned by the
 * callback.
 *
 * Its first arguments are the same as CPLHTTPFetchEx()
 * @param pszURL See CPLHTTPFetchEx()
 * @param papszOptions See CPLHTTPFetchEx()
 * @param pfnProgress See CPLHTTPFetchEx()
 * @param pProgressArg See CPLHTTPFetchEx()
 * @param pfnWrite See CPLHTTPFetchEx()
 * @param pWriteArg See CPLHTTPFetchEx()
 * @param pUserData user data value that was passed during
 * CPLHTTPPushFetchCallback()
 * @return nullptr if the request cannot be processed, in which case the
 * previous handler will be used.
 */
typedef CPLHTTPResult *(*CPLHTTPFetchCallbackFunc)(
    const char *pszURL, CSLConstList papszOptions, GDALProgressFunc pfnProgress,
    void *pProgressArg, CPLHTTPFetchWriteFunc pfnWrite, void *pWriteArg,
    void *pUserData);

void CPL_DLL CPLHTTPSetFetchCallback(CPLHTTPFetchCallbackFunc pFunc,
                                     void *pUserData);

int CPL_DLL CPLHTTPPushFetchCallback(CPLHTTPFetchCallbackFunc pFunc,
                                     void *pUserData);
int CPL_DLL CPLHTTPPopFetchCallback(void);

/* -------------------------------------------------------------------- */
/*      The following is related to OAuth2 authorization around         */
/*      google services like fusion tables, and potentially others      */
/*      in the future.  Code in cpl_google_oauth2.cpp.                  */
/*                                                                      */
/*      These services are built on CPL HTTP services.                  */
/* -------------------------------------------------------------------- */

char CPL_DLL *GOA2GetAuthorizationURL(const char *pszScope);
char CPL_DLL *GOA2GetRefreshToken(const char *pszAuthToken,
                                  const char *pszScope);
char CPL_DLL *GOA2GetAccessToken(const char *pszRefreshToken,
                                 const char *pszScope);

char CPL_DLL **GOA2GetAccessTokenFromServiceAccount(
    const char *pszPrivateKey, const char *pszClientEmail, const char *pszScope,
    CSLConstList papszAdditionalClaims, CSLConstList papszOptions);

char CPL_DLL **GOA2GetAccessTokenFromCloudEngineVM(CSLConstList papszOptions);

CPL_C_END

#if defined(__cplusplus) && !defined(CPL_SUPRESS_CPLUSPLUS)
/*! @cond Doxygen_Suppress */
// Not sure if this belong here, used in cpl_http.cpp, cpl_vsil_curl.cpp and
// frmts/wms/gdalhttp.cpp
void CPL_DLL *CPLHTTPSetOptions(void *pcurl, const char *pszURL,
                                const char *const *papszOptions);
char **CPLHTTPGetOptionsFromEnv(const char *pszFilename);
double CPLHTTPGetNewRetryDelay(int response_code, double dfOldDelay,
                               const char *pszErrBuf, const char *pszCurlError);
void CPL_DLL *CPLHTTPIgnoreSigPipe();
void CPL_DLL CPLHTTPRestoreSigPipeHandler(void *old_handler);
bool CPLMultiPerformWait(void *hCurlMultiHandle, int &repeats);
/*! @endcond */

bool CPL_DLL CPLIsMachinePotentiallyGCEInstance();
bool CPLIsMachineForSureGCEInstance();

/** Manager of Google OAuth2 authentication.
 *
 * This class handles different authentication methods and handles renewal
 * of access token.
 *
 * @since GDAL 2.3
 */
class GOA2Manager
{
  public:
    GOA2Manager();

    /** Authentication method */
    typedef enum
    {
        NONE,
        GCE,
        ACCESS_TOKEN_FROM_REFRESH,
        SERVICE_ACCOUNT
    } AuthMethod;

    bool SetAuthFromGCE(CSLConstList papszOptions);
    bool SetAuthFromRefreshToken(const char *pszRefreshToken,
                                 const char *pszClientId,
                                 const char *pszClientSecret,
                                 CSLConstList papszOptions);
    bool SetAuthFromServiceAccount(const char *pszPrivateKey,
                                   const char *pszClientEmail,
                                   const char *pszScope,
                                   CSLConstList papszAdditionalClaims,
                                   CSLConstList papszOptions);

    /** Returns the authentication method. */
    AuthMethod GetAuthMethod() const
    {
        return m_eMethod;
    }

    const char *GetBearer() const;

    /** Returns private key for SERVICE_ACCOUNT method */
    const CPLString &GetPrivateKey() const
    {
        return m_osPrivateKey;
    }

    /** Returns client email for SERVICE_ACCOUNT method */
    const CPLString &GetClientEmail() const
    {
        return m_osClientEmail;
    }

  private:
    mutable CPLString m_osCurrentBearer{};
    mutable time_t m_nExpirationTime = 0;
    AuthMethod m_eMethod = NONE;

    // for ACCESS_TOKEN_FROM_REFRESH
    CPLString m_osClientId{};
    CPLString m_osClientSecret{};
    CPLString m_osRefreshToken{};

    // for SERVICE_ACCOUNT
    CPLString m_osPrivateKey{};
    CPLString m_osClientEmail{};
    CPLString m_osScope{};
    CPLStringList m_aosAdditionalClaims{};

    CPLStringList m_aosOptions{};
};

#endif  // __cplusplus

#endif /* ndef CPL_HTTP_H_INCLUDED */
