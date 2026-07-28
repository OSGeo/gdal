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

#ifndef ICECHUNKMANIFEST_H
#define ICECHUNKMANIFEST_H

#include "icechunkdefs.h"
#include "icechunkfile.h"

#include <map>
#include <string>
#include <memory>
#include <vector>

namespace gdal::icechunk
{
class IcechunkSnapshot;

/************************************************************************/
/*                           IcechunkManifest                           */
/************************************************************************/

/** Instance of https://icechunk.io/en/stable/reference/spec/#manifestfileinfo
 *
 * It is thread-safe.
 */
class IcechunkManifest final : public IcechunkFile
{
  public:
    IcechunkManifest();
    ~IcechunkManifest() override;

    struct ChunkRef
    {
        ChunkIdx idx{};

        std::vector<uint8_t> inlineContent{};

        // offset and length only apply to chunkId or location not empty
        uint64_t offset = 0;
        uint64_t length = 0;

        // In-repository chunk
        std::string chunkId{};

        // Virtual reference
        std::string location{};

        uint32_t checksumLastModified = 0;
        std::string checksumEtag{};
    };

    struct ArrayManifest
    {
        ObjectId8 nodeId{};

        // Sorted by increasing ChunkRef::idx
        std::vector<ChunkRef> chunkRefs{};
    };

    static std::unique_ptr<IcechunkManifest> Open(const char *pszFilename);

    const ChunkRef *GetChunkRef(const ObjectId8 &nodeId,
                                const ChunkIdx &idx) const;

    std::string GetChunkFilename(const std::string &chunkId) const;

    inline uint32_t GetChunkRefsCount() const
    {
        return m_chunkRefsCount;
    }

  private:
    // Sorted by increasing ArrayManifest::nodeId
    std::vector<ArrayManifest> m_arrayManifests{};

    // count(m_arrayManifests[].chunkRefs[])
    uint32_t m_chunkRefsCount = 0;
};

}  // namespace gdal::icechunk

#endif
