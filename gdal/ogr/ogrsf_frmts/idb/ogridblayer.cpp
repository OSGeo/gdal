/******************************************************************************
 * $Id$
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  Implements OGRIDBLayer class, code shared between
 *           the direct table access, and the generic SQL results
 *           (based on ODBC and PG drivers).
 * Author:   Oleg Semykin, oleg.semykin@gmail.com
 *
 ******************************************************************************
 * Copyright (c) 2006, Oleg Semykin
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
#include "ogr_idb.h"
#include "cpl_string.h"

CPL_CVSID("$Id$");

/************************************************************************/
/*                            OGRIDBLayer()                            */
/************************************************************************/

OGRIDBLayer::OGRIDBLayer()

{
    poDS = NULL;

    bGeomColumnWKB = FALSE;
    pszFIDColumn = NULL;
    pszGeomColumn = NULL;

    poCurr = NULL;

    iNextShapeId = 0;

    poSRS = NULL;
    nSRSId = -2; // we haven't even queried the database for it yet. 
}

/************************************************************************/
/*                            ~OGRIDBLayer()                             */
/************************************************************************/

OGRIDBLayer::~OGRIDBLayer()

{
    if( poCurr != NULL )
    {
        poCurr->Close();
        delete poCurr;
        poCurr = NULL;
    }

    if( pszGeomColumn )
        CPLFree( pszGeomColumn );

    if(pszFIDColumn)
        CPLFree( pszFIDColumn );

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

CPLErr OGRIDBLayer::BuildFeatureDefn( const char *pszLayerName, 
                                    ITCursor *poCurr )

{
    poFeatureDefn = new OGRFeatureDefn( pszLayerName );
    SetDescription( poFeatureDefn->GetName() );
    const ITTypeInfo * poInfo = poCurr->RowType();
    int    nRawColumns = poInfo->ColumnCount();

    poFeatureDefn->Reference();

    for( int iCol = 0; iCol < nRawColumns; iCol++ )
    {
        const char * pszColName = poInfo->ColumnName(iCol);
        const ITTypeInfo * poTI = poInfo->ColumnType(iCol);
        const char * pszTypName = poTI->Name();

        OGRFieldDefn    oField( pszColName, OFTString );

        oField.SetWidth( MAX(0,poTI->Bound()) );

        if ( pszGeomColumn != NULL && EQUAL(pszColName,pszGeomColumn) )
            continue;

        if ( EQUALN("st_", pszTypName, 3) && pszGeomColumn == NULL )
        {
            // We found spatial column!
            pszGeomColumn = CPLStrdup(pszColName);

            if ( EQUAL("st_point", pszTypName) )
                poFeatureDefn->SetGeomType( wkbPoint );
            else if ( EQUAL("st_linestring", pszTypName) )
                poFeatureDefn->SetGeomType( wkbLineString );
            else if ( EQUAL("st_polygon", pszTypName) )
                poFeatureDefn->SetGeomType( wkbPolygon );
            else if ( EQUAL("st_multipoint", pszTypName) )
                poFeatureDefn->SetGeomType( wkbMultiPoint );
            else if ( EQUAL("st_multilinestring", pszTypName) )
                poFeatureDefn->SetGeomType( wkbMultiLineString );
            else if ( EQUAL("st_multipolygon", pszTypName) )
                poFeatureDefn->SetGeomType( wkbMultiPolygon );

            continue;
        }

        // Check other field types
        if ( EQUAL( pszTypName, "blob" ) ||
             EQUAL( pszTypName, "byte" ) ||
             EQUAL( pszTypName, "opaque" ) ||
             EQUAL( pszTypName, "text" ) ||
             EQUALN( pszTypName, "list", 4 ) ||
             EQUALN( pszTypName, "collection", 10 ) ||
             EQUALN( pszTypName, "row", 3 ) ||
             EQUALN( pszTypName, "set", 3 ) )
        {
            CPLDebug( "OGR_IDB", "'%s' column type not supported yet. Column '%s'",
                      pszTypName, pszColName );
            continue;
        }

        if ( EQUALN( pszTypName, "st_", 3 ) )
        {
            oField.SetType( OFTBinary );
        }
        else if ( EQUAL( pszTypName, "date" ) )
        {
            oField.SetType( OFTDate );
        }
        else if ( EQUAL( pszTypName, "datetime" ) )
        {
            oField.SetType( OFTDateTime );
        }
        else if ( EQUAL( pszTypName, "decimal" ) ||
                  EQUAL( pszTypName, "money" ) ||
                  EQUAL( pszTypName, "float" ) ||
                  EQUAL( pszTypName, "smallfloat" ) )
        {
            oField.SetType( OFTReal );
            oField.SetPrecision( MAX( 0, poTI->Scale() ) ); // -1 for numeric
        }
        else if ( EQUAL( pszTypName, "integer" ) ||
                  EQUAL( pszTypName, "serial" ) )
        {
            oField.SetType( OFTInteger );
            // 10 as hardcoded max int32 value length + 1 sig bit
            oField.SetWidth( 11 );
        }
        else if ( EQUAL( pszTypName, "smallint" ) )
        {
            oField.SetType( OFTInteger );
            // 5 as hardcoded max int16 value length + 1 sig bit
            oField.SetWidth( 6 );
        }
        else
        {
            // leave as string:
            // *char, character, character varing, *varchar
            // interval. int8, serial8
        }

        poFeatureDefn->AddFieldDefn( &oField );
    }

/* -------------------------------------------------------------------- */
/*      If we don't already have an FID, check if there is a special    */
/*      FID named column available.                                     */
/* -------------------------------------------------------------------- */
    if( pszFIDColumn == NULL )
    {
        const char *pszOGR_FID = CPLGetConfigOption("IDB_OGR_FID","OGR_FID");
        if( poFeatureDefn->GetFieldIndex( pszOGR_FID ) != -1 )
            pszFIDColumn = CPLStrdup(pszOGR_FID);
    }

    if( pszFIDColumn != NULL )
        CPLDebug( "OGR_IDB", "Using column %s as FID for table %s.",
                  pszFIDColumn, poFeatureDefn->GetName() );
    else
        CPLDebug( "OGR_IDB", "Table %s has no identified FID column.",
                  poFeatureDefn->GetName() );

    return CE_None;
}


/************************************************************************/
/*                            ResetReading()                            */
/************************************************************************/

void OGRIDBLayer::ResetReading()

{
    iNextShapeId = 0;
}

/************************************************************************/
/*                           GetNextFeature()                           */
/************************************************************************/

OGRFeature *OGRIDBLayer::GetNextFeature()

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

OGRFeature *OGRIDBLayer::GetNextRawFeature()

{
    if( GetQuery() == NULL )
        return NULL;

/* -------------------------------------------------------------------- */
/*      If we are marked to restart then do so, and fetch a record.     */
/* -------------------------------------------------------------------- */
    ITRow * row = poCurr->NextRow();
    if ( ! row )
    {
        delete poCurr;
        poCurr = NULL;
        return NULL;
    }

    iNextShapeId++;
    m_nFeaturesRead++;

/* -------------------------------------------------------------------- */
/*      Create a feature from the current result.                       */
/* -------------------------------------------------------------------- */
    int         iField;
    OGRFeature *poFeature = new OGRFeature( poFeatureDefn );

    const ITTypeInfo * poRowType = poCurr->RowType();
    int nFieldCount = poRowType->ColumnCount();

    for ( iField = 0; iField < nFieldCount; iField++ )
    {
/* -------------------------------------------------------------------- */
/*      Handle FID column                                               */
/* -------------------------------------------------------------------- */
        if ( pszFIDColumn != NULL &&
             EQUAL( poRowType->ColumnName( iField ), pszFIDColumn ) )
            poFeature->SetFID( atoi( row->Column( iField )->Printable() ) );

/* -------------------------------------------------------------------- */
/*      Handle geometry                                                 */
/* -------------------------------------------------------------------- */
        if( pszGeomColumn != NULL &&
            EQUAL( poRowType->ColumnName( iField ), pszGeomColumn ) )
        {
            OGRGeometry *poGeom = NULL;
            OGRErr eErr = OGRERR_NONE;

            ITValue * v = row->Column( iField );

            if( ! v->IsNull() && ! bGeomColumnWKB )
            {
                const char *pszGeomText = v->Printable();
                if ( pszGeomText != NULL )
                eErr =
                    OGRGeometryFactory::createFromWkt((char **) &pszGeomText,
                                                    poSRS, &poGeom);
            }
            else if( ! v->IsNull() && bGeomColumnWKB )
            {
                ITDatum *rv = 0;
                if ( v->QueryInterface( ITDatumIID, (void **) &rv ) ==
                     IT_QUERYINTERFACE_SUCCESS )
                {
                    int nLength = rv->DataLength();
                    unsigned char * wkb = (unsigned char *)rv->Data();

                    eErr = OGRGeometryFactory::createFromWkb( wkb, poSRS, &poGeom, nLength);
                    rv->Release();
                }
            }

            v->Release();


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
                    default:
                        pszMessage = "Unrecognized error";
                }
                CPLError(CE_Failure, CPLE_AppDefined,
                        "GetNextRawFeature(): %s", pszMessage);
            }

            if( poGeom != NULL )
            {
                poFeature->SetGeometryDirectly( poGeom );
            }

            continue;
        }

/* -------------------------------------------------------------------- */
/*      Transfer regular data fields.                                   */
/* -------------------------------------------------------------------- */
        int iOGRField =
            poFeatureDefn->GetFieldIndex( poRowType->ColumnName( iField ) );

        if( iOGRField < 0 )
            continue;

        const char * pszColData = row->Column( iField )->Printable();

        if( ! pszColData  )
            continue;

        if( poFeatureDefn->GetFieldDefn(iOGRField)->GetType() == OFTBinary )
            poFeature->SetField( iOGRField,
                                 poRowType->ColumnType( iField )->Size(),
                                 (GByte *) pszColData );
        else
            poFeature->SetField( iOGRField, pszColData );
    }

    row->Release();
    return poFeature;
}

/************************************************************************/
/*                             GetFeature()                             */
/************************************************************************/

OGRFeature *OGRIDBLayer::GetFeature( long nFeatureId )

{
    /* This should be implemented directly! */

    return OGRLayer::GetFeature( nFeatureId );
}

/************************************************************************/
/*                           TestCapability()                           */
/************************************************************************/

int OGRIDBLayer::TestCapability( const char * pszCap )

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

OGRSpatialReference *OGRIDBLayer::GetSpatialRef()

{
    return poSRS;
}

/************************************************************************/
/*                            GetFIDColumn()                            */
/************************************************************************/

const char *OGRIDBLayer::GetFIDColumn()

{
    if( pszFIDColumn != NULL )
        return pszFIDColumn;
    else
        return "";
}

/************************************************************************/
/*                         GetGeometryColumn()                          */
/************************************************************************/

const char *OGRIDBLayer::GetGeometryColumn()

{
    if( pszGeomColumn != NULL )
        return pszGeomColumn;
    else
        return "";
}

/* TODO Query to get layer extent */
/*
EXECUTE FUNCTION SE_BoundingBox ('table_name', 'geom_column' )
*/
