#include "gdal_map_algebra.h"
#include "gma_hash.h"
#include "type_switch.h"
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
} gma_cell_index; // cell coordinates in block

typedef struct {
    gma_block_index index;
    int w; // width of data in block
    int h; // height of data in block
    void *_block;
} gma_block;

gma_block *gma_block_create();
void gma_block_destroy(gma_block *block);

#define gma_typecast(type, var) ((type*)(var))
#define gma_block_cell(type, block, cell) gma_typecast(type, block->_block)[cell.x+cell.y*block->w]

typedef struct {
    size_t n;
    gma_block **blocks;
} gma_block_cache;

gma_block_cache gma_cache_initialize();
void gma_empty_cache(gma_block_cache *cache);
void gma_cache_remove(gma_block_cache *cache, int i);

gma_block *gma_cache_retrieve(gma_block_cache cache, gma_block_index index);
CPLErr gma_cache_add(gma_block_cache *cache, gma_block *block);

// fixme: make this template struct and add nodata value and bool has_nodata
// add also information about mask band
template <typename datatype_t> struct gma_band {
    GDALRasterBand *band;
    GDALRasterBand *mask;
    int w;
    int h;
    int w_block;
    int h_block;
    int w_blocks;
    int h_blocks;
    GDALDataType datatype;
    int datatype_size;
    gma_block_cache cache;
    datatype_t nodata;
    bool has_nodata;
};

#define gma_is_nodata(type,band,block,cell_index) band.has_nodata && gma_block_cell(type, block, cell_index) == band.nodata

#define gma_is_nodata_value(type,band,value) band.has_nodata && value == band.nodata

template <typename datatype> gma_band<datatype> gma_band_initialize(GDALRasterBand *b) {
    gma_band<datatype> band;
    band.band = b;
    band.w = b->GetXSize();
    band.h = b->GetYSize();
    b->GetBlockSize(&band.w_block, &band.h_block);
    band.w_blocks = (band.w + band.w_block - 1) / band.w_block;
    band.h_blocks = (band.h + band.h_block - 1) / band.h_block;
    band.datatype = b->GetRasterDataType();
    int has_nodata;
    double nodata = b->GetNoDataValue(&has_nodata);
    band.has_nodata = has_nodata != 0;
    band.nodata = band.has_nodata ? (datatype)nodata : 0;
    int mask_flags = b->GetMaskFlags();
    if (mask_flags | GMF_PER_DATASET || mask_flags | GMF_ALPHA)
        band.mask = b->GetMaskBand();
    else
        band.mask = NULL;
    switch (band.datatype) {
    case GDT_Byte:
        band.datatype_size = sizeof(uint8_t);
        break;
    case GDT_UInt16:
        band.datatype_size = sizeof(uint16_t);
        break;
    case GDT_Int16:
        band.datatype_size = sizeof(int16_t);
        break;
    case GDT_UInt32:
        band.datatype_size = sizeof(uint32_t);
        break;
    case GDT_Int32:
        band.datatype_size = sizeof(int32_t);
        break;
    case GDT_Float32:
        band.datatype_size = sizeof(float);
        break;
    case GDT_Float64:
        band.datatype_size = sizeof(double);
        break;
    default:
        fprintf(stderr, "datatype not supported");
    }
    band.cache = gma_cache_initialize();
    return band;
}

template <typename datatype> void gma_band_empty_cache(gma_band<datatype> *band) {
    gma_empty_cache(&band->cache);
}

template <typename type1,typename type2> CPLErr gma_band_iteration(gma_band<type1> *band1, gma_band<type2> *band2) {
    GDALDataset *ds1 = band1->band->GetDataset();
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
    gma_band_empty_cache(band2);
    GDALDataset *ds2 = (GDALDataset*)GDALOpen(newpath, GA_ReadOnly);
    *band2 = gma_band_initialize<type2>(ds2->GetRasterBand(1));

    // create new b1
    gma_band_empty_cache(band1);
    ds1 = d->Create(files[0], band1->w, band1->h, 1, band1->datatype, NULL);
    *band1 = gma_band_initialize<type1>(ds1->GetRasterBand(1));

    CPLFree(newpath);
    CSLDestroy(files);
}

template <typename datatype> void gma_band_set_block_size(gma_band<datatype> band, gma_block *block) {
    block->w = ( (block->index.x+1) * band.w_block > band.w ) ? band.w - block->index.x * band.w_block : band.w_block;
    block->h = ( (block->index.y+1) * band.h_block > band.h ) ? band.h - block->index.y * band.h_block : band.h_block;
}

template <typename datatype>
gma_block *gma_band_get_block(gma_band<datatype> band, gma_block_index i) {
    for (int j = 0; j < band.cache.n; j++)
        if (band.cache.blocks[j]->index.x == i.x && band.cache.blocks[j]->index.y == i.y) {
            return band.cache.blocks[j];
        }
    return NULL;
}

template <typename datatype> CPLErr gma_band_write_block(gma_band<datatype> band, gma_block *block) {
    return band.band->WriteBlock(block->index.x, block->index.y, block->_block);
}

template <typename datatype> CPLErr gma_band_add_to_cache(gma_band<datatype> *band, gma_block_index i) {
    gma_block *b = gma_cache_retrieve(band->cache, i);
    if (!b) {
        b = gma_block_create();
        b->index = i;
        gma_band_set_block_size(*band, b);
        b->_block = CPLMalloc(band->w_block * band->h_block * band->datatype_size);
        CPLErr e = band->band->ReadBlock(b->index.x, b->index.y, b->_block);
        gma_cache_add(&(band->cache), b);
    }
}

template <typename type1,typename type2>
CPLErr gma_band_update_cache(gma_band<type2> *band2, gma_band<type1> band1, gma_block *b1, int d) {

    // which blocks in band 2 are needed to cover a block extended with focal distance d in band 1?
    // assuming the raster size if the same

    gma_block_index i20, i21;

    // index of top left cell to be covered
    int x10 = b1->index.x * band1.w_block - d, y10 = b1->index.y * band1.h_block - d;

    // index of bottom right cell to be covered
    int x11 = x10 + d + b1->w-1 + d, y11 = y10 + d + b1->h-1 + d;

    // which block covers x10, y10 in band2?
    i20.x = MAX(x10 / band2->w_block, 0);
    i20.y = MAX(y10 / band2->h_block, 0);

    // which block covers x11, y11 in band2?
    i21.x = MIN(x11 / band2->w_block, band2->w_blocks-1);
    i21.y = MIN(y11 / band2->h_block, band2->h_blocks-1);

    {
        // add needed blocks
        gma_block_index i;
        for (i.y = i20.y; i.y <= i21.y; i.y++) {
            for (i.x = i20.x; i.x <= i21.x; i.x++) {
                gma_band_add_to_cache(band2, i);
            }
        }
    }
    {
        // remove unneeded blocks
        int i = 0;
        while (i < band2->cache.n) {
            if (band2->cache.blocks[i]->index.x < i20.x || band2->cache.blocks[i]->index.x > i21.x ||
                band2->cache.blocks[i]->index.y < i20.y || band2->cache.blocks[i]->index.y > i21.y)
                gma_cache_remove(&(band2->cache), i);
            else
                i++;
        }
    }

}

// get the block, which has the cell, that is pointed to by i1 in block1
// return the index within the block
template <typename type1,typename type2>
gma_block *gma_index12index2(gma_band<type1> band1, gma_block *b1, gma_cell_index i1, gma_band<type2> band2, gma_cell_index *i2) {
    // global cell index
    int x = b1->index.x * band1.w_block + i1.x;
    int y = b1->index.y * band1.h_block + i1.y;
    if (x < 0 || y < 0 || x >= band1.w || y >= band1.h)
        return NULL;
    // index of block 2
    int x2 = x / band2.w_block;
    int y2 = y / band2.h_block;
    for (int i = 0; i < band2.cache.n; i++)
        if (band2.cache.blocks[i]->index.x == x2 && band2.cache.blocks[i]->index.y == y2) {
            i2->x = x % band2.w_block;
            i2->y = y % band2.h_block;
            return band2.cache.blocks[i];
        }
    return NULL;
}

// fixme: return false if cell is nodata cell or masked off (alpha == 0)
template<typename this_datatype, typename other_datatype>
int gma_value_from_other_band(gma_band<this_datatype> this_band, 
                              gma_block *this_block,
                              gma_cell_index this_index,
                              gma_band<other_datatype> other_band,
                              other_datatype *value) {
    gma_cell_index other_index;
    gma_block *other_block = gma_index12index2<this_datatype,other_datatype>(this_band, this_block, this_index, other_band, &other_index);
    if (other_block) {
        *value = gma_block_cell(other_datatype, other_block, other_index);
        return 1;
    } else
        return 0;
}

#define gma_first_block(block) block->index.x == 0 && block->index.y == 0
#define gma_last_block(band, block) block->index.x == band.w_blocks-1 && block->index.y == band.h_blocks-1

template <typename datatype> int is_border_block(gma_band<datatype> band, gma_block *block) {
    if (block->index.x == 0) {
        if (block->index.y == 0)
            return 8;
        else if (block->index.y == band.h_blocks - 1)
            return 6;
        else
            return 7;
    } else if (block->index.x == band.w_blocks - 1) {
        if (block->index.y == 0)
            return 2;
        else if (block->index.y == band.h_blocks - 1)
            return 4;
        else
            return 3;
    } else if (block->index.y == 0)
        return 1;
    else if (block->index.y == band.h_blocks - 1)
        return 5;
    return 0;
}

int is_border_cell(gma_block *block, int border_block, gma_cell_index i);

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

#define COMMA ,
#define gma_retval_init(class, var, arg)        \
    class *var;                                 \
    if (*retval == NULL) {                      \
        var = new class(arg);                   \
        *retval = (gma_object_t*)var;           \
    } else                                      \
        var = (class *)*retval;
