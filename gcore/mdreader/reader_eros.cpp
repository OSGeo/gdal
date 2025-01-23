/******************************************************************************
 *
 * Project:  GDAL Core
 * Purpose:  Read metadata from EROS imagery.
 * Author:   Alexander Lisovenko
 * Author:   Dmitry Baryshnikov, polimax@mail.ru
 *
 ******************************************************************************
 * Copyright (c) 2014-2015, NextGIS info@nextgis.ru
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include "cpl_port.h"
#include "reader_eros.h"

#include <cstddef>
#include <cstdio>
#include <cstdlib>

#include "cpl_conv.h"
#include "cpl_error.h"
#include "cpl_string.h"
#include "cpl_time.h"

/**
 * GDALMDReaderEROS()
 */
GDALMDReaderEROS::GDALMDReaderEROS(const char *pszPath,
                                   char **papszSiblingFiles)
    : GDALMDReaderBase(pszPath, papszSiblingFiles)
{
    const CPLString osBaseName = CPLGetBasenameSafe(pszPath);
    const CPLString osDirName = CPLGetDirnameSafe(pszPath);
    char szMetadataName[512] = {0};
    size_t i;
    if (osBaseName.size() > 511)
        return;
    for (i = 0; i < osBaseName.size(); i++)
    {
        if (STARTS_WITH_CI(osBaseName + i, "."))
        {
            std::string osPassFileName =
                CPLFormFilenameSafe(osDirName, szMetadataName, "pass");
            if (CPLCheckForFile(&osPassFileName[0], papszSiblingFiles))
            {
                m_osIMDSourceFilename = std::move(osPassFileName);
                break;
            }
            else
            {
                osPassFileName =
                    CPLFormFilenameSafe(osDirName, szMetadataName, "PASS");
                if (CPLCheckForFile(&osPassFileName[0], papszSiblingFiles))
                {
                    m_osIMDSourceFilename = std::move(osPassFileName);
                    break;
                }
            }
        }
        szMetadataName[i] = osBaseName[i];
    }

    if (m_osIMDSourceFilename.empty())
    {
        std::string osPassFileName =
            CPLFormFilenameSafe(osDirName, szMetadataName, "pass");
        if (CPLCheckForFile(&osPassFileName[0], papszSiblingFiles))
        {
            m_osIMDSourceFilename = std::move(osPassFileName);
        }
        else
        {
            osPassFileName =
                CPLFormFilenameSafe(osDirName, szMetadataName, "PASS");
            if (CPLCheckForFile(&osPassFileName[0], papszSiblingFiles))
            {
                m_osIMDSourceFilename = std::move(osPassFileName);
            }
        }
    }

    std::string osRPCFileName =
        CPLFormFilenameSafe(osDirName, szMetadataName, "rpc");
    if (CPLCheckForFile(&osRPCFileName[0], papszSiblingFiles))
    {
        m_osRPBSourceFilename = std::move(osRPCFileName);
    }
    else
    {
        osRPCFileName = CPLFormFilenameSafe(osDirName, szMetadataName, "RPC");
        if (CPLCheckForFile(&osRPCFileName[0], papszSiblingFiles))
        {
            m_osRPBSourceFilename = std::move(osRPCFileName);
        }
    }

    if (!m_osIMDSourceFilename.empty())
        CPLDebug("MDReaderEROS", "IMD Filename: %s",
                 m_osIMDSourceFilename.c_str());
    if (!m_osRPBSourceFilename.empty())
        CPLDebug("MDReaderEROS", "RPB Filename: %s",
                 m_osRPBSourceFilename.c_str());
}

/**
 * ~GDALMDReaderEROS()
 */
GDALMDReaderEROS::~GDALMDReaderEROS()
{
}

/**
 * HasRequiredFiles()
 */
bool GDALMDReaderEROS::HasRequiredFiles() const
{
    if (!m_osIMDSourceFilename.empty())
        return true;
    if (!m_osRPBSourceFilename.empty())
        return true;

    return false;
}

/**
 * GetMetadataFiles()
 */
char **GDALMDReaderEROS::GetMetadataFiles() const
{
    char **papszFileList = nullptr;
    if (!m_osIMDSourceFilename.empty())
        papszFileList = CSLAddString(papszFileList, m_osIMDSourceFilename);
    if (!m_osRPBSourceFilename.empty())
        papszFileList = CSLAddString(papszFileList, m_osRPBSourceFilename);
    return papszFileList;
}

/**
 * LoadMetadata()
 */
void GDALMDReaderEROS::LoadMetadata()
{
    if (m_bIsMetadataLoad)
        return;

    if (!m_osIMDSourceFilename.empty())
    {
        m_papszIMDMD = LoadImdTxtFile();
    }

    if (!m_osRPBSourceFilename.empty())
    {
        m_papszRPCMD = GDALLoadRPCFile(m_osRPBSourceFilename);
    }

    m_papszDEFAULTMD =
        CSLAddNameValue(m_papszDEFAULTMD, MD_NAME_MDTYPE, "EROS");

    m_bIsMetadataLoad = true;

    const char *pszSatId1 = CSLFetchNameValue(m_papszIMDMD, "satellite");
    const char *pszSatId2 = CSLFetchNameValue(m_papszIMDMD, "camera");
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

    const char *pszCloudCover = CSLFetchNameValue(m_papszIMDMD, "overall_cc");
    if (nullptr != pszCloudCover)
    {
        int nCC = atoi(pszCloudCover);
        if (nCC > 100 || nCC < 0)
        {
            m_papszIMAGERYMD = CSLAddNameValue(
                m_papszIMAGERYMD, MD_NAME_CLOUDCOVER, MD_CLOUDCOVER_NA);
        }
        else
        {
            m_papszIMAGERYMD = CSLAddNameValue(
                m_papszIMAGERYMD, MD_NAME_CLOUDCOVER, CPLSPrintf("%d", nCC));
        }
    }

    const char *pszDate = CSLFetchNameValue(m_papszIMDMD, "sweep_start_utc");
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
}

/**
 * LoadImdTxtFile
 */
char **GDALMDReaderEROS::LoadImdTxtFile()
{
    char **papszLines = CSLLoad(m_osIMDSourceFilename);
    if (nullptr == papszLines)
        return nullptr;

    char **papszIMD = nullptr;

    for (int i = 0; papszLines[i] != nullptr; i++)
    {
        const char *pszLine = papszLines[i];
        if (CPLStrnlen(pszLine, 21) >= 21)
        {
            char szName[22];
            memcpy(szName, pszLine, 21);
            szName[21] = 0;
            char *pszSpace = strchr(szName, ' ');
            if (pszSpace)
            {
                *pszSpace = 0;
                papszIMD = CSLAddNameValue(papszIMD, szName, pszLine + 20);
            }
        }
    }

    CSLDestroy(papszLines);

    return papszIMD;
}

/**
 * GetAcqisitionTimeFromString()
 */
GIntBig GDALMDReaderEROS::GetAcquisitionTimeFromString(const char *pszDateTime)
{
    if (nullptr == pszDateTime)
        return 0;

    int iYear;
    int iMonth;
    int iDay;
    int iHours;
    int iMin;
    int iSec;

    // example: sweep_start_utc     2013-04-22,11:35:02.50724

    int r = sscanf(pszDateTime, "%d-%d-%d,%d:%d:%d.%*d", &iYear, &iMonth, &iDay,
                   &iHours, &iMin, &iSec);

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
