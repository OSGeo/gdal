///////////////////////////////////////////////////////////////////////////////
//
// Project:  C++ Test Suite for GDAL/OGR
// Purpose:  Main program of C++ Unit Tests runner for GDAL
// Author:   Mateusz Loskot <mateusz@loskot.net>
//
///////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2006, Mateusz Loskot <mateusz@loskot.net>
/*
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

#ifdef _MSC_VER
#define WIN32_LEAN_AND_MEAN
#endif  // _MSC_VER

#include "gdal_unit_test.h"

#include "cpl_conv.h"
#include "cpl_multiproc.h"
#include "gdal.h"
#include "ogr_api.h"
#include "ogrsf_frmts.h"
#include "test_data.h"

#include <algorithm>
#include <iostream>
#include <string>

#include "gtest_include.h"

namespace tut
{
// Common test data path
std::string const common::data_basedir(TUT_ROOT_DATA_DIR);
std::string const common::tmp_basedir(TUT_ROOT_TMP_DIR);

::testing::AssertionResult
CheckEqualGeometries(OGRGeometryH lhs, OGRGeometryH rhs, double tolerance)
{
    // Test raw pointers
    if (nullptr == lhs)
    {
        return ::testing::AssertionFailure() << "lhs is null";
    }
    if (nullptr == rhs)
    {
        return ::testing::AssertionFailure() << "rhs is null";
    }

    // Test basic properties
    if (strcmp(OGR_G_GetGeometryName(lhs), OGR_G_GetGeometryName(rhs)) != 0)
    {
        return ::testing::AssertionFailure()
               << "OGR_G_GetGeometryName(lhs) = " << OGR_G_GetGeometryName(lhs)
               << ". OGR_G_GetGeometryName(rhs) = "
               << OGR_G_GetGeometryName(rhs);
    }

    if (OGR_G_GetGeometryCount(lhs) != OGR_G_GetGeometryCount(rhs))
    {
        return ::testing::AssertionFailure()
               << "OGR_G_GetGeometryCount(lhs) = "
               << OGR_G_GetGeometryCount(lhs)
               << ". OGR_G_GetGeometryCount(rhs) = "
               << OGR_G_GetGeometryCount(rhs);
    }

    if (OGR_G_GetPointCount(lhs) != OGR_G_GetPointCount(rhs))
    {
        return ::testing::AssertionFailure()
               << "OGR_G_GetPointCount(lhs) = " << OGR_G_GetPointCount(lhs)
               << ". OGR_G_GetPointCount(rhs) = " << OGR_G_GetPointCount(rhs);
    }

    if (OGR_G_GetGeometryCount(lhs) > 0)
    {
        // Test sub-geometries recursively
        const int count = OGR_G_GetGeometryCount(lhs);
        for (int i = 0; i < count; ++i)
        {
            auto res =
                CheckEqualGeometries(OGR_G_GetGeometryRef(lhs, i),
                                     OGR_G_GetGeometryRef(rhs, i), tolerance);
            if (!res)
                return res;
        }
    }
    else
    {
        std::unique_ptr<OGRGeometry> lhs_normalized_cpp;
        std::unique_ptr<OGRGeometry> rhs_normalized_cpp;
        OGRGeometryH lhs_normalized;
        OGRGeometryH rhs_normalized;
        if (OGRGeometryFactory::haveGEOS())
        {
            if (EQUAL(OGR_G_GetGeometryName(lhs), "LINEARRING"))
            {
                // Normalize() doesn't work with LinearRing
                OGRLineString lhs_as_ls(
                    *OGRGeometry::FromHandle(lhs)->toLineString());
                lhs_normalized_cpp.reset(lhs_as_ls.Normalize());
                OGRLineString rhs_as_ls(
                    *OGRGeometry::FromHandle(rhs)->toLineString());
                rhs_normalized_cpp.reset(rhs_as_ls.Normalize());
            }
            else
            {
                lhs_normalized_cpp.reset(
                    OGRGeometry::FromHandle(OGR_G_Normalize(lhs)));
                rhs_normalized_cpp.reset(
                    OGRGeometry::FromHandle(OGR_G_Normalize(rhs)));
            }
            lhs_normalized = OGRGeometry::ToHandle(lhs_normalized_cpp.get());
            rhs_normalized = OGRGeometry::ToHandle(rhs_normalized_cpp.get());
        }
        else
        {
            lhs_normalized = lhs;
            rhs_normalized = rhs;
        }

        // Test geometry points
        const std::size_t csize = 3;
        double a[csize] = {0};
        double b[csize] = {0};
        double d[csize] = {0};
        double dmax = 0;

        const int count = OGR_G_GetPointCount(lhs_normalized);
        for (int i = 0; i < count; ++i)
        {
            OGR_G_GetPoint(lhs_normalized, i, &a[0], &a[1], &a[2]);
            OGR_G_GetPoint(rhs_normalized, i, &b[0], &b[1], &b[2]);

            // Test vertices
            for (std::size_t c = 0; c < csize; ++c)
            {
                d[c] = std::fabs(a[c] - b[c]);
            }

            const double *pos = std::max_element(d, d + csize);
            dmax = *pos;

            if (dmax > tolerance)
            {
                return ::testing::AssertionFailure()
                       << "dmax = " << dmax << " is > tolerance = " << tolerance
                       << " on vertex " << i;
            }
        }
    }

    return ::testing::AssertionSuccess();
}

}  // namespace tut

int main(int argc, char *argv[])
{
#if defined(PROJ_GRIDS_PATH) && defined(PROJ_DB_TMPDIR)
    // Look for proj.db in PROJ search paths, copy it in PROJ_DB_TMPDIR, and restrict
    // PROJ search paths to PROJ_DB_TMPDIR and PROJ_GRIDS_PATH
    VSIMkdir(PROJ_DB_TMPDIR, 0755);
    static char szProjNetworkOff[] = "PROJ_NETWORK=OFF";
    putenv(szProjNetworkOff);
    const CPLStringList aosPathsOri(OSRGetPROJSearchPaths());
    bool bFoundProjDB = false;
    for (int i = 0; i < aosPathsOri.size(); ++i)
    {
        VSIStatBufL sStat;
        if (VSIStatL(CPLFormFilename(aosPathsOri[i], "proj.db", nullptr),
                     &sStat) == 0)
        {
            CPLCopyFile(CPLFormFilename(PROJ_DB_TMPDIR, "proj.db", nullptr),
                        CPLFormFilename(aosPathsOri[i], "proj.db", nullptr));
            bFoundProjDB = true;
            break;
        }
    }
    if (bFoundProjDB)
    {
        CPLStringList aosPaths;
        aosPaths.AddString(PROJ_DB_TMPDIR);
        aosPaths.AddString(PROJ_GRIDS_PATH);
        OSRSetPROJSearchPaths(aosPaths.List());
    }
#endif

    // Register GDAL/OGR drivers
    ::GDALAllRegister();
    ::OGRRegisterAll();

    std::cout
        << "GDAL C/C++ API tests"
        << " (" << ::GDALVersionInfo("--version") << ")"
        << "\n---------------------------------------------------------\n";

    argc = GDALGeneralCmdLineProcessor(argc, &argv, 0);

    int nRetCode;
    try
    {
        testing::InitGoogleTest(&argc, argv);

        nRetCode = RUN_ALL_TESTS();
    }
    catch (const std::exception &e)
    {
        nRetCode = 1;
        fprintf(stderr, "Caught exception %s\n", e.what());
    }
    catch (...)
    {
        nRetCode = 1;
        fprintf(stderr, "Caught exception of unknown type\n");
    }

    CSLDestroy(argv);
    GDALDestroyDriverManager();

    GDALAllRegister();
    GDALDestroyDriverManager();

    OGRCleanupAll();

    CPLDumpSharedList(nullptr);
    CPLCleanupTLS();

    return nRetCode;
}
