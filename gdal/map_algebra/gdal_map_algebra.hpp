#ifndef GDAL_MAP_ALGEBRA
#define GDAL_MAP_ALGEBRA

#include "gdal_map_algebra_core.h"
#include "gdal_map_algebra_classes.hpp"
#include "gdal_priv.h"

// an attempt at an API for the map algebra
// goals: no templates, no void pointers

// a function to create an argument
gma_band_t *gma_new_band(GDALRasterBand *b);

// a function to create an argument
gma_object_t *gma_new_object(GDALRasterBand *b, gma_class_t klass);

// optional helper functions

void print_histogram(gma_histogram_t *hm);

#endif
