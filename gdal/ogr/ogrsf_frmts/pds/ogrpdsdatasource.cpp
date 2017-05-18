/******************************************************************************
 *
 * Project:  PDS Translator
 * Purpose:  Implements OGRPDSDataSource class
 * Author:   Even Rouault, even dot rouault at mines dash paris dot org
 *
 ******************************************************************************
 * Copyright (c) 2010-2011, Even Rouault <even dot rouault at mines-paris dot org>
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

#include "cpl_conv.h"
#include "cpl_string.h"
#include "ogr_pds.h"

CPL_CVSID("$Id$");

using namespace OGRPDS;

/************************************************************************/
/*                           OGRPDSDataSource()                         */
/************************************************************************/

OGRPDSDataSource::OGRPDSDataSource() :
    pszName(NULL),
    papoLayers(NULL),
    nLayers(0)
{}

/************************************************************************/
/*                        ~OGRPDSDataSource()                           */
/************************************************************************/

OGRPDSDataSource::~OGRPDSDataSource()

{
    for( int i = 0; i < nLayers; i++ )
        delete papoLayers[i];
    CPLFree( papoLayers );

    CPLFree( pszName );
}

/************************************************************************/
/*                           TestCapability()                           */
/************************************************************************/

int OGRPDSDataSource::TestCapability( const char * /* pszCap */ )
{
    return FALSE;
}

/************************************************************************/
/*                              GetLayer()                              */
/************************************************************************/

OGRLayer *OGRPDSDataSource::GetLayer( int iLayer )

{
    if( iLayer < 0 || iLayer >= nLayers )
        return NULL;

    return papoLayers[iLayer];
}

/************************************************************************/
/*                          GetKeywordSub()                             */
/************************************************************************/

const char * OGRPDSDataSource::GetKeywordSub( const char *pszPath,
                                              int iSubscript,
                                              const char *pszDefault )

{
    const char *pszResult = oKeywords.GetKeyword( pszPath, NULL );

    if( pszResult == NULL )
        return pszDefault;

    if( pszResult[0] != '(' )
        return pszDefault;

    char **papszTokens = CSLTokenizeString2( pszResult, "(,)",
                                             CSLT_HONOURSTRINGS );

    if( iSubscript <= CSLCount(papszTokens) )
    {
        osTempResult = papszTokens[iSubscript-1];
        CSLDestroy( papszTokens );
        return osTempResult.c_str();
    }

    CSLDestroy( papszTokens );
    return pszDefault;
}

/************************************************************************/
/*                            CleanString()                             */
/*                                                                      */
/* Removes single or double quotes, and converts spaces to underscores. */
/* The change is made in-place to CPLString.                            */
/************************************************************************/

void OGRPDSDataSource::CleanString( CPLString &osInput )

{
    if( ( osInput.size() < 2 ) ||
      ((osInput.at(0) != '"'   || osInput.at(osInput.size()-1) != '"' ) &&
       ( osInput.at(0) != '\'' || osInput.at(osInput.size()-1) != '\'')) )
         return;

    char *pszWrk = CPLStrdup(osInput.c_str() + 1);

    pszWrk[strlen(pszWrk)-1] = '\0';

    for( int i = 0; pszWrk[i] != '\0'; i++ )
    {
        if( pszWrk[i] == ' ' )
            pszWrk[i] = '_';
    }

    osInput = pszWrk;
    CPLFree( pszWrk );
}

/************************************************************************/
/*                           LoadTable()                                */
/************************************************************************/

static CPLString MakeAttr(CPLString os1, CPLString os2)
{
    return os1 + "." + os2;
}

bool OGRPDSDataSource::LoadTable( const char* pszFilename,
                                  int nRecordSize,
                                  CPLString osTableID )
{
    CPLString osTableFilename;
    int nStartBytes = 0;

    CPLString osTableLink = "^";
    osTableLink += osTableID;

    CPLString osTable = oKeywords.GetKeyword( osTableLink, "" );
    if( osTable[0] == '(' )
    {
        osTableFilename = GetKeywordSub(osTableLink, 1, "");
        CPLString osStartRecord = GetKeywordSub(osTableLink, 2, "");
        nStartBytes = atoi(osStartRecord.c_str()) - 1;
        if( nStartBytes < 0 ||
            (( nRecordSize > 0 && nStartBytes > INT_MAX / nRecordSize )) )
        {
            CPLError(CE_Failure, CPLE_NotSupported,
                     "Invalid StartBytes value");
            return false;
        }
        nStartBytes *= nRecordSize;
        if (osTableFilename.empty() || osStartRecord.empty() ||
            nStartBytes < 0)
        {
            CPLError(CE_Failure, CPLE_NotSupported,
                    "Cannot parse %s line", osTableLink.c_str());
            return false;
        }
        CPLString osTPath = CPLGetPath(pszFilename);
        CleanString( osTableFilename );
        osTableFilename = CPLFormCIFilename( osTPath, osTableFilename, NULL );
    }
    else
    {
        osTableFilename = oKeywords.GetKeyword( osTableLink, "" );
        if (!osTableFilename.empty() && osTableFilename[0] >= '0' &&
            osTableFilename[0] <= '9')
        {
            nStartBytes = atoi(osTableFilename.c_str()) - 1;
            if( nStartBytes < 0)
            {
                CPLError(CE_Failure, CPLE_NotSupported,
                        "Cannot parse %s line", osTableFilename.c_str());
                return false;
            }
            if (strstr(osTableFilename.c_str(), "<BYTES>") == NULL)
            {
                if( nRecordSize > 0 && nStartBytes > INT_MAX / nRecordSize )
                {
                    CPLError(CE_Failure, CPLE_NotSupported,
                             "Too big StartBytes value");
                    return false;
                }
                nStartBytes *= nRecordSize;
            }
            osTableFilename = pszFilename;
        }
        else
        {
            CPLString osTPath = CPLGetPath(pszFilename);
            CleanString( osTableFilename );
            osTableFilename =
                CPLFormCIFilename( osTPath, osTableFilename, NULL );
            nStartBytes = 0;
        }
    }

    CPLString osTableName =
        oKeywords.GetKeyword( MakeAttr(osTableID, "NAME"), "" );
    if (osTableName.empty())
    {
        if (GetLayerByName(osTableID.c_str()) == NULL)
            osTableName = osTableID;
        else
            osTableName = CPLSPrintf("Layer_%d", nLayers+1);
    }
    CleanString(osTableName);
    CPLString osTableInterchangeFormat =
            oKeywords.GetKeyword( MakeAttr(osTableID, "INTERCHANGE_FORMAT"),
                                  "" );
    CPLString osTableRows =
        oKeywords.GetKeyword( MakeAttr(osTableID, "ROWS"), "" );
    const int nRecords = atoi(osTableRows);
    if (osTableInterchangeFormat.empty() ||
        osTableRows.empty() || nRecords < 0)
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "One of TABLE.INTERCHANGE_FORMAT or TABLE.ROWS is missing");
        return false;
    }

    CleanString(osTableInterchangeFormat);
    if (osTableInterchangeFormat.compare("ASCII") != 0 &&
        osTableInterchangeFormat.compare("BINARY") != 0)
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "Only INTERCHANGE_FORMAT=ASCII or BINARY is supported");
        return false;
    }

    VSILFILE* fp = VSIFOpenL(osTableFilename, "rb");
    if (fp == NULL)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Cannot open %s",
                 osTableFilename.c_str());
        return false;
    }

    CPLString osTableStructure =
        oKeywords.GetKeyword( MakeAttr(osTableID, "^STRUCTURE"), "" );
    if (!osTableStructure.empty())
    {
        CPLString osTPath = CPLGetPath(pszFilename);
        CleanString( osTableStructure );
        osTableStructure = CPLFormCIFilename( osTPath, osTableStructure, NULL );
    }

    GByte* pabyRecord = (GByte*) VSI_MALLOC_VERBOSE(nRecordSize + 1);
    if (pabyRecord == NULL)
    {
        VSIFCloseL(fp);
        return false;
    }
    pabyRecord[nRecordSize] = 0;

    papoLayers = static_cast<OGRLayer**>(
        CPLRealloc(papoLayers, (nLayers + 1) * sizeof(OGRLayer*)));
    papoLayers[nLayers] = new OGRPDSLayer(
        osTableID, osTableName, fp,
        pszFilename,
        osTableStructure,
        nRecords, nStartBytes,
        nRecordSize, pabyRecord,
        osTableInterchangeFormat.compare("ASCII") == 0);
    nLayers++;

    return true;
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

int OGRPDSDataSource::Open( const char * pszFilename )

{
    pszName = CPLStrdup( pszFilename );

// --------------------------------------------------------------------
//      Does this appear to be a .PDS table file?
// --------------------------------------------------------------------

    VSILFILE* fp = VSIFOpenL(pszFilename, "rb");
    if (fp == NULL)
        return FALSE;

    char szBuffer[512];
    int nbRead = static_cast<int>(
        VSIFReadL(szBuffer, 1, sizeof(szBuffer) - 1, fp));
    szBuffer[nbRead] = '\0';

    const char* pszPos = strstr(szBuffer, "PDS_VERSION_ID");
    const bool bIsPDS = pszPos != NULL;

    if (!bIsPDS)
    {
        VSIFCloseL(fp);
        return FALSE;
    }

    if (!oKeywords.Ingest(fp, static_cast<int>(pszPos - szBuffer)))
    {
        VSIFCloseL(fp);
        return FALSE;
    }

    VSIFCloseL(fp);
    CPLString osRecordType = oKeywords.GetKeyword( "RECORD_TYPE", "" );
    CPLString osFileRecords = oKeywords.GetKeyword( "FILE_RECORDS", "" );
    CPLString osRecordBytes = oKeywords.GetKeyword( "RECORD_BYTES", "" );
    int nRecordSize = atoi(osRecordBytes);
    if (osRecordType.empty() || osFileRecords.empty() ||
        osRecordBytes.empty() || nRecordSize <= 0)
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "One of RECORD_TYPE, FILE_RECORDS or RECORD_BYTES is missing");
        return FALSE;
    }
    CleanString(osRecordType);
    if (osRecordType.compare("FIXED_LENGTH") != 0)
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "Only RECORD_TYPE=FIXED_LENGTH is supported");
        return FALSE;
    }

    CPLString osTable = oKeywords.GetKeyword( "^TABLE", "" );
    if (!osTable.empty())
    {
        LoadTable(pszFilename, nRecordSize, "TABLE");
    }
    else
    {
        fp = VSIFOpenL(pszFilename, "rb");
        if (fp == NULL)
            return FALSE;

        while( true )
        {
            CPLPushErrorHandler(CPLQuietErrorHandler);
            const char* pszLine = CPLReadLine2L(fp, 256, NULL);
            CPLPopErrorHandler();
            CPLErrorReset();
            if (pszLine == NULL)
                break;
            char** papszTokens =
                CSLTokenizeString2( pszLine, " =", CSLT_HONOURSTRINGS );
            int nTokens = CSLCount(papszTokens);
            if (nTokens == 2 &&
                papszTokens[0][0] == '^' &&
                strstr(papszTokens[0], "TABLE") != NULL)
            {
                LoadTable(pszFilename, nRecordSize, papszTokens[0] + 1);
            }
            CSLDestroy(papszTokens);
            papszTokens = NULL;
        }
        VSIFCloseL(fp);
    }

    return nLayers != 0;
}
