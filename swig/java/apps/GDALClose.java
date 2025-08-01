import org.gdal.gdal.Band;
import org.gdal.gdal.Dataset;
import org.gdal.gdal.Driver;
import org.gdal.gdal.gdal;

import static org.gdal.gdalconst.gdalconstConstants.GDT_Byte;

public class GDALClose {
    public static void main(String[] args) {

        gdal.AllRegister();

        Driver drv = gdal.GetDriverByName("GTiff");
        Dataset ds = drv.Create("/vsimem/test.tif", 10, 10, 1, GDT_Byte, new String[]{});
        Band bd = ds.GetRasterBand(1);
        System.err.println("First closing");
        bd.GetDataset().Close();
        ds = null;
        System.gc();

        ds = gdal.Open("/vsimem/test.tif");
        bd = ds.GetRasterBand(1);
        System.err.println("Second closing");
        bd.GetDataset().Close();
        ds = null;
        bd = null;
        System.gc();
        System.gc();
        try {
            Thread.sleep(500);
        } catch (Exception ignore) {
        }
    }
}
