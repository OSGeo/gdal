#!/usr/bin/env pytest
# -*- coding: utf-8 -*-
###############################################################################
#
# Project:  GDAL/OGR Test Suite
# Purpose:  gdaltindex testing
# Author:   Even Rouault <even dot rouault @ spatialys.com>
#
###############################################################################
# Copyright (c) 2008-2013, Even Rouault <even dot rouault at spatialys.com>
#
# SPDX-License-Identifier: MIT
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
    pytest.mark.xdist_group("test_gdaltindex"),
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

    _, err = gdaltest.runexternal_out_and_err(
        f"{gdaltindex_path} {tmp_path}/tileindex.shp {four_tiles[0]} {four_tiles[1]}"
    )
    assert err is None or err == "", "got error/warning"

    ret_stdout, ret_stderr = gdaltest.runexternal_out_and_err(
        f"{gdaltindex_path} {tmp_path}/tileindex.shp {four_tiles[2]} {four_tiles[3]}"
    )

    return f"{tmp_path}/tileindex.shp"


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
