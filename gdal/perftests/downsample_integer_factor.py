# SPDX-License-Identifier: MIT
# Copyright 2020 Even Rouault

from osgeo import gdal
import timeit

ds = gdal.GetDriverByName('MEM').Create('', 1024*10, 1024*10)
ds.GetRasterBand(1).Fill(127)

ds_nodata = gdal.GetDriverByName('MEM').Create('', 1024*10, 1024*10)
ds_nodata.GetRasterBand(1).SetNoDataValue(0)
ds_nodata.GetRasterBand(1).Fill(127)

NITERS = 50

def testNear(downsampling_factor):
    ds.ReadRaster(buf_xsize=ds.RasterXSize // downsampling_factor,
                  buf_ysize=ds.RasterYSize // downsampling_factor,
                  resample_alg=gdal.GRIORA_NearestNeighbour)

def testAverage(downsampling_factor):
    ds.ReadRaster(buf_xsize=ds.RasterXSize // downsampling_factor,
                  buf_ysize=ds.RasterYSize // downsampling_factor,
                  resample_alg=gdal.GRIORA_Average)

def testAverageNoData(downsampling_factor):
    ds_nodata.ReadRaster(buf_xsize=ds_nodata.RasterXSize // downsampling_factor,
                         buf_ysize=ds_nodata.RasterYSize // downsampling_factor,
                         resample_alg=gdal.GRIORA_Average)

def testRMS(downsampling_factor):
    ds.ReadRaster(buf_xsize=ds.RasterXSize // downsampling_factor,
                  buf_ysize=ds.RasterYSize // downsampling_factor,
                  resample_alg=gdal.GRIORA_RMS)

def testCubic(downsampling_factor):
    ds.ReadRaster(buf_xsize=ds.RasterXSize // downsampling_factor,
                  buf_ysize=ds.RasterYSize // downsampling_factor,
                  resample_alg=gdal.GRIORA_Cubic)

print('testNear(2): %.3f' % timeit.timeit("testNear(2)", setup="from __main__ import testNear", number=NITERS))
print('testNear(4): %.3f' % timeit.timeit("testNear(4)", setup="from __main__ import testNear", number=NITERS))

print('testAverage(2): %.3f' % timeit.timeit("testAverage(2)", setup="from __main__ import testAverage", number=NITERS))
print('testAverageNoData(2): %.3f' % timeit.timeit("testAverageNoData(2)", setup="from __main__ import testAverageNoData", number=NITERS))
print('testAverage(4): %.3f' % timeit.timeit("testAverage(4)", setup="from __main__ import testAverage", number=NITERS))
print('testAverageNoData(4): %.3f' % timeit.timeit("testAverageNoData(4)", setup="from __main__ import testAverageNoData", number=NITERS))

print('testRMS(2): %.3f' % timeit.timeit("testRMS(2)", setup="from __main__ import testRMS", number=NITERS))
print('testRMS(4): %.3f' % timeit.timeit("testRMS(4)", setup="from __main__ import testRMS", number=NITERS))

print('testCubic(2): %.3f' % timeit.timeit("testCubic(2)", setup="from __main__ import testCubic", number=NITERS))
print('testCubic(4): %.3f' % timeit.timeit("testCubic(4)", setup="from __main__ import testCubic", number=NITERS))
