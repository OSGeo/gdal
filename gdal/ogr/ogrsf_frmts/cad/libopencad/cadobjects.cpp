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

#include "cadobjects.h"

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

bool CADVector::operator==( const CADVector& second )
{
    return ( fcmp( this->X, second.X ) && fcmp( this->Y, second.Y ) &&
                                                    fcmp( this->Z, second.Z ) );
}

CADVector CADVector::operator=( const CADVector& second )
{
    X     = second.X;
    Y     = second.Y;
    Z     = second.Z;
    bHasZ = second.bHasZ;
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

CADTextObject::CADTextObject()
{
    type = TEXT;
}

//------------------------------------------------------------------------------
// CADAttdef
//------------------------------------------------------------------------------

CADAttdefObject::CADAttdefObject()
{
    type = ATTDEF;
}

//------------------------------------------------------------------------------
// CADAttribObject
//------------------------------------------------------------------------------

CADAttribObject::CADAttribObject()
{
    type = ATTRIB;
}

//------------------------------------------------------------------------------
// CADBlockObject
//------------------------------------------------------------------------------

CADBlockObject::CADBlockObject()
{
    type = BLOCK;
}

//------------------------------------------------------------------------------
// CADEndblkObject
//------------------------------------------------------------------------------

CADEndblkObject::CADEndblkObject()
{
    type = ENDBLK;
}

//------------------------------------------------------------------------------
// CADSeqendObject
//------------------------------------------------------------------------------

CADSeqendObject::CADSeqendObject()
{
    type = SEQEND;
}

//------------------------------------------------------------------------------
// CADInsertObject
//------------------------------------------------------------------------------

CADInsertObject::CADInsertObject()
{
    type = INSERT;
}

//------------------------------------------------------------------------------
// CADMInsertObject
//------------------------------------------------------------------------------

CADMInsertObject::CADMInsertObject()
{
    type = MINSERT1; // TODO: it has 2 type codes?
}

//------------------------------------------------------------------------------
// CADVertex2DObject
//------------------------------------------------------------------------------

CADVertex2DObject::CADVertex2DObject()
{
    type = VERTEX2D;
}

//------------------------------------------------------------------------------
// CADVertex3DObject
//------------------------------------------------------------------------------

CADVertex3DObject::CADVertex3DObject()
{
    type = VERTEX3D;
}

//------------------------------------------------------------------------------
// CADVertexMeshObject
//------------------------------------------------------------------------------

CADVertexMeshObject::CADVertexMeshObject()
{
    type = VERTEX_MESH;
}

//------------------------------------------------------------------------------
// CADVertexPFaceObject
//------------------------------------------------------------------------------

CADVertexPFaceObject::CADVertexPFaceObject()
{
    type = VERTEX_PFACE;
}

//------------------------------------------------------------------------------
// CADVertexPFaceFaceObject
//------------------------------------------------------------------------------

CADVertexPFaceFaceObject::CADVertexPFaceFaceObject()
{
    type = VERTEX_PFACE_FACE;
}

//------------------------------------------------------------------------------
// CADPolyline2DObject
//------------------------------------------------------------------------------

CADPolyline2DObject::CADPolyline2DObject()
{
    type = POLYLINE2D;
}

//------------------------------------------------------------------------------
// CADPolyline3DObject
//------------------------------------------------------------------------------

CADPolyline3DObject::CADPolyline3DObject()
{
    type = POLYLINE3D;
}

//------------------------------------------------------------------------------
// CADArcObject
//------------------------------------------------------------------------------

CADArcObject::CADArcObject()
{
    type = ARC;
}

//------------------------------------------------------------------------------
// CADCircleObject
//------------------------------------------------------------------------------

CADCircleObject::CADCircleObject()
{
    type = CIRCLE;
}

//------------------------------------------------------------------------------
// CADLineObject
//------------------------------------------------------------------------------

CADLineObject::CADLineObject()
{
    type = LINE;
}

//------------------------------------------------------------------------------
// CADBlockControlObject
//------------------------------------------------------------------------------

CADBlockControlObject::CADBlockControlObject()
{
    type = BLOCK_CONTROL_OBJ;
}

//------------------------------------------------------------------------------
// CADBlockHeaderObject
//------------------------------------------------------------------------------

CADBlockHeaderObject::CADBlockHeaderObject()
{
    type = BLOCK_HEADER;
}

//------------------------------------------------------------------------------
// CADLayerControlObject
//------------------------------------------------------------------------------

CADLayerControlObject::CADLayerControlObject()
{
    type = LAYER_CONTROL_OBJ;
}

//------------------------------------------------------------------------------
// CADLayerObject
//------------------------------------------------------------------------------

CADLayerObject::CADLayerObject()
{
    type = LAYER;
}

//------------------------------------------------------------------------------
// CADLineTypeControlObject
//------------------------------------------------------------------------------

CADLineTypeControlObject::CADLineTypeControlObject()
{
    type = LTYPE_CONTROL_OBJ;
}

//------------------------------------------------------------------------------
// CADLineTypeObject
//------------------------------------------------------------------------------

CADLineTypeObject::CADLineTypeObject()
{
    type = LTYPE1;
}

//------------------------------------------------------------------------------
// CADPointObject
//------------------------------------------------------------------------------

CADPointObject::CADPointObject()
{
    type = POINT;
}

//------------------------------------------------------------------------------
// CADSolidObject
//------------------------------------------------------------------------------

CADSolidObject::CADSolidObject()
{
    type = SOLID;
    avertCorners.reserve( 4 );
}

//------------------------------------------------------------------------------
// CADEllipseObject
//------------------------------------------------------------------------------

CADEllipseObject::CADEllipseObject()
{
    type = ELLIPSE;
}

//------------------------------------------------------------------------------
// CADRayObject
//------------------------------------------------------------------------------

CADRayObject::CADRayObject()
{
    type = RAY;
}

//------------------------------------------------------------------------------
// CADXLineObject
//------------------------------------------------------------------------------

CADXLineObject::CADXLineObject()
{
    type = XLINE;
}

//------------------------------------------------------------------------------
// CADDictionaryObject
//------------------------------------------------------------------------------

CADDictionaryObject::CADDictionaryObject()
{
    type = DICTIONARY;
}

//------------------------------------------------------------------------------
// CADLWPolylineObject
//------------------------------------------------------------------------------

CADLWPolylineObject::CADLWPolylineObject()
{
    type = LWPOLYLINE;
}

//------------------------------------------------------------------------------
// CADSplineObject
//------------------------------------------------------------------------------

CADSplineObject::CADSplineObject() :
    nNumFitPts( 0 ),
    nNumKnots( 0 ),
    nNumCtrlPts( 0 ) // should be zeroed.
{
    type = SPLINE;
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

void CADObject::setType( const ObjectType& value )
{
    type = value;
}

short CADObject::getCRC() const
{
    return CRC;
}

void CADObject::setCRC( short value )
{
    CRC = value;
}

//------------------------------------------------------------------------------
// CADDimensionOrdinateObject
//------------------------------------------------------------------------------

CADDimensionOrdinateObject::CADDimensionOrdinateObject()
{
    type = DIMENSION_ORDINATE;
}

//------------------------------------------------------------------------------
// CADDimensionLinearObject
//------------------------------------------------------------------------------

CADDimensionLinearObject::CADDimensionLinearObject()
{
    type = DIMENSION_LINEAR;
}

//------------------------------------------------------------------------------
// CADDimensionAlignedObject
//------------------------------------------------------------------------------

CADDimensionAlignedObject::CADDimensionAlignedObject()
{
    type = DIMENSION_ALIGNED;
}

//------------------------------------------------------------------------------
// CADDimensionAngular3PtObject
//------------------------------------------------------------------------------

CADDimensionAngular3PtObject::CADDimensionAngular3PtObject()
{
    type = DIMENSION_ANG_3PT;
}

//------------------------------------------------------------------------------
// CADDimensionAngular2LnObject
//------------------------------------------------------------------------------

CADDimensionAngular2LnObject::CADDimensionAngular2LnObject()
{
    type = DIMENSION_ANG_2LN;
}

//------------------------------------------------------------------------------
// CADDimensionRadiusObject
//------------------------------------------------------------------------------

CADDimensionRadiusObject::CADDimensionRadiusObject()
{
    type = DIMENSION_RADIUS;
}

//------------------------------------------------------------------------------
// CADDimensionDiameterObject
//------------------------------------------------------------------------------

CADDimensionDiameterObject::CADDimensionDiameterObject()
{
    type = DIMENSION_DIAMETER;
}

//------------------------------------------------------------------------------
// CADImageObject
//------------------------------------------------------------------------------

CADImageObject::CADImageObject()
{
    type = IMAGE;
}

//------------------------------------------------------------------------------
// CADImageDefObject
//------------------------------------------------------------------------------

CADImageDefObject::CADImageDefObject()
{
    type = IMAGEDEF;
}

//------------------------------------------------------------------------------
// CADImageDefReactorObject
//------------------------------------------------------------------------------

CADImageDefReactorObject::CADImageDefReactorObject()
{
    type = IMAGEDEFREACTOR;
}

//------------------------------------------------------------------------------
// CADMTextObject
//------------------------------------------------------------------------------

CADMTextObject::CADMTextObject()
{
    type = MTEXT;
}

//------------------------------------------------------------------------------
// CADMLineObject
//------------------------------------------------------------------------------

CADMLineObject::CADMLineObject()
{
    type = MLINE;
}

//------------------------------------------------------------------------------
// CAD3DFaceObject
//------------------------------------------------------------------------------

CAD3DFaceObject::CAD3DFaceObject()
{
    type = FACE3D;
}

//------------------------------------------------------------------------------
// CADPolylinePFaceObject
//------------------------------------------------------------------------------

CADPolylinePFaceObject::CADPolylinePFaceObject()
{
    type = POLYLINE_PFACE;
}

//------------------------------------------------------------------------------
// CADXRecordObject
//------------------------------------------------------------------------------

CADXRecordObject::CADXRecordObject()
{
    type = XRECORD;
}
