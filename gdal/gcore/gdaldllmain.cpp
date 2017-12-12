/******************************************************************************
 *
 * Project:  GDAL Core
 * Purpose:  The library set-up/clean-up routines.
 * Author:   Mateusz Loskot <mateusz@loskot.net>
 *
 ******************************************************************************
 * Copyright (c) 2010, Mateusz Loskot <mateusz@loskot.net>
 * Copyright (c) 2013, Even Rouault <even dot rouault at mines-paris dot org>
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

#include "cpl_port.h"
#include "gdal.h"

#include "cpl_conv.h"
#include "cpl_error.h"
#include "cpl_multiproc.h"
#include "cpl_string.h"
#include "ogr_api.h"


static bool bInGDALGlobalDestructor = false;
extern "C" int CPL_DLL GDALIsInGlobalDestructor();

int GDALIsInGlobalDestructor()
{
    return bInGDALGlobalDestructor;
}

void CPLFinalizeTLS();

/************************************************************************/
/*                           GDALDestroy()                              */
/************************************************************************/

/** Finalize GDAL/OGR library.
 *
 * This function calls GDALDestroyDriverManager() and OGRCleanupAll() and
 * finalize Thread Local Storage variables.
 *
 * This function should *not* usually be explicitly called by application code
 * if GDAL is dynamically linked, since it is automatically called through
 * the unregistration mechanisms of dynamic library loading.
 *
 * Note: no GDAL/OGR code should be called after this call!
 *
 * @since GDAL 2.0
 */

static bool bGDALDestroyAlreadyCalled = FALSE;
void GDALDestroy(void)
{
    if( bGDALDestroyAlreadyCalled )
        return;
    bGDALDestroyAlreadyCalled = true;

    CPLDebug("GDAL", "In GDALDestroy - unloading GDAL shared library.");
    bInGDALGlobalDestructor = true;
    GDALDestroyDriverManager();

    OGRCleanupAll();
    bInGDALGlobalDestructor = false;

    /* See corresponding bug reports: */
    /* https://trac.osgeo.org/gdal/ticket/6139 */
    /* https://trac.osgeo.org/gdal/ticket/6868 */
    /* Needed in case no driver manager has been instantiated. */
    CPLFreeConfig();
    CPLFinalizeTLS();
    CPLCleanupErrorMutex();
    CPLCleanupMasterMutex();
}

/************************************************************************/
/*  The library set-up/clean-up routines implemented with               */
/*  GNU C/C++ extensions.                                               */
/*  TODO: Is it Linux-only solution or Unix portable?                   */
/************************************************************************/
#ifdef __GNUC__

static void GDALInitialize(void) __attribute__ ((constructor)) ;
static void GDALDestructor(void) __attribute__ ((destructor)) ;

/************************************************************************/
/* Called when GDAL is loaded by loader or by dlopen(),                 */
/* and before dlopen() returns.                                         */
/************************************************************************/

static void GDALInitialize(void)
{
    // nothing to do
    //CPLDebug("GDAL", "Library loaded");
#ifdef DEBUG
    const char* pszLocale = CPLGetConfigOption("GDAL_LOCALE", nullptr);
    if( pszLocale )
        CPLsetlocale( LC_ALL, pszLocale );
#endif
}

/************************************************************************/
/* Called when GDAL is unloaded by loader or by dlclose(),              */
/* and before dlclose() returns.                                        */
/************************************************************************/

static void GDALDestructor(void)
{
    if( bGDALDestroyAlreadyCalled )
        return;
    if( !CPLTestBool(CPLGetConfigOption("GDAL_DESTROY", "YES")) )
        return;
    GDALDestroy();
}

#endif // __GNUC__

/************************************************************************/
/*  The library set-up/clean-up routine implemented as DllMain entry    */
/*  point specific for Windows.                                         */
/************************************************************************/
#ifdef _MSC_VER

#include <windows.h>

extern "C" int WINAPI DllMain( HINSTANCE /* hInstance */,
                               DWORD dwReason,
                               LPVOID /* lpReserved */ )
{
    if (dwReason == DLL_PROCESS_ATTACH)
    {
        // nothing to do
    }
    else if (dwReason == DLL_THREAD_ATTACH)
    {
        // nothing to do
    }
    else if (dwReason == DLL_THREAD_DETACH)
    {
        ::CPLCleanupTLS();
    }
    else if (dwReason == DLL_PROCESS_DETACH)
    {
        GDALDestroy();
    }

    return 1; // ignored for all reasons but DLL_PROCESS_ATTACH
}

#endif // _MSC_VER
