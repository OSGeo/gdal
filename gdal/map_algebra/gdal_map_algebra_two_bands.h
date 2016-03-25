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

#define gma_block_cell(type, block, cell) gma_typecast(type, block->_block)[cell.x+cell.y*block->w]

typedef struct {
    size_t n;
    gma_block **blocks;
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
    gma_block_cache cache;
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
    ds1 = d->Create(files[0], band1->w, band1->h, 1, band1->data_type, NULL);
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

CPLErr gma_band_write_block(gma_band band, gma_block block) {
    return band.band->WriteBlock(block.index.x, block.index.y, block._block);
}

CPLErr gma_band_add_to_cache(gma_band *band, gma_block_index i) {
    gma_block *b = gma_cache_retrieve(band->cache, i);
    if (!b) {
        b = gma_block_create();
        b->index = i;
        gma_band_set_block_size(*band, b);
        b->_block = CPLMalloc(band->w_block * band->h_block * band->size_of_data_type);
        CPLErr e = band->band->ReadBlock(b->index.x, b->index.y, b->_block);
        gma_cache_add(&(band->cache), b);
    }
}

// which blocks in band 2 are needed to cover a block extended with focal distance d in band 1?
// assuming the raster size if the same
void gma_blocks_in_band2(gma_band band1, gma_band band2,
                         gma_block b1, int d, gma_block_index *i20, gma_block_index *i21) {

    // index of top left cell to be covered
    int x10 = b1.index.x * band1.w_block - d, y10 = b1.index.y * band1.h_block - d;

    // index of bottom right cell to be covered
    int x11 = x10 + d + b1.w-1 + d, y11 = y10 + d + b1.h-1 + d;

    // which block covers x10, y10 in band2?
    i20->x = MAX(x10 / band2.w_block, 0);
    i20->y = MAX(y10 / band2.h_block, 0);

    // which block covers x11, y11 in band2?
    i21->x = MIN(x11 / band2.w_block, band2.w_blocks-1);
    i21->y = MIN(y11 / band2.h_block, band2.h_blocks-1);
}

CPLErr gma_band_update_cache(gma_band *band2,
                             gma_band band1,
                             gma_block b1, int d) {
    gma_block_index i, i20, i21;
    gma_blocks_in_band2(band1, *band2, b1, d, &i20, &i21);
    for (i.y = i20.y; i.y <= i21.y; i.y++) {
        for (i.x = i20.x; i.x <= i21.x; i.x++) {
            gma_band_add_to_cache(band2, i);
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
int gma_index12index2(gma_band band1, gma_band band2,
                      gma_block *b1, gma_block **b2,
                      gma_cell_index i1, gma_cell_index *i2) {
    // global cell index
    int x = b1->index.x * band1.w_block + i1.x;
    int y = b1->index.y * band1.h_block + i1.y;
    if (x < 0 || y < 0 || x >= band1.w || y >= band1.h)
        return 0;
    // index of block 2
    int x2 = x / band2.w_block;
    int y2 = y / band2.h_block;
    for (int i = 0; i < band2.cache.n; i++)
        if (band2.cache.blocks[i]->index.x == x2 && band2.cache.blocks[i]->index.y == y2) {
            *b2 = band2.cache.blocks[i];
            i2->x = x % band2.w_block;
            i2->y = y % band2.h_block;
            return 1;
        }
    return 0;
}

typedef int (*gma_two_bands_callback)(gma_band, gma_band, gma_block*, void*);

template<typename type1,typename type2>
int gma_add_band(gma_band band1, gma_band band2, gma_block *block1, void *) {
    gma_cell_index i1;
    for (i1.y = 0; i1.y < block1->h; i1.y++) {
        for (i1.x = 0; i1.x < block1->w; i1.x++) {
            gma_block *block2;
            gma_cell_index i2;
            if (gma_index12index2(band1, band2, block1, &block2, i1, &i2))
                gma_block_cell(type1, block1, i1) +=
                    gma_block_cell(type2, block2, i2);
        }
    }
    return 2;
}

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
int gma_D8(gma_band band1, gma_band band2, gma_block *block1, void *) {
    int border_block = is_border_block(band1, block1);
    gma_cell_index i1;
    for (i1.y = 0; i1.y < block1->h; i1.y++) {
        for (i1.x = 0; i1.x < block1->w; i1.x++) {
            int border_cell = is_border_cell(block1, border_block, i1);
            gma_block *block2;
            gma_cell_index i2;
            data2_t my_elevation;
            if (gma_index12index2(band1, band2, block1, &block2, i1, &i2))
                my_elevation = gma_block_cell(data2_t, block2, i2);
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

                if (!gma_index12index2(band1, band2, block1, &block2, i1n, &i2))
                    continue;

                data2_t tmp = gma_block_cell(data2_t, block2, i2);
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

            gma_block_cell(data1_t, block1, i1) = dir;

        }
    }
    return 2;
}

template<typename data1_t, typename data2_t>
int gma_pit_removal(gma_band band1, gma_band band2, gma_block *block1, void *pits) {
    int border_block = is_border_block(band1, block1);
    gma_cell_index i1;
    int _pits; // should be long?

    if (block1->index.x == 0 && block1->index.y == 0)
        _pits = 0;
    else
        _pits = *(int*)pits;

    for (i1.y = 0; i1.y < block1->h; i1.y++) {
        for (i1.x = 0; i1.x < block1->w; i1.x++) {
            int border_cell = is_border_cell(block1, border_block, i1);
            gma_block *block2;
            gma_cell_index i2;
            data2_t my_elevation;
            if (gma_index12index2(band1, band2, block1, &block2, i1, &i2))
                my_elevation = gma_block_cell(data2_t, block2, i2);
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

                if (!gma_index12index2(band1, band2, block1, &block2, i1n, &i2))
                    continue;

                data2_t tmp = gma_block_cell(data2_t, block2, i2);
                if (first || tmp < lowest) {
                    first = 0;
                    lowest = tmp;
                    dir = neighbor;
                }
            }

            // is this a pit?
            if (lowest > my_elevation && !border_cell) {
                gma_block_cell(data1_t, block1, i1) = lowest;
                _pits++;
            } else
                gma_block_cell(data1_t, block1, i1) = my_elevation;

        }
    }

    if (block1->index.x == band1.w_blocks-1 && block1->index.y == band1.h_blocks-1)
        fprintf(stderr, "%i pits.\n", _pits);

    *(int*)pits = _pits;
    if (_pits)
        return 3;
    else
        return 2;
}

template<typename data1_t, typename data2_t>
int gma_route_flats(gma_band band1, gma_band band2, gma_block *block1, void *flats) {
    int border_block = is_border_block(band1, block1);
    gma_cell_index i1;
    int _flats; // should be long?

    if (block1->index.x == 0 && block1->index.y == 0)
        _flats = 0;
    else
        _flats = *(int*)flats;

    for (i1.y = 0; i1.y < block1->h; i1.y++) {
        for (i1.x = 0; i1.x < block1->w; i1.x++) {
            int border_cell = is_border_cell(block1, border_block, i1);
            gma_block *block2;
            gma_cell_index i2;
            data2_t my_dir;
            if (gma_index12index2(band1, band2, block1, &block2, i1, &i2))
                my_dir = gma_block_cell(data2_t, block2, i2);

            // if flow direction is solved, nothing to do (assuming no pits)
            if (my_dir != 10) {
                gma_block_cell(data1_t, block1, i1) = my_dir;
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

                if (!gma_index12index2(band1, band2, block1, &block2, i1n, &i2))
                    continue;

                data2_t tmp = gma_block_cell(data2_t, block2, i2);
                if (tmp > 0 && tmp < 9) {
                    my_dir = neighbor;
                    break;
                }
            }

            gma_block_cell(data1_t, block1, i1) = my_dir;
            _flats++;

        }
    }

    if (block1->index.x == band1.w_blocks-1 && block1->index.y == band1.h_blocks-1)
        fprintf(stderr, "%i flats.\n", _flats);

    *(int*)flats = _flats;
    if (_flats)
        return 3;
    else
        return 2;
}

// band2 = flow directions
// band1 = upstream area = 1 + cells upstream
// need both bands cached!
template<typename data1_t, typename data2_t>
int gma_upstream_area(gma_band band1, gma_band band2, gma_block *block1, void *) {
    int border_block = is_border_block(band1, block1);
    gma_cell_index i1;
    int not_done = 0;
    for (i1.y = 0; i1.y < block1->h; i1.y++) {
        for (i1.x = 0; i1.x < block1->w; i1.x++) {
            int border_cell = is_border_cell(block1, border_block, i1);

            // upstream area is already computed
            if (gma_block_cell(data1_t, block1, i1) > 0)
                continue;

            gma_cell_index in = { .x = i1.x, .y = i1.y-1 };

            int upstream_neighbors = 0;
            int upstream_area = 0;

            for (int neighbor = 1; neighbor < 9; neighbor++) {

                switch(neighbor) {
                case 2: in.x++; break;
                case 3: in.y++; break;
                case 4: in.y++; break;
                case 5: in.x--; break;
                case 6: in.x--; break;
                case 7: in.y--; break;
                case 8: in.y--; break;
                }

                gma_block *block1n;
                gma_cell_index i1n;
                // neighbor is outside (or later also no data)
                if (!gma_index12index2(band1, band1, block1, &block1n, in, &i1n))
                    continue;

                gma_block *block2;
                gma_cell_index i2;
                if (!gma_index12index2(band1, band2, block1, &block2, in, &i2))
                    continue;

                data2_t tmp2 = gma_block_cell(data2_t, block2, i2);
                // if this neighbor does not point to us, then we're not interested
                if (tmp2 - neighbor != 4)
                    continue;

                upstream_neighbors++;

                data1_t tmp1 = gma_block_cell(data1_t, block1n, i1n);
                // if the neighbor's upstream area is not computed, then we're done
                if (tmp1 == 0) {
                    upstream_neighbors = -1;
                    break;
                }

                upstream_area += tmp1;

            }

            if (upstream_neighbors == -1) {
                not_done++;
                continue;
            } else if (upstream_neighbors == 0) {
                upstream_area = 1;
            } else if (upstream_neighbors > 0 && upstream_area == 0) {
                not_done++;
                continue;
            }

            gma_block_cell(data1_t, block1, i1) = upstream_area;

        }
    }

    if (block1->index.x == band1.w_blocks-1 && block1->index.y == band1.h_blocks-1)
        fprintf(stderr, "%i cells with unresolved upstream area.\n", not_done);

    if (not_done)
        return 4;
    else
        return 2;
}

// focal distance & cache updates might be best done in callback since the knowledge is there
// unless we want to have focal distance in user space too
// anyway, focal area may be needed only in b2 or both in b2 and b1
template<typename data1_t, typename data2_t>
void gma_two_bands_proc(GDALRasterBand *b1, gma_two_bands_callback cb, GDALRasterBand *b2, int focal_distance, void *x) {

    gma_band band1 = gma_band_initialize(b1), band2 = gma_band_initialize(b2);
    gma_block_index i;

    int iterate = 1;
    while (iterate) {
        iterate = 0;
        for (i.y = 0; i.y < band1.h_blocks; i.y++) {
            for (i.x = 0; i.x < band1.w_blocks; i.x++) {

                gma_band_add_to_cache(&band1, i);
                gma_block *block1 = gma_band_get_block(band1, i);
                CPLErr e = gma_band_update_cache(&band1, band1, *block1, focal_distance);

                e = gma_band_update_cache(&band2, band1, *block1, focal_distance);

                int ret = cb(band1, band2, block1, x);
                switch (ret) {
                case 0: return;
                case 1: break;
                case 2:
                    e = gma_band_write_block(band1, *block1);
                    break;
                case 3:
                    e = gma_band_write_block(band1, *block1);
                    iterate = 1;
                    break;
                case 4:
                    e = gma_band_write_block(band1, *block1);
                    iterate = 2;
                    break;
                }
            }
        }
        if (iterate == 1) // band 2 <- band 1; new band 1
            gma_band_iteration(&band1, &band2);
    }
    gma_band_empty_cache(&band1);
    gma_band_empty_cache(&band2);
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
            gma_two_bands_proc<int16_t,int16_t>(b1, gma_add_band<int16_t,int16_t>, b2, 0, NULL);
            break;
        }
        case GDT_Int32: {
            gma_two_bands_proc<int32_t,int32_t>(b1, gma_add_band<int32_t,int32_t>, b2, 0, NULL);
            break;
        }
        case GDT_Float64: {
            gma_two_bands_proc<double,double>(b1, gma_add_band<double,double>, b2, 0, NULL);
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
                gma_two_bands_proc<uint8_t,uint16_t>(b1, gma_D8<uint8_t,uint16_t>, b2, 1, NULL);
                break;
            case GDT_Float64:
                gma_two_bands_proc<uint8_t,double>(b1, gma_D8<uint8_t,double>, b2, 1, NULL);
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
                gma_two_bands_proc<uint8_t,uint16_t>(b1, gma_pit_removal<uint8_t,uint16_t>, b2, 1, &pits);
                break;
            case GDT_Float64:
                gma_two_bands_proc<uint8_t,double>(b1, gma_pit_removal<uint8_t,double>, b2, 1, &pits);
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
                gma_two_bands_proc<uint8_t,uint8_t>(b1, gma_route_flats<uint8_t,uint8_t>, b2, 1, &flats);
                break;
            case GDT_UInt16:
                gma_two_bands_proc<uint8_t,uint16_t>(b1, gma_route_flats<uint8_t,uint16_t>, b2, 1, &flats);
                break;
            case GDT_Float64:
                gma_two_bands_proc<uint8_t,double>(b1, gma_route_flats<uint8_t,double>, b2, 1, &flats);
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
    case gma_method_upstream_area: {
        switch (b1->GetRasterDataType()) {
        case GDT_Byte: {
            switch (b2->GetRasterDataType()) {
            case GDT_UInt16:
                gma_two_bands_proc<uint8_t,uint16_t>(b1, gma_upstream_area<uint8_t,uint16_t>, b2, 1, NULL);
                break;
            case GDT_UInt32:
                gma_two_bands_proc<uint8_t,uint32_t>(b1, gma_upstream_area<uint8_t,uint32_t>, b2, 1, NULL);
                break;
            }
            break;
        }
        case GDT_UInt16:{
            switch (b2->GetRasterDataType()) {
            case GDT_UInt16:
                gma_two_bands_proc<uint16_t,uint16_t>(b1, gma_upstream_area<uint16_t,uint16_t>, b2, 1, NULL);
                break;
            case GDT_UInt32:
                gma_two_bands_proc<uint16_t,uint32_t>(b1, gma_upstream_area<uint16_t,uint32_t>, b2, 1, NULL);
                break;
            }
            break;
        }
        case GDT_UInt32:{
            switch (b2->GetRasterDataType()) {
            case GDT_Byte:
                gma_two_bands_proc<uint32_t,uint8_t>(b1, gma_upstream_area<uint32_t,uint8_t>, b2, 1, NULL);
                break;
            case GDT_UInt16:
                gma_two_bands_proc<uint32_t,uint16_t>(b1, gma_upstream_area<uint32_t,uint16_t>, b2, 1, NULL);
                break;
            case GDT_UInt32:
                gma_two_bands_proc<uint32_t,uint32_t>(b1, gma_upstream_area<uint32_t,uint32_t>, b2, 1, NULL);
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
    fprintf(stderr, "Not implemented for these datatypes <%i,%i>.\n", b1->GetRasterDataType(), b2->GetRasterDataType());
    return;
unknown_method:
    fprintf(stderr, "Unknown method.\n");
    return;
}
