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

from osgeo import gdal, osr


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
@pytest.mark.require_driver("Zarr")
def test_gdalalg_mdim_reproject_to_multidim_only_driver(tmp_vsimem):

    out_filename = str(tmp_vsimem / "out.zarr")
    gdal.alg.mdim.reproject(
        input="../gdrivers/data/netcdf/byte.nc",
        output_crs="EPSG:3857",
        output_format="Zarr",
        output=out_filename,
    )

    with gdal.OpenEx(out_filename, gdal.OF_MULTIDIM_RASTER) as ds:
        ar = ds.GetRootGroup().OpenMDArray("Band1")
        assert ar.GetSpatialRef().GetAuthorityCode(None) == "3857"


@pytest.mark.require_driver("netCDF")
def test_gdalalg_mdim_reproject_vrt_not_possible(tmp_vsimem):

    with pytest.raises(Exception, match="Dataset is not compatible of VRT output"):
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


def test_gdalalg_mdim_reproject_array_names():

    src_ds = gdal.GetDriverByName("MEM").CreateMultiDimensional("myds")
    rg = src_ds.GetRootGroup()
    f64 = gdal.ExtendedDataType.Create(gdal.GDT_Float64)

    dimTime = rg.CreateDimension("time", None, None, 2)
    dimY = rg.CreateDimension("y", "HORIZONTAL_Y", "NORTH", 4)
    dimX = rg.CreateDimension("x", "HORIZONTAL_X", "EAST", 5)

    arTime = rg.CreateMDArray("time", [dimTime], f64)
    arTime.Write([0.0, 1.0])
    dimTime.SetIndexingVariable(arTime)

    arY = rg.CreateMDArray("y", [dimY], f64)
    arY.Write([3.5, 2.5, 1.5, 0.5])
    dimY.SetIndexingVariable(arY)

    arX = rg.CreateMDArray("x", [dimX], f64)
    arX.Write([0.5, 1.5, 2.5, 3.5, 4.5])
    dimX.SetIndexingVariable(arX)

    ar = rg.CreateMDArray(
        "ar",
        [dimTime, dimY, dimX],
        gdal.ExtendedDataType.Create(gdal.GDT_Float32),
    )
    srs = osr.SpatialReference()
    srs.ImportFromEPSG(4326)
    srs.SetDataAxisToSRSAxisMapping([2, 3])
    ar.SetSpatialRef(srs)

    with gdal.alg.mdim.reproject(
        input=src_ds,
        output_crs="EPSG:3857",
        output="",
        output_format="stream",
    ) as alg:
        out_rg = alg.Output().GetRootGroup()
        names = out_rg.GetMDArrayNames()
        # "x" and "y" are replaced by "dimX" and "dimY", "time" is preserved
        assert sorted(names) == ["ar", "dimX", "dimY", "time"]
        for name in names:
            assert out_rg.OpenMDArray(name) is not None
