typedef struct {
    int x;
    int y;
} gma_block_index; // block coordinates

typedef struct {
    int x;
    int y;
} gma_cell_index; // cell coordinates in block

typedef struct {
    GDALRasterBand *band;
    int w;
    int h;
    int w_block;
    int h_block;
    int w_blocks;
    int h_blocks;
    int size_of_data_type;
} gma_band;

gma_band gma_band_initialize(GDALRasterBand *b) {
    gma_band band;
    band.band = b;
    band.w = b->GetXSize();
    band.h = b->GetYSize();
    b->GetBlockSize(&band.w_block, &band.h_block);
    band.w_blocks = (band.w + band.w_block - 1) / band.w_block;
    band.h_blocks = (band.h + band.h_block - 1) / band.h_block;
    switch (b->GetRasterDataType()) {
    case GDT_Byte:
        band.size_of_data_type = sizeof(char);
        break;
    case GDT_UInt16:
        band.size_of_data_type = sizeof(char)*2;
        break;
    case GDT_Int16:
        band.size_of_data_type = sizeof(char)*2;
        break;
    case GDT_UInt32:
        band.size_of_data_type = sizeof(char)*4;
        break;
    case GDT_Int32:
        band.size_of_data_type = sizeof(char)*4;
        break;
    case GDT_Float32:
        band.size_of_data_type = sizeof(char)*4;
        break;
    case GDT_Float64:
        band.size_of_data_type = sizeof(char)*8;
        break;
    case GDT_CInt16:
        band.size_of_data_type = sizeof(char)*2*2;
        break;
    case GDT_CInt32:
        band.size_of_data_type = sizeof(char)*4*2;
        break;
    case GDT_CFloat32:
        band.size_of_data_type = sizeof(char)*4*2;
        break;
    case GDT_CFloat64:
        band.size_of_data_type = sizeof(char)*8*2;
        break;
    }
    return band;
}

typedef struct {
    gma_block_index index;
    int w; // width of data in block
    int h; // height of data in block
    void *block;
} gma_block;

CPLErr gma_block_read(gma_block *block, gma_band band) {
    block->block = CPLMalloc(band.w_block * band.h_block * band.size_of_data_type);
    CPLErr e = band.band->ReadBlock(block->index.x, block->index.y, block->block);
    block->w = ( (block->index.x+1) * band.w_block > band.w ) ? band.w - block->index.x * band.w_block : band.w_block;
    block->h = ( (block->index.y+1) * band.h_block > band.h ) ? band.h - block->index.x * band.h_block : band.h_block;
}

typedef struct {
    size_t n;
    gma_block *blocks;
} gma_block_cache;

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
        CPLFree(cache->blocks[i].block);
    CPLFree(cache->blocks);
    cache->n = 0;
    cache->blocks = NULL;
}

void gma_remove_from_cache(gma_block_cache *cache, int i) {
    gma_block *blocks = (gma_block *)CPLMalloc((cache->n-1) * sizeof(gma_block));
    int d = 0;
    for (int j = 0; j < cache->n; j++) {
        if (j == i) {
            CPLFree(cache->blocks[j].block);
            d = 1;
            continue;
        }
        blocks[j-d] = cache->blocks[j];
    }
    CPLFree(cache->blocks);
    cache->n = cache->n-1;
    cache->blocks = blocks;
}

int gma_cache_contains(gma_block_cache cache, gma_block_index index) {
    for (int i = 0; i < cache.n; i++)
        if (cache.blocks[i].index.x == index.x && cache.blocks[i].index.y == index.y)
            return 1;
    return 0;
}

CPLErr gma_add_to_cache(gma_block_cache *cache, gma_block block) {
    cache->n++;
    if (cache->n == 1)
        cache->blocks = (gma_block*)CPLMalloc(sizeof(gma_block));
    else
        cache->blocks = (gma_block*)CPLRealloc(cache->blocks, cache->n * sizeof(gma_block));
    cache->blocks[cache->n-1] = block;
}

// which blocks in band 2 are needed to cover a block extended with focal distance d in band 1?
// assuming the raster size if the same
void gma_blocks_in_band2(gma_band band1, gma_band band2,
                         gma_block b1, int d, gma_block_index *i20, gma_block_index *i21) {

    // index of top left cell to be covered
    int x10 = b1.index.x * band1.w_block - d, y10 = b1.index.y * band1.h_block + d;

    // index of bottom right cell to be covered
    int x11 = x10 + b1.w-1 + d, y11 = y10 + b1.h-1 + d;

    // which block covers x10, y10 in band2?
    i20->x = x10 / band2.w_block;
    i20->y = y10 / band2.h_block;

    // which block covers x11, y11 in band2?
    i21->x = x11 / band2.w_block;
    i21->y = y11 / band2.h_block;
}

CPLErr gma_update_block_cache(gma_block_cache *cache,
                              gma_band band1, gma_band band2,
                              GDALRasterBand *b2, gma_block b1, int d) {
    gma_block block2;
    gma_block_index i20, i21;
    gma_blocks_in_band2(band1, band2, b1, d, &i20, &i21);
    for (block2.index.y = i20.y; block2.index.y <= i21.y; block2.index.y++) {
        for (block2.index.x = i20.x; block2.index.x <= i21.x; block2.index.x++) {
            if (!gma_cache_contains(*cache, block2.index)) {
                gma_block_read(&block2, band2);
                gma_add_to_cache(cache, block2);
            }
        }
    }
    // remove unneeded blocks
    int i = 0;
    while (i < cache->n) {
        if (cache->blocks[i].index.x < i20.x || cache->blocks[i].index.x > i21.x ||
            cache->blocks[i].index.y < i20.y || cache->blocks[i].index.y > i21.y)
            gma_remove_from_cache(cache, i);
        else
            i++;
    }

}

// get the block, which has the cell, that is pointed to by i1 in block1
// return the index within the block
gma_cell_index gma_index12index2(gma_block_cache cache, gma_band band1, gma_band band2,
                                 gma_block b1, gma_block *b2,
                                 gma_cell_index i1) {
    int x = b1.index.x * band1.w_block + i1.x;
    int y = b1.index.y * band1.h_block + i1.y;
    gma_cell_index i2 = { .x = x / band2.w_block, .y = y / band2.h_block };
    for (int i = 0; i < cache.n; i++)
        if (cache.blocks[i].index.x == i2.x && cache.blocks[i].index.y == i2.y)
            *b2 = cache.blocks[i];
    i2.x = x % band2.w_block;
    i2.y = y % band2.h_block;
    return i2;
}

typedef int (*gma_two_bands_callback)(gma_block_cache, gma_band, gma_band, gma_block);

#define gma_add_band(type1,type2) int gma_add_band_##type1##type2(gma_block_cache cache, gma_band band1, gma_band band2, gma_block block1) { \
        gma_cell_index i1;                                              \
        for (i1.y = 0; i1.y < block1.h; i1.y++) {                       \
            for (i1.x = 0; i1.x < block1.w; i1.x++) {                   \
                gma_block block2;                                       \
                gma_cell_index i2 = gma_index12index2(cache, band1, band2, block1, &block2, i1); \
                gma_typecast(type1, block1.block)[i1.x+i1.y*block1.w] += \
                    gma_typecast(type2, block2.block)[i2.x+i2.y*block2.w]; \
            }                                                           \
        }                                                               \
        return 2;                                                       \
    }

gma_add_band(int16_t,int16_t)
gma_add_band(int32_t,int32_t)
gma_add_band(double,double)

template<typename data1_t, typename data2_t>
int gma_D8(gma_block_cache cache, gma_band band1, gma_band band2, gma_block block1) {
    gma_cell_index i1;
    for (i1.y = 0; i1.y < block1.h; i1.y++) {
        for (i1.x = 0; i1.x < block1.w; i1.x++) {

            int d = 0;
            gma_cell_index i1n;
            int first = 1;
            data2_t lowest;
            int dir;
            for (i1n.y = i1.y-1; i1n.y < i1.y+2; i1n.y++) {
                for (i1n.x = i1.x-1; i1n.x < i1.x+2; i1n.x++) {

                    gma_block block2;
                    gma_cell_index i2 = gma_index12index2(cache, band1, band2, block1, &block2, i1n);
                    data2_t tmp = gma_typecast(data2_t, block2.block)[i2.x+i2.y*block2.w];
                    if (first || tmp < lowest) {
                        first = 0;
                        lowest = tmp;
                        dir = d;
                    }

                    d++;
                }
            }

            gma_typecast(data1_t, block1.block)[i1.x+i1.y*block1.w] = dir;

        }
    }
    return 2;
}

template<typename data1_t, typename data2_t>
void gma_two_bands_proc(GDALRasterBand *b1, gma_two_bands_callback cb, GDALRasterBand *b2, int focal_distance) {

    gma_block_cache cache = gma_cache_initialize();
    gma_band band1 = gma_band_initialize(b1), band2 = gma_band_initialize(b2);
    gma_block block1;

    for (block1.index.y = 0; block1.index.y < band1.h_blocks; block1.index.y++) {
        for (block1.index.x = 0; block1.index.x < band1.w_blocks; block1.index.x++) {

            gma_block_read(&block1, band1);

            CPLErr e = gma_update_block_cache(&cache, band1, band2, b2, block1, focal_distance);

            int ret = cb(cache, band1, band2, block1);
            switch (ret) {
            case 0: return;
            case 1: break;
            case 2: {
                e = b1->WriteBlock(block1.index.x, block1.index.y, block1.block);
            }
            }
            CPLFree(block1.block);
        }
    }
}


void gma_two_bands(GDALRasterBand *b1, gma_two_bands_method_t method, GDALRasterBand *b2) {
    // b1 is changed, b2 is not
    // assuming b1 has same size as b2
/*
    int w_band = b1->GetXSize(), h_band = b1->GetYSize();

    check:
    int w_band == b2->GetXSize(), h_band == b2->GetYSize();
*/
    switch (method) {
    case gma_method_add_band: {
        // assuming b1 and b2 have same data type
        switch (b1->GetRasterDataType()) {
        case GDT_Int16: {
            gma_two_bands_proc<int16_t,int16_t>(b1, gma_add_band_int16_tint16_t, b2, 0);
            break;
        }
        case GDT_Int32: {
            gma_two_bands_proc<int32_t,int32_t>(b1, gma_add_band_int32_tint32_t, b2, 0);
            break;
        }
        case GDT_Float64: {
            gma_two_bands_proc<double,double>(b1, gma_add_band_doubledouble, b2, 0);
            break;
        }
        }
        break;
    }
    case gma_method_D8: {
        // compute flow directions from DEM
        switch (b1->GetRasterDataType()) {
        case GDT_Byte: {
            switch (b2->GetRasterDataType()) {
            case GDT_UInt16:
                gma_two_bands_proc<unsigned char,uint16_t>(b1, gma_D8<unsigned char,uint16_t>, b2, 1);
                break;
            case GDT_Float64:
                gma_two_bands_proc<char,double>(b1, gma_D8<char,double>, b2, 1);
                break;
            }
            break;
        }
        }
        break;
    }
    }
}
