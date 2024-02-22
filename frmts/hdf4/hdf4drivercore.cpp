/******************************************************************************
 *
 * Project:  GDAL
 * Purpose:  HDF4 driver
 * Author:   Even Rouault, <even.rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2023, Even Rouault, <even.rouault at spatialys.com>
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

#include "hdf4drivercore.h"

#include <cctype>

/************************************************************************/
/*                              Identify()                              */
/************************************************************************/

int HDF4DatasetIdentify(GDALOpenInfo *poOpenInfo)

{
    if (poOpenInfo->nHeaderBytes < 4)
        return FALSE;

    if (memcmp(poOpenInfo->pabyHeader, "\016\003\023\001", 4) != 0)
        return FALSE;

    return TRUE;
}

/************************************************************************/
/*                    HDF4DriverGetSubdatasetInfo()                     */
/************************************************************************/

struct HDF4DriverSubdatasetInfo : public GDALSubdatasetInfo
{
  public:
    explicit HDF4DriverSubdatasetInfo(const std::string &fileName)
        : GDALSubdatasetInfo(fileName)
    {
    }

    // GDALSubdatasetInfo interface
  private:
    void parseFileName() override
    {

        if (!STARTS_WITH_CI(m_fileName.c_str(), "HDF4_SDS:") &&
            !STARTS_WITH_CI(m_fileName.c_str(), "HDF4_EOS:"))
        {
            return;
        }

        CPLStringList aosParts{CSLTokenizeString2(m_fileName.c_str(), ":", 0)};
        const int iPartsCount{CSLCount(aosParts)};

        if (iPartsCount >= 3)
        {

            // prefix + mode
            m_driverPrefixComponent = aosParts[0];
            m_driverPrefixComponent.append(":");
            m_driverPrefixComponent.append(aosParts[1]);

            int subdatasetIndex{3};

            if (iPartsCount >= 4)
            {
                const bool hasDriveLetter{
                    (strlen(aosParts[3]) > 1 &&
                     (aosParts[3][0] == '\\' || aosParts[3][0] == '/')) &&
                    ((strlen(aosParts[2]) == 2 &&
                      std::isalpha(
                          static_cast<unsigned char>(aosParts[2][1]))) ||
                     (strlen(aosParts[2]) == 1 &&
                      std::isalpha(
                          static_cast<unsigned char>(aosParts[2][0]))))};
                m_pathComponent = aosParts[2];

                const bool hasProtocol{m_pathComponent.find("/vsicurl/") !=
                                       std::string::npos};

                if (hasDriveLetter || hasProtocol)
                {
                    m_pathComponent.append(":");
                    m_pathComponent.append(aosParts[3]);
                    subdatasetIndex++;
                }
            }

            if (iPartsCount > subdatasetIndex)
            {
                m_subdatasetComponent = aosParts[subdatasetIndex];

                // Append any remaining part
                for (int i = subdatasetIndex + 1; i < iPartsCount; ++i)
                {
                    m_subdatasetComponent.append(":");
                    m_subdatasetComponent.append(aosParts[i]);
                }
            }
        }
    }
};

static GDALSubdatasetInfo *HDF4DriverGetSubdatasetInfo(const char *pszFileName)
{
    if (STARTS_WITH_CI(pszFileName, "HDF4_SDS:") ||
        STARTS_WITH_CI(pszFileName, "HDF4_EOS:"))
    {
        std::unique_ptr<GDALSubdatasetInfo> info =
            std::make_unique<HDF4DriverSubdatasetInfo>(pszFileName);
        if (!info->GetSubdatasetComponent().empty() &&
            !info->GetPathComponent().empty())
        {
            return info.release();
        }
    }
    return nullptr;
}

/************************************************************************/
/*                   HDF4DriverSetCommonMetadata()                      */
/************************************************************************/

void HDF4DriverSetCommonMetadata(GDALDriver *poDriver)
{
    poDriver->SetDescription(HDF4_DRIVER_NAME);
    poDriver->SetMetadataItem(GDAL_DCAP_RASTER, "YES");
    poDriver->SetMetadataItem(GDAL_DMD_LONGNAME,
                              "Hierarchical Data Format Release 4");
    poDriver->SetMetadataItem(GDAL_DMD_HELPTOPIC, "drivers/raster/hdf4.html");
    poDriver->SetMetadataItem(GDAL_DMD_EXTENSION, "hdf");
    poDriver->SetMetadataItem(GDAL_DMD_SUBDATASETS, "YES");

    poDriver->SetMetadataItem(GDAL_DCAP_MULTIDIM_RASTER, "YES");

    poDriver->SetMetadataItem(
        GDAL_DMD_OPENOPTIONLIST,
        "<OpenOptionList>"
        "  <Option name='LIST_SDS' type='string-select' "
        "description='Whether to report Scientific Data Sets' default='AUTO'>"
        "       <Value>AUTO</Value>"
        "       <Value>YES</Value>"
        "       <Value>NO</Value>"
        "  </Option>"
        "</OpenOptionList>");

    poDriver->pfnIdentify = HDF4DatasetIdentify;
    poDriver->SetMetadataItem(GDAL_DCAP_OPEN, "YES");
    poDriver->pfnGetSubdatasetInfoFunc = HDF4DriverGetSubdatasetInfo;
}

/************************************************************************/
/*                     HDF4ImageDatasetIdentify()                       */
/************************************************************************/

int HDF4ImageDatasetIdentify(GDALOpenInfo *poOpenInfo)
{
    if (!STARTS_WITH_CI(poOpenInfo->pszFilename, "HDF4_SDS:") &&
        !STARTS_WITH_CI(poOpenInfo->pszFilename, "HDF4_GR:") &&
        !STARTS_WITH_CI(poOpenInfo->pszFilename, "HDF4_GD:") &&
        !STARTS_WITH_CI(poOpenInfo->pszFilename, "HDF4_EOS:"))
        return false;
    return true;
}

/************************************************************************/
/*                 HDF4ImageDriverSetCommonMetadata()                   */
/************************************************************************/

void HDF4ImageDriverSetCommonMetadata(GDALDriver *poDriver)
{
    poDriver->SetDescription(HDF4_IMAGE_DRIVER_NAME);
    poDriver->SetMetadataItem(GDAL_DCAP_RASTER, "YES");
    poDriver->SetMetadataItem(GDAL_DMD_LONGNAME, "HDF4 Dataset");
    poDriver->SetMetadataItem(GDAL_DMD_HELPTOPIC, "drivers/raster/hdf4.html");
    poDriver->SetMetadataItem(GDAL_DMD_CREATIONDATATYPES,
                              "Byte Int8 Int16 UInt16 Int32 UInt32 "
                              // "Int64 UInt64 "
                              "Float32 Float64");
    poDriver->SetMetadataItem(
        GDAL_DMD_CREATIONOPTIONLIST,
        "<CreationOptionList>"
        "   <Option name='RANK' type='int' description='Rank of output SDS'/>"
        "</CreationOptionList>");

    poDriver->SetMetadataItem(GDAL_DCAP_OPEN, "YES");
    poDriver->pfnIdentify = HDF4ImageDatasetIdentify;
    poDriver->SetMetadataItem(GDAL_DCAP_CREATE, "YES");
}

/************************************************************************/
/*                    DeclareDeferredHDF4Plugin()                       */
/************************************************************************/

#ifdef PLUGIN_FILENAME
void DeclareDeferredHDF4Plugin()
{
    if (GDALGetDriverByName(HDF4_DRIVER_NAME) != nullptr)
    {
        return;
    }
    {
        auto poDriver = new GDALPluginDriverProxy(PLUGIN_FILENAME);
#ifdef PLUGIN_INSTALLATION_MESSAGE
        poDriver->SetMetadataItem(GDAL_DMD_PLUGIN_INSTALLATION_MESSAGE,
                                  PLUGIN_INSTALLATION_MESSAGE);
#endif
        HDF4DriverSetCommonMetadata(poDriver);
        GetGDALDriverManager()->DeclareDeferredPluginDriver(poDriver);
    }
    {
        auto poDriver = new GDALPluginDriverProxy(PLUGIN_FILENAME);
#ifdef PLUGIN_INSTALLATION_MESSAGE
        poDriver->SetMetadataItem(GDAL_DMD_PLUGIN_INSTALLATION_MESSAGE,
                                  PLUGIN_INSTALLATION_MESSAGE);
#endif
        HDF4ImageDriverSetCommonMetadata(poDriver);
        GetGDALDriverManager()->DeclareDeferredPluginDriver(poDriver);
    }
}
#endif
