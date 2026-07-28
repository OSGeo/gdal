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

#ifndef ICECHUNKSNAPSHOT_H
#define ICECHUNKSNAPSHOT_H

#include "icechunkdefs.h"
#include "icechunkfile.h"

#include "cpl_error.h"
#include "cpl_string.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace gdal::icechunk
{

/************************************************************************/
/*                           IcechunkSnapshot                           */
/************************************************************************/

/** Instance of https://icechunk.io/en/stable/reference/spec/#snapshot-files
 *
 * It is thread-safe.
 */
class IcechunkSnapshot final : public IcechunkFile
{
  public:
    IcechunkSnapshot();
    ~IcechunkSnapshot() override;

    struct ChunkIndexRange
    {
        const uint32_t from;  // inclusive
        const uint32_t to;    // exclusive

        inline ChunkIndexRange(uint32_t fromIn, uint32_t toIn)
            : from(fromIn), to(toIn)
        {
        }
    };

    struct ManifestRef
    {
        ObjectId12 manifestId{};
        // Pair [from, to[, along each array dimension
        std::vector<ChunkIndexRange> extents{};
    };

    struct Node
    {
        ObjectId8 id{};
        std::string path{};
        std::string content{};
        bool isArray = false;

        // Below for arrays only
        std::vector<uint32_t> numChunks{};  // along each dimension
        std::vector<ManifestRef> manifestRefs{};

        // Defined as a template to be stripped from non-DEBUG builds
        template <class T = void>
        size_t countManifestIdForChunk(const ChunkIdx &anChunkIdx) const
        {
            CPLAssert(anChunkIdx.size() == numChunks.size());

            size_t counter = 0;
            for (const auto &ref : manifestRefs)
            {
                bool match = true;
                for (size_t iDim = 0; iDim < numChunks.size(); ++iDim)
                {
                    if (anChunkIdx[iDim] < ref.extents[iDim].from ||
                        anChunkIdx[iDim] >= ref.extents[iDim].to)
                    {
                        match = false;
                        break;
                    }
                }
                if (match)
                {
                    ++counter;
                }
            }

            return counter;
        }

        const ObjectId12 *
        findManifestIdForChunk(const ChunkIdx &anChunkIdx) const;
    };

    struct ManifestInfo
    {
        ObjectId12 id{};
        std::string strId{};
        uint64_t sizeBytes = 0;
        uint32_t numChunkRefs = 0;
    };

    static std::unique_ptr<IcechunkSnapshot> Open(const char *pszFilename);

    inline const std::string &GetCommitMessage() const
    {
        return m_osCommitMessage;
    }

    /** Return the t ime at which this snapshot was committed, as non-leap
     * microseconds since Jan 1, 1970 UTC.
     */
    inline uint64_t GetFlushTimestamp() const
    {
        return m_flushTimestamp;
    }

    size_t GetNodeCount() const
    {
        return m_nodes.size();
    }

    const std::vector<Node> &GetNodes() const
    {
        return m_nodes;
    }

    const Node *GetNodeFromPath(const std::string &path) const;

    const ManifestInfo *GetManifestInfoFromId(const ObjectId12 &id) const;

  private:
    std::string m_osCommitMessage{};

    // Non-leap microseconds since Jan 1, 1970 UTC.
    uint64_t m_flushTimestamp = 0;

    // Elements are sorted according to increasing value of Node::path
    std::vector<Node> m_nodes{};

    // Elements are sorted according to increasing value of ManifestInfo::id
    std::vector<ManifestInfo> m_manifestInfos{};
};

}  // namespace gdal::icechunk

#endif
