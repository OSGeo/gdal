#include <time.h>
#include <stdlib.h>
#include <stdint.h>
#include "gdal_priv.h"

typedef enum { 
    gma_method_print, 
    gma_method_rand,
    gma_method_abs,
    gma_method_exp,
    gma_method_exp2,
    gma_method_log,
    gma_method_log2,
    gma_method_log10,
    gma_method_sqrt,
    gma_method_sin,
    gma_method_cos,
    gma_method_tan,
    gma_method_ceil,
    gma_method_floor,
    gma_method_set_border_cells
} gma_method_t;

typedef enum { 
    gma_method_histogram,
    gma_method_zonal_neighbors,
    gma_method_get_min,
    gma_method_get_max,
    gma_method_get_cells
} gma_method_compute_value_t;

typedef enum { 
    gma_method_assign,
    gma_method_add,
    gma_method_subtract,
    gma_method_multiply,
    gma_method_divide,
    gma_method_modulus,
    gma_method_map
} gma_method_with_arg_t;

typedef enum { 
    gma_method_assign_band,
    gma_method_add_band,
    gma_method_subtract_band,
    gma_method_multiply_by_band,
    gma_method_divide_by_band,
    gma_method_modulus_by_band,
    gma_method_zonal_min,
    gma_method_zonal_max,
    gma_method_set_zonal_min,
    gma_method_rim_by8,
    gma_method_depression_pour_elevation,
    gma_method_fill_dem,
    gma_method_D8,
    gma_method_route_flats,
    gma_method_depressions,
    gma_method_upstream_area,
    gma_method_catchment
} gma_two_bands_method_t;

#include "gma_hash.h"
#include "gma_band.h"
#include "type_switch.h"
#include "gdal_map_algebra_simple.h"
#include "gdal_map_algebra_return.h"
#include "gdal_map_algebra_arg.h"
#include "gdal_map_algebra_two_bands.h"
