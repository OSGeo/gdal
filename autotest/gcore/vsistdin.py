#!/usr/bin/env pytest
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test /vsistdin/
# Author:   Even Rouault <even dot rouault at spatialys.com>
#
###############################################################################
# Copyright (c) 2012, Even Rouault <even dot rouault at spatialys.com>
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


from osgeo import gdal
import gdaltest
import test_cli_utilities
import pytest

###############################################################################
# Test on a small file


def test_vsistdin_1():
    if test_cli_utilities.get_gdal_translate_path() is None:
        pytest.skip()

    src_ds = gdal.Open('data/byte.tif')
    ds = gdal.GetDriverByName('GTiff').CreateCopy('tmp/vsistdin_1_src.tif', src_ds)
    ds = None
    cs = src_ds.GetRasterBand(1).Checksum()
    src_ds = None

    # Should work on both Unix and Windows
    os.system(test_cli_utilities.get_gdal_translate_path() + " /vsistdin/ tmp/vsistdin_1_out.tif -q < tmp/vsistdin_1_src.tif")

    gdal.Unlink("tmp/vsistdin_1_src.tif")

    ds = gdal.Open("tmp/vsistdin_1_out.tif")
    assert ds is not None
    assert ds.GetRasterBand(1).Checksum() == cs

    gdal.Unlink("tmp/vsistdin_1_out.tif")

###############################################################################
# Test on a bigger file (> 1 MB)


def test_vsistdin_2():
    if test_cli_utilities.get_gdal_translate_path() is None:
        pytest.skip()

    ds = gdal.GetDriverByName('GTiff').Create('tmp/vsistdin_2_src.tif', 2048, 2048)
    ds = None

    # Should work on both Unix and Windows
    os.system(test_cli_utilities.get_gdal_translate_path() + " /vsistdin/ tmp/vsistdin_2_out.tif -q < tmp/vsistdin_2_src.tif")

    gdal.Unlink("tmp/vsistdin_2_src.tif")

    ds = gdal.Open("tmp/vsistdin_2_out.tif")
    assert ds is not None
    ds = None

    gdal.Unlink("tmp/vsistdin_2_out.tif")

###############################################################################
# Test opening /vsistdin/ in write mode (failure expected)


def test_vsistdin_3():

    gdal.PushErrorHandler('CPLQuietErrorHandler')
    f = gdal.VSIFOpenL('/vsistdin/', 'wb')
    gdal.PopErrorHandler()
    assert f is None

###############################################################################
# Test fix for #6061


def test_vsistdin_4():
    if test_cli_utilities.get_gdal_translate_path() is None:
        pytest.skip()

    f = open('tmp/vsistdin_4_src.vrt', 'wt')
    f.write("""<VRTDataset rasterXSize="20" rasterYSize="20">
  %s
  <SRS>PROJCS["NAD27 / UTM zone 11N",GEOGCS["NAD27",DATUM["North_American_Datum_1927",SPHEROID["Clarke 1866",6378206.4,294.9786982138982,AUTHORITY["EPSG","7008"]],AUTHORITY["EPSG","6267"]],PRIMEM["Greenwich",0,AUTHORITY["EPSG","8901"]],UNIT["degree",0.0174532925199433,AUTHORITY["EPSG","9122"]],AUTHORITY["EPSG","4267"]],PROJECTION["Transverse_Mercator"],PARAMETER["latitude_of_origin",0],PARAMETER["central_meridian",-117],PARAMETER["scale_factor",0.9996],PARAMETER["false_easting",500000],PARAMETER["false_northing",0],UNIT["metre",1,AUTHORITY["EPSG","9001"]],AXIS["Easting",EAST],AXIS["Northing",NORTH],AUTHORITY["EPSG","26711"]]</SRS>
  <GeoTransform>  4.4072000000000000e+05,  6.0000000000000000e+01,  0.0000000000000000e+00,  3.7513200000000000e+06,  0.0000000000000000e+00, -6.0000000000000000e+01</GeoTransform>
  <VRTRasterBand dataType="Byte" band="1">
    <ColorInterp>Gray</ColorInterp>
    <SimpleSource>
      <SourceFilename relativeToVRT="0">data/byte.tif</SourceFilename>
      <SourceBand>1</SourceBand>
    </SimpleSource>
  </VRTRasterBand>
</VRTDataset>""" % (' ' * (2 * 1024 * 1024)))
    f.close()

    # Should work on both Unix and Windows
    os.system(test_cli_utilities.get_gdal_translate_path() + " /vsistdin/ tmp/vsistdin_4_out.tif -q < tmp/vsistdin_4_src.vrt")

    gdal.Unlink("tmp/vsistdin_4_src.vrt")

    ds = gdal.Open("tmp/vsistdin_4_out.tif")
    assert ds is not None
    ds = None

    gdal.Unlink("tmp/vsistdin_4_out.tif")


###############################################################################


def test_vsistdin_5():

    f = open('tmp/test_vsistdin_5.bin', 'wb')
    f.write(b'0123456789' * (1024 * 1024))
    f.close()

    with gdaltest.config_options({'CPL_VSISTDIN_FILE': 'tmp/test_vsistdin_5.bin',
                                  'CPL_VSISTDIN_RESET_POSITION': 'YES',
                                  'CPL_VSISTDIN_FILE_CLOSE': 'YES'}):
        f = gdal.VSIFOpenL('/vsistdin?buffer_limit=10MB', 'rb')
        assert f is not None
        assert gdal.VSIFEofL(f) == 0
        assert gdal.VSIFReadL(10, 1, f) == b'0123456789'
        assert gdal.VSIFTellL(f) == 10
        assert gdal.VSIFSeekL(f, 5, 0) == 0
        assert gdal.VSIFTellL(f) == 5
        assert gdal.VSIFReadL(3, 1, f) == b'567'
        assert gdal.VSIFTellL(f) == 8
        assert gdal.VSIFReadL(4, 1, f) == b'8901'
        assert gdal.VSIFTellL(f) == 12
        assert gdal.VSIFSeekL(f, 0, 2) == 0
        assert gdal.VSIFTellL(f) == 10*1024 * 1024
        assert gdal.VSIFReadL(1, 1, f) == b''
        assert gdal.VSIFEofL(f) == 1
        assert gdal.VSIFTellL(f) == 10*1024 * 1024
        assert gdal.VSIFSeekL(f, 5, 0) == 0
        assert gdal.VSIFTellL(f) == 5
        assert gdal.VSIFReadL(3, 1, f) == b'567'
        assert gdal.VSIFTellL(f) == 8
        assert gdal.VSIFSeekL(f, 10*1024 * 1024 - 10 + 1, 0) == 0
        assert gdal.VSIFReadL(3, 1, f) == b'123'
        gdal.VSIFCloseL(f)

    with gdaltest.config_options({'CPL_VSISTDIN_FILE': 'tmp/test_vsistdin_5.bin',
                                  'CPL_VSISTDIN_RESET_POSITION': 'YES',
                                  'CPL_VSISTDIN_FILE_CLOSE': 'YES'}):
        f = gdal.VSIFOpenL('/vsistdin?buffer_limit=-1', 'rb')
        assert f is not None
        assert gdal.VSIFSeekL(f, 0, 2) == 0
        assert gdal.VSIFTellL(f) == 10*1024 * 1024
        gdal.VSIFCloseL(f)

        assert gdal.VSIStatL('/vsistdin?buffer_limit=-1').size == 10*1024 * 1024

    with gdaltest.config_options({'CPL_VSISTDIN_FILE': 'tmp/test_vsistdin_5.bin',
                                  'CPL_VSISTDIN_RESET_POSITION': 'YES',
                                  'CPL_VSISTDIN_FILE_CLOSE': 'YES'}):
        f = gdal.VSIFOpenL('/vsistdin?buffer_limit=1GB', 'rb')
        assert f is not None
        assert gdal.VSIFSeekL(f, 0, 2) == 0
        assert gdal.VSIFTellL(f) == 10*1024 * 1024
        gdal.VSIFCloseL(f)

        assert gdal.VSIStatL('/vsistdin?buffer_limit=-1').size == 10*1024 * 1024

    with gdaltest.config_options({'CPL_VSISTDIN_FILE': 'tmp/test_vsistdin_5.bin',
                                  'CPL_VSISTDIN_RESET_POSITION': 'YES',
                                  'CPL_VSISTDIN_FILE_CLOSE': 'YES'}):
        f = gdal.VSIFOpenL('/vsistdin?buffer_limit=10', 'rb')
        assert f is not None
        assert gdal.VSIFSeekL(f, 0, 2) == 0
        assert gdal.VSIFTellL(f) == 10*1024 * 1024
        assert gdal.VSIFSeekL(f, 0, 0) == 0
        assert gdal.VSIFReadL(5, 1, f) == b'01234'
        assert gdal.VSIFReadL(3, 1, f) == b'567'
        with gdaltest.error_handler():
            assert gdal.VSIFReadL(3, 1, f) == b''
        gdal.VSIFCloseL(f)

    os.unlink('tmp/test_vsistdin_5.bin')
