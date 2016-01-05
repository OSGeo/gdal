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

OGRCARTODBTableLayer::OGRCARTODBTableLayer(OGRCARTODBDataSource* poDSIn,
                                           const char* pszName) :
                                           OGRCARTODBLayer(poDSIn)

{
    osName = pszName;
    SetDescription( osName );
    bLaunderColumnNames = TRUE;
    bInDeferedInsert = poDS->DoBatchInsert();
    eDeferedInsertState = INSERT_UNINIT;
    nNextFID = -1;
    bDeferedCreation = FALSE;
    bCartoDBify = FALSE;
    nMaxChunkSize = atoi(CPLGetConfigOption("CARTODB_MAX_CHUNK_SIZE", "15")) * 1024 * 1024;
}

/************************************************************************/
/*                    ~OGRCARTODBTableLayer()                           */
/************************************************************************/

OGRCARTODBTableLayer::~OGRCARTODBTableLayer()

{
    if( bDeferedCreation ) RunDeferedCreationIfNecessary();
    CPL_IGNORE_RET_VAL(FlushDeferedInsert());
    RunDeferedCartoDBfy();
}

/************************************************************************/
/*                          GetLayerDefnInternal()                      */
/************************************************************************/

OGRFeatureDefn * OGRCARTODBTableLayer::GetLayerDefnInternal(CPL_UNUSED json_object* poObjIn)
{
    if( poFeatureDefn != NULL )
        return poFeatureDefn;

    CPLString osCommand;
    if( poDS->IsAuthenticatedConnection() )
    {
        // Get everything !
        osCommand.Printf(
                 "SELECT a.attname, t.typname, a.attlen, "
                        "format_type(a.atttypid,a.atttypmod), "
                        "a.attnum, "
                        "a.attnotnull, "
                        "i.indisprimary, "
                        "pg_get_expr(def.adbin, c.oid) AS defaultexpr, "
                        "postgis_typmod_dims(a.atttypmod) dim, "
                        "postgis_typmod_srid(a.atttypmod) srid, "
                        "postgis_typmod_type(a.atttypmod)::text geomtyp, "
                        "srtext "
                 "FROM pg_class c "
                 "JOIN pg_attribute a ON a.attnum > 0 AND "
                                        "a.attrelid = c.oid AND c.relname = '%s' "
                 "JOIN pg_type t ON a.atttypid = t.oid "
                 "JOIN pg_namespace n ON c.relnamespace=n.oid AND n.nspname= '%s' "
                 "LEFT JOIN pg_index i ON c.oid = i.indrelid AND "
                                         "i.indisprimary = 't' AND a.attnum = ANY(i.indkey) "
                 "LEFT JOIN pg_attrdef def ON def.adrelid = c.oid AND "
                                              "def.adnum = a.attnum "
                 "LEFT JOIN spatial_ref_sys srs ON srs.srid = postgis_typmod_srid(a.atttypmod) "
                 "ORDER BY a.attnum",
                 OGRCARTODBEscapeLiteral(osName).c_str(),
                 OGRCARTODBEscapeLiteral(poDS->GetCurrentSchema()).c_str());
    }
    else if( poDS->HasOGRMetadataFunction() != FALSE )
    {
        osCommand.Printf( "SELECT * FROM ogr_table_metadata('%s', '%s')",
                          OGRCARTODBEscapeLiteral(poDS->GetCurrentSchema()).c_str(),
                          OGRCARTODBEscapeLiteral(osName).c_str() );
    }

    if( osCommand.size() )
    {
        if( !poDS->IsAuthenticatedConnection() && poDS->HasOGRMetadataFunction() < 0 )
            CPLPushErrorHandler(CPLQuietErrorHandler);
        OGRLayer* poLyr = poDS->ExecuteSQLInternal(osCommand);
        if( !poDS->IsAuthenticatedConnection() && poDS->HasOGRMetadataFunction() < 0 )
        {
            CPLPopErrorHandler();
            if( poLyr == NULL )
            {
                CPLDebug("CARTODB", "ogr_table_metadata(text, text) not available");
                CPLErrorReset();
            }
            else if( poLyr->GetLayerDefn()->GetFieldCount() != 12 )
            {
                CPLDebug("CARTODB", "ogr_table_metadata(text, text) has unexpected column count");
                poDS->ReleaseResultSet(poLyr);
                poLyr = NULL;
            }
            poDS->SetOGRMetadataFunction(poLyr != NULL);
        }
        if( poLyr )
        {
            OGRFeature* poFeat;
            while( (poFeat = poLyr->GetNextFeature()) != NULL )
            {
                if( poFeatureDefn == NULL )
                {
                    // We could do that outside of the while() loop, but
                    // by doing that here, we are somewhat robust to
                    // ogr_table_metadata() returning suddenly an empty result set
                    // for example if CDB_UserTables() no longer works
                    poFeatureDefn = new OGRFeatureDefn(osName);
                    poFeatureDefn->Reference();
                    poFeatureDefn->SetGeomType(wkbNone);
                }

                const char* pszAttname = poFeat->GetFieldAsString("attname");
                const char* pszType = poFeat->GetFieldAsString("typname");
                int nWidth = poFeat->GetFieldAsInteger("attlen");
                const char* pszFormatType = poFeat->GetFieldAsString("format_type");
                int bNotNull = poFeat->GetFieldAsInteger("attnotnull");
                int bIsPrimary = poFeat->GetFieldAsInteger("indisprimary");
                int iDefaultExpr = poLyr->GetLayerDefn()->GetFieldIndex("defaultexpr");
                const char* pszDefault = (iDefaultExpr >= 0 && poFeat->IsFieldSet(iDefaultExpr)) ?
                            poFeat->GetFieldAsString(iDefaultExpr) : NULL;

                if( bIsPrimary &&
                    (EQUAL(pszType, "int2") ||
                     EQUAL(pszType, "int4") ||
                     EQUAL(pszType, "int8") ||
                     EQUAL(pszType, "serial") ||
                     EQUAL(pszType, "bigserial")) )
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
                        int nDim = poFeat->GetFieldAsInteger("dim");
                        int nSRID = poFeat->GetFieldAsInteger("srid");
                        const char* pszGeomType = poFeat->GetFieldAsString("geomtyp");
                        const char* pszSRText = (poFeat->IsFieldSet(
                            poLyr->GetLayerDefn()->GetFieldIndex("srtext"))) ?
                                    poFeat->GetFieldAsString("srtext") : NULL;
                        OGRwkbGeometryType eType = OGRFromOGCGeomType(pszGeomType);
                        if( nDim == 3 )
                            eType = wkbSetZ(eType);
                        OGRCartoDBGeomFieldDefn *poFieldDefn =
                            new OGRCartoDBGeomFieldDefn(pszAttname, eType);
                        if( bNotNull )
                            poFieldDefn->SetNullable(FALSE);
                        OGRSpatialReference* l_poSRS = NULL;
                        if( pszSRText != NULL )
                        {
                            l_poSRS = new OGRSpatialReference();
                            char* pszTmp = (char* )pszSRText;
                            if( l_poSRS->importFromWkt(&pszTmp) != OGRERR_NONE )
                            {
                                delete l_poSRS;
                                l_poSRS = NULL;
                            }
                            if( l_poSRS != NULL )
                            {
                                poFieldDefn->SetSpatialRef(l_poSRS);
                                l_poSRS->Release();
                            }
                        }
                        poFieldDefn->nSRID = nSRID;
                        poFeatureDefn->AddGeomFieldDefn(poFieldDefn, FALSE);
                    }
                    else
                    {
                        OGRFieldDefn oField(pszAttname, OFTString);
                        if( bNotNull )
                            oField.SetNullable(FALSE);
                        OGRPGCommonLayerSetType(oField, pszType, pszFormatType, nWidth);
                        if( pszDefault )
                            OGRPGCommonLayerNormalizeDefault(&oField, pszDefault);

                        poFeatureDefn->AddFieldDefn( &oField );
                    }
                }
                delete poFeat;
            }

            poDS->ReleaseResultSet(poLyr);
        }
    }

    if( poFeatureDefn == NULL )
    {
        osBaseSQL.Printf("SELECT * FROM %s", OGRCARTODBEscapeIdentifier(osName).c_str());
        EstablishLayerDefn(osName, NULL);
        osBaseSQL = "";
    }

    if( osFIDColName.size() > 0 )
    {
        osBaseSQL = "SELECT ";
        osBaseSQL += OGRCARTODBEscapeIdentifier(osFIDColName);
    }
    for(int i=0; i<poFeatureDefn->GetGeomFieldCount(); i++)
    {
        if( osBaseSQL.size() == 0 )
            osBaseSQL = "SELECT ";
        else
            osBaseSQL += ", ";
        osBaseSQL += OGRCARTODBEscapeIdentifier(poFeatureDefn->GetGeomFieldDefn(i)->GetNameRef());
    }
    for(int i=0; i<poFeatureDefn->GetFieldCount(); i++)
    {
        if( osBaseSQL.size() == 0 )
            osBaseSQL = "SELECT ";
        else
            osBaseSQL += ", ";
        osBaseSQL += OGRCARTODBEscapeIdentifier(poFeatureDefn->GetFieldDefn(i)->GetNameRef());
    }
    if( osBaseSQL.size() == 0 )
        osBaseSQL = "SELECT *";
    osBaseSQL += " FROM ";
    osBaseSQL += OGRCARTODBEscapeIdentifier(osName);

    osSELECTWithoutWHERE = osBaseSQL;

    return poFeatureDefn;
}

/************************************************************************/
/*                        FetchNewFeatures()                            */
/************************************************************************/

json_object* OGRCARTODBTableLayer::FetchNewFeatures(GIntBig iNextIn)
{
    if( osFIDColName.size() > 0 )
    {
        CPLString osSQL;
        osSQL.Printf("%s WHERE %s%s >= " CPL_FRMT_GIB " ORDER BY %s ASC LIMIT %d",
                     osSELECTWithoutWHERE.c_str(),
                     ( osWHERE.size() ) ? CPLSPrintf("%s AND ", osWHERE.c_str()) : "",
                     OGRCARTODBEscapeIdentifier(osFIDColName).c_str(),
                     iNext,
                     OGRCARTODBEscapeIdentifier(osFIDColName).c_str(),
                     GetFeaturesToFetch());
        return poDS->RunSQL(osSQL);
    }
    else
        return OGRCARTODBLayer::FetchNewFeatures(iNextIn);
}

/************************************************************************/
/*                           GetNextRawFeature()                        */
/************************************************************************/

OGRFeature  *OGRCARTODBTableLayer::GetNextRawFeature()
{
    if( bDeferedCreation && RunDeferedCreationIfNecessary() != OGRERR_NONE )
        return NULL;
    if( FlushDeferedInsert() != OGRERR_NONE )
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
        osQuery = "(";
        osQuery += pszQuery;
        osQuery += ")";
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
/*                         RunDeferedCartoDBfy()                         */
/************************************************************************/


void OGRCARTODBTableLayer::RunDeferedCartoDBfy()

{
    if( bCartoDBify )
    {
        bCartoDBify = FALSE;

        CPLString osSQL;
        if( poDS->GetCurrentSchema() == "public" )
            osSQL.Printf("SELECT cdb_cartodbfytable('%s')",
                                OGRCARTODBEscapeLiteral(osName).c_str());
        else
            osSQL.Printf("SELECT cdb_cartodbfytable('%s', '%s')",
                                OGRCARTODBEscapeLiteral(poDS->GetCurrentSchema()).c_str(),
                                OGRCARTODBEscapeLiteral(osName).c_str());

        json_object* poObj = poDS->RunSQL(osSQL);
        if( poObj != NULL )
            json_object_put(poObj);
    }
}

/************************************************************************/
/*                         FlushDeferedInsert()                         */
/************************************************************************/

OGRErr OGRCARTODBTableLayer::FlushDeferedInsert(bool bReset)

{
    OGRErr eErr = OGRERR_NONE;
    if( bInDeferedInsert && osDeferedInsertSQL.size() > 0 )
    {
        osDeferedInsertSQL = "BEGIN;" + osDeferedInsertSQL;
        if( eDeferedInsertState == INSERT_MULTIPLE_FEATURE )
        {
            osDeferedInsertSQL += ";";
            eDeferedInsertState = INSERT_UNINIT;
        }
        osDeferedInsertSQL += "COMMIT;";

        json_object* poObj = poDS->RunSQL(osDeferedInsertSQL);
        if( poObj != NULL )
        {
            json_object_put(poObj);
        }
        else
        {
            bInDeferedInsert = FALSE;
            eErr = OGRERR_FAILURE;
        }
    }

    osDeferedInsertSQL = "";
    if( bReset )
    {
        bInDeferedInsert = FALSE;
        nNextFID = -1;
    }
    return eErr;
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

    if( eDeferedInsertState == INSERT_MULTIPLE_FEATURE )
    {
        if( FlushDeferedInsert() != OGRERR_NONE )
            return OGRERR_FAILURE;
    }

    OGRFieldDefn oField(poFieldIn);
    if( bLaunderColumnNames )
    {
        char* pszName = OGRPGCommonLaunderName(oField.GetNameRef());
        oField.SetName(pszName);
        CPLFree(pszName);
    }

/* -------------------------------------------------------------------- */
/*      Create the new field.                                           */
/* -------------------------------------------------------------------- */

    if( !bDeferedCreation )
    {
        CPLString osSQL;
        osSQL.Printf( "ALTER TABLE %s ADD COLUMN %s %s",
                    OGRCARTODBEscapeIdentifier(osName).c_str(),
                    OGRCARTODBEscapeIdentifier(oField.GetNameRef()).c_str(),
                    OGRPGCommonLayerGetType(oField, FALSE, TRUE).c_str() );
        if( !oField.IsNullable() )
            osSQL += " NOT NULL";
        if( oField.GetDefault() != NULL && !oField.IsDefaultDriverSpecific() )
        {
            osSQL += " DEFAULT ";
            osSQL += OGRPGCommonLayerGetPGDefault(&oField);
        }

        json_object* poObj = poDS->RunSQL(osSQL);
        if( poObj == NULL )
            return OGRERR_FAILURE;
        json_object_put(poObj);
    }

    poFeatureDefn->AddFieldDefn( &oField );

    return OGRERR_NONE;
}

/************************************************************************/
/*                            DeleteField()                             */
/************************************************************************/

OGRErr OGRCARTODBTableLayer::DeleteField( int iField )
{
    CPLString           osSQL;

    if (!poDS->IsReadWrite())
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Operation not available in read-only mode");
        return OGRERR_FAILURE;
    }

    if (iField < 0 || iField >= poFeatureDefn->GetFieldCount())
    {
        CPLError( CE_Failure, CPLE_NotSupported,
                  "Invalid field index");
        return OGRERR_FAILURE;
    }

    if( eDeferedInsertState == INSERT_MULTIPLE_FEATURE )
    {
        if( FlushDeferedInsert() != OGRERR_NONE )
            return OGRERR_FAILURE;
    }

/* -------------------------------------------------------------------- */
/*      Drop the field.                                                 */
/* -------------------------------------------------------------------- */

    osSQL.Printf( "ALTER TABLE %s DROP COLUMN %s",
                  OGRCARTODBEscapeIdentifier(osName).c_str(),
                  OGRCARTODBEscapeIdentifier(poFeatureDefn->GetFieldDefn(iField)->GetNameRef()).c_str() );

    json_object* poObj = poDS->RunSQL(osSQL);
    if( poObj == NULL )
        return OGRERR_FAILURE;
    json_object_put(poObj);

    return poFeatureDefn->DeleteFieldDefn( iField );
}

/************************************************************************/
/*                           ICreateFeature()                            */
/************************************************************************/

OGRErr OGRCARTODBTableLayer::ICreateFeature( OGRFeature *poFeature )

{
    int i;

    if( bDeferedCreation )
    {
        if( RunDeferedCreationIfNecessary() != OGRERR_NONE )
            return OGRERR_FAILURE;
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
    if( !bHasUserFieldMatchingFID && bInDeferedInsert && nNextFID < 0 && osFIDColName.size() )
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

    // Check if we can go on with multiple insertion mode
    if( eDeferedInsertState == INSERT_MULTIPLE_FEATURE )
    {
        if( !bHasUserFieldMatchingFID && osFIDColName.size() &&
            (poFeature->GetFID() != OGRNullFID || (nNextFID >= 0 && bHasJustGotNextFID)) )
        {
            if( FlushDeferedInsert(false) != OGRERR_NONE )
                return OGRERR_FAILURE;
        }
    }

    bool bWriteInsertInto = (eDeferedInsertState != INSERT_MULTIPLE_FEATURE);
    bool bResetToUninitInsertStateAfterwards = false;
    if( eDeferedInsertState == INSERT_UNINIT )
    {
        if( !bInDeferedInsert )
        {
            eDeferedInsertState = INSERT_SINGLE_FEATURE;
        }
        else if( !bHasUserFieldMatchingFID && osFIDColName.size() &&
            (poFeature->GetFID() != OGRNullFID || (nNextFID >= 0 && bHasJustGotNextFID)) )
        {
            eDeferedInsertState = INSERT_SINGLE_FEATURE;
            bResetToUninitInsertStateAfterwards = true;
        }
        else
        {
            eDeferedInsertState = INSERT_MULTIPLE_FEATURE;
            for(i = 0; i < poFeatureDefn->GetFieldCount(); i++)
            {
                if( poFeatureDefn->GetFieldDefn(i)->GetDefault() != NULL )
                    eDeferedInsertState = INSERT_SINGLE_FEATURE;
            }
        }
    }

    int bMustComma = FALSE;
    if( bWriteInsertInto )
    {
        osSQL.Printf("INSERT INTO %s ", OGRCARTODBEscapeIdentifier(osName).c_str());
        for(i = 0; i < poFeatureDefn->GetFieldCount(); i++)
        {
            if( eDeferedInsertState != INSERT_MULTIPLE_FEATURE &&
                !poFeature->IsFieldSet(i) )
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
            if( eDeferedInsertState != INSERT_MULTIPLE_FEATURE &&
                poFeature->GetGeomFieldRef(i) == NULL )
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
            osFIDColName.size() && (poFeature->GetFID() != OGRNullFID || (nNextFID >= 0 && bHasJustGotNextFID)) )
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

        if( !bMustComma && eDeferedInsertState == INSERT_MULTIPLE_FEATURE )
            eDeferedInsertState = INSERT_SINGLE_FEATURE;
    }

    if( !bMustComma && eDeferedInsertState == INSERT_SINGLE_FEATURE )
        osSQL += "DEFAULT VALUES";
    else
    {
        if( !bWriteInsertInto && eDeferedInsertState == INSERT_MULTIPLE_FEATURE )
            osSQL += ", (";
        else
            osSQL += ") VALUES (";

        bMustComma = FALSE;
        for(i = 0; i < poFeatureDefn->GetFieldCount(); i++)
        {
            if( !poFeature->IsFieldSet(i) )
            {
                if( eDeferedInsertState == INSERT_MULTIPLE_FEATURE )
                {
                    if( bMustComma )
                        osSQL += ", ";
                    else
                        bMustComma = TRUE;
                    osSQL += "NULL";
                }
                continue;
            }

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
            {
                if( eDeferedInsertState == INSERT_MULTIPLE_FEATURE )
                {
                    if( bMustComma )
                        osSQL += ", ";
                    else
                        bMustComma = TRUE;
                    osSQL += "NULL";
                }
                continue;
            }

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
                pszEWKB = OGRGeometryToHexEWKB(poNewGeom, nSRID,
                                               poDS->GetPostGISMajor(),
                                               poDS->GetPostGISMinor());
                delete poNewGeom;
            }
            else
                pszEWKB = OGRGeometryToHexEWKB(poGeom, nSRID,
                                               poDS->GetPostGISMajor(),
                                               poDS->GetPostGISMinor());
            osSQL += "'";
            osSQL += pszEWKB;
            osSQL += "'";
            CPLFree(pszEWKB);
        }

        if( !bHasUserFieldMatchingFID )
        {
            if( osFIDColName.size() && nNextFID >= 0 )
            {
                if( bHasJustGotNextFID )
                {
                    if( bMustComma )
                        osSQL += ", ";
                    else
                        bMustComma = TRUE;

                    osSQL += CPLSPrintf(CPL_FRMT_GIB, nNextFID);
                }
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
    CPL_IGNORE_RET_VAL(bMustComma);

    if( !bHasUserFieldMatchingFID && osFIDColName.size() && nNextFID >= 0 )
    {
        poFeature->SetFID(nNextFID);
        nNextFID ++;
    }

    if( bInDeferedInsert )
    {
        OGRErr eRet = OGRERR_NONE;
        if( eDeferedInsertState == INSERT_SINGLE_FEATURE && /* in multiple mode, this would require rebuilding the osSQL buffer. Annoying... */
            osDeferedInsertSQL.size() != 0 &&
            (int)osDeferedInsertSQL.size() + (int)osSQL.size() > nMaxChunkSize )
        {
            eRet = FlushDeferedInsert(false);
        }

        osDeferedInsertSQL += osSQL;
        if( eDeferedInsertState == INSERT_SINGLE_FEATURE )
            osDeferedInsertSQL += ";";

        if( (int)osDeferedInsertSQL.size() > nMaxChunkSize )
        {
            eRet = FlushDeferedInsert(false);
        }

        if( bResetToUninitInsertStateAfterwards )
            eDeferedInsertState = INSERT_UNINIT;

        return eRet;
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
    if( bDeferedCreation && RunDeferedCreationIfNecessary() != OGRERR_NONE )
        return OGRERR_FAILURE;
    if( FlushDeferedInsert() != OGRERR_NONE )
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
    for( int i = 0; i < poFeatureDefn->GetFieldCount(); i++ )
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

    for( int i = 0; i < poFeatureDefn->GetGeomFieldCount(); i++ )
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
            char* pszEWKB = OGRGeometryToHexEWKB(poGeom, nSRID,
                                               poDS->GetPostGISMajor(),
                                               poDS->GetPostGISMinor());
            osSQL += "'";
            osSQL += pszEWKB;
            osSQL += "'";
            CPLFree(pszEWKB);
        }
    }

    osSQL += CPLSPrintf(" WHERE %s = " CPL_FRMT_GIB,
                    OGRCARTODBEscapeIdentifier(osFIDColName).c_str(),
                    poFeature->GetFID());

    OGRErr eRet = OGRERR_FAILURE;
    json_object* poObj = poDS->RunSQL(osSQL);
    if( poObj != NULL )
    {
        json_object* poTotalRows = json_object_object_get(poObj, "total_rows");
        if( poTotalRows != NULL && json_object_get_type(poTotalRows) == json_type_int )
        {
            int nTotalRows = json_object_get_int(poTotalRows);
            if( nTotalRows > 0 )
            {
                eRet = OGRERR_NONE;
            }
            else
                eRet = OGRERR_NON_EXISTING_FEATURE;
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

    if( bDeferedCreation && RunDeferedCreationIfNecessary() != OGRERR_NONE )
        return OGRERR_FAILURE;
    if( FlushDeferedInsert() != OGRERR_NONE )
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

    OGRErr eRet = OGRERR_FAILURE;
    json_object* poObj = poDS->RunSQL(osSQL);
    if( poObj != NULL )
    {
        json_object* poTotalRows = json_object_object_get(poObj, "total_rows");
        if( poTotalRows != NULL && json_object_get_type(poTotalRows) == json_type_int )
        {
            int nTotalRows = json_object_get_int(poTotalRows);
            if( nTotalRows > 0 )
            {
                eRet = OGRERR_NONE;
            }
            else
                eRet = OGRERR_NON_EXISTING_FEATURE;
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

    osSQL.Printf("SELECT srid, srtext FROM spatial_ref_sys WHERE srid IN "
                "(SELECT Find_SRID('%s', '%s', '%s'))",
                OGRCARTODBEscapeLiteral(poDS->GetCurrentSchema()).c_str(),
                OGRCARTODBEscapeLiteral(osName).c_str(),
                OGRCARTODBEscapeLiteral(pszGeomCol).c_str());

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
        osWHERE.Printf("(%s && 'BOX3D(%s, %s)'::box3d)",
                       OGRCARTODBEscapeIdentifier(osGeomColumn).c_str(),
                       szBox3D_1, szBox3D_2 );
    }

    if( strlen(osQuery) > 0 )
    {
        if( osWHERE.size() > 0 )
            osWHERE += " AND ";
        osWHERE += osQuery;
    }

    if( osFIDColName.size() == 0 )
    {
        osBaseSQL = osSELECTWithoutWHERE;
        if( osWHERE.size() )
        {
            osBaseSQL += " WHERE ";
            osBaseSQL += osWHERE;
        }
    }
}

/************************************************************************/
/*                              GetFeature()                            */
/************************************************************************/

OGRFeature* OGRCARTODBTableLayer::GetFeature( GIntBig nFeatureId )
{

    if( bDeferedCreation && RunDeferedCreationIfNecessary() != OGRERR_NONE )
        return NULL;
    if( FlushDeferedInsert() != OGRERR_NONE )
        return NULL;

    GetLayerDefn();

    if( osFIDColName.size() == 0 )
        return OGRCARTODBLayer::GetFeature(nFeatureId);

    CPLString osSQL = osSELECTWithoutWHERE;
    osSQL += " WHERE ";
    osSQL += OGRCARTODBEscapeIdentifier(osFIDColName).c_str();
    osSQL += " = ";
    osSQL += CPLSPrintf(CPL_FRMT_GIB, nFeatureId);

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

    if( bDeferedCreation && RunDeferedCreationIfNecessary() != OGRERR_NONE )
        return 0;
    if( FlushDeferedInsert() != OGRERR_NONE )
        return 0;

    GetLayerDefn();

    CPLString osSQL(CPLSPrintf("SELECT COUNT(*) FROM %s",
                               OGRCARTODBEscapeIdentifier(osName).c_str()));
    if( osWHERE.size() )
    {
        osSQL += " WHERE ";
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

    if( bDeferedCreation && RunDeferedCreationIfNecessary() != OGRERR_NONE )
        return OGRERR_FAILURE;
    if( FlushDeferedInsert() != OGRERR_NONE )
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
            // For PostGIS ver >= 1.0.0 -> Tokens: X1 Y1 X2 Y2 (nTokenCnt = 4)
            // For PostGIS ver < 1.0.0 -> Tokens: X1 Y1 Z1 X2 Y2 Z2 (nTokenCnt = 6)
            // =>   X2 index calculated as nTokenCnt/2
            //      Y2 index calculated as nTokenCnt/2+1

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
     || EQUAL(pszCap,OLCCreateField)
     || EQUAL(pszCap,OLCDeleteField) )
    {
        return poDS->IsReadWrite();
    }

    return OGRCARTODBLayer::TestCapability(pszCap);
}

/************************************************************************/
/*                        SetDeferedCreation()                          */
/************************************************************************/

void OGRCARTODBTableLayer::SetDeferedCreation (OGRwkbGeometryType eGType,
                                               OGRSpatialReference* poSRSIn,
                                               int bGeomNullable,
                                               int bCartoDBifyIn)
{
    bDeferedCreation = TRUE;
    nNextFID = 1;
    CPLAssert(poFeatureDefn == NULL);
    this->bCartoDBify = bCartoDBifyIn;
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
        if( poSRSIn != NULL )
        {
            poFieldDefn->nSRID = poDS->FetchSRSId( poSRSIn );
            poFeatureDefn->GetGeomFieldDefn(
                poFeatureDefn->GetGeomFieldCount() - 1)->SetSpatialRef(poSRSIn);
        }
    }
    osFIDColName = "cartodb_id";
    osBaseSQL.Printf("SELECT * FROM %s",
                     OGRCARTODBEscapeIdentifier(osName).c_str());
    osSELECTWithoutWHERE = osBaseSQL;
}

/************************************************************************/
/*                      RunDeferedCreationIfNecessary()                 */
/************************************************************************/

OGRErr OGRCARTODBTableLayer::RunDeferedCreationIfNecessary()
{
    if( !bDeferedCreation )
        return OGRERR_NONE;
    bDeferedCreation = FALSE;

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

    return OGRERR_NONE;
}
