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
    : iterCurrent_( seqFeatures_.end() ), poDS_( poDS ), poFeatureDefn_(new OGRFeatureDefn( pszName ) )
{
    CPLAssert( NULL != poDS_ );
    CPLAssert( NULL != poFeatureDefn_ );
    
    poFeatureDefn_->Reference();
    poFeatureDefn_->SetGeomType( eGType );
    if( poFeatureDefn_->GetGeomFieldCount() != 0 )
        poFeatureDefn_->GetGeomFieldDefn(0)->SetSpatialRef(poSRSIn);
    SetDescription( poFeatureDefn_->GetName() );
}

/************************************************************************/
/*                          ~OGRGeoJSONLayer                            */
/************************************************************************/

OGRGeoJSONLayer::~OGRGeoJSONLayer()
{
    std::for_each(seqFeatures_.begin(), seqFeatures_.end(),
                  OGRFeature::DestroyFeature);

    if( NULL != poFeatureDefn_ )
    {
        poFeatureDefn_->Release();
    }
}

/************************************************************************/
/*                           GetLayerDefn                               */
/************************************************************************/

OGRFeatureDefn* OGRGeoJSONLayer::GetLayerDefn()
{
    return poFeatureDefn_;
}

/************************************************************************/
/*                           GetFeatureCount                            */
/************************************************************************/

int OGRGeoJSONLayer::GetFeatureCount( int bForce )
{
    if (m_poFilterGeom == NULL && m_poAttrQuery == NULL)
        return static_cast<int>( seqFeatures_.size() );
    else
        return OGRLayer::GetFeatureCount(bForce);
}

/************************************************************************/
/*                           ResetReading                               */
/************************************************************************/

void OGRGeoJSONLayer::ResetReading()
{
    iterCurrent_ = seqFeatures_.begin();
}

/************************************************************************/
/*                           GetNextFeature                             */
/************************************************************************/

OGRFeature* OGRGeoJSONLayer::GetNextFeature()
{
    while ( iterCurrent_ != seqFeatures_.end() )
    {
        OGRFeature* poFeature = (*iterCurrent_);
        CPLAssert( NULL != poFeature );
        ++iterCurrent_;
        
        if((m_poFilterGeom == NULL
            || FilterGeometry( poFeature->GetGeometryRef() ) )
        && (m_poAttrQuery == NULL
            || m_poAttrQuery->Evaluate( poFeature )) )
        {
            OGRFeature* poFeatureCopy = poFeature->Clone();
            CPLAssert( NULL != poFeatureCopy );

            if (poFeatureCopy->GetGeometryRef() != NULL && GetSpatialRef() != NULL)
            {
                poFeatureCopy->GetGeometryRef()->assignSpatialReference( GetSpatialRef() );
            }

            return poFeatureCopy;
        }
    }

    return NULL;
}

/************************************************************************/
/*                           TestCapability                             */
/************************************************************************/

int OGRGeoJSONLayer::TestCapability( const char* pszCap )
{
    UNREFERENCED_PARAM(pszCap);

    return FALSE;
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
/*                           AddFeature                                 */
/************************************************************************/

void OGRGeoJSONLayer::AddFeature( OGRFeature* poFeature )
{
    CPLAssert( NULL != poFeature );

    // NOTE - mloskot:
    // Features may not be sorted according to FID values.

    // TODO: Should we check if feature already exists?
    // TODO: Think about sync operation, upload, etc.

    OGRFeature* poNewFeature = NULL;
    poNewFeature = poFeature->Clone();


    if( -1 == poNewFeature->GetFID() )
    {
        int nFID = static_cast<int>(seqFeatures_.size());
        poNewFeature->SetFID( nFID );

        // TODO - mlokot: We need to redesign creation of FID column
        int nField = poNewFeature->GetFieldIndex( DefaultFIDColumn );
        if( -1 != nField && GetLayerDefn()->GetFieldDefn(nField)->GetType() == OFTInteger )
        {
            poNewFeature->SetField( nField, nFID );
        }
    }

    seqFeatures_.push_back( poNewFeature );
}

/************************************************************************/
/*                           DetectGeometryType                         */
/************************************************************************/

void OGRGeoJSONLayer::DetectGeometryType()
{
    if (poFeatureDefn_->GetGeomType() != wkbUnknown)
        return;

    OGRwkbGeometryType featType = wkbUnknown;
    OGRGeometry* poGeometry = NULL;
    FeaturesSeq::const_iterator it = seqFeatures_.begin();
    FeaturesSeq::const_iterator end = seqFeatures_.end();
    
    if( it != end )
    {
        poGeometry = (*it)->GetGeometryRef();
        if( NULL != poGeometry )
        {
            featType = poGeometry->getGeometryType();
            if( featType != poFeatureDefn_->GetGeomType() )
            {
                poFeatureDefn_->SetGeomType( featType );
            }
        }
        ++it;
    }

    while( it != end )
    {
        poGeometry = (*it)->GetGeometryRef();
        if( NULL != poGeometry )
        {
            featType = poGeometry->getGeometryType();
            if( featType != poFeatureDefn_->GetGeomType() )
            {
                CPLDebug( "GeoJSON",
                    "Detected layer of mixed-geometry type features." );
                poFeatureDefn_->SetGeomType( DefaultGeometryType );
                break;
            }
        }
        ++it;
    }
}
