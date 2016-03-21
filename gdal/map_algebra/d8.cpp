#include "gdal_map_algebra.h"

int main() {
    GDALAllRegister();
    GDALDataset *ds2 = (GDALDataset*)GDALOpen("L3423G010.tiff", GA_ReadOnly);
    GDALRasterBand *b2 = ds2->GetRasterBand(1);
    int w = ds2->GetRasterXSize();
    int h = ds2->GetRasterYSize();
    printf("%i %i\n", w, h);

    GDALDriver *d = GetGDALDriverManager()->GetDriverByName("GTiff");
    GDALDataset *ds1 = d->Create("fd.tiff", w, h, 1, GDT_Byte, NULL);
    GDALRasterBand *b1 = ds1->GetRasterBand(1);

    int w_block, h_block;
    b1->GetBlockSize(&w_block, &h_block);
    printf("%i %i\n", w_block, h_block);
    b2->GetBlockSize(&w_block, &h_block);
    printf("%i %i\n", w_block, h_block);

    gma_two_bands(b1, gma_method_D8, b2);

}
