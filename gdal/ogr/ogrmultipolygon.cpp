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

#include "ogr_geometry.h"
#include "ogr_api.h"
#include "ogr_p.h"

CPL_CVSID("$Id$");

/************************************************************************/
/*                          OGRMultiPolygon()                           */
/************************************************************************/

/**
 * \brief Create an empty multi polygon collection.
 */

OGRMultiPolygon::OGRMultiPolygon()
{
}

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
{
}

/************************************************************************/
/*                         ~OGRMultiPolygon()                           */
/************************************************************************/

OGRMultiPolygon::~OGRMultiPolygon()
{
}

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
    else if( flags & OGR_G_MEASURED  )
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

OGRBoolean OGRMultiPolygon::isCompatibleSubType( OGRwkbGeometryType eGeomType ) const
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

OGRBoolean OGRMultiPolygon::hasCurveGeometry(CPL_UNUSED int bLookForNonLinear) const
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

OGRMultiSurface* OGRMultiPolygon::CastToMultiSurface(OGRMultiPolygon* poMP)
{
    return (OGRMultiSurface*) TransferMembersAndDestroy(poMP, new OGRMultiSurface());
}
