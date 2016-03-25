typedef int (*gma_with_arg_callback)(void*, int, int, void*);

template<typename type>
int gma_add(void* block, int w, int h, void *to_add) {
    type a = *((type*)to_add);
    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            gma_typecast(type, block)[x+y*w] += a;
        }
    }
    return 2;
}

template<typename type>
void gma_with_arg_proc(GDALRasterBand *b, gma_with_arg_callback cb, void *arg) {
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
            type block[w_block*h_block];
            CPLErr e = b->ReadBlock(x_block, y_block, block);
            int ret = cb(block, w, h, arg);
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

template<typename type>
void gma_with_arg(GDALRasterBand *b, gma_method_with_arg_t method, type arg) {
    // need conversion method arg type -> cell type
    switch (method) {
    case gma_method_add: {
        switch (b->GetRasterDataType()) {
        case GDT_Int16: {
            int16_t a = arg;
            gma_with_arg_proc<int16_t>(b, gma_add<int16_t>, &a);
            break;
        }
        case GDT_Int32: {
            int32_t a = arg;
            gma_with_arg_proc<int32_t>(b, gma_add<int32_t>, &a);
            break;
        }
        case GDT_Float64: {
            double a = arg;
            gma_with_arg_proc<double>(b, gma_add<double>, &a);
            break;
        }
        }
        break;
    }
    }
}
