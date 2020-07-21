/******************************************************************************
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  Implements OGRMySQLLayer class.
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

#include "ogr_mysql.h"
#include "cpl_conv.h"
#include "cpl_string.h"

CPL_CVSID("$Id$")

/************************************************************************/
/*                           OGRMySQLLayer()                            */
/************************************************************************/

OGRMySQLLayer::OGRMySQLLayer() :
    poFeatureDefn(nullptr),
    poSRS(nullptr),
    nSRSId(-2), // we haven't even queried the database for it yet.
    iNextShapeId(0),
    poDS(nullptr),
    pszQueryStatement(nullptr),
    nResultOffset(0),
    pszGeomColumn(nullptr),
    pszGeomColumnTable(nullptr),
    nGeomType(0),
    bHasFid(FALSE),
    pszFIDColumn(nullptr),
    hResultSet(nullptr)
{}

/************************************************************************/
/*                           ~OGRMySQLLayer()                           */
/************************************************************************/

OGRMySQLLayer::~OGRMySQLLayer()

{
    if( m_nFeaturesRead > 0 && poFeatureDefn != nullptr )
    {
        CPLDebug( "MySQL", "%d features read on layer '%s'.",
                  (int) m_nFeaturesRead,
                  poFeatureDefn->GetName() );
    }

    OGRMySQLLayer::ResetReading();

    CPLFree( pszGeomColumn );
    CPLFree( pszGeomColumnTable );
    CPLFree( pszFIDColumn );
    CPLFree( pszQueryStatement );

    if( poSRS != nullptr )
        poSRS->Release();

    if( poFeatureDefn )
        poFeatureDefn->Release();
}

/************************************************************************/
/*                            ResetReading()                            */
/************************************************************************/

void OGRMySQLLayer::ResetReading()

{
    iNextShapeId = 0;

    if( hResultSet != nullptr )
    {
        mysql_free_result( hResultSet );
        hResultSet = nullptr;

        poDS->InterruptLongResult();
    }
    m_bEOF = false;
}

/************************************************************************/
/*                           GetNextFeature()                           */
/************************************************************************/

OGRFeature *OGRMySQLLayer::GetNextFeature()

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
/*                          RecordToFeature()                           */
/*                                                                      */
/*      Convert the indicated record of the current result set into     */
/*      a feature.                                                      */
/************************************************************************/

OGRFeature *OGRMySQLLayer::RecordToFeature( char **papszRow,
                                            unsigned long *panLengths )

{
    mysql_field_seek( hResultSet, 0 );

/* -------------------------------------------------------------------- */
/*      Create a feature from the current result.                       */
/* -------------------------------------------------------------------- */
    OGRFeature *poFeature = new OGRFeature( poFeatureDefn );

    poFeature->SetFID( iNextShapeId );
    m_nFeaturesRead++;

/* ==================================================================== */
/*      Transfer all result fields we can.                              */
/* ==================================================================== */
    for( int iField = 0;
         iField < (int) mysql_num_fields(hResultSet);
         iField++ )
    {
        MYSQL_FIELD *psMSField = mysql_fetch_field(hResultSet);

/* -------------------------------------------------------------------- */
/*      Handle FID.                                                     */
/* -------------------------------------------------------------------- */
        if( bHasFid && EQUAL(psMSField->name,pszFIDColumn) )
        {
            if( papszRow[iField] == nullptr )
            {
                CPLError( CE_Failure, CPLE_AppDefined,
                          "NULL primary key in RecordToFeature()" );
                return nullptr;
            }

            poFeature->SetFID( CPLAtoGIntBig(papszRow[iField]) );
        }

        if( papszRow[iField] == nullptr )
        {
            const int iOGRField = poFeatureDefn->GetFieldIndex(psMSField->name);
            if( iOGRField >= 0 )
                poFeature->SetFieldNull( iOGRField );

            continue;
        }

/* -------------------------------------------------------------------- */
/*      Handle MySQL geometry                                           */
/* -------------------------------------------------------------------- */
        if( pszGeomColumn && EQUAL(psMSField->name,pszGeomColumn))
        {
            OGRGeometry *poGeometry = nullptr;

            // Geometry columns will have the first 4 bytes contain the SRID.
            OGRGeometryFactory::createFromWkb(
                papszRow[iField] + 4,
                nullptr,
                &poGeometry,
                static_cast<int>(panLengths[iField] - 4) );

            if( poGeometry != nullptr )
            {
                poGeometry->assignSpatialReference( GetSpatialRef() );
                poFeature->SetGeometryDirectly( poGeometry );
            }
            continue;
        }

/* -------------------------------------------------------------------- */
/*      Transfer regular data fields.                                   */
/* -------------------------------------------------------------------- */
        const int iOGRField = poFeatureDefn->GetFieldIndex(psMSField->name);
        if( iOGRField < 0 )
            continue;

        OGRFieldDefn *psFieldDefn = poFeatureDefn->GetFieldDefn( iOGRField );

        if( psFieldDefn->GetType() == OFTBinary )
        {
            poFeature->SetField( iOGRField, static_cast<int>(panLengths[iField]),
                                 (GByte *) papszRow[iField] );
        }
        else
        {
            poFeature->SetField( iOGRField, papszRow[iField] );
        }
    }

    return poFeature;
}

/************************************************************************/
/*                         GetNextRawFeature()                          */
/************************************************************************/

OGRFeature *OGRMySQLLayer::GetNextRawFeature()

{
/* -------------------------------------------------------------------- */
/*      Do we need to establish an initial query?                       */
/* -------------------------------------------------------------------- */
    if( iNextShapeId == 0 && hResultSet == nullptr )
    {
        CPLAssert( pszQueryStatement != nullptr );

        poDS->RequestLongResult( this );

        if( mysql_query( poDS->GetConn(), pszQueryStatement ) )
        {
            poDS->ReportError( pszQueryStatement );
            return nullptr;
        }

        hResultSet = mysql_use_result( poDS->GetConn() );
        if( hResultSet == nullptr )
        {
            poDS->ReportError( "mysql_use_result() failed on query." );
            return nullptr;
        }
    }

/* -------------------------------------------------------------------- */
/*      Fetch next record.                                              */
/* -------------------------------------------------------------------- */
    char **papszRow;
    unsigned long *panLengths;

    papszRow = mysql_fetch_row( hResultSet );
    if( papszRow == nullptr )
    {
        ResetReading();
        return nullptr;
    }

    panLengths = mysql_fetch_lengths( hResultSet );

/* -------------------------------------------------------------------- */
/*      Process record.                                                 */
/* -------------------------------------------------------------------- */
    OGRFeature *poFeature = RecordToFeature( papszRow, panLengths );

    iNextShapeId++;

    return poFeature;
}

/************************************************************************/
/*                             GetFeature()                             */
/*                                                                      */
/*      Note that we actually override this in OGRMySQLTableLayer.      */
/************************************************************************/

OGRFeature *OGRMySQLLayer::GetFeature( GIntBig nFeatureId )

{
    return OGRLayer::GetFeature( nFeatureId );
}

/************************************************************************/
/*                            GetFIDColumn()                            */
/************************************************************************/

const char *OGRMySQLLayer::GetFIDColumn()

{
    if( pszFIDColumn != nullptr )
        return pszFIDColumn;
    else
        return "";
}

/************************************************************************/
/*                         FetchSRSId()                                 */
/************************************************************************/

int OGRMySQLLayer::FetchSRSId()
{
    CPLString        osCommand;
    char           **papszRow;

    if( hResultSet != nullptr )
        mysql_free_result( hResultSet );
    hResultSet = nullptr;

    if( poDS->GetMajorVersion() < 8 || poDS->IsMariaDB() )
    {
        osCommand.Printf(
                 "SELECT srid FROM geometry_columns "
                 "WHERE f_table_name = '%s'",
                 pszGeomColumnTable );
    }
    else
    {
        osCommand.Printf(
                "SELECT SRS_ID FROM INFORMATION_SCHEMA.ST_GEOMETRY_COLUMNS "
                "WHERE TABLE_NAME = '%s'",
                pszGeomColumnTable );
    }

    if( !mysql_query( poDS->GetConn(), osCommand ) )
        hResultSet = mysql_store_result( poDS->GetConn() );

    papszRow = nullptr;
    if( hResultSet != nullptr )
        papszRow = mysql_fetch_row( hResultSet );

    if( papszRow != nullptr && papszRow[0] != nullptr )
    {
        nSRSId = atoi(papszRow[0]);
    }

    // make sure to free our results
    if( hResultSet != nullptr )
        mysql_free_result( hResultSet );
    hResultSet = nullptr;

    return nSRSId;
}

/************************************************************************/
/*                           GetSpatialRef()                            */
/************************************************************************/

OGRSpatialReference *OGRMySQLLayer::GetSpatialRef()

{
    if( poSRS == nullptr && nSRSId > -1 )
    {
        poSRS = poDS->FetchSRS( nSRSId );
        if( poSRS != nullptr )
            poSRS->Reference();
        else
            nSRSId = poDS->GetUnknownSRID();
    }

    return poSRS;
}
