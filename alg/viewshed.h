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

#include <cstdint>
#include <memory>
#include <string>

#include "cpl_progress.h"
#include "gdal_priv.h"

namespace gdal
{

/**
 * Class to support viewshed raster generation.
 */
class Viewshed
{
  public:
    /**
     * Raster output mode.
     */
    enum class OutputMode
    {
        Normal,  //!< Normal output mode (visibility only)
        DEM,     //!< Output height from DEM
        Ground   //!< Output height from ground
    };

    /**
     * Cell height calculation mode.
     */
    enum class CellMode
    {
        Diagonal,  //!< Diagonal Mode
        Edge,      //!< Edge Mode
        Max,       //!< Maximum value produced by Diagonal and Edge mode
        Min        //!< Minimum value produced by Diagonal and Edge mode
    };

    /**
     * A point.
     */
    struct Point
    {
        double x;  //!< X value
        double y;  //!< Y value
        double z;  //!< Z value
    };

    /**
     * Options for viewshed generation.
     */
    struct Options
    {
        Point observer{0, 0, 0};  //!< x, y, and z of the observer
        uint8_t visibleVal{255};  //!< raster output value for visible pixels.
        uint8_t invisibleVal{
            0};  //!< raster output value for non-visible pixels.
        uint8_t outOfRangeVal{
            0};  //!< raster output value for pixels outside of max distance.
        double nodataVal{-1};  //!< raster output value for pixels with no data
        double targetHeight{0.0};  //!< target height above the DEM surface
        double maxDistance{
            0.0};  //!< maximum distance from observer to compute value
        double curveCoeff{.85714};  //!< coefficient for atmospheric refraction
        OutputMode outputMode{OutputMode::Normal};  //!< Output information.
            //!< Normal, Height from DEM or Height from ground
        std::string outputFormat{};    //!< output raster format
        std::string outputFilename{};  //!< output raster filename
        CPLStringList creationOpts{};  //!< options for output raster creation
        CellMode cellMode{
            CellMode::Edge};  //!< Mode of cell height calculation.
    };

    /**
     * Constructor.
     *
     * @param opts Options to use when calculating viewshed.
    */
    CPL_DLL explicit Viewshed(const Options &opts) : oOpts{opts}, poDstDS{}
    {
    }

    /**
     * Create the viewshed for the provided raster band.
     *
     * @param hBand  Handle to the raster band.
     * @param pfnProgress  Progress reporting callback function.
     * @param pProgressArg  Argument to pass to the progress callback.
    */
    CPL_DLL bool run(GDALRasterBandH hBand, GDALProgressFunc pfnProgress,
                     void *pProgressArg = nullptr);

    /**
     * Fetch a pointer to the created raster band.
     *
     * @return  Unique pointer to the viewshed dataset.
    */
    CPL_DLL std::unique_ptr<GDALDataset> output()
    {
        return std::move(poDstDS);
    }

  private:
    Options oOpts;
    std::unique_ptr<GDALDataset> poDstDS;
};

}  // namespace gdal
