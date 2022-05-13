/******************************************************************************
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

CPL_CVSID("$Id$")

/************************************************************************/
/*                            OGRODBCLayer()                            */
/************************************************************************/

OGRODBCLayer::OGRODBCLayer() :
    poFeatureDefn(nullptr),
    poStmt(nullptr),
    poSRS(nullptr),
    nSRSId(-2),  // Have not queried the database for it yet.
    iNextShapeId(0),
    poDS(nullptr),
    bGeomColumnWKB(FALSE),
    pszGeomColumn(nullptr),
    pszFIDColumn(nullptr),
    panFieldOrdinals(nullptr)
{}

/************************************************************************/
/*                            ~OGRODBCLayer()                             */
/************************************************************************/

OGRODBCLayer::~OGRODBCLayer()

{
    if( m_nFeaturesRead > 0 && poFeatureDefn != nullptr )
    {
        CPLDebug( "OGR_ODBC", "%d features read on layer '%s'.",
                  (int) m_nFeaturesRead,
                  poFeatureDefn->GetName() );
    }

    if( poStmt )
    {
        delete poStmt;
        poStmt = nullptr;
    }

    if( pszGeomColumn )
        CPLFree( pszGeomColumn );

    if ( panFieldOrdinals )
        CPLFree( panFieldOrdinals );

    if( poFeatureDefn )
    {
        poFeatureDefn->Release();
        poFeatureDefn = nullptr;
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
                                    CPLODBCStatement *poStmtIn )

{
    poFeatureDefn = new OGRFeatureDefn( pszLayerName );
    SetDescription( poFeatureDefn->GetName() );
    int    nRawColumns = poStmtIn->GetColCount();

    poFeatureDefn->Reference();

    panFieldOrdinals = (int *) CPLMalloc( sizeof(int) * nRawColumns );

    for( int iCol = 0; iCol < nRawColumns; iCol++ )
    {
        OGRFieldDefn    oField( poStmtIn->GetColName(iCol), OFTString );

        oField.SetWidth( MAX(0,poStmtIn->GetColSize( iCol )) );

        if( pszGeomColumn != nullptr
            && EQUAL(poStmtIn->GetColName(iCol),pszGeomColumn) )
            continue;

        switch( CPLODBCStatement::GetTypeMapping(poStmtIn->GetColType(iCol)) )
        {
            case SQL_C_SSHORT:
            case SQL_C_USHORT:
            case SQL_C_SLONG:
            case SQL_C_ULONG:
                oField.SetType( OFTInteger );
                break;

            case SQL_C_SBIGINT:
            case SQL_C_UBIGINT:
                oField.SetType( OFTInteger64 );
                break;

            case SQL_C_BINARY:
                oField.SetType( OFTBinary );
                break;

            case SQL_C_NUMERIC:
                oField.SetType( OFTReal );
                oField.SetPrecision( poStmtIn->GetColPrecision(iCol) );
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
            case SQL_C_TYPE_TIMESTAMP:
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
    if( pszFIDColumn == nullptr )
    {
        const char *pszOGR_FID = CPLGetConfigOption("ODBC_OGR_FID","OGR_FID");
        if( poFeatureDefn->GetFieldIndex( pszOGR_FID ) != -1 )
            pszFIDColumn = CPLStrdup(pszOGR_FID);
    }

    if( pszFIDColumn != nullptr )
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
    m_bEOF = false;
}

/************************************************************************/
/*                           GetNextFeature()                           */
/************************************************************************/

OGRFeature *OGRODBCLayer::GetNextFeature()

{
    while( true )
    {
        OGRFeature *poFeature = GetNextRawFeature();
        if( poFeature == nullptr )
            return nullptr;

        if( (m_poFilterGeom == nullptr
            || FilterGeometry( poFeature->GetGeometryRef() ) )
            && (m_poAttrQuery == nullptr
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
    if( m_bEOF || GetStatement() == nullptr )
        return nullptr;

/* -------------------------------------------------------------------- */
/*      If we are marked to restart then do so, and fetch a record.     */
/* -------------------------------------------------------------------- */
    if( !poStmt->Fetch() )
    {
        delete poStmt;
        poStmt = nullptr;
        m_bEOF = true;
        return nullptr;
    }

/* -------------------------------------------------------------------- */
/*      Create a feature from the current result.                       */
/* -------------------------------------------------------------------- */
    OGRFeature *poFeature = new OGRFeature( poFeatureDefn );

    if( pszFIDColumn != nullptr && poStmt->GetColId(pszFIDColumn) > -1 )
        poFeature->SetFID(
            atoi(poStmt->GetColData(poStmt->GetColId(pszFIDColumn))) );
    else
        poFeature->SetFID( iNextShapeId );

    iNextShapeId++;
    m_nFeaturesRead++;

/* -------------------------------------------------------------------- */
/*      Set the fields.                                                 */
/* -------------------------------------------------------------------- */
    for( int iField = 0; iField < poFeatureDefn->GetFieldCount(); iField++ )
    {
        const OGRFieldType eType = poFeatureDefn->GetFieldDefn(iField)->GetType();
        int iSrcField = panFieldOrdinals[iField]-1;

        if ( eType == OFTReal && (poStmt->Flags() & CPLODBCStatement::Flag::RetrieveNumericColumnsAsDouble) )
        {
            // for OFTReal fields we retrieve the value directly as a double
            // to avoid loss of precision associated with double/float->string conversion
            const double dfValue = poStmt->GetColDataAsDouble( iSrcField );
            if ( std::isnan( dfValue ) )
            {
                poFeature->SetFieldNull( iField );
            }
            else
            {
                poFeature->SetField( iField, dfValue );
            }
        }
        else
        {
            const char *pszValue = poStmt->GetColData( iSrcField );

            if( pszValue == nullptr )
                poFeature->SetFieldNull( iField );
            else if( poFeature->GetFieldDefnRef(iField)->GetType() == OFTBinary )
                poFeature->SetField( iField,
                                     poStmt->GetColDataLength(iSrcField),
                                     (GByte *) pszValue );
            else
                poFeature->SetField( iField, pszValue );
        }
    }

/* -------------------------------------------------------------------- */
/*      Try to extract a geometry.                                      */
/* -------------------------------------------------------------------- */
    if( pszGeomColumn != nullptr )
    {
        int iField = poStmt->GetColId( pszGeomColumn );
        const char *pszGeomText = poStmt->GetColData( iField );
        OGRGeometry *poGeom = nullptr;
        OGRErr eErr = OGRERR_NONE;

        if( pszGeomText != nullptr && !bGeomColumnWKB )
        {
            eErr =
                OGRGeometryFactory::createFromWkt(pszGeomText,
                                                  nullptr, &poGeom);
        }
        else if( pszGeomText != nullptr && bGeomColumnWKB )
        {
            int nLength = poStmt->GetColDataLength( iField );

            eErr =
                OGRGeometryFactory::createFromWkb(pszGeomText,
                                                  nullptr, &poGeom, nLength);
        }

        if ( eErr != OGRERR_NONE )
        {
            const char *pszMessage = nullptr;

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

        if( poGeom != nullptr )
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
