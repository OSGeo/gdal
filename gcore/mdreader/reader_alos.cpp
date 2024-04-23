/******************************************************************************
 *
 * Project:  GDAL Core
 * Purpose:  Read metadata from Alos imagery.
 * Author:   Alexander Lisovenko
 * Author:   Dmitry Baryshnikov, polimax@mail.ru
 *
 ******************************************************************************
 * Copyright (c) 2014-2015 NextGIS <info@nextgis.ru>
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

#include "reader_alos.h"

#include <cstdio>
#include <cstdlib>

#include <string>

#include "cpl_conv.h"
#include "cpl_error.h"
#include "cpl_string.h"
#include "cpl_time.h"
#include "gdal_mdreader.h"

/**
 * GDALMDReaderALOS()
 */
GDALMDReaderALOS::GDALMDReaderALOS(const char *pszPath,
                                   char **papszSiblingFiles)
    : GDALMDReaderBase(pszPath, papszSiblingFiles)
{
    CPLString osDirName = CPLGetDirname(pszPath);
    CPLString osBaseName = CPLGetBasename(pszPath);

    CPLString osIMDSourceFilename =
        CPLFormFilename(osDirName, "summary", ".txt");
    if (CPLCheckForFile(&osIMDSourceFilename[0], papszSiblingFiles))
    {
        m_osIMDSourceFilename = std::move(osIMDSourceFilename);
    }
    else
    {
        osIMDSourceFilename = CPLFormFilename(osDirName, "SUMMARY", ".TXT");
        if (CPLCheckForFile(&osIMDSourceFilename[0], papszSiblingFiles))
        {
            m_osIMDSourceFilename = std::move(osIMDSourceFilename);
        }
    }

    if (osBaseName.size() >= 6)
    {
        // check if this is separate band or whole image
        // test without 6 symbols
        CPLString osHDRFileName = CPLFormFilename(
            osDirName, CPLSPrintf("HDR%s", osBaseName + 6), "txt");
        if (CPLCheckForFile(&osHDRFileName[0], papszSiblingFiles))
        {
            m_osHDRSourceFilename = std::move(osHDRFileName);
        }
        else
        {
            osHDRFileName = CPLFormFilename(
                osDirName, CPLSPrintf("HDR%s", osBaseName + 6), "TXT");
            if (CPLCheckForFile(&osHDRFileName[0], papszSiblingFiles))
            {
                m_osHDRSourceFilename = std::move(osHDRFileName);
            }
        }
    }

    // test without 3 symbols
    if (osBaseName.size() >= 3 && m_osHDRSourceFilename.empty())
    {
        CPLString osHDRFileName = CPLFormFilename(
            osDirName, CPLSPrintf("HDR%s", osBaseName + 3), "txt");
        if (CPLCheckForFile(&osHDRFileName[0], papszSiblingFiles))
        {
            m_osHDRSourceFilename = std::move(osHDRFileName);
        }
        else
        {
            osHDRFileName = CPLFormFilename(
                osDirName, CPLSPrintf("HDR%s", osBaseName + 3), "TXT");
            if (CPLCheckForFile(&osHDRFileName[0], papszSiblingFiles))
            {
                m_osHDRSourceFilename = std::move(osHDRFileName);
            }
        }
    }

    // test without 6 symbols
    if (osBaseName.size() >= 6)
    {
        CPLString osRPCFileName = CPLFormFilename(
            osDirName, CPLSPrintf("RPC%s", osBaseName + 6), "txt");
        if (CPLCheckForFile(&osRPCFileName[0], papszSiblingFiles))
        {
            m_osRPBSourceFilename = std::move(osRPCFileName);
        }
        else
        {
            osRPCFileName = CPLFormFilename(
                osDirName, CPLSPrintf("RPC%s", osBaseName + 6), "TXT");
            if (CPLCheckForFile(&osRPCFileName[0], papszSiblingFiles))
            {
                m_osRPBSourceFilename = std::move(osRPCFileName);
            }
        }
    }

    // test without 3 symbols
    if (osBaseName.size() >= 3 && m_osRPBSourceFilename.empty())
    {
        CPLString osRPCFileName = CPLFormFilename(
            osDirName, CPLSPrintf("RPC%s", osBaseName + 3), "txt");
        if (CPLCheckForFile(&osRPCFileName[0], papszSiblingFiles))
        {
            m_osRPBSourceFilename = std::move(osRPCFileName);
        }
        else
        {
            osRPCFileName = CPLFormFilename(
                osDirName, CPLSPrintf("RPC%s", osBaseName + 3), "TXT");
            if (CPLCheckForFile(&osRPCFileName[0], papszSiblingFiles))
            {
                m_osRPBSourceFilename = std::move(osRPCFileName);
            }
        }
    }

    if (!m_osIMDSourceFilename.empty())
        CPLDebug("MDReaderALOS", "IMD Filename: %s",
                 m_osIMDSourceFilename.c_str());
    if (!m_osHDRSourceFilename.empty())
        CPLDebug("MDReaderALOS", "HDR Filename: %s",
                 m_osHDRSourceFilename.c_str());
    if (!m_osRPBSourceFilename.empty())
        CPLDebug("MDReaderALOS", "RPB Filename: %s",
                 m_osRPBSourceFilename.c_str());
}

/**
 * ~GDALMDReaderALOS()
 */
GDALMDReaderALOS::~GDALMDReaderALOS()
{
}

/**
 * HasRequiredFiles()
 */
bool GDALMDReaderALOS::HasRequiredFiles() const
{
    if (!m_osIMDSourceFilename.empty())
        return true;

    if (!m_osHDRSourceFilename.empty() && !m_osRPBSourceFilename.empty())
        return true;

    return false;
}

/**
 * GetMetadataFiles()
 */
char **GDALMDReaderALOS::GetMetadataFiles() const
{
    char **papszFileList = nullptr;
    if (!m_osIMDSourceFilename.empty())
        papszFileList = CSLAddString(papszFileList, m_osIMDSourceFilename);
    if (!m_osHDRSourceFilename.empty())
        papszFileList = CSLAddString(papszFileList, m_osHDRSourceFilename);
    if (!m_osRPBSourceFilename.empty())
        papszFileList = CSLAddString(papszFileList, m_osRPBSourceFilename);

    return papszFileList;
}

/**
 * LoadMetadata()
 */
void GDALMDReaderALOS::LoadMetadata()
{
    if (m_bIsMetadataLoad)
        return;

    if (!m_osIMDSourceFilename.empty())
    {
        m_papszIMDMD = CSLLoad(m_osIMDSourceFilename);
    }

    if (!m_osHDRSourceFilename.empty())
    {
        if (nullptr == m_papszIMDMD)
        {
            m_papszIMDMD = CSLLoad(m_osHDRSourceFilename);
        }
        else
        {
            char **papszHDR = CSLLoad(m_osHDRSourceFilename);
            m_papszIMDMD = CSLMerge(m_papszIMDMD, papszHDR);
            CSLDestroy(papszHDR);
        }
    }

    m_papszRPCMD = LoadRPCTxtFile();

    m_papszDEFAULTMD =
        CSLAddNameValue(m_papszDEFAULTMD, MD_NAME_MDTYPE, "ALOS");

    m_bIsMetadataLoad = true;

    const char *pszSatId1 = CSLFetchNameValue(m_papszIMDMD, "Lbi_Satellite");
    const char *pszSatId2 = CSLFetchNameValue(m_papszIMDMD, "Lbi_Sensor");
    if (nullptr != pszSatId1 && nullptr != pszSatId2)
    {
        m_papszIMAGERYMD = CSLAddNameValue(
            m_papszIMAGERYMD, MD_NAME_SATELLITE,
            CPLSPrintf("%s %s", CPLStripQuotes(pszSatId1).c_str(),
                       CPLStripQuotes(pszSatId2).c_str()));
    }
    else if (nullptr != pszSatId1 && nullptr == pszSatId2)
    {
        m_papszIMAGERYMD = CSLAddNameValue(m_papszIMAGERYMD, MD_NAME_SATELLITE,
                                           CPLStripQuotes(pszSatId1));
    }
    else if (nullptr == pszSatId1 && nullptr != pszSatId2)
    {
        m_papszIMAGERYMD = CSLAddNameValue(m_papszIMAGERYMD, MD_NAME_SATELLITE,
                                           CPLStripQuotes(pszSatId2));
    }

    const char *pszCloudCover =
        CSLFetchNameValue(m_papszIMDMD, "Img_CloudQuantityOfAllImage");
    if (nullptr != pszCloudCover)
    {
        int nCC = atoi(pszCloudCover);
        if (nCC >= 99)
        {
            m_papszIMAGERYMD = CSLAddNameValue(
                m_papszIMAGERYMD, MD_NAME_CLOUDCOVER, MD_CLOUDCOVER_NA);
        }
        else
        {
            m_papszIMAGERYMD =
                CSLAddNameValue(m_papszIMAGERYMD, MD_NAME_CLOUDCOVER,
                                CPLSPrintf("%d", nCC * 10));
        }
    }

    const char *pszDate =
        CSLFetchNameValue(m_papszIMDMD, "Img_SceneCenterDateTime");

    if (nullptr != pszDate)
    {
        char buffer[80];
        GIntBig timeMid = GetAcquisitionTimeFromString(CPLStripQuotes(pszDate));
        struct tm tmBuf;
        strftime(buffer, 80, MD_DATETIMEFORMAT,
                 CPLUnixTimeToYMDHMS(timeMid, &tmBuf));
        m_papszIMAGERYMD =
            CSLAddNameValue(m_papszIMAGERYMD, MD_NAME_ACQDATETIME, buffer);
    }
    else
    {
        pszDate = CSLFetchNameValue(m_papszIMDMD, "Lbi_ObservationDate");
        if (nullptr != pszDate)
        {
            const char *pszTime = "00:00:00.000";

            char buffer[80];
            GIntBig timeMid = GetAcquisitionTimeFromString(
                CPLSPrintf("%s %s", CPLStripQuotes(pszDate).c_str(),
                           CPLStripQuotes(pszTime).c_str()));
            struct tm tmBuf;
            strftime(buffer, 80, MD_DATETIMEFORMAT,
                     CPLUnixTimeToYMDHMS(timeMid, &tmBuf));
            m_papszIMAGERYMD =
                CSLAddNameValue(m_papszIMAGERYMD, MD_NAME_ACQDATETIME, buffer);
        }
    }
}

/**
 * LoadRPCTxtFile
 */
char **GDALMDReaderALOS::LoadRPCTxtFile()
{
    if (m_osRPBSourceFilename.empty())
        return nullptr;

    const CPLStringList aosLines(CSLLoad(m_osRPBSourceFilename));
    if (aosLines.empty())
        return nullptr;

    const char *pszFirstRow = aosLines[0];
    CPLStringList aosRPB;
    if (nullptr != pszFirstRow)
    {
        static const struct
        {
            const char *pszName;
            int nSize;
        } apsFieldDescriptors[] = {
            {RPC_LINE_OFF, 6},     {RPC_SAMP_OFF, 5},   {RPC_LAT_OFF, 8},
            {RPC_LONG_OFF, 9},     {RPC_HEIGHT_OFF, 5}, {RPC_LINE_SCALE, 6},
            {RPC_SAMP_SCALE, 5},   {RPC_LAT_SCALE, 8},  {RPC_LONG_SCALE, 9},
            {RPC_HEIGHT_SCALE, 5},
        };

        int nRequiredSize = 0;
        for (const auto &sFieldDescriptor : apsFieldDescriptors)
        {
            nRequiredSize += sFieldDescriptor.nSize;
        }

        static const char *const apszRPCTXT20ValItems[] = {
            RPC_LINE_NUM_COEFF, RPC_LINE_DEN_COEFF, RPC_SAMP_NUM_COEFF,
            RPC_SAMP_DEN_COEFF};

        constexpr int RPC_COEFF_COUNT1 = CPL_ARRAYSIZE(apszRPCTXT20ValItems);
        constexpr int RPC_COEFF_COUNT2 = 20;
        constexpr int RPC_COEFF_SIZE = 12;
        nRequiredSize += RPC_COEFF_COUNT1 * RPC_COEFF_COUNT2 * RPC_COEFF_SIZE;
        if (strlen(pszFirstRow) < static_cast<size_t>(nRequiredSize))
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "%s has only %d bytes wherea %d are required",
                     m_osRPBSourceFilename.c_str(), int(strlen(pszFirstRow)),
                     nRequiredSize);
            return nullptr;
        }

        int nOffset = 0;
        char buff[16] = {0};
        for (const auto &sFieldDescriptor : apsFieldDescriptors)
        {
            CPLAssert(sFieldDescriptor.nSize < int(sizeof(buff)));
            memcpy(buff, pszFirstRow + nOffset, sFieldDescriptor.nSize);
            buff[sFieldDescriptor.nSize] = 0;
            aosRPB.SetNameValue(sFieldDescriptor.pszName, buff);
            nOffset += sFieldDescriptor.nSize;
        }

        for (const char *pszItem : apszRPCTXT20ValItems)
        {
            std::string osValue;
            for (int j = 0; j < RPC_COEFF_COUNT2; j++)
            {
                memcpy(buff, pszFirstRow + nOffset, RPC_COEFF_SIZE);
                buff[RPC_COEFF_SIZE] = 0;
                nOffset += RPC_COEFF_SIZE;

                if (!osValue.empty())
                    osValue += " ";
                osValue += buff;
            }
            aosRPB.SetNameValue(pszItem, osValue.c_str());
        }
    }

    return aosRPB.StealList();
}

/**
 * GetAcqisitionTimeFromString()
 */
GIntBig GDALMDReaderALOS::GetAcquisitionTimeFromString(const char *pszDateTime)
{
    if (nullptr == pszDateTime)
        return 0;

    int iYear;
    int iMonth;
    int iDay;
    int iHours;
    int iMin;
    int iSec;

    int r = sscanf(pszDateTime, "%4d%2d%2d %d:%d:%d.%*d", &iYear, &iMonth,
                   &iDay, &iHours, &iMin, &iSec);

    if (r != 6)
        return 0;

    struct tm tmDateTime;
    tmDateTime.tm_sec = iSec;
    tmDateTime.tm_min = iMin;
    tmDateTime.tm_hour = iHours;
    tmDateTime.tm_mday = iDay;
    tmDateTime.tm_mon = iMonth - 1;
    tmDateTime.tm_year = iYear - 1900;
    tmDateTime.tm_isdst = -1;

    return CPLYMDHMSToUnixTime(&tmDateTime);
}
