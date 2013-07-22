/******************************************************************************
 * $Id$
 *
 * Project:  Google Maps Engine API Driver
 * Purpose:  OGRGMELayer Implementation.
 * Author:   Frank Warmerdam <warmerdam@pobox.com>
 *           (derived from GFT driver by Even)
 *
 ******************************************************************************
 * Copyright (c) 2013, Frank Warmerdam <warmerdam@pobox.com>
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

#include "ogr_gme.h"
#include "cpl_minixml.h"

CPL_CVSID("$Id$");

/************************************************************************/
/*                            OGRGMELayer()                             */
/************************************************************************/

OGRGMELayer::OGRGMELayer(OGRGMEDataSource* poDS)

{
    this->poDS = poDS;

    nNextInSeq = 0;

    poSRS = new OGRSpatialReference(SRS_WKT_WGS84);

    poFeatureDefn = NULL;

    nOffset = 0;
    bEOF = FALSE;

    iLatitudeField = iLongitudeField = -1;
    iGeometryField = -1;
    bHiddenGeometryField = FALSE;

    bFirstTokenIsFID = FALSE;
}

/************************************************************************/
/*                            ~OGRGMELayer()                            */
/************************************************************************/

OGRGMELayer::~OGRGMELayer()

{
    if( poSRS != NULL )
        poSRS->Release();

    if( poFeatureDefn != NULL )
        poFeatureDefn->Release();
}

/************************************************************************/
/*                            ResetReading()                            */
/************************************************************************/

void OGRGMELayer::ResetReading()

{
    nNextInSeq = 0;
    nOffset = 0;
    bEOF = FALSE;
}

/************************************************************************/
/*                           GetLayerDefn()                             */
/************************************************************************/

OGRFeatureDefn * OGRGMELayer::GetLayerDefn()
{
    CPLAssert(poFeatureDefn);
    return poFeatureDefn;
}

/************************************************************************/
/*                           GetNextFeature()                           */
/************************************************************************/

OGRFeature *OGRGMELayer::GetNextFeature()
{
    OGRFeature  *poFeature;

    GetLayerDefn();

    while(TRUE)
    {
        if (nNextInSeq < nOffset ||
            nNextInSeq >= nOffset + (int)aosRows.size())
        {
            if (bEOF)
                return NULL;

            nOffset += aosRows.size();
            if (!FetchNextRows())
                return NULL;
        }

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

char **OGRGMECSVSplitLine( const char *pszString, char chDelimiter )

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
/*                           ParseKMLGeometry()                         */
/************************************************************************/

static void ParseLineString(OGRLineString* poLS,
                            const char* pszCoordinates)
{
    char** papszTuples = CSLTokenizeString2(pszCoordinates, " ", 0);
    for(int iTuple = 0; papszTuples && papszTuples[iTuple]; iTuple++)
    {
        char** papszTokens = CSLTokenizeString2(papszTuples[iTuple], ",", 0);
        if (CSLCount(papszTokens) == 2)
            poLS->addPoint(atof(papszTokens[0]), atof(papszTokens[1]));
        else if (CSLCount(papszTokens) == 3)
            poLS->addPoint(atof(papszTokens[0]), atof(papszTokens[1]),
                            atof(papszTokens[2]));
        CSLDestroy(papszTokens);
    }
    CSLDestroy(papszTuples);
}

/* Could be moved somewhere else */

static OGRGeometry* ParseKMLGeometry(/* const */ CPLXMLNode* psXML)
{
    OGRGeometry* poGeom = NULL;
    const char* pszGeomType = psXML->pszValue;
    if (strcmp(pszGeomType, "Point") == 0)
    {
        const char* pszCoordinates = CPLGetXMLValue(psXML, "coordinates", NULL);
        if (pszCoordinates)
        {
            char** papszTokens = CSLTokenizeString2(pszCoordinates, ",", 0);
            if (CSLCount(papszTokens) == 2)
                poGeom = new OGRPoint(atof(papszTokens[0]), atof(papszTokens[1]));
            else if (CSLCount(papszTokens) == 3)
                poGeom = new OGRPoint(atof(papszTokens[0]), atof(papszTokens[1]),
                                      atof(papszTokens[2]));
            CSLDestroy(papszTokens);
        }
    }
    else if (strcmp(pszGeomType, "LineString") == 0)
    {
        const char* pszCoordinates = CPLGetXMLValue(psXML, "coordinates", NULL);
        if (pszCoordinates)
        {
            OGRLineString* poLS = new OGRLineString();
            ParseLineString(poLS, pszCoordinates);
            poGeom = poLS;
        }
    }
    else if (strcmp(pszGeomType, "Polygon") == 0)
    {
        OGRPolygon* poPoly = NULL;
        CPLXMLNode* psOuterBoundary = CPLGetXMLNode(psXML, "outerBoundaryIs");
        if (psOuterBoundary)
        {
            CPLXMLNode* psLinearRing = CPLGetXMLNode(psOuterBoundary, "LinearRing");
            const char* pszCoordinates = CPLGetXMLValue(
                psLinearRing ? psLinearRing : psOuterBoundary, "coordinates", NULL);
            if (pszCoordinates)
            {
                OGRLinearRing* poLS = new OGRLinearRing();
                ParseLineString(poLS, pszCoordinates);
                poPoly = new OGRPolygon();
                poPoly->addRingDirectly(poLS);
                poGeom = poPoly;
            }

            if (poPoly)
            {
                CPLXMLNode* psIter = psXML->psChild;
                while(psIter)
                {
                    if (psIter->eType == CXT_Element &&
                        strcmp(psIter->pszValue, "innerBoundaryIs") == 0)
                    {
                        CPLXMLNode* psLinearRing = CPLGetXMLNode(psIter, "LinearRing");
                        const char* pszCoordinates = CPLGetXMLValue(
                            psLinearRing ? psLinearRing : psIter, "coordinates", NULL);
                        if (pszCoordinates)
                        {
                            OGRLinearRing* poLS = new OGRLinearRing();
                            ParseLineString(poLS, pszCoordinates);
                            poPoly->addRingDirectly(poLS);
                        }
                    }
                    psIter = psIter->psNext;
                }
            }
        }
    }
    else if (strcmp(pszGeomType, "MultiGeometry") == 0)
    {
        CPLXMLNode* psIter;
        OGRwkbGeometryType eType = wkbUnknown;
        for(psIter = psXML->psChild; psIter; psIter = psIter->psNext)
        {
            if (psIter->eType == CXT_Element)
            {
                OGRwkbGeometryType eNewType = wkbUnknown;
                if (strcmp(psIter->pszValue, "Point") == 0)
                {
                    eNewType = wkbPoint;
                }
                else if (strcmp(psIter->pszValue, "LineString") == 0)
                {
                    eNewType = wkbLineString;
                }
                else if (strcmp(psIter->pszValue, "Polygon") == 0)
                {
                    eNewType = wkbPolygon;
                }
                else
                    break;
                if (eType == wkbUnknown)
                    eType = eNewType;
                else if (eType != eNewType)
                    break;
            }
        }
        OGRGeometryCollection* poColl = NULL;
        if (psIter != NULL)
            poColl = new OGRGeometryCollection();
        else if (eType == wkbPoint)
            poColl = new OGRMultiPoint();
        else if (eType == wkbLineString)
            poColl = new OGRMultiLineString();
        else if (eType == wkbPolygon)
            poColl = new OGRMultiPolygon();
        else
            CPLAssert(0);

        psIter = psXML->psChild;
        for(psIter = psXML->psChild; psIter; psIter = psIter->psNext)
        {
            if (psIter->eType == CXT_Element)
            {
                OGRGeometry* poSubGeom = ParseKMLGeometry(psIter);
                if (poSubGeom)
                    poColl->addGeometryDirectly(poSubGeom);
            }
        }

        poGeom = poColl;
    }

    return poGeom;
}

static OGRGeometry* ParseKMLGeometry(const char* pszKML)
{
    CPLXMLNode* psXML = CPLParseXMLString(pszKML);
    if (psXML == NULL)
        return NULL;

    if (psXML->eType != CXT_Element)
    {
        CPLDestroyXMLNode(psXML);
        return NULL;
    }

    OGRGeometry* poGeom = ParseKMLGeometry(psXML);

    CPLDestroyXMLNode(psXML);
    return poGeom;
}


/************************************************************************/
/*                         BuildFeatureFromSQL()                        */
/************************************************************************/

OGRFeature *OGRGMELayer::BuildFeatureFromSQL(const char* pszLine)
{
    OGRFeature* poFeature = new OGRFeature(poFeatureDefn);

    char** papszTokens = OGRGMECSVSplitLine(pszLine, ',');
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

    int nFieldCount = poFeatureDefn->GetFieldCount();
    if (nTokens == nFieldCount + bHiddenGeometryField + nAttrOffset)
    {
        for(int i=0;i<nFieldCount+bHiddenGeometryField;i++)
        {
            const char* pszVal = papszTokens[i+nAttrOffset];
            if (pszVal[0])
            {
                if (i<nFieldCount)
                    poFeature->SetField(i, pszVal);

                if (i == iGeometryField && i != iLatitudeField)
                {
                    if (pszVal[0] == '-' || (pszVal[0] >= '0' && pszVal[0] <= '9'))
                    {
                        char** papszLatLon = CSLTokenizeString2(pszVal, " ,", 0);
                        if (CSLCount(papszLatLon) == 2 &&
                            CPLGetValueType(papszLatLon[0]) != CPL_VALUE_STRING &&
                            CPLGetValueType(papszLatLon[1]) != CPL_VALUE_STRING)
                        {
                            OGRPoint* poPoint = new OGRPoint(atof( papszLatLon[1]),
                                                            atof( papszLatLon[0]));
                            poPoint->assignSpatialReference(poSRS);
                            poFeature->SetGeometryDirectly(poPoint);
                        }
                        CSLDestroy(papszLatLon);
                    }
                    else if (strstr(pszVal, "<Point>") ||
                             strstr(pszVal, "<LineString>") ||
                             strstr(pszVal, "<Polygon>"))
                    {
                        OGRGeometry* poGeom = ParseKMLGeometry(pszVal);
                        if (poGeom)
                        {
                            poGeom->assignSpatialReference(poSRS);
                            poFeature->SetGeometryDirectly(poGeom);
                        }
                    }
                }
                else if (i == iROWID)
                {
                    osFID = pszVal;
                }
            }
        }

        if (iLatitudeField >= 0 && iLongitudeField >= 0)
        {
            const char* pszLat = papszTokens[iLatitudeField+nAttrOffset];
            const char* pszLong = papszTokens[iLongitudeField+nAttrOffset];
            if (pszLat[0] != 0 && pszLong[0] != 0 &&
                CPLGetValueType(pszLat) != CPL_VALUE_STRING &&
                CPLGetValueType(pszLong) != CPL_VALUE_STRING)
            {
                OGRPoint* poPoint = new OGRPoint(atof(pszLong), atof(pszLat));
                poPoint->assignSpatialReference(poSRS);
                poFeature->SetGeometryDirectly(poPoint);
            }
        }
    }
    else
    {
        CPLDebug("GME", "Only %d columns for feature %s", nTokens, osFID.c_str());
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

OGRFeature *OGRGMELayer::GetNextRawFeature()
{
    if (nNextInSeq < nOffset ||
        nNextInSeq - nOffset >= (int)aosRows.size())
        return NULL;

    OGRFeature* poFeature = BuildFeatureFromSQL(aosRows[nNextInSeq - nOffset]);

    nNextInSeq ++;

    return poFeature;
}

/************************************************************************/
/*                          SetNextByIndex()                            */
/************************************************************************/

OGRErr OGRGMELayer::SetNextByIndex( long nIndex )
{
    if (nIndex < 0)
        return OGRERR_FAILURE;
    bEOF = FALSE;
    nNextInSeq = nIndex;
    return OGRERR_NONE;
}

/************************************************************************/
/*                           TestCapability()                           */
/************************************************************************/

int OGRGMELayer::TestCapability( const char * pszCap )

{
    if ( EQUAL(pszCap, OLCStringsAsUTF8) )
        return TRUE;
    else if ( EQUAL(pszCap, OLCFastSetNextByIndex) )
        return TRUE;
    return FALSE;
}

/************************************************************************/
/*                         GetGeometryColumn()                          */
/************************************************************************/

const char *OGRGMELayer::GetGeometryColumn()
{
    GetLayerDefn();
    if (iGeometryField < 0)
        return "";

    if (iGeometryField == poFeatureDefn->GetFieldCount())
    {
        CPLAssert(bHiddenGeometryField);
        return GetDefaultGeometryColumnName();
    }

    return poFeatureDefn->GetFieldDefn(iGeometryField)->GetNameRef();
}

/************************************************************************/
/*                              PatchSQL()                              */
/************************************************************************/

CPLString OGRGMELayer::PatchSQL(const char* pszSQL)
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

/************************************************************************/
/*                          GetSpatialRef()                             */
/************************************************************************/

OGRSpatialReference* OGRGMELayer::GetSpatialRef()
{
    GetLayerDefn();

    if (iGeometryField < 0)
        return NULL;

    return poSRS;
}

/************************************************************************/
/*                         LaunderColName()                             */
/************************************************************************/

CPLString OGRGMELayer::LaunderColName(const char* pszColName)
{
    CPLString osLaunderedColName;

    for(int i=0;pszColName[i];i++)
    {
        if (pszColName[i] == '\n')
            osLaunderedColName += "\\n";
        else
            osLaunderedColName += pszColName[i];
    }
    return osLaunderedColName;
}
