/******************************************************************************
 *
 * Project:  CPL - Common Portability Library
 * Purpose:  Implement VSI large file api for Alibaba Object Storage Service
 * Author:   Even Rouault, even.rouault at spatialys.com
 *
 ******************************************************************************
 * Copyright (c) 2017-2018, Even Rouault <even.rouault at spatialys.com>
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

#include "cpl_port.h"
#include "cpl_http.h"
#include "cpl_minixml.h"
#include "cpl_vsil_curl_priv.h"
#include "cpl_vsil_curl_class.h"

#include <errno.h>

#include <algorithm>
#include <set>
#include <map>
#include <memory>

#include "cpl_alibaba_oss.h"

#ifndef HAVE_CURL

void VSIInstallOSSFileHandler(void)
{
    // Not supported
}

#else

//! @cond Doxygen_Suppress
#ifndef DOXYGEN_SKIP

#define ENABLE_DEBUG 0

namespace cpl
{

/************************************************************************/
/*                         VSIOSSFSHandler                              */
/************************************************************************/

class VSIOSSFSHandler final : public IVSIS3LikeFSHandler
{
    CPL_DISALLOW_COPY_ASSIGN(VSIOSSFSHandler)

  protected:
    VSICurlHandle *CreateFileHandle(const char *pszFilename) override;
    std::string GetURLFromFilename(const std::string &osFilename) override;

    const char *GetDebugKey() const override
    {
        return "OSS";
    }

    IVSIS3LikeHandleHelper *CreateHandleHelper(const char *pszURI,
                                               bool bAllowNoObject) override;

    std::string GetFSPrefix() const override
    {
        return "/vsioss/";
    }

    void ClearCache() override;

    VSIVirtualHandleUniquePtr
    CreateWriteHandle(const char *pszFilename,
                      CSLConstList papszOptions) override;

  public:
    VSIOSSFSHandler() = default;
    ~VSIOSSFSHandler() override;

    const char *GetOptions() override;

    char *GetSignedURL(const char *pszFilename,
                       CSLConstList papszOptions) override;

    std::string
    GetStreamingFilename(const std::string &osFilename) const override
    {
        return osFilename;
    }
};

/************************************************************************/
/*                            VSIOSSHandle                              */
/************************************************************************/

class VSIOSSHandle final : public IVSIS3LikeHandle
{
    CPL_DISALLOW_COPY_ASSIGN(VSIOSSHandle)

    VSIOSSHandleHelper *m_poHandleHelper = nullptr;

  protected:
    struct curl_slist *
    GetCurlHeaders(const std::string &osVerb,
                   const struct curl_slist *psExistingHeaders) override;
    bool CanRestartOnError(const char *, const char *, bool) override;

  public:
    VSIOSSHandle(VSIOSSFSHandler *poFS, const char *pszFilename,
                 VSIOSSHandleHelper *poHandleHelper);
    ~VSIOSSHandle() override;
};

/************************************************************************/
/*                       ~VSIOSSFSHandler()                             */
/************************************************************************/

VSIOSSFSHandler::~VSIOSSFSHandler()
{
    VSIOSSFSHandler::ClearCache();
}

/************************************************************************/
/*                          CreateWriteHandle()                         */
/************************************************************************/

VSIVirtualHandleUniquePtr
VSIOSSFSHandler::CreateWriteHandle(const char *pszFilename,
                                   CSLConstList papszOptions)
{
    auto poHandleHelper =
        CreateHandleHelper(pszFilename + GetFSPrefix().size(), false);
    if (poHandleHelper == nullptr)
        return nullptr;
    auto poHandle = std::make_unique<VSIS3WriteHandle>(
        this, pszFilename, poHandleHelper, false, papszOptions);
    if (!poHandle->IsOK())
    {
        return nullptr;
    }
    return VSIVirtualHandleUniquePtr(poHandle.release());
}

/************************************************************************/
/*                            ClearCache()                              */
/************************************************************************/

void VSIOSSFSHandler::ClearCache()
{
    VSICurlFilesystemHandlerBase::ClearCache();

    VSIOSSUpdateParams::ClearCache();
}

/************************************************************************/
/*                           GetOptions()                               */
/************************************************************************/

const char *VSIOSSFSHandler::GetOptions()
{
    static std::string osOptions(
        std::string("<Options>") +
        "  <Option name='OSS_SECRET_ACCESS_KEY' type='string' "
        "description='Secret access key. To use with OSS_ACCESS_KEY_ID'/>"
        "  <Option name='OSS_ACCESS_KEY_ID' type='string' "
        "description='Access key id'/>"
        "  <Option name='OSS_ENDPOINT' type='string' "
        "description='Default endpoint' default='oss-us-east-1.aliyuncs.com'/>"
        "  <Option name='VSIOSS_CHUNK_SIZE' type='int' "
        "description='Size in MB for chunks of files that are uploaded. The"
        "default value of 50 MB allows for files up to 500 GB each' "
        "default='50' min='1' max='1000'/>" +
        VSICurlFilesystemHandlerBase::GetOptionsStatic() + "</Options>");
    return osOptions.c_str();
}

/************************************************************************/
/*                           GetSignedURL()                             */
/************************************************************************/

char *VSIOSSFSHandler::GetSignedURL(const char *pszFilename,
                                    CSLConstList papszOptions)
{
    if (!STARTS_WITH_CI(pszFilename, GetFSPrefix().c_str()))
        return nullptr;

    VSIOSSHandleHelper *poHandleHelper = VSIOSSHandleHelper::BuildFromURI(
        pszFilename + GetFSPrefix().size(), GetFSPrefix().c_str(), false,
        papszOptions);
    if (poHandleHelper == nullptr)
    {
        return nullptr;
    }

    std::string osRet(poHandleHelper->GetSignedURL(papszOptions));

    delete poHandleHelper;
    return CPLStrdup(osRet.c_str());
}

/************************************************************************/
/*                          CreateFileHandle()                          */
/************************************************************************/

VSICurlHandle *VSIOSSFSHandler::CreateFileHandle(const char *pszFilename)
{
    VSIOSSHandleHelper *poHandleHelper = VSIOSSHandleHelper::BuildFromURI(
        pszFilename + GetFSPrefix().size(), GetFSPrefix().c_str(), false);
    if (poHandleHelper)
    {
        return new VSIOSSHandle(this, pszFilename, poHandleHelper);
    }
    return nullptr;
}

/************************************************************************/
/*                          GetURLFromFilename()                         */
/************************************************************************/

std::string VSIOSSFSHandler::GetURLFromFilename(const std::string &osFilename)
{
    std::string osFilenameWithoutPrefix =
        osFilename.substr(GetFSPrefix().size());

    VSIOSSHandleHelper *poHandleHelper = VSIOSSHandleHelper::BuildFromURI(
        osFilenameWithoutPrefix.c_str(), GetFSPrefix().c_str(), true);
    if (poHandleHelper == nullptr)
    {
        return "";
    }

    std::string osBaseURL(poHandleHelper->GetURL());
    if (!osBaseURL.empty() && osBaseURL.back() == '/')
        osBaseURL.resize(osBaseURL.size() - 1);
    delete poHandleHelper;

    return osBaseURL;
}

/************************************************************************/
/*                          CreateHandleHelper()                        */
/************************************************************************/

IVSIS3LikeHandleHelper *VSIOSSFSHandler::CreateHandleHelper(const char *pszURI,
                                                            bool bAllowNoObject)
{
    return VSIOSSHandleHelper::BuildFromURI(pszURI, GetFSPrefix().c_str(),
                                            bAllowNoObject);
}

/************************************************************************/
/*                            VSIOSSHandle()                            */
/************************************************************************/

VSIOSSHandle::VSIOSSHandle(VSIOSSFSHandler *poFSIn, const char *pszFilename,
                           VSIOSSHandleHelper *poHandleHelper)
    : IVSIS3LikeHandle(poFSIn, pszFilename, poHandleHelper->GetURL().c_str()),
      m_poHandleHelper(poHandleHelper)
{
}

/************************************************************************/
/*                            ~VSIOSSHandle()                           */
/************************************************************************/

VSIOSSHandle::~VSIOSSHandle()
{
    delete m_poHandleHelper;
}

/************************************************************************/
/*                           GetCurlHeaders()                           */
/************************************************************************/

struct curl_slist *
VSIOSSHandle::GetCurlHeaders(const std::string &osVerb,
                             const struct curl_slist *psExistingHeaders)
{
    return m_poHandleHelper->GetCurlHeaders(osVerb, psExistingHeaders);
}

/************************************************************************/
/*                          CanRestartOnError()                         */
/************************************************************************/

bool VSIOSSHandle::CanRestartOnError(const char *pszErrorMsg,
                                     const char *pszHeaders, bool bSetError)
{
    if (m_poHandleHelper->CanRestartOnError(pszErrorMsg, pszHeaders, bSetError))
    {
        SetURL(m_poHandleHelper->GetURL().c_str());
        return true;
    }
    return false;
}

} /* end of namespace cpl */

#endif  // DOXYGEN_SKIP
//! @endcond

/************************************************************************/
/*                      VSIInstallOSSFileHandler()                      */
/************************************************************************/

/*!
 \brief Install /vsioss/ Alibaba Cloud Object Storage Service (OSS) file
 system handler (requires libcurl)

 \verbatim embed:rst
 See :ref:`/vsioss/ documentation <vsioss>`
 \endverbatim

 @since GDAL 2.3
 */
void VSIInstallOSSFileHandler(void)
{
    VSIFileManager::InstallHandler("/vsioss/", new cpl::VSIOSSFSHandler);
}

#endif /* HAVE_CURL */
