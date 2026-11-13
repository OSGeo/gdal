/******************************************************************************
 *
 * Project:  GDAL
 * Purpose:  Fuzzer for the GeoPackage driver (raster and vector sides)
 * Author:   Even Rouault, even.rouault at spatialys.com
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

/* Threat model / rationale:
 * GeoPackage is an SQLite container mixing vector tables, raster tile tables
 * and metadata tables. The attacker controls the whole SQLite file, so all
 * table/record parsing (gpkg_contents, gpkg_geometry_columns, tile lists,
 * WKB geometry blobs, tile PNG/JPEG/WEBP blobs), the RTree triggers and the
 * driver SQL are untrusted inputs. Before this harness the surface was only
 * reached incidentally by the generic gdal_fuzzer/ogr_fuzzer corpora (magic
 * gate "SQLite format 3" holds the mutator out); no dedicated target
 * existed, leaving ~15k own-code lines mostly dark (see manifest pair 3).
 * Expected bug classes: memory safety on blob/row parsing, SQL/logic
 * inconsistencies, pathological inputs. */

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
    // Tile blobs may be PNG/JPEG/WEBP encoded and temporary buffers use MEM
    RegisterOGRGeoPackage();
    GDALRegister_MEM();
    GDALRegister_PNG();
    GDALRegister_JPEG();
    GDALRegister_WEBP();

    return 0;
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *buf, size_t len)
{
    VSILFILE *fp = VSIFileFromMemBuffer(
        "/vsimem/test.gpkg", reinterpret_cast<GByte *>(const_cast<uint8_t *>(buf)),
        len, FALSE);
    VSIFCloseL(fp);

    CPLPushErrorHandler(CPLQuietErrorHandler);

    // Raster side
    GDALDatasetH hDS =
        GDALDataset::Open("/vsimem/test.gpkg", GDAL_OF_RASTER);
    if (hDS)
    {
        const int nBands = GDALGetRasterCount(hDS);
        time_t nStartTime = time(nullptr);
        if (nBands > 0)
        {
            GDALRasterBandH hBand = GDALGetRasterBand(hDS, 1);
            const int nXSizeToRead =
                std::min(1024, GDALGetRasterXSize(hDS));
            const int nYSizeToRead =
                std::min(1024, GDALGetRasterYSize(hDS));
            // Limit runtime to 20 seconds per band
            if (time(nullptr) - nStartTime < 20)
                GDALChecksumImage(hBand, 0, 0, nXSizeToRead, nYSizeToRead);
            int bFound = FALSE;
            GDALGetRasterNoDataValue(hBand, &bFound);
            GDALGetRasterOffset(hBand, &bFound);
            GDALGetRasterScale(hBand, &bFound);
            GDALGetMetadata(hBand, nullptr);
            int nFlags = GDALGetMaskFlags(hBand);
            GDALRasterBandH hMaskBand = GDALGetMaskBand(hBand);
            if (nFlags == GMF_PER_DATASET)
                GDALChecksumImage(hMaskBand, 0, 0, nXSizeToRead, nYSizeToRead);
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

    // Vector side
    OGRDataSourceH hVecDS = OGROpen("/vsimem/test.gpkg", FALSE, nullptr);
    if (hVecDS)
    {
        const int nLayers = OGR_DS_GetLayerCount(hVecDS);
        time_t nStartTime = time(nullptr);
        bool bStop = false;
        for (int i = 0; !bStop && i < 10 && i < nLayers; i++)
        {
            OGRLayerH hLayer = OGR_DS_GetLayer(hVecDS, i);
            OGR_L_GetSpatialRef(hLayer);
            OGR_L_GetGeomType(hLayer);
            OGR_L_GetFIDColumn(hLayer);
            OGR_L_GetGeometryColumn(hLayer);
            OGRFeatureH hFeature;
            OGRFeatureH hFeaturePrev = nullptr;
            for (int j = 0;
                 j < 1000 && !bStop &&
                 (hFeature = OGR_L_GetNextFeature(hLayer)) != nullptr;
                 j++)
            {
                // Limit runtime to 20 seconds if features returned are
                // different. Otherwise this may be a sign of a bug in the
                // reader and we want the infinite loop to be revealed.
                if (time(nullptr) - nStartTime > 20)
                {
                    bool bIsSameAsPrevious =
                        (hFeaturePrev != nullptr &&
                         OGR_F_Equal(hFeature, hFeaturePrev));
                    if (!bIsSameAsPrevious)
                    {
                        bStop = true;
                    }
                }
                if (hFeaturePrev)
                    OGR_F_Destroy(hFeaturePrev);
                hFeaturePrev = hFeature;
            }
            if (hFeaturePrev)
                OGR_F_Destroy(hFeaturePrev);
        }
        OGR_DS_Destroy(hVecDS);
    }

    CPLPopErrorHandler();
    VSIUnlink("/vsimem/test.gpkg");
    return 0;
}
