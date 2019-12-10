/******************************************************************************
 *
 * Project:  GDAL
 * Purpose:  Private header
 * Author:   Even Rouault <even dot rouault at spatialys dot com>
 *
 ******************************************************************************
 * Copyright (c) 2018, Even Rouault <even dot rouault at spatialys dot com>
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

#ifndef OGR_PROJ_P_H_INCLUDED
#define OGR_PROJ_P_H_INCLUDED

#include "proj.h"

#include "cpl_mem_cache.h"

#include <unordered_map>
#include <memory>
#include <utility>

/*! @cond Doxygen_Suppress */

PJ_CONTEXT* OSRGetProjTLSContext();
void OSRCleanupTLSContext();

class OSRProjTLSCache
{
        struct EPSGCacheKey
        {
            int nCode_;
            bool bUseNonDeprecated_;
            bool bAddTOWGS84_;

            EPSGCacheKey(int nCode, bool bUseNonDeprecated, bool bAddTOWGS84):
                nCode_(nCode), bUseNonDeprecated_(bUseNonDeprecated), bAddTOWGS84_(bAddTOWGS84) {}

            bool operator==(const EPSGCacheKey& other) const
            {
                return nCode_ == other.nCode_ &&
                       bUseNonDeprecated_ == other.bUseNonDeprecated_ &&
                       bAddTOWGS84_ == other.bAddTOWGS84_;
            }
        };
        struct EPSGCacheKeyHasher
        {
            std::size_t operator()(const EPSGCacheKey& k) const
            {
                return k.nCode_ |
                       ((k.bUseNonDeprecated_ ? 1 : 0) << 16) |
                       ((k.bAddTOWGS84_ ? 1 : 0) << 17);
            }
        };

        lru11::Cache<EPSGCacheKey, std::shared_ptr<PJ>,
                     lru11::NullLock,
                      std::unordered_map<
                        EPSGCacheKey,
                        typename std::list<lru11::KeyValuePair<EPSGCacheKey,
                            std::shared_ptr<PJ>>>::iterator,
                            EPSGCacheKeyHasher>> m_oCacheEPSG{};
        lru11::Cache<std::string, std::shared_ptr<PJ>> m_oCacheWKT{};

    public:
        OSRProjTLSCache() = default;

        void clear();

        PJ* GetPJForEPSGCode(int nCode, bool bUseNonDeprecated, bool bAddTOWGS84);
        void CachePJForEPSGCode(int nCode, bool bUseNonDeprecated, bool bAddTOWGS84, PJ* pj);

        PJ* GetPJForWKT(const std::string& wkt);
        void CachePJForWKT(const std::string& wkt, PJ* pj);
};

OSRProjTLSCache* OSRGetProjTLSCache();

/*! @endcond Doxygen_Suppress */

#endif