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
#include "cpl_string.h"

#include "ogr_proj_p.h"
#include "ogr_srs_api.h"

#include "proj.h"

#include <mutex>

/*! @cond Doxygen_Suppress */

static void osr_proj_logger(void * /* user_data */,
                            int level, const char * message)
{
    if( level == PJ_LOG_ERROR )
    {
        CPLError(CE_Failure, CPLE_AppDefined, "PROJ: %s", message);
    }
    else if( level == PJ_LOG_DEBUG )
    {
        CPLError(CE_Failure, CPLE_AppDefined, "PROJ: %s", message);
    }
}

static unsigned g_searchPathGenerationCounter = 0;
static std::mutex g_oSearchPathMutex;
static CPLStringList g_aosSearchpaths;

struct OSRPJContextHolder
{
    unsigned searchPathGenerationCounter = 0;
    PJ_CONTEXT* context = nullptr;

    OSRPJContextHolder();
    ~OSRPJContextHolder();

    void init();
    void deinit();

private:
    OSRPJContextHolder(const OSRPJContextHolder&) = delete;
    OSRPJContextHolder& operator=(const OSRPJContextHolder&) = delete;
};


OSRPJContextHolder::OSRPJContextHolder()
{
    init();
}

void OSRPJContextHolder::init()
{
    if( !context )
    {
        context = proj_context_create();
        proj_log_func (context, nullptr, osr_proj_logger);
    }
}


OSRPJContextHolder::~OSRPJContextHolder()
{
    deinit();
}

void OSRPJContextHolder::deinit()
{
    proj_context_destroy(context);
    context = nullptr;
}


static thread_local OSRPJContextHolder g_tls_projContext;

PJ_CONTEXT* OSRGetProjTLSContext()
{
    auto& l_projContext = g_tls_projContext;
    l_projContext.init();
    {
        // If OSRSetPROJSearchPaths() has been called since we created the mutex,
        // set the new search paths on the context.
        std::lock_guard<std::mutex> oLock(g_oSearchPathMutex);
        if( l_projContext.searchPathGenerationCounter !=
                                        g_searchPathGenerationCounter )
        {
            l_projContext.searchPathGenerationCounter =
                                            g_searchPathGenerationCounter;
            proj_context_set_search_paths(
                l_projContext.context,
                g_aosSearchpaths.Count(),
                g_aosSearchpaths.List());
        }
    }
    return l_projContext.context;
}

/************************************************************************/
/*                         OSRCleanupTLSContext()                       */
/************************************************************************/

void OSRCleanupTLSContext()
{
    g_tls_projContext.deinit();
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
void OSRSetPROJSearchPaths( const char* const * papszPaths )
{
    std::lock_guard<std::mutex> oLock(g_oSearchPathMutex);
    g_searchPathGenerationCounter ++;
    g_aosSearchpaths.Assign(CSLDuplicate(papszPaths), true);
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
void OSRGetPROJVersion( int* pnMajor, int* pnMinor, int* pnPatch )
{
    auto info = proj_info();
    if (pnMajor)
        *pnMajor = info.major;
    if (pnMinor)
        *pnMinor = info.minor;
    if (pnPatch)
        *pnPatch = info.patch;
}