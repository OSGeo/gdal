/******************************************************************************
 * $Id$
 *
 * Project:  GDAL Core
 * Purpose:  Read metadata from Spot imagery.
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

#include "reader_spot.h"

/**
 * GDALMDReaderSpot()
 */
GDALMDReaderSpot::GDALMDReaderSpot(const char *pszPath,
        char **papszSiblingFiles) : GDALMDReaderPleiades(pszPath, papszSiblingFiles)
{
    const char* pszIMDSourceFilename;
    const char* pszDirName = CPLGetDirname(pszPath);

    if(m_osIMDSourceFilename.empty())
    {
        pszIMDSourceFilename = CPLFormFilename( pszDirName, "METADATA.DIM", NULL );

        if (CPLCheckForFile((char*)pszIMDSourceFilename, papszSiblingFiles))
        {
            m_osIMDSourceFilename = pszIMDSourceFilename;
        }
        else
        {
            pszIMDSourceFilename = CPLFormFilename( pszDirName, "metadata.dim", NULL );
            if (CPLCheckForFile((char*)pszIMDSourceFilename, papszSiblingFiles))
            {
                m_osIMDSourceFilename = pszIMDSourceFilename;
            }
        }
    }

    // if the file name ended on METADATA.DIM
    // Linux specific
    // example: R2_CAT_091028105025131_1\METADATA.DIM
    if(m_osIMDSourceFilename.empty())
    {
        if(EQUAL(CPLGetFilename(pszPath), "IMAGERY.TIF"))
        {
            pszIMDSourceFilename = CPLSPrintf( "%s\\METADATA.DIM",
                                                           CPLGetPath(pszPath));

            if (CPLCheckForFile((char*)pszIMDSourceFilename, papszSiblingFiles))
            {
                m_osIMDSourceFilename = pszIMDSourceFilename;
            }
            else
            {
                pszIMDSourceFilename = CPLSPrintf( "%s\\metadata.dim",
                                                           CPLGetPath(pszPath));
                if (CPLCheckForFile((char*)pszIMDSourceFilename, papszSiblingFiles))
                {
                    m_osIMDSourceFilename = pszIMDSourceFilename;
                }
            }
        }
    }

    if(m_osIMDSourceFilename.size())
        CPLDebug( "MDReaderSpot", "IMD Filename: %s",
              m_osIMDSourceFilename.c_str() );
}

/**
 * ~GDALMDReaderSpot()
 */
GDALMDReaderSpot::~GDALMDReaderSpot()
{
}

/**
 * LoadMetadata()
 */
void GDALMDReaderSpot::LoadMetadata()
{
    if(m_bIsMetadataLoad)
        return;

    if (!m_osIMDSourceFilename.empty())
    {
        CPLXMLNode* psNode = CPLParseXMLFile(m_osIMDSourceFilename);

        if(psNode != NULL)
        {
            CPLXMLNode* psisdNode = CPLSearchXMLNode(psNode, "=Dimap_Document");

            if(psisdNode != NULL)
            {
                m_papszIMDMD = ReadXMLToList(psisdNode->psChild, m_papszIMDMD);
            }
            CPLDestroyXMLNode(psNode);
        }
    }

    m_papszDEFAULTMD = CSLAddNameValue(m_papszDEFAULTMD, MD_NAME_MDTYPE, "DIMAP");

    m_bIsMetadataLoad = true;

    if(NULL == m_papszIMDMD)
    {
        return;
    }

    //extract imagery metadata
    int nCounter = -1;
    const char* pszSatId1 = CSLFetchNameValue(m_papszIMDMD,
                  "Dataset_Sources.Source_Information.Scene_Source.MISSION");
    if(NULL == pszSatId1)
    {
        nCounter = 1;
        for(int i = 0; i < 5; i++)
        {
            pszSatId1 = CSLFetchNameValue(m_papszIMDMD,
            CPLSPrintf("Dataset_Sources.Source_Information_%d.Scene_Source.MISSION",
                       nCounter));
            if(NULL != pszSatId1)
                break;
            nCounter++;
        }
    }


    const char* pszSatId2;
    if(nCounter == -1)
        pszSatId2 = CSLFetchNameValue(m_papszIMDMD,
            "Dataset_Sources.Source_Information.Scene_Source.MISSION_INDEX");
    else
        pszSatId2 = CSLFetchNameValue(m_papszIMDMD, CPLSPrintf(
            "Dataset_Sources.Source_Information_%d.Scene_Source.MISSION_INDEX",
            nCounter));

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


    const char* pszDate;
    if(nCounter == -1)
        pszDate = CSLFetchNameValue(m_papszIMDMD,
             "Dataset_Sources.Source_Information.Scene_Source.IMAGING_DATE");
    else
        pszDate = CSLFetchNameValue(m_papszIMDMD, CPLSPrintf(
             "Dataset_Sources.Source_Information_%d.Scene_Source.IMAGING_DATE",
             nCounter));

    if(NULL != pszDate)
    {
        const char* pszTime;
        if(nCounter == -1)
            pszTime = CSLFetchNameValue(m_papszIMDMD,
             "Dataset_Sources.Source_Information.Scene_Source.IMAGING_TIME");
        else
            pszTime = CSLFetchNameValue(m_papszIMDMD, CPLSPrintf(
             "Dataset_Sources.Source_Information_%d.Scene_Source.IMAGING_TIME",
             nCounter));

        if(NULL == pszTime)
            pszTime = "00:00:00.0Z";

        char buffer[80];
        time_t timeMid = GetAcquisitionTimeFromString(CPLSPrintf( "%sT%s",
                                                     pszDate, pszTime));
        strftime (buffer, 80, MD_DATETIMEFORMAT, localtime(&timeMid));
        m_papszIMAGERYMD = CSLAddNameValue(m_papszIMAGERYMD,
                                           MD_NAME_ACQDATETIME, buffer);
    }

    m_papszIMAGERYMD = CSLAddNameValue(m_papszIMAGERYMD, MD_NAME_CLOUDCOVER,
                                       MD_CLOUDCOVER_NA);
}


/**
 * ReadXMLToList()
 */
char** GDALMDReaderSpot::ReadXMLToList(CPLXMLNode* psNode, char** papszList,
                                          const char* pszName)
{
    if(NULL == psNode)
        return papszList;

    if (psNode->eType == CXT_Text)
    {
        if(!EQUAL(pszName, ""))
            return AddXMLNameValueToList(papszList, pszName, psNode->pszValue);
    }

    if (psNode->eType == CXT_Element && !EQUAL(psNode->pszValue, "Data_Strip"))
    {
        int nAddIndex = 0;
        bool bReset = false;
        for(CPLXMLNode* psChildNode = psNode->psChild; NULL != psChildNode;
            psChildNode = psChildNode->psNext)
        {
            if (psChildNode->eType == CXT_Element)
            {
                // check name duplicates
                if(NULL != psChildNode->psNext)
                {
                    if(bReset)
                    {
                        bReset = false;
                        nAddIndex = 0;
                    }

                    if(EQUAL(psChildNode->pszValue, psChildNode->psNext->pszValue))
                    {
                        nAddIndex++;
                    }
                    else
                    { // the name changed

                        if(nAddIndex > 0)
                        {
                            bReset = true;
                            nAddIndex++;
                        }
                    }
                }
                else
                {
                    if(nAddIndex > 0)
                    {
                        nAddIndex++;
                    }
                }

                char szName[512];
                if(nAddIndex > 0)
                {
                    CPLsnprintf( szName, 511, "%s_%d", psChildNode->pszValue,
                                 nAddIndex);
                }
                else
                {
                    CPLStrlcpy(szName, psChildNode->pszValue, 511);
                }

                char szNameNew[512];
                if(CPLStrnlen( pszName, 511 ) > 0) //if no prefix just set name to node name
                {
                    CPLsnprintf( szNameNew, 511, "%s.%s", pszName, szName );
                }
                else
                {
                    CPLsnprintf( szNameNew, 511, "%s.%s", psNode->pszValue, szName );
                }

                papszList = ReadXMLToList(psChildNode, papszList, szNameNew);
            }
            else
            {
                // Text nodes should always have name
                if(EQUAL(pszName, ""))
                {
                    papszList = ReadXMLToList(psChildNode, papszList, psNode->pszValue);
                }
                else
                {
                    papszList = ReadXMLToList(psChildNode, papszList, pszName);
                }
            }
        }
    }

    // proceed next only on top level

    if(NULL != psNode->psNext && EQUAL(pszName, ""))
    {
         papszList = ReadXMLToList(psNode->psNext, papszList, pszName);
    }

    return papszList;
}
