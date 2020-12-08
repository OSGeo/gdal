# SPDX-License-Identifier: MIT
# Copyright 2020 Even Rouault

from osgeo import gdal
import timeit

ds = gdal.GetDriverByName('MEM').Create('', 1024*10, 1024*10)
ds.GetRasterBand(1).Fill(127)

ds_nodata = gdal.GetDriverByName('MEM').Create('', 1024*10, 1024*10)
ds_nodata.GetRasterBand(1).SetNoDataValue(0)
ds_nodata.GetRasterBand(1).Fill(127)

ds_uint16 = gdal.GetDriverByName('MEM').Create('', 1024*10, 1024*10, 1, gdal.GDT_UInt16)
ds_uint16.GetRasterBand(1).Fill(32767)

ds_uint16_16383 = gdal.GetDriverByName('MEM').Create('', 1024*10, 1024*10, 1, gdal.GDT_UInt16)
ds_uint16_16383.GetRasterBand(1).Fill(16383)

ds_float32 = gdal.GetDriverByName('MEM').Create('', 1024*10, 1024*10, 1, gdal.GDT_Float32)
ds_float32.GetRasterBand(1).Fill(32767)

NITERS = 50

def testNear(downsampling_factor):
    ds.ReadRaster(buf_xsize=ds.RasterXSize // downsampling_factor,
                  buf_ysize=ds.RasterYSize // downsampling_factor,
                  resample_alg=gdal.GRIORA_NearestNeighbour)

def testNearUInt16(downsampling_factor):
    ds_uint16.ReadRaster(buf_xsize=ds_uint16.RasterXSize // downsampling_factor,
                         buf_ysize=ds_uint16.RasterYSize // downsampling_factor,
                         resample_alg=gdal.GRIORA_NearestNeighbour)

def testNearFloat32(downsampling_factor):
    ds_float32.ReadRaster(buf_xsize=ds_float32.RasterXSize // downsampling_factor,
                          buf_ysize=ds_float32.RasterYSize // downsampling_factor,
                          resample_alg=gdal.GRIORA_NearestNeighbour)


def testAverage(downsampling_factor):
    ds.ReadRaster(buf_xsize=ds.RasterXSize // downsampling_factor,
                  buf_ysize=ds.RasterYSize // downsampling_factor,
                  resample_alg=gdal.GRIORA_Average)

def testAverageUInt16(downsampling_factor):
    ds_uint16.ReadRaster(buf_xsize=ds_uint16.RasterXSize // downsampling_factor,
                         buf_ysize=ds_uint16.RasterYSize // downsampling_factor,
                         resample_alg=gdal.GRIORA_Average)

def testAverageFloat32(downsampling_factor):
    ds_float32.ReadRaster(buf_xsize=ds_float32.RasterXSize // downsampling_factor,
                          buf_ysize=ds_float32.RasterYSize // downsampling_factor,
                          resample_alg=gdal.GRIORA_Average)

def testAverageNoData(downsampling_factor):
    ds_nodata.ReadRaster(buf_xsize=ds_nodata.RasterXSize // downsampling_factor,
                         buf_ysize=ds_nodata.RasterYSize // downsampling_factor,
                         resample_alg=gdal.GRIORA_Average)


def testRMS(downsampling_factor):
    ds.ReadRaster(buf_xsize=ds.RasterXSize // downsampling_factor,
                  buf_ysize=ds.RasterYSize // downsampling_factor,
                  resample_alg=gdal.GRIORA_RMS)

def testRMSUInt16(downsampling_factor):
    ds_uint16.ReadRaster(buf_xsize=ds_uint16.RasterXSize // downsampling_factor,
                         buf_ysize=ds_uint16.RasterYSize // downsampling_factor,
                         resample_alg=gdal.GRIORA_RMS)

# Test special case where all values are < 2^14.
def testRMSUInt16_16383(downsampling_factor):
    ds_uint16_16383.ReadRaster(buf_xsize=ds_uint16_16383.RasterXSize // downsampling_factor,
                               buf_ysize=ds_uint16_16383.RasterYSize // downsampling_factor,
                               resample_alg=gdal.GRIORA_RMS)

def testRMSFloat32(downsampling_factor):
    ds_float32.ReadRaster(buf_xsize=ds_float32.RasterXSize // downsampling_factor,
                          buf_ysize=ds_float32.RasterYSize // downsampling_factor,
                          resample_alg=gdal.GRIORA_RMS)
 

def testCubic(downsampling_factor):
    ds.ReadRaster(buf_xsize=ds.RasterXSize // downsampling_factor,
                  buf_ysize=ds.RasterYSize // downsampling_factor,
                  resample_alg=gdal.GRIORA_Cubic)

print('testNearUInt16(2): %.3f' % timeit.timeit("testNearUInt16(2)", setup="from __main__ import testNearUInt16", number=NITERS))
print('testAverageUInt16(2): %.3f' % timeit.timeit("testAverageUInt16(2)", setup="from __main__ import testAverageUInt16", number=NITERS))
print('testRMSUInt16(2): %.3f' % timeit.timeit("testRMSUInt16(2)", setup="from __main__ import testRMSUInt16", number=NITERS))
print('testRMSUInt16_16383(2): %.3f' % timeit.timeit("testRMSUInt16_16383(2)", setup="from __main__ import testRMSUInt16_16383", number=NITERS))
print('testNearFloat32(2): %.3f' % timeit.timeit("testNearFloat32(2)", setup="from __main__ import testNearFloat32", number=NITERS))
print('testAverageFloat32(2): %.3f' % timeit.timeit("testAverageFloat32(2)", setup="from __main__ import testAverageFloat32", number=NITERS))
print('testRMSFloat32(2): %.3f' % timeit.timeit("testRMSFloat32(2)", setup="from __main__ import testRMSFloat32", number=NITERS))

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
