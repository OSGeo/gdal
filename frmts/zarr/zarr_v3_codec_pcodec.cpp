/******************************************************************************
 *
 * Project:  GDAL
 * Purpose:  Zarr driver, "numcodecs.pcodec" codec
 * Author:   Even Rouault <even dot rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2026, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

// Implements https://numcodecs.readthedocs.io/en/stable/compression/pcodec.html

#include "zarr_v3_codec.h"

// Header from https://github.com/pcodec/pcodec/tree/main/pco_c/include
#include "cpcodec.h"

#include <cinttypes>

/************************************************************************/
/*                ZarrV3CodecPcodec::ZarrV3CodecPcodec()                */
/************************************************************************/

ZarrV3CodecPcodec::ZarrV3CodecPcodec() : ZarrV3Codec(NAME)
{
}

/************************************************************************/
/*               ZarrV3CodecPcodec::~ZarrV3CodecPcodec()                */
/************************************************************************/

ZarrV3CodecPcodec::~ZarrV3CodecPcodec() = default;

/************************************************************************/
/*                      ZarrV3CodecPcodec::Clone()                      */
/************************************************************************/

std::unique_ptr<ZarrV3Codec> ZarrV3CodecPcodec::Clone() const
{
    auto psClone = std::make_unique<ZarrV3CodecPcodec>();
    ZarrArrayMetadata oOutputArrayMetadata;
    psClone->InitFromConfiguration(std::string(), m_oConfiguration,
                                   m_oInputArrayMetadata, oOutputArrayMetadata,
                                   /* bEmitWarnings = */ false);
    return psClone;
}

/************************************************************************/
/*              ZarrV3CodecPcodec::InitFromConfiguration()              */
/************************************************************************/

bool ZarrV3CodecPcodec::InitFromConfiguration(
    const std::string & /*osArrayName*/, const CPLJSONObject &configuration,
    const ZarrArrayMetadata &oInputArrayMetadata,
    ZarrArrayMetadata &oOutputArrayMetadata, bool /* bEmitWarnings */)
{
    m_oConfiguration = configuration;
    m_oInputArrayMetadata = oInputArrayMetadata;
    oOutputArrayMetadata = oInputArrayMetadata;

    return true;
}

/************************************************************************/
/*                     ZarrV3CodecPcodec::Decode()                      */
/************************************************************************/

bool ZarrV3CodecPcodec::Decode(const ZarrByteVectorQuickResize &abySrc,
                               ZarrByteVectorQuickResize &abyDst) const
{
    const auto &oElt = m_oInputArrayMetadata.oElt;

    unsigned char pco_dtype = 0;
    switch (oElt.nativeType)
    {
        case DtypeElt::NativeType::BOOLEAN:
            CPLError(CE_Failure, CPLE_AppDefined,
                     "dtype=Boolean not supported by pcodec");
            return false;

        case DtypeElt::NativeType::UNSIGNED_INT:
        {
            switch (oElt.nativeSize)
            {
                case 1:
                    pco_dtype = PCO_TYPE_U8;
                    break;
                case 2:
                    pco_dtype = PCO_TYPE_U16;
                    break;
                case 4:
                    pco_dtype = PCO_TYPE_U32;
                    break;
                case 8:
                    pco_dtype = PCO_TYPE_U64;
                    break;
                default:
                {
                    CPLError(CE_Failure, CPLE_AppDefined,
                             "dtype=UNSIGNED_INT with size=%d not supported by "
                             "pcodec",
                             static_cast<int>(oElt.nativeSize));
                    return false;
                }
            }
            break;
        }

        case DtypeElt::NativeType::SIGNED_INT:
        {
            switch (oElt.nativeSize)
            {
                case 1:
                    pco_dtype = PCO_TYPE_I8;
                    break;
                case 2:
                    pco_dtype = PCO_TYPE_I16;
                    break;
                case 4:
                    pco_dtype = PCO_TYPE_I32;
                    break;
                case 8:
                    pco_dtype = PCO_TYPE_I64;
                    break;
                default:
                {
                    CPLError(
                        CE_Failure, CPLE_AppDefined,
                        "dtype=SIGNED_INT with size=%d not supported by pcodec",
                        static_cast<int>(oElt.nativeSize));
                    return false;
                }
            }
            break;
        }

        case DtypeElt::NativeType::IEEEFP:
        {
            switch (oElt.nativeSize)
            {
                case 2:
                    pco_dtype = PCO_TYPE_F16;
                    break;
                case 4:
                    pco_dtype = PCO_TYPE_F32;
                    break;
                case 8:
                    pco_dtype = PCO_TYPE_F64;
                    break;
                default:
                {
                    CPLError(
                        CE_Failure, CPLE_AppDefined,
                        "dtype=IEEEFP with size=%d not supported by pcodec",
                        static_cast<int>(oElt.nativeSize));
                    return false;
                }
            }
            break;
        }

        case DtypeElt::NativeType::COMPLEX_IEEEFP:
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "dtype=Complex not supported by pcodec");
            return false;
        }

        case DtypeElt::NativeType::STRING_ASCII:
        case DtypeElt::NativeType::STRING_UNICODE:
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "dtype=String not supported by pcodec");
            return false;
        }
    }

    const size_t nElements =
        MultiplyElements(m_oInputArrayMetadata.anBlockSizes);
    const size_t nExpectedSize = oElt.nativeSize * nElements;
    try
    {
        abyDst.resize(nExpectedSize);
    }
    catch (const std::exception &e)
    {
        CPLError(CE_Failure, CPLE_OutOfMemory, "%s", e.what());
        return false;
    }

    size_t nWritten = 0;
    const auto res = pco_standalone_simple_decompress_into(
        abySrc.data(), abySrc.size(), pco_dtype, abyDst.data(), abyDst.size(),
        &nWritten);
    if (res != PcoSuccess)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "pco_standalone_simple_decompress_into() failed");
        return false;
    }
    if (oElt.nativeSize * nWritten != abyDst.size())
    {
        CPLError(
            CE_Failure, CPLE_AppDefined,
            "pco_standalone_simple_decompress_into() only decompressed %" PRIu64
            " bytes, whereas %" PRIu64 " were expected",
            static_cast<uint64_t>(oElt.nativeSize * nWritten),
            static_cast<uint64_t>(abyDst.size()));
        return false;
    }

    return true;
}

/************************************************************************/
/*                     ZarrV3CodecPcodec::Encode()                      */
/************************************************************************/

bool ZarrV3CodecPcodec::Encode(const ZarrByteVectorQuickResize &,
                               ZarrByteVectorQuickResize &) const
{
    CPLError(CE_Failure, CPLE_NotSupported,
             "Encode() not currently supported for numcodecs.pcodec");
    return false;
}
