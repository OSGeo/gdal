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

// TUT
#include <tut.h>
#include <tut_reporter.h>
#include <gdal_common.h>
// GDAL
#include <gdal.h>
#include <ogrsf_frmts.h>
#include <cpl_conv.h>
#include <cpl_multiproc.h>
#include <ogr_api.h>
// STD
#include <iostream>
#include <string>

namespace tut
{
    test_runner_singleton runner;

    // Common test data path
    // Customize these paths if you need.
    std::string const common::data_basedir("data");
    std::string const common::tmp_basedir("tmp");

} // namespace tut

int main(int argc, char* argv[])
{
    // Register GDAL/OGR drivers
    ::GDALAllRegister();
    ::OGRRegisterAll();

    GDALGeneralCmdLineProcessor( argc, &argv, 0 );

    // We don't actually use the arguments ourself.
    CSLDestroy( argv );
    argv = NULL;

    // Retrieve GDAL version
    std::string gdalVersion(::GDALVersionInfo("RELEASE_NAME"));

    std::cout << "C++ Test Suite for GDAL C/C++ API\n"
        << "----------------------------------------\n"
        << "GDAL library version: " << gdalVersion
        << "\nGDAL test data: " << tut::common::data_basedir
        << "\n----------------------------------------\n";

    // Initialize TUT framework
    tut::reporter visi;
    tut::runner.get().set_callback(&visi);

    bool bOk = false;
    try
    {
        bOk = tut::runner.get().run_tests();
    }
    catch( const std::exception& ex )
    {
        std::cerr << "TUT raised ex: " << ex.what() << std::endl;
    }

    int nRetCode = bOk ? 0 : 1;

    GDALDestroyDriverManager();
    OGRCleanupAll();

    CPLDumpSharedList( NULL );
    CPLCleanupTLS();

    return nRetCode;
}
