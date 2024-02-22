/******************************************************************************
 *
 * Project:  GeoTIFF Driver
 * Purpose:  GDAL GeoTIFF support.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 1998, 2002, Frank Warmerdam <warmerdam@pobox.com>
 * Copyright (c) 2007-2015, Even Rouault <even dot rouault at spatialys dot com>
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

#include "gtiffoddbitsband.h"

#include "cpl_float.h"  // CPLFloatToHalf()
#include "gtiffdataset.h"
#include "tiffio.h"

/************************************************************************/
/*                           GTiffOddBitsBand()                         */
/************************************************************************/

GTiffOddBitsBand::GTiffOddBitsBand(GTiffDataset *m_poGDSIn, int nBandIn)
    : GTiffRasterBand(m_poGDSIn, nBandIn)

{
    eDataType = GDT_Unknown;
    if ((m_poGDS->m_nBitsPerSample == 16 || m_poGDS->m_nBitsPerSample == 24) &&
        m_poGDS->m_nSampleFormat == SAMPLEFORMAT_IEEEFP)
        eDataType = GDT_Float32;
    // FIXME ? in autotest we currently open gcore/data/int24.tif
    // which is declared as signed, but we consider it as unsigned
    else if ((m_poGDS->m_nSampleFormat == SAMPLEFORMAT_UINT ||
              m_poGDS->m_nSampleFormat == SAMPLEFORMAT_INT) &&
             m_poGDS->m_nBitsPerSample < 8)
        eDataType = GDT_Byte;
    else if ((m_poGDS->m_nSampleFormat == SAMPLEFORMAT_UINT ||
              m_poGDS->m_nSampleFormat == SAMPLEFORMAT_INT) &&
             m_poGDS->m_nBitsPerSample > 8 && m_poGDS->m_nBitsPerSample < 16)
        eDataType = GDT_UInt16;
    else if ((m_poGDS->m_nSampleFormat == SAMPLEFORMAT_UINT ||
              m_poGDS->m_nSampleFormat == SAMPLEFORMAT_INT) &&
             m_poGDS->m_nBitsPerSample > 16 && m_poGDS->m_nBitsPerSample < 32)
        eDataType = GDT_UInt32;
}

/************************************************************************/
/*                            IWriteBlock()                             */
/************************************************************************/

CPLErr GTiffOddBitsBand::IWriteBlock(int nBlockXOff, int nBlockYOff,
                                     void *pImage)

{
    m_poGDS->Crystalize();

    if (m_poGDS->m_bWriteError)
    {
        // Report as an error if a previously loaded block couldn't be written
        // correctly.
        return CE_Failure;
    }

    if (eDataType == GDT_Float32 && m_poGDS->m_nBitsPerSample != 16)
    {
        ReportError(
            CE_Failure, CPLE_NotSupported,
            "Writing float data with nBitsPerSample = %d is unsupported",
            m_poGDS->m_nBitsPerSample);
        return CE_Failure;
    }

    /* -------------------------------------------------------------------- */
    /*      Load the block buffer.                                          */
    /* -------------------------------------------------------------------- */
    const int nBlockId = ComputeBlockId(nBlockXOff, nBlockYOff);

    // Only read content from disk in the CONTIG case.
    {
        const CPLErr eErr = m_poGDS->LoadBlockBuf(
            nBlockId, m_poGDS->m_nPlanarConfig == PLANARCONFIG_CONTIG &&
                          m_poGDS->nBands > 1);
        if (eErr != CE_None)
            return eErr;
    }

    const GUInt32 nMaxVal = (1U << m_poGDS->m_nBitsPerSample) - 1;

    /* -------------------------------------------------------------------- */
    /*      Handle case of "separate" images or single band images where    */
    /*      no interleaving with other data is required.                    */
    /* -------------------------------------------------------------------- */
    if (m_poGDS->m_nPlanarConfig == PLANARCONFIG_SEPARATE ||
        m_poGDS->nBands == 1)
    {
        // TODO(schwehr): Create a CplNumBits8Aligned.
        // Bits per line rounds up to next byte boundary.
        GInt64 nBitsPerLine =
            static_cast<GInt64>(nBlockXSize) * m_poGDS->m_nBitsPerSample;
        if ((nBitsPerLine & 7) != 0)
            nBitsPerLine = (nBitsPerLine + 7) & (~7);

        GPtrDiff_t iPixel = 0;

        // Small optimization in 1 bit case.
        if (m_poGDS->m_nBitsPerSample == 1)
        {
            for (int iY = 0; iY < nBlockYSize; ++iY, iPixel += nBlockXSize)
            {
                GInt64 iBitOffset = iY * nBitsPerLine;

                const GByte *pabySrc =
                    static_cast<const GByte *>(pImage) + iPixel;
                auto iByteOffset = iBitOffset / 8;
                int iX = 0;  // Used after for.
                for (; iX + 7 < nBlockXSize; iX += 8, iByteOffset++)
                {
                    int nRes = (!(!pabySrc[iX + 0])) << 7;
                    nRes |= (!(!pabySrc[iX + 1])) << 6;
                    nRes |= (!(!pabySrc[iX + 2])) << 5;
                    nRes |= (!(!pabySrc[iX + 3])) << 4;
                    nRes |= (!(!pabySrc[iX + 4])) << 3;
                    nRes |= (!(!pabySrc[iX + 5])) << 2;
                    nRes |= (!(!pabySrc[iX + 6])) << 1;
                    nRes |= (!(!pabySrc[iX + 7])) << 0;
                    m_poGDS->m_pabyBlockBuf[iByteOffset] =
                        static_cast<GByte>(nRes);
                }
                iBitOffset = iByteOffset * 8;
                if (iX < nBlockXSize)
                {
                    int nRes = 0;
                    for (; iX < nBlockXSize; ++iX)
                    {
                        if (pabySrc[iX])
                            nRes |= (0x80 >> (iBitOffset & 7));
                        ++iBitOffset;
                    }
                    m_poGDS->m_pabyBlockBuf[iBitOffset >> 3] =
                        static_cast<GByte>(nRes);
                }
            }

            m_poGDS->m_bLoadedBlockDirty = true;

            return CE_None;
        }

        if (eDataType == GDT_Float32 && m_poGDS->m_nBitsPerSample == 16)
        {
            for (; iPixel < static_cast<GPtrDiff_t>(nBlockYSize) * nBlockXSize;
                 iPixel++)
            {
                GUInt32 nInWord = static_cast<GUInt32 *>(pImage)[iPixel];
                bool bClipWarn = m_poGDS->m_bClipWarn;
                GUInt16 nHalf = CPLFloatToHalf(nInWord, bClipWarn);
                m_poGDS->m_bClipWarn = bClipWarn;
                reinterpret_cast<GUInt16 *>(m_poGDS->m_pabyBlockBuf)[iPixel] =
                    nHalf;
            }

            m_poGDS->m_bLoadedBlockDirty = true;

            return CE_None;
        }

        // Initialize to zero as we set the buffer with binary or operations.
        if (m_poGDS->m_nBitsPerSample != 24)
            memset(m_poGDS->m_pabyBlockBuf, 0,
                   static_cast<size_t>((nBitsPerLine / 8) * nBlockYSize));

        for (int iY = 0; iY < nBlockYSize; ++iY)
        {
            GInt64 iBitOffset = iY * nBitsPerLine;

            if (m_poGDS->m_nBitsPerSample == 12)
            {
                for (int iX = 0; iX < nBlockXSize; ++iX)
                {
                    GUInt32 nInWord = static_cast<GUInt16 *>(pImage)[iPixel++];
                    if (nInWord > nMaxVal)
                    {
                        nInWord = nMaxVal;
                        if (!m_poGDS->m_bClipWarn)
                        {
                            m_poGDS->m_bClipWarn = true;
                            ReportError(
                                CE_Warning, CPLE_AppDefined,
                                "One or more pixels clipped to fit %d bit "
                                "domain.",
                                m_poGDS->m_nBitsPerSample);
                        }
                    }

                    if ((iBitOffset % 8) == 0)
                    {
                        m_poGDS->m_pabyBlockBuf[iBitOffset >> 3] =
                            static_cast<GByte>(nInWord >> 4);
                        // Let 4 lower bits to zero as they're going to be
                        // overridden by the next word.
                        m_poGDS->m_pabyBlockBuf[(iBitOffset >> 3) + 1] =
                            static_cast<GByte>((nInWord & 0xf) << 4);
                    }
                    else
                    {
                        // Must or to preserve the 4 upper bits written
                        // for the previous word.
                        m_poGDS->m_pabyBlockBuf[iBitOffset >> 3] |=
                            static_cast<GByte>(nInWord >> 8);
                        m_poGDS->m_pabyBlockBuf[(iBitOffset >> 3) + 1] =
                            static_cast<GByte>(nInWord & 0xff);
                    }

                    iBitOffset += m_poGDS->m_nBitsPerSample;
                }
                continue;
            }

            for (int iX = 0; iX < nBlockXSize; ++iX)
            {
                GUInt32 nInWord = 0;
                if (eDataType == GDT_Byte)
                {
                    nInWord = static_cast<GByte *>(pImage)[iPixel++];
                }
                else if (eDataType == GDT_UInt16)
                {
                    nInWord = static_cast<GUInt16 *>(pImage)[iPixel++];
                }
                else if (eDataType == GDT_UInt32)
                {
                    nInWord = static_cast<GUInt32 *>(pImage)[iPixel++];
                }
                else
                {
                    CPLAssert(false);
                }

                if (nInWord > nMaxVal)
                {
                    nInWord = nMaxVal;
                    if (!m_poGDS->m_bClipWarn)
                    {
                        m_poGDS->m_bClipWarn = true;
                        ReportError(
                            CE_Warning, CPLE_AppDefined,
                            "One or more pixels clipped to fit %d bit domain.",
                            m_poGDS->m_nBitsPerSample);
                    }
                }

                if (m_poGDS->m_nBitsPerSample == 24)
                {
/* -------------------------------------------------------------------- */
/*      Special case for 24bit data which is pre-byteswapped since      */
/*      the size falls on a byte boundary ... ugh (#2361).              */
/* -------------------------------------------------------------------- */
#ifdef CPL_MSB
                    m_poGDS->m_pabyBlockBuf[(iBitOffset >> 3) + 0] =
                        static_cast<GByte>(nInWord);
                    m_poGDS->m_pabyBlockBuf[(iBitOffset >> 3) + 1] =
                        static_cast<GByte>(nInWord >> 8);
                    m_poGDS->m_pabyBlockBuf[(iBitOffset >> 3) + 2] =
                        static_cast<GByte>(nInWord >> 16);
#else
                    m_poGDS->m_pabyBlockBuf[(iBitOffset >> 3) + 0] =
                        static_cast<GByte>(nInWord >> 16);
                    m_poGDS->m_pabyBlockBuf[(iBitOffset >> 3) + 1] =
                        static_cast<GByte>(nInWord >> 8);
                    m_poGDS->m_pabyBlockBuf[(iBitOffset >> 3) + 2] =
                        static_cast<GByte>(nInWord);
#endif
                    iBitOffset += 24;
                }
                else
                {
                    for (int iBit = 0; iBit < m_poGDS->m_nBitsPerSample; ++iBit)
                    {
                        if (nInWord &
                            (1 << (m_poGDS->m_nBitsPerSample - 1 - iBit)))
                            m_poGDS->m_pabyBlockBuf[iBitOffset >> 3] |=
                                (0x80 >> (iBitOffset & 7));
                        ++iBitOffset;
                    }
                }
            }
        }

        m_poGDS->m_bLoadedBlockDirty = true;

        return CE_None;
    }

    /* -------------------------------------------------------------------- */
    /*      Handle case of pixel interleaved (PLANARCONFIG_CONTIG) images.  */
    /* -------------------------------------------------------------------- */

    /* -------------------------------------------------------------------- */
    /*      On write of pixel interleaved data, we might as well flush      */
    /*      out any other bands that are dirty in our cache.  This is       */
    /*      especially helpful when writing compressed blocks.              */
    /* -------------------------------------------------------------------- */
    for (int iBand = 0; iBand < m_poGDS->nBands; ++iBand)
    {
        const GByte *pabyThisImage = nullptr;
        GDALRasterBlock *poBlock = nullptr;

        if (iBand + 1 == nBand)
        {
            pabyThisImage = static_cast<GByte *>(pImage);
        }
        else
        {
            poBlock = cpl::down_cast<GTiffOddBitsBand *>(
                          m_poGDS->GetRasterBand(iBand + 1))
                          ->TryGetLockedBlockRef(nBlockXOff, nBlockYOff);

            if (poBlock == nullptr)
                continue;

            if (!poBlock->GetDirty())
            {
                poBlock->DropLock();
                continue;
            }

            pabyThisImage = static_cast<GByte *>(poBlock->GetDataRef());
        }

        const int iPixelBitSkip = m_poGDS->m_nBitsPerSample * m_poGDS->nBands;
        const int iBandBitOffset = iBand * m_poGDS->m_nBitsPerSample;

        // Bits per line rounds up to next byte boundary.
        GInt64 nBitsPerLine = static_cast<GInt64>(nBlockXSize) * iPixelBitSkip;
        if ((nBitsPerLine & 7) != 0)
            nBitsPerLine = (nBitsPerLine + 7) & (~7);

        GPtrDiff_t iPixel = 0;

        if (eDataType == GDT_Float32 && m_poGDS->m_nBitsPerSample == 16)
        {
            for (; iPixel < static_cast<GPtrDiff_t>(nBlockYSize) * nBlockXSize;
                 iPixel++)
            {
                GUInt32 nInWord =
                    reinterpret_cast<const GUInt32 *>(pabyThisImage)[iPixel];
                bool bClipWarn = m_poGDS->m_bClipWarn;
                GUInt16 nHalf = CPLFloatToHalf(nInWord, bClipWarn);
                m_poGDS->m_bClipWarn = bClipWarn;
                reinterpret_cast<GUInt16 *>(
                    m_poGDS->m_pabyBlockBuf)[iPixel * m_poGDS->nBands + iBand] =
                    nHalf;
            }

            if (poBlock != nullptr)
            {
                poBlock->MarkClean();
                poBlock->DropLock();
            }
            continue;
        }

        for (int iY = 0; iY < nBlockYSize; ++iY)
        {
            GInt64 iBitOffset = iBandBitOffset + iY * nBitsPerLine;

            if (m_poGDS->m_nBitsPerSample == 12)
            {
                for (int iX = 0; iX < nBlockXSize; ++iX)
                {
                    GUInt32 nInWord = reinterpret_cast<const GUInt16 *>(
                        pabyThisImage)[iPixel++];
                    if (nInWord > nMaxVal)
                    {
                        nInWord = nMaxVal;
                        if (!m_poGDS->m_bClipWarn)
                        {
                            m_poGDS->m_bClipWarn = true;
                            ReportError(
                                CE_Warning, CPLE_AppDefined,
                                "One or more pixels clipped to fit %d bit "
                                "domain.",
                                m_poGDS->m_nBitsPerSample);
                        }
                    }

                    if ((iBitOffset % 8) == 0)
                    {
                        m_poGDS->m_pabyBlockBuf[iBitOffset >> 3] =
                            static_cast<GByte>(nInWord >> 4);
                        m_poGDS->m_pabyBlockBuf[(iBitOffset >> 3) + 1] =
                            static_cast<GByte>(
                                ((nInWord & 0xf) << 4) |
                                (m_poGDS
                                     ->m_pabyBlockBuf[(iBitOffset >> 3) + 1] &
                                 0xf));
                    }
                    else
                    {
                        m_poGDS->m_pabyBlockBuf[iBitOffset >> 3] =
                            static_cast<GByte>(
                                (m_poGDS->m_pabyBlockBuf[iBitOffset >> 3] &
                                 0xf0) |
                                (nInWord >> 8));
                        m_poGDS->m_pabyBlockBuf[(iBitOffset >> 3) + 1] =
                            static_cast<GByte>(nInWord & 0xff);
                    }

                    iBitOffset += iPixelBitSkip;
                }
                continue;
            }

            for (int iX = 0; iX < nBlockXSize; ++iX)
            {
                GUInt32 nInWord = 0;
                if (eDataType == GDT_Byte)
                {
                    nInWord =
                        static_cast<const GByte *>(pabyThisImage)[iPixel++];
                }
                else if (eDataType == GDT_UInt16)
                {
                    nInWord = reinterpret_cast<const GUInt16 *>(
                        pabyThisImage)[iPixel++];
                }
                else if (eDataType == GDT_UInt32)
                {
                    nInWord = reinterpret_cast<const GUInt32 *>(
                        pabyThisImage)[iPixel++];
                }
                else
                {
                    CPLAssert(false);
                }

                if (nInWord > nMaxVal)
                {
                    nInWord = nMaxVal;
                    if (!m_poGDS->m_bClipWarn)
                    {
                        m_poGDS->m_bClipWarn = true;
                        ReportError(
                            CE_Warning, CPLE_AppDefined,
                            "One or more pixels clipped to fit %d bit domain.",
                            m_poGDS->m_nBitsPerSample);
                    }
                }

                if (m_poGDS->m_nBitsPerSample == 24)
                {
/* -------------------------------------------------------------------- */
/*      Special case for 24bit data which is pre-byteswapped since      */
/*      the size falls on a byte boundary ... ugh (#2361).              */
/* -------------------------------------------------------------------- */
#ifdef CPL_MSB
                    m_poGDS->m_pabyBlockBuf[(iBitOffset >> 3) + 0] =
                        static_cast<GByte>(nInWord);
                    m_poGDS->m_pabyBlockBuf[(iBitOffset >> 3) + 1] =
                        static_cast<GByte>(nInWord >> 8);
                    m_poGDS->m_pabyBlockBuf[(iBitOffset >> 3) + 2] =
                        static_cast<GByte>(nInWord >> 16);
#else
                    m_poGDS->m_pabyBlockBuf[(iBitOffset >> 3) + 0] =
                        static_cast<GByte>(nInWord >> 16);
                    m_poGDS->m_pabyBlockBuf[(iBitOffset >> 3) + 1] =
                        static_cast<GByte>(nInWord >> 8);
                    m_poGDS->m_pabyBlockBuf[(iBitOffset >> 3) + 2] =
                        static_cast<GByte>(nInWord);
#endif
                    iBitOffset += 24;
                }
                else
                {
                    for (int iBit = 0; iBit < m_poGDS->m_nBitsPerSample; ++iBit)
                    {
                        // TODO(schwehr): Revisit this block.
                        if (nInWord &
                            (1 << (m_poGDS->m_nBitsPerSample - 1 - iBit)))
                        {
                            m_poGDS->m_pabyBlockBuf[iBitOffset >> 3] |=
                                (0x80 >> (iBitOffset & 7));
                        }
                        else
                        {
                            // We must explicitly unset the bit as we
                            // may update an existing block.
                            m_poGDS->m_pabyBlockBuf[iBitOffset >> 3] &=
                                ~(0x80 >> (iBitOffset & 7));
                        }

                        ++iBitOffset;
                    }
                }

                iBitOffset =
                    iBitOffset + iPixelBitSkip - m_poGDS->m_nBitsPerSample;
            }
        }

        if (poBlock != nullptr)
        {
            poBlock->MarkClean();
            poBlock->DropLock();
        }
    }

    m_poGDS->m_bLoadedBlockDirty = true;

    return CE_None;
}

/************************************************************************/
/*                             IReadBlock()                             */
/************************************************************************/

static void ExpandPacked8ToByte1(const GByte *const CPL_RESTRICT pabySrc,
                                 GByte *const CPL_RESTRICT pabyDest,
                                 GPtrDiff_t nBytes)
{
    for (decltype(nBytes) i = 0, j = 0; i < nBytes; i++, j += 8)
    {
        const GByte byVal = pabySrc[i];
        pabyDest[j + 0] = (byVal >> 7) & 0x1;
        pabyDest[j + 1] = (byVal >> 6) & 0x1;
        pabyDest[j + 2] = (byVal >> 5) & 0x1;
        pabyDest[j + 3] = (byVal >> 4) & 0x1;
        pabyDest[j + 4] = (byVal >> 3) & 0x1;
        pabyDest[j + 5] = (byVal >> 2) & 0x1;
        pabyDest[j + 6] = (byVal >> 1) & 0x1;
        pabyDest[j + 7] = (byVal >> 0) & 0x1;
    }
}

#if defined(__GNUC__) || defined(_MSC_VER)
// Signedness of char implementation dependent, so be explicit.
// Assumes 2-complement integer types and sign extension of right shifting
// GCC guarantees such:
// https://gcc.gnu.org/onlinedocs/gcc/Integers-implementation.html#Integers-implementation
static inline GByte ExtractBitAndConvertTo255(GByte byVal, int nBit)
{
    return static_cast<GByte>(static_cast<signed char>(byVal << (7 - nBit)) >>
                              7);
}
#else
// Portable way
static inline GByte ExtractBitAndConvertTo255(GByte byVal, int nBit)
{
    return (byVal & (1 << nBit)) ? 255 : 0;
}
#endif

static void ExpandPacked8ToByte255(const GByte *const CPL_RESTRICT pabySrc,
                                   GByte *const CPL_RESTRICT pabyDest,
                                   GPtrDiff_t nBytes)
{
    for (decltype(nBytes) i = 0, j = 0; i < nBytes; i++, j += 8)
    {
        const GByte byVal = pabySrc[i];
        pabyDest[j + 0] = ExtractBitAndConvertTo255(byVal, 7);
        pabyDest[j + 1] = ExtractBitAndConvertTo255(byVal, 6);
        pabyDest[j + 2] = ExtractBitAndConvertTo255(byVal, 5);
        pabyDest[j + 3] = ExtractBitAndConvertTo255(byVal, 4);
        pabyDest[j + 4] = ExtractBitAndConvertTo255(byVal, 3);
        pabyDest[j + 5] = ExtractBitAndConvertTo255(byVal, 2);
        pabyDest[j + 6] = ExtractBitAndConvertTo255(byVal, 1);
        pabyDest[j + 7] = ExtractBitAndConvertTo255(byVal, 0);
    }
}

CPLErr GTiffOddBitsBand::IReadBlock(int nBlockXOff, int nBlockYOff,
                                    void *pImage)

{
    m_poGDS->Crystalize();

    const int nBlockId = ComputeBlockId(nBlockXOff, nBlockYOff);

    /* -------------------------------------------------------------------- */
    /*      Handle the case of a strip in a writable file that doesn't      */
    /*      exist yet, but that we want to read.  Just set to zeros and     */
    /*      return.                                                         */
    /* -------------------------------------------------------------------- */
    if (nBlockId != m_poGDS->m_nLoadedBlock)
    {
        bool bErrOccurred = false;
        if (!m_poGDS->IsBlockAvailable(nBlockId, nullptr, nullptr,
                                       &bErrOccurred))
        {
            NullBlock(pImage);
            if (bErrOccurred)
                return CE_Failure;
            return CE_None;
        }
    }

    /* -------------------------------------------------------------------- */
    /*      Load the block buffer.                                          */
    /* -------------------------------------------------------------------- */
    {
        const CPLErr eErr = m_poGDS->LoadBlockBuf(nBlockId);
        if (eErr != CE_None)
            return eErr;
    }

    if (m_poGDS->m_nBitsPerSample == 1 &&
        (m_poGDS->nBands == 1 ||
         m_poGDS->m_nPlanarConfig == PLANARCONFIG_SEPARATE))
    {
        /* --------------------------------------------------------------------
         */
        /*      Translate 1bit data to eight bit. */
        /* --------------------------------------------------------------------
         */
        GPtrDiff_t iDstOffset = 0;
        const GByte *const CPL_RESTRICT m_pabyBlockBuf =
            m_poGDS->m_pabyBlockBuf;
        GByte *CPL_RESTRICT pabyDest = static_cast<GByte *>(pImage);

        for (int iLine = 0; iLine < nBlockYSize; ++iLine)
        {
            GPtrDiff_t iSrcOffsetByte =
                static_cast<GPtrDiff_t>((nBlockXSize + 7) >> 3) * iLine;

            if (!m_poGDS->m_bPromoteTo8Bits)
            {
                ExpandPacked8ToByte1(m_pabyBlockBuf + iSrcOffsetByte,
                                     pabyDest + iDstOffset, nBlockXSize / 8);
            }
            else
            {
                ExpandPacked8ToByte255(m_pabyBlockBuf + iSrcOffsetByte,
                                       pabyDest + iDstOffset, nBlockXSize / 8);
            }
            GPtrDiff_t iSrcOffsetBit = (iSrcOffsetByte + nBlockXSize / 8) * 8;
            iDstOffset += nBlockXSize & ~0x7;
            const GByte bSetVal = m_poGDS->m_bPromoteTo8Bits ? 255 : 1;
            for (int iPixel = nBlockXSize & ~0x7; iPixel < nBlockXSize;
                 ++iPixel, ++iSrcOffsetBit)
            {
                if (m_pabyBlockBuf[iSrcOffsetBit >> 3] &
                    (0x80 >> (iSrcOffsetBit & 0x7)))
                    static_cast<GByte *>(pImage)[iDstOffset++] = bSetVal;
                else
                    static_cast<GByte *>(pImage)[iDstOffset++] = 0;
            }
        }
    }
    /* -------------------------------------------------------------------- */
    /*      Handle the case of 16- and 24-bit floating point data as per    */
    /*      TIFF Technical Note 3.                                          */
    /* -------------------------------------------------------------------- */
    else if (eDataType == GDT_Float32)
    {
        const int nWordBytes = m_poGDS->m_nBitsPerSample / 8;
        const GByte *pabyImage =
            m_poGDS->m_pabyBlockBuf +
            ((m_poGDS->m_nPlanarConfig == PLANARCONFIG_SEPARATE)
                 ? 0
                 : (nBand - 1) * nWordBytes);
        const int iSkipBytes =
            (m_poGDS->m_nPlanarConfig == PLANARCONFIG_SEPARATE)
                ? nWordBytes
                : m_poGDS->nBands * nWordBytes;

        const auto nBlockPixels =
            static_cast<GPtrDiff_t>(nBlockXSize) * nBlockYSize;
        if (m_poGDS->m_nBitsPerSample == 16)
        {
            for (GPtrDiff_t i = 0; i < nBlockPixels; ++i)
            {
                static_cast<GUInt32 *>(pImage)[i] = CPLHalfToFloat(
                    *reinterpret_cast<const GUInt16 *>(pabyImage));
                pabyImage += iSkipBytes;
            }
        }
        else if (m_poGDS->m_nBitsPerSample == 24)
        {
            for (GPtrDiff_t i = 0; i < nBlockPixels; ++i)
            {
#ifdef CPL_MSB
                static_cast<GUInt32 *>(pImage)[i] = CPLTripleToFloat(
                    (static_cast<GUInt32>(*(pabyImage + 0)) << 16) |
                    (static_cast<GUInt32>(*(pabyImage + 1)) << 8) |
                    static_cast<GUInt32>(*(pabyImage + 2)));
#else
                static_cast<GUInt32 *>(pImage)[i] = CPLTripleToFloat(
                    (static_cast<GUInt32>(*(pabyImage + 2)) << 16) |
                    (static_cast<GUInt32>(*(pabyImage + 1)) << 8) |
                    static_cast<GUInt32>(*pabyImage));
#endif
                pabyImage += iSkipBytes;
            }
        }
    }

    /* -------------------------------------------------------------------- */
    /*      Special case for moving 12bit data somewhat more efficiently.   */
    /* -------------------------------------------------------------------- */
    else if (m_poGDS->m_nBitsPerSample == 12)
    {
        int iPixelBitSkip = 0;
        int iBandBitOffset = 0;

        if (m_poGDS->m_nPlanarConfig == PLANARCONFIG_CONTIG)
        {
            iPixelBitSkip = m_poGDS->nBands * m_poGDS->m_nBitsPerSample;
            iBandBitOffset = (nBand - 1) * m_poGDS->m_nBitsPerSample;
        }
        else
        {
            iPixelBitSkip = m_poGDS->m_nBitsPerSample;
        }

        // Bits per line rounds up to next byte boundary.
        GPtrDiff_t nBitsPerLine =
            static_cast<GPtrDiff_t>(nBlockXSize) * iPixelBitSkip;
        if ((nBitsPerLine & 7) != 0)
            nBitsPerLine = (nBitsPerLine + 7) & (~7);

        int iPixel = 0;
        for (int iY = 0; iY < nBlockYSize; ++iY)
        {
            GPtrDiff_t iBitOffset = iBandBitOffset + iY * nBitsPerLine;

            for (int iX = 0; iX < nBlockXSize; ++iX)
            {
                const auto iByte = iBitOffset >> 3;

                if ((iBitOffset & 0x7) == 0)
                {
                    // Starting on byte boundary.

                    static_cast<GUInt16 *>(pImage)[iPixel++] =
                        (m_poGDS->m_pabyBlockBuf[iByte] << 4) |
                        (m_poGDS->m_pabyBlockBuf[iByte + 1] >> 4);
                }
                else
                {
                    // Starting off byte boundary.

                    static_cast<GUInt16 *>(pImage)[iPixel++] =
                        ((m_poGDS->m_pabyBlockBuf[iByte] & 0xf) << 8) |
                        (m_poGDS->m_pabyBlockBuf[iByte + 1]);
                }
                iBitOffset += iPixelBitSkip;
            }
        }
    }

    /* -------------------------------------------------------------------- */
    /*      Special case for 24bit data which is pre-byteswapped since      */
    /*      the size falls on a byte boundary ... ugh (#2361).              */
    /* -------------------------------------------------------------------- */
    else if (m_poGDS->m_nBitsPerSample == 24)
    {
        int iPixelByteSkip = 0;
        int iBandByteOffset = 0;

        if (m_poGDS->m_nPlanarConfig == PLANARCONFIG_CONTIG)
        {
            iPixelByteSkip = (m_poGDS->nBands * m_poGDS->m_nBitsPerSample) / 8;
            iBandByteOffset = ((nBand - 1) * m_poGDS->m_nBitsPerSample) / 8;
        }
        else
        {
            iPixelByteSkip = m_poGDS->m_nBitsPerSample / 8;
        }

        const GPtrDiff_t nBytesPerLine =
            static_cast<GPtrDiff_t>(nBlockXSize) * iPixelByteSkip;

        GPtrDiff_t iPixel = 0;
        for (int iY = 0; iY < nBlockYSize; ++iY)
        {
            GByte *pabyImage =
                m_poGDS->m_pabyBlockBuf + iBandByteOffset + iY * nBytesPerLine;

            for (int iX = 0; iX < nBlockXSize; ++iX)
            {
#ifdef CPL_MSB
                static_cast<GUInt32 *>(pImage)[iPixel++] =
                    (static_cast<GUInt32>(*(pabyImage + 2)) << 16) |
                    (static_cast<GUInt32>(*(pabyImage + 1)) << 8) |
                    static_cast<GUInt32>(*(pabyImage + 0));
#else
                static_cast<GUInt32 *>(pImage)[iPixel++] =
                    (static_cast<GUInt32>(*(pabyImage + 0)) << 16) |
                    (static_cast<GUInt32>(*(pabyImage + 1)) << 8) |
                    static_cast<GUInt32>(*(pabyImage + 2));
#endif
                pabyImage += iPixelByteSkip;
            }
        }
    }

    /* -------------------------------------------------------------------- */
    /*      Handle 1-32 bit integer data.                                   */
    /* -------------------------------------------------------------------- */
    else
    {
        unsigned iPixelBitSkip = 0;
        unsigned iBandBitOffset = 0;

        if (m_poGDS->m_nPlanarConfig == PLANARCONFIG_CONTIG)
        {
            iPixelBitSkip = m_poGDS->nBands * m_poGDS->m_nBitsPerSample;
            iBandBitOffset = (nBand - 1) * m_poGDS->m_nBitsPerSample;
        }
        else
        {
            iPixelBitSkip = m_poGDS->m_nBitsPerSample;
        }

        // Bits per line rounds up to next byte boundary.
        GUIntBig nBitsPerLine =
            static_cast<GUIntBig>(nBlockXSize) * iPixelBitSkip;
        if ((nBitsPerLine & 7) != 0)
            nBitsPerLine = (nBitsPerLine + 7) & (~7);

        const GByte *const m_pabyBlockBuf = m_poGDS->m_pabyBlockBuf;
        const unsigned nBitsPerSample = m_poGDS->m_nBitsPerSample;
        GPtrDiff_t iPixel = 0;

        if (nBitsPerSample == 1 && eDataType == GDT_Byte)
        {
            for (unsigned iY = 0; iY < static_cast<unsigned>(nBlockYSize); ++iY)
            {
                GUIntBig iBitOffset = iBandBitOffset + iY * nBitsPerLine;

                for (unsigned iX = 0; iX < static_cast<unsigned>(nBlockXSize);
                     ++iX)
                {
                    if (m_pabyBlockBuf[iBitOffset >> 3] &
                        (0x80 >> (iBitOffset & 7)))
                        static_cast<GByte *>(pImage)[iPixel] = 1;
                    else
                        static_cast<GByte *>(pImage)[iPixel] = 0;
                    iBitOffset += iPixelBitSkip;
                    iPixel++;
                }
            }
        }
        else
        {
            for (unsigned iY = 0; iY < static_cast<unsigned>(nBlockYSize); ++iY)
            {
                GUIntBig iBitOffset = iBandBitOffset + iY * nBitsPerLine;

                for (unsigned iX = 0; iX < static_cast<unsigned>(nBlockXSize);
                     ++iX)
                {
                    unsigned nOutWord = 0;

                    for (unsigned iBit = 0; iBit < nBitsPerSample; ++iBit)
                    {
                        if (m_pabyBlockBuf[iBitOffset >> 3] &
                            (0x80 >> (iBitOffset & 7)))
                            nOutWord |= (1 << (nBitsPerSample - 1 - iBit));
                        ++iBitOffset;
                    }

                    iBitOffset = iBitOffset + iPixelBitSkip - nBitsPerSample;

                    if (eDataType == GDT_Byte)
                    {
                        static_cast<GByte *>(pImage)[iPixel++] =
                            static_cast<GByte>(nOutWord);
                    }
                    else if (eDataType == GDT_UInt16)
                    {
                        static_cast<GUInt16 *>(pImage)[iPixel++] =
                            static_cast<GUInt16>(nOutWord);
                    }
                    else if (eDataType == GDT_UInt32)
                    {
                        static_cast<GUInt32 *>(pImage)[iPixel++] = nOutWord;
                    }
                    else
                    {
                        CPLAssert(false);
                    }
                }
            }
        }
    }

    CacheMaskForBlock(nBlockXOff, nBlockYOff);

    return CE_None;
}
