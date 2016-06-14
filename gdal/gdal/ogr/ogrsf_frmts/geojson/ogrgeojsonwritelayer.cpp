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
                                            bool bWriteFC_BBOXIn,
                                           OGRGeoJSONDataSource* poDS )
    : poDS_( poDS ), poFeatureDefn_(new OGRFeatureDefn( pszName ) ), nOutCounter_( 0 )
{
    bWriteBBOX = CPLTestBool(CSLFetchNameValueDef(
        papszOptions, "WRITE_BBOX", "FALSE"));
    bBBOX3D = false;
    bWriteFC_BBOX = bWriteFC_BBOXIn;

    poFeatureDefn_->Reference();
    poFeatureDefn_->SetGeomType( eGType );
    SetDescription( poFeatureDefn_->GetName() );

    nCoordPrecision_ = atoi(CSLFetchNameValueDef(papszOptions, "COORDINATE_PRECISION", "-1"));
    nSignificantFigures_ = atoi(CSLFetchNameValueDef(papszOptions, "SIGNIFICANT_FIGURES", "-1"));
}

/************************************************************************/
/*                        ~OGRGeoJSONWriteLayer()                       */
/************************************************************************/

OGRGeoJSONWriteLayer::~OGRGeoJSONWriteLayer()
{
    VSILFILE* fp = poDS_->GetOutputFile();

    VSIFPrintfL( fp, "\n]" );

    if( bWriteFC_BBOX && sEnvelopeLayer.IsInit() )
    {
        CPLString osBBOX = "[ ";
        osBBOX += CPLSPrintf("%.15g, ", sEnvelopeLayer.MinX);
        osBBOX += CPLSPrintf("%.15g, ", sEnvelopeLayer.MinY);
        if( bBBOX3D )
            osBBOX += CPLSPrintf("%.15g, ", sEnvelopeLayer.MinZ);
        osBBOX += CPLSPrintf("%.15g, ", sEnvelopeLayer.MaxX);
        osBBOX += CPLSPrintf("%.15g", sEnvelopeLayer.MaxY);
        if( bBBOX3D )
            osBBOX += CPLSPrintf(", %.15g", sEnvelopeLayer.MaxZ);
        osBBOX += " ]";

        if( poDS_->GetFpOutputIsSeekable() && osBBOX.size() + 9 < SPACE_FOR_BBOX )
        {
            VSIFSeekL(fp, poDS_->GetBBOXInsertLocation(), SEEK_SET);
            VSIFPrintfL( fp, "\"bbox\": %s,", osBBOX.c_str() );
            VSIFSeekL(fp, 0, SEEK_END);
        }
        else
        {
            VSIFPrintfL( fp, ",\n\"bbox\": %s", osBBOX.c_str() );
        }
    }

    VSIFPrintfL( fp, "\n}\n" );

    if( NULL != poFeatureDefn_ )
    {
        poFeatureDefn_->Release();
    }
}

/************************************************************************/
/*                           ICreateFeature()                            */
/************************************************************************/

OGRErr OGRGeoJSONWriteLayer::ICreateFeature( OGRFeature* poFeature )
{
    VSILFILE* fp = poDS_->GetOutputFile();

    if( NULL == poFeature )
    {
        CPLDebug( "GeoJSON", "Feature is null" );
        return OGRERR_INVALID_HANDLE;
    }

    json_object* poObj = OGRGeoJSONWriteFeature( poFeature, bWriteBBOX,
                                                 nCoordPrecision_, nSignificantFigures_ );
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
    if ( (bWriteBBOX || bWriteFC_BBOX) && !poGeometry->IsEmpty() )
    {
        OGREnvelope3D sEnvelope;
        poGeometry->getEnvelope(&sEnvelope);

        if( poGeometry->getCoordinateDimension() == 3 )
            bBBOX3D = true;

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
