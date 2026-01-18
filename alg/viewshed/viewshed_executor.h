/******************************************************************************
 *
 * Project:  Viewshed Generation
 * Purpose:  Core algorithm implementation for viewshed generation.
 * Author:   Tamas Szekeres, szekerest@gmail.com
 *
 * (c) 2024 info@hobu.co
 *
 ******************************************************************************
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#pragma once

#include <array>
#include <limits>
#include <mutex>

#include "gdal_priv.h"
#include "cpl_worker_thread_pool.h"

#include "viewshed_types.h"

namespace gdal
{
namespace viewshed
{

/**
 * Container for lines necessary for processing.
 */
struct Lines
{
    std::vector<double> cur;     //!< Current line being processed
    std::vector<double> result;  //!< Result values for current line
    std::vector<double> prev;    //!< Height values for previous line
    std::vector<double>
        pitchMask;  //!< Height/indicator values for pitch masking.
    std::vector<double> prevTmp;  //!< Saved prev values when in SD mode.
    std::vector<double> sd;       //!< SD mask.

    /// Constructor
    Lines() : cur(), result(), prev(), pitchMask(), prevTmp(), sd()
    {
    }

    /// Constructor that initializes to line length
    /// \param lineLen  Line length.
    explicit Lines(size_t lineLen)
        : cur(lineLen), result(lineLen), prev(), pitchMask(), prevTmp(), sd()
    {
    }
};

/// Dummy raster band.
//! @cond Doxygen_Suppress
class DummyBand : public GDALRasterBand
{
    CPLErr IReadBlock(int, int, void *) override;
};
//! @endcond

class Progress;

/// Executes a viewshed computation on a source band, placing the result
/// in the destination band.
class ViewshedExecutor
{
  public:
    ViewshedExecutor(GDALRasterBand &srcBand, GDALRasterBand &sdBand,
                     GDALRasterBand &dstBand, int nX, int nY,
                     const Window &oOutExtent, const Window &oCurExtent,
                     const Options &opts, Progress &oProgress,
                     bool emitWarningIfNoData);

    ViewshedExecutor(GDALRasterBand &srcBand, GDALRasterBand &dstBand, int nX,
                     int nY, const Window &oOutExtent, const Window &oCurExtent,
                     const Options &opts, Progress &oProgress,
                     bool emitWarningIfNoData);
    bool run();

    /** Return whether an input pixel is at the nodata value. */
    bool hasFoundNoData() const
    {
        return m_hasFoundNoData;
    }

  private:
    CPLWorkerThreadPool m_pool;
    DummyBand m_dummyBand;
    GDALRasterBand &m_srcBand;
    GDALRasterBand &m_sdBand;
    GDALRasterBand &m_dstBand;
    const bool m_hasSdBand;
    double m_noDataValue = 0;
    bool m_hasNoData = false;
    bool m_emitWarningIfNoData = false;
    bool m_hasFoundNoData = false;
    const Window oOutExtent;
    const Window oCurExtent;
    const int m_nX;
    const int m_nY;
    const Options oOpts;
    Progress &oProgress;
    double m_dfHeightAdjFactor{0};
    double m_dfMinDistance2;
    double m_dfMaxDistance2;
    double m_dfZObserver{0};
    std::mutex iMutex{};
    std::mutex oMutex{};
    GDALGeoTransform m_gt{};
    std::array<double, 5> m_testAngle{};
    double m_lowTanPitch{std::numeric_limits<double>::quiet_NaN()};
    double m_highTanPitch{std::numeric_limits<double>::quiet_NaN()};
    double (*oZcalc)(int, int, double, double, double){};

    double calcHeightAdjFactor();

    void setOutputNormal(Lines &lines, int pos, double dfZ);
    void setOutputSd(Lines &lines, int pos, double dfZ);

    bool readLine(int nLine, Lines &lines);
    bool writeLine(int nLine, std::vector<double> &vResult);
    bool processLine(int nLine, Lines &lines);
    bool processFirstLine(Lines &lines);
    void processFirstLineLeft(const LineLimits &ll, Lines &lines, bool sdCalc);
    void processFirstLineRight(const LineLimits &ll, Lines &lines, bool sdCalc);
    void processFirstLineTopOrBottom(const LineLimits &ll, Lines &lines);
    void processLineLeft(int nYOffset, LineLimits &ll, Lines &lines,
                         bool sdCalc);
    void processLineRight(int nYOffset, LineLimits &ll, Lines &lines,
                          bool sdCalc);
    LineLimits adjustHeight(int iLine, Lines &lines);
    bool maskInitial(std::vector<double> &vResult, const LineLimits &ll,
                     int nLine);
    bool maskAngleLeft(std::vector<double> &vResult, int nLine);
    bool maskAngleRight(std::vector<double> &vResult, int nLine);
    void maskLineLeft(std::vector<double> &vResult, const LineLimits &ll,
                      int nLine);
    void maskLineRight(std::vector<double> &vResult, const LineLimits &ll,
                       int nLine);
    void calcPitchMask(double dfZ, double dfDist, double dfResult,
                       double &maskVal);
    void applyPitchMask(std::vector<double> &vResult,
                        const std::vector<double> &vPitchMaskVal);
    void calcTestAngles();

    inline bool sdMode() const
    {
        return m_hasSdBand;
    }
};

}  // namespace viewshed
}  // namespace gdal
