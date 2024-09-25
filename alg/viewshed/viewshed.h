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

#ifndef VIEWSHED_H_INCLUDED
#define VIEWSHED_H_INCLUDED

#include <algorithm>
#include <array>
#include <cstdint>
#include <functional>
#include <limits>
#include <memory>
#include <mutex>
#include <queue>
#include <string>

#include "cpl_progress.h"
#include "gdal_priv.h"
#include "viewshed_types.h"

namespace gdal
{
namespace viewshed
{

/**
 * Class to support viewshed raster generation.
 */
class Viewshed
{
  public:
    /**
     * Constructor.
     *
     * @param opts Options to use when calculating viewshed.
    */
    CPL_DLL explicit Viewshed(const Options &opts);

    /** Destructor */
    CPL_DLL ~Viewshed();

    CPL_DLL bool run(GDALRasterBandH hBand,
                     GDALProgressFunc pfnProgress = GDALDummyProgress,
                     void *pProgressArg = nullptr);

    /**
     * Fetch a pointer to the created raster band.
     *
     * @return  Unique pointer to the viewshed dataset.
    */
    CPL_DLL DatasetPtr output()
    {
        return std::move(poDstDS);
    }

  private:
    Options oOpts;
    Window oOutExtent{};
    Window oCurExtent{};
    DatasetPtr poDstDS{};
    GDALRasterBand *pSrcBand = nullptr;

    DatasetPtr execute(int nX, int nY, const std::string &outFilename);
    void setOutput(double &dfResult, double &dfCellVal, double dfZ);
    double calcHeight(double dfZ, double dfZ2);
    bool readLine(int nLine, double *data);
    std::pair<int, int> adjustHeight(int iLine, int nX,
                                     std::vector<double> &thisLineVal);
    bool calcExtents(int nX, int nY,
                     const std::array<double, 6> &adfInvTransform);

    Viewshed(const Viewshed &) = delete;
    Viewshed &operator=(const Viewshed &) = delete;
};

}  // namespace viewshed
}  // namespace gdal

#endif
