// SPDX-License-Identifier: MIT
// Copyright 2024, Even Rouault <even.rouault at spatialys.com>

#include "ogrvrtgeometrytypes.h"

/************************************************************************/
/*                       OGRVRTGetGeometryType()                        */
/************************************************************************/

#define STRINGIFY(x) x, #x

static const struct
{
    OGRwkbGeometryType eType;
    const char *pszName;
    bool bIsoFlags;
} asGeomTypeNames[] = {
    {STRINGIFY(wkbUnknown), false},

    {STRINGIFY(wkbPoint), false},
    {STRINGIFY(wkbLineString), false},
    {STRINGIFY(wkbPolygon), false},
    {STRINGIFY(wkbMultiPoint), false},
    {STRINGIFY(wkbMultiLineString), false},
    {STRINGIFY(wkbMultiPolygon), false},
    {STRINGIFY(wkbGeometryCollection), false},

    {STRINGIFY(wkbCircularString), true},
    {STRINGIFY(wkbCompoundCurve), true},
    {STRINGIFY(wkbCurvePolygon), true},
    {STRINGIFY(wkbMultiCurve), true},
    {STRINGIFY(wkbMultiSurface), true},
    {STRINGIFY(wkbCurve), true},
    {STRINGIFY(wkbSurface), true},
    {STRINGIFY(wkbPolyhedralSurface), true},
    {STRINGIFY(wkbTIN), true},
    {STRINGIFY(wkbTriangle), true},

    {STRINGIFY(wkbNone), false},
    {STRINGIFY(wkbLinearRing), false},
};

OGRwkbGeometryType OGRVRTGetGeometryType(const char *pszGType, int *pbError)
{
    if (pbError)
        *pbError = FALSE;

    for (const auto &entry : asGeomTypeNames)
    {
        if (EQUALN(pszGType, entry.pszName, strlen(entry.pszName)))
        {
            OGRwkbGeometryType eGeomType = entry.eType;

            if (strstr(pszGType, "25D") != nullptr ||
                strstr(pszGType, "Z") != nullptr)
                eGeomType = wkbSetZ(eGeomType);
            if (pszGType[strlen(pszGType) - 1] == 'M' ||
                pszGType[strlen(pszGType) - 2] == 'M')
                eGeomType = wkbSetM(eGeomType);
            return eGeomType;
        }
    }

    if (pbError)
        *pbError = TRUE;
    return wkbUnknown;
}

/************************************************************************/
/*                     OGRVRTGetSerializedGeometryType()                */
/************************************************************************/

std::string OGRVRTGetSerializedGeometryType(OGRwkbGeometryType eGeomType)
{
    for (const auto &entry : asGeomTypeNames)
    {
        if (entry.eType == wkbFlatten(eGeomType))
        {
            std::string osRet(entry.pszName);
            if (entry.bIsoFlags || OGR_GT_HasM(eGeomType))
            {
                if (OGR_GT_HasZ(eGeomType))
                {
                    osRet += "Z";
                }
                if (OGR_GT_HasM(eGeomType))
                {
                    osRet += "M";
                }
            }
            else if (OGR_GT_HasZ(eGeomType))
            {
                osRet += "25D";
            }
            return osRet;
        }
    }
    return std::string();
}
