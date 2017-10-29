/******************************************************************************
 *
 * Project:  DXF Translator
 * Purpose:  Implements translation support for LEADER and MULTILEADER
 *           elements as a part of the OGRDXFLayer class.
 * Author:   Alan Thomas, alant@outlook.com.au
 *
 ******************************************************************************
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
/*                          TranslateLEADER()                           */
/************************************************************************/

OGRDXFFeature *OGRDXFLayer::TranslateLEADER()

{
    char szLineBuf[257];
    int nCode;
    OGRDXFFeature *poFeature = new OGRDXFFeature( poFeatureDefn );

    OGRLineString *poLine = new OGRLineString();
    bool bHaveX = false;
    bool bHaveY = false;
    bool bHaveZ = false;
    double dfCurrentX = 0.0;
    double dfCurrentY = 0.0;
    double dfCurrentZ = 0.0;
    int nNumVertices = 0;

    // When $DIMTAD (77) is nonzero, the leader line is extended under
    // the text annotation. This extension is not stored as an additional
    // vertex, so we need to create it ourselves.
    bool bExtensionDirectionFlip = true;
    double dfExtensionDirectionX = 1.0;
    double dfExtensionDirectionY = 0.0;
    double dfExtensionDirectionZ = 0.0;
    bool bHasTextAnnotation = false;
    double dfTextAnnotationWidth = 0.0;

    // spec is silent as to default, but AutoCAD assumes true
    bool bWantArrowhead = true;

    bool bReadyForDimstyleOverride = false;

    std::map<CPLString,CPLString> oDimStyleProperties;
    poDS->PopulateDefaultDimStyleProperties(oDimStyleProperties);

    while( (nCode = poDS->ReadValue(szLineBuf,sizeof(szLineBuf))) > 0 )
    {
        switch( nCode )
        {
          case 3:
            // 3 is the dimension style name. We don't need to store it,
            // let's just fetch the dimension style properties
            poDS->LookupDimStyle(szLineBuf, oDimStyleProperties);
            break;

          case 10:
            // add the previous point onto the linestring
            if( bHaveX && bHaveY && bHaveZ ) {
                poLine->setPoint( nNumVertices++,
                    dfCurrentX, dfCurrentY, dfCurrentZ );
                bHaveY = bHaveZ = false;
            }
            dfCurrentX = CPLAtof(szLineBuf);
            bHaveX = true;
            break;

          case 20:
            // add the previous point onto the linestring
            if( bHaveX && bHaveY && bHaveZ ) {
                poLine->setPoint( nNumVertices++,
                    dfCurrentX, dfCurrentY, dfCurrentZ );
                bHaveX = bHaveZ = false;
            }
            dfCurrentY = CPLAtof(szLineBuf);
            bHaveY = true;
            break;

          case 30:
            // add the previous point onto the linestring
            if( bHaveX && bHaveY && bHaveZ ) {
                poLine->setPoint( nNumVertices++,
                    dfCurrentX, dfCurrentY, dfCurrentZ );
                bHaveX = bHaveY = false;
            }
            dfCurrentZ = CPLAtof(szLineBuf);
            bHaveZ = true;
            break;

          case 41:
            dfTextAnnotationWidth = CPLAtof(szLineBuf);
            break;

          case 71:
            bWantArrowhead = atoi(szLineBuf) != 0;
            break;

          case 73:
            bHasTextAnnotation = atoi(szLineBuf) == 0;
            break;

          case 74:
            // DXF spec seems to have this backwards. A value of 0 actually
            // indicates no flipping occurs, and 1 (flip) is the default
            bExtensionDirectionFlip = atoi(szLineBuf) != 0;
            break;

          case 211:
            dfExtensionDirectionX = CPLAtof(szLineBuf);
            break;

          case 221:
            dfExtensionDirectionY = CPLAtof(szLineBuf);
            break;

          case 231:
            dfExtensionDirectionZ = CPLAtof(szLineBuf);
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

    if( nCode == 0 )
        poDS->UnreadValue();

    if( bHaveX && bHaveY && bHaveZ )
        poLine->setPoint( nNumVertices++, dfCurrentX, dfCurrentY, dfCurrentZ );

    // Unpack the dimension style
    bool bWantExtension = atoi(oDimStyleProperties["DIMTAD"]) > 0;
    double dfTextOffset = CPLAtof(oDimStyleProperties["DIMGAP"]);
    double dfScale = CPLAtof(oDimStyleProperties["DIMSCALE"]);
    double dfArrowheadSize = CPLAtof(oDimStyleProperties["DIMASZ"]);
    // DIMLDRBLK is the entity handle of the BLOCK_RECORD table entry that
    // corresponds to the arrowhead block.
    CPLString osArrowheadBlockHandle = oDimStyleProperties["DIMLDRBLK"];

    // Zero scale has a special meaning which we aren't interested in,
    // so we can change it to 1.0
    if( dfScale == 0.0 )
        dfScale = 1.0;

/* -------------------------------------------------------------------- */
/*      Add an extension to the end of the leader line. This is not     */
/*      properly documented in the DXF spec, but it is needed to        */
/*      replicate the way AutoCAD displays leader objects.              */
/* -------------------------------------------------------------------- */
    if( bWantExtension && bHasTextAnnotation && dfTextAnnotationWidth > 0 &&
        nNumVertices >= 2 )
    {
        OGRPoint oLastVertex;
        poLine->getPoint( nNumVertices - 1, &oLastVertex );

        if( bExtensionDirectionFlip )
        {
            dfExtensionDirectionX *= -1;
            dfExtensionDirectionY *= -1;
            dfExtensionDirectionZ *= -1;
        }

        double dfExtensionX = oLastVertex.getX();
        double dfExtensionY = oLastVertex.getY();
        double dfExtensionZ = oLastVertex.getZ();

        double dfExtensionLength = ( dfTextOffset * dfScale ) +
            dfTextAnnotationWidth;
        dfExtensionX += dfExtensionDirectionX * dfExtensionLength;
        dfExtensionY += dfExtensionDirectionY * dfExtensionLength;
        dfExtensionZ += dfExtensionDirectionZ * dfExtensionLength;

        poLine->setPoint( nNumVertices++, dfExtensionX, dfExtensionY,
            dfExtensionZ );
    }

    poFeature->SetGeometryDirectly( poLine );

/* -------------------------------------------------------------------- */
/*      Add an arrowhead to the start of the leader line.               */
/* -------------------------------------------------------------------- */

    if( bWantArrowhead && nNumVertices >= 2 )
    {
        // Get the first line segment of the leader
        OGRPoint oPoint1, oPoint2;
        poLine->getPoint( 0, &oPoint1 );
        poLine->getPoint( 1, &oPoint2 );

        InsertArrowhead( poFeature, osArrowheadBlockHandle, oPoint1, oPoint2,
            dfArrowheadSize * dfScale );
    }

    PrepareLineStyle( poFeature );

    return poFeature;
}

/************************************************************************/
/*                         TranslateMLEADER()                           */
/************************************************************************/

OGRDXFFeature *OGRDXFLayer::TranslateMLEADER()

{
    char szLineBuf[257];
    int nCode = 0;
    OGRDXFFeature *poFeature = new OGRDXFFeature( poFeatureDefn );

    double dfLandingX = 0.0;
    double dfLandingY = 0.0;
    double dfDoglegVectorX = 0.0;
    double dfDoglegVectorY = 0.0;
    double dfDoglegLength = 0.0;

    OGRLineString *poLine = new OGRLineString();
    bool bHaveX = false;
    bool bHaveY = false;
    double dfCurrentX = 0.0;
    double dfCurrentY = 0.0;
    int nNumVertices = 0;

    double dfScale = 1.0;

    CPLString osText;
    double dfTextX = 0.0;
    double dfTextY = 0.0;
    int iTextAlignment = 1; // 1 = left, 2 = center, 3 = right
    double dfTextAngle = 0.0;
    double dfTextHeight = 4.0;

    CPLString osArrowheadBlockHandle;
    double dfArrowheadSize = 4.0;

    // Group codes mean different things in different sections of the
    // MLEADER entity. We need to keep track of the section we are in.
    const int MLS_COMMON = 0;
    const int MLS_CONTEXT_DATA = 1;
    const int MLS_LEADER = 2;
    const int MLS_LEADER_LINE = 3;
    int nSection = MLS_COMMON;

    while( (nCode = poDS->ReadValue(szLineBuf,sizeof(szLineBuf))) > 0 )
    {
        switch( nCode )
        {
          // The way the 30x group codes work is missing from the DXF docs.
          // We assume that the sections are always nested as follows:

          // COMMON group codes
          // 300
          //   CONTEXT_DATA group codes
          //   302
          //     LEADER group codes
          //     304
          //       LEADER_LINE group codes
          //     305
          //     LEADER group codes continued
          //   303
          //   CONTEXT_DATA group codes continued
          // 301
          // COMMON group codes continued

          case 300:
          case 303:
            nSection = MLS_CONTEXT_DATA;
            break;

          case 302:
          case 305:
            nSection = MLS_LEADER;
            break;

          case 304:
            if( nSection == MLS_CONTEXT_DATA )
                osText = TextUnescape(szLineBuf, true);
            else
                nSection = MLS_LEADER_LINE;
            break;

          case 301:
            nSection = MLS_COMMON;
            break;

          case 10:
            if( nSection == MLS_LEADER )
                dfLandingX = CPLAtof(szLineBuf);
            else if( nSection == MLS_LEADER_LINE )
            {
                // add the previous point onto the linestring
                if( bHaveX && bHaveY ) {
                    poLine->setPoint( nNumVertices++, dfCurrentX, dfCurrentY );
                    bHaveY = false;
                }
                dfCurrentX = CPLAtof(szLineBuf);
                bHaveX = true;
            }
            break;

          case 20:
            if( nSection == MLS_LEADER )
                dfLandingY = CPLAtof(szLineBuf);
            else if( nSection == MLS_LEADER_LINE )
            {
                // add the previous point onto the linestring
                if( bHaveX && bHaveY ) {
                    poLine->setPoint( nNumVertices++, dfCurrentX, dfCurrentY );
                    bHaveX = false;
                }
                dfCurrentY = CPLAtof(szLineBuf);
                bHaveY = true;
            }
            break;

          case 11:
            if( nSection == MLS_LEADER )
                dfDoglegVectorX = CPLAtof(szLineBuf);
            break;

          case 21:
            if( nSection == MLS_LEADER )
                dfDoglegVectorY = CPLAtof(szLineBuf);
            break;

          case 40:
            if( nSection == MLS_CONTEXT_DATA )
                dfScale = CPLAtof( szLineBuf );
            else if( nSection == MLS_LEADER )
                dfDoglegLength = CPLAtof(szLineBuf);
            break;

          case 290:
            if( nSection == MLS_LEADER && atoi(szLineBuf) != 1 )
            {
                // can't handle this situation, we need the last leader
                // line point to be set
                DXF_LAYER_READER_ERROR();
                delete poFeature;
                delete poLine;
                return NULL;
            }
            break;

          case 342:
            // 342 is the entity handle of the BLOCK_RECORD table entry that
            // corresponds to the arrowhead block.
            if( nSection == MLS_COMMON )
                osArrowheadBlockHandle = szLineBuf;
            break;

          case 12:
            if( nSection == MLS_CONTEXT_DATA )
                dfTextX = CPLAtof( szLineBuf );
            break;

          case 22:
            if( nSection == MLS_CONTEXT_DATA )
                dfTextY = CPLAtof( szLineBuf );
            break;

          case 41:
            if( nSection == MLS_CONTEXT_DATA )
                dfTextHeight = CPLAtof( szLineBuf );
            break;

          case 42:
            // TODO figure out difference between 42 and 140 for arrowheadsize
            if( nSection == MLS_COMMON )
                dfArrowheadSize = CPLAtof( szLineBuf );
            else if( nSection == MLS_CONTEXT_DATA )
                dfTextAngle = CPLAtof( szLineBuf ) * 180 / M_PI;
            break;

          case 171:
            if( nSection == MLS_CONTEXT_DATA )
                iTextAlignment = atoi( szLineBuf );
            break;

          default:
            if( nSection == MLS_COMMON )
                TranslateGenericProperty( poFeature, nCode, szLineBuf );
            break;
        }
    }
    if( nCode < 0 )
    {
        DXF_LAYER_READER_ERROR();
        delete poFeature;
        delete poLine;
        return NULL;
    }
    if( nCode == 0 )
        poDS->UnreadValue();

    if( bHaveX && bHaveY )
        poLine->setPoint( nNumVertices++, dfCurrentX, dfCurrentY );

/* -------------------------------------------------------------------- */
/*      Add the landing, and dogleg if present, onto the leader.        */
/* -------------------------------------------------------------------- */
    poLine->setPoint( nNumVertices++, dfLandingX, dfLandingY );

    if( dfDoglegLength != 0.0 && ( dfDoglegVectorX != 0.0 || dfDoglegVectorY != 0.0 ) )
    {
        // We assume that the dogleg vector in the DXF is a unit vector.
        // Safe assumption? Who knows. The documentation is so bad.
        poLine->setPoint( nNumVertices++,
            dfLandingX + dfDoglegVectorX * dfDoglegLength,
            dfLandingY + dfDoglegVectorY * dfDoglegLength );
    }

    poFeature->SetGeometryDirectly( poLine );

/* -------------------------------------------------------------------- */
/*      Add an arrowhead to the start of the leader line.               */
/* -------------------------------------------------------------------- */

    // Get the first line segment of the leader and its length.
    if( nNumVertices >= 2 )
    {
        OGRPoint oPoint1, oPoint2;
        poLine->getPoint( 0, &oPoint1 );
        poLine->getPoint( 1, &oPoint2 );

        InsertArrowhead( poFeature, osArrowheadBlockHandle, oPoint1, oPoint2,
            dfArrowheadSize * dfScale );
    }

    PrepareLineStyle( poFeature );

/* -------------------------------------------------------------------- */
/*      Prepare a new feature to serve as the leader text label         */
/*      feature.  We will push it onto the layer as a pending           */
/*      feature for the next feature read.                              */
/* -------------------------------------------------------------------- */

    if( osText.empty() || osText == " " )
        return poFeature;

    OGRDXFFeature *poLabelFeature = poFeature->CloneDXFFeature();

    poLabelFeature->SetField( "Text", osText );
    poLabelFeature->SetGeometryDirectly( new OGRPoint( dfTextX, dfTextY ) );

    CPLString osStyle;
    char szBuffer[64];

    osStyle.Printf("LABEL(f:\"Arial\",t:\"%s\",p:%d", osText.c_str(),
        iTextAlignment + 6); // 7,8,9: vertical align top

    if( dfTextAngle != 0.0 )
    {
        CPLsnprintf(szBuffer, sizeof(szBuffer), "%.3g", dfTextAngle);
        osStyle += CPLString().Printf(",a:%s", szBuffer);
    }

    if( dfTextHeight != 0.0 )
    {
        CPLsnprintf(szBuffer, sizeof(szBuffer), "%.3g", dfTextHeight);
        osStyle += CPLString().Printf(",s:%sg", szBuffer);
    }

    // Add color!

    osStyle += ")";

    poLabelFeature->SetStyleString( osStyle );

    apoPendingFeatures.push( poLabelFeature );

    return poFeature;
}

/************************************************************************/
/*                     GenerateDefaultArrowhead()                       */
/*                                                                      */
/*      Generates the default DWG/DXF arrowhead (a filled triangle      */
/*      with a 3:1 aspect ratio) on the end of the line segment         */
/*      defined by the two points.                                      */
/************************************************************************/
static void GenerateDefaultArrowhead( OGRDXFFeature* const poArrowheadFeature,
    const OGRPoint& oPoint1, const OGRPoint& oPoint2,
    const double dfArrowheadScale )

{
    // calculate the baseline to be expanded out into arrowheads
    const double dfParallelPartX = dfArrowheadScale *
        (oPoint2.getX() - oPoint1.getX());
    const double dfParallelPartY = dfArrowheadScale *
        (oPoint2.getY() - oPoint1.getY());
    // ...and drop a perpendicular
    const double dfPerpPartX = dfParallelPartY;
    const double dfPerpPartY = -dfParallelPartX;

    OGRLinearRing *poLinearRing = new OGRLinearRing();
    poLinearRing->setPoint( 0,
        oPoint1.getX() + dfParallelPartX + dfPerpPartX/6,
        oPoint1.getY() + dfParallelPartY + dfPerpPartY/6,
        oPoint1.getZ() );
    poLinearRing->setPoint( 1,
        oPoint1.getX(),
        oPoint1.getY(),
        oPoint1.getZ() );
    poLinearRing->setPoint( 2,
        oPoint1.getX() + dfParallelPartX - dfPerpPartX/6,
        oPoint1.getY() + dfParallelPartY - dfPerpPartY/6,
        oPoint1.getZ() );
    poLinearRing->closeRings();

    OGRPolygon* poPoly = new OGRPolygon();
    poPoly->addRingDirectly( poLinearRing );

    poArrowheadFeature->SetGeometryDirectly( poPoly );
}

/************************************************************************/
/*                          InsertArrowhead()                           */
/*                                                                      */
/*      Inserts the specified arrowhead block at the oPoint1 end of     */
/*      the line segment defined by the two points.                     */
/************************************************************************/
void OGRDXFLayer::InsertArrowhead( OGRDXFFeature* const poFeature,
    const CPLString& osBlockHandle, const OGRPoint& oPoint1,
    const OGRPoint& oPoint2, const double dfArrowheadSize )
{
    const double dfFirstSegmentLength = PointDist( oPoint1.getX(),
        oPoint1.getY(), oPoint2.getX(), oPoint2.getY() );

    // AutoCAD only displays an arrowhead if the length of the arrowhead
    // is less than or equal to half the length of the line segment
    if( dfArrowheadSize > 0.5 * dfFirstSegmentLength )
        return;

    OGRDXFFeature *poArrowheadFeature = poFeature->CloneDXFFeature();

    // Convert the block handle to a block name.
    const CPLString osBlockName =
        poDS->GetBlockNameByRecordHandle( osBlockHandle );

    // If the block doesn't exist, we need to fall back to the
    // default arrowhead.
    if( osBlockName == "" )
    {
        GenerateDefaultArrowhead( poArrowheadFeature, oPoint1, oPoint2,
            dfArrowheadSize / dfFirstSegmentLength );

        PrepareLineStyle( poArrowheadFeature );
        apoPendingFeatures.push( poArrowheadFeature );

        return;
    }

    // Build a transformer to insert the arrowhead block with the
    // required location, angle and scale.
    OGRDXFInsertTransformer oTransformer;
    oTransformer.dfXOffset = oPoint1.getX();
    oTransformer.dfYOffset = oPoint1.getY();
    oTransformer.dfZOffset = oPoint1.getZ();
    // Arrowhead blocks always point to the right (--->)
    oTransformer.dfAngle = atan2( oPoint2.getY() - oPoint1.getY(),
        oPoint2.getX() - oPoint1.getX() ) + M_PI;
    oTransformer.dfXScale = oTransformer.dfYScale =
        oTransformer.dfZScale = dfArrowheadSize;

    std::queue<OGRDXFFeature *> apoExtraFeatures;

    // Insert the block.
    try
    {
        poArrowheadFeature = InsertBlockInline( osBlockName,
            oTransformer, NULL, poArrowheadFeature, apoExtraFeatures,
            true, false );
    }
    catch( const std::invalid_argument& )
    {
        // Supposedly the block doesn't exist. But what has probably
        // happened is that the block exists in the DXF, but it contains
        // no entities, so the data source didn't read it in.
        // In this case, no arrowhead is required.
        delete poArrowheadFeature;
        poArrowheadFeature = NULL;
    }

    // Add the arrowhead geometries to the pending feature stack.
    if( poArrowheadFeature )
    {
        apoPendingFeatures.push( poArrowheadFeature );
    }
    while( !apoExtraFeatures.empty() )
    {
        apoPendingFeatures.push( apoExtraFeatures.front() );
        apoExtraFeatures.pop();
    }
}
