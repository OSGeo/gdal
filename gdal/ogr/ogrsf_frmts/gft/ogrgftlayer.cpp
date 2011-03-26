/******************************************************************************
 * $Id$
 *
 * Project:  GFT Translator
 * Purpose:  Implements OGRGFTLayer class.
 * Author:   Even Rouault, <even dot rouault at mines dash paris dot org>
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

#include "ogr_gft.h"
#include "cpl_conv.h"
#include "cpl_string.h"
#include "ogr_p.h"
#include "ogr_srs_api.h"
#include "cpl_http.h"

CPL_CVSID("$Id$");

/************************************************************************/
/*                            OGRGFTLayer()                             */
/************************************************************************/

OGRGFTLayer::OGRGFTLayer(OGRGFTDataSource* poDS,
                         const char* pszTableName,
                         const char* pszTableId)

{
    this->poDS = poDS;
    osTableName = pszTableName;
    osTableId = pszTableId;

    nNextFID = 0;

    poSRS = new OGRSpatialReference(SRS_WKT_WGS84);

    poFeatureDefn = NULL;

    nFeatureCount = -1;
    nOffset = 0;
    bEOF = FALSE;
}

/************************************************************************/
/*                            ~OGRGFTLayer()                            */
/************************************************************************/

OGRGFTLayer::~OGRGFTLayer()

{
    if( poSRS != NULL )
        poSRS->Release();

    if( poFeatureDefn != NULL )
        poFeatureDefn->Release();
}


/************************************************************************/
/*                            ResetReading()                            */
/************************************************************************/

void OGRGFTLayer::ResetReading()

{
    nNextFID = 0;
    nOffset = 0;
    bEOF = FALSE;
    aosRows.resize(0);
}


/************************************************************************/
/*                           GetNextFeature()                           */
/************************************************************************/

OGRFeature *OGRGFTLayer::GetNextFeature()
{
    OGRFeature  *poFeature;

    GetLayerDefn();

    if (nNextFID >= nOffset + (int)aosRows.size())
    {
        if (bEOF)
            return NULL;

        nOffset += aosRows.size();
        FetchNextRows();
        if (aosRows.size() == 0)
            bEOF = TRUE;
    }

    while(TRUE)
    {
        poFeature = GetNextRawFeature();
        if (poFeature == NULL)
            return NULL;

        if((m_poFilterGeom == NULL
            || FilterGeometry( poFeature->GetGeometryRef() ) )
        && (m_poAttrQuery == NULL
            || m_poAttrQuery->Evaluate( poFeature )) )
        {
            return poFeature;
        }
        else
            delete poFeature;
    }
}

/************************************************************************/
/*                            CSVSplitLine()                            */
/*                                                                      */
/*      Tokenize a CSV line into fields in the form of a string         */
/*      list.  This is used instead of the CPLTokenizeString()          */
/*      because it provides correct CSV escaping and quoting            */
/*      semantics.                                                      */
/************************************************************************/

static char **OGRGFTCSVSplitLine( const char *pszString, char chDelimiter )

{
    char        **papszRetList = NULL;
    char        *pszToken;
    int         nTokenMax, nTokenLen;

    pszToken = (char *) CPLCalloc(10,1);
    nTokenMax = 10;

    while( pszString != NULL && *pszString != '\0' )
    {
        int     bInString = FALSE;

        nTokenLen = 0;

        /* Try to find the next delimeter, marking end of token */
        for( ; *pszString != '\0'; pszString++ )
        {

            /* End if this is a delimeter skip it and break. */
            if( !bInString && *pszString == chDelimiter )
            {
                pszString++;
                break;
            }

            if( *pszString == '"' )
            {
                if( !bInString || pszString[1] != '"' )
                {
                    bInString = !bInString;
                    continue;
                }
                else  /* doubled quotes in string resolve to one quote */
                {
                    pszString++;
                }
            }

            if( nTokenLen >= nTokenMax-2 )
            {
                nTokenMax = nTokenMax * 2 + 10;
                pszToken = (char *) CPLRealloc( pszToken, nTokenMax );
            }

            pszToken[nTokenLen] = *pszString;
            nTokenLen++;
        }

        pszToken[nTokenLen] = '\0';
        papszRetList = CSLAddString( papszRetList, pszToken );

        /* If the last token is an empty token, then we have to catch
         * it now, otherwise we won't reenter the loop and it will be lost.
         */
        if ( *pszString == '\0' && *(pszString-1) == chDelimiter )
        {
            papszRetList = CSLAddString( papszRetList, "" );
        }
    }

    if( papszRetList == NULL )
        papszRetList = (char **) CPLCalloc(sizeof(char *),1);

    CPLFree( pszToken );

    return papszRetList;
}

/************************************************************************/
/*                         GetNextRawFeature()                          */
/************************************************************************/

OGRFeature *OGRGFTLayer::GetNextRawFeature()
{
    if (nNextFID - nOffset >= (int)aosRows.size())
        return NULL;

    OGRFeature* poFeature = new OGRFeature(poFeatureDefn);

    char** papszTokens = OGRGFTCSVSplitLine(aosRows[nNextFID - nOffset], ',');
    int nTokens = CSLCount(papszTokens);
    if (nTokens == poFeatureDefn->GetFieldCount())
    {
        for(int i=0;i<poFeatureDefn->GetFieldCount();i++)
        {
            poFeature->SetField(i, papszTokens[i]);
        }
    }
    else
    {
        CPLDebug("GFT", "Only %d columns for feature %d", nTokens, nNextFID);
    }
    CSLDestroy(papszTokens);

    poFeature->SetFID(nNextFID ++);

    return poFeature;
}

/************************************************************************/
/*                           TestCapability()                           */
/************************************************************************/

int OGRGFTLayer::TestCapability( const char * pszCap )

{
    return FALSE;
}

/************************************************************************/
/*                         GotoNextLine()                               */
/************************************************************************/

static char* OGRGFTLayerGotoNextLine(char* pszData)
{
    char* pszNextLine = strchr(pszData, '\n');
    if (pszNextLine)
        return pszNextLine + 1;
    return NULL;
}

/************************************************************************/
/*                           FetchDescribe()                            */
/************************************************************************/

int OGRGFTLayer::FetchDescribe()
{
    poFeatureDefn = new OGRFeatureDefn( osTableName );
    poFeatureDefn->Reference();
    poFeatureDefn->SetGeomType( wkbPoint );

    char** papszOptions = NULL;
    const CPLString& osAuth = poDS->GetAuth();
    papszOptions = CSLAddString(papszOptions, CPLSPrintf("HEADERS=Authorization: GoogleLogin auth=%s", osAuth.c_str()));
    papszOptions = CSLAddString(papszOptions, CPLSPrintf("POSTFIELDS=sql=DESCRIBE %s", osTableId.c_str()));
    CPLHTTPResult * psResult = CPLHTTPFetch( "https://www.google.com/fusiontables/api/query", papszOptions);
    CSLDestroy(papszOptions);
    papszOptions = NULL;

    if (psResult == NULL)
        return FALSE;

    char* pszLine = (char*) psResult->pabyData;
    if (pszLine == NULL ||
        strncmp(pszLine, "column id,name,type", strlen("column id,name,type")) != 0)
    {
        CPLHTTPDestroyResult(psResult);
        return FALSE;
    }

    pszLine = OGRGFTLayerGotoNextLine(pszLine);
    while(pszLine != NULL && *pszLine != 0)
    {
        char* pszNextLine = OGRGFTLayerGotoNextLine(pszLine);
        if (pszNextLine)
            pszNextLine[-1] = 0;

        char** papszTokens = CSLTokenizeString2(pszLine, ",", 0);
        if (CSLCount(papszTokens) == 3)
        {
            //printf("%s %s %s\n", papszTokens[0], papszTokens[1], papszTokens[2]);
            OGRFieldType eType = OFTString;
            if (EQUAL(papszTokens[2], "number") || EQUAL(papszTokens[2], "location"))
                eType = OFTReal;
            else if (EQUAL(papszTokens[2], "datetime"))
                eType = OFTDateTime;
            OGRFieldDefn oFieldDefn(papszTokens[1], eType);
            poFeatureDefn->AddFieldDefn(&oFieldDefn);
        }
        CSLDestroy(papszTokens);

        pszLine = pszNextLine;
    }

    CPLHTTPDestroyResult(psResult);

    return TRUE;
}

/************************************************************************/
/*                           FetchNextRows()                            */
/************************************************************************/

int OGRGFTLayer::FetchNextRows()
{
    aosRows.resize(0);

    char** papszOptions = NULL;
    const CPLString& osAuth = poDS->GetAuth();
    papszOptions = CSLAddString(papszOptions, CPLSPrintf("HEADERS=Authorization: GoogleLogin auth=%s", osAuth.c_str()));

    CPLString osURL = CPLSPrintf("https://www.google.com/fusiontables/api/query?sql=SELECT+*+FROM+%s+OFFSET+%d+LIMIT+500&hdrs=false",
                                 osTableId.c_str(), nOffset);

    CPLPushErrorHandler(CPLQuietErrorHandler);
    CPLHTTPResult * psResult = CPLHTTPFetch( osURL, papszOptions);
    CPLPopErrorHandler();
    CSLDestroy(papszOptions);
    papszOptions = NULL;

    if (psResult == NULL)
        return FALSE;

    char* pszLine = (char*) psResult->pabyData;
    if (pszLine == NULL || psResult->pszErrBuf != NULL)
    {
        CPLHTTPDestroyResult(psResult);
        return FALSE;
    }

    while(pszLine != NULL && *pszLine != 0)
    {
        char* pszNextLine = OGRGFTLayerGotoNextLine(pszLine);
        if (pszNextLine)
            pszNextLine[-1] = 0;

        int nDoubleQuotes = 0;
        char* pszIter = pszLine;
        while(*pszIter)
        {
            if (*pszIter == '"' && pszIter[1] != '"')
                nDoubleQuotes ++;
            pszIter ++;
        }

        if ((nDoubleQuotes % 2) == 0)
            aosRows.push_back(pszLine);
        else
        {
            CPLString osLine(pszLine);

            pszLine = pszNextLine;
            while(pszLine != NULL && *pszLine != 0)
            {
                pszNextLine = OGRGFTLayerGotoNextLine(pszLine);
                if (pszNextLine)
                    pszNextLine[-1] = 0;

                osLine += "\n";
                osLine += pszLine;

                pszIter = pszLine;
                while(*pszIter)
                {
                    if (*pszIter == '"' && pszIter[1] != '"')
                        nDoubleQuotes ++;
                    pszIter ++;
                }

                if ((nDoubleQuotes % 2) == 0)
                {
                    aosRows.push_back(osLine);
                    break;
                }

                pszLine = pszNextLine;
            }

        }

        pszLine = pszNextLine;
    }

    CPLHTTPDestroyResult(psResult);

    return TRUE;
}

/************************************************************************/
/*                           GetLayerDefn()                             */
/************************************************************************/

OGRFeatureDefn * OGRGFTLayer::GetLayerDefn()
{
    if (poFeatureDefn == NULL)
        FetchDescribe();

    return poFeatureDefn;
}

/************************************************************************/
/*                          GetFeatureCount()                           */
/************************************************************************/

int OGRGFTLayer::GetFeatureCount(int bForce)
{
    if (m_poFilterGeom != NULL || m_poAttrQuery != NULL)
        return OGRLayer::GetFeatureCount(bForce);

    if (nFeatureCount >= 0)
        return nFeatureCount;

    char** papszOptions = NULL;
    const CPLString& osAuth = poDS->GetAuth();
    papszOptions = CSLAddString(papszOptions, CPLSPrintf("HEADERS=Authorization: GoogleLogin auth=%s", osAuth.c_str()));

    CPLString osURL = CPLSPrintf("https://www.google.com/fusiontables/api/query?sql=SELECT+COUNT()+FROM+%s&hdrs=false", osTableId.c_str());

    CPLHTTPResult * psResult = CPLHTTPFetch( osURL, papszOptions);
    CSLDestroy(papszOptions);
    papszOptions = NULL;

    if (psResult == NULL)
        return 0;

    char* pszLine = (char*) psResult->pabyData;
    if (pszLine == NULL || psResult->pszErrBuf != NULL)
    {
        CPLHTTPDestroyResult(psResult);
        return 0;
    }

    nFeatureCount = atoi(pszLine);

    CPLHTTPDestroyResult(psResult);

    return nFeatureCount;
}
