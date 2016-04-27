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
    gma_pair,
    gma_bins,
    gma_histogram,
    gma_classifier,
    gma_cell,
    gma_logical_operation, /* logical operator and a number */
    gma_cell_callback,
    gma_hash,
    gma_iterator
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

#ifdef __cpluplus
} // extern "C"
#endif

#endif
