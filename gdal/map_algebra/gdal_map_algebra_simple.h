typedef int (*gma_simple_callback)(void*, int, int);

#define gma_typecast(type, var) ((type*)(var))

// can use templates if use streams
#define gma_print(type) int gma_print_##type(void* block, int w, int h) { \
        for (int y = 0; y < h; y++) {                                   \
            for (int x = 0; x < w; x++) {                               \
                printf("%02i ", gma_typecast(type, block)[x+y*w]);      \
            }                                                           \
            printf("\n");                                               \
        }                                                               \
        return 1;                                                       \
    }

gma_print(int16_t)
gma_print(int32_t)

int gma_print_double(void* block, int w, int h) {
    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            printf("%04.1f ", gma_typecast(double, block)[x+y*w]);
        }
        printf("\n");
    }
    return 1;
}

template<typename data_t>
int gma_rand(void* block, int w, int h) {
    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            gma_typecast(data_t, block)[x+y*w] = rand() % 20;
        }
    }
    return 2;
}

template<typename data_t>
void gma_proc_simple(GDALRasterBand *b, gma_simple_callback cb) {
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
            int ret = cb(block, w, h);
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

void gma_simple(GDALRasterBand *b, gma_method_t method) {
    switch (method) {
    case gma_method_rand: {
        switch (b->GetRasterDataType()) {
        case GDT_Int32:
            gma_proc_simple<int32_t>(b, gma_rand<int32_t>);
            break;
        case GDT_Float64:
            gma_proc_simple<double>(b, gma_rand<double>);
            break;
        }
        break;
    }
    case gma_method_print: {
        switch (b->GetRasterDataType()) {
        case GDT_Int16:
            gma_proc_simple<int16_t>(b, gma_print_int16_t);
            break;
        case GDT_Int32:
            gma_proc_simple<int32_t>(b, gma_print_int32_t);
            break;
        case GDT_Float64:
            gma_proc_simple<double>(b, gma_print_double);
            break;
        }
        break;
    }
    }
}
