/******************************************************************************
 * $Id$
 *
 * Project:  GFT Translator
 * Purpose:  Implements OGRGFTTableLayer class.
 * Author:   Even Rouault, <even dot rouault at mines dash paris dot org>
 *
 ******************************************************************************
 * Copyright (c) 2011-2013, Even Rouault <even dot rouault at mines-paris dot org>
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

CPL_CVSID("$Id$");

/************************************************************************/
/*                         OGRGFTTableLayer()                           */
/************************************************************************/

OGRGFTTableLayer::OGRGFTTableLayer(OGRGFTDataSource* poDS,
                         const char* pszTableName,
                         const char* pszTableId,
                         const char* pszGeomColumnName) : OGRGFTLayer(poDS)

{
    osTableName = pszTableName;
    osTableId = pszTableId;
    osGeomColumnName = pszGeomColumnName ? pszGeomColumnName : "";

    bHasTriedCreateTable = FALSE;
    bInTransaction = FALSE;
    nFeaturesInTransaction = 0;

    bFirstTokenIsFID = TRUE;
    eGTypeForCreation = wkbUnknown;
    
    SetDescription( osTableName );
}

/************************************************************************/
/*                        ~OGRGFTTableLayer()                           */
/************************************************************************/

OGRGFTTableLayer::~OGRGFTTableLayer()

{
    CreateTableIfNecessary();
}

/************************************************************************/
/*                            ResetReading()                            */
/************************************************************************/

void OGRGFTTableLayer::ResetReading()

{
    OGRGFTLayer::ResetReading();
    aosRows.resize(0);
}

/************************************************************************/
/*                           TestCapability()                           */
/************************************************************************/

int OGRGFTTableLayer::TestCapability( const char * pszCap )

{
    if( EQUAL(pszCap,OLCRandomRead) )
        return TRUE;

    else if( EQUAL(pszCap,OLCSequentialWrite)
             || EQUAL(pszCap,OLCRandomWrite)
             || EQUAL(pszCap,OLCDeleteFeature) )
        return poDS->IsReadWrite();

    else if( EQUAL(pszCap,OLCCreateField) )
        return poDS->IsReadWrite();

    else if( EQUAL(pszCap, OLCTransactions) )
        return poDS->IsReadWrite();

    return OGRGFTLayer::TestCapability(pszCap);
}

/************************************************************************/
/*                           FetchDescribe()                            */
/************************************************************************/

int OGRGFTTableLayer::FetchDescribe()
{
    poFeatureDefn = new OGRFeatureDefn( osTableName );
    poFeatureDefn->Reference();
    poFeatureDefn->GetGeomFieldDefn(0)->SetSpatialRef(poSRS);

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

        pszLine = OGRGFTGotoNextLine(pszLine);

        std::vector<CPLString> aosLines;
        ParseCSVResponse(pszLine, aosLines);
        for(int i=0;i<(int)aosLines.size();i++)
        {
            char** papszTokens = OGRGFTCSVSplitLine(aosLines[i], ',');
            if (CSLCount(papszTokens) == 3)
            {
                aosColumnInternalName.push_back(papszTokens[0]);

                //CPLDebug("GFT", "%s %s %s", papszTokens[0], papszTokens[1], papszTokens[2]);
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
                        CPLDebug("GFT", "Multiple geometry fields detected. "
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
            char** papszTokens = OGRGFTCSVSplitLine(aosHeaderAndFirstDataLine[0], ',');
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
            char** papszTokens = OGRGFTCSVSplitLine(aosHeaderAndFirstDataLine[1], ',');
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
                            CPLDebug("GFT", "Multiple geometry fields detected. "
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
                                CPLDebug("GFT", "Multiple geometry fields detected. "
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

    SetGeomFieldName();

    return TRUE;
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

int OGRGFTTableLayer::FetchNextRows()
{
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
        CPLDebug("GFT", "Error : %s", pszLine ? pszLine : psResult->pszErrBuf);
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
}
/************************************************************************/
/*                            GetFeature()                              */
/************************************************************************/

OGRFeature * OGRGFTTableLayer::GetFeature( long nFID )
{
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
    pszLine = OGRGFTGotoNextLine(pszLine);
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
}

/************************************************************************/
/*                           GetLayerDefn()                             */
/************************************************************************/

OGRFeatureDefn * OGRGFTTableLayer::GetLayerDefn()
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

int OGRGFTTableLayer::GetFeatureCount(int bForce)
{
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

    pszLine = OGRGFTGotoNextLine(pszLine);
    if (pszLine == NULL)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "GetFeatureCount() failed");
        CPLHTTPDestroyResult(psResult);
        return 0;
    }

    char* pszNextLine = OGRGFTGotoNextLine(pszLine);
    if (pszNextLine)
        pszNextLine[-1] = 0;

    int nFeatureCount = atoi(pszLine);

    CPLHTTPDestroyResult(psResult);

    return nFeatureCount;
}

/************************************************************************/
/*                            CreateField()                             */
/************************************************************************/

OGRErr OGRGFTTableLayer::CreateField( OGRFieldDefn *poField,
                                 int bApproxOK )
{

    if (!poDS->IsReadWrite())
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Operation not available in read-only mode");
        return OGRERR_FAILURE;
    }

    if (osTableId.size() != 0)
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "Cannot add field to already created table");
        return OGRERR_FAILURE;
    }

    if (poDS->GetAccessToken().size() == 0)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Operation not available in unauthenticated mode");
        return OGRERR_FAILURE;
    }

    if (poFeatureDefn == NULL)
    {
        poFeatureDefn = new OGRFeatureDefn( osTableName );
        poFeatureDefn->Reference();
        poFeatureDefn->GetGeomFieldDefn(0)->SetSpatialRef(poSRS);
        poFeatureDefn->GetGeomFieldDefn(0)->SetName(GetDefaultGeometryColumnName());
    }

    poFeatureDefn->AddFieldDefn(poField);

    return OGRERR_NONE;
}

/************************************************************************/
/*                       CreateTableIfNecessary()                       */
/************************************************************************/

void OGRGFTTableLayer::CreateTableIfNecessary()
{
    if (bHasTriedCreateTable || osTableId.size() != 0)
        return;

    bHasTriedCreateTable = TRUE;

    CPLString osSQL("CREATE TABLE '");
    osSQL += osTableName;
    osSQL += "' (";

    int i;

    if (poFeatureDefn == NULL)
    {
        /* In case CreateField() hasn't yet been called */
        poFeatureDefn = new OGRFeatureDefn( osTableName );
        poFeatureDefn->Reference();
        poFeatureDefn->GetGeomFieldDefn(0)->SetSpatialRef(poSRS);
        poFeatureDefn->GetGeomFieldDefn(0)->SetName(GetDefaultGeometryColumnName());
    }

    /* If there are longitude and latitude fields, use the latitude */
    /* field as the LOCATION field */
    for(i=0;i<poFeatureDefn->GetFieldCount();i++)
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
        iGeometryField = iLatitudeField;
        poFeatureDefn->SetGeomType( wkbPoint );
    }
    /* If no longitude/latitude field exist, let's look at a column */
    /* named 'geometry' and use it as the LOCATION column if the layer */
    /* hasn't been created with a none geometry type */
    else if (iGeometryField < 0 && eGTypeForCreation != wkbNone)
    {
        iGeometryField = poFeatureDefn->GetFieldIndex(GetDefaultGeometryColumnName());
        poFeatureDefn->SetGeomType( eGTypeForCreation );
    }
    /* The user doesn't want geometries, so don't create one */
    else if (eGTypeForCreation == wkbNone)
    {
        poFeatureDefn->SetGeomType( eGTypeForCreation );
    }

    for(i=0;i<poFeatureDefn->GetFieldCount();i++)
    {
        if (i > 0)
            osSQL += ", ";

        const char* pszFieldName =
            poFeatureDefn->GetFieldDefn(i)->GetNameRef();
        osSQL += EscapeAndQuote(pszFieldName);
        osSQL += ": ";

        if (iGeometryField == i)
        {
            osSQL += "LOCATION";
        }
        else
        {
            switch(poFeatureDefn->GetFieldDefn(i)->GetType())
            {
                case OFTInteger:
                case OFTReal:
                    osSQL += "NUMBER";
                    break;
                default:
                    osSQL += "STRING";
            }
        }
    }

    /* If there's not yet a geometry field and the user didn't forbid */
    /* the creation of one, then let's add it to the CREATE TABLE, but */
    /* DO NOT add it to the feature defn as a feature might already have */
    /* been created with it, so it is not safe to alter it at that point. */
    /* So we set the bHiddenGeometryField flag to be able to fetch/set this */
    /* column but not try to get/set a related feature field */
    if (iGeometryField < 0 && eGTypeForCreation != wkbNone)
    {
        if (i > 0)
            osSQL += ", ";
        osSQL += EscapeAndQuote(GetDefaultGeometryColumnName());
        osSQL += ": LOCATION";

        iGeometryField = poFeatureDefn->GetFieldCount();
        bHiddenGeometryField = TRUE;
    }
    osSQL += ")";

    CPLHTTPResult * psResult = poDS->RunSQL(osSQL);
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

    pszLine = OGRGFTGotoNextLine(pszLine);
    if (pszLine == NULL)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Table creation failed");
        CPLHTTPDestroyResult(psResult);
        return;
    }

    char* pszNextLine = OGRGFTGotoNextLine(pszLine);
    if (pszNextLine)
        pszNextLine[-1] = 0;

    osTableId = pszLine;
    CPLDebug("GFT", "Table %s --> id = %s", osTableName.c_str(), osTableId.c_str());

    CPLHTTPDestroyResult(psResult);
}

/************************************************************************/
/*                           CreateFeature()                            */
/************************************************************************/

OGRErr OGRGFTTableLayer::CreateFeature( OGRFeature *poFeature )

{
    if (!poDS->IsReadWrite())
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Operation not available in read-only mode");
        return OGRERR_FAILURE;
    }

    if (osTableId.size() == 0)
    {
        CreateTableIfNecessary();
        if (osTableId.size() == 0)
        {
            CPLError(CE_Failure, CPLE_NotSupported,
                    "Cannot add feature to non-created table");
            return OGRERR_FAILURE;
        }
    }

    if (poDS->GetAccessToken().size() == 0)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Operation not available in unauthenticated mode");
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

        const char* pszFieldName =
            poFeatureDefn->GetFieldDefn(iField)->GetNameRef();
        osCommand += EscapeAndQuote(pszFieldName);
    }
    if (bHiddenGeometryField)
    {
        if (iField > 0)
            osCommand += ", ";
        osCommand += EscapeAndQuote(GetGeometryColumn());
    }
    osCommand += ") VALUES (";
    for(iField = 0; iField < nFieldCount + bHiddenGeometryField; iField++)
    {
        if (iField > 0)
            osCommand += ", ";

        OGRGeometry* poGeom = poFeature->GetGeometryRef();
        /* If there's a geometry, let's use it in priority over the textual */
        /* content of the field. */
        if (iGeometryField != iLatitudeField && iField == iGeometryField &&
            (iField == nFieldCount || poGeom != NULL || !poFeature->IsFieldSet( iField )))
        {
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
            OGRFieldType eType = poFeatureDefn->GetFieldDefn(iField)->GetType();
            if (eType != OFTInteger && eType != OFTReal)
            {
                CPLString osTmp;
                const char* pszVal = poFeature->GetFieldAsString(iField);

                if (!CPLIsUTF8(pszVal, -1))
                {
                    static int bFirstTime = TRUE;
                    if (bFirstTime)
                    {
                        bFirstTime = FALSE;
                        CPLError(CE_Warning, CPLE_AppDefined,
                                "%s is not a valid UTF-8 string. Forcing it to ASCII.\n"
                                "This warning won't be issued anymore", pszVal);
                    }
                    else
                    {
                        CPLDebug("OGR", "%s is not a valid UTF-8 string. Forcing it to ASCII",
                                pszVal);
                    }
                    char* pszEscaped = CPLForceToASCII(pszVal, -1, '?');
                    osTmp = pszEscaped;
                    CPLFree(pszEscaped);
                    pszVal = osTmp.c_str();
                }

                osCommand += EscapeAndQuote(pszVal);
            }
            else
                osCommand += poFeature->GetFieldAsString(iField);
        }
    }

    osCommand += ")";

    //CPLDebug("GFT", "%s",  osCommand.c_str());

    if (bInTransaction)
    {
        nFeaturesInTransaction ++;
        if (nFeaturesInTransaction > 1)
            osTransaction += "; ";
        osTransaction += osCommand;
        return OGRERR_NONE;
    }

    CPLHTTPResult * psResult = poDS->RunSQL(osCommand);
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

    pszLine = OGRGFTGotoNextLine(pszLine);
    if (pszLine == NULL)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Feature creation failed");
        CPLHTTPDestroyResult(psResult);
        return OGRERR_FAILURE;
    }

    char* pszNextLine = OGRGFTGotoNextLine(pszLine);
    if (pszNextLine)
        pszNextLine[-1] = 0;

    CPLDebug("GFT", "Feature id = %s",  pszLine);

    int nFID = atoi(pszLine);
    if (strcmp(CPLSPrintf("%d", nFID), pszLine) == 0)
        poFeature->SetFID(nFID);

    CPLHTTPDestroyResult(psResult);

    return OGRERR_NONE;
}

/************************************************************************/
/*                           SetFeature()                               */
/************************************************************************/

OGRErr      OGRGFTTableLayer::SetFeature( OGRFeature *poFeature )
{
    GetLayerDefn();

    if (!poDS->IsReadWrite())
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Operation not available in read-only mode");
        return OGRERR_FAILURE;
    }

    if (osTableId.size() == 0)
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                "Cannot set feature to non-created table");
        return OGRERR_FAILURE;
    }

    if (poDS->GetAccessToken().size() == 0)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Operation not available in unauthenticated mode");
        return OGRERR_FAILURE;
    }

    if (poFeature->GetFID() == OGRNullFID)
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "FID required on features given to SetFeature()." );
        return OGRERR_FAILURE;
    }

    CPLString      osCommand;

    osCommand += "UPDATE ";
    osCommand += osTableId;
    osCommand += " SET ";

    int iField;
    int nFieldCount = poFeatureDefn->GetFieldCount();
    for(iField = 0; iField < nFieldCount + bHiddenGeometryField; iField++)
    {
        if (iField > 0)
            osCommand += ", ";

        if (iField == nFieldCount)
        {
            osCommand += EscapeAndQuote(GetGeometryColumn());
        }
        else
        {
            const char* pszFieldName =
                poFeatureDefn->GetFieldDefn(iField)->GetNameRef();
            osCommand += EscapeAndQuote(pszFieldName);
        }

        osCommand += " = ";

        OGRGeometry* poGeom = poFeature->GetGeometryRef();
        if (iGeometryField != iLatitudeField && iField == iGeometryField &&
            (iField == nFieldCount || poGeom != NULL || !poFeature->IsFieldSet( iField )))
        {
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
            OGRFieldType eType = poFeatureDefn->GetFieldDefn(iField)->GetType();
            if (eType != OFTInteger && eType != OFTReal)
            {
                CPLString osTmp;
                const char* pszVal = poFeature->GetFieldAsString(iField);

                if (!CPLIsUTF8(pszVal, -1))
                {
                    static int bFirstTime = TRUE;
                    if (bFirstTime)
                    {
                        bFirstTime = FALSE;
                        CPLError(CE_Warning, CPLE_AppDefined,
                                "%s is not a valid UTF-8 string. Forcing it to ASCII.\n"
                                "This warning won't be issued anymore", pszVal);
                    }
                    else
                    {
                        CPLDebug("OGR", "%s is not a valid UTF-8 string. Forcing it to ASCII",
                                pszVal);
                    }
                    char* pszEscaped = CPLForceToASCII(pszVal, -1, '?');
                    osTmp = pszEscaped;
                    CPLFree(pszEscaped);
                    pszVal = osTmp.c_str();
                }

                osCommand += EscapeAndQuote(pszVal);
            }
            else
                osCommand += poFeature->GetFieldAsString(iField);
        }
    }

    osCommand += " WHERE ROWID = '";
    osCommand += CPLSPrintf("%ld", poFeature->GetFID());
    osCommand += "'";

    CPLHTTPResult * psResult = poDS->RunSQL(osCommand);
    if (psResult == NULL)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Feature update failed (1)");
        return OGRERR_FAILURE;
    }

/* -------------------------------------------------------------------- */
/*      We expect a response like "affected_rows\n1".                   */
/* -------------------------------------------------------------------- */
    char* pszLine = (char*) psResult->pabyData;
    if (pszLine == NULL ||
        strncmp(pszLine, "affected_rows\n1\n", 16) != 0 ||
        psResult->pszErrBuf != NULL)
    {
        CPLDebug( "GFT", "%s/%s", 
                  pszLine ? pszLine : "null", 
                  psResult->pszErrBuf ? psResult->pszErrBuf : "null");
        CPLError(CE_Failure, CPLE_AppDefined, "Feature update failed (2)");
        CPLHTTPDestroyResult(psResult);
        return OGRERR_FAILURE;
    }

    CPLHTTPDestroyResult(psResult);

    return OGRERR_NONE;
}

/************************************************************************/
/*                          DeleteFeature()                             */
/************************************************************************/

OGRErr OGRGFTTableLayer::DeleteFeature( long nFID )
{
    GetLayerDefn();

    if (!poDS->IsReadWrite())
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Operation not available in read-only mode");
        return OGRERR_FAILURE;
    }

    if (osTableId.size() == 0)
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                "Cannot delete feature in non-created table");
        return OGRERR_FAILURE;
    }

    if (poDS->GetAccessToken().size() == 0)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Operation not available in unauthenticated mode");
        return OGRERR_FAILURE;
    }

    CPLString      osCommand;

    osCommand += "DELETE FROM ";
    osCommand += osTableId;
    osCommand += " WHERE ROWID = '";
    osCommand += CPLSPrintf("%ld", nFID);
    osCommand += "'";

    //CPLDebug("GFT", "%s",  osCommand.c_str());

    CPLHTTPResult * psResult = poDS->RunSQL(osCommand);
    if (psResult == NULL)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Feature deletion failed (1)");
        return OGRERR_FAILURE;
    }

/* -------------------------------------------------------------------- */
/*      We expect a response like "affected_rows\n1".                   */
/* -------------------------------------------------------------------- */
    char* pszLine = (char*) psResult->pabyData;
    if (pszLine == NULL ||
        strncmp(pszLine, "affected_rows\n1\n", 16) != 0 ||
        psResult->pszErrBuf != NULL)
    {
        CPLDebug( "GFT", "%s/%s", 
                  pszLine ? pszLine : "null", 
                  psResult->pszErrBuf ? psResult->pszErrBuf : "null");
        CPLError(CE_Failure, CPLE_AppDefined, "Feature deletion failed (2)");
        CPLHTTPDestroyResult(psResult);
        return OGRERR_FAILURE;
    }

    CPLHTTPDestroyResult(psResult);

    return OGRERR_NONE;
}

/************************************************************************/
/*                         StartTransaction()                           */
/************************************************************************/

OGRErr OGRGFTTableLayer::StartTransaction()
{
    GetLayerDefn();

    if (bInTransaction)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Already in transaction");
        return OGRERR_FAILURE;
    }

    if (!poDS->IsReadWrite())
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Operation not available in read-only mode");
        return OGRERR_FAILURE;
    }

    if (osTableId.size() == 0)
    {
        CreateTableIfNecessary();
        if (osTableId.size() == 0)
        {
            CPLError(CE_Failure, CPLE_NotSupported,
                    "Cannot add feature to non-created table");
            return OGRERR_FAILURE;
        }
    }

    if (poDS->GetAccessToken().size() == 0)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Operation not available in unauthenticated mode");
        return OGRERR_FAILURE;
    }

    bInTransaction = TRUE;
    osTransaction.resize(0);
    nFeaturesInTransaction = 0;

    return OGRERR_NONE;
}

/************************************************************************/
/*                         CommitTransaction()                          */
/************************************************************************/

OGRErr OGRGFTTableLayer::CommitTransaction()
{
    GetLayerDefn();

    if (!bInTransaction)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Should be in transaction");
        return OGRERR_FAILURE;
    }

    bInTransaction = FALSE;

    if (nFeaturesInTransaction > 0)
    {
        if (nFeaturesInTransaction > 1)
            osTransaction += ";";

        CPLHTTPResult * psResult = poDS->RunSQL(osTransaction);
        osTransaction.resize(0);
        nFeaturesInTransaction = 0;

        if (psResult == NULL)
        {
            CPLError(CE_Failure, CPLE_AppDefined, "CommitTransaction failed");
            return OGRERR_FAILURE;
        }

        char* pszLine = (char*) psResult->pabyData;
        if (pszLine == NULL ||
            strncmp(pszLine, "rowid", 5) != 0 ||
            psResult->pszErrBuf != NULL)
        {
            CPLError(CE_Failure, CPLE_AppDefined, "CommitTransaction failed : %s",
                     pszLine ? pszLine : psResult->pszErrBuf);
            CPLHTTPDestroyResult(psResult);
            return OGRERR_FAILURE;
        }

        pszLine = OGRGFTGotoNextLine(pszLine);
        while(pszLine && *pszLine != 0)
        {
            char* pszNextLine = OGRGFTGotoNextLine(pszLine);
            if (pszNextLine)
                pszNextLine[-1] = 0;
            //CPLDebug("GFT", "Feature id = %s",  pszLine);

            pszLine = pszNextLine;
        }

        CPLHTTPDestroyResult(psResult);
    }

    return OGRERR_NONE;
}

/************************************************************************/
/*                        RollbackTransaction()                         */
/************************************************************************/

OGRErr OGRGFTTableLayer::RollbackTransaction()
{
    GetLayerDefn();

    if (!bInTransaction)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Should be in transaction");
        return OGRERR_FAILURE;
    }
    bInTransaction = FALSE;
    nFeaturesInTransaction = 0;
    osTransaction.resize(0);
    return OGRERR_NONE;
}

/************************************************************************/
/*                         SetAttributeFilter()                         */
/************************************************************************/

OGRErr OGRGFTTableLayer::SetAttributeFilter( const char *pszQuery )

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

void OGRGFTTableLayer::SetSpatialFilter( OGRGeometry * poGeomIn )

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

void OGRGFTTableLayer::BuildWhere()

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

void OGRGFTTableLayer::SetGeometryType(OGRwkbGeometryType eGType)
{
    eGTypeForCreation = eGType;
}
