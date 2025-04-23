/******************************************************************************
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  The OGRMultiCurve class.
 * Author:   Even Rouault <even dot rouault at spatialys dot com>
 *
 ******************************************************************************
 * Copyright (c) 2014, Even Rouault <even dot rouault at spatialys dot com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include "cpl_port.h"
#include "ogr_geometry.h"

#include <cstddef>

#include "cpl_error.h"
#include "ogr_api.h"
#include "ogr_core.h"
#include "ogr_p.h"

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

OGRMultiCurve::OGRMultiCurve(const OGRMultiCurve &) = default;

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

OGRMultiCurve &OGRMultiCurve::operator=(const OGRMultiCurve &other)
{
    if (this != &other)
    {
        OGRGeometryCollection::operator=(other);
    }
    return *this;
}

/************************************************************************/
/*                               clone()                                */
/************************************************************************/

OGRMultiCurve *OGRMultiCurve::clone() const

{
    auto ret = new (std::nothrow) OGRMultiCurve(*this);
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

OGRwkbGeometryType OGRMultiCurve::getGeometryType() const

{
    if ((flags & OGR_G_3D) && (flags & OGR_G_MEASURED))
        return wkbMultiCurveZM;
    else if (flags & OGR_G_MEASURED)
        return wkbMultiCurveM;
    else if (flags & OGR_G_3D)
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

const char *OGRMultiCurve::getGeometryName() const

{
    return "MULTICURVE";
}

/************************************************************************/
/*                          isCompatibleSubType()                       */
/************************************************************************/

OGRBoolean
OGRMultiCurve::isCompatibleSubType(OGRwkbGeometryType eGeomType) const
{
    return OGR_GT_IsCurve(eGeomType);
}

/*! @cond Doxygen_Suppress */
/************************************************************************/
/*                       addCurveDirectlyFromWkt()                      */
/************************************************************************/

OGRErr OGRMultiCurve::addCurveDirectlyFromWkt(OGRGeometry *poSelf,
                                              OGRCurve *poCurve)
{
    return poSelf->toMultiCurve()->addGeometryDirectly(poCurve);
}

/*! @endcond */

/************************************************************************/
/*                           importFromWkt()                            */
/*                                                                      */
/*      Instantiate from well known text format.                        */
/************************************************************************/

OGRErr OGRMultiCurve::importFromWkt(const char **ppszInput)

{
    const bool bIsMultiCurve = wkbFlatten(getGeometryType()) == wkbMultiCurve;
    return importCurveCollectionFromWkt(ppszInput,
                                        TRUE,           // bAllowEmptyComponent.
                                        bIsMultiCurve,  // bAllowLineString.
                                        bIsMultiCurve,  // bAllowCurve.
                                        bIsMultiCurve,  // bAllowCompoundCurve.
                                        addCurveDirectlyFromWkt);
}

/************************************************************************/
/*                            exportToWkt()                             */
/************************************************************************/

std::string OGRMultiCurve::exportToWkt(const OGRWktOptions &opts,
                                       OGRErr *err) const
{
    OGRWktOptions optsModified(opts);
    optsModified.variant = wkbVariantIso;
    return exportToWktInternal(optsModified, err, "LINESTRING");
}

/************************************************************************/
/*                         hasCurveGeometry()                           */
/************************************************************************/

OGRBoolean OGRMultiCurve::hasCurveGeometry(int bLookForNonLinear) const
{
    if (bLookForNonLinear)
        return OGRGeometryCollection::hasCurveGeometry(TRUE);
    return true;
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
 * @param poMC the input geometry - ownership is passed to the method.
 * @return new geometry.
 */

OGRMultiLineString *OGRMultiCurve::CastToMultiLineString(OGRMultiCurve *poMC)
{
    for (auto &&poSubGeom : *poMC)
    {
        poSubGeom = OGRCurve::CastToLineString(poSubGeom);
        if (poSubGeom == nullptr)
        {
            delete poMC;
            return nullptr;
        }
    }
    OGRMultiLineString *poMLS = new OGRMultiLineString();
    TransferMembersAndDestroy(poMC, poMLS);
    return poMLS;
}
