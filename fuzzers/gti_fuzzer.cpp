/******************************************************************************
 *
 * Project:  GDAL
 * Purpose:  Fuzzer for the GDAL Tile Index (GTI) driver
 * Author:   Even Rouault, even.rouault at spatialys.com
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

/* Threat model / rationale:
 * The GTI driver composes a mosaic out of an index file (.gti.gpkg = SQLite
 * container, .gti / .gti.fgb = XML / FlatGeobuf) plus fuzz-referenced tile
 * datasets. The attacker controls the index (layer schemas, tile entries,
 * extent/resolution/size metadata, relative paths) which drives MDF assembly
 * and warp sizing in frmts/gti/gdaltileindexdataset.cpp (~4.2k dark own-code
 * lines, 94% dark, reached incidentally by generic targets but never deep —
 * see manifest pair, continuation). This harness packs an index plus its
 * tile siblings into a FUZZER_FRIENDLY_ARCHIVE tar so relative tile paths
 * resolve inside /vsitar/, opens it as /vsitar//vsimem/test.tar/test.gti.gpkg
 * and exercises the raster API (checksums bounded to the output window,
 * metadata, overviews). Expected bug classes: memory safety in extent /
 * resolution / tile-entry handling, integer overflow in mosaic sizing. */

#include <stddef.h>
#include <stdint.h>

#include <algorithm>
#include <ctime>

#include "gdal.h"
#include "gdal_priv.h"
#include "gdal_frmts.h"
#include "gdal_alg.h"
#include "cpl_conv.h"
#include "cpl_string.h"
#include "cpl_vsi.h"
#include "ogr_api.h"
#include "ogrsf_frmts.h"

#define MEM_FILENAME "/vsimem/test.tar"
#define GDAL_FILENAME "/vsitar//vsimem/test.tar/test.gti.gpkg"

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
    CPLSetConfigOption("GDAL_CACHEMAX", "1000");  // Limit to 1 GB
    // GTI composes: the index container is GPKG, tiles are typically GTiff
    // and tile blobs may be PNG/JPEG/WEBP encoded; MEM is used internally.
    GDALRegister_GTI();
    RegisterOGRGeoPackage();
    GDALRegister_GTiff();
    GDALRegister_MEM();
    GDALRegister_PNG();
    GDALRegister_JPEG();
    GDALRegister_WEBP();

    return 0;
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *buf, size_t len)
{
    VSILFILE *fp = VSIFileFromMemBuffer(
        MEM_FILENAME, reinterpret_cast<GByte *>(const_cast<uint8_t *>(buf)),
        len, FALSE);
    VSIFCloseL(fp);

    CPLPushErrorHandler(CPLQuietErrorHandler);

    GDALDatasetH hDS = GDALOpenEx(GDAL_FILENAME,
                                  GDAL_OF_RASTER | GDAL_OF_VERBOSE_ERROR,
                                  nullptr, nullptr, nullptr);
    if (hDS)
    {
        const int nBands = GDALGetRasterCount(hDS);
        if (nBands > 0)
        {
            GDALRasterBandH hBand = GDALGetRasterBand(hDS, 1);
            const int nXSizeToRead =
                std::min(1024, GDALGetRasterXSize(hDS));
            const int nYSizeToRead =
                std::min(1024, GDALGetRasterYSize(hDS));
            GDALChecksumImage(hBand, 0, 0, nXSizeToRead, nYSizeToRead);
            int bFound = FALSE;
            GDALGetRasterNoDataValue(hBand, &bFound);
            GDALGetRasterOffset(hBand, &bFound);
            GDALGetRasterScale(hBand, &bFound);
            GDALGetMetadata(hBand, nullptr);
            int nOverviewCount = GDALGetOverviewCount(hBand);
            for (int i = 0; i < nOverviewCount; i++)
                GDALGetOverview(hBand, i);
        }
        GDALGetProjectionRef(hDS);
        double adfGeoTransform[6];
        GDALGetGeoTransform(hDS, adfGeoTransform);
        CSLDestroy(GDALGetFileList(hDS));
        GDALGetMetadata(hDS, nullptr);
        GDALClose(hDS);
    }

    CPLPopErrorHandler();
    VSIUnlink(MEM_FILENAME);
    return 0;
}
