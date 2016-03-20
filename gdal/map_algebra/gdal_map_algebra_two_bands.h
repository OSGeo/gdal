typedef int (*gma_two_bands_callback)(void*, void*, int, int);

#define gma_add_band(type) int gma_add_band_##type(void* block, void* block2, int w, int h) { \
        for (int y = 0; y < h; y++) {                                   \
            for (int x = 0; x < w; x++) {                               \
                gma_typecast(type, block)[x+y*h] +=                     \
                    gma_typecast(type, block2)[x+y*h];                  \
            }                                                           \
        }                                                               \
        return 2;                                                       \
    }

gma_add_band(int16_t)
gma_add_band(int32_t)
gma_add_band(double)

template<typename data_t>
void gma_two_bands_proc(GDALRasterBand *b, gma_two_bands_callback cb, GDALRasterBand *b2) {
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
            e = b2->ReadBlock(x_block, y_block, block2);
            int ret = cb(block, blocks2, w, h);
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

// this one needs up to 9 blocks from band 2
int gma_D8(void* block, void* block2, int w, int h) {
    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            // set to block the direction to lowest neighbor or 0 for flat or -1 for pit
            gma_typecast(int, block)[x+y*h] +=
                gma_typecast(double, block2)[x+y*h];
        }
    }
    return 2;
}

template<typename data_t>
void gma_two_bands_proc_focal(GDALRasterBand *b, gma_two_bands_callback cb, GDALRasterBand *b2) {
    int w_band = b->GetXSize(), h_band = b->GetYSize();
    int w_block, h_block;
    b->GetBlockSize(&w_block, &h_block);
    int w_blocks = (w_band + w_block - 1) / w_block;
    int h_blocks = (h_band + h_block - 1) / h_block;
    data_t blocks2[w_block*h_block*9];
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
            data_t block2[w_block*h_block];
            CPLErr e = b->ReadBlock(x_block, y_block, block);

            // update the 3 x 3 block thing, 0 is up left, 2 is up right
            // usually copy 1 -> 0, 2 -> 1, 4 -> 3, 5 -> 4, 7 -> 6, 8 -> 7
            // usually read 2, 5, 8
            e = b2->ReadBlock(x_block, y_block, blocks2);

            int ret = cb(block, block2, w, h);
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

void gma_two_bands(GDALRasterBand *b, gma_two_bands_method_t method, GDALRasterBand *b2) {
    switch (method) {
    case gma_method_add_band: {
        // assuming b2 is same size and type as b
        switch (b->GetRasterDataType()) {
        case GDT_Int16: {
            gma_two_bands_proc<int16_t>(b, gma_add_band_int16_t, b2);
            break;
        }
        case GDT_Int32: {
            gma_two_bands_proc<int32_t>(b, gma_add_band_int32_t, b2);
            break;
        }
        case GDT_Float64: {
            gma_two_bands_proc<double>(b, gma_add_band_double, b2);
            break;
        }
        }
        break;
    }
    case gma_method_D8: {
        // compute flow directions from DEM
        // assuming b2 is DEM with doubles and b is bytes
        switch (b->GetRasterDataType()) {
        case GDT_Byte: {
            gma_two_bands_proc<char>(b, gma_D8, b2);
            break;
        }
        }
        break;
    }
    }
}
