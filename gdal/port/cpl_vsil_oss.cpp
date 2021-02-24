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

CPL_CVSID("$Id$")

#ifndef HAVE_CURL

void VSIInstallOSSFileHandler( void )
{
    // Not supported
}

#else

//! @cond Doxygen_Suppress
#ifndef DOXYGEN_SKIP

#define ENABLE_DEBUG 0

namespace cpl {

/************************************************************************/
/*                         VSIOSSFSHandler                              */
/************************************************************************/

class VSIOSSFSHandler final : public IVSIS3LikeFSHandler
{
    CPL_DISALLOW_COPY_ASSIGN(VSIOSSFSHandler)

    std::map< CPLString, VSIOSSUpdateParams > oMapBucketsToOSSParams{};

protected:
        VSICurlHandle* CreateFileHandle( const char* pszFilename ) override;
        CPLString GetURLFromFilename( const CPLString& osFilename ) override;

        const char* GetDebugKey() const override { return "OSS"; }

        IVSIS3LikeHandleHelper* CreateHandleHelper(
            const char* pszURI, bool bAllowNoObject) override;

        CPLString GetFSPrefix() const override { return "/vsioss/"; }

        void ClearCache() override;

public:
        VSIOSSFSHandler() = default;
        ~VSIOSSFSHandler() override;

        VSIVirtualHandle *Open( const char *pszFilename,
                                const char *pszAccess,
                                bool bSetError,
                                CSLConstList papszOptions ) override;

        const char* GetOptions() override;

        void UpdateMapFromHandle(
            IVSIS3LikeHandleHelper * poHandleHelper ) override;
        void UpdateHandleFromMap(
            IVSIS3LikeHandleHelper * poHandleHelper ) override;

    char* GetSignedURL( const char* pszFilename, CSLConstList papszOptions ) override;
};

/************************************************************************/
/*                            VSIOSSHandle                              */
/************************************************************************/

class VSIOSSHandle final : public IVSIS3LikeHandle
{
    CPL_DISALLOW_COPY_ASSIGN(VSIOSSHandle)

    VSIOSSHandleHelper* m_poHandleHelper = nullptr;

  protected:
    struct curl_slist* GetCurlHeaders(
        const CPLString& osVerb,
        const struct curl_slist* psExistingHeaders ) override;
    bool CanRestartOnError( const char*, const char*, bool ) override;

  public:
    VSIOSSHandle( VSIOSSFSHandler* poFS,
                  const char* pszFilename,
                  VSIOSSHandleHelper* poHandleHelper );
    ~VSIOSSHandle() override;
};

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

VSIVirtualHandle* VSIOSSFSHandler::Open( const char *pszFilename,
                                        const char *pszAccess,
                                        bool bSetError,
                                        CSLConstList papszOptions )
{
    if( !STARTS_WITH_CI(pszFilename, GetFSPrefix()) )
        return nullptr;

    if( strchr(pszAccess, 'w') != nullptr || strchr(pszAccess, 'a') != nullptr )
    {
        if( strchr(pszAccess, '+') != nullptr &&
            !CPLTestBool(CPLGetConfigOption("CPL_VSIL_USE_TEMP_FILE_FOR_RANDOM_WRITE", "NO")) )
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                        "w+ not supported for /vsioss, unless "
                        "CPL_VSIL_USE_TEMP_FILE_FOR_RANDOM_WRITE is set to YES");
            errno = EACCES;
            return nullptr;
        }

        VSIOSSHandleHelper* poHandleHelper =
            VSIOSSHandleHelper::BuildFromURI(pszFilename + GetFSPrefix().size(),
                                            GetFSPrefix().c_str(), false);
        if( poHandleHelper == nullptr )
            return nullptr;
        UpdateHandleFromMap(poHandleHelper);
        VSIS3WriteHandle* poHandle =
            new VSIS3WriteHandle(this, pszFilename, poHandleHelper, false, papszOptions);
        if( !poHandle->IsOK() )
        {
            delete poHandle;
            return nullptr;
        }
        if( strchr(pszAccess, '+') != nullptr)
        {
            return VSICreateUploadOnCloseFile(poHandle);
        }
        return poHandle;
    }

    return
        VSICurlFilesystemHandler::Open(pszFilename, pszAccess, bSetError, papszOptions);
}

/************************************************************************/
/*                       ~VSIOSSFSHandler()                             */
/************************************************************************/

VSIOSSFSHandler::~VSIOSSFSHandler()
{
    VSIOSSFSHandler::ClearCache();
}

/************************************************************************/
/*                            ClearCache()                              */
/************************************************************************/

void VSIOSSFSHandler::ClearCache()
{
    VSICurlFilesystemHandler::ClearCache();

    oMapBucketsToOSSParams.clear();
}

/************************************************************************/
/*                           GetOptions()                               */
/************************************************************************/

const char* VSIOSSFSHandler::GetOptions()
{
    static CPLString osOptions(
        CPLString("<Options>") +
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
        VSICurlFilesystemHandler::GetOptionsStatic() +
        "</Options>");
    return osOptions.c_str();
}

/************************************************************************/
/*                           GetSignedURL()                             */
/************************************************************************/

char* VSIOSSFSHandler::GetSignedURL(const char* pszFilename, CSLConstList papszOptions )
{
    if( !STARTS_WITH_CI(pszFilename, GetFSPrefix()) )
        return nullptr;

    VSIOSSHandleHelper* poHandleHelper =
        VSIOSSHandleHelper::BuildFromURI(pszFilename + GetFSPrefix().size(),
                                        GetFSPrefix().c_str(), false,
                                        papszOptions);
    if( poHandleHelper == nullptr )
    {
        return nullptr;
    }

    CPLString osRet(poHandleHelper->GetSignedURL(papszOptions));

    delete poHandleHelper;
    return CPLStrdup(osRet);
}

/************************************************************************/
/*                          CreateFileHandle()                          */
/************************************************************************/

VSICurlHandle* VSIOSSFSHandler::CreateFileHandle(const char* pszFilename)
{
    VSIOSSHandleHelper* poHandleHelper =
        VSIOSSHandleHelper::BuildFromURI(pszFilename + GetFSPrefix().size(),
                                        GetFSPrefix().c_str(), false);
    if( poHandleHelper )
    {
        UpdateHandleFromMap(poHandleHelper);
        return new VSIOSSHandle(this, pszFilename, poHandleHelper);
    }
    return nullptr;
}

/************************************************************************/
/*                          GetURLFromFilename()                         */
/************************************************************************/

CPLString VSIOSSFSHandler::GetURLFromFilename( const CPLString& osFilename )
{
    CPLString osFilenameWithoutPrefix = osFilename.substr(GetFSPrefix().size());

    VSIOSSHandleHelper* poHandleHelper =
        VSIOSSHandleHelper::BuildFromURI(osFilenameWithoutPrefix,
                                        GetFSPrefix().c_str(), true);
    if( poHandleHelper == nullptr )
    {
        return "";
    }
    UpdateHandleFromMap(poHandleHelper);
    CPLString osBaseURL(poHandleHelper->GetURL());
    if( !osBaseURL.empty() && osBaseURL.back() == '/' )
        osBaseURL.resize(osBaseURL.size()-1);
    delete poHandleHelper;

    return osBaseURL;
}

/************************************************************************/
/*                          CreateHandleHelper()                        */
/************************************************************************/

IVSIS3LikeHandleHelper* VSIOSSFSHandler::CreateHandleHelper(const char* pszURI,
                                                          bool bAllowNoObject)
{
    return VSIOSSHandleHelper::BuildFromURI(
                                pszURI, GetFSPrefix().c_str(), bAllowNoObject);
}

/************************************************************************/
/*                         UpdateMapFromHandle()                        */
/************************************************************************/

void VSIOSSFSHandler::UpdateMapFromHandle( IVSIS3LikeHandleHelper * poHandleHelper )
{
    CPLMutexHolder oHolder( &hMutex );

    VSIOSSHandleHelper * poOSSHandleHelper =
        dynamic_cast<VSIOSSHandleHelper *>(poHandleHelper);
    CPLAssert( poOSSHandleHelper );
    if( !poOSSHandleHelper )
        return;
    oMapBucketsToOSSParams[ poOSSHandleHelper->GetBucket() ] =
        VSIOSSUpdateParams ( poOSSHandleHelper );
}

/************************************************************************/
/*                         UpdateHandleFromMap()                        */
/************************************************************************/

void VSIOSSFSHandler::UpdateHandleFromMap( IVSIS3LikeHandleHelper * poHandleHelper )
{
    CPLMutexHolder oHolder( &hMutex );

    VSIOSSHandleHelper * poOSSHandleHelper =
        dynamic_cast<VSIOSSHandleHelper *>(poHandleHelper);
    CPLAssert( poOSSHandleHelper );
    if( !poOSSHandleHelper )
        return;
    std::map< CPLString, VSIOSSUpdateParams>::iterator oIter =
        oMapBucketsToOSSParams.find(poOSSHandleHelper->GetBucket());
    if( oIter != oMapBucketsToOSSParams.end() )
    {
        oIter->second.UpdateHandlerHelper(poOSSHandleHelper);
    }
}

/************************************************************************/
/*                            VSIOSSHandle()                            */
/************************************************************************/

VSIOSSHandle::VSIOSSHandle( VSIOSSFSHandler* poFSIn,
                          const char* pszFilename,
                          VSIOSSHandleHelper* poHandleHelper ) :
        IVSIS3LikeHandle(poFSIn, pszFilename, poHandleHelper->GetURL()),
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

struct curl_slist* VSIOSSHandle::GetCurlHeaders( const CPLString& osVerb,
                                const struct curl_slist* psExistingHeaders )
{
    return m_poHandleHelper->GetCurlHeaders(osVerb, psExistingHeaders);
}

/************************************************************************/
/*                          CanRestartOnError()                         */
/************************************************************************/

bool VSIOSSHandle::CanRestartOnError(const char* pszErrorMsg,
                                     const char* pszHeaders, bool bSetError)
{
    if( m_poHandleHelper->CanRestartOnError(pszErrorMsg, pszHeaders,
                                            bSetError, nullptr) )
    {
        static_cast<VSIOSSFSHandler *>(poFS)->
            UpdateMapFromHandle(m_poHandleHelper);

        SetURL(m_poHandleHelper->GetURL());
        return true;
    }
    return false;
}

} /* end of namespace cpl */


#endif // DOXYGEN_SKIP
//! @endcond

/************************************************************************/
/*                      VSIInstallOSSFileHandler()                      */
/************************************************************************/

/**
 * \brief Install /vsioss/ Alibaba Cloud Object Storage Service (OSS) file
 * system handler (requires libcurl)
 *
 * @see <a href="gdal_virtual_file_systems.html#gdal_virtual_file_systems_vsioss">/vsioss/ documentation</a>
 *
 * @since GDAL 2.3
 */
void VSIInstallOSSFileHandler( void )
{
    VSIFileManager::InstallHandler( "/vsioss/", new cpl::VSIOSSFSHandler );
}

#endif /* HAVE_CURL */
