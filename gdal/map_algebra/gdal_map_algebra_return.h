typedef int (*gma_compute_value_callback)(void*, int, int, void**);

template<typename data_t>
int gma_histogram(void* block, int w, int h, void **histogram) {
    int *hm;
    if (!(*histogram)) {
        hm = (int*)CPLMalloc(100*sizeof(int));
        for (int i = 0; i < 100; i++) {
            hm[i] = 0;
        }
        *histogram = hm;
    } else
        hm = (int*)*histogram;
    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            hm[gma_typecast(data_t, block)[x+y*w]]++;
        }
    }
    return 1;
}

template<typename data_t>
void gma_proc_compute_value(GDALRasterBand *b, gma_compute_value_callback cb, void **ret_val) {
    int w_band = b->GetXSize(), h_band = b->GetYSize();
    int w_block, h_block;
    b->GetBlockSize(&w_block, &h_block);
    int w_blocks = (w_band + w_block - 1) / w_block;
    int h_blocks = (h_band + h_block - 1) / h_block;
    for (int y_block = 0; y_block < h_blocks; y_block++) {
        for (int x_block = 0; x_block < w_blocks; x_block++) {
            int w, h;
            if( (x_block+1) * w_block > w_band )
                w = w_band - x_block * w_block;
            else
                w = w_block;
            if( (y_block+1) * h_block > h_band )
                h = h_band - y_block * h_block;
            else
                h = h_block;
            data_t block[w_block*h_block];
            CPLErr e = b->ReadBlock(x_block, y_block, block);
            int ret = cb(block, w, h, ret_val);
            switch (ret) {
            case 0: return;
            case 1: break;
            case 2: {
                e = b->WriteBlock(x_block, y_block, block);
            }
            }
        }
    }
}

void *gma_compute_value(GDALRasterBand *b, gma_method_compute_value_t method) {
    void *ret_val = NULL;
    switch (method) {
    case gma_method_histogram: {
        switch (b->GetRasterDataType()) {
        case GDT_Byte:
            gma_proc_compute_value<int32_t>(b, gma_histogram<char>, &ret_val);
            break;
        case GDT_Int32:
            gma_proc_compute_value<int32_t>(b, gma_histogram<int32_t>, &ret_val);
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
