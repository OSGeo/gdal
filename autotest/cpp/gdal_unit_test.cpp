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
#endif // _MSC_VER

#include "gdal_unit_test.h"

#include "cpl_conv.h"
#include "cpl_multiproc.h"
#include "gdal.h"
#include "ogr_api.h"
#include "ogrsf_frmts.h"
#include "test_data.h"

#include <tut_reporter.hpp>

#include <iostream>
#include <string>

namespace tut
{
    test_runner_singleton runner;

    // Common test data path
    std::string const common::data_basedir(TUT_ROOT_DATA_DIR);
    std::string const common::tmp_basedir(TUT_ROOT_TMP_DIR);

    static void check_test_group(char const* name)
    {
        std::string grpname(name);
        if (grpname.empty())
            throw std::runtime_error("missing test group name");

        tut::groupnames gl = runner.get().list_groups();
        tut::groupnames::const_iterator found = std::find(gl.begin(), gl.end(), grpname);
        if (found == gl.end())
            throw std::runtime_error("test group " + grpname + " not found");
    }
} // namespace tut

int main(int argc, char* argv[])
{
    // Register GDAL/OGR drivers
    ::GDALAllRegister();
    ::OGRRegisterAll();

    std::cout
        << "GDAL C/C++ API tests"
        << " (" << ::GDALVersionInfo("--version") << ")"
        << "\n---------------------------------------------------------\n";

    argc = GDALGeneralCmdLineProcessor( argc, &argv, 0 );
    if (argc < 1)
    {
        std::cout
            << "\n---------------------------------------------------------\n"
            << "No tests to run\n";
        return EXIT_SUCCESS;
    }



    // Initialize TUT framework
    int nRetCode = EXIT_FAILURE;
    {
        tut::reporter visi;
        tut::runner.get().set_callback(&visi);

        try
        {
            if (argc == 1)
            {
                tut::runner.get().run_tests();
            }
            else if (argc == 2 && std::string(argv[1]) == "--list")
            {
                tut::groupnames gl = tut::runner.get().list_groups();
                tut::groupnames::const_iterator b = gl.begin();
                tut::groupnames::const_iterator e = gl.end();
                tut::groupnames::difference_type d = std::distance(b, e);
                std::cout << "Registered " << d << " test groups:\n" << std::endl;
                while (b != e)
                {
                    std::cout << "  " << *b << std::endl;
                    ++b;
                }
            }
            else if (argc == 2 && std::string(argv[1]) != "--list")
            {
                tut::check_test_group(argv[1]);
                tut::runner.get().run_tests(argv[1]);
            }
            else if (argc == 3)
            {
                tut::check_test_group(argv[1]);

                tut::test_result result;
                tut::runner.get().run_test(argv[1], std::atoi(argv[2]), result);
            }
            nRetCode = EXIT_SUCCESS;
        }
        catch (const std::exception& ex)
        {
            std::cerr << "GDAL C/C++ API tests error: " << ex.what() << std::endl;
            nRetCode = EXIT_FAILURE;
        }
        if( !visi.all_ok() )
            nRetCode = EXIT_FAILURE;
    }

    CSLDestroy(argv);
    GDALDestroyDriverManager();

    GDALAllRegister();
    GDALDestroyDriverManager();

    OGRCleanupAll();

    CPLDumpSharedList( nullptr );
    CPLCleanupTLS();

    return nRetCode;
}
