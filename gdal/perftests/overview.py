# SPDX-License-Identifier: MIT
# Copyright 2020 Even Rouault

from osgeo import gdal
import time

def doit(compress, threads):

    gdal.SetConfigOption('GDAL_NUM_THREADS', str(threads))

    filename = '/vsimem/test.tif'
    ds = gdal.GetDriverByName('GTiff').Create(filename, 20000, 20000, 3,
                                              options = ['COMPRESS=' + compress,
                                                         'TILED=YES'])
    ds.GetRasterBand(1).Fill(50)
    ds.GetRasterBand(3).Fill(100)
    ds.GetRasterBand(3).Fill(200)
    ds = None

    ds = gdal.Open(filename, gdal.GA_Update)
    start = time.time()
    ds.BuildOverviews('CUBIC', [2,4,8])
    end = time.time()
    print('COMPRESS=%s, NUM_THREADS=%d: %.2f' % (compress, threads, end - start))

    gdal.SetConfigOption('GDAL_NUM_THREADS', None)

doit('NONE', 0)
doit('NONE', 2)
doit('NONE', 4)
doit('NONE', 8)

doit('ZSTD', 0)
doit('ZSTD', 2)
doit('ZSTD', 4)
doit('ZSTD', 8)
