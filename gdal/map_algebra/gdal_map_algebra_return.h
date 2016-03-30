typedef int (*gma_compute_value_callback)(gma_block *block, void*);

// histogram should be gma_hash<gma_int>

template<typename datatype>
int gma_histogram(gma_block *block, void *histogram) {
    gma_hash<gma_int> *hm = (gma_hash<gma_int>*)histogram;
    gma_cell_index i;
    for (i.y = 0; i.y < block->h; i.y++) {
        for (i.x = 0; i.x < block->w; i.x++) {
            datatype key = gma_block_cell(datatype, block, i);
            uint32_t val;
            if (hm->exists(key))
                hm->get(key)->add(1);
            else
                hm->put(key, new gma_int(val));
        }
    }
    return 1;
}

template<typename datatype>
void gma_proc_compute_value(GDALRasterBand *b, gma_compute_value_callback cb, void *ret_val) {
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
            int ret = cb(block, ret_val);
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

void *gma_compute_value(GDALRasterBand *b, gma_method_compute_value_t method) {
    void *ret_val = NULL;
    switch (method) {
    case gma_method_histogram: {
        gma_hash<gma_int> *h = new gma_hash<gma_int>;
        ret_val = (void*)h;
        switch (b->GetRasterDataType()) {
        case GDT_Byte:
            gma_proc_compute_value<uint8_t>(b, gma_histogram<uint8_t>, ret_val);
            break;
        case GDT_UInt16:
            gma_proc_compute_value<uint16_t>(b, gma_histogram<uint16_t>, ret_val);
            break;
        case GDT_Int32:
            gma_proc_compute_value<int32_t>(b, gma_histogram<int32_t>, ret_val);
            break;
        case GDT_UInt32:
            gma_proc_compute_value<uint32_t>(b, gma_histogram<uint32_t>, ret_val);
            break;
        default:
            goto not_implemented_for_this_datatype;
        }
        break;
    }
    default:
        goto unknown_method;
    }
    return ret_val;
not_implemented_for_this_datatype:
    fprintf(stderr, "Not implemented for this datatype.\n");
    return NULL;
unknown_method:
    fprintf(stderr, "Unknown method.\n");
    return NULL;
}
