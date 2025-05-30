/******************************************************************************
 *
 * Project:  DWG Translator
 * Purpose:  Implements translation support for HATCH elements as part
 *           of the OGRDWGLayer class.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2011, Frank Warmerdam <warmerdam@pobox.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include "ogr_dwg.h"
#include "cpl_conv.h"
#include "ogr_api.h"

#include "ogrdxf_polyline_smooth.h"

static OGRErr DWGCollectBoundaryLoop(OdDbHatchPtr poHatch, int iLoop,
                                     OGRGeometryCollection *poGC);

/************************************************************************/
/*                           TranslateHATCH()                           */
/*                                                                      */
/*      We mostly just try to convert hatch objects as polygons or      */
/*      multipolygons representing the hatched area.  It is hard to     */
/*      preserve the actual details of the hatching.                    */
/************************************************************************/

OGRFeature *OGRDWGLayer::TranslateHATCH(OdDbEntityPtr poEntity)

{
    OGRFeature *poFeature = new OGRFeature(poFeatureDefn);
    OdDbHatchPtr poHatch = OdDbHatch::cast(poEntity);
    OGRGeometryCollection oGC;

    TranslateGenericProperties(poFeature, poEntity);

    poFeature->SetField("Text", (const char *)poHatch->patternName());

    /* -------------------------------------------------------------------- */
    /*      Collect the loops.                                              */
    /* -------------------------------------------------------------------- */
    for (int i = 0; i < poHatch->numLoops(); i++)
    {
        DWGCollectBoundaryLoop(poHatch, i, &oGC);
    }

    /* -------------------------------------------------------------------- */
    /*      Try to turn the set of lines into something useful.             */
    /* -------------------------------------------------------------------- */
    OGRErr eErr;

    OGRGeometryH hFinalGeom = OGRBuildPolygonFromEdges((OGRGeometryH)&oGC, TRUE,
                                                       TRUE, 0.0000001, &eErr);

    poFeature->SetGeometryDirectly((OGRGeometry *)hFinalGeom);

    /* -------------------------------------------------------------------- */
    /*      Work out the color for this feature.  For now we just assume    */
    /*      solid fill.  We cannot trivially translate the various sorts    */
    /*      of hatching.                                                    */
    /* -------------------------------------------------------------------- */
    CPLString osLayer = poFeature->GetFieldAsString("Layer");

    int nColor = 256;

    if (oStyleProperties.count("Color") > 0)
        nColor = atoi(oStyleProperties["Color"]);

    // Use layer color?
    if (nColor < 1 || nColor > 255)
    {
        const char *pszValue = poDS->LookupLayerProperty(osLayer, "Color");
        if (pszValue != nullptr)
            nColor = atoi(pszValue);
    }

    /* -------------------------------------------------------------------- */
    /*      Setup the style string.                                         */
    /* -------------------------------------------------------------------- */
    if (nColor >= 1 && nColor <= 255)
    {
        CPLString osStyle;
        const unsigned char *pabyDWGColors = ACGetColorTable();

        osStyle.Printf("BRUSH(fc:#%02x%02x%02x)", pabyDWGColors[nColor * 3 + 0],
                       pabyDWGColors[nColor * 3 + 1],
                       pabyDWGColors[nColor * 3 + 2]);

        poFeature->SetStyleString(osStyle);
    }

    return poFeature;
}

/************************************************************************/
/*                        CollectBoundaryLoop()                         */
/************************************************************************/

static OGRErr DWGCollectBoundaryLoop(OdDbHatchPtr poHatch, int iLoop,
                                     OGRGeometryCollection *poGC)

{
    int i;

    /* -------------------------------------------------------------------- */
    /*      Handle simple polyline loops.                                   */
    /* -------------------------------------------------------------------- */
    if (poHatch->loopTypeAt(iLoop) & OdDbHatch::kPolyline)
    {
        DXFSmoothPolyline oSmoothPolyline;
        OdGePoint2dArray vertices;
        OdGeDoubleArray bulges;

        poHatch->getLoopAt(iLoop, vertices, bulges);

        for (i = 0; i < (int)vertices.size(); i++)
        {
            if (i >= (int)bulges.size())
                oSmoothPolyline.AddPoint(vertices[i].x, vertices[i].y, 0.0,
                                         0.0);
            else
                oSmoothPolyline.AddPoint(vertices[i].x, vertices[i].y, 0.0,
                                         bulges[i]);
        }

        oSmoothPolyline.Close();

        OGRLineString *poLS = oSmoothPolyline.Tessellate(false)->toLineString();
        poGC->addGeometryDirectly(poLS);

        return OGRERR_NONE;
    }

    /* -------------------------------------------------------------------- */
    /*      Handle an edges array.                                          */
    /* -------------------------------------------------------------------- */
    EdgeArray oEdges;
    poHatch->getLoopAt(iLoop, oEdges);

    for (OdGeCurve2d *poEdge : oEdges)
    {
        if (poEdge->type() == OdGe::kLineSeg2d)
        {
            OGRLineString *poLS = new OGRLineString();
            OdGePoint2d oStart = poEdge->evalPoint(0.0);
            OdGePoint2d oEnd = poEdge->evalPoint(1.0);

            poLS->addPoint(oStart.x, oStart.y);
            poLS->addPoint(oEnd.x, oEnd.y);
            poGC->addGeometryDirectly(poLS);
        }
        else if (poEdge->type() == OdGe::kCircArc2d)
        {
            OdGeCircArc2d *poCircArc = static_cast<OdGeCircArc2d *>(poEdge);
            OdGePoint2d oCenter = poCircArc->center();
            double dfStartAngle = poCircArc->startAng() * 180 / M_PI;
            double dfEndAngle = poCircArc->endAng() * 180 / M_PI;

            if (!poCircArc->isClockWise())
            {
                dfStartAngle *= -1;
                dfEndAngle *= -1;
            }
            else if (dfStartAngle > dfEndAngle)
            {
                dfEndAngle += 360.0;
            }

            OGRLineString *poLS =
                OGRGeometryFactory::approximateArcAngles(
                    oCenter.x, oCenter.y, 0.0, poCircArc->radius(),
                    poCircArc->radius(), 0.0, dfStartAngle, dfEndAngle, 0.0)
                    ->toLineString();

            poGC->addGeometryDirectly(poLS);
        }
        else if (poEdge->type() == OdGe::kEllipArc2d)
        {
            OdGeEllipArc2d *poArc = static_cast<OdGeEllipArc2d *>(poEdge);
            OdGePoint2d oCenter = poArc->center();
            double dfRatio = poArc->minorRadius() / poArc->majorRadius();
            OdGeVector2d oMajorAxis = poArc->majorAxis();
            double dfRotation;
            double dfStartAng, dfEndAng;

            dfRotation = -1 * atan2(oMajorAxis.y, oMajorAxis.x) * 180 / M_PI;

            dfStartAng = poArc->startAng() * 180 / M_PI;
            dfEndAng = poArc->endAng() * 180 / M_PI;

            if (!poArc->isClockWise())
            {
                dfStartAng *= -1;
                dfEndAng *= -1;
            }
            else if (dfStartAng > dfEndAng)
            {
                dfEndAng += 360.0;
            }

            OGRLineString *poLS =
                OGRGeometryFactory::approximateArcAngles(
                    oCenter.x, oCenter.y, 0.0, poArc->majorRadius(),
                    poArc->minorRadius(), dfRotation,
                    OGRDWGLayer::AngleCorrect(dfStartAng, dfRatio),
                    OGRDWGLayer::AngleCorrect(dfEndAng, dfRatio), 0.0)
                    ->toLineString();
            poGC->addGeometryDirectly(poLS);
        }
        else
            CPLDebug("DWG", "Unsupported edge type (%d) in hatch loop.",
                     (int)poEdge->type());

        // case OdGe::kNurbCurve2d : dumpNurbCurveEdge(indent + 1, pEdge);
    }

    return OGRERR_NONE;
}
