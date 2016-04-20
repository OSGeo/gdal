#ifndef GDAL_MAP_ALGEBRA_CLASSES
#define GDAL_MAP_ALGEBRA_CLASSES

#include "gdal_map_algebra_core.h"
#include "gdal.h"
#include "gdal_priv.h"
#include <vector>

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
    virtual void print() {};
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
    virtual int& x() {};
    virtual int& y() {};
    virtual void set_value(double value) {};
    virtual void set_value(int value) {};
    virtual int value_as_int() {};
    virtual double value_as_double() {};
};

/*
  Return value 0 interrupts, 1 denotes ok, and 2 denotes ok and a need
  to save the cell value back to band.
*/
typedef int (*gma_cell_callback_f)(gma_cell_t*, gma_object_t*);

class gma_cell_callback_t : public gma_object_t {
public:
    virtual gma_class_t get_class() {return gma_cell_callback;};
    virtual void set_callback(gma_cell_callback_f callback) {};
    virtual void set_user_data(gma_object_t*) {};
};

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

class gma_band_t : public gma_object_t {
public:
    virtual gma_class_t get_class() {return gma_band;};
    virtual void update() {};
    virtual GDALRasterBand *band() {};
    virtual GDALDataset *dataset() {};
    virtual GDALDriver *driver() {};
    virtual GDALDataType datatype() {};
    virtual bool datatype_is_integer() {};
    virtual bool datatype_is_float() {};
    virtual int w() {};
    virtual int h() {};

    virtual void set_progress_fct(GDALProgressFunc progress, void * progress_arg) {};

    virtual gma_band_t *new_band(const char *name, GDALDataType datatype) {};
    virtual gma_number_t *new_number() {};
    virtual gma_number_t *new_int(int value) {};
    virtual gma_pair_t *new_pair() {};
    virtual gma_pair_t *new_range() {};
    virtual gma_bins_t *new_bins() {};
    virtual gma_cell_t *new_cell() {};
    virtual gma_classifier_t *new_classifier() {};
    virtual gma_cell_callback_t *new_cell_callback() {};
    virtual gma_logical_operation_t *new_logical_operation() {};

    virtual void print() {};
    virtual void rand() {};
    virtual void abs() {};
    virtual void exp() {};
    virtual void log() {};
    virtual void log10() {};
    virtual void sqrt() {};
    virtual void sin() {};
    virtual void cos() {};
    virtual void tan() {};
    virtual void ceil() {};
    virtual void floor() {};

    // below op can be used to make the operation conditional
    virtual void assign(int value) {};
    virtual void assign_all(int value) {};
    virtual void add(int summand) {};
    virtual void subtract(int) {};
    virtual void multiply(int) {};
    virtual void divide(int) {};
    virtual void modulus(int divisor) {};
    
    virtual void assign(double value) {};
    virtual void assign_all(double value) {};
    virtual void add(double summand) {};
    virtual void subtract(double) {};
    virtual void multiply(double) {};
    virtual void divide(double) {};

    virtual void classify(gma_classifier_t*) {};
    virtual void cell_callback(gma_cell_callback_t*) {};

    // arg = NULL, pair:(n,pair:(min,max)), or bins; returns histogram
    virtual gma_histogram_t *histogram(gma_object_t *arg = NULL) {};
    // returns hash of a hashes, keys are zone numbers
    virtual gma_hash_t *zonal_neighbors() {};
    virtual gma_number_t *get_min() {};
    virtual gma_number_t *get_max() {};
    // returns a pair of numbers
    virtual gma_pair_t *get_range() {};
    virtual std::vector<gma_cell_t*> *cells() {};

    // below op can be used to make the operation conditional, 
    // the test is made against the value of the parameter band
    virtual void assign(gma_band_t *, gma_logical_operation_t *op = NULL) {};
    virtual void add(gma_band_t *, gma_logical_operation_t *op = NULL) {};
    virtual void subtract(gma_band_t *, gma_logical_operation_t *op = NULL) {};
    virtual void multiply(gma_band_t *, gma_logical_operation_t *op = NULL) {};
    virtual void divide(gma_band_t *, gma_logical_operation_t *op = NULL) {};
    virtual void modulus(gma_band_t *, gma_logical_operation_t *op = NULL) {};

    // this = value where decision is true
    // the decision band must be of type uint8_t
    virtual void decision(gma_band_t *value, gma_band_t *decision) {};

    virtual gma_hash_t *zonal_min(gma_band_t *zones) {};
    virtual gma_hash_t *zonal_max(gma_band_t *zones) {};

    virtual void rim_by8(gma_band_t *areas) {};

    virtual void fill_depressions(gma_band_t *dem) {};
    virtual void D8(gma_band_t *dem) {};
    virtual void route_flats(gma_band_t *dem) {};
    virtual void upstream_area(gma_band_t *) {};
    virtual void catchment(gma_band_t *, gma_cell_t *) {};

};

#endif
