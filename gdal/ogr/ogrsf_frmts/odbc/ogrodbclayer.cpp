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
 ****************************************************************************/

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
    panFieldOrdinals = NULL;

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
        CPLDebug( "OGR_ODBC", "%d features read on layer '%s'.",
                  (int) m_nFeaturesRead, 
                  poFeatureDefn->GetName() );
    }

    if( poStmt )
    {
        delete poStmt;
        poStmt = NULL;
    }

    if( pszGeomColumn )
        CPLFree( pszGeomColumn );

    if ( panFieldOrdinals )
        CPLFree( panFieldOrdinals );

    if( poFeatureDefn )
    {
        poFeatureDefn->Release();
        poFeatureDefn = NULL;
    }

    if( poSRS )
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
    SetDescription( poFeatureDefn->GetName() );
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

        switch( CPLODBCStatement::GetTypeMapping(poStmt->GetColType(iCol)) )
        {
            case SQL_C_SSHORT:
            case SQL_C_USHORT:
            case SQL_C_SLONG:
            case SQL_C_ULONG:
                oField.SetType( OFTInteger );
                break;

            case SQL_C_BINARY:
                oField.SetType( OFTBinary );
                break;

            case SQL_C_NUMERIC:
                oField.SetType( OFTReal );
                oField.SetPrecision( poStmt->GetColPrecision(iCol) );
                break;

            case SQL_C_FLOAT:
            case SQL_C_DOUBLE:
                oField.SetType( OFTReal );
                oField.SetWidth( 0 );
                break;

            case SQL_C_DATE:
                oField.SetType( OFTDate );
                break;

            case SQL_C_TIME:
                oField.SetType( OFTTime );
                break;

            case SQL_C_TIMESTAMP:
                oField.SetType( OFTDateTime );
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
        OGRErr eErr = OGRERR_NONE;

        if( pszGeomText != NULL && !bGeomColumnWKB )
        {
            eErr =
                OGRGeometryFactory::createFromWkt((char **) &pszGeomText,
                                                  NULL, &poGeom);
        }
        else if( pszGeomText != NULL && bGeomColumnWKB )
        {
            int nLength = poStmt->GetColDataLength( iField );

            eErr =
                OGRGeometryFactory::createFromWkb((unsigned char *) pszGeomText,
                                                  NULL, &poGeom, nLength);
        }
        
        if ( eErr != OGRERR_NONE )
        {
            const char *pszMessage;

            switch ( eErr )
            {
                case OGRERR_NOT_ENOUGH_DATA:
                    pszMessage = "Not enough data to deserialize";
                    break;
                case OGRERR_UNSUPPORTED_GEOMETRY_TYPE:
                    pszMessage = "Unsupported geometry type";
                    break;
                case OGRERR_CORRUPT_DATA:
                    pszMessage = "Corrupt data";
                    break;
                default:
                    pszMessage = "Unrecognized error";
            }
            CPLError(CE_Failure, CPLE_AppDefined,
                     "GetNextRawFeature(): %s", pszMessage);
        }

        if( poGeom != NULL )
            poFeature->SetGeometryDirectly( poGeom );
    }

    return poFeature;
}

/************************************************************************/
/*                             GetFeature()                             */
/************************************************************************/

OGRFeature *OGRODBCLayer::GetFeature( GIntBig nFeatureId )

{
    /* This should be implemented directly! */

    return OGRLayer::GetFeature( nFeatureId );
}

/************************************************************************/
/*                           TestCapability()                           */
/************************************************************************/

int OGRODBCLayer::TestCapability( CPL_UNUSED const char * pszCap )
{
    return FALSE;
}

/************************************************************************/
/*                           GetSpatialRef()                            */
/************************************************************************/

OGRSpatialReference *OGRODBCLayer::GetSpatialRef()

{
    return poSRS;
}
