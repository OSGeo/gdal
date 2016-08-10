/*******************************************************************************
 *  Project: OGR CAD Driver
 *  Purpose: Implements driver based on libopencad
 *  Author: Alexandr Borzykh, mush3d at gmail.com
 *  Language: C++
 *******************************************************************************
 *  The MIT License (MIT)
 *
 *  Copyright (c) 2016 Alexandr Borzykh
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
#include "ogr_cad.h"
#include "cpl_conv.h"

#include <sstream>
#include <iomanip>

OGRCADLayer::OGRCADLayer( CADLayer &poCADLayer_, OGRSpatialReference *poSR ) :
    poCADLayer( poCADLayer_ )
{
    nNextFID = 0;
    poSpatialRef = NULL;

    poFeatureDefn = new OGRFeatureDefn( CADRecode( poCADLayer_.getName(), 29 ) );

    // Setting up layer geometry type
    OGRwkbGeometryType eGeomType;
    switch( poCADLayer.getGeometryType() )
    {
        case CADObject::ATTDEF:
        case CADObject::TEXT:
        case CADObject::MTEXT:
        case CADObject::POINT:
            eGeomType = wkbPoint;
            break;
        case CADObject::ELLIPSE:
        case CADObject::ARC:
        case CADObject::CIRCLE:
        case CADObject::POLYLINE3D:
        case CADObject::POLYLINE2D:
        case CADObject::LWPOLYLINE:
        case CADObject::LINE:
            eGeomType = wkbLineString;
            break;
        case CADObject::FACE3D:
            eGeomType = wkbPolygon;
            break;
        case -1:
            eGeomType = wkbGeometryCollection;
            break;
        default:
            eGeomType = wkbUnknown;
            break;
    }
    poFeatureDefn->SetGeomType(eGeomType);

    OGRFieldDefn  oClassField( "cadgeom_type", OFTString );
    poFeatureDefn->AddFieldDefn( &oClassField );

    OGRFieldDefn  oLinetypeField( "thickness", OFTReal );
    poFeatureDefn->AddFieldDefn( &oLinetypeField );

    OGRFieldDefn  oColorField( "color", OFTString );
    poFeatureDefn->AddFieldDefn( &oColorField );

    OGRFieldDefn  oExtendedField( "extentity_data", OFTString );
    poFeatureDefn->AddFieldDefn( &oExtendedField );

    OGRFieldDefn  oTextField( "text", OFTString );
    poFeatureDefn->AddFieldDefn( &oTextField );

    auto oAttrTags = poCADLayer.getAttributesTags();
    for( auto citer = oAttrTags.cbegin(); citer != oAttrTags.cend(); ++citer )
    {
        OGRFieldDefn oAttrField( (*citer).c_str(), OFTString );
        poFeatureDefn->AddFieldDefn( &oAttrField );
        asFeaturesAttributes.push_back( *citer );
    }

    // Applying spatial ref info
    poSpatialRef = poSR;
    poFeatureDefn->GetGeomFieldDefn(0)->SetSpatialRef( poSR );

    SetDescription( poFeatureDefn->GetName() );
    poFeatureDefn->Reference();
}

GIntBig OGRCADLayer::GetFeatureCount( int bForce )
{
    if( m_poFilterGeom != NULL || m_poAttrQuery != NULL )
        return OGRLayer::GetFeatureCount( bForce );

    return poCADLayer.getGeometryCount();
}

OGRCADLayer::~OGRCADLayer()
{
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

    if( poFeature == NULL )
        return( NULL );

    if( ( m_poFilterGeom == NULL ||  FilterGeometry( poFeature->GetGeometryRef() ) )
        && ( m_poAttrQuery == NULL || m_poAttrQuery->Evaluate( poFeature ) ) )
    {
        return poFeature;
    }

    return( NULL );
}

OGRFeature *OGRCADLayer::GetFeature( GIntBig nFID )
{
    if( poCADLayer.getGeometryCount() <= static_cast<size_t>(nFID)
        || nFID < 0 )
    {
        return NULL;
    }

    OGRFeature  *poFeature = NULL;
    CADGeometry *poCADGeometry = poCADLayer.getGeometry( nFID );

    if( NULL == poCADGeometry || GetLastErrorCode() != CADErrorCodes::SUCCESS )
    {
        CPLError( CE_Failure, CPLE_NotSupported,
                 "Failed to get geometry with ID = %lld from layer \"%s\". Libopencad errorcode: %d",
                 nFID, poCADLayer.getName().c_str(), GetLastErrorCode() );
        return NULL;
    }

    poFeature = new OGRFeature( poFeatureDefn );
    poFeature->SetFID( nFID );
    poFeature->SetField( "thickness", poCADGeometry->getThickness() );

    if( poCADGeometry->getEED().size() != 0 )
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

        poFeature->SetField( "extentity_data", sEEDAsOneString.c_str() );
    }

    RGBColor stRGB = poCADGeometry->getColor();
    short adRGB[3] { stRGB.R, stRGB.G, stRGB.B };
    std::stringstream oStringStream;
    oStringStream << "#" << std::hex << adRGB[0] << adRGB[1] << adRGB[2];
    oStringStream << "ff ";
    poFeature->SetField( "color", oStringStream.str().c_str() );

    oStringStream.str(std::string());
    oStringStream << "PEN(c:#" << std::hex << adRGB[0] << adRGB[1] << adRGB[2];
    oStringStream << ",w:5px)" << std::dec;
    poFeature->SetStyleString( oStringStream.str().c_str() );

    std::vector< CADAttrib > oBlockAttrs = poCADGeometry->getBlockAttributes();
    for( std::vector< CADAttrib >::const_iterator citerBlockAttrs = oBlockAttrs.cbegin(); 
         citerBlockAttrs != oBlockAttrs.cend(); ++citerBlockAttrs )
    {
        for( std::vector< std::string >::const_iterator citerFeatAttrs = asFeaturesAttributes.cbegin(); 
            citerFeatAttrs != asFeaturesAttributes.cend(); 
            ++citerFeatAttrs)
        {
            if( citerBlockAttrs->getTag() == *citerFeatAttrs )
            {
                poFeature->SetField( (*citerFeatAttrs).c_str(), citerBlockAttrs->getTextValue().c_str() );
            }
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
            poFeature->SetField( "cadgeom_type", "CADPoint" );
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
            poFeature->SetField( "cadgeom_type", "CADLine" );
            break;
        }

        case CADGeometry::CIRCLE:
        {
            CADCircle * const poCADCircle = ( CADCircle* ) poCADGeometry;
            OGRGeometry *poCircle = OGRGeometryFactory::approximateArcAngles(
                    poCADCircle->getPosition().getX(), 
                    poCADCircle->getPosition().getY(),
                    poCADCircle->getPosition().getZ(),
                    poCADCircle->getRadius(), poCADCircle->getRadius(), 0.0,
                    0.0, 360.0,
                    0.0 );
            poFeature->SetGeometryDirectly( poCircle );

            poFeature->SetField( "cadgeom_type", "CADCircle" );
            break;
        }

        case CADGeometry::ARC:
        {
            CADArc * const poCADArc = ( CADArc* ) poCADGeometry;

            double dfStartAngle = -1 * poCADArc->getEndingAngle()
                                     * 180 / M_PI;
            double dfEndAngle = -1 * poCADArc->getStartingAngle()
                                   * 180 / M_PI;

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

            poFeature->SetGeometryDirectly( poArc );
            poFeature->SetField( "cadgeom_type", "CADArc" );
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

            poFeature->SetField( "cadgeom_type", "CADFace3D" );
            break;
        }

        case CADGeometry::LWPOLYLINE:
        {
            CADLWPolyline * const poCADLWPolyline = ( CADLWPolyline* ) poCADGeometry;

            poFeature->SetField( "cadgeom_type", "CADLWPolyline" );

            /*
             * Excessive check, like in DXF driver.
             * I tried to make a single-point polyline, but couldnt make it.
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

            if( poCADLWPolyline->getBulges().size() == 0 )
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
            size_t iCurrentVertex = 0, 
                   iLastVertex = poCADLWPolyline->getVertexCount() - 1;
            std::vector< double > adfBulges = poCADLWPolyline->getBulges();

            while( iCurrentVertex != iLastVertex )
            {
                CADVector stCurrentVertex = poCADLWPolyline->getVertex( iCurrentVertex );
                CADVector stNextVertex = poCADLWPolyline->getVertex( iCurrentVertex + 1 );

                double dfLength = sqrt( pow( stNextVertex.getX() - stCurrentVertex.getX(), 2 )
                                      + pow( stNextVertex.getY() - stCurrentVertex.getY(), 2 ) );

                /*
                 * Handling straigth polyline segment.
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
                    // TODO: check that length isnot 0
                    stPperp.setX( stPperp.getX() / dfStPperpLength );
                    stPperp.setY( stPperp.getY() / dfStPperpLength );

                    CADVector stOgrArcCenter;
                    stOgrArcCenter.setX( stMidPoint.getX() + ( stPperp.getX() * dfApo ) );
                    stOgrArcCenter.setY( stMidPoint.getY() + ( stPperp.getY() * dfApo ) );

                    /*
                     * Get the line's general vertical direction ( -1 = down, +1 = up ).
                     */
                    double dfLineDir = stNextVertex.getY() > stCurrentVertex.getY() ? 1.0f : -1.0f;

                    /*
                     * Get arc's starting angle.
                     */
                    double dfA = atan2( ( stOgrArcCenter.getY() - stCurrentVertex.getY() ),
                                        ( stOgrArcCenter.getX() - stCurrentVertex.getX() ) ) * 180.0 / M_PI;
                    if( bClockwise && ( dfLineDir == 1.0 ) )
                        dfA += ( dfLineDir * 180.0 );

                    double dfOgrArcStartAngle = dfA > 0.0 ? -( dfA - 180.0 ) :
                                                            -( dfA + 180.0 );

                    /*
                     * Get arc's ending angle.
                     */
                    dfA = atan2( ( stOgrArcCenter.getY() - stNextVertex.getY() ),
                                 ( stOgrArcCenter.getX() - stNextVertex.getX() ) ) * 180.0 / M_PI;
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
                     * Tesselate the arc segment and append to the linestring.
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

                ++iCurrentVertex;
            }

            if( poCADLWPolyline->isClosed() )
            {
                poLS->addPoint( poCADLWPolyline->getVertex(0).getX(),
                                poCADLWPolyline->getVertex(0).getY(),
                                poCADLWPolyline->getVertex(0).getZ()
                );
            }

            poFeature->SetGeometryDirectly( poLS );
            poFeature->SetField( "cadgeom_type", "CADLWPolyline" );
            break;
        }

        // TODO: unsupported smooth lines
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
            poFeature->SetField( "cadgeom_type", "CADPolyline3D" );
            break;
        }

        case CADGeometry::TEXT:
        {
            CADText * const poCADText = ( CADText * ) poCADGeometry;
            OGRPoint * poPoint = new OGRPoint( poCADText->getPosition().getX(),
                                               poCADText->getPosition().getY(),
                                               poCADText->getPosition().getZ() );
            CPLString sTextValue = CADRecode( poCADText->getTextValue(), 29 );
            poFeature->SetField( "text", sTextValue );

            poFeature->SetGeometryDirectly( poPoint );
            poFeature->SetField( "cadgeom_type", "CADText" );

            oStringStream.str(std::string());
            oStringStream << "LABEL(f:\"Arial\",t:\"" << sTextValue << "\",";
            oStringStream << "c:#" << std::hex << adRGB[0] << adRGB[1] << adRGB[2];
            oStringStream << ")" << std::dec;
            poFeature->SetStyleString( oStringStream.str().c_str() );
            break;
        }

        case CADGeometry::MTEXT:
        {
            CADMText * const poCADMText = ( CADMText * ) poCADGeometry;
            OGRPoint * poPoint = new OGRPoint( poCADMText->getPosition().getX(),
                                               poCADMText->getPosition().getY(),
                                               poCADMText->getPosition().getZ() );
            CPLString sTextValue = CADRecode( poCADMText->getTextValue(), 29 );

            poFeature->SetField( "text", sTextValue );
            poFeature->SetGeometryDirectly( poPoint );
            poFeature->SetField( "cadgeom_type", "CADMText" );

            oStringStream.str(std::string());
            oStringStream << "LABEL(f:\"Arial\",t:\"" << sTextValue << "\",";
            oStringStream << "c:#" << std::hex << adRGB[0] << adRGB[1] << adRGB[2];
            oStringStream << ")" << std::dec;
            poFeature->SetStyleString( oStringStream.str().c_str() );
            break;
        }

        case CADGeometry::SPLINE:
        {
            CADSpline * const poCADSpline = ( CADSpline * ) poCADGeometry;
            OGRLineString * poLS = new OGRLineString();

            for( size_t i = 0; i < poCADSpline->getControlPoints().size(); ++i )
            {
                poLS->addPoint( poCADSpline->getControlPoints()[i].getX(),
                                poCADSpline->getControlPoints()[i].getY(),
                                poCADSpline->getControlPoints()[i].getZ() );
            }

            poFeature->SetGeometryDirectly( poLS );
            poFeature->SetField( "cadgeom_type", "CADSpline" );
            break;
        }

        case CADGeometry::ELLIPSE:
        {
            CADEllipse * const poCADEllipse = ( CADEllipse* ) poCADGeometry;

            // FIXME: start/end angles should be swapped to work exactly as DXF driver.
            // is it correct?
            double dfStartAngle = -1 * poCADEllipse->getEndingAngle()
                                     * 180 / M_PI;
            double dfEndAngle = -1 * poCADEllipse->getStartingAngle()
                                     * 180 / M_PI;
            double dfAxisRatio = poCADEllipse->getAxisRatio();

            if( dfStartAngle > dfEndAngle )
                dfEndAngle += 360.0;

            CADVector vectPosition = poCADEllipse->getPosition();
            CADVector vectSMAxis = poCADEllipse->getSMAxis();
            double dfPrimaryRadius, dfSecondaryRadius;
            double dfRotation;
            dfPrimaryRadius = sqrt( vectSMAxis.getX() * vectSMAxis.getX()
                                    + vectSMAxis.getY() * vectSMAxis.getY()
                                    + vectSMAxis.getZ() * vectSMAxis.getZ() );

            dfSecondaryRadius = dfAxisRatio * dfPrimaryRadius;

            dfRotation = -1 * atan2( vectSMAxis.getY(), vectSMAxis.getX() ) * 180 / M_PI;

            OGRGeometry *poEllipse =
                OGRGeometryFactory::approximateArcAngles(
                    vectPosition.getX(), vectPosition.getY(), vectPosition.getZ(),
                    dfPrimaryRadius, dfSecondaryRadius, dfRotation,
                    dfStartAngle, dfEndAngle, 0.0 );

            poFeature->SetGeometryDirectly( poEllipse );
            poFeature->SetField( "cadgeom_type", "CADEllipse" );
            break;
        }

        case CADGeometry::ATTDEF:
        {
            CADAttdef * const poCADAttdef = ( CADAttdef* ) poCADGeometry;
            OGRPoint * poPoint = new OGRPoint( poCADAttdef->getPosition().getX(),
                                               poCADAttdef->getPosition().getY(),
                                               poCADAttdef->getPosition().getZ() );
            CPLString sTextValue = CADRecode( poCADAttdef->getTag(), 29 );

            poFeature->SetField( "text", sTextValue );
            poFeature->SetGeometryDirectly( poPoint );
            poFeature->SetField( "cadgeom_type", "CADAttdef" );

            oStringStream.str(std::string());
            oStringStream << "LABEL(f:\"Arial\",t:\"" << sTextValue << "\",";
            oStringStream << "c:#" << std::hex << adRGB[0] << adRGB[1] << adRGB[2];
            oStringStream << ")" << std::dec;
            poFeature->SetStyleString( oStringStream.str().c_str() );
            break;
        }

        default:
        {
            CPLError( CE_Warning, CPLE_NotSupported,
                     "Unhandled feature. Skipping it." );

            poFeature->SetField( "cadgeom_type", "CADUnknown" );
            delete( poCADGeometry );
            return poFeature;
        }
    }

    delete( poCADGeometry );
    poFeature->GetGeometryRef()->assignSpatialReference( poSpatialRef );
    return poFeature;
}
