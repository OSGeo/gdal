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
    bool readLine(int nLine, double *data);
    bool writeLine(int nLine, std::vector<double> &vResult);
    bool processLine(int nLine, std::vector<double> &vLastLineVal);
    bool processFirstLine(std::vector<double> &vLastLineVal);
    void processFirstLineLeft(const LineLimits &ll,
                              std::vector<double> &vResult,
                              std::vector<double> &vThisLineVal);
    void processFirstLineRight(const LineLimits &ll,
                               std::vector<double> &vResult,
                               std::vector<double> &vThisLineVal);
    void processFirstLineTopOrBottom(const LineLimits &ll,
                                     std::vector<double> &vResult,
                                     std::vector<double> &vThisLineVal);
    void processLineLeft(int nYOffset, LineLimits &ll,
                         std::vector<double> &vResult,
                         std::vector<double> &vThisLineVal,
                         std::vector<double> &vLastLineVal);
    void processLineRight(int nYOffset, LineLimits &ll,
                          std::vector<double> &vResult,
                          std::vector<double> &vThisLineVal,
                          std::vector<double> &vLastLineVal);
    LineLimits adjustHeight(int iLine, std::vector<double> &thisLineVal,
                            const std::vector<double> &vResult,
                            std::vector<double> &vPitchMaskVal);
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
