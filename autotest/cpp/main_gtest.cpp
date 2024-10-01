/******************************************************************************
 *
 * Project:  GDAL
 * Purpose:  gtest main
 * Author:   Even Rouault <even dot rouault at spatialys dot com>
 *
 ******************************************************************************
 * Copyright (c) 222, Even Rouault <even dot rouault at spatialys dot com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include <locale>

#include "gdal.h"
#include "cpl_string.h"

#include "gtest_include.h"

// So argc, argv can be used from test fixtures
int global_argc = 0;
char **global_argv = nullptr;

GTEST_API_ int main(int argc, char **argv)
{
    // Use a potentially non-C locale to make sure we are robust
    setlocale(LC_ALL, "");

    argc = GDALGeneralCmdLineProcessor(argc, &argv, 0);

    int nRetCode;
    try
    {
        testing::InitGoogleTest(&argc, argv);

        global_argc = argc;
        global_argv = argv;

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

    return nRetCode;
}
