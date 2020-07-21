# SPDX-License-Identifier: MIT
# Copyright 2020 Even Rouault

from osgeo import gdal
import time


srcfilename = '/vsimem/test.tif'
ds = gdal.GetDriverByName('GTiff').Create(srcfilename, 20000, 20000, 3,
                                          options = ['COMPRESS=' + 'ZSTD',
                                                     'TILED=YES'])
ds.GetRasterBand(1).Fill(50)
ds.GetRasterBand(3).Fill(100)
ds.GetRasterBand(3).Fill(200)
ds = None

def doit(compress, threads):

    gdal.SetConfigOption('GDAL_NUM_THREADS', str(threads))

    start = time.time()
    gdal.Translate('/vsimem/out.tif', srcfilename,
                   options = '-of COG -co COMPRESS=' + compress)
    end = time.time()
    print('COMPRESS=%s, NUM_THREADS=%d: %.2f' % (compress, threads, end - start))

    gdal.SetConfigOption('GDAL_NUM_THREADS', None)


doit('ZSTD', 0)
doit('ZSTD', 2)
doit('ZSTD', 4)
doit('ZSTD', 8)

doit('WEBP', 0)
doit('WEBP', 2)
doit('WEBP', 4)
doit('WEBP', 8)
