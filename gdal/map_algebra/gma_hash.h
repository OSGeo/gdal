// simple hash table for storing mappings from integer keys to objects

class gma_int {
private:
    int32_t m_value;
public:
    gma_int(int32_t value) {
        m_value = value;
    }
    int32_t value() {
        return m_value;
    }
    void add(int32_t value) {
        m_value += value;
    }
    char *as_string() {
        char *s = (char*)CPLMalloc(10);
        snprintf(s, 10, "%i", m_value);
        return s;
    }
};

template <typename type>
int my_xy_snprintf(char *s, type x, type y) {
    return snprintf(s, 20, "%i,%i", x, y);
}

template <>
int my_xy_snprintf<float>(char *s, float x, float y) {
    return snprintf(s, 40, "%f,%f", x, y);
}

template <>
int my_xy_snprintf<double>(char *s, double x, double y) {
    return snprintf(s, 40, "%f,%f", x, y);
}

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

template <typename datatype> class gma_cell {
private:
    int m_x;
    int m_y;
    datatype m_value;
public:
    gma_cell(int x, int y, datatype value) {
        m_x = x;
        m_y = y;
        m_value = value;
    }
    int x() {
        return m_x;
    }
    int y() {
        return m_y;
    }
    datatype value() {
        return m_value;
    }
};

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

int compare_int32s(const void *a, const void *b)
{
  const int32_t *da = (const int32_t *) a;
  const int32_t *db = (const int32_t *) b;
  return (*da > *db) - (*da < *db);
}

const int TABLE_SIZE = 128;

template <class V> class gma_hash {
private:
    gma_hash_entry<V> **table;
public:
    gma_hash() {
        table = (gma_hash_entry<V> **)CPLMalloc(sizeof(void*)*TABLE_SIZE);
        for (int i = 0; i < TABLE_SIZE; i++)
            table[i] = NULL;
    }
    ~gma_hash() {
        for (int i = 0; i < TABLE_SIZE; i++) {
            gma_hash_entry<V> *e = table[i];
            while (e) {
                gma_hash_entry<V> *n = e->next();
                delete e;
                e = n;
            }
        }
        CPLFree(table);
    }
    int exists(int32_t key) {
        int hash = (abs(key) % TABLE_SIZE);
        gma_hash_entry<V> *e = table[hash];
        while (e && e->key() != key)
            e = e->next();
        return e != NULL;
    }
    void del(int32_t key) {
        int hash = (abs(key) % TABLE_SIZE);
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
    V *get(int32_t key) {
        int hash = (abs(key) % TABLE_SIZE);
        gma_hash_entry<V> *e = table[hash];
        while (e && e->key() != key)
            e = e->next();
        if (e)
            return e->value();
    }
    void put(int32_t key, V *value) {
        int hash = (abs(key) % TABLE_SIZE);
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
    int size() {
        int n = 0;
        for (int i = 0; i < TABLE_SIZE; i++) {
            gma_hash_entry<V> *e = table[i];
            while (e) {
                n++;
                e = e->next();
            }
        }
        return n;
    }
    int32_t *keys(int size) {
        int32_t *keys = (int *)CPLMalloc(size*sizeof(int32_t));
        int i = 0;
        for (int j = 0; j < TABLE_SIZE; j++) {
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
    int32_t *keys_sorted(int size) {
        int32_t *k = keys(size);
        qsort(k, size, sizeof(int32_t), compare_int32s);
        return k;
    }
};

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
