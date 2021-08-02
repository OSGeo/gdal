#!/usr/bin/env pytest
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test STACIT driver
# Author:   Even Rouault <even.rouault@spatialys.com>
#
###############################################################################
# Copyright (c) 2021, Even Rouault <even.rouault@spatialys.com>
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

import gdaltest
import pytest

from osgeo import gdal, ogr

pytestmark = pytest.mark.require_driver('STACIT')


def test_stacit_basic():

    ds = gdal.Open('data/stacit/test.json')
    assert ds is not None
    assert ds.RasterCount == 1
    assert ds.RasterXSize == 40
    assert ds.RasterYSize == 20
    assert ds.GetSpatialRef().GetName() == 'NAD27 / UTM zone 11N'
    assert ds.GetGeoTransform() == pytest.approx([440720.0, 60.0, 0.0, 3751320.0, 0.0, -60.0], rel=1e-8)
    assert ds.GetRasterBand(1).GetNoDataValue() is None

    vrt = ds.GetMetadata('xml:VRT')[0]
    placement_vrt = """<SimpleSource>
      <SourceFilename relativeToVRT="0">data/byte.tif</SourceFilename>
      <SourceBand>1</SourceBand>
      <SrcRect xOff="0" yOff="0" xSize="20" ySize="20" />
      <DstRect xOff="0" yOff="0" xSize="20" ySize="20" />
    </SimpleSource>
    <SimpleSource>
      <SourceFilename relativeToVRT="0">data/int16.tif</SourceFilename>
      <SourceBand>1</SourceBand>
      <SrcRect xOff="0" yOff="0" xSize="20" ySize="20" />
      <DstRect xOff="20" yOff="0" xSize="20" ySize="20" />
    </SimpleSource>"""
    assert placement_vrt in vrt

    assert ds.GetRasterBand(1).Checksum() == 9239


def test_stacit_max_items():

    ds = gdal.OpenEx('data/stacit/test.json', open_options=['MAX_ITEMS=1'])
    assert ds is not None
    assert ds.RasterXSize == 20
    assert ds.GetRasterBand(1).Checksum() == 4672


def test_stacit_multiple_assets():

    ds = gdal.Open('data/stacit/test_multiple_assets.json')
    assert ds is not None
    assert ds.RasterCount == 0
    subds = ds.GetSubDatasets()
    assert subds == [
        ('STACIT:"data/stacit/test_multiple_assets.json":collection=my_collection,asset=B01,crs=EPSG_26711',
         'Collection my_collection, Asset B01 of data/stacit/test_multiple_assets.json in CRS EPSG:26711'),
        ('STACIT:"data/stacit/test_multiple_assets.json":collection=my_collection,asset=B01,crs=EPSG_26712',
         'Collection my_collection, Asset B01 of data/stacit/test_multiple_assets.json in CRS EPSG:26712'),
        ('STACIT:"data/stacit/test_multiple_assets.json":collection=my_collection,asset=B02',
         'Collection my_collection, Asset B02 of data/stacit/test_multiple_assets.json'),
        ('STACIT:"data/stacit/test_multiple_assets.json":collection=my_collection2,asset=B01',
         'Collection my_collection2, Asset B01 of data/stacit/test_multiple_assets.json'),
    ]

    ds = gdal.Open('STACIT:"data/stacit/test_multiple_assets.json":collection=my_collection,asset=B01,crs=EPSG_26711')
    assert ds.RasterXSize == 20
    assert ds.RasterYSize == 20
    assert ds.GetSpatialRef().GetName() == 'NAD27 / UTM zone 11N'
    assert ds.GetGeoTransform() == pytest.approx([440720.0, 60.0, 0.0, 3751320.0, 0.0, -60.0], rel=1e-8)

    ds = gdal.Open('STACIT:"data/stacit/test_multiple_assets.json":collection=my_collection,asset=B01,crs=EPSG_26712')
    assert ds.RasterXSize == 20
    assert ds.RasterYSize == 20
    assert ds.GetSpatialRef().GetName() == 'NAD27 / UTM zone 12N'
    assert ds.GetGeoTransform() == pytest.approx([440720.0, 60.0, 0.0, 3751320.0, 0.0, -60.0], rel=1e-8)

    ds = gdal.Open('STACIT:"data/stacit/test_multiple_assets.json":collection=my_collection,asset=B02')
    assert ds.RasterXSize == 20
    assert ds.RasterYSize == 20
    assert ds.GetSpatialRef().GetName() == 'NAD27 / UTM zone 11N'
    assert ds.GetGeoTransform() == pytest.approx([-440720.0, 60.0, 0.0, 3751320.0, 0.0, -60.0], rel=1e-8)

    ds = gdal.Open('STACIT:"data/stacit/test_multiple_assets.json":collection=my_collection2,asset=B01')
    assert ds.RasterXSize == 20
    assert ds.RasterYSize == 20
    assert ds.GetSpatialRef().GetName() == 'NAD27 / UTM zone 13N'
    assert ds.GetGeoTransform() == pytest.approx([440720.0, 60.0, 0.0, 3751320.0, 0.0, -60.0], rel=1e-8)

    with gdaltest.error_handler():
        ds = gdal.Open('STACIT:"data/stacit/test_multiple_assets.json":collection=i_dont_exist')
    assert ds is None

    with gdaltest.error_handler():
        ds = gdal.Open('STACIT:"data/stacit/test_multiple_assets.json":asset=i_dont_exist')
    assert ds is None


def test_stacit_overlapping_sources():

    if ogr.GetGEOSVersionMajor() == 0:
        pytest.skip('GEOS not available')

    ds = gdal.Open('data/stacit/overlapping_sources.json')
    assert ds is not None

    # Check that the source covered by another one is not listed
    vrt = ds.GetMetadata('xml:VRT')[0]
    placement_vrt = """
    <ColorInterp>Gray</ColorInterp>
    <SimpleSource>
      <SourceFilename relativeToVRT="0">data/byte.tif</SourceFilename>
      <SourceBand>1</SourceBand>
      <SrcRect xOff="0" yOff="0" xSize="20" ySize="20" />
      <DstRect xOff="0" yOff="0" xSize="20" ySize="20" />
    </SimpleSource>
  </VRTRasterBand>"""
    # print(vrt)
    assert placement_vrt in vrt
