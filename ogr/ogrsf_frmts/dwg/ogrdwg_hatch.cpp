/******************************************************************************
 * $Id: ogrdxf_dimension.cpp 19643 2010-05-08 21:56:18Z rouault $
 *
 * Project:  DWG Translator
 * Purpose:  Implements translation support for HATCH elements as part
 *           of the OGRDWGLayer class.  
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2011, Frank Warmerdam <warmerdam@pobox.com>
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

#include "ogr_dwg.h"
#include "cpl_conv.h"
#include "ogr_api.h"

#include "DbHatch.h"

#include "Ge/GePoint2dArray.h"
#include "Ge/GeCurve2d.h"
#include "Ge/GeCircArc2d.h"

CPL_CVSID("$Id: ogrdxf_dimension.cpp 19643 2010-05-08 21:56:18Z rouault $");

#ifndef PI
#define PI  3.14159265358979323846
#endif 

static OGRErr DWGCollectBoundaryLoop( OdDbHatchPtr poHatch, int iLoop,
                                      OGRGeometryCollection *poGC );

/************************************************************************/
/*                           TranslateHATCH()                           */
/*                                                                      */
/*      We mostly just try to convert hatch objects as polygons or      */
/*      multipolygons representing the hatched area.  It is hard to     */
/*      preserve the actual details of the hatching.                    */
/************************************************************************/

OGRFeature *OGRDWGLayer::TranslateHATCH( OdDbEntityPtr poEntity )

{
    OGRFeature *poFeature = new OGRFeature( poFeatureDefn );
    OdDbHatchPtr poHatch = OdDbHatch::cast( poEntity );
    OGRGeometryCollection oGC;

    TranslateGenericProperties( poFeature, poEntity );

    poFeature->SetField( "Text", 
                         (const char *) poHatch->patternName() );

/* -------------------------------------------------------------------- */
/*      Collect the loops.                                              */
/* -------------------------------------------------------------------- */
    for( int i = 0; i < poHatch->numLoops(); i++ )
    {
        DWGCollectBoundaryLoop( poHatch, i, &oGC );
    }

/* -------------------------------------------------------------------- */
/*      Try to turn the set of lines into something useful.             */
/* -------------------------------------------------------------------- */
    OGRErr eErr;

    OGRGeometryH hFinalGeom = 
        OGRBuildPolygonFromEdges( (OGRGeometryH) &oGC,
                                  TRUE, TRUE, 0.0000001, &eErr );

    poFeature->SetGeometryDirectly( (OGRGeometry *) hFinalGeom );

/* -------------------------------------------------------------------- */
/*      We ought to try and make some useful translation of the fill    */
/*      style but I'll leave that for another time.                     */
/* -------------------------------------------------------------------- */

    return poFeature;
}

/************************************************************************/
/*                        CollectBoundaryLoop()                         */
/************************************************************************/

static OGRErr DWGCollectBoundaryLoop( OdDbHatchPtr poHatch, int iLoop,
                                      OGRGeometryCollection *poGC )

{
    int i;

/* -------------------------------------------------------------------- */
/*      Handle simple polyline loops.                                   */
/* -------------------------------------------------------------------- */
    if( poHatch->loopTypeAt( iLoop ) & OdDbHatch::kPolyline )
    {
        OGRLineString *poLS = new OGRLineString();
        OdGePoint2dArray vertices;
        OdGeDoubleArray bulges;
        poHatch->getLoopAt (iLoop, vertices, bulges);

        for (i = 0; i < (int) vertices.size(); i++)
            poLS->addPoint( vertices[i].x, vertices[i].y );

        // make sure loop is closed.
        if( vertices[0].x != vertices[vertices.size()-1].x
            || vertices[0].y  != vertices[vertices.size()-1].y )
        {
            poLS->addPoint( vertices[0].x, vertices[0].y );
        }

        poGC->addGeometryDirectly( poLS );

        return OGRERR_NONE;
    }

/* -------------------------------------------------------------------- */
/*      Handle an edges array.                                          */
/* -------------------------------------------------------------------- */
    EdgeArray oEdges;
    poHatch->getLoopAt( iLoop, oEdges );

    for( i = 0; i < (int) oEdges.size(); i++ )
    {
        OdGeCurve2d* poEdge = oEdges[i];

        if( poEdge->type() == OdGe::kLineSeg2d )
        {
            OGRLineString *poLS = new OGRLineString();
            OdGePoint2d oStart = poEdge->evalPoint(0.0);
            OdGePoint2d oEnd = poEdge->evalPoint(1.0);

            poLS->addPoint( oStart.x, oStart.y );
            poLS->addPoint( oEnd.x, oEnd.y );
            poGC->addGeometryDirectly( poLS );
        }
        else if( poEdge->type() == OdGe::kCircArc2d )
        {
            OdGeCircArc2d* poCircArc = (OdGeCircArc2d*) poEdge;
            OdGePoint2d oCenter = poCircArc->center();
            
            poGC->addGeometryDirectly(
                OGRGeometryFactory::approximateArcAngles( 
                    oCenter.x, oCenter.y, 0.0,
                    poCircArc->radius(), poCircArc->radius(), 0.0,
                    -1 * poCircArc->endAng() * 180 / PI,
                    -1 * poCircArc->startAng() * 180 / PI,
                    0.0 ) );
            // do we need pCircArc->isClockWise()?
        }
        else
            CPLDebug( "DWG", "Unsupported edge in hatch loop." );

        //case OdGe::kEllipArc2d  : dumpEllipticalArcEdge(indent + 1, pEdge);
        //case OdGe::kNurbCurve2d : dumpNurbCurveEdge(indent + 1, pEdge);    
    }

    return OGRERR_NONE;
}

