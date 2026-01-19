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

    CPL_DLL bool run(GDALRasterBandH hBand, GDALRasterBandH hSdBand,
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
    GDALRasterBand *pSdBand = nullptr;

    DatasetPtr execute(int nX, int nY, const std::string &outFilename);
    void setOutput(double &dfResult, double &dfCellVal, double dfZ);
    double calcHeight(double dfZ, double dfZ2);
    bool readLine(int nLine, double *data);
    std::pair<int, int> adjustHeight(int iLine, int nX,
                                     std::vector<double> &thisLineVal);
    bool calcExtents(int nX, int nY, const GDALGeoTransform &invGT);

    Viewshed(const Viewshed &) = delete;
    Viewshed &operator=(const Viewshed &) = delete;
};

double CPL_DLL adjustCurveCoeff(double curveCoeff, GDALDatasetH hSrcDS);

}  // namespace viewshed
}  // namespace gdal

#endif
