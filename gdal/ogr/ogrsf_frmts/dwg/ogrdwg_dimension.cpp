/******************************************************************************
 *
 * Project:  DWG Translator
 * Purpose:  Implements translation support for DIMENSION elements as a part
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

CPL_CVSID("$Id$")

/************************************************************************/
/*                         TranslateDIMENSION()                         */
/************************************************************************/

OGRFeature *OGRDWGLayer::TranslateDIMENSION( OdDbEntityPtr poEntity )

{
    OdDbDimensionPtr poDim = OdDbDimension::cast( poEntity );
    OGRFeature *poFeature = new OGRFeature( poFeatureDefn );

    double dfHeight = CPLAtof(poDS->GetVariable("$DIMTXT", "2.5"));
    OdGePoint3d oTextPos, oTarget1, oTarget2, oArrow1;
    CPLString osText;

    TranslateGenericProperties( poFeature, poEntity );

/* -------------------------------------------------------------------- */
/*      Generic Dimension stuff.                                        */
/* -------------------------------------------------------------------- */
    osText = (const char *) poDim->dimensionText();

    oTextPos = poDim->textPosition();

/* -------------------------------------------------------------------- */
/*      Specific based on the subtype.                                  */
/* -------------------------------------------------------------------- */
    OdRxClass *poClass = poEntity->isA();
    const OdString osName = poClass->name();
    const char *pszEntityClassName = (const char *) osName;

    if( EQUAL(pszEntityClassName,"AcDbRotatedDimension") )
    {
        OdDbRotatedDimensionPtr poRDim = OdDbDimension::cast( poEntity );

        oTarget2 = poRDim->xLine1Point();
        oTarget1 = poRDim->xLine2Point();
        oArrow1 = poRDim->dimLinePoint();
    }

    else if( EQUAL(pszEntityClassName,"AcDbAlignedDimension") )
    {
        OdDbAlignedDimensionPtr poADim = OdDbDimension::cast( poEntity );

        oTarget2 = poADim->xLine1Point();
        oTarget1 = poADim->xLine2Point();
        oArrow1 = poADim->dimLinePoint();
    }

/*************************************************************************

   DIMENSION geometry layout

                  (11,21)(text center point)
        |          DimText                  |
(10,20) X<--------------------------------->X (Arrow2 - computed)
(Arrow1)|                                   |
        |                                   |
        |                                   X (13,23) (Target2)
        |
        X (14,24) (Target1)

Given:
  Locations Arrow1, Target1, and Target2 we need to compute Arrow2.

Steps:
 1) Compute direction vector from Target1 to Arrow1 (Vec1).
 2) Compute direction vector for arrow as perpendicular to Vec1 (call Vec2).
 3) Compute Arrow2 location as intersection between line defined by
    Vec2 and Arrow1 and line defined by Target2 and direction Vec1 (call Arrow2)

Then we can draw lines for the various components.

Note that Vec1 and Vec2 may be horizontal, vertical or on an angle but
the approach is as above in all these cases.

*************************************************************************/

/* -------------------------------------------------------------------- */
/*      Step 1, compute direction vector between Target1 and Arrow1.    */
/* -------------------------------------------------------------------- */
    double dfVec1X, dfVec1Y;

    dfVec1X = (oArrow1.x - oTarget1.x);
    dfVec1Y = (oArrow1.y - oTarget1.y);

/* -------------------------------------------------------------------- */
/*      Step 2, compute the direction vector from Arrow1 to Arrow2      */
/*      as a perpendicular to Vec1.                                     */
/* -------------------------------------------------------------------- */
    double dfVec2X, dfVec2Y;

    dfVec2X = dfVec1Y;
    dfVec2Y = -dfVec1X;

/* -------------------------------------------------------------------- */
/*      Step 3, compute intersection of line from target2 along         */
/*      direction vector 1, with the line through Arrow1 and            */
/*      direction vector 2.                                             */
/* -------------------------------------------------------------------- */
    double dfL1M, dfL1B, dfL2M, dfL2B;
    double dfArrowX2, dfArrowY2;

    // special case if vec1 is vertical.
    if( dfVec1X == 0.0 )
    {
        dfArrowX2 = oTarget2.x;
        dfArrowY2 = oArrow1.y;
    }

    // special case if vec2 is horizontal.
    else if( dfVec1Y == 0.0 )
    {
        dfArrowX2 = oArrow1.x;
        dfArrowY2 = oTarget2.y;
    }

    else // General case for diagonal vectors.
    {
        // first convert vec1 + target2 into y = mx + b format: call this L1

        dfL1M = dfVec1Y / dfVec1X;
        dfL1B = oTarget2.y - dfL1M * oTarget2.x;

        // convert vec2 + Arrow1 into y = mx + b format, call this L2

        dfL2M = dfVec2Y / dfVec2X;
        dfL2B = oArrow1.y - dfL2M * oArrow1.x;

        // Compute intersection x = (b2-b1) / (m1-m2)

        dfArrowX2 = (dfL2B - dfL1B) / (dfL1M-dfL2M);
        dfArrowY2 = dfL2M * dfArrowX2 + dfL2B;
    }

/* -------------------------------------------------------------------- */
/*      Compute the text angle.                                         */
/* -------------------------------------------------------------------- */
    double dfAngle = atan2(dfVec2Y,dfVec2X) * 180.0 / M_PI;

/* -------------------------------------------------------------------- */
/*      Rescale the direction vectors so we can use them in             */
/*      constructing arrowheads.  We want them to be about 3% of the    */
/*      length of line on which the arrows will be drawn.               */
/* -------------------------------------------------------------------- */
#define VECTOR_LEN(x,y) sqrt( (x)*(x) + (y)*(y) )
#define POINT_DIST(x1,y1,x2,y2)  VECTOR_LEN((x2-x1),(y2-y1))

    double dfBaselineLength = POINT_DIST(oArrow1.x,oArrow1.y,
                                         dfArrowX2,dfArrowY2);
    double dfTargetLength = dfBaselineLength * 0.03;
    double dfScaleFactor;

    // recompute vector 2 to ensure the direction is regular
    dfVec2X = (dfArrowX2 - oArrow1.x);
    dfVec2Y = (dfArrowY2 - oArrow1.y);

    // vector 1
    dfScaleFactor = dfTargetLength / VECTOR_LEN(dfVec1X,dfVec1Y);
    dfVec1X *= dfScaleFactor;
    dfVec1Y *= dfScaleFactor;

    // vector 2
    dfScaleFactor = dfTargetLength / VECTOR_LEN(dfVec2X,dfVec2Y);
    dfVec2X *= dfScaleFactor;
    dfVec2Y *= dfScaleFactor;

/* -------------------------------------------------------------------- */
/*      Create geometries for the different components of the           */
/*      dimension object.                                               */
/* -------------------------------------------------------------------- */
    OGRMultiLineString *poMLS = new OGRMultiLineString();
    OGRLineString oLine;

    // main arrow line between Arrow1 and Arrow2
    oLine.setPoint( 0, oArrow1.x, oArrow1.y );
    oLine.setPoint( 1, dfArrowX2, dfArrowY2 );
    poMLS->addGeometry( &oLine );

    // dimension line from Target1 to Arrow1 with a small extension.
    oLine.setPoint( 0, oTarget1.x, oTarget1.y );
    oLine.setPoint( 1, oArrow1.x + dfVec1X, oArrow1.y + dfVec1Y );
    poMLS->addGeometry( &oLine );

    // dimension line from Target2 to Arrow2 with a small extension.
    oLine.setPoint( 0, oTarget2.x, oTarget2.y );
    oLine.setPoint( 1, dfArrowX2 + dfVec1X, dfArrowY2 + dfVec1Y );
    poMLS->addGeometry( &oLine );

    // add arrow1 arrow head.

    oLine.setPoint( 0, oArrow1.x, oArrow1.y );
    oLine.setPoint( 1,
                    oArrow1.x + dfVec2X*3 + dfVec1X,
                    oArrow1.y + dfVec2Y*3 + dfVec1Y );
    poMLS->addGeometry( &oLine );

    oLine.setPoint( 0, oArrow1.x, oArrow1.y );
    oLine.setPoint( 1,
                    oArrow1.x + dfVec2X*3 - dfVec1X,
                    oArrow1.y + dfVec2Y*3 - dfVec1Y );
    poMLS->addGeometry( &oLine );

    // add arrow2 arrow head.

    oLine.setPoint( 0, dfArrowX2, dfArrowY2 );
    oLine.setPoint( 1,
                    dfArrowX2 - dfVec2X*3 + dfVec1X,
                    dfArrowY2 - dfVec2Y*3 + dfVec1Y );
    poMLS->addGeometry( &oLine );

    oLine.setPoint( 0, dfArrowX2, dfArrowY2 );
    oLine.setPoint( 1,
                    dfArrowX2 - dfVec2X*3 - dfVec1X,
                    dfArrowY2 - dfVec2Y*3 - dfVec1Y );
    poMLS->addGeometry( &oLine );

    poFeature->SetGeometryDirectly( poMLS );

    PrepareLineStyle( poFeature );

/* -------------------------------------------------------------------- */
/*      Is the layer disabled/hidden/frozen/off?                        */
/* -------------------------------------------------------------------- */
    CPLString osLayer = poFeature->GetFieldAsString("Layer");

    int bHidden =
        EQUAL(poDS->LookupLayerProperty( osLayer, "Hidden" ), "1");

/* -------------------------------------------------------------------- */
/*      Work out the color for this feature.                            */
/* -------------------------------------------------------------------- */
    int nColor = 256;

    if( oStyleProperties.count("Color") > 0 )
        nColor = atoi(oStyleProperties["Color"]);

    // Use layer color?
    if( nColor < 1 || nColor > 255 )
    {
        const char *pszValue = poDS->LookupLayerProperty( osLayer, "Color" );
        if( pszValue != nullptr )
            nColor = atoi(pszValue);
    }

    if( nColor < 1 || nColor > 255 )
        nColor = 8;

/* -------------------------------------------------------------------- */
/*      Prepare a new feature to serve as the dimension text label      */
/*      feature.  We will push it onto the layer as a pending           */
/*      feature for the next feature read.                              */
/* -------------------------------------------------------------------- */

    // a single space suppresses labeling.
    if( osText == " " )
        return poFeature;

    OGRFeature *poLabelFeature = poFeature->Clone();

    poLabelFeature->SetGeometryDirectly( new OGRPoint( oTextPos.x, oTextPos.y ) );

    // Do we need to compute the dimension value?
    if( osText.empty() )
    {
        FormatDimension( osText, POINT_DIST( oArrow1.x, oArrow1.y,
                                             dfArrowX2, dfArrowY2 ) );
    }

    CPLString osStyle;
    char szBuffer[64];
    char* pszComma = nullptr;

    osStyle.Printf("LABEL(f:\"Arial\",t:\"%s\",p:5",osText.c_str());

    if( dfAngle != 0.0 )
    {
        CPLsnprintf(szBuffer, sizeof(szBuffer), "%.3g", dfAngle);
        pszComma = strchr(szBuffer, ',');
        if (pszComma)
            *pszComma = '.';
        osStyle += CPLString().Printf(",a:%s", szBuffer);
    }

    if( dfHeight != 0.0 )
    {
        CPLsnprintf(szBuffer, sizeof(szBuffer), "%.3g", dfHeight);
        pszComma = strchr(szBuffer, ',');
        if (pszComma)
            *pszComma = '.';
        osStyle += CPLString().Printf(",s:%sg", szBuffer);
    }

    const unsigned char *pabyDWGColors = ACGetColorTable();

    snprintf( szBuffer, sizeof(szBuffer), ",c:#%02x%02x%02x",
              pabyDWGColors[nColor*3+0],
              pabyDWGColors[nColor*3+1],
              pabyDWGColors[nColor*3+2] );
    osStyle += szBuffer;

    if( bHidden )
        osStyle += "00";

    osStyle += ")";

    poLabelFeature->SetStyleString( osStyle );

    apoPendingFeatures.push( poLabelFeature );

    return poFeature;
}

/************************************************************************/
/*                          FormatDimension()                           */
/*                                                                      */
/*      Format a dimension number according to the current files        */
/*      formatting conventions.                                         */
/************************************************************************/

void OGRDWGLayer::FormatDimension( CPLString &osText, double dfValue )

{
    int nPrecision = atoi(poDS->GetVariable("$LUPREC","4"));
    char szFormat[32];
    char szBuffer[64];

    // we could do a significantly more precise formatting if we want
    // to spend the effort.  See QCAD's rs_dimlinear.cpp and related files
    // for example.

    snprintf(szFormat, sizeof(szFormat), "%%.%df", nPrecision );
    CPLsnprintf(szBuffer, sizeof(szBuffer), szFormat, dfValue);
    char* pszComma = strchr(szBuffer, ',');
    if (pszComma)
        *pszComma = '.';
    osText = szBuffer;
}
