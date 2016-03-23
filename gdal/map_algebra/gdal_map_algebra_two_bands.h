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
    GDALDataType data_type;
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
    band.data_type = b->GetRasterDataType();
    // there should be a way to not need this?
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
    block->h = ( (block->index.y+1) * band.h_block > band.h ) ? band.h - block->index.y * band.h_block : band.h_block;
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
    int x10 = b1.index.x * band1.w_block - d, y10 = b1.index.y * band1.h_block - d;

    // index of bottom right cell to be covered
    int x11 = x10 + b1.w-1 + d, y11 = y10 + b1.h-1 + d;

    // which block covers x10, y10 in band2?
    i20->x = MAX(x10 / band2.w_block, 0);
    i20->y = MAX(y10 / band2.h_block, 0);

    // which block covers x11, y11 in band2?
    i21->x = MIN(x11 / band2.w_block, band2.w_blocks-1);
    i21->y = MIN(y11 / band2.h_block, band2.h_blocks-1);
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
int gma_index12index2(gma_block_cache cache, gma_band band1, gma_band band2,
                      gma_block b1, gma_block *b2,
                      gma_cell_index i1, gma_cell_index *i2) {
    // global cell index
    int x = b1.index.x * band1.w_block + i1.x;
    int y = b1.index.y * band1.h_block + i1.y;
    if (x < 0 || y < 0 || x >= band1.w || y >= band1.h)
        return 0;
    // index of block 2
    int x2 = x / band2.w_block;
    int y2 = y / band2.h_block;
    for (int i = 0; i < cache.n; i++)
        if (cache.blocks[i].index.x == x2 && cache.blocks[i].index.y == y2) {
            *b2 = cache.blocks[i];
            i2->x = x % band2.w_block;
            i2->y = y % band2.h_block;
            return 1;
        }
    return 0;
}

typedef int (*gma_two_bands_callback)(gma_block_cache, gma_band, gma_band, gma_block, void *);

// can we use a template and not macro?
#define gma_add_band(type1,type2) int gma_add_band_##type1##type2(gma_block_cache cache, gma_band band1, gma_band band2, gma_block block1, void *) { \
        gma_cell_index i1;                                              \
        for (i1.y = 0; i1.y < block1.h; i1.y++) {                       \
            for (i1.x = 0; i1.x < block1.w; i1.x++) {                   \
                gma_block block2;                                       \
                gma_cell_index i2;                                      \
                if (gma_index12index2(cache, band1, band2, block1, &block2, i1, &i2)) \
                    gma_typecast(type1, block1.block)[i1.x+i1.y*block1.w] += \
                        gma_typecast(type2, block2.block)[i2.x+i2.y*block2.w]; \
            }                                                           \
        }                                                               \
        return 2;                                                       \
    }

gma_add_band(int16_t,int16_t)
gma_add_band(int32_t,int32_t)
gma_add_band(double,double)

int is_border_block(gma_band band, gma_block block) {
    if (block.index.x == 0) {
        if (block.index.y == 0)
            return 8;
        else if (block.index.y == band.h_blocks - 1)
            return 6;
        else
            return 7;
    } else if (block.index.x == band.w_blocks - 1) {
        if (block.index.y == 0)
            return 2;
        else if (block.index.y == band.h_blocks - 1)
            return 4;
        else
            return 3;
    } else if (block.index.y == 0) 
        return 1;
    else if (block.index.y == band.h_blocks - 1) 
        return 5;
    return 0;
}

int is_border_cell(gma_block block, int border_block, gma_cell_index i) {
    if (!border_block)
        return 0;
    if (i.x == 0) {
        if (i.y == 0 && border_block == 1)
            return 8;
        else if (i.y == block.h - 1 && border_block == 6)
            return 6;
        else if (border_block == 8 || border_block == 6 || border_block == 7)
            return 7;
    } else if (i.x == block.w - 1) {
        if (i.y == 0 && border_block == 2)
            return 2;
        else if (i.y == block.h - 1 && border_block == 4)
            return 4;
        else if (border_block == 2 || border_block == 4 || border_block == 3)
            return 3;
    } else if (i.y == 0 && (border_block == 8 || border_block == 2 || border_block == 1))
        return 1;
    else if (i.y == block.h - 1 && (border_block == 6 || border_block == 4 || border_block == 5))
        return 5;   
}

// The D8 directions method, compute direction to lowest 8-neighbor
//
// neighbors:
// 8 1 2
// 7 x 3
// 6 5 4
//
// case of nothing lower => flat = pseudo direction 10
// case of all higher => pit = pseudo direction 0
//
// if we are on global border and the cell is flat or pit, 
// then set direction to out of the map
//
// todo: no data cells, mask?
// currently if two neighbors are equally lower, the first is picked
//
template<typename data1_t, typename data2_t>
    int gma_D8(gma_block_cache cache, gma_band band1, gma_band band2, gma_block block1, void *) {
    int border_block = is_border_block(band1, block1);
    gma_cell_index i1;
    for (i1.y = 0; i1.y < block1.h; i1.y++) {
        for (i1.x = 0; i1.x < block1.w; i1.x++) {
            int border_cell = is_border_cell(block1, border_block, i1);
            gma_block block2;
            gma_cell_index i2;
            data2_t my_elevation;
            if (gma_index12index2(cache, band1, band2, block1, &block2, i1, &i2))
                my_elevation = gma_typecast(data2_t, block2.block)[i2.x+i2.y*block2.w];
            data2_t lowest;
            int dir;
            int first = 1;
            gma_cell_index i1n;
            i1n.x = i1.x;
            i1n.y = i1.y-1;

            for (int neighbor = 1; neighbor < 9; neighbor++) {

                switch(neighbor) {
                case 2: i1n.x++; break;
                case 3: i1n.y++; break;
                case 4: i1n.y++; break;
                case 5: i1n.x--; break;
                case 6: i1n.x--; break;
                case 7: i1n.y--; break;
                case 8: i1n.y--; break;
                }
                
                if (!gma_index12index2(cache, band1, band2, block1, &block2, i1n, &i2))
                    continue;

                data2_t tmp = gma_typecast(data2_t, block2.block)[i2.x+i2.y*block2.w];
                if (first || tmp < lowest) {
                    first = 0;
                    lowest = tmp;
                    dir = neighbor;
                }
                
            }

            // is this flat area or a pit?
            if (lowest == my_elevation)
                dir = 10;
            else if (lowest > my_elevation)
                dir = 0;

            gma_typecast(data1_t, block1.block)[i1.x+i1.y*block1.w] = dir;

        }
    }
    return 2;
}

template<typename data1_t, typename data2_t>
int gma_pit_removal(gma_block_cache cache, gma_band band1, gma_band band2, gma_block block1, void *pits) {
    int border_block = is_border_block(band1, block1);
    gma_cell_index i1;
    int _pits; // should be long?

    if (block1.index.x == 0 && block1.index.y == 0)
        _pits = 0;
    else
        _pits = *(int*)pits;

    for (i1.y = 0; i1.y < block1.h; i1.y++) {
        for (i1.x = 0; i1.x < block1.w; i1.x++) {
            int border_cell = is_border_cell(block1, border_block, i1);
            gma_block block2;
            gma_cell_index i2;
            data2_t my_elevation;
            if (gma_index12index2(cache, band1, band2, block1, &block2, i1, &i2))
                my_elevation = gma_typecast(data2_t, block2.block)[i2.x+i2.y*block2.w];
            data2_t lowest;
            int dir;
            int first = 1;
            gma_cell_index i1n;
            i1n.x = i1.x;
            i1n.y = i1.y-1;

            for (int neighbor = 1; neighbor < 9; neighbor++) {

                switch(neighbor) {
                case 2: i1n.x++; break;
                case 3: i1n.y++; break;
                case 4: i1n.y++; break;
                case 5: i1n.x--; break;
                case 6: i1n.x--; break;
                case 7: i1n.y--; break;
                case 8: i1n.y--; break;
                }

                if (!gma_index12index2(cache, band1, band2, block1, &block2, i1n, &i2))
                    continue;

                data2_t tmp = gma_typecast(data2_t, block2.block)[i2.x+i2.y*block2.w];
                if (first || tmp < lowest) {
                    first = 0;
                    lowest = tmp;
                    dir = neighbor;
                }
            }

            // is this a pit?
            if (lowest > my_elevation && !border_cell) {
                gma_typecast(data1_t, block1.block)[i1.x+i1.y*block1.w] = lowest;
                _pits++;
            } else
                gma_typecast(data1_t, block1.block)[i1.x+i1.y*block1.w] = my_elevation;
            
        }
    }

    if (block1.index.x == band1.w_blocks-1 && block1.index.y == band1.h_blocks-1)
        fprintf(stderr, "%i pits.\n", _pits);

    *(int*)pits = _pits;
    if (_pits)
        return 3;
    else
        return 2;
}

template<typename data1_t, typename data2_t>
int gma_route_flats(gma_block_cache cache, gma_band band1, gma_band band2, gma_block block1, void *flats) {
    int border_block = is_border_block(band1, block1);
    gma_cell_index i1;
    int _flats; // should be long?

    if (block1.index.x == 0 && block1.index.y == 0)
        _flats = 0;
    else
        _flats = *(int*)flats;

    for (i1.y = 0; i1.y < block1.h; i1.y++) {
        for (i1.x = 0; i1.x < block1.w; i1.x++) {
            int border_cell = is_border_cell(block1, border_block, i1);
            gma_block block2;
            gma_cell_index i2;
            data2_t my_dir;
            if (gma_index12index2(cache, band1, band2, block1, &block2, i1, &i2))
                my_dir = gma_typecast(data2_t, block2.block)[i2.x+i2.y*block2.w];

            // if flow direction is solved, nothing to do (assuming no pits)
            if (my_dir != 10) {
                gma_typecast(data1_t, block1.block)[i1.x+i1.y*block1.w] = my_dir;
                continue;
            }

            gma_cell_index i1n = { .x = i1.x, .y = i1.y-1 };

            for (int neighbor = 1; neighbor < 9; neighbor++) {

                switch(neighbor) {
                case 2: i1n.x++; break;
                case 3: i1n.y++; break;
                case 4: i1n.y++; break;
                case 5: i1n.x--; break;
                case 6: i1n.x--; break;
                case 7: i1n.y--; break;
                case 8: i1n.y--; break;
                }

                if (!gma_index12index2(cache, band1, band2, block1, &block2, i1n, &i2))
                    continue;

                data2_t tmp = gma_typecast(data2_t, block2.block)[i2.x+i2.y*block2.w];
                if (tmp > 0 && tmp < 9) {
                    my_dir = neighbor;
                    break;
                }
            }

            gma_typecast(data1_t, block1.block)[i1.x+i1.y*block1.w] = my_dir;
            _flats++;
            
        }
    }

    if (block1.index.x == band1.w_blocks-1 && block1.index.y == band1.h_blocks-1)
        fprintf(stderr, "%i flats.\n", _flats);

    *(int*)flats = _flats;
    if (_flats)
        return 3;
    else
        return 2;
}

template<typename data1_t, typename data2_t>
void gma_two_bands_proc(GDALRasterBand *b1, gma_two_bands_callback cb, GDALRasterBand *b2, int focal_distance, void *x) {

    gma_block_cache cache = gma_cache_initialize();
    gma_band band1 = gma_band_initialize(b1), band2 = gma_band_initialize(b2);
    gma_block block1;

    int iterate = 1;
    while (iterate) {
        iterate = 0;
        for (block1.index.y = 0; block1.index.y < band1.h_blocks; block1.index.y++) {
            for (block1.index.x = 0; block1.index.x < band1.w_blocks; block1.index.x++) {

                gma_block_read(&block1, band1);

                CPLErr e = gma_update_block_cache(&cache, band1, band2, b2, block1, focal_distance);
                
                int ret = cb(cache, band1, band2, block1, x);
                switch (ret) {
                case 0: return;
                case 1: break;
                case 2:
                    e = b1->WriteBlock(block1.index.x, block1.index.y, block1.block);
                    break;
                case 3:
                    e = b1->WriteBlock(block1.index.x, block1.index.y, block1.block);
                    iterate = 1;
                    break;
                }
                CPLFree(block1.block);
            }
        }
        if (iterate) {
            
            GDALDataset *ds1 = b1->GetDataset();
            GDALDriver *d = ds1->GetDriver();
            char **files = ds1->GetFileList();

            // flush and close b1
            ds1->FlushCache();
            delete ds1;

            // rename b1 to files[0]."_tmp"
            char *newpath = (char*)CPLMalloc(strlen(files[0])+5);
            strcpy(newpath, files[0]);
            strcat(newpath, "_tmp");
            int e = VSIRename(files[0], newpath);

            // reopen old b1 as b2
            GDALDataset *ds2 = (GDALDataset*)GDALOpen(newpath, GA_ReadOnly);
            b2 = ds2->GetRasterBand(1);
            band2.band = b2;

            // create new b1
            ds1 = d->Create(files[0], band1.w, band1.h, 1, band1.data_type, NULL);
            b1 = ds1->GetRasterBand(1);
            band1.band = b1;

            CPLFree(newpath);
            CSLDestroy(files);
        }
    }
}


void gma_two_bands(GDALRasterBand *b1, gma_two_bands_method_t method, GDALRasterBand *b2) {
    // b1 is changed, b2 is not
    if (b1->GetXSize() != b2->GetXSize() || b1->GetYSize() != b2->GetYSize()) {
        fprintf(stderr, "The sizes of the rasters should be the same.\n");
        return;
    }
    switch (method) {
    case gma_method_add_band: {
        if (b1->GetRasterDataType() != b2->GetRasterDataType()) {
            fprintf(stderr, "This method assumes the data types are the same.\n");
            return;
        }
        switch (b1->GetRasterDataType()) {
        case GDT_Int16: {
            gma_two_bands_proc<int16_t,int16_t>(b1, gma_add_band_int16_tint16_t, b2, 0, NULL);
            break;
        }
        case GDT_Int32: {
            gma_two_bands_proc<int32_t,int32_t>(b1, gma_add_band_int32_tint32_t, b2, 0, NULL);
            break;
        }
        case GDT_Float64: {
            gma_two_bands_proc<double,double>(b1, gma_add_band_doubledouble, b2, 0, NULL);
            break;
        }
        default:
            goto not_implemented_for_these_datatypes;
        }
        break;
    }
    case gma_method_D8: {
        // compute flow directions from DEM
        switch (b1->GetRasterDataType()) {
        case GDT_Byte: {
            switch (b2->GetRasterDataType()) {
            case GDT_UInt16:
                gma_two_bands_proc<unsigned char,uint16_t>(b1, gma_D8<unsigned char,uint16_t>, b2, 1, NULL);
                break;
            case GDT_Float64:
                gma_two_bands_proc<char,double>(b1, gma_D8<unsigned char,double>, b2, 1, NULL);
                break;
            default:
                goto not_implemented_for_these_datatypes;
            }
            break;
        }
        default:
            goto not_implemented_for_these_datatypes;
        }
        break;
    }
    case gma_method_pit_removal: {
        // iterative method to remove pits from DEM
        // datatypes must be the same in iteration
        int pits;
        switch (b1->GetRasterDataType()) {
        case GDT_Byte: {
            switch (b2->GetRasterDataType()) {
            case GDT_UInt16:
                gma_two_bands_proc<unsigned char,uint16_t>(b1, gma_pit_removal<unsigned char,uint16_t>, b2, 1, &pits);
                break;
            case GDT_Float64:
                gma_two_bands_proc<char,double>(b1, gma_pit_removal<unsigned char,double>, b2, 1, &pits);
                break;
            default:
                goto not_implemented_for_these_datatypes;
            }
            break;
        }
        case GDT_UInt16:{
            switch (b2->GetRasterDataType()) {
            case GDT_UInt16:
                gma_two_bands_proc<uint16_t,uint16_t>(b1, gma_pit_removal<uint16_t,uint16_t>, b2, 1, &pits);
                break;
            }
            break;
        }
        default:
            goto not_implemented_for_these_datatypes;
        }
        break;
    }
    case gma_method_route_flats: {
        // iterative method to route flats in fdr
        // datatypes must be the same in iteration
        int flats;
        switch (b1->GetRasterDataType()) {
        case GDT_Byte: {
            switch (b2->GetRasterDataType()) {
            case GDT_Byte:
                gma_two_bands_proc<unsigned char,unsigned char>(b1, gma_route_flats<unsigned char,unsigned char>, b2, 1, &flats);
                break;
            case GDT_UInt16:
                gma_two_bands_proc<unsigned char,uint16_t>(b1, gma_route_flats<unsigned char,uint16_t>, b2, 1, &flats);
                break;
            case GDT_Float64:
                gma_two_bands_proc<char,double>(b1, gma_route_flats<unsigned char,double>, b2, 1, &flats);
                break;
            default:
                goto not_implemented_for_these_datatypes;
            }
            break;
        }
        case GDT_UInt16:{
            switch (b2->GetRasterDataType()) {
            case GDT_UInt16:
                gma_two_bands_proc<uint16_t,uint16_t>(b1, gma_route_flats<uint16_t,uint16_t>, b2, 1, &flats);
                break;
            }
            break;
        }
        default:
            goto not_implemented_for_these_datatypes;
        }
        break;
    }
    default:
        goto unknown_method;
    }
    return;
not_implemented_for_these_datatypes:
    fprintf(stderr, "Not implemented for these datatypes.\n");
    return;
unknown_method:
    fprintf(stderr, "Unknown method.\n");
    return;
}
