/******************************************************************************
 * (c) 2024 info@hobu.co
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#ifndef VIEWSHED_UTIL_H_INCLUDED
#define VIEWSHED_UTIL_H_INCLUDED

#include "viewshed_types.h"

namespace gdal
{
namespace viewshed
{

size_t bandSize(GDALRasterBand &band);

DatasetPtr createOutputDataset(GDALRasterBand &srcBand, const Options &opts,
                               const Window &extent);

}  // namespace viewshed
}  // namespace gdal

#endif
