/******************************************************************************
 * $Id$
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  C API Functions that don't correspond one-to-one with C++ 
 *           methods, such as the "simplified" geometry access functions.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2002, Frank Warmerdam
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
 * Revision 1.2  2002/10/19 16:22:32  warmerda
 * fixed bug with OGR_G_GetGeometryRef() for interior rings of polygons
 *
 * Revision 1.1  2002/09/26 18:11:51  warmerda
 * New
 *
 */

#include "ogr_geometry.h"
#include "ogr_api.h"
#include "cpl_error.h"

/************************************************************************/
/*                        OGR_G_GetPointCount()                         */
/************************************************************************/

int OGR_G_GetPointCount( OGRGeometryH hGeom )

{
    switch( wkbFlatten(((OGRGeometry *) hGeom)->getGeometryType()) )
    {
      case wkbPoint:
        return 1;

      case wkbLineString:
      {
          OGRLineString *poLine = (OGRLineString *) hGeom;
          return poLine->getNumPoints();
      }

      default:
        return 0;
    }
}

/************************************************************************/
/*                             OGR_G_GetX()                             */
/************************************************************************/

double OGR_G_GetX( OGRGeometryH hGeom, int i )

{
    switch( wkbFlatten(((OGRGeometry *) hGeom)->getGeometryType()) )
    {
      case wkbPoint:
      {
          if( i == 0 )
              return ((OGRPoint *) hGeom)->getX();
          else
              return 0.0;
      }

      case wkbLineString:
        return ((OGRLineString *) hGeom)->getX( i );

      default:
        return 0.0;
    }
}

/************************************************************************/
/*                             OGR_G_GetY()                             */
/************************************************************************/

double OGR_G_GetY( OGRGeometryH hGeom, int i )

{
    switch( wkbFlatten(((OGRGeometry *) hGeom)->getGeometryType()) )
    {
      case wkbPoint:
      {
          if( i == 0 )
              return ((OGRPoint *) hGeom)->getY();
          else
              return 0.0;
      }

      case wkbLineString:
          return ((OGRLineString *) hGeom)->getY( i );

      default:
        return 0.0;
    }
}

/************************************************************************/
/*                             OGR_G_GetZ()                             */
/************************************************************************/

double OGR_G_GetZ( OGRGeometryH hGeom, int i )

{
    switch( wkbFlatten(((OGRGeometry *) hGeom)->getGeometryType()) )
    {
      case wkbPoint:
      {
          if( i == 0 )
              return ((OGRPoint *) hGeom)->getZ();
          else
              return 0.0;
      }

      case wkbLineString:
          return ((OGRLineString *) hGeom)->getZ( i );

      default:
          return 0.0;
    }
}

/************************************************************************/
/*                           OGR_G_GetPoint()                           */
/************************************************************************/

void OGR_G_GetPoint( OGRGeometryH hGeom, int i, 
                     double *pdfX, double *pdfY, double *pdfZ )

{
    switch( wkbFlatten(((OGRGeometry *) hGeom)->getGeometryType()) )
    {
      case wkbPoint:
      {
          CPLAssert( i == 0 );
          if( i == 0 )
          {
              *pdfX = ((OGRPoint *)hGeom)->getX();
              *pdfY = ((OGRPoint *)hGeom)->getX();
              if( pdfZ != NULL )
                  *pdfZ = ((OGRPoint *)hGeom)->getZ();
          }
      }
      break;

      case wkbLineString:
      {
          *pdfX = ((OGRLineString *) hGeom)->getX( i );
          *pdfY = ((OGRLineString *) hGeom)->getY( i );
          if( pdfZ != NULL )
              *pdfZ = ((OGRLineString *) hGeom)->getZ( i );
      }
      break;

      default:
        CPLAssert( FALSE );
        break;
    }
}

/************************************************************************/
/*                           OGR_G_SetPoint()                           */
/************************************************************************/

void OGR_G_SetPoint( OGRGeometryH hGeom, int i, 
                     double dfX, double dfY, double dfZ )

{
    switch( wkbFlatten(((OGRGeometry *) hGeom)->getGeometryType()) )
    {
      case wkbPoint:
      {
          CPLAssert( i == 0 );
          if( i == 0 )
          {
              ((OGRPoint *) hGeom)->setX( dfX );
              ((OGRPoint *) hGeom)->setY( dfY );
              ((OGRPoint *) hGeom)->setZ( dfZ );
          }
      }
      break;

      case wkbLineString:
        ((OGRLineString *) hGeom)->setPoint( i, dfX, dfY, dfZ );
        break;

      default:
        CPLAssert( FALSE );
        break;
    }
}

/************************************************************************/
/*                           OGR_G_SetPoint()                           */
/************************************************************************/

void OGR_G_AddPoint( OGRGeometryH hGeom, 
                     double dfX, double dfY, double dfZ )

{
    switch( wkbFlatten(((OGRGeometry *) hGeom)->getGeometryType()) )
    {
      case wkbPoint:
      {
          ((OGRPoint *) hGeom)->setX( dfX );
          ((OGRPoint *) hGeom)->setY( dfY );
          ((OGRPoint *) hGeom)->setZ( dfZ );
      }
      break;

      case wkbLineString:
        ((OGRLineString *) hGeom)->addPoint( dfX, dfY, dfZ );
        break;

      default:
        CPLAssert( FALSE );
        break;
    }
}

/************************************************************************/
/*                       OGR_G_GetGeometryCount()                       */
/************************************************************************/

int OGR_G_GetGeometryCount( OGRGeometryH hGeom )

{
    switch( wkbFlatten(((OGRGeometry *) hGeom)->getGeometryType()) )
    {
      case wkbPolygon:
        if( ((OGRPolygon *)hGeom)->getExteriorRing() == NULL )
            return 0;
        else
            return ((OGRPolygon *)hGeom)->getNumInteriorRings() + 1;

      case wkbMultiPoint:
      case wkbMultiLineString:
      case wkbMultiPolygon:
      case wkbGeometryCollection:
        return ((OGRGeometryCollection *)hGeom)->getNumGeometries();

      default:
        return 0;
    }
}

/************************************************************************/
/*                        OGR_G_GetGeometryRef()                        */
/************************************************************************/

OGRGeometryH OGR_G_GetGeometryRef( OGRGeometryH hGeom, int iSubGeom )

{
    switch( wkbFlatten(((OGRGeometry *) hGeom)->getGeometryType()) )
    {
      case wkbPolygon:
        if( iSubGeom == 0 )
            return (OGRGeometryH) 
                ((OGRPolygon *)hGeom)->getExteriorRing();
        else
            return (OGRGeometryH) 
                ((OGRPolygon *)hGeom)->getInteriorRing(iSubGeom-1);

      case wkbMultiPoint:
      case wkbMultiLineString:
      case wkbMultiPolygon:
      case wkbGeometryCollection:
        return (OGRGeometryH) 
            ((OGRGeometryCollection *)hGeom)->getGeometryRef( iSubGeom );

      default:
        return 0;
    }
}

/************************************************************************/
/*                         OGR_G_AddGeometry()                          */
/************************************************************************/

OGRErr OGR_G_AddGeometry( OGRGeometryH hGeom, OGRGeometryH hNewSubGeom )

{
    switch( wkbFlatten(((OGRGeometry *) hGeom)->getGeometryType()) )
    {
      case wkbPolygon:
      {
          OGRLinearRing *poRing = (OGRLinearRing *) hNewSubGeom;

          if( poRing->WkbSize() != 0 
              || poRing->getGeometryType() != wkbLineString )
              return OGRERR_UNSUPPORTED_GEOMETRY_TYPE;
          else
          {
              ((OGRPolygon *)hGeom)->addRing( poRing );
              return OGRERR_NONE;
          }
      }

      case wkbMultiPoint:
      case wkbMultiLineString:
      case wkbMultiPolygon:
      case wkbGeometryCollection:
        return ((OGRGeometryCollection *)hGeom)->addGeometry( 
            (OGRGeometry *) hNewSubGeom );

      default:
        return OGRERR_UNSUPPORTED_OPERATION;
    }
}

/************************************************************************/
/*                     OGR_G_AddGeometryDirectly()                      */
/************************************************************************/

OGRErr OGR_G_AddGeometryDirectly( OGRGeometryH hGeom, 
                                  OGRGeometryH hNewSubGeom )

{
    switch( wkbFlatten(((OGRGeometry *) hGeom)->getGeometryType()) )
    {
      case wkbPolygon:
      {
          OGRLinearRing *poRing = (OGRLinearRing *) hNewSubGeom;

          if( poRing->WkbSize() != 0 
              || poRing->getGeometryType() != wkbLineString )
              return OGRERR_UNSUPPORTED_GEOMETRY_TYPE;
          else
          {
              ((OGRPolygon *)hGeom)->addRingDirectly( poRing );
              return OGRERR_NONE;
          }
      }

      case wkbMultiPoint:
      case wkbMultiLineString:
      case wkbMultiPolygon:
      case wkbGeometryCollection:
        return ((OGRGeometryCollection *)hGeom)->addGeometryDirectly( 
            (OGRGeometry *) hNewSubGeom );

      default:
        return OGRERR_UNSUPPORTED_OPERATION;
    }
}
