// Copyright (c) 2018-2025 ISciences, LLC.
// All rights reserved.
//
// This software is licensed under the Apache License, Version 2.0 (the "License").
// You may not use this file except in compliance with the License. You may
// obtain a copy of the License at http://www.apache.org/licenses/LICENSE-2.0.
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#pragma once

#include <algorithm>
#include <cmath>
#include <limits>
#include <optional>
#include <unordered_map>

//! @cond Doxygen_Suppress

namespace gdal
{
struct RasterStatsOptions
{
    static constexpr float min_coverage_fraction_default =
        std::numeric_limits<float>::min();  // ~1e-38

    float min_coverage_fraction = min_coverage_fraction_default;
    bool calc_variance = false;
    bool store_histogram = false;
    bool store_values = false;
    bool store_weights = false;
    bool store_coverage_fraction = false;
    bool store_xy = false;
    bool include_nodata = false;
    double default_weight = std::numeric_limits<double>::quiet_NaN();

    bool operator==(const RasterStatsOptions &other) const
    {
        return min_coverage_fraction == other.min_coverage_fraction &&
               calc_variance == other.calc_variance &&
               store_histogram == other.store_histogram &&
               store_values == other.store_values &&
               store_weights == other.store_weights &&
               store_coverage_fraction == other.store_coverage_fraction &&
               store_xy == other.store_xy &&
               include_nodata == other.include_nodata &&
               (default_weight == other.default_weight ||
                (std::isnan(default_weight) &&
                 std::isnan(other.default_weight)));
    }

    bool operator!=(const RasterStatsOptions &other) const
    {
        return !(*this == other);
    }
};

class WestVariance
{
    /** \brief Implements an incremental algorithm for weighted standard
     * deviation, variance, and coefficient of variation, as described in
     * formula WV2 of West, D.H.D. (1979) "Updating Mean and Variance
     * Estimates: An Improved Method". Communications of the ACM 22(9).
     */

  private:
    double sum_w = 0;
    double mean = 0;
    double t = 0;

  public:
    /** \brief Update variance estimate with another value
     *
     * @param x value to add
     * @param w weight of `x`
     */
    void process(double x, double w)
    {
        if (w == 0)
        {
            return;
        }

        double mean_old = mean;

        sum_w += w;
        mean += (w / sum_w) * (x - mean_old);
        t += w * (x - mean_old) * (x - mean);
    }

    /** \brief Return the population variance.
     */
    constexpr double variance() const
    {
        return t / sum_w;
    }

    /** \brief Return the population standard deviation
     */
    double stdev() const
    {
        return std::sqrt(variance());
    }

    /** \brief Return the population coefficient of variation
     */
    double coefficent_of_variation() const
    {
        return stdev() / mean;
    }
};

class WeightedQuantiles
{
    // Compute a quantile from weighted values, linearly
    // interpolating between points.
    // Uses a formula from https://stats.stackexchange.com/a/13223
    //
    // Unlike spatstat::weighted.quantile, it matches the default behavior of the
    // base R stats::quantile function when all weights are equal.
    //
    // Unlike Hmisc::wtd.quantile, quantiles always change as the probability is
    // changed, unless there are duplicate values. Hmisc::wtd.quantile also
    // produces nonsense results for non-integer weights; see
    // https://github.com/harrelfe/Hmisc/issues/97

  public:
    CPLErr Process(double x, double w)
    {
        if (w < 0)
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Weighted quantile calculation does not support negative "
                     "weights.");
            return CE_Failure;
        }

        if (!std::isfinite(w))
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Weighted quantile does not support non-finite weights.");
            return CE_Failure;
        }

        m_ready_to_query = false;

        m_elems.emplace_back(x, w);

        return CE_None;
    }

    CPLErr Quantile(double q, double &ret) const
    {
        if (!std::isfinite(q) || q < 0 || q > 1)
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Quantile must be between 0 and 1.");
            return CE_Failure;
        }

        if (!m_ready_to_query)
        {
            Prepare();
        }

        auto sn = m_sum_w * (static_cast<double>(m_elems.size()) - 1);

        elem_t lookup(
            0, 0);  // create a dummy element to use with std::upper_bound
        lookup.s = q * sn;

        // get first element that is greater than the lookup value (q * sn)
        auto right = std::upper_bound(m_elems.cbegin(), m_elems.cend(), lookup,
                                      [](const elem_t &a, const elem_t &b)
                                      { return a.s < b.s; });

        // since the minimum value of "lookup" is zero, and the first value of "s" is zero,
        // we are guaranteed to have at least one element to the left of "right"
        auto left = std::prev(right, 1);

        if (right == m_elems.cend())
        {
            ret = left->x;
        }
        else
        {
            ret = left->x + (q * sn - left->s) * (right->x - left->x) /
                                (right->s - left->s);
        }

        return CE_None;
    }

  private:
    struct elem_t
    {
        elem_t(double _x, double _w) : x(_x), w(_w), cumsum(0), s(0)
        {
        }

        double x;
        double w;
        double cumsum;
        double s;
    };

    void Prepare() const
    {
        std::sort(m_elems.begin(), m_elems.end(),
                  [](const elem_t &a, const elem_t &b) { return a.x < b.x; });

        m_sum_w = 0;
        // relies on map being sorted which it is no
        for (size_t i = 0; i < m_elems.size(); i++)
        {
            m_sum_w += m_elems[i].w;

            if (i == 0)
            {
                m_elems[i].s = 0;
                m_elems[i].cumsum = m_elems[i].w;
            }
            else
            {
                m_elems[i].cumsum = m_elems[i - 1].cumsum + m_elems[i].w;
                m_elems[i].s = i * m_elems[i].w +
                               (static_cast<double>(m_elems.size()) - 1) *
                                   m_elems[i - 1].cumsum;
            }
        }

        m_ready_to_query = true;
    }

    mutable std::vector<elem_t> m_elems{};
    mutable double m_sum_w{0.0};
    mutable bool m_ready_to_query{false};
};

template <typename ValueType> class RasterStats
{
  public:
    /**
     * Compute raster statistics from a Raster representing intersection percentages,
     * a Raster representing data values, and (optionally) a Raster representing weights.
     * and a set of raster values.
     */
    explicit RasterStats(const RasterStatsOptions &options)
        : m_min{std::numeric_limits<ValueType>::max()},
          m_max{std::numeric_limits<ValueType>::lowest()}, m_sum_ciwi{0},
          m_sum_ci{0}, m_sum_xici{0}, m_sum_xiciwi{0}, m_options{options}
    {
    }

    //RasterStats(const RasterStats &other) = default;
    //RasterStats&operator=(RasterStats &other) = default;

    //RasterStats(RasterStats &&other) = default;
    //RasterStats&operator=(RasterStats &&other) = default;

    // All pixels covered 100%
    void process(const ValueType *pValues, const GByte *pabyMask,
                 const double *padfWeights, const GByte *pabyWeightsMask,
                 const double *padfX, const double *padfY, size_t nX, size_t nY)
    {
        for (size_t i = 0; i < nX * nY; i++)
        {
            if (pabyMask[i] == 255)
            {
                if (padfX && padfY)
                {
                    process_location(padfX[i % nX], padfY[i / nX]);
                }
                if (pabyWeightsMask)
                {
                    if (pabyWeightsMask[i] == 255)
                    {
                        process_value(pValues[i], 1.0f, padfWeights[i]);
                    }
                    // FIXME
                }
                else
                {
                    process_value(pValues[i], 1.0, 1.0);
                }
            }
        }
    }

    // Pixels covered 0% or 100%
    void process(const ValueType *pValues, const GByte *pabyMask,
                 const double *padfWeights, const GByte *pabyWeightsMask,
                 const GByte *pabyCov, const double *pdfX, const double *pdfY,
                 size_t nX, size_t nY)
    {
        for (size_t i = 0; i < nX * nY; i++)
        {
            if (pabyMask[i] == 255 && pabyCov[i])
            {
                if (pdfX && pdfY)
                {
                    process_location(pdfX[i % nX], pdfY[i / nX]);
                }
                if (pabyWeightsMask)
                {
                    if (pabyWeightsMask[i] == 255)
                    {
                        process_value(pValues[i], 1.0f, padfWeights[i]);
                    }
                    // FIXME
                }
                else
                {
                    process_value(pValues[i], 1.0, 1.0);
                }
            }
        }
    }

    // Pixels fractionally covered
    void process(const ValueType *pValues, const GByte *pabyMask,
                 const double *padfWeights, const GByte *pabyWeightsMask,
                 const float *pfCov, const double *pdfX, const double *pdfY,
                 size_t nX, size_t nY)
    {
        for (size_t i = 0; i < nX * nY; i++)
        {
            if (pabyMask[i] == 255 &&
                pfCov[i] >= m_options.min_coverage_fraction)
            {
                if (pdfX && pdfY)
                {
                    process_location(pdfX[i % nX], pdfY[i / nX]);
                }
                if (pabyWeightsMask)
                {
                    if (pabyWeightsMask[i] == 255)
                    {
                        process_value(pValues[i], pfCov[i], padfWeights[i]);
                    }
                    // FIXME
                }
                else
                {
                    process_value(pValues[i], pfCov[i], 1.0);
                }
            }
        }
    }

    void process_location(double x, double y)
    {
        if (m_options.store_xy)
        {
            m_cell_x.push_back(x);
            m_cell_y.push_back(y);
        }
    }

    void process_value(const ValueType &val, float coverage, double weight)
    {
        if (m_options.store_coverage_fraction)
        {
            m_cell_cov.push_back(coverage);
        }

        m_sum_ci += static_cast<double>(coverage);
        m_sum_xici += static_cast<double>(val) * static_cast<double>(coverage);

        double ciwi = static_cast<double>(coverage) * weight;
        m_sum_ciwi += ciwi;
        m_sum_xiciwi += static_cast<double>(val) * ciwi;

        if (m_options.calc_variance)
        {
            m_variance.process(static_cast<double>(val),
                               static_cast<double>(coverage));
            m_weighted_variance.process(static_cast<double>(val), ciwi);
        }

        if (val < m_min)
        {
            m_min = val;
            if (m_options.store_xy)
            {
                m_min_xy = {m_cell_x.back(), m_cell_y.back()};
            }
        }

        if (val > m_max)
        {
            m_max = val;
            if (m_options.store_xy)
            {
                m_max_xy = {m_cell_x.back(), m_cell_y.back()};
            }
        }

        if (m_options.store_histogram)
        {
            auto &entry = m_freq[val];
            entry.m_sum_ci += static_cast<double>(coverage);
            entry.m_sum_ciwi += ciwi;
            m_quantiles.reset();
        }

        if (m_options.store_values)
        {
            m_cell_values.push_back(val);
        }

        if (m_options.store_weights)
        {
            m_cell_weights.push_back(weight);
        }
    }

    /**
     * The mean value of cells covered by this polygon, weighted
     * by the percent of the cell that is covered.
     */
    double mean() const
    {
        if (count() > 0)
        {
            return sum() / count();
        }
        else
        {
            return std::numeric_limits<double>::quiet_NaN();
        }
    }

    /**
     * The mean value of cells covered by this polygon, weighted
     * by the percent of the cell that is covered and a secondary
     * weighting raster.
     *
     * If any weights are undefined, will return NAN. If this is undesirable,
     * caller should replace undefined weights with a suitable default
     * before computing statistics.
     */
    double weighted_mean() const
    {
        if (weighted_count() > 0)
        {
            return weighted_sum() / weighted_count();
        }
        else
        {
            return std::numeric_limits<double>::quiet_NaN();
        }
    }

    /** The fraction of weighted cells to unweighted cells.
     *  Meaningful only when the values of the weighting
     *  raster are between 0 and 1.
     */
    double weighted_fraction() const
    {
        return weighted_sum() / sum();
    }

    /**
     * The raster value occupying the greatest number of cells
     * or partial cells within the polygon. When multiple values
     * cover the same number of cells, the greatest value will
     * be returned. Weights are not taken into account.
     */
    std::optional<ValueType> mode() const
    {
        auto it = std::max_element(
            m_freq.cbegin(), m_freq.cend(),
            [](const auto &a, const auto &b)
            {
                return a.second.m_sum_ci < b.second.m_sum_ci ||
                       (a.second.m_sum_ci == b.second.m_sum_ci &&
                        a.first < b.first);
            });
        if (it == m_freq.end())
        {
            return std::nullopt;
        }
        return it->first;
    }

    /**
     * The minimum value in any raster cell wholly or partially covered
     * by the polygon. Weights are not taken into account.
     */
    std::optional<ValueType> min() const
    {
        if (m_sum_ci == 0)
        {
            return std::nullopt;
        }
        return m_min;
    }

    /// XY values corresponding to the center of the cell whose value
    /// is returned by min()
    std::optional<std::pair<double, double>> min_xy() const
    {
        if (m_sum_ci == 0)
        {
            return std::nullopt;
        }
        return m_min_xy;
    }

    /**
     * The maximum value in any raster cell wholly or partially covered
     * by the polygon. Weights are not taken into account.
     */
    std::optional<ValueType> max() const
    {
        if (m_sum_ci == 0)
        {
            return std::nullopt;
        }
        return m_max;
    }

    /// XY values corresponding to the center of the cell whose value
    /// is returned by max()
    std::optional<std::pair<double, double>> max_xy() const
    {
        if (m_sum_ci == 0)
        {
            return std::nullopt;
        }
        return m_max_xy;
    }

    /**
     * The given quantile (0-1) of raster cell values. Coverage fractions
     * are taken into account but weights are not.
     */
    std::optional<ValueType> quantile(double q) const
    {
        if (m_sum_ci == 0)
        {
            return std::nullopt;
        }

        // The weighted quantile computation is not processed incrementally.
        // Create it on demand and retain it in case we want multiple quantiles.
        if (!m_quantiles)
        {
            m_quantiles = std::make_unique<WeightedQuantiles>();

            for (const auto &entry : m_freq)
            {
                m_quantiles->Process(static_cast<double>(entry.first),
                                     entry.second.m_sum_ci);
            }
        }

        double ret;
        if (m_quantiles->Quantile(q, ret) != CE_None)
        {
            return std::nullopt;
        }

        return ret;
    }

    /**
     * The sum of raster cells covered by the polygon, with each raster
     * value weighted by its coverage fraction.
     */
    double sum() const
    {
        return m_sum_xici;
    }

    /**
     * The sum of raster cells covered by the polygon, with each raster
     * value weighted by its coverage fraction and weighting raster value.
     *
     * If any weights are undefined, will return NAN. If this is undesirable,
     * caller should replace undefined weights with a suitable default
     * before computing statistics.
     */
    double weighted_sum() const
    {
        return m_sum_xiciwi;
    }

    /**
     * The number of raster cells with any defined value
     * covered by the polygon. Weights are not taken
     * into account.
     */
    double count() const
    {
        return m_sum_ci;
    }

    /**
     * The number of raster cells with a specific value
     * covered by the polygon. Weights are not taken
     * into account.
     */
    std::optional<double> count(const ValueType &value) const
    {
        const auto &entry = m_freq.find(value);

        if (entry == m_freq.end())
        {
            return std::nullopt;
        }

        return entry->second.m_sum_ci;
    }

    /**
     * The fraction of defined raster cells covered by the polygon with
     * a value that equals the specified value.
     * Weights are not taken into account.
     */
    std::optional<double> frac(const ValueType &value) const
    {
        auto count_for_value = count(value);

        if (!count_for_value.has_value())
        {
            return count_for_value;
        }

        return count_for_value.value() / count();
    }

    /**
     * The weighted fraction of defined raster cells covered by the polygon with
     * a value that equals the specified value.
     */
    std::optional<double> weighted_frac(const ValueType &value) const
    {
        auto count_for_value = weighted_count(value);

        if (!count_for_value.has_value())
        {
            return count_for_value;
        }

        return count_for_value.value() / weighted_count();
    }

    /**
     * The population variance of raster cells touched
     * by the polygon. Cell coverage fractions are taken
     * into account; values of a weighting raster are not.
     */
    double variance() const
    {
        return m_variance.variance();
    }

    /**
     * The population variance of raster cells touched
     * by the polygon, taking into account cell coverage
     * fractions and values of a weighting raster.
     */
    double weighted_variance() const
    {
        return m_weighted_variance.variance();
    }

    /**
     * The population standard deviation of raster cells
     * touched by the polygon. Cell coverage fractions
     * are taken into account; values of a weighting
     * raster are not.
     */
    double stdev() const
    {
        return m_variance.stdev();
    }

    /**
     * The population standard deviation of raster cells
     * touched by the polygon, taking into account cell
     * coverage fractions and values of a weighting raster.
     */
    double weighted_stdev() const
    {
        return m_weighted_variance.stdev();
    }

    /**
     * The sum of weights for each cell covered by the
     * polygon, with each weight multiplied by the coverage
     * fraction of each cell.
     *
     * If any weights are undefined, will return NAN. If this is undesirable,
     * caller should replace undefined weights with a suitable default
     * before computing statistics.
     */
    double weighted_count() const
    {
        return m_sum_ciwi;
    }

    /**
     * The sum of weights for each cell of a specific value covered by the
     * polygon, with each weight multiplied by the coverage fraction
     * of each cell.
     *
     * If any weights are undefined, will return NAN. If this is undesirable,
     * caller should replace undefined weights with a suitable default
     * before computing statistics.
     */
    std::optional<double> weighted_count(const ValueType &value) const
    {
        const auto &entry = m_freq.find(value);

        if (entry == m_freq.end())
        {
            return std::nullopt;
        }

        return entry->second.m_sum_ciwi;
    }

    /**
     * The raster value occupying the least number of cells
     * or partial cells within the polygon. When multiple values
     * cover the same number of cells, the lowest value will
     * be returned.
     *
     * Cell weights are not taken into account.
     */
    std::optional<ValueType> minority() const
    {
        auto it = std::min_element(
            m_freq.cbegin(), m_freq.cend(),
            [](const auto &a, const auto &b)
            {
                return a.second.m_sum_ci < b.second.m_sum_ci ||
                       (a.second.m_sum_ci == b.second.m_sum_ci &&
                        a.first < b.first);
            });
        if (it == m_freq.end())
        {
            return std::nullopt;
        }
        return it->first;
    }

    /**
     * The number of distinct defined raster values in cells wholly
     * or partially covered by the polygon.
     */
    std::uint64_t variety() const
    {
        return m_freq.size();
    }

    const std::vector<ValueType> &values() const
    {
        return m_cell_values;
    }

    const std::vector<bool> &values_defined() const
    {
        return m_cell_values_defined;
    }

    const std::vector<float> &coverage_fractions() const
    {
        return m_cell_cov;
    }

    const std::vector<double> &weights() const
    {
        return m_cell_weights;
    }

    const std::vector<bool> &weights_defined() const
    {
        return m_cell_weights_defined;
    }

    const std::vector<double> &center_x() const
    {
        return m_cell_x;
    }

    const std::vector<double> &center_y() const
    {
        return m_cell_y;
    }

    const auto &freq() const
    {
        return m_freq;
    }

  private:
    ValueType m_min{};
    ValueType m_max{};
    std::pair<double, double> m_min_xy{
        std::numeric_limits<double>::quiet_NaN(),
        std::numeric_limits<double>::quiet_NaN()};
    std::pair<double, double> m_max_xy{
        std::numeric_limits<double>::quiet_NaN(),
        std::numeric_limits<double>::quiet_NaN()};

    // ci: coverage fraction of pixel i
    // wi: weight of pixel i
    // xi: value of pixel i
    double m_sum_ciwi{0};
    double m_sum_ci{0};
    double m_sum_xici{0};
    double m_sum_xiciwi{0};

    WestVariance m_variance{};
    WestVariance m_weighted_variance{};

    mutable std::unique_ptr<WeightedQuantiles> m_quantiles{nullptr};

    struct ValueFreqEntry
    {
        double m_sum_ci = 0;
        double m_sum_ciwi = 0;
    };

    std::unordered_map<ValueType, ValueFreqEntry> m_freq{};

    std::vector<float> m_cell_cov{};
    std::vector<ValueType> m_cell_values{};
    std::vector<double> m_cell_weights{};
    std::vector<double> m_cell_x{};
    std::vector<double> m_cell_y{};
    std::vector<bool> m_cell_values_defined{};
    std::vector<bool> m_cell_weights_defined{};

    const RasterStatsOptions m_options;
};

template <typename T>
std::ostream &operator<<(std::ostream &os, const RasterStats<T> &stats)
{
    os << "{" << std::endl;
    os << "  \"count\" : " << stats.count() << "," << std::endl;

    os << "  \"min\" : ";
    if (stats.min().has_value())
    {
        os << stats.min().value();
    }
    else
    {
        os << "null";
    }
    os << "," << std::endl;

    os << "  \"max\" : ";
    if (stats.max().has_value())
    {
        os << stats.max().value();
    }
    else
    {
        os << "null";
    }
    os << "," << std::endl;

    os << "  \"mean\" : " << stats.mean() << "," << std::endl;
    os << "  \"sum\" : " << stats.sum() << "," << std::endl;
    os << "  \"weighted_mean\" : " << stats.weighted_mean() << "," << std::endl;
    os << "  \"weighted_sum\" : " << stats.weighted_sum();
    if (stats.stores_values())
    {
        os << "," << std::endl;
        os << "  \"mode\" : ";
        if (stats.mode().has_value())
        {
            os << stats.mode().value();
        }
        else
        {
            os << "null";
        }
        os << "," << std::endl;

        os << "  \"minority\" : ";
        if (stats.minority().has_value())
        {
            os << stats.minority().value();
        }
        else
        {
            os << "null";
        }
        os << "," << std::endl;

        os << "  \"variety\" : " << stats.variety() << std::endl;
    }
    else
    {
        os << std::endl;
    }
    os << "}" << std::endl;
    return os;
}

}  // namespace gdal

//! @endcond
