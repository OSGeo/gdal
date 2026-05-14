/******************************************************************************
 *
 * Project:  KML Driver
 * Purpose:  Implementation of OGR -> KML geometries writer.
 * Author:   Christopher Condit, condit@sdsc.edu
 *
 ******************************************************************************
 * Copyright (c) 2006, Christopher Condit
 * Copyright (c) 2007-2010, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include "cpl_port.h"
#include "ogr_api.h"

#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <algorithm>
#include <cmath>

#include "cpl_conv.h"
#include "cpl_error.h"
#include "cpl_minixml.h"
#include "ogr_core.h"
#include "ogr_geometry.h"
#include "ogr_p.h"

/************************************************************************/
/*                         MakeKMLCoordinate()                          */
/************************************************************************/

static bool MakeKMLCoordinate(char *pszTarget, size_t /* nTargetLen*/, double x,
                              double y, double z, bool b3D)

{
    constexpr double EPSILON = 1e-8;

    if (y < -90 || y > 90)
    {
        if (y > 90 && y < 90 + EPSILON)
        {
            y = 90;
        }
        else if (y > -90 - EPSILON && y < -90)
        {
            y = -90;
        }
        else
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Latitude %f is invalid. Valid range is [-90,90].", y);
            return false;
        }
    }

    if (x < -180 || x > 180)
    {
        if (x > 180 && x < 180 + EPSILON)
        {
            x = 180;
        }
        else if (x > -180 - EPSILON && x < -180)
        {
            x = -180;
        }
        else
        {
            static bool bFirstWarning = true;
            if (bFirstWarning)
            {
                CPLError(CE_Warning, CPLE_AppDefined,
                         "Longitude %f has been modified to fit into "
                         "range [-180,180]. This warning will not be "
                         "issued any more",
                         x);
                bFirstWarning = false;
            }

            // Trash drastically non-sensical values.
            if (x > 1.0e6 || x < -1.0e6 || std::isnan(x))
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "Longitude %lf is unreasonable.", x);
                return false;
            }

            if (x > 180)
                x -= (static_cast<int>((x + 180) / 360) * 360);
            else if (x < -180)
                x += (static_cast<int>(180 - x) / 360) * 360;
        }
    }

    OGRMakeWktCoordinate(pszTarget, x, y, z, b3D ? 3 : 2);
    while (*pszTarget != '\0')
    {
        if (*pszTarget == ' ')
            *pszTarget = ',';
        pszTarget++;
    }

    return true;
}

/************************************************************************/
/*                            _GrowBuffer()                             */
/************************************************************************/

static void _GrowBuffer(size_t nNeeded, char **ppszText, size_t *pnMaxLength)

{
    if (nNeeded + 1 >= *pnMaxLength)
    {
        *pnMaxLength = std::max(*pnMaxLength * 2, nNeeded + 1);
        *ppszText = static_cast<char *>(CPLRealloc(*ppszText, *pnMaxLength));
    }
}

/************************************************************************/
/*                            AppendString()                            */
/************************************************************************/

static void AppendString(char **ppszText, size_t *pnLength, size_t *pnMaxLength,
                         const char *pszTextToAppend)

{
    _GrowBuffer(*pnLength + strlen(pszTextToAppend) + 1, ppszText, pnMaxLength);

    strcat(*ppszText + *pnLength, pszTextToAppend);
    *pnLength += strlen(*ppszText + *pnLength);
}

/************************************************************************/
/*                        AppendCoordinateList()                        */
/************************************************************************/

static bool AppendCoordinateList(const OGRLineString *poLine, char **ppszText,
                                 size_t *pnLength, size_t *pnMaxLength)

{
    char szCoordinate[256] = {0};
    const bool b3D = wkbHasZ(poLine->getGeometryType());

    AppendString(ppszText, pnLength, pnMaxLength, "<coordinates>");

    for (int iPoint = 0; iPoint < poLine->getNumPoints(); iPoint++)
    {
        if (!MakeKMLCoordinate(szCoordinate, sizeof(szCoordinate),
                               poLine->getX(iPoint), poLine->getY(iPoint),
                               poLine->getZ(iPoint), b3D))
        {
            return false;
        }

        if (iPoint > 0)
            AppendString(ppszText, pnLength, pnMaxLength, " ");
        AppendString(ppszText, pnLength, pnMaxLength, szCoordinate);
    }

    AppendString(ppszText, pnLength, pnMaxLength, "</coordinates>");

    return true;
}

/************************************************************************/
/*                       OGR2KMLGeometryAppend()                        */
/************************************************************************/

static bool OGR2KMLGeometryAppend(const OGRGeometry *poGeometry,
                                  char **ppszText, size_t *pnLength,
                                  size_t *pnMaxLength, char *szAltitudeMode)

{
    const auto eGeomType = poGeometry->getGeometryType();

    /* -------------------------------------------------------------------- */
    /*      2D Point                                                        */
    /* -------------------------------------------------------------------- */
    if (eGeomType == wkbPoint)
    {
        const OGRPoint *poPoint = poGeometry->toPoint();

        if (poPoint->IsEmpty())
        {
            AppendString(ppszText, pnLength, pnMaxLength, "<Point/>");
        }
        else
        {
            char szCoordinate[256] = {0};
            if (!MakeKMLCoordinate(szCoordinate, sizeof(szCoordinate),
                                   poPoint->getX(), poPoint->getY(), 0.0,
                                   false))
            {
                return false;
            }

            AppendString(ppszText, pnLength, pnMaxLength,
                         "<Point><coordinates>");
            AppendString(ppszText, pnLength, pnMaxLength, szCoordinate);
            AppendString(ppszText, pnLength, pnMaxLength,
                         "</coordinates></Point>");
        }
    }
    /* -------------------------------------------------------------------- */
    /*      3D Point                                                        */
    /* -------------------------------------------------------------------- */
    else if (eGeomType == wkbPoint25D)
    {
        char szCoordinate[256] = {0};
        const OGRPoint *poPoint = poGeometry->toPoint();

        if (!MakeKMLCoordinate(szCoordinate, sizeof(szCoordinate),
                               poPoint->getX(), poPoint->getY(),
                               poPoint->getZ(), true))
        {
            return false;
        }

        AppendString(ppszText, pnLength, pnMaxLength, "<Point>");
        if (szAltitudeMode)
            AppendString(ppszText, pnLength, pnMaxLength, szAltitudeMode);
        AppendString(ppszText, pnLength, pnMaxLength, "<coordinates>");
        AppendString(ppszText, pnLength, pnMaxLength, szCoordinate);
        AppendString(ppszText, pnLength, pnMaxLength, "</coordinates></Point>");
    }
    /* -------------------------------------------------------------------- */
    /*      LineString and LinearRing                                       */
    /* -------------------------------------------------------------------- */
    else if (eGeomType == wkbLineString || eGeomType == wkbLineString25D)
    {
        const bool bRing = EQUAL(poGeometry->getGeometryName(), "LINEARRING");

        if (bRing)
            AppendString(ppszText, pnLength, pnMaxLength, "<LinearRing>");
        else
            AppendString(ppszText, pnLength, pnMaxLength, "<LineString>");

        if (nullptr != szAltitudeMode)
        {
            AppendString(ppszText, pnLength, pnMaxLength, szAltitudeMode);
        }

        if (!AppendCoordinateList(poGeometry->toLineString(), ppszText,
                                  pnLength, pnMaxLength))
        {
            return false;
        }

        if (bRing)
            AppendString(ppszText, pnLength, pnMaxLength, "</LinearRing>");
        else
            AppendString(ppszText, pnLength, pnMaxLength, "</LineString>");
    }

    /* -------------------------------------------------------------------- */
    /*      Polygon                                                         */
    /* -------------------------------------------------------------------- */
    else if (eGeomType == wkbPolygon || eGeomType == wkbPolygon25D)
    {
        const OGRPolygon *poPolygon = poGeometry->toPolygon();

        AppendString(ppszText, pnLength, pnMaxLength, "<Polygon>");

        if (nullptr != szAltitudeMode)
        {
            AppendString(ppszText, pnLength, pnMaxLength, szAltitudeMode);
        }

        if (poPolygon->getExteriorRing() != nullptr)
        {
            AppendString(ppszText, pnLength, pnMaxLength, "<outerBoundaryIs>");

            if (!OGR2KMLGeometryAppend(poPolygon->getExteriorRing(), ppszText,
                                       pnLength, pnMaxLength, szAltitudeMode))
            {
                return false;
            }
            AppendString(ppszText, pnLength, pnMaxLength, "</outerBoundaryIs>");
        }

        for (int iRing = 0; iRing < poPolygon->getNumInteriorRings(); iRing++)
        {
            const OGRLinearRing *poRing = poPolygon->getInteriorRing(iRing);

            AppendString(ppszText, pnLength, pnMaxLength, "<innerBoundaryIs>");

            if (!OGR2KMLGeometryAppend(poRing, ppszText, pnLength, pnMaxLength,
                                       szAltitudeMode))
            {
                return false;
            }
            AppendString(ppszText, pnLength, pnMaxLength, "</innerBoundaryIs>");
        }

        AppendString(ppszText, pnLength, pnMaxLength, "</Polygon>");
    }

    /* -------------------------------------------------------------------- */
    /*      MultiPolygon                                                    */
    /* -------------------------------------------------------------------- */
    else if (wkbFlatten(eGeomType) == wkbMultiPolygon ||
             wkbFlatten(eGeomType) == wkbMultiLineString ||
             wkbFlatten(eGeomType) == wkbMultiPoint ||
             wkbFlatten(eGeomType) == wkbGeometryCollection)
    {
        const OGRGeometryCollection *poGC = poGeometry->toGeometryCollection();

        AppendString(ppszText, pnLength, pnMaxLength, "<MultiGeometry>");

        for (const auto *poMember : *poGC)
        {
            if (!OGR2KMLGeometryAppend(poMember, ppszText, pnLength,
                                       pnMaxLength, szAltitudeMode))
            {
                return false;
            }
        }
        AppendString(ppszText, pnLength, pnMaxLength, "</MultiGeometry>");
    }
    else
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "Unsupported geometry type in KML: %s",
                 OGRGeometryTypeToName(eGeomType));
        return false;
    }

    return true;
}

/************************************************************************/
/*                         OGR_G_ExportToKML()                          */
/************************************************************************/

/**
 * \brief Convert a geometry into KML format.
 *
 * The returned string should be freed with CPLFree() when no longer required.
 *
 * This method is the same as the C++ method OGRGeometry::exportToKML().
 *
 * @param hGeometry handle to the geometry.
 * @param pszAltitudeMode value to write in altitudeMode element, or NULL.
 * @return A KML fragment or NULL in case of error.
 */

char *OGR_G_ExportToKML(OGRGeometryH hGeometry, const char *pszAltitudeMode)
{
    char szAltitudeMode[128];

    if (hGeometry == nullptr)
        return nullptr;

    size_t nMaxLength = 128;
    char *pszText = static_cast<char *>(CPLMalloc(nMaxLength));
    pszText[0] = '\0';

    if (pszAltitudeMode &&
        strlen(pszAltitudeMode) < sizeof(szAltitudeMode) - (29 + 1))
    {
        snprintf(szAltitudeMode, sizeof(szAltitudeMode),
                 "<altitudeMode>%s</altitudeMode>", pszAltitudeMode);
    }
    else
    {
        szAltitudeMode[0] = 0;
    }

    size_t nLength = 0;
    if (!OGR2KMLGeometryAppend(OGRGeometry::FromHandle(hGeometry), &pszText,
                               &nLength, &nMaxLength, szAltitudeMode))
    {
        CPLFree(pszText);
        return nullptr;
    }

    return pszText;
}
