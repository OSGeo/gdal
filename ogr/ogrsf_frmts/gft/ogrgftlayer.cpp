/******************************************************************************
 * $Id$
 *
 * Project:  GFT Translator
 * Purpose:  Implements OGRGFTLayer class.
 * Author:   Even Rouault, <even dot rouault at mines dash paris dot org>
 *
 ******************************************************************************
 * Copyright (c) 2011, Even Rouault <even dot rouault at mines dash paris dot org>
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

OGRGFTLayer::OGRGFTLayer(OGRGFTDataSource* poDS)

{
    this->poDS = poDS;

    nNextInSeq = 0;

    poSRS = new OGRSpatialReference(SRS_WKT_WGS84);

    poFeatureDefn = NULL;

    nOffset = 0;
    bEOF = FALSE;

    iLatitudeField = iLongitudeField = -1;
    iGeometryField = -1;

    bFirstTokenIsFID = FALSE;
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
    nNextInSeq = 0;
    nOffset = 0;
    bEOF = FALSE;
}

/************************************************************************/
/*                           GetLayerDefn()                             */
/************************************************************************/

OGRFeatureDefn * OGRGFTLayer::GetLayerDefn()
{
    CPLAssert(poFeatureDefn);
    return poFeatureDefn;
}

/************************************************************************/
/*                           GetNextFeature()                           */
/************************************************************************/

OGRFeature *OGRGFTLayer::GetNextFeature()
{
    OGRFeature  *poFeature;

    GetLayerDefn();

    if (nNextInSeq >= nOffset + (int)aosRows.size())
    {
        if (bEOF)
            return NULL;

        nOffset += aosRows.size();
        if (!FetchNextRows())
            return NULL;
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
/*                         BuildFeatureFromSQL()                        */
/************************************************************************/

OGRFeature *OGRGFTLayer::BuildFeatureFromSQL(const char* pszLine)
{
    OGRFeature* poFeature = new OGRFeature(poFeatureDefn);

    char** papszTokens = OGRGFTCSVSplitLine(pszLine, ',');
    int nTokens = CSLCount(papszTokens);
    CPLString osFID;

    int nAttrOffset = 0;
    int iROWID = -1;
    if (bFirstTokenIsFID)
    {
        osFID = papszTokens[0];
        nAttrOffset = 1;
    }
    else
    {
        iROWID = poFeatureDefn->GetFieldIndex("rowid");
        if (iROWID < 0)
            iROWID = poFeatureDefn->GetFieldIndex("ROWID");
    }

    if (nTokens == poFeatureDefn->GetFieldCount() + nAttrOffset)
    {
        for(int i=0;i<poFeatureDefn->GetFieldCount();i++)
        {
            const char* pszVal = papszTokens[i+nAttrOffset];
            if (pszVal[0])
            {
                poFeature->SetField(i, pszVal);

                if (i == iGeometryField && i != iLatitudeField)
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
                else if (i == iROWID)
                {
                    osFID = pszVal;
                }
            }
        }

        if (iLatitudeField >= 0 && iLongitudeField >= 0 &&
            papszTokens[iLatitudeField+nAttrOffset][0] &&
            papszTokens[iLongitudeField+nAttrOffset][0])
        {
            OGRPoint* poPoint = new OGRPoint(atof( papszTokens[iLongitudeField+nAttrOffset] ),
                                             atof( papszTokens[iLatitudeField+nAttrOffset] ));
            poPoint->assignSpatialReference(poSRS);
            poFeature->SetGeometryDirectly(poPoint);
        }
    }
    else
    {
        CPLDebug("GFT", "Only %d columns for feature %s", nTokens, osFID.c_str());
    }
    CSLDestroy(papszTokens);

    int nFID = atoi(osFID);
    if (strcmp(CPLSPrintf("%d", nFID), osFID.c_str()) == 0)
        poFeature->SetFID(nFID);
    else
        poFeature->SetFID(nNextInSeq);

    return poFeature;
}

/************************************************************************/
/*                         GetNextRawFeature()                          */
/************************************************************************/

OGRFeature *OGRGFTLayer::GetNextRawFeature()
{
    if (nNextInSeq - nOffset >= (int)aosRows.size())
        return NULL;

    OGRFeature* poFeature = BuildFeatureFromSQL(aosRows[nNextInSeq - nOffset]);

    nNextInSeq ++;

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
/*                         GetGeometryColumn()                          */
/************************************************************************/

const char *OGRGFTLayer::GetGeometryColumn()
{
    GetLayerDefn();
    if (iGeometryField < 0)
        return "";

    return poFeatureDefn->GetFieldDefn(iGeometryField)->GetNameRef();
}

/************************************************************************/
/*                         ParseCSVResponse()                           */
/************************************************************************/

int OGRGFTLayer::ParseCSVResponse(char* pszLine,
                                  std::vector<CPLString>& aosRes)
{
    while(pszLine != NULL && *pszLine != 0)
    {
        char* pszNextLine = OGRGFTGotoNextLine(pszLine);
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
            aosRes.push_back(pszLine);
        else
        {
            CPLString osLine(pszLine);

            pszLine = pszNextLine;
            while(pszLine != NULL && *pszLine != 0)
            {
                pszNextLine = OGRGFTGotoNextLine(pszLine);
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

            aosRes.push_back(osLine);
        }

        pszLine = pszNextLine;
    }

    return TRUE;
}

/************************************************************************/
/*                              PatchSQL()                              */
/************************************************************************/

CPLString OGRGFTLayer::PatchSQL(const char* pszSQL)
{
    CPLString osSQL;

    while(*pszSQL)
    {
        if (EQUALN(pszSQL, "COUNT(", 5) && strchr(pszSQL, ')'))
        {
            const char* pszNext = strchr(pszSQL, ')');
            osSQL += "COUNT()";
            pszSQL = pszNext + 1;
        }
        else if ((*pszSQL == '<' && pszSQL[1] == '>') ||
                 (*pszSQL == '!' && pszSQL[1] == '='))
        {
            osSQL += " NOT EQUAL TO ";
            pszSQL += 2;
        }
        else
        {
            osSQL += *pszSQL;
            pszSQL ++;
        }
    }
    return osSQL;
}
