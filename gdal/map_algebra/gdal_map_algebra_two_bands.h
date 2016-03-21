typedef int (*gma_two_bands_callback)(void*, void*, int, int);

#define gma_add_band(type) int gma_add_band_##type(void* block1, void* block2, int w, int h) { \
        for (int y = 0; y < h; y++) {                                   \
            for (int x = 0; x < w; x++) {                               \
                gma_typecast(type, block1)[x+y*w] +=                    \
                    gma_typecast(type, block2)[x+y*w];                  \
            }                                                           \
        }                                                               \
        return 2;                                                       \
    }

gma_add_band(int16_t)
gma_add_band(int32_t)
gma_add_band(double)

template<typename data_t>
void gma_two_bands_proc(GDALRasterBand *b1, gma_two_bands_callback cb, GDALRasterBand *b2) {
    int w_band = b1->GetXSize(), h_band = b1->GetYSize();
    int w_block, h_block;
    b1->GetBlockSize(&w_block, &h_block);
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
            data_t block1[w_block*h_block];
            data_t block2[w_block*h_block];
            CPLErr e = b1->ReadBlock(x_block, y_block, block1);
            e = b2->ReadBlock(x_block, y_block, block2);
            int ret = cb(block1, block2, w, h);
            switch (ret) {
            case 0: return;
            case 1: break;
            case 2: {
                e = b1->WriteBlock(x_block, y_block, block1);
            }
            }
        }
    }
}

// these need up to 9 blocks from band 2
// 0 1 2
// 3 4 5
// 6 7 8
// first int is the size of one block
// second and third are pointers to the sizes of the 3 x 3 matrix blocks
// last int is validity of blocks in blocks2 denoted by bits
// 0 = 0x1, 1 = 0x2, 2 = 0x4, 3 = 0x8, 4 = 0x10, 5 = 0x20, 6 = 0x40, 7 = 0x80, 8 = 0x100
typedef int (*gma_two_bands_callback2)(void*, void*, int, int*, int*, int);

template<typename data_t>
data_t gma_neighbor(data_t *data, int n, int x, int y, int *w, int *h, int v, int *ok) {
    *ok = 0;
    if (y < 0) {
        if (x < 0) {
            if (v & 0x1) {
                *ok = 1;
                return gma_typecast(data_t, data)[0*n + (w[0]-1)+(h[0]-1)*w[0]];
            }
        } else if (x >= w[4]) {
            if (v & 0x4) {
                *ok = 1;
                return gma_typecast(data_t, data)[2*n + 0+(h[2]-1)*w[2]];
            }
        } else if (v & 0x2) {
            *ok = 1;
            return gma_typecast(data_t, data)[1*n + x+(h[1]-1)*w[1]];
        }
    } else if (y >= h[4]) {
        if (x < 0) {
            if (v & 0x40) {
                *ok = 1;
                return gma_typecast(data_t, data)[6*n + (w[6]-1)+(h[6]-1)*w[6]];
            }
        } else if (x >= w[4]) {
            if (v & 0x100) {
                *ok = 1;
                return gma_typecast(data_t, data)[8*n + 0+(h[8]-1)*w[8]];
            }
        } else if (v & 0x80) {
            *ok = 1;
            return gma_typecast(data_t, data)[7*n + x+(h[7]-1)*w[7]];
        }
    } else {
        if (x < 0) {
            if (v & 0x8) {
                *ok = 1;
                return gma_typecast(data_t, data)[3*n + (w[3]-1)+(h[3]-1)*w[3]];
            }
        } else if (x >= w[4]) {
            if (v & 0x20) {
                *ok = 1;
                return gma_typecast(data_t, data)[5*n + 0+(h[5]-1)*w[5]];
            }
        } else {
            *ok = 1;
            return gma_typecast(data_t, data)[4*n + x+y*w[4]];
        }
    }
}

template<typename data1_t,typename data2_t>
void gma_8neighbors(data2_t *data, int n, int x, int y, int *w, int *h, int v, 
                    data2_t *val, int *ok, 
                    data1_t *border) 
{
    int d = 0;
    for (int yn = y-1; yn < y+2; yn++) {
        for (int xn = x-1; xn < x+2; xn++) {
            val[d] = gma_neighbor((data2_t*)data, n, xn, yn, w, h, v, &ok[d]);
            d++;
        }
    }
    *border = 0;
    if (y == 0 && (!(v & 0x2)))
        *border = 1;
    else if (x == w[4]-1 && (!(v & 0x20)))
        *border = 5;
    else if (y == h[4]-1 && (!(v & 0x80)))
        *border = 7;
    else if (x == 0 && (!(v & 0x8)))
        *border = 3;
}

template<typename data1_t, typename data2_t>
int gma_D8(void* block1, void* blocks2, int n, int *w, int *h, int v) {
    for (int y = 0; y < h[4]; y++) {
        for (int x = 0; x < w[4]; x++) {

            // elevation at this cell
            data2_t h4 = gma_typecast(data2_t, blocks2)[4*n + x+y*w[4]]; 

            // elevations of neighboring cells
            data2_t neighbors[9];
            int ok[9];
            data1_t border;
            gma_8neighbors<data1_t,data2_t>((data2_t*)blocks2, n, x, y, w, h, v, neighbors, ok, &border);

            // the lowest neighboring cell
            int first = 1;
            data1_t dir;
            data2_t lowest;
            for (int d = 0; d < 10; d++) {
                if (ok[d] && ((first || neighbors[d] < lowest))) {
                    first = 0;
                    dir = d+1; // 1 to 9
                    lowest = neighbors[d];
                }
            }

            // the direction to lowest neighbor or 0 for flat or 10 for pit
            if (lowest >= h4 && border)
                dir = border;
            else if (lowest == h4)
                dir = 0;
            else if (lowest > h4)
                dir = 10;
            gma_typecast(data1_t, block1)[x+y*w[4]] = dir;
        }
    }
    return 2;
}

int block_data_size(int x_block, int w_block, int w_band) {
    if( (x_block+1) * w_block > w_band )
        return w_band - x_block * w_block;
    else
        return w_block;
}

template<typename data1_t, typename data2_t>
void gma_two_bands_proc_focal(GDALRasterBand *b1, gma_two_bands_callback2 cb, GDALRasterBand *b2) {
    int w_band = b1->GetXSize(), h_band = b1->GetYSize();
    int w_block1, h_block1;
    b1->GetBlockSize(&w_block, &h_block);
    int w_blocks = (w_band + w_block - 1) / w_block;
    int h_blocks = (h_band + h_block - 1) / h_block;
    int n_block = w_block*h_block;
    data2_t blocks2[n_block*9];
    int w[9], h[9];
    for (int y_block = 0; y_block < h_blocks; y_block++) {
        for (int x_block = 0; x_block < w_blocks; x_block++) {
            w[4] = block_data_size(x_block, w_block, w_band);
            h[4] = block_data_size(y_block, h_block, h_band);

            data1_t block1[n_block];
            CPLErr e = b1->ReadBlock(x_block, y_block, block1);

            // update the 3 x 3 block thing, 0 is up left, 2 is up right
            // usually copy 1 -> 0, 2 -> 1, 4 -> 3, 5 -> 4, 7 -> 6, 8 -> 7
            // usually read 2, 5, 8

            // validity of blocks in blocks2:
            int v = 0;

            if (x_block == 0) {
                if (y_block > 0) {
                    w[1] = block_data_size(x_block, w_block, w_band);
                    h[1] = block_data_size(y_block-1, h_block, h_band);
                    e = b2->ReadBlock(x_block, y_block-1, &(blocks2[n_block*1]));
                    v |= 0x2 | 0x10; // 1 4
                } else
                    v |= 0x10; // 4
                e = b2->ReadBlock(x_block, y_block, &(blocks2[n_block*4]));
                if (y_block < h_blocks-1) {
                    w[7] = block_data_size(x_block, w_block, w_band);
                    h[7] = block_data_size(y_block+1, h_block, h_band);
                    e = b2->ReadBlock(x_block, y_block+1, &(blocks2[n_block*7]));
                    v |= 0x80; // 7
                }   
            } else {
                w[3] = block_data_size(x_block-1, w_block, w_band);
                h[3] = block_data_size(y_block, h_block, h_band);
                if (y_block > 0) {
                    w[0] = block_data_size(x_block-1, w_block, w_band);
                    h[0] = block_data_size(y_block-1, h_block, h_band);
                    v |= 0x1 | 0x8; // 0 3
                } else
                    v |= 0x8; // 3
                if (y_block < h_blocks-1) {
                    w[6] = block_data_size(x_block-1, w_block, w_band);
                    h[6] = block_data_size(y_block+1, h_block, h_band);
                    v |= 0x40; // 6
                }
                if (x_block < w_blocks-1) {
                    w[5] = block_data_size(x_block+1, w_block, w_band);
                    h[5] = block_data_size(y_block, h_block, h_band);
                    if (y_block > 0) {
                        w[2] = block_data_size(x_block+1, w_block, w_band);
                        h[2] = block_data_size(y_block-1, h_block, h_band);
                        v |= 0x4 | 0x20; // 2 5
                    } else
                        v |= 0x20; // 5
                    if (y_block < h_blocks-1) {
                        w[8] = block_data_size(x_block+1, w_block, w_band);
                        h[8] = block_data_size(y_block+1, h_block, h_band);
                        v |= 0x100; // 8
                    }
                }
                memcpy(&(blocks2[n_block*0]), &(blocks2[n_block*1]), n_block*sizeof(data2_t));
                memcpy(&(blocks2[n_block*3]), &(blocks2[n_block*4]), n_block*sizeof(data2_t));
                memcpy(&(blocks2[n_block*6]), &(blocks2[n_block*7]), n_block*sizeof(data2_t));
                memcpy(&(blocks2[n_block*1]), &(blocks2[n_block*2]), n_block*sizeof(data2_t));
                memcpy(&(blocks2[n_block*4]), &(blocks2[n_block*5]), n_block*sizeof(data2_t));
                memcpy(&(blocks2[n_block*7]), &(blocks2[n_block*8]), n_block*sizeof(data2_t));
            }
            if (x_block < w_blocks-1) {
                if (y_block > 0)
                    e = b2->ReadBlock(x_block+1, y_block-1, &(blocks2[n_block*2]));
                e = b2->ReadBlock(x_block+1, y_block, &(blocks2[n_block*5]));
                if (y_block < h_blocks-1)
                    e = b2->ReadBlock(x_block+1, y_block+1, &(blocks2[n_block*8]));
            }

            int ret = cb(block1, blocks2, n_block, w, h, v);
            switch (ret) {
            case 0: return;
            case 1: break;
            case 2: {
                e = b1->WriteBlock(x_block, y_block, block1);
            }
            }
        }
    }
}

void gma_two_bands(GDALRasterBand *b1, gma_two_bands_method_t method, GDALRasterBand *b2) {
    // b1 is changed, b2 is not
    // assuming b1 has same size as b2
/*
    int w_band = b1->GetXSize(), h_band = b1->GetYSize();

    check:
    int w_band == b2->GetXSize(), h_band == b2->GetYSize();
*/
    switch (method) {
    case gma_method_add_band: {
        // assuming b1 and b2 have same data type
        switch (b1->GetRasterDataType()) {
        case GDT_Int16: {
            gma_two_bands_proc<int16_t>(b1, gma_add_band_int16_t, b2);
            break;
        }
        case GDT_Int32: {
            gma_two_bands_proc<int32_t>(b1, gma_add_band_int32_t, b2);
            break;
        }
        case GDT_Float64: {
            gma_two_bands_proc<double>(b1, gma_add_band_double, b2);
            break;
        }
        }
        break;
    }
    case gma_method_D8: {
        // compute flow directions from DEM
        switch (b1->GetRasterDataType()) {
        case GDT_Byte: {
            switch (b2->GetRasterDataType()) {
            case GDT_UInt16:
                gma_two_bands_proc_focal<unsigned char,uint16_t>(b1, gma_D8<unsigned char,uint16_t>, b2);
                break;
            case GDT_Float64:
                gma_two_bands_proc_focal<char,double>(b1, gma_D8<char,double>, b2);
                break;
            }
            break;
        }
        }
        break;
    }
    }
}
