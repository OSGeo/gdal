/***************************************************************************
  gdal_subdatasetinfo.cpp - GDALSubdatasetInfo

 ---------------------
 begin                : 21.7.2023
 copyright            : (C) 2023 by Alessndro Pasotti
 email                : elpaso@itopen.it
 ***************************************************************************
 *                                                                         *
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
 *                                                                         *
 ***************************************************************************/
#include "gdalsubdatasetinfo.h"
#include "gdal_priv.h"
#include <algorithm>
#include <stdexcept>

/************************************************************************/
/*                     Subdataset informational functions               */
/************************************************************************/

GDALSubdatasetInfoH GDALGetSubdatasetInfo(const char *pszFileName)
{
    // Iterate all drivers
    GDALDriverManager *poDM_ = GetGDALDriverManager();
    const int nDriverCount = poDM_->GetDriverCount();
    for (int iDriver = 0; iDriver < nDriverCount; ++iDriver)
    {
        GDALDriver *poDriver = poDM_->GetDriver(iDriver);
        if (!poDriver->pfnGetSubdatasetInfoFunc)
        {
            continue;
        }
        const char *pszDMDSubdatasets =
            GDALGetMetadataItem(poDriver, GDAL_DMD_SUBDATASETS, nullptr);
        if (!pszDMDSubdatasets || !CPLTestBool(pszDMDSubdatasets))
        {
            continue;
        }

        GDALSubdatasetInfo *poGetSubdatasetInfo =
            poDriver->pfnGetSubdatasetInfoFunc(pszFileName);

        if (!poGetSubdatasetInfo)
        {
            continue;
        }

        return static_cast<GDALSubdatasetInfoH>(poGetSubdatasetInfo);
    }
    return nullptr;
}

/************************************************************************/
/*                       GDALDestroySubdatasetInfo()                    */
/************************************************************************/

/**
 * \brief Destroys subdataset info.
 *
 * This function is the same as the C++ method GDALSubdatasetInfo::~GDALSubdatasetInfo()
 */
void GDALDestroySubdatasetInfo(GDALSubdatasetInfoH hInfo)

{
    delete hInfo;
}

char *GDALSubdatasetInfoGetPathComponent(GDALSubdatasetInfoH hInfo)
{
    return CPLStrdup(hInfo->GetPathComponent().c_str());
}

char *GDALSubdatasetInfoGetSubdatasetComponent(GDALSubdatasetInfoH hInfo)
{
    return CPLStrdup(hInfo->GetSubdatasetComponent().c_str());
}

char *GDALSubdatasetInfoModifyPathComponent(GDALSubdatasetInfoH hInfo,
                                            const char *pszNewFileName)
{
    return CPLStrdup(hInfo->ModifyPathComponent(pszNewFileName).c_str());
}

GDALSubdatasetInfo::GDALSubdatasetInfo(const std::string &fileName)
    : m_fileName(fileName), m_pathComponent(), m_cleanedPathComponent(),
      m_subdatasetComponent(), m_driverPrefixComponent()
{
}

std::string GDALSubdatasetInfo::GetPathComponent() const
{
    init();
    return m_cleanedPathComponent;
}

std::string
GDALSubdatasetInfo::ModifyPathComponent(const std::string &newPathName) const
{
    init();

    std::string replaced{m_fileName};

    try
    {
        auto newPathName_{newPathName};
        if (m_isQuoted)
        {
            if (newPathName_.length() >= 2 && newPathName_.at(0) != '"' &&
                newPathName_.at(newPathName_.length() - 1) != '"')
            {
                newPathName_ = quote(newPathName_);
            }
        }
        replaced.replace(replaced.find(m_pathComponent),
                         m_pathComponent.length(), newPathName_);
    }
    catch (const std::out_of_range &)
    {
        return "";
    }

    return replaced;
}

std::string GDALSubdatasetInfo::GetSubdatasetComponent() const
{
    init();
    return m_subdatasetComponent;
}

//! @cond Doxygen_Suppress
std::string GDALSubdatasetInfo::unquote(const std::string &path)
{
    if (path.length() >= 2)
    {
        std::string cleanedPath{path};
        if (cleanedPath.at(0) == '"' &&
            cleanedPath.at(cleanedPath.length() - 1) == '"')
        {
            cleanedPath = cleanedPath.substr(1, cleanedPath.length() - 2);
            while (cleanedPath.find(R"(\")") != std::string::npos)
            {
                const auto pos{cleanedPath.find(R"(\")")};
                if (pos == 0 || cleanedPath.at(pos - 1) != '\\')
                {
                    cleanedPath.erase(pos, 1);
                }
            }
            return cleanedPath;
        }
    }
    return path;
}

std::string GDALSubdatasetInfo::quote(const std::string &path)
{
    std::string quotedPath{'"'};
    for (size_t i = 0; i < path.length(); ++i)
    {
        if (path.at(i) == '"')
        {
            quotedPath += R"(\")";
        }
        else
        {
            quotedPath += path.at(i);
        }
    }
    return quotedPath + '"';
}

void GDALSubdatasetInfo::init() const
{
    if (!m_initialized)
    {
        GDALSubdatasetInfo *this_ = const_cast<GDALSubdatasetInfo *>(this);
        this_->parseFileName();
        this_->m_isQuoted =
            m_pathComponent.length() >= 2 && m_pathComponent.at(0) == '"' &&
            m_pathComponent.at(m_pathComponent.length() - 1) == '"';
        this_->m_cleanedPathComponent =
            m_isQuoted ? unquote(m_pathComponent) : m_pathComponent;
        m_initialized = true;
    }
}

//! @endcond
