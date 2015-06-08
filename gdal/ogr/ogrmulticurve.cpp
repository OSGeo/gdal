/******************************************************************************
 * $Id$
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  The OGRMultiCurve class.
 * Author:   Even Rouault <even dot rouault at spatialys dot com>
 *
 ******************************************************************************
 * Copyright (c) 2014, Even Rouault <even dot rouault at spatialys dot com>
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
#include "ogr_api.h"

CPL_CVSID("$Id$");

/************************************************************************/
/*                            OGRMultiCurve()                           */
/************************************************************************/

/**
 * \brief Create an empty multi curve collection.
 */

OGRMultiCurve::OGRMultiCurve()
{
}

/************************************************************************/
/*                OGRMultiCurve( const OGRMultiCurve& )                 */
/************************************************************************/

/**
 * \brief Copy constructor.
 * 
 * Note: before GDAL 2.1, only the default implementation of the constructor
 * existed, which could be unsafe to use.
 * 
 * @since GDAL 2.1
 */

OGRMultiCurve::OGRMultiCurve( const OGRMultiCurve& other ) :
    OGRGeometryCollection(other)
{
}

/************************************************************************/
/*                           ~OGRMultiCurve()                           */
/************************************************************************/

OGRMultiCurve::~OGRMultiCurve()
{
}

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

OGRMultiCurve& OGRMultiCurve::operator=( const OGRMultiCurve& other )
{
    if( this != &other)
    {
        OGRGeometryCollection::operator=( other );
    }
    return *this;
}

/************************************************************************/
/*                          getGeometryType()                           */
/************************************************************************/

OGRwkbGeometryType OGRMultiCurve::getGeometryType() const

{
    if( getCoordinateDimension() == 3 )
        return wkbMultiCurveZ;
    else
        return wkbMultiCurve;
}

/************************************************************************/
/*                            getDimension()                            */
/************************************************************************/

int OGRMultiCurve::getDimension() const

{
    return 1;
}

/************************************************************************/
/*                          getGeometryName()                           */
/************************************************************************/

const char * OGRMultiCurve::getGeometryName() const

{
    return "MULTICURVE";
}

/************************************************************************/
/*                          isCompatibleSubType()                       */
/************************************************************************/

OGRBoolean OGRMultiCurve::isCompatibleSubType( OGRwkbGeometryType eGeomType ) const
{
    return OGR_GT_IsCurve(eGeomType);
}

/************************************************************************/
/*                       addCurveDirectlyFromWkt()                      */
/************************************************************************/

OGRErr OGRMultiCurve::addCurveDirectlyFromWkt( OGRGeometry* poSelf, OGRCurve* poCurve )
{
    return ((OGRMultiCurve*)poSelf)->addGeometryDirectly(poCurve);
}

/************************************************************************/
/*                           importFromWkt()                            */
/*                                                                      */
/*      Instantiate from well known text format.                        */
/************************************************************************/

OGRErr OGRMultiCurve::importFromWkt( char ** ppszInput )

{
    int bIsMultiCurve = (wkbFlatten(getGeometryType()) == wkbMultiCurve);
    return importCurveCollectionFromWkt( ppszInput,
                                         TRUE, /* bAllowEmptyComponent */
                                         bIsMultiCurve, /* bAllowLineString */
                                         bIsMultiCurve, /* bAllowCurve */
                                         bIsMultiCurve, /* bAllowCompoundCurve */
                                         addCurveDirectlyFromWkt );
}

/************************************************************************/
/*                            exportToWkt()                             */
/************************************************************************/

OGRErr OGRMultiCurve::exportToWkt( char ** ppszDstText,
                                   CPL_UNUSED OGRwkbVariant eWkbVariant ) const

{
    return exportToWktInternal( ppszDstText, wkbVariantIso, "LINESTRING" );
}

/************************************************************************/
/*                         hasCurveGeometry()                           */
/************************************************************************/

OGRBoolean OGRMultiCurve::hasCurveGeometry(int bLookForNonLinear) const
{
    if( bLookForNonLinear )
        return OGRGeometryCollection::hasCurveGeometry(TRUE);
    return TRUE;
}

/************************************************************************/
/*                          CastToMultiLineString()                     */
/************************************************************************/

/**
 * \brief Cast to multi line string.
 *
 * This method should only be called if the multicurve actually only contains
 * instances of OGRLineString. This can be verified if hasCurveGeometry(TRUE)
 * returns FALSE. It is not intended to approximate circular curves. For that
 * use getLinearGeometry().
 * 
 * The passed in geometry is consumed and a new one returned (or NULL in case
 * of failure). 
 * 
 * @param poMS the input geometry - ownership is passed to the method.
 * @return new geometry.
 */

OGRMultiLineString* OGRMultiCurve::CastToMultiLineString(OGRMultiCurve* poMC)
{
    for(int i=0;i<poMC->nGeomCount;i++)
    {
        poMC->papoGeoms[i] = OGRCurve::CastToLineString( (OGRCurve*)poMC->papoGeoms[i] );
        if( poMC->papoGeoms[i] == NULL )
        {
            delete poMC;
            return NULL;
        }
    }
    return (OGRMultiLineString*) TransferMembersAndDestroy(poMC, new OGRMultiLineString());
}
