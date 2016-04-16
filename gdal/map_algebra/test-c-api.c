#include <stdio.h>
#include <math.h>
#include "gdal_map_algebra.h"

main() {
    GDALAllRegister();
    srand(time(NULL));
    GDALDriverH d = GDALGetDriverByName("MEM");
    int w_band = 16, h_band = 10;
    GDALDatasetH ds = GDALCreate(d, "", w_band, h_band, 1, GDT_Float64, NULL);
    GDALRasterBandH b = GDALGetRasterBand(ds, 1);
    gma_number_h x = (gma_number_h)gma_new_object(b, gma_number);
}
