/******************************************************************************
 *
 * Project:  GDAL
 * Purpose:  PROJ-related functionality
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

#include "cpl_error.h"
#include "cpl_multiproc.h"
#include "cpl_string.h"

#include "ogr_proj_p.h"
#include "ogr_srs_api.h"

#include "proj.h"

#ifndef _WIN32
#include <sys/types.h>
#include <unistd.h>
#if defined(HAVE_PTHREAD_ATFORK)
#include <pthread.h>
#endif
#endif

#include <mutex>
#include <vector>

// #define SIMUL_OLD_PROJ6

/*! @cond Doxygen_Suppress */

static void osr_proj_logger(void * /* user_data */, int level,
                            const char *message)
{
    if (level == PJ_LOG_ERROR)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "PROJ: %s", message);
    }
    else if (level == PJ_LOG_DEBUG)
    {
        CPLDebug("PROJ", "%s", message);
    }
    else if (level == PJ_LOG_TRACE)
    {
        CPLDebug("PROJ_TRACE", "%s", message);
    }
}

static unsigned g_searchPathGenerationCounter = 0;
static unsigned g_auxDbPathsGenerationCounter = 0;
static std::mutex g_oSearchPathMutex;
static CPLStringList g_aosSearchpaths;
static CPLStringList g_aosAuxDbPaths;
#if PROJ_VERSION_MAJOR >= 7
static int g_projNetworkEnabled = -1;
static unsigned g_projNetworkEnabledGenerationCounter = 0;
#endif

#if !defined(_WIN32) && defined(HAVE_PTHREAD_ATFORK)
static bool g_bForkOccurred = false;
static void ForkOccurred(void)
{
    g_bForkOccurred = true;
}
#endif

struct OSRPJContextHolder
{
    unsigned searchPathGenerationCounter = 0;
    unsigned auxDbPathsGenerationCounter = 0;
#if PROJ_VERSION_MAJOR >= 7
    unsigned projNetworkEnabledGenerationCounter = 0;
#endif
    PJ_CONTEXT *context = nullptr;
    OSRProjTLSCache oCache;
#if !defined(_WIN32)
#if !defined(HAVE_PTHREAD_ATFORK)
    pid_t curpid = 0;
#endif
#if defined(SIMUL_OLD_PROJ6) ||                                                \
    !(PROJ_VERSION_MAJOR > 6 || PROJ_VERSION_MINOR >= 2)
    std::vector<PJ_CONTEXT *> oldcontexts{};
#endif
#endif

#if !defined(_WIN32)
    OSRPJContextHolder()
        : oCache(init())
#if !defined(HAVE_PTHREAD_ATFORK)
          ,
          curpid(getpid())
#endif
    {
#if HAVE_PTHREAD_ATFORK
        pthread_atfork(nullptr, nullptr, ForkOccurred);
#endif
        init();
    }
#else
    OSRPJContextHolder() : oCache(init())
    {
    }
#endif

    ~OSRPJContextHolder();

    PJ_CONTEXT *init();
    void deinit();

  private:
    OSRPJContextHolder(const OSRPJContextHolder &) = delete;
    OSRPJContextHolder &operator=(const OSRPJContextHolder &) = delete;
};

PJ_CONTEXT *OSRPJContextHolder::init()
{
    if (!context)
    {
        context = proj_context_create();
        proj_log_func(context, nullptr, osr_proj_logger);
    }
    return context;
}

OSRPJContextHolder::~OSRPJContextHolder()
{
    deinit();
}

void OSRPJContextHolder::deinit()
{
    searchPathGenerationCounter = 0;
    oCache.clear();

    // Destroy context in last
    proj_context_destroy(context);
    context = nullptr;
#if defined(SIMUL_OLD_PROJ6) ||                                                \
    (!defined(_WIN32) && !(PROJ_VERSION_MAJOR > 6 || PROJ_VERSION_MINOR >= 2))
    for (size_t i = 0; i < oldcontexts.size(); i++)
    {
        proj_context_destroy(oldcontexts[i]);
    }
    oldcontexts.clear();
#endif
}

#ifdef WIN32
// Currently thread_local and C++ objects don't work well with DLL on Windows
static void FreeProjTLSContextHolder(void *pData)
{
    delete static_cast<OSRPJContextHolder *>(pData);
}

static OSRPJContextHolder &GetProjTLSContextHolder()
{
    static OSRPJContextHolder dummy;
    int bMemoryErrorOccurred = false;
    void *pData = CPLGetTLSEx(CTLS_PROJCONTEXTHOLDER, &bMemoryErrorOccurred);
    if (bMemoryErrorOccurred)
    {
        return dummy;
    }
    if (pData == nullptr)
    {
        auto pHolder = new OSRPJContextHolder();
        CPLSetTLSWithFreeFuncEx(CTLS_PROJCONTEXTHOLDER, pHolder,
                                FreeProjTLSContextHolder,
                                &bMemoryErrorOccurred);
        if (bMemoryErrorOccurred)
        {
            delete pHolder;
            return dummy;
        }
        return *pHolder;
    }
    return *static_cast<OSRPJContextHolder *>(pData);
}
#else
static thread_local OSRPJContextHolder g_tls_projContext;
static OSRPJContextHolder &GetProjTLSContextHolder()
{
    OSRPJContextHolder &l_projContext = g_tls_projContext;

    // Detect if we are now running in a child process created by fork()
    // In that situation we must make sure *not* to use the same underlying
    // file open descriptor to the sqlite3 database, since seeks&reads in one
    // of the parent or child will affect the other end.
#if defined(HAVE_PTHREAD_ATFORK)
    if (g_bForkOccurred)
#else
    const pid_t curpid = getpid();
    if (curpid != l_projContext.curpid)
#endif
    {
#if defined(HAVE_PTHREAD_ATFORK)
        g_bForkOccurred = false;
#else
        l_projContext.curpid = curpid;
#endif
#if defined(SIMUL_OLD_PROJ6) ||                                                \
    !(PROJ_VERSION_MAJOR > 6 || PROJ_VERSION_MINOR >= 2)
        // PROJ < 6.2 ? Recreate new context
        l_projContext.oldcontexts.push_back(l_projContext.context);
        l_projContext.context = nullptr;
        l_projContext.init();
#else
        const auto osr_proj_logger_none = [](void *, int, const char *) {};
        proj_log_func(l_projContext.context, nullptr, osr_proj_logger_none);
        proj_context_set_autoclose_database(l_projContext.context, true);
        // dummy call to cause the database to be closed
        proj_context_get_database_path(l_projContext.context);
        proj_context_set_autoclose_database(l_projContext.context, false);
        proj_log_func(l_projContext.context, nullptr, osr_proj_logger);
#endif
    }

    return l_projContext;
}
#endif

PJ_CONTEXT *OSRGetProjTLSContext()
{
    auto &l_projContext = GetProjTLSContextHolder();
    // This .init() must be kept, even if OSRPJContextHolder constructor
    // calls it. The reason is that OSRCleanupTLSContext() calls deinit(),
    // so if reusing the object, we must re-init again.
    l_projContext.init();
    {
        // If OSRSetPROJSearchPaths() has been called since we created the
        // context, set the new search paths on the context.
        std::lock_guard<std::mutex> oLock(g_oSearchPathMutex);
        if (l_projContext.searchPathGenerationCounter !=
            g_searchPathGenerationCounter)
        {
            l_projContext.searchPathGenerationCounter =
                g_searchPathGenerationCounter;
            proj_context_set_search_paths(l_projContext.context,
                                          g_aosSearchpaths.Count(),
                                          g_aosSearchpaths.List());
        }
        if (l_projContext.auxDbPathsGenerationCounter !=
            g_auxDbPathsGenerationCounter)
        {
            l_projContext.auxDbPathsGenerationCounter =
                g_auxDbPathsGenerationCounter;
            std::string oMainPath(
                proj_context_get_database_path(l_projContext.context));
            proj_context_set_database_path(l_projContext.context,
                                           oMainPath.c_str(),
                                           g_aosAuxDbPaths.List(), nullptr);
        }
#if PROJ_VERSION_MAJOR >= 7
        if (l_projContext.projNetworkEnabledGenerationCounter !=
            g_projNetworkEnabledGenerationCounter)
        {
            l_projContext.projNetworkEnabledGenerationCounter =
                g_projNetworkEnabledGenerationCounter;
            proj_context_set_enable_network(l_projContext.context,
                                            g_projNetworkEnabled);
        }
#endif
    }
    return l_projContext.context;
}

/************************************************************************/
/*                         OSRGetProjTLSCache()                         */
/************************************************************************/

OSRProjTLSCache *OSRGetProjTLSCache()
{
    auto &l_projContext = GetProjTLSContextHolder();
    return &l_projContext.oCache;
}

void OSRProjTLSCache::clear()
{
    m_oCacheEPSG.clear();
    m_oCacheWKT.clear();
    m_tlsContext = nullptr;
}

PJ_CONTEXT *OSRProjTLSCache::GetPJContext()
{
    if (m_tlsContext == nullptr)
        m_tlsContext = OSRGetProjTLSContext();
    return m_tlsContext;
}

PJ *OSRProjTLSCache::GetPJForEPSGCode(int nCode, bool bUseNonDeprecated,
                                      bool bAddTOWGS84)
{
    const EPSGCacheKey key(nCode, bUseNonDeprecated, bAddTOWGS84);
    auto cached = m_oCacheEPSG.getPtr(key);
    if (cached)
    {
        return proj_clone(GetPJContext(), cached->get());
    }
    return nullptr;
}

void OSRProjTLSCache::CachePJForEPSGCode(int nCode, bool bUseNonDeprecated,
                                         bool bAddTOWGS84, PJ *pj)
{
    const EPSGCacheKey key(nCode, bUseNonDeprecated, bAddTOWGS84);
    m_oCacheEPSG.insert(key, UniquePtrPJ(proj_clone(GetPJContext(), pj)));
}

PJ *OSRProjTLSCache::GetPJForWKT(const std::string &wkt)
{
    auto cached = m_oCacheWKT.getPtr(wkt);
    if (cached)
    {
        return proj_clone(GetPJContext(), cached->get());
    }
    return nullptr;
}

void OSRProjTLSCache::CachePJForWKT(const std::string &wkt, PJ *pj)
{
    m_oCacheWKT.insert(wkt, UniquePtrPJ(proj_clone(GetPJContext(), pj)));
}

/************************************************************************/
/*                         OSRCleanupTLSContext()                       */
/************************************************************************/

void OSRCleanupTLSContext()
{
    GetProjTLSContextHolder().deinit();
}

/*! @endcond */

/************************************************************************/
/*                        OSRSetPROJSearchPaths()                       */
/************************************************************************/

/** \brief Set the search path(s) for PROJ resource files.
 *
 * @param papszPaths NULL terminated list of directory paths.
 * @since GDAL 3.0
 */
void OSRSetPROJSearchPaths(const char *const *papszPaths)
{
    std::lock_guard<std::mutex> oLock(g_oSearchPathMutex);
    g_searchPathGenerationCounter++;
    g_aosSearchpaths.Assign(CSLDuplicate(papszPaths), true);
}

/************************************************************************/
/*                        OSRGetPROJSearchPaths()                       */
/************************************************************************/

/** \brief Get the search path(s) for PROJ resource files.
 *
 * @return NULL terminated list of directory paths. To be freed with
 * CSLDestroy()
 * @since GDAL 3.0.3
 */
char **OSRGetPROJSearchPaths()
{
    std::lock_guard<std::mutex> oLock(g_oSearchPathMutex);
    if (g_searchPathGenerationCounter > 0 && !g_aosSearchpaths.empty())
    {
        return CSLDuplicate(g_aosSearchpaths.List());
    }

    const char *pszSep =
#ifdef _WIN32
        ";"
#else
        ":"
#endif
        ;
    return CSLTokenizeString2(proj_info().searchpath, pszSep, 0);
}

/************************************************************************/
/*                        OSRSetPROJAuxDbPaths()                        */
/************************************************************************/

/** \brief Set list of PROJ auxiliary database filenames.
 *
 * @param papszAux NULL-terminated list of auxiliary database filenames, or NULL
 * @since GDAL 3.3
 *
 * @see OSRGetPROJAuxDbPaths, proj_context_set_database_path
 */
void OSRSetPROJAuxDbPaths(const char *const *papszAux)
{
    std::lock_guard<std::mutex> oLock(g_oSearchPathMutex);
    g_auxDbPathsGenerationCounter++;
    g_aosAuxDbPaths.Assign(CSLDuplicate(papszAux), true);
}

/************************************************************************/
/*                        OSRGetPROJAuxDbPaths()                        */
/************************************************************************/

/** \brief Get PROJ auxiliary database filenames.
 *
 * @return NULL terminated list of PROJ auxiliary database filenames. To be
 * freed with CSLDestroy()
 * @since GDAL 3.3.0
 *
 * @see OSRSetPROJAuxDbPaths, proj_context_set_database_path
 */
char **OSRGetPROJAuxDbPaths(void)
{
    std::lock_guard<std::mutex> oLock(g_oSearchPathMutex);
    // Unfortunately, there is no getter for auxiliary database list at PROJ.
    // So, return our copy for now.
    return CSLDuplicate(g_aosAuxDbPaths.List());
}

/************************************************************************/
/*                       OSRSetPROJEnableNetwork()                      */
/************************************************************************/

/** \brief Enable or disable PROJ networking capabilities.
 *
 * @param enabled Set to TRUE to enable networking capabilities.
 * @since GDAL 3.4 and PROJ 7
 *
 * @see OSRGetPROJEnableNetwork, proj_context_set_enable_network
 */
void OSRSetPROJEnableNetwork(int enabled)
{
#if PROJ_VERSION_MAJOR >= 7
    std::lock_guard<std::mutex> oLock(g_oSearchPathMutex);
    if (g_projNetworkEnabled != enabled)
    {
        g_projNetworkEnabled = enabled;
        g_projNetworkEnabledGenerationCounter++;
    }
#else
    if (enabled)
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "OSRSetPROJEnableNetwork() requires PROJ >= 7");
    }
#endif
}

/************************************************************************/
/*                        OSRGetPROJEnableNetwork()                     */
/************************************************************************/

/** \brief Get whether PROJ networking capabilities are enabled.
 *
 * @return TRUE if PROJ networking capabilities are enabled.
 * @since GDAL 3.4 and PROJ 7
 *
 * @see OSRSetPROJEnableNetwork, proj_context_is_network_enabled
 */
int OSRGetPROJEnableNetwork(void)
{
#if PROJ_VERSION_MAJOR >= 7
    std::lock_guard<std::mutex> oLock(g_oSearchPathMutex);
    if (g_projNetworkEnabled < 0)
    {
        g_oSearchPathMutex.unlock();
        const int ret = proj_context_is_network_enabled(OSRGetProjTLSContext());
        g_oSearchPathMutex.lock();
        g_projNetworkEnabled = ret;
    }
    return g_projNetworkEnabled;
#else
    return FALSE;
#endif
}

/************************************************************************/
/*                         OSRGetPROJVersion()                          */
/************************************************************************/

/** \brief Get the PROJ version
 *
 * @param pnMajor Pointer to major version number, or NULL
 * @param pnMinor Pointer to minor version number, or NULL
 * @param pnPatch Pointer to patch version number, or NULL
 * @since GDAL 3.0.1
 */
void OSRGetPROJVersion(int *pnMajor, int *pnMinor, int *pnPatch)
{
    auto info = proj_info();
    if (pnMajor)
        *pnMajor = info.major;
    if (pnMinor)
        *pnMinor = info.minor;
    if (pnPatch)
        *pnPatch = info.patch;
}
