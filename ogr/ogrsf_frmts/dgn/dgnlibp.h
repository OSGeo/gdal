/******************************************************************************
 * $Id$
 *
 * Project:  Microstation DGN Access Library
 * Purpose:  Internal (privatE) datastructures, and prototypes for DGN Access 
 *           Library.
 * Author:   Frank Warmerdam, warmerda@home.com
 *
 ******************************************************************************
 * Copyright (c) 2000, Frank Warmerdam (warmerda@home.com)
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
    int		nElementOffset;

    int         nElemBytes;
    GByte	abyElem[65540];

    int         got_tcb;
    int         dimension;
    double	scale;
    double	origin_x;
    double	origin_y;
    double	origin_z;
} DGNInfo;

#define DGN_INT32( p )	((p)[2] \
			+ (p)[3]*256 \
                        + (p)[1]*65536*256 \
                        + (p)[0]*65536)

int DGNParseCore( DGNInfo *, DGNElemCore * );
void DGNTransformPoint( DGNInfo *, DGNPoint * );
void DGN2IEEEDouble( void * );

#endif /* ndef _DGNLIBP_H_INCLUDED */
