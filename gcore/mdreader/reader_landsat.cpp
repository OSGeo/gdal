/******************************************************************************
 *
 * Project:  GDAL Core
 * Purpose:  Read metadata from Landsat imagery.
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
#include "reader_landsat.h"

#include <cstddef>
#include <cstring>
#include <ctime>

#include "cpl_conv.h"
#include "cpl_error.h"
#include "cpl_string.h"
#include "cpl_time.h"

CPL_CVSID("$Id$")

/**
 * GDALMDReaderLandsat()
 */
GDALMDReaderLandsat::GDALMDReaderLandsat(const char *pszPath,
        char **papszSiblingFiles) : GDALMDReaderBase(pszPath, papszSiblingFiles)
{
    const char* pszBaseName = CPLGetBasename(pszPath);
    const char* pszDirName = CPLGetDirname(pszPath);
    size_t nBaseNameLen = strlen(pszBaseName);
    if( nBaseNameLen > 511 )
        return;

    // split file name by _B or _b
    char szMetadataName[512] = {0};
    size_t i;
    for(i = 0; i < nBaseNameLen; i++)
    {
        szMetadataName[i] = pszBaseName[i];
        if(STARTS_WITH_CI(pszBaseName + i, "_B") || STARTS_WITH_CI(pszBaseName + i, "_b"))
        {
            break;
        }
    }

    // form metadata file name
    CPLStrlcpy(szMetadataName + i, "_MTL.txt", 9);

    CPLString osIMDSourceFilename = CPLFormFilename( pszDirName,
                                                        szMetadataName, nullptr );
    if (CPLCheckForFile(&osIMDSourceFilename[0], papszSiblingFiles))
    {
        m_osIMDSourceFilename = osIMDSourceFilename;
    }
    else
    {
        CPLStrlcpy(szMetadataName + i, "_MTL.TXT", 9);
        osIMDSourceFilename = CPLFormFilename( pszDirName, szMetadataName, nullptr );
        if (CPLCheckForFile(&osIMDSourceFilename[0], papszSiblingFiles))
        {
            m_osIMDSourceFilename = osIMDSourceFilename;
        }
    }

    if( !m_osIMDSourceFilename.empty() )
        CPLDebug( "MDReaderLandsat", "IMD Filename: %s",
                  m_osIMDSourceFilename.c_str() );
}

/**
 * ~GDALMDReaderLandsat()
 */
GDALMDReaderLandsat::~GDALMDReaderLandsat()
{
}

/**
 * HasRequiredFiles()
 */
bool GDALMDReaderLandsat::HasRequiredFiles() const
{
    if (!m_osIMDSourceFilename.empty())
        return true;

    return false;
}

/**
 * GetMetadataFiles()
 */
char** GDALMDReaderLandsat::GetMetadataFiles() const
{
    char **papszFileList = nullptr;
    if(!m_osIMDSourceFilename.empty())
        papszFileList= CSLAddString( papszFileList, m_osIMDSourceFilename );

    return papszFileList;
}

/**
 * LoadMetadata()
 */
void GDALMDReaderLandsat::LoadMetadata()
{
    if(m_bIsMetadataLoad)
        return;

    if (!m_osIMDSourceFilename.empty())
    {
        m_papszIMDMD = GDALLoadIMDFile( m_osIMDSourceFilename );
    }

    m_papszDEFAULTMD = CSLAddNameValue(m_papszDEFAULTMD, MD_NAME_MDTYPE, "ODL");

    m_bIsMetadataLoad = true;

    // date/time
    // DATE_ACQUIRED = 2013-04-07
    // SCENE_CENTER_TIME = 15:47:03.0882620Z

    // L1_METADATA_FILE.PRODUCT_METADATA.SPACECRAFT_ID
    const char* pszSatId = CSLFetchNameValue(m_papszIMDMD,
                            "L1_METADATA_FILE.PRODUCT_METADATA.SPACECRAFT_ID");
    if(nullptr != pszSatId)
    {
        m_papszIMAGERYMD = CSLAddNameValue(m_papszIMAGERYMD, MD_NAME_SATELLITE,
                                           CPLStripQuotes(pszSatId));
    }

    // L1_METADATA_FILE.IMAGE_ATTRIBUTES.CLOUD_COVER
    const char* pszCloudCover = CSLFetchNameValue(m_papszIMDMD,
                            "L1_METADATA_FILE.IMAGE_ATTRIBUTES.CLOUD_COVER");
    if(nullptr != pszCloudCover)
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
                          MD_NAME_CLOUDCOVER, CPLSPrintf("%d", int(fCC)));
        }
    }

    // L1_METADATA_FILE.PRODUCT_METADATA.ACQUISITION_DATE
    // L1_METADATA_FILE.PRODUCT_METADATA.SCENE_CENTER_SCAN_TIME

    // L1_METADATA_FILE.PRODUCT_METADATA.DATE_ACQUIRED
    // L1_METADATA_FILE.PRODUCT_METADATA.SCENE_CENTER_TIME

    const char* pszDate = CSLFetchNameValue(m_papszIMDMD,
                          "L1_METADATA_FILE.PRODUCT_METADATA.ACQUISITION_DATE");
    if(nullptr == pszDate)
    {
        pszDate = CSLFetchNameValue(m_papszIMDMD,
                             "L1_METADATA_FILE.PRODUCT_METADATA.DATE_ACQUIRED");
    }

    if(nullptr != pszDate)
    {
        const char* pszTime = CSLFetchNameValue(m_papszIMDMD,
                    "L1_METADATA_FILE.PRODUCT_METADATA.SCENE_CENTER_SCAN_TIME");
        if(nullptr == pszTime)
        {
            pszTime = CSLFetchNameValue(m_papszIMDMD,
                         "L1_METADATA_FILE.PRODUCT_METADATA.SCENE_CENTER_TIME");
        }
        if(nullptr == pszTime)
            pszTime = "00:00:00.000000Z";

        char buffer[80];
        GIntBig timeMid = GetAcquisitionTimeFromString(CPLSPrintf( "%sT%s",
                                                     pszDate, pszTime));
        struct tm tmBuf;
        strftime (buffer, 80, MD_DATETIMEFORMAT, CPLUnixTimeToYMDHMS(timeMid, &tmBuf));
        m_papszIMAGERYMD = CSLAddNameValue(m_papszIMAGERYMD,
                                           MD_NAME_ACQDATETIME, buffer);
    }
}
