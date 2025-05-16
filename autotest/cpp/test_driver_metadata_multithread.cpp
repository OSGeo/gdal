/******************************************************************************
 * Project:  GDAL Core
 * Purpose:  Test getting driver metadata concurrently from multiple threads.
 * Author:   Even Rouault <even dot rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2025, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include "gdal_priv.h"

#include "gtest_include.h"

#include <thread>

namespace
{

static bool test(int mainIter)
{
    bool ret = true;
    GDALAllRegister();

    const char *pszItem = (mainIter % 2) == 0
                              ? GDAL_DMD_CREATIONOPTIONLIST
                              : GDAL_DS_LAYER_CREATIONOPTIONLIST;

    auto poDM = GetGDALDriverManager();
    const int nDriverCount = poDM->GetDriverCount();
    for (int iDrv = 0; iDrv < nDriverCount; ++iDrv)
    {
        auto poDriver = poDM->GetDriver(iDrv);
        if (poDriver->pfnCreate || poDriver->pfnCreateCopy)
        {
            std::vector<std::thread> threads;
            std::vector<std::string> tabretval(4);
            for (size_t i = 0; i < tabretval.size(); ++i)
            {
                std::string &retval = tabretval[i];
                threads.emplace_back(
                    [pszItem, i, poDriver, &retval]()
                    {
                        const char *pszStr;
                        if ((i % 2) == 0)
                        {
                            pszStr = poDriver->GetMetadataItem(pszItem);
                        }
                        else
                        {
                            pszStr = CSLFetchNameValue(poDriver->GetMetadata(),
                                                       pszItem);
                        }
                        if (pszStr)
                            retval = pszStr;
                    });
            }
            for (size_t i = 0; i < tabretval.size(); ++i)
            {
                threads[i].join();
                if (i > 0 && tabretval[i] != tabretval[0])
                {
                    fprintf(stderr, "%s\n", poDriver->GetDescription());
                    ret = false;
                }
            }
        }
    }

    GDALDestroyDriverManager();
    return ret;
}

TEST(Test, test)
{
    for (int i = 0; i < 200; ++i)
    {
        ASSERT_TRUE(test(i));
    }
}

}  // namespace
