#!/usr/bin/env pytest
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
# Copyright (c) 2008-2012, Even Rouault <even dot rouault at spatialys.com>
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
import shutil
import struct

import pytest

import gdaltest
from osgeo import gdal
import test_cli_utilities

###############################################################################
# When imported build a list of units based on the files available.


init_list = [
    ('byte.vrt', 4672),
    ('int16.vrt', 4672),
    ('uint16.vrt', 4672),
    ('int32.vrt', 4672),
    ('uint32.vrt', 4672),
    ('float32.vrt', 4672),
    ('float64.vrt', 4672),
    ('cint16.vrt', 5028),
    ('cint32.vrt', 5028),
    ('cfloat32.vrt', 5028),
    ('cfloat64.vrt', 5028),
    ('msubwinbyte.vrt', 2699),
    ('utmsmall.vrt', 50054),
    ('byte_nearest_50pct.vrt', 1192),
    ('byte_averaged_50pct.vrt', 1152),
    ('byte_nearest_200pct.vrt', 18784),
    ('byte_averaged_200pct.vrt', 18784)
]


@pytest.mark.parametrize(
    'filename,checksum',
    init_list,
    ids=[tup[0].split('.')[0] for tup in init_list],
)
@pytest.mark.require_driver('VRT')
def test_vrt_open(filename, checksum):
    ut = gdaltest.GDALTest('VRT', filename, 1, checksum)
    ut.testOpen()


###############################################################################
# The VRT references a non existing TIF file


def test_vrt_read_1():

    gdal.PushErrorHandler('CPLQuietErrorHandler')
    ds = gdal.Open('data/idontexist.vrt')
    gdal.PopErrorHandler()

    if ds is None:
        return

    pytest.fail()

###############################################################################
# The VRT references a non existing TIF file, but using the proxy pool dataset API (#2837)


def test_vrt_read_2():

    ds = gdal.Open('data/idontexist2.vrt')
    assert ds is not None

    gdal.PushErrorHandler('CPLQuietErrorHandler')
    cs = ds.GetRasterBand(1).Checksum()
    gdal.PopErrorHandler()

    assert cs == 0

    ds.GetMetadata()
    ds.GetRasterBand(1).GetMetadata()
    ds.GetGCPs()

    ds = None

###############################################################################
# Test init of band data in case of cascaded VRT (ticket #2867)


def test_vrt_read_3():

    driver_tif = gdal.GetDriverByName("GTIFF")

    output_dst = driver_tif.Create('tmp/test_mosaic1.tif', 100, 100, 3, gdal.GDT_Byte)
    output_dst.GetRasterBand(1).Fill(255)
    output_dst = None

    output_dst = driver_tif.Create('tmp/test_mosaic2.tif', 100, 100, 3, gdal.GDT_Byte)
    output_dst.GetRasterBand(1).Fill(127)
    output_dst = None

    ds = gdal.Open('data/test_mosaic.vrt')
    # A simple Checksum() cannot detect if the fix works or not as
    # Checksum() reads line per line, and we must use IRasterIO() on multi-line request
    data = ds.GetRasterBand(1).ReadRaster(90, 0, 20, 100)
    got = struct.unpack('B' * 20 * 100, data)
    for i in range(100):
        assert got[i * 20 + 9] == 255, ('at line %d, did not find 255' % i)
    ds = None

    driver_tif.Delete('tmp/test_mosaic1.tif')
    driver_tif.Delete('tmp/test_mosaic2.tif')


###############################################################################
# Test complex source with complex data (#3977)

def test_vrt_read_4():

    try:
        import numpy as np
    except ImportError:
        pytest.skip()

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
        print('scaleddata[0, 0]: %f %f' % (scaleddata[0, 0].real, scaleddata[0, 0].imag))
        pytest.fail('did not get expected value')


###############################################################################
# Test serializing and deserializing of various band metadata


def test_vrt_read_5():

    src_ds = gdal.Open('data/testserialization.asc')
    ds = gdal.GetDriverByName('VRT').CreateCopy('/vsimem/vrt_read_5.vrt', src_ds)
    src_ds = None
    ds = None

    ds = gdal.Open('/vsimem/vrt_read_5.vrt')

    gcps = ds.GetGCPs()
    assert len(gcps) == 2 and ds.GetGCPCount() == 2

    assert ds.GetGCPProjection().find("WGS 84") != -1

    ds.SetGCPs(ds.GetGCPs(), ds.GetGCPProjection())

    gcps = ds.GetGCPs()
    assert len(gcps) == 2 and ds.GetGCPCount() == 2

    assert ds.GetGCPProjection().find("WGS 84") != -1

    band = ds.GetRasterBand(1)
    assert band.GetDescription() == 'MyDescription'

    assert band.GetUnitType() == 'MyUnit'

    assert band.GetOffset() == 1

    assert band.GetScale() == 2

    assert band.GetRasterColorInterpretation() == gdal.GCI_PaletteIndex

    assert band.GetCategoryNames() == ['Cat1', 'Cat2']

    ct = band.GetColorTable()
    assert ct.GetColorEntry(0) == (0, 0, 0, 255)
    assert ct.GetColorEntry(1) == (1, 1, 1, 255)

    assert band.GetMaximum() == 0

    assert band.GetMinimum() == 2

    assert band.GetMetadata() == {'STATISTICS_MEAN': '1', 'STATISTICS_MINIMUM': '2', 'STATISTICS_MAXIMUM': '0', 'STATISTICS_STDDEV': '3'}

    ds = None

    gdal.Unlink('/vsimem/vrt_read_5.vrt')

###############################################################################
# Test GetMinimum() and GetMaximum()


def test_vrt_read_6():

    gdal.Unlink('data/byte.tif.aux.xml')
    src_ds = gdal.Open('data/byte.tif')
    mem_ds = gdal.GetDriverByName('GTiff').CreateCopy('/vsimem/vrt_read_6.tif', src_ds)
    vrt_ds = gdal.GetDriverByName('VRT').CreateCopy('/vsimem/vrt_read_6.vrt', mem_ds)

    assert vrt_ds.GetRasterBand(1).GetMinimum() is None, 'got bad minimum value'
    assert vrt_ds.GetRasterBand(1).GetMaximum() is None, 'got bad maximum value'

    # Now compute source statistics
    mem_ds.GetRasterBand(1).ComputeStatistics(False)

    assert vrt_ds.GetRasterBand(1).GetMinimum() == 74, 'got bad minimum value'
    assert vrt_ds.GetRasterBand(1).GetMaximum() == 255, 'got bad maximum value'

    mem_ds = None
    vrt_ds = None

    gdal.GetDriverByName('GTiff').Delete('/vsimem/vrt_read_6.tif')
    gdal.GetDriverByName('VRT').Delete('/vsimem/vrt_read_6.vrt')

###############################################################################
# Test GDALOpen() anti-recursion mechanism


def test_vrt_read_7():

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

    assert ds is None

    assert error_msg != ''

###############################################################################
# Test ComputeRasterMinMax()


def test_vrt_read_8():

    src_ds = gdal.Open('data/byte.tif')
    mem_ds = gdal.GetDriverByName('GTiff').CreateCopy('/vsimem/vrt_read_8.tif', src_ds)
    vrt_ds = gdal.GetDriverByName('VRT').CreateCopy('/vsimem/vrt_read_8.vrt', mem_ds)

    vrt_minmax = vrt_ds.GetRasterBand(1).ComputeRasterMinMax()
    mem_minmax = mem_ds.GetRasterBand(1).ComputeRasterMinMax()

    mem_ds = None
    vrt_ds = None

    gdal.GetDriverByName('GTiff').Delete('/vsimem/vrt_read_8.tif')
    gdal.GetDriverByName('VRT').Delete('/vsimem/vrt_read_8.vrt')

    assert vrt_minmax == mem_minmax

###############################################################################
# Test ComputeStatistics()


def test_vrt_read_9():

    src_ds = gdal.Open('data/byte.tif')
    mem_ds = gdal.GetDriverByName('GTiff').CreateCopy('/vsimem/vrt_read_9.tif', src_ds)
    vrt_ds = gdal.GetDriverByName('VRT').CreateCopy('/vsimem/vrt_read_9.vrt', mem_ds)

    vrt_stats = vrt_ds.GetRasterBand(1).ComputeStatistics(False)
    mem_stats = mem_ds.GetRasterBand(1).ComputeStatistics(False)

    mem_ds = None
    vrt_ds = None

    gdal.GetDriverByName('GTiff').Delete('/vsimem/vrt_read_9.tif')
    gdal.GetDriverByName('VRT').Delete('/vsimem/vrt_read_9.vrt')

    assert vrt_stats == mem_stats

###############################################################################
# Test GetHistogram() & GetDefaultHistogram()


def test_vrt_read_10():

    src_ds = gdal.Open('data/byte.tif')
    mem_ds = gdal.GetDriverByName('GTiff').CreateCopy('/vsimem/vrt_read_10.tif', src_ds)
    vrt_ds = gdal.GetDriverByName('VRT').CreateCopy('/vsimem/vrt_read_10.vrt', mem_ds)

    vrt_hist = vrt_ds.GetRasterBand(1).GetHistogram()
    mem_hist = mem_ds.GetRasterBand(1).GetHistogram()

    mem_ds = None
    vrt_ds = None

    f = gdal.VSIFOpenL('/vsimem/vrt_read_10.vrt', 'rb')
    content = gdal.VSIFReadL(1, 10000, f).decode('ascii')
    gdal.VSIFCloseL(f)

    assert vrt_hist == mem_hist

    assert '<Histograms>' in content

    # Single source optimization
    for i in range(2):
        gdal.FileFromMemBuffer('/vsimem/vrt_read_10.vrt',
                               """<VRTDataset rasterXSize="20" rasterYSize="20">
    <VRTRasterBand dataType="Byte" band="1">
        <SimpleSource>
        <SourceFilename relativeToVRT="1">vrt_read_10.tif</SourceFilename>
        </SimpleSource>
    </VRTRasterBand>
    </VRTDataset>""")

        ds = gdal.Open('/vsimem/vrt_read_10.vrt')
        if i == 0:
            ds.GetRasterBand(1).GetDefaultHistogram()
        else:
            ds.GetRasterBand(1).GetHistogram()
        ds = None

        f = gdal.VSIFOpenL('/vsimem/vrt_read_10.vrt', 'rb')
        content = gdal.VSIFReadL(1, 10000, f).decode('ascii')
        gdal.VSIFCloseL(f)

        assert '<Histograms>' in content

    # Two sources general case
    for i in range(2):
        gdal.FileFromMemBuffer('/vsimem/vrt_read_10.vrt',
                               """<VRTDataset rasterXSize="20" rasterYSize="20">
    <VRTRasterBand dataType="Byte" band="1">
        <SimpleSource>
        <SourceFilename relativeToVRT="1">vrt_read_10.tif</SourceFilename>
        </SimpleSource>
        <SimpleSource>
        <SourceFilename relativeToVRT="1">vrt_read_10.tif</SourceFilename>
        </SimpleSource>
    </VRTRasterBand>
    </VRTDataset>""")

        ds = gdal.Open('/vsimem/vrt_read_10.vrt')
        if i == 0:
            ds.GetRasterBand(1).GetDefaultHistogram()
        else:
            ds.GetRasterBand(1).GetHistogram()
        ds = None

        f = gdal.VSIFOpenL('/vsimem/vrt_read_10.vrt', 'rb')
        content = gdal.VSIFReadL(1, 10000, f).decode('ascii')
        gdal.VSIFCloseL(f)

        assert '<Histograms>' in content

    gdal.GetDriverByName('GTiff').Delete('/vsimem/vrt_read_10.tif')
    gdal.GetDriverByName('VRT').Delete('/vsimem/vrt_read_10.vrt')

###############################################################################
# Test resolving files from a symlinked vrt using relativeToVRT with an absolute symlink


def test_vrt_read_11():

    if not gdaltest.support_symlink():
        pytest.skip()

    try:
        os.remove('tmp/byte.vrt')
        print('Removed tmp/byte.vrt. Was not supposed to exist...')
    except OSError:
        pass

    os.symlink(os.path.join(os.getcwd(), 'data/byte.vrt'), 'tmp/byte.vrt')

    ds = gdal.Open('tmp/byte.vrt')

    os.remove('tmp/byte.vrt')

    assert ds is not None

###############################################################################
# Test resolving files from a symlinked vrt using relativeToVRT
# with a relative symlink pointing to a relative symlink


def test_vrt_read_12():

    if not gdaltest.support_symlink():
        pytest.skip()

    try:
        os.remove('tmp/byte.vrt')
        print('Removed tmp/byte.vrt. Was not supposed to exist...')
    except OSError:
        pass

    os.symlink('../data/byte.vrt', 'tmp/byte.vrt')

    ds = gdal.Open('tmp/byte.vrt')

    os.remove('tmp/byte.vrt')

    assert ds is not None

###############################################################################
# Test resolving files from a symlinked vrt using relativeToVRT with a relative symlink


def test_vrt_read_13():

    if not gdaltest.support_symlink():
        pytest.skip()

    try:
        os.remove('tmp/byte.vrt')
        print('Removed tmp/byte.vrt. Was not supposed to exist...')
    except OSError:
        pass
    try:
        os.remove('tmp/other_byte.vrt')
        print('Removed tmp/other_byte.vrt. Was not supposed to exist...')
    except OSError:
        pass

    os.symlink('../data/byte.vrt', 'tmp/byte.vrt')
    os.symlink('../tmp/byte.vrt', 'tmp/other_byte.vrt')

    ds = gdal.Open('tmp/other_byte.vrt')

    os.remove('tmp/other_byte.vrt')
    os.remove('tmp/byte.vrt')

    assert ds is not None

###############################################################################
# Test ComputeStatistics() when the VRT is a subwindow of the source dataset (#5468)


def test_vrt_read_14():

    src_ds = gdal.Open('data/byte.tif')
    mem_ds = gdal.GetDriverByName('GTiff').CreateCopy('/vsimem/vrt_read_14.tif', src_ds)
    mem_ds.FlushCache()  # hum this should not be necessary ideally
    vrt_ds = gdal.Open("""<VRTDataset rasterXSize="4" rasterYSize="4">
  <VRTRasterBand dataType="Byte" band="1">
    <SimpleSource>
      <SourceFilename relativeToVRT="0">/vsimem/vrt_read_14.tif</SourceFilename>
      <SourceBand>1</SourceBand>
      <SourceProperties RasterXSize="20" RasterYSize="20" DataType="Byte" BlockXSize="20" BlockYSize="20" />
      <SrcRect xOff="2" yOff="2" xSize="4" ySize="4" />
      <DstRect xOff="0" yOff="0" xSize="4" ySize="4" />
    </SimpleSource>
  </VRTRasterBand>
</VRTDataset>""")

    vrt_stats = vrt_ds.GetRasterBand(1).ComputeStatistics(False)

    mem_ds = None
    vrt_ds = None

    gdal.GetDriverByName('GTiff').Delete('/vsimem/vrt_read_14.tif')

    assert vrt_stats[0] == 115.0 and vrt_stats[1] == 173.0

###############################################################################
# Test RasterIO() with resampling on SimpleSource


def test_vrt_read_15():

    vrt_ds = gdal.Open("""<VRTDataset rasterXSize="9" rasterYSize="9">
  <VRTRasterBand dataType="Byte" band="1">
    <SimpleSource>
      <SourceFilename relativeToVRT="0">data/byte.tif</SourceFilename>
      <SourceBand>1</SourceBand>
      <SourceProperties RasterXSize="20" RasterYSize="20" DataType="Byte" BlockXSize="20" BlockYSize="20" />
      <SrcRect xOff="0" yOff="0" xSize="20" ySize="20" />
      <DstRect xOff="0" yOff="0" xSize="9" ySize="9" />
    </SimpleSource>
    <SimpleSource>
      <SourceFilename relativeToVRT="0">data/byte.tif</SourceFilename>
      <SourceBand>1</SourceBand>
      <SourceProperties RasterXSize="20" RasterYSize="20" DataType="Byte" BlockXSize="20" BlockYSize="20" />
      <SrcRect xOff="0" yOff="0" xSize="20" ySize="20" />
      <DstRect xOff="0" yOff="0" xSize="9" ySize="9" />
    </SimpleSource>
  </VRTRasterBand>
</VRTDataset>""")

    cs = vrt_ds.GetRasterBand(1).Checksum()
    assert cs == 1044

###############################################################################
# Test RasterIO() with resampling on ComplexSource


def test_vrt_read_16():

    vrt_ds = gdal.Open("""<VRTDataset rasterXSize="9" rasterYSize="9">
  <VRTRasterBand dataType="Byte" band="1">
    <ComplexSource>
      <SourceFilename relativeToVRT="0">data/byte.tif</SourceFilename>
      <SourceBand>1</SourceBand>
      <SourceProperties RasterXSize="20" RasterYSize="20" DataType="Byte" BlockXSize="20" BlockYSize="20" />
      <SrcRect xOff="0" yOff="0" xSize="20" ySize="20" />
      <DstRect xOff="0" yOff="0" xSize="9" ySize="9" />
    </ComplexSource>
    <ComplexSource>
      <SourceFilename relativeToVRT="0">data/byte.tif</SourceFilename>
      <SourceBand>1</SourceBand>
      <SourceProperties RasterXSize="20" RasterYSize="20" DataType="Byte" BlockXSize="20" BlockYSize="20" />
      <SrcRect xOff="0" yOff="0" xSize="20" ySize="20" />
      <DstRect xOff="0" yOff="0" xSize="9" ySize="9" />
    </ComplexSource>
  </VRTRasterBand>
</VRTDataset>""")

    cs = vrt_ds.GetRasterBand(1).Checksum()
    assert cs == 1044

###############################################################################
# Test RasterIO() with resampling on AveragedSource


def test_vrt_read_17():

    vrt_ds = gdal.Open("""<VRTDataset rasterXSize="9" rasterYSize="9">
  <VRTRasterBand dataType="Byte" band="1">
    <AveragedSource>
      <SourceFilename relativeToVRT="0">data/byte.tif</SourceFilename>
      <SourceBand>1</SourceBand>
      <SourceProperties RasterXSize="20" RasterYSize="20" DataType="Byte" BlockXSize="20" BlockYSize="20" />
      <SrcRect xOff="0" yOff="0" xSize="20" ySize="20" />
      <DstRect xOff="0" yOff="0" xSize="9" ySize="9" />
    </AveragedSource>
  </VRTRasterBand>
</VRTDataset>""")

    # Note: AveragedSource with resampling does not give consistent results
    # depending on the RasterIO() request
    cs = vrt_ds.GetRasterBand(1).Checksum()
    assert cs == 847

###############################################################################
# Test that relative path is correctly VRT-in-VRT


def test_vrt_read_18():

    vrt_ds = gdal.Open('data/vrtinvrt.vrt')
    cs = vrt_ds.GetRasterBand(1).Checksum()
    assert cs == 4672

###############################################################################
# Test shared="0"


def test_vrt_read_19():

    vrt_ds = gdal.Open("""<VRTDataset rasterXSize="20" rasterYSize="20">
  <VRTRasterBand dataType="Byte" band="1">
    <AveragedSource>
      <SourceFilename relativeToVRT="0" shared="0">data/byte.tif</SourceFilename>
      <SourceBand>1</SourceBand>
      <SourceProperties RasterXSize="20" RasterYSize="20" DataType="Byte" BlockXSize="20" BlockYSize="20" />
    </AveragedSource>
  </VRTRasterBand>
</VRTDataset>""")

    vrt2_ds = gdal.Open("""<VRTDataset rasterXSize="20" rasterYSize="20">
  <VRTRasterBand dataType="Byte" band="1">
    <AveragedSource>
      <SourceFilename relativeToVRT="0" shared="0">data/byte.tif</SourceFilename>
      <SourceBand>1</SourceBand>
    </AveragedSource>
  </VRTRasterBand>
</VRTDataset>""")

    cs = vrt_ds.GetRasterBand(1).Checksum()
    assert cs == 4672

    cs = vrt2_ds.GetRasterBand(1).Checksum()
    assert cs == 4672


###############################################################################
# Test 2 level of VRT with shared="0"

def test_vrt_read_20():

    if test_cli_utilities.get_gdalinfo_path() is None:
        pytest.skip()

    shutil.copy('data/byte.tif', 'tmp')
    for i in range(3):
        open('tmp/byte1_%d.vrt' % (i + 1), 'wt').write("""<VRTDataset rasterXSize="20" rasterYSize="20">
    <VRTRasterBand dataType="Byte" band="1">
        <SimpleSource>
        <SourceFilename relativeToVRT="1">byte.tif</SourceFilename>
        <SourceBand>1</SourceBand>
        <SourceProperties RasterXSize="20" RasterYSize="20" DataType="Byte" BlockXSize="20" BlockYSize="20" />
        <SrcRect xOff="0" yOff="0" xSize="20" ySize="20" />
        <DstRect xOff="0" yOff="0" xSize="20" ySize="20" />
        </SimpleSource>
    </VRTRasterBand>
    </VRTDataset>""")
    open('tmp/byte2.vrt', 'wt').write("""<VRTDataset rasterXSize="20" rasterYSize="20">
  <VRTRasterBand dataType="Byte" band="1">
    <SimpleSource>
      <SourceFilename relativeToVRT="1">byte1_1.vrt</SourceFilename>
      <SourceBand>1</SourceBand>
      <SourceProperties RasterXSize="20" RasterYSize="20" DataType="Byte" BlockXSize="20" BlockYSize="20" />
      <SrcRect xOff="0" yOff="0" xSize="20" ySize="20" />
      <DstRect xOff="0" yOff="0" xSize="20" ySize="20" />
    </SimpleSource>
    <SimpleSource>
      <SourceFilename relativeToVRT="1">byte1_2.vrt</SourceFilename>
      <SourceBand>1</SourceBand>
      <SourceProperties RasterXSize="20" RasterYSize="20" DataType="Byte" BlockXSize="20" BlockYSize="20" />
      <SrcRect xOff="0" yOff="0" xSize="20" ySize="20" />
      <DstRect xOff="0" yOff="0" xSize="20" ySize="20" />
    </SimpleSource>
    <SimpleSource>
      <SourceFilename relativeToVRT="1">byte1_3.vrt</SourceFilename>
      <SourceBand>1</SourceBand>
      <SourceProperties RasterXSize="20" RasterYSize="20" DataType="Byte" BlockXSize="20" BlockYSize="20" />
      <SrcRect xOff="0" yOff="0" xSize="20" ySize="20" />
      <DstRect xOff="0" yOff="0" xSize="20" ySize="20" />
    </SimpleSource>
  </VRTRasterBand>
</VRTDataset>""")
    ret = gdaltest.runexternal(test_cli_utilities.get_gdalinfo_path() + ' -checksum tmp/byte2.vrt --config VRT_SHARED_SOURCE 0 --config GDAL_MAX_DATASET_POOL_SIZE 3')
    assert 'Checksum=4672' in ret

    for f in ['tmp/byte.tif', 'tmp/byte1_1.vrt', 'tmp/byte1_2.vrt', 'tmp/byte1_3.vrt', 'tmp/byte2.vrt']:
        os.unlink(f)


###############################################################################
# Test implicit virtual overviews


def test_vrt_read_21():

    ds = gdal.Open('data/byte.tif')
    data = ds.ReadRaster(0, 0, 20, 20, 400, 400)
    ds = None
    ds = gdal.GetDriverByName('GTiff').Create('/vsimem/byte.tif', 400, 400)
    ds.WriteRaster(0, 0, 400, 400, data)
    ds.BuildOverviews('NEAR', [2])
    ds = None

    gdal.FileFromMemBuffer('/vsimem/vrt_read_21.vrt', """<VRTDataset rasterXSize="800" rasterYSize="800">
  <VRTRasterBand dataType="Byte" band="1">
    <SimpleSource>
      <SourceFilename>/vsimem/byte.tif</SourceFilename>
      <SourceBand>1</SourceBand>
      <SourceProperties RasterXSize="400" RasterYSize="400" DataType="Byte" BlockXSize="400" BlockYSize="1" />
      <SrcRect xOff="100" yOff="100" xSize="200" ySize="250" />
      <DstRect xOff="300" yOff="400" xSize="200" ySize="250" />
    </SimpleSource>
  </VRTRasterBand>
</VRTDataset>""")
    ds = gdal.Open('/vsimem/vrt_read_21.vrt')
    assert ds.GetRasterBand(1).GetOverviewCount() == 1
    data_ds_one_band = ds.ReadRaster(0, 0, 800, 800, 400, 400)
    ds = None

    gdal.FileFromMemBuffer('/vsimem/vrt_read_21.vrt', """<VRTDataset rasterXSize="800" rasterYSize="800">
  <VRTRasterBand dataType="Byte" band="1">
    <SimpleSource>
      <SourceFilename>/vsimem/byte.tif</SourceFilename>
      <SourceBand>1</SourceBand>
      <SourceProperties RasterXSize="400" RasterYSize="400" DataType="Byte" BlockXSize="400" BlockYSize="1" />
      <SrcRect xOff="100" yOff="100" xSize="200" ySize="250" />
      <DstRect xOff="300" yOff="400" xSize="200" ySize="250" />
    </SimpleSource>
  </VRTRasterBand>
  <VRTRasterBand dataType="Byte" band="2">
    <ComplexSource>
      <SourceFilename>/vsimem/byte.tif</SourceFilename>
      <SourceBand>1</SourceBand>
      <SourceProperties RasterXSize="400" RasterYSize="400" DataType="Byte" BlockXSize="400" BlockYSize="1" />
      <SrcRect xOff="100" yOff="100" xSize="200" ySize="250" />
      <DstRect xOff="300" yOff="400" xSize="200" ySize="250" />
      <ScaleOffset>10</ScaleOffset>
    </ComplexSource>
  </VRTRasterBand>
</VRTDataset>""")
    ds = gdal.Open('/vsimem/vrt_read_21.vrt')
    assert ds.GetRasterBand(1).GetOverviewCount() == 1

    ds = gdal.Open('/vsimem/vrt_read_21.vrt')
    ovr_band = ds.GetRasterBand(1).GetOverview(-1)
    assert ovr_band is None
    ovr_band = ds.GetRasterBand(1).GetOverview(1)
    assert ovr_band is None
    ovr_band = ds.GetRasterBand(1).GetOverview(0)
    assert ovr_band is not None
    cs = ovr_band.Checksum()
    cs2 = ds.GetRasterBand(2).GetOverview(0).Checksum()

    data = ds.ReadRaster(0, 0, 800, 800, 400, 400)

    assert data == data_ds_one_band + ds.GetRasterBand(2).ReadRaster(0, 0, 800, 800, 400, 400)

    mem_ds = gdal.GetDriverByName('MEM').Create('', 400, 400, 2)
    mem_ds.WriteRaster(0, 0, 400, 400, data)
    ref_cs = mem_ds.GetRasterBand(1).Checksum()
    ref_cs2 = mem_ds.GetRasterBand(2).Checksum()
    mem_ds = None
    assert cs == ref_cs
    assert cs2 == ref_cs2

    ds.BuildOverviews('NEAR', [2])
    expected_cs = ds.GetRasterBand(1).GetOverview(0).Checksum()
    expected_cs2 = ds.GetRasterBand(2).GetOverview(0).Checksum()
    ds = None

    assert cs == expected_cs
    assert cs2 == expected_cs2

    gdal.Unlink('/vsimem/vrt_read_21.vrt')
    gdal.Unlink('/vsimem/vrt_read_21.vrt.ovr')
    gdal.Unlink('/vsimem/byte.tif')

###############################################################################
# Test that we honour NBITS with SimpleSource and ComplexSource


def test_vrt_read_22():

    ds = gdal.Open('data/byte.tif')
    data = ds.ReadRaster()
    ds = None
    ds = gdal.GetDriverByName('GTiff').Create('/vsimem/byte.tif', 20, 20)
    ds.WriteRaster(0, 0, 20, 20, data)
    ds.GetRasterBand(1).ComputeStatistics(False)
    ds = None

    ds = gdal.Open("""<VRTDataset rasterXSize="20" rasterYSize="20">
  <VRTRasterBand dataType="Byte" band="1">
    <Metadata domain="IMAGE_STRUCTURE">
        <MDI key="NBITS">6</MDI>
    </Metadata>
    <SimpleSource>
      <SourceFilename>/vsimem/byte.tif</SourceFilename>
      <SourceBand>1</SourceBand>
    </SimpleSource>
  </VRTRasterBand>
</VRTDataset>""")
    assert ds.GetRasterBand(1).GetMinimum() == 63

    assert ds.GetRasterBand(1).GetMaximum() == 63

    assert ds.GetRasterBand(1).ComputeRasterMinMax() == (63, 63)

    assert ds.GetRasterBand(1).ComputeStatistics(False) == [63.0, 63.0, 63.0, 0.0]

    data = ds.ReadRaster()
    got = struct.unpack('B' * 20 * 20, data)
    assert got[0] == 63

    ds = gdal.Open("""<VRTDataset rasterXSize="20" rasterYSize="20">
  <VRTRasterBand dataType="Byte" band="1">
    <Metadata domain="IMAGE_STRUCTURE">
        <MDI key="NBITS">6</MDI>
    </Metadata>
    <ComplexSource>
      <SourceFilename>/vsimem/byte.tif</SourceFilename>
      <SourceBand>1</SourceBand>
    </ComplexSource>
  </VRTRasterBand>
</VRTDataset>""")
    assert ds.GetRasterBand(1).GetMinimum() == 63

    assert ds.GetRasterBand(1).GetMaximum() == 63

    assert ds.GetRasterBand(1).ComputeRasterMinMax() == (63, 63)

    assert ds.GetRasterBand(1).ComputeStatistics(False) == [63.0, 63.0, 63.0, 0.0]

    ds = gdal.Open("""<VRTDataset rasterXSize="20" rasterYSize="20">
  <VRTRasterBand dataType="Byte" band="1">
    <Metadata domain="IMAGE_STRUCTURE">
        <MDI key="NBITS">6</MDI>
    </Metadata>
    <ComplexSource>
      <SourceFilename>/vsimem/byte.tif</SourceFilename>
      <SourceBand>1</SourceBand>
      <ScaleOffset>10</ScaleOffset>
    </ComplexSource>
  </VRTRasterBand>
</VRTDataset>""")
    assert ds.GetRasterBand(1).GetMinimum() is None

    assert ds.GetRasterBand(1).GetMaximum() is None

    assert ds.GetRasterBand(1).ComputeRasterMinMax() == (63, 63)

    assert ds.GetRasterBand(1).ComputeStatistics(False) == [63.0, 63.0, 63.0, 0.0]

    gdal.Unlink('/vsimem/byte.tif')
    gdal.Unlink('/vsimem/byte.tif.aux.xml')

###############################################################################
# Test non-nearest resampling on a VRT exposing a nodata value but with
# an underlying dataset without nodata


def test_vrt_read_23():

    try:
        import numpy
    except (ImportError, AttributeError):
        pytest.skip()

    mem_ds = gdal.GetDriverByName('GTiff').Create('/vsimem/vrt_read_23.tif', 2, 1)
    mem_ds.GetRasterBand(1).WriteArray(numpy.array([[0, 10]]))
    mem_ds = None
    ds = gdal.Open("""<VRTDataset rasterXSize="2" rasterYSize="1">
  <VRTRasterBand dataType="Byte" band="1">
    <NoDataValue>0</NoDataValue>
    <SimpleSource>
      <SourceFilename>/vsimem/vrt_read_23.tif</SourceFilename>
    </SimpleSource>
  </VRTRasterBand>
</VRTDataset>""")
    got_ar = ds.GetRasterBand(1).ReadAsArray(0, 0, 2, 1, 4, 1, resample_alg=gdal.GRIORA_Bilinear)
    assert list(got_ar[0]) == [0, 10, 10, 10]
    assert ds.ReadRaster(0, 0, 2, 1, 4, 1, resample_alg=gdal.GRIORA_Bilinear) == ds.GetRasterBand(1).ReadRaster(0, 0, 2, 1, 4, 1, resample_alg=gdal.GRIORA_Bilinear)
    ds = None

    gdal.Unlink('/vsimem/vrt_read_23.tif')

    # Same but with nodata set on source band too
    mem_ds = gdal.GetDriverByName('GTiff').Create('/vsimem/vrt_read_23.tif', 2, 1)
    mem_ds.GetRasterBand(1).SetNoDataValue(0)
    mem_ds.GetRasterBand(1).WriteArray(numpy.array([[0, 10]]))
    mem_ds = None
    ds = gdal.Open("""<VRTDataset rasterXSize="2" rasterYSize="1">
  <VRTRasterBand dataType="Byte" band="1">
    <NoDataValue>0</NoDataValue>
    <SimpleSource>
      <SourceFilename>/vsimem/vrt_read_23.tif</SourceFilename>
    </SimpleSource>
  </VRTRasterBand>
</VRTDataset>""")
    got_ar = ds.GetRasterBand(1).ReadAsArray(0, 0, 2, 1, 4, 1, resample_alg=gdal.GRIORA_Bilinear)
    assert list(got_ar[0]) == [0, 10, 10, 10]
    assert ds.ReadRaster(0, 0, 2, 1, 4, 1, resample_alg=gdal.GRIORA_Bilinear) == ds.GetRasterBand(1).ReadRaster(0, 0, 2, 1, 4, 1, resample_alg=gdal.GRIORA_Bilinear)
    ds = None

    gdal.Unlink('/vsimem/vrt_read_23.tif')

###############################################################################
# Test floating point rounding issues when the VRT does a zoom-in


def test_vrt_read_24():

    ds = gdal.Open('data/zoom_in.vrt')
    data = ds.ReadRaster(34, 5, 66, 87)
    ds = None

    ds = gdal.GetDriverByName('MEM').Create('', 66, 87)
    ds.WriteRaster(0, 0, 66, 87, data)
    cs = ds.GetRasterBand(1).Checksum()
    ds = None

    # Please do not change the expected checksum without checking that
    # the result image has no vertical black line in the middle
    assert cs == 46612
    ds = None

###############################################################################
# Test GetDataCoverageStatus()


def test_vrt_read_25():

    import ogrtest
    if not ogrtest.have_geos():
        pytest.skip()

    ds = gdal.Open("""<VRTDataset rasterXSize="2000" rasterYSize="200">
  <VRTRasterBand dataType="Byte" band="1">
    <SimpleSource>
      <SourceFilename relativeToVRT="0">data/byte.tif</SourceFilename>
      <SourceBand>1</SourceBand>
      <SrcRect xOff="0" yOff="0" xSize="20" ySize="20" />
      <DstRect xOff="0" yOff="0" xSize="20" ySize="20" />
    </SimpleSource>
    <SimpleSource>
      <SourceFilename relativeToVRT="0">data/byte.tif</SourceFilename>
      <SourceBand>1</SourceBand>
      <SrcRect xOff="0" yOff="0" xSize="20" ySize="20" />
      <DstRect xOff="1000" yOff="30" xSize="10" ySize="20" />
    </SimpleSource>
    <SimpleSource>
      <SourceFilename relativeToVRT="0">data/byte.tif</SourceFilename>
      <SourceBand>1</SourceBand>
      <SrcRect xOff="0" yOff="0" xSize="20" ySize="20" />
      <DstRect xOff="1010" yOff="30" xSize="10" ySize="20" />
    </SimpleSource>
  </VRTRasterBand>
</VRTDataset>""")

    (flags, pct) = ds.GetRasterBand(1).GetDataCoverageStatus(0, 0, 20, 20)
    assert flags == gdal.GDAL_DATA_COVERAGE_STATUS_DATA and pct == 100.0

    (flags, pct) = ds.GetRasterBand(1).GetDataCoverageStatus(1005, 35, 10, 10)
    assert flags == gdal.GDAL_DATA_COVERAGE_STATUS_DATA and pct == 100.0

    (flags, pct) = ds.GetRasterBand(1).GetDataCoverageStatus(100, 100, 20, 20)
    assert flags == gdal.GDAL_DATA_COVERAGE_STATUS_EMPTY and pct == 0.0

    (flags, pct) = ds.GetRasterBand(1).GetDataCoverageStatus(10, 10, 20, 20)
    assert flags == gdal.GDAL_DATA_COVERAGE_STATUS_DATA | gdal.GDAL_DATA_COVERAGE_STATUS_EMPTY and pct == 25.0


###############################################################################
# Test consistency of RasterIO() with resampling, that is extracting different
# sub-windows give consistent results

def test_vrt_read_26():

    vrt_ds = gdal.Open("""<VRTDataset rasterXSize="22" rasterYSize="22">
  <VRTRasterBand dataType="Byte" band="1">
    <SimpleSource>
      <SourceFilename relativeToVRT="0">data/byte.tif</SourceFilename>
      <SourceBand>1</SourceBand>
      <SourceProperties RasterXSize="20" RasterYSize="20" DataType="Byte" BlockXSize="20" BlockYSize="20" />
      <SrcRect xOff="0" yOff="0" xSize="20" ySize="20" />
      <DstRect xOff="0" yOff="0" xSize="22" ySize="22" />
    </SimpleSource>
  </VRTRasterBand>
</VRTDataset>""")

    full_data = vrt_ds.GetRasterBand(1).ReadRaster(0, 0, 22, 22)
    full_data = struct.unpack('B' * 22 * 22, full_data)

    partial_data = vrt_ds.GetRasterBand(1).ReadRaster(1, 1, 1, 1)
    partial_data = struct.unpack('B' * 1 * 1, partial_data)

    assert partial_data[0] == full_data[22 + 1]

###############################################################################
# Test fix for https://bugs.chromium.org/p/oss-fuzz/issues/detail?id=1553


def test_vrt_read_27():

    gdal.Open('data/empty_gcplist.vrt')

###############################################################################
# Test fix for https://bugs.chromium.org/p/oss-fuzz/issues/detail?id=1551


def test_vrt_read_28():

    with gdaltest.error_handler():
        ds = gdal.Open('<VRTDataset rasterXSize="1 "rasterYSize="1"><VRTRasterBand band="-2147483648"><SimpleSource></SimpleSource></VRTRasterBand></VRTDataset>')
    assert ds is None


###############################################################################
# Check VRT source sharing and non-sharing situations (#6939)

def test_vrt_read_29():

    f = open('data/byte.tif')
    lst_before = sorted(gdaltest.get_opened_files())
    if not lst_before:
        pytest.skip()
    f.close()

    gdal.Translate('tmp/vrt_read_29.tif', 'data/byte.tif')

    vrt_text = """<VRTDataset rasterXSize="20" rasterYSize="20">
    <VRTRasterBand dataType="Byte" band="1">
        <SimpleSource>
        <SourceFilename>tmp/vrt_read_29.tif</SourceFilename>
        <SourceBand>1</SourceBand>
        <SourceProperties RasterXSize="20" RasterYSize="20" DataType="Byte" BlockXSize="20" BlockYSize="20" />
        <SrcRect xOff="0" yOff="0" xSize="20" ySize="20" />
        <DstRect xOff="0" yOff="0" xSize="20" ySize="20" />
        </SimpleSource>
    </VRTRasterBand>
    <VRTRasterBand dataType="Byte" band="2">
        <SimpleSource>
        <SourceFilename>tmp/vrt_read_29.tif</SourceFilename>
        <SourceBand>1</SourceBand>
        <SourceProperties RasterXSize="20" RasterYSize="20" DataType="Byte" BlockXSize="20" BlockYSize="20" />
        <SrcRect xOff="0" yOff="0" xSize="20" ySize="20" />
        <DstRect xOff="0" yOff="0" xSize="20" ySize="20" />
        </SimpleSource>
    </VRTRasterBand>
    </VRTDataset>"""

    lst_before = sorted(gdaltest.get_opened_files())
    ds = gdal.Open(vrt_text)
    # Just after opening, we shouldn't have read the source
    lst = sorted(gdaltest.get_opened_files())
    assert lst == lst_before

    # Check that the 2 bands share the same source handle
    ds.GetRasterBand(1).Checksum()
    lst = sorted(gdaltest.get_opened_files())
    assert len(lst) == len(lst_before) + 1
    ds.GetRasterBand(2).Checksum()
    lst = sorted(gdaltest.get_opened_files())
    assert len(lst) == len(lst_before) + 1

    # Open a second VRT dataset handle
    ds2 = gdal.Open(vrt_text)

    # Check that it consumes an extra handle (don't share sources between
    # different VRT)
    ds2.GetRasterBand(1).Checksum()
    lst = sorted(gdaltest.get_opened_files())
    assert len(lst) == len(lst_before) + 2

    # Close first VRT dataset, and check that the handle it took on the TIFF
    # is released (https://github.com/OSGeo/gdal/issues/3253)
    ds = None
    lst = sorted(gdaltest.get_opened_files())
    assert len(lst) == len(lst_before) + 1

    gdal.Unlink('tmp/vrt_read_29.tif')

###############################################################################
# Check VRT reading with DatasetRasterIO


def test_vrt_read_30():

    ds = gdal.Open("""<VRTDataset rasterXSize="2" rasterYSize="2">
  <VRTRasterBand dataType="Byte" band="1">
  </VRTRasterBand>
  <VRTRasterBand dataType="Byte" band="2">
  </VRTRasterBand>
  <VRTRasterBand dataType="Byte" band="3">
  </VRTRasterBand>
</VRTDataset>""")

    data = ds.ReadRaster(0, 0, 2, 2, 2, 2, buf_pixel_space=3, buf_line_space=2 * 3, buf_band_space=1)
    got = struct.unpack('B' * 2 * 2 * 3, data)
    for i in range(2 * 2 * 3):
        assert got[i] == 0
    ds = None

###############################################################################
# Check that we take into account intermediate data type demotion


def test_vrt_read_31():

    gdal.FileFromMemBuffer('/vsimem/in.asc',
                           """ncols        2
nrows        2
xllcorner    0
yllcorner    0
dx           1
dy           1
-255         1
254          256""")

    ds = gdal.Translate('', '/vsimem/in.asc', outputType=gdal.GDT_Byte, format='VRT')

    data = ds.GetRasterBand(1).ReadRaster(0, 0, 2, 2, buf_type=gdal.GDT_Float32)
    got = struct.unpack('f' * 2 * 2, data)
    assert got == (0, 1, 254, 255)

    data = ds.ReadRaster(0, 0, 2, 2, buf_type=gdal.GDT_Float32)
    got = struct.unpack('f' * 2 * 2, data)
    assert got == (0, 1, 254, 255)

    ds = None

    gdal.Unlink('/vsimem/in.asc')


###############################################################################
# Test reading a VRT where the NODATA & NoDataValue are slightly below the
# minimum float value (https://github.com/OSGeo/gdal/issues/1071)

def test_vrt_float32_with_nodata_slightly_below_float_min():

    shutil.copyfile('data/minfloat.tif', 'tmp/minfloat.tif')
    shutil.copyfile('data/minfloat_nodata_slightly_out_of_float.vrt',
                    'tmp/minfloat_nodata_slightly_out_of_float.vrt')
    gdal.Unlink('tmp/minfloat_nodata_slightly_out_of_float.vrt.aux.xml')

    ds = gdal.Open('tmp/minfloat_nodata_slightly_out_of_float.vrt')
    nodata = ds.GetRasterBand(1).GetNoDataValue()
    stats = ds.GetRasterBand(1).ComputeStatistics(False)
    ds = None

    vrt_content = open('tmp/minfloat_nodata_slightly_out_of_float.vrt', 'rt').read()

    gdal.Unlink('tmp/minfloat.tif')
    gdal.Unlink('tmp/minfloat_nodata_slightly_out_of_float.vrt')

    # Check that the values were 'normalized' when regenerating the VRT
    assert '-3.402823466385289' not in vrt_content, \
        'did not get expected nodata in rewritten VRT'

    if nodata != -3.4028234663852886e+38:
        print("%.18g" % nodata)
        pytest.fail('did not get expected nodata')

    assert stats == [-3.0, 5.0, 1.0, 4.0], 'did not get expected stats'


###############################################################################
# Fix issue raised in https://lists.osgeo.org/pipermail/gdal-dev/2018-December/049415.html

def test_vrt_subpixel_offset():

    ds = gdal.Open('data/vrt_subpixel_offset.vrt')
    cs = ds.GetRasterBand(1).Checksum()
    assert cs == 4849


###############################################################################
# Check bug fix of bug fix of
# https://lists.osgeo.org/pipermail/gdal-dev/2018-December/049415.html

def test_vrt_dstsize_larger_than_source():

    ds = gdal.Open('data/dstsize_larger_than_source.vrt')
    cs = ds.GetRasterBand(1).Checksum()
    assert cs == 33273


def test_vrt_invalid_srcrect():

    vrt_text = """<VRTDataset rasterXSize="20" rasterYSize="20">
    <VRTRasterBand dataType="Byte" band="1">
        <SimpleSource>
        <SourceFilename relative="1">data/byte.tif</SourceFilename>
        <SourceBand>1</SourceBand>
        <SourceProperties RasterXSize="20" RasterYSize="20" DataType="Byte" BlockXSize="20" BlockYSize="20" />
        <SrcRect xOff="0" yOff="0" xSize="-10" ySize="20" />
        <DstRect xOff="0" yOff="0" xSize="20" ySize="20" />
        </SimpleSource>
    </VRTRasterBand>
    </VRTDataset>"""
    assert gdal.Open(vrt_text) is None


def test_vrt_invalid_dstrect():

    vrt_text = """<VRTDataset rasterXSize="20" rasterYSize="20">
    <VRTRasterBand dataType="Byte" band="1">
        <SimpleSource>
        <SourceFilename relative="1">data/byte.tif</SourceFilename>
        <SourceBand>1</SourceBand>
        <SourceProperties RasterXSize="20" RasterYSize="20" DataType="Byte" BlockXSize="20" BlockYSize="20" />
        <SrcRect xOff="0" yOff="0" xSize="20" ySize="20" />
        <DstRect xOff="0" yOff="0" xSize="20" ySize="1e400" />
        </SimpleSource>
    </VRTRasterBand>
    </VRTDataset>"""
    assert gdal.Open(vrt_text) is None


def test_vrt_no_explicit_dataAxisToSRSAxisMapping():

    vrt_text = """<VRTDataset rasterXSize="20" rasterYSize="20">
    <SRS>GEOGCS["WGS 84",DATUM["WGS_1984",SPHEROID["WGS 84",6378137,298.257223563,AUTHORITY["EPSG","7030"]],AUTHORITY["EPSG","6326"]],PRIMEM["Greenwich",0,AUTHORITY["EPSG","8901"]],UNIT["degree",0.0174532925199433,AUTHORITY["EPSG","9122"]],AXIS["Latitude",NORTH],AXIS["Longitude",EAST],AUTHORITY["EPSG","4326"]]</SRS>
    <VRTRasterBand dataType="Byte" band="1">
        <SimpleSource>
        <SourceFilename relative="1">data/byte.tif</SourceFilename>
        <SourceBand>1</SourceBand>
        <SourceProperties RasterXSize="20" RasterYSize="20" DataType="Byte" BlockXSize="20" BlockYSize="20" />
        <SrcRect xOff="0" yOff="0" xSize="20" ySize="20" />
        <DstRect xOff="0" yOff="0" xSize="20" ySize="20" />
        </SimpleSource>
    </VRTRasterBand>
    </VRTDataset>"""
    ds = gdal.Open(vrt_text)
    assert ds.GetSpatialRef().GetDataAxisToSRSAxisMapping() == [2,1]
    ds = None


def test_vrt_explicit_dataAxisToSRSAxisMapping_1_2():

    vrt_text = """<VRTDataset rasterXSize="20" rasterYSize="20">
    <SRS dataAxisToSRSAxisMapping="1,2">GEOGCS["WGS 84",DATUM["WGS_1984",SPHEROID["WGS 84",6378137,298.257223563,AUTHORITY["EPSG","7030"]],AUTHORITY["EPSG","6326"]],PRIMEM["Greenwich",0,AUTHORITY["EPSG","8901"]],UNIT["degree",0.0174532925199433,AUTHORITY["EPSG","9122"]],AXIS["Latitude",NORTH],AXIS["Longitude",EAST],AUTHORITY["EPSG","4326"]]</SRS>
    <VRTRasterBand dataType="Byte" band="1">
        <SimpleSource>
        <SourceFilename relative="1">data/byte.tif</SourceFilename>
        <SourceBand>1</SourceBand>
        <SourceProperties RasterXSize="20" RasterYSize="20" DataType="Byte" BlockXSize="20" BlockYSize="20" />
        <SrcRect xOff="0" yOff="0" xSize="20" ySize="20" />
        <DstRect xOff="0" yOff="0" xSize="20" ySize="20" />
        </SimpleSource>
    </VRTRasterBand>
    </VRTDataset>"""
    ds = gdal.Open(vrt_text)
    assert ds.GetSpatialRef().GetDataAxisToSRSAxisMapping() == [1,2]
    ds = None


def test_vrt_shared_no_proxy_pool():

    before = gdaltest.get_opened_files()
    vrt_text = """<VRTDataset rasterXSize="50" rasterYSize="50">
  <VRTRasterBand dataType="Byte" band="1">
    <ColorInterp>Red</ColorInterp>
    <SimpleSource>
      <SourceFilename>data/rgbsmall.tif</SourceFilename>
      <SourceBand>1</SourceBand>
    </SimpleSource>
  </VRTRasterBand>
  <VRTRasterBand dataType="Byte" band="2">
    <ColorInterp>Green</ColorInterp>
    <SimpleSource>
      <SourceFilename>data/rgbsmall.tif</SourceFilename>
      <SourceBand>2</SourceBand>
    </SimpleSource>
  </VRTRasterBand>
  <VRTRasterBand dataType="Byte" band="3">
    <ColorInterp>Blue</ColorInterp>
    <SimpleSource>
      <SourceFilename>data/rgbsmall.tif</SourceFilename>
      <SourceBand>3</SourceBand>
    </SimpleSource>
  </VRTRasterBand>
</VRTDataset>"""
    ds = gdal.Open(vrt_text)
    assert ds
    assert ds.GetRasterBand(1).Checksum() == 21212
    assert ds.GetRasterBand(2).Checksum() == 21053
    assert ds.GetRasterBand(3).Checksum() == 21349
    ds = None

    after = gdaltest.get_opened_files()

    if len(before) != len(after) and (gdaltest.is_travis_branch('trusty_clang') or gdaltest.is_travis_branch('trusty_32bit') or gdaltest.is_travis_branch('ubuntu_1604')):
        pytest.xfail('Mysterious failure')

    assert len(before) == len(after)


def test_vrt_shared_no_proxy_pool_error():

    vrt_text = """<VRTDataset rasterXSize="50" rasterYSize="50">
  <VRTRasterBand dataType="Byte" band="1">
    <SimpleSource>
      <SourceFilename>data/byte.tif</SourceFilename>
      <SourceBand>10</SourceBand>
    </SimpleSource>
  </VRTRasterBand>
  <VRTRasterBand dataType="Byte" band="2">
    <SimpleSource>
      <SourceFilename>data/byte.tif</SourceFilename>
      <SourceBand>11</SourceBand>
    </SimpleSource>
  </VRTRasterBand>
</VRTDataset>"""
    with gdaltest.error_handler():
        ds = gdal.Open(vrt_text)
    assert not ds


def test_vrt_protocol():

    with gdaltest.error_handler():
        assert not gdal.Open('vrt://')
        assert not gdal.Open('vrt://i_do_not_exist')
        assert not gdal.Open('vrt://i_do_not_exist?')

    ds = gdal.Open('vrt://data/byte.tif')
    assert ds.RasterCount == 1
    assert ds.GetRasterBand(1).Checksum() == 4672

    with gdaltest.error_handler():
        assert not gdal.Open('vrt://data/byte.tif?foo=bar')
        assert not gdal.Open('vrt://data/byte.tif?bands=foo')
        assert not gdal.Open('vrt://data/byte.tif?bands=0')
        assert not gdal.Open('vrt://data/byte.tif?bands=2')

    ds = gdal.Open('vrt://data/byte.tif?bands=1,mask,1')
    assert ds.RasterCount == 3
    assert ds.GetRasterBand(1).Checksum() == 4672
    assert ds.GetRasterBand(2).Checksum() == 4873
    assert ds.GetRasterBand(3).Checksum() == 4672


def test_vrt_source_no_dstrect():

    vrt_text = """<VRTDataset rasterXSize="20" rasterYSize="20">
  <VRTRasterBand dataType="Byte" band="1">
    <SimpleSource>
      <SourceFilename relativeToVRT="0">data/byte.tif</SourceFilename>
    </SimpleSource>
  </VRTRasterBand>
</VRTDataset>
"""
    filename = '/vsimem/out.tif'
    ds = gdal.Translate(filename, vrt_text)
    assert ds.GetRasterBand(1).Checksum() == 4672
    ds = None
    gdal.Unlink(filename)


def test_vrt_dataset_rasterio_recursion_detection():

    gdal.FileFromMemBuffer('/vsimem/test.vrt', """<VRTDataset rasterXSize="20" rasterYSize="20">
  <VRTRasterBand dataType="Byte" band="1">
    <SimpleSource>
      <SourceFilename relativeToVRT="0">data/byte.tif</SourceFilename>
      <SourceBand>1</SourceBand>
      <SourceProperties RasterXSize="20" RasterYSize="20" DataType="Byte" BlockXSize="20" BlockYSize="20" />
      <SrcRect xOff="0" yOff="0" xSize="20" ySize="20" />
      <DstRect xOff="0" yOff="0" xSize="20" ySize="20" />
    </SimpleSource>
    <Overview>
        <SourceFilename relativeToVRT="0">/vsimem/test.vrt</SourceFilename>
        <SourceBand>1</SourceBand>
    </Overview>
  </VRTRasterBand>
</VRTDataset>""")

    ds = gdal.Open('/vsimem/test.vrt')
    with gdaltest.error_handler():
        ds.ReadRaster(0,0,20,20,10,10)
    gdal.Unlink('/vsimem/test.vrt')

def test_vrt_dataset_rasterio_recursion_detection_does_not_trigger():

    vrt_text = """<VRTDataset rasterXSize="50" rasterYSize="50">
  <VRTRasterBand dataType="Byte" band="1">
    <ColorInterp>Red</ColorInterp>
    <ComplexSource>
      <SourceFilename>data/rgbsmall.tif</SourceFilename>
      <SourceBand>1</SourceBand>
    </ComplexSource>
  </VRTRasterBand>
  <VRTRasterBand dataType="Byte" band="2">
    <ColorInterp>Green</ColorInterp>
    <ComplexSource>
      <SourceFilename>data/rgbsmall.tif</SourceFilename>
      <SourceBand>2</SourceBand>
    </ComplexSource>
  </VRTRasterBand>
  <VRTRasterBand dataType="Byte" band="3">
    <ColorInterp>Blue</ColorInterp>
    <ComplexSource>
      <SourceFilename>data/rgbsmall.tif</SourceFilename>
      <SourceBand>3</SourceBand>
    </ComplexSource>
  </VRTRasterBand>
</VRTDataset>"""
    ds = gdal.Open(vrt_text)
    got_data = ds.ReadRaster(0,0,50,50,25,25,resample_alg=gdal.GRIORA_Cubic)
    ds = gdal.Open('data/rgbsmall.tif')
    ref_data = ds.ReadRaster(0,0,50,50,25,25,resample_alg=gdal.GRIORA_Cubic)
    assert got_data == ref_data


def test_vrt_dataset_rasterio_non_nearest_resampling_source_with_ovr():

    ds = gdal.GetDriverByName('GTiff').Create('/vsimem/src.tif', 10, 10, 3)
    ds.GetRasterBand(1).Fill(255)
    ds.BuildOverviews('NONE', [2])
    ds.GetRasterBand(1).GetOverview(0).Fill(10)
    ds = None

    vrt_text = """<VRTDataset rasterXSize="10" rasterYSize="10">
  <VRTRasterBand dataType="Byte" band="1">
    <ColorInterp>Red</ColorInterp>
    <!-- two sources to avoid virtual overview to be created on the VRTRasterBand -->
    <ComplexSource>
      <SourceFilename>/vsimem/src.tif</SourceFilename>
      <SourceBand>1</SourceBand>
      <SrcRect xOff="0" yOff="0" xSize="10" ySize="5" />
      <DstRect xOff="0" yOff="0" xSize="10" ySize="5" />
    </ComplexSource>
    <ComplexSource>
      <SourceFilename>/vsimem/src.tif</SourceFilename>
      <SourceBand>1</SourceBand>
      <SrcRect xOff="0" yOff="5" xSize="10" ySize="5" />
      <DstRect xOff="0" yOff="5" xSize="10" ySize="5" />
    </ComplexSource>
  </VRTRasterBand>
  <VRTRasterBand dataType="Byte" band="2">
    <ColorInterp>Green</ColorInterp>
    <ComplexSource>
      <SourceFilename>/vsimem/src.tif</SourceFilename>
      <SourceBand>2</SourceBand>
    </ComplexSource>
  </VRTRasterBand>
  <VRTRasterBand dataType="Byte" band="3">
    <ColorInterp>Blue</ColorInterp>
    <ComplexSource>
      <SourceFilename>/vsimem/src.tif</SourceFilename>
      <SourceBand>3</SourceBand>
    </ComplexSource>
  </VRTRasterBand>
</VRTDataset>"""
    ds = gdal.Open(vrt_text)

    got_data = ds.ReadRaster(0,0,10,10,4,4)
    got_data = struct.unpack('B' * 4 * 4 * 3, got_data)
    assert got_data[0] == 10

    got_data = ds.ReadRaster(0,0,10,10,4,4,resample_alg=gdal.GRIORA_Cubic)
    got_data = struct.unpack('B' * 4 * 4 * 3, got_data)
    assert got_data[0] == 10

    gdal.Unlink('/vsimem/src.tif')


def test_vrt_implicit_ovr_with_hidenodatavalue():

    ds = gdal.GetDriverByName('GTiff').Create('/vsimem/src.tif', 256, 256, 3)
    ds.GetRasterBand(1).Fill(255)
    ds.BuildOverviews('NONE', [2])
    ds.GetRasterBand(1).GetOverview(0).Fill(10)
    ds = None

    vrt_text = """<VRTDataset rasterXSize="256" rasterYSize="256">
  <VRTRasterBand dataType="Byte" band="1">
    <ColorInterp>Red</ColorInterp>
    <NoDataValue>5</NoDataValue>
    <HideNoDataValue>1</HideNoDataValue>
    <ComplexSource>
      <SourceFilename>/vsimem/src.tif</SourceFilename>
      <SourceBand>1</SourceBand>
      <SrcRect xOff="0" yOff="0" xSize="128" ySize="128" />
      <DstRect xOff="128" yOff="128" xSize="128" ySize="128" />
    </ComplexSource>
  </VRTRasterBand>
  <VRTRasterBand dataType="Byte" band="2">
    <ColorInterp>Green</ColorInterp>
    <NoDataValue>5</NoDataValue>
    <HideNoDataValue>1</HideNoDataValue>
    <ComplexSource>
      <SourceFilename>/vsimem/src.tif</SourceFilename>
      <SourceBand>2</SourceBand>
      <SrcRect xOff="0" yOff="0" xSize="128" ySize="128" />
      <DstRect xOff="128" yOff="128" xSize="128" ySize="128" />
    </ComplexSource>
  </VRTRasterBand>
  <VRTRasterBand dataType="Byte" band="3">
    <ColorInterp>Blue</ColorInterp>
    <NoDataValue>5</NoDataValue>
    <HideNoDataValue>1</HideNoDataValue>
    <ComplexSource>
      <SourceFilename>/vsimem/src.tif</SourceFilename>
      <SourceBand>3</SourceBand>
      <SrcRect xOff="0" yOff="0" xSize="128" ySize="128" />
      <DstRect xOff="128" yOff="128" xSize="128" ySize="128" />
    </ComplexSource>
  </VRTRasterBand>
</VRTDataset>"""
    ds = gdal.Open(vrt_text)
    assert ds.GetRasterBand(1).GetOverviewCount() == 1

    got_data = ds.ReadRaster(0,0,256,256,64,64)
    got_data = struct.unpack('B' * 64 * 64 * 3, got_data)
    assert got_data[0] == 5
    assert got_data[32*64+32] == 10

    got_data = ds.GetRasterBand(1).ReadRaster(0,0,256,256,64,64)
    got_data = struct.unpack('B' * 64 * 64, got_data)
    assert got_data[0] == 5
    assert got_data[32*64+32] == 10

    gdal.Unlink('/vsimem/src.tif')


def test_vrt_usemaskband():

    ds = gdal.GetDriverByName('GTiff').Create('/vsimem/src1.tif', 3, 1)
    ds.GetRasterBand(1).Fill(255)
    ds.CreateMaskBand(0)
    ds.GetRasterBand(1).GetMaskBand().WriteRaster(0, 0, 1, 1, b'\xff')
    ds = None

    ds = gdal.GetDriverByName('GTiff').Create('/vsimem/src2.tif', 3, 1)
    ds.GetRasterBand(1).Fill(127)
    ds.CreateMaskBand(0)
    ds.GetRasterBand(1).GetMaskBand().WriteRaster(1, 0, 1, 1, b'\xff')
    ds = None

    vrt_text = """<VRTDataset rasterXSize="3" rasterYSize="1">
  <VRTRasterBand dataType="Byte" band="1">
    <ComplexSource>
      <SourceFilename>/vsimem/src1.tif</SourceFilename>
      <SourceBand>1</SourceBand>
      <SrcRect xOff="0" yOff="0" xSize="3" ySize="1" />
      <DstRect xOff="0" yOff="0" xSize="3" ySize="1" />
      <UseMaskBand>true</UseMaskBand>
    </ComplexSource>
    <ComplexSource>
      <SourceFilename>/vsimem/src2.tif</SourceFilename>
      <SourceBand>1</SourceBand>
      <SrcRect xOff="0" yOff="0" xSize="3" ySize="1" />
      <DstRect xOff="0" yOff="0" xSize="3" ySize="1" />
      <UseMaskBand>true</UseMaskBand>
    </ComplexSource>
  </VRTRasterBand>
  <MaskBand>
    <VRTRasterBand dataType="Byte">
        <ComplexSource>
            <SourceFilename>/vsimem/src1.tif</SourceFilename>
            <SourceBand>mask,1</SourceBand>
            <SrcRect xOff="0" yOff="0" xSize="3" ySize="1" />
            <DstRect xOff="0" yOff="0" xSize="3" ySize="1" />
            <UseMaskBand>true</UseMaskBand>
        </ComplexSource>
        <ComplexSource>
            <SourceFilename>/vsimem/src2.tif</SourceFilename>
            <SourceBand>mask,1</SourceBand>
            <SrcRect xOff="0" yOff="0" xSize="3" ySize="1" />
            <DstRect xOff="0" yOff="0" xSize="3" ySize="1" />
            <UseMaskBand>true</UseMaskBand>
        </ComplexSource>
    </VRTRasterBand>
  </MaskBand>
</VRTDataset>"""
    ds = gdal.Open(vrt_text)
    assert struct.unpack('B' * 3, ds.ReadRaster()) == (255, 127, 0)
    assert struct.unpack('B' * 3, ds.GetRasterBand(1).GetMaskBand().ReadRaster()) == (255, 255, 0)

    gdal.GetDriverByName('GTiff').Delete('/vsimem/src1.tif')
    gdal.GetDriverByName('GTiff').Delete('/vsimem/src2.tif')


def test_vrt_usemaskband_alpha():

    ds = gdal.GetDriverByName('GTiff').Create('/vsimem/src1.tif', 3, 1, 2)
    ds.GetRasterBand(1).Fill(255)
    ds.GetRasterBand(1).GetMaskBand().WriteRaster(0, 0, 1, 1, b'\xff')
    ds.GetRasterBand(2).SetColorInterpretation(gdal.GCI_AlphaBand)
    ds.GetRasterBand(2).WriteRaster(0, 0, 1, 1, b'\xff')

    ds = gdal.GetDriverByName('GTiff').Create('/vsimem/src2.tif', 3, 1, 2)
    ds.GetRasterBand(1).Fill(127)
    ds.GetRasterBand(2).SetColorInterpretation(gdal.GCI_AlphaBand)
    ds.GetRasterBand(2).WriteRaster(1, 0, 1, 1, b'\xff')
    ds = None

    vrt_text = """<VRTDataset rasterXSize="3" rasterYSize="1">
  <VRTRasterBand dataType="Byte" band="1">
    <ComplexSource>
      <SourceFilename>/vsimem/src1.tif</SourceFilename>
      <SourceBand>1</SourceBand>
      <SrcRect xOff="0" yOff="0" xSize="3" ySize="1" />
      <DstRect xOff="0" yOff="0" xSize="3" ySize="1" />
      <UseMaskBand>true</UseMaskBand>
    </ComplexSource>
    <ComplexSource>
      <SourceFilename>/vsimem/src2.tif</SourceFilename>
      <SourceBand>1</SourceBand>
      <SrcRect xOff="0" yOff="0" xSize="3" ySize="1" />
      <DstRect xOff="0" yOff="0" xSize="3" ySize="1" />
      <UseMaskBand>true</UseMaskBand>
    </ComplexSource>
  </VRTRasterBand>
  <VRTRasterBand dataType="Byte" band="2">
    <ColorInterp>Alpha</ColorInterp>
    <ComplexSource>
        <SourceFilename>/vsimem/src1.tif</SourceFilename>
        <SourceBand>2</SourceBand>
        <SrcRect xOff="0" yOff="0" xSize="3" ySize="1" />
        <DstRect xOff="0" yOff="0" xSize="3" ySize="1" />
        <UseMaskBand>true</UseMaskBand>
    </ComplexSource>
    <ComplexSource>
        <SourceFilename>/vsimem/src2.tif</SourceFilename>
        <SourceBand>2</SourceBand>
        <SrcRect xOff="0" yOff="0" xSize="3" ySize="1" />
        <DstRect xOff="0" yOff="0" xSize="3" ySize="1" />
        <UseMaskBand>true</UseMaskBand>
    </ComplexSource>
  </VRTRasterBand>
</VRTDataset>"""
    ds = gdal.Open(vrt_text)
    assert struct.unpack('B' * 3, ds.GetRasterBand(1).ReadRaster()) == (255, 127, 0)
    assert struct.unpack('B' * 3, ds.GetRasterBand(2).ReadRaster()) == (255, 255, 0)

    gdal.GetDriverByName('GTiff').Delete('/vsimem/src1.tif')
    gdal.GetDriverByName('GTiff').Delete('/vsimem/src2.tif')
