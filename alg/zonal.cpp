/******************************************************************************
*
 * Project:  GDAL
 * Purpose:  GDALZonalStats implementation
 * Author:   Dan Baston
 *
 ******************************************************************************
 * Copyright (c) 2025, ISciences LLC
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include "cpl_string.h"
#include "gdal_priv.h"
#include "gdal_alg.h"
#include "gdal_utils.h"
#include "ogrsf_frmts.h"
#include "raster_stats.h"

#include "../frmts/mem/memdataset.h"
#include "../frmts/vrt/vrtdataset.h"

#include "ogr_geos.h"

#include <algorithm>
#include <array>
#include <cstring>
#include <variant>
#include <vector>

#if GEOS_VERSION_MAJOR > 3 ||                                                  \
    (GEOS_VERSION_MAJOR == 3 && GEOS_VERSION_MINOR >= 14)
#define GEOS_GRID_INTERSECTION_AVAILABLE 1
#endif

struct GDALZonalStatsOptions
{
    CPLErr Init(CSLConstList papszOptions)
    {
        for (const auto &[key, value] : cpl::IterateNameValue(papszOptions))
        {
            if (EQUAL(key, "BANDS"))
            {
                CPLStringList aosBands(CSLTokenizeString2(
                    value, ",", CSLT_STRIPLEADSPACES | CSLT_STRIPENDSPACES));
                for (const char *pszBand : aosBands)
                {
                    int nBand = std::atoi(pszBand);
                    if (nBand == 0)
                    {
                        CPLError(CE_Failure, CPLE_IllegalArg,
                                 "Invalid band: %s", pszBand);
                        return CE_Failure;
                    }
                    bands.push_back(nBand);
                }
            }
            else if (EQUAL(key, "INCLUDE_FIELDS"))
            {
                CPLStringList aosFields(CSLTokenizeString2(
                    value, ",",
                    CSLT_HONOURSTRINGS | CSLT_STRIPLEADSPACES |
                        CSLT_STRIPENDSPACES));
                for (const char *pszField : aosFields)
                {
                    include_fields.push_back(pszField);
                }
            }
            else if (EQUAL(key, "PIXEL_INTERSECTION"))
            {
                if (EQUAL(value, "DEFAULT"))
                {
                    pixels = DEFAULT;
                }
                else if (EQUAL(value, "ALL-TOUCHED") ||
                         EQUAL(value, "ALL_TOUCHED"))
                {
                    pixels = ALL_TOUCHED;
                }
                else if (EQUAL(value, "FRACTIONAL"))
                {
                    pixels = FRACTIONAL;
                }
                else
                {
                    CPLError(CE_Failure, CPLE_IllegalArg,
                             "Unexpected value of PIXEL_INTERSECTION: %s",
                             value);
                    return CE_Failure;
                }
            }
            else if (EQUAL(key, "RASTER_CHUNK_SIZE_BYTES"))
            {
                memory = std::stoul(value);
                if (memory == 0)
                {
                    CPLError(CE_Failure, CPLE_IllegalArg,
                             "Invalid memory size: %s", value);
                    return CE_Failure;
                }
            }
            else if (EQUAL(key, "STATS"))
            {
                CPLStringList aosStats(CSLTokenizeString2(
                    value, ",", CSLT_STRIPLEADSPACES | CSLT_STRIPENDSPACES));
                for (const char *pszStat : aosStats)
                {
                    stats.push_back(pszStat);
                }
            }
            else if (EQUAL(key, "STRATEGY"))
            {
                if (EQUAL(value, "FEATURE_SEQUENTIAL"))
                {
                    strategy = FEATURE_SEQUENTIAL;
                }
                else if (EQUAL(value, "RASTER_SEQUENTIAL"))
                {
                    strategy = RASTER_SEQUENTIAL;
                }
                else
                {
                    CPLError(CE_Failure, CPLE_IllegalArg,
                             "Unexpected value of STRATEGY: %s", value);
                    return CE_Failure;
                }
            }
            else if (EQUAL(key, "WEIGHTS_BAND"))
            {
                weights_band = std::atoi(value);
                if (weights_band == 0)
                {
                    CPLError(CE_Failure, CPLE_IllegalArg,
                             "Invalid weights band: %s", value);
                    return CE_Failure;
                }
            }
            else if (EQUAL(key, "ZONES_BAND"))
            {
                zones_band = std::atoi(value);
                if (zones_band == 0)
                {
                    CPLError(CE_Failure, CPLE_IllegalArg,
                             "Invalid zones band: %s", value);
                    return CE_Failure;
                }
            }
            else if (EQUAL(key, "ZONES_LAYER"))
            {
                zones_layer = value;
            }
            else
            {
                CPLError(CE_Failure, CPLE_IllegalArg,
                         "Unexpected zonal stats option: %s", key);
            }
        }

        return CE_None;
    }

    enum PixelIntersection
    {
        DEFAULT,
        ALL_TOUCHED,
        FRACTIONAL,
    };

    enum Strategy
    {
        FEATURE_SEQUENTIAL,
        RASTER_SEQUENTIAL,
    };

    PixelIntersection pixels{DEFAULT};
    Strategy strategy{FEATURE_SEQUENTIAL};
    std::vector<std::string> stats{};
    std::vector<std::string> include_fields{};
    std::vector<int> bands{};
    std::string zones_layer{};
    std::size_t memory{0};
    int zones_band{};
    int weights_band{};
};

template <typename T = GByte> auto CreateBuffer()
{
    return std::unique_ptr<T, decltype(&CPLFree)>{nullptr, CPLFree};
}

template <typename T> void Realloc(T &buf, size_t size, bool &success)
{
    if (!success)
    {
        return;
    }
    buf.reset(static_cast<typename T::element_type *>(
        VSI_REALLOC_VERBOSE(buf.release(), size)));
    if (buf == nullptr)
    {
        success = false;
    }
}

#ifdef HAVE_GEOS
// This function has nothing to do with GEOS, but if GEOS is not available it
// will not be used, triggering an error under  -Wunused-function.

// Trim a window so that it does not include any pixels outside a specified window.
static void TrimWindow(GDALRasterWindow &window,
                       const GDALRasterWindow &trimWindow)
{
    if (window.nXOff < trimWindow.nXOff)
    {
        window.nXSize -= (trimWindow.nXOff - window.nXOff);
        window.nXOff = trimWindow.nXOff;
    }
    if (window.nYOff < trimWindow.nYOff)
    {
        window.nYSize -= (trimWindow.nYOff - window.nYOff);
        window.nYOff = trimWindow.nYOff;
    }
    auto trimWindowXMax = trimWindow.nXOff + trimWindow.nXSize;
    if (window.nXOff + window.nXSize > trimWindowXMax)
    {
        window.nXSize = trimWindowXMax - window.nXOff;
    }
    auto trimWindowYMax = trimWindow.nYOff + trimWindow.nYSize;
    if (window.nYOff + window.nYSize > trimWindowYMax)
    {
        window.nYSize = trimWindowYMax - window.nYOff;
    }
}
#endif

// Trim a window so that it does not include any pixels outside the raster extent.
static void TrimWindowToRaster(GDALRasterWindow &window,
                               const GDALDataset &rast)
{
    const int nXSize = rast.GetRasterXSize();
    const int nYSize = rast.GetRasterYSize();

    if (window.nXOff < 0)
    {
        window.nXSize += window.nXOff;
        window.nXOff = 0;
    }
    else if (window.nXOff >= nXSize)
    {
        window.nXOff = 0;
        window.nXSize = 0;
    }

    if (window.nYOff < 0)
    {
        window.nYSize += window.nYOff;
        window.nYOff = 0;
    }
    else if (window.nYOff >= nYSize)
    {
        window.nYOff = 0;
        window.nYSize = 0;
    }
    window.nXSize = std::min(window.nXSize, nXSize - window.nXOff);
    window.nYSize = std::min(window.nYSize, nYSize - window.nYOff);
}

static void CalculateCellCenters(const GDALRasterWindow &window,
                                 const GDALGeoTransform &gt, double *padfX,
                                 double *padfY)
{
    double dfJunk;
    double x0 = window.nXOff;
    double y0 = window.nYOff;

    for (int i = 0; i < window.nXSize; i++)
    {
        gt.Apply(x0 + i + 0.5, window.nYOff, padfX + i, &dfJunk);
    }
    for (int i = 0; i < window.nYSize; i++)
    {
        gt.Apply(x0, y0 + i + 0.5, &dfJunk, padfY + i);
    }
}

class GDALZonalStatsImpl
{
  public:
    enum Stat
    {
        CENTER_X,
        CENTER_Y,
        COUNT,
        COVERAGE,
        FRAC,
        MAX,
        MAX_CENTER_X,
        MAX_CENTER_Y,
        MEAN,
        MEDIAN,
        MIN,
        MIN_CENTER_X,
        MIN_CENTER_Y,
        MINORITY,
        MODE,
        STDEV,
        SUM,
        UNIQUE,
        VALUES,
        VARIANCE,
        VARIETY,
        WEIGHTED_FRAC,
        WEIGHTED_MEAN,
        WEIGHTED_SUM,
        WEIGHTED_STDEV,
        WEIGHTED_VARIANCE,
        WEIGHTS,
        INVALID,
    };

    static constexpr bool IsWeighted(Stat eStat)
    {
        return eStat == WEIGHTS || eStat == WEIGHTED_FRAC ||
               eStat == WEIGHTED_MEAN || eStat == WEIGHTED_SUM ||
               eStat == WEIGHTED_VARIANCE || eStat == WEIGHTED_STDEV;
    }

    using BandOrLayer = std::variant<GDALRasterBand *, OGRLayer *>;

    GDALZonalStatsImpl(GDALDataset &src, GDALDataset &dst, GDALDataset *weights,
                       BandOrLayer zones, const GDALZonalStatsOptions &options)
        : m_src(src), m_weights(weights), m_dst(dst), m_zones(zones),
          m_coverageDataType(options.pixels == GDALZonalStatsOptions::FRACTIONAL
                                 ? GDT_Float32
                                 : GDT_Byte),
          m_workingDataType(GDT_Float64), m_maskDataType(GDT_Byte),
          m_options(options),
          m_maxCells(options.memory /
                     GDALGetDataTypeSizeBytes(m_workingDataType)),
          m_pabyCoverageBuf{nullptr, CPLFree}, m_pabyMaskBuf{nullptr, CPLFree},
          m_pabyValuesBuf{nullptr, CPLFree}, m_padfWeightsBuf{nullptr, CPLFree},
          m_pabyWeightsMaskBuf{nullptr, CPLFree}, m_padfX{nullptr, CPLFree},
          m_padfY{nullptr, CPLFree}
    {
#ifdef HAVE_GEOS
        m_geosContext = OGRGeometry::createGEOSContext();
#endif
    }

    ~GDALZonalStatsImpl()
    {
#ifdef HAVE_GEOS
        if (m_geosContext)
        {
            finishGEOS_r(m_geosContext);
        }
#endif
    }

  private:
    bool Init()
    {
#if !(GEOS_GRID_INTERSECTION_AVAILABLE)
        if (m_options.pixels == GDALZonalStatsOptions::FRACTIONAL)
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Fractional pixel coverage calculation requires a GDAL "
                     "build against GEOS >= 3.14");
            return false;
        }
#endif

        if (m_options.bands.empty())
        {
            int nBands = m_src.GetRasterCount();
            m_options.bands.resize(nBands);
            for (int i = 0; i < nBands; i++)
            {
                m_options.bands[i] = i + 1;
            }
        }

        {
            const auto eSrcType = m_src.GetRasterBand(m_options.bands.front())
                                      ->GetRasterDataType();
            if (GDALDataTypeIsConversionLossy(eSrcType, m_workingDataType))
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "GDALRasterZonalStats: Source data type %s is not "
                         "supported",
                         GDALGetDataTypeName(eSrcType));
                return false;
            }
        }

        if (m_weights)
        {
            const auto eWeightsType =
                m_weights->GetRasterBand(m_options.weights_band)
                    ->GetRasterDataType();
            if (GDALDataTypeIsConversionLossy(eWeightsType, GDT_Float64))
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "GDALRasterZonalStats: Weights data type %s is not "
                         "supported",
                         GDALGetDataTypeName(eWeightsType));
                return false;
            }
        }

        for (const auto &stat : m_options.stats)
        {
            const auto eStat = GetStat(stat);
            if (eStat == INVALID)
            {
                CPLError(CE_Failure, CPLE_AppDefined, "Invalid stat: %s",
                         stat.c_str());
                return false;
            }
            if (eStat == COVERAGE)
            {
                m_stats_options.store_coverage_fraction = true;
            }
            if (eStat == VARIETY || eStat == MODE || eStat == MINORITY ||
                eStat == UNIQUE || eStat == FRAC || eStat == WEIGHTED_FRAC ||
                eStat == MEDIAN)
            {
                m_stats_options.store_histogram = true;
            }
            if (eStat == VARIANCE || eStat == STDEV ||
                eStat == WEIGHTED_VARIANCE || eStat == WEIGHTED_STDEV)
            {
                m_stats_options.calc_variance = true;
            }
            if (eStat == CENTER_X || eStat == CENTER_Y ||
                eStat == MIN_CENTER_X || eStat == MIN_CENTER_Y ||
                eStat == MAX_CENTER_X || eStat == MAX_CENTER_Y)
            {
                m_stats_options.store_xy = true;
            }
            if (eStat == VALUES)
            {
                m_stats_options.store_values = true;
            }
            if (eStat == WEIGHTS)
            {
                m_stats_options.store_weights = true;
            }
            if (m_weights == nullptr && IsWeighted(eStat))
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "Stat %s requires weights but none were provided",
                         stat.c_str());
                return false;
            }
        }

        if (m_src.GetGeoTransform(m_srcGT) != CE_None)
        {
            return false;
        }
        if (!m_srcGT.GetInverse(m_srcInvGT))
        {
            return false;
        }

        const OGRSpatialReference *poRastSRS = m_src.GetSpatialRefRasterOnly();
        const OGRSpatialReference *poWeightsSRS =
            m_weights ? m_weights->GetSpatialRefRasterOnly() : nullptr;
        const OGRSpatialReference *poZonesSRS = nullptr;

        if (ZonesAreFeature())
        {
            const OGRLayer *poSrcLayer = std::get<OGRLayer *>(m_zones);
            const OGRFeatureDefn *poSrcDefn = poSrcLayer->GetLayerDefn();
            poZonesSRS = poSrcLayer->GetSpatialRef();

            for (const auto &field : m_options.include_fields)
            {
                if (poSrcDefn->GetFieldIndex(field.c_str()) == -1)
                {
                    CPLError(CE_Failure, CPLE_AppDefined, "Field %s not found.",
                             field.c_str());
                    return false;
                }
            }
        }
        else
        {
            poZonesSRS = std::get<GDALRasterBand *>(m_zones)
                             ->GetDataset()
                             ->GetSpatialRefRasterOnly();

            if (!m_options.include_fields.empty())
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "Cannot include fields from raster zones");
                return false;
            }
        }

        CPLStringList aosOptions;
        aosOptions.AddNameValue("IGNORE_DATA_AXIS_TO_SRS_AXIS_MAPPING", "1");
        std::vector<const OGRSpatialReference *> inputSRS;

        if (poRastSRS && poZonesSRS &&
            !poRastSRS->IsSame(poZonesSRS, aosOptions.List()))
        {
            CPLError(CE_Warning, CPLE_AppDefined,
                     "Inputs and zones do not have the same SRS");
        }

        if (poWeightsSRS && poZonesSRS &&
            !poWeightsSRS->IsSame(poZonesSRS, aosOptions.List()))
        {
            CPLError(CE_Warning, CPLE_AppDefined,
                     "Weights and zones do not have the same SRS");
        }

        if (poWeightsSRS && poRastSRS &&
            !poWeightsSRS->IsSame(poRastSRS, aosOptions.List()))
        {
            CPLError(CE_Warning, CPLE_AppDefined,
                     "Inputs and weights do not have the same SRS");
        }

        return true;
    }

    gdal::RasterStats<double> CreateStats() const
    {
        return gdal::RasterStats<double>{m_stats_options};
    }

    OGRLayer *GetOutputLayer(bool createValueField)
    {
        std::string osLayerName = "stats";

        OGRLayer *poLayer =
            m_dst.CreateLayer(osLayerName.c_str(), nullptr, nullptr);

        if (createValueField)
        {
            const auto poField =
                std::make_unique<OGRFieldDefn>("value", OFTReal);
            poLayer->CreateField(poField.get());
        }

        if (!m_options.include_fields.empty())
        {
            const OGRFeatureDefn *poSrcDefn =
                std::get<OGRLayer *>(m_zones)->GetLayerDefn();

            for (const auto &field : m_options.include_fields)
            {
                const int iField = poSrcDefn->GetFieldIndex(field.c_str());
                // Already checked field names during Init()
                poLayer->CreateField(poSrcDefn->GetFieldDefn(iField));
            }
        }

        for (int iBand : m_options.bands)
        {
            auto &aiStatFields = m_statFields[iBand];
            aiStatFields.fill(-1);

            for (const auto &stat : m_options.stats)
            {
                const Stat eStat = GetStat(stat);

                std::string osFieldName;
                if (m_options.bands.size() > 1)
                {
                    osFieldName = CPLSPrintf("%s_band_%d", stat.c_str(), iBand);
                }
                else
                {
                    osFieldName = stat;
                }

                const auto poField = std::make_unique<OGRFieldDefn>(
                    osFieldName.c_str(), GetFieldType(eStat));
                poLayer->CreateField(poField.get());
                const int iNewField =
                    poLayer->GetLayerDefn()->GetFieldIndex(osFieldName.c_str());
                aiStatFields[eStat] = iNewField;
            }
        }

        return poLayer;
    }

    static Stat GetStat(const std::string &stat)
    {
        if (stat == "center_x")
            return CENTER_X;
        if (stat == "center_y")
            return CENTER_Y;
        if (stat == "count")
            return COUNT;
        if (stat == "coverage")
            return COVERAGE;
        if (stat == "frac")
            return FRAC;
        if (stat == "max")
            return MAX;
        if (stat == "max_center_x")
            return MAX_CENTER_X;
        if (stat == "max_center_y")
            return MAX_CENTER_Y;
        if (stat == "mean")
            return MEAN;
        if (stat == "median")
            return MEDIAN;
        if (stat == "min")
            return MIN;
        if (stat == "minority")
            return MINORITY;
        if (stat == "min_center_x")
            return MIN_CENTER_X;
        if (stat == "min_center_y")
            return MIN_CENTER_Y;
        if (stat == "mode")
            return MODE;
        if (stat == "stdev")
            return STDEV;
        if (stat == "sum")
            return SUM;
        if (stat == "unique")
            return UNIQUE;
        if (stat == "values")
            return VALUES;
        if (stat == "variance")
            return VARIANCE;
        if (stat == "variety")
            return VARIETY;
        if (stat == "weighted_frac")
            return WEIGHTED_FRAC;
        if (stat == "weighted_mean")
            return WEIGHTED_MEAN;
        if (stat == "weighted_stdev")
            return WEIGHTED_STDEV;
        if (stat == "weighted_sum")
            return WEIGHTED_SUM;
        if (stat == "weighted_variance")
            return WEIGHTED_VARIANCE;
        if (stat == "weights")
            return WEIGHTS;
        return INVALID;
    }

    static OGRFieldType GetFieldType(Stat stat)
    {
        switch (stat)
        {
            case CENTER_X:
            case CENTER_Y:
            case COVERAGE:
            case FRAC:
            case UNIQUE:
            case VALUES:
            case WEIGHTS:
                return OFTRealList;
            case VARIETY:
                return OFTInteger;
            default:
                return OFTReal;
        }
    }

    int GetFieldIndex(int iBand, Stat eStat) const
    {
        auto it = m_statFields.find(iBand);
        if (it == m_statFields.end())
        {
            return -1;
        }

        return it->second[eStat];
    }

    OGREnvelope ToEnvelope(const GDALRasterWindow &window) const
    {
        OGREnvelope oSnappedGeomExtent;
        m_srcGT.Apply(window, oSnappedGeomExtent);
        return oSnappedGeomExtent;
    }

    void SetStatFields(OGRFeature &feature, int iBand,
                       const gdal::RasterStats<double> &stats) const
    {
        if (auto iField = GetFieldIndex(iBand, CENTER_X); iField != -1)
        {
            const auto &center_x = stats.center_x();
            feature.SetField(iField, static_cast<int>(center_x.size()),
                             center_x.data());
        }
        if (auto iField = GetFieldIndex(iBand, CENTER_Y); iField != -1)
        {
            const auto &center_y = stats.center_y();
            feature.SetField(iField, static_cast<int>(center_y.size()),
                             center_y.data());
        }
        if (auto iField = GetFieldIndex(iBand, COUNT); iField != -1)
        {
            feature.SetField(iField, stats.count());
        }
        if (auto iField = GetFieldIndex(iBand, COVERAGE); iField != -1)
        {
            const auto &cov = stats.coverage_fractions();
            std::vector<double> doubleCov(cov.begin(), cov.end());
            // TODO: Add float* overload to Feature::SetField to avoid this copy
            feature.SetField(iField, static_cast<int>(doubleCov.size()),
                             doubleCov.data());
        }
        if (auto iField = GetFieldIndex(iBand, FRAC); iField != -1)
        {
            const auto count = stats.count();
            const auto &freq = stats.freq();
            std::vector<double> values;
            values.reserve(freq.size());
            for (const auto &[_, valueCount] : freq)
            {
                values.push_back(valueCount.m_sum_ci / count);
            }
            feature.SetField(iField, static_cast<int>(values.size()),
                             values.data());
        }
        if (auto iField = GetFieldIndex(iBand, MAX); iField != -1)
        {
            const auto &max = stats.max();
            if (max.has_value())
                feature.SetField(iField, max.value());
        }
        if (auto iField = GetFieldIndex(iBand, MAX_CENTER_X); iField != -1)
        {
            const auto &loc = stats.max_xy();
            if (loc.has_value())
                feature.SetField(iField, loc.value().first);
        }
        if (auto iField = GetFieldIndex(iBand, MAX_CENTER_Y); iField != -1)
        {
            const auto &loc = stats.max_xy();
            if (loc.has_value())
                feature.SetField(iField, loc.value().second);
        }
        if (auto iField = GetFieldIndex(iBand, MEAN); iField != -1)
        {
            feature.SetField(iField, stats.mean());
        }
        if (auto iField = GetFieldIndex(iBand, MEDIAN); iField != -1)
        {
            auto median = stats.quantile(0.5);
            if (median.has_value())
                feature.SetField(iField, median.value());
        }
        if (auto iField = GetFieldIndex(iBand, MIN); iField != -1)
        {
            const auto &min = stats.min();
            if (min.has_value())
                feature.SetField(iField, min.value());
        }
        if (auto iField = GetFieldIndex(iBand, MINORITY); iField != -1)
        {
            const auto &minority = stats.minority();
            if (minority.has_value())
                feature.SetField(iField, minority.value());
        }
        if (auto iField = GetFieldIndex(iBand, MIN_CENTER_X); iField != -1)
        {
            const auto &loc = stats.min_xy();
            if (loc.has_value())
                feature.SetField(iField, loc.value().first);
        }
        if (auto iField = GetFieldIndex(iBand, MIN_CENTER_Y); iField != -1)
        {
            const auto &loc = stats.min_xy();
            if (loc.has_value())
                feature.SetField(iField, loc.value().second);
        }
        if (auto iField = GetFieldIndex(iBand, MODE); iField != -1)
        {
            const auto &mode = stats.mode();
            if (mode.has_value())
                feature.SetField(iField, mode.value());
        }
        if (auto iField = GetFieldIndex(iBand, STDEV); iField != -1)
        {
            feature.SetField(iField, stats.stdev());
        }
        if (auto iField = GetFieldIndex(iBand, SUM); iField != -1)
        {
            feature.SetField(iField, stats.sum());
        }
        if (auto iField = GetFieldIndex(iBand, UNIQUE); iField != -1)
        {
            const auto &freq = stats.freq();
            std::vector<double> values;
            values.reserve(freq.size());
            for (const auto &[value, _] : freq)
            {
                values.push_back(value);
            }

            feature.SetField(iField, static_cast<int>(values.size()),
                             values.data());
        }
        if (auto iField = GetFieldIndex(iBand, VALUES); iField != -1)
        {
            const auto &values = stats.values();
            feature.SetField(iField, static_cast<int>(values.size()),
                             values.data());
        }
        if (auto iField = GetFieldIndex(iBand, VARIANCE); iField != -1)
        {
            feature.SetField(iField, stats.variance());
        }
        if (auto iField = GetFieldIndex(iBand, VARIETY); iField != -1)
        {
            feature.SetField(iField, static_cast<GIntBig>(stats.variety()));
        }
        if (auto iField = GetFieldIndex(iBand, WEIGHTED_FRAC); iField != -1)
        {
            const auto count = stats.count();
            const auto &freq = stats.freq();
            std::vector<double> values;
            values.reserve(freq.size());
            for (const auto &[_, valueCount] : freq)
            {
                values.push_back(valueCount.m_sum_ciwi / count);
            }
            feature.SetField(iField, static_cast<int>(values.size()),
                             values.data());
        }
        if (auto iField = GetFieldIndex(iBand, WEIGHTED_MEAN); iField != -1)
        {
            feature.SetField(iField, stats.weighted_mean());
        }
        if (auto iField = GetFieldIndex(iBand, WEIGHTED_STDEV); iField != -1)
        {
            feature.SetField(iField, stats.weighted_stdev());
        }
        if (auto iField = GetFieldIndex(iBand, WEIGHTED_SUM); iField != -1)
        {
            feature.SetField(iField, stats.weighted_sum());
        }
        if (auto iField = GetFieldIndex(iBand, WEIGHTED_VARIANCE); iField != -1)
        {
            feature.SetField(iField, stats.weighted_variance());
        }
        if (auto iField = GetFieldIndex(iBand, WEIGHTED_SUM); iField != -1)
        {
            feature.SetField(iField, stats.weighted_sum());
        }
        if (auto iField = GetFieldIndex(iBand, WEIGHTS); iField != -1)
        {
            const auto &weights = stats.weights();
            feature.SetField(iField, static_cast<int>(weights.size()),
                             weights.data());
        }
    }

  public:
    bool ZonesAreFeature() const
    {
        return std::holds_alternative<OGRLayer *>(m_zones);
    }

    bool Process(GDALProgressFunc pfnProgress, void *pProgressData)
    {
        if (ZonesAreFeature())
        {
            if (m_options.strategy == GDALZonalStatsOptions::RASTER_SEQUENTIAL)
            {
                return ProcessVectorZonesByChunk(pfnProgress, pProgressData);
            }

            return ProcessVectorZonesByFeature(pfnProgress, pProgressData);
        }

        return ProcessRasterZones(pfnProgress, pProgressData);
    }

  private:
    static std::unique_ptr<GDALDataset>
    GetVRT(GDALDataset &src, const GDALDataset &dst, bool &resampled)
    {
        resampled = false;

        GDALGeoTransform srcGT, dstGT;
        if (src.GetGeoTransform(srcGT) != CE_None)
        {
            return nullptr;
        }
        if (dst.GetGeoTransform(dstGT) != CE_None)
        {
            return nullptr;
        }

        CPLStringList aosOptions;
        aosOptions.AddString("-of");
        aosOptions.AddString("VRT");

        aosOptions.AddString("-ot");
        aosOptions.AddString("Float64");

        // Prevent warning message about Computed -srcwin outside source raster extent.
        // We've already tested for this an issued a more understandable message.
        aosOptions.AddString("--no-warn-about-outside-window");

        if (srcGT != dstGT || src.GetRasterXSize() != dst.GetRasterXSize() ||
            src.GetRasterYSize() != dst.GetRasterYSize())
        {
            const double dfColOffset =
                std::fmod(std::abs(srcGT.xorig - dstGT.xorig), dstGT.xscale);
            const double dfRowOffset =
                std::fmod(std::abs(srcGT.yorig - dstGT.yorig), dstGT.yscale);

            OGREnvelope oDstEnv;
            dst.GetExtent(&oDstEnv);

            aosOptions.AddString("-projwin");
            aosOptions.AddString(CPLSPrintf("%.17g", oDstEnv.MinX));
            aosOptions.AddString(CPLSPrintf("%.17g", oDstEnv.MaxY));
            aosOptions.AddString(CPLSPrintf("%.17g", oDstEnv.MaxX));
            aosOptions.AddString(CPLSPrintf("%.17g", oDstEnv.MinY));

            if (srcGT.xscale != dstGT.xscale || srcGT.yscale != dstGT.yscale ||
                std::abs(dfColOffset) > 1e-4 || std::abs(dfRowOffset) > 1e-4)
            {
                resampled = true;
                aosOptions.AddString("-r");
                aosOptions.AddString("average");
            }

            aosOptions.AddString("-tr");
            aosOptions.AddString(CPLSPrintf("%.17g", dstGT.xscale));
            aosOptions.AddString(CPLSPrintf("%.17g", std::abs(dstGT.yscale)));
        }

        std::unique_ptr<GDALDataset> ret;

        GDALTranslateOptions *psOptions =
            GDALTranslateOptionsNew(aosOptions.List(), nullptr);
        ret.reset(GDALDataset::FromHandle(GDALTranslate(
            "", GDALDataset::ToHandle(&src), psOptions, nullptr)));
        GDALTranslateOptionsFree(psOptions);

        return ret;
    }

    void WarnIfZonesNotCovered(const GDALRasterBand *poZonesBand) const
    {
        OGREnvelope oZonesEnv;
        poZonesBand->GetDataset()->GetExtent(&oZonesEnv);

        {
            OGREnvelope oSrcEnv;
            m_src.GetExtent(&oSrcEnv);

            if (!oZonesEnv.Intersects(oSrcEnv))
            {
                // TODO: Make this an error? Or keep it as a warning but short-circuit to avoid reading pixels?
                CPLError(CE_Warning, CPLE_AppDefined,
                         "Source raster does not intersect zones raster");
            }
            else if (!oSrcEnv.Contains(oZonesEnv))
            {
                int bHasNoData;
                m_src.GetRasterBand(m_options.bands.front())
                    ->GetNoDataValue(&bHasNoData);
                if (bHasNoData)
                {
                    CPLError(CE_Warning, CPLE_AppDefined,
                             "Source raster does not fully cover zones raster."
                             "Pixels that do not intersect the values raster "
                             "will be treated as having a NoData value.");
                }
                else
                {
                    CPLError(CE_Warning, CPLE_AppDefined,
                             "Source raster does not fully cover zones raster. "
                             "Pixels that do not intersect the value raster "
                             "will be treated as having value of zero.");
                }
            }
        }

        if (!m_weights)
        {
            return;
        }

        OGREnvelope oWeightsEnv;
        m_weights->GetExtent(&oWeightsEnv);

        if (!oZonesEnv.Intersects(oWeightsEnv))
        {
            // TODO: Make this an error? Or keep it as a warning but short-circuit to avoid reading pixels?
            CPLError(CE_Warning, CPLE_AppDefined,
                     "Weighting raster does not intersect zones raster");
        }
        else if (!oWeightsEnv.Contains(oZonesEnv))
        {
            int bHasNoData;
            m_src.GetRasterBand(m_options.bands.front())
                ->GetNoDataValue(&bHasNoData);
            if (bHasNoData)
            {
                CPLError(CE_Warning, CPLE_AppDefined,
                         "Weighting raster does not fully cover zones raster."
                         "Pixels that do not intersect the weighting raster "
                         "will be treated as having a NoData weight.");
            }
            else
            {
                CPLError(CE_Warning, CPLE_AppDefined,
                         "Weighting raster does not fully cover zones raster. "
                         "Pixels that do not intersect the weighting raster "
                         "will be treated as having a weight of zero.");
            }
        }
    }

    bool ProcessRasterZones(GDALProgressFunc pfnProgress, void *pProgressData)
    {
        if (!Init())
        {
            return false;
        }

        GDALRasterBand *poZonesBand = std::get<GDALRasterBand *>(m_zones);
        WarnIfZonesNotCovered(poZonesBand);

        OGRLayer *poDstLayer = GetOutputLayer(true);

        // Align the src dataset to the zones.
        bool resampled;
        std::unique_ptr<GDALDataset> poAlignedValuesDS =
            GetVRT(m_src, *poZonesBand->GetDataset(), resampled);
        if (resampled)
        {
            CPLError(CE_Warning, CPLE_AppDefined,
                     "Resampled source raster to match zones using average "
                     "resampling.");
        }

        // Align the weighting dataset to the zones.
        std::unique_ptr<GDALDataset> poAlignedWeightsDS;
        GDALRasterBand *poWeightsBand = nullptr;
        if (m_weights)
        {
            poAlignedWeightsDS =
                GetVRT(*m_weights, *poZonesBand->GetDataset(), resampled);
            if (!poAlignedWeightsDS)
            {
                return false;
            }
            if (resampled)
            {
                CPLError(CE_Warning, CPLE_AppDefined,
                         "Resampled weighting raster to match zones using "
                         "average resampling.");
            }

            poWeightsBand =
                poAlignedWeightsDS->GetRasterBand(m_options.weights_band);
        }

        std::map<double, std::vector<gdal::RasterStats<double>>> stats;

        // Load all windows into a vector. This is a lazy way to allow us to
        // report progress.
        std::vector<GDALRasterWindow> windows;
        for (GDALRasterWindow oWindow :
             poAlignedValuesDS->GetRasterBand(1)->IterateWindows(m_maxCells))
        {
            windows.push_back(oWindow);
        }

        auto pabyZonesBuf = CreateBuffer();
        size_t nBufSize = 0;

        for (size_t iWindow = 0; iWindow < windows.size(); iWindow++)
        {
            const GDALRasterWindow &oWindow = windows[iWindow];
            auto nWindowSize = static_cast<size_t>(oWindow.nXSize) *
                               static_cast<size_t>(oWindow.nYSize);

            if (nBufSize < nWindowSize)
            {
                bool bAllocSuccess = true;
                Realloc(m_pabyValuesBuf,
                        nWindowSize *
                            GDALGetDataTypeSizeBytes(m_workingDataType),
                        bAllocSuccess);
                Realloc(pabyZonesBuf,
                        nWindowSize * GDALGetDataTypeSizeBytes(m_zonesDataType),
                        bAllocSuccess);
                Realloc(m_pabyMaskBuf,
                        nWindowSize * GDALGetDataTypeSizeBytes(m_maskDataType),
                        bAllocSuccess);

                if (m_stats_options.store_xy)
                {
                    Realloc(m_padfX,
                            oWindow.nXSize *
                                GDALGetDataTypeSizeBytes(GDT_Float64),
                            bAllocSuccess);
                    Realloc(m_padfY,
                            oWindow.nYSize *
                                GDALGetDataTypeSizeBytes(GDT_Float64),
                            bAllocSuccess);
                }

                if (poWeightsBand)
                {
                    Realloc(m_padfWeightsBuf,
                            nWindowSize * GDALGetDataTypeSizeBytes(GDT_Float64),
                            bAllocSuccess);
                    Realloc(m_pabyWeightsMaskBuf,
                            nWindowSize *
                                GDALGetDataTypeSizeBytes(m_maskDataType),
                            bAllocSuccess);
                }
                if (!bAllocSuccess)
                {
                    return false;
                }

                nBufSize = nWindowSize;
            }

            if (m_padfX && m_padfY)
            {
                CalculateCellCenters(oWindow, m_srcGT, m_padfX.get(),
                                     m_padfY.get());
            }

            if (!ReadWindow(*poZonesBand, oWindow, pabyZonesBuf.get(),
                            m_zonesDataType))
            {
                return false;
            }

            if (poWeightsBand)
            {
                if (!ReadWindow(
                        *poWeightsBand, oWindow,
                        reinterpret_cast<GByte *>(m_padfWeightsBuf.get()),
                        GDT_Float64))
                {
                    return false;
                }
                if (!ReadWindow(*poWeightsBand->GetMaskBand(), oWindow,
                                m_pabyWeightsMaskBuf.get(), GDT_Byte))
                {
                    return false;
                }
            }

            for (size_t i = 0; i < m_options.bands.size(); i++)
            {
                const int iBand = m_options.bands[i];

                GDALRasterBand *poBand =
                    poAlignedValuesDS->GetRasterBand(iBand);

                if (!ReadWindow(*poBand, oWindow, m_pabyValuesBuf.get(),
                                m_workingDataType))
                {
                    return false;
                }

                if (!ReadWindow(*poBand->GetMaskBand(), oWindow,
                                m_pabyMaskBuf.get(), m_maskDataType))
                {
                    return false;
                }

                size_t ipx = 0;
                for (int k = 0; k < oWindow.nYSize; k++)
                {
                    for (int j = 0; j < oWindow.nXSize; j++)
                    {
                        // TODO use inner loop to search for a block of constant pixel values.
                        double zone =
                            reinterpret_cast<double *>(pabyZonesBuf.get())[ipx];

                        auto &aoStats = stats[zone];
                        aoStats.resize(m_options.bands.size(), CreateStats());

                        aoStats[i].process(
                            reinterpret_cast<double *>(m_pabyValuesBuf.get()) +
                                ipx,
                            m_pabyMaskBuf.get() + ipx,
                            m_padfWeightsBuf.get()
                                ? m_padfWeightsBuf.get() + ipx
                                : nullptr,
                            m_pabyWeightsMaskBuf.get()
                                ? m_pabyWeightsMaskBuf.get() + ipx
                                : nullptr,
                            m_padfX ? m_padfX.get() + j : nullptr,
                            m_padfY ? m_padfY.get() + k : nullptr, 1, 1);

                        ipx++;
                    }
                }
            }

            if (pfnProgress != nullptr)
            {
                pfnProgress(static_cast<double>(iWindow + 1) /
                                static_cast<double>(windows.size()),
                            "", pProgressData);
            }
        }

        for (const auto &[dfValue, zoneStats] : stats)
        {
            std::unique_ptr<OGRFeature> poDstFeature(
                OGRFeature::CreateFeature(poDstLayer->GetLayerDefn()));
            poDstFeature->SetField("value", dfValue);
            for (size_t i = 0; i < m_options.bands.size(); i++)
            {
                const auto iBand = m_options.bands[i];
                SetStatFields(*poDstFeature, iBand, zoneStats[i]);
            }
            if (poDstLayer->CreateFeature(poDstFeature.get()) != OGRERR_NONE)
            {
                return false;
            }
        }

        return true;
    }

    static bool ReadWindow(GDALRasterBand &band,
                           const GDALRasterWindow &oWindow, GByte *pabyBuf,
                           GDALDataType dataType)
    {
        return band.RasterIO(GF_Read, oWindow.nXOff, oWindow.nYOff,
                             oWindow.nXSize, oWindow.nYSize, pabyBuf,
                             oWindow.nXSize, oWindow.nYSize, dataType, 0, 0,
                             nullptr) == CE_None;
    }

#ifndef HAVE_GEOS
    bool ProcessVectorZonesByChunk(GDALProgressFunc, void *)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "The GEOS library is required to iterate over blocks of the "
                 "input rasters. Processing can be performed by iterating over "
                 "the input features instead.");
        return false;
#else
    bool ProcessVectorZonesByChunk(GDALProgressFunc pfnProgress,
                                   void *pProgressData)
    {
        if (!Init())
        {
            return false;
        }

        std::unique_ptr<GDALDataset> poAlignedWeightsDS;
        // Align the weighting dataset to the values.
        if (m_weights)
        {
            bool resampled = false;
            poAlignedWeightsDS = GetVRT(*m_weights, m_src, resampled);
            if (!poAlignedWeightsDS)
            {
                return false;
            }
            if (resampled)
            {
                CPLError(CE_Warning, CPLE_AppDefined,
                         "Resampled weights to match source raster using "
                         "average resampling.");
            }
        }

        auto TreeDeleter = [this](GEOSSTRtree *tree)
        { GEOSSTRtree_destroy_r(m_geosContext, tree); };

        std::unique_ptr<GEOSSTRtree, decltype(TreeDeleter)> tree(
            GEOSSTRtree_create_r(m_geosContext, 10), TreeDeleter);

        std::vector<std::unique_ptr<OGRFeature>> features;
        std::map<int, std::vector<gdal::RasterStats<double>>> statsMap;

        // Construct spatial index of all input features, storing the index
        // of the feature.
        {
            OGREnvelope oGeomExtent;
            for (auto &poFeatureIn : *std::get<OGRLayer *>(m_zones))
            {
                features.emplace_back(poFeatureIn.release());

                const OGRGeometry *poGeom = features.back()->GetGeometryRef();

                if (poGeom == nullptr)
                {
                    continue;
                }

                if (poGeom->getDimension() != 2)
                {
                    CPLError(CE_Failure, CPLE_AppDefined,
                             "Non-polygonal geometry encountered.");
                    return false;
                }

                poGeom->getEnvelope(&oGeomExtent);
                GEOSGeometry *poEnv = CreateGEOSEnvelope(oGeomExtent);
                if (poEnv == nullptr)
                {
                    return false;
                }

                GEOSSTRtree_insert_r(
                    m_geosContext, tree.get(), poEnv,
                    reinterpret_cast<void *>(features.size() - 1));
                GEOSGeom_destroy_r(m_geosContext, poEnv);
            }
        }

        for (int iBand : m_options.bands)
        {
            statsMap[iBand].resize(features.size(), CreateStats());
        }

        std::vector<void *> aiHits;
        auto addHit = [](void *hit, void *hits)
        { static_cast<std::vector<void *> *>(hits)->push_back(hit); };
        size_t nBufSize = 0;

        std::vector<GDALRasterWindow> windows;
        for (GDALRasterWindow oWindow :
             m_src.GetRasterBand(m_options.bands.front())
                 ->IterateWindows(m_maxCells))
        {
            windows.push_back(oWindow);
        }

        for (size_t iWindow = 0; iWindow < windows.size(); iWindow++)
        {
            const GDALRasterWindow &oChunkWindow = windows[iWindow];
            const size_t nWindowSize =
                static_cast<size_t>(oChunkWindow.nXSize) *
                static_cast<size_t>(oChunkWindow.nYSize);

            aiHits.clear();

            {
                OGREnvelope oChunkExtent = ToEnvelope(oChunkWindow);
                GEOSGeometry *poEnv = CreateGEOSEnvelope(oChunkExtent);
                if (poEnv == nullptr)
                {
                    return false;
                }

                GEOSSTRtree_query_r(m_geosContext, tree.get(), poEnv, addHit,
                                    &aiHits);
                GEOSGeom_destroy_r(m_geosContext, poEnv);
            }

            if (!aiHits.empty())
            {
                if (nBufSize < nWindowSize)
                {
                    bool bAllocSuccess = true;
                    Realloc(m_pabyValuesBuf,
                            nWindowSize *
                                GDALGetDataTypeSizeBytes(m_workingDataType),
                            bAllocSuccess);
                    Realloc(m_pabyCoverageBuf,
                            nWindowSize *
                                GDALGetDataTypeSizeBytes(m_coverageDataType),
                            bAllocSuccess);
                    Realloc(m_pabyMaskBuf,
                            nWindowSize *
                                GDALGetDataTypeSizeBytes(m_maskDataType),
                            bAllocSuccess);
                    if (m_stats_options.store_xy)
                    {
                        Realloc(m_padfX,
                                oChunkWindow.nXSize *
                                    GDALGetDataTypeSizeBytes(GDT_Float64),
                                bAllocSuccess);
                        Realloc(m_padfY,
                                oChunkWindow.nYSize *
                                    GDALGetDataTypeSizeBytes(GDT_Float64),
                                bAllocSuccess);
                        if (bAllocSuccess)
                        {
                            CalculateCellCenters(oChunkWindow, m_srcGT,
                                                 m_padfX.get(), m_padfY.get());
                        }
                    }
                    if (m_weights != nullptr)
                    {
                        Realloc(m_padfWeightsBuf,
                                nWindowSize *
                                    GDALGetDataTypeSizeBytes(GDT_Float64),
                                bAllocSuccess);
                        Realloc(m_pabyWeightsMaskBuf,
                                nWindowSize *
                                    GDALGetDataTypeSizeBytes(m_maskDataType),
                                bAllocSuccess);
                    }
                    if (!bAllocSuccess)
                    {
                        return false;
                    }
                    nBufSize = nWindowSize;
                }

                if (m_weights != nullptr)
                {
                    GDALRasterBand *poWeightsBand =
                        poAlignedWeightsDS->GetRasterBand(
                            m_options.weights_band);

                    if (!ReadWindow(
                            *poWeightsBand, oChunkWindow,
                            reinterpret_cast<GByte *>(m_padfWeightsBuf.get()),
                            GDT_Float64))
                    {
                        return false;
                    }
                    if (!ReadWindow(*poWeightsBand->GetMaskBand(), oChunkWindow,
                                    m_pabyWeightsMaskBuf.get(), GDT_Byte))
                    {
                        return false;
                    }
                }

                for (int iBand : m_options.bands)
                {

                    GDALRasterBand *poBand = m_src.GetRasterBand(iBand);

                    if (!(ReadWindow(*poBand, oChunkWindow,
                                     m_pabyValuesBuf.get(),
                                     m_workingDataType) &&
                          ReadWindow(*poBand->GetMaskBand(), oChunkWindow,
                                     m_pabyMaskBuf.get(), m_maskDataType)))
                    {
                        return false;
                    }

                    GDALRasterWindow oGeomWindow;
                    OGREnvelope oGeomExtent;
                    for (const void *hit : aiHits)
                    {
                        size_t iHit = reinterpret_cast<size_t>(hit);

                        // Trim the chunk window to the portion that intersects
                        // the geometry being processed.
                        features[iHit]->GetGeometryRef()->getEnvelope(
                            &oGeomExtent);
                        if (!m_srcInvGT.Apply(oGeomExtent, oGeomWindow))
                        {
                            return false;
                        }
                        TrimWindow(oGeomWindow, oChunkWindow);
                        OGREnvelope oTrimmedEnvelope = ToEnvelope(oGeomWindow);

                        if (!CalculateCoverage(
                                features[iHit]->GetGeometryRef(),
                                oTrimmedEnvelope, oGeomWindow.nXSize,
                                oGeomWindow.nYSize, m_pabyCoverageBuf.get()))
                        {
                            return false;
                        }

                        // Because the window used for polygon coverage is not the
                        // same as the window used for raster values, iterate
                        // over partial scanlines on the raster window.
                        const auto nCoverageXOff =
                            oGeomWindow.nXOff - oChunkWindow.nXOff;
                        const auto nCoverageYOff =
                            oGeomWindow.nYOff - oChunkWindow.nYOff;
                        for (int iRow = 0; iRow < oGeomWindow.nYSize; iRow++)
                        {
                            const auto nFirstPx =
                                (nCoverageYOff + iRow) * oChunkWindow.nXSize +
                                nCoverageXOff;
                            UpdateStats(
                                statsMap[iBand][iHit],
                                m_pabyValuesBuf.get() +
                                    nFirstPx * GDALGetDataTypeSizeBytes(
                                                   m_workingDataType),
                                m_pabyMaskBuf.get() +
                                    nFirstPx * GDALGetDataTypeSizeBytes(
                                                   m_maskDataType),
                                m_padfWeightsBuf
                                    ? m_padfWeightsBuf.get() + nFirstPx
                                    : nullptr,
                                m_pabyWeightsMaskBuf
                                    ? m_pabyWeightsMaskBuf.get() +
                                          nFirstPx * GDALGetDataTypeSizeBytes(
                                                         m_maskDataType)
                                    : nullptr,
                                m_pabyCoverageBuf.get() +
                                    iRow * oGeomWindow.nXSize *
                                        GDALGetDataTypeSizeBytes(
                                            m_coverageDataType),
                                m_padfX ? m_padfX.get() + nCoverageXOff
                                        : nullptr,
                                m_padfY ? m_padfY.get() + nCoverageYOff + iRow
                                        : nullptr,
                                oGeomWindow.nXSize, 1);
                        }
                    }
                }
            }

            if (pfnProgress != nullptr)
            {
                pfnProgress(static_cast<double>(iWindow + 1) /
                                static_cast<double>(windows.size()),
                            "", pProgressData);
            }
        }

        OGRLayer *poDstLayer = GetOutputLayer(false);

        for (size_t iFeature = 0; iFeature < features.size(); iFeature++)
        {
            auto poDstFeature =
                std::make_unique<OGRFeature>(poDstLayer->GetLayerDefn());
            poDstFeature->SetFrom(features[iFeature].get());
            for (int iBand : m_options.bands)
            {
                SetStatFields(*poDstFeature, iBand, statsMap[iBand][iFeature]);
            }
            if (poDstLayer->CreateFeature(poDstFeature.get()) != OGRERR_NONE)
            {
                return false;
            }
        }

        return true;
#endif
    }

    bool ProcessVectorZonesByFeature(GDALProgressFunc pfnProgress,
                                     void *pProgressData)
    {
        if (!Init())
        {
            return false;
        }

        OGREnvelope oGeomExtent;
        GDALRasterWindow oWindow;

        std::unique_ptr<GDALDataset> poAlignedWeightsDS;
        // Align the weighting dataset to the values.
        if (m_weights)
        {
            bool resampled = false;
            poAlignedWeightsDS = GetVRT(*m_weights, m_src, resampled);
            if (!poAlignedWeightsDS)
            {
                return false;
            }
            if (resampled)
            {
                CPLError(CE_Warning, CPLE_AppDefined,
                         "Resampled weights to match source raster using "
                         "average resampling.");
            }
        }

        size_t nBufSize = 0;

        OGRLayer *poSrcLayer = std::get<OGRLayer *>(m_zones);
        OGRLayer *poDstLayer = GetOutputLayer(false);
        size_t i = 0;
        auto nFeatures = poSrcLayer->GetFeatureCount();

        for (const auto &poFeature : *poSrcLayer)
        {
            const auto *poGeom = poFeature->GetGeometryRef();

            if (poGeom == nullptr)
            {
                oWindow.nXSize = 0;
                oWindow.nYSize = 0;
            }
            else if (poGeom->getDimension() != 2)
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "Non-polygonal geometry encountered.");
                return false;
            }
            else
            {
                poGeom->getEnvelope(&oGeomExtent);

                if (!m_srcInvGT.Apply(oGeomExtent, oWindow))
                {
                    return false;
                }

                TrimWindowToRaster(oWindow, m_src);
            }

            std::unique_ptr<OGRFeature> poDstFeature(
                OGRFeature::CreateFeature(poDstLayer->GetLayerDefn()));
            poDstFeature->SetFrom(poFeature.get());

            if (oWindow.nXSize == 0 || oWindow.nYSize == 0)
            {
                const gdal::RasterStats<double> empty(CreateStats());
                for (int iBand : m_options.bands)
                {
                    SetStatFields(*poDstFeature, iBand, empty);
                }
            }
            else
            {
                // Calculate how many rows of raster data we can read in at
                // a time while remaining within maxCells.
                const int nRowsPerChunk = std::min(
                    oWindow.nYSize,
                    std::max(1, static_cast<int>(
                                    m_maxCells /
                                    static_cast<size_t>(oWindow.nXSize))));

                const size_t nWindowSize = static_cast<size_t>(oWindow.nXSize) *
                                           static_cast<size_t>(nRowsPerChunk);

                if (nBufSize < nWindowSize)
                {
                    bool bAllocSuccess = true;
                    Realloc(m_pabyValuesBuf,
                            nWindowSize *
                                GDALGetDataTypeSizeBytes(m_workingDataType),
                            bAllocSuccess);
                    Realloc(m_pabyCoverageBuf,
                            nWindowSize *
                                GDALGetDataTypeSizeBytes(m_coverageDataType),
                            bAllocSuccess);
                    Realloc(m_pabyMaskBuf,
                            nWindowSize *
                                GDALGetDataTypeSizeBytes(m_maskDataType),
                            bAllocSuccess);

                    if (m_stats_options.store_xy)
                    {
                        Realloc(m_padfX,
                                oWindow.nXSize *
                                    GDALGetDataTypeSizeBytes(GDT_Float64),
                                bAllocSuccess);
                        Realloc(m_padfY,
                                oWindow.nYSize *
                                    GDALGetDataTypeSizeBytes(GDT_Float64),
                                bAllocSuccess);
                        if (bAllocSuccess)
                        {
                            CalculateCellCenters(oWindow, m_srcGT,
                                                 m_padfX.get(), m_padfY.get());
                        }
                    }

                    if (m_weights != nullptr)
                    {
                        Realloc(m_padfWeightsBuf,
                                nWindowSize *
                                    GDALGetDataTypeSizeBytes(GDT_Float64),
                                bAllocSuccess);
                        Realloc(m_pabyWeightsMaskBuf,
                                nWindowSize *
                                    GDALGetDataTypeSizeBytes(m_maskDataType),
                                bAllocSuccess);
                    }
                    if (!bAllocSuccess)
                    {
                        return false;
                    }

                    nBufSize = nWindowSize;
                }

                std::vector<gdal::RasterStats<double>> aoStats;
                aoStats.resize(m_options.bands.size(), CreateStats());

                for (int nYOff = oWindow.nYOff;
                     nYOff < oWindow.nYOff + oWindow.nYSize;
                     nYOff += nRowsPerChunk)
                {
                    GDALRasterWindow oSubWindow;
                    oSubWindow.nXOff = oWindow.nXOff;
                    oSubWindow.nXSize = oWindow.nXSize;
                    oSubWindow.nYOff = nYOff;
                    oSubWindow.nYSize = std::min(
                        nRowsPerChunk, oWindow.nYOff + oWindow.nYSize - nYOff);

                    const OGREnvelope oSnappedGeomExtent =
                        ToEnvelope(oSubWindow);

                    if (!CalculateCoverage(poGeom, oSnappedGeomExtent,
                                           oSubWindow.nXSize, oSubWindow.nYSize,
                                           m_pabyCoverageBuf.get()))
                    {
                        return false;
                    }

                    if (m_weights != nullptr)
                    {
                        GDALRasterBand *poWeightsBand =
                            poAlignedWeightsDS->GetRasterBand(
                                m_options.weights_band);

                        if (!ReadWindow(*poWeightsBand, oSubWindow,
                                        reinterpret_cast<GByte *>(
                                            m_padfWeightsBuf.get()),
                                        GDT_Float64))
                        {
                            return false;
                        }
                        if (!ReadWindow(*poWeightsBand->GetMaskBand(),
                                        oSubWindow, m_pabyWeightsMaskBuf.get(),
                                        GDT_Byte))
                        {
                            return false;
                        }
                    }

                    for (size_t iBandInd = 0; iBandInd < m_options.bands.size();
                         iBandInd++)
                    {
                        GDALRasterBand *poBand =
                            m_src.GetRasterBand(m_options.bands[iBandInd]);

                        if (!ReadWindow(*poBand, oSubWindow,
                                        m_pabyValuesBuf.get(),
                                        m_workingDataType))
                        {
                            return false;
                        }
                        if (!ReadWindow(*poBand->GetMaskBand(), oSubWindow,
                                        m_pabyMaskBuf.get(), m_maskDataType))
                        {
                            return false;
                        }

                        UpdateStats(aoStats[iBandInd], m_pabyValuesBuf.get(),
                                    m_pabyMaskBuf.get(), m_padfWeightsBuf.get(),
                                    m_pabyWeightsMaskBuf.get(),
                                    m_pabyCoverageBuf.get(), m_padfX.get(),
                                    m_padfY.get(), oSubWindow.nXSize,
                                    oSubWindow.nYSize);
                    }
                }

                for (size_t iBandInd = 0; iBandInd < m_options.bands.size();
                     iBandInd++)
                {
                    SetStatFields(*poDstFeature, m_options.bands[iBandInd],
                                  aoStats[iBandInd]);
                }
            }

            if (poDstLayer->CreateFeature(poDstFeature.get()) != OGRERR_NONE)
            {
                return false;
            }

            if (pfnProgress)
            {
                pfnProgress(static_cast<double>(i + 1) /
                                static_cast<double>(nFeatures),
                            "", pProgressData);
            }
            i++;
        }

        return true;
    }

    void UpdateStats(gdal::RasterStats<double> &stats, const GByte *pabyValues,
                     const GByte *pabyMask, const double *padfWeights,
                     const GByte *pabyWeightsMask, const GByte *pabyCoverage,
                     const double *pdfX, const double *pdfY, size_t nX,
                     size_t nY) const
    {
        if (m_coverageDataType == GDT_Float32)
        {
            stats.process(reinterpret_cast<const double *>(pabyValues),
                          pabyMask, padfWeights, pabyWeightsMask,
                          reinterpret_cast<const float *>(pabyCoverage), pdfX,
                          pdfY, nX, nY);
        }
        else
        {
            stats.process(reinterpret_cast<const double *>(pabyValues),
                          pabyMask, padfWeights, pabyWeightsMask, pabyCoverage,
                          pdfX, pdfY, nX, nY);
        }
    }

    bool CalculateCoverage(const OGRGeometry *poGeom,
                           const OGREnvelope &oSnappedGeomExtent, int nXSize,
                           int nYSize, GByte *pabyCoverageBuf) const
    {
#if GEOS_GRID_INTERSECTION_AVAILABLE
        if (m_options.pixels == GDALZonalStatsOptions::FRACTIONAL)
        {
            std::memset(pabyCoverageBuf, 0,
                        static_cast<size_t>(nXSize) * nYSize *
                            GDALGetDataTypeSizeBytes(GDT_Float32));
            GEOSGeometry *poGeosGeom =
                poGeom->exportToGEOS(m_geosContext, true);
            if (!poGeosGeom)
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "Failed to convert geometry to GEOS.");
                return false;
            }

            if (!GEOSGridIntersectionFractions_r(
                    m_geosContext, poGeosGeom, oSnappedGeomExtent.MinX,
                    oSnappedGeomExtent.MinY, oSnappedGeomExtent.MaxX,
                    oSnappedGeomExtent.MaxY, nXSize, nYSize,
                    reinterpret_cast<float *>(pabyCoverageBuf)))
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "Failed to calculate pixel intersection fractions.");
                return false;
            }
            GEOSGeom_destroy_r(m_geosContext, poGeosGeom);

            return true;
        }
        else
#endif
        {
            GDALGeoTransform oCoverageGT;
            oCoverageGT.xorig = oSnappedGeomExtent.MinX;
            oCoverageGT.xscale = m_srcGT.xscale;
            oCoverageGT.xrot = 0;

            oCoverageGT.yorig = oSnappedGeomExtent.MaxY;
            oCoverageGT.yscale = m_srcGT.yscale;
            oCoverageGT.yrot = 0;

            // Create a memory dataset that wraps the coverage buffer so that
            // we can invoke GDALRasterize
            std::unique_ptr<MEMDataset> poMemDS(MEMDataset::Create(
                "", nXSize, nYSize, 0, m_coverageDataType, nullptr));
            poMemDS->SetGeoTransform(oCoverageGT);
            constexpr double dfBurnValue = 255.0;
            constexpr int nBand = 1;

            MEMRasterBand *poCoverageBand =
                new MEMRasterBand(poMemDS.get(), 1, pabyCoverageBuf,
                                  m_coverageDataType, 0, 0, false, nullptr);
            poMemDS->AddMEMBand(poCoverageBand);
            poCoverageBand->Fill(0);

            CPLStringList aosOptions;
            if (m_options.pixels == GDALZonalStatsOptions::ALL_TOUCHED)
            {
                aosOptions.AddString("ALL_TOUCHED=1");
            }

            OGRGeometryH hGeom =
                OGRGeometry::ToHandle(const_cast<OGRGeometry *>(poGeom));

            auto eErr = GDALRasterizeGeometries(
                GDALDataset::ToHandle(poMemDS.get()), 1, &nBand, 1, &hGeom,
                nullptr, nullptr, &dfBurnValue, aosOptions.List(), nullptr,
                nullptr);

            return eErr == CE_None;
        }
    }

#ifdef HAVE_GEOS
    GEOSGeometry *CreateGEOSEnvelope(const OGREnvelope &oEnv) const
    {
        GEOSCoordSequence *seq = GEOSCoordSeq_create_r(m_geosContext, 2, 2);
        if (seq == nullptr)
        {
            return nullptr;
        }
        GEOSCoordSeq_setXY_r(m_geosContext, seq, 0, oEnv.MinX, oEnv.MinY);
        GEOSCoordSeq_setXY_r(m_geosContext, seq, 1, oEnv.MaxX, oEnv.MaxY);
        return GEOSGeom_createLineString_r(m_geosContext, seq);
    }
#endif

    CPL_DISALLOW_COPY_ASSIGN(GDALZonalStatsImpl)

    GDALDataset &m_src;
    GDALDataset *m_weights;
    GDALDataset &m_dst;
    BandOrLayer m_zones;

    const GDALDataType m_coverageDataType;
    const GDALDataType m_workingDataType;
    const GDALDataType m_maskDataType;
    static constexpr GDALDataType m_zonesDataType = GDT_Float64;

    GDALGeoTransform m_srcGT{};
    GDALGeoTransform m_srcInvGT{};

    GDALZonalStatsOptions m_options{};
    gdal::RasterStatsOptions m_stats_options{};

    size_t m_maxCells{0};

    static constexpr auto NUM_VALID_STATS = Stat::INVALID;
    std::map<int, std::array<int, NUM_VALID_STATS>> m_statFields{};

    std::unique_ptr<GByte, decltype(&CPLFree)> m_pabyCoverageBuf;
    std::unique_ptr<GByte, decltype(&CPLFree)> m_pabyMaskBuf;
    std::unique_ptr<GByte, decltype(&CPLFree)> m_pabyValuesBuf;
    std::unique_ptr<double, decltype(&CPLFree)> m_padfWeightsBuf;
    std::unique_ptr<GByte, decltype(&CPLFree)> m_pabyWeightsMaskBuf;
    std::unique_ptr<double, decltype(&CPLFree)> m_padfX;
    std::unique_ptr<double, decltype(&CPLFree)> m_padfY;

#ifdef HAVE_GEOS
    GEOSContextHandle_t m_geosContext{nullptr};
#endif
};

static CPLErr GDALZonalStats(GDALDataset &srcDataset, GDALDataset *poWeights,
                             GDALDataset &zonesDataset, GDALDataset &dstDataset,
                             const GDALZonalStatsOptions &options,
                             GDALProgressFunc pfnProgress, void *pProgressData)
{
    int nZonesBand = options.zones_band;
    std::string osZonesLayer = options.zones_layer;

    if (nZonesBand < 1 && osZonesLayer.empty())
    {
        if (zonesDataset.GetRasterCount() + zonesDataset.GetLayerCount() > 1)
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Zones dataset has more than one band or layer. Use "
                     "the --zone-band or --zone-layer argument to specify "
                     "which should be used.");
            return CE_Failure;
        }
        if (zonesDataset.GetRasterCount() > 0)
        {
            nZonesBand = 1;
        }
        else
        {
            osZonesLayer = zonesDataset.GetLayer(0)->GetName();
        }
    }

    GDALZonalStatsImpl::BandOrLayer poZones;

    if (nZonesBand > 0)
    {
        GDALRasterBand *poZonesBand = zonesDataset.GetRasterBand(nZonesBand);
        if (poZonesBand == nullptr)
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Specified zones band %d not found", nZonesBand);
            return CE_Failure;
        }
        poZones = poZonesBand;
    }
    else
    {
        OGRLayer *poZonesLayer =
            zonesDataset.GetLayerByName(osZonesLayer.c_str());
        if (poZonesLayer == nullptr)
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Specified zones layer '%s' not found",
                     options.zones_layer.c_str());
            return CE_Failure;
        }
        poZones = poZonesLayer;
    }

    GDALZonalStatsImpl alg(srcDataset, dstDataset, poWeights, poZones, options);
    return alg.Process(pfnProgress, pProgressData) ? CE_None : CE_Failure;
}

/** Compute statistics of raster values within defined zones
 *
 * @param hSrcDS raster dataset containing values to be summarized
 * @param hWeightsDS optional raster dataset containing weights
 * @param hZonesDS raster or vector dataset containing zones across which values will be summarized
 * @param hOutDS dataset to which output layer will be written
 * @param papszOptions list of options
 * @param pfnProgress optional progress reporting callback
 * @param pProgressArg optional data for progress callback
 * @return CE_Failure if an error occurred, CE_None otherwise
 */
CPLErr GDALZonalStats(GDALDatasetH hSrcDS, GDALDatasetH hWeightsDS,
                      GDALDatasetH hZonesDS, GDALDatasetH hOutDS,
                      CSLConstList papszOptions, GDALProgressFunc pfnProgress,
                      void *pProgressArg)
{
    VALIDATE_POINTER1(hSrcDS, __func__, CE_Failure);
    VALIDATE_POINTER1(hZonesDS, __func__, CE_Failure);
    VALIDATE_POINTER1(hOutDS, __func__, CE_Failure);

    GDALZonalStatsOptions sOptions;
    if (papszOptions)
    {
        if (auto eErr = sOptions.Init(papszOptions); eErr != CE_None)
        {
            return eErr;
        }
    }

    return GDALZonalStats(
        *GDALDataset::FromHandle(hSrcDS), GDALDataset::FromHandle(hWeightsDS),
        *GDALDataset::FromHandle(hZonesDS), *GDALDataset::FromHandle(hOutDS),
        sOptions, pfnProgress, pProgressArg);
}