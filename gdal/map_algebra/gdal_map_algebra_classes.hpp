#include "gdal_map_algebra_core.h"
#include "gdal.h"
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
    virtual GDALDataType gdal_datatype() {};
    virtual gma_number_t *new_number(int value) {};
    virtual void print() {};
    virtual void rand() {};
    virtual void add(int summand) {};
    virtual void modulus(int divisor) {};
    virtual void add(gma_band_t *summand) {};
    virtual gma_pair_t *get_range() {};
    virtual gma_histogram_t *histogram(gma_object_t *arg = NULL) {};
};
