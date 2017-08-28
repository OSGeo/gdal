/******************************************************************************
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  The OGRCurve geometry class.
 * Author:   Frank Warmerdam, warmerda@home.com
 *
 ******************************************************************************
 * Copyright (c) 1999, Frank Warmerdam
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
#include "ogr_p.h"

CPL_CVSID("$Id$")

//! @cond Doxygen_Suppress

/************************************************************************/
/*                                OGRCurve()                            */
/************************************************************************/

OGRCurve::OGRCurve() {}

/************************************************************************/
/*                               ~OGRCurve()                            */
/************************************************************************/

OGRCurve::~OGRCurve() {}

/************************************************************************/
/*                       OGRCurve( const OGRCurve& )                    */
/************************************************************************/

OGRCurve::OGRCurve( const OGRCurve& other ) :
    OGRGeometry( other )
{}

/************************************************************************/
/*                       operator=( const OGRCurve& )                   */
/************************************************************************/

OGRCurve& OGRCurve::operator=( const OGRCurve& other )
{
    if( this != &other)
    {
        OGRGeometry::operator=( other );
    }
    return *this;
}
//! @endcond

/************************************************************************/
/*                            getDimension()                            */
/************************************************************************/

int OGRCurve::getDimension() const

{
    return 1;
}

/************************************************************************/
/*                            get_IsClosed()                            */
/************************************************************************/

/**
 * \brief Return TRUE if curve is closed.
 *
 * Tests if a curve is closed. A curve is closed if its start point is
 * equal to its end point.
 *
 * For equality tests, the M dimension is ignored.
 *
 * This method relates to the SFCOM ICurve::get_IsClosed() method.
 *
 * @return TRUE if closed, else FALSE.
 */

int OGRCurve::get_IsClosed() const

{
    OGRPoint oStartPoint;
    StartPoint( &oStartPoint );

    OGRPoint oEndPoint;
    EndPoint( &oEndPoint );

    if (oStartPoint.Is3D() && oEndPoint.Is3D())
    {
        // XYZ type
        if( oStartPoint.getX() == oEndPoint.getX() && oStartPoint.getY() == oEndPoint.getY()
            && oStartPoint.getZ() == oEndPoint.getZ())
        {
            return TRUE;
        }
        else
            return FALSE;
    }

    // one of the points is 3D
    else if (((oStartPoint.Is3D() & oEndPoint.Is3D()) == 0) &&
             ((oStartPoint.Is3D() | oEndPoint.Is3D()) == 1))
    {
        return FALSE;
    }

    else
    {
        // XY type
        if( oStartPoint.getX() == oEndPoint.getX() && oStartPoint.getY() == oEndPoint.getY() )
            return TRUE;
        else
            return FALSE;
    }
}

/**
 * \fn double OGRCurve::get_Length() const;
 *
 * \brief Returns the length of the curve.
 *
 * This method relates to the SFCOM ICurve::get_Length() method.
 *
 * @return the length of the curve, zero if the curve hasn't been
 * initialized.
 */

/**
 * \fn void OGRCurve::StartPoint( OGRPoint * poPoint ) const;
 *
 * \brief Return the curve start point.
 *
 * This method relates to the SF COM ICurve::get_StartPoint() method.
 *
 * @param poPoint the point to be assigned the start location.
 */

/**
 * \fn void OGRCurve::EndPoint( OGRPoint * poPoint ) const;
 *
 * \brief Return the curve end point.
 *
 * This method relates to the SF COM ICurve::get_EndPoint() method.
 *
 * @param poPoint the point to be assigned the end location.
 */

/**
 * \fn void OGRCurve::Value( double dfDistance, OGRPoint * poPoint ) const;
 *
 * \brief Fetch point at given distance along curve.
 *
 * This method relates to the SF COM ICurve::get_Value() method.
 *
 * This function is the same as the C function OGR_G_Value().
 *
 * @param dfDistance distance along the curve at which to sample position.
 *                   This distance should be between zero and get_Length()
 *                   for this curve.
 * @param poPoint the point to be assigned the curve position.
 */

/**
 * \fn OGRLineString* OGRCurve::CurveToLine( double dfMaxAngleStepSizeDegrees,
 *     const char* const* papszOptions ) const;
 *
 * \brief Return a linestring from a curve geometry.
 *
 * The returned geometry is a new instance whose ownership belongs to the caller.
 *
 * If the dfMaxAngleStepSizeDegrees is zero, then a default value will be
 * used.  This is currently 4 degrees unless the user has overridden the
 * value with the OGR_ARC_STEPSIZE configuration variable.
 *
 * This method relates to the ISO SQL/MM Part 3 ICurve::CurveToLine() method.
 *
 * This function is the same as C function OGR_G_CurveToLine().
 *
 * @param dfMaxAngleStepSizeDegrees the largest step in degrees along the
 * arc, zero to use the default setting.
 * @param papszOptions options as a null-terminated list of strings or NULL.
 *                     See OGRGeometryFactory::curveToLineString() for valid
 *                     options.
 *
 * @return a line string approximating the curve
 *
 * @since GDAL 2.0
 */

/**
 * \fn int OGRCurve::getNumPoints() const;
 *
 * \brief Return the number of points of a curve geometry.
 *
 *
 * This method, as a method of OGRCurve, does not relate to a standard.
 * For circular strings or linestrings, it returns the number of points,
 * conforming to SF COM NumPoints().
 * For compound curves, it returns the sum of the number of points of each
 * of its components (non including intermediate starting/ending points of
 * the different parts).
 *
 * @return the number of points of the curve.
 *
 * @since GDAL 2.0
 */

/**
 * \fn OGRPointIterator* OGRCurve::getPointIterator() const;
 *
 * \brief Returns a point iterator over the curve.
 *
 * The curve must not be modified while an iterator exists on it.
 *
 * The iterator must be destroyed with OGRPointIterator::destroy().
 *
 * @return a point iterator over the curve.
 *
 * @since GDAL 2.0
 */

/**
 * \fn double OGRCurve::get_Area() const;
 *
 * \brief Get the area of the (closed) curve.
 *
 * This method is designed to be used by OGRCurvePolygon::get_Area().
 *
 * @return the area of the feature in square units of the spatial reference
 * system in use.
 *
 * @since GDAL 2.0
 */

/**
 * \fn double OGRCurve::get_AreaOfCurveSegments() const;
 *
 * \brief Get the area of the purely curve portions of a (closed) curve.
 *
 * This method is designed to be used on a closed convex curve.
 *
 * @return the area of the feature in square units of the spatial reference
 * system in use.
 *
 * @since GDAL 2.0
 */

/************************************************************************/
/*                              IsConvex()                              */
/************************************************************************/

/**
 * \brief Returns if a (closed) curve forms a convex shape.
 *
 * @return TRUE if the curve forms a convex shape.
 *
 * @since GDAL 2.0
 */

OGRBoolean OGRCurve::IsConvex() const
{
    bool bRet = true;
    OGRPointIterator* poPointIter = getPointIterator();
    OGRPoint p1;
    OGRPoint p2;
    if( poPointIter->getNextPoint(&p1) &&
        poPointIter->getNextPoint(&p2) )
    {
        OGRPoint p3;
        while( poPointIter->getNextPoint(&p3) )
        {
            const double crossproduct =
                (p2.getX() - p1.getX()) * (p3.getY() - p2.getY()) -
                (p2.getY() - p1.getY()) * (p3.getX() - p2.getX());
            if( crossproduct > 0 )
            {
                bRet = false;
                break;
            }
            p1.setX(p2.getX());
            p1.setY(p2.getY());
            p2.setX(p3.getX());
            p2.setY(p3.getY());
        }
    }
    delete poPointIter;
    return bRet;
}

/************************************************************************/
/*                          CastToCompoundCurve()                       */
/************************************************************************/

/**
 * \brief Cast to compound curve
 *
 * The passed in geometry is consumed and a new one returned (or NULL in case
 * of failure)
 *
 * @param poCurve the input geometry - ownership is passed to the method.
 * @return new geometry
 *
 * @since GDAL 2.0
 */

OGRCompoundCurve* OGRCurve::CastToCompoundCurve( OGRCurve* poCurve )
{
    OGRCompoundCurve* poCC = new OGRCompoundCurve();
    if( poCurve->getGeometryType() == wkbLineString )
        poCurve = CastToLineString(poCurve);
    if( !poCurve->IsEmpty() && poCC->addCurveDirectly(poCurve) != OGRERR_NONE )
    {
        delete poCC;
        delete poCurve;
        return NULL;
    }
    poCC->assignSpatialReference(poCurve->getSpatialReference());
    return poCC;
}

/************************************************************************/
/*                          CastToLineString()                          */
/************************************************************************/

/**
 * \brief Cast to linestring
 *
 * The passed in geometry is consumed and a new one returned (or NULL in case
 * of failure)
 *
 * @param poCurve the input geometry - ownership is passed to the method.
 * @return new geometry.
 *
 * @since GDAL 2.0
 */

OGRLineString* OGRCurve::CastToLineString( OGRCurve* poCurve )
{
    OGRCurveCasterToLineString pfn = poCurve->GetCasterToLineString();
    return pfn(poCurve);
}

/************************************************************************/
/*                          CastToLinearRing()                          */
/************************************************************************/

/**
 * \brief Cast to linear ring
 *
 * The passed in geometry is consumed and a new one returned (or NULL in case
 * of failure)
 *
 * @param poCurve the input geometry - ownership is passed to the method.
 * @return new geometry.
 *
 * @since GDAL 2.0
 */

OGRLinearRing* OGRCurve::CastToLinearRing( OGRCurve* poCurve )
{
    OGRCurveCasterToLinearRing pfn = poCurve->GetCasterToLinearRing();
    return pfn(poCurve);
}

/************************************************************************/
/*                           ContainsPoint()                            */
/************************************************************************/

/**
 * \brief Returns if a point is contained in a (closed) curve.
 *
 * Final users should use OGRGeometry::Contains() instead.
 *
 * @param p the point to test
 * @return TRUE if it is inside the curve, FALSE otherwise or -1 if unknown.
 *
 * @since GDAL 2.0
 */

int OGRCurve::ContainsPoint( const OGRPoint* /* p */ ) const
{
    return -1;
}

/************************************************************************/
/*                          ~OGRPointIterator()                         */
/************************************************************************/

OGRPointIterator::~OGRPointIterator() {}

/**
 * \fn OGRBoolean OGRPointIterator::getNextPoint(OGRPoint* p);
 *
 * \brief Returns the next point followed by the iterator.
 *
 * @param p point to fill.
 *
 * @return TRUE in case of success, or FALSE if the end of the curve is reached.
 *
 * @since GDAL 2.0
 */

/************************************************************************/
/*                              destroy()                               */
/************************************************************************/

/**
 * \brief Destroys a point iterator.
 *
 * @since GDAL 2.0
 */
void OGRPointIterator::destroy( OGRPointIterator* poIter )
{
    delete poIter;
}
