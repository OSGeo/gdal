/******************************************************************************
 *
 * Project:  Viewshed Generation
 * Purpose:  Core algorithm implementation for viewshed generation.
 * Author:   Tamas Szekeres, szekerest@gmail.com
 *
 ******************************************************************************
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 ****************************************************************************/

#pragma once

#include <mutex>

#include "gdal_priv.h"

#include "viewshed.h"

namespace gdal
{

class ViewshedExecutor
{
  public:
    ViewshedExecutor(GDALRasterBand &srcBand, GDALRasterBand &dstBand, int nX,
                     int nY, const Viewshed::Window &oOutExtent,
                     const Viewshed::Window &oCurExtent,
                     const Viewshed::Options &opts);
    bool run();

  private:
    GDALRasterBand &m_srcBand;
    GDALRasterBand &m_dstBand;
    const int m_nX;
    const int m_nY;
    const Viewshed::Window oOutExtent;
    const Viewshed::Window oCurExtent;
    const Viewshed::Options oOpts;
    double m_dfHeightAdjFactor{0};
    double m_dfMaxDistance2;
    double m_dfZObserver{0};
    std::mutex iMutex{};
    std::mutex oMutex{};
    std::array<double, 6> m_adfTransform{0, 1, 0, 0, 0, 1};
    double (*oZcalc)(int, int, double, double, double);

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

}  // namespace gdal