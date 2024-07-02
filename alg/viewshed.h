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

#include <algorithm>
#include <array>
#include <cstdint>
#include <functional>
#include <limits>
#include <memory>
#include <mutex>
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
     * A window in a raster including pixels in [xStart, xStop) and [yStart, yStop).
     */
    struct Window
    {
        int xStart{};  //!< X start position
        int xStop{};   //!< X end position
        int yStart{};  //!< Y start position
        int yStop{};   //!< Y end position

        /// \brief  Window size in the X direction.
        int xSize() const
        {
            return xStop - xStart;
        }

        /// \brief  Window size in the Y direction.
        int ySize() const
        {
            return yStop - yStart;
        }

        /// \brief  Determine if the X window contains the index.
        /// \param  nX  Index to check
        /// \return  True if the index is contained, false otherwise.
        bool containsX(int nX) const
        {
            return nX >= xStart && nX < xStop;
        }

        /// \brief  Determine if the Y window contains the index.
        /// \param  nY  Index to check
        /// \return  True if the index is contained, false otherwise.
        bool containsY(int nY) const
        {
            return nY >= xStart && nY < yStop;
        }

        /// \brief  Determine if the window contains the index.
        /// \param  nX  X coordinate of the index to check
        /// \param  nY  Y coordinate of the index to check
        /// \return  True if the index is contained, false otherwise.
        bool contains(int nX, int nY) const
        {
            return containsX(nX) && containsY(nY);
        }

        /// \brief  Clamp the argument to be in the window in the X dimension.
        /// \param  nX  Value to clamp.
        /// \return  Clamped value.
        int clampX(int nX) const
        {
            return xSize() ? std::clamp(nX, xStart, xStop - 1) : xStart;
        }

        /// \brief  Clamp the argument to be in the window in the Y dimension.
        /// \param  nY  Value to clamp.
        /// \return  Clamped value.
        int clampY(int nY) const
        {
            return ySize() ? std::clamp(nY, yStart, yStop - 1) : yStart;
        }

        /// \brief  Shift the X dimension by nShift.
        /// \param  nShift  Amount to shift
        void shiftX(int nShift)
        {
            xStart += nShift;
            xStop += nShift;
        }
    };

    /**
     * Options for viewshed generation.
     */
    struct Options
    {
        Point observer{0, 0, 0};  //!< x, y, and z of the observer
        double visibleVal{255};   //!< raster output value for visible pixels.
        double invisibleVal{
            0};  //!< raster output value for non-visible pixels.
        double outOfRangeVal{
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
    CPL_DLL explicit Viewshed(const Options &opts)
        : oOpts{opts}, oOutExtent{}, oCurExtent{},
          dfMaxDistance2{opts.maxDistance * opts.maxDistance},
          dfZObserver{0}, poDstDS{}, pSrcBand{}, pDstBand{},
          dfHeightAdjFactor{0}, nLineCount{0}, adfTransform{0, 1, 0, 0, 0, 1},
          adfInvTransform{}, oProgress{}, oZcalc{}, oMutex{}, iMutex{}
    {
        if (dfMaxDistance2 == 0)
            dfMaxDistance2 = std::numeric_limits<double>::max();
    }

    Viewshed(const Viewshed &) = delete;
    Viewshed &operator=(const Viewshed &) = delete;

    CPL_DLL bool run(GDALRasterBandH hBand,
                     GDALProgressFunc pfnProgress = GDALDummyProgress,
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
    Window oOutExtent;
    Window oCurExtent;
    double dfMaxDistance2;
    double dfZObserver;
    std::unique_ptr<GDALDataset> poDstDS;
    GDALRasterBand *pSrcBand;
    GDALRasterBand *pDstBand;
    double dfHeightAdjFactor;
    int nLineCount;
    std::array<double, 6> adfTransform;
    std::array<double, 6> adfInvTransform;
    using ProgressFunc = std::function<bool(double frac, const char *msg)>;
    ProgressFunc oProgress;
    using ZCalc = std::function<double(int, int, double, double, double)>;
    ZCalc oZcalc;
    std::mutex oMutex;
    std::mutex iMutex;

    void setOutput(double &dfResult, double &dfCellVal, double dfZ);
    double calcHeight(double dfZ, double dfZ2);
    bool readLine(int nLine, double *data);
    bool writeLine(int nLine, std::vector<double> &vResult);
    bool processLine(int nX, int nY, int nLine,
                     std::vector<double> &vLastLineVal);
    bool processFirstLine(int nX, int nY, std::vector<double> &vLastLineVal);
    void processFirstLineLeft(int nX, int iStart, int iEnd,
                              std::vector<double> &vResult,
                              std::vector<double> &vThisLineVal);
    void processFirstLineRight(int nX, int iStart, int iEnd,
                               std::vector<double> &vResult,
                               std::vector<double> &vThisLineVal);
    void processFirstLineTopOrBottom(int iLeft, int iRight,
                                     std::vector<double> &vResult,
                                     std::vector<double> &vThisLineVal);
    void processLineLeft(int nX, int nYOffset, int iStart, int iEnd,
                         std::vector<double> &vResult,
                         std::vector<double> &vThisLineVal,
                         std::vector<double> &vLastLineVal);
    void processLineRight(int nX, int nYOffset, int iStart, int iEnd,
                          std::vector<double> &vResult,
                          std::vector<double> &vThisLineVal,
                          std::vector<double> &vLastLineVal);
    std::pair<int, int> adjustHeight(int iLine, int nX,
                                     std::vector<double> &thisLineVal);
    bool calcOutputExtent(int nX, int nY);
    bool createOutputDataset();
    bool lineProgress();
    bool emitProgress(double fraction);
};

}  // namespace gdal
