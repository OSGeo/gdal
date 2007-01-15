/******************************************************************************
 * $Id$
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  Implements OGRMySQLTableLayer class.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 * Author:   Howard Butler, hobu@hobu.net
 *
 ******************************************************************************
 * Copyright (c) 2004, Frank Warmerdam <warmerdam@pobox.com>
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
 ****************************************************************************/

#include "cpl_conv.h"
#include "cpl_string.h"
#include "ogr_mysql.h"

CPL_CVSID("$Id$");

/************************************************************************/
/*                         OGRMySQLTableLayer()                         */
/************************************************************************/

OGRMySQLTableLayer::OGRMySQLTableLayer( OGRMySQLDataSource *poDSIn, 
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
/*                        ~OGRMySQLTableLayer()                         */
/************************************************************************/

OGRMySQLTableLayer::~OGRMySQLTableLayer()

{
    CPLFree( pszQuery );
    CPLFree( pszWHERE );
}


/************************************************************************/
/*                        Initialize()                                  */
/*                                                                      */
/*      Make sure we only do a ResetReading once we really have a       */
/*      FieldDefn.  Otherwise, we'll segfault.  After you construct     */
/*      the MySQLTableLayer, make sure to do pLayer->Initialize()       */
/************************************************************************/

OGRErr  OGRMySQLTableLayer::Initialize(const char * pszTableName)
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

OGRFeatureDefn *OGRMySQLTableLayer::ReadTableDefinition( const char *pszTable )

{
    MYSQL_RES    *hResult;
    char         szCommand[1024];
    
/* -------------------------------------------------------------------- */
/*      Fire off commands to get back the schema of the table.          */
/* -------------------------------------------------------------------- */
    sprintf( szCommand, "DESCRIBE %s", pszTable );
    pszGeomColumnTable = CPLStrdup(pszTable);
    if( mysql_query( poDS->GetConn(), szCommand ) )
    {
        poDS->ReportError( "DESCRIBE Failed" );
        return FALSE;
    }

    hResult = mysql_store_result( poDS->GetConn() );
    if( hResult == NULL )
    {
        poDS->ReportError( "mysql_store_result() failed on DESCRIBE result." );
        return FALSE;
    }

/* -------------------------------------------------------------------- */
/*      Parse the returned table information.                           */
/* -------------------------------------------------------------------- */
    OGRFeatureDefn *poDefn = new OGRFeatureDefn( pszTable );
    char           **papszRow;

    poDefn->Reference();

    while( (papszRow = mysql_fetch_row( hResult )) != NULL )
    {
        const char      *pszType;
        OGRFieldDefn    oField( papszRow[0], OFTString);

        pszType = papszRow[1];

        if( pszType == NULL )
            continue;

        if( EQUAL(pszType,"varbinary")
            || (strlen(pszType)>3 && EQUAL(pszType+strlen(pszType)-4,"blob")))
        {
            oField.SetType( OFTBinary );
        }
        else if( EQUAL(pszType,"varchar") 
                 || EQUAL(pszType+strlen(pszType)-4,"enum") 
                 || EQUAL(pszType+strlen(pszType)-4,"set") )
        {
            oField.SetType( OFTString );

        }
        else if( EQUALN(pszType,"char",4)  )
        {
            oField.SetType( OFTString );
            char ** papszTokens;

            papszTokens = CSLTokenizeString2(pszType,"(),",0);
            
            /* width is the second */
            oField.SetWidth(atoi(papszTokens[1]));

            CSLDestroy( papszTokens );
            oField.SetType( OFTString );

        }
        
        if(strlen(pszType)>3 && EQUAL(pszType+strlen(pszType)-4,"text"))
        {
            oField.SetType( OFTString );            
        }
        else if( EQUALN(pszType,"varchar",6)  )
        {
            /* 
               pszType is usually in the form "varchar(15)" 
               so we'll split it up and get the width and precision
            */
            
            oField.SetType( OFTString );
            char ** papszTokens;

            papszTokens = CSLTokenizeString2(pszType,"(),",0);
            
            /* width is the second */
            oField.SetWidth(atoi(papszTokens[1]));

            CSLDestroy( papszTokens );
            oField.SetType( OFTString );
        }
        else if( EQUALN(pszType,"int", 3) )
        {
            oField.SetType( OFTInteger );
        }
        else if( EQUALN(pszType,"tinyint", 7) )
        {
            oField.SetType( OFTInteger );
        }
        else if( EQUALN(pszType,"smallint", 8) )
        {
            oField.SetType( OFTInteger );
        }
        else if( EQUALN(pszType,"mediumint",9) )
        {
            oField.SetType( OFTInteger );
        }
        else if( EQUALN(pszType,"bigint",6) )
        {
            oField.SetType( OFTInteger );
        }
        else if( EQUALN(pszType,"decimal",7) )
        {
            /* 
               pszType is usually in the form "decimal(15,2)" 
               so we'll split it up and get the width and precision
            */
            oField.SetType( OFTReal );
            char ** papszTokens;

            papszTokens = CSLTokenizeString2(pszType,"(),",0);
            
            /* width is the second and precision is the third */
            oField.SetWidth(atoi(papszTokens[1]));
            oField.SetPrecision(atoi(papszTokens[2]));
            CSLDestroy( papszTokens );


        }
        else if( EQUALN(pszType,"float", 5) )
        {
            oField.SetType( OFTReal );
        }
        else if( EQUAL(pszType,"double") )
        {
            oField.SetType( OFTReal );
        }
        else if( EQUALN(pszType,"double",6) )
        {
            // double can also be double(15,2)
            // so we'll handle this case here after 
            // we check for just a regular double 
            // without a width and precision specified
            
            char ** papszTokens=NULL;
            papszTokens = CSLTokenizeString2(pszType,"(),",0);
            /* width is the second and precision is the third */
            oField.SetWidth(atoi(papszTokens[1]));
            oField.SetPrecision(atoi(papszTokens[2]));
            CSLDestroy( papszTokens );  

            oField.SetType( OFTReal );
        }
        else if( EQUAL(pszType,"decimal") )
        {
            oField.SetType( OFTReal );
        }
        else if( EQUAL(pszType, "date") )
        {
            oField.SetType( OFTDate );
        }
        else if( EQUAL(pszType, "time") )
        {
            oField.SetType( OFTTime );
        }
        else if( EQUAL(pszType, "datetime") 
                 || EQUAL(pszType, "timestamp") )
        {
            oField.SetType( OFTDateTime );
        }
        else if( EQUAL(pszType, "year") )  
        {
            oField.SetType( OFTString );
            oField.SetWidth( 10 );
        }
        else if( EQUAL(pszType, "geometry") ) 
        {
            pszGeomColumn = CPLStrdup(papszRow[0]);
            continue;
        }
        // Is this an integer primary key field?
        if( !bHasFid && papszRow[3] != NULL && EQUAL(papszRow[3],"PRI") 
            && oField.GetType() == OFTInteger )
        {
            bHasFid = TRUE;
            pszFIDColumn = CPLStrdup(oField.GetNameRef());
            continue;
        }

        poDefn->AddFieldDefn( &oField );
    }

    // set to none for now... if we have a geometry column it will be set layer.
    poDefn->SetGeomType( wkbNone );

    if( hResult != NULL )
    {
        mysql_free_result( hResult );
        hResultSet = NULL;
    }

    if( bHasFid )
        CPLDebug( "MySQL", "table %s has FID column %s.",
                  pszTable, pszFIDColumn );
    else
        CPLDebug( "MySQL", 
                  "table %s has no FID column, FIDs will not be reliable!",
                  pszTable );

    if (pszGeomColumn) 
    {
        char*        pszType=NULL;
        
        // set to unknown first
        poDefn->SetGeomType( wkbUnknown );
        
        sprintf(szCommand, "SELECT type, coord_dimension FROM geometry_columns WHERE f_table_name='%s'",
                pszTable );
        
        hResult = NULL;
        if( !mysql_query( poDS->GetConn(), szCommand ) )
            hResult = mysql_store_result( poDS->GetConn() );

        papszRow = NULL;
        if( hResult != NULL )
            papszRow = mysql_fetch_row( hResult );

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
            mysql_free_result( hResult );   //Free our query results for finding type.
			hResult = NULL;
    } 
 
    // Fetch the SRID for this table now
    nSRSId = FetchSRSId(); 
    return poDefn;
}

/************************************************************************/
/*                          SetSpatialFilter()                          */
/************************************************************************/

void OGRMySQLTableLayer::SetSpatialFilter( OGRGeometry * poGeomIn )

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

void OGRMySQLTableLayer::BuildWhere()

{
    char        szWHERE[4096];
    
    CPLFree( pszWHERE );
    pszWHERE = NULL;

    szWHERE[0] = '\0';

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

void OGRMySQLTableLayer::BuildFullQueryStatement()

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

void OGRMySQLTableLayer::ResetReading()

{
    BuildFullQueryStatement();

    OGRMySQLLayer::ResetReading();
}

/************************************************************************/
/*                            BuildFields()                             */
/*                                                                      */
/*      Build list of fields to fetch, performing any required          */
/*      transformations (such as on geometry).                          */
/************************************************************************/

char *OGRMySQLTableLayer::BuildFields()

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

		/* ------------------------------------------------------------ */
		/*      Geometry returned from MySQL is as WKB, with the        */
        /*      first 4 bytes being an int that defines the SRID        */
        /*      and the rest being the WKB.                             */
		/* ------------------------------------------------------------ */            
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

OGRErr OGRMySQLTableLayer::SetAttributeFilter( const char *pszQuery )

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

int OGRMySQLTableLayer::TestCapability( const char * pszCap )

{
    if( EQUAL(pszCap,OLCRandomRead) )
        return bHasFid;

    else if( EQUAL(pszCap,OLCFastFeatureCount) )
        return TRUE;

    else 
        return OGRMySQLLayer::TestCapability( pszCap );
}

/************************************************************************/
/*                             SetFeature()                             */
/*                                                                      */
/*      SetFeature() is implemented by dropping the old copy of the     */
/*      feature in question (if there is one) and then creating a       */
/*      new one with the provided feature id.                           */
/************************************************************************/

OGRErr OGRMySQLTableLayer::SetFeature( OGRFeature *poFeature )

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

OGRErr OGRMySQLTableLayer::DeleteFeature( long nFID )

{
    MYSQL_RES           *hResult=NULL;
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
    if( mysql_query(poDS->GetConn(), osCommand.c_str() ) ){   
        poDS->ReportError(  osCommand.c_str() );
        return OGRERR_FAILURE;   
    }

    // make sure to attempt to free results of successful queries
    hResult = mysql_store_result( poDS->GetConn() );
    if( hResult != NULL )
        mysql_free_result( hResult );
    hResult = NULL;
    
    return OGRERR_NONE;
}


/************************************************************************/
/*                       CreateFeature()                                */
/************************************************************************/

OGRErr OGRMySQLTableLayer::CreateFeature( OGRFeature *poFeature )

{
    MYSQL_RES           *hResult=NULL;
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
        const char *pszStrValue = poFeature->GetFieldAsString(i);
        char *pszNeedToFree = NULL;

        if( !poFeature->IsFieldSet( i ) )
            continue;

        if( bNeedComma )
            osCommand += ", ";
        else
            bNeedComma = TRUE;

        if( poFeatureDefn->GetFieldDefn(i)->GetType() != OFTInteger
                 && poFeatureDefn->GetFieldDefn(i)->GetType() != OFTReal )
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
                    CPLDebug( "MYSQL",
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
        else
        {
            osCommand += pszStrValue;
        }

        if( pszNeedToFree )
            CPLFree( pszNeedToFree );
    }

    osCommand += ")";
    
    int nQueryResult = mysql_query(poDS->GetConn(), osCommand.c_str() );
    
    if( nQueryResult ){   
        int eErrorCode = mysql_errno(poDS->GetConn());
        if (eErrorCode == 1153) {//ER_NET_PACKET_TOO_LARGE)
            poDS->ReportError("CreateFeature failed because the MySQL server " \
                              "cannot read the entire query statement.  Increase " \
                              "the size of statements your server will allow by " \
                              "altering the 'max_allowed_packet' parameter in "\
                              "your MySQL server configuration.");
        }
        else
        {
        CPLDebug("MYSQL","Error number %d", eErrorCode);
            poDS->ReportError(  osCommand.c_str() );
        }

        // make sure to attempt to free results
        hResult = mysql_store_result( poDS->GetConn() );
        if( hResult != NULL )
            mysql_free_result( hResult );
        hResult = NULL;
            
        return OGRERR_FAILURE;   
    }

    // make sure to attempt to free results of successful queries
    hResult = mysql_store_result( poDS->GetConn() );
    if( hResult != NULL )
        mysql_free_result( hResult );
    hResult = NULL;
    
    return OGRERR_NONE;

}
/************************************************************************/
/*                            CreateField()                             */
/************************************************************************/

OGRErr OGRMySQLTableLayer::CreateField( OGRFieldDefn *poFieldIn, int bApproxOK )

{

    MYSQL_RES           *hResult=NULL;
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
/*      Work out the MySQL type.                                        */
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
                  "Can't create field %s with type %s on MySQL layers.  Creating as TEXT.",
                  oField.GetNameRef(),
                  OGRFieldDefn::GetFieldTypeName(oField.GetType()) );
        strcpy( szFieldType, "TEXT" );
    }
    else
    {
        CPLError( CE_Failure, CPLE_NotSupported,
                  "Can't create field %s with type %s on MySQL layers.",
                  oField.GetNameRef(),
                  OGRFieldDefn::GetFieldTypeName(oField.GetType()) );

        return OGRERR_FAILURE;
    }

    sprintf( szCommand,
             "ALTER TABLE %s ADD COLUMN %s %s",
             poFeatureDefn->GetName(), oField.GetNameRef(), szFieldType );

    if( mysql_query(poDS->GetConn(), szCommand ) )
    {
        poDS->ReportError( szCommand );
        return OGRERR_FAILURE;
    }

    // make sure to attempt to free results of successful queries
    hResult = mysql_store_result( poDS->GetConn() );
    if( hResult != NULL )
        mysql_free_result( hResult );
    hResult = NULL;   

    poFeatureDefn->AddFieldDefn( &oField );    
    
    return OGRERR_NONE;
}


/************************************************************************/
/*                             GetFeature()                             */
/************************************************************************/

OGRFeature *OGRMySQLTableLayer::GetFeature( long nFeatureId )

{
    if( pszFIDColumn == NULL )
        return OGRMySQLLayer::GetFeature( nFeatureId );

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
    if( mysql_query( poDS->GetConn(), pszCommand ) )
    {
        poDS->ReportError( pszCommand );
        return NULL;
    }
    CPLFree( pszCommand );

    hResultSet = mysql_store_result( poDS->GetConn() );
    if( hResultSet == NULL )
    {
        poDS->ReportError( "mysql_store_result() failed on query." );
        return NULL;
    }

/* -------------------------------------------------------------------- */
/*      Fetch the result record.                                        */
/* -------------------------------------------------------------------- */
    char **papszRow;
    unsigned long *panLengths;

    papszRow = mysql_fetch_row( hResultSet );
    if( papszRow == NULL )
        return NULL;

    panLengths = mysql_fetch_lengths( hResultSet );

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
        mysql_free_result( hResultSet );
 		hResultSet = NULL;

    return poFeature;
}

/************************************************************************/
/*                          GetFeatureCount()                           */
/*                                                                      */
/*      If a spatial filter is in effect, we turn control over to       */
/*      the generic counter.  Otherwise we return the total count.      */
/*      Eventually we should consider implementing a more efficient     */
/*      way of counting features matching a spatial query.              */
/************************************************************************/

int OGRMySQLTableLayer::GetFeatureCount( int bForce )

{
/* -------------------------------------------------------------------- */
/*      Ensure any active long result is interrupted.                   */
/* -------------------------------------------------------------------- */
    poDS->InterruptLongResult();
    
/* -------------------------------------------------------------------- */
/*      Issue the appropriate select command.                           */
/* -------------------------------------------------------------------- */
    MYSQL_RES    *hResult;
    const char         *pszCommand;

    pszCommand = CPLSPrintf( "SELECT COUNT(*) FROM %s %s", 
                             poFeatureDefn->GetName(), pszWHERE );

    if( mysql_query( poDS->GetConn(), pszCommand ) )
    {
        poDS->ReportError( pszCommand );
        return FALSE;
    }

    hResult = mysql_store_result( poDS->GetConn() );
    if( hResult == NULL )
    {
        poDS->ReportError( "mysql_store_result() failed on SELECT COUNT(*)." );
        return FALSE;
    }
    
/* -------------------------------------------------------------------- */
/*      Capture the result.                                             */
/* -------------------------------------------------------------------- */
    char **papszRow = mysql_fetch_row( hResult );
    int nCount = 0;

    if( papszRow != NULL && papszRow[0] != NULL )
        nCount = atoi(papszRow[0]);

    if( hResultSet != NULL )
        mysql_free_result( hResultSet );
 		hResultSet = NULL;
    
    return nCount;
}

