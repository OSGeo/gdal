/******************************************************************************
 * $Id$
 *
 * Project:  PDS Translator
 * Purpose:  Implements OGRPDSDataSource class
 * Author:   Even Rouault, even dot rouault at mines dash paris dot org
 *
 ******************************************************************************
 * Copyright (c) 2010, Even Rouault <even dot rouault at mines dash paris dot org>
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

#include "ogr_pds.h"
#include "cpl_conv.h"
#include "cpl_string.h"

CPL_CVSID("$Id$");

/************************************************************************/
/*                           OGRPDSDataSource()                         */
/************************************************************************/

OGRPDSDataSource::OGRPDSDataSource()

{
    papoLayers = NULL;
    nLayers = 0;

    pszName = NULL;
}

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

int OGRPDSDataSource::TestCapability( const char * pszCap )

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
    else
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
    else
    {
        CSLDestroy( papszTokens );
        return pszDefault;
    }
}

/************************************************************************/
/*                            CleanString()                             */
/*                                                                      */
/* Removes single or double quotes, and converts spaces to underscores. */
/* The change is made in-place to CPLString.                            */
/************************************************************************/

void OGRPDSDataSource::CleanString( CPLString &osInput )

{
   if(  ( osInput.size() < 2 ) ||
        ((osInput.at(0) != '"'   || osInput.at(osInput.size()-1) != '"' ) &&
        ( osInput.at(0) != '\'' || osInput.at(osInput.size()-1) != '\'')) )
        return;

    char *pszWrk = CPLStrdup(osInput.c_str() + 1);
    int i;

    pszWrk[strlen(pszWrk)-1] = '\0';
    
    for( i = 0; pszWrk[i] != '\0'; i++ )
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

int OGRPDSDataSource::LoadTable(const char* pszFilename,
                                     int nRecordSize,
                                     CPLString osTableID )
{

    CPLString osTableFilename;
    int nStartBytes;
    
    CPLString osTableLink = "^";
    osTableLink += osTableID;
    
    CPLString osTable = oKeywords.GetKeyword( osTableLink, "" );
    if( osTable[0] == '(' )
    {
        osTableFilename = GetKeywordSub(osTableLink, 1, "");
        CPLString osStartRecord = GetKeywordSub(osTableLink, 2, "");
        nStartBytes = (atoi(osStartRecord.c_str()) - 1) * nRecordSize;
        if (osTableFilename.size() == 0 || osStartRecord.size() == 0 ||
            nStartBytes < 0)
        {
            CPLError(CE_Failure, CPLE_NotSupported,
                    "Cannot parse %s line", osTableLink.c_str());
            return FALSE;
        }
        CPLString osTPath = CPLGetPath(pszFilename);
        CleanString( osTableFilename );
        osTableFilename = CPLFormCIFilename( osTPath, osTableFilename, NULL );
    }
    else
    {
        osTableFilename = oKeywords.GetKeyword( osTableLink, "" );
        if (osTableFilename.size() != 0 && osTableFilename[0] >= '0' &&
            osTableFilename[0] <= '9')
        {
            nStartBytes = atoi(osTableFilename.c_str()) - 1;
            if (strstr(osTableFilename.c_str(), "<BYTES>") == NULL)
                nStartBytes *= nRecordSize;
            osTableFilename = pszFilename;
        }
        else
        {
            CPLString osTPath = CPLGetPath(pszFilename);
            CleanString( osTableFilename );
            osTableFilename = CPLFormCIFilename( osTPath, osTableFilename, NULL );
            nStartBytes = 0;
        }
    }

    CPLString osTableName = oKeywords.GetKeyword( MakeAttr(osTableID, "NAME"), "" );
    if (osTableName.size() == 0)
    {
        if (GetLayerByName(osTableID.c_str()) == NULL)
            osTableName = osTableID;
        else
            osTableName = CPLSPrintf("Layer_%d", nLayers+1);
    }
    CleanString(osTableName);
    CPLString osTableInterchangeFormat =
            oKeywords.GetKeyword( MakeAttr(osTableID, "INTERCHANGE_FORMAT"), "" );
    CPLString osTableRows = oKeywords.GetKeyword( MakeAttr(osTableID, "ROWS"), "" );
    int nRecords = atoi(osTableRows);
    if (osTableInterchangeFormat.size() == 0 ||
        osTableRows.size() == 0 || nRecords < 0)
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "One of TABLE.INTERCHANGE_FORMAT or TABLE.ROWS is missing");
        return FALSE;
    }
    
    CleanString(osTableInterchangeFormat);
    if (osTableInterchangeFormat.compare("ASCII") != 0 &&
        osTableInterchangeFormat.compare("BINARY") != 0)
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "Only INTERCHANGE_FORMAT=ASCII or BINARY is supported");
        return FALSE;
    }

    FILE* fp = VSIFOpenL(osTableFilename, "rb");
    if (fp == NULL)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Cannot open %s",
                 osTableFilename.c_str());
        return FALSE;
    }

    CPLString osTableStructure = oKeywords.GetKeyword( MakeAttr(osTableID, "^STRUCTURE"), "" );
    if (osTableStructure.size() != 0)
    {
        CPLString osTPath = CPLGetPath(pszFilename);
        CleanString( osTableStructure );
        osTableStructure = CPLFormCIFilename( osTPath, osTableStructure, NULL );
    }

    GByte* pabyRecord = (GByte*) VSIMalloc(nRecordSize + 1);
    if (pabyRecord == NULL)
    {
        VSIFCloseL(fp);
        return FALSE;
    }
    pabyRecord[nRecordSize] = 0;

    papoLayers = (OGRLayer**) CPLRealloc(papoLayers, (nLayers + 1) * sizeof(OGRLayer*));
    papoLayers[nLayers] = new OGRPDSLayer(osTableID, osTableName, fp,
                                         pszFilename,
                                         osTableStructure,
                                         nRecords, nStartBytes,
                                         nRecordSize, pabyRecord,
                                         osTableInterchangeFormat.compare("ASCII") == 0);
    nLayers++;

    return TRUE;
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

int OGRPDSDataSource::Open( const char * pszFilename, int bUpdateIn)

{
    if (bUpdateIn)
    {
        return FALSE;
    }

    pszName = CPLStrdup( pszFilename );

/* -------------------------------------------------------------------- */
/*      Determine what sort of object this is.                          */
/* -------------------------------------------------------------------- */
    VSIStatBufL sStatBuf;

    if( VSIStatL( pszFilename, &sStatBuf ) != 0 &&
        VSI_ISREG(sStatBuf.st_mode) )
        return FALSE;
    
// -------------------------------------------------------------------- 
//      Does this appear to be a .PDS table file?
// --------------------------------------------------------------------

    FILE* fp = VSIFOpenL(pszFilename, "rb");
    if (fp == NULL)
        return FALSE;

    char szBuffer[512];
    int nbRead = (int)VSIFReadL(szBuffer, 1, sizeof(szBuffer) - 1, fp);
    szBuffer[nbRead] = '\0';

    const char* pszPos = strstr(szBuffer, "PDS_VERSION_ID");
    int bIsPDS = (pszPos != NULL);

    if (!bIsPDS)
    {
        VSIFCloseL(fp);
        return FALSE;
    }

    if (!oKeywords.Ingest(fp, pszPos - szBuffer))
    {
        VSIFCloseL(fp);
        return FALSE;
    }

    VSIFCloseL(fp);
    CPLString osRecordType = oKeywords.GetKeyword( "RECORD_TYPE", "" );
    CPLString osFileRecords = oKeywords.GetKeyword( "FILE_RECORDS", "" );
    CPLString osRecordBytes = oKeywords.GetKeyword( "RECORD_BYTES", "" );
    int nRecordSize = atoi(osRecordBytes);
    if (osRecordType.size() == 0 || osFileRecords.size() == 0 ||
        osRecordBytes.size() == 0 || nRecordSize <= 0)
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
    if (osTable.size() != 0)
        LoadTable(pszFilename, nRecordSize, "TABLE");
    else
    {
        FILE* fp = VSIFOpenL(pszFilename, "rb");
        if (fp == NULL)
            return FALSE;

        while(TRUE)
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
