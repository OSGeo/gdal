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

import gdaltest
import pytest
import test_cli_utilities

from osgeo import gdal, osr

pytestmark = pytest.mark.skipif(
    test_cli_utilities.get_gdal_path() is None, reason="gdal binary not available"
)


def get_alg():
    return gdal.GetGlobalAlgorithmRegistry()["raster"]["pixel-info"]


def test_gdalalg_raster_pixel_info_missing_position():

    alg = get_alg()
    alg["dataset"] = "../gcore/data/byte.tif"
    with pytest.raises(Exception, match="Argument 'position' must be specified"):
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


def test_gdalalg_raster_pixel_info_byte_csv():

    alg = get_alg()
    alg["dataset"] = "../gcore/data/byte.tif"
    alg["position"] = [5, 10]
    alg["format"] = "csv"
    assert alg.Run()
    assert (
        alg["output-string"]
        == 'input_x,input_y,extra_input,column,line,band_1_raw_value,band_1_unscaled_value\n5,10,"",5,10,132,132\n'
    )


def test_gdalalg_raster_pixel_info_out_of_raster_csv():

    alg = get_alg()
    alg["dataset"] = "../gcore/data/byte.tif"
    alg["position"] = [-5, 10]
    alg["format"] = "csv"
    assert alg.Run()
    assert (
        alg["output-string"]
        == 'input_x,input_y,extra_input,column,line,band_1_raw_value,band_1_unscaled_value\n-5,10,"",-5,10,,\n'
    )


def test_gdalalg_raster_pixel_info_complex_csv():

    alg = get_alg()
    alg["dataset"] = "../gcore/data/cfloat32.tif"
    alg["position"] = [5, 10]
    alg["format"] = "csv"
    assert alg.Run()
    assert (
        alg["output-string"]
        == 'input_x,input_y,extra_input,column,line,band_1_real_value,band_1_imaginary_value\n5,10,"",5,10,132,0\n'
    )


def test_gdalalg_raster_pixel_info_complex_out_of_raster_csv():

    alg = get_alg()
    alg["dataset"] = "../gcore/data/cfloat32.tif"
    alg["position"] = [-5, 10]
    alg["format"] = "csv"
    assert alg.Run()
    assert (
        alg["output-string"]
        == 'input_x,input_y,extra_input,column,line,band_1_real_value,band_1_imaginary_value\n-5,10,"",-5,10,,\n'
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
        alg["output-string"]
        == 'input_x,input_y,extra_input,column,line,band_1_raw_value,band_1_unscaled_value\n0,0,"",0,0,1.5,6.5\n'
    )


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


def test_gdalalg_raster_pixel_info_from_command_line_csv(gdal_path):

    ret = gdaltest.runexternal(
        f"{gdal_path} raster pixel-info --of=csv ../gcore/data/byte.tif",
        strin="5.5 10.5 foo bar",
    ).replace("\r\n", "\n")
    assert (
        ret
        == 'input_x,input_y,extra_input,column,line,band_1_raw_value,band_1_unscaled_value\n5.5,10.5,"foo bar",5.5,10.5,132,132\n'
    )
