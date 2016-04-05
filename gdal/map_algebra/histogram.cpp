#include "gdal_map_algebra.h"

int main(int argc, char *argv[]) {
    GDALAllRegister();
    GDALDataset *ds = (GDALDataset*)GDALOpen(argv[1], GA_ReadOnly);
    GDALRasterBand *b = ds->GetRasterBand(1);

    // how many bins? for integer bands bins or counts of values? arg = NULL, n, or bins
    // gma_object_t *arg = gma_new_object(b, gma_class_t klass);
    gma_histogram_t *hm = (gma_histogram_t*)gma_compute_value(b, gma_method_histogram, NULL);

    // histogram is an array of pairs
    for (unsigned int i = 0; i < hm->size(); i++) {
        gma_pair_t *kv = (gma_pair_t *)hm->at(i);
        // kv is an interval=>number or number=>number, now the latter
        gma_number_t *key = (gma_number_t*)kv->first();
        gma_number_t *val = (gma_number_t*)kv->second();
        printf("%i => %i\n", key->value_as_int(), val->value_as_int());
    }

    delete hm;
}
