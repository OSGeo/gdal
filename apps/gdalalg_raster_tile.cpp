/******************************************************************************
 *
 * Project:  GDAL
 * Purpose:  gdal "raster tile" subcommand
 * Author:   Even Rouault <even dot rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2025, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include "gdalalg_raster_tile.h"

#include "cpl_conv.h"
#include "cpl_json.h"
#include "cpl_mem_cache.h"
#include "cpl_spawn.h"
#include "cpl_time.h"
#include "cpl_vsi_virtual.h"
#include "cpl_worker_thread_pool.h"
#include "gdal_alg_priv.h"
#include "gdal_priv.h"
#include "gdalgetgdalpath.h"
#include "gdalwarper.h"
#include "gdal_utils.h"
#include "ogr_spatialref.h"
#include "memdataset.h"
#include "tilematrixset.hpp"
#include "ogr_p.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <cinttypes>
#include <cmath>
#include <mutex>
#include <utility>
#include <thread>

#ifdef USE_NEON_OPTIMIZATIONS
#include "include_sse2neon.h"
#elif defined(__x86_64) || defined(_M_X64)
#include <emmintrin.h>
#if defined(__SSSE3__) || defined(__AVX__)
#include <tmmintrin.h>
#endif
#if defined(__SSE4_1__) || defined(__AVX__)
#include <smmintrin.h>
#endif
#endif

#if defined(__x86_64) || defined(_M_X64) || defined(USE_NEON_OPTIMIZATIONS)
#define USE_PAETH_SSE2
#endif

#ifndef _WIN32
#define FORK_ALLOWED
#endif

#include "cpl_zlib_header.h"  // for crc32()

//! @cond Doxygen_Suppress

#ifndef _
#define _(x) (x)
#endif

// Unlikely substring to appear in stdout. We do that in case some GDAL
// driver would output on stdout.
constexpr const char PROGRESS_MARKER[] = {'!', '.', 'x'};
constexpr const char END_MARKER[] = {'?', 'E', '?', 'N', '?', 'D', '?'};

constexpr const char ERROR_START_MARKER[] = {'%', 'E', '%', 'R', '%', 'R',
                                             '%', '_', '%', 'S', '%', 'T',
                                             '%', 'A', '%', 'R', '%', 'T'};

constexpr const char *STOP_MARKER = "STOP\n";

namespace
{
struct BandMetadata
{
    std::string osDescription{};
    GDALDataType eDT{};
    GDALColorInterp eColorInterp{};
    std::string osCenterWaveLength{};
    std::string osFWHM{};
};
}  // namespace

/************************************************************************/
/*                     GetThresholdMinTilesPerJob()                     */
/************************************************************************/

static int GetThresholdMinThreadsForSpawn()
{
    // Minimum number of threads for automatic switch to spawning
    constexpr int THRESHOLD_MIN_THREADS_FOR_SPAWN = 8;

    // Config option for test only
    return std::max(1, atoi(CPLGetConfigOption(
                           "GDAL_THRESHOLD_MIN_THREADS_FOR_SPAWN",
                           CPLSPrintf("%d", THRESHOLD_MIN_THREADS_FOR_SPAWN))));
}

/************************************************************************/
/*                     GetThresholdMinTilesPerJob()                     */
/************************************************************************/

static int GetThresholdMinTilesPerJob()
{
    // Minimum number of tiles per job to decide for automatic switch to spawning
    constexpr int THRESHOLD_TILES_PER_JOB = 100;

    // Config option for test only
    return std::max(
        1, atoi(CPLGetConfigOption("GDAL_THRESHOLD_MIN_TILES_PER_JOB",
                                   CPLSPrintf("%d", THRESHOLD_TILES_PER_JOB))));
}

/************************************************************************/
/*          GDALRasterTileAlgorithm::GDALRasterTileAlgorithm()          */
/************************************************************************/

GDALRasterTileAlgorithm::GDALRasterTileAlgorithm(bool standaloneStep)
    : GDALRasterPipelineStepAlgorithm(NAME, DESCRIPTION, HELP_URL,
                                      ConstructorOptions()
                                          .SetStandaloneStep(standaloneStep)
                                          .SetInputDatasetMaxCount(1)
                                          .SetAddDefaultArguments(false)
                                          .SetInputDatasetAlias("dataset"))
{
    AddProgressArg();
    AddArg("spawned", 0, _("Whether this is a spawned worker"),
           &m_spawned)
        .SetHidden();  // Used in spawn mode
#ifdef FORK_ALLOWED
    AddArg("forked", 0, _("Whether this is a forked worker"),
           &m_forked)
        .SetHidden();  // Used in forked mode
#else
    CPL_IGNORE_RET_VAL(m_forked);
#endif
    AddArg("config-options-in-stdin", 0, _(""), &m_dummy)
        .SetHidden();  // Used in spawn mode
    AddArg("ovr-zoom-level", 0, _("Overview zoom level to compute"),
           &m_ovrZoomLevel)
        .SetMinValueIncluded(0)
        .SetHidden();  // Used in spawn mode
    AddArg("ovr-min-x", 0, _("Minimum tile X coordinate"), &m_minOvrTileX)
        .SetMinValueIncluded(0)
        .SetHidden();  // Used in spawn mode
    AddArg("ovr-max-x", 0, _("Maximum tile X coordinate"), &m_maxOvrTileX)
        .SetMinValueIncluded(0)
        .SetHidden();  // Used in spawn mode
    AddArg("ovr-min-y", 0, _("Minimum tile Y coordinate"), &m_minOvrTileY)
        .SetMinValueIncluded(0)
        .SetHidden();  // Used in spawn mode
    AddArg("ovr-max-y", 0, _("Maximum tile Y coordinate"), &m_maxOvrTileY)
        .SetMinValueIncluded(0)
        .SetHidden();  // Used in spawn mode

    if (standaloneStep)
    {
        AddRasterInputArgs(/* openForMixedRasterVector = */ false,
                           /* hiddenForCLI = */ false);
    }
    else
    {
        AddRasterHiddenInputDatasetArg();
    }

    m_format = "PNG";
    AddOutputFormatArg(&m_format)
        .SetDefault(m_format)
        .AddMetadataItem(
            GAAMDI_REQUIRED_CAPABILITIES,
            {GDAL_DCAP_RASTER, GDAL_DCAP_CREATECOPY, GDAL_DMD_EXTENSIONS})
        .AddMetadataItem(GAAMDI_VRT_COMPATIBLE, {"false"});
    AddCreationOptionsArg(&m_creationOptions);

    AddArg(GDAL_ARG_NAME_OUTPUT, 'o', _("Output directory"), &m_output)
        .SetRequired()
        .SetIsInput()
        .SetMinCharCount(1)
        .SetPositional();

    std::vector<std::string> tilingSchemes{"raster"};
    for (const std::string &scheme :
         gdal::TileMatrixSet::listPredefinedTileMatrixSets())
    {
        auto poTMS = gdal::TileMatrixSet::parse(scheme.c_str());
        OGRSpatialReference oSRS_TMS;
        if (poTMS && !poTMS->hasVariableMatrixWidth() &&
            oSRS_TMS.SetFromUserInput(poTMS->crs().c_str()) == OGRERR_NONE)
        {
            std::string identifier = scheme == "GoogleMapsCompatible"
                                         ? "WebMercatorQuad"
                                         : poTMS->identifier();
            m_mapTileMatrixIdentifierToScheme[identifier] = scheme;
            tilingSchemes.push_back(std::move(identifier));
        }
    }
    AddArg("tiling-scheme", 0, _("Tiling scheme"), &m_tilingScheme)
        .SetDefault("WebMercatorQuad")
        .SetChoices(tilingSchemes)
        .SetHiddenChoices(
            "GoogleMapsCompatible",  // equivalent of WebMercatorQuad
            "mercator",              // gdal2tiles equivalent of WebMercatorQuad
            "geodetic"  // gdal2tiles (not totally) equivalent of WorldCRS84Quad
        );

    AddArg("min-zoom", 0, _("Minimum zoom level"), &m_minZoomLevel)
        .SetMinValueIncluded(0);
    AddArg("max-zoom", 0, _("Maximum zoom level"), &m_maxZoomLevel)
        .SetMinValueIncluded(0);

    AddArg("min-x", 0, _("Minimum tile X coordinate"), &m_minTileX)
        .SetMinValueIncluded(0);
    AddArg("max-x", 0, _("Maximum tile X coordinate"), &m_maxTileX)
        .SetMinValueIncluded(0);
    AddArg("min-y", 0, _("Minimum tile Y coordinate"), &m_minTileY)
        .SetMinValueIncluded(0);
    AddArg("max-y", 0, _("Maximum tile Y coordinate"), &m_maxTileY)
        .SetMinValueIncluded(0);
    AddArg("no-intersection-ok", 0,
           _("Whether dataset extent not intersecting tile matrix is only a "
             "warning"),
           &m_noIntersectionIsOK);

    AddArg("resampling", 'r', _("Resampling method for max zoom"),
           &m_resampling)
        .SetChoices("nearest", "bilinear", "cubic", "cubicspline", "lanczos",
                    "average", "rms", "mode", "min", "max", "med", "q1", "q3",
                    "sum")
        .SetDefault("cubic")
        .SetHiddenChoices("near");
    AddArg("overview-resampling", 0, _("Resampling method for overviews"),
           &m_overviewResampling)
        .SetChoices("nearest", "bilinear", "cubic", "cubicspline", "lanczos",
                    "average", "rms", "mode", "min", "max", "med", "q1", "q3",
                    "sum")
        .SetHiddenChoices("near");

    AddArg("convention", 0,
           _("Tile numbering convention: xyz (from top) or tms (from bottom)"),
           &m_convention)
        .SetDefault(m_convention)
        .SetChoices("xyz", "tms");
    AddArg("tile-size", 0, _("Override default tile size"), &m_tileSize)
        .SetMinValueIncluded(64)
        .SetMaxValueIncluded(32768);
    AddArg("add-alpha", 0, _("Whether to force adding an alpha channel"),
           &m_addalpha)
        .SetMutualExclusionGroup("alpha");
    AddArg("no-alpha", 0, _("Whether to disable adding an alpha channel"),
           &m_noalpha)
        .SetMutualExclusionGroup("alpha");
    auto &dstNoDataArg =
        AddArg("dst-nodata", 0, _("Destination nodata value"), &m_dstNoData);
    AddArg("skip-blank", 0, _("Do not generate blank tiles"), &m_skipBlank);

    {
        auto &arg = AddArg("metadata", 0,
                           _("Add metadata item to output tiles"), &m_metadata)
                        .SetMetaVar("<KEY>=<VALUE>")
                        .SetPackedValuesAllowed(false);
        arg.AddValidationAction([this, &arg]()
                                { return ParseAndValidateKeyValue(arg); });
        arg.AddHiddenAlias("mo");
    }
    AddArg("copy-src-metadata", 0,
           _("Whether to copy metadata from source dataset"),
           &m_copySrcMetadata);

    AddArg("aux-xml", 0, _("Generate .aux.xml sidecar files when needed"),
           &m_auxXML);
    AddArg("kml", 0, _("Generate KML files"), &m_kml);
    AddArg("resume", 0, _("Generate only missing files"), &m_resume);

    AddNumThreadsArg(&m_numThreads, &m_numThreadsStr);
    AddArg("parallel-method", 0,
#ifdef FORK_ALLOWED
           _("Parallelization method (thread, spawn, fork)")
#else
           _("Parallelization method (thread / spawn)")
#endif
               ,
           &m_parallelMethod)
        .SetChoices("thread", "spawn"
#ifdef FORK_ALLOWED
                    ,
                    "fork"
#endif
        );

    constexpr const char *ADVANCED_RESAMPLING_CATEGORY = "Advanced Resampling";
    auto &excludedValuesArg =
        AddArg("excluded-values", 0,
               _("Tuples of values (e.g. <R>,<G>,<B> or (<R1>,<G1>,<B1>),"
                 "(<R2>,<G2>,<B2>)) that must beignored as contributing source "
                 "pixels during (average) resampling"),
               &m_excludedValues)
            .SetCategory(ADVANCED_RESAMPLING_CATEGORY);
    auto &excludedValuesPctThresholdArg =
        AddArg(
            "excluded-values-pct-threshold", 0,
            _("Minimum percentage of source pixels that must be set at one of "
              "the --excluded-values to cause the excluded value to be used as "
              "the target pixel value"),
            &m_excludedValuesPctThreshold)
            .SetDefault(m_excludedValuesPctThreshold)
            .SetMinValueIncluded(0)
            .SetMaxValueIncluded(100)
            .SetCategory(ADVANCED_RESAMPLING_CATEGORY);
    auto &nodataValuesPctThresholdArg =
        AddArg(
            "nodata-values-pct-threshold", 0,
            _("Minimum percentage of source pixels that must be set at one of "
              "nodata (or alpha=0 or any other way to express transparent pixel"
              "to cause the target pixel value to be transparent"),
            &m_nodataValuesPctThreshold)
            .SetDefault(m_nodataValuesPctThreshold)
            .SetMinValueIncluded(0)
            .SetMaxValueIncluded(100)
            .SetCategory(ADVANCED_RESAMPLING_CATEGORY);

    constexpr const char *PUBLICATION_CATEGORY = "Publication";
    AddArg("webviewer", 0, _("Web viewer to generate"), &m_webviewers)
        .SetDefault("all")
        .SetChoices("none", "all", "leaflet", "openlayers", "mapml", "stac")
        .SetCategory(PUBLICATION_CATEGORY);
    AddArg("url", 0,
           _("URL address where the generated tiles are going to be published"),
           &m_url)
        .SetCategory(PUBLICATION_CATEGORY);
    AddArg("title", 0, _("Title of the map"), &m_title)
        .SetCategory(PUBLICATION_CATEGORY);
    AddArg("copyright", 0, _("Copyright for the map"), &m_copyright)
        .SetCategory(PUBLICATION_CATEGORY);
    AddArg("mapml-template", 0,
           _("Filename of a template mapml file where variables will be "
             "substituted"),
           &m_mapmlTemplate)
        .SetMinCharCount(1)
        .SetCategory(PUBLICATION_CATEGORY);

    AddValidationAction(
        [this, &dstNoDataArg, &excludedValuesArg,
         &excludedValuesPctThresholdArg, &nodataValuesPctThresholdArg]()
        {
            if (m_minTileX >= 0 && m_maxTileX >= 0 && m_minTileX > m_maxTileX)
            {
                ReportError(CE_Failure, CPLE_IllegalArg,
                            "'min-x' must be lesser or equal to 'max-x'");
                return false;
            }

            if (m_minTileY >= 0 && m_maxTileY >= 0 && m_minTileY > m_maxTileY)
            {
                ReportError(CE_Failure, CPLE_IllegalArg,
                            "'min-y' must be lesser or equal to 'max-y'");
                return false;
            }

            if (m_minZoomLevel >= 0 && m_maxZoomLevel >= 0 &&
                m_minZoomLevel > m_maxZoomLevel)
            {
                ReportError(CE_Failure, CPLE_IllegalArg,
                            "'min-zoom' must be lesser or equal to 'max-zoom'");
                return false;
            }

            if (m_addalpha && dstNoDataArg.IsExplicitlySet())
            {
                ReportError(
                    CE_Failure, CPLE_IllegalArg,
                    "'add-alpha' and 'dst-nodata' are mutually exclusive");
                return false;
            }

            for (const auto *arg :
                 {&excludedValuesArg, &excludedValuesPctThresholdArg,
                  &nodataValuesPctThresholdArg})
            {
                if (arg->IsExplicitlySet() && m_resampling != "average")
                {
                    ReportError(CE_Failure, CPLE_AppDefined,
                                "'%s' can only be specified if 'resampling' is "
                                "set to 'average'",
                                arg->GetName().c_str());
                    return false;
                }
                if (arg->IsExplicitlySet() && !m_overviewResampling.empty() &&
                    m_overviewResampling != "average")
                {
                    ReportError(CE_Failure, CPLE_AppDefined,
                                "'%s' can only be specified if "
                                "'overview-resampling' is set to 'average'",
                                arg->GetName().c_str());
                    return false;
                }
            }

            return true;
        });
}

/************************************************************************/
/*                           GetTileIndices()                           */
/************************************************************************/

static bool GetTileIndices(gdal::TileMatrixSet::TileMatrix &tileMatrix,
                           bool bInvertAxisTMS, int tileSize,
                           const double adfExtent[4], int &nMinTileX,
                           int &nMinTileY, int &nMaxTileX, int &nMaxTileY,
                           bool noIntersectionIsOK, bool &bIntersects,
                           bool checkRasterOverflow = true)
{
    if (tileSize > 0)
    {
        tileMatrix.mResX *=
            static_cast<double>(tileMatrix.mTileWidth) / tileSize;
        tileMatrix.mResY *=
            static_cast<double>(tileMatrix.mTileHeight) / tileSize;
        tileMatrix.mTileWidth = tileSize;
        tileMatrix.mTileHeight = tileSize;
    }

    if (bInvertAxisTMS)
        std::swap(tileMatrix.mTopLeftX, tileMatrix.mTopLeftY);

    const double dfTileWidth = tileMatrix.mResX * tileMatrix.mTileWidth;
    const double dfTileHeight = tileMatrix.mResY * tileMatrix.mTileHeight;

    constexpr double EPSILON = 1e-3;
    const double dfMinTileX =
        (adfExtent[0] - tileMatrix.mTopLeftX) / dfTileWidth;
    nMinTileX = static_cast<int>(
        std::clamp(std::floor(dfMinTileX + EPSILON), 0.0,
                   static_cast<double>(tileMatrix.mMatrixWidth - 1)));
    const double dfMinTileY =
        (tileMatrix.mTopLeftY - adfExtent[3]) / dfTileHeight;
    nMinTileY = static_cast<int>(
        std::clamp(std::floor(dfMinTileY + EPSILON), 0.0,
                   static_cast<double>(tileMatrix.mMatrixHeight - 1)));
    const double dfMaxTileX =
        (adfExtent[2] - tileMatrix.mTopLeftX) / dfTileWidth;
    nMaxTileX = static_cast<int>(
        std::clamp(std::floor(dfMaxTileX + EPSILON), 0.0,
                   static_cast<double>(tileMatrix.mMatrixWidth - 1)));
    const double dfMaxTileY =
        (tileMatrix.mTopLeftY - adfExtent[1]) / dfTileHeight;
    nMaxTileY = static_cast<int>(
        std::clamp(std::floor(dfMaxTileY + EPSILON), 0.0,
                   static_cast<double>(tileMatrix.mMatrixHeight - 1)));

    bIntersects = (dfMinTileX <= tileMatrix.mMatrixWidth && dfMaxTileX >= 0 &&
                   dfMinTileY <= tileMatrix.mMatrixHeight && dfMaxTileY >= 0);
    if (!bIntersects)
    {
        CPLDebug("gdal_raster_tile",
                 "dfMinTileX=%g dfMinTileY=%g dfMaxTileX=%g dfMaxTileY=%g",
                 dfMinTileX, dfMinTileY, dfMaxTileX, dfMaxTileY);
        CPLError(noIntersectionIsOK ? CE_Warning : CE_Failure, CPLE_AppDefined,
                 "Extent of source dataset is not compatible with extent of "
                 "tile matrix %s",
                 tileMatrix.mId.c_str());
        return noIntersectionIsOK;
    }
    if (checkRasterOverflow)
    {
        if (nMaxTileX - nMinTileX + 1 > INT_MAX / tileMatrix.mTileWidth ||
            nMaxTileY - nMinTileY + 1 > INT_MAX / tileMatrix.mTileHeight)
        {
            CPLError(CE_Failure, CPLE_AppDefined, "Too large zoom level");
            return false;
        }
    }
    return true;
}

/************************************************************************/
/*                              GetFileY()                              */
/************************************************************************/

static int GetFileY(int iY, const gdal::TileMatrixSet::TileMatrix &tileMatrix,
                    const std::string &convention)
{
    return convention == "xyz" ? iY : tileMatrix.mMatrixHeight - 1 - iY;
}

/************************************************************************/
/*                            GenerateTile()                            */
/************************************************************************/

// Cf http://www.libpng.org/pub/png/spec/1.2/PNG-Filters.html
// for specification of SUB and AVG filters
inline GByte PNG_SUB(int nVal, int nValPrev)
{
    return static_cast<GByte>((nVal - nValPrev) & 0xff);
}

inline GByte PNG_AVG(int nVal, int nValPrev, int nValUp)
{
    return static_cast<GByte>((nVal - (nValPrev + nValUp) / 2) & 0xff);
}

inline GByte PNG_PAETH(int nVal, int nValPrev, int nValUp, int nValUpPrev)
{
    const int p = nValPrev + nValUp - nValUpPrev;
    const int pa = std::abs(p - nValPrev);
    const int pb = std::abs(p - nValUp);
    const int pc = std::abs(p - nValUpPrev);
    if (pa <= pb && pa <= pc)
        return static_cast<GByte>((nVal - nValPrev) & 0xff);
    else if (pb <= pc)
        return static_cast<GByte>((nVal - nValUp) & 0xff);
    else
        return static_cast<GByte>((nVal - nValUpPrev) & 0xff);
}

#ifdef USE_PAETH_SSE2

static inline __m128i abs_epi16(__m128i x)
{
#if defined(__SSSE3__) || defined(__AVX__) || defined(USE_NEON_OPTIMIZATIONS)
    return _mm_abs_epi16(x);
#else
    __m128i mask = _mm_srai_epi16(x, 15);
    return _mm_sub_epi16(_mm_xor_si128(x, mask), mask);
#endif
}

static inline __m128i blendv(__m128i a, __m128i b, __m128i mask)
{
#if defined(__SSE4_1__) || defined(__AVX__) || defined(USE_NEON_OPTIMIZATIONS)
    return _mm_blendv_epi8(a, b, mask);
#else
    return _mm_or_si128(_mm_andnot_si128(mask, a), _mm_and_si128(mask, b));
#endif
}

static inline __m128i PNG_PAETH_SSE2(__m128i up_prev, __m128i up, __m128i prev,
                                     __m128i cur, __m128i &cost)
{
    auto cur_lo = _mm_unpacklo_epi8(cur, _mm_setzero_si128());
    auto prev_lo = _mm_unpacklo_epi8(prev, _mm_setzero_si128());
    auto up_lo = _mm_unpacklo_epi8(up, _mm_setzero_si128());
    auto up_prev_lo = _mm_unpacklo_epi8(up_prev, _mm_setzero_si128());
    auto cur_hi = _mm_unpackhi_epi8(cur, _mm_setzero_si128());
    auto prev_hi = _mm_unpackhi_epi8(prev, _mm_setzero_si128());
    auto up_hi = _mm_unpackhi_epi8(up, _mm_setzero_si128());
    auto up_prev_hi = _mm_unpackhi_epi8(up_prev, _mm_setzero_si128());

    auto pa_lo = _mm_sub_epi16(up_lo, up_prev_lo);
    auto pb_lo = _mm_sub_epi16(prev_lo, up_prev_lo);
    auto pc_lo = _mm_add_epi16(pa_lo, pb_lo);
    pa_lo = abs_epi16(pa_lo);
    pb_lo = abs_epi16(pb_lo);
    pc_lo = abs_epi16(pc_lo);
    auto min_lo = _mm_min_epi16(_mm_min_epi16(pa_lo, pb_lo), pc_lo);

    auto res_lo = blendv(up_prev_lo, up_lo, _mm_cmpeq_epi16(min_lo, pb_lo));
    res_lo = blendv(res_lo, prev_lo, _mm_cmpeq_epi16(min_lo, pa_lo));
    res_lo = _mm_and_si128(_mm_sub_epi16(cur_lo, res_lo), _mm_set1_epi16(0xFF));

    auto cost_lo = blendv(_mm_sub_epi16(_mm_set1_epi16(256), res_lo), res_lo,
                          _mm_cmplt_epi16(res_lo, _mm_set1_epi16(128)));

    auto pa_hi = _mm_sub_epi16(up_hi, up_prev_hi);
    auto pb_hi = _mm_sub_epi16(prev_hi, up_prev_hi);
    auto pc_hi = _mm_add_epi16(pa_hi, pb_hi);
    pa_hi = abs_epi16(pa_hi);
    pb_hi = abs_epi16(pb_hi);
    pc_hi = abs_epi16(pc_hi);
    auto min_hi = _mm_min_epi16(_mm_min_epi16(pa_hi, pb_hi), pc_hi);

    auto res_hi = blendv(up_prev_hi, up_hi, _mm_cmpeq_epi16(min_hi, pb_hi));
    res_hi = blendv(res_hi, prev_hi, _mm_cmpeq_epi16(min_hi, pa_hi));
    res_hi = _mm_and_si128(_mm_sub_epi16(cur_hi, res_hi), _mm_set1_epi16(0xFF));

    auto cost_hi = blendv(_mm_sub_epi16(_mm_set1_epi16(256), res_hi), res_hi,
                          _mm_cmplt_epi16(res_hi, _mm_set1_epi16(128)));

    cost_lo = _mm_add_epi16(cost_lo, cost_hi);

    cost =
        _mm_add_epi32(cost, _mm_unpacklo_epi16(cost_lo, _mm_setzero_si128()));
    cost =
        _mm_add_epi32(cost, _mm_unpackhi_epi16(cost_lo, _mm_setzero_si128()));

    return _mm_packus_epi16(res_lo, res_hi);
}

static int RunPaeth(const GByte *srcBuffer, int nBands,
                    int nSrcBufferBandStride, GByte *outBuffer, int W,
                    int &costPaeth)
{
    __m128i xmm_cost = _mm_setzero_si128();
    int i = 1;
    for (int k = 0; k < nBands; ++k)
    {
        for (i = 1; i + 15 < W; i += 16)
        {
            auto up_prev = _mm_loadu_si128(
                reinterpret_cast<const __m128i *>(srcBuffer - W + (i - 1)));
            auto up = _mm_loadu_si128(
                reinterpret_cast<const __m128i *>(srcBuffer - W + i));
            auto prev = _mm_loadu_si128(
                reinterpret_cast<const __m128i *>(srcBuffer + (i - 1)));
            auto cur = _mm_loadu_si128(
                reinterpret_cast<const __m128i *>(srcBuffer + i));

            auto res = PNG_PAETH_SSE2(up_prev, up, prev, cur, xmm_cost);

            _mm_storeu_si128(reinterpret_cast<__m128i *>(outBuffer + k * W + i),
                             res);
        }
        srcBuffer += nSrcBufferBandStride;
    }

    int32_t ar_cost[4];
    _mm_storeu_si128(reinterpret_cast<__m128i *>(ar_cost), xmm_cost);
    for (int k = 0; k < 4; ++k)
        costPaeth += ar_cost[k];

    return i;
}

#endif  // USE_PAETH_SSE2

static bool GenerateTile(
    GDALDataset *poSrcDS, GDALDriver *m_poDstDriver, const char *pszExtension,
    CSLConstList creationOptions, GDALWarpOperation &oWO,
    const OGRSpatialReference &oSRS_TMS, GDALDataType eWorkingDataType,
    const gdal::TileMatrixSet::TileMatrix &tileMatrix,
    const std::string &outputDirectory, int nBands, const double *pdfDstNoData,
    int nZoomLevel, int iX, int iY, const std::string &convention,
    int nMinTileX, int nMinTileY, bool bSkipBlank, bool bUserAskedForAlpha,
    bool bAuxXML, bool bResume, const std::vector<std::string> &metadata,
    const GDALColorTable *poColorTable, std::vector<GByte> &dstBuffer,
    std::vector<GByte> &tmpBuffer)
{
    const std::string osDirZ = CPLFormFilenameSafe(
        outputDirectory.c_str(), CPLSPrintf("%d", nZoomLevel), nullptr);
    const std::string osDirX =
        CPLFormFilenameSafe(osDirZ.c_str(), CPLSPrintf("%d", iX), nullptr);
    const int iFileY = GetFileY(iY, tileMatrix, convention);
    const std::string osFilename = CPLFormFilenameSafe(
        osDirX.c_str(), CPLSPrintf("%d", iFileY), pszExtension);

    if (bResume)
    {
        VSIStatBufL sStat;
        if (VSIStatL(osFilename.c_str(), &sStat) == 0)
            return true;
    }

    const int nDstXOff = (iX - nMinTileX) * tileMatrix.mTileWidth;
    const int nDstYOff = (iY - nMinTileY) * tileMatrix.mTileHeight;
    memset(dstBuffer.data(), 0, dstBuffer.size());
    const CPLErr eErr = oWO.WarpRegionToBuffer(
        nDstXOff, nDstYOff, tileMatrix.mTileWidth, tileMatrix.mTileHeight,
        dstBuffer.data(), eWorkingDataType);
    if (eErr != CE_None)
        return false;

    bool bDstHasAlpha =
        nBands > poSrcDS->GetRasterCount() ||
        (nBands == poSrcDS->GetRasterCount() &&
         poSrcDS->GetRasterBand(nBands)->GetColorInterpretation() ==
             GCI_AlphaBand);
    const size_t nBytesPerBand = static_cast<size_t>(tileMatrix.mTileWidth) *
                                 tileMatrix.mTileHeight *
                                 GDALGetDataTypeSizeBytes(eWorkingDataType);
    if (bDstHasAlpha && bSkipBlank)
    {
        bool bBlank = true;
        for (size_t i = 0; i < nBytesPerBand && bBlank; ++i)
        {
            bBlank = (dstBuffer[(nBands - 1) * nBytesPerBand + i] == 0);
        }
        if (bBlank)
            return true;
    }
    if (bDstHasAlpha && !bUserAskedForAlpha)
    {
        bool bAllOpaque = true;
        for (size_t i = 0; i < nBytesPerBand && bAllOpaque; ++i)
        {
            bAllOpaque = (dstBuffer[(nBands - 1) * nBytesPerBand + i] == 255);
        }
        if (bAllOpaque)
        {
            bDstHasAlpha = false;
            nBands--;
        }
    }

    VSIMkdir(osDirZ.c_str(), 0755);
    VSIMkdir(osDirX.c_str(), 0755);

    const bool bSupportsCreateOnlyVisibleAtCloseTime =
        m_poDstDriver->GetMetadataItem(
            GDAL_DCAP_CREATE_ONLY_VISIBLE_AT_CLOSE_TIME) != nullptr;

    const std::string osTmpFilename = bSupportsCreateOnlyVisibleAtCloseTime
                                          ? osFilename
                                          : osFilename + ".tmp." + pszExtension;

    const int W = tileMatrix.mTileWidth;
    const int H = tileMatrix.mTileHeight;
    constexpr int EXTRA_BYTE_PER_ROW = 1;  // for filter type
    constexpr int EXTRA_ROWS = 2;          // for paethBuffer and paethBufferTmp
    if (!bAuxXML && EQUAL(pszExtension, "png") &&
        eWorkingDataType == GDT_UInt8 && poColorTable == nullptr &&
        pdfDstNoData == nullptr && W <= INT_MAX / nBands &&
        nBands * W <= INT_MAX - EXTRA_BYTE_PER_ROW &&
        H <= INT_MAX - EXTRA_ROWS &&
        EXTRA_BYTE_PER_ROW + nBands * W <= INT_MAX / (H + EXTRA_ROWS) &&
        CSLCount(creationOptions) == 0 &&
        CPLTestBool(
            CPLGetConfigOption("GDAL_RASTER_TILE_USE_PNG_OPTIM", "YES")))
    {
        // This is an optimized code path completely shortcircuiting libpng
        // We manually generate the PNG file using the Average or PAETH filter
        // and ZLIB compressing the whole buffer, hopefully with libdeflate.

        const int nDstBytesPerRow = EXTRA_BYTE_PER_ROW + nBands * W;
        const int nBPB = static_cast<int>(nBytesPerBand);

        bool bBlank = false;
        if (bDstHasAlpha)
        {
            bBlank = true;
            for (int i = 0; i < nBPB && bBlank; ++i)
            {
                bBlank = (dstBuffer[(nBands - 1) * nBPB + i] == 0);
            }
        }

        constexpr GByte PNG_FILTER_SUB = 1;  // horizontal diff
        constexpr GByte PNG_FILTER_AVG = 3;  // average with pixel before and up
        constexpr GByte PNG_FILTER_PAETH = 4;

        if (bBlank)
            tmpBuffer.clear();
        const int tmpBufferSize = cpl::fits_on<int>(nDstBytesPerRow * H);
        try
        {
            // cppcheck-suppress integerOverflowCond
            tmpBuffer.resize(tmpBufferSize + EXTRA_ROWS * nDstBytesPerRow);
        }
        catch (const std::exception &)
        {
            CPLError(CE_Failure, CPLE_OutOfMemory,
                     "Out of memory allocating temporary buffer");
            return false;
        }
        GByte *const paethBuffer = tmpBuffer.data() + tmpBufferSize;
#ifdef USE_PAETH_SSE2
        GByte *const paethBufferTmp =
            tmpBuffer.data() + tmpBufferSize + nDstBytesPerRow;
#endif

        const char *pszGDAL_RASTER_TILE_PNG_FILTER =
            CPLGetConfigOption("GDAL_RASTER_TILE_PNG_FILTER", "");
        const bool bForcePaeth = EQUAL(pszGDAL_RASTER_TILE_PNG_FILTER, "PAETH");
        const bool bForceAvg = EQUAL(pszGDAL_RASTER_TILE_PNG_FILTER, "AVERAGE");

        for (int j = 0; !bBlank && j < H; ++j)
        {
            if (j > 0)
            {
                tmpBuffer[cpl::fits_on<int>(j * nDstBytesPerRow)] =
                    PNG_FILTER_AVG;
                for (int i = 0; i < nBands; ++i)
                {
                    tmpBuffer[1 + j * nDstBytesPerRow + i] =
                        PNG_AVG(dstBuffer[i * nBPB + j * W], 0,
                                dstBuffer[i * nBPB + (j - 1) * W]);
                }
            }
            else
            {
                tmpBuffer[cpl::fits_on<int>(j * nDstBytesPerRow)] =
                    PNG_FILTER_SUB;
                for (int i = 0; i < nBands; ++i)
                {
                    tmpBuffer[1 + j * nDstBytesPerRow + i] =
                        dstBuffer[i * nBPB + j * W];
                }
            }

            if (nBands == 1)
            {
                if (j > 0)
                {
                    int costAvg = 0;
                    for (int i = 1; i < W; ++i)
                    {
                        const GByte v =
                            PNG_AVG(dstBuffer[0 * nBPB + j * W + i],
                                    dstBuffer[0 * nBPB + j * W + i - 1],
                                    dstBuffer[0 * nBPB + (j - 1) * W + i]);
                        tmpBuffer[1 + j * nDstBytesPerRow + i * nBands + 0] = v;

                        costAvg += (v < 128) ? v : 256 - v;
                    }

                    if (!bForceAvg)
                    {
                        int costPaeth = 0;
                        {
                            const int i = 0;
                            const GByte v = PNG_PAETH(
                                dstBuffer[0 * nBPB + j * W + i], 0,
                                dstBuffer[0 * nBPB + (j - 1) * W + i], 0);
                            paethBuffer[i] = v;

                            costPaeth += (v < 128) ? v : 256 - v;
                        }

#ifdef USE_PAETH_SSE2
                        const int iLimitSSE2 =
                            RunPaeth(dstBuffer.data() + j * W, nBands, nBPB,
                                     paethBuffer, W, costPaeth);
                        int i = iLimitSSE2;
#else
                        int i = 1;
#endif
                        for (; i < W && (costPaeth < costAvg || bForcePaeth);
                             ++i)
                        {
                            const GByte v = PNG_PAETH(
                                dstBuffer[0 * nBPB + j * W + i],
                                dstBuffer[0 * nBPB + j * W + i - 1],
                                dstBuffer[0 * nBPB + (j - 1) * W + i],
                                dstBuffer[0 * nBPB + (j - 1) * W + i - 1]);
                            paethBuffer[i] = v;

                            costPaeth += (v < 128) ? v : 256 - v;
                        }
                        if (costPaeth < costAvg || bForcePaeth)
                        {
                            GByte *out = tmpBuffer.data() +
                                         cpl::fits_on<int>(j * nDstBytesPerRow);
                            *out = PNG_FILTER_PAETH;
                            ++out;
                            memcpy(out, paethBuffer, nDstBytesPerRow - 1);
                        }
                    }
                }
                else
                {
                    for (int i = 1; i < W; ++i)
                    {
                        tmpBuffer[1 + j * nDstBytesPerRow + i * nBands + 0] =
                            PNG_SUB(dstBuffer[0 * nBPB + j * W + i],
                                    dstBuffer[0 * nBPB + j * W + i - 1]);
                    }
                }
            }
            else if (nBands == 2)
            {
                if (j > 0)
                {
                    int costAvg = 0;
                    for (int i = 1; i < W; ++i)
                    {
                        {
                            const GByte v =
                                PNG_AVG(dstBuffer[0 * nBPB + j * W + i],
                                        dstBuffer[0 * nBPB + j * W + i - 1],
                                        dstBuffer[0 * nBPB + (j - 1) * W + i]);
                            tmpBuffer[1 + j * nDstBytesPerRow + i * nBands +
                                      0] = v;

                            costAvg += (v < 128) ? v : 256 - v;
                        }
                        {
                            const GByte v =
                                PNG_AVG(dstBuffer[1 * nBPB + j * W + i],
                                        dstBuffer[1 * nBPB + j * W + i - 1],
                                        dstBuffer[1 * nBPB + (j - 1) * W + i]);
                            tmpBuffer[1 + j * nDstBytesPerRow + i * nBands +
                                      1] = v;

                            costAvg += (v < 128) ? v : 256 - v;
                        }
                    }

                    if (!bForceAvg)
                    {
                        int costPaeth = 0;
                        for (int k = 0; k < nBands; ++k)
                        {
                            const int i = 0;
                            const GByte v = PNG_PAETH(
                                dstBuffer[k * nBPB + j * W + i], 0,
                                dstBuffer[k * nBPB + (j - 1) * W + i], 0);
                            paethBuffer[i * nBands + k] = v;

                            costPaeth += (v < 128) ? v : 256 - v;
                        }

#ifdef USE_PAETH_SSE2
                        const int iLimitSSE2 =
                            RunPaeth(dstBuffer.data() + j * W, nBands, nBPB,
                                     paethBufferTmp, W, costPaeth);
                        int i = iLimitSSE2;
#else
                        int i = 1;
#endif
                        for (; i < W && (costPaeth < costAvg || bForcePaeth);
                             ++i)
                        {
                            {
                                const GByte v = PNG_PAETH(
                                    dstBuffer[0 * nBPB + j * W + i],
                                    dstBuffer[0 * nBPB + j * W + i - 1],
                                    dstBuffer[0 * nBPB + (j - 1) * W + i],
                                    dstBuffer[0 * nBPB + (j - 1) * W + i - 1]);
                                paethBuffer[i * nBands + 0] = v;

                                costPaeth += (v < 128) ? v : 256 - v;
                            }
                            {
                                const GByte v = PNG_PAETH(
                                    dstBuffer[1 * nBPB + j * W + i],
                                    dstBuffer[1 * nBPB + j * W + i - 1],
                                    dstBuffer[1 * nBPB + (j - 1) * W + i],
                                    dstBuffer[1 * nBPB + (j - 1) * W + i - 1]);
                                paethBuffer[i * nBands + 1] = v;

                                costPaeth += (v < 128) ? v : 256 - v;
                            }
                        }
                        if (costPaeth < costAvg || bForcePaeth)
                        {
                            GByte *out = tmpBuffer.data() +
                                         cpl::fits_on<int>(j * nDstBytesPerRow);
                            *out = PNG_FILTER_PAETH;
                            ++out;
#ifdef USE_PAETH_SSE2
                            memcpy(out, paethBuffer, nBands);
                            for (int iTmp = 1; iTmp < iLimitSSE2; ++iTmp)
                            {
                                out[nBands * iTmp + 0] =
                                    paethBufferTmp[0 * W + iTmp];
                                out[nBands * iTmp + 1] =
                                    paethBufferTmp[1 * W + iTmp];
                            }
                            memcpy(
                                out + iLimitSSE2 * nBands,
                                paethBuffer + iLimitSSE2 * nBands,
                                cpl::fits_on<int>((W - iLimitSSE2) * nBands));
#else
                            memcpy(out, paethBuffer, nDstBytesPerRow - 1);
#endif
                        }
                    }
                }
                else
                {
                    for (int i = 1; i < W; ++i)
                    {
                        tmpBuffer[1 + j * nDstBytesPerRow + i * nBands + 0] =
                            PNG_SUB(dstBuffer[0 * nBPB + j * W + i],
                                    dstBuffer[0 * nBPB + j * W + i - 1]);
                        tmpBuffer[1 + j * nDstBytesPerRow + i * nBands + 1] =
                            PNG_SUB(dstBuffer[1 * nBPB + j * W + i],
                                    dstBuffer[1 * nBPB + j * W + i - 1]);
                    }
                }
            }
            else if (nBands == 3)
            {
                if (j > 0)
                {
                    int costAvg = 0;
                    for (int i = 1; i < W; ++i)
                    {
                        {
                            const GByte v =
                                PNG_AVG(dstBuffer[0 * nBPB + j * W + i],
                                        dstBuffer[0 * nBPB + j * W + i - 1],
                                        dstBuffer[0 * nBPB + (j - 1) * W + i]);
                            tmpBuffer[1 + j * nDstBytesPerRow + i * nBands +
                                      0] = v;

                            costAvg += (v < 128) ? v : 256 - v;
                        }
                        {
                            const GByte v =
                                PNG_AVG(dstBuffer[1 * nBPB + j * W + i],
                                        dstBuffer[1 * nBPB + j * W + i - 1],
                                        dstBuffer[1 * nBPB + (j - 1) * W + i]);
                            tmpBuffer[1 + j * nDstBytesPerRow + i * nBands +
                                      1] = v;

                            costAvg += (v < 128) ? v : 256 - v;
                        }
                        {
                            const GByte v =
                                PNG_AVG(dstBuffer[2 * nBPB + j * W + i],
                                        dstBuffer[2 * nBPB + j * W + i - 1],
                                        dstBuffer[2 * nBPB + (j - 1) * W + i]);
                            tmpBuffer[1 + j * nDstBytesPerRow + i * nBands +
                                      2] = v;

                            costAvg += (v < 128) ? v : 256 - v;
                        }
                    }

                    if (!bForceAvg)
                    {
                        int costPaeth = 0;
                        for (int k = 0; k < nBands; ++k)
                        {
                            const int i = 0;
                            const GByte v = PNG_PAETH(
                                dstBuffer[k * nBPB + j * W + i], 0,
                                dstBuffer[k * nBPB + (j - 1) * W + i], 0);
                            paethBuffer[i * nBands + k] = v;

                            costPaeth += (v < 128) ? v : 256 - v;
                        }

#ifdef USE_PAETH_SSE2
                        const int iLimitSSE2 =
                            RunPaeth(dstBuffer.data() + j * W, nBands, nBPB,
                                     paethBufferTmp, W, costPaeth);
                        int i = iLimitSSE2;
#else
                        int i = 1;
#endif
                        for (; i < W && (costPaeth < costAvg || bForcePaeth);
                             ++i)
                        {
                            {
                                const GByte v = PNG_PAETH(
                                    dstBuffer[0 * nBPB + j * W + i],
                                    dstBuffer[0 * nBPB + j * W + i - 1],
                                    dstBuffer[0 * nBPB + (j - 1) * W + i],
                                    dstBuffer[0 * nBPB + (j - 1) * W + i - 1]);
                                paethBuffer[i * nBands + 0] = v;

                                costPaeth += (v < 128) ? v : 256 - v;
                            }
                            {
                                const GByte v = PNG_PAETH(
                                    dstBuffer[1 * nBPB + j * W + i],
                                    dstBuffer[1 * nBPB + j * W + i - 1],
                                    dstBuffer[1 * nBPB + (j - 1) * W + i],
                                    dstBuffer[1 * nBPB + (j - 1) * W + i - 1]);
                                paethBuffer[i * nBands + 1] = v;

                                costPaeth += (v < 128) ? v : 256 - v;
                            }
                            {
                                const GByte v = PNG_PAETH(
                                    dstBuffer[2 * nBPB + j * W + i],
                                    dstBuffer[2 * nBPB + j * W + i - 1],
                                    dstBuffer[2 * nBPB + (j - 1) * W + i],
                                    dstBuffer[2 * nBPB + (j - 1) * W + i - 1]);
                                paethBuffer[i * nBands + 2] = v;

                                costPaeth += (v < 128) ? v : 256 - v;
                            }
                        }

                        if (costPaeth < costAvg || bForcePaeth)
                        {
                            GByte *out = tmpBuffer.data() +
                                         cpl::fits_on<int>(j * nDstBytesPerRow);
                            *out = PNG_FILTER_PAETH;
                            ++out;
#ifdef USE_PAETH_SSE2
                            memcpy(out, paethBuffer, nBands);
                            for (int iTmp = 1; iTmp < iLimitSSE2; ++iTmp)
                            {
                                out[nBands * iTmp + 0] =
                                    paethBufferTmp[0 * W + iTmp];
                                out[nBands * iTmp + 1] =
                                    paethBufferTmp[1 * W + iTmp];
                                out[nBands * iTmp + 2] =
                                    paethBufferTmp[2 * W + iTmp];
                            }
                            memcpy(
                                out + iLimitSSE2 * nBands,
                                paethBuffer + iLimitSSE2 * nBands,
                                cpl::fits_on<int>((W - iLimitSSE2) * nBands));
#else
                            memcpy(out, paethBuffer, nDstBytesPerRow - 1);
#endif
                        }
                    }
                }
                else
                {
                    for (int i = 1; i < W; ++i)
                    {
                        tmpBuffer[1 + j * nDstBytesPerRow + i * nBands + 0] =
                            PNG_SUB(dstBuffer[0 * nBPB + j * W + i],
                                    dstBuffer[0 * nBPB + j * W + i - 1]);
                        tmpBuffer[1 + j * nDstBytesPerRow + i * nBands + 1] =
                            PNG_SUB(dstBuffer[1 * nBPB + j * W + i],
                                    dstBuffer[1 * nBPB + j * W + i - 1]);
                        tmpBuffer[1 + j * nDstBytesPerRow + i * nBands + 2] =
                            PNG_SUB(dstBuffer[2 * nBPB + j * W + i],
                                    dstBuffer[2 * nBPB + j * W + i - 1]);
                    }
                }
            }
            else /* if( nBands == 4 ) */
            {
                if (j > 0)
                {
                    int costAvg = 0;
                    for (int i = 1; i < W; ++i)
                    {
                        {
                            const GByte v =
                                PNG_AVG(dstBuffer[0 * nBPB + j * W + i],
                                        dstBuffer[0 * nBPB + j * W + i - 1],
                                        dstBuffer[0 * nBPB + (j - 1) * W + i]);
                            tmpBuffer[1 + j * nDstBytesPerRow + i * nBands +
                                      0] = v;

                            costAvg += (v < 128) ? v : 256 - v;
                        }
                        {
                            const GByte v =
                                PNG_AVG(dstBuffer[1 * nBPB + j * W + i],
                                        dstBuffer[1 * nBPB + j * W + i - 1],
                                        dstBuffer[1 * nBPB + (j - 1) * W + i]);
                            tmpBuffer[1 + j * nDstBytesPerRow + i * nBands +
                                      1] = v;

                            costAvg += (v < 128) ? v : 256 - v;
                        }
                        {
                            const GByte v =
                                PNG_AVG(dstBuffer[2 * nBPB + j * W + i],
                                        dstBuffer[2 * nBPB + j * W + i - 1],
                                        dstBuffer[2 * nBPB + (j - 1) * W + i]);
                            tmpBuffer[1 + j * nDstBytesPerRow + i * nBands +
                                      2] = v;

                            costAvg += (v < 128) ? v : 256 - v;
                        }
                        {
                            const GByte v =
                                PNG_AVG(dstBuffer[3 * nBPB + j * W + i],
                                        dstBuffer[3 * nBPB + j * W + i - 1],
                                        dstBuffer[3 * nBPB + (j - 1) * W + i]);
                            tmpBuffer[1 + j * nDstBytesPerRow + i * nBands +
                                      3] = v;

                            costAvg += (v < 128) ? v : 256 - v;
                        }
                    }

                    if (!bForceAvg)
                    {
                        int costPaeth = 0;
                        for (int k = 0; k < nBands; ++k)
                        {
                            const int i = 0;
                            const GByte v = PNG_PAETH(
                                dstBuffer[k * nBPB + j * W + i], 0,
                                dstBuffer[k * nBPB + (j - 1) * W + i], 0);
                            paethBuffer[i * nBands + k] = v;

                            costPaeth += (v < 128) ? v : 256 - v;
                        }

#ifdef USE_PAETH_SSE2
                        const int iLimitSSE2 =
                            RunPaeth(dstBuffer.data() + j * W, nBands, nBPB,
                                     paethBufferTmp, W, costPaeth);
                        int i = iLimitSSE2;
#else
                        int i = 1;
#endif
                        for (; i < W && (costPaeth < costAvg || bForcePaeth);
                             ++i)
                        {
                            {
                                const GByte v = PNG_PAETH(
                                    dstBuffer[0 * nBPB + j * W + i],
                                    dstBuffer[0 * nBPB + j * W + i - 1],
                                    dstBuffer[0 * nBPB + (j - 1) * W + i],
                                    dstBuffer[0 * nBPB + (j - 1) * W + i - 1]);
                                paethBuffer[i * nBands + 0] = v;

                                costPaeth += (v < 128) ? v : 256 - v;
                            }
                            {
                                const GByte v = PNG_PAETH(
                                    dstBuffer[1 * nBPB + j * W + i],
                                    dstBuffer[1 * nBPB + j * W + i - 1],
                                    dstBuffer[1 * nBPB + (j - 1) * W + i],
                                    dstBuffer[1 * nBPB + (j - 1) * W + i - 1]);
                                paethBuffer[i * nBands + 1] = v;

                                costPaeth += (v < 128) ? v : 256 - v;
                            }
                            {
                                const GByte v = PNG_PAETH(
                                    dstBuffer[2 * nBPB + j * W + i],
                                    dstBuffer[2 * nBPB + j * W + i - 1],
                                    dstBuffer[2 * nBPB + (j - 1) * W + i],
                                    dstBuffer[2 * nBPB + (j - 1) * W + i - 1]);
                                paethBuffer[i * nBands + 2] = v;

                                costPaeth += (v < 128) ? v : 256 - v;
                            }
                            {
                                const GByte v = PNG_PAETH(
                                    dstBuffer[3 * nBPB + j * W + i],
                                    dstBuffer[3 * nBPB + j * W + i - 1],
                                    dstBuffer[3 * nBPB + (j - 1) * W + i],
                                    dstBuffer[3 * nBPB + (j - 1) * W + i - 1]);
                                paethBuffer[i * nBands + 3] = v;

                                costPaeth += (v < 128) ? v : 256 - v;
                            }
                        }
                        if (costPaeth < costAvg || bForcePaeth)
                        {
                            GByte *out = tmpBuffer.data() +
                                         cpl::fits_on<int>(j * nDstBytesPerRow);
                            *out = PNG_FILTER_PAETH;
                            ++out;
#ifdef USE_PAETH_SSE2
                            memcpy(out, paethBuffer, nBands);
                            for (int iTmp = 1; iTmp < iLimitSSE2; ++iTmp)
                            {
                                out[nBands * iTmp + 0] =
                                    paethBufferTmp[0 * W + iTmp];
                                out[nBands * iTmp + 1] =
                                    paethBufferTmp[1 * W + iTmp];
                                out[nBands * iTmp + 2] =
                                    paethBufferTmp[2 * W + iTmp];
                                out[nBands * iTmp + 3] =
                                    paethBufferTmp[3 * W + iTmp];
                            }
                            memcpy(
                                out + iLimitSSE2 * nBands,
                                paethBuffer + iLimitSSE2 * nBands,
                                cpl::fits_on<int>((W - iLimitSSE2) * nBands));
#else
                            memcpy(out, paethBuffer, nDstBytesPerRow - 1);
#endif
                        }
                    }
                }
                else
                {
                    for (int i = 1; i < W; ++i)
                    {
                        tmpBuffer[1 + j * nDstBytesPerRow + i * nBands + 0] =
                            PNG_SUB(dstBuffer[0 * nBPB + j * W + i],
                                    dstBuffer[0 * nBPB + j * W + i - 1]);
                        tmpBuffer[1 + j * nDstBytesPerRow + i * nBands + 1] =
                            PNG_SUB(dstBuffer[1 * nBPB + j * W + i],
                                    dstBuffer[1 * nBPB + j * W + i - 1]);
                        tmpBuffer[1 + j * nDstBytesPerRow + i * nBands + 2] =
                            PNG_SUB(dstBuffer[2 * nBPB + j * W + i],
                                    dstBuffer[2 * nBPB + j * W + i - 1]);
                        tmpBuffer[1 + j * nDstBytesPerRow + i * nBands + 3] =
                            PNG_SUB(dstBuffer[3 * nBPB + j * W + i],
                                    dstBuffer[3 * nBPB + j * W + i - 1]);
                    }
                }
            }
        }
        size_t nOutSize = 0;
        // Shouldn't happen given the care we have done to dimension dstBuffer
        if (CPLZLibDeflate(tmpBuffer.data(), tmpBufferSize, -1,
                           dstBuffer.data(), dstBuffer.size(),
                           &nOutSize) == nullptr ||
            nOutSize > static_cast<size_t>(INT32_MAX))
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "CPLZLibDeflate() failed: too small destination buffer");
            return false;
        }

        VSILFILE *fp = VSIFOpenL(osTmpFilename.c_str(), "wb");
        if (!fp)
        {
            CPLError(CE_Failure, CPLE_FileIO, "Cannot create %s",
                     osTmpFilename.c_str());
            return false;
        }

        // Cf https://en.wikipedia.org/wiki/PNG#Examples for formatting of
        // IHDR, IDAT and IEND chunks

        // PNG Signature
        fp->Write("\x89PNG\x0D\x0A\x1A\x0A", 8, 1);

        uLong crc;
        const auto WriteAndUpdateCRC_Byte = [fp, &crc](uint8_t nVal)
        {
            fp->Write(&nVal, 1, sizeof(nVal));
            crc = crc32(crc, &nVal, sizeof(nVal));
        };
        const auto WriteAndUpdateCRC_Int = [fp, &crc](int32_t nVal)
        {
            CPL_MSBPTR32(&nVal);
            fp->Write(&nVal, 1, sizeof(nVal));
            crc = crc32(crc, reinterpret_cast<const Bytef *>(&nVal),
                        sizeof(nVal));
        };

        // IHDR chunk
        uint32_t nIHDRSize = 13;
        CPL_MSBPTR32(&nIHDRSize);
        fp->Write(&nIHDRSize, 1, sizeof(nIHDRSize));
        crc = crc32(0, reinterpret_cast<const Bytef *>("IHDR"), 4);
        fp->Write("IHDR", 1, 4);
        WriteAndUpdateCRC_Int(W);
        WriteAndUpdateCRC_Int(H);
        WriteAndUpdateCRC_Byte(8);  // Number of bits per pixel
        const uint8_t nColorType = nBands == 1   ? 0
                                   : nBands == 2 ? 4
                                   : nBands == 3 ? 2
                                                 : 6;
        WriteAndUpdateCRC_Byte(nColorType);
        WriteAndUpdateCRC_Byte(0);  // Compression method
        WriteAndUpdateCRC_Byte(0);  // Filter method
        WriteAndUpdateCRC_Byte(0);  // Interlacing=off
        {
            uint32_t nCrc32 = static_cast<uint32_t>(crc);
            CPL_MSBPTR32(&nCrc32);
            fp->Write(&nCrc32, 1, sizeof(nCrc32));
        }

        // IDAT chunk
        uint32_t nIDATSize = static_cast<uint32_t>(nOutSize);
        CPL_MSBPTR32(&nIDATSize);
        fp->Write(&nIDATSize, 1, sizeof(nIDATSize));
        crc = crc32(0, reinterpret_cast<const Bytef *>("IDAT"), 4);
        fp->Write("IDAT", 1, 4);
        crc = crc32(crc, dstBuffer.data(), static_cast<uint32_t>(nOutSize));
        fp->Write(dstBuffer.data(), 1, nOutSize);
        {
            uint32_t nCrc32 = static_cast<uint32_t>(crc);
            CPL_MSBPTR32(&nCrc32);
            fp->Write(&nCrc32, 1, sizeof(nCrc32));
        }

        // IEND chunk
        fp->Write("\x00\x00\x00\x00IEND\xAE\x42\x60\x82", 12, 1);

        bool bRet =
            fp->Tell() == 8 + 4 + 4 + 13 + 4 + 4 + 4 + nOutSize + 4 + 12;
        bRet = VSIFCloseL(fp) == 0 && bRet &&
               VSIRename(osTmpFilename.c_str(), osFilename.c_str()) == 0;
        if (!bRet)
            VSIUnlink(osTmpFilename.c_str());

        return bRet;
    }

    auto memDS = std::unique_ptr<GDALDataset>(
        MEMDataset::Create("", tileMatrix.mTileWidth, tileMatrix.mTileHeight, 0,
                           eWorkingDataType, nullptr));
    for (int i = 0; i < nBands; ++i)
    {
        char szBuffer[32] = {'\0'};
        int nRet = CPLPrintPointer(
            szBuffer, dstBuffer.data() + i * nBytesPerBand, sizeof(szBuffer));
        szBuffer[nRet] = 0;

        char szOption[64] = {'\0'};
        snprintf(szOption, sizeof(szOption), "DATAPOINTER=%s", szBuffer);

        char *apszOptions[] = {szOption, nullptr};

        memDS->AddBand(eWorkingDataType, apszOptions);
        auto poDstBand = memDS->GetRasterBand(i + 1);
        if (i + 1 <= poSrcDS->GetRasterCount())
            poDstBand->SetColorInterpretation(
                poSrcDS->GetRasterBand(i + 1)->GetColorInterpretation());
        else
            poDstBand->SetColorInterpretation(GCI_AlphaBand);
        if (pdfDstNoData)
            poDstBand->SetNoDataValue(*pdfDstNoData);
        if (i == 0 && poColorTable)
            poDstBand->SetColorTable(
                const_cast<GDALColorTable *>(poColorTable));
    }
    const CPLStringList aosMD(metadata);
    for (const auto [key, value] : cpl::IterateNameValue(aosMD))
    {
        memDS->SetMetadataItem(key, value);
    }

    GDALGeoTransform gt;
    gt.xorig =
        tileMatrix.mTopLeftX + iX * tileMatrix.mResX * tileMatrix.mTileWidth;
    gt.xscale = tileMatrix.mResX;
    gt.xrot = 0;
    gt.yorig =
        tileMatrix.mTopLeftY - iY * tileMatrix.mResY * tileMatrix.mTileHeight;
    gt.yrot = 0;
    gt.yscale = -tileMatrix.mResY;
    memDS->SetGeoTransform(gt);

    memDS->SetSpatialRef(&oSRS_TMS);

    CPLConfigOptionSetter oSetter("GDAL_PAM_ENABLED", bAuxXML ? "YES" : "NO",
                                  false);
    CPLConfigOptionSetter oSetter2("GDAL_DISABLE_READDIR_ON_OPEN", "YES",
                                   false);

    std::unique_ptr<CPLConfigOptionSetter> poSetter;
    // No need to reopen the dataset at end of CreateCopy() (for PNG
    // and JPEG) if we don't need to generate .aux.xml
    if (!bAuxXML)
        poSetter = std::make_unique<CPLConfigOptionSetter>(
            "GDAL_OPEN_AFTER_COPY", "NO", false);
    CPL_IGNORE_RET_VAL(poSetter);

    CPLStringList aosCreationOptions(creationOptions);
    if (bSupportsCreateOnlyVisibleAtCloseTime)
        aosCreationOptions.SetNameValue("@CREATE_ONLY_VISIBLE_AT_CLOSE_TIME",
                                        "YES");

    std::unique_ptr<GDALDataset> poOutDS(
        m_poDstDriver->CreateCopy(osTmpFilename.c_str(), memDS.get(), false,
                                  aosCreationOptions.List(), nullptr, nullptr));
    bool bRet = poOutDS && poOutDS->Close() == CE_None;
    poOutDS.reset();
    if (bRet)
    {
        if (!bSupportsCreateOnlyVisibleAtCloseTime)
        {
            bRet = VSIRename(osTmpFilename.c_str(), osFilename.c_str()) == 0;
            if (bAuxXML)
            {
                VSIRename((osTmpFilename + ".aux.xml").c_str(),
                          (osFilename + ".aux.xml").c_str());
            }
        }
    }
    else
    {
        VSIUnlink(osTmpFilename.c_str());
    }
    return bRet;
}

/************************************************************************/
/*                        GenerateOverviewTile()                        */
/************************************************************************/

static bool
GenerateOverviewTile(GDALDataset &oSrcDS, GDALDriver *m_poDstDriver,
                     const std::string &outputFormat, const char *pszExtension,
                     CSLConstList creationOptions,
                     CSLConstList papszWarpOptions,
                     const std::string &resampling,
                     const gdal::TileMatrixSet::TileMatrix &tileMatrix,
                     const std::string &outputDirectory, int nZoomLevel, int iX,
                     int iY, const std::string &convention, bool bSkipBlank,
                     bool bUserAskedForAlpha, bool bAuxXML, bool bResume)
{
    const std::string osDirZ = CPLFormFilenameSafe(
        outputDirectory.c_str(), CPLSPrintf("%d", nZoomLevel), nullptr);
    const std::string osDirX =
        CPLFormFilenameSafe(osDirZ.c_str(), CPLSPrintf("%d", iX), nullptr);

    const int iFileY = GetFileY(iY, tileMatrix, convention);
    const std::string osFilename = CPLFormFilenameSafe(
        osDirX.c_str(), CPLSPrintf("%d", iFileY), pszExtension);

    if (bResume)
    {
        VSIStatBufL sStat;
        if (VSIStatL(osFilename.c_str(), &sStat) == 0)
            return true;
    }

    VSIMkdir(osDirZ.c_str(), 0755);
    VSIMkdir(osDirX.c_str(), 0755);

    const bool bSupportsCreateOnlyVisibleAtCloseTime =
        m_poDstDriver->GetMetadataItem(
            GDAL_DCAP_CREATE_ONLY_VISIBLE_AT_CLOSE_TIME) != nullptr;

    CPLStringList aosOptions;

    aosOptions.AddString("-of");
    aosOptions.AddString(outputFormat.c_str());

    for (const char *pszCO : cpl::Iterate(creationOptions))
    {
        aosOptions.AddString("-co");
        aosOptions.AddString(pszCO);
    }
    if (bSupportsCreateOnlyVisibleAtCloseTime)
    {
        aosOptions.AddString("-co");
        aosOptions.AddString("@CREATE_ONLY_VISIBLE_AT_CLOSE_TIME=YES");
    }

    CPLConfigOptionSetter oSetter("GDAL_PAM_ENABLED", bAuxXML ? "YES" : "NO",
                                  false);
    CPLConfigOptionSetter oSetter2("GDAL_DISABLE_READDIR_ON_OPEN", "YES",
                                   false);

    aosOptions.AddString("-r");
    aosOptions.AddString(resampling.c_str());

    std::unique_ptr<GDALDataset> poOutDS;
    const double dfMinX =
        tileMatrix.mTopLeftX + iX * tileMatrix.mResX * tileMatrix.mTileWidth;
    const double dfMaxY =
        tileMatrix.mTopLeftY - iY * tileMatrix.mResY * tileMatrix.mTileHeight;
    const double dfMaxX = dfMinX + tileMatrix.mResX * tileMatrix.mTileWidth;
    const double dfMinY = dfMaxY - tileMatrix.mResY * tileMatrix.mTileHeight;

    const bool resamplingCompatibleOfTranslate =
        papszWarpOptions == nullptr &&
        (resampling == "nearest" || resampling == "average" ||
         resampling == "bilinear" || resampling == "cubic" ||
         resampling == "cubicspline" || resampling == "lanczos" ||
         resampling == "mode");

    const std::string osTmpFilename = bSupportsCreateOnlyVisibleAtCloseTime
                                          ? osFilename
                                          : osFilename + ".tmp." + pszExtension;

    if (resamplingCompatibleOfTranslate)
    {
        GDALGeoTransform upperGT;
        oSrcDS.GetGeoTransform(upperGT);
        const double dfMinXUpper = upperGT[0];
        const double dfMaxXUpper =
            dfMinXUpper + upperGT[1] * oSrcDS.GetRasterXSize();
        const double dfMaxYUpper = upperGT[3];
        const double dfMinYUpper =
            dfMaxYUpper + upperGT[5] * oSrcDS.GetRasterYSize();
        if (dfMinX >= dfMinXUpper && dfMaxX <= dfMaxXUpper &&
            dfMinY >= dfMinYUpper && dfMaxY <= dfMaxYUpper)
        {
            // If the overview tile is fully within the extent of the
            // upper zoom level, we can use GDALDataset::RasterIO() directly.

            const auto eDT = oSrcDS.GetRasterBand(1)->GetRasterDataType();
            const size_t nBytesPerBand =
                static_cast<size_t>(tileMatrix.mTileWidth) *
                tileMatrix.mTileHeight * GDALGetDataTypeSizeBytes(eDT);
            std::vector<GByte> dstBuffer(nBytesPerBand *
                                         oSrcDS.GetRasterCount());

            const double dfXOff = (dfMinX - dfMinXUpper) / upperGT[1];
            const double dfYOff = (dfMaxYUpper - dfMaxY) / -upperGT[5];
            const double dfXSize = (dfMaxX - dfMinX) / upperGT[1];
            const double dfYSize = (dfMaxY - dfMinY) / -upperGT[5];
            GDALRasterIOExtraArg sExtraArg;
            INIT_RASTERIO_EXTRA_ARG(sExtraArg);
            CPL_IGNORE_RET_VAL(sExtraArg.eResampleAlg);
            sExtraArg.eResampleAlg =
                GDALRasterIOGetResampleAlg(resampling.c_str());
            sExtraArg.dfXOff = dfXOff;
            sExtraArg.dfYOff = dfYOff;
            sExtraArg.dfXSize = dfXSize;
            sExtraArg.dfYSize = dfYSize;
            sExtraArg.bFloatingPointWindowValidity =
                sExtraArg.eResampleAlg != GRIORA_NearestNeighbour;
            constexpr double EPSILON = 1e-3;
            if (oSrcDS.RasterIO(GF_Read, static_cast<int>(dfXOff + EPSILON),
                                static_cast<int>(dfYOff + EPSILON),
                                static_cast<int>(dfXSize + 0.5),
                                static_cast<int>(dfYSize + 0.5),
                                dstBuffer.data(), tileMatrix.mTileWidth,
                                tileMatrix.mTileHeight, eDT,
                                oSrcDS.GetRasterCount(), nullptr, 0, 0, 0,
                                &sExtraArg) == CE_None)
            {
                int nDstBands = oSrcDS.GetRasterCount();
                const bool bDstHasAlpha =
                    oSrcDS.GetRasterBand(nDstBands)->GetColorInterpretation() ==
                    GCI_AlphaBand;
                if (bDstHasAlpha && bSkipBlank)
                {
                    bool bBlank = true;
                    for (size_t i = 0; i < nBytesPerBand && bBlank; ++i)
                    {
                        bBlank =
                            (dstBuffer[(nDstBands - 1) * nBytesPerBand + i] ==
                             0);
                    }
                    if (bBlank)
                        return true;
                    bSkipBlank = false;
                }
                if (bDstHasAlpha && !bUserAskedForAlpha)
                {
                    bool bAllOpaque = true;
                    for (size_t i = 0; i < nBytesPerBand && bAllOpaque; ++i)
                    {
                        bAllOpaque =
                            (dstBuffer[(nDstBands - 1) * nBytesPerBand + i] ==
                             255);
                    }
                    if (bAllOpaque)
                        nDstBands--;
                }

                auto memDS = std::unique_ptr<GDALDataset>(MEMDataset::Create(
                    "", tileMatrix.mTileWidth, tileMatrix.mTileHeight, 0, eDT,
                    nullptr));
                for (int i = 0; i < nDstBands; ++i)
                {
                    char szBuffer[32] = {'\0'};
                    int nRet = CPLPrintPointer(
                        szBuffer, dstBuffer.data() + i * nBytesPerBand,
                        sizeof(szBuffer));
                    szBuffer[nRet] = 0;

                    char szOption[64] = {'\0'};
                    snprintf(szOption, sizeof(szOption), "DATAPOINTER=%s",
                             szBuffer);

                    char *apszOptions[] = {szOption, nullptr};

                    memDS->AddBand(eDT, apszOptions);
                    auto poSrcBand = oSrcDS.GetRasterBand(i + 1);
                    auto poDstBand = memDS->GetRasterBand(i + 1);
                    poDstBand->SetColorInterpretation(
                        poSrcBand->GetColorInterpretation());
                    int bHasNoData = false;
                    const double dfNoData =
                        poSrcBand->GetNoDataValue(&bHasNoData);
                    if (bHasNoData)
                        poDstBand->SetNoDataValue(dfNoData);
                    if (auto poCT = poSrcBand->GetColorTable())
                        poDstBand->SetColorTable(poCT);
                }
                memDS->SetMetadata(oSrcDS.GetMetadata());
                memDS->SetGeoTransform(GDALGeoTransform(
                    dfMinX, tileMatrix.mResX, 0, dfMaxY, 0, -tileMatrix.mResY));

                memDS->SetSpatialRef(oSrcDS.GetSpatialRef());

                std::unique_ptr<CPLConfigOptionSetter> poSetter;
                // No need to reopen the dataset at end of CreateCopy() (for PNG
                // and JPEG) if we don't need to generate .aux.xml
                if (!bAuxXML)
                    poSetter = std::make_unique<CPLConfigOptionSetter>(
                        "GDAL_OPEN_AFTER_COPY", "NO", false);
                CPL_IGNORE_RET_VAL(poSetter);

                CPLStringList aosCreationOptions(creationOptions);
                if (bSupportsCreateOnlyVisibleAtCloseTime)
                    aosCreationOptions.SetNameValue(
                        "@CREATE_ONLY_VISIBLE_AT_CLOSE_TIME", "YES");
                poOutDS.reset(m_poDstDriver->CreateCopy(
                    osTmpFilename.c_str(), memDS.get(), false,
                    aosCreationOptions.List(), nullptr, nullptr));
            }
        }
        else
        {
            // If the overview tile is not fully within the extent of the
            // upper zoom level, use GDALTranslate() to use VRT padding

            aosOptions.AddString("-q");

            aosOptions.AddString("-projwin");
            aosOptions.AddString(CPLSPrintf("%.17g", dfMinX));
            aosOptions.AddString(CPLSPrintf("%.17g", dfMaxY));
            aosOptions.AddString(CPLSPrintf("%.17g", dfMaxX));
            aosOptions.AddString(CPLSPrintf("%.17g", dfMinY));

            aosOptions.AddString("-outsize");
            aosOptions.AddString(CPLSPrintf("%d", tileMatrix.mTileWidth));
            aosOptions.AddString(CPLSPrintf("%d", tileMatrix.mTileHeight));

            GDALTranslateOptions *psOptions =
                GDALTranslateOptionsNew(aosOptions.List(), nullptr);
            poOutDS.reset(GDALDataset::FromHandle(GDALTranslate(
                osTmpFilename.c_str(), GDALDataset::ToHandle(&oSrcDS),
                psOptions, nullptr)));
            GDALTranslateOptionsFree(psOptions);
        }
    }
    else
    {
        aosOptions.AddString("-te");
        aosOptions.AddString(CPLSPrintf("%.17g", dfMinX));
        aosOptions.AddString(CPLSPrintf("%.17g", dfMinY));
        aosOptions.AddString(CPLSPrintf("%.17g", dfMaxX));
        aosOptions.AddString(CPLSPrintf("%.17g", dfMaxY));

        aosOptions.AddString("-ts");
        aosOptions.AddString(CPLSPrintf("%d", tileMatrix.mTileWidth));
        aosOptions.AddString(CPLSPrintf("%d", tileMatrix.mTileHeight));

        for (int i = 0; papszWarpOptions && papszWarpOptions[i]; ++i)
        {
            aosOptions.AddString("-wo");
            aosOptions.AddString(papszWarpOptions[i]);
        }

        GDALWarpAppOptions *psOptions =
            GDALWarpAppOptionsNew(aosOptions.List(), nullptr);
        GDALDatasetH hSrcDS = GDALDataset::ToHandle(&oSrcDS);
        poOutDS.reset(GDALDataset::FromHandle(GDALWarp(
            osTmpFilename.c_str(), nullptr, 1, &hSrcDS, psOptions, nullptr)));
        GDALWarpAppOptionsFree(psOptions);
    }

    bool bRet = poOutDS != nullptr;
    if (bRet && bSkipBlank)
    {
        auto poLastBand = poOutDS->GetRasterBand(poOutDS->GetRasterCount());
        if (poLastBand->GetColorInterpretation() == GCI_AlphaBand)
        {
            std::vector<GByte> buffer(
                static_cast<size_t>(tileMatrix.mTileWidth) *
                tileMatrix.mTileHeight *
                GDALGetDataTypeSizeBytes(poLastBand->GetRasterDataType()));
            CPL_IGNORE_RET_VAL(poLastBand->RasterIO(
                GF_Read, 0, 0, tileMatrix.mTileWidth, tileMatrix.mTileHeight,
                buffer.data(), tileMatrix.mTileWidth, tileMatrix.mTileHeight,
                poLastBand->GetRasterDataType(), 0, 0, nullptr));
            bool bBlank = true;
            for (size_t i = 0; i < buffer.size() && bBlank; ++i)
            {
                bBlank = (buffer[i] == 0);
            }
            if (bBlank)
            {
                poOutDS.reset();
                VSIUnlink(osTmpFilename.c_str());
                if (bAuxXML)
                    VSIUnlink((osTmpFilename + ".aux.xml").c_str());
                return true;
            }
        }
    }
    bRet = bRet && poOutDS->Close() == CE_None;
    poOutDS.reset();
    if (bRet)
    {
        if (!bSupportsCreateOnlyVisibleAtCloseTime)
        {
            bRet = VSIRename(osTmpFilename.c_str(), osFilename.c_str()) == 0;
            if (bAuxXML)
            {
                VSIRename((osTmpFilename + ".aux.xml").c_str(),
                          (osFilename + ".aux.xml").c_str());
            }
        }
    }
    else
    {
        VSIUnlink(osTmpFilename.c_str());
    }
    return bRet;
}

namespace
{

/************************************************************************/
/*                        FakeMaxZoomRasterBand                         */
/************************************************************************/

class FakeMaxZoomRasterBand : public GDALRasterBand
{
    void *m_pDstBuffer = nullptr;
    CPL_DISALLOW_COPY_ASSIGN(FakeMaxZoomRasterBand)

  public:
    FakeMaxZoomRasterBand(int nBandIn, int nWidth, int nHeight,
                          int nBlockXSizeIn, int nBlockYSizeIn,
                          GDALDataType eDT, void *pDstBuffer)
        : m_pDstBuffer(pDstBuffer)
    {
        nBand = nBandIn;
        nRasterXSize = nWidth;
        nRasterYSize = nHeight;
        nBlockXSize = nBlockXSizeIn;
        nBlockYSize = nBlockYSizeIn;
        eDataType = eDT;
    }

    CPLErr IReadBlock(int, int, void *) override
    {
        CPLAssert(false);
        return CE_Failure;
    }

#ifdef DEBUG
    CPLErr IWriteBlock(int, int, void *) override
    {
        CPLAssert(false);
        return CE_Failure;
    }
#endif

    CPLErr IRasterIO(GDALRWFlag eRWFlag, [[maybe_unused]] int nXOff,
                     [[maybe_unused]] int nYOff, [[maybe_unused]] int nXSize,
                     [[maybe_unused]] int nYSize, void *pData,
                     [[maybe_unused]] int nBufXSize,
                     [[maybe_unused]] int nBufYSize, GDALDataType eBufType,
                     GSpacing nPixelSpace, [[maybe_unused]] GSpacing nLineSpace,
                     GDALRasterIOExtraArg *) override
    {
        // For sake of implementation simplicity, check various assumptions of
        // how GDALAlphaMask code does I/O
        CPLAssert((nXOff % nBlockXSize) == 0);
        CPLAssert((nYOff % nBlockYSize) == 0);
        CPLAssert(nXSize == nBufXSize);
        CPLAssert(nXSize == nBlockXSize);
        CPLAssert(nYSize == nBufYSize);
        CPLAssert(nYSize == nBlockYSize);
        CPLAssert(nLineSpace == nBlockXSize * nPixelSpace);
        CPLAssert(
            nBand ==
            poDS->GetRasterCount());  // only alpha band is accessed this way
        if (eRWFlag == GF_Read)
        {
            double dfZero = 0;
            GDALCopyWords64(&dfZero, GDT_Float64, 0, pData, eBufType,
                            static_cast<int>(nPixelSpace),
                            static_cast<size_t>(nBlockXSize) * nBlockYSize);
        }
        else
        {
            GDALCopyWords64(pData, eBufType, static_cast<int>(nPixelSpace),
                            m_pDstBuffer, eDataType,
                            GDALGetDataTypeSizeBytes(eDataType),
                            static_cast<size_t>(nBlockXSize) * nBlockYSize);
        }
        return CE_None;
    }
};

/************************************************************************/
/*                          FakeMaxZoomDataset                          */
/************************************************************************/

// This class is used to create a fake output dataset for GDALWarpOperation.
// In particular we need to implement GDALRasterBand::IRasterIO(GF_Write, ...)
// to catch writes (of one single tile) to the alpha band and redirect them
// to the dstBuffer passed to FakeMaxZoomDataset constructor.

class FakeMaxZoomDataset : public GDALDataset
{
    const int m_nBlockXSize;
    const int m_nBlockYSize;
    const OGRSpatialReference m_oSRS;
    const GDALGeoTransform m_gt{};

  public:
    FakeMaxZoomDataset(int nWidth, int nHeight, int nBandsIn, int nBlockXSize,
                       int nBlockYSize, GDALDataType eDT,
                       const GDALGeoTransform &gt,
                       const OGRSpatialReference &oSRS,
                       std::vector<GByte> &dstBuffer)
        : m_nBlockXSize(nBlockXSize), m_nBlockYSize(nBlockYSize), m_oSRS(oSRS),
          m_gt(gt)
    {
        eAccess = GA_Update;
        nRasterXSize = nWidth;
        nRasterYSize = nHeight;
        for (int i = 1; i <= nBandsIn; ++i)
        {
            SetBand(i,
                    new FakeMaxZoomRasterBand(
                        i, nWidth, nHeight, nBlockXSize, nBlockYSize, eDT,
                        dstBuffer.data() + static_cast<size_t>(i - 1) *
                                               nBlockXSize * nBlockYSize *
                                               GDALGetDataTypeSizeBytes(eDT)));
        }
    }

    const OGRSpatialReference *GetSpatialRef() const override
    {
        return m_oSRS.IsEmpty() ? nullptr : &m_oSRS;
    }

    CPLErr GetGeoTransform(GDALGeoTransform &gt) const override
    {
        gt = m_gt;
        return CE_None;
    }

    using GDALDataset::Clone;

    std::unique_ptr<FakeMaxZoomDataset>
    Clone(std::vector<GByte> &dstBuffer) const
    {
        return std::make_unique<FakeMaxZoomDataset>(
            nRasterXSize, nRasterYSize, nBands, m_nBlockXSize, m_nBlockYSize,
            GetRasterBand(1)->GetRasterDataType(), m_gt, m_oSRS, dstBuffer);
    }
};

/************************************************************************/
/*                           MosaicRasterBand                           */
/************************************************************************/

class MosaicRasterBand : public GDALRasterBand
{
    const int m_tileMinX;
    const int m_tileMinY;
    const GDALColorInterp m_eColorInterp;
    const gdal::TileMatrixSet::TileMatrix m_oTM;
    const std::string m_convention;
    const std::string m_directory;
    const std::string m_extension;
    const bool m_hasNoData;
    const double m_noData;
    std::unique_ptr<GDALColorTable> m_poColorTable{};

  public:
    MosaicRasterBand(GDALDataset *poDSIn, int nBandIn, int nWidth, int nHeight,
                     int nBlockXSizeIn, int nBlockYSizeIn, GDALDataType eDT,
                     GDALColorInterp eColorInterp, int nTileMinX, int nTileMinY,
                     const gdal::TileMatrixSet::TileMatrix &oTM,
                     const std::string &convention,
                     const std::string &directory, const std::string &extension,
                     const double *pdfDstNoData,
                     const GDALColorTable *poColorTable)
        : m_tileMinX(nTileMinX), m_tileMinY(nTileMinY),
          m_eColorInterp(eColorInterp), m_oTM(oTM), m_convention(convention),
          m_directory(directory), m_extension(extension),
          m_hasNoData(pdfDstNoData != nullptr),
          m_noData(pdfDstNoData ? *pdfDstNoData : 0),
          m_poColorTable(poColorTable ? poColorTable->Clone() : nullptr)
    {
        poDS = poDSIn;
        nBand = nBandIn;
        nRasterXSize = nWidth;
        nRasterYSize = nHeight;
        nBlockXSize = nBlockXSizeIn;
        nBlockYSize = nBlockYSizeIn;
        eDataType = eDT;
    }

    CPLErr IReadBlock(int nXBlock, int nYBlock, void *pData) override;

    GDALColorTable *GetColorTable() override
    {
        return m_poColorTable.get();
    }

    GDALColorInterp GetColorInterpretation() override
    {
        return m_eColorInterp;
    }

    double GetNoDataValue(int *pbHasNoData) override
    {
        if (pbHasNoData)
            *pbHasNoData = m_hasNoData;
        return m_noData;
    }
};

/************************************************************************/
/*                            MosaicDataset                             */
/************************************************************************/

// This class is to expose the tiles of a given level as a mosaic that
// can be used as a source to generate the immediately below zoom level.

class MosaicDataset : public GDALDataset
{
    friend class MosaicRasterBand;

    const std::string m_directory;
    const std::string m_extension;
    const std::string m_format;
    const std::vector<GDALColorInterp> m_aeColorInterp;
    const gdal::TileMatrixSet::TileMatrix &m_oTM;
    const OGRSpatialReference m_oSRS;
    const int m_nTileMinX;
    const int m_nTileMinY;
    const int m_nTileMaxX;
    const int m_nTileMaxY;
    const std::string m_convention;
    const GDALDataType m_eDT;
    const double *const m_pdfDstNoData;
    const std::vector<std::string> &m_metadata;
    const GDALColorTable *const m_poCT;

    GDALGeoTransform m_gt{};
    const int m_nMaxCacheTileSize;
    lru11::Cache<std::string, std::shared_ptr<GDALDataset>> m_oCacheTile;

    CPL_DISALLOW_COPY_ASSIGN(MosaicDataset)

  public:
    MosaicDataset(const std::string &directory, const std::string &extension,
                  const std::string &format,
                  const std::vector<GDALColorInterp> &aeColorInterp,
                  const gdal::TileMatrixSet::TileMatrix &oTM,
                  const OGRSpatialReference &oSRS, int nTileMinX, int nTileMinY,
                  int nTileMaxX, int nTileMaxY, const std::string &convention,
                  int nBandsIn, GDALDataType eDT, const double *pdfDstNoData,
                  const std::vector<std::string> &metadata,
                  const GDALColorTable *poCT, int maxCacheTileSize)
        : m_directory(directory), m_extension(extension), m_format(format),
          m_aeColorInterp(aeColorInterp), m_oTM(oTM), m_oSRS(oSRS),
          m_nTileMinX(nTileMinX), m_nTileMinY(nTileMinY),
          m_nTileMaxX(nTileMaxX), m_nTileMaxY(nTileMaxY),
          m_convention(convention), m_eDT(eDT), m_pdfDstNoData(pdfDstNoData),
          m_metadata(metadata), m_poCT(poCT),
          m_nMaxCacheTileSize(maxCacheTileSize),
          m_oCacheTile(/* max_size = */ maxCacheTileSize, /* elasticity = */ 0)
    {
        nRasterXSize = (nTileMaxX - nTileMinX + 1) * oTM.mTileWidth;
        nRasterYSize = (nTileMaxY - nTileMinY + 1) * oTM.mTileHeight;
        m_gt.xorig = oTM.mTopLeftX + nTileMinX * oTM.mResX * oTM.mTileWidth;
        m_gt.xscale = oTM.mResX;
        m_gt.xrot = 0;
        m_gt.yorig = oTM.mTopLeftY - nTileMinY * oTM.mResY * oTM.mTileHeight;
        m_gt.yrot = 0;
        m_gt.yscale = -oTM.mResY;
        for (int i = 1; i <= nBandsIn; ++i)
        {
            const GDALColorInterp eColorInterp =
                (i <= static_cast<int>(m_aeColorInterp.size()))
                    ? m_aeColorInterp[i - 1]
                    : GCI_AlphaBand;
            SetBand(i, new MosaicRasterBand(
                           this, i, nRasterXSize, nRasterYSize, oTM.mTileWidth,
                           oTM.mTileHeight, eDT, eColorInterp, nTileMinX,
                           nTileMinY, oTM, convention, directory, extension,
                           pdfDstNoData, poCT));
        }
        SetMetadataItem("INTERLEAVE", "PIXEL", "IMAGE_STRUCTURE");
        const CPLStringList aosMD(metadata);
        for (const auto [key, value] : cpl::IterateNameValue(aosMD))
        {
            SetMetadataItem(key, value);
        }
    }

    const OGRSpatialReference *GetSpatialRef() const override
    {
        return m_oSRS.IsEmpty() ? nullptr : &m_oSRS;
    }

    CPLErr GetGeoTransform(GDALGeoTransform &gt) const override
    {
        gt = m_gt;
        return CE_None;
    }

    using GDALDataset::Clone;

    std::unique_ptr<MosaicDataset> Clone() const
    {
        return std::make_unique<MosaicDataset>(
            m_directory, m_extension, m_format, m_aeColorInterp, m_oTM, m_oSRS,
            m_nTileMinX, m_nTileMinY, m_nTileMaxX, m_nTileMaxY, m_convention,
            nBands, m_eDT, m_pdfDstNoData, m_metadata, m_poCT,
            m_nMaxCacheTileSize);
    }
};

/************************************************************************/
/*                    MosaicRasterBand::IReadBlock()                    */
/************************************************************************/

CPLErr MosaicRasterBand::IReadBlock(int nXBlock, int nYBlock, void *pData)
{
    auto poThisDS = cpl::down_cast<MosaicDataset *>(poDS);
    std::string filename = CPLFormFilenameSafe(
        m_directory.c_str(), CPLSPrintf("%d", m_tileMinX + nXBlock), nullptr);
    const int iFileY = GetFileY(m_tileMinY + nYBlock, m_oTM, m_convention);
    filename = CPLFormFilenameSafe(filename.c_str(), CPLSPrintf("%d", iFileY),
                                   m_extension.c_str());

    std::shared_ptr<GDALDataset> poTileDS;
    if (!poThisDS->m_oCacheTile.tryGet(filename, poTileDS))
    {
        const char *const apszAllowedDrivers[] = {poThisDS->m_format.c_str(),
                                                  nullptr};
        const char *const apszAllowedDriversForCOG[] = {"GTiff", "LIBERTIFF",
                                                        nullptr};
        // CPLDebugOnly("gdal_raster_tile", "Opening %s", filename.c_str());
        poTileDS.reset(GDALDataset::Open(
            filename.c_str(), GDAL_OF_RASTER | GDAL_OF_INTERNAL,
            EQUAL(poThisDS->m_format.c_str(), "COG") ? apszAllowedDriversForCOG
                                                     : apszAllowedDrivers));
        if (!poTileDS)
        {
            VSIStatBufL sStat;
            if (VSIStatL(filename.c_str(), &sStat) == 0)
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "File %s exists but cannot be opened with %s driver",
                         filename.c_str(), poThisDS->m_format.c_str());
                return CE_Failure;
            }
        }
        poThisDS->m_oCacheTile.insert(filename, poTileDS);
    }
    if (!poTileDS || nBand > poTileDS->GetRasterCount())
    {
        memset(pData,
               (poTileDS && (nBand == poTileDS->GetRasterCount() + 1)) ? 255
                                                                       : 0,
               static_cast<size_t>(nBlockXSize) * nBlockYSize *
                   GDALGetDataTypeSizeBytes(eDataType));
        return CE_None;
    }
    else
    {
        return poTileDS->GetRasterBand(nBand)->RasterIO(
            GF_Read, 0, 0, nBlockXSize, nBlockYSize, pData, nBlockXSize,
            nBlockYSize, eDataType, 0, 0, nullptr);
    }
}

}  // namespace

/************************************************************************/
/*                         ApplySubstitutions()                         */
/************************************************************************/

static void ApplySubstitutions(CPLString &s,
                               const std::map<std::string, std::string> &substs)
{
    for (const auto &[key, value] : substs)
    {
        s.replaceAll("%(" + key + ")s", value);
        s.replaceAll("%(" + key + ")d", value);
        s.replaceAll("%(" + key + ")f", value);
        s.replaceAll("${" + key + "}", value);
    }
}

/************************************************************************/
/*                          GenerateLeaflet()                           */
/************************************************************************/

static void GenerateLeaflet(const std::string &osDirectory,
                            const std::string &osTitle, double dfSouthLat,
                            double dfWestLon, double dfNorthLat,
                            double dfEastLon, int nMinZoom, int nMaxZoom,
                            int nTileSize, const std::string &osExtension,
                            const std::string &osURL,
                            const std::string &osCopyright, bool bXYZ)
{
    if (const char *pszTemplate = CPLFindFile("gdal", "leaflet_template.html"))
    {
        const std::string osFilename(pszTemplate);
        std::map<std::string, std::string> substs;

        // For tests
        const char *pszFmt =
            atoi(CPLGetConfigOption("GDAL_RASTER_TILE_HTML_PREC", "17")) == 10
                ? "%.10g"
                : "%.17g";

        substs["double_quote_escaped_title"] =
            CPLString(osTitle).replaceAll('"', "\\\"");
        char *pszStr = CPLEscapeString(osTitle.c_str(), -1, CPLES_XML);
        substs["xml_escaped_title"] = pszStr;
        CPLFree(pszStr);
        substs["south"] = CPLSPrintf(pszFmt, dfSouthLat);
        substs["west"] = CPLSPrintf(pszFmt, dfWestLon);
        substs["north"] = CPLSPrintf(pszFmt, dfNorthLat);
        substs["east"] = CPLSPrintf(pszFmt, dfEastLon);
        substs["centerlon"] = CPLSPrintf(pszFmt, (dfNorthLat + dfSouthLat) / 2);
        substs["centerlat"] = CPLSPrintf(pszFmt, (dfWestLon + dfEastLon) / 2);
        substs["minzoom"] = CPLSPrintf("%d", nMinZoom);
        substs["maxzoom"] = CPLSPrintf("%d", nMaxZoom);
        substs["beginzoom"] = CPLSPrintf("%d", nMaxZoom);
        substs["tile_size"] = CPLSPrintf("%d", nTileSize);  // not used
        substs["tileformat"] = osExtension;
        substs["publishurl"] = osURL;  // not used
        substs["copyright"] = CPLString(osCopyright).replaceAll('"', "\\\"");
        substs["tms"] = bXYZ ? "0" : "1";

        GByte *pabyRet = nullptr;
        CPL_IGNORE_RET_VAL(VSIIngestFile(nullptr, osFilename.c_str(), &pabyRet,
                                         nullptr, 10 * 1024 * 1024));
        if (pabyRet)
        {
            CPLString osHTML(reinterpret_cast<char *>(pabyRet));
            CPLFree(pabyRet);

            ApplySubstitutions(osHTML, substs);

            VSILFILE *f = VSIFOpenL(CPLFormFilenameSafe(osDirectory.c_str(),
                                                        "leaflet.html", nullptr)
                                        .c_str(),
                                    "wb");
            if (f)
            {
                VSIFWriteL(osHTML.data(), 1, osHTML.size(), f);
                VSIFCloseL(f);
            }
        }
    }
}

/************************************************************************/
/*                           GenerateMapML()                            */
/************************************************************************/

static void
GenerateMapML(const std::string &osDirectory, const std::string &mapmlTemplate,
              const std::string &osTitle, int nMinTileX, int nMinTileY,
              int nMaxTileX, int nMaxTileY, int nMinZoom, int nMaxZoom,
              const std::string &osExtension, const std::string &osURL,
              const std::string &osCopyright, const gdal::TileMatrixSet &tms)
{
    if (const char *pszTemplate =
            (mapmlTemplate.empty() ? CPLFindFile("gdal", "template_tiles.mapml")
                                   : mapmlTemplate.c_str()))
    {
        const std::string osFilename(pszTemplate);
        std::map<std::string, std::string> substs;

        if (tms.identifier() == "GoogleMapsCompatible")
            substs["TILING_SCHEME"] = "OSMTILE";
        else if (tms.identifier() == "WorldCRS84Quad")
            substs["TILING_SCHEME"] = "WGS84";
        else
            substs["TILING_SCHEME"] = tms.identifier();

        substs["URL"] = osURL.empty() ? "./" : osURL;
        substs["MINTILEX"] = CPLSPrintf("%d", nMinTileX);
        substs["MINTILEY"] = CPLSPrintf("%d", nMinTileY);
        substs["MAXTILEX"] = CPLSPrintf("%d", nMaxTileX);
        substs["MAXTILEY"] = CPLSPrintf("%d", nMaxTileY);
        substs["CURZOOM"] = CPLSPrintf("%d", nMaxZoom);
        substs["MINZOOM"] = CPLSPrintf("%d", nMinZoom);
        substs["MAXZOOM"] = CPLSPrintf("%d", nMaxZoom);
        substs["TILEEXT"] = osExtension;
        char *pszStr = CPLEscapeString(osTitle.c_str(), -1, CPLES_XML);
        substs["TITLE"] = pszStr;
        CPLFree(pszStr);
        substs["COPYRIGHT"] = osCopyright;

        GByte *pabyRet = nullptr;
        CPL_IGNORE_RET_VAL(VSIIngestFile(nullptr, osFilename.c_str(), &pabyRet,
                                         nullptr, 10 * 1024 * 1024));
        if (pabyRet)
        {
            CPLString osMAPML(reinterpret_cast<char *>(pabyRet));
            CPLFree(pabyRet);

            ApplySubstitutions(osMAPML, substs);

            VSILFILE *f = VSIFOpenL(
                CPLFormFilenameSafe(osDirectory.c_str(), "mapml.mapml", nullptr)
                    .c_str(),
                "wb");
            if (f)
            {
                VSIFWriteL(osMAPML.data(), 1, osMAPML.size(), f);
                VSIFCloseL(f);
            }
        }
    }
}

/************************************************************************/
/*                            GenerateSTAC()                            */
/************************************************************************/

static void
GenerateSTAC(const std::string &osDirectory, const std::string &osTitle,
             double dfWestLon, double dfSouthLat, double dfEastLon,
             double dfNorthLat, const std::vector<std::string> &metadata,
             const std::vector<BandMetadata> &aoBandMetadata, int nMinZoom,
             int nMaxZoom, const std::string &osExtension,
             const std::string &osFormat, const std::string &osURL,
             const std::string &osCopyright, const OGRSpatialReference &oSRS,
             const gdal::TileMatrixSet &tms, bool bInvertAxisTMS, int tileSize,
             const double adfExtent[4], const GDALArgDatasetValue &dataset)
{
    CPLJSONObject oRoot;
    oRoot["stac_version"] = "1.1.0";
    CPLJSONArray oExtensions;
    oRoot["stac_extensions"] = oExtensions;
    oRoot["id"] = osTitle;
    oRoot["type"] = "Feature";
    oRoot["bbox"] = {dfWestLon, dfSouthLat, dfEastLon, dfNorthLat};
    CPLJSONObject oGeometry;

    const auto BuildPolygon = [](double x1, double y1, double x2, double y2)
    {
        return CPLJSONArray::Build({CPLJSONArray::Build(
            {CPLJSONArray::Build({x1, y1}), CPLJSONArray::Build({x1, y2}),
             CPLJSONArray::Build({x2, y2}), CPLJSONArray::Build({x2, y1}),
             CPLJSONArray::Build({x1, y1})})});
    };

    if (dfWestLon <= dfEastLon)
    {
        oGeometry["type"] = "Polygon";
        oGeometry["coordinates"] =
            BuildPolygon(dfWestLon, dfSouthLat, dfEastLon, dfNorthLat);
    }
    else
    {
        oGeometry["type"] = "MultiPolygon";
        oGeometry["coordinates"] = {
            BuildPolygon(dfWestLon, dfSouthLat, 180.0, dfNorthLat),
            BuildPolygon(-180.0, dfSouthLat, dfEastLon, dfNorthLat)};
    }
    oRoot["geometry"] = std::move(oGeometry);

    CPLJSONObject oProperties;
    oRoot["properties"] = oProperties;
    const CPLStringList aosMD(metadata);
    std::string osDateTime = "1970-01-01T00:00:00.000Z";
    if (!dataset.GetName().empty())
    {
        VSIStatBufL sStat;
        if (VSIStatL(dataset.GetName().c_str(), &sStat) == 0 &&
            sStat.st_mtime != 0)
        {
            struct tm tm;
            CPLUnixTimeToYMDHMS(sStat.st_mtime, &tm);
            osDateTime = CPLSPrintf(
                "%04d-%02d-%02dT%02d:%02d:%02dZ", tm.tm_year + 1900,
                tm.tm_mon + 1, tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec);
        }
    }
    std::string osStartDateTime = "0001-01-01T00:00:00.000Z";
    std::string osEndDateTime = "9999-12-31T23:59:59.999Z";

    const auto GetDateTimeAsISO8211 = [](const char *pszInput)
    {
        std::string osRet;
        OGRField sField;
        if (OGRParseDate(pszInput, &sField, 0))
        {
            char *pszDT = OGRGetXMLDateTime(&sField);
            if (pszDT)
                osRet = pszDT;
            CPLFree(pszDT);
        }
        return osRet;
    };

    for (const auto &[key, value] : cpl::IterateNameValue(aosMD))
    {
        if (EQUAL(key, "datetime"))
        {
            std::string osTmp = GetDateTimeAsISO8211(value);
            if (!osTmp.empty())
            {
                osDateTime = std::move(osTmp);
                continue;
            }
        }
        else if (EQUAL(key, "start_datetime"))
        {
            std::string osTmp = GetDateTimeAsISO8211(value);
            if (!osTmp.empty())
            {
                osStartDateTime = std::move(osTmp);
                continue;
            }
        }
        else if (EQUAL(key, "end_datetime"))
        {
            std::string osTmp = GetDateTimeAsISO8211(value);
            if (!osTmp.empty())
            {
                osEndDateTime = std::move(osTmp);
                continue;
            }
        }
        else if (EQUAL(key, "TIFFTAG_DATETIME"))
        {
            int nYear, nMonth, nDay, nHour, nMin, nSec;
            if (sscanf(value, "%04d:%02d:%02d %02d:%02d:%02d", &nYear, &nMonth,
                       &nDay, &nHour, &nMin, &nSec) == 6)
            {
                osDateTime = CPLSPrintf("%04d-%02d-%02dT%02d:%02d:%02dZ", nYear,
                                        nMonth, nDay, nHour, nMin, nSec);
                continue;
            }
        }

        oProperties[key] = value;
    }
    oProperties["datetime"] = osDateTime;
    oProperties["start_datetime"] = osStartDateTime;
    oProperties["end_datetime"] = osEndDateTime;
    if (!osCopyright.empty())
        oProperties["copyright"] = osCopyright;

    // Just keep the tile matrix zoom levels we use
    gdal::TileMatrixSet tmsLimitedToZoomLevelUsed(tms);
    auto &tileMatrixList = tmsLimitedToZoomLevelUsed.tileMatrixList();
    tileMatrixList.erase(tileMatrixList.begin() + nMaxZoom + 1,
                         tileMatrixList.end());
    tileMatrixList.erase(tileMatrixList.begin(),
                         tileMatrixList.begin() + nMinZoom);

    CPLJSONObject oLimits;
    // Patch their definition with the potentially overridden tileSize.
    for (auto &tm : tileMatrixList)
    {
        int nOvrMinTileX = 0;
        int nOvrMinTileY = 0;
        int nOvrMaxTileX = 0;
        int nOvrMaxTileY = 0;
        bool bIntersects = false;
        CPL_IGNORE_RET_VAL(GetTileIndices(
            tm, bInvertAxisTMS, tileSize, adfExtent, nOvrMinTileX, nOvrMinTileY,
            nOvrMaxTileX, nOvrMaxTileY, /* noIntersectionIsOK = */ true,
            bIntersects));

        CPLJSONObject oLimit;
        oLimit["min_tile_col"] = nOvrMinTileX;
        oLimit["max_tile_col"] = nOvrMaxTileX;
        oLimit["min_tile_row"] = nOvrMinTileY;
        oLimit["max_tile_row"] = nOvrMaxTileY;
        oLimits[tm.mId] = std::move(oLimit);
    }

    CPLJSONObject oTilesTileMatrixSets;
    {
        CPLJSONDocument oDoc;
        CPL_IGNORE_RET_VAL(
            oDoc.LoadMemory(tmsLimitedToZoomLevelUsed.exportToTMSJsonV1()));
        oTilesTileMatrixSets[tmsLimitedToZoomLevelUsed.identifier()] =
            oDoc.GetRoot();
    }
    oProperties["tiles:tile_matrix_sets"] = std::move(oTilesTileMatrixSets);

    CPLJSONObject oTilesTileMatrixLinks;
    CPLJSONObject oTilesTileMatrixLink;
    oTilesTileMatrixLink["url"] =
        std::string("#").append(tmsLimitedToZoomLevelUsed.identifier());
    oTilesTileMatrixLink["limits"] = std::move(oLimits);
    oTilesTileMatrixLinks[tmsLimitedToZoomLevelUsed.identifier()] =
        std::move(oTilesTileMatrixLink);
    oProperties["tiles:tile_matrix_links"] = std::move(oTilesTileMatrixLinks);

    const char *pszAuthName = oSRS.GetAuthorityName(nullptr);
    const char *pszAuthCode = oSRS.GetAuthorityCode(nullptr);
    if (pszAuthName && pszAuthCode)
    {
        oProperties["proj:code"] =
            std::string(pszAuthName).append(":").append(pszAuthCode);
    }
    else
    {
        char *pszPROJJSON = nullptr;
        CPL_IGNORE_RET_VAL(oSRS.exportToPROJJSON(&pszPROJJSON, nullptr));
        if (pszPROJJSON)
        {
            CPLJSONDocument oDoc;
            CPL_IGNORE_RET_VAL(oDoc.LoadMemory(pszPROJJSON));
            CPLFree(pszPROJJSON);
            oProperties["proj:projjson"] = oDoc.GetRoot();
        }
    }
    {
        auto ovrTileMatrix = tms.tileMatrixList()[nMaxZoom];
        int nOvrMinTileX = 0;
        int nOvrMinTileY = 0;
        int nOvrMaxTileX = 0;
        int nOvrMaxTileY = 0;
        bool bIntersects = false;
        CPL_IGNORE_RET_VAL(GetTileIndices(
            ovrTileMatrix, bInvertAxisTMS, tileSize, adfExtent, nOvrMinTileX,
            nOvrMinTileY, nOvrMaxTileX, nOvrMaxTileY,
            /* noIntersectionIsOK = */ true, bIntersects));
        oProperties["proj:shape"] = {
            (nOvrMaxTileY - nOvrMinTileY + 1) * ovrTileMatrix.mTileHeight,
            (nOvrMaxTileX - nOvrMinTileX + 1) * ovrTileMatrix.mTileWidth};

        oProperties["proj:transform"] = {
            ovrTileMatrix.mResX,
            0.0,
            ovrTileMatrix.mTopLeftX +
                nOvrMinTileX * ovrTileMatrix.mTileWidth * ovrTileMatrix.mResX,
            0.0,
            -ovrTileMatrix.mResY,
            ovrTileMatrix.mTopLeftY +
                nOvrMinTileY * ovrTileMatrix.mTileHeight * ovrTileMatrix.mResY,
            0.0,
            0.0,
            0.0};
    }

    constexpr const char *ASSET_NAME = "bands";

    CPLJSONObject oAssetTemplates;
    oRoot["asset_templates"] = oAssetTemplates;

    CPLJSONObject oAssetTemplate;
    oAssetTemplates[ASSET_NAME] = oAssetTemplate;

    std::string osHref = (osURL.empty() ? std::string(".") : std::string(osURL))
                             .append("/{TileMatrix}/{TileCol}/{TileRow}.")
                             .append(osExtension);

    const std::map<std::string, std::string> oMapVSIToURIPrefix = {
        {"vsis3", "s3://"},
        {"vsigs", "gs://"},
        {"vsiaz", "az://"},  // Not universally recognized
    };

    const CPLStringList aosSplitHref(
        CSLTokenizeString2(osHref.c_str(), "/", 0));
    if (!aosSplitHref.empty())
    {
        const auto oIter = oMapVSIToURIPrefix.find(aosSplitHref[0]);
        if (oIter != oMapVSIToURIPrefix.end())
        {
            // +2 because of 2 slash characters
            osHref = std::string(oIter->second)
                         .append(osHref.c_str() + strlen(aosSplitHref[0]) + 2);
        }
    }
    oAssetTemplate["href"] = osHref;

    if (EQUAL(osFormat.c_str(), "COG"))
        oAssetTemplate["type"] =
            "image/tiff; application=geotiff; profile=cloud-optimized";
    else if (osExtension == "tif")
        oAssetTemplate["type"] = "image/tiff; application=geotiff";
    else if (osExtension == "png")
        oAssetTemplate["type"] = "image/png";
    else if (osExtension == "jpg")
        oAssetTemplate["type"] = "image/jpeg";
    else if (osExtension == "webp")
        oAssetTemplate["type"] = "image/webp";

    const std::map<GDALDataType, const char *> oMapDTToStac = {
        {GDT_Int8, "int8"},
        {GDT_Int16, "int16"},
        {GDT_Int32, "int32"},
        {GDT_Int64, "int64"},
        {GDT_UInt8, "uint8"},
        {GDT_UInt16, "uint16"},
        {GDT_UInt32, "uint32"},
        {GDT_UInt64, "uint64"},
        // float16: 16-bit float; unhandled
        {GDT_Float32, "float32"},
        {GDT_Float64, "float64"},
        {GDT_CInt16, "cint16"},
        {GDT_CInt32, "cint32"},
        // cfloat16: complex 16-bit float; unhandled
        {GDT_CFloat32, "cfloat32"},
        {GDT_CFloat64, "cfloat64"},
    };

    CPLJSONArray oBands;
    int iBand = 1;
    bool bEOExtensionUsed = false;
    for (const auto &bandMD : aoBandMetadata)
    {
        CPLJSONObject oBand;
        oBand["name"] = bandMD.osDescription.empty()
                            ? std::string(CPLSPrintf("Band%d", iBand))
                            : bandMD.osDescription;

        const auto oIter = oMapDTToStac.find(bandMD.eDT);
        if (oIter != oMapDTToStac.end())
            oBand["data_type"] = oIter->second;

        if (const char *pszCommonName =
                GDALGetSTACCommonNameFromColorInterp(bandMD.eColorInterp))
        {
            bEOExtensionUsed = true;
            oBand["eo:common_name"] = pszCommonName;
        }
        if (!bandMD.osCenterWaveLength.empty() && !bandMD.osFWHM.empty())
        {
            bEOExtensionUsed = true;
            oBand["eo:center_wavelength"] =
                CPLAtof(bandMD.osCenterWaveLength.c_str());
            oBand["eo:full_width_half_max"] = CPLAtof(bandMD.osFWHM.c_str());
        }
        ++iBand;
        oBands.Add(oBand);
    }
    oAssetTemplate["bands"] = oBands;

    oRoot.Add("assets", CPLJSONObject());
    oRoot.Add("links", CPLJSONArray());

    oExtensions.Add(
        "https://stac-extensions.github.io/tiled-assets/v1.0.0/schema.json");
    oExtensions.Add(
        "https://stac-extensions.github.io/projection/v2.0.0/schema.json");
    if (bEOExtensionUsed)
        oExtensions.Add(
            "https://stac-extensions.github.io/eo/v2.0.0/schema.json");

    // Serialize JSON document to file
    const std::string osJSON =
        CPLString(oRoot.Format(CPLJSONObject::PrettyFormat::Pretty))
            .replaceAll("\\/", '/');
    VSILFILE *f = VSIFOpenL(
        CPLFormFilenameSafe(osDirectory.c_str(), "stacta.json", nullptr)
            .c_str(),
        "wb");
    if (f)
    {
        VSIFWriteL(osJSON.data(), 1, osJSON.size(), f);
        VSIFCloseL(f);
    }
}

/************************************************************************/
/*                         GenerateOpenLayers()                         */
/************************************************************************/

static void GenerateOpenLayers(
    const std::string &osDirectory, const std::string &osTitle, double dfMinX,
    double dfMinY, double dfMaxX, double dfMaxY, int nMinZoom, int nMaxZoom,
    int nTileSize, const std::string &osExtension, const std::string &osURL,
    const std::string &osCopyright, const gdal::TileMatrixSet &tms,
    bool bInvertAxisTMS, const OGRSpatialReference &oSRS_TMS, bool bXYZ)
{
    std::map<std::string, std::string> substs;

    // For tests
    const char *pszFmt =
        atoi(CPLGetConfigOption("GDAL_RASTER_TILE_HTML_PREC", "17")) == 10
            ? "%.10g"
            : "%.17g";

    char *pszStr = CPLEscapeString(osTitle.c_str(), -1, CPLES_XML);
    substs["xml_escaped_title"] = pszStr;
    CPLFree(pszStr);
    substs["ominx"] = CPLSPrintf(pszFmt, dfMinX);
    substs["ominy"] = CPLSPrintf(pszFmt, dfMinY);
    substs["omaxx"] = CPLSPrintf(pszFmt, dfMaxX);
    substs["omaxy"] = CPLSPrintf(pszFmt, dfMaxY);
    substs["center_x"] = CPLSPrintf(pszFmt, (dfMinX + dfMaxX) / 2);
    substs["center_y"] = CPLSPrintf(pszFmt, (dfMinY + dfMaxY) / 2);
    substs["minzoom"] = CPLSPrintf("%d", nMinZoom);
    substs["maxzoom"] = CPLSPrintf("%d", nMaxZoom);
    substs["tile_size"] = CPLSPrintf("%d", nTileSize);
    substs["tileformat"] = osExtension;
    substs["publishurl"] = osURL;
    substs["copyright"] = osCopyright;
    substs["sign_y"] = bXYZ ? "" : "-";

    CPLString s(R"raw(<!DOCTYPE html>
<html>
<head>
    <title>%(xml_escaped_title)s</title>
    <meta http-equiv="content-type" content="text/html; charset=utf-8"/>
    <meta http-equiv='imagetoolbar' content='no'/>
    <style type="text/css"> v\:* {behavior:url(#default#VML);}
        html, body { overflow: hidden; padding: 0; height: 100%; width: 100%; font-family: 'Lucida Grande',Geneva,Arial,Verdana,sans-serif; }
        body { margin: 10px; background: #fff; }
        h1 { margin: 0; padding: 6px; border:0; font-size: 20pt; }
        #header { height: 43px; padding: 0; background-color: #eee; border: 1px solid #888; }
        #subheader { height: 12px; text-align: right; font-size: 10px; color: #555;}
        #map { height: 90%; border: 1px solid #888; }
    </style>
    <link rel="stylesheet" href="https://cdn.jsdelivr.net/gh/openlayers/openlayers.github.io@main/dist/en/v7.0.0/legacy/ol.css" type="text/css">
    <script src="https://cdn.jsdelivr.net/gh/openlayers/openlayers.github.io@main/dist/en/v7.0.0/legacy/ol.js"></script>
    <script src="https://unpkg.com/ol-layerswitcher@4.1.1"></script>
    <link rel="stylesheet" href="https://unpkg.com/ol-layerswitcher@4.1.1/src/ol-layerswitcher.css" />
</head>
<body>
    <div id="header"><h1>%(xml_escaped_title)s</h1></div>
    <div id="subheader">Generated by <a href="https://gdal.org/programs/gdal_raster_tile.html">gdal raster tile</a>&nbsp;&nbsp;&nbsp;&nbsp;</div>
    <div id="map" class="map"></div>
    <div id="mouse-position"></div>
    <script type="text/javascript">
        var mousePositionControl = new ol.control.MousePosition({
            className: 'custom-mouse-position',
            target: document.getElementById('mouse-position'),
            undefinedHTML: '&nbsp;'
        });
        var map = new ol.Map({
            controls: ol.control.defaults.defaults().extend([mousePositionControl]),
            target: 'map',)raw");

    if (tms.identifier() == "GoogleMapsCompatible" ||
        tms.identifier() == "WorldCRS84Quad")
    {
        s += R"raw(
            layers: [
                new ol.layer.Group({
                        title: 'Base maps',
                        layers: [
                            new ol.layer.Tile({
                                title: 'OpenStreetMap',
                                type: 'base',
                                visible: true,
                                source: new ol.source.OSM()
                            }),
                        ]
                }),)raw";
    }

    if (tms.identifier() == "GoogleMapsCompatible")
    {
        s += R"raw(new ol.layer.Group({
                    title: 'Overlay',
                    layers: [
                        new ol.layer.Tile({
                            title: 'Overlay',
                            // opacity: 0.7,
                            extent: [%(ominx)f, %(ominy)f,%(omaxx)f, %(omaxy)f],
                            source: new ol.source.XYZ({
                                attributions: '%(copyright)s',
                                minZoom: %(minzoom)d,
                                maxZoom: %(maxzoom)d,
                                url: './{z}/{x}/{%(sign_y)sy}.%(tileformat)s',
                                tileSize: [%(tile_size)d, %(tile_size)d]
                            })
                        }),
                    ]
                }),)raw";
    }
    else if (tms.identifier() == "WorldCRS84Quad")
    {
        const double base_res = 180.0 / nTileSize;
        std::string resolutions = "[";
        for (int i = 0; i <= nMaxZoom; ++i)
        {
            if (i > 0)
                resolutions += ",";
            resolutions += CPLSPrintf(pszFmt, base_res / (1 << i));
        }
        resolutions += "]";
        substs["resolutions"] = std::move(resolutions);

        if (bXYZ)
        {
            substs["origin"] = "[-180,90]";
            substs["y_formula"] = "tileCoord[2]";
        }
        else
        {
            substs["origin"] = "[-180,-90]";
            substs["y_formula"] = "- 1 - tileCoord[2]";
        }

        s += R"raw(
                new ol.layer.Group({
                    title: 'Overlay',
                    layers: [
                        new ol.layer.Tile({
                            title: 'Overlay',
                            // opacity: 0.7,
                            extent: [%(ominx)f, %(ominy)f,%(omaxx)f, %(omaxy)f],
                            source: new ol.source.TileImage({
                                attributions: '%(copyright)s',
                                projection: 'EPSG:4326',
                                minZoom: %(minzoom)d,
                                maxZoom: %(maxzoom)d,
                                tileGrid: new ol.tilegrid.TileGrid({
                                    extent: [-180,-90,180,90],
                                    origin: %(origin)s,
                                    resolutions: %(resolutions)s,
                                    tileSize: [%(tile_size)d, %(tile_size)d]
                                }),
                                tileUrlFunction: function(tileCoord) {
                                    return ('./{z}/{x}/{y}.%(tileformat)s'
                                        .replace('{z}', String(tileCoord[0]))
                                        .replace('{x}', String(tileCoord[1]))
                                        .replace('{y}', String(%(y_formula)s)));
                                },
                            })
                        }),
                    ]
                }),)raw";
    }
    else
    {
        substs["maxres"] =
            CPLSPrintf(pszFmt, tms.tileMatrixList()[nMinZoom].mResX);
        std::string resolutions = "[";
        for (int i = 0; i <= nMaxZoom; ++i)
        {
            if (i > 0)
                resolutions += ",";
            resolutions += CPLSPrintf(pszFmt, tms.tileMatrixList()[i].mResX);
        }
        resolutions += "]";
        substs["resolutions"] = std::move(resolutions);

        std::string matrixsizes = "[";
        for (int i = 0; i <= nMaxZoom; ++i)
        {
            if (i > 0)
                matrixsizes += ",";
            matrixsizes +=
                CPLSPrintf("[%d,%d]", tms.tileMatrixList()[i].mMatrixWidth,
                           tms.tileMatrixList()[i].mMatrixHeight);
        }
        matrixsizes += "]";
        substs["matrixsizes"] = std::move(matrixsizes);

        double dfTopLeftX = tms.tileMatrixList()[0].mTopLeftX;
        double dfTopLeftY = tms.tileMatrixList()[0].mTopLeftY;
        if (bInvertAxisTMS)
            std::swap(dfTopLeftX, dfTopLeftY);

        if (bXYZ)
        {
            substs["origin"] =
                CPLSPrintf("[%.17g,%.17g]", dfTopLeftX, dfTopLeftY);
            substs["y_formula"] = "tileCoord[2]";
        }
        else
        {
            substs["origin"] = CPLSPrintf(
                "[%.17g,%.17g]", dfTopLeftX,
                dfTopLeftY - tms.tileMatrixList()[0].mResY *
                                 tms.tileMatrixList()[0].mTileHeight);
            substs["y_formula"] = "- 1 - tileCoord[2]";
        }

        substs["tilegrid_extent"] =
            CPLSPrintf("[%.17g,%.17g,%.17g,%.17g]", dfTopLeftX,
                       dfTopLeftY - tms.tileMatrixList()[0].mMatrixHeight *
                                        tms.tileMatrixList()[0].mResY *
                                        tms.tileMatrixList()[0].mTileHeight,
                       dfTopLeftX + tms.tileMatrixList()[0].mMatrixWidth *
                                        tms.tileMatrixList()[0].mResX *
                                        tms.tileMatrixList()[0].mTileWidth,
                       dfTopLeftY);

        s += R"raw(
            layers: [
                new ol.layer.Group({
                    title: 'Overlay',
                    layers: [
                        new ol.layer.Tile({
                            title: 'Overlay',
                            // opacity: 0.7,
                            extent: [%(ominx)f, %(ominy)f,%(omaxx)f, %(omaxy)f],
                            source: new ol.source.TileImage({
                                attributions: '%(copyright)s',
                                minZoom: %(minzoom)d,
                                maxZoom: %(maxzoom)d,
                                tileGrid: new ol.tilegrid.TileGrid({
                                    extent: %(tilegrid_extent)s,
                                    origin: %(origin)s,
                                    resolutions: %(resolutions)s,
                                    sizes: %(matrixsizes)s,
                                    tileSize: [%(tile_size)d, %(tile_size)d]
                                }),
                                tileUrlFunction: function(tileCoord) {
                                    return ('./{z}/{x}/{y}.%(tileformat)s'
                                        .replace('{z}', String(tileCoord[0]))
                                        .replace('{x}', String(tileCoord[1]))
                                        .replace('{y}', String(%(y_formula)s)));
                                },
                            })
                        }),
                    ]
                }),)raw";
    }

    s += R"raw(
            ],
            view: new ol.View({
                center: [%(center_x)f, %(center_y)f],)raw";

    if (tms.identifier() == "GoogleMapsCompatible" ||
        tms.identifier() == "WorldCRS84Quad")
    {
        substs["view_zoom"] = substs["minzoom"];
        if (tms.identifier() == "WorldCRS84Quad")
        {
            substs["view_zoom"] = CPLSPrintf("%d", nMinZoom + 1);
        }

        s += R"raw(
                zoom: %(view_zoom)d,)raw";
    }
    else
    {
        s += R"raw(
                resolution: %(maxres)f,)raw";
    }

    if (tms.identifier() == "WorldCRS84Quad")
    {
        s += R"raw(
                projection: 'EPSG:4326',)raw";
    }
    else if (!oSRS_TMS.IsEmpty() && tms.identifier() != "GoogleMapsCompatible")
    {
        const char *pszAuthName = oSRS_TMS.GetAuthorityName(nullptr);
        const char *pszAuthCode = oSRS_TMS.GetAuthorityCode(nullptr);
        if (pszAuthName && pszAuthCode && EQUAL(pszAuthName, "EPSG"))
        {
            substs["epsg_code"] = pszAuthCode;
            if (oSRS_TMS.IsGeographic())
            {
                substs["units"] = "deg";
            }
            else
            {
                const char *pszUnits = "";
                if (oSRS_TMS.GetLinearUnits(&pszUnits) == 1.0)
                    substs["units"] = "m";
                else
                    substs["units"] = pszUnits;
            }
            s += R"raw(
                projection: new ol.proj.Projection({code: 'EPSG:%(epsg_code)s', units:'%(units)s'}),)raw";
        }
    }

    s += R"raw(
            })
        });)raw";

    if (tms.identifier() == "GoogleMapsCompatible" ||
        tms.identifier() == "WorldCRS84Quad")
    {
        s += R"raw(
        map.addControl(new ol.control.LayerSwitcher());)raw";
    }

    s += R"raw(
    </script>
</body>
</html>)raw";

    ApplySubstitutions(s, substs);

    VSILFILE *f = VSIFOpenL(
        CPLFormFilenameSafe(osDirectory.c_str(), "openlayers.html", nullptr)
            .c_str(),
        "wb");
    if (f)
    {
        VSIFWriteL(s.data(), 1, s.size(), f);
        VSIFCloseL(f);
    }
}

/************************************************************************/
/*                         GetTileBoundingBox()                         */
/************************************************************************/

static void GetTileBoundingBox(int nTileX, int nTileY, int nTileZ,
                               const gdal::TileMatrixSet *poTMS,
                               bool bInvertAxisTMS,
                               OGRCoordinateTransformation *poCTToWGS84,
                               double &dfTLX, double &dfTLY, double &dfTRX,
                               double &dfTRY, double &dfLLX, double &dfLLY,
                               double &dfLRX, double &dfLRY)
{
    gdal::TileMatrixSet::TileMatrix tileMatrix =
        poTMS->tileMatrixList()[nTileZ];
    if (bInvertAxisTMS)
        std::swap(tileMatrix.mTopLeftX, tileMatrix.mTopLeftY);

    dfTLX = tileMatrix.mTopLeftX +
            nTileX * tileMatrix.mResX * tileMatrix.mTileWidth;
    dfTLY = tileMatrix.mTopLeftY -
            nTileY * tileMatrix.mResY * tileMatrix.mTileHeight;
    poCTToWGS84->Transform(1, &dfTLX, &dfTLY);

    dfTRX = tileMatrix.mTopLeftX +
            (nTileX + 1) * tileMatrix.mResX * tileMatrix.mTileWidth;
    dfTRY = tileMatrix.mTopLeftY -
            nTileY * tileMatrix.mResY * tileMatrix.mTileHeight;
    poCTToWGS84->Transform(1, &dfTRX, &dfTRY);

    dfLLX = tileMatrix.mTopLeftX +
            nTileX * tileMatrix.mResX * tileMatrix.mTileWidth;
    dfLLY = tileMatrix.mTopLeftY -
            (nTileY + 1) * tileMatrix.mResY * tileMatrix.mTileHeight;
    poCTToWGS84->Transform(1, &dfLLX, &dfLLY);

    dfLRX = tileMatrix.mTopLeftX +
            (nTileX + 1) * tileMatrix.mResX * tileMatrix.mTileWidth;
    dfLRY = tileMatrix.mTopLeftY -
            (nTileY + 1) * tileMatrix.mResY * tileMatrix.mTileHeight;
    poCTToWGS84->Transform(1, &dfLRX, &dfLRY);
}

/************************************************************************/
/*                            GenerateKML()                             */
/************************************************************************/

namespace
{
struct TileCoordinates
{
    int nTileX = 0;
    int nTileY = 0;
    int nTileZ = 0;
};
}  // namespace

static void GenerateKML(const std::string &osDirectory,
                        const std::string &osTitle, int nTileX, int nTileY,
                        int nTileZ, int nTileSize,
                        const std::string &osExtension,
                        const std::string &osURL,
                        const gdal::TileMatrixSet *poTMS, bool bInvertAxisTMS,
                        const std::string &convention,
                        OGRCoordinateTransformation *poCTToWGS84,
                        const std::vector<TileCoordinates> &children)
{
    std::map<std::string, std::string> substs;

    const bool bIsTileKML = nTileX >= 0;

    // For tests
    const char *pszFmt =
        atoi(CPLGetConfigOption("GDAL_RASTER_TILE_KML_PREC", "14")) == 10
            ? "%.10f"
            : "%.14f";

    substs["tx"] = CPLSPrintf("%d", nTileX);
    substs["tz"] = CPLSPrintf("%d", nTileZ);
    substs["tileformat"] = osExtension;
    substs["minlodpixels"] = CPLSPrintf("%d", nTileSize / 2);
    substs["maxlodpixels"] =
        children.empty() ? "-1" : CPLSPrintf("%d", nTileSize * 8);

    double dfTLX = 0;
    double dfTLY = 0;
    double dfTRX = 0;
    double dfTRY = 0;
    double dfLLX = 0;
    double dfLLY = 0;
    double dfLRX = 0;
    double dfLRY = 0;

    int nFileY = -1;
    if (!bIsTileKML)
    {
        char *pszStr = CPLEscapeString(osTitle.c_str(), -1, CPLES_XML);
        substs["xml_escaped_title"] = pszStr;
        CPLFree(pszStr);
    }
    else
    {
        nFileY = GetFileY(nTileY, poTMS->tileMatrixList()[nTileZ], convention);
        substs["realtiley"] = CPLSPrintf("%d", nFileY);
        substs["xml_escaped_title"] =
            CPLSPrintf("%d/%d/%d.kml", nTileZ, nTileX, nFileY);

        GetTileBoundingBox(nTileX, nTileY, nTileZ, poTMS, bInvertAxisTMS,
                           poCTToWGS84, dfTLX, dfTLY, dfTRX, dfTRY, dfLLX,
                           dfLLY, dfLRX, dfLRY);
    }

    substs["drawOrder"] = CPLSPrintf("%d", nTileX == 0  ? 2 * nTileZ + 1
                                           : nTileX > 0 ? 2 * nTileZ
                                                        : 0);

    substs["url"] = osURL.empty() && bIsTileKML ? "../../" : "";

    const bool bIsRectangle =
        (dfTLX == dfLLX && dfTRX == dfLRX && dfTLY == dfTRY && dfLLY == dfLRY);
    const bool bUseGXNamespace = bIsTileKML && !bIsRectangle;

    substs["xmlns_gx"] = bUseGXNamespace
                             ? " xmlns:gx=\"http://www.google.com/kml/ext/2.2\""
                             : "";

    CPLString s(R"raw(<?xml version="1.0" encoding="utf-8"?>
<kml xmlns="http://www.opengis.net/kml/2.2"%(xmlns_gx)s>
  <Document>
    <name>%(xml_escaped_title)s</name>
    <description></description>
    <Style>
      <ListStyle id="hideChildren">
        <listItemType>checkHideChildren</listItemType>
      </ListStyle>
    </Style>
)raw");
    ApplySubstitutions(s, substs);

    if (bIsTileKML)
    {
        CPLString s2(R"raw(    <Region>
      <LatLonAltBox>
        <north>%(north)f</north>
        <south>%(south)f</south>
        <east>%(east)f</east>
        <west>%(west)f</west>
      </LatLonAltBox>
      <Lod>
        <minLodPixels>%(minlodpixels)d</minLodPixels>
        <maxLodPixels>%(maxlodpixels)d</maxLodPixels>
      </Lod>
    </Region>
    <GroundOverlay>
      <drawOrder>%(drawOrder)d</drawOrder>
      <Icon>
        <href>%(realtiley)d.%(tileformat)s</href>
      </Icon>
      <LatLonBox>
        <north>%(north)f</north>
        <south>%(south)f</south>
        <east>%(east)f</east>
        <west>%(west)f</west>
      </LatLonBox>
)raw");

        if (!bIsRectangle)
        {
            s2 +=
                R"raw(      <gx:LatLonQuad><coordinates>%(LLX)f,%(LLY)f %(LRX)f,%(LRY)f %(TRX)f,%(TRY)f %(TLX)f,%(TLY)f</coordinates></gx:LatLonQuad>
)raw";
        }

        s2 += R"raw(    </GroundOverlay>
)raw";
        substs["north"] = CPLSPrintf(pszFmt, std::max(dfTLY, dfTRY));
        substs["south"] = CPLSPrintf(pszFmt, std::min(dfLLY, dfLRY));
        substs["east"] = CPLSPrintf(pszFmt, std::max(dfTRX, dfLRX));
        substs["west"] = CPLSPrintf(pszFmt, std::min(dfLLX, dfTLX));

        if (!bIsRectangle)
        {
            substs["TLX"] = CPLSPrintf(pszFmt, dfTLX);
            substs["TLY"] = CPLSPrintf(pszFmt, dfTLY);
            substs["TRX"] = CPLSPrintf(pszFmt, dfTRX);
            substs["TRY"] = CPLSPrintf(pszFmt, dfTRY);
            substs["LRX"] = CPLSPrintf(pszFmt, dfLRX);
            substs["LRY"] = CPLSPrintf(pszFmt, dfLRY);
            substs["LLX"] = CPLSPrintf(pszFmt, dfLLX);
            substs["LLY"] = CPLSPrintf(pszFmt, dfLLY);
        }

        ApplySubstitutions(s2, substs);
        s += s2;
    }

    for (const auto &child : children)
    {
        substs["tx"] = CPLSPrintf("%d", child.nTileX);
        substs["tz"] = CPLSPrintf("%d", child.nTileZ);
        substs["realtiley"] = CPLSPrintf(
            "%d", GetFileY(child.nTileY, poTMS->tileMatrixList()[child.nTileZ],
                           convention));

        GetTileBoundingBox(child.nTileX, child.nTileY, child.nTileZ, poTMS,
                           bInvertAxisTMS, poCTToWGS84, dfTLX, dfTLY, dfTRX,
                           dfTRY, dfLLX, dfLLY, dfLRX, dfLRY);

        CPLString s2(R"raw(    <NetworkLink>
      <name>%(tz)d/%(tx)d/%(realtiley)d.%(tileformat)s</name>
      <Region>
        <LatLonAltBox>
          <north>%(north)f</north>
          <south>%(south)f</south>
          <east>%(east)f</east>
          <west>%(west)f</west>
        </LatLonAltBox>
        <Lod>
          <minLodPixels>%(minlodpixels)d</minLodPixels>
          <maxLodPixels>-1</maxLodPixels>
        </Lod>
      </Region>
      <Link>
        <href>%(url)s%(tz)d/%(tx)d/%(realtiley)d.kml</href>
        <viewRefreshMode>onRegion</viewRefreshMode>
        <viewFormat/>
      </Link>
    </NetworkLink>
)raw");
        substs["north"] = CPLSPrintf(pszFmt, std::max(dfTLY, dfTRY));
        substs["south"] = CPLSPrintf(pszFmt, std::min(dfLLY, dfLRY));
        substs["east"] = CPLSPrintf(pszFmt, std::max(dfTRX, dfLRX));
        substs["west"] = CPLSPrintf(pszFmt, std::min(dfLLX, dfTLX));
        ApplySubstitutions(s2, substs);
        s += s2;
    }

    s += R"raw(</Document>
</kml>)raw";

    std::string osFilename(osDirectory);
    if (!bIsTileKML)
    {
        osFilename =
            CPLFormFilenameSafe(osFilename.c_str(), "doc.kml", nullptr);
    }
    else
    {
        osFilename = CPLFormFilenameSafe(osFilename.c_str(),
                                         CPLSPrintf("%d", nTileZ), nullptr);
        osFilename = CPLFormFilenameSafe(osFilename.c_str(),
                                         CPLSPrintf("%d", nTileX), nullptr);
        osFilename = CPLFormFilenameSafe(osFilename.c_str(),
                                         CPLSPrintf("%d.kml", nFileY), nullptr);
    }

    VSILFILE *f = VSIFOpenL(osFilename.c_str(), "wb");
    if (f)
    {
        VSIFWriteL(s.data(), 1, s.size(), f);
        VSIFCloseL(f);
    }
}

namespace
{

/************************************************************************/
/*                           ResourceManager                            */
/************************************************************************/

// Generic cache managing resources
template <class Resource> class ResourceManager /* non final */
{
  public:
    virtual ~ResourceManager() = default;

    std::unique_ptr<Resource> AcquireResources()
    {
        std::lock_guard oLock(m_oMutex);
        if (!m_oResources.empty())
        {
            auto ret = std::move(m_oResources.back());
            m_oResources.pop_back();
            return ret;
        }

        return CreateResources();
    }

    void ReleaseResources(std::unique_ptr<Resource> resources)
    {
        std::lock_guard oLock(m_oMutex);
        m_oResources.push_back(std::move(resources));
    }

    void SetError()
    {
        std::lock_guard oLock(m_oMutex);
        if (m_errorMsg.empty())
            m_errorMsg = CPLGetLastErrorMsg();
    }

    const std::string &GetErrorMsg() const
    {
        std::lock_guard oLock(m_oMutex);
        return m_errorMsg;
    }

  protected:
    virtual std::unique_ptr<Resource> CreateResources() = 0;

  private:
    mutable std::mutex m_oMutex{};
    std::vector<std::unique_ptr<Resource>> m_oResources{};
    std::string m_errorMsg{};
};

/************************************************************************/
/*                      PerThreadMaxZoomResources                       */
/************************************************************************/

// Per-thread resources for generation of tiles at full resolution
struct PerThreadMaxZoomResources
{
    struct GDALDatasetReleaser
    {
        void operator()(GDALDataset *poDS)
        {
            if (poDS)
                poDS->ReleaseRef();
        }
    };

    std::unique_ptr<GDALDataset, GDALDatasetReleaser> poSrcDS{};
    std::vector<GByte> dstBuffer{};
    std::unique_ptr<FakeMaxZoomDataset> poFakeMaxZoomDS{};
    std::unique_ptr<void, decltype(&GDALDestroyTransformer)> poTransformer{
        nullptr, GDALDestroyTransformer};
    std::unique_ptr<GDALWarpOperation> poWO{};
};

/************************************************************************/
/*                   PerThreadMaxZoomResourceManager                    */
/************************************************************************/

// Manage a cache of PerThreadMaxZoomResources instances
class PerThreadMaxZoomResourceManager final
    : public ResourceManager<PerThreadMaxZoomResources>
{
  public:
    PerThreadMaxZoomResourceManager(GDALDataset *poSrcDS,
                                    const GDALWarpOptions *psWO,
                                    void *pTransformerArg,
                                    const FakeMaxZoomDataset &oFakeMaxZoomDS,
                                    size_t nBufferSize)
        : m_poSrcDS(poSrcDS), m_psWOSource(psWO),
          m_pTransformerArg(pTransformerArg), m_oFakeMaxZoomDS(oFakeMaxZoomDS),
          m_nBufferSize(nBufferSize)
    {
    }

  protected:
    std::unique_ptr<PerThreadMaxZoomResources> CreateResources() override
    {
        auto ret = std::make_unique<PerThreadMaxZoomResources>();

        ret->poSrcDS.reset(GDALGetThreadSafeDataset(m_poSrcDS, GDAL_OF_RASTER));
        if (!ret->poSrcDS)
            return nullptr;

        try
        {
            ret->dstBuffer.resize(m_nBufferSize);
        }
        catch (const std::exception &)
        {
            CPLError(CE_Failure, CPLE_OutOfMemory,
                     "Out of memory allocating temporary buffer");
            return nullptr;
        }

        ret->poFakeMaxZoomDS = m_oFakeMaxZoomDS.Clone(ret->dstBuffer);

        ret->poTransformer.reset(GDALCloneTransformer(m_pTransformerArg));
        if (!ret->poTransformer)
            return nullptr;

        auto psWO =
            std::unique_ptr<GDALWarpOptions, decltype(&GDALDestroyWarpOptions)>(
                GDALCloneWarpOptions(m_psWOSource), GDALDestroyWarpOptions);
        if (!psWO)
            return nullptr;

        psWO->hSrcDS = GDALDataset::ToHandle(ret->poSrcDS.get());
        psWO->hDstDS = GDALDataset::ToHandle(ret->poFakeMaxZoomDS.get());
        psWO->pTransformerArg = ret->poTransformer.get();
        psWO->pfnTransformer = m_psWOSource->pfnTransformer;

        ret->poWO = std::make_unique<GDALWarpOperation>();
        if (ret->poWO->Initialize(psWO.get()) != CE_None)
            return nullptr;

        return ret;
    }

  private:
    GDALDataset *const m_poSrcDS;
    const GDALWarpOptions *const m_psWOSource;
    void *const m_pTransformerArg;
    const FakeMaxZoomDataset &m_oFakeMaxZoomDS;
    const size_t m_nBufferSize;

    CPL_DISALLOW_COPY_ASSIGN(PerThreadMaxZoomResourceManager)
};

/************************************************************************/
/*                     PerThreadLowerZoomResources                      */
/************************************************************************/

// Per-thread resources for generation of tiles at zoom level < max
struct PerThreadLowerZoomResources
{
    std::unique_ptr<GDALDataset> poSrcDS{};
};

/************************************************************************/
/*                  PerThreadLowerZoomResourceManager                   */
/************************************************************************/

// Manage a cache of PerThreadLowerZoomResources instances
class PerThreadLowerZoomResourceManager final
    : public ResourceManager<PerThreadLowerZoomResources>
{
  public:
    explicit PerThreadLowerZoomResourceManager(const MosaicDataset &oSrcDS)
        : m_oSrcDS(oSrcDS)
    {
    }

  protected:
    std::unique_ptr<PerThreadLowerZoomResources> CreateResources() override
    {
        auto ret = std::make_unique<PerThreadLowerZoomResources>();
        ret->poSrcDS = m_oSrcDS.Clone();
        return ret;
    }

  private:
    const MosaicDataset &m_oSrcDS;
};

}  // namespace

/************************************************************************/
/*           GDALRasterTileAlgorithm::ValidateOutputFormat()            */
/************************************************************************/

bool GDALRasterTileAlgorithm::ValidateOutputFormat(GDALDataType eSrcDT) const
{
    if (m_format == "PNG")
    {
        if (m_poSrcDS->GetRasterCount() > 4)
        {
            ReportError(CE_Failure, CPLE_NotSupported,
                        "Only up to 4 bands supported for PNG.");
            return false;
        }
        if (eSrcDT != GDT_UInt8 && eSrcDT != GDT_UInt16)
        {
            ReportError(CE_Failure, CPLE_NotSupported,
                        "Only Byte and UInt16 data types supported for PNG.");
            return false;
        }
    }
    else if (m_format == "JPEG")
    {
        if (m_poSrcDS->GetRasterCount() > 4)
        {
            ReportError(
                CE_Failure, CPLE_NotSupported,
                "Only up to 4 bands supported for JPEG (with alpha ignored).");
            return false;
        }
        const bool bUInt16Supported =
            strstr(m_poDstDriver->GetMetadataItem(GDAL_DMD_CREATIONDATATYPES),
                   "UInt16");
        if (eSrcDT != GDT_UInt8 && !(eSrcDT == GDT_UInt16 && bUInt16Supported))
        {
            ReportError(
                CE_Failure, CPLE_NotSupported,
                bUInt16Supported
                    ? "Only Byte and UInt16 data types supported for JPEG."
                    : "Only Byte data type supported for JPEG.");
            return false;
        }
        if (eSrcDT == GDT_UInt16)
        {
            if (const char *pszNBITS =
                    m_poSrcDS->GetRasterBand(1)->GetMetadataItem(
                        "NBITS", "IMAGE_STRUCTURE"))
            {
                if (atoi(pszNBITS) > 12)
                {
                    ReportError(CE_Failure, CPLE_NotSupported,
                                "JPEG output only supported up to 12 bits");
                    return false;
                }
            }
            else
            {
                double adfMinMax[2] = {0, 0};
                m_poSrcDS->GetRasterBand(1)->ComputeRasterMinMax(
                    /* bApproxOK = */ true, adfMinMax);
                if (adfMinMax[1] >= (1 << 12))
                {
                    ReportError(CE_Failure, CPLE_NotSupported,
                                "JPEG output only supported up to 12 bits");
                    return false;
                }
            }
        }
    }
    else if (m_format == "WEBP")
    {
        if (m_poSrcDS->GetRasterCount() != 3 &&
            m_poSrcDS->GetRasterCount() != 4)
        {
            ReportError(CE_Failure, CPLE_NotSupported,
                        "Only 3 or 4 bands supported for WEBP.");
            return false;
        }
        if (eSrcDT != GDT_UInt8)
        {
            ReportError(CE_Failure, CPLE_NotSupported,
                        "Only Byte data type supported for WEBP.");
            return false;
        }
    }
    return true;
}

/************************************************************************/
/*            GDALRasterTileAlgorithm::ComputeJobChunkSize()            */
/************************************************************************/

// Given a number of tiles in the Y dimension being nTilesPerCol and
// in the X dimension being nTilesPerRow, compute the (upper bound of)
// number of jobs needed to be nYOuterIterations x nXOuterIterations,
// with each job processing in average dfTilesYPerJob x dfTilesXPerJob
// tiles.
/* static */
void GDALRasterTileAlgorithm::ComputeJobChunkSize(
    int nMaxJobCount, int nTilesPerCol, int nTilesPerRow,
    double &dfTilesYPerJob, int &nYOuterIterations, double &dfTilesXPerJob,
    int &nXOuterIterations)
{
    CPLAssert(nMaxJobCount >= 1);
    dfTilesYPerJob = static_cast<double>(nTilesPerCol) / nMaxJobCount;
    nYOuterIterations = dfTilesYPerJob >= 1 ? nMaxJobCount : 1;

    dfTilesXPerJob = dfTilesYPerJob >= 1
                         ? nTilesPerRow
                         : static_cast<double>(nTilesPerRow) / nMaxJobCount;
    nXOuterIterations = dfTilesYPerJob >= 1 ? 1 : nMaxJobCount;

    if (dfTilesYPerJob < 1 && dfTilesXPerJob < 1 &&
        nTilesPerCol <= nMaxJobCount / nTilesPerRow)
    {
        dfTilesYPerJob = 1;
        dfTilesXPerJob = 1;
        nYOuterIterations = nTilesPerCol;
        nXOuterIterations = nTilesPerRow;
    }
}

/************************************************************************/
/*               GDALRasterTileAlgorithm::AddArgToArgv()                */
/************************************************************************/

bool GDALRasterTileAlgorithm::AddArgToArgv(const GDALAlgorithmArg *arg,
                                           CPLStringList &aosArgv) const
{
    aosArgv.push_back(CPLSPrintf("--%s", arg->GetName().c_str()));
    if (arg->GetType() == GAAT_STRING)
    {
        aosArgv.push_back(arg->Get<std::string>().c_str());
    }
    else if (arg->GetType() == GAAT_STRING_LIST)
    {
        bool bFirst = true;
        for (const std::string &s : arg->Get<std::vector<std::string>>())
        {
            if (!bFirst)
            {
                aosArgv.push_back(CPLSPrintf("--%s", arg->GetName().c_str()));
            }
            bFirst = false;
            aosArgv.push_back(s.c_str());
        }
    }
    else if (arg->GetType() == GAAT_REAL)
    {
        aosArgv.push_back(CPLSPrintf("%.17g", arg->Get<double>()));
    }
    else if (arg->GetType() == GAAT_INTEGER)
    {
        aosArgv.push_back(CPLSPrintf("%d", arg->Get<int>()));
    }
    else if (arg->GetType() != GAAT_BOOLEAN)
    {
        ReportError(CE_Failure, CPLE_AppDefined,
                    "Bug: argument of type %d not handled "
                    "by gdal raster tile!",
                    static_cast<int>(arg->GetType()));
        return false;
    }
    return true;
}

/************************************************************************/
/*            GDALRasterTileAlgorithm::IsCompatibleOfSpawn()            */
/************************************************************************/

bool GDALRasterTileAlgorithm::IsCompatibleOfSpawn(const char *&pszErrorMsg)
{
    pszErrorMsg = "";
    if (!m_bIsNamedNonMemSrcDS)
    {
        pszErrorMsg = "Unnamed or memory dataset sources are not supported "
                      "with spawn parallelization method";
        return false;
    }
    if (cpl::starts_with(m_output, "/vsimem/"))
    {
        pszErrorMsg = "/vsimem/ output directory not supported with spawn "
                      "parallelization method";
        return false;
    }

    if (m_osGDALPath.empty())
        m_osGDALPath = GDALGetGDALPath();
    return !(m_osGDALPath.empty());
}

/************************************************************************/
/*                    GetProgressForChildProcesses()                    */
/************************************************************************/

static void GetProgressForChildProcesses(
    bool &bRet, std::vector<CPLSpawnedProcess *> &ahSpawnedProcesses,
    std::vector<uint64_t> &anRemainingTilesForProcess, uint64_t &nCurTile,
    uint64_t nTotalTiles, GDALProgressFunc pfnProgress, void *pProgressData)
{
    std::vector<unsigned int> anProgressState(ahSpawnedProcesses.size(), 0);
    std::vector<unsigned int> anEndState(ahSpawnedProcesses.size(), 0);
    std::vector<bool> abFinished(ahSpawnedProcesses.size(), false);
    std::vector<unsigned int> anStartErrorState(ahSpawnedProcesses.size(), 0);

    while (bRet)
    {
        size_t iProcess = 0;
        size_t nFinished = 0;
        for (CPLSpawnedProcess *hSpawnedProcess : ahSpawnedProcesses)
        {
            char ch = 0;
            if (abFinished[iProcess] ||
                !CPLPipeRead(CPLSpawnAsyncGetInputFileHandle(hSpawnedProcess),
                             &ch, 1))
            {
                ++nFinished;
            }
            else if (ch == PROGRESS_MARKER[anProgressState[iProcess]])
            {
                ++anProgressState[iProcess];
                if (anProgressState[iProcess] == sizeof(PROGRESS_MARKER))
                {
                    anProgressState[iProcess] = 0;
                    --anRemainingTilesForProcess[iProcess];
                    ++nCurTile;
                    if (bRet && pfnProgress)
                    {
                        if (!pfnProgress(static_cast<double>(nCurTile) /
                                             static_cast<double>(nTotalTiles),
                                         "", pProgressData))
                        {
                            CPLError(CE_Failure, CPLE_UserInterrupt,
                                     "Process interrupted by user");
                            bRet = false;
                            return;
                        }
                    }
                }
            }
            else if (ch == END_MARKER[anEndState[iProcess]])
            {
                ++anEndState[iProcess];
                if (anEndState[iProcess] == sizeof(END_MARKER))
                {
                    anEndState[iProcess] = 0;
                    abFinished[iProcess] = true;
                    ++nFinished;
                }
            }
            else if (ch == ERROR_START_MARKER[anStartErrorState[iProcess]])
            {
                ++anStartErrorState[iProcess];
                if (anStartErrorState[iProcess] == sizeof(ERROR_START_MARKER))
                {
                    anStartErrorState[iProcess] = 0;
                    uint32_t nErr = 0;
                    CPLPipeRead(
                        CPLSpawnAsyncGetInputFileHandle(hSpawnedProcess), &nErr,
                        sizeof(nErr));
                    uint32_t nNum = 0;
                    CPLPipeRead(
                        CPLSpawnAsyncGetInputFileHandle(hSpawnedProcess), &nNum,
                        sizeof(nNum));
                    uint16_t nMsgLen = 0;
                    CPLPipeRead(
                        CPLSpawnAsyncGetInputFileHandle(hSpawnedProcess),
                        &nMsgLen, sizeof(nMsgLen));
                    std::string osMsg;
                    osMsg.resize(nMsgLen);
                    CPLPipeRead(
                        CPLSpawnAsyncGetInputFileHandle(hSpawnedProcess),
                        &osMsg[0], nMsgLen);
                    if (nErr <= CE_Fatal &&
                        nNum <= CPLE_ObjectStorageGenericError)
                    {
                        bool bDone = false;
                        if (nErr == CE_Debug)
                        {
                            auto nPos = osMsg.find(": ");
                            if (nPos != std::string::npos)
                            {
                                bDone = true;
                                CPLDebug(
                                    osMsg.substr(0, nPos).c_str(),
                                    "subprocess %d: %s",
                                    static_cast<int>(iProcess),
                                    osMsg.substr(nPos + strlen(": ")).c_str());
                            }
                        }
                        // cppcheck-suppress knownConditionTrueFalse
                        if (!bDone)
                        {
                            CPLError(nErr == CE_Fatal
                                         ? CE_Failure
                                         : static_cast<CPLErr>(nErr),
                                     static_cast<CPLErrorNum>(nNum),
                                     "Sub-process %d: %s",
                                     static_cast<int>(iProcess), osMsg.c_str());
                        }
                    }
                }
            }
            else
            {
                CPLErrorOnce(
                    CE_Warning, CPLE_AppDefined,
                    "Spurious character detected on stdout of child process");
                anProgressState[iProcess] = 0;
                if (ch == PROGRESS_MARKER[anProgressState[iProcess]])
                {
                    ++anProgressState[iProcess];
                }
            }
            ++iProcess;
        }
        if (!bRet || nFinished == ahSpawnedProcesses.size())
            break;
    }
}

/************************************************************************/
/*                      WaitForSpawnedProcesses()                       */
/************************************************************************/

void GDALRasterTileAlgorithm::WaitForSpawnedProcesses(
    bool &bRet, const std::vector<std::string> &asCommandLines,
    std::vector<CPLSpawnedProcess *> &ahSpawnedProcesses) const
{
    size_t iProcess = 0;
    for (CPLSpawnedProcess *hSpawnedProcess : ahSpawnedProcesses)
    {
        CPL_IGNORE_RET_VAL(
            CPLPipeWrite(CPLSpawnAsyncGetOutputFileHandle(hSpawnedProcess),
                         STOP_MARKER, static_cast<int>(strlen(STOP_MARKER))));

        char ch = 0;
        std::string errorMsg;
        while (CPLPipeRead(CPLSpawnAsyncGetErrorFileHandle(hSpawnedProcess),
                           &ch, 1))
        {
            if (ch == '\n')
            {
                if (!errorMsg.empty())
                {
                    if (cpl::starts_with(errorMsg, "ERROR "))
                    {
                        const auto nPos = errorMsg.find(": ");
                        if (nPos != std::string::npos)
                            errorMsg = errorMsg.substr(nPos + 1);
                        ReportError(CE_Failure, CPLE_AppDefined, "%s",
                                    errorMsg.c_str());
                    }
                    else
                    {
                        std::string osComp = "GDAL";
                        const auto nPos = errorMsg.find(": ");
                        if (nPos != std::string::npos)
                        {
                            osComp = errorMsg.substr(0, nPos);
                            errorMsg = errorMsg.substr(nPos + 1);
                        }
                        CPLDebug(osComp.c_str(), "%s", errorMsg.c_str());
                    }
                    errorMsg.clear();
                }
            }
            else
            {
                errorMsg += ch;
            }
        }

        if (CPLSpawnAsyncFinish(hSpawnedProcess, /* bWait = */ true,
                                /* bKill = */ false) != 0)
        {
            bRet = false;
            ReportError(CE_Failure, CPLE_AppDefined,
                        "Child process '%s' failed",
                        asCommandLines[iProcess].c_str());
        }
        ++iProcess;
    }
}

/************************************************************************/
/*               GDALRasterTileAlgorithm::GetMaxChildCount()            */
/**********************************f**************************************/

int GDALRasterTileAlgorithm::GetMaxChildCount(int nMaxJobCount) const
{
#ifndef _WIN32
    // Limit the number of jobs compared to how many file descriptors we have
    // left
    const int remainingFileDescriptorCount =
        CPLGetRemainingFileDescriptorCount();
    constexpr int SOME_MARGIN = 3;
    constexpr int FD_PER_CHILD = 3; /* stdin, stdout and stderr */
    if (FD_PER_CHILD * nMaxJobCount + SOME_MARGIN >
        remainingFileDescriptorCount)
    {
        nMaxJobCount = std::max(
            1, (remainingFileDescriptorCount - SOME_MARGIN) / FD_PER_CHILD);
        ReportError(
            CE_Warning, CPLE_AppDefined,
            "Limiting the number of child workers to %d (instead of %d), "
            "because there are not enough file descriptors left (%d)",
            nMaxJobCount, m_numThreads, remainingFileDescriptorCount);
    }
#endif
    return nMaxJobCount;
}

/************************************************************************/
/*                         SendConfigOptions()                          */
/************************************************************************/

static void SendConfigOptions(CPLSpawnedProcess *hSpawnedProcess, bool &bRet)
{
    // Send most config options through pipe, to avoid leaking
    // secrets when listing processes
    auto handle = CPLSpawnAsyncGetOutputFileHandle(hSpawnedProcess);
    for (auto pfnFunc : {&CPLGetConfigOptions, &CPLGetThreadLocalConfigOptions})
    {
        CPLStringList aosConfigOptions((*pfnFunc)());
        for (const char *pszNameValue : aosConfigOptions)
        {
            if (!STARTS_WITH(pszNameValue, "GDAL_CACHEMAX") &&
                !STARTS_WITH(pszNameValue, "GDAL_NUM_THREADS"))
            {
                constexpr const char *CONFIG_MARKER = "--config\n";
                bRet &= CPL_TO_BOOL(
                    CPLPipeWrite(handle, CONFIG_MARKER,
                                 static_cast<int>(strlen(CONFIG_MARKER))));
                char *pszEscaped = CPLEscapeString(pszNameValue, -1, CPLES_URL);
                bRet &= CPL_TO_BOOL(CPLPipeWrite(
                    handle, pszEscaped, static_cast<int>(strlen(pszEscaped))));
                CPLFree(pszEscaped);
                bRet &= CPL_TO_BOOL(CPLPipeWrite(handle, "\n", 1));
            }
        }
    }
    constexpr const char *END_CONFIG_MARKER = "END_CONFIG\n";
    bRet &=
        CPL_TO_BOOL(CPLPipeWrite(handle, END_CONFIG_MARKER,
                                 static_cast<int>(strlen(END_CONFIG_MARKER))));
}

/************************************************************************/
/*                      GenerateTilesForkMethod()                       */
/************************************************************************/

#ifdef FORK_ALLOWED

namespace
{
struct ForkWorkStructure
{
    uint64_t nCacheMaxPerProcess = 0;
    CPLStringList aosArgv{};
    GDALDataset *poMemSrcDS{};
};
}  // namespace

static CPL_FILE_HANDLE pipeIn = CPL_FILE_INVALID_HANDLE;
static CPL_FILE_HANDLE pipeOut = CPL_FILE_INVALID_HANDLE;

static int GenerateTilesForkMethod(CPL_FILE_HANDLE in, CPL_FILE_HANDLE out)
{
    pipeIn = in;
    pipeOut = out;

    const ForkWorkStructure *pWorkStructure = nullptr;
    CPLPipeRead(in, &pWorkStructure, sizeof(pWorkStructure));

    CPLSetConfigOption("GDAL_NUM_THREADS", "1");
    GDALSetCacheMax64(pWorkStructure->nCacheMaxPerProcess);

    GDALRasterTileAlgorithmStandalone alg;
    if (pWorkStructure->poMemSrcDS)
    {
        auto *inputArg = alg.GetArg(GDAL_ARG_NAME_INPUT);
        std::vector<GDALArgDatasetValue> val;
        val.resize(1);
        val[0].Set(pWorkStructure->poMemSrcDS);
        inputArg->Set(std::move(val));
    }
    return alg.ParseCommandLineArguments(pWorkStructure->aosArgv) && alg.Run()
               ? 0
               : 1;
}

#endif  // FORK_ALLOWED

/************************************************************************/
/*       GDALRasterTileAlgorithm::GenerateBaseTilesSpawnMethod()        */
/************************************************************************/

bool GDALRasterTileAlgorithm::GenerateBaseTilesSpawnMethod(
    int nBaseTilesPerCol, int nBaseTilesPerRow, int nMinTileX, int nMinTileY,
    int nMaxTileX, int nMaxTileY, uint64_t nTotalTiles, uint64_t nBaseTiles,
    GDALProgressFunc pfnProgress, void *pProgressData)
{
    if (m_parallelMethod == "spawn")
    {
        CPLAssert(!m_osGDALPath.empty());
    }

    const int nMaxJobCount = GetMaxChildCount(std::max(
        1, static_cast<int>(std::min<uint64_t>(
               m_numThreads, nBaseTiles / GetThresholdMinTilesPerJob()))));

    double dfTilesYPerJob;
    int nYOuterIterations;
    double dfTilesXPerJob;
    int nXOuterIterations;
    ComputeJobChunkSize(nMaxJobCount, nBaseTilesPerCol, nBaseTilesPerRow,
                        dfTilesYPerJob, nYOuterIterations, dfTilesXPerJob,
                        nXOuterIterations);

    CPLDebugOnly("gdal_raster_tile",
                 "nYOuterIterations=%d, dfTilesYPerJob=%g, "
                 "nXOuterIterations=%d, dfTilesXPerJob=%g",
                 nYOuterIterations, dfTilesYPerJob, nXOuterIterations,
                 dfTilesXPerJob);

    std::vector<std::string> asCommandLines;
    std::vector<CPLSpawnedProcess *> ahSpawnedProcesses;
    std::vector<uint64_t> anRemainingTilesForProcess;

    const uint64_t nCacheMaxPerProcess = GDALGetCacheMax64() / nMaxJobCount;

    const auto poSrcDriver = m_poSrcDS->GetDriver();
    const bool bIsMEMSource =
        poSrcDriver && EQUAL(poSrcDriver->GetDescription(), "MEM");

    int nLastYEndIncluded = nMinTileY - 1;

#ifdef FORK_ALLOWED
    std::vector<std::unique_ptr<ForkWorkStructure>> forkWorkStructures;
#endif

    bool bRet = true;
    for (int iYOuterIter = 0; bRet && iYOuterIter < nYOuterIterations &&
                              nLastYEndIncluded < nMaxTileY;
         ++iYOuterIter)
    {
        const int iYStart = nLastYEndIncluded + 1;
        const int iYEndIncluded =
            iYOuterIter + 1 == nYOuterIterations
                ? nMaxTileY
                : std::max(
                      iYStart,
                      static_cast<int>(std::floor(
                          nMinTileY + (iYOuterIter + 1) * dfTilesYPerJob - 1)));

        nLastYEndIncluded = iYEndIncluded;

        int nLastXEndIncluded = nMinTileX - 1;
        for (int iXOuterIter = 0; bRet && iXOuterIter < nXOuterIterations &&
                                  nLastXEndIncluded < nMaxTileX;
             ++iXOuterIter)
        {
            const int iXStart = nLastXEndIncluded + 1;
            const int iXEndIncluded =
                iXOuterIter + 1 == nXOuterIterations
                    ? nMaxTileX
                    : std::max(iXStart,
                               static_cast<int>(std::floor(
                                   nMinTileX +
                                   (iXOuterIter + 1) * dfTilesXPerJob - 1)));

            nLastXEndIncluded = iXEndIncluded;

            anRemainingTilesForProcess.push_back(
                static_cast<uint64_t>(iYEndIncluded - iYStart + 1) *
                (iXEndIncluded - iXStart + 1));

            CPLStringList aosArgv;
            if (m_parallelMethod == "spawn")
            {
                aosArgv.push_back(m_osGDALPath.c_str());
                aosArgv.push_back("raster");
                aosArgv.push_back("tile");
                aosArgv.push_back("--config-options-in-stdin");
                aosArgv.push_back("--config");
                aosArgv.push_back("GDAL_NUM_THREADS=1");
                aosArgv.push_back("--config");
                aosArgv.push_back(
                    CPLSPrintf("GDAL_CACHEMAX=%" PRIu64, nCacheMaxPerProcess));
            }
            aosArgv.push_back(
                std::string("--").append(GDAL_ARG_NAME_NUM_THREADS).c_str());
            aosArgv.push_back("1");
            aosArgv.push_back("--min-x");
            aosArgv.push_back(CPLSPrintf("%d", iXStart));
            aosArgv.push_back("--max-x");
            aosArgv.push_back(CPLSPrintf("%d", iXEndIncluded));
            aosArgv.push_back("--min-y");
            aosArgv.push_back(CPLSPrintf("%d", iYStart));
            aosArgv.push_back("--max-y");
            aosArgv.push_back(CPLSPrintf("%d", iYEndIncluded));
            aosArgv.push_back("--webviewer");
            aosArgv.push_back("none");
            aosArgv.push_back(m_parallelMethod == "spawn" ? "--spawned"
                                                          : "--forked");
            if (!bIsMEMSource)
            {
                aosArgv.push_back("--input");
                aosArgv.push_back(m_poSrcDS->GetDescription());
            }
            for (const auto &arg : GetArgs())
            {
                if (arg->IsExplicitlySet() && arg->GetName() != "min-x" &&
                    arg->GetName() != "min-y" && arg->GetName() != "max-x" &&
                    arg->GetName() != "max-y" && arg->GetName() != "min-zoom" &&
                    arg->GetName() != "progress" &&
                    arg->GetName() != "progress-forked" &&
                    arg->GetName() != GDAL_ARG_NAME_INPUT &&
                    arg->GetName() != GDAL_ARG_NAME_NUM_THREADS &&
                    arg->GetName() != "webviewer" &&
                    arg->GetName() != "parallel-method")
                {
                    if (!AddArgToArgv(arg.get(), aosArgv))
                        return false;
                }
            }

            std::string cmdLine;
            for (const char *arg : aosArgv)
            {
                if (!cmdLine.empty())
                    cmdLine += ' ';
                CPLString sArg(arg);
                if (sArg.find_first_of(" \"") != std::string::npos)
                {
                    cmdLine += '"';
                    cmdLine += sArg.replaceAll('"', "\\\"");
                    cmdLine += '"';
                }
                else
                    cmdLine += sArg;
            }
            CPLDebugOnly("gdal_raster_tile", "%s %s",
                         m_parallelMethod == "spawn" ? "Spawning" : "Forking",
                         cmdLine.c_str());
            asCommandLines.push_back(std::move(cmdLine));

#ifdef FORK_ALLOWED
            if (m_parallelMethod == "fork")
            {
                forkWorkStructures.push_back(
                    std::make_unique<ForkWorkStructure>());
                ForkWorkStructure *pData = forkWorkStructures.back().get();
                pData->nCacheMaxPerProcess = nCacheMaxPerProcess;
                pData->aosArgv = aosArgv;
                if (bIsMEMSource)
                    pData->poMemSrcDS = m_poSrcDS;
            }
            CPL_IGNORE_RET_VAL(aosArgv);
#endif

            CPLSpawnedProcess *hSpawnedProcess = CPLSpawnAsync(
#ifdef FORK_ALLOWED
                m_parallelMethod == "fork" ? GenerateTilesForkMethod :
#endif
                                           nullptr,
                m_parallelMethod == "fork" ? nullptr : aosArgv.List(),
                /* bCreateInputPipe = */ true,
                /* bCreateOutputPipe = */ true,
                /* bCreateErrorPipe = */ false, nullptr);
            if (!hSpawnedProcess)
            {
                ReportError(CE_Failure, CPLE_AppDefined,
                            "Spawning child gdal process '%s' failed",
                            asCommandLines.back().c_str());
                bRet = false;
                break;
            }

            CPLDebugOnly("gdal_raster_tile",
                         "Job for y in [%d,%d] and x in [%d,%d], "
                         "run by process %" PRIu64,
                         iYStart, iYEndIncluded, iXStart, iXEndIncluded,
                         static_cast<uint64_t>(
                             CPLSpawnAsyncGetChildProcessId(hSpawnedProcess)));

            ahSpawnedProcesses.push_back(hSpawnedProcess);

            if (m_parallelMethod == "spawn")
            {
                SendConfigOptions(hSpawnedProcess, bRet);
            }

#ifdef FORK_ALLOWED
            else
            {
                ForkWorkStructure *pData = forkWorkStructures.back().get();
                auto handle = CPLSpawnAsyncGetOutputFileHandle(hSpawnedProcess);
                bRet &= CPL_TO_BOOL(CPLPipeWrite(
                    handle, &pData, static_cast<int>(sizeof(pData))));
            }
#endif

            if (!bRet)
            {
                ReportError(CE_Failure, CPLE_AppDefined,
                            "Could not transmit config options to child gdal "
                            "process '%s'",
                            asCommandLines.back().c_str());
                break;
            }
        }
    }

    uint64_t nCurTile = 0;
    GetProgressForChildProcesses(bRet, ahSpawnedProcesses,
                                 anRemainingTilesForProcess, nCurTile,
                                 nTotalTiles, pfnProgress, pProgressData);

    WaitForSpawnedProcesses(bRet, asCommandLines, ahSpawnedProcesses);

    if (bRet && nCurTile != nBaseTiles)
    {
        bRet = false;
        ReportError(CE_Failure, CPLE_AppDefined,
                    "Not all tiles at max zoom level have been "
                    "generated. Got %" PRIu64 ", expected %" PRIu64,
                    nCurTile, nBaseTiles);
    }

    return bRet;
}

/************************************************************************/
/*     GDALRasterTileAlgorithm::GenerateOverviewTilesSpawnMethod()      */
/************************************************************************/

bool GDALRasterTileAlgorithm::GenerateOverviewTilesSpawnMethod(
    int iZ, int nOvrMinTileX, int nOvrMinTileY, int nOvrMaxTileX,
    int nOvrMaxTileY, std::atomic<uint64_t> &nCurTile, uint64_t nTotalTiles,
    GDALProgressFunc pfnProgress, void *pProgressData)
{
    if (m_parallelMethod == "spawn")
    {
        CPLAssert(!m_osGDALPath.empty());
    }

    const int nOvrTilesPerCol = nOvrMaxTileY - nOvrMinTileY + 1;
    const int nOvrTilesPerRow = nOvrMaxTileX - nOvrMinTileX + 1;
    const uint64_t nExpectedOvrTileCount =
        static_cast<uint64_t>(nOvrTilesPerCol) * nOvrTilesPerRow;

    const int nMaxJobCount = GetMaxChildCount(
        std::max(1, static_cast<int>(std::min<uint64_t>(
                        m_numThreads, nExpectedOvrTileCount /
                                          GetThresholdMinTilesPerJob()))));

    double dfTilesYPerJob;
    int nYOuterIterations;
    double dfTilesXPerJob;
    int nXOuterIterations;
    ComputeJobChunkSize(nMaxJobCount, nOvrTilesPerCol, nOvrTilesPerRow,
                        dfTilesYPerJob, nYOuterIterations, dfTilesXPerJob,
                        nXOuterIterations);

    CPLDebugOnly("gdal_raster_tile",
                 "z=%d, nYOuterIterations=%d, dfTilesYPerJob=%g, "
                 "nXOuterIterations=%d, dfTilesXPerJob=%g",
                 iZ, nYOuterIterations, dfTilesYPerJob, nXOuterIterations,
                 dfTilesXPerJob);

    std::vector<std::string> asCommandLines;
    std::vector<CPLSpawnedProcess *> ahSpawnedProcesses;
    std::vector<uint64_t> anRemainingTilesForProcess;

#ifdef FORK_ALLOWED
    std::vector<std::unique_ptr<ForkWorkStructure>> forkWorkStructures;
#endif

    const uint64_t nCacheMaxPerProcess = GDALGetCacheMax64() / nMaxJobCount;

    const auto poSrcDriver = m_poSrcDS ? m_poSrcDS->GetDriver() : nullptr;
    const bool bIsMEMSource =
        poSrcDriver && EQUAL(poSrcDriver->GetDescription(), "MEM");

    int nLastYEndIncluded = nOvrMinTileY - 1;
    bool bRet = true;
    for (int iYOuterIter = 0; bRet && iYOuterIter < nYOuterIterations &&
                              nLastYEndIncluded < nOvrMaxTileY;
         ++iYOuterIter)
    {
        const int iYStart = nLastYEndIncluded + 1;
        const int iYEndIncluded =
            iYOuterIter + 1 == nYOuterIterations
                ? nOvrMaxTileY
                : std::max(iYStart,
                           static_cast<int>(std::floor(
                               nOvrMinTileY +
                               (iYOuterIter + 1) * dfTilesYPerJob - 1)));

        nLastYEndIncluded = iYEndIncluded;

        int nLastXEndIncluded = nOvrMinTileX - 1;
        for (int iXOuterIter = 0; bRet && iXOuterIter < nXOuterIterations &&
                                  nLastXEndIncluded < nOvrMaxTileX;
             ++iXOuterIter)
        {
            const int iXStart = nLastXEndIncluded + 1;
            const int iXEndIncluded =
                iXOuterIter + 1 == nXOuterIterations
                    ? nOvrMaxTileX
                    : std::max(iXStart,
                               static_cast<int>(std::floor(
                                   nOvrMinTileX +
                                   (iXOuterIter + 1) * dfTilesXPerJob - 1)));

            nLastXEndIncluded = iXEndIncluded;

            anRemainingTilesForProcess.push_back(
                static_cast<uint64_t>(iYEndIncluded - iYStart + 1) *
                (iXEndIncluded - iXStart + 1));

            CPLStringList aosArgv;
            if (m_parallelMethod == "spawn")
            {
                aosArgv.push_back(m_osGDALPath.c_str());
                aosArgv.push_back("raster");
                aosArgv.push_back("tile");
                aosArgv.push_back("--config-options-in-stdin");
                aosArgv.push_back("--config");
                aosArgv.push_back("GDAL_NUM_THREADS=1");
                aosArgv.push_back("--config");
                aosArgv.push_back(
                    CPLSPrintf("GDAL_CACHEMAX=%" PRIu64, nCacheMaxPerProcess));
            }
            aosArgv.push_back(
                std::string("--").append(GDAL_ARG_NAME_NUM_THREADS).c_str());
            aosArgv.push_back("1");
            aosArgv.push_back("--ovr-zoom-level");
            aosArgv.push_back(CPLSPrintf("%d", iZ));
            aosArgv.push_back("--ovr-min-x");
            aosArgv.push_back(CPLSPrintf("%d", iXStart));
            aosArgv.push_back("--ovr-max-x");
            aosArgv.push_back(CPLSPrintf("%d", iXEndIncluded));
            aosArgv.push_back("--ovr-min-y");
            aosArgv.push_back(CPLSPrintf("%d", iYStart));
            aosArgv.push_back("--ovr-max-y");
            aosArgv.push_back(CPLSPrintf("%d", iYEndIncluded));
            aosArgv.push_back("--webviewer");
            aosArgv.push_back("none");
            aosArgv.push_back(m_parallelMethod == "spawn" ? "--spawned"
                                                          : "--forked");
            if (!bIsMEMSource)
            {
                aosArgv.push_back("--input");
                aosArgv.push_back(m_inputDataset[0].GetName().c_str());
            }
            for (const auto &arg : GetArgs())
            {
                if (arg->IsExplicitlySet() && arg->GetName() != "progress" &&
                    arg->GetName() != "progress-forked" &&
                    arg->GetName() != GDAL_ARG_NAME_INPUT &&
                    arg->GetName() != GDAL_ARG_NAME_NUM_THREADS &&
                    arg->GetName() != "webviewer" &&
                    arg->GetName() != "parallel-method")
                {
                    if (!AddArgToArgv(arg.get(), aosArgv))
                        return false;
                }
            }

            std::string cmdLine;
            for (const char *arg : aosArgv)
            {
                if (!cmdLine.empty())
                    cmdLine += ' ';
                CPLString sArg(arg);
                if (sArg.find_first_of(" \"") != std::string::npos)
                {
                    cmdLine += '"';
                    cmdLine += sArg.replaceAll('"', "\\\"");
                    cmdLine += '"';
                }
                else
                    cmdLine += sArg;
            }
            CPLDebugOnly("gdal_raster_tile", "%s %s",
                         m_parallelMethod == "spawn" ? "Spawning" : "Forking",
                         cmdLine.c_str());
            asCommandLines.push_back(std::move(cmdLine));

#ifdef FORK_ALLOWED
            if (m_parallelMethod == "fork")
            {
                forkWorkStructures.push_back(
                    std::make_unique<ForkWorkStructure>());
                ForkWorkStructure *pData = forkWorkStructures.back().get();
                pData->nCacheMaxPerProcess = nCacheMaxPerProcess;
                pData->aosArgv = aosArgv;
                if (bIsMEMSource)
                    pData->poMemSrcDS = m_poSrcDS;
            }
            CPL_IGNORE_RET_VAL(aosArgv);
#endif

            CPLSpawnedProcess *hSpawnedProcess = CPLSpawnAsync(
#ifdef FORK_ALLOWED
                m_parallelMethod == "fork" ? GenerateTilesForkMethod :
#endif
                                           nullptr,
                m_parallelMethod == "fork" ? nullptr : aosArgv.List(),
                /* bCreateInputPipe = */ true,
                /* bCreateOutputPipe = */ true,
                /* bCreateErrorPipe = */ true, nullptr);
            if (!hSpawnedProcess)
            {
                ReportError(CE_Failure, CPLE_AppDefined,
                            "Spawning child gdal process '%s' failed",
                            asCommandLines.back().c_str());
                bRet = false;
                break;
            }

            CPLDebugOnly("gdal_raster_tile",
                         "Job for z = %d, y in [%d,%d] and x in [%d,%d], "
                         "run by process %" PRIu64,
                         iZ, iYStart, iYEndIncluded, iXStart, iXEndIncluded,
                         static_cast<uint64_t>(
                             CPLSpawnAsyncGetChildProcessId(hSpawnedProcess)));

            ahSpawnedProcesses.push_back(hSpawnedProcess);

            if (m_parallelMethod == "spawn")
            {
                SendConfigOptions(hSpawnedProcess, bRet);
            }

#ifdef FORK_ALLOWED
            else
            {
                ForkWorkStructure *pData = forkWorkStructures.back().get();
                auto handle = CPLSpawnAsyncGetOutputFileHandle(hSpawnedProcess);
                bRet &= CPL_TO_BOOL(CPLPipeWrite(
                    handle, &pData, static_cast<int>(sizeof(pData))));
            }
#endif
            if (!bRet)
            {
                ReportError(CE_Failure, CPLE_AppDefined,
                            "Could not transmit config options to child gdal "
                            "process '%s'",
                            asCommandLines.back().c_str());
                break;
            }
        }
    }

    uint64_t nCurTileLocal = nCurTile;
    GetProgressForChildProcesses(bRet, ahSpawnedProcesses,
                                 anRemainingTilesForProcess, nCurTileLocal,
                                 nTotalTiles, pfnProgress, pProgressData);

    WaitForSpawnedProcesses(bRet, asCommandLines, ahSpawnedProcesses);

    if (bRet && nCurTileLocal - nCurTile != nExpectedOvrTileCount)
    {
        bRet = false;
        ReportError(CE_Failure, CPLE_AppDefined,
                    "Not all tiles at zoom level %d have been "
                    "generated. Got %" PRIu64 ", expected %" PRIu64,
                    iZ, nCurTileLocal - nCurTile, nExpectedOvrTileCount);
    }

    nCurTile = nCurTileLocal;

    return bRet;
}

/************************************************************************/
/*                  GDALRasterTileAlgorithm::RunImpl()                  */
/************************************************************************/

bool GDALRasterTileAlgorithm::RunImpl(GDALProgressFunc pfnProgress,
                                      void *pProgressData)
{
    GDALPipelineStepRunContext stepCtxt;
    stepCtxt.m_pfnProgress = pfnProgress;
    stepCtxt.m_pProgressData = pProgressData;
    return RunStep(stepCtxt);
}

/************************************************************************/
/*                        SpawnedErrorHandler()                         */
/************************************************************************/

static void CPL_STDCALL SpawnedErrorHandler(CPLErr eErr, CPLErrorNum eNum,
                                            const char *pszMsg)
{
    fwrite(ERROR_START_MARKER, sizeof(ERROR_START_MARKER), 1, stdout);
    uint32_t nErr = eErr;
    fwrite(&nErr, sizeof(nErr), 1, stdout);
    uint32_t nNum = eNum;
    fwrite(&nNum, sizeof(nNum), 1, stdout);
    uint16_t nLen = static_cast<uint16_t>(strlen(pszMsg));
    fwrite(&nLen, sizeof(nLen), 1, stdout);
    fwrite(pszMsg, nLen, 1, stdout);
    fflush(stdout);
}

/************************************************************************/
/*                  GDALRasterTileAlgorithm::RunStep()                  */
/************************************************************************/

bool GDALRasterTileAlgorithm::RunStep(GDALPipelineStepRunContext &ctxt)
{
    auto pfnProgress = ctxt.m_pfnProgress;
    auto pProgressData = ctxt.m_pProgressData;
    CPLAssert(m_inputDataset.size() == 1);
    m_poSrcDS = m_inputDataset[0].GetDatasetRef();
    CPLAssert(m_poSrcDS);

    const int nSrcWidth = m_poSrcDS->GetRasterXSize();
    const int nSrcHeight = m_poSrcDS->GetRasterYSize();
    if (m_poSrcDS->GetRasterCount() == 0 || nSrcWidth == 0 || nSrcHeight == 0)
    {
        ReportError(CE_Failure, CPLE_AppDefined, "Invalid source dataset");
        return false;
    }

    const bool bIsNamedSource = m_poSrcDS->GetDescription()[0] != 0;
    auto poSrcDriver = m_poSrcDS->GetDriver();
    const bool bIsMEMSource =
        poSrcDriver && EQUAL(poSrcDriver->GetDescription(), "MEM");
    m_bIsNamedNonMemSrcDS = bIsNamedSource && !bIsMEMSource;
    const bool bSrcIsFineForFork = bIsNamedSource || bIsMEMSource;

    if (m_parallelMethod == "spawn")
    {
        const char *pszErrorMsg = "";
        if (!IsCompatibleOfSpawn(pszErrorMsg))
        {
            if (pszErrorMsg[0])
                ReportError(CE_Failure, CPLE_AppDefined, "%s", pszErrorMsg);
            return false;
        }
    }
#ifdef FORK_ALLOWED
    else if (m_parallelMethod == "fork")
    {
        if (!bSrcIsFineForFork)
        {
            ReportError(CE_Failure, CPLE_AppDefined,
                        "Unnamed non-MEM source are not supported "
                        "with fork parallelization method");
            return false;
        }
        if (cpl::starts_with(m_output, "/vsimem/"))
        {
            ReportError(CE_Failure, CPLE_AppDefined,
                        "/vsimem/ output directory not supported with fork "
                        "parallelization method");
            return false;
        }
    }
#endif

    if (m_resampling == "near")
        m_resampling = "nearest";
    if (m_overviewResampling == "near")
        m_overviewResampling = "nearest";
    else if (m_overviewResampling.empty())
        m_overviewResampling = m_resampling;

    CPLStringList aosWarpOptions;
    if (!m_excludedValues.empty() || m_nodataValuesPctThreshold < 100)
    {
        aosWarpOptions.SetNameValue(
            "NODATA_VALUES_PCT_THRESHOLD",
            CPLSPrintf("%g", m_nodataValuesPctThreshold));
        if (!m_excludedValues.empty())
        {
            aosWarpOptions.SetNameValue("EXCLUDED_VALUES",
                                        m_excludedValues.c_str());
            aosWarpOptions.SetNameValue(
                "EXCLUDED_VALUES_PCT_THRESHOLD",
                CPLSPrintf("%g", m_excludedValuesPctThreshold));
        }
    }

    if (m_poSrcDS->GetRasterBand(1)->GetColorInterpretation() ==
            GCI_PaletteIndex &&
        ((m_resampling != "nearest" && m_resampling != "mode") ||
         (m_overviewResampling != "nearest" && m_overviewResampling != "mode")))
    {
        ReportError(CE_Failure, CPLE_NotSupported,
                    "Datasets with color table not supported with non-nearest "
                    "or non-mode resampling. Run 'gdal raster "
                    "color-map' before or set the 'resampling' argument to "
                    "'nearest' or 'mode'.");
        return false;
    }

    const auto eSrcDT = m_poSrcDS->GetRasterBand(1)->GetRasterDataType();
    m_poDstDriver = GetGDALDriverManager()->GetDriverByName(m_format.c_str());
    if (!m_poDstDriver)
    {
        ReportError(CE_Failure, CPLE_AppDefined,
                    "Invalid value for argument 'output-format'. Driver '%s' "
                    "does not exist",
                    m_format.c_str());
        return false;
    }

    if (!ValidateOutputFormat(eSrcDT))
        return false;

    const char *pszExtensions =
        m_poDstDriver->GetMetadataItem(GDAL_DMD_EXTENSIONS);
    CPLAssert(pszExtensions && pszExtensions[0] != 0);
    const CPLStringList aosExtensions(
        CSLTokenizeString2(pszExtensions, " ", 0));
    const char *pszExtension = aosExtensions[0];
    GDALGeoTransform srcGT;
    const bool bHasSrcGT = m_poSrcDS->GetGeoTransform(srcGT) == CE_None;
    const bool bHasNorthUpSrcGT =
        bHasSrcGT && srcGT[2] == 0 && srcGT[4] == 0 && srcGT[5] < 0;
    OGRSpatialReference oSRS_TMS;

    if (m_tilingScheme == "raster")
    {
        if (const auto poSRS = m_poSrcDS->GetSpatialRef())
            oSRS_TMS = *poSRS;
    }
    else
    {
        if (!bHasSrcGT && m_poSrcDS->GetGCPCount() == 0 &&
            m_poSrcDS->GetMetadata("GEOLOCATION") == nullptr &&
            m_poSrcDS->GetMetadata("RPC") == nullptr)
        {
            ReportError(CE_Failure, CPLE_NotSupported,
                        "Ungeoreferenced datasets are not supported, unless "
                        "'tiling-scheme' is set to 'raster'");
            return false;
        }

        if (m_poSrcDS->GetMetadata("GEOLOCATION") == nullptr &&
            m_poSrcDS->GetMetadata("RPC") == nullptr &&
            m_poSrcDS->GetSpatialRef() == nullptr &&
            m_poSrcDS->GetGCPSpatialRef() == nullptr)
        {
            ReportError(CE_Failure, CPLE_NotSupported,
                        "Ungeoreferenced datasets are not supported, unless "
                        "'tiling-scheme' is set to 'raster'");
            return false;
        }
    }

    if (m_copySrcMetadata)
    {
        CPLStringList aosMD(CSLDuplicate(m_poSrcDS->GetMetadata()));
        const CPLStringList aosNewMD(m_metadata);
        for (const auto [key, value] : cpl::IterateNameValue(aosNewMD))
        {
            aosMD.SetNameValue(key, value);
        }
        m_metadata = aosMD;
    }

    std::vector<BandMetadata> aoBandMetadata;
    for (int i = 1; i <= m_poSrcDS->GetRasterCount(); ++i)
    {
        auto poBand = m_poSrcDS->GetRasterBand(i);
        BandMetadata bm;
        bm.osDescription = poBand->GetDescription();
        bm.eDT = poBand->GetRasterDataType();
        bm.eColorInterp = poBand->GetColorInterpretation();
        if (const char *pszCenterWavelength =
                poBand->GetMetadataItem("CENTRAL_WAVELENGTH_UM", "IMAGERY"))
            bm.osCenterWaveLength = pszCenterWavelength;
        if (const char *pszFWHM = poBand->GetMetadataItem("FWHM_UM", "IMAGERY"))
            bm.osFWHM = pszFWHM;
        aoBandMetadata.emplace_back(std::move(bm));
    }

    GDALGeoTransform srcGTModif{0, 1, 0, 0, 0, -1};

    if (m_tilingScheme == "mercator")
        m_tilingScheme = "WebMercatorQuad";
    else if (m_tilingScheme == "geodetic")
        m_tilingScheme = "WorldCRS84Quad";
    else if (m_tilingScheme == "raster")
    {
        if (m_tileSize == 0)
            m_tileSize = 256;
        if (m_maxZoomLevel < 0)
        {
            m_maxZoomLevel = static_cast<int>(std::ceil(std::log2(
                std::max(1, std::max(nSrcWidth, nSrcHeight) / m_tileSize))));
        }
        if (bHasNorthUpSrcGT)
        {
            srcGTModif = srcGT;
        }
    }

    auto poTMS =
        m_tilingScheme == "raster"
            ? gdal::TileMatrixSet::createRaster(
                  nSrcWidth, nSrcHeight, m_tileSize, 1 + m_maxZoomLevel,
                  srcGTModif[0], srcGTModif[3], srcGTModif[1], -srcGTModif[5],
                  oSRS_TMS.IsEmpty() ? std::string() : oSRS_TMS.exportToWkt())
            : gdal::TileMatrixSet::parse(
                  m_mapTileMatrixIdentifierToScheme[m_tilingScheme].c_str());
    // Enforced by SetChoices() on the m_tilingScheme argument
    CPLAssert(poTMS && !poTMS->hasVariableMatrixWidth());

    CPLStringList aosTO;
    if (m_tilingScheme == "raster")
    {
        aosTO.SetNameValue("SRC_METHOD", "GEOTRANSFORM");
    }
    else
    {
        CPL_IGNORE_RET_VAL(oSRS_TMS.SetFromUserInput(poTMS->crs().c_str()));
        aosTO.SetNameValue("DST_SRS", oSRS_TMS.exportToWkt().c_str());
    }

    const char *pszAuthName = oSRS_TMS.GetAuthorityName(nullptr);
    const char *pszAuthCode = oSRS_TMS.GetAuthorityCode(nullptr);
    const int nEPSGCode =
        (pszAuthName && pszAuthCode && EQUAL(pszAuthName, "EPSG"))
            ? atoi(pszAuthCode)
            : 0;

    const bool bInvertAxisTMS =
        m_tilingScheme != "raster" &&
        (oSRS_TMS.EPSGTreatsAsLatLong() != FALSE ||
         oSRS_TMS.EPSGTreatsAsNorthingEasting() != FALSE);

    oSRS_TMS.SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);

    std::unique_ptr<void, decltype(&GDALDestroyTransformer)> hTransformArg(
        nullptr, GDALDestroyTransformer);

    // Hack to compensate for GDALSuggestedWarpOutput2() failure (or not
    // ideal suggestion with PROJ 8) when reprojecting latitude = +/- 90 to
    // EPSG:3857.
    std::unique_ptr<GDALDataset> poTmpDS;
    bool bEPSG3857Adjust = false;
    if (nEPSGCode == 3857 && bHasNorthUpSrcGT)
    {
        const auto poSrcSRS = m_poSrcDS->GetSpatialRef();
        if (poSrcSRS && poSrcSRS->IsGeographic())
        {
            double maxLat = srcGT[3];
            double minLat = srcGT[3] + nSrcHeight * srcGT[5];
            // Corresponds to the latitude of below MAX_GM
            constexpr double MAX_LAT = 85.0511287798066;
            bool bModified = false;
            if (maxLat > MAX_LAT)
            {
                maxLat = MAX_LAT;
                bModified = true;
            }
            if (minLat < -MAX_LAT)
            {
                minLat = -MAX_LAT;
                bModified = true;
            }
            if (bModified)
            {
                CPLStringList aosOptions;
                aosOptions.AddString("-of");
                aosOptions.AddString("VRT");
                aosOptions.AddString("-projwin");
                aosOptions.AddString(CPLSPrintf("%.17g", srcGT[0]));
                aosOptions.AddString(CPLSPrintf("%.17g", maxLat));
                aosOptions.AddString(
                    CPLSPrintf("%.17g", srcGT[0] + nSrcWidth * srcGT[1]));
                aosOptions.AddString(CPLSPrintf("%.17g", minLat));
                auto psOptions =
                    GDALTranslateOptionsNew(aosOptions.List(), nullptr);
                poTmpDS.reset(GDALDataset::FromHandle(GDALTranslate(
                    "", GDALDataset::ToHandle(m_poSrcDS), psOptions, nullptr)));
                GDALTranslateOptionsFree(psOptions);
                if (poTmpDS)
                {
                    bEPSG3857Adjust = true;
                    hTransformArg.reset(GDALCreateGenImgProjTransformer2(
                        GDALDataset::FromHandle(poTmpDS.get()), nullptr,
                        aosTO.List()));
                }
            }
        }
    }

    GDALGeoTransform dstGT;
    double adfExtent[4];
    int nXSize, nYSize;

    bool bSuggestOK;
    if (m_tilingScheme == "raster")
    {
        bSuggestOK = true;
        nXSize = nSrcWidth;
        nYSize = nSrcHeight;
        dstGT = srcGTModif;
        adfExtent[0] = dstGT[0];
        adfExtent[1] = dstGT[3] + nSrcHeight * dstGT[5];
        adfExtent[2] = dstGT[0] + nSrcWidth * dstGT[1];
        adfExtent[3] = dstGT[3];
    }
    else
    {
        if (!hTransformArg)
        {
            hTransformArg.reset(GDALCreateGenImgProjTransformer2(
                m_poSrcDS, nullptr, aosTO.List()));
        }
        if (!hTransformArg)
        {
            return false;
        }
        CPLErrorStateBackuper oBackuper(CPLQuietErrorHandler);
        bSuggestOK =
            (GDALSuggestedWarpOutput2(
                 m_poSrcDS,
                 static_cast<GDALTransformerInfo *>(hTransformArg.get())
                     ->pfnTransform,
                 hTransformArg.get(), dstGT.data(), &nXSize, &nYSize, adfExtent,
                 0) == CE_None);
    }
    if (!bSuggestOK)
    {
        ReportError(CE_Failure, CPLE_AppDefined,
                    "Cannot determine extent of raster in target CRS");
        return false;
    }

    poTmpDS.reset();

    if (bEPSG3857Adjust)
    {
        constexpr double SPHERICAL_RADIUS = 6378137.0;
        constexpr double MAX_GM =
            SPHERICAL_RADIUS * M_PI;  // 20037508.342789244
        double maxNorthing = dstGT[3];
        double minNorthing = dstGT[3] + dstGT[5] * nYSize;
        bool bChanged = false;
        if (maxNorthing > MAX_GM)
        {
            bChanged = true;
            maxNorthing = MAX_GM;
        }
        if (minNorthing < -MAX_GM)
        {
            bChanged = true;
            minNorthing = -MAX_GM;
        }
        if (bChanged)
        {
            dstGT[3] = maxNorthing;
            nYSize = int((maxNorthing - minNorthing) / (-dstGT[5]) + 0.5);
            adfExtent[1] = maxNorthing + nYSize * dstGT[5];
            adfExtent[3] = maxNorthing;
        }
    }

    const auto &tileMatrixList = poTMS->tileMatrixList();
    if (m_maxZoomLevel >= 0)
    {
        if (m_maxZoomLevel >= static_cast<int>(tileMatrixList.size()))
        {
            ReportError(CE_Failure, CPLE_AppDefined,
                        "max-zoom = %d is invalid. It must be in [0,%d] range",
                        m_maxZoomLevel,
                        static_cast<int>(tileMatrixList.size()) - 1);
            return false;
        }
    }
    else
    {
        const double dfComputedRes = dstGT[1];
        double dfPrevRes = 0.0;
        double dfRes = 0.0;
        constexpr double EPSILON = 1e-8;

        if (m_minZoomLevel >= 0)
            m_maxZoomLevel = m_minZoomLevel;
        else
            m_maxZoomLevel = 0;

        for (; m_maxZoomLevel < static_cast<int>(tileMatrixList.size());
             m_maxZoomLevel++)
        {
            dfRes = tileMatrixList[m_maxZoomLevel].mResX;
            if (dfComputedRes > dfRes ||
                fabs(dfComputedRes - dfRes) / dfRes <= EPSILON)
                break;
            dfPrevRes = dfRes;
        }
        if (m_maxZoomLevel >= static_cast<int>(tileMatrixList.size()))
        {
            ReportError(CE_Failure, CPLE_AppDefined,
                        "Could not find an appropriate zoom level. Perhaps "
                        "min-zoom is too large?");
            return false;
        }

        if (m_maxZoomLevel > 0 && fabs(dfComputedRes - dfRes) / dfRes > EPSILON)
        {
            // Round to closest resolution
            if (dfPrevRes / dfComputedRes < dfComputedRes / dfRes)
                m_maxZoomLevel--;
        }
    }
    if (m_minZoomLevel < 0)
        m_minZoomLevel = m_maxZoomLevel;

    auto tileMatrix = tileMatrixList[m_maxZoomLevel];
    int nMinTileX = 0;
    int nMinTileY = 0;
    int nMaxTileX = 0;
    int nMaxTileY = 0;
    bool bIntersects = false;
    if (!GetTileIndices(tileMatrix, bInvertAxisTMS, m_tileSize, adfExtent,
                        nMinTileX, nMinTileY, nMaxTileX, nMaxTileY,
                        m_noIntersectionIsOK, bIntersects,
                        /* checkRasterOverflow = */ false))
    {
        return false;
    }
    if (!bIntersects)
        return true;

    // Potentially restrict tiling to user specified coordinates
    if (m_minTileX >= tileMatrix.mMatrixWidth)
    {
        ReportError(CE_Failure, CPLE_IllegalArg,
                    "'min-x' value must be in [0,%d] range",
                    tileMatrix.mMatrixWidth - 1);
        return false;
    }
    if (m_maxTileX >= tileMatrix.mMatrixWidth)
    {
        ReportError(CE_Failure, CPLE_IllegalArg,
                    "'max-x' value must be in [0,%d] range",
                    tileMatrix.mMatrixWidth - 1);
        return false;
    }
    if (m_minTileY >= tileMatrix.mMatrixHeight)
    {
        ReportError(CE_Failure, CPLE_IllegalArg,
                    "'min-y' value must be in [0,%d] range",
                    tileMatrix.mMatrixHeight - 1);
        return false;
    }
    if (m_maxTileY >= tileMatrix.mMatrixHeight)
    {
        ReportError(CE_Failure, CPLE_IllegalArg,
                    "'max-y' value must be in [0,%d] range",
                    tileMatrix.mMatrixHeight - 1);
        return false;
    }

    if ((m_minTileX >= 0 && m_minTileX > nMaxTileX) ||
        (m_minTileY >= 0 && m_minTileY > nMaxTileY) ||
        (m_maxTileX >= 0 && m_maxTileX < nMinTileX) ||
        (m_maxTileY >= 0 && m_maxTileY < nMinTileY))
    {
        ReportError(
            m_noIntersectionIsOK ? CE_Warning : CE_Failure, CPLE_AppDefined,
            "Dataset extent not intersecting specified min/max X/Y tile "
            "coordinates");
        return m_noIntersectionIsOK;
    }
    if (m_minTileX >= 0 && m_minTileX > nMinTileX)
    {
        nMinTileX = m_minTileX;
        adfExtent[0] = tileMatrix.mTopLeftX +
                       nMinTileX * tileMatrix.mResX * tileMatrix.mTileWidth;
    }
    if (m_minTileY >= 0 && m_minTileY > nMinTileY)
    {
        nMinTileY = m_minTileY;
        adfExtent[3] = tileMatrix.mTopLeftY -
                       nMinTileY * tileMatrix.mResY * tileMatrix.mTileHeight;
    }
    if (m_maxTileX >= 0 && m_maxTileX < nMaxTileX)
    {
        nMaxTileX = m_maxTileX;
        adfExtent[2] = tileMatrix.mTopLeftX + (nMaxTileX + 1) *
                                                  tileMatrix.mResX *
                                                  tileMatrix.mTileWidth;
    }
    if (m_maxTileY >= 0 && m_maxTileY < nMaxTileY)
    {
        nMaxTileY = m_maxTileY;
        adfExtent[1] = tileMatrix.mTopLeftY - (nMaxTileY + 1) *
                                                  tileMatrix.mResY *
                                                  tileMatrix.mTileHeight;
    }

    if (nMaxTileX - nMinTileX + 1 > INT_MAX / tileMatrix.mTileWidth ||
        nMaxTileY - nMinTileY + 1 > INT_MAX / tileMatrix.mTileHeight)
    {
        ReportError(CE_Failure, CPLE_AppDefined, "Too large zoom level");
        return false;
    }

    dstGT[0] = tileMatrix.mTopLeftX +
               nMinTileX * tileMatrix.mResX * tileMatrix.mTileWidth;
    dstGT[1] = tileMatrix.mResX;
    dstGT[2] = 0;
    dstGT[3] = tileMatrix.mTopLeftY -
               nMinTileY * tileMatrix.mResY * tileMatrix.mTileHeight;
    dstGT[4] = 0;
    dstGT[5] = -tileMatrix.mResY;

    /* -------------------------------------------------------------------- */
    /*      Setup warp options.                                             */
    /* -------------------------------------------------------------------- */
    std::unique_ptr<GDALWarpOptions, decltype(&GDALDestroyWarpOptions)> psWO(
        GDALCreateWarpOptions(), GDALDestroyWarpOptions);

    psWO->papszWarpOptions = CSLSetNameValue(nullptr, "OPTIMIZE_SIZE", "YES");
    psWO->papszWarpOptions =
        CSLSetNameValue(psWO->papszWarpOptions, "SAMPLE_GRID", "YES");
    psWO->papszWarpOptions =
        CSLMerge(psWO->papszWarpOptions, aosWarpOptions.List());

    int bHasSrcNoData = false;
    const double dfSrcNoDataValue =
        m_poSrcDS->GetRasterBand(1)->GetNoDataValue(&bHasSrcNoData);

    const bool bLastSrcBandIsAlpha =
        (m_poSrcDS->GetRasterCount() > 1 &&
         m_poSrcDS->GetRasterBand(m_poSrcDS->GetRasterCount())
                 ->GetColorInterpretation() == GCI_AlphaBand);

    const bool bOutputSupportsAlpha = !EQUAL(m_format.c_str(), "JPEG");
    const bool bOutputSupportsNoData = EQUAL(m_format.c_str(), "GTiff");
    const bool bDstNoDataSpecified = GetArg("dst-nodata")->IsExplicitlySet();
    auto poColorTable = std::unique_ptr<GDALColorTable>(
        [this]()
        {
            auto poCT = m_poSrcDS->GetRasterBand(1)->GetColorTable();
            return poCT ? poCT->Clone() : nullptr;
        }());

    const bool bUserAskedForAlpha = m_addalpha;
    if (!m_noalpha && !m_addalpha)
    {
        m_addalpha = !(bHasSrcNoData && bOutputSupportsNoData) &&
                     !bDstNoDataSpecified && poColorTable == nullptr;
    }
    m_addalpha &= bOutputSupportsAlpha;

    psWO->nBandCount = m_poSrcDS->GetRasterCount();
    if (bLastSrcBandIsAlpha)
    {
        --psWO->nBandCount;
        psWO->nSrcAlphaBand = m_poSrcDS->GetRasterCount();
    }

    if (bHasSrcNoData)
    {
        psWO->padfSrcNoDataReal =
            static_cast<double *>(CPLCalloc(psWO->nBandCount, sizeof(double)));
        for (int i = 0; i < psWO->nBandCount; ++i)
        {
            psWO->padfSrcNoDataReal[i] = dfSrcNoDataValue;
        }
    }

    if ((bHasSrcNoData && !m_addalpha && bOutputSupportsNoData) ||
        bDstNoDataSpecified)
    {
        psWO->padfDstNoDataReal =
            static_cast<double *>(CPLCalloc(psWO->nBandCount, sizeof(double)));
        for (int i = 0; i < psWO->nBandCount; ++i)
        {
            psWO->padfDstNoDataReal[i] =
                bDstNoDataSpecified ? m_dstNoData : dfSrcNoDataValue;
        }
    }

    psWO->eWorkingDataType = eSrcDT;

    GDALGetWarpResampleAlg(m_resampling.c_str(), psWO->eResampleAlg);

    /* -------------------------------------------------------------------- */
    /*      Setup band mapping.                                             */
    /* -------------------------------------------------------------------- */

    psWO->panSrcBands =
        static_cast<int *>(CPLMalloc(psWO->nBandCount * sizeof(int)));
    psWO->panDstBands =
        static_cast<int *>(CPLMalloc(psWO->nBandCount * sizeof(int)));

    for (int i = 0; i < psWO->nBandCount; i++)
    {
        psWO->panSrcBands[i] = i + 1;
        psWO->panDstBands[i] = i + 1;
    }

    if (m_addalpha)
        psWO->nDstAlphaBand = psWO->nBandCount + 1;

    const int nDstBands =
        psWO->nDstAlphaBand ? psWO->nDstAlphaBand : psWO->nBandCount;

    std::vector<GByte> dstBuffer;
    const bool bIsPNGOutput = EQUAL(pszExtension, "png");
    uint64_t dstBufferSize =
        (static_cast<uint64_t>(tileMatrix.mTileWidth) *
             // + 1 for PNG filter type / row byte
             nDstBands * GDALGetDataTypeSizeBytes(psWO->eWorkingDataType) +
         (bIsPNGOutput ? 1 : 0)) *
        tileMatrix.mTileHeight;
    if (bIsPNGOutput)
    {
        // Security margin for deflate compression
        dstBufferSize += dstBufferSize / 10;
    }
    const uint64_t nUsableRAM =
        std::min<uint64_t>(INT_MAX, CPLGetUsablePhysicalRAM() / 4);
    if (dstBufferSize <=
        (nUsableRAM ? nUsableRAM : static_cast<uint64_t>(INT_MAX)))
    {
        try
        {
            dstBuffer.resize(static_cast<size_t>(dstBufferSize));
        }
        catch (const std::exception &)
        {
        }
    }
    if (dstBuffer.size() < dstBufferSize)
    {
        ReportError(CE_Failure, CPLE_AppDefined,
                    "Tile size and/or number of bands too large compared to "
                    "available RAM");
        return false;
    }

    FakeMaxZoomDataset oFakeMaxZoomDS(
        (nMaxTileX - nMinTileX + 1) * tileMatrix.mTileWidth,
        (nMaxTileY - nMinTileY + 1) * tileMatrix.mTileHeight, nDstBands,
        tileMatrix.mTileWidth, tileMatrix.mTileHeight, psWO->eWorkingDataType,
        dstGT, oSRS_TMS, dstBuffer);
    CPL_IGNORE_RET_VAL(oFakeMaxZoomDS.GetSpatialRef());

    psWO->hSrcDS = GDALDataset::ToHandle(m_poSrcDS);
    psWO->hDstDS = GDALDataset::ToHandle(&oFakeMaxZoomDS);

    std::unique_ptr<GDALDataset> tmpSrcDS;
    if (m_tilingScheme == "raster" && !bHasNorthUpSrcGT)
    {
        CPLStringList aosOptions;
        aosOptions.AddString("-of");
        aosOptions.AddString("VRT");
        aosOptions.AddString("-a_ullr");
        aosOptions.AddString(CPLSPrintf("%.17g", srcGTModif[0]));
        aosOptions.AddString(CPLSPrintf("%.17g", srcGTModif[3]));
        aosOptions.AddString(
            CPLSPrintf("%.17g", srcGTModif[0] + nSrcWidth * srcGTModif[1]));
        aosOptions.AddString(
            CPLSPrintf("%.17g", srcGTModif[3] + nSrcHeight * srcGTModif[5]));
        if (oSRS_TMS.IsEmpty())
        {
            aosOptions.AddString("-a_srs");
            aosOptions.AddString("none");
        }

        GDALTranslateOptions *psOptions =
            GDALTranslateOptionsNew(aosOptions.List(), nullptr);

        tmpSrcDS.reset(GDALDataset::FromHandle(GDALTranslate(
            "", GDALDataset::ToHandle(m_poSrcDS), psOptions, nullptr)));
        GDALTranslateOptionsFree(psOptions);
        if (!tmpSrcDS)
            return false;
    }
    hTransformArg.reset(GDALCreateGenImgProjTransformer2(
        tmpSrcDS ? tmpSrcDS.get() : m_poSrcDS, &oFakeMaxZoomDS, aosTO.List()));
    CPLAssert(hTransformArg);

    /* -------------------------------------------------------------------- */
    /*      Warp the transformer with a linear approximator                 */
    /* -------------------------------------------------------------------- */
    hTransformArg.reset(GDALCreateApproxTransformer(
        GDALGenImgProjTransform, hTransformArg.release(), 0.125));
    GDALApproxTransformerOwnsSubtransformer(hTransformArg.get(), TRUE);

    psWO->pfnTransformer = GDALApproxTransform;
    psWO->pTransformerArg = hTransformArg.get();

    /* -------------------------------------------------------------------- */
    /*      Determine total number of tiles                                 */
    /* -------------------------------------------------------------------- */
    const int nBaseTilesPerRow = nMaxTileX - nMinTileX + 1;
    const int nBaseTilesPerCol = nMaxTileY - nMinTileY + 1;
    const uint64_t nBaseTiles =
        static_cast<uint64_t>(nBaseTilesPerCol) * nBaseTilesPerRow;
    uint64_t nTotalTiles = nBaseTiles;
    std::atomic<uint64_t> nCurTile = 0;
    bool bRet = true;

    for (int iZ = m_maxZoomLevel - 1;
         bRet && bIntersects && iZ >= m_minZoomLevel; --iZ)
    {
        auto ovrTileMatrix = tileMatrixList[iZ];
        int nOvrMinTileX = 0;
        int nOvrMinTileY = 0;
        int nOvrMaxTileX = 0;
        int nOvrMaxTileY = 0;
        bRet =
            GetTileIndices(ovrTileMatrix, bInvertAxisTMS, m_tileSize, adfExtent,
                           nOvrMinTileX, nOvrMinTileY, nOvrMaxTileX,
                           nOvrMaxTileY, m_noIntersectionIsOK, bIntersects);
        if (bIntersects)
        {
            nTotalTiles +=
                static_cast<uint64_t>(nOvrMaxTileY - nOvrMinTileY + 1) *
                (nOvrMaxTileX - nOvrMinTileX + 1);
        }
    }

    /* -------------------------------------------------------------------- */
    /*      Generate tiles at max zoom level                                */
    /* -------------------------------------------------------------------- */
    GDALWarpOperation oWO;

    bRet = oWO.Initialize(psWO.get()) == CE_None && bRet;

    const auto GetUpdatedCreationOptions =
        [this](const gdal::TileMatrixSet::TileMatrix &oTM)
    {
        CPLStringList aosCreationOptions(m_creationOptions);
        if (m_format == "GTiff")
        {
            if (aosCreationOptions.FetchNameValue("TILED") == nullptr &&
                aosCreationOptions.FetchNameValue("BLOCKYSIZE") == nullptr)
            {
                if (oTM.mTileWidth <= 512 && oTM.mTileHeight <= 512)
                {
                    aosCreationOptions.SetNameValue(
                        "BLOCKYSIZE", CPLSPrintf("%d", oTM.mTileHeight));
                }
                else
                {
                    aosCreationOptions.SetNameValue("TILED", "YES");
                }
            }
            if (aosCreationOptions.FetchNameValue("COMPRESS") == nullptr)
                aosCreationOptions.SetNameValue("COMPRESS", "LZW");
        }
        else if (m_format == "COG")
        {
            if (aosCreationOptions.FetchNameValue("OVERVIEW_RESAMPLING") ==
                nullptr)
            {
                aosCreationOptions.SetNameValue("OVERVIEW_RESAMPLING",
                                                m_overviewResampling.c_str());
            }
            if (aosCreationOptions.FetchNameValue("BLOCKSIZE") == nullptr &&
                oTM.mTileWidth <= 512 && oTM.mTileWidth == oTM.mTileHeight)
            {
                aosCreationOptions.SetNameValue(
                    "BLOCKSIZE", CPLSPrintf("%d", oTM.mTileWidth));
            }
        }
        return aosCreationOptions;
    };

    VSIMkdir(m_output.c_str(), 0755);
    VSIStatBufL sStat;
    if (VSIStatL(m_output.c_str(), &sStat) != 0 || !VSI_ISDIR(sStat.st_mode))
    {
        ReportError(CE_Failure, CPLE_FileIO,
                    "Cannot create output directory %s", m_output.c_str());
        return false;
    }

    OGRSpatialReference oWGS84;
    oWGS84.importFromEPSG(4326);
    oWGS84.SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);

    std::unique_ptr<OGRCoordinateTransformation> poCTToWGS84;
    if (!oSRS_TMS.IsEmpty())
    {
        CPLErrorStateBackuper oBackuper(CPLQuietErrorHandler);
        poCTToWGS84.reset(
            OGRCreateCoordinateTransformation(&oSRS_TMS, &oWGS84));
    }

    const bool kmlCompatible = m_kml &&
                               [this, &poTMS, &poCTToWGS84, bInvertAxisTMS]()
    {
        CPLErrorStateBackuper oBackuper(CPLQuietErrorHandler);
        double dfX = poTMS->tileMatrixList()[0].mTopLeftX;
        double dfY = poTMS->tileMatrixList()[0].mTopLeftY;
        if (bInvertAxisTMS)
            std::swap(dfX, dfY);
        return (m_minZoomLevel == m_maxZoomLevel ||
                (poTMS->haveAllLevelsSameTopLeft() &&
                 poTMS->haveAllLevelsSameTileSize() &&
                 poTMS->hasOnlyPowerOfTwoVaryingScales())) &&
               poCTToWGS84 && poCTToWGS84->Transform(1, &dfX, &dfY);
    }();
    const int kmlTileSize =
        m_tileSize > 0 ? m_tileSize : poTMS->tileMatrixList()[0].mTileWidth;
    if (m_kml && !kmlCompatible)
    {
        ReportError(CE_Failure, CPLE_NotSupported,
                    "Tiling scheme not compatible with KML output");
        return false;
    }

    if (m_title.empty())
        m_title = CPLGetFilename(m_inputDataset[0].GetName().c_str());

    if (!m_url.empty())
    {
        if (m_url.back() != '/')
            m_url += '/';
        std::string out_path = m_output;
        if (m_output.back() == '/')
            out_path.pop_back();
        m_url += CPLGetFilename(out_path.c_str());
    }

    CPLWorkerThreadPool oThreadPool;

    bool bThreadPoolInitialized = false;
    const auto InitThreadPool =
        [this, &oThreadPool, &bRet, &bThreadPoolInitialized]()
    {
        if (!bThreadPoolInitialized)
        {
            bThreadPoolInitialized = true;

            if (bRet && m_numThreads > 1)
            {
                CPLDebug("gdal_raster_tile", "Using %d threads", m_numThreads);
                bRet = oThreadPool.Setup(m_numThreads, nullptr, nullptr);
            }
        }

        return bRet;
    };

    // Just for unit test purposes
    const bool bEmitSpuriousCharsOnStdout = CPLTestBool(
        CPLGetConfigOption("GDAL_RASTER_TILE_EMIT_SPURIOUS_CHARS", "NO"));

    const auto IsCompatibleOfSpawnSilent = [bSrcIsFineForFork, this]()
    {
        const char *pszErrorMsg = "";
        {
            CPLErrorStateBackuper oBackuper(CPLQuietErrorHandler);
            if (IsCompatibleOfSpawn(pszErrorMsg))
            {
                m_parallelMethod = "spawn";
                return true;
            }
        }
        (void)bSrcIsFineForFork;
#ifdef FORK_ALLOWED
        if (bSrcIsFineForFork && !cpl::starts_with(m_output, "/vsimem/"))
        {
            if (CPLGetCurrentThreadCount() == 1)
            {
                CPLDebugOnce(
                    "gdal_raster_tile",
                    "'gdal' binary not found. Using instead "
                    "parallel-method=fork. If causing instability issues, set "
                    "parallel-method to 'thread' or 'spawn'");
                m_parallelMethod = "fork";
                return true;
            }
        }
#endif
        return false;
    };

    m_numThreads = std::max(
        1, static_cast<int>(std::min<uint64_t>(
               m_numThreads, nBaseTiles / GetThresholdMinTilesPerJob())));

    std::atomic<bool> bParentAskedForStop = false;
    std::thread threadWaitForParentStop;
    std::unique_ptr<CPLErrorHandlerPusher> poErrorHandlerPusher;
    if (m_spawned)
    {
        // Redirect errors to stdout so the parent listens on a single
        // file descriptor.
        poErrorHandlerPusher =
            std::make_unique<CPLErrorHandlerPusher>(SpawnedErrorHandler);

        threadWaitForParentStop = std::thread(
            [&bParentAskedForStop]()
            {
                char szBuffer[81] = {0};
                while (fgets(szBuffer, 80, stdin))
                {
                    if (strcmp(szBuffer, STOP_MARKER) == 0)
                    {
                        bParentAskedForStop = true;
                        break;
                    }
                    else
                    {
                        CPLError(CE_Failure, CPLE_AppDefined,
                                 "Got unexpected input from parent '%s'",
                                 szBuffer);
                    }
                }
            });
    }
#ifdef FORK_ALLOWED
    else if (m_forked)
    {
        threadWaitForParentStop = std::thread(
            [&bParentAskedForStop]()
            {
                std::string buffer;
                buffer.resize(strlen(STOP_MARKER));
                if (CPLPipeRead(pipeIn, buffer.data(),
                                static_cast<int>(strlen(STOP_MARKER))) &&
                    buffer == STOP_MARKER)
                {
                    bParentAskedForStop = true;
                }
                else
                {
                    CPLError(CE_Failure, CPLE_AppDefined,
                             "Got unexpected input from parent '%s'",
                             buffer.c_str());
                }
            });
    }
#endif

    if (m_ovrZoomLevel >= 0)
    {
        // do not generate base tiles if called as a child process with
        // --ovr-zoom-level
    }
    else if (m_numThreads > 1 && nBaseTiles > 1 &&
             ((m_parallelMethod.empty() &&
               m_numThreads >= GetThresholdMinThreadsForSpawn() &&
               IsCompatibleOfSpawnSilent()) ||
              (m_parallelMethod == "spawn" || m_parallelMethod == "fork")))
    {
        if (!GenerateBaseTilesSpawnMethod(nBaseTilesPerCol, nBaseTilesPerRow,
                                          nMinTileX, nMinTileY, nMaxTileX,
                                          nMaxTileY, nTotalTiles, nBaseTiles,
                                          pfnProgress, pProgressData))
        {
            return false;
        }
        nCurTile = nBaseTiles;
    }
    else
    {
        // Branch for multi-threaded or single-threaded max zoom level tile
        // generation

        PerThreadMaxZoomResourceManager oResourceManager(
            m_poSrcDS, psWO.get(), hTransformArg.get(), oFakeMaxZoomDS,
            dstBuffer.size());

        const CPLStringList aosCreationOptions(
            GetUpdatedCreationOptions(tileMatrix));

        CPLDebug("gdal_raster_tile",
                 "Generating tiles z=%d, y=%d...%d, x=%d...%d", m_maxZoomLevel,
                 nMinTileY, nMaxTileY, nMinTileX, nMaxTileX);

        bRet &= InitThreadPool();

        if (bRet && m_numThreads > 1)
        {
            std::atomic<bool> bFailure = false;
            std::atomic<int> nQueuedJobs = 0;

            double dfTilesYPerJob;
            int nYOuterIterations;
            double dfTilesXPerJob;
            int nXOuterIterations;
            ComputeJobChunkSize(m_numThreads, nBaseTilesPerCol,
                                nBaseTilesPerRow, dfTilesYPerJob,
                                nYOuterIterations, dfTilesXPerJob,
                                nXOuterIterations);

            CPLDebugOnly("gdal_raster_tile",
                         "nYOuterIterations=%d, dfTilesYPerJob=%g, "
                         "nXOuterIterations=%d, dfTilesXPerJob=%g",
                         nYOuterIterations, dfTilesYPerJob, nXOuterIterations,
                         dfTilesXPerJob);

            int nLastYEndIncluded = nMinTileY - 1;
            for (int iYOuterIter = 0; bRet && iYOuterIter < nYOuterIterations &&
                                      nLastYEndIncluded < nMaxTileY;
                 ++iYOuterIter)
            {
                const int iYStart = nLastYEndIncluded + 1;
                const int iYEndIncluded =
                    iYOuterIter + 1 == nYOuterIterations
                        ? nMaxTileY
                        : std::max(
                              iYStart,
                              static_cast<int>(std::floor(
                                  nMinTileY +
                                  (iYOuterIter + 1) * dfTilesYPerJob - 1)));

                nLastYEndIncluded = iYEndIncluded;

                int nLastXEndIncluded = nMinTileX - 1;
                for (int iXOuterIter = 0;
                     bRet && iXOuterIter < nXOuterIterations &&
                     nLastXEndIncluded < nMaxTileX;
                     ++iXOuterIter)
                {
                    const int iXStart = nLastXEndIncluded + 1;
                    const int iXEndIncluded =
                        iXOuterIter + 1 == nXOuterIterations
                            ? nMaxTileX
                            : std::max(
                                  iXStart,
                                  static_cast<int>(std::floor(
                                      nMinTileX +
                                      (iXOuterIter + 1) * dfTilesXPerJob - 1)));

                    nLastXEndIncluded = iXEndIncluded;

                    CPLDebugOnly("gdal_raster_tile",
                                 "Job for y in [%d,%d] and x in [%d,%d]",
                                 iYStart, iYEndIncluded, iXStart,
                                 iXEndIncluded);

                    auto job = [this, &oThreadPool, &oResourceManager,
                                &bFailure, &bParentAskedForStop, &nCurTile,
                                &nQueuedJobs, pszExtension, &aosCreationOptions,
                                &psWO, &tileMatrix, nDstBands, iXStart,
                                iXEndIncluded, iYStart, iYEndIncluded,
                                nMinTileX, nMinTileY, &poColorTable,
                                bUserAskedForAlpha]()
                    {
                        CPLErrorStateBackuper oBackuper(CPLQuietErrorHandler);

                        auto resources = oResourceManager.AcquireResources();
                        if (resources)
                        {
                            std::vector<GByte> tmpBuffer;
                            for (int iY = iYStart;
                                 iY <= iYEndIncluded && !bParentAskedForStop;
                                 ++iY)
                            {
                                for (int iX = iXStart; iX <= iXEndIncluded &&
                                                       !bParentAskedForStop;
                                     ++iX)
                                {
                                    if (!GenerateTile(
                                            resources->poSrcDS.get(),
                                            m_poDstDriver, pszExtension,
                                            aosCreationOptions.List(),
                                            *(resources->poWO.get()),
                                            *(resources->poFakeMaxZoomDS
                                                  ->GetSpatialRef()),
                                            psWO->eWorkingDataType, tileMatrix,
                                            m_output, nDstBands,
                                            psWO->padfDstNoDataReal
                                                ? &(psWO->padfDstNoDataReal[0])
                                                : nullptr,
                                            m_maxZoomLevel, iX, iY,
                                            m_convention, nMinTileX, nMinTileY,
                                            m_skipBlank, bUserAskedForAlpha,
                                            m_auxXML, m_resume, m_metadata,
                                            poColorTable.get(),
                                            resources->dstBuffer, tmpBuffer))
                                    {
                                        oResourceManager.SetError();
                                        bFailure = true;
                                        --nQueuedJobs;
                                        return;
                                    }
                                    ++nCurTile;
                                    oThreadPool.WakeUpWaitEvent();
                                }
                            }
                            oResourceManager.ReleaseResources(
                                std::move(resources));
                        }
                        else
                        {
                            oResourceManager.SetError();
                            bFailure = true;
                        }

                        --nQueuedJobs;
                    };

                    ++nQueuedJobs;
                    oThreadPool.SubmitJob(std::move(job));
                }
            }

            // Wait for completion of all jobs
            while (bRet && nQueuedJobs > 0)
            {
                oThreadPool.WaitEvent();
                bRet &= !bFailure;
                if (bRet && pfnProgress &&
                    !pfnProgress(static_cast<double>(nCurTile) /
                                     static_cast<double>(nTotalTiles),
                                 "", pProgressData))
                {
                    bParentAskedForStop = true;
                    bRet = false;
                    CPLError(CE_Failure, CPLE_UserInterrupt,
                             "Process interrupted by user");
                }
            }
            oThreadPool.WaitCompletion();
            bRet &=
                !bFailure && (!pfnProgress ||
                              pfnProgress(static_cast<double>(nCurTile) /
                                              static_cast<double>(nTotalTiles),
                                          "", pProgressData));

            if (!oResourceManager.GetErrorMsg().empty())
            {
                // Re-emit error message from worker thread to main thread
                ReportError(CE_Failure, CPLE_AppDefined, "%s",
                            oResourceManager.GetErrorMsg().c_str());
            }
        }
        else
        {
            // Branch for single-thread max zoom level tile generation
            std::vector<GByte> tmpBuffer;
            for (int iY = nMinTileY;
                 bRet && !bParentAskedForStop && iY <= nMaxTileY; ++iY)
            {
                for (int iX = nMinTileX;
                     bRet && !bParentAskedForStop && iX <= nMaxTileX; ++iX)
                {
                    bRet = GenerateTile(
                        m_poSrcDS, m_poDstDriver, pszExtension,
                        aosCreationOptions.List(), oWO, oSRS_TMS,
                        psWO->eWorkingDataType, tileMatrix, m_output, nDstBands,
                        psWO->padfDstNoDataReal ? &(psWO->padfDstNoDataReal[0])
                                                : nullptr,
                        m_maxZoomLevel, iX, iY, m_convention, nMinTileX,
                        nMinTileY, m_skipBlank, bUserAskedForAlpha, m_auxXML,
                        m_resume, m_metadata, poColorTable.get(), dstBuffer,
                        tmpBuffer);

                    if (m_spawned)
                    {
                        if (bEmitSpuriousCharsOnStdout)
                            fwrite(&PROGRESS_MARKER[0], 1, 1, stdout);
                        fwrite(PROGRESS_MARKER, sizeof(PROGRESS_MARKER), 1,
                               stdout);
                        fflush(stdout);
                    }
#ifdef FORK_ALLOWED
                    else if (m_forked)
                    {
                        CPLPipeWrite(pipeOut, PROGRESS_MARKER,
                                     sizeof(PROGRESS_MARKER));
                    }
#endif
                    else
                    {
                        ++nCurTile;
                        if (bRet && pfnProgress &&
                            !pfnProgress(static_cast<double>(nCurTile) /
                                             static_cast<double>(nTotalTiles),
                                         "", pProgressData))
                        {
                            bRet = false;
                            CPLError(CE_Failure, CPLE_UserInterrupt,
                                     "Process interrupted by user");
                        }
                    }
                }
            }
        }

        if (m_kml && bRet)
        {
            for (int iY = nMinTileY; iY <= nMaxTileY; ++iY)
            {
                for (int iX = nMinTileX; iX <= nMaxTileX; ++iX)
                {
                    const int nFileY =
                        GetFileY(iY, poTMS->tileMatrixList()[m_maxZoomLevel],
                                 m_convention);
                    std::string osFilename = CPLFormFilenameSafe(
                        m_output.c_str(), CPLSPrintf("%d", m_maxZoomLevel),
                        nullptr);
                    osFilename = CPLFormFilenameSafe(
                        osFilename.c_str(), CPLSPrintf("%d", iX), nullptr);
                    osFilename = CPLFormFilenameSafe(
                        osFilename.c_str(),
                        CPLSPrintf("%d.%s", nFileY, pszExtension), nullptr);
                    if (VSIStatL(osFilename.c_str(), &sStat) == 0)
                    {
                        GenerateKML(m_output, m_title, iX, iY, m_maxZoomLevel,
                                    kmlTileSize, pszExtension, m_url,
                                    poTMS.get(), bInvertAxisTMS, m_convention,
                                    poCTToWGS84.get(), {});
                    }
                }
            }
        }
    }

    // Close source dataset if we have opened it (in GDALAlgorithm core code),
    // to free file descriptors, particularly if it is a VRT file.
    std::vector<GDALColorInterp> aeColorInterp;
    for (int i = 1; i <= m_poSrcDS->GetRasterCount(); ++i)
        aeColorInterp.push_back(
            m_poSrcDS->GetRasterBand(i)->GetColorInterpretation());
    if (m_inputDataset[0].HasDatasetBeenOpenedByAlgorithm())
    {
        m_inputDataset[0].Close();
        m_poSrcDS = nullptr;
    }

    /* -------------------------------------------------------------------- */
    /*      Generate tiles at lower zoom levels                             */
    /* -------------------------------------------------------------------- */
    const int iZStart =
        m_ovrZoomLevel >= 0 ? m_ovrZoomLevel : m_maxZoomLevel - 1;
    const int iZEnd = m_ovrZoomLevel >= 0 ? m_ovrZoomLevel : m_minZoomLevel;
    for (int iZ = iZStart; bRet && iZ >= iZEnd; --iZ)
    {
        int nOvrMinTileX = 0;
        int nOvrMinTileY = 0;
        int nOvrMaxTileX = 0;
        int nOvrMaxTileY = 0;

        auto ovrTileMatrix = tileMatrixList[iZ];
        CPL_IGNORE_RET_VAL(
            GetTileIndices(ovrTileMatrix, bInvertAxisTMS, m_tileSize, adfExtent,
                           nOvrMinTileX, nOvrMinTileY, nOvrMaxTileX,
                           nOvrMaxTileY, m_noIntersectionIsOK, bIntersects));

        bRet = bIntersects;

        if (m_minOvrTileX >= 0)
        {
            bRet = true;
            nOvrMinTileX = m_minOvrTileX;
            nOvrMinTileY = m_minOvrTileY;
            nOvrMaxTileX = m_maxOvrTileX;
            nOvrMaxTileY = m_maxOvrTileY;
        }

        if (bRet)
        {
            CPLDebug("gdal_raster_tile",
                     "Generating overview tiles z=%d, y=%d...%d, x=%d...%d", iZ,
                     nOvrMinTileY, nOvrMaxTileY, nOvrMinTileX, nOvrMaxTileX);
        }

        const int nOvrTilesPerCol = nOvrMaxTileY - nOvrMinTileY + 1;
        const int nOvrTilesPerRow = nOvrMaxTileX - nOvrMinTileX + 1;
        const uint64_t nOvrTileCount =
            static_cast<uint64_t>(nOvrTilesPerCol) * nOvrTilesPerRow;

        m_numThreads = std::max(
            1,
            static_cast<int>(std::min<uint64_t>(
                m_numThreads, nOvrTileCount / GetThresholdMinTilesPerJob())));

        if (m_numThreads > 1 && nOvrTileCount > 1 &&
            ((m_parallelMethod.empty() &&
              m_numThreads >= GetThresholdMinThreadsForSpawn() &&
              IsCompatibleOfSpawnSilent()) ||
             (m_parallelMethod == "spawn" || m_parallelMethod == "fork")))
        {
            bRet &= GenerateOverviewTilesSpawnMethod(
                iZ, nOvrMinTileX, nOvrMinTileY, nOvrMaxTileX, nOvrMaxTileY,
                nCurTile, nTotalTiles, pfnProgress, pProgressData);
        }
        else
        {
            bRet &= InitThreadPool();

            auto srcTileMatrix = tileMatrixList[iZ + 1];
            int nSrcMinTileX = 0;
            int nSrcMinTileY = 0;
            int nSrcMaxTileX = 0;
            int nSrcMaxTileY = 0;

            CPL_IGNORE_RET_VAL(GetTileIndices(
                srcTileMatrix, bInvertAxisTMS, m_tileSize, adfExtent,
                nSrcMinTileX, nSrcMinTileY, nSrcMaxTileX, nSrcMaxTileY,
                m_noIntersectionIsOK, bIntersects));

            constexpr double EPSILON = 1e-3;
            int maxCacheTileSizePerThread = static_cast<int>(
                (1 + std::ceil(
                         (ovrTileMatrix.mResY * ovrTileMatrix.mTileHeight) /
                             (srcTileMatrix.mResY * srcTileMatrix.mTileHeight) -
                         EPSILON)) *
                (1 + std::ceil(
                         (ovrTileMatrix.mResX * ovrTileMatrix.mTileWidth) /
                             (srcTileMatrix.mResX * srcTileMatrix.mTileWidth) -
                         EPSILON)));

            CPLDebugOnly("gdal_raster_tile",
                         "Ideal maxCacheTileSizePerThread = %d",
                         maxCacheTileSizePerThread);

#ifndef _WIN32
            const int remainingFileDescriptorCount =
                CPLGetRemainingFileDescriptorCount();
            CPLDebugOnly("gdal_raster_tile",
                         "remainingFileDescriptorCount = %d",
                         remainingFileDescriptorCount);
            if (remainingFileDescriptorCount >= 0 &&
                remainingFileDescriptorCount <
                    (1 + maxCacheTileSizePerThread) * m_numThreads)
            {
                const int newNumThreads =
                    std::max(1, remainingFileDescriptorCount /
                                    (1 + maxCacheTileSizePerThread));
                if (newNumThreads < m_numThreads)
                {
                    CPLError(CE_Warning, CPLE_AppDefined,
                             "Not enough file descriptors available given the "
                             "number of "
                             "threads. Reducing the number of threads %d to %d",
                             m_numThreads, newNumThreads);
                    m_numThreads = newNumThreads;
                }
            }
#endif

            MosaicDataset oSrcDS(
                CPLFormFilenameSafe(m_output.c_str(), CPLSPrintf("%d", iZ + 1),
                                    nullptr),
                pszExtension, m_format, aeColorInterp, srcTileMatrix, oSRS_TMS,
                nSrcMinTileX, nSrcMinTileY, nSrcMaxTileX, nSrcMaxTileY,
                m_convention, nDstBands, psWO->eWorkingDataType,
                psWO->padfDstNoDataReal ? &(psWO->padfDstNoDataReal[0])
                                        : nullptr,
                m_metadata, poColorTable.get(), maxCacheTileSizePerThread);

            const CPLStringList aosCreationOptions(
                GetUpdatedCreationOptions(ovrTileMatrix));

            PerThreadLowerZoomResourceManager oResourceManager(oSrcDS);
            std::atomic<bool> bFailure = false;
            std::atomic<int> nQueuedJobs = 0;

            const bool bUseThreads = m_numThreads > 1 && nOvrTileCount > 1;

            if (bUseThreads)
            {
                double dfTilesYPerJob;
                int nYOuterIterations;
                double dfTilesXPerJob;
                int nXOuterIterations;
                ComputeJobChunkSize(m_numThreads, nOvrTilesPerCol,
                                    nOvrTilesPerRow, dfTilesYPerJob,
                                    nYOuterIterations, dfTilesXPerJob,
                                    nXOuterIterations);

                CPLDebugOnly("gdal_raster_tile",
                             "z=%d, nYOuterIterations=%d, dfTilesYPerJob=%g, "
                             "nXOuterIterations=%d, dfTilesXPerJob=%g",
                             iZ, nYOuterIterations, dfTilesYPerJob,
                             nXOuterIterations, dfTilesXPerJob);

                int nLastYEndIncluded = nOvrMinTileY - 1;
                for (int iYOuterIter = 0;
                     bRet && iYOuterIter < nYOuterIterations &&
                     nLastYEndIncluded < nOvrMaxTileY;
                     ++iYOuterIter)
                {
                    const int iYStart = nLastYEndIncluded + 1;
                    const int iYEndIncluded =
                        iYOuterIter + 1 == nYOuterIterations
                            ? nOvrMaxTileY
                            : std::max(
                                  iYStart,
                                  static_cast<int>(std::floor(
                                      nOvrMinTileY +
                                      (iYOuterIter + 1) * dfTilesYPerJob - 1)));

                    nLastYEndIncluded = iYEndIncluded;

                    int nLastXEndIncluded = nOvrMinTileX - 1;
                    for (int iXOuterIter = 0;
                         bRet && iXOuterIter < nXOuterIterations &&
                         nLastXEndIncluded < nOvrMaxTileX;
                         ++iXOuterIter)
                    {
                        const int iXStart = nLastXEndIncluded + 1;
                        const int iXEndIncluded =
                            iXOuterIter + 1 == nXOuterIterations
                                ? nOvrMaxTileX
                                : std::max(iXStart, static_cast<int>(std::floor(
                                                        nOvrMinTileX +
                                                        (iXOuterIter + 1) *
                                                            dfTilesXPerJob -
                                                        1)));

                        nLastXEndIncluded = iXEndIncluded;

                        CPLDebugOnly(
                            "gdal_raster_tile",
                            "Job for z=%d, y in [%d,%d] and x in [%d,%d]", iZ,
                            iYStart, iYEndIncluded, iXStart, iXEndIncluded);
                        auto job =
                            [this, &oThreadPool, &oResourceManager, &bFailure,
                             &bParentAskedForStop, &nCurTile, &nQueuedJobs,
                             pszExtension, &aosCreationOptions, &aosWarpOptions,
                             &ovrTileMatrix, iZ, iXStart, iXEndIncluded,
                             iYStart, iYEndIncluded, bUserAskedForAlpha]()
                        {
                            CPLErrorStateBackuper oBackuper(
                                CPLQuietErrorHandler);

                            auto resources =
                                oResourceManager.AcquireResources();
                            if (resources)
                            {
                                for (int iY = iYStart; iY <= iYEndIncluded &&
                                                       !bParentAskedForStop;
                                     ++iY)
                                {
                                    for (int iX = iXStart;
                                         iX <= iXEndIncluded &&
                                         !bParentAskedForStop;
                                         ++iX)
                                    {
                                        if (!GenerateOverviewTile(
                                                *(resources->poSrcDS.get()),
                                                m_poDstDriver, m_format,
                                                pszExtension,
                                                aosCreationOptions.List(),
                                                aosWarpOptions.List(),
                                                m_overviewResampling,
                                                ovrTileMatrix, m_output, iZ, iX,
                                                iY, m_convention, m_skipBlank,
                                                bUserAskedForAlpha, m_auxXML,
                                                m_resume))
                                        {
                                            oResourceManager.SetError();
                                            bFailure = true;
                                            --nQueuedJobs;
                                            return;
                                        }

                                        ++nCurTile;
                                        oThreadPool.WakeUpWaitEvent();
                                    }
                                }
                                oResourceManager.ReleaseResources(
                                    std::move(resources));
                            }
                            else
                            {
                                oResourceManager.SetError();
                                bFailure = true;
                            }
                            --nQueuedJobs;
                        };

                        ++nQueuedJobs;
                        oThreadPool.SubmitJob(std::move(job));
                    }
                }

                // Wait for completion of all jobs
                while (bRet && nQueuedJobs > 0)
                {
                    oThreadPool.WaitEvent();
                    bRet &= !bFailure;
                    if (bRet && pfnProgress &&
                        !pfnProgress(static_cast<double>(nCurTile) /
                                         static_cast<double>(nTotalTiles),
                                     "", pProgressData))
                    {
                        bParentAskedForStop = true;
                        bRet = false;
                        CPLError(CE_Failure, CPLE_UserInterrupt,
                                 "Process interrupted by user");
                    }
                }
                oThreadPool.WaitCompletion();
                bRet &= !bFailure &&
                        (!pfnProgress ||
                         pfnProgress(static_cast<double>(nCurTile) /
                                         static_cast<double>(nTotalTiles),
                                     "", pProgressData));

                if (!oResourceManager.GetErrorMsg().empty())
                {
                    // Re-emit error message from worker thread to main thread
                    ReportError(CE_Failure, CPLE_AppDefined, "%s",
                                oResourceManager.GetErrorMsg().c_str());
                }
            }
            else
            {
                // Branch for single-thread overview generation

                for (int iY = nOvrMinTileY;
                     bRet && !bParentAskedForStop && iY <= nOvrMaxTileY; ++iY)
                {
                    for (int iX = nOvrMinTileX;
                         bRet && !bParentAskedForStop && iX <= nOvrMaxTileX;
                         ++iX)
                    {
                        bRet = GenerateOverviewTile(
                            oSrcDS, m_poDstDriver, m_format, pszExtension,
                            aosCreationOptions.List(), aosWarpOptions.List(),
                            m_overviewResampling, ovrTileMatrix, m_output, iZ,
                            iX, iY, m_convention, m_skipBlank,
                            bUserAskedForAlpha, m_auxXML, m_resume);

                        if (m_spawned)
                        {
                            if (bEmitSpuriousCharsOnStdout)
                                fwrite(&PROGRESS_MARKER[0], 1, 1, stdout);
                            fwrite(PROGRESS_MARKER, sizeof(PROGRESS_MARKER), 1,
                                   stdout);
                            fflush(stdout);
                        }
#ifdef FORK_ALLOWED
                        else if (m_forked)
                        {
                            CPLPipeWrite(pipeOut, PROGRESS_MARKER,
                                         sizeof(PROGRESS_MARKER));
                        }
#endif
                        else
                        {
                            ++nCurTile;
                            if (bRet && pfnProgress &&
                                !pfnProgress(
                                    static_cast<double>(nCurTile) /
                                        static_cast<double>(nTotalTiles),
                                    "", pProgressData))
                            {
                                bRet = false;
                                CPLError(CE_Failure, CPLE_UserInterrupt,
                                         "Process interrupted by user");
                            }
                        }
                    }
                }
            }
        }

        if (m_kml && bRet)
        {
            for (int iY = nOvrMinTileY; bRet && iY <= nOvrMaxTileY; ++iY)
            {
                for (int iX = nOvrMinTileX; bRet && iX <= nOvrMaxTileX; ++iX)
                {
                    int nFileY =
                        GetFileY(iY, poTMS->tileMatrixList()[iZ], m_convention);
                    std::string osFilename = CPLFormFilenameSafe(
                        m_output.c_str(), CPLSPrintf("%d", iZ), nullptr);
                    osFilename = CPLFormFilenameSafe(
                        osFilename.c_str(), CPLSPrintf("%d", iX), nullptr);
                    osFilename = CPLFormFilenameSafe(
                        osFilename.c_str(),
                        CPLSPrintf("%d.%s", nFileY, pszExtension), nullptr);
                    if (VSIStatL(osFilename.c_str(), &sStat) == 0)
                    {
                        std::vector<TileCoordinates> children;

                        for (int iChildY = 0; iChildY <= 1; ++iChildY)
                        {
                            for (int iChildX = 0; iChildX <= 1; ++iChildX)
                            {
                                nFileY =
                                    GetFileY(iY * 2 + iChildY,
                                             poTMS->tileMatrixList()[iZ + 1],
                                             m_convention);
                                osFilename = CPLFormFilenameSafe(
                                    m_output.c_str(), CPLSPrintf("%d", iZ + 1),
                                    nullptr);
                                osFilename = CPLFormFilenameSafe(
                                    osFilename.c_str(),
                                    CPLSPrintf("%d", iX * 2 + iChildX),
                                    nullptr);
                                osFilename = CPLFormFilenameSafe(
                                    osFilename.c_str(),
                                    CPLSPrintf("%d.%s", nFileY, pszExtension),
                                    nullptr);
                                if (VSIStatL(osFilename.c_str(), &sStat) == 0)
                                {
                                    TileCoordinates tc;
                                    tc.nTileX = iX * 2 + iChildX;
                                    tc.nTileY = iY * 2 + iChildY;
                                    tc.nTileZ = iZ + 1;
                                    children.push_back(std::move(tc));
                                }
                            }
                        }

                        GenerateKML(m_output, m_title, iX, iY, iZ, kmlTileSize,
                                    pszExtension, m_url, poTMS.get(),
                                    bInvertAxisTMS, m_convention,
                                    poCTToWGS84.get(), children);
                    }
                }
            }
        }
    }

    const auto IsWebViewerEnabled = [this](const char *name)
    {
        return std::find_if(m_webviewers.begin(), m_webviewers.end(),
                            [name](const std::string &s)
                            { return s == "all" || s == name; }) !=
               m_webviewers.end();
    };

    if (m_ovrZoomLevel < 0 && bRet &&
        poTMS->identifier() == "GoogleMapsCompatible" &&
        IsWebViewerEnabled("leaflet"))
    {
        double dfSouthLat = -90;
        double dfWestLon = -180;
        double dfNorthLat = 90;
        double dfEastLon = 180;

        if (poCTToWGS84)
        {
            poCTToWGS84->TransformBounds(
                adfExtent[0], adfExtent[1], adfExtent[2], adfExtent[3],
                &dfWestLon, &dfSouthLat, &dfEastLon, &dfNorthLat, 21);
        }

        GenerateLeaflet(m_output, m_title, dfSouthLat, dfWestLon, dfNorthLat,
                        dfEastLon, m_minZoomLevel, m_maxZoomLevel,
                        tileMatrix.mTileWidth, pszExtension, m_url, m_copyright,
                        m_convention == "xyz");
    }

    if (m_ovrZoomLevel < 0 && bRet && IsWebViewerEnabled("openlayers"))
    {
        GenerateOpenLayers(m_output, m_title, adfExtent[0], adfExtent[1],
                           adfExtent[2], adfExtent[3], m_minZoomLevel,
                           m_maxZoomLevel, tileMatrix.mTileWidth, pszExtension,
                           m_url, m_copyright, *(poTMS.get()), bInvertAxisTMS,
                           oSRS_TMS, m_convention == "xyz");
    }

    if (m_ovrZoomLevel < 0 && bRet && IsWebViewerEnabled("mapml") &&
        poTMS->identifier() != "raster" && m_convention == "xyz")
    {
        GenerateMapML(m_output, m_mapmlTemplate, m_title, nMinTileX, nMinTileY,
                      nMaxTileX, nMaxTileY, m_minZoomLevel, m_maxZoomLevel,
                      pszExtension, m_url, m_copyright, *(poTMS.get()));
    }

    if (m_ovrZoomLevel < 0 && bRet && IsWebViewerEnabled("stac") &&
        m_convention == "xyz")
    {
        OGRCoordinateTransformation *poCT = poCTToWGS84.get();
        std::unique_ptr<OGRCoordinateTransformation> poCTToLongLat;
        if (!poCTToWGS84)
        {
            CPLErrorStateBackuper oBackuper(CPLQuietErrorHandler);
            OGRSpatialReference oLongLat;
            oLongLat.SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);
            oLongLat.CopyGeogCSFrom(&oSRS_TMS);
            poCTToLongLat.reset(
                OGRCreateCoordinateTransformation(&oSRS_TMS, &oLongLat));
            poCT = poCTToLongLat.get();
        }

        double dfSouthLat = -90;
        double dfWestLon = -180;
        double dfNorthLat = 90;
        double dfEastLon = 180;
        if (poCT)
        {
            poCT->TransformBounds(adfExtent[0], adfExtent[1], adfExtent[2],
                                  adfExtent[3], &dfWestLon, &dfSouthLat,
                                  &dfEastLon, &dfNorthLat, 21);
        }

        GenerateSTAC(m_output, m_title, dfWestLon, dfSouthLat, dfEastLon,
                     dfNorthLat, m_metadata, aoBandMetadata, m_minZoomLevel,
                     m_maxZoomLevel, pszExtension, m_format, m_url, m_copyright,
                     oSRS_TMS, *(poTMS.get()), bInvertAxisTMS, m_tileSize,
                     adfExtent, m_inputDataset[0]);
    }

    if (m_ovrZoomLevel < 0 && bRet && m_kml)
    {
        std::vector<TileCoordinates> children;

        auto ovrTileMatrix = tileMatrixList[m_minZoomLevel];
        int nOvrMinTileX = 0;
        int nOvrMinTileY = 0;
        int nOvrMaxTileX = 0;
        int nOvrMaxTileY = 0;
        CPL_IGNORE_RET_VAL(
            GetTileIndices(ovrTileMatrix, bInvertAxisTMS, m_tileSize, adfExtent,
                           nOvrMinTileX, nOvrMinTileY, nOvrMaxTileX,
                           nOvrMaxTileY, m_noIntersectionIsOK, bIntersects));

        for (int iY = nOvrMinTileY; bRet && iY <= nOvrMaxTileY; ++iY)
        {
            for (int iX = nOvrMinTileX; bRet && iX <= nOvrMaxTileX; ++iX)
            {
                int nFileY = GetFileY(
                    iY, poTMS->tileMatrixList()[m_minZoomLevel], m_convention);
                std::string osFilename = CPLFormFilenameSafe(
                    m_output.c_str(), CPLSPrintf("%d", m_minZoomLevel),
                    nullptr);
                osFilename = CPLFormFilenameSafe(osFilename.c_str(),
                                                 CPLSPrintf("%d", iX), nullptr);
                osFilename = CPLFormFilenameSafe(
                    osFilename.c_str(),
                    CPLSPrintf("%d.%s", nFileY, pszExtension), nullptr);
                if (VSIStatL(osFilename.c_str(), &sStat) == 0)
                {
                    TileCoordinates tc;
                    tc.nTileX = iX;
                    tc.nTileY = iY;
                    tc.nTileZ = m_minZoomLevel;
                    children.push_back(std::move(tc));
                }
            }
        }
        GenerateKML(m_output, m_title, -1, -1, -1, kmlTileSize, pszExtension,
                    m_url, poTMS.get(), bInvertAxisTMS, m_convention,
                    poCTToWGS84.get(), children);
    }

    if (!bRet && CPLGetLastErrorType() == CE_None)
    {
        // If that happens, this is a programming error
        ReportError(CE_Failure, CPLE_AppDefined,
                    "Bug: process failed without returning an error message");
    }

    if (m_spawned)
    {
        // Uninstall he custom error handler, before we close stdout.
        poErrorHandlerPusher.reset();

        fwrite(END_MARKER, sizeof(END_MARKER), 1, stdout);
        fflush(stdout);
        fclose(stdout);
        threadWaitForParentStop.join();
    }
#ifdef FORK_ALLOWED
    else if (m_forked)
    {
        CPLPipeWrite(pipeOut, END_MARKER, sizeof(END_MARKER));
        threadWaitForParentStop.join();
    }
#endif

    return bRet;
}

GDALRasterTileAlgorithmStandalone::~GDALRasterTileAlgorithmStandalone() =
    default;

//! @endcond
