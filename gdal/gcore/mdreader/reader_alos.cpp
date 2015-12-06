/******************************************************************************
 * $Id$
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

/**
 * GDALMDReaderALOS()
 */
GDALMDReaderALOS::GDALMDReaderALOS(const char *pszPath,
        char **papszSiblingFiles) : GDALMDReaderBase(pszPath, papszSiblingFiles)
{
    CPLString osDirName = CPLGetDirname(pszPath);
    CPLString osBaseName = CPLGetBasename(pszPath);

    const char* pszIMDSourceFilename = CPLFormFilename(osDirName, "summary", ".txt");
    if (CPLCheckForFile((char*)pszIMDSourceFilename, papszSiblingFiles))
    {
        m_osIMDSourceFilename = pszIMDSourceFilename;
    }
    else
    {
        pszIMDSourceFilename = CPLFormFilename( osDirName, "SUMMARY", ".TXT");
        if (CPLCheckForFile((char*)pszIMDSourceFilename, papszSiblingFiles))
        {
            m_osIMDSourceFilename = pszIMDSourceFilename;
        }
    }

    const char *pszHDRFileName;
    if( osBaseName.size() >= 6 )
    {
        // check if this is separate band or whole image
        // test without 6 symbols
        pszHDRFileName = CPLFormFilename(osDirName, CPLSPrintf("HDR%s",
                                                    osBaseName + 6), "txt");
        if (CPLCheckForFile((char*)pszHDRFileName, papszSiblingFiles))
        {
            m_osHDRSourceFilename = pszHDRFileName;
        }
        else
        {
            pszHDRFileName = CPLFormFilename(osDirName, CPLSPrintf("HDR%s",
                                            osBaseName + 6), "TXT");
            if (CPLCheckForFile((char*)pszHDRFileName, papszSiblingFiles))
            {
                m_osHDRSourceFilename = pszHDRFileName;
            }
        }
    }

    // test without 3 symbols
    if( osBaseName.size() >= 3 && m_osHDRSourceFilename.empty())
    {
        pszHDRFileName = CPLFormFilename(osDirName, CPLSPrintf("HDR%s",
                                         osBaseName + 3), "txt");
        if (CPLCheckForFile((char*)pszHDRFileName, papszSiblingFiles))
        {
            m_osHDRSourceFilename = pszHDRFileName;
        }
        else
        {
            pszHDRFileName = CPLFormFilename(osDirName, CPLSPrintf("HDR%s",
                                             osBaseName + 3), "TXT");
            if (CPLCheckForFile((char*)pszHDRFileName, papszSiblingFiles))
            {
                m_osHDRSourceFilename = pszHDRFileName;
            }
        }
    }

    // test without 6 symbols
    const char *pszRPCFileName;
    if( osBaseName.size() >= 6 )
    {
        pszRPCFileName = CPLFormFilename(osDirName, CPLSPrintf("RPC%s",
                                                    osBaseName + 6), "txt");
        if (CPLCheckForFile((char*)pszRPCFileName, papszSiblingFiles))
        {
            m_osRPBSourceFilename = pszRPCFileName;
        }
        else
        {
            pszRPCFileName = CPLFormFilename(osDirName, CPLSPrintf("RPC%s",
                                            osBaseName + 6), "TXT");
            if (CPLCheckForFile((char*)pszRPCFileName, papszSiblingFiles))
            {
                m_osRPBSourceFilename = pszRPCFileName;
            }
        }
    }

    // test without 3 symbols
    if( osBaseName.size() >= 3 && m_osRPBSourceFilename.empty())
    {
        pszRPCFileName = CPLFormFilename(osDirName, CPLSPrintf("RPC%s",
                                         osBaseName + 3), "txt");
        if (CPLCheckForFile((char*)pszRPCFileName, papszSiblingFiles))
        {
            m_osRPBSourceFilename = pszRPCFileName;
        }
        else
        {
            pszRPCFileName = CPLFormFilename(osDirName, CPLSPrintf("RPC%s",
                                             osBaseName + 3), "TXT");
            if (CPLCheckForFile((char*)pszRPCFileName, papszSiblingFiles))
            {
                m_osRPBSourceFilename = pszRPCFileName;
            }
        }
    }

    if(m_osIMDSourceFilename.size())
        CPLDebug( "MDReaderALOS", "IMD Filename: %s",
              m_osIMDSourceFilename.c_str() );
    if(m_osHDRSourceFilename.size())
        CPLDebug( "MDReaderALOS", "HDR Filename: %s",
              m_osHDRSourceFilename.c_str() );
    if(m_osRPBSourceFilename.size())
        CPLDebug( "MDReaderALOS", "RPB Filename: %s",
              m_osRPBSourceFilename.c_str() );
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

    if(!m_osHDRSourceFilename.empty() && !m_osRPBSourceFilename.empty())
        return true;

    return false;
}

/**
 * GetMetadataFiles()
 */
char** GDALMDReaderALOS::GetMetadataFiles() const
{
    char **papszFileList = NULL;
    if(!m_osIMDSourceFilename.empty())
        papszFileList= CSLAddString( papszFileList, m_osIMDSourceFilename );
    if(!m_osHDRSourceFilename.empty())
        papszFileList= CSLAddString( papszFileList, m_osHDRSourceFilename );
    if(!m_osRPBSourceFilename.empty())
        papszFileList= CSLAddString( papszFileList, m_osRPBSourceFilename );

    return papszFileList;
}

/**
 * LoadMetadata()
 */
void GDALMDReaderALOS::LoadMetadata()
{
    if(m_bIsMetadataLoad)
        return;

    if(!m_osIMDSourceFilename.empty())
    {
        m_papszIMDMD = CSLLoad(m_osIMDSourceFilename);
    }

    if(!m_osHDRSourceFilename.empty())
    {
        if(NULL == m_papszIMDMD)
        {
            m_papszIMDMD = CSLLoad(m_osHDRSourceFilename);
        }
        else
        {
            char** papszHDR = CSLLoad(m_osHDRSourceFilename);
            m_papszIMDMD = CSLMerge(m_papszIMDMD, papszHDR);
            CSLDestroy(papszHDR);
        }
    }

    m_papszRPCMD = LoadRPCTxtFile();

    m_papszDEFAULTMD = CSLAddNameValue(m_papszDEFAULTMD, MD_NAME_MDTYPE, "ALOS");

    m_bIsMetadataLoad = true;

    const char* pszSatId1 = CSLFetchNameValue(m_papszIMDMD, "Lbi_Satellite");
    const char* pszSatId2 = CSLFetchNameValue(m_papszIMDMD, "Lbi_Sensor");
    if(NULL != pszSatId1 && NULL != pszSatId2)
    {
        m_papszIMAGERYMD = CSLAddNameValue(m_papszIMAGERYMD,
                           MD_NAME_SATELLITE, CPLSPrintf( "%s %s",
                           CPLStripQuotes(pszSatId1).c_str(),
                           CPLStripQuotes(pszSatId2).c_str()));
    }
    else if(NULL != pszSatId1 && NULL == pszSatId2)
    {
        m_papszIMAGERYMD = CSLAddNameValue(m_papszIMAGERYMD,
                                MD_NAME_SATELLITE, CPLStripQuotes(pszSatId1));
    }
    else if(NULL == pszSatId1 && NULL != pszSatId2)
    {
        m_papszIMAGERYMD = CSLAddNameValue(m_papszIMAGERYMD,
                                MD_NAME_SATELLITE, CPLStripQuotes(pszSatId2));
    }


    const char* pszCloudCover = CSLFetchNameValue(m_papszIMDMD,
                                                 "Img_CloudQuantityOfAllImage");
    if(NULL != pszCloudCover)
    {
        int nCC = atoi(pszCloudCover);
        if(nCC >= 99)
        {
            m_papszIMAGERYMD = CSLAddNameValue(m_papszIMAGERYMD, MD_NAME_CLOUDCOVER,
                                               MD_CLOUDCOVER_NA);
        }
        else
        {
            m_papszIMAGERYMD = CSLAddNameValue(m_papszIMAGERYMD,
                          MD_NAME_CLOUDCOVER, CPLSPrintf("%d", nCC * 10));
        }
    }

    const char* pszDate = CSLFetchNameValue(m_papszIMDMD,
                                                     "Img_SceneCenterDateTime");

    if(NULL != pszDate)
    {
        char buffer[80];
        time_t timeMid = GetAcquisitionTimeFromString(CPLStripQuotes(pszDate));
        strftime (buffer, 80, MD_DATETIMEFORMAT, localtime(&timeMid));
        m_papszIMAGERYMD = CSLAddNameValue(m_papszIMAGERYMD,
                                           MD_NAME_ACQDATETIME, buffer);
    }
    else
    {
        pszDate = CSLFetchNameValue(m_papszIMDMD, "Lbi_ObservationDate");
        if(NULL != pszDate)
        {
            const char* pszTime = "00:00:00.000";

            char buffer[80];
            time_t timeMid = GetAcquisitionTimeFromString(CPLSPrintf( "%s %s",
                                              CPLStripQuotes(pszDate).c_str(),
                                              CPLStripQuotes(pszTime).c_str()));
            strftime (buffer, 80, MD_DATETIMEFORMAT, localtime(&timeMid));
            m_papszIMAGERYMD = CSLAddNameValue(m_papszIMAGERYMD,
                                               MD_NAME_ACQDATETIME, buffer);
        }
    }
}

static const char * const apszRPCTXT20ValItems[] =
{
    RPC_LINE_NUM_COEFF,
    RPC_LINE_DEN_COEFF,
    RPC_SAMP_NUM_COEFF,
    RPC_SAMP_DEN_COEFF,
    NULL
};

/**
 * LoadRPCTxtFile
 */
char** GDALMDReaderALOS::LoadRPCTxtFile()
{
    if(m_osRPBSourceFilename.empty())
        return NULL;

    char** papszLines = CSLLoad(m_osRPBSourceFilename);
    if(NULL == papszLines)
        return NULL;

    const char* pszFirstRow = papszLines[0];
    char** papszRPB = NULL;
    if(NULL != pszFirstRow)
    {
        char buff[50] = {0};
        int nOffset = 0;
        CPLStrlcpy(buff, pszFirstRow + nOffset, 7);
        nOffset += 6;
        papszRPB = CSLAddNameValue(papszRPB, RPC_LINE_OFF, buff);

        CPLStrlcpy(buff, pszFirstRow + nOffset, 6);
        nOffset += 5;
        papszRPB = CSLAddNameValue(papszRPB, RPC_SAMP_OFF, buff);

        CPLStrlcpy(buff, pszFirstRow + nOffset, 9);
        nOffset += 8;
        papszRPB = CSLAddNameValue(papszRPB, RPC_LAT_OFF, buff);

        CPLStrlcpy(buff, pszFirstRow + nOffset, 10);
        nOffset += 9;
        papszRPB = CSLAddNameValue(papszRPB, RPC_LONG_OFF, buff);

        CPLStrlcpy(buff, pszFirstRow + nOffset, 6);
        nOffset += 5;
        papszRPB = CSLAddNameValue(papszRPB, RPC_HEIGHT_OFF, buff);

        CPLStrlcpy(buff, pszFirstRow + nOffset, 7);
        nOffset += 6;
        papszRPB = CSLAddNameValue(papszRPB, RPC_LINE_SCALE, buff);

        CPLStrlcpy(buff, pszFirstRow + nOffset, 6);
        nOffset += 5;
        papszRPB = CSLAddNameValue(papszRPB, RPC_SAMP_SCALE, buff);

        CPLStrlcpy(buff, pszFirstRow + nOffset, 9);
        nOffset += 8;
        papszRPB = CSLAddNameValue(papszRPB, RPC_LAT_SCALE, buff);

        CPLStrlcpy(buff, pszFirstRow + nOffset, 10);
        nOffset += 9;
        papszRPB = CSLAddNameValue(papszRPB, RPC_LONG_SCALE, buff);

        CPLStrlcpy(buff, pszFirstRow + nOffset, 6);
        nOffset += 5;
        papszRPB = CSLAddNameValue(papszRPB, RPC_HEIGHT_SCALE, buff);

        int i, j;
        for( i = 0; apszRPCTXT20ValItems[i] != NULL; i++ )
        {
            CPLString value;
            for( j = 1; j < 21; j++ )
            {
                CPLStrlcpy(buff, pszFirstRow + nOffset, 13);
                nOffset += 12;

                value = value + " " + CPLString(buff);
            }
            papszRPB = CSLAddNameValue(papszRPB, apszRPCTXT20ValItems[i], value);
        }
    }
    CSLDestroy(papszLines);

    return papszRPB;
}

/**
 * GetAcqisitionTimeFromString()
 */
time_t GDALMDReaderALOS::GetAcquisitionTimeFromString(
        const char* pszDateTime)
{
    if(NULL == pszDateTime)
        return 0;

    int iYear;
    int iMonth;
    int iDay;
    int iHours;
    int iMin;
    int iSec;

    int r = sscanf ( pszDateTime, "%4d%2d%2d %d:%d:%d.%*d",
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
