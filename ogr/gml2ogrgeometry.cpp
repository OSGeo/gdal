/******************************************************************************
 *
 * Project:  GML Reader
 * Purpose:  Code to translate between GML and OGR geometry forms.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2002, Frank Warmerdam
 * Copyright (c) 2009-2014, Even Rouault <even dot rouault at spatialys.com>
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
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 *****************************************************************************
 *
 * Independent Security Audit 2003/04/17 Andrey Kiselev:
 *   Completed audit of this module. All functions may be used without buffer
 *   overflows and stack corruptions with any kind of input data.
 *
 * Security Audit 2003/03/28 warmerda:
 *   Completed security audit.  I believe that this module may be safely used
 *   to parse, arbitrary GML potentially provided by a hostile source without
 *   compromising the system.
 *
 */

#include "cpl_port.h"
#include "ogr_api.h"

#include <algorithm>
#include <cassert>
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <cstring>

#include "cpl_conv.h"
#include "cpl_error.h"
#include "cpl_minixml.h"
#include "cpl_string.h"
#include "ogr_core.h"
#include "ogr_geometry.h"
#include "ogr_p.h"
#include "ogr_spatialref.h"
#include "ogr_srs_api.h"
#include "ogr_geo_utils.h"

constexpr double kdfD2R = M_PI / 180.0;
constexpr double kdf2PI = 2.0 * M_PI;

/************************************************************************/
/*                        GMLGetCoordTokenPos()                         */
/************************************************************************/

static const char *GMLGetCoordTokenPos(const char *pszStr,
                                       const char **ppszNextToken)
{
    char ch;
    while (true)
    {
        // cppcheck-suppress nullPointerRedundantCheck
        ch = *pszStr;
        if (ch == '\0')
        {
            *ppszNextToken = pszStr;
            return nullptr;
        }
        else if (!(ch == '\n' || ch == '\r' || ch == '\t' || ch == ' ' ||
                   ch == ','))
            break;
        pszStr++;
    }

    const char *pszToken = pszStr;
    while ((ch = *pszStr) != '\0')
    {
        if (ch == '\n' || ch == '\r' || ch == '\t' || ch == ' ' || ch == ',')
        {
            *ppszNextToken = pszStr;
            return pszToken;
        }
        pszStr++;
    }
    *ppszNextToken = pszStr;
    return pszToken;
}

/************************************************************************/
/*                           BareGMLElement()                           */
/*                                                                      */
/*      Returns the passed string with any namespace prefix             */
/*      stripped off.                                                   */
/************************************************************************/

static const char *BareGMLElement(const char *pszInput)

{
    const char *pszReturn = strchr(pszInput, ':');
    if (pszReturn == nullptr)
        pszReturn = pszInput;
    else
        pszReturn++;

    return pszReturn;
}

/************************************************************************/
/*                          FindBareXMLChild()                          */
/*                                                                      */
/*      Find a child node with the indicated "bare" name, that is       */
/*      after any namespace qualifiers have been stripped off.          */
/************************************************************************/

static const CPLXMLNode *FindBareXMLChild(const CPLXMLNode *psParent,
                                          const char *pszBareName)

{
    const CPLXMLNode *psCandidate = psParent->psChild;

    while (psCandidate != nullptr)
    {
        if (psCandidate->eType == CXT_Element &&
            EQUAL(BareGMLElement(psCandidate->pszValue), pszBareName))
            return psCandidate;

        psCandidate = psCandidate->psNext;
    }

    return nullptr;
}

/************************************************************************/
/*                           GetElementText()                           */
/************************************************************************/

static const char *GetElementText(const CPLXMLNode *psElement)

{
    if (psElement == nullptr)
        return nullptr;

    const CPLXMLNode *psChild = psElement->psChild;

    while (psChild != nullptr)
    {
        if (psChild->eType == CXT_Text)
            return psChild->pszValue;

        psChild = psChild->psNext;
    }

    return nullptr;
}

/************************************************************************/
/*                           GetChildElement()                          */
/************************************************************************/

static const CPLXMLNode *GetChildElement(const CPLXMLNode *psElement)

{
    if (psElement == nullptr)
        return nullptr;

    const CPLXMLNode *psChild = psElement->psChild;

    while (psChild != nullptr)
    {
        if (psChild->eType == CXT_Element)
            return psChild;

        psChild = psChild->psNext;
    }

    return nullptr;
}

/************************************************************************/
/*                    GetElementOrientation()                           */
/*     Returns true for positive orientation.                           */
/************************************************************************/

static bool GetElementOrientation(const CPLXMLNode *psElement)
{
    if (psElement == nullptr)
        return true;

    const CPLXMLNode *psChild = psElement->psChild;

    while (psChild != nullptr)
    {
        if (psChild->eType == CXT_Attribute &&
            EQUAL(psChild->pszValue, "orientation"))
            return EQUAL(psChild->psChild->pszValue, "+");

        psChild = psChild->psNext;
    }

    return true;
}

/************************************************************************/
/*                              AddPoint()                              */
/*                                                                      */
/*      Add a point to the passed geometry.                             */
/************************************************************************/

static bool AddPoint(OGRGeometry *poGeometry, double dfX, double dfY,
                     double dfZ, int nDimension)

{
    const OGRwkbGeometryType eType = wkbFlatten(poGeometry->getGeometryType());
    if (eType == wkbPoint)
    {
        OGRPoint *poPoint = poGeometry->toPoint();

        if (!poPoint->IsEmpty())
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "More than one coordinate for <Point> element.");
            return false;
        }

        poPoint->setX(dfX);
        poPoint->setY(dfY);
        if (nDimension == 3)
            poPoint->setZ(dfZ);

        return true;
    }
    else if (eType == wkbLineString || eType == wkbCircularString)
    {
        OGRSimpleCurve *poCurve = poGeometry->toSimpleCurve();
        if (nDimension == 3)
            poCurve->addPoint(dfX, dfY, dfZ);
        else
            poCurve->addPoint(dfX, dfY);

        return true;
    }

    CPLAssert(false);
    return false;
}

/************************************************************************/
/*                        ParseGMLCoordinates()                         */
/************************************************************************/

static bool ParseGMLCoordinates(const CPLXMLNode *psGeomNode,
                                OGRGeometry *poGeometry, int nSRSDimension)

{
    const CPLXMLNode *psCoordinates =
        FindBareXMLChild(psGeomNode, "coordinates");

    /* -------------------------------------------------------------------- */
    /*      Handle <coordinates> case.                                      */
    /*      Note that we don't do a strict validation, so we accept and     */
    /*      sometimes generate output whereas we should just reject it.     */
    /* -------------------------------------------------------------------- */
    if (psCoordinates != nullptr)
    {
        const char *pszCoordString = GetElementText(psCoordinates);

        const char *pszDecimal =
            CPLGetXMLValue(psCoordinates, "decimal", nullptr);
        char chDecimal = '.';
        if (pszDecimal != nullptr)
        {
            if (strlen(pszDecimal) != 1 ||
                (pszDecimal[0] >= '0' && pszDecimal[0] <= '9'))
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "Wrong value for decimal attribute");
                return false;
            }
            chDecimal = pszDecimal[0];
        }

        const char *pszCS = CPLGetXMLValue(psCoordinates, "cs", nullptr);
        char chCS = ',';
        if (pszCS != nullptr)
        {
            if (strlen(pszCS) != 1 || (pszCS[0] >= '0' && pszCS[0] <= '9'))
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "Wrong value for cs attribute");
                return false;
            }
            chCS = pszCS[0];
        }
        const char *pszTS = CPLGetXMLValue(psCoordinates, "ts", nullptr);
        char chTS = ' ';
        if (pszTS != nullptr)
        {
            if (strlen(pszTS) != 1 || (pszTS[0] >= '0' && pszTS[0] <= '9'))
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "Wrong value for ts attribute");
                return false;
            }
            chTS = pszTS[0];
        }

        if (pszCoordString == nullptr)
        {
            poGeometry->empty();
            return true;
        }

        // Skip leading whitespace. See
        // https://github.com/OSGeo/gdal/issues/5494
        while (*pszCoordString != '\0' &&
               isspace(static_cast<unsigned char>(*pszCoordString)))
        {
            pszCoordString++;
        }

        int iCoord = 0;
        const OGRwkbGeometryType eType =
            wkbFlatten(poGeometry->getGeometryType());
        OGRSimpleCurve *poCurve =
            (eType == wkbLineString || eType == wkbCircularString)
                ? poGeometry->toSimpleCurve()
                : nullptr;
        for (int iter = (eType == wkbPoint ? 1 : 0); iter < 2; iter++)
        {
            const char *pszStr = pszCoordString;
            double dfX = 0;
            double dfY = 0;
            iCoord = 0;
            while (*pszStr != '\0')
            {
                int nDimension = 2;
                // parse out 2 or 3 tuple.
                if (iter == 1)
                {
                    if (chDecimal == '.')
                        dfX = OGRFastAtof(pszStr);
                    else
                        dfX = CPLAtofDelim(pszStr, chDecimal);
                }
                while (*pszStr != '\0' && *pszStr != chCS &&
                       !isspace(static_cast<unsigned char>(*pszStr)))
                    pszStr++;

                if (*pszStr == '\0')
                {
                    CPLError(CE_Failure, CPLE_AppDefined,
                             "Corrupt <coordinates> value.");
                    return false;
                }
                else if (chCS == ',' && pszCS == nullptr &&
                         isspace(static_cast<unsigned char>(*pszStr)))
                {
                    // In theory, the coordinates inside a coordinate tuple
                    // should be separated by a comma. However it has been found
                    // in the wild that the coordinates are in rare cases
                    // separated by a space, and the tuples by a comma. See:
                    // https://52north.org/twiki/bin/view/Processing/WPS-IDWExtension-ObservationCollectionExample
                    // or
                    // http://agisdemo.faa.gov/aixmServices/getAllFeaturesByLocatorId?locatorId=DFW
                    chCS = ' ';
                    chTS = ',';
                }

                pszStr++;

                if (iter == 1)
                {
                    if (chDecimal == '.')
                        dfY = OGRFastAtof(pszStr);
                    else
                        dfY = CPLAtofDelim(pszStr, chDecimal);
                }
                while (*pszStr != '\0' && *pszStr != chCS && *pszStr != chTS &&
                       !isspace(static_cast<unsigned char>(*pszStr)))
                    pszStr++;

                double dfZ = 0.0;
                if (*pszStr == chCS)
                {
                    pszStr++;
                    if (iter == 1)
                    {
                        if (chDecimal == '.')
                            dfZ = OGRFastAtof(pszStr);
                        else
                            dfZ = CPLAtofDelim(pszStr, chDecimal);
                    }
                    nDimension = 3;
                    while (*pszStr != '\0' && *pszStr != chCS &&
                           *pszStr != chTS &&
                           !isspace(static_cast<unsigned char>(*pszStr)))
                        pszStr++;
                }

                if (*pszStr == chTS)
                {
                    pszStr++;
                }

                while (isspace(static_cast<unsigned char>(*pszStr)))
                    pszStr++;

                if (iter == 1)
                {
                    if (poCurve)
                    {
                        if (nDimension == 3)
                            poCurve->setPoint(iCoord, dfX, dfY, dfZ);
                        else
                            poCurve->setPoint(iCoord, dfX, dfY);
                    }
                    else if (!AddPoint(poGeometry, dfX, dfY, dfZ, nDimension))
                        return false;
                }

                iCoord++;
            }

            if (poCurve && iter == 0)
            {
                poCurve->setNumPoints(iCoord);
            }
        }

        return iCoord > 0;
    }

    /* -------------------------------------------------------------------- */
    /*      Is this a "pos"?  GML 3 construct.                              */
    /*      Parse if it exist a series of pos elements (this would allow    */
    /*      the correct parsing of gml3.1.1 geometries such as linestring    */
    /*      defined with pos elements.                                      */
    /* -------------------------------------------------------------------- */
    bool bHasFoundPosElement = false;
    for (const CPLXMLNode *psPos = psGeomNode->psChild; psPos != nullptr;
         psPos = psPos->psNext)
    {
        if (psPos->eType != CXT_Element)
            continue;

        const char *pszSubElement = BareGMLElement(psPos->pszValue);

        if (EQUAL(pszSubElement, "pointProperty"))
        {
            for (const CPLXMLNode *psPointPropertyIter = psPos->psChild;
                 psPointPropertyIter != nullptr;
                 psPointPropertyIter = psPointPropertyIter->psNext)
            {
                if (psPointPropertyIter->eType != CXT_Element)
                    continue;

                const char *pszBareElement =
                    BareGMLElement(psPointPropertyIter->pszValue);
                if (EQUAL(pszBareElement, "Point") ||
                    EQUAL(pszBareElement, "ElevatedPoint"))
                {
                    OGRPoint oPoint;
                    if (ParseGMLCoordinates(psPointPropertyIter, &oPoint,
                                            nSRSDimension))
                    {
                        const bool bSuccess = AddPoint(
                            poGeometry, oPoint.getX(), oPoint.getY(),
                            oPoint.getZ(), oPoint.getCoordinateDimension());
                        if (bSuccess)
                            bHasFoundPosElement = true;
                        else
                            return false;
                    }
                }
            }

            if (psPos->psChild && psPos->psChild->eType == CXT_Attribute &&
                psPos->psChild->psNext == nullptr &&
                strcmp(psPos->psChild->pszValue, "xlink:href") == 0)
            {
                CPLError(CE_Warning, CPLE_AppDefined,
                         "Cannot resolve xlink:href='%s'. "
                         "Try setting GML_SKIP_RESOLVE_ELEMS=NONE",
                         psPos->psChild->psChild->pszValue);
            }

            continue;
        }

        if (!EQUAL(pszSubElement, "pos"))
            continue;

        const char *pszPos = GetElementText(psPos);
        if (pszPos == nullptr)
        {
            poGeometry->empty();
            return true;
        }

        const char *pszCur = pszPos;
        const char *pszX = GMLGetCoordTokenPos(pszCur, &pszCur);
        const char *pszY = (pszCur[0] != '\0')
                               ? GMLGetCoordTokenPos(pszCur, &pszCur)
                               : nullptr;
        const char *pszZ = (pszCur[0] != '\0')
                               ? GMLGetCoordTokenPos(pszCur, &pszCur)
                               : nullptr;

        if (pszY == nullptr)
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Did not get 2+ values in <gml:pos>%s</gml:pos> tuple.",
                     pszPos);
            return false;
        }

        const double dfX = OGRFastAtof(pszX);
        const double dfY = OGRFastAtof(pszY);
        const double dfZ = (pszZ != nullptr) ? OGRFastAtof(pszZ) : 0.0;
        const bool bSuccess =
            AddPoint(poGeometry, dfX, dfY, dfZ, (pszZ != nullptr) ? 3 : 2);

        if (bSuccess)
            bHasFoundPosElement = true;
        else
            return false;
    }

    if (bHasFoundPosElement)
        return true;

    /* -------------------------------------------------------------------- */
    /*      Is this a "posList"?  GML 3 construct (SF profile).             */
    /* -------------------------------------------------------------------- */
    const CPLXMLNode *psPosList = FindBareXMLChild(psGeomNode, "posList");

    if (psPosList != nullptr)
    {
        int nDimension = 2;

        // Try to detect the presence of an srsDimension attribute
        // This attribute is only available for gml3.1.1 but not
        // available for gml3.1 SF.
        const char *pszSRSDimension =
            CPLGetXMLValue(psPosList, "srsDimension", nullptr);
        // If not found at the posList level, try on the enclosing element.
        if (pszSRSDimension == nullptr)
            pszSRSDimension =
                CPLGetXMLValue(psGeomNode, "srsDimension", nullptr);
        if (pszSRSDimension != nullptr)
            nDimension = atoi(pszSRSDimension);
        else if (nSRSDimension != 0)
            // Or use one coming from a still higher level element (#5606).
            nDimension = nSRSDimension;

        if (nDimension != 2 && nDimension != 3)
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "srsDimension = %d not supported", nDimension);
            return false;
        }

        const char *pszPosList = GetElementText(psPosList);
        if (pszPosList == nullptr)
        {
            poGeometry->empty();
            return true;
        }

        bool bSuccess = false;
        const char *pszCur = pszPosList;
        while (true)
        {
            const char *pszX = GMLGetCoordTokenPos(pszCur, &pszCur);
            if (pszX == nullptr && bSuccess)
                break;
            const char *pszY = (pszCur[0] != '\0')
                                   ? GMLGetCoordTokenPos(pszCur, &pszCur)
                                   : nullptr;
            const char *pszZ = (nDimension == 3 && pszCur[0] != '\0')
                                   ? GMLGetCoordTokenPos(pszCur, &pszCur)
                                   : nullptr;

            if (pszY == nullptr || (nDimension == 3 && pszZ == nullptr))
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "Did not get at least %d values or invalid number of "
                         "set of coordinates <gml:posList>%s</gml:posList>",
                         nDimension, pszPosList);
                return false;
            }

            double dfX = OGRFastAtof(pszX);
            double dfY = OGRFastAtof(pszY);
            double dfZ = (pszZ != nullptr) ? OGRFastAtof(pszZ) : 0.0;
            bSuccess = AddPoint(poGeometry, dfX, dfY, dfZ, nDimension);

            if (!bSuccess || pszCur == nullptr)
                break;
        }

        return bSuccess;
    }

    /* -------------------------------------------------------------------- */
    /*      Handle form with a list of <coord> items each with an <X>,      */
    /*      and <Y> element.                                                */
    /* -------------------------------------------------------------------- */
    int iCoord = 0;
    for (const CPLXMLNode *psCoordNode = psGeomNode->psChild;
         psCoordNode != nullptr; psCoordNode = psCoordNode->psNext)
    {
        if (psCoordNode->eType != CXT_Element ||
            !EQUAL(BareGMLElement(psCoordNode->pszValue), "coord"))
            continue;

        const CPLXMLNode *psXNode = FindBareXMLChild(psCoordNode, "X");
        const CPLXMLNode *psYNode = FindBareXMLChild(psCoordNode, "Y");
        const CPLXMLNode *psZNode = FindBareXMLChild(psCoordNode, "Z");

        if (psXNode == nullptr || psYNode == nullptr ||
            GetElementText(psXNode) == nullptr ||
            GetElementText(psYNode) == nullptr ||
            (psZNode != nullptr && GetElementText(psZNode) == nullptr))
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Corrupt <coord> element, missing <X> or <Y> element?");
            return false;
        }

        double dfX = OGRFastAtof(GetElementText(psXNode));
        double dfY = OGRFastAtof(GetElementText(psYNode));

        int nDimension = 2;
        double dfZ = 0.0;
        if (psZNode != nullptr && GetElementText(psZNode) != nullptr)
        {
            dfZ = OGRFastAtof(GetElementText(psZNode));
            nDimension = 3;
        }

        if (!AddPoint(poGeometry, dfX, dfY, dfZ, nDimension))
            return false;

        iCoord++;
    }

    return iCoord > 0;
}

#ifdef HAVE_GEOS
/************************************************************************/
/*                         GML2FaceExtRing()                            */
/*                                                                      */
/*      Identifies the "good" Polygon within the collection returned    */
/*      by GEOSPolygonize()                                             */
/*      short rationale: GEOSPolygonize() will possibly return a        */
/*      collection of many Polygons; only one is the "good" one,        */
/*      (including both exterior- and interior-rings)                   */
/*      any other simply represents a single "hole", and should be      */
/*      consequently ignored at all.                                    */
/************************************************************************/

static std::unique_ptr<OGRPolygon> GML2FaceExtRing(const OGRGeometry *poGeom)
{
    const OGRGeometryCollection *poColl =
        dynamic_cast<const OGRGeometryCollection *>(poGeom);
    if (poColl == nullptr)
    {
        CPLError(CE_Fatal, CPLE_AppDefined,
                 "dynamic_cast failed.  Expected OGRGeometryCollection.");
        return nullptr;
    }

    const OGRPolygon *poPolygonExterior = nullptr;
    const OGRPolygon *poPolygonInterior = nullptr;
    int iExterior = 0;
    int iInterior = 0;

    for (const auto *poChild : *poColl)
    {
        // A collection of Polygons is expected to be found.
        if (wkbFlatten(poChild->getGeometryType()) == wkbPolygon)
        {
            const OGRPolygon *poPoly = poChild->toPolygon();
            if (poPoly->getNumInteriorRings() > 0)
            {
                poPolygonExterior = poPoly;
                iExterior++;
            }
            else
            {
                poPolygonInterior = poPoly;
                iInterior++;
            }
        }
        else
        {
            return nullptr;
        }
    }

    if (poPolygonInterior && iExterior == 0 && iInterior == 1)
    {
        // There is a single Polygon within the collection.
        return std::unique_ptr<OGRPolygon>(poPolygonInterior->clone());
    }
    else if (poPolygonExterior && iExterior == 1 &&
             iInterior == poColl->getNumGeometries() - 1)
    {
        // Return the unique Polygon containing holes.
        return std::unique_ptr<OGRPolygon>(poPolygonExterior->clone());
    }

    return nullptr;
}
#endif

/************************************************************************/
/*                   GML2OGRGeometry_AddToCompositeCurve()              */
/************************************************************************/

static bool
GML2OGRGeometry_AddToCompositeCurve(OGRCompoundCurve *poCC,
                                    std::unique_ptr<OGRGeometry> poGeom,
                                    bool &bChildrenAreAllLineString)
{
    if (poGeom == nullptr || !OGR_GT_IsCurve(poGeom->getGeometryType()))
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "CompositeCurve: Got %.500s geometry as Member instead of a "
                 "curve.",
                 poGeom ? poGeom->getGeometryName() : "NULL");
        return false;
    }

    // Crazy but allowed by GML: composite in composite.
    if (wkbFlatten(poGeom->getGeometryType()) == wkbCompoundCurve)
    {
        auto poCCChild = std::unique_ptr<OGRCompoundCurve>(
            poGeom.release()->toCompoundCurve());
        while (poCCChild->getNumCurves() != 0)
        {
            auto poCurve = std::unique_ptr<OGRCurve>(poCCChild->stealCurve(0));
            if (wkbFlatten(poCurve->getGeometryType()) != wkbLineString)
                bChildrenAreAllLineString = false;
            if (poCC->addCurve(std::move(poCurve)) != OGRERR_NONE)
            {
                return false;
            }
        }
    }
    else
    {
        if (wkbFlatten(poGeom->getGeometryType()) != wkbLineString)
            bChildrenAreAllLineString = false;

        auto poCurve = std::unique_ptr<OGRCurve>(poGeom.release()->toCurve());
        if (poCC->addCurve(std::move(poCurve)) != OGRERR_NONE)
        {
            return false;
        }
    }
    return true;
}

/************************************************************************/
/*                   GML2OGRGeometry_AddToMultiSurface()                */
/************************************************************************/

static bool GML2OGRGeometry_AddToMultiSurface(
    OGRMultiSurface *poMS, std::unique_ptr<OGRGeometry> poGeom,
    const char *pszMemberElement, bool &bChildrenAreAllPolygons)
{
    if (poGeom == nullptr)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Invalid %s", pszMemberElement);
        return false;
    }

    OGRwkbGeometryType eType = wkbFlatten(poGeom->getGeometryType());
    if (eType == wkbPolygon || eType == wkbCurvePolygon)
    {
        if (eType != wkbPolygon)
            bChildrenAreAllPolygons = false;

        if (poMS->addGeometry(std::move(poGeom)) != OGRERR_NONE)
        {
            return false;
        }
    }
    else if (eType == wkbMultiPolygon || eType == wkbMultiSurface)
    {
        OGRMultiSurface *poMS2 = poGeom->toMultiSurface();
        for (int i = 0; i < poMS2->getNumGeometries(); i++)
        {
            if (wkbFlatten(poMS2->getGeometryRef(i)->getGeometryType()) !=
                wkbPolygon)
                bChildrenAreAllPolygons = false;

            if (poMS->addGeometry(poMS2->getGeometryRef(i)) != OGRERR_NONE)
            {
                return false;
            }
        }
    }
    else
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Got %.500s geometry as %s.",
                 poGeom->getGeometryName(), pszMemberElement);
        return false;
    }
    return true;
}

/************************************************************************/
/*                        GetDistanceInMetre()                          */
/************************************************************************/

static double GetDistanceInMetre(double dfDistance, const char *pszUnits)
{
    if (EQUAL(pszUnits, "m"))
        return dfDistance;

    if (EQUAL(pszUnits, "km"))
        return dfDistance * 1000;

    if (EQUAL(pszUnits, "nm") || EQUAL(pszUnits, "[nmi_i]"))
        return dfDistance * CPLAtof(SRS_UL_INTL_NAUT_MILE_CONV);

    if (EQUAL(pszUnits, "mi"))
        return dfDistance * CPLAtof(SRS_UL_INTL_STAT_MILE_CONV);

    if (EQUAL(pszUnits, "ft"))
        return dfDistance * CPLAtof(SRS_UL_INTL_FOOT_CONV);

    CPLDebug("GML2OGRGeometry", "Unhandled unit: %s", pszUnits);
    return -1;
}

/************************************************************************/
/*                      GML2OGRGeometry_XMLNode()                       */
/*                                                                      */
/*      Translates the passed XMLnode and its children into an         */
/*      OGRGeometry.  This is used recursively for geometry             */
/*      collections.                                                    */
/************************************************************************/

static std::unique_ptr<OGRGeometry> GML2OGRGeometry_XMLNode_Internal(
    const CPLXMLNode *psNode, int nPseudoBoolGetSecondaryGeometryOption,
    int nRecLevel, int nSRSDimension, const char *pszSRSName,
    bool bIgnoreGSG = false, bool bOrientation = true,
    bool bFaceHoleNegative = false);

OGRGeometry *GML2OGRGeometry_XMLNode(const CPLXMLNode *psNode,
                                     int nPseudoBoolGetSecondaryGeometryOption,
                                     int nRecLevel, int nSRSDimension,
                                     bool bIgnoreGSG, bool bOrientation,
                                     bool bFaceHoleNegative)

{
    return GML2OGRGeometry_XMLNode_Internal(
               psNode, nPseudoBoolGetSecondaryGeometryOption, nRecLevel,
               nSRSDimension, nullptr, bIgnoreGSG, bOrientation,
               bFaceHoleNegative)
        .release();
}

static std::unique_ptr<OGRGeometry> GML2OGRGeometry_XMLNode_Internal(
    const CPLXMLNode *psNode, int nPseudoBoolGetSecondaryGeometryOption,
    int nRecLevel, int nSRSDimension, const char *pszSRSName, bool bIgnoreGSG,
    bool bOrientation, bool bFaceHoleNegative)
{
    // constexpr bool bCastToLinearTypeIfPossible = true;  // Hard-coded for
    // now.

    // We need this nRecLevel == 0 check, otherwise this could result in
    // multiple revist of the same node, and exponential complexity.
    if (nRecLevel == 0 && psNode != nullptr &&
        strcmp(psNode->pszValue, "?xml") == 0)
        psNode = psNode->psNext;
    while (psNode != nullptr && psNode->eType == CXT_Comment)
        psNode = psNode->psNext;
    if (psNode == nullptr)
        return nullptr;

    const char *pszSRSDimension =
        CPLGetXMLValue(psNode, "srsDimension", nullptr);
    if (pszSRSDimension != nullptr)
        nSRSDimension = atoi(pszSRSDimension);

    if (pszSRSName == nullptr)
        pszSRSName = CPLGetXMLValue(psNode, "srsName", nullptr);

    const char *pszBaseGeometry = BareGMLElement(psNode->pszValue);
    if (nPseudoBoolGetSecondaryGeometryOption < 0)
        nPseudoBoolGetSecondaryGeometryOption =
            CPLTestBool(CPLGetConfigOption("GML_GET_SECONDARY_GEOM", "NO"));
    bool bGetSecondaryGeometry =
        bIgnoreGSG ? false : CPL_TO_BOOL(nPseudoBoolGetSecondaryGeometryOption);

    // Arbitrary value, but certainly large enough for reasonable usages.
    if (nRecLevel == 32)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Too many recursion levels (%d) while parsing GML geometry.",
                 nRecLevel);
        return nullptr;
    }

    if (bGetSecondaryGeometry)
        if (!(EQUAL(pszBaseGeometry, "directedEdge") ||
              EQUAL(pszBaseGeometry, "TopoCurve")))
            return nullptr;

    /* -------------------------------------------------------------------- */
    /*      Polygon / PolygonPatch / Rectangle                              */
    /* -------------------------------------------------------------------- */
    if (EQUAL(pszBaseGeometry, "Polygon") ||
        EQUAL(pszBaseGeometry, "PolygonPatch") ||
        EQUAL(pszBaseGeometry, "Rectangle"))
    {
        // Find outer ring.
        const CPLXMLNode *psChild = FindBareXMLChild(psNode, "outerBoundaryIs");
        if (psChild == nullptr)
            psChild = FindBareXMLChild(psNode, "exterior");

        psChild = GetChildElement(psChild);
        if (psChild == nullptr)
        {
            // <gml:Polygon/> is invalid GML2, but valid GML3, so be tolerant.
            return std::make_unique<OGRPolygon>();
        }

        // Translate outer ring and add to polygon.
        auto poGeom = GML2OGRGeometry_XMLNode_Internal(
            psChild, nPseudoBoolGetSecondaryGeometryOption, nRecLevel + 1,
            nSRSDimension, pszSRSName);
        if (poGeom == nullptr)
        {
            CPLError(CE_Failure, CPLE_AppDefined, "Invalid exterior ring");
            return nullptr;
        }

        if (!OGR_GT_IsCurve(poGeom->getGeometryType()))
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "%s: Got %.500s geometry as outerBoundaryIs.",
                     pszBaseGeometry, poGeom->getGeometryName());
            return nullptr;
        }

        if (wkbFlatten(poGeom->getGeometryType()) == wkbLineString &&
            !EQUAL(poGeom->getGeometryName(), "LINEARRING"))
        {
            OGRCurve *poCurve = poGeom.release()->toCurve();
            auto poLinearRing = OGRCurve::CastToLinearRing(poCurve);
            if (!poLinearRing)
                return nullptr;
            poGeom.reset(poLinearRing);
        }

        std::unique_ptr<OGRCurvePolygon> poCP;
        bool bIsPolygon = false;
        assert(poGeom);  // to please cppcheck
        if (EQUAL(poGeom->getGeometryName(), "LINEARRING"))
        {
            poCP = std::make_unique<OGRPolygon>();
            bIsPolygon = true;
        }
        else
        {
            poCP = std::make_unique<OGRCurvePolygon>();
            bIsPolygon = false;
        }

        {
            auto poCurve =
                std::unique_ptr<OGRCurve>(poGeom.release()->toCurve());
            if (poCP->addRing(std::move(poCurve)) != OGRERR_NONE)
            {
                return nullptr;
            }
        }

        // Find all inner rings
        for (psChild = psNode->psChild; psChild != nullptr;
             psChild = psChild->psNext)
        {
            if (psChild->eType == CXT_Element &&
                (EQUAL(BareGMLElement(psChild->pszValue), "innerBoundaryIs") ||
                 EQUAL(BareGMLElement(psChild->pszValue), "interior")))
            {
                const CPLXMLNode *psInteriorChild = GetChildElement(psChild);
                std::unique_ptr<OGRGeometry> poGeomInterior;
                if (psInteriorChild != nullptr)
                    poGeomInterior = GML2OGRGeometry_XMLNode_Internal(
                        psInteriorChild, nPseudoBoolGetSecondaryGeometryOption,
                        nRecLevel + 1, nSRSDimension, pszSRSName);
                if (poGeomInterior == nullptr)
                {
                    CPLError(CE_Failure, CPLE_AppDefined,
                             "Invalid interior ring");
                    return nullptr;
                }

                if (!OGR_GT_IsCurve(poGeomInterior->getGeometryType()))
                {
                    CPLError(CE_Failure, CPLE_AppDefined,
                             "%s: Got %.500s geometry as innerBoundaryIs.",
                             pszBaseGeometry,
                             poGeomInterior->getGeometryName());
                    return nullptr;
                }

                if (bIsPolygon)
                {
                    if (!EQUAL(poGeomInterior->getGeometryName(), "LINEARRING"))
                    {
                        if (wkbFlatten(poGeomInterior->getGeometryType()) ==
                            wkbLineString)
                        {
                            OGRLineString *poLS =
                                poGeomInterior.release()->toLineString();
                            auto poLinearRing =
                                OGRCurve::CastToLinearRing(poLS);
                            if (!poLinearRing)
                                return nullptr;
                            poGeomInterior.reset(poLinearRing);
                        }
                        else
                        {
                            // Might fail if some rings are not closed.
                            // We used to be tolerant about that with Polygon.
                            // but we have become stricter with CurvePolygon.
                            auto poCPNew = std::unique_ptr<OGRCurvePolygon>(
                                OGRSurface::CastToCurvePolygon(poCP.release()));
                            if (!poCPNew)
                            {
                                return nullptr;
                            }
                            poCP = std::move(poCPNew);
                            bIsPolygon = false;
                        }
                    }
                }
                else
                {
                    if (EQUAL(poGeomInterior->getGeometryName(), "LINEARRING"))
                    {
                        OGRCurve *poCurve = poGeomInterior.release()->toCurve();
                        poGeomInterior.reset(
                            OGRCurve::CastToLineString(poCurve));
                    }
                }
                auto poCurve = std::unique_ptr<OGRCurve>(
                    poGeomInterior.release()->toCurve());
                if (poCP->addRing(std::move(poCurve)) != OGRERR_NONE)
                {
                    return nullptr;
                }
            }
        }

        return poCP;
    }

    /* -------------------------------------------------------------------- */
    /*      Triangle                                                        */
    /* -------------------------------------------------------------------- */

    if (EQUAL(pszBaseGeometry, "Triangle"))
    {
        // Find outer ring.
        const CPLXMLNode *psChild = FindBareXMLChild(psNode, "exterior");
        if (!psChild)
            return nullptr;

        psChild = GetChildElement(psChild);
        if (psChild == nullptr)
        {
            CPLError(CE_Failure, CPLE_AppDefined, "Empty Triangle");
            return std::make_unique<OGRTriangle>();
        }

        // Translate outer ring and add to Triangle.
        auto poGeom = GML2OGRGeometry_XMLNode_Internal(
            psChild, nPseudoBoolGetSecondaryGeometryOption, nRecLevel + 1,
            nSRSDimension, pszSRSName);
        if (poGeom == nullptr)
        {
            CPLError(CE_Failure, CPLE_AppDefined, "Invalid exterior ring");
            return nullptr;
        }

        if (!OGR_GT_IsCurve(poGeom->getGeometryType()))
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "%s: Got %.500s geometry as outerBoundaryIs.",
                     pszBaseGeometry, poGeom->getGeometryName());
            return nullptr;
        }

        if (wkbFlatten(poGeom->getGeometryType()) == wkbLineString &&
            !EQUAL(poGeom->getGeometryName(), "LINEARRING"))
        {
            poGeom.reset(
                OGRCurve::CastToLinearRing(poGeom.release()->toCurve()));
        }

        if (poGeom == nullptr ||
            !EQUAL(poGeom->getGeometryName(), "LINEARRING"))
        {
            return nullptr;
        }

        auto poTriangle = std::make_unique<OGRTriangle>();
        auto poCurve = std::unique_ptr<OGRCurve>(poGeom.release()->toCurve());
        if (poTriangle->addRing(std::move(poCurve)) != OGRERR_NONE)
        {
            return nullptr;
        }

        return poTriangle;
    }

    /* -------------------------------------------------------------------- */
    /*      LinearRing                                                      */
    /* -------------------------------------------------------------------- */
    if (EQUAL(pszBaseGeometry, "LinearRing"))
    {
        auto poLinearRing = std::make_unique<OGRLinearRing>();

        if (!ParseGMLCoordinates(psNode, poLinearRing.get(), nSRSDimension))
        {
            return nullptr;
        }

        return poLinearRing;
    }

    const auto storeArcByCenterPointParameters =
        [](const CPLXMLNode *psChild, const char *l_pszSRSName,
           bool &bIsApproximateArc, double &dfLastCurveApproximateArcRadius,
           bool &bLastCurveWasApproximateArcInvertedAxisOrder)
    {
        const CPLXMLNode *psRadius = FindBareXMLChild(psChild, "radius");
        if (psRadius && psRadius->eType == CXT_Element)
        {
            double dfRadius = CPLAtof(CPLGetXMLValue(psRadius, nullptr, "0"));
            const char *pszUnits = CPLGetXMLValue(psRadius, "uom", nullptr);
            bool bSRSUnitIsDegree = false;
            bool bInvertedAxisOrder = false;
            if (l_pszSRSName != nullptr)
            {
                OGRSpatialReference oSRS;
                if (oSRS.SetFromUserInput(l_pszSRSName) == OGRERR_NONE)
                {
                    if (oSRS.IsGeographic())
                    {
                        bInvertedAxisOrder =
                            CPL_TO_BOOL(oSRS.EPSGTreatsAsLatLong());
                        bSRSUnitIsDegree =
                            fabs(oSRS.GetAngularUnits(nullptr) -
                                 CPLAtof(SRS_UA_DEGREE_CONV)) < 1e-8;
                    }
                }
            }
            if (bSRSUnitIsDegree && pszUnits != nullptr &&
                (dfRadius = GetDistanceInMetre(dfRadius, pszUnits)) > 0)
            {
                bIsApproximateArc = true;
                dfLastCurveApproximateArcRadius = dfRadius;
                bLastCurveWasApproximateArcInvertedAxisOrder =
                    bInvertedAxisOrder;
            }
        }
    };

    const auto connectArcByCenterPointToOtherSegments =
        [](OGRGeometry *poGeom, OGRCompoundCurve *poCC,
           const bool bIsApproximateArc, const bool bLastCurveWasApproximateArc,
           const double dfLastCurveApproximateArcRadius,
           const bool bLastCurveWasApproximateArcInvertedAxisOrder)
    {
        if (bIsApproximateArc)
        {
            if (poGeom->getGeometryType() == wkbLineString)
            {
                OGRCurve *poPreviousCurve =
                    poCC->getCurve(poCC->getNumCurves() - 1);
                OGRLineString *poLS = poGeom->toLineString();
                if (poPreviousCurve->getNumPoints() >= 2 &&
                    poLS->getNumPoints() >= 2)
                {
                    OGRPoint p;
                    OGRPoint p2;
                    poPreviousCurve->EndPoint(&p);
                    poLS->StartPoint(&p2);
                    double dfDistance = 0.0;
                    if (bLastCurveWasApproximateArcInvertedAxisOrder)
                        dfDistance = OGR_GreatCircle_Distance(
                            p.getX(), p.getY(), p2.getX(), p2.getY());
                    else
                        dfDistance = OGR_GreatCircle_Distance(
                            p.getY(), p.getX(), p2.getY(), p2.getX());
                    // CPLDebug("OGR", "%f %f",
                    //          dfDistance,
                    //          dfLastCurveApproximateArcRadius
                    //          / 10.0 );
                    if (dfDistance < dfLastCurveApproximateArcRadius / 5.0)
                    {
                        CPLDebug("OGR", "Moving approximate start of "
                                        "ArcByCenterPoint to end of "
                                        "previous curve");
                        poLS->setPoint(0, &p);
                    }
                }
            }
        }
        else if (bLastCurveWasApproximateArc)
        {
            OGRCurve *poPreviousCurve =
                poCC->getCurve(poCC->getNumCurves() - 1);
            if (poPreviousCurve->getGeometryType() == wkbLineString)
            {
                OGRLineString *poLS = poPreviousCurve->toLineString();
                OGRCurve *poAsCurve = poGeom->toCurve();

                if (poLS->getNumPoints() >= 2 && poAsCurve->getNumPoints() >= 2)
                {
                    OGRPoint p;
                    OGRPoint p2;
                    poAsCurve->StartPoint(&p);
                    poLS->EndPoint(&p2);
                    double dfDistance = 0.0;
                    if (bLastCurveWasApproximateArcInvertedAxisOrder)
                        dfDistance = OGR_GreatCircle_Distance(
                            p.getX(), p.getY(), p2.getX(), p2.getY());
                    else
                        dfDistance = OGR_GreatCircle_Distance(
                            p.getY(), p.getX(), p2.getY(), p2.getX());
                    // CPLDebug(
                    //    "OGR", "%f %f",
                    //    dfDistance,
                    //    dfLastCurveApproximateArcRadius / 10.0 );

                    // "A-311 WHEELER AFB OAHU, HI.xml" needs more
                    // than 10%.
                    if (dfDistance < dfLastCurveApproximateArcRadius / 5.0)
                    {
                        CPLDebug("OGR", "Moving approximate end of last "
                                        "ArcByCenterPoint to start of the "
                                        "current curve");
                        poLS->setPoint(poLS->getNumPoints() - 1, &p);
                    }
                }
            }
        }
    };

    /* -------------------------------------------------------------------- */
    /*      Ring GML3                                                       */
    /* -------------------------------------------------------------------- */
    if (EQUAL(pszBaseGeometry, "Ring"))
    {
        std::unique_ptr<OGRCurve> poRing;
        std::unique_ptr<OGRCompoundCurve> poCC;
        bool bChildrenAreAllLineString = true;

        bool bLastCurveWasApproximateArc = false;
        bool bLastCurveWasApproximateArcInvertedAxisOrder = false;
        double dfLastCurveApproximateArcRadius = 0.0;

        bool bIsFirstChild = true;
        bool bFirstChildIsApproximateArc = false;
        double dfFirstChildApproximateArcRadius = 0.0;
        bool bFirstChildWasApproximateArcInvertedAxisOrder = false;

        for (const CPLXMLNode *psChild = psNode->psChild; psChild != nullptr;
             psChild = psChild->psNext)
        {
            if (psChild->eType == CXT_Element &&
                EQUAL(BareGMLElement(psChild->pszValue), "curveMember"))
            {
                const CPLXMLNode *psCurveChild = GetChildElement(psChild);
                std::unique_ptr<OGRGeometry> poGeom;
                if (psCurveChild != nullptr)
                {
                    poGeom = GML2OGRGeometry_XMLNode_Internal(
                        psCurveChild, nPseudoBoolGetSecondaryGeometryOption,
                        nRecLevel + 1, nSRSDimension, pszSRSName);
                }
                else
                {
                    if (psChild->psChild &&
                        psChild->psChild->eType == CXT_Attribute &&
                        psChild->psChild->psNext == nullptr &&
                        strcmp(psChild->psChild->pszValue, "xlink:href") == 0)
                    {
                        CPLError(CE_Warning, CPLE_AppDefined,
                                 "Cannot resolve xlink:href='%s'. "
                                 "Try setting GML_SKIP_RESOLVE_ELEMS=NONE",
                                 psChild->psChild->psChild->pszValue);
                    }
                    return nullptr;
                }

                // Try to join multiline string to one linestring.
                if (poGeom &&
                    wkbFlatten(poGeom->getGeometryType()) == wkbMultiLineString)
                {
                    poGeom.reset(OGRGeometryFactory::forceToLineString(
                        poGeom.release(), false));
                }

                if (poGeom == nullptr ||
                    !OGR_GT_IsCurve(poGeom->getGeometryType()))
                {
                    return nullptr;
                }

                if (wkbFlatten(poGeom->getGeometryType()) != wkbLineString)
                    bChildrenAreAllLineString = false;

                // Ad-hoc logic to handle nicely connecting ArcByCenterPoint
                // with consecutive curves, as found in some AIXM files.
                bool bIsApproximateArc = false;
                const CPLXMLNode *psChild2, *psChild3;
                if (strcmp(BareGMLElement(psCurveChild->pszValue), "Curve") ==
                        0 &&
                    (psChild2 = GetChildElement(psCurveChild)) != nullptr &&
                    strcmp(BareGMLElement(psChild2->pszValue), "segments") ==
                        0 &&
                    (psChild3 = GetChildElement(psChild2)) != nullptr &&
                    strcmp(BareGMLElement(psChild3->pszValue),
                           "ArcByCenterPoint") == 0)
                {
                    storeArcByCenterPointParameters(
                        psChild3, pszSRSName, bIsApproximateArc,
                        dfLastCurveApproximateArcRadius,
                        bLastCurveWasApproximateArcInvertedAxisOrder);
                    if (bIsFirstChild && bIsApproximateArc)
                    {
                        bFirstChildIsApproximateArc = true;
                        dfFirstChildApproximateArcRadius =
                            dfLastCurveApproximateArcRadius;
                        bFirstChildWasApproximateArcInvertedAxisOrder =
                            bLastCurveWasApproximateArcInvertedAxisOrder;
                    }
                    else if (psChild3->psNext)
                    {
                        bIsApproximateArc = false;
                    }
                }
                bIsFirstChild = false;

                if (poCC == nullptr && poRing == nullptr)
                {
                    poRing.reset(poGeom.release()->toCurve());
                }
                else
                {
                    if (poCC == nullptr)
                    {
                        poCC = std::make_unique<OGRCompoundCurve>();
                        bool bIgnored = false;
                        if (!GML2OGRGeometry_AddToCompositeCurve(
                                poCC.get(), std::move(poRing), bIgnored))
                        {
                            return nullptr;
                        }
                        poRing.reset();
                    }

                    connectArcByCenterPointToOtherSegments(
                        poGeom.get(), poCC.get(), bIsApproximateArc,
                        bLastCurveWasApproximateArc,
                        dfLastCurveApproximateArcRadius,
                        bLastCurveWasApproximateArcInvertedAxisOrder);

                    auto poCurve =
                        std::unique_ptr<OGRCurve>(poGeom.release()->toCurve());

                    bool bIgnored = false;
                    if (!GML2OGRGeometry_AddToCompositeCurve(
                            poCC.get(), std::move(poCurve), bIgnored))
                    {
                        return nullptr;
                    }
                }

                bLastCurveWasApproximateArc = bIsApproximateArc;
            }
        }

        /* Detect if the last object in the following hierarchy is a
           ArcByCenterPoint <gml:Ring> <gml:curveMember> (may be repeated)
                    <gml:Curve>
                        <gml:segments>
                            ....
                            <gml:ArcByCenterPoint ... />
                        </gml:segments>
                    </gml:Curve>
                </gml:curveMember>
            </gml:Ring>
        */
        bool bLastChildIsApproximateArc = false;
        for (const CPLXMLNode *psChild = psNode->psChild; psChild != nullptr;
             psChild = psChild->psNext)
        {
            if (psChild->eType == CXT_Element &&
                EQUAL(BareGMLElement(psChild->pszValue), "curveMember"))
            {
                const CPLXMLNode *psCurveMemberChild = GetChildElement(psChild);
                if (psCurveMemberChild &&
                    psCurveMemberChild->eType == CXT_Element &&
                    EQUAL(BareGMLElement(psCurveMemberChild->pszValue),
                          "Curve"))
                {
                    const CPLXMLNode *psCurveChild =
                        GetChildElement(psCurveMemberChild);
                    if (psCurveChild && psCurveChild->eType == CXT_Element &&
                        EQUAL(BareGMLElement(psCurveChild->pszValue),
                              "segments"))
                    {
                        for (const CPLXMLNode *psChild2 = psCurveChild->psChild;
                             psChild2 != nullptr; psChild2 = psChild2->psNext)
                        {
                            if (psChild2->eType == CXT_Element &&
                                EQUAL(BareGMLElement(psChild2->pszValue),
                                      "ArcByCenterPoint"))
                            {
                                storeArcByCenterPointParameters(
                                    psChild2, pszSRSName,
                                    bLastChildIsApproximateArc,
                                    dfLastCurveApproximateArcRadius,
                                    bLastCurveWasApproximateArcInvertedAxisOrder);
                            }
                            else
                            {
                                bLastChildIsApproximateArc = false;
                            }
                        }
                    }
                    else
                    {
                        bLastChildIsApproximateArc = false;
                    }
                }
                else
                {
                    bLastChildIsApproximateArc = false;
                }
            }
            else
            {
                bLastChildIsApproximateArc = false;
            }
        }

        if (poRing)
        {
            if (poRing->getNumPoints() >= 2 && bFirstChildIsApproximateArc &&
                !poRing->get_IsClosed() &&
                wkbFlatten(poRing->getGeometryType()) == wkbLineString)
            {
                OGRLineString *poLS = poRing->toLineString();

                OGRPoint p;
                OGRPoint p2;
                poLS->StartPoint(&p);
                poLS->EndPoint(&p2);
                double dfDistance = 0.0;
                if (bFirstChildWasApproximateArcInvertedAxisOrder)
                    dfDistance = OGR_GreatCircle_Distance(p.getX(), p.getY(),
                                                          p2.getX(), p2.getY());
                else
                    dfDistance = OGR_GreatCircle_Distance(p.getY(), p.getX(),
                                                          p2.getY(), p2.getX());
                if (dfDistance < dfFirstChildApproximateArcRadius / 5.0)
                {
                    CPLDebug("OGR", "Moving approximate start of "
                                    "ArcByCenterPoint to end of "
                                    "curve");
                    poLS->setPoint(0, &p2);
                }
            }
            else if (poRing->getNumPoints() >= 2 &&
                     bLastChildIsApproximateArc && !poRing->get_IsClosed() &&
                     wkbFlatten(poRing->getGeometryType()) == wkbLineString)
            {
                OGRLineString *poLS = poRing->toLineString();

                OGRPoint p;
                OGRPoint p2;
                poLS->StartPoint(&p);
                poLS->EndPoint(&p2);
                double dfDistance = 0.0;
                if (bLastCurveWasApproximateArcInvertedAxisOrder)
                    dfDistance = OGR_GreatCircle_Distance(p.getX(), p.getY(),
                                                          p2.getX(), p2.getY());
                else
                    dfDistance = OGR_GreatCircle_Distance(p.getY(), p.getX(),
                                                          p2.getY(), p2.getX());
                if (dfDistance < dfLastCurveApproximateArcRadius / 5.0)
                {
                    CPLDebug("OGR", "Moving approximate end of "
                                    "ArcByCenterPoint to start of "
                                    "curve");
                    poLS->setPoint(poLS->getNumPoints() - 1, &p);
                }
            }

            if (poRing->getNumPoints() < 2 || !poRing->get_IsClosed())
            {
                CPLError(CE_Failure, CPLE_AppDefined, "Non-closed ring");
                return nullptr;
            }
            return poRing;
        }

        if (poCC == nullptr)
            return nullptr;

        else if (/* bCastToLinearTypeIfPossible &&*/ bChildrenAreAllLineString)
        {
            return std::unique_ptr<OGRLinearRing>(
                OGRCurve::CastToLinearRing(poCC.release()));
        }
        else
        {
            if (poCC->getNumPoints() < 2 || !poCC->get_IsClosed())
            {
                CPLError(CE_Failure, CPLE_AppDefined, "Non-closed ring");
                return nullptr;
            }
            return poCC;
        }
    }

    /* -------------------------------------------------------------------- */
    /*      LineString                                                      */
    /* -------------------------------------------------------------------- */
    if (EQUAL(pszBaseGeometry, "LineString") ||
        EQUAL(pszBaseGeometry, "LineStringSegment") ||
        EQUAL(pszBaseGeometry, "GeodesicString"))
    {
        auto poLine = std::make_unique<OGRLineString>();

        if (!ParseGMLCoordinates(psNode, poLine.get(), nSRSDimension))
        {
            return nullptr;
        }

        return poLine;
    }

    /* -------------------------------------------------------------------- */
    /*      Arc                                                             */
    /* -------------------------------------------------------------------- */
    if (EQUAL(pszBaseGeometry, "Arc"))
    {
        auto poCC = std::make_unique<OGRCircularString>();

        if (!ParseGMLCoordinates(psNode, poCC.get(), nSRSDimension))
        {
            return nullptr;
        }

        // Normally a gml:Arc has only 3 points of controls, but in the
        // wild we sometimes find GML with 5 points, so accept any odd
        // number >= 3 (ArcString should be used for > 3 points)
        if (poCC->getNumPoints() < 3 || (poCC->getNumPoints() % 2) != 1)
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Bad number of points in Arc");
            return nullptr;
        }

        return poCC;
    }

    /* -------------------------------------------------------------------- */
    /*     ArcString                                                        */
    /* -------------------------------------------------------------------- */
    if (EQUAL(pszBaseGeometry, "ArcString"))
    {
        auto poCC = std::make_unique<OGRCircularString>();

        if (!ParseGMLCoordinates(psNode, poCC.get(), nSRSDimension))
        {
            return nullptr;
        }

        if (poCC->getNumPoints() < 3 || (poCC->getNumPoints() % 2) != 1)
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Bad number of points in ArcString");
            return nullptr;
        }

        return poCC;
    }

    /* -------------------------------------------------------------------- */
    /*      Circle                                                          */
    /* -------------------------------------------------------------------- */
    if (EQUAL(pszBaseGeometry, "Circle"))
    {
        auto poLine = std::make_unique<OGRLineString>();

        if (!ParseGMLCoordinates(psNode, poLine.get(), nSRSDimension))
        {
            return nullptr;
        }

        if (poLine->getNumPoints() != 3)
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Bad number of points in Circle");
            return nullptr;
        }

        double R = 0.0;
        double cx = 0.0;
        double cy = 0.0;
        double alpha0 = 0.0;
        double alpha1 = 0.0;
        double alpha2 = 0.0;
        if (!OGRGeometryFactory::GetCurveParameters(
                poLine->getX(0), poLine->getY(0), poLine->getX(1),
                poLine->getY(1), poLine->getX(2), poLine->getY(2), R, cx, cy,
                alpha0, alpha1, alpha2))
        {
            return nullptr;
        }

        auto poCC = std::make_unique<OGRCircularString>();
        OGRPoint p;
        poLine->getPoint(0, &p);
        poCC->addPoint(&p);
        poLine->getPoint(1, &p);
        poCC->addPoint(&p);
        poLine->getPoint(2, &p);
        poCC->addPoint(&p);
        const double alpha4 =
            alpha2 > alpha0 ? alpha0 + kdf2PI : alpha0 - kdf2PI;
        const double alpha3 = (alpha2 + alpha4) / 2.0;
        const double x = cx + R * cos(alpha3);
        const double y = cy + R * sin(alpha3);
        if (poCC->getCoordinateDimension() == 3)
            poCC->addPoint(x, y, p.getZ());
        else
            poCC->addPoint(x, y);
        poLine->getPoint(0, &p);
        poCC->addPoint(&p);
        return poCC;
    }

    /* -------------------------------------------------------------------- */
    /*      ArcByBulge                                                      */
    /* -------------------------------------------------------------------- */
    if (EQUAL(pszBaseGeometry, "ArcByBulge"))
    {
        const CPLXMLNode *psChild = FindBareXMLChild(psNode, "bulge");
        if (psChild == nullptr || psChild->eType != CXT_Element ||
            psChild->psChild == nullptr)
        {
            CPLError(CE_Failure, CPLE_AppDefined, "Missing bulge element.");
            return nullptr;
        }
        const double dfBulge = CPLAtof(psChild->psChild->pszValue);

        psChild = FindBareXMLChild(psNode, "normal");
        if (psChild == nullptr || psChild->eType != CXT_Element)
        {
            CPLError(CE_Failure, CPLE_AppDefined, "Missing normal element.");
            return nullptr;
        }
        double dfNormal = CPLAtof(psChild->psChild->pszValue);

        auto poLS = std::make_unique<OGRLineString>();
        if (!ParseGMLCoordinates(psNode, poLS.get(), nSRSDimension))
        {
            return nullptr;
        }

        if (poLS->getNumPoints() != 2)
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Bad number of points in ArcByBulge");
            return nullptr;
        }

        auto poCC = std::make_unique<OGRCircularString>();
        OGRPoint p;
        poLS->getPoint(0, &p);
        poCC->addPoint(&p);

        const double dfMidX = (poLS->getX(0) + poLS->getX(1)) / 2.0;
        const double dfMidY = (poLS->getY(0) + poLS->getY(1)) / 2.0;
        const double dfDirX = (poLS->getX(1) - poLS->getX(0)) / 2.0;
        const double dfDirY = (poLS->getY(1) - poLS->getY(0)) / 2.0;
        double dfNormX = -dfDirY;
        double dfNormY = dfDirX;
        const double dfNorm = sqrt(dfNormX * dfNormX + dfNormY * dfNormY);
        if (dfNorm != 0.0)
        {
            dfNormX /= dfNorm;
            dfNormY /= dfNorm;
        }
        const double dfNewX = dfMidX + dfNormX * dfBulge * dfNormal;
        const double dfNewY = dfMidY + dfNormY * dfBulge * dfNormal;

        if (poCC->getCoordinateDimension() == 3)
            poCC->addPoint(dfNewX, dfNewY, p.getZ());
        else
            poCC->addPoint(dfNewX, dfNewY);

        poLS->getPoint(1, &p);
        poCC->addPoint(&p);

        return poCC;
    }

    /* -------------------------------------------------------------------- */
    /*      ArcByCenterPoint                                                */
    /* -------------------------------------------------------------------- */
    if (EQUAL(pszBaseGeometry, "ArcByCenterPoint"))
    {
        const CPLXMLNode *psChild = FindBareXMLChild(psNode, "radius");
        if (psChild == nullptr || psChild->eType != CXT_Element)
        {
            CPLError(CE_Failure, CPLE_AppDefined, "Missing radius element.");
            return nullptr;
        }
        const double dfRadius = CPLAtof(CPLGetXMLValue(psChild, nullptr, "0"));
        const char *pszUnits = CPLGetXMLValue(psChild, "uom", nullptr);

        psChild = FindBareXMLChild(psNode, "startAngle");
        if (psChild == nullptr || psChild->eType != CXT_Element)
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Missing startAngle element.");
            return nullptr;
        }
        const double dfStartAngle =
            CPLAtof(CPLGetXMLValue(psChild, nullptr, "0"));

        psChild = FindBareXMLChild(psNode, "endAngle");
        if (psChild == nullptr || psChild->eType != CXT_Element)
        {
            CPLError(CE_Failure, CPLE_AppDefined, "Missing endAngle element.");
            return nullptr;
        }
        const double dfEndAngle =
            CPLAtof(CPLGetXMLValue(psChild, nullptr, "0"));

        OGRPoint p;
        if (!ParseGMLCoordinates(psNode, &p, nSRSDimension))
        {
            return nullptr;
        }

        bool bSRSUnitIsDegree = false;
        bool bInvertedAxisOrder = false;
        if (pszSRSName != nullptr)
        {
            OGRSpatialReference oSRS;
            if (oSRS.SetFromUserInput(pszSRSName) == OGRERR_NONE)
            {
                if (oSRS.IsGeographic())
                {
                    bInvertedAxisOrder =
                        CPL_TO_BOOL(oSRS.EPSGTreatsAsLatLong());
                    bSRSUnitIsDegree = fabs(oSRS.GetAngularUnits(nullptr) -
                                            CPLAtof(SRS_UA_DEGREE_CONV)) < 1e-8;
                }
                else if (oSRS.IsProjected())
                {
                    bInvertedAxisOrder =
                        CPL_TO_BOOL(oSRS.EPSGTreatsAsNorthingEasting());
                }
            }
        }

        double dfCenterX = p.getX();
        double dfCenterY = p.getY();

        double dfDistance;
        if (bSRSUnitIsDegree && pszUnits != nullptr &&
            (dfDistance = GetDistanceInMetre(dfRadius, pszUnits)) > 0)
        {
            auto poLS = std::make_unique<OGRLineString>();
            // coverity[tainted_data]
            const double dfStep =
                CPLAtof(CPLGetConfigOption("OGR_ARC_STEPSIZE", "4"));
            const double dfSign = dfStartAngle < dfEndAngle ? 1 : -1;
            for (double dfAngle = dfStartAngle;
                 (dfAngle - dfEndAngle) * dfSign < 0;
                 dfAngle += dfSign * dfStep)
            {
                double dfLong = 0.0;
                double dfLat = 0.0;
                if (bInvertedAxisOrder)
                {
                    OGR_GreatCircle_ExtendPosition(
                        dfCenterX, dfCenterY, dfDistance,
                        // See
                        // https://ext.eurocontrol.int/aixm_confluence/display/ACG/ArcByCenterPoint+Interpretation+Summary
                        dfAngle, &dfLat, &dfLong);
                    p.setX(dfLat);  // yes, external code will do the swap later
                    p.setY(dfLong);
                }
                else
                {
                    OGR_GreatCircle_ExtendPosition(dfCenterY, dfCenterX,
                                                   dfDistance, 90 - dfAngle,
                                                   &dfLat, &dfLong);
                    p.setX(dfLong);
                    p.setY(dfLat);
                }
                poLS->addPoint(&p);
            }

            double dfLong = 0.0;
            double dfLat = 0.0;
            if (bInvertedAxisOrder)
            {
                OGR_GreatCircle_ExtendPosition(dfCenterX, dfCenterY, dfDistance,
                                               dfEndAngle, &dfLat, &dfLong);
                p.setX(dfLat);  // yes, external code will do the swap later
                p.setY(dfLong);
            }
            else
            {
                OGR_GreatCircle_ExtendPosition(dfCenterY, dfCenterX, dfDistance,
                                               90 - dfEndAngle, &dfLat,
                                               &dfLong);
                p.setX(dfLong);
                p.setY(dfLat);
            }
            poLS->addPoint(&p);

            return poLS;
        }

        if (bInvertedAxisOrder)
            std::swap(dfCenterX, dfCenterY);

        auto poCC = std::make_unique<OGRCircularString>();
        p.setX(dfCenterX + dfRadius * cos(dfStartAngle * kdfD2R));
        p.setY(dfCenterY + dfRadius * sin(dfStartAngle * kdfD2R));
        poCC->addPoint(&p);
        const double dfAverageAngle = (dfStartAngle + dfEndAngle) / 2.0;
        p.setX(dfCenterX + dfRadius * cos(dfAverageAngle * kdfD2R));
        p.setY(dfCenterY + dfRadius * sin(dfAverageAngle * kdfD2R));
        poCC->addPoint(&p);
        p.setX(dfCenterX + dfRadius * cos(dfEndAngle * kdfD2R));
        p.setY(dfCenterY + dfRadius * sin(dfEndAngle * kdfD2R));
        poCC->addPoint(&p);

        if (bInvertedAxisOrder)
            poCC->swapXY();

        return poCC;
    }

    /* -------------------------------------------------------------------- */
    /*      CircleByCenterPoint                                             */
    /* -------------------------------------------------------------------- */
    if (EQUAL(pszBaseGeometry, "CircleByCenterPoint"))
    {
        const CPLXMLNode *psChild = FindBareXMLChild(psNode, "radius");
        if (psChild == nullptr || psChild->eType != CXT_Element)
        {
            CPLError(CE_Failure, CPLE_AppDefined, "Missing radius element.");
            return nullptr;
        }
        const double dfRadius = CPLAtof(CPLGetXMLValue(psChild, nullptr, "0"));
        const char *pszUnits = CPLGetXMLValue(psChild, "uom", nullptr);

        OGRPoint p;
        if (!ParseGMLCoordinates(psNode, &p, nSRSDimension))
        {
            return nullptr;
        }

        bool bSRSUnitIsDegree = false;
        bool bInvertedAxisOrder = false;
        if (pszSRSName != nullptr)
        {
            OGRSpatialReference oSRS;
            if (oSRS.SetFromUserInput(pszSRSName) == OGRERR_NONE)
            {
                if (oSRS.IsGeographic())
                {
                    bInvertedAxisOrder =
                        CPL_TO_BOOL(oSRS.EPSGTreatsAsLatLong());
                    bSRSUnitIsDegree = fabs(oSRS.GetAngularUnits(nullptr) -
                                            CPLAtof(SRS_UA_DEGREE_CONV)) < 1e-8;
                }
                else if (oSRS.IsProjected())
                {
                    bInvertedAxisOrder =
                        CPL_TO_BOOL(oSRS.EPSGTreatsAsNorthingEasting());
                }
            }
        }

        double dfCenterX = p.getX();
        double dfCenterY = p.getY();

        double dfDistance;
        if (bSRSUnitIsDegree && pszUnits != nullptr &&
            (dfDistance = GetDistanceInMetre(dfRadius, pszUnits)) > 0)
        {
            auto poLS = std::make_unique<OGRLineString>();
            const double dfStep =
                CPLAtof(CPLGetConfigOption("OGR_ARC_STEPSIZE", "4"));
            for (double dfAngle = 0; dfAngle < 360; dfAngle += dfStep)
            {
                double dfLong = 0.0;
                double dfLat = 0.0;
                if (bInvertedAxisOrder)
                {
                    OGR_GreatCircle_ExtendPosition(dfCenterX, dfCenterY,
                                                   dfDistance, dfAngle, &dfLat,
                                                   &dfLong);
                    p.setX(dfLat);  // yes, external code will do the swap later
                    p.setY(dfLong);
                }
                else
                {
                    OGR_GreatCircle_ExtendPosition(dfCenterY, dfCenterX,
                                                   dfDistance, dfAngle, &dfLat,
                                                   &dfLong);
                    p.setX(dfLong);
                    p.setY(dfLat);
                }
                poLS->addPoint(&p);
            }
            poLS->getPoint(0, &p);
            poLS->addPoint(&p);
            return poLS;
        }

        if (bInvertedAxisOrder)
            std::swap(dfCenterX, dfCenterY);

        auto poCC = std::make_unique<OGRCircularString>();
        p.setX(dfCenterX - dfRadius);
        p.setY(dfCenterY);
        poCC->addPoint(&p);
        p.setX(dfCenterX + dfRadius);
        p.setY(dfCenterY);
        poCC->addPoint(&p);
        p.setX(dfCenterX - dfRadius);
        p.setY(dfCenterY);
        poCC->addPoint(&p);

        if (bInvertedAxisOrder)
            poCC->swapXY();

        return poCC;
    }

    /* -------------------------------------------------------------------- */
    /*      PointType                                                       */
    /* -------------------------------------------------------------------- */
    if (EQUAL(pszBaseGeometry, "PointType") ||
        EQUAL(pszBaseGeometry, "Point") ||
        EQUAL(pszBaseGeometry, "ConnectionPoint"))
    {
        auto poPoint = std::make_unique<OGRPoint>();

        if (!ParseGMLCoordinates(psNode, poPoint.get(), nSRSDimension))
        {
            return nullptr;
        }

        return poPoint;
    }

    /* -------------------------------------------------------------------- */
    /*      Box                                                             */
    /* -------------------------------------------------------------------- */
    if (EQUAL(pszBaseGeometry, "BoxType") || EQUAL(pszBaseGeometry, "Box"))
    {
        OGRLineString oPoints;

        if (!ParseGMLCoordinates(psNode, &oPoints, nSRSDimension))
            return nullptr;

        if (oPoints.getNumPoints() < 2)
            return nullptr;

        auto poBoxRing = std::make_unique<OGRLinearRing>();
        auto poBoxPoly = std::make_unique<OGRPolygon>();

        poBoxRing->setNumPoints(5);
        poBoxRing->setPoint(0, oPoints.getX(0), oPoints.getY(0),
                            oPoints.getZ(0));
        poBoxRing->setPoint(1, oPoints.getX(1), oPoints.getY(0),
                            oPoints.getZ(0));
        poBoxRing->setPoint(2, oPoints.getX(1), oPoints.getY(1),
                            oPoints.getZ(1));
        poBoxRing->setPoint(3, oPoints.getX(0), oPoints.getY(1),
                            oPoints.getZ(0));
        poBoxRing->setPoint(4, oPoints.getX(0), oPoints.getY(0),
                            oPoints.getZ(0));
        poBoxRing->set3D(oPoints.Is3D());

        poBoxPoly->addRing(std::move(poBoxRing));

        return poBoxPoly;
    }

    /* -------------------------------------------------------------------- */
    /*      Envelope                                                        */
    /* -------------------------------------------------------------------- */
    if (EQUAL(pszBaseGeometry, "Envelope"))
    {
        const CPLXMLNode *psLowerCorner =
            FindBareXMLChild(psNode, "lowerCorner");
        const CPLXMLNode *psUpperCorner =
            FindBareXMLChild(psNode, "upperCorner");
        if (psLowerCorner == nullptr || psUpperCorner == nullptr)
            return nullptr;
        const char *pszLowerCorner = GetElementText(psLowerCorner);
        const char *pszUpperCorner = GetElementText(psUpperCorner);
        if (pszLowerCorner == nullptr || pszUpperCorner == nullptr)
            return nullptr;
        char **papszLowerCorner = CSLTokenizeString(pszLowerCorner);
        char **papszUpperCorner = CSLTokenizeString(pszUpperCorner);
        const int nTokenCountLC = CSLCount(papszLowerCorner);
        const int nTokenCountUC = CSLCount(papszUpperCorner);
        if (nTokenCountLC < 2 || nTokenCountUC < 2)
        {
            CSLDestroy(papszLowerCorner);
            CSLDestroy(papszUpperCorner);
            return nullptr;
        }

        const double dfLLX = CPLAtof(papszLowerCorner[0]);
        const double dfLLY = CPLAtof(papszLowerCorner[1]);
        const double dfURX = CPLAtof(papszUpperCorner[0]);
        const double dfURY = CPLAtof(papszUpperCorner[1]);
        CSLDestroy(papszLowerCorner);
        CSLDestroy(papszUpperCorner);

        auto poEnvelopeRing = std::make_unique<OGRLinearRing>();
        auto poPoly = std::make_unique<OGRPolygon>();

        poEnvelopeRing->setNumPoints(5);
        poEnvelopeRing->setPoint(0, dfLLX, dfLLY);
        poEnvelopeRing->setPoint(1, dfURX, dfLLY);
        poEnvelopeRing->setPoint(2, dfURX, dfURY);
        poEnvelopeRing->setPoint(3, dfLLX, dfURY);
        poEnvelopeRing->setPoint(4, dfLLX, dfLLY);
        poPoly->addRing(std::move(poEnvelopeRing));

        return poPoly;
    }

    /* --------------------------------------------------------------------- */
    /*      MultiPolygon / MultiSurface / CompositeSurface                   */
    /*                                                                       */
    /* For CompositeSurface, this is a very rough approximation to deal with */
    /* it as a MultiPolygon, because it can several faces of a 3D volume.    */
    /* --------------------------------------------------------------------- */
    if (EQUAL(pszBaseGeometry, "MultiPolygon") ||
        EQUAL(pszBaseGeometry, "MultiSurface") ||
        EQUAL(pszBaseGeometry, "CompositeSurface"))
    {
        std::unique_ptr<OGRMultiSurface> poMS =
            EQUAL(pszBaseGeometry, "MultiPolygon")
                ? std::make_unique<OGRMultiPolygon>()
                : std::make_unique<OGRMultiSurface>();
        bool bReconstructTopology = false;
        bool bChildrenAreAllPolygons = true;

        // Iterate over children.
        for (const CPLXMLNode *psChild = psNode->psChild; psChild != nullptr;
             psChild = psChild->psNext)
        {
            const char *pszMemberElement = BareGMLElement(psChild->pszValue);
            if (psChild->eType == CXT_Element &&
                (EQUAL(pszMemberElement, "polygonMember") ||
                 EQUAL(pszMemberElement, "surfaceMember")))
            {
                const CPLXMLNode *psSurfaceChild = GetChildElement(psChild);

                if (psSurfaceChild != nullptr)
                {
                    // Cf #5421 where there are PolygonPatch with only inner
                    // rings.
                    const CPLXMLNode *psPolygonPatch =
                        GetChildElement(GetChildElement(psSurfaceChild));
                    const CPLXMLNode *psPolygonPatchChild = nullptr;
                    if (psPolygonPatch != nullptr &&
                        psPolygonPatch->eType == CXT_Element &&
                        EQUAL(BareGMLElement(psPolygonPatch->pszValue),
                              "PolygonPatch") &&
                        (psPolygonPatchChild =
                             GetChildElement(psPolygonPatch)) != nullptr &&
                        EQUAL(BareGMLElement(psPolygonPatchChild->pszValue),
                              "interior"))
                    {
                        // Find all inner rings
                        for (const CPLXMLNode *psChild2 =
                                 psPolygonPatch->psChild;
                             psChild2 != nullptr; psChild2 = psChild2->psNext)
                        {
                            if (psChild2->eType == CXT_Element &&
                                (EQUAL(BareGMLElement(psChild2->pszValue),
                                       "interior")))
                            {
                                const CPLXMLNode *psInteriorChild =
                                    GetChildElement(psChild2);
                                auto poRing =
                                    psInteriorChild == nullptr
                                        ? nullptr
                                        : GML2OGRGeometry_XMLNode_Internal(
                                              psInteriorChild,
                                              nPseudoBoolGetSecondaryGeometryOption,
                                              nRecLevel + 1, nSRSDimension,
                                              pszSRSName);
                                if (poRing == nullptr)
                                {
                                    CPLError(CE_Failure, CPLE_AppDefined,
                                             "Invalid interior ring");
                                    return nullptr;
                                }
                                if (!EQUAL(poRing->getGeometryName(),
                                           "LINEARRING"))
                                {
                                    CPLError(CE_Failure, CPLE_AppDefined,
                                             "%s: Got %.500s geometry as "
                                             "innerBoundaryIs instead of "
                                             "LINEARRING.",
                                             pszBaseGeometry,
                                             poRing->getGeometryName());
                                    return nullptr;
                                }

                                bReconstructTopology = true;
                                auto poPolygon = std::make_unique<OGRPolygon>();
                                auto poLinearRing =
                                    std::unique_ptr<OGRLinearRing>(
                                        poRing.release()->toLinearRing());
                                poPolygon->addRing(std::move(poLinearRing));
                                poMS->addGeometry(std::move(poPolygon));
                            }
                        }
                    }
                    else
                    {
                        auto poGeom = GML2OGRGeometry_XMLNode_Internal(
                            psSurfaceChild,
                            nPseudoBoolGetSecondaryGeometryOption,
                            nRecLevel + 1, nSRSDimension, pszSRSName);
                        if (!GML2OGRGeometry_AddToMultiSurface(
                                poMS.get(), std::move(poGeom), pszMemberElement,
                                bChildrenAreAllPolygons))
                        {
                            return nullptr;
                        }
                    }
                }
            }
            else if (psChild->eType == CXT_Element &&
                     EQUAL(pszMemberElement, "surfaceMembers"))
            {
                for (const CPLXMLNode *psChild2 = psChild->psChild;
                     psChild2 != nullptr; psChild2 = psChild2->psNext)
                {
                    pszMemberElement = BareGMLElement(psChild2->pszValue);
                    if (psChild2->eType == CXT_Element &&
                        (EQUAL(pszMemberElement, "Surface") ||
                         EQUAL(pszMemberElement, "Polygon") ||
                         EQUAL(pszMemberElement, "PolygonPatch") ||
                         EQUAL(pszMemberElement, "CompositeSurface")))
                    {
                        auto poGeom = GML2OGRGeometry_XMLNode_Internal(
                            psChild2, nPseudoBoolGetSecondaryGeometryOption,
                            nRecLevel + 1, nSRSDimension, pszSRSName);
                        if (!GML2OGRGeometry_AddToMultiSurface(
                                poMS.get(), std::move(poGeom), pszMemberElement,
                                bChildrenAreAllPolygons))
                        {
                            return nullptr;
                        }
                    }
                }
            }
        }

        if (bReconstructTopology && bChildrenAreAllPolygons)
        {
            auto poMPoly =
                wkbFlatten(poMS->getGeometryType()) == wkbMultiSurface
                    ? std::unique_ptr<OGRMultiPolygon>(
                          OGRMultiSurface::CastToMultiPolygon(poMS.release()))
                    : std::unique_ptr<OGRMultiPolygon>(
                          poMS.release()->toMultiPolygon());
            const int nPolygonCount = poMPoly->getNumGeometries();
            std::vector<OGRGeometry *> apoPolygons;
            apoPolygons.reserve(nPolygonCount);
            for (int i = 0; i < nPolygonCount; i++)
            {
                apoPolygons.emplace_back(poMPoly->getGeometryRef(0));
                poMPoly->removeGeometry(0, FALSE);
            }
            poMPoly.reset();
            int bResultValidGeometry = FALSE;
            return std::unique_ptr<OGRGeometry>(
                OGRGeometryFactory::organizePolygons(
                    apoPolygons.data(), nPolygonCount, &bResultValidGeometry));
        }
        else
        {
            if (/* bCastToLinearTypeIfPossible && */
                wkbFlatten(poMS->getGeometryType()) == wkbMultiSurface &&
                bChildrenAreAllPolygons)
            {
                return std::unique_ptr<OGRMultiPolygon>(
                    OGRMultiSurface::CastToMultiPolygon(poMS.release()));
            }

            return poMS;
        }
    }

    /* -------------------------------------------------------------------- */
    /*      MultiPoint                                                      */
    /* -------------------------------------------------------------------- */
    if (EQUAL(pszBaseGeometry, "MultiPoint"))
    {
        auto poMP = std::make_unique<OGRMultiPoint>();

        // Collect points.
        for (const CPLXMLNode *psChild = psNode->psChild; psChild != nullptr;
             psChild = psChild->psNext)
        {
            if (psChild->eType == CXT_Element &&
                EQUAL(BareGMLElement(psChild->pszValue), "pointMember"))
            {
                const CPLXMLNode *psPointChild = GetChildElement(psChild);

                if (psPointChild != nullptr)
                {
                    auto poPointMember = GML2OGRGeometry_XMLNode_Internal(
                        psPointChild, nPseudoBoolGetSecondaryGeometryOption,
                        nRecLevel + 1, nSRSDimension, pszSRSName);
                    if (poPointMember == nullptr ||
                        wkbFlatten(poPointMember->getGeometryType()) !=
                            wkbPoint)
                    {
                        CPLError(CE_Failure, CPLE_AppDefined,
                                 "MultiPoint: Got %.500s geometry as "
                                 "pointMember instead of POINT",
                                 poPointMember
                                     ? poPointMember->getGeometryName()
                                     : "NULL");
                        return nullptr;
                    }

                    poMP->addGeometry(std::move(poPointMember));
                }
            }
            else if (psChild->eType == CXT_Element &&
                     EQUAL(BareGMLElement(psChild->pszValue), "pointMembers"))
            {
                for (const CPLXMLNode *psChild2 = psChild->psChild;
                     psChild2 != nullptr; psChild2 = psChild2->psNext)
                {
                    if (psChild2->eType == CXT_Element &&
                        (EQUAL(BareGMLElement(psChild2->pszValue), "Point")))
                    {
                        auto poGeom = GML2OGRGeometry_XMLNode_Internal(
                            psChild2, nPseudoBoolGetSecondaryGeometryOption,
                            nRecLevel + 1, nSRSDimension, pszSRSName);
                        if (poGeom == nullptr)
                        {
                            CPLError(CE_Failure, CPLE_AppDefined, "Invalid %s",
                                     BareGMLElement(psChild2->pszValue));
                            return nullptr;
                        }

                        if (wkbFlatten(poGeom->getGeometryType()) == wkbPoint)
                        {
                            auto poPoint = std::unique_ptr<OGRPoint>(
                                poGeom.release()->toPoint());
                            poMP->addGeometry(std::move(poPoint));
                        }
                        else
                        {
                            CPLError(CE_Failure, CPLE_AppDefined,
                                     "Got %.500s geometry as pointMember "
                                     "instead of POINT.",
                                     poGeom->getGeometryName());
                            return nullptr;
                        }
                    }
                }
            }
        }

        return poMP;
    }

    /* -------------------------------------------------------------------- */
    /*      MultiLineString                                                 */
    /* -------------------------------------------------------------------- */
    if (EQUAL(pszBaseGeometry, "MultiLineString"))
    {
        auto poMLS = std::make_unique<OGRMultiLineString>();

        // Collect lines.
        for (const CPLXMLNode *psChild = psNode->psChild; psChild != nullptr;
             psChild = psChild->psNext)
        {
            if (psChild->eType == CXT_Element &&
                EQUAL(BareGMLElement(psChild->pszValue), "lineStringMember"))
            {
                const CPLXMLNode *psLineStringChild = GetChildElement(psChild);
                auto poGeom =
                    psLineStringChild == nullptr
                        ? nullptr
                        : GML2OGRGeometry_XMLNode_Internal(
                              psLineStringChild,
                              nPseudoBoolGetSecondaryGeometryOption,
                              nRecLevel + 1, nSRSDimension, pszSRSName);
                if (poGeom == nullptr ||
                    wkbFlatten(poGeom->getGeometryType()) != wkbLineString)
                {
                    CPLError(CE_Failure, CPLE_AppDefined,
                             "MultiLineString: Got %.500s geometry as Member "
                             "instead of LINESTRING.",
                             poGeom ? poGeom->getGeometryName() : "NULL");
                    return nullptr;
                }

                poMLS->addGeometry(std::move(poGeom));
            }
        }

        return poMLS;
    }

    /* -------------------------------------------------------------------- */
    /*      MultiCurve                                                      */
    /* -------------------------------------------------------------------- */
    if (EQUAL(pszBaseGeometry, "MultiCurve"))
    {
        auto poMC = std::make_unique<OGRMultiCurve>();
        bool bChildrenAreAllLineString = true;

        // Collect curveMembers.
        for (const CPLXMLNode *psChild = psNode->psChild; psChild != nullptr;
             psChild = psChild->psNext)
        {
            if (psChild->eType == CXT_Element &&
                EQUAL(BareGMLElement(psChild->pszValue), "curveMember"))
            {
                const CPLXMLNode *psChild2 = GetChildElement(psChild);
                if (psChild2 != nullptr)  // Empty curveMember is valid.
                {
                    auto poGeom = GML2OGRGeometry_XMLNode_Internal(
                        psChild2, nPseudoBoolGetSecondaryGeometryOption,
                        nRecLevel + 1, nSRSDimension, pszSRSName);
                    if (poGeom == nullptr ||
                        !OGR_GT_IsCurve(poGeom->getGeometryType()))
                    {
                        CPLError(CE_Failure, CPLE_AppDefined,
                                 "MultiCurve: Got %.500s geometry as Member "
                                 "instead of a curve.",
                                 poGeom ? poGeom->getGeometryName() : "NULL");
                        return nullptr;
                    }

                    if (wkbFlatten(poGeom->getGeometryType()) != wkbLineString)
                        bChildrenAreAllLineString = false;

                    if (poMC->addGeometry(std::move(poGeom)) != OGRERR_NONE)
                    {
                        return nullptr;
                    }
                }
            }
            else if (psChild->eType == CXT_Element &&
                     EQUAL(BareGMLElement(psChild->pszValue), "curveMembers"))
            {
                for (const CPLXMLNode *psChild2 = psChild->psChild;
                     psChild2 != nullptr; psChild2 = psChild2->psNext)
                {
                    if (psChild2->eType == CXT_Element)
                    {
                        auto poGeom = GML2OGRGeometry_XMLNode_Internal(
                            psChild2, nPseudoBoolGetSecondaryGeometryOption,
                            nRecLevel + 1, nSRSDimension, pszSRSName);
                        if (poGeom == nullptr ||
                            !OGR_GT_IsCurve(poGeom->getGeometryType()))
                        {
                            CPLError(CE_Failure, CPLE_AppDefined,
                                     "MultiCurve: Got %.500s geometry as "
                                     "Member instead of a curve.",
                                     poGeom ? poGeom->getGeometryName()
                                            : "NULL");
                            return nullptr;
                        }

                        if (wkbFlatten(poGeom->getGeometryType()) !=
                            wkbLineString)
                            bChildrenAreAllLineString = false;

                        if (poMC->addGeometry(std::move(poGeom)) != OGRERR_NONE)
                        {
                            return nullptr;
                        }
                    }
                }
            }
        }

        if (/* bCastToLinearTypeIfPossible && */ bChildrenAreAllLineString)
        {
            return std::unique_ptr<OGRMultiLineString>(
                OGRMultiCurve::CastToMultiLineString(poMC.release()));
        }

        return poMC;
    }

    /* -------------------------------------------------------------------- */
    /*      CompositeCurve                                                  */
    /* -------------------------------------------------------------------- */
    if (EQUAL(pszBaseGeometry, "CompositeCurve"))
    {
        auto poCC = std::make_unique<OGRCompoundCurve>();
        bool bChildrenAreAllLineString = true;

        // Collect curveMembers.
        for (const CPLXMLNode *psChild = psNode->psChild; psChild != nullptr;
             psChild = psChild->psNext)
        {
            if (psChild->eType == CXT_Element &&
                EQUAL(BareGMLElement(psChild->pszValue), "curveMember"))
            {
                const CPLXMLNode *psChild2 = GetChildElement(psChild);
                if (psChild2 != nullptr)  // Empty curveMember is valid.
                {
                    auto poGeom = GML2OGRGeometry_XMLNode_Internal(
                        psChild2, nPseudoBoolGetSecondaryGeometryOption,
                        nRecLevel + 1, nSRSDimension, pszSRSName);
                    if (!GML2OGRGeometry_AddToCompositeCurve(
                            poCC.get(), std::move(poGeom),
                            bChildrenAreAllLineString))
                    {
                        return nullptr;
                    }
                }
            }
            else if (psChild->eType == CXT_Element &&
                     EQUAL(BareGMLElement(psChild->pszValue), "curveMembers"))
            {
                for (const CPLXMLNode *psChild2 = psChild->psChild;
                     psChild2 != nullptr; psChild2 = psChild2->psNext)
                {
                    if (psChild2->eType == CXT_Element)
                    {
                        auto poGeom = GML2OGRGeometry_XMLNode_Internal(
                            psChild2, nPseudoBoolGetSecondaryGeometryOption,
                            nRecLevel + 1, nSRSDimension, pszSRSName);
                        if (!GML2OGRGeometry_AddToCompositeCurve(
                                poCC.get(), std::move(poGeom),
                                bChildrenAreAllLineString))
                        {
                            return nullptr;
                        }
                    }
                }
            }
        }

        if (/* bCastToLinearTypeIfPossible && */ bChildrenAreAllLineString)
        {
            return std::unique_ptr<OGRLineString>(
                OGRCurve::CastToLineString(poCC.release()));
        }

        return poCC;
    }

    /* -------------------------------------------------------------------- */
    /*      Curve                                                           */
    /* -------------------------------------------------------------------- */
    if (EQUAL(pszBaseGeometry, "Curve"))
    {
        const CPLXMLNode *psChild = FindBareXMLChild(psNode, "segments");
        if (psChild == nullptr)
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "GML3 Curve geometry lacks segments element.");
            return nullptr;
        }

        auto poGeom = GML2OGRGeometry_XMLNode_Internal(
            psChild, nPseudoBoolGetSecondaryGeometryOption, nRecLevel + 1,
            nSRSDimension, pszSRSName);
        if (poGeom == nullptr || !OGR_GT_IsCurve(poGeom->getGeometryType()))
        {
            CPLError(
                CE_Failure, CPLE_AppDefined,
                "Curve: Got %.500s geometry as Member instead of segments.",
                poGeom ? poGeom->getGeometryName() : "NULL");
            return nullptr;
        }

        return poGeom;
    }

    /* -------------------------------------------------------------------- */
    /*      segments                                                        */
    /* -------------------------------------------------------------------- */
    if (EQUAL(pszBaseGeometry, "segments"))
    {
        std::unique_ptr<OGRCurve> poCurve;
        std::unique_ptr<OGRCompoundCurve> poCC;
        bool bChildrenAreAllLineString = true;

        bool bLastCurveWasApproximateArc = false;
        bool bLastCurveWasApproximateArcInvertedAxisOrder = false;
        double dfLastCurveApproximateArcRadius = 0.0;

        for (const CPLXMLNode *psChild = psNode->psChild; psChild != nullptr;
             psChild = psChild->psNext)

        {
            if (psChild->eType == CXT_Element
                // && (EQUAL(BareGMLElement(psChild->pszValue),
                //           "LineStringSegment") ||
                //     EQUAL(BareGMLElement(psChild->pszValue),
                //           "GeodesicString") ||
                //    EQUAL(BareGMLElement(psChild->pszValue), "Arc") ||
                //    EQUAL(BareGMLElement(psChild->pszValue), "Circle"))
            )
            {
                auto poGeom = GML2OGRGeometry_XMLNode_Internal(
                    psChild, nPseudoBoolGetSecondaryGeometryOption,
                    nRecLevel + 1, nSRSDimension, pszSRSName);
                if (poGeom == nullptr ||
                    !OGR_GT_IsCurve(poGeom->getGeometryType()))
                {
                    CPLError(CE_Failure, CPLE_AppDefined,
                             "segments: Got %.500s geometry as Member "
                             "instead of curve.",
                             poGeom ? poGeom->getGeometryName() : "NULL");
                    return nullptr;
                }

                // Ad-hoc logic to handle nicely connecting ArcByCenterPoint
                // with consecutive curves, as found in some AIXM files.
                bool bIsApproximateArc = false;
                if (strcmp(BareGMLElement(psChild->pszValue),
                           "ArcByCenterPoint") == 0)
                {
                    storeArcByCenterPointParameters(
                        psChild, pszSRSName, bIsApproximateArc,
                        dfLastCurveApproximateArcRadius,
                        bLastCurveWasApproximateArcInvertedAxisOrder);
                }

                if (wkbFlatten(poGeom->getGeometryType()) != wkbLineString)
                    bChildrenAreAllLineString = false;

                if (poCC == nullptr && poCurve == nullptr)
                {
                    poCurve.reset(poGeom.release()->toCurve());
                }
                else
                {
                    if (poCC == nullptr)
                    {
                        poCC = std::make_unique<OGRCompoundCurve>();
                        if (poCC->addCurve(std::move(poCurve)) != OGRERR_NONE)
                        {
                            return nullptr;
                        }
                        poCurve.reset();
                    }

                    connectArcByCenterPointToOtherSegments(
                        poGeom.get(), poCC.get(), bIsApproximateArc,
                        bLastCurveWasApproximateArc,
                        dfLastCurveApproximateArcRadius,
                        bLastCurveWasApproximateArcInvertedAxisOrder);

                    auto poAsCurve =
                        std::unique_ptr<OGRCurve>(poGeom.release()->toCurve());
                    if (poCC->addCurve(std::move(poAsCurve)) != OGRERR_NONE)
                    {
                        return nullptr;
                    }
                }

                bLastCurveWasApproximateArc = bIsApproximateArc;
            }
        }

        if (poCurve != nullptr)
            return poCurve;
        if (poCC == nullptr)
            return nullptr;

        if (/* bCastToLinearTypeIfPossible && */ bChildrenAreAllLineString)
        {
            return std::unique_ptr<OGRLineString>(
                OGRCurve::CastToLineString(poCC.release()));
        }

        return poCC;
    }

    /* -------------------------------------------------------------------- */
    /*      MultiGeometry                                                   */
    /* CAUTION: OGR < 1.8.0 produced GML with GeometryCollection, which is  */
    /* not a valid GML 2 keyword! The right name is MultiGeometry. Let's be */
    /* tolerant with the non compliant files we produced.                   */
    /* -------------------------------------------------------------------- */
    if (EQUAL(pszBaseGeometry, "MultiGeometry") ||
        EQUAL(pszBaseGeometry, "GeometryCollection"))
    {
        auto poGC = std::make_unique<OGRGeometryCollection>();

        // Collect geoms.
        for (const CPLXMLNode *psChild = psNode->psChild; psChild != nullptr;
             psChild = psChild->psNext)
        {
            if (psChild->eType == CXT_Element &&
                EQUAL(BareGMLElement(psChild->pszValue), "geometryMember"))
            {
                const CPLXMLNode *psGeometryChild = GetChildElement(psChild);

                if (psGeometryChild != nullptr)
                {
                    auto poGeom = GML2OGRGeometry_XMLNode_Internal(
                        psGeometryChild, nPseudoBoolGetSecondaryGeometryOption,
                        nRecLevel + 1, nSRSDimension, pszSRSName);
                    if (poGeom == nullptr)
                    {
                        CPLError(CE_Failure, CPLE_AppDefined,
                                 "GeometryCollection: Failed to get geometry "
                                 "in geometryMember");
                        return nullptr;
                    }

                    poGC->addGeometry(std::move(poGeom));
                }
            }
            else if (psChild->eType == CXT_Element &&
                     EQUAL(BareGMLElement(psChild->pszValue),
                           "geometryMembers"))
            {
                for (const CPLXMLNode *psChild2 = psChild->psChild;
                     psChild2 != nullptr; psChild2 = psChild2->psNext)
                {
                    if (psChild2->eType == CXT_Element)
                    {
                        auto poGeom = GML2OGRGeometry_XMLNode_Internal(
                            psChild2, nPseudoBoolGetSecondaryGeometryOption,
                            nRecLevel + 1, nSRSDimension, pszSRSName);
                        if (poGeom == nullptr)
                        {
                            CPLError(
                                CE_Failure, CPLE_AppDefined,
                                "GeometryCollection: Failed to get geometry "
                                "in geometryMember");
                            return nullptr;
                        }

                        poGC->addGeometry(std::move(poGeom));
                    }
                }
            }
        }

        return poGC;
    }

    /* -------------------------------------------------------------------- */
    /*      Directed Edge                                                   */
    /* -------------------------------------------------------------------- */
    if (EQUAL(pszBaseGeometry, "directedEdge"))
    {
        // Collect edge.
        const CPLXMLNode *psEdge = FindBareXMLChild(psNode, "Edge");
        if (psEdge == nullptr)
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Failed to get Edge element in directedEdge");
            return nullptr;
        }

        // TODO(schwehr): Localize vars after removing gotos.
        std::unique_ptr<OGRGeometry> poGeom;
        const CPLXMLNode *psNodeElement = nullptr;
        const CPLXMLNode *psPointProperty = nullptr;
        const CPLXMLNode *psPoint = nullptr;
        bool bNodeOrientation = true;
        std::unique_ptr<OGRPoint> poPositiveNode;
        std::unique_ptr<OGRPoint> poNegativeNode;

        const bool bEdgeOrientation = GetElementOrientation(psNode);

        if (bGetSecondaryGeometry)
        {
            const CPLXMLNode *psdirectedNode =
                FindBareXMLChild(psEdge, "directedNode");
            if (psdirectedNode == nullptr)
                goto nonode;

            bNodeOrientation = GetElementOrientation(psdirectedNode);

            psNodeElement = FindBareXMLChild(psdirectedNode, "Node");
            if (psNodeElement == nullptr)
                goto nonode;

            psPointProperty = FindBareXMLChild(psNodeElement, "pointProperty");
            if (psPointProperty == nullptr)
                psPointProperty =
                    FindBareXMLChild(psNodeElement, "connectionPointProperty");
            if (psPointProperty == nullptr)
                goto nonode;

            psPoint = FindBareXMLChild(psPointProperty, "Point");
            if (psPoint == nullptr)
                psPoint = FindBareXMLChild(psPointProperty, "ConnectionPoint");
            if (psPoint == nullptr)
                goto nonode;

            poGeom = GML2OGRGeometry_XMLNode_Internal(
                psPoint, nPseudoBoolGetSecondaryGeometryOption, nRecLevel + 1,
                nSRSDimension, pszSRSName, true);
            if (poGeom == nullptr ||
                wkbFlatten(poGeom->getGeometryType()) != wkbPoint)
            {
                // CPLError( CE_Failure, CPLE_AppDefined,
                //           "Got %.500s geometry as Member instead of POINT.",
                //           poGeom ? poGeom->getGeometryName() : "NULL" );
                goto nonode;
            }

            {
                OGRPoint *poPoint = poGeom.release()->toPoint();
                if ((bNodeOrientation == bEdgeOrientation) != bOrientation)
                    poPositiveNode.reset(poPoint);
                else
                    poNegativeNode.reset(poPoint);
            }

            // Look for the other node.
            psdirectedNode = psdirectedNode->psNext;
            while (psdirectedNode != nullptr &&
                   !EQUAL(psdirectedNode->pszValue, "directedNode"))
                psdirectedNode = psdirectedNode->psNext;
            if (psdirectedNode == nullptr)
                goto nonode;

            if (GetElementOrientation(psdirectedNode) == bNodeOrientation)
                goto nonode;

            psNodeElement = FindBareXMLChild(psEdge, "Node");
            if (psNodeElement == nullptr)
                goto nonode;

            psPointProperty = FindBareXMLChild(psNodeElement, "pointProperty");
            if (psPointProperty == nullptr)
                psPointProperty =
                    FindBareXMLChild(psNodeElement, "connectionPointProperty");
            if (psPointProperty == nullptr)
                goto nonode;

            psPoint = FindBareXMLChild(psPointProperty, "Point");
            if (psPoint == nullptr)
                psPoint = FindBareXMLChild(psPointProperty, "ConnectionPoint");
            if (psPoint == nullptr)
                goto nonode;

            poGeom = GML2OGRGeometry_XMLNode_Internal(
                psPoint, nPseudoBoolGetSecondaryGeometryOption, nRecLevel + 1,
                nSRSDimension, pszSRSName, true);
            if (poGeom == nullptr ||
                wkbFlatten(poGeom->getGeometryType()) != wkbPoint)
            {
                // CPLError( CE_Failure, CPLE_AppDefined,
                //           "Got %.500s geometry as Member instead of POINT.",
                //           poGeom ? poGeom->getGeometryName() : "NULL" );
                goto nonode;
            }

            {
                OGRPoint *poPoint = poGeom.release()->toPoint();
                if ((bNodeOrientation == bEdgeOrientation) != bOrientation)
                    poNegativeNode.reset(poPoint);
                else
                    poPositiveNode.reset(poPoint);
            }

            {
                // Create a scope so that poMP can be initialized with goto
                // above and label below.
                auto poMP = std::make_unique<OGRMultiPoint>();
                poMP->addGeometry(std::move(poNegativeNode));
                poMP->addGeometry(std::move(poPositiveNode));

                return poMP;
            }
        nonode:;
        }

        // Collect curveproperty.
        const CPLXMLNode *psCurveProperty =
            FindBareXMLChild(psEdge, "curveProperty");
        if (psCurveProperty == nullptr)
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "directedEdge: Failed to get curveProperty in Edge");
            return nullptr;
        }

        const CPLXMLNode *psCurve =
            FindBareXMLChild(psCurveProperty, "LineString");
        if (psCurve == nullptr)
            psCurve = FindBareXMLChild(psCurveProperty, "Curve");
        if (psCurve == nullptr)
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "directedEdge: Failed to get LineString or "
                     "Curve tag in curveProperty");
            return nullptr;
        }

        auto poLineStringBeforeCast = GML2OGRGeometry_XMLNode_Internal(
            psCurve, nPseudoBoolGetSecondaryGeometryOption, nRecLevel + 1,
            nSRSDimension, pszSRSName, true);
        if (poLineStringBeforeCast == nullptr ||
            wkbFlatten(poLineStringBeforeCast->getGeometryType()) !=
                wkbLineString)
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Got %.500s geometry as Member instead of LINESTRING.",
                     poLineStringBeforeCast
                         ? poLineStringBeforeCast->getGeometryName()
                         : "NULL");
            return nullptr;
        }
        auto poLineString = std::unique_ptr<OGRLineString>(
            poLineStringBeforeCast.release()->toLineString());

        if (bGetSecondaryGeometry)
        {
            // Choose a point based on the orientation.
            poNegativeNode = std::make_unique<OGRPoint>();
            poPositiveNode = std::make_unique<OGRPoint>();
            if (bEdgeOrientation == bOrientation)
            {
                poLineString->StartPoint(poNegativeNode.get());
                poLineString->EndPoint(poPositiveNode.get());
            }
            else
            {
                poLineString->StartPoint(poPositiveNode.get());
                poLineString->EndPoint(poNegativeNode.get());
            }

            auto poMP = std::make_unique<OGRMultiPoint>();
            poMP->addGeometry(std::move(poNegativeNode));
            poMP->addGeometry(std::move(poPositiveNode));
            return poMP;
        }

        // correct orientation of the line string
        if (bEdgeOrientation != bOrientation)
        {
            int iStartCoord = 0;
            int iEndCoord = poLineString->getNumPoints() - 1;
            OGRPoint oTempStartPoint;
            OGRPoint oTempEndPoint;
            while (iStartCoord < iEndCoord)
            {
                poLineString->getPoint(iStartCoord, &oTempStartPoint);
                poLineString->getPoint(iEndCoord, &oTempEndPoint);
                poLineString->setPoint(iStartCoord, &oTempEndPoint);
                poLineString->setPoint(iEndCoord, &oTempStartPoint);
                iStartCoord++;
                iEndCoord--;
            }
        }
        return poLineString;
    }

    /* -------------------------------------------------------------------- */
    /*      TopoCurve                                                       */
    /* -------------------------------------------------------------------- */
    if (EQUAL(pszBaseGeometry, "TopoCurve"))
    {
        std::unique_ptr<OGRMultiLineString> poMLS;
        std::unique_ptr<OGRMultiPoint> poMP;

        if (bGetSecondaryGeometry)
            poMP = std::make_unique<OGRMultiPoint>();
        else
            poMLS = std::make_unique<OGRMultiLineString>();

        // Collect directedEdges.
        for (const CPLXMLNode *psChild = psNode->psChild; psChild != nullptr;
             psChild = psChild->psNext)
        {
            if (psChild->eType == CXT_Element &&
                EQUAL(BareGMLElement(psChild->pszValue), "directedEdge"))
            {
                auto poGeom = GML2OGRGeometry_XMLNode_Internal(
                    psChild, nPseudoBoolGetSecondaryGeometryOption,
                    nRecLevel + 1, nSRSDimension, pszSRSName);
                if (poGeom == nullptr)
                {
                    CPLError(CE_Failure, CPLE_AppDefined,
                             "Failed to get geometry in directedEdge");
                    return nullptr;
                }

                // Add the two points corresponding to the two nodes to poMP.
                if (bGetSecondaryGeometry &&
                    wkbFlatten(poGeom->getGeometryType()) == wkbMultiPoint)
                {
                    auto poMultiPoint = std::unique_ptr<OGRMultiPoint>(
                        poGeom.release()->toMultiPoint());

                    // TODO: TopoCurve geometries with more than one
                    //       directedEdge elements were not tested.
                    if (poMP->getNumGeometries() <= 0 ||
                        !(poMP->getGeometryRef(poMP->getNumGeometries() - 1)
                              ->Equals(poMultiPoint->getGeometryRef(0))))
                    {
                        poMP->addGeometry(poMultiPoint->getGeometryRef(0));
                    }
                    poMP->addGeometry(poMultiPoint->getGeometryRef(1));
                }
                else if (!bGetSecondaryGeometry &&
                         wkbFlatten(poGeom->getGeometryType()) == wkbLineString)
                {
                    poMLS->addGeometry(std::move(poGeom));
                }
                else
                {
                    CPLError(CE_Failure, CPLE_AppDefined,
                             "Got %.500s geometry as Member instead of %s.",
                             poGeom->getGeometryName(),
                             bGetSecondaryGeometry ? "MULTIPOINT"
                                                   : "LINESTRING");
                    return nullptr;
                }
            }
        }

        if (bGetSecondaryGeometry)
            return poMP;

        return poMLS;
    }

    /* -------------------------------------------------------------------- */
    /*      TopoSurface                                                     */
    /* -------------------------------------------------------------------- */
    if (EQUAL(pszBaseGeometry, "TopoSurface"))
    {
        /****************************************************************/
        /* applying the FaceHoleNegative = false rules                  */
        /*                                                              */
        /* - each <TopoSurface> is expected to represent a MultiPolygon */
        /* - each <Face> is expected to represent a distinct Polygon,   */
        /*   this including any possible Interior Ring (holes);         */
        /*   orientation="+/-" plays no role at all to identify "holes" */
        /* - each <Edge> within a <Face> may indifferently represent    */
        /*   an element of the Exterior or Interior Boundary; relative  */
        /*   order of <Edges> is absolutely irrelevant.                 */
        /****************************************************************/
        /* Contributor: Alessandro Furieri, a.furieri@lqt.it            */
        /* Developed for Faunalia (http://www.faunalia.it)              */
        /* with funding from Regione Toscana -                          */
        /* Settore SISTEMA INFORMATIVO TERRITORIALE ED AMBIENTALE       */
        /****************************************************************/
        if (!bFaceHoleNegative)
        {
            if (bGetSecondaryGeometry)
                return nullptr;

#ifndef HAVE_GEOS
            static bool bWarningAlreadyEmitted = false;
            if (!bWarningAlreadyEmitted)
            {
                CPLError(
                    CE_Failure, CPLE_AppDefined,
                    "Interpreating that GML TopoSurface geometry requires GDAL "
                    "to be built with GEOS support.  As a workaround, you can "
                    "try defining the GML_FACE_HOLE_NEGATIVE configuration "
                    "option to YES, so that the 'old' interpretation algorithm "
                    "is used. But be warned that the result might be "
                    "incorrect.");
                bWarningAlreadyEmitted = true;
            }
            return nullptr;
#else
            auto poTS = std::make_unique<OGRMultiPolygon>();

            // Collect directed faces.
            for (const CPLXMLNode *psChild = psNode->psChild;
                 psChild != nullptr; psChild = psChild->psNext)
            {
                if (psChild->eType == CXT_Element &&
                    EQUAL(BareGMLElement(psChild->pszValue), "directedFace"))
                {
                    // Collect next face (psChild->psChild).
                    const CPLXMLNode *psFaceChild = GetChildElement(psChild);

                    while (
                        psFaceChild != nullptr &&
                        !(psFaceChild->eType == CXT_Element &&
                          EQUAL(BareGMLElement(psFaceChild->pszValue), "Face")))
                        psFaceChild = psFaceChild->psNext;

                    if (psFaceChild == nullptr)
                        continue;

                    auto poCollectedGeom =
                        std::make_unique<OGRMultiLineString>();

                    // Collect directed edges of the face.
                    for (const CPLXMLNode *psDirectedEdgeChild =
                             psFaceChild->psChild;
                         psDirectedEdgeChild != nullptr;
                         psDirectedEdgeChild = psDirectedEdgeChild->psNext)
                    {
                        if (psDirectedEdgeChild->eType == CXT_Element &&
                            EQUAL(BareGMLElement(psDirectedEdgeChild->pszValue),
                                  "directedEdge"))
                        {
                            auto poEdgeGeom = GML2OGRGeometry_XMLNode_Internal(
                                psDirectedEdgeChild,
                                nPseudoBoolGetSecondaryGeometryOption,
                                nRecLevel + 1, nSRSDimension, pszSRSName, true);

                            if (poEdgeGeom == nullptr ||
                                wkbFlatten(poEdgeGeom->getGeometryType()) !=
                                    wkbLineString)
                            {
                                CPLError(
                                    CE_Failure, CPLE_AppDefined,
                                    "Failed to get geometry in directedEdge");
                                return nullptr;
                            }

                            poCollectedGeom->addGeometry(std::move(poEdgeGeom));
                        }
                    }

                    auto poFaceCollectionGeom = std::unique_ptr<OGRGeometry>(
                        poCollectedGeom->Polygonize());
                    if (poFaceCollectionGeom == nullptr)
                    {
                        CPLError(CE_Failure, CPLE_AppDefined,
                                 "Failed to assemble Edges in Face");
                        return nullptr;
                    }

                    auto poFaceGeom =
                        GML2FaceExtRing(poFaceCollectionGeom.get());

                    if (poFaceGeom == nullptr)
                    {
                        CPLError(CE_Failure, CPLE_AppDefined,
                                 "Failed to build Polygon for Face");
                        return nullptr;
                    }
                    else
                    {
                        int iCount = poTS->getNumGeometries();
                        if (iCount == 0)
                        {
                            // Inserting the first Polygon.
                            poTS->addGeometry(std::move(poFaceGeom));
                        }
                        else
                        {
                            // Using Union to add the current Polygon.
                            auto poUnion = std::unique_ptr<OGRGeometry>(
                                poTS->Union(poFaceGeom.get()));
                            if (poUnion == nullptr)
                            {
                                CPLError(CE_Failure, CPLE_AppDefined,
                                         "Failed Union for TopoSurface");
                                return nullptr;
                            }
                            if (wkbFlatten(poUnion->getGeometryType()) ==
                                wkbPolygon)
                            {
                                // Forcing to be a MultiPolygon.
                                poTS = std::make_unique<OGRMultiPolygon>();
                                poTS->addGeometry(std::move(poUnion));
                            }
                            else if (wkbFlatten(poUnion->getGeometryType()) ==
                                     wkbMultiPolygon)
                            {
                                poTS.reset(poUnion.release()->toMultiPolygon());
                            }
                            else
                            {
                                CPLError(CE_Failure, CPLE_AppDefined,
                                         "Unexpected geometry type resulting "
                                         "from Union for TopoSurface");
                                return nullptr;
                            }
                        }
                    }
                }
            }

            return poTS;
#endif  // HAVE_GEOS
        }

        /****************************************************************/
        /* applying the FaceHoleNegative = true rules                   */
        /*                                                              */
        /* - each <TopoSurface> is expected to represent a MultiPolygon */
        /* - any <Face> declaring orientation="+" is expected to        */
        /*   represent an Exterior Ring (no holes are allowed)          */
        /* - any <Face> declaring orientation="-" is expected to        */
        /*   represent an Interior Ring (hole) belonging to the latest  */
        /*   Exterior Ring.                                             */
        /* - <Edges> within the same <Face> are expected to be          */
        /*   arranged in geometrically adjacent and consecutive         */
        /*   sequence.                                                  */
        /****************************************************************/
        if (bGetSecondaryGeometry)
            return nullptr;
        bool bFaceOrientation = true;
        auto poTS = std::make_unique<OGRPolygon>();

        // Collect directed faces.
        for (const CPLXMLNode *psChild = psNode->psChild; psChild != nullptr;
             psChild = psChild->psNext)
        {
            if (psChild->eType == CXT_Element &&
                EQUAL(BareGMLElement(psChild->pszValue), "directedFace"))
            {
                bFaceOrientation = GetElementOrientation(psChild);

                // Collect next face (psChild->psChild).
                const CPLXMLNode *psFaceChild = GetChildElement(psChild);
                while (psFaceChild != nullptr &&
                       !EQUAL(BareGMLElement(psFaceChild->pszValue), "Face"))
                    psFaceChild = psFaceChild->psNext;

                if (psFaceChild == nullptr)
                    continue;

                auto poFaceGeom = std::make_unique<OGRLinearRing>();

                // Collect directed edges of the face.
                for (const CPLXMLNode *psDirectedEdgeChild =
                         psFaceChild->psChild;
                     psDirectedEdgeChild != nullptr;
                     psDirectedEdgeChild = psDirectedEdgeChild->psNext)
                {
                    if (psDirectedEdgeChild->eType == CXT_Element &&
                        EQUAL(BareGMLElement(psDirectedEdgeChild->pszValue),
                              "directedEdge"))
                    {
                        auto poEdgeGeom = GML2OGRGeometry_XMLNode_Internal(
                            psDirectedEdgeChild,
                            nPseudoBoolGetSecondaryGeometryOption,
                            nRecLevel + 1, nSRSDimension, pszSRSName, true,
                            bFaceOrientation);

                        if (poEdgeGeom == nullptr ||
                            wkbFlatten(poEdgeGeom->getGeometryType()) !=
                                wkbLineString)
                        {
                            CPLError(CE_Failure, CPLE_AppDefined,
                                     "Failed to get geometry in directedEdge");
                            return nullptr;
                        }

                        auto poEdgeGeomLS = std::unique_ptr<OGRLineString>(
                            poEdgeGeom.release()->toLineString());
                        if (!bFaceOrientation)
                        {
                            OGRLineString *poLS = poEdgeGeomLS.get();
                            OGRLineString *poAddLS = poFaceGeom.get();

                            // TODO(schwehr): Use AlmostEqual.
                            const double epsilon = 1.0e-14;
                            if (poAddLS->getNumPoints() < 2)
                            {
                                // Skip it.
                            }
                            else if (poLS->getNumPoints() > 0 &&
                                     fabs(poLS->getX(poLS->getNumPoints() - 1) -
                                          poAddLS->getX(0)) < epsilon &&
                                     fabs(poLS->getY(poLS->getNumPoints() - 1) -
                                          poAddLS->getY(0)) < epsilon &&
                                     fabs(poLS->getZ(poLS->getNumPoints() - 1) -
                                          poAddLS->getZ(0)) < epsilon)
                            {
                                // Skip the first point of the new linestring to
                                // avoid invalidate duplicate points.
                                poLS->addSubLineString(poAddLS, 1);
                            }
                            else
                            {
                                // Add the whole new line string.
                                poLS->addSubLineString(poAddLS);
                            }
                            poFaceGeom->empty();
                        }
                        // TODO(schwehr): Suspicious that poLS overwritten
                        // without else.
                        OGRLineString *poLS = poFaceGeom.get();
                        OGRLineString *poAddLS = poEdgeGeomLS.get();
                        if (poAddLS->getNumPoints() < 2)
                        {
                            // Skip it.
                        }
                        else if (poLS->getNumPoints() > 0 &&
                                 fabs(poLS->getX(poLS->getNumPoints() - 1) -
                                      poAddLS->getX(0)) < 1e-14 &&
                                 fabs(poLS->getY(poLS->getNumPoints() - 1) -
                                      poAddLS->getY(0)) < 1e-14 &&
                                 fabs(poLS->getZ(poLS->getNumPoints() - 1) -
                                      poAddLS->getZ(0)) < 1e-14)
                        {
                            // Skip the first point of the new linestring to
                            // avoid invalidate duplicate points.
                            poLS->addSubLineString(poAddLS, 1);
                        }
                        else
                        {
                            // Add the whole new line string.
                            poLS->addSubLineString(poAddLS);
                        }
                    }
                }

                // if( poFaceGeom == NULL )
                // {
                //     CPLError( CE_Failure, CPLE_AppDefined,
                //               "Failed to get Face geometry in directedFace"
                //               );
                //     delete poFaceGeom;
                //     return NULL;
                // }

                poTS->addRing(std::move(poFaceGeom));
            }
        }

        // if( poTS == NULL )
        // {
        //     CPLError( CE_Failure, CPLE_AppDefined,
        //               "Failed to get TopoSurface geometry" );
        //     delete poTS;
        //     return NULL;
        // }

        return poTS;
    }

    /* -------------------------------------------------------------------- */
    /*      Surface                                                         */
    /* -------------------------------------------------------------------- */
    if (EQUAL(pszBaseGeometry, "Surface") ||
        EQUAL(pszBaseGeometry, "ElevatedSurface") /* AIXM */)
    {
        // Find outer ring.
        const CPLXMLNode *psChild = FindBareXMLChild(psNode, "patches");
        if (psChild == nullptr)
            psChild = FindBareXMLChild(psNode, "polygonPatches");
        if (psChild == nullptr)
            psChild = FindBareXMLChild(psNode, "trianglePatches");

        psChild = GetChildElement(psChild);
        if (psChild == nullptr)
        {
            // <gml:Surface/> and <gml:Surface><gml:patches/></gml:Surface> are
            // valid GML.
            return std::make_unique<OGRPolygon>();
        }

        OGRMultiSurface *poMSPtr = nullptr;
        std::unique_ptr<OGRGeometry> poResultPoly;
        std::unique_ptr<OGRGeometry> poResultTri;
        OGRTriangulatedSurface *poTINPtr = nullptr;
        for (; psChild != nullptr; psChild = psChild->psNext)
        {
            if (psChild->eType == CXT_Element &&
                (EQUAL(BareGMLElement(psChild->pszValue), "PolygonPatch") ||
                 EQUAL(BareGMLElement(psChild->pszValue), "Rectangle")))
            {
                auto poGeom = GML2OGRGeometry_XMLNode_Internal(
                    psChild, nPseudoBoolGetSecondaryGeometryOption,
                    nRecLevel + 1, nSRSDimension, pszSRSName);
                if (poGeom == nullptr)
                {
                    return nullptr;
                }

                const OGRwkbGeometryType eGeomType =
                    wkbFlatten(poGeom->getGeometryType());

                if (poResultPoly == nullptr)
                    poResultPoly = std::move(poGeom);
                else
                {
                    if (poMSPtr == nullptr)
                    {
                        std::unique_ptr<OGRMultiSurface> poMS;
                        if (wkbFlatten(poResultPoly->getGeometryType()) ==
                                wkbPolygon &&
                            eGeomType == wkbPolygon)
                            poMS = std::make_unique<OGRMultiPolygon>();
                        else
                            poMS = std::make_unique<OGRMultiSurface>();
                        OGRErr eErr =
                            poMS->addGeometry(std::move(poResultPoly));
                        CPL_IGNORE_RET_VAL(eErr);
                        CPLAssert(eErr == OGRERR_NONE);
                        poResultPoly = std::move(poMS);
                        poMSPtr = cpl::down_cast<OGRMultiSurface *>(
                            poResultPoly.get());
                    }
                    else if (eGeomType != wkbPolygon &&
                             wkbFlatten(poResultPoly->getGeometryType()) ==
                                 wkbMultiPolygon)
                    {
                        OGRMultiPolygon *poMultiPoly =
                            poResultPoly.release()->toMultiPolygon();
                        poResultPoly.reset(
                            OGRMultiPolygon::CastToMultiSurface(poMultiPoly));
                        poMSPtr = cpl::down_cast<OGRMultiSurface *>(
                            poResultPoly.get());
                    }
                    OGRErr eErr = poMSPtr->addGeometry(std::move(poGeom));
                    CPL_IGNORE_RET_VAL(eErr);
                    CPLAssert(eErr == OGRERR_NONE);
                }
            }
            else if (psChild->eType == CXT_Element &&
                     EQUAL(BareGMLElement(psChild->pszValue), "Triangle"))
            {
                auto poGeom = GML2OGRGeometry_XMLNode_Internal(
                    psChild, nPseudoBoolGetSecondaryGeometryOption,
                    nRecLevel + 1, nSRSDimension, pszSRSName);
                if (poGeom == nullptr)
                {
                    return nullptr;
                }

                if (poResultTri == nullptr)
                    poResultTri = std::move(poGeom);
                else
                {
                    if (poTINPtr == nullptr)
                    {
                        auto poTIN = std::make_unique<OGRTriangulatedSurface>();
                        OGRErr eErr =
                            poTIN->addGeometry(std::move(poResultTri));
                        CPL_IGNORE_RET_VAL(eErr);
                        CPLAssert(eErr == OGRERR_NONE);
                        poResultTri = std::move(poTIN);
                        poTINPtr = cpl::down_cast<OGRTriangulatedSurface *>(
                            poResultTri.get());
                    }
                    OGRErr eErr = poTINPtr->addGeometry(std::move(poGeom));
                    CPL_IGNORE_RET_VAL(eErr);
                    CPLAssert(eErr == OGRERR_NONE);
                }
            }
        }

        if (poResultTri == nullptr && poResultPoly == nullptr)
            return nullptr;

        if (poResultTri == nullptr)
            return poResultPoly;
        else if (poResultPoly == nullptr)
            return poResultTri;
        else
        {
            auto poGC = std::make_unique<OGRGeometryCollection>();
            poGC->addGeometry(std::move(poResultTri));
            poGC->addGeometry(std::move(poResultPoly));
            return poGC;
        }
    }

    /* -------------------------------------------------------------------- */
    /*      TriangulatedSurface                                             */
    /* -------------------------------------------------------------------- */
    if (EQUAL(pszBaseGeometry, "TriangulatedSurface") ||
        EQUAL(pszBaseGeometry, "Tin"))
    {
        // Find trianglePatches.
        const CPLXMLNode *psChild = FindBareXMLChild(psNode, "trianglePatches");
        if (psChild == nullptr)
            psChild = FindBareXMLChild(psNode, "patches");

        psChild = GetChildElement(psChild);
        if (psChild == nullptr)
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Missing <trianglePatches> for %s.", pszBaseGeometry);
            return nullptr;
        }

        auto poTIN = std::make_unique<OGRTriangulatedSurface>();
        for (; psChild != nullptr; psChild = psChild->psNext)
        {
            if (psChild->eType == CXT_Element &&
                EQUAL(BareGMLElement(psChild->pszValue), "Triangle"))
            {
                auto poTriangle = GML2OGRGeometry_XMLNode_Internal(
                    psChild, nPseudoBoolGetSecondaryGeometryOption,
                    nRecLevel + 1, nSRSDimension, pszSRSName);
                if (poTriangle == nullptr)
                {
                    return nullptr;
                }
                else
                {
                    poTIN->addGeometry(std::move(poTriangle));
                }
            }
        }

        return poTIN;
    }

    /* -------------------------------------------------------------------- */
    /*      PolyhedralSurface                                               */
    /* -------------------------------------------------------------------- */
    if (EQUAL(pszBaseGeometry, "PolyhedralSurface"))
    {
        // Find polygonPatches.
        const CPLXMLNode *psParent = FindBareXMLChild(psNode, "polygonPatches");
        if (psParent == nullptr)
        {
            if (GetChildElement(psNode) == nullptr)
            {
                // This is empty PolyhedralSurface.
                return std::make_unique<OGRPolyhedralSurface>();
            }
            else
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "Missing <polygonPatches> for %s.", pszBaseGeometry);
                return nullptr;
            }
        }

        const CPLXMLNode *psChild = GetChildElement(psParent);
        if (psChild == nullptr)
        {
            // This is empty PolyhedralSurface.
            return std::make_unique<OGRPolyhedralSurface>();
        }
        else if (!EQUAL(BareGMLElement(psChild->pszValue), "PolygonPatch"))
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Missing <PolygonPatch> for %s.", pszBaseGeometry);
            return nullptr;
        }

        // Each psParent has the tags corresponding to <gml:polygonPatches>
        // Each psChild has the tags corresponding to <gml:PolygonPatch>
        // Each PolygonPatch has a set of polygons enclosed in a
        // OGRPolyhedralSurface.
        auto poGC = std::make_unique<OGRGeometryCollection>();
        for (; psParent != nullptr; psParent = psParent->psNext)
        {
            psChild = GetChildElement(psParent);
            if (psChild == nullptr)
                continue;
            auto poPS = std::make_unique<OGRPolyhedralSurface>();
            for (; psChild != nullptr; psChild = psChild->psNext)
            {
                if (psChild->eType == CXT_Element &&
                    EQUAL(BareGMLElement(psChild->pszValue), "PolygonPatch"))
                {
                    auto poPolygon = GML2OGRGeometry_XMLNode_Internal(
                        psChild, nPseudoBoolGetSecondaryGeometryOption,
                        nRecLevel + 1, nSRSDimension, pszSRSName);
                    if (poPolygon == nullptr)
                    {
                        CPLError(CE_Failure, CPLE_AppDefined,
                                 "Wrong geometry type for %s.",
                                 pszBaseGeometry);
                        return nullptr;
                    }

                    else if (wkbFlatten(poPolygon->getGeometryType()) ==
                             wkbPolygon)
                    {
                        poPS->addGeometry(std::move(poPolygon));
                    }
                    else if (wkbFlatten(poPolygon->getGeometryType()) ==
                             wkbCurvePolygon)
                    {
                        poPS->addGeometryDirectly(
                            OGRGeometryFactory::forceToPolygon(
                                poPolygon.release()));
                    }
                    else
                    {
                        CPLError(CE_Failure, CPLE_AppDefined,
                                 "Wrong geometry type for %s.",
                                 pszBaseGeometry);
                        return nullptr;
                    }
                }
            }
            poGC->addGeometry(std::move(poPS));
        }

        if (poGC->getNumGeometries() == 0)
        {
            return nullptr;
        }
        else if (poGC->getNumGeometries() == 1)
        {
            auto poResult =
                std::unique_ptr<OGRGeometry>(poGC->getGeometryRef(0));
            poGC->removeGeometry(0, FALSE);
            return poResult;
        }
        else
        {
            return poGC;
        }
    }

    /* -------------------------------------------------------------------- */
    /*      Solid                                                           */
    /* -------------------------------------------------------------------- */
    if (EQUAL(pszBaseGeometry, "Solid"))
    {
        const CPLXMLNode *psChild = FindBareXMLChild(psNode, "interior");
        if (psChild != nullptr)
        {
            static bool bWarnedOnce = false;
            if (!bWarnedOnce)
            {
                CPLError(CE_Warning, CPLE_AppDefined,
                         "<interior> elements of <Solid> are ignored");
                bWarnedOnce = true;
            }
        }

        // Find exterior element.
        psChild = FindBareXMLChild(psNode, "exterior");

        if (nSRSDimension == 0)
            nSRSDimension = 3;

        psChild = GetChildElement(psChild);
        if (psChild == nullptr)
        {
            // <gml:Solid/> and <gml:Solid><gml:exterior/></gml:Solid> are valid
            // GML.
            return std::make_unique<OGRPolyhedralSurface>();
        }

        if (EQUAL(BareGMLElement(psChild->pszValue), "CompositeSurface"))
        {
            auto poPS = std::make_unique<OGRPolyhedralSurface>();

            // Iterate over children.
            for (psChild = psChild->psChild; psChild != nullptr;
                 psChild = psChild->psNext)
            {
                const char *pszMemberElement =
                    BareGMLElement(psChild->pszValue);
                if (psChild->eType == CXT_Element &&
                    (EQUAL(pszMemberElement, "polygonMember") ||
                     EQUAL(pszMemberElement, "surfaceMember")))
                {
                    const CPLXMLNode *psSurfaceChild = GetChildElement(psChild);

                    if (psSurfaceChild != nullptr)
                    {
                        auto poGeom = GML2OGRGeometry_XMLNode_Internal(
                            psSurfaceChild,
                            nPseudoBoolGetSecondaryGeometryOption,
                            nRecLevel + 1, nSRSDimension, pszSRSName);
                        if (poGeom != nullptr &&
                            wkbFlatten(poGeom->getGeometryType()) == wkbPolygon)
                        {
                            poPS->addGeometry(std::move(poGeom));
                        }
                    }
                }
            }
            return poPS;
        }

        // Get the geometry inside <exterior>.
        auto poGeom = GML2OGRGeometry_XMLNode_Internal(
            psChild, nPseudoBoolGetSecondaryGeometryOption, nRecLevel + 1,
            nSRSDimension, pszSRSName);
        if (poGeom == nullptr)
        {
            CPLError(CE_Failure, CPLE_AppDefined, "Invalid exterior element");
            return nullptr;
        }

        return poGeom;
    }

    /* -------------------------------------------------------------------- */
    /*      OrientableSurface                                               */
    /* -------------------------------------------------------------------- */
    if (EQUAL(pszBaseGeometry, "OrientableSurface"))
    {
        // Find baseSurface.
        const CPLXMLNode *psChild = FindBareXMLChild(psNode, "baseSurface");

        psChild = GetChildElement(psChild);
        if (psChild == nullptr)
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Missing <baseSurface> for OrientableSurface.");
            return nullptr;
        }

        return GML2OGRGeometry_XMLNode_Internal(
            psChild, nPseudoBoolGetSecondaryGeometryOption, nRecLevel + 1,
            nSRSDimension, pszSRSName);
    }

    /* -------------------------------------------------------------------- */
    /*      SimplePolygon, SimpleRectangle, SimpleTriangle                  */
    /*      (GML 3.3 compact encoding)                                      */
    /* -------------------------------------------------------------------- */
    if (EQUAL(pszBaseGeometry, "SimplePolygon") ||
        EQUAL(pszBaseGeometry, "SimpleRectangle"))
    {
        auto poRing = std::make_unique<OGRLinearRing>();

        if (!ParseGMLCoordinates(psNode, poRing.get(), nSRSDimension))
        {
            return nullptr;
        }

        poRing->closeRings();

        auto poPolygon = std::make_unique<OGRPolygon>();
        poPolygon->addRing(std::move(poRing));
        return poPolygon;
    }

    if (EQUAL(pszBaseGeometry, "SimpleTriangle"))
    {
        auto poRing = std::make_unique<OGRLinearRing>();

        if (!ParseGMLCoordinates(psNode, poRing.get(), nSRSDimension))
        {
            return nullptr;
        }

        poRing->closeRings();

        auto poTriangle = std::make_unique<OGRTriangle>();
        poTriangle->addRing(std::move(poRing));
        return poTriangle;
    }

    /* -------------------------------------------------------------------- */
    /*      SimpleMultiPoint (GML 3.3 compact encoding)                     */
    /* -------------------------------------------------------------------- */
    if (EQUAL(pszBaseGeometry, "SimpleMultiPoint"))
    {
        auto poLS = std::make_unique<OGRLineString>();

        if (!ParseGMLCoordinates(psNode, poLS.get(), nSRSDimension))
        {
            return nullptr;
        }

        auto poMP = std::make_unique<OGRMultiPoint>();
        int nPoints = poLS->getNumPoints();
        for (int i = 0; i < nPoints; i++)
        {
            auto poPoint = std::make_unique<OGRPoint>();
            poLS->getPoint(i, poPoint.get());
            poMP->addGeometry(std::move(poPoint));
        }
        return poMP;
    }

    if (strcmp(pszBaseGeometry, "null") == 0)
    {
        return nullptr;
    }

    CPLError(CE_Failure, CPLE_AppDefined,
             "Unrecognized geometry type <%.500s>.", pszBaseGeometry);

    return nullptr;
}

/************************************************************************/
/*                      OGR_G_CreateFromGMLTree()                       */
/************************************************************************/

/** Create geometry from GML */
OGRGeometryH OGR_G_CreateFromGMLTree(const CPLXMLNode *psTree)

{
    return OGRGeometry::ToHandle(GML2OGRGeometry_XMLNode(psTree, -1));
}

/************************************************************************/
/*                        OGR_G_CreateFromGML()                         */
/************************************************************************/

/**
 * \brief Create geometry from GML.
 *
 * This method translates a fragment of GML containing only the geometry
 * portion into a corresponding OGRGeometry.  There are many limitations
 * on the forms of GML geometries supported by this parser, but they are
 * too numerous to list here.
 *
 * The following GML2 elements are parsed : Point, LineString, Polygon,
 * MultiPoint, MultiLineString, MultiPolygon, MultiGeometry.
 *
 * The following GML3 elements are parsed : Surface,
 * MultiSurface, PolygonPatch, Triangle, Rectangle, Curve, MultiCurve,
 * CompositeCurve, LineStringSegment, Arc, Circle, CompositeSurface,
 * OrientableSurface, Solid, Tin, TriangulatedSurface.
 *
 * Arc and Circle elements are returned as curves by default. Stroking to
 * linestrings can be done with
 * OGR_G_ForceTo(hGeom, OGR_GT_GetLinear(OGR_G_GetGeometryType(hGeom)), NULL).
 * A 4 degrees step is used by default, unless the user
 * has overridden the value with the OGR_ARC_STEPSIZE configuration variable.
 *
 * The C++ method OGRGeometryFactory::createFromGML() is the same as
 * this function.
 *
 * @param pszGML The GML fragment for the geometry.
 *
 * @return a geometry on success, or NULL on error.
 *
 * @see OGR_G_ForceTo()
 * @see OGR_GT_GetLinear()
 * @see OGR_G_GetGeometryType()
 */

OGRGeometryH OGR_G_CreateFromGML(const char *pszGML)

{
    if (pszGML == nullptr || strlen(pszGML) == 0)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "GML Geometry is empty in OGR_G_CreateFromGML().");
        return nullptr;
    }

    /* -------------------------------------------------------------------- */
    /*      Try to parse the XML snippet using the MiniXML API.  If this    */
    /*      fails, we assume the minixml api has already posted a CPL       */
    /*      error, and just return NULL.                                    */
    /* -------------------------------------------------------------------- */
    CPLXMLNode *psGML = CPLParseXMLString(pszGML);

    if (psGML == nullptr)
        return nullptr;

    /* -------------------------------------------------------------------- */
    /*      Convert geometry recursively.                                   */
    /* -------------------------------------------------------------------- */
    // Must be in synced in OGR_G_CreateFromGML(), OGRGMLLayer::OGRGMLLayer()
    // and GMLReader::GMLReader().
    const bool bFaceHoleNegative =
        CPLTestBool(CPLGetConfigOption("GML_FACE_HOLE_NEGATIVE", "NO"));
    OGRGeometry *poGeometry = GML2OGRGeometry_XMLNode(psGML, -1, 0, 0, false,
                                                      true, bFaceHoleNegative);

    CPLDestroyXMLNode(psGML);

    return OGRGeometry::ToHandle(poGeometry);
}
