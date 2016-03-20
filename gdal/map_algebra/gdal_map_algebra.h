#include <time.h>
#include <stdlib.h>
#include <stdint.h>
#include "gdal_priv.h"

typedef enum { gma_method_print, gma_method_rand } gma_method_t;
typedef enum { gma_method_add } gma_method_with_arg_t;
typedef enum { gma_method_add_band, gma_method_D8 } gma_two_bands_method_t;

#include "gdal_map_algebra_simple.h"
#include "gdal_map_algebra_arg.h"
#include "gdal_map_algebra_two_bands.h"
