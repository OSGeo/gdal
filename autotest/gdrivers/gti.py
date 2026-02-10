#!/usr/bin/env pytest
###############################################################################
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test GDALTileIndexDataset support.
# Author:   Even Rouault <even.rouault@spatialys.com>
#
###############################################################################
# Copyright (c) 2023, Even Rouault <even.rouault@spatialys.com>
#
# SPDX-License-Identifier: MIT
###############################################################################

import math
import os
import shutil
import struct

import gdaltest
import ogrtest
import pytest
import webserver

from osgeo import gdal, ogr

pytestmark = [pytest.mark.require_driver("GTI"), pytest.mark.require_driver("GPKG")]


def create_basic_tileindex(
    index_filename,
    src_ds,
    location_field_name="location",
    sort_field_name=None,
    sort_field_type=None,
    sort_values=None,
    lyr_name="index",
    add_to_existing=False,
):
    if isinstance(src_ds, list):
        src_ds_list = src_ds
    else:
        src_ds_list = [src_ds]
    if add_to_existing:
        index_ds = ogr.Open(index_filename, update=1)
    else:
        index_ds = ogr.GetDriverByName("GPKG").CreateDataSource(index_filename)
    lyr = index_ds.CreateLayer(
        lyr_name, srs=(src_ds_list[0].GetSpatialRef() if src_ds_list else None)
    )
    lyr.CreateField(ogr.FieldDefn(location_field_name))
    if sort_values:
        lyr.CreateField(ogr.FieldDefn(sort_field_name, sort_field_type))
        lyr.SetMetadataItem("SORT_FIELD", sort_field_name)
    for i, src_ds in enumerate(src_ds_list):
        f = ogr.Feature(lyr.GetLayerDefn())
        src_gt = src_ds.GetGeoTransform()
        minx = src_gt[0]
        maxx = minx + src_ds.RasterXSize * src_gt[1]
        maxy = src_gt[3]
        miny = maxy + src_ds.RasterYSize * src_gt[5]
        f[location_field_name] = src_ds.GetDescription()
        if sort_values:
            f[sort_field_name] = sort_values[i]
        f.SetGeometry(
            ogr.CreateGeometryFromWkt(
                f"POLYGON(({minx} {miny},{minx} {maxy},{maxx} {maxy},{maxx} {miny},{minx} {miny}))"
            )
        )
        lyr.CreateFeature(f)
    return index_ds, lyr


def check_basic(
    vrt_ds, src_ds, expected_dt=None, expected_colorinterp=None, expected_md={}
):
    assert vrt_ds.RasterXSize == src_ds.RasterXSize
    assert vrt_ds.RasterYSize == src_ds.RasterYSize
    assert vrt_ds.RasterCount == src_ds.RasterCount
    assert vrt_ds.GetGeoTransform() == pytest.approx(src_ds.GetGeoTransform())
    assert vrt_ds.GetSpatialRef().GetAuthorityCode(
        None
    ) == src_ds.GetSpatialRef().GetAuthorityCode(None)
    for iband in range(1, vrt_ds.RasterCount + 1):
        vrt_band = vrt_ds.GetRasterBand(iband)
        src_band = src_ds.GetRasterBand(iband)
        assert vrt_band.DataType == (
            expected_dt if expected_dt is not None else src_band.DataType
        )
        assert vrt_band.GetNoDataValue() == src_band.GetNoDataValue()
        assert vrt_band.GetColorInterpretation() == (
            expected_colorinterp
            if expected_colorinterp is not None
            else src_band.GetColorInterpretation()
        )
        assert vrt_band.Checksum() == src_band.Checksum()
        assert vrt_band.ReadRaster(
            1, 2, 3, 4, buf_type=src_band.DataType
        ) == src_band.ReadRaster(1, 2, 3, 4)

    assert vrt_ds.ReadRaster(
        1, 2, 3, 4, buf_type=src_band.DataType
    ) == src_ds.ReadRaster(1, 2, 3, 4)
    assert vrt_ds.GetMetadata_Dict() == expected_md


def test_gti_no_metadata(tmp_vsimem):

    index_filename = str(tmp_vsimem / "index.gpkg")

    src_ds = gdal.Open(os.path.join(os.getcwd(), "data", "byte.tif"))
    index_ds, _ = create_basic_tileindex(index_filename, src_ds)
    del index_ds

    with pytest.raises(Exception):
        gdal.Open(index_filename)

    vrt_ds = gdal.OpenEx(index_filename, allowed_drivers=["GTI"])
    assert vrt_ds.GetDriver().GetDescription() == "GTI"
    check_basic(vrt_ds, src_ds)
    assert (
        vrt_ds.GetMetadataItem("SCANNED_ONE_FEATURE_AT_OPENING", "__DEBUG__") == "YES"
    )
    assert vrt_ds.GetRasterBand(1).GetBlockSize() == [256, 256]
    assert vrt_ds.GetRasterBand(1).ReadBlock(0, 0)[0:20] == src_ds.GetRasterBand(
        1
    ).ReadRaster(0, 0, 20, 1)

    assert "byte.tif" in vrt_ds.GetRasterBand(1).GetMetadataItem(
        "Pixel_0_0", "LocationInfo"
    )
    assert vrt_ds.GetRasterBand(1).GetMetadataItem("foo") is None
    assert vrt_ds.GetRasterBand(1).GetMetadataItem("foo", "LocationInfo") is None
    assert vrt_ds.GetRasterBand(1).GetMetadataItem("Pixel_", "LocationInfo") is None
    assert vrt_ds.GetRasterBand(1).GetMetadataItem("Pixel_0", "LocationInfo") is None
    assert vrt_ds.GetRasterBand(1).GetMetadataItem("GeoPixel_", "LocationInfo") is None
    assert vrt_ds.GetRasterBand(1).GetMetadataItem("GeoPixel_0", "LocationInfo") is None

    assert vrt_ds.GetRasterBand(1).GetOverviewCount() == 0
    assert vrt_ds.GetRasterBand(1).GetOverview(-1) is None
    assert vrt_ds.GetRasterBand(1).GetOverview(0) is None


def test_gti_custom_metadata(tmp_vsimem):

    index_filename = str(tmp_vsimem / "index.gti.gpkg")

    src_ds = gdal.Open(os.path.join(os.getcwd(), "data", "byte.tif"))
    index_ds, lyr = create_basic_tileindex(index_filename, src_ds)
    lyr.SetMetadataItem("BLOCKXSIZE", "2")
    lyr.SetMetadataItem("BLOCKYSIZE", "4")
    lyr.SetMetadataItem("FOO", "BAR")
    del index_ds

    vrt_ds = gdal.Open(index_filename)
    check_basic(vrt_ds, src_ds, expected_md={"FOO": "BAR"})
    assert vrt_ds.GetRasterBand(1).GetBlockSize() == [2, 4]


def test_gti_cannot_open_index(tmp_vsimem):

    index_filename = str(tmp_vsimem / "index.gti.gpkg")
    f = gdal.VSIFOpenL(index_filename, "wb+")
    assert f
    gdal.VSIFTruncateL(f, 100)
    gdal.VSIFCloseL(f)

    with pytest.raises(
        Exception, match="not recognized as being in a supported file format"
    ):
        gdal.Open(index_filename)


def test_gti_several_layers(tmp_vsimem):

    index_filename = str(tmp_vsimem / "index.gti.gpkg")

    src_ds = gdal.Open(os.path.join(os.getcwd(), "data", "byte.tif"))
    index_ds, _ = create_basic_tileindex(index_filename, src_ds)
    index_ds.CreateLayer("another_layer")
    del index_ds

    with pytest.raises(
        Exception,
        match="has more than one layer. TILE_INDEX_LAYER metadata item must be defined",
    ):
        gdal.Open(index_filename)

    with pytest.raises(
        Exception, match="has more than one layer. LAYER open option must be defined"
    ):
        gdal.Open("GTI:" + index_filename)

    assert (
        gdal.OpenEx("GTI:" + index_filename, open_options=["LAYER=index"]) is not None
    )

    index_ds = ogr.Open(index_filename, update=1)
    index_ds.SetMetadataItem("TILE_INDEX_LAYER", "index")
    del index_ds

    vrt_ds = gdal.Open(index_filename)
    check_basic(vrt_ds, src_ds)
    assert (
        vrt_ds.GetMetadataItem("SCANNED_ONE_FEATURE_AT_OPENING", "__DEBUG__") == "YES"
    )


def test_gti_no_metadata_several_layers_wrong_TILE_INDEX_LAYER(
    tmp_vsimem,
):

    index_filename = str(tmp_vsimem / "index.gti.gpkg")

    src_ds = gdal.Open(os.path.join(os.getcwd(), "data", "byte.tif"))
    index_ds, _ = create_basic_tileindex(index_filename, src_ds)
    index_ds.CreateLayer("another_layer")
    index_ds.SetMetadataItem("TILE_INDEX_LAYER", "wrong")
    del index_ds

    with pytest.raises(Exception, match="Layer wrong does not exist"):
        gdal.Open(index_filename)


def test_gti_no_layer(tmp_vsimem):

    index_filename = str(tmp_vsimem / "index.gti.gpkg")
    ogr.GetDriverByName("GPKG").CreateDataSource(index_filename)

    with pytest.raises(Exception, match="has no vector layer"):
        gdal.Open(index_filename, gdal.GA_Update)


def test_gti_no_feature(tmp_vsimem):

    index_filename = str(tmp_vsimem / "index.gti.gpkg")
    index_ds = ogr.GetDriverByName("GPKG").CreateDataSource(index_filename)
    lyr = index_ds.CreateLayer("index")
    lyr.CreateField(ogr.FieldDefn("location"))
    del index_ds

    with pytest.raises(Exception, match="metadata items missing"):
        gdal.Open(index_filename)


def test_gti_location_wrong_type(tmp_vsimem):

    index_filename = str(tmp_vsimem / "index.gti.gpkg")
    index_ds = ogr.GetDriverByName("GPKG").CreateDataSource(index_filename)
    lyr = index_ds.CreateLayer("index")
    lyr.CreateField(ogr.FieldDefn("location", ogr.OFTInteger))
    del index_ds

    with pytest.raises(Exception, match="Field location is not of type string"):
        gdal.Open(index_filename)


def test_gti_wrong_prototype_tile(tmp_vsimem):

    index_filename = str(tmp_vsimem / "index.gti.gpkg")
    index_ds = ogr.GetDriverByName("GPKG").CreateDataSource(index_filename)
    lyr = index_ds.CreateLayer("index")
    lyr.CreateField(ogr.FieldDefn("location"))
    f = ogr.Feature(lyr.GetLayerDefn())
    f["location"] = "/i/do/not/exist"
    lyr.CreateFeature(f)
    del index_ds

    # no match, because error message is operating system dependent
    with pytest.raises(Exception):
        gdal.Open(index_filename)


def test_gti_prototype_tile_no_gt(tmp_vsimem):

    index_filename = str(tmp_vsimem / "index.gti.gpkg")
    protods_filename = str(tmp_vsimem / "protods_filename.tif")
    ds = gdal.GetDriverByName("GTiff").Create(protods_filename, 1, 1)
    del ds
    index_ds = ogr.GetDriverByName("GPKG").CreateDataSource(index_filename)
    lyr = index_ds.CreateLayer("index")
    lyr.CreateField(ogr.FieldDefn("location"))
    f = ogr.Feature(lyr.GetLayerDefn())
    f["location"] = protods_filename
    lyr.CreateFeature(f)
    del index_ds

    with pytest.raises(Exception, match="Cannot find geotransform"):
        gdal.Open(index_filename)


def test_gti_prototype_tile_wrong_gt_3rd_value(tmp_vsimem):

    index_filename = str(tmp_vsimem / "index.gti.gpkg")
    protods_filename = str(tmp_vsimem / "protods_filename.tif")
    ds = gdal.GetDriverByName("GTiff").Create(protods_filename, 1, 1)
    del ds
    ds = gdal.Open(protods_filename)
    ds.SetGeoTransform([0, 0, 1234, 0, 0, -1])
    del ds
    index_ds = ogr.GetDriverByName("GPKG").CreateDataSource(index_filename)
    lyr = index_ds.CreateLayer("index")
    lyr.CreateField(ogr.FieldDefn("location"))
    f = ogr.Feature(lyr.GetLayerDefn())
    f["location"] = protods_filename
    lyr.CreateFeature(f)
    del index_ds

    with pytest.raises(Exception, match="3rd value of GeoTransform"):
        gdal.Open(index_filename)


def test_gti_prototype_tile_wrong_gt_5th_value(tmp_vsimem):

    index_filename = str(tmp_vsimem / "index.gti.gpkg")
    protods_filename = str(tmp_vsimem / "protods_filename.tif")
    ds = gdal.GetDriverByName("GTiff").Create(protods_filename, 1, 1)
    del ds
    ds = gdal.Open(protods_filename)
    ds.SetGeoTransform([0, 0, 0, 0, 1, -1])
    del ds
    index_ds = ogr.GetDriverByName("GPKG").CreateDataSource(index_filename)
    lyr = index_ds.CreateLayer("index")
    lyr.CreateField(ogr.FieldDefn("location"))
    f = ogr.Feature(lyr.GetLayerDefn())
    f["location"] = protods_filename
    lyr.CreateFeature(f)
    del index_ds

    with pytest.raises(Exception, match="5th value of GeoTransform"):
        gdal.Open(index_filename)


def test_gti_prototype_tile_wrong_gt_2nd_value(tmp_vsimem):

    index_filename = str(tmp_vsimem / "index.gti.gpkg")
    protods_filename = str(tmp_vsimem / "protods_filename.tif")
    ds = gdal.GetDriverByName("GTiff").Create(protods_filename, 1, 1)
    del ds
    ds = gdal.Open(protods_filename)
    ds.SetGeoTransform([0, 0, 0, 0, 0, 1])
    del ds
    index_ds = ogr.GetDriverByName("GPKG").CreateDataSource(index_filename)
    lyr = index_ds.CreateLayer("index")
    lyr.CreateField(ogr.FieldDefn("location"))
    f = ogr.Feature(lyr.GetLayerDefn())
    f["location"] = protods_filename
    lyr.CreateFeature(f)
    del index_ds

    with pytest.raises(Exception, match="2nd value of GeoTransform"):
        gdal.Open(index_filename)


def test_gti_no_extent(tmp_vsimem):

    index_filename = str(tmp_vsimem / "index.gti.gpkg")
    protods_filename = str(tmp_vsimem / "protods_filename.tif")
    ds = gdal.GetDriverByName("GTiff").Create(protods_filename, 1, 1)
    ds.SetGeoTransform([0, 1, 0, 0, 0, -1])
    del ds
    index_ds = ogr.GetDriverByName("GPKG").CreateDataSource(index_filename)
    lyr = index_ds.CreateLayer("index")
    lyr.CreateField(ogr.FieldDefn("location"))
    f = ogr.Feature(lyr.GetLayerDefn())
    f["location"] = protods_filename
    lyr.CreateFeature(f)
    del index_ds

    with pytest.raises(Exception, match="Cannot get layer extent"):
        gdal.Open(index_filename)


def test_gti_south_up(tmp_vsimem):

    index_filename = str(tmp_vsimem / "index.gti.gpkg")
    protods_filename = str(tmp_vsimem / "protods_filename.tif")
    with gdal.GetDriverByName("GTiff").Create(protods_filename, 1, 2) as ds:
        ds.SetGeoTransform([0, 1, 0, 10, 0, 1])
        ds.WriteRaster(0, 0, 1, 2, b"\x01\x02")

    index_ds = ogr.GetDriverByName("GPKG").CreateDataSource(index_filename)
    lyr = index_ds.CreateLayer("index")
    lyr.CreateField(ogr.FieldDefn("location"))
    f = ogr.Feature(lyr.GetLayerDefn())
    f["location"] = protods_filename
    f.SetGeometry(ogr.CreateGeometryFromWkt("POLYGON((0 10,0 12,1 12,1 12,0 12))"))
    lyr.CreateFeature(f)
    del index_ds

    ds = gdal.Open(index_filename)
    assert ds.GetGeoTransform() == (0, 1, 0, 12, 0, -1)
    assert ds.ReadRaster() == b"\x02\x01"


def test_gti_too_big_x(tmp_vsimem):

    index_filename = str(tmp_vsimem / "index.gti.gpkg")
    protods_filename = str(tmp_vsimem / "protods_filename.tif")
    ds = gdal.GetDriverByName("GTiff").Create(protods_filename, 1, 1)
    ds.SetGeoTransform([0, 1e-30, 0, 0, 0, -1])
    del ds
    index_ds = ogr.GetDriverByName("GPKG").CreateDataSource(index_filename)
    lyr = index_ds.CreateLayer("index")
    lyr.CreateField(ogr.FieldDefn("location"))
    f = ogr.Feature(lyr.GetLayerDefn())
    f["location"] = protods_filename
    f.SetGeometry(ogr.CreateGeometryFromWkt("POLYGON((0 0,0 1,1 1,1 0,0 0))"))
    lyr.CreateFeature(f)
    del index_ds

    with pytest.raises(Exception, match="Too small RESX, or wrong layer extent"):
        gdal.Open(index_filename)


def test_gti_too_big_y(tmp_vsimem):

    index_filename = str(tmp_vsimem / "index.gti.gpkg")
    protods_filename = str(tmp_vsimem / "protods_filename.tif")
    ds = gdal.GetDriverByName("GTiff").Create(protods_filename, 1, 1)
    ds.SetGeoTransform([0, 1, 0, 0, 0, -1e-30])
    del ds
    index_ds = ogr.GetDriverByName("GPKG").CreateDataSource(index_filename)
    lyr = index_ds.CreateLayer("index")
    lyr.CreateField(ogr.FieldDefn("location"))
    f = ogr.Feature(lyr.GetLayerDefn())
    f["location"] = protods_filename
    f.SetGeometry(ogr.CreateGeometryFromWkt("POLYGON((0 0,0 1,1 1,1 0,0 0))"))
    lyr.CreateFeature(f)
    del index_ds

    with pytest.raises(Exception, match="Too small RESY, or wrong layer extent"):
        gdal.Open(index_filename)


def test_gti_location_field_missing(tmp_vsimem):

    index_filename = str(tmp_vsimem / "index.gti.gpkg")

    # Missing LOCATION_FIELD and non-default location field name
    src_ds = gdal.Open(os.path.join(os.getcwd(), "data", "byte.tif"))
    index_ds, _ = create_basic_tileindex(
        index_filename, src_ds, location_field_name="path"
    )
    del index_ds

    with pytest.raises(Exception, match="Cannot find field location"):
        gdal.Open(index_filename)


def test_gti_location_field_set(tmp_vsimem):

    index_filename = str(tmp_vsimem / "index.gti.gpkg")

    # LOCATION_FIELD set
    src_ds = gdal.Open(os.path.join(os.getcwd(), "data", "byte.tif"))
    index_ds, lyr = create_basic_tileindex(
        index_filename, src_ds, location_field_name="path"
    )
    lyr.SetMetadataItem("LOCATION_FIELD", "path")
    del index_ds

    vrt_ds = gdal.Open(index_filename)
    check_basic(vrt_ds, src_ds)


@pytest.mark.parametrize("missing_item", ["RESX", "RESY"])
def test_gti_resx_resy(tmp_vsimem, missing_item):

    index_filename = str(tmp_vsimem / "index.gti.gpkg")

    src_ds = gdal.Open(os.path.join(os.getcwd(), "data", "byte.tif"))
    index_ds, lyr = create_basic_tileindex(index_filename, src_ds)
    if missing_item != "RESX":
        lyr.SetMetadataItem("RESX", "60")
    if missing_item != "RESY":
        lyr.SetMetadataItem("RESY", "60")
    lyr.SetMetadataItem("BAND_COUNT", "1")
    lyr.SetMetadataItem("DATA_TYPE", "UInt16")
    lyr.SetMetadataItem("COLOR_INTERPRETATION", "Green")
    del index_ds

    if missing_item is None:
        vrt_ds = gdal.Open(index_filename)
        check_basic(
            vrt_ds,
            src_ds,
            expected_dt=gdal.GDT_UInt16,
            expected_colorinterp=gdal.GCI_GreenBand,
        )
        assert (
            vrt_ds.GetMetadataItem("SCANNED_ONE_FEATURE_AT_OPENING", "__DEBUG__")
            == "NO"
        )
    else:
        with pytest.raises(Exception, match=missing_item):
            gdal.Open(index_filename)


@pytest.mark.parametrize("missing_item", [None, "XSIZE", "YSIZE", "GEOTRANSFORM"])
def test_gti_width_height_geotransform(tmp_vsimem, missing_item):

    index_filename = str(tmp_vsimem / "index.gti.gpkg")

    src_ds = gdal.Open(os.path.join(os.getcwd(), "data", "byte.tif"))
    index_ds, lyr = create_basic_tileindex(index_filename, src_ds)
    gt = src_ds.GetGeoTransform()
    if missing_item != "XSIZE":
        lyr.SetMetadataItem("XSIZE", "20")
    if missing_item != "YSIZE":
        lyr.SetMetadataItem("YSIZE", "20")
    if missing_item != "GEOTRANSFORM":
        lyr.SetMetadataItem("GEOTRANSFORM", ",".join([str(x) for x in gt]))
    lyr.SetMetadataItem("BAND_COUNT", "1")
    lyr.SetMetadataItem("DATA_TYPE", "Byte")
    lyr.SetMetadataItem("COLOR_INTERPRETATION", "Gray")
    del index_ds

    if missing_item is None:
        vrt_ds = gdal.Open(index_filename)
        assert (
            vrt_ds.GetMetadataItem("SCANNED_ONE_FEATURE_AT_OPENING", "__DEBUG__")
            == "NO"
        )
        check_basic(vrt_ds, src_ds)
    else:
        with pytest.raises(Exception, match=missing_item):
            gdal.Open(index_filename)


def test_gti_wrong_width(tmp_vsimem):

    index_filename = str(tmp_vsimem / "index.gti.gpkg")

    src_ds = gdal.Open(os.path.join(os.getcwd(), "data", "byte.tif"))
    index_ds, lyr = create_basic_tileindex(index_filename, src_ds)
    gt = src_ds.GetGeoTransform()
    lyr.SetMetadataItem("XSIZE", "0")
    lyr.SetMetadataItem("YSIZE", "20")
    lyr.SetMetadataItem("GEOTRANSFORM", ",".join([str(x) for x in gt]))
    del index_ds

    with pytest.raises(Exception, match="XSIZE metadata item must be > 0"):
        gdal.Open(index_filename)


def test_gti_wrong_height(tmp_vsimem):

    index_filename = str(tmp_vsimem / "index.gti.gpkg")

    src_ds = gdal.Open(os.path.join(os.getcwd(), "data", "byte.tif"))
    index_ds, lyr = create_basic_tileindex(index_filename, src_ds)
    gt = src_ds.GetGeoTransform()
    lyr.SetMetadataItem("XSIZE", "20")
    lyr.SetMetadataItem("YSIZE", "0")
    lyr.SetMetadataItem("GEOTRANSFORM", ",".join([str(x) for x in gt]))
    del index_ds

    with pytest.raises(Exception, match="YSIZE metadata item must be > 0"):
        gdal.Open(index_filename)


def test_gti_wrong_blockxsize(tmp_vsimem):

    index_filename = str(tmp_vsimem / "index.gti.gpkg")

    src_ds = gdal.Open(os.path.join(os.getcwd(), "data", "byte.tif"))
    index_ds, lyr = create_basic_tileindex(index_filename, src_ds)
    lyr.SetMetadataItem("BLOCKXSIZE", "0")
    del index_ds

    with pytest.raises(Exception, match="Invalid BLOCKXSIZE"):
        gdal.Open(index_filename)


def test_gti_wrong_blockysize(tmp_vsimem):

    index_filename = str(tmp_vsimem / "index.gti.gpkg")

    src_ds = gdal.Open(os.path.join(os.getcwd(), "data", "byte.tif"))
    index_ds, lyr = create_basic_tileindex(index_filename, src_ds)
    lyr.SetMetadataItem("BLOCKYSIZE", "0")
    del index_ds

    with pytest.raises(Exception, match="Invalid BLOCKYSIZE"):
        gdal.Open(index_filename)


def test_gti_wrong_blockxsize_blockysize(tmp_vsimem):

    index_filename = str(tmp_vsimem / "index.gti.gpkg")

    src_ds = gdal.Open(os.path.join(os.getcwd(), "data", "byte.tif"))
    index_ds, lyr = create_basic_tileindex(index_filename, src_ds)
    lyr.SetMetadataItem("BLOCKXSIZE", "50000")
    lyr.SetMetadataItem("BLOCKYSIZE", "50000")
    del index_ds

    with pytest.raises(Exception, match=r"Too big BLOCKXSIZE \* BLOCKYSIZE"):
        gdal.Open(index_filename)


def test_gti_wrong_gt(tmp_vsimem):

    index_filename = str(tmp_vsimem / "index.gti.gpkg")

    src_ds = gdal.Open(os.path.join(os.getcwd(), "data", "byte.tif"))
    index_ds, lyr = create_basic_tileindex(index_filename, src_ds)
    lyr.SetMetadataItem("XSIZE", "20")
    lyr.SetMetadataItem("YSIZE", "20")
    lyr.SetMetadataItem("GEOTRANSFORM", "0,1,0,0,0")
    del index_ds

    with pytest.raises(
        Exception,
        match="GEOTRANSFORM metadata item must be 6 numeric values separated with comma",
    ):
        gdal.Open(index_filename)


def test_gti_wrong_gt_3rd_term(tmp_vsimem):

    index_filename = str(tmp_vsimem / "index.gti.gpkg")

    src_ds = gdal.Open(os.path.join(os.getcwd(), "data", "byte.tif"))
    index_ds, lyr = create_basic_tileindex(index_filename, src_ds)
    lyr.SetMetadataItem("XSIZE", "20")
    lyr.SetMetadataItem("YSIZE", "20")
    lyr.SetMetadataItem("GEOTRANSFORM", "0,1,123,0,0,-1")
    del index_ds

    with pytest.raises(Exception, match="3rd value of GEOTRANSFORM must be 0"):
        gdal.Open(index_filename)


def test_gti_wrong_gt_5th_term(tmp_vsimem):

    index_filename = str(tmp_vsimem / "index.gti.gpkg")

    src_ds = gdal.Open(os.path.join(os.getcwd(), "data", "byte.tif"))
    index_ds, lyr = create_basic_tileindex(index_filename, src_ds)
    lyr.SetMetadataItem("XSIZE", "20")
    lyr.SetMetadataItem("YSIZE", "20")
    lyr.SetMetadataItem("GEOTRANSFORM", "0,1,0,0,1234,-1")
    del index_ds

    with pytest.raises(Exception, match="5th value of GEOTRANSFORM must be 0"):
        gdal.Open(index_filename)


def test_gti_wrong_gt_6th_term(tmp_vsimem):

    index_filename = str(tmp_vsimem / "index.gti.gpkg")

    src_ds = gdal.Open(os.path.join(os.getcwd(), "data", "byte.tif"))
    index_ds, lyr = create_basic_tileindex(index_filename, src_ds)
    lyr.SetMetadataItem("XSIZE", "20")
    lyr.SetMetadataItem("YSIZE", "20")
    lyr.SetMetadataItem("GEOTRANSFORM", "0,1,0,0,0,1")
    del index_ds

    with pytest.raises(Exception, match="6th value of GEOTRANSFORM must be < 0"):
        gdal.Open(index_filename)


@pytest.mark.parametrize("missing_item", [None, "MINX", "MINY", "MAXX", "MAXY"])
def test_gti_minx_miny_maxx_maxy(tmp_vsimem, missing_item):

    index_filename = str(tmp_vsimem / "index.gti.gpkg")

    src_ds = gdal.Open(os.path.join(os.getcwd(), "data", "byte.tif"))
    index_ds, lyr = create_basic_tileindex(index_filename, src_ds)
    gt = src_ds.GetGeoTransform()
    lyr.SetMetadataItem("RESX", "30")
    lyr.SetMetadataItem("RESY", "15")
    lyr.SetMetadataItem("BAND_COUNT", "1")
    lyr.SetMetadataItem("DATA_TYPE", "Byte")
    lyr.SetMetadataItem("COLOR_INTERPRETATION", "Undefined")
    if missing_item != "MINX":
        lyr.SetMetadataItem("MINX", str(gt[0] + 1 * gt[1]))
    if missing_item != "MINY":
        lyr.SetMetadataItem("MINY", str(gt[3] + (src_ds.RasterYSize - 4) * gt[5]))
    if missing_item != "MAXX":
        lyr.SetMetadataItem("MAXX", str(gt[0] + (src_ds.RasterXSize - 3) * gt[1]))
    if missing_item != "MAXY":
        lyr.SetMetadataItem("MAXY", str(gt[3] + 2 * gt[5]))
    lyr.SetMetadataItem("RESAMPLING", "NEAREST")
    del index_ds

    if missing_item is None:
        vrt_ds = gdal.Open(index_filename)
        assert (
            vrt_ds.GetMetadataItem("SCANNED_ONE_FEATURE_AT_OPENING", "__DEBUG__")
            == "NO"
        )
        assert vrt_ds.RasterXSize == 2 * (src_ds.RasterXSize - 3 - 1)
        assert vrt_ds.RasterYSize == 4 * (src_ds.RasterYSize - 4 - 2)
        assert vrt_ds.ReadRaster() == src_ds.ReadRaster(
            1,
            2,
            vrt_ds.RasterXSize // 2,
            vrt_ds.RasterYSize // 4,
            buf_xsize=vrt_ds.RasterXSize,
            buf_ysize=vrt_ds.RasterYSize,
        )
    else:
        with pytest.raises(Exception, match=missing_item):
            gdal.Open(index_filename)


def test_gti_wrong_resx(tmp_vsimem):

    index_filename = str(tmp_vsimem / "index.gti.gpkg")

    src_ds = gdal.Open(os.path.join(os.getcwd(), "data", "byte.tif"))
    index_ds, lyr = create_basic_tileindex(index_filename, src_ds)
    lyr.SetMetadataItem("RESX", "0")
    lyr.SetMetadataItem("RESY", "1")
    del index_ds

    with pytest.raises(Exception, match="RESX metadata item must be > 0"):
        gdal.Open(index_filename)


def test_gti_wrong_resy(tmp_vsimem):

    index_filename = str(tmp_vsimem / "index.gti.gpkg")

    src_ds = gdal.Open(os.path.join(os.getcwd(), "data", "byte.tif"))
    index_ds, lyr = create_basic_tileindex(index_filename, src_ds)
    lyr.SetMetadataItem("RESX", "1")
    lyr.SetMetadataItem("RESY", "0")
    del index_ds

    with pytest.raises(Exception, match="RESY metadata item must be > 0"):
        gdal.Open(index_filename)


def test_gti_wrong_minx(tmp_vsimem):

    index_filename = str(tmp_vsimem / "index.gti.gpkg")

    src_ds = gdal.Open(os.path.join(os.getcwd(), "data", "byte.tif"))
    index_ds, lyr = create_basic_tileindex(index_filename, src_ds)
    gt = src_ds.GetGeoTransform()
    lyr.SetMetadataItem("RESX", "60")
    lyr.SetMetadataItem("RESY", "60")
    # lyr.SetMetadataItem("MINX", str(gt[0]))
    lyr.SetMetadataItem("MINX", str(1e10))
    lyr.SetMetadataItem("MINY", str(gt[3] + src_ds.RasterYSize * gt[5]))
    lyr.SetMetadataItem("MAXX", str(gt[0] + src_ds.RasterXSize * gt[1]))
    lyr.SetMetadataItem("MAXY", str(gt[3]))
    del index_ds

    with pytest.raises(Exception, match="MAXX metadata item must be > MINX"):
        gdal.Open(index_filename)


def test_gti_wrong_miny(tmp_vsimem):

    index_filename = str(tmp_vsimem / "index.gti.gpkg")

    src_ds = gdal.Open(os.path.join(os.getcwd(), "data", "byte.tif"))
    index_ds, lyr = create_basic_tileindex(index_filename, src_ds)
    gt = src_ds.GetGeoTransform()
    lyr.SetMetadataItem("RESX", "60")
    lyr.SetMetadataItem("RESY", "60")
    lyr.SetMetadataItem("MINX", str(gt[0]))
    # lyr.SetMetadataItem("MINY", str(gt[3] + src_ds.RasterYSize * gt[5]))
    lyr.SetMetadataItem("MINY", str(1e10))
    lyr.SetMetadataItem("MAXX", str(gt[0] + src_ds.RasterXSize * gt[1]))
    lyr.SetMetadataItem("MAXY", str(gt[3]))
    del index_ds

    with pytest.raises(Exception, match="MAXY metadata item must be > MINY"):
        gdal.Open(index_filename)


def test_gti_wrong_resx_wrt_min_max_xy(tmp_vsimem):

    index_filename = str(tmp_vsimem / "index.gti.gpkg")

    src_ds = gdal.Open(os.path.join(os.getcwd(), "data", "byte.tif"))
    index_ds, lyr = create_basic_tileindex(index_filename, src_ds)
    gt = src_ds.GetGeoTransform()
    lyr.SetMetadataItem("RESX", "1e-10")
    lyr.SetMetadataItem("RESY", "60")
    lyr.SetMetadataItem("MINX", str(gt[0]))
    lyr.SetMetadataItem("MINY", str(gt[3] + src_ds.RasterYSize * gt[5]))
    lyr.SetMetadataItem("MAXX", str(gt[0] + src_ds.RasterXSize * gt[1]))
    lyr.SetMetadataItem("MAXY", str(gt[3]))
    del index_ds

    with pytest.raises(Exception, match="Too small RESX, or wrong layer extent"):
        gdal.Open(index_filename)


def test_gti_wrong_resy_wrt_min_max_xy(tmp_vsimem):

    index_filename = str(tmp_vsimem / "index.gti.gpkg")

    src_ds = gdal.Open(os.path.join(os.getcwd(), "data", "byte.tif"))
    index_ds, lyr = create_basic_tileindex(index_filename, src_ds)
    gt = src_ds.GetGeoTransform()
    lyr.SetMetadataItem("RESX", "60")
    lyr.SetMetadataItem("RESY", "1e-10")
    lyr.SetMetadataItem("MINX", str(gt[0]))
    lyr.SetMetadataItem("MINY", str(gt[3] + src_ds.RasterYSize * gt[5]))
    lyr.SetMetadataItem("MAXX", str(gt[0] + src_ds.RasterXSize * gt[1]))
    lyr.SetMetadataItem("MAXY", str(gt[3]))
    del index_ds

    with pytest.raises(Exception, match="Too small RESY, or wrong layer extent"):
        gdal.Open(index_filename)


def test_gti_invalid_srs(tmp_vsimem):

    index_filename = str(tmp_vsimem / "index.gti.gpkg")

    src_ds = gdal.Open(os.path.join(os.getcwd(), "data", "byte.tif"))
    index_ds, lyr = create_basic_tileindex(index_filename, src_ds)
    lyr.SetMetadataItem("SRS", "invalid")
    del index_ds

    with pytest.raises(Exception, match="Invalid SRS"):
        gdal.Open(index_filename)


def test_gti_valid_srs(tmp_path):

    index_filename = str(tmp_path / "index.gti.gpkg")

    src_ds = gdal.Open(os.path.join(os.getcwd(), "data", "byte.tif"))
    index_ds, lyr = create_basic_tileindex(index_filename, src_ds)
    lyr.SetMetadataItem("SRS", "EPSG:4267")
    del index_ds

    ds = gdal.Open(index_filename)
    assert ds.GetSpatialRef().GetAuthorityCode(None) == "4267"


def test_gti_invalid_band_count(tmp_vsimem):

    index_filename = str(tmp_vsimem / "index.gti.gpkg")

    src_ds = gdal.Open(os.path.join(os.getcwd(), "data", "byte.tif"))
    index_ds, lyr = create_basic_tileindex(index_filename, src_ds)
    lyr.SetMetadataItem("BAND_COUNT", "0")
    del index_ds

    with pytest.raises(Exception, match="Invalid band count"):
        gdal.Open(index_filename)


@pytest.mark.parametrize(
    "md,error_msg",
    [
        (
            {"BAND_COUNT": "2", "COLOR_INTERPRETATION": "Undefined", "NODATA": "None"},
            "Number of data types values found not matching number of bands",
        ),
        (
            {
                "BAND_COUNT": "2",
                "COLOR_INTERPRETATION": "Undefined",
                "DATA_TYPE": "Byte",
            },
            "Number of nodata values found not matching number of bands",
        ),
        (
            {"BAND_COUNT": "2", "DATA_TYPE": "Byte", "NODATA": "None"},
            "Number of color interpretation values found not matching number of bands",
        ),
    ],
)
def test_gti_inconsistent_number_of_values(tmp_vsimem, md, error_msg):

    index_filename = str(tmp_vsimem / "index.gti.gpkg")

    src_ds = gdal.Open(os.path.join(os.getcwd(), "data", "byte.tif"))
    index_ds, lyr = create_basic_tileindex(index_filename, src_ds)
    for item in md:
        lyr.SetMetadataItem(item, md[item])
    del index_ds

    with pytest.raises(Exception, match=error_msg):
        gdal.Open(index_filename)


@pytest.mark.parametrize(
    "md,expected_nodata",
    [
        ({}, [None]),
        ({"NODATA": "none"}, [None]),
        ({"NODATA": "1"}, [1]),
        ({"NODATA": "1.5"}, [1.5]),
        ({"NODATA": "inf"}, [float("inf")]),
        ({"NODATA": "-inf"}, [float("-inf")]),
        ({"NODATA": "nan"}, [float("nan")]),
        (
            {
                "BAND_COUNT": "6",
                "COLOR_INTERPRETATION": "Undefined",
                "DATA_TYPE": "Byte",
                "NODATA": "1,2,none,-inf,inf,nan",
            },
            [1, 2, None, float("-inf"), float("inf"), float("nan")],
        ),
    ],
)
def test_gti_valid_nodata(tmp_vsimem, md, expected_nodata):

    index_filename = str(tmp_vsimem / "index.gti.gpkg")

    src_ds = gdal.Open(os.path.join(os.getcwd(), "data", "byte.tif"))
    index_ds, lyr = create_basic_tileindex(index_filename, src_ds)
    for item in md:
        lyr.SetMetadataItem(item, md[item])
    del index_ds

    ds = gdal.Open(index_filename)
    for i in range(ds.RasterCount):
        got_nd = ds.GetRasterBand(i + 1).GetNoDataValue()
        if expected_nodata[i] is None:
            assert got_nd is None
        elif math.isnan(expected_nodata[i]):
            assert math.isnan(got_nd)
        else:
            assert got_nd == expected_nodata[i]


@pytest.mark.parametrize(
    "md,error_msg",
    [
        ({"NODATA": "invalid"}, "Invalid value for NODATA"),
        ({"BAND_COUNT": "2", "NODATA": "0,invalid"}, "Invalid value for NODATA"),
        (
            {"BAND_COUNT": "2", "NODATA": "0,0,0"},
            "Number of values in NODATA must be 1 or BAND_COUNT",
        ),
    ],
)
def test_gti_invalid_nodata(tmp_vsimem, md, error_msg):

    index_filename = str(tmp_vsimem / "index.gti.gpkg")

    src_ds = gdal.Open(os.path.join(os.getcwd(), "data", "byte.tif"))
    index_ds, lyr = create_basic_tileindex(index_filename, src_ds)
    for item in md:
        lyr.SetMetadataItem(item, md[item])
    del index_ds

    with pytest.raises(Exception, match=error_msg):
        gdal.Open(index_filename)


@pytest.mark.parametrize(
    "md,error_msg",
    [
        ({"DATA_TYPE": "invalid"}, "Invalid value for DATA_TYPE"),
        (
            {"BAND_COUNT": "2", "DATA_TYPE": "byte,invalid"},
            "Invalid value for DATA_TYPE",
        ),
        (
            {"BAND_COUNT": "2", "DATA_TYPE": "byte,byte,byte"},
            "Number of values in DATA_TYPE must be 1 or BAND_COUNT",
        ),
    ],
)
def test_gti_invalid_data_type(tmp_vsimem, md, error_msg):

    index_filename = str(tmp_vsimem / "index.gti.gpkg")

    src_ds = gdal.Open(os.path.join(os.getcwd(), "data", "byte.tif"))
    index_ds, lyr = create_basic_tileindex(index_filename, src_ds)
    for item in md:
        lyr.SetMetadataItem(item, md[item])
    del index_ds

    with pytest.raises(Exception, match=error_msg):
        gdal.Open(index_filename)


@pytest.mark.parametrize(
    "md,error_msg",
    [
        ({"COLOR_INTERPRETATION": "invalid"}, "Invalid value for COLOR_INTERPRETATION"),
        (
            {"BAND_COUNT": "2", "COLOR_INTERPRETATION": "undefined,invalid"},
            "Invalid value for COLOR_INTERPRETATION",
        ),
        (
            {
                "BAND_COUNT": "2",
                "COLOR_INTERPRETATION": "undefined,undefined,undefined",
            },
            "Number of values in COLOR_INTERPRETATION must be 1 or BAND_COUNT",
        ),
    ],
)
def test_gti_invalid_color_interpretation(tmp_vsimem, md, error_msg):

    index_filename = str(tmp_vsimem / "index.gti.gpkg")

    src_ds = gdal.Open(os.path.join(os.getcwd(), "data", "byte.tif"))
    index_ds, lyr = create_basic_tileindex(index_filename, src_ds)
    for item in md:
        lyr.SetMetadataItem(item, md[item])
    del index_ds

    with pytest.raises(Exception, match=error_msg):
        gdal.Open(index_filename)


def test_gti_no_metadata_rgb(tmp_vsimem):

    index_filename = str(tmp_vsimem / "index.gti.gpkg")

    src_ds = gdal.Open(os.path.join(os.getcwd(), "data", "small_world.tif"))
    index_ds, _ = create_basic_tileindex(index_filename, src_ds)
    del index_ds

    vrt_ds = gdal.Open(index_filename)
    check_basic(vrt_ds, src_ds)


@pytest.mark.skipif(
    gdal.GetDriverByName("VRT").GetMetadataItem(gdal.DMD_OPENOPTIONLIST) is None,
    reason="VRT driver open missing",
)
def test_gti_rgb_left_right(tmp_vsimem):

    index_filename = str(tmp_vsimem / "index.gti.gpkg")

    src_ds = gdal.Open("data/small_world.tif")

    left_filename = str(tmp_vsimem / "left.tif")
    gdal.Translate(left_filename, src_ds, srcWin=[0, 0, 200, 200])

    right_filename = str(tmp_vsimem / "right.tif")
    gdal.Translate(right_filename, src_ds, srcWin=[200, 0, 200, 200])

    index_ds, _ = create_basic_tileindex(
        index_filename, [gdal.Open(left_filename), gdal.Open(right_filename)]
    )
    del index_ds

    vrt_ds = gdal.Open(index_filename)
    check_basic(vrt_ds, src_ds)

    index_ds, lyr = create_basic_tileindex(
        index_filename, [gdal.Open(left_filename), gdal.Open(right_filename)]
    )
    lyr.SetMetadataItem("NODATA", "255")
    del index_ds

    vrt_ds = gdal.Open(index_filename)
    assert vrt_ds.ReadRaster() == src_ds.ReadRaster()
    assert vrt_ds.GetMetadataItem("NUMBER_OF_CONTRIBUTING_SOURCES", "__DEBUG__") == "2"

    assert vrt_ds.ReadRaster(199, 100, 2, 1) == src_ds.ReadRaster(199, 100, 2, 1)
    assert vrt_ds.GetMetadataItem("NUMBER_OF_CONTRIBUTING_SOURCES", "__DEBUG__") == "2"

    assert vrt_ds.ReadRaster(0, 0, 200, 200) == src_ds.ReadRaster(0, 0, 200, 200)
    assert vrt_ds.GetMetadataItem("NUMBER_OF_CONTRIBUTING_SOURCES", "__DEBUG__") == "1"

    assert vrt_ds.ReadRaster(200, 0, 200, 200) == src_ds.ReadRaster(200, 0, 200, 200)
    assert vrt_ds.GetMetadataItem("NUMBER_OF_CONTRIBUTING_SOURCES", "__DEBUG__") == "1"

    assert (
        vrt_ds.GetRasterBand(1).GetMetadataItem("Pixel_0_0", "LocationInfo")
        == "<LocationInfo><File>/vsimem/test_gti_rgb_left_right/left.tif</File></LocationInfo>"
    )

    if ogrtest.have_geos():
        flags, pct = vrt_ds.GetRasterBand(1).GetDataCoverageStatus(
            0, 0, vrt_ds.RasterXSize, vrt_ds.RasterYSize
        )
        assert flags == gdal.GDAL_DATA_COVERAGE_STATUS_DATA and pct == 100.0

        flags, pct = vrt_ds.GetRasterBand(1).GetDataCoverageStatus(1, 2, 3, 4)
        assert flags == gdal.GDAL_DATA_COVERAGE_STATUS_DATA and pct == 100.0

        flags, pct = vrt_ds.GetRasterBand(1).GetDataCoverageStatus(
            vrt_ds.RasterXSize // 2 - 1, 2, 2, 4
        )
        assert flags == gdal.GDAL_DATA_COVERAGE_STATUS_DATA and pct == 100.0


@pytest.mark.skipif(
    gdal.GetDriverByName("VRT").GetMetadataItem(gdal.DMD_OPENOPTIONLIST) is None,
    reason="VRT driver open missing",
)
def test_gti_overlapping_sources(tmp_vsimem):

    filename1 = str(tmp_vsimem / "one.tif")
    ds = gdal.GetDriverByName("GTiff").Create(filename1, 1, 1)
    ds.SetGeoTransform([2, 1, 0, 49, 0, -1])
    ds.GetRasterBand(1).Fill(1)
    del ds

    filename2 = str(tmp_vsimem / "two.tif")
    ds = gdal.GetDriverByName("GTiff").Create(filename2, 1, 1)
    ds.SetGeoTransform([2, 1, 0, 49, 0, -1])
    ds.GetRasterBand(1).Fill(2)
    del ds

    # No sorting field: feature with max FID has the priority
    index_filename = str(tmp_vsimem / "index.gti.gpkg")
    index_ds, _ = create_basic_tileindex(
        index_filename, [gdal.Open(filename1), gdal.Open(filename2)]
    )
    del index_ds

    vrt_ds = gdal.Open(index_filename)
    assert vrt_ds.GetRasterBand(1).Checksum() == 2

    if ogrtest.have_geos():
        flags, pct = vrt_ds.GetRasterBand(1).GetDataCoverageStatus(
            0, 0, vrt_ds.RasterXSize, vrt_ds.RasterYSize
        )
        assert flags == gdal.GDAL_DATA_COVERAGE_STATUS_DATA and pct == 100.0

    # Test unsupported sort_field_type = OFTBinary
    index_filename = str(tmp_vsimem / "index.gti.gpkg")
    sort_values = [None, None]
    index_ds, _ = create_basic_tileindex(
        index_filename,
        [gdal.Open(filename1), gdal.Open(filename2)],
        sort_field_name="z_order",
        sort_field_type=ogr.OFTBinary,
        sort_values=sort_values,
    )
    del index_ds

    with pytest.raises(Exception, match="Unsupported type for field z_order"):
        gdal.Open(index_filename)
    gdal.Unlink(index_filename)

    # Test non existent SORT_FIELD
    index_filename = str(tmp_vsimem / "index.gti.gpkg")
    sort_values = [None, None]
    index_ds, lyr = create_basic_tileindex(index_filename, gdal.Open(filename1))
    lyr.SetMetadataItem("SORT_FIELD", "non_existing")
    del index_ds

    with pytest.raises(Exception, match="Cannot find field non_existing"):
        gdal.Open(index_filename)
    gdal.Unlink(index_filename)

    # Test sort_field_type = OFTString
    index_filename = str(tmp_vsimem / "index.gti.gpkg")
    sort_values = ["2", "1"]
    index_ds, _ = create_basic_tileindex(
        index_filename,
        [gdal.Open(filename1), gdal.Open(filename2)],
        sort_field_name="z_order",
        sort_field_type=ogr.OFTString,
        sort_values=sort_values,
    )
    del index_ds

    vrt_ds = gdal.Open(index_filename)
    assert vrt_ds.GetRasterBand(1).Checksum() == 1, sort_values

    sort_values.reverse()
    index_ds, _ = create_basic_tileindex(
        index_filename,
        [gdal.Open(filename2), gdal.Open(filename1)],
        sort_field_name="z_order",
        sort_field_type=ogr.OFTString,
        sort_values=sort_values,
    )
    del index_ds

    vrt_ds = gdal.Open(index_filename)
    assert vrt_ds.GetRasterBand(1).Checksum() == 1, sort_values

    # Test sort_field_type = OFTString
    index_filename = str(tmp_vsimem / "index.gti.gpkg")
    sort_values = ["1", "1"]
    index_ds, _ = create_basic_tileindex(
        index_filename,
        [gdal.Open(filename1), gdal.Open(filename2)],
        sort_field_name="z_order",
        sort_field_type=ogr.OFTString,
        sort_values=sort_values,
    )
    del index_ds

    vrt_ds = gdal.Open(index_filename)
    assert vrt_ds.GetRasterBand(1).Checksum() == 2, sort_values

    # Test sort_field_type = OFTInteger
    index_filename = str(tmp_vsimem / "index.gti.gpkg")
    sort_values = [2, 1]
    index_ds, _ = create_basic_tileindex(
        index_filename,
        [gdal.Open(filename1), gdal.Open(filename2)],
        sort_field_name="z_order",
        sort_field_type=ogr.OFTInteger,
        sort_values=sort_values,
    )
    del index_ds

    vrt_ds = gdal.Open(index_filename)
    assert vrt_ds.GetRasterBand(1).Checksum() == 1, sort_values

    sort_values.reverse()
    index_ds, _ = create_basic_tileindex(
        index_filename,
        [gdal.Open(filename2), gdal.Open(filename1)],
        sort_field_name="z_order",
        sort_field_type=ogr.OFTInteger,
        sort_values=sort_values,
    )
    del index_ds

    vrt_ds = gdal.Open(index_filename)
    assert vrt_ds.GetRasterBand(1).Checksum() == 1, sort_values

    # Test sort_field_type = OFTInteger64
    index_filename = str(tmp_vsimem / "index.gti.gpkg")
    sort_values = [1234567890123 + 2, 1234567890123 + 1]
    index_ds, _ = create_basic_tileindex(
        index_filename,
        [gdal.Open(filename1), gdal.Open(filename2)],
        sort_field_name="z_order",
        sort_field_type=ogr.OFTInteger64,
        sort_values=sort_values,
    )
    del index_ds

    vrt_ds = gdal.Open(index_filename)
    assert vrt_ds.GetRasterBand(1).Checksum() == 1, sort_values

    sort_values.reverse()
    index_ds, _ = create_basic_tileindex(
        index_filename,
        [gdal.Open(filename2), gdal.Open(filename1)],
        sort_field_name="z_order",
        sort_field_type=ogr.OFTInteger64,
        sort_values=sort_values,
    )
    del index_ds

    vrt_ds = gdal.Open(index_filename)
    assert vrt_ds.GetRasterBand(1).Checksum() == 1, sort_values

    # Test sort_field_type = OFTReal
    index_filename = str(tmp_vsimem / "index.gti.gpkg")
    sort_values = [2.5, 1.5]
    index_ds, _ = create_basic_tileindex(
        index_filename,
        [gdal.Open(filename1), gdal.Open(filename2)],
        sort_field_name="z_order",
        sort_field_type=ogr.OFTReal,
        sort_values=sort_values,
    )
    del index_ds

    vrt_ds = gdal.Open(index_filename)
    assert vrt_ds.GetRasterBand(1).Checksum() == 1, sort_values

    sort_values.reverse()
    index_ds, _ = create_basic_tileindex(
        index_filename,
        [gdal.Open(filename2), gdal.Open(filename1)],
        sort_field_name="z_order",
        sort_field_type=ogr.OFTReal,
        sort_values=sort_values,
    )
    del index_ds

    vrt_ds = gdal.Open(index_filename)
    assert vrt_ds.GetRasterBand(1).Checksum() == 1, sort_values

    # Test sort_field_type = OFTDate
    index_filename = str(tmp_vsimem / "index.gti.gpkg")
    for sort_values in [
        ["2023-01-01", "2022-12-31"],
        ["2023-02-01", "2023-01-31"],
        ["2023-01-02", "2023-01-01"],
    ]:
        index_ds, _ = create_basic_tileindex(
            index_filename,
            [gdal.Open(filename1), gdal.Open(filename2)],
            sort_field_name="z_order",
            sort_field_type=ogr.OFTDate,
            sort_values=sort_values,
        )
        del index_ds

        vrt_ds = gdal.Open(index_filename)
        assert vrt_ds.GetRasterBand(1).Checksum() == 1, sort_values

        sort_values.reverse()
        index_ds, _ = create_basic_tileindex(
            index_filename,
            [gdal.Open(filename2), gdal.Open(filename1)],
            sort_field_name="z_order",
            sort_field_type=ogr.OFTDate,
            sort_values=sort_values,
        )
        del index_ds

        vrt_ds = gdal.Open(index_filename)
        assert vrt_ds.GetRasterBand(1).Checksum() == 1, sort_values

    # Test sort_field_type = OFTDateTime
    index_filename = str(tmp_vsimem / "index.gti.gpkg")
    for sort_values in [
        ["2023-01-01T00:00:00", "2022-12-31T23:59:59"],
        ["2023-02-01T00:00:00", "2023-01-31T23:59:59"],
        ["2023-01-02T00:00:00", "2023-01-01T23:59:59"],
        ["2023-01-01T01:00:00", "2023-01-01T00:59:59"],
        ["2023-01-01T00:01:00", "2023-01-01T00:00:59"],
        ["2023-01-01T00:00:01", "2023-01-01T00:00:00"],
    ]:
        index_ds, _ = create_basic_tileindex(
            index_filename,
            [gdal.Open(filename1), gdal.Open(filename2)],
            sort_field_name="z_order",
            sort_field_type=ogr.OFTDateTime,
            sort_values=sort_values,
        )
        del index_ds

        vrt_ds = gdal.Open(index_filename)
        assert vrt_ds.GetRasterBand(1).Checksum() == 1, sort_values

        sort_values.reverse()
        index_ds, _ = create_basic_tileindex(
            index_filename,
            [gdal.Open(filename2), gdal.Open(filename1)],
            sort_field_name="z_order",
            sort_field_type=ogr.OFTDate,
            sort_values=sort_values,
        )
        del index_ds

        vrt_ds = gdal.Open(index_filename)
        assert vrt_ds.GetRasterBand(1).Checksum() == 1, sort_values

    # Test SORT_FIELD_ASC=NO
    index_filename = str(tmp_vsimem / "index.gti.gpkg")
    index_ds, lyr = create_basic_tileindex(
        index_filename,
        [gdal.Open(filename1), gdal.Open(filename2)],
        sort_field_name="z_order",
        sort_field_type=ogr.OFTInteger,
        sort_values=[2, 1],
    )
    lyr.SetMetadataItem("SORT_FIELD_ASC", "NO")
    del index_ds

    vrt_ds = gdal.Open(index_filename)
    assert vrt_ds.GetRasterBand(1).Checksum() == 2, sort_values


@pytest.mark.skipif(
    gdal.GetDriverByName("VRT").GetMetadataItem(gdal.DMD_OPENOPTIONLIST) is None,
    reason="VRT driver open missing",
)
def test_gti_gap_between_sources(tmp_vsimem):

    filename1 = str(tmp_vsimem / "one.tif")
    ds = gdal.GetDriverByName("GTiff").Create(filename1, 1, 1)
    ds.SetGeoTransform([2, 1, 0, 49, 0, -1])
    ds.GetRasterBand(1).Fill(1)
    del ds

    filename2 = str(tmp_vsimem / "two.tif")
    ds = gdal.GetDriverByName("GTiff").Create(filename2, 1, 1)
    ds.SetGeoTransform([4, 1, 0, 49, 0, -1])
    ds.GetRasterBand(1).Fill(2)
    del ds

    index_filename = str(tmp_vsimem / "index.gti.gpkg")
    index_ds, _ = create_basic_tileindex(
        index_filename, [gdal.Open(filename1), gdal.Open(filename2)]
    )
    del index_ds

    vrt_ds = gdal.Open(index_filename)
    assert vrt_ds.GetRasterBand(1).Checksum() == 3

    if ogrtest.have_geos():
        flags, pct = vrt_ds.GetRasterBand(1).GetDataCoverageStatus(
            0, 0, vrt_ds.RasterXSize, vrt_ds.RasterYSize
        )
        assert (
            flags
            == gdal.GDAL_DATA_COVERAGE_STATUS_DATA
            | gdal.GDAL_DATA_COVERAGE_STATUS_EMPTY
            and pct == pytest.approx(100.0 * 2 / 3)
        )


@pytest.mark.skipif(
    gdal.GetDriverByName("VRT").GetMetadataItem(gdal.DMD_OPENOPTIONLIST) is None,
    reason="VRT driver open missing",
)
def test_gti_no_source(tmp_vsimem):

    index_filename = str(tmp_vsimem / "index.gti.gpkg")
    index_ds, lyr = create_basic_tileindex(index_filename, [])
    lyr.SetMetadataItem("XSIZE", "2")
    lyr.SetMetadataItem("YSIZE", "3")
    lyr.SetMetadataItem("GEOTRANSFORM", "10,1,0,20,0,-1")
    lyr.SetMetadataItem("BAND_COUNT", "2")
    lyr.SetMetadataItem("NODATA", "255,254")
    del index_ds

    vrt_ds = gdal.Open(index_filename)
    assert vrt_ds.ReadRaster() == (b"\xff" * 6) + (b"\xfe" * 6)

    assert (
        vrt_ds.GetRasterBand(1).GetMetadataItem("Pixel_0_0", "LocationInfo")
        == "<LocationInfo></LocationInfo>"
    )
    assert (
        vrt_ds.GetRasterBand(1).GetMetadataItem("Pixel_1_2", "LocationInfo")
        == "<LocationInfo></LocationInfo>"
    )
    assert vrt_ds.GetRasterBand(1).GetMetadataItem("Pixel_-1_0", "LocationInfo") is None
    assert vrt_ds.GetRasterBand(1).GetMetadataItem("Pixel_0_-1", "LocationInfo") is None
    assert vrt_ds.GetRasterBand(1).GetMetadataItem("Pixel_1_3", "LocationInfo") is None
    assert vrt_ds.GetRasterBand(1).GetMetadataItem("Pixel_2_2", "LocationInfo") is None

    assert (
        vrt_ds.GetRasterBand(1).GetMetadataItem("GeoPixel_10_20", "LocationInfo")
        == "<LocationInfo></LocationInfo>"
    )
    assert (
        vrt_ds.GetRasterBand(1).GetMetadataItem("GeoPixel_9.99_20", "LocationInfo")
        is None
    )
    assert (
        vrt_ds.GetRasterBand(1).GetMetadataItem("GeoPixel_10_20.01", "LocationInfo")
        is None
    )

    if ogrtest.have_geos():
        flags, pct = vrt_ds.GetRasterBand(1).GetDataCoverageStatus(
            0, 0, vrt_ds.RasterXSize, vrt_ds.RasterYSize
        )
        assert flags == gdal.GDAL_DATA_COVERAGE_STATUS_EMPTY and pct == 0.0


def test_gti_invalid_source(tmp_vsimem):

    index_filename = str(tmp_vsimem / "index.gti.gpkg")
    index_ds = ogr.GetDriverByName("GPKG").CreateDataSource(index_filename)
    lyr = index_ds.CreateLayer("index", geom_type=ogr.wkbPolygon)
    lyr.CreateField(ogr.FieldDefn("location"))

    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetGeometry(ogr.CreateGeometryFromWkt("POLYGON((10 20,11 20,11 19,10 19,10 20))"))
    # Location not set
    lyr.CreateFeature(f)
    f = None

    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetGeometry(ogr.CreateGeometryFromWkt("POLYGON((10 20,11 20,11 19,10 19,10 20))"))
    # Invalid location
    f["location"] = "/i/do/not/exist"
    lyr.CreateFeature(f)
    f = None

    lyr.SetMetadataItem("XSIZE", "1")
    lyr.SetMetadataItem("YSIZE", "1")
    lyr.SetMetadataItem("GEOTRANSFORM", "10,1,0,20,0,-1")
    lyr.SetMetadataItem("BAND_COUNT", "1")
    del index_ds

    vrt_ds = gdal.Open(index_filename)
    with pytest.raises(Exception):
        vrt_ds.ReadRaster()


def test_gti_source_relative_location(tmp_vsimem):

    tile_filename = str(tmp_vsimem / "tile.tif")
    ds = gdal.GetDriverByName("GTiff").Create(tile_filename, 1, 1)
    ds.SetGeoTransform([10, 1, 0, 20, 0, -1])
    ds.GetRasterBand(1).Fill(255)
    del ds

    index_filename = str(tmp_vsimem / "index.gti.gpkg")
    index_ds = ogr.GetDriverByName("GPKG").CreateDataSource(index_filename)
    lyr = index_ds.CreateLayer("index", geom_type=ogr.wkbPolygon)
    lyr.CreateField(ogr.FieldDefn("location"))

    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetGeometry(ogr.CreateGeometryFromWkt("POLYGON((10 20,11 20,11 19,10 19,10 20))"))
    f["location"] = "tile.tif"
    lyr.CreateFeature(f)
    f = None

    lyr.SetMetadataItem("XSIZE", "1")
    lyr.SetMetadataItem("YSIZE", "1")
    lyr.SetMetadataItem("GEOTRANSFORM", "10,1,0,20,0,-1")
    lyr.SetMetadataItem("BAND_COUNT", "1")
    del index_ds

    vrt_ds = gdal.Open(index_filename)
    assert vrt_ds.ReadRaster() == b"\xff"


def test_gti_source_lacks_bands(tmp_vsimem):

    tile_filename = str(tmp_vsimem / "tile.tif")
    ds = gdal.GetDriverByName("GTiff").Create(tile_filename, 1, 1)
    ds.SetGeoTransform([10, 1, 0, 20, 0, -1])
    del ds

    index_filename = str(tmp_vsimem / "index.gti.gpkg")
    index_ds = ogr.GetDriverByName("GPKG").CreateDataSource(index_filename)
    lyr = index_ds.CreateLayer("index", geom_type=ogr.wkbPolygon)
    lyr.CreateField(ogr.FieldDefn("location"))

    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetGeometry(ogr.CreateGeometryFromWkt("POLYGON((10 20,11 20,11 19,10 19,10 20))"))
    f["location"] = tile_filename
    lyr.CreateFeature(f)
    f = None

    lyr.SetMetadataItem("XSIZE", "1")
    lyr.SetMetadataItem("YSIZE", "1")
    lyr.SetMetadataItem("GEOTRANSFORM", "10,1,0,20,0,-1")
    lyr.SetMetadataItem("BAND_COUNT", "2")
    del index_ds

    vrt_ds = gdal.Open(index_filename)
    with pytest.raises(Exception, match="has not enough bands"):
        vrt_ds.ReadRaster()


def test_gti_source_lacks_bands_and_relative_location(tmp_vsimem):

    tile_filename = str(tmp_vsimem / "tile.tif")
    ds = gdal.GetDriverByName("GTiff").Create(tile_filename, 1, 1)
    ds.SetGeoTransform([10, 1, 0, 20, 0, -1])
    del ds

    index_filename = str(tmp_vsimem / "index.gti.gpkg")
    index_ds = ogr.GetDriverByName("GPKG").CreateDataSource(index_filename)
    lyr = index_ds.CreateLayer("index", geom_type=ogr.wkbPolygon)
    lyr.CreateField(ogr.FieldDefn("location"))

    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetGeometry(ogr.CreateGeometryFromWkt("POLYGON((10 20,11 20,11 19,10 19,10 20))"))
    f["location"] = "tile.tif"
    lyr.CreateFeature(f)
    f = None

    lyr.SetMetadataItem("XSIZE", "1")
    lyr.SetMetadataItem("YSIZE", "1")
    lyr.SetMetadataItem("GEOTRANSFORM", "10,1,0,20,0,-1")
    lyr.SetMetadataItem("BAND_COUNT", "2")
    del index_ds

    vrt_ds = gdal.Open(index_filename)
    with pytest.raises(Exception, match="has not enough bands"):
        vrt_ds.ReadRaster()


@pytest.mark.require_driver("netCDF")
def test_gti_source_netcdf_subdataset_absolute(tmp_path):

    index_filename = str(tmp_path / "index.gti.gpkg")
    src_ds = gdal.Open(
        'netCDF:"' + os.path.join(os.getcwd(), "data", "netcdf", "byte.nc") + '":Band1'
    )
    index_ds, _ = create_basic_tileindex(index_filename, [src_ds])
    del index_ds

    assert gdal.Open(index_filename) is not None


@pytest.mark.require_driver("netCDF")
def test_gti_source_netcdf_subdataset_relative(tmp_path):

    tmp_netcdf_filename = str(tmp_path / "byte.nc")
    shutil.copy("data/netcdf/byte.nc", tmp_netcdf_filename)
    index_filename = str(tmp_path / "index.gti.gpkg")
    cwd = os.getcwd()
    try:
        os.chdir(tmp_path)
        src_ds = gdal.Open('netCDF:"byte.nc":Band1')
    finally:
        os.chdir(cwd)
    index_ds, _ = create_basic_tileindex(index_filename, [src_ds])
    del index_ds

    assert gdal.Open(index_filename) is not None


def test_gti_single_source_nodata_same_as_vrt(tmp_vsimem):

    filename1 = str(tmp_vsimem / "one.tif")
    ds = gdal.GetDriverByName("GTiff").Create(filename1, 3, 1, 3)
    ds.SetGeoTransform([2, 1, 0, 49, 0, -1])
    ds.GetRasterBand(1).WriteRaster(0, 0, 3, 1, b"\x01\x02\x01")
    ds.GetRasterBand(2).WriteRaster(0, 0, 3, 1, b"\x01\x03\x01")
    ds.GetRasterBand(3).WriteRaster(0, 0, 3, 1, b"\x01\x04\x01")
    ds.GetRasterBand(1).SetNoDataValue(1)
    ds.GetRasterBand(2).SetNoDataValue(1)
    ds.GetRasterBand(3).SetNoDataValue(1)
    del ds

    index_filename = str(tmp_vsimem / "index.gti.gpkg")
    index_ds, lyr = create_basic_tileindex(index_filename, gdal.Open(filename1))
    lyr.SetMetadataItem("NODATA", "1")
    del index_ds

    vrt_ds = gdal.Open(index_filename)
    assert vrt_ds.ReadRaster() == b"\x01\x02\x01" + b"\x01\x03\x01" + b"\x01\x04\x01"
    assert (
        vrt_ds.GetRasterBand(1).GetMetadataItem("Pixel_0_0", "LocationInfo")
        == "<LocationInfo><File>/vsimem/test_gti_single_source_nodata_same_as_vrt/one.tif</File></LocationInfo>"
    )


def test_gti_overlapping_sources_nodata(tmp_vsimem):

    filename1 = str(tmp_vsimem / "one.tif")
    ds = gdal.GetDriverByName("GTiff").Create(filename1, 3, 1)
    ds.SetGeoTransform([2, 1, 0, 49, 0, -1])
    ds.GetRasterBand(1).WriteRaster(0, 0, 3, 1, b"\x01\x02\x01")
    ds.GetRasterBand(1).SetNoDataValue(1)
    del ds

    filename2 = str(tmp_vsimem / "two.tif")
    ds = gdal.GetDriverByName("GTiff").Create(filename2, 3, 1)
    ds.SetGeoTransform([2, 1, 0, 49, 0, -1])
    ds.GetRasterBand(1).WriteRaster(0, 0, 3, 1, b"\x04\x03\x03")
    ds.GetRasterBand(1).SetNoDataValue(3)
    del ds

    index_filename = str(tmp_vsimem / "index.gti.gpkg")
    sort_values = [1, 2]
    index_ds, lyr = create_basic_tileindex(
        index_filename,
        [gdal.Open(filename1), gdal.Open(filename2)],
        sort_field_name="z_order",
        sort_field_type=ogr.OFTInteger,
        sort_values=sort_values,
    )
    lyr.SetMetadataItem("NODATA", "255")
    del index_ds

    vrt_ds = gdal.Open(index_filename)
    assert vrt_ds.GetRasterBand(1).ReadRaster() == b"\x04\x02\xff"

    assert (
        vrt_ds.GetRasterBand(1).GetMetadataItem("Pixel_0_0", "LocationInfo")
        == "<LocationInfo><File>/vsimem/test_gti_overlapping_sources_nodata/one.tif</File><File>/vsimem/test_gti_overlapping_sources_nodata/two.tif</File></LocationInfo>"
    )


def test_gti_on_the_fly_rgb_color_table_expansion(tmp_vsimem):

    tile_filename = str(tmp_vsimem / "color_table.tif")
    tile_ds = gdal.GetDriverByName("GTiff").Create(tile_filename, 1, 1)
    tile_ds.SetGeoTransform([2, 1, 0, 49, 0, -1])
    ct = gdal.ColorTable()
    ct.SetColorEntry(0, (1, 2, 3, 255))
    assert tile_ds.GetRasterBand(1).SetRasterColorTable(ct) == gdal.CE_None
    tile_ds = None

    index_filename = str(tmp_vsimem / "index.gti.gpkg")
    index_ds, lyr = create_basic_tileindex(index_filename, gdal.Open(tile_filename))
    lyr.SetMetadataItem("BAND_COUNT", "3")
    lyr.SetMetadataItem("COLOR_INTERPRETATION", "Red,Green,Blue")
    del index_ds

    vrt_ds = gdal.Open(index_filename)
    assert vrt_ds.GetRasterBand(1).ReadRaster() == b"\x01"
    assert vrt_ds.GetRasterBand(2).ReadRaster() == b"\x02"
    assert vrt_ds.GetRasterBand(3).ReadRaster() == b"\x03"


def test_gti_on_the_fly_rgba_color_table_expansion(tmp_vsimem):

    tile_filename = str(tmp_vsimem / "color_table.tif")
    tile_ds = gdal.GetDriverByName("GTiff").Create(tile_filename, 1, 1)
    tile_ds.SetGeoTransform([2, 1, 0, 49, 0, -1])
    ct = gdal.ColorTable()
    ct.SetColorEntry(0, (1, 2, 3, 255))
    assert tile_ds.GetRasterBand(1).SetRasterColorTable(ct) == gdal.CE_None
    tile_ds = None

    index_filename = str(tmp_vsimem / "index.gti.gpkg")
    index_ds, lyr = create_basic_tileindex(index_filename, gdal.Open(tile_filename))
    lyr.SetMetadataItem("BAND_COUNT", "4")
    lyr.SetMetadataItem("COLOR_INTERPRETATION", "Red,Green,Blue,Alpha")
    del index_ds

    vrt_ds = gdal.Open(index_filename)
    assert vrt_ds.GetRasterBand(1).ReadRaster() == b"\x01"
    assert vrt_ds.GetRasterBand(2).ReadRaster() == b"\x02"
    assert vrt_ds.GetRasterBand(3).ReadRaster() == b"\x03"
    assert vrt_ds.GetRasterBand(4).ReadRaster() == b"\xff"


def test_gti_on_the_fly_warping(tmp_vsimem):

    index_filename = str(tmp_vsimem / "index.gti.gpkg")

    src_ds = gdal.Open(os.path.join(os.getcwd(), "data", "byte.tif"))
    warped_ds = gdal.Warp("", src_ds, format="VRT", dstSRS="EPSG:4267")

    index_ds = ogr.GetDriverByName("GPKG").CreateDataSource(index_filename)
    lyr = index_ds.CreateLayer("index", srs=warped_ds.GetSpatialRef())
    lyr.CreateField(ogr.FieldDefn("location"))
    f = ogr.Feature(lyr.GetLayerDefn())
    warped_gt = warped_ds.GetGeoTransform()
    minx = warped_gt[0]
    maxx = minx + warped_ds.RasterXSize * warped_gt[1]
    maxy = warped_gt[3]
    miny = maxy + warped_ds.RasterYSize * warped_gt[5]
    f["location"] = src_ds.GetDescription()
    f.SetGeometry(
        ogr.CreateGeometryFromWkt(
            f"POLYGON(({minx} {miny},{minx} {maxy},{maxx} {maxy},{maxx} {miny},{minx} {miny}))"
        )
    )
    lyr.CreateFeature(f)
    lyr.SetMetadataItem("RESAMPLING", "CUBIC")
    del index_ds

    vrt_ds = gdal.Open(index_filename)
    assert vrt_ds.GetRasterBand(1).Checksum() == 4772

    # Check that we add transparency to the warped source
    index_ds = ogr.GetDriverByName("GPKG").CreateDataSource(index_filename)
    lyr = index_ds.CreateLayer("index", srs=warped_ds.GetSpatialRef())
    lyr.CreateField(ogr.FieldDefn("location"))
    f = ogr.Feature(lyr.GetLayerDefn())
    minx -= 5 * warped_gt[1]
    maxx += 5 * warped_gt[1]
    maxy -= 5 * warped_gt[5]
    miny += 5 * warped_gt[5]
    f["location"] = src_ds.GetDescription()
    f.SetGeometry(
        ogr.CreateGeometryFromWkt(
            f"POLYGON(({minx} {miny},{minx} {maxy},{maxx} {maxy},{maxx} {miny},{minx} {miny}))"
        )
    )
    lyr.CreateFeature(f)
    lyr.SetMetadataItem("NODATA", "254")
    del index_ds

    vrt_ds = gdal.Open(index_filename)
    assert vrt_ds.GetRasterBand(1).ReadRaster(0, 0, 1, 1) == b"\xfe"


def test_gti_single_source_alpha_no_dest_nodata(tmp_vsimem):

    filename1 = str(tmp_vsimem / "one.tif")
    ds = gdal.GetDriverByName("GTiff").Create(filename1, 2, 1, 2)
    ds.SetGeoTransform([2, 1, 0, 49, 0, -1])
    ds.GetRasterBand(2).SetColorInterpretation(gdal.GCI_AlphaBand)
    ds.GetRasterBand(1).WriteRaster(0, 0, 2, 1, b"\x01\x02")
    ds.GetRasterBand(2).WriteRaster(0, 0, 2, 1, b"\xff\x00")
    del ds

    index_filename = str(tmp_vsimem / "index.gti.gpkg")
    index_ds, _ = create_basic_tileindex(index_filename, gdal.Open(filename1))
    del index_ds

    vrt_ds = gdal.Open(index_filename)
    assert vrt_ds.RasterCount == 2
    assert (
        vrt_ds.GetRasterBand(1).GetMaskFlags() == gdal.GMF_PER_DATASET | gdal.GMF_ALPHA
    )
    assert vrt_ds.GetRasterBand(1).GetMaskBand().GetBand() == 2
    assert vrt_ds.GetRasterBand(1).ReadRaster() == b"\x01\x02"
    assert vrt_ds.GetRasterBand(2).ReadRaster() == b"\xff\x00"


def test_gti_overlapping_opaque_sources(tmp_vsimem):

    filename1 = str(tmp_vsimem / "one.tif")
    ds = gdal.GetDriverByName("GTiff").Create(filename1, 1, 1, 1)
    ds.SetGeoTransform([2, 1, 0, 49, 0, -1])
    ds.GetRasterBand(1).WriteRaster(0, 0, 1, 1, b"\x01")
    del ds

    filename2 = str(tmp_vsimem / "two.tif")
    ds = gdal.GetDriverByName("GTiff").Create(filename2, 1, 1, 1)
    ds.SetGeoTransform([2, 1, 0, 49, 0, -1])
    ds.GetRasterBand(1).WriteRaster(0, 0, 1, 1, b"\x02")
    del ds

    index_filename = str(tmp_vsimem / "index.gti.gpkg")
    sort_values = [1, 2]
    index_ds, _ = create_basic_tileindex(
        index_filename,
        [gdal.Open(filename1), gdal.Open(filename2)],
        sort_field_name="z_order",
        sort_field_type=ogr.OFTInteger,
        sort_values=sort_values,
    )
    del index_ds

    vrt_ds = gdal.Open(index_filename)
    assert vrt_ds.GetRasterBand(1).ReadRaster() == b"\x02"
    assert vrt_ds.GetMetadataItem("NUMBER_OF_CONTRIBUTING_SOURCES", "__DEBUG__") == "1"

    assert (
        vrt_ds.GetRasterBand(1).GetMetadataItem("Pixel_0_0", "LocationInfo")
        == "<LocationInfo><File>/vsimem/test_gti_overlapping_opaque_sources/two.tif</File></LocationInfo>"
    )


def test_gti_overlapping_sources_alpha_2x1(tmp_vsimem):

    filename1 = str(tmp_vsimem / "one.tif")
    ds = gdal.GetDriverByName("GTiff").Create(filename1, 2, 1, 2)
    ds.SetGeoTransform([2, 1, 0, 49, 0, -1])
    ds.GetRasterBand(2).SetColorInterpretation(gdal.GCI_AlphaBand)
    ds.GetRasterBand(1).WriteRaster(0, 0, 2, 1, b"\x01\x02")
    ds.GetRasterBand(2).WriteRaster(0, 0, 2, 1, b"\xff\x00")
    del ds

    filename2 = str(tmp_vsimem / "two.tif")
    ds = gdal.GetDriverByName("GTiff").Create(filename2, 2, 1, 2)
    ds.SetGeoTransform([2, 1, 0, 49, 0, -1])
    ds.GetRasterBand(2).SetColorInterpretation(gdal.GCI_AlphaBand)
    ds.GetRasterBand(1).WriteRaster(0, 0, 2, 1, b"\x03\x04")
    ds.GetRasterBand(2).WriteRaster(0, 0, 2, 1, b"\x00\xfe")
    del ds

    index_filename = str(tmp_vsimem / "index.gti.gpkg")
    sort_values = [1, 2]
    index_ds, _ = create_basic_tileindex(
        index_filename,
        [gdal.Open(filename1), gdal.Open(filename2)],
        sort_field_name="z_order",
        sort_field_type=ogr.OFTInteger,
        sort_values=sort_values,
    )
    del index_ds

    vrt_ds = gdal.Open(index_filename)
    assert vrt_ds.RasterCount == 2
    assert (
        vrt_ds.GetRasterBand(1).GetMaskFlags() == gdal.GMF_PER_DATASET | gdal.GMF_ALPHA
    )
    assert vrt_ds.GetRasterBand(1).GetMaskBand().GetBand() == 2
    assert vrt_ds.GetRasterBand(1).ReadRaster() == b"\x01\x04"
    assert vrt_ds.GetRasterBand(2).ReadRaster() == b"\xff\xfe"

    assert struct.unpack(
        "H" * 2, vrt_ds.GetRasterBand(1).ReadRaster(buf_type=gdal.GDT_UInt16)
    ) == (1, 4)

    assert vrt_ds.GetRasterBand(1).ReadRaster(0, 0, 1, 1) == b"\x01"
    assert vrt_ds.GetRasterBand(1).ReadRaster(1, 0, 1, 1) == b"\x04"
    assert vrt_ds.GetRasterBand(2).ReadRaster(0, 0, 1, 1) == b"\xff"
    assert vrt_ds.GetRasterBand(2).ReadRaster(1, 0, 1, 1) == b"\xfe"

    assert vrt_ds.ReadRaster() == b"\x01\x04\xff\xfe"
    assert struct.unpack("H" * 4, vrt_ds.ReadRaster(buf_type=gdal.GDT_UInt16)) == (
        1,
        4,
        255,
        254,
    )


def test_gti_overlapping_sources_alpha_1x2(tmp_vsimem):

    filename1 = str(tmp_vsimem / "one.tif")
    ds = gdal.GetDriverByName("GTiff").Create(filename1, 1, 2, 2)
    ds.SetGeoTransform([2, 1, 0, 49, 0, -1])
    ds.GetRasterBand(2).SetColorInterpretation(gdal.GCI_AlphaBand)
    ds.GetRasterBand(1).WriteRaster(0, 0, 1, 2, b"\x01\x02")
    ds.GetRasterBand(2).WriteRaster(0, 0, 1, 2, b"\xff\x00")
    del ds

    filename2 = str(tmp_vsimem / "two.tif")
    ds = gdal.GetDriverByName("GTiff").Create(filename2, 1, 2, 2)
    ds.SetGeoTransform([2, 1, 0, 49, 0, -1])
    ds.GetRasterBand(2).SetColorInterpretation(gdal.GCI_AlphaBand)
    ds.GetRasterBand(1).WriteRaster(0, 0, 1, 2, b"\x03\x04")
    ds.GetRasterBand(2).WriteRaster(0, 0, 1, 2, b"\x00\xfe")
    del ds

    index_filename = str(tmp_vsimem / "index.gti.gpkg")
    sort_values = [1, 2]
    index_ds, _ = create_basic_tileindex(
        index_filename,
        [gdal.Open(filename1), gdal.Open(filename2)],
        sort_field_name="z_order",
        sort_field_type=ogr.OFTInteger,
        sort_values=sort_values,
    )
    del index_ds

    vrt_ds = gdal.Open(index_filename)
    assert vrt_ds.RasterCount == 2
    assert vrt_ds.GetRasterBand(1).ReadRaster() == b"\x01\x04"
    assert vrt_ds.GetRasterBand(2).ReadRaster() == b"\xff\xfe"

    assert struct.unpack(
        "H" * 2, vrt_ds.GetRasterBand(1).ReadRaster(buf_type=gdal.GDT_UInt16)
    ) == (1, 4)

    assert vrt_ds.GetRasterBand(1).ReadRaster(0, 0, 1, 1) == b"\x01"
    assert vrt_ds.GetRasterBand(1).ReadRaster(0, 1, 1, 1) == b"\x04"
    assert vrt_ds.GetRasterBand(2).ReadRaster(0, 0, 1, 1) == b"\xff"
    assert vrt_ds.GetRasterBand(2).ReadRaster(0, 1, 1, 1) == b"\xfe"

    assert vrt_ds.ReadRaster() == b"\x01\x04\xff\xfe"
    assert struct.unpack("H" * 4, vrt_ds.ReadRaster(buf_type=gdal.GDT_UInt16)) == (
        1,
        4,
        255,
        254,
    )


def test_gti_overlapping_sources_alpha_sse2_optim(tmp_vsimem):

    filename1 = str(tmp_vsimem / "one.tif")
    ds = gdal.GetDriverByName("GTiff").Create(filename1, 17, 1, 2)
    ds.SetGeoTransform([2, 1, 0, 49, 0, -1])
    ds.GetRasterBand(2).SetColorInterpretation(gdal.GCI_AlphaBand)
    ds.GetRasterBand(1).WriteRaster(
        0,
        0,
        17,
        1,
        b"\x01\x02\x03\x04\x05\x06\x07\x08\x09\x0a\x0b\x0c\x0d\x0e\x0f\x00\x01",
    )
    ds.GetRasterBand(2).WriteRaster(
        0,
        0,
        17,
        1,
        b"\xff\x00\xff\x00\xff\x00\xff\x00\xff\x00\xff\x00\xff\x00\xff\x00\xff",
    )
    del ds

    filename2 = str(tmp_vsimem / "two.tif")
    ds = gdal.GetDriverByName("GTiff").Create(filename2, 17, 1, 2)
    ds.SetGeoTransform([2, 1, 0, 49, 0, -1])
    ds.GetRasterBand(2).SetColorInterpretation(gdal.GCI_AlphaBand)
    ds.GetRasterBand(1).WriteRaster(
        0,
        0,
        17,
        1,
        b"\x07\x08\x09\x0a\x0b\x0c\x0d\x0e\x0f\x01\x02\x03\x04\x05\x06\x07\x08",
    )
    ds.GetRasterBand(2).WriteRaster(
        0,
        0,
        17,
        1,
        b"\x00\xfe\x00\xfe\x00\xfe\x00\xfe\x00\xfe\x00\xfe\x00\xfe\x00\xfe\x00",
    )
    del ds

    index_filename = str(tmp_vsimem / "index.gti.gpkg")
    sort_values = [1, 2]
    index_ds, _ = create_basic_tileindex(
        index_filename,
        [gdal.Open(filename1), gdal.Open(filename2)],
        sort_field_name="z_order",
        sort_field_type=ogr.OFTInteger,
        sort_values=sort_values,
    )
    del index_ds

    vrt_ds = gdal.Open(index_filename)
    assert vrt_ds.RasterCount == 2
    assert (
        vrt_ds.GetRasterBand(1).ReadRaster()
        == b"\x01\x08\x03\x0a\x05\x0c\x07\x0e\x09\x01\x0b\x03\x0d\x05\x0f\x07\x01"
    )
    assert (
        vrt_ds.GetRasterBand(2).ReadRaster()
        == b"\xff\xfe\xff\xfe\xff\xfe\xff\xfe\xff\xfe\xff\xfe\xff\xfe\xff\xfe\xff"
    )


def test_gti_mix_rgb_rgba(tmp_vsimem):

    filename1 = str(tmp_vsimem / "rgba.tif")
    ds = gdal.GetDriverByName("GTiff").Create(filename1, 2, 1, 4)
    ds.SetGeoTransform([2, 1, 0, 49, 0, -1])
    ds.GetRasterBand(4).SetColorInterpretation(gdal.GCI_AlphaBand)
    ds.GetRasterBand(1).WriteRaster(0, 0, 2, 1, b"\x01\x02")
    ds.GetRasterBand(2).WriteRaster(0, 0, 2, 1, b"\x03\x04")
    ds.GetRasterBand(3).WriteRaster(0, 0, 2, 1, b"\x05\x06")
    ds.GetRasterBand(4).WriteRaster(0, 0, 2, 1, b"\xfe\x00")
    del ds

    filename2 = str(tmp_vsimem / "rgb.tif")
    ds = gdal.GetDriverByName("GTiff").Create(filename2, 2, 1, 3)
    ds.SetGeoTransform([2, 1, 0, 49, 0, -1])
    ds.GetRasterBand(1).WriteRaster(0, 0, 2, 1, b"\x07\x08")
    ds.GetRasterBand(2).WriteRaster(0, 0, 2, 1, b"\x0a\x0b")
    ds.GetRasterBand(3).WriteRaster(0, 0, 2, 1, b"\x0c\x0d")
    del ds

    index_filename = str(tmp_vsimem / "index.gti.gpkg")
    sort_values = [1, 2]
    index_ds, _ = create_basic_tileindex(
        index_filename,
        [gdal.Open(filename1), gdal.Open(filename2)],
        sort_field_name="z_order",
        sort_field_type=ogr.OFTInteger,
        sort_values=sort_values,
    )
    del index_ds

    vrt_ds = gdal.Open(index_filename)
    assert vrt_ds.RasterCount == 4
    assert vrt_ds.GetRasterBand(1).ReadRaster() == b"\x07\x08"
    assert vrt_ds.GetRasterBand(2).ReadRaster() == b"\x0a\x0b"
    assert vrt_ds.GetRasterBand(3).ReadRaster() == b"\x0c\x0d"
    assert vrt_ds.GetRasterBand(4).ReadRaster() == b"\xff\xff"
    assert vrt_ds.ReadRaster() == b"\x07\x08" + b"\x0a\x0b" + b"\x0c\x0d" + b"\xff\xff"

    index_filename = str(tmp_vsimem / "index.gti.gpkg")
    sort_values = [2, 1]
    index_ds, _ = create_basic_tileindex(
        index_filename,
        [gdal.Open(filename1), gdal.Open(filename2)],
        sort_field_name="z_order",
        sort_field_type=ogr.OFTInteger,
        sort_values=sort_values,
    )
    del index_ds

    vrt_ds = gdal.Open(index_filename)
    assert vrt_ds.RasterCount == 4
    assert vrt_ds.GetRasterBand(1).ReadRaster() == b"\x01\x08"
    assert vrt_ds.GetRasterBand(2).ReadRaster() == b"\x03\x0b"
    assert vrt_ds.GetRasterBand(3).ReadRaster() == b"\x05\x0d"
    assert vrt_ds.GetRasterBand(4).ReadRaster() == b"\xfe\xff"
    assert vrt_ds.ReadRaster() == b"\x01\x08" + b"\x03\x0b" + b"\x05\x0d" + b"\xfe\xff"


def test_gti_overlapping_sources_mask_band(tmp_vsimem):

    filename1 = str(tmp_vsimem / "one.tif")
    ds = gdal.GetDriverByName("GTiff").Create(filename1, 2, 1)
    ds.SetGeoTransform([2, 1, 0, 49, 0, -1])
    ds.GetRasterBand(1).WriteRaster(0, 0, 2, 1, b"\x01\x02")
    with gdal.config_option("GDAL_TIFF_INTERNAL_MASK", "NO"):
        ds.CreateMaskBand(gdal.GMF_PER_DATASET)
    ds.GetRasterBand(1).GetMaskBand().WriteRaster(0, 0, 2, 1, b"\xff\x00")
    del ds

    filename2 = str(tmp_vsimem / "two.tif")
    ds = gdal.GetDriverByName("GTiff").Create(filename2, 2, 1)
    ds.SetGeoTransform([2, 1, 0, 49, 0, -1])
    ds.GetRasterBand(1).WriteRaster(0, 0, 2, 1, b"\x03\x04")
    with gdal.config_option("GDAL_TIFF_INTERNAL_MASK", "NO"):
        ds.CreateMaskBand(gdal.GMF_PER_DATASET)
    ds.GetRasterBand(1).GetMaskBand().WriteRaster(0, 0, 2, 1, b"\x00\xfe")
    del ds

    index_filename = str(tmp_vsimem / "index.gti.gpkg")
    sort_values = [1, 2]
    index_ds, _ = create_basic_tileindex(
        index_filename,
        [gdal.Open(filename1), gdal.Open(filename2)],
        sort_field_name="z_order",
        sort_field_type=ogr.OFTInteger,
        sort_values=sort_values,
    )
    del index_ds

    vrt_ds = gdal.Open(index_filename)
    assert vrt_ds.RasterCount == 1
    assert vrt_ds.GetRasterBand(1).GetMaskFlags() == gdal.GMF_PER_DATASET
    assert vrt_ds.GetRasterBand(1).ReadRaster() == b"\x01\x04"
    assert vrt_ds.GetRasterBand(1).GetMaskBand().ReadRaster() == b"\xff\xfe"

    # Test the mask band of the mask band...
    assert vrt_ds.GetRasterBand(1).GetMaskBand().GetMaskFlags() == gdal.GMF_ALL_VALID
    assert (
        vrt_ds.GetRasterBand(1).GetMaskBand().GetMaskBand().ReadRaster() == b"\xff\xff"
    )

    assert struct.unpack(
        "H" * 2, vrt_ds.GetRasterBand(1).ReadRaster(buf_type=gdal.GDT_UInt16)
    ) == (1, 4)

    assert struct.unpack(
        "H" * 2,
        vrt_ds.GetRasterBand(1).GetMaskBand().ReadRaster(buf_type=gdal.GDT_UInt16),
    ) == (255, 254)


def test_gti_consistency_index_geometry_vs_source_extent(tmp_vsimem):

    filename1 = str(tmp_vsimem / "test.tif")
    ds = gdal.GetDriverByName("GTiff").Create(filename1, 10, 10)
    ds.SetGeoTransform([2, 1, 0, 49, 0, -1])
    ds.GetRasterBand(1).Fill(255)
    expected_cs = ds.GetRasterBand(1).Checksum()
    del ds

    index_filename = str(tmp_vsimem / "index.gti.gpkg")
    index_ds, _ = create_basic_tileindex(
        index_filename,
        [gdal.Open(filename1)],
    )
    del index_ds

    vrt_ds = gdal.Open(index_filename)
    with gdal.quiet_errors():
        gdal.ErrorReset()
        assert vrt_ds.GetRasterBand(1).Checksum() == expected_cs
        assert gdal.GetLastErrorMsg() == ""

    # No intersection
    with gdal.Open(filename1, gdal.GA_Update) as ds:
        ds.SetGeoTransform([100, 1, 0, 49, 0, -1])

    vrt_ds = gdal.Open(index_filename)
    with gdal.quiet_errors():
        assert vrt_ds.GetRasterBand(1).Checksum() == 0
        assert "does not intersect at all" in gdal.GetLastErrorMsg()

    # Partial intersection
    with gdal.Open(filename1, gdal.GA_Update) as ds:
        ds.SetGeoTransform([4, 1, 0, 49, 0, -1])

    vrt_ds = gdal.Open(index_filename)
    with gdal.quiet_errors():
        assert vrt_ds.GetRasterBand(1).Checksum() == 958
        assert "does not fully contain" in gdal.GetLastErrorMsg()


def test_gti_mask_band_explicit(tmp_vsimem):

    index_filename = str(tmp_vsimem / "index.gti.gpkg")

    src_ds = gdal.Open(os.path.join(os.getcwd(), "data", "byte.tif"))
    index_ds, lyr = create_basic_tileindex(index_filename, src_ds)
    lyr.SetMetadataItem("MASK_BAND", "YES")
    del index_ds

    vrt_ds = gdal.Open(index_filename)
    check_basic(vrt_ds, src_ds)
    assert vrt_ds.RasterCount == 1
    assert vrt_ds.GetRasterBand(1).GetMaskFlags() == gdal.GMF_PER_DATASET
    assert vrt_ds.GetRasterBand(1).ReadRaster() == src_ds.GetRasterBand(1).ReadRaster()
    assert vrt_ds.GetRasterBand(1).GetMaskBand().ReadRaster() == b"\xff" * (20 * 20)


def test_gti_flushcache(tmp_vsimem):

    filename1 = str(tmp_vsimem / "one.tif")
    ds = gdal.GetDriverByName("GTiff").Create(filename1, 1, 1, 1)
    ds.SetGeoTransform([2, 1, 0, 49, 0, -1])
    ds.GetRasterBand(1).WriteRaster(0, 0, 1, 1, b"\x01")
    del ds

    index_filename = str(tmp_vsimem / "index.gti.gpkg")
    index_ds, _ = create_basic_tileindex(
        index_filename,
        gdal.Open(filename1),
    )
    del index_ds

    vrt_ds = gdal.Open(index_filename)
    assert vrt_ds.GetRasterBand(1).ReadRaster() == b"\x01"

    # Modify source
    src_ds = gdal.Open(filename1, gdal.GA_Update)
    src_ds.GetRasterBand(1).WriteRaster(0, 0, 1, 1, b"\x02")
    src_ds = None

    # We still access a previously cached value
    assert vrt_ds.GetRasterBand(1).ReadRaster() == b"\x01"

    # Now flush the VRT cache and we should get the updated value
    vrt_ds.FlushCache()

    assert vrt_ds.GetRasterBand(1).ReadRaster() == b"\x02"


def test_gti_ovr_factor(tmp_vsimem):

    index_filename = str(tmp_vsimem / "index.gti.gpkg")

    src_ds = gdal.Open(os.path.join(os.getcwd(), "data", "byte.tif"))
    index_ds, lyr = create_basic_tileindex(index_filename, src_ds)
    lyr.SetMetadataItem("MASK_BAND", "YES")
    lyr.SetMetadataItem("OVERVIEW_0_FACTOR", "2")
    del index_ds

    vrt_ds = gdal.Open(index_filename)
    assert vrt_ds.GetRasterBand(1).GetOverviewCount() == 1
    assert vrt_ds.GetRasterBand(1).GetOverview(-1) is None
    assert vrt_ds.GetRasterBand(1).GetOverview(1) is None
    ovr = vrt_ds.GetRasterBand(1).GetOverview(0)
    assert ovr
    assert ovr.XSize == 10
    assert ovr.YSize == 10
    assert ovr.ReadRaster() == src_ds.ReadRaster(buf_xsize=10, buf_ysize=10)

    mask_band = vrt_ds.GetRasterBand(1).GetMaskBand()
    assert mask_band.GetOverviewCount() == 1
    assert mask_band.GetOverview(-1) is None
    assert mask_band.GetOverview(1) is None
    ovr = mask_band.GetOverview(0)
    assert ovr
    assert ovr.XSize == 10
    assert ovr.YSize == 10
    assert ovr.ReadRaster() == src_ds.GetRasterBand(1).GetMaskBand().ReadRaster(
        buf_xsize=10, buf_ysize=10
    )

    vrt_ds = gdal.Open(index_filename)
    assert vrt_ds.GetRasterBand(1).ReadRaster(
        buf_xsize=10, buf_ysize=10
    ) == src_ds.ReadRaster(buf_xsize=10, buf_ysize=10)


def test_gti_ovr_factor_invalid(tmp_vsimem):

    index_filename = str(tmp_vsimem / "index.gti.gpkg")

    src_ds = gdal.Open(os.path.join(os.getcwd(), "data", "byte.tif"))
    index_ds, lyr = create_basic_tileindex(index_filename, src_ds)
    # Also test GDAL 3.9.0 and 3.9.1 where the idx started at 1
    lyr.SetMetadataItem("OVERVIEW_1_FACTOR", "0.5")
    del index_ds

    vrt_ds = gdal.Open(index_filename)
    with pytest.raises(Exception, match="Wrong overview factor"):
        vrt_ds.GetRasterBand(1).GetOverviewCount()


def test_gti_ovr_ds_name(tmp_vsimem):

    index_filename = str(tmp_vsimem / "index.gti.gpkg")

    src_ds = gdal.Open(os.path.join(os.getcwd(), "data", "byte.tif"))
    index_ds, lyr = create_basic_tileindex(index_filename, src_ds)
    lyr.SetMetadataItem("OVERVIEW_0_DATASET", "/i/do/not/exist")
    del index_ds

    vrt_ds = gdal.Open(index_filename)
    with pytest.raises(Exception):
        vrt_ds.GetRasterBand(1).GetOverviewCount()


def test_gti_ovr_lyr_name(tmp_vsimem):

    index_filename = str(tmp_vsimem / "index.gti.gpkg")

    src_ds = gdal.Open(os.path.join(os.getcwd(), "data", "byte.tif"))
    index_ds, lyr = create_basic_tileindex(index_filename, src_ds)
    lyr.SetMetadataItem("OVERVIEW_0_LAYER", "non_existing")
    del index_ds

    vrt_ds = gdal.Open(index_filename)
    with pytest.raises(Exception, match="Layer non_existing does not exist"):
        vrt_ds.GetRasterBand(1).GetOverviewCount()


def test_gti_ovr_of_ovr(tmp_vsimem):

    index_filename = str(tmp_vsimem / "index.gti.gpkg")

    ovr_filename = str(tmp_vsimem / "byte_ovr.tif")
    ovr_ds = gdal.Translate(ovr_filename, "data/byte.tif", width=10)
    ovr_ds.BuildOverviews("NEAR", [2])
    ovr_ds = None

    src_ds = gdal.Open(os.path.join(os.getcwd(), "data", "byte.tif"))
    index_ds, lyr = create_basic_tileindex(index_filename, src_ds)
    lyr.SetMetadataItem("OVERVIEW_0_DATASET", ovr_filename)
    del index_ds

    vrt_ds = gdal.Open(index_filename)
    ovr_ds = gdal.Open(ovr_filename)
    assert vrt_ds.GetRasterBand(1).GetOverviewCount() == 2
    assert (
        vrt_ds.GetRasterBand(1).GetOverview(0).ReadRaster()
        == ovr_ds.GetRasterBand(1).ReadRaster()
    )
    assert (
        vrt_ds.GetRasterBand(1).GetOverview(1).ReadRaster()
        == ovr_ds.GetRasterBand(1).GetOverview(0).ReadRaster()
    )


def test_gti_ovr_of_ovr_OVERVIEW_LEVEL_NONE(tmp_vsimem):

    index_filename = str(tmp_vsimem / "index.gti.gpkg")

    ovr_filename = str(tmp_vsimem / "byte_ovr.tif")
    ovr_ds = gdal.Translate(ovr_filename, "data/byte.tif", width=10)
    ovr_ds.BuildOverviews("NEAR", [2])
    ovr_ds = None

    src_ds = gdal.Open(os.path.join(os.getcwd(), "data", "byte.tif"))
    index_ds, lyr = create_basic_tileindex(index_filename, src_ds)
    lyr.SetMetadataItem("OVERVIEW_0_DATASET", ovr_filename)
    lyr.SetMetadataItem("OVERVIEW_0_OPEN_OPTIONS", "OVERVIEW_LEVEL=NONE")
    del index_ds

    vrt_ds = gdal.Open(index_filename)
    ovr_ds = gdal.Open(ovr_filename)
    assert vrt_ds.GetRasterBand(1).GetOverviewCount() == 1
    assert (
        vrt_ds.GetRasterBand(1).GetOverview(0).ReadRaster()
        == ovr_ds.GetRasterBand(1).ReadRaster()
    )


def test_gti_external_ovr(tmp_vsimem):

    index_filename = str(tmp_vsimem / "index.gti.gpkg")

    src_ds = gdal.Open(os.path.join(os.getcwd(), "data", "byte.tif"))
    index_ds, _ = create_basic_tileindex(index_filename, src_ds)
    del index_ds

    vrt_ds = gdal.Open(index_filename)
    vrt_ds.BuildOverviews("CUBIC", [2])
    vrt_ds = None

    vrt_ds = gdal.Open(index_filename)
    assert vrt_ds.GetRasterBand(1).ReadRaster(
        buf_xsize=10, buf_ysize=10
    ) == src_ds.ReadRaster(buf_xsize=10, buf_ysize=10, resample_alg=gdal.GRIORA_Cubic)


def test_gti_dataset_metadata(tmp_vsimem):

    index_filename = str(tmp_vsimem / "index.gti.gpkg")

    src_ds = gdal.Open(os.path.join(os.getcwd(), "data", "byte.tif"))
    index_ds, lyr = create_basic_tileindex(index_filename, src_ds)
    lyr.SetMetadataItem("FOO", "BAR")
    lyr.SetMetadataItem("RESX", "59.999")
    lyr.SetMetadataItem("RESY", "59.999")
    del index_ds

    vrt_ds = gdal.Open(index_filename)
    assert vrt_ds.GetMetadata_Dict() == {"FOO": "BAR"}
    vrt_ds.SetMetadataItem("BAR", "BAZ")
    del vrt_ds

    assert gdal.VSIStatL(index_filename + ".aux.xml")
    vrt_ds = gdal.Open(index_filename)
    assert vrt_ds.GetGeoTransform()[1] == 59.999
    assert vrt_ds.GetMetadata_Dict() == {
        "FOO": "BAR",
        "BAR": "BAZ",
    }
    del vrt_ds

    gdal.Unlink(index_filename + ".aux.xml")
    vrt_ds = gdal.Open(index_filename, gdal.GA_Update)
    vrt_ds.SetMetadataItem("FOO", "BAR2")
    md = vrt_ds.GetMetadata_Dict()
    md["BAR"] = "BAZ2"
    vrt_ds.SetMetadata(md)
    del vrt_ds

    assert gdal.VSIStatL(index_filename + ".aux.xml") is None
    vrt_ds = gdal.Open(index_filename)
    assert vrt_ds.GetGeoTransform()[1] == 59.999
    assert vrt_ds.GetMetadata_Dict() == {
        "FOO": "BAR2",
        "BAR": "BAZ2",
    }
    del vrt_ds


def test_gti_band_metadata(tmp_vsimem):

    index_filename = str(tmp_vsimem / "index.gti.gpkg")

    src_ds = gdal.Open(os.path.join(os.getcwd(), "data", "byte.tif"))
    index_ds, lyr = create_basic_tileindex(index_filename, src_ds)
    lyr.SetMetadataItem("BAND_1_FOO", "BAR")
    lyr.SetMetadataItem("BAND_1_OFFSET", "2")
    lyr.SetMetadataItem("BAND_1_SCALE", "3")
    lyr.SetMetadataItem("BAND_1_UNITTYPE", "dn")
    lyr.SetMetadataItem("BAND_0_FOO", "BAR0")
    lyr.SetMetadataItem("BAND_2_FOO", "BAR2")
    del index_ds

    vrt_ds = gdal.Open(index_filename)
    assert vrt_ds.GetRasterBand(1).GetOffset() == 2
    assert vrt_ds.GetRasterBand(1).GetScale() == 3
    assert vrt_ds.GetRasterBand(1).GetUnitType() == "dn"
    assert vrt_ds.GetRasterBand(1).GetMetadata_Dict() == {"FOO": "BAR"}
    vrt_ds.GetRasterBand(1).ComputeStatistics(False)
    vrt_ds.GetRasterBand(1).SetMetadataItem("BAR", "BAZ")
    del vrt_ds

    assert gdal.VSIStatL(index_filename + ".aux.xml")
    vrt_ds = gdal.Open(index_filename)
    assert vrt_ds.GetRasterBand(1).GetOffset() == 2
    assert vrt_ds.GetRasterBand(1).GetScale() == 3
    assert vrt_ds.GetRasterBand(1).GetUnitType() == "dn"
    assert vrt_ds.GetRasterBand(1).GetMetadataDomainList() == ["", "LocationInfo"]
    assert vrt_ds.GetRasterBand(1).GetMetadata_Dict() == {
        "FOO": "BAR",
        "BAR": "BAZ",
        "STATISTICS_MAXIMUM": "255",
        "STATISTICS_MEAN": "126.765",
        "STATISTICS_MINIMUM": "74",
        "STATISTICS_STDDEV": "22.928470838676",
        "STATISTICS_VALID_PERCENT": "100",
    }
    del vrt_ds

    gdal.Unlink(index_filename + ".aux.xml")
    vrt_ds = gdal.Open(index_filename, gdal.GA_Update)
    vrt_ds.GetRasterBand(1).ComputeStatistics(False)
    vrt_ds.GetRasterBand(1).SetMetadataItem("BAR", "BAZ")
    del vrt_ds

    assert gdal.VSIStatL(index_filename + ".aux.xml") is None
    vrt_ds = gdal.Open(index_filename)
    assert vrt_ds.GetRasterBand(1).GetOffset() == 2
    assert vrt_ds.GetRasterBand(1).GetScale() == 3
    assert vrt_ds.GetRasterBand(1).GetUnitType() == "dn"
    assert vrt_ds.GetRasterBand(1).GetMetadata_Dict() == {
        "FOO": "BAR",
        "BAR": "BAZ",
        "STATISTICS_MAXIMUM": "255",
        "STATISTICS_MEAN": "126.765",
        "STATISTICS_MINIMUM": "74",
        "STATISTICS_STDDEV": "22.928470838676",
        "STATISTICS_VALID_PERCENT": "100",
    }
    del vrt_ds

    vrt_ds = gdal.Open(index_filename, gdal.GA_Update)
    md = vrt_ds.GetMetadata_Dict()
    md["BAR"] = "BAZ2"
    vrt_ds.SetMetadata(md)
    md = vrt_ds.GetRasterBand(1).GetMetadata_Dict()
    md["BAR"] = "BAZ3"
    vrt_ds.GetRasterBand(1).SetMetadata(md)
    del vrt_ds

    assert gdal.VSIStatL(index_filename + ".aux.xml") is None

    vrt_ds = gdal.Open(index_filename)
    assert vrt_ds.GetRasterBand(1).GetOffset() == 2
    assert vrt_ds.GetRasterBand(1).GetScale() == 3
    assert vrt_ds.GetRasterBand(1).GetUnitType() == "dn"
    assert vrt_ds.GetRasterBand(1).GetMetadata_Dict() == {
        "FOO": "BAR",
        "BAR": "BAZ3",
        "STATISTICS_MAXIMUM": "255",
        "STATISTICS_MEAN": "126.765",
        "STATISTICS_MINIMUM": "74",
        "STATISTICS_STDDEV": "22.928470838676",
        "STATISTICS_VALID_PERCENT": "100",
    }
    assert vrt_ds.GetMetadata_Dict() == {
        "BAR": "BAZ2",
    }


def test_gti_connection_prefix(tmp_vsimem):

    index_filename = str(tmp_vsimem / "index.gti.gpkg")

    src_ds = gdal.Open(os.path.join(os.getcwd(), "data", "byte.tif"))
    index_ds, _ = create_basic_tileindex(index_filename, src_ds)
    del index_ds

    vrt_ds = gdal.Open(f"GTI:{index_filename}")
    check_basic(vrt_ds, src_ds)
    del vrt_ds


def test_gti_xml(tmp_vsimem):

    index_filename = str(tmp_vsimem / "index.gti.gpkg")

    tile_filename = str(tmp_vsimem / "byte.tif")
    gdal.Translate(tile_filename, "data/byte.tif")

    src_ds = gdal.Open(tile_filename)
    index_ds, _ = create_basic_tileindex(index_filename, src_ds)
    del index_ds

    xml_filename = str(tmp_vsimem / "index.xml")
    xml_content = f"""<GDALTileIndexDataset>
  <IndexDataset>{index_filename}</IndexDataset>
</GDALTileIndexDataset>"""

    vrt_ds = gdal.Open(xml_content)
    check_basic(vrt_ds, src_ds)
    del vrt_ds

    with gdaltest.tempfile(xml_filename, xml_content):
        vrt_ds = gdal.Open(xml_filename)
        check_basic(vrt_ds, src_ds)
        del vrt_ds

        vrt_ds = gdal.Open(xml_filename, gdal.GA_Update)
        vrt_ds.SetMetadata({"FOO": "BAR"})
        vrt_ds.SetMetadataItem("BAR", "BAZ")
        vrt_ds.GetRasterBand(1).SetMetadata({"xFOO": "BAR"})
        vrt_ds.GetRasterBand(1).SetMetadataItem("xBAR", "BAZ")
        del vrt_ds

        assert gdal.VSIStatL(xml_filename + ".aux.xml") is None

        vrt_ds = gdal.Open(xml_filename)
        assert vrt_ds.GetMetadata_Dict() == {"FOO": "BAR", "BAR": "BAZ"}
        assert vrt_ds.GetRasterBand(1).GetMetadata_Dict() == {
            "xFOO": "BAR",
            "xBAR": "BAZ",
        }
        del vrt_ds

    xml_content = f"""<GDALTileIndexDataset>
  <IndexDataset>{index_filename}</IndexDataset>
  <Filter>invalid</Filter>
</GDALTileIndexDataset>"""
    with pytest.raises(Exception, match="no such column: invalid"):
        gdal.Open(xml_content)

    xml_content = f"""<GDALTileIndexDataset>
  <IndexDataset>{index_filename}</IndexDataset>
  <ResX>60</ResX>
  <ResY>60</ResY>
  <SortField>location</SortField>
  <SortFieldAsc>true</SortFieldAsc>
  <Band band="1" dataType="UInt16">
      <Description>my band</Description>
      <Offset>2</Offset>
      <Scale>3</Scale>
      <NoDataValue>4</NoDataValue>
      <UnitType>dn</UnitType>
      <ColorInterp>Gray</ColorInterp>
      <ColorTable/>
      <CategoryNames><Category>cat</Category></CategoryNames>
      <GDALRasterAttributeTable/>
  </Band>
</GDALTileIndexDataset>"""

    vrt_ds = gdal.Open(xml_content)
    band = vrt_ds.GetRasterBand(1)
    assert band.GetDescription() == "my band"
    assert band.DataType == gdal.GDT_UInt16
    assert band.GetOffset() == 2
    assert band.GetScale() == 3
    assert band.GetNoDataValue() == 4
    assert band.GetUnitType() == "dn"
    assert band.GetColorInterpretation() == gdal.GCI_GrayIndex
    assert band.GetColorTable() is not None
    assert band.GetCategoryNames() == ["cat"]
    assert band.GetDefaultRAT() is not None
    del vrt_ds

    xml_content = f"""<GDALTileIndexDataset>
  <IndexDataset>{index_filename}</IndexDataset>
  <ResX>60</ResX>
  <ResY>60</ResY>
  <Band/>
</GDALTileIndexDataset>"""
    with pytest.raises(Exception, match="band attribute missing on Band element"):
        gdal.Open(xml_content)

    xml_content = f"""<GDALTileIndexDataset>
  <IndexDataset>{index_filename}</IndexDataset>
  <ResX>60</ResX>
  <ResY>60</ResY>
  <Band band="-1" dataType="UInt16"/>
</GDALTileIndexDataset>"""
    with pytest.raises(Exception, match="Invalid band number"):
        gdal.Open(xml_content)

    xml_content = f"""<GDALTileIndexDataset>
  <IndexDataset>{index_filename}</IndexDataset>
  <ResX>60</ResX>
  <ResY>60</ResY>
  <Band band="2" dataType="UInt16"/>
</GDALTileIndexDataset>"""
    with pytest.raises(Exception, match="Invalid band number: found 2, expected 1"):
        gdal.Open(xml_content)

    xml_content = f"""<GDALTileIndexDataset>
  <IndexDataset>{index_filename}</IndexDataset>
  <ResX>60</ResX>
  <ResY>60</ResY>
  <BandCount>2</BandCount>
  <Band band="1" dataType="UInt16"/>
</GDALTileIndexDataset>"""
    with pytest.raises(
        Exception, match="Inconsistent BandCount with actual number of Band elements"
    ):
        gdal.Open(xml_content)

    xml_content = f"""<GDALTileIndexDataset>
  <IndexDataset>{index_filename}</IndexDataset>
      <Overview>
          <Factor>2</Factor>
      </Overview>
</GDALTileIndexDataset>"""
    vrt_ds = gdal.Open(xml_content)
    assert vrt_ds.GetRasterBand(1).GetOverviewCount() == 1
    assert vrt_ds.GetRasterBand(1).GetOverview(0).XSize == 10
    del vrt_ds

    tile_ovr_filename = str(tmp_vsimem / "byte_ovr.tif")
    gdal.Translate(tile_ovr_filename, "data/byte.tif", width=10)

    index2_filename = str(tmp_vsimem / "index2.gti.gpkg")
    create_basic_tileindex(index2_filename, gdal.Open(tile_ovr_filename))

    xml_content = f"""<GDALTileIndexDataset>
  <IndexDataset>{index_filename}</IndexDataset>
  <IndexLayer>index</IndexLayer>
      <Overview>
          <Dataset>{index2_filename}</Dataset>
      </Overview>
</GDALTileIndexDataset>"""
    vrt_ds = gdal.Open(xml_content)
    assert vrt_ds.GetRasterBand(1).GetOverviewCount() == 1
    assert vrt_ds.GetRasterBand(1).GetOverview(0).XSize == 10
    del vrt_ds

    create_basic_tileindex(
        index_filename,
        gdal.Open(tile_ovr_filename),
        add_to_existing=True,
        lyr_name="index_ovr",
    )

    xml_content = f"""<GDALTileIndexDataset>
  <IndexDataset>{index_filename}</IndexDataset>
  <IndexLayer>index</IndexLayer>
      <Overview>
          <Layer>index_ovr</Layer>
      </Overview>
</GDALTileIndexDataset>"""
    vrt_ds = gdal.Open(xml_content)
    assert vrt_ds.GetRasterBand(1).GetOverviewCount() == 1
    assert vrt_ds.GetRasterBand(1).GetOverview(0).XSize == 10
    del vrt_ds

    xml_content = f"""<GDALTileIndexDataset>
  <IndexDataset>{index_filename}</IndexDataset>
  <IndexLayer>index</IndexLayer>
      <Overview>
          <Layer>index</Layer>
          <OpenOptions>
              <OOI name="@FACTOR">2</OOI>
          </OpenOptions>
      </Overview>
</GDALTileIndexDataset>"""
    vrt_ds = gdal.Open(xml_content)
    assert vrt_ds.GetRasterBand(1).GetOverviewCount() == 1
    assert vrt_ds.GetRasterBand(1).GetOverview(0).XSize == 10
    del vrt_ds

    xml_content = """<GDALTileIndexDataset"""
    with pytest.raises(
        Exception,
        match="Parse error at EOF",
    ):
        gdal.Open(xml_content)

    with gdaltest.tempfile(xml_filename, xml_content):
        with pytest.raises(
            Exception,
            match="Parse error at EOF",
        ):
            gdal.Open(xml_filename)

    xml_content = """<foo><![CDATA[<GDALTileIndexDataset]]></foo>"""
    with gdaltest.tempfile(xml_filename, xml_content):
        with pytest.raises(
            Exception,
            match="Missing GDALTileIndexDataset root element",
        ):
            gdal.Open(xml_filename)

    xml_content = """<GDALTileIndexDataset/>"""
    with pytest.raises(
        Exception,
        match="Missing IndexDataset element",
    ):
        gdal.Open(xml_content)

    xml_content = """<GDALTileIndexDataset>
    <IndexDataset>i_do_not_exist</IndexDataset>
</GDALTileIndexDataset>"""
    with pytest.raises(
        Exception,
        match="i_do_not_exist",
    ):
        gdal.Open(xml_content)

    xml_content = f"""<GDALTileIndexDataset>
  <IndexDataset>{index_filename}</IndexDataset>
  <IndexLayer>i_do_not_exist</IndexLayer>
</GDALTileIndexDataset>"""
    with pytest.raises(
        Exception,
        match="i_do_not_exist",
    ):
        gdal.Open(xml_content)

    xml_content = f"""<GDALTileIndexDataset>
  <IndexDataset>{index_filename}</IndexDataset>
  <IndexLayer>index</IndexLayer>
      <Overview>
      </Overview>
</GDALTileIndexDataset>"""
    with pytest.raises(
        Exception,
        match="At least one of Dataset, Layer or Factor element must be present as an Overview child",
    ):
        gdal.Open(xml_content)

    xml_content = f"""<GDALTileIndexDataset>
  <IndexDataset>{index_filename}</IndexDataset>
  <IndexLayer>index</IndexLayer>
      <Overview>
          <Dataset>i_do_not_exist</Dataset>
      </Overview>
</GDALTileIndexDataset>"""
    vrt_ds = gdal.Open(xml_content)
    with pytest.raises(Exception, match="i_do_not_exist"):
        vrt_ds.GetRasterBand(1).GetOverviewCount()

    xml_content = f"""<GDALTileIndexDataset>
  <IndexDataset>{index_filename}</IndexDataset>
  <IndexLayer>index</IndexLayer>
      <Overview>
          <Layer>i_do_not_exist</Layer>
      </Overview>
</GDALTileIndexDataset>"""
    vrt_ds = gdal.Open(xml_content)
    with pytest.raises(Exception, match="i_do_not_exist"):
        vrt_ds.GetRasterBand(1).GetOverviewCount()


def test_gti_open_options(tmp_vsimem):

    index_filename = str(tmp_vsimem / "index.gti.gpkg")

    src_ds = gdal.Open(os.path.join(os.getcwd(), "data", "byte.tif"))
    index_ds, _ = create_basic_tileindex(index_filename, src_ds)
    del index_ds

    vrt_ds = gdal.OpenEx(index_filename, open_options=["RESX=30", "RESY=30"])
    assert vrt_ds.GetGeoTransform() == pytest.approx(
        (440720.0, 30.0, 0.0, 3751320.0, 0.0, -30.0)
    )


def test_gti_xml_vrtti_embedded(tmp_vsimem):

    index_filename = str(tmp_vsimem / "index.gti.gpkg")

    src_ds = gdal.Open(os.path.join(os.getcwd(), "data", "byte.tif"))
    index_ds, lyr = create_basic_tileindex(index_filename, src_ds)

    xml_content = """<GDALTileIndexDataset>
  <ResX>60</ResX>
  <ResY>60</ResY>
  <SortField>location</SortField>
  <SortFieldAsc>true</SortFieldAsc>
  <Band band="1" dataType="UInt16">
      <Description>my band</Description>
      <Offset>2</Offset>
      <Scale>3</Scale>
      <NoDataValue>4</NoDataValue>
      <UnitType>dn</UnitType>
      <ColorInterp>Gray</ColorInterp>
      <ColorTable/>
      <CategoryNames><Category>cat</Category></CategoryNames>
      <GDALRasterAttributeTable/>
  </Band>
</GDALTileIndexDataset>"""

    lyr.SetMetadata([xml_content], "xml:GTI")
    del index_ds

    vrt_ds = gdal.Open(index_filename)
    band = vrt_ds.GetRasterBand(1)
    assert band.GetDescription() == "my band"
    assert band.DataType == gdal.GDT_UInt16
    assert band.GetOffset() == 2
    assert band.GetScale() == 3
    assert band.GetNoDataValue() == 4
    assert band.GetUnitType() == "dn"
    assert band.GetColorInterpretation() == gdal.GCI_GrayIndex
    assert band.GetColorTable() is not None
    assert band.GetCategoryNames() == ["cat"]
    assert band.GetDefaultRAT() is not None
    del vrt_ds


###############################################################################
# Test multi-threaded reading


@pytest.mark.parametrize("use_threads", [True, False])
@pytest.mark.parametrize("num_tiles", [2, 128])
def test_gti_read_multi_threaded(tmp_vsimem, use_threads, num_tiles):

    width = 2048
    src_ds = gdal.Translate(
        "", "../gdrivers/data/small_world.tif", width=width, format="MEM"
    )
    assert width % num_tiles == 0
    tile_width = width // num_tiles
    tiles_ds = []
    for i in range(num_tiles):
        tile_filename = str(tmp_vsimem / ("%d.tif" % i))
        gdal.Translate(
            tile_filename, src_ds, srcWin=[i * tile_width, 0, tile_width, 1024]
        )
        tiles_ds.append(gdal.Open(tile_filename))

    index_filename = str(tmp_vsimem / "index.gti.gpkg")
    index_ds, _ = create_basic_tileindex(index_filename, tiles_ds)
    del index_ds

    vrt_ds = gdal.Open(index_filename)

    pcts = []

    def cbk(pct, msg, user_data):
        if pcts:
            assert pct >= pcts[-1]
        pcts.append(pct)
        return 1

    with gdal.config_options({} if use_threads else {"GTI_NUM_THREADS": "0"}):
        assert vrt_ds.ReadRaster(1, 2, 1030, 1020, callback=cbk) == src_ds.ReadRaster(
            1, 2, 1030, 1020
        )
    assert pcts[-1] == 1.0

    assert vrt_ds.GetMetadataItem("MULTI_THREADED_RASTERIO_LAST_USED", "__DEBUG__") == (
        "1" if gdal.GetNumCPUs() >= 2 and use_threads else "0"
    )

    # Again
    pcts = []
    with gdal.config_options({} if use_threads else {"GTI_NUM_THREADS": "0"}):
        assert vrt_ds.ReadRaster(1, 2, 1030, 1020, callback=cbk) == src_ds.ReadRaster(
            1, 2, 1030, 1020
        )
    assert pcts[-1] == 1.0

    assert vrt_ds.GetMetadataItem("MULTI_THREADED_RASTERIO_LAST_USED", "__DEBUG__") == (
        "1" if gdal.GetNumCPUs() >= 2 and use_threads else "0"
    )


###############################################################################
# Test multi-threaded reading


def test_gti_read_multi_threaded_disabled_since_overlapping_sources(tmp_vsimem):

    src_ds = gdal.Translate(
        "", "../gdrivers/data/small_world.tif", width=2048, format="MEM"
    )
    OVERLAP = 1
    left_filename = str(tmp_vsimem / "left.tif")
    gdal.Translate(left_filename, src_ds, srcWin=[0, 0, 1024 + OVERLAP, 1024])
    right_filename = str(tmp_vsimem / "right.tif")
    gdal.Translate(right_filename, src_ds, srcWin=[1024, 0, 1024, 1024])

    index_filename = str(tmp_vsimem / "index.gti.gpkg")
    index_ds, _ = create_basic_tileindex(
        index_filename, [gdal.Open(left_filename), gdal.Open(right_filename)]
    )
    del index_ds

    vrt_ds = gdal.Open(index_filename)

    assert vrt_ds.ReadRaster(1, 2, 1030, 1020) == src_ds.ReadRaster(1, 2, 1030, 1020)

    assert (
        vrt_ds.GetMetadataItem("MULTI_THREADED_RASTERIO_LAST_USED", "__DEBUG__") == "0"
    )


###############################################################################
# Test multi-threaded reading


def test_gti_read_multi_threaded_disabled_because_invalid_filename(tmp_vsimem):

    src_ds = gdal.Translate(
        "", "../gdrivers/data/small_world.tif", width=2048, format="MEM"
    )
    left_filename = str(tmp_vsimem / "left.tif")
    gdal.Translate(left_filename, src_ds, srcWin=[0, 0, 1024, 1024])
    right_filename = str(tmp_vsimem / "right.tif")
    gdal.Translate(right_filename, src_ds, srcWin=[1024, 0, 1024, 1024])

    index_filename = str(tmp_vsimem / "index.gti.gpkg")
    index_ds, _ = create_basic_tileindex(
        index_filename, [gdal.Open(left_filename), gdal.Open(right_filename)]
    )
    lyr = index_ds.GetLayer(0)
    f = lyr.GetFeature(2)
    f["location"] = "/i/do/not/exist"
    lyr.SetFeature(f)
    del index_ds

    vrt_ds = gdal.Open(index_filename)

    with pytest.raises(Exception, match="/i/do/not/exist"):
        vrt_ds.ReadRaster()

    assert vrt_ds.GetMetadataItem("MULTI_THREADED_RASTERIO_LAST_USED", "__DEBUG__") == (
        "1" if gdal.GetNumCPUs() >= 2 else "0"
    )


###############################################################################
# Test multi-threaded reading


def test_gti_read_multi_threaded_disabled_because_truncated_source(tmp_vsimem):

    src_ds = gdal.Translate(
        "", "../gdrivers/data/small_world.tif", width=2048, format="MEM"
    )
    left_filename = str(tmp_vsimem / "left.tif")
    gdal.Translate(left_filename, src_ds, srcWin=[0, 0, 1024, 1024])
    right_filename = str(tmp_vsimem / "right.tif")
    gdal.Translate(right_filename, src_ds, srcWin=[1024, 0, 1024, 1024])

    index_filename = str(tmp_vsimem / "index.gti.gpkg")
    index_ds, _ = create_basic_tileindex(
        index_filename, [gdal.Open(left_filename), gdal.Open(right_filename)]
    )
    del index_ds

    f = gdal.VSIFOpenL(right_filename, "rb+")
    assert f
    gdal.VSIFTruncateL(f, gdal.VSIStatL(right_filename).size - 10)
    gdal.VSIFCloseL(f)

    vrt_ds = gdal.Open(index_filename)

    with pytest.raises(Exception, match="right.tif"):
        vrt_ds.ReadRaster()

    assert vrt_ds.GetMetadataItem("MULTI_THREADED_RASTERIO_LAST_USED", "__DEBUG__") == (
        "1" if gdal.GetNumCPUs() >= 2 else "0"
    )


###############################################################################
# Test non existing source


def test_gti_read_non_existing_source(tmp_vsimem):

    src_ds = gdal.Translate(
        "", "../gdrivers/data/small_world.tif", width=2048, format="MEM"
    )
    left_filename = str(tmp_vsimem / "left.tif")
    gdal.Translate(left_filename, src_ds, srcWin=[0, 0, 1024, 1024])
    right_filename = str(tmp_vsimem / "right.tif")
    gdal.Translate(right_filename, src_ds, srcWin=[1024, 0, 1024, 1024])

    index_filename = str(tmp_vsimem / "index.gti.gpkg")
    index_ds, _ = create_basic_tileindex(
        index_filename, [gdal.Open(left_filename), gdal.Open(right_filename)]
    )
    del index_ds

    gdal.Unlink(right_filename)

    vrt_ds = gdal.Open(index_filename)

    with gdal.config_option("GDAL_NUM_THREADS", "1"), gdaltest.error_raised(
        gdal.CE_Failure
    ), gdaltest.disable_exceptions():
        assert vrt_ds.ReadRaster() is None


###############################################################################


@pytest.mark.require_curl()
@pytest.mark.require_driver("Parquet")
@pytest.mark.xfail(
    reason="https://naipeuwest.blob.core.windows.net/naip/v002/ok/2010/ok_100cm_2010/34099/m_3409901_nw_14_1_20100425.tif now leads to HTTP/1.1 409 Public access is not permitted on this storage account."
)
def test_gti_stac_geoparquet():

    url = (
        "https://github.com/stac-utils/stac-geoparquet/raw/main/tests/data/naip.parquet"
    )

    conn = gdaltest.gdalurlopen(url, timeout=4)
    if conn is None:
        pytest.skip("cannot open URL")

    ds = gdal.Open("GTI:/vsicurl/" + url)
    assert ds.GetSpatialRef().GetAuthorityCode(None) == "26914"
    assert ds.GetGeoTransform() == pytest.approx(
        (408231.0, 1.0, 0.0, 3873862.0, 0.0, -1.0), rel=1e-5
    )
    assert ds.RasterCount == 4
    assert [band.GetColorInterpretation() for band in ds] == [
        gdal.GCI_RedBand,
        gdal.GCI_GreenBand,
        gdal.GCI_BlueBand,
        gdal.GCI_Undefined,
    ]
    assert [band.GetDescription() for band in ds] == [
        "Red",
        "Green",
        "Blue",
        "NIR (near-infrared)",
    ]


###############################################################################


@pytest.mark.require_curl()
@pytest.mark.require_driver("GeoJSON")
@pytest.mark.parametrize(
    "filename",
    [
        "sentinel2_stac_geoparquet_proj_code.geojson",
        "sentinel2_stac_geoparquet_proj_epsg.geojson",
        "sentinel2_stac_geoparquet_proj_wkt2.geojson",
        "sentinel2_stac_geoparquet_proj_projjson.geojson",
    ],
)
def test_gti_stac_geoparquet_sentinel2(filename):

    url = "https://e84-earth-search-sentinel-data.s3.us-west-2.amazonaws.com/sentinel-2-c1-l2a/12/S/VD/2023/12/S2A_T12SVD_20231213T181818_L2A/B07.tif"

    conn = gdaltest.gdalurlopen(url, timeout=4)
    if conn is None:
        pytest.skip("cannot open URL")

    ds = gdal.Open(f"GTI:data/gti/{filename}")
    assert ds.RasterXSize == 5556
    assert ds.RasterYSize == 5540
    assert ds.GetSpatialRef().GetAuthorityCode(None) == "32612"
    assert ds.GetGeoTransform() == pytest.approx(
        (398760.0, 20.0, 0.0, 3900560.0, 0.0, -20.0), rel=1e-5
    )
    assert ds.RasterCount == 1
    band = ds.GetRasterBand(1)
    assert band.DataType == gdal.GDT_UInt16
    assert band.GetNoDataValue() == 0
    assert band.GetColorInterpretation() == gdal.GCI_RedEdgeBand
    assert band.GetDescription() == "B07"
    assert band.GetOffset() == -0.1
    assert band.GetScale() == 0.0001
    assert band.GetMetadata_Dict("IMAGERY") == {
        "CENTRAL_WAVELENGTH_UM": "0.783",
        "FWHM_UM": "0.028",
    }


###############################################################################
# Test case when 2 sources where collected, but erroneously discarded


def test_gti_tile_001():

    out_ds = gdal.Warp(
        "",
        "vrt://data/gti/tile-001.gti.gpkg?bands=3,2,1",
        options="-f MEM -t_srs epsg:3857 -dstalpha -ts 256 256 -te -12151653.008664179593 3101508.859699312598 -12141869.069043677300 3111292.799319814891 -r cubic",
    )
    assert out_ds.GetRasterBand(1).ComputeRasterMinMax() == (1000, 1000)
    assert out_ds.GetRasterBand(2).ComputeRasterMinMax() == (1000, 1000)
    assert out_ds.GetRasterBand(3).ComputeRasterMinMax() == (1000, 1000)
    assert out_ds.GetRasterBand(4).ComputeRasterMinMax() == (65535, 65535)


###############################################################################


@pytest.mark.require_driver("SQLite")
def test_gti_sql(tmp_vsimem):

    if "SPATIALITE" not in gdal.GetDriverByName("SQLite").GetMetadataItem(
        gdal.DMD_CREATIONOPTIONLIST
    ):
        pytest.skip("Spatialite support missing")

    with gdal.GetDriverByName("GTiff").Create(tmp_vsimem / "red.tif", 1, 1, 3) as ds:
        ds.SetGeoTransform([0, 1, 0, 1, 0, -1])
        ds.GetRasterBand(1).Fill(255)

    with gdal.GetDriverByName("GTiff").Create(tmp_vsimem / "green.tif", 1, 1, 3) as ds:
        ds.SetGeoTransform([0, 1, 0, 1, 0, -1])
        ds.GetRasterBand(2).Fill(255)

    with gdal.GetDriverByName("SQLite").CreateVector(
        tmp_vsimem / "index.db", options=["SPATIALITE=YES"]
    ) as sqlite_ds:
        lyr = sqlite_ds.CreateLayer("tileindex", geom_type=ogr.wkbPolygon)
        lyr.CreateField(ogr.FieldDefn("location"))
        lyr.CreateField(ogr.FieldDefn("tile_id", ogr.OFTInteger))
        lyr.CreateField(ogr.FieldDefn("version", ogr.OFTInteger))
        f = ogr.Feature(lyr.GetLayerDefn())
        f["location"] = tmp_vsimem / "red.tif"
        f["tile_id"] = 10
        f["version"] = 2
        f.SetGeometry(ogr.CreateGeometryFromWkt("POLYGON((0 0,0 1,1 1,1 0,0 0))"))
        lyr.CreateFeature(f)
        f = ogr.Feature(lyr.GetLayerDefn())
        f["location"] = tmp_vsimem / "green.tif"
        f["tile_id"] = 10
        f["version"] = 1
        f.SetGeometry(ogr.CreateGeometryFromWkt("POLYGON((0 0,0 1,1 1,1 0,0 0))"))
        lyr.CreateFeature(f)

    xml_content = f"""<GDALTileIndexDataset>
  <IndexDataset>{tmp_vsimem}/index.db</IndexDataset>
  <SQL>WITH target_version AS (SELECT tile_id,max(version) AS version FROM tileindex GROUP BY tile_id) SELECT * FROM tileindex INNER JOIN target_version ON target_version.tile_id=tileindex.tile_id AND target_version.version=tileindex.version</SQL>
  <MinX>0</MinX>
  <MinY>0</MinY>
  <MaxX>1</MaxX>
  <MaxY>1</MaxY>
</GDALTileIndexDataset>"""

    gti_ds = gdal.Open(xml_content)
    if ogrtest.have_geos():
        flags, pct = gti_ds.GetRasterBand(1).GetDataCoverageStatus(
            0, 0, gti_ds.RasterXSize, gti_ds.RasterYSize
        )
        assert flags == gdal.GDAL_DATA_COVERAGE_STATUS_DATA and pct == 100.0
    assert gti_ds.GetRasterBand(1).ComputeRasterMinMax() == (255, 255)
    assert gti_ds.GetRasterBand(2).ComputeRasterMinMax() == (0, 0)
    assert gti_ds.GetRasterBand(3).ComputeRasterMinMax() == (0, 0)

    xml_content = f"""<GDALTileIndexDataset>
  <IndexDataset>{tmp_vsimem}/index.db</IndexDataset>
  <SQL>WITH target_version AS (SELECT tile_id,max(version) AS version FROM tileindex GROUP BY tile_id) SELECT * FROM tileindex INNER JOIN target_version ON target_version.tile_id=tileindex.tile_id AND target_version.version=tileindex.version</SQL>
  <SpatialSQL>WITH target_version AS (SELECT tile_id,max(version) AS version FROM tileindex GROUP BY tile_id) SELECT * FROM tileindex INNER JOIN target_version ON target_version.tile_id=tileindex.tile_id AND target_version.version=tileindex.version INNER JOIN idx_tileindex_geometry ON tileindex.ogc_fid = idx_tileindex_geometry.pkid WHERE idx_tileindex_geometry.xmin &lt;= {{XMAX}} and idx_tileindex_geometry.ymin &lt;= {{YMAX}} and idx_tileindex_geometry.xmax &gt;= {{XMIN}} and idx_tileindex_geometry.ymax &gt;= {{YMIN}}</SpatialSQL>
  <MinX>0</MinX>
  <MinY>0</MinY>
  <MaxX>1</MaxX>
  <MaxY>1</MaxY>
</GDALTileIndexDataset>"""

    gti_ds = gdal.Open(xml_content)
    if ogrtest.have_geos():
        flags, pct = gti_ds.GetRasterBand(1).GetDataCoverageStatus(
            0, 0, gti_ds.RasterXSize, gti_ds.RasterYSize
        )
        assert flags == gdal.GDAL_DATA_COVERAGE_STATUS_DATA and pct == 100.0
    assert gti_ds.GetRasterBand(1).ComputeRasterMinMax() == (255, 255)
    assert gti_ds.GetRasterBand(2).ComputeRasterMinMax() == (0, 0)
    assert gti_ds.GetRasterBand(3).ComputeRasterMinMax() == (0, 0)

    gti_ds = gdal.OpenEx(
        f"GTI:{tmp_vsimem}/index.db",
        open_options=[
            "SQL=WITH target_version AS (SELECT tile_id,max(version) AS version FROM tileindex GROUP BY tile_id) SELECT * FROM tileindex INNER JOIN target_version ON target_version.tile_id=tileindex.tile_id AND target_version.version=tileindex.version",
            "SPATIAL_SQL=WITH target_version AS (SELECT tile_id,max(version) AS version FROM tileindex GROUP BY tile_id) SELECT * FROM tileindex INNER JOIN target_version ON target_version.tile_id=tileindex.tile_id AND target_version.version=tileindex.version INNER JOIN idx_tileindex_geometry ON tileindex.ogc_fid = idx_tileindex_geometry.pkid WHERE idx_tileindex_geometry.xmin <= {XMAX} and idx_tileindex_geometry.ymin <= {YMAX} and idx_tileindex_geometry.xmax >= {XMIN} and idx_tileindex_geometry.ymax >= {YMIN}",
            "MINX=0",
            "MINY=0",
            "MAXX=1",
            "MAXY=1",
        ],
    )
    assert gti_ds.GetRasterBand(1).ComputeRasterMinMax() == (255, 255)
    assert gti_ds.GetRasterBand(2).ComputeRasterMinMax() == (0, 0)
    assert gti_ds.GetRasterBand(3).ComputeRasterMinMax() == (0, 0)

    # Invalid SQL
    with pytest.raises(Exception, match="syntax error"):
        gdal.OpenEx(f"GTI:{tmp_vsimem}/index.db", open_options=["SQL=invalid"])

    # Invalid Spatial SQL
    gti_ds = gdal.OpenEx(
        f"GTI:{tmp_vsimem}/index.db",
        open_options=[
            "SQL=SELECT * FROM tileindex",
            "SPATIAL_SQL=invalid",
            "MINX=0",
            "MINY=0",
            "MAXX=1",
            "MAXY=1",
        ],
    )
    with pytest.raises(Exception, match="syntax error"):
        gti_ds.GetRasterBand(1).GetDataCoverageStatus(
            0, 0, gti_ds.RasterXSize, gti_ds.RasterYSize
        )
    with pytest.raises(Exception, match="syntax error"):
        gti_ds.GetRasterBand(1).ComputeRasterMinMax()


###############################################################################


@pytest.fixture(scope="module")
def server():

    process, port = webserver.launch(handler=webserver.DispatcherHttpHandler)
    if port == 0:
        pytest.skip()

    import collections

    WebServer = collections.namedtuple("WebServer", "process port")

    yield WebServer(process, port)

    # Clearcache needed to close all connections, since the Python server
    # can only handle one connection at a time
    gdal.VSICurlClearCache()

    webserver.server_stop(process, port)


###############################################################################


@pytest.mark.require_curl()
@pytest.mark.require_driver("HTTP")
@pytest.mark.require_driver("CSV")
def test_gti_non_rewriting_of_url(server, tmp_vsimem):
    """Check fix for https://github.com/OSGeo/gdal/issues/12900#issuecomment-3637663699"""

    gdal.FileFromMemBuffer(
        tmp_vsimem / "test.csv",
        f'location,WKT\n"http://localhost:{server.port}/test.tif","POLYGON((0 0,0 1,1 1,1 0))"',
    )

    with gdal.GetDriverByName("GTiff").Create(tmp_vsimem / "test.tif", 1, 1) as ds:
        ds.SetGeoTransform([0, 1, 0, 1, 0, -1])
        ds.GetRasterBand(1).Fill(1)

    with gdal.VSIFile(tmp_vsimem / "test.tif", "rb") as f:
        content = f.read()

    handler = webserver.SequentialHandler()
    # Check that we get a GET request for the full file (ie using the HTTP driver)
    # and not a /vsicurl/ range request
    handler.add("GET", "/test.tif", 200, {}, content, unexpected_headers=["Range"])
    handler.add("GET", "/test.tif", 200, {}, content, unexpected_headers=["Range"])

    with webserver.install_http_handler(handler), gdal.quiet_errors():
        ds = gdal.Open(f"GTI:{tmp_vsimem}/test.csv")
        assert ds.GetRasterBand(1).Checksum() == 1
