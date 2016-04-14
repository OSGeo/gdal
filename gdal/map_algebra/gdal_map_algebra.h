#include <time.h>
#include <stdlib.h>
#include <stdint.h>
#include "gdal_priv.h"

/* 
   Constants for creating argument objects or for
   detecting the type of return value objects.
*/
typedef enum {
    gma_object,
    gma_number,
    gma_integer, // not a real class, a number, which is integer
    gma_pair,
    gma_range,  // not a real class, a pair of two numbers of band datatype
    gma_bins,
    gma_histogram,
    gma_classifier,
    gma_cell,
    gma_logical_operation, // logical operator and a number
    gma_cell_callback,
    gma_hash
} gma_class_t;

/*
  Interface classes for argument and return value objects.
  It is legal to cast gma_object_t* to the subclass
  get_class() reports.
*/
class gma_object_t {
public:
    virtual ~gma_object_t() {};
    virtual gma_class_t get_class() {return gma_object;};
    virtual gma_object_t *clone() {};
};

class gma_number_t : public gma_object_t {
public:
    virtual gma_class_t get_class() {return gma_number;};
    virtual void set_value(double value) {};
    virtual void set_value(int value) {};
    virtual int value_as_int() {};
    virtual double value_as_double() {};
    virtual gma_number_t *clone() {};
    virtual void set_inf(int inf) {}; // -1 to minus inf, 0 to not inf, 1 to plus inf
    virtual bool is_inf() {};
    virtual bool is_integer() {};
    virtual bool is_float() {};
};

class gma_pair_t : public gma_object_t {
public:
    virtual gma_class_t get_class() {return gma_pair;};
    virtual void set_first(gma_object_t *first) {};
    virtual void set_second(gma_object_t *second) {};    
    virtual gma_object_t *first() {};
    virtual gma_object_t *second() {};
};

class gma_bins_t : public gma_object_t {
public:
    virtual gma_class_t get_class() {return gma_bins;};
    virtual unsigned int size() {};
    virtual void push(int value) {};
    virtual void push(double value) {};
};

class gma_histogram_t : public gma_object_t {
public:
    virtual gma_class_t get_class() {return gma_histogram;};
    virtual unsigned int size() {};
    virtual gma_object_t *at(unsigned int i) {};
};

class gma_classifier_t : public gma_object_t {
public:
    virtual gma_class_t get_class() {return gma_classifier;};
    virtual void add_class(gma_number_t *interval_max, gma_number_t *value) {};
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

/*
  Return value 0 interrupts, 1 denotes ok, and 2 denotes ok and a need
  to save the cell value back to band.
*/
typedef int (*gma_cell_callback_f)(gma_cell_t*);

class gma_cell_callback_t : public gma_object_t {
public:
    virtual gma_class_t get_class() {return gma_cell_callback;};
    virtual void set_callback(gma_cell_callback_f callback) {};
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
    virtual gma_class_t get_class() {return gma_logical_operation;};
    virtual void set_operation(gma_operator_t) {};
    virtual void set_value(int value) {};
    virtual void set_value(double value) {};
};

class gma_hash_t : public gma_object_t {
public:
    virtual gma_class_t get_class() {return gma_hash;};
    virtual int size() {};
    virtual std::vector<gma_number_t*> *keys_sorted() {};
    virtual gma_object_t *get(gma_number_t *key) {};
};

// methods in four groups

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

    // these two could probably go away
    gma_method_depressions,      // arg is ignored; return value should be ignored
    gma_method_depression_pour_elevation, // arg is ignored; returns a hash depression id => pour point elevation

    gma_method_upstream_area,   // arg is ignored; return value should be ignored
    gma_method_catchment        // arg is cell; return value should be ignored
} gma_two_bands_method_t;

typedef enum {
    gma_method_if // b1 = b2 if decision; decision band should be uint8_t
} gma_spatial_decision_method_t;

// an attempt at an API for the map algebra
// goals: no templates, no void pointers

// a function to create an argument
gma_object_t *gma_new_object(GDALRasterBand *b, gma_class_t klass);

// this could also be -with-arg where the arg is null
// failures are reported by CPLError
void gma_simple(GDALRasterBand *b, gma_method_t method);
void gma_with_arg(GDALRasterBand *b, gma_method_with_arg_t method, gma_object_t *arg);

// failures are reported by CPLError
gma_object_t *gma_compute_value(GDALRasterBand *b, gma_method_compute_value_t method, gma_object_t *arg = NULL);
gma_object_t *gma_two_bands(GDALRasterBand *b1, gma_two_bands_method_t method, GDALRasterBand *b2, gma_object_t *arg = NULL);

gma_object_t *gma_spatial_decision(GDALRasterBand *b1, 
                                   gma_spatial_decision_method_t method, 
                                   GDALRasterBand *decision, 
                                   GDALRasterBand *b2, 
                                   gma_object_t *arg = NULL);

// optional helper functions

void print_histogram(gma_histogram_t *hm);
