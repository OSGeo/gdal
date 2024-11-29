/******************************************************************************
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  The OGRMultiLineString class.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 1999, Frank Warmerdam
 * Copyright (c) 2008-2013, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include "cpl_port.h"
#include "ogr_geometry.h"

#include <cstddef>

#include "cpl_error.h"
#include "ogr_core.h"
#include "ogr_p.h"

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

OGRMultiLineString::OGRMultiLineString(const OGRMultiLineString &) = default;

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

OGRMultiLineString &
OGRMultiLineString::operator=(const OGRMultiLineString &other)
{
    if (this != &other)
    {
        OGRMultiCurve::operator=(other);
    }
    return *this;
}

/************************************************************************/
/*                               clone()                                */
/************************************************************************/

OGRMultiLineString *OGRMultiLineString::clone() const

{
    auto ret = new (std::nothrow) OGRMultiLineString(*this);
    if (ret)
    {
        if (ret->WkbSize() != WkbSize())
        {
            delete ret;
            ret = nullptr;
        }
    }
    return ret;
}

/************************************************************************/
/*                          getGeometryType()                           */
/************************************************************************/

OGRwkbGeometryType OGRMultiLineString::getGeometryType() const

{
    if ((flags & OGR_G_3D) && (flags & OGR_G_MEASURED))
        return wkbMultiLineStringZM;
    else if (flags & OGR_G_MEASURED)
        return wkbMultiLineStringM;
    else if (flags & OGR_G_3D)
        return wkbMultiLineString25D;
    else
        return wkbMultiLineString;
}

/************************************************************************/
/*                          getGeometryName()                           */
/************************************************************************/

const char *OGRMultiLineString::getGeometryName() const

{
    return "MULTILINESTRING";
}

/************************************************************************/
/*                          isCompatibleSubType()                       */
/************************************************************************/

OGRBoolean
OGRMultiLineString::isCompatibleSubType(OGRwkbGeometryType eGeomType) const
{
    return wkbFlatten(eGeomType) == wkbLineString;
}

/************************************************************************/
/*                           importFromWkb()                            */
/************************************************************************/

OGRErr OGRMultiLineString::importFromWkb(const unsigned char *pabyData,
                                         size_t nSize,
                                         OGRwkbVariant eWkbVariant,
                                         size_t &nBytesConsumedOut)

{
    if (nGeomCount == 1 && nSize >= 9 && flags == 0 && pabyData[0] == wkbNDR &&
        memcmp(pabyData + 1, "\x05\x00\x00\x00\x01\x00\x00\x00", 8) == 0)
    {
        // Optimization to import a Intel-ordered 1-part multilinestring on
        // top of an existing 1-part multilinestring, to save dynamic memory
        // allocations.
        const size_t nDataOffset = 9;
        size_t nBytesConsumedLineString = 0;
        // cppcheck-suppress knownConditionTrueFalse
        if (nSize != static_cast<size_t>(-1))
            nSize -= nDataOffset;
        OGRErr eErr = cpl::down_cast<OGRLineString *>(papoGeoms[0])
                          ->OGRLineString::importFromWkb(
                              pabyData + nDataOffset, nSize, eWkbVariant,
                              nBytesConsumedLineString);
        if (eErr == OGRERR_NONE)
        {
            nBytesConsumedOut = nDataOffset + nBytesConsumedLineString;
        }
        else
        {
            empty();
        }
        return eErr;
    }

    return OGRGeometryCollection::importFromWkbInternal(
        pabyData, nSize, /*nRecLevel=*/0, eWkbVariant, nBytesConsumedOut);
}

/************************************************************************/
/*                            exportToWkt()                             */
/************************************************************************/

std::string OGRMultiLineString::exportToWkt(const OGRWktOptions &opts,
                                            OGRErr *err) const

{
    return exportToWktInternal(opts, err, "LINESTRING");
}

/************************************************************************/
/*                         hasCurveGeometry()                           */
/************************************************************************/

OGRBoolean
OGRMultiLineString::hasCurveGeometry(int /* bLookForNonLinear */) const
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

OGRMultiCurve *OGRMultiLineString::CastToMultiCurve(OGRMultiLineString *poMLS)
{
    OGRMultiCurve *poMLC = new OGRMultiCurve();
    TransferMembersAndDestroy(poMLS, poMLC);
    return poMLC;
}
