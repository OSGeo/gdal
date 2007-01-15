/******************************************************************************
 * $Id$
 *
 * Project:  Microstation DGN Access Library
 * Purpose:  Internal (privatE) datastructures, and prototypes for DGN Access 
 *           Library.
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
 * Revision 1.18  2003/06/27 14:50:53  warmerda
 * avoid warnings
 *
 * Revision 1.17  2003/05/21 03:42:01  warmerda
 * Expanded tabs
 *
 * Revision 1.16  2003/01/20 20:07:19  warmerda
 * added ascii to rad50 prototype
 *
 * Revision 1.15  2002/11/12 19:44:51  warmerda
 * fixed up DGN_WRITE_INT32 macro
 *
 * Revision 1.14  2002/11/11 20:37:07  warmerda
 * added write related stuff
 *
 * Revision 1.13  2002/03/14 21:39:09  warmerda
 * added DGNLoadRawElement, max_element_count
 *
 * Revision 1.12  2002/02/22 22:17:42  warmerda
 * Ensure that components of complex chain/shapes are spatially selected
 * based on the decision made for their owner (header).
 *
 * Revision 1.11  2002/02/06 20:32:33  warmerda
 * handle improbably large elements
 *
 * Revision 1.10  2002/01/21 20:52:45  warmerda
 * added spatial filter support
 *
 * Revision 1.9  2002/01/15 06:39:08  warmerda
 * added default PI value
 *
 * Revision 1.8  2001/12/19 15:29:56  warmerda
 * added preliminary cell header support
 *
 * Revision 1.7  2001/08/21 03:01:39  warmerda
 * added raw_data support
 *
 * Revision 1.6  2001/03/07 13:56:44  warmerda
 * updated copyright to be held by Avenza Systems
 *
 * Revision 1.5  2001/01/16 18:12:18  warmerda
 * keep color table in DGNInfo
 *
 * Revision 1.4  2001/01/10 16:12:18  warmerda
 * added extents capture
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

#ifndef _DGNLIBP_H_INCLUDED
#define _DGNLIBP_H_INCLUDED

#include "dgnlib.h"


#ifndef PI
#define PI  3.1415926535897932384626433832795
#endif

typedef struct {
    FILE        *fp;
    int         next_element_id;

    int         nElemBytes;
    GByte       abyElem[131076];

    int         got_tcb;
    int         dimension;
    int         options;
    double      scale;
    double      origin_x;
    double      origin_y;
    double      origin_z;

    int         index_built;
    int         element_count;
    int         max_element_count;
    DGNElementInfo *element_index;

    int         got_color_table;
    GByte       color_table[256][3];

    int         got_bounds;
    GUInt32     min_x;
    GUInt32     min_y;
    GUInt32     min_z;
    GUInt32     max_x;
    GUInt32     max_y;
    GUInt32     max_z;

    int         has_spatial_filter;
    int         sf_converted_to_uor;

    int         select_complex_group;
    int         in_complex_group;

    GUInt32     sf_min_x;
    GUInt32     sf_min_y;
    GUInt32     sf_max_x;
    GUInt32     sf_max_y;

    double      sf_min_x_geo;
    double      sf_min_y_geo;
    double      sf_max_x_geo;
    double      sf_max_y_geo;
} DGNInfo;

#define DGN_INT32( p )  ((p)[2] \
                        + (p)[3]*256 \
                        + (p)[1]*65536*256 \
                        + (p)[0]*65536)
#define DGN_WRITE_INT32( n, p ) { GInt32 nMacroWork = (n);                   \
 ((unsigned char *)p)[0] = (unsigned char)((nMacroWork & 0x00ff0000) >> 16); \
 ((unsigned char *)p)[1] = (unsigned char)((nMacroWork & 0xff000000) >> 24); \
 ((unsigned char *)p)[2] = (unsigned char)((nMacroWork & 0x000000ff) >> 0);  \
 ((unsigned char *)p)[3] = (unsigned char)((nMacroWork & 0x0000ff00) >> 8); }

int DGNParseCore( DGNInfo *, DGNElemCore * );
void DGNTransformPoint( DGNInfo *, DGNPoint * );
void DGNInverseTransformPoint( DGNInfo *, DGNPoint * );
void DGNInverseTransformPointToInt( DGNInfo *, DGNPoint *, unsigned char * );
void DGN2IEEEDouble( void * );
void IEEE2DGNDouble( void * );
void DGNBuildIndex( DGNInfo * );
void DGNRad50ToAscii( unsigned short rad50, char *str );
void DGNAsciiToRad50( const char *str, unsigned short *rad50 );
void DGNSpatialFilterToUOR( DGNInfo *);
int  DGNLoadRawElement( DGNInfo *psDGN, int *pnType, int *pnLevel );

#endif /* ndef _DGNLIBP_H_INCLUDED */
