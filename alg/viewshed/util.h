#pragma once

#include "viewshed_types.h"

namespace gdal
{
namespace viewshed
{

DatasetPtr createOutputDataset(GDALRasterBand &srcBand, const Options &opts,
                               const Window &extent);

}  // namespace viewshed
}  // namespace gdal
