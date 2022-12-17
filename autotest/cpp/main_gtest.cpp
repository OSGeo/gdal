/******************************************************************************
 *
 * Project:  GDAL
 * Purpose:  gtest main
 * Author:   Even Rouault <even dot rouault at spatialys dot com>
 *
 ******************************************************************************
 * Copyright (c) 222, Even Rouault <even dot rouault at spatialys dot com>
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
