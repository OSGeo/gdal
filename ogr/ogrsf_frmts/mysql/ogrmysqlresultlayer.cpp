/******************************************************************************
 * $Id$
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  Implements OGRMySQLResultLayer class.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
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
 ******************************************************************************
 *
 * $Log$
 * Revision 1.1  2004/10/08 20:48:12  fwarmerdam
 * New
 *
 */

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

//    nFeatureCount = PQntuples(hInitialResultIn);

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
            poDefn->AddFieldDefn( &oField );
            break;

          case FIELD_TYPE_DECIMAL:
          case FIELD_TYPE_FLOAT:
          case FIELD_TYPE_DOUBLE:
            oField.SetType( OFTReal );
            poDefn->AddFieldDefn( &oField );
            break;

          case FIELD_TYPE_STRING:
          case FIELD_TYPE_TIMESTAMP:
          case FIELD_TYPE_DATE:
          case FIELD_TYPE_TIME:
          case FIELD_TYPE_DATETIME:
          case FIELD_TYPE_YEAR:
          case FIELD_TYPE_VAR_STRING:
          case FIELD_TYPE_BLOB:
            oField.SetType( OFTString );
            poDefn->AddFieldDefn( &oField );
            // should add max length info if available.
            break;

          default:
            // any other field we ignore. 
            break;
        }
    }

    poDefn->SetGeomType( wkbNone );

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

    /* Eventually we should consider trying to "insert" the spatial component
       of the query if possible within a SELECT, but for now we just use
       the raw query directly. */

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
    return OGRMySQLLayer::GetFeatureCount( bForce );
}
