#!/usr/bin/env pytest
# -*- coding: utf-8 -*-
###############################################################################
# Project:  GDAL/OGR Test Suite
# Purpose:  'gdal mdim info' testing
# Author:   Even Rouault <even dot rouault @ spatialys.com>
#
###############################################################################
# Copyright (c) 2025, Even Rouault <even dot rouault at spatialys.com>
#
# SPDX-License-Identifier: MIT
###############################################################################

import json

import gdaltest
import pytest
import test_cli_utilities

from osgeo import gdal

pytestmark = [
    pytest.mark.require_driver("netCDF"),
    pytest.mark.skipif(
        test_cli_utilities.get_gdal_path() is None, reason="gdal binary not available"
    ),
]


@pytest.fixture()
def gdal_path():
    return test_cli_utilities.get_gdal_path()


def get_mdim_info_alg():
    return gdal.GetGlobalAlgorithmRegistry()["mdim"]["info"]


def test_gdalalg_mdim_info():
    info = get_mdim_info_alg()
    assert info.ParseRunAndFinalize(["../gdrivers/data/netcdf/byte.nc"])
    output_string = info["output-string"]
    j = json.loads(output_string)
    del j["arrays"]["Band1"]["srs"]["wkt"]
    assert j == {
        "type": "group",
        "driver": "netCDF",
        "name": "/",
        "attributes": {
            "GDAL_AREA_OR_POINT": "Area",
            "Conventions": "CF-1.5",
            "GDAL": "GDAL 3.8.0dev-refs/heads-dirty, released 2023/10/09 (debug build)",
            "history": "Mon Oct 09 18:27:35 2023: GDAL CreateCopy( byte.nc, ... )",
        },
        "dimensions": [
            {
                "name": "x",
                "full_name": "/x",
                "size": 20,
                "type": "HORIZONTAL_X",
                "indexing_variable": {
                    "x": {
                        "datatype": "Float64",
                        "dimensions": ["/x"],
                        "dimension_size": [20],
                        "attributes": {
                            "standard_name": "projection_x_coordinate",
                            "long_name": "x coordinate of projection",
                        },
                        "full_name": "/x",
                        "unit": "m",
                    }
                },
            },
            {
                "name": "y",
                "full_name": "/y",
                "size": 20,
                "type": "HORIZONTAL_Y",
                "indexing_variable": {
                    "y": {
                        "datatype": "Float64",
                        "dimensions": ["/y"],
                        "dimension_size": [20],
                        "attributes": {
                            "standard_name": "projection_y_coordinate",
                            "long_name": "y coordinate of projection",
                        },
                        "full_name": "/y",
                        "unit": "m",
                    }
                },
            },
        ],
        "arrays": {
            "x": {
                "datatype": "Float64",
                "dimensions": ["/x"],
                "dimension_size": [20],
                "attributes": {
                    "standard_name": "projection_x_coordinate",
                    "long_name": "x coordinate of projection",
                },
                "full_name": "/x",
                "unit": "m",
            },
            "y": {
                "datatype": "Float64",
                "dimensions": ["/y"],
                "dimension_size": [20],
                "attributes": {
                    "standard_name": "projection_y_coordinate",
                    "long_name": "y coordinate of projection",
                },
                "full_name": "/y",
                "unit": "m",
            },
            "Band1": {
                "datatype": "Byte",
                "dimensions": ["/y", "/x"],
                "dimension_size": [20, 20],
                "attributes": {
                    "long_name": "GDAL Band Number 1",
                    "valid_range": [0, 255],
                },
                "full_name": "/Band1",
                "srs": {
                    "data_axis_to_srs_axis_mapping": [2, 1],
                },
            },
        },
        "structural_info": {"NC_FORMAT": "CLASSIC"},
    }


def test_gdalalg_mdim_info_all_options():
    info = get_mdim_info_alg()
    assert info.ParseRunAndFinalize(
        [
            "../gdrivers/data/netcdf/byte.nc",
            "--detailed",
            "--stats",
            "--limit=5",
            "--array=/Band1",
            "--array-option=USE_DEFAULT_FILL_AS_NODATA=NO",
        ]
    )
    output_string = info["output-string"]
    j = json.loads(output_string)
    del j["dimensions"]
    del j["statistics"]["mean"]
    del j["statistics"]["stddev"]
    del j["srs"]["wkt"]
    assert j == {
        "type": "array",
        "name": "Band1",
        "datatype": "Byte",
        "dimension_size": [20, 20],
        "attributes": {
            "long_name": {"datatype": "String", "value": "GDAL Band Number 1"},
            "_Unsigned": {"datatype": "String", "value": "true"},
            "valid_range": {"datatype": "UInt16", "value": [0, 255]},
            "grid_mapping": {"datatype": "String", "value": "transverse_mercator"},
        },
        "srs": {
            "data_axis_to_srs_axis_mapping": [2, 1],
        },
        "values": [
            [181, 181, 156, "[...]", 99, 107],
            [173, 247, 255, "[...]", 107, 123],
            [156, 181, 140, "[...]", 140, 99],
            "[...]",
            [115, 132, 107, "[...]", 99, 156],
            [107, 123, 132, "[...]", 156, 148],
        ],
        "statistics": {"min": 74, "max": 255, "valid_sample_count": 400},
    }


def test_gdalalg_mdim_info_binary(gdal_path):

    out = gdaltest.runexternal(f"{gdal_path} mdim info ../gdrivers/data/netcdf/byte.nc")
    assert json.loads(out) == {
        "arrays": {
            "Band1": {
                "attributes": {
                    "long_name": "GDAL Band Number 1",
                    "valid_range": [
                        0,
                        255,
                    ],
                },
                "datatype": "Byte",
                "dimension_size": [
                    20,
                    20,
                ],
                "dimensions": [
                    "/y",
                    "/x",
                ],
                "full_name": "/Band1",
                "srs": {
                    "data_axis_to_srs_axis_mapping": [
                        2,
                        1,
                    ],
                    "wkt": 'PROJCRS["NAD27 / UTM zone '
                    '11N",BASEGEOGCRS["NAD27",DATUM["North American Datum '
                    '1927",ELLIPSOID["Clarke '
                    '1866",6378206.4,294.978698213898,LENGTHUNIT["metre",1]]],PRIMEM["Greenwich",0,ANGLEUNIT["degree",0.0174532925199433]],ID["EPSG",4267]],CONVERSION["UTM '
                    'zone 11N",METHOD["Transverse '
                    'Mercator",ID["EPSG",9807]],PARAMETER["Latitude of natural '
                    'origin",0,ANGLEUNIT["degree",0.0174532925199433],ID["EPSG",8801]],PARAMETER["Longitude '
                    "of natural "
                    'origin",-117,ANGLEUNIT["degree",0.0174532925199433],ID["EPSG",8802]],PARAMETER["Scale '
                    "factor at natural "
                    'origin",0.9996,SCALEUNIT["unity",1],ID["EPSG",8805]],PARAMETER["False '
                    'easting",500000,LENGTHUNIT["metre",1],ID["EPSG",8806]],PARAMETER["False '
                    'northing",0,LENGTHUNIT["metre",1],ID["EPSG",8807]]],CS[Cartesian,2],AXIS["easting",east,ORDER[1],LENGTHUNIT["metre",1]],AXIS["northing",north,ORDER[2],LENGTHUNIT["metre",1]],ID["EPSG",26711]]',
                },
            },
            "x": {
                "attributes": {
                    "long_name": "x coordinate of projection",
                    "standard_name": "projection_x_coordinate",
                },
                "datatype": "Float64",
                "dimension_size": [
                    20,
                ],
                "dimensions": [
                    "/x",
                ],
                "full_name": "/x",
                "unit": "m",
            },
            "y": {
                "attributes": {
                    "long_name": "y coordinate of projection",
                    "standard_name": "projection_y_coordinate",
                },
                "datatype": "Float64",
                "dimension_size": [
                    20,
                ],
                "dimensions": [
                    "/y",
                ],
                "full_name": "/y",
                "unit": "m",
            },
        },
        "attributes": {
            "Conventions": "CF-1.5",
            "GDAL": "GDAL 3.8.0dev-refs/heads-dirty, released 2023/10/09 (debug build)",
            "GDAL_AREA_OR_POINT": "Area",
            "history": "Mon Oct 09 18:27:35 2023: GDAL CreateCopy( byte.nc, ... )",
        },
        "dimensions": [
            {
                "full_name": "/x",
                "indexing_variable": {
                    "x": {
                        "attributes": {
                            "long_name": "x coordinate of projection",
                            "standard_name": "projection_x_coordinate",
                        },
                        "datatype": "Float64",
                        "dimension_size": [
                            20,
                        ],
                        "dimensions": [
                            "/x",
                        ],
                        "full_name": "/x",
                        "unit": "m",
                    },
                },
                "name": "x",
                "size": 20,
                "type": "HORIZONTAL_X",
            },
            {
                "full_name": "/y",
                "indexing_variable": {
                    "y": {
                        "attributes": {
                            "long_name": "y coordinate of projection",
                            "standard_name": "projection_y_coordinate",
                        },
                        "datatype": "Float64",
                        "dimension_size": [
                            20,
                        ],
                        "dimensions": [
                            "/y",
                        ],
                        "full_name": "/y",
                        "unit": "m",
                    },
                },
                "name": "y",
                "size": 20,
                "type": "HORIZONTAL_Y",
            },
        ],
        "driver": "netCDF",
        "name": "/",
        "structural_info": {
            "NC_FORMAT": "CLASSIC",
        },
        "type": "group",
    }


def test_gdalalg_mdim_info_completion_array_invalid_ds(gdal_path):

    out = gdaltest.runexternal(
        f"{gdal_path} completion gdal mdim info ../gdrivers/data/netcdf/i_do_not_exist.nc --array"
    )
    assert out.startswith("**")


def test_gdalalg_mdim_info_completion_array(gdal_path):

    out = gdaltest.runexternal(
        f"{gdal_path} completion gdal mdim info ../gdrivers/data/netcdf/byte.nc --array"
    ).split(" ")
    assert out == ["/x", "/y", "/Band1"]


def test_gdalalg_mdim_info_completion_array_option_invalid_ds(gdal_path):

    out = gdaltest.runexternal(
        f"{gdal_path} completion gdal mdim info ../gdrivers/data/netcdf/i_do_not_exist.nc --array-option"
    )
    assert out.startswith("**")


def test_gdalalg_mdim_info_completion_array_option(gdal_path):

    out = gdaltest.runexternal(
        f"{gdal_path} completion gdal mdim info ../gdrivers/data/netcdf/byte.nc --array-option"
    ).split(" ")
    assert "USE_DEFAULT_FILL_AS_NODATA=" in out


def test_gdalalg_mdim_info_summary():
    info = gdal.alg.mdim.info(input="../gdrivers/data/netcdf/byte.nc", summary=True)
    output_string = info["output-string"]
    j = json.loads(output_string)
    assert j == {
        "arrays": {
            "Band1": {"full_name": "/Band1"},
            "x": {"full_name": "/x"},
            "y": {"full_name": "/y"},
        },
        "driver": "netCDF",
        "name": "/",
        "type": "group",
    }
