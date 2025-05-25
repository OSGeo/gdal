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

double normalizeAngle(double maskAngle);
double horizontalIntersect(double angle, int nX, int nY, int y);
int hIntersect(double angle, int nX, int nY, int y);
int hIntersect(double angle, int nX, int nY, const Window &win);
double verticalIntersect(double angle, int nX, int nY, int x);
int vIntersect(double angle, int nX, int nY, int x);
int vIntersect(double angle, int nX, int nY, const Window &win);
bool rayBetween(double start, double end, double test);
size_t bandSize(GDALRasterBand &band);

DatasetPtr createOutputDataset(GDALRasterBand &srcBand, const Options &opts,
                               const Window &extent);

}  // namespace viewshed
}  // namespace gdal

#endif
