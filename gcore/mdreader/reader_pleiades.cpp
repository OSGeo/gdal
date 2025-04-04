/******************************************************************************
 *
 * Project:  GDAL Core
 * Purpose:  Read metadata from Pleiades imagery.
 * Author:   Alexander Lisovenko
 * Author:   Dmitry Baryshnikov, polimax@mail.ru
 *
 ******************************************************************************
 * Copyright (c) 2014-2018 NextGIS <info@nextgis.ru>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include "cpl_port.h"
#include "reader_pleiades.h"

#include <cstddef>
#include <cstdio>
#include <cstring>
#include <ctime>

#include <string>

#include "cpl_conv.h"
#include "cpl_error.h"
#include "cpl_minixml.h"
#include "cpl_string.h"
#include "cpl_time.h"

/**
 * GDALMDReaderPleiades()
 */
GDALMDReaderPleiades::GDALMDReaderPleiades(const char *pszPath,
                                           char **papszSiblingFiles)
    : GDALMDReaderBase(pszPath, papszSiblingFiles), m_osBaseFilename(pszPath),
      m_osIMDSourceFilename(CPLString()), m_osRPBSourceFilename(CPLString())
{
    const CPLString osBaseName = CPLGetBasenameSafe(pszPath);
    const size_t nBaseNameLen = osBaseName.size();
    if (nBaseNameLen < 4 || nBaseNameLen > 511)
        return;

    const CPLString osDirName = CPLGetDirnameSafe(pszPath);

    std::string osIMDSourceFilename = CPLFormFilenameSafe(
        osDirName, (std::string("DIM_") + (osBaseName.c_str() + 4)).c_str(),
        "XML");
    std::string osRPBSourceFilename = CPLFormFilenameSafe(
        osDirName, (std::string("RPC_") + (osBaseName.c_str() + 4)).c_str(),
        "XML");

    // find last underline
    char sBaseName[512];
    size_t nLastUnderline = 0;
    for (size_t i = 4; i < nBaseNameLen; i++)
    {
        sBaseName[i - 4] = osBaseName[i];
        if (osBaseName[i] == '_')
            nLastUnderline = i - 4U;
    }

    sBaseName[nLastUnderline] = 0;

    // Check if last 4 characters are fit in mask RjCj
    unsigned int iRow, iCol;
    bool bHasRowColPart = nBaseNameLen > nLastUnderline + 5U;
    if (!bHasRowColPart || sscanf(osBaseName.c_str() + nLastUnderline + 5U,
                                  "R%uC%u", &iRow, &iCol) != 2)
    {
        return;
    }

    // Strip of suffix from PNEO products
    char *pszLastUnderScore = strrchr(sBaseName, '_');
    if (pszLastUnderScore &&
        (EQUAL(pszLastUnderScore, "_P") || EQUAL(pszLastUnderScore, "_RGB") ||
         EQUAL(pszLastUnderScore, "_NED")))
    {
        *pszLastUnderScore = 0;
    }

    if (CPLCheckForFile(&osIMDSourceFilename[0], papszSiblingFiles))
    {
        m_osIMDSourceFilename = std::move(osIMDSourceFilename);
    }
    else
    {
        osIMDSourceFilename = CPLFormFilenameSafe(
            osDirName, ("DIM_" + std::string(sBaseName)).c_str(), "XML");
        if (CPLCheckForFile(&osIMDSourceFilename[0], papszSiblingFiles))
        {
            m_osIMDSourceFilename = std::move(osIMDSourceFilename);
        }
    }

    if (CPLCheckForFile(&osRPBSourceFilename[0], papszSiblingFiles))
    {
        m_osRPBSourceFilename = std::move(osRPBSourceFilename);
    }
    else
    {
        osRPBSourceFilename = CPLFormFilenameSafe(
            osDirName, ("RPC_" + std::string(sBaseName)).c_str(), "XML");
        if (CPLCheckForFile(&osRPBSourceFilename[0], papszSiblingFiles))
        {
            m_osRPBSourceFilename = std::move(osRPBSourceFilename);
        }
    }

    if (!m_osIMDSourceFilename.empty())
        CPLDebug("MDReaderPleiades", "IMD Filename: %s",
                 m_osIMDSourceFilename.c_str());
    if (!m_osRPBSourceFilename.empty())
        CPLDebug("MDReaderPleiades", "RPB Filename: %s",
                 m_osRPBSourceFilename.c_str());
}

GDALMDReaderPleiades::GDALMDReaderPleiades()
    : GDALMDReaderBase(nullptr, nullptr)
{
}

GDALMDReaderPleiades *
GDALMDReaderPleiades::CreateReaderForRPC(const char *pszRPCSourceFilename)
{
    GDALMDReaderPleiades *poReader = new GDALMDReaderPleiades();
    poReader->m_osRPBSourceFilename = pszRPCSourceFilename;
    return poReader;
}

/**
 * ~GDALMDReaderPleiades()
 */
GDALMDReaderPleiades::~GDALMDReaderPleiades()
{
}

/**
 * HasRequiredFiles()
 */
bool GDALMDReaderPleiades::HasRequiredFiles() const
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
char **GDALMDReaderPleiades::GetMetadataFiles() const
{
    char **papszFileList = nullptr;
    if (!m_osIMDSourceFilename.empty())
        papszFileList = CSLAddString(papszFileList, m_osIMDSourceFilename);
    if (!m_osRPBSourceFilename.empty())
        papszFileList = CSLAddString(papszFileList, m_osRPBSourceFilename);

    return papszFileList;
}

/**
 * LoadMetadata()
 */
void GDALMDReaderPleiades::LoadMetadata()
{
    if (m_bIsMetadataLoad)
        return;

    CPLXMLTreeCloser oIMDTree(nullptr);
    if (!m_osIMDSourceFilename.empty())
    {
        oIMDTree.reset(CPLParseXMLFile(m_osIMDSourceFilename));

        if (oIMDTree)
        {
            const CPLXMLNode *psisdNode =
                CPLSearchXMLNode(oIMDTree.get(), "=Dimap_Document");

            if (psisdNode != nullptr)
            {
                m_papszIMDMD = ReadXMLToList(psisdNode->psChild, m_papszIMDMD);
            }
        }
    }

    if (!m_osRPBSourceFilename.empty())
    {
        m_papszRPCMD = LoadRPCXmlFile(oIMDTree.get());
    }

    m_papszDEFAULTMD =
        CSLAddNameValue(m_papszDEFAULTMD, MD_NAME_MDTYPE, "DIMAP");

    m_bIsMetadataLoad = true;

    if (nullptr == m_papszIMDMD)
    {
        return;
    }

    // extract imagery metadata
    int nCounter = -1;
    const char *pszSatId1 = CSLFetchNameValue(
        m_papszIMDMD,
        "Dataset_Sources.Source_Identification.Strip_Source.MISSION");
    if (nullptr == pszSatId1)
    {
        nCounter = 1;
        for (int i = 0; i < 5; i++)
        {
            pszSatId1 = CSLFetchNameValue(
                m_papszIMDMD,
                CPLSPrintf("Dataset_Sources.Source_Identification_%d.Strip_"
                           "Source.MISSION",
                           nCounter));
            if (nullptr != pszSatId1)
                break;
            nCounter++;
        }
    }

    const char *pszSatId2;
    if (nCounter == -1)
        pszSatId2 = CSLFetchNameValue(
            m_papszIMDMD,
            "Dataset_Sources.Source_Identification.Strip_Source.MISSION_INDEX");
    else
        pszSatId2 = CSLFetchNameValue(
            m_papszIMDMD, CPLSPrintf("Dataset_Sources.Source_Identification_%d."
                                     "Strip_Source.MISSION_INDEX",
                                     nCounter));

    if (nullptr != pszSatId1 && nullptr != pszSatId2)
    {
        m_papszIMAGERYMD = CSLAddNameValue(
            m_papszIMAGERYMD, MD_NAME_SATELLITE,
            CPLSPrintf("%s %s", CPLStripQuotes(pszSatId1).c_str(),
                       CPLStripQuotes(pszSatId2).c_str()));
    }
    else if (nullptr != pszSatId1 && nullptr == pszSatId2)
    {
        m_papszIMAGERYMD = CSLAddNameValue(m_papszIMAGERYMD, MD_NAME_SATELLITE,
                                           CPLStripQuotes(pszSatId1));
    }
    else if (nullptr == pszSatId1 && nullptr != pszSatId2)
    {
        m_papszIMAGERYMD = CSLAddNameValue(m_papszIMAGERYMD, MD_NAME_SATELLITE,
                                           CPLStripQuotes(pszSatId2));
    }

    const char *pszDate;
    if (nCounter == -1)
        pszDate = CSLFetchNameValue(
            m_papszIMDMD,
            "Dataset_Sources.Source_Identification.Strip_Source.IMAGING_DATE");
    else
        pszDate = CSLFetchNameValue(
            m_papszIMDMD, CPLSPrintf("Dataset_Sources.Source_Identification_%d."
                                     "Strip_Source.IMAGING_DATE",
                                     nCounter));

    if (nullptr != pszDate)
    {
        const char *pszTime;
        if (nCounter == -1)
            pszTime = CSLFetchNameValue(m_papszIMDMD,
                                        "Dataset_Sources.Source_Identification."
                                        "Strip_Source.IMAGING_TIME");
        else
            pszTime = CSLFetchNameValue(
                m_papszIMDMD,
                CPLSPrintf("Dataset_Sources.Source_Identification_%d.Strip_"
                           "Source.IMAGING_TIME",
                           nCounter));

        if (nullptr == pszTime)
            pszTime = "00:00:00.0Z";

        char buffer[80];
        GIntBig timeMid =
            GetAcquisitionTimeFromString(CPLSPrintf("%sT%s", pszDate, pszTime));
        struct tm tmBuf;
        strftime(buffer, 80, MD_DATETIMEFORMAT,
                 CPLUnixTimeToYMDHMS(timeMid, &tmBuf));
        m_papszIMAGERYMD =
            CSLAddNameValue(m_papszIMAGERYMD, MD_NAME_ACQDATETIME, buffer);
    }

    m_papszIMAGERYMD =
        CSLAddNameValue(m_papszIMAGERYMD, MD_NAME_CLOUDCOVER, MD_CLOUDCOVER_NA);
}

/**
 * LoadRPCXmlFile()
 */

static const char *const apszRPBMapPleiades[] = {
    RPC_LINE_OFF,     "RFM_Validity.LINE_OFF",  // do not change order !
    RPC_SAMP_OFF,     "RFM_Validity.SAMP_OFF",  // do not change order !
    RPC_LAT_OFF,      "RFM_Validity.LAT_OFF",
    RPC_LONG_OFF,     "RFM_Validity.LONG_OFF",
    RPC_HEIGHT_OFF,   "RFM_Validity.HEIGHT_OFF",
    RPC_LINE_SCALE,   "RFM_Validity.LINE_SCALE",
    RPC_SAMP_SCALE,   "RFM_Validity.SAMP_SCALE",
    RPC_LAT_SCALE,    "RFM_Validity.LAT_SCALE",
    RPC_LONG_SCALE,   "RFM_Validity.LONG_SCALE",
    RPC_HEIGHT_SCALE, "RFM_Validity.HEIGHT_SCALE",
    nullptr,          nullptr};

static const char *const apszRPCTXT20ValItemsPleiades[] = {
    RPC_LINE_NUM_COEFF, RPC_LINE_DEN_COEFF, RPC_SAMP_NUM_COEFF,
    RPC_SAMP_DEN_COEFF, nullptr};

char **GDALMDReaderPleiades::LoadRPCXmlFile(const CPLXMLNode *psDIMRootNode)
{
    CPLXMLTreeCloser oNode(CPLParseXMLFile(m_osRPBSourceFilename));

    if (!oNode)
        return nullptr;

    CPLStringList aosRPC;

    // Fetch the "average" height from the Center Point in the DIM_xx.XML file
    if (psDIMRootNode)
    {
        const CPLXMLNode *psDimMainNode =
            CPLSearchXMLNode(psDIMRootNode, "=Dimap_Document");
        if (psDimMainNode)
        {
            // This is an WGS 84 ellipsoidal height.
            const char *pszH = CPLGetXMLValue(
                psDimMainNode, "Dataset_Content.Dataset_Extent.Center.H",
                nullptr);
            if (pszH)
            {
                aosRPC.SetNameValue("HEIGHT_DEFAULT", pszH);
            }
        }
    }

    // search Global_RFM
    CPLStringList aosRawRPCList;
    CPLXMLNode *pGRFMNode = CPLSearchXMLNode(oNode.get(), "=Global_RFM");

    if (pGRFMNode != nullptr)
    {
        aosRawRPCList = ReadXMLToList(pGRFMNode->psChild, nullptr);
    }
    else
    {
        pGRFMNode = CPLSearchXMLNode(oNode.get(), "=Rational_Function_Model");

        if (pGRFMNode != nullptr)
        {
            aosRawRPCList =
                ReadXMLToList(pGRFMNode->psChild, aosRawRPCList.StealList());
        }
    }

    if (aosRawRPCList.empty())
    {
        return nullptr;
    }

    // If we are not the top-left tile, then we must shift LINE_OFF and SAMP_OFF
    int nLineOffShift = 0;
    int nPixelOffShift = 0;
    for (int i = 1; TRUE; i++)
    {
        CPLString osKey;
        osKey.Printf("Raster_Data.Data_Access.Data_Files.Data_File_%d.DATA_"
                     "FILE_PATH.href",
                     i);
        const char *pszHref = CSLFetchNameValue(m_papszIMDMD, osKey);
        if (pszHref == nullptr)
            break;
        if (strcmp(CPLGetFilename(pszHref), CPLGetFilename(m_osBaseFilename)) ==
            0)
        {
            osKey.Printf(
                "Raster_Data.Data_Access.Data_Files.Data_File_%d.tile_C", i);
            const char *pszC = CSLFetchNameValue(m_papszIMDMD, osKey);
            osKey.Printf(
                "Raster_Data.Data_Access.Data_Files.Data_File_%d.tile_R", i);
            const char *pszR = CSLFetchNameValue(m_papszIMDMD, osKey);
            const char *pszTileWidth = CSLFetchNameValue(
                m_papszIMDMD, "Raster_Data.Raster_Dimensions.Tile_Set.Regular_"
                              "Tiling.NTILES_SIZE.ncols");
            const char *pszTileHeight = CSLFetchNameValue(
                m_papszIMDMD, "Raster_Data.Raster_Dimensions.Tile_Set.Regular_"
                              "Tiling.NTILES_SIZE.nrows");
            const char *pszOVERLAP_COL =
                CSLFetchNameValueDef(m_papszIMDMD,
                                     "Raster_Data.Raster_Dimensions.Tile_Set."
                                     "Regular_Tiling.OVERLAP_COL",
                                     "0");
            const char *pszOVERLAP_ROW =
                CSLFetchNameValueDef(m_papszIMDMD,
                                     "Raster_Data.Raster_Dimensions.Tile_Set."
                                     "Regular_Tiling.OVERLAP_ROW",
                                     "0");

            if (pszC && pszR && pszTileWidth && pszTileHeight &&
                atoi(pszOVERLAP_COL) == 0 && atoi(pszOVERLAP_ROW) == 0)
            {
                nLineOffShift = -(atoi(pszR) - 1) * atoi(pszTileHeight);
                nPixelOffShift = -(atoi(pszC) - 1) * atoi(pszTileWidth);
            }
            break;
        }
    }

    // SPOT and PHR sensors use 1,1 as their upper left corner pixel convention
    // for RPCs which is non standard. This was fixed with PNEO which correctly
    // assumes 0,0.
    // Precompute the offset that will be applied to LINE_OFF and SAMP_OFF
    // in order to use the RPCs with the standard 0,0 convention
    double topleftOffset;
    CPLXMLNode *psDoc = CPLGetXMLNode(oNode.get(), "=Dimap_Document");
    if (!psDoc)
        psDoc = CPLGetXMLNode(oNode.get(), "=PHR_DIMAP_Document");
    const char *pszMetadataProfile = CPLGetXMLValue(
        psDoc, "Metadata_Identification.METADATA_PROFILE", "PHR_SENSOR");
    if (EQUAL(pszMetadataProfile, "PHR_SENSOR") ||
        EQUAL(pszMetadataProfile, "S7_SENSOR") ||
        EQUAL(pszMetadataProfile, "S6_SENSOR"))
    {
        topleftOffset = 1;
    }
    else if (EQUAL(pszMetadataProfile, "PNEO_SENSOR"))
    {
        topleftOffset = 0;
    }
    else
    {
        //CPLError(CE_Warning, CPLE_AppDefined,
        //         "Unknown RPC Metadata Profile: %s. Assuming PHR_SENSOR",
        //         pszMetadataProfile);
        topleftOffset = 1;
    }

    // format list
    for (int i = 0; apszRPBMapPleiades[i] != nullptr; i += 2)
    {
        const char *pszValue =
            aosRawRPCList.FetchNameValue(apszRPBMapPleiades[i + 1]);
        if ((i == 0 || i == 2) && pszValue)  //i.e. LINE_OFF or SAMP_OFF
        {
            CPLString osField;
            double dfVal = CPLAtofM(pszValue) - topleftOffset;
            if (i == 0)
                dfVal += nLineOffShift;
            else
                dfVal += nPixelOffShift;
            osField.Printf("%.15g", dfVal);
            aosRPC.SetNameValue(apszRPBMapPleiades[i], osField);
        }
        else
        {
            aosRPC.SetNameValue(apszRPBMapPleiades[i], pszValue);
        }
    }

    // merge coefficients
    for (int i = 0; apszRPCTXT20ValItemsPleiades[i] != nullptr; i++)
    {
        CPLString value;
        for (int j = 1; j < 21; j++)
        {
            // We want to use the Inverse_Model
            // Quoting PleiadesUserGuideV2-1012.pdf:
            // """When using the inverse model (ground --> image), the user
            // supplies geographic coordinates (lon, lat) and an altitude
            // (alt)"""
            const char *pszValue = aosRawRPCList.FetchNameValue(CPLSPrintf(
                "Inverse_Model.%s_%d", apszRPCTXT20ValItemsPleiades[i], j));
            if (nullptr != pszValue)
            {
                value = value + " " + CPLString(pszValue);
            }
            else
            {
                pszValue = aosRawRPCList.FetchNameValue(
                    CPLSPrintf("GroundtoImage_Values.%s_%d",
                               apszRPCTXT20ValItemsPleiades[i], j));
                if (nullptr != pszValue)
                {
                    value = value + " " + CPLString(pszValue);
                }
            }
        }
        aosRPC.SetNameValue(apszRPCTXT20ValItemsPleiades[i], value);
    }

    return aosRPC.StealList();
}
