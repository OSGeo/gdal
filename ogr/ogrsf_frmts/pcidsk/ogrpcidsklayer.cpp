/******************************************************************************
 * $Id: ogrcsvlayer.cpp 17496 2009-08-02 11:54:23Z rouault $
 *
 * Project:  PCIDSK Translator
 * Purpose:  Implements OGRPCIDSKLayer class.
 * Author:   Frank Warmerdam <warmerdam@pobox.com>
 *
 ******************************************************************************
 * Copyright (c) 2009, Frank Warmerdam <warmerdam@pobox.com>
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

#include "ogr_pcidsk.h"

CPL_CVSID("$Id: ogrcsvlayer.cpp 17496 2009-08-02 11:54:23Z rouault $");

/************************************************************************/
/*                           OGRPCIDSKLayer()                           */
/************************************************************************/

OGRPCIDSKLayer::OGRPCIDSKLayer( PCIDSK::PCIDSKSegment *poSegIn )

{
    poSeg = poSegIn;
    poVecSeg = dynamic_cast<PCIDSK::PCIDSKVectorSegment*>( poSeg );

    poFeatureDefn = new OGRFeatureDefn( poSeg->GetName().c_str() );
    poFeatureDefn->Reference();
    //poFeatureDefn->SetGeomType( wkbNone );

    hLastShapeId = PCIDSK::NullShapeId;

/* -------------------------------------------------------------------- */
/*      Attempt to assign a geometry type.                              */
/* -------------------------------------------------------------------- */
    try {
        std::string osLayerType = poSeg->GetMetadataValue( "LAYER_TYPE" );
        
        if( osLayerType == "WHOLE_POLYGONS" )
            poFeatureDefn->SetGeomType( wkbPolygon25D );
        else if( osLayerType == "ARCS" || osLayerType == "TOPO_ARCS" )
            poFeatureDefn->SetGeomType( wkbLineString25D );
        else if( osLayerType == "POINTS" || osLayerType == "TOPO_NODES" )
            poFeatureDefn->SetGeomType( wkbPoint25D );
        else if( osLayerType == "TABLE" )
            poFeatureDefn->SetGeomType( wkbNone );
    } catch(...) {}

/* -------------------------------------------------------------------- */
/*      Build field definitions.                                        */
/* -------------------------------------------------------------------- */
    iRingStartField = -1;

    for( int iField = 0; iField < poVecSeg->GetFieldCount(); iField++ )
    {
        OGRFieldDefn oField( poVecSeg->GetFieldName(iField).c_str(), OFTString);

        switch( poVecSeg->GetFieldType(iField) )
        {
          case PCIDSK::FieldTypeFloat:
          case PCIDSK::FieldTypeDouble:
            oField.SetType( OFTReal );
            break;

          case PCIDSK::FieldTypeInteger:
            oField.SetType( OFTInteger );
            break;

          case PCIDSK::FieldTypeString:
            oField.SetType( OFTString );
            break;
            
          case PCIDSK::FieldTypeCountedInt:
            oField.SetType( OFTIntegerList );
            break;

          default:
            CPLAssert( FALSE );
            break;
        }

        // we ought to try and extract some width/precision information
        // from the format string at some point.

        // If the last field is named RingStart we treat it specially.
        if( EQUAL(oField.GetNameRef(),"RingStart")
            && oField.GetType() == OFTIntegerList 
            && iField == poVecSeg->GetFieldCount()-1 )
            iRingStartField = iField;
        else
            poFeatureDefn->AddFieldDefn( &oField );
    }
}

/************************************************************************/
/*                          ~OGRPCIDSKLayer()                           */
/************************************************************************/

OGRPCIDSKLayer::~OGRPCIDSKLayer()

{
    if( m_nFeaturesRead > 0 && poFeatureDefn != NULL )
    {
        CPLDebug( "PCIDSK", "%d features read on layer '%s'.",
                  (int) m_nFeaturesRead, 
                  poFeatureDefn->GetName() );
    }

    poFeatureDefn->Release();
}

/************************************************************************/
/*                            ResetReading()                            */
/************************************************************************/

void OGRPCIDSKLayer::ResetReading()

{
    hLastShapeId = PCIDSK::NullShapeId;
}

/************************************************************************/
/*                      GetNextUnfilteredFeature()                      */
/************************************************************************/

OGRFeature * OGRPCIDSKLayer::GetNextUnfilteredFeature()

{
    unsigned int i;

/* -------------------------------------------------------------------- */
/*      Get the next shapeid.                                           */
/* -------------------------------------------------------------------- */
    if( hLastShapeId == PCIDSK::NullShapeId )
        hLastShapeId = poVecSeg->FindFirst();
    else
        hLastShapeId = poVecSeg->FindNext( hLastShapeId );

    if( hLastShapeId == PCIDSK::NullShapeId )
        return NULL;

/* -------------------------------------------------------------------- */
/*      Create the OGR feature.                                         */
/* -------------------------------------------------------------------- */
    OGRFeature *poFeature;

    poFeature = new OGRFeature( poFeatureDefn );
    poFeature->SetFID( (int) hLastShapeId );

/* -------------------------------------------------------------------- */
/*      Set attributes for any indicated attribute records.             */
/* -------------------------------------------------------------------- */
    std::vector<PCIDSK::ShapeField> aoFields;

    poVecSeg->GetFields( hLastShapeId, aoFields );
    for( i=0; i < aoFields.size(); i++ )
    {
        if( (int) i == iRingStartField )
            continue;

        switch( aoFields[i].GetType() )
        {
          case PCIDSK::FieldTypeNone:
            // null field value.
            break;

          case PCIDSK::FieldTypeInteger:
            poFeature->SetField( i, aoFields[i].GetValueInteger() );
            break;
                                 
          case PCIDSK::FieldTypeFloat:
            poFeature->SetField( i, aoFields[i].GetValueFloat() );
            break;
                                 
          case PCIDSK::FieldTypeDouble:
            poFeature->SetField( i, aoFields[i].GetValueDouble() );
            break;
                                 
          case PCIDSK::FieldTypeString:
            poFeature->SetField( i, aoFields[i].GetValueString().c_str() );
            break;
                                 
          case PCIDSK::FieldTypeCountedInt:
            std::vector<PCIDSK::int32> list = aoFields[i].GetValueCountedInt();
            
            poFeature->SetField( i, list.size(), &(list[0]) );
            break;
        }
    }

/* -------------------------------------------------------------------- */
/*      Translate the geometry.                                         */
/* -------------------------------------------------------------------- */
    std::vector<PCIDSK::ShapeVertex> aoVertices;

    poVecSeg->GetVertices( hLastShapeId, aoVertices );

/* -------------------------------------------------------------------- */
/*      Point                                                           */
/* -------------------------------------------------------------------- */
    if( poFeatureDefn->GetGeomType() == wkbPoint25D 
        || (wkbFlatten(poFeatureDefn->GetGeomType()) == wkbUnknown 
            && aoVertices.size() == 1) )
    {
        if( aoVertices.size() == 1 )
        {
            poFeature->SetGeometryDirectly(
                new OGRPoint( aoVertices[0].x, 
                              aoVertices[0].y, 
                              aoVertices[0].z ) );
        }
        else
        {
            // report issue?
        }
    }    

/* -------------------------------------------------------------------- */
/*      LineString                                                      */
/* -------------------------------------------------------------------- */
    else if( poFeatureDefn->GetGeomType() == wkbLineString25D 
             || (wkbFlatten(poFeatureDefn->GetGeomType()) == wkbUnknown 
                 && aoVertices.size() > 1) )
    {
        // We should likely be applying ringstart to break things into 
        // a multilinestring in some cases.
        if( aoVertices.size() > 1 )
        {
            OGRLineString *poLS = new OGRLineString();

            poLS->setNumPoints( aoVertices.size() );
            
            for( i = 0; i < aoVertices.size(); i++ )
                poLS->setPoint( i,
                                aoVertices[i].x, 
                                aoVertices[i].y, 
                                aoVertices[i].z );

            poFeature->SetGeometryDirectly( poLS );
        }
        else
        {
            // report issue?
        }
    }    

/* -------------------------------------------------------------------- */
/*      Polygon - Currently we have no way to recognise if we are       */
/*      dealing with a multipolygon when we have more than one          */
/*      ring.  Also, PCIDSK allows the rings to be in arbitrary         */
/*      order, not necessarily outside first which we are not yet       */
/*      ready to address in the following code.                         */
/* -------------------------------------------------------------------- */
    else if( poFeatureDefn->GetGeomType() == wkbPolygon25D )
    {
        std::vector<PCIDSK::int32> anRingStart;
        OGRPolygon *poPoly = new OGRPolygon();
        unsigned int iRing;

        if( iRingStartField != -1 )
            anRingStart = aoFields[iRingStartField].GetValueCountedInt();

        for( iRing = 0; iRing < anRingStart.size()+1; iRing++ )
        {
            unsigned int iStartVertex, iEndVertex, iVertex;
            OGRLinearRing *poRing = new OGRLinearRing();

            if( iRing == 0 )
                iStartVertex = 0;
            else
                iStartVertex = anRingStart[iRing-1];

            if( iRing == anRingStart.size() )
                iEndVertex = aoVertices.size() - 1;
            else
                iEndVertex = anRingStart[iRing] - 1;

            poRing->setNumPoints( iEndVertex - iStartVertex + 1 );
            for( iVertex = iStartVertex; iVertex <= iEndVertex; iVertex++ )
            {
                poRing->setPoint( iVertex - iStartVertex,
                                  aoVertices[iVertex].x, 
                                  aoVertices[iVertex].y, 
                                  aoVertices[iVertex].z );
            }

            poPoly->addRingDirectly( poRing );
        }

        poFeature->SetGeometryDirectly( poPoly );
    }    
    
    m_nFeaturesRead++;

    return poFeature;
}


/************************************************************************/
/*                           GetNextFeature()                           */
/************************************************************************/

OGRFeature *OGRPCIDSKLayer::GetNextFeature()

{
    OGRFeature  *poFeature = NULL;

/* -------------------------------------------------------------------- */
/*      Read features till we find one that satisfies our current       */
/*      spatial criteria.                                               */
/* -------------------------------------------------------------------- */
    while( TRUE )
    {
        poFeature = GetNextUnfilteredFeature();
        if( poFeature == NULL )
            break;

        if( (m_poFilterGeom == NULL
            || FilterGeometry( poFeature->GetGeometryRef() ) )
            && (m_poAttrQuery == NULL
                || m_poAttrQuery->Evaluate( poFeature )) )
            break;

        delete poFeature;
    }

    return poFeature;
}

/************************************************************************/
/*                           TestCapability()                           */
/************************************************************************/

int OGRPCIDSKLayer::TestCapability( const char * pszCap )

{
    return FALSE;
}

