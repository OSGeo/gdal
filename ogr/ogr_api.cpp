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
 * \brief Fetch number of points from a geometry.
 *
 * Only wkbPoint[25D] or wkbLineString[25D] may return a valid value.
 * Other geometry types will silently return 0.
 *
 * @param hGeom handle to the geometry from which to get the number of points.
 * @return the number of points.
 */

int OGR_G_GetPointCount( OGRGeometryH hGeom )

{
    VALIDATE_POINTER1( hGeom, "OGR_G_GetPointCount", 0 );

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
        // autotest/pymod/ogrtest.py calls this method on any geometry. So keep silent
        //CPLError(CE_Failure, CPLE_NotSupported, "Incompatible geometry for operation");
        return 0;
    }
}

/************************************************************************/
/*                             OGR_G_GetX()                             */
/************************************************************************/
/**
 * \brief Fetch the x coordinate of a point from a geometry.
 *
 * @param hGeom handle to the geometry from which to get the x coordinate.
 * @param i point to get the x coordinate.
 * @return the X coordinate of this point. 
 */

double OGR_G_GetX( OGRGeometryH hGeom, int i )

{
    VALIDATE_POINTER1( hGeom, "OGR_G_GetX", 0 );

    switch( wkbFlatten(((OGRGeometry *) hGeom)->getGeometryType()) )
    {
      case wkbPoint:
      {
          if( i == 0 )
              return ((OGRPoint *) hGeom)->getX();
          else
          {
              CPLError(CE_Failure, CPLE_NotSupported, "Only i == 0 is supported");
              return 0.0;
          }
      }

      case wkbLineString:
        return ((OGRLineString *) hGeom)->getX( i );

      default:
        CPLError(CE_Failure, CPLE_NotSupported, "Incompatible geometry for operation");
        return 0.0;
    }
}

/************************************************************************/
/*                             OGR_G_GetY()                             */
/************************************************************************/
/**
 * \brief Fetch the x coordinate of a point from a geometry.
 *
 * @param hGeom handle to the geometry from which to get the y coordinate.
 * @param i point to get the Y coordinate.
 * @return the Y coordinate of this point. 
 */

double OGR_G_GetY( OGRGeometryH hGeom, int i )

{
    VALIDATE_POINTER1( hGeom, "OGR_G_GetY", 0 );

    switch( wkbFlatten(((OGRGeometry *) hGeom)->getGeometryType()) )
    {
      case wkbPoint:
      {
          if( i == 0 )
              return ((OGRPoint *) hGeom)->getY();
          else
          {
              CPLError(CE_Failure, CPLE_NotSupported, "Only i == 0 is supported");
              return 0.0;
          }
      }

      case wkbLineString:
          return ((OGRLineString *) hGeom)->getY( i );

      default:
        CPLError(CE_Failure, CPLE_NotSupported, "Incompatible geometry for operation");
        return 0.0;
    }
}

/************************************************************************/
/*                             OGR_G_GetZ()                             */
/************************************************************************/
/**
 * \brief Fetch the z coordinate of a point from a geometry.
 *
 * @param hGeom handle to the geometry from which to get the Z coordinate.
 * @param i point to get the Z coordinate.
 * @return the Z coordinate of this point. 
 */

double OGR_G_GetZ( OGRGeometryH hGeom, int i )

{
    VALIDATE_POINTER1( hGeom, "OGR_G_GetZ", 0 );

    switch( wkbFlatten(((OGRGeometry *) hGeom)->getGeometryType()) )
    {
      case wkbPoint:
      {
          if( i == 0 )
              return ((OGRPoint *) hGeom)->getZ();
          else
          {
              CPLError(CE_Failure, CPLE_NotSupported, "Only i == 0 is supported");
              return 0.0;
          }
      }

      case wkbLineString:
          return ((OGRLineString *) hGeom)->getZ( i );

      default:
          CPLError(CE_Failure, CPLE_NotSupported, "Incompatible geometry for operation");
          return 0.0;
    }
}

/************************************************************************/
/*                           OGR_G_GetPoint()                           */
/************************************************************************/

/**
 * \brief Fetch a point in line string or a point geometry.
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
    VALIDATE_POINTER0( hGeom, "OGR_G_GetPoint" );

    switch( wkbFlatten(((OGRGeometry *) hGeom)->getGeometryType()) )
    {
      case wkbPoint:
      {
          if( i == 0 )
          {
              *pdfX = ((OGRPoint *)hGeom)->getX();
              *pdfY = ((OGRPoint *)hGeom)->getY();
              if( pdfZ != NULL )
                  *pdfZ = ((OGRPoint *)hGeom)->getZ();
          }
          else
          {
              CPLError(CE_Failure, CPLE_NotSupported, "Only i == 0 is supported");
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
        CPLError(CE_Failure, CPLE_NotSupported, "Incompatible geometry for operation");
        break;
    }
}

/************************************************************************/
/*                           OGR_G_SetPoint()                           */
/************************************************************************/
/**
 * \brief Set the location of a vertex in a point or linestring geometry.
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
    VALIDATE_POINTER0( hGeom, "OGR_G_SetPoint" );

    switch( wkbFlatten(((OGRGeometry *) hGeom)->getGeometryType()) )
    {
      case wkbPoint:
      {
          if( i == 0 )
          {
              ((OGRPoint *) hGeom)->setX( dfX );
              ((OGRPoint *) hGeom)->setY( dfY );
              ((OGRPoint *) hGeom)->setZ( dfZ );
          }
          else
          {
              CPLError(CE_Failure, CPLE_NotSupported, "Only i == 0 is supported");
          }
      }
      break;

      case wkbLineString:
        ((OGRLineString *) hGeom)->setPoint( i, dfX, dfY, dfZ );
        break;

      default:
        CPLError(CE_Failure, CPLE_NotSupported, "Incompatible geometry for operation");
        break;
    }
}

/************************************************************************/
/*                         OGR_G_SetPoint_2D()                          */
/************************************************************************/
/**
 * \brief Set the location of a vertex in a point or linestring geometry.
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
    VALIDATE_POINTER0( hGeom, "OGR_G_SetPoint_2D" );

    switch( wkbFlatten(((OGRGeometry *) hGeom)->getGeometryType()) )
    {
      case wkbPoint:
      {
          if( i == 0 )
          {
              ((OGRPoint *) hGeom)->setX( dfX );
              ((OGRPoint *) hGeom)->setY( dfY );
          }
          else
          {
              CPLError(CE_Failure, CPLE_NotSupported, "Only i == 0 is supported");
          }
      }
      break;

      case wkbLineString:
        ((OGRLineString *) hGeom)->setPoint( i, dfX, dfY );
        break;

      default:
        CPLError(CE_Failure, CPLE_NotSupported, "Incompatible geometry for operation");
        break;
    }
}

/************************************************************************/
/*                           OGR_G_AddPoint()                           */
/************************************************************************/
/**
 * \brief Add a point to a geometry (line string or point).
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
    VALIDATE_POINTER0( hGeom, "OGR_G_AddPoint" );

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
        CPLError(CE_Failure, CPLE_NotSupported, "Incompatible geometry for operation");
        break;
    }
}

/************************************************************************/
/*                           OGR_G_AddPoint()                           */
/************************************************************************/
/**
 * \brief Add a point to a geometry (line string or point).
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
    VALIDATE_POINTER0( hGeom, "OGR_G_AddPoint_2D" );

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
        CPLError(CE_Failure, CPLE_NotSupported, "Incompatible geometry for operation");
        break;
    }
}

/************************************************************************/
/*                       OGR_G_GetGeometryCount()                       */
/************************************************************************/
/**
 * \brief Fetch the number of elements in a geometry or number of geometries in container.
 *
 * Only geometries of type wkbPolygon[25D], wkbMultiPoint[25D], wkbMultiLineString[25D],
 * wkbMultiPolygon[25D] or wkbGeometryCollection[25D] may return a valid value.
 * Other geometry types will silently return 0.
 *
 * For a polygon, the returned number is the number of rings (exterior ring + interior rings).
 *
 * @param hGeom single geometry or geometry container from which to get
 * the number of elements.
 * @return the number of elements.
 */

int OGR_G_GetGeometryCount( OGRGeometryH hGeom )

{
    VALIDATE_POINTER1( hGeom, "OGR_G_GetGeometryCount", 0 );

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
        // autotest/pymod/ogrtest.py calls this method on any geometry. So keep silent
        //CPLError(CE_Failure, CPLE_NotSupported, "Incompatible geometry for operation");
        return 0;
    }
}

/************************************************************************/
/*                        OGR_G_GetGeometryRef()                        */
/************************************************************************/

/**
 * \brief Fetch geometry from a geometry container.
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
 * For a polygon, OGR_G_GetGeometryRef(iSubGeom) returns the exterior ring
 * if iSubGeom == 0, and the interior rings for iSubGeom > 0.
 *
 * @param hGeom handle to the geometry container from which to get a 
 * geometry from.
 * @param iSubGeom the index of the geometry to fetch, between 0 and
 *          getNumGeometries() - 1.
 * @return handle to the requested geometry.
 */

OGRGeometryH OGR_G_GetGeometryRef( OGRGeometryH hGeom, int iSubGeom )

{
    VALIDATE_POINTER1( hGeom, "OGR_G_GetGeometryRef", NULL );

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
        CPLError(CE_Failure, CPLE_NotSupported, "Incompatible geometry for operation");
        return 0;
    }
}

/************************************************************************/
/*                         OGR_G_AddGeometry()                          */
/************************************************************************/

/**
 * \brief Add a geometry to a geometry container.
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
 * For a polygon, hNewSubGeom must be a linearring. If the polygon is empty,
 * the first added subgeometry will be the exterior ring. The next ones will be
 * the interior rings.
 *
 * @param hGeom existing geometry container.
 * @param hNewSubGeom geometry to add to the container.
 *
 * @return OGRERR_NONE if successful, or OGRERR_UNSUPPORTED_GEOMETRY_TYPE if
 * the geometry type is illegal for the type of existing geometry.
 */

OGRErr OGR_G_AddGeometry( OGRGeometryH hGeom, OGRGeometryH hNewSubGeom )

{
    VALIDATE_POINTER1( hGeom, "OGR_G_AddGeometry", OGRERR_UNSUPPORTED_OPERATION );
    VALIDATE_POINTER1( hNewSubGeom, "OGR_G_AddGeometry", OGRERR_UNSUPPORTED_OPERATION );

    switch( wkbFlatten(((OGRGeometry *) hGeom)->getGeometryType()) )
    {
      case wkbPolygon:
      {
          if( !EQUAL( ((OGRGeometry*) hNewSubGeom)->getGeometryName(), "LINEARRING" ) )
          {
              return OGRERR_UNSUPPORTED_GEOMETRY_TYPE;
          }
          else
          {
              ((OGRPolygon *)hGeom)->addRing( (OGRLinearRing *) hNewSubGeom );
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
 * \brief Add a geometry directly to an existing geometry container.
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
 * For a polygon, hNewSubGeom must be a linearring. If the polygon is empty,
 * the first added subgeometry will be the exterior ring. The next ones will be
 * the interior rings.
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
    VALIDATE_POINTER1( hGeom, "OGR_G_AddGeometryDirectly", OGRERR_UNSUPPORTED_OPERATION );
    VALIDATE_POINTER1( hNewSubGeom, "OGR_G_AddGeometryDirectly", OGRERR_UNSUPPORTED_OPERATION );

    switch( wkbFlatten(((OGRGeometry *) hGeom)->getGeometryType()) )
    {
      case wkbPolygon:
      {
          if( !EQUAL( ((OGRGeometry*) hNewSubGeom)->getGeometryName(), "LINEARRING" ) )
          {
              return OGRERR_UNSUPPORTED_GEOMETRY_TYPE;
          }
          else
          {
              ((OGRPolygon *)hGeom)->addRingDirectly( (OGRLinearRing *) hNewSubGeom );
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
 * \brief Remove a geometry from an exiting geometry container.
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
    VALIDATE_POINTER1( hGeom, "OGR_G_GetArea", 0 );

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
 * \brief Compute geometry area.
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
    VALIDATE_POINTER1( hGeom, "OGR_G_GetArea", 0 );

    double fArea = 0.0;

    switch( wkbFlatten(((OGRGeometry *) hGeom)->getGeometryType()) )
    {
      case wkbPolygon:
        fArea = ((OGRPolygon *) hGeom)->get_Area();
        break;

      case wkbMultiPolygon:
        fArea = ((OGRMultiPolygon *) hGeom)->get_Area();
        break;

      case wkbLinearRing:
      case wkbLineString:
        /* This test below is required to filter out wkbLineString geometries
         * not being of type of wkbLinearRing.
         */
        if( EQUAL( ((OGRGeometry*) hGeom)->getGeometryName(), "LINEARRING" ) )
        {
            fArea = ((OGRLinearRing *) hGeom)->get_Area();
        }
        break;

      case wkbGeometryCollection:
        fArea = ((OGRGeometryCollection *) hGeom)->get_Area();
        break;

      default:
        CPLError( CE_Warning, CPLE_AppDefined,
                  "OGR_G_GetArea() called against non-surface geometry type." );

        fArea = 0.0;
    }

    return fArea;
}

