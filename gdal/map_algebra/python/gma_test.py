from osgeo import gdal
import gma

driver = gdal.GetDriverByName('MEM')
dataset = driver.Create('', 16, 16, 1)
band = dataset.GetRasterBand(1)
b = gma.gma_new_band(band)
b._print()
