/******************************************************************************
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  The OGRMultiLineString class.
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
#include "ogr_p.h"

CPL_CVSID("$Id$");

/************************************************************************/
/*                        OGRMultiLineString()                          */
/************************************************************************/

/**
 * \brief Create an empty multi line string collection.
 */

OGRMultiLineString::OGRMultiLineString() {}

/************************************************************************/
/*           OGRMultiLineString( const OGRMultiLineString& )            */
/************************************************************************/

/**
 * \brief Copy constructor.
 *
 * Note: before GDAL 2.1, only the default implementation of the constructor
 * existed, which could be unsafe to use.
 *
 * @since GDAL 2.1
 */

OGRMultiLineString::OGRMultiLineString( const OGRMultiLineString& other ) :
    OGRMultiCurve(other)
{}

/************************************************************************/
/*                       ~OGRMultiLineString()                          */
/************************************************************************/

OGRMultiLineString::~OGRMultiLineString() {}

/************************************************************************/
/*                  operator=( const OGRMultiCurve&)                    */
/************************************************************************/

/**
 * \brief Assignment operator.
 *
 * Note: before GDAL 2.1, only the default implementation of the operator
 * existed, which could be unsafe to use.
 *
 * @since GDAL 2.1
 */

OGRMultiLineString& OGRMultiLineString::operator=( const OGRMultiLineString& other )
{
    if( this != &other )
    {
        OGRMultiCurve::operator=( other );
    }
    return *this;
}

/************************************************************************/
/*                          getGeometryType()                           */
/************************************************************************/

OGRwkbGeometryType OGRMultiLineString::getGeometryType() const

{
    if( (flags & OGR_G_3D) && (flags & OGR_G_MEASURED) )
        return wkbMultiLineStringZM;
    else if( flags & OGR_G_MEASURED  )
        return wkbMultiLineStringM;
    else if( flags & OGR_G_3D )
        return wkbMultiLineString25D;
    else
        return wkbMultiLineString;
}

/************************************************************************/
/*                          getGeometryName()                           */
/************************************************************************/

const char * OGRMultiLineString::getGeometryName() const

{
    return "MULTILINESTRING";
}

/************************************************************************/
/*                          isCompatibleSubType()                       */
/************************************************************************/

OGRBoolean OGRMultiLineString::isCompatibleSubType( OGRwkbGeometryType eGeomType ) const
{
    return wkbFlatten(eGeomType) == wkbLineString;
}

/************************************************************************/
/*                            exportToWkt()                             */
/************************************************************************/

OGRErr OGRMultiLineString::exportToWkt( char ** ppszDstText,
                                        OGRwkbVariant eWkbVariant ) const

{
    return exportToWktInternal( ppszDstText, eWkbVariant, "LINESTRING" );
}

/************************************************************************/
/*                         hasCurveGeometry()                           */
/************************************************************************/

OGRBoolean OGRMultiLineString::hasCurveGeometry(
    int /* bLookForNonLinear */ ) const
{
    return false;
}

/************************************************************************/
/*                          CastToMultiCurve()                          */
/************************************************************************/

/**
 * \brief Cast to multicurve.
 *
 * The passed in geometry is consumed and a new one returned.
 *
 * @param poMLS the input geometry - ownership is passed to the method.
 * @return new geometry.
 */

OGRMultiCurve* OGRMultiLineString::CastToMultiCurve( OGRMultiLineString* poMLS )
{
    OGRMultiCurve *poMultiCurve = dynamic_cast<OGRMultiCurve *>(
        TransferMembersAndDestroy(poMLS, new OGRMultiCurve()) );
    if( poMultiCurve == NULL )
    {
        CPLError( CE_Fatal, CPLE_AppDefined,
                  "OGRMultiCurve dynamic_cast failed." );
        return NULL;
    }

    return poMultiCurve;
}
