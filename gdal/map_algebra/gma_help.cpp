#include "gdal_map_algebra_private.h"

void print_histogram(gma_histogram_t *hm) {
    if (hm == NULL) {
        printf("No histogram.\n");
        return;
    }
    for (unsigned int i = 0; i < hm->size(); i++) {
        gma_pair_t *kv = (gma_pair_t *)hm->at(i);
        // kv is an interval=>number or number=>number
        if (kv->first()->get_class() == gma_pair) {
            gma_pair_t *key = (gma_pair_t*)kv->first();
            gma_number_t *min = (gma_number_t*)key->first();
            gma_number_t *max = (gma_number_t*)key->second();
            gma_number_t *val = (gma_number_t*)kv->second();
            if (min->is_integer()) {
                printf("(%i..%i] => %i\n", min->value_as_int(), max->value_as_int(), val->value_as_int());
            } else {
                printf("(%f..%f] => %i\n", min->value_as_double(), max->value_as_double(), val->value_as_int());
            }
        } else {
            gma_number_t *key = (gma_number_t*)kv->first();
            gma_number_t *val = (gma_number_t*)kv->second();
            printf("%i => %i\n", key->value_as_int(), val->value_as_int());
        }
    }
}
