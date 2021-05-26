#!/usr/bin/env pytest
# -*- coding: utf-8 -*-
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  gdal_create testing
# Author:   Even Rouault <even dot rouault @ spatialys.com>
#
###############################################################################
# Copyright (c) 2020, Even Rouault <even dot rouault at spatialys.com>
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

def get_gdal_create_path():
    return test_cli_utilities.get_cli_utility_path('gdal_create')


###############################################################################

def test_gdal_create_pdf_tif():
    if get_gdal_create_path() is None:
        pytest.skip()

    (_, err) = gdaltest.runexternal_out_and_err(get_gdal_create_path() + ' tmp/tmp.tif -bands 3 -outsize 1 2 -a_srs EPSG:4326 -a_ullr 2 50 3 49 -a_nodata 5 -burn 1 2 -ot UInt16 -co COMPRESS=DEFLATE -mo FOO=BAR')
    assert (err is None or err == ''), 'got error/warning'

    ds = gdal.Open('tmp/tmp.tif')
    assert ds.RasterCount == 3
    assert ds.RasterXSize == 1
    assert ds.RasterYSize == 2
    assert ds.GetRasterBand(1).DataType == gdal.GDT_UInt16
    assert ds.GetRasterBand(1).GetNoDataValue() == 5
    assert '4326' in ds.GetProjectionRef()
    assert ds.GetGeoTransform() == (2.0, 1.0, 0.0, 50.0, 0.0, -0.5)
    assert ds.GetRasterBand(1).Checksum() == 2
    assert ds.GetRasterBand(2).Checksum() == 4
    assert ds.GetRasterBand(3).Checksum() == 4
    assert ds.GetMetadataItem('COMPRESSION', 'IMAGE_STRUCTURE') == 'DEFLATE'
    assert ds.GetMetadataItem('FOO') == 'BAR'
    ds = None

    os.unlink('tmp/tmp.tif')

###############################################################################

def test_gdal_create_pdf_no_direct_write_capabilities():
    if get_gdal_create_path() is None:
        pytest.skip()

    (_, err) = gdaltest.runexternal_out_and_err(get_gdal_create_path() + ' tmp/tmp.png -of PNG -outsize 1 2')
    assert (err is None or err == ''), 'got error/warning'

    ds = gdal.Open('tmp/tmp.png')
    assert ds.RasterCount == 1
    assert ds.RasterXSize == 1
    assert ds.RasterYSize == 2
    assert ds.GetRasterBand(1).DataType == gdal.GDT_Byte
    assert ds.GetRasterBand(1).GetNoDataValue() is None
    assert ds.GetProjectionRef() == ''
    assert ds.GetGeoTransform(can_return_null=True) is None
    assert ds.GetRasterBand(1).Checksum() == 0
    ds = None

    os.unlink('tmp/tmp.png')

###############################################################################

def test_gdal_create_pdf_composition():
    if get_gdal_create_path() is None:
        pytest.skip()
    drv = gdal.GetDriverByName('PDF')
    if drv is None:
        pytest.skip()
    if drv.GetMetadataItem('HAVE_POPPLER') is None:
        pytest.skip()

    open('tmp/tmp.xml', 'wt').write("""<PDFComposition>
    <Page>
        <DPI>72</DPI>
        <Width>20</Width>
        <Height>20</Height>
        <Content streamCompression="NONE">
            <Raster dataset="../gcore/data/byte.tif" tileSize="16">
                <Blending function="Multiply" opacity="0.7"/>
            </Raster>
        </Content>
    </Page>
</PDFComposition>""")

    (_, err) = gdaltest.runexternal_out_and_err(get_gdal_create_path() + ' tmp/tmp.pdf -co COMPOSITION_FILE=tmp/tmp.xml')
    os.unlink('tmp/tmp.xml')
    assert (err is None or err == ''), 'got error/warning'

    assert os.path.exists('tmp/tmp.pdf')

    os.unlink('tmp/tmp.pdf')

###############################################################################

def test_gdal_create_not_write_driver():
    if get_gdal_create_path() is None:
        pytest.skip()

    (_, err) = gdaltest.runexternal_out_and_err(get_gdal_create_path() + ' /vsimem/tmp.tga -of TGA -outsize 1 2')
    assert 'This driver has no creation capabilities' in err

###############################################################################


def test_gdal_create_input_file_invalid():
    if get_gdal_create_path() is None:
        pytest.skip()

    (_, err) = gdaltest.runexternal_out_and_err(get_gdal_create_path() + ' tmp/tmp.tif -if ../gdrivers/data/i_do_not_exist')
    assert err != ''

    assert not os.path.exists('tmp/tmp.tif')

###############################################################################


def test_gdal_create_input_file():
    if get_gdal_create_path() is None:
        pytest.skip()

    (_, err) = gdaltest.runexternal_out_and_err(get_gdal_create_path() + ' tmp/tmp.tif -if ../gdrivers/data/small_world.tif')
    assert (err is None or err == ''), 'got error/warning'

    assert os.path.exists('tmp/tmp.tif')

    ds = gdal.Open('tmp/tmp.tif')
    ref_ds = gdal.Open('../gdrivers/data/small_world.tif')
    assert ds.RasterCount == ref_ds.RasterCount
    assert ds.RasterXSize == ref_ds.RasterXSize
    assert ds.RasterYSize == ref_ds.RasterYSize
    assert ds.GetRasterBand(1).GetNoDataValue() is None
    assert ds.GetGeoTransform() == ref_ds.GetGeoTransform()
    assert ds.GetProjectionRef() == ref_ds.GetProjectionRef()
    ds = None

    os.unlink('tmp/tmp.tif')

###############################################################################


def test_gdal_create_input_file_overrrides():
    if get_gdal_create_path() is None:
        pytest.skip()

    (_, err) = gdaltest.runexternal_out_and_err(get_gdal_create_path() + ' tmp/tmp.tif -if ../gdrivers/data/small_world.tif -bands 2 -outsize 1 3 -a_nodata 1')
    assert (err is None or err == ''), 'got error/warning'

    assert os.path.exists('tmp/tmp.tif')

    ds = gdal.Open('tmp/tmp.tif')
    ref_ds = gdal.Open('../gdrivers/data/small_world.tif')
    assert ds.RasterCount == 2
    assert ds.RasterXSize == 1
    assert ds.RasterYSize == 3
    assert ds.GetRasterBand(1).GetNoDataValue() == 1
    assert ds.GetGeoTransform() == ref_ds.GetGeoTransform()
    assert ds.GetProjectionRef() == ref_ds.GetProjectionRef()
    ds = None

    os.unlink('tmp/tmp.tif')
