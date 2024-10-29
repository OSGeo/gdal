/******************************************************************************
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  The OGRCurve geometry class.
 * Author:   Frank Warmerdam, warmerda@home.com
 *
 ******************************************************************************
 * Copyright (c) 1999, Frank Warmerdam
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include "ogr_geometry.h"
#include "ogr_p.h"

//! @cond Doxygen_Suppress

/************************************************************************/
/*                       operator=( const OGRCurve& )                   */
/************************************************************************/

OGRCurve &OGRCurve::operator=(const OGRCurve &other)
{
    if (this != &other)
    {
        OGRGeometry::operator=(other);
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
    StartPoint(&oStartPoint);

    OGRPoint oEndPoint;
    EndPoint(&oEndPoint);

    if (oStartPoint.Is3D() && oEndPoint.Is3D())
    {
        // XYZ type
        if (oStartPoint.getX() == oEndPoint.getX() &&
            oStartPoint.getY() == oEndPoint.getY() &&
            oStartPoint.getZ() == oEndPoint.getZ())
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
        if (oStartPoint.getX() == oEndPoint.getX() &&
            oStartPoint.getY() == oEndPoint.getY())
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
 *
 * @see get_GeodesicLength() for an alternative method returning lengths
 * computed on the ellipsoid, and in meters.
 */

/**
 * \fn double OGRCurve::get_GeodesicLength(const OGRSpatialReference* poSRSOverride = nullptr) const;
 *
 * \brief Get the length of the curve, considered as a geodesic line on the
 * underlying ellipsoid of the SRS attached to the geometry.
 *
 * The returned length will always be in meters.
 *
 * <a href="https://geographiclib.sourceforge.io/html/python/geodesics.html">Geodesics</a>
 * follow the shortest route on the surface of the ellipsoid.
 *
 * If the geometry' SRS is not a geographic one, geometries are reprojected to
 * the underlying geographic SRS of the geometry' SRS.
 * OGRSpatialReference::GetDataAxisToSRSAxisMapping() is honored.
 *
 * Note that geometries with circular arcs will be linearized in their original
 * coordinate space first, so the resulting geodesic length will be an
 * approximation.
 *
 * @param poSRSOverride If not null, overrides OGRGeometry::getSpatialReference()
 * @return the length of the geometry in meters, or a negative value in case
 * of error.
 *
 * @see get_Length() for an alternative method returning areas computed in
 * 2D Cartesian space.
 *
 * @since GDAL 3.10
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
 * The returned geometry is a new instance whose ownership belongs to the
 * caller.
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
 * @return the area of the geometry in square units of the spatial reference
 * system in use.
 *
 * @see get_GeodesicArea() for an alternative method returning areas
 * computed on the ellipsoid, and in square meters.
 *
 * @since GDAL 2.0
 */

/**
 * \fn double OGRCurve::get_GeodesicArea(const OGRSpatialReference* poSRSOverride = nullptr) const;
 *
 * \brief Get the area of the (closed) curve, considered as a surface on the
 * underlying ellipsoid of the SRS attached to the geometry.
 *
 * This method is designed to be used by OGRCurvePolygon::get_GeodesicArea().
 *
 * The returned area will always be in square meters, and assumes that
 * polygon edges describe geodesic lines on the ellipsoid.
 *
 * <a href="https://geographiclib.sourceforge.io/html/python/geodesics.html">Geodesics</a>
 * follow the shortest route on the surface of the ellipsoid.
 *
 * If the geometry' SRS is not a geographic one, geometries are reprojected to
 * the underlying geographic SRS of the geometry' SRS.
 * OGRSpatialReference::GetDataAxisToSRSAxisMapping() is honored.
 *
 * Note that geometries with circular arcs will be linearized in their original
 * coordinate space first, so the resulting geodesic area will be an
 * approximation.
 *
 * @param poSRSOverride If not null, overrides OGRGeometry::getSpatialReference()
 * @return the area of the geometry in square meters, or a negative value in case
 * of error.
 *
 * @see get_Area() for an alternative method returning areas computed in
 * 2D Cartesian space.
 *
 * @since GDAL 3.9
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
    OGRPointIterator *poPointIter = getPointIterator();
    OGRPoint p1;
    OGRPoint p2;
    if (poPointIter->getNextPoint(&p1) && poPointIter->getNextPoint(&p2))
    {
        OGRPoint p3;
        while (poPointIter->getNextPoint(&p3))
        {
            const double crossproduct =
                (p2.getX() - p1.getX()) * (p3.getY() - p2.getY()) -
                (p2.getY() - p1.getY()) * (p3.getX() - p2.getX());
            if (crossproduct > 0)
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

OGRCompoundCurve *OGRCurve::CastToCompoundCurve(OGRCurve *poCurve)
{
    OGRCompoundCurve *poCC = new OGRCompoundCurve();
    if (wkbFlatten(poCurve->getGeometryType()) == wkbLineString)
        poCurve = CastToLineString(poCurve);
    if (!poCurve->IsEmpty() && poCC->addCurveDirectly(poCurve) != OGRERR_NONE)
    {
        delete poCC;
        delete poCurve;
        return nullptr;
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

OGRLineString *OGRCurve::CastToLineString(OGRCurve *poCurve)
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

OGRLinearRing *OGRCurve::CastToLinearRing(OGRCurve *poCurve)
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

int OGRCurve::ContainsPoint(CPL_UNUSED const OGRPoint *p) const
{
    return -1;
}

/************************************************************************/
/*                         IntersectsPoint()                            */
/************************************************************************/

/**
 * \brief Returns if a point intersects a (closed) curve.
 *
 * Final users should use OGRGeometry::Intersects() instead.
 *
 * @param p the point to test
 * @return TRUE if it intersects the curve, FALSE otherwise or -1 if unknown.
 *
 * @since GDAL 2.3
 */

int OGRCurve::IntersectsPoint(CPL_UNUSED const OGRPoint *p) const
{
    return -1;
}

/************************************************************************/
/*                          ~OGRPointIterator()                         */
/************************************************************************/

OGRPointIterator::~OGRPointIterator() = default;

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
void OGRPointIterator::destroy(OGRPointIterator *poIter)
{
    delete poIter;
}

/************************************************************************/
/*                     OGRSimpleCurve::Iterator                         */
/************************************************************************/

void OGRIteratedPoint::setX(double xIn)
{
    OGRPoint::setX(xIn);
    m_poCurve->setPoint(m_nPos, xIn, getY());
}

void OGRIteratedPoint::setY(double yIn)
{
    OGRPoint::setY(yIn);
    m_poCurve->setPoint(m_nPos, getX(), yIn);
}

void OGRIteratedPoint::setZ(double zIn)
{
    OGRPoint::setZ(zIn);
    m_poCurve->setZ(m_nPos, zIn);
}

void OGRIteratedPoint::setM(double mIn)
{
    OGRPoint::setM(mIn);
    m_poCurve->setM(m_nPos, mIn);
}

struct OGRSimpleCurve::Iterator::Private
{
    CPL_DISALLOW_COPY_ASSIGN(Private)
    Private() = default;

    bool m_bUpdateChecked = true;
    OGRIteratedPoint m_oPoint{};
};

void OGRSimpleCurve::Iterator::update()
{
    if (!m_poPrivate->m_bUpdateChecked)
    {
        OGRPoint oPointBefore;
        m_poPrivate->m_oPoint.m_poCurve->getPoint(m_poPrivate->m_oPoint.m_nPos,
                                                  &oPointBefore);
        if (oPointBefore != m_poPrivate->m_oPoint)
        {
            if (m_poPrivate->m_oPoint.Is3D())
                m_poPrivate->m_oPoint.m_poCurve->set3D(true);
            if (m_poPrivate->m_oPoint.IsMeasured())
                m_poPrivate->m_oPoint.m_poCurve->setMeasured(true);
            m_poPrivate->m_oPoint.m_poCurve->setPoint(
                m_poPrivate->m_oPoint.m_nPos, &m_poPrivate->m_oPoint);
        }
        m_poPrivate->m_bUpdateChecked = true;
    }
}

OGRSimpleCurve::Iterator::Iterator(OGRSimpleCurve *poSelf, int nPos)
    : m_poPrivate(new Private())
{
    m_poPrivate->m_oPoint.m_poCurve = poSelf;
    m_poPrivate->m_oPoint.m_nPos = nPos;
}

OGRSimpleCurve::Iterator::~Iterator()
{
    update();
}

OGRIteratedPoint &OGRSimpleCurve::Iterator::operator*()
{
    update();
    m_poPrivate->m_oPoint.m_poCurve->getPoint(m_poPrivate->m_oPoint.m_nPos,
                                              &m_poPrivate->m_oPoint);
    m_poPrivate->m_bUpdateChecked = false;
    return m_poPrivate->m_oPoint;
}

OGRSimpleCurve::Iterator &OGRSimpleCurve::Iterator::operator++()
{
    update();
    ++m_poPrivate->m_oPoint.m_nPos;
    return *this;
}

bool OGRSimpleCurve::Iterator::operator!=(const Iterator &it) const
{
    return m_poPrivate->m_oPoint.m_nPos != it.m_poPrivate->m_oPoint.m_nPos;
}

OGRSimpleCurve::Iterator OGRSimpleCurve::begin()
{
    return {this, 0};
}

OGRSimpleCurve::Iterator OGRSimpleCurve::end()
{
    return {this, nPointCount};
}

/************************************************************************/
/*                  OGRSimpleCurve::ConstIterator                       */
/************************************************************************/

struct OGRSimpleCurve::ConstIterator::Private
{
    CPL_DISALLOW_COPY_ASSIGN(Private)
    Private() = default;

    mutable OGRIteratedPoint m_oPoint{};
    const OGRSimpleCurve *m_poSelf = nullptr;
    int m_nPos = 0;
};

OGRSimpleCurve::ConstIterator::ConstIterator(const OGRSimpleCurve *poSelf,
                                             int nPos)
    : m_poPrivate(new Private())
{
    m_poPrivate->m_poSelf = poSelf;
    m_poPrivate->m_nPos = nPos;
}

OGRSimpleCurve::ConstIterator::~ConstIterator() = default;

const OGRPoint &OGRSimpleCurve::ConstIterator::operator*() const
{
    m_poPrivate->m_poSelf->getPoint(m_poPrivate->m_nPos,
                                    &m_poPrivate->m_oPoint);
    return m_poPrivate->m_oPoint;
}

OGRSimpleCurve::ConstIterator &OGRSimpleCurve::ConstIterator::operator++()
{
    ++m_poPrivate->m_nPos;
    return *this;
}

bool OGRSimpleCurve::ConstIterator::operator!=(const ConstIterator &it) const
{
    return m_poPrivate->m_nPos != it.m_poPrivate->m_nPos;
}

OGRSimpleCurve::ConstIterator OGRSimpleCurve::begin() const
{
    return {this, 0};
}

OGRSimpleCurve::ConstIterator OGRSimpleCurve::end() const
{
    return {this, nPointCount};
}

/************************************************************************/
/*                     OGRCurve::ConstIterator                          */
/************************************************************************/

struct OGRCurve::ConstIterator::Private
{
    CPL_DISALLOW_COPY_ASSIGN(Private)
    Private() = default;
    Private(Private &&) = delete;
    Private &operator=(Private &&) = default;

    OGRPoint m_oPoint{};
    const OGRCurve *m_poCurve{};
    int m_nStep = 0;
    std::unique_ptr<OGRPointIterator> m_poIterator{};
};

OGRCurve::ConstIterator::ConstIterator(const OGRCurve *poSelf, bool bStart)
    : m_poPrivate(new Private())
{
    m_poPrivate->m_poCurve = poSelf;
    if (bStart)
    {
        m_poPrivate->m_poIterator.reset(poSelf->getPointIterator());
        if (!m_poPrivate->m_poIterator->getNextPoint(&m_poPrivate->m_oPoint))
        {
            m_poPrivate->m_nStep = -1;
            m_poPrivate->m_poIterator.reset();
        }
    }
    else
    {
        m_poPrivate->m_nStep = -1;
    }
}

OGRCurve::ConstIterator::ConstIterator(ConstIterator &&oOther) noexcept
    : m_poPrivate(std::move(oOther.m_poPrivate))
{
}

OGRCurve::ConstIterator &
OGRCurve::ConstIterator::operator=(ConstIterator &&oOther)
{
    m_poPrivate = std::move(oOther.m_poPrivate);
    return *this;
}

OGRCurve::ConstIterator::~ConstIterator() = default;

const OGRPoint &OGRCurve::ConstIterator::operator*() const
{
    return m_poPrivate->m_oPoint;
}

OGRCurve::ConstIterator &OGRCurve::ConstIterator::operator++()
{
    CPLAssert(m_poPrivate->m_nStep >= 0);
    ++m_poPrivate->m_nStep;
    if (!m_poPrivate->m_poIterator->getNextPoint(&m_poPrivate->m_oPoint))
    {
        m_poPrivate->m_nStep = -1;
        m_poPrivate->m_poIterator.reset();
    }
    return *this;
}

bool OGRCurve::ConstIterator::operator!=(const ConstIterator &it) const
{
    return m_poPrivate->m_poCurve != it.m_poPrivate->m_poCurve ||
           m_poPrivate->m_nStep != it.m_poPrivate->m_nStep;
}

OGRCurve::ConstIterator OGRCurve::begin() const
{
    return {this, true};
}

OGRCurve::ConstIterator OGRCurve::end() const
{
    return {this, false};
}

/************************************************************************/
/*                            isClockwise()                             */
/************************************************************************/

/**
 * \brief Returns TRUE if the ring has clockwise winding (or less than 2 points)
 *
 * Assumes that the line is closed.
 *
 * @return TRUE if clockwise otherwise FALSE.
 */

int OGRCurve::isClockwise() const

{
    // WARNING: keep in sync OGRLineString::isClockwise(),
    // OGRCurve::isClockwise() and OGRWKBIsClockwiseRing()

    const int nPointCount = getNumPoints();
    if (nPointCount < 3)
        return TRUE;

    bool bUseFallback = false;

    // Find the lowest rightmost vertex.
    auto oIter = begin();
    const OGRPoint oStartPoint = *oIter;
    OGRPoint oPointBefore = oStartPoint;
    OGRPoint oPointBeforeSel;
    OGRPoint oPointSel = oStartPoint;
    OGRPoint oPointNextSel;
    bool bNextPointIsNextSel = true;
    int v = 0;

    for (int i = 1; i < nPointCount - 1; i++)
    {
        ++oIter;
        OGRPoint oPointCur = *oIter;
        if (bNextPointIsNextSel)
        {
            oPointNextSel = oPointCur;
            bNextPointIsNextSel = false;
        }
        if (oPointCur.getY() < oPointSel.getY() ||
            (oPointCur.getY() == oPointSel.getY() &&
             oPointCur.getX() > oPointSel.getX()))
        {
            v = i;
            oPointBeforeSel = oPointBefore;
            oPointSel = oPointCur;
            bUseFallback = false;
            bNextPointIsNextSel = true;
        }
        else if (oPointCur.getY() == oPointSel.getY() &&
                 oPointCur.getX() == oPointSel.getX())
        {
            // Two vertex with same coordinates are the lowest rightmost
            // vertex.  Cannot use that point as the pivot (#5342).
            bUseFallback = true;
        }
        oPointBefore = oPointCur;
    }
    const OGRPoint oPointN_m2 = *oIter;

    if (bNextPointIsNextSel)
    {
        oPointNextSel = oPointN_m2;
    }

    // Previous.
    if (v == 0)
    {
        oPointBeforeSel = oPointN_m2;
    }

    constexpr double EPSILON = 1.0E-5;
    const auto epsilonEqual = [](double a, double b, double eps)
    { return ::fabs(a - b) < eps; };

    if (epsilonEqual(oPointBeforeSel.getX(), oPointSel.getX(), EPSILON) &&
        epsilonEqual(oPointBeforeSel.getY(), oPointSel.getY(), EPSILON))
    {
        // Don't try to be too clever by retrying with a next point.
        // This can lead to false results as in the case of #3356.
        bUseFallback = true;
    }

    const double dx0 = oPointBeforeSel.getX() - oPointSel.getX();
    const double dy0 = oPointBeforeSel.getY() - oPointSel.getY();

    // Following.
    if (v + 1 >= nPointCount - 1)
    {
        oPointNextSel = oStartPoint;
    }

    if (epsilonEqual(oPointNextSel.getX(), oPointSel.getX(), EPSILON) &&
        epsilonEqual(oPointNextSel.getY(), oPointSel.getY(), EPSILON))
    {
        // Don't try to be too clever by retrying with a next point.
        // This can lead to false results as in the case of #3356.
        bUseFallback = true;
    }

    const double dx1 = oPointNextSel.getX() - oPointSel.getX();
    const double dy1 = oPointNextSel.getY() - oPointSel.getY();

    const double crossproduct = dx1 * dy0 - dx0 * dy1;

    if (!bUseFallback)
    {
        if (crossproduct > 0)  // CCW
            return FALSE;
        else if (crossproduct < 0)  // CW
            return TRUE;
    }

    // This is a degenerate case: the extent of the polygon is less than EPSILON
    // or 2 nearly identical points were found.
    // Try with Green Formula as a fallback, but this is not a guarantee
    // as we'll probably be affected by numerical instabilities.
    oIter = begin();
    oPointBefore = oStartPoint;
    ++oIter;
    auto oPointCur = *oIter;
    double dfSum = oStartPoint.getX() * (oPointCur.getY() - oStartPoint.getY());

    for (int i = 1; i < nPointCount - 1; i++)
    {
        ++oIter;
        const auto &oPointNext = *oIter;
        dfSum += oPointCur.getX() * (oPointNext.getY() - oPointBefore.getY());
        oPointBefore = oPointCur;
        oPointCur = oPointNext;
    }

    dfSum += oPointCur.getX() * (oStartPoint.getY() - oPointBefore.getY());

    return dfSum < 0;
}

/**
 * \fn void OGRCurve::reversePoints();
 *
 * \brief Reverse point order.
 *
 * This method updates the points in this curve in place
 * reversing the point ordering (first for last, etc) and component ordering
 * for a compound curve.
 *
 * @since 3.10
 */
