#include "gdal_map_algebra.h"

int main() {
    GDALAllRegister();
    srand(time(NULL));
    GDALDriver *d = GetGDALDriverManager()->GetDriverByName("MEM");
    int w_band = 16, h_band = 10;
    GDALDataset *ds = d->Create("", w_band, h_band, 2, GDT_Int32, NULL);
    GDALRasterBand *b = ds->GetRasterBand(1);
    gma_simple(b, gma_method_rand);
    gma_simple(b, gma_method_print);
    printf("\n");

    int32_t i = 5;
    gma_with_arg(b, gma_method_add, i);
    gma_simple(b, gma_method_print);
    printf("\n");

    GDALRasterBand *b2 = ds->GetRasterBand(2);
    gma_simple(b2, gma_method_rand);
    gma_simple(b2, gma_method_print);
    printf("\n");

    gma_two_bands(b, gma_method_add_band, b2);
    gma_simple(b, gma_method_print);
    printf("\n");

    ds = d->Create("", w_band, h_band, 1, GDT_Float64, NULL);
    b = ds->GetRasterBand(1);
    gma_simple(b, gma_method_rand);
    gma_simple(b, gma_method_print);
    printf("\n");

    double x = 1.1;
    gma_with_arg(b, gma_method_add, x);
    gma_simple(b, gma_method_print);
}
