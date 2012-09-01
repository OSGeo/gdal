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

OGRPCIDSKLayer::OGRPCIDSKLayer( PCIDSK::PCIDSKSegment *poSegIn,
                                bool bUpdate )

{
    poSRS = NULL;
    bUpdateAccess = bUpdate;
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
    try
    {
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

/* -------------------------------------------------------------------- */
/*      Look for a coordinate system.                                   */
/* -------------------------------------------------------------------- */
        CPLString osGeosys;
        const char *pszUnits = NULL;
        std::vector<double> adfParameters;

        adfParameters = poVecSeg->GetProjection( osGeosys );

        if( ((PCIDSK::UnitCode)(int)adfParameters[16]) 
            == PCIDSK::UNIT_DEGREE )
            pszUnits = "DEGREE";
        else if( ((PCIDSK::UnitCode)(int)adfParameters[16]) 
                 == PCIDSK::UNIT_METER )
            pszUnits = "METER";
        else if( ((PCIDSK::UnitCode)(int)adfParameters[16]) 
                 == PCIDSK::UNIT_US_FOOT )
            pszUnits = "FOOT";
        else if( ((PCIDSK::UnitCode)(int)adfParameters[16]) 
                 == PCIDSK::UNIT_INTL_FOOT )
            pszUnits = "INTL FOOT";

        poSRS = new OGRSpatialReference();

        if( poSRS->importFromPCI( osGeosys, pszUnits, 
                                  &(adfParameters[0]) ) != OGRERR_NONE )
        {
            delete poSRS;
            poSRS = NULL;
        }
    }

/* -------------------------------------------------------------------- */
/*      Trap pcidsk exceptions.                                         */
/* -------------------------------------------------------------------- */
    catch( PCIDSK::PCIDSKException ex )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "PCIDSK Exception while initializing layer, operation likely impaired.\n%s", ex.what() );
    }
    catch(...)
    {
        CPLError( CE_Failure, CPLE_AppDefined, 
                  "Non-PCIDSK exception trapped while initializing layer, operation likely impaired." );
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

    if (poSRS)
        poSRS->Release();
}

/************************************************************************/
/*                           GetSpatialRef()                            */
/************************************************************************/

OGRSpatialReference *OGRPCIDSKLayer::GetSpatialRef()

{
    return poSRS;
}

/************************************************************************/
/*                            ResetReading()                            */
/************************************************************************/

void OGRPCIDSKLayer::ResetReading()

{
    hLastShapeId = PCIDSK::NullShapeId;
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
/*                      GetNextUnfilteredFeature()                      */
/************************************************************************/

OGRFeature * OGRPCIDSKLayer::GetNextUnfilteredFeature()

{
/* -------------------------------------------------------------------- */
/*      Get the next shapeid.                                           */
/* -------------------------------------------------------------------- */
    if( hLastShapeId == PCIDSK::NullShapeId )
        hLastShapeId = poVecSeg->FindFirst();
    else
        hLastShapeId = poVecSeg->FindNext( hLastShapeId );

    if( hLastShapeId == PCIDSK::NullShapeId )
        return NULL;

    return GetFeature( hLastShapeId );
}

/************************************************************************/
/*                             GetFeature()                             */
/************************************************************************/

OGRFeature *OGRPCIDSKLayer::GetFeature( long nFID )

{
/* -------------------------------------------------------------------- */
/*      Create the OGR feature.                                         */
/* -------------------------------------------------------------------- */
    OGRFeature *poFeature;

    poFeature = new OGRFeature( poFeatureDefn );
    poFeature->SetFID( (int) nFID );

/* -------------------------------------------------------------------- */
/*      Set attributes for any indicated attribute records.             */
/* -------------------------------------------------------------------- */
    try {
        std::vector<PCIDSK::ShapeField> aoFields;
        unsigned int i;

        poVecSeg->GetFields( (int) nFID, aoFields );
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

        poVecSeg->GetVertices( (int) nFID, aoVertices );

/* -------------------------------------------------------------------- */
/*      Point                                                           */
/* -------------------------------------------------------------------- */
        if( poFeatureDefn->GetGeomType() == wkbPoint25D 
            || (wkbFlatten(poFeatureDefn->GetGeomType()) == wkbUnknown 
                && aoVertices.size() == 1) )
        {
            if( aoVertices.size() == 1 )
            {
                OGRPoint* poPoint =
                    new OGRPoint( aoVertices[0].x,
                                  aoVertices[0].y, 
                                  aoVertices[0].z );
                if (poSRS)
                    poPoint->assignSpatialReference(poSRS);
                poFeature->SetGeometryDirectly(poPoint);
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
                if (poSRS)
                    poLS->assignSpatialReference(poSRS);

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
                int iStartVertex, iEndVertex, iVertex;
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

            if (poSRS)
                poPoly->assignSpatialReference(poSRS);

            poFeature->SetGeometryDirectly( poPoly );
        }    
    }
    
/* -------------------------------------------------------------------- */
/*      Trap exceptions and report as CPL errors.                       */
/* -------------------------------------------------------------------- */
    catch( PCIDSK::PCIDSKException ex )
    {
        delete poFeature;
        CPLError( CE_Failure, CPLE_AppDefined, 
                  "%s", ex.what() );
        return NULL;
    }
    catch(...)
    {
        delete poFeature;
        CPLError( CE_Failure, CPLE_AppDefined, 
                  "Non-PCIDSK exception trapped." );
        return NULL;
    }

    m_nFeaturesRead++;

    return poFeature;
}

/************************************************************************/
/*                           TestCapability()                           */
/************************************************************************/

int OGRPCIDSKLayer::TestCapability( const char * pszCap )

{
    if( EQUAL(pszCap,OLCRandomRead) )
        return TRUE;

    else if( EQUAL(pszCap,OLCFastFeatureCount) )
        return m_poFilterGeom == NULL && m_poAttrQuery == NULL;

    else if( EQUAL(pszCap,OLCSequentialWrite) 
             || EQUAL(pszCap,OLCRandomWrite) )
        return bUpdateAccess;

    else if( EQUAL(pszCap,OLCDeleteFeature) )
        return bUpdateAccess;

    else if( EQUAL(pszCap,OLCCreateField) )
        return bUpdateAccess;

    else 
        return FALSE;
}

/************************************************************************/
/*                          GetFeatureCount()                           */
/************************************************************************/

int OGRPCIDSKLayer::GetFeatureCount( int bForce )

{
    if( m_poFilterGeom != NULL || m_poAttrQuery != NULL )
        return OGRLayer::GetFeatureCount( bForce );
    else
    {
        try {
            return poVecSeg->GetShapeCount();
        } catch(...) {
            return 0;
        }
    }
}

/************************************************************************/
/*                             GetExtent()                              */
/************************************************************************/

OGRErr OGRPCIDSKLayer::GetExtent (OGREnvelope *psExtent, int bForce)

{
    if( !bForce )
        return OGRERR_FAILURE;
    
/* -------------------------------------------------------------------- */
/*      Loop over all features, but just read the geometry.  This is    */
/*      a fair amount quicker than actually processing all the          */
/*      attributes, forming features and then exaimining the            */
/*      geometries as the default implemntation would do.               */
/* -------------------------------------------------------------------- */
    try
    {
        bool bHaveExtent = FALSE;

        std::vector<PCIDSK::ShapeVertex> asVertices;

        for( PCIDSK::ShapeIterator oIt = poVecSeg->begin(); 
             oIt != poVecSeg->end();
             oIt++ )
        {
            unsigned int i;

            poVecSeg->GetVertices( *oIt, asVertices );

            for( i = 0; i < asVertices.size(); i++ )
            {
                if( !bHaveExtent )
                {
                    psExtent->MinX = psExtent->MaxX = asVertices[i].x;
                    psExtent->MinY = psExtent->MaxY = asVertices[i].y;
                    bHaveExtent = true;
                }
                else
                {
                    psExtent->MinX = MIN(psExtent->MinX,asVertices[i].x);
                    psExtent->MaxX = MAX(psExtent->MaxX,asVertices[i].x);
                    psExtent->MinY = MIN(psExtent->MinY,asVertices[i].y);
                    psExtent->MaxY = MAX(psExtent->MaxY,asVertices[i].y);
                }
            }
        }

        if( bHaveExtent )
            return OGRERR_NONE;
        else
            return OGRERR_FAILURE;
    }

/* -------------------------------------------------------------------- */
/*      Trap pcidsk exceptions.                                         */
/* -------------------------------------------------------------------- */
    catch( PCIDSK::PCIDSKException ex )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "PCIDSK Exception while initializing layer, operation likely impaired.\n%s", ex.what() );
        return OGRERR_FAILURE;
    }
    catch(...)
    {
        CPLError( CE_Failure, CPLE_AppDefined, 
                  "Non-PCIDSK exception trapped while initializing layer, operation likely impaired." );
        return OGRERR_FAILURE;
    }
}

/************************************************************************/
/*                           DeleteFeature()                            */
/************************************************************************/

OGRErr OGRPCIDSKLayer::DeleteFeature( long nFID )

{
    try {

        poVecSeg->DeleteShape( (PCIDSK::ShapeId) nFID );

        return OGRERR_NONE;
    }

/* -------------------------------------------------------------------- */
/*      Trap exceptions and report as CPL errors.                       */
/* -------------------------------------------------------------------- */
    catch( PCIDSK::PCIDSKException ex )
    {
        CPLError( CE_Failure, CPLE_AppDefined, 
                  "%s", ex.what() );
        return OGRERR_FAILURE;
    }
    catch(...)
    {
        CPLError( CE_Failure, CPLE_AppDefined, 
                  "Non-PCIDSK exception trapped." );
        return OGRERR_FAILURE;
    }
}

/************************************************************************/
/*                           CreateFeature()                            */
/************************************************************************/

OGRErr OGRPCIDSKLayer::CreateFeature( OGRFeature *poFeature )

{
    try {

        PCIDSK::ShapeId id = poVecSeg->CreateShape( 
            (PCIDSK::ShapeId) poFeature->GetFID() );

        poFeature->SetFID( (long) id );

        return SetFeature( poFeature );
    }
/* -------------------------------------------------------------------- */
/*      Trap exceptions and report as CPL errors.                       */
/* -------------------------------------------------------------------- */
    catch( PCIDSK::PCIDSKException ex )
    {
        CPLError( CE_Failure, CPLE_AppDefined, 
                  "%s", ex.what() );
        return OGRERR_FAILURE;
    }
    catch(...)
    {
        CPLError( CE_Failure, CPLE_AppDefined, 
                  "Non-PCIDSK exception trapped." );
        return OGRERR_FAILURE;
    }
    
}

/************************************************************************/
/*                             SetFeature()                             */
/************************************************************************/

OGRErr OGRPCIDSKLayer::SetFeature( OGRFeature *poFeature )

{
    PCIDSK::ShapeId id = (PCIDSK::ShapeId) poFeature->GetFID();
    
/* -------------------------------------------------------------------- */
/*      Translate attribute fields.                                     */
/* -------------------------------------------------------------------- */
    try {

        int iPCI;
        std::vector<PCIDSK::ShapeField>  aoPCIFields;

        aoPCIFields.resize(poVecSeg->GetFieldCount());

        for( iPCI = 0; iPCI < poVecSeg->GetFieldCount(); iPCI++ )
        {
            int iOGR;

            iOGR = poFeatureDefn->GetFieldIndex(
                poVecSeg->GetFieldName(iPCI).c_str() );

            if( iOGR == -1 )
                continue;

            switch( poVecSeg->GetFieldType(iPCI) )
            {
              case PCIDSK::FieldTypeInteger:
                aoPCIFields[iPCI].SetValue(
                    poFeature->GetFieldAsInteger( iOGR ) );
                break;

              case PCIDSK::FieldTypeFloat:
                aoPCIFields[iPCI].SetValue(
                    (float) poFeature->GetFieldAsDouble( iOGR ) );
                break;

              case PCIDSK::FieldTypeDouble:
                aoPCIFields[iPCI].SetValue(
                    (double) poFeature->GetFieldAsDouble( iOGR ) );
                break;

              case PCIDSK::FieldTypeString:
                aoPCIFields[iPCI].SetValue(
                    poFeature->GetFieldAsString( iOGR ) );
                break;

              case PCIDSK::FieldTypeCountedInt:
              {
                  int nCount;
                  const int *panList = 
                      poFeature->GetFieldAsIntegerList( iOGR, &nCount );
                  std::vector<PCIDSK::int32> anList;
                  
                  anList.resize( nCount );
                  memcpy( &(anList[0]), panList, 4 * anList.size() );
                  aoPCIFields[iPCI].SetValue( anList );
              }
              break;

              default:
                CPLAssert( FALSE );
                break;
            }
        }

        if( poVecSeg->GetFieldCount() > 0 )
            poVecSeg->SetFields( id, aoPCIFields );

/* -------------------------------------------------------------------- */
/*      Translate the geometry.                                         */
/* -------------------------------------------------------------------- */
        std::vector<PCIDSK::ShapeVertex> aoVertices;
        OGRGeometry *poGeometry = poFeature->GetGeometryRef();

        if( poGeometry == NULL )
        {
        }

        else if( wkbFlatten(poGeometry->getGeometryType()) == wkbPoint )
        {
            OGRPoint *poPoint = (OGRPoint *) poGeometry;

            aoVertices.resize(1);
            aoVertices[0].x = poPoint->getX();
            aoVertices[0].y = poPoint->getY();
            aoVertices[0].z = poPoint->getZ();
        }

        else if( wkbFlatten(poGeometry->getGeometryType()) == wkbLineString )
        {
            OGRLineString *poLS = (OGRLineString *) poGeometry;
            unsigned int i;

            aoVertices.resize(poLS->getNumPoints());

            for( i = 0; i < aoVertices.size(); i++ )
            {
                aoVertices[i].x = poLS->getX(i);
                aoVertices[i].y = poLS->getY(i);
                aoVertices[i].z = poLS->getZ(i);
            }
        }

        else
        {
            CPLDebug( "PCIDSK", "Unsupported geometry type in SetFeature(): %s",
                      poGeometry->getGeometryName() );
        }

        poVecSeg->SetVertices( id, aoVertices );

    } /* try */

/* -------------------------------------------------------------------- */
/*      Trap exceptions and report as CPL errors.                       */
/* -------------------------------------------------------------------- */
    catch( PCIDSK::PCIDSKException ex )
    {
        CPLError( CE_Failure, CPLE_AppDefined, 
                  "%s", ex.what() );
        return OGRERR_FAILURE;
    }
    catch(...)
    {
        CPLError( CE_Failure, CPLE_AppDefined, 
                  "Non-PCIDSK exception trapped." );
        return OGRERR_FAILURE;
    }

    return OGRERR_NONE;
}

/************************************************************************/
/*                            CreateField()                             */
/************************************************************************/

OGRErr OGRPCIDSKLayer::CreateField( OGRFieldDefn *poFieldDefn,
                                    int bApproxOK )

{
    try {
        
        if( poFieldDefn->GetType() == OFTInteger )
        {
            poVecSeg->AddField( poFieldDefn->GetNameRef(), 
                                PCIDSK::FieldTypeInteger, 
                                "", "" );
            poFeatureDefn->AddFieldDefn( poFieldDefn );
        }
        else if( poFieldDefn->GetType() == OFTReal )
        {
            poVecSeg->AddField( poFieldDefn->GetNameRef(), 
                                PCIDSK::FieldTypeDouble, 
                                "", "" );
            poFeatureDefn->AddFieldDefn( poFieldDefn );
        }
        else if( poFieldDefn->GetType() == OFTString )
        {
            poVecSeg->AddField( poFieldDefn->GetNameRef(), 
                                PCIDSK::FieldTypeString, 
                                "", "" );
            poFeatureDefn->AddFieldDefn( poFieldDefn );
        }
        else if( poFieldDefn->GetType() == OFTIntegerList )
        {
            poVecSeg->AddField( poFieldDefn->GetNameRef(), 
                                PCIDSK::FieldTypeCountedInt, 
                                "", "" );
            poFeatureDefn->AddFieldDefn( poFieldDefn );
        }
        else if( bApproxOK )
        {
            // Fallback to treating everything else as a string field.
            OGRFieldDefn oModFieldDefn(poFieldDefn);
            oModFieldDefn.SetType( OFTString );
            poVecSeg->AddField( poFieldDefn->GetNameRef(), 
                                PCIDSK::FieldTypeString, 
                                "", "" );
            poFeatureDefn->AddFieldDefn( &oModFieldDefn );
        }
        else
        {
            CPLError( CE_Failure, CPLE_AppDefined,
                      "Attempt to create field '%s' of unsupported data type.",
                      poFieldDefn->GetNameRef() );
        }
    }

/* -------------------------------------------------------------------- */
/*      Trap exceptions and report as CPL errors.                       */
/* -------------------------------------------------------------------- */
    catch( PCIDSK::PCIDSKException ex )
    {
        CPLError( CE_Failure, CPLE_AppDefined, 
                  "%s", ex.what() );
        return OGRERR_FAILURE;
    }
    catch(...)
    {
        CPLError( CE_Failure, CPLE_AppDefined, 
                  "Non-PCIDSK exception trapped." );
        return OGRERR_FAILURE;
    }

    return OGRERR_NONE;
}
