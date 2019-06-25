/******************************************************************************
 *
 * Project:  MSSQL Spatial driver
 * Purpose:  Implements OGRMSSQLGeometryValidator class to create valid SqlGeometries.
 * Author:   Tamas Szekeres, szekerest at gmail.com
 *
 ******************************************************************************
 * Copyright (c) 2010, Tamas Szekeres
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

#include "cpl_conv.h"
#include "ogr_mssqlspatial.h"

CPL_CVSID("$Id$")

/************************************************************************/
/*                   OGRMSSQLGeometryValidator()                        */
/************************************************************************/

OGRMSSQLGeometryValidator::OGRMSSQLGeometryValidator(OGRGeometry *poGeom, int geomColumnType)
{
    poOriginalGeometry = poGeom;
    poValidGeometry = nullptr;
    nGeomColumnType = geomColumnType;
    bIsValid = IsValid(poGeom);
}

/************************************************************************/
/*                      ~OGRMSSQLGeometryValidator()                    */
/************************************************************************/

OGRMSSQLGeometryValidator::~OGRMSSQLGeometryValidator()
{
    if (poValidGeometry)
        delete poValidGeometry;
}

/************************************************************************/
/*                         IsValidLatLon()                             */
/************************************************************************/

static double MakeValidLatitude(double latitude)
{
    if (latitude < -90)
        return -90;

    if (latitude > 90.0)
        return 90.0;

    return latitude;
}

static double MakeValidLongitude(double longitude)
{
    if (longitude < -15069.0)
        return -15069.0;

    if (longitude > 15069.0)
        return 15069.0;

    return longitude;
}

bool OGRMSSQLGeometryValidator::IsValidLatLon(double longitude, double latitude)
{
    if (MakeValidLatitude(latitude) != latitude)
    {
        if (poValidGeometry == nullptr)
            CPLError(CE_Warning, CPLE_NotSupported,
                "Latitude values must be between -90 and 90 degrees");
        return false;
    }
    if (MakeValidLongitude(longitude) != longitude)
    {
        if (poValidGeometry == nullptr)
            CPLError(CE_Warning, CPLE_NotSupported,
                "Longitude values must be between -15069 and 15069 degrees");
        return false;
    }
    return true;
}

/************************************************************************/
/*                         IsValidCircularZ()                           */
/************************************************************************/

bool OGRMSSQLGeometryValidator::IsValidCircularZ(double z1, double z2)
{
    if (z1 != z2)
    {
        if (poValidGeometry == nullptr)
            CPLError(CE_Warning, CPLE_NotSupported,
                "Circular arc segments with Z values must have equal Z value for all 3 points");
        return false;
    }
    return true;
}

/************************************************************************/
/*                         IsValidPolygonRingCount()                    */
/************************************************************************/

bool OGRMSSQLGeometryValidator::IsValidPolygonRingCount(OGRCurve* poGeom)
{
    if (poGeom->getNumPoints() < 4)
    {
        if (poValidGeometry == nullptr)
            CPLError(CE_Warning, CPLE_NotSupported,
                "Each ring of a polygon must contain at least four points");
        return false;
    }
    return true;
}

/************************************************************************/
/*                         IsValidPolygonRingClosed()                   */
/************************************************************************/

bool OGRMSSQLGeometryValidator::IsValidPolygonRingClosed(OGRCurve* poGeom)
{
    if (poGeom->get_IsClosed() == FALSE)
    {
        if (poValidGeometry == nullptr)
            CPLError(CE_Warning, CPLE_NotSupported,
                "Each ring of a polygon must have the same start and end points.");
        return false;
    }
    return true;
}

/************************************************************************/
/*                         ValidatePoint()                              */
/************************************************************************/

bool OGRMSSQLGeometryValidator::IsValid(OGRPoint* poGeom)
{
    if (poGeom->IsEmpty())
        return true;

    if (nGeomColumnType == MSSQLCOLTYPE_GEOGRAPHY)
        return IsValidLatLon(poGeom->getX(), poGeom->getY());

    return true;
}

void OGRMSSQLGeometryValidator::MakeValid(OGRPoint* poGeom)
{
    if (poGeom->IsEmpty())
        return;

    if (nGeomColumnType == MSSQLCOLTYPE_GEOGRAPHY)
    {
        poGeom->setX(MakeValidLongitude(poGeom->getX()));
        poGeom->setY(MakeValidLatitude(poGeom->getY()));
    }
}

/************************************************************************/
/*                     ValidateMultiPoint()                             */
/************************************************************************/

bool OGRMSSQLGeometryValidator::IsValid(OGRMultiPoint* poGeom)
{
    if (nGeomColumnType == MSSQLCOLTYPE_GEOGRAPHY)
    {
        for (int i = 0; i < poGeom->getNumGeometries(); i++)
        {
            if (!IsValid(poGeom->getGeometryRef(i)->toPoint()))
                return false;
        }
    }
    return true;
}

void OGRMSSQLGeometryValidator::MakeValid(OGRMultiPoint* poGeom)
{
    if (nGeomColumnType == MSSQLCOLTYPE_GEOGRAPHY)
    {
        for (int i = 0; i < poGeom->getNumGeometries(); i++)
        {
            MakeValid(poGeom->getGeometryRef(i)->toPoint());
        }
    }
}

/************************************************************************/
/*                         ValidateSimpleCurve()                        */
/************************************************************************/

bool OGRMSSQLGeometryValidator::IsValid(OGRSimpleCurve* poGeom)
{
    if (nGeomColumnType == MSSQLCOLTYPE_GEOGRAPHY)
    {
        for (int i = 0; i < poGeom->getNumPoints(); i++)
        {
            if (!IsValidLatLon(poGeom->getX(i), poGeom->getY(i)))
                return false;
        }
    }
    return true;
}

void OGRMSSQLGeometryValidator::MakeValid(OGRSimpleCurve* poGeom)
{
    if (nGeomColumnType == MSSQLCOLTYPE_GEOGRAPHY)
    {
        for (int i = 0; i < poGeom->getNumPoints(); i++)
        {
            poGeom->setPoint(i, MakeValidLongitude(poGeom->getX(i)),
                                MakeValidLatitude(poGeom->getY(i)));
        }
    }
}

/************************************************************************/
/*                         ValidateLineString()                         */
/************************************************************************/

bool OGRMSSQLGeometryValidator::IsValid(OGRLineString * poGeom)
{
    return IsValid(poGeom->toSimpleCurve());
}

void OGRMSSQLGeometryValidator::MakeValid(OGRLineString * poGeom)
{
    MakeValid(poGeom->toSimpleCurve());
}

/************************************************************************/
/*                         ValidateCircularString()                     */
/************************************************************************/

bool OGRMSSQLGeometryValidator::IsValid(OGRCircularString* poGeom)
{   
    if (!IsValid(poGeom->toSimpleCurve()))
        return false;

    if (poGeom->Is3D())
    {
        for (int i = 1; i < poGeom->getNumPoints(); i++)
        {
            if (!IsValidCircularZ(poGeom->getZ(i), poGeom->getZ(0)))
            {
                return false;
            }
        }
    }
    return true;
}

void OGRMSSQLGeometryValidator::MakeValid(OGRCircularString* poGeom)
{
    MakeValid(poGeom->toSimpleCurve());

    if (poGeom->Is3D())
    {
        for (int i = 1; i < poGeom->getNumPoints(); i++)
        {
            poGeom->setZ(i, poGeom->getZ(0));
        }
    }
}

/************************************************************************/
/*                         ValidateCompoundCurve()                      */
/************************************************************************/

bool OGRMSSQLGeometryValidator::IsValid(OGRCompoundCurve* poGeom)
{
    for (int i = 0; i < poGeom->getNumCurves(); i++)
    {
        OGRCurve* poCurve = poGeom->getCurve(i);
        switch (wkbFlatten(poCurve->getGeometryType()))
        {
        case wkbLineString:
            if (!IsValid(poCurve->toLineString()))
                return false;
            break;

        case wkbCircularString:
            if (!IsValid(poCurve->toCircularString()))
                return false;
            break;

        default:
            break;
        }
    }
    return true;
}

void OGRMSSQLGeometryValidator::MakeValid(OGRCompoundCurve* poGeom)
{
    for (int i = 0; i < poGeom->getNumCurves(); i++)
    {
        OGRCurve* poCurve = poGeom->getCurve(i);
        switch (wkbFlatten(poCurve->getGeometryType()))
        {
        case wkbLineString:
            MakeValid(poCurve->toLineString());
            break;
            
        case wkbCircularString:
            MakeValid(poCurve->toCircularString());
            break;

        default:
            break;
        }
    }
}

/************************************************************************/
/*                         ValidateLinearRing()                         */
/************************************************************************/

bool OGRMSSQLGeometryValidator::IsValid(OGRLinearRing * poGeom)
{
    return IsValid(poGeom->toSimpleCurve());
}

void OGRMSSQLGeometryValidator::MakeValid(OGRLinearRing * poGeom)
{
    MakeValid(poGeom->toSimpleCurve());
}

/************************************************************************/
/*                     ValidateMultiLineString()                        */
/************************************************************************/

bool OGRMSSQLGeometryValidator::IsValid(OGRMultiLineString* poGeom)
{
    if (nGeomColumnType == MSSQLCOLTYPE_GEOGRAPHY)
    {
        for (int i = 0; i < poGeom->getNumGeometries(); i++)
        {
            if (!IsValid(poGeom->getGeometryRef(i)->toLineString()))
                return false;
        }
    }
    return true;
}

void OGRMSSQLGeometryValidator::MakeValid(OGRMultiLineString* poGeom)
{
    if (nGeomColumnType == MSSQLCOLTYPE_GEOGRAPHY)
    {
        for (int i = 0; i < poGeom->getNumGeometries(); i++)
        {
            MakeValid(poGeom->getGeometryRef(i)->toLineString());
        }
    }
}

/************************************************************************/
/*                         ValidatePolygon()                            */
/************************************************************************/

bool OGRMSSQLGeometryValidator::IsValid(OGRPolygon* poGeom)
{
    if (poGeom->IsEmpty())
        return true;

    if (!IsValid(poGeom->getExteriorRing()))
        return false;

    if (!IsValidPolygonRingCount(poGeom->getExteriorRing()))
        return false;

    if (!IsValidPolygonRingClosed(poGeom->getExteriorRing()))
        return false;

    for (int i = 0; i < poGeom->getNumInteriorRings(); i++)
    {
        if (!IsValid(poGeom->getInteriorRing(i)))
            return false;

        if (!IsValidPolygonRingCount(poGeom->getInteriorRing(i)))
            return false;

        if (!IsValidPolygonRingClosed(poGeom->getInteriorRing(i)))
            return false;
    }

    return true;
}

void OGRMSSQLGeometryValidator::MakeValid(OGRPolygon* poGeom)
{
    if (poGeom->IsEmpty())
        return;

    MakeValid(poGeom->getExteriorRing());

    for (int i = 0; i < poGeom->getNumInteriorRings(); i++)
    {
        MakeValid(poGeom->getInteriorRing(i));
    }

    poGeom->closeRings();
}

/************************************************************************/
/*                         ValidateCurvePolygon()                       */
/************************************************************************/

bool OGRMSSQLGeometryValidator::IsValid(OGRCurvePolygon* poGeom)
{
    if (poGeom->IsEmpty())
        return true;

    if (!IsValid(poGeom->getExteriorRingCurve()))
        return false;

    if (!IsValidPolygonRingCount(poGeom->getExteriorRingCurve()))
        return false;

    if (!IsValidPolygonRingClosed(poGeom->getExteriorRingCurve()))
        return false;

    for (int i = 0; i < poGeom->getNumInteriorRings(); i++)
    {
        if (!IsValid(poGeom->getInteriorRingCurve(i)))
            return false;

        if (!IsValidPolygonRingCount(poGeom->getInteriorRingCurve(i)))
            return false;

        if (!IsValidPolygonRingClosed(poGeom->getInteriorRingCurve(i)))
            return false;
    }

    return true;
}

void OGRMSSQLGeometryValidator::MakeValid(OGRCurvePolygon* poGeom)
{
    if (poGeom->IsEmpty())
        return;

    MakeValid(poGeom->getExteriorRingCurve());
    for (int i = 0; i < poGeom->getNumInteriorRings(); i++)
    {
        MakeValid(poGeom->getInteriorRingCurve(i));
    }
}

/************************************************************************/
/*                         ValidateMultiPolygon()                       */
/************************************************************************/

bool OGRMSSQLGeometryValidator::IsValid(OGRMultiPolygon* poGeom)
{
    for (int i = 0; i < poGeom->getNumGeometries(); i++)
    {
        if (!IsValid(poGeom->getGeometryRef(i)->toPolygon()))
            return false;
    }
    return true;
}

void OGRMSSQLGeometryValidator::MakeValid(OGRMultiPolygon* poGeom)
{
    for (int i = 0; i < poGeom->getNumGeometries(); i++)
    {
        MakeValid(poGeom->getGeometryRef(i)->toPolygon());
    }
}

/************************************************************************/
/*                     ValidateGeometryCollection()                     */
/************************************************************************/

bool OGRMSSQLGeometryValidator::IsValid(OGRGeometryCollection* poGeom)
{
    for (int i = 0; i < poGeom->getNumGeometries(); i++)
    {
        if (!IsValid(poGeom->getGeometryRef(i)))
            return false;
    }
    return true;
}

void OGRMSSQLGeometryValidator::MakeValid(OGRGeometryCollection* poGeom)
{
    for (int i = 0; i < poGeom->getNumGeometries(); i++)
    {
        MakeValid(poGeom->getGeometryRef(i));
    }
}

/************************************************************************/
/*                         ValidateGeometry()                           */
/************************************************************************/

bool OGRMSSQLGeometryValidator::IsValid(OGRGeometry* poGeom)
{
    if (!poGeom)
        return false;

    switch (wkbFlatten(poGeom->getGeometryType()))
    {
    case wkbPoint:
        return IsValid(poGeom->toPoint());
    case wkbLineString:
        return IsValid(poGeom->toLineString());
    case wkbPolygon:
        return IsValid(poGeom->toPolygon());
    case wkbCurvePolygon :
        return IsValid(poGeom->toCurvePolygon());
    case wkbMultiPoint:
        return IsValid(poGeom->toMultiPoint());
    case wkbMultiLineString:
        return IsValid(poGeom->toMultiLineString());
    case wkbCircularString:
        return IsValid(poGeom->toCircularString());
    case wkbCompoundCurve:
        return IsValid(poGeom->toCompoundCurve());
    case wkbMultiPolygon:
        return IsValid(poGeom->toMultiPolygon());
    case wkbGeometryCollection:
        return IsValid(poGeom->toGeometryCollection());
    case wkbLinearRing:
        return IsValid(poGeom->toLinearRing());
    default:
        break;
    }
    return false;
}

void OGRMSSQLGeometryValidator::MakeValid(OGRGeometry* poGeom)
{
    if (!poGeom)
        return;

    switch (wkbFlatten(poGeom->getGeometryType()))
    {
    case wkbPoint:
        MakeValid(poGeom->toPoint());
        break;
    case wkbLineString:
        MakeValid(poGeom->toLineString());
        break;
    case wkbPolygon:
        MakeValid(poGeom->toPolygon());
        break;
    case wkbCurvePolygon:
        MakeValid(poGeom->toCurvePolygon());
        break;
    case wkbMultiPoint:
        MakeValid(poGeom->toMultiPoint());
        break;
    case wkbMultiLineString:
        MakeValid(poGeom->toMultiLineString());
        break;
    case wkbCircularString:
        MakeValid(poGeom->toCircularString());
        break;
    case wkbCompoundCurve:
        MakeValid(poGeom->toCompoundCurve());
        break;
    case wkbMultiPolygon:
        MakeValid(poGeom->toMultiPolygon());
        break;
    case wkbGeometryCollection:
        MakeValid(poGeom->toGeometryCollection());
        break;
    case wkbLinearRing:
        MakeValid(poGeom->toLinearRing());
        break;
    default:
        break;
    }
}

bool OGRMSSQLGeometryValidator::ValidateGeometry(OGRGeometry* poGeom)
{
    if (poValidGeometry != nullptr)
    {
        delete poValidGeometry;
        poValidGeometry = nullptr;
    }

    if (!IsValid(poGeom))
    {
        poValidGeometry = poGeom->clone();
        MakeValid(poValidGeometry);
        return false;
    }
    return true;
}

/************************************************************************/
/*                      GetValidGeometryRef()                           */
/************************************************************************/
OGRGeometry* OGRMSSQLGeometryValidator::GetValidGeometryRef()
{
    if (bIsValid || poOriginalGeometry == nullptr)
        return poOriginalGeometry;

    if (poValidGeometry)
    {
        CPLError( CE_Warning, CPLE_NotSupported,
                      "Invalid geometry has been converted from %s to %s.",
                      poOriginalGeometry->getGeometryName(),
                      poValidGeometry->getGeometryName() );
    }
    else
    {
        CPLError( CE_Warning, CPLE_NotSupported,
                      "Invalid geometry has been converted from %s to null.",
                      poOriginalGeometry->getGeometryName());
    }

    return poValidGeometry;
}
