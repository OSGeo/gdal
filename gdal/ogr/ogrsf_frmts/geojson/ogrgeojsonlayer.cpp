/******************************************************************************
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

#include <algorithm>

#if !DEBUG_JSON
#  ifdef __clang__
#    pragma clang diagnostic push
#    pragma clang diagnostic ignored "-Wunknown-pragmas"
#    pragma clang diagnostic ignored "-Wdocumentation"
#  endif
#endif  // !DEBUG_VERBOSE

#include <json.h>

#if !DEBUG_JSON
#  ifdef __clang
#    pragma clang diagnostic pop
#  endif
#endif  // !DEBUG_VERBOSE

#include "ogr_geojson.h"

// Remove annoying warnings Microsoft Visual C++:
//   'class': assignment operator could not be generated.
//     The compiler cannot generate an assignment operator for the given
//     class. No assignment operator was created.
#if defined(_MSC_VER)
#  pragma warning(disable:4512)
#endif

CPL_CVSID("$Id$")

/************************************************************************/
/*                       STATIC MEMBERS DEFINITION                      */
/************************************************************************/

const char* const OGRGeoJSONLayer::DefaultName = "OGRGeoJSON";
const OGRwkbGeometryType OGRGeoJSONLayer::DefaultGeometryType = wkbUnknown;

/************************************************************************/
/*                           OGRGeoJSONLayer                            */
/************************************************************************/

OGRGeoJSONLayer::OGRGeoJSONLayer( const char* pszName,
                                  OGRSpatialReference* poSRSIn,
                                  OGRwkbGeometryType eGType,
                                  OGRGeoJSONDataSource* poDS ) :
    OGRMemLayer( pszName, poSRSIn, eGType),
    poDS_(poDS),
    bUpdated_(false),
    bOriginalIdModified_(false)
{
    SetAdvertizeUTF8(true);
    SetUpdatable( poDS->IsUpdatable() );
}

/************************************************************************/
/*                          ~OGRGeoJSONLayer                            */
/************************************************************************/

OGRGeoJSONLayer::~OGRGeoJSONLayer() {}

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
    GIntBig nFID = poFeature->GetFID();

    // Detect potential FID duplicates and make sure they are eventually
    // unique.
    if( -1 == nFID )
    {
        nFID = GetFeatureCount(FALSE);
        OGRFeature* poTryFeature = NULL;
        while( (poTryFeature = GetFeature(nFID) ) != NULL )
        {
            nFID++;
            delete poTryFeature;
        }
    }
    else
    {
        OGRFeature* poTryFeature = NULL;
        if( (poTryFeature = GetFeature(nFID) ) != NULL )
        {
            if( !bOriginalIdModified_ )
            {
                CPLError(
                    CE_Warning, CPLE_AppDefined,
                    "Several features with id = " CPL_FRMT_GIB " have been "
                    "found. Altering it to be unique. This warning will not "
                    "be emitted for this layer",
                    nFID );
                bOriginalIdModified_ = true;
            }
            delete poTryFeature;
            nFID = GetFeatureCount(FALSE);
            while( (poTryFeature = GetFeature(nFID) ) != NULL )
            {
                nFID++;
                delete poTryFeature;
            }
        }
    }
    poFeature->SetFID( nFID );

    if( !CPL_INT64_FITS_ON_INT32(nFID) )
        SetMetadataItem(OLMD_FID64, "YES");

    SetUpdatable( true );  // Temporary toggle on updatable flag.
    CPL_IGNORE_RET_VAL(OGRMemLayer::SetFeature(poFeature));
    SetUpdatable( poDS_->IsUpdatable() );
    SetUpdated( false );
}

/************************************************************************/
/*                           DetectGeometryType                         */
/************************************************************************/

void OGRGeoJSONLayer::DetectGeometryType()
{
    if( GetLayerDefn()->GetGeomType() != wkbUnknown )
        return;

    ResetReading();
    bool bFirstGeometry = true;
    OGRwkbGeometryType eLayerGeomType = wkbUnknown;
    OGRFeature* poFeature = NULL;
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
