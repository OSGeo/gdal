/******************************************************************************
 *
 * Project:  Carto Translator
 * Purpose:  Implements OGRCARTOTableLayer class.
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

#include "ogr_carto.h"
#include "ogr_p.h"
#include "ogr_pgdump.h"
#include "ogrgeojsonreader.h"

CPL_CVSID("$Id$")

/************************************************************************/
/*                    OGRCARTOEscapeIdentifier( )                     */
/************************************************************************/

CPLString OGRCARTOEscapeIdentifier(const char* pszStr)
{
    CPLString osStr;

    osStr += "\"";

    char ch = '\0';
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
/*                    OGRCARTOEscapeLiteral( )                        */
/************************************************************************/

CPLString OGRCARTOEscapeLiteral(const char* pszStr)
{
    CPLString osStr;

    char ch = '\0';
    for(int i=0; (ch = pszStr[i]) != '\0'; i++)
    {
        if (ch == '\'')
            osStr.append(1, ch);
        osStr.append(1, ch);
    }

    return osStr;
}

/************************************************************************/
/*                        OGRCARTOTableLayer()                        */
/************************************************************************/

OGRCARTOTableLayer::OGRCARTOTableLayer(OGRCARTODataSource* poDSIn,
                                       const char* pszName) :
    OGRCARTOLayer(poDSIn),
    osName( pszName )
{
    SetDescription( osName );
    bLaunderColumnNames = true;
    bInDeferredInsert = poDS->DoBatchInsert();
    eDeferredInsertState = INSERT_UNINIT;
    nNextFID = -1;
    bDeferredCreation = false;
    bCartodbfy = false;
    nMaxChunkSize = atoi(CPLGetConfigOption("CARTO_MAX_CHUNK_SIZE",
            CPLGetConfigOption("CARTODB_MAX_CHUNK_SIZE", "15"))) * 1024 * 1024;
}

/************************************************************************/
/*                    ~OGRCARTOTableLayer()                           */
/************************************************************************/

OGRCARTOTableLayer::~OGRCARTOTableLayer()

{
    if( bDeferredCreation ) RunDeferredCreationIfNecessary();
    CPL_IGNORE_RET_VAL(FlushDeferredInsert());
    RunDeferredCartofy();
}

/************************************************************************/
/*                          GetLayerDefnInternal()                      */
/************************************************************************/

OGRFeatureDefn * OGRCARTOTableLayer::GetLayerDefnInternal(CPL_UNUSED json_object* poObjIn)
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
                 OGRCARTOEscapeLiteral(osName).c_str(),
                 OGRCARTOEscapeLiteral(poDS->GetCurrentSchema()).c_str());
    }
    else if( poDS->HasOGRMetadataFunction() != FALSE )
    {
        osCommand.Printf( "SELECT * FROM ogr_table_metadata('%s', '%s')",
                          OGRCARTOEscapeLiteral(poDS->GetCurrentSchema()).c_str(),
                          OGRCARTOEscapeLiteral(osName).c_str() );
    }

    if( !osCommand.empty() )
    {
        if( !poDS->IsAuthenticatedConnection() && poDS->HasOGRMetadataFunction() < 0 )
            CPLPushErrorHandler(CPLQuietErrorHandler);
        OGRLayer* poLyr = poDS->ExecuteSQLInternal(osCommand);
        if( !poDS->IsAuthenticatedConnection() && poDS->HasOGRMetadataFunction() < 0 )
        {
            CPLPopErrorHandler();
            if( poLyr == NULL )
            {
                CPLDebug("CARTO", "ogr_table_metadata(text, text) not available");
                CPLErrorReset();
            }
            else if( poLyr->GetLayerDefn()->GetFieldCount() != 12 )
            {
                CPLDebug("CARTO", "ogr_table_metadata(text, text) has unexpected column count");
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
                const char* pszDefault = (iDefaultExpr >= 0 && poFeat->IsFieldSetAndNotNull(iDefaultExpr)) ?
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
                        const char* pszSRText = (poFeat->IsFieldSetAndNotNull(
                            poLyr->GetLayerDefn()->GetFieldIndex("srtext"))) ?
                                    poFeat->GetFieldAsString("srtext") : NULL;
                        OGRwkbGeometryType eType = OGRFromOGCGeomType(pszGeomType);
                        if( nDim == 3 )
                            eType = wkbSetZ(eType);
                        OGRCartoGeomFieldDefn *poFieldDefn =
                            new OGRCartoGeomFieldDefn(pszAttname, eType);
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
        osBaseSQL.Printf("SELECT * FROM %s", OGRCARTOEscapeIdentifier(osName).c_str());
        EstablishLayerDefn(osName, NULL);
        osBaseSQL = "";
    }

    if( !osFIDColName.empty() )
    {
        osBaseSQL = "SELECT ";
        osBaseSQL += OGRCARTOEscapeIdentifier(osFIDColName);
    }
    for(int i=0; i<poFeatureDefn->GetGeomFieldCount(); i++)
    {
        if( osBaseSQL.empty() )
            osBaseSQL = "SELECT ";
        else
            osBaseSQL += ", ";
        osBaseSQL += OGRCARTOEscapeIdentifier(poFeatureDefn->GetGeomFieldDefn(i)->GetNameRef());
    }
    for(int i=0; i<poFeatureDefn->GetFieldCount(); i++)
    {
        if( osBaseSQL.empty() )
            osBaseSQL = "SELECT ";
        else
            osBaseSQL += ", ";
        osBaseSQL += OGRCARTOEscapeIdentifier(poFeatureDefn->GetFieldDefn(i)->GetNameRef());
    }
    if( osBaseSQL.empty() )
        osBaseSQL = "SELECT *";
    osBaseSQL += " FROM ";
    osBaseSQL += OGRCARTOEscapeIdentifier(osName);

    osSELECTWithoutWHERE = osBaseSQL;

    return poFeatureDefn;
}

/************************************************************************/
/*                        FetchNewFeatures()                            */
/************************************************************************/

json_object* OGRCARTOTableLayer::FetchNewFeatures(GIntBig iNextIn)
{
    if( !osFIDColName.empty() )
    {
        CPLString osSQL;
        osSQL.Printf("%s WHERE %s%s >= " CPL_FRMT_GIB " ORDER BY %s ASC LIMIT %d",
                     osSELECTWithoutWHERE.c_str(),
                     ( osWHERE.size() ) ? CPLSPrintf("%s AND ", osWHERE.c_str()) : "",
                     OGRCARTOEscapeIdentifier(osFIDColName).c_str(),
                     iNext,
                     OGRCARTOEscapeIdentifier(osFIDColName).c_str(),
                     GetFeaturesToFetch());
        return poDS->RunSQL(osSQL);
    }
    else
        return OGRCARTOLayer::FetchNewFeatures(iNextIn);
}

/************************************************************************/
/*                           GetNextRawFeature()                        */
/************************************************************************/

OGRFeature  *OGRCARTOTableLayer::GetNextRawFeature()
{
    if( bDeferredCreation && RunDeferredCreationIfNecessary() != OGRERR_NONE )
        return NULL;
    if( FlushDeferredInsert() != OGRERR_NONE )
        return NULL;
    return OGRCARTOLayer::GetNextRawFeature();
}

/************************************************************************/
/*                         SetAttributeFilter()                         */
/************************************************************************/

OGRErr OGRCARTOTableLayer::SetAttributeFilter( const char *pszQuery )

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

void OGRCARTOTableLayer::SetSpatialFilter( int iGeomField, OGRGeometry * poGeomIn )

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
/*                         RunDeferredCartofy()                         */
/************************************************************************/

void OGRCARTOTableLayer::RunDeferredCartofy()

{
    if( bCartodbfy )
    {
        bCartodbfy = false;

        CPLString osSQL;
        if( poDS->GetCurrentSchema() == "public" )
            osSQL.Printf("SELECT cdb_cartodbfytable('%s')",
                                OGRCARTOEscapeLiteral(osName).c_str());
        else
            osSQL.Printf("SELECT cdb_cartodbfytable('%s', '%s')",
                                OGRCARTOEscapeLiteral(poDS->GetCurrentSchema()).c_str(),
                                OGRCARTOEscapeLiteral(osName).c_str());

        json_object* poObj = poDS->RunSQL(osSQL);
        if( poObj != NULL )
            json_object_put(poObj);
    }
}

/************************************************************************/
/*                         FlushDeferredInsert()                         */
/************************************************************************/

OGRErr OGRCARTOTableLayer::FlushDeferredInsert(bool bReset)

{
    OGRErr eErr = OGRERR_NONE;
    if( bInDeferredInsert && !osDeferredInsertSQL.empty() )
    {
        osDeferredInsertSQL = "BEGIN;" + osDeferredInsertSQL;
        if( eDeferredInsertState == INSERT_MULTIPLE_FEATURE )
        {
            osDeferredInsertSQL += ";";
            eDeferredInsertState = INSERT_UNINIT;
        }
        osDeferredInsertSQL += "COMMIT;";

        json_object* poObj = poDS->RunSQL(osDeferredInsertSQL);
        if( poObj != NULL )
        {
            json_object_put(poObj);
        }
        else
        {
            bInDeferredInsert = false;
            eErr = OGRERR_FAILURE;
        }
    }

    osDeferredInsertSQL = "";
    if( bReset )
    {
        bInDeferredInsert = false;
        nNextFID = -1;
    }
    return eErr;
}

/************************************************************************/
/*                            CreateField()                             */
/************************************************************************/

OGRErr OGRCARTOTableLayer::CreateField( OGRFieldDefn *poFieldIn,
                                          CPL_UNUSED int bApproxOK )
{
    GetLayerDefn();

    if (!poDS->IsReadWrite())
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Operation not available in read-only mode");
        return OGRERR_FAILURE;
    }

    if( eDeferredInsertState == INSERT_MULTIPLE_FEATURE )
    {
        if( FlushDeferredInsert() != OGRERR_NONE )
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

    if( !bDeferredCreation )
    {
        CPLString osSQL;
        osSQL.Printf( "ALTER TABLE %s ADD COLUMN %s %s",
                    OGRCARTOEscapeIdentifier(osName).c_str(),
                    OGRCARTOEscapeIdentifier(oField.GetNameRef()).c_str(),
                    OGRPGCommonLayerGetType(oField, false, true).c_str() );
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

OGRErr OGRCARTOTableLayer::DeleteField( int iField )
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

    if( eDeferredInsertState == INSERT_MULTIPLE_FEATURE )
    {
        if( FlushDeferredInsert() != OGRERR_NONE )
            return OGRERR_FAILURE;
    }

/* -------------------------------------------------------------------- */
/*      Drop the field.                                                 */
/* -------------------------------------------------------------------- */

    osSQL.Printf( "ALTER TABLE %s DROP COLUMN %s",
                  OGRCARTOEscapeIdentifier(osName).c_str(),
                  OGRCARTOEscapeIdentifier(poFeatureDefn->GetFieldDefn(iField)->GetNameRef()).c_str() );

    json_object* poObj = poDS->RunSQL(osSQL);
    if( poObj == NULL )
        return OGRERR_FAILURE;
    json_object_put(poObj);

    return poFeatureDefn->DeleteFieldDefn( iField );
}

/************************************************************************/
/*                           ICreateFeature()                            */
/************************************************************************/

OGRErr OGRCARTOTableLayer::ICreateFeature( OGRFeature *poFeature )

{
    if( bDeferredCreation )
    {
        if( RunDeferredCreationIfNecessary() != OGRERR_NONE )
            return OGRERR_FAILURE;
    }

    GetLayerDefn();
    bool bHasUserFieldMatchingFID = false;
    if( !osFIDColName.empty() )
        bHasUserFieldMatchingFID = poFeatureDefn->GetFieldIndex(osFIDColName) >= 0;

    if (!poDS->IsReadWrite())
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Operation not available in read-only mode");
        return OGRERR_FAILURE;
    }

    CPLString osSQL;

    bool bHasJustGotNextFID = false;
    if( !bHasUserFieldMatchingFID && bInDeferredInsert && nNextFID < 0 && !osFIDColName.empty() )
    {
        osSQL.Printf("SELECT nextval('%s') AS nextid",
                     OGRCARTOEscapeLiteral(CPLSPrintf("%s_%s_seq", osName.c_str(), osFIDColName.c_str())).c_str());

        json_object* poObj = poDS->RunSQL(osSQL);
        json_object* poRowObj = OGRCARTOGetSingleRow(poObj);
        if( poRowObj != NULL )
        {
            json_object* poID = CPL_json_object_object_get(poRowObj, "nextid");
            if( poID != NULL && json_object_get_type(poID) == json_type_int )
            {
                nNextFID = json_object_get_int64(poID);
                bHasJustGotNextFID = true;
            }
        }

        if( poObj != NULL )
            json_object_put(poObj);
    }

    // Check if we can go on with multiple insertion mode
    if( eDeferredInsertState == INSERT_MULTIPLE_FEATURE )
    {
        if( !bHasUserFieldMatchingFID && !osFIDColName.empty() &&
            (poFeature->GetFID() != OGRNullFID || (nNextFID >= 0 && bHasJustGotNextFID)) )
        {
            if( FlushDeferredInsert(false) != OGRERR_NONE )
                return OGRERR_FAILURE;
        }
    }

    bool bWriteInsertInto = (eDeferredInsertState != INSERT_MULTIPLE_FEATURE);
    bool bResetToUninitInsertStateAfterwards = false;
    if( eDeferredInsertState == INSERT_UNINIT )
    {
        if( !bInDeferredInsert )
        {
            eDeferredInsertState = INSERT_SINGLE_FEATURE;
        }
        else if( !bHasUserFieldMatchingFID && !osFIDColName.empty() &&
            (poFeature->GetFID() != OGRNullFID || (nNextFID >= 0 && bHasJustGotNextFID)) )
        {
            eDeferredInsertState = INSERT_SINGLE_FEATURE;
            bResetToUninitInsertStateAfterwards = true;
        }
        else
        {
            eDeferredInsertState = INSERT_MULTIPLE_FEATURE;
            for( int i = 0; i < poFeatureDefn->GetFieldCount(); i++ )
            {
                if( poFeatureDefn->GetFieldDefn(i)->GetDefault() != NULL )
                    eDeferredInsertState = INSERT_SINGLE_FEATURE;
            }
        }
    }

    bool bMustComma = false;
    if( bWriteInsertInto )
    {
        osSQL.Printf("INSERT INTO %s ", OGRCARTOEscapeIdentifier(osName).c_str());
        for( int i = 0; i < poFeatureDefn->GetFieldCount(); i++ )
        {
            if( eDeferredInsertState != INSERT_MULTIPLE_FEATURE &&
                !poFeature->IsFieldSet(i) )
                continue;

            if( bMustComma )
                osSQL += ", ";
            else
            {
                osSQL += "(";
                bMustComma = true;
            }

            osSQL += OGRCARTOEscapeIdentifier(poFeatureDefn->GetFieldDefn(i)->GetNameRef());
        }

        for( int i = 0; i < poFeatureDefn->GetGeomFieldCount(); i++ )
        {
            if( eDeferredInsertState != INSERT_MULTIPLE_FEATURE &&
                poFeature->GetGeomFieldRef(i) == NULL )
                continue;

            if( bMustComma )
                osSQL += ", ";
            else
            {
                osSQL += "(";
                bMustComma = true;
            }

            osSQL += OGRCARTOEscapeIdentifier(poFeatureDefn->GetGeomFieldDefn(i)->GetNameRef());
        }

        if( !bHasUserFieldMatchingFID &&
            !osFIDColName.empty() && (poFeature->GetFID() != OGRNullFID || (nNextFID >= 0 && bHasJustGotNextFID)) )
        {
            if( bMustComma )
                osSQL += ", ";
            else
            {
                osSQL += "(";
                bMustComma = true;
            }

            osSQL += OGRCARTOEscapeIdentifier(osFIDColName);
        }

        if( !bMustComma && eDeferredInsertState == INSERT_MULTIPLE_FEATURE )
            eDeferredInsertState = INSERT_SINGLE_FEATURE;
    }

    if( !bMustComma && eDeferredInsertState == INSERT_SINGLE_FEATURE )
        osSQL += "DEFAULT VALUES";
    else
    {
        if( !bWriteInsertInto && eDeferredInsertState == INSERT_MULTIPLE_FEATURE )
            osSQL += ", (";
        else
            osSQL += ") VALUES (";

        bMustComma = false;
        for( int i = 0; i < poFeatureDefn->GetFieldCount(); i++)
        {
            if( !poFeature->IsFieldSet(i) )
            {
                if( eDeferredInsertState == INSERT_MULTIPLE_FEATURE )
                {
                    if( bMustComma )
                        osSQL += ", ";
                    else
                        bMustComma = true;
                    osSQL += "NULL";
                }
                continue;
            }

            if( bMustComma )
                osSQL += ", ";
            else
                bMustComma = true;

            OGRFieldType eType = poFeatureDefn->GetFieldDefn(i)->GetType();
            if( poFeature->IsFieldNull(i) )
            {
                osSQL += "NULL";
            }
            else if( eType == OFTString || eType == OFTDateTime || eType == OFTDate || eType == OFTTime )
            {
                osSQL += "'";
                osSQL += OGRCARTOEscapeLiteral(poFeature->GetFieldAsString(i));
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

        for( int i = 0; i < poFeatureDefn->GetGeomFieldCount(); i++ )
        {
            OGRGeometry* poGeom = poFeature->GetGeomFieldRef(i);
            if( poGeom == NULL )
            {
                if( eDeferredInsertState == INSERT_MULTIPLE_FEATURE )
                {
                    if( bMustComma )
                        osSQL += ", ";
                    else
                        bMustComma = true;
                    osSQL += "NULL";
                }
                continue;
            }

            if( bMustComma )
                osSQL += ", ";
            else
                bMustComma = true;

            OGRCartoGeomFieldDefn* poGeomFieldDefn =
                (OGRCartoGeomFieldDefn *)poFeatureDefn->GetGeomFieldDefn(i);
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
            if( !osFIDColName.empty() && nNextFID >= 0 )
            {
                if( bHasJustGotNextFID )
                {
                    if( bMustComma )
                        osSQL += ", ";
                    // No need to set bMustComma to true in else case.
                    // Not in a loop.
                    osSQL += CPLSPrintf(CPL_FRMT_GIB, nNextFID);
                }
            }
            else if( !osFIDColName.empty() && poFeature->GetFID() != OGRNullFID )
            {
                if( bMustComma )
                    osSQL += ", ";
                // No need to set bMustComma to true in else case
                // Not in a loop.

                osSQL += CPLSPrintf(CPL_FRMT_GIB, poFeature->GetFID());
            }
        }

        osSQL += ")";
    }

    if( !bHasUserFieldMatchingFID && !osFIDColName.empty() && nNextFID >= 0 )
    {
        poFeature->SetFID(nNextFID);
        nNextFID ++;
    }

    if( bInDeferredInsert )
    {
        OGRErr eRet = OGRERR_NONE;
        // In multiple mode, this would require rebuilding the osSQL
        // buffer. Annoying.
        if( eDeferredInsertState == INSERT_SINGLE_FEATURE &&
            !osDeferredInsertSQL.empty() &&
            (int)osDeferredInsertSQL.size() + (int)osSQL.size() > nMaxChunkSize )
        {
            eRet = FlushDeferredInsert(false);
        }

        osDeferredInsertSQL += osSQL;
        if( eDeferredInsertState == INSERT_SINGLE_FEATURE )
            osDeferredInsertSQL += ";";

        if( (int)osDeferredInsertSQL.size() > nMaxChunkSize )
        {
            eRet = FlushDeferredInsert(false);
        }

        if( bResetToUninitInsertStateAfterwards )
            eDeferredInsertState = INSERT_UNINIT;

        return eRet;
    }

    if( !osFIDColName.empty() )
    {
        osSQL += " RETURNING ";
        osSQL += OGRCARTOEscapeIdentifier(osFIDColName);

        json_object* poObj = poDS->RunSQL(osSQL);
        json_object* poRowObj = OGRCARTOGetSingleRow(poObj);
        if( poRowObj == NULL )
        {
            if( poObj != NULL )
                json_object_put(poObj);
            return OGRERR_FAILURE;
        }

        json_object* poID = CPL_json_object_object_get(poRowObj, osFIDColName);
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
            json_object* poTotalRows = CPL_json_object_object_get(poObj, "total_rows");
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

OGRErr OGRCARTOTableLayer::ISetFeature( OGRFeature *poFeature )

{
    if( bDeferredCreation && RunDeferredCreationIfNecessary() != OGRERR_NONE )
        return OGRERR_FAILURE;
    if( FlushDeferredInsert() != OGRERR_NONE )
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
    osSQL.Printf("UPDATE %s SET ", OGRCARTOEscapeIdentifier(osName).c_str());
    bool bMustComma = false;
    for( int i = 0; i < poFeatureDefn->GetFieldCount(); i++ )
    {
        if( !poFeature->IsFieldSet(i) )
            continue;

        if( bMustComma )
            osSQL += ", ";
        else
            bMustComma = true;

        osSQL += OGRCARTOEscapeIdentifier(poFeatureDefn->GetFieldDefn(i)->GetNameRef());
        osSQL += " = ";

        if( poFeature->IsFieldNull(i) )
        {
            osSQL += "NULL";
        }
        else
        {
            OGRFieldType eType = poFeatureDefn->GetFieldDefn(i)->GetType();
            if( eType == OFTString || eType == OFTDateTime || eType == OFTDate || eType == OFTTime )
            {
                osSQL += "'";
                osSQL += OGRCARTOEscapeLiteral(poFeature->GetFieldAsString(i));
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
            bMustComma = true;

        osSQL += OGRCARTOEscapeIdentifier(poFeatureDefn->GetGeomFieldDefn(i)->GetNameRef());
        osSQL += " = ";

        OGRGeometry* poGeom = poFeature->GetGeomFieldRef(i);
        if( poGeom == NULL )
        {
            osSQL += "NULL";
        }
        else
        {
            OGRCartoGeomFieldDefn* poGeomFieldDefn =
                (OGRCartoGeomFieldDefn *)poFeatureDefn->GetGeomFieldDefn(i);
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

    if( !bMustComma ) // nothing to do
        return OGRERR_NONE;

    osSQL += CPLSPrintf(" WHERE %s = " CPL_FRMT_GIB,
                    OGRCARTOEscapeIdentifier(osFIDColName).c_str(),
                    poFeature->GetFID());

    OGRErr eRet = OGRERR_FAILURE;
    json_object* poObj = poDS->RunSQL(osSQL);
    if( poObj != NULL )
    {
        json_object* poTotalRows = CPL_json_object_object_get(poObj, "total_rows");
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

OGRErr OGRCARTOTableLayer::DeleteFeature( GIntBig nFID )

{

    if( bDeferredCreation && RunDeferredCreationIfNecessary() != OGRERR_NONE )
        return OGRERR_FAILURE;
    if( FlushDeferredInsert() != OGRERR_NONE )
        return OGRERR_FAILURE;

    GetLayerDefn();

    if (!poDS->IsReadWrite())
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Operation not available in read-only mode");
        return OGRERR_FAILURE;
    }

    if( osFIDColName.empty() )
        return OGRERR_FAILURE;

    CPLString osSQL;
    osSQL.Printf("DELETE FROM %s WHERE %s = " CPL_FRMT_GIB,
                    OGRCARTOEscapeIdentifier(osName).c_str(),
                    OGRCARTOEscapeIdentifier(osFIDColName).c_str(),
                    nFID);

    OGRErr eRet = OGRERR_FAILURE;
    json_object* poObj = poDS->RunSQL(osSQL);
    if( poObj != NULL )
    {
        json_object* poTotalRows = CPL_json_object_object_get(poObj, "total_rows");
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

CPLString OGRCARTOTableLayer::GetSRS_SQL(const char* pszGeomCol)
{
    CPLString osSQL;

    osSQL.Printf("SELECT srid, srtext FROM spatial_ref_sys WHERE srid IN "
                "(SELECT Find_SRID('%s', '%s', '%s'))",
                OGRCARTOEscapeLiteral(poDS->GetCurrentSchema()).c_str(),
                OGRCARTOEscapeLiteral(osName).c_str(),
                OGRCARTOEscapeLiteral(pszGeomCol).c_str());

    return osSQL;
}

/************************************************************************/
/*                             BuildWhere()                             */
/*                                                                      */
/*      Build the WHERE statement appropriate to the current set of     */
/*      criteria (spatial and attribute queries).                       */
/************************************************************************/

void OGRCARTOTableLayer::BuildWhere()

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
                       OGRCARTOEscapeIdentifier(osGeomColumn).c_str(),
                       szBox3D_1, szBox3D_2 );
    }

    if( !osQuery.empty() )
    {
        if( !osWHERE.empty() )
            osWHERE += " AND ";
        osWHERE += osQuery;
    }

    if( osFIDColName.empty() )
    {
        osBaseSQL = osSELECTWithoutWHERE;
        if( !osWHERE.empty() )
        {
            osBaseSQL += " WHERE ";
            osBaseSQL += osWHERE;
        }
    }
}

/************************************************************************/
/*                              GetFeature()                            */
/************************************************************************/

OGRFeature* OGRCARTOTableLayer::GetFeature( GIntBig nFeatureId )
{

    if( bDeferredCreation && RunDeferredCreationIfNecessary() != OGRERR_NONE )
        return NULL;
    if( FlushDeferredInsert() != OGRERR_NONE )
        return NULL;

    GetLayerDefn();

    if( osFIDColName.empty() )
        return OGRCARTOLayer::GetFeature(nFeatureId);

    CPLString osSQL = osSELECTWithoutWHERE;
    osSQL += " WHERE ";
    osSQL += OGRCARTOEscapeIdentifier(osFIDColName).c_str();
    osSQL += " = ";
    osSQL += CPLSPrintf(CPL_FRMT_GIB, nFeatureId);

    json_object* poObj = poDS->RunSQL(osSQL);
    json_object* poRowObj = OGRCARTOGetSingleRow(poObj);
    if( poRowObj == NULL )
    {
        if( poObj != NULL )
            json_object_put(poObj);
        return OGRCARTOLayer::GetFeature(nFeatureId);
    }

    OGRFeature* poFeature = BuildFeature(poRowObj);
    json_object_put(poObj);

    return poFeature;
}

/************************************************************************/
/*                          GetFeatureCount()                           */
/************************************************************************/

GIntBig OGRCARTOTableLayer::GetFeatureCount(int bForce)
{

    if( bDeferredCreation && RunDeferredCreationIfNecessary() != OGRERR_NONE )
        return 0;
    if( FlushDeferredInsert() != OGRERR_NONE )
        return 0;

    GetLayerDefn();

    CPLString osSQL(CPLSPrintf("SELECT COUNT(*) FROM %s",
                               OGRCARTOEscapeIdentifier(osName).c_str()));
    if( !osWHERE.empty() )
    {
        osSQL += " WHERE ";
        osSQL += osWHERE;
    }

    json_object* poObj = poDS->RunSQL(osSQL);
    json_object* poRowObj = OGRCARTOGetSingleRow(poObj);
    if( poRowObj == NULL )
    {
        if( poObj != NULL )
            json_object_put(poObj);
        return OGRCARTOLayer::GetFeatureCount(bForce);
    }

    json_object* poCount = CPL_json_object_object_get(poRowObj, "count");
    if( poCount == NULL || json_object_get_type(poCount) != json_type_int )
    {
        json_object_put(poObj);
        return OGRCARTOLayer::GetFeatureCount(bForce);
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

OGRErr OGRCARTOTableLayer::GetExtent( int iGeomField, OGREnvelope *psExtent, int bForce )
{
    CPLString   osSQL;

    if( bDeferredCreation && RunDeferredCreationIfNecessary() != OGRERR_NONE )
        return OGRERR_FAILURE;
    if( FlushDeferredInsert() != OGRERR_NONE )
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
                  OGRCARTOEscapeIdentifier(poGeomFieldDefn->GetNameRef()).c_str(),
                  OGRCARTOEscapeIdentifier(osName).c_str());

    json_object* poObj = poDS->RunSQL(osSQL);
    json_object* poRowObj = OGRCARTOGetSingleRow(poObj);
    if( poRowObj != NULL )
    {
        json_object* poExtent = CPL_json_object_object_get(poRowObj, "st_extent");
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

int OGRCARTOTableLayer::TestCapability( const char * pszCap )

{
    if( EQUAL(pszCap, OLCFastFeatureCount) )
        return TRUE;
    if( EQUAL(pszCap, OLCFastGetExtent) )
        return TRUE;
    if( EQUAL(pszCap, OLCRandomRead) )
    {
        GetLayerDefn();
        return !osFIDColName.empty();
    }

    if( EQUAL(pszCap,OLCSequentialWrite)
     || EQUAL(pszCap,OLCRandomWrite)
     || EQUAL(pszCap,OLCDeleteFeature)
     || EQUAL(pszCap,OLCCreateField)
     || EQUAL(pszCap,OLCDeleteField) )
    {
        return poDS->IsReadWrite();
    }

    return OGRCARTOLayer::TestCapability(pszCap);
}

/************************************************************************/
/*                        SetDeferredCreation()                          */
/************************************************************************/

void OGRCARTOTableLayer::SetDeferredCreation( OGRwkbGeometryType eGType,
                                              OGRSpatialReference* poSRSIn,
                                              bool bGeomNullable,
                                              bool bCartodbfyIn )
{
    bDeferredCreation = true;
    nNextFID = 1;
    CPLAssert(poFeatureDefn == NULL);
    bCartodbfy = bCartodbfyIn;
    poFeatureDefn = new OGRFeatureDefn(osName);
    poFeatureDefn->Reference();
    poFeatureDefn->SetGeomType(wkbNone);
    if( eGType == wkbPolygon )
        eGType = wkbMultiPolygon;
    else if( eGType == wkbPolygon25D )
        eGType = wkbMultiPolygon25D;
    if( eGType != wkbNone )
    {
        OGRCartoGeomFieldDefn *poFieldDefn =
            new OGRCartoGeomFieldDefn("the_geom", eGType);
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
                     OGRCARTOEscapeIdentifier(osName).c_str());
    osSELECTWithoutWHERE = osBaseSQL;
}

/************************************************************************/
/*                      RunDeferredCreationIfNecessary()                 */
/************************************************************************/

OGRErr OGRCARTOTableLayer::RunDeferredCreationIfNecessary()
{
    if( !bDeferredCreation )
        return OGRERR_NONE;
    bDeferredCreation = false;

    CPLString osSQL;
    osSQL.Printf("CREATE TABLE %s ( %s SERIAL,",
                 OGRCARTOEscapeIdentifier(osName).c_str(),
                 osFIDColName.c_str());

    int nSRID = 0;
    OGRwkbGeometryType eGType = GetGeomType();
    if( eGType != wkbNone )
    {
        CPLString osGeomType = OGRToOGCGeomType(eGType);
        if( wkbHasZ(eGType) )
            osGeomType += "Z";

        OGRCartoGeomFieldDefn *poFieldDefn =
            (OGRCartoGeomFieldDefn *)poFeatureDefn->GetGeomFieldDefn(0);
        nSRID = poFieldDefn->nSRID;

        osSQL += CPLSPrintf("%s GEOMETRY(%s, %d)%s,",
                 "the_geom",
                 osGeomType.c_str(),
                 nSRID,
                 (!poFieldDefn->IsNullable()) ? " NOT NULL" : "");
    }

    for( int i = 0; i < poFeatureDefn->GetFieldCount(); i++ )
    {
        OGRFieldDefn* poFieldDefn = poFeatureDefn->GetFieldDefn(i);
        if( strcmp(poFieldDefn->GetNameRef(), osFIDColName) != 0 )
        {
            osSQL += OGRCARTOEscapeIdentifier(poFieldDefn->GetNameRef());
            osSQL += " ";
            osSQL += OGRPGCommonLayerGetType(*poFieldDefn, false, true);
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

    CPLString osSeqName(OGRCARTOEscapeIdentifier(CPLSPrintf("%s_%s_seq",
                                osName.c_str(), osFIDColName.c_str())));

    osSQL += ";";
    osSQL += CPLSPrintf("DROP SEQUENCE IF EXISTS %s CASCADE", osSeqName.c_str());
    osSQL += ";";
    osSQL += CPLSPrintf("CREATE SEQUENCE %s START 1", osSeqName.c_str());
    osSQL += ";";
    osSQL += CPLSPrintf("ALTER SEQUENCE %s OWNED BY %s.%s", osSeqName.c_str(),
                        OGRCARTOEscapeIdentifier(osName).c_str(), osFIDColName.c_str());
    osSQL += ";";
    osSQL += CPLSPrintf("ALTER TABLE %s ALTER COLUMN %s SET DEFAULT nextval('%s')",
                        OGRCARTOEscapeIdentifier(osName).c_str(),
                        osFIDColName.c_str(), osSeqName.c_str());

    json_object* poObj = poDS->RunSQL(osSQL);
    if( poObj == NULL )
        return OGRERR_FAILURE;
    json_object_put(poObj);

    return OGRERR_NONE;
}
