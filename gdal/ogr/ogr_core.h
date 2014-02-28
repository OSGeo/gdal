/******************************************************************************
 * $Id$
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  Define some core portability services for cross-platform OGR code.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 1999, Frank Warmerdam
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

#ifndef OGR_CORE_H_INCLUDED
#define OGR_CORE_H_INCLUDED

#include "cpl_port.h"
#include "gdal_version.h"

/**
 * \file
 *
 * Core portability services for cross-platform OGR code.
 */

/**
 * Simple container for a bounding region.
 */

#if defined(__cplusplus) && !defined(CPL_SUPRESS_CPLUSPLUS)
class CPL_DLL OGREnvelope
{
  public:
        OGREnvelope() : MinX(0.0), MaxX(0.0), MinY(0.0), MaxY(0.0)
        {
        }

        OGREnvelope(const OGREnvelope& oOther) :
            MinX(oOther.MinX),MaxX(oOther.MaxX), MinY(oOther.MinY), MaxY(oOther.MaxY)
        {
        }

    double      MinX;
    double      MaxX;
    double      MinY;
    double      MaxY;

/* See http://trac.osgeo.org/gdal/ticket/5299 for details on this pragma */
#if (__GNUC__ == 4 && __GNUC_MINOR__ >= 6 && !defined(_MSC_VER)) 
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wfloat-equal"
#endif
    int  IsInit() const { return MinX != 0 || MinY != 0 || MaxX != 0 || MaxY != 0; }

#if (__GNUC__ == 4 && __GNUC_MINOR__ >= 6 && !defined(_MSC_VER))
#pragma GCC diagnostic ignored pop
#endif

    void Merge( OGREnvelope const& sOther ) {
        if( IsInit() )
        {
            MinX = MIN(MinX,sOther.MinX);
            MaxX = MAX(MaxX,sOther.MaxX);
            MinY = MIN(MinY,sOther.MinY);
            MaxY = MAX(MaxY,sOther.MaxY);
        }
        else
        {
            MinX = sOther.MinX;
            MaxX = sOther.MaxX;
            MinY = sOther.MinY;
            MaxY = sOther.MaxY;
        }
    }
    void Merge( double dfX, double dfY ) {
        if( IsInit() )
        {
            MinX = MIN(MinX,dfX);
            MaxX = MAX(MaxX,dfX);
            MinY = MIN(MinY,dfY);
            MaxY = MAX(MaxY,dfY);
        }
        else
        {
            MinX = MaxX = dfX;
            MinY = MaxY = dfY;
        }
    }
    
    void Intersect( OGREnvelope const& sOther ) {
        if(Intersects(sOther))
        {
            if( IsInit() )
            {
                MinX = MAX(MinX,sOther.MinX);
                MaxX = MIN(MaxX,sOther.MaxX);
                MinY = MAX(MinY,sOther.MinY);
                MaxY = MIN(MaxY,sOther.MaxY);
            }
            else
            {
                MinX = sOther.MinX;
                MaxX = sOther.MaxX;
                MinY = sOther.MinY;
                MaxY = sOther.MaxY;
            }
        }
        else
        {
            MinX = 0;
            MaxX = 0;
            MinY = 0;
            MaxY = 0;
        }
    }
 
    int Intersects(OGREnvelope const& other) const
    {
        return MinX <= other.MaxX && MaxX >= other.MinX && 
               MinY <= other.MaxY && MaxY >= other.MinY;
    }

    int Contains(OGREnvelope const& other) const
    {
        return MinX <= other.MinX && MinY <= other.MinY &&
               MaxX >= other.MaxX && MaxY >= other.MaxY;
    }
};
#else
typedef struct
{
    double      MinX;
    double      MaxX;
    double      MinY;
    double      MaxY;
} OGREnvelope;
#endif


/**
 * Simple container for a bounding region in 3D.
 */

#if defined(__cplusplus) && !defined(CPL_SUPRESS_CPLUSPLUS)
class CPL_DLL OGREnvelope3D : public OGREnvelope
{
  public:
        OGREnvelope3D() : OGREnvelope(), MinZ(0.0), MaxZ(0.0)
        {
        }

        OGREnvelope3D(const OGREnvelope3D& oOther) :
                            OGREnvelope(oOther),
                            MinZ(oOther.MinZ), MaxZ(oOther.MaxZ)
        {
        }

    double      MinZ;
    double      MaxZ;

    int  IsInit() const { return MinX != 0 || MinY != 0 || MaxX != 0 || MaxY != 0 || MinZ != 0 || MaxZ != 0; }
    void Merge( OGREnvelope3D const& sOther ) {
        if( IsInit() )
        {
            MinX = MIN(MinX,sOther.MinX);
            MaxX = MAX(MaxX,sOther.MaxX);
            MinY = MIN(MinY,sOther.MinY);
            MaxY = MAX(MaxY,sOther.MaxY);
            MinZ = MIN(MinZ,sOther.MinZ);
            MaxZ = MAX(MaxZ,sOther.MaxZ);
        }
        else
        {
            MinX = sOther.MinX;
            MaxX = sOther.MaxX;
            MinY = sOther.MinY;
            MaxY = sOther.MaxY;
            MinZ = sOther.MinZ;
            MaxZ = sOther.MaxZ;
        }
    }
    void Merge( double dfX, double dfY, double dfZ ) {
        if( IsInit() )
        {
            MinX = MIN(MinX,dfX);
            MaxX = MAX(MaxX,dfX);
            MinY = MIN(MinY,dfY);
            MaxY = MAX(MaxY,dfY);
            MinZ = MIN(MinZ,dfZ);
            MaxZ = MAX(MaxZ,dfZ);
        }
        else
        {
            MinX = MaxX = dfX;
            MinY = MaxY = dfY;
            MinZ = MaxZ = dfZ;
        }
    }

    void Intersect( OGREnvelope3D const& sOther ) {
        if(Intersects(sOther))
        {
            if( IsInit() )
            {
                MinX = MAX(MinX,sOther.MinX);
                MaxX = MIN(MaxX,sOther.MaxX);
                MinY = MAX(MinY,sOther.MinY);
                MaxY = MIN(MaxY,sOther.MaxY);
                MinZ = MAX(MinZ,sOther.MinZ);
                MaxZ = MIN(MaxZ,sOther.MaxZ);
            }
            else
            {
                MinX = sOther.MinX;
                MaxX = sOther.MaxX;
                MinY = sOther.MinY;
                MaxY = sOther.MaxY;
                MinZ = sOther.MinZ;
                MaxZ = sOther.MaxZ;
            }
        }
        else
        {
            MinX = 0;
            MaxX = 0;
            MinY = 0;
            MaxY = 0;
            MinZ = 0;
            MaxZ = 0;
        }
    }

    int Intersects(OGREnvelope3D const& other) const
    {
        return MinX <= other.MaxX && MaxX >= other.MinX &&
               MinY <= other.MaxY && MaxY >= other.MinY &&
               MinZ <= other.MaxZ && MaxZ >= other.MinZ;
    }

    int Contains(OGREnvelope3D const& other) const
    {
        return MinX <= other.MinX && MinY <= other.MinY &&
               MaxX >= other.MaxX && MaxY >= other.MaxY &&
               MinZ <= other.MinZ && MaxZ >= other.MaxZ;
    }
};
#else
typedef struct
{
    double      MinX;
    double      MaxX;
    double      MinY;
    double      MaxY;
    double      MinZ;
    double      MaxZ;
} OGREnvelope3D;
#endif


CPL_C_START

void CPL_DLL *OGRMalloc( size_t );
void CPL_DLL *OGRCalloc( size_t, size_t );
void CPL_DLL *OGRRealloc( void *, size_t );
char CPL_DLL *OGRStrdup( const char * );
void CPL_DLL OGRFree( void * );

typedef int OGRErr;

#define OGRERR_NONE                0
#define OGRERR_NOT_ENOUGH_DATA     1    /* not enough data to deserialize */
#define OGRERR_NOT_ENOUGH_MEMORY   2
#define OGRERR_UNSUPPORTED_GEOMETRY_TYPE 3
#define OGRERR_UNSUPPORTED_OPERATION 4
#define OGRERR_CORRUPT_DATA        5
#define OGRERR_FAILURE             6
#define OGRERR_UNSUPPORTED_SRS     7
#define OGRERR_INVALID_HANDLE      8

typedef int     OGRBoolean;

/* -------------------------------------------------------------------- */
/*      ogr_geometry.h related definitions.                             */
/* -------------------------------------------------------------------- */

/**
 * List of well known binary geometry types.  These are used within the BLOBs
 * but are also returned from OGRGeometry::getGeometryType() to identify the
 * type of a geometry object.
 */
typedef enum 
{
    wkbUnknown = 0,         /**< unknown type, non-standard */
    wkbPoint = 1,           /**< 0-dimensional geometric object, standard WKB */
    wkbLineString = 2,      /**< 1-dimensional geometric object with linear
                             *   interpolation between Points, standard WKB */
    wkbPolygon = 3,         /**< planar 2-dimensional geometric object defined
                             *   by 1 exterior boundary and 0 or more interior
                             *   boundaries, standard WKB */
    wkbMultiPoint = 4,      /**< GeometryCollection of Points, standard WKB */
    wkbMultiLineString = 5, /**< GeometryCollection of LineStrings, standard WKB */
    wkbMultiPolygon = 6,    /**< GeometryCollection of Polygons, standard WKB */
    wkbGeometryCollection = 7, /**< geometric object that is a collection of 1
                                    or more geometric objects, standard WKB */
    wkbNone = 100,          /**< non-standard, for pure attribute records */
    wkbLinearRing = 101,    /**< non-standard, just for createGeometry() */
    wkbPoint25D = 0x80000001, /**< 2.5D extension as per 99-402 */
    wkbLineString25D = 0x80000002, /**< 2.5D extension as per 99-402 */
    wkbPolygon25D = 0x80000003, /**< 2.5D extension as per 99-402 */
    wkbMultiPoint25D = 0x80000004, /**< 2.5D extension as per 99-402 */
    wkbMultiLineString25D = 0x80000005, /**< 2.5D extension as per 99-402 */
    wkbMultiPolygon25D = 0x80000006, /**< 2.5D extension as per 99-402 */
    wkbGeometryCollection25D = 0x80000007 /**< 2.5D extension as per 99-402 */
} OGRwkbGeometryType;

/**
 * Output variants of WKB we support. 
 * 99-402 was a short-lived extension to SFSQL 1.1 that used a high-bit flag
 * to indicate the presence of Z coordiantes in a WKB geometry.
 * SQL/MM Part 3 and SFSQL 1.2 use offsets of 1000 (Z), 2000 (M) and 3000 (ZM)
 * to indicate the present of higher dimensional coordinates in a WKB geometry.
 */
typedef enum 
{
    wkbVariantOgc, /**< Old-style 99-402 extended dimension (Z) WKB types */
    wkbVariantIso  /**< SFSQL 1.2 and ISO SQL/MM Part 3 extended dimension (Z&M) WKB types */
} OGRwkbVariant;

#define wkb25DBit 0x80000000
#define wkbFlatten(x)  ((OGRwkbGeometryType) ((x) & (~wkb25DBit)))

#define ogrZMarker 0x21125711

const char CPL_DLL * OGRGeometryTypeToName( OGRwkbGeometryType eType );
OGRwkbGeometryType CPL_DLL OGRMergeGeometryTypes( OGRwkbGeometryType eMain,
                                                  OGRwkbGeometryType eExtra );

typedef enum 
{
    wkbXDR = 0,         /* MSB/Sun/Motoroloa: Most Significant Byte First   */
    wkbNDR = 1          /* LSB/Intel/Vax: Least Significant Byte First      */
} OGRwkbByteOrder;

#ifndef NO_HACK_FOR_IBM_DB2_V72
#  define HACK_FOR_IBM_DB2_V72
#endif

#ifdef HACK_FOR_IBM_DB2_V72
#  define DB2_V72_FIX_BYTE_ORDER(x) ((((x) & 0x31) == (x)) ? (OGRwkbByteOrder) ((x) & 0x1) : (x))
#  define DB2_V72_UNFIX_BYTE_ORDER(x) ((unsigned char) (OGRGeometry::bGenerate_DB2_V72_BYTE_ORDER ? ((x) | 0x30) : (x)))
#else
#  define DB2_V72_FIX_BYTE_ORDER(x) (x)
#  define DB2_V72_UNFIX_BYTE_ORDER(x) (x)
#endif

#define ALTER_NAME_FLAG            0x1
#define ALTER_TYPE_FLAG            0x2
#define ALTER_WIDTH_PRECISION_FLAG 0x4
#define ALTER_ALL_FLAG             (ALTER_NAME_FLAG | ALTER_TYPE_FLAG | ALTER_WIDTH_PRECISION_FLAG)

/************************************************************************/
/*                  ogr_feature.h related definitions.                  */
/************************************************************************/

/**
 * List of feature field types.  This list is likely to be extended in the
 * future ... avoid coding applications based on the assumption that all
 * field types can be known.
 */

typedef enum 
{
  /** Simple 32bit integer */                   OFTInteger = 0,
  /** List of 32bit integers */                 OFTIntegerList = 1,
  /** Double Precision floating point */        OFTReal = 2,
  /** List of doubles */                        OFTRealList = 3,
  /** String of ASCII chars */                  OFTString = 4,
  /** Array of strings */                       OFTStringList = 5,
  /** deprecated */                             OFTWideString = 6,
  /** deprecated */                             OFTWideStringList = 7,
  /** Raw Binary data */                        OFTBinary = 8,
  /** Date */                                   OFTDate = 9,
  /** Time */                                   OFTTime = 10,
  /** Date and Time */                          OFTDateTime = 11,
                                                OFTMaxType = 11
} OGRFieldType;

/**
 * Display justification for field values.
 */

typedef enum 
{
    OJUndefined = 0,
    OJLeft = 1,
    OJRight = 2
} OGRJustification;

#define OGRNullFID            -1
#define OGRUnsetMarker        -21121

/************************************************************************/
/*                               OGRField                               */
/************************************************************************/

/**
 * OGRFeature field attribute value union.
 */

typedef union {
    int         Integer;
    double      Real;
    char       *String;
    
    struct {
        int     nCount;
        int     *paList;
    } IntegerList;
    
    struct {
        int     nCount;
        double  *paList;
    } RealList;
    
    struct {
        int     nCount;
        char    **paList;
    } StringList;

    struct {
        int     nCount;
        GByte   *paData;
    } Binary;
    
    struct {
        int     nMarker1;
        int     nMarker2;
    } Set;

    struct {
        GInt16  Year;
        GByte   Month;
        GByte   Day;
        GByte   Hour;
        GByte   Minute;
        GByte   Second;
        GByte   TZFlag; /* 0=unknown, 1=localtime(ambiguous), 
                           100=GMT, 104=GMT+1, 80=GMT-5, etc */
    } Date;
} OGRField;

int CPL_DLL OGRParseDate( const char *pszInput, OGRField *psOutput, 
                          int nOptions );

/* -------------------------------------------------------------------- */
/*      Constants from ogrsf_frmts.h for capabilities.                  */
/* -------------------------------------------------------------------- */
#define OLCRandomRead          "RandomRead"
#define OLCSequentialWrite     "SequentialWrite"
#define OLCRandomWrite         "RandomWrite"
#define OLCFastSpatialFilter   "FastSpatialFilter"
#define OLCFastFeatureCount    "FastFeatureCount"
#define OLCFastGetExtent       "FastGetExtent"
#define OLCCreateField         "CreateField"
#define OLCDeleteField         "DeleteField"
#define OLCReorderFields       "ReorderFields"
#define OLCAlterFieldDefn      "AlterFieldDefn"
#define OLCTransactions        "Transactions"
#define OLCDeleteFeature       "DeleteFeature"
#define OLCFastSetNextByIndex  "FastSetNextByIndex"
#define OLCStringsAsUTF8       "StringsAsUTF8"
#define OLCIgnoreFields        "IgnoreFields"
#define OLCCreateGeomField     "CreateGeomField"

#define ODsCCreateLayer        "CreateLayer"
#define ODsCDeleteLayer        "DeleteLayer"
#define ODsCCreateGeomFieldAfterCreateLayer   "CreateGeomFieldAfterCreateLayer"

#define ODrCCreateDataSource   "CreateDataSource"
#define ODrCDeleteDataSource   "DeleteDataSource"


/************************************************************************/
/*                  ogr_featurestyle.h related definitions.             */
/************************************************************************/

/**
 * OGRStyleTool derived class types (returned by GetType()).
 */

typedef enum ogr_style_tool_class_id
{
    OGRSTCNone   = 0,
    OGRSTCPen    = 1,
    OGRSTCBrush  = 2,
    OGRSTCSymbol = 3,
    OGRSTCLabel  = 4,
    OGRSTCVector = 5
} OGRSTClassId;

/**
 * List of units supported by OGRStyleTools.
 */
typedef enum ogr_style_tool_units_id
{
    OGRSTUGround = 0,
    OGRSTUPixel  = 1,
    OGRSTUPoints = 2,
    OGRSTUMM     = 3,
    OGRSTUCM     = 4,
    OGRSTUInches = 5
} OGRSTUnitId;

/**
 * List of parameters for use with OGRStylePen.
 */
typedef enum ogr_style_tool_param_pen_id
{  
    OGRSTPenColor       = 0,                   
    OGRSTPenWidth       = 1,                   
    OGRSTPenPattern     = 2,
    OGRSTPenId          = 3,
    OGRSTPenPerOffset   = 4,
    OGRSTPenCap         = 5,
    OGRSTPenJoin        = 6,
    OGRSTPenPriority    = 7,
    OGRSTPenLast        = 8
              
} OGRSTPenParam;

/**
 * List of parameters for use with OGRStyleBrush.
 */
typedef enum ogr_style_tool_param_brush_id
{  
    OGRSTBrushFColor    = 0,                   
    OGRSTBrushBColor    = 1,                   
    OGRSTBrushId        = 2,
    OGRSTBrushAngle     = 3,                   
    OGRSTBrushSize      = 4,
    OGRSTBrushDx        = 5,
    OGRSTBrushDy        = 6,
    OGRSTBrushPriority  = 7,
    OGRSTBrushLast      = 8
              
} OGRSTBrushParam;


/**
 * List of parameters for use with OGRStyleSymbol.
 */
typedef enum ogr_style_tool_param_symbol_id
{  
    OGRSTSymbolId       = 0,
    OGRSTSymbolAngle    = 1,
    OGRSTSymbolColor    = 2,
    OGRSTSymbolSize     = 3,
    OGRSTSymbolDx       = 4,
    OGRSTSymbolDy       = 5,
    OGRSTSymbolStep     = 6,
    OGRSTSymbolPerp     = 7,
    OGRSTSymbolOffset   = 8,
    OGRSTSymbolPriority = 9,
    OGRSTSymbolFontName = 10,
    OGRSTSymbolOColor   = 11,
    OGRSTSymbolLast     = 12
              
} OGRSTSymbolParam;

/**
 * List of parameters for use with OGRStyleLabel.
 */
typedef enum ogr_style_tool_param_label_id
{  
    OGRSTLabelFontName  = 0,
    OGRSTLabelSize      = 1,
    OGRSTLabelTextString = 2,
    OGRSTLabelAngle     = 3,
    OGRSTLabelFColor    = 4,
    OGRSTLabelBColor    = 5,
    OGRSTLabelPlacement = 6,
    OGRSTLabelAnchor    = 7,
    OGRSTLabelDx        = 8,
    OGRSTLabelDy        = 9,
    OGRSTLabelPerp      = 10,
    OGRSTLabelBold      = 11,
    OGRSTLabelItalic    = 12,
    OGRSTLabelUnderline = 13,
    OGRSTLabelPriority  = 14,
    OGRSTLabelStrikeout = 15,
    OGRSTLabelStretch   = 16,
    OGRSTLabelAdjHor    = 17,
    OGRSTLabelAdjVert   = 18,
    OGRSTLabelHColor    = 19,
    OGRSTLabelOColor    = 20,
    OGRSTLabelLast      = 21
              
} OGRSTLabelParam;

/* ------------------------------------------------------------------- */
/*                        Version checking                             */
/* -------------------------------------------------------------------- */

/* Note to developers : please keep this section in sync with gdal.h */

#ifndef GDAL_VERSION_INFO_DEFINED
#define GDAL_VERSION_INFO_DEFINED
const char CPL_DLL * CPL_STDCALL GDALVersionInfo( const char * );
#endif

#ifndef GDAL_CHECK_VERSION

/** Return TRUE if GDAL library version at runtime matches nVersionMajor.nVersionMinor.

    The purpose of this method is to ensure that calling code will run with the GDAL
    version it is compiled for. It is primarly intented for external plugins.

    @param nVersionMajor Major version to be tested against
    @param nVersionMinor Minor version to be tested against
    @param pszCallingComponentName If not NULL, in case of version mismatch, the method
                                   will issue a failure mentionning the name of
                                   the calling component.
  */
int CPL_DLL CPL_STDCALL GDALCheckVersion( int nVersionMajor, int nVersionMinor,
                                          const char* pszCallingComponentName);

/** Helper macro for GDALCheckVersion */
#define GDAL_CHECK_VERSION(pszCallingComponentName) \
 GDALCheckVersion(GDAL_VERSION_MAJOR, GDAL_VERSION_MINOR, pszCallingComponentName)

#endif

CPL_C_END

#endif /* ndef OGR_CORE_H_INCLUDED */
