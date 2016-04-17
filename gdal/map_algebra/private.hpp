#include "gdal_map_algebra.hpp"
#include "hash.hpp"
#include "type_switch.hpp"
#include <cstdlib>
#include <cmath>

template <typename T> struct GDALDataTypeTraits
{
    static const GDALDataType datatype;
    static const bool is_integer;
    static const bool is_float;
    static const bool is_complex;
};

template <> struct GDALDataTypeTraits<uint8_t>
{
    static const GDALDataType datatype = GDT_Byte;
    static const bool is_integer = true;
    static const bool is_float = false;
    static const bool is_complex = false;
};

template <> struct GDALDataTypeTraits<uint16_t>
{
    static const GDALDataType datatype = GDT_UInt16;
    static const bool is_integer = true;
    static const bool is_float = false;
    static const bool is_complex = false;
};

template <> struct GDALDataTypeTraits<int16_t>
{
    static const GDALDataType datatype = GDT_Int16;
    static const bool is_integer = true;
    static const bool is_float = false;
    static const bool is_complex = false;
};

template <> struct GDALDataTypeTraits<uint32_t>
{
    static const GDALDataType datatype = GDT_UInt32;
    static const bool is_integer = true;
    static const bool is_float = false;
    static const bool is_complex = false;
};

template <> struct GDALDataTypeTraits<int32_t>
{
    static const GDALDataType datatype = GDT_Int32;
    static const bool is_integer = true;
    static const bool is_float = false;
    static const bool is_complex = false;
};

template <> struct GDALDataTypeTraits<float>
{
    static const GDALDataType datatype = GDT_Float32;
    static const bool is_integer = false;
    static const bool is_float = true;
    static const bool is_complex = false;
};

template <> struct GDALDataTypeTraits<double>
{
    static const GDALDataType datatype = GDT_Float64;
    static const bool is_integer = false;
    static const bool is_float = true;
    static const bool is_complex = false;
};

typedef struct {
    int x;
    int y;
} gma_block_index; // block coordinates

typedef struct {
    int x;
    int y;
} gma_cell_index; // cell coordinates in block or globally

#define gma_cell_first_neighbor(center_cell) { .x = center_cell.x, .y = center_cell.y-1 }

#define gma_cell_move_to_neighbor(cell, neighbor)       \
    switch(neighbor) {                                   \
    case 2: cell.x++; break;                             \
    case 3: cell.y++; break;                             \
    case 4: cell.y++; break;                             \
    case 5: cell.x--; break;                             \
    case 6: cell.x--; break;                             \
    case 7: cell.y--; break;                             \
    case 8: cell.y--; break;                             \
    }

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
        CPLErr e = band->ReadBlock(m_index.x, m_index.y, block);
        if (e != CE_None) {
            fprintf(stderr, "ReadBlock error.");
            exit(1);
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
    CPLErr write(GDALRasterBand *band) {
        return band->WriteBlock(m_index.x, m_index.y, block);
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
        else
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
        if (m_n == 0)
            return;
        for (int i = 0; i < m_n; i++)
            delete m_blocks[i];
        CPLFree(m_blocks);
        m_n = 0;
        m_blocks = NULL;
    }
    void remove(int i) {
        if (i < 0 || i >= m_n) return;
        gma_block<datatype> **blocks = (gma_block<datatype>**)CPLMalloc((m_n-1) * sizeof(gma_block<datatype>*));
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
        for (int i = 0; i < m_n; i++)
            if (m_blocks[i]->m_index.x == index.x && m_blocks[i]->m_index.y == index.y)
                return m_blocks[i];
        return NULL;
    }
    CPLErr add(gma_block<datatype> *block) {
        m_n++;
        if (m_n == 1)
            m_blocks = (gma_block<datatype>**)CPLMalloc(sizeof(gma_block<datatype>*));
        else
            m_blocks = (gma_block<datatype>**)CPLRealloc(m_blocks, m_n * sizeof(gma_block<datatype>*));
        m_blocks[m_n-1] = block;
    }
    void remove(gma_block_index i20, gma_block_index i21) {
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
#define gma_retval_init(class, var, arg)        \
    class *var;                                 \
    if (*retval == NULL) {                      \
        var = new class(arg);                   \
        *retval = (gma_object_t*)var;           \
    } else                                      \
        var = (class *)*retval;

class gma_two_bands_t {
public:
    virtual void assign(gma_band_t *, gma_band_t *) {};
    virtual void add(gma_band_t *summand1, gma_band_t *summand2) {};
    virtual void subtract(gma_band_t *, gma_band_t *) {};
    virtual void multiply(gma_band_t *, gma_band_t *) {};
    virtual void divide(gma_band_t *, gma_band_t *) {};
    virtual void modulus(gma_band_t *, gma_band_t *) {};

    virtual gma_hash_t *zonal_min(gma_band_t *, gma_band_t *zones) {};
    virtual gma_hash_t *zonal_max(gma_band_t *, gma_band_t *zones) {};

    virtual void rim_by8(gma_band_t *rims, gma_band_t *zones) {};

    virtual void fill_depressions(gma_band_t *filled_dem, gma_band_t *dem) {};
    virtual void D8(gma_band_t *fd, gma_band_t *dem) {};
    virtual void route_flats(gma_band_t *fd, gma_band_t *dem) {};
    virtual void upstream_area(gma_band_t *ua, gma_band_t *fd) {};
    virtual void catchment(gma_band_t *catchment, gma_band_t *fd, gma_cell_t *outlet) {};
};
gma_two_bands_t *gma_new_two_bands(GDALDataType type1, GDALDataType type2);

template <typename datatype> class gma_band_p : public gma_band_t {
    GDALRasterBand *band;
    int m_w;
    int m_h;
    int m_w_block;
    int m_h_block;
    GDALDataType m_gdal_datatype;
    int datatype_size;
    gma_block_cache<datatype> cache;
    datatype m_nodata;
    bool m_has_nodata;
    gma_band_p<uint8_t> *mask;

public:
    int w_blocks;
    int h_blocks;
    gma_band_p(GDALRasterBand *b) {
        band = b;
        m_w = b->GetXSize();
        m_h = b->GetYSize();
        b->GetBlockSize(&m_w_block, &m_h_block);
        w_blocks = (m_w + m_w_block - 1) / m_w_block;
        h_blocks = (m_h + m_h_block - 1) / m_h_block;
        m_gdal_datatype = b->GetRasterDataType();
        int has_nodata;
        double nodata = b->GetNoDataValue(&has_nodata);
        m_has_nodata = has_nodata != 0;
        m_nodata = has_nodata ? (datatype)nodata : 0;
        int mask_flags = b->GetMaskFlags();
        mask = NULL;
        if (mask_flags & GMF_PER_DATASET || mask_flags & GMF_ALPHA) {
            GDALRasterBand *m = b->GetMaskBand();
            if (m) mask = new gma_band_p<uint8_t>(m);
        }
        datatype_size = sizeof(datatype);
    }
    ~gma_band_p() {
        delete(mask);
    }
    GDALDataset *dataset() {
        return band->GetDataset();
    }
    int w() {
        return m_w;
    }
    int h() {
        return m_h;
    }
    int w_block() {
        return m_w_block;
    }
    int h_block() {
        return m_h_block;
    }
    virtual GDALDataType gdal_datatype() {
        return m_gdal_datatype;
    }
    void empty_cache() {
        cache.empty();
        if (mask) mask->empty_cache();
    }
    gma_block<datatype> *get_block(gma_block_index i) {
        return cache.retrieve(i);
    }
    CPLErr write_block(gma_block<datatype> *block) {
        return block->write(band);
    }
    CPLErr add_to_cache(gma_block_index i) {
        gma_block<datatype> *b = cache.retrieve(i);
        if (!b) {
            int w = ( (i.x+1) * m_w_block > m_w ) ? m_w - i.x * m_w_block : m_w_block;
            int h = ( (i.y+1) * m_h_block > m_h ) ? m_h - i.y * m_h_block : m_h_block;
            b = new gma_block<datatype>(i, w, h, band, m_w_block, m_h_block);
            cache.add(b);
        }
        if (mask) {
        }
        // fixme: add here update mask cache
    }
    template <typename type1> CPLErr update_cache(gma_band_p<type1> *band1, gma_block<type1> *b1, int d) {

        // fixme: add here update mask cache

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

        {
            // add needed blocks
            gma_block_index i;
            for (i.y = i20.y; i.y <= i21.y; i.y++) {
                for (i.x = i20.x; i.x <= i21.x; i.x++) {
                    add_to_cache(i);
                }
            }
        }
        {
            // remove unneeded blocks
            cache.remove(i20, i21);
        }
    }
    inline gma_cell_index global_cell_index(gma_block<datatype> *b, gma_cell_index i) {
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
    inline bool cell_is_outside(gma_block<datatype> *b, gma_cell_index i) {
        // global cell index
        int x = b->m_index.x * m_w_block + i.x;
        int y = b->m_index.y * m_h_block + i.y;
        return (x < 0 || y < 0 || x >= m_w || y >= m_h);
    }
    inline bool cell_is_outside(gma_cell_index i) {
        return (i.x < 0 || i.y < 0 || i.x >= m_w || i.y >= m_h);
    }
    inline bool is_nodata(datatype value) {
        return m_has_nodata && value == m_nodata;
    }
    inline bool cell_is_nodata(gma_block<datatype> *b, gma_cell_index i) {
        if (mask) {
            uint8_t mask_value;
            if (mask->has_value(this, b, i, &mask_value)) {
                if (mask_value == 0) return true;
            } else {
                fprintf(stderr, "Mask's cache not updated.");
                exit(1);
            }
        }
        return m_has_nodata && b->cell(i) == m_nodata;
    }
    int is_border_block(gma_block<datatype> *block) {
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
    inline bool last_block(gma_block<datatype> *b) {
        return b->m_index.x == w_blocks-1 && b->m_index.y == h_blocks-1;
    }
    // get the block, which has the cell, that is pointed to by i2 in block2 in band2
    template <typename type2> gma_block<datatype> *get_block(gma_band_p<type2> *band2, gma_block<type2> *b2, gma_cell_index i2, gma_cell_index *i1) {
        gma_cell_index gi = band2->global_cell_index(b2, i2);
        if (cell_is_outside(gi)) return NULL;
        gma_block_index i = block_index(gi);
        gma_block<datatype> *rv = get_block(i);
        if (rv) *i1 = cell_index(gi);
        return rv;
    }
    // returns false if cell is not on the band or is nodata cell
    template<typename type2> bool has_value(gma_band_p<type2> *band2, gma_block<type2> *block2, gma_cell_index index2, datatype *value) {
        gma_cell_index index;
        gma_block<datatype> *block = get_block(band2, block2, index2, &index);
        if (block) {
            if (cell_is_nodata(block, index)) return false;
            *value = block->cell(index);
            return true;
        } else
            return false;
    }

    virtual gma_number_t *new_number(int value) {
        return new gma_number_p<int>(value);
    }

    struct callback {
        typedef int (gma_band_p<datatype>::*type)(gma_block<datatype>*, gma_object_t **, gma_object_t*);
        type fct;
    };

    // fixme: add focal_distance
    void within_block_loop(callback cb, gma_object_t **retval = NULL, gma_object_t *arg = NULL) {
        gma_block_index i;
        for (i.y = 0; i.y < h_blocks; i.y++) {
            for (i.x = 0; i.x < w_blocks; i.x++) {
                add_to_cache(i);
                gma_block<datatype> *block = get_block(i);
                int ret = (this->*cb.fct)(block, retval, arg);
                if (ret == 2) CPLErr e = write_block(block);
            }
        }
    }

    const char *space() { 
        return "";
    }
    const char *format() { 
        return "%i ";
    }
    int m_print(gma_block<datatype> *block, gma_object_t **, gma_object_t*) {
        gma_cell_index i;
        for (i.y = 0; i.y < block->h(); i.y++) {
            for (i.x = 0; i.x < block->w(); i.x++) {
                if (cell_is_nodata(block, i))
                    printf("%s", space());
                else
                    printf(format(), block->cell(i));
            }
            printf("\n");
        }
        return 1;
    }

    int m_rand(gma_block<datatype>* block, gma_object_t **, gma_object_t*) {
        gma_cell_index i;
        for (i.y = 0; i.y < block->h(); i.y++) {
            for (i.x = 0; i.x < block->w(); i.x++) {
                block->cell(i) = std::rand();
            }
        }
        return 2;
    }

    int m_abs(gma_block<datatype> *block, gma_object_t **, gma_object_t*) {
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

    int m_exp(gma_block<datatype> *block, gma_object_t **, gma_object_t*) {
        gma_cell_index i;
        for (i.y = 0; i.y < block->h(); i.y++) {
            for (i.x = 0; i.x < block->w(); i.x++) {
                if (cell_is_nodata(block, i)) continue;
                block->cell(i) = std::exp(block->cell(i));
            }
        }
        return 2;
    }

    int m_log(gma_block<datatype> *block, gma_object_t **, gma_object_t*) {
        gma_cell_index i;
        for (i.y = 0; i.y < block->h(); i.y++) {
            for (i.x = 0; i.x < block->w(); i.x++) {
                if (cell_is_nodata(block, i)) continue;
                block->cell(i) = std::log(block->cell(i));
            }
        }
        return 2;
    }

    int m_log10(gma_block<datatype> *block, gma_object_t **, gma_object_t*) {
        gma_cell_index i;
        for (i.y = 0; i.y < block->h(); i.y++) {
            for (i.x = 0; i.x < block->w(); i.x++) {
                if (cell_is_nodata(block, i)) continue;
                block->cell(i) = std::log10(block->cell(i));
            }
        }
        return 2;
    }

    int m_sqrt(gma_block<datatype> *block, gma_object_t **, gma_object_t*) {
        gma_cell_index i;
        for (i.y = 0; i.y < block->h(); i.y++) {
            for (i.x = 0; i.x < block->w(); i.x++) {
                if (cell_is_nodata(block, i)) continue;
                block->cell(i) = std::sqrt(block->cell(i));
            }
        }
        return 2;
    }

    int m_sin(gma_block<datatype> *block, gma_object_t **, gma_object_t*) {
        gma_cell_index i;
        for (i.y = 0; i.y < block->h(); i.y++) {
            for (i.x = 0; i.x < block->w(); i.x++) {
                if (cell_is_nodata(block, i)) continue;
                block->cell(i) = std::sin(block->cell(i));
            }
        }
        return 2;
    }

    int m_cos(gma_block<datatype> *block, gma_object_t **, gma_object_t*) {
        gma_cell_index i;
        for (i.y = 0; i.y < block->h(); i.y++) {
            for (i.x = 0; i.x < block->w(); i.x++) {
                if (cell_is_nodata(block, i)) continue;
                block->cell(i) = std::cos(block->cell(i));
            }
        }
        return 2;
    }

    int m_tan(gma_block<datatype> *block, gma_object_t **, gma_object_t*) {
        gma_cell_index i;
        for (i.y = 0; i.y < block->h(); i.y++) {
            for (i.x = 0; i.x < block->w(); i.x++) {
                if (cell_is_nodata(block, i)) continue;
                block->cell(i) = std::tan(block->cell(i));
            }
        }
        return 2;
    }

    int m_ceil(gma_block<datatype> *block, gma_object_t **, gma_object_t*) {
        gma_cell_index i;
        for (i.y = 0; i.y < block->h(); i.y++) {
            for (i.x = 0; i.x < block->w(); i.x++) {
                if (cell_is_nodata(block, i)) continue;
                block->cell(i) = std::ceil(block->cell(i));
            }
        }
        return 2;
    }

    int m_floor(gma_block<datatype> *block, gma_object_t **, gma_object_t*) {
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
        within_block_loop(cb);
    }
    virtual void rand() {
        callback cb;
        cb.fct = &gma_band_p::m_rand;
        within_block_loop(cb);
    }
    virtual void abs() {
        callback cb;
        cb.fct = &gma_band_p::m_abs;
        within_block_loop(cb);
    }
    virtual void exp() {
        callback cb;
        cb.fct = &gma_band_p::m_exp;
        within_block_loop(cb);
    }
    virtual void log() {
        callback cb;
        cb.fct = &gma_band_p::m_log;
        within_block_loop(cb);
    }
    virtual void log10() {
        callback cb;
        cb.fct = &gma_band_p::m_log10;
        within_block_loop(cb);
    }
    virtual void sqrt() {
        callback cb;
        cb.fct = &gma_band_p::m_sqrt;
        within_block_loop(cb);
    }
    virtual void sin() {
        callback cb;
        cb.fct = &gma_band_p::m_sin;
        within_block_loop(cb);
    }
    virtual void cos() {
        callback cb;
        cb.fct = &gma_band_p::m_cos;
        within_block_loop(cb);
    }
    virtual void tan() {
        callback cb;
        cb.fct = &gma_band_p::m_tan;
        within_block_loop(cb);
    }
    virtual void ceil() {
        callback cb;
        cb.fct = &gma_band_p::m_ceil;
        within_block_loop(cb);
    }
    virtual void floor() {
        callback cb;
        cb.fct = &gma_band_p::m_floor;
        within_block_loop(cb);
    }

    int m_add(gma_block<datatype>* block, gma_object_t **, gma_object_t *arg) {
        datatype a = ((gma_number_p<datatype>*)arg)->value();
        gma_cell_index i;
        for (i.y = 0; i.y < block->h(); i.y++) {
            for (i.x = 0; i.x < block->w(); i.x++) {
                if (!cell_is_nodata(block, i))
                    block->cell(i) += a;
            }
        }
        return 2;
    }
    int m_modulus(gma_block<datatype> *block, gma_object_t **, gma_object_t *arg) {
        int a = ((gma_number_t*)arg)->value_as_int();
        gma_cell_index i;
        for (i.y = 0; i.y < block->h(); i.y++) {
            for (i.x = 0; i.x < block->w(); i.x++) {
                if (cell_is_nodata(block, i)) continue;
                block->cell(i) %= a;
            }
        }
        return 2;
    }
    virtual void assign(int value) {};
    virtual void assign_all(int value) {};
    virtual void add(int summand) {
        callback cb;
        cb.fct = &gma_band_p::m_add;
        gma_number_p<datatype> *d = new gma_number_p<datatype>(summand);
        within_block_loop(cb, NULL, d);
    }
    virtual void subtract(int) {};
    virtual void multiply(int) {};
    virtual void divide(int) {};
    virtual void modulus(int divisor) {
        callback cb;
        cb.fct = &gma_band_p::m_modulus;
        gma_number_t *d = new_number(divisor);
        within_block_loop(cb, NULL, d);
    }
    virtual void assign(double value) {};
    virtual void assign_all(double value) {};
    virtual void add(double summand) {};
    virtual void subtract(double) {};
    virtual void multiply(double) {};
    virtual void divide(double) {};

    virtual void classify(gma_classifier_t*) {};
    virtual void cell_callback(gma_cell_callback_t*) {};


    int m_histogram(gma_block<datatype> *block, gma_object_t **retval, gma_object_t *arg) {
        gma_retval_init(gma_histogram_p<datatype>, hm, arg);
        gma_cell_index i;
        for (i.y = 0; i.y < block->h(); i.y++) {
            for (i.x = 0; i.x < block->w(); i.x++) {
                datatype value = block->cell(i);
                if (is_nodata(value)) continue;
                hm->increase_count_at(value);
            }
        }
        return 1;
    }
    int m_zonal_neighbors(gma_block<datatype> *block, gma_object_t **retval, gma_object_t *) {
        gma_retval_init(gma_hash_p<datatype COMMA gma_hash_p<datatype COMMA gma_number_p<int> > >, zn, );
        gma_cell_index i;
        for (i.y = 0; i.y < block->h(); i.y++) {
            for (i.x = 0; i.x < block->w(); i.x++) {
                datatype me = block->cell(i);
                if (is_nodata(me)) continue;
                gma_hash_p<datatype,gma_number_p<int> > *ns;
                if (zn->exists(me))
                    ns = zn->get(me);
                else {
                    ns = new gma_hash_p<datatype,gma_number_p<int> >;
                    zn->put(me, ns);
                }
                gma_cell_index in = gma_cell_first_neighbor(i);
                for (int neighbor = 1; neighbor < 9; neighbor++) {
                    gma_cell_move_to_neighbor(in, neighbor); // fixme: this needs focal_distance

                    if (cell_is_outside(block, in)) {
                        if (!ns->exists(-1))
                            ns->put((int32_t)-1, new gma_number_p<int>(1)); // using -1 to denote outside
                        continue;
                    }
                
                    datatype n;
                    has_value(this, block, in, &n);
                    if (n != me && !ns->exists(n))
                        ns->put(n, new gma_number_p<int>(1) );

                }

            }
        }
        return 1;
    }
    int m_get_min(gma_block<datatype> *block, gma_object_t **retval, gma_object_t *arg) {
        gma_retval_init(gma_number_p<datatype>, rv, );
        gma_cell_index i;
        for (i.y = 0; i.y < block->h(); i.y++) {
            for (i.x = 0; i.x < block->w(); i.x++) {
                datatype x = block->cell(i);
                if (is_nodata(x)) continue;
                if (!rv->defined() || x < rv->value())
                    rv->set_value(x);
            }
        }
        return 1;
    }
    int m_get_max(gma_block<datatype> *block, gma_object_t **retval, gma_object_t *arg) {
        gma_retval_init(gma_number_p<datatype>, rv, );
        gma_cell_index i;
        for (i.y = 0; i.y < block->h(); i.y++) {
            for (i.x = 0; i.x < block->w(); i.x++) {
                datatype x = block->cell(i);
                if (is_nodata(x)) continue;
                if (!rv->defined() || x > rv->value())
                    rv->set_value(x);
            }
        }
        return 1;
    }
    int m_get_range(gma_block<datatype> *block, gma_object_t **retval, gma_object_t *arg) {
        gma_retval_init(gma_pair_p<gma_number_p<datatype>* COMMA gma_number_p<datatype>* >, rv, 
                        new gma_number_p<datatype> COMMA new gma_number_p<datatype>);
        gma_number_p<datatype>* min = (gma_number_p<datatype>*)rv->first();
        gma_number_p<datatype>* max = (gma_number_p<datatype>*)rv->second();
        gma_cell_index i;
        for (i.y = 0; i.y < block->h(); i.y++) {
            for (i.x = 0; i.x < block->w(); i.x++) {
                datatype x = block->cell(i);
                if (is_nodata(x)) continue;
                if (!min->defined() || x < min->value())
                    min->set_value(x);
                if (!max->defined() || x > max->value())
                    max->set_value(x);
            }
        }
        return 1;
    }
    
    virtual gma_histogram_t *histogram(gma_object_t *arg = NULL) {
        gma_object_t *retval = NULL;
        callback cb;
        cb.fct = &gma_band_p::m_histogram;
        within_block_loop(cb, &retval, arg);
        return (gma_histogram_t*)retval;
    }
    virtual gma_hash_t *zonal_neighbors() {
        gma_object_t *retval = NULL;
        callback cb;
        cb.fct = &gma_band_p::m_zonal_neighbors;
        within_block_loop(cb, &retval, NULL);
        return (gma_hash_t*)retval;
    }
    virtual gma_number_t *get_min() {
        gma_object_t *retval = NULL;
        callback cb;
        cb.fct = &gma_band_p::m_get_min;
        within_block_loop(cb, &retval, NULL);
        return (gma_number_t*)retval;
    }
    virtual gma_number_t *get_max() {
        gma_object_t *retval = NULL;
        callback cb;
        cb.fct = &gma_band_p::m_get_max;
        within_block_loop(cb, &retval, NULL);
        return (gma_number_t*)retval;
    }
    virtual gma_pair_t *get_range() {
        gma_object_t *retval = NULL;
        callback cb;
        cb.fct = &gma_band_p::m_get_range;
        within_block_loop(cb, &retval, NULL);
        return (gma_pair_t*)retval;
    }
    virtual std::vector<gma_cell_t*> *gma_method_get_cells() {};

    virtual void assign(gma_band_t *b) {
        gma_two_bands_t *tb = gma_new_two_bands(gdal_datatype(), b->gdal_datatype()) ;
        tb->assign(this, b);
    }
    virtual void add(gma_band_t *summand) {
        gma_two_bands_t *tb = gma_new_two_bands(gdal_datatype(), summand->gdal_datatype()) ;
        tb->add(this, summand);
    }
    virtual void subtract(gma_band_t *b) {
        gma_two_bands_t *tb = gma_new_two_bands(gdal_datatype(), b->gdal_datatype()) ;
        tb->subtract(this, b);
    }
    virtual void multiply(gma_band_t *b) {
        gma_two_bands_t *tb = gma_new_two_bands(gdal_datatype(), b->gdal_datatype()) ;
        tb->multiply(this, b);
    }
    virtual void divide(gma_band_t *b) {
        gma_two_bands_t *tb = gma_new_two_bands(gdal_datatype(), b->gdal_datatype()) ;
        tb->divide(this, b);
    }
    virtual void modulus(gma_band_t *b) {
        gma_two_bands_t *tb = gma_new_two_bands(gdal_datatype(), b->gdal_datatype()) ;
        tb->modulus(this, b);
    }

    virtual gma_hash_t *zonal_min(gma_band_t *zones) {
        gma_two_bands_t *tb = gma_new_two_bands(gdal_datatype(), zones->gdal_datatype()) ;
        tb->zonal_min(this, zones);
    }
    virtual gma_hash_t *zonal_max(gma_band_t *zones) {
        gma_two_bands_t *tb = gma_new_two_bands(gdal_datatype(), zones->gdal_datatype()) ;
        tb->zonal_max(this, zones);
    }

    virtual void rim_by8(gma_band_t *areas) {
        gma_two_bands_t *tb = gma_new_two_bands(gdal_datatype(), areas->gdal_datatype()) ;
        tb->rim_by8(this, areas);
    }

    virtual void fill_depressions(gma_band_t *dem) {
        gma_two_bands_t *tb = gma_new_two_bands(gdal_datatype(), dem->gdal_datatype()) ;
        tb->fill_depressions(this, dem);
    }
    virtual void D8(gma_band_t *dem) {
        gma_two_bands_t *tb = gma_new_two_bands(gdal_datatype(), dem->gdal_datatype()) ;
        tb->D8(this, dem);
    }
    virtual void route_flats(gma_band_t *) {};
    virtual void upstream_area(gma_band_t *) {};
    virtual void catchment(gma_band_t *, gma_cell_t *) {};
};

template <> int gma_band_p<uint8_t>::m_log10(gma_block<uint8_t>*, gma_object_t**, gma_object_t*);
template <> int gma_band_p<uint16_t>::m_log10(gma_block<uint16_t>*, gma_object_t**, gma_object_t*);
template <> int gma_band_p<int16_t>::m_log10(gma_block<int16_t>*, gma_object_t**, gma_object_t*);
template <> int gma_band_p<uint32_t>::m_log10(gma_block<uint32_t>*, gma_object_t**, gma_object_t*);
template <> int gma_band_p<int32_t>::m_log10(gma_block<int32_t>*, gma_object_t**, gma_object_t*);

template <> int gma_band_p<float>::_modulus(gma_block<float>*, gma_object_t**, gma_object_t*);
template <> int gma_band_p<double>::_modulus(gma_block<double>*, gma_object_t**, gma_object_t*);

class gma_band_iterator_t : public gma_object_t {
public:
    long count_in_this_loop_of_band;
    long total_count;
    gma_band_iterator_t() {
        count_in_this_loop_of_band = 0;
        total_count = 0;
    }
    void new_loop() {
        count_in_this_loop_of_band = 0;
    }
    void add() {
        count_in_this_loop_of_band++;
        total_count++;
    }
};

template <typename type1,typename type2> class gma_two_bands_p : public gma_two_bands_t {
    gma_band_p<type1> *b1;
    gma_band_p<type2> *b2;
    struct callback {
        typedef int (gma_two_bands_p<type1,type2>::*type)(gma_block<type1>*, gma_object_t**, gma_object_t*, int);
        type fct;
    };
    void within_block_loop(callback cb, gma_object_t **retval = NULL, gma_object_t *arg = NULL, int focal_distance = 0) {
        gma_block_index i;
        int iterate = 1;
        while (iterate) {
            iterate = 0;
            for (i.y = 0; i.y < b1->h_blocks; i.y++) {
                for (i.x = 0; i.x < b1->w_blocks; i.x++) {
                    b1->add_to_cache(i);
                    gma_block<type1> *block = b1->get_block(i);
                    CPLErr e = b1->update_cache(b1, block, focal_distance);
                    e = b2->update_cache(b1, block, focal_distance);
                    int ret = (this->*cb.fct)(block, retval, arg, focal_distance);
                    switch (ret) {
                    case 0: return;
                    case 1: break;
                    case 2:
                        e = b1->write_block(block);
                        break;
                    case 3:
                        e = b1->write_block(block);
                        iterate = 1;
                        break;
                    case 4:
                        e = b1->write_block(block);
                        iterate = 2;
                        break;
                    }
                }
            }
            // fixme? iteration 
        }
        b1->empty_cache();
        b2->empty_cache();
    }
    bool test_operator(gma_logical_operation_p<type2> *op, type2 value) {
        switch (op->m_op) {
        case gma_eq:
            return value == op->m_value;
        case gma_ne:
            return value != op->m_value;
        case gma_gt:
            return value > op->m_value;
        case gma_lt:
            return value < op->m_value;
        case gma_ge:
            return value >= op->m_value;
        case gma_le:
            return value <= op->m_value;
        case gma_and:
            return value && op->m_value;
        case gma_or:
            return value || op->m_value;
        case gma_not:
            return not value;
        }
    }
    int m_assign(gma_block<type1> *block, gma_object_t**, gma_object_t *arg, int) {
        // fixme in all subs arg is checked here
        gma_cell_index i;
        for (i.y = 0; i.y < block->h(); i.y++) {
            for (i.x = 0; i.x < block->w(); i.x++) {
                if (b1->cell_is_nodata(block, i)) continue;
                type2 value;
                if (b2->has_value(b1, block, i, &value)) {
                    if (arg) {
                        if (test_operator((gma_logical_operation_p<type2> *)arg, value))
                            block->cell(i) = value;
                    } else
                        block->cell(i) = value;
                }
            }
        }
        return 2;
    }
    int m_add(gma_block<type1> *block, gma_object_t**, gma_object_t *arg, int) {
        gma_cell_index i;
        for (i.y = 0; i.y < block->h(); i.y++) {
            for (i.x = 0; i.x < block->w(); i.x++) {
                if (b1->cell_is_nodata(block, i)) continue;
                type2 value;
                if (b2->has_value(b1, block, i, &value)) {
                    if (arg) {
                        if (test_operator((gma_logical_operation_p<type2> *)arg, value))
                            block->cell(i) += value;
                    } else
                        block->cell(i) += value;
                }
            }
        }
        return 2;
    }
    int m_subtract(gma_block<type1> *block1, gma_object_t**, gma_object_t *arg, int) {
        gma_cell_index i1;
        for (i1.y = 0; i1.y < block1->h(); i1.y++) {
            for (i1.x = 0; i1.x < block1->w(); i1.x++) {
                if (b1->cell_is_nodata(block1,i1)) continue;
                type2 value;
                if (b2->has_value(b1, block1, i1, &value)) {
                    if (arg) {
                        if (test_operator((gma_logical_operation_p<type2> *)arg, value))
                            block1->cell(i1) -= value;
                    } else
                        block1->cell(i1) -= value;
                }
            }
        }
        return 2;
    }
    int m_multiply(gma_block<type1> *block1, gma_object_t**, gma_object_t *arg, int) {
        gma_cell_index i1;
        for (i1.y = 0; i1.y < block1->h(); i1.y++) {
            for (i1.x = 0; i1.x < block1->w(); i1.x++) {
                if (b1->cell_is_nodata(block1,i1)) continue;
                type2 value;
                if (b2->has_value(b1, block1, i1, &value)) {
                    if (arg) {
                        if (test_operator((gma_logical_operation_p<type2> *)arg, value))
                            block1->cell(i1) *= value;
                    } else
                        block1->cell(i1) *= value;
                }
            }
        }
        return 2;
    }
    int m_divide(gma_block<type1> *block1, gma_object_t**, gma_object_t *arg, int) {
        gma_cell_index i1;
        for (i1.y = 0; i1.y < block1->h(); i1.y++) {
            for (i1.x = 0; i1.x < block1->w(); i1.x++) {
                if (b1->cell_is_nodata(block1,i1)) continue;
                type2 value;
                if (b2->has_value(b1, block1, i1, &value)) {
                    if (arg) {
                        if (test_operator((gma_logical_operation_p<type2> *)arg, value))
                            block1->cell(i1) /= value;
                    } else
                        block1->cell(i1) /= value;
                }
            }
        }
        return 2;
    }
    int m_modulus(gma_block<type1> *block1, gma_object_t**, gma_object_t *arg, int) {
        gma_cell_index i1;
        for (i1.y = 0; i1.y < block1->h(); i1.y++) {
            for (i1.x = 0; i1.x < block1->w(); i1.x++) {
                if (b1->cell_is_nodata(block1,i1)) continue;
                type2 value;
                if (b2->has_value(b1, block1, i1, &value)) {
                    if (arg) {
                        if (test_operator((gma_logical_operation_p<type2> *)arg, value))
                            block1->cell(i1) %= value;
                    } else
                        block1->cell(i1) %= value;
                }
            }
        }
        return 2;
    }
    // b1 = values, b2 = zones
    int m_zonal_min(gma_block<type1> *block1, gma_object_t **retval, gma_object_t*, int) {
        gma_retval_init(gma_hash_p<type2 COMMA gma_number_p<type1> >, rv, );
        gma_cell_index i;
        for (i.y = 0; i.y < block1->h(); i.y++) {
            for (i.x = 0; i.x < block1->w(); i.x++) {
                if (b1->cell_is_nodata(block1,i)) continue;
                type1 value = block1->cell(i);
                type2 zone;
                if (!b2->has_value(b1, block1, i, &zone))
                    continue;
                if (rv->exists(zone)) {
                    type1 old_value = rv->get(zone)->value();
                    if (value > old_value)
                        continue;
                }
                rv->put(zone, new gma_number_p<type1>(value));
            }
        }
        return 1;
    }
    int m_zonal_max(gma_block<type1> *block1, gma_object_t **retval, gma_object_t*, int) {
        gma_retval_init(gma_hash_p<type2 COMMA gma_number_p<type1> >, rv, );
        gma_cell_index i;
        for (i.y = 0; i.y < block1->h(); i.y++) {
            for (i.x = 0; i.x < block1->w(); i.x++) {
                if (b1->cell_is_nodata(block1,i)) continue;
                type1 value = block1->cell(i);
                type2 zone;
                if (!b2->has_value(b1, block1, i, &zone))
                    continue;
                if (rv->exists(zone)) {
                    type1 old_value = rv->get(zone)->value();
                    if (value < old_value)
                        continue;
                }
                rv->put(zone, new gma_number_p<type1>(value));
            }
        }
        return 1;
    }
    // b1 = rims, b2 = areas
    int m_rim_by8(gma_block<type1> *block, gma_object_t**, gma_object_t*, int) {
        gma_cell_index i;
        for (i.y = 0; i.y < block->h(); i.y++) {
            for (i.x = 0; i.x < block->w(); i.x++) {

                // if the 8-neighborhood in areas is all of the same area, then set rims = 0, otherwise from area

                type2 area;
                b2->has_value(b1, block, i, &area);

                type1 my_area = 0;

                gma_cell_index in = gma_cell_first_neighbor(i);
                for (int neighbor = 1; neighbor < 9; neighbor++) {
                    gma_cell_move_to_neighbor(in, neighbor);
                    type2 n_area;
                    bool has_neighbor = b2->has_value(b1, block, in, &n_area);
                    if (!has_neighbor || (has_neighbor && (n_area != area))) {
                        my_area = area;
                        break;
                    }
                }

                block->cell(i) = my_area;

            }
        }
        return 2;
    }
    // b1 = filled_dem, b2 = dem
    int m_fill_depressions(gma_block<type1> *block, gma_object_t **retval, gma_object_t*, int) {
        gma_retval_init(gma_band_iterator_t, rv, );
        if (block->first_block())
            rv->new_loop();
        int border_block = b1->is_border_block(block);
        gma_cell_index i;
        for (i.y = 0; i.y < block->h(); i.y++) {
            for (i.x = 0; i.x < block->w(); i.x++) {
                int border_cell = block->is_border_cell(border_block, i);
                type2 dem_e;
                b2->has_value(b1, block, i, &dem_e);

                // initially my_e is set to max e of dem
                // set my_e = max(dem_e, lowest_e_in_nhood)

                type1 new_e = dem_e, lowest_e_in_nhood;
                if (border_cell)
                    lowest_e_in_nhood = 0;
                else {
                    int f = 1;
                    gma_cell_index in = gma_cell_first_neighbor(i);
                    for (int neighbor = 1; neighbor < 9; neighbor++) {
                        gma_cell_move_to_neighbor(in, neighbor);
                        type1 n_e;
                        b1->has_value(b1, block, in, &n_e);
                        if (f || n_e < lowest_e_in_nhood) {
                            f = 0;
                            lowest_e_in_nhood = n_e;
                        }
                    }
                }
                if (lowest_e_in_nhood > new_e)
                    new_e = lowest_e_in_nhood;

                type1 old_e = block->cell(i);
                if (new_e < old_e) {
                    block->cell(i) = new_e;
                    rv->add();
                }

            }
        }

        if (b1->last_block(block)) {
            fprintf(stderr, "%ld cells changed\n", rv->count_in_this_loop_of_band);
        }

        if (rv->count_in_this_loop_of_band)
            return 4;
        else
            return 2;
    }
    /*
      The D8 directions method, compute direction to lowest 8-neighbor

      neighbors:
      8 1 2
      7 x 3
      6 5 4
      
      case of nothing lower => flat = pseudo direction 10
      case of all higher => pit = pseudo direction 0
      
      if we are on global border and the cell is flat or pit,
      then set direction to out of the map
      
      todo: no data cells, mask?
      currently if two neighbors are equally lower, the first is picked
      
    */
    // b1 = fd, b2 = dem
    int m_D8(gma_block<type1> *block, gma_object_t**, gma_object_t*, int) {
        int border_block = b1->is_border_block(block);
        gma_cell_index i;
        for (i.y = 0; i.y < block->h(); i.y++) {
            for (i.x = 0; i.x < block->w(); i.x++) {
                int border_cell = block->is_border_cell(border_block, i);

                type2 my_elevation;
                b2->has_value(b1, block, i, &my_elevation);

                type2 lowest;
                int dir;
                int first = 1;

                gma_cell_index i_n = gma_cell_first_neighbor(i);
                for (int neighbor = 1; neighbor < 9; neighbor++) {
                    gma_cell_move_to_neighbor(i_n, neighbor);

                    type2 tmp;
                    if (!b2->has_value(b1, block, i_n, &tmp))
                        continue;

                    if (first || tmp < lowest) {
                        first = 0;
                        lowest = tmp;
                        dir = neighbor;
                    }

                }

                // is this flat area or a pit?
                if (first || lowest > my_elevation)
                    dir = 0;
                else if (lowest == my_elevation)
                    dir = 10;

                if (border_cell && (dir == 0 || dir == 10))
                    dir = border_cell;

                block->cell(i) = dir;

            }
        }
        return 2;
    }
public:
    virtual void assign(gma_band_t *band1, gma_band_t *band2) {
        b1 = (gma_band_p<type1>*)band1;
        b2 = (gma_band_p<type2>*)band2;
        callback cb;
        cb.fct = &gma_two_bands_p::m_assign;
        within_block_loop(cb);
    }
    virtual void add(gma_band_t *summand1, gma_band_t *summand2) {
        b1 = (gma_band_p<type1>*)summand1;
        b2 = (gma_band_p<type2>*)summand2;
        callback cb;
        cb.fct = &gma_two_bands_p::m_add;
        within_block_loop(cb);
    }
    virtual void subtract(gma_band_t *band1, gma_band_t *band2) {
        b1 = (gma_band_p<type1>*)band1;
        b2 = (gma_band_p<type2>*)band2;
        callback cb;
        cb.fct = &gma_two_bands_p::m_subtract;
        within_block_loop(cb);
    }
    virtual void multiply(gma_band_t *band1, gma_band_t *band2) {
        b1 = (gma_band_p<type1>*)band1;
        b2 = (gma_band_p<type2>*)band2;
        callback cb;
        cb.fct = &gma_two_bands_p::m_multiply;
        within_block_loop(cb);
    }
    virtual void divide(gma_band_t *band1, gma_band_t *band2) {
        b1 = (gma_band_p<type1>*)band1;
        b2 = (gma_band_p<type2>*)band2;
        callback cb;
        cb.fct = &gma_two_bands_p::m_divide;
        within_block_loop(cb);
    }
    virtual void modulus(gma_band_t *band1, gma_band_t *band2) {
        b1 = (gma_band_p<type1>*)band1;
        b2 = (gma_band_p<type2>*)band2;
        callback cb;
        cb.fct = &gma_two_bands_p::m_modulus;
        within_block_loop(cb);
    }

    virtual gma_hash_t *zonal_min(gma_band_t *band1, gma_band_t *zones) {
        b1 = (gma_band_p<type1>*)band1;
        b2 = (gma_band_p<type2>*)zones;
        callback cb;
        cb.fct = &gma_two_bands_p::m_zonal_min;
        within_block_loop(cb);
    }
    virtual gma_hash_t *zonal_max(gma_band_t *band1, gma_band_t *zones) {
        b1 = (gma_band_p<type1>*)band1;
        b2 = (gma_band_p<type2>*)zones;
        callback cb;
        cb.fct = &gma_two_bands_p::m_zonal_max;
        within_block_loop(cb);
    }
    virtual void rim_by8(gma_band_t *rims, gma_band_t *zones) {
        b1 = (gma_band_p<type1>*)rims;
        b2 = (gma_band_p<type2>*)zones;
        callback cb;
        cb.fct = &gma_two_bands_p::m_rim_by8;
        within_block_loop(cb);
    }
    virtual void fill_depressions(gma_band_t *filled_dem, gma_band_t *dem) {
        b1 = (gma_band_p<type1>*)filled_dem;
        b2 = (gma_band_p<type2>*)dem;

        double max_elev = b2->get_max()->value_as_double();
        b1->assign(max_elev);

        callback cb;
        cb.fct = &gma_two_bands_p::m_fill_depressions;
        within_block_loop(cb);
    }
    virtual void D8(gma_band_t *fd, gma_band_t *dem) {
        b1 = (gma_band_p<type1>*)fd;
        b2 = (gma_band_p<type2>*)dem;
        callback cb;
        cb.fct = &gma_two_bands_p::m_D8;
        within_block_loop(cb);
    }
    virtual void route_flats(gma_band_t *fd, gma_band_t *dem) {};
    virtual void upstream_area(gma_band_t *ua, gma_band_t *fd) {};
    virtual void catchment(gma_band_t *catchment, gma_band_t *fd, gma_cell_t *outlet) {};
};

template <> int gma_two_bands_p<uint8_t,float>::m_modulus(gma_block<uint8_t>*, gma_object_t**, gma_object_t*, int);
template <> int gma_two_bands_p<uint8_t,double>::m_modulus(gma_block<uint8_t>*, gma_object_t**, gma_object_t*, int);
template <> int gma_two_bands_p<uint16_t,float>::m_modulus(gma_block<uint16_t>*, gma_object_t**, gma_object_t*, int);
template <> int gma_two_bands_p<uint16_t,double>::m_modulus(gma_block<uint16_t>*, gma_object_t**, gma_object_t*, int);
template <> int gma_two_bands_p<int16_t,float>::m_modulus(gma_block<int16_t>*, gma_object_t**, gma_object_t*, int);
template <> int gma_two_bands_p<int16_t,double>::m_modulus(gma_block<int16_t>*, gma_object_t**, gma_object_t*, int);
template <> int gma_two_bands_p<uint32_t,float>::m_modulus(gma_block<uint32_t>*, gma_object_t**, gma_object_t*, int);
template <> int gma_two_bands_p<uint32_t,double>::m_modulus(gma_block<uint32_t>*, gma_object_t**, gma_object_t*, int);
template <> int gma_two_bands_p<int32_t,float>::m_modulus(gma_block<int32_t>*, gma_object_t**, gma_object_t*, int);
template <> int gma_two_bands_p<int32_t,double>::m_modulus(gma_block<int32_t>*, gma_object_t**, gma_object_t*, int);
template <> int gma_two_bands_p<float,float>::m_modulus(gma_block<float>*, gma_object_t**, gma_object_t*, int);
template <> int gma_two_bands_p<float,double>::m_modulus(gma_block<float>*, gma_object_t**, gma_object_t*, int);
template <> int gma_two_bands_p<float,uint8_t>::m_modulus(gma_block<float>*, gma_object_t**, gma_object_t*, int);
template <> int gma_two_bands_p<float,uint16_t>::m_modulus(gma_block<float>*, gma_object_t**, gma_object_t*, int);
template <> int gma_two_bands_p<float,int16_t>::m_modulus(gma_block<float>*, gma_object_t**, gma_object_t*, int);
template <> int gma_two_bands_p<float,uint32_t>::m_modulus(gma_block<float>*, gma_object_t**, gma_object_t*, int);
template <> int gma_two_bands_p<float,int32_t>::m_modulus(gma_block<float>*, gma_object_t**, gma_object_t*, int);
template <> int gma_two_bands_p<double,float>::m_modulus(gma_block<double>*, gma_object_t**, gma_object_t*, int);
template <> int gma_two_bands_p<double,double>::m_modulus(gma_block<double>*, gma_object_t**, gma_object_t*, int);
template <> int gma_two_bands_p<double,uint8_t>::m_modulus(gma_block<double>*, gma_object_t**, gma_object_t*, int);
template <> int gma_two_bands_p<double,uint16_t>::m_modulus(gma_block<double>*, gma_object_t**, gma_object_t*, int);
template <> int gma_two_bands_p<double,int16_t>::m_modulus(gma_block<double>*, gma_object_t**, gma_object_t*, int);
template <> int gma_two_bands_p<double,uint32_t>::m_modulus(gma_block<double>*, gma_object_t**, gma_object_t*, int);
template <> int gma_two_bands_p<double,int32_t>::m_modulus(gma_block<double>*, gma_object_t**, gma_object_t*, int);


template <typename type1,typename type2> CPLErr gma_band_iteration(gma_band_p<type1> **band1, gma_band_p<type2> **band2) {
    GDALDataset *ds1 = (*band1)->dataset();
    GDALDriver *d = ds1->GetDriver();
    char **files = ds1->GetFileList();

    // flush and close band1
    ds1->FlushCache();
    delete ds1;

    // rename band1 to files[0]."_tmp"
    char *newpath = (char*)CPLMalloc(strlen(files[0])+5);
    strcpy(newpath, files[0]);
    strcat(newpath, "_tmp");
    int e = VSIRename(files[0], newpath);

    // reopen old b1 as b2
    (*band2)->empty_cache();
    GDALDataset *ds2 = (GDALDataset*)GDALOpen(newpath, GA_ReadOnly);
    *band2 = new gma_band_p<type2>(ds2->GetRasterBand(1));

    // create new b1
    (*band1)->empty_cache();
    ds1 = d->Create(files[0], (*band1)->w(), (*band1)->h(), 1, (*band1)->gdal_datatype(), NULL);
    *band1 = new gma_band_p<type1>(ds1->GetRasterBand(1));

    CPLFree(newpath);
    CSLDestroy(files);
}
