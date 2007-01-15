/******************************************************************************
 * $Id$
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  Implements OGRWritableDWGLayer class.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2005, Frank Warmerdam <warmerdam@pobox.com>
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
 ****************************************************************************/

#include "ogr_dwg.h"
#include "cpl_conv.h"
#include "cpl_string.h"

#include "OdaCommon.h"
#include "DbDatabase.h"

#include "DbLayerTable.h"
#include "DbTextStyleTable.h"
#include "DbLinetypeTable.h"
#include "DbBlockTable.h"
#include "DbViewportTable.h"
#include "DbViewportTableRecord.h"

#include "DbXrecord.h"
#include "DbGroup.h"
#include "DbMlineStyle.h"
#include "DbRasterImageDef.h"
#include "DbRasterVariables.h"
#include "DbSortentsTable.h"
#include "DbLayout.h"

#include "DbLine.h"
#include "DbBlockReference.h"
#include "DbAttribute.h"
#include "DbAttributeDefinition.h"
#include "DbPoint.h"
#include "Db2dPolyline.h"
#include "Db2dVertex.h"
#include "Db3dPolyline.h"
#include "Db3dPolylineVertex.h"
#include "DbText.h"
#include "DbMText.h"
#include "DbPolygonMesh.h"
#include "DbPolygonMeshVertex.h"
#include "DbPolyFaceMesh.h"
#include "DbPolyFaceMeshVertex.h"
#include "DbFaceRecord.h"
#include "DbMline.h"
#include "DbFcf.h"
#include "DbLeader.h"
#include "Db3dSolid.h"
#include "DbRasterImage.h"
#include "DbFace.h"
#include "DbPoint.h"
#include "DbRay.h"
#include "DbXline.h"
#include "DbSolid.h"
#include "DbSpline.h"
#include "DbTrace.h"
#include "DbPolyline.h"
#include "DbArc.h"
#include "DbCircle.h"
#include "DbArcAlignedText.h"
#include "DbEllipse.h"
#include "RText.h"
#include "DbViewport.h"
#include "DbAlignedDimension.h"
#include "DbRadialDimension.h"
#include "Db3PointAngularDimension.h"
#include "DbOrdinateDimension.h"
#include "DbRotatedDimension.h"
#include "DbDimAssoc.h"
#include "DbTable.h"

#include "Ge/GeCircArc2d.h"
#include "Ge/GeScale3d.h"
#include "Ge/GeExtents3d.h"
#include "XRefMan.h"

#include <math.h>

#include "DbSymUtl.h"
#include "DbHostAppServices.h"
#include "HatchPatternManager.h"
#include "DbHyperlink.h"
#include "DbBody.h"

#include "DbWipeout.h"

CPL_CVSID("$Id$");

/************************************************************************/
/*                        OGRWritableDWGLayer()                         */
/************************************************************************/

OGRWritableDWGLayer::OGRWritableDWGLayer( const char *pszLayerName,
                                          char **papszOptionsIn, 
                                          OGRWritableDWGDataSource *poDSIn )

{
    poDS = poDSIn;
    papszOptions = CSLDuplicate( papszOptionsIn );
    pDb = poDS->pDb;

/* -------------------------------------------------------------------- */
/*      Create the DWGdirect layer object.                              */
/* -------------------------------------------------------------------- */
    // Add a new layer to the drawing
    OdDbLayerTablePtr pLayers;
    OdDbLayerTableRecordPtr pLayer;

    pLayers = pDb->getLayerTableId().safeOpenObject(OdDb::kForWrite);
    pLayer = OdDbLayerTableRecord::createObject();
    
    // Name must be set before a table object is added to a table.
    pLayer->setName( pszLayerName );
    
    // Add the object to the table.
    hLayerId = pLayers->add(pLayer);

/* -------------------------------------------------------------------- */
/*      Check for a layer color.                                        */
/* -------------------------------------------------------------------- */
    const char *pszColor = CSLFetchNameValue(papszOptionsIn,"COLOR");

    if( pszColor != NULL )
    {
        char **papszTokens = 
            CSLTokenizeStringComplex( pszColor, ",", FALSE, FALSE );
        
        if( CSLCount( papszTokens ) != 3 )
        {
            CPLError( CE_Warning, CPLE_AppDefined, 
                      "COLOR=%s setting not parsable.  Should be 'red,green,blue'.", 
                      pszColor );
        }
        else
        {
            OdCmColor oColor;

            oColor.setRGB( atoi(papszTokens[0]), 
                           atoi(papszTokens[1]), 
                           atoi(papszTokens[2]) );
            pLayer->setColor( oColor );
        }
    }

/* -------------------------------------------------------------------- */
/*      Create the starting OGRFeatureDefn.                             */
/* -------------------------------------------------------------------- */
    poFeatureDefn = new OGRFeatureDefn( pszLayerName );
    poFeatureDefn->Reference();
}

/************************************************************************/
/*                     ~OGRWritableDWGDataSource()                      */
/************************************************************************/

OGRWritableDWGLayer::~OGRWritableDWGLayer()

{
    poFeatureDefn->Release();
}

/************************************************************************/
/*                            ResetReading()                            */
/************************************************************************/

void OGRWritableDWGLayer::ResetReading()

{
}

/************************************************************************/
/*                           GetNextFeature()                           */
/************************************************************************/

OGRFeature *OGRWritableDWGLayer::GetNextFeature()

{
    return NULL;
}

/************************************************************************/
/*                           TestCapablity()                            */
/************************************************************************/

int OGRWritableDWGLayer::TestCapability( const char *pszCap )

{
    if( EQUAL(pszCap,OLCSequentialWrite) )
        return TRUE;

    else if( EQUAL(pszCap,OLCCreateField) )
        return TRUE;

    else
        return FALSE;
}

/************************************************************************/
/*                            CreateField()                             */
/************************************************************************/

OGRErr OGRWritableDWGLayer::CreateField( OGRFieldDefn *poField,
                                         int bApproxOK )

{
    poFeatureDefn->AddFieldDefn( poField );

    return OGRERR_NONE;
}

/************************************************************************/
/*                            WriteEntity()                             */
/************************************************************************/

OGRErr OGRWritableDWGLayer::WriteEntity( OGRGeometry *poGeom,
                                         OdDbObjectPtr *ppObjectRet )

{
    switch( wkbFlatten(poGeom->getGeometryType()) )
    {
      case wkbPoint:
      {
          OGRPoint *poOGRPoint = (OGRPoint *) poGeom;
          OdDbPointPtr pPoint = OdDbPoint::createObject();
          
          pPoint->setPosition( 
              OdGePoint3d(poOGRPoint->getX(), poOGRPoint->getY(), 
                          poOGRPoint->getZ() ) );

          pPoint->setLayer( hLayerId, false );
          poDS->pMs->appendOdDbEntity( pPoint );
          
          if( ppObjectRet != NULL )
              *ppObjectRet = pPoint;
          return OGRERR_NONE;
      }

      case wkbLineString:
      {
          OGRLineString *poLine = (OGRLineString *) poGeom;

          // Add a 2d polyline with vertices.
          OdDb2dPolylinePtr p2dPl = OdDb2dPolyline::createObject();
 
          int i;

          for (i = 0; i < poLine->getNumPoints(); i++)
          {
              OdDb2dVertexPtr pV;
              OdGePoint3d pos;

              pos.x = poLine->getX(i);
              pos.y = poLine->getY(i);
              pos.z = poLine->getZ(i);

              pV = OdDb2dVertex::createObject();
              p2dPl->appendVertex(pV);

              pV->setPosition(pos);
          }
  
          p2dPl->setLayer( hLayerId, false );

          poDS->pMs->appendOdDbEntity( p2dPl );

          if( ppObjectRet != NULL )
              *ppObjectRet = p2dPl;

          return OGRERR_NONE;
      }

      case wkbPolygon:
      {
          OGRPolygon *poPoly = (OGRPolygon *) poGeom;
          int iRing;
          OGRErr eErr;

          for( iRing = -1; iRing < poPoly->getNumInteriorRings(); iRing++ )
          {
              OGRLinearRing *poRing;

              if( iRing == -1 )
                  poRing = poPoly->getExteriorRing();
              else
                  poRing = poPoly->getInteriorRing( iRing );

              if( iRing == -1 )
                  eErr = WriteEntity( poRing, ppObjectRet );
              else
                  eErr = WriteEntity( poRing, NULL );
              if( eErr != OGRERR_NONE )
                  return eErr;

          }

          return OGRERR_NONE;
      }

      case wkbGeometryCollection:
      case wkbMultiPolygon:
      case wkbMultiPoint:
      case wkbMultiLineString:
      {
          OGRGeometryCollection *poColl = (OGRGeometryCollection *) poGeom;
          int iSubGeom;
          OGRErr eErr;

          for( iSubGeom=0; iSubGeom < poColl->getNumGeometries(); iSubGeom++ )
          {
              OGRGeometry *poGeom = poColl->getGeometryRef( iSubGeom );
              
              if( iSubGeom == 0 )
                  eErr = WriteEntity( poGeom, ppObjectRet );
              else
                  eErr = WriteEntity( poGeom, NULL );
                  
              if( eErr != OGRERR_NONE )
                  return eErr;
          }
          return OGRERR_NONE;
      }

      default:
        return OGRERR_FAILURE;
    }
}

/************************************************************************/
/*                           CreateFeature()                            */
/************************************************************************/

OGRErr OGRWritableDWGLayer::CreateFeature( OGRFeature *poFeature )

{
    OGRGeometry *poGeom = poFeature->GetGeometryRef();
    OGRErr eErr;

    if( poGeom == NULL )
        return OGRERR_FAILURE;

/* -------------------------------------------------------------------- */
/*      Keep track of file extents.                                     */
/* -------------------------------------------------------------------- */
    poDS->ExtendExtent( poGeom );

/* -------------------------------------------------------------------- */
/*      Translate geometry.                                             */
/* -------------------------------------------------------------------- */
    OdDbObjectPtr pObject;

    eErr = WriteEntity( poGeom, &pObject );
    if( eErr != OGRERR_NONE )
        return eErr;

/* -------------------------------------------------------------------- */
/*      Append attributes.                                              */
/* -------------------------------------------------------------------- */
    OdResBufPtr xIter = OdResBuf::newRb( 1001 ); 
    xIter->setString( "ACAD" );

    OdResBufPtr temp = xIter;
        
    for( int iField = 0; iField < poFeature->GetFieldCount(); iField++ )
    {
        if( !poFeature->IsFieldSet( iField ) )
            continue;

        CPLString sNameValue;
        const char *pszValue = poFeature->GetFieldAsString( iField );
        
        while( *pszValue == ' ' )
            pszValue++;

        sNameValue.Printf( "%s=%s", 
                           poFeature->GetFieldDefnRef( iField )->GetNameRef(),
                           pszValue );

        OdResBufPtr newRB = OdResBuf::newRb( 1000 );
        newRB->setString( sNameValue.c_str() );

        temp->setNext( newRB );
        temp = temp->next();
    }
    
    if( pObject != (const void *) NULL )
        pObject->setXData( xIter );

    return OGRERR_NONE;
}
