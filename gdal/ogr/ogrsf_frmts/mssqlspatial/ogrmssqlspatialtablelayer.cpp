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
    {
        poStatement->Append("null");
        return;
    }

    size_t  iIn, iOut , nTextLen = strlen(pszStrValue);
    char    *pszEscapedText = (char *) CPLMalloc(nTextLen*2 + 3);

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

OGRMSSQLSpatialTableLayer::OGRMSSQLSpatialTableLayer( OGRMSSQLSpatialDataSource *poDSIn ) :
    bLaunderColumnNames(FALSE),
    bPreservePrecision(FALSE)
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

    eGeomType = wkbNone;

    bNeedSpatialIndex = FALSE;
    nUploadGeometryFormat = MSSQLGEOMETRY_WKB;
}

/************************************************************************/
/*                          ~OGRMSSQLSpatialTableLayer()                */
/************************************************************************/

OGRMSSQLSpatialTableLayer::~OGRMSSQLSpatialTableLayer()

{
    if ( bNeedSpatialIndex && nLayerStatus == MSSQLLAYERSTATUS_CREATED )
    {
        /* recreate spatial index */
        DropSpatialIndex();
        CreateSpatialIndex();
    }

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

    if (eGeomType != wkbNone)
        poFeatureDefn->SetGeomType(eGeomType);

    if ( GetSpatialRef() && poFeatureDefn->GetGeomFieldCount() == 1)
        poFeatureDefn->GetGeomFieldDefn(0)->SetSpatialRef( poSRS );

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
                                              const char *pszLayerNameIn,
                                              const char *pszGeomCol,
                                              CPL_UNUSED int nCoordDimension,
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
    const char *pszDot = strstr(pszLayerNameIn,".");
    if( pszDot != NULL )
    {
        pszTableName = CPLStrdup(pszDot + 1);
        pszSchemaName = CPLStrdup(pszLayerNameIn);
        pszSchemaName[pszDot - pszLayerNameIn] = '\0';
        this->pszLayerName = CPLStrdup(pszLayerNameIn);
    }
    else
    {
        pszTableName = CPLStrdup(pszLayerNameIn);
        pszSchemaName = CPLStrdup(pszSchema);
        if ( EQUAL(pszSchemaName, "dbo") )
            this->pszLayerName = CPLStrdup(pszLayerNameIn);
        else
            this->pszLayerName = CPLStrdup(CPLSPrintf("%s.%s", pszSchemaName, pszTableName));
    }
    SetDescription( this->pszLayerName );

/* -------------------------------------------------------------------- */
/*      Have we been provided a geometry column?                        */
/* -------------------------------------------------------------------- */
    CPLFree( pszGeomColumn );
    if( pszGeomCol == NULL )
        GetLayerDefn(); /* fetch geom column if not specified */
    else
        pszGeomColumn = CPLStrdup( pszGeomCol );

    if (eType != wkbNone)
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
            CPLError( CE_Warning, CPLE_AppDefined,
                          "Failed to get extent for spatial index." );
            return OGRERR_FAILURE;
        }

        if (oExt.MinX == oExt.MaxX || oExt.MinY == oExt.MaxY)
            return OGRERR_NONE; /* skip creating index */

        oStatement.Appendf("CREATE SPATIAL INDEX [ogr_%s_%s_%s_sidx] ON [%s].[%s] ( [%s] ) "
            "USING GEOMETRY_GRID WITH (BOUNDING_BOX =(%.15g, %.15g, %.15g, %.15g))",
                           pszSchemaName, pszTableName, pszGeomColumn,
                           pszSchemaName, pszTableName, pszGeomColumn,
                           oExt.MinX, oExt.MinY, oExt.MaxX, oExt.MaxY );
    }
    else if (nGeomColumnType == MSSQLCOLTYPE_GEOGRAPHY)
    {
        oStatement.Appendf("CREATE SPATIAL INDEX [ogr_%s_%s_%s_sidx] ON [%s].[%s] ( [%s] ) "
            "USING GEOGRAPHY_GRID",
                           pszSchemaName, pszTableName, pszGeomColumn,
                           pszSchemaName, pszTableName, pszGeomColumn );
    }
    else
    {
        CPLError( CE_Failure, CPLE_AppDefined,
            "Spatial index is not supported on the geometry column '%s'", pszGeomColumn);
        return OGRERR_FAILURE;
    }

    if( !oStatement.ExecuteSQL() )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                      "Failed to create the spatial index, %s.",
                      poDS->GetSession()->GetLastError());
        return OGRERR_FAILURE;
    }

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
        "WHERE object_id = OBJECT_ID(N'[%s].[%s]') AND name = N'ogr_%s_%s_%s_sidx') "
        "DROP INDEX [ogr_%s_%s_%s_sidx] ON [%s].[%s]",
                       pszSchemaName, pszTableName,
                       pszSchemaName, pszTableName, pszGeomColumn,
                       pszSchemaName, pszTableName, pszGeomColumn,
                       pszSchemaName, pszTableName );

    if( !oStatement.ExecuteSQL() )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                      "Failed to drop the spatial index, %s.",
                      poDS->GetSession()->GetLastError());
        return;
    }
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

    GetLayerDefn();

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

OGRFeature *OGRMSSQLSpatialTableLayer::GetFeature( GIntBig nFeatureId )

{
    if( pszFIDColumn == NULL )
        return OGRMSSQLSpatialLayer::GetFeature( nFeatureId );

    ClearStatement();

    iNextShapeId = nFeatureId;

    poStmt = new CPLODBCStatement( poDS->GetSession() );
    CPLString osFields = BuildFields();
    poStmt->Appendf( "select %s from %s where %s = " CPL_FRMT_GIB, osFields.c_str(),
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

OGRErr OGRMSSQLSpatialTableLayer::SetAttributeFilter( const char *pszQueryIn )

{
    CPLFree(m_pszAttrQueryString);
    m_pszAttrQueryString = (pszQueryIn) ? CPLStrdup(pszQueryIn) : NULL;

    if( (pszQueryIn == NULL && this->pszQuery == NULL)
        || (pszQueryIn != NULL && this->pszQuery != NULL
            && EQUAL(pszQueryIn,this->pszQuery)) )
        return OGRERR_NONE;

    CPLFree( this->pszQuery );
    this->pszQuery = (pszQueryIn) ? CPLStrdup( pszQueryIn ) : NULL;

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

GIntBig OGRMSSQLSpatialTableLayer::GetFeatureCount( int bForce )

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

    GIntBig nRet = CPLAtoGIntBig(poStatement->GetColData( 0 ));
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
            snprintf( szFieldType, sizeof(szFieldType), "numeric(%d,0)", oField.GetWidth() );
        else
            strcpy( szFieldType, "int" );
    }
    else if( oField.GetType() == OFTInteger64 )
    {
        if( oField.GetWidth() > 0 && bPreservePrecision )
            snprintf( szFieldType, sizeof(szFieldType), "numeric(%d,0)", oField.GetWidth() );
        else
            strcpy( szFieldType, "bigint" );
    }
    else if( oField.GetType() == OFTReal )
    {
        if( oField.GetWidth() > 0 && oField.GetPrecision() > 0
            && bPreservePrecision )
            snprintf( szFieldType, sizeof(szFieldType), "numeric(%d,%d)",
                     oField.GetWidth(), oField.GetPrecision() );
        else
            strcpy( szFieldType, "float" );
    }
    else if( oField.GetType() == OFTString )
    {
        if( oField.GetWidth() == 0 || !bPreservePrecision )
            strcpy( szFieldType, "nvarchar(MAX)" );
        else
            snprintf( szFieldType, sizeof(szFieldType), "nvarchar(%d)", oField.GetWidth() );
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

    if ( !oField.IsNullable() )
    {
        oStmt.Append(" NOT NULL");
    }
    if ( oField.GetDefault() != NULL && !oField.IsDefaultDriverSpecific() )
    {
        /* process default value specifications */
        if ( EQUAL(oField.GetDefault(), "CURRENT_TIME") )
            oStmt.Append(" DEFAULT(CONVERT([time],getdate()))");
        else if ( EQUAL(oField.GetDefault(), "CURRENT_DATE") )
            oStmt.Append( " DEFAULT(CONVERT([date],getdate()))" );
        else
            oStmt.Appendf(" DEFAULT(%s)", oField.GetDefault());
    }

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
/*                             ISetFeature()                             */
/*                                                                      */
/*      SetFeature() is implemented by an UPDATE SQL command            */
/************************************************************************/

OGRErr OGRMSSQLSpatialTableLayer::ISetFeature( OGRFeature *poFeature )

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
                  "Geometry with FID = " CPL_FRMT_GIB " has been modified.", poFeature->GetFID() );
    }

    int nFieldCount = poFeatureDefn->GetFieldCount();
    int bind_num = 0;
    void** bind_buffer = (void**)CPLMalloc(sizeof(void*) * nFieldCount);


    int bNeedComma = FALSE;
    SQLLEN nWKBLenBindParameter;
    if(poGeom != NULL && pszGeomColumn != NULL)
    {
        oStmt.Appendf( "[%s] = ", pszGeomColumn );

        if (nUploadGeometryFormat == MSSQLGEOMETRY_WKB)
        {
            int nWKBLen = poGeom->WkbSize();
            GByte *pabyWKB = (GByte *) CPLMalloc(nWKBLen + 1);

            if( poGeom->exportToWkb( wkbNDR, pabyWKB ) == OGRERR_NONE && (nGeomColumnType == MSSQLCOLTYPE_GEOMETRY
                || nGeomColumnType == MSSQLCOLTYPE_GEOGRAPHY))
            {
                nWKBLenBindParameter = nWKBLen;
                int nRetCode = SQLBindParameter(oStmt.GetStatement(), (SQLUSMALLINT)(bind_num + 1),
                    SQL_PARAM_INPUT, SQL_C_BINARY, SQL_LONGVARBINARY,
                    nWKBLen, 0, (SQLPOINTER)pabyWKB, nWKBLen, &nWKBLenBindParameter);
                if ( nRetCode == SQL_SUCCESS || nRetCode == SQL_SUCCESS_WITH_INFO )
                {
                    if (nGeomColumnType == MSSQLCOLTYPE_GEOGRAPHY)
                    {
                        oStmt.Append( "geography::STGeomFromWKB(?" );
                        oStmt.Appendf(",%d)", nSRSId );
                    }
                    else
                    {
                        oStmt.Append( "geometry::STGeomFromWKB(?" );
                        oStmt.Appendf(",%d).MakeValid()", nSRSId );
                    }
                    bind_buffer[bind_num] = pabyWKB;
                    ++bind_num;
                }
                else
                {
                    oStmt.Append( "null" );
                    CPLFree(pabyWKB);
                }
            }
            else
            {
                oStmt.Append( "null" );
                CPLFree(pabyWKB);
            }
        }
        else if (nUploadGeometryFormat == MSSQLGEOMETRY_WKT)
        {
            char    *pszWKT = NULL;
            if( poGeom->exportToWkt( &pszWKT ) == OGRERR_NONE && (nGeomColumnType == MSSQLCOLTYPE_GEOMETRY
                || nGeomColumnType == MSSQLCOLTYPE_GEOGRAPHY))
            {
                size_t nLen = 0;
                while(pszWKT[nLen] != '\0')
                    nLen ++;

                int nRetCode = SQLBindParameter(oStmt.GetStatement(), (SQLUSMALLINT)(bind_num + 1),
                    SQL_PARAM_INPUT, SQL_C_CHAR, SQL_LONGVARCHAR,
                    nLen, 0, (SQLPOINTER)pszWKT, 0, NULL);
                if ( nRetCode == SQL_SUCCESS || nRetCode == SQL_SUCCESS_WITH_INFO )
                {
                    if (nGeomColumnType == MSSQLCOLTYPE_GEOGRAPHY)
                    {
                        oStmt.Append( "geography::STGeomFromText(?" );
                        oStmt.Appendf(",%d)", nSRSId );
                    }
                    else
                    {
                        oStmt.Append( "geometry::STGeomFromText(?" );
                        oStmt.Appendf(",%d).MakeValid()", nSRSId );
                    }
                    bind_buffer[bind_num] = pszWKT;
                    ++bind_num;
                }
                else
                {
                    oStmt.Append( "null" );
                    CPLFree(pszWKT);
                }
            }
            else
            {
                oStmt.Append( "null" );
                CPLFree(pszWKT);
            }
        }
        else
            oStmt.Append( "null" );

        bNeedComma = TRUE;
    }

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
            AppendFieldValue(&oStmt, poFeature, i, &bind_num, bind_buffer);
    }

    /* Add the WHERE clause */
    oStmt.Appendf( " WHERE [%s] = " CPL_FRMT_GIB, pszFIDColumn, poFeature->GetFID());

/* -------------------------------------------------------------------- */
/*      Execute the update.                                             */
/* -------------------------------------------------------------------- */

    if( !oStmt.ExecuteSQL() )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
            "Error updating feature with FID:" CPL_FRMT_GIB ", %s", poFeature->GetFID(),
                    poDS->GetSession()->GetLastError() );

        for( i = 0; i < bind_num; i++ )
            CPLFree(bind_buffer[i]);
        CPLFree(bind_buffer);

        return OGRERR_FAILURE;
    }

    for( i = 0; i < bind_num; i++ )
            CPLFree(bind_buffer[i]);
    CPLFree(bind_buffer);

    if (oStmt.GetRowCountAffected() < 1)
        return OGRERR_NON_EXISTING_FEATURE;

    return OGRERR_NONE;
}

/************************************************************************/
/*                          DeleteFeature()                             */
/************************************************************************/

OGRErr OGRMSSQLSpatialTableLayer::DeleteFeature( GIntBig nFID )

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

    oStatement.Appendf("DELETE FROM [%s] WHERE [%s] = " CPL_FRMT_GIB,
            poFeatureDefn->GetName(), pszFIDColumn, nFID);

    if( !oStatement.ExecuteSQL() )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "Attempt to delete feature with FID " CPL_FRMT_GIB " failed. %s",
                  nFID, poDS->GetSession()->GetLastError() );

        return OGRERR_FAILURE;
    }

    if (oStatement.GetRowCountAffected() < 1)
        return OGRERR_NON_EXISTING_FEATURE;

    return OGRERR_NONE;
}

/************************************************************************/
/*                           ICreateFeature()                            */
/************************************************************************/

OGRErr OGRMSSQLSpatialTableLayer::ICreateFeature( OGRFeature *poFeature )

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

    /* the fid values are retrieved from the source layer */
    if( poFeature->GetFID() != OGRNullFID && pszFIDColumn != NULL && bIsIdentityFid )
        oStatement.Appendf( "SET IDENTITY_INSERT [%s].[%s] ON;",
                            pszSchemaName, pszTableName );

/* -------------------------------------------------------------------- */
/*      Form the INSERT command.                                        */
/* -------------------------------------------------------------------- */

    oStatement.Appendf( "INSERT INTO [%s].[%s] ", pszSchemaName, pszTableName );

    OGRMSSQLGeometryValidator oValidator(poFeature->GetGeometryRef());
    OGRGeometry *poGeom = oValidator.GetValidGeometryRef();

    GIntBig nFID = poFeature->GetFID();
    if (poFeature->GetGeometryRef() != poGeom)
    {
        CPLError( CE_Warning, CPLE_NotSupported,
                  "Geometry with FID = " CPL_FRMT_GIB " has been modified.", nFID );
    }

    int bNeedComma = FALSE;

    if (poGeom != NULL && pszGeomColumn != NULL)
    {
        oStatement.Append("([");
        oStatement.Append( pszGeomColumn );
        oStatement.Append("]");
        bNeedComma = TRUE;
    }

    if( nFID != OGRNullFID && pszFIDColumn != NULL )
    {
        if( !CPL_INT64_FITS_ON_INT32(nFID) &&
            GetMetadataItem(OLMD_FID64) == NULL )
        {
            /* MSSQL server doesn't support modifying pk columns without recreating the field */
            CPLError( CE_Failure, CPLE_AppDefined,
                  "Failed to create feature with large integer fid. "
                  "The FID64 layer creation option should be used." );
            return OGRERR_FAILURE;
        }

        if (bNeedComma)
            oStatement.Appendf( ", [%s]", pszFIDColumn );
        else
        {
            oStatement.Appendf( "([%s]", pszFIDColumn );
            bNeedComma = TRUE;
        }
    }

    int nFieldCount = poFeatureDefn->GetFieldCount();

    int bind_num = 0;
    void** bind_buffer = (void**)CPLMalloc(sizeof(void*) * (nFieldCount + 1));

    int i;
    for( i = 0; i < nFieldCount; i++ )
    {
        if( !poFeature->IsFieldSet( i ) )
            continue;

        if (bNeedComma)
            oStatement.Appendf( ", [%s]", poFeatureDefn->GetFieldDefn(i)->GetNameRef() );
        else
        {
            oStatement.Appendf( "([%s]", poFeatureDefn->GetFieldDefn(i)->GetNameRef() );
            bNeedComma = TRUE;
        }
    }

    SQLLEN nWKBLenBindParameter;
    if (oStatement.GetCommand()[strlen(oStatement.GetCommand()) - 1] != ']')
    {
        /* no fields were added */
        oStatement.Appendf( "DEFAULT VALUES;" );
    }
    else
    {
        oStatement.Appendf( ") VALUES (" );

        /* Set the geometry */
        bNeedComma = FALSE;
        if(poGeom != NULL && pszGeomColumn != NULL)
        {
            if (nUploadGeometryFormat == MSSQLGEOMETRY_WKB)
            {
                int nWKBLen = poGeom->WkbSize();
                GByte *pabyWKB = (GByte *) CPLMalloc(nWKBLen + 1);

                if( poGeom->exportToWkb( wkbNDR, pabyWKB ) == OGRERR_NONE && (nGeomColumnType == MSSQLCOLTYPE_GEOMETRY
                    || nGeomColumnType == MSSQLCOLTYPE_GEOGRAPHY))
                {
                    nWKBLenBindParameter = nWKBLen;
                    int nRetCode = SQLBindParameter(oStatement.GetStatement(), (SQLUSMALLINT)(bind_num + 1),
                        SQL_PARAM_INPUT, SQL_C_BINARY, SQL_LONGVARBINARY,
                        nWKBLen, 0, (SQLPOINTER)pabyWKB, nWKBLen, &nWKBLenBindParameter);
                    if ( nRetCode == SQL_SUCCESS || nRetCode == SQL_SUCCESS_WITH_INFO )
                    {
                        if (nGeomColumnType == MSSQLCOLTYPE_GEOGRAPHY)
                        {
                            oStatement.Append( "geography::STGeomFromWKB(?" );
                            oStatement.Appendf(",%d)", nSRSId );
                        }
                        else
                        {
                            oStatement.Append( "geometry::STGeomFromWKB(?" );
                            oStatement.Appendf(",%d).MakeValid()", nSRSId );
                        }
                        bind_buffer[bind_num] = pabyWKB;
                        ++bind_num;
                    }
                    else
                    {
                        oStatement.Append( "null" );
                        CPLFree(pabyWKB);
                    }
                }
                else
                {
                    oStatement.Append( "null" );
                    CPLFree(pabyWKB);
                }
            }
            else if (nUploadGeometryFormat == MSSQLGEOMETRY_WKT)
            {
                char    *pszWKT = NULL;
                if( poGeom->exportToWkt( &pszWKT ) == OGRERR_NONE && (nGeomColumnType == MSSQLCOLTYPE_GEOMETRY
                    || nGeomColumnType == MSSQLCOLTYPE_GEOGRAPHY))
                {
                    size_t nLen = 0;
                    while(pszWKT[nLen] != '\0')
                        nLen ++;

                    int nRetCode = SQLBindParameter(oStatement.GetStatement(), (SQLUSMALLINT)(bind_num + 1),
                        SQL_PARAM_INPUT, SQL_C_CHAR, SQL_LONGVARCHAR,
                        nLen, 0, (SQLPOINTER)pszWKT, 0, NULL);
                    if ( nRetCode == SQL_SUCCESS || nRetCode == SQL_SUCCESS_WITH_INFO )
                    {
                        if (nGeomColumnType == MSSQLCOLTYPE_GEOGRAPHY)
                        {
                            oStatement.Append( "geography::STGeomFromText(?" );
                            oStatement.Appendf(",%d)", nSRSId );
                        }
                        else
                        {
                            oStatement.Append( "geometry::STGeomFromText(?" );
                            oStatement.Appendf(",%d).MakeValid()", nSRSId );
                        }
                        bind_buffer[bind_num] = pszWKT;
                        ++bind_num;
                    }
                    else
                    {
                        oStatement.Append( "null" );
                        CPLFree(pszWKT);
                    }
                }
                else
                {
                    oStatement.Append( "null" );
                    CPLFree(pszWKT);
                }
            }
            else
                oStatement.Append( "null" );

            bNeedComma = TRUE;
        }

        /* Set the FID */
        if( nFID != OGRNullFID && pszFIDColumn != NULL )
        {
            if (bNeedComma)
                oStatement.Appendf( ", " CPL_FRMT_GIB, nFID );
            else
            {
                oStatement.Appendf( CPL_FRMT_GIB, nFID );
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

            AppendFieldValue(&oStatement, poFeature, i, &bind_num, bind_buffer);
        }

        oStatement.Append( ");" );
    }

    if( nFID != OGRNullFID && pszFIDColumn != NULL && bIsIdentityFid )
        oStatement.Appendf("SET IDENTITY_INSERT [%s].[%s] OFF;", pszSchemaName, pszTableName );

/* -------------------------------------------------------------------- */
/*      Execute the insert.                                             */
/* -------------------------------------------------------------------- */

    if( !oStatement.ExecuteSQL() )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "INSERT command for new feature failed. %s",
                   poDS->GetSession()->GetLastError() );

        for( i = 0; i < bind_num; i++ )
            CPLFree(bind_buffer[i]);
        CPLFree(bind_buffer);

        return OGRERR_FAILURE;
    }

    for( i = 0; i < bind_num; i++ )
            CPLFree(bind_buffer[i]);
    CPLFree(bind_buffer);

    return OGRERR_NONE;
}

/************************************************************************/
/*                          AppendFieldValue()                          */
/*                                                                      */
/* Used by CreateFeature() and SetFeature() to format a                 */
/* non-empty field value                                                */
/************************************************************************/

void OGRMSSQLSpatialTableLayer::AppendFieldValue(CPLODBCStatement *poStatement,
                                       OGRFeature* poFeature, int i, int *bind_num, void **bind_buffer)
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
        if( STARTS_WITH_CI(pszStrValue, "0000") )
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

    if( nOGRFieldType != OFTInteger && nOGRFieldType != OFTInteger64 && nOGRFieldType != OFTReal
        && !bIsDateNull )
    {
        if (nOGRFieldType == OFTString)
        {
            // bind UTF8 as unicode parameter
            wchar_t* buffer = CPLRecodeToWChar( pszStrValue, CPL_ENC_UTF8, CPL_ENC_UCS2);
            int nRetCode = SQLBindParameter(poStatement->GetStatement(), (SQLUSMALLINT)((*bind_num) + 1),
                SQL_PARAM_INPUT, SQL_C_WCHAR, SQL_WVARCHAR,
                wcslen(buffer) + 1, 0, (SQLPOINTER)buffer, 0, NULL);
            if ( nRetCode == SQL_SUCCESS || nRetCode == SQL_SUCCESS_WITH_INFO )
            {
                poStatement->Append( "?" );
                bind_buffer[*bind_num] = buffer;
                ++(*bind_num);
            }
            else
            {
                OGRMSSQLAppendEscaped(poStatement, pszStrValue);
                CPLFree(buffer);
            }
        }
        else
            OGRMSSQLAppendEscaped(poStatement, pszStrValue);
    }
    else
    {
        poStatement->Append( pszStrValue );
    }
}
