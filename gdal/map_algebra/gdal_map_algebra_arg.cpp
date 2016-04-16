#include "gdal_map_algebra_private.h"

template<typename datatype> struct gma_with_arg_callback {
    typedef int (*type)(gma_band<datatype>*, gma_block<datatype>*, gma_object_t*);
    type fct;
};

template<typename datatype>
int gma_assign(gma_band<datatype> *band, gma_block<datatype> *block, gma_object_t *arg) {
    if (arg->get_class() != gma_number) {
        fprintf(stderr, "Argument is not a number.");
        return 0;
    }
    datatype a = (datatype)(((gma_number_t*)arg)->value_as_double());
    gma_cell_index i;
    for (i.y = 0; i.y < block->h; i.y++) {
        for (i.x = 0; i.x < block->w; i.x++) {
            if (band->cell_is_nodata(block, i)) continue;
            block->cell(i) = a;
        }
    }
    return 2;
}

template<typename datatype>
int gma_assign_all(gma_band<datatype> *band, gma_block<datatype> *block, gma_object_t *arg) {
    if (arg->get_class() != gma_number) {
        fprintf(stderr, "Argument is not a number.");
        return 0;
    }
    datatype a = (datatype)(((gma_number_t*)arg)->value_as_double());
    gma_cell_index i;
    for (i.y = 0; i.y < block->h; i.y++) {
        for (i.x = 0; i.x < block->w; i.x++) {
            block->cell(i) = a;
        }
    }
    return 2;
}

template<typename datatype>
int gma_add(gma_band<datatype> *band, gma_block<datatype> *block, gma_object_t *arg) {
    if (arg->get_class() != gma_number) {
        fprintf(stderr, "Argument is not a number.");
        return 0;
    }
    datatype a = (datatype)(((gma_number_t*)arg)->value_as_double());
    gma_cell_index i;
    for (i.y = 0; i.y < block->h; i.y++) {
        for (i.x = 0; i.x < block->w; i.x++) {
            if (band->cell_is_nodata(block, i)) continue;
            block->cell(i) += a;
        }
    }
    return 2;
}

template<typename datatype>
int gma_subtract(gma_band<datatype> *band, gma_block<datatype> *block, gma_object_t *arg) {
    if (arg->get_class() != gma_number) {
        fprintf(stderr, "Argument is not a number.");
        return 0;
    }
    datatype a = (datatype)(((gma_number_t*)arg)->value_as_double());
    gma_cell_index i;
    for (i.y = 0; i.y < block->h; i.y++) {
        for (i.x = 0; i.x < block->w; i.x++) {
            if (band->cell_is_nodata(block, i)) continue;
            block->cell(i) -= a;
        }
    }
    return 2;
}

template<typename datatype>
int gma_multiply(gma_band<datatype> *band, gma_block<datatype> *block, gma_object_t *arg) {
    if (arg->get_class() != gma_number) {
        fprintf(stderr, "Argument is not a number.");
        return 0;
    }
    datatype a = (datatype)(((gma_number_t*)arg)->value_as_double());
    gma_cell_index i;
    for (i.y = 0; i.y < block->h; i.y++) {
        for (i.x = 0; i.x < block->w; i.x++) {
            if (band->cell_is_nodata(block, i)) continue;
            block->cell(i) *= a;
        }
    }
    return 2;
}

template<typename datatype>
int gma_divide(gma_band<datatype> *band, gma_block<datatype> *block, gma_object_t *arg) {
    if (arg->get_class() != gma_number) {
        fprintf(stderr, "Argument is not a number.");
        return 0;
    }
    datatype a = (datatype)(((gma_number_t*)arg)->value_as_double());
    gma_cell_index i;
    for (i.y = 0; i.y < block->h; i.y++) {
        for (i.x = 0; i.x < block->w; i.x++) {
            if (band->cell_is_nodata(block, i)) continue;
            block->cell(i) /= a;
        }
    }
    return 2;
}

template<typename datatype>
int gma_modulus(gma_band<datatype> *band, gma_block<datatype> *block, gma_object_t *arg) {
    if (arg->get_class() != gma_number) {
        fprintf(stderr, "Argument is not a number.");
        return 0;
    }
    datatype a = (datatype)(((gma_number_t*)arg)->value_as_double());
    gma_cell_index i;
    for (i.y = 0; i.y < block->h; i.y++) {
        for (i.x = 0; i.x < block->w; i.x++) {
            if (band->cell_is_nodata(block, i)) continue;
            block->cell(i) %= a;
        }
    }
    return 2;
}

template<>
int gma_modulus<float>(gma_band<float> *band, gma_block<float> *block, gma_object_t *arg) {
    fprintf(stderr, "invalid type ‘float’ to binary operator %%");
    return 0;
}

template<>
int gma_modulus<double>(gma_band<double> *band, gma_block<double> *block, gma_object_t *arg) {
    fprintf(stderr, "invalid type ‘double’ to binary operator %%");
    return 0;
}

template<typename datatype>
int gma_classify_m(gma_band<datatype> *band, gma_block<datatype> *block, gma_object_t *classifier) {
    if (classifier->get_class() != gma_classifier) {
        fprintf(stderr, "Wrong kind of argument.\n");
        return 0;
    }
    gma_classifier_p<datatype> *c = (gma_classifier_p<datatype> *)classifier;
    gma_cell_index i;
    for (i.y = 0; i.y < block->h; i.y++) {
        for (i.x = 0; i.x < block->w; i.x++) {
            datatype a = block->cell(i);
            if (band->is_nodata(a))
                block->cell(i) = c->classify(a);
        }
    }
    return 2;
}

template<typename datatype>
int gma_cell_callback_m(gma_band<datatype> *band, gma_block<datatype> *block, gma_object_t *callback) {
    if (callback->get_class() != gma_cell_callback) {
        fprintf(stderr, "Wrong kind of argument.\n");
        return 0;
    }
    gma_cell_index i;
    int retval;
    for (i.y = 0; i.y < block->h; i.y++) {
        for (i.x = 0; i.x < block->w; i.x++) {
            datatype a = block->cell(i);
            if (band->is_nodata(a)) continue;
            gma_cell_index gi = band->global_cell_index(block, i);
            gma_cell_p<datatype> *c = new gma_cell_p<datatype>(gi.x, gi.y, a);
            retval = ((gma_cell_callback_p*)callback)->m_callback(c, ((gma_cell_callback_p*)callback)->m_user_data);
            if (retval == 0) return 0;
            if (retval == 2)
                block->cell(i) = c->value();
        }
    }
    return retval;
}

template<typename datatype>
void gma_with_arg_proc(GDALRasterBand *b, gma_with_arg_callback<datatype> cb, gma_object_t *arg) {
    gma_band<datatype> *band = new gma_band<datatype>(b);
    gma_block_index i;
    for (i.y = 0; i.y < band->h_blocks; i.y++) {
        for (i.x = 0; i.x < band->w_blocks; i.x++) {
            band->add_to_cache(i);
            gma_block<datatype> *block = band->get_block(i);
            int ret = cb.fct(band, block, arg);
            switch (ret) {
            case 0: return;
            case 1: break;
            case 2: {
                CPLErr e = band->write_block(block);
            }
            }
        }
    }
}

void gma_with_arg(GDALRasterBand *b, gma_method_with_arg_t method, gma_object_t *arg) {
    switch (method) {
    case gma_method_assign:
        type_switch_arg(gma_assign);
        break;
    case gma_method_add:
        type_switch_arg(gma_add);
        break;
    case gma_method_subtract:
        type_switch_arg(gma_subtract);
        break;
    case gma_method_multiply:
        type_switch_arg(gma_multiply);
        break;
    case gma_method_divide:
        type_switch_arg(gma_divide);
        break;
    case gma_method_modulus:
        type_switch_arg(gma_modulus);
        break;
    case gma_method_classify:
        type_switch_arg(gma_classify_m);
        break;
    case gma_method_cell_callback:
        type_switch_arg(gma_cell_callback_m);
        break;
    default:
        goto unknown_method;
    }
    return;
not_implemented_for_this_datatype:
    fprintf(stderr, "Not implemented for this datatype.\n");
    return;
unknown_method:
    fprintf(stderr, "Unknown method.\n");
    return;
}
