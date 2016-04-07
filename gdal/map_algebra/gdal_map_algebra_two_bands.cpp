#include "gdal_map_algebra_private.h"

typedef int (*gma_two_bands_callback)(gma_band, gma_block*, gma_band, gma_object_t**, gma_object_t*);

template<typename type1,typename type2>
type1 gma_test_operator(gma_logical_operation_p<type2> *op, type2 value) {
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

template<typename type1,typename type2>
int gma_assign_band(gma_band band1, gma_block *block1, gma_band band2, gma_object_t **retval, gma_object_t *arg) {
    // arg is checked here
    gma_cell_index i1;
    for (i1.y = 0; i1.y < block1->h; i1.y++) {
        for (i1.x = 0; i1.x < block1->w; i1.x++) {
            type2 value;
            if (gma_value_from_other_band<type2>(band1, block1, i1, band2, &value)) {
                if (arg) {
                    if (gma_test_operator<type1,type2>((gma_logical_operation_p<type2> *)arg, value))
                        gma_block_cell(type1, block1, i1) = value;
                } else
                    gma_block_cell(type1, block1, i1) = value;
            }
        }
    }
    return 2;
}

template<typename type1,typename type2>
int gma_add_band(gma_band band1, gma_block *block1, gma_band band2, gma_object_t **retval, gma_object_t *arg) {
    gma_cell_index i1;
    for (i1.y = 0; i1.y < block1->h; i1.y++) {
        for (i1.x = 0; i1.x < block1->w; i1.x++) {
            type2 value;
            if (gma_value_from_other_band<type2>(band1, block1, i1, band2, &value)) {
                if (arg) {
                    if (gma_test_operator<type1,type2>((gma_logical_operation_p<type2> *)arg, value))
                        gma_block_cell(type1, block1, i1) = value;
                } else
                    gma_block_cell(type1, block1, i1) = value;
            }
        }
    }
    return 2;
}

template<typename type1,typename type2>
int gma_subtract_band(gma_band band1, gma_block *block1, gma_band band2, gma_object_t **retval, gma_object_t *arg) {
    gma_cell_index i1;
    for (i1.y = 0; i1.y < block1->h; i1.y++) {
        for (i1.x = 0; i1.x < block1->w; i1.x++) {
            type2 value;
            if (gma_value_from_other_band<type2>(band1, block1, i1, band2, &value)) {
                if (arg) {
                    if (gma_test_operator<type1,type2>((gma_logical_operation_p<type2> *)arg, value))
                        gma_block_cell(type1, block1, i1) = value;
                } else
                    gma_block_cell(type1, block1, i1) = value;
            }
        }
    }
    return 2;
}

template<typename type1,typename type2>
int gma_multiply_by_band(gma_band band1, gma_block *block1, gma_band band2, gma_object_t **retval, gma_object_t *arg) {
    gma_cell_index i1;
    for (i1.y = 0; i1.y < block1->h; i1.y++) {
        for (i1.x = 0; i1.x < block1->w; i1.x++) {
            type2 value;
            if (gma_value_from_other_band<type2>(band1, block1, i1, band2, &value)) {
                if (arg) {
                    if (gma_test_operator<type1,type2>((gma_logical_operation_p<type2> *)arg, value))
                        gma_block_cell(type1, block1, i1) = value;
                } else
                    gma_block_cell(type1, block1, i1) = value;
            }
        }
    }
    return 2;
}

template<typename type1,typename type2>
int gma_divide_by_band(gma_band band1, gma_block *block1, gma_band band2, gma_object_t **retval, gma_object_t *arg) {
    gma_cell_index i1;
    for (i1.y = 0; i1.y < block1->h; i1.y++) {
        for (i1.x = 0; i1.x < block1->w; i1.x++) {
            type2 value;
            if (gma_value_from_other_band<type2>(band1, block1, i1, band2, &value)) {
                if (arg) {
                    if (gma_test_operator<type1,type2>((gma_logical_operation_p<type2> *)arg, value))
                        gma_block_cell(type1, block1, i1) = value;
                } else
                    gma_block_cell(type1, block1, i1) = value;
            }
        }
    }
    return 2;
}

template<typename type1,typename type2>
int gma_modulus_by_band(gma_band band1, gma_block *block1, gma_band band2, gma_object_t **retval, gma_object_t *arg) {
    gma_cell_index i1;
    for (i1.y = 0; i1.y < block1->h; i1.y++) {
        for (i1.x = 0; i1.x < block1->w; i1.x++) {
            type2 value;
            if (gma_value_from_other_band<type2>(band1, block1, i1, band2, &value)) {
                if (arg) {
                    if (gma_test_operator<type1,type2>((gma_logical_operation_p<type2> *)arg, value))
                        gma_block_cell(type1, block1, i1) = value;
                } else
                    gma_block_cell(type1, block1, i1) = value;
            }
        }
    }
    return 2;
}

#define gma_modulus_by_band_type_error(type1,type2)                     \
    template<>                                                          \
    int gma_modulus_by_band<type1,type2>(gma_band, gma_block*, gma_band, gma_object_t**, gma_object_t*) { \
        fprintf(stderr, "invalid type to binary operator %%");          \
        return 0;                                                       \
    }

gma_modulus_by_band_type_error(uint8_t,float)
gma_modulus_by_band_type_error(uint8_t,double)
gma_modulus_by_band_type_error(float,uint8_t)
gma_modulus_by_band_type_error(double,uint8_t)
gma_modulus_by_band_type_error(uint16_t,float)
gma_modulus_by_band_type_error(uint16_t,double)
gma_modulus_by_band_type_error(float,uint16_t)
gma_modulus_by_band_type_error(double,uint16_t)
gma_modulus_by_band_type_error(int16_t,float)
gma_modulus_by_band_type_error(int16_t,double)
gma_modulus_by_band_type_error(float,int16_t)
gma_modulus_by_band_type_error(double,int16_t)
gma_modulus_by_band_type_error(uint32_t,float)
gma_modulus_by_band_type_error(uint32_t,double)
gma_modulus_by_band_type_error(float,uint32_t)
gma_modulus_by_band_type_error(double,uint32_t)
gma_modulus_by_band_type_error(int32_t,float)
gma_modulus_by_band_type_error(int32_t,double)
gma_modulus_by_band_type_error(float,int32_t)
gma_modulus_by_band_type_error(double,int32_t)
gma_modulus_by_band_type_error(float,float)
gma_modulus_by_band_type_error(float,double)
gma_modulus_by_band_type_error(double,float)
gma_modulus_by_band_type_error(double,double)

// zonal min should be gma_hash_p<gma_int> or gma_hash_p<gma_float> depending on the datatype

template<typename zones_type,typename values_type>
int gma_zonal_min(gma_band zones_band, gma_block *zones_block, gma_band values_band, gma_object_t **retval, gma_object_t*) {
    gma_hash_p<zones_type,gma_number_p<values_type> > *rv;
    if (*retval == NULL) {
        rv = new gma_hash_p<zones_type,gma_number_p<values_type> >;
        *retval = rv;
    } else
        rv = (gma_hash_p<zones_type,gma_number_p<values_type> > *)*retval;
    gma_cell_index zones_i;
    for (zones_i.y = 0; zones_i.y < zones_block->h; zones_i.y++) {
        for (zones_i.x = 0; zones_i.x < zones_block->w; zones_i.x++) {
            values_type value;
            gma_value_from_other_band<values_type>(zones_band, zones_block, zones_i, values_band, &value);
            zones_type zone = gma_block_cell(zones_type, zones_block, zones_i);
            if (!zone)
                continue;
            if (rv->exists(zone)) {
                values_type old_value = rv->get(zone)->value();
                if (value > old_value)
                    continue;
            }
            rv->put(zone, new gma_number_p<values_type>(value));
        }
    }
    return 1;
}

template<typename zones_type,typename values_type>
int gma_zonal_max(gma_band zones_band, gma_block *zones_block, gma_band values_band, gma_object_t **retval, gma_object_t*) {
    gma_hash_p<zones_type,gma_number_p<values_type> > *rv;
    if (*retval == NULL) {
        rv = new gma_hash_p<zones_type,gma_number_p<values_type> >;
        *retval = rv;
    } else
        rv = (gma_hash_p<zones_type,gma_number_p<values_type> > *)*retval;
    gma_cell_index zones_i;
    for (zones_i.y = 0; zones_i.y < zones_block->h; zones_i.y++) {
        for (zones_i.x = 0; zones_i.x < zones_block->w; zones_i.x++) {
            values_type value;
            gma_value_from_other_band<values_type>(zones_band, zones_block, zones_i, values_band, &value);
            zones_type zone = gma_block_cell(zones_type, zones_block, zones_i);
            if (!zone)
                continue;
            if (rv->exists(zone)) {
                values_type old_value = ((gma_number_p<values_type>*)rv->get(zone))->value();
                if (value < old_value)
                    continue;
            }
            rv->put(zone, new gma_number_p<values_type>(value));
        }
    }
    return 1;
}

// raise values in zones to at least zonal min
// zonal min should be gma_hash_p<gma_int> or gma_hash_p<gma_float> depending on the datatype

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

template<typename values_type,typename zones_type>
int gma_set_zonal_min(gma_band values_band, gma_block *values_block, gma_band zones_band, gma_object_t **retval, gma_object_t *zonal_min) {
    gma_band_iterator_t *rv;
    if (*retval == NULL) {
        rv = new gma_band_iterator_t;
        *retval = rv;
    } else
        rv = (gma_band_iterator_t*)*retval;
    gma_hash_p<zones_type,gma_number_p<values_type> > *zm = (gma_hash_p<zones_type,gma_number_p<values_type> >*)zonal_min;
    if (gma_first_block(values_block)) {
        rv->new_loop();
    }
    gma_cell_index values_i;
    for (values_i.y = 0; values_i.y < values_block->h; values_i.y++) {
        for (values_i.x = 0; values_i.x < values_block->w; values_i.x++) {
            zones_type zone;
            gma_value_from_other_band<zones_type>(values_band, values_block, values_i, zones_band, &zone);
            if (!zone)
                continue;
            if (!zm->exists(zone))
                continue;
            values_type new_value = ((gma_number_p<values_type>*)zm->get(zone))->value();
            values_type value = gma_block_cell(values_type, values_block, values_i);
            if (value < new_value) {
                gma_block_cell(values_type, values_block, values_i) = new_value;
                rv->add();
            }
        }
    }
    if (gma_last_block(values_band, values_block))
        fprintf(stderr, "%ld cells raised.\n", rv->count_in_this_loop_of_band);
    return 2;
}

template<typename rims_type,typename areas_type>
int gma_rim_by8(gma_band rims_band, gma_block *rims_block, gma_band areas_band, gma_object_t**, gma_object_t*) {
    gma_cell_index i;
    for (i.y = 0; i.y < rims_block->h; i.y++) {
        for (i.x = 0; i.x < rims_block->w; i.x++) {

            // if the 8-neighborhood in areas is all of the same area, then set rims = 0, otherwise from area

            areas_type area;
            gma_value_from_other_band<areas_type>(rims_band, rims_block, i, areas_band,  &area);

            rims_type my_area = 0;

            gma_cell_index in = gma_cell_first_neighbor(i);
            for (int neighbor = 1; neighbor < 9; neighbor++) {
                gma_cell_move_to_neighbor(in, neighbor);
                areas_type n_area;
                int has_neighbor = gma_value_from_other_band<areas_type>(rims_band, rims_block, in, areas_band,  &n_area);
                if (!has_neighbor || (has_neighbor && (n_area != area))) {
                    my_area = area;
                    break;
                }
            }

            gma_block_cell(rims_type, rims_block, i) = my_area;

        }
    }
    return 2;
}

// the lowest neighbor outside the depression with its depression
// -1 is outside and its value is the elevation of the border cell

template<typename depr_type,typename dem_type>
int gma_depression_pour_elevation(gma_band depr_band, gma_block *depr_block, gma_band dem_band, gma_object_t **retval, gma_object_t *arg) {
    gma_hash_p<depr_type,gma_hash_p<depr_type,gma_number_p<dem_type> > > *z = 
        (gma_hash_p<depr_type,gma_hash_p<depr_type,gma_number_p<dem_type> > > *)arg;
    gma_cell_index i;
    for (i.y = 0; i.y < depr_block->h; i.y++) {
        for (i.x = 0; i.x < depr_block->w; i.x++) {
            depr_type zone = gma_block_cell(depr_type, depr_block, i);
            gma_hash_p<depr_type,gma_number_p<dem_type> > *ns;
            if (z->exists(zone))
                ns = (gma_hash_p<depr_type,gma_number_p<dem_type> >*)z->get(zone);
            else {
                ns = new gma_hash_p<depr_type,gma_number_p<dem_type> >;
                z->put(zone, ns);
            }
            dem_type value;
            gma_value_from_other_band<dem_type>(depr_band, depr_block, i, dem_band,  &value);

            gma_cell_index in = gma_cell_first_neighbor(i);
            for (int neighbor = 1; neighbor < 9; neighbor++) {
                gma_cell_move_to_neighbor(in, neighbor);
                dem_type n_value;
                if (!gma_value_from_other_band<dem_type>(depr_band, depr_block, in, dem_band,  &n_value)) {
                    if (!ns->exists(-1) || value < ((gma_number_p<dem_type>*)ns->get(-1))->value())
                        ns->put(-1, new gma_number_p<dem_type>(value));
                    continue;
                }
                depr_type n_zone;
                gma_value_from_other_band<depr_type>(depr_band, depr_block, in, depr_band,  &n_zone);

                if (n_zone == zone)
                    continue;

                if (!ns->exists(n_zone) || n_value < ((gma_number_p<dem_type>*)ns->get(n_zone))->value())
                    ns->put(n_zone, new gma_number_p<dem_type>(n_value));
            }

        }
    }
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
int gma_D8(gma_band band_fd, gma_block *block_fd, gma_band band_dem, gma_object_t**, gma_object_t*) {
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

// drain flat cells (10) to neighboring non-flat cells which are at same or lower elevation
// this leaves low lying flat areas undrained
template<typename fd_t, typename dem_t>
int gma_route_flats(gma_band band_fd, gma_block *block_fd, gma_band band_dem, gma_object_t **retval, gma_object_t*) {
    gma_band_iterator_t *rv;
    if (*retval == NULL) {
        rv = new gma_band_iterator_t;
        *retval = rv;
    } else
        rv = (gma_band_iterator_t*)*retval;
    if (gma_first_block(block_fd))
        rv->new_loop();
    gma_cell_index i_fd;
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
            rv->add();

        }
    }

    if (gma_last_block(band_fd, block_fd))
        fprintf(stderr, "%ld flat cells routed.\n", rv->count_in_this_loop_of_band);

    if (rv->count_in_this_loop_of_band)
        return 4;
    else
        return 2;
}

template<typename filled_t, typename dem_t>
int gma_fill_depressions(gma_band filled_band, gma_block *filled_block, gma_band dem_band, gma_object_t **retval, gma_object_t*) {
    gma_band_iterator_t *rv;
    if (*retval == NULL) {
        rv = new gma_band_iterator_t;
        *retval = rv;
    } else
        rv = (gma_band_iterator_t*)*retval;
    if (gma_first_block(filled_block))
        rv->new_loop();
    int border_block = is_border_block(filled_band, filled_block);
    gma_cell_index i;
    for (i.y = 0; i.y < filled_block->h; i.y++) {
        for (i.x = 0; i.x < filled_block->w; i.x++) {
            int border_cell = is_border_cell(filled_block, border_block, i);
            dem_t dem_e;
            gma_value_from_other_band<dem_t>(filled_band, filled_block, i, dem_band, &dem_e);

            // initially my_e is set to max e of dem
            // set my_e = max(dem_e, lowest_e_in_nhood)

            filled_t new_e = dem_e, lowest_e_in_nhood;
            if (border_cell)
                lowest_e_in_nhood = 0;
            else {
                int f = 1;
                gma_cell_index in = gma_cell_first_neighbor(i);
                for (int neighbor = 1; neighbor < 9; neighbor++) {
                    gma_cell_move_to_neighbor(in, neighbor);
                    filled_t n_e;
                    gma_value_from_other_band<filled_t>(filled_band, filled_block, in, filled_band, &n_e);
                    if (f || n_e < lowest_e_in_nhood) {
                        f = 0;
                        lowest_e_in_nhood = n_e;
                    }
                }
            }
            if (lowest_e_in_nhood > new_e)
                new_e = lowest_e_in_nhood;

            filled_t old_e = gma_block_cell(filled_t, filled_block, i);
            if (new_e < old_e) {
                gma_block_cell(filled_t, filled_block, i) = new_e;
                rv->add();
            }

        }
    }

    if (gma_last_block(filled_band, filled_block)) {
        fprintf(stderr, "%ld cells changed\n", rv->count_in_this_loop_of_band);
    }

    if (rv->count_in_this_loop_of_band)
        return 4;
    else
        return 2;
}

template<typename datatype>
class gma_depressions_iterator_t : public gma_object_t {
public:
    long count_in_this_loop_of_band;
    long total_count;
    int basin_id;
    int depressions;
    gma_hash_p<datatype,gma_hash_p<datatype,gma_number_p<datatype> > > *splits;
    gma_depressions_iterator_t() {
        count_in_this_loop_of_band = 0;
        total_count = 0;
        basin_id = 0;
        depressions = 0;
        splits = new gma_hash_p<datatype,gma_hash_p<datatype,gma_number_p<datatype> > >;
    }
    void new_loop() {
        count_in_this_loop_of_band = 0;
    }
    void new_basin() {
        count_in_this_loop_of_band++;
        total_count++;
        basin_id++;
        depressions++;
    }
    void add() {
        count_in_this_loop_of_band++;
        total_count++;
    }
};

// return depressions, i.e., areas, which drain to pits or flat cells
// iterate until no new cell is added to depressions
template<typename deps_t, typename fd_t>
int gma_depressions(gma_band band_deps, gma_block *block_deps, gma_band band_fd, gma_object_t **retval, gma_object_t*) {
    gma_depressions_iterator_t<deps_t> *rv;
    if (*retval == NULL) {
        rv = new gma_depressions_iterator_t<deps_t>;
        *retval = rv;
    } else
        rv = (gma_depressions_iterator_t<deps_t>*)*retval;
    if (gma_first_block(block_deps))
        rv->new_loop();

    gma_cell_index i_deps;
    for (i_deps.y = 0; i_deps.y < block_deps->h; i_deps.y++) {
        for (i_deps.x = 0; i_deps.x < block_deps->w; i_deps.x++) {

            // if on a basin, nothing to do
            if (gma_block_cell(deps_t, block_deps, i_deps)) continue;

            fd_t my_dir;
            gma_value_from_other_band<fd_t>(band_deps, block_deps, i_deps, band_fd,  &my_dir);

            // if pit, mark and we're done
            if (my_dir == 0) {
                rv->new_basin();
                gma_block_cell(deps_t, block_deps, i_deps) = rv->basin_id;
                continue;
            }

            // do we flow to a basin? or are we flat with a flat neighbor, which is a part of a basin?
            // this may lead to situations where a flat area is marked with two or more basin id's
            // although they are part of the same basin => that has to be fixed later
            deps_t n_basin[9] = {0,0,0,0,0,0,0,0,0};
            fd_t n_dir[9] = {0,0,0,0,0,0,0,0,0};

            gma_cell_index in_deps = gma_cell_first_neighbor(i_deps);
            for (int neighbor = 1; neighbor < 9; neighbor++) {
                gma_cell_move_to_neighbor(in_deps, neighbor);

                if (!gma_value_from_other_band<deps_t>(band_deps, block_deps, in_deps,
                                                         band_deps, &n_basin[neighbor]))
                    continue;  // we are at border and this is outside

                gma_value_from_other_band<fd_t>(band_deps, block_deps, in_deps, band_fd,  &n_dir[neighbor]);

            }

            if (my_dir == 10) {
                // we're flat, look for deps among flat neighbors
                deps_t split_basin = 0;
                for (int neighbor = 1; neighbor < 9; neighbor++) {
                    deps_t basin = n_basin[neighbor];
                    if (basin && (n_dir[neighbor] == 10)) {
                        // mark at first
                        if (!gma_block_cell(deps_t, block_deps, i_deps)) {
                            gma_block_cell(deps_t, block_deps, i_deps) = basin;
                            rv->add();
                        }
                        // denote the basin split
                        if (!split_basin)
                            split_basin = basin;
                        else if (basin != split_basin) {
                            if (!rv->splits->exists(split_basin)) {
                                gma_hash_p<deps_t,gma_number_p<deps_t> > *b = new gma_hash_p<deps_t,gma_number_p<deps_t> >;
                                b->put(basin, new gma_number_p<deps_t>(1));
                                rv->splits->put(split_basin, b);
                            } else {
                                gma_hash_p<deps_t,gma_number_p<deps_t> > *b = 
                                    (gma_hash_p<deps_t,gma_number_p<deps_t> > *)rv->splits->get(split_basin);
                                if (!b->exists(basin))
                                    b->put(basin, new gma_number_p<deps_t>(1));
                            }
                        }
                    }
                }
                // not done, do it now
                if (!gma_block_cell(deps_t, block_deps, i_deps)) {
                    rv->new_basin();
                    gma_block_cell(deps_t, block_deps, i_deps) = rv->basin_id;
                }
            } else if (n_basin[my_dir]) {
                // flow into a basin
                gma_block_cell(deps_t, block_deps, i_deps) = n_basin[my_dir];
                rv->add();
            }

        }
    }

    if (gma_last_block(band_deps, block_deps))
        fprintf(stderr, "%ld cells added to depressions, %i depressions.\n", rv->count_in_this_loop_of_band, rv->depressions);

    if (rv->count_in_this_loop_of_band)
        return 4;
    else
        return 2;
}

// function to fix the splitted depressions
template<typename datatype>
gma_hash_p<datatype,gma_number_p<datatype> > *gma_split_classifier
    (gma_hash_p<datatype,gma_hash_p<datatype,gma_number_p<datatype> > > *splits) {
    gma_hash_p<datatype,gma_number_p<datatype> > *map = new gma_hash_p<datatype,gma_number_p<datatype> >;
    gma_hash_p<datatype,gma_number_p<datatype> > *to = new gma_hash_p<datatype,gma_number_p<datatype> >;
    int n1 = splits->size();
    int32_t *keys1 = splits->keys(n1);
    for (int i1 = 0; i1 < n1; i1++) {
        int32_t key1 = keys1[i1];
        gma_hash_p<datatype,gma_number_p<datatype> > *s2 = (gma_hash_p<datatype,gma_number_p<datatype> > *)splits->get(key1);
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
                    map->put(a, new gma_number_p<datatype>(b));
                    to->put(b, new gma_number_p<datatype>(1));
                }
            }
            a = key1;
            b = key2;
            if (!to->exists(a)) {
                if (a != b && !map->exists(a) && !map->exists(b)) {
                    map->put(a, new gma_number_p<datatype>(b));
                    to->put(b, new gma_number_p<datatype>(1));
                }
            }
        }
        CPLFree(keys2);
    }
    CPLFree(keys1);
    delete to;
    return map;
}

// band2 = flow directions
// band1 = upstream area = 1 + cells upstream
template<typename data1_t, typename data2_t>
int gma_upstream_area(gma_band band1, gma_block *block1, gma_band band2, gma_object_t **retval, gma_object_t*) {
    gma_band_iterator_t *rv;
    if (*retval == NULL) {
        rv = new gma_band_iterator_t;
        *retval = rv;
    } else
        rv = (gma_band_iterator_t*)*retval;
    if (gma_first_block(block1))
        rv->new_loop();
    int border_block = is_border_block(band1, block1);
    gma_cell_index i1;
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
                if (abs(tmp2 - neighbor) != 4)
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
                continue;
            } else if (upstream_neighbors == 0) {
                upstream_area = 1;
            } else if (upstream_neighbors > 0 && upstream_area == 0) {
                continue;
            }

            rv->add();
            gma_block_cell(data1_t, block1, i1) = upstream_area;

        }
    }

    if (gma_last_block(band1, block1))
        fprintf(stderr, "Upstream area of %ld cells computed.\n", rv->count_in_this_loop_of_band);

    if (rv->count_in_this_loop_of_band)
        return 4;
    else
        return 2;
}

template<typename catchment_t, typename fd_t>
int gma_catchment(gma_band catchment_band, gma_block *catchment_block, gma_band band_fd, gma_object_t **retval, gma_object_t *arg) {
    gma_band_iterator_t *rv;
    if (*retval == NULL) {
        rv = new gma_band_iterator_t;
        *retval = rv;
    } else
        rv = (gma_band_iterator_t*)*retval;
    if (gma_first_block(catchment_block))
        rv->new_loop();

    gma_cell_index i;
    gma_cell_p<catchment_t> *cell = (gma_cell_p<catchment_t> *)arg; // check?

    for (i.y = 0; i.y < catchment_block->h; i.y++) {
        for (i.x = 0; i.x < catchment_block->w; i.x++) {

            if (gma_block_cell(catchment_t, catchment_block, i) == cell->value()) continue;

            // if this is the outlet cell, mark
            // global cell index
            int x = catchment_block->index.x * catchment_band.w_block + i.x;
            int y = catchment_block->index.y * catchment_band.h_block + i.y;
            if (cell->x() == x && cell->y() == y) {
                gma_block_cell(catchment_t, catchment_block, i) = cell->value();
                rv->add();
                continue;
            }

            // if this flows into a marked cell, then mark this
            fd_t my_dir;
            gma_value_from_other_band<fd_t>(catchment_band, catchment_block, i, band_fd, &my_dir);

            gma_cell_index id = gma_cell_first_neighbor(i);
            for (int neighbor = 1; neighbor <= my_dir; neighbor++) {
                gma_cell_move_to_neighbor(id, neighbor);
            }

            catchment_t my_down;
            if (!gma_value_from_other_band<catchment_t>(catchment_band, catchment_block, id, catchment_band, &my_down))
                continue;

            if (my_down == cell->value()) {
                gma_block_cell(catchment_t, catchment_block, i) = cell->value();
                rv->add();
            }

        }
    }

    if (gma_last_block(catchment_band, catchment_block))
        fprintf(stderr, "%ld cells added\n", rv->count_in_this_loop_of_band);

    if (rv->count_in_this_loop_of_band)
        return 4;
    else
        return 2;
}


// focal distance & cache updates might be best done in callback since the knowledge is there
// unless we want to have focal distance in user space too
// anyway, focal area may be needed only in b2 or both in b2 and b1
template<typename data1_t, typename data2_t>
void gma_two_bands_proc(GDALRasterBand *b1, gma_two_bands_callback cb, GDALRasterBand *b2, gma_object_t **retval, gma_object_t *arg, int focal_distance) {

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

                int ret = cb(band1, block1, band2, retval, arg);
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

gma_object_t *gma_two_bands(GDALRasterBand *b1, gma_two_bands_method_t method, GDALRasterBand *b2, gma_object_t *arg) {
    gma_object_t *retval = NULL;
    // b1 is changed, b2 is not
    if (b1->GetXSize() != b2->GetXSize() || b1->GetYSize() != b2->GetYSize()) {
        fprintf(stderr, "The sizes of the rasters should be the same.\n");
        return NULL;
    }
    switch (method) {
    case gma_method_assign_band: // b1 = b2
        if (arg) { // arg is 
            if (arg->get_class() != gma_logical_operation)
                goto wrong_argument_class;
            /*
            if (b2->GetRasterDataType() != (gma_logical_operation_p*)arg->get_datatype())
                goto wrong_argument_class;
            */
        }
        type_switch_bb(gma_assign_band, 0);
        break;
    case gma_method_add_band: // b1 += b2
        type_switch_bb(gma_add_band, 0);
        break;
    case gma_method_subtract_band:
        type_switch_bb(gma_subtract_band, 0);
        break;
    case gma_method_multiply_by_band: // b1 *= b2
        type_switch_bb(gma_multiply_by_band, 0);
        break;
    case gma_method_divide_by_band:
        type_switch_bb(gma_divide_by_band, 0);
        break;
    case gma_method_modulus_by_band:
        type_switch_bb(gma_modulus_by_band, 0);
        break;
    case gma_method_zonal_min: // b1 = zones, b2 = values
        type_switch_ib(gma_zonal_min, 0);
        break;
    case gma_method_zonal_max: // b1 = zones, b2 = values
        type_switch_ib(gma_zonal_max, 0);
        break;
    case gma_method_set_zonal_min: // b1 = values, b2 = zones, arg = hash of zonal mins, b1 is changed
        type_switch_bi(gma_set_zonal_min, 0);
        break;
    case gma_method_rim_by8: // rims <- areas
        type_switch_ii(gma_rim_by8, 1);
        break;
    case gma_method_D8: // fd <- dem
        // compute flow directions from DEM
        type_switch_ib(gma_D8, 1);
        break;
    case gma_method_route_flats: {  // fd, dem
        // iterative method to route flats in fdr
        // datatypes must be the same in iteration
        type_switch_ib(gma_route_flats, 1);
        break;
    }
    case gma_method_fill_depressions: { // b1 = filled, b2 = dem
        // compute max of b2
        gma_object_t *max = gma_compute_value(b2, gma_method_get_max);
        // set b1 to it
        gma_with_arg(b1, gma_method_assign, max);
        type_switch_bb(gma_fill_depressions, 1);
        break;
    }

    // the following two are not recommended
    case gma_method_depressions: // depressions <- fd
        type_switch_ii(gma_depressions, 1);
        break;
    case gma_method_depression_pour_elevation: // depression, dem compute depression => (depression => elevation)
        type_switch_ib(gma_depression_pour_elevation, 1);
        break;
    
    case gma_method_upstream_area: // ua, fd
        type_switch_bi(gma_upstream_area, 1);
        break;
    case gma_method_catchment: // mark into b1 the catchment with arg (), b2 contains fd
        type_switch_ii(gma_catchment, 1);
        break;
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
wrong_argument_class:
    fprintf(stderr, "Wrong class in argument.\n");
    return NULL;
}
