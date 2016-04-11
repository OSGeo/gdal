#include <stdio.h>
#include <math.h>
#include "gdal_map_algebra.h"

int callback(gma_cell_t *cell) {
    // set cell value = distance from 0,0
    double x = cell->x();
    double y = cell->y();
    double d = sqrt(x*x+y*y);
    cell->set_value(d);
    return 2;
}

main() {
    GDALAllRegister();
    srand(time(NULL));
    GDALDriver *d = GetGDALDriverManager()->GetDriverByName("MEM");
    int w_band = 16, h_band = 10;
    GDALRasterBand *b = d->Create("", w_band, h_band, 1, GDT_Float64, NULL)->GetRasterBand(1);
    
    gma_cell_callback_t *x = (gma_cell_callback_t*)gma_new_object(b, gma_cell_callback);
    x->set_callback(callback);
    gma_with_arg(b, gma_method_cell_callback, x);

    gma_simple(b, gma_method_print);

    gma_classifier_t *c = (gma_classifier_t*)gma_new_object(b, gma_classifier);
    // how to define classifier?
    // we have a float raster => currently only bin => value is ok
    for (int i = 0; i < 5; i++) {
        gma_number_t *x = (gma_number_t*)gma_new_object(b, gma_number);
        x->set_value((i+1)*3.0);
        gma_number_t *y = (gma_number_t*)gma_new_object(b, gma_number);
        y->set_value((i+1)*3.0);
        c->add_class(x, y);
    }
    {
        gma_number_t *x = (gma_number_t*)gma_new_object(b, gma_number);
        x->set_inf(1);
        gma_number_t *y = (gma_number_t*)gma_new_object(b, gma_number);
        y->set_value(6*3.0);
        c->add_class(x, y);
    }
    gma_with_arg(b, gma_method_classify, c);

    printf("\n");
    gma_simple(b, gma_method_print);

    GDALRasterBand *b2 = d->Create("", w_band, h_band, 1, GDT_Byte, NULL)->GetRasterBand(1);
    gma_logical_operation_t *op = (gma_logical_operation_t*)gma_new_object(b, gma_logical_operation);
    op->set_operation(gma_lt);
    op->set_value(18);
    gma_two_bands(b2, gma_method_assign_band, b, op);

    printf("\n");
    gma_simple(b2, gma_method_print);

    gma_hash_t *z = (gma_hash_t *)gma_compute_value(b2, gma_method_zonal_neighbors);

    printf("\n");
    std::vector<gma_number_t*> *zk = z->keys_sorted();
    for (int i = 0; i < zk->size(); i++) {
        gma_number_t *k = zk->at(i);
        int k1 = k->value_as_int();
        gma_hash_t *zn = (gma_hash_t*)z->get(k);
        std::vector<gma_number_t*> *znk = zn->keys_sorted();
        for (int j = 0; j < znk->size(); j++) {
            int k2 = znk->at(j)->value_as_int();
            printf("%i => %i\n", k1, k2);
        }
    }

}
