#include "gdal_map_algebra.hpp"
#include "hash.hpp"
#include "type_switch.hpp"
#include <math.h>

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

template <typename datatype> class gma_band {
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
    gma_band<uint8_t> *mask;
public:
    int w_blocks;
    int h_blocks;
    gma_band(GDALRasterBand *b) {
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
            if (m) mask = new gma_band<uint8_t>(m);
        }   
        switch (m_gdal_datatype) {
        case GDT_Byte:
            datatype_size = sizeof(uint8_t);
            break;
        case GDT_UInt16:
            datatype_size = sizeof(uint16_t);
            break;
        case GDT_Int16:
            datatype_size = sizeof(int16_t);
            break;
        case GDT_UInt32:
            datatype_size = sizeof(uint32_t);
            break;
        case GDT_Int32:
            datatype_size = sizeof(int32_t);
            break;
        case GDT_Float32:
            datatype_size = sizeof(float);
            break;
        case GDT_Float64:
            datatype_size = sizeof(double);
            break;
        default:
            fprintf(stderr, "datatype not supported");
        }
    }
    ~gma_band() {
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
    GDALDataType gdal_datatype() {
        return m_gdal_datatype;
    }
    void empty_cache() {
        cache.empty();
        if (mask) mask->empty_cache();
    }
    void set_block_size(gma_block<datatype> *block) {
        
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
    template <typename type1> CPLErr update_cache(gma_band<type1> *band1, gma_block<type1> *b1, int d) {

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
    template <typename type2> gma_block<datatype> *get_block(gma_band<type2> *band2, gma_block<type2> *b2, gma_cell_index i2, gma_cell_index *i1) {
        gma_cell_index gi = band2->global_cell_index(b2, i2);
        if (cell_is_outside(gi)) return NULL;
        gma_block_index i = block_index(gi);
        gma_block<datatype> *rv = get_block(i);
        if (rv) *i1 = cell_index(gi);
        return rv;
    }
    // returns false if cell is not on the band or is nodata cell
    template<typename type2> bool has_value(gma_band<type2> *band2, gma_block<type2> *block2, gma_cell_index index2, datatype *value) {
        gma_cell_index index;
        gma_block<datatype> *block = get_block(band2, block2, index2, &index);
        if (block) {
            if (cell_is_nodata(block, index)) return false;
            *value = block->cell(index);
            return true;
        } else
            return false;
    }
};

template <typename type1,typename type2> CPLErr gma_band_iteration(gma_band<type1> **band1, gma_band<type2> **band2) {
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
    *band2 = new gma_band<type2>(ds2->GetRasterBand(1));

    // create new b1
    (*band1)->empty_cache();
    ds1 = d->Create(files[0], (*band1)->w(), (*band1)->h(), 1, (*band1)->gdal_datatype(), NULL);
    *band1 = new gma_band<type1>(ds1->GetRasterBand(1));

    CPLFree(newpath);
    CSLDestroy(files);
}

#define COMMA ,
#define gma_retval_init(class, var, arg)        \
    class *var;                                 \
    if (*retval == NULL) {                      \
        var = new class(arg);                   \
        *retval = (gma_object_t*)var;           \
    } else                                      \
        var = (class *)*retval;
