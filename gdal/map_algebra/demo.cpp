#include "gdal_map_algebra.hpp"

int main() {

    GDALAllRegister();
    srand(time(NULL));
    GDALDriver *d = GetGDALDriverManager()->GetDriverByName("MEM");
    int w_band = 16, h_band = 10;
    GDALDataset *ds = d->Create("", w_band, h_band, 2, GDT_Int32, NULL);
    GDALRasterBand *b = ds->GetRasterBand(1);
    gma_simple(b, gma_method_rand);
    gma_number_t *x = (gma_number_t*)gma_new_object(b, gma_number);
    x->set_value(20);
    gma_with_arg(b, gma_method_modulus, x);
    gma_simple(b, gma_method_print);
    printf("\n");

    x->set_value(5);
    gma_with_arg(b, gma_method_add, x);
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

    x = (gma_number_t*)gma_new_object(b, gma_number);
    x->set_value(1.1);
    gma_with_arg(b, gma_method_add, x);
    gma_simple(b, gma_method_print);

    gma_histogram_t *hm = (gma_histogram_t*)gma_compute_value(b, gma_method_histogram, NULL);
    print_histogram(hm);
    gma_pair_t *arg = (gma_pair_t *)gma_new_object(b, gma_pair);
    {
        gma_number_t *tmp = (gma_number_t *)gma_new_object(b, gma_integer);
        tmp->set_value(5);
        arg->set_first(tmp);
        arg->set_second(gma_compute_value(b, gma_method_get_range));
    }
    hm = (gma_histogram_t*)gma_compute_value(b, gma_method_histogram, arg);
    print_histogram(hm);
}
