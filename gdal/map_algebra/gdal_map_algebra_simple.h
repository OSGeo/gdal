template<typename datatype>
int gma_print(gma_block *block) {
}

#define _gma_print(datatype, format)                                    \
    template<>                                                          \
    int gma_print<datatype>(gma_block *block) {                         \
        gma_cell_index i;                                               \
        for (i.y = 0; i.y < block->h; i.y++) {                          \
            for (i.x = 0; i.x < block->w; i.x++) {                      \
                printf(format" ", gma_block_cell(datatype, block, i));  \
            }                                                           \
            printf("\n");                                               \
        }                                                               \
        return 1;                                                       \
    }

_gma_print(uint8_t, "%03i")
_gma_print(uint16_t, "%04i")
_gma_print(int16_t, "%04i")
_gma_print(uint32_t, "%04i")
_gma_print(int32_t, "%04i")
_gma_print(float, "%04.1f")
_gma_print(double, "%04.2f")

template<typename datatype>
int gma_rand(gma_block *block) {
    gma_cell_index i;
    for (i.y = 0; i.y < block->h; i.y++) {
        for (i.x = 0; i.x < block->w; i.x++) {
            gma_block_cell(datatype, block, i) = rand() % 20;
        }
    }
    return 2;
}

template<typename datatype>
void gma_proc_simple(GDALRasterBand *b, gma_method_t method) {
    if (GDALDataTypeTraits<datatype>::datatype != b->GetRasterDataType()) {
        fprintf(stderr, "band and mapper are incompatible.");
        return;
    }
    gma_band band = gma_band_initialize(b);
    gma_block_index i;
    for (i.y = 0; i.y < band.h_blocks; i.y++) {
        for (i.x = 0; i.x < band.w_blocks; i.x++) {
            gma_band_add_to_cache(&band, i);
            gma_block *block = gma_band_get_block(band, i);
            int ret;
            switch (method) {
            case gma_method_print:
                ret = gma_print<datatype>(block);
                break;
            case gma_method_rand:
                ret = gma_rand<datatype>(block);
                break;
            }
            if (ret == 2)
                CPLErr e = gma_band_write_block(band, block);
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
