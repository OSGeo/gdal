/******************************************************************************
 * $Id$
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  Implements OGRODBCLayer class, code shared between 
 *           the direct table access, and the generic SQL results.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2003, Frank Warmerdam <warmerdam@pobox.com>
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
 * Revision 1.13  2005/11/18 16:50:46  fwarmerdam
 * added ODBC_OGR_FID config variable
 *
 * Revision 1.12  2005/09/21 00:56:55  fwarmerdam
 * fixup OGRFeatureDefn and OGRSpatialReference refcount handling
 *
 * Revision 1.11  2005/09/09 05:02:12  fwarmerdam
 * added wkb support
 *
 * Revision 1.10  2005/09/05 19:56:42  fwarmerdam
 * added binary field support
 *
 * Revision 1.9  2005/03/21 20:36:31  fwarmerdam
 * set precision for decimal fields - MS bug 1255
 *
 * Revision 1.8  2005/02/22 12:53:56  fwarmerdam
 * use OGRLayer base spatial filter support
 *
 * Revision 1.7  2005/02/02 20:54:27  fwarmerdam
 * track m_nFeaturesRead
 *
 * Revision 1.6  2004/03/04 17:16:05  warmerda
 * Cleanup featuredefn on exit.
 *
 * Revision 1.5  2004/01/05 22:38:17  warmerda
 * stripped out some junk
 *
 * Revision 1.4  2003/10/29 16:47:38  warmerda
 * Improved width/precision handling for real numbers.
 *
 * Revision 1.3  2003/10/08 16:00:16  warmerda
 * provide default GetFeature(int) implementation
 *
 * Revision 1.2  2003/09/26 20:03:03  warmerda
 * initialize pszFIDColumn
 *
 * Revision 1.1  2003/09/25 17:08:37  warmerda
 * New
 *
 */

#include "cpl_conv.h"
#include "ogr_odbc.h"
#include "cpl_string.h"

CPL_CVSID("$Id$");


/************************************************************************/
/*                            OGRODBCLayer()                            */
/************************************************************************/

OGRODBCLayer::OGRODBCLayer()

{
    poDS = NULL;

    bGeomColumnWKB = FALSE;
    pszGeomColumn = NULL;
    pszFIDColumn = NULL;

    poStmt = NULL;

    iNextShapeId = 0;

    poSRS = NULL;
    nSRSId = -2; // we haven't even queried the database for it yet. 
}

/************************************************************************/
/*                            ~OGRODBCLayer()                             */
/************************************************************************/

OGRODBCLayer::~OGRODBCLayer()

{
    if( m_nFeaturesRead > 0 && poFeatureDefn != NULL )
    {
        CPLDebug( "ODBC", "%d features read on layer '%s'.",
                  (int) m_nFeaturesRead, 
                  poFeatureDefn->GetName() );
    }

    if( poStmt != NULL )
    {
        delete poStmt;
        poStmt = NULL;
    }

    if( pszGeomColumn != NULL )
        CPLFree( pszGeomColumn );

    if( poFeatureDefn != NULL )
    {
        poFeatureDefn->Release();
        poFeatureDefn = NULL;
    }

    if( poSRS != NULL )
        poSRS->Release();
}

/************************************************************************/
/*                          BuildFeatureDefn()                          */
/*                                                                      */
/*      Build feature definition from a set of column definitions       */
/*      set on a statement.  Sift out geometry and FID fields.          */
/************************************************************************/

CPLErr OGRODBCLayer::BuildFeatureDefn( const char *pszLayerName, 
                                    CPLODBCStatement *poStmt )

{
    poFeatureDefn = new OGRFeatureDefn( pszLayerName );
    int    nRawColumns = poStmt->GetColCount();

    poFeatureDefn->Reference();

    panFieldOrdinals = (int *) CPLMalloc( sizeof(int) * nRawColumns );

    for( int iCol = 0; iCol < nRawColumns; iCol++ )
    {
        OGRFieldDefn    oField( poStmt->GetColName(iCol), OFTString );

        oField.SetWidth( MAX(0,poStmt->GetColSize( iCol )) );

        if( pszGeomColumn != NULL 
            && EQUAL(poStmt->GetColName(iCol),pszGeomColumn) )
            continue;
        
        switch( poStmt->GetColType(iCol) )
        {
          case SQL_INTEGER:
            oField.SetType( OFTInteger );
            break;

          case SQL_BINARY:
          case SQL_VARBINARY:
          case SQL_LONGVARBINARY:
            oField.SetType( OFTBinary );
            break;

          case SQL_DECIMAL:
            oField.SetType( OFTReal );
            oField.SetPrecision( poStmt->GetColPrecision(iCol) );
            break;

          case SQL_FLOAT:
          case SQL_REAL:
          case SQL_DOUBLE:
            oField.SetType( OFTReal );
            oField.SetWidth( 0 );
            break;

          default:
            /* leave it as OFTString */;
        }

        poFeatureDefn->AddFieldDefn( &oField );
        panFieldOrdinals[poFeatureDefn->GetFieldCount() - 1] = iCol+1;
    }

/* -------------------------------------------------------------------- */
/*      If we don't already have an FID, check if there is a special    */
/*      FID named column available.                                     */
/* -------------------------------------------------------------------- */
    if( pszFIDColumn == NULL )
    {
        const char *pszOGR_FID = CPLGetConfigOption("ODBC_OGR_FID","OGR_FID");
        if( poFeatureDefn->GetFieldIndex( pszOGR_FID ) != -1 )
            pszFIDColumn = CPLStrdup(pszOGR_FID);
    }

    if( pszFIDColumn != NULL )
        CPLDebug( "OGR_ODBC", "Using column %s as FID for table %s.",
                  pszFIDColumn, poFeatureDefn->GetName() );
    else
        CPLDebug( "OGR_ODBC", "Table %s has no identified FID column.",
                  poFeatureDefn->GetName() );

    return CE_None;
}


/************************************************************************/
/*                            ResetReading()                            */
/************************************************************************/

void OGRODBCLayer::ResetReading()

{
    iNextShapeId = 0;
}

/************************************************************************/
/*                           GetNextFeature()                           */
/************************************************************************/

OGRFeature *OGRODBCLayer::GetNextFeature()

{
    for( ; TRUE; )
    {
        OGRFeature      *poFeature;

        poFeature = GetNextRawFeature();
        if( poFeature == NULL )
            return NULL;

        if( (m_poFilterGeom == NULL
            || FilterGeometry( poFeature->GetGeometryRef() ) )
            && (m_poAttrQuery == NULL
                || m_poAttrQuery->Evaluate( poFeature )) )
            return poFeature;

        delete poFeature;
    }
}

/************************************************************************/
/*                         GetNextRawFeature()                          */
/************************************************************************/

OGRFeature *OGRODBCLayer::GetNextRawFeature()

{
    if( GetStatement() == NULL )
        return NULL;

/* -------------------------------------------------------------------- */
/*      If we are marked to restart then do so, and fetch a record.     */
/* -------------------------------------------------------------------- */
    if( !poStmt->Fetch() )
    {
        delete poStmt;
        poStmt = NULL;
        return NULL;
    }

/* -------------------------------------------------------------------- */
/*      Create a feature from the current result.                       */
/* -------------------------------------------------------------------- */
    int         iField;
    OGRFeature *poFeature = new OGRFeature( poFeatureDefn );

    if( pszFIDColumn != NULL && poStmt->GetColId(pszFIDColumn) > -1 )
        poFeature->SetFID( 
            atoi(poStmt->GetColData(poStmt->GetColId(pszFIDColumn))) );
    else
        poFeature->SetFID( iNextShapeId );

    iNextShapeId++;
    m_nFeaturesRead++;

/* -------------------------------------------------------------------- */
/*      Set the fields.                                                 */
/* -------------------------------------------------------------------- */
    for( iField = 0; iField < poFeatureDefn->GetFieldCount(); iField++ )
    {
        int iSrcField = panFieldOrdinals[iField]-1;
        const char *pszValue = poStmt->GetColData( iSrcField );

        if( pszValue == NULL )
            /* no value */;
        else if( poFeature->GetFieldDefnRef(iField)->GetType() == OFTBinary )
            poFeature->SetField( iField, 
                                 poStmt->GetColDataLength(iSrcField),
                                 (GByte *) pszValue );
        else
            poFeature->SetField( iField, pszValue );
    }

/* -------------------------------------------------------------------- */
/*      Try to extract a geometry.                                      */
/* -------------------------------------------------------------------- */
    if( pszGeomColumn != NULL )
    {
        int iField = poStmt->GetColId( pszGeomColumn );
        const char *pszGeomText = poStmt->GetColData( iField );
        OGRGeometry *poGeom = NULL;

        if( pszGeomText != NULL && !bGeomColumnWKB )
        {
            OGRGeometryFactory::createFromWkt( (char **) &pszGeomText,
                                               NULL, &poGeom );
        }
        else if( pszGeomText != NULL && bGeomColumnWKB )
        {
            int nLength = poStmt->GetColDataLength( iField );

            OGRGeometryFactory::createFromWkb( (unsigned char *) pszGeomText,
                                               NULL, &poGeom, nLength );
        }
        
        if( poGeom != NULL )
            poFeature->SetGeometryDirectly( poGeom );
    }

    return poFeature;
}

/************************************************************************/
/*                             GetFeature()                             */
/************************************************************************/

OGRFeature *OGRODBCLayer::GetFeature( long nFeatureId )

{
    /* This should be implemented directly! */

    return OGRLayer::GetFeature( nFeatureId );
}

/************************************************************************/
/*                           TestCapability()                           */
/************************************************************************/

int OGRODBCLayer::TestCapability( const char * pszCap )

{
    if( EQUAL(pszCap,OLCRandomRead) )
        return FALSE;

    else if( EQUAL(pszCap,OLCFastFeatureCount) )
        return FALSE;

    else if( EQUAL(pszCap,OLCFastSpatialFilter) )
        return FALSE;

    else if( EQUAL(pszCap,OLCTransactions) )
        return FALSE;

    else 
        return FALSE;
}

/************************************************************************/
/*                           GetSpatialRef()                            */
/************************************************************************/

OGRSpatialReference *OGRODBCLayer::GetSpatialRef()

{
    return poSRS;
}
