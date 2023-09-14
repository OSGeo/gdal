/******************************************************************************
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
constexpr uint32_t NO_PRED = 0xFFFF;

// We are using 12-bit codes in this particular implementation
constexpr uint32_t TABSIZE = 4096U;
constexpr uint32_t STACKSIZE = TABSIZE;

constexpr uint32_t NOT_FND = 0xFFFF;

/************************************************************************/
/*                           LZWStringTab                               */
/************************************************************************/

typedef struct
{
    bool bUsed;
    uint32_t iNext;         // hi bit is 'used' flag
    uint32_t iPredecessor;  // 12 bit code
    GByte iFollower;
} LZWStringTab;

/************************************************************************/
/*                           LZWUpdateTab()                             */
/************************************************************************/

CPL_NOSANITIZE_UNSIGNED_INT_OVERFLOW
static uint32_t UnsanitizedMul(uint32_t a, uint32_t b)
{
    return a * b;
}

static int UnsignedByteToSignedByte(GByte byVal)
{
    return byVal >= 128 ? byVal - 256 : byVal;
}

static void LZWUpdateTab(LZWStringTab *poCodeTab, uint32_t iPred, GByte bFollow)
{
    /* -------------------------------------------------------------------- */
    /* Hash uses the 'mid-square' algorithm. I.E. for a hash val of n bits  */
    /* hash = middle binary digits of (key * key).  Upon collision, hash    */
    /* searches down linked list of keys that hashed to that key already.   */
    /* It will NOT notice if the table is full. This must be handled        */
    /* elsewhere                                                            */
    /* -------------------------------------------------------------------- */
    const int iFollow = UnsignedByteToSignedByte(bFollow);
    uint32_t nLocal = CPLUnsanitizedAdd<uint32_t>(iPred, iFollow) | 0x0800;
    nLocal = (UnsanitizedMul(nLocal, nLocal) >> 6) &
             0x0FFF;  // middle 12 bits of result

    // If string is not used
    uint32_t nNext = nLocal;
    if (poCodeTab[nLocal].bUsed)
    {
        // If collision has occurred
        while ((nNext = poCodeTab[nLocal].iNext) != 0)
            nLocal = nNext;

        // Search for free entry from nLocal + 101
        nNext = (nLocal + 101) & 0x0FFF;
        while (poCodeTab[nNext].bUsed)
        {
            if (++nNext >= TABSIZE)
                nNext = 0;
        }

        // Put new tempnext into last element in collision list
        poCodeTab[nLocal].iNext = nNext;
    }

    poCodeTab[nNext].bUsed = true;
    poCodeTab[nNext].iNext = 0;
    poCodeTab[nNext].iPredecessor = iPred;
    poCodeTab[nNext].iFollower = bFollow;
}

/************************************************************************/
/*                           LZWCreateTab()                             */
/************************************************************************/

static LZWStringTab *LZWCreateTab()
{
    // Allocate space for the new table and pre-fill it
    LZWStringTab *poCodeTab =
        (LZWStringTab *)CPLMalloc(TABSIZE * sizeof(LZWStringTab));

    memset(poCodeTab, 0, TABSIZE * sizeof(LZWStringTab));

    for (uint32_t iCode = 0; iCode < 256; ++iCode)
        LZWUpdateTab(poCodeTab, NO_PRED, static_cast<GByte>(iCode));

    return poCodeTab;
}

/************************************************************************/
/*                            LZWFindIndex()                            */
/************************************************************************/

static uint32_t LZWFindIndex(const LZWStringTab *poCodeTab, uint32_t iPred,
                             GByte bFollow)
{
    const int iFollow = UnsignedByteToSignedByte(bFollow);
    uint32_t nLocal = CPLUnsanitizedAdd<uint32_t>(iPred, iFollow) | 0x0800;
    nLocal = (UnsanitizedMul(nLocal, nLocal) >> 6) &
             0x0FFF;  // middle 12 bits of result

    do
    {
        CPLAssert(nLocal < TABSIZE);
        if (poCodeTab[nLocal].iPredecessor == iPred &&
            poCodeTab[nLocal].iFollower == bFollow)
        {
            return nLocal;
        }
        nLocal = poCodeTab[nLocal].iNext;
    } while (nLocal > 0);

    return NOT_FND;
}

/************************************************************************/
/*                             LZWPutCode()                             */
/************************************************************************/

static bool LZWPutCode(uint32_t iCode, uint32_t &iTmp, bool &bBitsleft,
                       GByte *&pabyCurrent, const GByte *const pabyOutEnd)
{
    if (bBitsleft)
    {
        if (pabyCurrent >= pabyOutEnd)
        {
            return false;
        }
        *(pabyCurrent++) = static_cast<GByte>((iCode >> 4) & 0xFF);
        iTmp = iCode & 0x000F;
        bBitsleft = false;
    }
    else
    {
        if (pabyCurrent + 1 >= pabyOutEnd)
        {
            return false;
        }
        *(pabyCurrent++) =
            static_cast<GByte>(((iTmp << 4) & 0xFF0) + ((iCode >> 8) & 0x00F));
        *(pabyCurrent++) = static_cast<GByte>(iCode & 0xFF);
        bBitsleft = true;
    }
    return true;
}

/************************************************************************/
/*                           LZWReadStream()                            */
/************************************************************************/

static size_t LZWReadStream(const GByte *pabyIn, uint32_t nSizeIn,
                            GByte *pabyOut, uint32_t nSizeOut,
                            LZWStringTab *poCodeTab)
{
    GByte *const pabyOutBegin = pabyOut;

    // The first code is always known
    uint32_t iCode = (*pabyIn++ << 4) & 0xFF0;
    nSizeIn--;
    iCode += (*pabyIn >> 4) & 0x00F;
    uint32_t iOldCode = iCode;
    bool bBitsleft = true;

    GByte iFinChar = poCodeTab[iCode].iFollower;
    nSizeOut--;
    *pabyOut++ = iFinChar;

    uint32_t nCount = TABSIZE - 256;

    // Decompress the input buffer
    while (nSizeIn > 0)
    {
        // Fetch 12-bit code from input stream
        if (bBitsleft)
        {
            iCode = ((*pabyIn++ & 0x0F) << 8) & 0xF00;
            nSizeIn--;
            if (nSizeIn == 0)
                break;
            iCode += *pabyIn++;
            nSizeIn--;
            bBitsleft = FALSE;
        }
        else
        {
            iCode = (*pabyIn++ << 4) & 0xFF0;
            nSizeIn--;
            if (nSizeIn == 0)
                break;
            iCode += (*pabyIn >> 4) & 0x00F;
            bBitsleft = TRUE;
        }

        const uint32_t iInCode = iCode;
        GByte bLastChar = 0;  // TODO(schwehr): Why not nLastChar?

        // Do we have unknown code?
        bool bNewCode = false;
        if (!poCodeTab[iCode].bUsed)
        {
            iCode = iOldCode;
            bLastChar = iFinChar;
            bNewCode = true;
        }

        GByte abyStack[STACKSIZE] = {};
        GByte *pabyTail = abyStack + STACKSIZE;
        uint32_t nStackCount = 0;

        while (poCodeTab[iCode].iPredecessor != NO_PRED)
        {
            // Stack overrun
            if (nStackCount >= STACKSIZE)
                return 0;
            // Put the decoded character into stack
            *(--pabyTail) = poCodeTab[iCode].iFollower;
            nStackCount++;
            iCode = poCodeTab[iCode].iPredecessor;
        }

        if (!nSizeOut)
            return 0;
        // The first character
        iFinChar = poCodeTab[iCode].iFollower;
        nSizeOut--;
        *pabyOut++ = iFinChar;

        // Output buffer overrun
        if (nStackCount > nSizeOut)
            return 0;

        // Now copy the stack contents into output buffer. Our stack was
        // filled in reverse order, so no need in character reordering
        memcpy(pabyOut, pabyTail, nStackCount);
        nSizeOut -= nStackCount;
        pabyOut += nStackCount;

        // If code isn't known
        if (bNewCode)
        {
            // Output buffer overrun
            if (!nSizeOut)
                return 0;
            iFinChar = bLastChar;  // the follower char of last
            *pabyOut++ = iFinChar;
            nSizeOut--;
        }

        if (nCount > 0)
        {
            nCount--;
            // Add code to the table
            LZWUpdateTab(poCodeTab, iOldCode, iFinChar);
        }

        iOldCode = iInCode;
    }

    return static_cast<size_t>(pabyOut - pabyOutBegin);
}

/************************************************************************/
/*                           LZWDecompress()                            */
/************************************************************************/

size_t RMFDataset::LZWDecompress(const GByte *pabyIn, uint32_t nSizeIn,
                                 GByte *pabyOut, uint32_t nSizeOut, uint32_t,
                                 uint32_t)
{
    if (pabyIn == nullptr || pabyOut == nullptr || nSizeIn < 2)
        return 0;
    LZWStringTab *poCodeTab = LZWCreateTab();

    size_t nRet = LZWReadStream(pabyIn, nSizeIn, pabyOut, nSizeOut, poCodeTab);

    CPLFree(poCodeTab);

    return nRet;
}

/************************************************************************/
/*                             LZWWriteStream()                         */
/************************************************************************/

static size_t LZWWriteStream(const GByte *pabyIn, uint32_t nSizeIn,
                             GByte *pabyOut, uint32_t nSizeOut,
                             LZWStringTab *poCodeTab)
{
    uint32_t iCode;
    iCode = LZWFindIndex(poCodeTab, NO_PRED, *pabyIn++);
    nSizeIn--;

    uint32_t nCount = TABSIZE - 256;
    uint32_t iTmp = 0;
    bool bBitsleft = true;
    GByte *pabyCurrent = pabyOut;
    GByte *pabyOutEnd = pabyOut + nSizeOut;

    while (nSizeIn > 0)
    {
        const GByte bCurrentCode = *pabyIn++;
        nSizeIn--;

        uint32_t iNextCode = LZWFindIndex(poCodeTab, iCode, bCurrentCode);
        if (iNextCode != NOT_FND)
        {
            iCode = iNextCode;
            continue;
        }

        if (!LZWPutCode(iCode, iTmp, bBitsleft, pabyCurrent, pabyOutEnd))
        {
            return 0;
        }

        if (nCount > 0)
        {
            nCount--;
            LZWUpdateTab(poCodeTab, iCode, bCurrentCode);
        }

        iCode = LZWFindIndex(poCodeTab, NO_PRED, bCurrentCode);
    }

    if (!LZWPutCode(iCode, iTmp, bBitsleft, pabyCurrent, pabyOutEnd))
    {
        return 0;
    }

    if (!bBitsleft)
    {
        if (pabyCurrent >= pabyOutEnd)
        {
            return 0;
        }
        *(pabyCurrent++) = static_cast<GByte>((iTmp << 4) & 0xFF0);
    }

    return static_cast<size_t>(pabyCurrent - pabyOut);
}

/************************************************************************/
/*                             LZWCompress()                            */
/************************************************************************/

size_t RMFDataset::LZWCompress(const GByte *pabyIn, uint32_t nSizeIn,
                               GByte *pabyOut, uint32_t nSizeOut, uint32_t,
                               uint32_t, const RMFDataset *)
{
    if (pabyIn == nullptr || pabyOut == nullptr || nSizeIn == 0)
        return 0;

    // Allocate space for the new table and pre-fill it
    LZWStringTab *poCodeTab = LZWCreateTab();

    size_t nWritten =
        LZWWriteStream(pabyIn, nSizeIn, pabyOut, nSizeOut, poCodeTab);

    CPLFree(poCodeTab);

    return nWritten;
}
