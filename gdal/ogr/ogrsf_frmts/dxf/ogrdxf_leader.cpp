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
/*                          DXFMLEADERLeader                            */
/************************************************************************/

struct DXFMLEADERLeader {
    double                       dfLandingX;
    double                       dfLandingY;
    double                       dfDoglegVectorX;
    double                       dfDoglegVectorY;
    double                       dfDoglegLength;
    std::vector<OGRLineString *> apoLeaderLines;
};

/************************************************************************/
/*                         TranslateMLEADER()                           */
/************************************************************************/

OGRDXFFeature *OGRDXFLayer::TranslateMLEADER()

{
    char szLineBuf[257];
    int nCode = 0;

    // This is a dummy feature object used to store style properties
    // and the like. We end up deleting it without returning it
    OGRDXFFeature *poOverallFeature = new OGRDXFFeature( poFeatureDefn );

    DXFMLEADERLeader oLeader;
    std::vector<DXFMLEADERLeader> aoLeaders;

    OGRLineString *poLine = NULL;
    bool bHaveX = false;
    bool bHaveY = false;
    double dfCurrentX = 0.0;
    double dfCurrentY = 0.0;

    double dfScale = 1.0;
    int nLeaderLineType = 1; // 0 = none, 1 = straight, 2 = spline
    bool bHasDogleg = true;
    CPLString osLeaderColor = "0";

    CPLString osText;
    CPLString osTextStyleHandle;
    double dfTextX = 0.0;
    double dfTextY = 0.0;
    int iTextAlignment = 1; // 1 = left, 2 = center, 3 = right
    double dfTextAngle = 0.0;
    double dfTextHeight = 4.0;

    CPLString osBlockHandle;
    OGRDXFInsertTransformer oBlockTransformer;
    CPLString osBlockAttributeHandle;
    // Map of ATTDEF handles to attribute text
    std::map<CPLString, CPLString> oBlockAttributes;

    CPLString osArrowheadBlockHandle;
    double dfArrowheadSize = 4.0;

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
                    oBlockAttributes[osBlockAttributeHandle] = szLineBuf;
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
                iTextAlignment = atoi( szLineBuf );
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
                aoLeaders.push_back( oLeader );
                oLeader = DXFMLEADERLeader();
                break;

              case 304:
                nSection = MLS_LEADER_LINE;
                poLine = new OGRLineString();
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
                if( bHaveX && bHaveY && poLine )
                {
                    poLine->addPoint( dfCurrentX, dfCurrentY );
                    bHaveX = bHaveY = false;
                }
                oLeader.apoLeaderLines.push_back( poLine );
                poLine = NULL;
                break;

              case 10:
                // add the previous point onto the linestring
                if( bHaveX && bHaveY )
                {
                    poLine->addPoint( dfCurrentX, dfCurrentY );
                    bHaveY = false;
                }
                dfCurrentX = CPLAtof(szLineBuf);
                bHaveX = true;
                break;

              case 20:
                // add the previous point onto the linestring
                if( bHaveX && bHaveY )
                {
                    poLine->addPoint( dfCurrentX, dfCurrentY );
                    bHaveX = false;
                }
                dfCurrentY = CPLAtof(szLineBuf);
                bHaveY = true;
                break;
            }
            break;
        }
    }

    if( poLine )
    {
        delete poLine;
        poLine = NULL;
    }

    // delete any lines left in a stray, unclosed LEADER{...} group
    while( !oLeader.apoLeaderLines.empty() )
    {
        delete oLeader.apoLeaderLines.back();
        oLeader.apoLeaderLines.pop_back();
    }

    // if we don't need any leaders, delete them
    if( nCode < 0 || nLeaderLineType == 0 )
    {
        while( !aoLeaders.empty() )
        {
            std::vector<OGRLineString *>& apoLeaderLines =
                aoLeaders.back().apoLeaderLines;
            while( !apoLeaderLines.empty() )
            {
                delete apoLeaderLines.back();
                apoLeaderLines.pop_back();
            }
            aoLeaders.pop_back();
        }
    }

    if( nCode < 0 )
    {
        DXF_LAYER_READER_ERROR();
        delete poOverallFeature;
        return NULL;
    }
    if( nCode == 0 )
        poDS->UnreadValue();

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
         nLeaderLineType != 0 && oIt != aoLeaders.end();
         ++oIt )
    {
        const bool bLeaderHasDogleg = bHasDogleg &&
            oIt->dfDoglegLength != 0.0 &&
            ( oIt->dfDoglegVectorX != 0.0 || oIt->dfDoglegVectorY != 0.0 );

        // We assume that the dogleg vector in the DXF is a unit vector.
        // Safe assumption? Who knows. The documentation is so bad.
        const double dfDoglegX = oIt->dfLandingX +
            oIt->dfDoglegVectorX * oIt->dfDoglegLength;
        const double dfDoglegY = oIt->dfLandingY +
            oIt->dfDoglegVectorY * oIt->dfDoglegLength;

        // When the dogleg is turned off, it seems that the dogleg and landing
        // data are still present in the DXF file, but they are not supposed
        // to be drawn.
        if( !bHasDogleg )
        {
            oIt->dfLandingX = dfDoglegX;
            oIt->dfLandingY = dfDoglegY;
        }

        // If there is only one leader line, add the dogleg directly onto it
        if( oIt->apoLeaderLines.size() == 1 )
        {
            OGRLineString *poLeaderLine = oIt->apoLeaderLines.front();

            poLeaderLine->addPoint( oIt->dfLandingX, oIt->dfLandingY );
            if( bLeaderHasDogleg )
                poLeaderLine->addPoint( dfDoglegX, dfDoglegY );

            poMLS->addGeometryDirectly( poLeaderLine );
        }
        // Otherwise, add the dogleg as a separate line in the MLS
        else
        {
            for( std::vector<OGRLineString *>::iterator oLineIt =
                     oIt->apoLeaderLines.begin();
                 oLineIt != oIt->apoLeaderLines.end();
                 ++oLineIt )
            {
                OGRLineString *poLeaderLine = *oLineIt;
                poLeaderLine->addPoint( oIt->dfLandingX, oIt->dfLandingY );
                poMLS->addGeometryDirectly( poLeaderLine );
            }

            if( bLeaderHasDogleg )
            {
                OGRLineString *poDoglegLine = new OGRLineString();
                poDoglegLine->addPoint( oIt->dfLandingX, oIt->dfLandingY );
                poDoglegLine->addPoint( dfDoglegX, dfDoglegY );
                poMLS->addGeometryDirectly( poDoglegLine );
            }
        }

        // Add arrowheads where required
        for( std::vector<OGRLineString *>::iterator oLineIt =
                 oIt->apoLeaderLines.begin();
             oLineIt != oIt->apoLeaderLines.end();
             ++oLineIt )
        {
            OGRLineString *poLeaderLine = *oLineIt;

            if( poLeaderLine->getNumPoints() >= 2 )
            {
                OGRPoint oPoint1, oPoint2;
                poLeaderLine->getPoint( 0, &oPoint1 );
                poLeaderLine->getPoint( 1, &oPoint2 );

                InsertArrowhead( poArrowheadOwningFeature,
                    osArrowheadBlockHandle, oPoint1, oPoint2,
                    dfArrowheadSize * dfScale );
            }
        }
    }

    poLeaderFeature->SetGeometryDirectly( poMLS );

    PrepareLineStyle( poLeaderFeature, poOverallFeature );

/* -------------------------------------------------------------------- */
/*      If we have block content, insert that block.                    */
/* -------------------------------------------------------------------- */

    // Convert the block handle to a block name.
    CPLString osBlockName;

    if( osBlockHandle != "" )
        osBlockName = poDS->GetBlockNameByRecordHandle( osBlockHandle );

    if( osBlockName != "" )
    {
        oBlockTransformer.dfXScale *= dfScale;
        oBlockTransformer.dfYScale *= dfScale;

        std::map<OGRDXFFeature *, CPLString> oBlockAttributeValues;

        // If we have block attributes and will need to output them,
        // go through all the features on this block, looking for
        // ATTDEFs whose handle is in our list of attribute handles
        if( !oBlockAttributes.empty() &&
            ( poDS->InlineBlocks() ||
            poOverallFeature->GetFieldIndex( "BlockAttributes" ) != -1 ) )
        {
            DXFBlockDefinition *poBlock = poDS->LookupBlock( osBlockName );

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

                apszAttribs.push_back( NULL );

                poBlockFeature->SetField( "BlockAttributes", &apszAttribs[0] );
            }

            apoPendingFeatures.push( poBlockFeature );
        }
        else
        {
            // Insert the block inline.
            std::queue<OGRDXFFeature *> apoExtraFeatures;
            try
            {
                poBlockFeature = InsertBlockInline( osBlockName,
                    oBlockTransformer, poBlockFeature, apoExtraFeatures,
                    true, poDS->ShouldMergeBlockGeometries() );
            }
            catch( const std::invalid_argument& )
            {
                // Block doesn't exist
                delete poBlockFeature;
                poBlockFeature = NULL;
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
                CPLString osNewStyle = poAttribFeature->GetStyleString();
                const size_t nTextStartPos = osNewStyle.rfind( ",t:\"" );
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
                            nTextEndPos - ( nTextStartPos + 4 ), oIt->second );
                        poAttribFeature->SetStyleString( osNewStyle );
                    }
                }

                // The following bits are copied from
                // OGRDXFLayer::InsertBlockInline
                poAttribFeature->GetGeometryRef()->transform(
                    &oBlockTransformer );

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

    // Preserve legacy behaviour of specifying "Arial" as a default font name.
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
