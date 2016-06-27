/******************************************************************************
 * $Id$
 *
 * Project:  GeoPackage Translator
 * Purpose:  Implements OGRGeoPackageLayer class
 * Author:   Paul Ramsey <pramsey@boundlessgeo.com>
 *
 ******************************************************************************
 * Copyright (c) 2013, Paul Ramsey <pramsey@boundlessgeo.com>
 * Copyright (c) 2014, Even Rouault <even dot rouault at mines-paris dot org>
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

#include "ogr_geopackage.h"
#include "ogrgeopackageutility.h"

//----------------------------------------------------------------------
// SaveExtent()
// 
// Write the current contents of the layer envelope down to the
// gpkg_contents metadata table.
//
OGRErr OGRGeoPackageLayer::SaveExtent()
{
    if ( !m_poDS->IsUpdatable() || ! m_bExtentChanged || ! m_poExtent ) 
        return OGRERR_NONE;

    sqlite3* poDb = m_poDS->GetDatabaseHandle();

    if ( ! poDb ) return OGRERR_FAILURE;

    char *pszSQL = sqlite3_mprintf(
                "UPDATE gpkg_contents SET "
                "min_x = %g, min_y = %g, "
                "max_x = %g, max_y = %g "
                "WHERE table_name = '%q' AND "
                "Lower(data_type) = 'features'",
                m_poExtent->MinX, m_poExtent->MinY,
                m_poExtent->MaxX, m_poExtent->MaxY,
                m_pszTableName);

    OGRErr err = SQLCommand(poDb, pszSQL);
    sqlite3_free(pszSQL);
    m_bExtentChanged = FALSE;
    
    return err;
}

//----------------------------------------------------------------------
// UpdateExtent()
// 
// Expand the layer envelope if necessary to reflect the bounds
// of new features being added to the layer.
//
OGRErr OGRGeoPackageLayer::UpdateExtent( const OGREnvelope *poExtent )
{
    if ( ! m_poExtent )
    {
        m_poExtent = new OGREnvelope( *poExtent );
    }
    m_poExtent->Merge( *poExtent );
    m_bExtentChanged = TRUE;
    return OGRERR_NONE;
}

//----------------------------------------------------------------------
// BuildColumns()
// 
// Save a list of columns (fid, geometry, attributes) suitable
// for use in a SELECT query that retrieves all fields.
//
OGRErr OGRGeoPackageLayer::BuildColumns()
{
    if ( ! m_poFeatureDefn || ! m_pszFidColumn )
    {
        return OGRERR_FAILURE;
    }

    /* Always start with a primary key */
    CPLString soColumns = m_pszFidColumn;
    CPLString soColumn;

    /* Add a geometry column if there is one (just one) */
    if ( m_poFeatureDefn->GetGeomFieldCount() )
    {
        soColumns += ", ";
        soColumn.Printf("\"%s\"", m_poFeatureDefn->GetGeomFieldDefn(0)->GetNameRef());
        soColumns += soColumn;
    }

    /* Add all the attribute columns */
    for( int i = 0; i < m_poFeatureDefn->GetFieldCount(); i++ )
    {
        soColumns += ", ";
        soColumn.Printf("\"%s\"", m_poFeatureDefn->GetFieldDefn(i)->GetNameRef());
        soColumns += soColumn;
    }

    m_soColumns = soColumns;    
    return OGRERR_NONE;    
}

//----------------------------------------------------------------------
// ReadFeature()
// 
// Convert a row in a statement into an OGRFeature.
//
OGRErr OGRGeoPackageLayer::ReadFeature( sqlite3_stmt *poQuery, OGRFeature **ppoFeature )
{
    int iColOffset = 0;
    
    if ( ! m_poFeatureDefn )
        return OGRERR_FAILURE;

    OGRFeature *poFeature = new OGRFeature( m_poFeatureDefn );
    
    /* Primary key is always first column in our SQL call */
    poFeature->SetFID(sqlite3_column_int(poQuery, iColOffset++));
    
    /* If a geometry column exists, it's next */
    /* Add a geometry column if there is one (just the first one) */
    if ( m_poFeatureDefn->GetGeomFieldCount() )
    {
        if ( sqlite3_column_type(poQuery, iColOffset) != SQLITE_NULL )
        {
            OGRSpatialReference* poSrs = m_poFeatureDefn->GetGeomFieldDefn(0)->GetSpatialRef();
            int iGpkgSize = sqlite3_column_bytes(poQuery, iColOffset);
            if( iGpkgSize >= 8 )
            {
                GByte *pabyGpkg = (GByte *)sqlite3_column_blob(poQuery, iColOffset);
                OGRGeometry *poGeom = GPkgGeometryToOGR(pabyGpkg, iGpkgSize, poSrs);
                if ( ! poGeom )
                {
                    delete poFeature;
                    CPLError( CE_Failure, CPLE_AppDefined, "Unable to read geometry");
                    return OGRERR_FAILURE;
                }
                poFeature->SetGeometryDirectly( poGeom );
            }
        }
        iColOffset++;
    }
    
    /* Read all the attribute columns */
    for( int i = 0; i < m_poFeatureDefn->GetFieldCount(); i++ )
    {
        int j = iColOffset+i;

        switch(sqlite3_column_type(poQuery, j))
        {
            case SQLITE_INTEGER:
            {
                int iVal = sqlite3_column_int(poQuery, j);
                poFeature->SetField(i, iVal);
                break;
            }
            case SQLITE_FLOAT:
            {
                double dVal = sqlite3_column_double(poQuery, j);
                poFeature->SetField(i, dVal);
                break;
            }
            case SQLITE_BLOB:
            {
                int iBlobSize = sqlite3_column_bytes(poQuery, j);
                GByte *pabyBlob = (GByte *)sqlite3_column_blob(poQuery, j);
                poFeature->SetField(i, iBlobSize, pabyBlob);
                break;
            }
            case SQLITE_TEXT:
            {
                const char *pszVal = (const char *)sqlite3_column_text(poQuery, j);
                poFeature->SetField(i, pszVal);
                break;
            }
            default: /* SQLITE_NULL */
            {
                /* Do nothing for NULL fields */
                break;
            }
        }
    }
    
    /* Pass result back to the caller */
    *ppoFeature = poFeature;

    return OGRERR_NONE;
}



//----------------------------------------------------------------------
// IsGeomFieldSet()
// 
// Utility method to determine if there is a non-Null geometry
// in an OGRGeometry.
//
OGRBoolean OGRGeoPackageLayer::IsGeomFieldSet( OGRFeature *poFeature )
{
    if ( poFeature->GetDefnRef()->GetGeomFieldCount() && 
         poFeature->GetGeomFieldRef(0) )
    {
        return TRUE;        
    }
    else
    {
        return FALSE;
    }
}

OGRErr OGRGeoPackageLayer::FeatureBindParameters( OGRFeature *poFeature, sqlite3_stmt *poStmt, int *pnColCount )
{
    int nColCount = 1;
    int err;
    
    if ( ! (poFeature && poStmt && pnColCount) )
        return OGRERR_FAILURE;

    OGRFeatureDefn *poFeatureDefn = poFeature->GetDefnRef();
    
    /* Bind data values to the statement, here bind the blob for geometry */
    if ( poFeatureDefn->GetGeomFieldCount() )
    {
        GByte *pabyWkb = NULL;

        /* Non-NULL geometry */
        if ( poFeature->GetGeomFieldRef(0) )
        {
            size_t szWkb;
            pabyWkb = GPkgGeometryFromOGR(poFeature->GetGeomFieldRef(0), m_iSrs, &szWkb);
            err = sqlite3_bind_blob(poStmt, nColCount++, pabyWkb, szWkb, CPLFree);
        }
        /* NULL geometry */
        else
        {
            err = sqlite3_bind_null(poStmt, nColCount++);
        }
        if ( err != SQLITE_OK )
        {
            if ( pabyWkb )
                CPLFree(pabyWkb);
            CPLError( CE_Failure, CPLE_AppDefined,
                      "failed to bind geometry to statement");        
            return OGRERR_FAILURE;            
        }
    }

    /* Bind the attributes using appropriate SQLite data types */
    for( int i = 0; i < poFeatureDefn->GetFieldCount(); i++ )
    {
        OGRFieldDefn *poFieldDefn = poFeatureDefn->GetFieldDefn(i); 
        
        if( poFeature->IsFieldSet(i) )
        {
            switch(SQLiteFieldFromOGR(poFieldDefn->GetType()))
            {
                case SQLITE_INTEGER:
                {
                    err = sqlite3_bind_int(poStmt, nColCount++, poFeature->GetFieldAsInteger(i));
                    break;
                }
                case SQLITE_FLOAT:
                {
                    err = sqlite3_bind_double(poStmt, nColCount++, poFeature->GetFieldAsDouble(i));
                    break;
                }
                case SQLITE_BLOB:
                {
                    int szBlob;
                    GByte *pabyBlob = poFeature->GetFieldAsBinary(i, &szBlob);
                    err = sqlite3_bind_blob(poStmt, nColCount++, pabyBlob, szBlob, NULL);
                    break;
                }
                default:
                {
                    const char *pszVal = poFeature->GetFieldAsString(i);
                    err = sqlite3_bind_text(poStmt, nColCount++, pszVal, strlen(pszVal), SQLITE_TRANSIENT);
                    break;
                }            
            }            
        }
        else
        {
            err = sqlite3_bind_null(poStmt, nColCount++);
        }
    }
    
    *pnColCount = nColCount;
    return OGRERR_NONE;
}

//----------------------------------------------------------------------
// FeatureBindUpdateParameters()
// 
// Selectively bind the values of an OGRFeature to a prepared 
// statement, prior to execution. Carefully binds exactly the 
// same parameters that have been set up by FeatureGenerateUpdateSQL()
// as bindable.
//
OGRErr OGRGeoPackageLayer::FeatureBindUpdateParameters( OGRFeature *poFeature, sqlite3_stmt *poStmt )
{

    int nColCount;
    OGRErr err = FeatureBindParameters( poFeature, poStmt, &nColCount );
    if ( err != OGRERR_NONE )
        return err;

    /* Bind the FID to the "WHERE" clause */
    err = sqlite3_bind_int(poStmt, nColCount, poFeature->GetFID());    
    if ( err != SQLITE_OK )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "failed to bind FID '%ld' to statement", poFeature->GetFID());
        return OGRERR_FAILURE;       
    }
    
    return OGRERR_NONE;
}


//----------------------------------------------------------------------
// FeatureBindInsertParameters()
// 
// Selectively bind the values of an OGRFeature to a prepared 
// statement, prior to execution. Carefully binds exactly the 
// same parameters that have been set up by FeatureGenerateInsertSQL()
// as bindable.
//
OGRErr OGRGeoPackageLayer::FeatureBindInsertParameters( OGRFeature *poFeature, sqlite3_stmt *poStmt )
{    
    int nColCount;
    return FeatureBindParameters( poFeature, poStmt, &nColCount );
}   


//----------------------------------------------------------------------
// FeatureGenerateInsertSQL()
// 
// Build a SQL INSERT statement that references all the columns in
// the OGRFeatureDefn, then prepare it for repeated use in a prepared
// statement. All statements start off with geometry (if it exists)
// then reference each column in the order it appears in the OGRFeatureDefn.
// FeatureBindParameters operates on the expectation of this
// column ordering.
//
CPLString OGRGeoPackageLayer::FeatureGenerateInsertSQL( OGRFeature *poFeature )
{
    OGRBoolean bNeedComma = FALSE;
    OGRFeatureDefn *poFeatureDefn = poFeature->GetDefnRef();

    /* Set up our SQL string basics */
    CPLString osSQLFront;
    osSQLFront.Printf("INSERT INTO %s ( ", m_pszTableName);

    CPLString osSQLBack;
    osSQLBack = ") VALUES (";
    
    CPLString osSQLColumn;
    
    if ( poFeatureDefn->GetGeomFieldCount() )
    {
        osSQLColumn.Printf("\"%s\"", poFeatureDefn->GetGeomFieldDefn(0)->GetNameRef());
        osSQLFront += osSQLColumn;
        osSQLBack += "?";
        bNeedComma = TRUE;
    }

    /* Add attribute column names (except FID) to the SQL */
    for( int i = 0; i < poFeatureDefn->GetFieldCount(); i++ )
    {
        if( !bNeedComma )
        {
            bNeedComma = TRUE;
        }
        else 
        {
            osSQLFront += ", ";
            osSQLBack += ", ";
        }

        osSQLColumn.Printf("\"%s\"", poFeatureDefn->GetFieldDefn(i)->GetNameRef());
        osSQLFront += osSQLColumn;
        osSQLBack += "?";        
    }
    
    osSQLBack += ")";

    return osSQLFront + osSQLBack;
}


//----------------------------------------------------------------------
// FeatureGenerateUpdateSQL()
// 
// Build a SQL UPDATE statement that references all the columns in
// the OGRFeatureDefn, then prepare it for repeated use in a prepared
// statement. All statements start off with geometry (if it exists)
// then reference each column in the order it appears in the OGRFeatureDefn.
// FeatureBindParameters operates on the expectation of this
// column ordering.

//
CPLString OGRGeoPackageLayer::FeatureGenerateUpdateSQL( OGRFeature *poFeature )
{
    OGRBoolean bNeedComma = FALSE;
    OGRFeatureDefn *poFeatureDefn = poFeature->GetDefnRef();

    /* Set up our SQL string basics */
    CPLString osUpdate;
    osUpdate.Printf("UPDATE %s SET ", m_pszTableName);
    
    CPLString osSQLColumn;
    
    if ( poFeatureDefn->GetGeomFieldCount() > 0 )
    {
        osSQLColumn.Printf("\"%s\"", poFeatureDefn->GetGeomFieldDefn(0)->GetNameRef());
        osUpdate += osSQLColumn;
        osUpdate += "=?";
        bNeedComma = TRUE;
    }

    /* Add attribute column names (except FID) to the SQL */
    for( int i = 0; i < poFeatureDefn->GetFieldCount(); i++ )
    {
        if( !bNeedComma )
            bNeedComma = TRUE;
        else 
            osUpdate += ", ";

        osSQLColumn.Printf("\"%s\"", poFeatureDefn->GetFieldDefn(i)->GetNameRef());
        osUpdate += osSQLColumn;
        osUpdate += "=?";
    }
    
    CPLString osWhere;
    osWhere.Printf(" WHERE %s = ?", m_pszFidColumn);

    return osUpdate + osWhere;
}


//----------------------------------------------------------------------
// ReadTableDefinition()
// 
// Initialization routine. Read all the metadata about a table, 
// starting from just the table name. Reads information from GPKG
// metadata tables and from SQLite table metadata. Uses it to 
// populate OGRSpatialReference information and OGRFeatureDefn objects, 
// among others.
//
OGRErr OGRGeoPackageLayer::ReadTableDefinition()
{
    OGRErr err;
    SQLResult oResultTable;
    SQLResult oResultContents;
    SQLResult oResultGeomCols;
    char* pszSQL;
    OGRBoolean bReadExtent = FALSE;
    sqlite3* poDb = m_poDS->GetDatabaseHandle();

    /* Check that the table name is registered in gpkg_contents */
    pszSQL = sqlite3_mprintf(
                "SELECT table_name, data_type, identifier, "
                "description, min_x, min_y, max_x, max_y, srs_id "
                "FROM gpkg_contents "
                "WHERE table_name = '%q' AND "
                "Lower(data_type) = 'features'",
                m_pszTableName);
                
    err = SQLQuery(poDb, pszSQL, &oResultContents);
    sqlite3_free(pszSQL);
    
    /* gpkg_contents query has to work */
    /* gpkg_contents.table_name is supposed to be unique */
    if ( err != OGRERR_NONE || oResultContents.nRowCount != 1 )
    {
        if ( err != OGRERR_NONE )
            CPLError( CE_Failure, CPLE_AppDefined, "%s", oResultContents.pszErrMsg );
        else if ( oResultContents.nRowCount != 1 )
            CPLError( CE_Failure, CPLE_AppDefined, "layer '%s' is not registered in gpkg_contents", m_pszTableName );
        else
            CPLError( CE_Failure, CPLE_AppDefined, "error reading gpkg_contents" );
            
        SQLResultFree(&oResultContents);
        return OGRERR_FAILURE;
    }

    const char *pszMinX = SQLResultGetValue(&oResultContents, 4, 0);
    const char *pszMinY = SQLResultGetValue(&oResultContents, 5, 0);
    const char *pszMaxX = SQLResultGetValue(&oResultContents, 6, 0);
    const char *pszMaxY = SQLResultGetValue(&oResultContents, 7, 0);
    
	/* All the extrema have to be non-NULL for this to make sense */
    OGREnvelope oExtent;
    if ( pszMinX && pszMinY && pszMaxX && pszMaxY )
    {
        oExtent.MinX = atof(pszMinX);
        oExtent.MinY = atof(pszMinY);
        oExtent.MaxX = atof(pszMaxX);
        oExtent.MaxY = atof(pszMaxY);
        bReadExtent = TRUE;
    }

    /* Done with info from gpkg_contents now */
    SQLResultFree(&oResultContents);

    /* Check that the table name is registered in gpkg_geometry_columns */
    pszSQL = sqlite3_mprintf(
                "SELECT table_name, column_name, "
                "geometry_type_name, srs_id, z "
                "FROM gpkg_geometry_columns "
                "WHERE table_name = '%q'",
                m_pszTableName);
                
    err = SQLQuery(poDb, pszSQL, &oResultGeomCols);
    sqlite3_free(pszSQL);

    /* gpkg_geometry_columns query has to work */
    /* gpkg_geometry_columns.table_name is supposed to be unique */
    if ( err != OGRERR_NONE || oResultGeomCols.nRowCount != 1 )
    {
        CPLError( CE_Failure, CPLE_AppDefined, "%s", oResultGeomCols.pszErrMsg );
        SQLResultFree(&oResultGeomCols);
        return OGRERR_FAILURE;
    }

    /* Use the "PRAGMA TABLE_INFO()" call to get table definition */
    /*  #|name|type|nullable|default|pk */
    /*  0|id|integer|0||1 */
    /*  1|name|varchar|0||0 */    
    pszSQL = sqlite3_mprintf("pragma table_info('%q')", m_pszTableName);
    err = SQLQuery(poDb, pszSQL, &oResultTable);
    sqlite3_free(pszSQL);

    if ( err != OGRERR_NONE || oResultTable.nRowCount == 0 )
    {
        if( oResultTable.pszErrMsg != NULL )
            CPLError( CE_Failure, CPLE_AppDefined, "%s", oResultTable.pszErrMsg );
        else
            CPLError( CE_Failure, CPLE_AppDefined, "Cannot find table %s", m_pszTableName );

        SQLResultFree(&oResultGeomCols);
        SQLResultFree(&oResultTable);
        return OGRERR_FAILURE;
    }
    
    /* Populate feature definition from table description */
    m_poFeatureDefn = new OGRFeatureDefn( m_pszTableName );
    m_poFeatureDefn->SetGeomType(wkbNone);
    m_poFeatureDefn->Reference();
    
    const char *pszGeomColsType = SQLResultGetValue(&oResultGeomCols, 2, 0);
    int iSrsId = SQLResultGetValueAsInteger(&oResultGeomCols, 3, 0);
    int bHasZ = SQLResultGetValueAsInteger(&oResultGeomCols, 4, 0);
    int iRecord;
    OGRBoolean bFidFound = FALSE;
    m_iSrs = iSrsId;
    
    for ( iRecord = 0; iRecord < oResultTable.nRowCount; iRecord++ )
    {
        const char *pszName = SQLResultGetValue(&oResultTable, 1, iRecord);
        const char *pszType = SQLResultGetValue(&oResultTable, 2, iRecord);
        OGRBoolean bFid = SQLResultGetValueAsInteger(&oResultTable, 5, iRecord);
        OGRFieldType oType = GPkgFieldToOGR(pszType);

        /* Not a standard field type... */
        if ( oType > OFTMaxType )
        {
            /* Maybe it's a geometry type? */
            OGRwkbGeometryType oGeomType = GPkgGeometryTypeToWKB(pszType, bHasZ);
            if ( oGeomType != wkbNone )
            {
                OGRwkbGeometryType oGeomTypeGeomCols = GPkgGeometryTypeToWKB(pszGeomColsType, bHasZ);
                /* Enforce consistency between table and metadata */
                if ( oGeomType != oGeomTypeGeomCols )
                {
                    CPLError(CE_Failure, CPLE_AppDefined, 
                             "geometry column type in '%s.%s' is not consistent with type in gpkg_geometry_columns", 
                             m_pszTableName, pszName);
                    SQLResultFree(&oResultTable);
                    SQLResultFree(&oResultGeomCols);
                    return OGRERR_FAILURE;
                }
                
                if ( m_poFeatureDefn->GetGeomFieldCount() == 0 )
                {
                    OGRGeomFieldDefn oGeomField(pszName, oGeomType);
                    m_poFeatureDefn->AddGeomFieldDefn(&oGeomField);

                    /* Read the SRS */
                    OGRSpatialReference *poSRS = m_poDS->GetSpatialRef(iSrsId);
                    if ( poSRS )
                    {
                        m_poFeatureDefn->GetGeomFieldDefn(0)->SetSpatialRef(poSRS);
                        poSRS->Dereference();
                    }
                }
                else
                {
                    CPLError(CE_Failure, CPLE_AppDefined, 
                             "table '%s' has multiple geometry fields? not legal in gpkg", 
                             m_pszTableName);
                    SQLResultFree(&oResultTable);
                    SQLResultFree(&oResultGeomCols);
                    return OGRERR_FAILURE;
                }

            }
            else
            {
                // CPLError( CE_Failure, CPLE_AppDefined, "invalid field type '%s'", pszType );
                // SQLResultFree(&oResultTable);
                CPLError(CE_Warning, CPLE_AppDefined, 
                         "geometry column '%s' of type '%s' ignored", pszName, pszType);
            }
            
        }
        else
        {
            /* Is this the FID column? */
            if ( bFid )
            {
                bFidFound = TRUE;
                m_pszFidColumn = CPLStrdup(pszName);
            }
            else
            {
                OGRFieldDefn oField(pszName, oType);
                m_poFeatureDefn->AddFieldDefn(&oField);
            }
        }
    }

    /* Wait, we didn't find a FID? */
    /* Game over, all valid tables must have a FID */
    if ( ! bFidFound )
    {
        CPLError(CE_Failure, CPLE_AppDefined, 
                 "no primary key defined for table '%s'", m_pszTableName);
        return OGRERR_FAILURE;
        
    }

    if ( bReadExtent )
    {
        m_poExtent = new OGREnvelope(oExtent);
    }

    SQLResultFree(&oResultTable);
    SQLResultFree(&oResultGeomCols);

    /* Update the columns string */
    BuildColumns();

    return OGRERR_NONE;
}


/************************************************************************/
/*                      OGRGeoPackageLayer()                            */
/************************************************************************/

OGRGeoPackageLayer::OGRGeoPackageLayer(
                    OGRGeoPackageDataSource *poDS,
                    const char * pszTableName)
{
    m_pszTableName = CPLStrdup(pszTableName);
    m_pszFidColumn = NULL;
    m_iSrs = 0;
    m_poDS = poDS;
    m_poExtent = NULL;
    m_bExtentChanged = FALSE;
    m_poFeatureDefn = NULL;
    m_poQueryStatement = NULL;
    m_poUpdateStatement = NULL;
    m_poInsertStatement = NULL;
    m_poFidStatement = NULL;
    m_soColumns = "";
    m_soFilter = "";
}


/************************************************************************/
/*                      ~OGRGeoPackageLayer()                           */
/************************************************************************/

OGRGeoPackageLayer::~OGRGeoPackageLayer()
{
    /* Save metadata back to the database */
    SaveExtent();

    /* Clean up resources in memory */
    if ( m_pszTableName )
        CPLFree( m_pszTableName );
    
    if ( m_pszFidColumn )
        CPLFree( m_pszFidColumn );
    
    if ( m_poExtent )
        delete m_poExtent;
    
    if ( m_poQueryStatement )
        sqlite3_finalize(m_poQueryStatement);

    if ( m_poUpdateStatement )
        sqlite3_finalize(m_poUpdateStatement);

    if ( m_poInsertStatement )
        sqlite3_finalize(m_poInsertStatement);
    
    if ( m_poFidStatement )
        sqlite3_finalize(m_poFidStatement);
        
    if ( m_poFeatureDefn )
        m_poFeatureDefn->Release();
}


/************************************************************************/
/*                      CreateField()                                   */
/************************************************************************/

OGRErr OGRGeoPackageLayer::CreateField( OGRFieldDefn *poField,
                                        CPL_UNUSED int bApproxOK )
{
    if( !m_poDS->IsUpdatable() )
    {
        return OGRERR_FAILURE;
    }
    
    if ( ! m_poFeatureDefn || ! m_pszTableName )
    {
        CPLError(CE_Failure, CPLE_AppDefined, 
                 "feature definition or table name is null");
        return OGRERR_FAILURE;
    }

    OGRErr err = m_poDS->AddColumn(m_pszTableName, 
                                   poField->GetNameRef(),
                                   GPkgFieldFromOGR(poField->GetType()));

    if ( err != OGRERR_NONE )
        return err;
    
    m_poFeatureDefn->AddFieldDefn( poField );
    ResetReading();
    return OGRERR_NONE;
}


/************************************************************************/
/*                      CreateFeature()                                 */
/************************************************************************/

OGRErr OGRGeoPackageLayer::CreateFeature( OGRFeature *poFeature )
{
    if( !m_poDS->IsUpdatable() )
    {
        return OGRERR_FAILURE;
    }

    if ( ! m_poFeatureDefn || ! m_pszTableName )
    {
        CPLError(CE_Failure, CPLE_AppDefined, 
                 "feature definition or table name is null");
        return OGRERR_FAILURE;
    }

    if( NULL == poFeature )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "NULL pointer to OGRFeature passed to CreateFeature()" );
        return OGRERR_FAILURE;
    }

    if ( ! m_poInsertStatement ) 
    {
        /* Construct a SQL INSERT statement from the OGRFeature */
        /* Only work with fields that are set */
        /* Do not stick values into SQL, use placeholder and bind values later */    
        CPLString osCommand = FeatureGenerateInsertSQL(poFeature);
        
        /* Prepare the SQL into a statement */
        sqlite3 *poDb = m_poDS->GetDatabaseHandle();
        int err = sqlite3_prepare_v2(poDb, osCommand, -1, &m_poInsertStatement, NULL);
        if ( err != SQLITE_OK )
        {
            m_poInsertStatement = NULL;
            CPLError( CE_Failure, CPLE_AppDefined,
                      "failed to prepare SQL: %s", osCommand.c_str());        
            return OGRERR_FAILURE;
        }        
    }
    
    /* Bind values onto the statement now */
    OGRErr errOgr = FeatureBindInsertParameters(poFeature, m_poInsertStatement);
    if ( errOgr != OGRERR_NONE )
        return errOgr;

    /* From here execute the statement and check errors */
    int err = sqlite3_step(m_poInsertStatement);
    if ( ! (err == SQLITE_OK || err == SQLITE_DONE) )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "failed to execute insert");
        return OGRERR_FAILURE; 
    }

    sqlite3_reset(m_poInsertStatement);
    sqlite3_clear_bindings(m_poInsertStatement);

    /* Update the layer extents with this new object */
    if ( IsGeomFieldSet(poFeature) )
    {
        OGREnvelope oEnv;
        poFeature->GetGeomFieldRef(0)->getEnvelope(&oEnv);
        UpdateExtent(&oEnv);
    }

    /* Read the latest FID value */
    int iFid;
    if ( (iFid = sqlite3_last_insert_rowid(m_poDS->GetDatabaseHandle())) )
    {
        poFeature->SetFID(iFid);
    }
    else
    {
        poFeature->SetFID(OGRNullFID);
    }
    
    /* All done! */
	return OGRERR_NONE;
}


/************************************************************************/
/*                          SetFeature()                                */
/************************************************************************/

OGRErr OGRGeoPackageLayer::SetFeature( OGRFeature *poFeature )
{
    if( !m_poDS->IsUpdatable() )
    {
        return OGRERR_FAILURE;
    }
    
    if ( ! m_poFeatureDefn || ! m_pszTableName )
    {
        CPLError(CE_Failure, CPLE_AppDefined, 
                 "feature definition or table name is null");
        return OGRERR_FAILURE;
    }

    if( NULL == poFeature )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "NULL pointer to OGRFeature passed to CreateFeature()" );
        return OGRERR_FAILURE;
    }

    /* No FID? We can't set, we have to create */
    if ( poFeature->GetFID() == OGRNullFID )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "FID required on features given to SetFeature()." );
        return OGRERR_FAILURE;
    }

    if ( ! m_poUpdateStatement )
    {
        /* Construct a SQL INSERT statement from the OGRFeature */
        /* Only work with fields that are set */
        /* Do not stick values into SQL, use placeholder and bind values later */    
        CPLString osCommand = FeatureGenerateUpdateSQL(poFeature);

        /* Prepare the SQL into a statement */
        int err = sqlite3_prepare_v2(m_poDS->GetDatabaseHandle(), osCommand, -1, &m_poUpdateStatement, NULL);
        if ( err != SQLITE_OK )
        {
            CPLError( CE_Failure, CPLE_AppDefined,
                      "failed to prepare SQL: %s", osCommand.c_str());        
            return OGRERR_FAILURE;
        }
    }

    /* Bind values onto the statement now */
    OGRErr errOgr = FeatureBindUpdateParameters(poFeature, m_poUpdateStatement);
    if ( errOgr != OGRERR_NONE )
        return errOgr;

    /* From here execute the statement and check errors */
    int err = sqlite3_step(m_poUpdateStatement);
    if ( ! (err == SQLITE_OK || err == SQLITE_DONE) )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "failed to execute update");
        return OGRERR_FAILURE;       
    }

    sqlite3_reset(m_poUpdateStatement);
    sqlite3_clear_bindings(m_poUpdateStatement);

    /* Only update the envelope if we changed something */
    if (sqlite3_changes(m_poDS->GetDatabaseHandle()) )
    {
        /* Update the layer extents with this new object */
        if ( IsGeomFieldSet(poFeature) )
        {
            OGREnvelope oEnv;
            poFeature->GetGeomFieldRef(0)->getEnvelope(&oEnv);
            UpdateExtent(&oEnv);
        }
    }

    /* All done! */
	return OGRERR_NONE;
}



/************************************************************************/
/*                         SetAttributeFilter()                         */
/************************************************************************/

OGRErr OGRGeoPackageLayer::SetAttributeFilter( const char *pszQuery )

{
    if( pszQuery == NULL )
        m_soFilter.Clear();
    else
        m_soFilter = pszQuery;

    ResetReading();
    return OGRERR_NONE;
}


/************************************************************************/
/*                      ResetReading()                                  */
/************************************************************************/

void OGRGeoPackageLayer::ResetReading()
{
    if ( m_poQueryStatement )
    {
        sqlite3_finalize(m_poQueryStatement);
        m_poQueryStatement = NULL;
    }

    if ( m_poInsertStatement )
    {
        sqlite3_finalize(m_poInsertStatement);
        m_poInsertStatement = NULL;
    }

    if ( m_poUpdateStatement )
    {
        sqlite3_finalize(m_poUpdateStatement);
        m_poUpdateStatement = NULL;
    }
    
    if ( m_poFidStatement )
    {
        sqlite3_finalize(m_poFidStatement);
        m_poFidStatement = NULL;
    }
    
    BuildColumns();
    return;
}


/************************************************************************/
/*                        GetNextFeature()                              */
/************************************************************************/

OGRFeature* OGRGeoPackageLayer::GetNextFeature()
{
    /* There is no active query statement set up, */
    /* so job #1 is to prepare the statement. */
    if ( ! m_poQueryStatement )
    {
        /* Append the attribute filter, if there is one */
        CPLString soSQL;
        if ( m_soFilter.length() > 0 )
            soSQL.Printf("SELECT %s FROM %s WHERE %s", m_soColumns.c_str(), m_pszTableName, m_soFilter.c_str());
        else
            soSQL.Printf("SELECT %s FROM %s ", m_soColumns.c_str(), m_pszTableName);

        int err = sqlite3_prepare(m_poDS->GetDatabaseHandle(), soSQL.c_str(), -1, &m_poQueryStatement, NULL);
        if ( err != SQLITE_OK )
        {
            m_poQueryStatement = NULL;
            CPLError( CE_Failure, CPLE_AppDefined, "failed to prepare SQL: %s", soSQL.c_str());            
            return NULL;
        }
    }
    
    while ( TRUE )
    {
        int err = sqlite3_step(m_poQueryStatement);
        
        /* Nothing left in statement? NULL return indicates to caller */
        /* that there are no features left */
        if ( err == SQLITE_DONE )
        {
            return NULL;
        }
        /* Got a row, let's read it */
        else if ( err == SQLITE_ROW )
        {
            OGRFeature *poFeature;
            
            /* Fetch the feature */
            if ( ReadFeature(m_poQueryStatement, &poFeature) != OGRERR_NONE )
                return NULL;
            
            if( (m_poFilterGeom == NULL || FilterGeometry(poFeature->GetGeometryRef()) ) &&
                (m_poAttrQuery  == NULL || m_poAttrQuery->Evaluate(poFeature)) )
            {
                return poFeature;                
            }

            /* This feature doesn't pass the filters */
            /* So delete it and loop again to try the next row */
            delete poFeature;
        }
        else 
        {
            /* Got neither a row, nor the end of the query. */
            /* Something terrible has happened, break out of loop */
            /* CPLError( CE_Failure, CPLE_AppDefined, "unable to step through query statement"); */
            return NULL;
        }

    }

}	

/************************************************************************/
/*                        GetFeature()                                  */
/************************************************************************/

OGRFeature* OGRGeoPackageLayer::GetFeature(long nFID)
{
    /* No FID, no answer. */
    if (nFID == OGRNullFID)
        return NULL;
    
    /* Clear out any existing query */
    ResetReading();

    /* No filters apply, just use the FID */
    CPLString soSQL;
    soSQL.Printf("SELECT %s FROM %s WHERE %s = %ld",
                 m_soColumns.c_str(), m_pszTableName, m_pszFidColumn, nFID);

    int err = sqlite3_prepare(m_poDS->GetDatabaseHandle(), soSQL.c_str(), -1, &m_poQueryStatement, NULL);
    if ( err != SQLITE_OK )
    {
        m_poQueryStatement = NULL;
        CPLError( CE_Failure, CPLE_AppDefined, "failed to prepare SQL: %s", soSQL.c_str());            
        return NULL;
    }
    
    /* Should be only one or zero results */
    err = sqlite3_step(m_poQueryStatement);
        
    /* Nothing left in statement? NULL return indicates to caller */
    /* that there are no features left */
    if ( err == SQLITE_DONE )
        return NULL;

    /* Aha, got one */
    if ( err == SQLITE_ROW )
    {
        OGRFeature *poFeature;
        
        /* Fetch the feature */
        if ( ReadFeature(m_poQueryStatement, &poFeature) != OGRERR_NONE )
            return NULL;
            
        if ( poFeature )
            return poFeature;                
        else 
            return NULL;
    }
    
    /* Error out on all other return codes */
    return NULL;
}

/************************************************************************/
/*                        DeleteFeature()                               */
/************************************************************************/

OGRErr OGRGeoPackageLayer::DeleteFeature(long nFID)	
{
    if( !m_poDS->IsUpdatable() )
    {
        return OGRERR_FAILURE;
    }
    
    /* No FID, no answer. */
    if (nFID == OGRNullFID)
    {
        CPLError( CE_Failure, CPLE_AppDefined, "delete feature called with null FID");
        return OGRERR_FAILURE;
    }
    
    /* Clear out any existing query */
    ResetReading();

    /* No filters apply, just use the FID */
    CPLString soSQL;
    soSQL.Printf("DELETE FROM %s WHERE %s = %ld",
                 m_pszTableName, m_pszFidColumn, nFID);

    
    return SQLCommand(m_poDS->GetDatabaseHandle(), soSQL.c_str());
}

/************************************************************************/
/*                      GetFIDColumn()                                  */
/************************************************************************/

const char* OGRGeoPackageLayer::GetFIDColumn()
{
    if ( ! m_pszFidColumn )
        return "";
    else
        return m_pszFidColumn;
}

/************************************************************************/
/*                        SyncToDisk()                                  */
/************************************************************************/

OGRErr OGRGeoPackageLayer::SyncToDisk()
{
    SaveExtent();
    return OGRERR_NONE;
}

/************************************************************************/
/*                        StartTransaction()                            */
/************************************************************************/

OGRErr OGRGeoPackageLayer::StartTransaction()
{
    return SQLCommand(m_poDS->GetDatabaseHandle(), "BEGIN");
}


/************************************************************************/
/*                        CommitTransaction()                           */
/************************************************************************/

OGRErr OGRGeoPackageLayer::CommitTransaction()
{
    return SQLCommand(m_poDS->GetDatabaseHandle(), "COMMIT");
}


/************************************************************************/
/*                        RollbackTransaction()                         */
/************************************************************************/

OGRErr OGRGeoPackageLayer::RollbackTransaction()
{
    return SQLCommand(m_poDS->GetDatabaseHandle(), "ROLLBACK");
}


/************************************************************************/
/*                        GetFeatureCount()                             */
/************************************************************************/

int OGRGeoPackageLayer::GetFeatureCount( int bForce )
{
    if( m_poFilterGeom != NULL )
        return OGRLayer::GetFeatureCount(bForce);

    /* Ignore bForce, because we always do a full count on the database */
    OGRErr err;
    CPLString soSQL;
    if ( m_soFilter.length() > 0 )
        soSQL.Printf("SELECT Count(*) FROM %s WHERE %s", m_pszTableName, m_soFilter.c_str());
    else
        soSQL.Printf("SELECT Count(*) FROM %s ", m_pszTableName);

    /* Just run the query directly and get back integer */
    int iFeatureCount = SQLGetInteger(m_poDS->GetDatabaseHandle(), soSQL.c_str(), &err);

    /* Generic implementation uses -1 for error condition, so we will too */
    if ( err == OGRERR_NONE )
        return iFeatureCount;
    else
        return -1;
}


/************************************************************************/
/*                        GetExtent()                                   */
/************************************************************************/

OGRErr OGRGeoPackageLayer::GetExtent(OGREnvelope *psExtent, int bForce)
{
    /* Extent already calculated! We're done. */
    if ( m_poExtent != NULL )
    {
        if ( psExtent )
        {
            *psExtent = *m_poExtent;            
        }
        return OGRERR_NONE;
    }

    /* User is OK with expensive calculation, fall back to */
    /* default implementation (scan all features) and save */
    /* the result for later */
    if ( bForce )
    {
        OGRErr err = OGRLayer::GetExtent(psExtent, bForce);
        if ( err != OGRERR_NONE )
            return err;
    
        if ( ! m_poExtent )
            m_poExtent = new OGREnvelope( *psExtent );
        else
            *m_poExtent = *psExtent;
        return SaveExtent();
    }

    return OGRERR_FAILURE;
}



/************************************************************************/
/*                      TestCapability()                                */
/************************************************************************/

int OGRGeoPackageLayer::TestCapability ( const char * pszCap )
{
    if ( EQUAL(pszCap, OLCCreateField) ||
         EQUAL(pszCap, OLCSequentialWrite) ||
         EQUAL(pszCap, OLCDeleteFeature) ||
         EQUAL(pszCap, OLCRandomWrite) )
    {
        return m_poDS->IsUpdatable();
    }
    else if ( EQUAL(pszCap, OLCRandomRead) ||
              EQUAL(pszCap, OLCTransactions) )
    {
        return TRUE;
    }
    else if ( EQUAL(pszCap, OLCStringsAsUTF8) )
    {
        return m_poDS->GetUTF8();
    }
    else if ( EQUAL(pszCap, OLCFastGetExtent) )
    {
        if ( m_poExtent && ! m_poFilterGeom )
            return TRUE;
        else
            return FALSE;
    }
    else
    {
        return FALSE;
    }
}

