#!/usr/bin/env python
# -*- coding: utf-8 -*-
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test basic read support for a all datatypes from a VRT file.
# Author:   Frank Warmerdam <warmerdam@pobox.com>
# 
###############################################################################
# Copyright (c) 2003, Frank Warmerdam <warmerdam@pobox.com>
# 
# Permission is hereby granted, free of charge, to any person obtaining a
# copy of this software and associated documentation files (the "Software"),
# to deal in the Software without restriction, including without limitation
# the rights to use, copy, modify, merge, publish, distribute, sublicense,
# and/or sell copies of the Software, and to permit persons to whom the
# Software is furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included
# in all copies or substantial portions of the Software.
# 
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
# OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
# THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
# FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
# DEALINGS IN THE SOFTWARE.
###############################################################################

import os
import sys

sys.path.append( '../pymod' )

import gdaltest
from osgeo import gdal

###############################################################################
# When imported build a list of units based on the files available.

gdaltest_list = []

init_list = [ \
    ('byte.vrt', 1, 4672, None),
    ('int16.vrt', 1, 4672, None),
    ('uint16.vrt', 1, 4672, None),
    ('int32.vrt', 1, 4672, None),
    ('uint32.vrt', 1, 4672, None),
    ('float32.vrt', 1, 4672, None),
    ('float64.vrt', 1, 4672, None),
    ('cint16.vrt', 1, 5028, None),
    ('cint32.vrt', 1, 5028, None),
    ('cfloat32.vrt', 1, 5028, None),
    ('cfloat64.vrt', 1, 5028, None),
    ('msubwinbyte.vrt', 2, 2699, None),
    ('utmsmall.vrt', 1, 50054, None),
    ('byte_nearest_50pct.vrt', 1, 1192, None),
    ('byte_averaged_50pct.vrt', 1, 1152, None),
    ('byte_nearest_200pct.vrt', 1, 18784, None),
    ('byte_averaged_200pct.vrt', 1, 18784, None)]

###############################################################################
# The VRT references a non existing TIF file

def vrt_read_1():

    gdal.PushErrorHandler('CPLQuietErrorHandler')
    ds = gdal.Open('data/idontexist.vrt')
    gdal.PopErrorHandler()

    if ds is None:
        return 'success'

    return 'fail'

###############################################################################
# The VRT references a non existing TIF file, but using the proxy pool dataset API (#2837)

def vrt_read_2():

    ds = gdal.Open('data/idontexist2.vrt')
    if ds is None:
        return 'fail'

    gdal.PushErrorHandler('CPLQuietErrorHandler')
    cs = ds.GetRasterBand(1).Checksum()
    gdal.PopErrorHandler()

    if cs != 0:
        return 'fail'

    ds.GetMetadata()
    ds.GetRasterBand(1).GetMetadata()
    ds.GetGCPs()

    ds = None
    return 'success'

###############################################################################
# Test init of band data in case of cascaded VRT (ticket #2867)

def vrt_read_3():

    driver_tif = gdal.GetDriverByName("GTIFF")

    output_dst = driver_tif.Create( 'tmp/test_mosaic1.tif', 100, 100, 3, gdal.GDT_Byte)
    output_dst.GetRasterBand(1).Fill(255)
    output_dst = None

    output_dst = driver_tif.Create( 'tmp/test_mosaic2.tif', 100, 100, 3, gdal.GDT_Byte)
    output_dst.GetRasterBand(1).Fill(127)
    output_dst = None
    
    ds = gdal.Open('data/test_mosaic.vrt')
    # A simple Checksum() cannot detect if the fix works or not as
    # Checksum() reads line per line, and we must use IRasterIO() on multi-line request
    data = ds.GetRasterBand(1).ReadRaster(90,0,20,100)
    import struct
    got = struct.unpack('B' * 20*100, data)
    for i in range(100):
        if got[i*20 + 9 ] != 255:
            gdaltest.post_reason('at line %d, did not find 255' % i)
            return 'fail'
    ds = None
    
    driver_tif.Delete('tmp/test_mosaic1.tif')
    driver_tif.Delete('tmp/test_mosaic2.tif')

    return 'success'


###############################################################################
# Test complex source with complex data (#3977)

def vrt_read_4():

    try:
        import numpy as np
    except:
        return 'skip'

    data = np.zeros((1, 1), np.complex64)
    data[0, 0] = 1. + 3.j

    drv = gdal.GetDriverByName('GTiff')
    ds = drv.Create("/vsimem/test.tif", 1, 1, 1, gdal.GDT_CFloat32)
    ds.GetRasterBand(1).WriteArray(data)
    ds = None

    complex_xml = '''<VRTDataset rasterXSize="1" rasterYSize="1">
  <VRTRasterBand dataType="CFloat32" band="1">
    <ComplexSource>
      <SourceFilename relativeToVRT="1">/vsimem/test.tif</SourceFilename>
      <SourceBand>1</SourceBand>
      <ScaleOffset>3</ScaleOffset>
      <ScaleRatio>2</ScaleRatio>
    </ComplexSource>
  </VRTRasterBand>
</VRTDataset>
'''

    ds = gdal.Open(complex_xml)
    scaleddata = ds.GetRasterBand(1).ReadAsArray()
    ds = None

    gdal.Unlink("/vsimem/test.tif")

    if scaleddata[0, 0].real != 5.0 or scaleddata[0, 0].imag != 9.0:
        gdaltest.post_reason('did not get expected value')
        print('scaleddata[0, 0]: %f %f' % (scaleddata[0, 0].real, scaleddata[0, 0].imag))
        return 'fail'

    return 'success'

###############################################################################
# Test serializing and deserializing of various band metadata

def vrt_read_5():

    src_ds = gdal.Open('data/testserialization.asc')
    ds = gdal.GetDriverByName('VRT').CreateCopy('/vsimem/vrt_read_5.vrt', src_ds)
    src_ds = None
    ds = None

    ds = gdal.Open('/vsimem/vrt_read_5.vrt')

    gcps = ds.GetGCPs()
    if len(gcps) != 2 or ds.GetGCPCount() != 2:
        return 'fail'

    if ds.GetGCPProjection().find("WGS 84") == -1:
        print(ds.GetGCPProjection())
        return 'fail'

    band = ds.GetRasterBand(1)
    if band.GetDescription() != 'MyDescription':
        print(band.GetDescription())
        return 'fail'

    if band.GetUnitType() != 'MyUnit':
        print(band.GetUnitType())
        return 'fail'

    if band.GetOffset() != 1:
        print(band.GetOffset())
        return 'fail'

    if band.GetScale() != 2:
        print(band.GetScale())
        return 'fail'

    if band.GetRasterColorInterpretation() != gdal.GCI_PaletteIndex:
        print(band.GetRasterColorInterpretation())
        return 'fail'

    if band.GetCategoryNames() != ['Cat1', 'Cat2']:
        print(band.GetCategoryNames())
        return 'fail'

    ct = band.GetColorTable()
    if ct.GetColorEntry(0) != (0,0,0,255):
        print(ct.GetColorEntry(0))
        return 'fail'
    if ct.GetColorEntry(1) != (1,1,1,255):
        print(ct.GetColorEntry(1))
        return 'fail'

    if band.GetMaximum() != 0:
        print(band.GetMaximum())
        return 'fail'

    if band.GetMinimum() != 2:
        print(band.GetMinimum())
        return 'fail'

    if band.GetMetadata() != {'STATISTICS_MEAN': '1', 'STATISTICS_MINIMUM': '2', 'STATISTICS_MAXIMUM': '0', 'STATISTICS_STDDEV': '3'}:
        print(band.GetMetadata())
        return 'fail'

    ds = None

    gdal.Unlink('/vsimem/vrt_read_5.vrt')

    return 'success'

###############################################################################
# Test GetMinimum() and GetMaximum()

def vrt_read_6():

    try:
        os.unlink('data/byte.tif.aux.xml')
        print('Removed data/byte.tif.aux.xml. Was not supposed to exist...')
    except:
        pass

    src_ds = gdal.Open('data/byte.tif')
    mem_ds = gdal.GetDriverByName('GTiff').CreateCopy('/vsimem/vrt_read_6.tif', src_ds)
    vrt_ds = gdal.GetDriverByName('VRT').CreateCopy('/vsimem/vrt_read_6.vrt', mem_ds)

    if vrt_ds.GetRasterBand(1).GetMinimum() is not None:
        gdaltest.post_reason('got bad minimum value')
        print(vrt_ds.GetRasterBand(1).GetMinimum())
        return 'fail'
    if vrt_ds.GetRasterBand(1).GetMaximum() is not None:
        gdaltest.post_reason('got bad maximum value')
        print(vrt_ds.GetRasterBand(1).GetMaximum())
        return 'fail'

    # Now compute source statistics
    mem_ds.GetRasterBand(1).ComputeStatistics(False)

    if vrt_ds.GetRasterBand(1).GetMinimum() != 74:
        gdaltest.post_reason('got bad minimum value')
        print(vrt_ds.GetRasterBand(1).GetMinimum())
        return 'fail'
    if vrt_ds.GetRasterBand(1).GetMaximum() != 255:
        gdaltest.post_reason('got bad maximum value')
        print(vrt_ds.GetRasterBand(1).GetMaximum())
        return 'fail'

    mem_ds = None
    vrt_ds = None

    gdal.GetDriverByName('GTiff').Delete('/vsimem/vrt_read_6.tif')
    gdal.GetDriverByName('VRT').Delete('/vsimem/vrt_read_6.vrt')

    return 'success'

###############################################################################
# Test GDALOpen() anti-recursion mechanism

def vrt_read_7():

    filename = "/vsimem/vrt_read_7.vrt"

    content = """<VRTDataset rasterXSize="20" rasterYSize="20">
  <VRTRasterBand dataType="Byte" band="1">
    <SimpleSource>
      <SourceFilename relativeToVRT="1">%s</SourceFilename>
      <SourceBand>1</SourceBand>
      <SrcRect xOff="0" yOff="0" xSize="20" ySize="20" />
      <DstRect xOff="0" yOff="0" xSize="20" ySize="20" />
    </SimpleSource>
  </VRTRasterBand>
</VRTDataset>""" % filename

    gdal.FileFromMemBuffer(filename, content)
    gdal.PushErrorHandler('CPLQuietErrorHandler')
    ds = gdal.Open(filename)
    gdal.PopErrorHandler()
    error_msg = gdal.GetLastErrorMsg()
    gdal.Unlink(filename)

    if ds is not None:
        return 'fail'

    if error_msg != 'GDALOpen() called with too many recursion levels':
        return 'fail'

    return 'success'

###############################################################################
# Test ComputeRasterMinMax()

def vrt_read_8():

    src_ds = gdal.Open('data/byte.tif')
    mem_ds = gdal.GetDriverByName('GTiff').CreateCopy('/vsimem/vrt_read_8.tif', src_ds)
    vrt_ds = gdal.GetDriverByName('VRT').CreateCopy('/vsimem/vrt_read_8.vrt', mem_ds)

    vrt_minmax = vrt_ds.GetRasterBand(1).ComputeRasterMinMax()
    mem_minmax = mem_ds.GetRasterBand(1).ComputeRasterMinMax()

    mem_ds = None
    vrt_ds = None

    gdal.GetDriverByName('GTiff').Delete('/vsimem/vrt_read_8.tif')
    gdal.GetDriverByName('VRT').Delete('/vsimem/vrt_read_8.vrt')

    if vrt_minmax != mem_minmax:
        print(vrt_minmax)
        print(mem_minmax)
        return 'fail'

    return 'success'

###############################################################################
# Test ComputeStatistics()

def vrt_read_9():

    src_ds = gdal.Open('data/byte.tif')
    mem_ds = gdal.GetDriverByName('GTiff').CreateCopy('/vsimem/vrt_read_9.tif', src_ds)
    vrt_ds = gdal.GetDriverByName('VRT').CreateCopy('/vsimem/vrt_read_9.vrt', mem_ds)

    vrt_stats = vrt_ds.GetRasterBand(1).ComputeStatistics(False)
    mem_stats = mem_ds.GetRasterBand(1).ComputeStatistics(False)

    mem_ds = None
    vrt_ds = None

    gdal.GetDriverByName('GTiff').Delete('/vsimem/vrt_read_9.tif')
    gdal.GetDriverByName('VRT').Delete('/vsimem/vrt_read_9.vrt')

    if vrt_stats != mem_stats:
        print(vrt_stats)
        print(mem_stats)
        return 'fail'

    return 'success'

###############################################################################
# Test GetHistogram()

def vrt_read_10():

    src_ds = gdal.Open('data/byte.tif')
    mem_ds = gdal.GetDriverByName('GTiff').CreateCopy('/vsimem/vrt_read_10.tif', src_ds)
    vrt_ds = gdal.GetDriverByName('VRT').CreateCopy('/vsimem/vrt_read_10.vrt', mem_ds)

    vrt_hist = vrt_ds.GetRasterBand(1).GetHistogram()
    mem_hist = mem_ds.GetRasterBand(1).GetHistogram()

    mem_ds = None
    vrt_ds = None

    gdal.GetDriverByName('GTiff').Delete('/vsimem/vrt_read_10.tif')
    gdal.GetDriverByName('VRT').Delete('/vsimem/vrt_read_10.vrt')
    
    if vrt_hist != mem_hist:
        print(vrt_hist)
        print(mem_hist)
        return 'fail'

    return 'success'

###############################################################################
# Test resolving files from a symlinked vrt using relativeToVRT with an absolute symlink

def vrt_read_11():

    if not gdaltest.support_symlink():
        return 'skip'

    try:
        os.remove('tmp/byte.vrt')
        print('Removed tmp/byte.vrt. Was not supposed to exist...')
    except:
        pass

    os.symlink(os.path.join(os.getcwd(), 'data/byte.vrt'), 'tmp/byte.vrt')

    ds = gdal.Open('tmp/byte.vrt')

    os.remove('tmp/byte.vrt')

    if ds is None:
        return 'fail'

    return 'success'

###############################################################################
# Test resolving files from a symlinked vrt using relativeToVRT
# with a relative symlink pointing to a relative symlink

def vrt_read_12():

    if not gdaltest.support_symlink():
        return 'skip'

    try:
        os.remove('tmp/byte.vrt')
        print('Removed tmp/byte.vrt. Was not supposed to exist...')
    except:
        pass

    os.symlink('../data/byte.vrt', 'tmp/byte.vrt')

    ds = gdal.Open('tmp/byte.vrt')

    os.remove('tmp/byte.vrt')

    if ds is None:
        return 'fail'

    return 'success'

###############################################################################
# Test resolving files from a symlinked vrt using relativeToVRT with a relative symlink

def vrt_read_13():

    if not gdaltest.support_symlink():
        return 'skip'

    try:
        os.remove('tmp/byte.vrt')
        print('Removed tmp/byte.vrt. Was not supposed to exist...')
    except:
        pass
    try:
        os.remove('tmp/other_byte.vrt')
        print('Removed tmp/other_byte.vrt. Was not supposed to exist...')
    except:
        pass

    os.symlink('../data/byte.vrt', 'tmp/byte.vrt')
    os.symlink('../tmp/byte.vrt', 'tmp/other_byte.vrt')

    ds = gdal.Open('tmp/other_byte.vrt')

    os.remove('tmp/other_byte.vrt')
    os.remove('tmp/byte.vrt')

    if ds is None:
        return 'fail'

    return 'success'

for item in init_list:
    ut = gdaltest.GDALTest( 'VRT', item[0], item[1], item[2] )
    if ut is None:
        print( 'VRT tests skipped' )
        sys.exit()
    gdaltest_list.append( (ut.testOpen, item[0]) )
    
gdaltest_list.append( vrt_read_1 )
gdaltest_list.append( vrt_read_2 )
gdaltest_list.append( vrt_read_3 )
gdaltest_list.append( vrt_read_4 )
gdaltest_list.append( vrt_read_5 )
gdaltest_list.append( vrt_read_6 )
gdaltest_list.append( vrt_read_7 )
gdaltest_list.append( vrt_read_8 )
gdaltest_list.append( vrt_read_9 )
gdaltest_list.append( vrt_read_10 )
gdaltest_list.append( vrt_read_11 )
gdaltest_list.append( vrt_read_12 )
gdaltest_list.append( vrt_read_13 )

if __name__ == '__main__':

    gdaltest.setup_run( 'vrt_read' )

    gdaltest.run_tests( gdaltest_list )

    gdaltest.summarize()

