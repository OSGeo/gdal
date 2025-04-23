/******************************************************************************
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  The OGRSurface class.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 1999, Frank Warmerdam
 *
 * SPDX-License-Identifier: MIT
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
 * <a href="https://geographiclib.sourceforge.io/html/python/geodesics.html">Geodesics</a>
 * follow the shortest route on the surface of the ellipsoid.
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
/*                           get_Length()                               */
/************************************************************************/

/**
 * \fn double OGRSurface::get_Length() const;
 *
 * \brief Get the length of the surface.
 *
 * The length is computed as the sum of the lengths of all members
 * in this collection (including inner rings).
 *
 * @return the length of the geometry in meters.
 *
 * @see get_GeodesicLength() for an alternative method returning lengths
 * computed on the ellipsoid, and in meters.
 *
 * @since GDAL 3.10
 */

/************************************************************************/
/*                        get_GeodesicLength()                          */
/************************************************************************/

/**
 * \fn double OGRSurface::get_GeodesicLength(const OGRSpatialReference *poSRSOverride) const;
 *
 * \brief Get the length of the surface, where curve edges are geodesic lines
 * on the underlying ellipsoid of the SRS attached to the geometry.
 *
 * The returned length will always be in meters.
 *
 * Note that geometries with circular arcs will be linearized in their original
 * coordinate space first, so the resulting geodesic length will be an
 * approximation.
 *
 * The length is computed as the sum of the lengths of all members
 * in this collection (including inner rings).
 *
 * @note No warning will be issued if a member of the collection does not
 *       support the get_GeodesicLength method.
 *
 * @param poSRSOverride If not null, overrides OGRGeometry::getSpatialReference()
 * @return the length of the geometry in meters, or a negative value in case
 * of error.
 *
 * @see get_Length() for an alternative method returning lengths computed in
 * 2D Cartesian space.
 *
 * @since GDAL 3.10
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
