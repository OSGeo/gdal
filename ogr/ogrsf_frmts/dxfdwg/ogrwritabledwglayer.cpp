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
 * DEALINGS IN THE SOFTWARE.
 ******************************************************************************
 *
 * $Log$
 * Revision 1.2  2005/11/10 21:31:48  fwarmerdam
 * preliminary version
 *
 * Revision 1.1  2005/11/07 04:43:24  fwarmerdam
 * New
 *
 */

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
/*      Create block                                                    */
/* -------------------------------------------------------------------- */
  OdDbBlockTablePtr pTable = pDb->getBlockTableId().safeOpenObject(OdDb::kForWrite);
  OdDbBlockTableRecordPtr pEntry = OdDbBlockTableRecord::createObject();
  
  // Block must have a name before adding it to the table.
  pEntry->setName( pszLayerName );
  
  // Add the object to the table.
  hBlockId = pTable->add(pEntry);

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
/*                           CreateFeature()                            */
/************************************************************************/

OGRErr OGRWritableDWGLayer::CreateFeature( OGRFeature *poFeature )

{
    OGRGeometry *poGeom = poFeature->GetGeometryRef();
    OdDbBlockTableRecordPtr pBlock = hBlockId.safeOpenObject(OdDb::kForWrite);

    if( poGeom == NULL )
        return OGRERR_FAILURE;

    switch( wkbFlatten(poGeom->getGeometryType()) )
    {
      case wkbPoint:
        return OGRERR_FAILURE;

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

          pBlock->appendOdDbEntity(p2dPl);
          return OGRERR_NONE;
          break;
      }

      case wkbPolygon:
        return OGRERR_FAILURE;

      default:
        return OGRERR_FAILURE;
    }
    
}
