/******************************************************************************
 * Project:  OGR
 * Purpose:  OGRGMLASDriver implementation
 * Author:   Even Rouault, <even dot rouault at spatialys dot com>
 *
 * Initial development funded by the European Earth observation programme
 * Copernicus
 *
 ******************************************************************************
 * Copyright (c) 2016, Even Rouault, <even dot rouault at spatialys dot com>
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

#include "ogr_gmlas.h"

#include "cpl_http.h"
#include "cpl_sha256.h"

/************************************************************************/
/*                         SetCacheDirectory()                          */
/************************************************************************/

void GMLASResourceCache::SetCacheDirectory(const std::string &osCacheDirectory)
{
    m_osCacheDirectory = osCacheDirectory;
}

/************************************************************************/
/*                     RecursivelyCreateDirectoryIfNeeded()             */
/************************************************************************/

bool GMLASResourceCache::RecursivelyCreateDirectoryIfNeeded(
    const std::string &osDirname)
{
    VSIStatBufL sStat;
    if (VSIStatL(osDirname.c_str(), &sStat) == 0)
    {
        return true;
    }

    std::string osParent = CPLGetDirname(osDirname.c_str());
    if (!osParent.empty() && osParent != ".")
    {
        if (!RecursivelyCreateDirectoryIfNeeded(osParent.c_str()))
            return false;
    }
    return VSIMkdir(osDirname.c_str(), 0755) == 0;
}

bool GMLASResourceCache::RecursivelyCreateDirectoryIfNeeded()
{
    if (!m_bHasCheckedCacheDirectory)
    {
        m_bHasCheckedCacheDirectory = true;
        if (!RecursivelyCreateDirectoryIfNeeded(m_osCacheDirectory))
        {
            CPLError(CE_Warning, CPLE_AppDefined, "Cannot create %s",
                     m_osCacheDirectory.c_str());
            m_osCacheDirectory.clear();
            return false;
        }
    }
    return true;
}

/************************************************************************/
/*                        GetCachedFilename()                           */
/************************************************************************/

std::string GMLASResourceCache::GetCachedFilename(const std::string &osResource)
{
    std::string osLaunderedName(osResource);
    if (STARTS_WITH(osLaunderedName.c_str(), "http://"))
        osLaunderedName = osLaunderedName.substr(strlen("http://"));
    else if (STARTS_WITH(osLaunderedName.c_str(), "https://"))
        osLaunderedName = osLaunderedName.substr(strlen("https://"));
    for (size_t i = 0; i < osLaunderedName.size(); i++)
    {
        if (!isalnum(static_cast<unsigned char>(osLaunderedName[i])) &&
            osLaunderedName[i] != '.')
            osLaunderedName[i] = '_';
    }

    // If filename is too long, then truncate it and put a hash at the end
    // We try to make sure that the whole filename (including the cache path)
    // fits into 255 characters, for windows compat

    const size_t nWindowsMaxFilenameSize = 255;
    // 60 is arbitrary but should be sufficient for most people. We could
    // always take into account m_osCacheDirectory.size(), but if we want to
    // to be able to share caches between computers, then this would be
    // impractical.
    const size_t nTypicalMaxSizeForDirName = 60;
    const size_t nSizeForDirName =
        (m_osCacheDirectory.size() > nTypicalMaxSizeForDirName &&
         m_osCacheDirectory.size() < nWindowsMaxFilenameSize - strlen(".tmp") -
                                         2 * CPL_SHA256_HASH_SIZE)
            ? m_osCacheDirectory.size()
            : nTypicalMaxSizeForDirName;
    CPLAssert(nWindowsMaxFilenameSize >= nSizeForDirName);
    const size_t nMaxFilenameSize = nWindowsMaxFilenameSize - nSizeForDirName;

    CPLAssert(nMaxFilenameSize >= strlen(".tmp"));
    if (osLaunderedName.size() >= nMaxFilenameSize - strlen(".tmp"))
    {
        GByte abyHash[CPL_SHA256_HASH_SIZE];
        CPL_SHA256(osResource.c_str(), osResource.size(), abyHash);
        char *pszHash = CPLBinaryToHex(CPL_SHA256_HASH_SIZE, abyHash);
        osLaunderedName.resize(nMaxFilenameSize - strlen(".tmp") -
                               2 * CPL_SHA256_HASH_SIZE);
        osLaunderedName += pszHash;
        CPLFree(pszHash);
        CPLDebug("GMLAS", "Cached filename truncated to %s",
                 osLaunderedName.c_str());
    }

    return CPLFormFilename(m_osCacheDirectory.c_str(), osLaunderedName.c_str(),
                           nullptr);
}

/************************************************************************/
/*                          CacheAllGML321()                            */
/************************************************************************/

bool GMLASXSDCache::CacheAllGML321()
{
    // As of today (2024-01-02), the schemas in https://schemas.opengis.net/gml/3.2.1
    // are actually the same as the ones in the https://schemas.opengis.net/gml/gml-3_2_2.zip archive.
    // Download the later and unzip it for faster fetching of GML schemas.

    bool bSuccess = false;
    CPLErrorStateBackuper oErrorStateBackuper(CPLQuietErrorHandler);

    const char *pszHTTPZIP = "https://schemas.opengis.net/gml/gml-3_2_2.zip";
    CPLHTTPResult *psResult = CPLHTTPFetch(pszHTTPZIP, nullptr);
    if (psResult && psResult->nDataLen)
    {
        const std::string osZIPFilename(CPLSPrintf("/vsimem/%p.zip", this));
        auto fpZIP =
            VSIFileFromMemBuffer(osZIPFilename.c_str(), psResult->pabyData,
                                 psResult->nDataLen, FALSE);
        if (fpZIP)
        {
            VSIFCloseL(fpZIP);

            const std::string osVSIZIPFilename("/vsizip/" + osZIPFilename);
            const CPLStringList aosFiles(
                VSIReadDirRecursive(osVSIZIPFilename.c_str()));
            for (int i = 0; i < aosFiles.size(); ++i)
            {
                if (strstr(aosFiles[i], ".xsd"))
                {
                    const std::string osFilename(
                        std::string("https://schemas.opengis.net/gml/3.2.1/") +
                        CPLGetFilename(aosFiles[i]));
                    const std::string osCachedFileName(
                        GetCachedFilename(osFilename.c_str()));

                    std::string osTmpfilename(osCachedFileName + ".tmp");
                    if (CPLCopyFile(
                            osTmpfilename.c_str(),
                            (osVSIZIPFilename + "/" + aosFiles[i]).c_str()) ==
                        0)
                    {
                        VSIRename(osTmpfilename.c_str(),
                                  osCachedFileName.c_str());
                        bSuccess = true;
                    }
                }
            }
        }
        VSIUnlink(osZIPFilename.c_str());
    }
    CPLHTTPDestroyResult(psResult);
    if (!bSuccess)
    {
        static bool bHasWarned = false;
        if (!bHasWarned)
        {
            bHasWarned = true;
            CPLDebug("GMLAS", "Cannot get GML schemas from %s", pszHTTPZIP);
        }
    }
    return bSuccess;
}

/************************************************************************/
/*                         CacheAllISO20070417()                        */
/************************************************************************/

bool GMLASXSDCache::CacheAllISO20070417()
{
    // As of today (2024-01-02), the schemas in https://schemas.opengis.net/iso/19139/20070417/
    // are actually the same as the ones in the iso19139-20070417_5-v20220526.zip archive
    // in https://schemas.opengis.net/iso/19139/iso19139-20070417.zip archive.
    // Download the later and unzip it for faster fetching of ISO schemas.

    bool bSuccess = false;
    CPLErrorStateBackuper oErrorStateBackuper(CPLQuietErrorHandler);

    const char *pszHTTPZIP =
        "https://schemas.opengis.net/iso/19139/iso19139-20070417.zip";
    CPLHTTPResult *psResult = CPLHTTPFetch(pszHTTPZIP, nullptr);
    if (psResult && psResult->nDataLen)
    {
        const std::string osZIPFilename(CPLSPrintf("/vsimem/%p.zip", this));
        auto fpZIP =
            VSIFileFromMemBuffer(osZIPFilename.c_str(), psResult->pabyData,
                                 psResult->nDataLen, FALSE);
        if (fpZIP)
        {
            VSIFCloseL(fpZIP);

            const std::string osVSIZIPFilename(
                "/vsizip//vsizip/" + osZIPFilename +
                "/iso19139-20070417_5-v20220526.zip");
            const CPLStringList aosFiles(
                VSIReadDirRecursive(osVSIZIPFilename.c_str()));
            for (int i = 0; i < aosFiles.size(); ++i)
            {
                if (STARTS_WITH(aosFiles[i], "iso/19139/20070417/") &&
                    strstr(aosFiles[i], ".xsd"))
                {
                    const std::string osFilename(
                        std::string("https://schemas.opengis.net/") +
                        aosFiles[i]);
                    const std::string osCachedFileName(
                        GetCachedFilename(osFilename.c_str()));

                    std::string osTmpfilename(osCachedFileName + ".tmp");
                    if (CPLCopyFile(
                            osTmpfilename.c_str(),
                            (osVSIZIPFilename + "/" + aosFiles[i]).c_str()) ==
                        0)
                    {
                        VSIRename(osTmpfilename.c_str(),
                                  osCachedFileName.c_str());
                        bSuccess = true;
                    }
                }
            }
        }
        VSIUnlink(osZIPFilename.c_str());
    }
    CPLHTTPDestroyResult(psResult);
    if (!bSuccess)
    {
        static bool bHasWarned = false;
        if (!bHasWarned)
        {
            bHasWarned = true;
            CPLDebug("GMLAS", "Cannot get ISO schemas from %s", pszHTTPZIP);
        }
    }
    return bSuccess;
}

/************************************************************************/
/*                               Open()                                 */
/************************************************************************/

VSILFILE *GMLASXSDCache::Open(const std::string &osResource,
                              const std::string &osBasePath,
                              std::string &osOutFilename)
{
    osOutFilename = osResource;
    if (!STARTS_WITH(osResource.c_str(), "http://") &&
        !STARTS_WITH(osResource.c_str(), "https://") &&
        CPLIsFilenameRelative(osResource.c_str()) && !osResource.empty())
    {
        /* Transform a/b + ../c --> a/c */
        std::string osResourceModified(osResource);
        std::string osBasePathModified(osBasePath);
        while ((STARTS_WITH(osResourceModified.c_str(), "../") ||
                STARTS_WITH(osResourceModified.c_str(), "..\\")) &&
               !osBasePathModified.empty())
        {
            osBasePathModified = CPLGetDirname(osBasePathModified.c_str());
            osResourceModified = osResourceModified.substr(3);
        }

        osOutFilename = CPLFormFilename(osBasePathModified.c_str(),
                                        osResourceModified.c_str(), nullptr);
    }

    CPLDebug("GMLAS", "Resolving %s (%s) to %s", osResource.c_str(),
             osBasePath.c_str(), osOutFilename.c_str());

    VSILFILE *fp = nullptr;
    bool bHasTriedZIPArchive = false;
retry:
    if (!m_osCacheDirectory.empty() &&
        (STARTS_WITH(osOutFilename.c_str(), "http://") ||
         STARTS_WITH(osOutFilename.c_str(), "https://")) &&
        RecursivelyCreateDirectoryIfNeeded())
    {
        const std::string osCachedFileName(
            GetCachedFilename(osOutFilename.c_str()));
        if (!m_bRefresh || m_aoSetRefreshedFiles.find(osCachedFileName) !=
                               m_aoSetRefreshedFiles.end())
        {
            fp = VSIFOpenL(osCachedFileName.c_str(), "rb");
        }
        if (fp != nullptr)
        {
            CPLDebug("GMLAS", "Use cached %s", osCachedFileName.c_str());
        }
        else if (m_bAllowDownload)
        {
            if (m_bRefresh)
                m_aoSetRefreshedFiles.insert(osCachedFileName);

            else if (!bHasTriedZIPArchive &&
                     strstr(osOutFilename.c_str(),
                            "://schemas.opengis.net/gml/3.2.1/") &&
                     CPLTestBool(CPLGetConfigOption(
                         "OGR_GMLAS_USE_SCHEMAS_FROM_OGC_ZIP", "YES")))
            {
                bHasTriedZIPArchive = true;
                if (CacheAllGML321())
                    goto retry;
            }

            else if (!bHasTriedZIPArchive &&
                     strstr(osOutFilename.c_str(),
                            "://schemas.opengis.net/iso/19139/20070417/") &&
                     CPLTestBool(CPLGetConfigOption(
                         "OGR_GMLAS_USE_SCHEMAS_FROM_OGC_ZIP", "YES")))
            {
                bHasTriedZIPArchive = true;
                if (CacheAllISO20070417())
                    goto retry;
            }

            CPLHTTPResult *psResult =
                CPLHTTPFetch(osOutFilename.c_str(), nullptr);
            if (psResult == nullptr || psResult->nDataLen == 0)
            {
                CPLError(CE_Failure, CPLE_FileIO, "Cannot resolve %s",
                         osResource.c_str());
                CPLHTTPDestroyResult(psResult);
                return nullptr;
            }

            std::string osTmpfilename(osCachedFileName + ".tmp");
            VSILFILE *fpTmp = VSIFOpenL(osTmpfilename.c_str(), "wb");
            if (fpTmp)
            {
                const auto nRet = VSIFWriteL(psResult->pabyData,
                                             psResult->nDataLen, 1, fpTmp);
                VSIFCloseL(fpTmp);
                if (nRet == 1)
                {
                    VSIRename(osTmpfilename.c_str(), osCachedFileName.c_str());
                    fp = VSIFOpenL(osCachedFileName.c_str(), "rb");
                }
            }

            CPLHTTPDestroyResult(psResult);
        }
    }
    else
    {
        if (STARTS_WITH(osOutFilename.c_str(), "http://") ||
            STARTS_WITH(osOutFilename.c_str(), "https://"))
        {
            if (m_bAllowDownload)
            {
                CPLHTTPResult *psResult =
                    CPLHTTPFetch(osOutFilename.c_str(), nullptr);
                if (psResult == nullptr || psResult->nDataLen == 0)
                {
                    CPLError(CE_Failure, CPLE_FileIO, "Cannot resolve %s",
                             osResource.c_str());
                    CPLHTTPDestroyResult(psResult);
                    return nullptr;
                }

                fp = VSIFileFromMemBuffer(nullptr, psResult->pabyData,
                                          psResult->nDataLen, TRUE);
                if (fp)
                {
                    // Steal the memory buffer from HTTP result
                    psResult->pabyData = nullptr;
                    psResult->nDataLen = 0;
                    psResult->nDataAlloc = 0;
                }
                CPLHTTPDestroyResult(psResult);
            }
        }
        else
        {
            fp = VSIFOpenL(osOutFilename.c_str(), "rb");
        }
    }

    if (fp == nullptr)
    {
        CPLError(CE_Failure, CPLE_FileIO, "Cannot resolve %s",
                 osResource.c_str());
    }

    return fp;
}
