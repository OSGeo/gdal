/******************************************************************************
 * $Id$
 *
 * Project:  GDAL Core
 * Purpose:  The library set-up/clean-up routines.
 * Author:   Mateusz Loskot <mateusz@loskot.net>
 *
 ******************************************************************************
 * Copyright (c) 2010, Mateusz Loskot <mateusz@loskot.net>
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

#include "gdal.h"
#include "ogr_api.h"
#include "cpl_multiproc.h"
#include "cpl_conv.h"
#include "cpl_string.h"

extern "C" int CPL_DLL bInGDALGlobalDestructor;
int bInGDALGlobalDestructor = FALSE;

/************************************************************************/
/*  The library set-up/clean-up routines implemented with               */
/*  GNU C/C++ extensions.                                               */
/*  TODO: Is it Linux-only solution or Unix portable?                   */
/************************************************************************/
#ifdef __GNUC__

static void GDALInitialize(void) __attribute__ ((constructor)) ;
static void GDALDestroy(void)    __attribute__ ((destructor)) ;

/************************************************************************/
/* Called when GDAL is loaded by loader or by dlopen(),                 */
/* and before dlopen() returns.                                         */
/************************************************************************/

static void GDALInitialize(void)
{
    // nothing to do
    //CPLDebug("GDAL", "Library loaded");
}

/************************************************************************/
/* Called when GDAL is unloaded by loader or by dlclose(),              */
/* and before dlclose() returns.                                        */
/************************************************************************/

static void GDALDestroy(void)
{
    // TODO: Confirm if calling CPLCleanupTLS here is safe
    //CPLCleanupTLS();
    
    if( !CSLTestBoolean(CPLGetConfigOption("GDAL_DESTROY", "YES")) )
        return;

    CPLDebug("GDAL", "In GDALDestroy - unloading GDAL shared library.");
    bInGDALGlobalDestructor = TRUE;
    CPLSetConfigOption("GDAL_CLOSE_JP2ECW_RESOURCE", "NO");
    GDALDestroyDriverManager();
    CPLSetConfigOption("GDAL_CLOSE_JP2ECW_RESOURCE", NULL);

#ifdef OGR_ENABLED
    OGRCleanupAll();
#endif
    bInGDALGlobalDestructor = FALSE;
}

#endif // __GNUC__


/************************************************************************/
/*  The library set-up/clean-up routine implemented as DllMain entry    */
/*  point specific for Windows.                                         */
/************************************************************************/
#ifdef _MSC_VER

#include <windows.h>

extern "C" int WINAPI DllMain(HINSTANCE hInstance, DWORD dwReason, LPVOID lpReserved)
{
    UNREFERENCED_PARAMETER(hInstance);
    UNREFERENCED_PARAMETER(lpReserved);

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
        bInGDALGlobalDestructor = TRUE;
        ::GDALDestroyDriverManager();

#ifdef OGR_ENABLED
        ::OGRCleanupAll();
#endif
        bInGDALGlobalDestructor = FALSE;
    }

	return 1; // ignroed for all reasons but DLL_PROCESS_ATTACH
}

#endif // _MSC_VER

