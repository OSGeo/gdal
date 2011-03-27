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

#define MAX_FEATURES_FETCH  500

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

    bHasTriedCreateTable = FALSE;
    bInTransaction = FALSE;

    iLatitude = iLongitude = -1;
    iGeometryField = -1;
}

/************************************************************************/
/*                            ~OGRGFTLayer()                            */
/************************************************************************/

OGRGFTLayer::~OGRGFTLayer()

{
    CreateTableIfNecessary();

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
    CPLString osFID;
    if (nTokens == poFeatureDefn->GetFieldCount() + 1)
    {
        osFID = papszTokens[0];
        for(int i=0;i<poFeatureDefn->GetFieldCount();i++)
        {
            const char* pszVal = papszTokens[i+1];
            if (pszVal[0])
            {
                poFeature->SetField(i, pszVal);

                if (i == iGeometryField && i != iLatitude)
                {
                    if (pszVal[0] == '-' || (pszVal[0] >= '0' && pszVal[0] <= '9'))
                    {
                        char** papszLatLon = CSLTokenizeString2(pszVal, " ,", 0);
                        if (CSLCount(papszLatLon) == 2)
                        {
                            OGRPoint* poPoint = new OGRPoint(atof( papszLatLon[1]),
                                                            atof( papszLatLon[0]));
                            poPoint->assignSpatialReference(poSRS);
                            poFeature->SetGeometryDirectly(poPoint);
                        }
                        CSLDestroy(papszLatLon);
                    }
                    else
                    {
                        /* TODO : parse KML */
                    }
                }
            }
        }

        if (iLatitude >= 0 && iLongitude >= 0 &&
            papszTokens[iLatitude+1][0] && papszTokens[iLongitude+1][0])
        {
            OGRPoint* poPoint = new OGRPoint(atof( papszTokens[iLongitude+1] ),
                                             atof( papszTokens[iLatitude+1] ));
            poPoint->assignSpatialReference(poSRS);
            poFeature->SetGeometryDirectly(poPoint);
        }
    }
    else
    {
        CPLDebug("GFT", "Only %d columns for feature %d", nTokens, nNextFID);
    }
    CSLDestroy(papszTokens);

    /*int nFID = atoi(osFID);
    if (strcmp(CPLSPrintf("%d", nFID), osFID.c_str()) == 0)
        poFeature->SetFID(nFID);
    else*/
        poFeature->SetFID(nNextFID);

    nNextFID ++;

    return poFeature;
}

/************************************************************************/
/*                           TestCapability()                           */
/************************************************************************/

int OGRGFTLayer::TestCapability( const char * pszCap )

{
    if( EQUAL(pszCap,OLCRandomRead) )
        return TRUE;

    else if( EQUAL(pszCap,OLCSequentialWrite)
             /*|| EQUAL(pszCap,OLCRandomWrite)  */)
        return poDS->IsReadWrite();

    else if( EQUAL(pszCap,OLCCreateField) )
        return poDS->IsReadWrite();

    else if( EQUAL(pszCap, OLCTransactions) )
        return poDS->IsReadWrite();

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

    const CPLString& osAuth = poDS->GetAuth();
    if (osAuth.size())
    {
        char** papszOptions = CSLAddString(poDS->AddHTTPOptions(),
            CPLSPrintf("POSTFIELDS=sql=DESCRIBE %s", osTableId.c_str()));
        CPLHTTPResult* psResult = CPLHTTPFetch( poDS->GetAPIURL(), papszOptions);
        CSLDestroy(papszOptions);
        papszOptions = NULL;

        if (psResult == NULL)
            return FALSE;

        char* pszLine = (char*) psResult->pabyData;
        if (pszLine == NULL ||
            psResult->pszErrBuf != NULL ||
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
                //CPLDebug("GFT", "%s %s %s", papszTokens[0], papszTokens[1], papszTokens[2]);
                OGRFieldType eType = OFTString;
                if (EQUAL(papszTokens[2], "number"))
                    eType = OFTReal;
                else if (EQUAL(papszTokens[2], "datetime"))
                    eType = OFTDateTime;

                if (EQUAL(papszTokens[2], "location"))
                {
                    iGeometryField = poFeatureDefn->GetFieldCount();
                }
                OGRFieldDefn oFieldDefn(papszTokens[1], eType);
                poFeatureDefn->AddFieldDefn(&oFieldDefn);
            }
            CSLDestroy(papszTokens);

            pszLine = pszNextLine;
        }

        CPLHTTPDestroyResult(psResult);
    }
    else
    {
        /* http://code.google.com/intl/fr/apis/fusiontables/docs/developers_guide.html#Exploring states */
        /* that DESCRIBE should work on public tables without authentication, but it is not true... */
        CPLString osURL = CPLSPrintf("%s?sql=SELECT+*+FROM+%s+OFFSET+0+LIMIT+1&hdrs=true",
                                 poDS->GetAPIURL(), osTableId.c_str());
        char** papszOptions = poDS->AddHTTPOptions();
        CPLHTTPResult* psResult = CPLHTTPFetch( osURL, papszOptions );
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
        char* pszNextLine = OGRGFTLayerGotoNextLine(pszLine);
        if (pszNextLine)
            pszNextLine[-1] = 0;

        char** papszTokens = CSLTokenizeString2(pszLine, ",", 0);
        for(int i=0;papszTokens && papszTokens[i];i++)
        {
            OGRFieldDefn oFieldDefn(papszTokens[i], OFTString);
            poFeatureDefn->AddFieldDefn(&oFieldDefn);
        }
        CSLDestroy(papszTokens);

        CPLHTTPDestroyResult(psResult);
    }

    for(int i=0;i<poFeatureDefn->GetFieldCount();i++)
    {
        const char* pszName = poFeatureDefn->GetFieldDefn(i)->GetNameRef();
        if (EQUAL(pszName, "latitude") || EQUAL(pszName, "lat"))
            iLatitude = i;
        else if (EQUAL(pszName, "longitude") || EQUAL(pszName, "lon") || EQUAL(pszName, "long"))
            iLongitude = i;
    }

    if (iLatitude >= 0 && iLongitude >= 0)
    {
        iGeometryField = iLatitude;
        poFeatureDefn->SetGeomType( wkbPoint );
    }

    return TRUE;
}

/************************************************************************/
/*                           FetchNextRows()                            */
/************************************************************************/

int OGRGFTLayer::FetchNextRows()
{
    aosRows.resize(0);

    CPLString osGetColumns("SELECT ROWID");
    for(int i=0;i<poFeatureDefn->GetFieldCount();i++)
    {
        osGetColumns += ",'";
        osGetColumns += poFeatureDefn->GetFieldDefn(i)->GetNameRef();
        osGetColumns += "'";
    }
    osGetColumns += " FROM ";
    osGetColumns += osTableId;
    osGetColumns += " ";
    osGetColumns += osWHERE;
    osGetColumns += CPLSPrintf("OFFSET %d LIMIT %d", nOffset, MAX_FEATURES_FETCH);

    char** papszOptions = CSLAddString(poDS->AddHTTPOptions(),
            CPLSPrintf("POSTFIELDS=sql=%s", osGetColumns.c_str()));
    CPLPushErrorHandler(CPLQuietErrorHandler);
    CPLHTTPResult * psResult = CPLHTTPFetch( poDS->GetAPIURL(), papszOptions);
    CPLPopErrorHandler();
    CSLDestroy(papszOptions);
    papszOptions = NULL;

    if (psResult == NULL)
        return FALSE;

    char* pszLine = (char*) psResult->pabyData;
    if (pszLine == NULL || psResult->pszErrBuf != NULL)
    {
        CPLDebug("GFT", "Error : %s", pszLine ? pszLine : psResult->pszErrBuf);
        CPLHTTPDestroyResult(psResult);
        return FALSE;
    }

    pszLine = OGRGFTLayerGotoNextLine(pszLine);
    if (pszLine == NULL)
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
            if (*pszIter == '"')
            {
                if (pszIter[1] != '"')
                    nDoubleQuotes ++;
                else
                    pszIter ++;
            }
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
                    if (*pszIter == '"')
                    {
                        if (pszIter[1] != '"')
                            nDoubleQuotes ++;
                        else
                            pszIter ++;
                    }
                    pszIter ++;
                }

                if ((nDoubleQuotes % 2) == 0)
                {
                    break;
                }

                pszLine = pszNextLine;
            }

            aosRows.push_back(osLine);
        }

        pszLine = pszNextLine;
    }

    bEOF = aosRows.size() < MAX_FEATURES_FETCH;

    CPLHTTPDestroyResult(psResult);

    return TRUE;
}
/************************************************************************/
/*                            GetFeature()                              */
/************************************************************************/

OGRFeature * OGRGFTLayer::GetFeature( long nFID )
{
    GetLayerDefn();

    char** papszOptions = poDS->AddHTTPOptions();
    CPLString osURL = CPLSPrintf("%s?sql=SELECT+*+FROM+%s+OFFSET+%d+LIMIT+1&hdrs=false",
                                 poDS->GetAPIURL(), osTableId.c_str(), (int)nFID);

    CPLPushErrorHandler(CPLQuietErrorHandler);
    CPLHTTPResult * psResult = CPLHTTPFetch( osURL, papszOptions);
    CPLPopErrorHandler();
    CSLDestroy(papszOptions);
    papszOptions = NULL;

    if (psResult == NULL)
        return NULL;

    char* pszLine = (char*) psResult->pabyData;
    if (pszLine == NULL || psResult->pszErrBuf != NULL)
    {
        CPLHTTPDestroyResult(psResult);
        return NULL;
    }

    OGRFeature* poFeature = new OGRFeature(poFeatureDefn);

    char** papszTokens = OGRGFTCSVSplitLine(pszLine, ',');
    int nTokens = CSLCount(papszTokens);
    if (nTokens == poFeatureDefn->GetFieldCount())
    {
        for(int i=0;i<poFeatureDefn->GetFieldCount();i++)
        {
            if (papszTokens[i][0])
                poFeature->SetField(i, papszTokens[i]);
        }
    }
    else
    {
        CPLDebug("GFT", "Only %d columns for feature %d", nTokens, (int)nFID);
    }
    CSLDestroy(papszTokens);

    poFeature->SetFID(nFID);

    CPLHTTPDestroyResult(psResult);

    return poFeature;
}

/************************************************************************/
/*                           GetLayerDefn()                             */
/************************************************************************/

OGRFeatureDefn * OGRGFTLayer::GetLayerDefn()
{
    if (poFeatureDefn == NULL)
    {
        if (osTableId.size() == 0)
            return NULL;
        FetchDescribe();
    }

    return poFeatureDefn;
}

/************************************************************************/
/*                          GetFeatureCount()                           */
/************************************************************************/

int OGRGFTLayer::GetFeatureCount(int bForce)
{
    if (m_poFilterGeom != NULL || m_poAttrQuery != NULL)
        return OGRLayer::GetFeatureCount(bForce);

    /*if (nFeatureCount >= 0)
        return nFeatureCount;*/

    char** papszOptions = poDS->AddHTTPOptions();
    CPLString osURL = CPLSPrintf("%s?sql=SELECT+COUNT()+FROM+%s&hdrs=false",
                                 poDS->GetAPIURL(), osTableId.c_str());
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

/************************************************************************/
/*                            CreateField()                             */
/************************************************************************/

OGRErr OGRGFTLayer::CreateField( OGRFieldDefn *poField,
                                 int bApproxOK )
{

    if (!poDS->IsReadWrite())
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Operation not available in read-only mode");
        return OGRERR_FAILURE;
    }

    if (osTableId.size() != 0)
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "Cannot add field to already created table");
        return OGRERR_FAILURE;
    }

    if (poDS->GetAuth().size() == 0)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Operation not available in unauthenticated mode");
        return OGRERR_FAILURE;
    }

    if (poFeatureDefn == NULL)
    {
        poFeatureDefn = new OGRFeatureDefn( osTableName );
        poFeatureDefn->Reference();
        poFeatureDefn->SetGeomType( wkbUnknown );
    }

    poFeatureDefn->AddFieldDefn(poField);

    return OGRERR_NONE;
}

/************************************************************************/
/*                       CreateTableIfNecessary()                       */
/************************************************************************/

void OGRGFTLayer::CreateTableIfNecessary()
{
    if (bHasTriedCreateTable || osTableId.size() != 0)
        return;

    bHasTriedCreateTable = TRUE;

    CPLString osPOST("POSTFIELDS=sql=CREATE TABLE '");
    osPOST += osTableName;
    osPOST += "' (";

    int i;

    for(i=0;i<poFeatureDefn->GetFieldCount();i++)
    {
        const char* pszName = poFeatureDefn->GetFieldDefn(i)->GetNameRef();
        if (EQUAL(pszName, "latitude") || EQUAL(pszName, "lat"))
            iLatitude = i;
        else if (EQUAL(pszName, "longitude") || EQUAL(pszName, "lon") || EQUAL(pszName, "long"))
            iLongitude = i;
    }

    if (iLatitude >= 0 && iLongitude >= 0)
        iGeometryField = iLatitude;

    for(i=0;i<poFeatureDefn->GetFieldCount();i++)
    {
        if (i > 0)
            osPOST += ", ";
        osPOST += "'";
        osPOST += poFeatureDefn->GetFieldDefn(i)->GetNameRef();
        osPOST += "': ";

        if (iGeometryField == i)
        {
            osPOST += "LOCATION";
        }
        else
        {
            switch(poFeatureDefn->GetFieldDefn(i)->GetType())
            {
                case OFTInteger:
                case OFTReal:
                    osPOST += "NUMBER";
                    break;
                default:
                    osPOST += "STRING";
            }
        }
    }
    if (iGeometryField < 0)
    {
        if (i > 0)
            osPOST += ", ";
        osPOST += "geometry: LOCATION";

        iGeometryField = poFeatureDefn->GetFieldCount();
        OGRFieldDefn oFieldDefn("geometry", OFTString);
        poFeatureDefn->AddFieldDefn(&oFieldDefn);
    }
    osPOST += ")";

    char** papszOptions = CSLAddString(poDS->AddHTTPOptions(), osPOST);
    //CPLDebug("GFT", "%s",  osPOST.c_str());
    CPLHTTPResult* psResult = CPLHTTPFetch( poDS->GetAPIURL(), papszOptions);
    CSLDestroy(papszOptions);
    papszOptions = NULL;

    if (psResult == NULL)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Table creation failed");
        return;
    }

    char* pszLine = (char*) psResult->pabyData;
    if (pszLine == NULL ||
        strncmp(pszLine, "tableid", 7) != 0 ||
        psResult->pszErrBuf != NULL)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Table creation failed");
        CPLHTTPDestroyResult(psResult);
        return;
    }

    pszLine = OGRGFTLayerGotoNextLine(pszLine);
    if (pszLine == NULL)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Table creation failed");
        CPLHTTPDestroyResult(psResult);
        return;
    }

    char* pszNextLine = OGRGFTLayerGotoNextLine(pszLine);
    if (pszNextLine)
        pszNextLine[-1] = 0;

    osTableId = pszLine;
    CPLDebug("GFT", "Table %s --> id = %s", osTableName.c_str(), osTableId.c_str());

    CPLHTTPDestroyResult(psResult);
}

/************************************************************************/
/*                            EscapeQuote()                             */
/************************************************************************/

static CPLString EscapeQuote(const char* pszStr)
{
    CPLString osRes;
    while(*pszStr)
    {
        if (*pszStr == '\'')
            osRes += "\'\'";
        else
            osRes += *pszStr;
        pszStr ++;
    }
    return osRes;
}

/************************************************************************/
/*                           CreateFeature()                            */
/************************************************************************/

OGRErr OGRGFTLayer::CreateFeature( OGRFeature *poFeature )

{
    if (!poDS->IsReadWrite())
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Operation not available in read-only mode");
        return OGRERR_FAILURE;
    }

    if (osTableId.size() == 0)
    {
        CreateTableIfNecessary();
        if (osTableId.size() == 0)
        {
            CPLError(CE_Failure, CPLE_NotSupported,
                    "Cannot add field to non-created table");
            return OGRERR_FAILURE;
        }
    }

    if (poDS->GetAuth().size() == 0)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Operation not available in unauthenticated mode");
        return OGRERR_FAILURE;
    }

    CPLString      osCommand;

    osCommand += "INSERT INTO ";
    osCommand += osTableId;
    osCommand += " (";

    int iField;
    int nFieldCount = poFeatureDefn->GetFieldCount();
    for(iField = 0; iField < nFieldCount; iField++)
    {
        if (iField > 0)
            osCommand += ", ";
        osCommand += poFeatureDefn->GetFieldDefn(iField)->GetNameRef();
    }
    osCommand += ") VALUES (";
    for(iField = 0; iField < nFieldCount; iField++)
    {
        if (iField > 0)
            osCommand += ", ";

        if (iGeometryField != iLatitude && iField == iGeometryField)
        {
            OGRGeometry* poGeom = poFeature->GetGeometryRef();
            if (poGeom == NULL)
                osCommand += "''";
            else
            {
                char* pszKML;
                if (poGeom->getSpatialReference() != NULL &&
                    !poGeom->getSpatialReference()->IsSame(poSRS))
                {
                    OGRGeometry* poGeom4326 = poGeom->clone();
                    poGeom4326->transformTo(poSRS);
                    pszKML = poGeom4326->exportToKML();
                    delete poGeom4326;
                }
                else
                {
                    pszKML = poGeom->exportToKML();
                }
                osCommand += "'";
                osCommand += pszKML;
                osCommand += "'";
                CPLFree(pszKML);
            }
            continue;
        }

        if( !poFeature->IsFieldSet( iField ) )
        {
            osCommand += "''";
        }
        else
        {
            osCommand += "'";
            const char* pszVal = poFeature->GetFieldAsString(iField);
            if (strchr(pszVal, '\''))
                osCommand += EscapeQuote(pszVal);
            else
                osCommand += pszVal;
            osCommand += "'";
        }
    }

    osCommand += ")";

    CPLDebug("GFT", "%s",  osCommand.c_str());

    if (bInTransaction)
    {
        if (osTransaction.size())
            osTransaction += "; ";
        osTransaction += osCommand;
        return OGRERR_NONE;
    }

    CPLString osPOST("POSTFIELDS=sql=");
    osPOST += osCommand;
    char** papszOptions = CSLAddString(poDS->AddHTTPOptions(), osPOST);
    CPLHTTPResult* psResult = CPLHTTPFetch( poDS->GetAPIURL(), papszOptions);
    CSLDestroy(papszOptions);
    papszOptions = NULL;

    if (psResult == NULL)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Feature creation failed");
        return OGRERR_FAILURE;
    }

    char* pszLine = (char*) psResult->pabyData;
    if (pszLine == NULL ||
        strncmp(pszLine, "rowid", 5) != 0 ||
        psResult->pszErrBuf != NULL)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Feature creation failed");
        CPLHTTPDestroyResult(psResult);
        return OGRERR_FAILURE;
    }

    pszLine = OGRGFTLayerGotoNextLine(pszLine);
    if (pszLine == NULL)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Feature creation failed");
        CPLHTTPDestroyResult(psResult);
        return OGRERR_FAILURE;
    }

    char* pszNextLine = OGRGFTLayerGotoNextLine(pszLine);
    if (pszNextLine)
        pszNextLine[-1] = 0;

    CPLDebug("GFT", "Feature id = %s",  pszLine);

    CPLHTTPDestroyResult(psResult);

    return OGRERR_NONE;
}

/************************************************************************/
/*                         StartTransaction()                           */
/************************************************************************/

OGRErr OGRGFTLayer::StartTransaction()
{
    if (bInTransaction)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Already in transaction");
        return OGRERR_FAILURE;
    }

    if (!poDS->IsReadWrite())
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Operation not available in read-only mode");
        return OGRERR_FAILURE;
    }

    if (osTableId.size() == 0)
    {
        CreateTableIfNecessary();
        if (osTableId.size() == 0)
        {
            CPLError(CE_Failure, CPLE_NotSupported,
                    "Cannot add field to non-created table");
            return OGRERR_FAILURE;
        }
    }

    if (poDS->GetAuth().size() == 0)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Operation not available in unauthenticated mode");
        return OGRERR_FAILURE;
    }

    bInTransaction = TRUE;

    return OGRERR_NONE;
}

/************************************************************************/
/*                         CommitTransaction()                          */
/************************************************************************/

OGRErr OGRGFTLayer::CommitTransaction()
{
    if (!bInTransaction)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Should be in transaction");
        return OGRERR_FAILURE;
    }

    bInTransaction = FALSE;

    if (osTransaction)
    {
        CPLString osPOST("POSTFIELDS=sql=");
        osPOST += osTransaction;
        char** papszOptions = CSLAddString(poDS->AddHTTPOptions(), osPOST);
        CPLHTTPResult* psResult = CPLHTTPFetch( poDS->GetAPIURL(), papszOptions);
        CSLDestroy(papszOptions);
        papszOptions = NULL;

        osTransaction.resize(0);

        if (psResult == NULL)
        {
            CPLError(CE_Failure, CPLE_AppDefined, "Feature creation failed");
            return OGRERR_FAILURE;
        }

        char* pszLine = (char*) psResult->pabyData;
        if (pszLine == NULL ||
            strncmp(pszLine, "rowid", 5) != 0 ||
            psResult->pszErrBuf != NULL)
        {
            CPLError(CE_Failure, CPLE_AppDefined, "Feature creation failed");
            CPLHTTPDestroyResult(psResult);
            return OGRERR_FAILURE;
        }

        pszLine = OGRGFTLayerGotoNextLine(pszLine);
        while(pszLine && *pszLine != 0)
        {
            char* pszNextLine = OGRGFTLayerGotoNextLine(pszLine);
            if (pszNextLine)
                pszNextLine[-1] = 0;
            CPLDebug("GFT", "Feature id = %s",  pszLine);

            pszLine = pszNextLine;
        }

        CPLHTTPDestroyResult(psResult);
    }

    return OGRERR_NONE;
}

/************************************************************************/
/*                        RollbackTransaction()                         */
/************************************************************************/

OGRErr OGRGFTLayer::RollbackTransaction()
{
    if (!bInTransaction)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Should be in transaction");
        return OGRERR_FAILURE;
    }
    bInTransaction = FALSE;
    return OGRERR_NONE;
}

/************************************************************************/
/*                         SetAttributeFilter()                         */
/************************************************************************/

OGRErr OGRGFTLayer::SetAttributeFilter( const char *pszQuery )

{
    if( pszQuery == NULL )
        osQuery = "";
    else
        osQuery = pszQuery;

    BuildWhere();

    ResetReading();

    return OGRERR_NONE;
}


/************************************************************************/
/*                          SetSpatialFilter()                          */
/************************************************************************/

void OGRGFTLayer::SetSpatialFilter( OGRGeometry * poGeomIn )

{
    if( InstallFilter( poGeomIn ) )
    {
        BuildWhere();

        ResetReading();
    }
}

/************************************************************************/
/*                             BuildWhere()                             */
/*                                                                      */
/*      Build the WHERE statement appropriate to the current set of     */
/*      criteria (spatial and attribute queries).                       */
/************************************************************************/

void OGRGFTLayer::BuildWhere()

{
    osWHERE = "";

    if( m_poFilterGeom != NULL && iGeometryField >= 0)
    {
        OGREnvelope  sEnvelope;

        m_poFilterGeom->getEnvelope( &sEnvelope );
        osWHERE.Printf("WHERE ST_INTERSECTS('%s', RECTANGLE(LATLNG(%.12f, %.12f), LATLNG(%.12f, %.12f))) ",
                       poFeatureDefn->GetFieldDefn(iGeometryField)->GetNameRef(),
                       sEnvelope.MinY - 1e-11, sEnvelope.MinX - 1e-11,
                       sEnvelope.MaxY + 1e-11, sEnvelope.MaxX + 1e-11);
    }

    if( strlen(osQuery) > 0 )
    {
        if( strlen(osWHERE) == 0 )
        {
            osWHERE.Printf( "WHERE %s ", osQuery.c_str()  );
        }
        else
        {
            osWHERE += "AND ";
            osWHERE += osQuery;
        }
    }
}
