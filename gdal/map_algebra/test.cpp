#include <stdio.h>
#include <math.h>
#include "gdal_map_algebra.hpp"

int callback(gma_cell_t *cell, gma_object_t *loc) {
    // set cell value = distance from loc
    double x = cell->x() - ((gma_cell_t*)loc)->x();
    double y = cell->y() - ((gma_cell_t*)loc)->y();
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
    
    gma_band_t *bx = gma_new_band(b);
    gma_cell_callback_t *cb = bx->new_cell_callback();
    cb->set_callback(callback);
    gma_cell_t *loc = bx->new_cell();
    loc->set_x(5);
    loc->set_y(5);
    cb->set_user_data(loc);
    bx->cell_callback(cb);
    bx->print();
    printf("\n");

    gma_classifier_t *c = bx->new_classifier();
    // how to define classifier?
    // we have a float raster => currently only bin => value is ok
    for (int i = 0; i < 5; ++i) {
        gma_number_t *x = bx->new_number();
        x->set_value((i+1)*3.0);
        gma_number_t *y = bx->new_number();
        y->set_value((i+1)*3.0);
        c->add_class(x, y);
    }
    {
        gma_number_t *x = bx->new_number();
        x->set_inf(1);
        gma_number_t *y = bx->new_number();
        y->set_value(6*3.0);
        c->add_class(x, y);
    }
    bx->classify(c);
    bx->print();
    printf("\n");

    GDALRasterBand *b2 = d->Create("", w_band, h_band, 1, GDT_Byte, NULL)->GetRasterBand(1);
    gma_band_t *by = gma_new_band(b2);
    gma_logical_operation_t *op = bx->new_logical_operation();
    op->set_operation(gma_lt);
    op->set_value(11);
    by->assign(bx, op);

    by->print();
    printf("\n");

    // another type of classifier int => int
    delete c;
    c = by->new_classifier();
    gma_number_t *x = by->new_number(); x->set_value(3);
    gma_number_t *y = by->new_number(); y->set_value(4);
    c->add_value(x, y);
    by->classify(c);
    by->print();
    printf("\n");

    gma_hash_t *z = by->zonal_neighbors();

    std::vector<gma_number_t*> zk = z->keys_sorted();
    for (int i = 0; i < zk.size(); ++i) {
        gma_number_t *k = zk[i];
        int k1 = k->value_as_int();
        gma_hash_t *zn = (gma_hash_t*)z->get(k);
        std::vector<gma_number_t*> znk = zn->keys_sorted();
        for (int j = 0; j < znk.size(); ++j) {
            int k2 = znk[j]->value_as_int();
            printf("%i => %i\n", k1, k2);
        }
    }
    printf("\n");
    
    b2->SetNoDataValue(9);
    by->update();
    by->print();
    printf("\n");

    std::vector<gma_cell_t*> cells = by->cells();
    for (int i = 0; i < cells.size(); ++i) {
        gma_cell_t *cell = cells[i];
        printf("%i %i %i\n", cell->x(), cell->y(), cell->value_as_int());
    }

}
