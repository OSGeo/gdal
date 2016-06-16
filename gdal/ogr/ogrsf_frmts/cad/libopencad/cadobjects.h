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

#ifndef CADOBJECTS_H
#define CADOBJECTS_H

#include "cadheader.h"

using namespace std;

class CADVector
{
public:
    CADVector();
    CADVector( double dx, double dy);
    CADVector( double dx, double dy, double dz);
    CADVector(const CADVector& other);
    bool operator == (const CADVector& second);
    CADVector operator = (const CADVector& second);
    double getX() const;
    void setX(double value);

    double getY() const;
    void setY(double value);

    double getZ() const;
    void setZ(double value);

    bool getBHasZ() const;
    void setBHasZ(bool value);

protected:
    inline bool fcmp(double x, double y);
protected:
    double X;
    double Y;
    double Z;
    bool bHasZ;
};

typedef struct _Eed
{
    short dLength = 0;
    CADHandle hApplication;
    vector<unsigned char> acData;
} CADEed;

typedef vector<CADHandle> CADHandleArray;
typedef vector<CADEed> CADEedArray;

/**
 * @brief The base CAD object class
 */
class CADObject
{
public:
    enum ObjectType
    {
        UNUSED = 0x0,
        TEXT = 0x1,
        ATTRIB = 0x2,
        ATTDEF = 0x3,
        BLOCK = 0x4,
        ENDBLK = 0x5,
        SEQEND = 0x6,
        INSERT = 0x7,
        MINSERT1 = 0x8,
        MINSERT2 = 0x9,
        VERTEX2D = 0x0A,
        VERTEX3D = 0x0B,
        VERTEX_MESH = 0x0C,
        VERTEX_PFACE = 0x0D,
        VERTEX_PFACE_FACE = 0x0E,
        POLYLINE2D = 0x0F,
        POLYLINE3D = 0x10,
        ARC = 0x11,
        CIRCLE = 0x12,
        LINE = 0x13,
        DIMENSION_ORDINATE = 0x14,
        DIMENSION_LINEAR = 0x15,
        DIMENSION_ALIGNED = 0x16,
        DIMENSION_ANG_3PT = 0x17,
        DIMENSION_ANG_2LN = 0x18,
        DIMENSION_RADIUS = 0x19,
        DIMENSION_DIAMETER = 0x1A,
        POINT = 0x1B,
        FACE3D = 0x1C,
        POLYLINE_PFACE = 0x1D,
        POLYLINE_MESH = 0x1E,
        SOLID = 0x1F,
        TRACE = 0x20,
        SHAPE = 0x21,
        VIEWPORT = 0x22,
        ELLIPSE = 0x23,
        SPLINE = 0x24,
        REGION = 0x25,
        SOLID3D = 0x26,
        BODY = 0x27,
        RAY = 0x28,
        XLINE = 0x29,
        DICTIONARY = 0x2A,
        OLEFRAME = 0x2B,
        MTEXT = 0x2C,
        LEADER = 0x2D,
        TOLERANCE = 0x2E,
        MLINE = 0x2F,
        BLOCK_CONTROL_OBJ = 0x30,
        BLOCK_HEADER = 0x31,
        LAYER_CONTROL_OBJ = 0x32,
        LAYER = 0x33,
        STYLE_CONTROL_OBJ = 0x34,
        STYLE1 = 0x35,
        STYLE2 = 0x36,
        STYLE3 = 0x37,
        LTYPE_CONTROL_OBJ = 0x38,
        LTYPE1 = 0x39,
        LTYPE2 = 0x3A,
        LTYPE3 = 0x3B,
        VIEW_CONTROL_OBJ = 0x3C,
        VIEW = 0x3D,
        UCS_CONTROL_OBJ = 0x3E,
        UCS = 0x3F,
        VPORT_CONTROL_OBJ = 0x40,
        VPORT = 0x41,
        APPID_CONTROL_OBJ = 0x42,
        APPID = 0x43,
        DIMSTYLE_CONTROL_OBJ = 0x44,
        DIMSTYLE = 0x45,
        VP_ENT_HDR_CTRL_OBJ = 0x46,
        VP_ENT_HDR = 0x47,
        GROUP = 0x48,
        MLINESTYLE = 0x49,
        OLE2FRAME = 0x4A,
        DUMMY = 0x4B,
        LONG_TRANSACTION = 0x4C,
        LWPOLYLINE = 0x4D,
        HATCH = 0x4E,
        XRECORD = 0x4F,
        ACDBPLACEHOLDER = 0x50,
        VBA_PROJECT = 0x51,
        LAYOUT = 0x52,
        // Codes below arent fixed, libopencad uses it for reading, in writing it will be different!
        CELLSTYLEMAP = 0x53,
        DBCOLOR = 0x54,
        DICTIONARYVAR = 0x55,
        DICTIONARYWDFLT = 0x56,
        FIELD = 0x57,
        GROUP_UNFIXED = 0x58,
        HATCH_UNFIXED = 0x59,
        IDBUFFER = 0x5A,
        IMAGE = 0x5B,
        IMAGEDEF = 0x5C,
        IMAGEDEFREACTOR = 0x5D,
        LAYER_INDEX = 0x5E,
        LAYOUT_UNFIXED = 0x5F,
        LWPOLYLINE_UNFIXED = 0x60,
        MATERIAL = 0x61,
        MLEADER = 0x62,
        MLEADERSTYLE = 0x63,
        OLE2FRAME_UNFIXED = 0x64,
        PLACEHOLDER = 0x65,
        PLOTSETTINGS = 0x66,
        RASTERVARIABLES = 0x67,
        SCALE = 0x68,
        SORTENTSTABLE = 0x69,
        SPATIAL_FILTER = 0x6A,
        SPATIAL_INDEX = 0x6B,
        TABLEGEOMETRY = 0x6C,
        TABLESTYLES = 0x6D,
        VBA_PROJECT_UNFIXED = 0x6E,
        VISUALSTYLE = 0x6F,
        WIPEOUTVARIABLE = 0x70,
        XRECORD_UNFIXED = 0x71
    };


    ObjectType getType() const;
    long getSize() const;

    void setSize(long value);

    void setType(const ObjectType &value);

    short getCRC() const;
    void setCRC(short value);

protected:
    long  size;
    ObjectType type;
    short CRC;
};



string getNameByType(CADObject::ObjectType eType);
bool isGeometryType(short nType);

/**
 * @brief The CADCommonED struct
 */
struct CADCommonED
{
    long nObjectSizeInBits;
    CADHandle hObjectHandle;
    CADEedArray aEED;

    bool bGraphicsPresented;
    vector<char> abyGraphicsData;

    unsigned char bbEntMode;
    long nNumReactors;

    bool bNoXDictionaryHandlePresent;
    bool bBinaryDataPresent;

    bool bIsByLayerLT; // R13-14 only

    bool bNoLinks;
    short nCMColor;

    double dfLTypeScale;
    unsigned char bbLTypeFlags;
    unsigned char bbPlotStyleFlags;
    char bbMaterialFlags;
    char nShadowFlags;

    short nInvisibility;
    unsigned char  nLineWeight;
};

/**
 * @brief The CADCommonEHD struct
 */
struct CADCommonEHD
{
    CADHandle hOwner; // depends on entmode.
    CADHandleArray hReactors;
    CADHandle hXDictionary;
    CADHandle hLayer;
    CADHandle hLType;

    CADHandle hPrevEntity;
    CADHandle hNextEntity;

    CADHandle hColorBookHandle;

    CADHandle hMaterial;
    CADHandle hPlotStyle;

    CADHandle hFullVisualStyle;
    CADHandle hFaceVisualStyle;
    CADHandle hEdgeVisualStyle;
};

/*
 * @brief The abstract class, which contains data common to all entities
 */
class CADEntityObject : public CADObject
{
public:
    struct CADCommonED stCed;
    struct CADCommonEHD stChed;
};

/**
 * @brief The CAD Text Object class
 */
class CADTextObject : public CADEntityObject
{
public:
    CADTextObject();
    unsigned char   DataFlags;
    double dfElevation;
    CADVector vertInsetionPoint;
    CADVector vertAlignmentPoint;
    CADVector vectExtrusion;
    double dfThickness;
    double dfObliqueAng;
    double dfRotationAng;
    double dfHeight;
    double dfWidthFactor;
    string sTextValue;
    short  dGeneration;
    short  dHorizAlign;
    short  dVertAlign;

    CADHandle hStyle;
};

/**
 * @brief The CAD Attribute Object class
 */
class CADAttribObject : public CADEntityObject
{
public:
    CADAttribObject();
    unsigned char   DataFlags;
    double dfElevation;
    CADVector vertInsetionPoint;
    CADVector vertAlignmentPoint;
    CADVector vectExtrusion;
    double dfThickness;
    double dfObliqueAng;
    double dfRotationAng;
    double dfHeight;
    double dfWidthFactor;
    string sTextValue;
    short  dGeneration;
    short  dHorizAlign;
    short  dVertAlign;
    char   dVersion; // R2010+
    string sTag;
    short  nFieldLength;
    unsigned char   nFlags;
    bool   bLockPosition;

    CADHandle hStyle;
};

/**
 * @brief The CAD Attribute definition Object class
 */
class CADAttdefObject : public CADAttribObject
{
public:
    CADAttdefObject();
    string sPrompt;
};

/**
 * @brief The CAD Block Object class
 */
class CADBlockObject : public CADEntityObject
{
public:
    CADBlockObject();
    string sBlockName;
};

/**
 * @brief The CAD End block Object class
 */
//TODO: do we need this class? Maybe CADEntityObject enouth?
class CADEndblkObject : public CADEntityObject
{
public:
    CADEndblkObject();
    // it actually has nothing more thatn CED and CEHD.
};

/**
 * @brief The CADSeqendObject class
 */
//TODO: do we need this class? Maybe CADEntityObject enouth?
class CADSeqendObject : public CADEntityObject
{
public:
    CADSeqendObject();
    // it actually has nothing more thatn CED and CEHD.
};

/**
 * @brief The CADInsertObject class
 */
class CADInsertObject : public CADEntityObject
{
public:
    CADInsertObject();
    CADVector vertInsertionPoint;
    CADVector vertScales;
    double dfRotation;
    CADVector vectExtrusion;
    bool    bHasAttribs;
    long    nObjectsOwned;

    CADHandle hBlockHeader;
    CADHandleArray hAtrribs;
    CADHandle hSeqend; // if bHasAttribs == true
};

/**
 * @brief The CADMInsertObject class
 */
class CADMInsertObject : public CADEntityObject
{
public:
    CADMInsertObject();
    CADVector vertInsertionPoint;
    CADVector vertScales;
    double dfRotation;
    CADVector vectExtrusion;
    bool    bHasAttribs;
    long    nObjectsOwned;

    short nNumCols;
    short nNumRows;
    short nColSpacing;
    short nRowSpacing;

    CADHandle hBlockHeader;
    CADHandleArray hAtrribs;
    CADHandle hSeqend; // if bHasAttribs == true
};

/**
 * @brief The CADVertex2DObject class
 */
class CADVertex2DObject : public CADEntityObject
{
public:
    CADVertex2DObject();
    CADVector vertPosition; // Z must be taken from 2d polyline elevation.
    double   dfStartWidth;
    double   dfEndWidth;
    double   dfBulge;
    long     nVertexID;
    double   dfTangentDir;

/* NOTES: Neither elevation nor thickness are present in the 2D VERTEX data.
 * Both should be taken from the 2D POLYLINE entity (15)
 */
};

/**
 * @brief The CADVertex3DObject class
 */
// TODO: do we need so many identical classes. Maybe CADVector(enum ObjectType eType)
// for all cases?
class CADVertex3DObject : public CADEntityObject
{
public:
    CADVertex3DObject();

    CADVector vertPosition;
};

/**
 * @brief The CADVertexMesh class
 */
class CADVertexMeshObject : public CADEntityObject
{
public:
    CADVertexMeshObject();
    CADVector vertPosition;
};

/**
 * @brief The CADVertexPFaceObject class
 */
class CADVertexPFaceObject : public CADEntityObject
{
public:
    CADVertexPFaceObject ();
    CADVector vertPosition;
};

/**
 * @brief The CADVertexPFaceFaceObject class
 */
class CADVertexPFaceFaceObject : public CADEntityObject
{
public:
    CADVertexPFaceFaceObject();
    // TODO: check DXF ref to get info what does it mean.
    short iVertexIndex1;
    short iVertexIndex2;
    short iVertexIndex3;
    short iVertexIndex4;
};

/**
 * @brief The CADPolyline2DObject class
 */
class CADPolyline2DObject : public CADEntityObject
{
public:
    CADPolyline2DObject();

    short  dFlags;
    short  dCurveNSmoothSurfType;
    double dfStartWidth;
    double dfEndWidth;
    double dfThickness;
    double dfElevation;
    CADVector vectExtrusion;

    long   nObjectsOwned;

    CADHandleArray hVertexes; // content really depends on DWG version.

    CADHandle hSeqend;
};

/**
 * @brief The CADPolyline3DObject class
 */
class CADPolyline3DObject : public CADEntityObject
{
public:
    CADPolyline3DObject();
    unsigned char SplinedFlags;
    unsigned char ClosedFlags;

    long   nObjectsOwned;

    CADHandleArray hVertexes; // content really depends on DWG version.

    CADHandle hSeqend;
};

/**
 * @brief The CADArc class
 */
class CADArcObject : public CADEntityObject
{
public:
    CADArcObject();
    CADVector vertPosition;
    double   dfRadius;
    double   dfThickness;
    CADVector vectExtrusion;
    double   dfStartAngle;
    double   dfEndAngle;
};

/**
 * @brief The CADCircleObject class
 */
class CADCircleObject : public CADEntityObject
{
public:
    CADCircleObject();
    CADVector vertPosition;
    double   dfRadius;
    double   dfThickness;
    CADVector vectExtrusion;
};

/**
 * @brief The CADLineObject class
 */
class CADLineObject : public CADEntityObject
{
public:
    CADLineObject();
    CADVector vertStart;
    CADVector vertEnd;
    double   dfThickness;
    CADVector vectExtrusion;
};

/**
 * @brief The CADBlockControlObject class
 */
class CADBlockControlObject : public CADObject
{
public:
    CADBlockControlObject();
    long nObjectSizeInBits;
    CADHandle hObjectHandle;
    CADEedArray aEED;
    long nNumReactors;
    bool bNoXDictionaryPresent;
    long nNumEntries; // doesnt count MODELSPACE and PAPERSPACE
    CADHandle hNull;
    CADHandle hXDictionary;
    CADHandleArray hBlocks; // ends with modelspace and paperspace handles.
};

/**
 * @brief The CADBlockHeaderObject class
 */
class CADBlockHeaderObject : public CADObject
{
public:
    CADBlockHeaderObject();
    long nObjectSizeInBits;
    CADHandle hObjectHandle;
    CADEedArray aEED;
    long nNumReactors;
    bool bNoXDictionaryPresent;
    string sEntryName;
    bool b64Flag;
    short dXRefIndex;
    bool bXDep;
    bool bAnonymous;
    bool bHasAtts;
    bool bBlkisXRef;
    bool bXRefOverlaid;
    bool bLoadedBit;
    long nOwnedObjectsCount;
    CADVector vertBasePoint;
    string sXRefPName;
    vector<unsigned char> adInsertCount; // TODO: ???
    string sBlockDescription;
    long nSizeOfPreviewData;
    vector<unsigned char> abyBinaryPreviewData;
    short nInsertUnits;
    bool bExplodable;
    char dBlockScaling;
    CADHandle hBlockControl;
    vector<CADHandle> hReactors;
    CADHandle hXDictionary;
    CADHandle hNull;
    CADHandle hBlockEntity;
    CADHandleArray hEntities;
    CADHandle hEndBlk;
    CADHandleArray hInsertHandles;
    CADHandle hLayout;
};

/**
 * @brief The CADLayerControlObject class
 */
class CADLayerControlObject : public CADObject
{
public:
    CADLayerControlObject();

    long nObjectSizeInBits;
    CADHandle hObjectHandle;
    CADEedArray aEED;
    long nNumReactors;
    bool bNoXDictionaryPresent;
    long nNumEntries; // counts layer "0"
    CADHandle hNull;
    CADHandle hXDictionary;
    CADHandleArray hLayers;
};

/**
 * @brief The CADLayerObject class
 */
class CADLayerObject : public CADObject
{
public:
    CADLayerObject();

    long nObjectSizeInBits;
    CADHandle hObjectHandle;
    CADEedArray aEED;
    long nNumReactors;
    bool bNoXDictionaryPresent;
    string sLayerName;
    bool b64Flag;
    short dXRefIndex;
    bool bXDep;
    bool bFrozen;
    bool bOn;
    bool bFrozenInNewVPORT;
    bool bLocked;
    bool bPlottingFlag;
    short dLineWeight;
    short dCMColor;

    CADHandle hLayerControl;
    CADHandleArray hReactors;
    CADHandle hXDictionary;
    CADHandle hExternalRefBlockHandle;
    CADHandle hPlotStyle;
    CADHandle hMaterial;
    CADHandle hLType;
    CADHandle hUnknownHandle;
};

/**
 * @brief The CADLineTypeControlObject class
 */
class CADLineTypeControlObject : public CADObject
{
public:
    CADLineTypeControlObject();

    long nObjectSizeInBits;
    CADHandle hObjectHandle;
    CADEedArray aEED;
    long nNumReactors;
    bool bNoXDictionaryPresent;
    long nNumEntries; // doesnt count BYBLOCK / BYLAYER.
    CADHandle hNull;
    CADHandle hXDictionary;
    CADHandleArray hLTypes;
};


typedef struct _dash
{
    double dfLength;
    short dComplexShapecode;
    double dfXOffset;
    double dfYOffset;
    double dfScale;
    double dfRotation;
    short dShapeflag;
} CADDash;

/**
 * @brief The CADLineTypeObject class
 */
class CADLineTypeObject : public CADObject
{
public:
    CADLineTypeObject();

    long nObjectSizeInBits;
    CADHandle hObjectHandle;
    CADEedArray aEED;
    long nNumReactors;
    bool bNoXDictionaryPresent;
    string sEntryName;
    bool b64Flag;
    short dXRefIndex;
    bool bXDep;
    string sDescription;
    double dfPatternLen;
    unsigned char dAlignment;
    unsigned char nNumDashes;
    vector<CADDash> astDashes;
    vector<unsigned char> abyTextArea; // TODO: what is it?
    CADHandle hLTControl;
    CADHandleArray hReactors;
    CADHandle hXDictionary;
    CADHandle hXRefBlock;
    CADHandleArray hShapefiles; // TODO: one for each dash?
};

/**
 * @brief The CADPointObject class
 */
class CADPointObject : public CADEntityObject
{
public:
    CADPointObject();

    CADVector vertPosition;
    double   dfThickness;
    CADVector vectExtrusion;
    double   dfXAxisAng;
};

/**
 * @brief The CADSolidObject class
 */
class CADSolidObject : public CADEntityObject
{
public:
    CADSolidObject();

    double dfThickness;
    double dfElevation;
    vector<CADVector> avertCorners;
    CADVector vectExtrusion;
};

/**
 * @brief The CADEllipseObject class
 */
class CADEllipseObject : public CADEntityObject
{
public:
    CADEllipseObject();

    CADVector vertPosition;
    CADVector vectSMAxis;
    CADVector vectExtrusion;
    double   dfAxisRatio;
    double   dfBegAngle;
    double   dfEndAngle;
};

/**
 * @brief The CADRayObject class
 */
class CADRayObject : public CADEntityObject
{
public:
    CADRayObject();

    CADVector vertPosition;
    CADVector vectVector;
};

/**
 * @brief The CADXLineObject class
 */
class CADXLineObject : public CADEntityObject
{
public:
    CADXLineObject();

    CADVector vertPosition;
    CADVector vectVector;
};

/**
 * @brief The CADDictionaryObject class
 */
class CADDictionaryObject : public CADObject
{
public:
    CADDictionaryObject();

    long nObjectSizeInBits;
    CADHandle hObjectHandle;
    CADEedArray aEED;
    long nNumReactors;
    bool bNoXDictionaryPresent;
    long nNumItems;
    short dCloningFlag;
    unsigned char  dHardOwnerFlag;

    string sDictionaryEntryName;
    vector<string> sItemNames;

    CADHandle hParentHandle;
    CADHandleArray hReactors;
    CADHandle hXDictionary;
    CADHandleArray hItemHandles;
};

/**
 * @brief The CADLWPolylineObject class
 */
class CADLWPolylineObject : public CADEntityObject
{
public:
    CADLWPolylineObject();

    double dfConstWidth;
    double dfElevation;
    double dfThickness;
    CADVector vectExtrusion;
    vector<CADVector> avertVertexes;
    vector<double> adfBulges;
    vector<short> adVertexesID;
    vector<pair<double, double>> astWidths; // start, end.
};

class CADSplineObject : public CADEntityObject
{
public:
    CADSplineObject();

    long dScenario;
    long dSplineFlags; // 2013+
    long dKnotParameter; // 2013+

    long   dDegree;
    double dfFitTol;
    CADVector vectBegTangDir;
    CADVector vectEndTangDir;
    long   nNumFitPts;

    bool bRational;
    bool bClosed;
    bool bPeriodic;
    double dfKnotTol;
    double dfCtrlTol;
    long nNumKnots;
    long nNumCtrlPts;
    bool bWeight;

    vector<double> adfKnots;
    vector<double> adfCtrlPointsWeight;
    vector<CADVector> avertCtrlPoints;
    vector<CADVector> averFitPoints;
};

typedef struct _dimdata
{
    char dVersion;
    CADVector vectExtrusion;
    CADVector vertTextMidPt;
    double dfElevation;
    unsigned char dFlags;
    string sUserText;
    double dfTextRotation;
    double dfHorizDir;
    double dfInsXScale;
    double dfInsYScale;
    double dfInsZScale;
    double dfInsRotation;

    short dAttachmentPoint;
    short dLineSpacingStyle;
    double dfLineSpacingFactor;
    double dfActualMeasurement;

    bool bUnknown;
    bool bFlipArrow1;
    bool bFlipArrow2;

    CADVector vert12Pt;
} CADCommonDimensionData;

class CADDimensionObject : public CADEntityObject
{
public:
    CADCommonDimensionData cdd;
    CADVector vert10pt;
    CADHandle hDimstyle;
    CADHandle hAnonymousBlock;
};

class CADDimensionOrdinateObject : public CADDimensionObject
{
public:
    CADDimensionOrdinateObject();
    CADVector vert13pt, vert14pt;
    unsigned char Flags2;
};

class CADDimensionLinearObject : public CADDimensionObject
{
public:
    CADDimensionLinearObject();
    CADVector vert13pt, vert14pt;

    double dfExtLnRot;
    double dfDimRot;
};

class CADDimensionAlignedObject : public CADDimensionObject
{
public:
    CADDimensionAlignedObject();
    CADVector vert13pt, vert14pt;

    double dfExtLnRot;
};

class CADDimensionAngular3PtObject : public CADDimensionObject
{
public:
    CADDimensionAngular3PtObject();
    CADVector vert13pt, vert14pt;
    CADVector vert15pt;
};

class CADDimensionAngular2LnObject : public CADDimensionAngular3PtObject
{
public:
    CADDimensionAngular2LnObject();

    CADVector vert16pt;

};

class CADDimensionRadiusObject : public CADDimensionObject
{
public:
    CADDimensionRadiusObject();

    CADVector vert15pt;
    double dfLeaderLen;
};

class CADDimensionDiameterObject : public CADDimensionRadiusObject
{
public:
    CADDimensionDiameterObject();
};

class CADImageObject : public CADEntityObject
{
public:
    CADImageObject();

    long dClassVersion;
    CADVector vertInsertion;
    CADVector vectUDirection;
    CADVector vectVDirection;
    double dfSizeX;
    double dfSizeY;
    /*  display properties (bit coded), 1==show image,
        2==show image when not aligned with screen, 4==use
        clipping boundary, 8==transparency on */
    short dDisplayProps;

    bool bClipping;
    unsigned char dBrightness;
    unsigned char dContrast;
    unsigned char dFade;
    bool bClipMode; // R2010+

    short dClipBoundaryType;

    long nNumberVertexesInClipPolygon;
    vector < CADVector > avertClippingPolygonVertexes;

    CADHandle hImageDef;
    CADHandle hImageDefReactor;
};

class CADImageDefReactorObject : public CADObject
{
public:
    CADImageDefReactorObject();

    long nObjectSizeInBits;
    CADHandle hObjectHandle;
    vector < CADEed > aEED;
    long nNumReactors;
    bool bNoXDictionaryPresent;
    long dClassVersion;
    CADHandle hParentHandle;
    vector < CADHandle > hReactors;
    CADHandle hXDictionary;
};

class CADImageDefObject : public CADImageDefReactorObject
{
public:
    CADImageDefObject();

    double dfXImageSizeInPx;
    double dfYImageSizeInPx;
    string sFilePath;
    bool bIsLoaded;
    unsigned char dResUnits; // 0 == none, 2 == centimeters, 5 == inches
    double dfXPixelSize; // size of 1 pixel in autocad units
    double dfYPixelSize;
};

class CADMTextObject : public CADEntityObject
{
public:
    CADMTextObject();

    CADVector vertInsertionPoint;
    CADVector vectExtrusion;
    CADVector vectXAxisDir;
    double dfRectWidth;
    double dfTextHeight;
    short dAttachment; // TODO: meaning unknown
    short dDrawingDir;
    double dfExtents; // TODO: meaning unknown
    double dfExtentsWidth; // TODO: meaning unknown
    string sTextValue;
    short dLineSpacingStyle;
    double dLineSpacingFactor;
    bool bUnknownBit;
    long dBackgroundFlags;
    long dBackgroundScaleFactor;
    short dBackgroundColor;
    long dBackgroundTransparency;
    CADHandle hStyle;
};

typedef struct _linestyle
{
    short          nNumSegParms;
    vector<double> adfSegparms;
    short          nAreaFillParms;
    vector<double> adfAreaFillParameters;
} CADLineStyle;

typedef struct _mlinevertex
{
    CADVector vertPosition;
    CADVector vectDirection;
    CADVector vectMIterDirection;
    vector < CADLineStyle > astLStyles;
} CADMLineVertex;

class CADMLineObject : public CADEntityObject
{
public:
    CADMLineObject();

    double dfScale;
    unsigned char dJust;
    CADVector vertBasePoint;
    CADVector vectExtrusion;
    short dOpenClosed; // 1 open, 3 closed
    unsigned char nLinesInStyle;
    short nNumVertexes;

    vector < CADMLineVertex > avertVertexes;

    CADHandle hMLineStyle;
};

class CAD3DFaceObject : public CADEntityObject
{
public:
    CAD3DFaceObject();

    bool bHasNoFlagInd; // 2000+
    bool bZZero;
    vector < CADVector > avertCorners;
    short dInvisFlags;
};

class CADPolylinePFaceObject : public CADEntityObject
{
public:
    CADPolylinePFaceObject();

    short nNumVertexes;
    short nNumFaces;
    long   nObjectsOwned;
    vector < CADHandle > hVertexes; // content really depends on DWG version.
    CADHandle hSeqend;
};

#endif //CADOBJECTS_H
