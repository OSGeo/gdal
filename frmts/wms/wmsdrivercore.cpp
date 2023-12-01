/******************************************************************************
 *
 * Project:  WMS Client Driver
 * Purpose:  Implementation of Dataset and RasterBand classes for WMS
 *           and other similar services.
 * Author:   Adam Nowacki, nowak@xpam.de
 *
 ******************************************************************************
 * Copyright (c) 2007, Adam Nowacki
 * Copyright (c) 2009-2014, Even Rouault <even dot rouault at spatialys.com>
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

#include "wmsdrivercore.h"

/************************************************************************/
/*                     WMSDriverIdentify()                              */
/************************************************************************/

int WMSDriverIdentify(GDALOpenInfo *poOpenInfo)

{
    const char *pszFilename = poOpenInfo->pszFilename;
    const char *pabyHeader = (const char *)poOpenInfo->pabyHeader;
    if (poOpenInfo->nHeaderBytes == 0 &&
        STARTS_WITH_CI(pszFilename, "<GDAL_WMS>"))
    {
        return TRUE;
    }
    else if (poOpenInfo->nHeaderBytes >= 10 &&
             STARTS_WITH_CI(pabyHeader, "<GDAL_WMS>"))
    {
        return TRUE;
    }
    else if (poOpenInfo->nHeaderBytes == 0 &&
             (STARTS_WITH_CI(pszFilename, "WMS:") ||
              CPLString(pszFilename).ifind("SERVICE=WMS") != std::string::npos))
    {
        return TRUE;
    }
    else if (poOpenInfo->nHeaderBytes != 0 &&
             (strstr(pabyHeader, "<WMT_MS_Capabilities") != nullptr ||
              strstr(pabyHeader, "<WMS_Capabilities") != nullptr ||
              strstr(pabyHeader, "<!DOCTYPE WMT_MS_Capabilities") != nullptr))
    {
        return TRUE;
    }
    else if (poOpenInfo->nHeaderBytes != 0 &&
             strstr(pabyHeader, "<WMS_Tile_Service") != nullptr)
    {
        return TRUE;
    }
    else if (poOpenInfo->nHeaderBytes != 0 &&
             strstr(pabyHeader, "<TileMap version=\"1.0.0\"") != nullptr)
    {
        return TRUE;
    }
    else if (poOpenInfo->nHeaderBytes != 0 &&
             strstr(pabyHeader, "<Services") != nullptr &&
             strstr(pabyHeader, "<TileMapService version=\"1.0") != nullptr)
    {
        return TRUE;
    }
    else if (poOpenInfo->nHeaderBytes != 0 &&
             strstr(pabyHeader, "<TileMapService version=\"1.0.0\"") != nullptr)
    {
        return TRUE;
    }
    else if (poOpenInfo->nHeaderBytes == 0 &&
             STARTS_WITH_CI(pszFilename, "http") &&
             (strstr(pszFilename, "/MapServer?f=json") != nullptr ||
              strstr(pszFilename, "/MapServer/?f=json") != nullptr ||
              strstr(pszFilename, "/ImageServer?f=json") != nullptr ||
              strstr(pszFilename, "/ImageServer/?f=json") != nullptr))
    {
        return TRUE;
    }
    else if (poOpenInfo->nHeaderBytes == 0 &&
             STARTS_WITH_CI(pszFilename, "AGS:"))
    {
        return TRUE;
    }
    else if (poOpenInfo->nHeaderBytes == 0 &&
             STARTS_WITH_CI(pszFilename, "IIP:"))
    {
        return TRUE;
    }
    else
        return FALSE;
}

/************************************************************************/
/*                    OGRWMSDriverGetSubdatasetInfo()                   */
/************************************************************************/

struct WMSDriverSubdatasetInfo : public GDALSubdatasetInfo
{
  public:
    explicit WMSDriverSubdatasetInfo(const std::string &fileName)
        : GDALSubdatasetInfo(fileName)
    {
    }

    // GDALSubdatasetInfo interface
  private:
    void parseFileName() override
    {
        if (!STARTS_WITH_CI(m_fileName.c_str(), "WMS:"))
        {
            return;
        }

        const CPLString osLayers = CPLURLGetValue(m_fileName.c_str(), "LAYERS");

        if (!osLayers.empty())
        {
            m_subdatasetComponent = "LAYERS=" + osLayers;
            m_driverPrefixComponent = "WMS";

            m_pathComponent = m_fileName;
            m_pathComponent.erase(m_pathComponent.find(m_subdatasetComponent),
                                  m_subdatasetComponent.length());
            m_pathComponent.erase(0, 4);
            const std::size_t nDoubleAndPos = m_pathComponent.find("&&");
            if (nDoubleAndPos != std::string::npos)
            {
                m_pathComponent.erase(nDoubleAndPos, 1);
            }
            // Reconstruct URL with LAYERS at the end or ModifyPathComponent will fail
            m_fileName = m_driverPrefixComponent + ":" + m_pathComponent + "&" +
                         m_subdatasetComponent;
        }
    }
};

static GDALSubdatasetInfo *WMSDriverGetSubdatasetInfo(const char *pszFileName)
{
    if (STARTS_WITH(pszFileName, "WMS:"))
    {
        std::unique_ptr<GDALSubdatasetInfo> info =
            std::make_unique<WMSDriverSubdatasetInfo>(pszFileName);
        if (!info->GetSubdatasetComponent().empty() &&
            !info->GetPathComponent().empty())
        {
            return info.release();
        }
    }
    return nullptr;
}

/************************************************************************/
/*                      WMSDriverSetCommonMetadata()                    */
/************************************************************************/

void WMSDriverSetCommonMetadata(GDALDriver *poDriver)
{
    poDriver->SetDescription(DRIVER_NAME);
    poDriver->SetMetadataItem(GDAL_DCAP_RASTER, "YES");
    poDriver->SetMetadataItem(GDAL_DMD_LONGNAME, "OGC Web Map Service");
    poDriver->SetMetadataItem(GDAL_DMD_HELPTOPIC, "drivers/raster/wms.html");
    poDriver->SetMetadataItem(GDAL_DCAP_VIRTUALIO, "YES");
    poDriver->SetMetadataItem(GDAL_DMD_SUBDATASETS, "YES");

    poDriver->pfnIdentify = WMSDriverIdentify;
    poDriver->pfnGetSubdatasetInfoFunc = WMSDriverGetSubdatasetInfo;
    poDriver->SetMetadataItem(GDAL_DCAP_OPEN, "YES");
    poDriver->SetMetadataItem(GDAL_DCAP_CREATECOPY, "YES");
}

/************************************************************************/
/*                     DeclareDeferredWMSPlugin()                       */
/************************************************************************/

#ifdef PLUGIN_FILENAME
void DeclareDeferredWMSPlugin()
{
    if (GDALGetDriverByName(DRIVER_NAME) != nullptr)
    {
        return;
    }
    auto poDriver = new GDALPluginDriverProxy(PLUGIN_FILENAME);
#ifdef PLUGIN_INSTALLATION_MESSAGE
    poDriver->SetMetadataItem(GDAL_DMD_PLUGIN_INSTALLATION_MESSAGE,
                              PLUGIN_INSTALLATION_MESSAGE);
#endif
    WMSDriverSetCommonMetadata(poDriver);
    GetGDALDriverManager()->DeclareDeferredPluginDriver(poDriver);
}
#endif
