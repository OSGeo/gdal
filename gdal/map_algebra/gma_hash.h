// simple hash table for storing mappings from integer keys to objects
// hm. Maybe not so simple and small. Use Boost?

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
    virtual void set_value(double value) {
        m_defined = true;
        m_value = (type)value;
    }
    virtual void set_value(int value) {
        m_defined = true;
        m_value = (type)value;
    }
    virtual void set_value(unsigned int value) {
        m_defined = true;
        m_value = (type)value;
    }
    virtual int value_as_int() {
        if (m_inf < 0)
            return inf_int(-1);
        else if (m_inf > 0)
            return inf_int(1);
        else
            return (int)m_value;
    }
    virtual double value_as_double() {
        if (m_inf < 0)
            return inf_double(-1);
        else if (m_inf > 0)
            return inf_double(1);
        else
            return (double)m_value;
    }
    void add(type value) {
        m_value += value;
    }
    char *as_string() {
        // use type traits GDALDataTypeTraits<type>::is_integer etc
        char *s = (char*)CPLMalloc(10);
        snprintf(s, 10, "%i", m_value);
        return s;
    }
    GDALDataType get_datatype();
    virtual void set_inf(int inf) { m_inf = inf; };
    virtual bool is_inf() { return m_inf; };
    virtual bool is_integer();
    virtual bool is_float();
    virtual int inf_int(int sign);
    virtual double inf_double(int sign);
};

// wrap std::pair as concrete type gma_pair_p, 
// which is a subclass of public class gma_pair_t
// fixme: make sure K and V are subclasses of gma_object_t
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

template <typename datatype> class gma_cell_p : public gma_cell_t {
private:
    int m_x;
    int m_y;
    datatype m_value;
public:
    gma_cell_p(int x, int y, datatype value) {
        m_x = x;
        m_y = y;
        m_value = value;
    }
    virtual int x() {
        return m_x;
    }
    virtual int y() {
        return m_y;
    }
    virtual void set_value(double value) {
        m_value = value;
    }
    virtual void set_value(int value) {
        m_value = value;
    }
    virtual datatype value() {
        return m_value;
    }
    virtual int value_as_int() {
        return (int)m_value;
    }
    virtual double value_as_double() {
        return (double)m_value;
    }
};

// integer datatype => object
template <typename datatype, class V> class gma_hash_entry {
private:
    datatype m_key;
    V *m_value; // '*' to be able to delete values - could we somehow detect if m_value is pointer? std::is_pointer
    gma_hash_entry<datatype,V> *m_next;
public:
    gma_hash_entry(datatype key, V *value) {
        m_key = key;
        m_value = value;
        m_next = NULL;
    }
    ~gma_hash_entry() {
        delete m_value;
    }
    datatype key() {
        return m_key;
    }
    V *value() {
        return m_value;
    }
    void set_value(V *value) {
        if (m_value) delete m_value;
        m_value = value;
    }
    gma_hash_entry<datatype,V> *next() {
        return m_next;
    }
    void set_next(gma_hash_entry<datatype,V> *next) {
        m_next = next;
    }
};

template <typename datatype> int compare_integers(const void *a, const void *b) {
  const datatype *da = (const datatype *) a;
  const datatype *db = (const datatype *) b;
  return (*da > *db) - (*da < *db);
}

// replace with std::unordered_map?
// unordered_map is C++11 thing
// map.. seems to get a bit unnecessarily complicated
template <typename datatype, class V> class gma_hash_p : public gma_hash_t {
public:
    gma_hash_entry<datatype,V> **table;
    static const int table_size = 128;
    gma_hash_p() {
        table = (gma_hash_entry<datatype,V> **)CPLMalloc(sizeof(void*)*table_size);
        for (int i = 0; i < table_size; i++)
            table[i] = NULL;
    }
    ~gma_hash_p() {
        for (int i = 0; i < table_size; i++) {
            gma_hash_entry<datatype,V> *e = table[i];
            while (e) {
                gma_hash_entry<datatype,V> *n = e->next();
                delete e;
                e = n;
            }
        }
        CPLFree(table);
    }
    int exists(datatype key) {
        int hash = (abs(key) % table_size);
        gma_hash_entry<datatype,V> *e = table[hash];
        while (e && e->key() != key)
            e = e->next();
        return e != NULL;
    }
    void del(datatype key) {
        int hash = (abs(key) % table_size);
        gma_hash_entry<datatype,V> *e = table[hash], *p = NULL;
        while (e && e->key() != key) {
            p = e;
            e = e->next();
        }
        if (e) {
            gma_hash_entry<datatype,V> *n = e->next();
            delete e;
            if (p)
                p->set_next(n);
            else
                table[hash] = NULL;
        }
    }
    V *get(datatype key) {
        int hash = (abs(key) % table_size);
        gma_hash_entry<datatype,V> *e = table[hash];
        while (e && e->key() != key)
            e = e->next();
        if (e)
            return e->value();
    }
    virtual gma_object_t *get(gma_number_t *key) {
        datatype k = ((gma_number_p<datatype>*)key)->value();
        return get(k);
    }
    void put(datatype key, V *value) {
        int hash = (abs(key) % table_size);
        gma_hash_entry<datatype,V> *e = table[hash], *p = NULL;
        while (e && e->key() != key) {
            p = e;
            e = e->next();
        }
        if (e)
            e->set_value(value);
        else {
            e = new gma_hash_entry<datatype,V>(key, value);
            if (p)
                p->set_next(e);
            else
                table[hash] = e;
        }
    }
    virtual int size() {
        int n = 0;
        for (int i = 0; i < table_size; i++) {
            gma_hash_entry<datatype,V> *e = table[i];
            while (e) {
                n++;
                e = e->next();
            }
        }
        return n;
    }
    datatype *keys(int size) {
        datatype *keys = (datatype *)CPLMalloc(size*sizeof(datatype));
        int i = 0;
        for (int j = 0; j < table_size; j++) {
            gma_hash_entry<datatype,V> *e = table[j];
            while (e) {
                int32_t key = e->key();
                keys[i] = (int)key;
                i++;
                e = e->next();
            }
        }
        return keys;
    }
    datatype *keys_sorted(int size) {
        datatype *k = keys(size);
        qsort(k, size, sizeof(datatype), compare_integers<datatype>);
        return k;
    }
    virtual std::vector<gma_number_t*> *keys_sorted() {
        std::vector<gma_number_t*> *retval = new std::vector<gma_number_t*>;
        int n = size();
        datatype *keys = keys_sorted(n);
        for (int i = 0; i < n; i++)
            retval->push_back(new gma_number_p<datatype>(keys[i]));
        return retval;
    }
};

// a class to divide real | integer line into intervals without holes in between
// -inf ... x(i) .. x(i+1) .. .. +inf
// each range is (x(i),x(i+1)]
template <typename datatype> class gma_bins_p : public gma_bins_t {
public:
    std::vector<datatype> *m_data;
    gma_bins_p() {
        m_data = new std::vector<datatype>;
    }
    ~gma_bins_p() {
        delete m_data;
    }
    int add(datatype x) {
        int n = m_data->size();
        if (n == 0) {
            m_data->push_back(x);
            return 0;
        }
        int i = bin(x);
        datatype tmp = m_data->at(n-1);
        m_data->push_back(tmp);
        for (int j = n-1; j > i; j--) {
            m_data->at(j) = m_data->at(j-1);
        }
        m_data->at(i) = x;
        return i;
    }
    datatype get(int i) {
        // return pair?
        return m_data->at(i);
    }
    void set(int i, datatype x) {
        m_data->at(i) = x;
    }
    int bin(datatype x) {
        int i = 0;
        while (i < m_data->size() && x > m_data->at(i))
            i++;
        return i;
    }
    virtual gma_object_t *clone() {
        gma_bins_p<datatype> *c = new gma_bins_p<datatype>;
        for (int i = 0; i < m_data->size(); i++)
            c->m_data->push_back(m_data->at(i));
        return c;
    }
    virtual unsigned int size() {
        return m_data->size()+1;
    }
    virtual void push(int value) {
        m_data->push_back((datatype)value);
    }
    virtual void push(double value) {
        m_data->push_back((datatype)value);
    }
};

// fixme: use std::unordered_map and std::bins
template <typename datatype>
class gma_histogram_p : public gma_histogram_t {
    datatype *sorted_hash_keys;
public:
    gma_hash_p<datatype,gma_number_p<unsigned int> > *hash;
    gma_bins_p<datatype> *bins;
    std::vector<unsigned int> *counts;
    gma_histogram_p(gma_object_t *arg) {
        // fixme: use arg = null, integer in gma_number_p<int>, or gma_bins_p<datatype>
        hash = NULL;
        sorted_hash_keys = NULL;
        bins = NULL;
        counts = NULL;
        if (arg == NULL)
            hash = new gma_hash_p<datatype,gma_number_p<unsigned int> >;
        else if (arg->get_class() == gma_pair) { // (n bins, (min, max))
            gma_pair_p<gma_object_t*,gma_object_t* > *a = (gma_pair_p<gma_object_t*,gma_object_t* > *)arg;
            gma_number_p<int> *a1 = (gma_number_p<int> *)a->first();
            gma_pair_p<gma_number_p<datatype>*,gma_number_p<datatype>*> *a2 = 
                (gma_pair_p<gma_number_p<datatype>*,gma_number_p<datatype>*> *)a->second();
            gma_number_p<datatype> *a21 = (gma_number_p<datatype> *)a2->first();
            gma_number_p<datatype> *a22 = (gma_number_p<datatype> *)a2->second();
            int n = a1->value();
            double min = a21->value();
            double max = a22->value();
            double d = (max-min)/n;
            bins = new gma_bins_p<datatype>;
            counts = new std::vector<unsigned int>;
            for (int i = 0; i < n-1; i++) {
                bins->push(min+d*(i+1));
            }
            for (int i = 0; i < n; i++) {
                counts->push_back(0);
            }
        } else if (arg->get_class() == gma_bins) {
            bins = (gma_bins_p<datatype>*)arg->clone();
            counts = new std::vector<unsigned int>;
            for (int i = 0; i < bins->size(); i++)
                counts->push_back(0);
        } else {
            // error
        }
    }
    ~gma_histogram_p() {
        delete hash;
    }
    void set_size(int size, datatype min, datatype max) {
    }
    virtual unsigned int size() {
        if (hash)
            return hash->size();
        return bins->size();
    }
    void sort_hash_keys() {
        sorted_hash_keys = (datatype*)CPLMalloc(hash->size()*sizeof(datatype));
        int i = 0;
        for (int j = 0; j < hash->table_size; j++) {
            gma_hash_entry<datatype,gma_number_p<unsigned int> > *e = hash->table[j];
            while (e) {
                sorted_hash_keys[i] = e->key();
                i++;
                e = e->next();
            }
        }
        qsort(sorted_hash_keys, i, sizeof(datatype), compare_integers<datatype>);
    }
    virtual gma_object_t *at(unsigned int i) {
        if (hash) {
            if (!sorted_hash_keys) sort_hash_keys();
            datatype key = sorted_hash_keys[i];
            gma_number_p<unsigned int> *value = (gma_number_p<unsigned int>*)hash->get(key);
            // return gma_pair_t
            gma_number_p<datatype> *k = new gma_number_p<datatype>(key);
            gma_number_p<unsigned int> *v = new gma_number_p<unsigned int>(value->value());
            gma_pair_p<gma_number_p<datatype>*,gma_number_p<unsigned int>* > *pair = 
                new gma_pair_p<gma_number_p<datatype>*,gma_number_p<unsigned int>* >(k,v);
            return pair;
        }
        gma_number_p<datatype> *min;
        gma_number_p<datatype> *max;
        if (i == 0) {
            min = new gma_number_p<datatype>(0);
            min->set_inf(-1);
            max = new gma_number_p<datatype>(bins->get(i));
        } else if (i < bins->size() - 1) {
            min = new gma_number_p<datatype>(bins->get(i-1));
            max = new gma_number_p<datatype>(bins->get(i));
        } else {
            min = new gma_number_p<datatype>(bins->get(i-1));
            max = new gma_number_p<datatype>(0);
            max->set_inf(1);
        }
        gma_pair_p<gma_number_p<datatype>*,gma_number_p<datatype>* > *k = 
            new gma_pair_p<gma_number_p<datatype>*,gma_number_p<datatype>* >(min,max);
        gma_number_p<unsigned int> *v = new gma_number_p<unsigned int>(counts->at(i));
        gma_pair_p<gma_pair_p<gma_number_p<datatype>*,gma_number_p<datatype>* >*,gma_number_p<unsigned int>* > *pair = 
            new gma_pair_p<gma_pair_p<gma_number_p<datatype>*,gma_number_p<datatype>* >*,gma_number_p<unsigned int>* >(k,v);
        return pair;
    }
    void increase_count_at(datatype value) {
        if (hash) {
            if (hash->exists(value))
                ((gma_number_p<unsigned int>*)hash->get(value))->add(1);
            else
                hash->put(value, new gma_number_p<unsigned int>(1));
        } else {
            int i = bins->bin(value);
            counts->at(i)++;
        }
    }

    // fixme: remove these
    virtual int exists(int32_t key) {
        return hash->exists(key);
    }
    gma_number_p<unsigned int> *get(int32_t key) {
        return (gma_number_p<unsigned int> *)hash->get(key);
    }
    void put(int32_t key, gma_number_p<unsigned int> *value) {
        hash->put(key, value);
    }
};

template <typename datatype>
class gma_classifier_p : public gma_classifier_t {
private:
    // fixme:
    // range => numeric or (for int types) int => int
    // also default for the latter
    gma_hash_p<datatype,gma_number_p<int> > *m_hash;
    gma_bins_p<datatype> *m_bins;
    std::vector<datatype> *m_values;
    bool m_hash_ok;
    bool has_default;
    datatype deflt;
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
            m_bins = new gma_bins_p<datatype>;
            m_values = new std::vector<datatype>;
            m_values->push_back(0);
        }
        int i;
        int n = m_bins->size();
        int m = m_values->size();
        datatype val = ((gma_number_p<datatype>*)value)->value();
        if (interval_max->is_inf())
            m_values->at(m-1) = val;
        else {
            i = m_bins->add(((gma_number_p<datatype>*)interval_max)->value());
            m_values->resize(m+1);
            for (int j = m; j > i; j--)
                m_values->at(j) = m_values->at(j-1);
            m_values->at(i) = val;
        }
    }
    datatype classify(datatype value) {
        if (m_hash && m_hash->exists(value)) {
            return m_hash->get(value)->value();
        } else if (m_bins) {
            return m_values->at(m_bins->bin(value));
        } else if (has_default) {
            return deflt;
        } else
            return value;
    }
};

template <typename datatype>
class gma_logical_operation_p : public gma_logical_operation_t {
public:
    gma_operator_t m_op;
    datatype m_value;
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
    virtual void set_callback(gma_cell_callback_f callback) {
        m_callback = callback;
    }
};
