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

#include "icechunksnapshot.h"
#include "icechunkutils.h"
#include "icechunkdrivercore.h"

#include "cpl_vsi_virtual.h"

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

#include "flatbuffers/flexbuffers.h"
#include "generated/snapshot_generated.h"

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
IcechunkSnapshot::IcechunkSnapshot() = default;

IcechunkSnapshot::~IcechunkSnapshot() = default;

#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wnull-dereference"
#endif

/************************************************************************/
/*                       IcechunkSnapshot::Open()                       */
/************************************************************************/

std::unique_ptr<IcechunkSnapshot>
IcechunkSnapshot::Open(const char *pszFilename)
{
    CPLDebugOnly("Icechunk", "Opening snapshot %s", pszFilename);
    auto fp = VSIFilesystemHandler::OpenStatic(pszFilename, "rb");
    if (!fp)
    {
        CPLError(CE_Failure, CPLE_FileIO, "Cannot open %s", pszFilename);
        return nullptr;
    }

    int nVersion = 0;
    auto [buffer, size] =
        DecompressFile(pszFilename, fp.get(), FILE_TYPE_SNAPSHOT, &nVersion);
    if (!buffer)
        return nullptr;

    {
        Verifier verifier(buffer.get(), size);
        if (!VerifySnapshotBuffer(verifier))
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "%s: invalid Snapshot Flatbuffer", pszFilename);
            return nullptr;
        }
    }

    auto snapshot = std::make_unique<IcechunkSnapshot>();
    snapshot->m_osFilename = pszFilename;

    const auto *fbsSnapshot = GetSnapshot(buffer.get());
    const auto *id = fbsSnapshot->id();
    CPLAssertNotNull(id);  // guaranteed by VerifySnapshotBuffer()
    const auto idBytes = id->bytes();
    CPLAssertNotNull(idBytes);  // guaranteed by VerifySnapshotBuffer()
    const std::string snapshotIdBase32 = CrockfordBase32Encode(*idBytes);
    if (snapshotIdBase32 != CPLGetFilename(pszFilename))
    {
        CPLError(CE_Failure, CPLE_AppDefined, "%s: id=%s != expected %s",
                 pszFilename, snapshotIdBase32.c_str(),
                 CPLGetFilename(pszFilename));
        return nullptr;
    }

    const auto *message = fbsSnapshot->message();
    CPLAssertNotNull(message);  // guaranteed by VerifySnapshotBuffer()
    snapshot->m_osCommitMessage = GetString(message);
    CPLDebugOnly("Icechunk", "snapshot %s: commit message: '%s'", pszFilename,
                 snapshot->m_osCommitMessage.c_str());

    snapshot->m_flushTimestamp = fbsSnapshot->flushed_at();

    {
        const auto *metadata = fbsSnapshot->metadata();
        if (metadata && nVersion == 2)
        {
            for (const auto &md : *metadata)
            {
                if (const auto value = md->value())
                {
                    if (!flexbuffers::VerifyBuffer(value->data(),
                                                   value->size()))
                    {
                        CPLError(CE_Failure, CPLE_AppDefined,
                                 "%s: flexbuffers::VerifyBuffer() failed",
                                 pszFilename);
                        return nullptr;
                    }
                    if constexpr (IS_DEBUG_BUILD)
                    {
                        std::string val;
                        flexbuffers::GetRoot(value->data(), value->size())
                            .ToString(true, true, val);
                        CPLDebugOnly("Icechunk", "snapshot %s: metadata %s=%s",
                                     snapshotIdBase32.c_str(),
                                     md->name() ? md->name()->c_str()
                                                : "(null)",
                                     val.c_str());
                    }
                }
            }
        }
    }

    /* --------------------------------------------------------------------*/
    /* Parse nodes[] array                                                 */
    /* --------------------------------------------------------------------*/
    const auto *nodes = fbsSnapshot->nodes();
    CPLAssertAlways(nodes);  // guaranteed by VerifySnapshotBuffer()
    snapshot->m_nodes.reserve(nodes->size());
    bool needSort = false;
    for (const auto *nodePtr : *nodes)
    {
        const auto *fbsNodeId = nodePtr->id();
        CPLAssertNotNull(fbsNodeId);  // guaranteed by VerifySnapshotBuffer()
        const auto fbsNodeIdBytes = fbsNodeId->bytes();
        CPLAssertAlways(
            fbsNodeIdBytes);  // guaranteed by VerifySnapshotBuffer()

        ObjectId8 nodeId;
        static_assert(sizeof(*fbsNodeIdBytes) == sizeof(nodeId));
        memcpy(nodeId.data(), fbsNodeIdBytes->data(), sizeof(nodeId));

        const auto *pathPtr = nodePtr->path();
        CPLAssertNotNull(pathPtr);  // guaranteed by VerifySnapshotBuffer()

        const std::string path = GetString(pathPtr);
        if (path.empty() || path[0] != '/' ||
            (path.size() > 1 && path.back() == '/'))
        {
            CPLError(CE_Failure, CPLE_AppDefined, "%s: invalid node path '%s'",
                     pszFilename, path.c_str());
            return nullptr;
        }

        // Additional checks to avoid later issues. Might not be strictly
        // necessary, but if removing them, VSIIcechunkFileSystem should be
        // reviewed against risks of path traversal.
        if (path.find('\\') != std::string::npos ||
            path.find("/./") != std::string::npos ||
            path.find("/../") != std::string::npos ||
            cpl::ends_with(path, "/.."))
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "%s: path traversal pattern in node path '%s'",
                     pszFilename, path.c_str());
            return nullptr;
        }

        needSort = needSort || (!snapshot->m_nodes.empty() &&
                                snapshot->m_nodes.back().path > path);

        Node node;
        node.id = nodeId;
        node.path = path;

        const auto *userData = nodePtr->user_data();
        CPLAssertAlways(userData);  // guaranteed by VerifySnapshotBuffer()
        node.content.assign(reinterpret_cast<const char *>(userData->data()),
                            userData->size());

        CPLDebugOnly("Icechunk", "snapshot %s, node %s, path %s, user_data %s",
                     snapshotIdBase32.c_str(),
                     CrockfordBase32Encode(*(fbsNodeId->bytes())).c_str(),
                     path.c_str(), node.content.c_str());

        if (const auto arrayData = nodePtr->node_data_as_Array())
        {
            node.isArray = true;

            uint64_t totalNumChunks = 1;

            if (nVersion == 2)
            {
                const auto *shape = arrayData->shape_v2();
                if (!shape)
                {
                    CPLError(CE_Failure, CPLE_AppDefined,
                             "%s: missing shape_v2 in ArrayData", pszFilename);
                    return nullptr;
                }
                for (const auto *dimShape : *shape)
                {
                    const uint32_t numChunks = dimShape->num_chunks();
                    if (numChunks == 0)
                    {
                        CPLError(CE_Failure, CPLE_AppDefined,
                                 "%s: numChunks == 0", pszFilename);
                        return nullptr;
                    }
                    node.numChunks.push_back(numChunks);
                    if (numChunks >
                        std::numeric_limits<uint64_t>::max() / totalNumChunks)
                    {
                        CPLError(CE_Failure, CPLE_AppDefined,
                                 "%s: too many chunks", pszFilename);
                        return nullptr;
                    }
                    totalNumChunks *= numChunks;
                }
            }
            else
            {
                const auto *shape = arrayData->shape();
                CPLAssertAlways(shape);  // guaranteed by VerifySnapshotBuffer()
                for (const auto *dimShape : *shape)
                {
                    const uint64_t array_length = dimShape->array_length();
                    const uint64_t chunk_length = dimShape->chunk_length();
                    if (!array_length || !chunk_length)
                    {
                        CPLError(CE_Failure, CPLE_AppDefined,
                                 "%s: invalid shape in ArrayData", pszFilename);
                        return nullptr;
                    }
                    const uint64_t numChunks64 =
                        cpl::div_round_up(array_length, chunk_length);
                    if (numChunks64 > std::numeric_limits<uint32_t>::max())
                    {
                        CPLError(
                            CE_Failure, CPLE_AppDefined,
                            "%s: invalid shape in ArrayData: too many chunks",
                            pszFilename);
                        return nullptr;
                    }
                    const uint32_t numChunks =
                        static_cast<uint32_t>(numChunks64);
                    node.numChunks.push_back(numChunks);
                    if (numChunks >
                        std::numeric_limits<uint64_t>::max() / totalNumChunks)
                    {
                        CPLError(CE_Failure, CPLE_AppDefined,
                                 "%s: too many chunks", pszFilename);
                        return nullptr;
                    }
                    totalNumChunks *= numChunks;
                }
            }

            const auto *manifests = arrayData->manifests();
            CPLAssertAlways(manifests);  // guaranteed by VerifySnapshotBuffer()

            uint64_t totalNumChunksFromManifests = 0;
            for (const auto *manifestPtr : *manifests)
            {
                const auto manifestId = manifestPtr->object_id();
                CPLAssertNotNull(
                    manifestId);  // guaranteed by VerifySnapshotBuffer()
                const auto manifestIdBytes = manifestId->bytes();
                CPLAssertAlways(
                    manifestIdBytes);  // guaranteed by VerifySnapshotBuffer()

                ManifestRef manifestRef;
                static_assert(sizeof(*manifestIdBytes) ==
                              sizeof(manifestRef.manifestId));
                memcpy(manifestRef.manifestId.data(), manifestIdBytes->data(),
                       sizeof(manifestRef.manifestId));

                const auto extents = manifestPtr->extents();
                CPLAssertAlways(
                    extents);  // guaranteed by VerifySnapshotBuffer()

                CPLDebugOnly("Icechunk", "snapshot %s, manifest ref %s:",
                             snapshotIdBase32.c_str(),
                             CrockfordBase32Encode(*manifestIdBytes).c_str());

                uint64_t chunkCountFromManifest = 1;
                if (node.numChunks.empty() && extents->size() == 1 &&
                    node.numChunks.empty())
                {
                    // Special case for scalar arrays such as "crs" written by Icechunk v0
                    const auto *extent = (*extents)[0];
                    if (extent->from() != 0 || extent->to() != 1)
                    {
                        CPLError(CE_Failure, CPLE_AppDefined,
                                 "%s: array %s: invalid manifest extent "
                                 "[%u, %u[ for dim %u",
                                 pszFilename, path.c_str(), extent->from(),
                                 extent->to(), 0);
                        return nullptr;
                    }

                    manifestRef.extents.emplace_back(extent->from(),
                                                     extent->to());
                    CPLDebugOnly("Icechunk",
                                 "snapshot %s,   from %" PRIu32 " to %" PRIu32,
                                 snapshotIdBase32.c_str(),
                                 manifestRef.extents.back().from,
                                 manifestRef.extents.back().to);
                }
                else
                {
                    if (node.numChunks.size() != extents->size())
                    {
                        CPLError(
                            CE_Failure, CPLE_AppDefined,
                            "%s: array %s: manifest extents has not expected "
                            "dimension count. Got %d from manifest extents, "
                            "expected %d from node shape",
                            pszFilename, path.c_str(),
                            static_cast<int>(extents->size()),
                            static_cast<int>(node.numChunks.size()));
                        return nullptr;
                    }
                    for (unsigned iDim = 0; iDim < extents->size(); ++iDim)
                    {
                        const auto *extent = (*extents)[iDim];
                        if (extent->from() >= extent->to() ||
                            extent->from() >= node.numChunks[iDim] ||
                            extent->to() > node.numChunks[iDim])
                        {
                            CPLError(CE_Failure, CPLE_AppDefined,
                                     "%s: array %s: invalid manifest extent "
                                     "[%u, %u[ for dim %u",
                                     pszFilename, path.c_str(), extent->from(),
                                     extent->to(), iDim);
                            return nullptr;
                        }

                        // Overflow cannot happen given the validation of extent
                        // w.r.t node.numChunks and the fact that
                        // times(node.numChunks) has been checked to fit on uint64_t
                        chunkCountFromManifest *= extent->to() - extent->from();

                        manifestRef.extents.emplace_back(extent->from(),
                                                         extent->to());
                        CPLDebugOnly("Icechunk",
                                     "snapshot %s,   from %" PRIu32
                                     " to %" PRIu32,
                                     snapshotIdBase32.c_str(),
                                     manifestRef.extents.back().from,
                                     manifestRef.extents.back().to);
                    }
                }

                node.manifestRefs.push_back(std::move(manifestRef));

                if (totalNumChunksFromManifests >
                    std::numeric_limits<uint64_t>::max() -
                        chunkCountFromManifest)
                {
                    totalNumChunksFromManifests =
                        std::numeric_limits<uint64_t>::max();
                    break;
                }
                totalNumChunksFromManifests += chunkCountFromManifest;
            }

            // Quick partial consistency check
            if (totalNumChunksFromManifests > totalNumChunks)
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "%s: array %s: chunks referenced by manifest extents "
                         "= %" PRIu64 " > chunks in the array = %" PRIu64,
                         pszFilename, path.c_str(), totalNumChunksFromManifests,
                         totalNumChunks);
                return nullptr;
            }

            if constexpr (IS_DEBUG_BUILD)
            {
                // Check that all chunks of this array are referenced at most
                // once
                ChunkIdx anChunkIdx(node.numChunks.size());
                std::string chunkStr;
                for (uint64_t iChunk = 0; iChunk < totalNumChunks; ++iChunk)
                {
                    chunkStr = '[';
                    for (size_t iDim = 0; iDim < node.numChunks.size(); ++iDim)
                    {
                        if (iDim > 0)
                            chunkStr += ", ";
                        chunkStr += std::to_string(anChunkIdx[iDim]);
                    }
                    chunkStr += ']';

                    size_t counter = node.countManifestIdForChunk(anChunkIdx);
                    if (counter > 1)
                    {
                        CPLError(CE_Failure, CPLE_AppDefined,
                                 "%s: array %s: found more than one manifest "
                                 "ref for chunk %s",
                                 pszFilename, path.c_str(), chunkStr.c_str());
                        return nullptr;
                    }

                    if (iChunk + 1 < totalNumChunks)
                    {
                        // Increment anChunkIdx
                        for (size_t iDim = node.numChunks.size(); iDim > 0;
                             /* */)
                        {
                            --iDim;
                            ++anChunkIdx[iDim];
                            if (anChunkIdx[iDim] < node.numChunks[iDim])
                            {
                                break;
                            }
                            anChunkIdx[iDim] = 0;
                        }
                    }
                }
            }
        }

        snapshot->m_nodes.push_back(std::move(node));
    }

    if (needSort)
    {
        // Nominally not needed but there has been a confusion in the spec
        // regarding sort order, so normalize things
        // Cf https://github.com/earth-mover/icechunk/issues/2183
        std::sort(snapshot->m_nodes.begin(), snapshot->m_nodes.end(),
                  [](const Node &a, const Node &b) { return a.path < b.path; });
    }

    /* --------------------------------------------------------------------*/
    /* Parse manifest_files_v2 / manifest_files[] array                    */
    /* --------------------------------------------------------------------*/
    if (nVersion == 2)
    {
        const auto manifests = fbsSnapshot->manifest_files_v2();
        if (manifests)
        {
            snapshot->m_manifestInfos.reserve(manifests->size());
            for (const auto *manifestPtr : *manifests)
            {
                const auto manifestId = manifestPtr->id();
                if (!manifestId || !manifestId->bytes())
                {
                    CPLError(CE_Failure, CPLE_AppDefined,
                             "%s: missing manifest id", pszFilename);
                    return nullptr;
                }

                ManifestInfo info;
                info.sizeBytes = manifestPtr->size_bytes();
                info.numChunkRefs = manifestPtr->num_chunk_refs();
                static_assert(sizeof(*(manifestId->bytes())) ==
                              sizeof(info.id));
                memcpy(info.id.data(), manifestId->bytes()->data(),
                       sizeof(info.id));
                info.strId = CrockfordBase32Encode(info.id);

                CPLDebugOnly("Icechunk",
                             "snapshot %s, manifest %s, size_bytes %" PRIu64
                             ", num_chunk_refs %u",
                             snapshotIdBase32.c_str(), info.strId.c_str(),
                             info.sizeBytes, info.numChunkRefs);

                if (!snapshot->m_manifestInfos.empty() &&
                    info.id <= snapshot->m_manifestInfos.back().id)
                {
                    CPLError(
                        CE_Failure, CPLE_AppDefined,
                        "%s: ManifestInfo array not sorted by increasing id",
                        pszFilename);
                    return nullptr;
                }

                snapshot->m_manifestInfos.push_back(std::move(info));
            }
        }
    }
    else
    {
        const auto manifests = fbsSnapshot->manifest_files();
        CPLAssertAlways(manifests);  // guaranteed by VerifySnapshotBuffer()

        snapshot->m_manifestInfos.reserve(manifests->size());
        for (const auto *manifestPtr : *manifests)
        {
            const auto manifestId = manifestPtr->id();

            ManifestInfo info;
            info.sizeBytes = manifestPtr->size_bytes();
            info.numChunkRefs = manifestPtr->num_chunk_refs();
            static_assert(sizeof(*(manifestId.bytes())) == sizeof(info.id));
            memcpy(info.id.data(), manifestId.bytes()->data(), sizeof(info.id));
            info.strId = CrockfordBase32Encode(info.id);

            CPLDebugOnly("Icechunk",
                         "snapshot %s, manifest %s, size_bytes %" PRIu64
                         ", num_chunk_refs %u",
                         snapshotIdBase32.c_str(), info.strId.c_str(),
                         info.sizeBytes, info.numChunkRefs);

            if (!snapshot->m_manifestInfos.empty() &&
                info.id <= snapshot->m_manifestInfos.back().id)
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "%s: ManifestInfo array not sorted by increasing id",
                         pszFilename);
                return nullptr;
            }

            snapshot->m_manifestInfos.push_back(std::move(info));
        }
    }

    return snapshot;
}

#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif

/************************************************************************/
/*              IcechunkSnapshot::GetManifestInfoFromId()               */
/************************************************************************/

const IcechunkSnapshot::ManifestInfo *
IcechunkSnapshot::GetManifestInfoFromId(const ObjectId12 &id) const
{
#if __cplusplus >= 202002L
    const auto iter =
        std::ranges::lower_bound(m_manifestInfos, id, {}, &ManifestInfo::id);
#else
    ManifestInfo lookup;
    lookup.id = id;
    const auto iter =
        std::lower_bound(m_manifestInfos.begin(), m_manifestInfos.end(), lookup,
                         [](const ManifestInfo &a, const ManifestInfo &b)
                         { return a.id < b.id; });
#endif
    if (iter != m_manifestInfos.end() && iter->id == id)
        return &(*iter);
    return nullptr;
}

/************************************************************************/
/*                 IcechunkSnapshot::GetNodeFromPath()                  */
/************************************************************************/

const IcechunkSnapshot::Node *
IcechunkSnapshot::GetNodeFromPath(const std::string &path) const
{
#if __cplusplus >= 202002L
    const auto iter = std::ranges::lower_bound(m_nodes, path, {}, &Node::path);
#else
    Node lookup;
    lookup.path = path;
    const auto iter = std::lower_bound(m_nodes.begin(), m_nodes.end(), lookup,
                                       [](const Node &a, const Node &b)
                                       { return a.path < b.path; });
#endif
    if (iter != m_nodes.end() && iter->path == path)
        return &(*iter);
    return nullptr;
}

/************************************************************************/
/*                     DoRefExtentsMatchChunkIdx()                      */
/************************************************************************/

inline bool DoRefExtentsMatchChunkIdx(
    const std::vector<IcechunkSnapshot::ChunkIndexRange> &extents,
    const ChunkIdx &anChunkIdx)
{
    CPLAssert(anChunkIdx.size() == extents.size());
    for (size_t iDim = 0; iDim < extents.size(); ++iDim)
    {
        if (anChunkIdx[iDim] < extents[iDim].from ||
            anChunkIdx[iDim] >= extents[iDim].to)
        {
            return false;
        }
    }
    return true;
}

/************************************************************************/
/*              IcechunkSnapshot::findManifestIdForChunk()              */
/************************************************************************/

const ObjectId12 *
IcechunkSnapshot::Node::findManifestIdForChunk(const ChunkIdx &anChunkIdx) const
{
    CPLAssert(anChunkIdx.size() == numChunks.size());

    // Special case for scalar arrays such as "crs" written by Icechunk v0
    if (anChunkIdx.empty() && manifestRefs.size() == 1 &&
        manifestRefs[0].extents.size() == 1 &&
        manifestRefs[0].extents[0].from == 0 &&
        manifestRefs[0].extents[0].to == 1)
    {
        return &(manifestRefs[0].manifestId);
    }

    // Heuristics to find more quickly the chunk, assuming the passed chunk
    // index is contained in the last ChunkRef.
    thread_local const Node *lastNode = nullptr;
    thread_local size_t lastRefsIdx = 0;
    if (lastNode == this && lastRefsIdx < manifestRefs.size())
    {
        const auto &ref = manifestRefs[lastRefsIdx];
        const bool match = DoRefExtentsMatchChunkIdx(ref.extents, anChunkIdx);
        CPLDebugOnly("Icechunk", "findManifestIdForChunk() guess: %s",
                     match ? "success" : "missed");
        if (match)
        {
            return &(ref.manifestId);
        }
    }

    // Note: if there are many manifestRefs in a node, this linear search could
    // become a bottleneck. Consider RTree / KDTree maybe.
    for (const auto &ref : manifestRefs)
    {
        if (DoRefExtentsMatchChunkIdx(ref.extents, anChunkIdx))
        {
            lastNode = this;
            lastRefsIdx = &ref - manifestRefs.data();
            return &(ref.manifestId);
        }
    }

    return nullptr;
}

}  // namespace gdal::icechunk
