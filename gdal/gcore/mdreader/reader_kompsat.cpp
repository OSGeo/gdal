/******************************************************************************
 *
 * Project:  GDAL Core
 * Purpose:  Read metadata from Kompsat imagery.
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

#include "cpl_port.h"
#include "reader_kompsat.h"

#include <cstddef>
#include <cstdio>
#include <cstdlib>

#include "cpl_error.h"
#include "cpl_string.h"
#include "gdal_mdreader.h"
#include "gdal_priv.h"

CPL_CVSID("$Id$")

/**
 * GDALMDReaderKompsat()
 */
GDALMDReaderKompsat::GDALMDReaderKompsat(const char *pszPath,
                                         char **papszSiblingFiles) :
    GDALMDReaderBase(pszPath, papszSiblingFiles),
    m_osIMDSourceFilename ( GDALFindAssociatedFile( pszPath, "TXT",
                                                    papszSiblingFiles, 0 ) ),
    m_osRPBSourceFilename ( GDALFindAssociatedFile( pszPath, "RPC",
                                                    papszSiblingFiles, 0 ) )
{
    if( !m_osIMDSourceFilename.empty() )
        CPLDebug( "MDReaderDigitalGlobe", "IMD Filename: %s",
              m_osIMDSourceFilename.c_str() );
    if( !m_osRPBSourceFilename.empty() )
        CPLDebug( "MDReaderDigitalGlobe", "RPB Filename: %s",
              m_osRPBSourceFilename.c_str() );
}

/**
 * ~GDALMDReaderKompsat()
 */
GDALMDReaderKompsat::~GDALMDReaderKompsat()
{
}

/**
 * HasRequiredFiles()
 */
bool GDALMDReaderKompsat::HasRequiredFiles() const
{
    if (!m_osIMDSourceFilename.empty() && !m_osRPBSourceFilename.empty())
        return true;

    return false;
}

/**
 * GetMetadataFiles()
 */
char** GDALMDReaderKompsat::GetMetadataFiles() const
{
    char **papszFileList = NULL;
    if(!m_osIMDSourceFilename.empty())
        papszFileList= CSLAddString( papszFileList, m_osIMDSourceFilename );
    if(!m_osRPBSourceFilename.empty())
        papszFileList= CSLAddString( papszFileList, m_osRPBSourceFilename );

    return papszFileList;
}

/**
 * LoadMetadata()
 */
void GDALMDReaderKompsat::LoadMetadata()
{
    if(m_bIsMetadataLoad)
        return;

    if(!m_osIMDSourceFilename.empty())
    {
        m_papszIMDMD = ReadTxtToList( );
    }

    if(!m_osRPBSourceFilename.empty())
    {
        m_papszRPCMD = GDALLoadRPCFile( m_osRPBSourceFilename );
    }

    m_papszDEFAULTMD = CSLAddNameValue(m_papszDEFAULTMD, MD_NAME_MDTYPE, "KARI");

    m_bIsMetadataLoad = true;

    const char* pszSatId1 = CSLFetchNameValue(m_papszIMDMD, "AUX_SATELLITE_NAME");
    const char* pszSatId2 = CSLFetchNameValue(m_papszIMDMD, "AUX_SATELLITE_SENSOR");
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
                                                 "AUX_CLOUD_STATUS");
    if(NULL != pszCloudCover)
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

    const char* pszDate = CSLFetchNameValue(m_papszIMDMD,
                                            "AUX_STRIP_ACQ_DATE_UT");
    if(NULL != pszDate)
    {
        const char* pszTime = CSLFetchNameValue(m_papszIMDMD,
                                                "AUX_STRIP_ACQ_START_UT");

        if(NULL == pszTime)
            pszTime = "000000.000000";

        char buffer[80];
        time_t timeMid = GetAcquisitionTimeFromString(CPLSPrintf( "%sT%s",
                                                     pszDate, pszTime));
        strftime (buffer, 80, MD_DATETIMEFORMAT, localtime(&timeMid));
        m_papszIMAGERYMD = CSLAddNameValue(m_papszIMAGERYMD,
                                           MD_NAME_ACQDATETIME, buffer);
    }
}

/**
 * ReadTxtToList
 */
char** GDALMDReaderKompsat::ReadTxtToList()
{
    char** papszLines = CSLLoad(m_osIMDSourceFilename);
    if(NULL == papszLines)
        return NULL;

    char** papszIMD = NULL;
    char szName[512];
    int i;
    size_t j;
    CPLString soGroupName;

    for(i = 0; papszLines[i] != NULL; i++)
    {
        const char *pszLine = papszLines[i];
        const size_t nLineLenLimited = CPLStrnlen(pszLine, 512);

        //check if this is begin block
        if(STARTS_WITH_CI(pszLine, "BEGIN_"))
        {
            for(j = 6; j+1 < nLineLenLimited; j++)
            {
                if(STARTS_WITH_CI(pszLine + j, "_BLOCK"))
                {
                    szName[j - 6] = 0;
                    break;
                }
                szName[j - 6] = pszLine[j];
            }
            szName[j - 6] = '\0';

            soGroupName = szName;

            continue;
        }

        //check if this is end block
        if(STARTS_WITH_CI(pszLine, "END_"))
        {
            soGroupName.clear(); // we don't expect subblocks
            continue;
        }

        //get name and value
        for(j = 0; j+1 < nLineLenLimited; j++)
        {
            if(pszLine[j] == '\t')
            {
                if(soGroupName.empty() || j > 0)
                {
                    szName[j] = 0;
                    j++;
                    break;
                }
                else
                {
                    continue;
                }
            }
            szName[j] = pszLine[j];
        }
        szName[j] = '\0';

        // trim
        while( pszLine[j] == ' ' ) j++;

        if(soGroupName.empty())
        {
            papszIMD = CSLAddNameValue(papszIMD, szName, pszLine + j);
        }
        else
        {
            papszIMD = CSLAddNameValue(papszIMD, CPLSPrintf("%s.%s",
                                       soGroupName.c_str(), szName), pszLine + j);
        }
    }

    CSLDestroy(papszLines);

    return papszIMD;
}

/**
 * GetAcqisitionTimeFromString()
 */
time_t GDALMDReaderKompsat::GetAcquisitionTimeFromString(
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

    int r = sscanf ( pszDateTime, "%4d%2d%2dT%2d%2d%2d.%*s",
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
