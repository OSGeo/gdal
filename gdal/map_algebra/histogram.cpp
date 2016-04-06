#include "gdal_map_algebra.h"

int main(int argc, char *argv[]) {
    GDALAllRegister();
    GDALDataset *ds = (GDALDataset*)GDALOpen(argv[1], GA_ReadOnly);
    GDALRasterBand *b = ds->GetRasterBand(1);

    // histogram of all values, should work only for integer bands
    gma_histogram_t *hm = (gma_histogram_t*)gma_compute_value(b, gma_method_histogram, NULL);

    // histogram is an array of pairs
    printf("count of values:\n");
    for (unsigned int i = 0; i < hm->size(); i++) {
        gma_pair_t *kv = (gma_pair_t *)hm->at(i);
        // kv is an interval=>number or number=>number, now the latter
        gma_number_t *key = (gma_number_t*)kv->first();
        gma_number_t *val = (gma_number_t*)kv->second();
        printf("%i => %i\n", key->value_as_int(), val->value_as_int());
    }
    printf("\n");

    delete hm;

    // histogram in 3 bins between min and max
    gma_pair_t *arg2 = (gma_pair_t *)gma_new_object(b, gma_pair);
    {
        gma_number_t *tmp = (gma_number_t *)gma_new_object(b, gma_integer);
        tmp->set_value(3);
        arg2->set_first(tmp);
        arg2->set_second(gma_compute_value(b, gma_method_get_range));
    }
    hm = (gma_histogram_t*)gma_compute_value(b, gma_method_histogram, arg2);

    // histogram is an array of pairs
    printf("3 bins between min and max:\n");
    for (unsigned int i = 0; i < hm->size(); i++) {
        gma_pair_t *kv = (gma_pair_t *)hm->at(i);
        // kv is an interval=>number or number=>number, now the former
        gma_pair_t *key = (gma_pair_t*)kv->first();
        gma_number_t *min = (gma_number_t*)key->first();
        gma_number_t *max = (gma_number_t*)key->second();
        gma_number_t *val = (gma_number_t*)kv->second();
        printf("(%i..%i] => %i\n", min->value_as_int(), max->value_as_int(), val->value_as_int());
    }
    printf("\n");

    // histogram in 3 bins between specified min and max (actual min,max is always -inf,+inf)
    arg2 = (gma_pair_t *)gma_new_object(b, gma_pair);
    {
        gma_number_t *tmp = (gma_number_t *)gma_new_object(b, gma_integer);
        tmp->set_value(3);
        arg2->set_first(tmp);
        gma_pair_t *tmp2 = (gma_pair_t *)gma_new_object(b, gma_range);
        ((gma_number_t*)(tmp2->first()))->set_value(2);
        ((gma_number_t*)(tmp2->second()))->set_value(5);
        arg2->set_second(tmp2);
    }
    hm = (gma_histogram_t*)gma_compute_value(b, gma_method_histogram, arg2);

    // histogram is an array of pairs
    printf("3 bins between 2 and 5:\n");
    for (unsigned int i = 0; i < hm->size(); i++) {
        gma_pair_t *kv = (gma_pair_t *)hm->at(i);
        // kv is an interval=>number or number=>number, now the former
        gma_pair_t *key = (gma_pair_t*)kv->first();
        gma_number_t *min = (gma_number_t*)key->first();
        gma_number_t *max = (gma_number_t*)key->second();
        gma_number_t *val = (gma_number_t*)kv->second();
        printf("(%i..%i] => %i\n", min->value_as_int(), max->value_as_int(), val->value_as_int());
    }
    printf("\n");

    // histogram in hand-made bins ..3, 4..5, 6..
    gma_bins_t *arg3 = (gma_bins_t *)gma_new_object(b, gma_bins);
    arg3->push(3);
    arg3->push(5);
    hm = (gma_histogram_t*)gma_compute_value(b, gma_method_histogram, arg3);

    // histogram is an array of pairs
    printf("hand-made bins ..3, 4..5, 6..:\n");
    for (unsigned int i = 0; i < hm->size(); i++) {
        gma_pair_t *kv = (gma_pair_t *)hm->at(i);
        // kv is an interval=>number or number=>number, now the former
        gma_pair_t *key = (gma_pair_t*)kv->first();
        gma_number_t *min = (gma_number_t*)key->first();
        gma_number_t *max = (gma_number_t*)key->second();
        gma_number_t *val = (gma_number_t*)kv->second();
        printf("(%i..%i] => %i\n", min->value_as_int(), max->value_as_int(), val->value_as_int());
    }
}
