#include <time.h>
#include <stdlib.h>
#include <stdint.h>
#include "gdal_priv.h"

typedef enum { gma_method_print, gma_method_rand } gma_method_t;
typedef enum { gma_method_histogram } gma_method_compute_value_t;
typedef enum { gma_method_add } gma_method_with_arg_t;
typedef enum { gma_method_add_band, gma_method_D8, gma_method_pit_removal, gma_method_route_flats } gma_two_bands_method_t;

#include "gdal_map_algebra_simple.h"
#include "gdal_map_algebra_return.h"
#include "gdal_map_algebra_arg.h"
#include "gdal_map_algebra_two_bands.h"
