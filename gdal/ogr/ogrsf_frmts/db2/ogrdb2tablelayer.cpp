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

CPL_CVSID("$Id$")

/************************************************************************/
/*                         OGRDB2AppendEscaped( )                     */
/************************************************************************/

void OGRDB2AppendEscaped( OGRDB2Statement* poStatement,
                          const char* pszStrValue)
{
    if (!pszStrValue)
    {
        poStatement->Append("null");
        return;
    }

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

OGRDB2TableLayer::OGRDB2TableLayer( OGRDB2DataSource *poDSIn ) :
    eGeomType( wkbNone )

{
    poDS = poDSIn;
    m_poStmt = nullptr;
    m_poPrepStmt = nullptr;
    m_pszQuery = nullptr;
    m_nFeaturesRead = 0;

    bUpdateAccess = TRUE;

    iNextShapeId = 0;

    nSRSId = -1;

    poFeatureDefn = nullptr;

    pszTableName = nullptr;
    m_pszLayerName = nullptr;
    pszSchemaName = nullptr;
    pszFIDColumn = nullptr;

    bLaunderColumnNames = false;
    bPreservePrecision = false;
    bNeedSpatialIndex = false;
    m_iSrs = 0;
}

/************************************************************************/
/*                          ~OGRDB2TableLayer()                */
/************************************************************************/

OGRDB2TableLayer::~OGRDB2TableLayer()

{
    CPLDebug("OGRDB2TableLayer::~OGRDB2TableLayer","entering");
    CPLFree( pszTableName );
    CPLFree( m_pszLayerName );
    CPLFree( pszSchemaName );

    CPLFree( m_pszQuery );
    ClearStatement();
    if( m_poPrepStmt != nullptr )
    {
        delete m_poPrepStmt;
        m_poPrepStmt = nullptr;
    }
    CPLDebug("OGRDB2TableLayer::~OGRDB2TableLayer","exiting");
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

    OGRDB2Session *poSession = poDS->GetSession();
    /* -------------------------------------------------------------------- */
    /*      Do we have a simple primary key?                                */
    /* -------------------------------------------------------------------- */
    OGRDB2Statement oGetKey( poSession );
    CPLDebug( "OGR_DB2TableLayer::GetLayerDefn",
              "pszTableName: %s; pszSchemaName: %s",
              pszTableName, pszSchemaName);
    if( oGetKey.GetPrimaryKeys( pszTableName, nullptr, pszSchemaName ) ) {
        if( oGetKey.Fetch() )
        {
            pszFIDColumn = CPLStrdup(oGetKey.GetColData( 3 ));
            if( oGetKey.Fetch() ) // more than one field in key!
            {
                oGetKey.Clear();
                CPLFree( pszFIDColumn );
                pszFIDColumn = nullptr;
                CPLDebug( "OGR_DB2TableLayer::GetLayerDefn",
                          "Table %s has multiple primary key fields, "
                          "ignoring them all.", pszTableName );
            } else {
                // Attempt to get the 'identity' and 'generated' information
                // from syscat.columns. This is only valid on DB2 LUW so if it
                // fails, we assume that we are running on z/OS.
                OGRDB2Statement oStatement = OGRDB2Statement(
                                                 poDS->GetSession());
                oStatement.Appendf( "select identity, generated "
                                    "from syscat.columns "
                                    "where tabschema = '%s' "
                                    "and tabname = '%s' and colname = '%s'",
                                    pszSchemaName, pszTableName,
                                    pszFIDColumn );

                if( oStatement.DB2Execute("OGR_DB2TableLayer::GetLayerDefn") )
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
                    if (oGetKey.GetColumns(pszTableName, nullptr, pszSchemaName))
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
    OGRDB2Statement oGetCol( poSession );
    CPLErr eErr;

    if( !oGetCol.GetColumns( pszTableName, "", pszSchemaName ) )
        return nullptr;

    eErr = BuildFeatureDefn( m_pszLayerName, &oGetCol );
    if( eErr != CE_None )
        return nullptr;

    if (eGeomType != wkbNone)
        poFeatureDefn->SetGeomType(eGeomType);

    if ( GetSpatialRef() && poFeatureDefn->GetGeomFieldCount() == 1)
        poFeatureDefn->GetGeomFieldDefn(0)->SetSpatialRef( poSRS );

    if( poFeatureDefn->GetFieldCount() == 0 &&
            pszFIDColumn == nullptr && pszGeomColumn == nullptr )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "No column definitions found for table '%s', "
                  "layer not usable.",
                  m_pszLayerName );
        return nullptr;
    }

    /* -------------------------------------------------------------------- */
    /*      If we got a geometry column, does it exist?  Is it binary?      */
    /* -------------------------------------------------------------------- */

    if( pszGeomColumn != nullptr )
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
            pszGeomColumn = nullptr;
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
//    CPLFree( pszFIDColumn );
    pszFIDColumn = nullptr;

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
    if( pszDot != nullptr )
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
    CPLDebug( "OGR_DB2TableLayer::Initialize",
              "this->m_pszLayerName: '%s', layerName: '%s', geomCol: '%s'",
              this->m_pszLayerName, pszLayerName, pszGeomCol);
    /* -------------------------------------------------------------------- */
    /*      Have we been provided a geometry column?                        */
    /* -------------------------------------------------------------------- */
//    CPLFree( pszGeomColumn ); LATER
    if( pszGeomCol == nullptr )
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
        poSRS->SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);
        if( poSRS->importFromWkt( (char**)&pszSRText ) != OGRERR_NONE )
        {
            delete poSRS;
            poSRS = nullptr;
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
    OGRDB2Statement oStatement = OGRDB2Statement( poDS->GetSession() );

// first try to get the srid from st_geometry_columns
// if the spatial column was registered
    oStatement.Appendf( "select srs_id from db2gse.st_geometry_columns "
                        "where table_schema = '%s' and table_name = '%s'",
                        pszSchemaName, pszTableName );

    if( oStatement.DB2Execute("OGRDB2TableLayer::FetchSRSId")
            && oStatement.Fetch() )
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
        if ( oStatement.DB2Execute("OGR_DB2TableLayer::FetchSRSId")
                && oStatement.Fetch() )
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
    CPLDebug("OGRDB2TableLayer::CreateSpatialIndex","Enter");
    if (poDS->m_bIsZ) {
        CPLDebug("OGRDB2TableLayer::CreateSpatialIndex",
                 "Don't create spatial index on z/OS");
        return OGRERR_NONE;
    }
    GetLayerDefn();

    OGRDB2Statement oStatement( poDS->GetSession() );

    OGREnvelope oExt;
    if (GetExtent(&oExt, TRUE) != OGRERR_NONE)
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "Failed to get extent for spatial index." );
        return OGRERR_FAILURE;
    }
    CPLDebug("OGRDB2TableLayer::CreateSpatialIndex",
             "BOUNDING_BOX =(%.15g, %.15g, %.15g, %.15g)",
             oExt.MinX, oExt.MinY, oExt.MaxX, oExt.MaxY );

    oStatement.Appendf("CREATE  INDEX %s.%s_sidx ON %s.%s ( %s ) "
                       "extend using db2gse.spatial_index(.1,0.5,0)",
                       pszSchemaName, pszTableName,
                       pszSchemaName, pszTableName, pszGeomColumn );

    if( !oStatement.DB2Execute("OGR_DB2TableLayer::CreateSpatialIndex") )
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

void OGRDB2TableLayer::DropSpatialIndex()
{
    GetLayerDefn();

    OGRDB2Statement oStatement( poDS->GetSession() );

    oStatement.Appendf("DROP INDEX %s.%s",
                       pszSchemaName, pszTableName);

    //poDS->GetSession()->BeginTransaction();

    if( !oStatement.DB2Execute("OGR_DB2TableLayer::DropSpatialIndex") )
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

        for( int i = 0; i < poFeatureDefn->GetFieldCount(); i++ )
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
    if( m_poStmt != nullptr )
    {
        delete m_poStmt;
        m_poStmt = nullptr;
    }
}

/************************************************************************/
/*                            GetStatement()                            */
/************************************************************************/

OGRDB2Statement *OGRDB2TableLayer::GetStatement()

{
    if( m_poStmt == nullptr )
    {
        m_poStmt = BuildStatement(BuildFields());
        iNextShapeId = 0;
    }

    return m_poStmt;
}

/************************************************************************/
/*                           BuildStatement()                           */
/************************************************************************/

OGRDB2Statement* OGRDB2TableLayer::BuildStatement(const char* pszColumns)

{
    OGRDB2Statement* poStatement = new OGRDB2Statement( poDS->GetSession());
    poStatement->Append( "select " );
    poStatement->Append( pszColumns );
    poStatement->Append( " from " );
    poStatement->Append( pszSchemaName );
    poStatement->Append( "." );
    poStatement->Append( pszTableName );

    /* Append attribute query if we have it */
    if( m_pszQuery != nullptr )
        poStatement->Appendf( " where (%s)", m_pszQuery );

    /* If we have a spatial filter, query on it */
    if ( m_poFilterGeom != nullptr )
    {
        if( m_pszQuery == nullptr )
            poStatement->Append( " where" );
        else
            poStatement->Append( " and" );

        poStatement->Appendf(" db2gse.envelopesintersect(%s,%.15g,%.15g,"
                             "%.15g,%.15g, 0) = 1",
                             pszGeomColumn, m_sFilterEnvelope.MinX,
                             m_sFilterEnvelope.MinY, m_sFilterEnvelope.MaxX,
                             m_sFilterEnvelope.MaxY );
    }

    if( poStatement->DB2Execute("OGR_DB2TableLayer::BuildStatement") ) {
        return poStatement;
    }
    else
    {
        delete poStatement;
        CPLDebug( "OGR_DB2TableLayer::BuildStatement", "ExecuteSQL Failed" );
        return nullptr;
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

    if( pszFIDColumn == nullptr )
        return OGRDB2Layer::GetFeature( nFeatureId );

    ClearStatement();

    iNextShapeId = nFeatureId;

    m_poStmt = new OGRDB2Statement( poDS->GetSession() );
    CPLString osFields = BuildFields();
    m_poStmt->Appendf( "select %s from %s where %s = " CPL_FRMT_GIB, osFields.c_str(),
                       poFeatureDefn->GetName(), pszFIDColumn, nFeatureId );

    if( !m_poStmt->DB2Execute("OGR_DB2TableLayer::GetFeature") )
    {
        delete m_poStmt;
        m_poStmt = nullptr;
        return nullptr;
    }

    return GetNextRawFeature();
}

/************************************************************************/
/*                         SetAttributeFilter()                         */
/************************************************************************/

OGRErr OGRDB2TableLayer::SetAttributeFilter( const char *pszQuery )

{
    CPLFree(m_pszAttrQueryString);
    m_pszAttrQueryString = (pszQuery) ? CPLStrdup(pszQuery) : nullptr;

    if( (pszQuery == nullptr && this->m_pszQuery == nullptr)
            || (pszQuery != nullptr && this->m_pszQuery != nullptr
                && EQUAL(pszQuery,this->m_pszQuery)) )
        return OGRERR_NONE;

    CPLFree( this->m_pszQuery );
    this->m_pszQuery = (pszQuery) ? CPLStrdup( pszQuery ) : nullptr;

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
            return pszFIDColumn != nullptr;
    }

    if( EQUAL(pszCap,OLCTransactions) )
        return TRUE;

    if( EQUAL(pszCap,OLCIgnoreFields) )
        return TRUE;

    if( EQUAL(pszCap,OLCRandomRead) )
        return pszFIDColumn != nullptr;
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

    OGRDB2Statement* poStatement = BuildStatement( "count(*)" );

    if (poStatement == nullptr || !poStatement->Fetch())
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
            strcpy( szFieldType, "varchar(MAX)" );
        else
            snprintf( szFieldType, sizeof(szFieldType), "varchar(%d)", oField.GetWidth() );
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

    OGRDB2Statement oStmt( poDS->GetSession() );

    oStmt.Appendf( "ALTER TABLE %s.%s ADD COLUMN %s %s",
                   pszSchemaName, pszTableName, oField.GetNameRef(),
                   szFieldType);

    if( !oStmt.DB2Execute("OGR_DB2TableLayer::CreateField") )
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
/*                            ISetFeature()                             */
/*                                                                      */
/*     ISetFeature() is implemented by an UPDATE SQL command            */
/************************************************************************/

OGRErr OGRDB2TableLayer::ISetFeature( OGRFeature *poFeature )

{
    OGRErr              eErr = OGRERR_FAILURE;

    GetLayerDefn();

    if( nullptr == poFeature )
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
    if (PrepareFeature(poFeature, 'U'))
        return OGRERR_FAILURE;

    int nFieldCount = poFeatureDefn->GetFieldCount();
    int nBindNum = 0;
    void** papBindBuffer = (void**)CPLMalloc(sizeof(void*) * nFieldCount);

    /* Set the geometry */
    OGRGeometry *poGeom = poFeature->GetGeometryRef();
    char    *pszWKT = nullptr;

    if (poGeom != nullptr && pszGeomColumn != nullptr)
    {
        if( poGeom->exportToWkt( &pszWKT ) == OGRERR_NONE)
        {
            int nLen = (int) strlen(pszWKT);
            if (m_poPrepStmt->DB2BindParameterIn(
                        "OGRDB2TableLayer::UpdateFeature",
                        (nBindNum + 1),
                        SQL_C_CHAR,
                        SQL_LONGVARCHAR,
                        nLen,
                        (void *)(pszWKT)))
            {
                papBindBuffer[nBindNum] = pszWKT;
                nBindNum++;
            } else {
                CPLDebug("OGRDB2TableLayer::UpdateFeature",
                         "Bind parameter failed");
                FreeBindBuffer(nBindNum, papBindBuffer);
                return OGRERR_FAILURE;
            }
        }
    }

    for( int i = 0; i < nFieldCount; i++ )
    {
//        int nOGRFieldType = poFeatureDefn->GetFieldDefn(i)->GetType();
//        CPLDebug("OGRDB2TableLayer::UpdateFeature",
//               "i: %d; nOGRFieldType: %d",
//                i, nOGRFieldType);

        if (BindFieldValue(m_poPrepStmt,
                           poFeature, i,
                           nBindNum, papBindBuffer) != OGRERR_NONE) {
            CPLDebug("OGRDB2TableLayer::UpdateFeature",
                     "Bind parameter failed");
            FreeBindBuffer(nBindNum, papBindBuffer);
            return OGRERR_FAILURE;
        }
        nBindNum++;
    }

    /* -------------------------------------------------------------------- */
    /*      Execute the update.                                             */
    /* -------------------------------------------------------------------- */
    if( !m_poPrepStmt->DB2Execute("OGR_DB2TableLayer::UpdateFeature") )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "Error updating feature with FID:" CPL_FRMT_GIB ", %s",
                  poFeature->GetFID(),
                  poDS->GetSession()->GetLastError() );

        return OGRERR_FAILURE;
    }

    FreeBindBuffer(nBindNum, papBindBuffer);

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

    if( pszFIDColumn == nullptr )
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
    OGRDB2Statement oStatement( poDS->GetSession() );

    oStatement.Appendf("DELETE FROM %s WHERE %s = " CPL_FRMT_GIB,
                       poFeatureDefn->GetName(), pszFIDColumn, nFID);
    if( !oStatement.DB2Execute("OGR_DB2TableLayer::DeleteFeature") )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "Attempt to delete feature with FID " CPL_FRMT_GIB
                  " failed. %s",
                  nFID, poDS->GetSession()->GetLastError() );
        return OGRERR_FAILURE;
    }
    return OGRERR_NONE;
}

/************************************************************************/
/*                          isFieldTypeSupported()                      */
/************************************************************************/

OGRErr OGRDB2TableLayer::isFieldTypeSupported( OGRFieldType nFieldType )

{
    switch(nFieldType) {
    case OFTInteger:
    case OFTReal:
    case OFTString:
    case OFTDateTime:
    case OFTInteger64:
        return OGRERR_NONE;
    default:
        return OGRERR_FAILURE;
    }
}

/************************************************************************/
/*                          PrepareFeature()                            */
/************************************************************************/

OGRErr OGRDB2TableLayer::PrepareFeature( OGRFeature *poFeature, char cType )

{
// LATER - this defeats the point of prepared statements but need to find
// some place to clean up to avoid reusing the wrong statement
    if (m_poPrepStmt) delete m_poPrepStmt;
    m_poPrepStmt =  new OGRDB2Statement( poDS->GetSession());

    char    *pszWKT = nullptr;
    CPLString osValues= " VALUES(";
    int nFieldCount = poFeatureDefn->GetFieldCount();

    if (cType == 'I')
        m_poPrepStmt->Appendf( "INSERT INTO %s.%s (",
                               pszSchemaName, pszTableName );
    else
        m_poPrepStmt->Appendf( "UPDATE %s.%s SET ",
                               pszSchemaName, pszTableName);
    int bNeedComma = FALSE;
    OGRGeometry *poGeom = poFeature->GetGeometryRef();

    if (poGeom != nullptr && pszGeomColumn != nullptr)
    {
        if( poGeom->exportToWkt( &pszWKT ) == OGRERR_NONE)
        {
            int nLen = (int) strlen(pszWKT);
            if (cType == 'I')
            {
                m_poPrepStmt->Append( pszGeomColumn );
                CPLString geomValue;
                geomValue.Printf( "DB2GSE.ST_%s(CAST( ? AS CLOB(2M)),%d)",
                                  poGeom->getGeometryName(), nSRSId );
                osValues.append(geomValue);
            } else {
                m_poPrepStmt->Appendf( "%s = "
                                    "DB2GSE.ST_%s(CAST( ? AS CLOB(%d)),%d)",
                                    pszGeomColumn, poGeom->getGeometryName(),
                                    nLen, nSRSId );
            }
            bNeedComma = TRUE;
        }
    }

// Explicitly add FID column and value if needed
    if( cType == 'I' && poFeature->GetFID() != OGRNullFID
            && pszFIDColumn != nullptr && cGenerated != 'A' )
    {
        if (bNeedComma)
        {
            m_poPrepStmt->Appendf( ", ");
            osValues.append(", ");
        }
        m_poPrepStmt->Appendf( "%s", pszFIDColumn );
        osValues.append("?");
        bNeedComma = TRUE;
    }

    for( int i = 0; i < nFieldCount; i++ )
    {

        if( !poFeature->IsFieldSetAndNotNull( i ) )
            continue;

        if (bNeedComma)
        {
            m_poPrepStmt->Appendf( ", ");
            osValues.append(", ");
        }
        bNeedComma = TRUE;
        if (cType == 'I') {
            m_poPrepStmt->Appendf( "%s",
                                poFeatureDefn->GetFieldDefn(i)->GetNameRef());
            osValues.append("?");
        } else {
            m_poPrepStmt->Appendf( "%s = ?",
                                poFeatureDefn->GetFieldDefn(i)->GetNameRef());
        }
    }
    if (cType == 'I') {
        m_poPrepStmt->Appendf( ") %s )", osValues.c_str() );
    } else {
        /* Add the WHERE clause */
        m_poPrepStmt->Appendf( " WHERE (%s) = " CPL_FRMT_GIB, pszFIDColumn,
                               poFeature->GetFID());
    }
    if (!m_poPrepStmt->DB2Prepare("OGR_DB2TableLayer::PrepareFeature"))
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "PREPARE command for feature failed. %s",
                  poDS->GetSession()->GetLastError() );
        return OGRERR_FAILURE;
    }

    return OGRERR_NONE;
}

/************************************************************************/
/*                          ICreateFeature()                            */
/************************************************************************/

OGRErr OGRDB2TableLayer::ICreateFeature( OGRFeature *poFeature )
{
    GetLayerDefn();

    if( nullptr == poFeature )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "NULL pointer to OGRFeature passed to CreateFeature()." );
        return OGRERR_FAILURE;
    }

    if (PrepareFeature(poFeature, 'I'))
        return OGRERR_FAILURE;

    char    *pszWKT = nullptr;
    int nFieldCount = poFeatureDefn->GetFieldCount();
    int nBindNum = 0;
    void** papBindBuffer = (void**)CPLMalloc(sizeof(void*)
                                                * (nFieldCount + 1));
    OGRGeometry *poGeom = poFeature->GetGeometryRef();

    if (poGeom != nullptr && pszGeomColumn != nullptr)
    {
        if( poGeom->exportToWkt( &pszWKT ) == OGRERR_NONE)
        {
            int nLen = (int) strlen(pszWKT);
            if (m_poPrepStmt->DB2BindParameterIn(
                        "OGRDB2TableLayer::ICreateFeature",
                        (nBindNum + 1),
                        SQL_C_CHAR,
                        SQL_LONGVARCHAR,
                        nLen,
                        (void *)(pszWKT)))
            {
                papBindBuffer[nBindNum] = pszWKT;
                nBindNum++;
            }
            else
            {
                CPLDebug("OGRDB2TableLayer::ICreateFeature",
                         "Bind parameter failed");
                FreeBindBuffer(nBindNum, papBindBuffer);
                return OGRERR_FAILURE;
            }
        }
    }

// Explicitly add FID column and value if needed
    if( poFeature->GetFID() != OGRNullFID
            && pszFIDColumn != nullptr && cGenerated != 'A' )
    {
        GIntBig nFID = poFeature->GetFID();
        if (m_poPrepStmt->DB2BindParameterIn(
                    "OGRDB2TableLayer::ICreateFeature",
                    (nBindNum + 1),
                    SQL_C_SBIGINT,
                    SQL_BIGINT,
                    sizeof(GIntBig),
                    (void *)(&nFID)))
        {
            papBindBuffer[nBindNum] = nullptr;
            nBindNum++;
        }
        else
        {
            CPLDebug("OGRDB2TableLayer::ICreateFeature",
                     "Bind parameter failed");
            FreeBindBuffer(nBindNum, papBindBuffer);
            return OGRERR_FAILURE;
        }
    }

    for( int i = 0; i < nFieldCount; i++ )
    {

        if( !poFeature->IsFieldSetAndNotNull( i ) )
            continue;

//        int nOGRFieldType = poFeatureDefn->GetFieldDefn(i)->GetType();
//        CPLDebug("OGRDB2TableLayer::ICreateFeature",
//               "i: %d; nOGRFieldType: %d",
//                i, nOGRFieldType);

        if (BindFieldValue(m_poPrepStmt,
                           poFeature, i,
                           nBindNum, papBindBuffer) != OGRERR_NONE) {
            CPLDebug("OGRDB2TableLayer::ICreateFeature",
                     "Bind parameter failed");
            FreeBindBuffer(nBindNum, papBindBuffer);
            return OGRERR_FAILURE;
        }
        nBindNum++;
    }

    poDS->getDTime();
    /* -------------------------------------------------------------------- */
    /*      Execute the insert.                                             */
    /* -------------------------------------------------------------------- */

    if (!m_poPrepStmt->DB2Execute("OGR_DB2TableLayer::ICreateFeature"))
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "INSERT command for new feature failed. %s",
                  poDS->GetSession()->GetLastError() );
        FreeBindBuffer(nBindNum, papBindBuffer);
        return OGRERR_FAILURE;
    }
    poDS->getDTime();

    if( bIsIdentityFid) {
        GIntBig oldFID = poFeature->GetFID();
        OGRDB2Statement oStatement2( poDS->GetSession() );
        oStatement2.Append( "select IDENTITY_VAL_LOCAL() AS IDENTITY "
                            "FROM SYSIBM.SYSDUMMY1");
        if( oStatement2.DB2Execute("OGR_DB2TableLayer::ICreateFeature")
                && oStatement2.Fetch() )
        {
            poFeature->SetFID( atoi(oStatement2.GetColData( 0 ) ));

            if ( oStatement2.GetColData( 0 ) )
            {
                poFeature->SetFID( atoi(oStatement2.GetColData( 0 ) ));
            }
        }
        CPLDebug("OGR_DB2TableLayer::ICreateFeature","Old FID: " CPL_FRMT_GIB
                 "; New FID: " CPL_FRMT_GIB, oldFID, poFeature->GetFID());
    }

    FreeBindBuffer(nBindNum, papBindBuffer);

    return OGRERR_NONE;
}

/************************************************************************/
/*                          FreeBindBuffer()                            */
/************************************************************************/

void OGRDB2TableLayer::FreeBindBuffer(int nBindNum, void **papBindBuffer)
{
    for( int i = 0; i < nBindNum; i++ ) {
        if (papBindBuffer[i] ) CPLFree(papBindBuffer[i]); // only free if set
    };
    CPLFree(papBindBuffer);
}

/************************************************************************/
/*                          BindFieldValue()                            */
/*                                                                      */
/* Used by CreateFeature() and SetFeature() to bind a                   */
/* non-empty field value                                                */
/************************************************************************/

OGRErr OGRDB2TableLayer::BindFieldValue(OGRDB2Statement * /*poStatement*/,
                                        OGRFeature* poFeature, int i,
                                        int nBindNum, void **papBindBuffer)
{
    int nOGRFieldType = poFeatureDefn->GetFieldDefn(i)->GetType();

    int nLen = 0;
    void * pValuePointer = nullptr;
    int nValueType = 0;
    int nParameterType = 0;

    if( nOGRFieldType == OFTString ) {
        const char* stringValue = poFeature->GetFieldAsString(i);
        papBindBuffer[nBindNum] = nullptr; // Don't free
        nLen = (int) strlen(stringValue);
        pValuePointer = (void *) stringValue;
        nValueType = SQL_C_CHAR;
        nParameterType = SQL_VARCHAR;
    }

    if ( nOGRFieldType == OFTReal ) {
        double *pnRealValue = (double *)CPLMalloc(sizeof(double));
        papBindBuffer[nBindNum] = pnRealValue;
        *pnRealValue = poFeature->GetFieldAsInteger(i);
        nLen = sizeof(double);
        pValuePointer = (void *) pnRealValue;
        nValueType = SQL_C_DOUBLE;
        nParameterType = SQL_DOUBLE;
    }

    if ( nOGRFieldType == OFTInteger ) {
        int *pnIntValue = (int *)CPLMalloc(sizeof(int));
        papBindBuffer[nBindNum] = pnIntValue;
        *pnIntValue = poFeature->GetFieldAsInteger(i);
        nLen = sizeof(int);
        pValuePointer = (void *) pnIntValue;
        nValueType = SQL_C_SLONG;
        nParameterType = SQL_INTEGER;
    }

    if ( nOGRFieldType == OFTInteger64 ) {
        GIntBig *pnLongValue = (GIntBig *)CPLMalloc(sizeof(GIntBig));
        papBindBuffer[nBindNum] = pnLongValue;
        *pnLongValue = poFeature->GetFieldAsInteger64(i);
        nLen = sizeof(GIntBig);
        pValuePointer = (void *) pnLongValue;
        nValueType = SQL_C_SBIGINT;
        nParameterType = SQL_BIGINT;
    }

    if (pValuePointer) {
        if (!m_poPrepStmt->DB2BindParameterIn(
                    "OGRDB2TableLayer::BindFieldValue",
                    (nBindNum + 1),
                    nValueType,
                    nParameterType,
                    nLen,
                    pValuePointer))
        {
            CPLDebug("OGRDB2TableLayer::BindFieldValue",
                     "Bind parameter failed");
            return OGRERR_FAILURE;
        }
    }
    return OGRERR_NONE;
}

#ifdef notdef
/************************************************************************/
/*                     CreateSpatialIndexIfNecessary()                  */
/************************************************************************/

void OGRDB2TableLayer::CreateSpatialIndexIfNecessary()
{
    if( bDeferredSpatialIndexCreation )
    {
        CreateSpatialIndex();
    }
}
#endif

/************************************************************************/
/*                      RunDeferredCreationIfNecessary()                */
/************************************************************************/

OGRErr OGRDB2TableLayer::RunDeferredCreationIfNecessary()
{
    CPLDebug("OGRDB2TableLayer::RunDeferredCreationIfNecessary","NO-OP");
#ifdef LATER
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
                 (pszDefault[0] == '('
                 && pszDefault[strlen(pszDefault)-1] == ')'
                 && (STARTS_WITH_CI(pszDefault+1, "strftime")
                 || STARTS_WITH_CI(pszDefault+1, " strftime")))) )
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
/* Make sure CURRENT_TIMESTAMP is translated into appropriate format */
/* for GeoPackage */
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
                 "(table_name,data_type,identifier,description,"
                 "last_change,srs_id)"
                 " VALUES "
                 "('%q','%q','%q','%q',strftime('%%Y-%%m-%%dT%%H:%%M:%%fZ',"
                 "CURRENT_TIMESTAMP),%d)",
                 pszLayerName, (bIsSpatial ? "features": "aspatial"),
                 pszIdentifier, pszDescription, m_iSrs);

    err = SQLCommand(m_poDS->GetDB(), pszSQL);
    sqlite3_free(pszSQL);
    if ( err != OGRERR_NONE )
        return OGRERR_FAILURE;

    ResetReading();
#endif
    return OGRERR_NONE;
}
