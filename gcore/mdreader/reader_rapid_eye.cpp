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
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include "reader_rapid_eye.h"

#include <ctime>

#include "cpl_conv.h"
#include "cpl_error.h"
#include "cpl_minixml.h"
#include "cpl_string.h"
#include "cpl_time.h"

/**
 * GDALMDReaderRapidEye()
 */
GDALMDReaderRapidEye::GDALMDReaderRapidEye(const char *pszPath,
                                           char **papszSiblingFiles)
    : GDALMDReaderBase(pszPath, papszSiblingFiles)
{
    const std::string osDirName = CPLGetDirnameSafe(pszPath);
    const std::string osBaseName = CPLGetBasenameSafe(pszPath);

    std::string osIMDSourceFilename = CPLFormFilenameSafe(
        osDirName.c_str(), (osBaseName + "_metadata").c_str(), "xml");
    if (CPLCheckForFile(&osIMDSourceFilename[0], papszSiblingFiles))
    {
        m_osXMLSourceFilename = std::move(osIMDSourceFilename);
    }
    else
    {
        osIMDSourceFilename = CPLFormFilenameSafe(
            osDirName.c_str(), (osBaseName + "_METADATA").c_str(), "XML");
        if (CPLCheckForFile(&osIMDSourceFilename[0], papszSiblingFiles))
        {
            m_osXMLSourceFilename = std::move(osIMDSourceFilename);
        }
    }

    if (!m_osXMLSourceFilename.empty())
        CPLDebug("MDReaderRapidEye", "XML Filename: %s",
                 m_osXMLSourceFilename.c_str());
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
char **GDALMDReaderRapidEye::GetMetadataFiles() const
{
    char **papszFileList = nullptr;
    if (!m_osXMLSourceFilename.empty())
        papszFileList = CSLAddString(papszFileList, m_osXMLSourceFilename);

    return papszFileList;
}

/**
 * LoadMetadata()
 */
void GDALMDReaderRapidEye::LoadMetadata()
{
    if (m_bIsMetadataLoad)
        return;

    CPLXMLNode *psNode = CPLParseXMLFile(m_osXMLSourceFilename);

    if (psNode != nullptr)
    {
        CPLXMLNode *pRootNode =
            CPLSearchXMLNode(psNode, "=re:EarthObservation");

        if (pRootNode != nullptr)
        {
            m_papszIMDMD = ReadXMLToList(pRootNode->psChild, m_papszIMDMD);
        }
        CPLDestroyXMLNode(psNode);
    }

    m_papszDEFAULTMD = CSLAddNameValue(m_papszDEFAULTMD, MD_NAME_MDTYPE, "RE");

    m_bIsMetadataLoad = true;

    if (nullptr == m_papszIMDMD)
    {
        return;
    }

    // extract imagery metadata
    const char *pszSatId = CSLFetchNameValue(
        m_papszIMDMD, "gml:using.eop:EarthObservationEquipment.eop:platform."
                      "eop:Platform.eop:serialIdentifier");
    if (nullptr != pszSatId)
    {
        m_papszIMAGERYMD = CSLAddNameValue(m_papszIMAGERYMD, MD_NAME_SATELLITE,
                                           CPLStripQuotes(pszSatId));
    }

    const char *pszDateTime = CSLFetchNameValue(
        m_papszIMDMD,
        "gml:using.eop:EarthObservationEquipment.eop:acquisitionParameters.re:"
        "Acquisition.re:acquisitionDateTime");
    if (nullptr != pszDateTime)
    {
        char buffer[80];
        GIntBig timeMid = GetAcquisitionTimeFromString(pszDateTime);
        struct tm tmBuf;
        strftime(buffer, 80, MD_DATETIMEFORMAT,
                 CPLUnixTimeToYMDHMS(timeMid, &tmBuf));
        m_papszIMAGERYMD =
            CSLAddNameValue(m_papszIMAGERYMD, MD_NAME_ACQDATETIME, buffer);
    }

    const char *pszCC = CSLFetchNameValue(
        m_papszIMDMD,
        "gml:resultOf.re:EarthObservationResult.opt:cloudCoverPercentage");
    if (nullptr != pszSatId)
    {
        m_papszIMAGERYMD =
            CSLAddNameValue(m_papszIMAGERYMD, MD_NAME_CLOUDCOVER, pszCC);
    }
}
