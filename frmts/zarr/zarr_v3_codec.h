/******************************************************************************
 *
 * Project:  GDAL
 * Purpose:  Zarr driver
 * Author:   Even Rouault <even dot rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2021, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#ifndef ZARR_V3_CODEC_H
#define ZARR_V3_CODEC_H

#include "zarr.h"

struct VSIVirtualHandle;

/************************************************************************/
/*                          ZarrArrayMetadata                           */
/************************************************************************/

/** Array-related metadata needed for the good working of Zarr V3 codecs */
struct ZarrArrayMetadata
{
    /** Data type of the array */
    DtypeElt oElt{};

    /** Shape of a block/chunk */
    std::vector<size_t> anBlockSizes{};

    /** No data value of the array. Empty or abyNoData.size() == oElt.nNativeSize */
    std::vector<GByte> abyNoData{};
};

/************************************************************************/
/*                             ZarrV3Codec                              */
/************************************************************************/

/** Abstract class for a Zarr V3 codec */
class ZarrV3Codec CPL_NON_FINAL
{
  protected:
    const std::string m_osName;
    CPLJSONObject m_oConfiguration{};
    ZarrArrayMetadata m_oInputArrayMetadata{};

    ZarrV3Codec(const std::string &osName);

  public:
    virtual ~ZarrV3Codec();

    enum class IOType
    {
        BYTES,
        ARRAY
    };

    virtual IOType GetInputType() const = 0;
    virtual IOType GetOutputType() const = 0;

    virtual bool
    InitFromConfiguration(const CPLJSONObject &configuration,
                          const ZarrArrayMetadata &oInputArrayMetadata,
                          ZarrArrayMetadata &oOutputArrayMetadata,
                          bool bEmitWarnings) = 0;

    virtual std::unique_ptr<ZarrV3Codec> Clone() const = 0;

    virtual bool IsNoOp() const
    {
        return false;
    }

    virtual bool Encode(const ZarrByteVectorQuickResize &abySrc,
                        ZarrByteVectorQuickResize &abyDst) const = 0;
    virtual bool Decode(const ZarrByteVectorQuickResize &abySrc,
                        ZarrByteVectorQuickResize &abyDst) const = 0;

    /** Partial decoding.
     * anStartIdx[i]: coordinate in pixel, within the array of an outer chunk,
     * that is < m_oInputArrayMetadata.anBlockSizes[i]
     * anCount[i]: number of pixels to extract <= m_oInputArrayMetadata.anBlockSizes[i]
     */
    virtual bool DecodePartial(VSIVirtualHandle *poFile,
                               const ZarrByteVectorQuickResize &abySrc,
                               ZarrByteVectorQuickResize &abyDst,
                               std::vector<size_t> &anStartIdx,
                               std::vector<size_t> &anCount);

    const std::string &GetName() const
    {
        return m_osName;
    }

    const CPLJSONObject &GetConfiguration() const
    {
        return m_oConfiguration;
    }

    virtual std::vector<size_t>
    GetInnerMostBlockSize(const std::vector<size_t> &input) const
    {
        return input;
    }

    virtual void ChangeArrayShapeForward(std::vector<size_t> &anStartIdx,
                                         std::vector<size_t> &anCount)
    {
        (void)anStartIdx;
        (void)anCount;
    }
};

/************************************************************************/
/*                    ZarrV3CodecAbstractCompressor                     */
/************************************************************************/

class ZarrV3CodecAbstractCompressor CPL_NON_FINAL : public ZarrV3Codec
{
  protected:
    CPLStringList m_aosCompressorOptions{};
    const CPLCompressor *m_pDecompressor = nullptr;
    const CPLCompressor *m_pCompressor = nullptr;

    explicit ZarrV3CodecAbstractCompressor(const std::string &osName);

    ZarrV3CodecAbstractCompressor(const ZarrV3CodecAbstractCompressor &) =
        delete;
    ZarrV3CodecAbstractCompressor &
    operator=(const ZarrV3CodecAbstractCompressor &) = delete;

  public:
    IOType GetInputType() const override
    {
        return IOType::BYTES;
    }

    IOType GetOutputType() const override
    {
        return IOType::BYTES;
    }

    bool Encode(const ZarrByteVectorQuickResize &abySrc,
                ZarrByteVectorQuickResize &abyDst) const override;
    bool Decode(const ZarrByteVectorQuickResize &abySrc,
                ZarrByteVectorQuickResize &abyDst) const override;
};

/************************************************************************/
/*                           ZarrV3CodecGZip                            */
/************************************************************************/

// Implements https://zarr-specs.readthedocs.io/en/latest/v3/codecs/gzip/v1.0.html
class ZarrV3CodecGZip final : public ZarrV3CodecAbstractCompressor
{
  public:
    static constexpr const char *NAME = "gzip";

    ZarrV3CodecGZip();

    static CPLJSONObject GetConfiguration(int nLevel);

    bool InitFromConfiguration(const CPLJSONObject &configuration,
                               const ZarrArrayMetadata &oInputArrayMetadata,
                               ZarrArrayMetadata &oOutputArrayMetadata,
                               bool bEmitWarnings) override;

    std::unique_ptr<ZarrV3Codec> Clone() const override;
};

/************************************************************************/
/*                           ZarrV3CodecBlosc                           */
/************************************************************************/

// Implements https://zarr-specs.readthedocs.io/en/latest/v3/codecs/blosc/v1.0.html
class ZarrV3CodecBlosc final : public ZarrV3CodecAbstractCompressor
{
  public:
    static constexpr const char *NAME = "blosc";

    ZarrV3CodecBlosc();

    static CPLJSONObject GetConfiguration(const char *cname, int clevel,
                                          const char *shuffle, int typesize,
                                          int blocksize);

    bool InitFromConfiguration(const CPLJSONObject &configuration,
                               const ZarrArrayMetadata &oInputArrayMetadata,
                               ZarrArrayMetadata &oOutputArrayMetadata,
                               bool bEmitWarnings) override;

    std::unique_ptr<ZarrV3Codec> Clone() const override;
};

/************************************************************************/
/*                           ZarrV3CodecZstd                            */
/************************************************************************/

// Implements https://github.com/zarr-developers/zarr-specs/pull/256
class ZarrV3CodecZstd final : public ZarrV3CodecAbstractCompressor
{
  public:
    static constexpr const char *NAME = "zstd";

    ZarrV3CodecZstd();

    static CPLJSONObject GetConfiguration(int level, bool checksum);

    bool InitFromConfiguration(const CPLJSONObject &configuration,
                               const ZarrArrayMetadata &oInputArrayMetadata,
                               ZarrArrayMetadata &oOutputArrayMetadata,
                               bool bEmitWarnings) override;

    std::unique_ptr<ZarrV3Codec> Clone() const override;
};

/************************************************************************/
/*                           ZarrV3CodecBytes                           */
/************************************************************************/

// Implements https://zarr-specs.readthedocs.io/en/latest/v3/codecs/bytes/v1.0.html
class ZarrV3CodecBytes final : public ZarrV3Codec
{
    bool m_bLittle = true;

  public:
    static constexpr const char *NAME = "bytes";

    ZarrV3CodecBytes();

    IOType GetInputType() const override
    {
        return IOType::ARRAY;
    }

    IOType GetOutputType() const override
    {
        return IOType::BYTES;
    }

    static CPLJSONObject GetConfiguration(bool bLittle);

    bool InitFromConfiguration(const CPLJSONObject &configuration,
                               const ZarrArrayMetadata &oInputArrayMetadata,
                               ZarrArrayMetadata &oOutputArrayMetadata,
                               bool bEmitWarnings) override;

    bool IsLittle() const
    {
        return m_bLittle;
    }

    bool IsNoOp() const override
    {
        if constexpr (CPL_IS_LSB)
            return m_oInputArrayMetadata.oElt.nativeSize == 1 || m_bLittle;
        else
            return m_oInputArrayMetadata.oElt.nativeSize == 1 || !m_bLittle;
    }

    std::unique_ptr<ZarrV3Codec> Clone() const override;

    bool Encode(const ZarrByteVectorQuickResize &abySrc,
                ZarrByteVectorQuickResize &abyDst) const override;
    bool Decode(const ZarrByteVectorQuickResize &abySrc,
                ZarrByteVectorQuickResize &abyDst) const override;
};

/************************************************************************/
/*                         ZarrV3CodecTranspose                         */
/************************************************************************/

// Implements https://zarr-specs.readthedocs.io/en/latest/v3/codecs/transpose/v1.0.html
class ZarrV3CodecTranspose final : public ZarrV3Codec
{
    // m_anOrder is such that dest_shape[i] = source_shape[m_anOrder[i]]
    // where source_shape[] is the size of the array before the Encode() operation
    // and dest_shape[] its size after.
    // m_anOrder[] describes a bijection of [0,N-1] to [0,N-1]
    std::vector<int> m_anOrder{};

    // m_anReverseOrder is such that m_anReverseOrder[m_anOrder[i]] = i
    std::vector<int> m_anReverseOrder{};

    bool Transpose(const ZarrByteVectorQuickResize &abySrc,
                   ZarrByteVectorQuickResize &abyDst, bool bEncodeDirection,
                   const std::vector<size_t> &anForwardBlockSizes) const;

  public:
    static constexpr const char *NAME = "transpose";

    ZarrV3CodecTranspose();

    IOType GetInputType() const override
    {
        return IOType::ARRAY;
    }

    IOType GetOutputType() const override
    {
        return IOType::ARRAY;
    }

    static CPLJSONObject GetConfiguration(const std::vector<int> &anOrder);

    bool InitFromConfiguration(const CPLJSONObject &configuration,
                               const ZarrArrayMetadata &oInputArrayMetadata,
                               ZarrArrayMetadata &oOutputArrayMetadata,
                               bool bEmitWarnings) override;

    const std::vector<int> &GetOrder() const
    {
        return m_anOrder;
    }

    bool IsNoOp() const override;

    std::unique_ptr<ZarrV3Codec> Clone() const override;

    bool Encode(const ZarrByteVectorQuickResize &abySrc,
                ZarrByteVectorQuickResize &abyDst) const override;
    bool Decode(const ZarrByteVectorQuickResize &abySrc,
                ZarrByteVectorQuickResize &abyDst) const override;

    bool DecodePartial(VSIVirtualHandle *poFile,
                       const ZarrByteVectorQuickResize &abySrc,
                       ZarrByteVectorQuickResize &abyDst,
                       std::vector<size_t> &anStartIdx,
                       std::vector<size_t> &anCount) override;

    std::vector<size_t>
    GetInnerMostBlockSize(const std::vector<size_t> &input) const override;

    template <class T>
    inline void Reorder1DForward(std::vector<T> &vector) const
    {
        std::vector<T> res;
        for (int idx : m_anOrder)
            res.push_back(vector[idx]);
        vector = std::move(res);
    }

    template <class T>
    inline void Reorder1DInverse(std::vector<T> &vector) const
    {
        std::vector<T> res;
        for (int idx : m_anReverseOrder)
            res.push_back(vector[idx]);
        vector = std::move(res);
    }

    void ChangeArrayShapeForward(std::vector<size_t> &anStartIdx,
                                 std::vector<size_t> &anCount) override
    {
        Reorder1DForward(anStartIdx);
        Reorder1DForward(anCount);
    }
};

/************************************************************************/
/*                          ZarrV3CodecCRC32C                           */
/************************************************************************/

// Implements https://zarr-specs.readthedocs.io/en/latest/v3/codecs/crc32c/index.html
class ZarrV3CodecCRC32C final : public ZarrV3Codec
{
    bool m_bCheckCRC = true;

  public:
    static constexpr const char *NAME = "crc32c";

    ZarrV3CodecCRC32C();

    IOType GetInputType() const override
    {
        return IOType::BYTES;
    }

    IOType GetOutputType() const override
    {
        return IOType::BYTES;
    }

    bool InitFromConfiguration(const CPLJSONObject &configuration,
                               const ZarrArrayMetadata &oInputArrayMetadata,
                               ZarrArrayMetadata &oOutputArrayMetadata,
                               bool bEmitWarnings) override;

    std::unique_ptr<ZarrV3Codec> Clone() const override;

    bool Encode(const ZarrByteVectorQuickResize &abySrc,
                ZarrByteVectorQuickResize &abyDst) const override;
    bool Decode(const ZarrByteVectorQuickResize &abySrc,
                ZarrByteVectorQuickResize &abyDst) const override;
};

/************************************************************************/
/*                      ZarrV3CodecShardingIndexed                      */
/************************************************************************/

class ZarrV3CodecSequence;

// https://zarr-specs.readthedocs.io/en/latest/v3/codecs/sharding-indexed/index.html
class ZarrV3CodecShardingIndexed final : public ZarrV3Codec
{
    std::unique_ptr<ZarrV3CodecSequence> m_poCodecSequence{};
    std::unique_ptr<ZarrV3CodecSequence> m_poIndexCodecSequence{};
    bool m_bIndexLocationAtEnd = true;
    bool m_bIndexHasCRC32 = false;
    std::vector<size_t> m_anInnerBlockSize{};

    struct Location
    {
        uint64_t nOffset;
        uint64_t nSize;
    };

  public:
    static constexpr const char *NAME = "sharding_indexed";

    ZarrV3CodecShardingIndexed();

    IOType GetInputType() const override
    {
        return IOType::ARRAY;
    }

    IOType GetOutputType() const override
    {
        return IOType::BYTES;
    }

    bool InitFromConfiguration(const CPLJSONObject &configuration,
                               const ZarrArrayMetadata &oInputArrayMetadata,
                               ZarrArrayMetadata &oOutputArrayMetadata,
                               bool bEmitWarnings) override;

    std::unique_ptr<ZarrV3Codec> Clone() const override;

    bool Encode(const ZarrByteVectorQuickResize &abySrc,
                ZarrByteVectorQuickResize &abyDst) const override;
    bool Decode(const ZarrByteVectorQuickResize &abySrc,
                ZarrByteVectorQuickResize &abyDst) const override;

    bool DecodePartial(VSIVirtualHandle *poFile,
                       const ZarrByteVectorQuickResize &abySrc,
                       ZarrByteVectorQuickResize &abyDst,
                       std::vector<size_t> &anStartIdx,
                       std::vector<size_t> &anCount) override;

    /** Batch-read multiple inner chunks from the same shard via two
     *  ReadMultiRange() passes (index entries, then data), then decode.
     *  No persistent state is kept — each call reads the needed index
     *  entries on demand.
     */
    bool BatchDecodePartial(
        VSIVirtualHandle *poFile,
        const std::vector<std::pair<std::vector<size_t>, std::vector<size_t>>>
            &anRequests,
        std::vector<ZarrByteVectorQuickResize> &aResults);

    std::vector<size_t>
    GetInnerMostBlockSize(const std::vector<size_t> &input) const override;
};

/************************************************************************/
/*                         ZarrV3CodecSequence                          */
/************************************************************************/

class ZarrV3CodecSequence
{
    const ZarrArrayMetadata m_oInputArrayMetadata;
    std::vector<std::unique_ptr<ZarrV3Codec>> m_apoCodecs{};
    CPLJSONObject m_oCodecArray{};
    ZarrByteVectorQuickResize m_abyTmp{};
    bool m_bPartialDecodingPossible = false;

    bool AllocateBuffer(ZarrByteVectorQuickResize &abyBuffer, size_t nEltCount);

  public:
    explicit ZarrV3CodecSequence(const ZarrArrayMetadata &oInputArrayMetadata)
        : m_oInputArrayMetadata(oInputArrayMetadata)
    {
    }

    // This method is not thread safe due to cloning a JSON object
    std::unique_ptr<ZarrV3CodecSequence> Clone() const;

    bool InitFromJson(const CPLJSONObject &oCodecs,
                      ZarrArrayMetadata &oOutputArrayMetadata);

    const CPLJSONObject &GetJSon() const
    {
        return m_oCodecArray;
    }

    const std::vector<std::unique_ptr<ZarrV3Codec>> &GetCodecs() const
    {
        return m_apoCodecs;
    }

    bool SupportsPartialDecoding() const
    {
        return m_bPartialDecodingPossible;
    }

    bool Encode(ZarrByteVectorQuickResize &abyBuffer);
    bool Decode(ZarrByteVectorQuickResize &abyBuffer);

    /** Partial decoding.
     * anStartIdx[i]: coordinate in pixel, within the array of an outer chunk,
     * that is < m_oInputArrayMetadata.anBlockSizes[i]
     * anCount[i]: number of pixels to extract <= m_oInputArrayMetadata.anBlockSizes[i]
     */
    bool DecodePartial(VSIVirtualHandle *poFile,
                       ZarrByteVectorQuickResize &abyBuffer,
                       const std::vector<size_t> &anStartIdx,
                       const std::vector<size_t> &anCount);

    /** Batch-read multiple inner chunks via ReadMultiRange().
     *  Delegates to the sharding codec if present, otherwise falls back
     *  to sequential DecodePartial() calls.
     */
    bool BatchDecodePartial(
        VSIVirtualHandle *poFile,
        const std::vector<std::pair<std::vector<size_t>, std::vector<size_t>>>
            &anRequests,
        std::vector<ZarrByteVectorQuickResize> &aResults);

    std::vector<size_t>
    GetInnerMostBlockSize(const std::vector<size_t> &anOuterBlockSize) const;
};

#endif
