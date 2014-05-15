/******************************************************************************
 * $Id$
 *
 * Project:  GeoPackage Translator
 * Purpose:  Implements OGRGeoPackageTableLayer class
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
OGRErr OGRGeoPackageTableLayer::SaveExtent()
{
    if ( !m_poDS->GetUpdate() || ! m_bExtentChanged || ! m_poExtent ) 
        return OGRERR_NONE;

    sqlite3* poDb = m_poDS->GetDB();

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
OGRErr OGRGeoPackageTableLayer::UpdateExtent( const OGREnvelope *poExtent )
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
OGRErr OGRGeoPackageTableLayer::BuildColumns()
{
    if ( ! m_poFeatureDefn || ! m_pszFidColumn )
    {
        return OGRERR_FAILURE;
    }

    CPLFree(panFieldOrdinals);
    panFieldOrdinals = (int *) CPLMalloc( sizeof(int) * m_poFeatureDefn->GetFieldCount() );

    /* Always start with a primary key */
    CPLString soColumns = m_pszFidColumn;
    iFIDCol = 0;

    /* Add a geometry column if there is one (just one) */
    if ( m_poFeatureDefn->GetGeomFieldCount() )
    {
        soColumns += ", ";
        soColumns += m_poFeatureDefn->GetGeomFieldDefn(0)->GetNameRef();
        iGeomCol = 1;
    }

    /* Add all the attribute columns */
    for( int i = 0; i < m_poFeatureDefn->GetFieldCount(); i++ )
    {
        soColumns += ", ";
        soColumns += m_poFeatureDefn->GetFieldDefn(i)->GetNameRef();
        panFieldOrdinals[i] = 1 + (iGeomCol >= 0) + i;
    }

    m_soColumns = soColumns;    
    return OGRERR_NONE;    
}

//----------------------------------------------------------------------
// IsGeomFieldSet()
// 
// Utility method to determine if there is a non-Null geometry
// in an OGRGeometry.
//
OGRBoolean OGRGeoPackageTableLayer::IsGeomFieldSet( OGRFeature *poFeature )
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

OGRErr OGRGeoPackageTableLayer::FeatureBindParameters( OGRFeature *poFeature,
                                                       sqlite3_stmt *poStmt,
                                                       int *pnColCount,
                                                       int bAddFID )
{
    int nColCount = 1;
    int err;
    
    if ( ! (poFeature && poStmt && pnColCount) )
        return OGRERR_FAILURE;

    OGRFeatureDefn *poFeatureDefn = poFeature->GetDefnRef();

    if( bAddFID )
    {
        err = sqlite3_bind_int(poStmt, nColCount++, poFeature->GetFID());
    }

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
OGRErr OGRGeoPackageTableLayer::FeatureBindUpdateParameters( OGRFeature *poFeature, sqlite3_stmt *poStmt )
{

    int nColCount;
    OGRErr err = FeatureBindParameters( poFeature, poStmt, &nColCount, FALSE );
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
OGRErr OGRGeoPackageTableLayer::FeatureBindInsertParameters( OGRFeature *poFeature, sqlite3_stmt *poStmt, int bAddFID )
{    
    int nColCount;
    return FeatureBindParameters( poFeature, poStmt, &nColCount, bAddFID );
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
CPLString OGRGeoPackageTableLayer::FeatureGenerateInsertSQL( OGRFeature *poFeature, int bAddFID )
{
    OGRBoolean bNeedComma = FALSE;
    OGRFeatureDefn *poFeatureDefn = poFeature->GetDefnRef();

    if( poFeatureDefn->GetFieldCount() == 0 &&
        poFeatureDefn->GetGeomFieldCount() == 0 &&
        !bAddFID )
        return CPLSPrintf("INSERT INTO \"%s\" DEFAULT VALUES", m_pszTableName);

    /* Set up our SQL string basics */
    CPLString osSQLFront;
    osSQLFront.Printf("INSERT INTO \"%s\" ( ", m_pszTableName);

    CPLString osSQLBack;
    osSQLBack = ") VALUES (";
    
    if( bAddFID )
    {
        osSQLFront += GetFIDColumn();
        osSQLBack += "?";
        bNeedComma = TRUE;
    }
    
    if ( poFeatureDefn->GetGeomFieldCount() )
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

        osSQLFront += poFeatureDefn->GetGeomFieldDefn(0)->GetNameRef();
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

        osSQLFront += poFeatureDefn->GetFieldDefn(i)->GetNameRef();
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
CPLString OGRGeoPackageTableLayer::FeatureGenerateUpdateSQL( OGRFeature *poFeature )
{
    OGRBoolean bNeedComma = FALSE;
    OGRFeatureDefn *poFeatureDefn = poFeature->GetDefnRef();

    /* Set up our SQL string basics */
    CPLString osUpdate;
    osUpdate.Printf("UPDATE \"%s\" SET ", m_pszTableName);
    
    if ( poFeatureDefn->GetGeomFieldCount() > 0 )
    {
        osUpdate += poFeatureDefn->GetGeomFieldDefn(0)->GetNameRef();
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

        osUpdate += poFeatureDefn->GetFieldDefn(i)->GetNameRef();
        osUpdate += "=?";
    }
    
    CPLString osWhere;
    osWhere.Printf(" WHERE \"%s\" = ?", m_pszFidColumn);

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
OGRErr OGRGeoPackageTableLayer::ReadTableDefinition(int bIsSpatial)
{
    OGRErr err;
    SQLResult oResultTable;
    char* pszSQL;
    OGRBoolean bReadExtent = FALSE;
    sqlite3* poDb = m_poDS->GetDB();
    OGREnvelope oExtent;
    CPLString osGeomColsType;
    int bHasZ = FALSE;

    if( bIsSpatial )
    {
        /* Check that the table name is registered in gpkg_contents */
        pszSQL = sqlite3_mprintf(
                    "SELECT table_name, data_type, identifier, "
                    "description, min_x, min_y, max_x, max_y, srs_id "
                    "FROM gpkg_contents "
                    "WHERE table_name = '%q' AND "
                    "Lower(data_type) = 'features'",
                    m_pszTableName);
                    
        SQLResult oResultContents;
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

        SQLResult oResultGeomCols;
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

        const char* pszGeomColsType = SQLResultGetValue(&oResultGeomCols, 2, 0);
        if( pszGeomColsType != NULL )
            osGeomColsType = pszGeomColsType;
        m_iSrs = SQLResultGetValueAsInteger(&oResultGeomCols, 3, 0);
        bHasZ = SQLResultGetValueAsInteger(&oResultGeomCols, 4, 0);

        SQLResultFree(&oResultGeomCols);
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

        SQLResultFree(&oResultTable);
        return OGRERR_FAILURE;
    }
    
    /* Populate feature definition from table description */
    m_poFeatureDefn = new OGRFeatureDefn( m_pszTableName );
    m_poFeatureDefn->SetGeomType(wkbNone);
    m_poFeatureDefn->Reference();
    
    int iRecord;
    OGRBoolean bFidFound = FALSE;

    for ( iRecord = 0; iRecord < oResultTable.nRowCount; iRecord++ )
    {
        const char *pszName = SQLResultGetValue(&oResultTable, 1, iRecord);
        const char *pszType = SQLResultGetValue(&oResultTable, 2, iRecord);
        OGRBoolean bFid = SQLResultGetValueAsInteger(&oResultTable, 5, iRecord);
        OGRFieldType oType = GPkgFieldToOGR(pszType);

        /* Not a standard field type... */
        if ( oType > OFTMaxType && osGeomColsType.size() )
        {
            /* Maybe it's a geometry type? */
            OGRwkbGeometryType oGeomType = GPkgGeometryTypeToWKB(pszType, bHasZ);
            if ( oGeomType != wkbNone )
            {
                OGRwkbGeometryType oGeomTypeGeomCols = GPkgGeometryTypeToWKB(osGeomColsType.c_str(), bHasZ);
                /* Enforce consistency between table and metadata */
                if( wkbFlatten(oGeomType) == wkbUnknown )
                    oGeomType = oGeomTypeGeomCols;
                if ( oGeomType != oGeomTypeGeomCols )
                {
                    CPLError(CE_Warning, CPLE_AppDefined, 
                             "geometry column type in '%s.%s' is not consistent with type in gpkg_geometry_columns", 
                             m_pszTableName, pszName);
                }

                if ( m_poFeatureDefn->GetGeomFieldCount() == 0 )
                {
                    OGRGeomFieldDefn oGeomField(pszName, oGeomType);
                    m_poFeatureDefn->AddGeomFieldDefn(&oGeomField);

                    /* Read the SRS */
                    OGRSpatialReference *poSRS = m_poDS->GetSpatialRef(m_iSrs);
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

    /* Update the columns string */
    BuildColumns();

    return OGRERR_NONE;
}


/************************************************************************/
/*                      OGRGeoPackageTableLayer()                       */
/************************************************************************/

OGRGeoPackageTableLayer::OGRGeoPackageTableLayer(
                    OGRGeoPackageDataSource *poDS,
                    const char * pszTableName) : OGRGeoPackageLayer(poDS)
{
    m_pszTableName = CPLStrdup(pszTableName);
    m_iSrs = 0;
    m_poExtent = NULL;
    m_bExtentChanged = FALSE;
    m_poQueryStatement = NULL;
    m_poUpdateStatement = NULL;
    m_poInsertStatement = NULL;
    m_soColumns = "";
    m_soFilter = "";
    bDeferedSpatialIndexCreation = FALSE;
    m_bHasSpatialIndex = -1;
}


/************************************************************************/
/*                      ~OGRGeoPackageTableLayer()                      */
/************************************************************************/

OGRGeoPackageTableLayer::~OGRGeoPackageTableLayer()
{
    CreateSpatialIndexIfNecessary();

    /* Save metadata back to the database */
    SaveExtent();

    /* Clean up resources in memory */
    if ( m_pszTableName )
        CPLFree( m_pszTableName );

    if ( m_poExtent )
        delete m_poExtent;

    if ( m_poUpdateStatement )
        sqlite3_finalize(m_poUpdateStatement);

    if ( m_poInsertStatement )
        sqlite3_finalize(m_poInsertStatement);
}


/************************************************************************/
/*                      CreateField()                                   */
/************************************************************************/

OGRErr OGRGeoPackageTableLayer::CreateField( OGRFieldDefn *poField, int bApproxOK )
{
    if( !m_poDS->GetUpdate() )
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
    panFieldOrdinals = (int *) CPLRealloc( panFieldOrdinals,
                                           sizeof(int) * m_poFeatureDefn->GetFieldCount() );
    panFieldOrdinals[ m_poFeatureDefn->GetFieldCount() - 1 ] =
        m_poFeatureDefn->GetFieldCount() - 1 + (iFIDCol >= 0) + (iGeomCol >= 0);

    ResetReading();
    return OGRERR_NONE;
}


/************************************************************************/
/*                      CreateFeature()                                 */
/************************************************************************/

OGRErr OGRGeoPackageTableLayer::CreateFeature( OGRFeature *poFeature )
{
    if( !m_poDS->GetUpdate() )
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
        CPLString osCommand = FeatureGenerateInsertSQL(poFeature, FALSE);
        
        /* Prepare the SQL into a statement */
        sqlite3 *poDb = m_poDS->GetDB();
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
    OGRErr errOgr = FeatureBindInsertParameters(poFeature, m_poInsertStatement, FALSE);
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
    if ( (iFid = sqlite3_last_insert_rowid(m_poDS->GetDB())) )
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

OGRErr OGRGeoPackageTableLayer::SetFeature( OGRFeature *poFeature )
{
    if( !m_poDS->GetUpdate() )
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

    /* Old version of SQLite have issues with some of the spatial index triggers */
#if SQLITE_VERSION_NUMBER < 3007008
    if( HasSpatialIndex() )
    {
        if ( ! m_poUpdateStatement )
        {
            /* Construct a SQL INSERT statement from the OGRFeature */
            /* Only work with fields that are set */
            /* Do not stick values into SQL, use placeholder and bind values later */    
            CPLString osCommand = FeatureGenerateInsertSQL(poFeature, TRUE);

            /* Prepare the SQL into a statement */
            int err = sqlite3_prepare_v2(m_poDS->GetDB(), osCommand, -1, &m_poUpdateStatement, NULL);
            if ( err != SQLITE_OK )
            {
                CPLError( CE_Failure, CPLE_AppDefined,
                        "failed to prepare SQL: %s", osCommand.c_str());        
                return OGRERR_FAILURE;
            }
        }

        sqlite3_stmt* hBackupStmt = m_poUpdateStatement;
        m_poUpdateStatement = NULL;

        DeleteFeature( poFeature->GetFID() );

        m_poUpdateStatement = hBackupStmt;

        /* Bind values onto the statement now */
        OGRErr errOgr = FeatureBindInsertParameters(poFeature, m_poUpdateStatement, TRUE);
        if ( errOgr != OGRERR_NONE )
            return errOgr;
    }
    else
#endif
    {
        if ( ! m_poUpdateStatement )
        {
            /* Construct a SQL UPDATE statement from the OGRFeature */
            /* Only work with fields that are set */
            /* Do not stick values into SQL, use placeholder and bind values later */    
            CPLString osCommand = FeatureGenerateUpdateSQL(poFeature);

            /* Prepare the SQL into a statement */
            int err = sqlite3_prepare_v2(m_poDS->GetDB(), osCommand, -1, &m_poUpdateStatement, NULL);
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
    }

    /* From here execute the statement and check errors */
    int err = sqlite3_step(m_poUpdateStatement);
    if ( ! (err == SQLITE_OK || err == SQLITE_DONE) )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "failed to execute update : %s",
                  sqlite3_errmsg( m_poDS->GetDB() ) );
        return OGRERR_FAILURE;       
    }

    sqlite3_reset(m_poUpdateStatement);
    sqlite3_clear_bindings(m_poUpdateStatement);

    /* Only update the envelope if we changed something */
    if (sqlite3_changes(m_poDS->GetDB()) )
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

OGRErr OGRGeoPackageTableLayer::SetAttributeFilter( const char *pszQuery )

{
    CPLFree(m_pszAttrQueryString);
    m_pszAttrQueryString = (pszQuery) ? CPLStrdup(pszQuery) : NULL;

    if( pszQuery == NULL )
        osQuery = "";
    else
        osQuery = pszQuery;

    BuildWhere();

    ResetReading();

    return OGRERR_NONE;
}


/************************************************************************/
/*                      ResetReading()                                  */
/************************************************************************/

void OGRGeoPackageTableLayer::ResetReading()
{
    OGRGeoPackageLayer::ResetReading();

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

    BuildColumns();
    return;
}


/************************************************************************/
/*                           ResetStatement()                           */
/************************************************************************/

OGRErr OGRGeoPackageTableLayer::ResetStatement()

{
    ClearStatement();

    /* There is no active query statement set up, */
    /* so job #1 is to prepare the statement. */
    /* Append the attribute filter, if there is one */
    CPLString soSQL;
    if ( m_soFilter.length() > 0 )
        soSQL.Printf("SELECT %s FROM \"%s\" WHERE %s", m_soColumns.c_str(), m_pszTableName, m_soFilter.c_str());
    else
        soSQL.Printf("SELECT %s FROM \"%s\" ", m_soColumns.c_str(), m_pszTableName);

    int err = sqlite3_prepare(m_poDS->GetDB(), soSQL.c_str(), -1, &m_poQueryStatement, NULL);
    if ( err != SQLITE_OK )
    {
        m_poQueryStatement = NULL;
        CPLError( CE_Failure, CPLE_AppDefined, "failed to prepare SQL: %s", soSQL.c_str());            
        return OGRERR_FAILURE;
    }
    
    return OGRERR_NONE;
}

/************************************************************************/
/*                           GetNextFeature()                           */
/************************************************************************/

OGRFeature* OGRGeoPackageTableLayer::GetNextFeature()
{
    CreateSpatialIndexIfNecessary();
    return OGRGeoPackageLayer::GetNextFeature();
}

/************************************************************************/
/*                        GetFeature()                                  */
/************************************************************************/

OGRFeature* OGRGeoPackageTableLayer::GetFeature(long nFID)
{
    /* No FID, no answer. */
    if (nFID == OGRNullFID)
        return NULL;
    
    CreateSpatialIndexIfNecessary();
    
    /* Clear out any existing query */
    ResetReading();

    /* No filters apply, just use the FID */
    CPLString soSQL;
    soSQL.Printf("SELECT %s FROM \"%s\" WHERE \"%s\" = %ld",
                 m_soColumns.c_str(), m_pszTableName, m_pszFidColumn, nFID);

    int err = sqlite3_prepare(m_poDS->GetDB(), soSQL.c_str(), -1, &m_poQueryStatement, NULL);
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
        return TranslateFeature(m_poQueryStatement);
    }
    
    /* Error out on all other return codes */
    return NULL;
}

/************************************************************************/
/*                        DeleteFeature()                               */
/************************************************************************/

OGRErr OGRGeoPackageTableLayer::DeleteFeature(long nFID) 
{
    if( !m_poDS->GetUpdate() )
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
    soSQL.Printf("DELETE FROM \"%s\" WHERE \"%s\" = %ld",
                 m_pszTableName, m_pszFidColumn, nFID);

    
    return SQLCommand(m_poDS->GetDB(), soSQL.c_str());
}

/************************************************************************/
/*                        SyncToDisk()                                  */
/************************************************************************/

OGRErr OGRGeoPackageTableLayer::SyncToDisk()
{
    SaveExtent();
    return OGRERR_NONE;
}

/************************************************************************/
/*                        StartTransaction()                            */
/************************************************************************/

OGRErr OGRGeoPackageTableLayer::StartTransaction()
{
    return SQLCommand(m_poDS->GetDB(), "BEGIN");
}


/************************************************************************/
/*                        CommitTransaction()                           */
/************************************************************************/

OGRErr OGRGeoPackageTableLayer::CommitTransaction()
{
    return SQLCommand(m_poDS->GetDB(), "COMMIT");
}


/************************************************************************/
/*                        RollbackTransaction()                         */
/************************************************************************/

OGRErr OGRGeoPackageTableLayer::RollbackTransaction()
{
    return SQLCommand(m_poDS->GetDB(), "ROLLBACK");
}


/************************************************************************/
/*                        GetFeatureCount()                             */
/************************************************************************/

int OGRGeoPackageTableLayer::GetFeatureCount( int bForce )
{
    if( m_poFilterGeom != NULL && !m_bFilterIsEnvelope )
        return OGRGeoPackageLayer::GetFeatureCount();

    /* Ignore bForce, because we always do a full count on the database */
    OGRErr err;
    CPLString soSQL;
    if ( m_soFilter.length() > 0 )
        soSQL.Printf("SELECT Count(*) FROM \"%s\" WHERE %s", m_pszTableName, m_soFilter.c_str());
    else
        soSQL.Printf("SELECT Count(*) FROM \"%s\" ", m_pszTableName);

    /* Just run the query directly and get back integer */
    int iFeatureCount = SQLGetInteger(m_poDS->GetDB(), soSQL.c_str(), &err);

    /* Generic implementation uses -1 for error condition, so we will too */
    if ( err == OGRERR_NONE )
        return iFeatureCount;
    else
        return -1;
}


/************************************************************************/
/*                        GetExtent()                                   */
/************************************************************************/

OGRErr OGRGeoPackageTableLayer::GetExtent(OGREnvelope *psExtent, int bForce)
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

int OGRGeoPackageTableLayer::TestCapability ( const char * pszCap )
{
    if ( EQUAL(pszCap, OLCCreateField) ||
         EQUAL(pszCap, OLCSequentialWrite) ||
         EQUAL(pszCap, OLCDeleteFeature) ||
         EQUAL(pszCap, OLCRandomWrite) )
    {
        return m_poDS->GetUpdate();
    }
    else if ( EQUAL(pszCap, OLCRandomRead) ||
              EQUAL(pszCap, OLCTransactions) )
    {
        return TRUE;
    }
    else if ( EQUAL(pszCap, OLCFastSpatialFilter) )
    {
        return HasSpatialIndex();
    }
    else if ( EQUAL(pszCap, OLCFastGetExtent) )
    {
        if ( m_poExtent )
            return TRUE;
        else
            return FALSE;
    }
    else
    {
        return OGRGeoPackageLayer::TestCapability(pszCap);
    }
}

/************************************************************************/
/*                     CreateSpatialIndexIfNecessary()                  */
/************************************************************************/

void OGRGeoPackageTableLayer::CreateSpatialIndexIfNecessary()
{
    if( bDeferedSpatialIndexCreation )
    {
        bDeferedSpatialIndexCreation = FALSE;
        CreateSpatialIndex();
    }
}

/************************************************************************/
/*                       CreateSpatialIndex()                           */
/************************************************************************/

void OGRGeoPackageTableLayer::CreateSpatialIndex()
{
    char* pszSQL;
    OGRErr err;
    if( m_poFeatureDefn->GetGeomFieldCount() == 0 )
        return;
    if( m_poDS->CreateExtensionsTableIfNecessary() != OGRERR_NONE )
        return;

    const char* pszT = m_pszTableName;
    const char* pszC = m_poFeatureDefn->GetGeomFieldDefn(0)->GetNameRef();
    const char* pszI = GetFIDColumn();

    StartTransaction();

    /* Register the table in gpkg_extensions */
    pszSQL = sqlite3_mprintf(
                 "INSERT INTO gpkg_extensions "
                 "(table_name,column_name,extension_name,definition,scope) "
                 "VALUES ('%q', '%q', 'gpkg_rtree_index', 'GeoPackage 1.0 Specification Annex L', 'write-only')",
                 pszT, pszC );
    err = SQLCommand(m_poDS->GetDB(), pszSQL);
    sqlite3_free(pszSQL);
    if( err != OGRERR_NONE )
    {
        RollbackTransaction();
        return;
    }

    /* Create virtual table */
    pszSQL = sqlite3_mprintf(
                 "CREATE VIRTUAL TABLE \"rtree_%s_%s\" USING rtree(id, minx, maxx, miny, maxy)",
                 pszT, pszC );
    err = SQLCommand(m_poDS->GetDB(), pszSQL);
    sqlite3_free(pszSQL);
    if( err != OGRERR_NONE )
    {
        RollbackTransaction();
        return;
    }

    /* Populate the RTree */
    pszSQL = sqlite3_mprintf(
                 "INSERT OR REPLACE INTO \"rtree_%s_%s\" "
                 "SELECT \"%s\", st_minx(\"%s\"), st_maxx(\"%s\"), st_miny(\"%s\"), st_maxy(\"%s\") FROM \"%s\"",
                 pszT, pszC, pszI, pszC, pszC, pszC, pszC, pszT );
    err = SQLCommand(m_poDS->GetDB(), pszSQL);
    sqlite3_free(pszSQL);
    if( err != OGRERR_NONE )
    {
        RollbackTransaction();
        return;
    }

    /* Define Triggers to Maintain Spatial Index Values */

    /* Conditions: Insertion of non-empty geometry
       Actions   : Insert record into rtree */
    pszSQL = sqlite3_mprintf(
                   "CREATE TRIGGER \"rtree_%s_%s_insert\" AFTER INSERT ON \"%s\" "
                   "WHEN (new.\"%s\" NOT NULL AND NOT ST_IsEmpty(NEW.\"%s\")) "
                   "BEGIN "
                   "INSERT OR REPLACE INTO \"rtree_%s_%s\" VALUES ("
                   "NEW.\"%s\","
                   "ST_MinX(NEW.\"%s\"), ST_MaxX(NEW.\"%s\"),"
                   "ST_MinY(NEW.\"%s\"), ST_MaxY(NEW.\"%s\")"
                   "); "
                   "END",
                   pszT, pszC, pszT,
                   pszC, pszC,
                   pszT, pszC,
                   pszI,
                   pszC, pszC,
                   pszC, pszC);
    err = SQLCommand(m_poDS->GetDB(), pszSQL);
    sqlite3_free(pszSQL);
    if( err != OGRERR_NONE )
    {
        RollbackTransaction();
        return;
    }

    /* Conditions: Update of geometry column to non-empty geometry
               No row ID change
       Actions   : Update record in rtree */
    pszSQL = sqlite3_mprintf(
                   "CREATE TRIGGER \"rtree_%s_%s_update1\" AFTER UPDATE OF \"%s\" ON \"%s\" "
                   "WHEN OLD.\"%s\" = NEW.\"%s\" AND "
                   "(NEW.\"%s\" NOTNULL AND NOT ST_IsEmpty(NEW.\"%s\")) "
                   "BEGIN "
                   "INSERT OR REPLACE INTO \"rtree_%s_%s\" VALUES ("
                   "NEW.\"%s\","
                   "ST_MinX(NEW.\"%s\"), ST_MaxX(NEW.\"%s\"),"
                   "ST_MinY(NEW.\"%s\"), ST_MaxY(NEW.\"%s\")"
                   "); "
                   "END",
                   pszT, pszC, pszC, pszT,
                   pszI, pszI,
                   pszC, pszC,
                   pszT, pszC,
                   pszI,
                   pszC, pszC,
                   pszC, pszC);
    err = SQLCommand(m_poDS->GetDB(), pszSQL);
    sqlite3_free(pszSQL);
    if( err != OGRERR_NONE )
    {
        RollbackTransaction();
        return;
    }

    /* Conditions: Update of geometry column to empty geometry
               No row ID change
       Actions   : Remove record from rtree */
    pszSQL = sqlite3_mprintf(
                   "CREATE TRIGGER \"rtree_%s_%s_update2\" AFTER UPDATE OF \"%s\" ON \"%s\" "
                   "WHEN OLD.\"%s\" = NEW.\"%s\" AND "
                   "(NEW.\"%s\" ISNULL OR ST_IsEmpty(NEW.\"%s\")) "
                   "BEGIN "
                   "DELETE FROM \"rtree_%s_%s\" WHERE id = OLD.\"%s\"; "
                   "END",
                   pszT, pszC, pszC, pszT,
                   pszI, pszI,
                   pszC, pszC,
                   pszT, pszC, pszI);
    err = SQLCommand(m_poDS->GetDB(), pszSQL);
    sqlite3_free(pszSQL);
    if( err != OGRERR_NONE )
    {
        RollbackTransaction();
        return;
    }

    /* Conditions: Update of any column
                    Row ID change
                    Non-empty geometry
        Actions   : Remove record from rtree for old <i>
                    Insert record into rtree for new <i> */
    pszSQL = sqlite3_mprintf(
                   "CREATE TRIGGER \"rtree_%s_%s_update3\" AFTER UPDATE OF \"%s\" ON \"%s\" "
                   "WHEN OLD.\"%s\" != NEW.\"%s\" AND "
                   "(NEW.\"%s\" NOTNULL AND NOT ST_IsEmpty(NEW.\"%s\")) "
                   "BEGIN "
                   "DELETE FROM \"rtree_%s_%s\" WHERE id = OLD.\"%s\"; "
                   "INSERT OR REPLACE INTO \"rtree_%s_%s\" VALUES ("
                   "NEW.\"%s\","
                   "ST_MinX(NEW.\"%s\"), ST_MaxX(NEW.\"%s\"),"
                   "ST_MinY(NEW.\"%s\"), ST_MaxY(NEW.\"%s\")"
                   "); "
                   "END",
                   pszT, pszC, pszC, pszT,
                   pszI, pszI,
                   pszC, pszC,
                   pszT, pszC, pszI,
                   pszT, pszC,
                   pszI,
                   pszC, pszC,
                   pszC, pszC);
    err = SQLCommand(m_poDS->GetDB(), pszSQL);
    sqlite3_free(pszSQL);
    if( err != OGRERR_NONE )
    {
        RollbackTransaction();
        return;
    }

    /* Conditions: Update of any column
                    Row ID change
                    Empty geometry
        Actions   : Remove record from rtree for old and new <i> */
    pszSQL = sqlite3_mprintf(
                   "CREATE TRIGGER \"rtree_%s_%s_update4\" AFTER UPDATE ON \"%s\" "
                   "WHEN OLD.\"%s\" != NEW.\"%s\" AND "
                   "(NEW.\"%s\" ISNULL OR ST_IsEmpty(NEW.\"%s\")) "
                   "BEGIN "
                   "DELETE FROM \"rtree_%s_%s\" WHERE id IN (OLD.\"%s\", NEW.\"%s\"); "
                   "END",
                   pszT, pszC, pszT,
                   pszI, pszI,
                   pszC, pszC,
                   pszT, pszC, pszI, pszI);
    err = SQLCommand(m_poDS->GetDB(), pszSQL);
    sqlite3_free(pszSQL);
    if( err != OGRERR_NONE )
    {
        RollbackTransaction();
        return;
    }

    /* Conditions: Row deleted
        Actions   : Remove record from rtree for old <i> */
    pszSQL = sqlite3_mprintf(
                   "CREATE TRIGGER \"rtree_%s_%s_delete\" AFTER DELETE ON \"%s\" "
                   "WHEN old.\"%s\" NOT NULL "
                   "BEGIN "
                   "DELETE FROM \"rtree_%s_%s\" WHERE id = OLD.\"%s\"; "
                   "END",
                   pszT, pszC, pszT,
                   pszC,
                   pszT, pszC, pszI);
    err = SQLCommand(m_poDS->GetDB(), pszSQL);
    sqlite3_free(pszSQL);
    if( err != OGRERR_NONE )
    {
        RollbackTransaction();
        return;
    }

    CommitTransaction();
}

/************************************************************************/
/*                        HasSpatialIndex()                             */
/************************************************************************/

int OGRGeoPackageTableLayer::HasSpatialIndex()
{
    if( m_bHasSpatialIndex >= 0 )
        return m_bHasSpatialIndex;
    m_bHasSpatialIndex = FALSE;

    if( m_poFeatureDefn->GetGeomFieldCount() == 0 ||
        !m_poDS->HasExtensionsTable() )
        return FALSE;

    const char* pszT = m_pszTableName;
    const char* pszC = m_poFeatureDefn->GetGeomFieldDefn(0)->GetNameRef();

    /* Check into gpkg_extensions */
    char* pszSQL = sqlite3_mprintf(
                 "SELECT * FROM gpkg_extensions WHERE table_name='%q' "
                 "AND column_name='%q' AND extension_name='gpkg_rtree_index'",
                 pszT, pszC );
    SQLResult oResultTable;
    OGRErr err = SQLQuery(m_poDS->GetDB(), pszSQL, &oResultTable);
    sqlite3_free(pszSQL);
    if ( err == OGRERR_NONE && oResultTable.nRowCount == 1 )
    {
        m_bHasSpatialIndex = TRUE;
    }
    SQLResultFree(&oResultTable);
    
    return m_bHasSpatialIndex;
}

/************************************************************************/
/*                        DropSpatialIndex()                            */
/************************************************************************/

void OGRGeoPackageTableLayer::DropSpatialIndex()
{
    if( !HasSpatialIndex() )
        return;

    const char* pszT = m_pszTableName;
    const char* pszC =m_poFeatureDefn->GetGeomFieldDefn(0)->GetNameRef();
    char* pszSQL;

    pszSQL = sqlite3_mprintf("DELETE FROM gpkg_extensions WHERE table_name='%q' "
                 "AND column_name='%q' AND extension_name='gpkg_rtree_index'",
                 pszT, pszC );
    SQLCommand(m_poDS->GetDB(), pszSQL);
    sqlite3_free(pszSQL);

    pszSQL = sqlite3_mprintf("DROP TABLE \"rtree_%s_%s\"", pszT, pszC);
    SQLCommand(m_poDS->GetDB(), pszSQL);
    sqlite3_free(pszSQL);

    pszSQL = sqlite3_mprintf("DROP TRIGGER \"rtree_%s_%s_insert\"", pszT, pszC);
    SQLCommand(m_poDS->GetDB(), pszSQL);
    sqlite3_free(pszSQL);

    pszSQL = sqlite3_mprintf("DROP TRIGGER \"rtree_%s_%s_update1\"", pszT, pszC);
    SQLCommand(m_poDS->GetDB(), pszSQL);
    sqlite3_free(pszSQL);

    pszSQL = sqlite3_mprintf("DROP TRIGGER \"rtree_%s_%s_update2\"", pszT, pszC);
    SQLCommand(m_poDS->GetDB(), pszSQL);
    sqlite3_free(pszSQL);

    pszSQL = sqlite3_mprintf("DROP TRIGGER \"rtree_%s_%s_update3\"", pszT, pszC);
    SQLCommand(m_poDS->GetDB(), pszSQL);
    sqlite3_free(pszSQL);

    pszSQL = sqlite3_mprintf("DROP TRIGGER \"rtree_%s_%s_update4\"", pszT, pszC);
    SQLCommand(m_poDS->GetDB(), pszSQL);
    sqlite3_free(pszSQL);

    pszSQL = sqlite3_mprintf("DROP TRIGGER \"rtree_%s_%s_delete\"", pszT, pszC);
    SQLCommand(m_poDS->GetDB(), pszSQL);
    sqlite3_free(pszSQL);

    m_bHasSpatialIndex = FALSE;
}

/************************************************************************/
/*                          RenameTo()                                  */
/************************************************************************/

void OGRGeoPackageTableLayer::RenameTo(const char* pszDstTableName)
{
    int bHasSpatialIndex = HasSpatialIndex();

    if( bHasSpatialIndex )
    {
        DropSpatialIndex();
    }

    /* We also need to update GeoPackage metadata tables */
    char* pszSQL;
    pszSQL = sqlite3_mprintf(
            "UPDATE gpkg_geometry_columns SET table_name = '%s' WHERE table_name = '%s'",
            pszDstTableName, m_pszTableName);
    SQLCommand(m_poDS->GetDB(), pszSQL);
    sqlite3_free(pszSQL);
    
    pszSQL = sqlite3_mprintf(
            "UPDATE gpkg_contents SET table_name = '%s' WHERE table_name = '%s'",
            pszDstTableName, m_pszTableName);
    SQLCommand(m_poDS->GetDB(), pszSQL);
    sqlite3_free(pszSQL);

    CPLFree(m_pszTableName);
    m_pszTableName = CPLStrdup(pszDstTableName);

    if( bHasSpatialIndex )
    {
        CreateSpatialIndex();
    }
}

/************************************************************************/
/*                          SetSpatialFilter()                          */
/************************************************************************/

void OGRGeoPackageTableLayer::SetSpatialFilter( OGRGeometry * poGeomIn )

{
    if( InstallFilter( poGeomIn ) )
    {
        BuildWhere();

        ResetReading();
    }
}

/************************************************************************/
/*                           GetSpatialWhere()                          */
/************************************************************************/

CPLString OGRGeoPackageTableLayer::GetSpatialWhere(int iGeomCol,
                                               OGRGeometry* poFilterGeom)
{
    CPLString osSpatialWHERE;

    if( iGeomCol < 0 || iGeomCol >= m_poFeatureDefn->GetGeomFieldCount() )
        return osSpatialWHERE;

    const char* pszT = m_pszTableName;
    const char* pszC = m_poFeatureDefn->GetGeomFieldDefn(iGeomCol)->GetNameRef();

    if( poFilterGeom != NULL )
    {
        OGREnvelope  sEnvelope;

        CPLLocaleC  oLocaleEnforcer;

        poFilterGeom->getEnvelope( &sEnvelope );
        
        if( CPLIsInf(sEnvelope.MinX) || CPLIsInf(sEnvelope.MinY) ||
            CPLIsInf(sEnvelope.MaxX) || CPLIsInf(sEnvelope.MaxY) )
        {
            return osSpatialWHERE;
        }

        if( HasSpatialIndex() )
        {
            osSpatialWHERE.Printf("ROWID IN ( SELECT id FROM \"rtree_%s_%s\" WHERE "
                            "maxx >= %.12f AND minx <= %.12f AND maxy >= %.12f AND miny <= %.12f)",
                            pszT, pszC,
                            sEnvelope.MinX - 1e-11, sEnvelope.MaxX + 1e-11,
                            sEnvelope.MinY - 1e-11, sEnvelope.MaxY + 1e-11);
        }
        else
        {
            /* A bit inefficient but still faster than OGR filtering */
            osSpatialWHERE.Printf(
                        "(ST_MaxX(\"%s\") >= %.12f AND ST_MinX(\"%s\") <= %.12f AND "
                        "ST_MaxY(\"%s\") >= %.12f AND ST_MinY(\"%s\") <= %.12f)",
                        pszC, sEnvelope.MinX - 1e-11,
                        pszC, sEnvelope.MaxX + 1e-11,
                        pszC, sEnvelope.MinY - 1e-11,
                        pszC, sEnvelope.MaxY + 1e-11);
        }
    }

    return osSpatialWHERE;
}
/************************************************************************/
/*                             BuildWhere()                             */
/*                                                                      */
/*      Build the WHERE statement appropriate to the current set of     */
/*      criteria (spatial and attribute queries).                       */
/************************************************************************/

void OGRGeoPackageTableLayer::BuildWhere()

{
    m_soFilter = "";

    CPLString osSpatialWHERE = GetSpatialWhere(m_iGeomFieldFilter,
                                               m_poFilterGeom);
    if (osSpatialWHERE.size() != 0)
    {
        m_soFilter += osSpatialWHERE;
    }

    if( osQuery.size() > 0 )
    {
        if( m_soFilter.size() == 0 )
        {
            m_soFilter += osQuery;
        }
        else    
        {
            m_soFilter += " AND (";
            m_soFilter += osQuery;
            m_soFilter += ")";
        }
    }
}
