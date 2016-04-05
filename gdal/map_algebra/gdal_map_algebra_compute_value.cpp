#include "gdal_map_algebra_private.h"

typedef int (*gma_compute_value_callback)(gma_band, gma_block*, gma_object_t**, gma_object_t*);

template<typename datatype>
int gma_get_min(gma_band band, gma_block *block, gma_object_t **retval, gma_object_t *arg) {
    gma_number_p<datatype> *rv;
    if (*retval == NULL) {
        rv = new gma_number_p<datatype>;
        *retval = rv;
    } else
        rv = (gma_number_p<datatype> *)*retval;
    gma_cell_index i;
    for (i.y = 0; i.y < block->h; i.y++) {
        for (i.x = 0; i.x < block->w; i.x++) {
            datatype x = gma_block_cell(datatype, block, i);
            if (!rv->defined() || x < rv->value())
                rv->set_value(x);
        }
    }
    return 1;
}

template<typename datatype>
int gma_get_max(gma_band band, gma_block *block, gma_object_t **retval, gma_object_t *arg) {
    gma_number_p<datatype> *rv;
    if (*retval == NULL) {
        rv = new gma_number_p<datatype>;
        *retval = rv;
    } else
        rv = (gma_number_p<datatype> *)*retval;
    gma_cell_index i;
    for (i.y = 0; i.y < block->h; i.y++) {
        for (i.x = 0; i.x < block->w; i.x++) {
            datatype x = gma_block_cell(datatype, block, i);
            if (!rv->defined() || x > rv->value())
                rv->set_value(x);
        }
    }
    return 1;
}

// histogram should be defined by the user, at least its kind: range | value => cound
// value => count is meaningful only for bands with integer cell value

template<typename datatype>
int gma_compute_histogram(gma_band band, gma_block *block, gma_object_t **retval, gma_object_t *arg) {
    gma_histogram_p<datatype> *hm;
    if (*retval == NULL) {
        hm = new gma_histogram_p<datatype>(arg);
        *retval = hm;
    } else
        hm = (gma_histogram_p<datatype>*)*retval;
    gma_cell_index i;
    // use type traits 
    for (i.y = 0; i.y < block->h; i.y++) {
        for (i.x = 0; i.x < block->w; i.x++) {
            datatype key = gma_block_cell(datatype, block, i);
            if (hm->exists(key))
                ((gma_number_p<unsigned int>*)hm->get(key))->add(1);
            else
                hm->put(key, new gma_number_p<unsigned int>(1));
        }
    }
    return 1;
}

template<typename datatype>
int gma_zonal_neighbors(gma_band band, gma_block *block, gma_object_t **retval, gma_object_t *arg) {
    gma_hash_p<gma_hash_p<gma_number_p<datatype> > > *h;
    gma_cell_index i;
    for (i.y = 0; i.y < block->h; i.y++) {
        for (i.x = 0; i.x < block->w; i.x++) {
            datatype me = gma_block_cell(datatype, block, i);
            gma_hash_p<gma_number_p<datatype> > *ns;
            if (h->exists(me))
                ns = (gma_hash_p<gma_number_p<datatype> > *)h->get(me);
            else {
                ns = new gma_hash_p<gma_number_p<datatype> >;
                h->put(me, ns);
            }

            gma_cell_index in = gma_cell_first_neighbor(i);
            for (int neighbor = 1; neighbor < 9; neighbor++) {
                gma_cell_move_to_neighbor(in, neighbor);
                datatype n;

                if (!gma_value_from_other_band<datatype>(band, block, in, band, &n)) {
                    ns->put(-1, new gma_number_p<datatype>(1));
                    continue;  // we are at border and this is outside
                }
                
                if (n != me && !ns->exists(n))
                    ns->put(n, new gma_number_p<datatype>(1));
                
            }

        }
    }
    return 1;
}

template<typename datatype>
int gma_get_cells(gma_band band, gma_block *block, gma_object_t **retval, gma_object_t *arg) {
    gma_array<gma_cell_p<datatype> > *c;
    gma_cell_index i;
    for (i.y = 0; i.y < block->h; i.y++) {
        for (i.x = 0; i.x < block->w; i.x++) {
            datatype me = gma_block_cell(datatype, block, i);
            // global cell index
            int x = block->index.x * band.w_block + i.x;
            int y = block->index.y * band.h_block + i.y;
            if (me) c->push(new gma_cell_p<datatype>(x, y, me));
        }
    }
    return 1;
}

template<typename datatype>
void gma_proc_compute_value(GDALRasterBand *b, gma_compute_value_callback cb, gma_object_t **retval, gma_object_t *arg, int focal_distance) {
    gma_band band = gma_band_initialize(b);
    gma_block_index i;
    for (i.y = 0; i.y < band.h_blocks; i.y++) {
        for (i.x = 0; i.x < band.w_blocks; i.x++) {
            gma_band_add_to_cache(&band, i);
            gma_block *block = gma_band_get_block(band, i);
            CPLErr e = gma_band_update_cache(&band, band, block, focal_distance);
            int ret = cb(band, block, retval, arg);
            switch (ret) {
            case 0: return;
            case 1: break;
            case 2: {
                CPLErr e = gma_band_write_block(band, block);
            }
            }
        }
    }
}

gma_object_t *gma_compute_value(GDALRasterBand *b, gma_method_compute_value_t method, gma_object_t *arg) {
    gma_object_t *retval = NULL;
    switch (method) {
    case gma_method_get_min:
        type_switch_single(gma_get_min, 0);
        break;
    case gma_method_get_max:
        type_switch_single(gma_get_max, 0);
        break;
    case gma_method_histogram:
        type_switch_single(gma_compute_histogram, 0);
        break;
    case gma_method_zonal_neighbors:
        type_switch_single(gma_zonal_neighbors, 1);
        break;
    case gma_method_get_cells: {
        type_switch_single(gma_get_cells, 0);
        break;
    }
    default:
        goto unknown_method;
    }
    return retval;
not_implemented_for_this_datatype:
    fprintf(stderr, "Not implemented for this datatype.\n");
    return NULL;
unknown_method:
    fprintf(stderr, "Unknown method.\n");
    return NULL;
}
