#include "gdal_map_algebra.h"

int main(int argc, char *argv[]) {
    GDALAllRegister();
    GDALDataset *ds = (GDALDataset*)GDALOpen(argv[1], GA_ReadOnly);
    GDALRasterBand *b = ds->GetRasterBand(1);
    gma_histogram_base *hm;
    new_object(hm, b, gma_histogram, 0);
    gma_compute_value_object(b, gma_method_histogram, &hm);
    int n = hm->size();
    int32_t *keys = hm->keys_sorted(n);
    for (int i = 0; i < n; i++)
        printf("%i => %i\n", keys[i], hm->get(keys[i])->value_as_int());
    CPLFree(keys);
    delete hm;
}
