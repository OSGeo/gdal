typedef int (*gma_with_arg_callback)(gma_block*, void*);

template <typename datatype>
class gma_mapper {
private:
    gma_hash<gma_int> *m_mapper;
public:
    gma_mapper(gma_hash<gma_int> *mapper) {
        m_mapper = mapper;
    }
    int map(datatype *value) {
        if (m_mapper->exists(*value)) {
            *value = m_mapper->get(*value)->value();
            return 1;
        } else
            return 0;
    }
};

template<typename datatype>
int gma_add(gma_block *block, void *to_add) {
    datatype a = *(datatype *)to_add;
    gma_cell_index i;
    for (i.y = 0; i.y < block->h; i.y++) {
        for (i.x = 0; i.x < block->w; i.x++) {
            gma_block_cell(datatype, block, i) += a;
        }
    }
    return 2;
}

template<typename datatype>
int gma_map_block(gma_block *block, gma_mapper<datatype> *mapper) {
    gma_cell_index i;
    for (i.y = 0; i.y < block->h; i.y++) {
        for (i.x = 0; i.x < block->w; i.x++) {
            datatype a = gma_block_cell(datatype, block, i);
            if (mapper->map(&a))
                gma_block_cell(datatype, block, i) = a;
        }
    }
    return 2;
}

template<typename datatype>
void gma_with_arg_proc(GDALRasterBand *b, gma_with_arg_callback cb, void *arg) {
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

template<typename datatype>
void gma_with_arg(GDALRasterBand *b, gma_method_with_arg_t method, datatype arg) {
    // need conversion method arg type -> cell type?
    if (GDALDataTypeTraits<datatype>::datatype != b->GetRasterDataType()) {
        fprintf(stderr, "band and mapper are incompatible.");
        return;
    }
    switch (method) {
    case gma_method_add:
        gma_with_arg_proc<datatype>(b, gma_add<datatype>, &arg);
    }
}

template<typename datatype>
void gma_map(GDALRasterBand *b, gma_mapper<datatype> *mapper) {
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
            gma_map_block<datatype>(block, mapper);
            CPLErr e = gma_band_write_block(band, block);
        }
    }
}
