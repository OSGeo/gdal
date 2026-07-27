/******************************************************************************
 *
 * Project:  GDAL
 * Purpose:  HDF4 driver
 * Author:   Even Rouault, <even.rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2023, Even Rouault, <even.rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include "hdf4drivercore.h"

#include "gdal_frmts.h"
#include "gdalplugindriverproxy.h"

#include <cctype>

#include "gdalsubdatasetinfo.h"

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

struct HDF4DriverSubdatasetInfo final : public GDALSubdatasetInfo
{
  public:
    explicit HDF4DriverSubdatasetInfo(const std::string &fileName)
        : GDALSubdatasetInfo(fileName)
    {
    }

    // GDALSubdatasetInfo interface
  private:
    void parseFileName() override;
};

void HDF4DriverSubdatasetInfo::parseFileName()
{

    if (!STARTS_WITH_CI(m_fileName.c_str(), "HDF4_SDS:") &&
        !STARTS_WITH_CI(m_fileName.c_str(), "HDF4_EOS:"))
    {
        return;
    }

    CPLStringList aosParts{CSLTokenizeString2(m_fileName.c_str(), ":", 0)};
    const int iPartsCount{CSLCount(aosParts)};

    auto unescapeDoubleQuotes = [](std::string &str)
    {
        size_t pos = 0;
        while ((pos = str.find("\\\"", pos)) != std::string::npos)
        {
            str.replace(pos, 2, "\"");
            pos += 1;
        }
    };

    if (iPartsCount >= 3)
    {

        // prefix + mode
        m_driverPrefixComponent = aosParts[0];
        m_driverPrefixComponent.append(":");
        m_driverPrefixComponent.append(aosParts[1]);

        int subdatasetIndex{3};

        if (iPartsCount >= 4)
        {

            std::string part2{aosParts[2]};
            const bool pathIsDoubleQuoted{!part2.empty() && part2[0] == '"'};

            if (pathIsDoubleQuoted)
            {
                // The path component is everything up to the next double quote.
                if (part2.back() != '"')
                {
                    // If the path component is double quoted and there is no closing quote, then the path component
                    // is everything up to the next part that ends with an unescaped double quote.
                    // This is to handle cases where the path component contains colons.
                    for (int i = 3; i < iPartsCount; ++i)
                    {
                        const size_t partLen = strlen(aosParts[i]);
                        if (partLen > 0 && aosParts[i][partLen - 1] == '"' &&
                            !(partLen > 1 && aosParts[i][partLen - 2] == '\\'))
                        {
                            part2.append(":");
                            part2.append(std::string(aosParts[i]));
                            subdatasetIndex = i + 1;
                            break;
                        }
                        part2.append(":");
                        part2.append(std::string(aosParts[i]));
                    }
                }
                unescapeDoubleQuotes(part2);
            }

            m_pathComponent = part2;

            const bool hasDriveLetter{
                (strlen(aosParts[3]) > 1 &&
                 (aosParts[3][0] == '\\' || aosParts[3][0] == '/')) &&
                ((strlen(aosParts[2]) == 2 &&
                  std::isalpha(static_cast<unsigned char>(aosParts[2][1]))) ||
                 (strlen(aosParts[2]) == 1 &&
                  std::isalpha(static_cast<unsigned char>(aosParts[2][0]))))};

            const bool hasProtocol{m_pathComponent.find("/vsicurl/") !=
                                   std::string::npos};

            if (!pathIsDoubleQuoted && (hasDriveLetter || hasProtocol))
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
/*                    HDF4DriverSetCommonMetadata()                     */
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
/*                      HDF4ImageDatasetIdentify()                      */
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
/*                  HDF4ImageDriverSetCommonMetadata()                  */
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
/*                     DeclareDeferredHDF4Plugin()                      */
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
