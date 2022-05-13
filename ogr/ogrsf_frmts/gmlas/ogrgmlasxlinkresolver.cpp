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

#include <time.h>

CPL_CVSID("$Id$")

/************************************************************************/
/*                         GMLASXLinkResolver()                         */
/************************************************************************/

GMLASXLinkResolver::GMLASXLinkResolver() :
    m_nGlobalResolutionTime(0),
    m_nMaxRAMCacheSize(atoi(CPLGetConfigOption("GMLAS_XLINK_RAM_CACHE_SIZE",
                                               "10000000"))),
    m_nCurrentRAMCacheSize(0)
{
}

/************************************************************************/
/*                             SetConf()                                */
/************************************************************************/

void GMLASXLinkResolver::SetConf( const GMLASXLinkResolutionConf& oConf )
{
    m_oConf = oConf;
    SetCacheDirectory(m_oConf.m_osCacheDirectory);
}

/************************************************************************/
/*                          FetchRawContent()                           */
/************************************************************************/

CPLString GMLASXLinkResolver::FetchRawContent(const CPLString& osURL,
                                              const char* pszHeaders)
{
    char** papszOptions = nullptr;
    if( m_oConf.m_nMaxGlobalResolutionTime > 0 &&
        m_nGlobalResolutionTime > m_oConf.m_nMaxGlobalResolutionTime )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Maximum global resolution time has been reached. "
                 "No remote resource will be fetched");
        return CPLString();
    }
    if( m_oConf.m_nTimeOut > 0 || m_oConf.m_nMaxGlobalResolutionTime > 0 )
    {
        int nTimeout = m_oConf.m_nTimeOut;
        if( m_oConf.m_nTimeOut > 0 && m_oConf.m_nMaxGlobalResolutionTime > 0 )
        {
            // Select the minimum between the individual timeout and the
            // remaining time granted by the max global resolution time.
            int nRemaining = m_oConf.m_nMaxGlobalResolutionTime -
                             m_nGlobalResolutionTime;
            if( nRemaining < nTimeout )
                nTimeout = nRemaining;
        }
        else if( m_oConf.m_nMaxGlobalResolutionTime > 0 )
        {
            nTimeout = m_oConf.m_nMaxGlobalResolutionTime -
                       m_nGlobalResolutionTime;
        }
        papszOptions = CSLSetNameValue(papszOptions, "TIMEOUT",
                                       CPLSPrintf("%d", nTimeout));
    }
    if( m_oConf.m_nMaxFileSize > 0 )
    {
        papszOptions = CSLSetNameValue(papszOptions, "MAX_FILE_SIZE",
                                       CPLSPrintf("%d", m_oConf.m_nMaxFileSize));
    }
    if( !m_oConf.m_osProxyServerPort.empty() )
    {
        papszOptions = CSLSetNameValue(papszOptions, "PROXY",
                                       m_oConf.m_osProxyServerPort);
    }
    if( !m_oConf.m_osProxyUserPassword.empty() )
    {
        papszOptions = CSLSetNameValue(papszOptions, "PROXYUSERPWD",
                                       m_oConf.m_osProxyUserPassword);
    }
    if( !m_oConf.m_osProxyAuth.empty() )
    {
        papszOptions = CSLSetNameValue(papszOptions, "PROXYAUTH",
                                       m_oConf.m_osProxyAuth);
    }
    if( pszHeaders != nullptr )
    {
        papszOptions = CSLSetNameValue(papszOptions, "HEADERS",
                                       pszHeaders);
    }
    time_t nTimeStart = time(nullptr);
    CPLHTTPResult* psResult = CPLHTTPFetch(osURL, papszOptions);
    time_t nTimeStop = time(nullptr);
    m_nGlobalResolutionTime += static_cast<int>(nTimeStop - nTimeStart);
    CSLDestroy(papszOptions);
    if( psResult == nullptr )
        return CPLString();

    if( psResult->nStatus != 0 ||
        psResult->pabyData == nullptr )
    {
        CPLHTTPDestroyResult(psResult);
        return CPLString();
    }

    CPLString osResult;
    osResult.assign( reinterpret_cast<char*>(psResult->pabyData),
                     psResult->nDataLen );
    CPLHTTPDestroyResult(psResult);
    return osResult;
}

/************************************************************************/
/*                           GetRawContent()                            */
/************************************************************************/

CPLString GMLASXLinkResolver::GetRawContent(const CPLString& osURL,
                                            const char* pszHeaders,
                                            bool bAllowRemoteDownload,
                                            bool bCacheResults)
{
    bool bDiskCacheAvailable = false;
    if( !m_osCacheDirectory.empty() &&
        RecursivelyCreateDirectoryIfNeeded() )
    {
        bDiskCacheAvailable = true;

        CPLString osCachedFileName(GetCachedFilename(osURL));
        VSILFILE* fp = nullptr;
        if( !m_bRefresh ||
            m_aoSetRefreshedFiles.find(osCachedFileName) !=
                                            m_aoSetRefreshedFiles.end() )
        {
            fp = VSIFOpenL( osCachedFileName, "rb");
        }
        if( fp != nullptr )
        {
            CPLDebug("GMLAS", "Use cached %s", osCachedFileName.c_str());
            GByte* pabyRet = nullptr;
            vsi_l_offset nSize = 0;
            CPLString osContent;
            if( VSIIngestFile( fp, nullptr, &pabyRet, &nSize, -1 ) )
            {
                osContent.assign( reinterpret_cast<const char*>(pabyRet),
                                  static_cast<size_t>(nSize) );
            }
            VSIFree(pabyRet);
            VSIFCloseL(fp);
            return osContent;
        }
        else if( bAllowRemoteDownload )
        {
            if( m_bRefresh )
                m_aoSetRefreshedFiles.insert(osCachedFileName);
        }
        else
        {
            CPLDebug("GMLAS",
                     "Could not find locally cached %s, and not allowed to"
                     "download it",
                     osURL.c_str());
            return CPLString();
        }
    }

    // Check memory cache first
    {
        const auto oIter = m_oMapURLToContent.find(osURL);
        if( oIter != m_oMapURLToContent.end() )
            return oIter->second;
    }

    const CPLString osContent(FetchRawContent(osURL, pszHeaders));
    // Cache to disk if possible
    if( bDiskCacheAvailable && bCacheResults && !osContent.empty() )
    {
        CPLString osCachedFileName(GetCachedFilename(osURL));
        CPLString osTmpfilename( osCachedFileName + ".tmp" );
        VSILFILE* fpTemp = VSIFOpenL( osTmpfilename, "wb" );
        if( fpTemp != nullptr )
        {
            const bool bSuccess = VSIFWriteL( osContent.data(),
                                              osContent.size(), 1,
                                              fpTemp ) == 1;
            VSIFCloseL(fpTemp);
            if( bSuccess )
                VSIRename( osTmpfilename, osCachedFileName );
        }
    }
    // Otherwise to RAM
    else if( !osContent.empty() && osContent.size() < m_nMaxRAMCacheSize )
    {
        // If cache is going to be saturated, evict larger objects first
        while( osContent.size() + m_nCurrentRAMCacheSize > m_nMaxRAMCacheSize )
        {
            std::map<size_t, std::vector<CPLString> >::reverse_iterator oIter =
                m_oMapFileSizeToURLs.rbegin();
            const size_t nSizeToEvict = oIter->first;
            m_nCurrentRAMCacheSize -= nSizeToEvict;
            const CPLString osURLToEvict(oIter->second.front());
            m_oMapURLToContent.erase(osURLToEvict);
            oIter->second.erase(oIter->second.begin());
            if( oIter->second.empty() )
                m_oMapFileSizeToURLs.erase( nSizeToEvict );
        }
        m_oMapURLToContent[osURL] = osContent;
        m_oMapFileSizeToURLs[osContent.size()].push_back(osURL);
        m_nCurrentRAMCacheSize += osContent.size();
    }
    return osContent;
}

/************************************************************************/
/*                     IsRawContentResolutionEnabled()                  */
/************************************************************************/

bool GMLASXLinkResolver::IsRawContentResolutionEnabled() const
{
    return m_oConf.m_bDefaultResolutionEnabled &&
           m_oConf.m_eDefaultResolutionMode ==
                                        GMLASXLinkResolutionConf::RawContent;
}

/************************************************************************/
/*                      GetMatchingResolutionRule()                      */
/************************************************************************/

int GMLASXLinkResolver::GetMatchingResolutionRule(const CPLString& osURL) const
{
    for(size_t i = 0; i < m_oConf.m_aoURLSpecificRules.size(); ++i )
    {
        if( osURL.compare(0,
                          m_oConf.m_aoURLSpecificRules[i].m_osURLPrefix.size(),
                          m_oConf.m_aoURLSpecificRules[i].m_osURLPrefix) == 0 )
        {
            return static_cast<int>(i);
        }
    }

    // No match
    return -1;
}

/************************************************************************/
/*                           GetRawContent()                            */
/************************************************************************/

CPLString GMLASXLinkResolver::GetRawContent(const CPLString& osURL)
{
    return GetRawContent(osURL,
                         nullptr,
                         m_oConf.m_bDefaultAllowRemoteDownload,
                         m_oConf.m_bDefaultCacheResults);
}

/************************************************************************/
/*                         GetRawContentForRule()                       */
/************************************************************************/

CPLString GMLASXLinkResolver::GetRawContentForRule(const CPLString& osURL,
                                                   int nIdxRule)
{
    const GMLASXLinkResolutionConf::URLSpecificResolution& oRule(
                                    m_oConf.m_aoURLSpecificRules[nIdxRule] );

    CPLString osHeaders;
    for( size_t i=0; i< oRule.m_aosNameValueHTTPHeaders.size(); ++i )
    {
        if( !osHeaders.empty() )
            osHeaders += "\r\n";
        osHeaders += oRule.m_aosNameValueHTTPHeaders[i].first;
        osHeaders += ": ";
        osHeaders += oRule.m_aosNameValueHTTPHeaders[i].second;
    }
    return GetRawContent(osURL,
                         osHeaders.empty() ? nullptr : osHeaders.c_str(),
                         oRule.m_bAllowRemoteDownload,
                         oRule.m_bCacheResults);
}
