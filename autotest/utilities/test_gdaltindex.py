#!/usr/bin/env pytest
# -*- coding: utf-8 -*-
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  gdaltindex testing
# Author:   Even Rouault <even dot rouault @ spatialys.com>
#
###############################################################################
# Copyright (c) 2008-2013, Even Rouault <even dot rouault at spatialys.com>
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
import test_cli_utilities

from osgeo import gdal, ogr, osr

pytestmark = [
    pytest.mark.skipif(
        test_cli_utilities.get_gdaltindex_path() is None,
        reason="gdaltindex not available",
    ),
    pytest.mark.random_order(disabled=True),
]


@pytest.fixture()
def gdaltindex_path():
    return test_cli_utilities.get_gdaltindex_path()


###############################################################################
# Simple test


@pytest.fixture(scope="module")
def four_tiles(tmp_path_factory):

    drv = gdal.GetDriverByName("GTiff")
    wkt = 'GEOGCS["WGS 84",DATUM["WGS_1984",SPHEROID["WGS 84",6378137,298.257223563,AUTHORITY["EPSG","7030"]],TOWGS84[0,0,0,0,0,0,0],AUTHORITY["EPSG","6326"]],PRIMEM["Greenwich",0,AUTHORITY["EPSG","8901"]],UNIT["degree",0.0174532925199433,AUTHORITY["EPSG","9108"]],AUTHORITY["EPSG","4326"]]'

    dirname = tmp_path_factory.mktemp("test_gdaltindex")
    fnames = [f"{dirname}/gdaltindex{i}.tif" for i in (1, 2, 3, 4)]

    ds = drv.Create(fnames[0], 10, 10, 1)
    ds.SetProjection(wkt)
    ds.SetGeoTransform([49, 0.1, 0, 2, 0, -0.1])
    ds = None

    ds = drv.Create(fnames[1], 10, 10, 1)
    ds.SetProjection(wkt)
    ds.SetGeoTransform([49, 0.1, 0, 3, 0, -0.1])
    ds = None

    ds = drv.Create(fnames[2], 10, 10, 1)
    ds.SetProjection(wkt)
    ds.SetGeoTransform([48, 0.1, 0, 2, 0, -0.1])
    ds = None

    ds = drv.Create(fnames[3], 10, 10, 1)
    ds.SetProjection(wkt)
    ds.SetGeoTransform([48, 0.1, 0, 3, 0, -0.1])
    ds = None

    return fnames


@pytest.fixture()
def four_tile_index(gdaltindex_path, four_tiles, tmp_path):

    (_, err) = gdaltest.runexternal_out_and_err(
        f"{gdaltindex_path} {tmp_path}/tileindex.shp {four_tiles[0]} {four_tiles[1]}"
    )
    assert err is None or err == "", "got error/warning"

    (ret_stdout, ret_stderr) = gdaltest.runexternal_out_and_err(
        f"{gdaltindex_path} {tmp_path}/tileindex.shp {four_tiles[2]} {four_tiles[3]}"
    )

    return f"{tmp_path}/tileindex.shp"


def test_gdaltindex_1(gdaltindex_path, four_tile_index):

    ds = ogr.Open(four_tile_index)
    assert ds.GetLayer(0).GetFeatureCount() == 4

    tileindex_wkt = ds.GetLayer(0).GetSpatialRef().ExportToWkt()
    assert "WGS_1984" in tileindex_wkt

    expected_wkts = [
        "POLYGON ((49 2,50 2,50 1,49 1,49 2))",
        "POLYGON ((49 3,50 3,50 2,49 2,49 3))",
        "POLYGON ((48 2,49 2,49 1,48 1,48 2))",
        "POLYGON ((48 3,49 3,49 2,48 2,48 3))",
    ]

    for i, feat in enumerate(ds.GetLayer(0)):
        assert (
            feat.GetGeometryRef().ExportToWkt() == expected_wkts[i]
        ), "i=%d, wkt=%s" % (i, feat.GetGeometryRef().ExportToWkt())


###############################################################################
# Try adding the same rasters again


def test_gdaltindex_2(gdaltindex_path, four_tiles, four_tile_index, tmp_path):

    (_, ret_stderr) = gdaltest.runexternal_out_and_err(
        f"{gdaltindex_path} {four_tile_index} {four_tiles[0]} {four_tiles[1]} {four_tiles[2]} {four_tiles[3]}"
    )

    assert "gdaltindex1.tif is already in tileindex. Skipping it." in ret_stderr
    assert "gdaltindex2.tif is already in tileindex. Skipping it." in ret_stderr
    assert "gdaltindex3.tif is already in tileindex. Skipping it." in ret_stderr
    assert "gdaltindex4.tif is already in tileindex. Skipping it." in ret_stderr

    ds = ogr.Open(four_tile_index)
    assert ds.GetLayer(0).GetFeatureCount() == 4


###############################################################################
# Try adding a raster in another projection with -skip_different_projection
# 5th tile should NOT be inserted


def test_gdaltindex_3(gdaltindex_path, tmp_path, four_tile_index):

    drv = gdal.GetDriverByName("GTiff")
    wkt = """GEOGCS["WGS 72",
    DATUM["WGS_1972",
        SPHEROID["WGS 72",6378135,298.26],
        TOWGS84[0,0,4.5,0,0,0.554,0.2263]],
    PRIMEM["Greenwich",0],
    UNIT["degree",0.0174532925199433]]"""

    ds = drv.Create(f"{tmp_path}/gdaltindex5.tif", 10, 10, 1)
    ds.SetProjection(wkt)
    ds.SetGeoTransform([47, 0.1, 0, 2, 0, -0.1])
    ds = None

    (_, ret_stderr) = gdaltest.runexternal_out_and_err(
        f"{gdaltindex_path} -skip_different_projection {four_tile_index} {tmp_path}/gdaltindex5.tif"
    )

    assert (
        "gdaltindex5.tif is not using the same projection system as other files in the tileindex."
        in ret_stderr
    )
    assert (
        "Use -t_srs option to set target projection system (not supported by MapServer)."
        in ret_stderr
    )

    ds = ogr.Open(four_tile_index)
    assert ds.GetLayer(0).GetFeatureCount() == 4


###############################################################################
# Try adding a raster in another projection with -t_srs
# 5th tile should be inserted, will not be if there is a srs transformation error


def test_gdaltindex_4(gdaltindex_path, tmp_path, four_tile_index):

    drv = gdal.GetDriverByName("GTiff")
    wkt = """GEOGCS["WGS 72",
    DATUM["WGS_1972",
        SPHEROID["WGS 72",6378135,298.26],
        TOWGS84[0,0,4.5,0,0,0.554,0.2263]],
    PRIMEM["Greenwich",0],
    UNIT["degree",0.0174532925199433]]"""

    ds = drv.Create(f"{tmp_path}/gdaltindex5.tif", 10, 10, 1)
    ds.SetProjection(wkt)
    ds.SetGeoTransform([47, 0.1, 0, 2, 0, -0.1])
    ds = None

    gdaltest.runexternal_out_and_err(
        f"{gdaltindex_path} -t_srs EPSG:4326 {four_tile_index} {tmp_path}/gdaltindex5.tif"
    )

    ds = ogr.Open(four_tile_index)
    assert ds.GetLayer(0).GetFeatureCount() == 5, (
        "got %d features, expecting 5" % ds.GetLayer(0).GetFeatureCount()
    )


###############################################################################
# Test -src_srs_name, -src_srs_format options


@pytest.mark.parametrize(
    "src_srs_format",
    [
        "",
        "-src_srs_format AUTO",
        "-src_srs_format EPSG",
        "-src_srs_format PROJ",
        "-src_srs_format WKT",
    ],
)
def test_gdaltindex_5(gdaltindex_path, tmp_path, four_tiles, src_srs_format):

    index_shp = str(tmp_path / "test_gdaltindex_5.shp")
    tile1_tif = four_tiles[0]
    tile2_tif = str(tmp_path / "gdaltindex6.tif")

    drv = gdal.GetDriverByName("GTiff")

    ds = drv.Create(tile2_tif, 10, 10, 1)
    sr = osr.SpatialReference()
    sr.ImportFromEPSG(4322)
    ds.SetProjection(sr.ExportToWkt())
    ds.SetGeoTransform([47, 0.1, 0, 2, 0, -0.1])
    ds = None

    gdaltest.runexternal_out_and_err(
        f"{gdaltindex_path} -src_srs_name src_srs {src_srs_format} -t_srs EPSG:4326 {index_shp} {tile1_tif} {tile2_tif}"
    )

    ds = ogr.Open(index_shp)
    lyr = ds.GetLayer(0)
    assert lyr.GetFeatureCount() == 2, (
        "got %d features, expecting 2" % ds.GetLayer(0).GetFeatureCount()
    )
    feat = lyr.GetNextFeature()
    feat = lyr.GetNextFeature()
    if src_srs_format == "-src_srs_format PROJ":
        assert "+proj=longlat +ellps=WGS72" in feat.GetField("src_srs")
    elif src_srs_format == "-src_srs_format WKT":
        # if feat.GetField('src_srs').find('GEOGCS["WGS 72"') != 0:
        # Full definition too long...
        if feat.GetField("src_srs") is not None:
            feat.DumpReadable()
            pytest.fail()
    else:
        assert feat.GetField("src_srs") == "EPSG:4322"


###############################################################################
# Test -f, -lyr_name


@pytest.mark.parametrize("option", ["", "-lyr_name tileindex"])
def test_gdaltindex_6(gdaltindex_path, tmp_path, four_tiles, option):

    index_mif = str(tmp_path / "test_gdaltindex6.mif")

    gdaltest.runexternal_out_and_err(
        f'{gdaltindex_path} -f "MapInfo File" {option} {index_mif} {four_tiles[0]}'
    )
    ds = ogr.Open(index_mif)
    lyr = ds.GetLayer(0)
    assert lyr.GetFeatureCount() == 1, (
        "got %d features, expecting 1" % lyr.GetFeatureCount()
    )
    ds = None
