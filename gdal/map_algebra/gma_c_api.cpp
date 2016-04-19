#include "gdal_map_algebra.h"
#include "gdal_map_algebra.hpp"

#ifdef __cplusplus
  extern "C" {
#endif

gma_class_t gma_object_get_class(gma_object_h o) {
    return ((gma_object_t*)o)->get_class();
}

gma_object_h gma_new_object(GDALRasterBandH b, gma_class_t klass) {
      return (gma_object_h)gma_new_object((GDALRasterBand*)b, klass);
}

#ifdef __cplusplus
  }
#endif
