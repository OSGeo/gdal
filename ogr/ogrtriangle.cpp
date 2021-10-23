/******************************************************************************
 * $Id$
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  The OGRTriangle geometry class.
 * Author:   Avyav Kumar Singh <avyavkumar at gmail dot com>
 *
 ******************************************************************************
 * Copyright (c) 2016, Avyav Kumar Singh <avyavkumar at gmail dot com>
 * Copyright (c) 2016, Even Rouault <even.roauult at spatialys.com>
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

CPL_CVSID("$Id$")

/************************************************************************/
/*                             OGRTriangle()                            */
/************************************************************************/

/**
 * \brief Constructor.
 *
 */

OGRTriangle::OGRTriangle() = default;

/************************************************************************/
/*                             OGRTriangle()                            */
/************************************************************************/

/**
 * \brief Copy constructor.
 *
 */

OGRTriangle::OGRTriangle(const OGRTriangle&) = default;

/************************************************************************/
/*                             OGRTriangle()                            */
/************************************************************************/

/**
 * \brief Constructs an OGRTriangle from a valid OGRPolygon. In case of error,
 * NULL is returned.
 *
 * @param other the Polygon we wish to construct a triangle from
 * @param eErr encapsulates an error code; contains OGRERR_NONE if the triangle
 * is constructed successfully
 */

OGRTriangle::OGRTriangle(const OGRPolygon& other, OGRErr &eErr)
{
    // In case of Polygon, we have to check that it is a valid triangle -
    // closed and contains one external ring of four points
    // If not, then eErr will contain the error description
    const OGRCurve *poCurve = other.getExteriorRingCurve();
    if (other.getNumInteriorRings() == 0 &&
        poCurve != nullptr && poCurve->get_IsClosed() &&
        poCurve->getNumPoints() == 4)
    {
        // everything is fine
        eErr = addRing( const_cast<OGRCurve*>(poCurve) );
        if (eErr != OGRERR_NONE)
            CPLError( CE_Failure, CPLE_NotSupported, "Invalid Triangle");
    }
    assignSpatialReference( other.getSpatialReference() );
}

/************************************************************************/
/*                             OGRTriangle()                            */
/************************************************************************/

/**
 * \brief Construct a triangle from points
 *
 * @param p Point 1
 * @param q Point 2
 * @param r Point 3
 */

OGRTriangle::OGRTriangle(const OGRPoint &p, const OGRPoint &q,
                         const OGRPoint &r)
{
    OGRLinearRing *poCurve = new OGRLinearRing();
    poCurve->addPoint(&p);
    poCurve->addPoint(&q);
    poCurve->addPoint(&r);
    poCurve->addPoint(&p);

    oCC.addCurveDirectly(this, poCurve, TRUE);
}

/************************************************************************/
/*                             ~OGRTriangle()                            */
/************************************************************************/

/**
 * \brief Destructor
 *
 */

OGRTriangle::~OGRTriangle() = default;

/************************************************************************/
/*                    operator=( const OGRGeometry&)                    */
/************************************************************************/

/**
 * \brief Assignment operator
 *
 * @param other A triangle passed as a parameter
 *
 * @return OGRTriangle A copy of other
 *
 */

OGRTriangle& OGRTriangle::operator=( const OGRTriangle& other )
{
    if( this != &other)
    {
        OGRPolygon::operator=( other );
    }
    return *this;
}

/************************************************************************/
/*                               clone()                                */
/************************************************************************/

OGRTriangle *OGRTriangle::clone() const

{
    return new (std::nothrow) OGRTriangle(*this);
}

/************************************************************************/
/*                          getGeometryName()                           */
/************************************************************************/

const char* OGRTriangle::getGeometryName() const
{
    return "TRIANGLE";
}

/************************************************************************/
/*                          getGeometryType()                           */
/************************************************************************/

OGRwkbGeometryType OGRTriangle::getGeometryType() const
{
    if( (flags & OGR_G_3D) && (flags & OGR_G_MEASURED) )
        return wkbTriangleZM;
    else if( flags & OGR_G_MEASURED  )
        return wkbTriangleM;
    else if( flags & OGR_G_3D )
        return wkbTriangleZ;
    else
        return wkbTriangle;
}

/************************************************************************/
/*                        quickValidityCheck()                          */
/************************************************************************/

bool OGRTriangle::quickValidityCheck() const
{
    return oCC.nCurveCount == 0 ||
           (oCC.nCurveCount == 1 &&
            oCC.papoCurves[0]->getNumPoints() == 4 &&
            oCC.papoCurves[0]->get_IsClosed());
}

/************************************************************************/
/*                           importFromWkb()                            */
/************************************************************************/

OGRErr OGRTriangle::importFromWkb( const unsigned char *pabyData,
                                   size_t nSize,
                                   OGRwkbVariant eWkbVariant,
                                   size_t& nBytesConsumedOut )
{
    OGRErr eErr = OGRPolygon::importFromWkb( pabyData, nSize, eWkbVariant,
                                             nBytesConsumedOut );
    if( eErr != OGRERR_NONE )
        return eErr;

    if ( !quickValidityCheck() )
    {
        CPLDebug("OGR", "Triangle is not made of a closed rings of 3 points");
        empty();
        return OGRERR_CORRUPT_DATA;
    }

    return OGRERR_NONE;
}

/*! @cond Doxygen_Suppress */
/************************************************************************/
/*                        importFromWKTListOnly()                       */
/*                                                                      */
/*      Instantiate from "((x y, x y, ...),(x y, ...),...)"             */
/************************************************************************/

OGRErr OGRTriangle::importFromWKTListOnly( const char ** ppszInput,
                                          int bHasZ, int bHasM,
                                          OGRRawPoint*& paoPoints,
                                          int& nMaxPoints,
                                          double*& padfZ )

{
    OGRErr eErr = OGRPolygon::importFromWKTListOnly(ppszInput,
                                                    bHasZ, bHasM,
                                                    paoPoints,
                                                    nMaxPoints,
                                                    padfZ );
    if( eErr == OGRERR_NONE )
    {
        if( !quickValidityCheck() )
        {
            CPLDebug("OGR",
                     "Triangle is not made of a closed rings of 3 points");
            empty();
            eErr = OGRERR_CORRUPT_DATA;
        }
    }

    return eErr;
}
/*! @endcond */

/************************************************************************/
/*                           addRingDirectly()                          */
/************************************************************************/

OGRErr OGRTriangle::addRingDirectly( OGRCurve * poNewRing )
{
    if (oCC.nCurveCount == 0)
        return addRingDirectlyInternal( poNewRing, TRUE );
    else
        return OGRERR_FAILURE;
}

//! @cond Doxygen_Suppress
/************************************************************************/
/*                      GetCasterToPolygon()                            */
/************************************************************************/

OGRPolygon* OGRTriangle::CasterToPolygon(OGRSurface* poSurface)
{
    OGRTriangle* poTriangle = poSurface->toTriangle();
    OGRPolygon* poRet = new OGRPolygon( *poTriangle );
    delete poTriangle;
    return poRet;
}

OGRSurfaceCasterToPolygon OGRTriangle::GetCasterToPolygon() const {
    return OGRTriangle::CasterToPolygon;
}

/************************************************************************/
/*                        CastToPolygon()                               */
/************************************************************************/

OGRGeometry* OGRTriangle::CastToPolygon(OGRGeometry* poGeom)
{
    OGRGeometry* poRet = new OGRPolygon( *(poGeom->toPolygon()) );
    delete poGeom;
    return poRet;
}
//! @endcond

