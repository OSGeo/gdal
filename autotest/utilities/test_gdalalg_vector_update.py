#!/usr/bin/env pytest
# -*- coding: utf-8 -*-
###############################################################################
# Project:  GDAL/OGR Test Suite
# Purpose:  'gdal vector update' testing
# Author:   Even Rouault <even dot rouault @ spatialys.com>
#
###############################################################################
# Copyright (c) 2025, Even Rouault <even dot rouault at spatialys.com>
#
# SPDX-License-Identifier: MIT
###############################################################################

import pytest

from osgeo import gdal, ogr


def test_gdalalg_vector_update_by_key():

    src_ds = gdal.GetDriverByName("MEM").CreateVector("src")
    src_lyr = src_ds.CreateLayer("test")
    src_lyr.CreateField(ogr.FieldDefn("other_field", ogr.OFTString))
    src_lyr.CreateField(ogr.FieldDefn("src_only_field", ogr.OFTString))
    src_lyr.CreateField(ogr.FieldDefn("int_field", ogr.OFTInteger))
    src_lyr.CreateField(ogr.FieldDefn("int64_field", ogr.OFTInteger64))
    src_lyr.CreateField(ogr.FieldDefn("real_field", ogr.OFTReal))
    src_lyr.CreateField(ogr.FieldDefn("str_field", ogr.OFTString))

    f = ogr.Feature(src_lyr.GetLayerDefn())
    src_lyr.CreateFeature(f)

    f = ogr.Feature(src_lyr.GetLayerDefn())
    f["int_field"] = 123
    f["int64_field"] = 1234567890123
    f["real_field"] = 1.25
    f["str_field"] = "other"
    src_lyr.CreateFeature(f)

    f = ogr.Feature(src_lyr.GetLayerDefn())
    f["int_field"] = 123
    f["int64_field"] = 1234567890123
    f["real_field"] = 1.25
    f["str_field"] = "foo"
    f["other_field"] = "update"
    f["src_only_field"] = "src_only_field"
    src_lyr.CreateFeature(f)

    f = ogr.Feature(src_lyr.GetLayerDefn())
    f["int_field"] = 124
    f["int64_field"] = 1234567890123
    f["real_field"] = 1.25
    f["str_field"] = "foo2"
    f["other_field"] = "update2"
    f["src_only_field"] = "src_only_field2"
    src_lyr.CreateFeature(f)

    f = ogr.Feature(src_lyr.GetLayerDefn())
    f["int_field"] = 125
    f["int64_field"] = 1234567890123
    f["real_field"] = 1.25
    f["str_field"] = "foo3"
    f["other_field"] = "update3"
    f["src_only_field"] = "src_only_field3"
    src_lyr.CreateFeature(f)

    dst_ds = gdal.GetDriverByName("MEM").CreateVector("dst")
    dst_lyr = dst_ds.CreateLayer("test")
    dst_lyr.CreateField(ogr.FieldDefn("int_field", ogr.OFTInteger))
    dst_lyr.CreateField(ogr.FieldDefn("int64_field", ogr.OFTInteger64))
    dst_lyr.CreateField(ogr.FieldDefn("real_field", ogr.OFTReal))
    dst_lyr.CreateField(ogr.FieldDefn("str_field", ogr.OFTString))
    dst_lyr.CreateField(ogr.FieldDefn("other_field", ogr.OFTString))
    dst_lyr.CreateField(ogr.FieldDefn("dst_only_field", ogr.OFTString))

    f = ogr.Feature(dst_lyr.GetLayerDefn())
    f["int_field"] = 123
    f["int64_field"] = 1234567890123
    f["real_field"] = 1.25
    f["str_field"] = "foo"
    f["other_field"] = "initial"
    f["dst_only_field"] = "dst_only_field"
    f.SetFID(20)
    dst_lyr.CreateFeature(f)

    f = ogr.Feature(dst_lyr.GetLayerDefn())
    f["int_field"] = 124
    f["int64_field"] = 1234567890123
    f["real_field"] = 1.25
    f["str_field"] = "foo2"
    f["other_field"] = "initial2"
    f["dst_only_field"] = "dst_only_field2"
    f.SetFID(21)
    dst_lyr.CreateFeature(f)

    f = ogr.Feature(dst_lyr.GetLayerDefn())
    f["int_field"] = 125
    f["int64_field"] = 1234567890123
    f["real_field"] = 1.25
    f["str_field"] = "foo3"
    f["other_field"] = "initial3"
    f["dst_only_field"] = "dst_only_field3"
    f.SetFID(22)
    dst_lyr.CreateFeature(f)

    f = ogr.Feature(dst_lyr.GetLayerDefn())
    f["int_field"] = 125
    f["int64_field"] = 1234567890123
    f["real_field"] = 1.25
    f["str_field"] = "foo3"
    f["other_field"] = "initial4"
    f["dst_only_field"] = "dst_only_field4"
    f.SetFID(23)
    dst_lyr.CreateFeature(f)

    tab_pct = [0]

    def my_progress(pct, msg, user_data):
        assert pct >= tab_pct[0]
        tab_pct[0] = pct
        return True

    assert gdal.alg.vector.update(
        input=src_ds,
        output=dst_ds,
        key=["int_field", "int64_field", "real_field", "str_field"],
        mode="update-only",
        progress=my_progress,
    )

    assert tab_pct[0] == 1.0

    assert dst_lyr.GetFeatureCount() == 4

    f = dst_lyr.GetFeature(20)
    assert f["int_field"] == 123
    assert f["int64_field"] == 1234567890123
    assert f["real_field"] == 1.25
    assert f["str_field"] == "foo"
    assert f["other_field"] == "update"
    assert f["dst_only_field"] == "dst_only_field"

    f = dst_lyr.GetFeature(21)
    assert f["int_field"] == 124
    assert f["int64_field"] == 1234567890123
    assert f["real_field"] == 1.25
    assert f["str_field"] == "foo2"
    assert f["other_field"] == "update2"
    assert f["dst_only_field"] == "dst_only_field2"

    f = dst_lyr.GetFeature(22)
    assert f["int_field"] == 125
    assert f["int64_field"] == 1234567890123
    assert f["real_field"] == 1.25
    assert f["str_field"] == "foo3"
    assert f["other_field"] == "initial3"
    assert f["dst_only_field"] == "dst_only_field3"

    f = dst_lyr.GetFeature(23)
    assert f["int_field"] == 125
    assert f["int64_field"] == 1234567890123
    assert f["real_field"] == 1.25
    assert f["str_field"] == "foo3"
    assert f["other_field"] == "initial4"
    assert f["dst_only_field"] == "dst_only_field4"


def test_gdalalg_vector_update_by_fid():

    src_ds = gdal.GetDriverByName("MEM").CreateVector("src")
    src_lyr = src_ds.CreateLayer("test")
    src_lyr.CreateField(ogr.FieldDefn("some_field", ogr.OFTString))
    f = ogr.Feature(src_lyr.GetLayerDefn())
    f["some_field"] = "foo"
    f.SetFID(1)
    src_lyr.CreateFeature(f)

    dst_ds = gdal.GetDriverByName("MEM").CreateVector("dst")
    dst_lyr = dst_ds.CreateLayer("test")
    dst_lyr.CreateField(ogr.FieldDefn("some_field", ogr.OFTString))
    f = ogr.Feature(dst_lyr.GetLayerDefn())
    f["some_field"] = "bar"
    f.SetFID(1)
    dst_lyr.CreateFeature(f)

    assert gdal.alg.vector.update(input=src_ds, output=dst_ds)

    f = dst_lyr.GetFeature(1)
    assert f["some_field"] == "foo"


@pytest.mark.parametrize("mode", ["merge", "update-only", "append-only"])
def test_gdalalg_vector_update_mode(mode):

    src_ds = gdal.GetDriverByName("MEM").CreateVector("src")
    src_lyr = src_ds.CreateLayer("test")
    src_lyr.CreateField(ogr.FieldDefn("some_field", ogr.OFTString))
    f = ogr.Feature(src_lyr.GetLayerDefn())
    f["some_field"] = "foo"
    f.SetFID(0)
    src_lyr.CreateFeature(f)

    f = ogr.Feature(src_lyr.GetLayerDefn())
    f["some_field"] = "foo2"
    f.SetFID(1)
    src_lyr.CreateFeature(f)

    dst_ds = gdal.GetDriverByName("MEM").CreateVector("dst")
    dst_lyr = dst_ds.CreateLayer("test")
    dst_lyr.CreateField(ogr.FieldDefn("some_field", ogr.OFTString))
    f = ogr.Feature(dst_lyr.GetLayerDefn())
    f["some_field"] = "bar"
    f.SetFID(0)
    dst_lyr.CreateFeature(f)

    assert gdal.alg.vector.update(input=src_ds, output=dst_ds, mode=mode)

    if mode == "merge":
        assert dst_lyr.GetFeatureCount() == 2

        f = dst_lyr.GetFeature(0)
        assert f["some_field"] == "foo"

        f = dst_lyr.GetFeature(1)
        assert f["some_field"] == "foo2"

    elif mode == "update-only":
        assert dst_lyr.GetFeatureCount() == 1

        f = dst_lyr.GetFeature(0)
        assert f["some_field"] == "foo"

    else:
        assert dst_lyr.GetFeatureCount() == 2

        f = dst_lyr.GetFeature(0)
        assert f["some_field"] == "bar"

        f = dst_lyr.GetFeature(1)
        assert f["some_field"] == "foo2"


def test_gdalalg_vector_update_src_is_same_as_dst_pointer():

    ds = gdal.GetDriverByName("MEM").CreateVector("")

    with pytest.raises(Exception, match="Input and output datasets must be different"):
        gdal.alg.vector.update(input=ds, output=ds)


def test_gdalalg_vector_update_src_is_same_as_dst_filename(tmp_vsimem):

    filename = tmp_vsimem / "tmp.dbf"
    with gdal.GetDriverByName("ESRI Shapefile").CreateVector(filename) as ds:
        ds.CreateLayer("test")

    with pytest.raises(Exception, match="Input and output datasets must be different"):
        gdal.alg.vector.update(
            input=gdal.OpenEx(filename, gdal.OF_VECTOR),
            output=gdal.OpenEx(filename, gdal.OF_VECTOR | gdal.OF_UPDATE),
        )


def test_gdalalg_vector_missing_layer_names():

    with pytest.raises(
        Exception, match="Please specify the 'input-layer' and 'output-layer' arguments"
    ):
        gdal.alg.vector.update(
            input=gdal.GetDriverByName("MEM").CreateVector(""),
            output=gdal.GetDriverByName("MEM").CreateVector(""),
        )


def test_gdalalg_vector_missing_input_layer_name():

    with pytest.raises(Exception, match="Please specify the 'input-layer' argument"):
        gdal.alg.vector.update(
            input=gdal.GetDriverByName("MEM").CreateVector(""),
            output=gdal.GetDriverByName("MEM").CreateVector(""),
            output_layer="non_existing",
        )


def test_gdalalg_vector_missing_output_layer_name():

    src_ds = gdal.GetDriverByName("MEM").CreateVector("")
    src_ds.CreateLayer("src")

    with pytest.raises(Exception, match="Please specify the 'output-layer' argument"):
        gdal.alg.vector.update(
            input=src_ds, output=gdal.GetDriverByName("MEM").CreateVector("")
        )


def test_gdalalg_vector_wrong_input_layer():

    with pytest.raises(
        Exception, match="No layer named 'non_existing' in input dataset"
    ):
        gdal.alg.vector.update(
            input=gdal.GetDriverByName("MEM").CreateVector(""),
            output=gdal.GetDriverByName("MEM").CreateVector(""),
            input_layer="non_existing",
        )


def test_gdalalg_vector_wrong_output_layer():

    src_ds = gdal.GetDriverByName("MEM").CreateVector("")
    src_ds.CreateLayer("src")

    with pytest.raises(
        Exception, match="No layer named 'non_existing' in output dataset"
    ):
        gdal.alg.vector.update(
            input=src_ds,
            output=gdal.GetDriverByName("MEM").CreateVector(""),
            output_layer="non_existing",
        )


def test_gdalalg_vector_non_existing_key_column_in_input():

    src_ds = gdal.GetDriverByName("MEM").CreateVector("")
    src_ds.CreateLayer("src")

    dst_ds = gdal.GetDriverByName("MEM").CreateVector("")
    dst_ds.CreateLayer("dst")

    with pytest.raises(
        Exception, match="Cannot find field 'non_existing' in input layer"
    ):
        gdal.alg.vector.update(input=src_ds, output=dst_ds, key="non_existing")


def test_gdalalg_vector_non_existing_key_column_in_output():

    src_ds = gdal.GetDriverByName("MEM").CreateVector("")
    src_lyr = src_ds.CreateLayer("src")
    src_lyr.CreateField(ogr.FieldDefn("non_existing"))

    dst_ds = gdal.GetDriverByName("MEM").CreateVector("")
    dst_ds.CreateLayer("dst")

    with pytest.raises(
        Exception, match="Cannot find field 'non_existing' in output layer"
    ):
        gdal.alg.vector.update(input=src_ds, output=dst_ds, key="non_existing")


def test_gdalalg_vector_key_unsupported_type():

    src_ds = gdal.GetDriverByName("MEM").CreateVector("")
    src_lyr = src_ds.CreateLayer("src")
    src_lyr.CreateField(ogr.FieldDefn("unsupported_type", ogr.OFTBinary))

    dst_ds = gdal.GetDriverByName("MEM").CreateVector("")
    dst_ds.CreateLayer("dst")

    with pytest.raises(
        Exception,
        match="Type of field 'unsupported_type' is not one of those supported for a key field: String, Integer, Integer64, Real",
    ):
        gdal.alg.vector.update(input=src_ds, output=dst_ds, key="unsupported_type")


def test_gdalalg_vector_interrupted_by_user():

    src_ds = gdal.GetDriverByName("MEM").CreateVector("")
    src_lyr = src_ds.CreateLayer("src")
    f = ogr.Feature(src_lyr.GetLayerDefn())
    src_lyr.CreateFeature(f)

    dst_ds = gdal.GetDriverByName("MEM").CreateVector("")
    dst_ds.CreateLayer("dst")

    def my_progress(pct, msg, user_data):
        return False

    with pytest.raises(Exception, match="Interrupted by user"):
        gdal.alg.vector.update(input=src_ds, output=dst_ds, progress=my_progress)


def test_gdalalg_vector_update_pipeline_intermediate_step(tmp_vsimem):

    src_ds = gdal.GetDriverByName("MEM").CreateVector("")
    src_lyr = src_ds.CreateLayer("src")
    f = ogr.Feature(src_lyr.GetLayerDefn())
    src_lyr.CreateFeature(f)

    with gdal.GetDriverByName("ESRI Shapefile").CreateVector(
        tmp_vsimem / "out.dbf"
    ) as dst_ds:
        dst_ds.CreateLayer("out")

    with gdal.alg.vector.pipeline(
        input=src_ds, pipeline=f"read ! update {tmp_vsimem}/out.dbf ! info"
    ) as alg:
        j = alg.Output()
        assert j["layers"][0]["featureCount"] == 1
