/******************************************************************************
 * $Id$
 *
 * Project:  MSSQL Spatial driver
 * Purpose:  Implements OGRMSSQLSpatialTableLayer class, access to an existing table.
 * Author:   Tamas Szekeres, szekerest at gmail.com
 *
 ******************************************************************************
 * Copyright (c) 2010, Tamas Szekeres
 * Copyright (c) 2010-2012, Even Rouault <even dot rouault at mines-paris dot org>
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
#include "ogr_mssqlspatial.h"

CPL_CVSID("$Id$");

/************************************************************************/
/*                         OGRMSSQLAppendEscaped( )                     */
/************************************************************************/

void OGRMSSQLAppendEscaped( CPLODBCStatement* poStatement, const char* pszStrValue)
{
    if (!pszStrValue)
        poStatement->Append("null");
    
    size_t  iIn, iOut , nTextLen = strlen(pszStrValue);
    char    *pszEscapedText = (char *) VSIMalloc(nTextLen*2 + 3);

    pszEscapedText[0] = '\'';
    
    for( iIn = 0, iOut = 1; iIn < nTextLen; iIn++ )
    {
        switch( pszStrValue[iIn] )
        {
            case '\'':
                pszEscapedText[iOut++] = '\''; // double quote
                pszEscapedText[iOut++] = pszStrValue[iIn];
                break;

            default:
                pszEscapedText[iOut++] = pszStrValue[iIn];
                break;
        }
    }

    pszEscapedText[iOut++] = '\'';

    pszEscapedText[iOut] = '\0';

    poStatement->Append(pszEscapedText);

    CPLFree( pszEscapedText );
}

/************************************************************************/
/*                          OGRMSSQLSpatialTableLayer()                 */
/************************************************************************/

OGRMSSQLSpatialTableLayer::OGRMSSQLSpatialTableLayer( OGRMSSQLSpatialDataSource *poDSIn )

{
    poDS = poDSIn;

    pszQuery = NULL;

    bUpdateAccess = TRUE;

    iNextShapeId = 0;

    nSRSId = -1;

    poFeatureDefn = NULL;
    
    pszTableName = NULL;
    pszLayerName = NULL;
    pszSchemaName = NULL;
}

/************************************************************************/
/*                          ~OGRMSSQLSpatialTableLayer()                */
/************************************************************************/

OGRMSSQLSpatialTableLayer::~OGRMSSQLSpatialTableLayer()

{
    CPLFree( pszTableName );
    CPLFree( pszLayerName );
    CPLFree( pszSchemaName );

    CPLFree( pszQuery );
    ClearStatement();
}

/************************************************************************/
/*                               GetName()                              */
/************************************************************************/

const char *OGRMSSQLSpatialTableLayer::GetName()

{
    return pszLayerName;
}

/************************************************************************/
/*                             GetLayerDefn()                           */
/************************************************************************/
OGRFeatureDefn* OGRMSSQLSpatialTableLayer::GetLayerDefn() 
{ 
    if (poFeatureDefn)
        return poFeatureDefn;
    
    CPLODBCSession *poSession = poDS->GetSession();
/* -------------------------------------------------------------------- */
/*      Do we have a simple primary key?                                */
/* -------------------------------------------------------------------- */
    CPLODBCStatement oGetKey( poSession );
    
    if( oGetKey.GetPrimaryKeys( pszTableName, poDS->GetCatalog(), pszSchemaName ) 
        && oGetKey.Fetch() )
    {
        pszFIDColumn = CPLStrdup(oGetKey.GetColData( 3 ));
        
        if( oGetKey.Fetch() ) // more than one field in key! 
        {
            oGetKey.Clear();
            CPLFree( pszFIDColumn );
            pszFIDColumn = NULL;

            CPLDebug( "OGR_MSSQLSpatial", "Table %s has multiple primary key fields, "
                      "ignoring them all.", pszTableName );
        }
    }

    
/* -------------------------------------------------------------------- */
/*      Get the column definitions for this table.                      */
/* -------------------------------------------------------------------- */
    CPLODBCStatement oGetCol( poSession );
    CPLErr eErr;

    if( !oGetCol.GetColumns( pszTableName, poDS->GetCatalog(), pszSchemaName ) )
        return NULL;

    eErr = BuildFeatureDefn( pszLayerName, &oGetCol );
    if( eErr != CE_None )
        return NULL;
        
    poFeatureDefn->SetGeomType(eGeomType);

    if( poFeatureDefn->GetFieldCount() == 0 &&
        pszFIDColumn == NULL && pszGeomColumn == NULL )
    {
        CPLError( CE_Failure, CPLE_AppDefined, 
                  "No column definitions found for table '%s', layer not usable.", 
                  pszLayerName );
        return NULL;
    }
        
/* -------------------------------------------------------------------- */
/*      If we got a geometry column, does it exist?  Is it binary?      */
/* -------------------------------------------------------------------- */
    if( pszGeomColumn != NULL )
    {
        int iColumn = oGetCol.GetColId( pszGeomColumn );
        if( iColumn < 0 )
        {
            CPLError( CE_Failure, CPLE_AppDefined, 
                      "Column %s requested for geometry, but it does not exist.", 
                      pszGeomColumn );
            CPLFree( pszGeomColumn );
            pszGeomColumn = NULL;
        }
        else
        {
            if ( nGeomColumnType < 0 )
            {
                /* last attempt to identify the geometry column type */
                if ( EQUAL(oGetCol.GetColTypeName( iColumn ), "geometry") )
                    nGeomColumnType = MSSQLCOLTYPE_GEOMETRY;
                else if ( EQUAL(oGetCol.GetColTypeName( iColumn ), "geography") )
                    nGeomColumnType = MSSQLCOLTYPE_GEOGRAPHY;
                else if ( EQUAL(oGetCol.GetColTypeName( iColumn ), "varchar") )
                    nGeomColumnType = MSSQLCOLTYPE_TEXT;
                else if ( EQUAL(oGetCol.GetColTypeName( iColumn ), "nvarchar") )
                    nGeomColumnType = MSSQLCOLTYPE_TEXT;
                else if ( EQUAL(oGetCol.GetColTypeName( iColumn ), "text") )
                    nGeomColumnType = MSSQLCOLTYPE_TEXT;
                else if ( EQUAL(oGetCol.GetColTypeName( iColumn ), "ntext") )
                    nGeomColumnType = MSSQLCOLTYPE_TEXT;
                else if ( EQUAL(oGetCol.GetColTypeName( iColumn ), "image") )
                    nGeomColumnType = MSSQLCOLTYPE_BINARY; 
                else
                {
                    CPLError( CE_Failure, CPLE_AppDefined, 
                          "Column type %s is not supported for geometry column.", 
                          oGetCol.GetColTypeName( iColumn ) );
                    CPLFree( pszGeomColumn );
                    pszGeomColumn = NULL;
                }
            }
        }
    }

    return poFeatureDefn; 
}

/************************************************************************/
/*                             Initialize()                             */
/************************************************************************/

CPLErr OGRMSSQLSpatialTableLayer::Initialize( const char *pszSchema,
                                              const char *pszLayerName, 
                                              const char *pszGeomCol,
                                              int nCoordDimension, 
                                              int nSRId,
                                              const char *pszSRText,
                                              OGRwkbGeometryType eType )

{
    CPLFree( pszFIDColumn );
    pszFIDColumn = NULL;

/* -------------------------------------------------------------------- */
/*      Parse out schema name if present in layer.  We assume a         */
/*      schema is provided if there is a dot in the name, and that      */
/*      it is in the form <schema>.<tablename>                          */
/* -------------------------------------------------------------------- */
    const char *pszDot = strstr(pszLayerName,".");
    if( pszDot != NULL )
    {
        pszTableName = CPLStrdup(pszDot + 1);
        pszSchemaName = CPLStrdup(pszLayerName);
        pszSchemaName[pszDot - pszLayerName] = '\0';
        this->pszLayerName = CPLStrdup(pszLayerName);
    }
    else
    {
        pszTableName = CPLStrdup(pszLayerName);
        pszSchemaName = CPLStrdup(pszSchema);
        if ( EQUAL(pszSchemaName, "dbo") )
            this->pszLayerName = CPLStrdup(pszLayerName);
        else
            this->pszLayerName = CPLStrdup(CPLSPrintf("%s.%s", pszSchemaName, pszTableName));
    }

/* -------------------------------------------------------------------- */
/*      Have we been provided a geometry column?                        */
/* -------------------------------------------------------------------- */
    CPLFree( pszGeomColumn );
    if( pszGeomCol == NULL )
        pszGeomColumn = NULL;
    else
        pszGeomColumn = CPLStrdup( pszGeomCol );

    eGeomType = eType;


/* -------------------------------------------------------------------- */
/*             Try to find out the spatial reference                    */
/* -------------------------------------------------------------------- */

    nSRSId = nSRId;

    if (pszSRText)
    {
        /* Process srtext directly if specified */
        poSRS = new OGRSpatialReference();
        if( poSRS->importFromWkt( (char**)&pszSRText ) != OGRERR_NONE )
        {
            delete poSRS;
            poSRS = NULL;
        }
    }
    
    if (!poSRS)
    {
        if (nSRSId < 0)
            nSRSId = FetchSRSId();

        GetSpatialRef();
    }

    return CE_None;
}

/************************************************************************/
/*                         FetchSRSId()                                 */
/************************************************************************/

int OGRMSSQLSpatialTableLayer::FetchSRSId()
{
    if ( poDS->UseGeometryColumns() )
    {
        CPLODBCStatement oStatement = CPLODBCStatement( poDS->GetSession() );
        oStatement.Appendf( "select srid from geometry_columns "
                        "where f_table_schema = '%s' and f_table_name = '%s'", 
                        pszSchemaName, pszTableName );

        if( oStatement.ExecuteSQL() && oStatement.Fetch() )
        {
            if ( oStatement.GetColData( 0 ) )
                nSRSId = atoi( oStatement.GetColData( 0 ) );
        }
    }
        
    return nSRSId;
}

/************************************************************************/
/*                       CreateSpatialIndex()                           */
/*                                                                      */
/*      Create a spatial index on the geometry column of the layer      */
/************************************************************************/

OGRErr OGRMSSQLSpatialTableLayer::CreateSpatialIndex()
{
    GetLayerDefn();

    CPLODBCStatement oStatement( poDS->GetSession() );

    if (nGeomColumnType == MSSQLCOLTYPE_GEOMETRY)
    {
        OGREnvelope oExt;
        if (GetExtent(&oExt, TRUE) != OGRERR_NONE)
        {
            CPLError( CE_Failure, CPLE_AppDefined, 
                          "Failed to get extent for spatial index." );
            return OGRERR_FAILURE;
        }

        oStatement.Appendf("CREATE SPATIAL INDEX [ogr_%s_sidx] ON [%s].[%s] ( [%s] ) "
            "USING GEOMETRY_GRID WITH (BOUNDING_BOX =(%.15g, %.15g, %.15g, %.15g))",
                           pszGeomColumn, pszSchemaName, pszTableName, pszGeomColumn, 
                           oExt.MinX, oExt.MinY, oExt.MaxX, oExt.MaxY );
    }
    else if (nGeomColumnType == MSSQLCOLTYPE_GEOGRAPHY)
    {
        oStatement.Appendf("CREATE SPATIAL INDEX [ogr_%s_sidx] ON [%s].[%s] ( [%s] ) "
            "USING GEOGRAPHY_GRID",
                           pszGeomColumn, pszSchemaName, pszTableName, pszGeomColumn );
    }
    else
    {
        CPLError( CE_Failure, CPLE_AppDefined, 
            "Spatial index is not supported on the geometry column '%s'", pszGeomColumn);
        return OGRERR_FAILURE;
    }

    //poDS->GetSession()->BeginTransaction();
    
    if( !oStatement.ExecuteSQL() )
    {
        CPLError( CE_Failure, CPLE_AppDefined, 
                      "Failed to create the spatial index, %s.", 
                      poDS->GetSession()->GetLastError());
        return OGRERR_FAILURE;
    } 

    //poDS->GetSession()->CommitTransaction();

    return OGRERR_NONE;
}

/************************************************************************/
/*                       DropSpatialIndex()                             */
/*                                                                      */
/*      Drop the spatial index on the geometry column of the layer      */
/************************************************************************/

void OGRMSSQLSpatialTableLayer::DropSpatialIndex()
{
    GetLayerDefn();

    CPLODBCStatement oStatement( poDS->GetSession() );

    oStatement.Appendf("IF  EXISTS (SELECT * FROM sys.indexes "
        "WHERE object_id = OBJECT_ID(N'[%s].[%s]') AND name = N'ogr_%s_sidx') "
        "DROP INDEX [ogr_%s_sidx] ON [%s].[%s]",
                       pszSchemaName, pszTableName, pszGeomColumn, 
                       pszGeomColumn, pszSchemaName, pszTableName );
    
    //poDS->GetSession()->BeginTransaction();

    if( !oStatement.ExecuteSQL() )
    {
        CPLError( CE_Failure, CPLE_AppDefined, 
                      "Failed to drop the spatial index, %s.", 
                      poDS->GetSession()->GetLastError());
        return;
    } 

    //poDS->GetSession()->CommitTransaction();
}

/************************************************************************/
/*                            BuildFields()                             */
/*                                                                      */
/*      Build list of fields to fetch, performing any required          */
/*      transformations (such as on geometry).                          */
/************************************************************************/

CPLString OGRMSSQLSpatialTableLayer::BuildFields()

{
    int i = 0;
    int nColumn = 0;
    CPLString osFieldList;

    if( pszFIDColumn && poFeatureDefn->GetFieldIndex( pszFIDColumn ) == -1 )
    {
        /* Always get the FID column */
        osFieldList += "[";
        osFieldList += pszFIDColumn;
        osFieldList += "]";
        ++nColumn;
    }

    if( pszGeomColumn && !poFeatureDefn->IsGeometryIgnored())
    {
        if( nColumn > 0 )
            osFieldList += ", ";

        osFieldList += "[";
        osFieldList += pszGeomColumn;
        if (nGeomColumnType == MSSQLCOLTYPE_GEOMETRY || 
                            nGeomColumnType == MSSQLCOLTYPE_GEOGRAPHY)
        {
            if ( poDS->GetGeometryFormat() == MSSQLGEOMETRY_WKB )
            {
                osFieldList += "].STAsBinary() as [";
                osFieldList += pszGeomColumn;
            }
            else if ( poDS->GetGeometryFormat() == MSSQLGEOMETRY_WKT )
            {
                osFieldList += "].AsTextZM() as [";
                osFieldList += pszGeomColumn;
            }
            else if ( poDS->GetGeometryFormat() == MSSQLGEOMETRY_WKBZM )
            {
                /* SQL Server 2012 */
                osFieldList += "].AsBinaryZM() as [";
                osFieldList += pszGeomColumn;
            }
        }
        osFieldList += "]";

        ++nColumn;
    }

    if (poFeatureDefn->GetFieldCount() > 0)
    {
        /* need to reconstruct the field ordinals list */
        CPLFree(panFieldOrdinals);
        panFieldOrdinals = (int *) CPLMalloc( sizeof(int) * poFeatureDefn->GetFieldCount() );

        for( i = 0; i < poFeatureDefn->GetFieldCount(); i++ )
        {
            if ( poFeatureDefn->GetFieldDefn(i)->IsIgnored() )
                continue;
            
            const char *pszName = poFeatureDefn->GetFieldDefn(i)->GetNameRef();

            if( nColumn > 0 )
                osFieldList += ", ";
        
            osFieldList += "[";
            osFieldList += pszName;
            osFieldList += "]";

            panFieldOrdinals[i] = nColumn;

            ++nColumn;
        }
    }

    return osFieldList;
}

/************************************************************************/
/*                           ClearStatement()                           */
/************************************************************************/

void OGRMSSQLSpatialTableLayer::ClearStatement()

{
    if( poStmt != NULL )
    {
        delete poStmt;
        poStmt = NULL;
    }
}

/************************************************************************/
/*                            GetStatement()                            */
/************************************************************************/

CPLODBCStatement *OGRMSSQLSpatialTableLayer::GetStatement()

{
    if( poStmt == NULL )
    {
        poStmt = BuildStatement(BuildFields());
        iNextShapeId = 0;
    }

    return poStmt;
}


/************************************************************************/
/*                           BuildStatement()                           */
/************************************************************************/

CPLODBCStatement* OGRMSSQLSpatialTableLayer::BuildStatement(const char* pszColumns)

{
    CPLODBCStatement* poStatement = new CPLODBCStatement( poDS->GetSession() );
    poStatement->Append( "select " );
    poStatement->Append( pszColumns );
    poStatement->Append( " from " );
    poStatement->Append( pszSchemaName );
    poStatement->Append( "." );
    poStatement->Append( pszTableName );

    /* Append attribute query if we have it */
    if( pszQuery != NULL )
        poStatement->Appendf( " where (%s)", pszQuery );

    /* If we have a spatial filter, query on it */
    if ( m_poFilterGeom != NULL )
    {
        if (nGeomColumnType == MSSQLCOLTYPE_GEOMETRY 
            || nGeomColumnType == MSSQLCOLTYPE_GEOGRAPHY)
        {
            if( pszQuery == NULL )
                poStatement->Append( " where" );
            else
                poStatement->Append( " and" );

            poStatement->Appendf(" [%s].STIntersects(", pszGeomColumn );

            if (nGeomColumnType == MSSQLCOLTYPE_GEOGRAPHY)
                poStatement->Append( "geography::" );
            else
                poStatement->Append( "geometry::" );
            
            if ( m_sFilterEnvelope.MinX == m_sFilterEnvelope.MaxX || 
                 m_sFilterEnvelope.MinY == m_sFilterEnvelope.MaxY)
                poStatement->Appendf("STGeomFromText('POINT(%.15g %.15g)',%d)) = 1", 
                            m_sFilterEnvelope.MinX, m_sFilterEnvelope.MinY, nSRSId >= 0? nSRSId : 0);
            else
                poStatement->Appendf( "STGeomFromText('POLYGON((%.15g %.15g,%.15g %.15g,%.15g %.15g,%.15g %.15g,%.15g %.15g))',%d)) = 1", 
                                            m_sFilterEnvelope.MinX, m_sFilterEnvelope.MinY,
                                            m_sFilterEnvelope.MaxX, m_sFilterEnvelope.MinY, 
                                            m_sFilterEnvelope.MaxX, m_sFilterEnvelope.MaxY,
                                            m_sFilterEnvelope.MinX, m_sFilterEnvelope.MaxY,
                                            m_sFilterEnvelope.MinX, m_sFilterEnvelope.MinY,
                                            nSRSId >= 0? nSRSId : 0 );
        }
        else
        {
            CPLError( CE_Failure, CPLE_AppDefined, 
                      "Spatial filter is supported only on geometry and geography column types." );
            
            delete poStatement;
            return NULL;
        }
    }

    CPLDebug( "OGR_MSSQLSpatial", "ExecuteSQL(%s)", poStatement->GetCommand() );
    if( poStatement->ExecuteSQL() )
        return poStatement;
    else
    {
        delete poStatement;
        return NULL;
    }
}

/************************************************************************/
/*                            ResetReading()                            */
/************************************************************************/

void OGRMSSQLSpatialTableLayer::ResetReading()

{
    ClearStatement();
    OGRMSSQLSpatialLayer::ResetReading();
}

/************************************************************************/
/*                             GetFeature()                             */
/************************************************************************/

OGRFeature *OGRMSSQLSpatialTableLayer::GetFeature( long nFeatureId )

{
    if( pszFIDColumn == NULL )
        return OGRMSSQLSpatialLayer::GetFeature( nFeatureId );

    ClearStatement();

    iNextShapeId = nFeatureId;

    poStmt = new CPLODBCStatement( poDS->GetSession() );
    CPLString osFields = BuildFields();
    poStmt->Appendf( "select %s from %s where %s = %ld", osFields.c_str(), 
        poFeatureDefn->GetName(), pszFIDColumn, nFeatureId );

    if( !poStmt->ExecuteSQL() )
    {
        delete poStmt;
        poStmt = NULL;
        return NULL;
    }

    return GetNextRawFeature();
}

/************************************************************************/
/*                         SetAttributeFilter()                         */
/************************************************************************/

OGRErr OGRMSSQLSpatialTableLayer::SetAttributeFilter( const char *pszQuery )

{
    CPLFree(m_pszAttrQueryString);
    m_pszAttrQueryString = (pszQuery) ? CPLStrdup(pszQuery) : NULL;

    if( (pszQuery == NULL && this->pszQuery == NULL)
        || (pszQuery != NULL && this->pszQuery != NULL 
            && EQUAL(pszQuery,this->pszQuery)) )
        return OGRERR_NONE;

    CPLFree( this->pszQuery );
    this->pszQuery = (pszQuery) ? CPLStrdup( pszQuery ) : NULL;

    ClearStatement();

    return OGRERR_NONE;
}


/************************************************************************/
/*                           TestCapability()                           */
/************************************************************************/

int OGRMSSQLSpatialTableLayer::TestCapability( const char * pszCap )

{
    if ( bUpdateAccess )
    {
        if( EQUAL(pszCap,OLCSequentialWrite) || EQUAL(pszCap,OLCCreateField)
            || EQUAL(pszCap,OLCDeleteFeature) )
            return TRUE;

        else if( EQUAL(pszCap,OLCRandomWrite) )
            return (pszFIDColumn != NULL);
    }

#if (ODBCVER >= 0x0300)
    if( EQUAL(pszCap,OLCTransactions) )
        return TRUE;
#else
    if( EQUAL(pszCap,OLCTransactions) )
        return FALSE;
#endif

    if( EQUAL(pszCap,OLCIgnoreFields) )
        return TRUE; 
    
    if( EQUAL(pszCap,OLCRandomRead) )
        return (pszFIDColumn != NULL);
    else if( EQUAL(pszCap,OLCFastFeatureCount) )
        return TRUE;
    else 
        return OGRMSSQLSpatialLayer::TestCapability( pszCap );
}

/************************************************************************/
/*                          GetFeatureCount()                           */
/************************************************************************/

int OGRMSSQLSpatialTableLayer::GetFeatureCount( int bForce )

{
    GetLayerDefn();

    if( TestCapability(OLCFastFeatureCount) == FALSE )
        return OGRMSSQLSpatialLayer::GetFeatureCount( bForce );

    ClearStatement();
        
    CPLODBCStatement* poStatement = BuildStatement( "count(*)" );

    if (poStatement == NULL || !poStatement->Fetch())
    {
        delete poStatement;
        return OGRMSSQLSpatialLayer::GetFeatureCount( bForce );
    }

    int nRet = atoi(poStatement->GetColData( 0 ));
    delete poStatement;
    return nRet;
}


/************************************************************************/
/*                            CreateField()                             */
/************************************************************************/

OGRErr OGRMSSQLSpatialTableLayer::CreateField( OGRFieldDefn *poFieldIn, 
                                         int bApproxOK )

{
    char                szFieldType[256];
    OGRFieldDefn        oField( poFieldIn );

    GetLayerDefn();

/* -------------------------------------------------------------------- */
/*      Do we want to "launder" the column names into MSSQL             */
/*      friendly format?                                                */
/* -------------------------------------------------------------------- */
    if( bLaunderColumnNames )
    {
        char    *pszSafeName = poDS->LaunderName( oField.GetNameRef() );

        oField.SetName( pszSafeName );
        CPLFree( pszSafeName );
    }

/* -------------------------------------------------------------------- */
/*      Identify the MSSQL type.                                        */
/* -------------------------------------------------------------------- */
    
    if( oField.GetType() == OFTInteger )
    {
        if( oField.GetWidth() > 0 && bPreservePrecision )
            sprintf( szFieldType, "numeric(%d,0)", oField.GetWidth() );
        else
            strcpy( szFieldType, "int" );
    }
    else if( oField.GetType() == OFTReal )
    {
        if( oField.GetWidth() > 0 && oField.GetPrecision() > 0
            && bPreservePrecision )
            sprintf( szFieldType, "numeric(%d,%d)",
                     oField.GetWidth(), oField.GetPrecision() );
        else
            strcpy( szFieldType, "float" );
    }
    else if( oField.GetType() == OFTString )
    {
        if( oField.GetWidth() == 0 || !bPreservePrecision )
            strcpy( szFieldType, "varchar(MAX)" );
        else
            sprintf( szFieldType, "varchar(%d)", oField.GetWidth() );
    }
    else if( oField.GetType() == OFTDate )
    {
        strcpy( szFieldType, "date" );
    }
    else if( oField.GetType() == OFTTime )
    {
        strcpy( szFieldType, "time(7)" );
    }
    else if( oField.GetType() == OFTDateTime )
    {
        strcpy( szFieldType, "datetime" );
    }
    else if( oField.GetType() == OFTBinary )
    {
        strcpy( szFieldType, "image" );
    }
    else if( bApproxOK )
    {
        CPLError( CE_Warning, CPLE_NotSupported,
                  "Can't create field %s with type %s on MSSQL layers.  Creating as varchar.",
                  oField.GetNameRef(),
                  OGRFieldDefn::GetFieldTypeName(oField.GetType()) );
        strcpy( szFieldType, "varchar" );
    }
    else
    {
        CPLError( CE_Failure, CPLE_NotSupported,
                  "Can't create field %s with type %s on MSSQL layers.",
                  oField.GetNameRef(),
                  OGRFieldDefn::GetFieldTypeName(oField.GetType()) );

        return OGRERR_FAILURE;
    }

/* -------------------------------------------------------------------- */
/*      Create the new field.                                           */
/* -------------------------------------------------------------------- */
    
    CPLODBCStatement oStmt( poDS->GetSession() );
            
    oStmt.Appendf( "ALTER TABLE [%s].[%s] ADD [%s] %s", 
        pszSchemaName, pszTableName, oField.GetNameRef(), szFieldType);

    if( !oStmt.ExecuteSQL() )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                    "Error creating field %s, %s", oField.GetNameRef(), 
                    poDS->GetSession()->GetLastError() );

        return OGRERR_FAILURE;
    }

/* -------------------------------------------------------------------- */
/*      Add the field to the OGRFeatureDefn.                            */
/* -------------------------------------------------------------------- */

    poFeatureDefn->AddFieldDefn( &oField );

    return OGRERR_NONE;
}

/************************************************************************/
/*                             SetFeature()                             */
/*                                                                      */
/*      SetFeature() is implemented by an UPDATE SQL command            */
/************************************************************************/

OGRErr OGRMSSQLSpatialTableLayer::SetFeature( OGRFeature *poFeature )

{
    OGRErr              eErr = OGRERR_FAILURE;

    GetLayerDefn();

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

    if( !pszFIDColumn )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "Unable to update features in tables without\n"
                  "a recognised FID column.");
        return eErr;

    }
    
    ClearStatement();

/* -------------------------------------------------------------------- */
/*      Form the UPDATE command.                                        */
/* -------------------------------------------------------------------- */
    CPLODBCStatement oStmt( poDS->GetSession() );
            
    oStmt.Appendf( "UPDATE [%s].[%s] SET ", pszSchemaName, pszTableName);

    OGRMSSQLGeometryValidator oValidator(poFeature->GetGeometryRef());
    OGRGeometry *poGeom = oValidator.GetValidGeometryRef();

    if (poFeature->GetGeometryRef() != poGeom)
    {
        CPLError( CE_Warning, CPLE_NotSupported,
                  "Geometry with FID = %ld has been modified.", poFeature->GetFID() );
    }

    int bNeedComma = FALSE;
    if(pszGeomColumn != NULL)
    {
        char    *pszWKT = NULL;

        if (poGeom != NULL)
            poGeom->exportToWkt( &pszWKT );

        oStmt.Appendf( "[%s] = ", pszGeomColumn );

        if( pszWKT != NULL && (nGeomColumnType == MSSQLCOLTYPE_GEOMETRY 
            || nGeomColumnType == MSSQLCOLTYPE_GEOGRAPHY))
        {
            if (nGeomColumnType == MSSQLCOLTYPE_GEOGRAPHY)
            {
                oStmt.Append( "geography::STGeomFromText(" );
                OGRMSSQLAppendEscaped(&oStmt, pszWKT);
                oStmt.Appendf(",%d)", nSRSId );
            }
            else
            {
                oStmt.Append( "geometry::STGeomFromText(" );
                OGRMSSQLAppendEscaped(&oStmt, pszWKT);
                oStmt.Appendf(",%d).MakeValid()", nSRSId );
            }
        }
        else
            oStmt.Append( "null" );

        bNeedComma = TRUE;
        CPLFree(pszWKT);
    }

    int nFieldCount = poFeatureDefn->GetFieldCount();
    int i;
    for( i = 0; i < nFieldCount; i++ )
    {
        if (bNeedComma)
            oStmt.Appendf( ", [%s] = ", poFeatureDefn->GetFieldDefn(i)->GetNameRef() );
        else
        {
            oStmt.Appendf( "[%s] = ", poFeatureDefn->GetFieldDefn(i)->GetNameRef() );
            bNeedComma = TRUE;
        }

        if( !poFeature->IsFieldSet( i ) )
            oStmt.Append( "null" );
        else
            AppendFieldValue(&oStmt, poFeature, i);
    }

    /* Add the WHERE clause */
    oStmt.Appendf( " WHERE [%s] = %ld" , pszFIDColumn, poFeature->GetFID());

/* -------------------------------------------------------------------- */
/*      Execute the update.                                             */
/* -------------------------------------------------------------------- */

    if( !oStmt.ExecuteSQL() )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
            "Error updating feature with FID:%ld, %s", poFeature->GetFID(), 
                    poDS->GetSession()->GetLastError() );

        return OGRERR_FAILURE;
    }
    
    return OGRERR_NONE;
}

/************************************************************************/
/*                          DeleteFeature()                             */
/************************************************************************/

OGRErr OGRMSSQLSpatialTableLayer::DeleteFeature( long nFID )

{
    GetLayerDefn();
    
    if( pszFIDColumn == NULL )
    {
        CPLError( CE_Failure, CPLE_AppDefined, 
                  "DeleteFeature() without any FID column." );
        return OGRERR_FAILURE;
    }
    
    if( nFID == OGRNullFID )
    {
        CPLError( CE_Failure, CPLE_AppDefined, 
                  "DeleteFeature() with unset FID fails." );
        return OGRERR_FAILURE;
    }
    
    ClearStatement();

/* -------------------------------------------------------------------- */
/*      Drop the record with this FID.                                  */
/* -------------------------------------------------------------------- */
    CPLODBCStatement oStatement( poDS->GetSession() );

    oStatement.Appendf("DELETE FROM [%s] WHERE [%s] = %ld", 
            poFeatureDefn->GetName(), pszFIDColumn, nFID);
    
    if( !oStatement.ExecuteSQL() )
    {
        CPLError( CE_Failure, CPLE_AppDefined, 
                  "Attempt to delete feature with FID %ld failed. %s", 
                  nFID, poDS->GetSession()->GetLastError() );

        return OGRERR_FAILURE;
    }
    
    return OGRERR_NONE;
}

/************************************************************************/
/*                           CreateFeature()                            */
/************************************************************************/

OGRErr OGRMSSQLSpatialTableLayer::CreateFeature( OGRFeature *poFeature )

{
    GetLayerDefn();

    if( NULL == poFeature )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "NULL pointer to OGRFeature passed to CreateFeature()." );
        return OGRERR_FAILURE;
    }
    
    ClearStatement();

    CPLODBCStatement oStatement( poDS->GetSession() );

    /* the fid values are retieved from the source layer */
    if( poFeature->GetFID() != OGRNullFID && pszFIDColumn != NULL && bIsIdentityFid )
        oStatement.Appendf("SET IDENTITY_INSERT [%s].[%s] ON;", pszSchemaName, pszTableName );

/* -------------------------------------------------------------------- */
/*      Form the INSERT command.                                        */
/* -------------------------------------------------------------------- */

    oStatement.Appendf( "INSERT INTO [%s].[%s] (", pszSchemaName, pszTableName );

    OGRMSSQLGeometryValidator oValidator(poFeature->GetGeometryRef());
    OGRGeometry *poGeom = oValidator.GetValidGeometryRef();

    if (poFeature->GetGeometryRef() != poGeom)
    {
        CPLError( CE_Warning, CPLE_NotSupported,
                  "Geometry with FID = %ld has been modified.", poFeature->GetFID() );
    }

    int bNeedComma = FALSE;

    if (poGeom != NULL && pszGeomColumn != NULL)
    {
        oStatement.Append( pszGeomColumn );
        bNeedComma = TRUE;
    }

    if( poFeature->GetFID() != OGRNullFID && pszFIDColumn != NULL )
    {
        if (bNeedComma)
            oStatement.Appendf( ", [%s]", pszFIDColumn );
        else
        {
            oStatement.Appendf( "[%s]", pszFIDColumn );
            bNeedComma = TRUE;
        }
    }

    int nFieldCount = poFeatureDefn->GetFieldCount();
    int i;
    for( i = 0; i < nFieldCount; i++ )
    {
        if( !poFeature->IsFieldSet( i ) )
            continue;

        if (bNeedComma)
            oStatement.Appendf( ", [%s]", poFeatureDefn->GetFieldDefn(i)->GetNameRef() );
        else
        {
            oStatement.Appendf( "[%s]", poFeatureDefn->GetFieldDefn(i)->GetNameRef() );
            bNeedComma = TRUE;
        }
    }

    oStatement.Appendf( ") VALUES (" );

    /* Set the geometry */
    bNeedComma = FALSE;
    if(poGeom != NULL && pszGeomColumn != NULL)
    {
        char    *pszWKT = NULL;
    
        //poGeom->setCoordinateDimension( nCoordDimension );

        poGeom->exportToWkt( &pszWKT );

        if( pszWKT != NULL && (nGeomColumnType == MSSQLCOLTYPE_GEOMETRY 
            || nGeomColumnType == MSSQLCOLTYPE_GEOGRAPHY))
        {
            if (nGeomColumnType == MSSQLCOLTYPE_GEOGRAPHY)
            {
                oStatement.Append( "geography::STGeomFromText(" );
                OGRMSSQLAppendEscaped(&oStatement, pszWKT);
                oStatement.Appendf(",%d)", nSRSId );
            }
            else
            {
                oStatement.Append( "geometry::STGeomFromText(" );
                OGRMSSQLAppendEscaped(&oStatement, pszWKT);
                oStatement.Appendf(",%d).MakeValid()", nSRSId );
            }     
        }
        else
            oStatement.Append( "null" );

        bNeedComma = TRUE;
        CPLFree(pszWKT);
    }

    /* Set the FID */
    if( poFeature->GetFID() != OGRNullFID && pszFIDColumn != NULL )
    {
        if (bNeedComma)
            oStatement.Appendf( ", %ld", poFeature->GetFID() );
        else
        {
            oStatement.Appendf( "%ld", poFeature->GetFID() );
            bNeedComma = TRUE;
        }
    }

    for( i = 0; i < nFieldCount; i++ )
    {
        if( !poFeature->IsFieldSet( i ) )
            continue;

        if (bNeedComma)
            oStatement.Append( ", " );
        else
            bNeedComma = TRUE;

        AppendFieldValue(&oStatement, poFeature, i);
    }

    oStatement.Append( ");" );

    if( poFeature->GetFID() != OGRNullFID && pszFIDColumn != NULL && bIsIdentityFid )
        oStatement.Appendf("SET IDENTITY_INSERT [%s].[%s] OFF;", pszSchemaName, pszTableName );

/* -------------------------------------------------------------------- */
/*      Execute the insert.                                             */
/* -------------------------------------------------------------------- */
    
    if( !oStatement.ExecuteSQL() )
    {
        CPLError( CE_Failure, CPLE_AppDefined, 
                  "INSERT command for new feature failed. %s", 
                   poDS->GetSession()->GetLastError() );

        return OGRERR_FAILURE;
    }

    return OGRERR_NONE;
}

/************************************************************************/
/*                          AppendFieldValue()                          */
/*                                                                      */
/* Used by CreateFeature() and SetFeature() to format a                 */
/* non-empty field value                                                */
/************************************************************************/

void OGRMSSQLSpatialTableLayer::AppendFieldValue(CPLODBCStatement *poStatement,
                                       OGRFeature* poFeature, int i)
{
    int nOGRFieldType = poFeatureDefn->GetFieldDefn(i)->GetType();

    // We need special formatting for integer list values.
    if(  nOGRFieldType == OFTIntegerList )
    {
        //TODO
        poStatement->Append( "null" );
        return;
    }

    // We need special formatting for real list values.
    else if( nOGRFieldType == OFTRealList )
    {
        //TODO
        poStatement->Append( "null" );
        return;
    }

    // We need special formatting for string list values.
    else if( nOGRFieldType == OFTStringList )
    {
        //TODO
        poStatement->Append( "null" );
        return;
    }

    // Binary formatting
    if( nOGRFieldType == OFTBinary )
    {
        int nLen = 0;
        GByte* pabyData = poFeature->GetFieldAsBinary( i, &nLen );
        char* pszBytes = GByteArrayToHexString( pabyData, nLen);
        poStatement->Append( pszBytes );
        CPLFree(pszBytes);
        return;
    }

    // Flag indicating NULL or not-a-date date value
    // e.g. 0000-00-00 - there is no year 0
    OGRBoolean bIsDateNull = FALSE;

    const char *pszStrValue = poFeature->GetFieldAsString(i);

    // Check if date is NULL: 0000-00-00
    if( nOGRFieldType == OFTDate )
    {
        if( EQUALN( pszStrValue, "0000", 4 ) )
        {
            pszStrValue = "null";
            bIsDateNull = TRUE;
        }
    }
    else if ( nOGRFieldType == OFTReal )
    {
        char* pszComma = strchr((char*)pszStrValue, ',');
        if (pszComma)
            *pszComma = '.';
    }

    if( nOGRFieldType != OFTInteger && nOGRFieldType != OFTReal
        && !bIsDateNull )
    {
        OGRMSSQLAppendEscaped(poStatement, pszStrValue);
    }
    else
    {
        poStatement->Append( pszStrValue );
    }
}

