/**********************************************************************
 * $Id: mitab_priv.h,v 1.55 2010-01-07 20:39:12 aboudreault Exp $
 *
 * Name:     mitab_priv.h
 * Project:  MapInfo TAB Read/Write library
 * Language: C++
 * Purpose:  Header file containing private definitions for the library.
 * Author:   Daniel Morissette, dmorissette@dmsolutions.ca
 *
 **********************************************************************
 * Copyright (c) 1999-2003, Daniel Morissette
 * Copyright (c) 2014, Even Rouault <even.rouault at spatialys.com>
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
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER 
 * DEALINGS IN THE SOFTWARE.
 **********************************************************************
 *
 * $Log: mitab_priv.h,v $
 * Revision 1.55  2010-01-07 20:39:12  aboudreault
 * Added support to handle duplicate field names, Added validation to check if a field name start with a number (bug 2141)
 *
 * Revision 1.54  2008-11-27 20:50:23  aboudreault
 * Improved support for OGR date/time types. New Read/Write methods (bug 1948)
 * Added support of OGR date/time types for MIF features.
 *
 * Revision 1.53  2008/03/05 20:35:39  dmorissette
 * Replace MITAB 1.x SetFeature() with a CreateFeature() for V2.x (bug 1859)
 *
 * Revision 1.52  2008/02/20 21:35:30  dmorissette
 * Added support for V800 COLLECTION of large objects (bug 1496)
 *
 * Revision 1.51  2008/02/05 22:22:48  dmorissette
 * Added support for TAB_GEOM_V800_MULTIPOINT (bug 1496)
 *
 * Revision 1.50  2008/02/01 19:36:31  dmorissette
 * Initial support for V800 REGION and MULTIPLINE (bug 1496)
 *
 * Revision 1.49  2008/01/29 20:46:32  dmorissette
 * Added support for v9 Time and DateTime fields (byg 1754)
 *
 * Revision 1.48  2007/10/09 17:43:16  fwarmerdam
 * Remove static variables that interfere with reentrancy. (GDAL #1883)
 *
 * Revision 1.47  2007/06/12 12:50:39  dmorissette
 * Use Quick Spatial Index by default until bug 1732 is fixed (broken files
 * produced by current coord block splitting technique).
 *
 * Revision 1.46  2007/06/11 14:52:31  dmorissette
 * Return a valid m_nCoordDatasize value for Collection objects to prevent
 * trashing of collection data during object splitting (bug 1728)
 *
 * Revision 1.45  2007/03/21 21:15:56  dmorissette
 * Added SetQuickSpatialIndexMode() which generates a non-optimal spatial
 * index but results in faster write time (bug 1669)
 *
 * Revision 1.44  2007/02/22 18:35:53  dmorissette
 * Fixed problem writing collections where MITAB was sometimes trying to
 * read past EOF in write mode (bug 1657).
 *
 * Revision 1.43  2006/11/28 18:49:08  dmorissette
 * Completed changes to split TABMAPObjectBlocks properly and produce an
 * optimal spatial index (bug 1585)
 *
 * Revision 1.42  2006/11/20 20:05:58  dmorissette
 * First pass at improving generation of spatial index in .map file (bug 1585)
 * New methods for insertion and splittung in the spatial index are done.
 * Also implemented a method to dump the spatial index to .mif/.mid
 * Still need to implement splitting of TABMapObjectBlock to get optimal
 * results.
 *
 * Revision 1.41  2006/09/05 23:05:08  dmorissette
 * Added TABMAPFile::DumpSpatialIndex() (bug 1585)
 *
 * Revision 1.40  2005/10/06 19:15:31  dmorissette
 * Collections: added support for reading/writing pen/brush/symbol ids and
 * for writing collection objects to .TAB/.MAP (bug 1126)
 *
 * Revision 1.39  2005/10/04 15:44:31  dmorissette
 * First round of support for Collection objects. Currently supports reading
 * from .TAB/.MAP and writing to .MIF. Still lacks symbol support and write
 * support. (Based in part on patch and docs from Jim Hope, bug 1126)
 *
 * Revision 1.38  2005/03/22 23:24:54  dmorissette
 * Added support for datum id in .MAP header (bug 910)
 *
 * Revision 1.37  2004/06/30 20:29:04  dmorissette
 * Fixed refs to old address danmo@videotron.ca
 *
 * Revision 1.36  2003/08/12 23:17:21  dmorissette
 * Added reading of v500+ coordsys affine params (Anthony D. - Encom)
 *
 * Revision 1.35  2003/01/18 20:25:44  daniel
 * Increased MIDMAXCHAR value to 10000
 *
 * Revision 1.34  2002/04/25 16:05:24  julien
 * Disabled the overflow warning in SetCoordFilter() by adding bIgnoreOverflow
 * variable in Coordsys2Int of the TABMAPFile class and TABMAPHeaderBlock class
 *
 * Revision 1.33  2002/04/22 13:49:09  julien
 * Add EOF validation in MIDDATAFile::GetLastLine() (Bug 819)
 *
 * Revision 1.32  2002/03/26 19:27:43  daniel
 * Got rid of tabs in source
 *
 * Revision 1.31  2002/03/26 01:48:40  daniel
 * Added Multipoint object type (V650)
 *
 * Revision 1.30  2002/02/22 20:44:51  julien
 * Prevent infinite loop with TABRelation by suppress the m_poCurFeature object
 * from the class and setting it in the calling function and add GetFeature in
 * the class. (bug 706)
 *
 * Revision 1.29  2001/11/19 15:07:54  daniel
 * Added TABMAPObjNone to handle the case of TAB_GEOM_NONE
 *
 * Revision 1.28  2001/11/17 21:54:06  daniel
 * Made several changes in order to support writing objects in 16 bits 
 * coordinate format. New TABMAPObjHdr-derived classes are used to hold 
 * object info in mem until block is full.
 *
 * Revision 1.27  2001/09/18 20:33:52  warmerda
 * fixed case of spatial search on file with just one object block
 *
 * Revision 1.26  2001/09/14 03:23:55  warmerda
 * Substantial upgrade to support spatial queries using spatial indexes
 *
 * Revision 1.25  2001/05/01 18:28:10  daniel
 * Fixed default BRUSH, should be BRUSH(1,0,16777215).
 *
 * Revision 1.24  2001/05/01 03:39:51  daniel
 * Added SetLastPtr() to TABBinBlockManager.
 *
 * Revision 1.23  2001/03/15 03:57:51  daniel
 * Added implementation for new OGRLayer::GetExtent(), returning data MBR.
 *
 * Revision 1.22  2000/11/23 21:11:07  daniel
 * OOpps... VC++ didn't like the way TABPenDef, etc. were initialized
 *
 * Revision 1.21  2000/11/23 20:47:46  daniel
 * Use MI defaults for Pen, Brush, Font, Symbol instead of all zeros
 *
 * Revision 1.20  2000/11/15 04:13:50  daniel
 * Fixed writing of TABMAPToolBlock to allocate a new block when full
 *
 * Revision 1.19  2000/11/13 22:19:30  daniel
 * Added TABINDNode::UpdateCurChildEntry()
 *
 * Revision 1.18  2000/05/19 06:45:25  daniel
 * Modified generation of spatial index to split index nodes and produce a
 * more balanced tree.
 *
 * Revision 1.17  2000/03/01 00:30:03  daniel
 * Completed support for joined tables
 *
 * Revision 1.16  2000/02/28 16:53:23  daniel
 * Added support for indexed, unique, and for new V450 object types
 *
 * ...
 *
 * Revision 1.1  1999/07/12 04:18:25  daniel
 * Initial checkin
 *
 **********************************************************************/

#ifndef _MITAB_PRIV_H_INCLUDED_
#define _MITAB_PRIV_H_INCLUDED_

#include "cpl_conv.h"
#include "cpl_string.h"
#include "ogr_feature.h"

class TABFile;
class TABFeature;
class TABMAPToolBlock;
class TABMAPIndexBlock;

/*---------------------------------------------------------------------
 * Access mode: Read or Write
 *--------------------------------------------------------------------*/
typedef enum
{
    TABRead,
    TABWrite,
    TABReadWrite
} TABAccess;

/*---------------------------------------------------------------------
 * Supported .MAP block types (the first byte at the beginning of a block)
 *--------------------------------------------------------------------*/
#define TAB_RAWBIN_BLOCK        -1
#define TABMAP_HEADER_BLOCK     0
#define TABMAP_INDEX_BLOCK      1
#define TABMAP_OBJECT_BLOCK     2
#define TABMAP_COORD_BLOCK      3
#define TABMAP_GARB_BLOCK       4
#define TABMAP_TOOL_BLOCK       5
#define TABMAP_LAST_VALID_BLOCK_TYPE  5

/*---------------------------------------------------------------------
 * Drawing Tool types
 *--------------------------------------------------------------------*/
#define TABMAP_TOOL_PEN         1
#define TABMAP_TOOL_BRUSH       2
#define TABMAP_TOOL_FONT        3
#define TABMAP_TOOL_SYMBOL      4

/*---------------------------------------------------------------------
 * Limits related to .TAB version number.  If we pass any of those limits
 * then we have to use larger object types
 *--------------------------------------------------------------------*/
#define TAB_REGION_PLINE_300_MAX_VERTICES    32767

#define TAB_REGION_PLINE_450_MAX_SEGMENTS       32767
#define TAB_REGION_PLINE_450_MAX_VERTICES       1048575

#define TAB_MULTIPOINT_650_MAX_VERTICES         1048576

/* Use this macro to test whether the number of segments and vertices 
 * in this object exceeds the V450/650 limits and requires a V800 object 
 */
#define TAB_REGION_PLINE_REQUIRES_V800(numSegments, numVerticesTotal) \
    ((numSegments) > TAB_REGION_PLINE_450_MAX_SEGMENTS || \
     ((numSegments)*3 + numVerticesTotal) > TAB_REGION_PLINE_450_MAX_VERTICES )


/*---------------------------------------------------------------------
 * Codes for the known MapInfo Geometry types
 *--------------------------------------------------------------------*/
typedef enum
{
    TAB_GEOM_UNSET          = -1,

    TAB_GEOM_NONE           = 0,
    TAB_GEOM_SYMBOL_C       = 0x01,
    TAB_GEOM_SYMBOL         = 0x02,
    TAB_GEOM_LINE_C         = 0x04,
    TAB_GEOM_LINE           = 0x05,
    TAB_GEOM_PLINE_C        = 0x07,
    TAB_GEOM_PLINE          = 0x08,
    TAB_GEOM_ARC_C          = 0x0a,
    TAB_GEOM_ARC            = 0x0b,
    TAB_GEOM_REGION_C       = 0x0d,
    TAB_GEOM_REGION         = 0x0e,
    TAB_GEOM_TEXT_C         = 0x10,
    TAB_GEOM_TEXT           = 0x11,
    TAB_GEOM_RECT_C         = 0x13,
    TAB_GEOM_RECT           = 0x14,
    TAB_GEOM_ROUNDRECT_C    = 0x16,
    TAB_GEOM_ROUNDRECT      = 0x17,
    TAB_GEOM_ELLIPSE_C      = 0x19,
    TAB_GEOM_ELLIPSE        = 0x1a,
    TAB_GEOM_MULTIPLINE_C   = 0x25,
    TAB_GEOM_MULTIPLINE     = 0x26,
    TAB_GEOM_FONTSYMBOL_C   = 0x28, 
    TAB_GEOM_FONTSYMBOL     = 0x29,
    TAB_GEOM_CUSTOMSYMBOL_C = 0x2b,
    TAB_GEOM_CUSTOMSYMBOL   = 0x2c,
/* Version 450 object types: */
    TAB_GEOM_V450_REGION_C  = 0x2e,
    TAB_GEOM_V450_REGION    = 0x2f,
    TAB_GEOM_V450_MULTIPLINE_C = 0x31,
    TAB_GEOM_V450_MULTIPLINE   = 0x32,
/* Version 650 object types: */
    TAB_GEOM_MULTIPOINT_C   = 0x34,
    TAB_GEOM_MULTIPOINT     = 0x35,
    TAB_GEOM_COLLECTION_C   = 0x37,
    TAB_GEOM_COLLECTION     = 0x38,
/* Version 800 object types: */
    TAB_GEOM_UNKNOWN1_C     = 0x3a,    // ???
    TAB_GEOM_UNKNOWN1       = 0x3b,    // ???
    TAB_GEOM_V800_REGION_C  = 0x3d,
    TAB_GEOM_V800_REGION    = 0x3e,
    TAB_GEOM_V800_MULTIPLINE_C = 0x40,
    TAB_GEOM_V800_MULTIPLINE   = 0x41,
    TAB_GEOM_V800_MULTIPOINT_C = 0x43,
    TAB_GEOM_V800_MULTIPOINT   = 0x44,
    TAB_GEOM_V800_COLLECTION_C = 0x46,
    TAB_GEOM_V800_COLLECTION   = 0x47,
    TAB_GEOM_MAX_TYPE /* TODo: Does this need to be 0x80? */
} TABGeomType;

#define TAB_GEOM_GET_VERSION(nGeomType)                     \
    (((nGeomType) < TAB_GEOM_V450_REGION_C)  ? 300:         \
     ((nGeomType) < TAB_GEOM_MULTIPOINT_C)   ? 450:         \
     ((nGeomType) < TAB_GEOM_UNKNOWN1_C)     ? 650: 800 )


/*---------------------------------------------------------------------
 * struct TABMAPIndexEntry - Entries found in type 1 blocks of .MAP files
 *
 * We will use this struct to rebuild the geographic index in memory
 *--------------------------------------------------------------------*/
typedef struct TABMAPIndexEntry_t
{
    // These members refer to the info we find in the file
    GInt32      XMin;
    GInt32      YMin;
    GInt32      XMax;
    GInt32      YMax;
    GInt32      nBlockPtr;
}TABMAPIndexEntry;

#define TAB_MAX_ENTRIES_INDEX_BLOCK     ((512-4)/20)


/*---------------------------------------------------------------------
 * TABVertex 
 *--------------------------------------------------------------------*/
typedef struct TABVertex_t
{
    double x;
    double y;
} TABVertex;

/*---------------------------------------------------------------------
 * TABTableType - Attribute table format
 *--------------------------------------------------------------------*/
typedef enum
{
    TABTableNative,     // The default
    TABTableDBF,
    TABTableAccess
} TABTableType;

/*---------------------------------------------------------------------
 * TABFieldType - Native MapInfo attribute field types
 *--------------------------------------------------------------------*/
typedef enum
{
    TABFUnknown = 0,
    TABFChar,
    TABFInteger,
    TABFSmallInt,
    TABFDecimal,
    TABFFloat,
    TABFDate,
    TABFLogical,
    TABFTime,
    TABFDateTime
} TABFieldType;

#define TABFIELDTYPE_2_STRING(type)     \
   (type == TABFChar ? "Char" :         \
    type == TABFInteger ? "Integer" :   \
    type == TABFSmallInt ? "SmallInt" : \
    type == TABFDecimal ? "Decimal" :   \
    type == TABFFloat ? "Float" :       \
    type == TABFDate ? "Date" :         \
    type == TABFLogical ? "Logical" :   \
    type == TABFTime ? "Time" :         \
    type == TABFDateTime ? "DateTime" : \
    "Unknown field type"   )

/*---------------------------------------------------------------------
 * TABDATFieldDef
 *--------------------------------------------------------------------*/
typedef struct TABDATFieldDef_t
{
    char        szName[11];
    char        cType;
    GByte       byLength;
    GByte       byDecimals;

    TABFieldType eTABType;
} TABDATFieldDef;

/*---------------------------------------------------------------------
 * TABMAPCoordSecHdr
 * struct used in the TABMAPCoordBlock to store info about the coordinates
 * for a section of a PLINE MULTIPLE or a REGION.
 *--------------------------------------------------------------------*/
typedef struct TABMAPCoordSecHdr_t
{
    GInt32      numVertices;
    GInt32      numHoles;
    GInt32      nXMin;
    GInt32      nYMin;
    GInt32      nXMax;
    GInt32      nYMax;

    GInt32      nDataOffset;
    int         nVertexOffset;
} TABMAPCoordSecHdr;

/*---------------------------------------------------------------------
 * TABProjInfo
 * struct used to store the projection parameters from the .MAP header
 *--------------------------------------------------------------------*/
typedef struct TABProjInfo_t
{
    GByte       nProjId;           // See MapInfo Ref. Manual, App. F and G
    GByte       nEllipsoidId;
    GByte       nUnitsId;
    double      adProjParams[6];   // params in same order as in .MIF COORDSYS

    GInt16      nDatumId;       // Datum Id added in MapInfo 7.8+ (.map V500)
    double      dDatumShiftX;   // Before that, we had to always lookup datum
    double      dDatumShiftY;   // parameters to establish datum id
    double      dDatumShiftZ;
    double      adDatumParams[5];

    // Affine parameters only in .map version 500 and up
    GByte       nAffineFlag;    // 0=No affine param, 1=Affine params   
    GByte       nAffineUnits;
    double      dAffineParamA;  // Affine params
    double      dAffineParamB;
    double      dAffineParamC;
    double      dAffineParamD;
    double      dAffineParamE;
    double      dAffineParamF;

} TABProjInfo;


/*---------------------------------------------------------------------
 * TABPenDef - Pen definition information
 *--------------------------------------------------------------------*/
typedef struct TABPenDef_t
{
    GInt32      nRefCount;
    GByte       nPixelWidth;
    GByte       nLinePattern;
    int         nPointWidth;
    GInt32      rgbColor;
} TABPenDef;

/* MI Default = PEN(1,2,0) */
#define MITAB_PEN_DEFAULT {0, 1, 2, 0, 0x000000}

/*---------------------------------------------------------------------
 * TABBrushDef - Brush definition information
 *--------------------------------------------------------------------*/
typedef struct TABBrushDef_t
{
    GInt32      nRefCount;
    GByte       nFillPattern;
    GByte       bTransparentFill; // 1 = Transparent
    GInt32      rgbFGColor;
    GInt32      rgbBGColor;
} TABBrushDef;

/* MI Default = BRUSH(1,0,16777215) */
#define MITAB_BRUSH_DEFAULT {0, 1, 0, 0, 0xffffff}

/*---------------------------------------------------------------------
 * TABFontDef - Font Name information
 *--------------------------------------------------------------------*/
typedef struct TABFontDef_t
{
    GInt32      nRefCount;
    char        szFontName[33];
} TABFontDef;

/* MI Default = FONT("Arial",0,0,0) */
#define MITAB_FONT_DEFAULT {0, "Arial"}

/*---------------------------------------------------------------------
 * TABSymbolDef - Symbol definition information
 *--------------------------------------------------------------------*/
typedef struct TABSymbolDef_t
{
    GInt32      nRefCount;
    GInt16      nSymbolNo;
    GInt16      nPointSize;
    GByte       _nUnknownValue_;// Style???
    GInt32      rgbColor;
} TABSymbolDef;

/* MI Default = SYMBOL(35,0,12) */
#define MITAB_SYMBOL_DEFAULT {0, 35, 12, 0, 0x000000}

/*---------------------------------------------------------------------
 *                      class TABToolDefTable
 *
 * Class to handle the list of Drawing Tool Definitions for a dataset
 *
 * This class also contains methods to read tool defs from the file and
 * write them to the file.
 *--------------------------------------------------------------------*/

class TABToolDefTable
{
  protected:
    TABPenDef   **m_papsPen;
    int         m_numPen;
    int         m_numAllocatedPen;
    TABBrushDef **m_papsBrush;
    int         m_numBrushes;
    int         m_numAllocatedBrushes;
    TABFontDef  **m_papsFont;
    int         m_numFonts;
    int         m_numAllocatedFonts;
    TABSymbolDef **m_papsSymbol;
    int         m_numSymbols;
    int         m_numAllocatedSymbols;

  public:
    TABToolDefTable();
    ~TABToolDefTable();

    int         ReadAllToolDefs(TABMAPToolBlock *poToolBlock);
    int         WriteAllToolDefs(TABMAPToolBlock *poToolBlock);

    TABPenDef   *GetPenDefRef(int nIndex);
    int         AddPenDefRef(TABPenDef *poPenDef);
    int         GetNumPen();

    TABBrushDef *GetBrushDefRef(int nIndex);
    int         AddBrushDefRef(TABBrushDef *poBrushDef);
    int         GetNumBrushes();

    TABFontDef  *GetFontDefRef(int nIndex);
    int         AddFontDefRef(TABFontDef *poFontDef);
    int         GetNumFonts();

    TABSymbolDef *GetSymbolDefRef(int nIndex);
    int         AddSymbolDefRef(TABSymbolDef *poSymbolDef);
    int         GetNumSymbols();

    int         GetMinVersionNumber();
};

/*=====================================================================
          Classes to handle Object Headers inside TABMAPObjectBlocks
 =====================================================================*/

class TABMAPObjectBlock;
class TABMAPHeaderBlock;

class TABMAPObjHdr
{
  public:
    TABGeomType m_nType;
    GInt32      m_nId;
    GInt32      m_nMinX;  /* Object MBR */
    GInt32      m_nMinY;
    GInt32      m_nMaxX;
    GInt32      m_nMaxY;

    TABMAPObjHdr() {};
    virtual ~TABMAPObjHdr() {};

    static TABMAPObjHdr *NewObj(TABGeomType nNewObjType, GInt32 nId=0);
    static TABMAPObjHdr *ReadNextObj(TABMAPObjectBlock *poObjBlock,
                                     TABMAPHeaderBlock *poHeader);

    GBool       IsCompressedType();
    int         WriteObjTypeAndId(TABMAPObjectBlock *);
    void        SetMBR(GInt32 nMinX, GInt32 nMinY, GInt32 nMaxX, GInt32 mMaxY);

    virtual int WriteObj(TABMAPObjectBlock *) {return -1;};

//  protected:
    virtual int ReadObj(TABMAPObjectBlock *) {return -1;};
};

class TABMAPObjHdrWithCoord: public TABMAPObjHdr
{
  public:
    GInt32      m_nCoordBlockPtr;
    GInt32      m_nCoordDataSize;

    /* Eventually this class may have methods to help maintaining refs to
     * coord. blocks when splitting object blocks.
     */
};


class TABMAPObjNone: public TABMAPObjHdr
{
  public:

    TABMAPObjNone() {};
    virtual ~TABMAPObjNone() {};

    virtual int WriteObj(TABMAPObjectBlock *) {return 0;};

//  protected:
    virtual int ReadObj(TABMAPObjectBlock *) {return 0;};
};


class TABMAPObjPoint: public TABMAPObjHdr
{
  public:
    GInt32      m_nX;
    GInt32      m_nY;
    GByte       m_nSymbolId;

    TABMAPObjPoint() {};
    virtual ~TABMAPObjPoint() {};

    virtual int WriteObj(TABMAPObjectBlock *);

//  protected:
    virtual int ReadObj(TABMAPObjectBlock *);
};

class TABMAPObjFontPoint: public TABMAPObjPoint
{
  public:
    GByte       m_nPointSize;
    GInt16      m_nFontStyle;
    GByte       m_nR;
    GByte       m_nG;
    GByte       m_nB;
    GInt16      m_nAngle;  /* In tenths of degree */
    GByte       m_nFontId;

    TABMAPObjFontPoint() {};
    virtual ~TABMAPObjFontPoint() {};

    virtual int WriteObj(TABMAPObjectBlock *);

//  protected:
    virtual int ReadObj(TABMAPObjectBlock *);
};

class TABMAPObjCustomPoint: public TABMAPObjPoint
{
  public:
    GByte m_nUnknown_;
    GByte m_nCustomStyle;
    GByte m_nFontId;

    TABMAPObjCustomPoint() {};
    virtual ~TABMAPObjCustomPoint() {};

    virtual int WriteObj(TABMAPObjectBlock *);

//  protected:
    virtual int ReadObj(TABMAPObjectBlock *);
};


class TABMAPObjLine: public TABMAPObjHdr
{
  public:
    GInt32      m_nX1;
    GInt32      m_nY1;
    GInt32      m_nX2;
    GInt32      m_nY2;
    GByte       m_nPenId;

    TABMAPObjLine() {};
    virtual ~TABMAPObjLine() {};

    virtual int WriteObj(TABMAPObjectBlock *);

//  protected:
    virtual int ReadObj(TABMAPObjectBlock *);
};

class TABMAPObjPLine: public TABMAPObjHdrWithCoord
{
  public:
    GInt32      m_numLineSections;  /* MULTIPLINE/REGION only. Not in PLINE */
    GInt32      m_nLabelX;      /* Centroid/label location */
    GInt32      m_nLabelY;
    GInt32      m_nComprOrgX;   /* Present only in compressed coord. case */
    GInt32      m_nComprOrgY;
    GByte       m_nPenId;
    GByte       m_nBrushId;
    GBool       m_bSmooth;      /* TRUE if (m_nCoordDataSize & 0x80000000) */

    TABMAPObjPLine() {};
    virtual ~TABMAPObjPLine() {};

    virtual int WriteObj(TABMAPObjectBlock *);

//  protected:
    virtual int ReadObj(TABMAPObjectBlock *);
};

class TABMAPObjRectEllipse: public TABMAPObjHdr
{
  public:
    GInt32      m_nCornerWidth;   /* For rounded rect only */
    GInt32      m_nCornerHeight;
    GByte       m_nPenId;
    GByte       m_nBrushId;

    TABMAPObjRectEllipse() {};
    virtual ~TABMAPObjRectEllipse() {};

    virtual int WriteObj(TABMAPObjectBlock *);

//  protected:
    virtual int ReadObj(TABMAPObjectBlock *);
};

class TABMAPObjArc: public TABMAPObjHdr
{
  public:
    GInt32      m_nStartAngle;
    GInt32      m_nEndAngle;
    GInt32      m_nArcEllipseMinX;  /* MBR of the arc defining ellipse */
    GInt32      m_nArcEllipseMinY;  /* Only present in arcs            */
    GInt32      m_nArcEllipseMaxX;
    GInt32      m_nArcEllipseMaxY;
    GByte       m_nPenId;

    TABMAPObjArc() {};
    virtual ~TABMAPObjArc() {};

    virtual int WriteObj(TABMAPObjectBlock *);

//  protected:
    virtual int ReadObj(TABMAPObjectBlock *);
};


class TABMAPObjText: public TABMAPObjHdrWithCoord
{
  public:
    /* String and its len stored in the nCoordPtr and nCoordSize */

    GInt16      m_nTextAlignment;
    GInt32      m_nAngle;
    GInt16      m_nFontStyle;

    GByte       m_nFGColorR;
    GByte       m_nFGColorG;
    GByte       m_nFGColorB;
    GByte       m_nBGColorR;
    GByte       m_nBGColorG;
    GByte       m_nBGColorB;

    GInt32      m_nLineEndX;
    GInt32      m_nLineEndY;

    GInt32      m_nHeight;
    GByte       m_nFontId;

    GByte       m_nPenId;

    TABMAPObjText() {};
    virtual ~TABMAPObjText() {};

    virtual int WriteObj(TABMAPObjectBlock *);

//  protected:
    virtual int ReadObj(TABMAPObjectBlock *);
};


class TABMAPObjMultiPoint: public TABMAPObjHdrWithCoord
{
  public:
    GInt32      m_nNumPoints;
    GInt32      m_nComprOrgX;   /* Present only in compressed coord. case */
    GInt32      m_nComprOrgY;
    GByte       m_nSymbolId;
    GInt32      m_nLabelX;      /* Not sure if it's a label point, but */
    GInt32      m_nLabelY;      /* it's similar to what we find in PLINE */

    TABMAPObjMultiPoint() {};
    virtual ~TABMAPObjMultiPoint() {};

    virtual int WriteObj(TABMAPObjectBlock *);

//  protected:
    virtual int ReadObj(TABMAPObjectBlock *);
};

class TABMAPObjCollection: public TABMAPObjHdrWithCoord
{
  public:
    GInt32      m_nRegionDataSize;
    GInt32      m_nPolylineDataSize;
    GInt32      m_nMPointDataSize;
    GInt32      m_nComprOrgX;   /* Present only in compressed coord. case */
    GInt32      m_nComprOrgY;
    GInt32      m_nNumMultiPoints;
    GInt32      m_nNumRegSections;
    GInt32      m_nNumPLineSections;

    GByte       m_nMultiPointSymbolId;
    GByte       m_nRegionPenId;
    GByte       m_nRegionBrushId;
    GByte       m_nPolylinePenId;

    TABMAPObjCollection() {};
    virtual ~TABMAPObjCollection() 
    {}

    virtual int WriteObj(TABMAPObjectBlock *);

//  protected:
    virtual int ReadObj(TABMAPObjectBlock *);

  private:
    // private copy ctor and assignment operator to prevent shallow copying
    TABMAPObjCollection& operator=(const TABMAPObjCollection& rhs);
    TABMAPObjCollection(const TABMAPObjCollection& rhs);
};

/*=====================================================================
          Classes to handle .MAP files low-level blocks
 =====================================================================*/

typedef struct TABBlockRef_t
{
    GInt32                nBlockPtr;
    struct TABBlockRef_t *psPrev;
    struct TABBlockRef_t *psNext;
} TABBlockRef;

/*---------------------------------------------------------------------
 *                      class TABBinBlockManager
 *
 * This class is used to keep track of allocated blocks and is used
 * by various classes that need to allocate a new block in a .MAP file.
 *--------------------------------------------------------------------*/
class TABBinBlockManager
{
  protected:
    int         m_nBlockSize;
    GInt32      m_nLastAllocatedBlock;
    TABBlockRef *m_psGarbageBlocksFirst;
    TABBlockRef *m_psGarbageBlocksLast;
    char        m_szName[32]; /* for debug purposes */

  public:
    TABBinBlockManager(int nBlockSize=512);
    ~TABBinBlockManager();

    GInt32      AllocNewBlock(const char* pszReason = "");
    void        Reset();
    void        SetLastPtr(int nBlockPtr) {m_nLastAllocatedBlock=nBlockPtr; };

    void        PushGarbageBlockAsFirst(GInt32 nBlockPtr);
    void        PushGarbageBlockAsLast(GInt32 nBlockPtr);
    GInt32      GetFirstGarbageBlock();
    GInt32      PopGarbageBlock();
    
    void        SetName(const char* pszName);
};

/*---------------------------------------------------------------------
 *                      class TABRawBinBlock
 *
 * This is the base class used for all other data block types... it 
 * contains all the base functions to handle binary data.
 *--------------------------------------------------------------------*/

class TABRawBinBlock
{
  protected:
    VSILFILE    *m_fp;          /* Associated file handle               */
    TABAccess   m_eAccess;      /* Read/Write access mode               */

    int         m_nBlockType;

    GByte       *m_pabyBuf;     /* Buffer to contain the block's data    */
    int         m_nBlockSize;   /* Size of current block (and buffer)    */
    int         m_nSizeUsed;    /* Number of bytes used in buffer        */
    GBool       m_bHardBlockSize;/* TRUE=Blocks MUST always be nSize bytes  */
                                 /* FALSE=last block may be less than nSize */
    int         m_nFileOffset;  /* Location of current block in the file */
    int         m_nCurPos;      /* Next byte to read from m_pabyBuf[]    */
    int         m_nFirstBlockPtr;/* Size of file header when different from */
                                 /* block size (used by GotoByteInFile())   */
    int         m_nFileSize;

    int         m_bModified;     /* Used only to detect changes        */

  public:
    TABRawBinBlock(TABAccess eAccessMode = TABRead,
                   GBool bHardBlockSize = TRUE);
    virtual ~TABRawBinBlock();

    virtual int ReadFromFile(VSILFILE *fpSrc, int nOffset, int nSize = 512);
    virtual int CommitToFile();
    int         CommitAsDeleted(GInt32 nNextBlockPtr);

    virtual int InitBlockFromData(GByte *pabyBuf, 
                                  int nBlockSize, int nSizeUsed,
                                  GBool bMakeCopy = TRUE,
                                  VSILFILE *fpSrc = NULL, int nOffset = 0);
    virtual int InitNewBlock(VSILFILE *fpSrc, int nBlockSize, int nFileOffset=0);

    int         GetBlockType();
    virtual int GetBlockClass() { return TAB_RAWBIN_BLOCK; };

    GInt32      GetStartAddress() {return m_nFileOffset;};
#ifdef DEBUG
    virtual void Dump(FILE *fpOut = NULL);
#endif
    void        DumpBytes(GInt32 nValue, int nOffset=0, FILE *fpOut=NULL);

    int         GotoByteRel(int nOffset);
    int         GotoByteInBlock(int nOffset);
    int         GotoByteInFile(int nOffset, 
                               GBool bForceReadFromFile = FALSE,
                               GBool bOffsetIsEndOfData = FALSE);
    void        SetFirstBlockPtr(int nOffset);

    int         GetNumUnusedBytes();
    int         GetFirstUnusedByteOffset();
    int         GetCurAddress();

    virtual int ReadBytes(int numBytes, GByte *pabyDstBuf);
    GByte       ReadByte();
    GInt16      ReadInt16();
    GInt32      ReadInt32();
    float       ReadFloat();
    double      ReadDouble();

    virtual int WriteBytes(int nBytesToWrite, GByte *pBuf);
    int         WriteByte(GByte byValue);
    int         WriteInt16(GInt16 n16Value);
    int         WriteInt32(GInt32 n32Value);
    int         WriteFloat(float fValue);
    int         WriteDouble(double dValue);
    int         WriteZeros(int nBytesToWrite);
    int         WritePaddedString(int nFieldSize, const char *pszString);

    void        SetModifiedFlag(GBool bModified) {m_bModified=bModified;};

    // This semi-private method gives a direct access to the internal 
    // buffer... to be used with extreme care!!!!!!!!!
    GByte *     GetCurDataPtr() { return (m_pabyBuf + m_nCurPos); } ;
};


/*---------------------------------------------------------------------
 *                      class TABMAPHeaderBlock
 *
 * Class to handle Read/Write operation on .MAP Header Blocks 
 *--------------------------------------------------------------------*/

class TABMAPHeaderBlock: public TABRawBinBlock
{
    void        InitMembersWithDefaultValues();
    void        UpdatePrecision();

  protected:
    TABProjInfo m_sProj;

  public:
    TABMAPHeaderBlock(TABAccess eAccessMode = TABRead);
    ~TABMAPHeaderBlock();

    virtual int CommitToFile();

    virtual int InitBlockFromData(GByte *pabyBuf,
                                  int nBlockSize, int nSizeUsed,
                                  GBool bMakeCopy = TRUE,
                                  VSILFILE *fpSrc = NULL, int nOffset = 0);
    virtual int InitNewBlock(VSILFILE *fpSrc, int nBlockSize, int nFileOffset=0);

    virtual int GetBlockClass() { return TABMAP_HEADER_BLOCK; };

    int         Int2Coordsys(GInt32 nX, GInt32 nY, double &dX, double &dY);
    int         Coordsys2Int(double dX, double dY, GInt32 &nX, GInt32 &nY, 
                             GBool bIgnoreOverflow=FALSE);
    int         ComprInt2Coordsys(GInt32 nCenterX, GInt32 nCenterY, 
                                  int nDeltaX, int nDeltaY, 
                                  double &dX, double &dY);
    int         Int2CoordsysDist(GInt32 nX, GInt32 nY, double &dX, double &dY);
    int         Coordsys2IntDist(double dX, double dY, GInt32 &nX, GInt32 &nY);
    int         SetCoordsysBounds(double dXMin, double dYMin, 
                                  double dXMax, double dYMax);

    int         GetMapObjectSize(int nObjType);
    GBool       MapObjectUsesCoordBlock(int nObjType);

    int         GetProjInfo(TABProjInfo *psProjInfo);
    int         SetProjInfo(TABProjInfo *psProjInfo);

#ifdef DEBUG
    virtual void Dump(FILE *fpOut = NULL);
#endif

    // Instead of having over 30 get/set methods, we'll make all data 
    // members public and we will initialize them in the overloaded
    // LoadFromFile().  For this reason, this class should be used with care.

    GInt16      m_nMAPVersionNumber;
    GInt16      m_nBlockSize;
    
    double      m_dCoordsys2DistUnits;
    GInt32      m_nXMin;
    GInt32      m_nYMin;
    GInt32      m_nXMax;
    GInt32      m_nYMax;
    GBool       m_bIntBoundsOverflow;  // Set to TRUE if coordinates 
                                       // outside of bounds were written

    GInt32      m_nFirstIndexBlock;
    GInt32      m_nFirstGarbageBlock;
    GInt32      m_nFirstToolBlock;
    GInt32      m_numPointObjects;
    GInt32      m_numLineObjects;
    GInt32      m_numRegionObjects;
    GInt32      m_numTextObjects;
    GInt32      m_nMaxCoordBufSize;

    GByte       m_nDistUnitsCode;       // See Appendix F
    GByte       m_nMaxSpIndexDepth;
    GByte       m_nCoordPrecision;      // Num. decimal places on coord.
    GByte       m_nCoordOriginQuadrant;
    GByte       m_nReflectXAxisCoord;
    GByte       m_nMaxObjLenArrayId;     // See gabyObjLenArray[]
    GByte       m_numPenDefs;
    GByte       m_numBrushDefs;
    GByte       m_numSymbolDefs;
    GByte       m_numFontDefs;
    GInt16      m_numMapToolBlocks;

    double      m_XScale;
    double      m_YScale;
    double      m_XDispl;
    double      m_YDispl;
    double      m_XPrecision; // maximum achievable precision along X axis depending on bounds extent
    double      m_YPrecision; // maximum achievable precision along Y axis depending on bounds extent
};

/*---------------------------------------------------------------------
 *                      class TABMAPIndexBlock
 *
 * Class to handle Read/Write operation on .MAP Index Blocks (Type 01)
 *--------------------------------------------------------------------*/

class TABMAPIndexBlock: public TABRawBinBlock
{
  protected:
    int         m_numEntries;
    TABMAPIndexEntry m_asEntries[TAB_MAX_ENTRIES_INDEX_BLOCK];

    int         ReadNextEntry(TABMAPIndexEntry *psEntry);
    int         WriteNextEntry(TABMAPIndexEntry *psEntry);

    // Use these to keep track of current block's MBR
    GInt32      m_nMinX;
    GInt32      m_nMinY;
    GInt32      m_nMaxX;
    GInt32      m_nMaxY;

    TABBinBlockManager *m_poBlockManagerRef;

    // Info about child currently loaded
    TABMAPIndexBlock *m_poCurChild;
    int         m_nCurChildIndex;
    // Also need to know about its parent
    TABMAPIndexBlock *m_poParentRef;

    int         ReadAllEntries();

  public:
    TABMAPIndexBlock(TABAccess eAccessMode = TABRead);
    ~TABMAPIndexBlock();

    virtual int InitBlockFromData(GByte *pabyBuf,
                                  int nBlockSize, int nSizeUsed,
                                  GBool bMakeCopy = TRUE,
                                  VSILFILE *fpSrc = NULL, int nOffset = 0);
    virtual int InitNewBlock(VSILFILE *fpSrc, int nBlockSize, int nFileOffset=0);
    virtual int CommitToFile();

    virtual int GetBlockClass() { return TABMAP_INDEX_BLOCK; };

    void        UnsetCurChild();

    int         GetNumFreeEntries();
    int         GetNumEntries()         {return m_numEntries;};
    TABMAPIndexEntry *GetEntry( int iIndex );
    int         AddEntry(GInt32 XMin, GInt32 YMin,
                         GInt32 XMax, GInt32 YMax,
                         GInt32 nBlockPtr,
                         GBool bAddInThisNodeOnly=FALSE);
    int         GetCurMaxDepth();
    void        GetMBR(GInt32 &nXMin, GInt32 &nYMin, 
                       GInt32 &nXMax, GInt32 &nYMax);
    void        SetMBR(GInt32 nXMin, GInt32 nYMin, 
                       GInt32 nXMax, GInt32 nYMax);

    GInt32      GetNodeBlockPtr() { return GetStartAddress();};

    void        SetMAPBlockManagerRef(TABBinBlockManager *poBlockMgr);
    void        SetParentRef(TABMAPIndexBlock *poParent);
    void        SetCurChildRef(TABMAPIndexBlock *poChild, int nChildIndex);

    int         GetCurChildIndex() { return m_nCurChildIndex; }
    TABMAPIndexBlock *GetCurChild() { return m_poCurChild; }
    TABMAPIndexBlock *GetParentRef() { return m_poParentRef; } 

    int         SplitNode(GInt32 nNewEntryXMin, GInt32 nNewEntryYMin,
                          GInt32 nNewEntryXMax, GInt32 nNewEntryYMax);
    int         SplitRootNode(GInt32 nNewEntryXMin, GInt32 nNewEntryYMin,
                              GInt32 nNewEntryXMax, GInt32 nNewEntryYMax);
    void        UpdateCurChildMBR(GInt32 nXMin, GInt32 nYMin,
                                  GInt32 nXMax, GInt32 nYMax,
                                  GInt32 nBlockPtr);
    void        RecomputeMBR();
    int         InsertEntry(GInt32 XMin, GInt32 YMin,
                            GInt32 XMax, GInt32 YMax, GInt32 nBlockPtr);
    int         ChooseSubEntryForInsert(GInt32 nXMin, GInt32 nYMin,
                                        GInt32 nXMax, GInt32 nYMax);
    GInt32      ChooseLeafForInsert(GInt32 nXMin, GInt32 nYMin,
                                    GInt32 nXMax, GInt32 nYMax);
    int         UpdateLeafEntry(GInt32 nBlockPtr,
                                GInt32 nXMin, GInt32 nYMin,
                                GInt32 nXMax, GInt32 nYMax);
    int         GetCurLeafEntryMBR(GInt32 nBlockPtr,
                                   GInt32 &nXMin, GInt32 &nYMin,
                                   GInt32 &nXMax, GInt32 &nYMax);

    // Static utility functions for node splitting, also used by
    // the TABMAPObjectBlock class.
    static double ComputeAreaDiff(GInt32 nNodeXMin, GInt32 nNodeYMin,
                                  GInt32 nNodeXMax, GInt32 nNodeYMax,
                                  GInt32 nEntryXMin, GInt32 nEntryYMin,
                                  GInt32 nEntryXMax, GInt32 nEntryYMax);
    static int    PickSeedsForSplit(TABMAPIndexEntry *pasEntries,
                                    int numEntries,
                                    int nSrcCurChildIndex,
                                    GInt32 nNewEntryXMin, 
                                    GInt32 nNewEntryYMin,
                                    GInt32 nNewEntryXMax, 
                                    GInt32 nNewEntryYMax,
                                    int &nSeed1, int &nSeed2);
#ifdef DEBUG
    virtual void Dump(FILE *fpOut = NULL);
#endif

};

/*---------------------------------------------------------------------
 *                      class TABMAPObjectBlock
 *
 * Class to handle Read/Write operation on .MAP Object data Blocks (Type 02)
 *--------------------------------------------------------------------*/

class TABMAPObjectBlock: public TABRawBinBlock
{
  protected:
    int         m_numDataBytes; /* Excluding first 4 bytes header */
    GInt32      m_nFirstCoordBlock;
    GInt32      m_nLastCoordBlock;
    GInt32      m_nCenterX;
    GInt32      m_nCenterY;

    // In order to compute block center, we need to keep track of MBR
    GInt32      m_nMinX;
    GInt32      m_nMinY;
    GInt32      m_nMaxX;
    GInt32      m_nMaxY;

    // Keep track of current object either in read or read/write mode
    int         m_nCurObjectOffset; // -1 if there is no current object.
    int         m_nCurObjectId;     // -1 if there is no current object.
    TABGeomType m_nCurObjectType;   // TAB_GEOM_UNSET if there is no current object.
    
    int         m_bLockCenter;

  public:
    TABMAPObjectBlock(TABAccess eAccessMode = TABRead);
    ~TABMAPObjectBlock();

    virtual int CommitToFile();
    virtual int InitBlockFromData(GByte *pabyBuf,
                                  int nBlockSize, int nSizeUsed,
                                  GBool bMakeCopy = TRUE,
                                  VSILFILE *fpSrc = NULL, int nOffset = 0);
    virtual int InitNewBlock(VSILFILE *fpSrc, int nBlockSize, int nFileOffset=0);

    virtual int GetBlockClass() { return TABMAP_OBJECT_BLOCK; };

    virtual int ReadIntCoord(GBool bCompressed, GInt32 &nX, GInt32 &nY);
    int         WriteIntCoord(GInt32 nX, GInt32 nY, GBool bCompressed);
    int         WriteIntMBRCoord(GInt32 nXMin, GInt32 nYMin,
                                 GInt32 nXMax, GInt32 nYMax,
                                 GBool bCompressed);
    int         UpdateMBR(GInt32 nX, GInt32 nY);

    int         PrepareNewObject(TABMAPObjHdr *poObjHdr);
    int         CommitNewObject(TABMAPObjHdr *poObjHdr);

    void        AddCoordBlockRef(GInt32 nCoordBlockAddress);
    GInt32      GetFirstCoordBlockAddress() { return m_nFirstCoordBlock; }
    GInt32      GetLastCoordBlockAddress() { return m_nLastCoordBlock; }

    void        GetMBR(GInt32 &nXMin, GInt32 &nYMin, 
                       GInt32 &nXMax, GInt32 &nYMax);
    void        SetMBR(GInt32 nXMin, GInt32 nYMin, 
                       GInt32 nXMax, GInt32 nYMax);

    void        Rewind();
    void        ClearObjects();
    void        LockCenter();
    void        SetCenterFromOtherBlock(TABMAPObjectBlock* poOtherObjBlock);
    int         AdvanceToNextObject( TABMAPHeaderBlock * );
    int         GetCurObjectOffset() { return m_nCurObjectOffset; }
    int         GetCurObjectId() { return m_nCurObjectId; }
    TABGeomType GetCurObjectType() { return m_nCurObjectType; }

#ifdef DEBUG
    virtual void Dump(FILE *fpOut = NULL) { Dump(fpOut, FALSE); };
    void Dump(FILE *fpOut, GBool bDetails);
#endif

};

/*---------------------------------------------------------------------
 *                      class TABMAPCoordBlock
 *
 * Class to handle Read/Write operation on .MAP Coordinate Blocks (Type 03)
 *--------------------------------------------------------------------*/

class TABMAPCoordBlock: public TABRawBinBlock
{
  protected:
    int         m_numDataBytes; /* Excluding first 8 bytes header */
    GInt32      m_nNextCoordBlock;
    int         m_numBlocksInChain;

    GInt32      m_nComprOrgX;
    GInt32      m_nComprOrgY;

    // In order to compute block center, we need to keep track of MBR
    GInt32      m_nMinX;
    GInt32      m_nMinY;
    GInt32      m_nMaxX;
    GInt32      m_nMaxY;

    TABBinBlockManager *m_poBlockManagerRef;

    int         m_nTotalDataSize;       // Num bytes in whole chain of blocks
    int         m_nFeatureDataSize;     // Num bytes for current feature coords
    
    GInt32      m_nFeatureXMin;         // Used to keep track of current 
    GInt32      m_nFeatureYMin;         // feature MBR.
    GInt32      m_nFeatureXMax;
    GInt32      m_nFeatureYMax;

  public:
    TABMAPCoordBlock(TABAccess eAccessMode = TABRead);
    ~TABMAPCoordBlock();

    virtual int InitBlockFromData(GByte *pabyBuf,
                                  int nBlockSize, int nSizeUsed,
                                  GBool bMakeCopy = TRUE,
                                  VSILFILE *fpSrc = NULL, int nOffset = 0);
    virtual int InitNewBlock(VSILFILE *fpSrc, int nBlockSize, int nFileOffset=0);
    virtual int CommitToFile();

    virtual int GetBlockClass() { return TABMAP_COORD_BLOCK; };

    void        SetMAPBlockManagerRef(TABBinBlockManager *poBlockManager);
    virtual int ReadBytes(int numBytes, GByte *pabyDstBuf);
    virtual int WriteBytes(int nBytesToWrite, GByte *pBuf);
    void        SetComprCoordOrigin(GInt32 nX, GInt32 nY);
    int         ReadIntCoord(GBool bCompressed, GInt32 &nX, GInt32 &nY);
    int         ReadIntCoords(GBool bCompressed, int numCoords, GInt32 *panXY);
    int         ReadCoordSecHdrs(GBool bCompressed, int nVersion,
                                 int numSections, TABMAPCoordSecHdr *pasHdrs,
                                 GInt32    &numVerticesTotal);
    int         WriteCoordSecHdrs(int nVersion, int numSections,
                                  TABMAPCoordSecHdr *pasHdrs,
                                  GBool bCompressed);

    void        SetNextCoordBlock(GInt32 nNextCoordBlockAddress);
    GInt32      GetNextCoordBlock()   { return m_nNextCoordBlock; };

    int         WriteIntCoord(GInt32 nX, GInt32 nY, GBool bCompressed);

    int         GetNumBlocksInChain() { return m_numBlocksInChain; };

    void        ResetTotalDataSize() {m_nTotalDataSize = 0;};
    int         GetTotalDataSize() {return m_nTotalDataSize;};

    void        SeekEnd();
    void        StartNewFeature();
    int         GetFeatureDataSize() {return m_nFeatureDataSize;};
//__TODO__ Can we flush GetFeatureMBR() and all MBR tracking in this class???
    void        GetFeatureMBR(GInt32 &nXMin, GInt32 &nYMin, 
                              GInt32 &nXMax, GInt32 &nYMax);

#ifdef DEBUG
    virtual void Dump(FILE *fpOut = NULL);
#endif

};

/*---------------------------------------------------------------------
 *                      class TABMAPToolBlock
 *
 * Class to handle Read/Write operation on .MAP Drawing Tool Blocks (Type 05)
 *
 * In addition to handling the I/O, this class also maintains the list 
 * of Tool definitions in memory.
 *--------------------------------------------------------------------*/

class TABMAPToolBlock: public TABRawBinBlock
{
  protected:
    int         m_numDataBytes; /* Excluding first 8 bytes header */
    GInt32      m_nNextToolBlock;
    int         m_numBlocksInChain;

    TABBinBlockManager *m_poBlockManagerRef;

  public:
    TABMAPToolBlock(TABAccess eAccessMode = TABRead);
    ~TABMAPToolBlock();

    virtual int InitBlockFromData(GByte *pabyBuf,
                                  int nBlockSize, int nSizeUsed,
                                  GBool bMakeCopy = TRUE,
                                  VSILFILE *fpSrc = NULL, int nOffset = 0);
    virtual int InitNewBlock(VSILFILE *fpSrc, int nBlockSize, int nFileOffset=0);
    virtual int CommitToFile();

    virtual int GetBlockClass() { return TABMAP_TOOL_BLOCK; };

    void        SetMAPBlockManagerRef(TABBinBlockManager *poBlockManager);
    virtual int ReadBytes(int numBytes, GByte *pabyDstBuf);
    virtual int WriteBytes(int nBytesToWrite, GByte *pBuf);

    void        SetNextToolBlock(GInt32 nNextCoordBlockAddress);

    GBool       EndOfChain();
    int         GetNumBlocksInChain() { return m_numBlocksInChain; };

    int         CheckAvailableSpace(int nToolType);

#ifdef DEBUG
    virtual void Dump(FILE *fpOut = NULL);
#endif

};


/*=====================================================================
       Classes to deal with .MAP files at the MapInfo object level
 =====================================================================*/

/*---------------------------------------------------------------------
 *                      class TABIDFile
 *
 * Class to handle Read/Write operation on .ID files... the .ID file
 * contains an index to the objects in the .MAP file by object id.
 *--------------------------------------------------------------------*/

class TABIDFile
{
  private:
    char        *m_pszFname;
    VSILFILE    *m_fp;
    TABAccess   m_eAccessMode;

    TABRawBinBlock *m_poIDBlock;
    int         m_nBlockSize;
    GInt32      m_nMaxId;

   public:
    TABIDFile();
    ~TABIDFile();

    int         Open(const char *pszFname, const char* pszAccess);
    int         Open(const char *pszFname, TABAccess eAccess);
    int         Close();

    GInt32      GetObjPtr(GInt32 nObjId);
    int         SetObjPtr(GInt32 nObjId, GInt32 nObjPtr);
    GInt32      GetMaxObjId();

#ifdef DEBUG
    void Dump(FILE *fpOut = NULL);
#endif

};

/*---------------------------------------------------------------------
 *                      class TABMAPFile
 *
 * Class to handle Read/Write operation on .MAP files... this class hides
 * all the dealings with blocks, indexes, etc.
 * Use this class to deal with MapInfo objects directly.
 *--------------------------------------------------------------------*/

class TABMAPFile
{
  private:
    int         m_nMinTABVersion;
    char        *m_pszFname;
    VSILFILE    *m_fp;
    TABAccess   m_eAccessMode;

    TABBinBlockManager m_oBlockManager;

    TABMAPHeaderBlock   *m_poHeader;

    // Members used to access objects using the spatial index
    TABMAPIndexBlock  *m_poSpIndex;

    // Defaults to FALSE, i.e. optimized spatial index
    GBool       m_bQuickSpatialIndexMode;

    // Member used to access objects using the object ids (.ID file)
    TABIDFile   *m_poIdIndex;

    // Current object data block.
    TABMAPObjectBlock *m_poCurObjBlock;
    int         m_nCurObjPtr;
    TABGeomType m_nCurObjType;
    int         m_nCurObjId;
    TABMAPCoordBlock *m_poCurCoordBlock;

    // Drawing Tool Def. table (takes care of all drawing tools in memory)
    TABToolDefTable *m_poToolDefTable;

    // Coordinates filter... default is MBR of the whole file
    TABVertex   m_sMinFilter;
    TABVertex   m_sMaxFilter;
    GInt32      m_XMinFilter;
    GInt32      m_YMinFilter;
    GInt32      m_XMaxFilter;
    GInt32      m_YMaxFilter;
    
    int         m_bUpdated;
    int         m_bLastOpWasRead;
    int         m_bLastOpWasWrite;

    int         CommitObjAndCoordBlocks(GBool bDeleteObjects =FALSE);
    int         LoadObjAndCoordBlocks(GInt32 nBlockPtr);
    TABMAPObjectBlock *SplitObjBlock(TABMAPObjHdr *poObjHdrToAdd,
                                     int nSizeOfObjToAdd);
    int         MoveObjToBlock(TABMAPObjHdr       *poObjHdr,
                               TABMAPCoordBlock   *poSrcCoordBlock,
                               TABMAPObjectBlock  *poDstObjBlock,
                               TABMAPCoordBlock   **ppoDstCoordBlock);
    int         PrepareCoordBlock(int nObjType,
                                  TABMAPObjectBlock *poObjBlock,
                                  TABMAPCoordBlock  **ppoCoordBlock);

    int         InitDrawingTools();
    int         CommitDrawingTools();

    int         CommitSpatialIndex();

    // Stuff related to traversing spatial index.
    TABMAPIndexBlock *m_poSpIndexLeaf;

    int         LoadNextMatchingObjectBlock(int bFirstObject);
    TABRawBinBlock *PushBlock( int nFileOffset );
    
    int         ReOpenReadWrite();
    
  public:
    TABMAPFile();
    ~TABMAPFile();

    int         Open(const char *pszFname, const char* pszAccess,
                     GBool bNoErrorMsg = FALSE );
    int         Open(const char *pszFname, TABAccess eAccess,
                     GBool bNoErrorMsg = FALSE );
    int         Close();

    int         SetQuickSpatialIndexMode(GBool bQuickSpatialIndexMode = TRUE);

    int         Int2Coordsys(GInt32 nX, GInt32 nY, double &dX, double &dY);
    int         Coordsys2Int(double dX, double dY, GInt32 &nX, GInt32 &nY, 
                             GBool bIgnoreOveflow=FALSE);
    int         Int2CoordsysDist(GInt32 nX, GInt32 nY, double &dX, double &dY);
    int         Coordsys2IntDist(double dX, double dY, GInt32 &nX, GInt32 &nY);
    void        SetCoordFilter(TABVertex sMin, TABVertex sMax);
    void        GetCoordFilter(TABVertex &sMin, TABVertex &sMax);
    void        ResetCoordFilter();
    int         SetCoordsysBounds(double dXMin, double dYMin, 
                                  double dXMax, double dYMax);

    GInt32      GetMaxObjId();
    int         MoveToObjId(int nObjId);
    void        UpdateMapHeaderInfo(TABGeomType nObjType);
    int         PrepareNewObj(TABMAPObjHdr *poObjHdr);
    int         PrepareNewObjViaSpatialIndex(TABMAPObjHdr *poObjHdr);
    int         PrepareNewObjViaObjBlock(TABMAPObjHdr *poObjHdr);
    int         CommitNewObj(TABMAPObjHdr *poObjHdr);

    void        ResetReading();
    int         GetNextFeatureId( int nPrevId );

    int         MarkAsDeleted();

    TABGeomType GetCurObjType();
    int         GetCurObjId();
    TABMAPObjectBlock *GetCurObjBlock();
    TABMAPCoordBlock  *GetCurCoordBlock();
    TABMAPCoordBlock  *GetCoordBlock(int nFileOffset);
    TABMAPHeaderBlock *GetHeaderBlock();
    TABIDFile         *GetIDFileRef();
    TABRawBinBlock    *GetIndexObjectBlock(int nFileOffset);

    int         ReadPenDef(int nPenIndex, TABPenDef *psDef);
    int         ReadBrushDef(int nBrushIndex, TABBrushDef *psDef);
    int         ReadFontDef(int nFontIndex, TABFontDef *psDef);
    int         ReadSymbolDef(int nSymbolIndex, TABSymbolDef *psDef);
    int         WritePenDef(TABPenDef *psDef);
    int         WriteBrushDef(TABBrushDef *psDef);
    int         WriteFontDef(TABFontDef *psDef);
    int         WriteSymbolDef(TABSymbolDef *psDef);

    int         GetMinTABFileVersion();

#ifdef DEBUG
    void Dump(FILE *fpOut = NULL);
    void DumpSpatialIndexToMIF(TABMAPIndexBlock *poNode, 
                               FILE *fpMIF, FILE *fpMID, 
                               int nIndexInNode=-1, 
                               int nParentId=-1, 
                               int nCurDepth=0,
                               int nMaxDepth=-1);
#endif

};



/*---------------------------------------------------------------------
 *                      class TABINDNode
 *
 * An index node in a .IND file.
 *
 * This class takes care of reading child nodes as necessary when looking
 * for a given key value in the index tree.
 *--------------------------------------------------------------------*/

class TABINDNode
{
  private:
    VSILFILE    *m_fp;
    TABAccess   m_eAccessMode;
    TABINDNode *m_poCurChildNode;
    TABINDNode *m_poParentNodeRef;

    TABBinBlockManager *m_poBlockManagerRef;

    int         m_nSubTreeDepth;
    int         m_nKeyLength;
    TABFieldType m_eFieldType;
    GBool       m_bUnique;

    GInt32      m_nCurDataBlockPtr;
    int         m_nCurIndexEntry;
    TABRawBinBlock *m_poDataBlock;
    int         m_numEntriesInNode;
    GInt32      m_nPrevNodePtr;
    GInt32      m_nNextNodePtr;

    int         GotoNodePtr(GInt32 nNewNodePtr);
    GInt32      ReadIndexEntry(int nEntryNo, GByte *pKeyValue);
    int         IndexKeyCmp(GByte *pKeyValue, int nEntryNo);

    int         InsertEntry(GByte *pKeyValue, GInt32 nRecordNo,
                            GBool bInsertAfterCurChild=FALSE,
                            GBool bMakeNewEntryCurChild=FALSE);
    int         SetNodeBufferDirectly(int numEntries, GByte *pBuf,
                                      int nCurIndexEntry=0, 
                                      TABINDNode *poCurChild=NULL);

   public:
    TABINDNode(TABAccess eAccessMode = TABRead);
    ~TABINDNode();

    int         InitNode(VSILFILE *fp, int nBlockPtr, 
                         int nKeyLength, int nSubTreeDepth, GBool bUnique,
                         TABBinBlockManager *poBlockMgr=NULL,
                         TABINDNode *poParentNode=NULL,
                         int nPrevNodePtr=0, int nNextNodePtr=0);

    int         SetFieldType(TABFieldType eType);
    TABFieldType GetFieldType()         {return m_eFieldType;};

    void        SetUnique(GBool bUnique){m_bUnique = bUnique;};
    GBool       IsUnique()              {return m_bUnique;};

    int         GetKeyLength()          {return m_nKeyLength;};
    int         GetSubTreeDepth()       {return m_nSubTreeDepth;};
    GInt32      GetNodeBlockPtr()       {return m_nCurDataBlockPtr;};
    int         GetNumEntries()         {return m_numEntriesInNode;};
    int         GetMaxNumEntries()      {return (512-12)/(m_nKeyLength+4);};

    GInt32      FindFirst(GByte *pKeyValue);
    GInt32      FindNext(GByte *pKeyValue);

    int         CommitToFile();

    int         AddEntry(GByte *pKeyValue, GInt32 nRecordNo,
                         GBool bAddInThisNodeOnly=FALSE,
                         GBool bInsertAfterCurChild=FALSE,
                         GBool bMakeNewEntryCurChild=FALSE);
    int         SplitNode();
    int         SplitRootNode();
    GByte*      GetNodeKey();
    int         UpdateCurChildEntry(GByte *pKeyValue, GInt32 nRecordNo);
    int         UpdateSplitChild(GByte *pKeyValue1, GInt32 nRecordNo1,
                                 GByte *pKeyValue2, GInt32 nRecordNo2,
                                 int nNewCurChildNo /* 1 or 2 */);

    int         SetNodeBlockPtr(GInt32 nThisNodePtr);
    int         SetPrevNodePtr(GInt32 nPrevNodePtr);
    int         SetNextNodePtr(GInt32 nNextNodePtr);

#ifdef DEBUG
    void Dump(FILE *fpOut = NULL);
#endif

};


/*---------------------------------------------------------------------
 *                      class TABINDFile
 *
 * Class to handle table field index (.IND) files... we use this
 * class as the main entry point to open and search the table field indexes.
 * Note that .IND files are supported for read access only.
 *--------------------------------------------------------------------*/

class TABINDFile
{
  private:
    char        *m_pszFname;
    VSILFILE    *m_fp;
    TABAccess   m_eAccessMode;

    TABBinBlockManager m_oBlockManager;

    int         m_numIndexes;
    TABINDNode  **m_papoIndexRootNodes;
    GByte       **m_papbyKeyBuffers;

    int         ValidateIndexNo(int nIndexNumber);
    int         ReadHeader();
    int         WriteHeader();

   public:
    TABINDFile();
    ~TABINDFile();

    int         Open(const char *pszFname, const char *pszAccess, 
                     GBool bTestOpenNoError=FALSE);
    int         Close();

    int         GetNumIndexes() {return m_numIndexes;};
    int         SetIndexFieldType(int nIndexNumber, TABFieldType eType);
    int         SetIndexUnique(int nIndexNumber, GBool bUnique=TRUE);
    GByte      *BuildKey(int nIndexNumber, GInt32 nValue);
    GByte      *BuildKey(int nIndexNumber, const char *pszStr);
    GByte      *BuildKey(int nIndexNumber, double dValue);
    GInt32      FindFirst(int nIndexNumber, GByte *pKeyValue);
    GInt32      FindNext(int nIndexNumber, GByte *pKeyValue);

    int         CreateIndex(TABFieldType eType, int nFieldSize);
    int         AddEntry(int nIndexNumber, GByte *pKeyValue, GInt32 nRecordNo);

#ifdef DEBUG
    void Dump(FILE *fpOut = NULL);
#endif

};


/*---------------------------------------------------------------------
 *                      class TABDATFile
 *
 * Class to handle Read/Write operation on .DAT files... the .DAT file
 * contains the table of attribute field values.
 *--------------------------------------------------------------------*/

class TABDATFile
{
  private:
    char        *m_pszFname;
    VSILFILE    *m_fp;
    TABAccess   m_eAccessMode;
    TABTableType m_eTableType;

    TABRawBinBlock *m_poHeaderBlock;
    int         m_numFields;
    TABDATFieldDef *m_pasFieldDef;

    TABRawBinBlock *m_poRecordBlock;
    int         m_nBlockSize;
    int         m_nRecordSize;
    int         m_nCurRecordId;
    GBool       m_bCurRecordDeletedFlag;

    GInt32      m_numRecords;
    GInt32      m_nFirstRecordPtr;
    GBool       m_bWriteHeaderInitialized;
    GBool       m_bWriteEOF;
    
    int         m_bUpdated;

    int         InitWriteHeader();
    int         WriteHeader();

	// We know that character strings are limited to 254 chars in MapInfo
	// Using a buffer pr. class instance to avoid threading issues with the library
	char		m_szBuffer[256];

   public:
    TABDATFile();
    ~TABDATFile();

    int         Open(const char *pszFname, const char* pszAccess,
                     TABTableType eTableType =TABTableNative);
    int         Open(const char *pszFname, TABAccess eAccess,
                     TABTableType eTableType =TABTableNative);
    int         Close();

    int         GetNumFields();
    TABFieldType GetFieldType(int nFieldId);
    int         GetFieldWidth(int nFieldId);
    int         GetFieldPrecision(int nFieldId);
    int         ValidateFieldInfoFromTAB(int iField, const char *pszName,
                                         TABFieldType eType,
                                         int nWidth, int nPrecision);

    int         AddField(const char *pszName, TABFieldType eType,
                         int nWidth, int nPrecision=0);

    int         DeleteField( int iField );
    int         ReorderFields( int* panMap );
    int         AlterFieldDefn( int iField, OGRFieldDefn* poNewFieldDefn, int nFlags );

    GInt32      GetNumRecords();
    TABRawBinBlock *GetRecordBlock(int nRecordId);
    GBool       IsCurrentRecordDeleted() { return m_bCurRecordDeletedFlag;};
    int         CommitRecordToFile();
    
    int         MarkAsDeleted();
    int         MarkRecordAsExisting();

    const char  *ReadCharField(int nWidth);
    GInt32      ReadIntegerField(int nWidth);
    GInt16      ReadSmallIntField(int nWidth);
    double      ReadFloatField(int nWidth);
    double      ReadDecimalField(int nWidth);
    const char  *ReadLogicalField(int nWidth);
    const char  *ReadDateField(int nWidth);
    int         ReadDateField(int nWidth, int *nYear, int *nMonth, int *nDay);
    const char  *ReadTimeField(int nWidth);
    int         ReadTimeField(int nWidth, int *nHour, int *nMinute, 
                              int *nSecond, int *nMS);
    const char  *ReadDateTimeField(int nWidth);
    int         ReadDateTimeField(int nWidth, int *nYear, int *nMonth, int *nDay,
                                 int *nHour, int *nMinute, int *nSecond, int *nMS);

    int         WriteCharField(const char *pszValue, int nWidth,
                               TABINDFile *poINDFile, int nIndexNo);
    int         WriteIntegerField(GInt32 nValue,
                                  TABINDFile *poINDFile, int nIndexNo);
    int         WriteSmallIntField(GInt16 nValue,
                                   TABINDFile *poINDFile, int nIndexNo);
    int         WriteFloatField(double dValue,
                                TABINDFile *poINDFile, int nIndexNo);
    int         WriteDecimalField(double dValue, int nWidth, int nPrecision,
                                  TABINDFile *poINDFile, int nIndexNo);
    int         WriteLogicalField(const char *pszValue,
                                  TABINDFile *poINDFile, int nIndexNo);
    int         WriteDateField(const char *pszValue,
                               TABINDFile *poINDFile, int nIndexNo);
    int         WriteDateField(int nYear, int nMonth, int nDay,
                               TABINDFile *poINDFile, int nIndexNo);
    int         WriteTimeField(const char *pszValue,
                               TABINDFile *poINDFile, int nIndexNo);
    int         WriteTimeField(int nHour, int nMinute, int nSecond, int nMS,
                               TABINDFile *poINDFile, int nIndexNo);
    int         WriteDateTimeField(const char *pszValue,
                               TABINDFile *poINDFile, int nIndexNo);
    int         WriteDateTimeField(int nYear, int nMonth, int nDay, 
                                   int nHour, int nMinute, int nSecond, int nMS,
                                   TABINDFile *poINDFile, int nIndexNo);

#ifdef DEBUG
    void Dump(FILE *fpOut = NULL);
#endif

};


/*---------------------------------------------------------------------
 *                      class TABRelation
 *
 * Class that maintains a relation between 2 tables through a field
 * in each table (the SQL "where table1.field1=table2.field2" found in 
 * TABView datasets).
 *
 * An instance of this class is used to read data records from the
 * combined tables as if they were a single one.
 *--------------------------------------------------------------------*/

class TABRelation
{
  private:
    /* Information about the main table.
     */
    TABFile     *m_poMainTable;
    char        *m_pszMainFieldName;
    int         m_nMainFieldNo;

    /* Information about the related table.  
     * NOTE: The related field MUST be indexed.
     */
    TABFile     *m_poRelTable;
    char        *m_pszRelFieldName;
    int         m_nRelFieldNo;

    TABINDFile  *m_poRelINDFileRef;
    int         m_nRelFieldIndexNo;

    int         m_nUniqueRecordNo;

    /* Main and Rel table field map:
     * For each field in the source tables, -1 means that the field is not
     * selected, and a value >=0 is the index of this field in the combined
     * FeatureDefn
     */
    int         *m_panMainTableFieldMap;
    int         *m_panRelTableFieldMap;

    OGRFeatureDefn *m_poDefn;

    void        ResetAllMembers();
    GByte       *BuildFieldKey(TABFeature *poFeature, int nFieldNo,
                                  TABFieldType eType, int nIndexNo);

   public:
    TABRelation();
    ~TABRelation();

    int         Init(const char *pszViewName,
                     TABFile *poMainTable, TABFile *poRelTable,
                     const char *pszMainFieldName,
                     const char *pszRelFieldName,
                     char **papszSelectedFields);
    int         CreateRelFields();

    OGRFeatureDefn *GetFeatureDefn()  {return m_poDefn;};
    TABFieldType    GetNativeFieldType(int nFieldId);
    TABFeature     *GetFeature(int nFeatureId);

    int         WriteFeature(TABFeature *poFeature, int nFeatureId=-1);

    int         SetFeatureDefn(OGRFeatureDefn *poFeatureDefn,
                           TABFieldType *paeMapInfoNativeFieldTypes=NULL);
    int         AddFieldNative(const char *pszName, TABFieldType eMapInfoType,
                               int nWidth=0, int nPrecision=0,
                               GBool bIndexed=FALSE, GBool bUnique=FALSE, int bApproxOK=TRUE);

    int         SetFieldIndexed(int nFieldId);
    GBool       IsFieldIndexed(int nFieldId);
    GBool       IsFieldUnique(int nFieldId);

    const char *GetMainFieldName()      {return m_pszMainFieldName;};
    const char *GetRelFieldName()       {return m_pszRelFieldName;};
};


/*---------------------------------------------------------------------
 *                      class MIDDATAFile
 *
 * Class to handle a file pointer with a copy of the latest readed line
 *
 *--------------------------------------------------------------------*/

class MIDDATAFile
{
   public:
      MIDDATAFile();
     ~MIDDATAFile();

     int         Open(const char *pszFname, const char *pszAccess);
     int         Close();

     const char *GetLine();
     const char *GetLastLine();
     int Rewind();
     void SaveLine(const char *pszLine);
     const char *GetSavedLine();
     void WriteLine(const char*, ...);
     GBool IsValidFeature(const char *pszString);

//  Translation information
     void SetTranslation(double, double, double, double);
     double GetXTrans(double);
     double GetYTrans(double);
     double GetXMultiplier(){return m_dfXMultiplier;}
     const char *GetDelimiter(){return m_pszDelimiter;}
     void SetDelimiter(const char *pszDelimiter){m_pszDelimiter=pszDelimiter;}

     void SetEof(GBool bEof);
     GBool GetEof();

     private:
       VSILFILE *m_fp;
       const char *m_pszDelimiter;

       // Set limit for the length of a line
#define MIDMAXCHAR 10000
       char m_szLastRead[MIDMAXCHAR];
       char m_szSavedLine[MIDMAXCHAR];

       char        *m_pszFname;
       TABAccess   m_eAccessMode;
       double      m_dfXMultiplier;
       double      m_dfYMultiplier;
       double      m_dfXDisplacement;
       double      m_dfYDisplacement;
       GBool       m_bEof;
};



/*=====================================================================
                        Function prototypes
 =====================================================================*/

TABRawBinBlock *TABCreateMAPBlockFromFile(VSILFILE *fpSrc, int nOffset, 
                                          int nSize = 512, 
                                          GBool bHardBlockSize = TRUE,
                                          TABAccess eAccessMode = TABRead);


#endif /* _MITAB_PRIV_H_INCLUDED_ */

