#include "gdal_map_algebra_private.h"

template<typename datatype>
int gma_print(gma_band<datatype> *band, gma_block<datatype> *block) {
}

#define _gma_print(type, format, space)                                 \
    template<>                                                          \
    int gma_print<type>(gma_band<type> *band, gma_block<type> *block) { \
        gma_cell_index i;                                               \
        for (i.y = 0; i.y < block->h; i.y++) {                          \
            for (i.x = 0; i.x < block->w; i.x++) {                      \
                if (band->cell_is_nodata(block, i))                     \
                    printf(space" ");                                   \
                else                                                    \
                    printf(format" ", block->cell(i));                  \
            }                                                           \
            printf("\n");                                               \
        }                                                               \
        return 1;                                                       \
    }

_gma_print(uint8_t, "%03i", "   ")
_gma_print(uint16_t, "%04i", "    ")
_gma_print(int16_t, "%04i", "    ")
_gma_print(uint32_t, "%04i", "    ")
_gma_print(int32_t, "%04i", "    ")
_gma_print(float, "%04.1f", "    ")
_gma_print(double, "%04.2f", "    ")

template<typename datatype>
int gma_rand(gma_block<datatype> *block) {
    gma_cell_index i;
    for (i.y = 0; i.y < block->h; i.y++) {
        for (i.x = 0; i.x < block->w; i.x++) {
            block->cell(i) = rand();
        }
    }
    return 2;
}

#include <math.h>

template<typename datatype>
int gma_abs(gma_band<datatype> *band, gma_block<datatype> *block) {
    gma_cell_index i;
    for (i.y = 0; i.y < block->h; i.y++) {
        for (i.x = 0; i.x < block->w; i.x++) {
            if (band->cell_is_nodata(block, i)) continue;
            block->cell(i) = abs(block->cell(i));
        }
    }
    return 2;
}
// fabs for floats

template<typename datatype>
int gma_exp(gma_band<datatype> *band, gma_block<datatype> *block) {
    gma_cell_index i;
    for (i.y = 0; i.y < block->h; i.y++) {
        for (i.x = 0; i.x < block->w; i.x++) {
            if (band->cell_is_nodata(block, i)) continue;
            block->cell(i) = exp(block->cell(i));
        }
    }
    return 2;
}

template<typename datatype>
int gma_exp2(gma_band<datatype> *band, gma_block<datatype> *block) {
    gma_cell_index i;
    for (i.y = 0; i.y < block->h; i.y++) {
        for (i.x = 0; i.x < block->w; i.x++) {
            if (band->cell_is_nodata(block, i)) continue;
            block->cell(i) = exp2(block->cell(i));
        }
    }
    return 2;
}

template<typename datatype>
int gma_log(gma_band<datatype> *band, gma_block<datatype> *block) {
    gma_cell_index i;
    for (i.y = 0; i.y < block->h; i.y++) {
        for (i.x = 0; i.x < block->w; i.x++) {
            if (band->cell_is_nodata(block, i)) continue;
            block->cell(i) = log(block->cell(i));
        }
    }
    return 2;
}

template<typename datatype>
int gma_log2(gma_band<datatype> *band, gma_block<datatype> *block) {
    gma_cell_index i;
    for (i.y = 0; i.y < block->h; i.y++) {
        for (i.x = 0; i.x < block->w; i.x++) {
            if (band->cell_is_nodata(block, i)) continue;
            block->cell(i) = log2(block->cell(i));
        }
    }
    return 2;
}

template<typename datatype>
int gma_log10(gma_band<datatype> *band, gma_block<datatype> *block) {
    gma_cell_index i;
    for (i.y = 0; i.y < block->h; i.y++) {
        for (i.x = 0; i.x < block->w; i.x++) {
            if (band->cell_is_nodata(block, i)) continue;
            block->cell(i) = log10(block->cell(i));
        }
    }
    return 2;
}

template<typename datatype>
int gma_sqrt(gma_band<datatype> *band, gma_block<datatype> *block) {
    gma_cell_index i;
    for (i.y = 0; i.y < block->h; i.y++) {
        for (i.x = 0; i.x < block->w; i.x++) {
            if (band->cell_is_nodata(block, i)) continue;
            block->cell(i) = sqrt(block->cell(i));
        }
    }
    return 2;
}

template<typename datatype>
int gma_sin(gma_band<datatype> *band, gma_block<datatype> *block) {
    gma_cell_index i;
    for (i.y = 0; i.y < block->h; i.y++) {
        for (i.x = 0; i.x < block->w; i.x++) {
            if (band->cell_is_nodata(block, i)) continue;
            block->cell(i) = sin(block->cell(i));
        }
    }
    return 2;
}

template<typename datatype>
int gma_cos(gma_band<datatype> *band, gma_block<datatype> *block) {
    gma_cell_index i;
    for (i.y = 0; i.y < block->h; i.y++) {
        for (i.x = 0; i.x < block->w; i.x++) {
            if (band->cell_is_nodata(block, i)) continue;
            block->cell(i) = cos(block->cell(i));
        }
    }
    return 2;
}

template<typename datatype>
int gma_tan(gma_band<datatype> *band, gma_block<datatype> *block) {
    gma_cell_index i;
    for (i.y = 0; i.y < block->h; i.y++) {
        for (i.x = 0; i.x < block->w; i.x++) {
            if (band->cell_is_nodata(block, i)) continue;
            block->cell(i) = tan(block->cell(i));
        }
    }
    return 2;
}

template<typename datatype>
int gma_ceil(gma_band<datatype> *band, gma_block<datatype> *block) {
    gma_cell_index i;
    for (i.y = 0; i.y < block->h; i.y++) {
        for (i.x = 0; i.x < block->w; i.x++) {
            if (band->cell_is_nodata(block, i)) continue;
            block->cell(i) = ceil(block->cell(i));
        }
    }
    return 2;
}

template<typename datatype>
int gma_floor(gma_band<datatype> *band, gma_block<datatype> *block) {
    gma_cell_index i;
    for (i.y = 0; i.y < block->h; i.y++) {
        for (i.x = 0; i.x < block->w; i.x++) {
            if (band->cell_is_nodata(block, i)) continue;
            block->cell(i) = floor(block->cell(i));
        }
    }
    return 2;
}

template<typename datatype>
void gma_proc_simple(GDALRasterBand *b, gma_method_t method) {
    gma_band<datatype> *band = new gma_band<datatype>(b);
    gma_block_index i;
    for (i.y = 0; i.y < band->h_blocks; i.y++) {
        for (i.x = 0; i.x < band->w_blocks; i.x++) {
            band->add_to_cache(i);
            gma_block<datatype> *block = band->get_block(i);
            int ret;
            switch (method) {
            case gma_method_print:
                ret = gma_print<datatype>(band, block);
                break;
            case gma_method_rand:
                ret = gma_rand<datatype>(block);
                break;
            case gma_method_abs:
                ret = gma_abs<datatype>(band, block);
                break;
            case gma_method_exp:
                ret = gma_exp<datatype>(band, block);
                break;
            case gma_method_exp2:
                ret = gma_exp2<datatype>(band, block);
                break;
            case gma_method_log:
                ret = gma_log<datatype>(band, block);
                break;
            case gma_method_log2:
                ret = gma_log2<datatype>(band, block);
                break;
            case gma_method_log10:
                ret = gma_log10<datatype>(band, block);
                break;
            case gma_method_sqrt:
                ret = gma_sqrt<datatype>(band, block);
                break;
            case gma_method_sin:
                ret = gma_sin<datatype>(band, block);
                break;
            case gma_method_cos:
                ret = gma_cos<datatype>(band, block);
                break;
            case gma_method_tan:
                ret = gma_tan<datatype>(band, block);
                break;
            case gma_method_ceil:
                ret = gma_ceil<datatype>(band, block);
                break;
            case gma_method_floor:
                ret = gma_floor<datatype>(band, block);
                break;
            }
            if (ret == 2)
                CPLErr e = band->write_block(block);
        }
    }
}

void gma_simple(GDALRasterBand *b, gma_method_t method) {
    switch (b->GetRasterDataType()) {
    case GDT_Byte:
        gma_proc_simple<uint8_t>(b, method);
        break;
    case GDT_UInt16:
        gma_proc_simple<uint16_t>(b, method);
        break;
    case GDT_Int16:
        gma_proc_simple<int16_t>(b, method);
        break;
    case GDT_UInt32:
        gma_proc_simple<uint32_t>(b, method);
        break;
    case GDT_Int32:
        gma_proc_simple<int32_t>(b, method);
        break;
    case GDT_Float32:
        gma_proc_simple<float>(b, method);
        break;
    case GDT_Float64:
        gma_proc_simple<double>(b, method);
        break;
    default:
        fprintf(stderr, "datatype not supported");
    }
}
