#include "gdal_map_algebra.h"

int main(int argc, char *argv[]) {
    GDALAllRegister();
    GDALDataset *ds = (GDALDataset*)GDALOpen(argv[1], GA_ReadOnly);
    GDALRasterBand *b = ds->GetRasterBand(1);
    gma_hash<gma_int> *hm = gma_compute_value_object<gma_hash<gma_int> >(b, gma_method_histogram);
    int n = hm->size();
    int32_t *keys = hm->keys_sorted(n);
    for (int i = 0; i < n; i++)
        printf("%i => %i\n", keys[i], hm->get(keys[i])->value());
    CPLFree(keys);
    delete hm;
}
