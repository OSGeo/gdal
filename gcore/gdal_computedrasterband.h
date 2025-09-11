/******************************************************************************
 *
 * Name:     gdalc_omputedrasterband.h
 * Project:  GDAL Core
 * Purpose:  Declaration of GDALComputedRasterBand and related gdal:: methods
 * Author:   Even Rouault, <even.rouault@spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2025, Even Rouault, <even.rouault@spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#ifndef GDALCOMPUTEDRASTERBAND_H_INCLUDED
#define GDALCOMPUTEDRASTERBAND_H_INCLUDED

#include "cpl_port.h"
#include "gdal_dataset.h"  // GDALDatasetUniquePtrReleaser
#include "gdal_rasterband.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <memory>
#include <vector>

/* ******************************************************************** */
/*                       GDALComputedRasterBand                         */
/* ******************************************************************** */

/** Class represented the result of an operation on one or two input bands.
 *
 * Such class is instantiated only by operators on GDALRasterBand.
 * The resulting band is lazy evaluated.
 *
 * @since 3.12
 */
class CPL_DLL GDALComputedRasterBand final : public GDALRasterBand
{
  public:
    /** Destructor */
    ~GDALComputedRasterBand() override;

    //! @cond Doxygen_Suppress
    enum class Operation
    {
        OP_ADD,
        OP_SUBTRACT,
        OP_MULTIPLY,
        OP_DIVIDE,
        OP_MIN,
        OP_MAX,
        OP_MEAN,
        OP_GT,
        OP_GE,
        OP_LT,
        OP_LE,
        OP_EQ,
        OP_NE,
        OP_LOGICAL_AND,
        OP_LOGICAL_OR,
        OP_CAST,
        OP_TERNARY,
        OP_ABS,
        OP_SQRT,
        OP_LOG,
        OP_LOG10,
        OP_POW,
    };

    GDALComputedRasterBand(
        Operation op, const std::vector<const GDALRasterBand *> &bands,
        double constant = std::numeric_limits<double>::quiet_NaN());
    GDALComputedRasterBand(Operation op, const GDALRasterBand &band);
    GDALComputedRasterBand(Operation op, double constant,
                           const GDALRasterBand &band);
    GDALComputedRasterBand(Operation op, const GDALRasterBand &band,
                           double constant);
    GDALComputedRasterBand(Operation op, const GDALRasterBand &band,
                           GDALDataType dt);

    // Semi-public for gdal::min(), gdal::max()
    GDALComputedRasterBand(Operation op, const GDALRasterBand &firstBand,
                           const GDALRasterBand &secondBand);

    GDALComputedRasterBand(GDALComputedRasterBand &&) = default;

    //! @endcond

    double GetNoDataValue(int *pbSuccess = nullptr) override;

    /** Convert a GDALComputedRasterBand* to a GDALComputedRasterBandH.
     */
    static inline GDALComputedRasterBandH
    ToHandle(GDALComputedRasterBand *poBand)
    {
        return static_cast<GDALComputedRasterBandH>(poBand);
    }

    /** Convert a GDALComputedRasterBandH to a GDALComputedRasterBand*.
     */
    static inline GDALComputedRasterBand *
    FromHandle(GDALComputedRasterBandH hBand)
    {
        return static_cast<GDALComputedRasterBand *>(hBand);
    }

  protected:
    friend class GDALRasterBand;

    CPLErr IReadBlock(int, int, void *) override;

    CPLErr IRasterIO(GDALRWFlag eRWFlag, int nXOff, int nYOff, int nXSize,
                     int nYSize, void *pData, int nBufXSize, int nBufYSize,
                     GDALDataType eBufType, GSpacing nPixelSpace,
                     GSpacing nLineSpace,
                     GDALRasterIOExtraArg *psExtraArg) override;

  private:
    friend class GDALComputedDataset;
    std::unique_ptr<GDALDataset, GDALDatasetUniquePtrReleaser> m_poOwningDS{};
    bool m_bHasNoData{false};
    double m_dfNoDataValue{0};

    GDALComputedRasterBand(const GDALComputedRasterBand &, bool);
    GDALComputedRasterBand(const GDALComputedRasterBand &) = delete;
    GDALComputedRasterBand &operator=(const GDALComputedRasterBand &) = delete;
    GDALComputedRasterBand &operator=(GDALComputedRasterBand &&) = delete;
};

namespace gdal
{
using std::abs;
GDALComputedRasterBand CPL_DLL abs(const GDALRasterBand &band);

using std::fabs;
GDALComputedRasterBand CPL_DLL fabs(const GDALRasterBand &band);

using std::sqrt;
GDALComputedRasterBand CPL_DLL sqrt(const GDALRasterBand &band);

using std::log;
GDALComputedRasterBand CPL_DLL log(const GDALRasterBand &band);

using std::log10;
GDALComputedRasterBand CPL_DLL log10(const GDALRasterBand &band);

using std::pow;
GDALComputedRasterBand CPL_DLL pow(const GDALRasterBand &band, double constant);
#ifndef DOXYGEN_SKIP
GDALComputedRasterBand CPL_DLL pow(double constant, const GDALRasterBand &band);
GDALComputedRasterBand CPL_DLL pow(const GDALRasterBand &band1,
                                   const GDALRasterBand &band2);
#endif

GDALComputedRasterBand CPL_DLL IfThenElse(const GDALRasterBand &condBand,
                                          const GDALRasterBand &thenBand,
                                          const GDALRasterBand &elseBand);

//! @cond Doxygen_Suppress

GDALComputedRasterBand CPL_DLL IfThenElse(const GDALRasterBand &condBand,
                                          double thenValue,
                                          const GDALRasterBand &elseBand);

GDALComputedRasterBand CPL_DLL IfThenElse(const GDALRasterBand &condBand,
                                          const GDALRasterBand &thenBand,
                                          double elseValue);

GDALComputedRasterBand CPL_DLL IfThenElse(const GDALRasterBand &condBand,
                                          double thenValue, double elseValue);

//! @endcond

using std::max;
using std::min;

GDALComputedRasterBand CPL_DLL min(const GDALRasterBand &first,
                                   const GDALRasterBand &second);

//! @cond Doxygen_Suppress

namespace detail
{

template <typename U, typename Enable> struct minDealFirstArg;

template <typename U>
struct minDealFirstArg<
    U, typename std::enable_if<std::is_arithmetic<U>::value>::type>
{
    inline static void process(std::vector<const GDALRasterBand *> &,
                               double &constant, const U &first)
    {
        if (std::isnan(constant) || static_cast<double>(first) < constant)
            constant = static_cast<double>(first);
    }
};

template <typename U>
struct minDealFirstArg<
    U, typename std::enable_if<!std::is_arithmetic<U>::value>::type>
{
    inline static void process(std::vector<const GDALRasterBand *> &bands,
                               double &, const U &first)
    {
        if (!bands.empty())
            GDALRasterBand::ThrowIfNotSameDimensions(first, *(bands.front()));
        bands.push_back(&first);
    }
};

inline static GDALComputedRasterBand
minInternal(std::vector<const GDALRasterBand *> &bands, double constant)
{
    return GDALComputedRasterBand(GDALComputedRasterBand::Operation::OP_MIN,
                                  bands, constant);
}

template <typename U, typename... V>
GDALComputedRasterBand minInternal(std::vector<const GDALRasterBand *> &bands,
                                   double constant, const U &first, V &&...rest)
{
    minDealFirstArg<U, void>::process(bands, constant, first);
    return minInternal(bands, constant, std::forward<V>(rest)...);
}

}  // namespace detail

template <typename U, typename... V>
inline GDALComputedRasterBand min(const U &first, V &&...rest)
{
    std::vector<const GDALRasterBand *> bands;
    return detail::minInternal(bands, std::numeric_limits<double>::quiet_NaN(),
                               first, std::forward<V>(rest)...);
}

//! @endcond

GDALComputedRasterBand CPL_DLL max(const GDALRasterBand &first,
                                   const GDALRasterBand &second);

//! @cond Doxygen_Suppress

namespace detail
{

template <typename U, typename Enable> struct maxDealFirstArg;

template <typename U>
struct maxDealFirstArg<
    U, typename std::enable_if<std::is_arithmetic<U>::value>::type>
{
    inline static void process(std::vector<const GDALRasterBand *> &,
                               double &constant, const U &first)
    {
        if (std::isnan(constant) || static_cast<double>(first) > constant)
            constant = static_cast<double>(first);
    }
};

template <typename U>
struct maxDealFirstArg<
    U, typename std::enable_if<!std::is_arithmetic<U>::value>::type>
{
    inline static void process(std::vector<const GDALRasterBand *> &bands,
                               double &, const U &first)
    {
        if (!bands.empty())
            GDALRasterBand::ThrowIfNotSameDimensions(first, *(bands.front()));
        bands.push_back(&first);
    }
};

inline static GDALComputedRasterBand
maxInternal(std::vector<const GDALRasterBand *> &bands, double constant)
{
    return GDALComputedRasterBand(GDALComputedRasterBand::Operation::OP_MAX,
                                  bands, constant);
}

template <typename U, typename... V>
GDALComputedRasterBand maxInternal(std::vector<const GDALRasterBand *> &bands,
                                   double constant, const U &first, V &&...rest)
{
    maxDealFirstArg<U, void>::process(bands, constant, first);
    return maxInternal(bands, constant, std::forward<V>(rest)...);
}

}  // namespace detail

template <typename U, typename... V>
inline GDALComputedRasterBand max(const U &first, V &&...rest)
{
    std::vector<const GDALRasterBand *> bands;
    return detail::maxInternal(bands, std::numeric_limits<double>::quiet_NaN(),
                               first, std::forward<V>(rest)...);
}

//! @endcond

GDALComputedRasterBand CPL_DLL mean(const GDALRasterBand &first,
                                    const GDALRasterBand &second);

//! @cond Doxygen_Suppress
inline GDALComputedRasterBand
meanInternal(std::vector<const GDALRasterBand *> &bands)
{
    return GDALComputedRasterBand(GDALComputedRasterBand::Operation::OP_MEAN,
                                  bands);
}

template <typename U, typename... V>
inline GDALComputedRasterBand
meanInternal(std::vector<const GDALRasterBand *> &bands, const U &first,
             V &&...rest)
{
    if (!bands.empty())
        GDALRasterBand::ThrowIfNotSameDimensions(first, *(bands.front()));
    bands.push_back(&first);
    return meanInternal(bands, std::forward<V>(rest)...);
}

template <typename... Args> inline GDALComputedRasterBand mean(Args &&...args)
{
    std::vector<const GDALRasterBand *> bands;
    return meanInternal(bands, std::forward<Args>(args)...);
}

//! @endcond

}  // namespace gdal

#endif
