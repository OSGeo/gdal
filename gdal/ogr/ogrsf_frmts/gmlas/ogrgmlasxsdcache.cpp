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

// Must be first for DEBUG_BOOL case
#include "ogr_gmlas.h"

#include "cpl_sha256.h"

CPL_CVSID("$Id$")

/************************************************************************/
/*                         GMLASResourceCache()                         */
/************************************************************************/

GMLASResourceCache::GMLASResourceCache()
    : m_bHasCheckedCacheDirectory(false)
    , m_bRefresh(false)
    , m_bAllowDownload(true)
{
}

/************************************************************************/
/*                        ~GMLASResourceCache()                         */
/************************************************************************/

GMLASResourceCache::~GMLASResourceCache()
{
}

/************************************************************************/
/*                         SetCacheDirectory()                          */
/************************************************************************/

void GMLASResourceCache::SetCacheDirectory(const CPLString& osCacheDirectory)
{
    m_osCacheDirectory = osCacheDirectory;
}

/************************************************************************/
/*                     RecursivelyCreateDirectoryIfNeeded()             */
/************************************************************************/

bool GMLASResourceCache::RecursivelyCreateDirectoryIfNeeded(
                                                const CPLString& osDirname)
{
    VSIStatBufL sStat;
    if( VSIStatL(osDirname, &sStat) == 0 )
    {
        return true;
    }

    CPLString osParent = CPLGetDirname(osDirname);
    if( !osParent.empty() && osParent != "." )
    {
        if( !RecursivelyCreateDirectoryIfNeeded(osParent) )
            return false;
    }
    return VSIMkdir( osDirname, 0755 ) == 0;
}

bool GMLASResourceCache::RecursivelyCreateDirectoryIfNeeded()
{
    if( !m_bHasCheckedCacheDirectory )
    {
        m_bHasCheckedCacheDirectory = true;
        if( !RecursivelyCreateDirectoryIfNeeded(m_osCacheDirectory) )
        {
            CPLError(CE_Warning, CPLE_AppDefined,
                     "Cannot create %s", m_osCacheDirectory.c_str());
            m_osCacheDirectory.clear();
            return false;
        }
    }
    return true;
}

/************************************************************************/
/*                        GetCachedFilename()                           */
/************************************************************************/

CPLString GMLASResourceCache::GetCachedFilename(const CPLString& osResource)
{
    CPLString osLaunderedName(osResource);
    if( osLaunderedName.find("/vsicurl_streaming/") == 0 )
        osLaunderedName = osLaunderedName.substr(
                                strlen("/vsicurl_streaming/") );
    if( osLaunderedName.find("http://") == 0 )
        osLaunderedName = osLaunderedName.substr( strlen("http://") );
    else if( osLaunderedName.find("https://") == 0 )
        osLaunderedName = osLaunderedName.substr( strlen("https://") );
    for(size_t i=0; i<osLaunderedName.size(); i++)
    {
        if( !isalnum(osLaunderedName[i]) && osLaunderedName[i] != '.' )
            osLaunderedName[i] = '_';
    }

    // If filename is too long, then truncate it and put a hash at the end
    // We try to make sure that the whole filename (including the cache path)
    // fits into 255 characters, for windows compat

    const size_t nWindowsMaxFilenameSize = 255;
    // 60 is arbitrary but should be sufficient for most people. We could
    // always take into account m_osCacheDirectory.size(), but if we want to
    // to be able to share caches between computers, then this would be impractical.
    const size_t nTypicalMaxSizeForDirName = 60;
    const size_t nSizeForDirName =
    (m_osCacheDirectory.size() > nTypicalMaxSizeForDirName &&
     m_osCacheDirectory.size() <
        nWindowsMaxFilenameSize - strlen(".tmp") -  2 * CPL_SHA256_HASH_SIZE) ?
                m_osCacheDirectory.size() : nTypicalMaxSizeForDirName;
    CPLAssert( nWindowsMaxFilenameSize >= nSizeForDirName );
    const size_t nMaxFilenameSize = nWindowsMaxFilenameSize - nSizeForDirName;

    CPLAssert( nMaxFilenameSize >= strlen(".tmp") );
    if( osLaunderedName.size() >= nMaxFilenameSize - strlen(".tmp") )
    {
        GByte abyHash[CPL_SHA256_HASH_SIZE];
        CPL_SHA256(osResource, osResource.size(), abyHash);
        char* pszHash = CPLBinaryToHex(CPL_SHA256_HASH_SIZE, abyHash);
        osLaunderedName.resize(nMaxFilenameSize - strlen(".tmp") -  2 * CPL_SHA256_HASH_SIZE);
        osLaunderedName += pszHash;
        CPLFree(pszHash);
        CPLDebug("GMLAS", "Cached filename truncated to %s",
                    osLaunderedName.c_str());
    }

    return CPLFormFilename( m_osCacheDirectory, osLaunderedName, nullptr );
}

/************************************************************************/
/*                          GMLASXSDCache()                             */
/************************************************************************/

GMLASXSDCache::GMLASXSDCache()
{
}

/************************************************************************/
/*                         ~GMLASXSDCache()                             */
/************************************************************************/

GMLASXSDCache::~GMLASXSDCache()
{
}
/************************************************************************/
/*                               Open()                                 */
/************************************************************************/

VSILFILE* GMLASXSDCache::Open( const CPLString& osResource,
                               const CPLString& osBasePath,
                               CPLString& osOutFilename )
{
    osOutFilename = osResource;
    if( osResource.find("http://") == 0 ||
        osResource.find("https://") == 0 )
    {
        osOutFilename = "/vsicurl_streaming/" + osResource;
    }
    else if( CPLIsFilenameRelative( osResource ) && !osResource.empty() )
    {
        /* Transform a/b + ../c --> a/c */
        CPLString osResourceModified(osResource);
        CPLString osBasePathModified(osBasePath);
        while( (osResourceModified.find("../") == 0 ||
                osResourceModified.find("..\\") == 0) &&
               !osBasePathModified.empty() )
        {
            osBasePathModified = CPLGetDirname(osBasePathModified);
            osResourceModified = osResourceModified.substr(3);
        }

        osOutFilename = CPLFormFilename(osBasePathModified,
                                        osResourceModified, nullptr);
    }

    CPLDebug("GMLAS", "Resolving %s (%s) to %s",
                osResource.c_str(),
                osBasePath.c_str(),
                osOutFilename.c_str());

    VSILFILE* fp = nullptr;
    if( !m_osCacheDirectory.empty() &&
        osOutFilename.find("/vsicurl_streaming/") == 0 &&
        RecursivelyCreateDirectoryIfNeeded() )
    {
        CPLString osCachedFileName(GetCachedFilename(osOutFilename));
        if( !m_bRefresh ||
            m_aoSetRefreshedFiles.find(osCachedFileName) !=
                                            m_aoSetRefreshedFiles.end() )
        {
            fp = VSIFOpenL( osCachedFileName, "rb");
        }
        if( fp != nullptr )
        {
            CPLDebug("GMLAS", "Use cached %s", osCachedFileName.c_str());
        }
        else if( m_bAllowDownload )
        {
            if( m_bRefresh )
                m_aoSetRefreshedFiles.insert(osCachedFileName);

            CPLString osTmpfilename( osCachedFileName + ".tmp" );
            if( CPLCopyFile( osTmpfilename, osOutFilename) == 0 )
            {
                // Due to the caching done by /vsicurl_streaming/, if the
                // web server is no longer available but was before in the
                // same process, then file opening will succeed. Hence we
                // check that the downloaded file is not 0. This will only
                // happen in practice with the unit tests.
                VSIStatBufL sStat;
                if( VSIStatL(osTmpfilename, &sStat) == 0 &&
                    sStat.st_size != 0 )
                {
                    VSIRename( osTmpfilename, osCachedFileName );
                    fp = VSIFOpenL(osCachedFileName, "rb");
                }
                else
                {
                    VSIUnlink(osTmpfilename);
                }
            }
        }
    }
    else
    {
        if( m_bAllowDownload ||
            osOutFilename.find("/vsicurl_streaming/") != 0 )
        {
            fp = VSIFOpenL(osOutFilename, "rb");
        }
    }

    if( fp == nullptr )
    {
        CPLError(CE_Failure, CPLE_FileIO,
                 "Cannot resolve %s", osResource.c_str());
    }

    return fp;
}
