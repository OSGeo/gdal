/******************************************************************************
 *
 * Project:  S-57 Reader
 * Purpose:  Implements polygon assembly from a bunch of arcs.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 1999, Frank Warmerdam
 * Copyright (c) 2009-2011, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include "cpl_port.h"
#include "ogr_api.h"

#include <cmath>
#include <cstddef>
#include <list>
#include <vector>

#include "ogr_core.h"
#include "ogr_geometry.h"
#include "cpl_conv.h"
#include "cpl_error.h"

/************************************************************************/
/*                            CheckPoints()                             */
/*                                                                      */
/*      Check if two points are closer than the current best            */
/*      distance.  Update the current best distance if they are.        */
/************************************************************************/

static bool CheckPoints(OGRLineString *poLine1, int iPoint1,
                        OGRLineString *poLine2, int iPoint2,
                        double *pdfDistance)

{
    if (pdfDistance == nullptr || *pdfDistance == 0)
    {
        if (poLine1->getX(iPoint1) == poLine2->getX(iPoint2) &&
            poLine1->getY(iPoint1) == poLine2->getY(iPoint2))
        {
            if (pdfDistance)
                *pdfDistance = 0.0;
            return true;
        }
        return false;
    }

    const double dfDeltaX =
        std::abs(poLine1->getX(iPoint1) - poLine2->getX(iPoint2));

    if (dfDeltaX > *pdfDistance)
        return false;

    const double dfDeltaY =
        std::abs(poLine1->getY(iPoint1) - poLine2->getY(iPoint2));

    if (dfDeltaY > *pdfDistance)
        return false;

    const double dfDistance = sqrt(dfDeltaX * dfDeltaX + dfDeltaY * dfDeltaY);

    if (dfDistance < *pdfDistance)
    {
        *pdfDistance = dfDistance;
        return true;
    }

    return false;
}

/************************************************************************/
/*                           AddEdgeToRing()                            */
/************************************************************************/

static void AddEdgeToRing(OGRLinearRing *poRing, OGRLineString *poLine,
                          bool bReverse, double dfTolerance)

{
    /* -------------------------------------------------------------------- */
    /*      Establish order and range of traverse.                          */
    /* -------------------------------------------------------------------- */
    const int nVertToAdd = poLine->getNumPoints();

    int iStart = bReverse ? nVertToAdd - 1 : 0;
    const int iEnd = bReverse ? 0 : nVertToAdd - 1;
    const int iStep = bReverse ? -1 : 1;

    /* -------------------------------------------------------------------- */
    /*      Should we skip a repeating vertex?                              */
    /* -------------------------------------------------------------------- */
    if (poRing->getNumPoints() > 0 &&
        CheckPoints(poRing, poRing->getNumPoints() - 1, poLine, iStart,
                    &dfTolerance))
    {
        iStart += iStep;
    }

    poRing->addSubLineString(poLine, iStart, iEnd);
}

/************************************************************************/
/*                      OGRBuildPolygonFromEdges()                      */
/************************************************************************/

/**
 * Build a ring from a bunch of arcs.
 *
 * @param hLines handle to an OGRGeometryCollection (or OGRMultiLineString)
 * containing the line string geometries to be built into rings.
 * @param bBestEffort not yet implemented???.
 * @param bAutoClose indicates if the ring should be close when first and
 * last points of the ring are the same.
 * @param dfTolerance tolerance into which two arcs are considered
 * close enough to be joined.
 * @param peErr OGRERR_NONE on success, or OGRERR_FAILURE on failure.
 * @return a handle to the new geometry, a polygon or multipolygon.
 *
 */

OGRGeometryH OGRBuildPolygonFromEdges(OGRGeometryH hLines,
                                      CPL_UNUSED int bBestEffort,
                                      int bAutoClose, double dfTolerance,
                                      OGRErr *peErr)

{
    if (hLines == nullptr)
    {
        if (peErr != nullptr)
            *peErr = OGRERR_NONE;
        return nullptr;
    }

    /* -------------------------------------------------------------------- */
    /*      Check for the case of a geometrycollection that can be          */
    /*      promoted to MultiLineString.                                    */
    /* -------------------------------------------------------------------- */
    OGRGeometry *poGeom = OGRGeometry::FromHandle(hLines);
    if (wkbFlatten(poGeom->getGeometryType()) == wkbGeometryCollection)
    {
        for (auto &&poMember : poGeom->toGeometryCollection())
        {
            if (wkbFlatten(poMember->getGeometryType()) != wkbLineString)
            {
                if (peErr != nullptr)
                    *peErr = OGRERR_FAILURE;
                CPLError(CE_Failure, CPLE_NotSupported,
                         "The geometry collection contains "
                         "non-line string geometries");
                return nullptr;
            }
        }
    }
    else if (wkbFlatten(poGeom->getGeometryType()) != wkbMultiLineString)
    {
        if (peErr != nullptr)
            *peErr = OGRERR_FAILURE;
        CPLError(CE_Failure, CPLE_NotSupported,
                 "The passed geometry is not an OGRGeometryCollection "
                 "(or OGRMultiLineString) "
                 "containing line string geometries");
        return nullptr;
    }

    bool bSuccess = true;
    OGRGeometryCollection *poLines = poGeom->toGeometryCollection();
    std::vector<OGRGeometry *> apoPolys;

    /* -------------------------------------------------------------------- */
    /*      Setup array of line markers indicating if they have been        */
    /*      added to a ring yet.                                            */
    /* -------------------------------------------------------------------- */
    const int nEdges = poLines->getNumGeometries();
    std::list<OGRLineString *> oListEdges;
    for (int i = 0; i < nEdges; i++)
    {
        OGRLineString *poLine = poLines->getGeometryRef(i)->toLineString();
        if (poLine->getNumPoints() >= 2)
        {
            oListEdges.push_back(poLine);
        }
    }

    /* ==================================================================== */
    /*      Loop generating rings.                                          */
    /* ==================================================================== */
    while (!oListEdges.empty())
    {
        /* --------------------------------------------------------------------
         */
        /*      Find the first unconsumed edge. */
        /* --------------------------------------------------------------------
         */
        OGRLineString *poLine = oListEdges.front();
        oListEdges.erase(oListEdges.begin());

        /* --------------------------------------------------------------------
         */
        /*      Start a new ring, copying in the current line directly */
        /* --------------------------------------------------------------------
         */
        OGRLinearRing *poRing = new OGRLinearRing();

        AddEdgeToRing(poRing, poLine, FALSE, 0);

        /* ====================================================================
         */
        /*      Loop adding edges to this ring until we make a whole pass */
        /*      within finding anything to add. */
        /* ====================================================================
         */
        bool bWorkDone = true;
        double dfBestDist = dfTolerance;

        while (!CheckPoints(poRing, 0, poRing, poRing->getNumPoints() - 1,
                            nullptr) &&
               !oListEdges.empty() && bWorkDone)
        {
            bool bReverse = false;

            bWorkDone = false;
            dfBestDist = dfTolerance;

            // We consider linking the end to the beginning.  If this is
            // closer than any other option we will just close the loop.

            // CheckPoints(poRing, 0, poRing, poRing->getNumPoints()-1,
            //             &dfBestDist);

            // Find unused edge with end point closest to our loose end.
            OGRLineString *poBestEdge = nullptr;
            std::list<OGRLineString *>::iterator oBestIter;
            for (auto oIter = oListEdges.begin(); oIter != oListEdges.end();
                 ++oIter)
            {
                poLine = *oIter;

                if (CheckPoints(poLine, 0, poRing, poRing->getNumPoints() - 1,
                                &dfBestDist))
                {
                    poBestEdge = poLine;
                    oBestIter = oIter;
                    bReverse = false;
                }
                if (CheckPoints(poLine, poLine->getNumPoints() - 1, poRing,
                                poRing->getNumPoints() - 1, &dfBestDist))
                {
                    poBestEdge = poLine;
                    oBestIter = oIter;
                    bReverse = true;
                }

                // If we found an exact match, jump now.
                if (dfBestDist == 0.0 && poBestEdge != nullptr)
                    break;
            }

            // We found one within tolerance - add it.
            if (poBestEdge)
            {
                AddEdgeToRing(poRing, poBestEdge, bReverse, dfTolerance);

                oListEdges.erase(oBestIter);
                bWorkDone = true;
            }
        }

        /* --------------------------------------------------------------------
         */
        /*      Did we fail to complete the ring? */
        /* --------------------------------------------------------------------
         */
        dfBestDist = dfTolerance;

        if (!CheckPoints(poRing, 0, poRing, poRing->getNumPoints() - 1,
                         &dfBestDist))
        {
            CPLDebug("OGR",
                     "Failed to close ring %d.\n"
                     "End Points are: (%.8f,%.7f) and (%.7f,%.7f)",
                     static_cast<int>(apoPolys.size()), poRing->getX(0),
                     poRing->getY(0), poRing->getX(poRing->getNumPoints() - 1),
                     poRing->getY(poRing->getNumPoints() - 1));

            bSuccess = false;
        }

        /* --------------------------------------------------------------------
         */
        /*      Do we need to auto-close this ring? */
        /* --------------------------------------------------------------------
         */
        dfBestDist = dfTolerance;

        if (bAutoClose)
        {
            if (!CheckPoints(poRing, 0, poRing, poRing->getNumPoints() - 1,
                             &dfBestDist))
            {
                poRing->addPoint(poRing->getX(0), poRing->getY(0),
                                 poRing->getZ(0));
            }
            else if (!CheckPoints(poRing, 0, poRing, poRing->getNumPoints() - 1,
                                  nullptr))
            {
                // The endpoints are very close but do not exactly match.
                // Alter the last one so it is equal to the first, to prevent
                // invalid self-intersecting rings.
                poRing->setPoint(poRing->getNumPoints() - 1, poRing->getX(0),
                                 poRing->getY(0), poRing->getZ(0));
            }
        }

        auto poPoly = new OGRPolygon();
        poPoly->addRingDirectly(poRing);
        apoPolys.push_back(poPoly);
    }  // Next ring.

    if (peErr != nullptr)
    {
        *peErr = bSuccess ? OGRERR_NONE : OGRERR_FAILURE;
    }

    return OGRGeometry::ToHandle(OGRGeometryFactory::organizePolygons(
        apoPolys.data(), static_cast<int>(apoPolys.size()), nullptr, nullptr));
}
