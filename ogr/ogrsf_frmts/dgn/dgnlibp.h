/******************************************************************************
 * $Id$
 *
 * Project:  Microstation DGN Access Library
 * Purpose:  Internal (privatE) datastructures, and prototypes for DGN Access 
 *           Library.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2000, Frank Warmerdam (warmerdam@pobox.com)
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

typedef struct {
    FILE	*fp;
    int		next_element_id;

    int         nElemBytes;
    GByte	abyElem[65540];

    int         got_tcb;
    int         dimension;
    double	scale;
    double	origin_x;
    double	origin_y;
    double	origin_z;

    int         index_built;
    int	        element_count;
    DGNElementInfo *element_index;

    int         got_color_table;
    GByte	color_table[256][3];

    int         got_bounds;
    GUInt32	min_x;
    GUInt32	min_y;
    GUInt32	min_z;
    GUInt32	max_x;
    GUInt32	max_y;
    GUInt32	max_z;

} DGNInfo;

#define DGN_INT32( p )	((p)[2] \
			+ (p)[3]*256 \
                        + (p)[1]*65536*256 \
                        + (p)[0]*65536)

int DGNParseCore( DGNInfo *, DGNElemCore * );
void DGNTransformPoint( DGNInfo *, DGNPoint * );
void DGN2IEEEDouble( void * );
void DGNBuildIndex( DGNInfo * );

#endif /* ndef _DGNLIBP_H_INCLUDED */
