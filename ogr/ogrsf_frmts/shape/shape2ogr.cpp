/******************************************************************************
 * $Id$
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  Implements translation of Shapefile shapes into OGR
 *           representation.
 * Author:   Frank Warmerdam, warmerda@home.com
 *
 ******************************************************************************
 * Copyright (c) 1999,  Les Technologies SoftMap Inc.
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
 * Revision 1.8  2000/01/26 22:05:45  warmerda
 * fixed bug with 2d/3d arcs
 *
 * Revision 1.7  1999/12/23 14:51:07  warmerda
 * Improved support in face of 3D geometries.
 *
 * Revision 1.6  1999/11/04 21:18:01  warmerda
 * fix bug with shapeid for new shapes
 *
 * Revision 1.5  1999/07/27 01:52:04  warmerda
 * added support for writing polygon/multipoint/arc files
 *
 * Revision 1.4  1999/07/27 00:52:40  warmerda
 * added write methods
 *
 * Revision 1.3  1999/07/26 13:59:25  warmerda
 * added feature writing api
 *
 * Revision 1.2  1999/07/12 15:39:18  warmerda
 * added setting of shape geometry
 *
 * Revision 1.1  1999/07/05 18:58:07  warmerda
 * New
 *
 * Revision 1.3  1999/06/11 19:22:13  warmerda
 * added OGRFeature and OGRFeatureDefn related translation
 *
 * Revision 1.2  1999/05/31 20:41:49  warmerda
 * added multipoint support
 *
 * Revision 1.1  1999/05/31 19:26:03  warmerda
 * New
 */

#include "ogrshape.h"
#include "cpl_conv.h"

/************************************************************************/
/*                          SHPReadOGRObject()                          */
/*                                                                      */
/*      Read an item in a shapefile, and translate to OGR geometry      */
/*      representation.                                                 */
/************************************************************************/

OGRGeometry *SHPReadOGRObject( SHPHandle hSHP, int iShape )

{
    SHPObject	*psShape;
    OGRGeometry *poOGR = NULL;

    psShape = SHPReadObject( hSHP, iShape );

    if( psShape == NULL )
        return NULL;

/* -------------------------------------------------------------------- */
/*      Point.                                                          */
/* -------------------------------------------------------------------- */
    else if( psShape->nSHPType == SHPT_POINT
             || psShape->nSHPType == SHPT_POINTM
             || psShape->nSHPType == SHPT_POINTZ )
    {
        poOGR = new OGRPoint( psShape->padfX[0], psShape->padfY[0] );
    }

/* -------------------------------------------------------------------- */
/*      Multipoint.                                                     */
/* -------------------------------------------------------------------- */
    else if( psShape->nSHPType == SHPT_MULTIPOINT
             || psShape->nSHPType == SHPT_MULTIPOINTM
             || psShape->nSHPType == SHPT_MULTIPOINTZ )
    {
        OGRMultiPoint *poOGRMPoint = new OGRMultiPoint();
        int		i;

        for( i = 0; i < psShape->nVertices; i++ )
        {
            OGRPoint	*poPoint;

            poPoint = new OGRPoint( psShape->padfX[i], psShape->padfY[i] );
            
            poOGRMPoint->addGeometry( poPoint );

            delete poPoint;
        }
        
        poOGR = poOGRMPoint;
    }

/* -------------------------------------------------------------------- */
/*      Arc (LineString)                                                */
/*                                                                      */
/*      I am ignoring parts though they can apply to arcs as well.      */
/* -------------------------------------------------------------------- */
    else if( psShape->nSHPType == SHPT_ARC
             || psShape->nSHPType == SHPT_ARCM
             || psShape->nSHPType == SHPT_ARCZ )
    {
        OGRLineString *poOGRLine = new OGRLineString();

        poOGRLine->setPoints( psShape->nVertices,
                              psShape->padfX, psShape->padfY );
        
        poOGR = poOGRLine;
    }

/* -------------------------------------------------------------------- */
/*      Polygon                                                         */
/*                                                                      */
/*      For now we assume the first ring is an outer ring, and          */
/*      everything else is an inner ring.  This must smarten up in      */
/*      the future.                                                     */
/* -------------------------------------------------------------------- */
    else if( psShape->nSHPType == SHPT_POLYGON
             || psShape->nSHPType == SHPT_POLYGONM
             || psShape->nSHPType == SHPT_POLYGONZ )
    {
        int	iRing;
        OGRPolygon *poOGRPoly;
        
        poOGR = poOGRPoly = new OGRPolygon();

        for( iRing = 0; iRing < psShape->nParts; iRing++ )
        {
            OGRLinearRing	*poRing;
            int	nRingPoints;
            int	nRingStart;

            poRing = new OGRLinearRing();

            if( psShape->panPartStart == NULL )
            {
                nRingPoints = psShape->nVertices;
                nRingStart = 0;
            }
            else
            {

                if( iRing == psShape->nParts - 1 )
                    nRingPoints =
                        psShape->nVertices - psShape->panPartStart[iRing];
                else
                    nRingPoints = psShape->panPartStart[iRing+1]
                        - psShape->panPartStart[iRing];
                nRingStart = psShape->panPartStart[iRing];
            }
            
            poRing->setPoints( nRingPoints, 
                               psShape->padfX + nRingStart,
                               psShape->padfY + nRingStart );

            poOGRPoly->addRing( poRing );

            delete poRing;
        }
    }

/* -------------------------------------------------------------------- */
/*      Otherwise for now we just ignore the object.  Eventually we     */
/*      should implement multipoints, and perhaps do something with     */
/*      multipatch.                                                     */
/* -------------------------------------------------------------------- */
    else
    {
        /* nothing returned */
    }
    
/* -------------------------------------------------------------------- */
/*      Cleanup shape, and set feature id.                              */
/* -------------------------------------------------------------------- */
    SHPDestroyObject( psShape );

    return poOGR;
}

/************************************************************************/
/*                         SHPWriteOGRObject()                          */
/************************************************************************/

OGRErr SHPWriteOGRObject( SHPHandle hSHP, int iShape, OGRGeometry *poGeom )

{
/* ==================================================================== */
/*      Write point geometry.                                           */
/* ==================================================================== */
    if( hSHP->nShapeType == SHPT_POINT
        || hSHP->nShapeType == SHPT_POINTM
        || hSHP->nShapeType == SHPT_POINTZ )
    {
        SHPObject	*psShape;
        OGRPoint	*poPoint = (OGRPoint *) poGeom;
        double		dfX, dfY, dfZ = 0;

        if( poGeom->getGeometryType() != wkbPoint
            && poGeom->getGeometryType() != wkbPoint25D )
        {
            CPLError( CE_Failure, CPLE_AppDefined,
                      "Attempt to write non-point (%s) geometry to"
                      " point shapefile.",
                      poGeom->getGeometryName() );

            return OGRERR_UNSUPPORTED_GEOMETRY_TYPE;
        }

        dfX = poPoint->getX();
        dfY = poPoint->getY();
        dfZ = poPoint->getZ();
        
        psShape = SHPCreateSimpleObject( hSHP->nShapeType, 1,
                                         &dfX, &dfY, &dfZ );
        SHPWriteObject( hSHP, iShape, psShape );
        SHPDestroyObject( psShape );
    }
/* ==================================================================== */
/*      MultiPoint.                                                     */
/* ==================================================================== */
    else if( hSHP->nShapeType == SHPT_MULTIPOINT
             || hSHP->nShapeType == SHPT_MULTIPOINTM
             || hSHP->nShapeType == SHPT_MULTIPOINTZ )
    {
        OGRMultiPoint	*poMP = (OGRMultiPoint *) poGeom;
        double		*padfX, *padfY, *padfZ;
        int		iPoint;
        SHPObject	*psShape;

        if( poGeom->getGeometryType() != wkbMultiPoint )
        {
            CPLError( CE_Failure, CPLE_AppDefined,
                      "Attempt to write non-multipoint (%s) geometry to "
                      "multipoint shapefile.",
                      poGeom->getGeometryName() );

            return OGRERR_UNSUPPORTED_GEOMETRY_TYPE;
        }

        padfX = (double *) CPLMalloc(sizeof(double)*poMP->getNumGeometries());
        padfY = (double *) CPLMalloc(sizeof(double)*poMP->getNumGeometries());
        padfZ = (double *) CPLCalloc(sizeof(double),poMP->getNumGeometries());

        for( iPoint = 0; iPoint < poMP->getNumGeometries(); iPoint++ )
        {
            OGRPoint	*poPoint = (OGRPoint *) poMP->getGeometryRef(iPoint);
            
            padfX[iPoint] = poPoint->getX();
            padfY[iPoint] = poPoint->getY();
        }

        psShape = SHPCreateSimpleObject( hSHP->nShapeType,
                                         poMP->getNumGeometries(),
                                         padfX, padfY, padfZ );
        SHPWriteObject( hSHP, iShape, psShape );
        SHPDestroyObject( psShape );
        
        CPLFree( padfX );
        CPLFree( padfY );
        CPLFree( padfZ );
    }

/* ==================================================================== */
/*      Arcs (we don't properly support multi part arcs).               */
/* ==================================================================== */
    else if( hSHP->nShapeType == SHPT_ARC
             || hSHP->nShapeType == SHPT_ARCM
             || hSHP->nShapeType == SHPT_ARCZ )
    {
        OGRLineString	*poArc = (OGRLineString *) poGeom;
        double		*padfX, *padfY, *padfZ;
        int		iPoint;
        SHPObject	*psShape;

        if( poGeom->getGeometryType() != wkbLineString
            && poGeom->getGeometryType() != wkbLineString25D )
        {
            CPLError( CE_Failure, CPLE_AppDefined,
                      "Attempt to write non-linestring (%s) geometry to "
                      "ARC type shapefile.",
                      poGeom->getGeometryName() );

            return OGRERR_UNSUPPORTED_GEOMETRY_TYPE;
        }

        padfX = (double *) CPLMalloc(sizeof(double)*poArc->getNumPoints());
        padfY = (double *) CPLMalloc(sizeof(double)*poArc->getNumPoints());
        padfZ = (double *) CPLCalloc(sizeof(double),poArc->getNumPoints());

        for( iPoint = 0; iPoint < poArc->getNumPoints(); iPoint++ )
        {
            padfX[iPoint] = poArc->getX( iPoint );
            padfY[iPoint] = poArc->getY( iPoint );
        }

        psShape = SHPCreateSimpleObject( hSHP->nShapeType,
                                         poArc->getNumPoints(),
                                         padfX, padfY, padfZ );
        SHPWriteObject( hSHP, iShape, psShape );
        SHPDestroyObject( psShape );
        
        CPLFree( padfX );
        CPLFree( padfY );
        CPLFree( padfZ );
    }
/* ==================================================================== */
/*      Polygons.                                                       */
/* ==================================================================== */
    else if( hSHP->nShapeType == SHPT_POLYGON
             || hSHP->nShapeType == SHPT_POLYGONM
             || hSHP->nShapeType == SHPT_POLYGONZ )
    {
        OGRPolygon	*poPoly = (OGRPolygon *) poGeom;
        double		*padfX=NULL, *padfY=NULL, *padfZ=NULL;
        int		iPoint, iRing, nRings, nVertex=0, *panRingStart;
        SHPObject	*psShape;

        if( poGeom->getGeometryType() != wkbPolygon )
        {
            CPLError( CE_Failure, CPLE_AppDefined,
                      "Attempt to write non-polygon (%s) geometry to "
                      " type shapefile.",
                      poGeom->getGeometryName() );

            return OGRERR_UNSUPPORTED_GEOMETRY_TYPE;
        }

        nRings = poPoly->getNumInteriorRings()+1;
        panRingStart = (int *) CPLMalloc(sizeof(int) * nRings);

        for( iRing = 0; iRing < nRings; iRing++ )
        {
            OGRLinearRing *poRing;

            if( iRing == 0 )
                poRing = poPoly->getExteriorRing();
            else
                poRing = poPoly->getInteriorRing( iRing-1 );

            panRingStart[iRing] = nVertex;

            padfX = (double *)
             CPLRealloc(padfX,sizeof(double)*(nVertex+poRing->getNumPoints()));
            padfY = (double *) 
             CPLRealloc(padfY,sizeof(double)*(nVertex+poRing->getNumPoints()));
            padfZ = (double *) 
             CPLRealloc(padfZ,sizeof(double)*(nVertex+poRing->getNumPoints()));
            
            for( iPoint = 0; iPoint < poRing->getNumPoints(); iPoint++ )
            {
                padfX[nVertex] = poRing->getX( iPoint );
                padfY[nVertex] = poRing->getY( iPoint );
                nVertex++;
            }
        }

        psShape = SHPCreateObject( hSHP->nShapeType, iShape, nRings,
                                   panRingStart, NULL,
                                   nVertex, padfX, padfY, padfZ, NULL );
        SHPWriteObject( hSHP, iShape, psShape );
        SHPDestroyObject( psShape );
        
        CPLFree( padfX );
        CPLFree( padfY );
        CPLFree( padfZ );
    }
    else
    {
        /* do nothing for multipatch */
        return OGRERR_UNSUPPORTED_GEOMETRY_TYPE;
    }

    return OGRERR_NONE;
}

/************************************************************************/
/*                       SHPReadOGRFeatureDefn()                        */
/************************************************************************/

OGRFeatureDefn *SHPReadOGRFeatureDefn( const char * pszName,
                                       SHPHandle hSHP, DBFHandle hDBF )

{
    OGRFeatureDefn	*poDefn = new OGRFeatureDefn( pszName );
    int			iField;

    for( iField = 0; iField < DBFGetFieldCount( hDBF ); iField++ )
    {
        char		szFieldName[20];
        int		nWidth, nPrecision;
        DBFFieldType	eDBFType;
        OGRFieldDefn	oField("", OFTInteger);

        eDBFType = DBFGetFieldInfo( hDBF, iField, szFieldName,
                                    &nWidth, &nPrecision );

        oField.SetName( szFieldName );
        oField.SetWidth( nWidth );
        oField.SetPrecision( nPrecision );

        if( eDBFType == FTString )
            oField.SetType( OFTString );
        else if( eDBFType == FTInteger )
            oField.SetType( OFTInteger );
        else
            oField.SetType( OFTReal );

        poDefn->AddFieldDefn( &oField );
    }

    switch( hSHP->nShapeType )
    {
      case SHPT_POINT:
      case SHPT_POINTZ:
      case SHPT_POINTM:
        poDefn->SetGeomType( wkbPoint );
        break;

      case SHPT_ARC:
      case SHPT_ARCZ:
      case SHPT_ARCM:
        poDefn->SetGeomType( wkbLineString );
        break;

      case SHPT_MULTIPOINT:
      case SHPT_MULTIPOINTZ:
      case SHPT_MULTIPOINTM:
        poDefn->SetGeomType( wkbMultiPoint );
        break;

      case SHPT_POLYGON:
      case SHPT_POLYGONZ:
      case SHPT_POLYGONM:
        poDefn->SetGeomType( wkbPolygon );
        break;
    }

    return poDefn;
}

/************************************************************************/
/*                         SHPReadOGRFeature()                          */
/************************************************************************/

OGRFeature *SHPReadOGRFeature( SHPHandle hSHP, DBFHandle hDBF,
                               OGRFeatureDefn * poDefn, int iShape )

{
    OGRFeature	*poFeature = new OGRFeature( poDefn );

    poFeature->SetGeometryDirectly( SHPReadOGRObject( hSHP, iShape ) );

    for( int iField = 0; iField < poDefn->GetFieldCount(); iField++ )
    {
        switch( poDefn->GetFieldDefn(iField)->GetType() )
        {
          case OFTString:
            poFeature->SetField( iField,
                                 DBFReadStringAttribute( hDBF, iShape,
                                                         iField ) );
            break;

          case OFTInteger:
            poFeature->SetField( iField,
                                 DBFReadIntegerAttribute( hDBF, iShape,
                                                          iField ) );
            break;

          case OFTReal:
            poFeature->SetField( iField,
                                 DBFReadDoubleAttribute( hDBF, iShape,
                                                         iField ) );
            break;

          default:
            CPLAssert( FALSE );
        }
    }

    if( poFeature != NULL )
        poFeature->SetFID( iShape );

    return( poFeature );
}

/************************************************************************/
/*                         SHPWriteOGRFeature()                         */
/*                                                                      */
/*      Write to an existing feature in a shapefile, or create a new    */
/*      feature.                                                        */
/************************************************************************/

OGRErr SHPWriteOGRFeature( SHPHandle hSHP, DBFHandle hDBF,
                           OGRFeatureDefn * poDefn, 
                           OGRFeature * poFeature )

{
/* -------------------------------------------------------------------- */
/*      Write the geometry.                                             */
/* -------------------------------------------------------------------- */
    OGRErr	eErr;

    eErr = SHPWriteOGRObject( hSHP, poFeature->GetFID(),
                              poFeature->GetGeometryRef() );
    if( eErr != OGRERR_NONE )
        return eErr;
    
/* -------------------------------------------------------------------- */
/*      If this is a new feature, establish it's feature id.            */
/* -------------------------------------------------------------------- */
    if( poFeature->GetFID() == OGRNullFID )
        poFeature->SetFID( DBFGetRecordCount( hDBF ) );

/* -------------------------------------------------------------------- */
/*      Write all the fields.                                           */
/* -------------------------------------------------------------------- */
    for( int iField = 0; iField < poDefn->GetFieldCount(); iField++ )
    {
        switch( poDefn->GetFieldDefn(iField)->GetType() )
        {
          case OFTString:
            DBFWriteStringAttribute( hDBF, poFeature->GetFID(), iField, 
                                     poFeature->GetFieldAsString( iField ));
            break;

          case OFTInteger:
            DBFWriteIntegerAttribute( hDBF, poFeature->GetFID(), iField, 
                                      poFeature->GetFieldAsInteger(iField) );
            break;

          case OFTReal:
            DBFWriteDoubleAttribute( hDBF, poFeature->GetFID(), iField, 
                                     poFeature->GetFieldAsDouble(iField) );
            break;

          default:
            CPLAssert( FALSE );
        }
    }

    return OGRERR_NONE;
}

