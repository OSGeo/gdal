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
 * Copyright (c) 2008-2013, Even Rouault <even dot rouault at mines-paris dot org>
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
#include "ogr_mysql.h"

CPL_CVSID("$Id$");

/************************************************************************/
/*                         OGRMySQLTableLayer()                         */
/************************************************************************/

OGRMySQLTableLayer::OGRMySQLTableLayer( OGRMySQLDataSource *poDSIn,
                                        CPL_UNUSED const char * pszTableName,
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
    
    SetDescription( pszTableName );
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
    CPLString     osCommand;
    
/* -------------------------------------------------------------------- */
/*      Fire off commands to get back the schema of the table.          */
/* -------------------------------------------------------------------- */
    osCommand.Printf("DESCRIBE `%s`", pszTable );
    pszGeomColumnTable = CPLStrdup(pszTable);
    if( mysql_query( poDS->GetConn(), osCommand ) )
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
    OGRwkbGeometryType eForcedGeomType = wkbUnknown;
    int bGeomColumnNotNullable = FALSE;

    poDefn->Reference();

    while( (papszRow = mysql_fetch_row( hResult )) != NULL )
    {
        const char      *pszType;
        OGRFieldDefn    oField( papszRow[0], OFTString);
        int             nLenType;

        pszType = papszRow[1];

        if( pszType == NULL )
            continue;

        nLenType = (int)strlen(pszType);

        if( EQUAL(pszType,"varbinary")
            || (nLenType>=4 && EQUAL(pszType+nLenType-4,"blob")))
        {
            oField.SetType( OFTBinary );
        }
        else if( EQUAL(pszType,"varchar") 
                 || (nLenType>=4 && EQUAL(pszType+nLenType-4,"enum"))
                 || (nLenType>=3 && EQUAL(pszType+nLenType-3,"set")) )
        {
            oField.SetType( OFTString );

        }
        else if( EQUALN(pszType,"char",4)  )
        {
            oField.SetType( OFTString );
            char ** papszTokens;

            papszTokens = CSLTokenizeString2(pszType,"(),",0);
            if (CSLCount(papszTokens) >= 2)
            {
                /* width is the second */
                oField.SetWidth(atoi(papszTokens[1]));
            }

            CSLDestroy( papszTokens );
            oField.SetType( OFTString );

        }
        
        if(nLenType>=4 && EQUAL(pszType+nLenType-4,"text"))
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
            if (CSLCount(papszTokens) >= 2)
            {
                /* width is the second */
                oField.SetWidth(atoi(papszTokens[1]));
            }

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
            oField.SetType( OFTInteger64 );
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
            if (CSLCount(papszTokens) >= 3)
            {
                /* width is the second and precision is the third */
                oField.SetWidth(atoi(papszTokens[1]));
                oField.SetPrecision(atoi(papszTokens[2]));
            }
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
            if (CSLCount(papszTokens) >= 3)
            {
                /* width is the second and precision is the third */
                oField.SetWidth(atoi(papszTokens[1]));
                oField.SetPrecision(atoi(papszTokens[2]));
            }
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
        else if( EQUAL(pszType, "geometry") ||
                 OGRFromOGCGeomType(pszType) != wkbUnknown)
        {
            if (pszGeomColumn == NULL)
            {
                pszGeomColumn = CPLStrdup(papszRow[0]);
                eForcedGeomType = OGRFromOGCGeomType(pszType);
                bGeomColumnNotNullable = ( papszRow[2] != NULL && EQUAL(papszRow[2], "NO") );
            }
            else
            {
                CPLDebug("MYSQL",
                         "Ignoring %s as geometry column. Another one(%s) has already been found before",
                         papszRow[0], pszGeomColumn);
            }
            continue;
        }
        // Is this an integer primary key field?
        if( !bHasFid && papszRow[3] != NULL && EQUAL(papszRow[3],"PRI") 
            && (oField.GetType() == OFTInteger || oField.GetType() == OFTInteger64) )
        {
            bHasFid = TRUE;
            pszFIDColumn = CPLStrdup(oField.GetNameRef());
            if( oField.GetType() == OFTInteger64 )
                SetMetadataItem(OLMD_FID64, "YES");
            continue;
        }
        
        // Is not nullable ?
        if( papszRow[2] != NULL && EQUAL(papszRow[2], "NO") )
            oField.SetNullable(FALSE);
        
        // Has default ?
        const char* pszDefault = papszRow[4];
        if( pszDefault != NULL )
        {
            if( !EQUAL(pszDefault, "NULL") &&
                !EQUALN(pszDefault, "CURRENT_", strlen("CURRENT_")) &&
                pszDefault[0] != '(' &&
                pszDefault[0] != '\'' &&
                CPLGetValueType(pszDefault) == CPL_VALUE_STRING )
            {
                int nYear, nMonth, nDay, nHour, nMinute;
                float fSecond;
                if( oField.GetType() == OFTDateTime &&
                    sscanf(pszDefault, "%d-%d-%d %d:%d:%f", &nYear, &nMonth, &nDay,
                                &nHour, &nMinute, &fSecond) == 6 )
                {
                    oField.SetDefault(CPLSPrintf("'%04d/%02d/%02d %02d:%02d:%02d'",
                                            nYear, nMonth, nDay, nHour, nMinute, (int)(fSecond+0.5)));
                }
                else
                {
                    CPLString osDefault("'");
                    char* pszTmp = CPLEscapeString(pszDefault, -1, CPLES_SQL);
                    osDefault += pszTmp;
                    CPLFree(pszTmp);
                    osDefault += "'";
                    oField.SetDefault(osDefault);
                }
            }
            else
            {
                oField.SetDefault(pszDefault);
            }
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
        
        osCommand = "SELECT type, coord_dimension FROM geometry_columns WHERE f_table_name='";
        osCommand += pszTable;
        osCommand += "'";
        
        hResult = NULL;
        if( !mysql_query( poDS->GetConn(), osCommand ) )
            hResult = mysql_store_result( poDS->GetConn() );

        papszRow = NULL;
        if( hResult != NULL )
            papszRow = mysql_fetch_row( hResult );

        if( papszRow != NULL && papszRow[0] != NULL )
        {

            pszType = papszRow[0];

            OGRwkbGeometryType nGeomType = OGRFromOGCGeomType(pszType);

            if( papszRow[1] != NULL && atoi(papszRow[1]) == 3 )
                nGeomType = wkbSetZ(nGeomType);

            poDefn->SetGeomType( nGeomType );

        }
        else if (eForcedGeomType != wkbUnknown)
            poDefn->SetGeomType(eForcedGeomType);

        if( bGeomColumnNotNullable )
            poDefn->GetGeomFieldDefn(0)->SetNullable(FALSE);
        
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
    CPLFree( pszWHERE );
    pszWHERE = (char*)CPLMalloc(500 + ((pszQuery) ? strlen(pszQuery) : 0));
    pszWHERE[0] = '\0';

    if( m_poFilterGeom != NULL && pszGeomColumn )
    {
        char szEnvelope[400];
        OGREnvelope  sEnvelope;
        szEnvelope[0] = '\0';
        
        //POLYGON((MINX MINY, MAXX MINY, MAXX MAXY, MINX MAXY, MINX MINY))
        m_poFilterGeom->getEnvelope( &sEnvelope );
        
        CPLsnprintf(szEnvelope, sizeof(szEnvelope),
                "POLYGON((%.18g %.18g, %.18g %.18g, %.18g %.18g, %.18g %.18g, %.18g %.18g))",
                sEnvelope.MinX, sEnvelope.MinY,
                sEnvelope.MaxX, sEnvelope.MinY,
                sEnvelope.MaxX, sEnvelope.MaxY,
                sEnvelope.MinX, sEnvelope.MaxY,
                sEnvelope.MinX, sEnvelope.MinY);

        sprintf( pszWHERE,
                 "WHERE MBRIntersects(GeomFromText('%s'), `%s`)",
                 szEnvelope,
                 pszGeomColumn);

    }

    if( pszQuery != NULL )
    {
        if( strlen(pszWHERE) == 0 )
            sprintf( pszWHERE, "WHERE %s ", pszQuery  );
        else
            sprintf( pszWHERE+strlen(pszWHERE), "&& (%s) ", pszQuery );
    }
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
             "SELECT %s FROM `%s` %s", 
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
        nSize += strlen(poFeatureDefn->GetFieldDefn(i)->GetNameRef()) + 6;

    pszFieldList = (char *) CPLMalloc(nSize);
    pszFieldList[0] = '\0';

    if( bHasFid && poFeatureDefn->GetFieldIndex( pszFIDColumn ) == -1 )
        sprintf( pszFieldList, "`%s`", pszFIDColumn );

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
                 "`%s` `%s`", pszGeomColumn, pszGeomColumn );
    }

    for( i = 0; i < poFeatureDefn->GetFieldCount(); i++ )
    {
        const char *pszName = poFeatureDefn->GetFieldDefn(i)->GetNameRef();

        if( strlen(pszFieldList) > 0 )
            strcat( pszFieldList, ", " );

        strcat( pszFieldList, "`");
        strcat( pszFieldList, pszName );
        strcat( pszFieldList, "`");
    }

    CPLAssert( (int) strlen(pszFieldList) < nSize );

    return pszFieldList;
}

/************************************************************************/
/*                         SetAttributeFilter()                         */
/************************************************************************/

OGRErr OGRMySQLTableLayer::SetAttributeFilter( const char *pszQuery )

{
    CPLFree(m_pszAttrQueryString);
    m_pszAttrQueryString = (pszQuery) ? CPLStrdup(pszQuery) : NULL;

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

    else if( EQUAL(pszCap,OLCFastSpatialFilter) )
        return TRUE;

    else if( EQUAL(pszCap,OLCFastGetExtent) )
        return TRUE;

    else if( EQUAL(pszCap,OLCCreateField) )
        return bUpdateAccess;

    else if( EQUAL(pszCap,OLCDeleteFeature) )
        return bUpdateAccess;

    else if( EQUAL(pszCap,OLCRandomWrite) )
        return bUpdateAccess;

    else if( EQUAL(pszCap,OLCSequentialWrite) )
        return bUpdateAccess;

    else
        return FALSE;
}

/************************************************************************/
/*                             ISetFeature()                             */
/*                                                                      */
/*      SetFeature() is implemented by dropping the old copy of the     */
/*      feature in question (if there is one) and then creating a       */
/*      new one with the provided feature id.                           */
/************************************************************************/

OGRErr OGRMySQLTableLayer::ISetFeature( OGRFeature *poFeature )

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

OGRErr OGRMySQLTableLayer::DeleteFeature( GIntBig nFID )

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
                  "DeleteFeature(" CPL_FRMT_GIB ") failed.  Unable to delete features "
                  "in tables without\n a recognised FID column.",
                  nFID );
        return OGRERR_FAILURE;

    }

/* -------------------------------------------------------------------- */
/*      Form the statement to drop the record.                          */
/* -------------------------------------------------------------------- */
    osCommand.Printf( "DELETE FROM `%s` WHERE `%s` = " CPL_FRMT_GIB,
                      poFeatureDefn->GetName(), pszFIDColumn, nFID );
                      
/* -------------------------------------------------------------------- */
/*      Execute the delete.                                             */
/* -------------------------------------------------------------------- */
    poDS->InterruptLongResult();
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
/*                       ICreateFeature()                                */
/************************************************************************/

OGRErr OGRMySQLTableLayer::ICreateFeature( OGRFeature *poFeature )

{
    MYSQL_RES           *hResult=NULL;
    CPLString           osCommand;
    int                 i, bNeedComma = FALSE;

/* -------------------------------------------------------------------- */
/*      Form the INSERT command.                                        */
/* -------------------------------------------------------------------- */
    osCommand.Printf( "INSERT INTO `%s` (", poFeatureDefn->GetName() );

    if( poFeature->GetGeometryRef() != NULL )
    {
        osCommand = osCommand + "`" + pszGeomColumn + "` ";
        bNeedComma = TRUE;
    }

    if( poFeature->GetFID() != OGRNullFID && pszFIDColumn != NULL )
    {
        if( bNeedComma )
            osCommand += ", ";
        
        osCommand = osCommand + "`" + pszFIDColumn + "` ";
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

        osCommand = osCommand + "`"
             + poFeatureDefn->GetFieldDefn(i)->GetNameRef() + "`";
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
        if( (GIntBig)(int)poFeature->GetFID() != poFeature->GetFID() &&
            GetMetadataItem(OLMD_FID64) == NULL )
        {
            CPLString osCommand2;
            osCommand2.Printf(
                     "ALTER TABLE `%s` MODIFY COLUMN `%s` BIGINT UNIQUE NOT NULL AUTO_INCREMENT",
                     poFeatureDefn->GetName(), pszFIDColumn );

            if( mysql_query(poDS->GetConn(), osCommand2 ) )
            {
                poDS->ReportError( osCommand2 );
                return OGRERR_FAILURE;
            }

            // make sure to attempt to free results of successful queries
            hResult = mysql_store_result( poDS->GetConn() );
            if( hResult != NULL )
                mysql_free_result( hResult );
            hResult = NULL;   

            SetMetadataItem(OLMD_FID64, "YES");
        }
        
        if( bNeedComma )
            osCommand += ", ";
        osCommand += CPLString().Printf( CPL_FRMT_GIB, poFeature->GetFID() );
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
                 && poFeatureDefn->GetFieldDefn(i)->GetType() != OFTInteger64 
                 && poFeatureDefn->GetFieldDefn(i)->GetType() != OFTReal
                 && poFeatureDefn->GetFieldDefn(i)->GetType() != OFTBinary )
        {
            int         iChar;

            //We need to quote and escape string fields. 
            osCommand += "'";

            for( iChar = 0; pszStrValue[iChar] != '\0'; iChar++ )
            {
                if( poFeatureDefn->GetFieldDefn(i)->GetType() != OFTIntegerList
                    && poFeatureDefn->GetFieldDefn(i)->GetType() != OFTInteger64List
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
    
    //CPLDebug("MYSQL", "%s", osCommand.c_str());
    int nQueryResult = mysql_query(poDS->GetConn(), osCommand.c_str() );
    const my_ulonglong nFID = mysql_insert_id( poDS->GetConn() );
    
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

    if( nFID > 0 ) {
        poFeature->SetFID( nFID );
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
    CPLString            osCommand;
    
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
    else if( oField.GetType() == OFTInteger64 )
    {
        if( oField.GetWidth() > 0 && bPreservePrecision )
            sprintf( szFieldType, "DECIMAL(%d,0)", oField.GetWidth() );
        else
            strcpy( szFieldType, "BIGINT" );
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
        oField.SetDefault(NULL);
        sprintf( szFieldType, "DATE" );
    }

    else if( oField.GetType() == OFTDateTime )
    {
        if( oField.GetDefault() != NULL && EQUAL(oField.GetDefault(), "CURRENT_TIMESTAMP") )
            sprintf( szFieldType, "TIMESTAMP" );
        else
            sprintf( szFieldType, "DATETIME" );
    }

    else if( oField.GetType() == OFTTime )
    {
        oField.SetDefault(NULL);
        sprintf( szFieldType, "TIME" );
    }

    else if( oField.GetType() == OFTBinary )
    {
        sprintf( szFieldType, "LONGBLOB" );
    }

    else if( oField.GetType() == OFTString )
    {
        if( oField.GetWidth() == 0 || !bPreservePrecision )
        {
            if( oField.GetDefault() != NULL )
                strcpy( szFieldType, "VARCHAR(256)" );
            else
                strcpy( szFieldType, "TEXT" );
        }
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
        oField.SetWidth(0);
        oField.SetPrecision(0);
    }
    else
    {
        CPLError( CE_Failure, CPLE_NotSupported,
                  "Can't create field %s with type %s on MySQL layers.",
                  oField.GetNameRef(),
                  OGRFieldDefn::GetFieldTypeName(oField.GetType()) );

        return OGRERR_FAILURE;
    }

    osCommand.Printf(
             "ALTER TABLE `%s` ADD COLUMN `%s` %s%s",
             poFeatureDefn->GetName(), oField.GetNameRef(), szFieldType,
             (!oField.IsNullable()) ? " NOT NULL" : "");
    if( oField.GetDefault() != NULL && !oField.IsDefaultDriverSpecific() )
    {
        osCommand += " DEFAULT ";
        osCommand += oField.GetDefault();
    }

    if( mysql_query(poDS->GetConn(), osCommand ) )
    {
        poDS->ReportError( osCommand );
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

OGRFeature *OGRMySQLTableLayer::GetFeature( GIntBig nFeatureId )

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
    CPLString    osCommand;

    osCommand.Printf(
             "SELECT %s FROM `%s` WHERE `%s` = " CPL_FRMT_GIB, 
             pszFieldList, poFeatureDefn->GetName(), pszFIDColumn, 
             nFeatureId );
    CPLFree( pszFieldList );

/* -------------------------------------------------------------------- */
/*      Issue the command.                                              */
/* -------------------------------------------------------------------- */
    if( mysql_query( poDS->GetConn(), osCommand ) )
    {
        poDS->ReportError( osCommand );
        return NULL;
    }

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

GIntBig OGRMySQLTableLayer::GetFeatureCount( CPL_UNUSED int bForce )
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

    pszCommand = CPLSPrintf( "SELECT COUNT(*) FROM `%s` %s", 
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
    GIntBig nCount = 0;

    if( papszRow != NULL && papszRow[0] != NULL )
        nCount = CPLAtoGIntBig(papszRow[0]);

    if( hResult != NULL )
        mysql_free_result( hResult );
    hResult = NULL;
    
    return nCount;
}

/************************************************************************/
/*                          GetExtent()					*/
/*                                                                      */
/*      Retrieve the MBR of the MySQL table.  This should be made more  */
/*      in the future when MySQL adds support for a single MBR query    */
/*      like PostgreSQL.						*/
/************************************************************************/

OGRErr OGRMySQLTableLayer::GetExtent(OGREnvelope *psExtent, CPL_UNUSED int bForce )
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

	osCommand.Printf( "SELECT Envelope(`%s`) FROM `%s`;", pszGeomColumn, pszGeomColumnTable);

	if (mysql_query(poDS->GetConn(), osCommand) == 0)
	{
		MYSQL_RES* result = mysql_use_result(poDS->GetConn());
		if ( result == NULL )
        {
            poDS->ReportError( "mysql_use_result() failed on extents query." );
            return OGRERR_FAILURE;
        }

		MYSQL_ROW row; 
		unsigned long *panLengths = NULL;
		while ((row = mysql_fetch_row(result)))
		{
			if (panLengths == NULL)
			{
				panLengths = mysql_fetch_lengths( result );
				if ( panLengths == NULL )
				{
					poDS->ReportError( "mysql_fetch_lengths() failed on extents query." );
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

		mysql_free_result(result);      
	}

	return (bExtentSet ? OGRERR_NONE : OGRERR_FAILURE);
}
