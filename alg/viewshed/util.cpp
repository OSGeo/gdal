/******************************************************************************
 * (c) 2024 info@hobu.co
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

#include <array>

#include "gdal_priv.h"
#include "util.h"
#include "viewshed_types.h"

namespace gdal
{
namespace viewshed
{

/// Get the band size
///
/// @param  band Raster band
/// @return  The raster band size.
size_t bandSize(GDALRasterBand &band)
{
    return static_cast<size_t>(band.GetXSize()) * band.GetYSize();
}

/// Create the output dataset.
///
/// @param  srcBand  Source raster band.
/// @param  opts  Options.
/// @param  extent  Output dataset extent.
/// @return  The output dataset to be filled with data.
DatasetPtr createOutputDataset(GDALRasterBand &srcBand, const Options &opts,
                               const Window &extent)
{
    GDALDriverManager *hMgr = GetGDALDriverManager();
    GDALDriver *hDriver = hMgr->GetDriverByName(opts.outputFormat.c_str());
    if (!hDriver)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Cannot get driver");
        return nullptr;
    }

    /* create output raster */
    DatasetPtr dataset(hDriver->Create(
        opts.outputFilename.c_str(), extent.xSize(), extent.ySize(), 1,
        opts.outputMode == OutputMode::Normal ? GDT_Byte : GDT_Float64,
        const_cast<char **>(opts.creationOpts.List())));
    if (!dataset)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Cannot create dataset for %s",
                 opts.outputFilename.c_str());
        return nullptr;
    }

    /* copy srs */
    dataset->SetSpatialRef(srcBand.GetDataset()->GetSpatialRef());

    std::array<double, 6> adfSrcTransform;
    std::array<double, 6> adfDstTransform;
    srcBand.GetDataset()->GetGeoTransform(adfSrcTransform.data());
    adfDstTransform[0] = adfSrcTransform[0] +
                         adfSrcTransform[1] * extent.xStart +
                         adfSrcTransform[2] * extent.yStart;
    adfDstTransform[1] = adfSrcTransform[1];
    adfDstTransform[2] = adfSrcTransform[2];
    adfDstTransform[3] = adfSrcTransform[3] +
                         adfSrcTransform[4] * extent.xStart +
                         adfSrcTransform[5] * extent.yStart;
    adfDstTransform[4] = adfSrcTransform[4];
    adfDstTransform[5] = adfSrcTransform[5];
    dataset->SetGeoTransform(adfDstTransform.data());

    GDALRasterBand *pBand = dataset->GetRasterBand(1);
    if (!pBand)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Cannot get band for %s",
                 opts.outputFilename.c_str());
        return nullptr;
    }

    if (opts.nodataVal >= 0)
        GDALSetRasterNoDataValue(pBand, opts.nodataVal);
    return dataset;
}

}  // namespace viewshed
}  // namespace gdal
