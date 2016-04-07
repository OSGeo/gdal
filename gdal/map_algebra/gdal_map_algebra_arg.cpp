#include "gdal_map_algebra_private.h"

typedef int (*gma_with_arg_callback)(gma_band, gma_block*, gma_object_t*);

template<typename datatype>
int gma_assign(gma_band band, gma_block *block, gma_object_t *arg) {
    if (arg->get_class() != gma_number) {
        fprintf(stderr, "Argument is not a number.");
        return 0;
    }
    datatype a = (datatype)(((gma_number_t*)arg)->value_as_double());
    gma_cell_index i;
    for (i.y = 0; i.y < block->h; i.y++) {
        for (i.x = 0; i.x < block->w; i.x++) {
            gma_block_cell(datatype, block, i) = a;
        }
    }
    return 2;
}

template<typename datatype>
int gma_add(gma_band band, gma_block *block, gma_object_t *arg) {
    if (arg->get_class() != gma_number) {
        fprintf(stderr, "Argument is not a number.");
        return 0;
    }
    datatype a = (datatype)(((gma_number_t*)arg)->value_as_double());
    gma_cell_index i;
    for (i.y = 0; i.y < block->h; i.y++) {
        for (i.x = 0; i.x < block->w; i.x++) {
            gma_block_cell(datatype, block, i) += a;
        }
    }
    return 2;
}

template<typename datatype>
int gma_subtract(gma_band band, gma_block *block, gma_object_t *arg) {
    if (arg->get_class() != gma_number) {
        fprintf(stderr, "Argument is not a number.");
        return 0;
    }
    datatype a = (datatype)(((gma_number_t*)arg)->value_as_double());
    gma_cell_index i;
    for (i.y = 0; i.y < block->h; i.y++) {
        for (i.x = 0; i.x < block->w; i.x++) {
            gma_block_cell(datatype, block, i) -= a;
        }
    }
    return 2;
}

template<typename datatype>
int gma_multiply(gma_band band, gma_block *block, gma_object_t *arg) {
    if (arg->get_class() != gma_number) {
        fprintf(stderr, "Argument is not a number.");
        return 0;
    }
    datatype a = (datatype)(((gma_number_t*)arg)->value_as_double());
    gma_cell_index i;
    for (i.y = 0; i.y < block->h; i.y++) {
        for (i.x = 0; i.x < block->w; i.x++) {
            gma_block_cell(datatype, block, i) *= a;
        }
    }
    return 2;
}

template<typename datatype>
int gma_divide(gma_band band, gma_block *block, gma_object_t *arg) {
    if (arg->get_class() != gma_number) {
        fprintf(stderr, "Argument is not a number.");
        return 0;
    }
    datatype a = (datatype)(((gma_number_t*)arg)->value_as_double());
    gma_cell_index i;
    for (i.y = 0; i.y < block->h; i.y++) {
        for (i.x = 0; i.x < block->w; i.x++) {
            gma_block_cell(datatype, block, i) /= a;
        }
    }
    return 2;
}

template<typename datatype>
int gma_modulus(gma_band band, gma_block *block, gma_object_t *arg) {
    if (arg->get_class() != gma_number) {
        fprintf(stderr, "Argument is not a number.");
        return 0;
    }
    datatype a = (datatype)(((gma_number_t*)arg)->value_as_double());
    gma_cell_index i;
    for (i.y = 0; i.y < block->h; i.y++) {
        for (i.x = 0; i.x < block->w; i.x++) {
            gma_block_cell(datatype, block, i) %= a;
        }
    }
    return 2;
}

template<>
int gma_modulus<float>(gma_band band, gma_block *block, gma_object_t *arg) {
    fprintf(stderr, "invalid type ‘float’ to binary operator %%");
    return 0;
}

template<>
int gma_modulus<double>(gma_band band, gma_block *block, gma_object_t *arg) {
    fprintf(stderr, "invalid type ‘double’ to binary operator %%");
    return 0;
}

template<typename datatype>
int gma_classify_m(gma_band band, gma_block *block, gma_object_t *classifier) {
    if (classifier->get_class() != gma_classifier) {
        fprintf(stderr, "Wrong kind of argument.\n");
        return 0;
    }
    gma_classifier_p<datatype> *c = (gma_classifier_p<datatype> *)classifier;
    gma_cell_index i;
    for (i.y = 0; i.y < block->h; i.y++) {
        for (i.x = 0; i.x < block->w; i.x++) {
            datatype a = gma_block_cell(datatype, block, i);
            gma_block_cell(datatype, block, i) = c->classify(a);
        }
    }
    return 2;
}

template<typename datatype>
int gma_cell_callback_m(gma_band band, gma_block *block, gma_object_t *callback) {
    if (callback->get_class() != gma_cell_callback) {
        fprintf(stderr, "Wrong kind of argument.\n");
        return 0;
    }
    gma_cell_index i;
    int retval;
    for (i.y = 0; i.y < block->h; i.y++) {
        for (i.x = 0; i.x < block->w; i.x++) {
            datatype a = gma_block_cell(datatype, block, i);
            int x = block->index.x * band.w_block + i.x;
            int y = block->index.y * band.h_block + i.y;
            gma_cell_p<datatype> *c = new gma_cell_p<datatype>(x,y,a);
            retval = ((gma_cell_callback_p*)callback)->m_callback(c);
            if (retval == 0) return 0;
            if (retval == 2)
                gma_block_cell(datatype, block, i) = c->value();
        }
    }
    return retval;
}

template<typename datatype>
void gma_with_arg_proc(GDALRasterBand *b, gma_with_arg_callback cb, gma_object_t *arg) {
    gma_band band = gma_band_initialize(b);
    gma_block_index i;
    for (i.y = 0; i.y < band.h_blocks; i.y++) {
        for (i.x = 0; i.x < band.w_blocks; i.x++) {
            gma_band_add_to_cache(&band, i);
            gma_block *block = gma_band_get_block(band, i);
            int ret = cb(band, block, arg);
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
