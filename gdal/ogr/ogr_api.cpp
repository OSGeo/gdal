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
 ****************************************************************************/

#include "ogr_geometry.h"
#include "ogr_api.h"
#include "cpl_error.h"

/************************************************************************/
/*                        OGR_G_GetPointCount()                         */
/************************************************************************/
/**
 * Fetch number of points from a geometry.
 *
 * @param hGeom handle to the geometry from which to get the number of points.
 * @return the number of points.
 */

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
/**
 * Fetch the x coordinate of a point from a geometry.
 *
 * @param hGeom handle to the geometry from which to get the x coordinate.
 * @param i point to get the x coordinate.
 * @return the X coordinate of this point. 
 */

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
/**
 * Fetch the x coordinate of a point from a geometry.
 *
 * @param hGeom handle to the geometry from which to get the y coordinate.
 * @param i point to get the Y coordinate.
 * @return the Y coordinate of this point. 
 */

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
/**
 * Fetch the z coordinate of a point from a geometry.
 *
 * @param hGeom handle to the geometry from which to get the Z coordinate.
 * @param i point to get the Z coordinate.
 * @return the Z coordinate of this point. 
 */

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

/**
 * Fetch a point in line string or a point geometry.
 *
 * @param hGeom handle to the geometry from which to get the coordinates.
 * @param i the vertex to fetch, from 0 to getNumPoints()-1, zero for a point.
 * @param pdfX value of x coordinate.
 * @param pdfY value of y coordinate.
 * @param pdfZ value of z coordinate.
 */

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
              *pdfY = ((OGRPoint *)hGeom)->getY();
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
/**
 * Set the location of a vertex in a point or linestring geometry.
 *
 * If iPoint is larger than the number of existing
 * points in the linestring, the point count will be increased to
 * accomodate the request.
 *
 * @param hGeom handle to the geometry to add a vertex to.
 * @param i the index of the vertex to assign (zero based) or
 *  zero for a point.
 * @param dfX input X coordinate to assign.
 * @param dfY input Y coordinate to assign.
 * @param dfZ input Z coordinate to assign (defaults to zero).
 */

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
/*                         OGR_G_SetPoint_2D()                          */
/************************************************************************/
/**
 * Set the location of a vertex in a point or linestring geometry.
 *
 * If iPoint is larger than the number of existing
 * points in the linestring, the point count will be increased to
 * accomodate the request.
 *
 * @param hGeom handle to the geometry to add a vertex to.
 * @param i the index of the vertex to assign (zero based) or
 *  zero for a point.
 * @param dfX input X coordinate to assign.
 * @param dfY input Y coordinate to assign.
 */

void OGR_G_SetPoint_2D( OGRGeometryH hGeom, int i, 
                        double dfX, double dfY )
    
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
          }
      }
      break;

      case wkbLineString:
        ((OGRLineString *) hGeom)->setPoint( i, dfX, dfY );
        break;

      default:
        CPLAssert( FALSE );
        break;
    }
}

/************************************************************************/
/*                           OGR_G_AddPoint()                           */
/************************************************************************/
/**
 * Add a point to a geometry (line string or point).
 *
 * The vertex count of the line string is increased by one, and assigned from
 * the passed location value.
 *
 * @param hGeom handle to the geometry to add a point to.
 * @param dfX x coordinate of point to add.
 * @param dfY y coordinate of point to add.
 * @param dfZ z coordinate of point to add.
 */

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
/*                           OGR_G_AddPoint()                           */
/************************************************************************/
/**
 * Add a point to a geometry (line string or point).
 *
 * The vertex count of the line string is increased by one, and assigned from
 * the passed location value.
 *
 * @param hGeom handle to the geometry to add a point to.
 * @param dfX x coordinate of point to add.
 * @param dfY y coordinate of point to add.
 */

void OGR_G_AddPoint_2D( OGRGeometryH hGeom, 
                        double dfX, double dfY )

{
    switch( wkbFlatten(((OGRGeometry *) hGeom)->getGeometryType()) )
    {
      case wkbPoint:
      {
          ((OGRPoint *) hGeom)->setX( dfX );
          ((OGRPoint *) hGeom)->setY( dfY );
      }
      break;

      case wkbLineString:
        ((OGRLineString *) hGeom)->addPoint( dfX, dfY );
        break;

      default:
        CPLAssert( FALSE );
        break;
    }
}

/************************************************************************/
/*                       OGR_G_GetGeometryCount()                       */
/************************************************************************/
/**
 * Fetch the number of elements in a geometry.
 *
 * @param hGeom geometry from which to get the number of elements.
 * @return the number of elements.
 */

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

/**
 * Fetch geometry from a geometry container.
 *
 * This function returns an handle to a geometry within the container.
 * The returned geometry remains owned by the container, and should not be
 * modified.  The handle is only valid untill the next change to the
 * geometry container.  Use OGR_G_Clone() to make a copy.
 *
 * This function relates to the SFCOM 
 * IGeometryCollection::get_Geometry() method.
 *
 * This function is the same as the CPP method 
 * OGRGeometryCollection::getGeometryRef().
 *
 * @param hGeom handle to the geometry container from which to get a 
 * geometry from.
 * @param iSubGeom the index of the geometry to fetch, between 0 and
 *          getNumGeometries() - 1.
 * @return handle to the requested geometry.
 */

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

/**
 * Add a geometry to a geometry container.
 *
 * Some subclasses of OGRGeometryCollection restrict the types of geometry
 * that can be added, and may return an error.  The passed geometry is cloned
 * to make an internal copy.
 *
 * There is no SFCOM analog to this method.
 *
 * This function is the same as the CPP method 
 * OGRGeometryCollection::addGeometry.
 *
 * @param hGeom existing geometry container.
 * @param hNewSubGeom geometry to add to the container.
 *
 * @return OGRERR_NONE if successful, or OGRERR_UNSUPPORTED_GEOMETRY_TYPE if
 * the geometry type is illegal for the type of existing geometry.
 */

OGRErr OGR_G_AddGeometry( OGRGeometryH hGeom, OGRGeometryH hNewSubGeom )

{
    switch( wkbFlatten(((OGRGeometry *) hGeom)->getGeometryType()) )
    {
      case wkbPolygon:
      {
          OGRLinearRing *poRing = (OGRLinearRing *) hNewSubGeom;

          if( poRing->WkbSize() != 0 
              || wkbFlatten(poRing->getGeometryType()) != wkbLineString )
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
/**
 * Add a geometry directly to an existing geometry container.
 *
 * Some subclasses of OGRGeometryCollection restrict the types of geometry
 * that can be added, and may return an error.  Ownership of the passed
 * geometry is taken by the container rather than cloning as addGeometry()
 * does.
 *
 * This function is the same as the CPP method 
 * OGRGeometryCollection::addGeometryDirectly.
 *
 * There is no SFCOM analog to this method.
 *
 * @param hGeom existing geometry.
 * @param hNewSubGeom geometry to add to the existing geometry.
 *
 * @return OGRERR_NONE if successful, or OGRERR_UNSUPPORTED_GEOMETRY_TYPE if
 * the geometry type is illegal for the type of geometry container.
 */

OGRErr OGR_G_AddGeometryDirectly( OGRGeometryH hGeom, 
                                  OGRGeometryH hNewSubGeom )

{
    switch( wkbFlatten(((OGRGeometry *) hGeom)->getGeometryType()) )
    {
      case wkbPolygon:
      {
          OGRLinearRing *poRing = (OGRLinearRing *) hNewSubGeom;

          if( poRing->WkbSize() != 0 
              || wkbFlatten(poRing->getGeometryType()) != wkbLineString )
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

/************************************************************************/
/*                        OGR_G_RemoveGeometry()                        */
/************************************************************************/

/**
 * Remove a geometry from an exiting geometry container.
 *
 * Removing a geometry will cause the geometry count to drop by one, and all
 * "higher" geometries will shuffle down one in index.
 *
 * There is no SFCOM analog to this method.
 *
 * This function is the same as the CPP method 
 * OGRGeometryCollection::removeGeometry().
 *
 * @param hGeom the existing geometry to delete from.
 * @param iGeom the index of the geometry to delete.  A value of -1 is a
 * special flag meaning that all geometries should be removed.
 *
 * @param bDelete if TRUE the geometry will be destroyed, otherwise it will
 * not.  The default is TRUE as the existing geometry is considered to own the
 * geometries in it. 
 *
 * @return OGRERR_NONE if successful, or OGRERR_FAILURE if the index is
 * out of range.
 */



OGRErr OGR_G_RemoveGeometry( OGRGeometryH hGeom, int iGeom, int bDelete )

{
    switch( wkbFlatten(((OGRGeometry *) hGeom)->getGeometryType()) )
    {
      case wkbPolygon:
      {
          CPLError( CE_Failure, CPLE_AppDefined, 
                    "OGR_G_RemoveGeometry() not supported on polygons yet." );
          return OGRERR_UNSUPPORTED_OPERATION;
      }

      case wkbMultiPoint:
      case wkbMultiLineString:
      case wkbMultiPolygon:
      case wkbGeometryCollection:
        return ((OGRGeometryCollection *)hGeom)->removeGeometry( iGeom,bDelete);

      default:
        return OGRERR_UNSUPPORTED_OPERATION;
    }
}

/************************************************************************/
/*                           OGR_G_GetArea()                            */
/************************************************************************/

/**
 * Compute geometry area.
 *
 * Computes the area for an OGRLinearRing, OGRPolygon or OGRMultiPolygon.
 * Undefined for all other geometry types (returns zero). 
 *
 * This function utilizes the C++ get_Area() methods such as
 * OGRPolygon::get_Area(). 
 *
 * @param hGeom the geometry to operate on. 
 * @return the area or 0.0 for unsupported geometry types.
 */

double OGR_G_GetArea( OGRGeometryH hGeom )

{
    switch( wkbFlatten(((OGRGeometry *) hGeom)->getGeometryType()) )
    {
      case wkbPolygon:
        return ((OGRPolygon *) hGeom)->get_Area();
        break;

      case wkbMultiPolygon:
        return ((OGRMultiPolygon *) hGeom)->get_Area();
        break;

      case wkbLinearRing:
        return ((OGRLinearRing *) hGeom)->get_Area();
        break;
        
      default:
        return 0.0;
    }
}

