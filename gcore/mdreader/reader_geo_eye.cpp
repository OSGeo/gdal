/******************************************************************************
 *
 * Project:  GDAL Core
 * Purpose:  Read metadata from GeoEye imagery.
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
#include "reader_geo_eye.h"

#include <cstddef>
#include <cstdio>
#include <cstring>

#include <string>

#include "cpl_conv.h"
#include "cpl_error.h"
#include "cpl_string.h"
#include "cpl_time.h"

CPL_CVSID("$Id$")

/**
 * GDALMDReaderGeoEye()
 */
GDALMDReaderGeoEye::GDALMDReaderGeoEye(const char *pszPath,
        char **papszSiblingFiles) : GDALMDReaderBase(pszPath, papszSiblingFiles)
{

    const CPLString osBaseName = CPLGetBasename(pszPath);
    const CPLString osDirName = CPLGetDirname(pszPath);
    const size_t nBaseNameLen = osBaseName.size();
    if( nBaseNameLen > 511 )
        return;

    // get _metadata.txt file

    // split file name by _rgb_ or _pan_
    char szMetadataName[512] = {0};
    size_t i;
    for(i = 0; i < nBaseNameLen; i++)
    {
        szMetadataName[i] = osBaseName[i];
        if(STARTS_WITH_CI(osBaseName.c_str() + i, "_rgb_") || STARTS_WITH_CI(osBaseName.c_str() + i, "_pan_"))
        {
            break;
        }
    }

    // form metadata file name
    CPLStrlcpy(szMetadataName + i, "_metadata.txt", 14);
    CPLString osIMDSourceFilename = CPLFormFilename( osDirName,
                                                        szMetadataName, nullptr );
    if (CPLCheckForFile(&osIMDSourceFilename[0], papszSiblingFiles))
    {
        m_osIMDSourceFilename = osIMDSourceFilename;
    }
    else
    {
        CPLStrlcpy(szMetadataName + i, "_METADATA.TXT", 14);
        osIMDSourceFilename = CPLFormFilename( osDirName, szMetadataName, nullptr );
        if (CPLCheckForFile(&osIMDSourceFilename[0], papszSiblingFiles))
        {
            m_osIMDSourceFilename = osIMDSourceFilename;
        }
    }

    // get _rpc.txt file

    CPLString osRPBSourceFilename = CPLFormFilename( osDirName,
                                                     (osBaseName + "_rpc").c_str(),
                                                     "txt" );
    if (CPLCheckForFile(&osRPBSourceFilename[0], papszSiblingFiles))
    {
        m_osRPBSourceFilename = osRPBSourceFilename;
    }
    else
    {
        osRPBSourceFilename = CPLFormFilename( osDirName,
                                               (osBaseName + "_RPC").c_str(),
                                               "TXT" );
        if (CPLCheckForFile(&osRPBSourceFilename[0], papszSiblingFiles))
        {
            m_osRPBSourceFilename = osRPBSourceFilename;
        }
    }

    if( !m_osIMDSourceFilename.empty() )
        CPLDebug( "MDReaderGeoEye", "IMD Filename: %s",
                  m_osIMDSourceFilename.c_str() );
    if( !m_osRPBSourceFilename.empty() )
        CPLDebug( "MDReaderGeoEye", "RPB Filename: %s",
                  m_osRPBSourceFilename.c_str() );
}

/**
 * ~GDALMDReaderGeoEye()
 */
GDALMDReaderGeoEye::~GDALMDReaderGeoEye()
{
}

/**
 * HasRequiredFiles()
 */
bool GDALMDReaderGeoEye::HasRequiredFiles() const
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
char** GDALMDReaderGeoEye::GetMetadataFiles() const
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
void GDALMDReaderGeoEye::LoadMetadata()
{
    if(m_bIsMetadataLoad)
        return;

    if (!m_osIMDSourceFilename.empty())
    {
        m_papszIMDMD = LoadIMDWktFile( );
    }

    if(!m_osRPBSourceFilename.empty())
    {
        m_papszRPCMD = GDALLoadRPCFile( m_osRPBSourceFilename );
    }

    m_papszDEFAULTMD = CSLAddNameValue(m_papszDEFAULTMD, MD_NAME_MDTYPE, "GE");

    m_bIsMetadataLoad = true;

    if(nullptr == m_papszIMDMD)
    {
        return;
    }

    //extract imagery metadata
    const char* pszSatId = CSLFetchNameValue(m_papszIMDMD,
                                             "Source Image Metadata.Sensor");
    if(nullptr != pszSatId)
    {
        m_papszIMAGERYMD = CSLAddNameValue(m_papszIMAGERYMD,
                                           MD_NAME_SATELLITE,
                                           CPLStripQuotes(pszSatId));
    }

    const char* pszCloudCover = CSLFetchNameValue(m_papszIMDMD,
                                   "Source Image Metadata.Percent Cloud Cover");
    if(nullptr != pszCloudCover)
    {
        m_papszIMAGERYMD = CSLAddNameValue(m_papszIMAGERYMD,
                                           MD_NAME_CLOUDCOVER, pszCloudCover);
    }

    const char* pszDateTime = CSLFetchNameValue(m_papszIMDMD,
                                 "Source Image Metadata.Acquisition Date/Time");

    if(nullptr != pszDateTime)
    {
        char buffer[80];
        GIntBig timeMid = GetAcquisitionTimeFromString(pszDateTime);

        struct tm tmBuf;
        strftime (buffer, 80, MD_DATETIMEFORMAT, CPLUnixTimeToYMDHMS(timeMid, &tmBuf));
        m_papszIMAGERYMD = CSLAddNameValue(m_papszIMAGERYMD,
                                           MD_NAME_ACQDATETIME, buffer);
    }
}

/**
 * GetAcqisitionTimeFromString()
 */
GIntBig GDALMDReaderGeoEye::GetAcquisitionTimeFromString(
        const char* pszDateTime)
{
    if(nullptr == pszDateTime)
        return 0;

    int iYear;
    int iMonth;
    int iDay;
    int iHours;
    int iMin;
    int iSec = 0;

    // string example: Acquisition Date/Time: 2006-03-01 11:08 GMT

    int r = sscanf ( pszDateTime, "%d-%d-%d %d:%d GMT", &iYear, &iMonth,
                     &iDay, &iHours, &iMin);

    if (r != 5)
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

/**
 * LoadWKTIMDFile()
 */
char** GDALMDReaderGeoEye::LoadIMDWktFile() const
{
    char** papszResultList = nullptr;
    char** papszLines = CSLLoad( m_osIMDSourceFilename );
    bool bBeginSection = false;
    CPLString osSection;
    CPLString osKeyLevel1;
    CPLString osKeyLevel2;
    CPLString osKeyLevel3;
    int nLevel = 0;
    int nSpaceCount;

    if( papszLines == nullptr )
        return nullptr;

    for( int i = 0; papszLines[i] != nullptr; i++ )
    {
        // skip section (=== or ---) lines

        if(STARTS_WITH_CI(papszLines[i], "==="))
        {
            bBeginSection = true;
            continue;
        }

        if(STARTS_WITH_CI(papszLines[i], "---") || CPLStrnlen(papszLines[i], 512) == 0)
            continue;

        // check the metadata level
        nSpaceCount = 0;
        for(int j = 0; j < 11; j++)
        {
            if(papszLines[i][j] != ' ')
                break;
            nSpaceCount++;
        }

        if(nSpaceCount % 3 != 0)
            continue; // not a metadata item
        nLevel = nSpaceCount / 3;

        const char *pszValue;
        char *pszKey = nullptr;
        pszValue = CPLParseNameValue(papszLines[i], &pszKey);

        if(nullptr != pszValue && CPLStrnlen(pszValue, 512) > 0)
        {

            CPLString osCurrentKey;
            if(nLevel == 0)
            {
                osCurrentKey = CPLOPrintf("%s", pszKey);
            }
            else if(nLevel == 1)
            {
                osCurrentKey = osKeyLevel1 + "." +
                        CPLOPrintf("%s", pszKey + nSpaceCount);
            }
            else if(nLevel == 2)
            {
                osCurrentKey = osKeyLevel1 + "." +
                        osKeyLevel2 + "." + CPLOPrintf("%s", pszKey + nSpaceCount);
            }
            else if(nLevel == 3)
            {
                osCurrentKey = osKeyLevel1 + "." +
                        osKeyLevel2 + "." + osKeyLevel3 + "." +
                        CPLOPrintf("%s", pszKey + nSpaceCount);
            }

            if(!osSection.empty())
            {
                osCurrentKey = osSection + "." + osCurrentKey;
            }

            papszResultList = CSLAddNameValue(papszResultList, osCurrentKey, pszValue);
        }

        if(nullptr != pszKey && CPLStrnlen(pszKey, 512) > 0)
        {
            if(bBeginSection)
            {
                osSection = CPLOPrintf("%s", pszKey);
                bBeginSection = false;
            }
            else if(nLevel == 0)
            {
                osKeyLevel1 = CPLOPrintf("%s", pszKey);
            }
            else if(nLevel == 1)
            {
                osKeyLevel2 = CPLOPrintf("%s", pszKey + nSpaceCount);
            }
            else if(nLevel == 2)
            {
                osKeyLevel3 = CPLOPrintf("%s", pszKey + nSpaceCount);
            }
        }
        else
        {
            if(bBeginSection)
            {
                osSection = CPLOPrintf("%s", papszLines[i]);
                bBeginSection = false;
            }
            else if(nLevel == 0)
            {
                osKeyLevel1 = CPLOPrintf("%s", papszLines[i]);
            }
            else if(nLevel == 1)
            {
                osKeyLevel2 = CPLOPrintf("%s", papszLines[i] + nSpaceCount);
            }
            else if(nLevel == 2)
            {
                osKeyLevel3 = CPLOPrintf("%s", papszLines[i]+ nSpaceCount);
            }
        }

        CPLFree( pszKey );
    }

    CSLDestroy( papszLines );

    return papszResultList;
}
