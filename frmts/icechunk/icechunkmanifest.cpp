/******************************************************************************
 *
 * Project:  GDAL
 * Purpose:  Icechunk driver
 * Author:   Even Rouault <even dot rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2026, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include "icechunkmanifest.h"
#include "icechunkutils.h"
#include "icechunkdrivercore.h"

/* ------------------------------------------------------------------------- */

#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdocumentation"
#pragma clang diagnostic ignored "-Wdocumentation-unknown-command"
#endif
#include <zstd.h>
#if defined(__clang__)
#pragma clang diagnostic pop
#endif

/* ------------------------------------------------------------------------- */

#include <algorithm>
#include <cinttypes>
#include <limits>
#if __cplusplus >= 202002L
#include <ranges>
#endif

/* ------------------------------------------------------------------------- */

#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Weffc++"
#pragma GCC diagnostic ignored "-Wnull-dereference"
#endif

#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wweak-vtables"
#endif

#include "generated/manifest_generated.h"

#if defined(__clang__)
#pragma clang diagnostic pop
#endif

#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif

/* ------------------------------------------------------------------------- */

using namespace flatbuffers;
using namespace generated;

namespace gdal::icechunk
{
IcechunkManifest::IcechunkManifest() = default;

IcechunkManifest::~IcechunkManifest() = default;

#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wnull-dereference"
#endif

/************************************************************************/
/*                       IcechunkManifest::Open()                       */
/************************************************************************/

std::unique_ptr<IcechunkManifest>
IcechunkManifest::Open(const char *pszFilename)
{
    CPLDebugOnly("Icechunk", "Opening manifest %s", pszFilename);
    auto fp = VSIFilesystemHandler::OpenStatic(pszFilename, "rb");
    if (!fp)
    {
        CPLError(CE_Failure, CPLE_FileIO, "Cannot open %s", pszFilename);
        return nullptr;
    }

    int nVersion = 0;
    auto [buffer, size] =
        DecompressFile(pszFilename, fp.get(), FILE_TYPE_MANIFEST, &nVersion);
    if (!buffer)
        return nullptr;

    {
        // By default max_tables is 1 million, which can be insufficient for
        // some datasets. See https://github.com/OSGeo/gdal/issues/14830
        const uoffset_t max_tables = static_cast<uoffset_t>(
            std::min<uint64_t>(size, std::numeric_limits<uoffset_t>::max()));
        Verifier verifier(buffer.get(), size, /* max_depth = */ 64, max_tables);
        if (!VerifyManifestBuffer(verifier))
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "%s: invalid Manifest Flatbuffer", pszFilename);
            return nullptr;
        }
    }

    const auto *fbsManifest = GetManifest(buffer.get());
    const auto *id = fbsManifest->id();
    CPLAssertNotNull(id);  // guaranteed by VerifyManifestBuffer()
    const auto idBytes = id->bytes();
    CPLAssertNotNull(idBytes);  // guaranteed by VerifyManifestBuffer()
    const std::string idBase32 = CrockfordBase32Encode(*idBytes);
    if (idBase32 != CPLGetFilename(pszFilename))
    {
        CPLError(CE_Failure, CPLE_AppDefined, "%s: id=%s != expected %s",
                 pszFilename, idBase32.c_str(), CPLGetFilename(pszFilename));
        return nullptr;
    }

    auto manifest = std::make_unique<IcechunkManifest>();
    manifest->m_osFilename = pszFilename;

    constexpr int COMPRESSION_ALG_NONE = 0;
    constexpr int COMPRESSION_ALG_ZSTD_DICT = 1;
    const int nCompressionAlg = nVersion == 1
                                    ? COMPRESSION_ALG_NONE
                                    : fbsManifest->compression_algorithm();
    if (nCompressionAlg != COMPRESSION_ALG_NONE &&
        nCompressionAlg != COMPRESSION_ALG_ZSTD_DICT)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "%s: invalid compression_algorithm = %d", pszFilename,
                 nCompressionAlg);
        return nullptr;
    }

    struct ZSTDContextFreer
    {
        void operator()(ZSTD_DCtx *ctx)
        {
            ZSTD_freeDCtx(ctx);
        }
    };

    std::unique_ptr<ZSTD_DCtx, ZSTDContextFreer> dctx;
    if (nCompressionAlg == COMPRESSION_ALG_ZSTD_DICT)
    {
        dctx.reset(ZSTD_createDCtx());
        if (!dctx)
            return nullptr;
        if (const auto location_dictionary = fbsManifest->location_dictionary())
        {
#if (ZSTD_VERSION_MAJOR > 1) ||                                                \
    (ZSTD_VERSION_MAJOR == 1 && ZSTD_VERSION_MINOR >= 4)
            CPLDebugOnly("Icechunk", "%s: ZSTD dictionary of size %u",
                         pszFilename,
                         static_cast<uint32_t>(location_dictionary->size()));
            if (ZSTD_isError(ZSTD_DCtx_loadDictionary(
                    dctx.get(), location_dictionary->data(),
                    location_dictionary->size())))
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "%s: ZSTD_DCtx_loadDictionary() failed", pszFilename);
                return nullptr;
            }
#else
#error "ZSTD_DCtx_loadDictionary() requires libzstd >= 1.4"
#endif
        }
    }

    // 1024 should be sufficiently large for any practical purpose
    std::vector<char> achTempDecompressedLocation(1024);

    const auto *fbsArrays = fbsManifest->arrays();
    CPLAssertAlways(fbsArrays);  // guaranteed by VerifyManifestBuffer()
    manifest->m_arrayManifests.reserve(fbsArrays->size());

    for (const auto *arrayManifestFbs : *fbsArrays)
    {
        const auto *fbsNodeId = arrayManifestFbs->node_id();
        CPLAssertNotNull(fbsNodeId);  // guaranteed by VerifyManifestBuffer()
        const auto fbsNodeIdBytes = fbsNodeId->bytes();
        CPLAssertAlways(
            fbsNodeIdBytes);  // guaranteed by VerifyManifestBuffer()

        ArrayManifest arrayManifest;
        ObjectId8 &nodeId = arrayManifest.nodeId;
        static_assert(sizeof(*fbsNodeIdBytes) == sizeof(nodeId));
        memcpy(nodeId.data(), fbsNodeIdBytes->data(), sizeof(nodeId));

        // We rely on that order, required by the spec, in GetChunkRef()
        if (!manifest->m_arrayManifests.empty() &&
            nodeId <= manifest->m_arrayManifests.back().nodeId)
        {
            CPLError(
                CE_Failure, CPLE_AppDefined,
                "%s: arrayManifests array not sorted by increasing node id",
                pszFilename);
            return nullptr;
        }

        const auto GetNodeIdStr = [fbsNodeIdBytes]()
        { return CrockfordBase32Encode(*fbsNodeIdBytes); };

        CPLDebugOnly("Icechunk", "%s: manifest nodeId %s", pszFilename,
                     GetNodeIdStr().c_str());
        const auto *refs = arrayManifestFbs->refs();
        CPLAssertAlways(refs);  // guaranteed by VerifyManifestBuffer()
        arrayManifest.chunkRefs.reserve(refs->size());

        manifest->m_chunkRefsCount += refs->size();

        for (const auto *ref : *refs)
        {
            const auto *index = ref->index();
            CPLAssertAlways(index);  // guaranteed by VerifyManifestBuffer()

            ChunkRef chunkRef;

            chunkRef.idx = ChunkIdx(index->begin(), index->end());
            if (!arrayManifest.chunkRefs.empty())
            {
                const auto &prevChunkRef = arrayManifest.chunkRefs.back();

                // Not formally needed by the spec, but cannot hurt
                if (chunkRef.idx.size() != prevChunkRef.idx.size())
                {
                    CPLError(CE_Failure, CPLE_AppDefined,
                             "%s: chunkRefs array for node %s: chunk index do "
                             "not have the same dimension",
                             pszFilename, GetNodeIdStr().c_str());
                    return nullptr;
                }

                // We rely on that order, required by the spec, in GetChunkRef()
                if (chunkRef.idx <= prevChunkRef.idx)
                {
                    CPLError(CE_Failure, CPLE_AppDefined,
                             "%s: chunkRefs array for node %s: not sorted by "
                             "increasing chunk index",
                             pszFilename, GetNodeIdStr().c_str());
                    return nullptr;
                }
            }

            chunkRef.offset = ref->offset();
            chunkRef.length = ref->length();
            if (chunkRef.offset >
                std::numeric_limits<uint64_t>::max() - chunkRef.length)
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "%s: chunkRef: invalid offset/size", pszFilename);
                return nullptr;
            }

            chunkRef.checksumLastModified = ref->checksum_last_modified();

            if (const auto checksumEtag = ref->checksum_etag())
            {
                chunkRef.checksumEtag = GetString(checksumEtag);
            }

            int nAlternativeCount = 0;
            if (const auto inlineContent = ref->inline_())
            {
                ++nAlternativeCount;
                chunkRef.inlineContent.insert(chunkRef.inlineContent.end(),
                                              inlineContent->begin(),
                                              inlineContent->end());

                if (chunkRef.offset != 0 || chunkRef.length != 0)
                {
                    CPLError(CE_Failure, CPLE_AppDefined,
                             "%s: chunkRef: offset/size != 0 found with inline "
                             "content",
                             pszFilename);
                    return nullptr;
                }
            }

            if (const auto chunk_id = ref->chunk_id())
            {
                ++nAlternativeCount;
                CPLAssertAlways(
                    chunk_id->bytes());  // guaranteed by VerifyManifestBuffer()
                chunkRef.chunkId = CrockfordBase32Encode(*(chunk_id->bytes()));
            }

            if (const auto location = ref->location())
            {
                ++nAlternativeCount;
                chunkRef.location = GetString(location);
            }

            if (const auto compressed_location = ref->compressed_location())
            {
                ++nAlternativeCount;
                if (nCompressionAlg == COMPRESSION_ALG_NONE)
                {
                    // Code path likely not possible when using Icechunk writer
                    const char *pchLocation = reinterpret_cast<const char *>(
                        compressed_location->data());
                    chunkRef.location.insert(
                        chunkRef.location.end(), pchLocation,
                        pchLocation + compressed_location->size());
                }
                else
                {
                    CPLAssert(nCompressionAlg == COMPRESSION_ALG_ZSTD_DICT);

                    const size_t nStatus = ZSTD_decompressDCtx(
                        dctx.get(), achTempDecompressedLocation.data(),
                        achTempDecompressedLocation.size(),
                        compressed_location->data(),
                        compressed_location->size());
                    if (ZSTD_isError(nStatus))
                    {
                        CPLError(CE_Failure, CPLE_AppDefined,
                                 "%s: chunkRef node_id %s: "
                                 "ZSTD_decompressDCtx() failed",
                                 pszFilename, GetNodeIdStr().c_str());
                        return nullptr;
                    }
                    chunkRef.location.assign(achTempDecompressedLocation.data(),
                                             nStatus);
                }
            }

            if (nAlternativeCount == 0)
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "%s: chunkRef node_id %s: not inline, chunk or "
                         "virtual location",
                         pszFilename, GetNodeIdStr().c_str());
                return nullptr;
            }
            else if (nAlternativeCount > 1)
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "%s: chunkRef node_id %s: more than one method among "
                         "inline, chunk or virtual location found. "
                         "inlineContent.size() = %" PRIu64 ", offset = %" PRIu64
                         ", length = %" PRIu64 ", chunkId=%s, location=%s",
                         pszFilename, GetNodeIdStr().c_str(),
                         static_cast<uint64_t>(chunkRef.inlineContent.size()),
                         chunkRef.offset, chunkRef.length,
                         chunkRef.chunkId.c_str(), chunkRef.location.c_str());
                return nullptr;
            }

            arrayManifest.chunkRefs.push_back(std::move(chunkRef));
        }

        manifest->m_arrayManifests.push_back(std::move(arrayManifest));
    }

    return manifest;
}

#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif

/************************************************************************/
/*                 IcechunkManifest::GetChunkFilename()                 */
/************************************************************************/

std::string IcechunkManifest::GetChunkFilename(const std::string &chunkId) const
{
    return CPLFormFilenameSafe(
        CPLFormFilenameSafe(
            CPLGetDirnameSafe(CPLGetDirnameSafe(m_osFilename.c_str()).c_str())
                .c_str(),
            "chunks", nullptr)
            .c_str(),
        chunkId.c_str(), nullptr);
}

/************************************************************************/
/*                   IcechunkManifest::GetChunkRef()                    */
/************************************************************************/

const IcechunkManifest::ChunkRef *
IcechunkManifest::GetChunkRef(const ObjectId8 &nodeId,
                              const ChunkIdx &idx) const
{
#if __cplusplus >= 202002L
    const auto iterArrayManifests = std::ranges::lower_bound(
        m_arrayManifests, nodeId, {}, &ArrayManifest::nodeId);
#else
    ArrayManifest arrayManifestLookup;
    arrayManifestLookup.nodeId = nodeId;
    const auto iterArrayManifests = std::lower_bound(
        m_arrayManifests.begin(), m_arrayManifests.end(), arrayManifestLookup,
        [](const ArrayManifest &a, const ArrayManifest &b)
        { return a.nodeId < b.nodeId; });
#endif
    if (iterArrayManifests == m_arrayManifests.end() ||
        iterArrayManifests->nodeId != nodeId)
    {
        return nullptr;
    }
    const auto &arrayManifest = *iterArrayManifests;

    if (idx.empty() && arrayManifest.chunkRefs.size() == 1 &&
        arrayManifest.chunkRefs[0].idx.size() == 1 &&
        arrayManifest.chunkRefs[0].idx[0] == 0)
    {
        // Special case for scalar arrays such as "crs" written by Icechunk v0
        return &(arrayManifest.chunkRefs[0]);
    }

#if __cplusplus >= 202002L
    const auto iterChunkRefs = std::ranges::lower_bound(
        arrayManifest.chunkRefs, idx, {}, &ChunkRef::idx);
#else
    ChunkRef chunkRefLookup;
    chunkRefLookup.idx = idx;
    const auto iterChunkRefs = std::lower_bound(
        arrayManifest.chunkRefs.begin(), arrayManifest.chunkRefs.end(),
        chunkRefLookup,
        [](const ChunkRef &a, const ChunkRef &b) { return a.idx < b.idx; });
#endif
    if (iterChunkRefs == arrayManifest.chunkRefs.end() ||
        iterChunkRefs->idx != idx)
    {
        if (!arrayManifest.chunkRefs.empty() &&
            idx.size() != arrayManifest.chunkRefs.front().idx.size())
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "GetChunkRef(%s): querying with index of dimension %u "
                     "whereas chunk refs have dimension %u",
                     CrockfordBase32Encode(nodeId).c_str(),
                     static_cast<unsigned>(idx.size()),
                     static_cast<unsigned>(
                         arrayManifest.chunkRefs.front().idx.size()));
        }

        return nullptr;
    }

    return &(*iterChunkRefs);
}

}  // namespace gdal::icechunk
