#ifndef GDAL_MAP_ALGEBRA
#define GDAL_MAP_ALGEBRA

#include "gdal_map_algebra_core.h"
#include "gdal_map_algebra_classes.hpp"
#include "gdal_priv.h"

// a band object factory
gma_band_t *gma_new_band(GDALRasterBand *b);
gma_band_t *gma_new_band(const char *name);

#endif
