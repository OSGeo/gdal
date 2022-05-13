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
 *  Copyright (c) 2016-2018 NextGIS, <info@nextgis.com>
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

#include "cadobjects.h"

#include <limits>
#include <math.h>
#include <algorithm>

//------------------------------------------------------------------------------
// CADVector
//------------------------------------------------------------------------------
#define EPSILON std::numeric_limits<double>::epsilon() * 16

CADVector::CADVector( double x, double y ) :
    X( x ),
    Y( y ),
    Z( 0.0 ),
    bHasZ( false )
{

}

CADVector::CADVector( double x, double y, double z ) :
    X( x ),
    Y( y ),
    Z( z ),
    bHasZ( true )
{

}

CADVector::CADVector( const CADVector& other )
{
    X     = other.X;
    Y     = other.Y;
    Z     = other.Z;
    bHasZ = other.bHasZ;
}

bool operator==( const CADVector& first, const CADVector& second )
{
    return ( CADVector::fcmp( first.X, second.X ) &&
             CADVector::fcmp( first.Y, second.Y ) &&
             CADVector::fcmp( first.Z, second.Z ) );
}

CADVector& CADVector::operator=( const CADVector& second )
{
    if( &second != this )
    {
        X     = second.X;
        Y     = second.Y;
        Z     = second.Z;
        bHasZ = second.bHasZ;
    }
    return * this;
}

bool CADVector::fcmp( double x, double y )
{
    return fabs( x - y ) < EPSILON ? true : false;
}

bool CADVector::getBHasZ() const
{
    return bHasZ;
}

void CADVector::setBHasZ( bool value )
{
    bHasZ = value;
}

double CADVector::getZ() const
{
    return Z;
}

void CADVector::setZ( double value )
{
    if( !bHasZ )
        bHasZ = true;
    Z         = value;
}

double CADVector::getY() const
{
    return Y;
}

void CADVector::setY( double value )
{
    Y = value;
}

double CADVector::getX() const
{
    return X;
}

void CADVector::setX( double value )
{
    X = value;
}

CADVector::CADVector() : X( .0 ), Y( .0 ), Z( .0 ), bHasZ( true )
{

}

//------------------------------------------------------------------------------
// CADText
//------------------------------------------------------------------------------

CADTextObject::CADTextObject() :
    CADEntityObject(TEXT),
    DataFlags(0),
    dfElevation(0.0),
    dfThickness(0.0),
    dfObliqueAng(0.0),
    dfRotationAng(0.0),
    dfHeight(0.0),
    dfWidthFactor(0.0),
    dGeneration(0),
    dHorizAlign(0),
    dVertAlign(0)
{
}

//------------------------------------------------------------------------------
// CADAttribObject
//------------------------------------------------------------------------------

CADAttribObject::CADAttribObject(ObjectType typeIn) :
    CADEntityObject(typeIn),
    DataFlags( 0 ),
    dfElevation(0.0),
    dfThickness(0.0),
    dfObliqueAng(0.0),
    dfRotationAng(0.0),
    dfHeight(0.0),
    dfWidthFactor(0.0),
    dGeneration(0),
    dHorizAlign(0),
    dVertAlign(0),
    dVersion(0),
    nFieldLength(0),
    nFlags(0),
    bLockPosition(false)
{
}

//------------------------------------------------------------------------------
// CADAttdef
//------------------------------------------------------------------------------

CADAttdefObject::CADAttdefObject() :
    CADAttribObject(ATTDEF)
{
}


//------------------------------------------------------------------------------
// CADBlockObject
//------------------------------------------------------------------------------

CADBlockObject::CADBlockObject() :
    CADEntityObject(BLOCK)
{
}

//------------------------------------------------------------------------------
// CADEndblkObject
//------------------------------------------------------------------------------

CADEndblkObject::CADEndblkObject() :
    CADEntityObject(ENDBLK)
{
}

//------------------------------------------------------------------------------
// CADSeqendObject
//------------------------------------------------------------------------------

CADSeqendObject::CADSeqendObject() :
    CADEntityObject(SEQEND)
{
}

//------------------------------------------------------------------------------
// CADInsertObject
//------------------------------------------------------------------------------

CADInsertObject::CADInsertObject(ObjectType typeIn) :
    CADEntityObject(typeIn),
    dfRotation( 0.0 ),
    bHasAttribs( false ),
    nObjectsOwned( 0 )
{
}

//------------------------------------------------------------------------------
// CADMInsertObject
//------------------------------------------------------------------------------

CADMInsertObject::CADMInsertObject() :
    CADEntityObject(MINSERT1), // TODO: it has 2 type codes?
    dfRotation( 0.0 ),
    bHasAttribs( false ),
    nObjectsOwned( 0 ),
    nNumCols( 0 ),
    nNumRows( 0 ),
    nColSpacing( 0 ),
    nRowSpacing( 0 )
{
}

//------------------------------------------------------------------------------
// CADVertex2DObject
//------------------------------------------------------------------------------

CADVertex2DObject::CADVertex2DObject() :
    CADEntityObject(VERTEX2D),
    dfStartWidth( 0.0 ),
    dfEndWidth( 0.0 ),
    dfBulge( 0.0 ),
    nVertexID( 0 ),
    dfTangentDir( 0.0 )
{
}

//------------------------------------------------------------------------------
// CADVertex3DObject
//------------------------------------------------------------------------------

CADVertex3DObject::CADVertex3DObject() :
    CADEntityObject(VERTEX3D)
{
}

//------------------------------------------------------------------------------
// CADVertexMeshObject
//------------------------------------------------------------------------------

CADVertexMeshObject::CADVertexMeshObject() :
    CADEntityObject(VERTEX_MESH)
{
}

//------------------------------------------------------------------------------
// CADVertexPFaceObject
//------------------------------------------------------------------------------

CADVertexPFaceObject::CADVertexPFaceObject() :
    CADEntityObject(VERTEX_PFACE)
{
}

//------------------------------------------------------------------------------
// CADVertexPFaceFaceObject
//------------------------------------------------------------------------------

CADVertexPFaceFaceObject::CADVertexPFaceFaceObject() :
    CADEntityObject(VERTEX_PFACE_FACE),
    iVertexIndex1( 0 ),
    iVertexIndex2( 0 ),
    iVertexIndex3( 0 ),
    iVertexIndex4( 0 )
{
}

//------------------------------------------------------------------------------
// CADPolyline2DObject
//------------------------------------------------------------------------------

CADPolyline2DObject::CADPolyline2DObject() :
    CADEntityObject(POLYLINE2D),
    dFlags( 0 ),
    dCurveNSmoothSurfType( 0 ),
    dfStartWidth( 0.0 ),
    dfEndWidth( 0.0 ),
    dfThickness( 0.0 ),
    dfElevation( 0.0 ),
    nObjectsOwned( 0 )
{
}

//------------------------------------------------------------------------------
// CADPolyline3DObject
//------------------------------------------------------------------------------

CADPolyline3DObject::CADPolyline3DObject() :
    CADEntityObject(POLYLINE3D),
    SplinedFlags( 0 ),
    ClosedFlags( 0 ),
    nObjectsOwned( 0 )
{
}

//------------------------------------------------------------------------------
// CADArcObject
//------------------------------------------------------------------------------

CADArcObject::CADArcObject() :
    CADEntityObject(ARC),
    dfRadius( 0.0 ),
    dfThickness( 0.0 ),
    dfStartAngle( 0.0 ),
    dfEndAngle( 0.0 )
{
}

//------------------------------------------------------------------------------
// CADCircleObject
//------------------------------------------------------------------------------

CADCircleObject::CADCircleObject() :
    CADEntityObject(CIRCLE),
    dfRadius( 0.0 ),
    dfThickness( 0.0 )
{
}

//------------------------------------------------------------------------------
// CADLineObject
//------------------------------------------------------------------------------

CADLineObject::CADLineObject() :
    CADEntityObject(LINE),
    dfThickness( 0.0 )
{
}

//------------------------------------------------------------------------------
// CADBaseControlObject
//------------------------------------------------------------------------------

CADBaseControlObject::CADBaseControlObject(ObjectType typeIn) :
    CADObject(typeIn),
    nObjectSizeInBits( 0 ),
    nNumReactors( 0 ),
    bNoXDictionaryPresent( false )
{
}

//------------------------------------------------------------------------------
// CADBlockControlObject
//------------------------------------------------------------------------------

CADBlockControlObject::CADBlockControlObject() :
    CADBaseControlObject(BLOCK_CONTROL_OBJ),
    nNumEntries( 0 )
{
}

//------------------------------------------------------------------------------
// CADBlockHeaderObject
//------------------------------------------------------------------------------

CADBlockHeaderObject::CADBlockHeaderObject() :
    CADBaseControlObject(BLOCK_HEADER),
    b64Flag( false ),
    dXRefIndex( 0 ),
    bXDep( false ),
    bAnonymous( false ),
    bHasAtts( false ),
    bBlkisXRef( false ),
    bXRefOverlaid( false ),
    bLoadedBit( false ),
    nOwnedObjectsCount( 0 ),
    nSizeOfPreviewData( 0 ),
    nInsertUnits( 0 ),
    bExplodable( false ),
    dBlockScaling( 0 )
{
}

//------------------------------------------------------------------------------
// CADLayerControlObject
//------------------------------------------------------------------------------

CADLayerControlObject::CADLayerControlObject() :
    CADBaseControlObject(LAYER_CONTROL_OBJ),
    nNumEntries( 0 )
{
}

//------------------------------------------------------------------------------
// CADLayerObject
//------------------------------------------------------------------------------

CADLayerObject::CADLayerObject() :
    CADBaseControlObject(LAYER),
    b64Flag( 0 ),
    dXRefIndex( 0 ),
    bXDep( 0 ),
    bFrozen( false ),
    bOn( false ),
    bFrozenInNewVPORT( false ),
    bLocked( false ),
    bPlottingFlag( false ),
    dLineWeight( 0 ),
    dCMColor( 0 )
{
}

//------------------------------------------------------------------------------
// CADLineTypeControlObject
//------------------------------------------------------------------------------

CADLineTypeControlObject::CADLineTypeControlObject() :
    CADBaseControlObject(LTYPE_CONTROL_OBJ),
    nNumEntries( 0 )
{
}

//------------------------------------------------------------------------------
// CADLineTypeObject
//------------------------------------------------------------------------------

CADLineTypeObject::CADLineTypeObject() :
    CADBaseControlObject(LTYPE1),
    b64Flag( false ),
    dXRefIndex( 0 ),
    bXDep( false ),
    dfPatternLen( 0.0 ),
    dAlignment( 0 ),
    nNumDashes( 0 )
{
}

//------------------------------------------------------------------------------
// CADPointObject
//------------------------------------------------------------------------------

CADPointObject::CADPointObject() :
    CADEntityObject(POINT),
    dfThickness( 0.0 ),
    dfXAxisAng( 0.0 )
{
}

//------------------------------------------------------------------------------
// CADSolidObject
//------------------------------------------------------------------------------

CADSolidObject::CADSolidObject() :
    CADEntityObject(SOLID),
    dfThickness( 0.0 ),
    dfElevation( 0.0 )
{
    avertCorners.reserve( 4 );
}

//------------------------------------------------------------------------------
// CADEllipseObject
//------------------------------------------------------------------------------

CADEllipseObject::CADEllipseObject() :
    CADEntityObject(ELLIPSE),
    dfAxisRatio( 0.0 ),
    dfBegAngle( 0.0 ),
    dfEndAngle( 0.0 )
{
}

//------------------------------------------------------------------------------
// CADRayObject
//------------------------------------------------------------------------------

CADRayObject::CADRayObject() :
    CADEntityObject(RAY)
{
}

//------------------------------------------------------------------------------
// CADXLineObject
//------------------------------------------------------------------------------

CADXLineObject::CADXLineObject() :
    CADEntityObject(XLINE)
{
}

//------------------------------------------------------------------------------
// CADDictionaryObject
//------------------------------------------------------------------------------

CADDictionaryObject::CADDictionaryObject() :
    CADBaseControlObject(DICTIONARY),
    nNumItems( 0 ),
    dCloningFlag( 0 ),
    dHardOwnerFlag( 0 )
{
}

//------------------------------------------------------------------------------
// CADLWPolylineObject
//------------------------------------------------------------------------------

CADLWPolylineObject::CADLWPolylineObject() :
    CADEntityObject(LWPOLYLINE),
    bClosed( false ),
    dfConstWidth( 0.0 ),
    dfElevation( 0.0 ),
    dfThickness( 0.0 )
{
}

//------------------------------------------------------------------------------
// CADSplineObject
//------------------------------------------------------------------------------

CADSplineObject::CADSplineObject() :
    CADEntityObject( SPLINE ),
    dScenario( 0 ),
    dSplineFlags( 0 ),
    dKnotParameter( 0 ),
    dDegree( 0 ),
    dfFitTol( 0.0 ),
    nNumFitPts( 0 ),
    bRational( false ),
    bClosed( false ),
    bPeriodic( false ),
    dfKnotTol( 0.0 ),
    dfCtrlTol( 0.0 ),
    nNumKnots( 0 ),
    nNumCtrlPts( 0 ),
    bWeight( false )
{
}

//------------------------------------------------------------------------------

const std::vector<char> CADCommonEntityObjectTypes{
    CADObject::POINT, CADObject::ARC, CADObject::TEXT, CADObject::ELLIPSE,
    CADObject::CIRCLE, CADObject::LINE, CADObject::LWPOLYLINE,
    CADObject::POLYLINE3D, CADObject::MLINE, CADObject::SPLINE, CADObject::SOLID,
    CADObject::MTEXT, CADObject::IMAGE, CADObject::XLINE, CADObject::RAY,
    CADObject::MLINE, CADObject::FACE3D, CADObject::POLYLINE_PFACE,
    CADObject::ATTRIB, CADObject::ATTDEF, CADObject::POLYLINE2D, CADObject::HATCH,
    CADObject::INSERT, CADObject::VERTEX3D, CADObject::VERTEX2D,
    CADObject::VERTEX_MESH, CADObject::VERTEX_PFACE, CADObject::VERTEX_PFACE_FACE,
    CADObject::TOLERANCE, CADObject::SOLID3D, CADObject::WIPEOUT, CADObject::TRACE
};

const std::vector<char> CADSupportedGeometryTypes{
    CADObject::POINT, CADObject::ARC, CADObject::TEXT, CADObject::ELLIPSE,
    CADObject::CIRCLE, CADObject::LINE, CADObject::LWPOLYLINE,
    CADObject::POLYLINE3D, CADObject::MLINE, CADObject::ATTRIB, CADObject::ATTDEF,
    CADObject::RAY, CADObject::SPLINE, CADObject::SOLID, CADObject::IMAGE,
    CADObject::MTEXT, CADObject::POLYLINE_PFACE, CADObject::XLINE,
    CADObject::FACE3D
};

bool isCommonEntityType( short nType )
{
    return std::find( CADCommonEntityObjectTypes.begin(),
                      CADCommonEntityObjectTypes.end(),
                      nType ) != CADCommonEntityObjectTypes.end();
}

bool isSupportedGeometryType( short nType )
{
    return std::find( CADSupportedGeometryTypes.begin(),
                      CADSupportedGeometryTypes.end(),
                      nType ) !=  CADSupportedGeometryTypes.end();
}

const std::map<char, std::string> CADObjectNames{
    { CADObject::UNUSED,               "UNUSED" },
    { CADObject::TEXT,                 "TEXT" },
    { CADObject::ATTRIB,               "ATTRIB" },
    { CADObject::ATTDEF,               "ATTDEF" },
    { CADObject::BLOCK,                "BLOCK" },
    { CADObject::ENDBLK,               "ENDBLK" },
    { CADObject::SEQEND,               "SEQEND" },
    { CADObject::INSERT,               "INSERT" },
    { CADObject::MINSERT1,             "MINSERT" },
    { CADObject::MINSERT2,             "MINSERT" },
    { CADObject::VERTEX2D,             "VERTEX 2D" },
    { CADObject::VERTEX3D,             "VERTEX 3D" },
    { CADObject::VERTEX_MESH,          "VERTEX MESH" },
    { CADObject::VERTEX_PFACE,         "VERTEX PFACE" },
    { CADObject::VERTEX_PFACE_FACE,    "VERTEX PFACE FACE" },
    { CADObject::POLYLINE2D,           "POLYLINE 2D" },
    { CADObject::POLYLINE3D,           "POLYLINE 3D" },
    { CADObject::ARC,                  "ARC" },
    { CADObject::CIRCLE,               "CIRCLE" },
    { CADObject::LINE,                 "LINE" },
    { CADObject::DIMENSION_ORDINATE,   "DIMENSION ORDINATE" },
    { CADObject::DIMENSION_LINEAR,     "DIMENSION LINEAR" },
    { CADObject::DIMENSION_ALIGNED,    "DIMENSION ALIGNED" },
    { CADObject::DIMENSION_ANG_3PT,    "DIMENSION ANG 3PT" },
    { CADObject::DIMENSION_ANG_2LN,    "DIMENSION AND 2LN" },
    { CADObject::DIMENSION_RADIUS,     "DIMENSION RADIUS" },
    { CADObject::DIMENSION_DIAMETER,   "DIMENSION DIAMETER" },
    { CADObject::POINT,                "POINT" },
    { CADObject::FACE3D,               "3DFACE" },
    { CADObject::POLYLINE_PFACE,       "POLYLINE PFACE" },
    { CADObject::POLYLINE_MESH,        "POLYLINE MESH" },
    { CADObject::SOLID,                "SOLID" },
    { CADObject::TRACE,                "TRACE" },
    { CADObject::SHAPE,                "SHAPE" },
    { CADObject::VIEWPORT,             "VIEWPORT" },
    { CADObject::ELLIPSE,              "ELLIPSE" },
    { CADObject::SPLINE,               "SPLINE" },
    { CADObject::REGION,               "REGION" },
    { CADObject::SOLID3D,              "3DSOLID" },
    { CADObject::BODY,                 "BODY" },
    { CADObject::RAY,                  "RAY" },
    { CADObject::XLINE,                "XLINE" },
    { CADObject::DICTIONARY,           "DICTIONARY" },
    { CADObject::OLEFRAME,             "OLEFRAME" },
    { CADObject::MTEXT,                "MTEXT" },
    { CADObject::LEADER,               "LEADER" },
    { CADObject::TOLERANCE,            "TOLERANCE" },
    { CADObject::MLINE,                "MLINE" },
    { CADObject::BLOCK_CONTROL_OBJ,    "BLOCK CONTROL OBJ" },
    { CADObject::BLOCK_HEADER,         "BLOCK HEADER" },
    { CADObject::LAYER_CONTROL_OBJ,    "LAYER CONTROL OBJ" },
    { CADObject::LAYER,                "LAYER" },
    { CADObject::STYLE_CONTROL_OBJ,    "STYLE CONTROL OBJ" },
    { CADObject::STYLE1,               "STYLE1" },
    { CADObject::STYLE2,               "STYLE2" },
    { CADObject::STYLE3,               "STYLE3" },
    { CADObject::LTYPE_CONTROL_OBJ,    "LTYPE CONTROL OBJ" },
    { CADObject::LTYPE1,               "LTYPE1" },
    { CADObject::LTYPE2,               "LTYPE2" },
    { CADObject::LTYPE3,               "LTYPE3" },
    { CADObject::VIEW_CONTROL_OBJ,     "VIEW CONTROL OBJ" },
    { CADObject::VIEW,                 "VIEW" },
    { CADObject::UCS_CONTROL_OBJ,      "UCS CONTROL OBJ" },
    { CADObject::UCS,                  "UCS" },
    { CADObject::VPORT_CONTROL_OBJ,    "VPORT CONTROL OBJ" },
    { CADObject::VPORT,                "VPORT" },
    { CADObject::APPID_CONTROL_OBJ,    "APPID CONTROL OBJ" },
    { CADObject::APPID,                "APPID" },
    { CADObject::DIMSTYLE_CONTROL_OBJ, "DIMSTYLE CONTROL OBJ" },
    { CADObject::DIMSTYLE,             "DIMSTYLE" },
    { CADObject::VP_ENT_HDR_CTRL_OBJ,  "VP ENT HDR CTRL OBJ" },
    { CADObject::VP_ENT_HDR,           "VP ENT HDR" },
    { CADObject::GROUP,                "GROUP" },
    { CADObject::MLINESTYLE,           "MLINESTYLE" },
    { CADObject::OLE2FRAME,            "OLE2FRAME" },
    { CADObject::DUMMY,                "DUMMY" },
    { CADObject::LONG_TRANSACTION,     "LONG TRANSACTION" },
    { CADObject::LWPOLYLINE,           "LWPOLYLINE" },
    { CADObject::HATCH,                "HATCH" },
    { CADObject::XRECORD,              "XRECORD" },
    { CADObject::ACDBPLACEHOLDER,      "ACDBPLACEHOLDER" },
    { CADObject::VBA_PROJECT,          "VBA PROJECT" },
    { CADObject::LAYOUT,               "LAYOUT" }
};

std::string getNameByType( CADObject::ObjectType eType )
{
    auto it = CADObjectNames.find( eType );
    if( it == CADObjectNames.end() )
        return "";

    return it->second;
}
//------------------------------------------------------------------------------
// CADObject
//------------------------------------------------------------------------------

CADObject::ObjectType CADObject::getType() const
{
    return type;
}

long CADObject::getSize() const
{
    return size;
}

void CADObject::setSize( long value )
{
    size = value;
}

short CADObject::getCRC() const
{
    return CRC;
}

void CADObject::setCRC( unsigned short value )
{
    CRC = value;
}

//------------------------------------------------------------------------------
// CADDimensionOrdinateObject
//------------------------------------------------------------------------------

CADDimensionOrdinateObject::CADDimensionOrdinateObject() :
    CADDimensionObject(DIMENSION_ORDINATE),
    Flags2( 0 )
{
}

//------------------------------------------------------------------------------
// CADDimensionLinearObject
//------------------------------------------------------------------------------

CADDimensionLinearObject::CADDimensionLinearObject() :
    CADDimensionObject(DIMENSION_LINEAR),
    dfExtLnRot( 0.0 ),
    dfDimRot( 0.0 )
{
}

//------------------------------------------------------------------------------
// CADDimensionAlignedObject
//------------------------------------------------------------------------------

CADDimensionAlignedObject::CADDimensionAlignedObject() :
    CADDimensionObject(DIMENSION_ALIGNED),
    dfExtLnRot( 0.0 )
{
}

//------------------------------------------------------------------------------
// CADDimensionAngular3PtObject
//------------------------------------------------------------------------------

CADDimensionAngular3PtObject::CADDimensionAngular3PtObject(ObjectType typeIn) :
    CADDimensionObject(typeIn)
{
}

//------------------------------------------------------------------------------
// CADDimensionAngular2LnObject
//------------------------------------------------------------------------------

CADDimensionAngular2LnObject::CADDimensionAngular2LnObject() :
    CADDimensionAngular3PtObject(DIMENSION_ANG_2LN)
{
}

//------------------------------------------------------------------------------
// CADDimensionRadiusObject
//------------------------------------------------------------------------------

CADDimensionRadiusObject::CADDimensionRadiusObject(ObjectType typeIn) :
    CADDimensionObject(typeIn),
    dfLeaderLen( 0.0 )
{
}

//------------------------------------------------------------------------------
// CADDimensionDiameterObject
//------------------------------------------------------------------------------

CADDimensionDiameterObject::CADDimensionDiameterObject() :
    CADDimensionRadiusObject(DIMENSION_DIAMETER)
{
}

//------------------------------------------------------------------------------
// CADImageObject
//------------------------------------------------------------------------------

CADImageObject::CADImageObject() :
    CADEntityObject(IMAGE),
    dClassVersion( 0 ),
    dfSizeX( 0.0 ),
    dfSizeY( 0.0 ),
    dDisplayProps( 0 ),
    bClipping( false ),
    dBrightness( 0 ),
    dContrast( 0 ),
    dFade( 0 ),
    bClipMode( false ),
    dClipBoundaryType( 0 ),
    nNumberVerticesInClipPolygon( 0 )
{
}

//------------------------------------------------------------------------------
// CADImageDefObject
//------------------------------------------------------------------------------

CADImageDefObject::CADImageDefObject() :
    CADImageDefReactorObject(IMAGEDEF),
    dfXImageSizeInPx( 0.0 ),
    dfYImageSizeInPx( 0.0 ),
    bIsLoaded( false ),
    dResUnits( 0 ),
    dfXPixelSize( 0.0 ),
    dfYPixelSize( 0.0 )
{
}

//------------------------------------------------------------------------------
// CADImageDefReactorObject
//------------------------------------------------------------------------------

CADImageDefReactorObject::CADImageDefReactorObject(ObjectType typeIn) :
    CADBaseControlObject(typeIn),
    dClassVersion( 0 )
{
}

//------------------------------------------------------------------------------
// CADMTextObject
//------------------------------------------------------------------------------

CADMTextObject::CADMTextObject() :
    CADEntityObject(MTEXT),
    dfRectWidth( 0.0 ),
    dfTextHeight( 0.0 ),
    dAttachment( 0 ),
    dDrawingDir( 0 ),
    dfExtents( 0.0 ),
    dfExtentsWidth( 0.0 ),
    dLineSpacingStyle( 0 ),
    dLineSpacingFactor( 0 ),
    bUnknownBit( false),
    dBackgroundFlags( 0 ),
    dBackgroundScaleFactor( 0 ),
    dBackgroundColor( 0 ),
    dBackgroundTransparency( 0 )
{
}

//------------------------------------------------------------------------------
// CADMLineObject
//------------------------------------------------------------------------------

CADMLineObject::CADMLineObject() :
    CADEntityObject(MLINE),
    dfScale( 0.0 ),
    dJust( 0 ),
    dOpenClosed( 0 ),
    nLinesInStyle( 0 ),
    nNumVertices( 0 )
{
}

//------------------------------------------------------------------------------
// CAD3DFaceObject
//------------------------------------------------------------------------------

CAD3DFaceObject::CAD3DFaceObject() :
    CADEntityObject(FACE3D),
    bHasNoFlagInd( false ),
    bZZero( false ),
    dInvisFlags( 0 )
{
}

//------------------------------------------------------------------------------
// CADPolylinePFaceObject
//------------------------------------------------------------------------------

CADPolylinePFaceObject::CADPolylinePFaceObject() :
    CADEntityObject(POLYLINE_PFACE),
    nNumVertices( 0 ),
    nNumFaces( 0 ),
    nObjectsOwned( 0 )
{
}

//------------------------------------------------------------------------------
// CADXRecordObject
//------------------------------------------------------------------------------

CADXRecordObject::CADXRecordObject() :
    CADBaseControlObject(XRECORD),
    nNumDataBytes( 0 ),
    dCloningFlag( 0 )
{
}
