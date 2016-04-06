#include "gdal_map_algebra_private.h"

typedef int (*gma_with_arg_callback)(gma_block*, gma_object_t*);

template<typename datatype>
int gma_assign(gma_block *block, gma_object_t *arg) {
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
int gma_add(gma_block *block, gma_object_t *arg) {
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
int gma_subtract(gma_block *block, gma_object_t *arg) {
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
int gma_multiply(gma_block *block, gma_object_t *arg) {
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
int gma_divide(gma_block *block, gma_object_t *arg) {
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
int gma_modulus(gma_block *block, gma_object_t *arg) {
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
int gma_modulus<float>(gma_block *block, gma_object_t *arg) {
    fprintf(stderr, "invalid type ‘float’ to binary operator %%");
    return 0;
}

template<>
int gma_modulus<double>(gma_block *block, gma_object_t *arg) {
    fprintf(stderr, "invalid type ‘double’ to binary operator %%");
    return 0;
}

template<typename datatype>
int gma_map_block(gma_block *block, gma_object_t *mapper) {
    if (mapper->get_class() != gma_reclassifier) {
        fprintf(stderr, "Wrong kind of argument.\n");
        return 0;
    }
    gma_mapper_p<datatype> *m = (gma_mapper_p<datatype> *)mapper;
    gma_cell_index i;
    for (i.y = 0; i.y < block->h; i.y++) {
        for (i.x = 0; i.x < block->w; i.x++) {
            datatype a = gma_block_cell(datatype, block, i);
            if (m->map(&a))
                gma_block_cell(datatype, block, i) = a;
        }
    }
    return 2;
}

template<typename datatype>
void gma_with_arg_proc(GDALRasterBand *b, gma_with_arg_callback cb, gma_object_t *arg) {
    gma_band band = gma_band_initialize(b);
    gma_block_index i;
    for (i.y = 0; i.y < band.h_blocks; i.y++) {
        for (i.x = 0; i.x < band.w_blocks; i.x++) {
            gma_band_add_to_cache(&band, i);
            gma_block *block = gma_band_get_block(band, i);
            int ret = cb(block, arg);
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
    case gma_method_map:
        type_switch_arg(gma_map_block);
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
