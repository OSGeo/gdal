/******************************************************************************
 *
 * Project:  WMS Client Driver
 * Purpose:  Implementation of Dataset and RasterBand classes for WMS
 *           and other similar services.
 * Author:   Adam Nowacki, nowak@xpam.de
 *
 ******************************************************************************
 * Copyright (c) 2007, Adam Nowacki
 * Copyright (c) 2017, Dmitry Baryshnikov, <polimax@mail.ru>
 * Copyright (c) 2017, NextGIS, <info@nextgis.com>
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

CPL_CVSID("$Id$")


static void CleanCacheThread( void *pData )
{
    GDALWMSCache *pCache = static_cast<GDALWMSCache *>(pData);
    pCache->Clean();
}

//------------------------------------------------------------------------------
// FileCache
//------------------------------------------------------------------------------
class FileCache : public GDALWMSCacheImpl
{
public:
    FileCache(const CPLString& soPath, CPLXMLNode *pConfig) :
        GDALWMSCacheImpl(soPath, pConfig),
        m_osPostfix(""),
        m_nDepth(2),
        m_nExpires(604800),
        m_nMaxSize(67108864)
    {
        const char *pszCacheDepth = CPLGetXMLValue( pConfig, "Depth", "2" );
        if( pszCacheDepth != nullptr )
            m_nDepth = atoi( pszCacheDepth );

        const char *pszCacheExtension = CPLGetXMLValue( pConfig, "Extension", nullptr );
        if( pszCacheExtension != nullptr )
            m_osPostfix = pszCacheExtension;

        const char *pszCacheExpires = CPLGetXMLValue( pConfig, "Expires", nullptr );
        if( pszCacheExpires != nullptr )
        {
            m_nExpires = atoi( pszCacheExpires );
            CPLDebug("WMS", "Cache expires in %d sec", m_nExpires);
        }
        const char *pszCacheMaxSize = CPLGetXMLValue( pConfig, "MaxSize", nullptr );
        if( pszCacheMaxSize != nullptr )
            m_nMaxSize = atoi( pszCacheMaxSize );
    }

    virtual CPLErr Insert(const char *pszKey, const CPLString &osFileName) CPL_OVERRIDE
    {
        // Warns if it fails to write, but returns success
        CPLString soFilePath = GetFilePath( pszKey );
        MakeDirs( CPLGetDirname(soFilePath) );
        if ( CPLCopyFile( soFilePath, osFileName ) == CE_None)
            return CE_None;
        // Warn if it fails after folder creation
        CPLError( CE_Warning, CPLE_FileIO, "Error writing to WMS cache %s",
                 m_soPath.c_str() );
        return CE_None;
    }

    virtual enum GDALWMSCacheItemStatus GetItemStatus(const char *pszKey) const CPL_OVERRIDE
    {
        VSIStatBufL  sStatBuf;
        if( VSIStatL( GetFilePath(pszKey), &sStatBuf ) == 0 )
        {
            long seconds = time( nullptr ) - sStatBuf.st_mtime;
            return seconds < m_nExpires ? CACHE_ITEM_OK : CACHE_ITEM_EXPIRED;
        }
        return  CACHE_ITEM_NOT_FOUND;
    }

    virtual GDALDataset* GetDataset(const char *pszKey, char **papszOpenOptions) const CPL_OVERRIDE
    {
        return reinterpret_cast<GDALDataset*>(
                    GDALOpenEx( GetFilePath( pszKey ), GDAL_OF_RASTER |
                               GDAL_OF_READONLY | GDAL_OF_VERBOSE_ERROR, nullptr,
                               papszOpenOptions, nullptr ) );
    }

    virtual void Clean() CPL_OVERRIDE
    {
        char **papszList = VSIReadDirRecursive( m_soPath );
        if( papszList == nullptr )
        {
            return;
        }

        int counter = 0;
        std::vector<int> toDelete;
        off_t nSize = 0;
        long nTime = time( nullptr );
        while( papszList[counter] != nullptr )
        {
            const char* pszPath = CPLFormFilename( m_soPath, papszList[counter], nullptr );
            VSIStatBufL sStatBuf;
            if( VSIStatL( pszPath, &sStatBuf ) == 0 )
            {
                if( !VSI_ISDIR( sStatBuf.st_mode ) )
                {
                    long seconds = nTime - sStatBuf.st_mtime;
                    if(seconds > m_nExpires)
                    {
                        toDelete.push_back(counter);
                    }

                    nSize += sStatBuf.st_size;
                }
            }
            counter++;
        }

        if( nSize > m_nMaxSize )
        {
            CPLDebug( "WMS", "Delete %ld items from cache", toDelete.size());
            for( size_t i = 0; i < toDelete.size(); ++i )
            {
                const char* pszPath = CPLFormFilename( m_soPath,
                                                       papszList[toDelete[i]],
                                                       nullptr );
                VSIUnlink( pszPath );
            }
        }

        CSLDestroy(papszList);

        // Prevent very frequently execution
        CPLSleep(15);
    }

private:
    CPLString GetFilePath(const char* pszKey) const
    {
        CPLString soHash( CPLMD5String( pszKey ) );
        CPLString soCacheFile( m_soPath );

        if( !soCacheFile.empty() && soCacheFile.back() != '/' )
        {
            soCacheFile.append(1, '/');
        }

        for( int i = 0; i < m_nDepth; ++i )
        {
            soCacheFile.append( 1, soHash[i] );
            soCacheFile.append( 1, '/' );
        }
        soCacheFile.append( soHash );
        soCacheFile.append( m_osPostfix );
        return soCacheFile;
    }

    static void MakeDirs(const char *pszPath)
    {
        if( IsPathExists( pszPath ) )
        {
            return;
        }
        // Recursive makedirs, ignoring errors
        const char *pszDirPath = CPLGetDirname( pszPath );
        MakeDirs( pszDirPath );

        VSIMkdir( pszPath, 0744 );
    }

    static bool IsPathExists(const char *pszPath)
    {
        VSIStatBufL sbuf;
        return VSIStatL( pszPath, &sbuf ) == 0;
    }

private:
    CPLString m_osPostfix;
    int m_nDepth;
    int m_nExpires;
    int m_nMaxSize;
};

//------------------------------------------------------------------------------
// GDALWMSCache
//------------------------------------------------------------------------------

GDALWMSCache::GDALWMSCache() :
    m_osCachePath("./gdalwmscache"),
    m_hCleanThread(nullptr),
    m_poCache(nullptr)
{

}

GDALWMSCache::~GDALWMSCache()
{

}

CPLErr GDALWMSCache::Initialize(const char *pszUrl, CPLXMLNode *pConfig) {
    const char *pszXmlCachePath = CPLGetXMLValue( pConfig, "Path", nullptr );
    const char *pszUserCachePath = CPLGetConfigOption( "GDAL_DEFAULT_WMS_CACHE_PATH",
                                                     nullptr );
    if( pszXmlCachePath != nullptr )
    {
        m_osCachePath = pszXmlCachePath;
    }
    else if( pszUserCachePath != nullptr )
    {
        m_osCachePath = pszUserCachePath;
    }

    // Separate folder for each unique dataset url
    m_osCachePath = CPLFormFilename( m_osCachePath, CPLMD5String( pszUrl ), nullptr );

    // TODO: Add sqlite db cache type
    const char *pszType = CPLGetXMLValue( pConfig, "Type", "file" );
    if( EQUAL(pszType, "file") )
    {
        m_poCache = new FileCache(m_osCachePath, pConfig);
    }

    return CE_None;
}

CPLErr GDALWMSCache::Insert(const char *pszKey, const CPLString &soFileName)
{
    if( m_poCache != nullptr && pszKey != nullptr )
    {
        // Add file to cache
        CPLErr result = m_poCache->Insert(pszKey, soFileName);
        if( result == CE_None )
        {
            // Start clean thread
            if( m_hCleanThread == nullptr)
            {
                m_hCleanThread = CPLCreateJoinableThread(CleanCacheThread, this);
            }
        }
        return result;
    }

    return CE_Failure;
}

enum GDALWMSCacheItemStatus GDALWMSCache::GetItemStatus(const char *pszKey) const
{
    if( m_poCache != nullptr )
    {
        return m_poCache->GetItemStatus(pszKey);
    }
    return CACHE_ITEM_NOT_FOUND;
}

GDALDataset* GDALWMSCache::GetDataset(const char *pszKey,
                                      char **papszOpenOptions) const
{
    if( m_poCache != nullptr )
    {
        return m_poCache->GetDataset(pszKey, papszOpenOptions);
    }
    return nullptr;
}

void GDALWMSCache::Clean()
{
    if( m_poCache != nullptr )
    {
        CPLDebug("WMS", "Clean cache");
        m_poCache->Clean();
    }

    m_hCleanThread = nullptr;
}
