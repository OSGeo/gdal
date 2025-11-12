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

    /// Constructor
    Lines() : cur(), result(), prev(), pitchMask()
    {
    }

    /// Constructor that initializes to line length
    /// \param lineLen  Line length.
    explicit Lines(size_t lineLen)
        : cur(lineLen), result(lineLen), prev(), pitchMask()
    {
    }
};

class Progress;

/// Executes a viewshed computation on a source band, placing the result
/// in the destination band.
class ViewshedExecutor
{
  public:
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
    GDALRasterBand &m_srcBand;
    GDALRasterBand &m_dstBand;
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
    void setOutput(double &dfResult, double &dfCellVal, double dfZ);
    bool readLine(int nLine, std::vector<double> &line);
    bool writeLine(int nLine, std::vector<double> &vResult);
    bool processLine(int nLine, Lines &lines);
    bool processFirstLine(Lines &lines);
    void processFirstLineLeft(const LineLimits &ll, Lines &lines);
    void processFirstLineRight(const LineLimits &ll, Lines &lines);
    void processFirstLineTopOrBottom(const LineLimits &ll, Lines &lines);
    void processLineLeft(int nYOffset, LineLimits &ll, Lines &lines);
    void processLineRight(int nYOffset, LineLimits &ll, Lines &lines);
    LineLimits adjustHeight(int iLine, Lines &lines);
    void maskInitial(std::vector<double> &vResult, int nLine);
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
};

}  // namespace viewshed
}  // namespace gdal
