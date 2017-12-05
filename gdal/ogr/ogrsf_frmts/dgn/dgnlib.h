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
 ****************************************************************************/

#ifndef DGNLIB_H_INCLUDED
#define DGNLIB_H_INCLUDED

#undef STRICT_CPLERRORNUM_TYPE

#include "cpl_conv.h"

CPL_C_START

#define CPLE_DGN_ERROR_BASE
#define CPLE_ElementTooBig              CPLE_DGN_ERROR_BASE+1

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
    double x;   /*!< x (normally eastwards) coordinate. */
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
    unsigned char       level;   /*!< Element Level: 0-63 */
    unsigned char       type;    /*!< Element type (DGNT_*) */
    unsigned char       stype;   /*!< Structure type (DGNST_*) */
    unsigned char       flags;   /*!< Other flags */
    vsi_l_offset        offset;  /*!< Offset within file (private) */
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
    int         level;          /*!< Element Level: 0-63 */
    int         type;           /*!< Element type (DGNT_) */
    int         complex;        /*!< Is element complex? */
    int         deleted;        /*!< Is element deleted? */

    int         graphic_group;  /*!< Graphic group number */
    int         properties;     /*!< Properties: ORing of DGNPF_ flags */
    int         color;          /*!< Color index (0-255) */
    int         weight;         /*!< Line Weight (0-31) */
    int         style;          /*!< Line Style: One of DGNS_* values */

    int         attr_bytes;     /*!< Bytes of attribute data, usually zero. */
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
 * DGNT_BSPLINE_POLE(21)
 */

typedef struct {
  DGNElemCore   core;

  int           num_vertices;  /*!< Number of vertices in "vertices" */
  DGNPoint      vertices[1];   /*!< Array of two or more vertices */
                               // Note: We set [1] to please clang UBSAN
                               // regarding structures that are extended at
                               // malloc time

} DGNElemMultiPoint;

/**
 * Ellipse element
 *
 * The core.stype code is DGNST_ARC.
 *
 * Used for: DGNT_ELLIPSE(15), DGNT_ARC(16)
 */

typedef struct {
  DGNElemCore   core;

  DGNPoint      origin;         /*!< Origin of ellipse */

  double        primary_axis;   /*!< Primary axis length */
  double        secondary_axis; /*!< Secondary axis length */

  double        rotation;       /*!< Counterclockwise rotation in degrees */
  int           quat[4];

  double        startang;       /*!< Start angle (degrees counterclockwise of primary axis) */
  double        sweepang;       /*!< Sweep angle (degrees) */

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
    DGNElemCore core;   /* must be at first position ! */

    int         font_id;       /*!< Microstation font id, no list available*/
    int         justification; /*!< Justification, see DGNJ_* */
    double      length_mult;   /*!< Char width in master (if square) */
    double      height_mult;   /*!< Char height in master units */
    double      rotation;      /*!< Counterclockwise rotation in degrees */
    DGNPoint    origin;        /*!< Bottom left corner of text. */
    char        string[1];     /*!< Actual text (length varies, \0 terminated*/
} DGNElemText;

/**
 * Complex header element
 *
 * The core.stype code is DGNST_COMPLEX_HEADER.
 *
 * Used for: DGNT_COMPLEX_CHAIN_HEADER(12), DGNT_COMPLEX_SHAPE_HEADER(14),
 *   DGNT_3DSURFACE_HEADER(18) and DGNT_3DSOLID_HEADER(19).
 *
 * Compatible with DGNT_TEXT_NODE (7), see DGNAddRawAttrLink()
 */

typedef struct {
    DGNElemCore core;   /* must be at first position ! */

    int         totlength;     /*!< Total length of surface in words,
                                    excluding the first 19 words
                                    (header + totlength field) */
    int         numelems;      /*!< # of elements in surface */
    int         surftype;      /*!< surface/solid type
                                    (only used for 3D surface/solid).
                                    One of  DGNSUT_* or DGNSOT_*. */
    int         boundelms;     /*!< # of elements in each boundary
                                    (only used for 3D surface/solid). */
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
  DGNElemCore   core;

  int           screen_flag;
  GByte         color_info[256][3]; /*!< Color table, 256 colors by red (0), green(1) and blue(2) component. */
} DGNElemColorTable;

typedef struct {
    int           flags;
    unsigned char levels[8];
    DGNPoint      origin;
    DGNPoint      delta;
    double        transmatrx[9];
    double        conversion;
    unsigned long activez;
} DGNViewInfo;

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
    DGNElemCore core;   /* must be at first position ! */

    int         dimension;         /*!< Dimension (2 or 3) */

    double      origin_x;       /*!< X origin of UOR space in master units(?)*/
    double      origin_y;       /*!< Y origin of UOR space in master units(?)*/
    double      origin_z;       /*!< Z origin of UOR space in master units(?)*/

    long        uor_per_subunit;   /*!< UOR per subunit. */
    char        sub_units[3];      /*!< User name for subunits (2 chars)*/
    long        subunits_per_master; /*!< Subunits per master unit. */
    char        master_units[3];   /*!< User name for master units (2 chars)*/

    DGNViewInfo views[8];

} DGNElemTCB;

/**
 * Cell Header.
 *
 * The core.stype code is DGNST_CELL_HEADER.
 *
 * Returned for DGNT_CELL_HEADER(2).
 */

typedef struct {
    DGNElemCore core;   /* must be at first position ! */

    int         totlength;         /*!< Total length of cell in words,
                                        excluding the first 19 words
                                        (header + totlength field) */
    char        name[7];           /*!< Cell name */
 unsigned short cclass;            /*!< Class bitmap */
 unsigned short levels[4];         /*!< Levels used in cell */

    DGNPoint    rnglow;            /*!< X/Y/Z minimums for cell */
    DGNPoint    rnghigh;           /*!< X/Y/Z maximums for cell */

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
    DGNElemCore core;   /* must be at first position ! */

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
 * Shared Cell Definition.
 *
 * The core.stype code is DGNST_SHARED_CELL_DEFN.
 *
 * Returned for DGNT_SHARED_CELL_DEFN(2).
 */

typedef struct {
    DGNElemCore core;   /* must be at first position ! */

    int         totlength;         /*!< Total length of cell in words,
                                        excluding the first 19 words
                                        (header + totlength field) */
  // FIXME: Most of the contents of this element type is currently
  // unknown. Please get in touch with the authors if you have
  // information about how to decode this element.
} DGNElemSharedCellDefn;

typedef union { char *string; GInt32 integer; double real; } tagValueUnion;

/**
 * Tag Value.
 *
 * The core.stype code is DGNST_TAG_VALUE.
 *
 * Returned for DGNT_TAG_VALUE(37).
 */

typedef struct {
    DGNElemCore core;   /* must be at first position ! */

    int         tagType;           /*!< Tag type indicator, DGNTT_* */
    int         tagSet;            /*!< Which tag set does this relate to? */
    int         tagIndex;          /*!< Tag index within tag set. */
    int         tagLength;         /*!< Length of tag information (text) */
    tagValueUnion tagValue;        /*!< Textual value of tag */

} DGNElemTagValue;

/**
 * Tag definition.
 *
 * Structure holding definition of one tag within a DGNTagSet.
 */
typedef struct _DGNTagDef {
    char        *name;      /*!< Name of this tag. */
    int         id;         /*!< Tag index/identifier. */
    char        *prompt;    /*!< User prompt when requesting value. */
    int         type;       /*!< Tag type (one of DGNTT_STRING(1), DGNTT_INTEGER(3) or DGNTT_FLOAT(4). */
    tagValueUnion defaultValue; /*!< Default tag value */
} DGNTagDef;

#define DGNTT_STRING      1
#define DGNTT_INTEGER     3
#define DGNTT_FLOAT       4

/**
 * Tag Set.
 *
 * The core.stype code is DGNST_TAG_SET.
 *
 * Returned for DGNT_APPLICATION_ELEM(66), Level: 24.
 */

typedef struct {
    DGNElemCore core;   /* must be at first position ! */

    int        tagCount;    /*!< Number of tags in tagList. */
    int        tagSet;      /*!< Tag set index. */
    int        flags;       /*!< Tag flags - not too much known. */
    char       *tagSetName; /*!< Tag set name. */

    DGNTagDef  *tagList;    /*!< List of tag definitions in this set. */

} DGNElemTagSet;

/**
 * Cone element
 *
 * The core.stype code is DGNST_CONE.
 *
 * Used for: DGNT_CONE(23)
 */
typedef struct {
  DGNElemCore core;   /* must be at first position ! */

  short unknown;     /*!< Unknown data */
  int quat[4];      /*!< Orientation quaternion */
  DGNPoint center_1; /*!< center of first circle */
  double radius_1;   /*!< radius of first circle */
  DGNPoint center_2; /*!< center of second circle */
  double radius_2;   /*!< radius of second circle */

} DGNElemCone;

/**
 * Text Node Header.
 *
 * The core.stype code is DGNST_TEXT_NODE.
 *
 * Used for DGNT_TEXT_NODE (7).
 * First fields (up to numelems) are compatible with DGNT_COMPLEX_HEADER (7),
 * \sa DGNAddRawAttrLink()
 */

typedef struct {
  DGNElemCore core;   /* must be at first position ! */

  int       totlength;          /*!<  Total length of the node
                                      (bytes = totlength * 2 + 38) */
  int       numelems;           /*!<  Number of text strings */
  int       node_number;        /*!<  text node number */
  short     max_length;         /*!<  maximum length allowed, characters */
  short     max_used;           /*!<  maximum length used */
  short     font_id;            /*!<  text font used */
  short     justification;      /*!<  justification type, see DGNJ_ */
  long      line_spacing;       /*!<  spacing between text strings */
  double    length_mult;        /*!<  length multiplier */
  double    height_mult;        /*!<  height multiplier */
  double    rotation;           /*!<  rotation angle (2d)*/
  DGNPoint  origin;             /*!<  Snap origin (as defined by user) */

} DGNElemTextNode;

/**
 * B-Spline Surface Header element
 *
 * The core.stype code is DGNST_BSPLINE_SURFACE_HEADER.
 *
 * Used for: DGNT_BSPLINE_SURFACE_HEADER(24)
 */
typedef struct {
  DGNElemCore core;   /* must be at first position ! */

  long desc_words;               /*!< Total length of B-Spline surface in
                                      words, excluding the first 20 words
                                      (header + desc_words field) */
  unsigned char curve_type;      /*!< curve type */
  unsigned char u_order;         /*!< B-spline U order: 2-15 */
  unsigned short u_properties;   /*!< surface U properties:
                                      ORing of DGNBSC_ flags */
  short num_poles_u;             /*!< number of poles */
  short num_knots_u;             /*!< number of knots */
  short rule_lines_u;            /*!< number of rule lines */

  unsigned char v_order;         /*!< B-spline V order: 2-15 */
  unsigned short v_properties;   /*!< surface V properties:
                                      Oring of DGNBSS_ flags */
  short num_poles_v;             /*!< number of poles */
  short num_knots_v;             /*!< number of knots */
  short rule_lines_v;            /*!< number of rule lines */

  short num_bounds;              /*!< number of boundaries */
} DGNElemBSplineSurfaceHeader;

/**
 * B-Spline Curve Header element
 *
 * The core.stype code is DGNST_BSPLINE_CURVE_HEADER.
 *
 * Used for: DGNT_BSPLINE_CURVE_HEADER(27)
 */
typedef struct {
  DGNElemCore core;   /* must be at first position ! */

  long desc_words;               /*!< Total length of B-Spline curve in words,
                                      excluding the first 20 words
                                      (header + desc_words field) */
  unsigned char order;           /*!< B-spline order: 2-15 */
  unsigned char properties;      /*!< Properties: ORing of DGNBSC_ flags */
  unsigned char curve_type;      /*!< curve type */
  short num_poles;               /*!< number of poles, max. 101 */
  short num_knots;               /*!< number of knots */
} DGNElemBSplineCurveHeader;

/**
 * B-Spline Surface Boundary element
 *
 * The core.stype code is DGNST_BSPLINE_SURFACE_BOUNDARY
 *
 * Used for: DGNT_BSPLINE_SURFACE_BOUNDARY(25)
 */
typedef struct {
  DGNElemCore core;   /* must be at first position ! */

  short number;         /*!< boundary number */
  short numverts;       /*!< number of boundary vertices */
  DGNPoint vertices[1]; /*!< Array of 1 or more 2D boundary vertices
                            (in UV space) */
} DGNElemBSplineSurfaceBoundary;

/**
 * B-Spline Knot/Weight element
 *
 * The core.stype code is DGNST_KNOT_WEIGHT
 *
 * Used for: DGNT_BSPLINE_KNOT(26), DGNT_BSPLINE_WEIGHT_FACTOR(28)
 */
typedef struct {
  DGNElemCore core;   /* must be at first position ! */

  float array[1];         /*!< array (variable length). Length is
                             given in the corresponding B-Spline
                             header. */
} DGNElemKnotWeight;

/* -------------------------------------------------------------------- */
/*      Structure types                                                 */
/* -------------------------------------------------------------------- */

/** DGNElemCore style: Element uses DGNElemCore structure */
#define DGNST_CORE                 1

/** DGNElemCore style: Element uses DGNElemMultiPoint structure */
#define DGNST_MULTIPOINT           2

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

/** DGNElemCore style: Element uses DGNElemCone structure */
#define DGNST_CONE                12

/** DGNElemCore style: Element uses DGNElemTextNode structure */
#define DGNST_TEXT_NODE           13

/** DGNElemCore style: Element uses DGNElemBSplineSurfaceHeader structure */
#define DGNST_BSPLINE_SURFACE_HEADER   14

/** DGNElemCore style: Element uses DGNElemBSplineCurveHeader structure */
#define DGNST_BSPLINE_CURVE_HEADER     15

/** DGNElemCore style: Element uses DGNElemBSplineSurfaceBoundary structure */
#define DGNST_BSPLINE_SURFACE_BOUNDARY 16

/** DGNElemCore style: Element uses DGNElemKnotWeight structure */
#define DGNST_KNOT_WEIGHT              17

/** DGNElemCore style: Element uses DGNElemSharedCellDefn structure */
#define DGNST_SHARED_CELL_DEFN         18

/* -------------------------------------------------------------------- */
/*      Element types                                                   */
/* -------------------------------------------------------------------- */
#define DGNT_CELL_LIBRARY              1
#define DGNT_CELL_HEADER               2
#define DGNT_LINE                      3
#define DGNT_LINE_STRING               4
#define DGNT_GROUP_DATA                5
#define DGNT_SHAPE                     6
#define DGNT_TEXT_NODE                 7
#define DGNT_DIGITIZER_SETUP           8
#define DGNT_TCB                       9
#define DGNT_LEVEL_SYMBOLOGY          10
#define DGNT_CURVE                    11
#define DGNT_COMPLEX_CHAIN_HEADER     12
#define DGNT_COMPLEX_SHAPE_HEADER     14
#define DGNT_ELLIPSE                  15
#define DGNT_ARC                      16
#define DGNT_TEXT                     17
#define DGNT_3DSURFACE_HEADER         18
#define DGNT_3DSOLID_HEADER           19
#define DGNT_BSPLINE_POLE             21
#define DGNT_POINT_STRING             22
#define DGNT_BSPLINE_SURFACE_HEADER   24
#define DGNT_BSPLINE_SURFACE_BOUNDARY 25
#define DGNT_BSPLINE_KNOT             26
#define DGNT_BSPLINE_CURVE_HEADER     27
#define DGNT_BSPLINE_WEIGHT_FACTOR    28
#define DGNT_CONE                     23
#define DGNT_SHARED_CELL_DEFN         34
#define DGNT_SHARED_CELL_ELEM         35
#define DGNT_TAG_VALUE                37
#define DGNT_APPLICATION_ELEM         66

/* -------------------------------------------------------------------- */
/*      Line Styles                                                     */
/* -------------------------------------------------------------------- */
#define DGNS_SOLID              0
#define DGNS_DOTTED             1
#define DGNS_MEDIUM_DASH        2
#define DGNS_LONG_DASH          3
#define DGNS_DOT_DASH           4
#define DGNS_SHORT_DASH         5
#define DGNS_DASH_DOUBLE_DOT    6
#define DGNS_LONG_DASH_SHORT_DASH 7

/* -------------------------------------------------------------------- */
/*      3D Surface Types                                                */
/* -------------------------------------------------------------------- */
#define DGNSUT_SURFACE_OF_PROJECTION    0
#define DGNSUT_BOUNDED_PLANE            1
#define DGNSUT_BOUNDED_PLANE2           2
#define DGNSUT_RIGHT_CIRCULAR_CYLINDER  3
#define DGNSUT_RIGHT_CIRCULAR_CONE      4
#define DGNSUT_TABULATED_CYLINDER       5
#define DGNSUT_TABULATED_CONE           6
#define DGNSUT_CONVOLUTE                7
#define DGNSUT_SURFACE_OF_REVOLUTION    8
#define DGNSUT_WARPED_SURFACE           9

/* -------------------------------------------------------------------- */
/*      3D Solid Types                                                  */
/* -------------------------------------------------------------------- */
#define DGNSOT_VOLUME_OF_PROJECTION     0
#define DGNSOT_VOLUME_OF_REVOLUTION     1
#define DGNSOT_BOUNDED_VOLUME           2

/* -------------------------------------------------------------------- */
/*      Class                                                           */
/* -------------------------------------------------------------------- */
#define DGNC_PRIMARY                    0
#define DGNC_PATTERN_COMPONENT          1
#define DGNC_CONSTRUCTION_ELEMENT       2
#define DGNC_DIMENSION_ELEMENT          3
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
#define DGNPF_HOLE         0x8000
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
#define DGNJ_LEFT_TOP           0
#define DGNJ_LEFT_CENTER        1
#define DGNJ_LEFT_BOTTOM        2
#define DGNJ_LEFTMARGIN_TOP     3    /* text node header only */
#define DGNJ_LEFTMARGIN_CENTER  4    /* text node header only */
#define DGNJ_LEFTMARGIN_BOTTOM  5    /* text node header only */
#define DGNJ_CENTER_TOP         6
#define DGNJ_CENTER_CENTER      7
#define DGNJ_CENTER_BOTTOM      8
#define DGNJ_RIGHTMARGIN_TOP    9    /* text node header only */
#define DGNJ_RIGHTMARGIN_CENTER 10   /* text node header only */
#define DGNJ_RIGHTMARGIN_BOTTOM 11   /* text node header only */
#define DGNJ_RIGHT_TOP          12
#define DGNJ_RIGHT_CENTER       13
#define DGNJ_RIGHT_BOTTOM       14

/* -------------------------------------------------------------------- */
/*      DGN file reading options.                                       */
/* -------------------------------------------------------------------- */
#define DGNO_CAPTURE_RAW_DATA   0x01

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
#define DGNLT_ASSOC_ID          0x7D2F

/* -------------------------------------------------------------------- */
/*      File creation options.                                          */
/* -------------------------------------------------------------------- */

#define DGNCF_USE_SEED_UNITS              0x01
#define DGNCF_USE_SEED_ORIGIN             0x02
#define DGNCF_COPY_SEED_FILE_COLOR_TABLE  0x04
#define DGNCF_COPY_WHOLE_SEED_FILE        0x08

/* -------------------------------------------------------------------- */
/*      B-Spline Curve flags. Also used for U-direction of surfaces     */
/* -------------------------------------------------------------------- */
#define DGNBSC_CURVE_DISPLAY              0x10
#define DGNBSC_POLY_DISPLAY               0x20
#define DGNBSC_RATIONAL                   0x40
#define DGNBSC_CLOSED                     0x80

/* -------------------------------------------------------------------- */
/*      B-Spline Curve flags for V-direction of surfaces.               */
/* -------------------------------------------------------------------- */
#define DGNBSS_ARC_SPACING                0x40
#define DGNBSS_CLOSED                     0x80

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
int CPL_DLL          DGNGetDimension( DGNHandle );
DGNElemCore CPL_DLL *DGNReadElement( DGNHandle );
void CPL_DLL         DGNFreeElement( DGNHandle, DGNElemCore * );
void CPL_DLL         DGNRewind( DGNHandle );
int  CPL_DLL         DGNGotoElement( DGNHandle, int );
void CPL_DLL         DGNClose( DGNHandle );
int  CPL_DLL         DGNLoadTCB( DGNHandle );
int  CPL_DLL         DGNLookupColor( DGNHandle, int, int *, int *, int * );
int  CPL_DLL         DGNGetShapeFillInfo( DGNHandle, DGNElemCore *, int * );
int  CPL_DLL         DGNGetAssocID( DGNHandle, DGNElemCore * );
int  CPL_DLL         DGNGetElementExtents( DGNHandle, DGNElemCore *,
                                           DGNPoint *, DGNPoint * );

void CPL_DLL         DGNDumpElement( DGNHandle, DGNElemCore *, FILE * );
const char CPL_DLL  *DGNTypeToName( int );

void CPL_DLL  DGNRotationToQuaternion( double, int * );
void CPL_DLL  DGNQuaternionToMatrix( int *, float * );
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

/* Write API */

int  CPL_DLL  DGNWriteElement( DGNHandle, DGNElemCore * );
int  CPL_DLL  DGNResizeElement( DGNHandle, DGNElemCore *, int );
DGNHandle CPL_DLL
      DGNCreate( const char *pszNewFilename, const char *pszSeedFile,
                 int nCreationFlags,
                 double dfOriginX, double dfOriginY, double dfOriginZ,
                 int nMasterUnitPerSubUnit, int nUORPerSubUnit,
                 const char *pszMasterUnits, const char *pszSubUnits );
DGNElemCore CPL_DLL *DGNCloneElement( DGNHandle hDGNSrc, DGNHandle hDGNDst,
                                      DGNElemCore *psSrcElement );
int CPL_DLL   DGNUpdateElemCore( DGNHandle hDGN, DGNElemCore *psElement,
                                 int nLevel, int nGraphicGroup, int nColor,
                                 int nWeight, int nStyle );
int CPL_DLL   DGNUpdateElemCoreExtended( DGNHandle hDGN,
                                         DGNElemCore *psElement );

DGNElemCore CPL_DLL *
              DGNCreateMultiPointElem( DGNHandle hDGN, int nType,
                                       int nPointCount, DGNPoint*pasVertices );
DGNElemCore CPL_DLL  *
              DGNCreateArcElem2D( DGNHandle hDGN, int nType,
                                  double dfOriginX, double dfOriginY,
                                  double dfPrimaryAxis, double dfSecondaryAxis,
                                  double dfRotation,
                                  double dfStartAngle, double dfSweepAngle );

DGNElemCore CPL_DLL  *
              DGNCreateArcElem( DGNHandle hDGN, int nType,
                                double dfOriginX, double dfOriginY,
                                double dfOriginZ,
                                double dfPrimaryAxis, double dfSecondaryAxis,
                                double dfStartAngle, double dfSweepAngle,
                                double dfRotation, int *panQuaternion );

DGNElemCore CPL_DLL  *
              DGNCreateConeElem( DGNHandle hDGN,
                                 double center_1X, double center_1Y,
                                 double center_1Z, double radius_1,
                                 double center_2X, double center_2Y,
                                 double center_2Z, double radius_2,
                                 int *panQuaternion );

DGNElemCore CPL_DLL *
             DGNCreateTextElem( DGNHandle hDGN, const char *pszText,
                                int nFontId, int nJustification,
                                double dfLengthMult, double dfHeightMult,
                                double dfRotation, int *panQuaternion,
                       double dfOriginX, double dfOriginY, double dfOriginZ );

DGNElemCore CPL_DLL *
            DGNCreateColorTableElem( DGNHandle hDGN, int nScreenFlag,
                                     GByte abyColorInfo[256][3] );
DGNElemCore CPL_DLL *
DGNCreateComplexHeaderElem( DGNHandle hDGN, int nType,
                            int nTotLength, int nNumElems );
DGNElemCore CPL_DLL *
DGNCreateComplexHeaderFromGroup( DGNHandle hDGN, int nType,
                                 int nNumElems, DGNElemCore **papsElems );

DGNElemCore CPL_DLL *
DGNCreateSolidHeaderElem( DGNHandle hDGN, int nType, int nSurfType,
                          int nBoundElems, int nTotLength, int nNumElems );
DGNElemCore CPL_DLL *
DGNCreateSolidHeaderFromGroup( DGNHandle hDGN, int nType, int nSurfType,
                               int nBoundElems, int nNumElems,
                               DGNElemCore **papsElems );

DGNElemCore CPL_DLL  *
DGNCreateCellHeaderElem( DGNHandle hDGN, int nTotLength, const char *pszName,
                         short nClass, short *panLevels,
                         DGNPoint *psRangeLow, DGNPoint *psRangeHigh,
                         DGNPoint *psOrigin, double dfXScale, double dfYScale,
                         double dfRotation );

DGNElemCore CPL_DLL *
DGNCreateCellHeaderFromGroup( DGNHandle hDGN, const char *pszName,
                              short nClass, short *panLevels,
                              int nNumElems, DGNElemCore **papsElems,
                              DGNPoint *psOrigin,
                              double dfXScale, double dfYScale,
                              double dfRotation );

int CPL_DLL DGNAddMSLink( DGNHandle hDGN, DGNElemCore *psElement,
                          int nLinkageType, int nEntityNum, int nMSLink );

int CPL_DLL DGNAddRawAttrLink( DGNHandle hDGN, DGNElemCore *psElement,
                               int nLinkSize, unsigned char *pabyRawLinkData );

int CPL_DLL DGNAddShapeFillInfo( DGNHandle hDGN, DGNElemCore *psElement,
                                 int nColor );

int CPL_DLL DGNElemTypeHasDispHdr( int nElemType );

CPL_C_END

#endif /* ndef DGNLIB_H_INCLUDED */
