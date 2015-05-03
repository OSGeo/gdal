/******************************************************************************
 * $Id$
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

#include "reader_landsat.h"

/**
 * GDALMDReaderLandsat()
 */
GDALMDReaderLandsat::GDALMDReaderLandsat(const char *pszPath,
        char **papszSiblingFiles) : GDALMDReaderBase(pszPath, papszSiblingFiles)
{
    const char* pszBaseName = CPLGetBasename(pszPath);
    const char* pszDirName = CPLGetDirname(pszPath);

    // split file name by _B or _b
    char szMetadataName[512] = {0};
    size_t i;
    for(i = 0; i < CPLStrnlen(pszBaseName, 511); i++)
    {
        szMetadataName[i] = pszBaseName[i];
        if(EQUALN(pszBaseName + i, "_B", 2) || EQUALN(pszBaseName + i, "_b", 2))
        {
            break;
        }
    }

    // form metadata file name
    CPLStrlcpy(szMetadataName + i, "_MTL.txt", 9);

    CPLDebug( "MDReaderLandsat", "Try IMD Filename: %s",
              szMetadataName );

    const char* pszIMDSourceFilename = CPLFormFilename( pszDirName,
                                                        szMetadataName, NULL );
    if (CPLCheckForFile((char*)pszIMDSourceFilename, papszSiblingFiles))
    {
        m_osIMDSourceFilename = pszIMDSourceFilename;
    }
    else
    {
        CPLStrlcpy(szMetadataName + i, "_MTL.TXT", 9);
        pszIMDSourceFilename = CPLFormFilename( pszDirName, szMetadataName, NULL );
        if (CPLCheckForFile((char*)pszIMDSourceFilename, papszSiblingFiles))
        {
            m_osIMDSourceFilename = pszIMDSourceFilename;
        }
    }

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
const bool GDALMDReaderLandsat::HasRequiredFiles() const
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
    char **papszFileList = NULL;
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
    if(NULL != pszSatId)
    {
        m_papszIMAGERYMD = CSLAddNameValue(m_papszIMAGERYMD, MD_NAME_SATELLITE,
                                           CPLStripQuotes(pszSatId));
    }

    // L1_METADATA_FILE.IMAGE_ATTRIBUTES.CLOUD_COVER
    const char* pszCloudCover = CSLFetchNameValue(m_papszIMDMD,
                            "L1_METADATA_FILE.IMAGE_ATTRIBUTES.CLOUD_COVER");
    if(NULL != pszCloudCover)
    {
        float fCC = CPLAtofM(pszCloudCover);
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
    if(NULL == pszDate)
    {
        pszDate = CSLFetchNameValue(m_papszIMDMD,
                             "L1_METADATA_FILE.PRODUCT_METADATA.DATE_ACQUIRED");
    }

    if(NULL != pszDate)
    {
        const char* pszTime = CSLFetchNameValue(m_papszIMDMD,
                    "L1_METADATA_FILE.PRODUCT_METADATA.SCENE_CENTER_SCAN_TIME");
        if(NULL == pszTime)
        {
            pszTime = CSLFetchNameValue(m_papszIMDMD,
                         "L1_METADATA_FILE.PRODUCT_METADATA.SCENE_CENTER_TIME");
        }
        if(NULL == pszTime)
            pszTime = "00:00:00.000000Z";

        char buffer[80];
        time_t timeMid = GetAcquisitionTimeFromString(CPLSPrintf( "%sT%s",
                                                     pszDate, pszTime));
        strftime (buffer, 80, MD_DATETIMEFORMAT, localtime(&timeMid));
        m_papszIMAGERYMD = CSLAddNameValue(m_papszIMAGERYMD,
                                           MD_NAME_ACQDATETIME, buffer);
    }

}
