#!/usr/bin/env pytest
# -*- coding: utf-8 -*-
###############################################################################
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Tests of ogrtest utility functions
# Author:   Dan Baston <dbaston@gmail.com>
#
###############################################################################
# Copyright (c) 2023, ISciences LLC
#
# SPDX-License-Identifier: MIT
###############################################################################

import ogrtest
import pytest

from osgeo import ogr


def test_check_geometry_equals_null():
    geom = ogr.CreateGeometryFromWkt("POINT (3 8)")
    with pytest.raises(AssertionError, match="expected NULL geometry"):
        ogrtest.check_feature_geometry(geom, None)

    with pytest.raises(AssertionError, match="expected geometry"):
        ogrtest.check_feature_geometry(None, geom)


def test_check_geometry_equals_type_mismatch():
    geom = ogr.CreateGeometryFromWkt("POINT (3 8)")

    with pytest.raises(AssertionError, match="geometry types do not match"):
        ogrtest.check_feature_geometry(geom, "LINESTRING (1 1, 2 2)")


def test_check_geometry_equals_dim_mismatch():
    geom_xy = ogr.CreateGeometryFromWkt("POINT (3 8)")
    geom_xyz = ogr.CreateGeometryFromWkt("POINT Z (3 8 1)")
    geom_xym = ogr.CreateGeometryFromWkt("POINT M (3 8 1)")

    with pytest.raises(AssertionError, match="expected Z"):
        ogrtest.check_feature_geometry(geom_xy, geom_xyz)
    with pytest.raises(AssertionError, match="expected Z"):
        ogrtest.check_feature_geometry(geom_xym, geom_xyz)
    with pytest.raises(AssertionError, match="unexpected Z"):
        ogrtest.check_feature_geometry(geom_xyz, geom_xy)
    with pytest.raises(AssertionError, match="unexpected Z"):
        ogrtest.check_feature_geometry(geom_xyz, geom_xym)

    with pytest.raises(AssertionError, match="expected M"):
        ogrtest.check_feature_geometry(geom_xy, geom_xym)
    with pytest.raises(AssertionError, match="unexpected M"):
        ogrtest.check_feature_geometry(geom_xym, geom_xy)


def test_check_geometry_equals_point_count_mismatch():
    geom = ogr.CreateGeometryFromWkt("LINESTRING (1 1, 2 2, 3 3)")

    with pytest.raises(AssertionError, match="point counts do not match"):
        ogrtest.check_feature_geometry(geom, "LINESTRING (1 1, 2 2)")


def test_check_geometry_equals_ngeoms_mismatch():
    geom = ogr.CreateGeometryFromWkt("MULTIPOINT ((1 1), (2 2), (3 3))")

    with pytest.raises(AssertionError, match="counts do not match"):
        ogrtest.check_feature_geometry(geom, "MULTIPOINT ((1 1), (2 2))")


def test_check_geometry_equals_orientation_differs():
    poly_ccw = ogr.CreateGeometryFromWkt("POLYGON ((0 0, 1 0, 1 1, 0 1, 0 0))")
    poly_cw = ogr.CreateGeometryFromWkt("POLYGON ((0 0, 0 1, 1 1, 1 0, 0 0))")

    if ogrtest.have_geos():
        ogrtest.check_feature_geometry(poly_ccw, poly_cw)

    with pytest.raises(AssertionError, match="Error in vertex 2/5"):
        ogrtest.check_feature_geometry(poly_ccw, poly_cw, pointwise=True)

    ogrtest.check_feature_geometry(poly_ccw, poly_cw, max_error=1, pointwise=True)


def test_check_geometry_equals_z_difference():
    geom1 = ogr.CreateGeometryFromWkt("LINESTRING Z (1 2 3, 4 5 6)")
    geom2 = ogr.CreateGeometryFromWkt("LINESTRING Z (1 2 3, 4 5 6.6)")

    with pytest.raises(AssertionError, match="Error in vertex 2/2"):
        ogrtest.check_feature_geometry(geom1, geom2)


def test_check_geometry_equals_m_difference():
    geom1 = ogr.CreateGeometryFromWkt("LINESTRING M (1 2 3, 4 5 6)")
    geom2 = ogr.CreateGeometryFromWkt("LINESTRING M (1 2 3, 4 5 6.6)")

    with pytest.raises(AssertionError, match="Error in vertex 2/2"):
        ogrtest.check_feature_geometry(geom1, geom2)
