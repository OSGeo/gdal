/******************************************************************************
 * $Id$
 *
 * Project:  GDAL
 * Purpose:  Test CPL_LOG
 * Author:   Even Rouault, even.rouault at spatialys.com
 *
 ******************************************************************************
 * Copyright (c) 2021, Even Rouault <even.rouault at spatialys.com>
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

#include "cpl_conv.h"
#include "cpl_error.h"
#include "cpl_vsi.h"

int main(int /* argc */, char* /* argv */[])
{
    const char* logname = "log_with_âccent.txt";
    CPLSetConfigOption("CPL_LOG", logname);
    CPLError(CE_Failure, CPLE_AppDefined, "test");
    VSILFILE* fp = VSIFOpenL(logname, "rb");
    char szExpected[20 + 1];
    size_t nRead = VSIFReadL(szExpected, 1, 20, fp);
    szExpected[nRead] = 0;
    VSIFCloseL(fp);
    CPLCleanupErrorMutex();
    VSIUnlink(logname);

    if( strncmp(szExpected, "ERROR 1: test", strlen("ERROR 1: test")) != 0 )
    {
        fprintf(stderr, "Did not get expected log: '%s'\n", szExpected);
    }

    return 0;
}
