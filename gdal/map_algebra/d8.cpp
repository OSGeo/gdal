#include "gdal_map_algebra.hpp"

int set_border_cells(gma_cell_t *cell, gma_object_t *band_size) {
    int w = ((gma_cell_t *)band_size)->x();
    int h = ((gma_cell_t *)band_size)->y();
    if (cell->x() == 0 || cell->x() == w-1 || cell->y() == 0 || cell->y() == h-1)
        cell->set_value(1);
    else 
        cell->set_value(0);
    return 2;
}

int progress(double, const char *message, void *) {
    printf("%s\n", message);
    return TRUE;
}

int main() {
    GDALAllRegister();

    if (1) {

        // starting point is a DEM
        gma_band_t *d = gma_new_band(((GDALDataset*)GDALOpen("data/L3423G010.tiff", GA_ReadOnly))->GetRasterBand(1));

        // first we create a depressionless dem
        gma_band_t *dem = d->new_band("data/dem.tiff", d->datatype());

        dem->set_progress_fct(progress, NULL);
        dem->fill_depressions(d); // would be nicer as dem = d->fill_depressions() or dem->depressionless_dem_from(d);
        exit(1);

        // now flow directions are easy
        gma_band_t *fd = dem->new_band("fd.tiff", GDT_Byte);
        fd->D8(dem);
        fd->route_flats(dem);

        // upstream area from the fd
        gma_band_t *ua = dem->new_band("ua.tiff", GDT_UInt32);
        ua->upstream_area(fd);

    }

    if (0) {

        gma_band_t *fd = gma_new_band(((GDALDataset*)GDALOpen("fd.tiff", GA_ReadOnly))->GetRasterBand(1));
        gma_band_t *ua = gma_new_band("ua.tiff");

        // catchments from the fd
        gma_band_t *c = fd->new_band("catchments.tiff", GDT_UInt32);

        // c = 0 except 1 on the borders
        gma_cell_callback_t *cb = c->new_cell_callback();
        cb->set_callback(set_border_cells);
        gma_cell_t *wh = c->new_cell();
        wh->set_x(c->w());
        wh->set_y(c->h());
        cb->set_user_data(wh);
        c->cell_callback(cb);

        // c *= ua
        c->multiply(ua);

        //c -= c if c < 10000;
        gma_logical_operation_t *op = c->new_logical_operation();
        op->set_operation(gma_lt);
        op->set_value(10000);
        c->subtract(c, op);

        // outlet cells
        std::vector<gma_cell_t*> outlets = c->cells();

        // c = 0
        c->assign(0);

        for (int i = 0; i < outlets.size(); ++i) {
            gma_cell_t *cell = outlets[i];
            printf("%i %i %i %i\n", cell->x(), cell->y(), cell->value_as_int(), i+1);
            cell->set_value(i+1);
            c->catchment(fd, cell);
            break;
        }

    }

    if (0) {
        gma_band_t *ua = gma_new_band("ua.tiff");
        gma_band_t *c = gma_new_band("catchments.tiff");
        gma_band_t *ua2 = c->new_band("ua2.tiff", GDT_Float32);
        ua2->add(ua);
        ua2->multiply(c);
        ua2->log();
    }

}
