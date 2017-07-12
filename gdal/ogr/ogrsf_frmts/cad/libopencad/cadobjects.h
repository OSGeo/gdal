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

/*
 * @brief Class which basically implements implements 3D vertex
 */
class OCAD_EXTERN CADVector
{
public:
    CADVector();
    CADVector( double dx, double dy );
    CADVector( double dx, double dy, double dz );
    CADVector( const CADVector& other );
    bool      operator==( const CADVector& second );
    CADVector& operator=( const CADVector& second );
    double getX() const;
    void   setX( double value );

    double getY() const;
    void   setY( double value );

    double getZ() const;
    void   setZ( double value );

    bool getBHasZ() const;
    void setBHasZ( bool value );

protected:
    inline static bool fcmp( double x, double y );
protected:
    double X;
    double Y;
    double Z;
    bool   bHasZ;
};

typedef struct _Eed
{
    short                 dLength = 0;
    CADHandle             hApplication;
    std::vector<unsigned char> acData;
} CADEed;

typedef std::vector<CADHandle> CADHandleArray;
typedef std::vector<CADEed>    CADEedArray;

/**
 * @brief The base CAD object class
 */
class OCAD_EXTERN CADObject
{
public:
    enum ObjectType
    {
        UNUSED               = 0x0,  // 0
        TEXT                 = 0x1,  // 1
        ATTRIB               = 0x2,  // 2
        ATTDEF               = 0x3,  // 3
        BLOCK                = 0x4,  // 4
        ENDBLK               = 0x5,  // 5
        SEQEND               = 0x6,  // 6
        INSERT               = 0x7,  // 7
        MINSERT1             = 0x8,  // 8
        MINSERT2             = 0x9,  // 9
        VERTEX2D             = 0x0A, // 10
        VERTEX3D             = 0x0B, // 11
        VERTEX_MESH          = 0x0C, // 12
        VERTEX_PFACE         = 0x0D, // 13
        VERTEX_PFACE_FACE    = 0x0E, // 14
        POLYLINE2D           = 0x0F, // 15
        POLYLINE3D           = 0x10, // 16
        ARC                  = 0x11, // 17
        CIRCLE               = 0x12, // 18
        LINE                 = 0x13, // 19
        DIMENSION_ORDINATE   = 0x14, // 20
        DIMENSION_LINEAR     = 0x15, // 21
        DIMENSION_ALIGNED    = 0x16, // 22
        DIMENSION_ANG_3PT    = 0x17, // 23
        DIMENSION_ANG_2LN    = 0x18, // 24
        DIMENSION_RADIUS     = 0x19, // 25
        DIMENSION_DIAMETER   = 0x1A, // 26
        POINT                = 0x1B, // 27
        FACE3D               = 0x1C, // 28
        POLYLINE_PFACE       = 0x1D, // 29
        POLYLINE_MESH        = 0x1E, // 30
        SOLID                = 0x1F, // 31
        TRACE                = 0x20, // 32
        SHAPE                = 0x21, // 33
        VIEWPORT             = 0x22, // 34
        ELLIPSE              = 0x23, // 35
        SPLINE               = 0x24, // 36
        REGION               = 0x25, // 37
        SOLID3D              = 0x26, // 38
        BODY                 = 0x27, // 39
        RAY                  = 0x28, // 40
        XLINE                = 0x29, // 41
        DICTIONARY           = 0x2A, // 42
        OLEFRAME             = 0x2B, // 43
        MTEXT                = 0x2C, // 44
        LEADER               = 0x2D, // 45
        TOLERANCE            = 0x2E, // 46
        MLINE                = 0x2F, // 47
        BLOCK_CONTROL_OBJ    = 0x30, // 48
        BLOCK_HEADER         = 0x31, // 49
        LAYER_CONTROL_OBJ    = 0x32, // 50
        LAYER                = 0x33, // 51
        STYLE_CONTROL_OBJ    = 0x34, // 52
        STYLE1               = 0x35, // 53
        STYLE2               = 0x36, // 54
        STYLE3               = 0x37, // 55
        LTYPE_CONTROL_OBJ    = 0x38, // 56
        LTYPE1               = 0x39, // 57
        LTYPE2               = 0x3A, // 58
        LTYPE3               = 0x3B, // 59
        VIEW_CONTROL_OBJ     = 0x3C, // 60
        VIEW                 = 0x3D, // 61
        UCS_CONTROL_OBJ      = 0x3E, // 62
        UCS                  = 0x3F, // 63
        VPORT_CONTROL_OBJ    = 0x40, // 64
        VPORT                = 0x41, // 65
        APPID_CONTROL_OBJ    = 0x42, // 66
        APPID                = 0x43, // 67
        DIMSTYLE_CONTROL_OBJ = 0x44, // 68
        DIMSTYLE             = 0x45, // 69
        VP_ENT_HDR_CTRL_OBJ  = 0x46, // 70
        VP_ENT_HDR           = 0x47, // 71
        GROUP                = 0x48, // 72
        MLINESTYLE           = 0x49, // 73
        OLE2FRAME            = 0x4A, // 74
        DUMMY                = 0x4B, // 75
        LONG_TRANSACTION     = 0x4C, // 76
        LWPOLYLINE           = 0x4D, // 77
        HATCH                = 0x4E, // 78
        XRECORD              = 0x4F, // 79
        ACDBPLACEHOLDER      = 0x50, // 80
        VBA_PROJECT          = 0x51, // 81
        LAYOUT               = 0x52, // 82
        // Codes below aren't fixed, libopencad uses it for reading, in writing it will be different!
        CELLSTYLEMAP         = 0x53, // 83
        DBCOLOR              = 0x54, // 84
        DICTIONARYVAR        = 0x55, // 85
        DICTIONARYWDFLT      = 0x56, // 86
        FIELD                = 0x57, // 87
        GROUP_UNFIXED        = 0x58, // 88
        HATCH_UNFIXED        = 0x59, // 89
        IDBUFFER             = 0x5A, // 90
        IMAGE                = 0x5B, // 91
        IMAGEDEF             = 0x5C, // 92
        IMAGEDEFREACTOR      = 0x5D, // 93
        LAYER_INDEX          = 0x5E, // 94
        LAYOUT_UNFIXED       = 0x5F, // 95
        LWPOLYLINE_UNFIXED   = 0x60, // 96
        MATERIAL             = 0x61, // 97
        MLEADER              = 0x62, // 98
        MLEADERSTYLE         = 0x63, // 99
        OLE2FRAME_UNFIXED    = 0x64, // 100
        PLACEHOLDER          = 0x65, // 101
        PLOTSETTINGS         = 0x66, // 102
        RASTERVARIABLES      = 0x67, // 103
        SCALE                = 0x68, // 104
        SORTENTSTABLE        = 0x69, // 105
        SPATIAL_FILTER       = 0x6A, // 106
        SPATIAL_INDEX        = 0x6B, // 107
        TABLEGEOMETRY        = 0x6C, // 108
        TABLESTYLES          = 0x6D, // 109
        VBA_PROJECT_UNFIXED  = 0x6E, // 110
        VISUALSTYLE          = 0x6F, // 111
        WIPEOUTVARIABLE      = 0x70, // 112
        XRECORD_UNFIXED      = 0x71, // 113
        WIPEOUT              = 0x72  // 114
    };

    virtual ~CADObject(){}

    ObjectType getType() const;
    long       getSize() const;

    void setSize( long value );

    short getCRC() const;
    void  setCRC(unsigned short value );

protected:
    long       size;
    ObjectType type;
    unsigned short CRC;

        explicit CADObject(ObjectType typeIn) : size(0), type(typeIn), CRC(0) {}
};

std::string getNameByType( CADObject::ObjectType eType );
bool   isCommonEntityType( short nType );
bool   isSupportedGeometryType( short nType );

/**
 * @brief The CADCommonED struct
 */
struct CADCommonED
{
    long        nObjectSizeInBits;
    CADHandle   hObjectHandle;
    CADEedArray aEED;

    bool         bGraphicsPresented;
    std::vector<char> abyGraphicsData;

    unsigned char bbEntMode;
    long          nNumReactors;

    bool bNoXDictionaryHandlePresent;
    bool bBinaryDataPresent;

    bool bIsByLayerLT; // R13-14 only

    bool  bNoLinks;
    short nCMColor;

    double        dfLTypeScale;
    unsigned char bbLTypeFlags;
    unsigned char bbPlotStyleFlags;
    char          bbMaterialFlags;
    char          nShadowFlags;

    short         nInvisibility;
    unsigned char nLineWeight;

    CADCommonED() :
        nObjectSizeInBits(0),
        bGraphicsPresented(false),
        bbEntMode(0),
        nNumReactors(0),
        bNoXDictionaryHandlePresent(false),
        bBinaryDataPresent(false),
        bIsByLayerLT(false),
        bNoLinks(false),
        nCMColor(0),
        dfLTypeScale(0.0),
        bbLTypeFlags(0),
        bbPlotStyleFlags(0),
        bbMaterialFlags(0),
        nShadowFlags(0),
        nInvisibility(0),
        nLineWeight(0)
    {
    }
};

/**
 * @brief The CADCommonEHD struct
 */
struct CADCommonEHD
{
    CADHandle      hOwner; // depends on entmode.
    CADHandleArray hReactors;
    CADHandle      hXDictionary;
    CADHandle      hLayer;
    CADHandle      hLType;

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
    explicit CADEntityObject(ObjectType typeIn): CADObject(typeIn) {}

    virtual ~CADEntityObject(){}
    struct CADCommonED  stCed;
    struct CADCommonEHD stChed;
};

/**
 * @brief The CAD Text Object class
 */
class CADTextObject : public CADEntityObject
{
public:
    CADTextObject();
    virtual ~CADTextObject(){}
    unsigned char DataFlags;
    double        dfElevation;
    CADVector     vertInsetionPoint;
    CADVector     vertAlignmentPoint;
    CADVector     vectExtrusion;
    double        dfThickness;
    double        dfObliqueAng;
    double        dfRotationAng;
    double        dfHeight;
    double        dfWidthFactor;
    std::string   sTextValue;
    short         dGeneration;
    short         dHorizAlign;
    short         dVertAlign;

    CADHandle hStyle;
};

/**
 * @brief The CAD Attribute Object class
 */
class CADAttribObject : public CADEntityObject
{
public:
    explicit CADAttribObject( ObjectType typeIn = ATTRIB );
    virtual ~CADAttribObject(){}
    unsigned char DataFlags;
    double        dfElevation;
    CADVector     vertInsetionPoint;
    CADVector     vertAlignmentPoint;
    CADVector     vectExtrusion;
    double        dfThickness;
    double        dfObliqueAng;
    double        dfRotationAng;
    double        dfHeight;
    double        dfWidthFactor;
    std::string   sTextValue;
    short         dGeneration;
    short         dHorizAlign;
    short         dVertAlign;
    char          dVersion; // R2010+
    std::string   sTag;
    short         nFieldLength;
    unsigned char nFlags;
    bool          bLockPosition;

    CADHandle hStyle;
};

/**
 * @brief The CAD Attribute definition Object class
 */
class CADAttdefObject : public CADAttribObject
{
public:
    CADAttdefObject();
    virtual ~CADAttdefObject(){}
    std::string sPrompt;
};

/**
 * @brief The CAD Block Object class
 */
class CADBlockObject : public CADEntityObject
{
public:
    CADBlockObject();
    virtual ~CADBlockObject(){}
    std::string sBlockName;
};

/**
 * @brief The CAD End block Object class
 */
//TODO: do we need this class? Maybe CADEntityObject enouth?
class CADEndblkObject : public CADEntityObject
{
public:
    CADEndblkObject();
    virtual ~CADEndblkObject(){}
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
    virtual ~CADSeqendObject(){}
    // it actually has nothing more thatn CED and CEHD.
};

/**
 * @brief The CADInsertObject class
 */
class CADInsertObject : public CADEntityObject
{
public:
    explicit CADInsertObject( ObjectType typeIn = INSERT );
    virtual ~CADInsertObject(){}
    CADVector vertInsertionPoint;
    CADVector vertScales;
    double    dfRotation;
    CADVector vectExtrusion;
    bool      bHasAttribs;
    long      nObjectsOwned;

    CADHandle      hBlockHeader;
    CADHandleArray hAttribs;
    CADHandle      hSeqend; // if bHasAttribs == true
};

/**
 * @brief The CADMInsertObject class
 */
class CADMInsertObject : public CADEntityObject
{
public:
    CADMInsertObject();
    virtual ~CADMInsertObject(){}
    CADVector vertInsertionPoint;
    CADVector vertScales;
    double    dfRotation;
    CADVector vectExtrusion;
    bool      bHasAttribs;
    long      nObjectsOwned;

    short nNumCols;
    short nNumRows;
    short nColSpacing;
    short nRowSpacing;

    CADHandle      hBlockHeader;
    CADHandleArray hAtrribs;
    CADHandle      hSeqend; // if bHasAttribs == true
};

/**
 * @brief The CADVertex2DObject class
 */
class CADVertex2DObject : public CADEntityObject
{
public:
    CADVertex2DObject();
    virtual ~CADVertex2DObject(){}
    CADVector vertPosition; // Z must be taken from 2d polyline elevation.
    double    dfStartWidth;
    double    dfEndWidth;
    double    dfBulge;
    long      nVertexID;
    double    dfTangentDir;

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
    virtual ~CADVertex3DObject(){}
    CADVector vertPosition;
};

/**
 * @brief The CADVertexMesh class
 */
class CADVertexMeshObject : public CADEntityObject
{
public:
    CADVertexMeshObject();
    virtual ~CADVertexMeshObject(){}
    CADVector vertPosition;
};

/**
 * @brief The CADVertexPFaceObject class
 */
class CADVertexPFaceObject : public CADEntityObject
{
public:
    CADVertexPFaceObject();
    virtual ~CADVertexPFaceObject(){}
    CADVector vertPosition;
};

/**
 * @brief The CADVertexPFaceFaceObject class
 */
class CADVertexPFaceFaceObject : public CADEntityObject
{
public:
    CADVertexPFaceFaceObject();
    virtual ~CADVertexPFaceFaceObject(){}
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
    virtual ~CADPolyline2DObject(){}
    short     dFlags;
    short     dCurveNSmoothSurfType;
    double    dfStartWidth;
    double    dfEndWidth;
    double    dfThickness;
    double    dfElevation;
    CADVector vectExtrusion;

    long nObjectsOwned;

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
    virtual ~CADPolyline3DObject(){}
    unsigned char SplinedFlags;
    unsigned char ClosedFlags;

    long nObjectsOwned;

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
    virtual ~CADArcObject(){}
    CADVector vertPosition;
    double    dfRadius;
    double    dfThickness;
    CADVector vectExtrusion;
    double    dfStartAngle;
    double    dfEndAngle;
};

/**
 * @brief The CADCircleObject class
 */
class CADCircleObject : public CADEntityObject
{
public:
    CADCircleObject();
    virtual ~CADCircleObject(){}
    CADVector vertPosition;
    double    dfRadius;
    double    dfThickness;
    CADVector vectExtrusion;
};

/**
 * @brief The CADLineObject class
 */
class CADLineObject : public CADEntityObject
{
public:
    CADLineObject();
    virtual ~CADLineObject(){}
    CADVector vertStart;
    CADVector vertEnd;
    double    dfThickness;
    CADVector vectExtrusion;
};

/**
 * @brief The CADBlockControlObject class
 */
class CADBlockControlObject : public CADObject
{
public:
    CADBlockControlObject();
    virtual ~CADBlockControlObject(){}
    long           nObjectSizeInBits;
    CADHandle      hObjectHandle;
    CADEedArray    aEED;
    long           nNumReactors;
    bool           bNoXDictionaryPresent;
    long           nNumEntries; // doesn't count MODELSPACE and PAPERSPACE
    CADHandle      hNull;
    CADHandle      hXDictionary;
    CADHandleArray hBlocks; // ends with modelspace and paperspace handles.
};

/**
 * @brief The CADBlockHeaderObject class
 */
class CADBlockHeaderObject : public CADObject
{
public:
    CADBlockHeaderObject();
    virtual ~CADBlockHeaderObject(){}
    long                  nObjectSizeInBits;
    CADHandle             hObjectHandle;
    CADEedArray           aEED;
    long                  nNumReactors;
    bool                  bNoXDictionaryPresent;
    std::string           sEntryName;
    bool                  b64Flag;
    short                 dXRefIndex;
    bool                  bXDep;
    bool                  bAnonymous;
    bool                  bHasAtts;
    bool                  bBlkisXRef;
    bool                  bXRefOverlaid;
    bool                  bLoadedBit;
    long                  nOwnedObjectsCount;
    CADVector             vertBasePoint;
    std::string           sXRefPName;
    std::vector<unsigned char> adInsertCount; // TODO: ???
    std::string           sBlockDescription;
    long                  nSizeOfPreviewData;
    std::vector<unsigned char> abyBinaryPreviewData;
    short                 nInsertUnits;
    bool                  bExplodable;
    char                  dBlockScaling;
    CADHandle             hBlockControl;
    std::vector<CADHandle> hReactors;
    CADHandle             hXDictionary;
    CADHandle             hNull;
    CADHandle             hBlockEntity;
    CADHandleArray        hEntities;
    CADHandle             hEndBlk;
    CADHandleArray        hInsertHandles;
    CADHandle             hLayout;
};

/**
 * @brief The CADLayerControlObject class
 */
class CADLayerControlObject : public CADObject
{
public:
    CADLayerControlObject();
    virtual ~CADLayerControlObject(){}

    long           nObjectSizeInBits;
    CADHandle      hObjectHandle;
    CADEedArray    aEED;
    long           nNumReactors;
    bool           bNoXDictionaryPresent;
    long           nNumEntries; // counts layer "0"
    CADHandle      hNull;
    CADHandle      hXDictionary;
    CADHandleArray hLayers;
};

/**
 * @brief The CADLayerObject class
 */
class CADLayerObject : public CADObject
{
public:
    CADLayerObject();
    virtual ~CADLayerObject(){}

    long        nObjectSizeInBits;
    CADHandle   hObjectHandle;
    CADEedArray aEED;
    long        nNumReactors;
    bool        bNoXDictionaryPresent;
    std::string sLayerName;
    bool        b64Flag;
    short       dXRefIndex;
    bool        bXDep;
    bool        bFrozen;
    bool        bOn;
    bool        bFrozenInNewVPORT;
    bool        bLocked;
    bool        bPlottingFlag;
    short       dLineWeight;
    short       dCMColor;

    CADHandle      hLayerControl;
    CADHandleArray hReactors;
    CADHandle      hXDictionary;
    CADHandle      hExternalRefBlockHandle;
    CADHandle      hPlotStyle;
    CADHandle      hMaterial;
    CADHandle      hLType;
    CADHandle      hUnknownHandle;
};

/**
 * @brief The CADLineTypeControlObject class
 */
class CADLineTypeControlObject : public CADObject
{
public:
    CADLineTypeControlObject();
    virtual ~CADLineTypeControlObject(){}

    long           nObjectSizeInBits;
    CADHandle      hObjectHandle;
    CADEedArray    aEED;
    long           nNumReactors;
    bool           bNoXDictionaryPresent;
    long           nNumEntries; // doesn't count BYBLOCK / BYLAYER.
    CADHandle      hNull;
    CADHandle      hXDictionary;
    CADHandleArray hLTypes;
};

typedef struct _dash
{
    double dfLength;
    short  dComplexShapecode;
    double dfXOffset;
    double dfYOffset;
    double dfScale;
    double dfRotation;
    short  dShapeflag;
} CADDash;

/**
 * @brief The CADLineTypeObject class
 */
class CADLineTypeObject : public CADObject
{
public:
    CADLineTypeObject();
    virtual ~CADLineTypeObject(){}

    long                  nObjectSizeInBits;
    CADHandle             hObjectHandle;
    CADEedArray           aEED;
    long                  nNumReactors;
    bool                  bNoXDictionaryPresent;
    std::string           sEntryName;
    bool                  b64Flag;
    short                 dXRefIndex;
    bool                  bXDep;
    std::string           sDescription;
    double                dfPatternLen;
    unsigned char         dAlignment;
    unsigned char         nNumDashes;
    std::vector<CADDash>  astDashes;
    std::vector<unsigned char> abyTextArea; // TODO: what is it?
    CADHandle             hLTControl;
    CADHandleArray        hReactors;
    CADHandle             hXDictionary;
    CADHandle             hXRefBlock;
    CADHandleArray        hShapefiles; // TODO: one for each dash?
};

/**
 * @brief The CADPointObject class
 */
class CADPointObject : public CADEntityObject
{
public:
    CADPointObject();
    virtual ~CADPointObject(){}

    CADVector vertPosition;
    double    dfThickness;
    CADVector vectExtrusion;
    double    dfXAxisAng;
};

/**
 * @brief The CADSolidObject class
 */
class CADSolidObject : public CADEntityObject
{
public:
    CADSolidObject();
    virtual ~CADSolidObject(){}

    double            dfThickness;
    double            dfElevation;
    std::vector<CADVector> avertCorners;
    CADVector         vectExtrusion;
};

/**
 * @brief The CADEllipseObject class
 */
class CADEllipseObject : public CADEntityObject
{
public:
    CADEllipseObject();
    virtual ~CADEllipseObject(){}

    CADVector vertPosition;
    CADVector vectSMAxis;
    CADVector vectExtrusion;
    double    dfAxisRatio;
    double    dfBegAngle;
    double    dfEndAngle;
};

/**
 * @brief The CADRayObject class
 */
class CADRayObject : public CADEntityObject
{
public:
    CADRayObject();
    virtual ~CADRayObject(){}

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
    virtual ~CADXLineObject(){}

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
    virtual ~CADDictionaryObject(){}

    long          nObjectSizeInBits;
    CADHandle     hObjectHandle;
    CADEedArray   aEED;
    long          nNumReactors;
    bool          bNoXDictionaryPresent;
    long          nNumItems;
    short         dCloningFlag;
    unsigned char dHardOwnerFlag;

    std::vector<std::string> sItemNames;

    CADHandle      hParentHandle;
    CADHandleArray hReactors;
    CADHandle      hXDictionary;
    CADHandleArray hItemHandles;
};

/**
 * @brief The CADLWPolylineObject class
 */
class CADLWPolylineObject : public CADEntityObject
{
public:
    CADLWPolylineObject();
    virtual ~CADLWPolylineObject(){}

    bool                         bClosed;
    double                       dfConstWidth;
    double                       dfElevation;
    double                       dfThickness;
    CADVector                    vectExtrusion;
    std::vector<CADVector>            avertVertexes;
    std::vector<double>               adfBulges;
    std::vector<short>                adVertexesID;
    std::vector<std::pair<double, double>> astWidths; // start, end.
};

/**
 * @brief The CADSplineObject class
 */
class CADSplineObject : public CADEntityObject
{
public:
    CADSplineObject();
    virtual ~CADSplineObject(){}

    long dScenario;
    long dSplineFlags; // 2013+
    long dKnotParameter; // 2013+

    long      dDegree;
    double    dfFitTol;
    CADVector vectBegTangDir;
    CADVector vectEndTangDir;
    long      nNumFitPts;

    bool   bRational;
    bool   bClosed;
    bool   bPeriodic;
    double dfKnotTol;
    double dfCtrlTol;
    long   nNumKnots;
    long   nNumCtrlPts;
    bool   bWeight;

    std::vector<double>    adfKnots;
    std::vector<double>    adfCtrlPointsWeight;
    std::vector<CADVector> avertCtrlPoints;
    std::vector<CADVector> averFitPoints;
};

/**
 * @brief Common Dimensional Data structure
 */
struct CADCommonDimensionData
{
    char          dVersion;
    CADVector     vectExtrusion;
    CADVector     vertTextMidPt;
    double        dfElevation;
    unsigned char dFlags;
    std::string   sUserText;
    double        dfTextRotation;
    double        dfHorizDir;
    double        dfInsXScale;
    double        dfInsYScale;
    double        dfInsZScale;
    double        dfInsRotation;

    short  dAttachmentPoint;
    short  dLineSpacingStyle;
    double dfLineSpacingFactor;
    double dfActualMeasurement;

    bool bUnknown;
    bool bFlipArrow1;
    bool bFlipArrow2;

    CADVector vert12Pt;

    CADCommonDimensionData() :
        dVersion(0),
        dfElevation(0.0),
        dFlags(0),
        dfTextRotation(0.0),
        dfHorizDir(0.0),
        dfInsXScale(0.0),
        dfInsYScale(0.0),
        dfInsZScale(0.0),
        dfInsRotation(0.0),
        dAttachmentPoint(0),
        dLineSpacingStyle(0),
        dfLineSpacingFactor(0.0),
        dfActualMeasurement(0.0),
        bUnknown(false),
        bFlipArrow1(false),
        bFlipArrow2(false)
    {
    }
};

/**
 * @brief The CADDimensionObject class
 */
class CADDimensionObject : public CADEntityObject
{
public:
    explicit CADDimensionObject( ObjectType typeIn ) : CADEntityObject(typeIn) {}
    virtual ~CADDimensionObject(){}
    CADCommonDimensionData cdd;
    CADVector              vert10pt;
    CADHandle              hDimstyle;
    CADHandle              hAnonymousBlock;
};

/**
 * @brief The CADDimensionOrdinateObject class
 */
class CADDimensionOrdinateObject : public CADDimensionObject
{
public:
    CADDimensionOrdinateObject();
    virtual ~CADDimensionOrdinateObject(){}
    CADVector     vert13pt, vert14pt;
    unsigned char Flags2;
};

/**
 * @brief The CADDimensionLinearObject class
 */
class CADDimensionLinearObject : public CADDimensionObject
{
public:
    CADDimensionLinearObject();
    virtual ~CADDimensionLinearObject(){}
    CADVector vert13pt, vert14pt;

    double dfExtLnRot;
    double dfDimRot;
};

/**
 * @brief The CADDimensionAlignedObject class
 */
class CADDimensionAlignedObject : public CADDimensionObject
{
public:
    CADDimensionAlignedObject();
    virtual ~CADDimensionAlignedObject(){}
    CADVector vert13pt, vert14pt;

    double dfExtLnRot;
};

/**
 * @brief The CADDimensionAngular3PtObject class
 */
class CADDimensionAngular3PtObject : public CADDimensionObject
{
public:
    explicit CADDimensionAngular3PtObject(ObjectType typeIn = DIMENSION_ANG_3PT);
    virtual ~CADDimensionAngular3PtObject(){}
    CADVector vert13pt, vert14pt;
    CADVector vert15pt;
};

/**
 * @brief The CADDimensionAngular2LnObject class
 */
class CADDimensionAngular2LnObject : public CADDimensionAngular3PtObject
{
public:
    CADDimensionAngular2LnObject();
    virtual ~CADDimensionAngular2LnObject(){}

    CADVector vert16pt;
};

/**
 * @brief The CADDimensionRadiusObject class
 */
class CADDimensionRadiusObject : public CADDimensionObject
{
public:
    explicit CADDimensionRadiusObject(ObjectType typeIn = DIMENSION_RADIUS);
    virtual ~CADDimensionRadiusObject(){}

    CADVector vert15pt;
    double    dfLeaderLen;
};

/**
 * @brief The CADDimensionDiameterObject class
 */
class CADDimensionDiameterObject : public CADDimensionRadiusObject
{
public:
    CADDimensionDiameterObject();
    virtual ~CADDimensionDiameterObject(){}
};

/**
 * @brief The CADImageObject class
 */
class CADImageObject : public CADEntityObject
{
public:
    CADImageObject();
    virtual ~CADImageObject(){}

    long      dClassVersion;
    CADVector vertInsertion;
    CADVector vectUDirection;
    CADVector vectVDirection;
    double    dfSizeX;
    double    dfSizeY;
    /*  display properties (bit coded), 1==show image,
        2==show image when not aligned with screen, 4==use
        clipping boundary, 8==transparency on */
    short     dDisplayProps;

    bool          bClipping;
    unsigned char dBrightness;
    unsigned char dContrast;
    unsigned char dFade;
    bool          bClipMode; // R2010+

    short dClipBoundaryType;

    long              nNumberVertexesInClipPolygon;
    std::vector<CADVector> avertClippingPolygonVertexes;

    CADHandle hImageDef;
    CADHandle hImageDefReactor;
};

/**
 * @brief The CADImageDefReactorObject class
 */
class CADImageDefReactorObject : public CADObject
{
public:
    explicit CADImageDefReactorObject(ObjectType typeIn = IMAGEDEFREACTOR);
    virtual ~CADImageDefReactorObject(){}

    long              nObjectSizeInBits;
    CADHandle         hObjectHandle;
    std::vector<CADEed>    aEED;
    long              nNumReactors;
    bool              bNoXDictionaryPresent;
    long              dClassVersion;
    CADHandle         hParentHandle;
    std::vector<CADHandle> hReactors;
    CADHandle         hXDictionary;
};

/**
 * @brief The CADImageDefObject class
 */
class CADImageDefObject : public CADImageDefReactorObject
{
public:
    CADImageDefObject();
    virtual ~CADImageDefObject(){}

    double        dfXImageSizeInPx;
    double        dfYImageSizeInPx;
    std::string   sFilePath;
    bool          bIsLoaded;
    unsigned char dResUnits; // 0 == none, 2 == centimeters, 5 == inches
    double        dfXPixelSize; // size of 1 pixel in autocad units
    double        dfYPixelSize;
};

/**
 * @brief The CADMTextObject class
 */
class CADMTextObject : public CADEntityObject
{
public:
    CADMTextObject();
    virtual ~CADMTextObject(){}

    CADVector vertInsertionPoint;
    CADVector vectExtrusion;
    CADVector vectXAxisDir;
    double    dfRectWidth;
    double    dfTextHeight;
    short     dAttachment; // TODO: meaning unknown
    short     dDrawingDir;
    double    dfExtents; // TODO: meaning unknown
    double    dfExtentsWidth; // TODO: meaning unknown
    std::string sTextValue;
    short     dLineSpacingStyle;
    double    dLineSpacingFactor;
    bool      bUnknownBit;
    long      dBackgroundFlags;
    long      dBackgroundScaleFactor;
    short     dBackgroundColor;
    long      dBackgroundTransparency;
    CADHandle hStyle;
};

/**
 * @brief Linestyle data structure
 */
typedef struct _linestyle
{
    short          nNumSegParms;
    std::vector<double> adfSegparms;
    short          nAreaFillParms;
    std::vector<double> adfAreaFillParameters;
} CADLineStyle;

/**
 * @brief MLine vertex data structure
 */
typedef struct _mlinevertex
{
    CADVector            vertPosition;
    CADVector            vectDirection;
    CADVector            vectMIterDirection;
    std::vector<CADLineStyle> astLStyles;
} CADMLineVertex;

/**
 * @brief The CADMLineObject class
 */
class CADMLineObject : public CADEntityObject
{
public:
    CADMLineObject();
    virtual ~CADMLineObject(){}

    double        dfScale;
    unsigned char dJust;
    CADVector     vertBasePoint;
    CADVector     vectExtrusion;
    short         dOpenClosed; // 1 open, 3 closed
    unsigned char nLinesInStyle;
    short         nNumVertexes;

    std::vector<CADMLineVertex> avertVertexes;

    CADHandle hMLineStyle;
};

/**
 * @brief The CAD3DFaceObject class
 */
class CAD3DFaceObject : public CADEntityObject
{
public:
    CAD3DFaceObject();
    virtual ~CAD3DFaceObject(){}

    bool              bHasNoFlagInd; // 2000+
    bool              bZZero;
    std::vector<CADVector> avertCorners;
    short             dInvisFlags;
};

/**
 * @brief The CADPolylinePFaceObject class
 */
class CADPolylinePFaceObject : public CADEntityObject
{
public:
    CADPolylinePFaceObject();
    virtual ~CADPolylinePFaceObject(){}

    short             nNumVertexes;
    short             nNumFaces;
    long              nObjectsOwned;
    std::vector<CADHandle> hVertexes; // content really depends on DWG version.
    CADHandle         hSeqend;
};

#ifdef TODO
/**
 * @brief The CADHatchObject class TODO: not completed
 */
class CADHatchObject : public CADEntityObject
{
public:
    typedef struct
    {
        double dfUnknowndouble;
        short  dUnknownshort;
        long   dRGBColor;
        char   dIgnoredColorByte;
    } _gradient_color;

    CADHatchObject();
    virtual ~CADHatchObject(){}

    long                    dIsGradientFill; // 2004+
    long                    dReserved;
    double                  dfGradientAngle;
    double                  dfGradientShift;
    long                    dSingleColorGrad;
    double                  dfGradientTint;
    long                    nGradientColors;
    std::vector<_gradient_color> astGradientColors;

    std::string sGradientName;

    double    dfZCoord; // always X = Y = 0.0
    CADVector vectExtrusion;
    std::string    sHatchName;
    bool      bSolidFill;
    bool      bAssociative;
    long      nNumPaths;

    typedef struct
    {
        char      dPathTypeStatus;
        CADVector vectPt0;
        CADVector vectPt1;
    } _path_segment;
};
#endif

/**
 * @brief The CADXRecordObject class
 */
class CADXRecordObject : public CADObject
{
public:
    CADXRecordObject();
    virtual ~CADXRecordObject(){}
    long                                nObjectSizeInBits;
    CADHandle                           hObjectHandle;
    CADEedArray                         aEED;
    long                                nNumReactors;
    bool                                bNoXDictionaryPresent;
    long                                nNumDataBytes;
    std::vector<char>                   abyDataBytes;
    short                               dCloningFlag;
    std::vector<std::pair<short, std::vector<char> > > astXRecordData;
    CADHandle                           hParentHandle;
    std::vector<CADHandle>              hReactors;
    CADHandle                           hXDictionary;
    std::vector<CADHandle>              hObjIdHandles;
};

#endif //CADOBJECTS_H
