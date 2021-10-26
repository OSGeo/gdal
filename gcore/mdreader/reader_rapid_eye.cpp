/******************************************************************************
 *
 * Project:  GDAL Core
 * Purpose:  Read metadata from RapidEye imagery.
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

#include "reader_rapid_eye.h"

#include <ctime>

#include "cpl_conv.h"
#include "cpl_error.h"
#include "cpl_minixml.h"
#include "cpl_string.h"

CPL_CVSID("$Id$")

/**
 * GDALMDReaderRapidEye()
 */
GDALMDReaderRapidEye::GDALMDReaderRapidEye(const char *pszPath,
        char **papszSiblingFiles) : GDALMDReaderBase(pszPath, papszSiblingFiles)
{
    const char* pszDirName = CPLGetDirname(pszPath);
    const char* pszBaseName = CPLGetBasename(pszPath);

    CPLString osIMDSourceFilename = CPLFormFilename( pszDirName,
                                                        CPLSPrintf("%s_metadata",
                                                        pszBaseName), "xml" );
    if (CPLCheckForFile(&osIMDSourceFilename[0], papszSiblingFiles))
    {
        m_osXMLSourceFilename = osIMDSourceFilename;
    }
    else
    {
        osIMDSourceFilename = CPLFormFilename( pszDirName,
                                                CPLSPrintf("%s_METADATA",
                                                pszBaseName), "XML" );
        if (CPLCheckForFile(&osIMDSourceFilename[0], papszSiblingFiles))
        {
            m_osXMLSourceFilename = osIMDSourceFilename;
        }
    }

    if(!m_osXMLSourceFilename.empty() )
        CPLDebug( "MDReaderRapidEye", "XML Filename: %s",
              m_osXMLSourceFilename.c_str() );
}

/**
 * ~GDALMDReaderRapidEye()
 */
GDALMDReaderRapidEye::~GDALMDReaderRapidEye()
{
}

/**
 * HasRequiredFiles()
 */
bool GDALMDReaderRapidEye::HasRequiredFiles() const
{
    // check re:EarthObservation
    if (!m_osXMLSourceFilename.empty() &&
            GDALCheckFileHeader(m_osXMLSourceFilename, "re:EarthObservation"))
        return true;

    return false;
}

/**
 * GetMetadataFiles()
 */
char** GDALMDReaderRapidEye::GetMetadataFiles() const
{
    char **papszFileList = nullptr;
    if(!m_osXMLSourceFilename.empty())
        papszFileList= CSLAddString( papszFileList, m_osXMLSourceFilename );

    return papszFileList;
}

/**
 * LoadMetadata()
 */
void GDALMDReaderRapidEye::LoadMetadata()
{
    if(m_bIsMetadataLoad)
        return;

    CPLXMLNode* psNode = CPLParseXMLFile(m_osXMLSourceFilename);

    if(psNode != nullptr)
    {
        CPLXMLNode* pRootNode = CPLSearchXMLNode(psNode, "=re:EarthObservation");

        if(pRootNode != nullptr)
        {
            m_papszIMDMD = ReadXMLToList(pRootNode->psChild, m_papszIMDMD);
        }
        CPLDestroyXMLNode(psNode);
    }

    m_papszDEFAULTMD = CSLAddNameValue(m_papszDEFAULTMD, MD_NAME_MDTYPE, "RE");

    m_bIsMetadataLoad = true;

    if(nullptr == m_papszIMDMD)
    {
        return;
    }

    //extract imagery metadata
    const char* pszSatId = CSLFetchNameValue(m_papszIMDMD,
    "gml:using.eop:EarthObservationEquipment.eop:platform.eop:Platform.eop:serialIdentifier");
    if(nullptr != pszSatId)
    {
        m_papszIMAGERYMD = CSLAddNameValue(m_papszIMAGERYMD,
                                MD_NAME_SATELLITE, CPLStripQuotes(pszSatId));
    }

    const char* pszDateTime = CSLFetchNameValue(m_papszIMDMD,
    "gml:using.eop:EarthObservationEquipment.eop:acquisitionParameters.re:Acquisition.re:acquisitionDateTime");
    if(nullptr != pszDateTime)
    {
        char buffer[80];
        time_t timeMid = GetAcquisitionTimeFromString(pszDateTime);
        strftime (buffer, 80, MD_DATETIMEFORMAT, localtime(&timeMid));
        m_papszIMAGERYMD = CSLAddNameValue(m_papszIMAGERYMD,
                                           MD_NAME_ACQDATETIME, buffer);
    }

    const char* pszCC = CSLFetchNameValue(m_papszIMDMD,
    "gml:resultOf.re:EarthObservationResult.opt:cloudCoverPercentage");
    if(nullptr != pszSatId)
    {
        m_papszIMAGERYMD = CSLAddNameValue(m_papszIMAGERYMD,
                                MD_NAME_CLOUDCOVER, pszCC);
    }
}
