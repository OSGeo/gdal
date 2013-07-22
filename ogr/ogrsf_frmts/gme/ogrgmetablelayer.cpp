/******************************************************************************
 * $Id$
 *
 * Project:  Google Maps Engine API Driver
 * Purpose:  OGRGMETableLayer Implementation.
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

CPL_CVSID("$Id: ogrgmetablelayer.cpp 25475 2013-01-09 09:09:59Z warmerdam $");

/************************************************************************/
/*                         OGRGMETableLayer()                           */
/************************************************************************/

OGRGMETableLayer::OGRGMETableLayer(OGRGMEDataSource* poDS,
                                   const char* pszTableName,
                                   const char* pszTableId) : OGRGMELayer(poDS)

{
    osTableName = pszTableName;
    osTableId = pszTableId;
}

/************************************************************************/
/*                        ~OGRGMETableLayer()                           */
/************************************************************************/

OGRGMETableLayer::~OGRGMETableLayer()

{
}

/************************************************************************/
/*                            ResetReading()                            */
/************************************************************************/

void OGRGMETableLayer::ResetReading()

{
    OGRGMELayer::ResetReading();
}

/************************************************************************/
/*                           TestCapability()                           */
/************************************************************************/

int OGRGMETableLayer::TestCapability( const char * pszCap )

{
    if( EQUAL(pszCap,OLCRandomRead) )
        return TRUE;
    return OGRGMELayer::TestCapability(pszCap);
}

/************************************************************************/
/*                           FetchDescribe()                            */
/************************************************************************/

int OGRGMETableLayer::FetchDescribe()
{
    CPLString osRequest = "tables/" + osTableId;

    CPLHTTPResult *psDescribe = poDS->MakeRequest(osRequest);
    if (psDescribe != NULL)
        printf("Result=%s\n", (const char *) psDescribe->pabyData);
    return 0;
#ifdef notdef
    poFeatureDefn = new OGRFeatureDefn( osTableName );
    poFeatureDefn->Reference();

    const CPLString& osAuth = poDS->GetAccessToken();
    std::vector<CPLString> aosHeaderAndFirstDataLine;
    if (osAuth.size())
    {
        CPLString osSQL("DESCRIBE ");
        osSQL += osTableId;
        CPLHTTPResult * psResult = poDS->RunSQL(osSQL);

        if (psResult == NULL)
            return FALSE;

        char* pszLine = (char*) psResult->pabyData;
        if (pszLine == NULL ||
            psResult->pszErrBuf != NULL ||
            strncmp(pszLine, "column id,name,type",
                    strlen("column id,name,type")) != 0)
        {
            CPLHTTPDestroyResult(psResult);
            return FALSE;
        }

        pszLine = OGRGMEGotoNextLine(pszLine);

        std::vector<CPLString> aosLines;
        ParseCSVResponse(pszLine, aosLines);
        for(int i=0;i<(int)aosLines.size();i++)
        {
            char** papszTokens = OGRGMECSVSplitLine(aosLines[i], ',');
            if (CSLCount(papszTokens) == 3)
            {
                aosColumnInternalName.push_back(papszTokens[0]);

                //CPLDebug("GME", "%s %s %s", papszTokens[0], papszTokens[1], papszTokens[2]);
                OGRFieldType eType = OFTString;
                if (EQUAL(papszTokens[2], "number"))
                    eType = OFTReal;
                else if (EQUAL(papszTokens[2], "datetime"))
                    eType = OFTDateTime;

                if (EQUAL(papszTokens[2], "location") && osGeomColumnName.size() == 0)
                {
                    if (iGeometryField < 0)
                        iGeometryField = poFeatureDefn->GetFieldCount();
                    else
                        CPLDebug("GME", "Multiple geometry fields detected. "
                                         "Only first encountered one is handled");
                }

                CPLString osLaunderedColName(LaunderColName(papszTokens[1]));
                OGRFieldDefn oFieldDefn(osLaunderedColName, eType);
                poFeatureDefn->AddFieldDefn(&oFieldDefn);
            }
            CSLDestroy(papszTokens);
        }

        CPLHTTPDestroyResult(psResult);
    }
    else
    {
        /* http://code.google.com/intl/fr/apis/fusiontables/docs/developers_guide.html#Exploring states */
        /* that DESCRIBE should work on public tables without authentication, but it is not true... */
        CPLString osSQL("SELECT * FROM ");
        osSQL += osTableId;
        osSQL += " OFFSET 0 LIMIT 1";

        CPLHTTPResult * psResult = poDS->RunSQL(osSQL);

        if (psResult == NULL)
            return FALSE;

        char* pszLine = (char*) psResult->pabyData;
        if (pszLine == NULL || psResult->pszErrBuf != NULL)
        {
            CPLHTTPDestroyResult(psResult);
            return FALSE;
        }

        ParseCSVResponse(pszLine, aosHeaderAndFirstDataLine);
        if (aosHeaderAndFirstDataLine.size() > 0)
        {
            char** papszTokens = OGRGMECSVSplitLine(aosHeaderAndFirstDataLine[0], ',');
            for(int i=0;papszTokens && papszTokens[i];i++)
            {
                CPLString osLaunderedColName(LaunderColName(papszTokens[i]));
                OGRFieldDefn oFieldDefn(osLaunderedColName, OFTString);
                poFeatureDefn->AddFieldDefn(&oFieldDefn);
            }
            CSLDestroy(papszTokens);
        }

        CPLHTTPDestroyResult(psResult);
    }
    
    if (osGeomColumnName.size() > 0)
    {
        iGeometryField = poFeatureDefn->GetFieldIndex(osGeomColumnName);
        if (iGeometryField < 0)
        {
            CPLError(CE_Warning, CPLE_AppDefined,
                     "Cannot find column called %s", osGeomColumnName.c_str());
        }
    }

    for(int i=0;i<poFeatureDefn->GetFieldCount();i++)
    {
        const char* pszName = poFeatureDefn->GetFieldDefn(i)->GetNameRef();
        if (EQUAL(pszName, "latitude") || EQUAL(pszName, "lat") ||
            EQUAL(pszName, "latdec"))
            iLatitudeField = i;
        else if (EQUAL(pszName, "longitude") || EQUAL(pszName, "lon") ||
                 EQUAL(pszName, "londec") || EQUAL(pszName, "long"))
            iLongitudeField = i;
    }

    if (iLatitudeField >= 0 && iLongitudeField >= 0)
    {
        if (iGeometryField < 0)
            iGeometryField = iLatitudeField;
        poFeatureDefn->GetFieldDefn(iLatitudeField)->SetType(OFTReal);
        poFeatureDefn->GetFieldDefn(iLongitudeField)->SetType(OFTReal);
        poFeatureDefn->SetGeomType( wkbPoint );
    }
    else if (iGeometryField < 0 && osGeomColumnName.size() == 0)
    {
        iLatitudeField = iLongitudeField = -1;

        /* In the unauthentified case, we try to parse the first record to */
        /* autodetect the geometry field */
        OGRwkbGeometryType eType = wkbUnknown;
        if (aosHeaderAndFirstDataLine.size() == 2)
        {
            char** papszTokens = OGRGMECSVSplitLine(aosHeaderAndFirstDataLine[1], ',');
            if (CSLCount(papszTokens) == poFeatureDefn->GetFieldCount())
            {
                for(int i=0;i<poFeatureDefn->GetFieldCount();i++)
                {
                    const char* pszVal = papszTokens[i];
                    if (pszVal != NULL &&
                        (strncmp(pszVal, "<Point>", 7) == 0 ||
                         strncmp(pszVal, "<LineString>", 12) == 0 ||
                         strncmp(pszVal, "<Polygon>", 9) == 0 ||
                         strncmp(pszVal, "<MultiGeometry>", 15) == 0))
                    {
                        if (iGeometryField < 0)
                        {
                            iGeometryField = i;
                        }
                        else
                        {
                            CPLDebug("GME", "Multiple geometry fields detected. "
                                     "Only first encountered one is handled");
                        }
                    }
                    else if (pszVal)
                    {
                        /* http://www.google.com/fusiontables/DataSource?dsrcid=423292 */
                        char** papszTokens2 = CSLTokenizeString2(pszVal, " ,", 0);
                        if (CSLCount(papszTokens2) == 2 &&
                            CPLGetValueType(papszTokens2[0]) == CPL_VALUE_REAL &&
                            CPLGetValueType(papszTokens2[1]) == CPL_VALUE_REAL &&
                            fabs(CPLAtof(papszTokens2[0])) <= 90 &&
                            fabs(CPLAtof(papszTokens2[1])) <= 180 )
                        {
                            if (iGeometryField < 0)
                            {
                                iGeometryField = i;
                                eType = wkbPoint;
                            }
                            else
                            {
                                CPLDebug("GME", "Multiple geometry fields detected. "
                                         "Only first encountered one is handled");
                            }
                        }
                        CSLDestroy(papszTokens2);
                    }
                }
            }
            CSLDestroy(papszTokens);
        }
        
        if (iGeometryField < 0)
            poFeatureDefn->SetGeomType( wkbNone );
        else
            poFeatureDefn->SetGeomType( eType );
    }

    return TRUE;
#endif
}

/************************************************************************/
/*                         EscapeAndQuote()                             */
/************************************************************************/

static CPLString EscapeAndQuote(const char* pszStr)
{
    char ch;
    CPLString osRes("'");
    while((ch = *pszStr) != 0)
    {
        if (ch == '\'')
            osRes += "\\\'";
        else
            osRes += ch;
        pszStr ++;
    }
    osRes += "'";
    return osRes;
}

/************************************************************************/
/*                           FetchNextRows()                            */
/************************************************************************/

int OGRGMETableLayer::FetchNextRows()
{
    return 0;
#ifdef notdef
    aosRows.resize(0);

    CPLString osSQL("SELECT ROWID");
    for(int i=0;i<poFeatureDefn->GetFieldCount();i++)
    {
        osSQL += ",";

        if (i < (int)aosColumnInternalName.size())
            osSQL += aosColumnInternalName[i];
        else
        {
            const char* pszFieldName =
                poFeatureDefn->GetFieldDefn(i)->GetNameRef();
            osSQL += EscapeAndQuote(pszFieldName);
        }
    }
    if (bHiddenGeometryField)
    {
        osSQL += ",";
        osSQL += EscapeAndQuote(GetGeometryColumn());
    }
    osSQL += " FROM ";
    osSQL += osTableId;
    if (osWHERE.size())
    {
        osSQL += " ";
        osSQL += osWHERE;
    }

    int nFeaturesToFetch = GetFeaturesToFetch();
    if (nFeaturesToFetch > 0)
        osSQL += CPLSPrintf(" OFFSET %d LIMIT %d", nOffset, nFeaturesToFetch);

    CPLPushErrorHandler(CPLQuietErrorHandler);
    CPLHTTPResult * psResult = poDS->RunSQL(osSQL);
    CPLPopErrorHandler();

    if (psResult == NULL)
    {
        bEOF = TRUE;
        return FALSE;
    }

    char* pszLine = (char*) psResult->pabyData;
    if (pszLine == NULL || psResult->pszErrBuf != NULL)
    {
        CPLDebug("GME", "Error : %s", pszLine ? pszLine : psResult->pszErrBuf);
        CPLHTTPDestroyResult(psResult);
        bEOF = TRUE;
        return FALSE;
    }

    ParseCSVResponse(pszLine, aosRows);

    if (aosRows.size() > 0)
        aosRows.erase(aosRows.begin());

    if (nFeaturesToFetch > 0)
        bEOF = (int)aosRows.size() < GetFeaturesToFetch();
    else
        bEOF = TRUE;

    CPLHTTPDestroyResult(psResult);

    return TRUE;
#endif
}
/************************************************************************/
/*                            GetFeature()                              */
/************************************************************************/

OGRFeature * OGRGMETableLayer::GetFeature( long nFID )
{
    return NULL;

#ifdef notdef
    GetLayerDefn();

    CPLString osSQL("SELECT ROWID");
    for(int i=0;i<poFeatureDefn->GetFieldCount();i++)
    {
        osSQL += ",";

        const char* pszFieldName =
            poFeatureDefn->GetFieldDefn(i)->GetNameRef();
        osSQL += EscapeAndQuote(pszFieldName);
    }
    if (bHiddenGeometryField)
    {
        osSQL += ",";
        osSQL += EscapeAndQuote(GetGeometryColumn());
    }
    osSQL += " FROM ";
    osSQL += osTableId;
    osSQL += CPLSPrintf(" WHERE ROWID='%ld'", nFID);

    CPLPushErrorHandler(CPLQuietErrorHandler);
    CPLHTTPResult * psResult = poDS->RunSQL(osSQL);
    CPLPopErrorHandler();

    if (psResult == NULL)
        return NULL;

    char* pszLine = (char*) psResult->pabyData;
    if (pszLine == NULL || psResult->pszErrBuf != NULL)
    {
        CPLHTTPDestroyResult(psResult);
        return NULL;
    }

    /* skip header line */
    pszLine = OGRGMEGotoNextLine(pszLine);
    if (pszLine == NULL || pszLine[0] == 0)
    {
        CPLHTTPDestroyResult(psResult);
        return NULL;
    }

    int nLen = (int)strlen(pszLine);
    if (nLen > 0 && pszLine[nLen-1] == '\n')
        pszLine[nLen-1] = '\0';

    OGRFeature* poFeature = BuildFeatureFromSQL(pszLine);

    CPLHTTPDestroyResult(psResult);

    return poFeature;
#endif
}

/************************************************************************/
/*                           GetLayerDefn()                             */
/************************************************************************/

OGRFeatureDefn * OGRGMETableLayer::GetLayerDefn()
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

int OGRGMETableLayer::GetFeatureCount(int bForce)
{
    return 0;
#ifdef notdef
    GetLayerDefn();

    CPLString osSQL("SELECT COUNT() FROM ");
    osSQL += osTableId;
    if (osWHERE.size())
    {
        osSQL += " ";
        osSQL += osWHERE;
    }

    CPLHTTPResult * psResult = poDS->RunSQL(osSQL);

    if (psResult == NULL)
        return 0;

    char* pszLine = (char*) psResult->pabyData;
    if (pszLine == NULL ||
        strncmp(pszLine, "count()", 7) != 0 ||
        psResult->pszErrBuf != NULL)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "GetFeatureCount() failed");
        CPLHTTPDestroyResult(psResult);
        return 0;
    }

    pszLine = OGRGMEGotoNextLine(pszLine);
    if (pszLine == NULL)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "GetFeatureCount() failed");
        CPLHTTPDestroyResult(psResult);
        return 0;
    }

    char* pszNextLine = OGRGMEGotoNextLine(pszLine);
    if (pszNextLine)
        pszNextLine[-1] = 0;

    int nFeatureCount = atoi(pszLine);

    CPLHTTPDestroyResult(psResult);

    return nFeatureCount;
#endif
}

/************************************************************************/
/*                         SetAttributeFilter()                         */
/************************************************************************/

OGRErr OGRGMETableLayer::SetAttributeFilter( const char *pszQuery )

{
    GetLayerDefn();

    if( pszQuery == NULL )
        osQuery = "";
    else
    {
        osQuery = PatchSQL(pszQuery);
    }

    BuildWhere();

    ResetReading();

    return OGRERR_NONE;
}


/************************************************************************/
/*                          SetSpatialFilter()                          */
/************************************************************************/

void OGRGMETableLayer::SetSpatialFilter( OGRGeometry * poGeomIn )

{
    GetLayerDefn();

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

void OGRGMETableLayer::BuildWhere()

{
    osWHERE = "";

    if( m_poFilterGeom != NULL && iGeometryField >= 0)
    {
        OGREnvelope  sEnvelope;

        m_poFilterGeom->getEnvelope( &sEnvelope );

        CPLString osQuotedGeomColumn(EscapeAndQuote(GetGeometryColumn()));

        osWHERE.Printf("WHERE ST_INTERSECTS(%s, RECTANGLE(LATLNG(%.12f, %.12f), LATLNG(%.12f, %.12f)))",
                       osQuotedGeomColumn.c_str(),
                       MAX(-90.,sEnvelope.MinY - 1e-11), MAX(-180., sEnvelope.MinX - 1e-11),
                       MIN(90.,sEnvelope.MaxY + 1e-11), MIN(180.,sEnvelope.MaxX + 1e-11));
    }

    if( strlen(osQuery) > 0 )
    {
        if( strlen(osWHERE) == 0 )
            osWHERE = "WHERE ";
        else
            osWHERE += " AND ";
        osWHERE += osQuery;
    }
}

/************************************************************************/
/*                          SetGeometryType()                           */
/************************************************************************/

void OGRGMETableLayer::SetGeometryType(OGRwkbGeometryType eGType)
{
    eGTypeForCreation = eGType;
}
