/******************************************************************************
 * $Id$
 *
 * Project:  Raster Matrix Format
 * Purpose:  Implementation of the LZW compression algorithm as used in
 *           GIS "Panorama"/"Integratsia" raster files. Based on implementation
 *           of Kent Williams, but heavily modified over it. The key point
 *           in the initial implementation is a hashing algorithm.
 * Author:   Andrey Kiselev, dron@ak4719.spb.edu
 *
 ******************************************************************************
 * Copyright (c) 2007, Andrey Kiselev <dron@ak4719.spb.edu>
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
 * COPYRIGHT NOTICE FROM THE INITIAL IMPLEMENTATION:
 *
 * The programs LZWCOM and LZWUNC, both in binary executable and source forms,
 * are in the public domain.  No warranty is given or implied, and no
 * liability will be assumed by the author.
 *
 * Everyone on earth is hereby given permission to use, copy, distribute,
 * change, mangle, destroy or otherwise employ these programs, provided they
 * hurt no one but themselves in the process.
 *
 * Kent Williams
 * Norand Inc.
 * 550 2nd St S.E.
 * Cedar Rapids, Iowa 52401
 * (319) 369-3131
 ****************************************************************************/

#include "cpl_conv.h"

#include "rmfdataset.h"

// Code marks that there is no predecessor in the string
#define NO_PRED     0xFFFF

// We are using 12-bit codes in this particular implementation
#define TABSIZE     4096U
#define STACKSIZE   TABSIZE

/************************************************************************/
/*                           LZWStringTab                               */
/************************************************************************/

typedef struct
{
    int     bUsed;
    GUInt32 iNext;          // hi bit is 'used' flag
    GUInt32 iPredecessor;   // 12 bit code
    GByte   iFollower;
} LZWStringTab;

/************************************************************************/
/*                           LZWUpdateTab()                             */
/************************************************************************/

static void LZWUpdateTab(LZWStringTab *poCodeTab, GUInt32 iPred, char bFoll)
{
    GUInt32 nNext;

/* -------------------------------------------------------------------- */
/* Hash uses the 'mid-square' algorithm. I.E. for a hash val of n bits  */
/* hash = middle binary digits of (key * key).  Upon collision, hash    */
/* searches down linked list of keys that hashed to that key already.   */
/* It will NOT notice if the table is full. This must be handled        */
/* elsewhere                                                            */
/* -------------------------------------------------------------------- */
    GUInt32 nLocal = (iPred + bFoll) | 0x0800;
    nLocal = (nLocal*nLocal >> 6) & 0x0FFF;      // middle 12 bits of result

    // If string is not used
    if (poCodeTab[nLocal].bUsed == FALSE)
        nNext = nLocal;
    else
    {
        // If collision has occured
        while ( (nNext = poCodeTab[nLocal].iNext) != 0 )
            nLocal = nNext;

        // Search for free entry from nLocal + 101
        nNext = (nLocal + 101) & 0x0FFF;
        while ( poCodeTab[nNext].bUsed )
        {
            if ( ++nNext >= TABSIZE )
                nNext = 0;
        }

        // Put new tempnext into last element in collision list
        poCodeTab[nLocal].iNext = nNext;
    }

    poCodeTab[nNext].bUsed = TRUE;
    poCodeTab[nNext].iNext = 0;
    poCodeTab[nNext].iPredecessor = iPred;
    poCodeTab[nNext].iFollower = bFoll;
}

/************************************************************************/
/*                           LZWDecompress()                            */
/************************************************************************/

int RMFDataset::LZWDecompress( const GByte* pabyIn, GUInt32 nSizeIn,
                               GByte* pabyOut, GUInt32 nSizeOut )
{
    GUInt32         nCount = TABSIZE - 256;
    GUInt32         iCode, iOldCode, iInCode;
    GByte           iFinChar, bLastChar=FALSE;
    LZWStringTab    *poCodeTab;
    int             bBitsleft;

    if ( pabyIn == 0 ||
         pabyOut == 0 ||
         nSizeOut < nSizeIn ||
         nSizeIn < 2 )
        return 0;

    // Allocate space for the new table and pre-fill it
    poCodeTab = (LZWStringTab *)CPLMalloc( TABSIZE * sizeof(LZWStringTab) );
    if ( !poCodeTab )
        return 0;
    memset( poCodeTab, 0, TABSIZE * sizeof(LZWStringTab) );
    for ( iCode = 0; iCode < 256; iCode++ )
        LZWUpdateTab( poCodeTab, NO_PRED, (char)iCode );

    // The first code is always known
    iCode = (*pabyIn++ << 4) & 0xFF0; nSizeIn--;
    iCode += (*pabyIn >> 4) & 0x00F;
    iOldCode = iCode;
    bBitsleft = TRUE;

    *pabyOut++ = iFinChar = poCodeTab[iCode].iFollower; nSizeOut--;

    // Decompress the input buffer
    while ( nSizeIn > 0 )
    {
        int     bNewCode;
        GUInt32 nStackCount = 0;
        GByte   abyStack[STACKSIZE];
        GByte   *pabyTail = abyStack + STACKSIZE;

        // Fetch 12-bit code from input stream
        if ( bBitsleft )
        {
            iCode = ((*pabyIn++ & 0x0F) << 8) & 0xF00; nSizeIn--;
            if ( nSizeIn <= 0 )
                break;
            iCode += *pabyIn++; nSizeIn--;
            bBitsleft = FALSE;
        }
        else
        {
            iCode = (*pabyIn++ << 4) & 0xFF0; nSizeIn--;
            if ( nSizeIn <= 0 )
                break;
            iCode += (*pabyIn >> 4) & 0x00F;
            bBitsleft = TRUE;
        }
        iInCode = iCode;

        // Do we have unknown code?
        if ( poCodeTab[iCode].bUsed )
            bNewCode = FALSE;
        else
        {
            iCode = iOldCode;
            bLastChar = iFinChar;
            bNewCode = TRUE;
        }

        while ( poCodeTab[iCode].iPredecessor != NO_PRED )
        {
            // Stack overrun
            if ( nStackCount >= STACKSIZE )
                goto bad;
            // Put the decoded character into stack
            *(--pabyTail) = poCodeTab[iCode].iFollower; nStackCount++;
            iCode = poCodeTab[iCode].iPredecessor;
        }

        if ( !nSizeOut )
            goto bad;
        // The first character
        *pabyOut++ = iFinChar = poCodeTab[iCode].iFollower; nSizeOut--;

        // Output buffer overrun
        if ( nStackCount > nSizeOut )
            goto bad;

        // Now copy the stack contents into output buffer. Our stack was
        // filled in reverse order, so no need in character reordering
        memcpy( pabyOut, pabyTail, nStackCount );
        nSizeOut -= nStackCount;
        pabyOut += nStackCount;

        // If code isn't known
        if ( bNewCode )
        {
            // Output buffer overrun
            if ( !nSizeOut )
                goto bad;
            *pabyOut++ = iFinChar = bLastChar;  // the follower char of last
            nSizeOut--;
        }

        if ( nCount > 0 )
        {
            nCount--;
            // Add code to the table
            LZWUpdateTab( poCodeTab, iOldCode, iFinChar );
        }

        iOldCode = iInCode;
    }

    CPLFree( poCodeTab );
    return 1;

bad:
    CPLFree( poCodeTab );
    return 0;
}

