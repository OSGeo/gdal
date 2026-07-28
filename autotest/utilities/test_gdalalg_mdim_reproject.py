#!/usr/bin/env pytest
# -*- coding: utf-8 -*-
###############################################################################
# Project:  GDAL/OGR Test Suite
# Purpose:  'gdal mdim reproject' testing
# Author:   Even Rouault <even dot rouault @ spatialys.com>
#
###############################################################################
# Copyright (c) 2026, Even Rouault <even dot rouault at spatialys.com>
#
# SPDX-License-Identifier: MIT
###############################################################################

import pytest

from osgeo import gdal


@pytest.mark.require_driver("netCDF")
def test_gdalalg_mdim_reproject():

    with gdal.alg.mdim.reproject(
        input="../gdrivers/data/netcdf/byte.nc",
        output_crs="EPSG:4326",
        output="",
        output_format="stream",
    ) as alg:
        ds = alg.Output()
        got = gdal.MultiDimInfo(ds)
        # srs removed for robustness w.r.t PROJ versions
        del got["arrays"]["Band1"]["srs"]
        assert got == {
            "type": "group",
            "name": "/",
            "attributes": {
                "GDAL_AREA_OR_POINT": "Area",
                "Conventions": "CF-1.5",
                "GDAL": "GDAL 3.8.0dev-refs/heads-dirty, released 2023/10/09 (debug build)",
                "history": "Mon Oct 09 18:27:35 2023: GDAL CreateCopy( byte.nc, ... )",
            },
            "dimensions": [
                {
                    "name": "dimX",
                    "full_name": "dimX",
                    "size": 22,
                    "type": "HORIZONTAL_X",
                    "direction": "EAST",
                    "indexing_variable": {
                        "dimX": {
                            "full_name": "dimX",
                            "datatype": "Float64",
                            "dimensions": ["dimX"],
                            "dimension_size": [22],
                        }
                    },
                },
                {
                    "name": "dimY",
                    "full_name": "dimY",
                    "size": 18,
                    "type": "HORIZONTAL_Y",
                    "direction": "NORTH",
                    "indexing_variable": {
                        "dimY": {
                            "full_name": "dimY",
                            "datatype": "Float64",
                            "dimensions": ["dimY"],
                            "dimension_size": [18],
                        }
                    },
                },
            ],
            "arrays": {
                "Band1": {
                    "full_name": "/Band1",
                    "datatype": "Byte",
                    "dimensions": [
                        {
                            "name": "dimY",
                            "full_name": "dimY",
                            "size": 18,
                            "type": "HORIZONTAL_Y",
                            "direction": "NORTH",
                            "indexing_variable": {
                                "dimY": {
                                    "full_name": "dimY",
                                    "datatype": "Float64",
                                    "dimensions": ["dimY"],
                                    "dimension_size": [18],
                                }
                            },
                        },
                        {
                            "name": "dimX",
                            "full_name": "dimX",
                            "size": 22,
                            "type": "HORIZONTAL_X",
                            "direction": "EAST",
                            "indexing_variable": {
                                "dimX": {
                                    "full_name": "dimX",
                                    "datatype": "Float64",
                                    "dimensions": ["dimX"],
                                    "dimension_size": [22],
                                }
                            },
                        },
                    ],
                    "dimension_size": [18, 22],
                    "block_size": [18, 22],
                    "attributes": {
                        "long_name": "GDAL Band Number 1",
                        "valid_range": [0, 255],
                    },
                },
                "dimY": {
                    "full_name": "dimY",
                    "datatype": "Float64",
                    "dimensions": [
                        {
                            "name": "dimY",
                            "full_name": "dimY",
                            "size": 18,
                            "type": "HORIZONTAL_Y",
                            "direction": "NORTH",
                            "indexing_variable": {
                                "dimY": {
                                    "full_name": "dimY",
                                    "datatype": "Float64",
                                    "dimensions": ["dimY"],
                                    "dimension_size": [18],
                                }
                            },
                        }
                    ],
                    "dimension_size": [18],
                },
                "dimX": {
                    "full_name": "dimX",
                    "datatype": "Float64",
                    "dimensions": [
                        {
                            "name": "dimX",
                            "full_name": "dimX",
                            "size": 22,
                            "type": "HORIZONTAL_X",
                            "direction": "EAST",
                            "indexing_variable": {
                                "dimX": {
                                    "full_name": "dimX",
                                    "datatype": "Float64",
                                    "dimensions": ["dimX"],
                                    "dimension_size": [22],
                                }
                            },
                        }
                    ],
                    "dimension_size": [22],
                },
            },
        }


@pytest.mark.require_driver("netCDF")
def test_gdalalg_mdim_reproject_vrt_not_possible(tmp_vsimem):

    with pytest.raises(Exception, match="Cannot guess driver"):
        gdal.alg.mdim.reproject(
            input="../gdrivers/data/netcdf/byte.nc",
            output_crs="EPSG:4326",
            output=tmp_vsimem / "out.vrt",
        )


@pytest.mark.require_driver("netCDF")
def test_gdalalg_mdim_reproject_vrt_not_possible_in_pipeline(tmp_vsimem):

    with pytest.raises(Exception, match="Dataset is not compatible of VRT output"):
        gdal.alg.mdim.pipeline(
            pipeline="read ../gdrivers/data/netcdf/byte.nc ! reproject ! write {tmp_vsimem}/out.vrt"
        )
