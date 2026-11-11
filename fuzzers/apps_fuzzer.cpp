/******************************************************************************
 *
 * Project:  GDAL
 * Purpose:  Fuzzer for the classic apps utility libraries (gdalinfo /
 *           ogrinfo / gdaldem / gdal_contour / gdal_grid / gdalbuildvrt /
 *           gdaltindex public entry points)
 * Author:   Even Rouault, even.rouault at spatialys.com
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

/* Threat model / rationale:
 * The apps utility libraries (apps/ogrinfo_lib.cpp, apps/gdaldem_lib.cpp,
 * apps/gdaltindex_lib.cpp, apps/gdalbuildvrt_lib.cpp) and the gridding /
 * contouring kernels (alg/gdalgrid.cpp, alg/contour.cpp) consume untrusted
 * datasets together with user-supplied option strings. They are compiled
 * into libgdal but no OSS-Fuzz target reached them at all (see manifest
 * pair 2). This harness drives their public entry points (GDALInfo /
 * GDALVectorInfo / GDALDEMProcessing / GDALContourGenerateEx /
 * GDALGridCreate / GDALBuildVRT / GDALTileIndex) over tar-packaged fuzz
 * datasets with fuzz-chosen options.
 * To keep the harness from turning against itself, numeric option values
 * are parsed from the fuzz input but clamped to sane ranges, and option
 * names must match a fixed allowlist table (combinatorial option-space
 * coverage without unbounded allocations); datasets come from the (small)
 * tar members. Expected bug classes: memory safety in option/arg handling,
 * grid/contour kernels on adversarial point sets, VRT/tile-index assembly
 * logic, info-rendering paths. */

#include <stddef.h>
#include <stdint.h>

#include <algorithm>
#include <cmath>
#include <vector>

#include "gdal.h"
#include "gdal_alg.h"
#include "gdal_priv.h"
#include "gdal_utils.h"
#include "gdalgrid.h"
#include "cpl_conv.h"
#include "cpl_string.h"
#include "cpl_vsi.h"
#include "ogr_api.h"
#include "ogrsf_frmts.h"

#define TAR_PATH "/vsimem/apps_fuzzer.tar"
#define CMD_PATH "/vsitar/" TAR_PATH "/apps_fuzzer_cmd.txt"
#define OUT_PATH "/vsimem/apps_fuzzer_out"

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
    CPLSetConfigOption("GDAL_PDF_RENDERING_OPTIONS", "RASTER,VECTOR");
    CPLSetConfigOption("GDAL_HTTP_TIMEOUT", "1");
    CPLSetConfigOption("GDAL_HTTP_CONNECTTIMEOUT", "1");
    CPLSetConfigOption("GDAL_CACHEMAX", "1000");  // Limit to 1 GB
    GDALAllRegister();

    return 0;
}

namespace
{

// Option-name allowlists (multi-byte tokens traceable to the apps sources).
const char *const g_papszInfoOpts[] = {
    "-json", "-mm", "-stats", "-approx_stats", "-hist", "-checksum",
    "-nofl", "-nomd", "-nogcp", "-noct", "-listmdd", "-mdd", "all",
    nullptr};

const char *const g_papszVecInfoOpts[] = {
    "-json", "-al", "-so", "-summary", "-fields", "ALL", "-geom", "YES",
    "-wkt_format", "WKT2_2019", nullptr};

const char *const g_papszDemNames[] = {
    "slope", "aspect", "hillshade", "roughness", "tpi", "tri", nullptr};

const char *const g_papszDemOpts[] = {
    "-s", "-z", "-computeEdges", "-alg", "Horn", "ZevenbergenThorne",
    "-b", "1", "-of", "GTiff", nullptr};

const char *const g_papszBuildVrtOpts[] = {
    "-resolution", "highest", "lowest", "average", "-separate", "-b", "1",
    "-addalpha", "-allow_src_difference", nullptr};

const char *const g_papszTileIndexOpts[] = {
    "-f", "Memory", "-lyr", "index", "-tileindex", "location",
    "-write_absolute_path", "-accept_different_schemas", nullptr};

const char *const g_papszGridAlgs[] = {
    "invdist", "invdistnn", "average", "nearest", "minimum", "maximum",
    "range", "count", "average_distance", "average_distance_pts", "linear",
    nullptr};

double ClampArg(const char *pszVal, double dfMin, double dfMax)
{
    double dfVal = CPLAtof(pszVal);
    if (!std::isfinite(dfVal))
        return dfMin;
    return std::min(dfMax, std::max(dfMin, dfVal));
}

bool IsAllowed(const char *const *papszTable, const char *pszToken)
{
    for (int i = 0; papszTable[i] != nullptr; i++)
    {
        if (strcmp(papszTable[i], pszToken) == 0)
            return true;
    }
    return false;
}

// Keep only allowlisted tokens; values following clamped-name options are
// clamped, everything else must be allowlisted verbatim.
char **FilterArgs(char **papszSrc, const char *const *papszTable,
                  const char *const *papszNumOpts)
{
    char **papszOut = nullptr;
    for (int i = 0; papszSrc && papszSrc[i]; i++)
    {
        const char *pszToken = papszSrc[i];
        const char *pszNext = papszSrc[i + 1];
        if (IsAllowed(papszTable, pszToken))
        {
            papszOut = CSLAddString(papszOut, pszToken);
        }
        else if (papszNumOpts != nullptr && IsAllowed(papszNumOpts, pszToken) &&
                 pszNext != nullptr)
        {
            CPLString osVal;
            const double dfClamped = ClampArg(pszNext, 0.001, 1000.0);
            osVal.Printf("%.6f", dfClamped);
            papszOut = CSLAddString(papszOut, pszToken);
            papszOut = CSLAddString(papszOut, osVal);
            i++;
        }
    }
    return papszOut;
}

}  // namespace

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *buf, size_t len)
{
    VSILFILE *fp = VSIFileFromMemBuffer(
        TAR_PATH, reinterpret_cast<GByte *>(const_cast<uint8_t *>(buf)), len,
        FALSE);
    VSIFCloseL(fp);

    CPLPushErrorHandler(CPLQuietErrorHandler);

    fp = VSIFOpenL(CMD_PATH, "rb");
    if (fp != nullptr)
    {
        const char *pszMode = CPLReadLineL(fp);
        if (pszMode != nullptr)
        {
            CPLString osMode(pszMode);
            // Raster-side modes operate on the in.tif member whose declared
            // dimensions are attacker-controlled (mutable header fields).
            // Gate them to bound CPU time of whole-raster algorithms (dem,
            // contour, info rendering, VRT/tile-index assembly).
            bool bRasterOK = false;
            {
                GDALDatasetH hProbe =
                    GDALOpenEx("/vsitar/" TAR_PATH "/in.tif", GDAL_OF_RASTER,
                               nullptr, nullptr, nullptr);
                if (hProbe)
                {
                    bRasterOK = GDALGetRasterXSize(hProbe) <= 4096 &&
                                GDALGetRasterYSize(hProbe) <= 4096;
                    GDALClose(hProbe);
                }
            }
            char **papszRawArgs = nullptr;
            const char *pszLine = nullptr;
            while ((pszLine = CPLReadLineL(fp)) != nullptr)
                papszRawArgs = CSLAddString(papszRawArgs, pszLine);

            if (osMode == "info" && bRasterOK)
            {
                GDALDatasetH hDS = GDALOpenEx(
                    "/vsitar/" TAR_PATH "/in.tif", GDAL_OF_RASTER | GDAL_OF_VERBOSE_ERROR,
                    nullptr, nullptr, nullptr);
                if (hDS)
                {
                    char **papszArgs =
                        FilterArgs(papszRawArgs, g_papszInfoOpts, nullptr);
                    GDALInfoOptions *psOptions =
                        GDALInfoOptionsNew(papszArgs, nullptr);
                    if (psOptions)
                        CPLFree(GDALInfo(hDS, psOptions));
                    GDALInfoOptionsFree(psOptions);
                    CSLDestroy(papszArgs);
                    GDALClose(hDS);
                }
            }
            else if (osMode == "vectorinfo")
            {
                GDALDatasetH hDS = GDALOpenEx(
                    "/vsitar/" TAR_PATH "/in.csv", GDAL_OF_VECTOR | GDAL_OF_VERBOSE_ERROR,
                    nullptr, nullptr, nullptr);
                if (hDS)
                {
                    char **papszArgs =
                        FilterArgs(papszRawArgs, g_papszVecInfoOpts, nullptr);
                    GDALVectorInfoOptions *psOptions =
                        GDALVectorInfoOptionsNew(papszArgs, nullptr);
                    if (psOptions)
                        CPLFree(GDALVectorInfo(hDS, psOptions));
                    GDALVectorInfoOptionsFree(psOptions);
                    CSLDestroy(papszArgs);
                    GDALClose(hDS);
                }
            }
            else if (osMode == "dem" && bRasterOK)
            {
                // first arg line: processing name; the rest: options
                if (CSLCount(papszRawArgs) >= 1 &&
                    IsAllowed(g_papszDemNames, papszRawArgs[0]))
                {
                    GDALDatasetH hDS = GDALOpenEx(
                        "/vsitar/" TAR_PATH "/in.tif",
                        GDAL_OF_RASTER | GDAL_OF_VERBOSE_ERROR, nullptr,
                        nullptr, nullptr);
                    if (hDS)
                    {
                        char **papszArgs = FilterArgs(papszRawArgs + 1,
                                                      g_papszDemOpts,
                                                      g_papszDemOpts);
                        GDALDEMProcessingOptions *psOptions =
                            GDALDEMProcessingOptionsNew(papszArgs, nullptr);
                        int bUsageError = FALSE;
                        GDALDatasetH hOutDS = GDALDEMProcessing(
                            OUT_PATH, hDS, papszRawArgs[0], nullptr, psOptions,
                            &bUsageError);
                        if (hOutDS)
                        {
                            GDALRasterBandH hBand =
                                GDALGetRasterBand(hOutDS, 1);
                            GDALChecksumImage(
                                hBand, 0, 0,
                                std::min(64, GDALGetRasterXSize(hOutDS)),
                                std::min(64, GDALGetRasterYSize(hOutDS)));
                            GDALClose(hOutDS);
                        }
                        GDALDEMProcessingOptionsFree(psOptions);
                        CSLDestroy(papszArgs);
                        GDALClose(hDS);
                    }
                }
            }
            else if (osMode == "contour" && bRasterOK)
            {
                // arg lines are LEVEL_* / *_FIELD style options passed to
                // GDALContourGenerateEx; numeric values clamped
                GDALDatasetH hDS = GDALOpenEx(
                    "/vsitar/" TAR_PATH "/in.tif", GDAL_OF_RASTER | GDAL_OF_VERBOSE_ERROR,
                    nullptr, nullptr, nullptr);
                if (hDS)
                {
                    GDALRasterBandH hBand = GDALGetRasterBand(hDS, 1);
                    GDALDriverH hMemDrv = GDALGetDriverByName("MEM");
                    GDALDatasetH hVecOut =
                        hMemDrv ? GDALCreate(hMemDrv, "contour_out", 0, 0, 0,
                                             GDT_Unknown, nullptr)
                                : nullptr;
                    OGRLayerH hLayer =
                        hVecOut ? GDALDatasetCreateLayer(hVecOut, "contours",
                                                         nullptr,
                                                         wkbLineString, nullptr)
                                : nullptr;
                    if (hLayer)
                    {
                        char **papszOpts = nullptr;
                        for (int i = 0; papszRawArgs && papszRawArgs[i]; i++)
                        {
                            const char *pszNext = papszRawArgs[i + 1];
                            if (pszNext == nullptr)
                                break;
                            if (EQUAL(papszRawArgs[i], "LEVEL_INTERVAL"))
                            {
                                papszOpts = CSLSetNameValue(
                                    papszOpts, "LEVEL_INTERVAL",
                                    CPLSPrintf("%.6f",
                                               ClampArg(pszNext, 1.0, 1e6)));
                                i++;
                            }
                            else if (EQUAL(papszRawArgs[i], "LEVEL_BASE"))
                            {
                                papszOpts = CSLSetNameValue(
                                    papszOpts, "LEVEL_BASE",
                                    CPLSPrintf("%.6f",
                                               ClampArg(pszNext, -1e6, 1e6)));
                                i++;
                            }
                            else if (EQUAL(papszRawArgs[i], "ID_FIELD") ||
                                     EQUAL(papszRawArgs[i], "ELEV_FIELD"))
                            {
                                // field names are fixed, not attacker
                                // identifiers
                                papszOpts = CSLSetNameValue(
                                    papszOpts, papszRawArgs[i],
                                    EQUAL(papszRawArgs[i], "ID_FIELD") ? "id"
                                                                       : "elev");
                                i++;
                            }
                        }
                        GDALContourGenerateEx(hBand, hLayer, papszOpts, nullptr,
                                              nullptr);
                        CSLDestroy(papszOpts);
                    }
                    if (hVecOut)
                        GDALClose(hVecOut);
                    GDALClose(hDS);
                }
            }
            else if (osMode == "grid")
            {
                // first arg line: algorithm name; points come from the
                // points.txt member ("x y z" per line, <= 64 points)
                if (CSLCount(papszRawArgs) >= 1 &&
                    IsAllowed(g_papszGridAlgs, papszRawArgs[0]))
                {
                    VSILFILE *fpPoints = VSIFOpenL("/vsitar/" TAR_PATH "/points.txt", "rb");
                    if (fpPoints)
                    {
                        std::vector<double> adfX, adfY, adfZ;
                        const char *pszPt = nullptr;
                        while (adfX.size() < 64 &&
                               (pszPt = CPLReadLineL(fpPoints)) != nullptr)
                        {
                            char **papszParts = CSLTokenizeString2(
                                pszPt, " ,", CSLT_HONOURSTRINGS);
                            if (CSLCount(papszParts) == 3)
                            {
                                adfX.push_back(ClampArg(papszParts[0], 0, 100));
                                adfY.push_back(ClampArg(papszParts[1], 0, 100));
                                adfZ.push_back(
                                    ClampArg(papszParts[2], -1000, 1000));
                            }
                            CSLDestroy(papszParts);
                        }
                        VSIFCloseL(fpPoints);
                        if (!adfX.empty())
                        {
                            GDALGridAlgorithm eAlg = GGA_InverseDistanceToAPower;
                            void *pOptions = nullptr;
                            if (GDALGridParseAlgorithmAndOptions(
                                    papszRawArgs[0], &eAlg, &pOptions) == CE_None)
                            {
                                const GUInt32 nX = 32;
                                const GUInt32 nY = 32;
                                std::vector<double> adfOut(nX * nY);
                                GDALGridCreate(
                                    eAlg, pOptions,
                                    static_cast<GUInt32>(adfX.size()),
                                    adfX.data(), adfY.data(), adfZ.data(), 0,
                                    100, 0, 100, nX, nY, GDT_Float64,
                                    adfOut.data(), nullptr, nullptr);
                                CPLFree(pOptions);
                            }
                        }
                    }
                }
            }
            else if ((osMode == "buildvrt" || osMode == "tileindex") && bRasterOK)
            {
                GDALDatasetH hSrc1 = GDALOpenEx(
                    "/vsitar/" TAR_PATH "/in.tif", GDAL_OF_RASTER | GDAL_OF_VERBOSE_ERROR,
                    nullptr, nullptr, nullptr);
                GDALDatasetH hSrc2 = GDALOpenEx("/vsitar/" TAR_PATH "/tile2.tif",
                                                GDAL_OF_RASTER, nullptr, nullptr,
                                                nullptr);
                if (hSrc1 && hSrc2)
                {
                    GDALDatasetH pahSrc[2] = {hSrc1, hSrc2};
                    const char *apszNames[2] = {"/vsitar/" TAR_PATH "/in.tif",
                                                "/vsitar/" TAR_PATH "/tile2.tif"};
                    char **papszArgs = FilterArgs(
                        papszRawArgs,
                        osMode == "buildvrt" ? g_papszBuildVrtOpts
                                             : g_papszTileIndexOpts,
                        nullptr);
                    int bUsageError = FALSE;
                    if (osMode == "buildvrt")
                    {
                        GDALBuildVRTOptions *psOptions =
                            GDALBuildVRTOptionsNew(papszArgs, nullptr);
                        GDALDatasetH hOut =
                            GDALBuildVRT(OUT_PATH, 2, pahSrc, apszNames,
                                         psOptions, &bUsageError);
                        if (hOut)
                            GDALClose(hOut);
                        GDALBuildVRTOptionsFree(psOptions);
                    }
                    else
                    {
                        GDALTileIndexOptions *psOptions =
                            GDALTileIndexOptionsNew(papszArgs, nullptr);
                        GDALDatasetH hOut =
                            GDALTileIndex(OUT_PATH, 2, apszNames, psOptions,
                                          &bUsageError);
                        if (hOut)
                            GDALClose(hOut);
                        GDALTileIndexOptionsFree(psOptions);
                    }
                    CSLDestroy(papszArgs);
                }
                if (hSrc1)
                    GDALClose(hSrc1);
                if (hSrc2)
                    GDALClose(hSrc2);
            }
            CSLDestroy(papszRawArgs);
        }
        VSIFCloseL(fp);
    }

    CPLPopErrorHandler();
    VSIRmdirRecursive("/vsimem/");
    return 0;
}
