#include <time.h>
#include <stdlib.h>
#include <stdint.h>
#include "gdal_priv.h"

/* 
   need classes for 
   numbers, 
   intervals/ranges/zonal values (std::pair), 
   bins (division of number line into intervals) (std::vector)
   histograms (std::unordered_map of integer => count or bins => array of counts)
   reclassifiers (std::unordered_map of integer => integer or bins => array of numbers)
   cells (int,int,number)
   logical operations (eq, le, .., number) (for if (op) then value -operations)
*/
typedef enum {
    gma_object,
    gma_number,
    gma_pair,
    gma_bins,
    gma_histogram,
    gma_reclassifier,
    gma_cell,
    gma_logical_operation
} gma_class_t;

// base class and introspection,
// after the class is known, it is legal to cast it to an object of that class
class gma_object_t {
public:
    gma_object_t() {};
    virtual ~gma_object_t() {};
    virtual gma_class_t get_class() {return gma_object;};
};

class gma_number_t : public gma_object_t {
public:
    virtual gma_class_t get_class() {return gma_number;};
    virtual void set_value(double value) {};
    virtual void set_value(int value) {};
    virtual int value_as_int() {};
    virtual double value_as_double() {};
    virtual gma_number_t *clone() {};
};

class gma_pair_t : public gma_object_t {
    public:
    virtual gma_class_t get_class() {return gma_pair;};
    virtual gma_object_t *first() {};
    virtual gma_object_t *second() {};
};

class gma_histogram_t : public gma_object_t {
public:
    virtual gma_class_t get_class() {return gma_histogram;};
    virtual unsigned int size() {};
    virtual gma_object_t *at(unsigned int i) {};
};

class gma_cell_t  : public gma_object_t {
public:
    virtual gma_class_t get_class() {return gma_cell;};
    virtual int x() {};
    virtual int y() {};
    virtual void set_value(double value) {};
    virtual void set_value(int value) {};
    virtual int value_as_int() {};
    virtual double value_as_double() {};
};

// logical operators

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

class gma_logical_operation_t  : public gma_object_t {
public:
    gma_logical_operation_t(gma_operator_t, gma_number_t);
    virtual gma_class_t get_class() {return gma_cell;};
    virtual void set_operation(gma_operator_t) {};
    virtual void set_number(gma_number_t*) {};
};

// methods in four groups

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

// an attempt at an API for the map algebra
// goals: no templates, no void pointers

// a function to create an argument
gma_object_t *gma_new_object(GDALRasterBand *b, gma_class_t klass);

// this could also be -with-arg where the arg is null
// return int for ok/fail?
void gma_simple(GDALRasterBand *b, gma_method_t method);
void gma_with_arg(GDALRasterBand *b, gma_method_with_arg_t method, gma_object_t *arg);

// return null if fail
gma_object_t *gma_compute_value(GDALRasterBand *b, gma_method_compute_value_t method, gma_object_t *arg);
gma_object_t *gma_two_bands(GDALRasterBand *b1, gma_two_bands_method_t method, GDALRasterBand *b2, gma_object_t *arg);

#include "gma_hash.h"
#include "gma_band.h"
#include "type_switch.h"
#include "gdal_map_algebra_simple.h"
#include "gdal_map_algebra_compute_value.h"
#include "gdal_map_algebra_arg.h"
#include "gdal_map_algebra_two_bands.h"


