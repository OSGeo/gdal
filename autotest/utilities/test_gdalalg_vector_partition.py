#!/usr/bin/env pytest
# -*- coding: utf-8 -*-
###############################################################################
# Project:  GDAL/OGR Test Suite
# Purpose:  'gdal vector partition' testing
# Author:   Even Rouault <even dot rouault @ spatialys.com>
#
###############################################################################
# Copyright (c) 2025, Even Rouault <even dot rouault at spatialys.com>
#
# SPDX-License-Identifier: MIT
###############################################################################

import gdaltest
import pytest

from osgeo import gdal, ogr


def src_ds(unique_constraint=False):
    ds = gdal.GetDriverByName("MEM").CreateVector("")
    lyr = ds.CreateLayer("test", geom_type=ogr.wkbPoint, options=["FID=my_fid"])
    str_field = ogr.FieldDefn("str_field")
    if unique_constraint:
        str_field.SetUnique(True)
    lyr.CreateField(str_field)
    lyr.CreateField(ogr.FieldDefn("int_field", ogr.OFTInteger))
    lyr.CreateField(ogr.FieldDefn("int64 field", ogr.OFTInteger64))
    lyr.CreateField(ogr.FieldDefn("real_field", ogr.OFTReal))

    f = ogr.Feature(lyr.GetLayerDefn())
    f["str_field"] = "one"
    f["int_field"] = 1
    f["int64 field"] = 1234567890123
    f["real_field"] = 1.5
    f.SetFID(10)
    f.SetGeometry(ogr.CreateGeometryFromWkt("POINT (1 2)"))
    lyr.CreateFeature(f)

    f = ogr.Feature(lyr.GetLayerDefn())
    f["str_field"] = "one"
    f["int_field"] = 2
    f.SetFID(11)
    lyr.CreateFeature(f)

    f = ogr.Feature(lyr.GetLayerDefn())
    f["str_field"] = "two"
    f["int_field"] = 1
    f.SetFID(12)
    lyr.CreateFeature(f)

    lyr = ds.CreateLayer("non spatial", geom_type=ogr.wkbNone)
    lyr.CreateField(ogr.FieldDefn("str_field"))
    lyr.CreateField(ogr.FieldDefn("int_field", ogr.OFTInteger))
    lyr.CreateField(ogr.FieldDefn("int64 field", ogr.OFTInteger64))
    lyr.CreateField(ogr.FieldDefn("real_field", ogr.OFTReal))
    f = ogr.Feature(lyr.GetLayerDefn())
    lyr.CreateFeature(f)

    return ds


@pytest.mark.require_driver("GPKG")
@pytest.mark.parametrize("max_cache_size", [100, 1])
def test_gdalalg_vector_partition_str_field(tmp_vsimem, max_cache_size):

    last_pct = [0]

    def my_progress(pct, msg, user_data):
        assert pct >= last_pct[0]
        last_pct[0] = pct
        return True

    gdal.Run(
        "vector",
        "partition",
        input=src_ds(),
        output=tmp_vsimem / "out",
        format="GPKG",
        field="str_field",
        max_cache_size=max_cache_size,
        progress=my_progress,
    )

    assert last_pct[0] == 1

    with ogr.Open(tmp_vsimem / "out/test/str_field=one/part_0000000001.gpkg") as ds:
        lyr = ds.GetLayer(0)
        assert lyr.GetFIDColumn() == "my_fid"

        f = lyr.GetNextFeature()
        assert f["str_field"] == "one"
        assert f["int_field"] == 1
        assert f["int64 field"] == 1234567890123
        assert f["real_field"] == 1.5
        assert f.GetFID() == 10
        assert f.GetGeometryRef().ExportToWkt() == "POINT (1 2)"

        f = lyr.GetNextFeature()
        assert f["str_field"] == "one"
        assert f["int_field"] == 2

        assert lyr.GetNextFeature() is None

    with ogr.Open(tmp_vsimem / "out/test/str_field=two/part_0000000001.gpkg") as ds:
        lyr = ds.GetLayer(0)

        f = lyr.GetNextFeature()
        assert f["str_field"] == "two"
        assert f["int_field"] == 1

        assert lyr.GetNextFeature() is None

    with ogr.Open(
        tmp_vsimem
        / "out/non%20spatial/str_field=__HIVE_DEFAULT_PARTITION__/part_0000000001.gpkg"
    ) as ds:
        lyr = ds.GetLayer(0)
        assert lyr.GetFeatureCount() == 1


@pytest.mark.require_driver("GPKG")
@pytest.mark.parametrize("transaction_size", [100, 1])
def test_gdalalg_vector_partition_int_field(tmp_vsimem, transaction_size):

    gdal.Run(
        "vector",
        "partition",
        input=src_ds(),
        output=tmp_vsimem / "out",
        format="GPKG",
        field="int_field",
        transaction_size=transaction_size,
    )

    with ogr.Open(tmp_vsimem / "out/test/int_field=1/part_0000000001.gpkg") as ds:
        lyr = ds.GetLayer(0)
        assert lyr.GetFIDColumn() == "my_fid"

        f = lyr.GetNextFeature()
        assert f["str_field"] == "one"
        assert f["int_field"] == 1
        assert f["int64 field"] == 1234567890123
        assert f["real_field"] == 1.5
        assert f.GetFID() == 10
        assert f.GetGeometryRef().ExportToWkt() == "POINT (1 2)"

        f = lyr.GetNextFeature()
        assert f["str_field"] == "two"
        assert f["int_field"] == 1

        assert lyr.GetNextFeature() is None

    with ogr.Open(tmp_vsimem / "out/test/int_field=2/part_0000000001.gpkg") as ds:
        lyr = ds.GetLayer(0)

        f = lyr.GetNextFeature()
        assert f["str_field"] == "one"
        assert f["int_field"] == 2

        assert lyr.GetNextFeature() is None


@pytest.mark.require_driver("GPKG")
def test_gdalalg_vector_partition_int64_field(tmp_vsimem):

    gdal.Run(
        "vector",
        "partition",
        input=src_ds(),
        output=tmp_vsimem / "out",
        format="GPKG",
        field="int64 field",
    )

    with ogr.Open(
        tmp_vsimem / "out/test/int64%20field=1234567890123/part_0000000001.gpkg"
    ) as ds:
        lyr = ds.GetLayer(0)
        assert lyr.GetFIDColumn() == "my_fid"

        f = lyr.GetNextFeature()
        assert f["str_field"] == "one"
        assert f["int_field"] == 1
        assert f["int64 field"] == 1234567890123
        assert f["real_field"] == 1.5
        assert f.GetFID() == 10
        assert f.GetGeometryRef().ExportToWkt() == "POINT (1 2)"

        assert lyr.GetNextFeature() is None

    with ogr.Open(
        tmp_vsimem
        / "out/test/int64%20field=__HIVE_DEFAULT_PARTITION__/part_0000000001.gpkg"
    ) as ds:
        lyr = ds.GetLayer(0)

        f = lyr.GetNextFeature()
        assert f["str_field"] == "one"
        assert f["int_field"] == 2

        f = lyr.GetNextFeature()
        assert f["str_field"] == "two"
        assert f["int_field"] == 1

        assert lyr.GetNextFeature() is None


@pytest.mark.require_driver("GPKG")
def test_gdalalg_vector_partition_omit_partitioned_field(tmp_vsimem):

    gdal.Run(
        "vector",
        "partition",
        input=src_ds(),
        output=tmp_vsimem / "out",
        format="GPKG",
        field="str_field",
        omit_partitioned_field=True,
    )

    with ogr.Open(tmp_vsimem / "out/test/str_field=one/part_0000000001.gpkg") as ds:
        lyr = ds.GetLayer(0)
        assert lyr.GetFIDColumn() == "my_fid"
        assert lyr.GetLayerDefn().GetFieldIndex("str_field") == -1

        f = lyr.GetNextFeature()
        assert f["int_field"] == 1
        assert f["int64 field"] == 1234567890123
        assert f["real_field"] == 1.5
        assert f.GetFID() == 10
        assert f.GetGeometryRef().ExportToWkt() == "POINT (1 2)"

        f = lyr.GetNextFeature()
        assert f["int_field"] == 2

        assert lyr.GetNextFeature() is None

    with ogr.Open(tmp_vsimem / "out/test/str_field=two/part_0000000001.gpkg") as ds:
        lyr = ds.GetLayer(0)
        assert lyr.GetLayerDefn().GetFieldIndex("str_field") == -1

        f = lyr.GetNextFeature()
        assert f["int_field"] == 1

        assert lyr.GetNextFeature() is None


@pytest.mark.require_driver("GPKG")
def test_gdalalg_vector_partition_overwrite_append(tmp_vsimem):

    gdal.Run(
        "vector",
        "partition",
        input=src_ds(),
        output=tmp_vsimem / "out",
        format="GPKG",
        field="str_field",
    )

    with pytest.raises(
        Exception,
        match="already exists. Specify --overwrite or --append",
    ):
        gdal.Run(
            "vector",
            "partition",
            input=src_ds(),
            output=tmp_vsimem / "out",
            format="GPKG",
            field="str_field",
        )

    gdal.Run(
        "vector",
        "partition",
        input=src_ds(),
        output=tmp_vsimem / "out",
        format="GPKG",
        field="str_field",
        overwrite=True,
    )

    with ogr.Open(tmp_vsimem / "out/test/str_field=one/part_0000000001.gpkg") as ds:
        lyr = ds.GetLayer(0)
        assert lyr.GetFeatureCount() == 2

    gdal.Run(
        "vector",
        "partition",
        input=src_ds(),
        output=tmp_vsimem / "out",
        format="GPKG",
        field="str_field",
        overwrite=True,
    )

    with ogr.Open(tmp_vsimem / "out/test/str_field=one/part_0000000001.gpkg") as ds:
        lyr = ds.GetLayer(0)
        assert lyr.GetFeatureCount() == 2

    gdal.Run(
        "vector",
        "partition",
        input=src_ds(),
        output=tmp_vsimem / "out",
        format="GPKG",
        field="str_field",
        append=True,
    )

    with ogr.Open(tmp_vsimem / "out/test/str_field=one/part_0000000001.gpkg") as ds:
        lyr = ds.GetLayer(0)
        assert lyr.GetFeatureCount() == 4

    gdal.Mkdir(tmp_vsimem / "out_not_a_partition_dir", 0o755)
    gdal.Mkdir(tmp_vsimem / "out_not_a_partition_dir/foo", 0o755)
    with pytest.raises(
        Exception,
        match="it does not look like a directory generated by this utility",
    ):
        gdal.Run(
            "vector",
            "partition",
            input=src_ds(),
            output=tmp_vsimem / "out_not_a_partition_dir",
            format="GPKG",
            field="str_field",
            overwrite=True,
        )


@pytest.mark.require_driver("GPKG")
@pytest.mark.require_driver("JML")
def test_gdalalg_vector_partition_errors(tmp_vsimem):

    with pytest.raises(
        Exception,
        match="Cannot infer output format. Please specify 'output-format' argument",
    ):
        gdal.Run(
            "vector",
            "partition",
            input=src_ds(),
            output=tmp_vsimem / "out",
            field="str_field",
        )

    with pytest.raises(Exception, match="Output driver has no known file extension"):
        gdal.Run(
            "vector",
            "partition",
            input=src_ds(),
            output=tmp_vsimem / "out",
            format="MEM",
            field="str_field",
        )

    with pytest.raises(
        Exception,
        match="Invalid value for argument 'output-format'. Driver 'SXF' does not have write support",
    ):
        gdal.Run(
            "vector",
            "partition",
            input=src_ds(),
            output=tmp_vsimem / "out",
            format="SXF",
            field="str_field",
        )

    with pytest.raises(Exception, match="Driver 'JML' does not support update"):
        gdal.Run(
            "vector",
            "partition",
            input=src_ds(),
            output=tmp_vsimem / "out",
            format="JML",
            field="str_field",
            append=True,
        )

    with pytest.raises(
        Exception,
        match="Cannot find field 'non_existing_field' in layer 'test'",
    ):
        gdal.Run(
            "vector",
            "partition",
            input=src_ds(),
            output=tmp_vsimem / "out_non_existing_field",
            format="GPKG",
            field="non_existing_field",
        )

    with pytest.raises(
        Exception,
        match="Field 'real_field' not valid for partitioning",
    ):
        gdal.Run(
            "vector",
            "partition",
            input=src_ds(),
            output=tmp_vsimem / "out",
            format="GPKG",
            field="real_field",
        )

    with pytest.raises(
        Exception, match="Cannot create directory '/i_do/not_exits/out'"
    ):
        gdal.Run(
            "vector",
            "partition",
            input=src_ds(),
            output="/i_do/not_exits/out",
            format="GPKG",
            field="str_field",
        )

    with pytest.raises(Exception, match="may not contain special characters or spaces"):
        gdal.Run(
            "vector",
            "partition",
            input=src_ds(),
            output=tmp_vsimem / "out_cannot_create_layer",
            format="GPKG",
            field="str_field",
            layer_creation_option={"FID": "!!invalid!!"},
        )

    with pytest.raises(Exception, match="Invalid value for max-file-size"):
        gdal.Run(
            "vector",
            "partition",
            input=src_ds(),
            output=tmp_vsimem / "out",
            format="GPKG",
            field="str_field",
            max_file_size=-1,
        )

    with pytest.raises(Exception, match="max-file-size should be at least one MB"):
        gdal.Run(
            "vector",
            "partition",
            input=src_ds(),
            output=tmp_vsimem / "out",
            format="GPKG",
            field="str_field",
            max_file_size=1,
        )

    my_src_ds = gdal.GetDriverByName("MEM").CreateVector("")
    lyr = my_src_ds.CreateLayer("test", geom_type=ogr.wkbPoint)
    lyr.CreateField(ogr.FieldDefn("str_field", ogr.OFTString))
    lyr.CreateField(ogr.FieldDefn("dup_field", ogr.OFTString))
    lyr.CreateField(ogr.FieldDefn("dup_field", ogr.OFTString))
    lyr.CreateFeature(ogr.Feature(lyr.GetLayerDefn()))
    with pytest.raises(Exception, match="A field with the same name already exists"):
        gdal.Run(
            "vector",
            "partition",
            input=my_src_ds,
            output=tmp_vsimem / "out_cannot_create_field",
            format="GPKG",
            field="str_field",
        )

    my_src_ds = gdal.GetDriverByName("MEM").CreateVector("")
    lyr = my_src_ds.CreateLayer("test", geom_type=ogr.wkbPoint)
    lyr.CreateField(ogr.FieldDefn("str_field", ogr.OFTString))
    lyr.CreateGeomField(ogr.GeomFieldDefn("geom1", ogr.wkbPoint))
    lyr.CreateGeomField(ogr.GeomFieldDefn("geom2", ogr.wkbPoint))
    lyr.CreateFeature(ogr.Feature(lyr.GetLayerDefn()))
    with pytest.raises(
        Exception, match="Cannot create more than one geometry field in GeoPackage"
    ):
        gdal.Run(
            "vector",
            "partition",
            input=my_src_ds,
            output=tmp_vsimem / "out_cannot_create_geom_field",
            format="GPKG",
            field="str_field",
        )


def _has_arrow_dataset():
    drv = gdal.GetDriverByName("Parquet")
    return drv is not None and drv.GetMetadataItem("ARROW_DATASET") is not None


@pytest.mark.require_driver("Parquet")
def test_gdalalg_vector_partition_parquet(tmp_vsimem):

    gdal.Run(
        "vector",
        "partition",
        input=src_ds(),
        output=tmp_vsimem / "out",
        format="Parquet",
        field="str_field",
    )

    with ogr.Open(tmp_vsimem / "out/test/str_field=one/part_0000000001.parquet") as ds:
        lyr = ds.GetLayer(0)
        assert lyr.GetLayerDefn().GetFieldIndex("str_field") == -1

        f = lyr.GetNextFeature()
        assert f["int_field"] == 1
        assert f["int64 field"] == 1234567890123
        assert f["real_field"] == 1.5
        assert f.GetGeometryRef().ExportToWkt() == "POINT (1 2)"

        f = lyr.GetNextFeature()
        assert f["int_field"] == 2

        assert lyr.GetNextFeature() is None

    with ogr.Open(tmp_vsimem / "out/test/str_field=two/part_0000000001.parquet") as ds:
        lyr = ds.GetLayer(0)
        assert lyr.GetLayerDefn().GetFieldIndex("str_field") == -1

        f = lyr.GetNextFeature()
        assert f["int_field"] == 1

        assert lyr.GetNextFeature() is None

    assert gdal.VSIStatL(tmp_vsimem / "out/test/_metadata") is not None

    if _has_arrow_dataset():
        with ogr.Open("PARQUET:" + str(tmp_vsimem) + "/out/test") as ds:
            lyr = ds.GetLayer(0)
            assert lyr.GetFeatureCount() == 3

            with ds.ExecuteSQL(
                "GET_SET_FILES_ASKED_TO_BE_OPEN", dialect="_DEBUG_"
            ) as sql_lyr:
                set_files = [f.GetField(0) for f in sql_lyr]
                assert set_files == [str(tmp_vsimem / "out/test/_metadata")]

            lyr.SetAttributeFilter("str_field = 'two'")
            assert lyr.GetFeatureCount() == 1

            with ds.ExecuteSQL(
                "GET_SET_FILES_ASKED_TO_BE_OPEN", dialect="_DEBUG_"
            ) as sql_lyr:
                set_files = [f.GetField(0) for f in sql_lyr]
                assert set_files == [
                    str(tmp_vsimem / "out/test/str_field=two/part_0000000001.parquet")
                ]


@pytest.mark.require_driver("Parquet")
@pytest.mark.parametrize("max_cache_size", [100, 1])
def test_gdalalg_vector_partition_parquet_two_fields(tmp_vsimem, max_cache_size):

    gdal.Run(
        "vector",
        "partition",
        input=src_ds(),
        output=tmp_vsimem / "out",
        format="Parquet",
        field=["str_field", "int_field"],
        max_cache_size=max_cache_size,
    )

    with ogr.Open(
        tmp_vsimem / "out/test/str_field=one/int_field=1/part_0000000001.parquet"
    ) as ds:
        lyr = ds.GetLayer(0)
        assert lyr.GetFeatureCount() == 1

    with ogr.Open(
        tmp_vsimem / "out/test/str_field=one/int_field=2/part_0000000001.parquet"
    ) as ds:
        lyr = ds.GetLayer(0)
        assert lyr.GetFeatureCount() == 1

    with ogr.Open(
        tmp_vsimem / "out/test/str_field=two/int_field=1/part_0000000001.parquet"
    ) as ds:
        lyr = ds.GetLayer(0)
        assert lyr.GetFeatureCount() == 1


@pytest.mark.require_driver("FlatGeoBuf")
def test_gdalalg_vector_partition_flatgeobuf(tmp_vsimem):

    gdal.Run(
        "vector",
        "partition",
        input=src_ds(),
        output=tmp_vsimem / "out",
        format="FlatGeoBuf",
        field="str_field",
        layer_creation_option=["SPATIAL_INDEX=NO"],
    )

    with ogr.Open(tmp_vsimem / "out/test/str_field=one/part_0000000001.fgb") as ds:
        lyr = ds.GetLayer(0)

        f = lyr.GetNextFeature()
        assert f["str_field"] == "one"
        assert f["int_field"] == 1
        assert f["int64 field"] == 1234567890123
        assert f["real_field"] == 1.5
        assert f.GetGeometryRef().ExportToWkt() == "POINT (1 2)"

        f = lyr.GetNextFeature()
        assert f["str_field"] == "one"
        assert f["int_field"] == 2

        assert lyr.GetNextFeature() is None

    with ogr.Open(tmp_vsimem / "out/test/str_field=two/part_0000000001.fgb") as ds:
        lyr = ds.GetLayer(0)

        f = lyr.GetNextFeature()
        assert f["str_field"] == "two"
        assert f["int_field"] == 1

        assert lyr.GetNextFeature() is None


@pytest.mark.require_driver("GPKG")
def test_gdalalg_vector_partition_feature_limit(tmp_vsimem):

    gdal.Run(
        "vector",
        "partition",
        input=src_ds(),
        output=tmp_vsimem / "out",
        format="GPKG",
        field="str_field",
        feature_limit=1,
    )

    with ogr.Open(tmp_vsimem / "out/test/str_field=one/part_0000000001.gpkg") as ds:
        lyr = ds.GetLayer(0)
        assert lyr.GetFeatureCount() == 1

    with ogr.Open(tmp_vsimem / "out/test/str_field=one/part_0000000002.gpkg") as ds:
        lyr = ds.GetLayer(0)
        assert lyr.GetFeatureCount() == 1

    with ogr.Open(tmp_vsimem / "out/test/str_field=two/part_0000000001.gpkg") as ds:
        lyr = ds.GetLayer(0)
        assert lyr.GetFeatureCount() == 1

    gdal.Run(
        "vector",
        "partition",
        input=src_ds(),
        output=tmp_vsimem / "out",
        format="GPKG",
        field="str_field",
        feature_limit=1,
        append=True,
    )

    with ogr.Open(tmp_vsimem / "out/test/str_field=one/part_0000000001.gpkg") as ds:
        lyr = ds.GetLayer(0)
        assert lyr.GetFeatureCount() == 1

    with ogr.Open(tmp_vsimem / "out/test/str_field=one/part_0000000002.gpkg") as ds:
        lyr = ds.GetLayer(0)
        assert lyr.GetFeatureCount() == 1

    with ogr.Open(tmp_vsimem / "out/test/str_field=one/part_0000000003.gpkg") as ds:
        lyr = ds.GetLayer(0)
        assert lyr.GetFeatureCount() == 1

    with ogr.Open(tmp_vsimem / "out/test/str_field=one/part_0000000004.gpkg") as ds:
        lyr = ds.GetLayer(0)
        assert lyr.GetFeatureCount() == 1

    with ogr.Open(tmp_vsimem / "out/test/str_field=two/part_0000000001.gpkg") as ds:
        lyr = ds.GetLayer(0)
        assert lyr.GetFeatureCount() == 1

    with ogr.Open(tmp_vsimem / "out/test/str_field=two/part_0000000002.gpkg") as ds:
        lyr = ds.GetLayer(0)
        assert lyr.GetFeatureCount() == 1


@pytest.mark.require_driver("GPKG")
def test_gdalalg_vector_partition_max_file_size_gpkg(tmp_vsimem):

    src_ds = gdal.GetDriverByName("MEM").CreateVector("")
    lyr = src_ds.CreateLayer("test", geom_type=ogr.wkbPoint, options=["FID=my_fid"])
    lyr.CreateField(ogr.FieldDefn("str_field"))
    lyr.CreateField(ogr.FieldDefn("int_field", ogr.OFTInteger))
    lyr.CreateField(ogr.FieldDefn("int64 field", ogr.OFTInteger64))
    lyr.CreateField(ogr.FieldDefn("real_field", ogr.OFTReal))
    lyr.CreateField(ogr.FieldDefn("binary_field", ogr.OFTBinary))
    lyr.CreateField(ogr.FieldDefn("time_field", ogr.OFTTime))
    lyr.CreateField(ogr.FieldDefn("date_field", ogr.OFTDate))
    lyr.CreateField(ogr.FieldDefn("datetime_field", ogr.OFTDateTime))

    f = ogr.Feature(lyr.GetLayerDefn())
    f["str_field"] = "one"
    f["int_field"] = 1
    f["int64 field"] = 1234567890123
    f["real_field"] = 1.5
    f["binary_field"] = b"\x01"
    f["time_field"] = "12:34:56.789"
    f["date_field"] = "2025-09-02"
    f["datetime_field"] = "2025-09-02T12:34:56.789+05:30"
    f.SetGeometry(ogr.CreateGeometryFromWkt("POINT (1 2)"))
    for i in range(5000):
        f.SetFID(-1)
        lyr.CreateFeature(f)

    gdal.Run(
        "vector",
        "partition",
        input=src_ds,
        output=tmp_vsimem / "out",
        format="GPKG",
        field="str_field",
        max_file_size="1 MB",
    )

    file_size = gdal.VSIStatL(
        tmp_vsimem / "out/test/str_field=one/part_0000000001.gpkg"
    ).size
    assert file_size > (1024 * 1024) * 0.8
    assert file_size < (1024 * 1024) * 1.1

    fc = 0
    with ogr.Open(tmp_vsimem / "out/test/str_field=one/part_0000000001.gpkg") as ds:
        lyr = ds.GetLayer(0)
        fc += lyr.GetFeatureCount()

    with ogr.Open(tmp_vsimem / "out/test/str_field=one/part_0000000002.gpkg") as ds:
        lyr = ds.GetLayer(0)
        fc += lyr.GetFeatureCount()
    assert fc == 5000


@pytest.mark.require_driver("SQLite")
def test_gdalalg_vector_partition_max_file_size_sqlite(tmp_vsimem):

    src_ds = gdal.GetDriverByName("MEM").CreateVector("")
    lyr = src_ds.CreateLayer("test", geom_type=ogr.wkbPoint, options=["FID=my_fid"])
    lyr.CreateField(ogr.FieldDefn("str_field"))
    lyr.CreateField(ogr.FieldDefn("int_field", ogr.OFTInteger))
    lyr.CreateField(ogr.FieldDefn("int64 field", ogr.OFTInteger64))
    lyr.CreateField(ogr.FieldDefn("real_field", ogr.OFTReal))
    lyr.CreateField(ogr.FieldDefn("binary_field", ogr.OFTBinary))
    lyr.CreateField(ogr.FieldDefn("time_field", ogr.OFTTime))
    lyr.CreateField(ogr.FieldDefn("date_field", ogr.OFTDate))
    lyr.CreateField(ogr.FieldDefn("datetime_field", ogr.OFTDateTime))
    lyr.CreateField(ogr.FieldDefn("strlist_field", ogr.OFTStringList))
    lyr.CreateField(ogr.FieldDefn("intlist_field", ogr.OFTIntegerList))
    lyr.CreateField(ogr.FieldDefn("int64list_field", ogr.OFTInteger64List))
    lyr.CreateField(ogr.FieldDefn("reallist_field", ogr.OFTRealList))

    f = ogr.Feature(lyr.GetLayerDefn())
    f["str_field"] = "one"
    f["int_field"] = 1
    f["int64 field"] = 1234567890123
    f["real_field"] = 1.5
    f["binary_field"] = b"\x01"
    f["time_field"] = "12:34:56.789"
    f["date_field"] = "2025-09-02"
    f["datetime_field"] = "2025-09-02T12:34:56.789+05:30"
    f["strlist_field"] = ["foo", "bar"]
    f["intlist_field"] = [1]
    f["int64list_field"] = [1234567890123]
    f["int64list_field"] = [1.5]
    f.SetGeometry(ogr.CreateGeometryFromWkt("POINT (1 2)"))
    for i in range(5000):
        f.SetFID(-1)
        lyr.CreateFeature(f)

    gdal.Run(
        "vector",
        "partition",
        input=src_ds,
        output=tmp_vsimem / "out",
        format="SQLite",
        field="str_field",
        max_file_size="1 MB",
    )

    file_size = gdal.VSIStatL(
        tmp_vsimem / "out/test/str_field=one/part_0000000001.sqlite"
    ).size
    assert file_size > (1024 * 1024) * 0.6
    assert file_size < (1024 * 1024) * 1.1

    fc = 0
    with ogr.Open(tmp_vsimem / "out/test/str_field=one/part_0000000001.sqlite") as ds:
        lyr = ds.GetLayer(0)
        fc += lyr.GetFeatureCount()

    with ogr.Open(tmp_vsimem / "out/test/str_field=one/part_0000000002.sqlite") as ds:
        lyr = ds.GetLayer(0)
        fc += lyr.GetFeatureCount()
    assert fc == 5000


@pytest.mark.require_driver("GPKG")
def test_gdalalg_vector_partition_append_error_reopening_file(tmp_vsimem):

    gdal.Run(
        "vector",
        "partition",
        input=src_ds(),
        output=tmp_vsimem / "out",
        format="GPKG",
        field="str_field",
    )

    with gdal.VSIFile(
        tmp_vsimem / "out/test/str_field=one/part_0000000001.gpkg", "wb"
    ) as f:
        f.write(b"invalid")

    with pytest.raises(
        Exception, match="not recognized as being in a supported file format"
    ):
        gdal.Run(
            "vector",
            "partition",
            input=src_ds(),
            output=tmp_vsimem / "out",
            format="GPKG",
            field="str_field",
            append=True,
        )


@pytest.mark.require_driver("GPKG")
def test_gdalalg_vector_partition_append_error_no_layer(tmp_vsimem):

    gdal.Run(
        "vector",
        "partition",
        input=src_ds(),
        output=tmp_vsimem / "out",
        format="GPKG",
        field="str_field",
    )

    with ogr.Open(
        tmp_vsimem / "out/test/str_field=one/part_0000000001.gpkg", update=1
    ) as ds:
        ds.DeleteLayer(0)

    with pytest.raises(Exception, match="No layer in"):
        gdal.Run(
            "vector",
            "partition",
            input=src_ds(),
            output=tmp_vsimem / "out",
            format="GPKG",
            field="str_field",
            append=True,
        )


@pytest.mark.require_driver("GPKG")
def test_gdalalg_vector_partition_append_error_incompatible_schema(tmp_vsimem):

    gdal.Run(
        "vector",
        "partition",
        input=src_ds(),
        output=tmp_vsimem / "out",
        format="GPKG",
        field="str_field",
    )

    with ogr.Open(
        tmp_vsimem / "out/test/str_field=one/part_0000000001.gpkg", update=1
    ) as ds:
        ds.GetLayer(0).DeleteField(0)

    with pytest.raises(
        Exception, match="does not have the same feature definition as the source layer"
    ):
        gdal.Run(
            "vector",
            "partition",
            input=src_ds(),
            output=tmp_vsimem / "out",
            format="GPKG",
            field="str_field",
            append=True,
        )


@pytest.mark.require_driver("GPKG")
def test_gdalalg_vector_partition_error_cannot_insert_feature(tmp_vsimem):

    with pytest.raises(Exception, match="Cannot insert feature 11"):
        gdal.Run(
            "vector",
            "partition",
            input=src_ds(unique_constraint=True),
            output=tmp_vsimem / "out",
            format="GPKG",
            field="str_field",
        )


@pytest.mark.require_driver("GPKG")
def test_gdalalg_vector_partition_skip_errors(tmp_vsimem):

    with pytest.raises(Exception, match="Transaction not established"):
        with gdaltest.error_raised(gdal.CE_Warning):
            gdal.Run(
                "vector",
                "partition",
                input=src_ds(unique_constraint=True),
                output=tmp_vsimem / "out",
                format="GPKG",
                field="str_field",
                skip_errors=True,
            )


@pytest.mark.require_driver("GPKG")
def test_gdalalg_vector_partition_error_interrupted_by_user(tmp_vsimem):
    def my_progress(pct, msg, user_data):
        return False

    with pytest.raises(Exception, match="Interrupted by user"):
        gdal.Run(
            "vector",
            "partition",
            input=src_ds(),
            output=tmp_vsimem / "out",
            format="GPKG",
            field="str_field",
            progress=my_progress,
        )


@pytest.mark.require_driver("Parquet")
def test_gdalalg_vector_partition_preexisting_filter(tmp_vsimem):

    gdal.Run(
        "vector",
        "pipeline",
        input=src_ds(),
        pipeline=f"read ! filter --where \"str_field='one'\" ! partition --output={tmp_vsimem}/out --field str_field --format Parquet",
    )

    with ogr.Open(tmp_vsimem / "out/test/str_field=one/part_0000000001.parquet") as ds:
        lyr = ds.GetLayer(0)
        assert lyr.GetFeatureCount() == 2

    assert (
        gdal.VSIStatL(tmp_vsimem / "out/test/str_field=two/part_0000000001.parquet")
        is None
    )


@pytest.mark.require_driver("GPKG")
def test_gdalalg_vector_partition_flat(tmp_vsimem):

    gdal.Run(
        "vector",
        "partition",
        input=src_ds(),
        output=tmp_vsimem / "out",
        format="GPKG",
        field="str_field",
        scheme="flat",
    )

    with ogr.Open(tmp_vsimem / "out/test_one_0000000001.gpkg") as ds:
        lyr = ds.GetLayer(0)
        assert lyr.GetFIDColumn() == "my_fid"

        f = lyr.GetNextFeature()
        assert f["str_field"] == "one"
        assert f["int_field"] == 1
        assert f["int64 field"] == 1234567890123
        assert f["real_field"] == 1.5
        assert f.GetFID() == 10
        assert f.GetGeometryRef().ExportToWkt() == "POINT (1 2)"

        f = lyr.GetNextFeature()
        assert f["str_field"] == "one"
        assert f["int_field"] == 2

        assert lyr.GetNextFeature() is None

    with ogr.Open(tmp_vsimem / "out/test_two_0000000001.gpkg") as ds:
        lyr = ds.GetLayer(0)

        f = lyr.GetNextFeature()
        assert f["str_field"] == "two"
        assert f["int_field"] == 1

        assert lyr.GetNextFeature() is None

    with ogr.Open(tmp_vsimem / "out/non%20spatial___NULL___0000000001.gpkg") as ds:
        lyr = ds.GetLayer(0)
        assert lyr.GetFeatureCount() == 1

    gdal.Mkdir(tmp_vsimem / "out" / "subdir", 0o755)

    with pytest.raises(
        Exception,
        match="it does not look like a directory generated by this utility",
    ):
        gdal.Run(
            "vector",
            "partition",
            input=src_ds(),
            output=tmp_vsimem / "out",
            format="GPKG",
            field="str_field",
            scheme="flat",
            overwrite=True,
        )

    gdal.Rmdir(tmp_vsimem / "out" / "subdir")

    gdal.Run(
        "vector",
        "partition",
        input=src_ds(),
        output=tmp_vsimem / "out",
        format="GPKG",
        field="str_field",
        scheme="flat",
        pattern="my_{LAYER_NAME}_%d_{FIELD_VALUE}",
        overwrite=True,
    )

    ogr.Open(tmp_vsimem / "out/my_test_1_one.gpkg")


@pytest.mark.require_driver("GPKG")
def test_gdalalg_vector_partition_pattern_error(tmp_vsimem):

    alg = gdal.Algorithm("vector", "partition")
    with pytest.raises(
        Exception,
        match="Value of argument 'pattern' is '', but should have at least 1 character",
    ):
        alg["pattern"] = ""
    with pytest.raises(Exception, match="Missing '%' character in pattern"):
        alg["pattern"] = "x"
    with pytest.raises(Exception, match="pattern value must include a single"):
        alg["pattern"] = "%"
    with pytest.raises(
        Exception, match="A single '%' character is expected in pattern"
    ):
        alg["pattern"] = "%%"
    with pytest.raises(Exception, match="pattern value must include a single"):
        alg["pattern"] = "%xd"
    with pytest.raises(Exception, match="pattern value must include a single"):
        alg["pattern"] = "%5"
    with pytest.raises(
        Exception,
        match=r"Number of digits in part number specifiation should be in \[1,10\] range",
    ):
        alg["pattern"] = "%11d"


@pytest.mark.require_driver("GPKG")
def test_gdalalg_vector_partition_geometry(tmp_vsimem):

    src_ds = gdal.GetDriverByName("MEM").CreateVector("")
    lyr = src_ds.CreateLayer("test")

    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetGeometry(ogr.CreateGeometryFromWkt("POLYGON ((0 0,0 1,1 1,0 0))"))
    lyr.CreateFeature(f)

    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetGeometry(ogr.CreateGeometryFromWkt("POINT (1 2)"))
    lyr.CreateFeature(f)

    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetGeometry(ogr.CreateGeometryFromWkt("POINT (3 4)"))
    lyr.CreateFeature(f)

    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetGeometry(ogr.CreateGeometryFromWkt("POINT Z (1 2 3)"))
    lyr.CreateFeature(f)

    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetGeometry(ogr.CreateGeometryFromWkt("POINT M (1 2 4)"))
    lyr.CreateFeature(f)

    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetGeometry(ogr.CreateGeometryFromWkt("POINT ZM (1 2 3 4)"))
    lyr.CreateFeature(f)

    f = ogr.Feature(lyr.GetLayerDefn())
    lyr.CreateFeature(f)

    gdal.alg.vector.partition(
        input=src_ds,
        output=tmp_vsimem / "out",
        output_format="GPKG",
        field="OGR_GEOMETRY",
    )

    with ogr.Open(
        tmp_vsimem / "out/test/OGR_GEOMETRY=POLYGON/part_0000000001.gpkg"
    ) as ds:
        lyr = ds.GetLayer(0)
        assert lyr.GetGeomType() == ogr.wkbPolygon
        assert lyr.GetFeatureCount() == 1
        assert (
            lyr.GetFeature(0).GetGeometryRef().ExportToIsoWkt()
            == "POLYGON ((0 0,0 1,1 1,0 0))"
        )

    with ogr.Open(
        tmp_vsimem / "out/test/OGR_GEOMETRY=POINT/part_0000000001.gpkg"
    ) as ds:
        lyr = ds.GetLayer(0)
        assert lyr.GetGeomType() == ogr.wkbPoint
        assert lyr.GetFeatureCount() == 2
        assert lyr.GetFeature(1).GetGeometryRef().ExportToIsoWkt() == "POINT (1 2)"
        assert lyr.GetFeature(2).GetGeometryRef().ExportToIsoWkt() == "POINT (3 4)"

    with ogr.Open(
        tmp_vsimem / "out/test/OGR_GEOMETRY=POINTZ/part_0000000001.gpkg"
    ) as ds:
        lyr = ds.GetLayer(0)
        assert lyr.GetGeomType() == ogr.wkbPoint25D
        assert lyr.GetFeatureCount() == 1
        assert lyr.GetFeature(3).GetGeometryRef().ExportToIsoWkt() == "POINT Z (1 2 3)"

    with ogr.Open(
        tmp_vsimem / "out/test/OGR_GEOMETRY=POINTM/part_0000000001.gpkg"
    ) as ds:
        lyr = ds.GetLayer(0)
        assert lyr.GetGeomType() == ogr.wkbPointM
        assert lyr.GetFeatureCount() == 1
        assert lyr.GetFeature(4).GetGeometryRef().ExportToIsoWkt() == "POINT M (1 2 4)"

    with ogr.Open(
        tmp_vsimem / "out/test/OGR_GEOMETRY=POINTZM/part_0000000001.gpkg"
    ) as ds:
        lyr = ds.GetLayer(0)
        assert lyr.GetGeomType() == ogr.wkbPointZM
        assert lyr.GetFeatureCount() == 1
        assert (
            lyr.GetFeature(5).GetGeometryRef().ExportToIsoWkt() == "POINT ZM (1 2 3 4)"
        )

    with ogr.Open(
        tmp_vsimem
        / "out/test/OGR_GEOMETRY=__HIVE_DEFAULT_PARTITION__/part_0000000001.gpkg"
    ) as ds:
        lyr = ds.GetLayer(0)
        assert lyr.GetGeomType() == ogr.wkbNone
        assert lyr.GetFeatureCount() == 1
        assert lyr.GetFeature(6).GetGeometryRef() is None


@pytest.mark.require_driver("SQLite")
def test_gdalalg_vector_partition_geometry_multi_geom_fields(tmp_vsimem):

    src_ds = gdal.GetDriverByName("MEM").CreateVector("")
    lyr = src_ds.CreateLayer("test")
    lyr.CreateGeomField(ogr.GeomFieldDefn("aux_geom", ogr.wkbUnknown))

    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetGeomField(0, ogr.CreateGeometryFromWkt("POLYGON ((0 0,0 1,1 1,0 0))"))
    f.SetGeomField(1, ogr.CreateGeometryFromWkt("POINT (1 2)"))
    lyr.CreateFeature(f)

    f = ogr.Feature(lyr.GetLayerDefn())
    f.SetGeomField(0, ogr.CreateGeometryFromWkt("POINT (3 4)"))
    f.SetGeomField(1, ogr.CreateGeometryFromWkt("LINESTRING (5 6,7 8)"))
    lyr.CreateFeature(f)

    gdal.alg.vector.partition(
        input=src_ds,
        output=tmp_vsimem / "out",
        output_format="SQLite",
        field="aux_geom",
    )

    with ogr.Open(tmp_vsimem / "out/test/aux_geom=POINT/part_0000000001.sqlite") as ds:
        lyr = ds.GetLayer(0)
        assert lyr.GetLayerDefn().GetGeomFieldDefn(0).GetType() == ogr.wkbUnknown
        assert lyr.GetLayerDefn().GetGeomFieldDefn(1).GetType() == ogr.wkbPoint
        assert lyr.GetFeatureCount() == 1
        assert (
            lyr.GetFeature(0).GetGeomFieldRef(0).ExportToIsoWkt()
            == "POLYGON ((0 0,0 1,1 1,0 0))"
        )
        assert lyr.GetFeature(0).GetGeomFieldRef(1).ExportToIsoWkt() == "POINT (1 2)"

    with ogr.Open(
        tmp_vsimem / "out/test/aux_geom=LINESTRING/part_0000000001.sqlite"
    ) as ds:
        lyr = ds.GetLayer(0)
        assert lyr.GetLayerDefn().GetGeomFieldDefn(0).GetType() == ogr.wkbUnknown
        assert lyr.GetLayerDefn().GetGeomFieldDefn(1).GetType() == ogr.wkbLineString
        assert lyr.GetFeatureCount() == 1
        assert lyr.GetFeature(1).GetGeomFieldRef(0).ExportToIsoWkt() == "POINT (3 4)"
        assert (
            lyr.GetFeature(1).GetGeomFieldRef(1).ExportToIsoWkt()
            == "LINESTRING (5 6,7 8)"
        )


@pytest.mark.require_driver("GPKG")
def test_gdalalg_vector_partition_no_fields(tmp_vsimem):

    with pytest.raises(
        Exception,
        match="When 'fields' argument is not specified, 'feature-limit' and/or 'max-file-size' must be specified",
    ):
        gdal.alg.vector.partition(
            input=src_ds(), output=tmp_vsimem / "out", output_format="GPKG"
        )

    gdal.alg.vector.partition(
        input=src_ds(), output=tmp_vsimem / "out", feature_limit=2, output_format="GPKG"
    )

    assert gdal.ReadDir(tmp_vsimem / "out" / "test") == [
        "part_0000000001.gpkg",
        "part_0000000002.gpkg",
    ]

    gdal.RmdirRecursive(tmp_vsimem / "out")

    gdal.alg.vector.partition(
        input=src_ds(),
        output=tmp_vsimem / "out",
        feature_limit=2,
        output_format="GPKG",
        scheme="flat",
    )

    assert gdal.ReadDir(tmp_vsimem / "out") == [
        "non%20spatial_0000000001.gpkg",
        "test_0000000001.gpkg",
        "test_0000000002.gpkg",
    ]
