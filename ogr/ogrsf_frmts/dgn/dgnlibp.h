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
 ****************************************************************************/

#ifndef DGNLIBP_H_INCLUDED
#define DGNLIBP_H_INCLUDED

#include "cpl_vsi.h"
#include "cpl_vax.h"
#include "dgnlib.h"

typedef struct
{
    VSILFILE *fp;
    int next_element_id;

    int nElemBytes;
    GByte abyElem[131076 + 1];

    bool got_tcb;
    int dimension;
    int options;
    double scale;
    double origin_x;
    double origin_y;
    double origin_z;

    bool index_built;
    int element_count;
    int max_element_count;
    DGNElementInfo *element_index;

    int got_color_table;
    GByte color_table[256][3];

    bool got_bounds;
    uint32_t min_x;
    uint32_t min_y;
    uint32_t min_z;
    uint32_t max_x;
    uint32_t max_y;
    uint32_t max_z;

    bool has_spatial_filter;
    bool sf_converted_to_uor;

    bool select_complex_group;
    bool in_complex_group;

    uint32_t sf_min_x;
    uint32_t sf_min_y;
    uint32_t sf_max_x;
    uint32_t sf_max_y;

    double sf_min_x_geo;
    double sf_min_y_geo;
    double sf_max_x_geo;
    double sf_max_y_geo;
} DGNInfo;

#define DGN_INT32(p) ((p)[2] + ((p)[3] << 8) + ((p)[1] << 24) + ((p)[0] << 16))
#define DGN_WRITE_INT32(n, p)                                                  \
    {                                                                          \
        int32_t nMacroWork = (int32_t)(n);                                     \
        ((unsigned char *)p)[0] =                                              \
            (unsigned char)((nMacroWork & 0x00ff0000) >> 16);                  \
        ((unsigned char *)p)[1] =                                              \
            (unsigned char)((nMacroWork & 0xff000000) >> 24);                  \
        ((unsigned char *)p)[2] =                                              \
            (unsigned char)((nMacroWork & 0x000000ff) >> 0);                   \
        ((unsigned char *)p)[3] =                                              \
            (unsigned char)((nMacroWork & 0x0000ff00) >> 8);                   \
    }

int DGNParseCore(DGNInfo *, DGNElemCore *);
void DGNTransformPoint(DGNInfo *, DGNPoint *);
void DGNInverseTransformPoint(DGNInfo *, DGNPoint *);
void DGNInverseTransformPointToInt(DGNInfo *, DGNPoint *, unsigned char *);
#define DGN2IEEEDouble CPLVaxToIEEEDouble
#define IEEE2DGNDouble CPLIEEEToVaxDouble
void DGNBuildIndex(DGNInfo *);
void DGNRad50ToAscii(unsigned short rad50, char *str);
void DGNAsciiToRad50(const char *str, unsigned short *rad50);
void DGNSpatialFilterToUOR(DGNInfo *);
int DGNLoadRawElement(DGNInfo *psDGN, int *pnType, int *pnLevel);

#endif /* ndef DGNLIBP_H_INCLUDED */
