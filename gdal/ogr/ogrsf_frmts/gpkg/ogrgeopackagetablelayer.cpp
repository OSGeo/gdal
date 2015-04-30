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
#include "cpl_time.h"
#include "ogr_p.h"

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
    if ( ! m_poFeatureDefn )
    {
        return OGRERR_FAILURE;
    }

    CPLFree(panFieldOrdinals);
    panFieldOrdinals = (int *) CPLMalloc( sizeof(int) * m_poFeatureDefn->GetFieldCount() );

    /* Always start with a primary key */
    CPLString soColumns = m_pszFidColumn ? m_pszFidColumn : "_rowid_";
    CPLString soColumn;
    iFIDCol = 0;

    /* Add a geometry column if there is one (just one) */
    if ( m_poFeatureDefn->GetGeomFieldCount() )
    {
        soColumns += ", ";
        soColumn.Printf("\"%s\"", m_poFeatureDefn->GetGeomFieldDefn(0)->GetNameRef());
        soColumns += soColumn;
        iGeomCol = 1;
    }

    /* Add all the attribute columns */
    for( int i = 0; i < m_poFeatureDefn->GetFieldCount(); i++ )
    {
        soColumns += ", ";
        soColumn.Printf("\"%s\"", m_poFeatureDefn->GetFieldDefn(i)->GetNameRef());
        soColumns += soColumn;
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
                                                       int bAddFID,
                                                       int bBindNullFields )
{
    int nColCount = 1;
    int err;
    
    if ( ! (poFeature && poStmt && pnColCount) )
        return OGRERR_FAILURE;

    OGRFeatureDefn *poFeatureDefn = poFeature->GetDefnRef();

    if( bAddFID )
    {
        err = sqlite3_bind_int64(poStmt, nColCount++, poFeature->GetFID());
    }

    /* Bind data values to the statement, here bind the blob for geometry */
    if ( poFeatureDefn->GetGeomFieldCount() )
    {
        GByte *pabyWkb = NULL;

        /* Non-NULL geometry */
        OGRGeometry* poGeom = poFeature->GetGeomFieldRef(0);
        if ( poGeom )
        {
            size_t szWkb;
            pabyWkb = GPkgGeometryFromOGR(poGeom, m_iSrs, &szWkb);
            err = sqlite3_bind_blob(poStmt, nColCount++, pabyWkb, szWkb, CPLFree);

            // FIXME: in case the geometry is a GeometryCollection, we should
            // inspect its subgeometries to see if there's non-linear ones.
            if( OGR_GT_IsNonLinear(poGeom->getGeometryType()) )
                CreateGeometryExtensionIfNecessary(poGeom->getGeometryType());
        }
        /* NULL geometry */
        else
        {
            err = sqlite3_bind_null(poStmt, nColCount++);
        }
        if ( err != SQLITE_OK )
        {
            CPLError( CE_Failure, CPLE_AppDefined,
                      "failed to bind geometry to statement");        
            return OGRERR_FAILURE;            
        }
    }

    /* Bind the attributes using appropriate SQLite data types */
    for( int i = 0; i < poFeatureDefn->GetFieldCount(); i++ )
    {
        if( i == m_iFIDAsRegularColumnIndex )
            continue;
        OGRFieldDefn *poFieldDefn = poFeatureDefn->GetFieldDefn(i); 
        
        if( poFeature->IsFieldSet(i) )
        {
            switch(SQLiteFieldFromOGR(poFieldDefn->GetType()))
            {
                case SQLITE_INTEGER:
                {
                    err = sqlite3_bind_int64(poStmt, nColCount++, poFeature->GetFieldAsInteger64(i));
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
                    int nValLengthBytes = (int)strlen(pszVal);
                    char szVal[32];
                    int nYear, nMonth, nDay, nHour, nMinute, nSecond, nTZFlag;
                    CPLString osTemp;
                    if( poFieldDefn->GetType() == OFTDate )
                    {
                        poFeature->GetFieldAsDateTime(i, &nYear, &nMonth, &nDay, &nHour, &nMinute, &nSecond, &nTZFlag);
                        snprintf(szVal, sizeof(szVal), "%04d-%02d-%02d", nYear, nMonth, nDay);
                        pszVal = szVal;
                        nValLengthBytes = (int)strlen(pszVal);
                    }
                    else if( poFieldDefn->GetType() == OFTDateTime )
                    {
                        float fSecond;
                        poFeature->GetFieldAsDateTime(i, &nYear, &nMonth, &nDay, &nHour, &nMinute, &fSecond, &nTZFlag);
                        if( nTZFlag == 0 || nTZFlag == 100 )
                        {
                            if( OGR_GET_MS(fSecond) )
                                snprintf(szVal, sizeof(szVal), "%04d-%02d-%02dT%02d:%02d:%06.3fZ",
                                     nYear, nMonth, nDay, nHour, nMinute, fSecond);
                            else
                                snprintf(szVal, sizeof(szVal), "%04d-%02d-%02dT%02d:%02d:%02dZ",
                                     nYear, nMonth, nDay, nHour, nMinute, (int)fSecond);
                            pszVal = szVal;
                            nValLengthBytes = (int)strlen(pszVal);
                        }
                    }
                    else if( poFieldDefn->GetType() == OFTString &&
                             poFieldDefn->GetWidth() > 0 )
                    {
                        if( !CPLIsUTF8(pszVal, -1) )
                        {
                            CPLError(CE_Warning, CPLE_AppDefined,
                                     "Value of field '%s' is not a valid UTF-8 string.%s",
                                     poFeatureDefn->GetFieldDefn(i)->GetNameRef(),
                                     m_bTruncateFields ? " Value will be laundered." : "");
                            if( m_bTruncateFields )
                            {
                                char* pszTemp = CPLForceToASCII(pszVal, -1, '_');
                                osTemp = pszTemp;
                                pszVal = osTemp.c_str();
                                CPLFree(pszTemp);
                            }
                        }

                        if( CPLStrlenUTF8(pszVal) > poFieldDefn->GetWidth() )
                        {
                            CPLError(CE_Warning, CPLE_AppDefined,
                                     "Value of field '%s' has %d characters, whereas maximum allowed is %d.%s",
                                     poFeatureDefn->GetFieldDefn(i)->GetNameRef(),
                                     CPLStrlenUTF8(pszVal),
                                     poFieldDefn->GetWidth(),
                                     m_bTruncateFields ? " Value will be truncated." : "");
                            if( m_bTruncateFields )
                            {
                                int k = 0;
                                nValLengthBytes = 0;
                                while (pszVal[nValLengthBytes])
                                {
                                    if ((pszVal[nValLengthBytes] & 0xc0) != 0x80)
                                    {
                                        k++;
                                        // Stop at the start of the character just beyond the maximum accepted
                                        if( k > poFieldDefn->GetWidth() )
                                            break;
                                    }
                                    nValLengthBytes++;
                                }
                            }
                        }
                    }
                    err = sqlite3_bind_text(poStmt, nColCount++, pszVal, nValLengthBytes, SQLITE_TRANSIENT);
                    break;
                }            
            }            
        }
        else
        {
            if( bBindNullFields )
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
    OGRErr err = FeatureBindParameters( poFeature, poStmt, &nColCount, FALSE, TRUE );
    if ( err != OGRERR_NONE )
        return err;

    /* Bind the FID to the "WHERE" clause */
    err = sqlite3_bind_int64(poStmt, nColCount, poFeature->GetFID());    
    if ( err != SQLITE_OK )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "failed to bind FID '" CPL_FRMT_GIB "' to statement", poFeature->GetFID());
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
OGRErr OGRGeoPackageTableLayer::FeatureBindInsertParameters( OGRFeature *poFeature,
                                                             sqlite3_stmt *poStmt,
                                                             int bAddFID,
                                                             int bBindNullFields )
{    
    int nColCount;
    return FeatureBindParameters( poFeature, poStmt, &nColCount, bAddFID, bBindNullFields );
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
CPLString OGRGeoPackageTableLayer::FeatureGenerateInsertSQL( OGRFeature *poFeature,
                                                             int bAddFID,
                                                             int bBindNullFields )
{
    OGRBoolean bNeedComma = FALSE;
    OGRFeatureDefn *poFeatureDefn = poFeature->GetDefnRef();

    if( poFeatureDefn->GetFieldCount() == ((m_iFIDAsRegularColumnIndex >= 0) ? 1 : 0) &&
        poFeatureDefn->GetGeomFieldCount() == 0 &&
        !bAddFID )
        return CPLSPrintf("INSERT INTO \"%s\" DEFAULT VALUES", m_pszTableName);

    /* Set up our SQL string basics */
    CPLString osSQLFront;
    osSQLFront.Printf("INSERT INTO \"%s\" ( ", m_pszTableName);

    CPLString osSQLBack;
    osSQLBack = ") VALUES (";

    CPLString osSQLColumn;
    
    if( bAddFID )
    {
        osSQLColumn.Printf("\"%s\"", GetFIDColumn());
        osSQLFront += osSQLColumn;
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

        osSQLColumn.Printf("\"%s\"", poFeatureDefn->GetGeomFieldDefn(0)->GetNameRef());
        osSQLFront += osSQLColumn;
        osSQLBack += "?";
        bNeedComma = TRUE;
    }

    /* Add attribute column names (except FID) to the SQL */
    for( int i = 0; i < poFeatureDefn->GetFieldCount(); i++ )
    {
        if( i == m_iFIDAsRegularColumnIndex )
            continue;
        if( !bBindNullFields && !poFeature->IsFieldSet(i) )
            continue;

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
    
    if( !bNeedComma )
        return CPLSPrintf("INSERT INTO \"%s\" DEFAULT VALUES", m_pszTableName);

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
        if( i == m_iFIDAsRegularColumnIndex )
            continue;
        if( !bNeedComma )
            bNeedComma = TRUE;
        else 
            osUpdate += ", ";

        osSQLColumn.Printf("\"%s\"", poFeatureDefn->GetFieldDefn(i)->GetNameRef());
        osUpdate += osSQLColumn;
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
    CPLString osGeomColumnName;
    CPLString osGeomColsType;
    int bHasZ = FALSE;

    /* Check that the table name is registered in gpkg_contents */
    pszSQL = sqlite3_mprintf(
                "SELECT table_name, data_type, identifier, "
                "description, min_x, min_y, max_x, max_y, srs_id "
                "FROM gpkg_contents "
                "WHERE table_name = '%q'",
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
    
    const char* pszIdentifier = SQLResultGetValue(&oResultContents, 2, 0);
    if( pszIdentifier && strcmp(pszIdentifier, m_pszTableName) != 0 )
        OGRLayer::SetMetadataItem("IDENTIFIER", pszIdentifier);
    const char* pszDescription = SQLResultGetValue(&oResultContents, 3, 0);
    if( pszDescription && pszDescription[0] )
        OGRLayer::SetMetadataItem("DESCRIPTION", pszDescription);

    if( bIsSpatial )
    {
        const char *pszMinX = SQLResultGetValue(&oResultContents, 4, 0);
        const char *pszMinY = SQLResultGetValue(&oResultContents, 5, 0);
        const char *pszMaxX = SQLResultGetValue(&oResultContents, 6, 0);
        const char *pszMaxY = SQLResultGetValue(&oResultContents, 7, 0);
        
        /* All the extrema have to be non-NULL for this to make sense */
        if ( pszMinX && pszMinY && pszMaxX && pszMaxY )
        {
            oExtent.MinX = CPLAtof(pszMinX);
            oExtent.MinY = CPLAtof(pszMinY);
            oExtent.MaxX = CPLAtof(pszMaxX);
            oExtent.MaxY = CPLAtof(pszMaxY);
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

        const char* pszGeomColName = SQLResultGetValue(&oResultGeomCols, 1, 0);
        if( pszGeomColName != NULL )
            osGeomColumnName = pszGeomColName;
        const char* pszGeomColsType = SQLResultGetValue(&oResultGeomCols, 2, 0);
        if( pszGeomColsType != NULL )
            osGeomColsType = pszGeomColsType;
        m_iSrs = SQLResultGetValueAsInteger(&oResultGeomCols, 3, 0);
        bHasZ = SQLResultGetValueAsInteger(&oResultGeomCols, 4, 0);

        SQLResultFree(&oResultGeomCols);
    }
    else
        SQLResultFree(&oResultContents);

    /* Use the "PRAGMA TABLE_INFO()" call to get table definition */
    /*  #|name|type|notnull|default|pk */
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
    SetDescription( m_poFeatureDefn->GetName() );
    m_poFeatureDefn->SetGeomType(wkbNone);
    m_poFeatureDefn->Reference();
    
    int iRecord;
    OGRBoolean bFidFound = FALSE;

    for ( iRecord = 0; iRecord < oResultTable.nRowCount; iRecord++ )
    {
        const char *pszName = SQLResultGetValue(&oResultTable, 1, iRecord);
        const char *pszType = SQLResultGetValue(&oResultTable, 2, iRecord);
        int bNotNull = SQLResultGetValueAsInteger(&oResultTable, 3, iRecord);
        const char* pszDefault = SQLResultGetValue(&oResultTable, 4, iRecord);
        OGRBoolean bFid = SQLResultGetValueAsInteger(&oResultTable, 5, iRecord);
        OGRFieldSubType eSubType;
        int nMaxWidth;
        OGRFieldType oType = GPkgFieldToOGR(pszType, eSubType, nMaxWidth);

        /* Not a standard field type... */
        if ( (oType > OFTMaxType && osGeomColsType.size()) || EQUAL(osGeomColumnName, pszName) )
        {
            /* Maybe it's a geometry type? */
            OGRwkbGeometryType oGeomType;
            if( oType > OFTMaxType )
                oGeomType = GPkgGeometryTypeToWKB(pszType, bHasZ);
            else
                oGeomType = wkbUnknown;
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
                    if( bNotNull )
                        oGeomField.SetNullable(FALSE);
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
            if ( bFid && (oType == OFTInteger || oType == OFTInteger64) )
            {
                if( bFidFound )
                {
                    CPLDebug("GPKG", "For table %s, a new FID column has been found (%s). Keeping previous one (%s)",
                             m_pszTableName, pszName, m_pszFidColumn);
                }
                else
                {
                    bFidFound = TRUE;
                    m_pszFidColumn = CPLStrdup(pszName);
                }
            }
            else
            {
                OGRFieldDefn oField(pszName, oType);
                oField.SetSubType(eSubType);
                oField.SetWidth(nMaxWidth);
                if( bNotNull )
                    oField.SetNullable(FALSE);
                if( pszDefault != NULL )
                {
                    int nYear, nMonth, nDay, nHour, nMinute;
                    float fSecond;
                    if( oField.GetType() == OFTString &&
                        !EQUAL(pszDefault, "NULL") &&
                        !EQUALN(pszDefault, "CURRENT_", strlen("CURRENT_")) &&
                        pszDefault[0] != '(' &&
                        pszDefault[0] != '\'' &&
                        CPLGetValueType(pszDefault) == CPL_VALUE_STRING )
                    {
                        CPLString osDefault("'");
                        char* pszTmp = CPLEscapeString(pszDefault, -1, CPLES_SQL);
                        osDefault += pszTmp;
                        CPLFree(pszTmp);
                        osDefault += "'";
                        oField.SetDefault(osDefault);
                    }
                    else if( oType == OFTDateTime &&
                             sscanf(pszDefault, "'%d-%d-%dT%d:%d:%fZ'", &nYear, &nMonth, &nDay,
                                        &nHour, &nMinute, &fSecond) == 6 )
                    {
                        if( strchr(pszDefault, '.') == NULL )
                            oField.SetDefault(CPLSPrintf("'%04d/%02d/%02d %02d:%02d:%02d'",
                                                      nYear, nMonth, nDay, nHour, nMinute, (int)(fSecond+0.5)));
                        else
                            oField.SetDefault(CPLSPrintf("'%04d/%02d/%02d %02d:%02d:%06.3f'",
                                                            nYear, nMonth, nDay, nHour, nMinute, fSecond));
                    }
                    else if( (oField.GetType() == OFTDate || oField.GetType() == OFTDateTime) &&
                             !EQUAL(pszDefault, "NULL") &&
                             !EQUALN(pszDefault, "CURRENT_", strlen("CURRENT_")) &&
                             pszDefault[0] != '(' &&
                             pszDefault[0] != '\'' &&
                             !(pszDefault[0] >= '0' && pszDefault[0] <= '9') &&
                            CPLGetValueType(pszDefault) == CPL_VALUE_STRING )
                    {
                        CPLString osDefault("(");
                        osDefault += pszDefault;
                        osDefault += ")";
                        if( EQUAL(osDefault, "(strftime('%Y-%m-%dT%H:%M:%fZ','now'))") )
                            oField.SetDefault("CURRENT_TIMESTAMP");
                        else
                            oField.SetDefault(osDefault);
                    }
                    else
                    {
                        oField.SetDefault(pszDefault);
                    }
                }
                m_poFeatureDefn->AddFieldDefn(&oField);
            }
        }
    }

    /* Wait, we didn't find a FID? Some operations will not be possible */
    if ( ! bFidFound )
    {
        CPLDebug("GPKG", 
                 "no integer primary key defined for table '%s'", m_pszTableName);
    }
    else
    {
    /* -------------------------------------------------------------------- */
    /*      Find if the FID holds 64bit values                              */
    /* -------------------------------------------------------------------- */
        const char* pszSQLStatic = CPLSPrintf("SELECT MAX(%s) FROM '%s'",
                            OGRSQLiteEscape(m_pszFidColumn).c_str(),
                            m_pszTableName);
        sqlite3_stmt* hColStmt = NULL;
        int rc = sqlite3_prepare( poDb, pszSQLStatic, strlen(pszSQLStatic), &hColStmt, NULL ); 
        if( rc == SQLITE_OK )
        {
            rc = sqlite3_step( hColStmt );
            if( rc == SQLITE_ROW )
            {
                GIntBig nMaxId = sqlite3_column_int64( hColStmt, 0 );
                if( nMaxId > INT_MAX )
                    OGRLayer::SetMetadataItem(OLMD_FID64, "YES");
            }
        }
        sqlite3_finalize( hColStmt );
    }

    if ( bReadExtent )
    {
        m_poExtent = new OGREnvelope(oExtent);
    }

    SQLResultFree(&oResultTable);

    /* Update the columns string */
    BuildColumns();
    
    CheckUnknownExtensions();

    return OGRERR_NONE;
}


/************************************************************************/
/*                      OGRGeoPackageTableLayer()                       */
/************************************************************************/

OGRGeoPackageTableLayer::OGRGeoPackageTableLayer(
                    GDALGeoPackageDataset *poDS,
                    const char * pszTableName) : OGRGeoPackageLayer(poDS)
{
    m_pszTableName = CPLStrdup(pszTableName);
    m_iSrs = 0;
    m_poExtent = NULL;
    m_bExtentChanged = FALSE;
    m_poQueryStatement = NULL;
    m_poUpdateStatement = NULL;
    m_bInsertStatementWithFID = FALSE;
    m_poInsertStatement = NULL;
    m_soColumns = "";
    m_soFilter = "";
    bDeferedSpatialIndexCreation = FALSE;
    m_bHasSpatialIndex = -1;
    bDropRTreeTable = FALSE;
    memset(m_anHasGeometryExtension, 0, sizeof(m_anHasGeometryExtension));
    m_bPreservePrecision = TRUE;
    m_bTruncateFields = FALSE;
    m_bDeferredCreation = FALSE;
    m_iFIDAsRegularColumnIndex = -1;
    m_bHasReadMetadataFromStorage = FALSE;
}


/************************************************************************/
/*                      ~OGRGeoPackageTableLayer()                      */
/************************************************************************/

OGRGeoPackageTableLayer::~OGRGeoPackageTableLayer()
{
    if( m_bDeferredCreation )
        RunDeferredCreationIfNecessary();

    if( bDropRTreeTable )
    {
        const char* pszT = m_pszTableName;
        const char* pszC =m_poFeatureDefn->GetGeomFieldDefn(0)->GetNameRef();
        char* pszSQL;
        pszSQL = sqlite3_mprintf("DROP TABLE \"rtree_%s_%s\"", pszT, pszC);
        SQLCommand(m_poDS->GetDB(), pszSQL);
        sqlite3_free(pszSQL);
    }
    else
    {
        CreateSpatialIndexIfNecessary();
    }

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

OGRErr OGRGeoPackageTableLayer::CreateField( OGRFieldDefn *poField,
                                             CPL_UNUSED int bApproxOK )
{
    OGRFieldDefn oFieldDefn(poField);
    if( !m_poDS->GetUpdate() )
    {
        return OGRERR_FAILURE;
    }
    
    int nMaxWidth = 0;
    if( m_bPreservePrecision && poField->GetType() == OFTString )
        nMaxWidth = poField->GetWidth();
    else
        oFieldDefn.SetWidth(0);
    oFieldDefn.SetPrecision(0);
    
    if( m_pszFidColumn != NULL &&
        EQUAL( oFieldDefn.GetNameRef(), m_pszFidColumn ) &&
        oFieldDefn.GetType() != OFTInteger &&
        oFieldDefn.GetType() != OFTInteger64 )
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Wrong field type for %s",
                 oFieldDefn.GetNameRef());
        return CE_Failure;
    }

    if( !m_bDeferredCreation )
    {
        CPLString osCommand;
    
        osCommand.Printf("ALTER TABLE \"%s\" ADD COLUMN \"%s\" %s", 
                                 m_pszTableName, poField->GetNameRef(),
                                 GPkgFieldFromOGR(poField->GetType(),
                                                  poField->GetSubType(),
                                                  nMaxWidth));
        if(  !poField->IsNullable() )
            osCommand += " NOT NULL";
        if( poField->GetDefault() != NULL && !poField->IsDefaultDriverSpecific() )
        {
            osCommand += " DEFAULT ";
            int nYear, nMonth, nDay, nHour, nMinute;
            float fSecond;
            if( poField->GetType() == OFTDateTime &&
                sscanf(poField->GetDefault(), "'%d/%d/%d %d:%d:%f'", &nYear, &nMonth, &nDay,
                                        &nHour, &nMinute, &fSecond) == 6 )
            {
                if( strchr(poField->GetDefault(), '.') == NULL )
                    osCommand += CPLSPrintf("'%04d-%02d-%02dT%02d:%02d:%02dZ'",
                                        nYear, nMonth, nDay, nHour, nMinute, (int)(fSecond+0.5));
                else
                    osCommand += CPLSPrintf("'%04d-%02d-%02dT%02d:%02d:%06.3fZ'",
                                            nYear, nMonth, nDay, nHour, nMinute, fSecond);
            }
            else
                osCommand += poField->GetDefault();
        }
        else if( !poField->IsNullable() )
        {
            // This is kind of dumb, but SQLite mandates a DEFAULT value
            // when adding a NOT NULL column in an ALTER TABLE ADD COLUMN
            // statement, which defeats the purpose of NOT NULL,
            // whereas it doesn't in CREATE TABLE
            osCommand += " DEFAULT ''";
        }

        OGRErr err = SQLCommand(m_poDS->GetDB(), osCommand.c_str());

        if ( err != OGRERR_NONE )
            return err;
    }

    m_poFeatureDefn->AddFieldDefn( &oFieldDefn );

    if( m_pszFidColumn != NULL &&
        EQUAL( oFieldDefn.GetNameRef(), m_pszFidColumn ) )
    {
        m_iFIDAsRegularColumnIndex = m_poFeatureDefn->GetFieldCount() - 1;
    }

    if( !m_bDeferredCreation )
    {
        ResetReading();
    }

    return OGRERR_NONE;
}

/************************************************************************/
/*                           CreateGeomField()                          */
/************************************************************************/

OGRErr OGRGeoPackageTableLayer::CreateGeomField( OGRGeomFieldDefn *poGeomFieldIn,
                                                 CPL_UNUSED int bApproxOK )
{
    if( m_poFeatureDefn->GetGeomFieldCount() == 1 )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Cannot create more than on geometry field in GeoPackage");
        return OGRERR_FAILURE;
    }
    
    OGRwkbGeometryType eType = poGeomFieldIn->GetType();
    if( eType == wkbNone )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Cannot create geometry field of type wkbNone");
        return OGRERR_FAILURE;
    }

    OGRGeomFieldDefn oGeomField(poGeomFieldIn);
    if( EQUAL(oGeomField.GetNameRef(), "") )
    {
        oGeomField.SetName( "geom" );
    }

    OGRSpatialReference* poSRS = oGeomField.GetSpatialRef();
    if( poSRS != NULL )
        m_iSrs = m_poDS->GetSrsId(poSRS);

/* -------------------------------------------------------------------- */
/*      Create the new field.                                           */
/* -------------------------------------------------------------------- */
    if( !m_bDeferredCreation )
    {
        char *pszSQL;

        pszSQL = sqlite3_mprintf("ALTER TABLE \"%s\" ADD COLUMN \"%s\" %s%s", 
                                 m_pszTableName, oGeomField.GetNameRef(),
                                 m_poDS->GetGeometryTypeString(oGeomField.GetType()),
                                 !oGeomField.IsNullable() ? " NOT NULL DEFAULT ''" : "");

        OGRErr err = SQLCommand(m_poDS->GetDB(), pszSQL);
        sqlite3_free(pszSQL);

        if ( err != OGRERR_NONE )
            return err;

        pszSQL = sqlite3_mprintf(
            "UPDATE gpkg_contents SET data_type = 'features' WHERE table_name = '%q'",
            GetName());
        err = SQLCommand(m_poDS->GetDB(), pszSQL);
        sqlite3_free(pszSQL);
        if ( err != OGRERR_NONE )
            return OGRERR_FAILURE;

        int bHasASpatialLayers = FALSE;
        for(int i=0;i<m_poDS->GetLayerCount();i++)
        {
            if( m_poDS->GetLayer(i) != this &&
                m_poDS->GetLayer(i)->GetLayerDefn()->GetGeomFieldCount() == 0 )
                bHasASpatialLayers = TRUE;
        }
        if( !bHasASpatialLayers )
        {
            err = SQLCommand(m_poDS->GetDB(),
                             "DELETE FROM gpkg_extensions WHERE "
                             "extension_name = 'gdal_aspatial' "
                             "AND table_name IS NULL "
                             "AND column_name IS NULL");
            if ( err != OGRERR_NONE )
                return OGRERR_FAILURE;
        }
    }

    m_poFeatureDefn->AddGeomFieldDefn( &oGeomField );

    if( !m_bDeferredCreation )
    {
        OGRErr err = RegisterGeometryColumn();
        if ( err != OGRERR_NONE )
            return err;

        ResetReading();
    }

    return OGRERR_NONE;
}

/************************************************************************/
/*                      ICreateFeature()                                 */
/************************************************************************/

OGRErr OGRGeoPackageTableLayer::ICreateFeature( OGRFeature *poFeature )
{
    if( !m_poDS->GetUpdate() )
    {
        return OGRERR_FAILURE;
    }

    if( m_bDeferredCreation && RunDeferredCreationIfNecessary() != OGRERR_NONE )
        return OGRERR_FAILURE;

    /* Substitute default values for null Date/DateTime fields as the standard */
    /* format of SQLite is not the one mandated by GeoPackage */
    poFeature->FillUnsetWithDefault(FALSE, NULL);
    int bHasDefaultValue = FALSE;
    int iField;
    int nFieldCount = m_poFeatureDefn->GetFieldCount();
    for( iField = 0; iField < nFieldCount; iField++ )
    {
        if( poFeature->IsFieldSet( iField ) )
            continue;
        const char* pszDefault = poFeature->GetFieldDefnRef(iField)->GetDefault();
        if( pszDefault != NULL )
        {
            bHasDefaultValue = TRUE;
            break;
        }
    }

    /* In case the FID column has also been created as a regular field */
    if( m_iFIDAsRegularColumnIndex >= 0 )
    {
        if( poFeature->GetFID() == OGRNullFID )
        {
            if( poFeature->IsFieldSet( m_iFIDAsRegularColumnIndex ) )
            {
                poFeature->SetFID(
                    poFeature->GetFieldAsInteger64(m_iFIDAsRegularColumnIndex));
            }
        }
        else
        {
            if( !poFeature->IsFieldSet( m_iFIDAsRegularColumnIndex ) ||
                poFeature->GetFieldAsInteger64(m_iFIDAsRegularColumnIndex) != poFeature->GetFID() )
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                            "Inconsistant values of FID and field of same name");
                return CE_Failure;
            }
        }
    }

    /* If there's a unset field with a default value, then we must create */
    /* a specific INSERT statement to avoid unset fields to be bound to NULL */
    if( m_poInsertStatement && (bHasDefaultValue || m_bInsertStatementWithFID != (poFeature->GetFID() != OGRNullFID)) )
    {
        sqlite3_finalize(m_poInsertStatement);
        m_poInsertStatement = NULL;
    }
    
    if ( ! m_poInsertStatement ) 
    {
        /* Construct a SQL INSERT statement from the OGRFeature */
        /* Only work with fields that are set */
        /* Do not stick values into SQL, use placeholder and bind values later */    
        m_bInsertStatementWithFID = poFeature->GetFID() != OGRNullFID;
        CPLString osCommand = FeatureGenerateInsertSQL(poFeature, m_bInsertStatementWithFID, !bHasDefaultValue);
        
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
    OGRErr errOgr = FeatureBindInsertParameters(poFeature, m_poInsertStatement,
                                                m_bInsertStatementWithFID, !bHasDefaultValue);
    if ( errOgr != OGRERR_NONE )
    {
        sqlite3_reset(m_poInsertStatement);
        sqlite3_clear_bindings(m_poInsertStatement);
        sqlite3_finalize(m_poInsertStatement);
        m_poInsertStatement = NULL;
        return errOgr;
    }

    /* From here execute the statement and check errors */
    int err = sqlite3_step(m_poInsertStatement);
    if ( ! (err == SQLITE_OK || err == SQLITE_DONE) )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "failed to execute insert : %s",
                  sqlite3_errmsg(m_poDS->GetDB()) ? sqlite3_errmsg(m_poDS->GetDB()) : "");
        sqlite3_reset(m_poInsertStatement);
        sqlite3_clear_bindings(m_poInsertStatement);
        sqlite3_finalize(m_poInsertStatement);
        m_poInsertStatement = NULL;
        return OGRERR_FAILURE; 
    }

    sqlite3_reset(m_poInsertStatement);
    sqlite3_clear_bindings(m_poInsertStatement);
    
    if( bHasDefaultValue )
    {
        sqlite3_finalize(m_poInsertStatement);
        m_poInsertStatement = NULL;
    }

    /* Update the layer extents with this new object */
    if ( IsGeomFieldSet(poFeature) )
    {
        OGREnvelope oEnv;
        poFeature->GetGeomFieldRef(0)->getEnvelope(&oEnv);
        UpdateExtent(&oEnv);
    }

    /* Read the latest FID value */
    GIntBig nFID;
    if ( (nFID = sqlite3_last_insert_rowid(m_poDS->GetDB())) )
    {
        poFeature->SetFID(nFID);
        if( m_iFIDAsRegularColumnIndex >= 0 )
            poFeature->SetField( m_iFIDAsRegularColumnIndex, nFID );
    }
    else
    {
        poFeature->SetFID(OGRNullFID);
    }
    
    /* All done! */
    return OGRERR_NONE;
}


/************************************************************************/
/*                          ISetFeature()                                */
/************************************************************************/

OGRErr OGRGeoPackageTableLayer::ISetFeature( OGRFeature *poFeature )
{
    if( !m_poDS->GetUpdate() || m_pszFidColumn == NULL )
    {
        return OGRERR_FAILURE;
    }

    /* No FID? We can't set, we have to create */
    if ( poFeature->GetFID() == OGRNullFID )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "FID required on features given to SetFeature()." );
        return OGRERR_FAILURE;
    }

    /* In case the FID column has also been created as a regular field */
    if( m_iFIDAsRegularColumnIndex >= 0 )
    {
        if( !poFeature->IsFieldSet( m_iFIDAsRegularColumnIndex ) ||
            poFeature->GetFieldAsInteger64(m_iFIDAsRegularColumnIndex) != poFeature->GetFID() )
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                        "Inconsistant values of FID and field of same name");
            return CE_Failure;
        }
    }

    if( m_bDeferredCreation && RunDeferredCreationIfNecessary() != OGRERR_NONE )
        return OGRERR_FAILURE;

    /* Old version of SQLite have issues with some of the spatial index triggers */
#if SQLITE_VERSION_NUMBER < 3007008
    if( HasSpatialIndex() )
    {
        if ( ! m_poUpdateStatement )
        {
            /* Construct a SQL INSERT statement from the OGRFeature */
            /* Only work with fields that are set */
            /* Do not stick values into SQL, use placeholder and bind values later */    
            CPLString osCommand = FeatureGenerateInsertSQL(poFeature, TRUE, TRUE);

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

        OGRErr errOgr = DeleteFeature( poFeature->GetFID() );

        m_poUpdateStatement = hBackupStmt;

        if ( errOgr != OGRERR_NONE )
            return errOgr;

        /* Bind values onto the statement now */
        errOgr = FeatureBindInsertParameters(poFeature, m_poUpdateStatement, TRUE, TRUE);
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
        {
            sqlite3_reset(m_poUpdateStatement);
            sqlite3_clear_bindings(m_poUpdateStatement);
            return errOgr;
        }
    }

    /* From here execute the statement and check errors */
    int err = sqlite3_step(m_poUpdateStatement);
    if ( ! (err == SQLITE_OK || err == SQLITE_DONE) )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "failed to execute update : %s",
                  sqlite3_errmsg( m_poDS->GetDB() ) );
        sqlite3_reset(m_poUpdateStatement);
        sqlite3_clear_bindings(m_poUpdateStatement);
        return OGRERR_FAILURE;       
    }

    sqlite3_reset(m_poUpdateStatement);
    sqlite3_clear_bindings(m_poUpdateStatement);

    /* Only update the envelope if we changed something */
    OGRErr eErr = (sqlite3_changes(m_poDS->GetDB()) > 0) ? OGRERR_NONE : OGRERR_NON_EXISTING_FEATURE;
    if (eErr == OGRERR_NONE)
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
    return eErr;
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
    if( m_bDeferredCreation && RunDeferredCreationIfNecessary() != OGRERR_NONE )
        return;

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
    if( m_bDeferredCreation && RunDeferredCreationIfNecessary() != OGRERR_NONE )
        return NULL;

    CreateSpatialIndexIfNecessary();

    OGRFeature* poFeature = OGRGeoPackageLayer::GetNextFeature();
    if( poFeature && m_iFIDAsRegularColumnIndex >= 0 )
    {
        poFeature->SetField(m_iFIDAsRegularColumnIndex, poFeature->GetFID());
    }
    return poFeature;
}

/************************************************************************/
/*                        GetFeature()                                  */
/************************************************************************/

OGRFeature* OGRGeoPackageTableLayer::GetFeature(GIntBig nFID)
{
    /* No FID, no answer. */
    if (nFID == OGRNullFID || m_pszFidColumn == NULL )
        return NULL;

    if( m_bDeferredCreation && RunDeferredCreationIfNecessary() != OGRERR_NONE )
        return NULL;

    CreateSpatialIndexIfNecessary();
    
    /* Clear out any existing query */
    ResetReading();

    /* No filters apply, just use the FID */
    CPLString soSQL;
    soSQL.Printf("SELECT %s FROM \"%s\" WHERE \"%s\" = " CPL_FRMT_GIB,
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
        OGRFeature* poFeature = TranslateFeature(m_poQueryStatement);
        if( poFeature && m_iFIDAsRegularColumnIndex >= 0 )
        {
            poFeature->SetField(m_iFIDAsRegularColumnIndex, poFeature->GetFID());
        }
        return poFeature;
    }
    
    /* Error out on all other return codes */
    return NULL;
}

/************************************************************************/
/*                        DeleteFeature()                               */
/************************************************************************/

OGRErr OGRGeoPackageTableLayer::DeleteFeature(GIntBig nFID) 
{
    if( !m_poDS->GetUpdate() || m_pszFidColumn == NULL )
    {
        return OGRERR_FAILURE;
    }
    
    /* No FID, no answer. */
    if (nFID == OGRNullFID)
    {
        CPLError( CE_Failure, CPLE_AppDefined, "delete feature called with null FID");
        return OGRERR_FAILURE;
    }

    if( m_bDeferredCreation && RunDeferredCreationIfNecessary() != OGRERR_NONE )
        return OGRERR_FAILURE;

    /* Clear out any existing query */
    ResetReading();

    /* No filters apply, just use the FID */
    CPLString soSQL;
    soSQL.Printf("DELETE FROM \"%s\" WHERE \"%s\" = " CPL_FRMT_GIB,
                 m_pszTableName, m_pszFidColumn, nFID);

    OGRErr eErr = SQLCommand(m_poDS->GetDB(), soSQL.c_str());
    if( eErr == OGRERR_NONE )
        eErr = (sqlite3_changes(m_poDS->GetDB()) > 0) ? OGRERR_NONE : OGRERR_NON_EXISTING_FEATURE;
    return eErr;
}

/************************************************************************/
/*                        SyncToDisk()                                  */
/************************************************************************/

OGRErr OGRGeoPackageTableLayer::SyncToDisk()
{
    if( m_bDeferredCreation && RunDeferredCreationIfNecessary() != OGRERR_NONE )
        return OGRERR_FAILURE;

    SaveExtent();
    return OGRERR_NONE;
}

/************************************************************************/
/*                        StartTransaction()                            */
/************************************************************************/

OGRErr OGRGeoPackageTableLayer::StartTransaction()
{
    return m_poDS->StartTransaction();
}


/************************************************************************/
/*                        CommitTransaction()                           */
/************************************************************************/

OGRErr OGRGeoPackageTableLayer::CommitTransaction()
{
    return m_poDS->CommitTransaction();
}


/************************************************************************/
/*                        RollbackTransaction()                         */
/************************************************************************/

OGRErr OGRGeoPackageTableLayer::RollbackTransaction()
{
    return m_poDS->RollbackTransaction();
}


/************************************************************************/
/*                        GetFeatureCount()                             */
/************************************************************************/

GIntBig OGRGeoPackageTableLayer::GetFeatureCount( CPL_UNUSED int bForce )
{
    if( m_poFilterGeom != NULL && !m_bFilterIsEnvelope )
        return OGRGeoPackageLayer::GetFeatureCount();

    if( m_bDeferredCreation && RunDeferredCreationIfNecessary() != OGRERR_NONE )
        return 0;

    /* Ignore bForce, because we always do a full count on the database */
    OGRErr err;
    CPLString soSQL;
    if ( m_soFilter.length() > 0 )
        soSQL.Printf("SELECT Count(*) FROM \"%s\" WHERE %s", m_pszTableName, m_soFilter.c_str());
    else
        soSQL.Printf("SELECT Count(*) FROM \"%s\" ", m_pszTableName);

    /* Just run the query directly and get back integer */
    GIntBig iFeatureCount = SQLGetInteger64(m_poDS->GetDB(), soSQL.c_str(), &err);

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

    if( m_bDeferredCreation && RunDeferredCreationIfNecessary() != OGRERR_NONE )
        return OGRERR_FAILURE;

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
    else if( EQUAL(pszCap,OLCCurveGeometries) )
        return TRUE;
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
        CreateSpatialIndex();
    }
}

/************************************************************************/
/*                       CreateSpatialIndex()                           */
/************************************************************************/

int OGRGeoPackageTableLayer::CreateSpatialIndex()
{
    char* pszSQL;
    OGRErr err;

    if( m_bDeferredCreation && RunDeferredCreationIfNecessary() != OGRERR_NONE )
        return FALSE;

    bDeferedSpatialIndexCreation = FALSE;
    
    if( m_pszFidColumn == NULL )
        return FALSE;

    if( HasSpatialIndex() )
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Spatial index already existing");
        return FALSE;
    }

    if( m_poFeatureDefn->GetGeomFieldCount() == 0 )
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Spatial index already existing");
        return FALSE;
    }
    if( m_poDS->CreateExtensionsTableIfNecessary() != OGRERR_NONE )
        return FALSE;

    const char* pszT = m_pszTableName;
    const char* pszC = m_poFeatureDefn->GetGeomFieldDefn(0)->GetNameRef();
    const char* pszI = GetFIDColumn();

    m_poDS->SoftStartTransaction();

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
        m_poDS->SoftRollbackTransaction();
        return FALSE;
    }

    /* Create virtual table */
    if( !bDropRTreeTable )
    {
        pszSQL = sqlite3_mprintf(
                    "CREATE VIRTUAL TABLE \"rtree_%s_%s\" USING rtree(id, minx, maxx, miny, maxy)",
                    pszT, pszC );
        err = SQLCommand(m_poDS->GetDB(), pszSQL);
        sqlite3_free(pszSQL);
        if( err != OGRERR_NONE )
        {
            m_poDS->SoftRollbackTransaction();
            return FALSE;
        }
    }
    bDropRTreeTable = FALSE;

    /* Populate the RTree */
    pszSQL = sqlite3_mprintf(
                 "INSERT OR REPLACE INTO \"rtree_%s_%s\" "
                 "SELECT \"%s\", st_minx(\"%s\"), st_maxx(\"%s\"), st_miny(\"%s\"), st_maxy(\"%s\") FROM \"%s\"",
                 pszT, pszC, pszI, pszC, pszC, pszC, pszC, pszT );
    err = SQLCommand(m_poDS->GetDB(), pszSQL);
    sqlite3_free(pszSQL);
    if( err != OGRERR_NONE )
    {
        m_poDS->SoftRollbackTransaction();
        return FALSE;
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
        m_poDS->SoftRollbackTransaction();
        return FALSE;
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
        m_poDS->SoftRollbackTransaction();
        return FALSE;
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
        m_poDS->SoftRollbackTransaction();
        return FALSE;
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
        m_poDS->SoftRollbackTransaction();
        return FALSE;
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
        m_poDS->SoftRollbackTransaction();
        return FALSE;
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
        m_poDS->SoftRollbackTransaction();
        return FALSE;
    }

    m_poDS->SoftCommitTransaction();

    m_bHasSpatialIndex = TRUE;

    return TRUE;
}

/************************************************************************/
/*                    CheckUnknownExtensions()                          */
/************************************************************************/

void OGRGeoPackageTableLayer::CheckUnknownExtensions()
{
    if( !m_poDS->HasExtensionsTable() )
        return;

    const char* pszT = m_pszTableName;

    /* We have only the SQL functions needed by the 3 following extensions */
    /* anything else will likely cause troubles */
    char* pszSQL;

    if( m_poFeatureDefn->GetGeomFieldCount() == 0 )
    {
        pszSQL = sqlite3_mprintf(
                    "SELECT extension_name, definition, scope FROM gpkg_extensions WHERE table_name='%q'",
                    pszT );
    }
    else
    {
        pszSQL = sqlite3_mprintf(
                    "SELECT extension_name, definition, scope FROM gpkg_extensions WHERE table_name='%q' "
                    "AND column_name='%q' AND extension_name NOT LIKE 'gpkg_geom_%s' AND extension_name NOT IN "
                    "('gpkg_rtree_index', 'gpkg_geometry_type_trigger', 'gpkg_srs_id_trigger')",
                    pszT,
                    m_poFeatureDefn->GetGeomFieldDefn(0)->GetNameRef(),
                    m_poDS->GetGeometryTypeString(m_poFeatureDefn->GetGeomFieldDefn(0)->GetType()) );
    }
    SQLResult oResultTable;
    OGRErr err = SQLQuery(m_poDS->GetDB(), pszSQL, &oResultTable);
    sqlite3_free(pszSQL);
    if ( err == OGRERR_NONE && oResultTable.nRowCount > 0 )
    {
        for(int i=0; i<oResultTable.nRowCount;i++)
        {
            const char* pszExtName = SQLResultGetValue(&oResultTable, 0, i);
            const char* pszDefinition = SQLResultGetValue(&oResultTable, 1, i);
            const char* pszScope = SQLResultGetValue(&oResultTable, 2, i);
            if( pszExtName == NULL ) pszExtName = "(null)";
            if( pszDefinition == NULL ) pszDefinition = "(null)";
            if( pszScope == NULL ) pszScope = "(null)";
            if( m_poDS->GetUpdate() && EQUAL(pszScope, "write-only") )
            {
                CPLError(CE_Warning, CPLE_AppDefined,
                         "Layer %s relies on the '%s' (%s) extension that should "
                         "be implemented for safe write-support, but is not currently. "
                         "Update of that layer are strongly discouraged to avoid corruption.",
                         GetName(), pszExtName, pszDefinition);
            }
            else if( m_poDS->GetUpdate() && EQUAL(pszScope, "read-write") )
            {
                CPLError(CE_Warning, CPLE_AppDefined,
                         "Layer %s relies on the '%s' (%s) extension that should "
                         "be implemented in order to read/write it safely, but is not currently. "
                         "Some data may be missing while reading that layer, and updates are strongly discouraged.",
                         GetName(), pszExtName, pszDefinition);
            }
            else if( EQUAL(pszScope, "read-write") )
            {
                CPLError(CE_Warning, CPLE_AppDefined,
                         "Layer %s relies on the '%s' (%s) extension that should "
                         "be implemented in order to read it safely, but is not currently. "
                         "Some data may be missing while reading that layer.",
                         GetName(), pszExtName, pszDefinition);
            }
        }
    }
    SQLResultFree(&oResultTable);
}

/************************************************************************/
/*                     CreateGeometryExtensionIfNecessary()             */
/************************************************************************/

int OGRGeoPackageTableLayer::CreateGeometryExtensionIfNecessary(OGRwkbGeometryType eGType)
{
    eGType = wkbFlatten(eGType);
    CPLAssert(eGType <= wkbMultiSurface);
    if( m_anHasGeometryExtension[eGType] )
        return TRUE;

    if( m_poDS->CreateExtensionsTableIfNecessary() != OGRERR_NONE )
        return FALSE;

    const char* pszT = m_pszTableName;
    const char* pszC = m_poFeatureDefn->GetGeomFieldDefn(0)->GetNameRef();
    const char *pszGeometryType = m_poDS->GetGeometryTypeString(eGType);

    /* Register the table in gpkg_extensions */
    char* pszSQL = sqlite3_mprintf(
                "INSERT INTO gpkg_extensions "
                "(table_name,column_name,extension_name,definition,scope) "
                "VALUES ('%q', '%q', 'gpkg_geom_%s', 'GeoPackage 1.0 Specification Annex J', 'write-only')",
                pszT, pszC, pszGeometryType);
    OGRErr err = SQLCommand(m_poDS->GetDB(), pszSQL);
    sqlite3_free(pszSQL);
    if ( err != OGRERR_NONE )
        return FALSE;

    m_anHasGeometryExtension[eGType] = TRUE;
    return TRUE;
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

int OGRGeoPackageTableLayer::DropSpatialIndex(int bCalledFromSQLFunction)
{
    if( !HasSpatialIndex() )
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Spatial index not existing");
        return FALSE;
    }

    const char* pszT = m_pszTableName;
    const char* pszC =m_poFeatureDefn->GetGeomFieldDefn(0)->GetNameRef();
    char* pszSQL;

    pszSQL = sqlite3_mprintf("DELETE FROM gpkg_extensions WHERE table_name='%q' "
                 "AND column_name='%q' AND extension_name='gpkg_rtree_index'",
                 pszT, pszC );
    SQLCommand(m_poDS->GetDB(), pszSQL);
    sqlite3_free(pszSQL);

    if( bCalledFromSQLFunction )
    {
        /* We cannot drop a table from a SQLite function call, so we just */
        /* remove the content and memorize that we will have to delete the */
        /* table later */
        bDropRTreeTable = TRUE;
        pszSQL = sqlite3_mprintf("DELETE FROM \"rtree_%s_%s\"", pszT, pszC);
    }
    else
    {
        pszSQL = sqlite3_mprintf("DROP TABLE \"rtree_%s_%s\"", pszT, pszC);
    }
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
    return TRUE;
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
/*                        HasFastSpatialFilter()                        */
/************************************************************************/

int OGRGeoPackageTableLayer::HasFastSpatialFilter(int iGeomCol)
{
    if( iGeomCol < 0 || iGeomCol >= m_poFeatureDefn->GetGeomFieldCount() )
        return FALSE;
    return HasSpatialIndex();
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

/************************************************************************/
/*                        SetCreationParameters()                       */
/************************************************************************/

void OGRGeoPackageTableLayer::SetCreationParameters( OGRwkbGeometryType eGType,
                                                     const char* pszGeomColumnName,
                                                     int bGeomNullable,
                                                     OGRSpatialReference* poSRS,
                                                     const char* pszFIDColumnName,
                                                     const char* pszIdentifier,
                                                     const char* pszDescription )
{
    m_bDeferredCreation = TRUE;
    m_pszFidColumn = CPLStrdup(pszFIDColumnName);
    m_poFeatureDefn = new OGRFeatureDefn( m_pszTableName );
    SetDescription( m_poFeatureDefn->GetName() );
    m_poFeatureDefn->SetGeomType(wkbNone);
    m_poFeatureDefn->Reference();
    if( eGType != wkbNone )
    {
        OGRGeomFieldDefn oGeomFieldDefn(pszGeomColumnName, eGType);
        if( poSRS )
            m_iSrs = m_poDS->GetSrsId(poSRS);
        oGeomFieldDefn.SetSpatialRef(poSRS);
        oGeomFieldDefn.SetNullable(bGeomNullable);
        m_poFeatureDefn->AddGeomFieldDefn(&oGeomFieldDefn);
    }
    if( pszIdentifier )
    {
        m_osIdentifierLCO = pszIdentifier;
        OGRLayer::SetMetadataItem("IDENTIFIER", pszIdentifier);
    }
    if( pszDescription )
    {
        m_osDescriptionLCO = pszDescription;
        OGRLayer::SetMetadataItem("DESCRIPTION", pszDescription);
    }
}

/************************************************************************/
/*                      RegisterGeometryColumn()                        */
/************************************************************************/

OGRErr OGRGeoPackageTableLayer::RegisterGeometryColumn()
{
    OGRwkbGeometryType eGType = GetGeomType();
    const char *pszGeometryType = m_poDS->GetGeometryTypeString(eGType);
    /* Requirement 27: The z value in a gpkg_geometry_columns table row */
    /* SHALL be one of 0 (none), 1 (mandatory), or 2 (optional) */
    int bGeometryTypeHasZ = wkbHasZ(eGType);

    /* Update gpkg_geometry_columns with the table info */
    char* pszSQL = sqlite3_mprintf(
        "INSERT INTO gpkg_geometry_columns "
        "(table_name,column_name,geometry_type_name,srs_id,z,m)"
        " VALUES "
        "('%q','%q','%q',%d,%d,%d)",
        GetName(),GetGeometryColumn(),pszGeometryType,
        m_iSrs,bGeometryTypeHasZ,0);

    OGRErr err = SQLCommand(m_poDS->GetDB(), pszSQL);
    sqlite3_free(pszSQL);
    if ( err != OGRERR_NONE )
        return OGRERR_FAILURE;

    if( OGR_GT_IsNonLinear( eGType ) )
        CreateGeometryExtensionIfNecessary(eGType);
    
    return OGRERR_NONE;
}

/************************************************************************/
/*                      RunDeferredCreationIfNecessary()                */
/************************************************************************/

OGRErr OGRGeoPackageTableLayer::RunDeferredCreationIfNecessary()
{
    if( !m_bDeferredCreation )
        return OGRERR_NONE;
    m_bDeferredCreation = FALSE;

    const char* pszLayerName = m_poFeatureDefn->GetName();
    OGRwkbGeometryType eGType = GetGeomType();

    int bIsSpatial = (eGType != wkbNone);

    /* Requirement 25: The geometry_type_name value in a gpkg_geometry_columns */
    /* row SHALL be one of the uppercase geometry type names specified in */
    /* Geometry Types (Normative). */
    const char *pszGeometryType = m_poDS->GetGeometryTypeString(eGType);

    /* Create the table! */
    char *pszSQL = NULL;
    CPLString osCommand;

    pszSQL = sqlite3_mprintf(
        "CREATE TABLE \"%s\" ( "
        "\"%s\" INTEGER PRIMARY KEY AUTOINCREMENT",
            pszLayerName, m_pszFidColumn);
    osCommand += pszSQL;
    sqlite3_free(pszSQL);
    
    if( GetGeomType() != wkbNone )
    {
        pszSQL = sqlite3_mprintf(", '%q' %s",
                                 GetGeometryColumn(), pszGeometryType);
        osCommand += pszSQL;
        sqlite3_free(pszSQL);
        if( !m_poFeatureDefn->GetGeomFieldDefn(0)->IsNullable() )
        {
            osCommand += " NOT NULL";
        }
    }

    for(int i = 0; i < m_poFeatureDefn->GetFieldCount(); i++ )
    {
        if( i == m_iFIDAsRegularColumnIndex )
            continue;
        OGRFieldDefn* poFieldDefn = m_poFeatureDefn->GetFieldDefn(i);
        pszSQL = sqlite3_mprintf(", '%q' %s",
                                 poFieldDefn->GetNameRef(),
                                 GPkgFieldFromOGR(poFieldDefn->GetType(),
                                                  poFieldDefn->GetSubType(),
                                                  poFieldDefn->GetWidth()));
        osCommand += pszSQL;
        sqlite3_free(pszSQL);
        if( !poFieldDefn->IsNullable() )
        {
            osCommand += " NOT NULL";
        }
        const char* pszDefault = poFieldDefn->GetDefault();
        if( pszDefault != NULL &&
            (!poFieldDefn->IsDefaultDriverSpecific() ||
             (pszDefault[0] == '(' && pszDefault[strlen(pszDefault)-1] == ')' &&
             (EQUALN(pszDefault+1, "strftime", strlen("strftime")) ||
              EQUALN(pszDefault+1, " strftime", strlen(" strftime"))))) )
        {
            osCommand += " DEFAULT ";
            OGRField sField;
            if( poFieldDefn->GetType() == OFTDateTime &&
                OGRParseDate(pszDefault, &sField, 0) )
            {
                char* pszXML = OGRGetXMLDateTime(&sField);
                osCommand += pszXML;
                CPLFree(pszXML);
            }
            /* Make sure CURRENT_TIMESTAMP is translated into appropriate format for GeoPackage */
            else if( poFieldDefn->GetType() == OFTDateTime &&
                     EQUAL(pszDefault, "CURRENT_TIMESTAMP") )
            {
                osCommand += "(strftime('%Y-%m-%dT%H:%M:%fZ','now'))";
            }
            else
            {
                osCommand += poFieldDefn->GetDefault();
            }
        }
    }

    osCommand += ")";

#ifdef DEBUG
    CPLDebug( "GPKG", "exec(%s)", osCommand.c_str() );
#endif
    OGRErr err = SQLCommand(m_poDS->GetDB(), osCommand.c_str());
    if ( OGRERR_NONE != err )
        return OGRERR_FAILURE;

    /* Update gpkg_contents with the table info */
    if ( bIsSpatial )
        err = RegisterGeometryColumn();
    else
        err = m_poDS->CreateGDALAspatialExtension();

    if ( err != OGRERR_NONE )
        return OGRERR_FAILURE;

    const char* pszIdentifier = GetMetadataItem("IDENTIFIER");
    if( pszIdentifier == NULL )
        pszIdentifier = pszLayerName;
    const char* pszDescription = GetMetadataItem("DESCRIPTION");
    if( pszDescription == NULL )
        pszDescription = "";
    pszSQL = sqlite3_mprintf(
        "INSERT INTO gpkg_contents "
        "(table_name,data_type,identifier,description,last_change,srs_id)"
        " VALUES "
        "('%q','%q','%q','%q',strftime('%%Y-%%m-%%dT%%H:%%M:%%fZ',CURRENT_TIMESTAMP),%d)",
        pszLayerName, (bIsSpatial ? "features": "aspatial"), pszIdentifier, pszDescription, m_iSrs);

    err = SQLCommand(m_poDS->GetDB(), pszSQL);
    sqlite3_free(pszSQL);
    if ( err != OGRERR_NONE )
        return OGRERR_FAILURE;

    ResetReading();

    return OGRERR_NONE;
}

/************************************************************************/
/*                            GetMetadata()                             */
/************************************************************************/

char **OGRGeoPackageTableLayer::GetMetadata( const char *pszDomain )

{
    if( m_bHasReadMetadataFromStorage )
        return OGRLayer::GetMetadata( pszDomain );

    m_bHasReadMetadataFromStorage = TRUE;

    if ( !m_poDS->HasMetadataTables() )
        return OGRLayer::GetMetadata( pszDomain );

    char* pszSQL;

    pszSQL = sqlite3_mprintf(
        "SELECT md.metadata, md.md_standard_uri, md.mime_type, mdr.reference_scope FROM gpkg_metadata md "
        "JOIN gpkg_metadata_reference mdr ON (md.id = mdr.md_file_id ) "
        "WHERE mdr.table_name = '%q' ORDER BY md.id",
        m_pszTableName);


    SQLResult oResult;
    OGRErr err = SQLQuery(m_poDS->GetDB(), pszSQL, &oResult);
    sqlite3_free(pszSQL);
    if  ( err != OGRERR_NONE )
    {
        SQLResultFree(&oResult);
        return OGRLayer::GetMetadata( pszDomain );
    }

    char** papszMetadata = CSLDuplicate(OGRLayer::GetMetadata());

    /* GDAL metadata */
    for(int i=0;i<oResult.nRowCount;i++)
    {
        const char *pszMetadata = SQLResultGetValue(&oResult, 0, i);
        const char* pszMDStandardURI = SQLResultGetValue(&oResult, 1, i);
        const char* pszMimeType = SQLResultGetValue(&oResult, 2, i);
        //const char* pszReferenceScope = SQLResultGetValue(&oResult, 3, i);
        //int bIsGPKGScope = EQUAL(pszReferenceScope, "geopackage");
        if( pszMetadata == NULL )
            continue;
        if( pszMDStandardURI != NULL && EQUAL(pszMDStandardURI, "http://gdal.org") &&
            pszMimeType != NULL && EQUAL(pszMimeType, "text/xml") )
        {
            CPLXMLNode* psXMLNode = CPLParseXMLString(pszMetadata);
            if( psXMLNode )
            {
                GDALMultiDomainMetadata oLocalMDMD;
                oLocalMDMD.XMLInit(psXMLNode, FALSE);

                papszMetadata = CSLMerge(papszMetadata, oLocalMDMD.GetMetadata());
                char** papszDomainList = oLocalMDMD.GetDomainList();
                char** papszIter = papszDomainList;
                while( papszIter && *papszIter )
                {
                    if( !EQUAL(*papszIter, "") )
                        oMDMD.SetMetadata(oLocalMDMD.GetMetadata(*papszIter), *papszIter);
                    papszIter ++;
                }

                CPLDestroyXMLNode(psXMLNode);
            }
        }
    }

    OGRLayer::SetMetadata(papszMetadata);
    CSLDestroy(papszMetadata);
    papszMetadata = NULL;

    /* Add non-GDAL metadata now */
    int nNonGDALMDILocal = 1;
    for(int i=0;i<oResult.nRowCount;i++)
    {
        const char *pszMetadata = SQLResultGetValue(&oResult, 0, i);
        const char* pszMDStandardURI = SQLResultGetValue(&oResult, 1, i);
        const char* pszMimeType = SQLResultGetValue(&oResult, 2, i);
        //const char* pszReferenceScope = SQLResultGetValue(&oResult, 3, i);
        //int bIsGPKGScope = EQUAL(pszReferenceScope, "geopackage");
        if( pszMetadata == NULL )
            continue;
        if( pszMDStandardURI != NULL && EQUAL(pszMDStandardURI, "http://gdal.org") &&
            pszMimeType != NULL && EQUAL(pszMimeType, "text/xml") )
            continue;

        /*if( strcmp( pszMDStandardURI, "http://www.isotc211.org/2005/gmd" ) == 0 &&
            strcmp( pszMimeType, "text/xml" ) == 0 )
        {
            char* apszMD[2];
            apszMD[0] = (char*)pszMetadata;
            apszMD[1] = NULL;
            oMDMD.SetMetadata(apszMD, "xml:MD_Metadata");
        }
        else*/
        {
            oMDMD.SetMetadataItem( CPLSPrintf("GPKG_METADATA_ITEM_%d", nNonGDALMDILocal),
                                    pszMetadata );
            nNonGDALMDILocal ++;
        }
    }

    SQLResultFree(&oResult);

    return OGRLayer::GetMetadata(pszDomain);
}

/************************************************************************/
/*                          GetMetadataItem()                           */
/************************************************************************/

const char *OGRGeoPackageTableLayer::GetMetadataItem( const char * pszName,
                                                    const char * pszDomain )
{
    return CSLFetchNameValue( GetMetadata(pszDomain), pszName );
}

/************************************************************************/
/*                      GetMetadataDomainList()                         */
/************************************************************************/

char **OGRGeoPackageTableLayer::GetMetadataDomainList()
{
    GetMetadata();
    return OGRLayer::GetMetadataDomainList();
}

/************************************************************************/
/*                            SetMetadata()                             */
/************************************************************************/

CPLErr OGRGeoPackageTableLayer::SetMetadata( char ** papszMetadata, const char * pszDomain )
{
    GetMetadata(); /* force loading from storage if needed */
    CPLErr eErr = OGRLayer::SetMetadata(papszMetadata, pszDomain);
    m_poDS->SetMetadataDirty();
    if( pszDomain == NULL || EQUAL(pszDomain, "") )
    {
        if( m_osIdentifierLCO.size() )
            OGRLayer::SetMetadataItem("IDENTIFIER", m_osIdentifierLCO);
        if( m_osDescriptionLCO.size() )
            OGRLayer::SetMetadataItem("DESCRIPTION", m_osDescriptionLCO);
    }
    return eErr;
}

/************************************************************************/
/*                          SetMetadataItem()                           */
/************************************************************************/

CPLErr OGRGeoPackageTableLayer::SetMetadataItem( const char * pszName,
                                                 const char * pszValue,
                                                 const char * pszDomain )
{
    GetMetadata(); /* force loading from storage if needed */
    if( m_osIdentifierLCO.size() && EQUAL(pszName, "IDENTIFIER") &&
        (pszDomain == NULL || EQUAL(pszDomain, "")) )
        return CE_None;
    if( m_osDescriptionLCO.size() && EQUAL(pszName, "DESCRIPTION") &&
        (pszDomain == NULL || EQUAL(pszDomain, "")) )
        return CE_None;
    m_poDS->SetMetadataDirty();
    return OGRLayer::SetMetadataItem(pszName, pszValue, pszDomain);
}
