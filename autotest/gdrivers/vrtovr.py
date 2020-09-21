#!/usr/bin/env pytest
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test overviews support in VRT driver
# Author:   Even Rouault <even dot rouault at spatialys.com>
#
###############################################################################
# Copyright (c) 2010, 2020, Even Rouault <even dot rouault at spatialys.com>
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

from osgeo import gdal
import struct

import gdaltest

###############################################################################
# Simple test


def test_vrtovr_1():

    vrt_string = """<VRTDataset rasterXSize="20" rasterYSize="20">
  <VRTRasterBand dataType="Byte" band="1">
    <ColorInterp>Gray</ColorInterp>
    <SimpleSource>
      <SourceFilename relativeToVRT="0">data/byte.tif</SourceFilename>
      <SourceBand>1</SourceBand>
      <SourceProperties RasterXSize="20" RasterYSize="20" DataType="Byte" BlockXSize="20" BlockYSize="20" />
      <SrcRect xOff="0" yOff="0" xSize="20" ySize="20" />
      <DstRect xOff="0" yOff="0" xSize="20" ySize="20" />
    </SimpleSource>
    <Overview>
      <SourceFilename relativeToVRT="0">data/int16.tif</SourceFilename>
      <SourceBand>1</SourceBand>
    </Overview>
  </VRTRasterBand>
</VRTDataset>"""

    ds = gdal.Open(vrt_string)
    assert ds.GetRasterBand(1).GetOverviewCount() == 1, \
        'did not get expected overview count'

    cs = ds.GetRasterBand(1).GetOverview(0).Checksum()
    assert cs == 4672, 'did not get expected overview checksum'

    fl = ds.GetFileList()
    assert fl == ['data/byte.tif', 'data/int16.tif'], 'did not get expected file list'

    ds = None

###############################################################################
# Test serialization


def test_vrtovr_2():

    vrt_string = """<VRTDataset rasterXSize="20" rasterYSize="20">
  <VRTRasterBand dataType="Byte" band="1">
    <ColorInterp>Gray</ColorInterp>
    <SimpleSource>
      <SourceFilename relativeToVRT="0">data/byte.tif</SourceFilename>
      <SourceBand>1</SourceBand>
      <SourceProperties RasterXSize="20" RasterYSize="20" DataType="Byte" BlockXSize="20" BlockYSize="20" />
      <SrcRect xOff="0" yOff="0" xSize="20" ySize="20" />
      <DstRect xOff="0" yOff="0" xSize="20" ySize="20" />
    </SimpleSource>
    <Overview>
      <SourceFilename relativeToVRT="0">data/byte.tif</SourceFilename>
      <SourceBand>1</SourceBand>
    </Overview>
  </VRTRasterBand>
</VRTDataset>"""

    f = gdal.VSIFOpenL("/vsimem/vrtovr_2.vrt", "wb")
    gdal.VSIFWriteL(vrt_string, len(vrt_string), 1, f)
    gdal.VSIFCloseL(f)

    ds = gdal.Open("/vsimem/vrtovr_2.vrt", gdal.GA_Update)
    ds.GetRasterBand(1).SetDescription('foo')
    ds = None

    ds = gdal.Open("/vsimem/vrtovr_2.vrt")
    assert ds.GetRasterBand(1).GetOverviewCount() == 1, \
        'did not get expected overview count'

    cs = ds.GetRasterBand(1).GetOverview(0).Checksum()
    assert cs == 4672, 'did not get expected overview checksum'

    ds = None

    gdal.Unlink("/vsimem/vrtovr_2.vrt")

###############################################################################
#


def test_vrtovr_none():

    vrt_string = """<VRTDataset rasterXSize="20" rasterYSize="20">
  <VRTRasterBand dataType="Byte" band="1">
    <ColorInterp>Gray</ColorInterp>
    <SimpleSource>
      <SourceFilename relativeToVRT="0">data/byte.tif</SourceFilename>
      <SourceBand>1</SourceBand>
      <SourceProperties RasterXSize="20" RasterYSize="20" DataType="Byte" BlockXSize="20" BlockYSize="20" />
      <SrcRect xOff="0" yOff="0" xSize="20" ySize="20" />
      <DstRect xOff="0" yOff="0" xSize="20" ySize="20" />
    </SimpleSource>
  </VRTRasterBand>
</VRTDataset>"""

    ds = gdal.Open(vrt_string)
    assert ds.GetRasterBand(1).GetOverviewCount() == 0, \
        'did not get expected overview count'

    assert not ds.GetRasterBand(1).GetOverview(0)

###############################################################################
#


def test_vrtovr_errors():

    vrt_string = """<VRTDataset rasterXSize="20" rasterYSize="20">
  <VRTRasterBand dataType="Byte" band="1">
    <ColorInterp>Gray</ColorInterp>
    <SimpleSource>
      <SourceFilename relativeToVRT="0">data/byte.tif</SourceFilename>
      <SourceBand>1</SourceBand>
      <SourceProperties RasterXSize="20" RasterYSize="20" DataType="Byte" BlockXSize="20" BlockYSize="20" />
      <SrcRect xOff="0" yOff="0" xSize="20" ySize="20" />
      <DstRect xOff="0" yOff="0" xSize="20" ySize="20" />
    </SimpleSource>
    <Overview>
      <SourceFilename relativeToVRT="0">data/int16.tif</SourceFilename>
      <SourceBand>123456</SourceBand>
    </Overview>
  </VRTRasterBand>
</VRTDataset>"""

    ds = gdal.Open(vrt_string)
    assert ds.GetRasterBand(1).GetOverviewCount() == 1, \
        'did not get expected overview count'

    assert not ds.GetRasterBand(1).GetOverview(-1)

    assert not ds.GetRasterBand(1).GetOverview(1)

    with gdaltest.error_handler():
        assert not ds.GetRasterBand(1).GetOverview(0)


###############################################################################
# Test support for virtual overviews


def test_vrtovr_virtual():

    tif_tmpfilename = '/vsimem/temp.tif'
    src_ds = gdal.GetDriverByName('GTiff').Create(tif_tmpfilename, 20, 20, 3)
    src_ds.BuildOverviews('NEAR', [2, 4])
    src_ds.GetRasterBand(1).Fill(200)
    src_ds.GetRasterBand(2).Fill(100)
    src_ds.GetRasterBand(1).GetOverview(0).Fill(100)
    src_ds.GetRasterBand(1).GetOverview(1).Fill(50)
    src_ds = None
    src_ds = gdal.Open(tif_tmpfilename)

    tmpfilename = '/vsimem/temp.vrt'
    vrt_ds = gdal.Translate(tmpfilename, src_ds, format='VRT')
    assert vrt_ds.GetRasterBand(1).GetOverviewCount() == 0 # we normally don't create implicit overviews on that small datasets

    with gdaltest.config_option('VRT_VIRTUAL_OVERVIEWS', 'YES'):
        vrt_ds.BuildOverviews('NEAR', [2, 4, 5, 50]) # level 50 is too big
    assert gdal.VSIStatL(tmpfilename + '.ovr') is None
    assert vrt_ds.GetRasterBand(1).GetOverviewCount() == 3

    # Clean overviews
    with gdaltest.config_option('VRT_VIRTUAL_OVERVIEWS', 'YES'):
        vrt_ds.BuildOverviews('NONE', [])
    assert vrt_ds.GetRasterBand(1).GetOverviewCount() == 0

    # Add in two steps
    with gdaltest.config_option('VRT_VIRTUAL_OVERVIEWS', 'YES'):
        vrt_ds.BuildOverviews('NEAR', [2, 4])
        vrt_ds.BuildOverviews('NEAR', [5])
    assert vrt_ds.GetRasterBand(1).GetOverviewCount() == 3

    assert vrt_ds.GetRasterBand(1).GetOverview(0).Checksum() == src_ds.GetRasterBand(1).GetOverview(0).Checksum()
    assert vrt_ds.GetRasterBand(1).GetOverview(1).Checksum() == src_ds.GetRasterBand(1).GetOverview(1).Checksum()
    assert vrt_ds.ReadRaster(0,0,20,20,10,10) == src_ds.ReadRaster(0,0,20,20,10,10)
    assert vrt_ds.GetRasterBand(1).ReadRaster(0,0,20,20,10,10) == src_ds.GetRasterBand(1).ReadRaster(0,0,20,20,10,10)
    assert vrt_ds.GetRasterBand(1).ReadRaster(0,0,20,20,5,5) == src_ds.GetRasterBand(1).ReadRaster(0,0,20,20,5,5)
    assert struct.unpack('B' * 4 * 4,vrt_ds.GetRasterBand(1).ReadRaster(0,0,20,20,4,4))[0] == 50
    vrt_ds = None

    # Re-open VRT and re-run checks
    vrt_ds = gdal.Open(tmpfilename)
    assert vrt_ds.GetRasterBand(1).GetOverviewCount() == 3
    assert vrt_ds.GetRasterBand(1).GetOverview(0).Checksum() == src_ds.GetRasterBand(1).GetOverview(0).Checksum()
    assert vrt_ds.GetRasterBand(1).GetOverview(1).Checksum() == src_ds.GetRasterBand(1).GetOverview(1).Checksum()
    assert vrt_ds.ReadRaster(0,0,20,20,10,10) == src_ds.ReadRaster(0,0,20,20,10,10)
    assert vrt_ds.GetRasterBand(1).ReadRaster(0,0,20,20,10,10) == src_ds.GetRasterBand(1).ReadRaster(0,0,20,20,10,10)
    assert vrt_ds.GetRasterBand(1).ReadRaster(0,0,20,20,5,5) == src_ds.GetRasterBand(1).ReadRaster(0,0,20,20,5,5)
    assert struct.unpack('B' * 4 * 4,vrt_ds.GetRasterBand(1).ReadRaster(0,0,20,20,4,4))[0] == 50

    gdal.Unlink(tmpfilename)
    gdal.Unlink(tif_tmpfilename)


###############################################################################
# Test support for virtual overviews, when there are pre-existing implicit
# overviews


def test_vrtovr_virtual_with_preexisting_implicit_ovr():

    src_ds = gdal.GetDriverByName('MEM').Create('', 1000, 1000)
    src_ds.BuildOverviews('NEAR', [2])
    vrt_ds = gdal.Translate('', src_ds, format='VRT')
    assert vrt_ds.GetRasterBand(1).GetOverviewCount() == 1

    with gdaltest.config_option('VRT_VIRTUAL_OVERVIEWS', 'YES'):
        vrt_ds.BuildOverviews('NEAR', [2, 4])
    assert vrt_ds.GetRasterBand(1).GetOverviewCount() == 2


###############################################################################
# Cleanup.

def test_vrtovr_cleanup():
    pass


