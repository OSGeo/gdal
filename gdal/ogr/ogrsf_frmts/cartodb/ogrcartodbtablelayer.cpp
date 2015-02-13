/******************************************************************************
 * $Id$
 *
 * Project:  CartoDB Translator
 * Purpose:  Implements OGRCARTODBTableLayer class.
 * Author:   Even Rouault, <even dot rouault at mines dash paris dot org>
 *
 ******************************************************************************
 * Copyright (c) 2013, Even Rouault <even dot rouault at mines-paris dot org>
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

#include "ogr_cartodb.h"
#include "ogr_p.h"
#include "ogr_pgdump.h"

CPL_CVSID("$Id$");

/************************************************************************/
/*                    OGRCARTODBEscapeIdentifier( )                     */
/************************************************************************/

CPLString OGRCARTODBEscapeIdentifier(const char* pszStr)
{
    CPLString osStr;

    osStr += "\"";

    char ch;
    for(int i=0; (ch = pszStr[i]) != '\0'; i++)
    {
        if (ch == '"')
            osStr.append(1, ch);
        osStr.append(1, ch);
    }

    osStr += "\"";

    return osStr;
}

/************************************************************************/
/*                    OGRCARTODBEscapeLiteral( )                        */
/************************************************************************/

CPLString OGRCARTODBEscapeLiteral(const char* pszStr)
{
    CPLString osStr;

    char ch;
    for(int i=0; (ch = pszStr[i]) != '\0'; i++)
    {
        if (ch == '\'')
            osStr.append(1, ch);
        osStr.append(1, ch);
    }

    return osStr;
}

/************************************************************************/
/*                        OGRCARTODBTableLayer()                        */
/************************************************************************/

OGRCARTODBTableLayer::OGRCARTODBTableLayer(OGRCARTODBDataSource* poDS,
                                           const char* pszName) :
                                           OGRCARTODBLayer(poDS)

{
    osName = pszName;
    SetDescription( osName );
    bInTransaction = FALSE;
    nNextFID = -1;
    bDifferedCreation = FALSE;
}

/************************************************************************/
/*                    ~OGRCARTODBTableLayer()                           */
/************************************************************************/

OGRCARTODBTableLayer::~OGRCARTODBTableLayer()

{
    if( bDifferedCreation ) RunDifferedCreationIfNecessary();
}

/************************************************************************/
/*                          GetLayerDefnInternal()                      */
/************************************************************************/

OGRFeatureDefn * OGRCARTODBTableLayer::GetLayerDefnInternal(CPL_UNUSED json_object* poObjIn)
{
    if( poFeatureDefn != NULL )
        return poFeatureDefn;

    osBaseSQL.Printf("SELECT * FROM %s",
                     OGRCARTODBEscapeIdentifier(osName).c_str());

    if( poDS->IsAuthenticatedConnection() )
    {
        CPLString osCommand;
        int bHasDefault = FALSE;

        osCommand.Printf(
                 "SELECT DISTINCT a.attname, t.typname, a.attlen,"
                 "       format_type(a.atttypid,a.atttypmod), a.attnum, a.attnotnull, a.atthasdef "
                 "FROM pg_class c, pg_attribute a, pg_type t, pg_namespace n "
                 "WHERE c.relname = '%s' "
                 "AND a.attnum > 0 AND a.attrelid = c.oid "
                 "AND a.atttypid = t.oid "
                 "AND c.relnamespace=n.oid "
                 "AND n.nspname= '%s' "
                 "ORDER BY a.attnum",
                 OGRCARTODBEscapeLiteral(osName).c_str(),
                 OGRCARTODBEscapeLiteral(poDS->GetCurrentSchema()).c_str());

        OGRLayer* poLyr = poDS->ExecuteSQL(osCommand, NULL, NULL);
        if( poLyr )
        {
            poFeatureDefn = new OGRFeatureDefn(osName);
            poFeatureDefn->Reference();
            poFeatureDefn->SetGeomType(wkbNone);

            OGRFeature* poFeat;
            while( (poFeat = poLyr->GetNextFeature()) != NULL )
            {
                const char* pszAttname = poFeat->GetFieldAsString("attname");
                const char* pszType = poFeat->GetFieldAsString("typname");
                int nWidth = poFeat->GetFieldAsInteger("attlen");
                const char* pszFormatType = poFeat->GetFieldAsString("format_type");
                int bNotNull = poFeat->GetFieldAsInteger("attnotnull");
                if( poFeat->GetFieldAsInteger("atthasdef") )
                    bHasDefault = TRUE;
                if( strcmp(pszAttname, "cartodb_id") == 0 )
                {
                    osFIDColName = pszAttname;
                }
                else if( strcmp(pszAttname, "created_at") == 0 ||
                         strcmp(pszAttname, "updated_at") == 0 ||
                         strcmp(pszAttname, "the_geom_webmercator") == 0)
                {
                    /* ignored */
                }
                else
                {
                    if( EQUAL(pszType,"geometry") )
                    {
                        OGRCartoDBGeomFieldDefn *poFieldDefn =
                            new OGRCartoDBGeomFieldDefn(pszAttname, wkbUnknown);
                        if( bNotNull )
                            poFieldDefn->SetNullable(FALSE);
                        poFeatureDefn->AddGeomFieldDefn(poFieldDefn, FALSE);
                        OGRSpatialReference* poSRS = GetSRS(pszAttname, &poFieldDefn->nSRID);
                        if( poSRS != NULL )
                        {
                            poFeatureDefn->GetGeomFieldDefn(
                                poFeatureDefn->GetGeomFieldCount() - 1)->SetSpatialRef(poSRS);
                            poSRS->Release();
                        }
                    }
                    else
                    {
                        OGRFieldDefn oField(pszAttname, OFTString);
                        if( bNotNull )
                            oField.SetNullable(FALSE);
                        OGRPGCommonLayerSetType(oField, pszType, pszFormatType, nWidth);

                        poFeatureDefn->AddFieldDefn( &oField );
                    }
                }
                delete poFeat;
            }

            poDS->ReleaseResultSet(poLyr);
        }

        if( bHasDefault )
        {
            osCommand.Printf(
                    "SELECT a.attname, pg_get_expr(def.adbin, c.oid) "
                    "FROM pg_attrdef def, pg_class c, pg_attribute a, pg_type t, pg_namespace n "
                    "WHERE c.relname = '%s' AND a.attnum > 0 AND a.attrelid = c.oid "
                    "AND a.atttypid = t.oid AND c.relnamespace=n.oid AND "
                    "def.adrelid = c.oid AND def.adnum = a.attnum "
                    "AND n.nspname= '%s' "
                    "ORDER BY a.attnum",
                    OGRCARTODBEscapeLiteral(osName).c_str(),
                    OGRCARTODBEscapeLiteral(poDS->GetCurrentSchema()).c_str());

            poLyr = poDS->ExecuteSQL(osCommand, NULL, NULL);
            if( poLyr != NULL )
            {
                OGRFeature* poFeat;
                while( (poFeat = poLyr->GetNextFeature()) != NULL )
                {
                    const char      *pszName = poFeat->GetFieldAsString(0);
                    const char      *pszDefault = poFeat->GetFieldAsString(1);
                    int nIdx = poFeatureDefn->GetFieldIndex(pszName);
                    if( nIdx >= 0 )
                    {
                        OGRFieldDefn* poFieldDefn = poFeatureDefn->GetFieldDefn(nIdx);
                        OGRPGCommonLayerNormalizeDefault(poFieldDefn, pszDefault);
                    }
                    delete poFeat;
                }

                poDS->ReleaseResultSet(poLyr);
            }
        }
    }

    if( poFeatureDefn == NULL )
    {
        EstablishLayerDefn(osName, NULL);
    }

    if( osFIDColName.size() > 0 )
    {
        osBaseSQL.Printf("SELECT * FROM %s ORDER BY %s ASC",
                         OGRCARTODBEscapeIdentifier(osName).c_str(),
                         OGRCARTODBEscapeIdentifier(osFIDColName).c_str());
    }

    return poFeatureDefn;
}

/************************************************************************/
/*                           GetNextRawFeature()                        */
/************************************************************************/

OGRFeature  *OGRCARTODBTableLayer::GetNextRawFeature()
{
    if( bDifferedCreation && RunDifferedCreationIfNecessary() != OGRERR_NONE )
        return NULL;
    return OGRCARTODBLayer::GetNextRawFeature();
}

/************************************************************************/
/*                         SetAttributeFilter()                         */
/************************************************************************/

OGRErr OGRCARTODBTableLayer::SetAttributeFilter( const char *pszQuery )

{
    GetLayerDefn();

    if( pszQuery == NULL )
        osQuery = "";
    else
    {
        osQuery = pszQuery;
    }

    BuildWhere();

    ResetReading();

    return OGRERR_NONE;
}

/************************************************************************/
/*                          SetSpatialFilter()                          */
/************************************************************************/

void OGRCARTODBTableLayer::SetSpatialFilter( int iGeomField, OGRGeometry * poGeomIn )

{
    if( iGeomField < 0 || iGeomField >= GetLayerDefn()->GetGeomFieldCount() ||
        GetLayerDefn()->GetGeomFieldDefn(iGeomField)->GetType() == wkbNone )
    {
        if( iGeomField != 0 )
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Invalid geometry field index : %d", iGeomField);
        }
        return;
    }
    m_iGeomFieldFilter = iGeomField;

    if( InstallFilter( poGeomIn ) )
    {
        BuildWhere();

        ResetReading();
    }
}

/************************************************************************/
/*                          StartTransaction()                          */
/************************************************************************/

OGRErr OGRCARTODBTableLayer::StartTransaction()

{
    bInTransaction = TRUE;
    osTransactionSQL = "";
    nNextFID = -1;
    return OGRERR_NONE;
}

/************************************************************************/
/*                         CommitTransaction()                          */
/************************************************************************/

OGRErr OGRCARTODBTableLayer::CommitTransaction()

{
    OGRErr eRet = OGRERR_NONE;

    if( bInTransaction && osTransactionSQL.size() > 0 )
    {
        eRet = OGRERR_FAILURE;
        osTransactionSQL = "BEGIN;" + osTransactionSQL + "COMMIT;";
        json_object* poObj = poDS->RunSQL(osTransactionSQL);
        if( poObj != NULL )
        {
            eRet = OGRERR_NONE;
            json_object_put(poObj);
        }
    }

    bInTransaction = FALSE;
    osTransactionSQL = "";
    nNextFID = -1;
    return eRet;
}

/************************************************************************/
/*                        RollbackTransaction()                         */
/************************************************************************/

OGRErr OGRCARTODBTableLayer::RollbackTransaction()

{
    bInTransaction = FALSE;
    osTransactionSQL = "";
    nNextFID = -1;
    return OGRERR_NONE;
}

/************************************************************************/
/*                            CreateField()                             */
/************************************************************************/

OGRErr OGRCARTODBTableLayer::CreateField( OGRFieldDefn *poFieldIn,
                                          CPL_UNUSED int bApproxOK )
{
    GetLayerDefn();

    if (!poDS->IsReadWrite())
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Operation not available in read-only mode");
        return OGRERR_FAILURE;
    }

/* -------------------------------------------------------------------- */
/*      Create the new field.                                           */
/* -------------------------------------------------------------------- */

    if( !bDifferedCreation )
    {
        CPLString osSQL;
        osSQL.Printf( "ALTER TABLE %s ADD COLUMN %s %s",
                    OGRCARTODBEscapeIdentifier(osName).c_str(),
                    OGRCARTODBEscapeIdentifier(poFieldIn->GetNameRef()).c_str(),
                    OGRPGCommonLayerGetType(*poFieldIn, FALSE, TRUE).c_str() );
        if( !poFieldIn->IsNullable() )
            osSQL += " NOT NULL";
        if( poFieldIn->GetDefault() != NULL && !poFieldIn->IsDefaultDriverSpecific() )
        {
            osSQL += " DEFAULT ";
            osSQL += OGRPGCommonLayerGetPGDefault(poFieldIn);
        }

        json_object* poObj = poDS->RunSQL(osSQL);
        if( poObj == NULL )
            return OGRERR_FAILURE;
        json_object_put(poObj);
    }

    poFeatureDefn->AddFieldDefn( poFieldIn );

    return OGRERR_NONE;
}

/************************************************************************/
/*                           ICreateFeature()                            */
/************************************************************************/

OGRErr OGRCARTODBTableLayer::ICreateFeature( OGRFeature *poFeature )

{
    int i;

    if( bDifferedCreation )
    {
        if( RunDifferedCreationIfNecessary() != OGRERR_NONE )
            return OGRERR_FAILURE;
        if( bInTransaction )
            nNextFID = 1;
    }

    GetLayerDefn();
    int bHasUserFieldMatchingFID = FALSE;
    if( osFIDColName.size() )
        bHasUserFieldMatchingFID = poFeatureDefn->GetFieldIndex(osFIDColName) >= 0;

    if (!poDS->IsReadWrite())
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Operation not available in read-only mode");
        return OGRERR_FAILURE;
    }

    CPLString osSQL;

    int bHasJustGotNextFID = FALSE;
    if( !bHasUserFieldMatchingFID && bInTransaction && nNextFID < 0 && osFIDColName.size() )
    {
        osSQL.Printf("SELECT nextval('%s') AS nextid",
                     OGRCARTODBEscapeLiteral(CPLSPrintf("%s_%s_seq", osName.c_str(), osFIDColName.c_str())).c_str());

        json_object* poObj = poDS->RunSQL(osSQL);
        json_object* poRowObj = OGRCARTODBGetSingleRow(poObj);
        if( poRowObj != NULL )
        {
            json_object* poID = json_object_object_get(poRowObj, "nextid");
            if( poID != NULL && json_object_get_type(poID) == json_type_int )
            {
                nNextFID = json_object_get_int64(poID);
                bHasJustGotNextFID = TRUE;
            }
        }

        if( poObj != NULL )
            json_object_put(poObj);
    }

    osSQL.Printf("INSERT INTO %s ", OGRCARTODBEscapeIdentifier(osName).c_str());
    int bMustComma = FALSE;
    for(i = 0; i < poFeatureDefn->GetFieldCount(); i++)
    {
        if( !poFeature->IsFieldSet(i) )
            continue;

        if( bMustComma )
            osSQL += ", ";
        else
        {
            osSQL += "(";
            bMustComma = TRUE;
        }

        osSQL += OGRCARTODBEscapeIdentifier(poFeatureDefn->GetFieldDefn(i)->GetNameRef());
    }

    for(i = 0; i < poFeatureDefn->GetGeomFieldCount(); i++)
    {
        if( poFeature->GetGeomFieldRef(i) == NULL )
            continue;

        if( bMustComma )
            osSQL += ", ";
        else
        {
            osSQL += "(";
            bMustComma = TRUE;
        }

        osSQL += OGRCARTODBEscapeIdentifier(poFeatureDefn->GetGeomFieldDefn(i)->GetNameRef());
    }
    
    if( !bHasUserFieldMatchingFID &&
        osFIDColName.size() && (poFeature->GetFID() != OGRNullFID || nNextFID >= 0) )
    {
        if( bMustComma )
            osSQL += ", ";
        else
        {
            osSQL += "(";
            bMustComma = TRUE;
        }

        osSQL += OGRCARTODBEscapeIdentifier(osFIDColName);
    }

    if( !bMustComma )
        osSQL += " DEFAULT VALUES";
    else
    {
        osSQL += ") VALUES (";
        
        bMustComma = FALSE;
        for(i = 0; i < poFeatureDefn->GetFieldCount(); i++)
        {
            if( !poFeature->IsFieldSet(i) )
                continue;
        
            if( bMustComma )
                osSQL += ", ";
            else
                bMustComma = TRUE;
            
            OGRFieldType eType = poFeatureDefn->GetFieldDefn(i)->GetType();
            if( eType == OFTString || eType == OFTDateTime || eType == OFTDate || eType == OFTTime )
            {
                osSQL += "'";
                osSQL += OGRCARTODBEscapeLiteral(poFeature->GetFieldAsString(i));
                osSQL += "'";
            }
            else if( (eType == OFTInteger || eType == OFTInteger64) &&
                     poFeatureDefn->GetFieldDefn(i)->GetSubType() == OFSTBoolean )
            {
                osSQL += poFeature->GetFieldAsInteger(i) ? "'t'" : "'f'";
            }
            else
                osSQL += poFeature->GetFieldAsString(i);
        }
        
        for(i = 0; i < poFeatureDefn->GetGeomFieldCount(); i++)
        {
            OGRGeometry* poGeom = poFeature->GetGeomFieldRef(i);
            if( poGeom == NULL )
                continue;
        
            if( bMustComma )
                osSQL += ", ";
            else
                bMustComma = TRUE;

            OGRCartoDBGeomFieldDefn* poGeomFieldDefn =
                (OGRCartoDBGeomFieldDefn *)poFeatureDefn->GetGeomFieldDefn(i);
            int nSRID = poGeomFieldDefn->nSRID;
            if( nSRID == 0 )
                nSRID = 4326;
            char* pszEWKB;
            if( wkbFlatten(poGeom->getGeometryType()) == wkbPolygon &&
                wkbFlatten(GetGeomType()) == wkbMultiPolygon )
            {
                OGRMultiPolygon* poNewGeom = new OGRMultiPolygon();
                poNewGeom->addGeometry(poGeom);
                pszEWKB = OGRGeometryToHexEWKB(poNewGeom, nSRID, FALSE);
                delete poNewGeom;
            }
            else
                pszEWKB = OGRGeometryToHexEWKB(poGeom, nSRID, FALSE);
            osSQL += "'";
            osSQL += pszEWKB;
            osSQL += "'";
            CPLFree(pszEWKB);
        }

        if( !bHasUserFieldMatchingFID )
        {
            if( osFIDColName.size() && nNextFID >= 0 )
            {
                if( bMustComma )
                    osSQL += ", ";
                else
                    bMustComma = TRUE;

                if( bHasJustGotNextFID )
                {
                    osSQL += CPLSPrintf(CPL_FRMT_GIB, nNextFID);
                }
                else
                {
                    osSQL += CPLSPrintf("nextval('%s')",
                            OGRCARTODBEscapeLiteral(CPLSPrintf("%s_%s_seq", osName.c_str(), osFIDColName.c_str())).c_str());
                }
                poFeature->SetFID(nNextFID);
                nNextFID ++;
            }
            else if( osFIDColName.size() && poFeature->GetFID() != OGRNullFID )
            {
                if( bMustComma )
                    osSQL += ", ";
                else
                    bMustComma = TRUE;

                osSQL += CPLSPrintf(CPL_FRMT_GIB, poFeature->GetFID());
            }
        }

        osSQL += ")";
    }

    if( bInTransaction )
    {
        osTransactionSQL += osSQL;
        osTransactionSQL += ";";
        return OGRERR_NONE;
    }
    
    if( osFIDColName.size() )
    {
        osSQL += " RETURNING ";
        osSQL += OGRCARTODBEscapeIdentifier(osFIDColName);

        json_object* poObj = poDS->RunSQL(osSQL);
        json_object* poRowObj = OGRCARTODBGetSingleRow(poObj);
        if( poRowObj == NULL )
        {
            if( poObj != NULL )
                json_object_put(poObj);
            return OGRERR_FAILURE;
        }

        json_object* poID = json_object_object_get(poRowObj, osFIDColName);
        if( poID != NULL && json_object_get_type(poID) == json_type_int )
        {
            poFeature->SetFID(json_object_get_int64(poID));
        }

        if( poObj != NULL )
            json_object_put(poObj);

        return OGRERR_NONE;
    }
    else
    {
        OGRErr eRet = OGRERR_FAILURE;
        json_object* poObj = poDS->RunSQL(osSQL);
        if( poObj != NULL )
        {
            json_object* poTotalRows = json_object_object_get(poObj, "total_rows");
            if( poTotalRows != NULL && json_object_get_type(poTotalRows) == json_type_int )
            {
                int nTotalRows = json_object_get_int(poTotalRows);
                if( nTotalRows == 1 )
                {
                    eRet = OGRERR_NONE;
                }
            }
            json_object_put(poObj);
        }

        return eRet;
    }
}

/************************************************************************/
/*                            ISetFeature()                              */
/************************************************************************/

OGRErr OGRCARTODBTableLayer::ISetFeature( OGRFeature *poFeature )

{
    int i;

    if( bDifferedCreation && RunDifferedCreationIfNecessary() != OGRERR_NONE )
        return OGRERR_FAILURE;

    GetLayerDefn();

    if (!poDS->IsReadWrite())
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Operation not available in read-only mode");
        return OGRERR_FAILURE;
    }

    if (poFeature->GetFID() == OGRNullFID)
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "FID required on features given to SetFeature()." );
        return OGRERR_FAILURE;
    }
    
    CPLString osSQL;
    osSQL.Printf("UPDATE %s SET ", OGRCARTODBEscapeIdentifier(osName).c_str());
    int bMustComma = FALSE;
    for(i = 0; i < poFeatureDefn->GetFieldCount(); i++)
    {
        if( bMustComma )
            osSQL += ", ";
        else
            bMustComma = TRUE;

        osSQL += OGRCARTODBEscapeIdentifier(poFeatureDefn->GetFieldDefn(i)->GetNameRef());
        osSQL += " = ";

        if( !poFeature->IsFieldSet(i) )
        {
            osSQL += "NULL";
        }
        else
        {
            OGRFieldType eType = poFeatureDefn->GetFieldDefn(i)->GetType();
            if( eType == OFTString || eType == OFTDateTime || eType == OFTDate || eType == OFTTime )
            {
                osSQL += "'";
                osSQL += OGRCARTODBEscapeLiteral(poFeature->GetFieldAsString(i));
                osSQL += "'";
            }
            else if( (eType == OFTInteger || eType == OFTInteger64) &&
                poFeatureDefn->GetFieldDefn(i)->GetSubType() == OFSTBoolean )
            {
                osSQL += poFeature->GetFieldAsInteger(i) ? "'t'" : "'f'";
            }
            else
                osSQL += poFeature->GetFieldAsString(i);
        }
    }

    for(i = 0; i < poFeatureDefn->GetGeomFieldCount(); i++)
    {
        if( bMustComma )
            osSQL += ", ";
        else
            bMustComma = TRUE;

        osSQL += OGRCARTODBEscapeIdentifier(poFeatureDefn->GetGeomFieldDefn(i)->GetNameRef());
        osSQL += " = ";

        OGRGeometry* poGeom = poFeature->GetGeomFieldRef(i);
        if( poGeom == NULL )
        {
            osSQL += "NULL";
        }
        else
        {
            OGRCartoDBGeomFieldDefn* poGeomFieldDefn =
                (OGRCartoDBGeomFieldDefn *)poFeatureDefn->GetGeomFieldDefn(i);
            int nSRID = poGeomFieldDefn->nSRID;
            if( nSRID == 0 )
                nSRID = 4326;
            char* pszEWKB = OGRGeometryToHexEWKB(poGeom, nSRID, FALSE);
            osSQL += "'";
            osSQL += pszEWKB;
            osSQL += "'";
            CPLFree(pszEWKB);
        }
    }

    osSQL += CPLSPrintf(" WHERE %s = " CPL_FRMT_GIB,
                    OGRCARTODBEscapeIdentifier(osFIDColName).c_str(),
                    poFeature->GetFID());
    
    if( bInTransaction )
    {
        osTransactionSQL += osSQL;
        osTransactionSQL += ";";
        return OGRERR_NONE;
    }

    OGRErr eRet = OGRERR_FAILURE;
    json_object* poObj = poDS->RunSQL(osSQL);
    if( poObj != NULL )
    {
        json_object* poTotalRows = json_object_object_get(poObj, "total_rows");
        if( poTotalRows != NULL && json_object_get_type(poTotalRows) == json_type_int )
        {
            int nTotalRows = json_object_get_int(poTotalRows);
            if( nTotalRows == 1 )
            {
                eRet = OGRERR_NONE;
            }
        }
        json_object_put(poObj);
    }

    return eRet;
}

/************************************************************************/
/*                          DeleteFeature()                             */
/************************************************************************/

OGRErr OGRCARTODBTableLayer::DeleteFeature( GIntBig nFID )

{

    if( bDifferedCreation && RunDifferedCreationIfNecessary() != OGRERR_NONE )
        return OGRERR_FAILURE;

    GetLayerDefn();

    if (!poDS->IsReadWrite())
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Operation not available in read-only mode");
        return OGRERR_FAILURE;
    }
    
    if( osFIDColName.size() == 0 )
        return OGRERR_FAILURE;
    
    CPLString osSQL;
    osSQL.Printf("DELETE FROM %s WHERE %s = " CPL_FRMT_GIB,
                    OGRCARTODBEscapeIdentifier(osName).c_str(),
                    OGRCARTODBEscapeIdentifier(osFIDColName).c_str(),
                    nFID);
    
    if( bInTransaction )
    {
        osTransactionSQL += osSQL;
        osTransactionSQL += ";";
        return OGRERR_NONE;
    }
    
    OGRErr eRet = OGRERR_FAILURE;
    json_object* poObj = poDS->RunSQL(osSQL);
    if( poObj != NULL )
    {
        json_object* poTotalRows = json_object_object_get(poObj, "total_rows");
        if( poTotalRows != NULL && json_object_get_type(poTotalRows) == json_type_int )
        {
            int nTotalRows = json_object_get_int(poTotalRows);
            if( nTotalRows == 1 )
            {
                eRet = OGRERR_NONE;
            }
        }
        json_object_put(poObj);
    }

    return eRet;
}

/************************************************************************/
/*                             GetSRS_SQL()                             */
/************************************************************************/

CPLString OGRCARTODBTableLayer::GetSRS_SQL(const char* pszGeomCol)
{
    CPLString osSQL;

    if( poDS->IsAuthenticatedConnection() )
    {
        /* Find_SRID needs access to geometry_columns table, whhose access */
        /* is restricted to authenticated connections. */
        osSQL.Printf("SELECT srid, srtext FROM spatial_ref_sys WHERE srid IN "
                    "(SELECT Find_SRID('%s', '%s', '%s'))",
                    OGRCARTODBEscapeLiteral(poDS->GetCurrentSchema()).c_str(),
                    OGRCARTODBEscapeLiteral(osName).c_str(),
                    OGRCARTODBEscapeLiteral(pszGeomCol).c_str());
    }
    else
    {
        /* Assuming that the SRID of the first non-NULL geometry applies */
        /* to geometries of all rows. */
        osSQL.Printf("SELECT srid, srtext FROM spatial_ref_sys WHERE srid IN "
                    "(SELECT ST_SRID(%s) FROM %s WHERE %s IS NOT NULL LIMIT 1)",
                    OGRCARTODBEscapeIdentifier(pszGeomCol).c_str(),
                    OGRCARTODBEscapeIdentifier(osName).c_str(),
                    OGRCARTODBEscapeIdentifier(pszGeomCol).c_str());
    }

    return osSQL;
}

/************************************************************************/
/*                             BuildWhere()                             */
/*                                                                      */
/*      Build the WHERE statement appropriate to the current set of     */
/*      criteria (spatial and attribute queries).                       */
/************************************************************************/

void OGRCARTODBTableLayer::BuildWhere()

{
    osWHERE = "";

    if( m_poFilterGeom != NULL &&
        m_iGeomFieldFilter >= 0 &&
        m_iGeomFieldFilter < poFeatureDefn->GetGeomFieldCount() )
    {
        OGREnvelope  sEnvelope;

        m_poFilterGeom->getEnvelope( &sEnvelope );

        CPLString osGeomColumn(poFeatureDefn->GetGeomFieldDefn(m_iGeomFieldFilter)->GetNameRef());

        char szBox3D_1[128];
        char szBox3D_2[128];
        char* pszComma;

        CPLsnprintf(szBox3D_1, sizeof(szBox3D_1), "%.18g %.18g", sEnvelope.MinX, sEnvelope.MinY);
        while((pszComma = strchr(szBox3D_1, ',')) != NULL)
            *pszComma = '.';
        CPLsnprintf(szBox3D_2, sizeof(szBox3D_2), "%.18g %.18g", sEnvelope.MaxX, sEnvelope.MaxY);
        while((pszComma = strchr(szBox3D_2, ',')) != NULL)
            *pszComma = '.';
        osWHERE.Printf("WHERE %s && 'BOX3D(%s, %s)'::box3d",
                       OGRCARTODBEscapeIdentifier(osGeomColumn).c_str(),
                       szBox3D_1, szBox3D_2 );
    }

    if( strlen(osQuery) > 0 )
    {
        if( strlen(osWHERE) == 0 )
            osWHERE = "WHERE ";
        else
            osWHERE += " AND ";
        osWHERE += osQuery;
    }

    osBaseSQL.Printf("SELECT * FROM %s",
                     OGRCARTODBEscapeIdentifier(osName).c_str());
    if( osWHERE.size() )
    {
        osBaseSQL += " ";
        osBaseSQL += osWHERE;
    }
    if( osFIDColName.size() > 0 )
    {
        osBaseSQL += CPLSPrintf(" ORDER BY %s ASC",
                         OGRCARTODBEscapeIdentifier(osFIDColName).c_str());
    }
}

/************************************************************************/
/*                              GetFeature()                            */
/************************************************************************/

OGRFeature* OGRCARTODBTableLayer::GetFeature( GIntBig nFeatureId )
{

    if( bDifferedCreation && RunDifferedCreationIfNecessary() != OGRERR_NONE )
        return NULL;

    GetLayerDefn();
    
    if( osFIDColName.size() == 0 )
        return OGRCARTODBLayer::GetFeature(nFeatureId);

    CPLString osSQL(CPLSPrintf("SELECT * FROM %s WHERE %s = " CPL_FRMT_GIB,
                               OGRCARTODBEscapeIdentifier(osName).c_str(),
                               OGRCARTODBEscapeIdentifier(osFIDColName).c_str(),
                               nFeatureId));

    json_object* poObj = poDS->RunSQL(osSQL);
    json_object* poRowObj = OGRCARTODBGetSingleRow(poObj);
    if( poRowObj == NULL )
    {
        if( poObj != NULL )
            json_object_put(poObj);
        return OGRCARTODBLayer::GetFeature(nFeatureId);
    }

    OGRFeature* poFeature = BuildFeature(poRowObj);
    json_object_put(poObj);

    return poFeature;
}

/************************************************************************/
/*                          GetFeatureCount()                           */
/************************************************************************/

GIntBig OGRCARTODBTableLayer::GetFeatureCount(int bForce)
{

    if( bDifferedCreation && RunDifferedCreationIfNecessary() != OGRERR_NONE )
        return 0;

    GetLayerDefn();

    CPLString osSQL(CPLSPrintf("SELECT COUNT(*) FROM %s",
                               OGRCARTODBEscapeIdentifier(osName).c_str()));
    if( osWHERE.size() )
    {
        osSQL += " ";
        osSQL += osWHERE;
    }

    json_object* poObj = poDS->RunSQL(osSQL);
    json_object* poRowObj = OGRCARTODBGetSingleRow(poObj);
    if( poRowObj == NULL )
    {
        if( poObj != NULL )
            json_object_put(poObj);
        return OGRCARTODBLayer::GetFeatureCount(bForce);
    }

    json_object* poCount = json_object_object_get(poRowObj, "count");
    if( poCount == NULL || json_object_get_type(poCount) != json_type_int )
    {
        json_object_put(poObj);
        return OGRCARTODBLayer::GetFeatureCount(bForce);
    }

    GIntBig nRet = (GIntBig)json_object_get_int64(poCount);

    json_object_put(poObj);

    return nRet;
}

/************************************************************************/
/*                             GetExtent()                              */
/*                                                                      */
/*      For PostGIS use internal Extend(geometry) function              */
/*      in other cases we use standard OGRLayer::GetExtent()            */
/************************************************************************/

OGRErr OGRCARTODBTableLayer::GetExtent( int iGeomField, OGREnvelope *psExtent, int bForce )
{
    CPLString   osSQL;

    if( bDifferedCreation && RunDifferedCreationIfNecessary() != OGRERR_NONE )
        return OGRERR_FAILURE;

    if( iGeomField < 0 || iGeomField >= GetLayerDefn()->GetGeomFieldCount() ||
        GetLayerDefn()->GetGeomFieldDefn(iGeomField)->GetType() == wkbNone )
    {
        if( iGeomField != 0 )
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Invalid geometry field index : %d", iGeomField);
        }
        return OGRERR_FAILURE;
    }

    OGRGeomFieldDefn* poGeomFieldDefn =
        poFeatureDefn->GetGeomFieldDefn(iGeomField);

    /* Do not take the spatial filter into account */
    osSQL.Printf( "SELECT ST_Extent(%s) FROM %s",
                  OGRCARTODBEscapeIdentifier(poGeomFieldDefn->GetNameRef()).c_str(),
                  OGRCARTODBEscapeIdentifier(osName).c_str());

    json_object* poObj = poDS->RunSQL(osSQL);
    json_object* poRowObj = OGRCARTODBGetSingleRow(poObj);
    if( poRowObj != NULL )
    {
        json_object* poExtent = json_object_object_get(poRowObj, "st_extent");
        if( poExtent != NULL && json_object_get_type(poExtent) == json_type_string )
        {
            const char* pszBox = json_object_get_string(poExtent);
            const char * ptr, *ptrEndParenthesis;
            char szVals[64*6+6];

            ptr = strchr(pszBox, '(');
            if (ptr)
                ptr ++;
            if (ptr == NULL ||
                (ptrEndParenthesis = strchr(ptr, ')')) == NULL ||
                ptrEndParenthesis - ptr > (int)(sizeof(szVals) - 1))
            {
                CPLError( CE_Failure, CPLE_IllegalArg,
                            "Bad extent representation: '%s'", pszBox);

                json_object_put(poObj);
                return OGRERR_FAILURE;
            }

            strncpy(szVals,ptr,ptrEndParenthesis - ptr);
            szVals[ptrEndParenthesis - ptr] = '\0';

            char ** papszTokens = CSLTokenizeString2(szVals," ,",CSLT_HONOURSTRINGS);
            int nTokenCnt = 4;

            if ( CSLCount(papszTokens) != nTokenCnt )
            {
                CPLError( CE_Failure, CPLE_IllegalArg,
                            "Bad extent representation: '%s'", pszBox);
                CSLDestroy(papszTokens);

                json_object_put(poObj);
                return OGRERR_FAILURE;
            }

            // Take X,Y coords
            // For PostGis ver >= 1.0.0 -> Tokens: X1 Y1 X2 Y2 (nTokenCnt = 4)
            // For PostGIS ver < 1.0.0 -> Tokens: X1 Y1 Z1 X2 Y2 Z2 (nTokenCnt = 6)
            // =>   X2 index calculated as nTokenCnt/2
            //      Y2 index caluclated as nTokenCnt/2+1
            
            psExtent->MinX = CPLAtof( papszTokens[0] );
            psExtent->MinY = CPLAtof( papszTokens[1] );
            psExtent->MaxX = CPLAtof( papszTokens[nTokenCnt/2] );
            psExtent->MaxY = CPLAtof( papszTokens[nTokenCnt/2+1] );

            CSLDestroy(papszTokens);

            json_object_put(poObj);
            return OGRERR_NONE;
        }
    }

    if( poObj != NULL )
        json_object_put(poObj);

    if( iGeomField == 0 )
        return OGRLayer::GetExtent( psExtent, bForce );
    else
        return OGRLayer::GetExtent( iGeomField, psExtent, bForce );
}

/************************************************************************/
/*                           TestCapability()                           */
/************************************************************************/

int OGRCARTODBTableLayer::TestCapability( const char * pszCap )

{
    if( EQUAL(pszCap, OLCFastFeatureCount) )
        return TRUE;
    if( EQUAL(pszCap, OLCFastGetExtent) )
        return TRUE;
    if( EQUAL(pszCap, OLCRandomRead) )
    {
        GetLayerDefn();
        return osFIDColName.size() != 0;
    }

    if( EQUAL(pszCap,OLCSequentialWrite)
     || EQUAL(pszCap,OLCRandomWrite)
     || EQUAL(pszCap,OLCDeleteFeature)
     || EQUAL(pszCap,OLCCreateField) )
    {
        return poDS->IsReadWrite();
    }

    if( EQUAL(pszCap,OLCTransactions) )
        return TRUE;

    return OGRCARTODBLayer::TestCapability(pszCap);
}

/************************************************************************/
/*                        SetDifferedCreation()                         */
/************************************************************************/

void OGRCARTODBTableLayer::SetDifferedCreation(OGRwkbGeometryType eGType,
                                               OGRSpatialReference* poSRS,
                                               int bGeomNullable)
{
    bDifferedCreation = TRUE;
    CPLAssert(poFeatureDefn == NULL);
    poFeatureDefn = new OGRFeatureDefn(osName);
    poFeatureDefn->Reference();
    poFeatureDefn->SetGeomType(wkbNone);
    if( eGType == wkbPolygon )
        eGType = wkbMultiPolygon;
    else if( eGType == wkbPolygon25D )
        eGType = wkbMultiPolygon25D;
    if( eGType != wkbNone )
    {
        OGRCartoDBGeomFieldDefn *poFieldDefn =
            new OGRCartoDBGeomFieldDefn("the_geom", eGType);
        poFieldDefn->SetNullable(bGeomNullable);
        poFeatureDefn->AddGeomFieldDefn(poFieldDefn, FALSE);
        if( poSRS != NULL )
        {
            poFieldDefn->nSRID = poDS->FetchSRSId( poSRS );
            poFeatureDefn->GetGeomFieldDefn(
                poFeatureDefn->GetGeomFieldCount() - 1)->SetSpatialRef(poSRS);
        }
    }
    osFIDColName = "cartodb_id";
    osBaseSQL.Printf("SELECT * FROM %s ORDER BY %s ASC",
                         OGRCARTODBEscapeIdentifier(osName).c_str(),
                         OGRCARTODBEscapeIdentifier(osFIDColName).c_str());
}

/************************************************************************/
/*                      RunDifferedCreationIfNecessary()                */
/************************************************************************/

OGRErr OGRCARTODBTableLayer::RunDifferedCreationIfNecessary()
{
    if( !bDifferedCreation )
        return OGRERR_NONE;
    bDifferedCreation = FALSE;

    CPLString osSQL;
    osSQL.Printf("CREATE TABLE %s ( %s SERIAL,",
                 OGRCARTODBEscapeIdentifier(osName).c_str(),
                 osFIDColName.c_str());

    int nSRID = 0;
    OGRwkbGeometryType eGType = GetGeomType();
    if( eGType != wkbNone )
    {
        CPLString osGeomType = OGRToOGCGeomType(eGType);
        if( wkbHasZ(eGType) )
            osGeomType += "Z";

        OGRCartoDBGeomFieldDefn *poFieldDefn =
            (OGRCartoDBGeomFieldDefn *)poFeatureDefn->GetGeomFieldDefn(0);
        nSRID = poFieldDefn->nSRID;

        osSQL += CPLSPrintf("%s GEOMETRY(%s, %d)%s, %s GEOMETRY(%s, %d),",
                 "the_geom",
                 osGeomType.c_str(),
                 nSRID,
                 (!poFieldDefn->IsNullable()) ? " NOT NULL" : "",
                 "the_geom_webmercator",
                 osGeomType.c_str(),
                 3857);
    }

    for( int i = 0; i < poFeatureDefn->GetFieldCount(); i++ )
    {
        OGRFieldDefn* poFieldDefn = poFeatureDefn->GetFieldDefn(i);
        if( strcmp(poFieldDefn->GetNameRef(), osFIDColName) != 0 )
        {
            osSQL += OGRCARTODBEscapeIdentifier(poFieldDefn->GetNameRef());
            osSQL += " ";
            osSQL += OGRPGCommonLayerGetType(*poFieldDefn, FALSE, TRUE);
            if( !poFieldDefn->IsNullable() )
                osSQL += " NOT NULL";
            if( poFieldDefn->GetDefault() != NULL && !poFieldDefn->IsDefaultDriverSpecific() )
            {
                osSQL += " DEFAULT ";
                osSQL += poFieldDefn->GetDefault();
            }
            osSQL += ",";
        }
    }

    osSQL += CPLSPrintf("PRIMARY KEY (%s) )", osFIDColName.c_str());
    
    CPLString osSeqName(OGRCARTODBEscapeIdentifier(CPLSPrintf("%s_%s_seq",
                                osName.c_str(), osFIDColName.c_str())));

    osSQL += ";";
    osSQL += CPLSPrintf("DROP SEQUENCE IF EXISTS %s CASCADE", osSeqName.c_str());
    osSQL += ";";
    osSQL += CPLSPrintf("CREATE SEQUENCE %s START 1", osSeqName.c_str());
    osSQL += ";";
    osSQL += CPLSPrintf("ALTER TABLE %s ALTER COLUMN %s SET DEFAULT nextval('%s')",
                        OGRCARTODBEscapeIdentifier(osName).c_str(),
                        osFIDColName.c_str(), osSeqName.c_str());

    json_object* poObj = poDS->RunSQL(osSQL);
    if( poObj == NULL )
        return OGRERR_FAILURE;
    json_object_put(poObj);

    if( nSRID != 4326 )
    {
        if( eGType != wkbNone )
        {
            CPLError(CE_Warning, CPLE_AppDefined,
                    "Cannot register table in dashboard with "
                    "cdb_cartodbfytable() since its SRS is not EPSG:4326");
        }
    }
    else
    {
        if( poDS->GetCurrentSchema() == "public" )
            osSQL.Printf("SELECT cdb_cartodbfytable('%s')",
                                OGRCARTODBEscapeLiteral(osName).c_str());
        else
            osSQL.Printf("SELECT cdb_cartodbfytable('%s', '%s')",
                                OGRCARTODBEscapeLiteral(poDS->GetCurrentSchema()).c_str(),
                                OGRCARTODBEscapeLiteral(osName).c_str());

        poObj = poDS->RunSQL(osSQL);
        if( poObj == NULL )
            return OGRERR_FAILURE;
        json_object_put(poObj);
    }

    return OGRERR_NONE;
}
