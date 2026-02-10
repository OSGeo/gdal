#!/usr/bin/env pytest
# -*- coding: utf-8 -*-
###############################################################################
# Project:  GDAL/OGR Test Suite
# Purpose:  'gdal raster pixel-info' testing
# Author:   Even Rouault <even dot rouault @ spatialys.com>
#
###############################################################################
# Copyright (c) 2024, Even Rouault <even dot rouault at spatialys.com>
#
# SPDX-License-Identifier: MIT
###############################################################################

import json
import sys

import gdaltest
import pytest
import test_cli_utilities

from osgeo import gdal, ogr, osr

pytestmark = pytest.mark.skipif(
    test_cli_utilities.get_gdal_path() is None, reason="gdal binary not available"
)


def get_alg():
    return gdal.GetGlobalAlgorithmRegistry()["raster"]["pixel-info"]


def test_gdalalg_raster_pixel_info_missing_position():

    alg = get_alg()
    alg["dataset"] = "../gcore/data/byte.tif"
    with pytest.raises(
        Exception, match="Argument 'position' or 'position-dataset' must be specified"
    ):
        alg.Run()


def test_gdalalg_raster_pixel_info_invalid_position():

    alg = get_alg()
    with pytest.raises(Exception, match="even number of values must be specified"):
        alg["position"] = 5


def test_gdalalg_raster_pixel_info_byte_json():

    alg = get_alg()
    alg["dataset"] = "../gcore/data/byte.tif"
    alg["position"] = [5, 10]
    assert alg.Run()
    j = json.loads(alg["output-string"])
    assert j == {
        "type": "FeatureCollection",
        "crs": {"type": "name", "properties": {"name": "urn:ogc:def:crs:EPSG::26711"}},
        "features": [
            {
                "type": "Feature",
                "properties": {
                    "input_coordinate": [5.0, 10.0],
                    "column": 5.0,
                    "line": 10.0,
                    "bands": [
                        {"band_number": 1, "raw_value": 132, "unscaled_value": 132}
                    ],
                },
                "geometry": {"type": "Point", "coordinates": [441020.0, 3750720.0]},
            }
        ],
    }


def test_gdalalg_raster_pixel_info_float64_json():

    alg = get_alg()
    alg["dataset"] = "../gcore/data/float64.tif"
    alg["position"] = [5, 10]
    assert alg.Run()
    j = json.loads(alg["output-string"])
    assert j == {
        "type": "FeatureCollection",
        "crs": {"type": "name", "properties": {"name": "urn:ogc:def:crs:EPSG::26711"}},
        "features": [
            {
                "type": "Feature",
                "properties": {
                    "input_coordinate": [5.0, 10.0],
                    "column": 5.0,
                    "line": 10.0,
                    "bands": [
                        {"band_number": 1, "raw_value": 132, "unscaled_value": 132}
                    ],
                },
                "geometry": {"type": "Point", "coordinates": [441020.0, 3750720.0]},
            }
        ],
    }


def test_gdalalg_raster_pixel_info_complex_json():

    alg = get_alg()
    alg["dataset"] = "../gcore/data/cfloat32.tif"
    alg["position"] = [5, 10]
    assert alg.Run()
    j = json.loads(alg["output-string"])
    assert j == {
        "type": "FeatureCollection",
        "crs": {"type": "name", "properties": {"name": "urn:ogc:def:crs:EPSG::26711"}},
        "features": [
            {
                "type": "Feature",
                "properties": {
                    "input_coordinate": [5.0, 10.0],
                    "column": 5.0,
                    "line": 10.0,
                    "bands": [
                        {
                            "band_number": 1,
                            "value": {"real": 132.0, "imaginary": 0.0},
                        }
                    ],
                },
                "geometry": {"type": "Point", "coordinates": [441020.0, 3750720.0]},
            }
        ],
    }


@pytest.mark.require_driver("CSV")
def test_gdalalg_raster_pixel_info_byte_csv():

    alg = get_alg()
    alg["dataset"] = "../gcore/data/byte.tif"
    alg["position"] = [5, 10]
    alg["format"] = "csv"
    assert alg.Run()
    assert (
        alg["output-string"].replace("\r\n", "\n")
        == "geom_x,geom_y,extra_content,column,line,band_1_raw_value,band_1_unscaled_value\n5,10,,5,10,132,132\n"
    )


@pytest.mark.require_driver("CSV")
def test_gdalalg_raster_pixel_info_out_of_raster_csv():

    alg = get_alg()
    alg["dataset"] = "../gcore/data/byte.tif"
    alg["position"] = [-5, 10]
    alg["format"] = "csv"
    assert alg.Run()
    assert (
        alg["output-string"].replace("\r\n", "\n")
        == "geom_x,geom_y,extra_content,column,line,band_1_raw_value,band_1_unscaled_value\n-5,10,,-5,10,,\n"
    )


@pytest.mark.require_driver("CSV")
def test_gdalalg_raster_pixel_info_complex_csv():

    alg = get_alg()
    alg["dataset"] = "../gcore/data/cfloat32.tif"
    alg["position"] = [5, 10]
    alg["format"] = "csv"
    assert alg.Run()
    assert (
        alg["output-string"].replace("\r\n", "\n")
        == "geom_x,geom_y,extra_content,column,line,band_1_real_value,band_1_imaginary_value\n5,10,,5,10,132,0\n"
    )


@pytest.mark.require_driver("CSV")
def test_gdalalg_raster_pixel_info_complex_out_of_raster_csv():

    alg = get_alg()
    alg["dataset"] = "../gcore/data/cfloat32.tif"
    alg["position"] = [-5, 10]
    alg["format"] = "csv"
    assert alg.Run()
    assert (
        alg["output-string"].replace("\r\n", "\n")
        == "geom_x,geom_y,extra_content,column,line,band_1_real_value,band_1_imaginary_value\n-5,10,,-5,10,,\n"
    )


def test_gdalalg_raster_pixel_info_invalid_overview():

    alg = get_alg()
    alg["dataset"] = "../gcore/data/byte.tif"
    alg["overview"] = 0
    with pytest.raises(
        Exception,
        match="Source dataset has no overviews. Argument 'overview' must not be specified",
    ):
        alg.Run()


def test_gdalalg_raster_pixel_info_invalid_overview_bis():

    src_ds = gdal.Translate("", "../gcore/data/byte.tif", format="MEM")
    src_ds.BuildOverviews("NEAR", [2])

    alg = get_alg()
    alg["dataset"] = src_ds
    alg["overview"] = 1
    with pytest.raises(
        Exception,
        match="Source dataset has only 1 overview level. 'overview' value must be strictly lower than this number",
    ):
        alg.Run()


def test_gdalalg_raster_pixel_info_overview():

    src_ds = gdal.Translate("", "../gcore/data/byte.tif", format="MEM")
    src_ds.BuildOverviews("NEAR", [2])

    alg = get_alg()
    alg["dataset"] = src_ds
    alg["overview"] = 0
    alg["position"] = [5, 10]
    assert alg.Run()
    j = json.loads(alg["output-string"])
    assert j == {
        "type": "FeatureCollection",
        "crs": {"type": "name", "properties": {"name": "urn:ogc:def:crs:EPSG::26711"}},
        "features": [
            {
                "type": "Feature",
                "properties": {
                    "input_coordinate": [5.0, 10.0],
                    "column": 5.0,
                    "line": 10.0,
                    "bands": [
                        {"band_number": 1, "raw_value": 99, "unscaled_value": 99}
                    ],
                },
                "geometry": {"type": "Point", "coordinates": [441020.0, 3750720.0]},
            }
        ],
    }


def test_gdalalg_raster_pixel_info_unscaled():

    src_ds = gdal.GetDriverByName("MEM").Create("", 1, 1)
    src_ds.GetRasterBand(1).Fill(1)
    src_ds.GetRasterBand(1).SetOffset(2)
    src_ds.GetRasterBand(1).SetScale(3)

    alg = get_alg()
    alg["dataset"] = src_ds
    alg["position"] = [0, 0]
    assert alg.Run()
    j = json.loads(alg["output-string"])
    assert j == {
        "type": "FeatureCollection",
        "features": [
            {
                "type": "Feature",
                "properties": {
                    "input_coordinate": [0.0, 0.0],
                    "column": 0.0,
                    "line": 0.0,
                    "bands": [
                        {"band_number": 1, "raw_value": 1, "unscaled_value": 5.0}
                    ],
                },
                "geometry": None,
            }
        ],
    }


@pytest.mark.require_driver("CSV")
def test_gdalalg_raster_pixel_info_unscaled_csv():

    src_ds = gdal.GetDriverByName("MEM").Create("", 1, 1, 1, gdal.GDT_Float32)
    src_ds.GetRasterBand(1).Fill(1.5)
    src_ds.GetRasterBand(1).SetOffset(2)
    src_ds.GetRasterBand(1).SetScale(3)

    alg = get_alg()
    alg["dataset"] = src_ds
    alg["position"] = [0, 0]
    alg["format"] = "csv"
    assert alg.Run()
    assert (
        alg["output-string"].replace("\r\n", "\n")
        == "geom_x,geom_y,extra_content,column,line,band_1_raw_value,band_1_unscaled_value\n0,0,,0,0,1.5,6.5\n"
    )


@pytest.mark.require_driver("CSV")
def test_gdalalg_raster_pixel_info_missing_crs():

    src_ds = gdal.GetDriverByName("MEM").Create("", 1, 1)

    alg = get_alg()
    alg["dataset"] = src_ds
    alg["position"] = [0, 0]
    alg["position-crs"] = "dataset"
    alg["format"] = "csv"
    with pytest.raises(
        Exception,
        match="Dataset has no CRS. Only 'position-crs' = 'pixel' is supported",
    ):
        alg.Run()


@pytest.mark.require_driver("CSV")
def test_gdalalg_raster_pixel_info_missing_gt():

    src_ds = gdal.GetDriverByName("MEM").Create("", 1, 1)
    srs = osr.SpatialReference("WGS84")
    src_ds.SetSpatialRef(srs)

    alg = get_alg()
    alg["dataset"] = src_ds
    alg["position"] = [0, 0]
    alg["position-crs"] = "WGS84"
    alg["format"] = "csv"
    with pytest.raises(Exception, match="Cannot get geotransform"):
        alg.Run()


@pytest.mark.require_driver("CSV")
def test_gdalalg_raster_pixel_info_wrong_gt():

    src_ds = gdal.GetDriverByName("MEM").Create("", 1, 1)
    srs = osr.SpatialReference("WGS84")
    src_ds.SetSpatialRef(srs)
    src_ds.SetGeoTransform([0, 0, 0, 0, 0, 0])

    alg = get_alg()
    alg["dataset"] = src_ds
    alg["position"] = [0, 0]
    alg["position-crs"] = "WGS84"
    alg["format"] = "csv"
    with pytest.raises(Exception, match="Cannot invert geotransform"):
        alg.Run()


def test_gdalalg_raster_pixel_info_position_crs_dataset():

    src_ds = gdal.GetDriverByName("MEM").Create("", 1, 1)
    srs = osr.SpatialReference("WGS84")
    srs.SetAxisMappingStrategy(osr.OAMS_TRADITIONAL_GIS_ORDER)
    src_ds.SetSpatialRef(srs)
    src_ds.SetGeoTransform([10, 1, 0, 20, 0, 1])

    alg = get_alg()
    alg["dataset"] = src_ds
    alg["position"] = [10.5, 20.5]
    alg["position-crs"] = "dataset"
    assert alg.Run()
    j = json.loads(alg["output-string"])
    assert j == {
        "type": "FeatureCollection",
        "crs": {
            "type": "name",
            "properties": {"name": "urn:ogc:def:crs:OGC:1.3:CRS84"},
        },
        "features": [
            {
                "type": "Feature",
                "properties": {
                    "input_coordinate": [10.5, 20.5],
                    "column": 0.5,
                    "line": 0.5,
                    "bands": [{"band_number": 1, "raw_value": 0, "unscaled_value": 0}],
                },
                "geometry": {"type": "Point", "coordinates": [10.5, 20.5]},
            }
        ],
    }


def test_gdalalg_raster_pixel_info_position_crs():

    src_ds = gdal.GetDriverByName("MEM").Create("", 1, 1)
    srs = osr.SpatialReference("WGS84")
    srs.SetAxisMappingStrategy(osr.OAMS_TRADITIONAL_GIS_ORDER)
    src_ds.SetSpatialRef(srs)
    src_ds.SetGeoTransform([10, 1, 0, 20, 0, 1])

    alg = get_alg()
    alg["dataset"] = src_ds
    alg["position"] = [10.5, 20.5]
    alg["position-crs"] = srs
    assert alg.Run()
    j = json.loads(alg["output-string"])
    assert j == {
        "type": "FeatureCollection",
        "crs": {
            "type": "name",
            "properties": {"name": "urn:ogc:def:crs:OGC:1.3:CRS84"},
        },
        "features": [
            {
                "type": "Feature",
                "properties": {
                    "input_coordinate": [10.5, 20.5],
                    "column": 0.5,
                    "line": 0.5,
                    "bands": [{"band_number": 1, "raw_value": 0, "unscaled_value": 0}],
                },
                "geometry": {"type": "Point", "coordinates": [10.5, 20.5]},
            }
        ],
    }


def test_gdalalg_raster_pixel_info_non_epsg_crs():

    src_ds = gdal.GetDriverByName("MEM").Create("", 1, 1)
    srs = osr.SpatialReference("+proj=utm +zone=17")
    srs.SetAxisMappingStrategy(osr.OAMS_TRADITIONAL_GIS_ORDER)
    src_ds.SetSpatialRef(srs)
    src_ds.SetGeoTransform([500000, 1, 0, 0, 0, 1])

    alg = get_alg()
    alg["dataset"] = src_ds
    alg["position"] = [0, 0]
    assert alg.Run()
    j = json.loads(alg["output-string"])
    assert j == {
        "type": "FeatureCollection",
        "crs": {
            "type": "name",
            "properties": {"name": "urn:ogc:def:crs:OGC:1.3:CRS84"},
        },
        "features": [
            {
                "type": "Feature",
                "properties": {
                    "input_coordinate": [0.0, 0.0],
                    "column": 0.0,
                    "line": 0.0,
                    "bands": [{"band_number": 1, "raw_value": 0, "unscaled_value": 0}],
                },
                "geometry": {"type": "Point", "coordinates": [-81.0, 0.0]},
            }
        ],
    }


def test_gdalalg_raster_pixel_info_files():

    alg = get_alg()
    alg["dataset"] = "../gcore/data/byte.vrt"
    alg["position"] = [5, 10]
    assert alg.Run()
    j = json.loads(alg["output-string"].replace("\\\\", "/").replace("\\/", "/"))
    assert j == {
        "type": "FeatureCollection",
        "crs": {"type": "name", "properties": {"name": "urn:ogc:def:crs:EPSG::26711"}},
        "features": [
            {
                "type": "Feature",
                "properties": {
                    "input_coordinate": [5.0, 10.0],
                    "column": 5.0,
                    "line": 10.0,
                    "bands": [
                        {
                            "band_number": 1,
                            "raw_value": 132,
                            "unscaled_value": 132,
                            "files": ["../gcore/data/byte.tif"],
                        }
                    ],
                },
                "geometry": {"type": "Point", "coordinates": [441020.0, 3750720.0]},
            }
        ],
    }


def test_gdalalg_raster_pixel_info_promote_pixel_value_to_z():

    alg = get_alg()
    alg["dataset"] = "../gcore/data/byte.vrt"
    alg["position"] = [5, 10]
    alg["promote-pixel-value-to-z"] = True
    assert alg.Run()
    j = json.loads(alg["output-string"])
    assert j["features"][0]["geometry"] == {
        "type": "Point",
        "coordinates": [441020.0, 3750720.0, 132.0],
    }


@pytest.fixture()
def gdal_path():
    return test_cli_utilities.get_gdal_path()


def test_gdalalg_raster_pixel_info_from_command_line(gdal_path):

    ret = gdaltest.runexternal(
        f"{gdal_path} raster pixel-info ../gcore/data/byte.tif",
        strin="5.5 10.5 foo bar",
    )
    assert json.loads(ret) == {
        "type": "FeatureCollection",
        "crs": {"type": "name", "properties": {"name": "urn:ogc:def:crs:EPSG::26711"}},
        "features": [
            {
                "type": "Feature",
                "properties": {
                    "input_coordinate": [5.5, 10.5],
                    "extra_content": "foo bar",
                    "column": 5.5,
                    "line": 10.5,
                    "bands": [
                        {"band_number": 1, "raw_value": 132, "unscaled_value": 132}
                    ],
                },
                "geometry": {"type": "Point", "coordinates": [441050.0, 3750690.0]},
            }
        ],
    }


@pytest.mark.skipif(
    # Some weird issue about end of line characters I don't understand
    sys.platform == "win32",
    reason="Windows not supported for this test",
)
@pytest.mark.require_driver("CSV")
def test_gdalalg_raster_pixel_info_from_command_line_csv(gdal_path):

    ret = "\n".join(
        gdaltest.runexternal(
            f"{gdal_path} raster pixel-info --of=csv ../gcore/data/byte.tif",
            strin="5.5 10.5 foo bar",
        ).splitlines()
    )
    assert (
        ret
        == "geom_x,geom_y,extra_content,column,line,band_1_raw_value,band_1_unscaled_value\n5.5,10.5,foo bar,5.5,10.5,132,132"
    )


@pytest.mark.require_driver("CSV")
def test_gdalalg_raster_pixel_info_from_command_line_to_named_output_csv(
    gdal_path, tmp_path
):

    gdaltest.runexternal(
        f"{gdal_path} raster pixel-info --of=csv --output={tmp_path}/out.csv ../gcore/data/byte.tif",
        strin="5.5 10.5 foo bar",
    )

    with gdal.VSIFile(tmp_path / "out.csv", "rb") as f:
        data = f.read().decode("ascii").replace("\r\n", "\n")
    assert (
        data
        == "geom_x,geom_y,extra_content,column,line,band_1_raw_value,band_1_unscaled_value\n5.5,10.5,foo bar,5.5,10.5,132,132\n"
    )


@pytest.mark.require_driver("CSV")
def test_gdalalg_raster_pixel_info_named_output_csv(tmp_vsimem):

    alg = gdal.alg.raster.pixel_info(
        input="../gcore/data/byte.tif",
        position=[5.5, 10.5],
        output=tmp_vsimem / "out.csv",
    )
    assert alg.Output().GetDescription() == str(tmp_vsimem / "out.csv")

    with gdal.VSIFile(tmp_vsimem / "out.csv", "rb") as f:
        data = f.read().decode("ascii").replace("\r\n", "\n")
    assert (
        data
        == "geom_x,geom_y,extra_content,column,line,band_1_raw_value,band_1_unscaled_value\n5.5,10.5,,5.5,10.5,132,132\n"
    )


def test_gdalalg_raster_pixel_info_named_output_geojson(tmp_vsimem):

    alg = gdal.alg.raster.pixel_info(
        input="../gcore/data/byte.tif",
        position=[5.5, 10.5],
        output=tmp_vsimem / "out.json",
    )
    assert alg.Output().GetDescription() == str(tmp_vsimem / "out.json")

    with gdal.VSIFile(tmp_vsimem / "out.json", "rb") as f:
        data = f.read()
    assert json.loads(data) == {
        "crs": {"properties": {"name": "urn:ogc:def:crs:EPSG::26711"}, "type": "name"},
        "features": [
            {
                "geometry": {"coordinates": [441050.0, 3750690.0], "type": "Point"},
                "properties": {
                    "bands": [
                        {"band_number": 1, "raw_value": 132, "unscaled_value": 132.0}
                    ],
                    "column": 5.5,
                    "input_coordinate": [5.5, 10.5],
                    "line": 10.5,
                },
                "type": "Feature",
            }
        ],
        "type": "FeatureCollection",
    }


@pytest.mark.require_driver("GPKG")
@pytest.mark.parametrize("include_field", [None, "ALL", "NONE", "foo", "non_existing"])
def test_gdalalg_raster_pixel_info_from_to_vector_dataset(tmp_vsimem, include_field):

    position_dataset = gdal.GetDriverByName("MEM").CreateVector("")
    srs = osr.SpatialReference(epsg=4267)
    srs.SetAxisMappingStrategy(osr.OAMS_TRADITIONAL_GIS_ORDER)
    coordinate_lyr = position_dataset.CreateLayer("coords", srs=srs)
    coordinate_lyr.CreateField(ogr.FieldDefn("foo"))
    f = ogr.Feature(coordinate_lyr.GetLayerDefn())
    f.SetGeometry(
        ogr.CreateGeometryFromWkt("POINT (-117.634316882504 33.8972472661961)")
    )
    f["foo"] = "bar"
    coordinate_lyr.CreateFeature(f)

    kwargs = {}
    if include_field:
        kwargs["include_field"] = include_field

    if include_field == "non_existing":
        with pytest.raises(
            Exception, match="Field 'non_existing' does not exist in layer 'coords'"
        ):
            gdal.alg.raster.pixel_info(
                input="../gcore/data/byte.tif",
                position_dataset=position_dataset,
                output=tmp_vsimem / "out.gpkg",
                **kwargs,
            )
        return
    else:
        assert gdal.alg.raster.pixel_info(
            input="../gcore/data/byte.tif",
            position_dataset=position_dataset,
            output=tmp_vsimem / "out.gpkg",
            **kwargs,
        )

    with ogr.Open(tmp_vsimem / "out.gpkg") as out_ds:
        out_lyr = out_ds.GetLayer(0)
        assert out_lyr.GetGeomType() == ogr.wkbPoint
        assert out_lyr.GetSpatialRef().GetAuthorityCode(None) == "4267"
        f = out_lyr.GetNextFeature()
        assert f["line"] == pytest.approx(9.5)
        assert f["column"] == pytest.approx(10.5)
        assert f["band_1_raw_value"] == 115
        assert f["band_1_unscaled_value"] == 115
        if include_field is None or include_field in ("ALL", "foo"):
            assert f["foo"] == "bar"
        else:
            assert out_lyr.GetLayerDefn().GetFieldIndex("foo") < 0
        assert f.GetGeometryRef().GetX(0) == pytest.approx(-117.634316882504)
        assert f.GetGeometryRef().GetY(0) == pytest.approx(33.8972472661961)

        assert out_lyr.GetNextFeature() is None


def test_gdalalg_raster_pixel_info_from_vector_dataset_to_geojson(tmp_vsimem):

    position_dataset = gdal.GetDriverByName("MEM").CreateVector("")
    coordinate_lyr = position_dataset.CreateLayer("coords")
    coordinate_lyr.CreateField(ogr.FieldDefn("str", ogr.OFTString))
    coordinate_lyr.CreateField(ogr.FieldDefn("int", ogr.OFTInteger))
    coordinate_lyr.CreateField(ogr.FieldDefn("int64", ogr.OFTInteger64))
    coordinate_lyr.CreateField(ogr.FieldDefn("double", ogr.OFTReal))
    coordinate_lyr.CreateField(ogr.FieldDefn("strarray", ogr.OFTStringList))
    fld_defn = ogr.FieldDefn("bool", ogr.OFTInteger)
    fld_defn.SetSubType(ogr.OFSTBoolean)
    coordinate_lyr.CreateField(fld_defn)
    fld_defn = ogr.FieldDefn("json", ogr.OFTString)
    fld_defn.SetSubType(ogr.OFSTJSON)
    coordinate_lyr.CreateField(fld_defn)
    f = ogr.Feature(coordinate_lyr.GetLayerDefn())
    f.SetGeometry(ogr.CreateGeometryFromWkt("POINT (10.5 9.5)"))
    f["str"] = "bar"
    f["int"] = 1
    f["int64"] = 1234567890123
    f["double"] = 1.5
    f["bool"] = True
    f["strarray"] = ["foo", "bar"]
    f["json"] = '{"foo":"bar"}'
    coordinate_lyr.CreateFeature(f)

    alg = gdal.alg.raster.pixel_info(
        input="../gcore/data/byte.tif",
        position_dataset=position_dataset,
        position_crs="pixel",
    )
    assert json.loads(alg["output-string"]) == {
        "type": "FeatureCollection",
        "crs": {"type": "name", "properties": {"name": "urn:ogc:def:crs:EPSG::26711"}},
        "features": [
            {
                "type": "Feature",
                "properties": {
                    "input_coordinate": [10.5, 9.5],
                    "column": 10.5,
                    "line": 9.5,
                    "str": "bar",
                    "int": 1,
                    "int64": 1234567890123,
                    "double": 1.5,
                    "strarray": ["foo", "bar"],
                    "bool": True,
                    "json": {"foo": "bar"},
                    "bands": [
                        {"band_number": 1, "raw_value": 115, "unscaled_value": 115.0}
                    ],
                },
                "geometry": {"type": "Point", "coordinates": [441350.0, 3750750.0]},
            }
        ],
    }


@pytest.mark.require_driver("GPKG")
def test_gdalalg_raster_pixel_info_from_to_vector_dataset_with_pos_crs_user_crs(
    tmp_vsimem,
):

    position_dataset = gdal.GetDriverByName("MEM").CreateVector("")
    coordinate_lyr = position_dataset.CreateLayer("coords")
    f = ogr.Feature(coordinate_lyr.GetLayerDefn())
    f.SetGeometry(
        ogr.CreateGeometryFromWkt("POINT (-117.634316882504 33.8972472661961)")
    )
    coordinate_lyr.CreateFeature(f)

    assert gdal.alg.raster.pixel_info(
        input="../gcore/data/byte.tif",
        position_dataset=position_dataset,
        output=tmp_vsimem / "out.gpkg",
        position_crs="EPSG:4267",
    )

    with ogr.Open(tmp_vsimem / "out.gpkg") as out_ds:
        out_lyr = out_ds.GetLayer(0)
        assert out_lyr.GetGeomType() == ogr.wkbPoint
        assert out_lyr.GetSpatialRef().GetAuthorityCode(None) == "4267"
        f = out_lyr.GetNextFeature()
        assert f["line"] == pytest.approx(9.5)
        assert f["column"] == pytest.approx(10.5)
        assert f["band_1_raw_value"] == 115
        assert f["band_1_unscaled_value"] == 115
        assert f.GetGeometryRef().GetX(0) == pytest.approx(-117.634316882504)
        assert f.GetGeometryRef().GetY(0) == pytest.approx(33.8972472661961)

        assert out_lyr.GetNextFeature() is None


@pytest.mark.require_driver("GPKG")
def test_gdalalg_raster_pixel_info_from_to_vector_dataset_with_pos_crs_pixel(
    tmp_vsimem,
):

    position_dataset = gdal.GetDriverByName("MEM").CreateVector("")
    coordinate_lyr = position_dataset.CreateLayer("coords")
    f = ogr.Feature(coordinate_lyr.GetLayerDefn())
    f.SetGeometry(ogr.CreateGeometryFromWkt("POINT (10.5 9.5)"))
    coordinate_lyr.CreateFeature(f)

    assert gdal.alg.raster.pixel_info(
        input="../gcore/data/byte.tif",
        position_dataset=position_dataset,
        output=tmp_vsimem / "out.gpkg",
        position_crs="pixel",
    )

    with ogr.Open(tmp_vsimem / "out.gpkg") as out_ds:
        out_lyr = out_ds.GetLayer(0)
        assert out_lyr.GetGeomType() == ogr.wkbPoint
        assert out_lyr.GetSpatialRef().GetAuthorityCode(None) == "26711"
        f = out_lyr.GetNextFeature()
        assert f["line"] == pytest.approx(9.5)
        assert f["column"] == pytest.approx(10.5)
        assert f["band_1_raw_value"] == 115
        assert f["band_1_unscaled_value"] == 115
        assert f.GetGeometryRef().GetX(0) == pytest.approx(441350.0)
        assert f.GetGeometryRef().GetY(0) == pytest.approx(3750750.0)

        assert out_lyr.GetNextFeature() is None


@pytest.mark.require_driver("GPKG")
def test_gdalalg_raster_pixel_info_from_to_vector_dataset_complex(tmp_vsimem):

    position_dataset = gdal.GetDriverByName("MEM").CreateVector("")
    coordinate_lyr = position_dataset.CreateLayer("coords")
    f = ogr.Feature(coordinate_lyr.GetLayerDefn())
    f.SetGeometry(ogr.CreateGeometryFromWkt("POINT (5 10)"))
    coordinate_lyr.CreateFeature(f)

    assert gdal.alg.raster.pixel_info(
        input="../gcore/data/cfloat32.tif",
        position_dataset=position_dataset,
        output=tmp_vsimem / "out.gpkg",
    )

    with ogr.Open(tmp_vsimem / "out.gpkg") as out_ds:
        out_lyr = out_ds.GetLayer(0)
        assert out_lyr.GetGeomType() == ogr.wkbPoint
        assert out_lyr.GetSpatialRef().GetAuthorityCode(None) == "26711"
        f = out_lyr.GetNextFeature()
        assert f["line"] == pytest.approx(10)
        assert f["column"] == pytest.approx(5)
        assert f["band_1_real_value"] == 132
        assert f["band_1_imaginary_value"] == 0
        assert f.GetGeometryRef().GetX(0) == pytest.approx(441020.0)
        assert f.GetGeometryRef().GetY(0) == pytest.approx(3750720.0)

        assert out_lyr.GetNextFeature() is None


def test_gdalalg_raster_pixel_info_coordinate_from_vector_dataset_errors():

    with pytest.raises(Exception, match="/i/do_not_exist"):
        gdal.alg.raster.pixel_info(
            input="../gcore/data/byte.tif", position_dataset="/i/do_not_exist"
        )

    with pytest.raises(Exception, match="has no vector layer"):
        gdal.alg.raster.pixel_info(
            input="../gcore/data/byte.tif",
            position_dataset=gdal.GetDriverByName("MEM").CreateVector(""),
        )

    two_layers_ds = gdal.GetDriverByName("MEM").CreateVector("")
    two_layers_ds.CreateLayer("a", geom_type=ogr.wkbNone)
    two_layers_ds.CreateLayer("b")

    with pytest.raises(Exception, match="has more than one vector layer"):
        gdal.alg.raster.pixel_info(
            input="../gcore/data/byte.tif", position_dataset=two_layers_ds
        )

    with pytest.raises(Exception, match="Cannot find layer 'i_do_not_exist'"):
        gdal.alg.raster.pixel_info(
            input="../gcore/data/byte.tif",
            position_dataset=two_layers_ds,
            input_layer="i_do_not_exist",
        )

    with pytest.raises(Exception, match="has no geometry column"):
        gdal.alg.raster.pixel_info(
            input="../gcore/data/byte.tif",
            position_dataset=two_layers_ds,
            input_layer="a",
        )


@pytest.mark.require_driver("GPKG")
def test_gdalalg_raster_pixel_info_coordinate_to_vector_dataset_errors():

    with pytest.raises(Exception, match="Cannot guess driver for /i/do_not_exist"):
        gdal.alg.raster.pixel_info(
            input="../gcore/data/byte.tif", position=[0, 0], output="/i/do_not_exist"
        )

    with pytest.raises(
        Exception, match=r"sqlite3_open\(/i/do_not_exist/out.gpkg\) failed"
    ):
        gdal.alg.raster.pixel_info(
            input="../gcore/data/byte.tif",
            position=[0, 0],
            output="/i/do_not_exist/out.gpkg",
        )

    with pytest.raises(Exception, match="Cannot create layer 'out'"):
        gdal.alg.raster.pixel_info(
            input="../gcore/data/byte.tif",
            position=[0, 0],
            output="/i/do_not_exist/out.shp",
        )


def test_gdalalg_raster_pixel_info_in_pipeline(tmp_vsimem):

    with gdal.GetDriverByName("ESRI Shapefile").CreateVector(
        tmp_vsimem / "coords.shp"
    ) as position_dataset:
        coordinate_lyr = position_dataset.CreateLayer("coords")
        f = ogr.Feature(coordinate_lyr.GetLayerDefn())
        f.SetGeometry(ogr.CreateGeometryFromWkt("POINT (10.5 9.5)"))
        coordinate_lyr.CreateFeature(f)

    with gdal.alg.pipeline(
        pipeline=f"read ../gcore/data/byte.tif ! pixel-info {tmp_vsimem}/coords.shp"
    ) as alg:
        out_ds = alg.Output()
        out_lyr = out_ds.GetLayer(0)
        assert out_lyr.GetGeomType() == ogr.wkbPoint
        assert out_lyr.GetSpatialRef().GetAuthorityCode(None) == "26711"
        f = out_lyr.GetNextFeature()
        assert f["line"] == pytest.approx(9.5)
        assert f["column"] == pytest.approx(10.5)
        assert f["band_1_raw_value"] == 115
        assert f["band_1_unscaled_value"] == 115
        assert f.GetGeometryRef().GetX(0) == pytest.approx(441350.0)
        assert f.GetGeometryRef().GetY(0) == pytest.approx(3750750.0)

        assert out_lyr.GetNextFeature() is None

    with gdal.alg.pipeline(
        pipeline=f"read ../gcore/data/byte.tif ! pixel-info --position-dataset {tmp_vsimem}/coords.shp"
    ) as alg:
        out_ds = alg.Output()
        out_lyr = out_ds.GetLayer(0)
        assert out_lyr.GetFeatureCount() == 1

    with gdal.alg.pipeline(
        pipeline=f"read ../gcore/data/byte.tif ! pixel-info --input _PIPE_ --position-dataset {tmp_vsimem}/coords.shp"
    ) as alg:
        out_ds = alg.Output()
        out_lyr = out_ds.GetLayer(0)
        assert out_lyr.GetFeatureCount() == 1

    with gdal.alg.pipeline(
        pipeline=f"read {tmp_vsimem}/coords.shp ! pixel-info --input ../gcore/data/byte.tif --position-dataset _PIPE_"
    ) as alg:
        out_ds = alg.Output()
        out_lyr = out_ds.GetLayer(0)
        assert out_lyr.GetFeatureCount() == 1

    with pytest.raises(
        Exception,
        match="Positional arguments starting at 'POSITION-DATASET' have not been specified",
    ):
        gdal.alg.pipeline(pipeline="read ../gcore/data/byte.tif ! pixel-info")

    with pytest.raises(
        Exception,
        match="Step 'pixel-info' expects a raster input dataset, but previous step 'read' generates a vector output dataset",
    ):
        gdal.alg.pipeline(
            pipeline=f"read {tmp_vsimem}/coords.shp ! pixel-info --position-dataset foo"
        )

    with pytest.raises(Exception, match="has no raster band"):
        gdal.alg.pipeline(
            pipeline=f"read {tmp_vsimem}/coords.shp ! pixel-info --input _PIPE_ --position-dataset _PIPE_"
        )
