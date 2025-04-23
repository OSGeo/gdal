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
    double m_dfMaxDistance2;
    double m_dfZObserver{0};
    std::mutex iMutex{};
    std::mutex oMutex{};
    std::array<double, 6> m_adfTransform{0, 1, 0, 0, 0, 1};
    double (*oZcalc)(int, int, double, double, double){};

    double calcHeightAdjFactor();
    void setOutput(double &dfResult, double &dfCellVal, double dfZ);
    bool readLine(int nLine, double *data);
    bool writeLine(int nLine, std::vector<double> &vResult);
    bool processLine(int nLine, std::vector<double> &vLastLineVal);
    bool processFirstLine(std::vector<double> &vLastLineVal);
    void processFirstLineLeft(int iStart, int iEnd,
                              std::vector<double> &vResult,
                              std::vector<double> &vThisLineVal);
    void processFirstLineRight(int iStart, int iEnd,
                               std::vector<double> &vResult,
                               std::vector<double> &vThisLineVal);
    void processFirstLineTopOrBottom(int iLeft, int iRight,
                                     std::vector<double> &vResult,
                                     std::vector<double> &vThisLineVal);
    void processLineLeft(int nYOffset, int iStart, int iEnd,
                         std::vector<double> &vResult,
                         std::vector<double> &vThisLineVal,
                         std::vector<double> &vLastLineVal);
    void processLineRight(int nYOffset, int iStart, int iEnd,
                          std::vector<double> &vResult,
                          std::vector<double> &vThisLineVal,
                          std::vector<double> &vLastLineVal);
    std::pair<int, int> adjustHeight(int iLine,
                                     std::vector<double> &thisLineVal);
};

}  // namespace viewshed
}  // namespace gdal
