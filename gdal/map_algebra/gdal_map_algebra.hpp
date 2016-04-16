#include "gdal_map_algebra_core.h"
#include "gdal_map_algebra_classes.hpp"
#include "gdal_priv.h"

// an attempt at an API for the map algebra
// goals: no templates, no void pointers

// a function to create an argument
gma_object_t *gma_new_object(GDALRasterBand *b, gma_class_t klass);

void gma_simple(GDALRasterBand *b, gma_method_t method);
void gma_with_arg(GDALRasterBand *b, gma_method_with_arg_t method, gma_object_t *arg);
gma_object_t *gma_compute_value(GDALRasterBand *b, gma_method_compute_value_t method, gma_object_t *arg = NULL);
gma_object_t *gma_two_bands(GDALRasterBand *b1, gma_two_bands_method_t method, GDALRasterBand *b2, gma_object_t *arg = NULL);
gma_object_t *gma_spatial_decision(GDALRasterBand *b1, gma_spatial_decision_method_t method, GDALRasterBand *decision, GDALRasterBand *b2, gma_object_t *arg = NULL);

// optional helper functions

void print_histogram(gma_histogram_t *hm);
