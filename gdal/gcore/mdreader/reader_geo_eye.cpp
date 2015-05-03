/******************************************************************************
 * $Id$
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
 
#include "reader_geo_eye.h"

CPL_CVSID("$Id$");

/**
 * GDALMDReaderGeoEye()
 */
GDALMDReaderGeoEye::GDALMDReaderGeoEye(const char *pszPath, 
        char **papszSiblingFiles) : GDALMDReaderBase(pszPath, papszSiblingFiles)
{
    
    const char* pszBaseName = CPLGetBasename(pszPath);
    const char* pszDirName = CPLGetDirname(pszPath);

    // get _metadata.txt file
    
    // split file name by _rgb_ or _pan_
    char szMetadataName[512] = {0};
    size_t i;
    for(i = 0; i < CPLStrnlen(pszBaseName, 511); i++)
    {
        szMetadataName[i] = pszBaseName[i];
        if(EQUALN(pszBaseName + i, "_rgb_", 5) || EQUALN(pszBaseName + i, "_pan_", 5))
        {
            break;
        }
    }
    
    // form metadata file name
    CPLStrlcpy(szMetadataName + i, "_metadata.txt", 14);
    const char* pszIMDSourceFilename = CPLFormFilename( pszDirName,
                                                        szMetadataName, NULL );
    if (CPLCheckForFile((char*)pszIMDSourceFilename, papszSiblingFiles))
    {
        m_osIMDSourceFilename = pszIMDSourceFilename;
    }                                                     
    else
    {
        CPLStrlcpy(szMetadataName + i, "_METADATA.TXT", 14);
        pszIMDSourceFilename = CPLFormFilename( pszDirName, szMetadataName, NULL );
        if (CPLCheckForFile((char*)pszIMDSourceFilename, papszSiblingFiles))
        {
            m_osIMDSourceFilename = pszIMDSourceFilename;
        }
    }

    // get _rpc.txt file
    
    const char* pszRPBSourceFilename = CPLFormFilename( pszDirName,
                                                        CPLSPrintf("%s_rpc",
                                                        pszBaseName),
                                                        "txt" );
    if (CPLCheckForFile((char*)pszRPBSourceFilename, papszSiblingFiles))
    {
        m_osRPBSourceFilename = pszRPBSourceFilename;
    }
    else
    {
        pszRPBSourceFilename = CPLFormFilename( pszDirName, CPLSPrintf("%s_RPC",
                                                pszBaseName), "TXT" );
        if (CPLCheckForFile((char*)pszRPBSourceFilename, papszSiblingFiles))
        {
            m_osRPBSourceFilename = pszRPBSourceFilename;
        }
    }

    CPLDebug( "MDReaderGeoEye", "IMD Filename: %s",
              m_osIMDSourceFilename.c_str() );
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
const bool GDALMDReaderGeoEye::HasRequiredFiles() const
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
    char **papszFileList = NULL;
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
    
    if(NULL == m_papszIMDMD)
    {
        return;
    }   
    
    //extract imagery metadata
    const char* pszSatId = CSLFetchNameValue(m_papszIMDMD,
                                             "Source Image Metadata.Sensor");
    if(NULL != pszSatId)
    {
        m_papszIMAGERYMD = CSLAddNameValue(m_papszIMAGERYMD,
                                           MD_NAME_SATELLITE,
                                           CPLStripQuotes(pszSatId));
    }
        
    const char* pszCloudCover = CSLFetchNameValue(m_papszIMDMD,
                                   "Source Image Metadata.Percent Cloud Cover");
    if(NULL != pszCloudCover)
    {
        m_papszIMAGERYMD = CSLAddNameValue(m_papszIMAGERYMD,
                                           MD_NAME_CLOUDCOVER, pszCloudCover);
    }
    
    const char* pszDateTime = CSLFetchNameValue(m_papszIMDMD,
                                 "Source Image Metadata.Acquisition Date/Time");
                                         
    if(NULL != pszDateTime)
    {
        char buffer[80];
        time_t timeMid = GetAcquisitionTimeFromString(pszDateTime);

        strftime (buffer, 80, MD_DATETIMEFORMAT, localtime(&timeMid));
        m_papszIMAGERYMD = CSLAddNameValue(m_papszIMAGERYMD, 
                                           MD_NAME_ACQDATETIME, buffer);
    }  
}

/**
 * GetAcqisitionTimeFromString()
 */
const time_t GDALMDReaderGeoEye::GetAcquisitionTimeFromString(
        const char* pszDateTime)
{
    if(NULL == pszDateTime)
        return 0;
        
    int iYear;
    int iMonth;
    int iDay;
    int iHours;
    int iMin;
    int iSec = 0;

// string exampe: Acquisition Date/Time: 2006-03-01 11:08 GMT

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

    return mktime(&tmDateTime);
}

/**
 * LoadWKTIMDFile()
 */
char** GDALMDReaderGeoEye::LoadIMDWktFile() const
{	
    char** papszResultList = NULL;
    char** papszLines = CSLLoad( m_osIMDSourceFilename );
    bool bBeginSection = false;
    CPLString osSection;
    CPLString osKeyLevel1;
    CPLString osKeyLevel2;
    CPLString osKeyLevel3;
    int nLevel = 0;
    int nSpaceCount;

    if( papszLines == NULL )
        return NULL;

    for( int i = 0; papszLines[i] != NULL; i++ )
    {
        // skip section (=== or ---) lines

        if(EQUALN( papszLines[i], "===",3))
        {
            bBeginSection = true;
            continue;
        }

        if(EQUALN( papszLines[i], "---",3) || CPLStrnlen(papszLines[i], 512) == 0)
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
        char *pszKey = NULL;
        pszValue = CPLParseNameValue(papszLines[i], &pszKey);

        if(NULL != pszValue && CPLStrnlen(pszValue, 512) > 0)
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

        if(NULL != pszKey && CPLStrnlen(pszKey, 512) > 0)
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
