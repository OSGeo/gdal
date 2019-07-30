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




