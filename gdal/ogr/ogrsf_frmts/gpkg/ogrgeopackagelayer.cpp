/******************************************************************************
 *
 * Project:  GeoPackage Translator
 * Purpose:  Implements OGRGeoPackageLayer class
 * Author:   Paul Ramsey <pramsey@boundlessgeo.com>
 *
 ******************************************************************************
 * Copyright (c) 2013, Paul Ramsey <pramsey@boundlessgeo.com>
 * Copyright (c) 2014, Even Rouault <even dot rouault at mines-paris dot org>
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

#include "ogr_geopackage.h"
#include "ogrgeopackageutility.h"
#include "ogrsqliteutility.h"
#include "ogr_p.h"

CPL_CVSID("$Id$");

/************************************************************************/
/*                      OGRGeoPackageLayer()                            */
/************************************************************************/

OGRGeoPackageLayer::OGRGeoPackageLayer(GDALGeoPackageDataset *poDS) :
    m_poDS(poDS),
    m_poFeatureDefn(NULL),
    iNextShapeId(0),
    m_poQueryStatement(NULL),
    bDoStep(true),
    m_pszFidColumn(NULL),
    iFIDCol(-1),
    iGeomCol(-1),
    panFieldOrdinals(NULL)
{}

/************************************************************************/
/*                      ~OGRGeoPackageLayer()                           */
/************************************************************************/

OGRGeoPackageLayer::~OGRGeoPackageLayer()
{

    CPLFree( m_pszFidColumn );

    if ( m_poQueryStatement )
        sqlite3_finalize(m_poQueryStatement);

    CPLFree(panFieldOrdinals);

    if ( m_poFeatureDefn )
        m_poFeatureDefn->Release();
}

/************************************************************************/
/*                            ResetReading()                            */
/************************************************************************/

void OGRGeoPackageLayer::ResetReading()

{
    ClearStatement();
    iNextShapeId = 0;
}

/************************************************************************/
/*                           ClearStatement()                           */
/************************************************************************/

void OGRGeoPackageLayer::ClearStatement()

{
    if( m_poQueryStatement != NULL )
    {
        CPLDebug( "GPKG", "finalize %p", m_poQueryStatement );
        sqlite3_finalize( m_poQueryStatement );
        m_poQueryStatement = NULL;
    }
}

/************************************************************************/
/*                           GetNextFeature()                           */
/************************************************************************/

OGRFeature *OGRGeoPackageLayer::GetNextFeature()

{
    for( ; true; )
    {
        if( m_poQueryStatement == NULL )
        {
            ResetStatement();
            if (m_poQueryStatement == NULL)
                return NULL;
        }

    /* -------------------------------------------------------------------- */
    /*      Fetch a record (unless otherwise instructed)                    */
    /* -------------------------------------------------------------------- */
        if( bDoStep )
        {
            int rc = sqlite3_step( m_poQueryStatement );
            if( rc != SQLITE_ROW )
            {
                if ( rc != SQLITE_DONE )
                {
                    sqlite3_reset(m_poQueryStatement);
                    CPLError( CE_Failure, CPLE_AppDefined,
                            "In GetNextRawFeature(): sqlite3_step() : %s",
                            sqlite3_errmsg(m_poDS->GetDB()) );
                }

                ClearStatement();

                return NULL;
            }
        }
        else
        {
            bDoStep = true;
        }

        OGRFeature *poFeature = TranslateFeature(m_poQueryStatement);

        if( (m_poFilterGeom == NULL
            || FilterGeometry( poFeature->GetGeomFieldRef(m_iGeomFieldFilter) ) )
            && (m_poAttrQuery == NULL
                || m_poAttrQuery->Evaluate( poFeature )) )
            return poFeature;

        delete poFeature;
    }
}

/************************************************************************/
/*                         TranslateFeature()                           */
/************************************************************************/

OGRFeature *OGRGeoPackageLayer::TranslateFeature( sqlite3_stmt* hStmt )

{
/* -------------------------------------------------------------------- */
/*      Create a feature from the current result.                       */
/* -------------------------------------------------------------------- */
    OGRFeature *poFeature = new OGRFeature( m_poFeatureDefn );

/* -------------------------------------------------------------------- */
/*      Set FID if we have a column to set it from.                     */
/* -------------------------------------------------------------------- */
    if( iFIDCol >= 0 )
    {
        poFeature->SetFID( sqlite3_column_int64( hStmt, iFIDCol ) );
        if( m_pszFidColumn == NULL && poFeature->GetFID() == 0 )
        {
            // Miht be the case for views with joins.
            poFeature->SetFID( iNextShapeId );
        }
    }
    else
        poFeature->SetFID( iNextShapeId );

    iNextShapeId++;

    m_nFeaturesRead++;

/* -------------------------------------------------------------------- */
/*      Process Geometry if we have a column.                           */
/* -------------------------------------------------------------------- */
    if( iGeomCol >= 0 )
    {
        OGRGeomFieldDefn* poGeomFieldDefn = m_poFeatureDefn->GetGeomFieldDefn(0);
        if ( sqlite3_column_type(hStmt, iGeomCol) != SQLITE_NULL &&
            !poGeomFieldDefn->IsIgnored() )
        {
            OGRSpatialReference* poSrs = poGeomFieldDefn->GetSpatialRef();
            int iGpkgSize = sqlite3_column_bytes(hStmt, iGeomCol);
            // coverity[tainted_data_return]
            GByte *pabyGpkg = (GByte *)sqlite3_column_blob(hStmt, iGeomCol);
            OGRGeometry *poGeom = GPkgGeometryToOGR(pabyGpkg, iGpkgSize, NULL);
            if ( poGeom == NULL )
            {
                // Try also spatialite geometry blobs
                if( OGRSQLiteLayer::ImportSpatiaLiteGeometry( pabyGpkg, iGpkgSize,
                                                              &poGeom ) != OGRERR_NONE )
                {
                    CPLError( CE_Failure, CPLE_AppDefined, "Unable to read geometry");
                }
            }
            if( poGeom != NULL )
                poGeom->assignSpatialReference(poSrs);
            poFeature->SetGeometryDirectly( poGeom );
        }
    }

/* -------------------------------------------------------------------- */
/*      set the fields.                                                 */
/* -------------------------------------------------------------------- */
    for( int iField = 0; iField < m_poFeatureDefn->GetFieldCount(); iField++ )
    {
        OGRFieldDefn *poFieldDefn = m_poFeatureDefn->GetFieldDefn( iField );
        if ( poFieldDefn->IsIgnored() )
            continue;

        const int iRawField = panFieldOrdinals[iField];

        if( sqlite3_column_type( hStmt, iRawField ) == SQLITE_NULL )
        {
            poFeature->SetFieldNull( iField );
            continue;
        }

        switch( poFieldDefn->GetType() )
        {
            case OFTInteger:
                poFeature->SetField( iField,
                    sqlite3_column_int( hStmt, iRawField ) );
                break;

            case OFTInteger64:
                poFeature->SetField( iField,
                    sqlite3_column_int64( hStmt, iRawField ) );
                break;

            case OFTReal:
                poFeature->SetField( iField,
                    sqlite3_column_double( hStmt, iRawField ) );
                break;

            case OFTBinary:
            {
                const int nBytes = sqlite3_column_bytes( hStmt, iRawField );
                // coverity[tainted_data_return]
                const GByte* pabyData = reinterpret_cast<const GByte*>(
                    sqlite3_column_blob( hStmt, iRawField ) );
                poFeature->SetField( iField, nBytes,
                                     const_cast<GByte*>(pabyData) );
                break;
            }

            case OFTDate:
            {
                const char* pszTxt = (const char*)sqlite3_column_text( hStmt, iRawField );
                int nYear, nMonth, nDay;
                if( sscanf(pszTxt, "%d-%d-%d", &nYear, &nMonth, &nDay) == 3 )
                    poFeature->SetField(iField, nYear, nMonth, nDay, 0, 0, 0, 0);
                break;
            }

            case OFTDateTime:
            {
                const char* pszTxt = (const char*)sqlite3_column_text( hStmt, iRawField );
                OGRField sField;
                if( OGRParseXMLDateTime(pszTxt, &sField) )
                    poFeature->SetField(iField, &sField);
                break;
            }

            case OFTString:
                poFeature->SetField( iField,
                        (const char *) sqlite3_column_text( hStmt, iRawField ) );
                break;

            default:
                break;
        }
    }

    return poFeature;
}

/************************************************************************/
/*                      GetFIDColumn()                                  */
/************************************************************************/

const char* OGRGeoPackageLayer::GetFIDColumn()
{
    if ( ! m_pszFidColumn )
        return "";
    else
        return m_pszFidColumn;
}

/************************************************************************/
/*                      TestCapability()                                */
/************************************************************************/

int OGRGeoPackageLayer::TestCapability ( const char * pszCap )
{
    if( EQUAL(pszCap,OLCIgnoreFields) )
        return TRUE;
    else if ( EQUAL(pszCap, OLCStringsAsUTF8) )
        return m_poDS->GetUTF8();
    else
        return FALSE;
}

/************************************************************************/
/*                          BuildFeatureDefn()                          */
/*                                                                      */
/*      Build feature definition from a set of column definitions       */
/*      set on a statement.  Sift out geometry and FID fields.          */
/************************************************************************/

void OGRGeoPackageLayer::BuildFeatureDefn( const char *pszLayerName,
                                           sqlite3_stmt *hStmt )

{
    m_poFeatureDefn = new OGRSQLiteFeatureDefn( pszLayerName );
    SetDescription( m_poFeatureDefn->GetName() );
    m_poFeatureDefn->SetGeomType(wkbNone);
    m_poFeatureDefn->Reference();

    const int    nRawColumns = sqlite3_column_count( hStmt );

    panFieldOrdinals = (int *) CPLMalloc( sizeof(int) * nRawColumns );

    const bool bPromoteToInteger64 =
        CPLTestBool(CPLGetConfigOption("OGR_PROMOTE_TO_INTEGER64", "FALSE"));

    for( int iCol = 0; iCol < nRawColumns; iCol++ )
    {
        OGRFieldDefn    oField(
            SQLUnescape(sqlite3_column_name( hStmt, iCol )),
            OFTString );

        // In some cases, particularly when there is a real name for
        // the primary key/_rowid_ column we will end up getting the
        // primary key column appearing twice.  Ignore any repeated names.
        if( m_poFeatureDefn->GetFieldIndex( oField.GetNameRef() ) != -1 )
            continue;

        if( m_pszFidColumn != NULL && EQUAL(m_pszFidColumn,
                                            oField.GetNameRef()) )
            continue;

        // The rowid is for internal use, not a real column.
        if( EQUAL(oField.GetNameRef(),"_rowid_") )
            continue;

        // this will avoid the old geom field to appear when running something
        // like "select st_buffer(geom,5) as geom, * from my_layer"
        if( m_poFeatureDefn->GetGeomFieldCount() &&
            EQUAL(oField.GetNameRef(),
                  m_poFeatureDefn->GetGeomFieldDefn(0)->GetNameRef()) )
        {
            continue;
        }


#ifdef SQLITE_HAS_COLUMN_METADATA
        const char* pszTableName = sqlite3_column_table_name( hStmt, iCol );
        const char* pszOriginName = sqlite3_column_origin_name( hStmt, iCol );
        if( pszTableName != NULL && pszOriginName != NULL )
        {
            OGRLayer* poLayer = m_poDS->GetLayerByName(pszTableName);
            if( poLayer != NULL )
            {
                if( m_poFeatureDefn->GetGeomFieldCount() == 0 &&
                    EQUAL(pszOriginName, poLayer->GetGeometryColumn()) )
                {
                    OGRGeomFieldDefn oGeomField(
                            poLayer->GetLayerDefn()->GetGeomFieldDefn(0));
                    oGeomField.SetName( oField.GetNameRef() );
                    m_poFeatureDefn->AddGeomFieldDefn(&oGeomField);
                    iGeomCol = iCol;
                    continue;
                }
                else if( EQUAL(pszOriginName, poLayer->GetFIDColumn()) )
                {
                    CPLFree(m_pszFidColumn);
                    m_pszFidColumn = CPLStrdup(oField.GetNameRef());
                    iFIDCol = iCol;
                    continue;
                }
                int nSrcIdx = poLayer->GetLayerDefn()->GetFieldIndex(
                                                        oField.GetNameRef());
                if( nSrcIdx >= 0 )
                {
                    OGRFieldDefn* poSrcField =
                            poLayer->GetLayerDefn()->GetFieldDefn(nSrcIdx);
                    oField.SetType( poSrcField->GetType() );
                    oField.SetSubType( poSrcField->GetSubType() );
                    oField.SetWidth( poSrcField->GetWidth() );
                    oField.SetPrecision( poSrcField->GetPrecision() );
                    m_poFeatureDefn->AddFieldDefn( &oField );
                    panFieldOrdinals[
                            m_poFeatureDefn->GetFieldCount() - 1] = iCol;
                    continue;
                }
            }
        }
#endif

        const int nColType = sqlite3_column_type( hStmt, iCol );
        if( m_pszFidColumn == NULL && nColType == SQLITE_INTEGER &&
            EQUAL(oField.GetNameRef(), "FID") )
        {
            m_pszFidColumn = CPLStrdup(oField.GetNameRef());
            iFIDCol = iCol;
            continue;
        }

        const char * pszDeclType = sqlite3_column_decltype(hStmt, iCol);

        // Recognize a geometry column from trying to build the geometry
        if( nColType == SQLITE_BLOB &&
            m_poFeatureDefn->GetGeomFieldCount() == 0 )
        {
            const int nBytes = sqlite3_column_bytes( hStmt, iCol );
            if( nBytes >= 8 )
            {
                // coverity[tainted_data_return]
                const GByte* pabyGpkg =
                        (const GByte*)sqlite3_column_blob( hStmt, iCol  );
                GPkgHeader oHeader;
                OGRGeometry* poGeom = NULL;
                int nSRID = 0;

                if( GPkgHeaderFromWKB(pabyGpkg, nBytes, &oHeader) ==
                                                                OGRERR_NONE )
                {
                    poGeom = GPkgGeometryToOGR(pabyGpkg, nBytes, NULL);
                    nSRID = oHeader.iSrsId;
                }
                else
                {
                    // Try also spatialite geometry blobs
                    if( OGRSQLiteLayer::ImportSpatiaLiteGeometry(
                          pabyGpkg, nBytes, &poGeom, &nSRID ) != OGRERR_NONE )
                    {
                        delete poGeom;
                        poGeom = NULL;
                    }
                }

                if( poGeom )
                {
                    OGRGeomFieldDefn oGeomField(oField.GetNameRef(),
                                                wkbUnknown);

                    /* Read the SRS */
                    OGRSpatialReference *poSRS =
                                        m_poDS->GetSpatialRef(nSRID);
                    if ( poSRS )
                    {
                        oGeomField.SetSpatialRef(poSRS);
                        poSRS->Dereference();
                    }

                    OGRwkbGeometryType eGeomType = poGeom->getGeometryType();
                    if( pszDeclType != NULL )
                    {
                        OGRwkbGeometryType eDeclaredGeomType =
                            GPkgGeometryTypeToWKB(pszDeclType, false, false);
                        if( eDeclaredGeomType != wkbUnknown )
                        {
                            eGeomType = OGR_GT_SetModifier(eDeclaredGeomType,
                                               OGR_GT_HasZ(eGeomType),
                                               OGR_GT_HasM(eGeomType));
                        }
                    }
                    oGeomField.SetType( eGeomType );

                    delete poGeom;
                    poGeom = NULL;

                    m_poFeatureDefn->AddGeomFieldDefn(&oGeomField);
                    iGeomCol = iCol;
                    continue;
                }
            }
        }

        switch( nColType )
        {
          case SQLITE_INTEGER:
            if( bPromoteToInteger64 )
                oField.SetType( OFTInteger64 );
            else
            {
                GIntBig nVal = sqlite3_column_int64(hStmt, iCol);
                if( CPL_INT64_FITS_ON_INT32(nVal) )
                    oField.SetType( OFTInteger );
                else
                    oField.SetType( OFTInteger64 );
            }
            break;

          case SQLITE_FLOAT:
            oField.SetType( OFTReal );
            break;

          case SQLITE_BLOB:
            oField.SetType( OFTBinary );
            break;

          default:
            /* leave it as OFTString */;
        }

        if (pszDeclType != NULL)
        {
            OGRFieldSubType eSubType;
            int nMaxWidth = 0;
            const OGRFieldType eFieldType =
                GPkgFieldToOGR(pszDeclType, eSubType, nMaxWidth);
            if( (int)eFieldType <= OFTMaxType )
            {
                oField.SetType(eFieldType);
                oField.SetSubType(eSubType);
                oField.SetWidth(nMaxWidth);
            }
        }

        m_poFeatureDefn->AddFieldDefn( &oField );
        panFieldOrdinals[m_poFeatureDefn->GetFieldCount() - 1] = iCol;
    }
}
