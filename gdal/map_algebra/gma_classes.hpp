#include "gdal_priv.h"
#include <limits>
#include <vector>
#include <algorithm>

// concrete classes

template <typename type> class gma_number_p : public gma_number_t {
    int m_inf;
    bool m_defined;
    type m_value;
public:
    gma_number_p() {
        m_inf = 0;
        m_defined = false;
    }
    gma_number_p(type value) {
        m_inf = 0;
        m_defined = true;
        m_value = value;
    }
    virtual gma_number_t *clone() {
        gma_number_p<type> *n = new gma_number_p<type>();
        if (m_defined)
            n->set_value(m_value);
        return n;
    }
    bool defined() {
        return m_defined;
    }
    type value() {
        if (!m_defined)
            return std::numeric_limits<type>::quiet_NaN();
        if (m_inf < 0) {
            if (std::numeric_limits<type>::has_infinity)
                return -1*std::numeric_limits<type>::infinity();
            else
                return std::numeric_limits<type>::min();
        } else if (m_inf > 0)
            if (std::numeric_limits<type>::has_infinity)
                return std::numeric_limits<type>::infinity();
            else
                return std::numeric_limits<type>::max();
        else
            return (double)m_value;
    }
    void inc() {
        ++m_value;
    }
    virtual void set_value(double value) {
        m_defined = true;
        value = MAX(MIN(value, std::numeric_limits<type>::max()), std::numeric_limits<type>::min());
        m_value = (type)value;
    }
    virtual void set_value(int value) {
        m_defined = true;
        value = MAX(MIN(value, std::numeric_limits<type>::max()), std::numeric_limits<type>::min());
        m_value = (type)value;
    }
    virtual void set_value(unsigned int value) {
        m_defined = true;
        value = MAX(MIN(value, std::numeric_limits<type>::max()), std::numeric_limits<type>::min());
        m_value = (type)value;
    }
    virtual int value_as_int() {
        if (!m_defined)
            return std::numeric_limits<type>::quiet_NaN();
        if (m_inf < 0)
            return std::numeric_limits<type>::min();
        else if (m_inf > 0)
            return std::numeric_limits<type>::max();
        else
            return (int)m_value;
    }
    virtual unsigned value_as_unsigned() {
        if (!m_defined)
            return std::numeric_limits<type>::quiet_NaN();
        if (m_inf < 0)
            return std::numeric_limits<type>::min();
        else if (m_inf > 0)
            return std::numeric_limits<type>::max();
        else
            return (unsigned)m_value;
    }
    virtual double value_as_double() {
        if (!m_defined)
            return std::numeric_limits<type>::quiet_NaN();
        if (m_inf < 0) {
            if (std::numeric_limits<type>::has_infinity)
                return -1*std::numeric_limits<type>::infinity();
            else
                return std::numeric_limits<type>::min();
        } else if (m_inf > 0)
            if (std::numeric_limits<type>::has_infinity)
                return std::numeric_limits<type>::infinity();
            else
                return std::numeric_limits<type>::max();
        else
            return (double)m_value;
    }
    static const char *format();
    char *as_string() {
        char *s = (char*)CPLMalloc(20);
        if (!m_defined)
            snprintf(s, 20, "NaN");
        else if (m_inf < 0) {
            if (std::numeric_limits<type>::is_integer)
                snprintf(s, 20, format(), std::numeric_limits<type>::min());
            else
                snprintf(s, 20, "-inf");
        } else if (m_inf > 0)
            if (std::numeric_limits<type>::is_integer)
                snprintf(s, 20, format(), std::numeric_limits<type>::max());
            else
                snprintf(s, 20, "+inf");
        else
            snprintf(s, 20, format(), m_value);
        return s;
    }
    virtual bool is_defined() { return m_defined; }
    virtual void set_inf(int inf) { m_inf = inf; }
    virtual bool is_inf() { return m_inf; }
    virtual bool is_integer() { return std::numeric_limits<type>::is_integer; }
    virtual bool is_unsigned() { return !std::numeric_limits<type>::is_signed; }
    virtual bool is_float();
    static GDALDataType datatype_p();
    virtual GDALDataType datatype() { return gma_number_p<type>::datatype_p(); };
};

// wrap std::pair as concrete type gma_pair_p,
// K and V must be subclasses of gma_object_t
template<class K, class V>class gma_pair_p : public gma_pair_t {
    std::pair<K,V> m_pair;
public:
    gma_pair_p() {
        m_pair.first = NULL;
        m_pair.second = NULL;
    }
    gma_pair_p(K key, V value) {
        m_pair.first = key;
        m_pair.second = value;
    }
    virtual void set_first(gma_object_t *first) {
        m_pair.first = (K)first;
    }
    virtual void set_second(gma_object_t *second) {
        m_pair.second = (V)second;
    }
    virtual gma_object_t *first() {
        return m_pair.first;
    }
    virtual gma_object_t *second() {
        return m_pair.second;
    }
};

template <typename datatype_t> class gma_cell_p : public gma_cell_t {
    int m_x;
    int m_y;
    datatype_t m_value;
public:
    gma_cell_p(int x, int y, datatype_t value) {
        m_x = x;
        m_y = y;
        m_value = value;
    }
    datatype_t& value() {
        return m_value;
    }
    virtual int x() {
        return m_x;
    }
    virtual int y() {
        return m_y;
    }
    virtual void set_x(int x) {
        m_x = x;
    }
    virtual void set_y(int y) {
        m_y = y;
    }
    virtual void set_value(double value) {
        m_value = value;
    }
    virtual void set_value(int value) {
        m_value = value;
    }
    virtual int value_as_int() {
        return (int)m_value;
    }
    virtual double value_as_double() {
        return (double)m_value;
    }
    virtual GDALDataType datatype() { return gma_number_p<datatype_t>::datatype_p(); };
};

// integer datatype_t => object
template <typename datatype_t, class V> class gma_hash_entry {
    datatype_t m_key;
    V *m_value;
    gma_hash_entry<datatype_t,V> *m_next;
public:
    gma_hash_entry(datatype_t key, V *value) {
        m_key = key;
        m_value = value;
        m_next = NULL;
    }
    ~gma_hash_entry() {
        delete m_value;
    }
    datatype_t key() {
        return m_key;
    }
    V *value() {
        return m_value;
    }
    void set_value(V *value) {
        if (m_value) delete m_value;
        m_value = value;
    }
    gma_hash_entry<datatype_t,V> *next() {
        return m_next;
    }
    void set_next(gma_hash_entry<datatype_t,V> *next) {
        m_next = next;
    }
};

template <typename datatype_t> bool gma_numeric_comparison(datatype_t a, datatype_t b) {
  return (a < b);
}

// replace with std::unordered_map? seems to get a bit unnecessarily complicated
template <typename datatype_t, class V> class gma_hash_p : public gma_hash_t {
    static const int m_size = 128;
    std::vector<gma_hash_entry<datatype_t,V>*> m_table;
public:
    gma_hash_p() {
        m_table.resize(m_size);
        for (int i = 0; i < m_size; ++i) {
            m_table[i] = NULL;
        }
    }
    ~gma_hash_p() {
        for (int i = 0; i < m_size; ++i) {
            gma_hash_entry<datatype_t,V> *e = m_table[i];
            while (e) {
                gma_hash_entry<datatype_t,V> *n = e->next();
                delete e;
                e = n;
            }
        }
    }
    bool exists(datatype_t key) {
        int hash = (abs(key) % m_size);
        gma_hash_entry<datatype_t,V> *e = m_table[hash];
        while (e && e->key() != key)
            e = e->next();
        return e != NULL;
    }
    void del(datatype_t key) {
        int hash = (abs(key) % m_size);
        gma_hash_entry<datatype_t,V> *e = m_table[hash], *p = NULL;
        while (e && e->key() != key) {
            p = e;
            e = e->next();
        }
        if (e) {
            gma_hash_entry<datatype_t,V> *n = e->next();
            delete e;
            if (p)
                p->set_next(n);
            else
                m_table[hash] = NULL;
        }
    }
    V *get(datatype_t key) {
        int hash = (abs(key) % m_size);
        gma_hash_entry<datatype_t,V> *e = m_table[hash];
        while (e && e->key() != key)
            e = e->next();
        if (e)
            return e->value();
        else
            return NULL;
    }
    virtual gma_object_t *get(gma_number_t *key) {
        datatype_t k = ((gma_number_p<datatype_t>*)key)->value();
        return get(k);
    }
    void put(datatype_t key, V *value) {
        int hash = (abs(key) % m_size);
        gma_hash_entry<datatype_t,V> *e = m_table[hash], *p = NULL;
        while (e && e->key() != key) {
            p = e;
            e = e->next();
        }
        if (e)
            e->set_value(value);
        else {
            e = new gma_hash_entry<datatype_t,V>(key, value);
            if (p)
                p->set_next(e);
            else
                m_table[hash] = e;
        }
    }
    virtual int size() {
        int n = 0;
        for (int i = 0; i < m_size; ++i) {
            gma_hash_entry<datatype_t,V> *e = m_table[i];
            while (e) {
                ++n;
                e = e->next();
            }
        }
        return n;
    }
    std::vector<datatype_t> keys(int size) {
        std::vector<datatype_t> keys(size);
        int i = 0;
        for (int j = 0; j < m_size; ++j) {
            gma_hash_entry<datatype_t,V> *e = m_table[j];
            while (e) {
                datatype_t key = e->key();
                keys[i++] = key;
                e = e->next();
            }
        }
        return keys;
    }
    std::vector<datatype_t> keys_sorted(int size) {
        std::vector<datatype_t> k = keys(size);
        std::sort(k.begin(), k.end(), gma_numeric_comparison<datatype_t>);
        return k;
    }
    virtual std::vector<gma_number_t*> keys_sorted() {
        int n = size();
        std::vector<datatype_t> keys = keys_sorted(n);
        std::vector<gma_number_t*> retval;
        for (int i = 0; i < n; ++i)
            retval.push_back(new gma_number_p<datatype_t>(keys[i]));
        return retval;
    }
    virtual GDALDataType datatype() { return gma_number_p<datatype_t>::datatype_p(); };
};

// a class to divide real | integer line into intervals without holes in between
// -inf ... x(i) .. x(i+1) .. .. +inf
// each range is (x(i),x(i+1)]
template <typename datatype_t> class gma_bins_p : public gma_bins_t {
    std::vector<datatype_t> m_data;
public:
    gma_bins_p() {
    }
    ~gma_bins_p() {
    }
    int add(datatype_t x) {
        int n = m_data.size();
        if (n == 0) {
            m_data.push_back(x);
            return 0;
        }
        int i = bin(x);
        datatype_t tmp = m_data[n-1];
        m_data.push_back(tmp);
        for (int j = n-1; j > i; --j) {
            m_data[j] = m_data[j-1];
        }
        m_data[i] = x;
        return i;
    }
    datatype_t get(int i) {
        return m_data[i];
    }
    void set(int i, datatype_t x) {
        m_data[i] = x;
    }
    int bin(datatype_t x) {
        int i = 0;
        while (i < m_data.size() && x > m_data[i])
            ++i;
        return i;
    }
    gma_bins_p<datatype_t> *clone_p() {
        gma_bins_p<datatype_t> *c = new gma_bins_p<datatype_t>;
        for (int i = 0; i < m_data.size(); ++i)
            c->m_data.push_back(m_data[i]);
        return c;
    }
    virtual unsigned int size() {
        return m_data.size()+1;
    }
    virtual void push(int value) {
        m_data.push_back((datatype_t)value);
    }
    virtual void push(double value) {
        m_data.push_back((datatype_t)value);
    }
    virtual GDALDataType datatype() { return gma_number_p<datatype_t>::datatype_p(); };
};

template <typename datatype_t>
class gma_histogram_p : public gma_histogram_t {
    bool m_sorted;
    std::vector<datatype_t> sorted_hash_keys;
    gma_hash_p<datatype_t,gma_number_p<unsigned int> > *hash;
    gma_bins_p<datatype_t> *bins;
    std::vector<unsigned int> *counts;
public:
    gma_histogram_p(gma_object_t *arg) {
        hash = NULL;
        m_sorted = false;
        bins = NULL;
        counts = NULL;
        if (arg == NULL)
            hash = new gma_hash_p<datatype_t,gma_number_p<unsigned int> >;
        else if (arg->get_class() == gma_pair) { // (n bins, (min, max))
            gma_pair_p<gma_object_t*,gma_object_t* > *a = (gma_pair_p<gma_object_t*,gma_object_t* > *)arg;
            gma_number_p<int> *a1 = (gma_number_p<int> *)a->first();
            gma_pair_p<gma_number_p<datatype_t>*,gma_number_p<datatype_t>*> *a2 =
                (gma_pair_p<gma_number_p<datatype_t>*,gma_number_p<datatype_t>*> *)a->second();
            gma_number_p<datatype_t> *a21 = (gma_number_p<datatype_t> *)a2->first();
            gma_number_p<datatype_t> *a22 = (gma_number_p<datatype_t> *)a2->second();
            int n = a1->value();
            double min = a21->value();
            double max = a22->value();
            double d = (max-min)/n;
            bins = new gma_bins_p<datatype_t>;
            counts = new std::vector<unsigned int>;
            for (int i = 0; i < n-1; ++i) {
                bins->push(min+d*(i+1));
            }
            for (int i = 0; i < n; ++i) {
                counts->push_back(0);
            }
        } else if (arg->get_class() == gma_bins) {
            bins = ((gma_bins_p<datatype_t>*)arg)->clone_p();
            counts = new std::vector<unsigned int>;
            for (int i = 0; i < bins->size(); ++i)
                counts->push_back(0);
        } else {
            // error
        }
    }
    ~gma_histogram_p() {
        delete hash;
    }
    void set_size(int size, datatype_t min, datatype_t max) {
    }
    virtual unsigned int size() {
        if (hash)
            return hash->size();
        return bins->size();
    }
    virtual gma_object_t *at(unsigned int i) {
        if (hash) {
            if (!m_sorted) {
                sorted_hash_keys = hash->keys_sorted(hash->size());
                m_sorted = true;
            }
            datatype_t key = sorted_hash_keys[i];
            gma_number_p<unsigned int> *value = (gma_number_p<unsigned int>*)hash->get(key);
            // return gma_pair_t
            gma_number_p<datatype_t> *k = new gma_number_p<datatype_t>(key);
            gma_number_p<unsigned int> *v = new gma_number_p<unsigned int>(value->value());
            gma_pair_p<gma_number_p<datatype_t>*,gma_number_p<unsigned int>* > *pair =
                new gma_pair_p<gma_number_p<datatype_t>*,gma_number_p<unsigned int>* >(k,v);
            return pair;
        }
        gma_number_p<datatype_t> *min;
        gma_number_p<datatype_t> *max;
        if (i == 0) {
            min = new gma_number_p<datatype_t>(0);
            min->set_inf(-1);
            max = new gma_number_p<datatype_t>(bins->get(i));
        } else if (i < bins->size() - 1) {
            min = new gma_number_p<datatype_t>(bins->get(i-1));
            max = new gma_number_p<datatype_t>(bins->get(i));
        } else {
            min = new gma_number_p<datatype_t>(bins->get(i-1));
            max = new gma_number_p<datatype_t>(0);
            max->set_inf(1);
        }
        gma_pair_p<gma_number_p<datatype_t>*,gma_number_p<datatype_t>* > *k =
            new gma_pair_p<gma_number_p<datatype_t>*,gma_number_p<datatype_t>* >(min,max);
        gma_number_p<unsigned int> *v = new gma_number_p<unsigned int>(counts->at(i));
        gma_pair_p<gma_pair_p<gma_number_p<datatype_t>*,gma_number_p<datatype_t>* >*,gma_number_p<unsigned int>* > *pair =
            new gma_pair_p<gma_pair_p<gma_number_p<datatype_t>*,gma_number_p<datatype_t>* >*,gma_number_p<unsigned int>* >(k,v);
        return pair;
    }
    void increase_count_at(datatype_t value) {
        if (hash) {
            if (hash->exists(value))
                ((gma_number_p<unsigned int>*)hash->get(value))->inc();
            else
                hash->put(value, new gma_number_p<unsigned int>(1));
        } else {
            int i = bins->bin(value);
            ++(counts->at(i));
        }
    }
    virtual void print() {
        for (unsigned int i = 0; i < size(); ++i) {
            gma_pair_t *kv = (gma_pair_t *)at(i);
            // kv is an interval=>number or number=>number
            if (kv->first()->get_class() == gma_pair) {
                gma_pair_t *key = (gma_pair_t*)kv->first();
                gma_number_p<datatype_t> *min = (gma_number_p<datatype_t>*)key->first();
                gma_number_p<datatype_t> *max = (gma_number_p<datatype_t>*)key->second();
                gma_number_p<unsigned int> *val = (gma_number_p<unsigned int>*)kv->second();
                printf("(%s .. %s] => %s\n", min->as_string(), max->as_string(), val->as_string());
            } else {
                gma_number_p<datatype_t> *key = (gma_number_p<datatype_t>*)kv->first();
                gma_number_p<unsigned int> *val = (gma_number_p<unsigned int>*)kv->second();
                printf("%s => %s\n", key->as_string(), val->as_string());
            }
        }
    }
    virtual GDALDataType datatype() { return gma_number_p<datatype_t>::datatype_p(); };
};

template <typename datatype_t>
class gma_classifier_p : public gma_classifier_t {
    gma_hash_p<datatype_t,gma_number_p<datatype_t> > *m_hash;
    gma_bins_p<datatype_t> *m_bins;
    std::vector<datatype_t> *m_values;
    bool m_hash_ok;
    bool has_default;
    datatype_t deflt;
public:
    gma_classifier_p(bool hash_ok) {
        m_hash_ok = hash_ok;
        m_hash = NULL;
        m_bins = NULL;
        m_values = NULL;
        has_default = false;
    }
    ~gma_classifier_p() {
        delete m_hash;
        delete m_bins;
        delete m_values;
    }
    virtual void add_class(gma_number_t *interval_max, gma_number_t *value) {
        if (value->datatype() != datatype()) {
            // fixme: emit error
            return;
        }
        // fixme: error if hash is not null
        if (m_bins == NULL) {
            m_bins = new gma_bins_p<datatype_t>;
            m_values = new std::vector<datatype_t>;
            m_values->push_back(0);
        }
        int i;
        //int n = m_bins->size();
        int m = m_values->size();
        datatype_t val = ((gma_number_p<datatype_t>*)value)->value();
        if (interval_max->is_inf())
            m_values->at(m-1) = val;
        else {
            i = m_bins->add(((gma_number_p<datatype_t>*)interval_max)->value());
            m_values->resize(m+1);
            for (int j = m; j > i; --j)
                m_values->at(j) = m_values->at(j-1);
            m_values->at(i) = val;
        }
    }
    virtual void add_value(gma_number_t *old_value, gma_number_t *new_value) {
        if (old_value->datatype() != datatype() || new_value->datatype() != datatype()) {
            // fixme: emit error
            return;
        }
        // fixme: error if bins is not null
        if (m_hash == NULL) {
            m_hash = new gma_hash_p<datatype_t,gma_number_p<datatype_t> >;
        }
        gma_number_p<datatype_t> *old = (gma_number_p<datatype_t>*)old_value;
        m_hash->put(old->value(), (gma_number_p<datatype_t>*)new_value);
    }
    virtual void add_default(gma_number_t *default_value) {
        if (default_value->datatype() != datatype()) {
            // fixme: emit error
            return;
        }
        has_default = true;
        deflt = ((gma_number_p<datatype_t>*)default_value)->value();
    }
    datatype_t classify(datatype_t value) {
        if (m_hash && m_hash->exists(value)) {
            return m_hash->get(value)->value();
        } else if (m_bins) {
            return m_values->at(m_bins->bin(value));
        } else if (has_default) {
            return deflt;
        } else
            return value;
    }
    virtual GDALDataType datatype() { return gma_number_p<datatype_t>::datatype_p(); };
};

template <typename datatype_t>
class gma_logical_operation_p : public gma_logical_operation_t {
public:
    gma_operator_t m_op;
    datatype_t m_value;
    gma_logical_operation_p() {
        m_op = gma_eq;
        m_value = 0;
    }
    virtual GDALDataType datatype() {
        return gma_number_p<datatype_t>::datatype_p();
    }
    virtual void set_operation(gma_operator_t op) {
        m_op = op;
    }
    virtual void set_value(int value) {
        m_value = value;
    }
    virtual void set_value(double value) {
        m_value = value;
    }
};

class gma_cell_callback_p : public gma_cell_callback_t {
public:
    gma_cell_callback_f m_callback;
    gma_object_t *m_user_data;
    virtual void set_callback(gma_cell_callback_f callback) {
        m_callback = callback;
        m_user_data = NULL;
    }
    virtual void set_user_data(gma_object_t *user_data) {
        m_user_data = user_data;
    }
};
