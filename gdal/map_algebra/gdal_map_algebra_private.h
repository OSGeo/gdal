#include "gdal_map_algebra.h"
#include "gma_hash.h"
#include "type_switch.h"

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

typedef struct {
    GDALRasterBand *band;
    int w;
    int h;
    int w_block;
    int h_block;
    int w_blocks;
    int h_blocks;
    GDALDataType datatype;
    int datatype_size;
    gma_block_cache cache;
} gma_band;

gma_band gma_band_initialize(GDALRasterBand *b);
void gma_band_empty_cache(gma_band *band);
CPLErr gma_band_iteration(gma_band *band1, gma_band *band2);
void gma_band_set_block_size(gma_band band, gma_block *block);
gma_block *gma_band_get_block(gma_band band, gma_block_index i);
CPLErr gma_band_write_block(gma_band band, gma_block *block);
CPLErr gma_band_add_to_cache(gma_band *band, gma_block_index i);
CPLErr gma_band_update_cache(gma_band *band2, gma_band band1, gma_block *b1, int d);
gma_block *gma_index12index2(gma_band band1, gma_block *b1, gma_cell_index i1, gma_band band2, gma_cell_index *i2);
template<typename other_datatype>
int gma_value_from_other_band(gma_band this_band, 
                              gma_block *this_block,
                              gma_cell_index this_index,
                              gma_band other_band,
                              other_datatype *value);

#define gma_first_block(block) block->index.x == 0 && block->index.y == 0
#define gma_last_block(band, block) block->index.x == band.w_blocks-1 && block->index.y == band.h_blocks-1

int is_border_block(gma_band band, gma_block *block);
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
