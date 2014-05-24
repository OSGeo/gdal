/******************************************************************************
 * $Id$
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  Implementation of OGRGeoJSONWriteLayer class (OGR GeoJSON Driver).
 * Author:   Mateusz Loskot, mateusz@loskot.net
 *
 ******************************************************************************
 * Copyright (c) 2011, Even Rouault <even dot rouault at mines-paris dot org>
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

/* Remove annoying warnings Microsoft Visual C++ */
#if defined(_MSC_VER)
#  pragma warning(disable:4512)
#endif

/************************************************************************/
/*                         OGRGeoJSONWriteLayer()                       */
/************************************************************************/

OGRGeoJSONWriteLayer::OGRGeoJSONWriteLayer( const char* pszName,
                                  OGRwkbGeometryType eGType,
                                  char** papszOptions,
                                  OGRGeoJSONDataSource* poDS )
    : poDS_( poDS ), poFeatureDefn_(new OGRFeatureDefn( pszName ) ), nOutCounter_( 0 )
{
    bWriteBBOX = CSLTestBoolean(CSLFetchNameValueDef(papszOptions, "WRITE_BBOX", "FALSE"));
    bBBOX3D = FALSE;

    poFeatureDefn_->Reference();
    poFeatureDefn_->SetGeomType( eGType );
    SetDescription( poFeatureDefn_->GetName() );

    nCoordPrecision = atoi(CSLFetchNameValueDef(papszOptions, "COORDINATE_PRECISION", "-1"));
}

/************************************************************************/
/*                        ~OGRGeoJSONWriteLayer()                       */
/************************************************************************/

OGRGeoJSONWriteLayer::~OGRGeoJSONWriteLayer()
{
    VSILFILE* fp = poDS_->GetOutputFile();

    VSIFPrintfL( fp, "\n]" );

    if( bWriteBBOX && sEnvelopeLayer.IsInit() )
    {
        json_object* poObjBBOX = json_object_new_array();
        json_object_array_add(poObjBBOX,
                        json_object_new_double_with_precision(sEnvelopeLayer.MinX, nCoordPrecision));
        json_object_array_add(poObjBBOX,
                        json_object_new_double_with_precision(sEnvelopeLayer.MinY, nCoordPrecision));
        if( bBBOX3D )
            json_object_array_add(poObjBBOX,
                        json_object_new_double_with_precision(sEnvelopeLayer.MinZ, nCoordPrecision));
        json_object_array_add(poObjBBOX,
                        json_object_new_double_with_precision(sEnvelopeLayer.MaxX, nCoordPrecision));
        json_object_array_add(poObjBBOX,
                        json_object_new_double_with_precision(sEnvelopeLayer.MaxY, nCoordPrecision));
        if( bBBOX3D )
            json_object_array_add(poObjBBOX,
                        json_object_new_double_with_precision(sEnvelopeLayer.MaxZ, nCoordPrecision));

        const char* pszBBOX = json_object_to_json_string( poObjBBOX );
        if( poDS_->GetFpOutputIsSeekable() )
        {
            VSIFSeekL(fp, poDS_->GetBBOXInsertLocation(), SEEK_SET);
            if (strlen(pszBBOX) + 9 < SPACE_FOR_BBOX)
                VSIFPrintfL( fp, "\"bbox\": %s,", pszBBOX );
            VSIFSeekL(fp, 0, SEEK_END);
        }
        else
        {
            VSIFPrintfL( fp, ",\n\"bbox\": %s", pszBBOX );
        }

        json_object_put( poObjBBOX );
    }

    VSIFPrintfL( fp, "\n}\n" );

    if( NULL != poFeatureDefn_ )
    {
        poFeatureDefn_->Release();
    }
}

/************************************************************************/
/*                           CreateFeature()                            */
/************************************************************************/

OGRErr OGRGeoJSONWriteLayer::CreateFeature( OGRFeature* poFeature )
{
    VSILFILE* fp = poDS_->GetOutputFile();

    if( NULL == poFeature )
    {
        CPLDebug( "GeoJSON", "Feature is null" );
        return OGRERR_INVALID_HANDLE;
    }

    json_object* poObj = OGRGeoJSONWriteFeature( poFeature, bWriteBBOX, nCoordPrecision );
    CPLAssert( NULL != poObj );

    if( nOutCounter_ > 0 )
    {
        /* Separate "Feature" entries in "FeatureCollection" object. */
        VSIFPrintfL( fp, ",\n" );
    }
    VSIFPrintfL( fp, "%s", json_object_to_json_string( poObj ) );

    json_object_put( poObj );

    ++nOutCounter_;

    OGRGeometry* poGeometry = poFeature->GetGeometryRef();
    if ( bWriteBBOX && !poGeometry->IsEmpty() )
    {
        OGREnvelope3D sEnvelope;
        poGeometry->getEnvelope(&sEnvelope);

        if( poGeometry->getCoordinateDimension() == 3 )
            bBBOX3D = TRUE;

        sEnvelopeLayer.Merge(sEnvelope);
    }

    return OGRERR_NONE;
}

/************************************************************************/
/*                           CreateField()                              */
/************************************************************************/

OGRErr OGRGeoJSONWriteLayer::CreateField(OGRFieldDefn* poField, int bApproxOK)
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
/*                           TestCapability()                           */
/************************************************************************/

int OGRGeoJSONWriteLayer::TestCapability( const char* pszCap )
{
    if (EQUAL(pszCap, OLCCreateField))
        return TRUE;
    else if (EQUAL(pszCap, OLCSequentialWrite))
        return TRUE;

    return FALSE;
}
