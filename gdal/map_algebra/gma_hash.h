// simple hash table for storing mappings from integer keys to objects
// hm. Maybe not so simple and small. Use Boost?

template <typename type> class gma_number_p : public gma_number_t {
private:
    bool m_defined;
    type m_value;
public:
    gma_number_p() {
        m_defined = false;
    }
    gma_number_p(type value) {
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
    bool is_integer() {};
    bool is_float() {};
};

// wrap std::pair as concrete type gma_pair_p, 
// which is a subclass of public class gma_pair_t
// fixme: make sure K and V are subclasses of gma_object_t
template<class K, class V>class gma_pair_p : public gma_pair_t {
public:
    std::pair<K,V> m_pair;
    gma_pair_p(K key, V value) {
        m_pair.first = key;
        m_pair.second = value;
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
template <typename datatype> class gma_bins_p {
    int n_intervals;
    datatype *table;
public:
    gma_bins_p(int n_intervals) {
        table = (datatype*)CPLMalloc(sizeof(datatype)*(n_intervals-1));
    }
    ~gma_bins_p() {
        CPLFree(table);
    }
    datatype get(int i) {
        if (i >= 0 && i < n_intervals-1)
            return table[i];
    }
    void set(int i, datatype x) {
        if (i >= 0 && i < n_intervals-1)
            table[i] = x;
    }
    int bin(datatype x) {
        int i = 0;
        while (i < n_intervals-1 && x > table[i])
            i++;
        return i;
    }
};

// fixme: use std::unordered_map and std::bins
template <typename datatype>
class gma_histogram_p : public gma_histogram_t {
public:
    gma_hash_p<gma_number_p<unsigned int> > *hash;
    gma_histogram_p(gma_object_t *arg) {
        // fixme: use arg
        hash = new gma_hash_p<gma_number_p<unsigned int> >;
    }
    ~gma_histogram_p() {
        delete hash;
    }
    virtual unsigned int size() {
        return hash->size();
    }
    virtual gma_object_t *at(unsigned int i) {
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
