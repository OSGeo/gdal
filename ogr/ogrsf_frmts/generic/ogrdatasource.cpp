/******************************************************************************
 * $Id$
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  The generic portions of the OGRDataSource class.
 * Author:   Frank Warmerdam, warmerda@home.com
 *
 ******************************************************************************
 * Copyright (c) 1999,  Les Technologies SoftMap Inc.
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
 * Revision 1.6  2002/04/29 19:35:50  warmerda
 * fixes for selecting FID
 *
 * Revision 1.5  2002/04/25 03:42:04  warmerda
 * fixed spatial filter support on SQL results
 *
 * Revision 1.4  2002/04/25 02:24:45  warmerda
 * added ExecuteSQL method
 *
 * Revision 1.3  2001/07/18 04:55:16  warmerda
 * added CPL_CSVID
 *
 * Revision 1.2  2000/08/21 16:37:43  warmerda
 * added constructor, and initialization of styletable
 *
 * Revision 1.1  1999/11/04 21:10:51  warmerda
 * New
 *
 */

#include "ogrsf_frmts.h"
#include "ogr_p.h"
#include "ogr_gensql.h"

CPL_C_START
#include "swq.h"
CPL_C_END

CPL_CVSID("$Id$");

/************************************************************************/
/*                           ~OGRDataSource()                           */
/************************************************************************/

OGRDataSource::OGRDataSource()

{
    m_poStyleTable = NULL;
}

/************************************************************************/
/*                           ~OGRDataSource()                           */
/************************************************************************/

OGRDataSource::~OGRDataSource()

{
}

/************************************************************************/
/*                            CreateLayer()                             */
/************************************************************************/

OGRLayer *OGRDataSource::CreateLayer( const char * pszName,
                                      OGRSpatialReference * poSpatialRef,
                                      OGRwkbGeometryType eType,
                                      char ** )

{
    CPLError( CE_Failure, CPLE_NotSupported,
              "CreateLayer() not supported by this data source.\n" );
              
    return NULL;
}

/************************************************************************/
/*                             ExecuteSQL()                             */
/************************************************************************/

OGRLayer * OGRDataSource::ExecuteSQL( const char *pszSQLCommand,
                                      OGRGeometry *poSpatialFilter,
                                      const char *pszDialect )

{
    const char *pszError;
    swq_select *psSelectInfo = NULL;
    
/* -------------------------------------------------------------------- */
/*      Preparse the SQL statement.                                     */
/* -------------------------------------------------------------------- */
    pszError = swq_select_preparse( pszSQLCommand, &psSelectInfo );
    if( pszError != NULL )
    {
        CPLError( CE_Failure, CPLE_AppDefined, 
                  "SQL: %s", pszError );
        return NULL;
    }

/* -------------------------------------------------------------------- */
/*      Identify the layer (table) being operated on.                   */
/* -------------------------------------------------------------------- */
    OGRLayer *poSrcLayer = NULL;

    for( int iLayer = 0; iLayer < GetLayerCount(); iLayer++ )
    {
        if( EQUAL(GetLayer(iLayer)->GetLayerDefn()->GetName(),
                  psSelectInfo->from_table) )
        {
            poSrcLayer = GetLayer(iLayer);
            break;
        }
    }

    if( poSrcLayer == NULL )
    {
        swq_select_free( psSelectInfo );
        CPLError( CE_Failure, CPLE_AppDefined, 
                  "SELECT from table %s failed, no such table/featureclass.",
                  psSelectInfo->from_table );
        return NULL;
    }
    
/* -------------------------------------------------------------------- */
/*      Build the field list.                                           */
/* -------------------------------------------------------------------- */
    int  nFieldCount = poSrcLayer->GetLayerDefn()->GetFieldCount();
    char **papszFieldList;
    swq_field_type *paeFieldType;
    
    papszFieldList = (char **) CPLMalloc( sizeof(char *) * (nFieldCount+1) );
    paeFieldType = (swq_field_type *)  
        CPLMalloc( sizeof(swq_field_type) * (nFieldCount+1) );
    
    for( int iField = 0; iField < nFieldCount; iField++ )
    {
        OGRFieldDefn *poFDefn=poSrcLayer->GetLayerDefn()->GetFieldDefn(iField);
        papszFieldList[iField] = (char *) poFDefn->GetNameRef();
        if( poFDefn->GetType() == OFTInteger )
            paeFieldType[iField] = SWQ_INTEGER;
        else if( poFDefn->GetType() == OFTReal )
            paeFieldType[iField] = SWQ_FLOAT;
        else if( poFDefn->GetType() == OFTString )
            paeFieldType[iField] = SWQ_STRING;
        else
            paeFieldType[iField] = SWQ_OTHER;
    }

/* -------------------------------------------------------------------- */
/*      Expand '*' in 'SELECT *' now before we add the pseudo field     */
/*      'FID'.                                                          */
/* -------------------------------------------------------------------- */
    pszError = 
        swq_select_expand_wildcard( psSelectInfo, nFieldCount, papszFieldList);

    if( pszError != NULL )
    {
        CPLError( CE_Failure, CPLE_AppDefined, 
                  "SQL: %s", pszError );
        return NULL;
    }

    papszFieldList[nFieldCount] = "FID";
    paeFieldType[nFieldCount++] = SWQ_INTEGER;

/* -------------------------------------------------------------------- */
/*      Finish the parse operation.                                     */
/* -------------------------------------------------------------------- */
    
    pszError = swq_select_parse( psSelectInfo, nFieldCount, papszFieldList,
                                 paeFieldType, 0 );

    CPLFree( papszFieldList );
    CPLFree( paeFieldType );

    if( pszError != NULL )
    {
        CPLError( CE_Failure, CPLE_AppDefined, 
                  "SQL: %s", pszError );
        return NULL;
    }

/* -------------------------------------------------------------------- */
/*      Everything seems OK, try to instantiate a results layer.        */
/* -------------------------------------------------------------------- */
    OGRGenSQLResultsLayer *poResults;

    poResults = new OGRGenSQLResultsLayer( this, psSelectInfo, 
                                           poSpatialFilter );

    // Eventually, we should keep track of layers to cleanup.

    return poResults;
}

/************************************************************************/
/*                          ReleaseResultSet()                          */
/************************************************************************/

void OGRDataSource::ReleaseResultSet( OGRLayer * poLayer )

{
    delete poLayer;
}
