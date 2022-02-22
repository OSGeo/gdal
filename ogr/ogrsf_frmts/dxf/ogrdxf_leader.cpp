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
#include "../../../alg/gdallinearsystem.h"
#include <stdexcept>
#include <algorithm>

CPL_CVSID("$Id$")

static void InterpolateSpline( OGRLineString* const poLine,
    const DXFTriple& oEndTangentDirection );

/************************************************************************/
/*                             PointDist()                              */
/************************************************************************/

inline static double PointDist( double x1, double y1, double x2, double y2 )
{
    return sqrt( (x2 - x1) * (x2 - x1) + (y2 - y1) * (y2 - y1) );
}

inline static double PointDist( double x1, double y1, double z1, double x2,
    double y2, double z2 )
{
    return sqrt( (x2 - x1) * (x2 - x1) + (y2 - y1) * (y2 - y1) +
        (z2 - z1) * (z2 - z1) );
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

    bool bHorizontalDirectionFlip = true;
    double dfHorizontalDirectionX = 1.0;
    double dfHorizontalDirectionY = 0.0;
    double dfHorizontalDirectionZ = 0.0;
    bool bHasTextAnnotation = false;
    double dfTextAnnotationWidth = 0.0;
    bool bIsSpline = false;

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

          case 72:
            bIsSpline = atoi(szLineBuf) != 0;
            break;

          case 73:
            bHasTextAnnotation = atoi(szLineBuf) == 0;
            break;

          case 74:
            // DXF spec seems to have this backwards. A value of 0 actually
            // indicates no flipping occurs, and 1 (flip) is the default
            bHorizontalDirectionFlip = atoi(szLineBuf) != 0;
            break;

          case 211:
            dfHorizontalDirectionX = CPLAtof(szLineBuf);
            break;

          case 221:
            dfHorizontalDirectionY = CPLAtof(szLineBuf);
            break;

          case 231:
            dfHorizontalDirectionZ = CPLAtof(szLineBuf);
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
    int nLeaderColor = atoi(oDimStyleProperties["DIMCLRD"]);
    // DIMLDRBLK is the entity handle of the BLOCK_RECORD table entry that
    // corresponds to the arrowhead block.
    CPLString osArrowheadBlockHandle = oDimStyleProperties["DIMLDRBLK"];

    // Zero scale has a special meaning which we aren't interested in,
    // so we can change it to 1.0
    if( dfScale == 0.0 )
        dfScale = 1.0;

    // Use the color from the dimension style if it is not ByBlock
    if( nLeaderColor > 0 )
        poFeature->oStyleProperties["Color"] = oDimStyleProperties["DIMCLRD"];

/* -------------------------------------------------------------------- */
/*      Add an arrowhead to the start of the leader line.               */
/* -------------------------------------------------------------------- */

    if( bWantArrowhead && nNumVertices >= 2 )
    {
        InsertArrowhead( poFeature, osArrowheadBlockHandle, poLine,
            dfArrowheadSize * dfScale );
    }


    if( bHorizontalDirectionFlip )
    {
        dfHorizontalDirectionX *= -1;
        dfHorizontalDirectionX *= -1;
        dfHorizontalDirectionX *= -1;
    }

/* -------------------------------------------------------------------- */
/*      For a spline leader, determine the end tangent direction        */
/*      and interpolate the spline vertices.                            */
/* -------------------------------------------------------------------- */

    if( bIsSpline )
    {
        DXFTriple oEndTangent;
        if( bHasTextAnnotation )
        {
            oEndTangent = DXFTriple( dfHorizontalDirectionX,
                dfHorizontalDirectionY, dfHorizontalDirectionZ );
        }
        InterpolateSpline( poLine, oEndTangent );
    }

/* -------------------------------------------------------------------- */
/*      Add an extension to the end of the leader line. This is not     */
/*      properly documented in the DXF spec, but it is needed to        */
/*      replicate the way AutoCAD displays leader objects.              */
/*                                                                      */
/*      When $DIMTAD (77) is nonzero, the leader line is extended       */
/*      under the text annotation. This extension is not stored as an   */
/*      additional vertex, so we need to create it ourselves.           */
/* -------------------------------------------------------------------- */

    if( bWantExtension && bHasTextAnnotation && poLine->getNumPoints() >= 2 )
    {
        OGRPoint oLastVertex;
        poLine->getPoint( poLine->getNumPoints() - 1, &oLastVertex );

        double dfExtensionX = oLastVertex.getX();
        double dfExtensionY = oLastVertex.getY();
        double dfExtensionZ = oLastVertex.getZ();

        double dfExtensionLength = ( dfTextOffset * dfScale ) +
            dfTextAnnotationWidth;
        dfExtensionX += dfHorizontalDirectionX * dfExtensionLength;
        dfExtensionY += dfHorizontalDirectionY * dfExtensionLength;
        dfExtensionZ += dfHorizontalDirectionZ * dfExtensionLength;

        poLine->setPoint( poLine->getNumPoints(), dfExtensionX, dfExtensionY,
            dfExtensionZ );
    }

    poFeature->SetGeometryDirectly( poLine );

    PrepareLineStyle( poFeature );

    return poFeature;
}

/************************************************************************/
/*       DXFMLEADERVertex, DXFMLEADERLeaderLine, DXFMLEADERLeader       */
/************************************************************************/

struct DXFMLEADERVertex {
    DXFTriple                oCoords;
    std::vector<std::pair<DXFTriple, DXFTriple>> aoBreaks;

    DXFMLEADERVertex( double dfX, double dfY )
        : oCoords( DXFTriple( dfX, dfY, 0.0 ) ) {}
};

struct DXFMLEADERLeader {
    double                   dfLandingX = 0;
    double                   dfLandingY = 0;
    double                   dfDoglegVectorX = 0;
    double                   dfDoglegVectorY = 0;
    double                   dfDoglegLength = 0;
    std::vector<std::pair<DXFTriple, DXFTriple>> aoDoglegBreaks;
    std::vector<std::vector<DXFMLEADERVertex>> aaoLeaderLines;
};

/************************************************************************/
/*                         TranslateMLEADER()                           */
/************************************************************************/

OGRDXFFeature *OGRDXFLayer::TranslateMLEADER()

{
    // The MLEADER line buffer has to be very large, as the text contents
    // (group code 304) do not wrap and may be arbitrarily long
    char szLineBuf[4096];
    int nCode = 0;

    // This is a dummy feature object used to store style properties
    // and the like. We end up deleting it without returning it
    OGRDXFFeature *poOverallFeature = new OGRDXFFeature( poFeatureDefn );

    DXFMLEADERLeader oLeader;
    std::vector<DXFMLEADERLeader> aoLeaders;

    std::vector<DXFMLEADERVertex> oLeaderLine;
    double dfCurrentX = 0.0;
    double dfCurrentY = 0.0;
    double dfCurrentX2 = 0.0;
    double dfCurrentY2 = 0.0;
    size_t nCurrentVertex = 0;

    double dfScale = 1.0;
    bool bHasDogleg = true;
    CPLString osLeaderColor = "0";

    CPLString osText;
    CPLString osTextStyleHandle;
    double dfTextX = 0.0;
    double dfTextY = 0.0;
    int nTextAlignment = 1; // 1 = left, 2 = center, 3 = right
    double dfTextAngle = 0.0;
    double dfTextHeight = 4.0;

    CPLString osBlockHandle;
    OGRDXFInsertTransformer oBlockTransformer;
    CPLString osBlockAttributeHandle;
    // Map of ATTDEF handles to attribute text
    std::map<CPLString, CPLString> oBlockAttributes;

    CPLString osArrowheadBlockHandle;
    double dfArrowheadSize = 4.0;

    // The different leader line types
    const int MLT_NONE = 0;
    const int MLT_STRAIGHT = 1;
    const int MLT_SPLINE = 2;
    int nLeaderLineType = MLT_STRAIGHT;

    // Group codes mean different things in different sections of the
    // MLEADER entity. We need to keep track of the section we are in.
    const int MLS_COMMON = 0;
    const int MLS_CONTEXT_DATA = 1;
    const int MLS_LEADER = 2;
    const int MLS_LEADER_LINE = 3;
    int nSection = MLS_COMMON;

    // The way the 30x group codes work is missing from the DXF docs.
    // We assume that the sections are always nested as follows:

    // ... [this part is identified as MLS_COMMON]
    // 300 CONTEXT_DATA{
    //   ...
    //   302 LEADER{
    //     ...
    //     304 LEADER_LINE{
    //       ...
    //     305 }
    //     304 LEADER_LINE{
    //       ...
    //     305 }
    //     ...
    //   303 }
    //   302 LEADER{
    //     ...
    //   303 }
    //   ...
    // 301 }
    // ... [MLS_COMMON]

    while( (nCode = poDS->ReadValue(szLineBuf,sizeof(szLineBuf))) > 0 )
    {
        switch( nSection )
        {
          case MLS_COMMON:
            switch( nCode )
            {
              case 300:
                nSection = MLS_CONTEXT_DATA;
                break;

              case 342:
                // 342 is the entity handle of the BLOCK_RECORD table entry that
                // corresponds to the arrowhead block.
                osArrowheadBlockHandle = szLineBuf;
                break;

              case 42:
                // TODO figure out difference between 42 and 140 for arrowheadsize
                dfArrowheadSize = CPLAtof( szLineBuf );
                break;

              case 330:
                osBlockAttributeHandle = szLineBuf;
                break;

              case 302:
                if( osBlockAttributeHandle != "" )
                {
                    oBlockAttributes[osBlockAttributeHandle] =
                        TextUnescape( szLineBuf, true );
                    osBlockAttributeHandle = "";
                }
                break;

              case 91:
                osLeaderColor = szLineBuf;
                break;

              case 170:
                nLeaderLineType = atoi(szLineBuf);
                break;

              case 291:
                bHasDogleg = atoi(szLineBuf) != 0;
                break;

              default:
                TranslateGenericProperty( poOverallFeature, nCode, szLineBuf );
                break;
            }
            break;

          case MLS_CONTEXT_DATA:
            switch( nCode )
            {
              case 301:
                nSection = MLS_COMMON;
                break;

              case 302:
                nSection = MLS_LEADER;
                break;

              case 304:
                osText = TextUnescape(szLineBuf, true);
                break;

              case 40:
                dfScale = CPLAtof( szLineBuf );
                break;

              case 340:
                // 340 is the entity handle of the STYLE table entry that
                // corresponds to the text style.
                osTextStyleHandle = szLineBuf;
                break;

              case 12:
                dfTextX = CPLAtof( szLineBuf );
                break;

              case 22:
                dfTextY = CPLAtof( szLineBuf );
                break;

              case 41:
                dfTextHeight = CPLAtof( szLineBuf );
                break;

              case 42:
                dfTextAngle = CPLAtof( szLineBuf ) * 180 / M_PI;
                break;

              case 171:
                nTextAlignment = atoi( szLineBuf );
                break;

              case 341:
                // 341 is the entity handle of the BLOCK_RECORD table entry that
                // corresponds to the block content of this MLEADER.
                osBlockHandle = szLineBuf;
                break;

              case 15:
                oBlockTransformer.dfXOffset = CPLAtof( szLineBuf );
                break;

              case 25:
                oBlockTransformer.dfYOffset = CPLAtof( szLineBuf );
                break;

              case 16:
                oBlockTransformer.dfXScale = CPLAtof( szLineBuf );
                break;

              case 26:
                oBlockTransformer.dfYScale = CPLAtof( szLineBuf );
                break;

              case 46:
                oBlockTransformer.dfAngle = CPLAtof( szLineBuf );
                break;
            }
            break;

          case MLS_LEADER:
            switch( nCode )
            {
              case 303:
                nSection = MLS_CONTEXT_DATA;
                aoLeaders.emplace_back( std::move(oLeader) );
                oLeader = DXFMLEADERLeader();
                break;

              case 304:
                nSection = MLS_LEADER_LINE;
                break;

              case 10:
                oLeader.dfLandingX = CPLAtof(szLineBuf);
                break;

              case 20:
                oLeader.dfLandingY = CPLAtof(szLineBuf);
                break;

              case 11:
                oLeader.dfDoglegVectorX = CPLAtof(szLineBuf);
                break;

              case 21:
                oLeader.dfDoglegVectorY = CPLAtof(szLineBuf);
                break;

              case 12:
                dfCurrentX = CPLAtof(szLineBuf);
                break;

              case 22:
                dfCurrentY = CPLAtof(szLineBuf);
                break;

              case 13:
                dfCurrentX2 = CPLAtof(szLineBuf);
                break;

              case 23:
                dfCurrentY2 = CPLAtof(szLineBuf);
                oLeader.aoDoglegBreaks.push_back( std::make_pair(
                    DXFTriple( dfCurrentX, dfCurrentY, 0.0 ),
                    DXFTriple( dfCurrentX2, dfCurrentY2, 0.0 ) ) );
                break;

              case 40:
                oLeader.dfDoglegLength = CPLAtof(szLineBuf);
                break;
            }
            break;

          case MLS_LEADER_LINE:
            switch( nCode )
            {
              case 305:
                nSection = MLS_LEADER;
                oLeader.aaoLeaderLines.emplace_back( std::move(oLeaderLine) );
                oLeaderLine = std::vector<DXFMLEADERVertex>();
                break;

              case 10:
                dfCurrentX = CPLAtof(szLineBuf);
                break;

              case 20:
                dfCurrentY = CPLAtof(szLineBuf);
                oLeaderLine.push_back(
                    DXFMLEADERVertex( dfCurrentX, dfCurrentY ) );
                break;

              case 90:
                nCurrentVertex = atoi(szLineBuf);
                if( nCurrentVertex >= oLeaderLine.size() )
                {
                    CPLError( CE_Warning, CPLE_AppDefined,
                        "Wrong group code 90 in LEADER_LINE: %s", szLineBuf );
                    DXF_LAYER_READER_ERROR();
                    delete poOverallFeature;
                    return nullptr;
                }
                break;

              case 11:
                dfCurrentX = CPLAtof(szLineBuf);
                break;

              case 21:
                dfCurrentY = CPLAtof(szLineBuf);
                break;

              case 12:
                dfCurrentX2 = CPLAtof(szLineBuf);
                break;

              case 22:
                if( nCurrentVertex >= oLeaderLine.size() )
                {
                    CPLError( CE_Warning, CPLE_AppDefined,
                        "Misplaced group code 22 in LEADER_LINE" );
                    DXF_LAYER_READER_ERROR();
                    delete poOverallFeature;
                    return nullptr;
                }
                dfCurrentY2 = CPLAtof(szLineBuf);
                oLeaderLine[nCurrentVertex].aoBreaks.push_back( std::make_pair(
                    DXFTriple( dfCurrentX, dfCurrentY, 0.0 ),
                    DXFTriple( dfCurrentX2, dfCurrentY2, 0.0 ) ) );
                break;
            }
            break;
        }
    }

    if( nCode < 0 )
    {
        DXF_LAYER_READER_ERROR();
        delete poOverallFeature;
        return nullptr;
    }
    if( nCode == 0 )
        poDS->UnreadValue();

    // Convert the block handle to a block name. If there is no block,
    // osBlockName will remain empty.
    CPLString osBlockName;

    if( osBlockHandle != "" )
        osBlockName = poDS->GetBlockNameByRecordHandle( osBlockHandle );

/* -------------------------------------------------------------------- */
/*      Add the landing and arrowhead onto each leader line, and add    */
/*      the dogleg, if present, onto the leader.                        */
/* -------------------------------------------------------------------- */
    OGRDXFFeature* poLeaderFeature = poOverallFeature->CloneDXFFeature();
    poLeaderFeature->oStyleProperties["Color"] = osLeaderColor;

    OGRMultiLineString *poMLS = new OGRMultiLineString();

    // Arrowheads should be the same color as the leader line. If the leader
    // line is ByBlock or ByLayer then the arrowhead should be "owned" by the
    // overall feature for styling purposes.
    OGRDXFFeature* poArrowheadOwningFeature = poLeaderFeature;
    if( ( atoi(osLeaderColor) & 0xC2000000 ) == 0xC0000000 )
        poArrowheadOwningFeature = poOverallFeature;

    for( std::vector<DXFMLEADERLeader>::iterator oIt = aoLeaders.begin();
         nLeaderLineType != MLT_NONE && oIt != aoLeaders.end();
         ++oIt )
    {
        const bool bLeaderHasDogleg = bHasDogleg &&
            nLeaderLineType != MLT_SPLINE &&
            oIt->dfDoglegLength != 0.0 &&
            ( oIt->dfDoglegVectorX != 0.0 || oIt->dfDoglegVectorY != 0.0 );

        // We assume that the dogleg vector in the DXF is a unit vector.
        // Safe assumption? Who knows. The documentation is so bad.
        const double dfDoglegX = oIt->dfLandingX +
            oIt->dfDoglegVectorX * oIt->dfDoglegLength;
        const double dfDoglegY = oIt->dfLandingY +
            oIt->dfDoglegVectorY * oIt->dfDoglegLength;

        // When the dogleg is turned off or we are in spline mode, it seems
        // that the dogleg and landing data are still present in the DXF file,
        // but they are not supposed to be drawn.
        if( !bHasDogleg || nLeaderLineType == MLT_SPLINE )
        {
            oIt->dfLandingX = dfDoglegX;
            oIt->dfLandingY = dfDoglegY;
        }

        // Iterate through each leader line
        for( const auto& aoLineVertices : oIt->aaoLeaderLines )
        {
            if( aoLineVertices.empty() )
                continue;

            OGRLineString* poLeaderLine = new OGRLineString();

            // Get the first line segment for arrowhead purposes
            poLeaderLine->addPoint(
                aoLineVertices[0].oCoords.dfX,
                aoLineVertices[0].oCoords.dfY );

            if( aoLineVertices.size() > 1 )
            {
                poLeaderLine->addPoint(
                    aoLineVertices[1].oCoords.dfX,
                    aoLineVertices[1].oCoords.dfY );
            }
            else
            {
                poLeaderLine->addPoint( oIt->dfLandingX, oIt->dfLandingY );
            }

            // Add an arrowhead if required
            InsertArrowhead( poArrowheadOwningFeature,
                osArrowheadBlockHandle, poLeaderLine,
                dfArrowheadSize * dfScale );

            poLeaderLine->setNumPoints( 1 );

            // Go through the vertices of the leader line, adding them,
            // as well as break start and end points, to the linestring.
            for( size_t iVertex = 0; iVertex < aoLineVertices.size();
                 iVertex++ )
            {
                if( iVertex > 0 )
                {
                    poLeaderLine->addPoint(
                        aoLineVertices[iVertex].oCoords.dfX,
                        aoLineVertices[iVertex].oCoords.dfY );
                }

                // Breaks are ignored for spline leaders
                if( nLeaderLineType != MLT_SPLINE )
                {
                    for( const auto& oBreak :
                         aoLineVertices[iVertex].aoBreaks )
                    {
                        poLeaderLine->addPoint( oBreak.first.dfX,
                            oBreak.first.dfY );

                        poMLS->addGeometryDirectly( poLeaderLine );
                        poLeaderLine = new OGRLineString();

                        poLeaderLine->addPoint( oBreak.second.dfX,
                            oBreak.second.dfY );
                    }
                }
            }

            // Add the final vertex (the landing) to the end of the line
            poLeaderLine->addPoint( oIt->dfLandingX, oIt->dfLandingY );

            // Make the spline geometry for spline leaders
            if( nLeaderLineType == MLT_SPLINE )
            {
                DXFTriple oEndTangent;
                if( osBlockName.empty() )
                {
                    oEndTangent = DXFTriple( oIt->dfDoglegVectorX,
                        oIt->dfDoglegVectorY, 0 );
                }
                InterpolateSpline( poLeaderLine, oEndTangent );
            }

            poMLS->addGeometryDirectly( poLeaderLine );
        }

        // Add the dogleg as a separate line in the MLS
        if( bLeaderHasDogleg )
        {
            OGRLineString *poDoglegLine = new OGRLineString();
            poDoglegLine->addPoint( oIt->dfLandingX, oIt->dfLandingY );

            // Interrupt the dogleg line at breaks
            for( const auto& oBreak : oIt->aoDoglegBreaks )
            {
                poDoglegLine->addPoint( oBreak.first.dfX,
                    oBreak.first.dfY );

                poMLS->addGeometryDirectly( poDoglegLine );
                poDoglegLine = new OGRLineString();

                poDoglegLine->addPoint( oBreak.second.dfX,
                    oBreak.second.dfY );
            }

            poDoglegLine->addPoint( dfDoglegX, dfDoglegY );
            poMLS->addGeometryDirectly( poDoglegLine );
        }
    }

    poLeaderFeature->SetGeometryDirectly( poMLS );

    PrepareLineStyle( poLeaderFeature, poOverallFeature );

/* -------------------------------------------------------------------- */
/*      If we have block content, insert that block.                    */
/* -------------------------------------------------------------------- */

    if( osBlockName != "" )
    {
        oBlockTransformer.dfXScale *= dfScale;
        oBlockTransformer.dfYScale *= dfScale;

        DXFBlockDefinition *poBlock = poDS->LookupBlock( osBlockName );

        std::map<OGRDXFFeature *, CPLString> oBlockAttributeValues;

        // If we have block attributes and will need to output them,
        // go through all the features on this block, looking for
        // ATTDEFs whose handle is in our list of attribute handles
        if( poBlock && !oBlockAttributes.empty() &&
            ( poDS->InlineBlocks() ||
            poOverallFeature->GetFieldIndex( "BlockAttributes" ) != -1 ) )
        {
            for( std::vector<OGRDXFFeature *>::iterator oIt =
                poBlock->apoFeatures.begin();
                oIt != poBlock->apoFeatures.end();
                ++oIt )
            {
                const char* pszHandle =
                    (*oIt)->GetFieldAsString( "EntityHandle" );

                if( pszHandle && oBlockAttributes.count( pszHandle ) > 0 )
                    oBlockAttributeValues[*oIt] = oBlockAttributes[pszHandle];
            }
        }

        OGRDXFFeature *poBlockFeature = poOverallFeature->CloneDXFFeature();

        // If not inlining the block, insert a reference and add attributes
        // to this feature.
        if( !poDS->InlineBlocks() )
        {
            poBlockFeature = InsertBlockReference( osBlockName,
                oBlockTransformer, poBlockFeature );

            if( !oBlockAttributes.empty() &&
                poOverallFeature->GetFieldIndex( "BlockAttributes" ) != -1 )
            {
                std::vector<char *> apszAttribs;

                for( std::map<OGRDXFFeature *, CPLString>::iterator oIt =
                    oBlockAttributeValues.begin();
                    oIt != oBlockAttributeValues.end();
                    ++oIt )
                {
                    // Store the attribute tag and the text value as
                    // a space-separated entry in the BlockAttributes field
                    CPLString osAttribString = oIt->first->osAttributeTag;
                    osAttribString += " ";
                    osAttribString += oIt->second;

                    apszAttribs.push_back(
                        new char[osAttribString.length() + 1] );
                    CPLStrlcpy( apszAttribs.back(), osAttribString.c_str(),
                        osAttribString.length() + 1 );
                }

                apszAttribs.push_back( nullptr );

                poBlockFeature->SetField( "BlockAttributes", &apszAttribs[0] );
            }

            apoPendingFeatures.push( poBlockFeature );
        }
        else
        {
            // Insert the block inline.
            OGRDXFFeatureQueue apoExtraFeatures;
            try
            {
                poBlockFeature = InsertBlockInline(
                    CPLGetErrorCounter(), osBlockName,
                    oBlockTransformer, poBlockFeature, apoExtraFeatures,
                    true, poDS->ShouldMergeBlockGeometries() );
            }
            catch( const std::invalid_argument& )
            {
                // Block doesn't exist
                delete poBlockFeature;
                poBlockFeature = nullptr;
            }

            // Add the block geometries to the pending feature stack.
            if( poBlockFeature )
            {
                apoPendingFeatures.push( poBlockFeature );
            }
            while( !apoExtraFeatures.empty() )
            {
                apoPendingFeatures.push( apoExtraFeatures.front() );
                apoExtraFeatures.pop();
            }

            // Also add any attributes to the pending feature stack.
            for( std::map<OGRDXFFeature *, CPLString>::iterator oIt =
                     oBlockAttributeValues.begin();
                 oIt != oBlockAttributeValues.end();
                 ++oIt )
            {
                OGRDXFFeature *poAttribFeature = oIt->first->CloneDXFFeature();

                poAttribFeature->SetField( "Text", oIt->second );

                // Replace text in the style string
                const char* poStyleString = poAttribFeature->GetStyleString();
                if( poStyleString && STARTS_WITH(poStyleString, "LABEL(") )
                {
                    CPLString osNewStyle = poStyleString;
                    const size_t nTextStartPos = osNewStyle.find( ",t:\"" );
                    if( nTextStartPos != std::string::npos )
                    {
                        size_t nTextEndPos = nTextStartPos + 4;
                        while( nTextEndPos < osNewStyle.size() &&
                            osNewStyle[nTextEndPos] != '\"' )
                        {
                            nTextEndPos++;
                            if( osNewStyle[nTextEndPos] == '\\' )
                                nTextEndPos++;
                        }

                        if( nTextEndPos < osNewStyle.size() )
                        {
                            osNewStyle.replace( nTextStartPos + 4,
                                nTextEndPos - ( nTextStartPos + 4 ),
                                oIt->second );
                            poAttribFeature->SetStyleString( osNewStyle );
                        }
                    }
                }

                // The following bits are copied from
                // OGRDXFLayer::InsertBlockInline
                if( poAttribFeature->GetGeometryRef() )
                {
                    poAttribFeature->GetGeometryRef()->transform(
                        &oBlockTransformer );
                }

                if( EQUAL( poAttribFeature->GetFieldAsString( "Layer" ), "0" ) &&
                    !EQUAL( poOverallFeature->GetFieldAsString( "Layer" ), "" ) )
                {
                    poAttribFeature->SetField( "Layer",
                        poOverallFeature->GetFieldAsString( "Layer" ) );
                }

                PrepareFeatureStyle( poAttribFeature, poOverallFeature );

                ACAdjustText( oBlockTransformer.dfAngle * 180 / M_PI,
                    oBlockTransformer.dfXScale, oBlockTransformer.dfYScale,
                    poAttribFeature );

                if ( !EQUAL( poOverallFeature->GetFieldAsString(
                    "EntityHandle" ), "" ) )
                {
                    poAttribFeature->SetField( "EntityHandle",
                        poOverallFeature->GetFieldAsString( "EntityHandle" ) );
                }

                apoPendingFeatures.push( poAttribFeature );
            }
        }
    }

/* -------------------------------------------------------------------- */
/*      Prepare a new feature to serve as the leader text label         */
/*      refeature.  We will push it onto the layer as a pending           */
/*      feature for the next feature read.                              */
/* -------------------------------------------------------------------- */

    if( osText.empty() || osText == " " )
    {
        delete poOverallFeature;
        return poLeaderFeature;
    }

    OGRDXFFeature *poLabelFeature = poOverallFeature->CloneDXFFeature();

    poLabelFeature->SetField( "Text", osText );
    poLabelFeature->SetGeometryDirectly( new OGRPoint( dfTextX, dfTextY ) );

    CPLString osStyle;
    char szBuffer[64];

    const CPLString osStyleName =
        poDS->GetTextStyleNameByHandle( osTextStyleHandle );

    // Font name
    osStyle.Printf("LABEL(f:\"");

    // Preserve legacy behavior of specifying "Arial" as a default font name.
    osStyle += poDS->LookupTextStyleProperty( osStyleName, "Font", "Arial" );

    osStyle += "\"";

    // Bold, italic
    if( EQUAL( poDS->LookupTextStyleProperty( osStyleName,
        "Bold", "0" ), "1" ) )
    {
        osStyle += ",bo:1";
    }
    if( EQUAL( poDS->LookupTextStyleProperty( osStyleName,
        "Italic", "0" ), "1" ) )
    {
        osStyle += ",it:1";
    }

    osStyle += CPLString().Printf(",t:\"%s\",p:%d", osText.c_str(),
        nTextAlignment + 6); // 7,8,9: vertical align top

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

    const char *pszWidthFactor = poDS->LookupTextStyleProperty( osStyleName,
        "Width", "1" );
    if( pszWidthFactor && CPLAtof( pszWidthFactor ) != 1.0 )
    {
        CPLsnprintf(szBuffer, sizeof(szBuffer), "%.4g",
            CPLAtof( pszWidthFactor ) * 100.0);
        osStyle += CPLString().Printf(",w:%s", szBuffer);
    }

    // Color
    osStyle += ",c:";
    osStyle += poLabelFeature->GetColor( poDS );

    osStyle += ")";

    poLabelFeature->SetStyleString( osStyle );

    apoPendingFeatures.push( poLabelFeature );

    delete poOverallFeature;
    return poLeaderFeature;
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
/*      Inserts the specified arrowhead block at the start of the       */
/*      first segment of the given line string (or the end of the       */
/*      last segment if bReverse is false).  2D only.                   */
/*                                                                      */
/*      The first (last) point of the line string may be updated.       */
/************************************************************************/
void OGRDXFLayer::InsertArrowhead( OGRDXFFeature* const poFeature,
    const CPLString& osBlockHandle, OGRLineString* const poLine,
    const double dfArrowheadSize, const bool bReverse /* = false */ )
{
    OGRPoint oPoint1, oPoint2;
    poLine->getPoint( bReverse ? poLine->getNumPoints() - 1 : 0, &oPoint1 );
    poLine->getPoint( bReverse ? poLine->getNumPoints() - 2 : 1, &oPoint2 );

    const double dfFirstSegmentLength = PointDist( oPoint1.getX(),
        oPoint1.getY(), oPoint2.getX(), oPoint2.getY() );

    // AutoCAD only displays an arrowhead if the length of the arrowhead
    // is less than or equal to half the length of the line segment
    if( dfArrowheadSize == 0.0 || dfFirstSegmentLength == 0.0 ||
        dfArrowheadSize > 0.5 * dfFirstSegmentLength )
    {
        return;
    }

    OGRDXFFeature *poArrowheadFeature = poFeature->CloneDXFFeature();

    // Convert the block handle to a block name.
    CPLString osBlockName = "";

    if( osBlockHandle != "" )
        osBlockName = poDS->GetBlockNameByRecordHandle( osBlockHandle );

    OGRDXFFeatureQueue apoExtraFeatures;

    // If the block doesn't exist, we need to fall back to the
    // default arrowhead.
    if( osBlockName == "" )
    {
        GenerateDefaultArrowhead( poArrowheadFeature, oPoint1, oPoint2,
            dfArrowheadSize / dfFirstSegmentLength );

        PrepareBrushStyle( poArrowheadFeature );
    }
    else
    {
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

        // Insert the block.
        try
        {
            poArrowheadFeature = InsertBlockInline(
                CPLGetErrorCounter(), osBlockName,
                oTransformer, poArrowheadFeature, apoExtraFeatures,
                true, false );
        }
        catch( const std::invalid_argument& )
        {
            // Supposedly the block doesn't exist. But what has probably
            // happened is that the block exists in the DXF, but it contains
            // no entities, so the data source didn't read it in.
            // In this case, no arrowhead is required.
            delete poArrowheadFeature;
            poArrowheadFeature = nullptr;
        }
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

    // Move the endpoint of the line out of the way of the arrowhead.
    // We assume that arrowheads are 1 unit long, except for a list
    // of specific block names which are treated as having no length

    static const char* apszSpecialArrowheads[] = {
        "_ArchTick",
        "_DotSmall",
        "_Integral",
        "_None",
        "_Oblique",
        "_Small"
    };

    if( std::find( apszSpecialArrowheads, apszSpecialArrowheads + 6,
        osBlockName ) == ( apszSpecialArrowheads + 6 ) )
    {
        oPoint1.setX( oPoint1.getX() + dfArrowheadSize *
            ( oPoint2.getX() - oPoint1.getX() ) / dfFirstSegmentLength );
        oPoint1.setY( oPoint1.getY() + dfArrowheadSize *
            ( oPoint2.getY() - oPoint1.getY() ) / dfFirstSegmentLength );

        poLine->setPoint( bReverse ? poLine->getNumPoints() - 1 : 0,
            &oPoint1 );
    }
}

/************************************************************************/
/*                        basis(), rbspline2()                          */
/*                                                                      */
/*      Spline calculation functions defined in intronurbs.cpp.         */
/************************************************************************/
void basis( int c, double t, int npts, double x[], double N[] );
void rbspline2( int npts,int k,int p1,double b[],double h[],
    bool bCalculateKnots, double x[], double p[] );


#if defined(__GNUC__) && __GNUC__ >= 6
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wnull-dereference"
#endif

namespace {
    inline void setRow(GDALMatrix & m, int row, DXFTriple const & t)
    {
        m(row, 0) = t.dfX;
        m(row, 1) = t.dfY;
        m(row, 2) = t.dfZ;
    }
}

/************************************************************************/
/*                      GetBSplineControlPoints()                       */
/*                                                                      */
/*      Evaluates the control points for the B-spline of given degree   */
/*      that interpolates the given data points, using the given        */
/*      parameters, start tangent and end tangent.  The parameters      */
/*      and knot vector must be increasing sequences with first         */
/*      element 0 and last element 1.  Given n data points, there       */
/*      must be n parameters and n + nDegree + 3 knots.                 */
/*                                                                      */
/*      It is recommended to match AutoCAD by generating a knot         */
/*      vector from the parameters as follows:                          */
/*              0 0 ... 0 adfParameters 1 1 ... 1                       */
/*        (nDegree zeros)               (nDegree ones)                  */
/*      To fully match AutoCAD's behavior, a chord-length              */
/*      parameterisation should be used, and the start and end          */
/*      tangent vectors should be multiplied by the total chord         */
/*      length of all chords.                                           */
/*                                                                      */
/*      Reference: Piegl, L., Tiller, W. (1995), The NURBS Book,        */
/*      2nd ed. (Springer), sections 2.2 and 9.2.                       */
/*      Although this book contains implementations of algorithms,      */
/*      this function is an original implementation based on the        */
/*      concepts discussed in the book and was written without          */
/*      reference to Piegl and Tiller's implementations.                */
/************************************************************************/
static std::vector<DXFTriple> GetBSplineControlPoints(
    const std::vector<double>& adfParameters,
    const std::vector<double>& adfKnots,
    const std::vector<DXFTriple>& aoDataPoints, const int nDegree,
    DXFTriple oStartTangent, DXFTriple oEndTangent )
{
    CPLAssert( nDegree > 1 );

    // Count the number of data points
    // Note: The literature often sets n to one less than the number of data
    // points for some reason, but we don't do that here
    const int nPoints = static_cast<int>( aoDataPoints.size() );

    CPLAssert( nPoints > 0 );
    CPLAssert( nPoints == static_cast<int>( adfParameters.size() ) );

    // RAM consumption is quadratic in the number of control points.
    if( nPoints > atoi(CPLGetConfigOption(
                        "DXF_MAX_BSPLINE_CONTROL_POINTS", "2000")) )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Too many control points (%d) for spline leader. "
                 "Set DXF_MAX_BSPLINE_CONTROL_POINTS configuration "
                 "option to a higher value to remove this limitation "
                 "(at the cost of significant RAM consumption)", nPoints);
        return std::vector<DXFTriple>();
    }

    // We want to solve the linear system NP=D for P, where N is a coefficient
    // matrix made up of values of the basis functions at each parameter
    // value, with two additional rows for the endpoint tangent information.
    // Each row relates to a different parameter.

    // Set up D as a matrix consisting initially of the data points
    GDALMatrix D(nPoints + 2, 3);

    setRow(D, 0, aoDataPoints[0]);
    for( int iIndex = 1; iIndex < nPoints - 1; iIndex++ )
        setRow(D, iIndex + 1, aoDataPoints[iIndex]);
    setRow(D, nPoints + 1, aoDataPoints[nPoints - 1]);


    const double dfStartMultiplier = adfKnots[nDegree + 1] / nDegree;
    oStartTangent *= dfStartMultiplier;
    setRow(D, 1, oStartTangent);

    const double dfEndMultiplier = ( 1.0 - adfKnots[nPoints + 1] ) / nDegree;
    oEndTangent *= dfEndMultiplier;
    setRow(D, nPoints, oEndTangent);

    GDALMatrix N(nPoints + 2, nPoints + 2);
    // First control point will be the first data point
    N(0,0) = 1.0;

    // Start tangent determines the second control point
    N(1,0) = -1.0;
    N(1,1) = 1.0;

    // Fill the middle rows of the matrix with basis function values. We
    // have to use a temporary vector, because intronurbs' basis function
    // requires an additional nDegree entries for temporary storage.
    std::vector<double> adfTempRow( nPoints + 2 + nDegree, 0.0 );
    for( int iRow = 2; iRow < nPoints; iRow++ )
    {
        basis( nDegree + 1, adfParameters[iRow - 1], nPoints + 2,
            const_cast<double *>( &adfKnots[0] ) - 1, &adfTempRow[0] - 1 );
        for(int iCol = 0; iCol < nPoints + 2; ++iCol)
            N(iRow, iCol) = adfTempRow[iCol];
    }

    // End tangent determines the second-last control point
    N(nPoints,nPoints) = -1.0;
    N(nPoints,nPoints + 1) = 1.0;

    // Last control point will be the last data point
    N(nPoints + 1,nPoints + 1) = 1.0;

    // Solve the linear system
    GDALMatrix P(nPoints + 2, 3);
    GDALLinearSystemSolve( N, D, P );

    std::vector<DXFTriple> aoControlPoints( nPoints + 2 );
    for( int iRow = 0; iRow < nPoints + 2; iRow++ )
    {
        aoControlPoints[iRow].dfX = P(iRow, 0);
        aoControlPoints[iRow].dfY = P(iRow, 1);
        aoControlPoints[iRow].dfZ = P(iRow, 2);
    }

    return aoControlPoints;
}

#if defined(__GNUC__) && __GNUC__ >= 6
#pragma GCC diagnostic pop
#endif


/************************************************************************/
/*                         InterpolateSpline()                          */
/*                                                                      */
/*      Interpolates a cubic spline between the data points of the      */
/*      given line string. The line string is updated with the new      */
/*      spline geometry.                                                */
/*                                                                      */
/*      If an end tangent of (0,0,0) is given, the direction vector     */
/*      of the last chord (line segment) is used.                       */
/************************************************************************/
static void InterpolateSpline( OGRLineString* const poLine,
    const DXFTriple& oEndTangentDirection )
{
    int nDataPoints = static_cast<int>( poLine->getNumPoints() );
    if ( nDataPoints < 2 )
        return;

    // Transfer line vertices into DXFTriple objects
    std::vector<DXFTriple> aoDataPoints;
    OGRPoint oPrevPoint;
    for( int iIndex = 0; iIndex < nDataPoints; iIndex++ )
    {
        OGRPoint oPoint;
        poLine->getPoint( iIndex, &oPoint );

        // Remove sequential duplicate points
        if( iIndex > 0 && oPrevPoint.Equals( &oPoint ) )
            continue;

        aoDataPoints.push_back( DXFTriple( oPoint.getX(), oPoint.getY(),
            oPoint.getZ() ) );
        oPrevPoint = oPoint;
    }
    nDataPoints = static_cast<int>( aoDataPoints.size() );
    if( nDataPoints < 2 )
        return;

    // Work out the chord length parameterisation
    std::vector<double> adfParameters;
    adfParameters.push_back( 0.0 );
    for( int iIndex = 1; iIndex < nDataPoints; iIndex++ )
    {
        const double dfParameter = adfParameters[iIndex - 1] +
            PointDist( aoDataPoints[iIndex - 1].dfX,
                aoDataPoints[iIndex - 1].dfY,
                aoDataPoints[iIndex - 1].dfZ,
                aoDataPoints[iIndex].dfX,
                aoDataPoints[iIndex].dfY,
                aoDataPoints[iIndex].dfZ );

        // Bail out in pathological cases. This will happen when
        // some lengths are very large (above 10^16) and others are
        // very small (such as 1)
        if( dfParameter == adfParameters[iIndex - 1] )
            return;

        adfParameters.push_back( dfParameter );
    }

    const double dfTotalChordLength = adfParameters[adfParameters.size() - 1];

    // Start tangent can be worked out from the first chord
    DXFTriple oStartTangent( aoDataPoints[1].dfX - aoDataPoints[0].dfX,
        aoDataPoints[1].dfY - aoDataPoints[0].dfY,
        aoDataPoints[1].dfZ - aoDataPoints[0].dfZ );
    oStartTangent *= dfTotalChordLength / adfParameters[1];

    // If end tangent is zero, it is worked out from the last chord
    DXFTriple oEndTangent = oEndTangentDirection;
    if( oEndTangent.dfX == 0.0 && oEndTangent.dfY == 0.0 &&
        oEndTangent.dfZ == 0.0 )
    {
        oEndTangent = DXFTriple(
            aoDataPoints[nDataPoints - 1].dfX -
                aoDataPoints[nDataPoints - 2].dfX,
            aoDataPoints[nDataPoints - 1].dfY -
                aoDataPoints[nDataPoints - 2].dfY,
            aoDataPoints[nDataPoints - 1].dfZ -
                aoDataPoints[nDataPoints - 2].dfZ );
        oEndTangent /= dfTotalChordLength - adfParameters[nDataPoints - 2];
    }

    // End tangent direction is multiplied by total chord length
    oEndTangent *= dfTotalChordLength;

    // Normalise the parameter vector
    for( int iIndex = 1; iIndex < nDataPoints; iIndex++ )
        adfParameters[iIndex] /= dfTotalChordLength;

    // Generate a knot vector
    const int nDegree = 3;
    std::vector<double> adfKnots( aoDataPoints.size() + nDegree + 3, 0.0 );
    std::copy( adfParameters.begin(), adfParameters.end(),
        adfKnots.begin() + nDegree );
    std::fill( adfKnots.end() - nDegree, adfKnots.end(), 1.0 );

    // Calculate the spline control points
    std::vector<DXFTriple> aoControlPoints = GetBSplineControlPoints(
        adfParameters, adfKnots, aoDataPoints, nDegree,
        oStartTangent, oEndTangent );
    const int nControlPoints = static_cast<int>( aoControlPoints.size() );

    if( nControlPoints == 0 )
        return;

    // Interpolate the spline using the intronurbs code
    int nWantedPoints = nControlPoints * 8;
    std::vector<double> adfWeights( nControlPoints, 1.0 );
    std::vector<double> adfPoints( 3 * nWantedPoints, 0.0 );

    rbspline2( nControlPoints, nDegree + 1, nWantedPoints,
        reinterpret_cast<double*>( &aoControlPoints[0] ) - 1,
        &adfWeights[0] - 1, false, &adfKnots[0] - 1, &adfPoints[0] - 1 );

    // Preserve 2D/3D status as we add the interpolated points to the line
    const int bIs3D = poLine->Is3D();
    poLine->empty();
    for( int iIndex = 0; iIndex < nWantedPoints; iIndex++ )
    {
        poLine->addPoint( adfPoints[iIndex * 3], adfPoints[iIndex * 3 + 1],
            adfPoints[iIndex * 3 + 2] );
    }
    if( !bIs3D )
        poLine->flattenTo2D();
}
