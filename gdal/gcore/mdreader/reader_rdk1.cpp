/******************************************************************************
 * $Id$
 *
 * Project:  GDAL Core
 * Purpose:  Read metadata from Resurs-DK1 imagery.
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

#include "reader_rdk1.h"

CPL_CVSID("$Id$");

/**
 * GDALMDReaderResursDK1()
 */
GDALMDReaderResursDK1::GDALMDReaderResursDK1(const char *pszPath,
        char **papszSiblingFiles) : GDALMDReaderBase(pszPath, papszSiblingFiles)
{
    m_osXMLSourceFilename = GDALFindAssociatedFile( pszPath, "XML",
                                                         papszSiblingFiles, 0 );
    if( m_osXMLSourceFilename.size() )
        CPLDebug( "MDReaderResursDK1", "XML Filename: %s",
                  m_osXMLSourceFilename.c_str() );
}

/**
 * ~GDALMDReaderResursDK1()
 */
GDALMDReaderResursDK1::~GDALMDReaderResursDK1()
{
}

/**
 * HasRequiredFiles()
 */
bool GDALMDReaderResursDK1::HasRequiredFiles() const
{
    // check <MSP_ROOT>
    if (!m_osXMLSourceFilename.empty() &&
            GDALCheckFileHeader(m_osXMLSourceFilename, "<MSP_ROOT>"))
        return true;

    return false;
}

/**
 * GetMetadataFiles()
 */
char** GDALMDReaderResursDK1::GetMetadataFiles() const
{
    char **papszFileList = NULL;
    if(!m_osXMLSourceFilename.empty())
        papszFileList= CSLAddString( papszFileList, m_osXMLSourceFilename );

    return papszFileList;
}

/**
 * LoadMetadata()
 */
void GDALMDReaderResursDK1::LoadMetadata()
{
    if(m_bIsMetadataLoad)
        return;

    if (!m_osXMLSourceFilename.empty())
    {
        CPLXMLNode* psNode = CPLParseXMLFile(m_osXMLSourceFilename);

        if(psNode != NULL)
        {
            CPLXMLNode* pMSPRootNode = CPLSearchXMLNode(psNode, "=MSP_ROOT");

            if(pMSPRootNode != NULL)
            {
                m_papszIMDMD = ReadXMLToList(pMSPRootNode, m_papszIMDMD, "MSP_ROOT");
            }
            CPLDestroyXMLNode(psNode);
        }
    }

    m_papszDEFAULTMD = CSLAddNameValue(m_papszDEFAULTMD, MD_NAME_MDTYPE, "MSP");

    m_bIsMetadataLoad = true;

    if(NULL == m_papszIMDMD)
    {
        return;
    }

    //extract imagery metadata
    const char* pszSatId = CSLFetchNameValue(m_papszIMDMD, "MSP_ROOT.cCodeKA");
    if(NULL != pszSatId)
    {
        m_papszIMAGERYMD = CSLAddNameValue(m_papszIMAGERYMD, MD_NAME_SATELLITE,
                                           CPLStripQuotes(pszSatId));
    }


    const char* pszDate = CSLFetchNameValue(m_papszIMDMD,
                                            "MSP_ROOT.Normal.dSceneDate");

    if(NULL != pszDate)
    {
        const char* pszTime = CSLFetchNameValue(m_papszIMDMD,
                                         "MSP_ROOT.Normal.tSceneTime");
        if(NULL == pszTime)
            pszTime = "00:00:00.000000";

        char buffer[80];
        time_t timeMid = GetAcquisitionTimeFromString(CPLSPrintf( "%s %s",
                                                     pszDate, pszTime));
        strftime (buffer, 80, MD_DATETIMEFORMAT, localtime(&timeMid));
        m_papszIMAGERYMD = CSLAddNameValue(m_papszIMAGERYMD,
                                           MD_NAME_ACQDATETIME, buffer);
    }

    m_papszIMAGERYMD = CSLAddNameValue(m_papszIMAGERYMD, MD_NAME_CLOUDCOVER,
                                       MD_CLOUDCOVER_NA);
}

/**
 * GetAcqisitionTimeFromString()
 */
time_t GDALMDReaderResursDK1::GetAcquisitionTimeFromString(
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

// string exampe: <Normal>
//                  tSceneTime = 10:21:36.000000
//                  dSceneDate = 16/9/2008
//                </Normal>

    int r = sscanf ( pszDateTime, "%d/%d/%d %d:%d:%d.%*s",
                     &iDay, &iMonth, &iYear, &iHours, &iMin, &iSec);

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

    return mktime(&tmDateTime) - 10800; // int UTC+3 MSK
}

char** GDALMDReaderResursDK1::AddXMLNameValueToList(char** papszList,
                                               const char *pszName,
                                               const char *pszValue)
{
    char** papszTokens = CSLTokenizeString2( pszValue, "\n",
        CSLT_STRIPLEADSPACES | CSLT_STRIPENDSPACES );

    for(int i = 0; papszTokens[i] != NULL; i++ )
    {

        char** papszSubTokens = CSLTokenizeString2( papszTokens[i], "=",
            CSLT_STRIPLEADSPACES | CSLT_STRIPENDSPACES );
        if(CSLCount(papszSubTokens) < 2)
        {
            CSLDestroy( papszSubTokens );
            continue;
        }

        papszList = CSLAddNameValue(papszList, CPLSPrintf("%s.%s", pszName,
                                              papszSubTokens[0]),
                                              papszSubTokens[1]);
        CSLDestroy( papszSubTokens );
    }

    CSLDestroy( papszTokens );

    return papszList;
}
