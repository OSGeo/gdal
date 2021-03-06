/*******************************************************************************
 *  Project: libopencad
 *  Purpose: OpenSource CAD formats support library
 *  Author: Alexandr Borzykh, mush3d at gmail.com
 *  Author: Dmitry Baryshnikov, bishop.dev@gmail.com
 *  Language: C++
 *******************************************************************************
 *  The MIT License (MIT)
 *
 *  Copyright (c) 2016 Alexandr Borzykh
 *  Copyright (c) 2016 NextGIS, <info@nextgis.com>
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
#include "cadgeometry.h"

#include <cmath>
#include <iostream>

using namespace std;

//------------------------------------------------------------------------------
// CADGeometry
//------------------------------------------------------------------------------

Matrix::Matrix()
{
    matrix[0] = 1.0;
    matrix[1] = 0.0;
    matrix[2] = 0.0;
    matrix[3] = 0.0;
    matrix[4] = 1.0;
    matrix[5] = 0.0;
    matrix[6] = 0.0;
    matrix[7] = 0.0;
    matrix[8] = 1.0;
}

void Matrix::translate( const CADVector& vector )
{
    double a00 = matrix[0];
    double a01 = matrix[1];
    double a02 = matrix[2];
    double a10 = matrix[3];
    double a11 = matrix[4];
    double a12 = matrix[5];
    double a20 = matrix[6];
    double a21 = matrix[7];
    double a22 = matrix[8];

    matrix[6] = vector.getX() * a00 + vector.getY() * a10 + a20;
    matrix[7] = vector.getX() * a01 + vector.getY() * a11 + a21;
    matrix[8] = vector.getX() * a02 + vector.getY() * a12 + a22;
}

void Matrix::rotate( double rotation )
{
    const double s = sin( rotation );
    const double c = cos( rotation );
    double a00 = matrix[0];
    double a01 = matrix[1];
    double a02 = matrix[2];
    double a10 = matrix[3];
    double a11 = matrix[4];
    double a12 = matrix[5];

    matrix[0] = c * a00 + s * a10;
    matrix[1] = c * a01 + s * a11;
    matrix[2] = c * a02 + s * a12;

    matrix[3] = c * a10 - s * a00;
    matrix[4] = c * a11 - s * a01;
    matrix[5] = c * a12 - s * a02;
}

void Matrix::scale( const CADVector& vector )
{
    matrix[0] *= vector.getX();
    matrix[1] *= vector.getX();
    matrix[2] *= vector.getX();
    matrix[3] *= vector.getY();
    matrix[4] *= vector.getY();
    matrix[5] *= vector.getY();
}

CADVector Matrix::multiply( const CADVector& vector ) const
{
    CADVector out;
    out.setX( vector.getX() * matrix[0] + vector.getY() * matrix[1] + vector.getZ() * matrix[2] );
    out.setY( vector.getX() * matrix[3] + vector.getY() * matrix[4] + vector.getZ() * matrix[5] );
    out.setZ( vector.getX() * matrix[6] + vector.getY() * matrix[7] + vector.getZ() * matrix[8] );
    return out;
}

//------------------------------------------------------------------------------
// CADGeometry
//------------------------------------------------------------------------------

CADGeometry::CADGeometry() :
    geometryType( UNDEFINED ),
    m_thickness( 0 )
{
    geometry_color.R = 0;
    geometry_color.G = 0;
    geometry_color.B = 0;
}

CADGeometry::~CADGeometry()
{

}

CADGeometry::GeometryType CADGeometry::getType() const
{
    return geometryType;
}

double CADGeometry::getThickness() const
{
    return m_thickness;
}

void CADGeometry::setThickness( double thickness )
{
    m_thickness = thickness;
}

RGBColor CADGeometry::getColor() const
{
    return geometry_color;
}

void CADGeometry::setColor( RGBColor color )
{
    geometry_color = color;
}

vector<string> CADGeometry::getEED() const
{
    return asEED;
}

void CADGeometry::setEED( const vector<string>& eed )
{
    asEED = eed;
}

vector<CADAttrib> CADGeometry::getBlockAttributes() const
{
    return blockAttributes;
}

void CADGeometry::setBlockAttributes( const vector<CADAttrib>& data )
{
    blockAttributes = data;
}

//------------------------------------------------------------------------------
// CADUnknown
//------------------------------------------------------------------------------
CADUnknown::CADUnknown()
{
}

void CADUnknown::transform( const Matrix& /*matrix*/)
{
}

void CADUnknown::print() const
{
    cout << "|---------Unhandled---------|\n\n";
}

//------------------------------------------------------------------------------
// CADPoint3D
//------------------------------------------------------------------------------

CADPoint3D::CADPoint3D() :
    xAxisAng( 0.0 )
{
    geometryType = CADGeometry::POINT;
}

CADPoint3D::CADPoint3D( const CADVector& positionIn, double thicknessIn ) :
    position( positionIn),
    xAxisAng( 0.0 )
{
    m_thickness = thicknessIn;
    geometryType = CADGeometry::POINT;
}

CADVector CADPoint3D::getPosition() const
{
    return position;
}

void CADPoint3D::setPosition( const CADVector& value )
{
    position = value;
}

CADVector CADPoint3D::getExtrusion() const
{
    return extrusion;
}

void CADPoint3D::setExtrusion( const CADVector& value )
{
    extrusion = value;
}

double CADPoint3D::getXAxisAng() const
{
    return xAxisAng;
}

void CADPoint3D::setXAxisAng( double value )
{
    xAxisAng = value;
}

void CADPoint3D::print() const
{
    cout << "|---------Point---------|\n" <<
        "Position: \t" << position.getX() <<
                  "\t" << position.getY() <<
                  "\t" << position.getZ() << "\n\n";
}

void CADPoint3D::transform( const Matrix& matrix )
{
    position = matrix.multiply( position );
}

//------------------------------------------------------------------------------
// CADLine
//------------------------------------------------------------------------------

CADLine::CADLine()
{
    geometryType = CADGeometry::LINE;
}

CADLine::CADLine( const CADPoint3D& startIn, const CADPoint3D& endIn ) :
    start( startIn ),
    end( endIn )
{
    geometryType = CADGeometry::LINE;
}

CADPoint3D CADLine::getStart() const
{
    return start;
}

void CADLine::setStart( const CADPoint3D& value )
{
    start = value;
}

CADPoint3D CADLine::getEnd() const
{
    return end;
}

void CADLine::setEnd( const CADPoint3D& value )
{
    end = value;
}

void CADLine::print() const
{
    cout << "|---------Line---------|\n" <<
        "Start Position: \t" << start.getPosition().getX() <<
                        "\t" << start.getPosition().getY() <<
                        "\t" << start.getPosition().getZ() << "\n" <<
        "End Position: \t" << end.getPosition().getX() <<
                      "\t" << end.getPosition().getY() <<
                      "\t" << end.getPosition().getZ() << "\n\n";
}

void CADLine::transform( const Matrix& matrix )
{
    start.transform( matrix );
    end.transform( matrix );
}

//------------------------------------------------------------------------------
// CADCircle
//------------------------------------------------------------------------------

CADCircle::CADCircle() : radius( 0.0f )
{
    geometryType = CADGeometry::CIRCLE;
}

double CADCircle::getRadius() const
{
    return radius;
}

void CADCircle::setRadius( double value )
{
    radius = value;
}

void CADCircle::print() const
{
    cout << "|---------Circle---------|\n" <<
        "Position: \t" << position.getX() <<
                  "\t" << position.getY() <<
                  "\t" << position.getZ() << "\n" <<
        "Radius: " << radius << "\n\n";
}

//------------------------------------------------------------------------------
// CADArc
//------------------------------------------------------------------------------

CADArc::CADArc() : CADCircle(),
    startingAngle( 0.0f ),
    endingAngle( 0.0f )
{
    geometryType = CADGeometry::ARC;
}

double CADArc::getStartingAngle() const
{
    return startingAngle;
}

void CADArc::setStartingAngle( double value )
{
    startingAngle = value;
}

double CADArc::getEndingAngle() const
{
    return endingAngle;
}

void CADArc::setEndingAngle( double value )
{
    endingAngle = value;
}

void CADArc::print() const
{
    cout << "|---------Arc---------|\n" <<
        "Position: \t" << position.getX() <<
                  "\t" << position.getY() <<
                  "\t" << position.getZ() << "\n" <<
        "Radius: \t" << radius << "\n" <<
        "Beg & End angles: \t" << startingAngle <<
                          "\t" << endingAngle << "\n\n";
}

//------------------------------------------------------------------------------
// CADPolyline3D
//------------------------------------------------------------------------------

CADPolyline3D::CADPolyline3D()
{
    geometryType = CADGeometry::POLYLINE3D;
}

void CADPolyline3D::addVertex( const CADVector& vertex )
{
    vertices.push_back( vertex );
}

size_t CADPolyline3D::getVertexCount() const
{
    return vertices.size();
}

CADVector& CADPolyline3D::getVertex( size_t index )
{
    return vertices[index];
}

void CADPolyline3D::print() const
{
    cout << "|------Polyline3D-----|\n";
    for( size_t i = 0; i < vertices.size(); ++i )
    {
        cout << "  #" << i <<
            ". X: " << vertices[i].getX() <<
            ", Y: " << vertices[i].getY() << "\n";
    }
    cout << "\n";
}

void CADPolyline3D::transform( const Matrix& matrix )
{
    for( CADVector& vertex : vertices )
    {
        vertex = matrix.multiply( vertex );
    }
}

//------------------------------------------------------------------------------
// CADLWPolyline
//------------------------------------------------------------------------------

CADLWPolyline::CADLWPolyline() :
    bClosed( false ),
    constWidth( 0.0 ),
    elevation( 0.0 )
{
    geometryType = CADGeometry::LWPOLYLINE;
}

void CADLWPolyline::print() const
{
    cout << "|------LWPolyline-----|\n";
    for( size_t i = 0; i < vertices.size(); ++i )
    {
        cout << "  #" << i <<
            ". X: " << vertices[i].getX() <<
            ", Y: " << vertices[i].getY() << "\n";
    }
    cout << "\n";
}

double CADLWPolyline::getConstWidth() const
{
    return constWidth;
}

void CADLWPolyline::setConstWidth( double value )
{
    constWidth = value;
}

double CADLWPolyline::getElevation() const
{
    return elevation;
}

void CADLWPolyline::setElevation( double value )
{
    elevation = value;
}

CADVector CADLWPolyline::getVectExtrusion() const
{
    return vectExtrusion;
}

void CADLWPolyline::setVectExtrusion( const CADVector& value )
{
    vectExtrusion = value;
}

vector<pair<double, double> > CADLWPolyline::getWidths() const
{
    return widths;
}

void CADLWPolyline::setWidths( const vector<pair<double, double> >& value )
{
    widths = value;
}

vector<double> CADLWPolyline::getBulges() const
{
    return bulges;
}

void CADLWPolyline::setBulges( const vector<double>& value )
{
    bulges = value;
}

bool CADLWPolyline::isClosed() const
{
    return bClosed;
}

void CADLWPolyline::setClosed( bool state )
{
    bClosed = state;
}

//------------------------------------------------------------------------------
// CADEllipse
//------------------------------------------------------------------------------

CADEllipse::CADEllipse() : CADArc(),
    axisRatio( 0.0f )
{
    geometryType = CADGeometry::ELLIPSE;
}

double CADEllipse::getAxisRatio() const
{
    return axisRatio;
}

void CADEllipse::setAxisRatio( double value )
{
    axisRatio = value;
}

CADVector CADEllipse::getSMAxis()
{
    return vectSMAxis;
}

void CADEllipse::setSMAxis( const CADVector& SMAxisVect )
{
    vectSMAxis = SMAxisVect;
}

void CADEllipse::print() const
{
    cout << "|---------Ellipse---------|\n" <<
        "Position: \t" << position.getX() <<
                  "\t" << position.getY() <<
                  "\t" << position.getZ() << "\n" <<
        "Beg & End angles: \t" << startingAngle <<
                          "\t" << endingAngle << "\n\n";
}

//------------------------------------------------------------------------------
// CADText
//------------------------------------------------------------------------------

CADText::CADText() : CADPoint3D(),
    obliqueAngle( 0 ),
    rotationAngle( 0 ),
    height( 0 )
{
    geometryType = CADGeometry::TEXT;
}

string CADText::getTextValue() const
{
    return textValue;
}

void CADText::setTextValue( const string& value )
{
    textValue = value;
}

double CADText::getHeight() const
{
    return height;
}

void CADText::setHeight( double value )
{
    height = value;
}

double CADText::getRotationAngle() const
{
    return rotationAngle;
}

void CADText::setRotationAngle( double value )
{
    rotationAngle = value;
}

double CADText::getObliqueAngle() const
{
    return obliqueAngle;
}

void CADText::setObliqueAngle( double value )
{
    obliqueAngle = value;
}

void CADText::print() const
{
    cout << "|---------Text---------|\n" <<
        "Position: \t" << position.getX() <<
                  "\t" << position.getY() << "\n" <<
        "Text value: \t" << textValue << "\n\n";
}

//------------------------------------------------------------------------------
// CADRay
//------------------------------------------------------------------------------

CADRay::CADRay() : CADPoint3D()
{
    geometryType = CADGeometry::RAY;
}

CADVector CADRay::getVectVector() const
{
    return extrusion;
}

void CADRay::setVectVector( const CADVector& value )
{
    extrusion = value;
}

void CADRay::print() const
{
    cout << "|---------Ray---------|\n" <<
        "Position: \t" << position.getX() <<
                  "\t" << position.getY() << "\n" <<
        "Vector: \t" << extrusion.getX() <<
                "\t" << extrusion.getY() << "\n\n";
}

//------------------------------------------------------------------------------
// CADHatch
//------------------------------------------------------------------------------

CADHatch::CADHatch()
{
    geometryType = CADGeometry::HATCH;
}

//------------------------------------------------------------------------------
// CADSpline
//------------------------------------------------------------------------------

CADSpline::CADSpline() :
    scenario( 0 ),
    rational( false ),
    closed( false ),
    weight( false ),
    fitTolerance( 0.0 ),
    degree( 0 )
{
    geometryType = CADGeometry::SPLINE;
}

void CADSpline::print() const
{

    cout << "|---------Spline---------|\n" <<
        "Is rational: \t" << rational << "\n" <<
        "Is closed: \t" << closed << "\n" <<
        "Control pts count: " << avertCtrlPoints.size() << "\n";
    for( size_t j = 0; j < avertCtrlPoints.size(); ++j )
    {
        cout << "  #" << j << ".\t" << avertCtrlPoints[j].getX() <<
                               "\t" << avertCtrlPoints[j].getY() <<
                               "\t" << avertCtrlPoints[j].getZ() << "\t";
        if( weight == true )
            cout << ctrlPointsWeight[j] << "\n";
        else
            cout << "\n";
    }

    cout << "Fit pts count: " << averFitPoints.size() << "\n";
    for( size_t j = 0; j < averFitPoints.size(); ++j )
    {
        cout << "  #" << j << ".\t" << averFitPoints[j].getX() <<
                               "\t" << averFitPoints[j].getY() <<
                               "\t" << averFitPoints[j].getZ() << "\n";
    }
    cout << "\n";
}

void CADSpline::transform( const Matrix& matrix )
{
    for( CADVector& pt : avertCtrlPoints )
        pt = matrix.multiply( pt );
    for( CADVector& pt : averFitPoints )
        pt = matrix.multiply( pt );
}

long CADSpline::getScenario() const
{
    return scenario;
}

void CADSpline::setScenario( long value )
{
    scenario = value;
}

bool CADSpline::isRational() const
{
    return rational;
}

void CADSpline::setRational( bool value )
{
    rational = value;
}

bool CADSpline::isClosed() const
{
    return closed;
}

void CADSpline::setClosed( bool value )
{
    closed = value;
}

void CADSpline::addControlPointsWeight( double p_weight )
{
    ctrlPointsWeight.push_back( p_weight );
}

void CADSpline::addControlPoint( const CADVector& point )
{
    avertCtrlPoints.push_back( point );
}

void CADSpline::addFitPoint( const CADVector& point )
{
    averFitPoints.push_back( point );
}

bool CADSpline::getWeight() const
{
    return weight;
}

void CADSpline::setWeight( bool value )
{
    weight = value;
}

double CADSpline::getFitTolerance() const
{
    return fitTolerance;
}

void CADSpline::setFitTolerance( double value )
{
    fitTolerance = value;
}

long CADSpline::getDegree() const
{
    return degree;
}

void CADSpline::setDegree( long value )
{
    degree = value;
}

vector<CADVector>& CADSpline::getControlPoints()
{
    return avertCtrlPoints;
}

vector<CADVector>& CADSpline::getFitPoints()
{
    return averFitPoints;
}

vector<double>& CADSpline::getControlPointsWeights()
{
    return ctrlPointsWeight;
}

//------------------------------------------------------------------------------
// CADSolid
//------------------------------------------------------------------------------

CADSolid::CADSolid() :
    elevation( 0.0 )
{
    geometryType = CADGeometry::SOLID;
}

void CADSolid::print() const
{
    cout << "|---------Solid---------|\n";
    for( size_t i = 0; i < avertCorners.size(); ++i )
    {
        cout << "  #" << i << ".\t" << avertCorners[i].getX() <<
                               "\t" << avertCorners[i].getY() << "\n" <<
                "Elevation: " << elevation << "\n";
    }
    cout << "\n";
}

void CADSolid::transform( const Matrix& matrix )
{
    CADPoint3D::transform( matrix );
    for( CADVector& corner : avertCorners )
        corner = matrix.multiply( corner );
}

double CADSolid::getElevation() const
{
    return elevation;
}

void CADSolid::setElevation( double value )
{
    elevation = value;
}

void CADSolid::addCorner( const CADVector& corner )
{
    avertCorners.push_back( corner );
}

vector<CADVector> CADSolid::getCorners()
{
    return avertCorners;
}

//------------------------------------------------------------------------------
// CADImage
//------------------------------------------------------------------------------

CADImage::CADImage() :
    bTransparency( false ),
    bClipping( false ),
    dBrightness( 0 ),
    dContrast( 0 ),
    resolutionUnits( NONE ),
    clippingBoundaryType( 0 )
{
    geometryType = CADGeometry::IMAGE;
}

CADVector CADImage::getVertInsertionPoint() const
{
    return vertInsertionPoint;
}

void CADImage::setVertInsertionPoint( const CADVector& value )
{
    vertInsertionPoint = value;
}

CADVector CADImage::getImageSize() const
{
    return imageSize;
}

void CADImage::setImageSize( const CADVector& value )
{
    imageSize = value;
}

CADVector CADImage::getImageSizeInPx() const
{
    return imageSizeInPx;
}

void CADImage::setImageSizeInPx( const CADVector& value )
{
    imageSizeInPx = value;
}

CADVector CADImage::getPixelSizeInACADUnits() const
{
    return pixelSizeInACADUnits;
}

void CADImage::setPixelSizeInACADUnits( const CADVector& value )
{
    pixelSizeInACADUnits = value;
}

short CADImage::getClippingBoundaryType() const
{
    return clippingBoundaryType;
}

void CADImage::setClippingBoundaryType( short value )
{
    clippingBoundaryType = value;
}

enum CADImage::ResolutionUnit CADImage::getResolutionUnits() const
{
    return resolutionUnits;
}

void CADImage::setResolutionUnits( enum CADImage::ResolutionUnit res_unit )
{
    resolutionUnits = res_unit;
}

string CADImage::getFilePath() const
{
    return filePath;
}

void CADImage::setFilePath( const string& value )
{
    filePath = value;
}

void CADImage::setOptions( bool transparency, bool clip, unsigned char brightness,
    unsigned char contrast )
{
    bTransparency = transparency;
    bClipping     = clip;
    dBrightness   = brightness;
    dContrast     = contrast;
}

void CADImage::print() const
{
    cout << "|---------Image---------|\n" <<
        "Filepath: " << filePath << "\n" <<
        "Insertion point: " << vertInsertionPoint.getX() << "\t" <<
                               vertInsertionPoint.getY() << "\n" <<
        "Transparent? : " << bTransparency << "\n" <<
        "Brightness (0-100) : " << dBrightness << "\n" <<
        "Contrast (0-100) : " << dContrast << "\n" <<
        "Clipping polygon:" << endl;
    for( size_t i = 0; i < avertClippingPolygon.size(); ++i )
    {
        cout << "  #" << i << ". X: " << avertClippingPolygon[i].getX() <<
                              ", Y: " << avertClippingPolygon[i].getY() << "\n";
    }
    cout << "\n";
}

void CADImage::transform( const Matrix& matrix )
{
    vertInsertionPoint = matrix.multiply( vertInsertionPoint );
    for( CADVector& pt : avertClippingPolygon )
        pt = matrix.multiply( pt );
}

void CADImage::addClippingPoint( const CADVector& pt )
{
    avertClippingPolygon.push_back( pt );
}

//------------------------------------------------------------------------------
// CADMText
//------------------------------------------------------------------------------

CADMText::CADMText() :
    rectWidth( 0.0 ),
    extents( 0.0 ),
    extentsWidth( 0.0 )
{
    geometryType = CADGeometry::MTEXT;
}

double CADMText::getRectWidth() const
{
    return rectWidth;
}

void CADMText::setRectWidth( double value )
{
    rectWidth = value;
}

double CADMText::getExtents() const
{
    return extents;
}

void CADMText::setExtents( double value )
{
    extents = value;
}

double CADMText::getExtentsWidth() const
{
    return extentsWidth;
}

void CADMText::setExtentsWidth( double value )
{
    extentsWidth = value;
}

void CADMText::print() const
{
    cout << "|---------MText---------|\n" <<
        "Position: " << position.getX() << "\t" <<
                        position.getY() << "\t" <<
                        position.getZ() << "\n" <<
        "Text: " << textValue << "\n\n";
}

//------------------------------------------------------------------------------
// CADFace3D
//------------------------------------------------------------------------------

CADFace3D::CADFace3D() :
    invisFlags( 0 )
{
    geometryType = CADGeometry::FACE3D;
}

void CADFace3D::addCorner( const CADVector& corner )
{
    avertCorners.push_back( corner );
}

CADVector CADFace3D::getCorner( size_t index )
{
    return avertCorners[index];
}

void CADFace3D::print() const
{
    cout << "|---------3DFace---------|\n" <<
        "Corners: \n";
    for( size_t i = 0; i < avertCorners.size(); ++i )
    {
        cout << "  #" << i << ". X: " << avertCorners[i].getX() << "\t" <<
                                "Y: " << avertCorners[i].getY() << "\t" <<
                                "Z: " << avertCorners[i].getZ() << "\n";
    }
    cout << "\n";
}

void CADFace3D::transform( const Matrix& matrix )
{
    for( CADVector& corner : avertCorners )
    {
        corner = matrix.multiply( corner );
    }
}

short CADFace3D::getInvisFlags() const
{
    return invisFlags;
}

void CADFace3D::setInvisFlags( short value )
{
    invisFlags = value;
}

//------------------------------------------------------------------------------
// CADPolylinePFace
//------------------------------------------------------------------------------

CADPolylinePFace::CADPolylinePFace()
{
    geometryType = CADGeometry::POLYLINE_PFACE;
}

void CADPolylinePFace::print() const
{
    cout << "|---------PolylinePface---------|\n";
    for( size_t i = 0; i < vertices.size(); ++i )
    {
        cout << "  #" << i << ".\t" << vertices[i].getX() <<
                               "\t" << vertices[i].getY() <<
                               "\t" << vertices[i].getZ() << "\n";
    }
    cout << "\n";
}

void CADPolylinePFace::transform( const Matrix& matrix )
{
    for( CADVector& vertex : vertices )
        vertex = matrix.multiply( vertex );
}

void CADPolylinePFace::addVertex( const CADVector& vertex )
{
    vertices.push_back( vertex );
}

//------------------------------------------------------------------------------
// CADXLine
//------------------------------------------------------------------------------

CADXLine::CADXLine()
{
    geometryType = CADGeometry::XLINE;
}

void CADXLine::print() const
{
    cout << "|---------XLine---------|\n" <<
        "Position: " << position.getX() << "\t" <<
                        position.getY() << "\t" <<
                        position.getZ() << "\n" <<
        "Direction: " << extrusion.getX() << "\t" <<
                         extrusion.getY() << "\t" <<
                         extrusion.getZ() << "\n\n";
}

//------------------------------------------------------------------------------
// CADMLine
//------------------------------------------------------------------------------

CADMLine::CADMLine() :
    scale( 0.0 ),
    opened( false )
{
    geometryType = CADGeometry::MLINE;
}

void CADMLine::print() const
{
    cout << "|---------MLine---------|\n" <<
        "Base point: " << position.getX() << "\t" <<
                          position.getY() << "\t" <<
                          position.getZ() << "\n" <<
        "Vertices:\n";
    for( size_t i = 0; i < avertVertices.size(); ++i )
    {
        cout << "  #" << i << ".\t" << avertVertices[i].getX() <<
                               "\t" << avertVertices[i].getY() <<
                               "\t" << avertVertices[i].getZ() << "\n";
    }
    cout << "\n";
}

void CADMLine::transform( const Matrix& matrix )
{
    CADPoint3D::transform( matrix );
    for( CADVector& vertex : avertVertices )
    {
        vertex = matrix.multiply( vertex );
    }
}

double CADMLine::getScale() const
{
    return scale;
}

void CADMLine::setScale( double value )
{
    scale = value;
}

bool CADMLine::isOpened() const
{
    return opened;
}

void CADMLine::setOpened( bool value )
{
    opened = value;
}

void CADMLine::addVertex( const CADVector& vertex )
{
    avertVertices.push_back( vertex );
}

//------------------------------------------------------------------------------
// CADAttrib
//------------------------------------------------------------------------------

CADAttrib::CADAttrib() :
    dfElevation( 0.0 ),
    bLockPosition( false )
{
    geometryType = CADGeometry::ATTRIB;
}

void CADAttrib::print() const
{
    cout << "|---------Attribute---------|\n" <<
        "Base point: " << position.getX() << "\t" <<
                          position.getY() << "\t" <<
                          position.getZ() << "\n" <<
        "Tag: " << sTag << "\n" <<
        "Text: " << textValue << "\n\n";
}

void CADAttrib::transform( const Matrix& matrix )
{
    CADText::transform( matrix );
    vertAlignmentPoint = matrix.multiply( vertAlignmentPoint );
}

double CADAttrib::getElevation() const
{
    return dfElevation;
}

void CADAttrib::setElevation( double elev )
{
    dfElevation = elev;
}

string CADAttrib::getTag() const
{
    return sTag;
}

void CADAttrib::setTag( const string& tag )
{
    sTag = tag;
}

CADVector CADAttrib::getAlignmentPoint() const
{
    return vertAlignmentPoint;
}

void CADAttrib::setAlignmentPoint( const CADVector& vect )
{
    vertAlignmentPoint = vect;
}

bool CADAttrib::isPositionLocked() const
{
    return bLockPosition;
}

void CADAttrib::setPositionLocked( bool lock )
{
    bLockPosition = lock;
}

//------------------------------------------------------------------------------
// CADAttdef
//------------------------------------------------------------------------------

CADAttdef::CADAttdef()
{
    geometryType = CADGeometry::ATTDEF;
}

void CADAttdef::print() const
{
    cout << "|---------Attribute defn---------|\n" <<
        "Base point: " << position.getX() << "\t" <<
                          position.getY() << "\t" <<
                          position.getZ() << "\n" <<
        "Tag: " << sTag << "\n" <<
        "Text: " << textValue << "\n" <<
        "Prompt: " << sPrompt << "\n\n";
}

string CADAttdef::getPrompt() const
{
    return sPrompt;
}

void CADAttdef::setPrompt( const string& prompt )
{
    sPrompt = prompt;
}
