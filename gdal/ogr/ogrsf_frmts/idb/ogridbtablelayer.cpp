/******************************************************************************
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  Implements OGRIDBTableLayer class, access to an existing table
 *           (based on ODBC and PG drivers).
 * Author:   Oleg Semykin, oleg.semykin@gmail.com
 *
 ******************************************************************************
 * Copyright (c) 2006, Oleg Semykin
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

#include "cpl_conv.h"
#include "cpl_string.h"
#include "ogr_idb.h"

CPL_CVSID("$Id$")
/************************************************************************/
/*                          OGRIDBTableLayer()                         */
/************************************************************************/

OGRIDBTableLayer::OGRIDBTableLayer( OGRIDBDataSource *poDSIn )

{
    poDS = poDSIn;

    pszQuery = NULL;

    bUpdateAccess = TRUE;
    bHaveSpatialExtents = FALSE;

    iNextShapeId = 0;

    poFeatureDefn = NULL;
}

/************************************************************************/
/*                          ~OGRIDBTableLayer()                          */
/************************************************************************/

OGRIDBTableLayer::~OGRIDBTableLayer()

{
    CPLFree( pszQuery );
    ClearQuery();
}

/************************************************************************/
/*                             Initialize()                             */
/************************************************************************/

CPLErr OGRIDBTableLayer::Initialize( const char *pszTableName,
                                     const char *pszGeomCol,
                                     int bUpdate )

{
    bUpdateAccess = bUpdate;

    ITConnection *poConn = poDS->GetConnection();

    if ( pszFIDColumn )
    {
        CPLFree( pszFIDColumn );
        pszFIDColumn = NULL;
    }

/* -------------------------------------------------------------------- */
/*      Do we have a simple primary key?                                */
/* -------------------------------------------------------------------- */
    if ( pszFIDColumn == NULL )
    {
        ITCursor oGetKey( *poConn );

        CPLString osSql =
                " select sc.colname"
                " from syscolumns sc, sysindexes si, systables st"
                " where st.tabid = si.tabid"
                " and st.tabid = sc.tabid"
                " and si.idxtype = 'U'"
                " and sc.colno = si.part1"
                " and si.part2 = 0" // only one-column keys
                " and st.tabname='";
        osSql += pszTableName;
        osSql += "'";

        if( oGetKey.Prepare( osSql.c_str() ) &&
            oGetKey.Open(ITCursor::ReadOnly) )
        {
            ITValue * poVal = oGetKey.Fetch();
            if ( poVal && poVal->IsNull() == false )
            {
                pszFIDColumn = CPLStrdup(poVal->Printable());
                poVal->Release();
            }

            if( oGetKey.Fetch() ) // more than one field in key!
            {
                CPLFree( pszFIDColumn );
                pszFIDColumn = NULL;

                CPLDebug("OGR_IDB", "Table %s has multiple primary key fields,"
                         " ignoring them all.", pszTableName );
            }
        }
    }

/* -------------------------------------------------------------------- */
/*      Have we been provided a geometry column?                        */
/* -------------------------------------------------------------------- */
    CPLFree( pszGeomColumn );
    if( pszGeomCol == NULL )
        pszGeomColumn = NULL;
    else
        pszGeomColumn = CPLStrdup( pszGeomCol );

/* -------------------------------------------------------------------- */
/*      Get the column definitions for this table.                      */
/* -------------------------------------------------------------------- */
    ITCursor oGetCol( *poConn );
    CPLErr eErr;

    CPLString sql;
    sql.Printf( "select * from %s where 1=0", pszTableName );
    if( ! oGetCol.Prepare( sql.c_str() ) ||
        ! oGetCol.Open(ITCursor::ReadOnly) )
        return CE_Failure;

    eErr = BuildFeatureDefn( pszTableName, &oGetCol );
    if( eErr != CE_None )
        return eErr;

    if( poFeatureDefn->GetFieldCount() == 0 )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "No column definitions found for table '%s', layer not usable.",
                  pszTableName );
        return CE_Failure;
    }

/* -------------------------------------------------------------------- */
/*      Do we have XMIN, YMIN, XMAX, YMAX extent fields?                */
/* -------------------------------------------------------------------- */
    if( poFeatureDefn->GetFieldIndex( "XMIN" ) != -1
        && poFeatureDefn->GetFieldIndex( "XMAX" ) != -1
        && poFeatureDefn->GetFieldIndex( "YMIN" ) != -1
        && poFeatureDefn->GetFieldIndex( "YMAX" ) != -1 )
    {
        bHaveSpatialExtents = TRUE;
        CPLDebug( "OGR_IDB", "Table %s has geometry extent fields.",
                  pszTableName );
    }

/* -------------------------------------------------------------------- */
/*      If we got a geometry column, does it exist?  Is it binary?      */
/* -------------------------------------------------------------------- */
    if( pszGeomColumn != NULL )
    {
        int iColumn = oGetCol.RowType()->ColumnId( pszGeomColumn );
        if( iColumn < 0 )
        {
            CPLError( CE_Failure, CPLE_AppDefined,
                      "Column %s requested for geometry, but it does not exist.",
                      pszGeomColumn );
            CPLFree( pszGeomColumn );
            pszGeomColumn = NULL;
        }
        bGeomColumnWKB = TRUE;
        /*else
        {
            if( ITCursor::GetTypeMapping(
                    oGetCol.GetColType( iColumn )) == SQL_C_BINARY )
                bGeomColumnWKB = TRUE;
        }*/
    }

    return CE_None;
}

/************************************************************************/
/*                           ClearQuery()                           */
/************************************************************************/

void OGRIDBTableLayer::ClearQuery()

{
    if( poCurr != NULL )
    {
        poCurr->Close();
        delete poCurr;
        poCurr = NULL;
    }
}

/************************************************************************/
/*                            GetQuery()                            */
/************************************************************************/

ITCursor *OGRIDBTableLayer::GetQuery()

{
    if( poCurr == NULL )
        ResetQuery();

    return poCurr;
}

/************************************************************************/
/*                           ResetQuery()                           */
/************************************************************************/

OGRErr OGRIDBTableLayer::ResetQuery()

{
    ClearQuery();

    iNextShapeId = 0;

    poCurr = new ITCursor( *poDS->GetConnection() );

    // Create list of fields
    CPLString osFields;

    if ( pszGeomColumn )
    {
        if ( ! osFields.empty() )
            osFields += ",";

        osFields += "st_asbinary(";
        osFields += pszGeomColumn;
        osFields += ") as ";
        osFields += pszGeomColumn;
    }

    for( int i = 0; i < poFeatureDefn->GetFieldCount(); i++ )
    {
        if ( ! osFields.empty() )
            osFields += ",";

        osFields += poFeatureDefn->GetFieldDefn(i)->GetNameRef();
    }

    CPLString sql;

    sql += "SELECT ";
    sql += osFields;
    sql += " FROM ";
    sql += poFeatureDefn->GetName();

    /* Append attribute query if we have it */
    if( pszQuery != NULL )
    {
        sql += " WHERE ";
        sql += pszQuery;
    }

    /* If we have a spatial filter, and per record extents, query on it */
    if( m_poFilterGeom != NULL && bHaveSpatialExtents )
    {
        if( pszQuery == NULL )
            sql += " WHERE";
        else
            sql += " AND";

        CPLString sqlTmp;
        sqlTmp.Printf( "%s XMAX > %.8f AND XMIN < %.8f"
                    " AND YMAX > %.8f AND YMIN < %.8f",
                    sql.c_str(),
                    m_sFilterEnvelope.MinX, m_sFilterEnvelope.MaxX,
                    m_sFilterEnvelope.MinY, m_sFilterEnvelope.MaxY );
        sql = sqlTmp;
    }
    /* If we have a spatial filter and GeomColum, query using st_intersects function */
    else if( m_poFilterGeom != NULL && pszGeomColumn )
    {
        if( pszQuery == NULL )
            sql += " WHERE";
        else
            sql += " AND";

        CPLString sqlTmp;
        sqlTmp.Printf(
                "%s st_intersects(st_geomfromtext('POLYGON((%.8f %.8f, %.8f %.8f, %.8f %.8f, %.8f %.8f, %.8f %.8f))',0),%s)",
                sql.c_str(),
                m_sFilterEnvelope.MinX, m_sFilterEnvelope.MinY,
                m_sFilterEnvelope.MaxX, m_sFilterEnvelope.MinY,
                m_sFilterEnvelope.MaxX, m_sFilterEnvelope.MaxY,
                m_sFilterEnvelope.MinX, m_sFilterEnvelope.MaxY,
                m_sFilterEnvelope.MinX, m_sFilterEnvelope.MinY, pszGeomColumn );
        sql = sqlTmp;
    }

    CPLDebug( "OGR_IDB", "Exec(%s)", sql.c_str() );
    if( poCurr->Prepare( sql.c_str() ) &&
        poCurr->Open(ITCursor::ReadOnly) )
    {
        return OGRERR_NONE;
    }
    else
    {
        delete poCurr;
        poCurr = NULL;
        return OGRERR_FAILURE;
    }
}

/************************************************************************/
/*                            ResetReading()                            */
/************************************************************************/

void OGRIDBTableLayer::ResetReading()

{
    ClearQuery();
    OGRIDBLayer::ResetReading();
}

/************************************************************************/
/*                             GetFeature()                             */
/************************************************************************/

OGRFeature *OGRIDBTableLayer::GetFeature( GIntBig nFeatureId )

{
    if( pszFIDColumn == NULL )
        return OGRIDBLayer::GetFeature( nFeatureId );

    ClearQuery();

    iNextShapeId = nFeatureId;

    poCurr = new ITCursor( *poDS->GetConnection() );

    // Create list of fields
    CPLString osFields;

    if ( poFeatureDefn->GetFieldIndex( pszFIDColumn ) == -1 )
        osFields += pszFIDColumn;

    if ( pszGeomColumn )
    {
        if ( ! osFields.empty() )
            osFields += ",";

        osFields += "st_asbinary(";
        osFields += pszGeomColumn;
        osFields += ") as ";
        osFields += pszGeomColumn;
    }

    for( int i = 0; i < poFeatureDefn->GetFieldCount(); i++ )
    {
        if ( ! osFields.empty() )
            osFields += ",";

        osFields += poFeatureDefn->GetFieldDefn(i)->GetNameRef();
    }

    CPLString sql;

    sql.Printf( "SELECT %s FROM %s WHERE %s = %d",
                osFields.c_str(), poFeatureDefn->GetName(),
                pszFIDColumn, nFeatureId );

    CPLDebug( "OGR_IDB", "ExecuteSQL(%s)", sql.c_str() );
    if( !poCurr->Prepare( sql.c_str() ) ||
        !poCurr->Open(ITCursor::ReadOnly) )
    {
        delete poCurr;
        poCurr = NULL;
        return NULL;
    }

    return GetNextRawFeature();
}

/************************************************************************/
/*                         SetAttributeFilter()                         */
/************************************************************************/

OGRErr OGRIDBTableLayer::SetAttributeFilter( const char *pszQuery )

{
    CPLFree(m_pszAttrQueryString);
    m_pszAttrQueryString = (pszQuery) ? CPLStrdup(pszQuery) : NULL;

    if( (pszQuery == NULL && this->pszQuery == NULL)
        || (pszQuery != NULL && this->pszQuery != NULL
            && EQUAL(pszQuery,this->pszQuery)) )
        return OGRERR_NONE;

    CPLFree( this->pszQuery );
    this->pszQuery = CPLStrdup( pszQuery );

    ClearQuery();

    return OGRERR_NONE;
}

/************************************************************************/
/*                           TestCapability()                           */
/************************************************************************/

int OGRIDBTableLayer::TestCapability( const char * pszCap )

{
    if( EQUAL(pszCap,OLCSequentialWrite) ||
        EQUAL(pszCap,OLCRandomWrite) )
        return bUpdateAccess;

    else if( EQUAL(pszCap,OLCRandomRead) )
        return TRUE;

    else
        return OGRIDBLayer::TestCapability( pszCap );
}

/************************************************************************/
/*                          GetFeatureCount()                           */
/*                                                                      */
/*      If a spatial filter is in effect, we turn control over to       */
/*      the generic counter.  Otherwise we return the total count.      */
/*      Eventually we should consider implementing a more efficient     */
/*      way of counting features matching a spatial query.              */
/************************************************************************/

GIntBig OGRIDBTableLayer::GetFeatureCount( int bForce )

{
    return OGRIDBLayer::GetFeatureCount( bForce );
}

/************************************************************************/
/*                           GetSpatialRef()                            */
/*                                                                      */
/*      We override this to try and fetch the table SRID from the       */
/*      geometry_columns table if the srsid is -2 (meaning we           */
/*      haven't yet even looked for it).                                */
/************************************************************************/

OGRSpatialReference *OGRIDBTableLayer::GetSpatialRef()

{
    if( nSRSId == -2 )
    {
        nSRSId = -1;

        if ( ! pszGeomColumn )
            return NULL;

        CPLString osCmd;
        osCmd.Printf( " SELECT FIRST 1 srid, trim(srtext)"
                      " FROM spatial_ref_sys, %s"
                      " WHERE srid = ST_Srid(%s) ",
                      poFeatureDefn->GetName(), pszGeomColumn );

        ITCursor oSridCur( *poDS->GetConnection() );

        if( oSridCur.Prepare( osCmd.c_str() )&&
            oSridCur.Open( ITCursor::ReadOnly ) )
        {
            ITRow * row = static_cast<ITRow *>( oSridCur.NextRow() );
            if ( row && ! row->IsNull() )
            {
                nSRSId = atoi(row->Column(0)->Printable());
                const char * wkt = row->Column(1)->Printable();

                if ( poSRS )
                {
                    // Hmm ... it should be null
                    delete poSRS;
                }
                poSRS = new OGRSpatialReference();
                if ( poSRS->importFromWkt( (char **)&wkt ) != OGRERR_NONE )
                {
                    CPLError( CE_Warning, CPLE_AppDefined,
                              "Error parse srs wkt: %s", wkt );
                    delete poSRS;
                    poSRS = NULL;
                }
            }
        }
    }

    return OGRIDBLayer::GetSpatialRef();
}

#if 0
OGRErr OGRIDBTableLayer::ISetFeature( OGRFeature *poFeature )
{
    OGRErr eErr(OGRERR_FAILURE);

    if ( ! bUpdateAccess )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "Error update feature. Layer is read only." );
        return eErr;
    }

    if( NULL == poFeature )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "NULL pointer to OGRFeature passed to SetFeature()." );
        return eErr;
    }

    if( poFeature->GetFID() == OGRNullFID )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "FID required on features given to SetFeature()." );
        return eErr;
    }

    ITStatement oQuery( *poDS->GetConnection() );

    int bUpdateGeom = TRUE;
    OGRwkbGeometryType nGeomType = poFeature->GetGeometryRef()->getGeometryType();
    CPLString osGeomFunc;
    int nSrid = 0; // FIXME Obtain geometry SRID

    switch (nGeomType)
    {
        case wkbPoint:
            osGeomFunc = "ST_PointFromText";
            break;
        case wkbLineString:
            osGeomFunc = "ST_LineFromText";
            break;
        case wkbPolygon:
            osGeomFunc = "ST_PolyFromText";
            break;
        case wkbMultiPoint:
            osGeomFunc = "ST_MPointFromText";
            break;
        case wkbMultiLineString:
            osGeomFunc = "ST_MLineFromText";
            break;
        case wkbMultiPolygon:
            osGeomFunc = "ST_MPolyFromText";
            break;
        default:
            bUpdateGeom = FALSE;
            CPLDebug("OGR_IDB", "SetFeature(): Unknown geometry type. Geometry will not be updated.");
    }

    // Create query
    CPLString osSql;
    CPLString osFields;

    if ( pszGeomColumn && bUpdateGeom )
    {
        osFields.Printf( "%s = %s( ?, %d )", pszGeomColumn, osGeomFunc.c_str(), nSrid );
    }

    for( int i = 0; i < poFeatureDefn->GetFieldCount(); i++ )
    {
        const char * pszFieldName = poFeatureDefn->GetFieldDefn(i)->GetNameRef();

        // skip fid column from update
        if ( EQUAL( pszFIDColumn, pszFieldName ) )
            continue;

        if ( ! osFields.empty() )
        {
            osFields += ",";
        }

        osFields += pszFieldName;
        osFields += "=?";
    }

    osSql.Printf( "UPDATE %s SET %s WHERE %s = %d",
                  poFeatureDefn->GetName(),
                  osFields.c_str(),
                  pszFIDColumn,
                  poFeature->GetFID() );

    if ( ! oQuery.Prepare( osSql.c_str() ) )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "Error prepare SQL.\n%s",osSql.c_str() );
        return eErr;
    }

    int iParam = 0;

    if ( pszGeomColumn && bUpdateGeom )
    {
        ITValue * par = oQuery.Param( iParam ); // it should be a geom value
        if ( ! par )
        {
            CPLError( CE_Failure, CPLE_AppDefined,
                      "Error prepare geom param");
            return eErr;
        }

        OGRGeometry * poGeom = poFeature->GetGeometryRef();
        char * wkt;
        poGeom->exportToWkt( &wkt );

        if( ! par->FromPrintable( wkt ) )
        {
            CPLError( CE_Failure, CPLE_AppDefined,
                      "Error prepare geom param");
            par->Release();
            return eErr;
        }

        CPLFree( wkt );
        par->Release();

        iParam++;
    }

    for ( int i = 0; i < poFeatureDefn->GetFieldCount(); i++ )
    {
        ITValue * par = oQuery.Param( iParam );
        if ( ! par )
        {
            CPLError( CE_Failure, CPLE_AppDefined,
                      "Error prepare param %d", iParam);
            return eErr;
        }

        if ( ! poFeature->IsFieldSetAndNotNull( i ) )
        {
            if ( ! par->SetNull() )
            {
                CPLError( CE_Failure, CPLE_AppDefined,
                        "Error set param %d to NULL", iParam);
                par->Release();
                return eErr;
            }
            par->Release();
            continue;
        }

        ITConversions * cv = 0;
        bool res = FALSE;

        if ( par->QueryInterface( ITConversionsIID, (void **) &cv) !=
             IT_QUERYINTERFACE_SUCCESS )
        {
            CPLError( CE_Failure, CPLE_AppDefined,
                    "Error prepare param %d", iParam);
            par->Release();
            return eErr;
        }

        switch ( poFeatureDefn->GetFieldDefn( i )->GetType() )
        {
            case OFTInteger:
                res = cv->ConvertFrom( poFeature->GetFieldAsInteger( i ) );
                break;

            case OFTReal:
                res = cv->ConvertFrom( poFeature->GetFieldAsDouble( i ) );
                break;

            case OFTIntegerList:
            case OFTRealList:
            case OFTStringList:
                // FIXME Prepare array of values field
                //cv->ConvertFrom( poFeature->GetFieldAsStringList( i ) );
                //break;

            case OFTBinary:
                // FIXME Prepare binary field

            case OFTString:
            case OFTDate:
            case OFTTime:
            case OFTDateTime:
                res = cv->ConvertFrom( poFeature->GetFieldAsString( i ) );
                break;
            default:
                CPLError( CE_Failure, CPLE_AppDefined,
                        "Error prepare param %d. Unknown data type.", iParam);
                cv->Release();
                par->Release();
                return eErr;
        }
        if ( res != TRUE )
            CPLError( CE_Failure, CPLE_AppDefined,
                      "Error prepare param.");
        cv->Release();
        par->Release();
    }

    CPLDebug( "OGR_IDB", "ExecuteSQL(%s)", oQuery.QueryText().Data() );
    if( !oQuery.Exec() )
    {
        CPLError( CE_Failure, CPLE_AppDefined, "Error update Feature.");
        return eErr;
    }

    return OGRERR_NONE;
}

#endif

OGRErr OGRIDBTableLayer::ISetFeature( OGRFeature *poFeature )
{
    OGRErr eErr(OGRERR_FAILURE);

    if ( ! bUpdateAccess )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "Error update feature. Layer is read only." );
        return eErr;
    }

    if( NULL == poFeature )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "NULL pointer to OGRFeature passed to SetFeature()." );
        return eErr;
    }

    if( poFeature->GetFID() == OGRNullFID )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "FID required on features given to SetFeature()." );
        return eErr;
    }

    ITStatement oQuery( *poDS->GetConnection() );

    int bUpdateGeom = TRUE;
    CPLString osGeomFunc;

    if ( poFeature->GetGeometryRef() )
    {
        OGRwkbGeometryType nGeomType = poFeature->GetGeometryRef()->getGeometryType();

        switch (nGeomType)
        {
            case wkbPoint:
                osGeomFunc = "ST_PointFromText";
                break;
            case wkbLineString:
                osGeomFunc = "ST_LineFromText";
                break;
            case wkbPolygon:
                osGeomFunc = "ST_PolyFromText";
                break;
            case wkbMultiPoint:
                osGeomFunc = "ST_MPointFromText";
                break;
            case wkbMultiLineString:
                osGeomFunc = "ST_MLineFromText";
                break;
            case wkbMultiPolygon:
                osGeomFunc = "ST_MPolyFromText";
                break;
            default:
                bUpdateGeom = FALSE;
                CPLDebug("OGR_IDB", "SetFeature(): Unknown geometry type. Geometry will not be updated.");
        }
    }
    else
    {
        bUpdateGeom = FALSE;
    }

    // Create query
    CPLString osSql;
    CPLString osFields;

    if ( pszGeomColumn && bUpdateGeom )
    {
        OGRGeometry * poGeom = poFeature->GetGeometryRef();
        char * wkt;
        poGeom->exportToWkt( &wkt );

        osFields.Printf( "%s = %s( '%s', %d )", pszGeomColumn, osGeomFunc.c_str(), wkt, nSRSId );

        CPLFree( wkt );
    }

    for( int i = 0; i < poFeatureDefn->GetFieldCount(); i++ )
    {
        const char * pszFieldName = poFeatureDefn->GetFieldDefn(i)->GetNameRef();

        // skip fid column from update
        if ( EQUAL( pszFIDColumn, pszFieldName ) )
            continue;

        if ( ! osFields.empty() )
        {
            osFields += ",";
        }

        osFields += pszFieldName;
        osFields += "=";

        if ( ! poFeature->IsFieldSetAndNotNull( i ) )
        {
            osFields += "NULL";
            continue;
        }

        CPLString osVal;

        switch ( poFeatureDefn->GetFieldDefn( i )->GetType() )
        {
            case OFTInteger:
                osVal.Printf( "%d", poFeature->GetFieldAsInteger( i ) );
                break;

            case OFTReal:
                if ( poFeatureDefn->GetFieldDefn( i )->GetPrecision() )
                {
                    // have a decimal format width.precision
                    CPLString osFormatString;
                    osFormatString.Printf( "%%%d.%df",
                                           poFeatureDefn->GetFieldDefn( i )->GetWidth(),
                                           poFeatureDefn->GetFieldDefn( i )->GetPrecision() );
                    osVal.Printf( osFormatString.c_str(), poFeature->GetFieldAsDouble( i ) );
                }
                else
                    osVal.Printf( "%f", poFeature->GetFieldAsDouble( i ) );
                break;

            case OFTIntegerList:
            case OFTRealList:
            case OFTStringList:
                // FIXME Prepare array of values field
                //cv->ConvertFrom( poFeature->GetFieldAsStringList( i ) );
                //break;

            case OFTBinary:
                // FIXME Prepare binary field

            case OFTString:
            case OFTDate:
            case OFTTime:
            case OFTDateTime:
            default:
                osVal.Printf( "'%s'", poFeature->GetFieldAsString( i ) );
                break;
        }
        osFields += osVal;
    }

    osSql.Printf( "UPDATE %s SET %s WHERE %s = %d",
                  poFeatureDefn->GetName(),
                  osFields.c_str(),
                  pszFIDColumn,
                  poFeature->GetFID() );

    if ( ! oQuery.Prepare( osSql.c_str() ) )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "Error prepare SQL.\n%s",osSql.c_str() );
        return eErr;
    }

    CPLDebug( "OGR_IDB", "Exec(%s)", oQuery.QueryText().Data() );
    if( !oQuery.Exec() )
    {
        CPLError( CE_Failure, CPLE_AppDefined, "Error update Feature.");
        return eErr;
    }

    return OGRERR_NONE;
}

OGRErr OGRIDBTableLayer::ICreateFeature( OGRFeature *poFeature )
{
    OGRErr eErr(OGRERR_FAILURE);

    if ( ! bUpdateAccess )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "Error create feature. Layer is read only." );
        return eErr;
    }

    if( NULL == poFeature )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "NULL pointer to OGRFeature passed to CreateFeature()." );
        return eErr;
    }

    if( poFeature->GetFID() != OGRNullFID && pszFIDColumn == NULL )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "FID ignored on feature given to CreateFeature(). Unknown FID column." );
        return eErr;
    }

    int bUpdateGeom = TRUE;
    CPLString osGeomFunc;

    if ( poFeature->GetGeometryRef() )
    {
        OGRwkbGeometryType nGeomType = poFeature->GetGeometryRef()->getGeometryType();

        switch (nGeomType)
        {
            case wkbPoint:
                osGeomFunc = "ST_PointFromText";
                break;
            case wkbLineString:
                osGeomFunc = "ST_LineFromText";
                break;
            case wkbPolygon:
                osGeomFunc = "ST_PolyFromText";
                break;
            case wkbMultiPoint:
                osGeomFunc = "ST_MPointFromText";
                break;
            case wkbMultiLineString:
                osGeomFunc = "ST_MLineFromText";
                break;
            case wkbMultiPolygon:
                osGeomFunc = "ST_MPolyFromText";
                break;
            default:
                bUpdateGeom = FALSE;
                CPLDebug("OGR_IDB", "SetFeature(): Unknown geometry type. Geometry will not be updated.");
        }
    }
    else
        bUpdateGeom = FALSE;

    // Create query
    CPLString osSql;
    CPLString osFields;
    CPLString osValues;

    if ( pszGeomColumn && bUpdateGeom )
    {
        OGRGeometry * poGeom = poFeature->GetGeometryRef();
        char * wkt;
        poGeom->exportToWkt( &wkt );

        osFields += pszGeomColumn;
        osValues.Printf( "%s( '%s', %d )", osGeomFunc.c_str(), wkt, nSRSId );

        CPLFree( wkt );
    }

    for( int i = 0; i < poFeatureDefn->GetFieldCount(); i++ )
    {
        const char * pszFieldName = poFeatureDefn->GetFieldDefn(i)->GetNameRef();

        // Skip NULL fields
        if ( ! poFeature->IsFieldSetAndNotNull( i ) )
        {
            continue;
        }

        if ( ! osFields.empty() )
        {
            osFields += ",";
            osValues += ",";
        }

        osFields += pszFieldName;

        CPLString osVal;

        switch ( poFeatureDefn->GetFieldDefn( i )->GetType() )
        {
            case OFTInteger:
                osVal.Printf( "%d", poFeature->GetFieldAsInteger( i ) );
                break;

            case OFTReal:
                if ( poFeatureDefn->GetFieldDefn( i )->GetPrecision() )
                {
                    // have a decimal format width.precision
                    CPLString osFormatString;
                    osFormatString.Printf( "%%%d.%df",
                                           poFeatureDefn->GetFieldDefn( i )->GetWidth(),
                                           poFeatureDefn->GetFieldDefn( i )->GetPrecision() );
                    osVal.Printf( osFormatString.c_str(), poFeature->GetFieldAsDouble( i ) );
                }
                else
                    osVal.Printf( "%f", poFeature->GetFieldAsDouble( i ) );
                break;

            case OFTIntegerList:
            case OFTRealList:
            case OFTStringList:
                // FIXME Prepare array of values field
                //cv->ConvertFrom( poFeature->GetFieldAsStringList( i ) );
                //break;

            case OFTBinary:
                // FIXME Prepare binary field

            case OFTString:
            case OFTDate:
            case OFTTime:
            case OFTDateTime:
            default:
                osVal.Printf( "'%s'", poFeature->GetFieldAsString( i ) );
                break;
        }
        osValues += osVal;
    }

    osSql.Printf( "INSERT INTO %s (%s) VALUES (%s)",
                  poFeatureDefn->GetName(),
                  osFields.c_str(),
                  osValues.c_str() );

    ITStatement oQuery( *poDS->GetConnection() );

    if ( ! oQuery.Prepare( osSql.c_str() ) )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "Error prepare SQL.\n%s",osSql.c_str() );
        return eErr;
    }

    CPLDebug( "OGR_IDB", "Exec(%s)", oQuery.QueryText().Data() );
    if( !oQuery.Exec() )
    {
        CPLError( CE_Failure, CPLE_AppDefined, "Error create Feature.");
        return eErr;
    }

    ITQuery oFidQuery( *poDS->GetConnection() );
    osSql.Printf( "SELECT MAX(%s) from %s", pszFIDColumn, poFeatureDefn->GetName() );

    CPLDebug( "OGR_IDB", "Exec(%s)", osSql.c_str() );

    ITRow * row = oFidQuery.ExecOneRow( osSql.c_str() );
    if( ! row || row->NumColumns() < 1 )
    {
        CPLError( CE_Failure, CPLE_AppDefined, "Error create Feature.");
        return eErr;
    }

    int fid = atoi( row->Column(0)->Printable() );

    if ( fid > 0 )
        poFeature->SetFID( fid );
    else
    {
        CPLError( CE_Failure, CPLE_AppDefined, "Error create Feature. Unable to get new fid" );
        return eErr;
    }

    return OGRERR_NONE;
}
