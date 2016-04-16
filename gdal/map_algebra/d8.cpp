#include "gdal_map_algebra.h"

int set_border_cells(gma_cell_t *cell, gma_object_t *band_size) {
    int w = ((gma_cell_t *)band_size)->x();
    int h = ((gma_cell_t *)band_size)->y();
    if (cell->x() == 0 || cell->x() == w-1 || cell->y() == 0 || cell->y() == h-1)
        cell->set_value(1);
    else 
        cell->set_value(0);
    return 2;
}

int main() {
    GDALAllRegister();

    if (0) {

        GDALRasterBand *dem = ((GDALDataset*)GDALOpen("L3423G010.tiff", GA_ReadOnly))->GetRasterBand(1);
        GDALDriver *driver = dem->GetDataset()->GetDriver();
        int w = dem->GetXSize();
        int h = dem->GetYSize();

        // create a depressionless dem
        GDALRasterBand *tmp = driver->Create("dem.tiff", w, h, 1, GDT_UInt16, NULL)->GetRasterBand(1);
        gma_two_bands(tmp, gma_method_fill_depressions, dem);
        tmp->FlushCache();
        dem = tmp;

        // fd from dem
        GDALRasterBand *fd = driver->Create("fd.tiff", w, h, 1, GDT_Byte, NULL)->GetRasterBand(1);
        gma_two_bands(fd, gma_method_D8, dem);
        gma_two_bands(fd, gma_method_route_flats, dem); // drain drainable flat areas
        fd->FlushCache();

        // upstream area from the fd
        GDALRasterBand *ua = driver->Create("ua.tiff", w, h, 1, GDT_UInt32, NULL)->GetRasterBand(1);
        gma_two_bands(ua, gma_method_upstream_area, fd);

    }

    if (1) {

        GDALRasterBand *ua = ((GDALDataset*)GDALOpen("ua.tiff", GA_ReadOnly))->GetRasterBand(1);
        GDALRasterBand *fd = ((GDALDataset*)GDALOpen("fd.tiff", GA_ReadOnly))->GetRasterBand(1);
        GDALDriver *driver = ua->GetDataset()->GetDriver();
        int w = ua->GetXSize();
        int h = ua->GetYSize();

        // catchments from the fd
        GDALRasterBand *c = driver->Create("catchments.tiff", w, h, 1, GDT_UInt32, NULL)->GetRasterBand(1);

        // c = 0 except 1 on the borders
        gma_cell_callback_t *cb = (gma_cell_callback_t*)gma_new_object(c, gma_cell_callback);
        cb->set_callback(set_border_cells);
        gma_cell_t *wh = (gma_cell_t*)gma_new_object(c, gma_cell);
        wh->x() = w;
        wh->y() = h;
        cb->set_user_data(wh);
        gma_with_arg(c, gma_method_cell_callback, cb);

        // c *= ua
        gma_two_bands(c, gma_method_multiply_by_band, ua);

        //c *= c > 10000;
        gma_logical_operation_t *op = (gma_logical_operation_t *)gma_new_object(c, gma_logical_operation);
        op->set_operation(gma_gt);
        op->set_value(10000);
        gma_two_bands(c, gma_method_multiply_by_band, c, op);

        // outlet cells
        std::vector<gma_cell_t*> *outlets = (std::vector<gma_cell_t*>*)gma_compute_value(c, gma_method_get_cells);

        // c = 0
        gma_number_t *tmp = (gma_number_t*)gma_new_object(c, gma_number);
        tmp->set_value(0);
        gma_with_arg(c, gma_method_assign, tmp);

        for (int i = 0; i < outlets->size(); i++) {
            gma_cell_t *cell = outlets->at(i);
            printf("%i %i %i %i\n", cell->x(), cell->y(), cell->value_as_int(), i);
            cell->set_value(i+1);
            gma_two_bands(c, gma_method_catchment, fd, cell);
            break;
        }

    }

    if (0) {
        GDALRasterBand *ua = ((GDALDataset*)GDALOpen("ua.tiff", GA_ReadOnly))->GetRasterBand(1);
        GDALRasterBand *c = ((GDALDataset*)GDALOpen("catchments.tiff", GA_ReadOnly))->GetRasterBand(1);
        GDALDriver *driver = ua->GetDataset()->GetDriver();
        int w = ua->GetXSize();
        int h = ua->GetYSize();
        GDALRasterBand *ua2 = driver->Create("ua2.tiff", w, h, 1, GDT_Float32, NULL)->GetRasterBand(1);
        gma_two_bands(ua2, gma_method_add_band, ua);
        gma_two_bands(ua2, gma_method_multiply_by_band, c);
        gma_simple(ua2, gma_method_log);
    }

}
