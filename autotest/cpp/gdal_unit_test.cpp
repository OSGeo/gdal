///////////////////////////////////////////////////////////////////////////////
//
// Project:  C++ Test Suite for GDAL/OGR
// Purpose:  Main program of C++ Unit Tests runner for GDAL
// Author:   Mateusz Loskot <mateusz@loskot.net>
//
///////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2006, Mateusz Loskot <mateusz@loskot.net>
//
// This library is free software; you can redistribute it and/or
// modify it under the terms of the GNU Library General Public
// License as published by the Free Software Foundation; either
// version 2 of the License, or (at your option) any later version.
//
// This library is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
// Library General Public License for more details.
//
// You should have received a copy of the GNU Library General Public
// License along with this library; if not, write to the
// Free Software Foundation, Inc., 59 Temple Place - Suite 330,
// Boston, MA 02111-1307, USA.
///////////////////////////////////////////////////////////////////////////////

#ifdef _MSC_VER
#define WIN32_LEAN_AND_MEAN
#endif // _MSC_VER

#include <gdal_unit_test.h>

#include <cpl_conv.h>
#include <cpl_multiproc.h>
#include <gdal.h>
#include <ogr_api.h>
#include <ogrsf_frmts.h>

#include <tut_reporter.hpp>

#include <iostream>
#include <string>

namespace tut
{
    test_runner_singleton runner;

    // Common test data path
    // Customize these paths if you need.
    std::string const common::data_basedir("data");
    std::string const common::tmp_basedir("tmp");

    void check_test_group(char const* name)
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
    }

    CSLDestroy(argv);
    GDALDestroyDriverManager();
    OGRCleanupAll();

    CPLDumpSharedList( NULL );
    CPLCleanupTLS();

    return nRetCode;
}
