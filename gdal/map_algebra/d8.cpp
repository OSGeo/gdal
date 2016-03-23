#include "gdal_map_algebra.h"

int main() {
    GDALAllRegister();

    //GDALDataset *ds2 = (GDALDataset*)GDALOpen("L3423G010.tiff", GA_ReadOnly);
    //GDALDataset *ds2 = (GDALDataset*)GDALOpen("pitless_dem.tiff", GA_ReadOnly);
    //GDALDataset *ds2 = (GDALDataset*)GDALOpen("fd.tiff", GA_ReadOnly);
    GDALDataset *ds2 = (GDALDataset*)GDALOpen("flatless_fd.tiff", GA_ReadOnly);

    GDALRasterBand *b2 = ds2->GetRasterBand(1);
    int w = b2->GetXSize();
    int h = b2->GetYSize();

    GDALDriver *d = ds2->GetDriver();

    if (0) {
        GDALDataset *ds1 = d->Create("pitless_dem.tiff", w, h, 1, b2->GetRasterDataType(), NULL);
        char **files = ds1->GetFileList();
        for (int i = 0; files[i]; i++)
            printf("file %s\n", files[i]);
        CSLDestroy(files);
    }

    if (0) {
        GDALDataset *ds1 = d->Create("fd.tiff", w, h, 1, GDT_Byte, NULL);
        GDALRasterBand *b1 = ds1->GetRasterBand(1);
        gma_two_bands(b1, gma_method_D8, b2);
        ds1->FlushCache();
    }

    if (1) {
        int *hm = (int *)gma_compute_value(b2, gma_method_histogram);
        for (int i = 0; i < 11; i++)
            printf("%i => %i\n", i, hm[i]);
    }

    if (0) {
        GDALDataset *ds1 = d->Create("pitless_dem.tiff", w, h, 1, b2->GetRasterDataType(), NULL);
        GDALRasterBand *b1 = ds1->GetRasterBand(1);
        gma_two_bands(b1, gma_method_pit_removal, b2);
    }

    if (0) {
        GDALDataset *ds1 = d->Create("flatless_fd.tiff", w, h, 1, b2->GetRasterDataType(), NULL);
        GDALRasterBand *b1 = ds1->GetRasterBand(1);
        gma_two_bands(b1, gma_method_route_flats, b2);
    }

}
