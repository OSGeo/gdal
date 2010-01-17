/******************************************************************************
 * $Id$
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  Implementation of OGRGeoJSONLayer class (OGR GeoJSON Driver).
 * Author:   Mateusz Loskot, mateusz@loskot.net
 *
 ******************************************************************************
 * Copyright (c) 2007, Mateusz Loskot
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
#include "ogr_geojson.h"
#include "ogrgeojsonwriter.h"
#include <jsonc/json.h> // JSON-C
#include <algorithm> // for_each, find_if

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
                                  char** papszOptions,
                                  OGRGeoJSONDataSource* poDS )
    : iterCurrent_( seqFeatures_.end() ), poDS_( poDS ), poFeatureDefn_(new OGRFeatureDefn( pszName ) ), poSRS_( NULL ), nOutCounter_( 0 )
{
    UNREFERENCED_PARAM(papszOptions);

    CPLAssert( NULL != poDS_ );
    CPLAssert( NULL != poFeatureDefn_ );
    
    poFeatureDefn_->Reference();
    poFeatureDefn_->SetGeomType( eGType );

    if( NULL != poSRSIn )
    {
        SetSpatialRef( poSRSIn );
    }
}

/************************************************************************/
/*                          ~OGRGeoJSONLayer                            */
/************************************************************************/

OGRGeoJSONLayer::~OGRGeoJSONLayer()
{
    FILE* fp = poDS_->GetOutputFile();
    if( NULL != fp )
    {
        VSIFPrintfL( fp, "\n]\n}\n" );
    }

    std::for_each(seqFeatures_.begin(), seqFeatures_.end(),
                  OGRFeature::DestroyFeature);

    if( NULL != poFeatureDefn_ )
    {
        poFeatureDefn_->Release();
    }

    if( NULL != poSRS_ )
    {
        poSRS_->Release();   
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
/*                           GetSpatialRef                              */
/************************************************************************/

OGRSpatialReference* OGRGeoJSONLayer::GetSpatialRef()
{
    return poSRS_;
}

void OGRGeoJSONLayer::SetSpatialRef( OGRSpatialReference* poSRSIn )
{
    if( NULL == poSRSIn )
    {
        poSRS_ = NULL;
        // poSRS_ = new OGRSpatialReference();
        // if( OGRERR_NONE != poSRS_->importFromEPSG( 4326 ) )
        // {
        //     delete poSRS_;
        //     poSRS_ = NULL;
        // }
    }
    else
    {
        poSRS_ = poSRSIn->Clone(); 
    }
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

/*======================================================================*/
/*                           Features Filter Utilities                  */
/*======================================================================*/

/*******************************************/
/*          EvaluateSpatialFilter          */
/*******************************************/

bool OGRGeoJSONLayer::EvaluateSpatialFilter( OGRGeometry* poGeometry )
{
    return ( FilterGeometry( poGeometry ) == 0 ? false : true );
}

/*******************************************/
/*          SpatialFilterPredicate         */
/*******************************************/

struct SpatialFilterPredicate
{
    explicit SpatialFilterPredicate(OGRGeoJSONLayer& layer)
        : layer_(layer)
    {}
    bool operator()( OGRFeature* p )
    {
        return layer_.EvaluateSpatialFilter( p->GetGeometryRef() );
    }

private:

    OGRGeoJSONLayer& layer_;
};

/*******************************************/
/*          AttributeFilterPredicate       */
/*******************************************/

struct AttributeFilterPredicate
{
    explicit AttributeFilterPredicate(OGRFeatureQuery& query)
        : query_(query)
    {}

    bool operator()( OGRFeature* p )
    {
        return ( query_.Evaluate( p ) == 0 ? false : true );
    }

private:

    OGRFeatureQuery& query_;
};

/************************************************************************/
/*                           GetNextFeature                             */
/************************************************************************/

OGRFeature* OGRGeoJSONLayer::GetNextFeature()
{
    bool bSingle = false;

    if( NULL != m_poFilterGeom )
    {
        iterCurrent_ = std::find_if( iterCurrent_, seqFeatures_.end(),
                       SpatialFilterPredicate(*this) );
        bSingle = (iterCurrent_ != seqFeatures_.end());
    }

    if( NULL != m_poAttrQuery )
    {
        FeaturesSeq::iterator seqEnd = 
            ( bSingle ? iterCurrent_ : seqFeatures_.end() );

        iterCurrent_ = std::find_if( iterCurrent_, seqEnd,
                       AttributeFilterPredicate(*m_poAttrQuery) );
    }

    if( iterCurrent_ != seqFeatures_.end() )
    {
        OGRFeature* poFeature = (*iterCurrent_);
        CPLAssert( NULL != poFeature );

        OGRFeature* poFeatureCopy = poFeature->Clone();
        CPLAssert( NULL != poFeatureCopy );

        if (poFeatureCopy->GetGeometryRef() != NULL && poSRS_ != NULL)
        {
            poFeatureCopy->GetGeometryRef()->assignSpatialReference( poSRS_ );
        }

        ++iterCurrent_;
        return poFeatureCopy;
    }

    return NULL;
}

/************************************************************************/
/*                           GetFeature                             */
/************************************************************************/

OGRFeature* OGRGeoJSONLayer::GetFeature( long nFID )
{
	OGRFeature* poFeature = NULL;
	poFeature = OGRLayer::GetFeature( nFID );

	return poFeature;
}

/************************************************************************/
/*                           CreateFeature                              */
/************************************************************************/

OGRErr OGRGeoJSONLayer::CreateFeature( OGRFeature* poFeature )
{
    FILE* fp = poDS_->GetOutputFile();
    if( NULL == poFeature )
    {
        CPLDebug( "GeoJSON", "Target datasource file is invalid." );
        return CE_Failure;
    }

    if( NULL == poFeature )
    {
        CPLDebug( "GeoJSON", "Feature is null" );
        return OGRERR_INVALID_HANDLE;
    }

    json_object* poObj = OGRGeoJSONWriteFeature( poFeature );
    CPLAssert( NULL != poObj );

    if( nOutCounter_ > 0 )
    {
        /* Separate "Feature" entries in "FeatureCollection" object. */
        VSIFPrintfL( fp, ",\n" );
    }
    VSIFPrintfL( fp, "%s\n", json_object_to_json_string( poObj ) );

    json_object_put( poObj );

    ++nOutCounter_;

    return OGRERR_NONE;
}

OGRErr OGRGeoJSONLayer::CreateField(OGRFieldDefn* poField, int bApproxOK)
{
    UNREFERENCED_PARAM(bApproxOK);

    for( int i = 0; i < poFeatureDefn_->GetFieldCount(); ++i )
    {
        OGRFieldDefn* poDefn = poFeatureDefn_->GetFieldDefn(i);
        CPLAssert( NULL != poDefn );

        if( EQUAL( poDefn->GetNameRef(), poField->GetNameRef() ) )
        {
            CPLDebug( "GeoJSON", "Field '%s' already present in schema",
                      poField->GetNameRef() );
            
            // TODO - mloskot: Is this return code correct?
            return OGRERR_NONE;
        }
    }

    poFeatureDefn_->AddFieldDefn( poField );

    return OGRERR_NONE;
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
        if( -1 != nField )
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
