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
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include "cpl_port.h"
#include "ogr_geometry.h"

#include "ogr_api.h"
#include "ogr_core.h"
#include "ogr_p.h"

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

OGRMultiPolygon::OGRMultiPolygon(const OGRMultiPolygon &) = default;

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

OGRMultiPolygon &OGRMultiPolygon::operator=(const OGRMultiPolygon &other)
{
    if (this != &other)
    {
        OGRMultiSurface::operator=(other);
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
    if ((flags & OGR_G_3D) && (flags & OGR_G_MEASURED))
        return wkbMultiPolygonZM;
    else if (flags & OGR_G_MEASURED)
        return wkbMultiPolygonM;
    else if (flags & OGR_G_3D)
        return wkbMultiPolygon25D;
    else
        return wkbMultiPolygon;
}

/************************************************************************/
/*                          getGeometryName()                           */
/************************************************************************/

const char *OGRMultiPolygon::getGeometryName() const

{
    return "MULTIPOLYGON";
}

/************************************************************************/
/*                          isCompatibleSubType()                       */
/************************************************************************/

OGRBoolean
OGRMultiPolygon::isCompatibleSubType(OGRwkbGeometryType eGeomType) const
{
    return wkbFlatten(eGeomType) == wkbPolygon;
}

/************************************************************************/
/*                           importFromWkb()                            */
/************************************************************************/

OGRErr OGRMultiPolygon::importFromWkb(const unsigned char *pabyData,
                                      size_t nSize, OGRwkbVariant eWkbVariant,
                                      size_t &nBytesConsumedOut)

{
    if (nGeomCount == 1 && nSize >= 9 && flags == 0 && pabyData[0] == wkbNDR &&
        memcmp(pabyData + 1, "\x06\x00\x00\x00\x01\x00\x00\x00", 8) == 0)
    {
        // Optimization to import a Intel-ordered 1-part multipolyon on
        // top of an existing 1-part multipolygon, to save dynamic memory
        // allocations.
        const size_t nDataOffset = 9;
        size_t nBytesConsumedPolygon = 0;
        // cppcheck-suppress knownConditionTrueFalse
        if (nSize != static_cast<size_t>(-1))
            nSize -= nDataOffset;
        OGRErr eErr =
            cpl::down_cast<OGRPolygon *>(papoGeoms[0])
                ->OGRPolygon::importFromWkb(pabyData + nDataOffset, nSize,
                                            eWkbVariant, nBytesConsumedPolygon);
        if (eErr == OGRERR_NONE)
        {
            nBytesConsumedOut = nDataOffset + nBytesConsumedPolygon;
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

std::string OGRMultiPolygon::exportToWkt(const OGRWktOptions &opts,
                                         OGRErr *err) const
{
    return exportToWktInternal(opts, err, "POLYGON");
}

/************************************************************************/
/*                         hasCurveGeometry()                           */
/************************************************************************/

OGRBoolean OGRMultiPolygon::hasCurveGeometry(int /* bLookForNonLinear */) const
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

OGRMultiSurface *OGRMultiPolygon::CastToMultiSurface(OGRMultiPolygon *poMP)
{
    OGRMultiSurface *poMS = new OGRMultiSurface();
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
    const OGRGeometry *poNewGeom, OGRwkbGeometryType eSubGeometryType)

{
    OGRGeometry *poClone = poNewGeom->clone();
    OGRErr eErr;

    if (poClone == nullptr)
        return OGRERR_FAILURE;
    eErr = _addGeometryDirectlyWithExpectedSubGeometryType(poClone,
                                                           eSubGeometryType);
    if (eErr != OGRERR_NONE)
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
    OGRGeometry *poNewGeom, OGRwkbGeometryType eSubGeometryType)
{
    if (wkbFlatten(poNewGeom->getGeometryType()) != eSubGeometryType)
        return OGRERR_UNSUPPORTED_GEOMETRY_TYPE;

    HomogenizeDimensionalityWith(poNewGeom);

    OGRGeometry **papoNewGeoms = static_cast<OGRGeometry **>(
        VSI_REALLOC_VERBOSE(papoGeoms, sizeof(void *) * (nGeomCount + 1)));
    if (papoNewGeoms == nullptr)
        return OGRERR_FAILURE;

    papoGeoms = papoNewGeoms;
    papoGeoms[nGeomCount] = poNewGeom;
    nGeomCount++;

    return OGRERR_NONE;
}

//! @endcond
