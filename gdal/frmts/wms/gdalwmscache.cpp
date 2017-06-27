/******************************************************************************
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

#include "wmsdriver.h"

CPL_CVSID("$Id$")

GDALWMSCache::GDALWMSCache() :
    m_cache_path("./gdalwmscache"),
    // No need to do the default: m_postfix("");
    m_cache_depth(2)
{}

GDALWMSCache::~GDALWMSCache() {}

CPLErr GDALWMSCache::Initialize(CPLXMLNode *config) {
    const char *xmlcache_path = CPLGetXMLValue(config, "Path", NULL);
    const char *usercache_path = CPLGetConfigOption("GDAL_DEFAULT_WMS_CACHE_PATH", NULL);
    if(xmlcache_path)
    {
        m_cache_path = xmlcache_path;
    }
    else
    {
        if(usercache_path)
        {
            m_cache_path = usercache_path;
        }
        else
        {
            m_cache_path = "./gdalwmscache";
        }
    }

    const char *cache_depth = CPLGetXMLValue(config, "Depth", "2");
    m_cache_depth = atoi(cache_depth);

    const char *cache_extension = CPLGetXMLValue(config, "Extension", "");
    m_postfix = cache_extension;

    return CE_None;
}

// Recursive makedirs, ignoring errors
static void MakeDirs(const CPLString & path) {
    CPLString p(CPLGetDirname(path));
    if (p.size() >= 2)
        MakeDirs(p);
    VSIMkdir(p, 0744);
}

// Warns if it fails to write, but returns success
CPLErr GDALWMSCache::Write(const char *key, const CPLString &file_name) {
    CPLString cache_file(KeyToCacheFile(key));
    // printf("GDALWMSCache::Write(%s, %s) -> %s\n", key, file_name.c_str());
    if (CPLCopyFile(cache_file, file_name) == CE_None)
        return CE_None;
    MakeDirs(cache_file.c_str());
    if (CPLCopyFile(cache_file, file_name) == CE_None)
        return CE_None;
    // Warn if it fails after folder creation
    CPLError(CE_Warning, CPLE_FileIO, "Error writing to WMS cache %s", m_cache_path.c_str());
    return CE_None;
}

CPLErr GDALWMSCache::Read(const char *key, CPLString *file_name) {
    CPLErr ret = CE_Failure;
    CPLString cache_file(KeyToCacheFile(key));
    VSILFILE* fp = VSIFOpenL(cache_file.c_str(), "rb");
    if (fp != NULL)
    {
        VSIFCloseL(fp);
        *file_name = cache_file;
        ret = CE_None;
    }

    return ret;
}

CPLString GDALWMSCache::KeyToCacheFile(const char *key) {
    CPLString hash(MD5String(key));
    CPLString cache_file(m_cache_path);

    if (!cache_file.empty() && cache_file.back() != '/')
        cache_file.append(1, '/');
    for (int i = 0; i < m_cache_depth; ++i) {
        cache_file.append(1, hash[i]);
        cache_file.append(1, '/');
    }
    cache_file.append(hash);
    cache_file.append(m_postfix);
    return cache_file;
}
