/******************************************************************************
 * File :    PostGISRasterDriver.cpp
 * Project:  PostGIS Raster driver
 * Purpose:  Implements PostGIS Raster driver class methods
 * Author:   Jorge Arevalo, jorge.arevalo@deimos-space.com
 *
 * Last changes: $Id$
 *
 ******************************************************************************
 * Copyright (c) 2010, Jorge Arevalo, jorge.arevalo@deimos-space.com
 * Copyright (c) 2013, Even Rouault
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
 ******************************************************************************/

#include <stdexcept>

#include "postgisrasterdrivercore.h"

/************************************************************************/
/*                     PostGISRasterDriverIdentify()                    */
/************************************************************************/

int PostGISRasterDriverIdentify(GDALOpenInfo *poOpenInfo)

{
    if (poOpenInfo->pszFilename == nullptr || poOpenInfo->fpL != nullptr ||
        !STARTS_WITH_CI(poOpenInfo->pszFilename, "PG:"))
    {
        return FALSE;
    }

    // Will avoid a OGR PostgreSQL connection string to be recognized as a
    // PostgisRaster one and later fail (#6034)
    if (strstr(poOpenInfo->pszFilename, " schemas=") ||
        strstr(poOpenInfo->pszFilename, " SCHEMAS="))
    {
        return FALSE;
    }

    return TRUE;
}

/**************************************************************
 * \brief Replace the single quotes by " in the input string
 *
 * Needed before tokenize function
 *************************************************************/
static char *ReplaceSingleQuotes(const char *pszInput, int nLength)
{
    int i;
    char *pszOutput = nullptr;

    if (nLength == -1)
        nLength = static_cast<int>(strlen(pszInput));

    pszOutput = static_cast<char *>(CPLCalloc(nLength + 1, sizeof(char)));

    for (i = 0; i < nLength; i++)
    {
        if (pszInput[i] == '\'')
            pszOutput[i] = '"';
        else
            pszOutput[i] = pszInput[i];
    }

    return pszOutput;
}

/***********************************************************************
 * \brief Split connection string into user, password, host, database...
 *
 * The parameters separated by spaces are return as a list of strings.
 * The function accepts all the PostgreSQL recognized parameter keywords.
 *
 * The returned list must be freed with CSLDestroy when no longer needed
 **********************************************************************/
char **PostGISRasterParseConnectionString(const char *pszConnectionString)
{

    /* Escape string following SQL scheme */
    char *pszEscapedConnectionString =
        ReplaceSingleQuotes(pszConnectionString, -1);

    /* Avoid PG: part */
    char *pszStartPos = strstr(pszEscapedConnectionString, ":") + 1;

    /* Tokenize */
    char **papszParams =
        CSLTokenizeString2(pszStartPos, " ", CSLT_HONOURSTRINGS);

    /* Free */
    CPLFree(pszEscapedConnectionString);

    return papszParams;
}

/************************************************************************/
/*                    PostGISRasterDriverGetSubdatasetInfo()            */
/************************************************************************/

struct PostGISRasterDriverSubdatasetInfo : public GDALSubdatasetInfo
{
  public:
    explicit PostGISRasterDriverSubdatasetInfo(const std::string &fileName)
        : GDALSubdatasetInfo(fileName)
    {
    }

    // GDALSubdatasetInfo interface
  private:
    void parseFileName() override
    {
        if (!STARTS_WITH_CI(m_fileName.c_str(), "PG:"))
        {
            return;
        }

        char **papszParams =
            PostGISRasterParseConnectionString(m_fileName.c_str());

        const int nTableIdx = CSLFindName(papszParams, "table");
        if (nTableIdx != -1)
        {
            size_t nTableStart = m_fileName.find("table=");
            bool bHasQuotes{false};
            try
            {
                bHasQuotes = m_fileName.at(nTableStart + 6) == '\'';
            }
            catch (const std::out_of_range &)
            {
                // ignore error
            }

            m_subdatasetComponent = papszParams[nTableIdx];

            if (bHasQuotes)
            {
                m_subdatasetComponent.insert(6, "'");
                m_subdatasetComponent.push_back('\'');
            }

            m_driverPrefixComponent = "PG";

            size_t nPathLength = m_subdatasetComponent.length();
            if (nTableStart != 0)
            {
                nPathLength++;
                nTableStart--;
            }

            m_pathComponent = m_fileName;
            m_pathComponent.erase(nTableStart, nPathLength);
            m_pathComponent.erase(0, 3);
        }

        CSLDestroy(papszParams);
    }
};

static GDALSubdatasetInfo *
PostGISRasterDriverGetSubdatasetInfo(const char *pszFileName)
{
    if (STARTS_WITH_CI(pszFileName, "PG:"))
    {
        std::unique_ptr<GDALSubdatasetInfo> info =
            std::make_unique<PostGISRasterDriverSubdatasetInfo>(pszFileName);
        if (!info->GetSubdatasetComponent().empty() &&
            !info->GetPathComponent().empty())
        {
            return info.release();
        }
    }
    return nullptr;
}

/************************************************************************/
/*                PostGISRasterDriverSetCommonMetadata()                */
/************************************************************************/

void PostGISRasterDriverSetCommonMetadata(GDALDriver *poDriver)
{
    poDriver->SetDescription(DRIVER_NAME);
    poDriver->SetMetadataItem(GDAL_DCAP_RASTER, "YES");
    poDriver->SetMetadataItem(GDAL_DMD_LONGNAME, "PostGIS Raster driver");
    poDriver->SetMetadataItem(GDAL_DMD_SUBDATASETS, "YES");

    poDriver->pfnIdentify = PostGISRasterDriverIdentify;
    poDriver->pfnGetSubdatasetInfoFunc = PostGISRasterDriverGetSubdatasetInfo;

    poDriver->SetMetadataItem(GDAL_DCAP_OPEN, "YES");
    poDriver->SetMetadataItem(GDAL_DCAP_CREATECOPY, "YES");
}

/************************************************************************/
/*                   DeclareDeferredPostGISRasterPlugin()               */
/************************************************************************/

#ifdef PLUGIN_FILENAME
void DeclareDeferredPostGISRasterPlugin()
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
    PostGISRasterDriverSetCommonMetadata(poDriver);
    GetGDALDriverManager()->DeclareDeferredPluginDriver(poDriver);
}
#endif
