#!/usr/bin/env pytest
# -*- coding: utf-8 -*-
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test functionality for OGR Arrow driver.
# Author:   Even Rouault <even dot rouault at spatialys.com>
#
###############################################################################
# Copyright (c) 2022, Planet Labs
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

import json
import math

import gdaltest
import pytest

from osgeo import gdal, ogr, osr

pytestmark = pytest.mark.require_driver("Arrow")

from . import ogr_parquet


def check(test_filename, filename_prefix, dim):
    ref_filename = (
        "data/arrow/from_paleolimbot_geoarrow/" + filename_prefix + dim + "-wkb.feather"
    )
    ds_ref = ogr.Open(ref_filename)
    lyr_ref = ds_ref.GetLayer(0)
    ds = ogr.Open(test_filename)
    lyr = ds.GetLayer(0)
    assert lyr_ref.GetFeatureCount() == lyr.GetFeatureCount()
    while True:
        f_ref = lyr_ref.GetNextFeature()
        f = lyr.GetNextFeature()
        assert (f_ref is None) == (f is None)
        if f is None:
            break
        g = f.GetGeometryRef()
        g_ref = f_ref.GetGeometryRef()
        assert (g_ref is None) == (g is None)
        if g:
            if g.IsEmpty():
                assert g.IsEmpty() == g_ref.IsEmpty()
            else:
                assert g.Equals(g_ref), (g.ExportToIsoWkt(), g_ref.ExportToIsoWkt())


###############################################################################
# Test reading test files from https://github.com/paleolimbot/geoarrow/tree/master/inst/example_feather


@pytest.mark.parametrize(
    "filename_prefix",
    [
        "point",
        "linestring",
        "polygon",
        "multipoint",
        "multilinestring",
        "multipolygon",
        "geometrycollection",
    ],
)
@pytest.mark.parametrize("dim", ["", "_z", "_m", "_zm"])
def test_ogr_arrow_read_all_geom_types(filename_prefix, dim):

    test_filename = (
        "data/arrow/from_paleolimbot_geoarrow/"
        + filename_prefix
        + dim
        + "-default.feather"
    )
    check(test_filename, filename_prefix, dim)


###############################################################################
# Test dplicating test files from https://github.com/paleolimbot/geoarrow/tree/master/inst/example_feather


@pytest.mark.parametrize(
    "filename_prefix",
    [
        "point",
        "linestring",
        "polygon",
        "multipoint",
        "multilinestring",
        "multipolygon",
        "geometrycollection",
    ],
)
@pytest.mark.parametrize("dim", ["", "_z", "_m", "_zm"])
@pytest.mark.parametrize("encoding", ["WKB", "WKT", "GEOARROW", "GEOARROW_INTERLEAVED"])
def test_ogr_arrow_write_all_geom_types(filename_prefix, dim, encoding):

    test_filename = (
        "data/arrow/from_paleolimbot_geoarrow/"
        + filename_prefix
        + dim
        + "-default.feather"
    )
    ds_ref = ogr.Open(test_filename)
    lyr_ref = ds_ref.GetLayer(0)

    if not encoding.startswith("GEOARROW") or lyr_ref.GetGeomType() not in (
        ogr.wkbGeometryCollection,
        ogr.wkbGeometryCollection25D,
        ogr.wkbGeometryCollectionM,
        ogr.wkbGeometryCollectionZM,
    ):
        vsifilename = "/vsimem/test.feather"
        with gdaltest.config_option("OGR_ARROW_ALLOW_ALL_DIMS", "YES"):
            gdal.VectorTranslate(
                vsifilename,
                test_filename,
                dstSRS="EPSG:4326",
                reproject=False,
                layerCreationOptions=["GEOMETRY_ENCODING=" + encoding],
            )
        check(vsifilename, filename_prefix, dim)
        gdal.Unlink(vsifilename)


###############################################################################
# Read a file with all data types


@pytest.mark.parametrize("use_vsi", [False, True])
def test_ogr_arrow_1(use_vsi):

    filename = "data/arrow/test.feather"
    if use_vsi:
        vsifilename = "/vsimem/test.feather"
        gdal.FileFromMemBuffer(vsifilename, open(filename, "rb").read())
        filename = vsifilename

    try:
        ogr_parquet._check_test_parquet(
            filename, expect_fast_get_extent=False, expect_ignore_fields=False
        )
    finally:
        if use_vsi:
            gdal.Unlink(vsifilename)


###############################################################################
# Run test_ogrsf on a Feather file


def test_ogr_arrow_test_ogrsf_test_feather():
    import test_cli_utilities

    if test_cli_utilities.get_test_ogrsf_path() is None:
        pytest.skip()

    ret = gdaltest.runexternal(
        test_cli_utilities.get_test_ogrsf_path()
        + " -ro data/arrow/from_paleolimbot_geoarrow/polygon-default.feather"
    )

    assert "INFO" in ret
    assert "ERROR" not in ret


###############################################################################
# Run test_ogrsf on a Feather file


def test_ogr_arrow_test_ogrsf_test_feather_all_types():
    import test_cli_utilities

    if test_cli_utilities.get_test_ogrsf_path() is None:
        pytest.skip()

    ret = gdaltest.runexternal(
        test_cli_utilities.get_test_ogrsf_path() + " -ro data/arrow/test.feather"
    )

    assert "INFO" in ret
    assert "ERROR" not in ret


###############################################################################
# Run test_ogrsf on a IPC stream file


def test_ogr_arrow_test_ogrsf_test_ipc():
    import test_cli_utilities

    if test_cli_utilities.get_test_ogrsf_path() is None:
        pytest.skip()

    ret = gdaltest.runexternal(
        test_cli_utilities.get_test_ogrsf_path()
        + " -ro data/arrow/from_paleolimbot_geoarrow/polygon-default.ipc"
    )

    assert "INFO" in ret
    assert "ERROR" not in ret


###############################################################################
# Run test_ogrsf on a IPC stream file, in streamable mode


def test_ogr_arrow_test_ogrsf_test_ipc_streamable():
    import test_cli_utilities

    if test_cli_utilities.get_test_ogrsf_path() is None:
        pytest.skip()

    ret = gdaltest.runexternal(
        test_cli_utilities.get_test_ogrsf_path()
        + " -ro ARROW_IPC_STREAM:data/arrow/from_paleolimbot_geoarrow/polygon-default.ipc"
    )

    assert "INFO" in ret
    assert "ERROR" not in ret


###############################################################################
# Test write support


@pytest.mark.parametrize(
    "use_vsi,batch_size,fid,write_gdal_footer,format,open_as_stream",
    [
        (False, None, None, False, "FILE", None),
        (True, 2, "fid", True, "FILE", None),
        (False, None, None, False, "STREAM", False),
        (False, 2, None, False, "STREAM", False),
        (False, 2, None, False, "STREAM", True),
    ],
)
def test_ogr_arrow_write_from_another_dataset(
    use_vsi, batch_size, fid, write_gdal_footer, format, open_as_stream
):

    outfilename = "/vsimem/out" if use_vsi else "tmp/out"
    try:
        layerCreationOptions = ["FORMAT=" + format]
        if batch_size:
            layerCreationOptions.append("BATCH_SIZE=" + str(batch_size))
        if fid:
            layerCreationOptions.append("FID=" + fid)
        with gdaltest.config_option(
            "OGR_ARROW_WRITE_GDAL_FOOTER", str(write_gdal_footer)
        ):
            gdal.VectorTranslate(
                outfilename,
                "data/arrow/test.feather",
                format="ARROW",
                layerCreationOptions=layerCreationOptions,
            )

        ds = gdal.OpenEx(
            "ARROW_IPC_STREAM:" + outfilename if open_as_stream else outfilename
        )
        lyr = ds.GetLayer(0)
        assert lyr.GetDataset().GetDescription() == ds.GetDescription()

        assert lyr.GetFIDColumn() == (fid if fid else "")
        f = lyr.GetNextFeature()
        assert f.GetGeometryRef() is not None

        if fid:
            f = lyr.GetFeature(4)
            assert f is not None
            assert f.GetFID() == 4

            assert lyr.GetFeature(5) is None

        if batch_size and format == "FILE":
            num_features = lyr.GetFeatureCount()
            expected_num_row_groups = int(math.ceil(num_features / batch_size))
            assert lyr.GetMetadataItem("NUM_RECORD_BATCHES", "_ARROW_") == str(
                expected_num_row_groups
            )
            for i in range(expected_num_row_groups):
                got_num_rows = lyr.GetMetadataItem(
                    "RECORD_BATCHES[%d].NUM_ROWS" % i, "_ARROW_"
                )
                if i < expected_num_row_groups - 1:
                    assert got_num_rows == str(batch_size)
                else:
                    assert got_num_rows == str(
                        num_features - (expected_num_row_groups - 1) * batch_size
                    )

        assert lyr.GetMetadataItem("FORMAT", "_ARROW_") == format

        geo = lyr.GetMetadataItem("geo", "_ARROW_METADATA_")
        assert geo is not None
        j = json.loads(geo)
        assert j is not None
        assert "primary_column" in j
        assert j["primary_column"] == "geometry"
        assert "columns" in j
        assert "geometry" in j["columns"]
        assert "encoding" in j["columns"]["geometry"]
        assert j["columns"]["geometry"]["encoding"] == "geoarrow.point"
        assert "bbox" not in j["columns"]["geometry"]

        md = lyr.GetMetadata("_ARROW_METADATA_")
        assert "geo" in md

        if write_gdal_footer:
            geo = lyr.GetMetadataItem("gdal:geo", "_ARROW_FOOTER_METADATA_")
            assert geo is not None
            j = json.loads(geo)
            assert j is not None
            assert "bbox" in j["columns"]["geometry"]

        md = lyr.GetMetadata("_ARROW_FOOTER_METADATA_")
        if write_gdal_footer:
            assert "gdal:geo" in md
        else:
            assert "gdal:geo" not in md

        if open_as_stream:

            with pytest.raises(Exception):
                lyr.GetFeatureCount(force=0)

            assert lyr.GetFeatureCount() == 5

            with pytest.raises(Exception, match="rewind non-seekable stream"):
                lyr.GetNextFeature()

        elif format == "STREAM" and batch_size:

            assert lyr.GetFeatureCount(force=0) == 5

        ogr_parquet._check_test_parquet(
            outfilename,
            expect_fast_feature_count=False if open_as_stream else True,
            expect_fast_get_extent=False,
            expect_ignore_fields=False,
        )

    finally:
        ds = None
        gdal.Unlink(outfilename)


###############################################################################
# Test compression support


@pytest.mark.parametrize("compression", ["uncompressed", "lz4", "zstd"])
def test_ogr_arrow_write_compression(compression):

    lco = gdal.GetDriverByName("Arrow").GetMetadataItem("DS_LAYER_CREATIONOPTIONLIST")
    if compression.upper() not in lco:
        pytest.skip()

    outfilename = "/vsimem/out.feather"
    ds = gdal.GetDriverByName("Arrow").Create(outfilename, 0, 0, 0, gdal.GDT_Unknown)
    options = ["FID=fid", "COMPRESSION=" + compression]
    lyr = ds.CreateLayer("out", geom_type=ogr.wkbNone, options=options)
    assert lyr is not None
    assert lyr.GetDataset().GetDescription() == ds.GetDescription()
    f = ogr.Feature(lyr.GetLayerDefn())
    lyr.CreateFeature(f)
    lyr = None
    ds = None

    ds = ogr.Open(outfilename)
    assert ds is not None
    lyr = ds.GetLayer(0)
    assert lyr is not None
    # TODO: it would be good to check the compression type, but I can't find anything in the arrow API for that
    lyr = None
    ds = None

    gdal.Unlink(outfilename)


###############################################################################
# Read invalid file .arrow


def test_ogr_arrow_invalid_arrow():

    with pytest.raises(Exception):
        ogr.Open("data/arrow/invalid.arrow")


###############################################################################
# Read invalid file .arrows


def test_ogr_arrow_invalid_arrows():

    with pytest.raises(Exception):
        ogr.Open("data/arrow/invalid.arrows")

    with pytest.raises(Exception):
        ogr.Open("ARROW_IPC_STREAM:/vsimem/i_dont_exist.bin")


###############################################################################
# Test coordinate epoch support


@pytest.mark.parametrize("write_gdal_footer", [True, False])
def test_ogr_arrow_coordinate_epoch(write_gdal_footer):

    outfilename = "/vsimem/out.feather"
    with gdaltest.config_option("OGR_ARROW_WRITE_GDAL_FOOTER", str(write_gdal_footer)):
        ds = gdal.GetDriverByName("Arrow").Create(
            outfilename, 0, 0, 0, gdal.GDT_Unknown
        )
        srs = osr.SpatialReference()
        srs.ImportFromEPSG(4326)
        srs.SetCoordinateEpoch(2022.3)
        ds.CreateLayer("out", geom_type=ogr.wkbPoint, srs=srs)
        ds = None

    ds = ogr.Open(outfilename)
    assert ds is not None
    lyr = ds.GetLayer(0)
    assert lyr is not None
    srs = lyr.GetSpatialRef()
    assert srs is not None
    assert srs.GetCoordinateEpoch() == 2022.3
    lyr = None
    ds = None

    gdal.Unlink(outfilename)


###############################################################################
# Test that Arrow extension type is recognized as geometry column
# if "geo" metadata is absent


def test_ogr_arrow_extension_type():

    outfilename = "/vsimem/out.feather"
    with gdaltest.config_options(
        {"OGR_ARROW_WRITE_GDAL_FOOTER": "NO", "OGR_ARROW_WRITE_GEO": "NO"}
    ):
        gdal.VectorTranslate(outfilename, "data/arrow/test.feather")

    ds = ogr.Open(outfilename)
    assert ds is not None
    lyr = ds.GetLayer(0)
    assert lyr is not None
    assert lyr.GetGeometryColumn()
    assert lyr.GetLayerDefn().GetGeomFieldCount() == 1
    lyr = None
    ds = None

    gdal.Unlink(outfilename)


###############################################################################
# Test reading a file with a geoarrow.point extension registered with
# PyArrow (https://github.com/OSGeo/gdal/issues/5834)


def test_ogr_arrow_read_with_geoarrow_extension_registered():
    pa = pytest.importorskip("pyarrow")
    _point_storage_type = pa.list_(pa.field("xy", pa.float64()), 2)

    class PointGeometryType(pa.ExtensionType):
        def __init__(self):
            pa.ExtensionType.__init__(self, _point_storage_type, "geoarrow.point")

        def __arrow_ext_serialize__(self):
            return b""

        @classmethod
        def __arrow_ext_deserialize__(cls, storage_type, serialized):
            return cls()

    point_type = PointGeometryType()

    pa.register_extension_type(point_type)
    try:
        ds = ogr.Open("data/arrow/from_paleolimbot_geoarrow/point-default.feather")
        lyr = ds.GetLayer(0)
        assert lyr.GetGeometryColumn() == "geometry"
        f = lyr.GetNextFeature()
        assert f.GetGeometryRef().ExportToIsoWkt() == "POINT (30 10)"
    finally:
        pa.unregister_extension_type(point_type.extension_name)


###############################################################################
# Test reading a file with an extension on a regular field registered with
# PyArrow


def test_ogr_arrow_read_with_extension_registered_on_regular_field():
    pa = pytest.importorskip("pyarrow")

    class MyJsonType(pa.ExtensionType):
        def __init__(self):
            super().__init__(pa.string(), "my_json")

        def __arrow_ext_serialize__(self):
            return b""

        @classmethod
        def __arrow_ext_deserialize__(cls, storage_type, serialized):
            return cls()

    my_json_type = MyJsonType()

    pa.register_extension_type(my_json_type)
    try:
        ds = ogr.Open("data/arrow/extension_custom.feather")
        lyr = ds.GetLayer(0)
        f = lyr.GetNextFeature()
        assert f["extension_custom"] == '{"foo":"bar"}'
    finally:
        pa.unregister_extension_type(my_json_type.extension_name)


###############################################################################
# Test reading a file with an extension on a regular field not registered with
# PyArrow


def test_ogr_arrow_read_with_extension_not_registered_on_regular_field():

    ds = ogr.Open("data/arrow/extension_custom.feather")
    lyr = ds.GetLayer(0)
    f = lyr.GetNextFeature()
    assert f["extension_custom"] == '{"foo":"bar"}'


###############################################################################
# Test reading a file with the arrow.json extension


def test_ogr_arrow_read_arrow_json_extension():

    ds = ogr.Open("data/arrow/extension_json.feather")
    lyr = ds.GetLayer(0)
    assert lyr.GetLayerDefn().GetFieldDefn(0).GetSubType() == ogr.OFSTJSON
    f = lyr.GetNextFeature()
    assert f["extension_json"] == '{"foo":"bar"}'

    stream = lyr.GetArrowStream()
    schema = stream.GetSchema()

    dst_ds = gdal.GetDriverByName("Memory").Create("", 0, 0, 0, gdal.GDT_Unknown)
    dst_lyr = dst_ds.CreateLayer("test")
    success, error_msg = dst_lyr.IsArrowSchemaSupported(schema)
    assert success

    for i in range(schema.GetChildrenCount()):
        if schema.GetChild(i).GetName() not in ("wkb_geometry", "OGC_FID"):
            dst_lyr.CreateFieldFromArrowSchema(schema.GetChild(i))

    assert dst_lyr.GetLayerDefn().GetFieldDefn(0).GetSubType() == ogr.OFSTJSON


###############################################################################
# Test storing OGR field alternative name and comment in gdal:schema extension


def test_ogr_arrow_field_alternative_name_comment():

    outfilename = "/vsimem/out.feather"
    try:
        ds = ogr.GetDriverByName("Arrow").CreateDataSource(outfilename)
        lyr = ds.CreateLayer("test", geom_type=ogr.wkbNone)
        fld_defn = ogr.FieldDefn("fld", ogr.OFTInteger)
        fld_defn.SetAlternativeName("long_field_name")
        fld_defn.SetComment("this is a field")
        lyr.CreateField(fld_defn)
        ds = None

        ds = ogr.Open(outfilename)
        assert ds is not None
        lyr = ds.GetLayer(0)
        assert lyr is not None
        assert lyr.GetLayerDefn().GetFieldDefn(0).GetName() == "fld"
        assert (
            lyr.GetLayerDefn().GetFieldDefn(0).GetAlternativeName() == "long_field_name"
        )
        assert lyr.GetLayerDefn().GetFieldDefn(0).GetComment() == "this is a field"
        lyr = None
        ds = None
    finally:
        gdal.Unlink(outfilename)


###############################################################################


@gdaltest.enable_exceptions()
@pytest.mark.parametrize("encoding", ["WKB", "WKT", "GEOARROW"])
def test_ogr_arrow_write_arrow(encoding, tmp_vsimem):

    src_ds = ogr.Open("data/arrow/test.feather")
    src_lyr = src_ds.GetLayer(0)

    outfilename = str(tmp_vsimem / "test_ogr_arrow_write_arrow.feather")
    with ogr.GetDriverByName("Arrow").CreateDataSource(outfilename) as dst_ds:
        dst_lyr = dst_ds.CreateLayer(
            "test",
            srs=src_lyr.GetSpatialRef(),
            geom_type=ogr.wkbPoint,
            options=["GEOMETRY_ENCODING=" + encoding],
        )

        stream = src_lyr.GetArrowStream(["MAX_FEATURES_IN_BATCH=3"])
        schema = stream.GetSchema()

        success, error_msg = dst_lyr.IsArrowSchemaSupported(schema)
        assert success

        for i in range(schema.GetChildrenCount()):
            if schema.GetChild(i).GetName() != src_lyr.GetGeometryColumn():
                dst_lyr.CreateFieldFromArrowSchema(schema.GetChild(i))

        while True:
            array = stream.GetNextRecordBatch()
            if array is None:
                break
            assert dst_lyr.WriteArrowBatch(schema, array) == ogr.OGRERR_NONE

    ogr_parquet._check_test_parquet(
        outfilename, expect_fast_get_extent=False, expect_ignore_fields=False
    )


###############################################################################


@gdaltest.enable_exceptions()
def test_ogr_arrow_write_arrow_fid_in_input_and_output(tmp_vsimem):

    src_ds = ogr.Open("data/poly.shp")
    src_lyr = src_ds.GetLayer(0)

    outfilename = str(tmp_vsimem / "poly.feather")
    with ogr.GetDriverByName("Arrow").CreateDataSource(outfilename) as dst_ds:
        dst_lyr = dst_ds.CreateLayer(
            "test",
            srs=src_lyr.GetSpatialRef(),
            geom_type=ogr.wkbPoint,
            options=["GEOMETRY_ENCODING=WKB", "FID=my_fid"],
        )

        stream = src_lyr.GetArrowStream(["INCLUDE_FID=YES"])
        schema = stream.GetSchema()

        success, error_msg = dst_lyr.IsArrowSchemaSupported(schema)
        assert success

        for i in range(schema.GetChildrenCount()):
            if schema.GetChild(i).GetName() not in ("wkb_geometry", "OGC_FID"):
                dst_lyr.CreateFieldFromArrowSchema(schema.GetChild(i))

        while True:
            array = stream.GetNextRecordBatch()
            if array is None:
                break
            assert dst_lyr.WriteArrowBatch(schema, array) == ogr.OGRERR_NONE

    ds = ogr.Open(outfilename)
    lyr = ds.GetLayer(0)
    src_lyr.ResetReading()
    for i in range(src_lyr.GetFeatureCount()):
        assert str(src_lyr.GetNextFeature()) == str(lyr.GetNextFeature())


###############################################################################


@gdaltest.enable_exceptions()
def test_ogr_arrow_write_arrow_fid_in_input_but_not_in_output(tmp_vsimem):

    src_ds = ogr.Open("data/poly.shp")
    src_lyr = src_ds.GetLayer(0)

    outfilename = str(tmp_vsimem / "poly.feather")
    with ogr.GetDriverByName("Arrow").CreateDataSource(outfilename) as dst_ds:
        dst_lyr = dst_ds.CreateLayer(
            "test",
            srs=src_lyr.GetSpatialRef(),
            geom_type=ogr.wkbPoint,
            options=["GEOMETRY_ENCODING=WKB"],
        )

        stream = src_lyr.GetArrowStream(["INCLUDE_FID=YES"])
        schema = stream.GetSchema()

        success, error_msg = dst_lyr.IsArrowSchemaSupported(schema)
        assert success

        for i in range(schema.GetChildrenCount()):
            if schema.GetChild(i).GetName() not in ("wkb_geometry", "OGC_FID"):
                dst_lyr.CreateFieldFromArrowSchema(schema.GetChild(i))

        while True:
            array = stream.GetNextRecordBatch()
            if array is None:
                break
            assert dst_lyr.WriteArrowBatch(schema, array) == ogr.OGRERR_NONE

    ds = ogr.Open(outfilename)
    lyr = ds.GetLayer(0)
    src_lyr.ResetReading()
    for i in range(src_lyr.GetFeatureCount()):
        assert str(src_lyr.GetNextFeature()) == str(lyr.GetNextFeature())


###############################################################################


@gdaltest.enable_exceptions()
def test_ogr_arrow_write_arrow_fid_in_output_but_not_in_input(tmp_vsimem):

    src_ds = ogr.Open("data/poly.shp")
    src_lyr = src_ds.GetLayer(0)

    outfilename = str(tmp_vsimem / "poly.feather")
    with ogr.GetDriverByName("Arrow").CreateDataSource(outfilename) as dst_ds:
        dst_lyr = dst_ds.CreateLayer(
            "test",
            srs=src_lyr.GetSpatialRef(),
            geom_type=ogr.wkbPoint,
            options=["GEOMETRY_ENCODING=WKB", "FID=my_fid"],
        )

        stream = src_lyr.GetArrowStream(["INCLUDE_FID=NO"])
        schema = stream.GetSchema()

        success, error_msg = dst_lyr.IsArrowSchemaSupported(schema)
        assert success

        for i in range(schema.GetChildrenCount()):
            if schema.GetChild(i).GetName() not in ("wkb_geometry", "OGC_FID"):
                dst_lyr.CreateFieldFromArrowSchema(schema.GetChild(i))

        while True:
            array = stream.GetNextRecordBatch()
            if array is None:
                break
            assert dst_lyr.WriteArrowBatch(schema, array) == ogr.OGRERR_NONE

    ds = ogr.Open(outfilename)
    lyr = ds.GetLayer(0)
    src_lyr.ResetReading()
    for i in range(src_lyr.GetFeatureCount()):
        assert str(src_lyr.GetNextFeature()) == str(lyr.GetNextFeature())
