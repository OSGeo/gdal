/******************************************************************************
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  The OGRMultiPolygon class.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 1999, Frank Warmerdam
 * Copyright (c) 2008-2013, Even Rouault <even dot rouault at spatialys.com>
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

#include "ogr_api.h"
#include "ogr_core.h"
#include "ogr_p.h"

CPL_CVSID("$Id$")

/************************************************************************/
/*                          OGRMultiPolygon()                           */
/************************************************************************/

/**
 * \brief Create an empty multi polygon collection.
 */

OGRMultiPolygon::OGRMultiPolygon() = default;

/************************************************************************/
/*              OGRMultiPolygon( const OGRMultiPolygon& )               */
/************************************************************************/

/**
 * \brief Copy constructor.
 *
 * Note: before GDAL 2.1, only the default implementation of the constructor
 * existed, which could be unsafe to use.
 *
 * @since GDAL 2.1
 */

OGRMultiPolygon::OGRMultiPolygon( const OGRMultiPolygon& ) = default;

/************************************************************************/
/*                         ~OGRMultiPolygon()                           */
/************************************************************************/

OGRMultiPolygon::~OGRMultiPolygon() = default;

/************************************************************************/
/*                  operator=( const OGRMultiPolygon&)                    */
/************************************************************************/

/**
 * \brief Assignment operator.
 *
 * Note: before GDAL 2.1, only the default implementation of the operator
 * existed, which could be unsafe to use.
 *
 * @since GDAL 2.1
 */

OGRMultiPolygon& OGRMultiPolygon::operator=( const OGRMultiPolygon& other )
{
    if( this != &other)
    {
        OGRMultiSurface::operator=( other );
    }
    return *this;
}

/************************************************************************/
/*                               clone()                                */
/************************************************************************/

OGRMultiPolygon *OGRMultiPolygon::clone() const

{
    return new (std::nothrow) OGRMultiPolygon(*this);
}

/************************************************************************/
/*                          getGeometryType()                           */
/************************************************************************/

OGRwkbGeometryType OGRMultiPolygon::getGeometryType() const

{
    if( (flags & OGR_G_3D) && (flags & OGR_G_MEASURED) )
        return wkbMultiPolygonZM;
    else if( flags & OGR_G_MEASURED )
        return wkbMultiPolygonM;
    else if( flags & OGR_G_3D )
        return wkbMultiPolygon25D;
    else
        return wkbMultiPolygon;
}

/************************************************************************/
/*                          getGeometryName()                           */
/************************************************************************/

const char * OGRMultiPolygon::getGeometryName() const

{
    return "MULTIPOLYGON";
}

/************************************************************************/
/*                          isCompatibleSubType()                       */
/************************************************************************/

OGRBoolean
OGRMultiPolygon::isCompatibleSubType( OGRwkbGeometryType eGeomType ) const
{
    return wkbFlatten(eGeomType) == wkbPolygon;
}

/************************************************************************/
/*                            exportToWkt()                             */
/************************************************************************/

std::string OGRMultiPolygon::exportToWkt(const OGRWktOptions& opts, OGRErr *err) const
{
    return exportToWktInternal(opts, err, "POLYGON");
}

/************************************************************************/
/*                         hasCurveGeometry()                           */
/************************************************************************/

OGRBoolean
OGRMultiPolygon::hasCurveGeometry( int /* bLookForNonLinear */ ) const
{
    return FALSE;
}

/************************************************************************/
/*                          CastToMultiSurface()                        */
/************************************************************************/

/**
 * \brief Cast to multisurface.
 *
 * The passed in geometry is consumed and a new one returned .
 *
 * @param poMP the input geometry - ownership is passed to the method.
 * @return new geometry.
 */

OGRMultiSurface* OGRMultiPolygon::CastToMultiSurface( OGRMultiPolygon* poMP )
{
    OGRMultiSurface* poMS = new OGRMultiSurface();
    TransferMembersAndDestroy(poMP, poMS);
    return poMS;
}


/************************************************************************/
/*               _addGeometryWithExpectedSubGeometryType()              */
/*      Only to be used in conjunction with OGRPolyhedralSurface.       */
/*                        DO NOT USE IT ELSEWHERE.                      */
/************************************************************************/

//! @cond Doxygen_Suppress
OGRErr OGRMultiPolygon::_addGeometryWithExpectedSubGeometryType(
                                      const OGRGeometry * poNewGeom,
                                      OGRwkbGeometryType eSubGeometryType )

{
    OGRGeometry *poClone = poNewGeom->clone();
    OGRErr      eErr;

    if( poClone == nullptr )
        return OGRERR_FAILURE;
    eErr = _addGeometryDirectlyWithExpectedSubGeometryType( poClone, eSubGeometryType );
    if( eErr != OGRERR_NONE )
        delete poClone;

    return eErr;
}
//! @endcond

/************************************************************************/
/*                 _addGeometryDirectlyWithExpectedSubGeometryType()    */
/*      Only to be used in conjunction with OGRPolyhedralSurface.       */
/*                        DO NOT USE IT ELSEWHERE.                      */
/************************************************************************/

//! @cond Doxygen_Suppress
OGRErr OGRMultiPolygon::_addGeometryDirectlyWithExpectedSubGeometryType(
                                      OGRGeometry * poNewGeom,
                                      OGRwkbGeometryType eSubGeometryType )
{
    if ( wkbFlatten(poNewGeom->getGeometryType()) != eSubGeometryType)
        return OGRERR_UNSUPPORTED_GEOMETRY_TYPE;

    HomogenizeDimensionalityWith(poNewGeom);

    OGRGeometry** papoNewGeoms = static_cast<OGRGeometry **>(
        VSI_REALLOC_VERBOSE( papoGeoms, sizeof(void*) * (nGeomCount+1) ));
    if( papoNewGeoms == nullptr )
        return OGRERR_FAILURE;

    papoGeoms = papoNewGeoms;
    papoGeoms[nGeomCount] = poNewGeom;
    nGeomCount++;

    return OGRERR_NONE;
}
//! @endcond

