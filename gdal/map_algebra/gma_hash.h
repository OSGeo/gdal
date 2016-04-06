// simple hash table for storing mappings from integer keys to objects
// hm. Maybe not so simple and small. Use Boost?

template <typename type> class gma_number_p : public gma_number_t {
private:
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

template <typename type> class gma_xy {
private:
    type m_x;
    type m_y;
public:
    gma_xy(type x, type y) {
        m_x = x;
        m_y = y;
    }
    type x() {
        return m_x;
    }
    type y() {
        return m_y;
    }
    char *as_string() {
        char *s = (char*)CPLMalloc(20);
        my_xy_snprintf(s, m_x, m_y);
        return s;
    }
};

template <typename datatype> class gma_cell_p : gma_cell_t {
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

// int32_t => object
template <class V> class gma_hash_entry {
private:
    int32_t m_key;
    V *m_value;
    gma_hash_entry<V> *m_next;
public:
    gma_hash_entry(int32_t key, V *value) {
        m_key = key;
        m_value = value;
        m_next = NULL;
    }
    ~gma_hash_entry() {
        delete m_value;
    }
    int32_t key() {
        return m_key;
    }
    V *value() {
        return m_value;
    }
    void set_value(V *value) {
        if (m_value) delete m_value;
        m_value = value;
    }
    gma_hash_entry<V> *next() {
        return m_next;
    }
    void set_next(gma_hash_entry<V> *next) {
        m_next = next;
    }
};

int compare_int32s(const void *a, const void *b);

class gma_hash_t : public gma_object_t {
public:
    virtual int exists(int32_t key) {
    }
    virtual gma_object_t *get(int32_t key) {
    }
    virtual int size() {
    }
    virtual int32_t *keys_sorted(int size) {
    }
};

// replace with std::unordered_map?
template <class V> class gma_hash_p : public gma_hash_t {
public:
    gma_hash_entry<V> **table;
    static const int table_size = 128;
    gma_hash_p() {
        table = (gma_hash_entry<V> **)CPLMalloc(sizeof(void*)*table_size);
        for (int i = 0; i < table_size; i++)
            table[i] = NULL;
    }
    ~gma_hash_p() {
        for (int i = 0; i < table_size; i++) {
            gma_hash_entry<V> *e = table[i];
            while (e) {
                gma_hash_entry<V> *n = e->next();
                delete e;
                e = n;
            }
        }
        CPLFree(table);
    }
    virtual int exists(int32_t key) {
        int hash = (abs(key) % table_size);
        gma_hash_entry<V> *e = table[hash];
        while (e && e->key() != key)
            e = e->next();
        return e != NULL;
    }
    void del(int32_t key) {
        int hash = (abs(key) % table_size);
        gma_hash_entry<V> *e = table[hash], *p = NULL;
        while (e && e->key() != key) {
            p = e;
            e = e->next();
        }
        if (e) {
            gma_hash_entry<V> *n = e->next();
            delete e;
            if (p)
                p->set_next(n);
            else
                table[hash] = NULL;
        }
    }
    gma_object_t *get(int32_t key) {
        int hash = (abs(key) % table_size);
        gma_hash_entry<V> *e = table[hash];
        while (e && e->key() != key)
            e = e->next();
        if (e)
            return (gma_object_t *)e->value();
    }
    void put(int32_t key, V *value) {
        int hash = (abs(key) % table_size);
        gma_hash_entry<V> *e = table[hash], *p = NULL;
        while (e && e->key() != key) {
            p = e;
            e = e->next();
        }
        if (e)
            e->set_value(value);
        else {
            e = new gma_hash_entry<V>(key, value);
            if (p)
                p->set_next(e);
            else
                table[hash] = e;
        }
    }
    virtual int size() {
        int n = 0;
        for (int i = 0; i < table_size; i++) {
            gma_hash_entry<V> *e = table[i];
            while (e) {
                n++;
                e = e->next();
            }
        }
        return n;
    }
    virtual int32_t *keys(int size) {
        int32_t *keys = (int *)CPLMalloc(size*sizeof(int32_t));
        int i = 0;
        for (int j = 0; j < table_size; j++) {
            gma_hash_entry<V> *e = table[j];
            while (e) {
                int32_t key = e->key();
                keys[i] = (int)key;
                i++;
                e = e->next();
            }
        }
        return keys;
    }
    virtual int32_t *keys_sorted(int size) {
        int32_t *k = keys(size);
        qsort(k, size, sizeof(int32_t), compare_int32s);
        return k;
    }
};

// replace with std::vector
template <class V> class gma_array {
private:
    int table_size;
    int m_size;
    V **table;
    void resize(int i) {
        int old = table_size;
        table_size = i;
        table = (V **)CPLRealloc(table, sizeof(V*)*table_size);
        for (int i = old; i < table_size; i++)
            table[i] = NULL;
    }
public:
    gma_array() {
        table_size = 100;
        table = (V **)CPLMalloc(sizeof(V*)*table_size);
        for (int i = 0; i < table_size; i++)
            table[i] = NULL;
        m_size = 0;
    }
    ~gma_array() {
        for (int i = 0; i < table_size; i++)
            if (table[i]) delete table[i];
        CPLFree(table);
    }
    int size() {
        return m_size;
    }
    V *get(int i) {
        if (i < 0 || i >= table_size)
            return NULL;
        else
            return table[i];
    }
    void set(int i, V *item) {
        if (i >= table_size) resize(i+100);
        if (i >= 0 && i < table_size) {
            if (table[i])
                delete table[i];
            table[i] = item;
            m_size = i+1;
        }
    }
    void push(V *item) {
        if (m_size >= table_size) resize(table_size+100);
        table[m_size] = item;
        m_size++;
    }
    V *pop() {
        if (m_size <= 0)
            return NULL;
        m_size--;
        return table[m_size];
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
public:
    gma_hash_p<gma_number_p<unsigned int> > *hash;
    gma_bins_p<datatype> *bins;
    std::vector<unsigned int> *counts;
    gma_histogram_p(gma_object_t *arg) {
        // fixme: use arg = null, integer in gma_number_p<int>, or gma_bins_p<datatype>
        hash = NULL;
        bins = NULL;
        counts = NULL;
        if (arg == NULL)
            hash = new gma_hash_p<gma_number_p<unsigned int> >;
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
    virtual gma_object_t *at(unsigned int i) {
        if (hash) {
            int n = 0;
            for (int j = 0; j < hash->table_size; j++) {
                gma_hash_entry<gma_number_p<unsigned int> > *e = hash->table[j];
                while (e) {
                    if (n == i) {
                        // return gma_pair_t
                        gma_number_p<datatype> *k = new gma_number_p<datatype>(e->key());
                        gma_number_p<unsigned int> *v = new gma_number_p<unsigned int>(e->value()->value());
                        gma_pair_p<gma_number_p<datatype>*,gma_number_p<unsigned int>* > *pair = 
                            new gma_pair_p<gma_number_p<datatype>*,gma_number_p<unsigned int>* >(k,v);
                        return pair;
                    }
                    n++;
                    e = e->next();
                }
            }
            return NULL;
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
class gma_mapper_p : public gma_object_t {
private:
    // fixme:
    // range => numeric or (for int types) int => int
    // also default for the latter
    gma_hash_p<gma_number_p<datatype> > *m_mapper;
public:
    gma_mapper_p(gma_hash_p<gma_number_p<datatype> > *mapper) {
        m_mapper = mapper;
    }
    int map(datatype *value) {
        if (m_mapper->exists(*value)) {
            *value = ((gma_number_p<datatype>*)m_mapper->get(*value))->value();
            return 1;
        } else
            return 0;
    }
};
