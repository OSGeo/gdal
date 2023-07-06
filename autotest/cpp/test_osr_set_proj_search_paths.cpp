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

#include "test_data.h"
#include "gtest_include.h"

namespace
{

// ---------------------------------------------------------------------------

static void func1(void *)
{
    OGRSpatialReferenceH hSRS = OSRNewSpatialReference(nullptr);
    CPLPushErrorHandler(CPLQuietErrorHandler);
    auto ret = OSRImportFromEPSG(hSRS, 32631);
    CPLPopErrorHandler();
    EXPECT_NE(ret, OGRERR_NONE);
    OSRDestroySpatialReference(hSRS);
}

static void func2(void *)
{
    OGRSpatialReferenceH hSRS = OSRNewSpatialReference(nullptr);
    EXPECT_EQ(OSRImportFromEPSG(hSRS, 32631), OGRERR_NONE);
    OSRDestroySpatialReference(hSRS);
}

TEST(test_osr_set_proj_search_paths, test)
{
    auto tokens = OSRGetPROJSearchPaths();

    // Overriding PROJ_LIB and PROJ_DATA
    static char szPROJ_LIB[] = "PROJ_LIB=/i_do/not_exist";
    putenv(szPROJ_LIB);
    static char szPROJ_DATA[] = "PROJ_DATA=/i_do/not_exist";
    putenv(szPROJ_DATA);

    // Test we can no longer find the database
    func1(nullptr);

    // In a thread as well
    auto t1 = CPLCreateJoinableThread(func1, nullptr);
    CPLJoinThread(t1);

    {
        const char *const apszDummyPaths[] = {"/i/am/dummy", nullptr};
        OSRSetPROJSearchPaths(apszDummyPaths);
        auto tokens2 = OSRGetPROJSearchPaths();
        EXPECT_STREQ(tokens2[0], "/i/am/dummy");
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
}

static void osr_cleanup_in_threads_thread_func(void *)
{
    OGRSpatialReferenceH hSRS = OSRNewSpatialReference(nullptr);
    EXPECT_EQ(OSRImportFromEPSG(hSRS, 32631), OGRERR_NONE);

    // Test cleanup effect
    OSRCleanup();

    for (int epsg = 32601; epsg <= 32661; epsg++)
    {
        EXPECT_EQ(OSRImportFromEPSG(hSRS, epsg), OGRERR_NONE);
        EXPECT_EQ(OSRImportFromEPSG(hSRS, epsg + 100), OGRERR_NONE);
    }
    OSRDestroySpatialReference(hSRS);
}

TEST(test_osr_set_proj_search_paths, osr_cleanup_in_threads)
{
    // Test fix for #2744
    CPLJoinableThread *ahThreads[4];
    for (int i = 0; i < 4; i++)
    {
        ahThreads[i] = CPLCreateJoinableThread(
            osr_cleanup_in_threads_thread_func, nullptr);
    }
    for (int i = 0; i < 4; i++)
    {
        CPLJoinThread(ahThreads[i]);
    }
}

TEST(test_osr_set_proj_search_paths, auxiliary_db)
{
    // This test use auxiliary database created with proj 6.3.2
    // (tested up to 8.0.0) and can be sensitive to future
    // database structure change.
    //
    // See PR https://github.com/OSGeo/gdal/pull/3590
    //
    // Starting with sqlite 3.41, and commit
    // https://github.com/sqlite/sqlite/commit/ed07d0ea765386c5bdf52891154c70f048046e60
    // we must use the same exact table definition in the auxiliary db, otherwise
    // SQLite3 is confused regarding column types. Hence this PROJ >= 9 check,
    // to use a table structure identical to proj.db of PROJ 9.
    int nPROJMajor = 0;
    OSRGetPROJVersion(&nPROJMajor, nullptr, nullptr);
    const char *apszAux0[] = {nPROJMajor >= 9
                                  ? TUT_ROOT_DATA_DIR "/test_aux_proj_9.db"
                                  : TUT_ROOT_DATA_DIR "/test_aux.db",
                              nullptr};
    OSRSetPROJAuxDbPaths(apszAux0);

    CPLStringList aosAux1(OSRGetPROJAuxDbPaths());
    ASSERT_EQ(aosAux1.size(), 1);
    ASSERT_STREQ(apszAux0[0], aosAux1[0]);
    OGRSpatialReferenceH hSRS = OSRNewSpatialReference(nullptr);
    EXPECT_EQ(OSRImportFromEPSG(hSRS, 4326), OGRERR_NONE);
    EXPECT_EQ(OSRImportFromEPSG(hSRS, 111111), OGRERR_NONE);
    OSRDestroySpatialReference(hSRS);
}

}  // namespace
