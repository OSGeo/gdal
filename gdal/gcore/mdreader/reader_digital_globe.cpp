/******************************************************************************
 * $Id$
 *
 * Project:  GDAL Core
 * Purpose:  Read metadata from DigitalGlobe imagery.
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
 
#include "reader_digital_globe.h"
 
/**
 * GDALMDReaderDigitalGlobe()
 */
GDALMDReaderDigitalGlobe::GDALMDReaderDigitalGlobe(const char *pszPath, 
        char **papszSiblingFiles) : GDALMDReaderBase(pszPath, papszSiblingFiles)
{
    m_osIMDSourceFilename = GDALFindAssociatedFile( pszPath, "IMD", 
                                                         papszSiblingFiles, 0 );
    m_osRPBSourceFilename = GDALFindAssociatedFile( pszPath, "RPB", 
                                                         papszSiblingFiles, 0 );    
    m_osXMLSourceFilename = GDALFindAssociatedFile( pszPath, "XML", 
                                                         papszSiblingFiles, 0 );


    CPLDebug( "MDReaderDigitalGlobe", "IMD Filename: %s",
              m_osIMDSourceFilename.c_str() );
    CPLDebug( "MDReaderDigitalGlobe", "RPB Filename: %s",
              m_osRPBSourceFilename.c_str() );
    CPLDebug( "MDReaderDigitalGlobe", "XML Filename: %s",
              m_osXMLSourceFilename.c_str() );
}

/**
 * ~GDALMDReaderDigitalGlobe()
 */ 
GDALMDReaderDigitalGlobe::~GDALMDReaderDigitalGlobe()
{
    
}

/**
 * HasRequiredFiles()
 */
const bool GDALMDReaderDigitalGlobe::HasRequiredFiles() const
{
    if (!m_osIMDSourceFilename.empty())
        return true;
    if (!m_osRPBSourceFilename.empty())
        return true;

    // check <isd>
    if(!m_osXMLSourceFilename.empty() &&
            GDALCheckFileHeader(m_osXMLSourceFilename, "<isd>"))
        return true;

    return false;
}

/**
 * LoadMetadata()
 */
void GDALMDReaderDigitalGlobe::LoadMetadata()
{
    if(m_bIsMetadataLoad)
        return;
        
    if (!m_osIMDSourceFilename.empty())
    {
        m_papszIMDMD = GDALLoadIMDFile( m_osIMDSourceFilename );
    }

    if(!m_osRPBSourceFilename.empty())
    {
        m_papszRPCMD = GDALLoadRPBFile( m_osRPBSourceFilename );
    }

    if((NULL == m_papszIMDMD || NULL == m_papszRPCMD) && !m_osXMLSourceFilename.empty())
    { 
        CPLXMLNode* psNode = CPLParseXMLFile(m_osXMLSourceFilename);
       
        if(psNode != NULL)
        {
            CPLXMLNode* psisdNode = psNode->psNext;
            if(psisdNode != NULL)
            {
                if( m_papszIMDMD == NULL )
                    m_papszIMDMD = LoadIMDXmlNode( CPLSearchXMLNode(psisdNode,
                                                                        "IMD") );
                if( m_papszRPCMD == NULL )
                    m_papszRPCMD = LoadRPBXmlNode( CPLSearchXMLNode(psisdNode,
                                                                        "RPB") );
            }
            CPLDestroyXMLNode(psNode);
        }
    }

    m_papszDEFAULTMD = CSLAddNameValue(m_papszDEFAULTMD, MD_NAME_MDTYPE, "DG");
           
    m_bIsMetadataLoad = true;      
    
    if(NULL == m_papszIMDMD)
    {
        return;
    }   
    //extract imagery metadata
    const char* pszSatId = CSLFetchNameValue(m_papszIMDMD, "IMAGE.SATID");
    if(NULL != pszSatId)
    {
        m_papszIMAGERYMD = CSLAddNameValue(m_papszIMAGERYMD, 
                                           MD_NAME_SATELLITE,
                                           CPLStripQuotes(pszSatId));
    }
    else
    {
        pszSatId = CSLFetchNameValue(m_papszIMDMD, "IMAGE_1.SATID");
        if(NULL != pszSatId)
        {
            m_papszIMAGERYMD = CSLAddNameValue(m_papszIMAGERYMD,
                                               MD_NAME_SATELLITE,
                                               CPLStripQuotes(pszSatId));
        }
    }
    
    const char* pszCloudCover = CSLFetchNameValue(m_papszIMDMD, 
                                                  "IMAGE.CLOUDCOVER");
    if(NULL != pszCloudCover)
    {
        double fCC = CPLAtofM(pszCloudCover);
        if(fCC < 0)
        {
            m_papszIMAGERYMD = CSLAddNameValue(m_papszIMAGERYMD, MD_NAME_CLOUDCOVER,
                                               MD_CLOUDCOVER_NA);
        }
        else
        {
            m_papszIMAGERYMD = CSLAddNameValue(m_papszIMAGERYMD,
                          MD_NAME_CLOUDCOVER, CPLSPrintf("%d", int(fCC * 100)));
        }
    }
    else
    {
        pszCloudCover = CSLFetchNameValue(m_papszIMDMD, "IMAGE_1.cloudCover");
        if(NULL != pszCloudCover)
        {
            double fCC = CPLAtofM(pszCloudCover);
            if(fCC < 0)
            {
                m_papszIMAGERYMD = CSLAddNameValue(m_papszIMAGERYMD, MD_NAME_CLOUDCOVER,
                                                   MD_CLOUDCOVER_NA);
            }
            else
            {
                m_papszIMAGERYMD = CSLAddNameValue(m_papszIMAGERYMD,
                              MD_NAME_CLOUDCOVER, CPLSPrintf("%d", int(fCC * 100)));
            }
        }
    }
    
    const char* pszDateTime = CSLFetchNameValue(m_papszIMDMD,
                                       "IMAGE.FIRSTLINETIME");
    if(NULL != pszDateTime)
    {
        time_t timeStart = GetAcquisitionTimeFromString(pszDateTime);
        char szMidDateTime[80];
        strftime (szMidDateTime, 80, MD_DATETIMEFORMAT, localtime(&timeStart));

        m_papszIMAGERYMD = CSLAddNameValue(m_papszIMAGERYMD,
                                           MD_NAME_ACQDATETIME,
                                           szMidDateTime);
    }
    else
    {
        pszDateTime = CSLFetchNameValue(m_papszIMDMD, "IMAGE_1.firstLineTime");
        if(NULL != pszDateTime)
        {
            time_t timeStart = GetAcquisitionTimeFromString(pszDateTime);
            char szMidDateTime[80];
            strftime (szMidDateTime, 80, MD_DATETIMEFORMAT, localtime(&timeStart));

            m_papszIMAGERYMD = CSLAddNameValue(m_papszIMAGERYMD,
                                               MD_NAME_ACQDATETIME,
                                               szMidDateTime);
        }
    }
}

/**
 * GetMetadataFiles()
 */
char** GDALMDReaderDigitalGlobe::GetMetadataFiles() const
{
    char **papszFileList = NULL;
    if(!m_osIMDSourceFilename.empty()) 
        papszFileList = CSLAddString( papszFileList, m_osIMDSourceFilename );
    if(!m_osRPBSourceFilename.empty()) 
        papszFileList = CSLAddString( papszFileList, m_osRPBSourceFilename );
    if(!m_osXMLSourceFilename.empty()) 
        papszFileList = CSLAddString( papszFileList, m_osXMLSourceFilename );
    
    return papszFileList;
}

/**
 * GDALLoadIMDXmlNode()
 */
char** GDALMDReaderDigitalGlobe::LoadIMDXmlNode(CPLXMLNode* psNode)
{
    if(NULL == psNode)
        return NULL;    
    char** papszList = NULL;
    return ReadXMLToList(psNode->psChild, papszList);
}

/**
 * GDALLoadRPBXmlNode()
 */
static const char *apszRPBMap[] = {
    RPC_LINE_OFF,   "image.lineOffset",
    RPC_SAMP_OFF,   "image.sampOffset",
    RPC_LAT_OFF,    "image.latOffset",
    RPC_LONG_OFF,   "image.longOffset",
    RPC_HEIGHT_OFF, "image.heightOffset",
    RPC_LINE_SCALE, "image.lineScale",
    RPC_SAMP_SCALE, "image.sampScale",
    RPC_LAT_SCALE,  "image.latScale",
    RPC_LONG_SCALE, "image.longScale",
    RPC_HEIGHT_SCALE,   "image.heightScale",
    RPC_LINE_NUM_COEFF, "image.lineNumCoefList.lineNumCoef",
    RPC_LINE_DEN_COEFF, "image.lineDenCoefList.lineDenCoef",
    RPC_SAMP_NUM_COEFF, "image.sampNumCoefList.sampNumCoef",
    RPC_SAMP_DEN_COEFF, "image.sampDenCoefList.sampDenCoef",
    NULL,             NULL };

char** GDALMDReaderDigitalGlobe::LoadRPBXmlNode(CPLXMLNode* psNode)
{
    if(NULL == psNode)
        return NULL;
    char** papszList = NULL;
    papszList = ReadXMLToList(psNode->psChild, papszList);

    if(NULL == papszList)
        return NULL;
    
    char** papszRPB = NULL;
    for( int i = 0; apszRPBMap[i] != NULL; i += 2 )
    {
        papszRPB = CSLAddNameValue(papszRPB, apszRPBMap[i],
                               CSLFetchNameValue(papszList, apszRPBMap[i + 1]));
    }

    CSLDestroy(papszList);
      
    return papszRPB;
}
