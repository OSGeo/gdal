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

/************************************************************************/
/*                     Subdataset informational functions               */
/************************************************************************/

GDALSubdatasetInfoH CPL_STDCALL GDALGetSubdatasetInfo(const char *pszFileName)
{
    // Iterate all drivers
    GDALDriverManager *poDM_ = GetGDALDriverManager();
    const int nDriverCount = poDM_->GetDriverCount();
    for (int iDriver = 0; iDriver < nDriverCount; ++iDriver)
    {
        GDALDriver *poDriver = poDM_->GetDriver(iDriver);
        char **papszMD = GDALGetMetadata(poDriver, nullptr);
        if (!CPLFetchBool(papszMD, GDAL_DMD_SUBDATASETS, false) ||
            !poDriver->pfnGetSubdatasetInfoFunc)
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

char *GDALSubdatasetInfoGetFileName(GDALSubdatasetInfoH hInfo)
{
    return CPLStrdup(hInfo->GetFileName().c_str());
}

char *GDALSubdatasetInfoGetSubdatasetName(GDALSubdatasetInfoH hInfo)
{
    return CPLStrdup(hInfo->GetSubdatasetName().c_str());
}

char *GDALSubdatasetInfoModifyFileName(GDALSubdatasetInfoH hInfo,
                                       const char *pszNewFileName)
{
    return CPLStrdup(hInfo->ModifyFileName(pszNewFileName).c_str());
}

GDALSubdatasetInfo::GDALSubdatasetInfo(const std::string &fileName)
    : m_fileName(fileName)
{
}

std::string GDALSubdatasetInfo::GetFileName() const
{
    if (!m_initialized)
    {
        GDALSubdatasetInfo *this_ = const_cast<GDALSubdatasetInfo *>(this);
        this_->parseFileName();
        m_initialized = true;
    }
    return m_driverPrefixComponent + ":" + m_baseComponent;
}

std::string
GDALSubdatasetInfo::ModifyFileName(const std::string &newFileName) const
{
    if (!m_initialized)
    {
        GDALSubdatasetInfo *this_ = const_cast<GDALSubdatasetInfo *>(this);
        this_->parseFileName();
        m_initialized = true;
    }

    std::string replaced{m_fileName};
    replaced.replace(replaced.find(m_baseComponent), m_baseComponent.length(),
                     newFileName);
    return replaced;
}

std::string GDALSubdatasetInfo::GetSubdatasetName() const
{
    if (!m_initialized)
    {
        GDALSubdatasetInfo *this_ = const_cast<GDALSubdatasetInfo *>(this);
        this_->parseFileName();
        m_initialized = true;
    }
    return m_subdatasetComponent;
}
