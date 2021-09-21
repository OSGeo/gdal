/******************************************************************************
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  Implements OGRMySQLTableLayer class.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 * Author:   Howard Butler, hobu@hobu.net
 *
 ******************************************************************************
 * Copyright (c) 2004, Frank Warmerdam <warmerdam@pobox.com>
 * Copyright (c) 2008-2013, Even Rouault <even dot rouault at spatialys.com>
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

CPL_CVSID("$Id$")

/************************************************************************/
/*                         OGRMySQLTableLayer()                         */
/************************************************************************/

OGRMySQLTableLayer::OGRMySQLTableLayer( OGRMySQLDataSource *poDSIn,
                                        CPL_UNUSED const char * pszTableName,
                                        int bUpdate, int nSRSIdIn ) :
    bUpdateAccess(bUpdate),
    pszQuery(nullptr),
    pszWHERE(CPLStrdup("")),
    bLaunderColumnNames(TRUE),
    bPreservePrecision(FALSE)
{
    poDS = poDSIn;

    pszQueryStatement = nullptr;

    iNextShapeId = 0;

    nSRSId = nSRSIdIn;

    poFeatureDefn = nullptr;

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
        return nullptr;
    }

    hResult = mysql_store_result( poDS->GetConn() );
    if( hResult == nullptr )
    {
        poDS->ReportError( "mysql_store_result() failed on DESCRIBE result." );
        return nullptr;
    }

/* -------------------------------------------------------------------- */
/*      Parse the returned table information.                           */
/* -------------------------------------------------------------------- */
    OGRFeatureDefn *poDefn = new OGRFeatureDefn( pszTable );
    char           **papszRow;
    OGRwkbGeometryType eForcedGeomType = wkbUnknown;
    int bGeomColumnNotNullable = FALSE;

    poDefn->Reference();

    while( (papszRow = mysql_fetch_row( hResult )) != nullptr )
    {
        OGRFieldDefn    oField( papszRow[0], OFTString);

        const char *pszType = papszRow[1];
        if( pszType == nullptr )
            continue;

        int nLenType = (int)strlen(pszType);

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
        else if( STARTS_WITH_CI(pszType, "char")  )
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
        else if( STARTS_WITH_CI(pszType,"varchar")  )
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
        else if( STARTS_WITH_CI(pszType, "int") )
        {
            oField.SetType( OFTInteger );
        }
        else if( STARTS_WITH_CI(pszType, "tinyint") )
        {
            oField.SetType( OFTInteger );
        }
        else if( STARTS_WITH_CI(pszType, "smallint") )
        {
            oField.SetType( OFTInteger );
        }
        else if( STARTS_WITH_CI(pszType, "mediumint") )
        {
            oField.SetType( OFTInteger );
        }
        else if( STARTS_WITH_CI(pszType, "bigint") )
        {
            oField.SetType( OFTInteger64 );
        }
        else if( STARTS_WITH_CI(pszType, "decimal") )
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
        else if( STARTS_WITH_CI(pszType, "float") )
        {
            oField.SetType( OFTReal );
        }
        else if( EQUAL(pszType,"double") )
        {
            oField.SetType( OFTReal );
        }
        else if( STARTS_WITH_CI(pszType, "double") )
        {
            // double can also be double(15,2)
            // so we'll handle this case here after
            // we check for just a regular double
            // without a width and precision specified

            char ** papszTokens = CSLTokenizeString2(pszType,"(),",0);
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
            if (pszGeomColumn == nullptr)
            {
                pszGeomColumn = CPLStrdup(papszRow[0]);
                eForcedGeomType = OGRFromOGCGeomType(pszType);
                bGeomColumnNotNullable = ( papszRow[2] != nullptr && EQUAL(papszRow[2], "NO") );
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
        if( !bHasFid && papszRow[3] != nullptr && EQUAL(papszRow[3],"PRI")
            && (oField.GetType() == OFTInteger || oField.GetType() == OFTInteger64) )
        {
            bHasFid = TRUE;
            pszFIDColumn = CPLStrdup(oField.GetNameRef());
            if( oField.GetType() == OFTInteger64 )
                SetMetadataItem(OLMD_FID64, "YES");
            continue;
        }

        // Is not nullable ?
        if( papszRow[2] != nullptr && EQUAL(papszRow[2], "NO") )
            oField.SetNullable(FALSE);

        // Has default ?
        const char* pszDefault = papszRow[4];
        if( pszDefault != nullptr )
        {
            if( !EQUAL(pszDefault, "NULL") &&
                !STARTS_WITH_CI(pszDefault, "CURRENT_") &&
                pszDefault[0] != '(' &&
                pszDefault[0] != '\'' &&
                CPLGetValueType(pszDefault) == CPL_VALUE_STRING )
            {
                int nYear = 0;
                int nMonth = 0;
                int nDay = 0;
                int nHour = 0;
                int nMinute = 0;
                float fSecond = 0.0f;
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
                if( EQUAL(pszDefault, "CURRENT_TIMESTAMP()") )
                    oField.SetDefault("CURRENT_TIMESTAMP");
                else
                    oField.SetDefault(pszDefault);
            }
        }

        poDefn->AddFieldDefn( &oField );
    }

    // set to none for now... if we have a geometry column it will be set layer.
    poDefn->SetGeomType( wkbNone );

    mysql_free_result( hResult );
    hResult = nullptr;

    if( bHasFid )
        CPLDebug( "MySQL", "table %s has FID column %s.",
                  pszTable, pszFIDColumn );
    else
        CPLDebug( "MySQL",
                  "table %s has no FID column, FIDs will not be reliable!",
                  pszTable );

    if (pszGeomColumn)
    {
        char*        pszType=nullptr;

        // set to unknown first
        poDefn->SetGeomType( wkbUnknown );
        poDefn->GetGeomFieldDefn(0)->SetName( pszGeomColumn );

        if( poDS->GetMajorVersion() < 8 || poDS->IsMariaDB() )
            osCommand.Printf("SELECT type, coord_dimension FROM geometry_columns WHERE f_table_name='%s'",
                             pszTable);

        else
            osCommand.Printf("SELECT GEOMETRY_TYPE_NAME FROM INFORMATION_SCHEMA.ST_GEOMETRY_COLUMNS "
                             "WHERE TABLE_NAME = '%s'", pszTable);

        if( !mysql_query( poDS->GetConn(), osCommand ) )
            hResult = mysql_store_result( poDS->GetConn() );

        papszRow = nullptr;
        if( hResult != nullptr )
            papszRow = mysql_fetch_row( hResult );

        if( papszRow != nullptr && papszRow[0] != nullptr )
        {

            pszType = papszRow[0];

            OGRwkbGeometryType l_nGeomType = OGRFromOGCGeomType(pszType);

            if( poDS->GetMajorVersion() < 8 || poDS->IsMariaDB() )
                if( papszRow[1] != nullptr && atoi(papszRow[1]) == 3 )
                    l_nGeomType = wkbSetZ(l_nGeomType);

            poDefn->SetGeomType( l_nGeomType );
        }
        else if (eForcedGeomType != wkbUnknown)
            poDefn->SetGeomType(eForcedGeomType);

        if( bGeomColumnNotNullable )
            poDefn->GetGeomFieldDefn(0)->SetNullable(FALSE);

        if( hResult != nullptr )
            mysql_free_result( hResult );   //Free our query results for finding type.
        hResult = nullptr;
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
    const size_t nWHERELen = 500 + ((pszQuery) ? strlen(pszQuery) : 0);
    pszWHERE = (char*)CPLMalloc(nWHERELen);
    pszWHERE[0] = '\0';

    if( m_poFilterGeom != nullptr && pszGeomColumn )
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

        const char* pszAxisOrder = "";
        OGRSpatialReference* l_poSRS = GetSpatialRef();
        if( poDS->GetMajorVersion() >= 8 && !poDS->IsMariaDB() &&
            l_poSRS && l_poSRS->IsGeographic() )
        {
            pszAxisOrder = ", 'axis-order=long-lat'";
        }

        snprintf( pszWHERE, nWHERELen,
                 "WHERE MBRIntersects(%s('%s', %d%s), `%s`)",
                 poDS->GetMajorVersion() >= 8 ? "ST_GeomFromText" : "GeomFromText",
                 szEnvelope,
                 nSRSId,
                 pszAxisOrder,
                 pszGeomColumn);
    }

    if( pszQuery != nullptr )
    {
        if( strlen(pszWHERE) == 0 )
            snprintf( pszWHERE, nWHERELen, "WHERE %s ", pszQuery  );
        else
            snprintf( pszWHERE+strlen(pszWHERE),
                      nWHERELen - strlen(pszWHERE), "&& (%s) ", pszQuery );
    }
}

/************************************************************************/
/*                      BuildFullQueryStatement()                       */
/************************************************************************/

void OGRMySQLTableLayer::BuildFullQueryStatement()

{
    if( pszQueryStatement != nullptr )
    {
        CPLFree( pszQueryStatement );
        pszQueryStatement = nullptr;
    }

    char *pszFields = BuildFields();

    pszQueryStatement = (char *)
        CPLMalloc(strlen(pszFields)+strlen(pszWHERE)
                  +strlen(poFeatureDefn->GetName()) + 40);
    snprintf( pszQueryStatement,
              strlen(pszFields)+strlen(pszWHERE)
                  +strlen(poFeatureDefn->GetName()) + 40,
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
    size_t nSize = 25;
    if( pszGeomColumn )
        nSize += strlen(pszGeomColumn);

    if( bHasFid )
        nSize += strlen(pszFIDColumn);

    for( int i = 0; i < poFeatureDefn->GetFieldCount(); i++ )
        nSize += strlen(poFeatureDefn->GetFieldDefn(i)->GetNameRef()) + 6;

    char *pszFieldList = (char *) CPLMalloc(nSize);
    pszFieldList[0] = '\0';

    if( bHasFid && poFeatureDefn->GetFieldIndex( pszFIDColumn ) == -1 )
        snprintf( pszFieldList, nSize, "`%s`", pszFIDColumn );

    if( pszGeomColumn )
    {
        if( strlen(pszFieldList) > 0 )
            strcat( pszFieldList, ", " );

        /* ------------------------------------------------------------ */
        /*      Geometry returned from MySQL is as WKB, with the        */
        /*      first 4 bytes being an int that defines the SRID        */
        /*      and the rest being the WKB.                             */
        /* ------------------------------------------------------------ */
        snprintf( pszFieldList+strlen(pszFieldList), nSize-strlen(pszFieldList),
                 "`%s` `%s`", pszGeomColumn, pszGeomColumn );
    }

    for( int i = 0; i < poFeatureDefn->GetFieldCount(); i++ )
    {
        const char *pszName = poFeatureDefn->GetFieldDefn(i)->GetNameRef();

        if( strlen(pszFieldList) > 0 )
            strcat( pszFieldList, ", " );

        strcat( pszFieldList, "`");
        strcat( pszFieldList, pszName );
        strcat( pszFieldList, "`");
    }

    CPLAssert( strlen(pszFieldList) < nSize );

    return pszFieldList;
}

/************************************************************************/
/*                         SetAttributeFilter()                         */
/************************************************************************/

OGRErr OGRMySQLTableLayer::SetAttributeFilter( const char *pszQueryIn )

{
    CPLFree(m_pszAttrQueryString);
    m_pszAttrQueryString = pszQueryIn ? CPLStrdup(pszQueryIn) : nullptr;

    CPLFree( pszQuery );

    if( pszQueryIn == nullptr || strlen(pszQueryIn) == 0 )
        pszQuery = nullptr;
    else
        pszQuery = CPLStrdup( pszQueryIn );

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
    MYSQL_RES           *hResult=nullptr;
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
    if( hResult != nullptr )
        mysql_free_result( hResult );
    hResult = nullptr;

    return mysql_affected_rows( poDS->GetConn() ) > 0 ? OGRERR_NONE : OGRERR_NON_EXISTING_FEATURE;
}

/************************************************************************/
/*                       ICreateFeature()                                */
/************************************************************************/

OGRErr OGRMySQLTableLayer::ICreateFeature( OGRFeature *poFeature )

{
    int bNeedComma = FALSE;
    CPLString osCommand;

/* -------------------------------------------------------------------- */
/*      Form the INSERT command.                                        */
/* -------------------------------------------------------------------- */
    osCommand.Printf( "INSERT INTO `%s` (", poFeatureDefn->GetName() );

    if( poFeature->GetGeometryRef() != nullptr )
    {
        osCommand = osCommand + "`" + pszGeomColumn + "` ";
        bNeedComma = TRUE;
    }

    if( poFeature->GetFID() != OGRNullFID && pszFIDColumn != nullptr )
    {
        if( bNeedComma )
            osCommand += ", ";

        osCommand = osCommand + "`" + pszFIDColumn + "` ";
        bNeedComma = TRUE;
    }

    for( int i = 0; i < poFeatureDefn->GetFieldCount(); i++ )
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
    OGRGeometry *poGeom = poFeature->GetGeometryRef();
    bNeedComma = poGeom != nullptr;
    if( poGeom != nullptr)
    {
        char    *pszWKT = nullptr;
        poGeom->closeRings();
        poGeom->flattenTo2D();
        poGeom->exportToWkt( &pszWKT );

        if( pszWKT != nullptr )
        {
            const char* pszAxisOrder = "";
            OGRSpatialReference* l_poSRS = GetSpatialRef();
            if( poDS->GetMajorVersion() >= 8 && !poDS->IsMariaDB() &&
                l_poSRS && l_poSRS->IsGeographic() )
            {
                pszAxisOrder = ", 'axis-order=long-lat'";
            }

            osCommand +=
                CPLString().Printf(
                    "%s('%s',%d%s) ",
                    poDS->GetMajorVersion() >= 8 ? "ST_GeomFromText" : "GeometryFromText",
                    pszWKT, nSRSId, pszAxisOrder );

            CPLFree( pszWKT );
        }
        else
            osCommand += "''";
    }

    // Set the FID
    if( poFeature->GetFID() != OGRNullFID && pszFIDColumn != nullptr )
    {
        GIntBig nFID = poFeature->GetFID();
        if( !CPL_INT64_FITS_ON_INT32(nFID) &&
            GetMetadataItem(OLMD_FID64) == nullptr )
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
            MYSQL_RES *hResult = mysql_store_result( poDS->GetConn() );
            if( hResult != nullptr )
                mysql_free_result( hResult );
            hResult = nullptr;

            SetMetadataItem(OLMD_FID64, "YES");
        }

        if( bNeedComma )
            osCommand += ", ";
        osCommand += CPLString().Printf( CPL_FRMT_GIB, nFID );
        bNeedComma = TRUE;
    }

    for( int i = 0; i < poFeatureDefn->GetFieldCount(); i++ )
    {
        if( !poFeature->IsFieldSet( i ) )
            continue;

        if( bNeedComma )
            osCommand += ", ";
        else
            bNeedComma = TRUE;

        const char *pszStrValue = poFeature->GetFieldAsString(i);

        if( poFeature->IsFieldNull(i) )
        {
            osCommand += "NULL";
        }
        else if( poFeatureDefn->GetFieldDefn(i)->GetType() != OFTInteger
                 && poFeatureDefn->GetFieldDefn(i)->GetType() != OFTInteger64
                 && poFeatureDefn->GetFieldDefn(i)->GetType() != OFTReal
                 && poFeatureDefn->GetFieldDefn(i)->GetType() != OFTBinary )
        {
            // We need to quote and escape string fields.
            osCommand += "'";

            for( int iChar = 0; pszStrValue[iChar] != '\0'; iChar++ )
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
        MYSQL_RES *hResult = mysql_store_result( poDS->GetConn() );
        if( hResult != nullptr )
            mysql_free_result( hResult );
        hResult = nullptr;

        return OGRERR_FAILURE;
    }

    if( nFID > 0 ) {
        poFeature->SetFID( nFID );
    }

    // make sure to attempt to free results of successful queries
    MYSQL_RES *hResult = mysql_store_result( poDS->GetConn() );
    if( hResult != nullptr )
        mysql_free_result( hResult );
    hResult = nullptr;

    return OGRERR_NONE;
}

/************************************************************************/
/*                            CreateField()                             */
/************************************************************************/

OGRErr OGRMySQLTableLayer::CreateField( OGRFieldDefn *poFieldIn, int bApproxOK )

{

    MYSQL_RES           *hResult=nullptr;
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
            snprintf( szFieldType, sizeof(szFieldType), "DECIMAL(%d,0)", oField.GetWidth() );
        else
            strcpy( szFieldType, "INTEGER" );
    }
    else if( oField.GetType() == OFTInteger64 )
    {
        if( oField.GetWidth() > 0 && bPreservePrecision )
            snprintf( szFieldType, sizeof(szFieldType), "DECIMAL(%d,0)", oField.GetWidth() );
        else
            strcpy( szFieldType, "BIGINT" );
    }
    else if( oField.GetType() == OFTReal )
    {
        if( oField.GetWidth() > 0 && oField.GetPrecision() > 0
            && bPreservePrecision )
            snprintf( szFieldType, sizeof(szFieldType), "DOUBLE(%d,%d)",
                     oField.GetWidth(), oField.GetPrecision() );
        else
            strcpy( szFieldType, "DOUBLE" );
    }

    else if( oField.GetType() == OFTDate )
    {
        oField.SetDefault(nullptr);
        snprintf( szFieldType, sizeof(szFieldType), "DATE" );
    }

    else if( oField.GetType() == OFTDateTime )
    {
        if( oField.GetDefault() != nullptr && STARTS_WITH_CI(oField.GetDefault(), "CURRENT_TIMESTAMP") )
            snprintf( szFieldType, sizeof(szFieldType), "TIMESTAMP" );
        else
            snprintf( szFieldType, sizeof(szFieldType), "DATETIME" );
    }

    else if( oField.GetType() == OFTTime )
    {
        oField.SetDefault(nullptr);
        snprintf( szFieldType, sizeof(szFieldType), "TIME" );
    }

    else if( oField.GetType() == OFTBinary )
    {
        snprintf( szFieldType, sizeof(szFieldType), "LONGBLOB" );
    }

    else if( oField.GetType() == OFTString )
    {
        if( oField.GetWidth() == 0 || !bPreservePrecision )
        {
            if( oField.GetDefault() != nullptr )
                strcpy( szFieldType, "VARCHAR(256)" );
            else
                strcpy( szFieldType, "TEXT" );
        }
        else
            snprintf( szFieldType, sizeof(szFieldType), "VARCHAR(%d)", oField.GetWidth() );
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
    if( oField.GetDefault() != nullptr && !oField.IsDefaultDriverSpecific() )
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
    if( hResult != nullptr )
        mysql_free_result( hResult );
    hResult = nullptr;

    poFeatureDefn->AddFieldDefn( &oField );

    return OGRERR_NONE;
}

/************************************************************************/
/*                             GetFeature()                             */
/************************************************************************/

OGRFeature *OGRMySQLTableLayer::GetFeature( GIntBig nFeatureId )

{
    if( pszFIDColumn == nullptr )
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
        return nullptr;
    }

    hResultSet = mysql_store_result( poDS->GetConn() );
    if( hResultSet == nullptr )
    {
        poDS->ReportError( "mysql_store_result() failed on query." );
        return nullptr;
    }

/* -------------------------------------------------------------------- */
/*      Fetch the result record.                                        */
/* -------------------------------------------------------------------- */
    char **papszRow;
    unsigned long *panLengths;

    papszRow = mysql_fetch_row( hResultSet );
    if( papszRow == nullptr )
        return nullptr;

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
    if( hResultSet != nullptr )
        mysql_free_result( hResultSet );
    hResultSet = nullptr;

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
    if( hResult == nullptr )
    {
        poDS->ReportError( "mysql_store_result() failed on SELECT COUNT(*)." );
        return FALSE;
    }

/* -------------------------------------------------------------------- */
/*      Capture the result.                                             */
/* -------------------------------------------------------------------- */
    char **papszRow = mysql_fetch_row( hResult );
    GIntBig nCount = 0;

    if( papszRow != nullptr && papszRow[0] != nullptr )
        nCount = CPLAtoGIntBig(papszRow[0]);

    mysql_free_result( hResult );

    return nCount;
}

/************************************************************************/
/*                          GetExtent()                                 */
/*                                                                      */
/*      Retrieve the MBR of the MySQL table.  This should be made more  */
/*      in the future when MySQL adds support for a single MBR query    */
/*      like PostgreSQL.                                                */
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

    ResetReading();

    OGREnvelope oEnv;
    CPLString   osCommand;
    GBool       bExtentSet = FALSE;

    if( poDS->GetMajorVersion() >= 8 && !poDS->IsMariaDB() )
    {
        // ST_Envelope() does not work on geographic SRS, so force to 0
        osCommand.Printf( "SELECT ST_Envelope(ST_SRID(`%s`,0)) FROM `%s`;", pszGeomColumn, pszGeomColumnTable);
    }
    else
    {
        osCommand.Printf( "SELECT Envelope(`%s`) FROM `%s`;", pszGeomColumn, pszGeomColumnTable);
    }

    if (mysql_query(poDS->GetConn(), osCommand) == 0)
    {
        MYSQL_RES* result = mysql_use_result(poDS->GetConn());
        if ( result == nullptr )
        {
            poDS->ReportError( "mysql_use_result() failed on extents query." );
            return OGRERR_FAILURE;
        }

        MYSQL_ROW row;
        unsigned long *panLengths = nullptr;
        while ((row = mysql_fetch_row(result)) != nullptr)
        {
            if (panLengths == nullptr)
            {
                panLengths = mysql_fetch_lengths( result );
                if ( panLengths == nullptr )
                {
                    poDS->ReportError( "mysql_fetch_lengths() failed on extents query." );
                    return OGRERR_FAILURE;
                }
            }

            OGRGeometry *poGeometry = nullptr;
            // Geometry columns will have the first 4 bytes contain the SRID.
            OGRGeometryFactory::createFromWkb(row[0] + 4,
                                              nullptr,
                                              &poGeometry,
                                              static_cast<int>(panLengths[0] - 4) );

            if ( poGeometry != nullptr )
            {
                if (!bExtentSet)
                {
                    poGeometry->getEnvelope(psExtent);
                    bExtentSet = TRUE;
                }
                else
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
    else
    {
        poDS->ReportError(  osCommand.c_str() );
    }

    return bExtentSet ? OGRERR_NONE : OGRERR_FAILURE;
}
