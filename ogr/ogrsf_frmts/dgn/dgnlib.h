/******************************************************************************
 * $Id$
 *
 * Project:  Microstation DGN Access Library
 * Purpose:  Definitions of public structures and API of DGN Library.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2000, Avenza Systems Inc, http://www.avenza.com/
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
 ******************************************************************************
 *
 * $Log$
 * Revision 1.21  2002/05/31 03:40:22  warmerda
 * added improved support for parsing attribute linkages
 *
 * Revision 1.20  2002/04/22 20:44:40  warmerda
 * added (partial) cell library support
 *
 * Revision 1.19  2002/03/14 21:38:52  warmerda
 * added DGNWriteElement, DGNResizeElement, make offset/size manditory
 *
 * Revision 1.18  2002/03/12 17:07:26  warmerda
 * added tagset and tag value element support
 *
 * Revision 1.17  2002/02/06 20:33:02  warmerda
 * preliminary addition of tagset support
 *
 * Revision 1.16  2002/01/21 20:50:12  warmerda
 * added SetSpatialFilter function
 *
 * Revision 1.15  2002/01/15 06:38:18  warmerda
 * added DGNGetShapeFillInfo
 *
 * Revision 1.14  2001/12/19 15:29:56  warmerda
 * added preliminary cell header support
 *
 * Revision 1.13  2001/08/21 03:01:39  warmerda
 * added raw_data support
 *
 * Revision 1.12  2001/06/25 15:07:51  warmerda
 * Added support for DGNElemComplexHeader
 * Don't include elements with the complex bit (such as shared cell definition
 * elements) in extents computation for fear they are in a different coord sys.
 *
 * Revision 1.11  2001/03/18 16:54:39  warmerda
 * added use of DGNTestOpen, remove extention test
 *
 * Revision 1.10  2001/03/07 19:29:46  warmerda
 * added support for stroking curves
 *
 * Revision 1.9  2001/03/07 13:56:44  warmerda
 * updated copyright to be held by Avenza Systems
 *
 * Revision 1.8  2001/03/07 13:48:59  warmerda
 * added DGNEIF_DELETED
 *
 * Revision 1.7  2001/02/02 22:20:15  warmerda
 * document DGNElemText, length/height_mult now double
 *
 * Revision 1.6  2001/01/16 21:17:28  warmerda
 * added justification enum
 *
 * Revision 1.5  2001/01/16 18:12:02  warmerda
 * added arc support, color lookup
 *
 * Revision 1.4  2001/01/10 16:11:33  warmerda
 * added lots of documentation, and extents api
 *
 * Revision 1.3  2000/12/28 21:28:43  warmerda
 * added element index support
 *
 * Revision 1.2  2000/12/14 17:10:57  warmerda
 * implemented TCB, Ellipse, TEXT
 *
 * Revision 1.1  2000/11/28 19:03:47  warmerda
 * New
 *
 */

#ifndef _DGNLIB_H_INCLUDED
#define _DGNLIB_H_INCLUDED

#include "cpl_conv.h"

CPL_C_START

/**
 * \file dgnlib.h
 *
 * Definitions of public structures and API of DGN Library.
 */

/**
 * DGN Point structure.
 *
 * Note that the DGNReadElement() function transforms points into "master"
 * coordinate system space when they are in the file in UOR (units of
 * resolution) coordinates. 
 */

typedef struct {
    double x;	/*!< X (normally eastwards) coordinate. */
    double y;   /*!< y (normally northwards) coordinate. */
    double z;   /*!< z, up coordinate.  Zero for 2D objects. */
} DGNPoint;

/**
 * Element summary information.
 *
 * Minimal information kept about each element if an element summary
 * index is built for a file by DGNGetElementIndex().
 */
typedef struct {
    unsigned char	level;   /*!< Element Level: 0-63 */
    unsigned char       type;    /*!< Element type (DGNT_*) */
    unsigned char       stype;   /*!< Structure type (DGNST_*) */
    unsigned char       flags;   /*!< Other flags */
    long                offset;  /*!< Offset within file (private) */
} DGNElementInfo;  

/**
 * Core element structure. 
 *
 * Core information kept about each element that can be read from a DGN
 * file.  This structure is the first component of each specific element 
 * structure (like DGNElemMultiPoint).  Normally the DGNElemCore.stype
 * field would be used to decide what specific structure type to case the
 * DGNElemCore pointer to. 
 *
 */

typedef struct {
    int         offset;
    int         size;

    int         element_id;     /*!< Element number (zero based) */
    int         stype;          /*!< Structure type: (DGNST_*) */
    int		level;		/*!< Element Level: 0-63 */
    int		type;		/*!< Element type (DGNT_) */
    int		complex;	/*!< Is element complex? */
    int		deleted;	/*!< Is element deleted? */

    int		graphic_group;  /*!< Graphic group number */
    int		properties;     /*!< Properties: ORing of DGNP_ flags */
    int         color;          /*!< Color index (0-255) */
    int         weight;         /*!< Line Weight (0-31) */
    int         style;          /*!< Line Style: One of DGNS_* values */

    int		attr_bytes;	/*!< Bytes of attribute data, usually zero. */
    unsigned char *attr_data;   /*!< Raw attribute data */

    int         raw_bytes;      /*!< Bytes of raw data, usually zero. */
    unsigned char *raw_data;    /*!< All raw element data including header. */
} DGNElemCore;

/** 
 * Multipoint element 
 *
 * The core.stype code is DGNST_MULTIPOINT.
 *
 * Used for: DGNT_LINE(3), DGNT_LINE_STRING(4), DGNT_SHAPE(6), DGNT_CURVE(11),
 * DGNT_BSPLINE(21)
 */

typedef struct {
  DGNElemCore 	core;

  int		num_vertices;  /*!< Number of vertices in "vertices" */
  DGNPoint      vertices[2];   /*!< Array of two or more vertices */

} DGNElemMultiPoint;    

/** 
 * Ellipse element 
 *
 * The core.stype code is DGNST_ARC.
 *
 * Used for: DGNT_ELLIPSE(15), DGNT_ARC(16)
 */

typedef struct {
  DGNElemCore 	core;

  DGNPoint	origin;		/*!< Origin of ellipse */

  double	primary_axis;	/*!< Primary axis length */
  double        secondary_axis; /*!< Secondary axis length */

  double	rotation;       /*!< Counterclockwise rotation in degrees */
  long          quat[4];

  double	startang;       /*!< Start angle (degrees counterclockwise of primary axis) */
  double	sweepang;       /*!< Sweep angle (degrees) */

} DGNElemArc;

/** 
 * Text element 
 *
 * The core.stype code is DGNST_TEXT.
 *
 * NOTE: Currently we are not capturing the "editable fields" information.
 *
 * Used for: DGNT_TEXT(17).
 */

typedef struct {
    DGNElemCore core;
    
    int		font_id;       /*!< Microstation font id, no list available*/
    int		justification; /*!< Justification, see DGNJ_* */
    double      length_mult;   /*!< Char width in master (if square) */
    double      height_mult;   /*!< Char height in master units */
    double	rotation;      /*!< Counterclockwise rotation in degrees */
    DGNPoint	origin;        /*!< Bottom left corner of text. */
    char	string[1];     /*!< Actual text (length varies, \0 terminated*/
} DGNElemText;

/** 
 * Complex header element 
 *
 * The core.stype code is DGNST_COMPLEXHEADER.
 *
 * Used for: DGNT_COMPLEX_CHAIN_HEADER(12), DGNT_COMPLEX_SHAPE_HEADER(14).
 */

typedef struct {
    DGNElemCore core;
    
    int		totlength;     /*!< Total length of surface */
    int		numelems;      /*!< # of elements in surface */
} DGNElemComplexHeader;

/** 
 * Color table.
 *
 * The core.stype code is DGNST_COLORTABLE.
 *
 * Returned for DGNT_GROUP_DATA(5) elements, with a level number of 
 * DGN_GDL_COLOR_TABLE(1).
 */

typedef struct {
  DGNElemCore 	core;

  int           screen_flag;
  GByte         color_info[256][3]; /*!< Color table, 256 colors by red (0), green(1) and blue(2) component. */
} DGNElemColorTable;

/** 
 * Terminal Control Block (header).
 *
 * The core.stype code is DGNST_TCB.
 *
 * Returned for DGNT_TCB(9).
 *
 * The first TCB in the file is used to determine the dimension (2D vs. 3D),
 * and transformation from UOR (units of resolution) to subunits, and subunits
 * to master units.  This is handled transparently within DGNReadElement(), so
 * it is not normally necessary to handle this element type at the application
 * level, though it can be useful to get the sub_units, and master_units names.
 */

typedef struct {
    DGNElemCore core;

    int		dimension;         /*!< Dimension (2 or 3) */

    double	origin_x;       /*!< X origin of UOR space in master units(?)*/
    double      origin_y;       /*!< Y origin of UOR space in master units(?)*/
    double      origin_z;       /*!< Z origin of UOR space in master units(?)*/
    
    long	uor_per_subunit;   /*!< UOR per subunit. */
    char	sub_units[3];      /*!< User name for subunits (2 chars)*/
    long        subunits_per_master; /*!< Subunits per master unit. */
    char        master_units[3];   /*!< User name for master units (2 chars)*/

} DGNElemTCB;

/** 
 * Cell Header.
 *
 * The core.stype code is DGNST_CELL_HEADER.
 *
 * Returned for DGNT_CELL_HEADER(2).
 */

typedef struct {
    DGNElemCore core;

    int		totlength;         /*!< Total length of cell */
    char        name[7];           /*!< Cell name */
 unsigned short cclass;            /*!< Class bitmap */
 unsigned short levels[4];         /*!< Levels used in cell */
    
    DGNPoint    rnglow;            /*!< X/Y/Z minimums for cell */
    DGNPoint    rnghigh;           /*!< X/Y/Z minimums for cell */
    
    double      trans[9];          /*!< 2D/3D Transformation Matrix */
    DGNPoint    origin;            /*!< Cell Origin */

    double      xscale;
    double      yscale;
    double      rotation;

} DGNElemCellHeader;

/** 
 * Cell Library.
 *
 * The core.stype code is DGNST_CELL_LIBRARY.
 *
 * Returned for DGNT_CELL_LIBRARY(1).
 */

typedef struct {
    DGNElemCore core;

    short       celltype;          /*!< Cell type. */
    short       attindx;           /*!< Attribute linkage. */
    char        name[7];           /*!< Cell name */

    int         numwords;          /*!< Number of words in cell definition */

    short       dispsymb;          /*!< Display symbol */
 unsigned short cclass;            /*!< Class bitmap */
 unsigned short levels[4];         /*!< Levels used in cell */

    char        description[28];   /*!< Description */
    
} DGNElemCellLibrary;

/** 
 * Tag Value.
 *
 * The core.stype code is DGNST_TAG_VALUE.
 *
 * Returned for DGNT_TAG_VALUE(37).
 */

typedef union { char *string; GInt32 integer; double real; } tagValueUnion;

typedef struct {
    DGNElemCore core;

    int         tagType;           /*!< Tag type indicator, 1=string */
    int         tagSet;            /*!< Which tag set does this relate to? */
    int         tagIndex;          /*!< Tag index within tag set. */
    int         tagLength;         /*!< Length of tag information (text) */
    tagValueUnion tagValue;        /*!< Textual value of tag */

} DGNElemTagValue;

/** 
 * Tag Set.
 *
 * The core.stype code is DGNST_TAG_SET.
 *
 * Returned for DGNT_APPLICATION_ELEM(66), Level: 24.
 */

typedef struct _DGNTagDef {
    char	*name;
    int         id;
    char        *prompt;
    int         type;
    tagValueUnion defaultValue;
} DGNTagDef;

typedef struct {
    DGNElemCore core;

    int        tagCount;
    int        tagSet; 
    int        flags;
    char       *tagSetName;

    DGNTagDef  *tagList;

} DGNElemTagSet;

/* -------------------------------------------------------------------- */
/*      Structure types                                                 */
/* -------------------------------------------------------------------- */

/** DGNElemCore style: Element uses DGNElemCore structure */
#define DGNST_CORE		   1 

/** DGNElemCore style: Element uses DGNElemMultiPoint structure */
#define DGNST_MULTIPOINT	   2 

/** DGNElemCore style: Element uses DGNElemColorTable structure */
#define DGNST_COLORTABLE           3 

/** DGNElemCore style: Element uses DGNElemTCB structure */
#define DGNST_TCB                  4 

/** DGNElemCore style: Element uses DGNElemArc structure */
#define DGNST_ARC                  5 

/** DGNElemCore style: Element uses DGNElemText structure */
#define DGNST_TEXT                 6 

/** DGNElemCore style: Element uses DGNElemComplexHeader structure */
#define DGNST_COMPLEX_HEADER       7

/** DGNElemCore style: Element uses DGNElemCellHeader structure */
#define DGNST_CELL_HEADER          8

/** DGNElemCore style: Element uses DGNElemTagValue structure */
#define DGNST_TAG_VALUE            9

/** DGNElemCore style: Element uses DGNElemTagSet structure */
#define DGNST_TAG_SET             10

/** DGNElemCore style: Element uses DGNElemCellLibrary structure */
#define DGNST_CELL_LIBRARY        11

/* -------------------------------------------------------------------- */
/*      Element types                                                   */
/* -------------------------------------------------------------------- */
#define DGNT_CELL_LIBRARY	   1
#define DGNT_CELL_HEADER	   2
#define DGNT_LINE		   3
#define DGNT_LINE_STRING	   4
#define DGNT_GROUP_DATA            5
#define DGNT_SHAPE		   6
#define DGNT_TEXT_NODE             7
#define DGNT_DIGITIZER_SETUP       8
#define DGNT_TCB                   9
#define DGNT_LEVEL_SYMBOLOGY      10
#define DGNT_CURVE		  11
#define DGNT_COMPLEX_CHAIN_HEADER 12
#define DGNT_COMPLEX_SHAPE_HEADER 14
#define DGNT_ELLIPSE              15
#define DGNT_ARC                  16
#define DGNT_TEXT                 17
#define DGNT_BSPLINE              21
#define DGNT_SHARED_CELL_DEFN     34
#define DGNT_SHARED_CELL_ELEM     35
#define DGNT_TAG_VALUE            37
#define DGNT_APPLICATION_ELEM     66

/* -------------------------------------------------------------------- */
/*      Line Styles                                                     */
/* -------------------------------------------------------------------- */
#define DGNS_SOLID		0
#define DGNS_DOTTED		1
#define DGNS_MEDIUM_DASH	2
#define DGNS_LONG_DASH		3
#define DGNS_DOT_DASH		4
#define DGNS_SHORT_DASH		5
#define DGNS_DASH_DOUBLE_DOT    6
#define DGNS_LONG_DASH_SHORT_DASH 7

/* -------------------------------------------------------------------- */
/*      Class                                                           */
/* -------------------------------------------------------------------- */
#define DGNC_PRIMARY			0
#define DGNC_PATTERN_COMPONENT		1
#define DGNC_CONSTRUCTION_ELEMENT	2
#define DGNC_DIMENSION_ELEMENT	        3
#define DGNC_PRIMARY_RULE_ELEMENT       4
#define DGNC_LINEAR_PATTERNED_ELEMENT   5
#define DGNC_CONSTRUCTION_RULE_ELEMENT  6

/* -------------------------------------------------------------------- */
/*      Group Data level numbers.                                       */
/*                                                                      */
/*      These are symbolic values for the typ 5 (DGNT_GROUP_DATA)       */
/*      level values that have special meanings.                        */
/* -------------------------------------------------------------------- */
#define DGN_GDL_COLOR_TABLE     1
#define DGN_GDL_NAMED_VIEW      3
#define DGN_GDL_REF_FILE        9

/* -------------------------------------------------------------------- */
/*      Word 17 property flags.                                         */
/* -------------------------------------------------------------------- */
#define DGNPF_HOLE	   0x8000
#define DGNPF_SNAPPABLE    0x4000
#define DGNPF_PLANAR       0x2000
#define DGNPF_ORIENTATION  0x1000
#define DGNPF_ATTRIBUTES   0x0800
#define DGNPF_MODIFIED     0x0400
#define DGNPF_NEW          0x0200
#define DGNPF_LOCKED       0x0100
#define DGNPF_CLASS        0x000f

/* -------------------------------------------------------------------- */
/*      DGNElementInfo flag values.                                     */
/* -------------------------------------------------------------------- */
#define DGNEIF_DELETED     0x01
#define DGNEIF_COMPLEX     0x02

/* -------------------------------------------------------------------- */
/*      Justifications                                                  */
/* -------------------------------------------------------------------- */
#define DGNJ_LEFT_TOP		0
#define DGNJ_LEFT_CENTER	1
#define DGNJ_LEFT_BOTTOM	2
#define DGNJ_CENTER_TOP		3
#define DGNJ_CENTER_CENTER	4
#define DGNJ_CENTER_BOTTOM	5
#define DGNJ_RIGHT_TOP		6
#define DGNJ_RIGHT_CENTER	7
#define DGNJ_RIGHT_BOTTOM	8

/* -------------------------------------------------------------------- */
/*      DGN file reading options.                                       */
/* -------------------------------------------------------------------- */
#define DGNO_CAPTURE_RAW_DATA	0x01

/* -------------------------------------------------------------------- */
/*      Known attribute linkage types, including my synthetic ones.     */
/* -------------------------------------------------------------------- */
#define DGNLT_DMRS              0x0000
#define DGNLT_INFORMIX          0x3848
#define DGNLT_ODBC              0x5e62
#define DGNLT_ORACLE            0x6091
#define DGNLT_RIS               0x71FB
#define DGNLT_SYBASE            0x4f58
#define DGNLT_XBASE             0x1971
#define DGNLT_SHAPE_FILL        0x0041

/* -------------------------------------------------------------------- */
/*      API                                                             */
/* -------------------------------------------------------------------- */
/** Opaque handle representing DGN file, used with DGN API. */
typedef void *DGNHandle;

DGNHandle CPL_DLL    DGNOpen( const char *, int );
void CPL_DLL         DGNSetOptions( DGNHandle, int );
int CPL_DLL          DGNTestOpen( GByte *, int );
const DGNElementInfo CPL_DLL *DGNGetElementIndex( DGNHandle, int * );
int CPL_DLL          DGNGetExtents( DGNHandle, double * );
DGNElemCore CPL_DLL *DGNReadElement( DGNHandle );
int  CPL_DLL         DGNWriteElement( DGNHandle, DGNElemCore * );
int  CPL_DLL         DGNResizeElement( DGNHandle, DGNElemCore *, int );
void CPL_DLL         DGNFreeElement( DGNHandle, DGNElemCore * );
void CPL_DLL         DGNRewind( DGNHandle );
int  CPL_DLL         DGNGotoElement( DGNHandle, int );
void CPL_DLL         DGNClose( DGNHandle );
int  CPL_DLL         DGNLookupColor( DGNHandle, int, int *, int *, int * );
int  CPL_DLL         DGNGetShapeFillInfo( DGNHandle, DGNElemCore *, int * );

void CPL_DLL         DGNDumpElement( DGNHandle, DGNElemCore *, FILE * );
const char CPL_DLL  *DGNTypeToName( int );

int CPL_DLL   DGNStrokeArc( DGNHandle, DGNElemArc *, int, DGNPoint * );
int CPL_DLL   DGNStrokeCurve( DGNHandle, DGNElemMultiPoint*, int, DGNPoint * );
void CPL_DLL  DGNSetSpatialFilter( DGNHandle hDGN, 
                                   double dfXMin, double dfYMin, 
                                   double dfXMax, double dfYMax );
int  CPL_DLL  DGNGetAttrLinkSize( DGNHandle, DGNElemCore *, int );
unsigned char CPL_DLL *
	      DGNGetLinkage( DGNHandle hDGN, DGNElemCore *psElement, 
                             int iIndex, int *pnLinkageType,
                             int *pnEntityNum, int *pnMSLink, int *pnLinkSize);
    


CPL_C_END

#endif /* ndef _DGNLIB_H_INCLUDED */
