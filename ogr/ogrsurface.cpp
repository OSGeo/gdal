/******************************************************************************
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  The OGRSurface class.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
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

#include "cpl_port.h"
#include "ogr_geometry.h"
#include "ogr_p.h"

/**
 * \fn double OGRSurface::get_Area() const;
 *
 * \brief Get the area of the surface object.
 *
 * The returned area is a 2D Cartesian (planar) area in square units of the
 * spatial reference system in use, so potentially "square degrees" for a
 * geometry expressed in a geographic SRS.
 *
 * For polygons the area is computed as the area of the outer ring less
 * the area of all internal rings.
 *
 * This method relates to the SFCOM ISurface::get_Area() method.
 *
 * @return the area of the geometry in square units of the spatial reference
 * system in use.
 *
 * @see get_GeodesicArea() for an alternative method returning areas
 * computed on the ellipsoid, an in square meters.
 */

/**
 * \fn double OGRSurface::get_GeodesicArea(const OGRSpatialReference* poSRSOverride = nullptr) const;
 *
 * \brief Get the area of the surface object, considered as a surface on the
 * underlying ellipsoid of the SRS attached to the geometry.
 *
 * The returned area will always be in square meters, and assumes that
 * polygon edges describe geodesic lines on the ellipsoid.
 *
 * If the geometry' SRS is not a geographic one, geometries are reprojected to
 * the underlying geographic SRS of the geometry' SRS.
 * OGRSpatialReference::GetDataAxisToSRSAxisMapping() is honored.
 *
 * For polygons the area is computed as the area of the outer ring less
 * the area of all internal rings.
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
 * \fn OGRErr OGRSurface::PointOnSurface( OGRPoint * poPoint ) const;
 *
 * \brief This method relates to the SFCOM
 * ISurface::get_PointOnSurface() method.
 *
 * NOTE: Only implemented when GEOS included in build.
 *
 * @param poPoint point to be set with an internal point.
 *
 * @return OGRERR_NONE if it succeeds or OGRERR_FAILURE otherwise.
 */

/************************************************************************/
/*                          CastToPolygon()                             */
/************************************************************************/

/*! @cond Doxygen_Suppress */
/**
 * \brief Cast to polygon
 *
 * The passed in geometry is consumed and a new one returned (or NULL in case
 * of failure)
 *
 * @param poSurface the input geometry - ownership is passed to the method.
 * @return new geometry.
 */

OGRPolygon *OGRSurface::CastToPolygon(OGRSurface *poSurface)
{
    OGRSurfaceCasterToPolygon pfn = poSurface->GetCasterToPolygon();
    return pfn(poSurface);
}

/************************************************************************/
/*                          CastToCurvePolygon()                        */
/************************************************************************/

/**
 * \brief Cast to curve polygon
 *
 * The passed in geometry is consumed and a new one returned (or NULL in case
 * of failure)
 *
 * @param poSurface the input geometry - ownership is passed to the method.
 * @return new geometry.
 */

OGRCurvePolygon *OGRSurface::CastToCurvePolygon(OGRSurface *poSurface)
{
    OGRSurfaceCasterToCurvePolygon pfn = poSurface->GetCasterToCurvePolygon();
    return pfn(poSurface);
}

/*! @endcond */
