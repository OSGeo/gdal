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
#ifndef CADGEOMETRIES_H
#define CADGEOMETRIES_H

#include "cadobjects.h"
#include "cadcolors.h"

#include <array>

class CADAttdef;
class CADAttrib;

/**
 * @brief The Matrix class
 */
class Matrix
{
public:
              Matrix();
    void      translate( const CADVector& vector );
    void      rotate( double rotation );
    void      scale( const CADVector& vector );
    CADVector multiply( const CADVector& vector ) const;
protected:
    std::array<double, 9> matrix;
};

/**
 * @brief Base CAD geometry class
 */
class OCAD_EXTERN CADGeometry
{
public:
    CADGeometry();
    virtual ~CADGeometry();
    /**
     * @brief The CAD geometry types enum
     */
    enum GeometryType
    {
        UNDEFINED = 0,
        POINT,
        CIRCLE,
        LWPOLYLINE,
        ELLIPSE,
        LINE,
        POLYLINE3D,
        TEXT,
        ARC,
        SPLINE,
        SOLID,
        RAY,
        HATCH, // NOT IMPLEMENTED
        IMAGE,
        MTEXT,
        MLINE,
        XLINE,
        FACE3D,
        POLYLINE_PFACE,
        ATTRIB,
        ATTDEF
    };

    enum GeometryType getType() const;
    double            getThickness() const;
    void              setThickness( double thicknes );
    RGBColor          getColor() const;
    void              setColor( RGBColor color ); // TODO: In 2004+ ACI is not the only way to set the color.

    std::vector<CADAttrib> getBlockAttributes() const;
    void              setBlockAttributes( const std::vector<CADAttrib>& value );

    std::vector<std::string> getEED() const;
    void setEED( const std::vector<std::string>& eed );

    virtual void print() const                     = 0;
    virtual void transform( const Matrix& matrix ) = 0;
protected:
    std::vector<CADAttrib> blockAttributes; // Attributes of block reference this geometry is attached to.

    std::vector<std::string>    asEED;
    enum GeometryType geometryType;
    double            thickness;
    RGBColor          geometry_color;
};

/**
 * @brief Geometry class which represents Unhandled geometry (means that library can't read it yet)
 */
class CADUnknown : public CADGeometry
{
public:
    CADUnknown();
    virtual ~CADUnknown(){}

    virtual void print() const override;
    void         transform( const Matrix& matrix ) override;
};

/**
 * @brief Geometry class which a single Point
 */
class OCAD_EXTERN CADPoint3D : public CADGeometry
{
public:
    CADPoint3D();
    CADPoint3D( const CADVector& positionIn, double thicknessIn );
    virtual ~CADPoint3D(){}
    CADVector getPosition() const;
    void      setPosition( const CADVector& value );

    CADVector getExtrusion() const;
    void      setExtrusion( const CADVector& value );

    double getXAxisAng() const;
    void   setXAxisAng( double value );

    virtual void print() const override;
    virtual void transform( const Matrix& matrix ) override;
protected:
    CADVector position;
    CADVector extrusion;
    double    xAxisAng;
};

/**
 * @brief Geometry class which represents a simple Line
 */
class OCAD_EXTERN CADLine : public CADGeometry
{
public:
    CADLine();
    CADLine( const CADPoint3D& startIn, const CADPoint3D& endIn );
    virtual ~CADLine(){}
    CADPoint3D getStart() const;
    void       setStart( const CADPoint3D& value );

    CADPoint3D getEnd() const;
    void       setEnd( const CADPoint3D& value );

    virtual void print() const override;
    virtual void transform( const Matrix& matrix ) override;
protected:
    CADPoint3D start;
    CADPoint3D end;
};

/**
 * @brief Geometry class which represents Polyline 3D
 */
class OCAD_EXTERN CADPolyline3D : public CADGeometry
{
public:
    CADPolyline3D();
    virtual ~CADPolyline3D(){}
    void   addVertex( const CADVector& vertex );
    size_t getVertexCount() const;
    CADVector& getVertex( size_t index );

    virtual void print() const override;
    virtual void transform( const Matrix& matrix ) override;
protected:
    std::vector<CADVector> vertexes;
};

/**
 * @brief Geometry class which represents LWPolyline
 */

class OCAD_EXTERN CADLWPolyline : public CADPolyline3D
{
public:
    CADLWPolyline();
    virtual ~CADLWPolyline(){}

    double getConstWidth() const;
    void   setConstWidth( double value );

    double getElevation() const;
    void   setElevation( double value );

    CADVector getVectExtrusion() const;
    void      setVectExtrusion( const CADVector& value );

    std::vector<std::pair<double, double> > getWidths() const;
    void  setWidths( const std::vector<std::pair<double, double> >& value );

    std::vector<double> getBulges() const;
    void           setBulges( const std::vector<double>& value );

    bool isClosed() const;
    void setClosed( bool state );

    virtual void print() const override;
protected:
    bool                          bClosed;
    double                        constWidth;
    double                        elevation;
    CADVector                     vectExtrusion;
    std::vector<double>                bulges;
    std::vector<std::pair<double, double> > widths; // Start & end.
};

/**
 * @brief Geometry class which represents Circle
 */
class OCAD_EXTERN CADCircle : public CADPoint3D
{
public:
    CADCircle();
    virtual ~CADCircle(){}

    double getRadius() const;
    void   setRadius( double value );

    virtual void print() const override;
protected:
    double radius;
};

/**
 * @brief Geometry class which represents Text
 */
class OCAD_EXTERN CADText : public CADPoint3D
{
public:
    CADText();
    virtual ~CADText(){}

    std::string getTextValue() const;
    void   setTextValue( const std::string& value );

    double getHeight() const;
    void   setHeight( double value );

    double getRotationAngle() const;
    void   setRotationAngle( double value );

    double getObliqueAngle() const;
    void   setObliqueAngle( double value );

    virtual void print() const override;
protected:
    double obliqueAngle;
    double rotationAngle;
    double height;
    std::string textValue;
};

/**
 * @brief Geometry class which represents Arc
 */
class OCAD_EXTERN CADArc : public CADCircle
{
public:
    CADArc();
    virtual ~CADArc(){}

    double getStartingAngle() const;
    void   setStartingAngle( double value );

    double getEndingAngle() const;
    void   setEndingAngle( double value );

    virtual void print() const override;
protected:
    double startingAngle;
    double endingAngle;
};

/**
 * @brief Geometry class which represents Ellipse
 */
class OCAD_EXTERN CADEllipse : public CADArc
{
public:
    CADEllipse();
    virtual ~CADEllipse(){}

    double getAxisRatio() const;
    void   setAxisRatio( double value );

    CADVector getSMAxis();
    void      setSMAxis( const CADVector& vectSMA );

    virtual void print() const override;
protected:
    CADVector vectSMAxis;
    double    axisRatio;
};

/**
 * @brief Geometry class which represents Spline
 */
class OCAD_EXTERN CADSpline : public CADGeometry
{
public:
    CADSpline();
    virtual ~CADSpline(){}

    long getScenario() const;
    void setScenario( long value );

    bool isRational() const;
    void setRational( bool value );

    bool isClosed() const;
    void setClosed( bool value );

    std::vector<CADVector>& getControlPoints();
    std::vector<CADVector>& getFitPoints();
    std::vector<double>   & getControlPointsWeights();

    void addControlPointsWeight( double p_weight );
    void addControlPoint( const CADVector& point );
    void addFitPoint( const CADVector& point );

    bool getWeight() const;
    void setWeight( bool value );

    double getFitTollerance() const;
    void   setFitTollerance( double value );

    long getDegree() const;
    void setDegree( long value );

    virtual void print() const override;
    virtual void transform( const Matrix& matrix ) override;
protected:
    long   scenario;
    bool   rational;
    bool   closed;
    bool   weight;
    double fitTollerance;
    long   degree;

    std::vector<double>    ctrlPointsWeight;
    std::vector<CADVector> avertCtrlPoints;
    std::vector<CADVector> averFitPoints;
};

/**
 * @brief Geometry class which represents Solid
 */
class OCAD_EXTERN CADSolid : public CADPoint3D
{
public:
    CADSolid();
    virtual ~CADSolid(){}

    double getElevation() const;
    void   setElevation( double value );
    void   addCorner( const CADVector& corner );
    std::vector<CADVector> getCorners();

    virtual void print() const override;
    virtual void transform( const Matrix& matrix ) override;
protected:
    double            elevation;
    std::vector<CADVector> avertCorners;
};

/**
 * @brief Geometry class which represents Ray
 */
class OCAD_EXTERN CADRay : public CADPoint3D
{
public:
    CADRay();
    virtual ~CADRay(){}

    CADVector getVectVector() const;
    void      setVectVector( const CADVector& value );

    virtual void print() const override;
};

/**
 * @brief Geometry class which represents Hatch
 */
class OCAD_EXTERN CADHatch : public CADGeometry
{
public:
    CADHatch();
    virtual ~CADHatch(){}
};

/**
 * @brief Geometry class which represents Image (Raster Image)
 */
class OCAD_EXTERN CADImage : public CADGeometry
{
public:
    /**
     * @brief enum which describes in which units Image resolutions is present
     */
    enum ResolutionUnit
    {
        NONE = 0, CENTIMETER = 2, INCH = 5
    };

    CADImage();
    virtual ~CADImage(){}

    CADVector getVertInsertionPoint() const;
    void      setVertInsertionPoint( const CADVector& value );

    CADVector getImageSize() const;
    void      setImageSize( const CADVector& value );

    CADVector getImageSizeInPx() const;
    void      setImageSizeInPx( const CADVector& value );

    CADVector getPixelSizeInACADUnits() const;
    void      setPixelSizeInACADUnits( const CADVector& value );

    short getClippingBoundaryType() const;
    void  setClippingBoundaryType( short value );

    enum ResolutionUnit getResolutionUnits() const;
    void                setResolutionUnits( enum ResolutionUnit value );

    std::string getFilePath() const;
    void   setFilePath( const std::string& value );

    void setOptions( bool transparency, bool clip, unsigned char brightness,
                     unsigned char contrast );

    void addClippingPoint( const CADVector& pt );

    virtual void print() const override;
    virtual void transform( const Matrix& matrix ) override;
protected:
    CADVector     vertInsertionPoint;
    //CADVector vectUDirection;
    //CADVector vectVDirection;
    CADVector     imageSize;
    //bool bShow;
    //bool bShowWhenNotAlignedWithScreen;
    //bool bUseClippingBoundary;
    bool          bTransparency;
    bool          bClipping;
    unsigned char dBrightness;
    unsigned char dContrast;
    //char dFade;

    CADVector           imageSizeInPx;
    std::string         filePath;
    //bool bIsLoaded;
    enum ResolutionUnit resolutionUnits;
    //unsigned char       resolutionUnit; // 0 == none, 2 == centimeters, 5 == inches;
    CADVector           pixelSizeInACADUnits;

    short clippingBoundaryType; // 1 == rect, 2 == polygon
    std::vector<CADVector> avertClippingPolygon;
};

/**
 * @brief Geometry class which represents MText
 */
class OCAD_EXTERN CADMText : public CADText
{
public:
    CADMText();
    virtual ~CADMText(){}

    double getRectWidth() const;
    void   setRectWidth( double value );

    double getExtents() const;
    void   setExtents( double value );

    double getExtentsWidth() const;
    void   setExtentsWidth( double value );

    virtual void print() const override;
protected:
    double rectWidth;
    double extents;
    double extentsWidth;
    // TODO: do we need this here?
    //short dDrawingDir;
    //short dLineSpacingStyle;
    //short dLineSpacingFactor;
    //long dBackgroundFlags; // R2004+
    //long dBackgroundScaleFactor;
    //short dBackgroundColor;
    //long dBackgroundTransparency;
};

/**
 * @brief Geometry class which represents 3DFace
 */
class OCAD_EXTERN CADFace3D : public CADGeometry
{
public:
    CADFace3D();
    virtual ~CADFace3D(){}

    void      addCorner( const CADVector& corner );
    CADVector getCorner( size_t index );

    short getInvisFlags() const;
    void  setInvisFlags( short value );

    virtual void print() const override;
    virtual void transform( const Matrix& matrix ) override;
protected:
    std::vector<CADVector> avertCorners;
    short             invisFlags;
};

/**
 * @brief Geometry class which represents Polyline (PFace)
 */
class OCAD_EXTERN CADPolylinePFace : public CADGeometry
{
public:
    CADPolylinePFace();
    virtual ~CADPolylinePFace(){}

    void addVertex( const CADVector& vertex );

    virtual void print() const override;
    virtual void transform( const Matrix& matrix ) override;
protected:
    std::vector<CADVector> vertexes;
};

/**
 * @brief Geometry class which represents XLine
 */
class OCAD_EXTERN CADXLine : public CADRay
{
public:
    CADXLine();
    virtual ~CADXLine(){}

    virtual void print() const override;
};

/**
 * @brief Geometry class which represents MLine
 */
class OCAD_EXTERN CADMLine : public CADPoint3D
{
public:
    CADMLine();
    virtual ~CADMLine(){}

    double getScale() const;
    void   setScale( double value );

    bool isOpened() const;
    void setOpened( bool value );

    void addVertex( const CADVector& vertex );

    virtual void print() const override;
    virtual void transform( const Matrix& matrix ) override;
protected:
    double            scale;
    //char dJust;
    bool              opened; // 1 == open, 0 == close
    // TODO: do we need more properties here?
    std::vector<CADVector> avertVertexes;
};

/**
 * @brief Geometry class which represents Attribute
 */
class OCAD_EXTERN CADAttrib : public CADText
{
public:
    CADAttrib();
    virtual ~CADAttrib(){}

    double getElevation() const;
    void   setElevation( double );

    std::string getTag() const;
    void   setTag( const std::string& );

    CADVector getAlignmentPoint() const;
    void      setAlignmentPoint( const CADVector& );

    bool isPositionLocked() const;
    void setPositionLocked( bool );

    virtual void print() const override;
    virtual void transform( const Matrix& matrix ) override;
protected:
    CADVector vertAlignmentPoint;
    double    dfElevation;
    std::string    sTag;
    bool      bLockPosition;
};

/**
 * @brief Geometry class which represents Attribute definition
 */
class OCAD_EXTERN CADAttdef : public CADAttrib
{
public:
    CADAttdef();
    virtual ~CADAttdef(){}

    std::string getPrompt() const;
    void   setPrompt( const std::string& );

    virtual void print() const override;
protected:
    std::string sPrompt;
};

//class EXTERN LineType
//{
//public:
//    std::string sEntryName;
//    std::string sDescription;
//    double dfPatternLen;
//    char dAlignment;
//    char nNumDashes;
//    struct Dash
//    {
//        double dfLength;
//        short dComplexShapecode;
//        double dfXOffset;
//        double dfYOffset;
//        double dfScale;
//        double dfRotation;
//        short dShapeflag;
//    };
//    std::vector < char > abyTextArea; // TODO: what is it?
//    std::vector < CADHandle > hShapefiles; // TODO: one for each dash?
//};

//class EXTERN Block
//{
//public:
//    Block(CADFile * pCADFile)
//    {
//        pstCADFile_m = pCADFile;
//    }
//
//    std::string sBlockName;
//
//    CADFile * pstCADFile_m;
//
//    std::vector < std::pair < long long, short > > astAttachedGeometries;
//};


#endif // CADGEOMETRIES_H
