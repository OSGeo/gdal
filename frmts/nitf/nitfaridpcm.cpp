/******************************************************************************
 *
 * Project:  NITF Read/Write Library
 * Purpose:  ARIDPCM reading code.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 **********************************************************************
 * Copyright (c) 2007, Frank Warmerdam
 * Copyright (c) 2009, Even Rouault <even dot rouault at spatialys.com>
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
#include "nitflib.h"

#include <cstring>

#include "gdal.h"
#include "cpl_conv.h"
#include "cpl_error.h"

CPL_CVSID("$Id$")

constexpr int neighbourhood_size_75[4] = { 23, 47, 74, 173 };
constexpr int bits_per_level_by_busycode_75[4/*busy code*/][4/*level*/] = {
    { 8, 5, 0, 0 }, // BC = 00
    { 8, 5, 2, 0 }, // BC = 01
    { 8, 6, 4, 0 }, // BC = 10
    { 8, 7, 4, 2 }};// BC = 11

constexpr int CR075 = 1;

// Level for each index value.
constexpr int level_index_table[64] =
{ 0,
  1, 1, 1,
  2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
  3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3,
  3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3,
  3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3 };

// List of i,j to linear index macros mappings.
// Note that i is vertical and j is horizontal and progression is
// right to left, bottom to top.

#define IND(i,j) (ij_index[i+j*8]-1)

constexpr int ij_index[64] = {

     1, // 0, 0
    18, // 1, 0
     6, // 2, 0
    30, // 3, 0
     3, // 4, 0
    42, // 5, 0
    12, // 6, 0
    54, // 7, 0

    17, // 0, 1
    19, // 1, 1
    29, // 2, 1
    31, // 3, 1
    41, // 4, 1
    43, // 5, 1
    53, // 6, 1
    55, // 7, 1

     5, // 0, 2
    21, // 1, 2
     7, // 2, 2
    33, // 3, 2
    11, // 4, 2
    45, // 5, 2
    13, // 6, 2
    57, // 7, 2

    20, // 0, 3
    22, // 1, 3
    32, // 2, 3
    34, // 3, 3
    44, // 4, 3
    46, // 5, 3
    56, // 6, 3
    58, // 7, 3

     2, // 0, 4
    24, // 1, 4
     9, // 2, 4
    36, // 3, 4
     4, // 4, 4
    48, // 5, 4
    15, // 6, 4
    60, // 7, 4

    23, // 0, 5
    25, // 1, 5
    35, // 2, 5
    37, // 3, 5
    47, // 4, 5
    49, // 5, 5
    59, // 6, 5
    61, // 7, 5

     8, // 0, 6
    27, // 1, 6
    10, // 2, 6
    39, // 3, 6
    14, // 4, 6
    51, // 5, 6
    16, // 6, 6
    63, // 7, 6

    26, // 0, 7
    28, // 1, 7
    38, // 2, 7
    40, // 3, 7
    50, // 4, 7
    52, // 5, 7
    62, // 6, 7
    64};// 7, 7

constexpr int delta_075_level_2_bc_0[32] =
{-71, -49, -38, -32, -27, -23, -20, -17, -14, -12, -10, -8, -6, -4, -3, -1,
 1, 2, 4, 6, 8, 12, 14, 16, 19, 22, 26, 31, 37, 46, 72 };
constexpr int delta_075_level_2_bc_1[32] =
{-71, -49, -38, -32, -27, -23, -20, -17, -14, -12, -10, -8, -6, -4, -3, -1,
 1, 2, 4, 6, 8, 12, 14, 16, 19, 22, 26, 31, 37, 46, 72 };
constexpr int delta_075_level_2_bc_2[64] =
{ -109, -82, -68, -59, -52, -46, -41, -37, -33, -30, -27, -25, -22, -20,
  -18, -16, -15, -13, -11, -10, -9, -8, -7, -6, -5,
  -4, -3, -2, -1, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9,10,11,12,
  13,14,15,16,17,18,19,20,21,24,26,28,31,35,38,
  42,47,52,60,69,85,118};
constexpr int delta_075_level_2_bc_3[128] =
{-159,-134,-122,-113,-106,-100,-94,-88,-83,-79,-76,-72,-69,-66,-63,-61,
 -58,-56,-54,-52,-50,-48,-47,-45,-43,-42,-40,-39,-37,-36,-35,-33,-32,-31,
 -30,-29,-28,-27,-25,-24,-23,-22,-21,-20,-19,-18,-17,-16,-15,-14,
 -13,-12,-11,-10,-9,-8,-7,-6,-5,-4,-3,-2,-1,0,1,2,3,4,5,6,7,8,9,10,11,
 12,13,14,15,16,17,18,19,20,21,22,23,24,25,26,27,28,29,30,31,32,33,34,
 35,36,37,38,39,40,41,42,43,45,48,52,56,60,64,68,73,79,85,92,100,109,
 118,130,144,159,177,196,217,236};
static const int * const delta_075_level_2[4] =
{ delta_075_level_2_bc_0, delta_075_level_2_bc_1,
  delta_075_level_2_bc_2, delta_075_level_2_bc_3 };

constexpr int delta_075_level_3_bc_1[4] = { -24, -6, 6, 24 };
constexpr int delta_075_level_3_bc_2[16] =
{-68,-37,-23,-15, -9, -6, -3, -1, 1, 4, 7,10,16,24,37,70 };
constexpr int delta_075_level_3_bc_3[16] =
{-117,-72, -50, -36, -25, -17, -10, -5,-1, 3, 7,14,25,45,82,166};
static const int *const delta_075_level_3[4] =
{ nullptr, delta_075_level_3_bc_1,
  delta_075_level_3_bc_2, delta_075_level_3_bc_3 };

constexpr int delta_075_level_4_bc_3[4] = {-47,-8,4,43};
static const int *const delta_075_level_4[4] = { nullptr, nullptr, nullptr, delta_075_level_4_bc_3 };

static const int *const * const delta_075_by_level_by_bc[4] =
{ nullptr, delta_075_level_2, delta_075_level_3, delta_075_level_4 };

/************************************************************************/
/*                              get_bits()                              */
/************************************************************************/

static int
get_bits( unsigned char *buffer, int first_bit, int num_bits )

{
    int total =0;

    for( int i = first_bit; i < first_bit+num_bits; i++ )
    {
        total *= 2;
        if( buffer[i>>3] & (0x80 >> (i&7)) )
            total++;
    }

    return total;
}

/************************************************************************/
/*                             get_delta()                              */
/*                                                                      */
/*      Compute the delta value for a particular (i,j) location.        */
/************************************************************************/
static int
get_delta( unsigned char *srcdata,
           int nInputBytes,
           int busy_code,
           CPL_UNUSED int comrat,
           int block_offset,
           CPL_UNUSED int block_size,
           int i,
           int j,
           int *pbError )

{
    CPLAssert( comrat == CR075 );
    const int pixel_index = IND(i,j);
    const int level_index = level_index_table[pixel_index];
    const int *bits_per_level = bits_per_level_by_busycode_75[busy_code];
    const int delta_bits = bits_per_level[level_index];
    int delta_offset = 0;

    *pbError = FALSE;

    if( delta_bits == 0 )
        return 0;

    if( level_index == 3 )
        delta_offset = bits_per_level[0] + bits_per_level[1] * 3
            + bits_per_level[2] * 12 + (pixel_index - 16) * bits_per_level[3];
    else if( level_index == 2 )
        delta_offset = bits_per_level[0] + bits_per_level[1] * 3
            + (pixel_index - 4) * bits_per_level[2];
    else if( level_index == 1 )
        delta_offset = bits_per_level[0] + (pixel_index-1)*bits_per_level[1];

    if (nInputBytes * 8 < block_offset+delta_offset + delta_bits)
    {
        CPLError( CE_Failure, CPLE_AppDefined, "Input buffer too small");
        *pbError = TRUE;
        return 0;
    }

    const int delta_raw = get_bits( srcdata, block_offset+delta_offset, delta_bits );

    /* Should not happen as delta_075_by_level_by_bc[level_index] == NULL if and
       only if level_index == 0, which means that pixel_index == 0, which means
       (i, j) = (0, 0). That cannot happen as we are never called with those
       values
    */
    CPLAssert( delta_075_by_level_by_bc[level_index] != nullptr );
    const int *lookup_table = delta_075_by_level_by_bc[level_index][busy_code];

    CPLAssert( lookup_table != nullptr );
    int delta = lookup_table[delta_raw];

    return delta;
}

/************************************************************************/
/*                            decode_block()                            */
/*                                                                      */
/*      Decode one 8x8 block.  The 9x9 L buffer is pre-loaded with      */
/*      the left and top values from previous blocks.                   */
/************************************************************************/
static int
decode_block( unsigned char *srcdata, int nInputBytes,
              int busy_code, int comrat,
              int block_offset, int block_size,
              int left_side, int top_side, int L[9][9] )

{
    int bError;

    // Level 2
    L[0][4] = (L[0][0] + L[0][8])/2
        + get_delta(srcdata,nInputBytes,busy_code,comrat,block_offset,block_size,0,4,&bError);
    if (bError) return FALSE;
    L[4][0] = (L[0][0] + L[8][0])/2
        + get_delta(srcdata,nInputBytes,busy_code,comrat,block_offset,block_size,4,0,&bError);
    if (bError) return FALSE;
    L[4][4] = (L[0][0] + L[8][0] + L[0][8] + L[8][8])/4
        + get_delta(srcdata,nInputBytes,busy_code,comrat,block_offset,block_size,4,4,&bError);
    if (bError) return FALSE;

    if( left_side )
        L[4][8] = L[4][0];
    if( top_side )
        L[8][4] = L[0][4];

    // Level 3
    for( int i = 0; i < 8; i += 4 )
    {
        for( int j = 0; j < 8; j += 4 )
        {
            // above
            L[i+2][j] = (L[i][j]+L[i+4][j])/2
                + get_delta(srcdata,nInputBytes,busy_code,comrat,
                            block_offset,block_size,i+2,j,&bError);
            if (bError) return FALSE;
            // left
            L[i][j+2] = (L[i][j]+L[i][j+4])/2
                + get_delta(srcdata,nInputBytes,busy_code,comrat,
                            block_offset,block_size,i,j+2,&bError);
            if (bError) return FALSE;
            // up-left
            L[i+2][j+2] = (L[i][j]+L[i][j+4]+L[i+4][j]+L[i+4][j+4])/4
                + get_delta(srcdata,nInputBytes,busy_code,comrat,
                            block_offset,block_size,i+2,j+2,&bError);
            if (bError) return FALSE;
        }
    }

    if( left_side )
    {
        L[2][8] = L[2][0];
        L[6][8] = L[6][0];
    }
    if( top_side )
    {
        L[8][2] = L[0][2];
        L[8][6] = L[0][6];
    }

    // Level 4
    for( int i = 0; i < 8; i += 2 )
    {
        for( int j = 0; j < 8; j += 2 )
        {
            // above
            L[i+1][j] = (L[i][j]+L[i+2][j])/2
                + get_delta(srcdata,nInputBytes,busy_code,comrat,
                            block_offset,block_size,i+1,j,&bError);
            if (bError) return FALSE;
            // left
            L[i][j+1] = (L[i][j]+L[i][j+2])/2
                + get_delta(srcdata,nInputBytes,busy_code,comrat,
                            block_offset,block_size,i,j+1,&bError);
            if (bError) return FALSE;
            // up-left
            L[i+1][j+1] = (L[i][j]+L[i][j+2]+L[i+2][j]+L[i+2][j+2])/4
                + get_delta(srcdata,nInputBytes,busy_code,comrat,
                            block_offset,block_size,i+1,j+1,&bError);
            if (bError) return FALSE;
        }
    }

    return TRUE;
}

/************************************************************************/
/*                       NITFUncompressARIDPCM()                        */
/************************************************************************/

int NITFUncompressARIDPCM( NITFImage *psImage,
                           GByte *pabyInputData,
                           int nInputBytes,
                           GByte *pabyOutputImage )

{
/* -------------------------------------------------------------------- */
/*      First, verify that we are a COMRAT 0.75 image, which is all     */
/*      we currently support.                                           */
/* -------------------------------------------------------------------- */
    if( !EQUAL(psImage->szCOMRAT,"0.75") )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "COMRAT=%s ARIDPCM is not supported.\n"
                  "Currently only 0.75 is supported.",
                  psImage->szCOMRAT );
        return FALSE;
    }

/* -------------------------------------------------------------------- */
/*      Setup up the various info we need for each 8x8 neighbourhood    */
/*      (which we call blocks in this context).                         */
/* -------------------------------------------------------------------- */
    const int blocks_x = (psImage->nBlockWidth + 7) / 8;
    const int blocks_y = (psImage->nBlockHeight + 7) / 8;
    const int block_count = blocks_x * blocks_y;
    const int rowlen = blocks_x * 8;

    if( psImage->nBlockWidth > 1000 || /* to detect int overflow above */
        psImage->nBlockHeight > 1000 ||
        block_count > 1000 )
    {
        CPLError( CE_Failure, CPLE_AppDefined, "Block too large to be decoded");
        return FALSE;
    }

    int block_offset[1000];
    int block_size[1000];
    int busy_code[1000];
    const int busy_code_table_size = blocks_x * blocks_y * 2;
    unsigned char L00[1000];

    /* to make clang static analyzer happy */
    block_offset[0] = 0;
    block_size[0] = 0;
    busy_code[0] = 0;
    L00[0] = 0;
    CPL_IGNORE_RET_VAL(busy_code[0]);
    CPL_IGNORE_RET_VAL(block_size[0]);
    CPL_IGNORE_RET_VAL(busy_code[0]);
    CPL_IGNORE_RET_VAL(L00[0]);

/* -------------------------------------------------------------------- */
/*      We allocate a working copy of the full image that may be a      */
/*      bit larger than the output buffer if the width or height is     */
/*      not divisible by 8.                                             */
/* -------------------------------------------------------------------- */
    GByte *full_image = reinterpret_cast<GByte *>(
        CPLMalloc(blocks_x * blocks_y * 8 * 8 ) );

/* -------------------------------------------------------------------- */
/*      Scan through all the neighbourhoods determining the busyness    */
/*      code, and the offset to each's data as well as the L00 value.   */
/* -------------------------------------------------------------------- */
    int total = busy_code_table_size;

    for( int i = 0; i < blocks_x * blocks_y; i++ )
    {
        if (nInputBytes * 8 < i * 2 + 2)
        {
            CPLError( CE_Failure, CPLE_AppDefined, "Input buffer too small");
            CPLFree(full_image);
            return FALSE;
        }
        busy_code[i] = get_bits( pabyInputData, i*2, 2 );

        block_offset[i] = total;
        block_size[i] = neighbourhood_size_75[busy_code[i]];

        if (nInputBytes * 8 < block_offset[i] + 8)
        {
            CPLError( CE_Failure, CPLE_AppDefined, "Input buffer too small");
            CPLFree(full_image);
            return FALSE;
        }
        L00[i] = (unsigned char) get_bits( pabyInputData, block_offset[i], 8 );

        total += block_size[i];
    }

/* -------------------------------------------------------------------- */
/*      Process all the blocks, forming into a final image.             */
/* -------------------------------------------------------------------- */
    for( int iY = 0; iY < blocks_y; iY++ )
    {
        for( int iX = 0; iX < blocks_x; iX++ )
        {
            int iBlock = iX + iY * blocks_x;
            int L[9][9];
            unsigned char *full_tl = full_image + iX * 8 + iY * 8 * rowlen;

            L[0][0] = L00[iBlock];
            if( iX > 0 )
            {
                L[0][8] = full_tl[rowlen * 7 - 1];
                L[2][8] = full_tl[rowlen * 5 - 1];
                L[4][8] = full_tl[rowlen * 3 - 1];
                L[6][8] = full_tl[rowlen * 1 - 1];
            }
            else
            {
                L[0][8] = L[0][0];
                L[2][8] = L[0][8]; // need to reconstruct the rest!
                L[4][8] = L[0][8];
                L[6][8] = L[0][8];
            }

            if( iY > 0 )
            {
                L[8][0] = full_tl[7 - rowlen];
                L[8][2] = full_tl[5 - rowlen];
                L[8][4] = full_tl[3 - rowlen];
                L[8][6] = full_tl[1 - rowlen];
            }
            else
            {
                L[8][0] = L[0][0];
                L[8][2] = L[0][0]; // Need to reconstruct the rest!
                L[8][4] = L[0][0];
                L[8][5] = L[0][0];
            }

            if( iX == 0 || iY == 0 )
                L[8][8] = L[0][0];
            else
                L[8][8] = full_tl[-1-rowlen];

            if (!(decode_block( pabyInputData, nInputBytes, busy_code[iBlock], CR075,
                          block_offset[iBlock], block_size[iBlock],
                          iX == 0, iY == 0, L )))
            {
                CPLFree( full_image );
                return FALSE;
            }

            // Assign to output matrix.
            for( int i = 0; i < 8; i++ )
            {
                for( int j = 0; j < 8; j++ )
                {
                    int value = L[i][j];
                    if( value < 0 )
                        value = 0;
                    if( value > 255 )
                        value = 255;

                    full_tl[8-j-1 + (8-i-1) * rowlen] = (unsigned char) value;
                }
            }
        }
    }

/* -------------------------------------------------------------------- */
/*      Copy full image back into target buffer, and free.              */
/* -------------------------------------------------------------------- */
    for( int iY = 0; iY < psImage->nBlockHeight; iY++ )
    {
        memcpy( pabyOutputImage + iY * psImage->nBlockWidth,
                full_image + iY * rowlen,
                psImage->nBlockWidth );
    }

    CPLFree( full_image );

    return TRUE;
}
