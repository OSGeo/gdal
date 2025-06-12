/******************************************************************************
 *
 * Project:  GDAL Core
 * Purpose:  Base class for thread safe dataset
 * Author:   Even Rouault <even dot rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2024, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#ifndef DOXYGEN_SKIP

#include "cpl_mem_cache.h"
#include "gdal_proxy.h"
#include "gdal_rat.h"
#include "gdal_priv.h"

#include <map>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

bool GDALThreadLocalDatasetCacheIsInDestruction();

/** Design notes of this file.
 *
 * This file is at the core of the "RFC 101 - Raster dataset read-only thread-safety".
 * Please consult it for high level understanding.
 *
 * 3 classes are involved:
 * - GDALThreadSafeDataset whose instances are returned to the user, and can
 *   use them in a thread-safe way.
 * - GDALThreadSafeRasterBand whose instances are created (and owned) by a
 *   GDALThreadSafeDataset instance, and returned to the user, which can use
 *   them in a thread-safe way.
 * - GDALThreadLocalDatasetCache which is an internal class, which holds the
 *   thread-local datasets.
 */

/************************************************************************/
/*                     GDALThreadLocalDatasetCache                      */
/************************************************************************/

class GDALThreadSafeDataset;

/** This class is instantiated once per thread that uses a
 * GDALThreadSafeDataset instance. It holds mostly a cache that maps a
 * GDALThreadSafeDataset* pointer to the corresponding per-thread dataset.
 */
class GDALThreadLocalDatasetCache
{
  private:
    /** Least-recently-used based cache that maps a GDALThreadSafeDataset*
     * instance to the corresponding per-thread dataset.
     * It should be noted as this a LRU cache, entries might get evicted when
     * its capacity is reached (64 datasets), which might be undesirable.
     * Hence it is doubled with m_oMapReferencedDS for datasets that are in
     * active used by a thread.
     *
     * This cache is created as a unique_ptr, and not a standard object, for
     * delicate reasons related to application termination, where we might
     * want to leak the memory associated to it, to avoid the dataset it
     * references from being closed, after GDAL has been "closed" (typically
     * GDALDestroyDriverManager() has been called), which would otherwise lead
     * to crashes.
     */
    std::unique_ptr<lru11::Cache<const GDALThreadSafeDataset *,
                                 std::shared_ptr<GDALDataset>>>
        m_poCache{};

    static thread_local bool tl_inDestruction;

    GDALThreadLocalDatasetCache(const GDALThreadLocalDatasetCache &) = delete;
    GDALThreadLocalDatasetCache &
    operator=(const GDALThreadLocalDatasetCache &) = delete;

  public:
    GDALThreadLocalDatasetCache();
    ~GDALThreadLocalDatasetCache();

    /** Thread-id of the thread that instantiated this object. Used only for
     * CPLDebug() purposes
     */
    GIntBig m_nThreadID = 0;

    /** Mutex that protects access to m_oCache. There is "competition" around
     * access to m_oCache since the destructor of a GDALThreadSafeDataset
     * instance needs to evict entries corresponding to itself from all
     * GDALThreadLocalDatasetCache instances.
     */
    std::mutex m_oMutex{};

    /** This is a reference to *(m_poCache.get()).
     */
    lru11::Cache<const GDALThreadSafeDataset *, std::shared_ptr<GDALDataset>>
        &m_oCache;

    /** Pair of shared_ptr<GDALDataset> with associated thread-local config
     * options that were valid in the calling thread at the time
     * GDALThreadLocalDatasetCache::RefUnderlyingDataset() was called, so they
     * can be restored at UnrefUnderlyingDataset() time.
     */
    struct SharedPtrDatasetThreadLocalConfigOptionsPair
    {
        std::shared_ptr<GDALDataset> poDS;
        CPLStringList aosTLConfigOptions;

        SharedPtrDatasetThreadLocalConfigOptionsPair(
            const std::shared_ptr<GDALDataset> &poDSIn,
            CPLStringList &&aosTLConfigOptionsIn)
            : poDS(poDSIn), aosTLConfigOptions(std::move(aosTLConfigOptionsIn))
        {
        }
    };

    /** Maps a GDALThreadSafeDataset*
     * instance to the corresponding per-thread dataset. Insertion into this
     * map is done by GDALThreadLocalDatasetCache::RefUnderlyingDataset() and
     * removal by UnrefUnderlyingDataset(). In most all use cases, the size of
     * this map should be 0 or 1 (not clear if it could be more, that would
     * involve RefUnderlyingDataset() being called in nested ways by the same
     * thread, but it doesn't hurt from being robust to that potential situation)
     */
    std::map<const GDALThreadSafeDataset *,
             SharedPtrDatasetThreadLocalConfigOptionsPair>
        m_oMapReferencedDS{};

    /** Maps a GDALRasterBand* returned by GDALThreadSafeRasterBand::RefUnderlyingDataset()
     * to the (thread-local) dataset that owns it (that is a dataset returned
     * by RefUnderlyingDataset(). The size of his map should be 0 or 1 in
     * most cases.
     */
    std::map<GDALRasterBand *, GDALDataset *> m_oMapReferencedDSFromBand{};

    static bool IsInDestruction()
    {
        return tl_inDestruction;
    }
};

/************************************************************************/
/*                      GDALThreadSafeDataset                           */
/************************************************************************/

/** Global variable used to determine if the singleton of GlobalCache
 * owned by GDALThreadSafeDataset is valid.
 * This is needed to avoid issues at process termination where the order
 * of destruction between static global instances and TLS instances can be
 * tricky.
 */
static bool bGlobalCacheValid = false;

/** Thread-safe GDALDataset class.
 *
 * That class delegates all calls to its members to per-thread GDALDataset
 * instances.
 */
class GDALThreadSafeDataset final : public GDALProxyDataset
{
  public:
    GDALThreadSafeDataset(std::unique_ptr<GDALDataset> poPrototypeDSUniquePtr,
                          GDALDataset *poPrototypeDS);
    ~GDALThreadSafeDataset() override;

    static std::unique_ptr<GDALDataset>
    Create(std::unique_ptr<GDALDataset> poPrototypeDS, int nScopeFlags);

    static GDALDataset *Create(GDALDataset *poPrototypeDS, int nScopeFlags);

    /* All below public methods override GDALDataset methods, and instead of
     * forwarding to a thread-local dataset, they act on the prototype dataset,
     * because they return a non-trivial type, that could be invalidated
     * otherwise if the thread-local dataset is evicted from the LRU cache.
     */
    const OGRSpatialReference *GetSpatialRef() const override
    {
        std::lock_guard oGuard(m_oPrototypeDSMutex);
        if (m_oSRS.IsEmpty())
        {
            auto poSRS = m_poPrototypeDS->GetSpatialRef();
            if (poSRS)
            {
                m_oSRS.AssignAndSetThreadSafe(*poSRS);
            }
        }
        return m_oSRS.IsEmpty() ? nullptr : &m_oSRS;
    }

    const OGRSpatialReference *GetGCPSpatialRef() const override
    {
        std::lock_guard oGuard(m_oPrototypeDSMutex);
        if (m_oGCPSRS.IsEmpty())
        {
            auto poSRS = m_poPrototypeDS->GetGCPSpatialRef();
            if (poSRS)
            {
                m_oGCPSRS.AssignAndSetThreadSafe(*poSRS);
            }
        }
        return m_oGCPSRS.IsEmpty() ? nullptr : &m_oGCPSRS;
    }

    const GDAL_GCP *GetGCPs() override
    {
        std::lock_guard oGuard(m_oPrototypeDSMutex);
        return const_cast<GDALDataset *>(m_poPrototypeDS)->GetGCPs();
    }

    const char *GetMetadataItem(const char *pszName,
                                const char *pszDomain = "") override
    {
        std::lock_guard oGuard(m_oPrototypeDSMutex);
        return const_cast<GDALDataset *>(m_poPrototypeDS)
            ->GetMetadataItem(pszName, pszDomain);
    }

    char **GetMetadata(const char *pszDomain = "") override
    {
        std::lock_guard oGuard(m_oPrototypeDSMutex);
        return const_cast<GDALDataset *>(m_poPrototypeDS)
            ->GetMetadata(pszDomain);
    }

    /* End of methods that forward on the prototype dataset */

    GDALAsyncReader *BeginAsyncReader(int, int, int, int, void *, int, int,
                                      GDALDataType, int, int *, int, int, int,
                                      char **) override
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "GDALThreadSafeDataset::BeginAsyncReader() not supported");
        return nullptr;
    }

  protected:
    GDALDataset *RefUnderlyingDataset() const override;

    void
    UnrefUnderlyingDataset(GDALDataset *poUnderlyingDataset) const override;

    int CloseDependentDatasets() override;

  private:
    friend class GDALThreadSafeRasterBand;
    friend class GDALThreadLocalDatasetCache;

    /** Mutex that protects accesses to m_poPrototypeDS */
    mutable std::mutex m_oPrototypeDSMutex{};

    /** "Prototype" dataset, that is the dataset that was passed to the
     * GDALThreadSafeDataset constructor. All calls on to it should be on
     * const methods, and should be protected by m_oPrototypeDSMutex (except
     * during GDALThreadSafeDataset instance construction)
     */
    const GDALDataset *m_poPrototypeDS = nullptr;

    /** Unique pointer for m_poPrototypeDS in the cases where GDALThreadSafeDataset
     * has been passed a unique pointer */
    std::unique_ptr<GDALDataset> m_poPrototypeDSUniquePtr{};

    /** Thread-local config options at the time where GDALThreadSafeDataset
     * has been constructed.
     */
    const CPLStringList m_aosThreadLocalConfigOptions{};

    /** Cached value returned by GetSpatialRef() */
    mutable OGRSpatialReference m_oSRS{};

    /** Cached value returned by GetGCPSpatialRef() */
    mutable OGRSpatialReference m_oGCPSRS{};

    /** Structure that references all GDALThreadLocalDatasetCache* instances.
     */
    struct GlobalCache
    {
        /** Mutex that protect access to oSetOfCache */
        std::mutex oMutex{};

        /** Set of GDALThreadLocalDatasetCache* instances. That is it has
         * one entry per thread that has used at least once a
         * GDALThreadLocalDatasetCache/GDALThreadSafeRasterBand
         */
        std::set<GDALThreadLocalDatasetCache *> oSetOfCache{};

        GlobalCache()
        {
            bGlobalCacheValid = true;
        }

        ~GlobalCache()
        {
            bGlobalCacheValid = false;
        }
    };

    /** Returns a singleton for a GlobalCache instance that references all
     * GDALThreadLocalDatasetCache* instances.
     */
    static GlobalCache &GetSetOfCache()
    {
        static GlobalCache cache;
        return cache;
    }

    /** Thread-local dataset cache. */
    static thread_local std::unique_ptr<GDALThreadLocalDatasetCache> tl_poCache;

    void UnrefUnderlyingDataset(GDALDataset *poUnderlyingDataset,
                                GDALThreadLocalDatasetCache *poCache) const;

    GDALThreadSafeDataset(const GDALThreadSafeDataset &) = delete;
    GDALThreadSafeDataset &operator=(const GDALThreadSafeDataset &) = delete;
};

/************************************************************************/
/*                     GDALThreadSafeRasterBand                         */
/************************************************************************/

/** Thread-safe GDALRasterBand class.
 *
 * That class delegates all calls to its members to per-thread GDALDataset
 * instances.
 */
class GDALThreadSafeRasterBand final : public GDALProxyRasterBand
{
  public:
    GDALThreadSafeRasterBand(GDALThreadSafeDataset *poTSDS,
                             GDALDataset *poParentDS, int nBandIn,
                             GDALRasterBand *poPrototypeBand,
                             int nBaseBandOfMaskBand, int nOvrIdx);

    GDALRasterBand *GetMaskBand() override;
    int GetOverviewCount() override;
    GDALRasterBand *GetOverview(int idx) override;
    GDALRasterBand *GetRasterSampleOverview(GUIntBig nDesiredSamples) override;

    GDALRasterAttributeTable *GetDefaultRAT() override;

    /* All below public methods override GDALRasterBand methods, and instead of
     * forwarding to a thread-local dataset, they act on the prototype band,
     * because they return a non-trivial type, that could be invalidated
     * otherwise if the thread-local dataset is evicted from the LRU cache.
     */
    const char *GetMetadataItem(const char *pszName,
                                const char *pszDomain = "") override
    {
        std::lock_guard oGuard(m_poTSDS->m_oPrototypeDSMutex);
        return const_cast<GDALRasterBand *>(m_poPrototypeBand)
            ->GetMetadataItem(pszName, pszDomain);
    }

    char **GetMetadata(const char *pszDomain = "") override
    {
        std::lock_guard oGuard(m_poTSDS->m_oPrototypeDSMutex);
        return const_cast<GDALRasterBand *>(m_poPrototypeBand)
            ->GetMetadata(pszDomain);
    }

    const char *GetUnitType() override
    {
        std::lock_guard oGuard(m_poTSDS->m_oPrototypeDSMutex);
        return const_cast<GDALRasterBand *>(m_poPrototypeBand)->GetUnitType();
    }

    GDALColorTable *GetColorTable() override
    {
        std::lock_guard oGuard(m_poTSDS->m_oPrototypeDSMutex);
        return const_cast<GDALRasterBand *>(m_poPrototypeBand)->GetColorTable();
    }

    /* End of methods that forward on the prototype band */

    CPLVirtualMem *GetVirtualMemAuto(GDALRWFlag, int *, GIntBig *,
                                     char **) override
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "GDALThreadSafeRasterBand::GetVirtualMemAuto() not supported");
        return nullptr;
    }

  protected:
    GDALRasterBand *RefUnderlyingRasterBand(bool bForceOpen) const override;
    void UnrefUnderlyingRasterBand(
        GDALRasterBand *poUnderlyingRasterBand) const override;

  private:
    /** Pointer to the thread-safe dataset from which this band has been
     *created */
    GDALThreadSafeDataset *m_poTSDS = nullptr;

    /** Pointer to the "prototype" raster band that corresponds to us.
     * All calls to m_poPrototypeBand should be protected by
     * GDALThreadSafeDataset:m_oPrototypeDSMutex.
     */
    const GDALRasterBand *m_poPrototypeBand = nullptr;

    /** 0 for standard bands, otherwise > 0 value that indicates that this
     * band is a mask band and m_nBaseBandOfMaskBand is then the number
     * of the band that is the parent of the mask band.
     */
    const int m_nBaseBandOfMaskBand;

    /** 0 for standard bands, otherwise >= 0 value that indicates that this
     * band is an overview band and m_nOvrIdx is then the index of the overview.
     */
    const int m_nOvrIdx;

    /** Mask band associated with this band. */
    std::unique_ptr<GDALRasterBand> m_poMaskBand{};

    /** List of overviews associated with this band. */
    std::vector<std::unique_ptr<GDALRasterBand>> m_apoOverviews{};

    GDALThreadSafeRasterBand(const GDALThreadSafeRasterBand &) = delete;
    GDALThreadSafeRasterBand &
    operator=(const GDALThreadSafeRasterBand &) = delete;
};

/************************************************************************/
/*                  Global variables initialization.                    */
/************************************************************************/

/** Instantiation of the TLS cache of datasets */
thread_local std::unique_ptr<GDALThreadLocalDatasetCache>
    GDALThreadSafeDataset::tl_poCache;

thread_local bool GDALThreadLocalDatasetCache::tl_inDestruction = false;

/************************************************************************/
/*                    GDALThreadLocalDatasetCache()                     */
/************************************************************************/

/** Constructor of GDALThreadLocalDatasetCache. This is called implicitly
 * when GDALThreadSafeDataset::tl_poCache is called the first time by a
 * thread.
 */
GDALThreadLocalDatasetCache::GDALThreadLocalDatasetCache()
    : m_poCache(std::make_unique<lru11::Cache<const GDALThreadSafeDataset *,
                                              std::shared_ptr<GDALDataset>>>()),
      m_nThreadID(CPLGetPID()), m_oCache(*m_poCache.get())
{
    CPLDebug("GDAL",
             "Registering thread-safe dataset cache for thread " CPL_FRMT_GIB,
             m_nThreadID);

    // We reference ourselves to the GDALThreadSafeDataset set-of-cache singleton
    auto &oSetOfCache = GDALThreadSafeDataset::GetSetOfCache();
    std::lock_guard oLock(oSetOfCache.oMutex);
    oSetOfCache.oSetOfCache.insert(this);
}

/************************************************************************/
/*                   ~GDALThreadLocalDatasetCache()                     */
/************************************************************************/

/** Destructor of GDALThreadLocalDatasetCache. This is called implicitly when a
 * thread is terminated.
 */
GDALThreadLocalDatasetCache::~GDALThreadLocalDatasetCache()
{
    tl_inDestruction = true;

    // If GDAL has been de-initialized explicitly (ie GDALDestroyDriverManager()
    // has been called), or we are during process termination, do not try to
    // free m_poCache at all, which would cause the datasets its owned to be
    // destroyed, which will generally lead to crashes in those situations where
    // GDAL has been de-initialized.
    const bool bDriverManagerDestroyed = *GDALGetphDMMutex() == nullptr;
    if (bDriverManagerDestroyed || !bGlobalCacheValid)
    {
        // Leak datasets when GDAL has been de-initialized
        if (!m_poCache->empty())
        {
            // coverity[leaked_storage]
            CPL_IGNORE_RET_VAL(m_poCache.release());
        }
        return;
    }

    // Unreference ourselves from the GDALThreadSafeDataset set-of-cache singleton
    CPLDebug("GDAL",
             "Unregistering thread-safe dataset cache for thread " CPL_FRMT_GIB,
             m_nThreadID);
    {
        auto &oSetOfCache = GDALThreadSafeDataset::GetSetOfCache();
        std::lock_guard oLock(oSetOfCache.oMutex);
        oSetOfCache.oSetOfCache.erase(this);
    }

    // Below code is just for debugging purposes and show which internal
    // thread-local datasets are released at thread termination.
    const auto lambda =
        [this](const lru11::KeyValuePair<const GDALThreadSafeDataset *,
                                         std::shared_ptr<GDALDataset>> &kv)
    {
        CPLDebug("GDAL",
                 "~GDALThreadLocalDatasetCache(): GDALClose(%s, this=%p) "
                 "for thread " CPL_FRMT_GIB,
                 kv.value->GetDescription(), kv.value.get(), m_nThreadID);
    };
    m_oCache.cwalk(lambda);
}

/************************************************************************/
/*                 GDALThreadLocalDatasetCacheIsInDestruction()         */
/************************************************************************/

bool GDALThreadLocalDatasetCacheIsInDestruction()
{
    return GDALThreadLocalDatasetCache::IsInDestruction();
}

/************************************************************************/
/*                     GDALThreadSafeDataset()                          */
/************************************************************************/

/** Constructor of GDALThreadSafeDataset.
 * It may be called with poPrototypeDSUniquePtr set to null, in the situations
 * where GDALThreadSafeDataset doesn't own the prototype dataset.
 * poPrototypeDS should always be not-null, and if poPrototypeDSUniquePtr is
 * not null, then poPrototypeDS should be equal to poPrototypeDSUniquePtr.get()
 */
GDALThreadSafeDataset::GDALThreadSafeDataset(
    std::unique_ptr<GDALDataset> poPrototypeDSUniquePtr,
    GDALDataset *poPrototypeDS)
    : m_poPrototypeDS(poPrototypeDS),
      m_aosThreadLocalConfigOptions(CPLGetThreadLocalConfigOptions())
{
    CPLAssert(poPrototypeDS != nullptr);
    if (poPrototypeDSUniquePtr)
    {
        CPLAssert(poPrototypeDS == poPrototypeDSUniquePtr.get());
    }

    // Replicate the characteristics of the prototype dataset onto ourselves
    nRasterXSize = poPrototypeDS->GetRasterXSize();
    nRasterYSize = poPrototypeDS->GetRasterYSize();
    for (int i = 1; i <= poPrototypeDS->GetRasterCount(); ++i)
    {
        SetBand(i, std::make_unique<GDALThreadSafeRasterBand>(
                       this, this, i, poPrototypeDS->GetRasterBand(i), 0, -1));
    }
    nOpenFlags = GDAL_OF_RASTER | GDAL_OF_THREAD_SAFE;
    SetDescription(poPrototypeDS->GetDescription());
    papszOpenOptions = CSLDuplicate(poPrototypeDS->GetOpenOptions());

    m_poPrototypeDSUniquePtr = std::move(poPrototypeDSUniquePtr);

    // In the case where we are constructed without owning the prototype
    // dataset, let's increase its reference counter though.
    if (!m_poPrototypeDSUniquePtr)
        const_cast<GDALDataset *>(m_poPrototypeDS)->Reference();
}

/************************************************************************/
/*                             Create()                                 */
/************************************************************************/

/** Utility method used by GDALGetThreadSafeDataset() to construct a
 * GDALThreadSafeDataset instance in the case where the GDALThreadSafeDataset
 * instance owns the prototype dataset.
 */

/* static */ std::unique_ptr<GDALDataset>
GDALThreadSafeDataset::Create(std::unique_ptr<GDALDataset> poPrototypeDS,
                              int nScopeFlags)
{
    if (nScopeFlags != GDAL_OF_RASTER)
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "GDALGetThreadSafeDataset(): Only nScopeFlags == "
                 "GDAL_OF_RASTER is supported");
        return nullptr;
    }
    if (poPrototypeDS->IsThreadSafe(nScopeFlags))
    {
        return poPrototypeDS;
    }
    if (!poPrototypeDS->CanBeCloned(nScopeFlags, /* bCanShareState = */ true))
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "GDALGetThreadSafeDataset(): Source dataset cannot be "
                 "cloned");
        return nullptr;
    }
    auto poPrototypeDSRaw = poPrototypeDS.get();
    return std::make_unique<GDALThreadSafeDataset>(std::move(poPrototypeDS),
                                                   poPrototypeDSRaw);
}

/************************************************************************/
/*                             Create()                                 */
/************************************************************************/

/** Utility method used by GDALGetThreadSafeDataset() to construct a
 * GDALThreadSafeDataset instance in the case where the GDALThreadSafeDataset
 * instance does not own the prototype dataset.
 */

/* static */ GDALDataset *
GDALThreadSafeDataset::Create(GDALDataset *poPrototypeDS, int nScopeFlags)
{
    if (nScopeFlags != GDAL_OF_RASTER)
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "GDALGetThreadSafeDataset(): Only nScopeFlags == "
                 "GDAL_OF_RASTER is supported");
        return nullptr;
    }
    if (poPrototypeDS->IsThreadSafe(nScopeFlags))
    {
        poPrototypeDS->Reference();
        return poPrototypeDS;
    }
    if (!poPrototypeDS->CanBeCloned(nScopeFlags, /* bCanShareState = */ true))
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "GDALGetThreadSafeDataset(): Source dataset cannot be "
                 "cloned");
        return nullptr;
    }
    return std::make_unique<GDALThreadSafeDataset>(nullptr, poPrototypeDS)
        .release();
}

/************************************************************************/
/*                    ~GDALThreadSafeDataset()                          */
/************************************************************************/

GDALThreadSafeDataset::~GDALThreadSafeDataset()
{
    // Collect TLS datasets in a vector, and free them after releasing
    // g_nInDestructorCounter to limit contention
    std::vector<std::pair<std::shared_ptr<GDALDataset>, GIntBig>> aoDSToFree;
    {
        auto &oSetOfCache = GetSetOfCache();
        std::lock_guard oLock(oSetOfCache.oMutex);
        for (auto *poCache : oSetOfCache.oSetOfCache)
        {
            std::unique_lock oLockCache(poCache->m_oMutex);
            std::shared_ptr<GDALDataset> poDS;
            if (poCache->m_oCache.tryGet(this, poDS))
            {
                aoDSToFree.emplace_back(std::move(poDS), poCache->m_nThreadID);
                poCache->m_oCache.remove(this);
            }
        }
    }

    for (const auto &oEntry : aoDSToFree)
    {
        CPLDebug("GDAL",
                 "~GDALThreadSafeDataset(): GDALClose(%s, this=%p) for "
                 "thread " CPL_FRMT_GIB,
                 GetDescription(), oEntry.first.get(), oEntry.second);
    }
    // Actually release TLS datasets
    aoDSToFree.clear();

    GDALThreadSafeDataset::CloseDependentDatasets();
}

/************************************************************************/
/*                      CloseDependentDatasets()                        */
/************************************************************************/

/** Implements GDALDataset::CloseDependentDatasets()
 *
 * Takes care of releasing the prototype dataset.
 *
 * As implied by the contract of CloseDependentDatasets(), returns true if
 * the prototype dataset has actually been released (or false if
 * CloseDependentDatasets() has already been closed)
 */
int GDALThreadSafeDataset::CloseDependentDatasets()
{
    int bRet = false;
    if (m_poPrototypeDSUniquePtr)
    {
        bRet = true;
    }
    else if (m_poPrototypeDS)
    {
        if (const_cast<GDALDataset *>(m_poPrototypeDS)->ReleaseRef())
        {
            bRet = true;
        }
    }

    m_poPrototypeDSUniquePtr.reset();
    m_poPrototypeDS = nullptr;

    return bRet;
}

/************************************************************************/
/*                       RefUnderlyingDataset()                         */
/************************************************************************/

/** Implements GDALProxyDataset::RefUnderlyingDataset.
 *
 * This method is called by all virtual methods of GDALDataset overridden by
 * RefUnderlyingDataset() when it delegates the calls to the underlying
 * dataset.
 *
 * Our implementation takes care of opening a thread-local dataset, on the
 * same underlying dataset of m_poPrototypeDS, if needed, and to insert it
 * into a cache for fast later uses by the same thread.
 */
GDALDataset *GDALThreadSafeDataset::RefUnderlyingDataset() const
{
    // Back-up thread-local config options at the time we are called
    CPLStringList aosTLConfigOptionsBackup(CPLGetThreadLocalConfigOptions());

    // Now merge the thread-local config options at the time where this
    // instance has been created with the current ones.
    const CPLStringList aosMerged(
        CSLMerge(CSLDuplicate(m_aosThreadLocalConfigOptions.List()),
                 aosTLConfigOptionsBackup.List()));

    // And make that merged list active
    CPLSetThreadLocalConfigOptions(aosMerged.List());

    std::shared_ptr<GDALDataset> poTLSDS;

    // Get the thread-local dataset cache for this thread.
    GDALThreadLocalDatasetCache *poCache = tl_poCache.get();
    if (!poCache)
    {
        auto poCacheUniquePtr = std::make_unique<GDALThreadLocalDatasetCache>();
        poCache = poCacheUniquePtr.get();
        tl_poCache = std::move(poCacheUniquePtr);
    }

    // Check if there's an entry in this cache for our current GDALThreadSafeDataset
    // instance.
    std::unique_lock oLock(poCache->m_oMutex);
    if (poCache->m_oCache.tryGet(this, poTLSDS))
    {
        // If so, return it, but before returning, make sure to creates a
        // "hard" reference to the thread-local dataset, in case it would
        // get evicted from poCache->m_oCache (by other threads that would
        // access lots of datasets in between)
        CPLAssert(!cpl::contains(poCache->m_oMapReferencedDS, this));
        auto poDSRet = poTLSDS.get();
        poCache->m_oMapReferencedDS.insert(
            {this, GDALThreadLocalDatasetCache::
                       SharedPtrDatasetThreadLocalConfigOptionsPair(
                           poTLSDS, std::move(aosTLConfigOptionsBackup))});
        return poDSRet;
    }

    // "Clone" the prototype dataset, which in 99% of the cases, involves
    // doing a GDALDataset::Open() call to re-open it. Do that by temporarily
    // dropping the lock that protects poCache->m_oCache.
    // coverity[uninit_use_in_call]
    oLock.unlock();
    poTLSDS = m_poPrototypeDS->Clone(GDAL_OF_RASTER, /* bCanShareState=*/true);
    if (poTLSDS)
    {
        CPLDebug("GDAL", "GDALOpen(%s, this=%p) for thread " CPL_FRMT_GIB,
                 GetDescription(), poTLSDS.get(), CPLGetPID());

        // Check that the re-openeded dataset has the same characteristics
        // as "this" / m_poPrototypeDS
        if (poTLSDS->GetRasterXSize() != nRasterXSize ||
            poTLSDS->GetRasterYSize() != nRasterYSize ||
            poTLSDS->GetRasterCount() != nBands)
        {
            poTLSDS.reset();
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Re-opened dataset for %s does not share the same "
                     "characteristics has the master dataset",
                     GetDescription());
        }
    }

    // Re-acquire the lok
    oLock.lock();

    // In case of failed closing, restore the thread-local config options that
    // were valid at the beginning of this method, and return in error.
    if (!poTLSDS)
    {
        CPLSetThreadLocalConfigOptions(aosTLConfigOptionsBackup.List());
        return nullptr;
    }

    // We have managed to get a thread-local dataset. Insert it into the
    // LRU cache and the m_oMapReferencedDS map that holds strong references.
    auto poDSRet = poTLSDS.get();
    {
        poCache->m_oCache.insert(this, poTLSDS);
        CPLAssert(!cpl::contains(poCache->m_oMapReferencedDS, this));
        poCache->m_oMapReferencedDS.insert(
            {this, GDALThreadLocalDatasetCache::
                       SharedPtrDatasetThreadLocalConfigOptionsPair(
                           poTLSDS, std::move(aosTLConfigOptionsBackup))});
    }
    return poDSRet;
}

/************************************************************************/
/*                      UnrefUnderlyingDataset()                        */
/************************************************************************/

/** Implements GDALProxyDataset::UnrefUnderlyingDataset.
 *
 * This is called by GDALProxyDataset overridden methods of GDALDataset, when
 * they no longer need to access the underlying dataset.
 *
 * This method actually delegates most of the work to the other
 * UnrefUnderlyingDataset() method that takes an explicit GDALThreadLocalDatasetCache*
 * instance.
 */
void GDALThreadSafeDataset::UnrefUnderlyingDataset(
    GDALDataset *poUnderlyingDataset) const
{
    GDALThreadLocalDatasetCache *poCache = tl_poCache.get();
    CPLAssert(poCache);
    std::unique_lock oLock(poCache->m_oMutex);
    UnrefUnderlyingDataset(poUnderlyingDataset, poCache);
}

/************************************************************************/
/*                      UnrefUnderlyingDataset()                        */
/************************************************************************/

/** Takes care of removing the strong reference to a thread-local dataset
 * from the TLS cache of datasets.
 */
void GDALThreadSafeDataset::UnrefUnderlyingDataset(
    [[maybe_unused]] GDALDataset *poUnderlyingDataset,
    GDALThreadLocalDatasetCache *poCache) const
{
    auto oIter = poCache->m_oMapReferencedDS.find(this);
    CPLAssert(oIter != poCache->m_oMapReferencedDS.end());
    CPLAssert(oIter->second.poDS.get() == poUnderlyingDataset);
    CPLSetThreadLocalConfigOptions(oIter->second.aosTLConfigOptions.List());
    poCache->m_oMapReferencedDS.erase(oIter);
}

/************************************************************************/
/*                      GDALThreadSafeRasterBand()                      */
/************************************************************************/

GDALThreadSafeRasterBand::GDALThreadSafeRasterBand(
    GDALThreadSafeDataset *poTSDS, GDALDataset *poParentDS, int nBandIn,
    GDALRasterBand *poPrototypeBand, int nBaseBandOfMaskBand, int nOvrIdx)
    : m_poTSDS(poTSDS), m_poPrototypeBand(poPrototypeBand),
      m_nBaseBandOfMaskBand(nBaseBandOfMaskBand), m_nOvrIdx(nOvrIdx)
{
    // Replicates characteristics of the prototype band.
    poDS = poParentDS;
    nBand = nBandIn;
    eDataType = poPrototypeBand->GetRasterDataType();
    nRasterXSize = poPrototypeBand->GetXSize();
    nRasterYSize = poPrototypeBand->GetYSize();
    poPrototypeBand->GetBlockSize(&nBlockXSize, &nBlockYSize);

    if (nBandIn > 0)
    {
        // For regular bands instantiates a (thread-safe) mask band and
        // as many overviews as needed.

        m_poMaskBand = std::make_unique<GDALThreadSafeRasterBand>(
            poTSDS, nullptr, 0, poPrototypeBand->GetMaskBand(), nBandIn,
            nOvrIdx);
        if (nOvrIdx < 0)
        {
            const int nOvrCount = poPrototypeBand->GetOverviewCount();
            for (int iOvrIdx = 0; iOvrIdx < nOvrCount; ++iOvrIdx)
            {
                m_apoOverviews.emplace_back(
                    std::make_unique<GDALThreadSafeRasterBand>(
                        poTSDS, nullptr, nBandIn,
                        poPrototypeBand->GetOverview(iOvrIdx),
                        nBaseBandOfMaskBand, iOvrIdx));
            }
        }
    }
    else if (nBaseBandOfMaskBand > 0)
    {
        // If we are a mask band, nstanciates a (thread-safe) mask band of
        // ourselves, but with the trick of negating nBaseBandOfMaskBand to
        // avoid infinite recursion...
        m_poMaskBand = std::make_unique<GDALThreadSafeRasterBand>(
            poTSDS, nullptr, 0, poPrototypeBand->GetMaskBand(),
            -nBaseBandOfMaskBand, nOvrIdx);
    }
}

/************************************************************************/
/*                      RefUnderlyingRasterBand()                       */
/************************************************************************/

/** Implements GDALProxyRasterBand::RefUnderlyingDataset.
 *
 * This method is called by all virtual methods of GDALRasterBand overridden by
 * RefUnderlyingRasterBand() when it delegates the calls to the underlying
 * band.
 */
GDALRasterBand *
GDALThreadSafeRasterBand::RefUnderlyingRasterBand(bool /*bForceOpen*/) const
{
    // Get a thread-local dataset
    auto poTLDS = m_poTSDS->RefUnderlyingDataset();
    if (!poTLDS)
        return nullptr;

    // Get corresponding thread-local band. If m_nBaseBandOfMaskBand is not
    // zero, then the base band is indicated into it, otherwise use nBand.
    const int nTLSBandIdx =
        m_nBaseBandOfMaskBand ? std::abs(m_nBaseBandOfMaskBand) : nBand;
    auto poTLRasterBand = poTLDS->GetRasterBand(nTLSBandIdx);
    if (!poTLRasterBand)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "GDALThreadSafeRasterBand::RefUnderlyingRasterBand(): "
                 "GetRasterBand(%d) failed",
                 nTLSBandIdx);
        m_poTSDS->UnrefUnderlyingDataset(poTLDS);
        return nullptr;
    }

    // Get the overview level if needed.
    if (m_nOvrIdx >= 0)
    {
        poTLRasterBand = poTLRasterBand->GetOverview(m_nOvrIdx);
        if (!poTLRasterBand)
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "GDALThreadSafeRasterBand::RefUnderlyingRasterBand(): "
                     "GetOverview(%d) failed",
                     m_nOvrIdx);
            m_poTSDS->UnrefUnderlyingDataset(poTLDS);
            return nullptr;
        }
    }

    // Get the mask band (or the mask band of the mask band) if needed.
    if (m_nBaseBandOfMaskBand)
    {
        poTLRasterBand = poTLRasterBand->GetMaskBand();
        if (m_nBaseBandOfMaskBand < 0)
            poTLRasterBand = poTLRasterBand->GetMaskBand();
    }

    // Check that the thread-local band characteristics are identical to the
    // ones of the prototype band.
    if (m_poPrototypeBand->GetXSize() != poTLRasterBand->GetXSize() ||
        m_poPrototypeBand->GetYSize() != poTLRasterBand->GetYSize() ||
        m_poPrototypeBand->GetRasterDataType() !=
            poTLRasterBand->GetRasterDataType()
        // m_poPrototypeBand->GetMaskFlags() is not thread-safe
        // || m_poPrototypeBand->GetMaskFlags() != poTLRasterBand->GetMaskFlags()
    )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "GDALThreadSafeRasterBand::RefUnderlyingRasterBand(): TLS "
                 "band has not expected characteristics");
        m_poTSDS->UnrefUnderlyingDataset(poTLDS);
        return nullptr;
    }
    int nThisBlockXSize;
    int nThisBlockYSize;
    int nTLSBlockXSize;
    int nTLSBlockYSize;
    m_poPrototypeBand->GetBlockSize(&nThisBlockXSize, &nThisBlockYSize);
    poTLRasterBand->GetBlockSize(&nTLSBlockXSize, &nTLSBlockYSize);
    if (nThisBlockXSize != nTLSBlockXSize || nThisBlockYSize != nTLSBlockYSize)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "GDALThreadSafeRasterBand::RefUnderlyingRasterBand(): TLS "
                 "band has not expected characteristics");
        m_poTSDS->UnrefUnderlyingDataset(poTLDS);
        return nullptr;
    }

    // Registers the association between the thread-local band and the
    // thread-local dataset
    {
        GDALThreadLocalDatasetCache *poCache =
            GDALThreadSafeDataset::tl_poCache.get();
        CPLAssert(poCache);
        std::unique_lock oLock(poCache->m_oMutex);
        CPLAssert(!cpl::contains(poCache->m_oMapReferencedDSFromBand,
                                 poTLRasterBand));
        poCache->m_oMapReferencedDSFromBand[poTLRasterBand] = poTLDS;
    }
    // CPLDebug("GDAL", "%p->RefUnderlyingRasterBand() return %p", this, poTLRasterBand);
    return poTLRasterBand;
}

/************************************************************************/
/*                      UnrefUnderlyingRasterBand()                     */
/************************************************************************/

/** Implements GDALProxyRasterBand::UnrefUnderlyingRasterBand.
 *
 * This is called by GDALProxyRasterBand overridden methods of GDALRasterBand,
 * when they no longer need to access the underlying dataset.
 */
void GDALThreadSafeRasterBand::UnrefUnderlyingRasterBand(
    GDALRasterBand *poUnderlyingRasterBand) const
{
    // CPLDebug("GDAL", "%p->UnrefUnderlyingRasterBand(%p)", this, poUnderlyingRasterBand);

    // Unregisters the association between the thread-local band and the
    // thread-local dataset
    {
        GDALThreadLocalDatasetCache *poCache =
            GDALThreadSafeDataset::tl_poCache.get();
        CPLAssert(poCache);
        std::unique_lock oLock(poCache->m_oMutex);
        auto oIter =
            poCache->m_oMapReferencedDSFromBand.find(poUnderlyingRasterBand);
        CPLAssert(oIter != poCache->m_oMapReferencedDSFromBand.end());

        m_poTSDS->UnrefUnderlyingDataset(oIter->second, poCache);
        poCache->m_oMapReferencedDSFromBand.erase(oIter);
    }
}

/************************************************************************/
/*                           GetMaskBand()                              */
/************************************************************************/

/** Implements GDALRasterBand::GetMaskBand
 */
GDALRasterBand *GDALThreadSafeRasterBand::GetMaskBand()
{
    return m_poMaskBand ? m_poMaskBand.get() : this;
}

/************************************************************************/
/*                         GetOverviewCount()                           */
/************************************************************************/

/** Implements GDALRasterBand::GetOverviewCount
 */
int GDALThreadSafeRasterBand::GetOverviewCount()
{
    return static_cast<int>(m_apoOverviews.size());
}

/************************************************************************/
/*                           GetOverview()                              */
/************************************************************************/

/** Implements GDALRasterBand::GetOverview
 */
GDALRasterBand *GDALThreadSafeRasterBand::GetOverview(int nIdx)
{
    if (nIdx < 0 || nIdx >= static_cast<int>(m_apoOverviews.size()))
        return nullptr;
    return m_apoOverviews[nIdx].get();
}

/************************************************************************/
/*                      GetRasterSampleOverview()                       */
/************************************************************************/

/** Implements GDALRasterBand::GetRasterSampleOverview
 */
GDALRasterBand *
GDALThreadSafeRasterBand::GetRasterSampleOverview(GUIntBig nDesiredSamples)

{
    // Call the base implementation, and do not forward to proxy
    return GDALRasterBand::GetRasterSampleOverview(nDesiredSamples);
}

/************************************************************************/
/*                             GetDefaultRAT()                          */
/************************************************************************/

/** Implements GDALRasterBand::GetDefaultRAT
 *
 * This is a bit tricky to do as GDALRasterAttributeTable has virtual methods
 * with potential (non thread-safe) side-effects. The clean solution would be
 * to implement a GDALThreadSafeRAT wrapper class, but this is a bit too much
 * effort. So for now, we check if the RAT returned by the prototype band is
 * an instance of GDALDefaultRasterAttributeTable. If it is, given that this
 * class has thread-safe getters, we can directly return it.
 * Otherwise return in error.
 */
GDALRasterAttributeTable *GDALThreadSafeRasterBand::GetDefaultRAT()
{
    std::lock_guard oGuard(m_poTSDS->m_oPrototypeDSMutex);
    const auto poRAT =
        const_cast<GDALRasterBand *>(m_poPrototypeBand)->GetDefaultRAT();
    if (!poRAT)
        return nullptr;

    if (dynamic_cast<GDALDefaultRasterAttributeTable *>(poRAT))
        return poRAT;

    CPLError(CE_Failure, CPLE_AppDefined,
             "GDALThreadSafeRasterBand::GetDefaultRAT() not supporting a "
             "non-GDALDefaultRasterAttributeTable implementation");
    return nullptr;
}

#endif  // DOXYGEN_SKIP

/************************************************************************/
/*                    GDALDataset::IsThreadSafe()                       */
/************************************************************************/

/** Return whether this dataset, and its related objects (typically raster
 * bands), can be called for the intended scope.
 *
 * Note that in the current implementation, nScopeFlags should be set to
 * GDAL_OF_RASTER, as thread-safety is limited to read-only operations and
 * excludes operations on vector layers (OGRLayer) or multidimensional API
 * (GDALGroup, GDALMDArray, etc.)
 *
 * This is the same as the C function GDALDatasetIsThreadSafe().
 *
 * @since 3.10
 */
bool GDALDataset::IsThreadSafe(int nScopeFlags) const
{
    return (nOpenFlags & GDAL_OF_THREAD_SAFE) != 0 &&
           nScopeFlags == GDAL_OF_RASTER && (nOpenFlags & GDAL_OF_RASTER) != 0;
}

/************************************************************************/
/*                     GDALDatasetIsThreadSafe()                        */
/************************************************************************/

/** Return whether this dataset, and its related objects (typically raster
 * bands), can be called for the intended scope.
 *
 * Note that in the current implementation, nScopeFlags should be set to
 * GDAL_OF_RASTER, as thread-safety is limited to read-only operations and
 * excludes operations on vector layers (OGRLayer) or multidimensional API
 * (GDALGroup, GDALMDArray, etc.)
 *
 * This is the same as the C++ method GDALDataset::IsThreadSafe().
 *
 * @param hDS Source dataset
 * @param nScopeFlags Intended scope of use.
 * Only GDAL_OF_RASTER is supported currently.
 * @param papszOptions Options. None currently.
 *
 * @since 3.10
 */
bool GDALDatasetIsThreadSafe(GDALDatasetH hDS, int nScopeFlags,
                             CSLConstList papszOptions)
{
    VALIDATE_POINTER1(hDS, __func__, false);

    CPL_IGNORE_RET_VAL(papszOptions);

    return GDALDataset::FromHandle(hDS)->IsThreadSafe(nScopeFlags);
}

/************************************************************************/
/*                       GDALGetThreadSafeDataset()                     */
/************************************************************************/

/** Return a thread-safe dataset.
 *
 * In the general case, this thread-safe dataset will open a
 * behind-the-scenes per-thread dataset (re-using the name and open options of poDS),
 * the first time a thread calls a method on the thread-safe dataset, and will
 * transparently redirect calls from the calling thread to this behind-the-scenes
 * per-thread dataset. Hence there is an initial setup cost per thread.
 * Datasets of the MEM driver cannot be opened by name, but this function will
 * take care of "cloning" them, using the same backing memory, when needed.
 *
 * Ownership of the passed dataset is transferred to the thread-safe dataset.
 *
 * The function may also return the passed dataset if it is already thread-safe.
 *
 * @param poDS Source dataset
 * @param nScopeFlags Intended scope of use.
 * Only GDAL_OF_RASTER is supported currently.
 *
 * @return a new thread-safe dataset, or nullptr in case of error.
 *
 * @since 3.10
 */
std::unique_ptr<GDALDataset>
GDALGetThreadSafeDataset(std::unique_ptr<GDALDataset> poDS, int nScopeFlags)
{
    return GDALThreadSafeDataset::Create(std::move(poDS), nScopeFlags);
}

/************************************************************************/
/*                       GDALGetThreadSafeDataset()                     */
/************************************************************************/

/** Return a thread-safe dataset.
 *
 * In the general case, this thread-safe dataset will open a
 * behind-the-scenes per-thread dataset (re-using the name and open options of poDS),
 * the first time a thread calls a method on the thread-safe dataset, and will
 * transparently redirect calls from the calling thread to this behind-the-scenes
 * per-thread dataset. Hence there is an initial setup cost per thread.
 * Datasets of the MEM driver cannot be opened by name, but this function will
 * take care of "cloning" them, using the same backing memory, when needed.
 *
 * The life-time of the passed dataset must be longer than the one of
 * the returned thread-safe dataset.
 *
 * Note that this function does increase the reference count on poDS while
 * it is being used
 *
 * The function may also return the passed dataset if it is already thread-safe.
 * A non-nullptr returned dataset must be released with ReleaseRef().
 *
 * Patterns like the following one are valid:
 * \code{.cpp}
 * auto poDS = GDALDataset::Open(...);
 * auto poThreadSafeDS = GDALGetThreadSafeDataset(poDS, GDAL_OF_RASTER | GDAL_OF_THREAD_SAFE);
 * poDS->ReleaseRef();
 * if (poThreadSafeDS )
 * {
 *     // ... do something with poThreadSafeDS ...
 *     poThreadSafeDS->ReleaseRef();
 * }
 * \endcode
 *
 * @param poDS Source dataset
 * @param nScopeFlags Intended scope of use.
 * Only GDAL_OF_RASTER is supported currently.
 *
 * @return a new thread-safe dataset or poDS, or nullptr in case of error.

 * @since 3.10
 */
GDALDataset *GDALGetThreadSafeDataset(GDALDataset *poDS, int nScopeFlags)
{
    return GDALThreadSafeDataset::Create(poDS, nScopeFlags);
}

/************************************************************************/
/*                       GDALGetThreadSafeDataset()                     */
/************************************************************************/

/** Return a thread-safe dataset.
 *
 * In the general case, this thread-safe dataset will open a
 * behind-the-scenes per-thread dataset (re-using the name and open options of hDS),
 * the first time a thread calls a method on the thread-safe dataset, and will
 * transparently redirect calls from the calling thread to this behind-the-scenes
 * per-thread dataset. Hence there is an initial setup cost per thread.
 * Datasets of the MEM driver cannot be opened by name, but this function will
 * take care of "cloning" them, using the same backing memory, when needed.
 *
 * The life-time of the passed dataset must be longer than the one of
 * the returned thread-safe dataset.
 *
 * Note that this function does increase the reference count on poDS while
 * it is being used
 *
 * The function may also return the passed dataset if it is already thread-safe.
 * A non-nullptr returned dataset must be released with GDALReleaseDataset().
 *
 * \code{.cpp}
 * hDS = GDALOpenEx(...);
 * hThreadSafeDS = GDALGetThreadSafeDataset(hDS, GDAL_OF_RASTER | GDAL_OF_THREAD_SAFE, NULL);
 * GDALReleaseDataset(hDS);
 * if( hThreadSafeDS )
 * {
 *     // ... do something with hThreadSafeDS ...
 *     GDALReleaseDataset(hThreadSafeDS);
 * }
 * \endcode
 *
 * @param hDS Source dataset
 * @param nScopeFlags Intended scope of use.
 * Only GDAL_OF_RASTER is supported currently.
 * @param papszOptions Options. None currently.
 *
 * @since 3.10
 */
GDALDatasetH GDALGetThreadSafeDataset(GDALDatasetH hDS, int nScopeFlags,
                                      CSLConstList papszOptions)
{
    VALIDATE_POINTER1(hDS, __func__, nullptr);

    CPL_IGNORE_RET_VAL(papszOptions);
    return GDALDataset::ToHandle(
        GDALGetThreadSafeDataset(GDALDataset::FromHandle(hDS), nScopeFlags));
}
