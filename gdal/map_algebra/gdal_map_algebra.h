#ifdef __cplusplus
  extern "C" {
#endif

#include "gdal_map_algebra_core.h"
#include "gdal_map_algebra_types.h"
#include "gdal.h"

// an attempt at an API for the map algebra
// goals: no templates, no void pointers

// a function to create an argument
gma_object_h gma_new_object(GDALRasterBandH b, gma_class_t klass);

void gma_simple(GDALRasterBandH b, gma_method_t method);
void gma_with_arg(GDALRasterBandH b, gma_method_with_arg_t method, gma_object_h arg);
gma_object_h gma_compute_value(GDALRasterBandH b, gma_method_compute_value_t method, gma_object_h arg);
gma_object_h gma_two_bands(GDALRasterBandH b1, gma_two_bands_method_t method, GDALRasterBandH b2, gma_object_h arg);
gma_object_h gma_spatial_decision(GDALRasterBandH b1, gma_spatial_decision_method_t method, GDALRasterBandH decision, GDALRasterBandH b2, gma_object_h arg);

// optional helper functions

void print_histogram(gma_histogram_h hm);

#ifdef __cplusplus
  }
#endif
