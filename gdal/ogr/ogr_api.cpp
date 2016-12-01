/******************************************************************************
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  C API Functions that don't correspond one-to-one with C++
 *           methods, such as the "simplified" geometry access functions.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2002, Frank Warmerdam
 * Copyright (c) 2009-2011, Even Rouault <even dot rouault at mines-paris dot org>
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

#include "cpl_port.h"
#include "ogr_api.h"

#include <cstddef>

#include "cpl_error.h"
#include "ogr_geometry.h"

CPL_CVSID("$Id$");

static bool bNonLinearGeometriesEnabled = true;

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

    const OGRwkbGeometryType eGType =
        wkbFlatten(reinterpret_cast<OGRGeometry *>(hGeom)->getGeometryType());
    if( eGType == wkbPoint )
    {
        return 1;
    }
    else if( OGR_GT_IsCurve(eGType) )
    {
        return reinterpret_cast<OGRCurve *>(hGeom)->getNumPoints();
    }
    else
    {
        // autotest/pymod/ogrtest.py calls this method on any geometry. So keep
        // silent.
        // CPLError(CE_Failure, CPLE_NotSupported,
        //          "Incompatible geometry for operation");
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
 * @param hGeom handle to the geometry.
 * @param nNewPointCount the new number of points for geometry.
 */

void OGR_G_SetPointCount( OGRGeometryH hGeom, int nNewPointCount )

{
    VALIDATE_POINTER0( hGeom, "OGR_G_SetPointCount" );

    switch( wkbFlatten(reinterpret_cast<OGRGeometry *>(hGeom)->
                           getGeometryType()) )
    {
      case wkbLineString:
      case wkbCircularString:
      {
          OGRSimpleCurve* poSC = reinterpret_cast<OGRSimpleCurve *>(hGeom);
          poSC->setNumPoints( nNewPointCount );
          break;
      }
      default:
          CPLError(CE_Failure, CPLE_NotSupported,
                   "Incompatible geometry for operation");
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

    switch( wkbFlatten(reinterpret_cast<OGRGeometry *>(hGeom)->
                           getGeometryType()) )
    {
      case wkbPoint:
      {
          if( i == 0 )
          {
              return reinterpret_cast<OGRPoint *>(hGeom)->getX();
          }
          else
          {
              CPLError(CE_Failure, CPLE_NotSupported,
                       "Only i == 0 is supported");
              return 0.0;
          }
      }

      case wkbLineString:
      case wkbCircularString:
      {
          OGRSimpleCurve* poSC = reinterpret_cast<OGRSimpleCurve *>(hGeom);
          if( i < 0 || i >= poSC->getNumPoints() )
          {
              CPLError(CE_Failure, CPLE_NotSupported, "Index out of bounds");
              return 0.0;
          }
          return poSC->getX( i );
      }

      default:
          CPLError(CE_Failure, CPLE_NotSupported,
                   "Incompatible geometry for operation");
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

    switch( wkbFlatten(reinterpret_cast<OGRGeometry *>(hGeom)->
                           getGeometryType()) )
    {
      case wkbPoint:
      {
          if( i == 0 )
          {
              return reinterpret_cast<OGRPoint *>(hGeom)->getY();
          }
          else
          {
              CPLError(CE_Failure, CPLE_NotSupported,
                       "Only i == 0 is supported");
              return 0.0;
          }
      }

      case wkbLineString:
      case wkbCircularString:
      {
          OGRSimpleCurve* poSC = reinterpret_cast<OGRSimpleCurve *>(hGeom);
          if( i < 0 || i >= poSC->getNumPoints() )
          {
              CPLError(CE_Failure, CPLE_NotSupported, "Index out of bounds");
              return 0.0;
          }
          return poSC->getY( i );
      }

      default:
        CPLError(CE_Failure, CPLE_NotSupported,
                 "Incompatible geometry for operation");
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

    switch( wkbFlatten(reinterpret_cast<OGRGeometry *>(hGeom)->
                           getGeometryType()) )
    {
      case wkbPoint:
      {
          if( i == 0 )
          {
              return reinterpret_cast<OGRPoint *>(hGeom)->getZ();
          }
          else
          {
              CPLError(CE_Failure, CPLE_NotSupported,
                       "Only i == 0 is supported");
              return 0.0;
          }
      }

      case wkbLineString:
      case wkbCircularString:
      {
          OGRSimpleCurve* poSC = reinterpret_cast<OGRSimpleCurve *>(hGeom);
          if( i < 0 || i >= poSC->getNumPoints() )
          {
              CPLError(CE_Failure, CPLE_NotSupported, "Index out of bounds");
              return 0.0;
          }
          return poSC->getZ( i );
      }

      default:
          CPLError(CE_Failure, CPLE_NotSupported,
                   "Incompatible geometry for operation");
          return 0.0;
    }
}

/************************************************************************/
/*                             OGR_G_GetM()                             */
/************************************************************************/
/**
 * \brief Fetch the m coordinate of a point from a geometry.
 *
 * @param hGeom handle to the geometry from which to get the M coordinate.
 * @param i point to get the M coordinate.
 * @return the M coordinate of this point.
 */

double OGR_G_GetM( OGRGeometryH hGeom, int i )

{
    VALIDATE_POINTER1( hGeom, "OGR_G_GetM", 0 );

    switch( wkbFlatten(reinterpret_cast<OGRGeometry *>(hGeom)->
                           getGeometryType()) )
    {
      case wkbPoint:
      {
          if( i == 0 )
          {
              return reinterpret_cast<OGRPoint *>(hGeom)->getM();
          }
          else
          {
              CPLError(CE_Failure, CPLE_NotSupported,
                       "Only i == 0 is supported");
              return 0.0;
          }
      }

      case wkbLineString:
      case wkbCircularString:
      {
          OGRSimpleCurve* poSC = reinterpret_cast<OGRSimpleCurve *>(hGeom);
          if( i < 0 || i >= poSC->getNumPoints() )
          {
              CPLError(CE_Failure, CPLE_NotSupported, "Index out of bounds");
              return 0.0;
          }
          return poSC->getM( i );
      }

      default:
          CPLError(CE_Failure, CPLE_NotSupported,
                   "Incompatible geometry for operation");
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
 * stride between 2 consecutive elements of the array.
 *
 * On some CPU architectures, care must be taken so that the arrays are properly
 * aligned.
 *
 * @param hGeom handle to the geometry from which to get the coordinates.
 * @param pabyX a buffer of at least (sizeof(double) * nXStride * nPointCount)
 * bytes, may be NULL.
 * @param nXStride the number of bytes between 2 elements of pabyX.
 * @param pabyY a buffer of at least (sizeof(double) * nYStride * nPointCount)
 * bytes, may be NULL.
 * @param nYStride the number of bytes between 2 elements of pabyY.
 * @param pabyZ a buffer of at last size (sizeof(double) * nZStride *
 * nPointCount) bytes, may be NULL.
 * @param nZStride the number of bytes between 2 elements of pabyZ.
 *
 * @return the number of points
 *
 * @since OGR 1.9.0
 */

int OGR_G_GetPoints( OGRGeometryH hGeom,
                     void* pabyX, int nXStride,
                     void* pabyY, int nYStride,
                     void* pabyZ, int nZStride )
{
    VALIDATE_POINTER1( hGeom, "OGR_G_GetPoints", 0 );

    switch( wkbFlatten(reinterpret_cast<OGRGeometry *>(hGeom)->
                           getGeometryType()) )
    {
      case wkbPoint:
      {
          OGRPoint *poPoint = reinterpret_cast<OGRPoint *>(hGeom);
          if( pabyX ) *(static_cast<double *>(pabyX)) = poPoint->getX();
          if( pabyY ) *(static_cast<double *>(pabyY)) = poPoint->getY();
          if( pabyZ ) *(static_cast<double *>(pabyZ)) = poPoint->getZ();
          return 1;
      }
      break;

      case wkbLineString:
      case wkbCircularString:
      {
          OGRSimpleCurve* poSC = reinterpret_cast<OGRSimpleCurve *>(hGeom);
          poSC->getPoints(pabyX, nXStride, pabyY, nYStride, pabyZ, nZStride);
          return poSC->getNumPoints();
      }
      break;

      default:
        CPLError(CE_Failure, CPLE_NotSupported,
                 "Incompatible geometry for operation");
        return 0;
        break;
    }
}

/************************************************************************/
/*                          OGR_G_GetPointsZM()                         */
/************************************************************************/

/**
 * \brief Returns all points of line string.
 *
 * This method copies all points into user arrays. The user provides the
 * stride between 2 consecutive elements of the array.
 *
 * On some CPU architectures, care must be taken so that the arrays are properly
 * aligned.
 *
 * @param hGeom handle to the geometry from which to get the coordinates.
 * @param pabyX a buffer of at least (sizeof(double) * nXStride * nPointCount)
 * bytes, may be NULL.
 * @param nXStride the number of bytes between 2 elements of pabyX.
 * @param pabyY a buffer of at least (sizeof(double) * nYStride * nPointCount)
 * bytes, may be NULL.
 * @param nYStride the number of bytes between 2 elements of pabyY.
 * @param pabyZ a buffer of at last size (sizeof(double) * nZStride *
 * nPointCount) bytes, may be NULL.
 * @param nZStride the number of bytes between 2 elements of pabyZ.
 * @param pabyM a buffer of at last size (sizeof(double) * nMStride *
 * nPointCount) bytes, may be NULL.
 * @param nMStride the number of bytes between 2 elements of pabyM.
 *
 * @return the number of points
 *
 * @since OGR 1.9.0
 */

int OGR_G_GetPointsZM( OGRGeometryH hGeom,
                       void* pabyX, int nXStride,
                       void* pabyY, int nYStride,
                       void* pabyZ, int nZStride,
                       void* pabyM, int nMStride )
{
    VALIDATE_POINTER1( hGeom, "OGR_G_GetPointsZM", 0 );

    switch( wkbFlatten(reinterpret_cast<OGRGeometry *>(hGeom)->
                           getGeometryType()) )
    {
      case wkbPoint:
      {
          OGRPoint *poPoint = reinterpret_cast<OGRPoint *>(hGeom);
          if( pabyX ) *static_cast<double *>(pabyX) = poPoint->getX();
          if( pabyY ) *static_cast<double *>(pabyY) = poPoint->getY();
          if( pabyZ ) *static_cast<double *>(pabyZ) = poPoint->getZ();
          if( pabyM ) *static_cast<double *>(pabyM) = poPoint->getM();
          return 1;
      }
      break;

      case wkbLineString:
      case wkbCircularString:
      {
          OGRSimpleCurve* poSC = reinterpret_cast<OGRSimpleCurve *>(hGeom);
          poSC->getPoints(pabyX, nXStride, pabyY, nYStride, pabyZ, nZStride,
                          pabyM, nMStride);
          return poSC->getNumPoints();
      }
      break;

      default:
          CPLError(CE_Failure, CPLE_NotSupported,
                   "Incompatible geometry for operation");
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

    switch( wkbFlatten(reinterpret_cast<OGRGeometry *>(hGeom)->
                           getGeometryType()) )
    {
      case wkbPoint:
      {
          if( i == 0 )
          {
              OGRPoint *poPoint = reinterpret_cast<OGRPoint *>(hGeom);
              *pdfX = poPoint->getX();
              *pdfY = poPoint->getY();
              if( pdfZ != NULL )
                  *pdfZ = poPoint->getZ();
          }
          else
          {
              CPLError(CE_Failure, CPLE_NotSupported,
                       "Only i == 0 is supported");
          }
      }
      break;

      case wkbLineString:
      case wkbCircularString:
      {
          OGRSimpleCurve* poSC = reinterpret_cast<OGRSimpleCurve *>(hGeom);
          if( i < 0 || i >= poSC->getNumPoints() )
          {
              CPLError(CE_Failure, CPLE_NotSupported, "Index out of bounds");
              *pdfX = 0.0;
              *pdfY = 0.0;
              if( pdfZ != NULL )
                  *pdfZ = 0.0;
          }
          else
          {
            *pdfX = poSC->getX( i );
            *pdfY = poSC->getY( i );
            if( pdfZ != NULL )
                *pdfZ = poSC->getZ( i );
          }
      }
      break;

      default:
        CPLError(CE_Failure, CPLE_NotSupported,
                 "Incompatible geometry for operation");
        break;
    }
}

/************************************************************************/
/*                           OGR_G_GetPointZM()                         */
/************************************************************************/

/**
 * \brief Fetch a point in line string or a point geometry.
 *
 * @param hGeom handle to the geometry from which to get the coordinates.
 * @param i the vertex to fetch, from 0 to getNumPoints()-1, zero for a point.
 * @param pdfX value of x coordinate.
 * @param pdfY value of y coordinate.
 * @param pdfZ value of z coordinate.
 * @param pdfM value of m coordinate.
 */

void OGR_G_GetPointZM( OGRGeometryH hGeom, int i,
                       double *pdfX, double *pdfY, double *pdfZ, double *pdfM )

{
    VALIDATE_POINTER0( hGeom, "OGR_G_GetPointZM" );

    switch( wkbFlatten(reinterpret_cast<OGRGeometry *>(hGeom)->
                           getGeometryType()) )
    {
      case wkbPoint:
      {
          if( i == 0 )
          {
              OGRPoint *poPoint = reinterpret_cast<OGRPoint *>(hGeom);
              *pdfX = poPoint->getX();
              *pdfY = poPoint->getY();
              if( pdfZ != NULL )
                  *pdfZ = poPoint->getZ();
              if( pdfM != NULL )
                  *pdfM = poPoint->getM();
          }
          else
          {
              CPLError(CE_Failure, CPLE_NotSupported,
                       "Only i == 0 is supported");
          }
      }
      break;

      case wkbLineString:
      case wkbCircularString:
      {
          OGRSimpleCurve* poSC = reinterpret_cast<OGRSimpleCurve *>(hGeom);
          if( i < 0 || i >= poSC->getNumPoints() )
          {
              CPLError(CE_Failure, CPLE_NotSupported, "Index out of bounds");
              *pdfX = 0.0;
              *pdfY = 0.0;
              if( pdfZ != NULL )
                  *pdfZ = 0.0;
              if( pdfM != NULL )
                  *pdfM = 0.0;
          }
          else
          {
              *pdfX = poSC->getX( i );
              *pdfY = poSC->getY( i );
              if( pdfZ != NULL )
                  *pdfZ = poSC->getZ( i );
              if( pdfM != NULL )
                  *pdfM = poSC->getM( i );
          }
      }
      break;

      default:
          CPLError(CE_Failure, CPLE_NotSupported,
                   "Incompatible geometry for operation");
          break;
    }
}

/************************************************************************/
/*                           OGR_G_SetPoints()                          */
/************************************************************************/
/**
 * \brief Assign all points in a point or a line string geometry.
 *
 * This method clear any existing points assigned to this geometry,
 * and assigns a whole new set.
 *
 * @param hGeom handle to the geometry to set the coordinates.
 * @param nPointsIn number of points being passed in padfX and padfY.
 * @param pabyX list of X coordinates (double values) of points being assigned.
 * @param nXStride the number of bytes between 2 elements of pabyX.
 * @param pabyY list of Y coordinates (double values) of points being assigned.
 * @param nYStride the number of bytes between 2 elements of pabyY.
 * @param pabyZ list of Z coordinates (double values) of points being assigned
 * (defaults to NULL for 2D objects).
 * @param nZStride the number of bytes between 2 elements of pabyZ.
 */

void CPL_DLL OGR_G_SetPoints( OGRGeometryH hGeom, int nPointsIn,
                              void* pabyX, int nXStride,
                              void* pabyY, int nYStride,
                              void* pabyZ, int nZStride )

{
    VALIDATE_POINTER0( hGeom, "OGR_G_SetPoints" );

    if( pabyX == NULL || pabyY == NULL )
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "pabyX == NULL || pabyY == NULL");
        return;
    }

    double * const padfX = static_cast<double *>(pabyX);
    double * const padfY = static_cast<double *>(pabyY);
    double * const padfZ = static_cast<double *>(pabyZ);

    switch( wkbFlatten(reinterpret_cast<OGRGeometry *>(hGeom)->
                           getGeometryType()) )
    {
      case wkbPoint:
      {
          OGRPoint *poPoint = reinterpret_cast<OGRPoint *>(hGeom);
          poPoint->setX( *padfX );
          poPoint->setY( *padfY );
          if( pabyZ != NULL )
              poPoint->setZ(*( padfZ ) );
          break;
      }
      case wkbLineString:
      case wkbCircularString:
      {
        OGRSimpleCurve* poSC = reinterpret_cast<OGRSimpleCurve *>(hGeom);

        const int nSizeDouble = (int)sizeof(double);
        if( nXStride == nSizeDouble &&
            nYStride == nSizeDouble &&
            ((nZStride == 0 && pabyZ == NULL) ||
             (nZStride == nSizeDouble && pabyZ != NULL)) )
        {
            poSC->setPoints(nPointsIn, padfX, padfY, padfZ);
        }
        else
        {
          poSC->setNumPoints( nPointsIn );

          // TODO(schwehr): Create pasX and pasY.
          for( int i = 0; i < nPointsIn; ++i )
          {
            const double x = *reinterpret_cast<double *>(
                reinterpret_cast<char *>(pabyX) + i * nXStride);
            const double y = *reinterpret_cast<double *>(
                reinterpret_cast<char *>(pabyY) + i * nYStride);
            if( pabyZ )
            {
                const double z = *reinterpret_cast<double *>(
                    reinterpret_cast<char *>(pabyZ) + i * nZStride);
                poSC->setPoint( i, x, y, z );
            }
            else
            {
                poSC->setPoint( i, x, y );
            }
          }
        }
        break;
      }
      default:
        CPLError(CE_Failure, CPLE_NotSupported,
                 "Incompatible geometry for operation");
        break;
    }
}

/************************************************************************/
/*                           OGR_G_SetPointsZM()                        */
/************************************************************************/
/**
 * \brief Assign all points in a point or a line string geometry.
 *
 * This method clear any existing points assigned to this geometry,
 * and assigns a whole new set.
 *
 * @param hGeom handle to the geometry to set the coordinates.
 * @param nPointsIn number of points being passed in padfX and padfY.
 * @param pabyX list of X coordinates (double values) of points being assigned.
 * @param nXStride the number of bytes between 2 elements of pabyX.
 * @param pabyY list of Y coordinates (double values) of points being assigned.
 * @param nYStride the number of bytes between 2 elements of pabyY.
 * @param pabyZ list of Z coordinates (double values) of points being assigned
 * (if not NULL, upgrades the geometry to have Z coordinate).
 * @param nZStride the number of bytes between 2 elements of pabyZ.
 * @param pabyM list of M coordinates (double values) of points being assigned
 * (if not NULL, upgrades the geometry to have M coordinate).
 * @param nMStride the number of bytes between 2 elements of pabyM.
 */

void CPL_DLL OGR_G_SetPointsZM( OGRGeometryH hGeom, int nPointsIn,
                                void* pabyX, int nXStride,
                                void* pabyY, int nYStride,
                                void* pabyZ, int nZStride,
                                void* pabyM, int nMStride )

{
    VALIDATE_POINTER0( hGeom, "OGR_G_SetPointsZM" );

    if( pabyX == NULL || pabyY == NULL )
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "pabyX == NULL || pabyY == NULL");
        return;
    }

    double * const padfX = static_cast<double *>(pabyX);
    double * const padfY = static_cast<double *>(pabyY);
    double * const padfZ = static_cast<double *>(pabyZ);
    double * const padfM = static_cast<double *>(pabyM);

    switch( wkbFlatten(reinterpret_cast<OGRGeometry *>(hGeom)->
                           getGeometryType()) )
    {
      case wkbPoint:
      {
          OGRPoint *poPoint = reinterpret_cast<OGRPoint *>(hGeom);
          poPoint->setX(*padfX);
          poPoint->setY(*padfY);
          if( pabyZ )
              poPoint->setZ(*padfZ);
          if( pabyM )
              poPoint->setM(*padfM);
          break;
      }
      case wkbLineString:
      case wkbCircularString:
      {
        OGRSimpleCurve* poSC = reinterpret_cast<OGRSimpleCurve *>(hGeom);

        const int nSizeDouble = static_cast<int>(sizeof(double));
        if( nXStride == nSizeDouble &&
            nYStride == nSizeDouble &&
            ((nZStride == 0 && padfZ == NULL) ||
             (nZStride == nSizeDouble && padfZ != NULL)) &&
            ((nMStride == 0 && padfM == NULL) ||
             (nMStride == nSizeDouble && padfM != NULL)) )
        {
            if( !padfZ && !padfM )
                poSC->setPoints( nPointsIn, padfX, padfY );
            else if( pabyZ && !pabyM )
                poSC->setPoints( nPointsIn, padfX, padfY, padfZ );
            else if( !pabyZ && pabyM )
                poSC->setPointsM( nPointsIn, padfX, padfY, padfM );
            else
                poSC->setPoints( nPointsIn, padfX, padfY, padfZ, padfM );
        }
        else
        {
          poSC->setNumPoints( nPointsIn );

          // TODO(schwehr): Create pasX and pasY.
          if( !pabyZ && !pabyM )
          {
              for( int i = 0; i < nPointsIn; ++i )
              {
                  const double x = *reinterpret_cast<double *>(
                      reinterpret_cast<char *>(pabyX) + i * nXStride);
                  const double y = *reinterpret_cast<double *>(
                      reinterpret_cast<char *>(pabyY) + i * nYStride);
                  poSC->setPoint( i, x, y );
              }
          }
          else if( pabyZ && !pabyM )
          {
              for( int i = 0; i < nPointsIn; ++i )
              {
                  const double x = *reinterpret_cast<double *>(
                      reinterpret_cast<char *>(pabyX) + i * nXStride);
                  const double y = *reinterpret_cast<double *>(
                      reinterpret_cast<char *>(pabyY) + i * nYStride);
                  const double z = *reinterpret_cast<double *>(
                      reinterpret_cast<char *>(pabyZ) + i * nZStride);
                  poSC->setPoint( i, x, y, z );
              }
          }
          else if( !pabyZ && pabyM )
          {
              for( int i = 0; i < nPointsIn; ++i )
              {
                  const double x = *reinterpret_cast<double *>(
                      reinterpret_cast<char *>(pabyX) + i * nXStride);
                  const double y = *reinterpret_cast<double *>(
                      reinterpret_cast<char *>(pabyY) + i * nYStride);
                  const double m = *reinterpret_cast<double *>(
                      reinterpret_cast<char *>(pabyM) + i * nMStride);
                  poSC->setPointM( i, x, y, m );
              }
          }
          else
          {
              for( int i = 0; i < nPointsIn; ++i )
              {
                  const double x = *reinterpret_cast<double *>(
                      reinterpret_cast<char *>(pabyX) + i * nXStride);
                  const double y = *reinterpret_cast<double *>(
                      reinterpret_cast<char *>(pabyY) + i * nYStride);
                  const double z = *reinterpret_cast<double *>(
                      reinterpret_cast<char *>(pabyZ) + i * nZStride);
                  const double m = *reinterpret_cast<double *>(
                      reinterpret_cast<char *>(pabyM) + i * nMStride);
                  poSC->setPoint( i, x, y, z, m );
              }
          }
        }
        break;
      }
      default:
        CPLError(CE_Failure, CPLE_NotSupported,
                 "Incompatible geometry for operation");
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
 * accommodate the request.
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

    switch( wkbFlatten(reinterpret_cast<OGRGeometry *>(hGeom)->
                           getGeometryType()) )
    {
      case wkbPoint:
      {
          if( i == 0 )
          {
              OGRPoint *poPoint = reinterpret_cast<OGRPoint *>(hGeom);
              poPoint->setX(dfX);
              poPoint->setY(dfY);
              poPoint->setZ(dfZ);
          }
          else
          {
              CPLError(CE_Failure, CPLE_NotSupported,
                       "Only i == 0 is supported");
          }
      }
      break;

      case wkbLineString:
      case wkbCircularString:
      {
          if( i < 0 )
          {
              CPLError(CE_Failure, CPLE_NotSupported, "Index out of bounds");
              return;
          }
          reinterpret_cast<OGRSimpleCurve *>(hGeom)->setPoint(i, dfX, dfY, dfZ);
          break;
      }

      default:
          CPLError(CE_Failure, CPLE_NotSupported,
                   "Incompatible geometry for operation");
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
 * accommodate the request.
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

    switch( wkbFlatten(reinterpret_cast<OGRGeometry *>(hGeom)->
                           getGeometryType()) )
    {
      case wkbPoint:
      {
          if( i == 0 )
          {
              OGRPoint *poPoint = reinterpret_cast<OGRPoint *>(hGeom);
              poPoint->setX(dfX);
              poPoint->setY(dfY);
          }
          else
          {
              CPLError(CE_Failure, CPLE_NotSupported,
                       "Only i == 0 is supported");
          }
      }
      break;

      case wkbLineString:
      case wkbCircularString:
      {
          if( i < 0 )
          {
              CPLError(CE_Failure, CPLE_NotSupported, "Index out of bounds");
              return;
          }
          reinterpret_cast<OGRSimpleCurve *>(hGeom)->setPoint(i, dfX, dfY);
          break;
      }

      default:
          CPLError(CE_Failure, CPLE_NotSupported,
                   "Incompatible geometry for operation");
        break;
    }
}

/************************************************************************/
/*                           OGR_G_SetPointM()                          */
/************************************************************************/
/**
 * \brief Set the location of a vertex in a point or linestring geometry.
 *
 * If iPoint is larger than the number of existing
 * points in the linestring, the point count will be increased to
 * accommodate the request.
 *
 * @param hGeom handle to the geometry to add a vertex to.
 * @param i the index of the vertex to assign (zero based) or
 *  zero for a point.
 * @param dfX input X coordinate to assign.
 * @param dfY input Y coordinate to assign.
 * @param dfM input M coordinate to assign.
 */

void OGR_G_SetPointM( OGRGeometryH hGeom, int i,
                      double dfX, double dfY, double dfM )

{
    VALIDATE_POINTER0( hGeom, "OGR_G_SetPointM" );

    switch( wkbFlatten(reinterpret_cast<OGRGeometry *>(hGeom)->
                           getGeometryType()) )
    {
      case wkbPoint:
      {
          if( i == 0 )
          {
              OGRPoint *poPoint = reinterpret_cast<OGRPoint *>(hGeom);
              poPoint->setX(dfX);
              poPoint->setY(dfY);
              poPoint->setM(dfM);
          }
          else
          {
              CPLError(CE_Failure, CPLE_NotSupported,
                       "Only i == 0 is supported");
          }
      }
      break;

      case wkbLineString:
      case wkbCircularString:
      {
          if( i < 0 )
          {
              CPLError(CE_Failure, CPLE_NotSupported, "Index out of bounds");
              return;
          }
          reinterpret_cast<OGRSimpleCurve *>(hGeom)->
              setPointM(i, dfX, dfY, dfM);
          break;
      }

      default:
          CPLError(CE_Failure, CPLE_NotSupported,
                   "Incompatible geometry for operation");
          break;
    }
}

/************************************************************************/
/*                           OGR_G_SetPointZM()                         */
/************************************************************************/
/**
 * \brief Set the location of a vertex in a point or linestring geometry.
 *
 * If iPoint is larger than the number of existing
 * points in the linestring, the point count will be increased to
 * accommodate the request.
 *
 * @param hGeom handle to the geometry to add a vertex to.
 * @param i the index of the vertex to assign (zero based) or
 *  zero for a point.
 * @param dfX input X coordinate to assign.
 * @param dfY input Y coordinate to assign.
 * @param dfZ input Z coordinate to assign.
 * @param dfM input M coordinate to assign.
 */

void OGR_G_SetPointZM( OGRGeometryH hGeom, int i,
                       double dfX, double dfY, double dfZ, double dfM )

{
    VALIDATE_POINTER0( hGeom, "OGR_G_SetPointZM" );

    switch( wkbFlatten(reinterpret_cast<OGRGeometry *>(hGeom)->
                           getGeometryType()) )
    {
      case wkbPoint:
      {
          if( i == 0 )
          {
              OGRPoint *poPoint = reinterpret_cast<OGRPoint *>(hGeom);
              poPoint->setX(dfX);
              poPoint->setY(dfY);
              poPoint->setZ(dfZ);
              poPoint->setM(dfM);
          }
          else
          {
              CPLError(CE_Failure, CPLE_NotSupported,
                       "Only i == 0 is supported");
          }
      }
      break;

      case wkbLineString:
      case wkbCircularString:
      {
          if( i < 0 )
          {
              CPLError(CE_Failure, CPLE_NotSupported, "Index out of bounds");
              return;
          }
          reinterpret_cast<OGRSimpleCurve *>(hGeom)->
              setPoint(i, dfX, dfY, dfZ, dfM);
          break;
      }

      default:
          CPLError(CE_Failure, CPLE_NotSupported,
                   "Incompatible geometry for operation");
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

    switch( wkbFlatten(reinterpret_cast<OGRGeometry *>(hGeom)->
                           getGeometryType()) )
    {
      case wkbPoint:
      {
          OGRPoint *poPoint = reinterpret_cast<OGRPoint *>(hGeom);
          poPoint->setX(dfX);
          poPoint->setY(dfY);
          poPoint->setZ(dfZ);
      }
      break;

      case wkbLineString:
      case wkbCircularString:
          reinterpret_cast<OGRSimpleCurve *>(hGeom)->addPoint(dfX, dfY, dfZ);
          break;

      default:
          CPLError(CE_Failure, CPLE_NotSupported,
                   "Incompatible geometry for operation");
          break;
    }
}

/************************************************************************/
/*                           OGR_G_AddPoint_2D()                        */
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

    switch( wkbFlatten(reinterpret_cast<OGRGeometry *>(hGeom)->
                           getGeometryType()) )
    {
      case wkbPoint:
      {
          OGRPoint *poPoint = reinterpret_cast<OGRPoint *>(hGeom);
          poPoint->setX(dfX);
          poPoint->setY(dfY);
      }
      break;

      case wkbLineString:
      case wkbCircularString:
          reinterpret_cast<OGRSimpleCurve *>(hGeom)->addPoint(dfX, dfY);
          break;

      default:
          CPLError(CE_Failure, CPLE_NotSupported,
                   "Incompatible geometry for operation");
          break;
    }
}

/************************************************************************/
/*                           OGR_G_AddPointM()                          */
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
 * @param dfM m coordinate of point to add.
 */

void OGR_G_AddPointM( OGRGeometryH hGeom,
                      double dfX, double dfY, double dfM )

{
    VALIDATE_POINTER0( hGeom, "OGR_G_AddPointM" );

    switch( wkbFlatten(reinterpret_cast<OGRGeometry *>(hGeom)->
                           getGeometryType()) )
    {
      case wkbPoint:
      {
          OGRPoint *poPoint = reinterpret_cast<OGRPoint *>(hGeom);
          poPoint->setX(dfX);
          poPoint->setY(dfY);
          poPoint->setM(dfM);
      }
      break;

      case wkbLineString:
      case wkbCircularString:
          reinterpret_cast<OGRSimpleCurve *>(hGeom)->addPointM(dfX, dfY, dfM);
          break;

      default:
          CPLError(CE_Failure, CPLE_NotSupported,
                   "Incompatible geometry for operation");
          break;
    }
}

/************************************************************************/
/*                           OGR_G_AddPointZM()                         */
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
 * @param dfM m coordinate of point to add.
 */

void OGR_G_AddPointZM( OGRGeometryH hGeom,
                       double dfX, double dfY, double dfZ, double dfM )

{
    VALIDATE_POINTER0( hGeom, "OGR_G_AddPointZM" );

    switch( wkbFlatten(reinterpret_cast<OGRGeometry *>(hGeom)->
                           getGeometryType()) )
    {
      case wkbPoint:
      {
          OGRPoint *poPoint = reinterpret_cast<OGRPoint *>(hGeom);
          poPoint->setX( dfX );
          poPoint->setY( dfY );
          poPoint->setZ( dfZ );
          poPoint->setM( dfM );
      }
      break;

      case wkbLineString:
      case wkbCircularString:
          reinterpret_cast<OGRSimpleCurve *>(hGeom)->
              addPoint(dfX, dfY, dfZ, dfM);
          break;

      default:
          CPLError(CE_Failure, CPLE_NotSupported,
                   "Incompatible geometry for operation");
          break;
    }
}

/************************************************************************/
/*                       OGR_G_GetGeometryCount()                       */
/************************************************************************/
/**
 * \brief Fetch the number of elements in a geometry or number of geometries in
 * container.
 *
 * Only geometries of type wkbPolygon[25D], wkbMultiPoint[25D],
 * wkbMultiLineString[25D], wkbMultiPolygon[25D] or wkbGeometryCollection[25D]
 * may return a valid value.  Other geometry types will silently return 0.
 *
 * For a polygon, the returned number is the number of rings (exterior ring +
 * interior rings).
 *
 * @param hGeom single geometry or geometry container from which to get
 * the number of elements.
 * @return the number of elements.
 */

int OGR_G_GetGeometryCount( OGRGeometryH hGeom )

{
    VALIDATE_POINTER1( hGeom, "OGR_G_GetGeometryCount", 0 );

    const OGRwkbGeometryType eType =
        wkbFlatten(reinterpret_cast<OGRGeometry *>(hGeom)->getGeometryType());
    if( OGR_GT_IsSubClassOf(eType, wkbCurvePolygon) )
    {
        if( reinterpret_cast<OGRCurvePolygon *>(hGeom)->
                getExteriorRingCurve() == NULL )
            return 0;
        else
            return reinterpret_cast<OGRCurvePolygon *>(hGeom)->
                getNumInteriorRings() + 1;
    }
    else if( OGR_GT_IsSubClassOf(eType, wkbCompoundCurve) )
    {
        return reinterpret_cast<OGRCompoundCurve *>(hGeom)->getNumCurves();
    }
    else if( OGR_GT_IsSubClassOf(eType, wkbGeometryCollection) )
    {
        return reinterpret_cast<OGRGeometryCollection *>(hGeom)->
            getNumGeometries();
    }
    else
    {
        // autotest/pymod/ogrtest.py calls this method on any geometry. So keep
        // silent.
        // CPLError(CE_Failure, CPLE_NotSupported,
        //          "Incompatible geometry for operation");
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
 * modified.  The handle is only valid until the next change to the
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

    const OGRwkbGeometryType eType =
        wkbFlatten(reinterpret_cast<OGRGeometry *>(hGeom)->getGeometryType());
    if( OGR_GT_IsSubClassOf(eType, wkbCurvePolygon) )
    {
        if( iSubGeom == 0 )
            return reinterpret_cast<OGRGeometryH>(
                reinterpret_cast<OGRCurvePolygon *>(hGeom)->
                    getExteriorRingCurve());
        else
            return reinterpret_cast<OGRGeometryH>(
                reinterpret_cast<OGRCurvePolygon *>(hGeom)->
                    getInteriorRingCurve(iSubGeom - 1));
    }
    else if( OGR_GT_IsSubClassOf(eType, wkbCompoundCurve) )
    {
      return reinterpret_cast<OGRGeometryH>(
          reinterpret_cast<OGRCompoundCurve *>(hGeom)->getCurve(iSubGeom));
    }
    else if( OGR_GT_IsSubClassOf(eType, wkbGeometryCollection) )
    {
        return reinterpret_cast<OGRGeometryH>(
            reinterpret_cast<OGRGeometryCollection *>(hGeom)->
                getGeometryRef(iSubGeom));
    }
    else
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "Incompatible geometry for operation");
        return NULL;
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
    VALIDATE_POINTER1( hGeom, "OGR_G_AddGeometry",
                       OGRERR_UNSUPPORTED_OPERATION );
    VALIDATE_POINTER1( hNewSubGeom, "OGR_G_AddGeometry",
                       OGRERR_UNSUPPORTED_OPERATION );

    OGRErr eErr = OGRERR_UNSUPPORTED_GEOMETRY_TYPE;

    const OGRwkbGeometryType eType =
        wkbFlatten(reinterpret_cast<OGRGeometry *>(hGeom)->getGeometryType());
    if( OGR_GT_IsSubClassOf(eType, wkbCurvePolygon) )
    {
        if( OGR_GT_IsCurve( wkbFlatten(reinterpret_cast<OGRGeometry *>(
                hNewSubGeom)->getGeometryType()) ) )
            eErr = reinterpret_cast<OGRCurvePolygon *>(hGeom)->
                addRing(reinterpret_cast<OGRCurve *>(hNewSubGeom));
    }
    else if( OGR_GT_IsSubClassOf(eType, wkbCompoundCurve) )
    {
        if( OGR_GT_IsCurve( wkbFlatten(reinterpret_cast<OGRGeometry *>(
                hNewSubGeom)->getGeometryType()) ) )
            eErr = reinterpret_cast<OGRCompoundCurve *>(hGeom)->
                addCurve(reinterpret_cast<OGRCurve *>(hNewSubGeom));
    }
    else if( OGR_GT_IsSubClassOf(eType, wkbGeometryCollection) )
    {
        eErr = reinterpret_cast<OGRGeometryCollection *>(hGeom)->
            addGeometry(reinterpret_cast<OGRGeometry *>(hNewSubGeom));
    }

    return eErr;
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
    VALIDATE_POINTER1( hGeom, "OGR_G_AddGeometryDirectly",
                       OGRERR_UNSUPPORTED_OPERATION );
    VALIDATE_POINTER1( hNewSubGeom, "OGR_G_AddGeometryDirectly",
                       OGRERR_UNSUPPORTED_OPERATION );

    OGRErr eErr = OGRERR_UNSUPPORTED_GEOMETRY_TYPE;

    const OGRwkbGeometryType eType =
        wkbFlatten(reinterpret_cast<OGRGeometry *>(hGeom)->getGeometryType());

    if( OGR_GT_IsSubClassOf(eType, wkbCurvePolygon) )
    {
        if( OGR_GT_IsCurve( wkbFlatten(reinterpret_cast<OGRGeometry *>(
                hNewSubGeom)->getGeometryType()) ) )
            eErr = reinterpret_cast<OGRCurvePolygon *>(hGeom)->
                addRingDirectly(reinterpret_cast<OGRCurve *>(hNewSubGeom));
    }
    else if( OGR_GT_IsSubClassOf(eType, wkbCompoundCurve) )
    {
        if( OGR_GT_IsCurve( wkbFlatten(reinterpret_cast<OGRGeometry *>(
                hNewSubGeom)->getGeometryType()) ) )
            eErr = reinterpret_cast<OGRCompoundCurve *>(hGeom)->
              addCurveDirectly(reinterpret_cast<OGRCurve *>(hNewSubGeom));
    }
    else if( OGR_GT_IsSubClassOf(eType, wkbGeometryCollection) )
    {
        eErr = reinterpret_cast<OGRGeometryCollection *>(hGeom)->
            addGeometryDirectly(reinterpret_cast<OGRGeometry *>(hNewSubGeom));
    }

    if( eErr != OGRERR_NONE )
        delete reinterpret_cast<OGRGeometry *>(hNewSubGeom);

    return eErr;
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
    VALIDATE_POINTER1( hGeom, "OGR_G_RemoveGeometry", OGRERR_FAILURE );

    const OGRwkbGeometryType eType =
        wkbFlatten(reinterpret_cast<OGRGeometry *>(hGeom)->getGeometryType());
    if( OGR_GT_IsSubClassOf(eType, wkbCurvePolygon) )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                "OGR_G_RemoveGeometry() not supported on polygons yet." );
        return OGRERR_UNSUPPORTED_OPERATION;
    }
    else if( OGR_GT_IsSubClassOf(eType, wkbGeometryCollection) )
    {
        return reinterpret_cast<OGRGeometryCollection *>(hGeom)->
            removeGeometry(iGeom, bDelete);
    }
    else
    {
        return OGRERR_UNSUPPORTED_OPERATION;
    }
}

/************************************************************************/
/*                           OGR_G_Length()                             */
/************************************************************************/

/**
 * \brief Compute length of a geometry.
 *
 * Computes the length for OGRCurve or MultiCurve objects.
 * Undefined for all other geometry types (returns zero).
 *
 * This function utilizes the C++ get_Length() method.
 *
 * @param hGeom the geometry to operate on.
 * @return the length or 0.0 for unsupported geometry types.
 *
 * @since OGR 1.8.0
 */

double OGR_G_Length( OGRGeometryH hGeom )

{
    VALIDATE_POINTER1( hGeom, "OGR_G_GetLength", 0 );

    double dfLength = 0.0;

    const OGRwkbGeometryType eType =
        wkbFlatten(reinterpret_cast<OGRGeometry *>(hGeom)->getGeometryType());
    if( OGR_GT_IsCurve(eType) )
    {
        dfLength = reinterpret_cast<OGRCurve *>(hGeom)->get_Length();
    }
    else if( OGR_GT_IsSubClassOf(eType, wkbMultiCurve) ||
             eType == wkbGeometryCollection )
    {
        dfLength = reinterpret_cast<OGRGeometryCollection *>(hGeom)->
            get_Length();
    }
    else
    {
        CPLError( CE_Warning, CPLE_AppDefined,
                  "OGR_G_Length() called against a non-curve geometry type." );
        dfLength = 0.0;
    }

    return dfLength;
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

    double dfArea = 0.0;

    const OGRwkbGeometryType eType =
        wkbFlatten(reinterpret_cast<OGRGeometry *>(hGeom)->getGeometryType());
    if( OGR_GT_IsSurface(eType) )
    {
        dfArea = reinterpret_cast<OGRSurface *>(hGeom)->get_Area();
    }
    else if( OGR_GT_IsCurve(eType) )
    {
        dfArea = reinterpret_cast<OGRCurve *>(hGeom)->get_Area();
    }
    else if( OGR_GT_IsSubClassOf(eType, wkbMultiSurface) ||
             eType == wkbGeometryCollection )
    {
        dfArea = reinterpret_cast<OGRGeometryCollection *>(hGeom)->get_Area();
    }
    else
    {
        CPLError( CE_Warning, CPLE_AppDefined,
                  "OGR_G_Area() called against non-surface geometry type." );

        dfArea = 0.0;
    }

    return dfArea;
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

/************************************************************************/
/*                         OGR_G_HasCurveGeometry()                     */
/************************************************************************/

/**
 * \brief Returns if this geometry is or has curve geometry.
 *
 * Returns if a geometry is or has CIRCULARSTRING, COMPOUNDCURVE, CURVEPOLYGON,
 * MULTICURVE or MULTISURFACE in it.
 *
 * If bLookForNonLinear is set to TRUE, it will be actually looked if the
 * geometry or its subgeometries are or contain a non-linear geometry in
 * them. In which case, if the method returns TRUE, it means that
 * OGR_G_GetLinearGeometry() would return an approximate version of the
 * geometry. Otherwise, OGR_G_GetLinearGeometry() would do a conversion, but
 * with just converting container type, like COMPOUNDCURVE -> LINESTRING,
 * MULTICURVE -> MULTILINESTRING or MULTISURFACE -> MULTIPOLYGON, resulting in a
 * "loss-less" conversion.
 *
 * This function is the same as C++ method OGRGeometry::hasCurveGeometry().
 *
 * @param hGeom the geometry to operate on.
 * @param bLookForNonLinear set it to TRUE to check if the geometry is or
 * contains a CIRCULARSTRING.
 * @return TRUE if this geometry is or has curve geometry.
 *
 * @since GDAL 2.0
 */

int OGR_G_HasCurveGeometry( OGRGeometryH hGeom, int bLookForNonLinear )
{
    VALIDATE_POINTER1( hGeom, "OGR_G_HasCurveGeometry", FALSE );
    return reinterpret_cast<OGRGeometry *>(hGeom)->
        hasCurveGeometry(bLookForNonLinear);
}

/************************************************************************/
/*                         OGR_G_GetLinearGeometry()                   */
/************************************************************************/

/**
 * \brief Return, possibly approximate, linear version of this geometry.
 *
 * Returns a geometry that has no CIRCULARSTRING, COMPOUNDCURVE, CURVEPOLYGON,
 * MULTICURVE or MULTISURFACE in it, by approximating curve geometries.
 *
 * The ownership of the returned geometry belongs to the caller.
 *
 * The reverse function is OGR_G_GetCurveGeometry().
 *
 * This method relates to the ISO SQL/MM Part 3 ICurve::CurveToLine() and
 * CurvePolygon::CurvePolyToPoly() methods.
 *
 * This function is the same as C++ method OGRGeometry::getLinearGeometry().
 *
 * @param hGeom the geometry to operate on.
 * @param dfMaxAngleStepSizeDegrees the largest step in degrees along the
 * arc, zero to use the default setting.
 * @param papszOptions options as a null-terminated list of strings or NULL.
 * See OGRGeometryFactory::curveToLineString() for valid options.
 *
 * @return a new geometry.
 *
 * @since GDAL 2.0
 */

OGRGeometryH CPL_DLL OGR_G_GetLinearGeometry( OGRGeometryH hGeom,
                                              double dfMaxAngleStepSizeDegrees,
                                              char** papszOptions )
{
    VALIDATE_POINTER1( hGeom, "OGR_G_GetLinearGeometry", NULL );
    return reinterpret_cast<OGRGeometryH>(
        reinterpret_cast<OGRGeometry *>(hGeom)->
            getLinearGeometry(dfMaxAngleStepSizeDegrees,
                              papszOptions ));
}

/************************************************************************/
/*                         OGR_G_GetCurveGeometry()                     */
/************************************************************************/

/**
 * \brief Return curve version of this geometry.
 *
 * Returns a geometry that has possibly CIRCULARSTRING, COMPOUNDCURVE,
 * CURVEPOLYGON, MULTICURVE or MULTISURFACE in it, by de-approximating linear
 * into curve geometries.
 *
 * If the geometry has no curve portion, the returned geometry will be a clone
 * of it.
 *
 * The ownership of the returned geometry belongs to the caller.
 *
 * The reverse function is OGR_G_GetLinearGeometry().
 *
 * This function is the same as C++ method OGRGeometry::getCurveGeometry().
 *
 * @param hGeom the geometry to operate on.
 * @param papszOptions options as a null-terminated list of strings.
 *                     Unused for now. Must be set to NULL.
 *
 * @return a new geometry.
 *
 * @since GDAL 2.0
 */

OGRGeometryH CPL_DLL OGR_G_GetCurveGeometry( OGRGeometryH hGeom,
                                             char** papszOptions )
{
    VALIDATE_POINTER1( hGeom, "OGR_G_GetCurveGeometry", NULL );

    return reinterpret_cast<OGRGeometryH>(
        reinterpret_cast<OGRGeometry *>(hGeom)->
            getCurveGeometry(papszOptions));
}

/************************************************************************/
/*                          OGR_G_Value()                               */
/************************************************************************/
/**
 * \brief Fetch point at given distance along curve.
 *
 * This function relates to the SF COM ICurve::get_Value() method.
 *
 * This function is the same as the C++ method OGRCurve::Value().
 *
 * @param hGeom curve geometry.
 * @param dfDistance distance along the curve at which to sample position.
 *                   This distance should be between zero and get_Length()
 *                   for this curve.
 * @return a point or NULL.
 *
 * @since GDAL 2.0
 */

OGRGeometryH OGR_G_Value( OGRGeometryH hGeom, double dfDistance )
{
    VALIDATE_POINTER1( hGeom, "OGR_G_Value", NULL );

    if( OGR_GT_IsCurve(reinterpret_cast<OGRGeometry *>(hGeom)->
                           getGeometryType()) )
    {
        OGRPoint* p = new OGRPoint();
        reinterpret_cast<OGRCurve *>(hGeom)->Value(dfDistance, p);
        return reinterpret_cast<OGRGeometryH>(p);
    }

    return NULL;
}

/************************************************************************/
/*                 OGRSetNonLinearGeometriesEnabledFlag()               */
/************************************************************************/

/**
 * \brief Set flag to enable/disable returning non-linear geometries in the C
 * API.
 *
 * This flag has only an effect on the OGR_F_GetGeometryRef(),
 * OGR_F_GetGeomFieldRef(), OGR_L_GetGeomType(), OGR_GFld_GetType() and
 * OGR_FD_GetGeomType() C API, and corresponding methods in the SWIG
 * bindings. It is meant as making it simple for applications using the OGR C
 * API not to have to deal with non-linear geometries, even if such geometries
 * might be returned by drivers. In which case, they will be transformed into
 * their closest linear geometry, by doing linear approximation, with
 * OGR_G_ForceTo().
 *
 * Libraries should generally *not* use that method, since that could interfere
 * with other libraries or applications.
 *
 * Note that it *does* not affect the behaviour of the C++ API.
 *
 * @param bFlag TRUE if non-linear geometries might be returned (default value).
 *              FALSE to ask for non-linear geometries to be approximated as
 *              linear geometries.
 *
 * @return a point or NULL.
 *
 * @since GDAL 2.0
 */

void OGRSetNonLinearGeometriesEnabledFlag( int bFlag )
{
    bNonLinearGeometriesEnabled = bFlag != FALSE;
}

/************************************************************************/
/*                 OGRGetNonLinearGeometriesEnabledFlag()               */
/************************************************************************/

/**
 * \brief Get flag to enable/disable returning non-linear geometries in the C
 * API.
 *
 * return TRUE if non-linear geometries might be returned (default value is
 * TRUE).
 *
 * @since GDAL 2.0
 * @see OGRSetNonLinearGeometriesEnabledFlag()
 */

int OGRGetNonLinearGeometriesEnabledFlag( void )
{
    return bNonLinearGeometriesEnabled;
}
