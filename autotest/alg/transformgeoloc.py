#!/usr/bin/env pytest
# -*- coding: utf-8 -*-
###############################################################################
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test TransformGeoloc algorithm.
# Author:   Frank Warmerdam <warmerdam@pobox.com>
#
###############################################################################
# Copyright (c) 2012, Frank Warmerdam <warmerdam@pobox.com>
# Copyright (c) 2012, Even Rouault <even dot rouault at spatialys.com>
#
# SPDX-License-Identifier: MIT
###############################################################################


import gdaltest
import pytest

from osgeo import gdal, osr

###############################################################################
# Test a fairly default case.


def test_transformgeoloc_1():

    gdaltest.importorskip_gdal_array()
    numpy = pytest.importorskip("numpy")

    # Setup 2x2 geolocation arrays in a memory dataset with lat/long values.

    drv = gdal.GetDriverByName("MEM")
    geoloc_ds = drv.Create("geoloc_1", 2, 2, 3, gdal.GDT_Float64)

    lon_array = numpy.asarray([[-117.0, -116.0], [-116.5, -115.5]])
    lat_array = numpy.asarray([[45.0, 45.5], [44.0, 44.5]])

    geoloc_ds.GetRasterBand(1).WriteArray(lon_array)
    geoloc_ds.GetRasterBand(2).WriteArray(lat_array)
    # Z left as default zero.

    # Create a wgs84 to utm transformer.

    wgs84_wkt = osr.GetUserInputAsWKT("WGS84")
    utm_wkt = osr.GetUserInputAsWKT("+proj=utm +zone=11 +datum=WGS84")

    ll_utm_transformer = gdal.Transformer(
        None, None, ["SRC_SRS=" + wgs84_wkt, "DST_SRS=" + utm_wkt]
    )

    # transform the geoloc dataset in place.
    status = ll_utm_transformer.TransformGeolocations(
        geoloc_ds.GetRasterBand(1),
        geoloc_ds.GetRasterBand(2),
        geoloc_ds.GetRasterBand(3),
    )

    assert status == 0

    expected = numpy.asarray(
        [
            [[500000.0, 578126.73752062], [540087.07398217, 619246.88515195]],
            [
                [4982950.40022655, 5038982.81207855],
                [4871994.34702622, 4928503.38229753],
            ],
            [[0.0, 0.0], [0.0, 0.0]],
        ]
    )

    assert numpy.allclose(geoloc_ds.ReadAsArray(), expected)
