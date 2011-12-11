/******************************************************************************
 * $Id$
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  VSI*L wrapper for shptree.c
 * Author:   Even Rouault, <even dot rouault at mines dash paris dot org>
 *
 ******************************************************************************
 * Copyright (c) 2011, Even Rouault
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

#include "cpl_port.h"
#include "cpl_vsi.h"

#define SHPAPI_CALL
#define SHPAPI_CALL1(x)  x

#define FILE    VSILFILE
#define fopen   VSIFOpenL
#define fclose  VSIFCloseL
#define fseek   VSIFSeekL
#define fread   VSIFReadL
#define fwrite  VSIFWriteL

/* We redefine all non static symbols to make sure not conflict */
/* with regular shptree.c */

#define SHPCreateTree VSI_SHPCreateTree
#define SHPDestroyTree VSI_SHPDestroyTree
#define SHPWriteTree VSI_SHPWriteTree
#define SHPTreeAddShapeId VSI_SHPTreeAddShapeId
#define SHPTreeRemoveShapeId VSI_SHPTreeRemoveShapeId
#define SHPTreeTrimExtraNodes VSI_SHPTreeTrimExtraNodes
#define SHPTreeFindLikelyShapes VSI_SHPTreeFindLikelyShapes
#define SHPCheckBoundsOverlap VSI_SHPCheckBoundsOverlap
#define SHPSearchDiskTree VSI_SHPSearchDiskTree
#define SHPTreeSplitBounds VSI_SHPTreeSplitBounds
#define SHPTreeCollectShapeIds VSI_SHPTreeCollectShapeIds

#include "shptree.c"
