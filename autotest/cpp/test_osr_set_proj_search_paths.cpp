/******************************************************************************
 * $Id$
 *
 * Project:  GDAL Core
 * Purpose:  Test OSRSetPROJSearchPaths()
 * Author:   Even Rouault, <even dot rouault at spatialys dot com>
 *
 ******************************************************************************
 * Copyright (c) 2019, Even Rouault <even dot rouault at spatialys dot com>
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

#include <stdlib.h>

#include "cpl_error.h"
#include "cpl_string.h"
#include "ogr_srs_api.h"
#include "cpl_multiproc.h"

#include "proj.h"

static void func1(void*)
{
    OGRSpatialReferenceH hSRS = OSRNewSpatialReference(nullptr);
    CPLPushErrorHandler(CPLQuietErrorHandler);
    auto ret = OSRImportFromEPSG(hSRS, 32631);
    CPLPopErrorHandler();
    if( ret == OGRERR_NONE )
    {
        fprintf(stderr, "failure expected (1)\n");
        exit(1);
    }
    OSRDestroySpatialReference(hSRS);
}

static void func2(void*)
{
    OGRSpatialReferenceH hSRS = OSRNewSpatialReference(nullptr);
    if( OSRImportFromEPSG(hSRS, 32631) != OGRERR_NONE )
    {
        fprintf(stderr, "failure not expected (2)\n");
        exit(1);
    }
    OSRDestroySpatialReference(hSRS);
}

static void func3(void*)
{
    OGRSpatialReferenceH hSRS = OSRNewSpatialReference(nullptr);
    if( OSRImportFromEPSG(hSRS, 32631) != OGRERR_NONE )
    {
        fprintf(stderr, "failure not expected (3)\n");
        exit(1);
    }

    // Test cleanup effect
    OSRCleanup();

    for(int epsg = 32601; epsg <= 32661; epsg++ )
    {
        if( OSRImportFromEPSG(hSRS, epsg) != OGRERR_NONE ||
            OSRImportFromEPSG(hSRS, epsg+100) != OGRERR_NONE )
        {
            fprintf(stderr, "failure not expected (4)\n");
            exit(1);
        }
    }
    OSRDestroySpatialReference(hSRS);
}

static void func4()
{
    // This test use auxiliary database created with proj 6.3.2
    // (tested up to 8.0.0) and can be sensitive to future
    // database structure change.
    //
    // See PR https://github.com/OSGeo/gdal/pull/3590
    const char* apszAux0[] = {"data/test_aux.db", nullptr};
    OSRSetPROJAuxDbPaths(apszAux0);

    char** papszAux1 = OSRGetPROJAuxDbPaths();
    if(papszAux1 == nullptr || papszAux1[0] == nullptr)
    {
        fprintf(stderr, "failure not expected (5)\n");
        exit(1);
    }
    if(!EQUAL(apszAux0[0], papszAux1[0]))
    {
        fprintf(stderr, "failure not expected (6)\n");
        exit(1);
    }
    OGRSpatialReferenceH hSRS = OSRNewSpatialReference(nullptr);
    if(OSRImportFromEPSG(hSRS, 4326) != OGRERR_NONE)
    {
        fprintf(stderr, "failure not expected (7)\n");
        exit(1); 
    }
    if(OSRImportFromEPSG(hSRS, 111111) != OGRERR_NONE)
    {
        fprintf(stderr, "failure not expected (8)\n");
        exit(1); 
    }
    OSRDestroySpatialReference(hSRS);
}

int main()
{
    auto tokens = OSRGetPROJSearchPaths();

    // Overriding PROJ_LIB
    setenv("PROJ_LIB", "/i_do/not_exist", true);

    // Test we can no longer find the database
    func1(nullptr);

    // In a thread as well
    auto t1 = CPLCreateJoinableThread(func1, nullptr);
    CPLJoinThread(t1);

    {
        const char* const apszDummyPaths[] = { "/i/am/dummy", nullptr };
        OSRSetPROJSearchPaths(apszDummyPaths);
        auto tokens2 = OSRGetPROJSearchPaths();
        if( strcmp(tokens2[0], "/i/am/dummy") != 0 )
        {
            fprintf(stderr, "failure not expected (5)\n");
            exit(1);
        }
        CSLDestroy(tokens2);
    }

    // Use OSRSetPROJSearchPaths to restore search paths
    OSRSetPROJSearchPaths(tokens);

    // This time this should work
    func2(nullptr);

    // In a thread as well
    auto t2 = CPLCreateJoinableThread(func2, nullptr);
    CPLJoinThread(t2);

    CSLDestroy(tokens);
    OSRCleanup();

    // Test fix for #2744
    CPLJoinableThread* ahThreads[4];
    for( int i = 0; i< 4; i++ )
    {
        ahThreads[i] = CPLCreateJoinableThread(func3, nullptr);
    }
    for( int i = 0; i< 4; i++ )
    {
        CPLJoinThread(ahThreads[i]);
    }
    
    func4();

    return 0;
}
