/******************************************************************************
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  Implementation of OGRGeoJSONWriteLayer class (OGR GeoJSON Driver).
 * Author:   Mateusz Loskot, mateusz@loskot.net
 *
 ******************************************************************************
 * Copyright (c) 2011, Even Rouault <even dot rouault at spatialys.com>
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

#include <algorithm>

CPL_CVSID("$Id$")

/************************************************************************/
/*                         OGRGeoJSONWriteLayer()                       */
/************************************************************************/

OGRGeoJSONWriteLayer::OGRGeoJSONWriteLayer( const char* pszName,
                                            OGRwkbGeometryType eGType,
                                            char** papszOptions,
                                            bool bWriteFC_BBOXIn,
                                            OGRCoordinateTransformation* poCT,
                                            OGRGeoJSONDataSource* poDS ) :
    poDS_(poDS),
    poFeatureDefn_(new OGRFeatureDefn( pszName )),
    nOutCounter_(0),
    bWriteBBOX(CPLTestBool(
        CSLFetchNameValueDef(papszOptions, "WRITE_BBOX", "FALSE"))),
    bBBOX3D(false),
    bWriteFC_BBOX(bWriteFC_BBOXIn),
    nCoordPrecision_(atoi(
        CSLFetchNameValueDef(papszOptions, "COORDINATE_PRECISION", "-1"))),
    nSignificantFigures_(atoi(
        CSLFetchNameValueDef(papszOptions, "SIGNIFICANT_FIGURES", "-1"))),
    bRFC7946_(CPLTestBool(
        CSLFetchNameValueDef(papszOptions, "RFC7946", "FALSE"))),
    poCT_(poCT)
{
    poFeatureDefn_->Reference();
    poFeatureDefn_->SetGeomType( eGType );
    SetDescription( poFeatureDefn_->GetName() );
    if( bRFC7946_ && nCoordPrecision_ < 0 )
        nCoordPrecision_ = 7;
    oWriteOptions_.bWriteBBOX = bWriteBBOX;
    oWriteOptions_.nCoordPrecision = nCoordPrecision_;
    oWriteOptions_.nSignificantFigures = nSignificantFigures_;
    if( bRFC7946_ )
    {
        oWriteOptions_.SetRFC7946Settings();
    }
    oWriteOptions_.SetIDOptions(papszOptions);
    oWriteOptions_.bAllowNonFiniteValues = CPLTestBool(
        CSLFetchNameValueDef(papszOptions, "WRITE_NON_FINITE_VALUES", "FALSE"));
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
        if( bRFC7946_ )
        {
            char szFormat[32];
            snprintf(szFormat, sizeof(szFormat), "%%.%df", nCoordPrecision_);
            osBBOX += CPLSPrintf(szFormat, sEnvelopeLayer.MinX);
            osBBOX += ", ";
            osBBOX += CPLSPrintf(szFormat, sEnvelopeLayer.MinY);
            osBBOX += ", ";
            if( bBBOX3D )
            {
                osBBOX += CPLSPrintf(szFormat, sEnvelopeLayer.MinZ);
                osBBOX += ", ";
            }
            osBBOX += CPLSPrintf(szFormat, sEnvelopeLayer.MaxX);
            osBBOX += ", ";
            osBBOX += CPLSPrintf(szFormat, sEnvelopeLayer.MaxY);
            if( bBBOX3D )
            {
                osBBOX += ", ";
                osBBOX += CPLSPrintf(szFormat, sEnvelopeLayer.MaxZ);
            }
        }
        else
        {
            osBBOX += CPLSPrintf("%.15g, ", sEnvelopeLayer.MinX);
            osBBOX += CPLSPrintf("%.15g, ", sEnvelopeLayer.MinY);
            if( bBBOX3D )
                osBBOX += CPLSPrintf("%.15g, ", sEnvelopeLayer.MinZ);
            osBBOX += CPLSPrintf("%.15g, ", sEnvelopeLayer.MaxX);
            osBBOX += CPLSPrintf("%.15g", sEnvelopeLayer.MaxY);
            if( bBBOX3D )
                osBBOX += CPLSPrintf(", %.15g", sEnvelopeLayer.MaxZ);
        }
        osBBOX += " ]";

        if( poDS_->GetFpOutputIsSeekable() &&
            osBBOX.size() + 9 < OGRGeoJSONDataSource::SPACE_FOR_BBOX )
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

    if( nullptr != poFeatureDefn_ )
    {
        poFeatureDefn_->Release();
    }

    delete poCT_;
}

/************************************************************************/
/*                           ICreateFeature()                            */
/************************************************************************/

OGRErr OGRGeoJSONWriteLayer::ICreateFeature( OGRFeature* poFeature )
{
    VSILFILE* fp = poDS_->GetOutputFile();

    OGRFeature* poFeatureToWrite;
    if( poCT_ != nullptr || bRFC7946_ )
    {
        poFeatureToWrite = new OGRFeature(poFeatureDefn_);
        poFeatureToWrite->SetFrom( poFeature );
        poFeatureToWrite->SetFID( poFeature->GetFID() );
        OGRGeometry* poGeometry = poFeatureToWrite->GetGeometryRef();
        if( poGeometry )
        {
            const char* const apszOptions[] = { "WRAPDATELINE=YES", nullptr };
            OGRGeometry* poNewGeom =
                OGRGeometryFactory::transformWithOptions(
                    poGeometry, poCT_, const_cast<char**>(apszOptions),
                    oTransformCache_);
            if( poNewGeom == nullptr )
            {
                delete poFeatureToWrite;
                return OGRERR_FAILURE;
            }

            OGREnvelope sEnvelope;
            poNewGeom->getEnvelope(&sEnvelope);
            if( sEnvelope.MinX < -180.0 || sEnvelope.MaxX > 180.0 ||
                sEnvelope.MinY < -90.0 || sEnvelope.MaxY > 90.0 )
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "Geometry extent outside of [-180.0,180.0]x[-90.0,90.0] bounds");
                delete poFeatureToWrite;
                return OGRERR_FAILURE;
            }

            poFeatureToWrite->SetGeometryDirectly( poNewGeom );
        }
    }
    else
    {
        poFeatureToWrite = poFeature;
    }

    if (oWriteOptions_.bGenerateID && poFeatureToWrite->GetFID() == OGRNullFID) {
        poFeatureToWrite->SetFID(nOutCounter_);
    }
    json_object* poObj =
        OGRGeoJSONWriteFeature( poFeatureToWrite, oWriteOptions_ );
    CPLAssert( nullptr != poObj );

    if( nOutCounter_ > 0 )
    {
        /* Separate "Feature" entries in "FeatureCollection" object. */
        VSIFPrintfL( fp, ",\n" );
    }
    VSIFPrintfL( fp, "%s", json_object_to_json_string_ext( poObj,
        JSON_C_TO_STRING_SPACED
#ifdef JSON_C_TO_STRING_NOSLASHESCAPE
        | JSON_C_TO_STRING_NOSLASHESCAPE
#endif
    ) );

    json_object_put( poObj );

    ++nOutCounter_;

    OGRGeometry* poGeometry = poFeatureToWrite->GetGeometryRef();
    if( poGeometry != nullptr && !poGeometry->IsEmpty() )
    {
        OGREnvelope3D sEnvelope = OGRGeoJSONGetBBox( poGeometry,
                                                     oWriteOptions_ );
        if( poGeometry->getCoordinateDimension() == 3 )
            bBBOX3D = true;

        if( !sEnvelopeLayer.IsInit() )
        {
            sEnvelopeLayer = sEnvelope;
        }
        else if( oWriteOptions_.bBBOXRFC7946 )
        {
            const bool bEnvelopeCrossAM = ( sEnvelope.MinX > sEnvelope.MaxX );
            const bool bEnvelopeLayerCrossAM =
                                ( sEnvelopeLayer.MinX > sEnvelopeLayer.MaxX );
            if( bEnvelopeCrossAM )
            {
                if( bEnvelopeLayerCrossAM )
                {
                    sEnvelopeLayer.MinX = std::min(sEnvelopeLayer.MinX,
                                                   sEnvelope.MinX);
                    sEnvelopeLayer.MaxX = std::max(sEnvelopeLayer.MaxX,
                                                   sEnvelope.MaxX);
                }
                else
                {
                    if( sEnvelopeLayer.MinX > 0 )
                    {
                        sEnvelopeLayer.MinX = std::min(sEnvelopeLayer.MinX,
                                                       sEnvelope.MinX);
                        sEnvelopeLayer.MaxX = sEnvelope.MaxX;
                    }
                    else if( sEnvelopeLayer.MaxX < 0 )
                    {
                        sEnvelopeLayer.MaxX = std::max(sEnvelopeLayer.MaxX,
                                                       sEnvelope.MaxX);
                        sEnvelopeLayer.MinX = sEnvelope.MinX;
                    }
                    else
                    {
                        sEnvelopeLayer.MinX = -180.0;
                        sEnvelopeLayer.MaxX = 180.0;
                    }
                }
            }
            else if( bEnvelopeLayerCrossAM )
            {
                if( sEnvelope.MinX > 0 )
                {
                    sEnvelopeLayer.MinX = std::min(sEnvelopeLayer.MinX,
                                                   sEnvelope.MinX);
                }
                else if( sEnvelope.MaxX < 0 )
                {
                    sEnvelopeLayer.MaxX = std::max(sEnvelopeLayer.MaxX,
                                                   sEnvelope.MaxX);
                }
                else
                {
                    sEnvelopeLayer.MinX = -180.0;
                    sEnvelopeLayer.MaxX = 180.0;
                }
            }
            else
            {
                sEnvelopeLayer.MinX = std::min(sEnvelopeLayer.MinX,
                                               sEnvelope.MinX);
                sEnvelopeLayer.MaxX = std::max(sEnvelopeLayer.MaxX,
                                               sEnvelope.MaxX);
            }

            sEnvelopeLayer.MinY = std::min(sEnvelopeLayer.MinY, sEnvelope.MinY);
            sEnvelopeLayer.MaxY = std::max(sEnvelopeLayer.MaxY, sEnvelope.MaxY);
        }
        else
        {
            sEnvelopeLayer.Merge(sEnvelope);
        }
    }

    if( poFeatureToWrite != poFeature )
        delete poFeatureToWrite;

    return OGRERR_NONE;
}

/************************************************************************/
/*                           CreateField()                              */
/************************************************************************/

OGRErr OGRGeoJSONWriteLayer::CreateField( OGRFieldDefn* poField,
                                          int /* bApproxOK */  )
{
    if( poFeatureDefn_->GetFieldIndexCaseSensitive(poField->GetNameRef()) >= 0 )
    {
        CPLDebug( "GeoJSON", "Field '%s' already present in schema",
                    poField->GetNameRef() );

        // TODO - mloskot: Is this return code correct?
        return OGRERR_NONE;
    }

    poFeatureDefn_->AddFieldDefn( poField );

    return OGRERR_NONE;
}

/************************************************************************/
/*                           TestCapability()                           */
/************************************************************************/

int OGRGeoJSONWriteLayer::TestCapability( const char* pszCap )
{
    if( EQUAL(pszCap, OLCCreateField) )
        return TRUE;
    else if( EQUAL(pszCap, OLCSequentialWrite) )
        return TRUE;
    else if( EQUAL(pszCap, OLCStringsAsUTF8) )
        return TRUE;
    return FALSE;
}

/************************************************************************/
/*                            GetExtent()                               */
/************************************************************************/

OGRErr OGRGeoJSONWriteLayer::GetExtent(OGREnvelope *psExtent, int)
{
    if( sEnvelopeLayer.IsInit() )
    {
        *psExtent = sEnvelopeLayer;
        return OGRERR_NONE;
    }
    return OGRERR_FAILURE;
}
