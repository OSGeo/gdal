/******************************************************************************
 *
 * Project:  GDAL
 * Purpose:  Fuzzer for GDALAlgorithm
 * Author:   Even Rouault <even dot rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2025, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include <stddef.h>
#include <stdint.h>

#include "gdal.h"
#include "gdalalgorithm.h"
#include "gdal_alg.h"
#include "gdal_priv.h"
#include "ogrsf_frmts.h"

#include "cpl_conv.h"
#include "cpl_vsi.h"

extern "C" int LLVMFuzzerInitialize(int *argc, char ***argv);
extern "C" int LLVMFuzzerTestOneInput(const uint8_t *buf, size_t len);

int LLVMFuzzerInitialize(int * /*argc*/, char ***argv)
{
    const char *exe_path = (*argv)[0];
    if (CPLGetConfigOption("GDAL_DATA", nullptr) == nullptr)
    {
        CPLSetConfigOption("GDAL_DATA", CPLGetPathSafe(exe_path).c_str());
    }
    CPLSetConfigOption("CPL_TMPDIR", "/tmp");
    CPLSetConfigOption("DISABLE_OPEN_REAL_NETCDF_FILES", "YES");
    // Disable PDF text rendering as fontconfig cannot access its config files
    CPLSetConfigOption("GDAL_PDF_RENDERING_OPTIONS", "RASTER,VECTOR");
    // to avoid timeout in WMS driver
    CPLSetConfigOption("GDAL_WMS_ABORT_CURL_REQUEST", "YES");
    CPLSetConfigOption("GDAL_HTTP_TIMEOUT", "1");
    CPLSetConfigOption("GDAL_HTTP_CONNECTTIMEOUT", "1");
    CPLSetConfigOption("GDAL_CACHEMAX", "1000");  // Limit to 1 GB

    GDALAllRegister();

    return 0;
}

int LLVMFuzzerTestOneInput(const uint8_t *buf, size_t len)
{
    VSILFILE *fp = VSIFileFromMemBuffer(
        "/vsimem/test.tar",
        reinterpret_cast<GByte *>(const_cast<uint8_t *>(buf)), len, FALSE);
    VSIFCloseL(fp);

    CPLPushErrorHandler(CPLQuietErrorHandler);

    auto &singleton = GDALGlobalAlgorithmRegistry::GetSingleton();
    std::unique_ptr<GDALAlgorithm> alg;

    fp = VSIFOpenL("/vsitar//vsimem/test.tar/cmd.txt", "rb");
    if (fp != nullptr)
    {
        const char *pszLine = nullptr;
        if ((pszLine = CPLReadLineL(fp)) != nullptr)
        {
            alg = singleton.Instantiate(pszLine);
        }
        if (alg)
        {
            while ((pszLine = CPLReadLineL(fp)) != nullptr)
            {
                auto subalg = alg->InstantiateSubAlgorithm(pszLine);
                if (subalg)
                    alg = std::move(subalg);
                else
                    break;
            }
        }
        if (alg && pszLine)
        {
            GDALAlgorithmArg *arg = nullptr;
            do
            {
                if (!arg)
                {
                    arg = alg->GetArg(pszLine);
                    if (!arg)
                        break;
                    if (arg->GetType() == GAAT_BOOLEAN)
                    {
                        arg->Set(true);
                        arg = nullptr;
                    }
                }
                else
                {
                    try
                    {
                        arg->Set(pszLine);
                    }
                    catch (const std::exception &)
                    {
                    }
                    arg = nullptr;
                }
            } while ((pszLine = CPLReadLineL(fp)) != nullptr);
        }
        if (alg)
        {
            alg->Run();

            auto outputArg = alg->GetArg("output");
            if (outputArg && outputArg->GetType() == GAAT_DATASET)
            {
                auto &val = outputArg->Get<GDALArgDatasetValue>();
                if (auto poDS = val.GetDatasetRef())
                {
                    if (poDS->GetRasterCount() > 0)
                    {
                        auto poBand = poDS->GetRasterBand(1);
                        int nXSizeToRead =
                            std::min(1024, poDS->GetRasterXSize());
                        int nYSizeToRead =
                            std::min(1024, poDS->GetRasterYSize());
                        GDALChecksumImage(poBand, 0, 0, nXSizeToRead,
                                          nYSizeToRead);
                    }

                    for (auto *poLayer : poDS->GetLayers())
                    {
                        for (auto &poFeat : *poLayer)
                        {
                        }
                    }
                }
            }

            alg->Finalize();
        }
        VSIFCloseL(fp);
    }

    VSIRmdirRecursive("/vsimem/");

    CPLPopErrorHandler();

    return 0;
}
