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

#include "cpl_port.h"
#include "reader_eros.h"

#include <cstddef>
#include <cstdio>
#include <cstdlib>

#include "cpl_conv.h"
#include "cpl_error.h"
#include "cpl_string.h"

CPL_CVSID("$Id$")

/**
 * GDALMDReaderEROS()
 */
GDALMDReaderEROS::GDALMDReaderEROS(const char *pszPath,
        char **papszSiblingFiles) : GDALMDReaderBase(pszPath, papszSiblingFiles)
{
    CPLString osBaseName = CPLGetBasename(pszPath);
    CPLString osDirName = CPLGetDirname(pszPath);
    char szMetadataName[512] = {0};
    size_t i;
    if( osBaseName.size() > 511 )
        return;
    for(i = 0; i < osBaseName.size(); i++)
    {
        if(STARTS_WITH_CI(osBaseName + i, "."))
        {
            CPLString osPassFileName = CPLFormFilename( osDirName, szMetadataName,
                                               "pass" );
            if (CPLCheckForFile(&osPassFileName[0], papszSiblingFiles))
            {
                m_osIMDSourceFilename = osPassFileName;
                break;
            }
            else
            {
                osPassFileName = CPLFormFilename( osDirName, szMetadataName,
                                                   "PASS" );
                if (CPLCheckForFile(&osPassFileName[0], papszSiblingFiles))
                {
                    m_osIMDSourceFilename = osPassFileName;
                    break;
                }
            }
        }
        szMetadataName[i] = osBaseName[i];
    }

    if(m_osIMDSourceFilename.empty())
    {
        CPLString osPassFileName = CPLFormFilename( osDirName, szMetadataName, "pass" );
        if (CPLCheckForFile(&osPassFileName[0], papszSiblingFiles))
        {
            m_osIMDSourceFilename = osPassFileName;
        }
        else
        {
            osPassFileName = CPLFormFilename( osDirName, szMetadataName, "PASS" );
            if (CPLCheckForFile(&osPassFileName[0], papszSiblingFiles))
            {
                m_osIMDSourceFilename = osPassFileName;
            }
        }
    }

    CPLString osRPCFileName = CPLFormFilename( osDirName, szMetadataName,
                                                  "rpc" );
    if (CPLCheckForFile(&osRPCFileName[0], papszSiblingFiles))
    {
        m_osRPBSourceFilename = osRPCFileName;
    }
    else
    {
        osRPCFileName = CPLFormFilename( osDirName, szMetadataName, "RPC" );
        if (CPLCheckForFile(&osRPCFileName[0], papszSiblingFiles))
        {
            m_osRPBSourceFilename = osRPCFileName;
        }
    }

    if(!m_osIMDSourceFilename.empty() )
        CPLDebug( "MDReaderEROS", "IMD Filename: %s",
              m_osIMDSourceFilename.c_str() );
    if(!m_osRPBSourceFilename.empty() )
        CPLDebug( "MDReaderEROS", "RPB Filename: %s",
              m_osRPBSourceFilename.c_str() );
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
char** GDALMDReaderEROS::GetMetadataFiles() const
{
    char **papszFileList = nullptr;
    if(!m_osIMDSourceFilename.empty())
        papszFileList= CSLAddString( papszFileList, m_osIMDSourceFilename );
    if(!m_osRPBSourceFilename.empty())
        papszFileList = CSLAddString( papszFileList, m_osRPBSourceFilename );
    return papszFileList;
}

/**
 * LoadMetadata()
 */
void GDALMDReaderEROS::LoadMetadata()
{
    if(m_bIsMetadataLoad)
        return;

    if(!m_osIMDSourceFilename.empty())
    {
        m_papszIMDMD = LoadImdTxtFile();
    }

    if(!m_osRPBSourceFilename.empty())
    {
        m_papszRPCMD = GDALLoadRPCFile( m_osRPBSourceFilename );
    }

    m_papszDEFAULTMD = CSLAddNameValue(m_papszDEFAULTMD, MD_NAME_MDTYPE, "EROS");

    m_bIsMetadataLoad = true;

    const char* pszSatId1 = CSLFetchNameValue(m_papszIMDMD, "satellite");
    const char* pszSatId2 = CSLFetchNameValue(m_papszIMDMD, "camera");
    if(nullptr != pszSatId1 && nullptr != pszSatId2)
    {
        m_papszIMAGERYMD = CSLAddNameValue(m_papszIMAGERYMD,
                           MD_NAME_SATELLITE, CPLSPrintf( "%s %s",
                           CPLStripQuotes(pszSatId1).c_str(),
                           CPLStripQuotes(pszSatId2).c_str()));
    }
    else if(nullptr != pszSatId1 && nullptr == pszSatId2)
    {
        m_papszIMAGERYMD = CSLAddNameValue(m_papszIMAGERYMD,
                                MD_NAME_SATELLITE, CPLStripQuotes(pszSatId1));
    }
    else if(nullptr == pszSatId1 && nullptr != pszSatId2)
    {
        m_papszIMAGERYMD = CSLAddNameValue(m_papszIMAGERYMD,
                                MD_NAME_SATELLITE, CPLStripQuotes(pszSatId2));
    }

    const char* pszCloudCover = CSLFetchNameValue(m_papszIMDMD,
                                                 "overall_cc");
    if(nullptr != pszCloudCover)
    {
        int nCC = atoi(pszCloudCover);
        if(nCC > 100 || nCC < 0)
        {
            m_papszIMAGERYMD = CSLAddNameValue(m_papszIMAGERYMD, MD_NAME_CLOUDCOVER,
                                               MD_CLOUDCOVER_NA);
        }
        else
        {
            m_papszIMAGERYMD = CSLAddNameValue(m_papszIMAGERYMD,
                          MD_NAME_CLOUDCOVER, CPLSPrintf("%d", nCC));
        }
    }

    const char* pszDate = CSLFetchNameValue(m_papszIMDMD, "sweep_start_utc");
    if(nullptr != pszDate)
    {
        char buffer[80];
        time_t timeMid = GetAcquisitionTimeFromString(CPLStripQuotes(pszDate));
        strftime (buffer, 80, MD_DATETIMEFORMAT, localtime(&timeMid));
        m_papszIMAGERYMD = CSLAddNameValue(m_papszIMAGERYMD,
                                           MD_NAME_ACQDATETIME, buffer);
    }
}

/**
 * LoadImdTxtFile
 */
char** GDALMDReaderEROS::LoadImdTxtFile()
{
    char** papszLines = CSLLoad(m_osIMDSourceFilename);
    if(nullptr == papszLines)
        return nullptr;

    char** papszIMD = nullptr;

    for(int i = 0; papszLines[i] != nullptr; i++)
    {
        const char *pszLine = papszLines[i];
        if( CPLStrnlen(pszLine, 21) >= 21 )
        {
            char szName[22];
            memcpy(szName, pszLine, 21);
            szName[21] = 0;
            char* pszSpace = strchr(szName, ' ');
            if(pszSpace)
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
time_t GDALMDReaderEROS::GetAcquisitionTimeFromString(
        const char* pszDateTime)
{
    if(nullptr == pszDateTime)
        return 0;

    int iYear;
    int iMonth;
    int iDay;
    int iHours;
    int iMin;
    int iSec;

    // example: sweep_start_utc     2013-04-22,11:35:02.50724

    int r = sscanf ( pszDateTime, "%d-%d-%d,%d:%d:%d.%*d",
                     &iYear, &iMonth, &iDay, &iHours, &iMin, &iSec);

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

    return mktime(&tmDateTime);
}
