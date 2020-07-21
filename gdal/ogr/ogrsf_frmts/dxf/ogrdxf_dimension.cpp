/******************************************************************************
 *
 * Project:  DXF Translator
 * Purpose:  Implements translation support for DIMENSION elements as a part
 *           of the OGRDXFLayer class.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2009, Frank Warmerdam <warmerdam@pobox.com>
 * Copyright (c) 2010, Even Rouault <even dot rouault at spatialys.com>
 * Copyright (c) 2017, Alan Thomas <alant@outlook.com.au>
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

#include <stdexcept>

CPL_CVSID("$Id$")

/************************************************************************/
/*                             PointDist()                              */
/************************************************************************/

inline static double PointDist( double x1, double y1, double x2, double y2 )
{
    return sqrt( (x2 - x1) * (x2 - x1) + (y2 - y1) * (y2 - y1) );
}

/************************************************************************/
/*                         TranslateDIMENSION()                         */
/************************************************************************/

OGRDXFFeature *OGRDXFLayer::TranslateDIMENSION()

{
    char szLineBuf[257];
    int nCode = 0;
    // int  nDimType = 0;
    OGRDXFFeature *poFeature = new OGRDXFFeature( poFeatureDefn );
    double dfArrowX1 = 0.0;
    double dfArrowY1 = 0.0;
    // double dfArrowZ1 = 0.0;
    double dfTargetX1 = 0.0;
    double dfTargetY1 = 0.0;
    // double dfTargetZ1 = 0.0;
    double dfTargetX2 = 0.0;
    double dfTargetY2 = 0.0;
    // double dfTargetZ2 = 0.0;
    double dfTextX = 0.0;
    double dfTextY = 0.0;
    // double dfTextZ = 0.0;

    bool bReadyForDimstyleOverride = false;

    bool bHaveBlock = false;
    CPLString osBlockName;
    CPLString osText;

    std::map<CPLString,CPLString> oDimStyleProperties;
    poDS->PopulateDefaultDimStyleProperties(oDimStyleProperties);

    while( (nCode = poDS->ReadValue(szLineBuf,sizeof(szLineBuf))) > 0 )
    {
        switch( nCode )
        {
          case 2:
            bHaveBlock = true;
            osBlockName = szLineBuf;
            break;

          case 3:
            // 3 is the dimension style name. We don't need to store it,
            // let's just fetch the dimension style properties
            poDS->LookupDimStyle(szLineBuf, oDimStyleProperties);
            break;

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

          case 1001:
            bReadyForDimstyleOverride = EQUAL(szLineBuf, "ACAD");
            break;

          case 1070:
            if( bReadyForDimstyleOverride )
            {
                // Store DIMSTYLE override values in the dimension
                // style property map. The nInnerCode values match the
                // group codes used in the DIMSTYLE table.
                const int nInnerCode = atoi(szLineBuf);
                const char* pszProperty = ACGetDimStylePropertyName(nInnerCode);
                if( pszProperty )
                {
                    nCode = poDS->ReadValue(szLineBuf,sizeof(szLineBuf));
                    if( nCode == 1005 || nCode == 1040 || nCode == 1070 )
                        oDimStyleProperties[pszProperty] = szLineBuf;
                }
            }
            break;

          default:
            TranslateGenericProperty( poFeature, nCode, szLineBuf );
            break;
        }
    }
    if( nCode < 0 )
    {
        DXF_LAYER_READER_ERROR();
        delete poFeature;
        return nullptr;
    }
    if( nCode == 0 )
        poDS->UnreadValue();

    // If osBlockName (group code 2) refers to a valid block, we can just insert
    // that block - that should give us the correctly exploded geometry of this
    // dimension. If this value is missing, or doesn't refer to a valid block,
    // we will need to use our own logic to generate the dimension lines.
    if( bHaveBlock && osBlockName.length() > 0 )
    {
        // Always inline the block, because this is an anonymous block that the
        // user likely doesn't know or care about
        try
        {
            OGRDXFFeature* poBlockFeature = InsertBlockInline(
                CPLGetErrorCounter(), osBlockName,
                OGRDXFInsertTransformer(), poFeature, apoPendingFeatures,
                true, false );

            return poBlockFeature; // may be NULL but that is OK
        }
        catch( const std::invalid_argument& ) {}
    }

    // Unpack the dimension style
    const double dfScale = CPLAtof(oDimStyleProperties["DIMSCALE"]);
    const double dfArrowheadSize = CPLAtof(oDimStyleProperties["DIMASZ"]);
    const double dfExtLineExtendLength = CPLAtof(oDimStyleProperties["DIMEXE"]);
    const double dfExtLineOffset = CPLAtof(oDimStyleProperties["DIMEXO"]);
    const bool bWantExtLine1 = atoi(oDimStyleProperties["DIMSE1"]) == 0;
    const bool bWantExtLine2 = atoi(oDimStyleProperties["DIMSE2"]) == 0;
    const double dfTextHeight = CPLAtof(oDimStyleProperties["DIMTXT"]);
    const int nUnitsPrecision = atoi(oDimStyleProperties["DIMDEC"]);
    const bool bTextSupposedlyCentered = atoi(oDimStyleProperties["DIMTAD"]) == 0;
    const CPLString osTextColor = oDimStyleProperties["DIMCLRT"];

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
    double dfVec1X = dfArrowX1 - dfTargetX1;
    double dfVec1Y = dfArrowY1 - dfTargetY1;

    // make Vec1 a unit vector
    double dfVec1Length = PointDist(0, 0, dfVec1X, dfVec1Y);
    if( dfVec1Length > 0.0 )
    {
        dfVec1X /= dfVec1Length;
        dfVec1Y /= dfVec1Length;
    }

/* -------------------------------------------------------------------- */
/*      Step 2, compute the direction vector from Arrow1 to Arrow2      */
/*      as a perpendicular to Vec1.                                     */
/* -------------------------------------------------------------------- */
    double dfVec2X = dfVec1Y;
    double dfVec2Y = -dfVec1X;

/* -------------------------------------------------------------------- */
/*      Step 3, compute intersection of line from target2 along         */
/*      direction vector 1, with the line through Arrow1 and            */
/*      direction vector 2.                                             */
/* -------------------------------------------------------------------- */
    double dfArrowX2 = 0.0;
    double dfArrowY2 = 0.0;

    // special case if vec1 is zero, which means the arrow and target
    // points coincide.
    if( dfVec1X == 0.0 && dfVec1Y == 0.0 )
    {
        dfArrowX2 = dfTargetX2;
        dfArrowY2 = dfTargetY2;
    }

    // special case if vec1 is vertical.
    else if( dfVec1X == 0.0 )
    {
        dfArrowX2 = dfTargetX2;
        dfArrowY2 = dfArrowY1;
    }

    // special case if vec1 is horizontal.
    else if( dfVec1Y == 0.0 )
    {
        dfArrowX2 = dfArrowX1;
        dfArrowY2 = dfTargetY2;
    }

    else // General case for diagonal vectors.
    {
        // first convert vec1 + target2 into y = mx + b format: call this L1

        const double dfL1M = dfVec1Y / dfVec1X;
        const double dfL1B = dfTargetY2 - dfL1M * dfTargetX2;

        // convert vec2 + Arrow1 into y = mx + b format, call this L2

        const double dfL2M = dfVec2Y / dfVec2X;
        const double dfL2B = dfArrowY1 - dfL2M * dfArrowX1;

        // Compute intersection x = (b2-b1) / (m1-m2)

        dfArrowX2 = (dfL2B - dfL1B) / (dfL1M-dfL2M);
        dfArrowY2 = dfL2M * dfArrowX2 + dfL2B;
    }

/* -------------------------------------------------------------------- */
/*      Create geometries for the different components of the           */
/*      dimension object.                                               */
/* -------------------------------------------------------------------- */
    OGRMultiLineString *poMLS = new OGRMultiLineString();
    OGRLineString oLine;

    // Main arrow line between Arrow1 and Arrow2.
    oLine.setPoint( 0, dfArrowX1, dfArrowY1 );
    oLine.setPoint( 1, dfArrowX2, dfArrowY2 );
    poMLS->addGeometry( &oLine );

    // Insert default arrowheads.
    InsertArrowhead( poFeature, "", &oLine, dfArrowheadSize * dfScale );
    InsertArrowhead( poFeature, "", &oLine, dfArrowheadSize * dfScale, true );

    // Dimension line from Target1 to Arrow1 with a small extension.
    oLine.setPoint( 0, dfTargetX1 + dfVec1X * dfExtLineOffset,
        dfTargetY1 + dfVec1Y * dfExtLineOffset );
    oLine.setPoint( 1, dfArrowX1 + dfVec1X * dfExtLineExtendLength,
        dfArrowY1 + dfVec1Y * dfExtLineExtendLength );
    if( bWantExtLine1 && oLine.get_Length() > 0.0 ) {
        poMLS->addGeometry( &oLine );
    }

    // Dimension line from Target2 to Arrow2 with a small extension.
    oLine.setPoint( 0, dfTargetX2 + dfVec1X * dfExtLineOffset,
        dfTargetY2 + dfVec1Y * dfExtLineOffset );
    oLine.setPoint( 1, dfArrowX2 + dfVec1X * dfExtLineExtendLength,
        dfArrowY2 + dfVec1Y * dfExtLineExtendLength );
    if( bWantExtLine2 && oLine.get_Length() > 0.0 ) {
        poMLS->addGeometry( &oLine );
    }

    poFeature->SetGeometryDirectly( poMLS );

    PrepareLineStyle( poFeature );

/* -------------------------------------------------------------------- */
/*      Prepare a new feature to serve as the dimension text label      */
/*      feature.  We will push it onto the layer as a pending           */
/*      feature for the next feature read.                              */
/*                                                                      */
/*      The DXF format supports a myriad of options for dimension       */
/*      text placement, some of which involve the drawing of            */
/*      additional lines and the like.  For now we ignore most of       */
/*      those properties and place the text alongside the dimension     */
/*      line.                                                           */
/* -------------------------------------------------------------------- */

    // a single space suppresses labeling.
    if( osText == " " )
        return poFeature;

    OGRDXFFeature *poLabelFeature = poFeature->CloneDXFFeature();

    poLabelFeature->SetGeometryDirectly( new OGRPoint( dfTextX, dfTextY ) );

    if( osText.empty() )
        osText = "<>";

    // Do we need to compute the dimension value?
    size_t nDimensionPos = osText.find("<>");
    if( nDimensionPos == std::string::npos )
    {
        poLabelFeature->SetField( "Text", TextUnescape( osText, true ) );
    }
    else
    {
        // Replace the first occurrence of <> with the dimension
        CPLString osDimensionText;
        FormatDimension( osDimensionText,
            PointDist( dfArrowX1, dfArrowY1, dfArrowX2, dfArrowY2 ),
            nUnitsPrecision );
        osText.replace( nDimensionPos, 2, osDimensionText );
        poLabelFeature->SetField( "Text", TextUnescape( osText, true ) );
    }

    CPLString osStyle;
    char szBuffer[64];

    osStyle.Printf("LABEL(f:\"Arial\",t:\"%s\"",
        TextUnescape( osText.c_str(), true ).c_str());

    // If the text is supposed to be centered on the line, we align
    // it above the line. Drawing it properly would require us to
    // work out the width of the text, which seems like too much
    // effort for what is just a fallback renderer.
    if( bTextSupposedlyCentered )
        osStyle += ",p:11";
    else
        osStyle += ",p:5";

    // Compute the text angle. Use atan to avoid upside-down text
    const double dfTextAngle = ( dfArrowX1 == dfArrowX2 ) ?
        -90.0 :
        atan( (dfArrowY1 - dfArrowY2) / (dfArrowX1 - dfArrowX2) ) * 180.0 / M_PI;

    if( dfTextAngle != 0.0 )
    {
        CPLsnprintf(szBuffer, sizeof(szBuffer), "%.3g", dfTextAngle);
        osStyle += CPLString().Printf(",a:%s", szBuffer);
    }

    if( dfTextHeight != 0.0 )
    {
        CPLsnprintf(szBuffer, sizeof(szBuffer), "%.3g",
            dfTextHeight * dfScale);
        osStyle += CPLString().Printf(",s:%sg", szBuffer);
    }

    poLabelFeature->oStyleProperties["Color"] = osTextColor;
    osStyle += ",c:";
    osStyle += poLabelFeature->GetColor( poDS, poFeature );

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

void OGRDXFLayer::FormatDimension( CPLString &osText, const double dfValue,
    int nPrecision )

{
    if( nPrecision < 0 )
        nPrecision = 0;
    else if( nPrecision > 20 )
        nPrecision = 20;

    // We could do a significantly more precise formatting if we want
    // to spend the effort.  See QCAD's rs_dimlinear.cpp and related files
    // for example.

    char szFormat[32];
    snprintf(szFormat, sizeof(szFormat), "%%.%df", nPrecision );

    char szBuffer[64];
    CPLsnprintf(szBuffer, sizeof(szBuffer), szFormat, dfValue);

    osText = szBuffer;
}
