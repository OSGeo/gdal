/******************************************************************************
 * $Id$
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  Implements OGRIngresTableLayer class.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2008, Frank Warmerdam <warmerdam@pobox.com>
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
#include "ogr_ingres.h"

CPL_CVSID("$Id$");

/************************************************************************/
/*                         OGRIngresTableLayer()                         */
/************************************************************************/

OGRIngresTableLayer::OGRIngresTableLayer( OGRIngresDataSource *poDSIn, 
                                  const char * pszTableName,
                                  int bUpdate, int nSRSIdIn )

{
    poDS = poDSIn;

    pszQuery = NULL;
    pszWHERE = CPLStrdup( "" );
    pszQueryStatement = NULL;
    
    bUpdateAccess = bUpdate;

    iNextShapeId = 0;

    nSRSId = nSRSIdIn;

    poFeatureDefn = NULL;
    bLaunderColumnNames = TRUE;
}

/************************************************************************/
/*                        ~OGRIngresTableLayer()                         */
/************************************************************************/

OGRIngresTableLayer::~OGRIngresTableLayer()

{
    CPLFree( pszQuery );
    CPLFree( pszWHERE );
}


/************************************************************************/
/*                        Initialize()                                  */
/*                                                                      */
/*      Make sure we only do a ResetReading once we really have a       */
/*      FieldDefn.  Otherwise, we'll segfault.  After you construct     */
/*      the IngresTableLayer, make sure to do pLayer->Initialize()       */
/************************************************************************/

OGRErr  OGRIngresTableLayer::Initialize(const char * pszTableName)
{
    poFeatureDefn = ReadTableDefinition( pszTableName );   
    if (poFeatureDefn)
    {
        ResetReading();
        return OGRERR_NONE;
    }
    else
    {
        return OGRERR_FAILURE;
    }
}

/************************************************************************/
/*                        ReadTableDefinition()                         */
/*                                                                      */
/*      Build a schema from the named table.  Done by querying the      */
/*      catalog.                                                        */
/************************************************************************/

OGRFeatureDefn *OGRIngresTableLayer::ReadTableDefinition( const char *pszTable )

{
/* -------------------------------------------------------------------- */
/*      Fire off commands to get back the schema of the table.          */
/* -------------------------------------------------------------------- */
    CPLString osCommand;
    OGRIngresStatement oStatement( poDS->GetConn() );

    osCommand.Printf( "select column_name, column_datatype, column_length, column_scale from iicolumns where table_name = '%s'", 
                      pszTable );

    if( !oStatement.ExecuteSQL( osCommand ) )
    {
        return NULL;
    }

/* -------------------------------------------------------------------- */
/*      Parse the returned table information.                           */
/* -------------------------------------------------------------------- */
    OGRFeatureDefn *poDefn = new OGRFeatureDefn( pszTable );
    char           **papszRow;

    poDefn->Reference();

    while( (papszRow = oStatement.GetRow()) != NULL )
    {
        CPLString       osFieldName = papszRow[0];
        CPLString       osIngresType = papszRow[1];
        GInt32          nWidth, nScale;

        osIngresType.Trim();
        osFieldName.Trim();

        memcpy( &nWidth, papszRow[2], 4 );
        memcpy( &nScale, papszRow[3], 4 );

        OGRFieldDefn    oField( osFieldName, OFTString);

        if( EQUALN(osIngresType,"byte",4) 
            || EQUALN(osIngresType,"long byte",9) )
        {
            oField.SetType( OFTBinary );
        }
        else if( EQUALN(osIngresType,"varchar",7) 
                 || EQUAL(osIngresType,"text") 
                 || EQUALN(osIngresType,"long varchar",12) )
        {
            oField.SetType( OFTString );
        }
        else if( EQUALN(osIngresType,"char",4) || EQUAL(osIngresType,"c") )
        {
            oField.SetType( OFTString );
            oField.SetWidth( nWidth );
        }
        else if( EQUAL(osIngresType,"integer") )
        {
            oField.SetType( OFTInteger );
        }
        else if( EQUALN(osIngresType,"decimal", 7) )
        {
            if( nScale != 0 )
            {
                oField.SetType( OFTReal );
                oField.SetPrecision( nScale );
                oField.SetWidth( nWidth );
            }
            else
            {
                oField.SetType( OFTInteger );
                oField.SetWidth( nWidth );
            }
        }
        else if( EQUALN(osIngresType,"float", 5) )
        {
            oField.SetType( OFTReal );
        }
#ifdef notdef
        else if( EQUAL(osIngresType,"date") 
                 || EQUAL(osIngresType,"ansidate") 
                 || EQUAL(osIngresType,"ingresdate") )
        {
            oField.SetType( OFTDate );
        }
#endif

#ifdef notdef
        // Is this an integer primary key field?
        if( !bHasFid && papszRow[3] != NULL && EQUAL(papszRow[3],"PRI") 
            && oField.GetType() == OFTInteger )
        {
            bHasFid = TRUE;
            pszFIDColumn = CPLStrdup(oField.GetNameRef());
            continue;
        }
#endif
        poDefn->AddFieldDefn( &oField );
    }

    // set to none for now... if we have a geometry column it will be set layer.
    poDefn->SetGeomType( wkbNone );

    if( bHasFid )
        CPLDebug( "Ingres", "table %s has FID column %s.",
                  pszTable, pszFIDColumn );
    else
        CPLDebug( "Ingres", 
                  "table %s has no FID column, FIDs will not be reliable!",
                  pszTable );
#ifdef notdef
    if (pszGeomColumn) 
    {
        char*        pszType=NULL;
        
        // set to unknown first
        poDefn->SetGeomType( wkbUnknown );
        
        sprintf(szCommand, "SELECT type, coord_dimension FROM geometry_columns WHERE f_table_name='%s'",
                pszTable );
        
        hResult = NULL;
        if( !ingres_query( poDS->GetConn(), szCommand ) )
            hResult = ingres_store_result( poDS->GetConn() );

        papszRow = NULL;
        if( hResult != NULL )
            papszRow = ingres_fetch_row( hResult );

        if( papszRow != NULL && papszRow[0] != NULL )
        {

            pszType = papszRow[0];

            OGRwkbGeometryType nGeomType = wkbUnknown;

            // check only standard OGC geometry types
            if ( EQUAL(pszType, "POINT") )
                nGeomType = wkbPoint;
            else if ( EQUAL(pszType,"LINESTRING"))
                nGeomType = wkbLineString;
            else if ( EQUAL(pszType,"POLYGON"))
                nGeomType = wkbPolygon;
            else if ( EQUAL(pszType,"MULTIPOINT"))
                nGeomType = wkbMultiPoint;
            else if ( EQUAL(pszType,"MULTILINESTRING"))
                nGeomType = wkbMultiLineString;
            else if ( EQUAL(pszType,"MULTIPOLYGON"))
                nGeomType = wkbMultiPolygon;
            else if ( EQUAL(pszType,"GEOMETRYCOLLECTION"))
                nGeomType = wkbGeometryCollection;

            if( papszRow[1] != NULL && atoi(papszRow[1]) == 3 )
                nGeomType = (OGRwkbGeometryType) (nGeomType | wkb25DBit);

            poDefn->SetGeomType( nGeomType );

        } 

        if( hResult != NULL )
            ingres_free_result( hResult );   //Free our query results for finding type.
			hResult = NULL;
    } 
#endif 
    // Fetch the SRID for this table now
    nSRSId = FetchSRSId(); 
    return poDefn;
}

/************************************************************************/
/*                          SetSpatialFilter()                          */
/************************************************************************/

void OGRIngresTableLayer::SetSpatialFilter( OGRGeometry * poGeomIn )

{
    if( !InstallFilter( poGeomIn ) )
        return;

    BuildWhere();

    ResetReading();
}



/************************************************************************/
/*                             BuildWhere()                             */
/*                                                                      */
/*      Build the WHERE statement appropriate to the current set of     */
/*      criteria (spatial and attribute queries).                       */
/************************************************************************/

void OGRIngresTableLayer::BuildWhere()

{
    char        szWHERE[4096];
    
    CPLFree( pszWHERE );
    pszWHERE = NULL;

    szWHERE[0] = '\0';

#ifdef notdef
    if( m_poFilterGeom != NULL && pszGeomColumn )
    {
        char szEnvelope[4096];
        OGREnvelope  sEnvelope;
        szEnvelope[0] = '\0';
        
        //POLYGON((MINX MINY, MAXX MINY, MAXX MAXY, MINX MAXY, MINX MINY))
        m_poFilterGeom->getEnvelope( &sEnvelope );
        
        sprintf(szEnvelope,
                "POLYGON((%.12f %.12f, %.12f %.12f, %.12f %.12f, %.12f %.12f, %.12f %.12f))",
                sEnvelope.MinX, sEnvelope.MinY,
                sEnvelope.MaxX, sEnvelope.MinY,
                sEnvelope.MaxX, sEnvelope.MaxY,
                sEnvelope.MinX, sEnvelope.MaxY,
                sEnvelope.MinX, sEnvelope.MinY);

        sprintf( szWHERE,
                 "WHERE MBRIntersects(GeomFromText('%s'), %s)",
                 szEnvelope,
                 pszGeomColumn);

    }
#endif

    if( pszQuery != NULL )
    {
        if( strlen(szWHERE) == 0 )
            sprintf( szWHERE, "WHERE %s ", pszQuery  );
        else
            sprintf( szWHERE+strlen(szWHERE), "&& %s ", pszQuery );
    }

    pszWHERE = CPLStrdup(szWHERE);
}

/************************************************************************/
/*                      BuildFullQueryStatement()                       */
/************************************************************************/

void OGRIngresTableLayer::BuildFullQueryStatement()

{
    if( pszQueryStatement != NULL )
    {
        CPLFree( pszQueryStatement );
        pszQueryStatement = NULL;
    }

    char *pszFields = BuildFields();

    pszQueryStatement = (char *) 
        CPLMalloc(strlen(pszFields)+strlen(pszWHERE)
                  +strlen(poFeatureDefn->GetName()) + 40);
    sprintf( pszQueryStatement,
             "SELECT %s FROM %s %s", 
             pszFields, poFeatureDefn->GetName(), pszWHERE );
    
    CPLFree( pszFields );
}

/************************************************************************/
/*                            ResetReading()                            */
/************************************************************************/

void OGRIngresTableLayer::ResetReading()

{
    BuildFullQueryStatement();

    OGRIngresLayer::ResetReading();
}

/************************************************************************/
/*                            BuildFields()                             */
/*                                                                      */
/*      Build list of fields to fetch, performing any required          */
/*      transformations (such as on geometry).                          */
/************************************************************************/

char *OGRIngresTableLayer::BuildFields()

{
    int         i, nSize;
    char        *pszFieldList;

    nSize = 25;
    if( pszGeomColumn )
        nSize += strlen(pszGeomColumn);

    if( bHasFid )
        nSize += strlen(pszFIDColumn);
        

    for( i = 0; i < poFeatureDefn->GetFieldCount(); i++ )
        nSize += strlen(poFeatureDefn->GetFieldDefn(i)->GetNameRef()) + 4;

    pszFieldList = (char *) CPLMalloc(nSize);
    pszFieldList[0] = '\0';

    if( bHasFid && poFeatureDefn->GetFieldIndex( pszFIDColumn ) == -1 )
        sprintf( pszFieldList, "%s", pszFIDColumn );

    if( pszGeomColumn )
    {
        if( strlen(pszFieldList) > 0 )
            strcat( pszFieldList, ", " );

        sprintf( pszFieldList+strlen(pszFieldList), 
                 "%s %s", pszGeomColumn, pszGeomColumn );
    }

    for( i = 0; i < poFeatureDefn->GetFieldCount(); i++ )
    {
        const char *pszName = poFeatureDefn->GetFieldDefn(i)->GetNameRef();

        if( strlen(pszFieldList) > 0 )
            strcat( pszFieldList, ", " );

        strcat( pszFieldList, pszName );
    }

    CPLAssert( (int) strlen(pszFieldList) < nSize );

    return pszFieldList;
}

/************************************************************************/
/*                         SetAttributeFilter()                         */
/************************************************************************/

OGRErr OGRIngresTableLayer::SetAttributeFilter( const char *pszQuery )

{
    CPLFree( this->pszQuery );

    if( pszQuery == NULL || strlen(pszQuery) == 0 )
        this->pszQuery = NULL;
    else
        this->pszQuery = CPLStrdup( pszQuery );

    BuildWhere();

    ResetReading();

    return OGRERR_NONE;
}

/************************************************************************/
/*                           TestCapability()                           */
/************************************************************************/

int OGRIngresTableLayer::TestCapability( const char * pszCap )

{
    if( EQUAL(pszCap,OLCRandomRead) )
        return bHasFid;

    else if( EQUAL(pszCap,OLCFastFeatureCount) )
        return TRUE;

    else 
        return OGRIngresLayer::TestCapability( pszCap );
}

/************************************************************************/
/*                             SetFeature()                             */
/*                                                                      */
/*      SetFeature() is implemented by dropping the old copy of the     */
/*      feature in question (if there is one) and then creating a       */
/*      new one with the provided feature id.                           */
/************************************************************************/

#ifdef notdef
OGRErr OGRIngresTableLayer::SetFeature( OGRFeature *poFeature )

{
    OGRErr eErr;

    if( poFeature->GetFID() == OGRNullFID )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "FID required on features given to SetFeature()." );
        return OGRERR_FAILURE;
    }

    eErr = DeleteFeature( poFeature->GetFID() );
    if( eErr != OGRERR_NONE )
        return eErr;

    return CreateFeature( poFeature );
}

/************************************************************************/
/*                           DeleteFeature()                            */
/************************************************************************/

OGRErr OGRIngresTableLayer::DeleteFeature( long nFID )

{
    INGRES_RES           *hResult=NULL;
    CPLString           osCommand;


/* -------------------------------------------------------------------- */
/*      We can only delete features if we have a well defined FID       */
/*      column to target.                                               */
/* -------------------------------------------------------------------- */
    if( !bHasFid )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "DeleteFeature(%d) failed.  Unable to delete features "
                  "in tables without\n a recognised FID column.",
                  nFID );
        return OGRERR_FAILURE;

    }

/* -------------------------------------------------------------------- */
/*      Form the statement to drop the record.                          */
/* -------------------------------------------------------------------- */
    osCommand.Printf( "DELETE FROM %s WHERE %s = %ld",
                      poFeatureDefn->GetName(), pszFIDColumn, nFID );
                      
/* -------------------------------------------------------------------- */
/*      Execute the delete.                                             */
/* -------------------------------------------------------------------- */
    poDS->InterruptLongResult();
    if( ingres_query(poDS->GetConn(), osCommand.c_str() ) ){   
        poDS->ReportError(  osCommand.c_str() );
        return OGRERR_FAILURE;   
    }

    // make sure to attempt to free results of successful queries
    hResult = ingres_store_result( poDS->GetConn() );
    if( hResult != NULL )
        ingres_free_result( hResult );
    hResult = NULL;
    
    return OGRERR_NONE;
}


/************************************************************************/
/*                       CreateFeature()                                */
/************************************************************************/

OGRErr OGRIngresTableLayer::CreateFeature( OGRFeature *poFeature )

{
    INGRES_RES           *hResult=NULL;
    CPLString           osCommand;
    int                 i, bNeedComma = FALSE;

/* -------------------------------------------------------------------- */
/*      Form the INSERT command.                                        */
/* -------------------------------------------------------------------- */
    osCommand.Printf( "INSERT INTO %s (", poFeatureDefn->GetName() );

    if( poFeature->GetGeometryRef() != NULL )
    {
        osCommand = osCommand + pszGeomColumn + " ";
        bNeedComma = TRUE;
    }

    if( poFeature->GetFID() != OGRNullFID && pszFIDColumn != NULL )
    {
        if( bNeedComma )
            osCommand += ", ";
        
        osCommand = osCommand + pszFIDColumn + " ";
        bNeedComma = TRUE;
    }

    for( i = 0; i < poFeatureDefn->GetFieldCount(); i++ )
    {
        if( !poFeature->IsFieldSet( i ) )
            continue;

        if( !bNeedComma )
            bNeedComma = TRUE;
        else
            osCommand += ", ";

        osCommand = osCommand 
             + poFeatureDefn->GetFieldDefn(i)->GetNameRef();
    }

    osCommand += ") VALUES (";

    // Set the geometry 
    bNeedComma = poFeature->GetGeometryRef() != NULL;
    if( poFeature->GetGeometryRef() != NULL)
    {
        char    *pszWKT = NULL;

        if( poFeature->GetGeometryRef() != NULL )
        {
            OGRGeometry *poGeom = (OGRGeometry *) poFeature->GetGeometryRef();
            
            poGeom->closeRings();
            poGeom->flattenTo2D();
            poGeom->exportToWkt( &pszWKT );
        }

        if( pszWKT != NULL )
        {

            osCommand += 
                CPLString().Printf(
                    "GeometryFromText('%s',%d) ", pszWKT, nSRSId );

            OGRFree( pszWKT );
        }
        else
            osCommand += "''";
    }


    // Set the FID 
    if( poFeature->GetFID() != OGRNullFID && pszFIDColumn != NULL )
    {
        if( bNeedComma )
            osCommand += ", ";
        osCommand += CPLString().Printf( "%ld ", poFeature->GetFID() );
        bNeedComma = TRUE;
    }

    for( i = 0; i < poFeatureDefn->GetFieldCount(); i++ )
    {
        if( !poFeature->IsFieldSet( i ) )
            continue;

        if( bNeedComma )
            osCommand += ", ";
        else
            bNeedComma = TRUE;

        const char *pszStrValue = poFeature->GetFieldAsString(i);

        if( poFeatureDefn->GetFieldDefn(i)->GetType() != OFTInteger
                 && poFeatureDefn->GetFieldDefn(i)->GetType() != OFTReal
                 && poFeatureDefn->GetFieldDefn(i)->GetType() != OFTBinary )
        {
            int         iChar;

            //We need to quote and escape string fields. 
            osCommand += "'";

            for( iChar = 0; pszStrValue[iChar] != '\0'; iChar++ )
            {
                if( poFeatureDefn->GetFieldDefn(i)->GetType() != OFTIntegerList
                    && poFeatureDefn->GetFieldDefn(i)->GetType() != OFTRealList
                    && poFeatureDefn->GetFieldDefn(i)->GetWidth() > 0
                    && iChar == poFeatureDefn->GetFieldDefn(i)->GetWidth() )
                {
                    CPLDebug( "INGRES",
                              "Truncated %s field value, it was too long.",
                              poFeatureDefn->GetFieldDefn(i)->GetNameRef() );
                    break;
                }

                if( pszStrValue[iChar] == '\\'
                    || pszStrValue[iChar] == '\'' )
                {
                    osCommand += '\\';
                    osCommand += pszStrValue[iChar];
                }
                else
                    osCommand += pszStrValue[iChar];
            }

            osCommand += "'";
        }
        else if( poFeatureDefn->GetFieldDefn(i)->GetType() == OFTBinary )
        {
            int binaryCount = 0;
            GByte* binaryData = poFeature->GetFieldAsBinary(i, &binaryCount);
            char* pszHexValue = CPLBinaryToHex( binaryCount, binaryData );

            osCommand += "x'";
            osCommand += pszHexValue;
            osCommand += "'";

            CPLFree( pszHexValue );
        }
        else
        {
            osCommand += pszStrValue;
        }

    }

    osCommand += ")";
    
    int nQueryResult = ingres_query(poDS->GetConn(), osCommand.c_str() );
    const my_ulonglong nFID = ingres_insert_id( poDS->GetConn() );
    
    if( nQueryResult ){   
        int eErrorCode = ingres_errno(poDS->GetConn());
        if (eErrorCode == 1153) {//ER_NET_PACKET_TOO_LARGE)
            poDS->ReportError("CreateFeature failed because the Ingres server " \
                              "cannot read the entire query statement.  Increase " \
                              "the size of statements your server will allow by " \
                              "altering the 'max_allowed_packet' parameter in "\
                              "your Ingres server configuration.");
        }
        else
        {
        CPLDebug("INGRES","Error number %d", eErrorCode);
            poDS->ReportError(  osCommand.c_str() );
        }

        // make sure to attempt to free results
        hResult = ingres_store_result( poDS->GetConn() );
        if( hResult != NULL )
            ingres_free_result( hResult );
        hResult = NULL;
            
        return OGRERR_FAILURE;   
    }

    if( nFID > 0 ) {
        poFeature->SetFID( nFID );
    }

    // make sure to attempt to free results of successful queries
    hResult = ingres_store_result( poDS->GetConn() );
    if( hResult != NULL )
        ingres_free_result( hResult );
    hResult = NULL;
    
    return OGRERR_NONE;

}
/************************************************************************/
/*                            CreateField()                             */
/************************************************************************/

OGRErr OGRIngresTableLayer::CreateField( OGRFieldDefn *poFieldIn, int bApproxOK )

{

    INGRES_RES           *hResult=NULL;
    char        		szCommand[1024];
    
    char                szFieldType[256];
    OGRFieldDefn        oField( poFieldIn );

/* -------------------------------------------------------------------- */
/*      Do we want to "launder" the column names into Postgres          */
/*      friendly format?                                                */
/* -------------------------------------------------------------------- */
    if( bLaunderColumnNames )
    {
        char    *pszSafeName = poDS->LaunderName( oField.GetNameRef() );

        oField.SetName( pszSafeName );
        CPLFree( pszSafeName );

    }

/* -------------------------------------------------------------------- */
/*      Work out the Ingres type.                                        */
/* -------------------------------------------------------------------- */
    if( oField.GetType() == OFTInteger )
    {
        if( oField.GetWidth() > 0 && bPreservePrecision )
            sprintf( szFieldType, "DECIMAL(%d,0)", oField.GetWidth() );
        else
            strcpy( szFieldType, "INTEGER" );
    }
    else if( oField.GetType() == OFTReal )
    {
        if( oField.GetWidth() > 0 && oField.GetPrecision() > 0
            && bPreservePrecision )
            sprintf( szFieldType, "DOUBLE(%d,%d)",
                     oField.GetWidth(), oField.GetPrecision() );
        else
            strcpy( szFieldType, "DOUBLE" );
    }

    else if( oField.GetType() == OFTDate )
    {
        sprintf( szFieldType, "DATE" );
    }

    else if( oField.GetType() == OFTDateTime )
    {
        sprintf( szFieldType, "DATETIME" );
    }

    else if( oField.GetType() == OFTTime )
    {
        sprintf( szFieldType, "TIME" );
    }

    else if( oField.GetType() == OFTBinary )
    {
        sprintf( szFieldType, "LONGBLOB" );
    }

    else if( oField.GetType() == OFTString )
    {
        if( oField.GetWidth() == 0 || !bPreservePrecision )
            strcpy( szFieldType, "TEXT" );
        else
            sprintf( szFieldType, "VARCHAR(%d)", oField.GetWidth() );
    }
    else if( bApproxOK )
    {
        CPLError( CE_Warning, CPLE_NotSupported,
                  "Can't create field %s with type %s on Ingres layers.  Creating as TEXT.",
                  oField.GetNameRef(),
                  OGRFieldDefn::GetFieldTypeName(oField.GetType()) );
        strcpy( szFieldType, "TEXT" );
    }
    else
    {
        CPLError( CE_Failure, CPLE_NotSupported,
                  "Can't create field %s with type %s on Ingres layers.",
                  oField.GetNameRef(),
                  OGRFieldDefn::GetFieldTypeName(oField.GetType()) );

        return OGRERR_FAILURE;
    }

    sprintf( szCommand,
             "ALTER TABLE %s ADD COLUMN %s %s",
             poFeatureDefn->GetName(), oField.GetNameRef(), szFieldType );

    if( ingres_query(poDS->GetConn(), szCommand ) )
    {
        poDS->ReportError( szCommand );
        return OGRERR_FAILURE;
    }

    // make sure to attempt to free results of successful queries
    hResult = ingres_store_result( poDS->GetConn() );
    if( hResult != NULL )
        ingres_free_result( hResult );
    hResult = NULL;   

    poFeatureDefn->AddFieldDefn( &oField );    
    
    return OGRERR_NONE;
}
#endif


/************************************************************************/
/*                             GetFeature()                             */
/************************************************************************/
#ifdef notdef
OGRFeature *OGRIngresTableLayer::GetFeature( long nFeatureId )

{
    if( pszFIDColumn == NULL )
        return OGRIngresLayer::GetFeature( nFeatureId );

/* -------------------------------------------------------------------- */
/*      Discard any existing resultset.                                 */
/* -------------------------------------------------------------------- */
    ResetReading();

/* -------------------------------------------------------------------- */
/*      Prepare query command that will just fetch the one record of    */
/*      interest.                                                       */
/* -------------------------------------------------------------------- */
    char        *pszFieldList = BuildFields();
    char        *pszCommand = (char *) CPLMalloc(strlen(pszFieldList)+2000);

    sprintf( pszCommand, 
             "SELECT %s FROM %s WHERE %s = %ld", 
             pszFieldList, poFeatureDefn->GetName(), pszFIDColumn, 
             nFeatureId );
    CPLFree( pszFieldList );

/* -------------------------------------------------------------------- */
/*      Issue the command.                                              */
/* -------------------------------------------------------------------- */
    if( ingres_query( poDS->GetConn(), pszCommand ) )
    {
        poDS->ReportError( pszCommand );
        return NULL;
    }
    CPLFree( pszCommand );

    hResultSet = ingres_store_result( poDS->GetConn() );
    if( hResultSet == NULL )
    {
        poDS->ReportError( "ingres_store_result() failed on query." );
        return NULL;
    }

/* -------------------------------------------------------------------- */
/*      Fetch the result record.                                        */
/* -------------------------------------------------------------------- */
    char **papszRow;
    unsigned long *panLengths;

    papszRow = ingres_fetch_row( hResultSet );
    if( papszRow == NULL )
        return NULL;

    panLengths = ingres_fetch_lengths( hResultSet );

/* -------------------------------------------------------------------- */
/*      Transform into a feature.                                       */
/* -------------------------------------------------------------------- */
    iNextShapeId = nFeatureId;

    OGRFeature *poFeature = RecordToFeature( papszRow, panLengths );

    iNextShapeId = 0;

/* -------------------------------------------------------------------- */
/*      Cleanup                                                         */
/* -------------------------------------------------------------------- */
    if( hResultSet != NULL )
        ingres_free_result( hResultSet );
 		hResultSet = NULL;

    return poFeature;
}
#endif

/************************************************************************/
/*                          GetFeatureCount()                           */
/*                                                                      */
/*      If a spatial filter is in effect, we turn control over to       */
/*      the generic counter.  Otherwise we return the total count.      */
/*      Eventually we should consider implementing a more efficient     */
/*      way of counting features matching a spatial query.              */
/************************************************************************/

#ifdef notdef
int OGRIngresTableLayer::GetFeatureCount( int bForce )

{
/* -------------------------------------------------------------------- */
/*      Ensure any active long result is interrupted.                   */
/* -------------------------------------------------------------------- */
    poDS->InterruptLongResult();
    
/* -------------------------------------------------------------------- */
/*      Issue the appropriate select command.                           */
/* -------------------------------------------------------------------- */
    INGRES_RES    *hResult;
    const char         *pszCommand;

    pszCommand = CPLSPrintf( "SELECT COUNT(*) FROM %s %s", 
                             poFeatureDefn->GetName(), pszWHERE );

    if( ingres_query( poDS->GetConn(), pszCommand ) )
    {
        poDS->ReportError( pszCommand );
        return FALSE;
    }

    hResult = ingres_store_result( poDS->GetConn() );
    if( hResult == NULL )
    {
        poDS->ReportError( "ingres_store_result() failed on SELECT COUNT(*)." );
        return FALSE;
    }
    
/* -------------------------------------------------------------------- */
/*      Capture the result.                                             */
/* -------------------------------------------------------------------- */
    char **papszRow = ingres_fetch_row( hResult );
    int nCount = 0;

    if( papszRow != NULL && papszRow[0] != NULL )
        nCount = atoi(papszRow[0]);

    if( hResultSet != NULL )
        ingres_free_result( hResultSet );
 		hResultSet = NULL;
    
    return nCount;
}
#endif

/************************************************************************/
/*                          GetExtent()					*/
/*                                                                      */
/*      Retrieve the MBR of the Ingres table.  This should be made more  */
/*      in the future when Ingres adds support for a single MBR query    */
/*      like PostgreSQL.						*/
/************************************************************************/
#ifdef notdef
OGRErr OGRIngresTableLayer::GetExtent(OGREnvelope *psExtent, int bForce )

{
	if( GetLayerDefn()->GetGeomType() == wkbNone )
    {
        psExtent->MinX = 0.0;
        psExtent->MaxX = 0.0;
        psExtent->MinY = 0.0;
        psExtent->MaxY = 0.0;
        
        return OGRERR_FAILURE;
    }

	OGREnvelope oEnv;
	CPLString   osCommand;
	GBool       bExtentSet = FALSE;

	osCommand.Printf( "SELECT Envelope(%s) FROM %s;", pszGeomColumn, pszGeomColumnTable);

	if (ingres_query(poDS->GetConn(), osCommand) == 0)
	{
		INGRES_RES* result = ingres_use_result(poDS->GetConn());
		if ( result == NULL )
        {
            poDS->ReportError( "ingres_use_result() failed on extents query." );
            return OGRERR_FAILURE;
        }

		INGRES_ROW row; 
		unsigned long *panLengths = NULL;
		while ((row = ingres_fetch_row(result)))
		{
			if (panLengths == NULL)
			{
				panLengths = ingres_fetch_lengths( result );
				if ( panLengths == NULL )
				{
					poDS->ReportError( "ingres_fetch_lengths() failed on extents query." );
					return OGRERR_FAILURE;
				}
			}

			OGRGeometry *poGeometry = NULL;
			// Geometry columns will have the first 4 bytes contain the SRID.
			OGRGeometryFactory::createFromWkb(((GByte *)row[0]) + 4, 
											  NULL,
											  &poGeometry,
											  panLengths[0] - 4 );

			if ( poGeometry != NULL )
			{
				if (poGeometry && !bExtentSet)
				{
					poGeometry->getEnvelope(psExtent);
					bExtentSet = TRUE;
				}
				else if (poGeometry)
				{
					poGeometry->getEnvelope(&oEnv);
					if (oEnv.MinX < psExtent->MinX) 
						psExtent->MinX = oEnv.MinX;
					if (oEnv.MinY < psExtent->MinY) 
						psExtent->MinY = oEnv.MinY;
					if (oEnv.MaxX > psExtent->MaxX) 
						psExtent->MaxX = oEnv.MaxX;
					if (oEnv.MaxY > psExtent->MaxY) 
						psExtent->MaxY = oEnv.MaxY;
				}
				delete poGeometry;
			}
		}

		ingres_free_result(result);      
	}

	return (bExtentSet ? OGRERR_NONE : OGRERR_FAILURE);
}
#endif

