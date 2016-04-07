#include <time.h>
#include <stdlib.h>
#include <stdint.h>
#include "gdal_priv.h"

/* 
   need classes for 
   intervals/ranges/zonal values (std::pair), 
   classifiers (std::unordered_map of integer => integer or bins => array of numbers)
   cells (int,int,number)
   logical operations (eq, le, .., number) (for if (op) then value -operations)
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

// base class and introspection,
// after the class is known, it is legal to cast it to an object of that class
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
    gma_method_zonal_neighbors, // 
    gma_method_get_min,
    gma_method_get_max,
    gma_method_get_range,
    gma_method_get_cells
} gma_method_compute_value_t;

typedef enum { 
    gma_method_assign,
    gma_method_add,
    gma_method_subtract,
    gma_method_multiply,
    gma_method_divide,
    gma_method_modulus,
    gma_method_classify,
    gma_method_cell_callback
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
    gma_method_D8,
    gma_method_route_flats,
    gma_method_fill_depressions,
    gma_method_depressions,
    gma_method_depression_pour_elevation,
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
gma_object_t *gma_compute_value(GDALRasterBand *b, gma_method_compute_value_t method, gma_object_t *arg = NULL);
gma_object_t *gma_two_bands(GDALRasterBand *b1, gma_two_bands_method_t method, GDALRasterBand *b2, gma_object_t *arg = NULL);

// optional helper functions

void print_histogram(gma_histogram_t *hm);
