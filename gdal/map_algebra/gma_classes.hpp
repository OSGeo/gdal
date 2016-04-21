#include "gdal_priv.h"
#include <limits>

// concrete classes

template <typename type> class gma_number_p : public gma_number_t {
public:
    int m_inf;
    bool m_defined;
    type m_value;
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
        return m_value;
    }
    void inc() {
        m_value++;
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
        if (m_inf < 0)
            return std::numeric_limits<type>::min();
        else if (m_inf > 0)
            return std::numeric_limits<type>::max();
        else
            return (int)m_value;
    }
    virtual double value_as_double() {
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
    static const char *space();
    static const char *format();
    char *as_string() {
        char *s = (char*)CPLMalloc(10);
        snprintf(s, 10, format(), m_value);
        return s;
    }
    virtual bool is_defined() { return m_defined; }
    virtual void set_inf(int inf) { m_inf = inf; }
    virtual bool is_inf() { return m_inf; }
    virtual bool is_integer() { return std::numeric_limits<type>::is_integer; }
    virtual bool is_float();
    static GDALDataType datatype_p();
    virtual GDALDataType datatype() { return gma_number_p<type>::datatype_p(); };
};

// wrap std::pair as concrete type gma_pair_p, 
// K and V must be subclasses of gma_object_t
template<class K, class V>class gma_pair_p : public gma_pair_t {
public:
    std::pair<K,V> m_pair;
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
    virtual int& x() {
        return m_x;
    }
    virtual int& y() {
        return m_y;
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
private:
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

template <typename datatype_t> int gma_numeric_comparison(const void *a, const void *b) {
  const datatype_t *da = (const datatype_t *) a;
  const datatype_t *db = (const datatype_t *) b;
  return (*da > *db) - (*da < *db);
}

// replace with std::unordered_map? seems to get a bit unnecessarily complicated
template <typename datatype_t, class V> class gma_hash_p : public gma_hash_t {
public:
    gma_hash_entry<datatype_t,V> **table;
    static const int table_size = 128;
    gma_hash_p() {
        table = (gma_hash_entry<datatype_t,V> **)CPLMalloc(sizeof(void*)*table_size);
        if (!table) return;
        for (int i = 0; i < table_size; i++)
            table[i] = NULL;
    }
    ~gma_hash_p() {
        if (!table) return;
        for (int i = 0; i < table_size; i++) {
            gma_hash_entry<datatype_t,V> *e = table[i];
            while (e) {
                gma_hash_entry<datatype_t,V> *n = e->next();
                delete e;
                e = n;
            }
        }
        CPLFree(table);
    }
    bool exists(datatype_t key) {
        if (!table) return false;
        int hash = (abs(key) % table_size);
        gma_hash_entry<datatype_t,V> *e = table[hash];
        while (e && e->key() != key)
            e = e->next();
        return e != NULL;
    }
    void del(datatype_t key) {
        if (!table) return;
        int hash = (abs(key) % table_size);
        gma_hash_entry<datatype_t,V> *e = table[hash], *p = NULL;
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
                table[hash] = NULL;
        }
    }
    V *get(datatype_t key) {
        if (!table) return NULL;
        int hash = (abs(key) % table_size);
        gma_hash_entry<datatype_t,V> *e = table[hash];
        while (e && e->key() != key)
            e = e->next();
        if (e)
            return e->value();
    }
    virtual gma_object_t *get(gma_number_t *key) {
        datatype_t k = ((gma_number_p<datatype_t>*)key)->value();
        return get(k);
    }
    void put(datatype_t key, V *value) {
        if (!table) return;
        int hash = (abs(key) % table_size);
        gma_hash_entry<datatype_t,V> *e = table[hash], *p = NULL;
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
                table[hash] = e;
        }
    }
    virtual int size() {
        if (!table) return 0;
        int n = 0;
        for (int i = 0; i < table_size; i++) {
            gma_hash_entry<datatype_t,V> *e = table[i];
            while (e) {
                n++;
                e = e->next();
            }
        }
        return n;
    }
    datatype_t *keys(int size) {
        if (!table) return NULL;
        datatype_t *keys = (datatype_t *)CPLMalloc(size*sizeof(datatype_t));
        if (!keys) return NULL;
        int i = 0;
        for (int j = 0; j < table_size; j++) {
            gma_hash_entry<datatype_t,V> *e = table[j];
            while (e) {
                int32_t key = e->key();
                keys[i] = (int)key;
                i++;
                e = e->next();
            }
        }
        return keys;
    }
    datatype_t *keys_sorted(int size) {
        datatype_t *k = keys(size);
        if (!k) return NULL;
        qsort(k, size, sizeof(datatype_t), gma_numeric_comparison<datatype_t>);
        return k;
    }
    virtual std::vector<gma_number_t*> *keys_sorted() {
        std::vector<gma_number_t*> *retval = new std::vector<gma_number_t*>;
        int n = size();
        datatype_t *keys = keys_sorted(n);
        if (!keys) return NULL;
        for (int i = 0; i < n; i++)
            retval->push_back(new gma_number_p<datatype_t>(keys[i]));
        return retval;
    }
    virtual GDALDataType datatype() { return gma_number_p<datatype_t>::datatype_p(); };
};

// a class to divide real | integer line into intervals without holes in between
// -inf ... x(i) .. x(i+1) .. .. +inf
// each range is (x(i),x(i+1)]
template <typename datatype_t> class gma_bins_p : public gma_bins_t {
public:
    std::vector<datatype_t> *m_data;
    gma_bins_p() {
        m_data = new std::vector<datatype_t>;
    }
    ~gma_bins_p() {
        delete m_data;
    }
    int add(datatype_t x) {
        int n = m_data->size();
        if (n == 0) {
            m_data->push_back(x);
            return 0;
        }
        int i = bin(x);
        datatype_t tmp = m_data->at(n-1);
        m_data->push_back(tmp);
        for (int j = n-1; j > i; j--) {
            m_data->at(j) = m_data->at(j-1);
        }
        m_data->at(i) = x;
        return i;
    }
    datatype_t get(int i) {
        return m_data->at(i);
    }
    void set(int i, datatype_t x) {
        m_data->at(i) = x;
    }
    int bin(datatype_t x) {
        int i = 0;
        while (i < m_data->size() && x > m_data->at(i))
            i++;
        return i;
    }
    virtual gma_object_t *clone() {
        gma_bins_p<datatype_t> *c = new gma_bins_p<datatype_t>;
        for (int i = 0; i < m_data->size(); i++)
            c->m_data->push_back(m_data->at(i));
        return c;
    }
    virtual unsigned int size() {
        return m_data->size()+1;
    }
    virtual void push(int value) {
        m_data->push_back((datatype_t)value);
    }
    virtual void push(double value) {
        m_data->push_back((datatype_t)value);
    }
    virtual GDALDataType datatype() { return gma_number_p<datatype_t>::datatype_p(); };
};

template <typename datatype_t>
class gma_histogram_p : public gma_histogram_t {
    datatype_t *sorted_hash_keys;
public:
    gma_hash_p<datatype_t,gma_number_p<unsigned int> > *hash;
    gma_bins_p<datatype_t> *bins;
    std::vector<unsigned int> *counts;
    gma_histogram_p(gma_object_t *arg) {
        hash = NULL;
        sorted_hash_keys = NULL;
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
            for (int i = 0; i < n-1; i++) {
                bins->push(min+d*(i+1));
            }
            for (int i = 0; i < n; i++) {
                counts->push_back(0);
            }
        } else if (arg->get_class() == gma_bins) {
            bins = (gma_bins_p<datatype_t>*)arg->clone();
            counts = new std::vector<unsigned int>;
            for (int i = 0; i < bins->size(); i++)
                counts->push_back(0);
        } else {
            // error
        }
    }
    ~gma_histogram_p() {
        delete hash;
        CPLFree(sorted_hash_keys);
    }
    void set_size(int size, datatype_t min, datatype_t max) {
    }
    virtual unsigned int size() {
        if (hash)
            return hash->size();
        return bins->size();
    }
    void sort_hash_keys() {
        sorted_hash_keys = (datatype_t*)CPLMalloc(hash->size()*sizeof(datatype_t));
        if (!sorted_hash_keys) return;
        int i = 0;
        for (int j = 0; j < hash->table_size; j++) {
            gma_hash_entry<datatype_t,gma_number_p<unsigned int> > *e = hash->table[j];
            while (e) {
                sorted_hash_keys[i] = e->key();
                i++;
                e = e->next();
            }
        }
        qsort(sorted_hash_keys, i, sizeof(datatype_t), gma_numeric_comparison<datatype_t>);
    }
    virtual gma_object_t *at(unsigned int i) {
        if (hash) {
            if (!sorted_hash_keys) sort_hash_keys();
            if (!sorted_hash_keys) return NULL;
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
            counts->at(i)++;
        }
    }
    virtual void print() {
        for (unsigned int i = 0; i < size(); i++) {
            gma_pair_t *kv = (gma_pair_t *)at(i);
            // kv is an interval=>number or number=>number
            if (kv->first()->get_class() == gma_pair) {
                gma_pair_t *key = (gma_pair_t*)kv->first();
                gma_number_t *min = (gma_number_t*)key->first();
                gma_number_t *max = (gma_number_t*)key->second();
                gma_number_t *val = (gma_number_t*)kv->second();
                if (min->is_integer()) {
                    printf("(%i..%i] => %i\n", min->value_as_int(), max->value_as_int(), val->value_as_int());
                } else {
                    printf("(%f..%f] => %i\n", min->value_as_double(), max->value_as_double(), val->value_as_int());
                }
            } else {
                gma_number_t *key = (gma_number_t*)kv->first();
                gma_number_t *val = (gma_number_t*)kv->second();
                printf("%i => %i\n", key->value_as_int(), val->value_as_int());
            }
        }
    }
    virtual GDALDataType datatype() { return gma_number_p<datatype_t>::datatype_p(); };
};

template <typename datatype_t>
class gma_classifier_p : public gma_classifier_t {
    gma_hash_p<datatype_t,gma_number_p<int> > *m_hash;
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
        delete(m_hash);
        delete(m_bins);
        delete(m_values);
    }
    virtual void add_class(gma_number_t *interval_max, gma_number_t *value) {
        if (m_bins == NULL) {
            m_bins = new gma_bins_p<datatype_t>;
            m_values = new std::vector<datatype_t>;
            m_values->push_back(0);
        }
        int i;
        int n = m_bins->size();
        int m = m_values->size();
        datatype_t val = ((gma_number_p<datatype_t>*)value)->value();
        if (interval_max->is_inf())
            m_values->at(m-1) = val;
        else {
            i = m_bins->add(((gma_number_p<datatype_t>*)interval_max)->value());
            m_values->resize(m+1);
            for (int j = m; j > i; j--)
                m_values->at(j) = m_values->at(j-1);
            m_values->at(i) = val;
        }
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
    virtual GDALDataType datatype() {
        return gma_number_p<datatype_t>::datatype_p();
    }
    gma_logical_operation_p() {
        m_op = gma_eq;
        m_value = 0;
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
