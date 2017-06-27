/******************************************************************************
 *
 * Project:  GDAL Core
 * Purpose:  Read metadata from Pleiades imagery.
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
#include "reader_pleiades.h"

#include <cstddef>
#include <cstring>
#include <ctime>

#include <string>

#include "cpl_conv.h"
#include "cpl_error.h"
#include "cpl_minixml.h"
#include "cpl_string.h"

CPL_CVSID("$Id$")

/**
 * GDALMDReaderPleiades()
 */
GDALMDReaderPleiades::GDALMDReaderPleiades(const char *pszPath,
                                        char **papszSiblingFiles) :
    GDALMDReaderBase(pszPath, papszSiblingFiles),
    m_osBaseFilename( pszPath ),
    m_osIMDSourceFilename( CPLString() ),
    m_osRPBSourceFilename( CPLString() )
{
    const char* pszBaseName = CPLGetBasename(pszPath);
    size_t nBaseNameLen = strlen(pszBaseName);
    if( nBaseNameLen < 4 || nBaseNameLen > 511 )
        return;

    const char* pszDirName = CPLGetDirname(pszPath);

    const char* pszIMDSourceFilename = CPLFormFilename( pszDirName,
                                CPLSPrintf("DIM_%s", pszBaseName + 4), "XML" );
    const char* pszRPBSourceFilename = CPLFormFilename( pszDirName,
                                CPLSPrintf("RPC_%s", pszBaseName + 4), "XML" );

    // find last underline
    char sBaseName[512];
    size_t nLastUnderline = 0;
    for(size_t i = 4; i < nBaseNameLen; i++)
    {
        sBaseName[i - 4] = pszBaseName[i];
        if(pszBaseName[i] == '_')
            nLastUnderline = i - 4U;
    }

    sBaseName[nLastUnderline] = 0;

    if (CPLCheckForFile((char*)pszIMDSourceFilename, papszSiblingFiles))
    {
        m_osIMDSourceFilename = pszIMDSourceFilename;
    }
    else
    {
        pszIMDSourceFilename = CPLFormFilename( pszDirName, CPLSPrintf("DIM_%s",
                                                            sBaseName), "XML" );
        if (CPLCheckForFile((char*)pszIMDSourceFilename, papszSiblingFiles))
        {
            m_osIMDSourceFilename = pszIMDSourceFilename;
        }
    }

    if (CPLCheckForFile((char*)pszRPBSourceFilename, papszSiblingFiles))
    {
        m_osRPBSourceFilename = pszRPBSourceFilename;
    }
    else
    {
        pszRPBSourceFilename = CPLFormFilename( pszDirName, CPLSPrintf("RPC_%s",
                                                            sBaseName), "XML" );
        if (CPLCheckForFile((char*)pszRPBSourceFilename, papszSiblingFiles))
        {
            m_osRPBSourceFilename = pszRPBSourceFilename;
        }
    }

    if( !m_osIMDSourceFilename.empty() )
        CPLDebug( "MDReaderPleiades", "IMD Filename: %s",
                  m_osIMDSourceFilename.c_str() );
    if( !m_osRPBSourceFilename.empty() )
        CPLDebug( "MDReaderPleiades", "RPB Filename: %s",
                  m_osRPBSourceFilename.c_str() );
}

GDALMDReaderPleiades::GDALMDReaderPleiades() : GDALMDReaderBase(NULL, NULL)
{
}

GDALMDReaderPleiades* GDALMDReaderPleiades::CreateReaderForRPC(const char* pszRPCSourceFilename)
{
    GDALMDReaderPleiades* poReader = new GDALMDReaderPleiades();
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
char** GDALMDReaderPleiades::GetMetadataFiles() const
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
void GDALMDReaderPleiades::LoadMetadata()
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

    if(!m_osRPBSourceFilename.empty())
    {
        m_papszRPCMD = LoadRPCXmlFile( );
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
                  "Dataset_Sources.Source_Identification.Strip_Source.MISSION");
    if(NULL == pszSatId1)
    {
        nCounter = 1;
        for(int i = 0; i < 5; i++)
        {
            pszSatId1 = CSLFetchNameValue(m_papszIMDMD,
            CPLSPrintf("Dataset_Sources.Source_Identification_%d.Strip_Source.MISSION",
                       nCounter));
            if(NULL != pszSatId1)
                break;
            nCounter++;
        }
    }

    const char* pszSatId2;
    if(nCounter == -1)
        pszSatId2 = CSLFetchNameValue(m_papszIMDMD,
            "Dataset_Sources.Source_Identification.Strip_Source.MISSION_INDEX");
    else
        pszSatId2 = CSLFetchNameValue(m_papszIMDMD, CPLSPrintf(
            "Dataset_Sources.Source_Identification_%d.Strip_Source.MISSION_INDEX",
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
             "Dataset_Sources.Source_Identification.Strip_Source.IMAGING_DATE");
    else
        pszDate = CSLFetchNameValue(m_papszIMDMD, CPLSPrintf(
             "Dataset_Sources.Source_Identification_%d.Strip_Source.IMAGING_DATE",
             nCounter));

    if(NULL != pszDate)
    {
        const char* pszTime;
        if(nCounter == -1)
            pszTime = CSLFetchNameValue(m_papszIMDMD,
             "Dataset_Sources.Source_Identification.Strip_Source.IMAGING_TIME");
        else
            pszTime = CSLFetchNameValue(m_papszIMDMD, CPLSPrintf(
             "Dataset_Sources.Source_Identification_%d.Strip_Source.IMAGING_TIME",
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
 * LoadRPCXmlFile()
 */

static const char * const apszRPBMap[] = {
    RPC_LINE_OFF,   "RFM_Validity.LINE_OFF", // do not change order !
    RPC_SAMP_OFF,   "RFM_Validity.SAMP_OFF", // do not change order !
    RPC_LAT_OFF,    "RFM_Validity.LAT_OFF",
    RPC_LONG_OFF,   "RFM_Validity.LONG_OFF",
    RPC_HEIGHT_OFF, "RFM_Validity.HEIGHT_OFF",
    RPC_LINE_SCALE, "RFM_Validity.LINE_SCALE",
    RPC_SAMP_SCALE, "RFM_Validity.SAMP_SCALE",
    RPC_LAT_SCALE,  "RFM_Validity.LAT_SCALE",
    RPC_LONG_SCALE, "RFM_Validity.LONG_SCALE",
    RPC_HEIGHT_SCALE,   "RFM_Validity.HEIGHT_SCALE",
    NULL,             NULL };

static const char * const apszRPCTXT20ValItems[] =
{
    RPC_LINE_NUM_COEFF,
    RPC_LINE_DEN_COEFF,
    RPC_SAMP_NUM_COEFF,
    RPC_SAMP_DEN_COEFF,
    NULL
};

char** GDALMDReaderPleiades::LoadRPCXmlFile()
{
    CPLXMLNode* pNode = CPLParseXMLFile(m_osRPBSourceFilename);

    if(NULL == pNode)
        return NULL;

    // search Global_RFM
    char** papszRawRPCList = NULL;
    CPLXMLNode* pGRFMNode = CPLSearchXMLNode(pNode, "=Global_RFM");

    if(pGRFMNode != NULL)
    {
        papszRawRPCList = ReadXMLToList(pGRFMNode->psChild, papszRawRPCList);
    }

    if( NULL == papszRawRPCList )
    {
        CPLDestroyXMLNode(pNode);
        return NULL;
    }

    // If we are not the top-left tile, then we must shift LINE_OFF and SAMP_OFF
    int nLineOffShift = 0;
    int nPixelOffShift = 0;
    for(int i=1; TRUE; i++ )
    {
        CPLString osKey;
        osKey.Printf("Raster_Data.Data_Access.Data_Files.Data_File_%d.DATA_FILE_PATH.href", i);
        const char* pszHref = CSLFetchNameValue(m_papszIMDMD, osKey);
        if( pszHref == NULL )
            break;
        if( strcmp( CPLGetFilename(pszHref), CPLGetFilename(m_osBaseFilename) ) == 0 )
        {
            osKey.Printf("Raster_Data.Data_Access.Data_Files.Data_File_%d.tile_C", i);
            const char* pszC = CSLFetchNameValue(m_papszIMDMD, osKey);
            osKey.Printf("Raster_Data.Data_Access.Data_Files.Data_File_%d.tile_R", i);
            const char* pszR = CSLFetchNameValue(m_papszIMDMD, osKey);
            const char* pszTileWidth = CSLFetchNameValue(m_papszIMDMD,
                "Raster_Data.Raster_Dimensions.Tile_Set.Regular_Tiling.NTILES_SIZE.ncols");
            const char* pszTileHeight = CSLFetchNameValue(m_papszIMDMD,
                "Raster_Data.Raster_Dimensions.Tile_Set.Regular_Tiling.NTILES_SIZE.nrows");
            const char* pszOVERLAP_COL = CSLFetchNameValueDef(m_papszIMDMD,
                "Raster_Data.Raster_Dimensions.Tile_Set.Regular_Tiling.OVERLAP_COL", "0");
            const char* pszOVERLAP_ROW = CSLFetchNameValueDef(m_papszIMDMD,
                "Raster_Data.Raster_Dimensions.Tile_Set.Regular_Tiling.OVERLAP_ROW", "0");

            if( pszC && pszR && pszTileWidth && pszTileHeight &&
                atoi(pszOVERLAP_COL) == 0 && atoi(pszOVERLAP_ROW) == 0 )
            {
                nLineOffShift = - (atoi(pszR) - 1) * atoi(pszTileHeight);
                nPixelOffShift = - (atoi(pszC) - 1) * atoi(pszTileWidth);
            }
            break;
        }
    }

    // format list
    char** papszRPB = NULL;
    for( int i = 0; apszRPBMap[i] != NULL; i += 2 )
    {
        // Pleiades RPCs use "center of upper left pixel is 1,1" convention, convert to
        // Digital globe convention of "center of upper left pixel is 0,0".
        if (i == 0 || i == 2)
        {
            CPLString osField;
            const char *pszOffset = CSLFetchNameValue(papszRawRPCList,
                                                    apszRPBMap[i + 1]);
            double dfVal = CPLAtofM( pszOffset ) -1.0 ;
            if( i == 0 )
                dfVal += nLineOffShift;
            else
                dfVal += nPixelOffShift;
            osField.Printf( "%.15g", dfVal );
            papszRPB = CSLAddNameValue( papszRPB, apszRPBMap[i], osField );
        }
        else
        {
            papszRPB = CSLAddNameValue(papszRPB, apszRPBMap[i],
                                    CSLFetchNameValue(papszRawRPCList,
                                                        apszRPBMap[i + 1]));
        }
    }

    // merge coefficients
    for( int i = 0; apszRPCTXT20ValItems[i] != NULL; i++ )
    {
        CPLString value;
        for( int j = 1; j < 21; j++ )
        {
            // We want to use the Inverse_Model
            // Quoting PleiadesUserGuideV2-1012.pdf:
            // """When using the inverse model (ground --> image), the user
            // supplies geographic coordinates (lon, lat) and an altitude (alt)"""
            const char* pszValue = CSLFetchNameValue(papszRawRPCList,
                 CPLSPrintf("Inverse_Model.%s_%d", apszRPCTXT20ValItems[i], j));
            if(NULL != pszValue)
                value = value + " " + CPLString(pszValue);
        }
        papszRPB = CSLAddNameValue(papszRPB, apszRPCTXT20ValItems[i], value);
    }

    CSLDestroy(papszRawRPCList);
    CPLDestroyXMLNode(pNode);
    return papszRPB;
}
