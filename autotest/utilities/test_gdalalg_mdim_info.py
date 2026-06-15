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


def test_gdalalg_mdim_info_binary_json(gdal_path):

    out = gdaltest.runexternal(
        f"{gdal_path} mdim info ../gdrivers/data/netcdf/byte.nc --format json"
    )
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


def test_gdalalg_mdim_info_text():

    with gdal.alg.mdim.info(
        input="../gdrivers/data/netcdf/byte.nc", format="text"
    ) as alg:
        out = alg.Output()

    out = "\n".join([x.rstrip() for x in out.replace("\r\n", "\n").split("\n")])
    assert out == """Driver: netCDF

Structural metadata:
  NC_FORMAT  CLASSIC

Dimensions:
  Name (path)  Size      Type      Direction
  -----------  ----  ------------  ---------
  /x             20  HORIZONTAL_X
  /y             20  HORIZONTAL_Y

Coordinates (indexing variables):
  Name (path)  Dimension   Type    Unit
  -----------  ---------  -------  ----
  /x           (x)        Float64  m
  /y           (y)        Float64  m

Data variables:
  Name (path)  Type  Unit   Shape    Chunk size
  -----------  ----  ----  --------  ----------

 (/y, /x):
  /Band1       Byte        [20, 20]  (unknown)

Attributes:
         Name          Type                                  Value
  ------------------  ------  -------------------------------------------------------------------
  GDAL_AREA_OR_POINT  String  "Area"
  Conventions         String  "CF-1.5"
  GDAL                String  "GDAL 3.8.0dev-refs/heads-dirty, released 2023/10/09 (debug build)"
  history             String  "Mon Oct 09 18:27:35 2023: GDAL CreateCopy( byte.nc, ... )"

Arrays:

  - /x:
      Dimensions:  (/x)
      Shape:       [20]
      Type:        Float64
      Unit:        m

      Attributes:
            Name        Type              Value
        -------------  ------  ----------------------------
        standard_name  String  "projection_x_coordinate"
        long_name      String  "x coordinate of projection"

  - /y:
      Dimensions:  (/y)
      Shape:       [20]
      Type:        Float64
      Unit:        m

      Attributes:
            Name        Type              Value
        -------------  ------  ----------------------------
        standard_name  String  "projection_y_coordinate"
        long_name      String  "y coordinate of projection"

  - /Band1:
      Dimensions:  (/y, /x)
      Shape:       [20, 20]
      Type:        Byte

      Attributes:
           Name       Type          Value
        -----------  ------  --------------------
        long_name    String  "GDAL Band Number 1"
        valid_range  UInt16  [0,255]

      Coordinate Reference System:
        - name: NAD27 / UTM zone 11N
        - ID: EPSG:26711
        - type: Projected
        - projection type: UTM zone 11N, Transverse Mercator
        - units: metre
""", out


def test_gdalalg_mdim_info_text_summary():

    with gdal.alg.mdim.info(
        input="../gdrivers/data/netcdf/byte.nc", summary=True, format="text"
    ) as alg:
        out = alg.Output()

    out = "\n".join([x.rstrip() for x in out.replace("\r\n", "\n").split("\n")])
    assert out == """Driver: netCDF

Structural metadata:
  NC_FORMAT  CLASSIC

Dimensions:
  Name (path)  Size      Type      Direction
  -----------  ----  ------------  ---------
  /x             20  HORIZONTAL_X
  /y             20  HORIZONTAL_Y

Coordinates (indexing variables):
  Name (path)  Dimension   Type    Unit
  -----------  ---------  -------  ----
  /x           (x)        Float64  m
  /y           (y)        Float64  m

Data variables:
  Name (path)  Type  Unit   Shape    Chunk size
  -----------  ----  ----  --------  ----------

 (/y, /x):
  /Band1       Byte        [20, 20]  (unknown)
""", out


def test_gdalalg_mdim_info_text_array_detailed_stats():

    with gdal.alg.mdim.info(
        input="../gdrivers/data/netcdf/byte.nc",
        detailed=True,
        stats=True,
        array="/Band1",
        format="text",
    ) as alg:
        out = alg.Output()

    out = "\n".join([x.rstrip() for x in out.replace("\r\n", "\n").split("\n")])
    assert out == """Arrays:

  - /Band1:
      Dimensions:  (/y, /x)
      Shape:       [20, 20]
      Type:        Byte

      Attributes:
           Name       Type          Value
        -----------  ------  --------------------
        long_name    String  "GDAL Band Number 1"
        valid_range  UInt16  [0,255]

      Coordinate Reference System:
        - name: NAD27 / UTM zone 11N
        - ID: EPSG:26711
        - type: Projected
        - projection type: UTM zone 11N, Transverse Mercator
        - units: metre

      Statistics:
        min                  74.000000
        max                 255.000000
        mean                126.765000
        stddev               22.928471
        valid sample count         400

      Values:
      [
        [181, 181, 156, 148, 156, 156, 156, 181, 132, 148, 115, 132, 107, 107, 107, 107, 107, 115, 99, 107],
        [173, 247, 255, 206, 132, 107, 140, 123, 148, 132, 165, 165, 148, 140, 132, 123, 107, 123, 107, 123],
        [156, 181, 140, 173, 123, 132, 99, 115, 123, 74, 115, 99, 123, 140, 156, 132, 165, 140, 140, 99],
        [189, 173, 140, 140, 165, 115, 132, 90, 99, 115, 90, 99, 99, 107, 99, 132, 99, 107, 132, 132],
        [165, 148, 156, 123, 107, 107, 107, 115, 140, 99, 115, 99, 99, 107, 115, 132, 115, 90, 123, 115],
        [140, 107, 140, 90, 107, 115, 107, 90, 99, 123, 115, 115, 115, 123, 123, 148, 115, 148, 99, 132],
        [148, 132, 132, 107, 123, 99, 99, 115, 99, 132, 99, 140, 115, 148, 123, 99, 132, 123, 148, 140],
        [173, 148, 99, 123, 123, 107, 123, 99, 107, 189, 173, 107, 115, 115, 107, 99, 140, 107, 173, 140],
        [123, 123, 123, 107, 140, 123, 123, 115, 115, 90, 107, 173, 107, 107, 107, 107, 99, 132, 123, 115],
        [132, 132, 132, 123, 99, 132, 123, 107, 148, 99, 115, 123, 140, 173, 123, 107, 123, 123, 123, 107],
        [140, 140, 99, 140, 99, 115, 123, 107, 132, 107, 115, 107, 115, 123, 132, 123, 107, 123, 132, 132],
        [123, 115, 132, 115, 123, 132, 115, 132, 132, 123, 123, 132, 99, 115, 99, 123, 132, 115, 115, 107],
        [148, 123, 148, 115, 148, 123, 140, 123, 107, 115, 132, 115, 107, 115, 99, 123, 99, 181, 99, 107],
        [197, 173, 148, 140, 140, 132, 99, 132, 123, 115, 140, 132, 132, 99, 132, 123, 132, 173, 123, 115],
        [189, 173, 173, 148, 148, 115, 148, 123, 107, 132, 115, 132, 156, 99, 123, 115, 132, 132, 206, 107],
        [132, 156, 132, 140, 132, 132, 115, 115, 115, 123, 148, 123, 165, 123, 132, 107, 107, 132, 156, 123],
        [148, 132, 123, 123, 115, 132, 132, 123, 115, 123, 115, 123, 107, 115, 148, 107, 115, 140, 115, 132],
        [115, 132, 140, 132, 123, 115, 140, 107, 140, 115, 132, 123, 107, 132, 132, 115, 115, 107, 115, 107],
        [115, 132, 107, 123, 148, 115, 165, 115, 140, 107, 123, 123, 99, 132, 123, 132, 132, 132, 99, 156],
        [107, 123, 132, 115, 132, 132, 140, 132, 132, 132, 107, 132, 107, 132, 132, 107, 123, 115, 156, 148]
      ]
""", out


def test_gdalalg_mdim_info_text_structural_md_and_line_truncation():

    with gdal.alg.mdim.info(
        input="../gdrivers/data/hdf5/deflate.h5", format="text"
    ) as alg:
        out = alg.Output()

    out = "\n".join([x.rstrip() for x in out.replace("\r\n", "\n").split("\n")])
    assert out == """Driver: HDF5

Dimensions:
  Name (path)  Size  Type  Direction
  -----------  ----  ----  ---------
  /x             20
  /y             20

Data variables:
  Name (path)  Type  Unit   Shape    Chunk size
  -----------  ----  ----  --------  ----------

 (/y, /x):
  /Band1       Byte        [20, 20]  [1, 2]

Scalar arrays:
      Name (path)        Type   Unit
  --------------------  ------  ----
  /transverse_mercator  String

Attributes:
     Name       Type    Value
  -----------  ------  --------
  Conventions  String  "CF-1.6"

Arrays:

  - /Band1:
      Dimensions:    (/y, /x)
      Shape:         [20, 20]
      Chunk size:    [1, 2]
      Type:          Byte
      Nodata value:  0

      Attributes:
            Name       Type           Value
        ------------  ------  ---------------------
        long_name     String  "GDAL Band Number 1"
        valid_range   UInt16  [0,255]
        grid_mapping  String  "transverse_mercator"

      Structural metadata:
        COMPRESSION  DEFLATE
        FILTER       SHUFFLE

  - /transverse_mercator:
      Dimensions:  ()
      Shape:       []
      Type:        String

      Attributes:
                      Name                 Type                                         Value
        --------------------------------  -------  -------------------------------------------------------------------------------
        grid_mapping_name                 String   "transverse_mercator"
        longitude_of_central_meridian     Float64  -117
        false_easting                     Float64  500000
        false_northing                    Float64  0
        latitude_of_projection_origin     Float64  0
        scale_factor_at_central_meridian  Float64  0.99960000000000004
        long_name                         String   "CRS definition"
        longitude_of_prime_meridian       Float64  0
        semi_major_axis                   Float64  6378206.4000000004
        inverse_flattening                Float64  294.97869821389821
        crs_wkt                           String   "PROJCS[\\"NAD27 / UTM zone 11N\\",GEOGCS[\\"NAD27\\",
                                                   DATUM[\\"North_American_Datum_1927\\",SPHEROID[\\"Clarke 1866\\",6378206.4,
                                                   294.978698213898,AUTHORITY[\\"EPSG\\",\\"7008\\"]],AUTHORITY[\\"EPSG\\",\\"6267\\"]],
                                                   PRIMEM[\\"Greenwich\\",0,AUTHORITY[\\"EPSG\\",\\"8901\\"]],UNIT[\\"degree\\",
                                                   0.0174532925199433,AUTHORITY[\\"EPSG\\",\\"9122\\"]],AUTHORITY[\\"EPSG\\",\\"4267\\"]],
                                                   PROJECTION[\\"Transverse_Mercator\\"],PARAMETER[\\"latitude_of_origin\\",0],
                                                   PARAMETER[\\"central_meridian\\",-117],PARAMETER[\\"scale_factor\\",0.9996],
                                                   PARAMETER[\\"false_easting\\",500000],PARAMETER[\\"false_northing\\",0],
                                                   UNIT[\\"metre\\",1,AUTHORITY[\\"EPSG\\",\\"9001\\"]],AXIS[\\"Easting\\",EAST],
                                                   AXIS[\\"Northing\\",NORTH],AUTHORITY[\\"EPSG\\",\\"26711\\"]]"
        spatial_ref                       String   "PROJCS[\\"NAD27 / UTM zone 11N\\",GEOGCS[\\"NAD27\\",
                                                   DATUM[\\"North_American_Datum_1927\\",SPHEROID[\\"Clarke 1866\\",6378206.4,
                                                   294.978698213898,AUTHORITY[\\"EPSG\\",\\"7008\\"]],AUTHORITY[\\"EPSG\\",\\"6267\\"]],
                                                   PRIMEM[\\"Greenwich\\",0,AUTHORITY[\\"EPSG\\",\\"8901\\"]],UNIT[\\"degree\\",
                                                   0.0174532925199433,AUTHORITY[\\"EPSG\\",\\"9122\\"]],AUTHORITY[\\"EPSG\\",\\"4267\\"]],
                                                   PROJECTION[\\"Transverse_Mercator\\"],PARAMETER[\\"latitude_of_origin\\",0],
                                                   PARAMETER[\\"central_meridian\\",-117],PARAMETER[\\"scale_factor\\",0.9996],
                                                   PARAMETER[\\"false_easting\\",500000],PARAMETER[\\"false_northing\\",0],
                                                   UNIT[\\"metre\\",1,AUTHORITY[\\"EPSG\\",\\"9001\\"]],AXIS[\\"Easting\\",EAST],
                                                   AXIS[\\"Northing\\",NORTH],AUTHORITY[\\"EPSG\\",\\"26711\\"]]"
"""


def test_gdalalg_mdim_info_binary_text(gdal_path):

    out = gdaltest.runexternal(f"{gdal_path} mdim info ../gdrivers/data/netcdf/byte.nc")
    out = "\n".join([x.rstrip() for x in out.replace("\r\n", "\n").split("\n")])
    assert out == """Driver: netCDF

Structural metadata:
  NC_FORMAT  CLASSIC

Dimensions:
  Name (path)  Size      Type      Direction
  -----------  ----  ------------  ---------
  /x             20  HORIZONTAL_X
  /y             20  HORIZONTAL_Y

Coordinates (indexing variables):
  Name (path)  Dimension   Type    Unit
  -----------  ---------  -------  ----
  /x           (x)        Float64  m
  /y           (y)        Float64  m

Data variables:
  Name (path)  Type  Unit   Shape    Chunk size
  -----------  ----  ----  --------  ----------

 (/y, /x):
  /Band1       Byte        [20, 20]  (unknown)

Attributes:
         Name          Type                                  Value
  ------------------  ------  -------------------------------------------------------------------
  GDAL_AREA_OR_POINT  String  "Area"
  Conventions         String  "CF-1.5"
  GDAL                String  "GDAL 3.8.0dev-refs/heads-dirty, released 2023/10/09 (debug build)"
  history             String  "Mon Oct 09 18:27:35 2023: GDAL CreateCopy( byte.nc, ... )"

Arrays:

  - /x:
      Dimensions:  (/x)
      Shape:       [20]
      Type:        Float64
      Unit:        m

      Attributes:
            Name        Type              Value
        -------------  ------  ----------------------------
        standard_name  String  "projection_x_coordinate"
        long_name      String  "x coordinate of projection"

  - /y:
      Dimensions:  (/y)
      Shape:       [20]
      Type:        Float64
      Unit:        m

      Attributes:
            Name        Type              Value
        -------------  ------  ----------------------------
        standard_name  String  "projection_y_coordinate"
        long_name      String  "y coordinate of projection"

  - /Band1:
      Dimensions:  (/y, /x)
      Shape:       [20, 20]
      Type:        Byte

      Attributes:
           Name       Type          Value
        -----------  ------  --------------------
        long_name    String  "GDAL Band Number 1"
        valid_range  UInt16  [0,255]

      Coordinate Reference System:
        - name: NAD27 / UTM zone 11N
        - ID: EPSG:26711
        - type: Projected
        - projection type: UTM zone 11N, Transverse Mercator
        - units: metre
""", out


def test_gdalalg_mdim_info_binary_text_array_and_detailed(gdal_path):

    out = gdaltest.runexternal(
        f"{gdal_path} mdim info --array Band1 --detailed ../gdrivers/data/netcdf/byte.nc"
    )
    assert (
        "\n".join([x.rstrip() for x in out.replace("\r\n", "\n").split("\n")])
        == """Arrays:

  - /Band1:
      Dimensions:  (/y, /x)
      Shape:       [20, 20]
      Type:        Byte

      Attributes:
           Name       Type          Value
        -----------  ------  --------------------
        long_name    String  "GDAL Band Number 1"
        valid_range  UInt16  [0,255]

      Coordinate Reference System:
        - name: NAD27 / UTM zone 11N
        - ID: EPSG:26711
        - type: Projected
        - projection type: UTM zone 11N, Transverse Mercator
        - units: metre

      Values:
      [
        [181, 181, 156, 148, 156, 156, 156, 181, 132, 148, 115, 132, 107, 107, 107, 107, 107, 115, 99, 107],
        [173, 247, 255, 206, 132, 107, 140, 123, 148, 132, 165, 165, 148, 140, 132, 123, 107, 123, 107, 123],
        [156, 181, 140, 173, 123, 132, 99, 115, 123, 74, 115, 99, 123, 140, 156, 132, 165, 140, 140, 99],
        [189, 173, 140, 140, 165, 115, 132, 90, 99, 115, 90, 99, 99, 107, 99, 132, 99, 107, 132, 132],
        [165, 148, 156, 123, 107, 107, 107, 115, 140, 99, 115, 99, 99, 107, 115, 132, 115, 90, 123, 115],
        [140, 107, 140, 90, 107, 115, 107, 90, 99, 123, 115, 115, 115, 123, 123, 148, 115, 148, 99, 132],
        [148, 132, 132, 107, 123, 99, 99, 115, 99, 132, 99, 140, 115, 148, 123, 99, 132, 123, 148, 140],
        [173, 148, 99, 123, 123, 107, 123, 99, 107, 189, 173, 107, 115, 115, 107, 99, 140, 107, 173, 140],
        [123, 123, 123, 107, 140, 123, 123, 115, 115, 90, 107, 173, 107, 107, 107, 107, 99, 132, 123, 115],
        [132, 132, 132, 123, 99, 132, 123, 107, 148, 99, 115, 123, 140, 173, 123, 107, 123, 123, 123, 107],
        [140, 140, 99, 140, 99, 115, 123, 107, 132, 107, 115, 107, 115, 123, 132, 123, 107, 123, 132, 132],
        [123, 115, 132, 115, 123, 132, 115, 132, 132, 123, 123, 132, 99, 115, 99, 123, 132, 115, 115, 107],
        [148, 123, 148, 115, 148, 123, 140, 123, 107, 115, 132, 115, 107, 115, 99, 123, 99, 181, 99, 107],
        [197, 173, 148, 140, 140, 132, 99, 132, 123, 115, 140, 132, 132, 99, 132, 123, 132, 173, 123, 115],
        [189, 173, 173, 148, 148, 115, 148, 123, 107, 132, 115, 132, 156, 99, 123, 115, 132, 132, 206, 107],
        [132, 156, 132, 140, 132, 132, 115, 115, 115, 123, 148, 123, 165, 123, 132, 107, 107, 132, 156, 123],
        [148, 132, 123, 123, 115, 132, 132, 123, 115, 123, 115, 123, 107, 115, 148, 107, 115, 140, 115, 132],
        [115, 132, 140, 132, 123, 115, 140, 107, 140, 115, 132, 123, 107, 132, 132, 115, 115, 107, 115, 107],
        [115, 132, 107, 123, 148, 115, 165, 115, 140, 107, 123, 123, 99, 132, 123, 132, 132, 132, 99, 156],
        [107, 123, 132, 115, 132, 132, 140, 132, 132, 132, 107, 132, 107, 132, 132, 107, 123, 115, 156, 148]
      ]
"""
    ), out


def test_gdalalg_mdim_info_completion_array_invalid_ds(gdal_path):

    out = gdaltest.runexternal(
        f"{gdal_path} completion gdal mdim info ../gdrivers/data/netcdf/i_do_not_exist.nc --array"
    )
    assert out.startswith("**")


def test_gdalalg_mdim_info_completion_array(gdal_path):

    out = gdaltest.run_and_parse_completion_output(
        f"{gdal_path} completion gdal mdim info ../gdrivers/data/netcdf/byte.nc --array"
    )
    assert out == ["/x", "/y", "/Band1"]


def test_gdalalg_mdim_info_completion_array_option_invalid_ds(gdal_path):

    out = gdaltest.runexternal(
        f"{gdal_path} completion gdal mdim info ../gdrivers/data/netcdf/i_do_not_exist.nc --array-option"
    )
    assert out.startswith("**")


def test_gdalalg_mdim_info_completion_array_option(gdal_path):

    out = gdaltest.run_and_parse_completion_output(
        f"{gdal_path} completion gdal mdim info ../gdrivers/data/netcdf/byte.nc --array-option"
    )
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
