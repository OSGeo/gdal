/*****************************************************************************
 *
 * Project:  DB2 Spatial driver
 * Purpose:  Implements OGRDB2TableLayer class, access to an existing table.
 * Author:   David Adler, dadler at adtechgeospatial dot com
 *
 *****************************************************************************
 * Copyright (c) 2010, Tamas Szekeres
 * Copyright (c) 2015, David Adler
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
#include "ogr_db2.h"

/************************************************************************/
/*                         OGRDB2AppendEscaped( )                     */
/************************************************************************/

void OGRDB2AppendEscaped( CPLODBCStatement* poStatement,
                          const char* pszStrValue)
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
/*                          OGRDB2TableLayer()                 */
/************************************************************************/

OGRDB2TableLayer::OGRDB2TableLayer( OGRDB2DataSource *poDSIn )

{
    poDS = poDSIn;

    m_pszQuery = NULL;

    bUpdateAccess = TRUE;

    iNextShapeId = 0;

    nSRSId = -1;

    poFeatureDefn = NULL;

    pszTableName = NULL;
    m_pszLayerName = NULL;
    pszSchemaName = NULL;

    eGeomType = wkbNone;
}

/************************************************************************/
/*                          ~OGRDB2TableLayer()                */
/************************************************************************/

OGRDB2TableLayer::~OGRDB2TableLayer()

{
    CPLFree( pszTableName );
    CPLFree( m_pszLayerName );
    CPLFree( pszSchemaName );

    CPLFree( m_pszQuery );
    ClearStatement();
}

/************************************************************************/
/*                               GetName()                              */
/************************************************************************/

const char *OGRDB2TableLayer::GetName()

{
    return m_pszLayerName;
}

/************************************************************************/
/*                             GetLayerDefn()                           */
/************************************************************************/
OGRFeatureDefn* OGRDB2TableLayer::GetLayerDefn()
{
    if (poFeatureDefn)
        return poFeatureDefn;

    CPLODBCSession *poSession = poDS->GetSession();
    /* -------------------------------------------------------------------- */
    /*      Do we have a simple primary key?                                */
    /* -------------------------------------------------------------------- */
    CPLODBCStatement oGetKey( poSession );
    CPLDebug( "OGR_DB2TableLayer::GetLayerDefn",
              "pszTableName: %s; pszSchemaName: %s",
              pszTableName, pszSchemaName);
    if( oGetKey.GetPrimaryKeys( pszTableName, NULL, pszSchemaName ) ) {
        if( oGetKey.Fetch() )
        {
            pszFIDColumn = CPLStrdup(oGetKey.GetColData( 3 ));
            if( oGetKey.Fetch() ) // more than one field in key!
            {
                oGetKey.Clear();
                CPLFree( pszFIDColumn );
                pszFIDColumn = NULL;
                CPLDebug( "OGR_DB2TableLayer::GetLayerDefn",
                          "Table %s has multiple primary key fields, "
                          "ignoring them all.", pszTableName );
            } else {
                // Attempt to get the 'identity' and 'generated' information
                // from syscat.columns. This is only valid on DB2 LUW so if it
                // fails, we assume that we are running on z/OS.
                CPLODBCStatement oStatement = CPLODBCStatement(
                                                  poDS->GetSession());
                oStatement.Appendf( "select identity, generated "
                                    "from syscat.columns "
                                    "where tabschema = '%s' "
                                    "and tabname = '%s' and colname = '%s'",
                                    pszSchemaName, pszTableName,
                                    pszFIDColumn );
                CPLDebug( "OGR_DB2TableLayer::GetLayerDefn",
                          "Identity qry: %s", oStatement.GetCommand());

                if( oStatement.ExecuteSQL() )
                {
                    if( oStatement.Fetch() )
                    {
                        if ( oStatement.GetColData( 0 )
                                && EQUAL(oStatement.GetColData( 0 ), "Y")) {
                            bIsIdentityFid = TRUE;
                            if ( oStatement.GetColData( 1 ) ) {
                                cGenerated = oStatement.GetColData( 1 )[0];
                            }
                        }
                    }
                } else {
                    CPLDebug( "OGR_DB2TableLayer::GetLayerDefn",
                              "Must be z/OS");
                    // on z/OS, get all the column data for table and loop
                    // through looking for the FID column, then check the
                    // column default information for 'IDENTIY' and 'ALWAYS'
                    if (oGetKey.GetColumns(pszTableName, NULL, pszSchemaName))
                    {
                        CPLDebug( "OGR_DB2TableLayer::GetLayerDefn",
                                  "GetColumns succeeded");
                        CPLDebug( "OGR_DB2TableLayer::GetLayerDefn",
                                  "ColName[0]: '%s'",oGetKey.GetColName(0));
                        for (int idx = 0; idx < oGetKey.GetColCount(); idx++)
                        {
                            CPLDebug( "OGR_DB2TableLayer::GetLayerDefn",
                                      "ColName[0]: '%s'",
                                      oGetKey.GetColName(idx));
                            if (!strcmp(pszFIDColumn,oGetKey.GetColName(idx)))
                            {
                                CPLDebug( "OGR_DB2TableLayer::GetLayerDefn",
                                          "ColDef[0]: '%s'",
                                          oGetKey.GetColColumnDef(idx));
                                if (strstr(oGetKey.GetColColumnDef(idx),
                                           "IDENTITY"))
                                    bIsIdentityFid = TRUE;
                                if (strstr(oGetKey.GetColColumnDef(idx),
                                           "ALWAYS"))
                                    cGenerated = 'A';
                            }
                        }
                    }
                }
                CPLDebug( "OGR_DB2TableLayer::GetLayerDefn",
                          "FIDColumn: '%s', identity: '%d', generated: '%c'",
                          pszFIDColumn, bIsIdentityFid, cGenerated);
            }
        }
    } else
        CPLDebug( "OGR_DB2TableLayer::GetLayerDefn", "GetPrimaryKeys failed");

    /* -------------------------------------------------------------------- */
    /*      Get the column definitions for this table.                      */
    /* -------------------------------------------------------------------- */
    CPLODBCStatement oGetCol( poSession );
    CPLErr eErr;

    if( !oGetCol.GetColumns( pszTableName, "", pszSchemaName ) )
        return NULL;

    eErr = BuildFeatureDefn( m_pszLayerName, &oGetCol );
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
                  "No column definitions found for table '%s', "
                  "layer not usable.",
                  m_pszLayerName );
        return NULL;
    }

    /* -------------------------------------------------------------------- */
    /*      If we got a geometry column, does it exist?  Is it binary?      */
    /* -------------------------------------------------------------------- */

    if( pszGeomColumn != NULL )
    {
        poFeatureDefn->GetGeomFieldDefn(0)->SetName( pszGeomColumn );
        int iColumn = oGetCol.GetColId( pszGeomColumn );
        if( iColumn < 0 )
        {
            CPLError( CE_Failure, CPLE_AppDefined,
                      "Column %s requested for geometry, "
                      "but it does not exist.",
                      pszGeomColumn );
            CPLFree( pszGeomColumn );
            pszGeomColumn = NULL;
        }
    }

    return poFeatureDefn;
}

/************************************************************************/
/*                             Initialize()                             */
/************************************************************************/

CPLErr OGRDB2TableLayer::Initialize( const char *pszSchema,
                                     const char *pszLayerName,
                                     const char *pszGeomCol,
                                     CPL_UNUSED int nCoordDimension,
                                     int nSRId,
                                     const char *pszSRText,
                                     OGRwkbGeometryType eType )
{
    CPLFree( pszFIDColumn );
    pszFIDColumn = NULL;

    CPLDebug( "OGR_DB2TableLayer::Initialize",
              "schema: '%s', layerName: '%s', geomCol: '%s'",
              pszSchema, pszLayerName, pszGeomCol);
    CPLDebug( "OGR_DB2TableLayer::Initialize",
              "nSRId: '%d', eType: '%d', srText: '%s'",
              nSRId, eType, pszSRText);


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
        this->m_pszLayerName = CPLStrdup(pszLayerName);
    }
    else
    {
        pszTableName = CPLStrdup(pszLayerName);
        pszSchemaName = CPLStrdup(pszSchema);
        this->m_pszLayerName = CPLStrdup(CPLSPrintf("%s.%s", pszSchemaName,
                                       pszTableName));
    }
    SetDescription( this->m_pszLayerName );

    /* -------------------------------------------------------------------- */
    /*      Have we been provided a geometry column?                        */
    /* -------------------------------------------------------------------- */
    CPLFree( pszGeomColumn );
    if( pszGeomCol == NULL )
        GetLayerDefn(); /* fetch geom colum if not specified */
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

int OGRDB2TableLayer::FetchSRSId()
{
    CPLODBCStatement oStatement = CPLODBCStatement( poDS->GetSession() );

// first try to get the srid from st_geometry_columns
// if the spatial column was registered
    oStatement.Appendf( "select srs_id from db2gse.st_geometry_columns "
                        "where table_schema = '%s' and table_name = '%s'",
                        pszSchemaName, pszTableName );

    if( oStatement.ExecuteSQL() && oStatement.Fetch() )
    {
        if ( oStatement.GetColData( 0 ) )
            nSRSId = atoi( oStatement.GetColData( 0 ) );
    }

// If it was not found there, try to get it from the data table.
// This only works if there is spatial data in the first row.
    if (nSRSId < 0 )
    {
        oStatement.Clear();
        oStatement.Appendf("select db2gse.st_srid(%s) from %s.%s "
                           "fetch first row only",
                           pszGeomColumn, pszSchemaName, pszTableName);
        if ( oStatement.ExecuteSQL() && oStatement.Fetch() )
        {
            if ( oStatement.GetColData( 0 ) )
                nSRSId = atoi( oStatement.GetColData( 0 ) );
        }
    }
    CPLDebug( "OGR_DB2TableLayer::FetchSRSId", "nSRSId: '%d'",
              nSRSId);
    return nSRSId;
}

/************************************************************************/
/*                       CreateSpatialIndex()                           */
/*                                                                      */
/*      Create a spatial index on the geometry column of the layer      */
/************************************************************************/

OGRErr OGRDB2TableLayer::CreateSpatialIndex()
{
    GetLayerDefn();

    CPLODBCStatement oStatement( poDS->GetSession() );


    OGREnvelope oExt;
    if (GetExtent(&oExt, TRUE) != OGRERR_NONE)
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "Failed to get extent for spatial index." );
        return OGRERR_FAILURE;
    }

    oStatement.Appendf("CREATE SPATIAL INDEX [ogr_%s_sidx] ON %s.%s ( %s ) "
                       "USING GEOMETRY_GRID WITH (BOUNDING_BOX =(%.15g, g, "
                       "%.15g, %.15g))",
                       pszGeomColumn, pszSchemaName, pszTableName,
                       pszGeomColumn,
                       oExt.MinX, oExt.MinY, oExt.MaxX, oExt.MaxY );

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

void OGRDB2TableLayer::DropSpatialIndex()
{
    GetLayerDefn();

    CPLODBCStatement oStatement( poDS->GetSession() );

    oStatement.Appendf("DROP INDEX %s.%s",
                       pszSchemaName, pszTableName);

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

CPLString OGRDB2TableLayer::BuildFields()

{
    int i = 0;
    int nColumn = 0;
    CPLString osFieldList;

    GetLayerDefn();

    if( pszFIDColumn && poFeatureDefn->GetFieldIndex( pszFIDColumn ) == -1 )
    {
        /* Always get the FID column */
        osFieldList += " ";
        osFieldList += pszFIDColumn;
        osFieldList += " ";
        ++nColumn;
    }

    if( pszGeomColumn && !poFeatureDefn->IsGeometryIgnored())
    {
        if( nColumn > 0 )
            osFieldList += ", ";

        osFieldList += " db2gse.st_astext(";
        osFieldList += pszGeomColumn;
        osFieldList += ") as ";
        osFieldList += pszGeomColumn;

        osFieldList += " ";

        ++nColumn;
    }

    if (poFeatureDefn->GetFieldCount() > 0)
    {
        /* need to reconstruct the field ordinals list */
        CPLFree(panFieldOrdinals);
        panFieldOrdinals = (int *) CPLMalloc( sizeof(int)
                                              * poFeatureDefn->GetFieldCount() );

        for( i = 0; i < poFeatureDefn->GetFieldCount(); i++ )
        {
            if ( poFeatureDefn->GetFieldDefn(i)->IsIgnored() )
                continue;

            const char *pszName =poFeatureDefn->GetFieldDefn(i)->GetNameRef();

            if( nColumn > 0 )
                osFieldList += ", ";

            osFieldList += " ";
            osFieldList += pszName;
            osFieldList += " ";

            panFieldOrdinals[i] = nColumn;

            ++nColumn;
        }
    }

    return osFieldList;
}

/************************************************************************/
/*                           ClearStatement()                           */
/************************************************************************/

void OGRDB2TableLayer::ClearStatement()

{
    if( m_poStmt != NULL )
    {
        delete m_poStmt;
        m_poStmt = NULL;
    }
}

/************************************************************************/
/*                            GetStatement()                            */
/************************************************************************/

CPLODBCStatement *OGRDB2TableLayer::GetStatement()

{
    if( m_poStmt == NULL )
    {
        m_poStmt = BuildStatement(BuildFields());
        iNextShapeId = 0;
    }

    return m_poStmt;
}


/************************************************************************/
/*                           BuildStatement()                           */
/************************************************************************/

CPLODBCStatement* OGRDB2TableLayer::BuildStatement(const char* pszColumns)

{
    CPLODBCStatement* poStatement = new CPLODBCStatement( poDS->GetSession());
    poStatement->Append( "select " );
    poStatement->Append( pszColumns );
    poStatement->Append( " from " );
    poStatement->Append( pszSchemaName );
    poStatement->Append( "." );
    poStatement->Append( pszTableName );

    /* Append attribute query if we have it */
    if( m_pszQuery != NULL )
        poStatement->Appendf( " where (%s)", m_pszQuery );

    /* If we have a spatial filter, query on it */
    if ( m_poFilterGeom != NULL )
    {
        if( m_pszQuery == NULL )
            poStatement->Append( " where" );
        else
            poStatement->Append( " and" );

        poStatement->Appendf(" db2gse.envelopesintersect(%s,%.15g,%.15g,"
                             "%.15g,%.15g, 0) = 1",
                             pszGeomColumn, m_sFilterEnvelope.MinX,
                             m_sFilterEnvelope.MinY, m_sFilterEnvelope.MaxX,
                             m_sFilterEnvelope.MaxY );
    }

    CPLDebug( "OGR_DB2TableLayer::BuildStatement",
              "ExecuteSQL(%s)", poStatement->GetCommand() );
    if( poStatement->ExecuteSQL() ) {
//    CPLDebug( "OGR_DB2TableLayer::BuildStatement", "Execute successful");
        return poStatement;
    }
    else
    {
        delete poStatement;
        CPLDebug( "OGR_DB2TableLayer::BuildStatement", "ExecuteSQL Failed" );
        return NULL;
    }
}

/************************************************************************/
/*                            ResetReading()                            */
/************************************************************************/

void OGRDB2TableLayer::ResetReading()

{
    ClearStatement();
    OGRDB2Layer::ResetReading();
}

/************************************************************************/
/*                             GetFeature()                             */
/************************************************************************/

OGRFeature *OGRDB2TableLayer::GetFeature( GIntBig nFeatureId )

{

    if( pszFIDColumn == NULL )
        return OGRDB2Layer::GetFeature( nFeatureId );

    ClearStatement();

    iNextShapeId = nFeatureId;

    m_poStmt = new CPLODBCStatement( poDS->GetSession() );
    CPLString osFields = BuildFields();
    m_poStmt->Appendf( "select %s from %s where %s = %ld", osFields.c_str(),
                     poFeatureDefn->GetName(), pszFIDColumn, nFeatureId );

    if( !m_poStmt->ExecuteSQL() )
    {
        delete m_poStmt;
        m_poStmt = NULL;
        return NULL;
    }

    return GetNextRawFeature();
}

/************************************************************************/
/*                         SetAttributeFilter()                         */
/************************************************************************/

OGRErr OGRDB2TableLayer::SetAttributeFilter( const char *pszQuery )

{
    CPLFree(m_pszAttrQueryString);
    m_pszAttrQueryString = (pszQuery) ? CPLStrdup(pszQuery) : NULL;

    if( (pszQuery == NULL && this->m_pszQuery == NULL)
            || (pszQuery != NULL && this->m_pszQuery != NULL
                && EQUAL(pszQuery,this->m_pszQuery)) )
        return OGRERR_NONE;

    CPLFree( this->m_pszQuery );
    this->m_pszQuery = (pszQuery) ? CPLStrdup( pszQuery ) : NULL;

    ClearStatement();

    return OGRERR_NONE;
}


/************************************************************************/
/*                           TestCapability()                           */
/************************************************************************/

int OGRDB2TableLayer::TestCapability( const char * pszCap )

{
    if ( bUpdateAccess )
    {
        if( EQUAL(pszCap,OLCSequentialWrite) || EQUAL(pszCap,OLCCreateField)
                || EQUAL(pszCap,OLCDeleteFeature) )
            return TRUE;

        else if( EQUAL(pszCap,OLCRandomWrite) )
            return (pszFIDColumn != NULL);
    }

    if( EQUAL(pszCap,OLCTransactions) )
        return TRUE;

    if( EQUAL(pszCap,OLCIgnoreFields) )
        return TRUE;

    if( EQUAL(pszCap,OLCRandomRead) )
        return (pszFIDColumn != NULL);
    else if( EQUAL(pszCap,OLCFastFeatureCount) )
        return TRUE;
    else
        return OGRDB2Layer::TestCapability( pszCap );
}

/************************************************************************/
/*                          GetFeatureCount()                           */
/************************************************************************/

GIntBig OGRDB2TableLayer::GetFeatureCount( int bForce )

{
    GetLayerDefn();

    if( TestCapability(OLCFastFeatureCount) == FALSE )
        return OGRDB2Layer::GetFeatureCount( bForce );

    ClearStatement();

    CPLODBCStatement* poStatement = BuildStatement( "count(*)" );

    if (poStatement == NULL || !poStatement->Fetch())
    {
        delete poStatement;
        return OGRDB2Layer::GetFeatureCount( bForce );
    }

    int nRet = atoi(poStatement->GetColData( 0 ));
    delete poStatement;
    return nRet;
}


/************************************************************************/
/*                            CreateField()                             */
/************************************************************************/

OGRErr OGRDB2TableLayer::CreateField( OGRFieldDefn *poFieldIn,
                                      int bApproxOK )

{
    char                szFieldType[256];
    OGRFieldDefn        oField( poFieldIn );

    GetLayerDefn();

    /* -------------------------------------------------------------------- */
    /*      Do we want to "launder" the column names into DB2               */
    /*      friendly format?                                                */
    /* -------------------------------------------------------------------- */
    if( bLaunderColumnNames )
    {
        char    *pszSafeName = poDS->LaunderName( oField.GetNameRef() );

        oField.SetName( pszSafeName );
        CPLFree( pszSafeName );
    }

    /* -------------------------------------------------------------------- */
    /*      Identify the DB2 type.                                          */
    /* -------------------------------------------------------------------- */

    int fieldType = oField.GetType();
    CPLDebug("OGR_DB2TableLayer::CreateField","fieldType: %d", fieldType);
    if( oField.GetType() == OFTInteger )
    {
        if( oField.GetWidth() > 0 && bPreservePrecision )
            sprintf( szFieldType, "numeric(%d,0)", oField.GetWidth() );
        else
            strcpy( szFieldType, "int" );
    }
    else if( oField.GetType() == OFTInteger64 )
    {
        if( oField.GetWidth() > 0 && bPreservePrecision )
            sprintf( szFieldType, "numeric(%d,0)", oField.GetWidth() );
        else
            strcpy( szFieldType, "bigint" );
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
                  "Can't create field %s with type %s on DB2 layers.  "
                  "Creating as varchar.",
                  oField.GetNameRef(),
                  OGRFieldDefn::GetFieldTypeName(oField.GetType()) );
        strcpy( szFieldType, "varchar" );
    }
    else
    {
        CPLError( CE_Failure, CPLE_NotSupported,
                  "Can't create field %s with type %s on DB2 layers.",
                  oField.GetNameRef(),
                  OGRFieldDefn::GetFieldTypeName(oField.GetType()) );

        return OGRERR_FAILURE;
    }

    /* -------------------------------------------------------------------- */
    /*      Create the new field.                                           */
    /* -------------------------------------------------------------------- */

    CPLODBCStatement oStmt( poDS->GetSession() );

    oStmt.Appendf( "ALTER TABLE %s.%s ADD COLUMN %s %s",
                   pszSchemaName, pszTableName, oField.GetNameRef(),
                   szFieldType);

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
    CPLDebug("OGR_DB2TableLayer::CreateField","stmt: %s", oStmt.GetCommand());
    return OGRERR_NONE;
}

/************************************************************************/
/*                            ISetFeature()                             */
/*                                                                      */
/*     ISetFeature() is implemented by an UPDATE SQL command            */
/************************************************************************/

OGRErr OGRDB2TableLayer::ISetFeature( OGRFeature *poFeature )

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

    oStmt.Appendf( "UPDATE %s.%s SET ", pszSchemaName, pszTableName);

    int nFieldCount = poFeatureDefn->GetFieldCount();
    int bind_num = 0;
    void** bind_buffer = (void**)CPLMalloc(sizeof(void*) * nFieldCount);

    /* Set the geometry */
    int bNeedComma = FALSE;
    OGRGeometry *poGeom = poFeature->GetGeometryRef();
    char    *pszWKT = NULL;
    if(pszGeomColumn != NULL)
    {
        if (poGeom != NULL) {
            if( poGeom->exportToWkt( &pszWKT ) == OGRERR_NONE)
            {
                size_t nLen = 0;
                while(pszWKT[nLen] != '\0') nLen ++;

                int nRetCode = SQLBindParameter(oStmt.GetStatement(),
                                                (SQLUSMALLINT)(bind_num + 1),
                                                SQL_PARAM_INPUT, SQL_C_CHAR,
                                                SQL_LONGVARCHAR,
                                                nLen, 0,
                                                (SQLPOINTER)pszWKT, 0, NULL);
                if ( nRetCode == SQL_SUCCESS
                        || nRetCode == SQL_SUCCESS_WITH_INFO )
                {
                    CPLDebug("OGR_DB2TableLayer::UpdateFeature",
                             "nRetCode: %d",
                             nRetCode);
                    oStmt.Appendf( "%s = ", pszGeomColumn );
                    oStmt.Appendf( "DB2GSE.ST_%s(CAST( ? AS CLOB(%d)),%d)",
                                   poGeom->getGeometryName(), nLen, nSRSId );
                    CPLDebug("OGR_DB2TableLayer::UpdateFeature",
                             "bind_num: %d;  wkt: %s",
                             bind_num, pszWKT);
                    bind_buffer[bind_num] = pszWKT;
                    ++bind_num;
                }
            }

            bNeedComma = TRUE;
        }
    }

    int i;
    for( i = 0; i < nFieldCount; i++ )
    {
        if (bNeedComma)
            oStmt.Appendf( ", %s = ",
                           poFeatureDefn->GetFieldDefn(i)->GetNameRef() );
        else
        {
            oStmt.Appendf( "%s = ",
                           poFeatureDefn->GetFieldDefn(i)->GetNameRef() );
            bNeedComma = TRUE;
        }

        if( !poFeature->IsFieldSet( i ) )
            oStmt.Append( "null" );
        else
            AppendFieldValue(&oStmt, poFeature, i, &bind_num, bind_buffer);
    }

    /* Add the WHERE clause */
    oStmt.Appendf( " WHERE (%s) = " CPL_FRMT_GIB, pszFIDColumn,
                   poFeature->GetFID());

    /* -------------------------------------------------------------------- */
    /*      Execute the update.                                             */
    /* -------------------------------------------------------------------- */
    CPLDebug("OGR_DB2TableLayer::UpdateFeature",
             "statement: %s", oStmt.GetCommand());
    if( !oStmt.ExecuteSQL() )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "Error updating feature with FID:" CPL_FRMT_GIB ", %s",
                  poFeature->GetFID(),
                  poDS->GetSession()->GetLastError() );

        return OGRERR_FAILURE;
    }

    for( i = 0; i < bind_num; i++ )
        CPLFree(bind_buffer[i]);
    CPLFree(bind_buffer);

    return OGRERR_NONE;
}

/************************************************************************/
/*                          DeleteFeature()                             */
/************************************************************************/

OGRErr OGRDB2TableLayer::DeleteFeature( GIntBig nFID )

{
    CPLDebug("OGR_DB2TableLayer::DeleteFeature",
             " entering, nFID: " CPL_FRMT_GIB,nFID);
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

    oStatement.Appendf("DELETE FROM %s WHERE %s = " CPL_FRMT_GIB,
                       poFeatureDefn->GetName(), pszFIDColumn, nFID);
    CPLDebug("OGR_DB2TableLayer::DeleteFeature"," sql: '%s'",
             oStatement.GetCommand());
    if( !oStatement.ExecuteSQL() )
    {
        CPLDebug("OGR_DB2TableLayer::DeleteFeature failed",
                 " sql: '%s'",oStatement.GetCommand());
        CPLError( CE_Failure, CPLE_AppDefined,
                  "Attempt to delete feature with FID " CPL_FRMT_GIB
                  " failed. %s",
                  nFID, poDS->GetSession()->GetLastError() );
        return OGRERR_FAILURE;
    }

    return OGRERR_NONE;
}

/************************************************************************/
/*                          ICreateFeature()                            */
/************************************************************************/

OGRErr OGRDB2TableLayer::ICreateFeature( OGRFeature *poFeature )

{
    char    *pszWKT = NULL;
    GetLayerDefn();

    if( NULL == poFeature )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "NULL pointer to OGRFeature passed to CreateFeature()." );
        return OGRERR_FAILURE;
    }

    ClearStatement();

    CPLODBCStatement oStatement( poDS->GetSession() );

    /* -------------------------------------------------------------------- */
    /*      Form the INSERT command.                                        */
    /* -------------------------------------------------------------------- */

    oStatement.Appendf( "INSERT INTO %s.%s (", pszSchemaName, pszTableName );

    int bNeedComma = FALSE;
    OGRGeometry *poGeom = poFeature->GetGeometryRef();

    if (poGeom != NULL && pszGeomColumn != NULL)
    {
        oStatement.Append( pszGeomColumn );
        bNeedComma = TRUE;
    }

    if( poFeature->GetFID() != OGRNullFID
            && pszFIDColumn != NULL && cGenerated != 'A' )
    {
        if (bNeedComma) oStatement.Appendf( ", ");
        oStatement.Appendf( "%s", pszFIDColumn );
        bNeedComma = TRUE;
    }

    int nFieldCount = poFeatureDefn->GetFieldCount();
    int bind_num = 0;
    void** bind_buffer = (void**)CPLMalloc(sizeof(void*) * (nFieldCount + 1));
    int i;
    for( i = 0; i < nFieldCount; i++ )
    {
        if( !poFeature->IsFieldSet( i ) )
            continue;
        if (bNeedComma) oStatement.Appendf( ", ");
        oStatement.Appendf( "%s",
                            poFeatureDefn->GetFieldDefn(i)->GetNameRef() );
        bNeedComma = TRUE;
    }

    oStatement.Appendf( ") VALUES (" );

    /* Set the geometry */
    bNeedComma = FALSE;
    if(poGeom != NULL && pszGeomColumn != NULL)
    {


        //poGeom->setCoordinateDimension( nCoordDimension );

        if( poGeom->exportToWkt( &pszWKT ) == OGRERR_NONE)
        {
            size_t nLen = 0;

            while(pszWKT[nLen] != '\0')
                nLen ++;

            int nRetCode = SQLBindParameter(oStatement.GetStatement(),
                                            (SQLUSMALLINT)(bind_num + 1),
                                            SQL_PARAM_INPUT, SQL_C_CHAR,
                                            SQL_LONGVARCHAR,
                                            nLen, 0, (SQLPOINTER)pszWKT,
                                            0, NULL);
            if ( nRetCode == SQL_SUCCESS
                    || nRetCode == SQL_SUCCESS_WITH_INFO )
            {
                oStatement.Appendf( "DB2GSE.ST_%s(CAST( ? AS CLOB(%d)),%d)",
                                    poGeom->getGeometryName(), nLen, nSRSId );

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


        bNeedComma = TRUE;
    }

    if( poFeature->GetFID() != OGRNullFID && pszFIDColumn != NULL
            && cGenerated != 'A' ) {
        if (bNeedComma) oStatement.Appendf( ", ");
        oStatement.Appendf( CPL_FRMT_GIB, poFeature->GetFID() );
        bNeedComma = TRUE;
    }

    for( i = 0; i < nFieldCount; i++ )
    {
        if( !poFeature->IsFieldSet( i ) )
            continue;

        if (bNeedComma) oStatement.Appendf( ", ");
        bNeedComma = TRUE;
        AppendFieldValue(&oStatement, poFeature, i, &bind_num, bind_buffer);
    }

    oStatement.Append( ");" );


    /* -------------------------------------------------------------------- */
    /*      Execute the insert.                                             */
    /* -------------------------------------------------------------------- */
    CPLDebug("OGR_DB2TableLayer::ICreateFeature",
             "stmt: '%s'", oStatement.GetCommand());
    if( !oStatement.ExecuteSQL() )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "INSERT command for new feature failed. %s",
                  poDS->GetSession()->GetLastError() );

        CPLDebug("OGR_DB2TableLayer::ICreateFeature"," insert failed; '%s'",
                 poDS->GetSession()->GetLastError() );
        return OGRERR_FAILURE;
    }

    GIntBig oldFID = poFeature->GetFID();
    if( bIsIdentityFid) {
        CPLODBCStatement oStatement2( poDS->GetSession() );
        oStatement2.Appendf( "select IDENTITY_VAL_LOCAL() AS IDENTITY "
                             "FROM SYSIBM.SYSDUMMY1");
        if( oStatement2.ExecuteSQL() && oStatement2.Fetch() )
        {
            if ( oStatement2.GetColData( 0 ) )
                poFeature->SetFID( atoi(oStatement2.GetColData( 0 ) ));
        }
    }
    CPLDebug("OGR_DB2TableLayer::ICreateFeature","Old FID: " CPL_FRMT_GIB
             "; New FID: " CPL_FRMT_GIB, oldFID, poFeature->GetFID());
    // bind_buffer is only used for values associated with a parameter marker
    for( i = 0; i < bind_num; i++ ) {
        CPLFree(bind_buffer[i]);
    };
    CPLFree(bind_buffer);

    return OGRERR_NONE;
}

/************************************************************************/
/*                          AppendFieldValue()                          */
/*                                                                      */
/* Used by CreateFeature() and SetFeature() to format a                 */
/* non-empty field value                                                */
/************************************************************************/

void OGRDB2TableLayer::AppendFieldValue(CPLODBCStatement *poStatement,
                                        OGRFeature* poFeature, int i,
                                        int *bind_num, void **bind_buffer)
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
        OGRDB2AppendEscaped(poStatement, pszStrValue);
    }
    else
    {
        poStatement->Append( pszStrValue );
    }
}
