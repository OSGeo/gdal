/**********************************************************************
 *
 * Name:     mitab_geometry.cpp
 * Project:  MapInfo TAB Read/Write library
 * Language: C++
 * Purpose:  Geometry manipulation functions.
 * Author:   Daniel Morissette, dmorissette@dmsolutions.ca
 *           Based on functions from mapprimitive.c/mapsearch.c in the source
 *           of UMN MapServer by Stephen Lime (http://mapserver.gis.umn.edu/)
 **********************************************************************
 * Copyright (c) 1999-2001, Daniel Morissette
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
 **********************************************************************/

#include "cpl_port.h"
#include "mitab_geometry.h"

#include <cmath>
#include <cstdlib>
#include <algorithm>
#include <utility>

#include "ogr_core.h"

CPL_CVSID("$Id$")

#define OGR_NUM_RINGS(poly)   (poly->getNumInteriorRings()+1)
#define OGR_GET_RING(poly, i) (i==0?poly->getExteriorRing():poly->getInteriorRing(i-1))

/**********************************************************************
 *                   OGRPointInRing()
 *
 * Returns TRUE is point is inside ring, FALSE otherwise
 *
 * Adapted version of msPointInPolygon() from MapServer's mapsearch.c
 **********************************************************************/
GBool OGRPointInRing(OGRPoint *poPoint, OGRLineString *poRing)
{
    int i, j, numpoints;
    GBool status = FALSE;
    double x, y;

    numpoints = poRing->getNumPoints();
    x = poPoint->getX();
    y = poPoint->getY();

    for (i = 0, j = numpoints-1; i < numpoints; j = i++)
    {
        if ((((poRing->getY(i)<=y) && (y<poRing->getY(j))) ||
             ((poRing->getY(j)<=y) && (y<poRing->getY(i)))) &&
            (x < (poRing->getX(j) - poRing->getX(i)) * (y - poRing->getY(i)) /
                 (poRing->getY(j) - poRing->getY(i)) + poRing->getX(i)))
            status = !status;
    }

    return status;
}

/**********************************************************************
 *                   OGRIntersectPointPolygon()
 *
 * Instead of using ring orientation we count the number of parts the
 * point falls in. If odd the point is in the polygon, if 0 or even
 * then the point is in a hole or completely outside.
 *
 * Returns TRUE is point is inside polygon, FALSE otherwise
 *
 * Adapted version of msIntersectPointPolygon() from MapServer's mapsearch.c
 **********************************************************************/
GBool OGRIntersectPointPolygon(OGRPoint *poPoint, OGRPolygon *poPoly)
{
    GBool status = FALSE;

    for( int i = 0; i<OGR_NUM_RINGS(poPoly); i++ )
    {
        if (OGRPointInRing(poPoint, OGR_GET_RING(poPoly, i)))
        {
            /* ok, the point is in a line */
            status = !status;
        }
    }

    return status;
}

/**********************************************************************
 *                   OGRPolygonLabelPoint()
 *
 * Generate a label point on the surface of a polygon.
 *
 * The function is based on a scanline conversion routine used for polygon
 * fills.  Instead of processing each line the as with drawing, the
 * polygon is sampled. The center of the longest sample is chosen for the
 * label point. The label point is guaranteed to be in the polygon even if
 * it has holes assuming the polygon is properly formed.
 *
 * Returns OGRERR_NONE if it succeeds or OGRERR_FAILURE otherwise.
 *
 * Adapted version of msPolygonLabelPoint() from MapServer's mapprimitive.c
 **********************************************************************/

typedef enum { CLIP_LEFT, CLIP_MIDDLE, CLIP_RIGHT } CLIP_STATE;
static CLIP_STATE EDGE_CHECK( double x0, double x, double x1 )
{
    if( x < std::min(x0, x1) )
        return CLIP_LEFT;
    if( x > std::max(x0, x1) )
        return CLIP_RIGHT;
    return CLIP_MIDDLE;
}

static const int NUM_SCANLINES = 5;

int OGRPolygonLabelPoint(OGRPolygon *poPoly, OGRPoint *poLabelPoint)
{
    if (poPoly == NULL)
        return OGRERR_FAILURE;

    OGREnvelope   oEnv;
    poPoly->getEnvelope(&oEnv);

    poLabelPoint->setX((oEnv.MaxX + oEnv.MinX)/2.0);
    poLabelPoint->setY((oEnv.MaxY + oEnv.MinY)/2.0);

    // if( get_centroid(p, lp, &miny, &maxy) == -1 ) return -1;

    if(OGRIntersectPointPolygon(poLabelPoint, poPoly) == TRUE) /* cool, done */
        return OGRERR_NONE;

    /* do it the hard way - scanline */

    double skip = (oEnv.MaxY - oEnv.MinY)/NUM_SCANLINES;

    int n = 0;
    for( int j = 0; j < OGR_NUM_RINGS(poPoly); j++ )
    {
        /* count total number of points */
        n += OGR_GET_RING(poPoly, j)->getNumPoints();
    }
    if( n == 0 )
        return OGRERR_FAILURE;

    double *xintersect = (double *)calloc(n, sizeof(double));
    if( xintersect == NULL )
        return OGRERR_FAILURE;

    double max_len = 0.0;

    for( int k = 1; k <= NUM_SCANLINES; k++ )
    {
        /* sample the shape in the y direction */

        double y = oEnv.MaxY - k*skip;

        // Need to find a y that won't intersect any vertices exactly.
        // First initializing lo_y, hi_y to be any 2 pnts on either side of
        // lp->y.
        double hi_y = y - 1;
        double lo_y = y + 1;
        for( int j = 0; j < OGR_NUM_RINGS(poPoly); j++ )
        {
            OGRLinearRing *poRing = OGR_GET_RING(poPoly,j);

            if((lo_y < y) && (hi_y >= y))
                break; /* already initialized */
            for( int i = 0; i < poRing->getNumPoints(); i++ )
            {
                if((lo_y < y) && (hi_y >= y))
                    break; /* already initialized */
                if(poRing->getY(i) < y)
                    lo_y = poRing->getY(i);
                if(poRing->getY(i) >= y)
                    hi_y = poRing->getY(i);
            }
        }

        for( int j = 0; j<OGR_NUM_RINGS(poPoly); j++ )
        {
            OGRLinearRing *poRing = OGR_GET_RING(poPoly,j);

            for( int i = 0; i < poRing->getNumPoints(); i++ )
            {
                if((poRing->getY(i) < y) &&
                   ((y - poRing->getY(i)) < (y - lo_y)))
                    lo_y = poRing->getY(i);
                if((poRing->getY(i) >= y) &&
                   ((poRing->getY(i) - y) < (hi_y - y)))
                    hi_y = poRing->getY(i);
            }
        }

        if( lo_y == hi_y )
        {
            free(xintersect);
            return OGRERR_FAILURE;
        }

        y = (hi_y + lo_y) / 2.0;

        OGRRawPoint point1;
        OGRRawPoint point2;

        int nfound = 0;
        for( int j = 0; j < OGR_NUM_RINGS(poPoly); j++ )   // For each line.
        {
            OGRLinearRing *poRing = OGR_GET_RING(poPoly,j);
            point1.x = poRing->getX(poRing->getNumPoints()-1);
            point1.y = poRing->getY(poRing->getNumPoints()-1);

            for( int i = 0; i < poRing->getNumPoints(); i++ )
            {
                point2.x = poRing->getX(i);
                point2.y = poRing->getY(i);

                if(EDGE_CHECK(point1.y, y, point2.y) == CLIP_MIDDLE)
                {
                    if(point1.y == point2.y)
                        continue;  // Ignore horizontal edges.

                    const double slope =
                        (point2.x - point1.x) / (point2.y - point1.y);

                    double x = point1.x + (y - point1.y)*slope;
                    xintersect[nfound++] = x;
                } /* End of checking this edge */

                point1 = point2;  /* Go on to next edge */
            }
        } /* Finished the scanline */

        /* First, sort the intersections */
        bool wrong_order = false;
        do
        {
            wrong_order = false;
            for( int i = 0; i < nfound-1; i++ )
            {
                if(xintersect[i] > xintersect[i+1])
                {
                    wrong_order = true;
                    std::swap(xintersect[i], xintersect[i+1]);
                }
            }
        } while( wrong_order );

        // Great, now find longest span.
        // point1.y = y;
        // point2.y = y;
        for( int i = 0; i < nfound; i += 2 )
        {
            point1.x = xintersect[i];
            point2.x = xintersect[i+1];
            /* len = length(point1, point2); */
            const double len = std::abs((point2.x - point1.x));
            if(len > max_len)
            {
                max_len = len;
                poLabelPoint->setX( (point1.x + point2.x)/2 );
                poLabelPoint->setY( y );
            }
        }
    }

    free(xintersect);

    /* __TODO__ Bug 673
     * There seem to be some polygons for which the label is returned
     * completely outside of the polygon's MBR and this messes the
     * file bounds, etc.
     * Until we find the source of the problem, we'll at least validate
     * the label point to make sure that it overlaps the polygon MBR.
     */
    if( poLabelPoint->getX() < oEnv.MinX
        || poLabelPoint->getY() < oEnv.MinY
        || poLabelPoint->getX() > oEnv.MaxX
        || poLabelPoint->getY() > oEnv.MaxY )
    {
        // Reset label coordinates to center of MBR, just in case
        poLabelPoint->setX((oEnv.MaxX + oEnv.MinX)/2.0);
        poLabelPoint->setY((oEnv.MaxY + oEnv.MinY)/2.0);

        // And return an error
        return OGRERR_FAILURE;
    }

    if(max_len > 0)
        return OGRERR_NONE;
    else
        return OGRERR_FAILURE;
}

#ifdef unused
/**********************************************************************
 *                   OGRGetCentroid()
 *
 * Calculate polygon gravity center.
 *
 * Returns OGRERR_NONE if it succeeds or OGRERR_FAILURE otherwise.
 *
 * Adapted version of get_centroid() from MapServer's mapprimitive.c
 **********************************************************************/

int OGRGetCentroid(OGRPolygon *poPoly, OGRPoint *poCentroid)
{
    double cent_weight_x = 0.0;
    double cent_weight_y = 0.0;
    double len = 0.0;
    double total_len = 0.0;

    for( int i = 0; i<OGR_NUM_RINGS(poPoly); i++ )
    {
        OGRLinearRing *poRing = OGR_GET_RING(poPoly, i);

        double x2 = poRing->getX(0);
        double y2 = poRing->getY(0);

        for( int j = 1; j<poRing->getNumPoints(); j++ )
        {
            double x1 = x2;
            double y1 = y2;
            x2 = poRing->getX(j);
            y2 = poRing->getY(j);

            len = sqrt( pow((x2-x1),2) + pow((y2-y1),2) );
            cent_weight_x += len * ((x1 + x2)/2.0);
            cent_weight_y += len * ((y1 + y2)/2.0);
            total_len += len;
        }
    }

    if(total_len == 0)
        return OGRERR_FAILURE;

    poCentroid->setX( cent_weight_x / total_len );
    poCentroid->setY( cent_weight_y / total_len );

    return OGRERR_NONE;
}
#endif

/**********************************************************************
 *                   OGRPolylineCenterPoint()
 *
 * Return the center point of a polyline.
 *
 * In MapInfo, for a simple or multiple polyline (pline), the center point
 * in the object definition is supposed to be either the center point of
 * the pline or the first section of a multiple pline (if an odd number of
 * points in the pline or first section), or the midway point between the
 * two central points (if an even number of points involved).
 *
 * Returns OGRERR_NONE if it succeeds or OGRERR_FAILURE otherwise.
 **********************************************************************/

int OGRPolylineCenterPoint(OGRLineString *poLine, OGRPoint *poLabelPoint)
{
    if (poLine == NULL || poLine->getNumPoints() < 2)
        return OGRERR_FAILURE;

    if (poLine->getNumPoints() % 2 == 0)
    {
        // Return the midway between the 2 center points
        int i = poLine->getNumPoints()/2;
        poLabelPoint->setX( (poLine->getX(i-1) + poLine->getX(i))/2.0 );
        poLabelPoint->setY( (poLine->getY(i-1) + poLine->getY(i))/2.0 );
    }
    else
    {
        // Return the center point
        poLine->getPoint(poLine->getNumPoints()/2, poLabelPoint);
    }

    return OGRERR_NONE;
}

/**********************************************************************
 *                   OGRPolylineLabelPoint()
 *
 * Generate a label point on a polyline: The center of the longest segment.
 *
 * Returns OGRERR_NONE if it succeeds or OGRERR_FAILURE otherwise.
 **********************************************************************/

int OGRPolylineLabelPoint(OGRLineString *poLine, OGRPoint *poLabelPoint)
{
    if (poLine == NULL || poLine->getNumPoints() < 2)
        return OGRERR_FAILURE;

    double max_segment_length = -1.0;

    double x2 = poLine->getX(0);
    double y2 = poLine->getY(0);

    for( int i = 1; i < poLine->getNumPoints(); i++ )
    {
        double x1 = x2;
        double y1 = y2;
        x2 = poLine->getX(i);
        y2 = poLine->getY(i);

        double segment_length = pow((x2-x1),2) + pow((y2-y1),2);
        if (segment_length > max_segment_length)
        {
            max_segment_length = segment_length;
            poLabelPoint->setX( (x1 + x2)/2.0 );
            poLabelPoint->setY( (y1 + y2)/2.0 );
        }
    }

    return OGRERR_NONE;
}
