#include "gdal_map_algebra.h"

int main() {
    GDALAllRegister();

    //GDALDataset *ds2 = (GDALDataset*)GDALOpen("L3423G010.tiff", GA_ReadOnly);
    GDALDataset *ds2 = (GDALDataset*)GDALOpen("pitless_dem.tiff", GA_ReadOnly);
    //GDALDataset *ds2 = (GDALDataset*)GDALOpen("fd.tiff", GA_ReadOnly);
    //GDALDataset *ds2 = (GDALDataset*)GDALOpen("flatless_fd.tiff", GA_ReadOnly);
    //GDALDataset *ds2 = (GDALDataset*)GDALOpen("depressions.tiff", GA_ReadOnly);
    //GDALDataset *ds2 = (GDALDataset*)GDALOpen("dep-rims.tiff", GA_ReadOnly);
    GDALRasterBand *b2 = ds2->GetRasterBand(1);
    GDALDriver *d = ds2->GetDriver();

    GDALDataType t = b2->GetRasterDataType();
    int w = b2->GetXSize();
    int h = b2->GetYSize();
    printf("data type = %i\n", t);

    if (0) {
        GDALDataset *ds1 = d->Create("fd.tiff", w, h, 1, GDT_Byte, NULL);
        GDALRasterBand *b1 = ds1->GetRasterBand(1);
        gma_two_bands(b1, gma_method_D8, b2);
    }

    if (0) {
        GDALDataset *ds1 = (GDALDataset*)GDALOpen("flatless_fd.tiff", GA_Update);
        GDALRasterBand *b1 = ds1->GetRasterBand(1);
        gma_two_bands(b1, gma_method_route_flats, b2);
    }

    if (0) {
        GDALDataset *ds1 = d->Create("depressions.tiff", w, h, 1, GDT_UInt32, NULL);
        GDALRasterBand *b1 = ds1->GetRasterBand(1);
        gma_two_bands(b1, gma_method_depressions, b2);
    }

    if (0) {
        GDALDataset *ds1 = d->Create("dep-rims.tiff", w, h, 1, GDT_UInt32, NULL);
        GDALRasterBand *b1 = ds1->GetRasterBand(1);
        gma_two_bands(b1, gma_method_rim_by8, b2);
    }

    if (0) {
        gma_hash<gma_int> *hm = (gma_hash<gma_int>*)gma_compute_value(b2, gma_method_histogram);
        int n = hm->size();
        int32_t *keys = hm->keys_sorted(n);
        for (int i = 0; i < n; i++)
            printf("%i => %i\n", keys[i], hm->get(keys[i])->value());
        CPLFree(keys);
        delete hm;
    }

    if (1) {
        GDALDataset *ds1 = (GDALDataset*)GDALOpen("dep-rims.tiff", GA_ReadOnly);
        GDALRasterBand *b1 = ds1->GetRasterBand(1);
        gma_hash<gma_int> *z = (gma_hash<gma_int>*)gma_two_bands(b1, gma_method_zonal_min, b2);
        if (z) {
            int n = z->size();
            int32_t *keys = z->keys_sorted(n);
            for (int i = 0; i < n; i++)
                printf("%i => %i\n", keys[i], z->get(keys[i])->value());
            CPLFree(keys);

            gma_two_bands(b2, gma_method_set_zonal_min, b1, z);

            delete z;
        }
    }

    if (0) {
        GDALDataset *ds1 = d->Create("ua.tiff", w, h, 1, GDT_UInt32, NULL);
        GDALRasterBand *b1 = ds1->GetRasterBand(1);
        gma_two_bands(b1, gma_method_upstream_area, b2);
    }

}
