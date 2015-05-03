/******************************************************************************
 * $Id$
 *
 * Project:  GDAL Core
 * Purpose:  Read metadata (mainly the remote sensing imagery) from files of 
 *           different providers like DigitalGlobe, GeoEye etc.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 * Author:   Dmitry Baryshnikov, polimax@mail.ru
 *
 ******************************************************************************
 * Copyright (c) HER MAJESTY THE QUEEN IN RIGHT OF CANADA (2008)
 * as represented by the Canadian Nuclear Safety Commission
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
 
#include "gdal_mdreader.h"
#include "cpl_string.h"
#include "cplkeywordparser.h"

//readers
#include "mdreader/reader_digital_globe.h"
#include "mdreader/reader_geo_eye.h"
#include "mdreader/reader_orb_view.h"
#include "mdreader/reader_pleiades.h"
#include "mdreader/reader_rdk1.h"
#include "mdreader/reader_landsat.h"

/**
 * The RPC parameters names
 */

static const char *apszRPCTXTSingleValItems[] =
{
    RPC_LINE_OFF,
    RPC_SAMP_OFF,
    RPC_LAT_OFF,
    RPC_LONG_OFF,
    RPC_HEIGHT_OFF,
    RPC_LINE_SCALE,
    RPC_SAMP_SCALE,
    RPC_LAT_SCALE,
    RPC_LONG_SCALE,
    RPC_HEIGHT_SCALE,
    NULL
};

static const char *apszRPCTXT20ValItems[] =
{
    RPC_LINE_NUM_COEFF,
    RPC_LINE_DEN_COEFF,
    RPC_SAMP_NUM_COEFF,
    RPC_SAMP_DEN_COEFF,
    NULL
};

/**
 * GDALMDReaderManager()
 */  
GDALMDReaderManager::GDALMDReaderManager()
{
    m_pReader = NULL;
}

/**
 * ~GDALMDReaderManager()
 */
GDALMDReaderManager::~GDALMDReaderManager()
{
   if(NULL != m_pReader)
   {
       delete m_pReader;
   }
}

/**
 * GetReader()
 */

#define INIT_READER(reader) \
    GDALMDReaderBase* pReaderBase = new reader(pszPath, papszSiblingFiles); \
    if(pReaderBase->HasRequiredFiles()) { m_pReader = pReaderBase; \
    return m_pReader; } \
    delete pReaderBase

GDALMDReaderBase* GDALMDReaderManager::GetReader(const char *pszPath,
                                                 char **papszSiblingFiles,
                                                 GUInt32 nType)
{
    if(nType & MDR_DG)
    {
        INIT_READER(GDALMDReaderDigitalGlobe);
    }

    // required filename.tif filename.pvl filename_rpc.txt
    if(nType & MDR_OV)
    {
        INIT_READER(GDALMDReaderOrbView);
    }

    // required filename.tif filename_rpc.txt (filename_metadata.txt optional)
    if(nType & MDR_GE)
    {
        INIT_READER(GDALMDReaderGeoEye);
    }

    if(nType & MDR_PLEIADES)
    {
        INIT_READER(GDALMDReaderPleiades);
    }

    if(nType & MDR_RDK1)
    {
        INIT_READER(GDALMDReaderResursDK1);
    }

    if(nType & MDR_LS)
    {
        INIT_READER(GDALMDReaderLandsat);
    }

    return NULL;
}

/**
 * GDALMDReaderBase()
 */ 
GDALMDReaderBase::GDALMDReaderBase(CPL_UNUSED const char *pszPath,
                                   CPL_UNUSED char **papszSiblingFiles)
{
    m_bIsMetadataLoad = false;    
    m_papszIMDMD = NULL;
    m_papszRPCMD = NULL;
    m_papszIMAGERYMD = NULL;
    m_papszDEFAULTMD = NULL;    
} 

/**
 * ~GDALMDReaderBase()
 */
GDALMDReaderBase::~GDALMDReaderBase()
{
    CSLDestroy(m_papszIMDMD);
    CSLDestroy(m_papszRPCMD);
    CSLDestroy(m_papszIMAGERYMD);
    CSLDestroy(m_papszDEFAULTMD);
} 

/**
 * GetMetadataItem()
 */
char ** GDALMDReaderBase::GetMetadataDomain(const char *pszDomain)
{
    LoadMetadata();
    if(EQUAL(pszDomain, MD_DOMAIN_DEFAULT))
        return m_papszDEFAULTMD;
    else if(EQUAL(pszDomain, MD_DOMAIN_IMD))
        return m_papszIMDMD;    
    else if(EQUAL(pszDomain, MD_DOMAIN_RPC))
        return m_papszRPCMD;   
    else if(EQUAL(pszDomain, MD_DOMAIN_IMAGERY))
        return m_papszIMAGERYMD;             
    return NULL;    
}

/**
 * LoadMetadata()
 */
void GDALMDReaderBase::LoadMetadata()
{
    if(m_bIsMetadataLoad)
        return;
    m_bIsMetadataLoad = true;
}

/**
 * GetAcqisitionTimeFromString1()
 */
const time_t GDALMDReaderBase::GetAcquisitionTimeFromString(
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

    int r = sscanf ( pszDateTime, "%d-%d-%dT%d:%d:%d.%*dZ",
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

/**
 * FillMetadata()
 */

#define SETMETADATA(mdmd, md, domain) if(NULL != md) { \
    char** papszCurrentMd = CSLDuplicate(mdmd->GetMetadata(domain)); \
    papszCurrentMd = CSLMerge(papszCurrentMd, md); \
    mdmd->SetMetadata(papszCurrentMd, domain); \
    CSLDestroy(papszCurrentMd); }

bool GDALMDReaderBase::FillMetadata(GDALMultiDomainMetadata* poMDMD)
{
    if(NULL == poMDMD)
        return false;
        
    LoadMetadata();

    SETMETADATA(poMDMD, m_papszIMDMD, MD_DOMAIN_IMD );
    SETMETADATA(poMDMD, m_papszRPCMD, MD_DOMAIN_RPC );
    SETMETADATA(poMDMD, m_papszIMAGERYMD, MD_DOMAIN_IMAGERY );
    SETMETADATA(poMDMD, m_papszDEFAULTMD, MD_DOMAIN_DEFAULT );
    
    return true;
}

/**
 * AddXMLNameValueToList()
 */
char** GDALMDReaderBase::AddXMLNameValueToList(char** papszList,
                                               const char *pszName,
                                               const char *pszValue)
{
    return CSLAddNameValue(papszList, pszName, pszValue);
}

/**
 * CPLReadXMLToList()
 */
char** GDALMDReaderBase::ReadXMLToList(CPLXMLNode* psNode, char** papszList,
                                          const char* pszName)
{
    if(NULL == psNode)
        return papszList;

    if (psNode->eType == CXT_Text)
    {
        return AddXMLNameValueToList(papszList, pszName, psNode->pszValue);
    }

    if (psNode->eType == CXT_Element)
    {

        int nAddIndex = 0;
        bool bReset = false;
        for(CPLXMLNode* psChildNode = psNode->psChild; NULL != psChildNode;
            psChildNode = psChildNode->psNext)
        {
            if (psChildNode->eType == CXT_Element)
            {
                // check name duplicates
                if(NULL != psChildNode->psNext)
                {
                    if(bReset)
                    {
                        bReset = false;
                        nAddIndex = 0;
                    }

                    if(EQUAL(psChildNode->pszValue, psChildNode->psNext->pszValue))
                    {
                        nAddIndex++;
                    }
                    else
                    { // the name changed

                        if(nAddIndex > 0)
                        {
                            bReset = true;
                            nAddIndex++;
                        }
                    }
                }

                char szName[512];
                if(nAddIndex > 0)
                {
                    CPLsnprintf( szName, 511, "%s_%d", psChildNode->pszValue,
                                 nAddIndex);
                }
                else
                {
                    CPLStrlcpy(szName, psChildNode->pszValue, 511);
                }

                char szNameNew[512];
                if(CPLStrnlen( pszName, 511 ) > 0) //if no prefix just set name to node name
                {
                    CPLsnprintf( szNameNew, 511, "%s.%s", pszName, szName );
                }
                else
                {
                    CPLsnprintf( szNameNew, 511, "%s.%s", psNode->pszValue, szName );
                }

                papszList = ReadXMLToList(psChildNode, papszList, szNameNew);
            }
            else
            {
                // Text nodes should always have name
                if(EQUAL(pszName, ""))
                {
                    papszList = ReadXMLToList(psChildNode, papszList, psNode->pszValue);
                }
                else
                {
                    papszList = ReadXMLToList(psChildNode, papszList, pszName);
                }
            }
        }

        // proceed next only on top level

        if(NULL != psNode->psNext && EQUAL(pszName, ""))
        {
             papszList = ReadXMLToList(psNode->psNext, papszList, pszName);
        }
    }

    return papszList;
}

//------------------------------------------------------------------------------
// Miscellaneous functions
//------------------------------------------------------------------------------


/**
 * GDALCheckFileHeader()
 */
const bool GDALCheckFileHeader(const CPLString& soFilePath,
                               const char * pszTestString, int nBufferSize)
{
    VSILFILE* fpL = VSIFOpenL( soFilePath, "r" );
    if( fpL == NULL )
        return false;
    char *pBuffer = new char[nBufferSize + 1];
    pBuffer[nBufferSize + 1] = 0;
    int nReadBytes = (int) VSIFReadL( pBuffer, 1, nBufferSize, fpL );
    VSIFCloseL(fpL);
    if(nReadBytes == 0)
    {
        delete [] pBuffer;
        return false;
    }

    bool bResult = strstr(pBuffer, pszTestString) != NULL;
    delete [] pBuffer;

    return bResult;
}

/**
 * CPLStrip()
 */
CPLString CPLStrip(const CPLString& sString, const char cChar)
{
    if(sString.empty())
        return sString;

    size_t dCopyFrom = 0;
    size_t dCopyCount = sString.size();

    if (sString[0] == cChar)
    {
        dCopyFrom++;
        dCopyCount--;
    }

    if (sString[sString.size() - 1] == cChar)
        dCopyCount--;

    if(dCopyCount == 0)
        return CPLString();
    
    return sString.substr(dCopyFrom, dCopyCount);
}

/**
 * CPLStripQuotes()
 */
CPLString CPLStripQuotes(const CPLString& sString)
{
    return CPLStrip( CPLStrip(sString, '"'), '\'');
}




/************************************************************************/
/*                          GDALLoadRPBFile()                           */
/************************************************************************/

static const char *apszRPBMap[] = {
    apszRPCTXTSingleValItems[0], "IMAGE.lineOffset",
    apszRPCTXTSingleValItems[1], "IMAGE.sampOffset",
    apszRPCTXTSingleValItems[2], "IMAGE.latOffset",
    apszRPCTXTSingleValItems[3], "IMAGE.longOffset",
    apszRPCTXTSingleValItems[4], "IMAGE.heightOffset",
    apszRPCTXTSingleValItems[5], "IMAGE.lineScale",
    apszRPCTXTSingleValItems[6], "IMAGE.sampScale",
    apszRPCTXTSingleValItems[7], "IMAGE.latScale",
    apszRPCTXTSingleValItems[8], "IMAGE.longScale",
    apszRPCTXTSingleValItems[9], "IMAGE.heightScale",
    apszRPCTXT20ValItems[0], "IMAGE.lineNumCoef",
    apszRPCTXT20ValItems[1], "IMAGE.lineDenCoef",
    apszRPCTXT20ValItems[2], "IMAGE.sampNumCoef",
    apszRPCTXT20ValItems[3], "IMAGE.sampDenCoef",
    NULL,             NULL };

char **GDALLoadRPBFile( const CPLString& soFilePath )
{
    if( soFilePath.empty() )
        return NULL;

/* -------------------------------------------------------------------- */
/*      Read file and parse.                                            */
/* -------------------------------------------------------------------- */
    CPLKeywordParser oParser;

    VSILFILE *fp = VSIFOpenL( soFilePath, "r" );

    if( fp == NULL )
        return NULL;

    if( !oParser.Ingest( fp ) )
    {
        VSIFCloseL( fp );
        return NULL;
    }

    VSIFCloseL( fp );

/* -------------------------------------------------------------------- */
/*      Extract RPC information, in a GDAL "standard" metadata format.  */
/* -------------------------------------------------------------------- */
    int i;
    char **papszMD = NULL;
    for( i = 0; apszRPBMap[i] != NULL; i += 2 )
    {
        const char *pszRPBVal = oParser.GetKeyword( apszRPBMap[i+1] );
        CPLString osAdjVal;

        if( pszRPBVal == NULL )
        {
            CPLError( CE_Failure, CPLE_AppDefined,
                      "%s file found, but missing %s field (and possibly others).",
                      soFilePath.c_str(), apszRPBMap[i+1] );
            CSLDestroy( papszMD );
            return NULL;
        }

        if( strchr(pszRPBVal,',') == NULL )
            osAdjVal = pszRPBVal;
        else
        {
            // strip out commas and turn newlines into spaces.
            int j;

            for( j = 0; pszRPBVal[j] != '\0'; j++ )
            {
                switch( pszRPBVal[j] )
                {
                  case ',':
                  case '\n':
                  case '\r':
                    osAdjVal += ' ';
                    break;

                  case '(':
                  case ')':
                    break;

                  default:
                    osAdjVal += pszRPBVal[j];
                }
            }
        }

        papszMD = CSLSetNameValue( papszMD, apszRPBMap[i], osAdjVal );
    }

    return papszMD;
}

/************************************************************************/
/*                          GDALLoadRPCFile()                           */
/************************************************************************/

/* Load a GeoEye _rpc.txt file. See ticket http://trac.osgeo.org/gdal/ticket/3639 */

char ** GDALLoadRPCFile( const CPLString& soFilePath )
{
    if( soFilePath.empty() )
        return NULL;

/* -------------------------------------------------------------------- */
/*      Read file and parse.                                            */
/* -------------------------------------------------------------------- */
    char **papszLines = CSLLoad2( soFilePath, 100, 100, NULL );
    if(!papszLines)
        return NULL;

    char **papszMD = NULL;

    /* From LINE_OFF to HEIGHT_SCALE */
    for(size_t i = 0; i < 19; i += 2 )
    {
        const char *pszRPBVal = CSLFetchNameValue(papszLines, apszRPBMap[i] );
        if( pszRPBVal == NULL )
        {
            CPLError( CE_Failure, CPLE_AppDefined,
                "%s file found, but missing %s field (and possibly others).",
                soFilePath.c_str(), apszRPBMap[i]);
            CSLDestroy( papszMD );
            CSLDestroy( papszLines );
            return NULL;
        }
        else
        {
            while( *pszRPBVal == ' ' ) pszRPBVal ++;
            papszMD = CSLSetNameValue( papszMD, apszRPBMap[i], pszRPBVal );
        }
    }

    /* For LINE_NUM_COEFF, LINE_DEN_COEFF, SAMP_NUM_COEFF, SAMP_DEN_COEFF */
    /* parameters that have 20 values each */
    for(size_t i = 20; apszRPBMap[i] != NULL; i += 2 )
    {
        CPLString soVal;
        for(int j = 1; j <= 20; j++)
        {
            CPLString soRPBMapItem;
            soRPBMapItem.Printf("%s_%d", apszRPBMap[i], j);
            const char *pszRPBVal = CSLFetchNameValue(papszLines, soRPBMapItem.c_str() );
            if( pszRPBVal == NULL )
            {
                CPLError( CE_Failure, CPLE_AppDefined,
                    "%s file found, but missing %s field (and possibly others).",
                    soFilePath.c_str(), soRPBMapItem.c_str() );
                CSLDestroy( papszMD );
                CSLDestroy( papszLines );
                return NULL;
            }
            else
            {
                while( *pszRPBVal == ' ' ) pszRPBVal ++;
                soVal += pszRPBVal;
                soVal += " ";
            }
        }
        papszMD = CSLSetNameValue( papszMD, apszRPBMap[i], soVal.c_str() );
    }

    CSLDestroy( papszLines );
    return papszMD;
}

/************************************************************************/
/*                         GDALWriteRPCTXTFile()                        */
/************************************************************************/

CPLErr CPL_STDCALL GDALWriteRPCTXTFile( const char *pszFilename, char **papszMD )

{
    CPLString osRPCFilename = pszFilename;
    CPLString soPt(".");
    size_t found = osRPCFilename.rfind(soPt);
    if (found == CPLString::npos)
        return CE_Failure;
    osRPCFilename.replace (found, osRPCFilename.size() - found, "_RPC.TXT");

/* -------------------------------------------------------------------- */
/*      Read file and parse.                                            */
/* -------------------------------------------------------------------- */
    VSILFILE *fp = VSIFOpenL( osRPCFilename, "w" );

    if( fp == NULL )
    {
        CPLError( CE_Failure, CPLE_OpenFailed,
                  "Unable to create %s for writing.\n%s",
                  osRPCFilename.c_str(), CPLGetLastErrorMsg() );
        return CE_Failure;
    }

/* -------------------------------------------------------------------- */
/*      Write RPC values from our RPC metadata.                         */
/* -------------------------------------------------------------------- */
    int i;

    for( i = 0; apszRPCTXTSingleValItems[i] != NULL; i ++ )
    {
        const char *pszRPCVal = CSLFetchNameValue( papszMD, apszRPCTXTSingleValItems[i] );
        if( pszRPCVal == NULL )
        {
            CPLError( CE_Failure, CPLE_AppDefined,
                      "%s field missing in metadata, %s file not written.",
                      apszRPCTXTSingleValItems[i], osRPCFilename.c_str() );
            VSIFCloseL( fp );
            VSIUnlink( osRPCFilename );
            return CE_Failure;
        }

        VSIFPrintfL( fp, "%s: %s\n", apszRPCTXTSingleValItems[i], pszRPCVal );
    }


    for( i = 0; apszRPCTXT20ValItems[i] != NULL; i ++ )
    {
        const char *pszRPCVal = CSLFetchNameValue( papszMD, apszRPCTXT20ValItems[i] );
        if( pszRPCVal == NULL )
        {
            CPLError( CE_Failure, CPLE_AppDefined,
                      "%s field missing in metadata, %s file not written.",
                      apszRPCTXTSingleValItems[i], osRPCFilename.c_str() );
            VSIFCloseL( fp );
            VSIUnlink( osRPCFilename );
            return CE_Failure;
        }

        char **papszItems = CSLTokenizeStringComplex( pszRPCVal, " ,",
                                                          FALSE, FALSE );

        if( CSLCount(papszItems) != 20 )
        {
            CPLError( CE_Failure, CPLE_AppDefined,
                      "%s field is corrupt (not 20 values), %s file not written.\n%s = %s",
                      apszRPCTXT20ValItems[i], osRPCFilename.c_str(),
                      apszRPCTXT20ValItems[i], pszRPCVal );
            VSIFCloseL( fp );
            VSIUnlink( osRPCFilename );
            CSLDestroy( papszItems );
            return CE_Failure;
        }

        int j;

        for( j = 0; j < 20; j++ )
        {
            VSIFPrintfL( fp, "%s_%d: %s\n", apszRPCTXT20ValItems[i], j+1,
                         papszItems[j] );
        }
        CSLDestroy( papszItems );
    }


    VSIFCloseL( fp );

    return CE_None;
}

/************************************************************************/
/*                          GDALWriteRPBFile()                          */
/************************************************************************/

CPLErr CPL_STDCALL GDALWriteRPBFile( const char *pszFilename, char **papszMD )

{
    CPLString osRPBFilename = CPLResetExtension( pszFilename, "RPB" );


/* -------------------------------------------------------------------- */
/*      Read file and parse.                                            */
/* -------------------------------------------------------------------- */
    VSILFILE *fp = VSIFOpenL( osRPBFilename, "w" );

    if( fp == NULL )
    {
        CPLError( CE_Failure, CPLE_OpenFailed,
                  "Unable to create %s for writing.\n%s",
                  osRPBFilename.c_str(), CPLGetLastErrorMsg() );
        return CE_Failure;
    }

/* -------------------------------------------------------------------- */
/*      Write the prefix information.                                   */
/* -------------------------------------------------------------------- */
    VSIFPrintfL( fp, "%s", "satId = \"QB02\";\n" );
    VSIFPrintfL( fp, "%s", "bandId = \"P\";\n" );
    VSIFPrintfL( fp, "%s", "SpecId = \"RPC00B\";\n" );
    VSIFPrintfL( fp, "%s", "BEGIN_GROUP = IMAGE\n" );
    VSIFPrintfL( fp, "%s", "\terrBias = 0.0;\n" );
    VSIFPrintfL( fp, "%s", "\terrRand = 0.0;\n" );

/* -------------------------------------------------------------------- */
/*      Write RPC values from our RPC metadata.                         */
/* -------------------------------------------------------------------- */
    int i;

    for( i = 0; apszRPBMap[i] != NULL; i += 2 )
    {
        const char *pszRPBVal = CSLFetchNameValue( papszMD, apszRPBMap[i] );
        const char *pszRPBTag;

        if( pszRPBVal == NULL )
        {
            CPLError( CE_Failure, CPLE_AppDefined,
                      "%s field missing in metadata, %s file not written.",
                      apszRPBMap[i], osRPBFilename.c_str() );
            VSIFCloseL( fp );
            VSIUnlink( osRPBFilename );
            return CE_Failure;
        }

        pszRPBTag = apszRPBMap[i+1];
        if( EQUALN(pszRPBTag,"IMAGE.",6) )
            pszRPBTag += 6;

        if( strstr(apszRPBMap[i], "COEF" ) == NULL )
        {
            VSIFPrintfL( fp, "\t%s = %s;\n", pszRPBTag, pszRPBVal );
        }
        else
        {
            // Reformat in brackets with commas over multiple lines.

            VSIFPrintfL( fp, "\t%s = (\n", pszRPBTag );

            char **papszItems = CSLTokenizeStringComplex( pszRPBVal, " ,",
                                                          FALSE, FALSE );

            if( CSLCount(papszItems) != 20 )
            {
                CPLError( CE_Failure, CPLE_AppDefined,
                          "%s field is corrupt (not 20 values), %s file not written.\n%s = %s",
                          apszRPBMap[i], osRPBFilename.c_str(),
                          apszRPBMap[i], pszRPBVal );
                VSIFCloseL( fp );
                VSIUnlink( osRPBFilename );
                CSLDestroy( papszItems );
                return CE_Failure;
            }

            int j;

            for( j = 0; j < 20; j++ )
            {
                if( j < 19 )
                    VSIFPrintfL( fp, "\t\t\t%s,\n", papszItems[j] );
                else
                    VSIFPrintfL( fp, "\t\t\t%s);\n", papszItems[j] );
            }
            CSLDestroy( papszItems );
        }
    }

/* -------------------------------------------------------------------- */
/*      Write end part                                                  */
/* -------------------------------------------------------------------- */
    VSIFPrintfL( fp, "%s", "END_GROUP = IMAGE\n" );
    VSIFPrintfL( fp, "END;\n" );
    VSIFCloseL( fp );

    return CE_None;
}

/************************************************************************/
/*                           GDAL_IMD_AA2R()                            */
/*                                                                      */
/*      Translate AA version IMD file to R version.                     */
/************************************************************************/

static int GDAL_IMD_AA2R( char ***ppapszIMD )

{
    char **papszIMD = *ppapszIMD;

/* -------------------------------------------------------------------- */
/*      Verify that we have a new format file.                          */
/* -------------------------------------------------------------------- */
    const char *pszValue = CSLFetchNameValue( papszIMD, "version" );

    if( pszValue == NULL )
        return FALSE;

    if( EQUAL(pszValue,"\"R\"") )
        return TRUE;

    if( !EQUAL(pszValue,"\"AA\"") )
    {
        CPLDebug( "IMD", "The file is not the expected 'version = \"AA\"' format.\nProceeding, but file may be corrupted." );
    }

/* -------------------------------------------------------------------- */
/*      Fix the version line.                                           */
/* -------------------------------------------------------------------- */
    papszIMD = CSLSetNameValue( papszIMD, "version", "\"R\"" );

/* -------------------------------------------------------------------- */
/*      remove a bunch of fields.                                       */
/* -------------------------------------------------------------------- */
    int iKey;

    static const char *apszToRemove[] = {
        "productCatalogId",
        "childCatalogId",
        "productType",
        "numberOfLooks",
        "effectiveBandwidth",
        "mode",
        "scanDirection",
        "cloudCover",
        "productGSD",
        NULL };

    for( iKey = 0; apszToRemove[iKey] != NULL; iKey++ )
    {
        int iTarget = CSLFindName( papszIMD, apszToRemove[iKey] );
        if( iTarget != -1 )
            papszIMD = CSLRemoveStrings( papszIMD, iTarget, 1, NULL );
    }

/* -------------------------------------------------------------------- */
/*      Replace various min/mean/max with just the mean.                */
/* -------------------------------------------------------------------- */
    static const char *keylist[] = {
        "CollectedRowGSD",
        "CollectedColGSD",
        "SunAz",
        "SunEl",
        "SatAz",
        "SatEl",
        "InTrackViewAngle",
        "CrossTrackViewAngle",
        "OffNadirViewAngle",
        NULL };

    for( iKey = 0; keylist[iKey] != NULL; iKey++ )
    {
        CPLString osTarget;
        int       iTarget;

        osTarget.Printf( "IMAGE_1.min%s", keylist[iKey] );
        iTarget = CSLFindName( papszIMD, osTarget );
        if( iTarget != -1 )
            papszIMD = CSLRemoveStrings( papszIMD, iTarget, 1, NULL );

        osTarget.Printf( "IMAGE_1.max%s", keylist[iKey] );
        iTarget = CSLFindName( papszIMD, osTarget );
        if( iTarget != -1 )
            papszIMD = CSLRemoveStrings( papszIMD, iTarget, 1, NULL );

        osTarget.Printf( "IMAGE_1.mean%s", keylist[iKey] );
        iTarget = CSLFindName( papszIMD, osTarget );
        if( iTarget != -1 )
        {
            CPLString osValue = CSLFetchNameValue( papszIMD, osTarget );
            CPLString osLine;

            osTarget.Printf( "IMAGE_1.%c%s",
                             tolower(keylist[iKey][0]),
                             keylist[iKey]+1 );

            osLine = osTarget + "=" + osValue;

            CPLFree( papszIMD[iTarget] );
            papszIMD[iTarget] = CPLStrdup(osLine);
        }
    }

    *ppapszIMD = papszIMD;
    return TRUE;
}

/************************************************************************/
/*                          GDALLoadIMDFile()                           */
/************************************************************************/

char ** GDALLoadIMDFile( const CPLString& osFilePath )
{
    if( osFilePath.empty() )
        return NULL;

/* -------------------------------------------------------------------- */
/*      Read file and parse.                                            */
/* -------------------------------------------------------------------- */
    CPLKeywordParser oParser;

    VSILFILE *fp = VSIFOpenL( osFilePath, "r" );

    if( fp == NULL )
        return NULL;

    if( !oParser.Ingest( fp ) )
    {
        VSIFCloseL( fp );
        return NULL;
    }

    VSIFCloseL( fp );

/* -------------------------------------------------------------------- */
/*      Consider version changing.                                      */
/* -------------------------------------------------------------------- */
    char **papszIMD = CSLDuplicate( oParser.GetAllKeywords() );
    const char *pszVersion = CSLFetchNameValue( papszIMD, "version" );

    if( pszVersion == NULL )
    {
        /* ? */;
    }
    else if( EQUAL(pszVersion,"\"AA\"") )
    {
        GDAL_IMD_AA2R( &papszIMD );
    }

    return papszIMD;
}

/************************************************************************/
/*                       GDALWriteIMDMultiLine()                        */
/*                                                                      */
/*      Write a value that is split over multiple lines.                */
/************************************************************************/

static void GDALWriteIMDMultiLine( VSILFILE *fp, const char *pszValue )

{
    char **papszItems = CSLTokenizeStringComplex( pszValue, "(,) ",
                                                  FALSE, FALSE );
    int nItemCount = CSLCount(papszItems);
    int i;

    VSIFPrintfL( fp, "(\n" );

    for( i = 0; i < nItemCount; i++ )
    {
        if( i == nItemCount-1 )
            VSIFPrintfL( fp, "\t%s );\n", papszItems[i] );
        else
            VSIFPrintfL( fp, "\t%s,\n", papszItems[i] );
    }
    CSLDestroy( papszItems );
}

/************************************************************************/
/*                          GDALWriteIMDFile()                          */
/************************************************************************/

CPLErr CPL_STDCALL GDALWriteIMDFile( const char *pszFilename, char **papszMD )

{
    CPLString osRPBFilename = CPLResetExtension( pszFilename, "IMD" );

/* -------------------------------------------------------------------- */
/*      Read file and parse.                                            */
/* -------------------------------------------------------------------- */
    VSILFILE *fp = VSIFOpenL( osRPBFilename, "w" );

    if( fp == NULL )
    {
        CPLError( CE_Failure, CPLE_OpenFailed,
                  "Unable to create %s for writing.\n%s",
                  osRPBFilename.c_str(), CPLGetLastErrorMsg() );
        return CE_Failure;
    }

/* ==================================================================== */
/* -------------------------------------------------------------------- */
/*      Loop through all values writing.                                */
/* -------------------------------------------------------------------- */
/* ==================================================================== */
    int iKey;
    CPLString osCurSection;

    for( iKey = 0; papszMD[iKey] != NULL; iKey++ )
    {
        char *pszRawKey = NULL;
        const char *pszValue = CPLParseNameValue( papszMD[iKey], &pszRawKey );
        CPLString osKeySection, osKeyItem;
        char *pszDot = strchr(pszRawKey,'.');

/* -------------------------------------------------------------------- */
/*      Split stuff like BAND_P.ULLon into section and item.            */
/* -------------------------------------------------------------------- */
        if( pszDot == NULL )
        {
            osKeyItem = pszRawKey;
        }
        else
        {
            osKeyItem = pszDot+1;
            *pszDot = '\0';
            osKeySection = pszRawKey;
        }
        CPLFree( pszRawKey );

/* -------------------------------------------------------------------- */
/*      Close and/or start sections as needed.                          */
/* -------------------------------------------------------------------- */
        if( osCurSection.size() && !EQUAL(osCurSection,osKeySection) )
            VSIFPrintfL( fp, "END_GROUP = %s\n", osCurSection.c_str() );

        if( osKeySection.size() && !EQUAL(osCurSection,osKeySection) )
            VSIFPrintfL( fp, "BEGIN_GROUP = %s\n", osKeySection.c_str() );

        osCurSection = osKeySection;

/* -------------------------------------------------------------------- */
/*      Print out simple item.                                          */
/* -------------------------------------------------------------------- */
        if( osCurSection.size() )
            VSIFPrintfL( fp, "\t%s = ", osKeyItem.c_str() );
        else
            VSIFPrintfL( fp, "%s = ", osKeyItem.c_str() );

        if( pszValue[0] != '(' )
            VSIFPrintfL( fp, "%s;\n", pszValue );
        else
            GDALWriteIMDMultiLine( fp, pszValue );
    }

/* -------------------------------------------------------------------- */
/*      Close off.                                                      */
/* -------------------------------------------------------------------- */
    if( osCurSection.size() )
        VSIFPrintfL( fp, "END_GROUP = %s\n", osCurSection.c_str() );

    VSIFPrintfL( fp, "END;\n" );

    VSIFCloseL( fp );

    return CE_None;
}
