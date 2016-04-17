#include "gdal_map_algebra.hpp"

int main() {

    GDALAllRegister();
    srand(time(NULL));
    GDALDriver *d = GetGDALDriverManager()->GetDriverByName("MEM");
    int w_band = 16, h_band = 10;
    GDALDataset *ds = d->Create("", w_band, h_band, 2, GDT_Int32, NULL);
    GDALRasterBand *b = ds->GetRasterBand(1);

    gma_band_t *bx = (gma_band_t*)gma_new_object(b, gma_band);
    bx->rand();
    bx->modulus(20);
    bx->print();
    printf("\n");

    bx->add(5);
    bx->print();
    printf("\n");
    

    GDALRasterBand *b2 = ds->GetRasterBand(2);

    gma_band_t *by = (gma_band_t*)gma_new_object(b2, gma_band);
    by->rand();
    by->modulus(10);
    by->print();
    printf("\n");

    bx->add(by);
    bx->print();
    printf("\n");

    gma_histogram_t *hm = bx->histogram();

    print_histogram(hm);

    gma_pair_t *r = bx->get_range();
    gma_number_t *min = (gma_number_t*)r->first();
    gma_number_t *max = (gma_number_t*)r->second();
    printf("[%i..%i]\n", min->value_as_int(), max->value_as_int());

    gma_pair_t *arg = (gma_pair_t *)gma_new_object(b, gma_pair);

    gma_number_t *tmp = (gma_number_t *)gma_new_object(b, gma_integer);
    tmp->set_value(5);
    arg->set_first(tmp);
    arg->set_second(r);

    hm = (gma_histogram_t*)gma_compute_value(b, gma_method_histogram, arg);
    print_histogram(hm);
}
