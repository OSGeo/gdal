/******************************************************************************
 * $Id$
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  Implements OGRMySQLResultLayer class.
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
 * DEALINGS IN THE SOFTWARE.
 ****************************************************************************/

#include "cpl_conv.h"
#include "ogr_mysql.h"

CPL_CVSID("$Id$");

/************************************************************************/
/*                        OGRMySQLResultLayer()                         */
/************************************************************************/

OGRMySQLResultLayer::OGRMySQLResultLayer( OGRMySQLDataSource *poDSIn, 
                                          const char * pszRawQueryIn,
                                          MYSQL_RES *hResultSetIn )
{
    poDS = poDSIn;

    iNextShapeId = 0;

    pszRawStatement = CPLStrdup(pszRawQueryIn);

    hResultSet = hResultSetIn;

    BuildFullQueryStatement();

    poFeatureDefn = ReadResultDefinition();
}

/************************************************************************/
/*                        ~OGRMySQLResultLayer()                        */
/************************************************************************/

OGRMySQLResultLayer::~OGRMySQLResultLayer()

{
    CPLFree( pszRawStatement );
}

/************************************************************************/
/*                        ReadResultDefinition()                        */
/*                                                                      */
/*      Build a schema from the current resultset.                      */
/************************************************************************/

OGRFeatureDefn *OGRMySQLResultLayer::ReadResultDefinition()

{

/* -------------------------------------------------------------------- */
/*      Parse the returned table information.                           */
/* -------------------------------------------------------------------- */
    OGRFeatureDefn *poDefn = new OGRFeatureDefn( "sql_statement" );
    int            iRawField;

    poDefn->Reference();
    int width;
    int precision;

    mysql_field_seek( hResultSet, 0 );
    for( iRawField = 0; 
         iRawField < (int) mysql_num_fields(hResultSet); 
         iRawField++ )
    {
        MYSQL_FIELD *psMSField = mysql_fetch_field( hResultSet );
        OGRFieldDefn    oField( psMSField->name, OFTString);

        switch( psMSField->type )
        {
          case FIELD_TYPE_TINY:
          case FIELD_TYPE_SHORT:
          case FIELD_TYPE_LONG:
          case FIELD_TYPE_INT24:
          case FIELD_TYPE_LONGLONG:
            oField.SetType( OFTInteger );
            width = (int)psMSField->length;
            oField.SetWidth(width);
            poDefn->AddFieldDefn( &oField );
            break;

          case FIELD_TYPE_DECIMAL:
#ifdef FIELD_TYPE_NEWDECIMAL
          case FIELD_TYPE_NEWDECIMAL:
#endif
            oField.SetType( OFTReal );
            
            // a bunch of hackery to munge the widths that MySQL gives 
            // us into corresponding widths and precisions for OGR
            precision =    (int)psMSField->decimals;
            width = (int)psMSField->length;
            if (!precision)
                width = width - 1;
            width = width - precision;
            
            oField.SetWidth(width);
            oField.SetPrecision(precision);
            poDefn->AddFieldDefn( &oField );
            break;

          case FIELD_TYPE_FLOAT:
          case FIELD_TYPE_DOUBLE:
         /* MYSQL_FIELD is always reporting ->length = 22 and ->decimals = 31
            for double type regardless of the data it returned. In an example,
            the data it returned had only 5 or 6 decimal places which were
            exactly as entered into the database but reported the decimals
            as 31. */
         /* Assuming that a length of 22 means no particular width and 31
            decimals means no particular precision. */
            width = (int)psMSField->length;
            precision = (int)psMSField->decimals;
            oField.SetType( OFTReal );
            if( width != 22 )
                oField.SetWidth(width);
            if( precision != 31 )
                oField.SetPrecision(precision);
            poDefn->AddFieldDefn( &oField );
            break;

          case FIELD_TYPE_DATE:
            oField.SetType( OFTDate );
            oField.SetWidth(0);
            poDefn->AddFieldDefn( &oField );
            break;

          case FIELD_TYPE_TIME:
            oField.SetType( OFTTime );
            oField.SetWidth(0);
            poDefn->AddFieldDefn( &oField );
            break;

          case FIELD_TYPE_TIMESTAMP:
          case FIELD_TYPE_DATETIME:
            oField.SetType( OFTDateTime );
            oField.SetWidth(0);
            poDefn->AddFieldDefn( &oField );
            break;

          case FIELD_TYPE_YEAR:
          case FIELD_TYPE_STRING:
          case FIELD_TYPE_VAR_STRING:
            oField.SetType( OFTString );
            oField.SetWidth((int)psMSField->length);
            poDefn->AddFieldDefn( &oField );
            break;

          case FIELD_TYPE_TINY_BLOB:
          case FIELD_TYPE_MEDIUM_BLOB:
          case FIELD_TYPE_LONG_BLOB:
          case FIELD_TYPE_BLOB:
            if( psMSField->charsetnr == 63 )
                oField.SetType( OFTBinary );
            else
                oField.SetType( OFTString );
            oField.SetWidth((int)psMSField->max_length);
            poDefn->AddFieldDefn( &oField );
            break;
                        
          case FIELD_TYPE_GEOMETRY:
            if (pszGeomColumn == NULL)
            {
                pszGeomColumnTable = CPLStrdup( psMSField->table);
                pszGeomColumn = CPLStrdup( psMSField->name);
            }
            break;
            
          default:
            // any other field we ignore. 
            break;
        }
        
        // assume a FID name first, and if it isn't there
        // take a field that is not null, a primary key, 
        // and is an integer-like field
        if( EQUAL(psMSField->name,"ogc_fid") )
        {
            bHasFid = TRUE;
            pszFIDColumn = CPLStrdup(oField.GetNameRef());
            continue;
        } else  
        if (IS_NOT_NULL(psMSField->flags)
            && IS_PRI_KEY(psMSField->flags)
            && 
                (
                    psMSField->type == FIELD_TYPE_TINY
                    || psMSField->type == FIELD_TYPE_SHORT
                    || psMSField->type == FIELD_TYPE_LONG
                    || psMSField->type == FIELD_TYPE_INT24
                    || psMSField->type == FIELD_TYPE_LONGLONG
                )
            )
        {
           bHasFid = TRUE;
           pszFIDColumn = CPLStrdup(oField.GetNameRef());
           continue;
        }
    }


    poDefn->SetGeomType( wkbNone );

    if (pszGeomColumn) 
    {
        char*        pszType=NULL;
        CPLString    osCommand;
        char           **papszRow;  
         
        // set to unknown first
        poDefn->SetGeomType( wkbUnknown );
        
        osCommand.Printf(
                "SELECT type FROM geometry_columns WHERE f_table_name='%s'",
                pszGeomColumnTable );

        if( hResultSet != NULL )
            mysql_free_result( hResultSet );
     		hResultSet = NULL;

        if( !mysql_query( poDS->GetConn(), osCommand ) )
            hResultSet = mysql_store_result( poDS->GetConn() );

        papszRow = NULL;
        if( hResultSet != NULL )
            papszRow = mysql_fetch_row( hResultSet );


        if( papszRow != NULL && papszRow[0] != NULL )
        {
            pszType = papszRow[0];

            OGRwkbGeometryType nGeomType = OGRFromOGCGeomType(pszType);

            poDefn->SetGeomType( nGeomType );

        } 

		nSRSId = FetchSRSId();
    } 


    return poDefn;
}

/************************************************************************/
/*                      BuildFullQueryStatement()                       */
/************************************************************************/

void OGRMySQLResultLayer::BuildFullQueryStatement()

{
    if( pszQueryStatement != NULL )
    {
        CPLFree( pszQueryStatement );
        pszQueryStatement = NULL;
    }

    pszQueryStatement = CPLStrdup(pszRawStatement);
}

/************************************************************************/
/*                            ResetReading()                            */
/************************************************************************/

void OGRMySQLResultLayer::ResetReading()

{
    OGRMySQLLayer::ResetReading();
}

/************************************************************************/
/*                          GetFeatureCount()                           */
/************************************************************************/

int OGRMySQLResultLayer::GetFeatureCount( int bForce )

{
    // I wonder if we could do anything smart here...
    // ... not till MySQL grows up (HB)
    return OGRMySQLLayer::GetFeatureCount( bForce );
}

/************************************************************************/
/*                           TestCapability()                           */
/************************************************************************/

int OGRMySQLResultLayer::TestCapability( const char * pszCap )

{
    return FALSE;
}

