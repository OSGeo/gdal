/******************************************************************************
 *
 * Project:  GDAL
 * Purpose:  Test CPL_LOG
 * Author:   Even Rouault, even.rouault at spatialys.com
 *
 ******************************************************************************
 * Copyright (c) 2021, Even Rouault <even.rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include "cpl_conv.h"
#include "cpl_error.h"
#include "cpl_vsi.h"

#include "gtest_include.h"

namespace
{

// ---------------------------------------------------------------------------

TEST(testlog, test)
{
    const char *logname = "log_with_âccent.txt";
    CPLSetConfigOption("CPL_LOG", logname);
    CPLError(CE_Failure, CPLE_AppDefined, "test");
    VSILFILE *fp = VSIFOpenL(logname, "rb");
    char szGot[20 + 1];
    size_t nRead = VSIFReadL(szGot, 1, 20, fp);
    szGot[nRead] = 0;
    VSIFCloseL(fp);
    CPLCleanupErrorMutex();
    VSIUnlink(logname);

    EXPECT_TRUE(strstr(szGot, "ERROR 1") != nullptr) << szGot;
    EXPECT_TRUE(strstr(szGot, "test") != nullptr) << szGot;
}

}  // namespace
