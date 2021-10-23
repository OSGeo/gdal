/*******************************************************************************
 *  Project: OGR CAD Driver
 *  Purpose: Implements driver based on libopencad
 *  Author: Alexandr Borzykh, mush3d at gmail.com
 *  Author: Dmitry Baryshnikov, polimax@mail.ru
 *  Language: C++
 *******************************************************************************
 *  The MIT License (MIT)
 *
 *  Copyright (c) 2016 Alexandr Borzykh
 *  Copyright (c) 2016-2019, NextGIS <info@nextgis.com>
 *
 *  Permission is hereby granted, free of charge, to any person obtaining a copy
 *  of this software and associated documentation files (the "Software"), to deal
 *  in the Software without restriction, including without limitation the rights
 *  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 *  copies of the Software, and to permit persons to whom the Software is
 *  furnished to do so, subject to the following conditions:
 *
 *  The above copyright notice and this permission notice shall be included in all
 *  copies or substantial portions of the Software.
 *
 *  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 *  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 *  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 *  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 *  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 *  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 *  SOFTWARE.
 *******************************************************************************/
#include "cpl_conv.h"
#include "ogr_cad.h"

#include <iomanip>
#include <sstream>

#define FIELD_NAME_GEOMTYPE "cadgeom_type"
#define FIELD_NAME_THICKNESS "thickness"
#define FIELD_NAME_COLOR "color"
#define FIELD_NAME_EXT_DATA "extentity_data"
#define FIELD_NAME_TEXT "text"

constexpr double DEG2RAD = M_PI / 180.0;
constexpr double RAD2DEG = 1.0 / DEG2RAD;

OGRCADLayer::OGRCADLayer( CADLayer &poCADLayer_, OGRSpatialReference *poSR,
                          int nEncoding) :
                          poSpatialRef( poSR ),
                          poCADLayer( poCADLayer_ ),
                          nDWGEncoding( nEncoding)
{
    nNextFID = 0;

    if( poSpatialRef )
        poSpatialRef->Reference();
    poFeatureDefn = new OGRFeatureDefn( CADRecode( poCADLayer_.getName(), nDWGEncoding ) );

    // Setting up layer geometry type
    OGRwkbGeometryType eGeomType;
    char dLineStringPresented = 0;
    char dCircularStringPresented = 0;
    char dPointPresented = 0;
    char dPolygonPresented = 0;
    std::vector<CADObject::ObjectType> aePresentedGeometryTypes =
                                                    poCADLayer.getGeometryTypes();
    for( size_t i = 0; i < aePresentedGeometryTypes.size(); ++i )
    {
        switch( aePresentedGeometryTypes[i] )
        {
        case CADObject::ATTDEF:
        case CADObject::TEXT:
        case CADObject::MTEXT:
        case CADObject::POINT:
            dPointPresented = 1;
            break;
        case CADObject::CIRCLE:
            dCircularStringPresented = 1;
            break;
        case CADObject::SPLINE:
        case CADObject::ELLIPSE:
        case CADObject::ARC:
        case CADObject::POLYLINE3D:
        case CADObject::POLYLINE2D:
        case CADObject::LWPOLYLINE:
        case CADObject::LINE:
            dLineStringPresented = 1;
            break;
        case CADObject::FACE3D:
        case CADObject::SOLID:
            dPolygonPresented = 1;
            break;
        default:
            break;
        }
    }

    if( ( dLineStringPresented +
          dCircularStringPresented +
          dPointPresented +
          dPolygonPresented ) > 1 )
    {
        eGeomType = wkbGeometryCollection;
    }
    else
    {
        if( dLineStringPresented )
        {
            eGeomType = wkbLineString;
        }
        else if( dCircularStringPresented )
        {
            eGeomType = wkbCircularString;
        }
        else if( dPointPresented )
        {
            eGeomType = wkbPoint;
        }
        else if( dPolygonPresented )
        {
            eGeomType = wkbPolygon;
        }
        else
        {
            eGeomType = wkbUnknown;
        }
    }
    poFeatureDefn->SetGeomType( eGeomType );

    OGRFieldDefn  oClassField( FIELD_NAME_GEOMTYPE, OFTString );
    poFeatureDefn->AddFieldDefn( &oClassField );

    OGRFieldDefn  oLinetypeField( FIELD_NAME_THICKNESS, OFTReal );
    poFeatureDefn->AddFieldDefn( &oLinetypeField );

    OGRFieldDefn  oColorField( FIELD_NAME_COLOR, OFTString );
    poFeatureDefn->AddFieldDefn( &oColorField );

    OGRFieldDefn  oExtendedField( FIELD_NAME_EXT_DATA, OFTString );
    poFeatureDefn->AddFieldDefn( &oExtendedField );

    OGRFieldDefn  oTextField( FIELD_NAME_TEXT, OFTString );
    poFeatureDefn->AddFieldDefn( &oTextField );

    auto oAttrTags = poCADLayer.getAttributesTags();
    for( const std::string & osTag : oAttrTags )
    {
        auto ret = asFeaturesAttributes.insert( osTag );
        if( ret.second == true)
        {
            OGRFieldDefn oAttrField( osTag.c_str(), OFTString );
            poFeatureDefn->AddFieldDefn( &oAttrField );
        }
    }

    // Applying spatial ref info
    if (poFeatureDefn->GetGeomFieldCount() != 0)
        poFeatureDefn->GetGeomFieldDefn(0)->SetSpatialRef( poSpatialRef );

    SetDescription( poFeatureDefn->GetName() );
    poFeatureDefn->Reference();
}

GIntBig OGRCADLayer::GetFeatureCount( int bForce )
{
    if( m_poFilterGeom != nullptr || m_poAttrQuery != nullptr )
        return OGRLayer::GetFeatureCount( bForce );

    return poCADLayer.getGeometryCount();
}

OGRCADLayer::~OGRCADLayer()
{
    if( poSpatialRef )
        poSpatialRef->Release();
    poFeatureDefn->Release();
}

void OGRCADLayer::ResetReading()
{
    nNextFID = 0;
}

OGRFeature *OGRCADLayer::GetNextFeature()
{
    OGRFeature *poFeature = GetFeature( nNextFID );
    ++nNextFID;

    if( poFeature == nullptr )
        return nullptr;

    if( ( m_poFilterGeom == nullptr ||  FilterGeometry( poFeature->GetGeometryRef() ) )
        && ( m_poAttrQuery == nullptr || m_poAttrQuery->Evaluate( poFeature ) ) )
    {
        return poFeature;
    }

    return nullptr;
}

OGRFeature *OGRCADLayer::GetFeature( GIntBig nFID )
{
    if( poCADLayer.getGeometryCount() <= static_cast<size_t>(nFID)
        || nFID < 0 )
    {
        return nullptr;
    }

    OGRFeature  *poFeature = nullptr;
    CADGeometry *poCADGeometry = poCADLayer.getGeometry( static_cast<size_t>(nFID) );

    if( nullptr == poCADGeometry || GetLastErrorCode() != CADErrorCodes::SUCCESS )
    {
        CPLError( CE_Failure, CPLE_NotSupported,
                 "Failed to get geometry with ID = " CPL_FRMT_GIB " from layer \"%s\". Libopencad errorcode: %d",
                 nFID, poCADLayer.getName().c_str(), GetLastErrorCode() );
        return nullptr;
    }

    poFeature = new OGRFeature( poFeatureDefn );
    poFeature->SetFID( nFID );
    poFeature->SetField( FIELD_NAME_THICKNESS, poCADGeometry->getThickness() );

    if( !poCADGeometry->getEED().empty() )
    {
        std::vector<std::string> asGeometryEED = poCADGeometry->getEED();
        std::string sEEDAsOneString = "";
        for ( std::vector<std::string>::const_iterator
              iter = asGeometryEED.cbegin();
              iter != asGeometryEED.cend(); ++iter )
        {
            sEEDAsOneString += *iter;
            sEEDAsOneString += ' ';
        }

        poFeature->SetField( FIELD_NAME_EXT_DATA, sEEDAsOneString.c_str() );
    }

    RGBColor stRGB = poCADGeometry->getColor();
    CPLString sHexColor;
    sHexColor.Printf("#%02X%02X%02X%02X", stRGB.R, stRGB.G, stRGB.B, 255);
    poFeature->SetField( FIELD_NAME_COLOR, sHexColor );

    CPLString sStyle;
    sStyle.Printf("PEN(c:%s,w:5px)", sHexColor.c_str());
    poFeature->SetStyleString( sStyle );

    std::vector< CADAttrib > oBlockAttrs = poCADGeometry->getBlockAttributes();
    for( const CADAttrib& oAttrib : oBlockAttrs )
    {
        CPLString osTag = oAttrib.getTag();
        auto featureAttrIt = asFeaturesAttributes.find( osTag );
        if( featureAttrIt != asFeaturesAttributes.end())
        {
            poFeature->SetField(*featureAttrIt, oAttrib.getTextValue().c_str());
        }
    }

    switch( poCADGeometry->getType() )
    {
        case CADGeometry::POINT:
        {
            CADPoint3D * const poCADPoint = ( CADPoint3D* ) poCADGeometry;
            CADVector stPositionVector = poCADPoint->getPosition();

            poFeature->SetGeometryDirectly( new OGRPoint( stPositionVector.getX(),
                                                          stPositionVector.getY(),
                                                          stPositionVector.getZ() ) );
            poFeature->SetField( FIELD_NAME_GEOMTYPE, "CADPoint" );
            break;
        }

        case CADGeometry::LINE:
        {
            CADLine * const poCADLine = ( CADLine* ) poCADGeometry;
            OGRLineString *poLS = new OGRLineString();
            poLS->addPoint( poCADLine->getStart().getPosition().getX(),
                           poCADLine->getStart().getPosition().getY(),
                           poCADLine->getStart().getPosition().getZ() );
            poLS->addPoint( poCADLine->getEnd().getPosition().getX(),
                           poCADLine->getEnd().getPosition().getY(),
                           poCADLine->getEnd().getPosition().getZ() );

            poFeature->SetGeometryDirectly( poLS );
            poFeature->SetField( FIELD_NAME_GEOMTYPE, "CADLine" );
            break;
        }

        case CADGeometry::SOLID:
        {
            CADSolid * const poCADSolid = ( CADSolid* ) poCADGeometry;
            OGRPolygon * poPoly = new OGRPolygon();
            OGRLinearRing * poLR = new OGRLinearRing();

            std::vector<CADVector> astSolidCorners = poCADSolid->getCorners();
            for( size_t i = 0; i < astSolidCorners.size(); ++i )
            {
                poLR->addPoint( astSolidCorners[i].getX(),
                                astSolidCorners[i].getY(),
                                astSolidCorners[i].getZ());
            }
            poPoly->addRingDirectly( poLR );
            poPoly->closeRings();
            poFeature->SetGeometryDirectly( poPoly );

            poFeature->SetField( FIELD_NAME_GEOMTYPE, "CADSolid" );
            break;
        }

        case CADGeometry::CIRCLE:
        {
            CADCircle * poCADCircle = static_cast<CADCircle*>(poCADGeometry);
            OGRCircularString * poCircle = new OGRCircularString();

            CADVector stCircleCenter = poCADCircle->getPosition();
            OGRPoint  oCirclePoint1;
            oCirclePoint1.setX( stCircleCenter.getX() - poCADCircle->getRadius() );
            oCirclePoint1.setY( stCircleCenter.getY() );
            oCirclePoint1.setZ( stCircleCenter.getZ() );
            poCircle->addPoint( &oCirclePoint1 );

            OGRPoint  oCirclePoint2;
            oCirclePoint2.setX( stCircleCenter.getX() );
            oCirclePoint2.setY( stCircleCenter.getY() + poCADCircle->getRadius() );
            oCirclePoint2.setZ( stCircleCenter.getZ() );
            poCircle->addPoint( &oCirclePoint2 );

            OGRPoint  oCirclePoint3;
            oCirclePoint3.setX( stCircleCenter.getX() + poCADCircle->getRadius() );
            oCirclePoint3.setY( stCircleCenter.getY() );
            oCirclePoint3.setZ( stCircleCenter.getZ() );
            poCircle->addPoint( &oCirclePoint3 );

            OGRPoint  oCirclePoint4;
            oCirclePoint4.setX( stCircleCenter.getX() );
            oCirclePoint4.setY( stCircleCenter.getY() - poCADCircle->getRadius() );
            oCirclePoint4.setZ( stCircleCenter.getZ() );
            poCircle->addPoint( &oCirclePoint4 );

            // Close the circle
            poCircle->addPoint( &oCirclePoint1 );

            /*NOTE: The alternative way:
                    OGRGeometry *poCircle = OGRGeometryFactory::approximateArcAngles(
                    poCADCircle->getPosition().getX(),
                    poCADCircle->getPosition().getY(),
                    poCADCircle->getPosition().getZ(),
                    poCADCircle->getRadius(), poCADCircle->getRadius(), 0.0,
                    0.0, 360.0,
                    0.0 );
            */
            poFeature->SetGeometryDirectly( poCircle );

            poFeature->SetField( FIELD_NAME_GEOMTYPE, "CADCircle" );
            break;
        }

        case CADGeometry::ARC:
        {
            CADArc * poCADArc = static_cast<CADArc*>(poCADGeometry);
            OGRCircularString * poCircle = new OGRCircularString();

            // Need at least 3 points in arc
            double dfStartAngle = poCADArc->getStartingAngle() * RAD2DEG;
            double dfEndAngle = poCADArc->getEndingAngle() * RAD2DEG;
            double dfMidAngle = (dfEndAngle + dfStartAngle) / 2;
            CADVector stCircleCenter = poCADArc->getPosition();

            OGRPoint  oCirclePoint;
            oCirclePoint.setX( stCircleCenter.getX() + poCADArc->getRadius() *
                                                            cos( dfStartAngle ) );
            oCirclePoint.setY( stCircleCenter.getY() + poCADArc->getRadius() *
                                                            sin( dfStartAngle ) );
            oCirclePoint.setZ( stCircleCenter.getZ() );
            poCircle->addPoint( &oCirclePoint );

            oCirclePoint.setX( stCircleCenter.getX() + poCADArc->getRadius() *
                                                            cos( dfMidAngle ) );
            oCirclePoint.setY( stCircleCenter.getY() + poCADArc->getRadius() *
                                                            sin( dfMidAngle ) );
            oCirclePoint.setZ( stCircleCenter.getZ() );
            poCircle->addPoint( &oCirclePoint );

            oCirclePoint.setX( stCircleCenter.getX() + poCADArc->getRadius() *
                                                            cos( dfEndAngle ) );
            oCirclePoint.setY( stCircleCenter.getY() + poCADArc->getRadius() *
                                                            sin( dfEndAngle ) );
            oCirclePoint.setZ( stCircleCenter.getZ() );
            poCircle->addPoint( &oCirclePoint );

            /*NOTE: alternative way:
                OGRGeometry * poArc = OGRGeometryFactory::approximateArcAngles(
                poCADArc->getPosition().getX(),
                poCADArc->getPosition().getY(),
                poCADArc->getPosition().getZ(),
                poCADArc->getRadius(), poCADArc->getRadius(), 0.0,
                dfStartAngle,
                dfStartAngle > dfEndAngle ?
                    ( dfEndAngle + 360.0f ) :
                    dfEndAngle,
                0.0 );
            */

            poFeature->SetGeometryDirectly( poCircle );
            poFeature->SetField( FIELD_NAME_GEOMTYPE, "CADArc" );

            break;
        }

        case CADGeometry::FACE3D:
        {
            CADFace3D * const poCADFace = ( CADFace3D* ) poCADGeometry;
            OGRPolygon * poPoly = new OGRPolygon();
            OGRLinearRing * poLR = new OGRLinearRing();

            for ( size_t i = 0; i < 3; ++i )
            {
                poLR->addPoint(
                    poCADFace->getCorner( i ).getX(),
                    poCADFace->getCorner( i ).getY(),
                    poCADFace->getCorner( i ).getZ()
                );
            }
            if ( !(poCADFace->getCorner( 2 ) == poCADFace->getCorner( 3 )) )
            {
                poLR->addPoint(
                    poCADFace->getCorner( 3 ).getX(),
                    poCADFace->getCorner( 3 ).getY(),
                    poCADFace->getCorner( 3 ).getZ()
                );
            }
            poPoly->addRingDirectly( poLR );
            poPoly->closeRings();
            poFeature->SetGeometryDirectly( poPoly );

            poFeature->SetField( FIELD_NAME_GEOMTYPE, "CADFace3D" );
            break;
        }

        case CADGeometry::LWPOLYLINE:
        {
            CADLWPolyline * const poCADLWPolyline = ( CADLWPolyline* ) poCADGeometry;

            poFeature->SetField( FIELD_NAME_GEOMTYPE, "CADLWPolyline" );

            /*
             * Excessive check, like in DXF driver.
             * I tried to make a single-point polyline, but couldn't make it.
             * Probably this check should be removed.
             */
            if( poCADLWPolyline->getVertexCount() == 1 )
            {
                poFeature->SetGeometryDirectly(
                            new OGRPoint( poCADLWPolyline->getVertex(0).getX(),
                                          poCADLWPolyline->getVertex(0).getY(),
                                          poCADLWPolyline->getVertex(0).getZ() )
                );

                break;
            }

            /*
             * If polyline has no arcs, handle it in easy way.
             */
            OGRLineString * poLS = new OGRLineString();

            if( poCADLWPolyline->getBulges().empty() )
            {
                for( size_t i = 0; i < poCADLWPolyline->getVertexCount(); ++i )
                {
                    CADVector stVertex = poCADLWPolyline->getVertex( i );
                    poLS->addPoint( stVertex.getX(),
                                    stVertex.getY(),
                                    stVertex.getZ()
                    );
                }

                poFeature->SetGeometryDirectly( poLS );
                break;
            }

            /*
             * Last case - if polyline has mixed arcs and lines.
             */
            bool   bLineStringStarted = false;
            std::vector< double > adfBulges = poCADLWPolyline->getBulges();
            const size_t nCount = std::min(adfBulges.size(), poCADLWPolyline->getVertexCount());

            for( size_t iCurrentVertex = 0; iCurrentVertex + 1 < nCount; iCurrentVertex++ )
            {
                CADVector stCurrentVertex = poCADLWPolyline->getVertex( iCurrentVertex );
                CADVector stNextVertex = poCADLWPolyline->getVertex( iCurrentVertex + 1 );

                double dfLength = sqrt( pow( stNextVertex.getX() - stCurrentVertex.getX(), 2 )
                                      + pow( stNextVertex.getY() - stCurrentVertex.getY(), 2 ) );

                /*
                 * Handling straight polyline segment.
                 */
                if( ( dfLength == 0 ) || ( adfBulges[iCurrentVertex] == 0 ) )
                {
                    if( !bLineStringStarted )
                    {
                        poLS->addPoint( stCurrentVertex.getX(),
                                        stCurrentVertex.getY(),
                                        stCurrentVertex.getZ()
                        );
                        bLineStringStarted = true;
                    }

                    poLS->addPoint( stNextVertex.getX(),
                                    stNextVertex.getY(),
                                    stNextVertex.getZ()
                    );
                }
                else
                {
                    double dfSegmentBulge = adfBulges[iCurrentVertex];
                    double dfH = ( dfSegmentBulge * dfLength ) / 2;
                    if( dfH == 0.0 )
                        dfH = 1.0; // just to avoid a division by zero
                    double dfRadius = ( dfH / 2 ) + ( dfLength * dfLength / ( 8 * dfH ) );
                    double dfOgrArcRotation = 0, dfOgrArcRadius = fabs( dfRadius );

                    /*
                     * Set arc's direction and keep bulge positive.
                     */
                    bool   bClockwise = ( dfSegmentBulge < 0 );
                    if( bClockwise )
                        dfSegmentBulge *= -1;

                    /*
                     * Get arc's center point.
                     */
                    double dfSaggita = fabs( dfSegmentBulge * ( dfLength / 2.0 ) );
                    double dfApo = bClockwise ? -( dfOgrArcRadius - dfSaggita ) :
                                                -( dfSaggita - dfOgrArcRadius );

                    CADVector stVertex;
                    stVertex.setX( stCurrentVertex.getX() - stNextVertex.getX() );
                    stVertex.setY( stCurrentVertex.getY() - stNextVertex.getY() );
                    stVertex.setZ( stCurrentVertex.getZ() );

                    CADVector stMidPoint;
                    stMidPoint.setX( stNextVertex.getX() + 0.5 * stVertex.getX() );
                    stMidPoint.setY( stNextVertex.getY() + 0.5 * stVertex.getY() );
                    stMidPoint.setZ( stVertex.getZ() );

                    CADVector stPperp;
                    stPperp.setX( stVertex.getY() );
                    stPperp.setY( -stVertex.getX() );
                    double dfStPperpLength = sqrt( stPperp.getX() * stPperp.getX() +
                                                   stPperp.getY() * stPperp.getY() );
                    // TODO: Check that length isnot 0
                    stPperp.setX( stPperp.getX() / dfStPperpLength );
                    stPperp.setY( stPperp.getY() / dfStPperpLength );

                    CADVector stOgrArcCenter;
                    stOgrArcCenter.setX( stMidPoint.getX() + ( stPperp.getX() * dfApo ) );
                    stOgrArcCenter.setY( stMidPoint.getY() + ( stPperp.getY() * dfApo ) );

                    /*
                     * Get the line's general vertical direction ( -1 = down, +1 = up ).
                     */
                    double dfLineDir = stNextVertex.getY() >
                                            stCurrentVertex.getY() ? 1.0f : -1.0f;

                    /*
                     * Get arc's starting angle.
                     */
                    double dfA = atan2( ( stOgrArcCenter.getY() - stCurrentVertex.getY() ),
                                        ( stOgrArcCenter.getX() - stCurrentVertex.getX() ) ) * RAD2DEG;
                    if( bClockwise && ( dfLineDir == 1.0 ) )
                        dfA += ( dfLineDir * 180.0 );

                    double dfOgrArcStartAngle = dfA > 0.0 ? -( dfA - 180.0 ) :
                                                            -( dfA + 180.0 );

                    /*
                     * Get arc's ending angle.
                     */
                    dfA = atan2( ( stOgrArcCenter.getY() - stNextVertex.getY() ),
                                 ( stOgrArcCenter.getX() - stNextVertex.getX() ) ) * RAD2DEG;
                    if( bClockwise && ( dfLineDir == 1.0 ) )
                        dfA += ( dfLineDir * 180.0 );

                    double dfOgrArcEndAngle = dfA > 0.0 ? -( dfA - 180.0 ) :
                                                          -( dfA + 180.0 );

                    if( !bClockwise && ( dfOgrArcStartAngle < dfOgrArcEndAngle) )
                        dfOgrArcEndAngle = -180.0 + ( dfLineDir * dfA );

                    if( bClockwise && ( dfOgrArcStartAngle > dfOgrArcEndAngle ) )
                        dfOgrArcEndAngle += 360.0;

                    /*
                     * Flip arc's rotation if necessary.
                     */
                    if( bClockwise && ( dfLineDir == 1.0 ) )
                        dfOgrArcRotation = dfLineDir * 180.0;

                    /*
                     * Tessellate the arc segment and append to the linestring.
                     */
                    OGRLineString * poArcpoLS =
                        ( OGRLineString * ) OGRGeometryFactory::approximateArcAngles(
                            stOgrArcCenter.getX(), stOgrArcCenter.getY(), stOgrArcCenter.getZ(),
                            dfOgrArcRadius, dfOgrArcRadius, dfOgrArcRotation,
                            dfOgrArcStartAngle,dfOgrArcEndAngle,
                            0.0 );

                    poLS->addSubLineString( poArcpoLS );

                    delete( poArcpoLS );
                }
            }

            if( poCADLWPolyline->isClosed() )
            {
                poLS->addPoint( poCADLWPolyline->getVertex(0).getX(),
                                poCADLWPolyline->getVertex(0).getY(),
                                poCADLWPolyline->getVertex(0).getZ()
                );
            }

            poFeature->SetGeometryDirectly( poLS );
            poFeature->SetField( FIELD_NAME_GEOMTYPE, "CADLWPolyline" );
            break;
        }

        // TODO: Unsupported smooth lines
        case CADGeometry::POLYLINE3D:
        {
            CADPolyline3D * const poCADPolyline3D = ( CADPolyline3D* ) poCADGeometry;
            OGRLineString * poLS = new OGRLineString();

            for( size_t i = 0; i < poCADPolyline3D->getVertexCount(); ++i )
            {
                CADVector stVertex = poCADPolyline3D->getVertex( i );

                poLS->addPoint( stVertex.getX(),
                                stVertex.getY(),
                                stVertex.getZ() );
            }

            poFeature->SetGeometryDirectly( poLS );
            poFeature->SetField( FIELD_NAME_GEOMTYPE, "CADPolyline3D" );
            break;
        }

        case CADGeometry::TEXT:
        {
            CADText * const poCADText = ( CADText * ) poCADGeometry;
            OGRPoint * poPoint = new OGRPoint( poCADText->getPosition().getX(),
                                               poCADText->getPosition().getY(),
                                               poCADText->getPosition().getZ() );
            CPLString sTextValue = CADRecode( poCADText->getTextValue(), nDWGEncoding );

            poFeature->SetField( FIELD_NAME_TEXT, sTextValue );
            poFeature->SetGeometryDirectly( poPoint );
            poFeature->SetField( FIELD_NAME_GEOMTYPE, "CADText" );

            sStyle.Printf("LABEL(f:\"Arial\",t:\"%s\",c:%s)", sTextValue.c_str(),
                                                              sHexColor.c_str());
            poFeature->SetStyleString( sStyle );
            break;
        }

        case CADGeometry::MTEXT:
        {
            CADMText * const poCADMText = ( CADMText * ) poCADGeometry;
            OGRPoint * poPoint = new OGRPoint( poCADMText->getPosition().getX(),
                                               poCADMText->getPosition().getY(),
                                               poCADMText->getPosition().getZ() );
            CPLString sTextValue = CADRecode( poCADMText->getTextValue(), nDWGEncoding );

            poFeature->SetField( FIELD_NAME_TEXT, sTextValue );
            poFeature->SetGeometryDirectly( poPoint );
            poFeature->SetField( FIELD_NAME_GEOMTYPE, "CADMText" );

            sStyle.Printf("LABEL(f:\"Arial\",t:\"%s\",c:%s)", sTextValue.c_str(),
                                                              sHexColor.c_str());
            poFeature->SetStyleString( sStyle );
            break;
        }

        case CADGeometry::SPLINE:
        {
            CADSpline * const poCADSpline = ( CADSpline * ) poCADGeometry;
            OGRLineString * poLS = new OGRLineString();

            // TODO: Interpolate spline as points or curves
            for( size_t i = 0; i < poCADSpline->getControlPoints().size(); ++i )
            {
                poLS->addPoint( poCADSpline->getControlPoints()[i].getX(),
                                poCADSpline->getControlPoints()[i].getY(),
                                poCADSpline->getControlPoints()[i].getZ() );
            }

            poFeature->SetGeometryDirectly( poLS );
            poFeature->SetField( FIELD_NAME_GEOMTYPE, "CADSpline" );
            break;
        }

        case CADGeometry::ELLIPSE:
        {
            CADEllipse * poCADEllipse = static_cast<CADEllipse*>(poCADGeometry);

            // FIXME: Start/end angles should be swapped to work exactly as DXF driver.
            // is it correct?
            double dfStartAngle = poCADEllipse->getStartingAngle() * RAD2DEG;
            double dfEndAngle = poCADEllipse->getEndingAngle() * RAD2DEG;
            if( dfStartAngle > dfEndAngle )
            {
                dfEndAngle += 360.0;
            }
            double dfAxisRatio = poCADEllipse->getAxisRatio();

            CADVector stEllipseCenter = poCADEllipse->getPosition();
            CADVector vectSMAxis = poCADEllipse->getSMAxis();
            double dfPrimaryRadius, dfSecondaryRadius;
            double dfRotation;
            dfPrimaryRadius = sqrt( vectSMAxis.getX() * vectSMAxis.getX()
                                    + vectSMAxis.getY() * vectSMAxis.getY()
                                    + vectSMAxis.getZ() * vectSMAxis.getZ() );

            dfSecondaryRadius = dfAxisRatio * dfPrimaryRadius;

            dfRotation = -1 * atan2( vectSMAxis.getY(), vectSMAxis.getX() ) * RAD2DEG;
            /* NOTE: alternative way:
            OGRCircularString * poEllipse = new OGRCircularString();
            OGRPoint  oEllipsePoint1;
            oEllipsePoint1.setX( stEllipseCenter.getX() - dfPrimaryRadius *
                                                            cos( dfRotation ) );
            oEllipsePoint1.setY( stEllipseCenter.getY() + dfPrimaryRadius *
                                                            sin( dfRotation ) );
            oEllipsePoint1.setZ( stEllipseCenter.getZ() );
            poEllipse->addPoint( &oEllipsePoint1 );

            OGRPoint  oEllipsePoint2;
            oEllipsePoint2.setX( stEllipseCenter.getX() + dfSecondaryRadius *
                                                            cos( dfRotation ) );
            oEllipsePoint2.setY( stEllipseCenter.getY() + dfSecondaryRadius *
                                                            sin( dfRotation ) );
            oEllipsePoint2.setZ( stEllipseCenter.getZ() );
            poEllipse->addPoint( &oEllipsePoint2 );

            OGRPoint  oEllipsePoint3;
            oEllipsePoint3.setX( stEllipseCenter.getX() + dfPrimaryRadius *
                                                            cos( dfRotation ) );
            oEllipsePoint3.setY( stEllipseCenter.getY() - dfPrimaryRadius *
                                                            sin( dfRotation ) );
            oEllipsePoint3.setZ( stEllipseCenter.getZ() );
            poEllipse->addPoint( &oEllipsePoint3 );

            OGRPoint  oEllipsePoint4;
            oEllipsePoint4.setX( stEllipseCenter.getX() - dfSecondaryRadius *
                                                            cos( dfRotation ) );
            oEllipsePoint4.setY( stEllipseCenter.getY() - dfSecondaryRadius *
                                                            sin( dfRotation ) );
            oEllipsePoint4.setZ( stEllipseCenter.getZ() );
            poEllipse->addPoint( &oEllipsePoint4 );

            // Close the ellipse
            poEllipse->addPoint( &oEllipsePoint1 );
            */

            CPLDebug("CAD", "Position: %f, %f, %f, radius %f/%f, start angle: %f, end angle: %f, rotation: %f",
                stEllipseCenter.getX(), stEllipseCenter.getY(), stEllipseCenter.getZ(),
                dfPrimaryRadius, dfSecondaryRadius, dfStartAngle, dfEndAngle, dfRotation
            );
            OGRGeometry *poEllipse = OGRGeometryFactory::approximateArcAngles(
                    stEllipseCenter.getX(), stEllipseCenter.getY(),
                    stEllipseCenter.getZ(),
                    dfPrimaryRadius, dfSecondaryRadius, dfRotation,
                    dfStartAngle, dfEndAngle, 0.0 );

            poFeature->SetGeometryDirectly( poEllipse );
            poFeature->SetField( FIELD_NAME_GEOMTYPE, "CADEllipse" );
            break;
        }

        case CADGeometry::ATTDEF:
        {
            CADAttdef * const poCADAttdef = ( CADAttdef* ) poCADGeometry;
            OGRPoint * poPoint = new OGRPoint( poCADAttdef->getPosition().getX(),
                                               poCADAttdef->getPosition().getY(),
                                               poCADAttdef->getPosition().getZ() );
            CPLString sTextValue = CADRecode( poCADAttdef->getTag(), nDWGEncoding );

            poFeature->SetField( FIELD_NAME_TEXT, sTextValue );
            poFeature->SetGeometryDirectly( poPoint );
            poFeature->SetField( FIELD_NAME_GEOMTYPE, "CADAttdef" );

            sStyle.Printf("LABEL(f:\"Arial\",t:\"%s\",c:%s)", sTextValue.c_str(),
                                                              sHexColor.c_str());
            poFeature->SetStyleString( sStyle );
            break;
        }

        default:
        {
            CPLError( CE_Warning, CPLE_NotSupported,
                     "Unhandled feature. Skipping it." );

            poFeature->SetField( FIELD_NAME_GEOMTYPE, "CADUnknown" );
            delete poCADGeometry;
            return poFeature;
        }
    }

    delete poCADGeometry;
    poFeature->GetGeometryRef()->assignSpatialReference( poSpatialRef );
    return poFeature;
}
