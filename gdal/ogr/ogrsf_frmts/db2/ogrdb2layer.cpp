/*****************************************************************************
 *
 * Project:  DB2 Spatial driver
 * Purpose:  Definition of classes for OGR DB2 Spatial driver.
 * Author:   David Adler, dadler at adtechgeospatial dot com
 *
 *****************************************************************************
 * Copyright (c) 2010, Tamas Szekeres
 * Copyright (c) 2015, David Adler
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

#include "ogr_db2.h"

CPL_CVSID("$Id$")

/************************************************************************/
/*                        OGRDB2Layer()                        */
/************************************************************************/

OGRDB2Layer::OGRDB2Layer()

{
    m_poDS = nullptr;
    poFeatureDefn = nullptr;
    poDS = nullptr;
    pszGeomColumn = nullptr;
    pszFIDColumn = nullptr;
    bIsIdentityFid = FALSE;
    cGenerated = ' ';
    nLayerStatus = 0;
    panFieldOrdinals = nullptr;
    m_poStmt = nullptr;
    m_poPrepStmt = nullptr;
    iNextShapeId = 0;
    poSRS = nullptr;
    nSRSId = -1; // we haven't even queried the database for it yet.
}

/************************************************************************/
/*                      ~OGRDB2Layer()                         */
/************************************************************************/

OGRDB2Layer::~OGRDB2Layer()

{
    CPLDebug("OGRDB2Layer::~OGRDB2Layer","entering");
    CPLDebug("OGRDB2Layer::~OGRDB2Layer",
             "m_nFeaturesRead: " CPL_FRMT_GIB "; poFeatureDefn: %p",
             m_nFeaturesRead,poFeatureDefn);
    if( m_nFeaturesRead > 0 && poFeatureDefn != nullptr )
    {
        CPLDebug( "OGR_DB2Layer",
                  "%d features read on layer '%s'.",
                  (int) m_nFeaturesRead,
                  poFeatureDefn->GetName() );
    }

    if( m_poStmt )
    {
        delete m_poStmt;
        m_poStmt = nullptr;
    }

    CPLFree( pszGeomColumn );
    CPLFree( pszFIDColumn );
    CPLFree( panFieldOrdinals );

    if( poFeatureDefn )
    {
        poFeatureDefn->Release();
        poFeatureDefn = nullptr;
    }

    if( poSRS )
        poSRS->Release();
    CPLDebug("OGRDB2Layer::~OGRDB2Layer","exiting") ;
}

/************************************************************************/
/*                          BuildFeatureDefn()                          */
/*                                                                      */
/*      Build feature definition from a set of column definitions       */
/*      set on a statement.  Sift out geometry and FID fields.          */
/************************************************************************/

CPLErr OGRDB2Layer::BuildFeatureDefn( const char *pszLayerName,
                                      OGRDB2Statement *poStmt )

{
    poFeatureDefn = new OGRFeatureDefn( pszLayerName );
    int    nRawColumns = poStmt->GetColCount();
    CPLDebug( "OGR_DB2Layer::BuildFeatureDefn",
              "pszLayerName: '%s'; pszGeomColumn: '%s'",
              pszLayerName, pszGeomColumn);
    poFeatureDefn->Reference();

    CPLFree(panFieldOrdinals);
    panFieldOrdinals = (int *) CPLMalloc( sizeof(int) * nRawColumns );

    /* -------------------------------------------------------------------- */
    /*      If we don't already have an FID, check if there is a special    */
    /*      FID named column available.                                     */
    /* -------------------------------------------------------------------- */
    if( pszFIDColumn == nullptr )
    {
        const char *pszOGR_FID = CPLGetConfigOption("DB2SPATIAL_OGR_FID",
                                 "OBJECTID");
        for( int iCol = 0; iCol < nRawColumns; iCol++ )
        {
            if( EQUAL(poStmt->GetColName(iCol),pszOGR_FID) )
            {
                pszFIDColumn = CPLStrdup(pszOGR_FID);
                break;
            }
        }
    }

    if( pszFIDColumn != nullptr )
        CPLDebug( "OGR_DB2Layer::BuildFeatureDefn",
                  "Using column %s as FID for table %s.",
                  pszFIDColumn, poFeatureDefn->GetName() );
    else
        CPLDebug( "OGR_DB2Layer::BuildFeatureDefn",
                  "Table %s has no identified FID column.",
                  poFeatureDefn->GetName() );

    for( int iCol = 0; iCol < nRawColumns; iCol++ )
    {
        if ( pszGeomColumn == nullptr )
        {
            /* need to identify the geometry column */
            if ( EQUAL(poStmt->GetColTypeName( iCol ),
                       "VARCHAR () FOR BIT DATA") )
            {
                pszGeomColumn = CPLStrdup( poStmt->GetColName(iCol) );
                continue;
            }
        }
        else
        {
            if( EQUAL(poStmt->GetColName(iCol),pszGeomColumn) ) {
                continue;
            }
        }
        if( pszFIDColumn != nullptr &&
                EQUAL(poStmt->GetColName(iCol), pszFIDColumn) )
        {
            /* skip FID */
            continue;
        }
        // default field type is string
        OGRFieldDefn    oField( poStmt->GetColName(iCol), OFTString );
        oField.SetWidth( MAX(0,poStmt->GetColSize( iCol )) );

        int colType = poStmt->GetColType(iCol);
        switch( colType )
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
            /* leave it as OFTString */
            ;
        }

        poFeatureDefn->AddFieldDefn( &oField );
        panFieldOrdinals[poFeatureDefn->GetFieldCount() - 1] = iCol;
    }

    return CE_None;
}

/************************************************************************/
/*                            ResetReading()                            */
/************************************************************************/

void OGRDB2Layer::ResetReading()

{
    iNextShapeId = 0;
    CPLDebug("OGR_DB2Layer::ResetReading","Reset");
}

/************************************************************************/
/*                           GetNextFeature()                           */
/************************************************************************/

OGRFeature *OGRDB2Layer::GetNextFeature()

{
    while( TRUE )
    {
        OGRFeature      *poFeature;

        poFeature = GetNextRawFeature();
        if( poFeature == nullptr )
            return nullptr;
        if( (m_poFilterGeom == nullptr
                || FilterGeometry( poFeature->GetGeometryRef() ) )
                && (m_poAttrQuery == nullptr
                    || m_poAttrQuery->Evaluate( poFeature )) ) {
            return poFeature;
        }

        delete poFeature;
    }
}

/************************************************************************/
/*                         GetNextRawFeature()                          */
/************************************************************************/

OGRFeature *OGRDB2Layer::GetNextRawFeature()

{

    if( GetStatement() == nullptr )
        return nullptr;

    /* -------------------------------------------------------------------- */
    /*      If we are marked to restart then do so, and fetch a record.     */
    /* -------------------------------------------------------------------- */
    if( !m_poStmt->Fetch() )  // fail is normal for final fetch
    {
//      CPLDebug("OGR_DB2Layer::GetNextRawFeature","Fetch failed");
        delete m_poStmt;
        m_poStmt = nullptr;
        return nullptr;
    }
//      CPLDebug("OGR_DB2Layer::GetNextRawFeature","Create feature");
    /* -------------------------------------------------------------------- */
    /*      Create a feature from the current result.                       */
    /* -------------------------------------------------------------------- */
    int         iField;
    OGRFeature *poFeature = new OGRFeature( poFeatureDefn );
    if( pszFIDColumn != nullptr && m_poStmt->GetColId(pszFIDColumn) > -1 )
        poFeature->SetFID(
            atoi(m_poStmt->GetColData(m_poStmt->GetColId(pszFIDColumn))) );
    else
        poFeature->SetFID( iNextShapeId );

    iNextShapeId++;
    m_nFeaturesRead++;

    /* -------------------------------------------------------------------- */
    /*      Set the fields.                                                 */
    /* -------------------------------------------------------------------- */

    for( iField = 0; iField < poFeatureDefn->GetFieldCount(); iField++ )
    {
        if ( poFeatureDefn->GetFieldDefn(iField)->IsIgnored() )
            continue;

        int iSrcField = panFieldOrdinals[iField];
        const char *pszValue = m_poStmt->GetColData( iSrcField );

        if( pszValue == nullptr )
            poFeature->SetFieldNull( iField );
        else if( poFeature->GetFieldDefnRef(iField)->GetType() == OFTBinary )
            poFeature->SetField( iField,
                                 m_poStmt->GetColDataLength(iSrcField),
                                 (GByte *) pszValue );
        else
            poFeature->SetField( iField, pszValue );
    }

    /* -------------------------------------------------------------------- */
    /*      Try to extract a geometry.                                      */
    /* -------------------------------------------------------------------- */

    if( pszGeomColumn != nullptr && !poFeatureDefn->IsGeometryIgnored())
    {
        iField = m_poStmt->GetColId( pszGeomColumn );
        const char *pszGeomText = m_poStmt->GetColData( iField );
        OGRGeometry *poGeom = nullptr;
        OGRErr eErr = OGRERR_NONE;
        if( pszGeomText != nullptr )
        {
            eErr = OGRGeometryFactory::createFromWkt(pszGeomText,
                    nullptr, &poGeom);
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

        if( poGeom != nullptr )
        {
            if ( GetSpatialRef() )
                poGeom->assignSpatialReference( poSRS );

            poFeature->SetGeometryDirectly( poGeom );
        }
    }

    return poFeature;
}

/************************************************************************/
/*                             GetFeature()                             */
/************************************************************************/

OGRFeature *OGRDB2Layer::GetFeature( GIntBig nFeatureId )

{
    /* This should be implemented directly! */

    return OGRLayer::GetFeature( nFeatureId );
}

/************************************************************************/
/*                           TestCapability()                           */
/************************************************************************/

int OGRDB2Layer::TestCapability( CPL_UNUSED const char * pszCap )
{
    return FALSE;
}

/************************************************************************/
/*                          StartTransaction()                          */
/************************************************************************/

OGRErr OGRDB2Layer::StartTransaction()

{
    poDS->GetSession()->BeginTransaction();
    return OGRERR_NONE;
}

/************************************************************************/
/*                         CommitTransaction()                          */
/************************************************************************/

OGRErr OGRDB2Layer::CommitTransaction()

{
    poDS->GetSession()->CommitTransaction();
    return OGRERR_NONE;
}

/************************************************************************/
/*                        RollbackTransaction()                         */
/************************************************************************/

OGRErr OGRDB2Layer::RollbackTransaction()

{
    poDS->GetSession()->RollbackTransaction();
    return OGRERR_NONE;
}

/************************************************************************/
/*                           GetSpatialRef()                            */
/************************************************************************/

OGRSpatialReference *OGRDB2Layer::GetSpatialRef()

{
//CPLDebug("OGR_DB2Layer::GetSpatialRef", "nSrsId: %d", nSRSId);
    if( poSRS == nullptr && nSRSId > 0 )
    {
        poSRS = poDS->FetchSRS( nSRSId );
        if( poSRS != nullptr )
            poSRS->Reference();
        else
            nSRSId = 0;
    }

    return poSRS;
}

/************************************************************************/
/*                            GetFIDColumn()                            */
/************************************************************************/

const char *OGRDB2Layer::GetFIDColumn()

{
    GetLayerDefn();

    if( pszFIDColumn != nullptr )
        return pszFIDColumn;
    else
        return "";
}

/************************************************************************/
/*                         GetGeometryColumn()                          */
/************************************************************************/

const char *OGRDB2Layer::GetGeometryColumn()

{
    GetLayerDefn();

    if( pszGeomColumn != nullptr )
        return pszGeomColumn;
    else
        return "";
}

/************************************************************************/
/*                        GByteArrayToHexString()                       */
/************************************************************************/

char* OGRDB2Layer::GByteArrayToHexString( const GByte* pabyData, int nLen)
{
    char* pszTextBuf;
    const size_t nBufLen = nLen*2+3;

    pszTextBuf = (char *) CPLMalloc(nBufLen);

    int  iSrc, iDst=0;

    for( iSrc = 0; iSrc < nLen; iSrc++ )
    {
        if( iSrc == 0 )
        {
            snprintf( pszTextBuf+iDst, nBufLen - iDst, "0x%02x", pabyData[iSrc] );
            iDst += 4;
        }
        else
        {
            snprintf( pszTextBuf+iDst, nBufLen - iDst, "%02x", pabyData[iSrc] );
            iDst += 2;
        }
    }
    pszTextBuf[iDst] = 0;

    return pszTextBuf;
}
