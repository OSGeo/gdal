/******************************************************************************
 * $Id$
 *
 * Project:  DXF Translator
 * Purpose:  Implements translation support for DIMENSION elements as a part
 *           of the OGRDXFLayer class.  
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2009, Frank Warmerdam <warmerdam@pobox.com>
 * Copyright (c) 2010, Even Rouault <even dot rouault at mines-paris dot org>
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

#include "ogr_dxf.h"
#include "cpl_conv.h"

CPL_CVSID("$Id$");

#ifndef PI
#define PI  3.14159265358979323846
#endif

/************************************************************************/
/*                         TranslateDIMENSION()                         */
/************************************************************************/

OGRFeature *OGRDXFLayer::TranslateDIMENSION()

{
    char szLineBuf[257];
    int nCode /*, nDimType = 0 */;
    OGRFeature *poFeature = new OGRFeature( poFeatureDefn );
    double dfArrowX1 = 0.0, dfArrowY1 = 0.0 /*, dfArrowZ1 = 0.0 */;
    double dfTargetX1 = 0.0, dfTargetY1 = 0.0 /* , dfTargetZ1 = 0.0 */;
    double dfTargetX2 = 0.0, dfTargetY2 = 0.0 /* , dfTargetZ2 = 0.0 */;
    double dfTextX = 0.0, dfTextY = 0.0 /* , dfTextZ = 0.0 */;
    double dfAngle = 0.0;
    double dfHeight = CPLAtof(poDS->GetVariable("$DIMTXT", "2.5"));

    CPLString osText;

    while( (nCode = poDS->ReadValue(szLineBuf,sizeof(szLineBuf))) > 0 )
    {
        switch( nCode )
        {
          case 10:
            dfArrowX1 = CPLAtof(szLineBuf);
            break;

          case 20:
            dfArrowY1 = CPLAtof(szLineBuf);
            break;

          case 30:
            /* dfArrowZ1 = CPLAtof(szLineBuf); */
            break;

          case 11:
            dfTextX = CPLAtof(szLineBuf);
            break;

          case 21:
            dfTextY = CPLAtof(szLineBuf);
            break;

          case 31:
            /* dfTextZ = CPLAtof(szLineBuf); */
            break;

          case 13:
            dfTargetX2 = CPLAtof(szLineBuf);
            break;

          case 23:
            dfTargetY2 = CPLAtof(szLineBuf);
            break;

          case 33:
            /* dfTargetZ2 = CPLAtof(szLineBuf); */
            break;

          case 14:
            dfTargetX1 = CPLAtof(szLineBuf);
            break;

          case 24:
            dfTargetY1 = CPLAtof(szLineBuf);
            break;

          case 34:
            /* dfTargetZ1 = CPLAtof(szLineBuf); */
            break;

          case 70:
            /* nDimType = atoi(szLineBuf); */
            break;

          case 1:
            osText = szLineBuf;
            break;

          default:
            TranslateGenericProperty( poFeature, nCode, szLineBuf );
            break;
        }
    }

    if( nCode == 0 )
        poDS->UnreadValue();

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

    ;
    
/* -------------------------------------------------------------------- */
/*      Step 1, compute direction vector between Target1 and Arrow1.    */
/* -------------------------------------------------------------------- */
    double dfVec1X, dfVec1Y;

    dfVec1X = (dfArrowX1 - dfTargetX1);
    dfVec1Y = (dfArrowY1 - dfTargetY1);
    
/* -------------------------------------------------------------------- */
/*      Step 2, compute the direction vector from Arrow1 to Arrow2      */
/*      as a perpendicluar to Vec1.                                     */
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
        dfArrowX2 = dfTargetX2;
        dfArrowY2 = dfArrowY1;
    }

    // special case if vec2 is horizontal.
    else if( dfVec1Y == 0.0 )
    {
        dfArrowX2 = dfArrowX1;
        dfArrowY2 = dfTargetY2;
    }

    else // General case for diagonal vectors.
    {
        // first convert vec1 + target2 into y = mx + b format: call this L1

        dfL1M = dfVec1Y / dfVec1X;
        dfL1B = dfTargetY2 - dfL1M * dfTargetX2;

        // convert vec2 + Arrow1 into y = mx + b format, call this L2
        
        dfL2M = dfVec2Y / dfVec2X;
        dfL2B = dfArrowY1 - dfL2M * dfArrowX1;
        
        // Compute intersection x = (b2-b1) / (m1-m2)
        
        dfArrowX2 = (dfL2B - dfL1B) / (dfL1M-dfL2M);
        dfArrowY2 = dfL2M * dfArrowX2 + dfL2B;
    }

/* -------------------------------------------------------------------- */
/*      Compute the text angle.                                         */
/* -------------------------------------------------------------------- */
    dfAngle = atan2(dfVec2Y,dfVec2X) * 180.0 / PI;

/* -------------------------------------------------------------------- */
/*      Rescale the direction vectors so we can use them in             */
/*      constructing arrowheads.  We want them to be about 3% of the    */
/*      length of line on which the arrows will be drawn.               */
/* -------------------------------------------------------------------- */
#define VECTOR_LEN(x,y) sqrt( (x)*(x) + (y)*(y) )
#define POINT_DIST(x1,y1,x2,y2)  VECTOR_LEN((x2-x1),(y2-y1))

    double dfBaselineLength = POINT_DIST(dfArrowX1,dfArrowY1,
                                         dfArrowX2,dfArrowY2);
    double dfTargetLength = dfBaselineLength * 0.03;
    double dfScaleFactor;

    // recompute vector 2 to ensure the direction is regular
    dfVec2X = (dfArrowX2 - dfArrowX1);
    dfVec2Y = (dfArrowY2 - dfArrowY1);

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
    oLine.setPoint( 0, dfArrowX1, dfArrowY1 );
    oLine.setPoint( 1, dfArrowX2, dfArrowY2 );
    poMLS->addGeometry( &oLine );

    // dimension line from Target1 to Arrow1 with a small extension.
    oLine.setPoint( 0, dfTargetX1, dfTargetY1 );
    oLine.setPoint( 1, dfArrowX1 + dfVec1X, dfArrowY1 + dfVec1Y );
    poMLS->addGeometry( &oLine );
    
    // dimension line from Target2 to Arrow2 with a small extension.
    oLine.setPoint( 0, dfTargetX2, dfTargetY2 );
    oLine.setPoint( 1, dfArrowX2 + dfVec1X, dfArrowY2 + dfVec1Y );
    poMLS->addGeometry( &oLine );

    // add arrow1 arrow head.

    oLine.setPoint( 0, dfArrowX1, dfArrowY1 );
    oLine.setPoint( 1, 
                    dfArrowX1 + dfVec2X*3 + dfVec1X,
                    dfArrowY1 + dfVec2Y*3 + dfVec1Y );
    poMLS->addGeometry( &oLine );

    oLine.setPoint( 0, dfArrowX1, dfArrowY1 );
    oLine.setPoint( 1, 
                    dfArrowX1 + dfVec2X*3 - dfVec1X,
                    dfArrowY1 + dfVec2Y*3 - dfVec1Y );
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
/*      Prepare a new feature to serve as the dimension text label      */
/*      feature.  We will push it onto the layer as a pending           */
/*      feature for the next feature read.                              */
/* -------------------------------------------------------------------- */

    // a single space suppresses labelling.
    if( osText == " " )
        return poFeature;

    OGRFeature *poLabelFeature = poFeature->Clone();

    poLabelFeature->SetGeometryDirectly( new OGRPoint( dfTextX, dfTextY ) );

    // Do we need to compute the dimension value?
    if( osText.size() == 0 )
    {
        FormatDimension( osText, POINT_DIST( dfArrowX1, dfArrowY1, 
                                             dfArrowX2, dfArrowY2 ) );
    }

    CPLString osStyle;
    char szBuffer[64];

    osStyle.Printf("LABEL(f:\"Arial\",t:\"%s\",p:5",osText.c_str());

    if( dfAngle != 0.0 )
    {
        CPLsnprintf(szBuffer, sizeof(szBuffer), "%.3g", dfAngle);
        osStyle += CPLString().Printf(",a:%s", szBuffer);
    }

    if( dfHeight != 0.0 )
    {
        CPLsnprintf(szBuffer, sizeof(szBuffer), "%.3g", dfHeight);
        osStyle += CPLString().Printf(",s:%sg", szBuffer);
    }

    // add color!

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

void OGRDXFLayer::FormatDimension( CPLString &osText, double dfValue )

{
    int nPrecision = atoi(poDS->GetVariable("$LUPREC","4"));
    char szFormat[32];
    char szBuffer[64];

    // we could do a significantly more precise formatting if we want
    // to spend the effort.  See QCAD's rs_dimlinear.cpp and related files
    // for example.  

    sprintf(szFormat, "%%.%df", nPrecision );
    CPLsnprintf(szBuffer, sizeof(szBuffer), szFormat, dfValue);
    osText = szBuffer;
}
