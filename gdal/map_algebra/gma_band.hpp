#include "gdal_map_algebra.hpp"
#include <cstdlib>
#include <cmath>

typedef struct {
    int x;
    int y;
} gma_block_index; // block coordinates

// cell coordinates in block or globally
class gma_cell_index {
public:
    int x;
    int y;
    inline gma_cell_index() {
        x = 0;
        y = 0;
    }
    inline gma_cell_index(int init_x, int init_y) {
        x = init_x;
        y = init_y;
    }
    inline gma_cell_index first_neighbor() {
        return gma_cell_index(x, y-1);
    }
    inline void move_to_neighbor(int neighbor) {
        switch(neighbor) {
        case 2: x++; break;
        case 3: y++; break;
        case 4: y++; break;
        case 5: x--; break;
        case 6: x--; break;
        case 7: y--; break;
        case 8: y--; break;
        }
    }
};

template <typename datatype> class gma_block {
    void *block;
public:
    gma_block_index m_index;
    int m_w; // width of data in block
    int m_h; // height of data in block
    gma_block(gma_block_index index, int w, int h, GDALRasterBand *band, int w_block, int h_block) {
        m_index = index;
        m_w = w;
        m_h = h;
        block = CPLMalloc(w_block * h_block * sizeof(datatype));
        if (block) {
            // constructor can't return a value and ReadBlock calls CPLError ice error
            if (band->ReadBlock(m_index.x, m_index.y, block) != CE_None);
        }
    }
    ~gma_block() {
        CPLFree(block);
    }
    inline int w() { return m_w; }
    inline int h() { return m_h; }
    inline datatype& cell(gma_cell_index i) {
        return ((datatype*)block)[i.x+i.y*m_w];
    }
    inline void cell(gma_cell_index i, datatype value) {
        ((datatype*)block)[i.x+i.y*m_w] = value;
    }
    void write(GDALRasterBand *band) {
        if (band->WriteBlock(m_index.x, m_index.y, block) != CE_None); // WriteBlock calls CPLError ice error
    }
    int is_border_cell(int border_block, gma_cell_index i) {
        if (!border_block)
            return 0;
        if (i.x == 0) {
            if (i.y == 0 && border_block == 1)
                return 8;
            else if (i.y == m_h - 1 && border_block == 6)
                return 6;
            else if (border_block == 8 || border_block == 6 || border_block == 7)
                return 7;
        } else if (i.x == m_w - 1) {
            if (i.y == 0 && border_block == 2)
                return 2;
            else if (i.y == m_h - 1 && border_block == 4)
                return 4;
            else if (border_block == 2 || border_block == 4 || border_block == 3)
                return 3;
        } else if (i.y == 0 && (border_block == 8 || border_block == 2 || border_block == 1))
            return 1;
        else if (i.y == m_h - 1 && (border_block == 6 || border_block == 4 || border_block == 5))
            return 5;
        return 0;
    }
    inline bool first_block() {
        return m_index.x == 0 && m_index.y == 0;
    }
};

template <typename datatype> class gma_block_cache {
    size_t m_n;
    gma_block<datatype> **m_blocks;
public:
    gma_block_cache() {
        m_n = 0;
        m_blocks = NULL;
    }
    inline size_t size() {
        return m_n;
    }
    void empty() {
        if (!m_blocks) return;
        if (m_n == 0)
            return;
        for (int i = 0; i < m_n; i++)
            delete m_blocks[i];
        CPLFree(m_blocks);
        m_n = 0;
        m_blocks = NULL;
    }
    void remove(int i) {
        if (!m_blocks || i < 0 || i >= m_n) return;
        gma_block<datatype> **blocks = (gma_block<datatype>**)CPLMalloc((m_n-1) * sizeof(gma_block<datatype>*));
        if (!blocks) return;
        int d = 0;
        for (int j = 0; j < m_n; j++) {
            if (j == i) {
                delete m_blocks[j];
                d = 1;
            } else {
                blocks[j-d] = m_blocks[j];
            }
        }
        CPLFree(m_blocks);
        m_n--;
        m_blocks = blocks;
    }
    gma_block<datatype> *retrieve(gma_block_index index) {
        if (!m_blocks) return NULL;
        for (int i = 0; i < m_n; i++)
            if (m_blocks[i]->m_index.x == index.x && m_blocks[i]->m_index.y == index.y)
                return m_blocks[i];
        return NULL;
    }
    void add(gma_block<datatype> *block) {
        if (!m_blocks && m_n != 0) return;
        m_n++;
        if (m_n == 1)
            m_blocks = (gma_block<datatype>**)CPLMalloc(sizeof(gma_block<datatype>*));
        else
            m_blocks = (gma_block<datatype>**)CPLRealloc(m_blocks, m_n * sizeof(gma_block<datatype>*));
        if (m_blocks) m_blocks[m_n-1] = block;
    }
    void remove(gma_block_index i20, gma_block_index i21) {
        if (!m_blocks) return;
        int i = 0;
        while (i < m_n) {
            if (m_blocks[i]->m_index.x < i20.x || m_blocks[i]->m_index.x > i21.x ||
                m_blocks[i]->m_index.y < i20.y || m_blocks[i]->m_index.y > i21.y)
                remove(i);
            else
                i++;
        }
    }
};

//
// gma_band_p class
//

#define COMMA ,
#define GMA_RETVAL_INIT(class, var, arg)        \
    class *var;                                 \
    if (*retval == NULL) {                      \
        var = new class(arg);                   \
        *retval = (gma_object_t*)var;           \
    } else                                      \
        var = (class *)*retval;

class gma_two_bands_t {
public:
    virtual void set_progress_fct(GDALProgressFunc progress, void * progress_arg) {};

    virtual void assign(gma_band_t *, gma_band_t *, gma_logical_operation_t *op = NULL) {};
    virtual void add(gma_band_t *, gma_band_t *, gma_logical_operation_t *op = NULL) {};
    virtual void subtract(gma_band_t *, gma_band_t *, gma_logical_operation_t *op = NULL) {};
    virtual void multiply(gma_band_t *, gma_band_t *, gma_logical_operation_t *op = NULL) {};
    virtual void divide(gma_band_t *, gma_band_t *, gma_logical_operation_t *op = NULL) {};
    virtual void modulus(gma_band_t *, gma_band_t *, gma_logical_operation_t *op = NULL) {};

    virtual void decision(gma_band_t *a, gma_band_t *b, gma_band_t *c) {};

    virtual gma_hash_t *zonal_min(gma_band_t *, gma_band_t *zones) {return NULL;};
    virtual gma_hash_t *zonal_max(gma_band_t *, gma_band_t *zones) {return NULL;};

    virtual void rim_by8(gma_band_t *rims, gma_band_t *zones) {};

    virtual void fill_depressions(gma_band_t *filled_dem, gma_band_t *dem) {};
    virtual void D8(gma_band_t *fd, gma_band_t *dem) {};
    virtual void route_flats(gma_band_t *fd, gma_band_t *dem) {};
    virtual void upstream_area(gma_band_t *ua, gma_band_t *fd) {};
    virtual void catchment(gma_band_t *catchment, gma_band_t *fd, gma_cell_t *outlet) {};
};
gma_two_bands_t *gma_new_two_bands(GDALDataType type1, GDALDataType type2);

template <typename datatype_t> class gma_band_p : public gma_band_t {
    GDALRasterBand *m_band;
    int m_w;
    int m_h;
    int m_w_block;
    int m_h_block;
    GDALDataType m_gdal_datatype;
    int datatype_size;
    gma_block_cache<datatype_t> cache;
    datatype_t m_nodata;
    bool m_has_nodata;
    gma_band_p<uint8_t> *mask;
    GDALProgressFunc m_progress;
    void * m_progress_arg;

public:
    int w_blocks;
    int h_blocks;
    gma_band_p(GDALRasterBand *b) {
        m_band = b;
        m_w = b->GetXSize();
        m_h = b->GetYSize();
        b->GetBlockSize(&m_w_block, &m_h_block);
        w_blocks = (m_w + m_w_block - 1) / m_w_block;
        h_blocks = (m_h + m_h_block - 1) / m_h_block;
        m_gdal_datatype = b->GetRasterDataType();
        int has_nodata;
        double nodata = b->GetNoDataValue(&has_nodata);
        m_has_nodata = has_nodata != 0;
        m_nodata = has_nodata ? (datatype_t)nodata : 0;
        int mask_flags = b->GetMaskFlags();
        mask = NULL;
        if (mask_flags & GMF_PER_DATASET || mask_flags & GMF_ALPHA) {
            GDALRasterBand *m = b->GetMaskBand();
            if (m) mask = new gma_band_p<uint8_t>(m);
        }
        datatype_size = sizeof(datatype_t);
        m_progress = NULL;
        m_progress_arg = NULL;
    }
    ~gma_band_p() {
        delete(mask);
    }
    virtual void update() {
        int has_nodata;
        double nodata = m_band->GetNoDataValue(&has_nodata);
        m_has_nodata = has_nodata != 0;
        m_nodata = has_nodata ? (datatype_t)nodata : 0;
        int mask_flags = m_band->GetMaskFlags();
        delete(mask);
        if (mask_flags & GMF_PER_DATASET || mask_flags & GMF_ALPHA) {
            GDALRasterBand *m = m_band->GetMaskBand();
            if (m) mask = new gma_band_p<uint8_t>(m);
        }
    }
    virtual GDALRasterBand *band() {
        return m_band;
    }
    virtual GDALDataset *dataset() {
        return m_band->GetDataset();
    }
    virtual GDALDriver *driver() {
        return m_band->GetDataset()->GetDriver();
    }
    virtual GDALDataType datatype() {
        return m_gdal_datatype;
    }
    bool datatype_is_integer() {
        gma_number_p<datatype_t> n;
        return n.is_integer();
    }
    bool datatype_is_float() {
        gma_number_p<datatype_t> n;
        return n.is_float();
    }
    virtual int w() {
        return m_w;
    }
    virtual int h() {
        return m_h;
    }
    virtual void set_progress_fct(GDALProgressFunc progress, void * progress_arg) {
        m_progress = progress;
        m_progress_arg = progress_arg;
    }
    int w_block() {
        return m_w_block;
    }
    int h_block() {
        return m_h_block;
    }
    void empty_cache() {
        cache.empty();
        if (mask) mask->empty_cache();
    }
    gma_block<datatype_t> *get_block(gma_block_index i) {
        return cache.retrieve(i);
    }
    void write_block(gma_block<datatype_t> *block) {
        if (!block) return;
        block->write(m_band);
    }
    void add_to_cache(gma_block_index i) {
        gma_block<datatype_t> *b = cache.retrieve(i);
        if (!b) {
            int w = ( (i.x+1) * m_w_block > m_w ) ? m_w - i.x * m_w_block : m_w_block;
            int h = ( (i.y+1) * m_h_block > m_h ) ? m_h - i.y * m_h_block : m_h_block;
            b = new gma_block<datatype_t>(i, w, h, m_band, m_w_block, m_h_block);
            cache.add(b);
        }
    }
    template <typename type1> void update_cache(gma_band_p<type1> *band1, gma_block<type1> *b1, int d) {

        // which blocks in this band are needed to cover a block extended with focal distance d in band 1?
        // assuming the raster size is the same

        gma_block_index i20, i21;

        // index of top left cell to be covered
        int x10 = b1->m_index.x * band1->w_block() - d, y10 = b1->m_index.y * band1->h_block() - d;

        // index of bottom right cell to be covered
        int x11 = x10 + d + b1->w()-1 + d, y11 = y10 + d + b1->h()-1 + d;

        // which block covers x10, y10 in band2?
        i20.x = MAX(x10 / m_w_block, 0);
        i20.y = MAX(y10 / m_h_block, 0);

        // which block covers x11, y11 in band2?
        i21.x = MIN(x11 / m_w_block, w_blocks-1);
        i21.y = MIN(y11 / m_h_block, h_blocks-1);

        // add needed blocks
        gma_block_index i;
        for (i.y = i20.y; i.y <= i21.y; i.y++) {
            for (i.x = i20.x; i.x <= i21.x; i.x++) {
                add_to_cache(i);
            }
        }
        // remove unneeded blocks
        cache.remove(i20, i21);
        if (mask) mask->update_cache(band1, b1, d);
    }
    inline gma_cell_index global_cell_index(gma_block<datatype_t> *b, gma_cell_index i) {
        gma_cell_index ret;
        ret.x = b->m_index.x * m_w_block + i.x;
        ret.y = b->m_index.y * m_h_block + i.y;
        return ret;
    }
    inline gma_cell_index cell_index(gma_cell_index i) {
        gma_cell_index ret;
        ret.x = i.x % m_w_block;
        ret.y = i.y % m_h_block;
        return ret;
    }
    inline gma_block_index block_index(gma_cell_index gi) {
        gma_block_index i;
        i.x = gi.x / m_w_block;
        i.y = gi.y / m_h_block;
        return i;
    }
    inline bool cell_is_outside(gma_block<datatype_t> *b, gma_cell_index i) {
        // global cell index
        int x = b->m_index.x * m_w_block + i.x;
        int y = b->m_index.y * m_h_block + i.y;
        return (x < 0 || y < 0 || x >= m_w || y >= m_h);
    }
    inline bool cell_is_outside(gma_cell_index i) {
        return (i.x < 0 || i.y < 0 || i.x >= m_w || i.y >= m_h);
    }
    inline bool is_nodata(datatype_t value) {
        return m_has_nodata && value == m_nodata;
    }
    inline bool cell_is_nodata(gma_block<datatype_t> *b, gma_cell_index i) {
        if (mask) {
            // https://trac.osgeo.org/gdal/wiki/rfc15_nodatabitmask
            uint8_t mask_value = 0;
            mask->has_value(this, b, i, &mask_value); // we assume mask cache is updated
            return mask_value == 0; // strict zero means nodata
        }
        return m_has_nodata && b->cell(i) == m_nodata;
    }
    int is_border_block(gma_block<datatype_t> *block) {
        if (block->m_index.x == 0) {
            if (block->m_index.y == 0)
                return 8;
            else if (block->m_index.y == h_blocks - 1)
                return 6;
            else
                return 7;
        } else if (block->m_index.x == w_blocks - 1) {
            if (block->m_index.y == 0)
                return 2;
            else if (block->m_index.y == h_blocks - 1)
                return 4;
            else
                return 3;
        } else if (block->m_index.y == 0)
            return 1;
        else if (block->m_index.y == h_blocks - 1)
            return 5;
        return 0;
    }
    inline bool last_block(gma_block<datatype_t> *b) {
        return b->m_index.x == w_blocks-1 && b->m_index.y == h_blocks-1;
    }
    // get the block, which has the cell, that is pointed to by i2 in block2 in band2
    template <typename type2> gma_block<datatype_t> *get_block(gma_band_p<type2> *band2, gma_block<type2> *b2, gma_cell_index i2, gma_cell_index *i1) {
        gma_cell_index gi = band2->global_cell_index(b2, i2);
        if (cell_is_outside(gi)) return NULL;
        gma_block_index i = block_index(gi);
        gma_block<datatype_t> *rv = get_block(i);
        if (rv) *i1 = cell_index(gi);
        return rv;
    }
    // returns false if cell is not on the band or is nodata cell
    template<typename type2> bool has_value(gma_band_p<type2> *band2, gma_block<type2> *block2, gma_cell_index index2, datatype_t *value) {
        gma_cell_index index;
        gma_block<datatype_t> *block = get_block(band2, block2, index2, &index);
        if (block) {
            if (cell_is_nodata(block, index)) return false;
            *value = block->cell(index);
            return true;
        } else
            return false;
    }

    virtual gma_band_t *new_band(const char *name, GDALDataType datatype) {
        return gma_new_band(driver()->Create(name, w(), h(), 1, datatype, NULL)->GetRasterBand(1));
    }
    virtual gma_number_t *new_number() {
        return new gma_number_p<datatype_t>();
    }
    virtual gma_number_t *new_number(datatype_t value) {
        return new gma_number_p<datatype_t>(value);
    }
    virtual gma_number_t *new_int(int value) {
        return new gma_number_p<int>(value);
    }
    virtual gma_pair_t *new_pair() {
        return new gma_pair_p<gma_object_t*,gma_object_t* >;
    }
    virtual gma_pair_t *new_range() {
        return new gma_pair_p<gma_number_p<datatype_t>*,gma_number_p<datatype_t>* >
            (new gma_number_p<datatype_t>, new gma_number_p<datatype_t>);
    }
    virtual gma_bins_t *new_bins() {
        return new gma_bins_p<datatype_t>;
    }
    virtual gma_classifier_t *new_classifier() {
        return new gma_classifier_p<datatype_t>(true);
    }
    virtual gma_cell_t *new_cell() {
        return new gma_cell_p<datatype_t>(0, 0, 0);
    }
    virtual gma_cell_callback_t *new_cell_callback() {
        return new gma_cell_callback_p;
    }
    virtual gma_logical_operation_t *new_logical_operation() {
        return new gma_logical_operation_p<datatype_t>;
    }

    struct callback {
        typedef int (gma_band_p<datatype_t>::*type)(gma_block<datatype_t>*, gma_object_t **, gma_object_t*, int);
        type fct;
    };

    void block_loop(callback cb, gma_object_t **retval = NULL, gma_object_t *arg = NULL, int fd = 0) {
        gma_block_index i;
        for (i.y = 0; i.y < h_blocks; i.y++) {
            for (i.x = 0; i.x < w_blocks; i.x++) {
                add_to_cache(i);
                gma_block<datatype_t> *block = get_block(i);
                if (!block) return;
                update_cache(this, block, fd);
                int ret = (this->*cb.fct)(block, retval, arg, fd);
                switch (ret) {
                case 0: return;
                case 1: break;
                case 2: write_block(block);
                }
                if (CPLGetLastErrorNo() != CPLE_None) return;
            }
        }
    }

    int m_print(gma_block<datatype_t> *block, gma_object_t **, gma_object_t*, int) {
        gma_cell_index i;
        for (i.y = 0; i.y < block->h(); i.y++) {
            for (i.x = 0; i.x < block->w(); i.x++) {
                if (cell_is_nodata(block, i))
                    printf("x");
                else
                    printf(gma_number_p<datatype_t>::format(), block->cell(i));
                printf(" ");
            }
            printf("\n");
        }
        return 1;
    }

    int m_rand(gma_block<datatype_t>* block, gma_object_t **, gma_object_t*, int) {
        gma_cell_index i;
        for (i.y = 0; i.y < block->h(); i.y++) {
            for (i.x = 0; i.x < block->w(); i.x++) {
                // fixme: datatype specific value
                block->cell(i) = std::rand();
            }
        }
        return 2;
    }

    int m_abs(gma_block<datatype_t> *block, gma_object_t **, gma_object_t*, int) {
        gma_cell_index i;
        for (i.y = 0; i.y < block->h(); i.y++) {
            for (i.x = 0; i.x < block->w(); i.x++) {
                if (cell_is_nodata(block, i)) continue;
                block->cell(i) = std::abs(block->cell(i));
            }
        }
        return 2;
    }
    // fabs for floats

    int m_exp(gma_block<datatype_t> *block, gma_object_t **, gma_object_t*, int) {
        // fixme: error if not float
        gma_cell_index i;
        for (i.y = 0; i.y < block->h(); i.y++) {
            for (i.x = 0; i.x < block->w(); i.x++) {
                if (cell_is_nodata(block, i)) continue;
                block->cell(i) = std::exp(block->cell(i));
            }
        }
        return 2;
    }

    int m_log(gma_block<datatype_t> *block, gma_object_t **, gma_object_t*, int) {
        gma_cell_index i;
        for (i.y = 0; i.y < block->h(); i.y++) {
            for (i.x = 0; i.x < block->w(); i.x++) {
                if (cell_is_nodata(block, i)) continue;
                block->cell(i) = std::log(block->cell(i));
            }
        }
        return 2;
    }

    int m_log10(gma_block<datatype_t> *block, gma_object_t **, gma_object_t*, int) {
        gma_cell_index i;
        for (i.y = 0; i.y < block->h(); i.y++) {
            for (i.x = 0; i.x < block->w(); i.x++) {
                if (cell_is_nodata(block, i)) continue;
                block->cell(i) = std::log10(block->cell(i));
            }
        }
        return 2;
    }

    int m_sqrt(gma_block<datatype_t> *block, gma_object_t **, gma_object_t*, int) {
        gma_cell_index i;
        for (i.y = 0; i.y < block->h(); i.y++) {
            for (i.x = 0; i.x < block->w(); i.x++) {
                if (cell_is_nodata(block, i)) continue;
                block->cell(i) = std::sqrt(block->cell(i));
            }
        }
        return 2;
    }

    int m_sin(gma_block<datatype_t> *block, gma_object_t **, gma_object_t*, int) {
        gma_cell_index i;
        for (i.y = 0; i.y < block->h(); i.y++) {
            for (i.x = 0; i.x < block->w(); i.x++) {
                if (cell_is_nodata(block, i)) continue;
                block->cell(i) = std::sin(block->cell(i));
            }
        }
        return 2;
    }

    int m_cos(gma_block<datatype_t> *block, gma_object_t **, gma_object_t*, int) {
        gma_cell_index i;
        for (i.y = 0; i.y < block->h(); i.y++) {
            for (i.x = 0; i.x < block->w(); i.x++) {
                if (cell_is_nodata(block, i)) continue;
                block->cell(i) = std::cos(block->cell(i));
            }
        }
        return 2;
    }

    int m_tan(gma_block<datatype_t> *block, gma_object_t **, gma_object_t*, int) {
        gma_cell_index i;
        for (i.y = 0; i.y < block->h(); i.y++) {
            for (i.x = 0; i.x < block->w(); i.x++) {
                if (cell_is_nodata(block, i)) continue;
                block->cell(i) = std::tan(block->cell(i));
            }
        }
        return 2;
    }

    int m_ceil(gma_block<datatype_t> *block, gma_object_t **, gma_object_t*, int) {
        gma_cell_index i;
        for (i.y = 0; i.y < block->h(); i.y++) {
            for (i.x = 0; i.x < block->w(); i.x++) {
                if (cell_is_nodata(block, i)) continue;
                block->cell(i) = std::ceil(block->cell(i));
            }
        }
        return 2;
    }

    int m_floor(gma_block<datatype_t> *block, gma_object_t **, gma_object_t*, int) {
        gma_cell_index i;
        for (i.y = 0; i.y < block->h(); i.y++) {
            for (i.x = 0; i.x < block->w(); i.x++) {
                if (cell_is_nodata(block, i)) continue;
                block->cell(i) = std::floor(block->cell(i));
            }
        }
        return 2;
    }
    virtual void print() {
        callback cb;
        cb.fct = &gma_band_p::m_print;
        block_loop(cb);
    }
    virtual void rand() {
        callback cb;
        cb.fct = &gma_band_p::m_rand;
        block_loop(cb);
    }
    virtual void abs() {
        callback cb;
        cb.fct = &gma_band_p::m_abs;
        block_loop(cb);
    }
    virtual void exp() {
        callback cb;
        cb.fct = &gma_band_p::m_exp;
        block_loop(cb);
    }
    virtual void log() {
        callback cb;
        cb.fct = &gma_band_p::m_log;
        block_loop(cb);
    }
    virtual void log10() {
        callback cb;
        cb.fct = &gma_band_p::m_log10;
        block_loop(cb);
    }
    virtual void sqrt() {
        callback cb;
        cb.fct = &gma_band_p::m_sqrt;
        block_loop(cb);
    }
    virtual void sin() {
        callback cb;
        cb.fct = &gma_band_p::m_sin;
        block_loop(cb);
    }
    virtual void cos() {
        callback cb;
        cb.fct = &gma_band_p::m_cos;
        block_loop(cb);
    }
    virtual void tan() {
        callback cb;
        cb.fct = &gma_band_p::m_tan;
        block_loop(cb);
    }
    virtual void ceil() {
        callback cb;
        cb.fct = &gma_band_p::m_ceil;
        block_loop(cb);
    }
    virtual void floor() {
        callback cb;
        cb.fct = &gma_band_p::m_floor;
        block_loop(cb);
    }


    int m_assign(gma_block<datatype_t> *block, gma_object_t **, gma_object_t *arg, int) {
        datatype_t a = ((gma_number_p<datatype_t>*)arg)->value();
        gma_cell_index i;
        for (i.y = 0; i.y < block->h(); i.y++) {
            for (i.x = 0; i.x < block->w(); i.x++) {
                if (cell_is_nodata(block, i)) continue;
                block->cell(i) = a;
            }
        }
        return 2;
    }
    int m_assign_all(gma_block<datatype_t> *block, gma_object_t **, gma_object_t *arg, int) {
        datatype_t a = ((gma_number_p<datatype_t>*)arg)->value();
        gma_cell_index i;
        for (i.y = 0; i.y < block->h(); i.y++) {
            for (i.x = 0; i.x < block->w(); i.x++) {
                block->cell(i) = a;
            }
        }
        return 2;
    }
    int m_add(gma_block<datatype_t>* block, gma_object_t **, gma_object_t *arg, int) {
        datatype_t a = ((gma_number_p<datatype_t>*)arg)->value();
        gma_cell_index i;
        for (i.y = 0; i.y < block->h(); i.y++) {
            for (i.x = 0; i.x < block->w(); i.x++) {
                if (!cell_is_nodata(block, i))
                    block->cell(i) = MAX(MIN(block->cell(i) + a, std::numeric_limits<datatype_t>::max()), std::numeric_limits<datatype_t>::min());
            }
        }
        return 2;
    }
    int m_subtract(gma_block<datatype_t> *block, gma_object_t **, gma_object_t *arg, int) {
        datatype_t a = ((gma_number_p<datatype_t>*)arg)->value();
        gma_cell_index i;
        for (i.y = 0; i.y < block->h(); i.y++) {
            for (i.x = 0; i.x < block->w(); i.x++) {
                if (cell_is_nodata(block, i)) continue;
                block->cell(i) = MAX(MIN(block->cell(i) - a, std::numeric_limits<datatype_t>::max()), std::numeric_limits<datatype_t>::min());
            }
        }
        return 2;
    }
    int m_multiply(gma_block<datatype_t> *block, gma_object_t **, gma_object_t *arg, int) {
        datatype_t a = ((gma_number_p<datatype_t>*)arg)->value();
        gma_cell_index i;
        for (i.y = 0; i.y < block->h(); i.y++) {
            for (i.x = 0; i.x < block->w(); i.x++) {
                if (cell_is_nodata(block, i)) continue;
                block->cell(i) = MAX(MIN(block->cell(i) * a, std::numeric_limits<datatype_t>::max()), std::numeric_limits<datatype_t>::min());
            }
        }
        return 2;
    }
    int m_divide(gma_block<datatype_t> *block, gma_object_t **, gma_object_t *arg, int) {
        datatype_t a = ((gma_number_p<datatype_t>*)arg)->value();
        gma_cell_index i;
        for (i.y = 0; i.y < block->h(); i.y++) {
            for (i.x = 0; i.x < block->w(); i.x++) {
                if (cell_is_nodata(block, i)) continue;
                if (a == 0)
                    block->cell(i) = std::numeric_limits<datatype_t>::quiet_NaN();
                else
                    block->cell(i) = MAX(MIN(block->cell(i) / a, std::numeric_limits<datatype_t>::max()), std::numeric_limits<datatype_t>::min());
            }
        }
        return 2;
    }
    int m_modulus(gma_block<datatype_t> *block, gma_object_t **, gma_object_t *arg, int) {
        datatype_t a = ((gma_number_p<datatype_t>*)arg)->value();
        gma_cell_index i;
        for (i.y = 0; i.y < block->h(); i.y++) {
            for (i.x = 0; i.x < block->w(); i.x++) {
                if (cell_is_nodata(block, i)) continue;
                if (a == 0)
                    block->cell(i) = std::numeric_limits<datatype_t>::quiet_NaN();
                else
                    block->cell(i) %= a;
            }
        }
        return 2;
    }
    virtual void assign(int value) {
        callback cb;
        cb.fct = &gma_band_p::m_assign;
        datatype_t v = MAX(MIN(value, std::numeric_limits<datatype_t>::max()), std::numeric_limits<datatype_t>::min());
        gma_number_p<datatype_t> *d = new gma_number_p<datatype_t>(v);
        block_loop(cb, NULL, d);
    }
    virtual void assign_all(int value) {
        callback cb;
        cb.fct = &gma_band_p::m_assign_all;
        gma_number_p<datatype_t> *d = new gma_number_p<datatype_t>(value);
        block_loop(cb, NULL, d);
    }
    virtual void add(int summand) {
        callback cb;
        cb.fct = &gma_band_p::m_add;
        gma_number_p<datatype_t> *d = new gma_number_p<datatype_t>(summand);
        block_loop(cb, NULL, d);
    }
    virtual void subtract(int value) {
        callback cb;
        cb.fct = &gma_band_p::m_subtract;
        gma_number_p<datatype_t> *d = new gma_number_p<datatype_t>(value);
        block_loop(cb, NULL, d);
    }
    virtual void multiply(int value) {
        callback cb;
        cb.fct = &gma_band_p::m_multiply;
        gma_number_p<datatype_t> *d = new gma_number_p<datatype_t>(value);
        block_loop(cb, NULL, d);
    }
    virtual void divide(int value) {
        callback cb;
        cb.fct = &gma_band_p::m_divide;
        gma_number_p<datatype_t> *d = new gma_number_p<datatype_t>(value);
        block_loop(cb, NULL, d);
    }
    virtual void modulus(int divisor) {
        callback cb;
        cb.fct = &gma_band_p::m_modulus;
        gma_number_t *d = new_number(divisor);
        block_loop(cb, NULL, d);
    }

    virtual void assign(double value) {
        callback cb;
        cb.fct = &gma_band_p::m_assign;
        datatype_t v = MAX(MIN(value, std::numeric_limits<datatype_t>::max()), std::numeric_limits<datatype_t>::min());
        gma_number_p<datatype_t> *d = new gma_number_p<datatype_t>(v);
        block_loop(cb, NULL, d);
    }
    virtual void assign_all(double value) {
        callback cb;
        cb.fct = &gma_band_p::m_assign_all;
        gma_number_p<datatype_t> *d = new gma_number_p<datatype_t>(value);
        block_loop(cb, NULL, d);
    }
    virtual void add(double summand) {
        callback cb;
        cb.fct = &gma_band_p::m_add;
        gma_number_p<datatype_t> *d = new gma_number_p<datatype_t>(summand);
        block_loop(cb, NULL, d);
    }
    virtual void subtract(double value) {
        callback cb;
        cb.fct = &gma_band_p::m_subtract;
        gma_number_p<datatype_t> *d = new gma_number_p<datatype_t>(value);
        block_loop(cb, NULL, d);
    }
    virtual void multiply(double value) {
        callback cb;
        cb.fct = &gma_band_p::m_multiply;
        gma_number_p<datatype_t> *d = new gma_number_p<datatype_t>(value);
        block_loop(cb, NULL, d);
    }
    virtual void divide(double value) {
        callback cb;
        cb.fct = &gma_band_p::m_divide;
        gma_number_p<datatype_t> *d = new gma_number_p<datatype_t>(value);
        block_loop(cb, NULL, d);
    }

    int m_classify(gma_block<datatype_t> *block, gma_object_t **, gma_object_t *arg, int) {
        gma_classifier_p<datatype_t> *c = (gma_classifier_p<datatype_t> *)arg;
        gma_cell_index i;
        for (i.y = 0; i.y < block->h(); i.y++) {
            for (i.x = 0; i.x < block->w(); i.x++) {
                datatype_t a = block->cell(i);
                // fixme: it should be possible to classify nodata => value and value => nodata
                if (!is_nodata(a))
                    block->cell(i) = c->classify(a);
            }
        }
        return 2;
    }
    int m_cell_callback(gma_block<datatype_t> *block, gma_object_t **, gma_object_t *arg, int) {
        gma_cell_index i;
        int retval;
        for (i.y = 0; i.y < block->h(); i.y++) {
            for (i.x = 0; i.x < block->w(); i.x++) {
                datatype_t a = block->cell(i);
                if (is_nodata(a)) continue;
                gma_cell_index gi = global_cell_index(block, i);
                gma_cell_p<datatype_t> *c = new gma_cell_p<datatype_t>(gi.x, gi.y, a);
                retval = ((gma_cell_callback_p*)arg)->m_callback(c, ((gma_cell_callback_p*)arg)->m_user_data);
                if (retval == 0) return 0;
                if (retval == 2)
                    block->cell(i) = c->value();
            }
        }
        return retval;
    }
    virtual void classify(gma_classifier_t *c) {
        callback cb;
        cb.fct = &gma_band_p::m_classify;
        block_loop(cb, NULL, c);
    }
    virtual void cell_callback(gma_cell_callback_t *c) {
        callback cb;
        cb.fct = &gma_band_p::m_cell_callback;
        block_loop(cb, NULL, c);
    }

    int m_histogram(gma_block<datatype_t> *block, gma_object_t **retval, gma_object_t *arg, int) {
        GMA_RETVAL_INIT(gma_histogram_p<datatype_t>, hm, arg);
        gma_cell_index i;
        for (i.y = 0; i.y < block->h(); i.y++) {
            for (i.x = 0; i.x < block->w(); i.x++) {
                datatype_t value = block->cell(i);
                if (is_nodata(value)) continue;
                hm->increase_count_at(value);
            }
        }
        return 1;
    }
    int m_zonal_neighbors(gma_block<datatype_t> *block, gma_object_t **retval, gma_object_t *, int) {
        GMA_RETVAL_INIT(gma_hash_p<datatype_t COMMA gma_hash_p<datatype_t COMMA gma_number_p<int> > >, zn, );
        gma_cell_index i;
        for (i.y = 0; i.y < block->h(); i.y++) {
            for (i.x = 0; i.x < block->w(); i.x++) {
                datatype_t me = block->cell(i);
                if (is_nodata(me)) continue;
                gma_hash_p<datatype_t,gma_number_p<int> > *ns;
                if (zn->exists(me))
                    ns = zn->get(me);
                else {
                    ns = new gma_hash_p<datatype_t,gma_number_p<int> >;
                    zn->put(me, ns);
                }
                gma_cell_index in = i.first_neighbor();
                for (int neighbor = 1; neighbor < 9; neighbor++) {
                    in.move_to_neighbor(neighbor);

                    if (cell_is_outside(block, in)) {
                        if (!ns->exists(-1))
                            ns->put((int32_t)-1, new gma_number_p<int>(1)); // using -1 to denote outside
                        continue;
                    }

                    datatype_t n;
                    has_value(this, block, in, &n);
                    if (n != me && !ns->exists(n))
                        ns->put(n, new gma_number_p<int>(1) );

                }

            }
        }
        return 1;
    }
    int m_get_min(gma_block<datatype_t> *block, gma_object_t **retval, gma_object_t *arg, int) {
        GMA_RETVAL_INIT(gma_number_p<datatype_t>, rv, );
        gma_cell_index i;
        for (i.y = 0; i.y < block->h(); i.y++) {
            for (i.x = 0; i.x < block->w(); i.x++) {
                datatype_t x = block->cell(i);
                if (is_nodata(x)) continue;
                if (!rv->defined() || x < rv->value())
                    rv->set_value(x);
            }
        }
        return 1;
    }
    int m_get_max(gma_block<datatype_t> *block, gma_object_t **retval, gma_object_t*, int) {
        GMA_RETVAL_INIT(gma_number_p<datatype_t>, rv, );
        gma_cell_index i;
        for (i.y = 0; i.y < block->h(); i.y++) {
            for (i.x = 0; i.x < block->w(); i.x++) {
                datatype_t x = block->cell(i);
                if (is_nodata(x)) continue;
                if (!rv->defined() || x > rv->value())
                    rv->set_value(x);
            }
        }
        return 1;
    }
    int m_get_range(gma_block<datatype_t> *block, gma_object_t **retval, gma_object_t*, int) {
        GMA_RETVAL_INIT(gma_pair_p<gma_number_p<datatype_t>* COMMA gma_number_p<datatype_t>* >, rv,
                        new gma_number_p<datatype_t> COMMA new gma_number_p<datatype_t>);
        gma_number_p<datatype_t>* min = (gma_number_p<datatype_t>*)rv->first();
        gma_number_p<datatype_t>* max = (gma_number_p<datatype_t>*)rv->second();
        gma_cell_index i;
        for (i.y = 0; i.y < block->h(); i.y++) {
            for (i.x = 0; i.x < block->w(); i.x++) {
                datatype_t x = block->cell(i);
                if (is_nodata(x)) continue;
                if (!min->defined() || x < min->value())
                    min->set_value(x);
                if (!max->defined() || x > max->value())
                    max->set_value(x);
            }
        }
        return 1;
    }
    int m_get_cells(gma_block<datatype_t> *block, gma_object_t **retval, gma_object_t*, int) {
        GMA_RETVAL_INIT(std::vector<gma_cell_t*>, cells, );
        gma_cell_index i;
        for (i.y = 0; i.y < block->h(); i.y++) {
            for (i.x = 0; i.x < block->w(); i.x++) {
                datatype_t me = block->cell(i);
                if (is_nodata(me)) continue;
                gma_cell_index gi = global_cell_index(block, i);
                if (me) cells->push_back(new gma_cell_p<datatype_t>(gi.x, gi.y, me));
            }
        }
        return 1;
    }

    virtual gma_histogram_t *histogram() {
        if (!datatype_is_integer()) {
            CPLError(CE_Failure, CPLE_IllegalArg, "Count of values is not supported for non integer bands.");
            return NULL;
        }
        gma_object_t *retval = NULL;
        callback cb;
        cb.fct = &gma_band_p::m_histogram;
        block_loop(cb, &retval);
        return (gma_histogram_t*)retval;
    }
    virtual gma_histogram_t *histogram(gma_pair_t *arg) {
        gma_object_t *retval = NULL;
        callback cb;
        cb.fct = &gma_band_p::m_histogram;
        block_loop(cb, &retval, arg);
        return (gma_histogram_t*)retval;
    }
    virtual gma_histogram_t *histogram(gma_bins_t *arg) {
        gma_object_t *retval = NULL;
        callback cb;
        cb.fct = &gma_band_p::m_histogram;
        block_loop(cb, &retval, arg);
        return (gma_histogram_t*)retval;
    }
    virtual gma_hash_t *zonal_neighbors() {
        gma_object_t *retval = NULL;
        callback cb;
        cb.fct = &gma_band_p::m_zonal_neighbors;
        block_loop(cb, &retval, NULL, 1);
        return (gma_hash_t*)retval;
    }
    virtual gma_number_t *get_min() {
        gma_object_t *retval = NULL;
        callback cb;
        cb.fct = &gma_band_p::m_get_min;
        block_loop(cb, &retval, NULL);
        return (gma_number_t*)retval;
    }
    virtual gma_number_t *get_max() {
        gma_object_t *retval = NULL;
        callback cb;
        cb.fct = &gma_band_p::m_get_max;
        block_loop(cb, &retval, NULL);
        return (gma_number_t*)retval;
    }
    virtual gma_pair_t *get_range() {
        gma_object_t *retval = NULL;
        callback cb;
        cb.fct = &gma_band_p::m_get_range;
        block_loop(cb, &retval, NULL);
        return (gma_pair_t*)retval;
    }
    virtual std::vector<gma_cell_t*> *cells() {
        gma_object_t *retval = NULL;
        callback cb;
        cb.fct = &gma_band_p::m_get_cells;
        block_loop(cb, &retval, NULL);
        return (std::vector<gma_cell_t*> *)retval;
    }

    virtual void assign(gma_band_t *b, gma_logical_operation_t *op = NULL) {
        if (op && op->datatype() != b->datatype()) {
            CPLError(CE_Failure, CPLE_IllegalArg, "The operation must have the same datatype as the argument band.");
            return;
        }
        gma_two_bands_t *tb = gma_new_two_bands(datatype(), b->datatype()) ;
        tb->set_progress_fct(m_progress, m_progress_arg);
        tb->assign(this, b, op);
    }
    virtual void add(gma_band_t *b, gma_logical_operation_t *op = NULL) {
        if (op && op->datatype() != b->datatype()) {
            CPLError(CE_Failure, CPLE_IllegalArg, "The operation must have the same datatype as the argument band.");
            return;
        }
        gma_two_bands_t *tb = gma_new_two_bands(datatype(), b->datatype()) ;
        tb->set_progress_fct(m_progress, m_progress_arg);
        tb->add(this, b, op);
    }
    virtual void subtract(gma_band_t *b, gma_logical_operation_t *op = NULL) {
        if (op && op->datatype() != b->datatype()) {
            CPLError(CE_Failure, CPLE_IllegalArg, "The operation must have the same datatype as the argument band.");
            return;
        }
        gma_two_bands_t *tb = gma_new_two_bands(datatype(), b->datatype()) ;
        tb->set_progress_fct(m_progress, m_progress_arg);
        tb->subtract(this, b, op);
    }
    virtual void multiply(gma_band_t *b, gma_logical_operation_t *op = NULL) {
        if (op && op->datatype() != b->datatype()) {
            CPLError(CE_Failure, CPLE_IllegalArg, "The operation must have the same datatype as the argument band.");
            return;
        }
        gma_two_bands_t *tb = gma_new_two_bands(datatype(), b->datatype()) ;
        tb->set_progress_fct(m_progress, m_progress_arg);
        tb->multiply(this, b, op);
    }
    virtual void divide(gma_band_t *b, gma_logical_operation_t *op = NULL) {
        if (op && op->datatype() != b->datatype()) {
            CPLError(CE_Failure, CPLE_IllegalArg, "The operation must have the same datatype as the argument band.");
            return;
        }
        gma_two_bands_t *tb = gma_new_two_bands(datatype(), b->datatype()) ;
        tb->divide(this, b, op);
        tb->set_progress_fct(m_progress, m_progress_arg);
    }
    virtual void modulus(gma_band_t *b, gma_logical_operation_t *op = NULL) {
        if (op && op->datatype() != b->datatype()) {
            CPLError(CE_Failure, CPLE_IllegalArg, "The operation must have the same datatype as the argument band.");
            return;
        }
        gma_two_bands_t *tb = gma_new_two_bands(datatype(), b->datatype()) ;
        tb->set_progress_fct(m_progress, m_progress_arg);
        tb->modulus(this, b, op);
    }

    virtual void decision(gma_band_t *value, gma_band_t *decision) {
        gma_two_bands_t *tb = gma_new_two_bands(datatype(), value->datatype()) ;
        tb->set_progress_fct(m_progress, m_progress_arg);
        tb->decision(this, value, decision);
    }

    virtual gma_hash_t *zonal_min(gma_band_t *zones) {
        gma_two_bands_t *tb = gma_new_two_bands(datatype(), zones->datatype()) ;
        tb->set_progress_fct(m_progress, m_progress_arg);
        return tb->zonal_min(this, zones);
    }
    virtual gma_hash_t *zonal_max(gma_band_t *zones) {
        gma_two_bands_t *tb = gma_new_two_bands(datatype(), zones->datatype()) ;
        tb->set_progress_fct(m_progress, m_progress_arg);
        return tb->zonal_max(this, zones);
    }

    virtual void rim_by8(gma_band_t *areas) {
        gma_two_bands_t *tb = gma_new_two_bands(datatype(), areas->datatype()) ;
        tb->set_progress_fct(m_progress, m_progress_arg);
        tb->rim_by8(this, areas);
    }

    virtual void fill_depressions(gma_band_t *dem) {
        gma_two_bands_t *tb = gma_new_two_bands(datatype(), dem->datatype()) ;
        tb->set_progress_fct(m_progress, m_progress_arg);
        tb->fill_depressions(this, dem);
    }
    virtual void D8(gma_band_t *dem) {
        gma_two_bands_t *tb = gma_new_two_bands(datatype(), dem->datatype()) ;
        tb->set_progress_fct(m_progress, m_progress_arg);
        tb->D8(this, dem);
    }
    virtual void route_flats(gma_band_t *dem) {
        gma_two_bands_t *tb = gma_new_two_bands(datatype(), dem->datatype()) ;
        tb->set_progress_fct(m_progress, m_progress_arg);
        tb->route_flats(this, dem);
    }
    virtual void upstream_area(gma_band_t *fd) {
        gma_two_bands_t *tb = gma_new_two_bands(datatype(), fd->datatype()) ;
        tb->set_progress_fct(m_progress, m_progress_arg);
        tb->upstream_area(this, fd);
    }
    virtual void catchment(gma_band_t *fd, gma_cell_t *cell) {
        gma_two_bands_t *tb = gma_new_two_bands(datatype(), fd->datatype()) ;
        tb->set_progress_fct(m_progress, m_progress_arg);
        tb->catchment(this, fd, cell);
    }
};

template <> int gma_band_p<uint8_t>::m_log10(gma_block<uint8_t>*, gma_object_t**, gma_object_t*, int);
template <> int gma_band_p<uint16_t>::m_log10(gma_block<uint16_t>*, gma_object_t**, gma_object_t*, int);
template <> int gma_band_p<int16_t>::m_log10(gma_block<int16_t>*, gma_object_t**, gma_object_t*, int);
template <> int gma_band_p<uint32_t>::m_log10(gma_block<uint32_t>*, gma_object_t**, gma_object_t*, int);
template <> int gma_band_p<int32_t>::m_log10(gma_block<int32_t>*, gma_object_t**, gma_object_t*, int);

template <> int gma_band_p<float>::m_modulus(gma_block<float>*, gma_object_t**, gma_object_t*, int);
template <> int gma_band_p<double>::m_modulus(gma_block<double>*, gma_object_t**, gma_object_t*, int);
