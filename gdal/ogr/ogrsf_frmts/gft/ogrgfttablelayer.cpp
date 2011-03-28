/******************************************************************************
 * $Id$
 *
 * Project:  GFT Translator
 * Purpose:  Implements OGRGFTTableLayer class.
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
/*                         OGRGFTTableLayer()                           */
/************************************************************************/

OGRGFTTableLayer::OGRGFTTableLayer(OGRGFTDataSource* poDS,
                         const char* pszTableName,
                         const char* pszTableId) : OGRGFTLayer(poDS)

{
    osTableName = pszTableName;
    osTableId = pszTableId;

    bHasTriedCreateTable = FALSE;
    bInTransaction = FALSE;
    nFeaturesInTransaction = 0;

    bFirstTokenIsFID = TRUE;
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
             /*|| EQUAL(pszCap,OLCRandomWrite)  */)
        return poDS->IsReadWrite();

    else if( EQUAL(pszCap,OLCCreateField) )
        return poDS->IsReadWrite();

    else if( EQUAL(pszCap, OLCTransactions) )
        return poDS->IsReadWrite();

    return FALSE;
}

/************************************************************************/
/*                           FetchDescribe()                            */
/************************************************************************/

int OGRGFTTableLayer::FetchDescribe()
{
    poFeatureDefn = new OGRFeatureDefn( osTableName );
    poFeatureDefn->Reference();

    const CPLString& osAuth = poDS->GetAuth();
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
            strncmp(pszLine, "column id,name,type", strlen("column id,name,type")) != 0)
        {
            CPLHTTPDestroyResult(psResult);
            return FALSE;
        }

        pszLine = OGRGFTGotoNextLine(pszLine);
        while(pszLine != NULL && *pszLine != 0)
        {
            char* pszNextLine = OGRGFTGotoNextLine(pszLine);
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

        char* pszNextLine = OGRGFTGotoNextLine(pszLine);
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
            iLatitudeField = i;
        else if (EQUAL(pszName, "longitude") || EQUAL(pszName, "lon") || EQUAL(pszName, "long"))
            iLongitudeField = i;
    }

    if (iLatitudeField >= 0 && iLongitudeField >= 0)
    {
        iGeometryField = iLatitudeField;
        poFeatureDefn->SetGeomType( wkbPoint );
    }

    return TRUE;
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
        osSQL += ",'";
        osSQL += poFeatureDefn->GetFieldDefn(i)->GetNameRef();
        osSQL += "'";
    }
    osSQL += " FROM ";
    osSQL += osTableId;
    if (osWHERE.size())
    {
        osSQL += " ";
        osSQL += osWHERE;
    }
    osSQL += CPLSPrintf(" OFFSET %d LIMIT %d", nOffset, MAX_FEATURES_FETCH);

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

    pszLine = OGRGFTGotoNextLine(pszLine);
    if (pszLine == NULL)
    {
        CPLHTTPDestroyResult(psResult);
        bEOF = TRUE;
        return FALSE;
    }

    ParseCSVResponse(pszLine, aosRows);

    bEOF = aosRows.size() < MAX_FEATURES_FETCH;

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
        osSQL += ",'";
        osSQL += poFeatureDefn->GetFieldDefn(i)->GetNameRef();
        osSQL += "'";
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

    if (poDS->GetAuth().size() == 0)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Operation not available in unauthenticated mode");
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

void OGRGFTTableLayer::CreateTableIfNecessary()
{
    if (bHasTriedCreateTable || osTableId.size() != 0)
        return;

    bHasTriedCreateTable = TRUE;

    CPLString osSQL("CREATE TABLE '");
    osSQL += osTableName;
    osSQL += "' (";

    int i;

    for(i=0;i<poFeatureDefn->GetFieldCount();i++)
    {
        const char* pszName = poFeatureDefn->GetFieldDefn(i)->GetNameRef();
        if (EQUAL(pszName, "latitude") || EQUAL(pszName, "lat"))
            iLatitudeField = i;
        else if (EQUAL(pszName, "longitude") || EQUAL(pszName, "lon") || EQUAL(pszName, "long"))
            iLongitudeField = i;
    }

    if (iLatitudeField >= 0 && iLongitudeField >= 0)
        iGeometryField = iLatitudeField;
    if (iGeometryField < 0)
        iGeometryField = poFeatureDefn->GetFieldIndex("geometry");

    for(i=0;i<poFeatureDefn->GetFieldCount();i++)
    {
        if (i > 0)
            osSQL += ", ";
        osSQL += "'";
        osSQL += poFeatureDefn->GetFieldDefn(i)->GetNameRef();
        osSQL += "': ";

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
    if (iGeometryField < 0)
    {
        if (i > 0)
            osSQL += ", ";
        osSQL += "geometry: LOCATION";

        iGeometryField = poFeatureDefn->GetFieldCount();
        OGRFieldDefn oFieldDefn("geometry", OFTString);
        poFeatureDefn->AddFieldDefn(&oFieldDefn);
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
/*                            EscapeQuote()                             */
/************************************************************************/

static CPLString EscapeQuote(const char* pszStr)
{
    CPLString osRes;
    while(*pszStr)
    {
        if (*pszStr == '\'')
            osRes += "\\\'";
        else
            osRes += *pszStr;
        pszStr ++;
    }
    return osRes;
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
                    "Cannot add field to non-created table");
            return OGRERR_FAILURE;
        }
    }

    if (poDS->GetAuth().size() == 0)
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
        osCommand += poFeatureDefn->GetFieldDefn(iField)->GetNameRef();
    }
    osCommand += ") VALUES (";
    for(iField = 0; iField < nFieldCount; iField++)
    {
        if (iField > 0)
            osCommand += ", ";

        if (iGeometryField != iLatitudeField && iField == iGeometryField &&
            !poFeature->IsFieldSet( iField ))
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
            OGRFieldType eType = poFeatureDefn->GetFieldDefn(iField)->GetType();
            if (eType != OFTInteger && eType != OFTReal)
                osCommand += "'";
            const char* pszVal = poFeature->GetFieldAsString(iField);
            if (strchr(pszVal, '\''))
                osCommand += EscapeQuote(pszVal);
            else
                osCommand += pszVal;
            if (eType != OFTInteger && eType != OFTReal)
                osCommand += "'";
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

    CPLHTTPDestroyResult(psResult);

    return OGRERR_NONE;
}

/************************************************************************/
/*                         StartTransaction()                           */
/************************************************************************/

OGRErr OGRGFTTableLayer::StartTransaction()
{
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
                    "Cannot add field to non-created table");
            return OGRERR_FAILURE;
        }
    }

    if (poDS->GetAuth().size() == 0)
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
        osWHERE.Printf("WHERE ST_INTERSECTS('%s', RECTANGLE(LATLNG(%.12f, %.12f), LATLNG(%.12f, %.12f)))",
                       poFeatureDefn->GetFieldDefn(iGeometryField)->GetNameRef(),
                       sEnvelope.MinY - 1e-11, sEnvelope.MinX - 1e-11,
                       sEnvelope.MaxY + 1e-11, sEnvelope.MaxX + 1e-11);
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
