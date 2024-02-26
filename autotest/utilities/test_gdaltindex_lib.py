#!/usr/bin/env pytest
# -*- coding: utf-8 -*-
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Library version of gdaltindex testing
# Author:   Even Rouault <even dot rouault @ spatialys.com>
#
###############################################################################
# Copyright (c) 2023, Even Rouault <even dot rouault at spatialys.com>
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

import gdaltest
import pytest

from osgeo import gdal, ogr

###############################################################################
# Simple test


@pytest.fixture(scope="module")
def four_tiles(tmp_path_factory):

    drv = gdal.GetDriverByName("GTiff")
    wkt = 'GEOGCS["WGS 84",DATUM["WGS_1984",SPHEROID["WGS 84",6378137,298.257223563,AUTHORITY["EPSG","7030"]],TOWGS84[0,0,0,0,0,0,0],AUTHORITY["EPSG","6326"]],PRIMEM["Greenwich",0,AUTHORITY["EPSG","8901"]],UNIT["degree",0.0174532925199433,AUTHORITY["EPSG","9108"]],AUTHORITY["EPSG","4326"]]'

    dirname = tmp_path_factory.mktemp("test_gdaltindex")
    fnames = [f"{dirname}/gdaltindex{i}.tif" for i in (1, 2, 3, 4)]

    ds = drv.Create(fnames[0], 10, 10, 1)
    ds.SetMetadataItem("foo", "bar")
    ds.SetMetadataItem("TIFFTAG_DATETIME", "2023:12:20 16:10:00")
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
def four_tile_index(four_tiles, tmp_path):

    gdal.TileIndex(f"{tmp_path}/tileindex.shp", [four_tiles[0], four_tiles[1]])
    gdal.TileIndex(f"{tmp_path}/tileindex.shp", [four_tiles[2], four_tiles[3]])
    return f"{tmp_path}/tileindex.shp"


def test_gdaltindex_lib_basic(four_tile_index):

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


def test_gdaltindex_lib_already_existing_rasters(four_tiles, four_tile_index, tmp_path):
    class GdalErrorHandler(object):
        def __init__(self):
            self.warnings = []

        def handler(self, err_level, err_no, err_msg):
            if err_level == gdal.CE_Warning:
                self.warnings.append(err_msg)

    err_handler = GdalErrorHandler()
    with gdaltest.error_handler(err_handler.handler):
        ds = gdal.TileIndex(four_tile_index, four_tiles)
        del ds

    assert len(err_handler.warnings) == 4
    assert (
        "gdaltindex1.tif is already in tileindex. Skipping it."
        in err_handler.warnings[0]
    )
    assert (
        "gdaltindex2.tif is already in tileindex. Skipping it."
        in err_handler.warnings[1]
    )
    assert (
        "gdaltindex3.tif is already in tileindex. Skipping it."
        in err_handler.warnings[2]
    )
    assert (
        "gdaltindex4.tif is already in tileindex. Skipping it."
        in err_handler.warnings[3]
    )

    ds = ogr.Open(four_tile_index)
    assert ds.GetLayer(0).GetFeatureCount() == 4


###############################################################################
# Try adding a raster in another projection with -skip_different_projection
# 5th tile should NOT be inserted


def test_gdaltindex_skipDifferentProjection(tmp_path, four_tile_index):

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

    class GdalErrorHandler(object):
        def __init__(self):
            self.warning = None

        def handler(self, err_level, err_no, err_msg):
            if err_level == gdal.CE_Warning:
                self.warning = err_msg

    err_handler = GdalErrorHandler()
    with gdaltest.error_handler(err_handler.handler):
        gdal.TileIndex(
            four_tile_index,
            [f"{tmp_path}/gdaltindex5.tif"],
            skipDifferentProjection=True,
        )
    assert (
        "gdaltindex5.tif is not using the same projection system as other files in the tileindex"
        in err_handler.warning
    )

    ds = ogr.Open(four_tile_index)
    assert ds.GetLayer(0).GetFeatureCount() == 4


###############################################################################
# Try adding a raster in another projection with -t_srs
# 5th tile should be inserted, will not be if there is a srs transformation error


def test_gdaltindex_lib_outputSRS_writeAbsoluePath(tmp_path, four_tile_index):

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

    saved_dir = os.getcwd()
    try:
        os.chdir(tmp_path)
        gdal.TileIndex(
            four_tile_index,
            ["gdaltindex5.tif"],
            outputSRS="EPSG:4326",
            writeAbsolutePath=True,
        )
    finally:
        os.chdir(saved_dir)

    ds = ogr.Open(four_tile_index)
    lyr = ds.GetLayer(0)
    assert lyr.GetFeatureCount() == 5, (
        "got %d features, expecting 5" % ds.GetLayer(0).GetFeatureCount()
    )
    filename = lyr.GetFeature(4).GetField("location")
    # Check that path is absolute
    assert filename.endswith("gdaltindex5.tif")
    assert filename != "gdaltindex5.tif"


###############################################################################
# Test -f, -lyr_name


def test_gdaltindex_lib_format_layerName(tmp_path, four_tiles):

    index_mif = str(tmp_path / "test_gdaltindex6.mif")

    gdal.TileIndex(
        index_mif, [four_tiles[0]], format="MapInfo File", layerName="tileindex"
    )
    ds = ogr.Open(index_mif)
    lyr = ds.GetLayer(0)
    assert lyr.GetFeatureCount() == 1, (
        "got %d features, expecting 1" % lyr.GetFeatureCount()
    )
    ds = None


###############################################################################
# Test -overwrite


def test_gdaltindex_lib_overwrite(tmp_path, four_tiles):

    index_filename = str(tmp_path / "test_gdaltindex_lib_overwrite.shp")

    gdal.TileIndex(index_filename, [four_tiles[0]])
    gdal.TileIndex(index_filename, [four_tiles[1], four_tiles[2]], overwrite=True)
    ds = ogr.Open(index_filename)
    lyr = ds.GetLayer(0)
    assert lyr.GetFeatureCount() == 2


###############################################################################
# Test GTI related options


@pytest.mark.require_driver("GPKG")
@pytest.mark.require_driver("GTI")
def test_gdaltindex_lib_gti_non_xml(tmp_path, four_tiles):

    index_filename = str(tmp_path / "test_gdaltindex_lib_gti_non_xml.gti.gpkg")

    gdal.TileIndex(
        index_filename,
        four_tiles,
        layerName="tileindex",
        xRes=60,
        yRes=60,
        outputBounds=[0, 1, 2, 3],
        bandCount=1,
        noData=0,
        colorInterpretation="gray",
        mask=True,
        metadataOptions={"foo": "bar"},
        layerCreationOptions=["FID=my_fid"],
    )

    ds = ogr.Open(index_filename)
    lyr = ds.GetLayer(0)
    assert lyr.GetFIDColumn() == "my_fid"
    assert lyr.GetMetadataItem("RESX") == "60"
    assert lyr.GetMetadataItem("RESY") == "60"
    assert lyr.GetMetadataItem("MINX") == "0"
    assert lyr.GetMetadataItem("MINY") == "1"
    assert lyr.GetMetadataItem("MAXX") == "2"
    assert lyr.GetMetadataItem("MAXY") == "3"
    assert lyr.GetMetadataItem("BAND_COUNT") == "1"
    assert lyr.GetMetadataItem("NODATA") == "0"
    assert lyr.GetMetadataItem("COLOR_INTERPRETATION") == "gray"
    assert lyr.GetMetadataItem("MASK_BAND") == "YES"
    assert lyr.GetMetadataItem("foo") == "bar"
    del ds

    ds = gdal.Open(index_filename)
    assert ds.GetGeoTransform() == (0.0, 60.0, 0.0, 3.0, 0.0, -60.0)
    assert ds.RasterCount == 1
    assert ds.GetRasterBand(1).GetNoDataValue() == 0
    assert ds.GetRasterBand(1).GetColorInterpretation() == gdal.GCI_GrayIndex
    assert ds.GetRasterBand(1).GetMaskFlags() == gdal.GMF_PER_DATASET
    del ds


###############################################################################
# Test GTI related options


@pytest.mark.require_driver("GPKG")
@pytest.mark.require_driver("GTI")
def test_gdaltindex_lib_gti_xml(tmp_path, four_tiles):

    index_filename = str(tmp_path / "test_gdaltindex_lib_gti_non_xml.gti.gpkg")
    gti_filename = str(tmp_path / "test_gdaltindex_lib_gti_non_xml.gti")

    gdal.TileIndex(
        index_filename,
        four_tiles,
        layerName="tileindex",
        gtiFilename=gti_filename,
        xRes=60,
        yRes=60,
        outputBounds=[0, 1, 2, 3],
        noData=[0],
        colorInterpretation=["gray"],
        mask=True,
    )

    xml = open(gti_filename, "rb").read().decode("UTF-8")
    assert "test_gdaltindex_lib_gti_non_xml.gti.gpkg</IndexDataset>" in xml
    assert "<IndexLayer>tileindex</IndexLayer>" in xml
    assert "<LocationField>location</LocationField>" in xml
    assert "<ResX>60</ResX>" in xml
    assert "<ResY>60</ResY>" in xml
    assert "<MinX>0</MinX>" in xml
    assert "<MinY>1</MinY>" in xml
    assert "<MaxX>2</MaxX>" in xml
    assert "<MaxY>3</MaxY>" in xml
    assert (
        """<Band band="1">\n    <NoDataValue>0</NoDataValue>\n    <ColorInterp>gray</ColorInterp>\n  </Band>"""
        in xml
    )
    assert "<MaskBand>true</MaskBand>" in xml

    ds = gdal.Open(gti_filename)
    assert ds.GetGeoTransform() == (0.0, 60.0, 0.0, 3.0, 0.0, -60.0)
    assert ds.RasterCount == 1
    assert ds.GetRasterBand(1).GetNoDataValue() == 0
    assert ds.GetRasterBand(1).GetColorInterpretation() == gdal.GCI_GrayIndex
    assert ds.GetRasterBand(1).GetMaskFlags() == gdal.GMF_PER_DATASET


###############################################################################
# Test directory exploration and filtering


def test_gdaltindex_lib_directory(tmp_path, four_tiles):

    index_filename = str(tmp_path / "test_gdaltindex_lib_overwrite.shp")

    gdal.TileIndex(
        index_filename,
        [os.path.dirname(four_tiles[0])],
        recursive=True,
        filenameFilter="*.?if",
        minPixelSize=0,
        maxPixelSize=1,
    )

    ds = ogr.Open(index_filename)
    lyr = ds.GetLayer(0)
    assert lyr.GetFeatureCount() == 4
    del ds

    with gdal.quiet_errors():
        # triggers warnings: "file XXXX has 0.010000 as pixel size (< 10.000000). Skipping"
        gdal.TileIndex(
            index_filename,
            [os.path.dirname(four_tiles[0])],
            minPixelSize=10,
            overwrite=True,
        )

    ds = ogr.Open(index_filename)
    lyr = ds.GetLayer(0)
    assert lyr.GetFeatureCount() == 0
    del ds

    with gdal.quiet_errors():
        gdal.TileIndex(
            index_filename,
            [os.path.dirname(four_tiles[0])],
            maxPixelSize=1e-3,
            overwrite=True,
        )

    ds = ogr.Open(index_filename)
    lyr = ds.GetLayer(0)
    assert lyr.GetFeatureCount() == 0
    del ds

    with pytest.raises(Exception, match="Cannot find any tile"):
        gdal.TileIndex(
            index_filename,
            [os.path.dirname(four_tiles[0])],
            filenameFilter="*.xyz",
            overwrite=True,
        )


###############################################################################
# Test -fetchMD


@pytest.mark.require_driver("GPKG")
def test_gdaltindex_lib_fetch_md(tmp_path, four_tiles):

    index_filename = str(tmp_path / "test_gdaltindex_lib_fetch_md.gpkg")

    gdal.TileIndex(
        index_filename,
        four_tiles[0],
        fetchMD=[
            ("foo", "foo_field", "String"),
            ("{PIXEL_SIZE}", "pixel_size", "Real"),
            ("TIFFTAG_DATETIME", "dt", "DateTime"),
        ],
    )

    ds = ogr.Open(index_filename)
    lyr = ds.GetLayer(0)
    f = lyr.GetNextFeature()
    assert f["foo_field"] == "bar"
    assert f["dt"] == "2023/12/20 16:10:00"
    assert f["pixel_size"] == pytest.approx(0.01)
    del ds

    gdal.TileIndex(
        index_filename,
        four_tiles[0],
        fetchMD=("foo", "foo_field", "String"),
        overwrite=True,
    )

    ds = ogr.Open(index_filename)
    lyr = ds.GetLayer(0)
    f = lyr.GetNextFeature()
    assert f["foo_field"] == "bar"
