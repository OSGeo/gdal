/******************************************************************************
 *
 * Project:  GDAL Core
 * Purpose:  Read metadata from OrbView imagery.
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
#include "reader_orb_view.h"

#include <ctime>

#include "cpl_conv.h"
#include "cpl_error.h"
#include "cpl_string.h"
#include "gdal_priv.h"

CPL_CVSID("$Id$")

/**
 * GDALMDReaderOrbView()
 */
GDALMDReaderOrbView::GDALMDReaderOrbView(const char *pszPath,
                                         char **papszSiblingFiles) :
    GDALMDReaderBase(pszPath, papszSiblingFiles),
    m_osIMDSourceFilename( GDALFindAssociatedFile( pszPath, "PVL",
                                                    papszSiblingFiles, 0 ) ),
    m_osRPBSourceFilename( CPLString() )
{
    const char* pszBaseName = CPLGetBasename(pszPath);
    const char* pszDirName = CPLGetDirname(pszPath);

    CPLString osRPBSourceFilename = CPLFormFilename( pszDirName,
                                                        CPLSPrintf("%s_rpc",
                                                        pszBaseName),
                                                        "txt" );
    if (CPLCheckForFile(&osRPBSourceFilename[0], papszSiblingFiles))
    {
        m_osRPBSourceFilename = osRPBSourceFilename;
    }
    else
    {
        osRPBSourceFilename = CPLFormFilename( pszDirName, CPLSPrintf("%s_RPC",
                                                pszBaseName), "TXT" );
        if (CPLCheckForFile(&osRPBSourceFilename[0], papszSiblingFiles))
        {
            m_osRPBSourceFilename = osRPBSourceFilename;
        }
    }

    if( !m_osIMDSourceFilename.empty() )
        CPLDebug( "MDReaderOrbView", "IMD Filename: %s",
                  m_osIMDSourceFilename.c_str() );
    if( !m_osRPBSourceFilename.empty() )
        CPLDebug( "MDReaderOrbView", "RPB Filename: %s",
                  m_osRPBSourceFilename.c_str() );
}

/**
 * ~GDALMDReaderOrbView()
 */
GDALMDReaderOrbView::~GDALMDReaderOrbView()
{
}

/**
 * HasRequiredFiles()
 */
bool GDALMDReaderOrbView::HasRequiredFiles() const
{
    if (!m_osIMDSourceFilename.empty() && !m_osRPBSourceFilename.empty())
        return true;

    return false;
}

/**
 * GetMetadataFiles()
 */
char** GDALMDReaderOrbView::GetMetadataFiles() const
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
void GDALMDReaderOrbView::LoadMetadata()
{
    if(m_bIsMetadataLoad)
        return;

    if (!m_osIMDSourceFilename.empty())
    {
        m_papszIMDMD = GDALLoadIMDFile( m_osIMDSourceFilename );
    }

    if(!m_osRPBSourceFilename.empty())
    {
        m_papszRPCMD = GDALLoadRPCFile( m_osRPBSourceFilename );
    }

    m_papszDEFAULTMD = CSLAddNameValue(m_papszDEFAULTMD, MD_NAME_MDTYPE, "OV");

    m_bIsMetadataLoad = true;

    if(nullptr == m_papszIMDMD)
    {
        return;
    }

    //extract imagery metadata
    const char* pszSatId = CSLFetchNameValue(m_papszIMDMD,
                                             "sensorInfo.satelliteName");
    if(nullptr != pszSatId)
    {
        m_papszIMAGERYMD = CSLAddNameValue(m_papszIMAGERYMD,
                                           MD_NAME_SATELLITE,
                                           CPLStripQuotes(pszSatId));
    }

    const char* pszCloudCover = CSLFetchNameValue(m_papszIMDMD,
                                   "productInfo.productCloudCoverPercentage");
    if(nullptr != pszCloudCover)
    {
        m_papszIMAGERYMD = CSLAddNameValue(m_papszIMAGERYMD,
                                           MD_NAME_CLOUDCOVER, pszCloudCover);
    }

    const char* pszDateTime = CSLFetchNameValue(m_papszIMDMD,
                                 "inputImageInfo.firstLineAcquisitionDateTime");

    if(nullptr != pszDateTime)
    {
        char buffer[80];
        time_t timeMid = GetAcquisitionTimeFromString(pszDateTime);
        strftime (buffer, 80, MD_DATETIMEFORMAT, localtime(&timeMid));
        m_papszIMAGERYMD = CSLAddNameValue(m_papszIMAGERYMD,
                                           MD_NAME_ACQDATETIME, buffer);
    }
}
