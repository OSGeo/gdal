
typedef int (*gma_two_bands_callback)(gma_band, gma_block*, gma_band, void*);

template<typename type1,typename type2>
int gma_add_band(gma_band band1, gma_block *block1, gma_band band2, void *) {
    gma_cell_index i1;
    for (i1.y = 0; i1.y < block1->h; i1.y++) {
        for (i1.x = 0; i1.x < block1->w; i1.x++) {
            gma_cell_index i2;
            gma_block *block2 = gma_index12index2(band1, block1, i1, band2, &i2);
            if (block2)
                gma_block_cell(type1, block1, i1) += gma_block_cell(type2, block2, i2);
        }
    }
    return 2;
}

// zonal min should be gma_hash<gma_int> or gma_hash<gma_float> depending on the datatype

template<typename zones_type,typename values_type>
int gma_zonal_min(gma_band zones_band, gma_block *zones_block, gma_band values_band, void *zonal_min) {
    gma_hash<gma_int> *z = (gma_hash<gma_int>*)zonal_min;
    gma_cell_index zones_i;
    for (zones_i.y = 0; zones_i.y < zones_block->h; zones_i.y++) {
        for (zones_i.x = 0; zones_i.x < zones_block->w; zones_i.x++) {
            gma_cell_index values_i;
            gma_block *values_block = gma_index12index2(zones_band, zones_block, zones_i, values_band, &values_i);
            zones_type zone = gma_block_cell(zones_type, zones_block, zones_i);
            values_type value = gma_block_cell(values_type, values_block, values_i);
            if (z->exists(zone)) {
                values_type old_value = z->get(zone)->value();
                if (value > old_value)
                    continue;
            }
            z->put(zone, new gma_int(value));
        }
    }
    return 1;
}

// raise values in zones to at least zonal min
// zonal min should be gma_hash<gma_int> or gma_hash<gma_float> depending on the datatype

template<typename values_type,typename zones_type>
int gma_set_zonal_min(gma_band values_band, gma_block *values_block, gma_band zones_band, void *zonal_min) {
    gma_hash<gma_int> *z = (gma_hash<gma_int>*)zonal_min;
    gma_cell_index values_i;
    for (values_i.y = 0; values_i.y < values_block->h; values_i.y++) {
        for (values_i.x = 0; values_i.x < values_block->w; values_i.x++) {
            gma_cell_index zones_i;
            gma_block *zones_block = gma_index12index2(values_band, values_block, values_i, zones_band, &zones_i);
            values_type value = gma_block_cell(values_type, values_block, values_i);
            zones_type zone = gma_block_cell(zones_type, zones_block, zones_i);
            if (z->exists(zone)) {
                zones_type new_value = z->get(zone)->value();
                if (value < new_value)
                    gma_block_cell(values_type, values_block, values_i) = new_value;
            }
        }
    }
    return 1;
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
    else
        return 0;
}

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

template<typename rims_type,typename areas_type>
int gma_rim_by8(gma_band rims_band, gma_block *rims_block, gma_band areas_band, void *) {
    int border_block = is_border_block(rims_band, rims_block);
    gma_cell_index i;
    for (i.y = 0; i.y < rims_block->h; i.y++) {
        for (i.x = 0; i.x < rims_block->w; i.x++) {
            int border_cell = is_border_cell(rims_block, border_block, i);
            if (border_cell) continue;

            // if the 8-neighborhood in areas is all of the same area, then set rims = 0, otherwise from area

            areas_type area;
            gma_value_from_other_band<areas_type>(rims_band, rims_block, i, areas_band,  &area);

            rims_type my_area = 0;

            gma_cell_index in = gma_cell_first_neighbor(i);
            for (int neighbor = 1; neighbor < 9; neighbor++) {
                gma_cell_move_to_neighbor(in, neighbor);
                areas_type n_area;
                gma_value_from_other_band<areas_type>(rims_band, rims_block, in, areas_band,  &n_area);
                if (n_area != area) {
                    my_area = area;
                    break;
                }
            }

            gma_block_cell(rims_type, rims_block, i) = my_area;

        }
    }
    return 2;
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
template<typename fd_t, typename dem_t>
int gma_D8(gma_band band_fd, gma_block *block_fd, gma_band band_dem, void *) {
    int border_block = is_border_block(band_fd, block_fd);
    gma_cell_index i_fd;
    for (i_fd.y = 0; i_fd.y < block_fd->h; i_fd.y++) {
        for (i_fd.x = 0; i_fd.x < block_fd->w; i_fd.x++) {
            int border_cell = is_border_cell(block_fd, border_block, i_fd);
            
            dem_t my_elevation;
            gma_value_from_other_band<dem_t>(band_fd, block_fd, i_fd, band_dem,  &my_elevation);

            dem_t lowest;
            int dir;
            int first = 1;

            gma_cell_index i_n_fd = gma_cell_first_neighbor(i_fd);
            for (int neighbor = 1; neighbor < 9; neighbor++) {
                gma_cell_move_to_neighbor(i_n_fd, neighbor);

                dem_t tmp;
                if (!gma_value_from_other_band<dem_t>(band_fd, block_fd, i_n_fd, band_dem, &tmp))
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

            gma_block_cell(fd_t, block_fd, i_fd) = dir;

        }
    }
    return 2;
}

#define gma_first_block(block) block->index.x == 0 && block->index.y == 0

#define gma_last_block(band, block) block->index.x == band.w_blocks-1 && block->index.y == band.h_blocks-1

// drain flat cells (10) to neighboring non-flat cells which are at same or lower elevation
// this leaves low lying flat areas undrained
template<typename fd_t, typename dem_t>
int gma_route_flats(gma_band band_fd, gma_block *block_fd, gma_band band_dem, void *flats) {
    gma_cell_index i_fd;
    int _flats; // should be long?

    if (gma_first_block(block_fd))
        _flats = 0;
    else
        _flats = *(int*)flats;

    for (i_fd.y = 0; i_fd.y < block_fd->h; i_fd.y++) {
        for (i_fd.x = 0; i_fd.x < block_fd->w; i_fd.x++) {

            // if not flat cell, nothing to do
            if (gma_block_cell(fd_t, block_fd, i_fd) != 10) continue;

            dem_t my_elevation;
            gma_value_from_other_band<dem_t>(band_fd, block_fd, i_fd, band_dem,  &my_elevation);

            fd_t new_dir = 0;
            gma_cell_index in_fd = gma_cell_first_neighbor(i_fd);
            for (int neighbor = 1; neighbor < 9; neighbor++) {
                gma_cell_move_to_neighbor(in_fd, neighbor);

                fd_t n_dir;
                if (!gma_value_from_other_band<fd_t>(band_fd, block_fd, in_fd, band_fd,  &n_dir))
                    continue;  // we are at border and this is outside

                if (n_dir == 10) continue;

                dem_t n_elevation;
                gma_value_from_other_band<dem_t>(band_fd, block_fd, in_fd, band_dem,  &n_elevation);

                if (n_elevation > my_elevation) continue;

                new_dir = neighbor;
                break;
            }
            if (new_dir == 0) continue;

            gma_block_cell(fd_t, block_fd, i_fd) = new_dir;
            _flats++;

        }
    }

    if (gma_last_block(band_fd, block_fd))
        fprintf(stderr, "%i flat cells routed.\n", _flats);

    *(int*)flats = _flats;
    if (_flats)
        return 4;
    else
        return 2;
}

typedef struct {
    int cells_added;
    int basin_id;
    gma_hash<gma_hash<gma_int> > *splits; // basin => (basin => 1) 
} gma_basins_data;

// return basins, i.e., areas, which drain to pits or flat cells
// iterate until no new cell is added to basins
template<typename basins_t, typename fd_t>
int gma_basins(gma_band band_basins, gma_block *block_basins, gma_band band_fd, void *data) {
    gma_cell_index i_basins;
    gma_basins_data *d = (gma_basins_data *)data;
    
    if (gma_first_block(block_basins)) {
        d->cells_added = 0;
        d->basin_id = 0;
    }

    for (i_basins.y = 0; i_basins.y < block_basins->h; i_basins.y++) {
        for (i_basins.x = 0; i_basins.x < block_basins->w; i_basins.x++) {

            // if on a basin, nothing to do
            if (gma_block_cell(basins_t, block_basins, i_basins)) continue;

            fd_t my_dir;
            gma_value_from_other_band<fd_t>(band_basins, block_basins, i_basins, band_fd,  &my_dir);
            
            // if pit, mark and we're done
            if (my_dir == 0) {
                d->basin_id++;
                gma_block_cell(basins_t, block_basins, i_basins) = d->basin_id;
                d->cells_added++;
                continue;
            }

            // do we flow to a basin? or are we flat with a flat neighbor, which is a part of a basin?
            // this may lead to situations where a flat area is marked with two or more basin id's 
            // although they are part of the same basin => that has to be fixed later
            basins_t n_basin[9] = {0,0,0,0,0,0,0,0,0};
            fd_t n_dir[9] = {0,0,0,0,0,0,0,0,0};
            
            gma_cell_index in_basins = gma_cell_first_neighbor(i_basins);
            for (int neighbor = 1; neighbor < 9; neighbor++) {
                gma_cell_move_to_neighbor(in_basins, neighbor);
                
                if (!gma_value_from_other_band<basins_t>(band_basins, block_basins, in_basins, 
                                                         band_basins, &n_basin[neighbor]))
                    continue;  // we are at border and this is outside
                
                gma_value_from_other_band<fd_t>(band_basins, block_basins, in_basins, band_fd,  &n_dir[neighbor]);

            }

            if (my_dir == 10) {
                // we're flat, look for basins among flat neighbors
                basins_t split_basin = 0;
                for (int neighbor = 1; neighbor < 9; neighbor++) {
                    basins_t basin = n_basin[neighbor];
                    if (basin && (n_dir[neighbor] == 10)) {
                        // mark at first
                        if (!gma_block_cell(basins_t, block_basins, i_basins)) {
                            gma_block_cell(basins_t, block_basins, i_basins) = basin;
                            d->cells_added++;
                        }
                        // denote the basin split
                        if (!split_basin)
                            split_basin = basin;
                        else if (basin != split_basin) {
                            if (!d->splits) {
                                d->splits = new gma_hash<gma_hash<gma_int> >;
                            } else if (!d->splits->exists(split_basin)) {
                                gma_hash<gma_int> *b = new gma_hash<gma_int>;
                                b->put(basin, new gma_int(1));
                                d->splits->put(split_basin, b);
                            } else {
                                gma_hash<gma_int> *b = d->splits->get(split_basin);
                                if (!b->exists(basin))
                                    b->put(basin, new gma_int(1));
                            }
                        }
                    }
                }
                // not done, do it now
                if (!gma_block_cell(basins_t, block_basins, i_basins)) {
                    d->basin_id++;
                    gma_block_cell(basins_t, block_basins, i_basins) = d->basin_id;
                    d->cells_added++;
                }
            } else if (n_basin[my_dir]) {
                // flow into a basin
                gma_block_cell(basins_t, block_basins, i_basins) = n_basin[my_dir];
                d->cells_added++;
            }

        }
    }

    if (gma_last_block(band_basins, block_basins))
        fprintf(stderr, "%i cells added to basins.\n", d->cells_added);

    if (d->cells_added)
        return 4;
    else
        return 2;
}

// band2 = flow directions
// band1 = upstream area = 1 + cells upstream
template<typename data1_t, typename data2_t>
int gma_upstream_area(gma_band band1, gma_block *block1, gma_band band2, void *) {
    int border_block = is_border_block(band1, block1);
    gma_cell_index i1;
    int not_done = 0;
    for (i1.y = 0; i1.y < block1->h; i1.y++) {
        for (i1.x = 0; i1.x < block1->w; i1.x++) {
            int border_cell = is_border_cell(block1, border_block, i1);

            // upstream area is already computed
            if (gma_block_cell(data1_t, block1, i1) > 0)
                continue;

            int upstream_neighbors = 0;
            int upstream_area = 0;

            gma_cell_index in = gma_cell_first_neighbor(i1);
            for (int neighbor = 1; neighbor < 9; neighbor++) {
                gma_cell_move_to_neighbor(in, neighbor);

                gma_cell_index i1n;
                gma_block *block1n = gma_index12index2(band1, block1, in, band1, &i1n);
                // neighbor is outside (or later also no data)
                if (!block1n)
                    continue;

                gma_cell_index i2;
                gma_block *block2 = gma_index12index2(band1, block1, in, band2, &i2);
                if (!block2)
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
                CPLErr e = gma_band_update_cache(&band1, band1, block1, focal_distance);

                e = gma_band_update_cache(&band2, band1, block1, focal_distance);

                int ret = cb(band1, block1, band2, x);
                switch (ret) {
                case 0: return;
                case 1: break;
                case 2:
                    e = gma_band_write_block(band1, block1);
                    break;
                case 3:
                    e = gma_band_write_block(band1, block1);
                    iterate = 1;
                    break;
                case 4:
                    e = gma_band_write_block(band1, block1);
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

void *gma_two_bands(GDALRasterBand *b1, gma_two_bands_method_t method, GDALRasterBand *b2, void *arg = NULL) {
    void *retval = NULL;
    // b1 is changed, b2 is not
    if (b1->GetXSize() != b2->GetXSize() || b1->GetYSize() != b2->GetYSize()) {
        fprintf(stderr, "The sizes of the rasters should be the same.\n");
        return NULL;
    }
    switch (method) {
    case gma_method_add_band: // b1 += b2
        if (b1->GetRasterDataType() != b2->GetRasterDataType()) {
            fprintf(stderr, "This method assumes the data types are the same.\n");
            return NULL;
        }
        switch (b1->GetRasterDataType()) {
        case GDT_Int16:
            gma_two_bands_proc<int16_t,int16_t>(b1, gma_add_band<int16_t,int16_t>, b2, 0, NULL);
            break;
        case GDT_Int32:
            gma_two_bands_proc<int32_t,int32_t>(b1, gma_add_band<int32_t,int32_t>, b2, 0, NULL);
            break;
        case GDT_Float64:
            gma_two_bands_proc<double,double>(b1, gma_add_band<double,double>, b2, 0, NULL);
            break;
        default:
            goto not_implemented_for_these_datatypes;
        }
        break;
    case gma_method_zonal_min: { // b1 = zones, b2 = values
        gma_hash<gma_int> *zonal_min = new gma_hash<gma_int>;
        retval = (void*)zonal_min;
        switch (b1->GetRasterDataType()) {
        case GDT_Int16:
            switch (b2->GetRasterDataType()) {
                case GDT_Int16:
                    gma_two_bands_proc<int16_t,int16_t>(b1, gma_zonal_min<int16_t,int16_t>, b2, 0, zonal_min);
                    break;
            default:
                delete zonal_min;
                goto not_implemented_for_these_datatypes;
            }
            break;
        case GDT_Int32:
            switch (b2->GetRasterDataType()) {
                case GDT_Int16:
                    gma_two_bands_proc<int32_t,int16_t>(b1, gma_zonal_min<int32_t,int16_t>, b2, 0, zonal_min);
                    break;
            default:
                delete zonal_min;
                goto not_implemented_for_these_datatypes;
            }
            
            break;
        case GDT_UInt32:
            switch (b2->GetRasterDataType()) {
                case GDT_UInt16:
                    gma_two_bands_proc<uint32_t,uint16_t>(b1, gma_zonal_min<uint32_t,uint16_t>, b2, 0, zonal_min);
                    break;
                case GDT_Int16:
                    gma_two_bands_proc<uint32_t,int16_t>(b1, gma_zonal_min<uint32_t,int16_t>, b2, 0, zonal_min);
                    break;
            default:
                delete zonal_min;
                goto not_implemented_for_these_datatypes;
            }
            break;
        default:
            goto not_implemented_for_these_datatypes;
        }
        break;
    }
    case gma_method_set_zonal_min: { // b1 = values, b2 = zones, arg = hash of zonal mins, b1 is changed
        gma_hash<gma_int> *zonal_min = (gma_hash<gma_int>*)arg;
        switch (b1->GetRasterDataType()) {
        case GDT_Int16:
            switch (b2->GetRasterDataType()) {
                case GDT_Int16:
                    gma_two_bands_proc<int16_t,int16_t>(b1, gma_set_zonal_min<int16_t,int16_t>, b2, 0, zonal_min);
                    break;
            default:
                delete zonal_min;
                goto not_implemented_for_these_datatypes;
            }
            break;
        case GDT_Int32:
            switch (b2->GetRasterDataType()) {
                case GDT_Int16:
                    gma_two_bands_proc<int32_t,int16_t>(b1, gma_set_zonal_min<int32_t,int16_t>, b2, 0, zonal_min);
                    break;
            default:
                delete zonal_min;
                goto not_implemented_for_these_datatypes;
            }
            
            break;
        case GDT_UInt32:
            switch (b2->GetRasterDataType()) {
                case GDT_UInt16:
                    gma_two_bands_proc<uint32_t,uint16_t>(b1, gma_set_zonal_min<uint32_t,uint16_t>, b2, 0, zonal_min);
                    break;
                case GDT_Int16:
                    gma_two_bands_proc<uint32_t,int16_t>(b1, gma_set_zonal_min<uint32_t,int16_t>, b2, 0, zonal_min);
                    break;
            default:
                delete zonal_min;
                goto not_implemented_for_these_datatypes;
            }
            break;
        default:
            goto not_implemented_for_these_datatypes;
        }
        break;
    }
    case gma_method_rim_by8: // rims <- areas
        if (b1->GetRasterDataType() != b2->GetRasterDataType()) {
            fprintf(stderr, "This method assumes the data types are the same.\n");
            return NULL;
        }
        switch (b1->GetRasterDataType()) {
        case GDT_Int16:
            gma_two_bands_proc<int16_t,int16_t>(b1, gma_rim_by8<int16_t,int16_t>, b2, 1, NULL);
            break;
        case GDT_Int32:
            gma_two_bands_proc<int32_t,int32_t>(b1, gma_rim_by8<int32_t,int32_t>, b2, 1, NULL);
            break;
        case GDT_UInt32:
            gma_two_bands_proc<uint32_t,uint32_t>(b1, gma_rim_by8<uint32_t,uint32_t>, b2, 1, NULL);
            break;
        default:
            goto not_implemented_for_these_datatypes;
        }
        break;
    case gma_method_D8: // fd <- dem
        // compute flow directions from DEM
        switch (b1->GetRasterDataType()) {
        case GDT_Byte:
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
        default:
            goto not_implemented_for_these_datatypes;
        }
        break;
    case gma_method_route_flats: {  // fd, dem
        // iterative method to route flats in fdr
        // datatypes must be the same in iteration
        int flats;
        switch (b1->GetRasterDataType()) {
        case GDT_Byte:
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
        default:
            goto not_implemented_for_these_datatypes;
        }
        break;
    }
    case gma_method_depressions: { // depressions <- fd
        gma_basins_data data;
        data.splits = NULL;
        switch (b1->GetRasterDataType()) {
        case GDT_Byte:
            switch (b2->GetRasterDataType()) {
            case GDT_Byte:
                gma_two_bands_proc<uint8_t,uint8_t>(b1, gma_basins<uint8_t,uint8_t>, b2, 1, &data);
                break;
            }
            break;
        case GDT_UInt16:
            switch (b2->GetRasterDataType()) {
            case GDT_Byte:
                gma_two_bands_proc<uint16_t,uint8_t>(b1, gma_basins<uint16_t,uint8_t>, b2, 1, &data);
                break;
            }
            break;
        case GDT_UInt32:
            switch (b2->GetRasterDataType()) {
            case GDT_Byte:
                gma_two_bands_proc<uint32_t,uint8_t>(b1, gma_basins<uint32_t,uint8_t>, b2, 1, &data);
                break;
            }
            break;
        default:
            goto not_implemented_for_these_datatypes;
        }
        if (data.splits) {
            // prune the splits into distinct linked sets
            gma_hash<gma_int> *map = new gma_hash<gma_int>;
            gma_hash<gma_int> *to = new gma_hash<gma_int>;
            int n1 = data.splits->size();
            int32_t *keys1 = data.splits->keys(n1);
            for (int i1 = 0; i1 < n1; i1++) {
                int32_t key1 = keys1[i1];
                gma_hash<gma_int> *s2 = data.splits->get(key1);
                int n2 = s2->size();
                int32_t *keys2 = s2->keys(n2);
                for (int i2 = 0; i2 < n2; i2++) {
                    int32_t key2 = keys2[i2];
                    // key1 is unique, key2 may be what ever
                    // we need a => b, where b is from key1 and key2 and a is key1 and key2 minus b
                    int32_t a = key2;
                    int32_t b = key1;
                    if (!to->exists(a)) {
                        if (a != b && !map->exists(a) && !map->exists(b)) {
                            map->put(a, new gma_int(b));
                            to->put(b, new gma_int(1));
                        }
                    }
                    a = key1;
                    b = key2;
                    if (!to->exists(a)) { 
                        if (a != b && !map->exists(a) && !map->exists(b)) {
                            map->put(a, new gma_int(b));
                            to->put(b, new gma_int(1));
                        }
                    }                        
                }
                CPLFree(keys2);
            }
            CPLFree(keys1);
            delete to;
            delete data.splits;

            // fix depressions (b1) with map
            printf("unite split depressions\n");
            switch (b1->GetRasterDataType()) {
            case GDT_Byte: {
                gma_mapper<uint8_t> *mapper = new gma_mapper<uint8_t>(map);
                gma_map(b1, mapper);
                break;
            }
            case GDT_UInt16: {
                gma_mapper<uint16_t> *mapper = new gma_mapper<uint16_t>(map);
                gma_map(b1, mapper);
                break;
            }
            case GDT_UInt32: {
                gma_mapper<uint32_t> *mapper = new gma_mapper<uint32_t>(map);
                gma_map(b1, mapper);
                break;
            }
            }
            

            if (1) {
                // report the splits
                int n = map->size();
                int32_t *keys = map->keys_sorted(n);
                for (int i = 0; i < n; i++) {
                    int32_t value = map->get(keys[i])->value();
                    printf("%i => %i\n", keys[i], value);
                }
                CPLFree(keys);
            }
            delete map;
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
    return retval;
not_implemented_for_these_datatypes:
    fprintf(stderr, "Not implemented for these datatypes <%i,%i>.\n", b1->GetRasterDataType(), b2->GetRasterDataType());
    return NULL;
unknown_method:
    fprintf(stderr, "Unknown method.\n");
    return NULL;
}
