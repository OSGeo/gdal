class gma_band_iterator_p : public gma_object_t {
public:
    long count_in_this_loop_of_band;
    long total_count;
    gma_band_iterator_p() {
        count_in_this_loop_of_band = 0;
        total_count = 0;
    }
    virtual gma_class_t get_class() {
        return gma_iterator;
    }
    void new_loop() {
        count_in_this_loop_of_band = 0;
    }
    void add() {
        ++count_in_this_loop_of_band;
        ++total_count;
    }
};

template <typename type1,typename type2> class gma_two_bands_p : public gma_two_bands_t {
    gma_band_p<type1> *b1;
    gma_band_p<type2> *b2;
    gma_band_p<uint8_t> *b3;
    GDALProgressFunc m_progress;
    void *m_progress_arg;
public:
    gma_two_bands_p() {
        b1 = NULL;
        b2 = NULL;
        b3 = NULL;
        m_progress = NULL;
        m_progress_arg = NULL;
    }
    virtual void set_progress_fct(GDALProgressFunc progress, void *progress_arg) {
        m_progress = progress;
        m_progress_arg = progress_arg;
    }
    struct callback {
        typedef int (gma_two_bands_p<type1,type2>::*type)(gma_block<type1>*, gma_object_t**, gma_object_t*, int);
        type fct;
    };
    void block_loop(callback cb, gma_object_t **retval = NULL, gma_object_t *arg = NULL, int focal_distance = 0) {
        if (b1->w() != b2->w() || b1->h() != b2->h() || (b3 && (b1->w() != b3->w() || b1->h() != b3->h()))) {
            CPLError(CE_Failure, CPLE_IllegalArg, "The sizes of the rasters should be the same.");
            return;
        }
        gma_block_index i;
        int iterate = 1;
        while (iterate) {
            iterate = 0;
            for (i.y = 0; i.y < b1->h_blocks; ++i.y) {
                for (i.x = 0; i.x < b1->w_blocks; ++i.x) {
                    b1->add_to_cache(i);
                    gma_block<type1> *block = b1->get_block(i);
                    if (!block) return;
                    b1->update_cache(b1, block, focal_distance);
                    b2->update_cache(b1, block, focal_distance);
                    if (b3) b3->update_cache(b1, block, focal_distance);
                    int ret = (this->*cb.fct)(block, retval, arg, focal_distance);
                    switch (ret) {
                    case 0: return;
                    case 1: break;
                    case 2:
                        b1->write_block(block);
                        break;
                    case 3:
                        b1->write_block(block);
                        iterate = 1;
                        break;
                    }
                    if (CPLGetLastErrorNo() != CPLE_None) return;
                }
            }
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
        return false;
    }
    int m_assign(gma_block<type1> *block, gma_object_t**, gma_object_t *arg, int) {
        gma_cell_index i;
        for (i.y = 0; i.y < block->h(); ++i.y) {
            for (i.x = 0; i.x < block->w(); ++i.x) {
                if (b1->cell_is_nodata(block, i)) continue;
                type2 a;
                if (b2->has_value(b1, block, i, &a)) {
                    if ((arg && test_operator((gma_logical_operation_p<type2> *)arg, a)) || !arg)
                        block->cell(i) = MAX(MIN(a, std::numeric_limits<type1>::max()), std::numeric_limits<type1>::min());
                }
            }
        }
        return 2;
    }
    int m_add(gma_block<type1> *block, gma_object_t**, gma_object_t *arg, int) {
        gma_cell_index i;
        for (i.y = 0; i.y < block->h(); ++i.y) {
            for (i.x = 0; i.x < block->w(); ++i.x) {
                if (b1->cell_is_nodata(block, i)) continue;
                type2 a;
                if (b2->has_value(b1, block, i, &a)) {
                    if ((arg && test_operator((gma_logical_operation_p<type2> *)arg, a)) || !arg)
                        block->cell(i) = MAX(MIN(block->cell(i) + a, std::numeric_limits<type1>::max()), std::numeric_limits<type1>::min());
                }
            }
        }
        return 2;
    }
    int m_subtract(gma_block<type1> *block, gma_object_t**, gma_object_t *arg, int) {
        gma_cell_index i;
        for (i.y = 0; i.y < block->h(); ++i.y) {
            for (i.x = 0; i.x < block->w(); ++i.x) {
                if (b1->cell_is_nodata(block,i)) continue;
                type2 a;
                if (b2->has_value(b1, block, i, &a)) {
                    if ((arg && test_operator((gma_logical_operation_p<type2> *)arg, a)) || !arg)
                        block->cell(i) = MAX(MIN(block->cell(i) - a, std::numeric_limits<type1>::max()), std::numeric_limits<type1>::min());
                }
            }
        }
        return 2;
    }
    int m_multiply(gma_block<type1> *block, gma_object_t**, gma_object_t *arg, int) {
        gma_cell_index i;
        for (i.y = 0; i.y < block->h(); ++i.y) {
            for (i.x = 0; i.x < block->w(); ++i.x) {
                if (b1->cell_is_nodata(block,i)) continue;
                type2 a;
                if (b2->has_value(b1, block, i, &a)) {
                    if ((arg && test_operator((gma_logical_operation_p<type2> *)arg, a)) || !arg)
                        block->cell(i) = MAX(MIN(block->cell(i) * a, std::numeric_limits<type1>::max()), std::numeric_limits<type1>::min());
                }
            }
        }
        return 2;
    }
    int m_divide(gma_block<type1> *block, gma_object_t**, gma_object_t *arg, int) {
        gma_cell_index i;
        for (i.y = 0; i.y < block->h(); ++i.y) {
            for (i.x = 0; i.x < block->w(); ++i.x) {
                if (b1->cell_is_nodata(block,i)) continue;
                type2 a;
                if (b2->has_value(b1, block, i, &a)) {
                    if ((arg && test_operator((gma_logical_operation_p<type2> *)arg, a)) || !arg) {
                        if (a == 0)
                            block->cell(i) = std::numeric_limits<type1>::quiet_NaN();
                        else
                            block->cell(i) = MAX(MIN(block->cell(i) / a, std::numeric_limits<type1>::max()), std::numeric_limits<type1>::min());
                    }
                }
            }
        }
        return 2;
    }
    int m_modulus(gma_block<type1> *block, gma_object_t**, gma_object_t *arg, int) {
        gma_cell_index i;
        for (i.y = 0; i.y < block->h(); ++i.y) {
            for (i.x = 0; i.x < block->w(); ++i.x) {
                if (b1->cell_is_nodata(block,i)) continue;
                type2 a;
                if (b2->has_value(b1, block, i, &a)) {
                    if ((arg && test_operator((gma_logical_operation_p<type2> *)arg, a)) || !arg) {
                        if (a == 0)
                            block->cell(i) = std::numeric_limits<type1>::quiet_NaN();
                        else
                            block->cell(i) %= a;
                    }
                }
            }
        }
        return 2;
    }
    int m_decision(gma_block<type1> *block, gma_object_t**, gma_object_t*, int) {
        gma_cell_index i;
        for (i.y = 0; i.y < block->h(); ++i.y) {
            for (i.x = 0; i.x < block->w(); ++i.x) {
                if (b1->cell_is_nodata(block,i)) continue;
                type2 a;
                if (b2->has_value(b1, block, i, &a)) {
                    uint8_t decision;
                    if (b3->has_value(b1, block, i, &decision)) {
                        if (decision)
                            block->cell(i) = MAX(MIN(a, std::numeric_limits<type1>::max()), std::numeric_limits<type1>::min());
                    }
                }
            }
        }
        return 2;
    }
    // b1 = values, b2 = zones
    int m_zonal_min(gma_block<type1> *block, gma_object_t **retval, gma_object_t*, int) {
        GMA_RETVAL_INIT(gma_hash_p<type2 COMMA gma_number_p<type1> >, rv, );
        gma_cell_index i;
        for (i.y = 0; i.y < block->h(); ++i.y) {
            for (i.x = 0; i.x < block->w(); ++i.x) {
                if (b1->cell_is_nodata(block,i)) continue;
                type1 value = block->cell(i);
                type2 zone;
                if (!b2->has_value(b1, block, i, &zone))
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
    int m_zonal_max(gma_block<type1> *block, gma_object_t **retval, gma_object_t*, int) {
        GMA_RETVAL_INIT(gma_hash_p<type2 COMMA gma_number_p<type1> >, rv, );
        gma_cell_index i;
        for (i.y = 0; i.y < block->h(); ++i.y) {
            for (i.x = 0; i.x < block->w(); ++i.x) {
                if (b1->cell_is_nodata(block,i)) continue;
                type1 value = block->cell(i);
                type2 zone;
                if (!b2->has_value(b1, block, i, &zone))
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
        for (i.y = 0; i.y < block->h(); ++i.y) {
            for (i.x = 0; i.x < block->w(); ++i.x) {

                // if the 8-neighborhood in areas is all of the same area, then set rims = 0, otherwise from area

                type2 area;
                b2->has_value(b1, block, i, &area);

                type1 my_area = 0;

                gma_cell_index in = i.first_neighbor();
                for (int neighbor = 1; neighbor < 9; ++neighbor) {
                    in.move_to_neighbor(neighbor);
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
        GMA_RETVAL_INIT(gma_band_iterator_p, rv, );
        if (block->first_block())
            rv->new_loop();
        int border_block = b1->is_border_block(block);
        gma_cell_index i;
        for (i.y = 0; i.y < block->h(); ++i.y) {
            for (i.x = 0; i.x < block->w(); ++i.x) {
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
                    gma_cell_index in = i.first_neighbor();
                    for (int neighbor = 1; neighbor < 9; ++neighbor) {
                        in.move_to_neighbor(neighbor);
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

        if (b1->last_block(block) && m_progress) {
            char buffer[30];
            snprintf(buffer, 30, "%ld cells changed.", rv->count_in_this_loop_of_band);
            if (!m_progress(0, buffer, m_progress_arg)) {
                CPLError(CE_Failure, CPLE_UserInterrupt, "User terminated.");
                return 0;
            }
        }

        if (rv->count_in_this_loop_of_band)
            return 3;
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
        for (i.y = 0; i.y < block->h(); ++i.y) {
            for (i.x = 0; i.x < block->w(); ++i.x) {
                int border_cell = block->is_border_cell(border_block, i);

                type2 my_elevation;
                b2->has_value(b1, block, i, &my_elevation);

                type2 lowest;
                int dir;
                int first = 1;

                gma_cell_index i_n = i.first_neighbor();
                for (int neighbor = 1; neighbor < 9; ++neighbor) {
                    i_n.move_to_neighbor(neighbor);

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
    // drain flat cells (10) to neighboring non-flat cells which are at same or lower elevation
    // this leaves low lying flat areas undrained
    // b1 = fd, b2 = dem
    int m_route_flats(gma_block<type1> *block, gma_object_t **retval, gma_object_t*, int) {
        GMA_RETVAL_INIT(gma_band_iterator_p, rv, );
        if (block->first_block())
            rv->new_loop();
        gma_cell_index i;
        for (i.y = 0; i.y < block->h(); ++i.y) {
            for (i.x = 0; i.x < block->w(); ++i.x) {

                // if not flat cell, nothing to do
                if (block->cell(i) != 10) continue;

                type2 my_elevation;
                b2->has_value(b1, block, i, &my_elevation);

                type1 new_dir = 0;
                gma_cell_index in = i.first_neighbor();
                for (int neighbor = 1; neighbor < 9; ++neighbor) {
                    in.move_to_neighbor(neighbor);

                    if (b1->cell_is_outside(block, in))
                        continue;

                    type1 n_dir;
                    b1->has_value(b1, block, in, &n_dir);

                    if (n_dir == 10) continue;

                    type2 n_elevation;
                    b2->has_value(b1, block, in, &n_elevation);

                    if (n_elevation > my_elevation) continue;

                    new_dir = neighbor;
                    break;
                }
                if (new_dir == 0) continue;

                block->cell(i) = new_dir;
                rv->add();

            }
        }

        if (b1->last_block(block) && m_progress) {
            char buffer[30];
            snprintf(buffer, 30, "%ld flat cells routed.", rv->count_in_this_loop_of_band);
            if (!m_progress(0, buffer, m_progress_arg)) {
                CPLError(CE_Failure, CPLE_UserInterrupt, "User terminated.");
                return 0;
            }
        }
        if (rv->count_in_this_loop_of_band)
            return 3;
        else
            return 2;
    }
    // b1 = upstream area = 1 + cells upstream, b2 = flow directions
    int m_upstream_area(gma_block<type1> *block, gma_object_t **retval, gma_object_t*, int) {
        GMA_RETVAL_INIT(gma_band_iterator_p, rv, );
        if (block->first_block())
            rv->new_loop();
        //int border_block = b1->is_border_block(block);
        gma_cell_index i;
        for (i.y = 0; i.y < block->h(); ++i.y) {
            for (i.x = 0; i.x < block->w(); ++i.x) {
                //int border_cell = block->is_border_cell(border_block, i);

                // upstream area is already computed
                if (block->cell(i) > 0)
                    continue;

                int upstream_neighbors = 0;
                int upstream_area = 0;

                gma_cell_index in = i.first_neighbor();
                for (int neighbor = 1; neighbor < 9; ++neighbor) {
                    in.move_to_neighbor(neighbor);

                    gma_cell_index i1;
                    gma_block<type1> *blockn = b1->get_block(b1, block, in, &i1);

                    // neighbor is outside (or later also no data)
                    if (!blockn)
                        continue;

                    gma_cell_index i2;
                    gma_block<type2> *block2 = b2->get_block(b1, block, in, &i2);

                    if (!block2)
                        continue;

                    type2 tmp2 = block2->cell(i2);
                    // if this neighbor does not point to us, then we're not interested
                    if (abs(tmp2 - neighbor) != 4)
                        continue;

                    ++upstream_neighbors;

                    type1 tmp1 = blockn->cell(i1);
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
                block->cell(i) = upstream_area;

            }
        }

        if (b1->last_block(block) && m_progress) {
            char buffer[30];
            snprintf(buffer, 30, "Upstream area of %ld cells computed.", rv->count_in_this_loop_of_band);
            if (!m_progress(0, buffer, m_progress_arg)) {
                CPLError(CE_Failure, CPLE_UserInterrupt, "User terminated.");
                return 0;
            }
        }
        if (rv->count_in_this_loop_of_band)
            return 3;
        else
            return 2;
    }
    // b1 = catchment, b2 = fd
    int m_catchment(gma_block<type1> *block, gma_object_t **retval, gma_object_t *arg, int) {
        GMA_RETVAL_INIT(gma_band_iterator_p, rv,  );
        if (block->first_block())
            rv->new_loop();

        gma_cell_index i;
        gma_cell_p<type1> *cell = (gma_cell_p<type1> *)arg; // check?

        for (i.y = 0; i.y < block->h(); ++i.y) {
            for (i.x = 0; i.x < block->w(); ++i.x) {

                if (block->cell(i) == cell->value()) continue;

                // if this is the outlet cell, mark
                gma_cell_index gi = b1->global_cell_index(block, i);
                if (cell->x() == gi.x && cell->y() == gi.y) {
                    block->cell(i) = cell->value();
                    rv->add();
                    continue;
                }

                // if this flows into a marked cell, then mark this
                type2 my_dir;
                b2->has_value(b1, block, i, &my_dir);

                gma_cell_index id = i.first_neighbor();
                for (int neighbor = 1; neighbor <= my_dir; ++neighbor) {
                    id.move_to_neighbor(neighbor);
                }

                type1 my_down;
                if (!b1->has_value(b1, block, id, &my_down))
                    continue;

                if (my_down == cell->value()) {
                    block->cell(i) = cell->value();
                    rv->add();
                }

            }
        }

        if (b1->last_block(block) && m_progress) {
            char buffer[30];
            snprintf(buffer, 30, "%ld cells added.", rv->count_in_this_loop_of_band);
            if (!m_progress(0, buffer, m_progress_arg)) {
                CPLError(CE_Failure, CPLE_UserInterrupt, "User terminated.");
                return 0;
            }
        }
        if (rv->count_in_this_loop_of_band)
            return 3;
        else
            return 2;
    }


public:
    virtual void assign(gma_band_t *band1, gma_band_t *band2, gma_logical_operation_t *op = NULL) {
        b1 = (gma_band_p<type1>*)band1;
        b2 = (gma_band_p<type2>*)band2;
        callback cb;
        cb.fct = &gma_two_bands_p::m_assign;
        block_loop(cb, NULL, op);
    }
    virtual void add(gma_band_t *summand1, gma_band_t *summand2, gma_logical_operation_t *op = NULL) {
        b1 = (gma_band_p<type1>*)summand1;
        b2 = (gma_band_p<type2>*)summand2;
        callback cb;
        cb.fct = &gma_two_bands_p::m_add;
        block_loop(cb, NULL, op);
    }
    virtual void subtract(gma_band_t *band1, gma_band_t *band2, gma_logical_operation_t *op = NULL) {
        b1 = (gma_band_p<type1>*)band1;
        b2 = (gma_band_p<type2>*)band2;
        callback cb;
        cb.fct = &gma_two_bands_p::m_subtract;
        block_loop(cb, NULL, op);
    }
    virtual void multiply(gma_band_t *band1, gma_band_t *band2, gma_logical_operation_t *op = NULL) {
        b1 = (gma_band_p<type1>*)band1;
        b2 = (gma_band_p<type2>*)band2;
        callback cb;
        cb.fct = &gma_two_bands_p::m_multiply;
        block_loop(cb, NULL, op);
    }
    virtual void divide(gma_band_t *band1, gma_band_t *band2, gma_logical_operation_t *op = NULL) {
        b1 = (gma_band_p<type1>*)band1;
        b2 = (gma_band_p<type2>*)band2;
        callback cb;
        cb.fct = &gma_two_bands_p::m_divide;
        block_loop(cb, NULL, op);
    }
    virtual void modulus(gma_band_t *band1, gma_band_t *band2, gma_logical_operation_t *op = NULL) {
        b1 = (gma_band_p<type1>*)band1;
        b2 = (gma_band_p<type2>*)band2;
        callback cb;
        cb.fct = &gma_two_bands_p::m_modulus;
        block_loop(cb, NULL, op);
    }
    virtual void decision(gma_band_t *a, gma_band_t *b, gma_band_t *c) {
        b1 = (gma_band_p<type1>*)a;
        b2 = (gma_band_p<type2>*)b;
        b3 = (gma_band_p<uint8_t>*)c;
        callback cb;
        cb.fct = &gma_two_bands_p::m_decision;
        block_loop(cb);
    }
    virtual gma_hash_t *zonal_min(gma_band_t *band1, gma_band_t *zones) {
        b1 = (gma_band_p<type1>*)band1;
        b2 = (gma_band_p<type2>*)zones;
        callback cb;
        cb.fct = &gma_two_bands_p::m_zonal_min;
        gma_object_t *retval = NULL;
        block_loop(cb, &retval);
        return (gma_hash_t *)retval;
    }
    virtual gma_hash_t *zonal_max(gma_band_t *band1, gma_band_t *zones) {
        b1 = (gma_band_p<type1>*)band1;
        b2 = (gma_band_p<type2>*)zones;
        callback cb;
        cb.fct = &gma_two_bands_p::m_zonal_max;
        gma_object_t *retval = NULL;
        block_loop(cb, &retval);
        return (gma_hash_t *)retval;
    }
    virtual void rim_by8(gma_band_t *rims, gma_band_t *zones) {
        b1 = (gma_band_p<type1>*)rims;
        b2 = (gma_band_p<type2>*)zones;
        callback cb;
        cb.fct = &gma_two_bands_p::m_rim_by8;
        block_loop(cb, NULL, NULL, 1);
    }
    virtual void fill_depressions(gma_band_t *filled_dem, gma_band_t *dem) {
        b1 = (gma_band_p<type1>*)filled_dem;
        b2 = (gma_band_p<type2>*)dem;
        double max_elev = b2->get_max()->value_as_double();
        b1->assign(max_elev);
        callback cb;
        cb.fct = &gma_two_bands_p::m_fill_depressions;
        gma_object_t *retval = NULL;
        block_loop(cb, &retval, NULL, 1);
    }
    virtual void D8(gma_band_t *fd, gma_band_t *dem) {
        b1 = (gma_band_p<type1>*)fd;
        b2 = (gma_band_p<type2>*)dem;
        callback cb;
        cb.fct = &gma_two_bands_p::m_D8;
        block_loop(cb, NULL, NULL, 1);
    }
    virtual void route_flats(gma_band_t *fd, gma_band_t *dem) {
        b1 = (gma_band_p<type1>*)fd;
        b2 = (gma_band_p<type2>*)dem;
        callback cb;
        cb.fct = &gma_two_bands_p::m_route_flats;
        gma_object_t *retval = NULL;
        block_loop(cb, &retval, NULL, 1);
    }
    virtual void upstream_area(gma_band_t *ua, gma_band_t *fd) {
        b1 = (gma_band_p<type1>*)ua;
        b2 = (gma_band_p<type2>*)fd;
        callback cb;
        cb.fct = &gma_two_bands_p::m_upstream_area;
        gma_object_t *retval = NULL;
        block_loop(cb, &retval, NULL, 1);
    }
    virtual void catchment(gma_band_t *catchment, gma_band_t *fd, gma_cell_t *outlet) {
        b1 = (gma_band_p<type1>*)catchment;
        b2 = (gma_band_p<type2>*)fd;
        callback cb;
        cb.fct = &gma_two_bands_p::m_catchment;
        gma_object_t *retval = NULL;
        block_loop(cb, &retval, outlet, 1);
    }
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
