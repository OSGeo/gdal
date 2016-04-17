#ifndef GDAL_MAP_ALGEBRA_CORE_H
#define GDAL_MAP_ALGEBRA_CORE_H

#include <time.h>
#include <stdlib.h>
#include <stdint.h>

#ifdef __cpluplus
extern "C"
{
#endif

/* 
   Constants for creating argument objects or for
   detecting the type of return value objects.
*/
typedef enum {
    gma_object,
    gma_band,
    gma_number,
    gma_integer, /* not a real class, a number, which is integer */
    gma_pair,
    gma_range,  /* not a real class, a pair of two numbers of band datatype */
    gma_bins,
    gma_histogram,
    gma_classifier,
    gma_cell,
    gma_logical_operation, /* logical operator and a number */
    gma_cell_callback,
    gma_hash
} gma_class_t;

/* logical operators */

typedef enum {
    gma_eq,
    gma_ne,
    gma_gt,
    gma_lt,
    gma_ge,
    gma_le,
    gma_and,
    gma_or,
    gma_not
} gma_operator_t;

/* methods in four groups */

typedef enum { 
    gma_method_print, // print the band to stdout, remove this?
    gma_method_rand,  // sets the cell value with rand() [0..RAND_MAX]
    gma_method_abs,   // cell = abs(cell), no impact on nodata cells
    gma_method_exp,   // cell = exp(cell), no impact on nodata cells
    gma_method_exp2,  // cell = exp2(cell), no impact on nodata cells
    gma_method_log,   // cell = log(cell), no impact on nodata cells
    gma_method_log2,  // cell = log2(cell), no impact on nodata cells
    gma_method_log10, // cell = log10(cell), no impact on nodata cells
    gma_method_sqrt,  // cell = sqrt(cell), no impact on nodata cells
    gma_method_sin,   // cell = sin(cell), no impact on nodata cells
    gma_method_cos,   // cell = cos(cell), no impact on nodata cells
    gma_method_tan,   // cell = tan(cell), no impact on nodata cells
    gma_method_ceil,  // cell = ceil(cell), no impact on nodata cells
    gma_method_floor, // cell = floor(cell), no impact on nodata cells
} gma_method_t;

typedef enum { 
    gma_method_histogram,       // arg = NULL, pair:(n,pair:(min,max)), or bins; returns histogram
    gma_method_zonal_neighbors, // arg is ignored; returns hash of a hashes, keys are zone numbers
    gma_method_get_min,         // arg is ignored; returns a number
    gma_method_get_max,         // arg is ignored; returns a number
    gma_method_get_range,       // arg is ignored; returns a pair of numbers
    gma_method_get_cells        // arg is ignored; returns a std::vector of cells whose value is not zero
} gma_method_compute_value_t;

typedef enum { 
    gma_method_assign,          // arg must be a number, no impact on nodata cells
    gma_method_assign_all,      // arg must be a number, impact on all, also nodata, cells
    gma_method_add,             // arg must be a number, no impact on nodata cells
    gma_method_subtract,        // arg must be a number, no impact on nodata cells
    gma_method_multiply,        // arg must be a number, no impact on nodata cells
    gma_method_divide,          // arg must be a number, no impact on nodata cells
    gma_method_modulus,         // arg must be a number, no impact on nodata cells
    gma_method_classify,        // arg must be a classifier for the band datatype, no impact on nodata cells
    gma_method_cell_callback    // arg must be a cell callback, not called for nodata cells
} gma_method_with_arg_t;

typedef enum { 
    gma_method_assign_band,      // arg may be a logical operation, no impact on nodata cells, nodata cells are ignored
    gma_method_add_band,         // arg may be a logical operation, no impact on nodata cells, nodata cells are ignored
    gma_method_subtract_band,    // arg may be a logical operation, no impact on nodata cells, nodata cells are ignored
    gma_method_multiply_by_band, // arg may be a logical operation, no impact on nodata cells, nodata cells are ignored
    gma_method_divide_by_band,   // arg may be a logical operation, no impact on nodata cells, nodata cells are ignored
    gma_method_modulus_by_band,  // arg may be a logical operation, no impact on nodata cells, nodata cells are ignored
    gma_method_zonal_min,        // arg is ignored; returns a hash of zone => number, nodata cells are ignored
    gma_method_zonal_max,        // arg is ignored; returns a hash of zone => number, nodata cells are ignored
    gma_method_rim_by8,          // arg is ignored; returns nothing
    gma_method_D8,               // arg is ignored; return value should be ignored
    gma_method_route_flats,      // arg is ignored; return value should be ignored
    gma_method_fill_depressions, // arg is ignored; return value should be ignored
    gma_method_upstream_area,   // arg is ignored; return value should be ignored
    gma_method_catchment        // arg is cell; return value should be ignored
} gma_two_bands_method_t;

typedef enum {
    gma_method_if // b1 = b2 if decision; decision band should be uint8_t
} gma_spatial_decision_method_t;

#ifdef __cpluplus
} // extern "C"
#endif

#endif
