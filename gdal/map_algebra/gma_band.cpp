#include "gdal_map_algebra_private.h"

gma_block *gma_block_create() {
    gma_block *block;
    block = (gma_block *)CPLMalloc(sizeof(gma_block));
    block->_block = NULL;
    return block;
}

void gma_block_destroy(gma_block *block) {
    if (block->_block) CPLFree(block->_block);
    CPLFree(block);
}

gma_block_cache gma_cache_initialize() {
    gma_block_cache cache;
    cache.n = 0;
    cache.blocks = NULL;
    return cache;
}

void gma_empty_cache(gma_block_cache *cache) {
    if (cache->n == 0)
        return;
    for (int i = 0; i < cache->n; i++)
        gma_block_destroy(cache->blocks[i]);
    CPLFree(cache->blocks);
    cache->n = 0;
    cache->blocks = NULL;
}

void gma_cache_remove(gma_block_cache *cache, int i) {
    if (i < 0 || i >= cache->n) return;
    gma_block **blocks = (gma_block **)CPLMalloc((cache->n-1) * sizeof(gma_block*));
    int d = 0;
    for (int j = 0; j < cache->n; j++) {
        if (j == i) {
            gma_block_destroy(cache->blocks[j]);
            d = 1;
        } else {
            blocks[j-d] = cache->blocks[j];
        }
    }
    CPLFree(cache->blocks);
    cache->n = cache->n-1;
    cache->blocks = blocks;
}

gma_block *gma_cache_retrieve(gma_block_cache cache, gma_block_index index) {
    for (int i = 0; i < cache.n; i++)
        if (cache.blocks[i]->index.x == index.x && cache.blocks[i]->index.y == index.y)
            return cache.blocks[i];
    return NULL;
}

CPLErr gma_cache_add(gma_block_cache *cache, gma_block *block) {
    cache->n++;
    if (cache->n == 1)
        cache->blocks = (gma_block**)CPLMalloc(sizeof(gma_block*));
    else
        cache->blocks = (gma_block**)CPLRealloc(cache->blocks, cache->n * sizeof(gma_block*));
    cache->blocks[cache->n-1] = block;
}

gma_band gma_band_initialize(GDALRasterBand *b) {
    gma_band band;
    band.band = b;
    band.w = b->GetXSize();
    band.h = b->GetYSize();
    b->GetBlockSize(&band.w_block, &band.h_block);
    band.w_blocks = (band.w + band.w_block - 1) / band.w_block;
    band.h_blocks = (band.h + band.h_block - 1) / band.h_block;
    band.datatype = b->GetRasterDataType();
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

void gma_band_empty_cache(gma_band *band) {
    gma_empty_cache(&band->cache);
}

CPLErr gma_band_iteration(gma_band *band1, gma_band *band2) {
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
    *band2 = gma_band_initialize(ds2->GetRasterBand(1));

    // create new b1
    gma_band_empty_cache(band1);
    ds1 = d->Create(files[0], band1->w, band1->h, 1, band1->datatype, NULL);
    *band1 = gma_band_initialize(ds1->GetRasterBand(1));

    CPLFree(newpath);
    CSLDestroy(files);
}

void gma_band_set_block_size(gma_band band, gma_block *block) {
    block->w = ( (block->index.x+1) * band.w_block > band.w ) ? band.w - block->index.x * band.w_block : band.w_block;
    block->h = ( (block->index.y+1) * band.h_block > band.h ) ? band.h - block->index.y * band.h_block : band.h_block;
}

gma_block *gma_band_get_block(gma_band band, gma_block_index i) {
    for (int j = 0; j < band.cache.n; j++)
        if (band.cache.blocks[j]->index.x == i.x && band.cache.blocks[j]->index.y == i.y) {
            return band.cache.blocks[j];
        }
    return NULL;
}

CPLErr gma_band_write_block(gma_band band, gma_block *block) {
    return band.band->WriteBlock(block->index.x, block->index.y, block->_block);
}

CPLErr gma_band_add_to_cache(gma_band *band, gma_block_index i) {
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

CPLErr gma_band_update_cache(gma_band *band2,
                             gma_band band1,
                             gma_block *b1, int d) {

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
gma_block *gma_index12index2(gma_band band1, 
                             gma_block *b1,
                             gma_cell_index i1, 
                             gma_band band2,
                             gma_cell_index *i2) {
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

template<typename other_datatype>
int gma_value_from_other_band(gma_band this_band, 
                              gma_block *this_block,
                              gma_cell_index this_index,
                              gma_band other_band,
                              other_datatype *value) {
    gma_cell_index other_index;
    gma_block *other_block = gma_index12index2(this_band, this_block, this_index, other_band, &other_index);
    if (other_block) {
        *value = gma_block_cell(other_datatype, other_block, other_index);
        return 1;
    } else
        return 0;
}

#define gma_value_from_other_band_instantiation(type)   \
    template int gma_value_from_other_band<type>(       \
        gma_band this_band, gma_block *this_block,      \
        gma_cell_index this_index,                      \
        gma_band other_band,                            \
        type *value)

gma_value_from_other_band_instantiation(uint8_t);
gma_value_from_other_band_instantiation(uint16_t);
gma_value_from_other_band_instantiation(int16_t);
gma_value_from_other_band_instantiation(uint32_t);
gma_value_from_other_band_instantiation(int32_t);
gma_value_from_other_band_instantiation(float);
gma_value_from_other_band_instantiation(double);

int is_border_block(gma_band band, gma_block *block) {
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

int is_border_cell(gma_block *block, int border_block, gma_cell_index i) {
    if (!border_block)
        return 0;
    if (i.x == 0) {
        if (i.y == 0 && border_block == 1)
            return 8;
        else if (i.y == block->h - 1 && border_block == 6)
            return 6;
        else if (border_block == 8 || border_block == 6 || border_block == 7)
            return 7;
    } else if (i.x == block->w - 1) {
        if (i.y == 0 && border_block == 2)
            return 2;
        else if (i.y == block->h - 1 && border_block == 4)
            return 4;
        else if (border_block == 2 || border_block == 4 || border_block == 3)
            return 3;
    } else if (i.y == 0 && (border_block == 8 || border_block == 2 || border_block == 1))
        return 1;
    else if (i.y == block->h - 1 && (border_block == 6 || border_block == 4 || border_block == 5))
        return 5;
    else
        return 0;
}
