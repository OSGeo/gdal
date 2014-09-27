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
}

/************************************************************************/
/*                    ~OGRCARTODBTableLayer()                           */
/************************************************************************/

OGRCARTODBTableLayer::~OGRCARTODBTableLayer()

{
}

/************************************************************************/
/*                           GetLayerDefn()                             */
/************************************************************************/

OGRFeatureDefn * OGRCARTODBTableLayer::GetLayerDefn()
{
    if( poFeatureDefn != NULL )
        return poFeatureDefn;

    osBaseSQL.Printf("SELECT * FROM %s",
                     OGRCARTODBEscapeIdentifier(osName).c_str());
    EstablishLayerDefn(osName);
    if( osFIDColName.size() > 0 )
    {
        osBaseSQL.Printf("SELECT * FROM %s ORDER BY %s ASC",
                         OGRCARTODBEscapeIdentifier(osName).c_str(),
                         OGRCARTODBEscapeIdentifier(osFIDColName).c_str());
    }

    return poFeatureDefn;
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

    const char* pszFieldType = "VARCHAR";
    switch( poFieldIn->GetType() )
    {
        case OFTInteger:
            pszFieldType = "INTEGER";
            break;
        case OFTReal:
            pszFieldType = "FLOAT8";
            break;
        case OFTDate:
            pszFieldType = "date";
            break;
        case OFTTime:
            pszFieldType = "time";
            break;
        case OFTDateTime:
            pszFieldType = "timestamp with time zone";
            break;
        default:
            break;
    }

    CPLString osSQL;
    osSQL.Printf( "ALTER TABLE %s ADD COLUMN %s %s",
                  OGRCARTODBEscapeIdentifier(osName).c_str(),
                  OGRCARTODBEscapeIdentifier(poFieldIn->GetNameRef()).c_str(),
                  pszFieldType );

    json_object* poObj = poDS->RunSQL(osSQL);
    if( poObj == NULL )
        return OGRERR_FAILURE;
    json_object_put(poObj);

    poFeatureDefn->AddFieldDefn( poFieldIn );

    return OGRERR_NONE;
}

/************************************************************************/
/*                           CreateFeature()                            */
/************************************************************************/

OGRErr OGRCARTODBTableLayer::CreateFeature( OGRFeature *poFeature )

{
    int i;
    
    GetLayerDefn();

    if (!poDS->IsReadWrite())
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Operation not available in read-only mode");
        return OGRERR_FAILURE;
    }

    CPLString osSQL;

    int bHasJustGotNextFID = FALSE;
    if( bInTransaction && nNextFID < 0 && osFIDColName.size() )
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
    
    if( osFIDColName.size() && (poFeature->GetFID() != OGRNullFID || nNextFID >= 0) )
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
            
            if( poFeatureDefn->GetFieldDefn(i)->GetType() == OFTString )
            {
                osSQL += "'";
                osSQL += OGRCARTODBEscapeLiteral(poFeature->GetFieldAsString(i));
                osSQL += "'";
            }
            else
                osSQL += OGRCARTODBEscapeLiteral(poFeature->GetFieldAsString(i));
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
            char* pszEWKB = OGRGeometryToHexEWKB(poGeom, nSRID);
            osSQL += "'";
            osSQL += pszEWKB;
            osSQL += "'";
            CPLFree(pszEWKB);
        }

        if( osFIDColName.size() && nNextFID >= 0 )
        {
            if( bMustComma )
                osSQL += ", ";
            else
                bMustComma = TRUE;

            if( bHasJustGotNextFID )
            {
                osSQL += CPLSPrintf("%ld", nNextFID);
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

            osSQL += CPLSPrintf("%ld", poFeature->GetFID());
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

        json_object* poID = json_object_object_get(poRowObj, "cartodb_id");
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
/*                            SetFeature()                              */
/************************************************************************/

OGRErr OGRCARTODBTableLayer::SetFeature( OGRFeature *poFeature )

{
    int i;
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
        else if( poFeatureDefn->GetFieldDefn(i)->GetType() == OFTString )
        {
            osSQL += "'";
            osSQL += OGRCARTODBEscapeLiteral(poFeature->GetFieldAsString(i));
            osSQL += "'";
        }
        else
            osSQL += OGRCARTODBEscapeLiteral(poFeature->GetFieldAsString(i));
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
            char* pszEWKB = OGRGeometryToHexEWKB(poGeom, nSRID);
            osSQL += "'";
            osSQL += pszEWKB;
            osSQL += "'";
            CPLFree(pszEWKB);
        }
    }

    osSQL += CPLSPrintf(" WHERE %s = %ld",
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

OGRErr OGRCARTODBTableLayer::DeleteFeature( long nFID )

{
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
    osSQL.Printf("DELETE FROM %s WHERE %s = %ld",
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
                    "(SELECT Find_SRID('public', '%s', '%s'))",
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

        snprintf(szBox3D_1, sizeof(szBox3D_1), "%.18g %.18g", sEnvelope.MinX, sEnvelope.MinY);
        while((pszComma = strchr(szBox3D_1, ',')) != NULL)
            *pszComma = '.';
        snprintf(szBox3D_2, sizeof(szBox3D_2), "%.18g %.18g", sEnvelope.MaxX, sEnvelope.MaxY);
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
}

/************************************************************************/
/*                              GetFeature()                            */
/************************************************************************/

OGRFeature* OGRCARTODBTableLayer::GetFeature( long nFeatureId )
{
    GetLayerDefn();
    
    if( osFIDColName.size() == 0 )
        return OGRCARTODBLayer::GetFeature(nFeatureId);

    CPLString osSQL(CPLSPrintf("SELECT * FROM %s WHERE %s = %ld",
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

int OGRCARTODBTableLayer::GetFeatureCount(int bForce)
{
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

    int nRet = (int)json_object_get_int64(poCount);

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
