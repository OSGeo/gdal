/******************************************************************************
 *
 * Project:  GDAL
 * Purpose:  Zarr driver, "vlen-utf8" codec
 * Author:   Wietze Suijker
 *
 ******************************************************************************
 * Copyright (c) 2025, Wietze Suijker
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include "zarr_v3_codec.h"

#include <algorithm>
#include <cinttypes>
#include <cstring>

// Implements the vlen-utf8 codec from zarr-extensions:
// https://github.com/zarr-developers/zarr-extensions/tree/main/codecs/vlen-utf8
//
// Binary format (all integers little-endian u32):
//   [item_count] [len_0][data_0] [len_1][data_1] ...
//
// Decode produces a flat buffer of nItems * nSlotSize bytes where
// nSlotSize = m_oInputArrayMetadata.oElt.nativeSize (set from
// ZARR_VLEN_STRING_MAX_LENGTH, default 256).  Strings exceeding
// (nSlotSize - 1) bytes are truncated.

/************************************************************************/
/*                       ZarrV3CodecVLenUTF8()                          */
/************************************************************************/

ZarrV3CodecVLenUTF8::ZarrV3CodecVLenUTF8() : ZarrV3Codec(NAME)
{
}

/************************************************************************/
/*            ZarrV3CodecVLenUTF8::InitFromConfiguration()              */
/************************************************************************/

bool ZarrV3CodecVLenUTF8::InitFromConfiguration(
    const CPLJSONObject &configuration,
    const ZarrArrayMetadata &oInputArrayMetadata,
    ZarrArrayMetadata &oOutputArrayMetadata, bool /* bEmitWarnings */)
{
    m_oConfiguration = configuration.Clone();
    m_oInputArrayMetadata = oInputArrayMetadata;
    oOutputArrayMetadata = oInputArrayMetadata;
    return true;
}

/************************************************************************/
/*                    ZarrV3CodecVLenUTF8::Clone()                      */
/************************************************************************/

std::unique_ptr<ZarrV3Codec> ZarrV3CodecVLenUTF8::Clone() const
{
    auto psClone = std::make_unique<ZarrV3CodecVLenUTF8>();
    ZarrArrayMetadata oOutputArrayMetadata;
    psClone->InitFromConfiguration(m_oConfiguration, m_oInputArrayMetadata,
                                   oOutputArrayMetadata,
                                   /* bEmitWarnings = */ false);
    return psClone;
}

/************************************************************************/
/*                    ZarrV3CodecVLenUTF8::Encode()                     */
/************************************************************************/

bool ZarrV3CodecVLenUTF8::Encode(
    const ZarrByteVectorQuickResize & /* abySrc */,
    ZarrByteVectorQuickResize & /* abyDst */) const
{
    CPLError(CE_Failure, CPLE_NotSupported,
             "ZarrV3CodecVLenUTF8::Encode(): write not yet implemented");
    return false;
}

/************************************************************************/
/*                    ZarrV3CodecVLenUTF8::Decode()                     */
/************************************************************************/

bool ZarrV3CodecVLenUTF8::Decode(const ZarrByteVectorQuickResize &abySrc,
                                  ZarrByteVectorQuickResize &abyDst) const
{
    const size_t nSrcSize = abySrc.size();
    const size_t nSlotSize = m_oInputArrayMetadata.oElt.nativeSize;

    if (nSlotSize < 2)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "vlen-utf8: invalid slot size %u",
                 static_cast<unsigned>(nSlotSize));
        return false;
    }

    // Minimum: 4 bytes for item_count
    if (nSrcSize < 4)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "vlen-utf8: buffer too small for header");
        return false;
    }

    const GByte *pSrc = abySrc.data();

    // Read item count (little-endian u32)
    uint32_t nItems = 0;
    memcpy(&nItems, pSrc, 4);
    CPL_LSBPTR32(&nItems);

    const size_t nExpected =
        MultiplyElements(m_oInputArrayMetadata.anBlockSizes);
    if (nItems != nExpected)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "vlen-utf8: item_count %u != expected %u from block shape",
                 nItems, static_cast<uint32_t>(nExpected));
        return false;
    }

    // Allocate output: nItems * nSlotSize null-padded slots
    const size_t nDstSize = static_cast<size_t>(nItems) * nSlotSize;
    try
    {
        abyDst.resize(nDstSize);
    }
    catch (const std::bad_alloc &)
    {
        CPLError(CE_Failure, CPLE_OutOfMemory,
                 "vlen-utf8: cannot allocate %" PRIu64
                 " bytes for decoded buffer",
                 static_cast<uint64_t>(nDstSize));
        return false;
    }
    memset(abyDst.data(), 0, nDstSize);

    // Parse interleaved format and copy strings into fixed-size slots.
    // Strings exceeding (nSlotSize - 1) bytes are silently truncated.
    size_t nOffset = 4;
    GByte *pDst = abyDst.data();
    bool bTruncated = false;
    for (uint32_t i = 0; i < nItems; ++i)
    {
        if (nOffset + 4 > nSrcSize)
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "vlen-utf8: truncated buffer at string %u", i);
            return false;
        }
        uint32_t nLen = 0;
        memcpy(&nLen, pSrc + nOffset, 4);
        CPL_LSBPTR32(&nLen);
        nOffset += 4;

        if (nOffset + nLen > nSrcSize)
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "vlen-utf8: truncated buffer at string %u data", i);
            return false;
        }

        const size_t nCopy = std::min(static_cast<size_t>(nLen), nSlotSize - 1);
        if (nLen > nSlotSize - 1)
            bTruncated = true;
        memcpy(pDst, pSrc + nOffset, nCopy);
        // Slot is already zero-filled, so null terminator is implicit
        nOffset += nLen;
        pDst += nSlotSize;
    }

    if (bTruncated)
    {
        CPLError(CE_Warning, CPLE_AppDefined,
                 "vlen-utf8: one or more strings truncated to %u bytes. "
                 "Increase ZARR_VLEN_STRING_MAX_LENGTH to read longer strings.",
                 static_cast<unsigned>(nSlotSize - 1));
    }

    return true;
}
