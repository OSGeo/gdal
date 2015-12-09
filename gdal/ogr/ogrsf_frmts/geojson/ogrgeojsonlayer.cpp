/******************************************************************************
 * $Id$
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  Implementation of OGRGeoJSONLayer class (OGR GeoJSON Driver).
 * Author:   Mateusz Loskot, mateusz@loskot.net
 *
 ******************************************************************************
 * Copyright (c) 2007, Mateusz Loskot
 * Copyright (c) 2010-2013, Even Rouault <even dot rouault at mines-paris dot org>
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
#include <algorithm> // for_each, find_if
#include <json.h> // JSON-C
#include "ogr_geojson.h"

/* Remove annoying warnings Microsoft Visual C++ */
#if defined(_MSC_VER)
#  pragma warning(disable:4512)
#endif

/************************************************************************/
/*                       STATIC MEMBERS DEFINITION                      */
/************************************************************************/

const char* const OGRGeoJSONLayer::DefaultName = "OGRGeoJSON";
const char* const OGRGeoJSONLayer::DefaultFIDColumn = "id";
const OGRwkbGeometryType OGRGeoJSONLayer::DefaultGeometryType = wkbUnknown;

/************************************************************************/
/*                           OGRGeoJSONLayer                            */
/************************************************************************/

OGRGeoJSONLayer::OGRGeoJSONLayer( const char* pszName,
                                  OGRSpatialReference* poSRSIn,
                                  OGRwkbGeometryType eGType,
                                  OGRGeoJSONDataSource* poDS )
  : OGRMemLayer( pszName, poSRSIn, eGType), poDS_(poDS), bUpdated_(false)
{
    SetAdvertizeUTF8(TRUE);
    SetUpdatable( poDS->IsUpdatable() ? TRUE : FALSE );
}

/************************************************************************/
/*                          ~OGRGeoJSONLayer                            */
/************************************************************************/

OGRGeoJSONLayer::~OGRGeoJSONLayer()
{
}

/************************************************************************/
/*                           GetFIDColumn                               */
/************************************************************************/

const char* OGRGeoJSONLayer::GetFIDColumn()
{
	return sFIDColumn_.c_str();
}

/************************************************************************/
/*                           SetFIDColumn                               */
/************************************************************************/

void OGRGeoJSONLayer::SetFIDColumn( const char* pszFIDColumn )
{
	sFIDColumn_ = pszFIDColumn;
}

/************************************************************************/
/*                           TestCapability()                           */
/************************************************************************/

int OGRGeoJSONLayer::TestCapability( const char * pszCap )

{
    if( EQUAL(pszCap, OLCCurveGeometries) )
        return FALSE;
    return OGRMemLayer::TestCapability(pszCap);
}

/************************************************************************/
/*                           SyncToDisk()                               */
/************************************************************************/

OGRErr OGRGeoJSONLayer::SyncToDisk()
{
    poDS_->FlushCache();
    return OGRERR_NONE;
}

/************************************************************************/
/*                           AddFeature                                 */
/************************************************************************/

void OGRGeoJSONLayer::AddFeature( OGRFeature* poFeature )
{
    if( -1 == poFeature->GetFID() )
    {
        GIntBig nFID = GetFeatureCount(FALSE);
        poFeature->SetFID( nFID );

        // TODO - mloskot: We need to redesign creation of FID column
        int nField = poFeature->GetFieldIndex( DefaultFIDColumn );
        if( -1 != nField && 
            (GetLayerDefn()->GetFieldDefn(nField)->GetType() == OFTInteger ||
             GetLayerDefn()->GetFieldDefn(nField)->GetType() == OFTInteger64 ))
        {
            poFeature->SetField( nField, nFID );
        }
    }

    GIntBig nFID = poFeature->GetFID();
    if( !CPL_INT64_FITS_ON_INT32(nFID) )
        SetMetadataItem(OLMD_FID64, "YES");

    SetUpdatable( TRUE ); /* temporary toggle on updatable flag */
    CPL_IGNORE_RET_VAL(OGRMemLayer::SetFeature(poFeature));
    SetUpdatable( poDS_->IsUpdatable() ? TRUE : FALSE );
    SetUpdated( FALSE );
}

/************************************************************************/
/*                           DetectGeometryType                         */
/************************************************************************/

void OGRGeoJSONLayer::DetectGeometryType()
{
    if (GetLayerDefn()->GetGeomType() != wkbUnknown)
        return;

    ResetReading();
    bool bFirstGeometry = true;
    OGRwkbGeometryType eLayerGeomType = wkbUnknown;
    OGRFeature* poFeature;
    while( (poFeature = GetNextFeature()) != NULL )
    {
        OGRGeometry* poGeometry = poFeature->GetGeometryRef();
        if( NULL != poGeometry )
        {
            OGRwkbGeometryType eGeomType = poGeometry->getGeometryType();
            if( bFirstGeometry )
            {
                eLayerGeomType = eGeomType;
                GetLayerDefn()->SetGeomType( eGeomType );
                bFirstGeometry = false;
            }
            else if( eGeomType != eLayerGeomType )
            {
                CPLDebug( "GeoJSON",
                    "Detected layer of mixed-geometry type features." );
                GetLayerDefn()->SetGeomType( DefaultGeometryType );
                delete poFeature;
                break;
            }
        }
        delete poFeature;
    }

    ResetReading();
}
