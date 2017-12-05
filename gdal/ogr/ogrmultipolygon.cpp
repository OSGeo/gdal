/******************************************************************************
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  The OGRMultiPolygon class.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 1999, Frank Warmerdam
 * Copyright (c) 2008-2013, Even Rouault <even dot rouault at mines-paris dot org>
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

OGRMultiPolygon::OGRMultiPolygon() {}

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

OGRMultiPolygon::OGRMultiPolygon( const OGRMultiPolygon& other ) :
    OGRMultiSurface(other)
{}

/************************************************************************/
/*                         ~OGRMultiPolygon()                           */
/************************************************************************/

OGRMultiPolygon::~OGRMultiPolygon() {}

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

OGRErr OGRMultiPolygon::exportToWkt( char ** ppszDstText,
                                     OGRwkbVariant eWkbVariant ) const

{
    return exportToWktInternal( ppszDstText, eWkbVariant, "POLYGON" );
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
/*                            PointOnSurface()                          */
/************************************************************************/

OGRErr OGRMultiPolygon::PointOnSurface( OGRPoint * poPoint ) const
{
    return PointOnSurfaceInternal(poPoint);
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
    OGRGeometryCollection *poGC =
        TransferMembersAndDestroy(poMP, new OGRMultiSurface());

    OGRMultiSurface* poMultiSurface = dynamic_cast<OGRMultiSurface *>(poGC);
    if( poMultiSurface == NULL )
    {
        CPLError(CE_Fatal, CPLE_AppDefined,
                 "dynamic_cast failed.  Expected OGRMultiSurface.");
    }

    return poMultiSurface;
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

    if( poClone == NULL )
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

    if( poNewGeom->Is3D() && !Is3D() )
        set3D(TRUE);

    if( poNewGeom->IsMeasured() && !IsMeasured() )
        setMeasured(TRUE);

    if( !poNewGeom->Is3D() && Is3D() )
        poNewGeom->set3D(TRUE);

    if( !poNewGeom->IsMeasured() && IsMeasured() )
        poNewGeom->setMeasured(TRUE);

    OGRGeometry** papoNewGeoms = (OGRGeometry **) VSI_REALLOC_VERBOSE( papoGeoms,
                                             sizeof(void*) * (nGeomCount+1) );
    if( papoNewGeoms == NULL )
        return OGRERR_FAILURE;

    papoGeoms = papoNewGeoms;
    papoGeoms[nGeomCount] = poNewGeom;
    nGeomCount++;

    return OGRERR_NONE;
}
//! @endcond

