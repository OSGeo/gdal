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
/*                        OGR_G_SetPointCount()                         */
/************************************************************************/
/**
 * \brief Set number of points in a geometry.
 *
 * This method primary exists to preset the number of points in a linestring
 * geometry before setPoint() is used to assign them to avoid reallocating
 * the array larger with each call to addPoint(). 
 *
 * @param nNewPointCount the new number of points for geometry.
 */

void OGR_G_SetPointCount( OGRGeometryH hGeom, int nNewPointCount )

{
    VALIDATE_POINTER0( hGeom, "OGR_G_SetPointCount" );

    switch( wkbFlatten(((OGRGeometry *) hGeom)->getGeometryType()) )
    {
      case wkbLineString:
      {
        OGRLineString *poLine = (OGRLineString *) hGeom;
        poLine->setNumPoints( nNewPointCount );
        break;
      }
      default:
        CPLError(CE_Failure, CPLE_NotSupported, "Incompatible geometry for operation");
        break;
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
      {
          OGRLineString* poLS = (OGRLineString *) hGeom;
          if (i < 0 || i >= poLS->getNumPoints())
          {
              CPLError(CE_Failure, CPLE_NotSupported, "Index out of bounds");
              return 0.0;
          }
          return poLS->getX( i );
      }

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
      {
          OGRLineString* poLS = (OGRLineString *) hGeom;
          if (i < 0 || i >= poLS->getNumPoints())
          {
              CPLError(CE_Failure, CPLE_NotSupported, "Index out of bounds");
              return 0.0;
          }
          return poLS->getY( i );
      }

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
      {
          OGRLineString* poLS = (OGRLineString *) hGeom;
          if (i < 0 || i >= poLS->getNumPoints())
          {
              CPLError(CE_Failure, CPLE_NotSupported, "Index out of bounds");
              return 0.0;
          }
          return poLS->getZ( i );
      }

      default:
          CPLError(CE_Failure, CPLE_NotSupported, "Incompatible geometry for operation");
          return 0.0;
    }
}

/************************************************************************/
/*                          OGR_G_GetPoints()                           */
/************************************************************************/

/**
 * \brief Returns all points of line string.
 *
 * This method copies all points into user arrays. The user provides the
 * stride between 2 consecutives elements of the array.
 *
 * On some CPU architectures, care must be taken so that the arrays are properly aligned.
 *
 * @param hGeom handle to the geometry from which to get the coordinates.
 * @param pabyX a buffer of at least (sizeof(double) * nXStride * nPointCount) bytes, may be NULL.
 * @param nXStride the number of bytes between 2 elements of pabyX.
 * @param pabyY a buffer of at least (sizeof(double) * nYStride * nPointCount) bytes, may be NULL.
 * @param nYStride the number of bytes between 2 elements of pabyY.
 * @param pabyZ a buffer of at last size (sizeof(double) * nZStride * nPointCount) bytes, may be NULL.
 * @param nZStride the number of bytes between 2 elements of pabyZ.
 *
 * @return the number of points
 *
 * @since OGR 1.9.0
 */

int OGR_G_GetPoints( OGRGeometryH hGeom,
                     void* pabyX, int nXStride,
                     void* pabyY, int nYStride,
                     void* pabyZ, int nZStride)
{
    VALIDATE_POINTER1( hGeom, "OGR_G_GetPoints", 0 );

    switch( wkbFlatten(((OGRGeometry *) hGeom)->getGeometryType()) )
    {
      case wkbPoint:
      {
        if (pabyX) *((double*)pabyX) = ((OGRPoint *)hGeom)->getX();
        if (pabyY) *((double*)pabyY) = ((OGRPoint *)hGeom)->getY();
        if (pabyZ) *((double*)pabyZ) = ((OGRPoint *)hGeom)->getZ();
        return 1;
      }
      break;

      case wkbLineString:
      {
          OGRLineString* poLS = (OGRLineString *) hGeom;
          poLS->getPoints(pabyX, nXStride, pabyY, nYStride, pabyZ, nZStride);
          return poLS->getNumPoints();
      }
      break;

      default:
        CPLError(CE_Failure, CPLE_NotSupported, "Incompatible geometry for operation");
        return 0;
        break;
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
          OGRLineString* poLS = (OGRLineString *) hGeom;
          if (i < 0 || i >= poLS->getNumPoints())
          {
              CPLError(CE_Failure, CPLE_NotSupported, "Index out of bounds");
              *pdfX = *pdfY = 0;
              if( pdfZ != NULL )
                  *pdfZ = 0;
          }
          else
          {
            *pdfX = poLS->getX( i );
            *pdfY = poLS->getY( i );
            if( pdfZ != NULL )
                *pdfZ =  poLS->getZ( i );
          }
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
 * \brief Assign all points in a point or a line string geometry.
 *
 * This method clear any existing points assigned to this geometry,
 * and assigns a whole new set.
 *
 * @param hGeom handle to the geometry to set the coordinates.
 * @param nPointsIn number of points being passed in padfX and padfY.
 * @param padfX list of X coordinates of points being assigned.
 * @param nXStride the number of bytes between 2 elements of pabyX.
 * @param padfY list of Y coordinates of points being assigned.
 * @param nYStride the number of bytes between 2 elements of pabyY.
 * @param padfZ list of Z coordinates of points being assigned (defaults to NULL for 2D objects).
 * @param nZStride the number of bytes between 2 elements of pabyZ.
 */

void CPL_DLL OGR_G_SetPoints( OGRGeometryH hGeom, int nPointsIn,
                              void* pabyX, int nXStride,
                              void* pabyY, int nYStride,
                              void* pabyZ, int nZStride )

{
    VALIDATE_POINTER0( hGeom, "OGR_G_SetPoints" );

    switch( wkbFlatten(((OGRGeometry *) hGeom)->getGeometryType()) )
    {
      case wkbPoint:
      {
        ((OGRPoint *) hGeom)->setX( pabyX ? *( (double *)pabyX ) : 0.0 );
        ((OGRPoint *) hGeom)->setY( pabyY ? *( (double *)pabyY ) : 0.0 );
        ((OGRPoint *) hGeom)->setZ( pabyZ ? *( (double *)pabyZ ) : 0.0 );
        break;
      }
      case wkbLineString:
      {
        OGRLineString* poLine = (OGRLineString *) hGeom;

        if( nXStride == 0 && nYStride == 0 && nZStride == 0 )
        {
          poLine->setPoints( nPointsIn, (double *)pabyX, (double *)pabyY, (double *)pabyZ ); 
        }
        else
        {
          double x, y, z;		  
          x = y = z = 0;
          poLine->setNumPoints( nPointsIn );

          for (int i = 0; i < nPointsIn; ++i)
          {
            if( pabyX ) x = *(double*)((char*)pabyX + i * nXStride);
            if( pabyY ) y = *(double*)((char*)pabyY + i * nYStride);
            if( pabyZ ) z = *(double*)((char*)pabyZ + i * nZStride);

            poLine->setPoint( i, x, y, z );
          }
        }
        break;
      }
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
      {
          if (i < 0)
          {
              CPLError(CE_Failure, CPLE_NotSupported, "Index out of bounds");
              return;
          }
          ((OGRLineString *) hGeom)->setPoint( i, dfX, dfY, dfZ );
          break;
      }

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
      {
          if (i < 0)
          {
              CPLError(CE_Failure, CPLE_NotSupported, "Index out of bounds");
              return;
          }
          ((OGRLineString *) hGeom)->setPoint( i, dfX, dfY );
          break;
      }

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
    VALIDATE_POINTER1( hGeom, "OGR_G_RemoveGeometry", 0 );

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
/*                           OGR_G_Length()                             */
/************************************************************************/

/**
 * \brief Compute length of a geometry.
 *
 * Computes the area for OGRCurve or MultiCurve objects.
 * Undefined for all other geometry types (returns zero). 
 *
 * This function utilizes the C++ get_Length() method.
 *
 * @param hGeom the geometry to operate on.
 * @return the lenght or 0.0 for unsupported geometry types.
 *
 * @since OGR 1.8.0
 */

double OGR_G_Length( OGRGeometryH hGeom )

{
    VALIDATE_POINTER1( hGeom, "OGR_G_GetLength", 0 );

    double fLength = 0.0;

    switch( wkbFlatten(((OGRGeometry *) hGeom)->getGeometryType()) )
    {
      case wkbLinearRing:
      case wkbLineString:
        fLength = ((OGRCurve *) hGeom)->get_Length();
        break;

      case wkbMultiLineString:
      case wkbGeometryCollection:
        fLength = ((OGRGeometryCollection *) hGeom)->get_Length();
        break;

      default:
        CPLError( CE_Warning, CPLE_AppDefined,
                  "OGR_G_Length() called against a non-curve geometry type." );

        fLength = 0.0;
    }

    return fLength;
}

/************************************************************************/
/*                           OGR_G_Area()                               */
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
 *
 * @since OGR 1.8.0
 */

double OGR_G_Area( OGRGeometryH hGeom )

{
    VALIDATE_POINTER1( hGeom, "OGR_G_Area", 0 );

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
                  "OGR_G_Area() called against non-surface geometry type." );

        fArea = 0.0;
    }

    return fArea;
}

/**
 * \brief Compute geometry area (deprecated)
 *
 * @deprecated
 * @see OGR_G_Area()
 */
double OGR_G_GetArea( OGRGeometryH hGeom )

{
    return OGR_G_Area( hGeom );
}
