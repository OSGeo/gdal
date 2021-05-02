#!/usr/bin/env pytest
# -*- coding: utf-8 -*-
###############################################################################
#
# Project:  GDAL/OGR Test Suite
# Purpose:  osgeo_utils.auxiliary.osr_utils (gdal-utils) testing
# Author:   Idan Miara <idan@miara.com>
#
###############################################################################
# Copyright (c) 2021, Idan Miara <idan@miara.com>
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

import pytest

from osgeo import osr

# test that osgeo_utils and numpy are available, otherwise skip all tests
pytest.importorskip('osgeo_utils')

from osgeo_utils.auxiliary import osr_util, array_util


def test_gis_order():
    pj4326_gis2 = osr_util.get_srs(4326)  # tests the correct default
    assert pj4326_gis2.GetAxisMappingStrategy() == osr.OAMS_AUTHORITY_COMPLIANT

    pj4326_auth = osr_util.get_srs(4326, axis_order=osr.OAMS_AUTHORITY_COMPLIANT)
    assert pj4326_auth.GetAxisMappingStrategy() == osr.OAMS_AUTHORITY_COMPLIANT

    pj4326_gis = osr_util.get_srs(4326, axis_order=osr.OAMS_TRADITIONAL_GIS_ORDER)
    assert pj4326_gis.GetAxisMappingStrategy() == osr.OAMS_TRADITIONAL_GIS_ORDER

    assert not osr_util.are_srs_equivalent(pj4326_gis, pj4326_auth)
    assert osr_util.are_srs_equivalent(pj4326_auth, 4326)
    assert not osr_util.are_srs_equivalent(pj4326_gis, 4326)

    pj4326_str = osr_util.get_srs_pj(pj4326_auth)
    pj4326_str2 = osr_util.get_srs_pj(pj4326_gis)

    # axis order is not reflected in proj strings
    assert isinstance(pj4326_str, str) and pj4326_str == pj4326_str2

    assert osr_util.are_srs_equivalent(pj4326_str, 4326)
    assert osr_util.are_srs_equivalent(pj4326_auth, pj4326_str)
    assert not osr_util.are_srs_equivalent(pj4326_gis, pj4326_str)

    osr_util.set_default_axis_order(osr.OAMS_TRADITIONAL_GIS_ORDER)  # sets gis order

    srs = osr_util.get_srs(4326)  # check the the default was changed
    assert srs.GetAxisMappingStrategy() == osr.OAMS_TRADITIONAL_GIS_ORDER

    # check that srs object is not affected by default
    srs = osr_util.get_srs(pj4326_auth)
    assert srs.GetAxisMappingStrategy() == osr.OAMS_AUTHORITY_COMPLIANT
    assert osr_util.are_srs_equivalent(srs, pj4326_auth)

    # check that srs object is also affected if explicitly set
    srs = osr_util.get_srs(pj4326_auth, axis_order=osr.OAMS_TRADITIONAL_GIS_ORDER)
    assert srs.GetAxisMappingStrategy() == osr.OAMS_TRADITIONAL_GIS_ORDER

    # check that the default does not effect explicit order
    srs = osr_util.get_srs(4326, axis_order=osr.OAMS_TRADITIONAL_GIS_ORDER)
    assert srs.GetAxisMappingStrategy() == osr.OAMS_TRADITIONAL_GIS_ORDER

    srs = osr_util.get_srs(pj4326_str)
    assert srs.GetAxisMappingStrategy() == osr.OAMS_TRADITIONAL_GIS_ORDER

    srs = osr_util.get_srs(4326, axis_order=osr.OAMS_AUTHORITY_COMPLIANT)
    assert srs.GetAxisMappingStrategy() == osr.OAMS_AUTHORITY_COMPLIANT

    assert not osr_util.are_srs_equivalent(pj4326_gis, pj4326_auth)
    assert not osr_util.are_srs_equivalent(pj4326_auth, 4326)
    assert osr_util.are_srs_equivalent(pj4326_gis, 4326)

    # restore the default and repeat some tests
    osr_util.set_default_axis_order()

    srs = osr_util.get_srs(4326)  # check the the default was restored
    assert srs.GetAxisMappingStrategy() == osr.OAMS_AUTHORITY_COMPLIANT

    srs = osr_util.get_srs(pj4326_str)
    assert srs.GetAxisMappingStrategy() == osr.OAMS_AUTHORITY_COMPLIANT

    # check that srs object is not affected by default
    srs = osr_util.get_srs(pj4326_gis)  # check that srs object is also affected
    assert srs.GetAxisMappingStrategy() == osr.OAMS_TRADITIONAL_GIS_ORDER

    # check that srs object is also affected if explicitly set
    srs = osr_util.get_srs(pj4326_gis, axis_order=osr.OAMS_AUTHORITY_COMPLIANT)
    assert srs.GetAxisMappingStrategy() == osr.OAMS_AUTHORITY_COMPLIANT

    srs = osr_util.get_srs(4326, axis_order=osr.OAMS_TRADITIONAL_GIS_ORDER)
    assert srs.GetAxisMappingStrategy() == osr.OAMS_TRADITIONAL_GIS_ORDER


def test_gis_order2():
    pj4326_gis = osr_util.get_srs(4326, axis_order=osr.OAMS_TRADITIONAL_GIS_ORDER)
    pj4326_str = osr_util.get_srs_pj(4326)

    srs_from_epsg = osr.SpatialReference()
    srs_from_epsg.ImportFromEPSG(4326)

    assert srs_from_epsg.GetAxisMappingStrategy() == osr.OAMS_AUTHORITY_COMPLIANT
    srs_from_str = osr.SpatialReference()
    srs_from_str.ImportFromProj4(pj4326_str)
    assert srs_from_str.GetAxisMappingStrategy() == osr.OAMS_AUTHORITY_COMPLIANT
    assert srs_from_epsg.IsSame(srs_from_str)

    # testing that explicitly setting OAMS_AUTHORITY_COMPLIANT does not effect equivalence
    srs_from_epsg.SetAxisMappingStrategy(osr.OAMS_AUTHORITY_COMPLIANT)
    srs_from_str.SetAxisMappingStrategy(osr.OAMS_AUTHORITY_COMPLIANT)
    assert srs_from_epsg.IsSame(srs_from_str)

    srs_from_epsg.SetAxisMappingStrategy(osr.OAMS_TRADITIONAL_GIS_ORDER)
    srs_from_str.SetAxisMappingStrategy(osr.OAMS_TRADITIONAL_GIS_ORDER)

    srs_from_epsg2 = osr.SpatialReference()
    srs_from_epsg2.ImportFromEPSG(4326)
    srs_from_epsg2.SetAxisMappingStrategy(osr.OAMS_TRADITIONAL_GIS_ORDER)
    assert srs_from_epsg.IsSame(srs_from_epsg2)

    test_gis_order_proj_str_vs_epsg = False
    # explicitly setting OAMS_TRADITIONAL_GIS_ORDER triggers inequality between srs from proj-string and srs from epsg
    # if this issue is resolved these tests can be enabled
    if test_gis_order_proj_str_vs_epsg:
        assert srs_from_epsg.IsSame(srs_from_str)
        assert osr_util.are_srs_equivalent(pj4326_str, 4326)
        assert osr_util.are_srs_equivalent(pj4326_gis, pj4326_str)


def test_transform():
    pj_utm = osr_util.get_srs(32636)
    utm_x = [690950.4640, 688927.6381]
    utm_y = [3431318.8435, 3542183.4911]

    for gis_order in (False, True):
        axis_order = osr_util.get_axis_order_from_gis_order(gis_order)
        pj4326 = osr_util.get_srs(4326, axis_order=axis_order)
        ct = osr_util.get_transform(pj4326, pj_utm)
        lon = [35, 35]
        lat = [31, 32]
        x, y = (lon, lat) if gis_order else (lat, lon)
        osr_util.transform_points(ct, x, y)
        d = array_util.array_dist(x, utm_x), array_util.array_dist(y, utm_y)
        assert max(d) < 0.01

