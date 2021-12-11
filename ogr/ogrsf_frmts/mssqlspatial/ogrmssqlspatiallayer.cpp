/******************************************************************************
 *
 * Project:  MSSQL Spatial driver
 * Purpose:  Definition of classes for OGR MSSQL Spatial driver.
 * Author:   Tamas Szekeres, szekerest at gmail.com
 *
 ******************************************************************************
 * Copyright (c) 2010, Tamas Szekeres
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

#include "ogr_mssqlspatial.h"

CPL_CVSID("$Id$")
/************************************************************************/
/*                        OGRMSSQLSpatialLayer()                        */
/************************************************************************/

OGRMSSQLSpatialLayer::OGRMSSQLSpatialLayer()

{
}

/************************************************************************/
/*                      ~OGRMSSQLSpatialLayer()                         */
/************************************************************************/

OGRMSSQLSpatialLayer::~OGRMSSQLSpatialLayer()

{
    if( m_nFeaturesRead > 0 && poFeatureDefn != nullptr )
    {
        CPLDebug( "OGR_MSSQLSpatial", "%d features read on layer '%s'.",
                  (int) m_nFeaturesRead,
                  poFeatureDefn->GetName() );
    }

    ClearStatement();

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
}

/************************************************************************/
/*                          BuildFeatureDefn()                          */
/*                                                                      */
/*      Build feature definition from a set of column definitions       */
/*      set on a statement.  Sift out geometry and FID fields.          */
/************************************************************************/

void OGRMSSQLSpatialLayer::BuildFeatureDefn( const char *pszLayerName,
                                    CPLODBCStatement *poStmtIn )

{
    bool bShowFidColumn = CPLTestBool(CPLGetConfigOption("MSSQLSPATIAL_SHOW_FID_COLUMN", "NO"));

    poFeatureDefn = new OGRFeatureDefn( pszLayerName );
    nRawColumns = poStmtIn->GetColCount();

    poFeatureDefn->Reference();

    CPLFree(panFieldOrdinals);
    panFieldOrdinals = (int *) CPLMalloc( sizeof(int) * nRawColumns );

    for( int iCol = 0; iCol < nRawColumns; iCol++ )
    {
        if ( pszGeomColumn == nullptr )
        {
            /* need to identify the geometry column */
            if ( EQUAL(poStmtIn->GetColTypeName( iCol ), "geometry") )
            {
                nGeomColumnType = MSSQLCOLTYPE_GEOMETRY;
                pszGeomColumn = CPLStrdup( poStmtIn->GetColName(iCol) );
                if (poFeatureDefn->GetGeomFieldCount() == 1)
                {
                    poFeatureDefn->GetGeomFieldDefn(0)->SetNullable( poStmtIn->GetColNullable(iCol) );
                    poFeatureDefn->GetGeomFieldDefn(0)->SetName(pszGeomColumn);
                }
                nGeomColumnIndex = iCol;
                continue;
            }
            else if ( EQUAL(poStmtIn->GetColTypeName( iCol ), "geography") )
            {
                nGeomColumnType = MSSQLCOLTYPE_GEOGRAPHY;
                pszGeomColumn = CPLStrdup( poStmtIn->GetColName(iCol) );
                if (poFeatureDefn->GetGeomFieldCount() == 1)
                {
                    poFeatureDefn->GetGeomFieldDefn(0)->SetNullable( poStmtIn->GetColNullable(iCol) );
                    poFeatureDefn->GetGeomFieldDefn(0)->SetName(pszGeomColumn);
                }
                nGeomColumnIndex = iCol;
                continue;
            }
        }
        else
        {
            if( EQUAL(poStmtIn->GetColName(iCol),pszGeomColumn) )
            {
                if (poFeatureDefn->GetGeomFieldCount() == 1)
                {
                    poFeatureDefn->GetGeomFieldDefn(0)->SetNullable( poStmtIn->GetColNullable(iCol) );
                    poFeatureDefn->GetGeomFieldDefn(0)->SetName(pszGeomColumn);
                }
                nGeomColumnIndex = iCol;
                continue;
            }
        }

        if( pszFIDColumn != nullptr)
        {
            if (EQUAL(poStmtIn->GetColName(iCol), pszFIDColumn) )
            {
                bool bIntegerFID = false;
                switch( CPLODBCStatement::GetTypeMapping(poStmtIn->GetColType(iCol)) )
                {
                    case SQL_C_SSHORT:
                    case SQL_C_USHORT:
                    case SQL_C_SLONG:
                    case SQL_C_ULONG:
                    case SQL_C_SBIGINT:
                    case SQL_C_UBIGINT:
                        bIntegerFID = true;
                        break;
                    default:
                        break;
                }
                if( !bIntegerFID )
                {
                    CPLDebug("MSSQL", "Ignoring FID column %s as it is of non integer type",
                             pszFIDColumn);
                    CPLFree(pszFIDColumn);
                    pszFIDColumn = nullptr;
                }
                else
                {
                    if (STARTS_WITH_CI(poStmtIn->GetColTypeName( iCol ), "bigint"))
                        SetMetadataItem(OLMD_FID64, "YES");

                    if ( EQUAL(poStmtIn->GetColTypeName( iCol ), "int identity") ||
                        EQUAL(poStmtIn->GetColTypeName( iCol ), "bigint identity"))
                        bIsIdentityFid = TRUE;

                    nFIDColumnIndex = iCol;

                    if (!bShowFidColumn)
                        continue;
                }
            }
        }
        else
        {
            if (EQUAL(poStmtIn->GetColTypeName( iCol ), "int identity"))
            {
                pszFIDColumn = CPLStrdup( poStmtIn->GetColName(iCol) );
                bIsIdentityFid = TRUE;
                nFIDColumnIndex = iCol;

                if (!bShowFidColumn)
                    continue;
            }
            else if (EQUAL(poStmtIn->GetColTypeName( iCol ), "bigint identity"))
            {
                pszFIDColumn = CPLStrdup( poStmtIn->GetColName(iCol) );
                bIsIdentityFid = TRUE;
                SetMetadataItem(OLMD_FID64, "YES");
                nFIDColumnIndex = iCol;

                if (!bShowFidColumn)
                    continue;
            }
        }

        OGRFieldDefn    oField( poStmtIn->GetColName(iCol), OFTString );

        switch( CPLODBCStatement::GetTypeMapping(poStmtIn->GetColType(iCol)) )
        {
            case SQL_C_SSHORT:
                oField.SetType( OFTInteger );
                oField.SetSubType( OFSTInt16 );
                break;

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
                oField.SetWidth( MAX(0,poStmtIn->GetColSize( iCol )) );
                break;

            case SQL_C_NUMERIC:
                oField.SetType( OFTReal );
                oField.SetPrecision( poStmtIn->GetColPrecision(iCol) );
                oField.SetWidth( MAX(0,poStmtIn->GetColSize( iCol )) );
                if( oField.GetPrecision() == 0 && oField.GetWidth() <= 9 )
                    oField.SetType( OFTInteger );
                else if( oField.GetPrecision() == 0 && oField.GetWidth() <= 18 )
                    oField.SetType( OFTInteger64 );
                break;

            case SQL_C_FLOAT:
                oField.SetType( OFTReal );
                oField.SetSubType( OFSTFloat32 );
                break;

            case SQL_C_DOUBLE:
                oField.SetType( OFTReal );
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

            case SQL_C_GUID:
                m_bHasUUIDColumn = true;
                oField.SetType( OFTString );
                oField.SetSubType( OFSTUUID );
                break;


            default:
                oField.SetWidth( MAX(0,poStmtIn->GetColSize( iCol )) );
                /* leave it as OFTString */;
        }

        oField.SetNullable( poStmtIn->GetColNullable(iCol) );

        if ( poStmtIn->GetColColumnDef(iCol) )
        {
            /* process default value specification */
            if ( EQUAL(poStmtIn->GetColColumnDef(iCol), "(getdate())") )
                oField.SetDefault( "CURRENT_TIMESTAMP" );
            else if ( STARTS_WITH_CI(poStmtIn->GetColColumnDef(iCol), "(CONVERT([time],getdate()") )
                oField.SetDefault( "CURRENT_TIME" );
            else if ( STARTS_WITH_CI(poStmtIn->GetColColumnDef(iCol), "(CONVERT([date],getdate()") )
                oField.SetDefault( "CURRENT_DATE" );
            else
            {
                char* pszDefault = CPLStrdup(poStmtIn->GetColColumnDef(iCol));
                int nLen = static_cast<int>(strlen(pszDefault));
                if (nLen >= 1 && pszDefault[0] == '(' && pszDefault[nLen-1] == ')')
                {
                    // All default values are encapsulated in brackets
                    // by MSSQL server.
                    if (nLen >= 4 && pszDefault[1] == '(' && pszDefault[nLen-2] == ')')
                    {
                        /* for numeric values double brackets are used */
                        pszDefault[nLen-2] = '\0';
                        oField.SetDefault(pszDefault + 2);
                    }
                    else
                    {
                        pszDefault[nLen-1] = '\0';
                        oField.SetDefault(pszDefault + 1);
                    }
                }
                else
                    oField.SetDefault( pszDefault );

                CPLFree(pszDefault);
            }
        }

        poFeatureDefn->AddFieldDefn( &oField );
        panFieldOrdinals[poFeatureDefn->GetFieldCount() - 1] = iCol;
    }

/* -------------------------------------------------------------------- */
/*      If we don't already have an FID, check if there is a special    */
/*      FID named column available.                                     */
/* -------------------------------------------------------------------- */
    if( pszFIDColumn == nullptr )
    {
        const char *pszOGR_FID = CPLGetConfigOption("MSSQLSPATIAL_OGR_FID","OGR_FID");
        if( poFeatureDefn->GetFieldIndex( pszOGR_FID ) != -1 )
            pszFIDColumn = CPLStrdup(pszOGR_FID);
    }

    if( pszFIDColumn != nullptr )
        CPLDebug( "OGR_MSSQLSpatial", "Using column %s as FID for table %s.",
                  pszFIDColumn, poFeatureDefn->GetName() );
    else
        CPLDebug( "OGR_MSSQLSpatial", "Table %s has no identified FID column.",
                  poFeatureDefn->GetName() );
}

/************************************************************************/
/*                           ClearStatement()                           */
/************************************************************************/

void OGRMSSQLSpatialLayer::ClearStatement()

{
    if( poStmt != nullptr )
    {
        delete poStmt;
        poStmt = nullptr;
    }
}

/************************************************************************/
/*                            ResetReading()                            */
/************************************************************************/

void OGRMSSQLSpatialLayer::ResetReading()

{
    if( m_bResetNeeded )
    {
        iNextShapeId = 0;
        ClearStatement();
        m_bEOF = false;
        m_bResetNeeded = false;
    }
}

/************************************************************************/
/*                           GetNextFeature()                           */
/************************************************************************/

OGRFeature *OGRMSSQLSpatialLayer::GetNextFeature()

{
    if( m_bEOF )
        return nullptr;

    while( true )
    {
        OGRFeature      *poFeature;

        poFeature = GetNextRawFeature();
        if( poFeature == nullptr )
        {
            m_bEOF = true;
            return nullptr;
        }

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

OGRFeature *OGRMSSQLSpatialLayer::GetNextRawFeature()

{
    m_bResetNeeded = true;
    if( GetStatement() == nullptr )
        return nullptr;

/* -------------------------------------------------------------------- */
/*      If we are marked to restart then do so, and fetch a record.     */
/* -------------------------------------------------------------------- */
    if( !poStmt->Fetch() )
    {
        delete poStmt;
        poStmt = nullptr;
        return nullptr;
    }

/* -------------------------------------------------------------------- */
/*      Create a feature from the current result.                       */
/* -------------------------------------------------------------------- */
    OGRFeature *poFeature = new OGRFeature( poFeatureDefn );

    const char* pszFID;
    if( pszFIDColumn != nullptr && poStmt->GetColId(pszFIDColumn) > -1 &&
        (pszFID = poStmt->GetColData(poStmt->GetColId(pszFIDColumn))) != nullptr )
        poFeature->SetFID( CPLAtoGIntBig(pszFID) );
    else
        poFeature->SetFID( iNextShapeId );

    iNextShapeId++;
    m_nFeaturesRead++;

/* -------------------------------------------------------------------- */
/*      Set the fields.                                                 */
/* -------------------------------------------------------------------- */
    for( int iField = 0; iField < poFeatureDefn->GetFieldCount(); iField++ )
    {
        if ( poFeatureDefn->GetFieldDefn(iField)->IsIgnored() )
            continue;

        int iSrcField = panFieldOrdinals[iField];
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

/* -------------------------------------------------------------------- */
/*      Try to extract a geometry.                                      */
/* -------------------------------------------------------------------- */
    if( pszGeomColumn != nullptr && !poFeatureDefn->IsGeometryIgnored())
    {
        int iField = poStmt->GetColId( pszGeomColumn );
        const char *pszGeomText = poStmt->GetColData( iField );
        OGRGeometry *poGeom = nullptr;
        OGRErr eErr = OGRERR_NONE;

        if( pszGeomText != nullptr )
        {
            int nLength = poStmt->GetColDataLength( iField );

            if ( nGeomColumnType == MSSQLCOLTYPE_GEOMETRY ||
                 nGeomColumnType == MSSQLCOLTYPE_GEOGRAPHY ||
                 nGeomColumnType == MSSQLCOLTYPE_BINARY)
            {
                switch ( poDS->GetGeometryFormat() )
                {
                case MSSQLGEOMETRY_NATIVE:
                    {
                        OGRMSSQLGeometryParser oParser( nGeomColumnType );
                        eErr = oParser.ParseSqlGeometry(
                            (unsigned char *) pszGeomText, nLength, &poGeom );
                        nSRSId = oParser.GetSRSId();
                    }
                    break;
                case MSSQLGEOMETRY_WKB:
                case MSSQLGEOMETRY_WKBZM:
                    eErr = OGRGeometryFactory::createFromWkb(pszGeomText,
                                                      nullptr, &poGeom, nLength);
                    break;
                case MSSQLGEOMETRY_WKT:
                    eErr = OGRGeometryFactory::createFromWkt(pszGeomText,
                                                      nullptr, &poGeom);
                    break;
                }
            }
            else if (nGeomColumnType == MSSQLCOLTYPE_TEXT)
            {
                eErr = OGRGeometryFactory::createFromWkt(pszGeomText,
                                                      nullptr, &poGeom);
            }
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

OGRFeature *OGRMSSQLSpatialLayer::GetFeature( GIntBig nFeatureId )

{
    /* This should be implemented directly! */

    return OGRLayer::GetFeature( nFeatureId );
}

/************************************************************************/
/*                           TestCapability()                           */
/************************************************************************/

int OGRMSSQLSpatialLayer::TestCapability( CPL_UNUSED const char * pszCap )
{
    return FALSE;
}

/************************************************************************/
/*                          StartTransaction()                          */
/************************************************************************/

OGRErr OGRMSSQLSpatialLayer::StartTransaction()

{
    if (!poDS->GetSession()->BeginTransaction())
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                    "Failed to start transaction: %s", poDS->GetSession()->GetLastError() );
        return OGRERR_FAILURE;
    }
    return OGRERR_NONE;
}

/************************************************************************/
/*                         CommitTransaction()                          */
/************************************************************************/

OGRErr OGRMSSQLSpatialLayer::CommitTransaction()

{
    if (!poDS->GetSession()->CommitTransaction())
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                    "Failed to commit transaction: %s", poDS->GetSession()->GetLastError() );
        return OGRERR_FAILURE;
    }
    return OGRERR_NONE;
}

/************************************************************************/
/*                        RollbackTransaction()                         */
/************************************************************************/

OGRErr OGRMSSQLSpatialLayer::RollbackTransaction()

{
    if (!poDS->GetSession()->RollbackTransaction())
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                    "Failed to roll back transaction: %s", poDS->GetSession()->GetLastError() );
        return OGRERR_FAILURE;
    }
    return OGRERR_NONE;
}

/************************************************************************/
/*                           GetSpatialRef()                            */
/************************************************************************/

OGRSpatialReference *OGRMSSQLSpatialLayer::GetSpatialRef()

{
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

const char *OGRMSSQLSpatialLayer::GetFIDColumn()

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

const char *OGRMSSQLSpatialLayer::GetGeometryColumn()

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

char* OGRMSSQLSpatialLayer::GByteArrayToHexString( const GByte* pabyData, int nLen)
{
    char* pszTextBuf;

    const size_t nTextBufLen = nLen*2+3;
    pszTextBuf = (char *) CPLMalloc(nTextBufLen);

    int  iSrc, iDst=0;

    for( iSrc = 0; iSrc < nLen; iSrc++ )
    {
        if( iSrc == 0 )
        {
            snprintf( pszTextBuf+iDst, nTextBufLen-iDst, "0x%02x", pabyData[iSrc] );
            iDst += 4;
        }
        else
        {
            snprintf( pszTextBuf+iDst, nTextBufLen-iDst, "%02x", pabyData[iSrc] );
            iDst += 2;
        }
    }
    pszTextBuf[iDst] = 0;

    return pszTextBuf;
}
