###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Support functions for OGR tests.
# Author:   Frank Warmerdam <warmerdam@pobox.com>
#
###############################################################################
# Copyright (c) 2003, Frank Warmerdam <warmerdam@pobox.com>
#
# This library is free software; you can redistribute it and/or
# modify it under the terms of the GNU Library General Public
# License as published by the Free Software Foundation; either
# version 2 of the License, or (at your option) any later version.
#
# This library is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
# Library General Public License for more details.
#
# You should have received a copy of the GNU Library General Public
# License along with this library; if not, write to the
# Free Software Foundation, Inc., 59 Temple Place - Suite 330,
# Boston, MA 02111-1307, USA.
###############################################################################

import contextlib
import sys

sys.path.append("../pymod")

import gdaltest
import pytest

from osgeo import ogr

geos_flag = None
sfcgal_flag = None

###############################################################################


def check_features_against_list(layer, field_name, value_list):
    __tracebackhide__ = True

    field_index = layer.GetLayerDefn().GetFieldIndex(field_name)
    assert field_index >= 0, f"did not find required field {field_name}"

    for i, value in enumerate(value_list):
        feat = layer.GetNextFeature()

        assert (
            feat is not None
        ), f"Got only {i} features, not the expected {len(value_list)} features."

        failure_message = (
            "field %s feature %d did not match expected value %s, got %s."
            % (field_name, i, str(value), str(feat.GetField(field_index)))
        )

        if isinstance(value, type("str")):
            assert feat.GetFieldAsString(field_index) == value, failure_message
        else:
            assert feat.GetField(field_index) == value, failure_message

    feat = layer.GetNextFeature()
    assert feat is None, "got more features than expected"


###############################################################################


@gdaltest.disable_exceptions()
def check_feature_geometry(
    actual,
    expected,
    max_error=0.0001,
    *,
    pointwise=False,
    context=None,
    _root_actual=None,
    _root_expected=None,
):
    """
    Check that a geometry matches an expected value.

    :param actual: ogr.Feature or ogr.Geometry to test
    :param expected: ogr.Geometry or WKT string representing expected value
    :param max_error: maximum error in any single ordinate (applies to pointwise comparison only)
    :param pointwise: if True, a pointwise comparison will be used to check structural equality (ring ordering, orientation, etc.)
                      if False, the pointwise comparison will be done only if a topological equality check (ST_Equals semantics) fails.
    :param context: Optional context information to include in assertion failure message (e.g., "i = 5")
    """
    __tracebackhide__ = True
    if type(actual) is ogr.Feature:
        actual = actual.GetGeometryRef()

    if context:
        context_msg = f" ({context})"
    else:
        context_msg = ""

    if expected is not None and isinstance(expected, type("a")):
        expected = ogr.CreateGeometryFromWkt(expected)
        assert expected is not None, (
            "failed to parse expected geometry as WKT" + context_msg
        )

    if expected is None:
        assert actual is None, "expected NULL geometry but got one" + context_msg
        return
    else:
        assert actual is not None, "expected geometry but got NULL" + context_msg

    assert expected.GetGeometryName() == expected.GetGeometryName()

    assert actual.GetGeometryName() == expected.GetGeometryName(), (
        "geometry types do not match" + context_msg
    )

    assert actual.GetGeometryCount() == expected.GetGeometryCount(), (
        "sub-geometry counts do not match" + context_msg
    )

    assert actual.GetPointCount() == expected.GetPointCount(), (
        "point counts do not match" + context_msg
    )

    if expected.Is3D():
        assert actual.Is3D(), "expected Z dimension not found"
    if actual.Is3D():
        assert expected.Is3D(), "unexpected Z dimension"

    if expected.IsMeasured():
        assert actual.IsMeasured(), "expected M dimension not found"
    if actual.IsMeasured():
        assert expected.IsMeasured(), "unexpected M dimension"

    # ST_Equals(a,b) <==> ST_Within(a,b) && ST_Within(b,a)
    # We can't use OGRGeometry::Equals() because it doesn't test spatial
    # equality, but structural one
    # Within does not take into account Z or M values, so we skip to the
    # pointwise check if they are present.
    if (
        (not pointwise)
        and have_geos()
        and actual.Within(expected)
        and expected.Within(actual)
        and (not actual.Is3D())
        and (not actual.IsMeasured())
    ):
        return

    if _root_actual is None:
        _root_actual = actual
    if _root_expected is None:
        _root_expected = expected

    if actual.GetGeometryCount() > 0:
        count = actual.GetGeometryCount()
        for i in range(count):
            check_feature_geometry(
                actual.GetGeometryRef(i),
                expected.GetGeometryRef(i),
                max_error,
                context=context,
                _root_actual=_root_actual,
                _root_expected=_root_expected,
            )
    else:
        count = actual.GetPointCount()
        if ogr.GT_Flatten(actual.GetGeometryType()) == ogr.wkbPoint:
            count = 1

            # Point Empty is often encoded with NaN values, hence do not attempt
            # X/Y comparisons
            if expected.IsEmpty():
                assert actual.IsEmpty()
                return
            else:
                assert not actual.IsEmpty()

        for i in range(count):
            actual_pt = [actual.GetX(i), actual.GetY(i)]
            expected_pt = [expected.GetX(i), expected.GetY(i)]

            if actual.Is3D() or expected.Is3D():
                actual_pt.append(actual.GetZ(i))
                expected_pt.append(expected.GetZ(i))

            if actual.IsMeasured() or expected.IsMeasured():
                # Hack to deal with shapefile not-a-number M values that equal to -1.79769313486232e+308
                if actual.GetM(i) >= -1.7e308 and expected.GetM(i) >= -1.7e308:
                    actual_pt.append(actual.GetM(i))
                    expected_pt.append(expected.GetM(i))

            assert actual_pt == pytest.approx(
                expected_pt, abs=max_error
            ), f"Error in vertex {i+1}/{count} exceeds tolerance. {context_msg}\n  Expected: {_root_expected.ExportToWkt()}\n  Actual: {_root_actual.ExportToWkt()}"


###############################################################################


def check_feature(feat, feat_ref, max_error=0.0001, excluded_fields=None):
    __tracebackhide__ = True

    for i in range(feat.GetGeomFieldCount()):
        check_feature_geometry(
            feat.GetGeomFieldRef(i), feat_ref.GetGeomFieldRef(i), max_error=max_error
        )

    for i in range(feat.GetFieldCount()):
        if excluded_fields is not None:
            if feat.GetDefnRef().GetFieldDefn(i).GetName() in excluded_fields:
                continue

        assert feat.GetField(i) == feat_ref.GetField(
            i
        ), f"Field {i}, expected {feat.GetField(i)}, got {feat_ref.GetField(i)}"


###############################################################################


def compare_layers(lyr, lyr_ref, excluded_fields=None):

    for f_ref in lyr_ref:
        f = lyr.GetNextFeature()
        assert f is not None, "not enough features"
        check_feature(f, f_ref, excluded_fields=excluded_fields)
    f = lyr.GetNextFeature()
    assert f is None, "more features than expected"


###############################################################################


def get_wkt_data_series(with_z, with_m, with_gc, with_circular, with_surface):
    basic_wkts = [
        "POINT (1 1)",
        "POINT (1.1234 1.4321)",
        "POINT (1.12345678901234 1.4321)",
        "POINT (1.2 -2.1)",
        "MULTIPOINT ((10 40),(40 30),(20 20),(30 10))",
        "LINESTRING (1.2 -2.1,2.4 -4.8)",
        "MULTILINESTRING ((10 10,20 20,10 40),(40 40,30 30,40 20,30 10),(50 50,60 60,50 90))",
        "MULTILINESTRING ((1.2 -2.1,2.4 -4.8))",
        "POLYGON ((30 10,40 40,20 40,10 20,30 10))",
        "POLYGON ((35 10,45 45,15 40,10 20,35 10),(20 30,35 35,30 20,20 30))",
        "MULTIPOLYGON (((30 20,45 40,10 40,30 20)),((15 5,40 10,10 20,5 10,15 5)))",
        "MULTIPOLYGON (((40 40,20 45,45 30,40 40)),((20 35,10 30,10 10,30 5,45 20,20 35),(30 20,20 15,20 25,30 20)))",
        "MULTIPOLYGON (((30 20,45 40,10 40,30 20)))",
        "MULTIPOLYGON (((35 10,45 45,15 40,10 20,35 10),(20 30,35 35,30 20,20 30)))",
    ]
    gc_wkts = [
        "GEOMETRYCOLLECTION (POINT (4 6),LINESTRING (4 6,7 10))",
        "GEOMETRYCOLLECTION (POINT (4 6),GEOMETRYCOLLECTION (POINT (4 6),LINESTRING (4 6,7 10)))",
    ]
    circular_wkts = [
        "CIRCULARSTRING (0 0,1 1,1 0)",
        "COMPOUNDCURVE (CIRCULARSTRING (0 0,1 1,1 0),(1 0,0 1))",
        "CURVEPOLYGON (CIRCULARSTRING (0 0,4 0,4 4,0 4,0 0),(1 1,3 3,3 1,1 1))",
        "MULTICURVE ((0 0,5 5),CIRCULARSTRING (4 0,4 4,8 4))",
        "MULTISURFACE (CURVEPOLYGON (CIRCULARSTRING (0 0,4 0,4 4,0 4,0 0),(1 1,3 3,3 1,1 1)),((10 10,14 12,11 10,10 10),(11 11,11.5 11.0,11.0 11.5,11 11)))",
    ]
    surface_wkts = [
        "POLYHEDRALSURFACE Z (((0 0 0,0 0 1,0 1 1,0 1 0,0 0 0)),((0 0 0,0 1 0,1 1 0,1 0 0,0 0 0)),((0 0 0,1 0 0,1 0 1,0 0 1,0 0 0)),((1 1 0,1 1 1,1 0 1,1 0 0,1 1 0)),((0 1 0,0 1 1,1 1 1,1 1 0,0 1 0)),((0 0 1,1 0 1,1 1 1,0 1 1,0 0 1)))",
        "TRIANGLE ((0 0,0 9,9 0,0 0))",
        "TIN Z (((0 0 0,0 0 1,0 1 0,0 0 0)))",
        "TIN Z (((0 0 0,0 0 1,0 1 0,0 0 0)),((0 0 0,0 1 0,1 1 0,0 0 0)))",
    ]

    wkts = basic_wkts
    wkts_with_z = []
    wkts_with_m = []
    wkts_with_zm = []
    if with_gc:
        wkts.extend(gc_wkts)
    if with_circular:
        wkts.extend(circular_wkts)
    if with_z or with_m:
        for i, wkt in enumerate(wkts):
            if with_z:
                g = ogr.CreateGeometryFromWkt(wkt)
                g.Set3D(True)
                wkts_with_z.extend([g.ExportToIsoWkt()])
            if with_z:
                g = ogr.CreateGeometryFromWkt(wkt)
                g.SetMeasured(True)
                wkts_with_m.extend([g.ExportToIsoWkt()])
            if with_z and with_m:
                g = ogr.CreateGeometryFromWkt(wkt)
                g.Set3D(True)
                g.SetMeasured(True)
                wkts_with_zm.extend([g.ExportToIsoWkt()])
    wkts.extend(wkts_with_z)
    wkts.extend(wkts_with_m)
    wkts.extend(wkts_with_zm)
    if with_surface:
        wkts.extend(surface_wkts)
    return wkts


###############################################################################


def quick_create_layer_def(lyr, field_list):
    # Each field is a tuple of (name, type, width, precision)
    # Any of type, width and precision can be skipped.  Default type is string.

    for field in field_list:
        name = field[0]
        if len(field) > 1:
            field_type = field[1]
        else:
            field_type = ogr.OFTString

        field_defn = ogr.FieldDefn(name, field_type)

        if len(field) > 2:
            field_defn.SetWidth(int(field[2]))

        if len(field) > 3:
            field_defn.SetPrecision(int(field[3]))

        lyr.CreateField(field_defn)


###############################################################################


def quick_create_feature(layer, field_values, wkt_geometry):
    feature = ogr.Feature(feature_def=layer.GetLayerDefn())

    for i, field_value in enumerate(field_values):
        feature.SetField(i, field_value)

    if wkt_geometry is not None:
        geom = ogr.CreateGeometryFromWkt(wkt_geometry)
        if geom is None:
            raise ValueError("Failed to create geometry from: " + wkt_geometry)
        feature.SetGeometryDirectly(geom)

    result = layer.CreateFeature(feature)

    if result != 0:
        raise ValueError("CreateFeature() failed in ogrtest.quick_create_feature()")


###############################################################################


def have_geos():
    return ogr.GetGEOSVersionMajor() > 0


###############################################################################


def have_sfcgal():
    global sfcgal_flag

    if sfcgal_flag is None:
        with gdaltest.disable_exceptions():
            pnt1 = ogr.CreateGeometryFromWkt("POINT(10 20 30)")
            pnt2 = ogr.CreateGeometryFromWkt("POINT(40 50 60)")
            sfcgal_flag = pnt1.Distance3D(pnt2) >= 0

    return sfcgal_flag


###############################################################################
# Temporarily set an attribute filter


@contextlib.contextmanager
def attribute_filter(lyr, filter_txt):
    lyr.SetAttributeFilter(filter_txt)
    try:
        yield
    finally:
        lyr.SetAttributeFilter(None)


###############################################################################
# Temporarily set a spatial filter
# Single argument is parsed as WKT or assumed to be an OGRGeometry
# Four arguments are interpreted as bounding rectangle


@contextlib.contextmanager
def spatial_filter(lyr, *args):

    if len(args) == 1:
        if type(args[0]) is str:
            geom = ogr.CreateGeometryFromWkt(args[0])
            lyr.SetSpatialFilter(geom)
        else:
            lyr.SetSpatialFilter(args[0])
    elif len(args) == 4:
        lyr.SetSpatialFilterRect(*args)
    else:
        raise Exception("Unknown spatial filter type")
    try:
        yield
    finally:
        lyr.SetSpatialFilter(None)
