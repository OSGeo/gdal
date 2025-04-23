/******************************************************************************
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  The OGRTriangulatedSurface geometry class.
 * Author:   Avyav Kumar Singh <avyavkumar at gmail dot com>
 *
 ******************************************************************************
 * Copyright (c) 2016, Avyav Kumar Singh <avyavkumar at gmail dot com>
 * Copyright (c) 2016, Even Rouault <even.roauult at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include "ogr_geometry.h"
#include "ogr_p.h"
#include "ogr_api.h"

/************************************************************************/
/*        OGRTriangulatedSurface( const OGRTriangulatedSurface& )       */
/************************************************************************/

/**
 * \brief Copy constructor.
 *
 */

OGRTriangulatedSurface::OGRTriangulatedSurface(
    const OGRTriangulatedSurface &other)
    : OGRPolyhedralSurface()
{
    *this = other;
}

/************************************************************************/
/*                 operator=( const OGRTriangulatedSurface&)            */
/************************************************************************/

/**
 * \brief Assignment operator.
 *
 */

OGRTriangulatedSurface &
OGRTriangulatedSurface::operator=(const OGRTriangulatedSurface &other)
{
    if (this != &other)
    {
        // We need to do it manually. We cannot rely on the = operator
        // of OGRPolyhedralSurface since it will be confused by a multipolygon
        // of triangles.
        OGRSurface::operator=(other);
        for (const auto *poPoly : other.oMP)
        {
            OGRTriangulatedSurface::addGeometry(poPoly);
        }
    }
    return *this;
}

/************************************************************************/
/*                               clone()                                */
/************************************************************************/

OGRTriangulatedSurface *OGRTriangulatedSurface::clone() const

{
    return new (std::nothrow) OGRTriangulatedSurface(*this);
}

/************************************************************************/
/*                          getGeometryName()                           */
/************************************************************************/

/**
 * \brief Returns the geometry name of the TriangulatedSurface
 *
 * @return "TIN"
 *
 */

const char *OGRTriangulatedSurface::getGeometryName() const
{
    return "TIN";
}

/************************************************************************/
/*                          getGeometryType()                           */
/************************************************************************/

/**
 * \brief Returns the WKB Type of TriangulatedSurface
 *
 */

OGRwkbGeometryType OGRTriangulatedSurface::getGeometryType() const
{
    if ((flags & OGR_G_3D) && (flags & OGR_G_MEASURED))
        return wkbTINZM;
    else if (flags & OGR_G_MEASURED)
        return wkbTINM;
    else if (flags & OGR_G_3D)
        return wkbTINZ;
    else
        return wkbTIN;
}

/************************************************************************/
/*                         isCompatibleSubType()                        */
/************************************************************************/

//! @cond Doxygen_Suppress
OGRBoolean
OGRTriangulatedSurface::isCompatibleSubType(OGRwkbGeometryType eSubType) const
{
    return wkbFlatten(eSubType) == wkbTriangle;
}

//! @endcond

/************************************************************************/
/*                         getSubGeometryName()                         */
/************************************************************************/

//! @cond Doxygen_Suppress
const char *OGRTriangulatedSurface::getSubGeometryName() const
{
    return "TRIANGLE";
}

//! @endcond

/************************************************************************/
/*                         getSubGeometryType()                         */
/************************************************************************/

//! @cond Doxygen_Suppress
OGRwkbGeometryType OGRTriangulatedSurface::getSubGeometryType() const
{
    return wkbTriangle;
}

//! @endcond

/************************************************************************/
/*                            addGeometry()                             */
/************************************************************************/

OGRErr OGRTriangulatedSurface::addGeometry(const OGRGeometry *poNewGeom)
{
    // If the geometry is a polygon, check if it can be cast as a triangle
    if (EQUAL(poNewGeom->getGeometryName(), "POLYGON"))
    {
        OGRErr eErr = OGRERR_FAILURE;
        OGRTriangle *poTriangle =
            new OGRTriangle(*(poNewGeom->toPolygon()), eErr);
        if (poTriangle != nullptr && eErr == OGRERR_NONE)
        {
            eErr = addGeometryDirectly(poTriangle);

            if (eErr != OGRERR_NONE)
                delete poTriangle;

            return eErr;
        }
        else
        {
            delete poTriangle;
            return OGRERR_UNSUPPORTED_GEOMETRY_TYPE;
        }
    }

    return OGRPolyhedralSurface::addGeometry(poNewGeom);
}

/************************************************************************/
/*                     GetCasterToMultiPolygon()                        */
/************************************************************************/

//! @cond Doxygen_Suppress
OGRPolyhedralSurfaceCastToMultiPolygon
OGRTriangulatedSurface::GetCasterToMultiPolygon() const
{
    return OGRTriangulatedSurface::CastToMultiPolygonImpl;
}

/************************************************************************/
/*                         CastToMultiPolygon()                         */
/************************************************************************/

OGRMultiPolygon *
OGRTriangulatedSurface::CastToMultiPolygonImpl(OGRPolyhedralSurface *poTS)
{
    OGRMultiPolygon *poMultiPolygon = new OGRMultiPolygon();
    poMultiPolygon->assignSpatialReference(poTS->getSpatialReference());

    for (auto &&poSubGeom : *poTS)
    {
        OGRPolygon *poPolygon = OGRSurface::CastToPolygon(poSubGeom);
        poMultiPolygon->addGeometryDirectly(poPolygon);
        poSubGeom = nullptr;
    }
    delete poTS;

    return poMultiPolygon;
}

//! @endcond

/************************************************************************/
/*                     CastToPolyhedralSurface()                        */
/************************************************************************/

/**
 * \brief Casts the OGRTriangulatedSurface to an OGRPolyhedralSurface
 *
 * The passed in geometry is consumed and a new one returned (or NULL in case
 * of failure)
 *
 * @param poTS the input geometry - ownership is passed to the method.
 * @return new geometry.
 */

OGRPolyhedralSurface *
OGRTriangulatedSurface::CastToPolyhedralSurface(OGRTriangulatedSurface *poTS)
{
    OGRPolyhedralSurface *poPS = new OGRPolyhedralSurface();
    poPS->assignSpatialReference(poTS->getSpatialReference());
    for (auto &&poSubGeom : *poTS)
    {
        OGRPolygon *poPolygon = OGRSurface::CastToPolygon(poSubGeom);
        poPS->oMP.addGeometryDirectly(poPolygon);
        poSubGeom = nullptr;
    }
    delete poTS;
    return poPS;
}
