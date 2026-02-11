#!/usr/bin/env pytest
# -*- coding: utf-8 -*-
###############################################################################
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test coordinate transformations.
# Author:   Frank Warmerdam <warmerdam@pobox.com>
#
###############################################################################
# Copyright (c) 2003, Frank Warmerdam <warmerdam@pobox.com>
# Copyright (c) 2009-2013, Even Rouault <even dot rouault at spatialys.com>
# Copyright (c) 2014, Google
#
# SPDX-License-Identifier: MIT
###############################################################################

import math

import gdaltest
import pytest

from osgeo import gdal, ogr, osr

###############################################################################
# Verify that we have PROJ.4 available.


def test_osr_ct_1():

    utm_srs = osr.SpatialReference()
    utm_srs.SetUTM(11)
    utm_srs.SetWellKnownGeogCS("WGS84")

    ll_srs = osr.SpatialReference()
    ll_srs.SetWellKnownGeogCS("WGS84")

    try:
        with gdal.quiet_errors():
            ct = osr.CoordinateTransformation(ll_srs, utm_srs)
        if gdal.GetLastErrorMsg().find("Unable to load PROJ.4") != -1:
            pytest.skip("PROJ.4 missing, transforms not available.")
    except ValueError:
        gdal.PopErrorHandler()
        if gdal.GetLastErrorMsg().find("Unable to load PROJ.4") != -1:
            pytest.skip("PROJ.4 missing, transforms not available.")
        pytest.fail(gdal.GetLastErrorMsg())

    assert not (
        ct is None or ct.this is None
    ), "Unable to create simple CoordinateTransformat."


###############################################################################
# Actually perform a simple LL to UTM conversion.


def test_osr_ct_2():

    utm_srs = osr.SpatialReference()
    utm_srs.SetUTM(11)
    utm_srs.SetWellKnownGeogCS("WGS84")

    ll_srs = osr.SpatialReference()
    ll_srs.SetAxisMappingStrategy(osr.OAMS_AUTHORITY_COMPLIANT)
    ll_srs.SetWellKnownGeogCS("WGS84")

    ct = osr.CoordinateTransformation(ll_srs, utm_srs)

    result = ct.TransformPoint(32.0, -117.5, 0.0)
    assert (
        result[0] == pytest.approx(452772.06, abs=0.01)
        and result[1] == pytest.approx(3540544.89, abs=0.01)
        and result[2] == pytest.approx(0.0, abs=0.01)
    ), "Wrong LL to UTM result"

    result = ct.TransformPoint([32.0, -117.5, 10.0])
    assert (
        result[0] == pytest.approx(452772.06, abs=0.01)
        and result[1] == pytest.approx(3540544.89, abs=0.01)
        and result[2] == 10
    ), "Wrong LL to UTM result"

    result = ct.TransformPoint([32.0, -117.5, 10.0, 2000.0])
    assert (
        result[0] == pytest.approx(452772.06, abs=0.01)
        and result[1] == pytest.approx(3540544.89, abs=0.01)
        and result[2] == 10
        and result[3] == 2000
    ), "Wrong LL to UTM result"


###############################################################################
# Transform an OGR geometry ... this is mostly aimed at ensuring that
# the OGRCoordinateTransformation target SRS isn't deleted till the output
# geometry which also uses it is deleted.


def test_osr_ct_3():

    utm_srs = osr.SpatialReference()
    utm_srs.SetUTM(11)
    utm_srs.SetWellKnownGeogCS("WGS84")

    ll_srs = osr.SpatialReference()
    ll_srs.SetWellKnownGeogCS("WGS84")
    ll_srs.SetAxisMappingStrategy(osr.OAMS_TRADITIONAL_GIS_ORDER)

    ct = osr.CoordinateTransformation(ll_srs, utm_srs)

    pnt = ogr.CreateGeometryFromWkt("POINT(-117.5 32.0)", ll_srs)
    result = pnt.Transform(ct)
    assert result == 0

    ll_srs = None
    ct = None
    utm_srs = None

    out_srs = pnt.GetSpatialReference().ExportToPrettyWkt()
    assert out_srs[0:6] == "PROJCS", "output srs corrupt, ref counting issue?"

    pnt = None


###############################################################################
# Actually perform a simple LL to UTM conversion.
# Works for both OG and NG bindings


def test_osr_ct_4():

    utm_srs = osr.SpatialReference()
    utm_srs.SetUTM(11)
    utm_srs.SetWellKnownGeogCS("WGS84")

    ll_srs = osr.SpatialReference()
    ll_srs.SetWellKnownGeogCS("WGS84")
    ll_srs.SetAxisMappingStrategy(osr.OAMS_TRADITIONAL_GIS_ORDER)

    ct = osr.CoordinateTransformation(ll_srs, utm_srs)

    result = ct.TransformPoints([(-117.5, 32.0, 0.0), (-117.5, 32.0)])
    assert len(result) == 2
    assert len(result[0]) == 3

    for i in range(2):
        assert (
            result[i][0] == pytest.approx(452772.06, abs=0.01)
            and result[i][1] == pytest.approx(3540544.89, abs=0.01)
            and result[i][2] == pytest.approx(0.0, abs=0.01)
        ), "Wrong LL to UTM result"


###############################################################################
# Same test, but with any sequence of tuples instead of a tuple of tuple
# New in NG bindings (#3020)


def test_osr_ct_5():

    utm_srs = osr.SpatialReference()
    utm_srs.SetUTM(11)
    utm_srs.SetWellKnownGeogCS("WGS84")

    ll_srs = osr.SpatialReference()
    ll_srs.SetWellKnownGeogCS("WGS84")
    ll_srs.SetAxisMappingStrategy(osr.OAMS_TRADITIONAL_GIS_ORDER)

    ct = osr.CoordinateTransformation(ll_srs, utm_srs)

    result = ct.TransformPoints(((-117.5, 32.0, 0.0), (-117.5, 32.0)))

    for i in range(2):
        assert (
            result[i][0] == pytest.approx(452772.06, abs=0.01)
            and result[i][1] == pytest.approx(3540544.89, abs=0.01)
            and result[i][2] == pytest.approx(0.0, abs=0.01)
        ), "Wrong LL to UTM result"


###############################################################################
# Test osr.CreateCoordinateTransformation() method


def test_osr_ct_6():

    with pytest.raises(Exception):
        osr.CreateCoordinateTransformation(None, None)

    utm_srs = osr.SpatialReference()
    utm_srs.SetUTM(11)
    utm_srs.SetWellKnownGeogCS("WGS84")

    ll_srs = osr.SpatialReference()
    ll_srs.SetWellKnownGeogCS("WGS84")
    ll_srs.SetAxisMappingStrategy(osr.OAMS_TRADITIONAL_GIS_ORDER)

    ct = osr.CreateCoordinateTransformation(ll_srs, utm_srs)
    assert ct is not None

    result = ct.TransformPoints(((-117.5, 32.0, 0.0), (-117.5, 32.0)))

    for i in range(2):
        assert (
            result[i][0] == pytest.approx(452772.06, abs=0.01)
            and result[i][1] == pytest.approx(3540544.89, abs=0.01)
            and result[i][2] == pytest.approx(0.0, abs=0.01)
        ), "Wrong LL to UTM result"


###############################################################################
# Actually perform a simple Pseudo Mercator to LL conversion.


def test_osr_ct_7():

    pm_srs = osr.SpatialReference()
    pm_srs.ImportFromEPSG(3857)

    ll_srs = osr.SpatialReference()
    ll_srs.SetWellKnownGeogCS("WGS84")
    ll_srs.SetAxisMappingStrategy(osr.OAMS_TRADITIONAL_GIS_ORDER)

    ct = osr.CoordinateTransformation(pm_srs, ll_srs)

    x, y, z = ct.TransformPoint(7000000, 7000000, 0)
    exp_x, exp_y, exp_z = (62.8820698884, 53.0918187696, 0.0)
    if (
        exp_x != pytest.approx(x, abs=0.00001)
        or exp_y != pytest.approx(y, abs=0.00001)
        or exp_z != pytest.approx(z, abs=0.00001)
    ):
        print("Got:      (%f, %f, %f)" % (x, y, z))
        print("Expected: (%f, %f, %f)" % (exp_x, exp_y, exp_z))
        pytest.fail("Wrong LL for Pseudo Mercator result")

    pnt = ogr.CreateGeometryFromWkt("POINT(%g %g)" % (7000000, 7000000), pm_srs)
    expected_pnt = ogr.CreateGeometryFromWkt(
        "POINT(%.10f %.10f)" % (exp_x, exp_y), ll_srs
    )
    result = pnt.Transform(ct)
    assert result == 0
    if (
        expected_pnt.GetX() != pytest.approx(pnt.GetX(), abs=0.00001)
        or expected_pnt.GetY() != pytest.approx(pnt.GetY(), abs=0.00001)
        or expected_pnt.GetZ() != pytest.approx(pnt.GetZ(), abs=0.00001)
    ):
        print("Got:      %s" % pnt.ExportToWkt())
        print("Expected: %s" % expected_pnt.ExportToWkt())
        pytest.fail("Failed to transform from Pseudo Mercator to LL")


###############################################################################
# Test WebMercator -> WGS84 optimized transform


def test_osr_ct_8():

    src_srs = osr.SpatialReference()
    src_srs.ImportFromEPSG(3857)

    dst_srs = osr.SpatialReference()
    dst_srs.SetWellKnownGeogCS("WGS84")
    dst_srs.SetAxisMappingStrategy(osr.OAMS_TRADITIONAL_GIS_ORDER)

    ct = osr.CoordinateTransformation(src_srs, dst_srs)

    pnts = [(0, 6274861.39400658), (1, 6274861.39400658)]
    result = ct.TransformPoints(pnts)
    expected_result = [
        (0.0, 49.000000000000007, 0.0),
        (8.9831528411952125e-06, 49.000000000000007, 0.0),
    ]

    for i in range(2):
        for j in range(3):
            if result[i][j] != pytest.approx(expected_result[i][j], abs=1e-10):
                print("Got:      %s" % str(result))
                print("Expected: %s" % str(expected_result))
                pytest.fail("Failed to transform from Pseudo Mercator to LL")

    pnts = [(0, 6274861.39400658), (1 + 0, 1 + 6274861.39400658)]
    result = ct.TransformPoints(pnts)
    expected_result = [
        (0.0, 49.000000000000007, 0.0),
        (8.9831528411952125e-06, 49.000005893478189, 0.0),
    ]

    for i in range(2):
        for j in range(3):
            if result[i][j] != pytest.approx(expected_result[i][j], abs=1e-10):
                print("Got:      %s" % str(result))
                print("Expected: %s" % str(expected_result))
                pytest.fail("Failed to transform from Pseudo Mercator to LL")


###############################################################################
# Test coordinate transformation where only one CRS has a towgs84 clause (#1156)


def test_osr_ct_towgs84_only_one_side():

    srs_towgs84 = osr.SpatialReference()
    srs_towgs84.SetFromUserInput("+proj=longlat +ellps=GRS80 +towgs84=100,200,300")

    srs_just_ellps = osr.SpatialReference()
    srs_just_ellps.SetFromUserInput("+proj=longlat +ellps=GRS80")

    ct = osr.CoordinateTransformation(srs_towgs84, srs_just_ellps)
    x, y, z = ct.TransformPoint(0, 0, 0)
    assert x == 0
    assert y == 0
    assert z == 0

    ct = osr.CoordinateTransformation(srs_just_ellps, srs_towgs84)
    x, y, z = ct.TransformPoint(0, 0, 0)
    assert x == 0
    assert y == 0
    assert z == 0


###############################################################################
# Test coordinate transformation where both side have towgs84/datum clause (#1156)


def test_osr_ct_towgs84_both_side():

    srs_towgs84 = osr.SpatialReference()
    srs_towgs84.SetFromUserInput("+proj=longlat +ellps=GRS80 +towgs84=100,200,300")

    srs_other_towgs84 = osr.SpatialReference()
    srs_other_towgs84.SetFromUserInput("+proj=longlat +ellps=GRS80 +towgs84=0,0,0")

    ct = osr.CoordinateTransformation(srs_towgs84, srs_other_towgs84)
    x, y, z = ct.TransformPoint(0, 0, 20)
    assert x != 0
    assert y != 0
    assert z == 20

    srs_datum_wgs84 = osr.SpatialReference()
    srs_datum_wgs84.SetFromUserInput("+proj=longlat +datum=WGS84")

    ct = osr.CoordinateTransformation(srs_towgs84, srs_datum_wgs84)
    x, y, z = ct.TransformPoint(0, 0, 20)
    assert x != 0
    assert y != 0
    assert z == 20

    ct = osr.CoordinateTransformation(srs_datum_wgs84, srs_towgs84)
    x, y, z = ct.TransformPoint(0, 0, 20)
    assert x != 0
    assert y != 0
    assert z == 20


###############################################################################
# Test coordinate transformation with custom operation


def test_osr_ct_options_operation():

    options = osr.CoordinateTransformationOptions()
    assert options.SetOperation("+proj=affine +xoff=1")
    ct = osr.CoordinateTransformation(None, None, options)
    assert ct
    x, y, z = ct.TransformPoint(1, 2, 3)
    assert x == 2
    assert y == 2
    assert z == 3

    ct_inverse = ct.GetInverse()
    x, y, z = ct_inverse.TransformPoint(1, 2, 3)
    assert x == 0
    assert y == 2
    assert z == 3

    options = osr.CoordinateTransformationOptions()
    # inverse coordinate operation
    assert options.SetOperation("+proj=affine +xoff=1", True)
    ct = osr.CoordinateTransformation(None, None, options)
    assert ct
    x, y, z = ct.TransformPoint(1, 2, 3)
    assert x == 0
    assert y == 2
    assert z == 3


###############################################################################
# Test coordinate transformation with area of interest


def test_osr_ct_options_area_of_interest():

    srs_nad27 = osr.SpatialReference()
    srs_nad27.SetFromUserInput("NAD27")
    srs_nad27.SetAxisMappingStrategy(osr.OAMS_AUTHORITY_COMPLIANT)
    srs_wgs84 = osr.SpatialReference()
    srs_wgs84.SetFromUserInput("WGS84")
    srs_wgs84.SetAxisMappingStrategy(osr.OAMS_AUTHORITY_COMPLIANT)
    options = osr.CoordinateTransformationOptions()
    with pytest.raises(Exception):
        options.SetAreaOfInterest(-200, 40, -99, 41)
    with pytest.raises(Exception):
        options.SetAreaOfInterest(-100, -100, -99, 41)
    with pytest.raises(Exception):
        options.SetAreaOfInterest(-100, 40, 200, 41)
    with pytest.raises(Exception):
        options.SetAreaOfInterest(-100, 40, -99, 100)
    assert options.SetAreaOfInterest(-100, 40, -99, 41)
    ct = osr.CoordinateTransformation(srs_nad27, srs_wgs84, options)
    assert ct

    x, y, z = ct.TransformPoint(40.5, -99.5, 0)
    assert x != 40.5
    assert x == pytest.approx(40.5, abs=1e-3)


###############################################################################
# Test 4D transformations


@pytest.mark.parametrize("source_crs", [None, "EPSG:4326"])  # random code (not used)
@pytest.mark.parametrize("target_crs", [None, "EPSG:4326"])  # random code (not used)
def test_osr_ct_4D(source_crs, target_crs):

    options = osr.CoordinateTransformationOptions()
    assert options.SetOperation(
        "+proj=pipeline +step +proj=unitconvert +xy_in=deg +xy_out=rad +step +proj=cart +step +proj=helmert +convention=position_vector +x=0.0127 +dx=-0.0029 +rx=-0.00039 +drx=-0.00011 +y=0.0065 +dy=-0.0002 +ry=0.00080 +dry=-0.00019 +z=-0.0209 +dz=-0.0006 +rz=-0.00114 +drz=0.00007 +s=0.00195 +ds=0.00001 +t_epoch=1988.0 +step +proj=cart +inv +step +proj=unitconvert +xy_in=rad +xy_out=deg"
    )
    if source_crs:
        srs = osr.SpatialReference()
        srs.SetFromUserInput(source_crs)
        source_crs = srs
    if target_crs:
        srs = osr.SpatialReference()
        srs.SetFromUserInput(target_crs)
        target_crs = srs
    ct = osr.CoordinateTransformation(source_crs, target_crs, options)
    assert ct

    x, y, z, t = ct.TransformPoint(2, 49, 0, 2000)
    assert x == pytest.approx(2.0000005420366, abs=1e-10), x
    assert y == pytest.approx(49.0000003766711, abs=1e-10), y
    assert z == pytest.approx(-0.0222802283242345, abs=1e-8), z
    assert t == pytest.approx(2000, abs=1e-10), t

    ret = ct.TransformPoints([[2, 49, 0, 2000], [2, 49, 0, 1988]])
    assert len(ret) == 2, ret

    assert len(ret[0]) == 4, ret
    x, y, z, t = ret[0]
    assert x == pytest.approx(2.0000005420366, abs=1e-10), x
    assert y == pytest.approx(49.0000003766711, abs=1e-10), y
    assert z == pytest.approx(-0.0222802283242345, abs=1e-8), z
    assert t == pytest.approx(2000, abs=1e-10), t

    assert len(ret[1]) == 4, ret
    x, y, z, t = ret[1]
    assert x == pytest.approx(1.9999998809056305, abs=1e-10), x
    assert y == pytest.approx(48.9999995630005, abs=1e-10), y
    assert z == pytest.approx(0.005032399669289589, abs=1e-8), z
    assert t == pytest.approx(1988, abs=1e-10), t


###############################################################################
# Test geocentric transformations


def test_osr_ct_geocentric():

    s = osr.SpatialReference()
    s.SetFromUserInput("IGNF:RGR92")
    t = osr.SpatialReference()
    t.SetFromUserInput("IGNF:REUN47")
    ct = osr.CoordinateTransformation(s, t)
    assert ct

    x, y, z = ct.TransformPoint(3356123.5400, 1303218.3090, 5247430.6050)
    assert x == pytest.approx(3353420.949, abs=1e-1)
    assert y == pytest.approx(1304075.021, abs=1e-1)
    assert z == pytest.approx(5248935.144, abs=1e-1)


###############################################################################
# Test with +lon_wrap=180


@pytest.mark.require_proj(7, 0, 1)
def test_osr_ct_lon_wrap():

    s = osr.SpatialReference()
    s.SetFromUserInput("+proj=longlat +ellps=GRS80")
    t = osr.SpatialReference()
    t.SetFromUserInput("+proj=longlat +ellps=GRS80 +lon_wrap=180")
    ct = osr.CoordinateTransformation(s, t)
    assert ct

    x, y, _ = ct.TransformPoint(-25, 60, 0)
    assert x == pytest.approx(-25 + 360, abs=1e-12)
    assert y == pytest.approx(60, abs=1e-12)


###############################################################################
# Test ct.TransformPointWithErrorCode


@pytest.mark.require_proj(8)
def test_osr_ct_transformpointwitherrorcode():

    s = osr.SpatialReference()
    s.SetFromUserInput("+proj=longlat +ellps=GRS80")
    t = osr.SpatialReference()
    t.SetFromUserInput("+proj=tmerc +ellps=GRS80")
    ct = osr.CoordinateTransformation(s, t)
    assert ct

    x, y, z, t, error_code = ct.TransformPointWithErrorCode(1, 2, 3, 4)
    assert x == pytest.approx(111257.80439304397, rel=1e-10)
    assert y == pytest.approx(221183.3401672801, rel=1e-10)
    assert z == 3
    assert t == 4
    assert error_code == 0

    with pytest.raises(Exception):
        ct.TransformPointWithErrorCode(90, 0, 0, 0)

    with osr.ExceptionMgr(useExceptions=False):
        x, y, z, t, error_code = ct.TransformPointWithErrorCode(90, 0, 0, 0)

    assert math.isinf(x)
    assert error_code == osr.PROJ_ERR_COORD_TRANSFM_OUTSIDE_PROJECTION_DOMAIN


###############################################################################
# Test CoordinateTransformationOptions.SetDesiredAccuracy


def test_osr_ct_options_accuracy():

    s = osr.SpatialReference()
    s.SetFromUserInput("EPSG:4326")
    t = osr.SpatialReference()
    t.SetFromUserInput("EPSG:4258")  # ETRS89
    options = osr.CoordinateTransformationOptions()
    options.SetDesiredAccuracy(0.05)
    with gdal.quiet_errors():
        with osr.ExceptionMgr(useExceptions=False):
            ct = osr.CoordinateTransformation(s, t, options)
    with pytest.raises(Exception):
        ct.TransformPoint(49, 2, 0)


###############################################################################
# Test CoordinateTransformationOptions.SetBallparkAllowed


def test_osr_ct_options_ballpark_disallowed():

    s = osr.SpatialReference()
    s.SetFromUserInput("EPSG:4267")  # NAD27
    t = osr.SpatialReference()
    t.SetFromUserInput("EPSG:4258")  # ETRS89
    options = osr.CoordinateTransformationOptions()
    options.SetBallparkAllowed(False)
    with gdal.quiet_errors():
        with osr.ExceptionMgr(useExceptions=False):
            ct = osr.CoordinateTransformation(s, t, options)
    with pytest.raises(Exception):
        ct.TransformPoint(49, 2, 0)


###############################################################################
# Test CoordinateTransformationOptions.SetOnlyBest


@pytest.mark.require_proj(9, 2)
def test_osr_ct_options_only_best_enabled():

    s = osr.SpatialReference()
    s.SetFromUserInput("EPSG:4746")  # PD/83
    t = osr.SpatialReference()
    t.SetFromUserInput("EPSG:4326")  # WGS 84

    # Best operation normally uses the BETA2007 grid
    options = osr.CoordinateTransformationOptions()
    options.SetOnlyBest(True)
    ct = osr.CoordinateTransformation(s, t, options)
    with pytest.raises(Exception, match=r"Grid de_adv_BETA2007.tif is not available"):
        ct.TransformPoint(50.5, 10, 0)

    options = osr.CoordinateTransformationOptions()
    options.SetOnlyBest(False)
    ct = osr.CoordinateTransformation(s, t, options)
    # Ballpark operation
    assert ct.TransformPoint(50.5, 10, 0) == pytest.approx(
        (50.498804120408145, 9.99880336687919, 0.0)
    )


###############################################################################
# Test that we pass a neutral time when not explicitly specified


def test_osr_ct_non_specified_time_with_time_dependent_transformation():

    options = osr.CoordinateTransformationOptions()
    options.SetOperation(
        "+proj=pipeline +step +proj=axisswap +order=2,1 +step +proj=unitconvert +xy_in=deg +z_in=m +xy_out=rad +z_out=m +step +proj=cart +ellps=GRS80 +step +inv +proj=helmert +dx=0.0008 +dy=-0.0006 +dz=-0.0014 +drx=6.67e-05 +dry=-0.0007574 +drz=-5.13e-05 +ds=-7e-05 +t_epoch=2010 +convention=coordinate_frame +step +inv +proj=cart +ellps=GRS80 +step +proj=unitconvert +xy_in=rad +z_in=m +xy_out=deg +z_out=m +step +proj=axisswap +order=2,1"
    )
    ct = osr.CoordinateTransformation(None, None, options)
    assert ct
    x, y, _ = ct.TransformPoint(50, -40, 0)
    assert x == pytest.approx(50, abs=1e-10)
    assert y == pytest.approx(-40, abs=1e-10)


###############################################################################
# Test using OGRSpatialReference::CoordinateEpoch()


@pytest.mark.require_proj(7, 2)
def test_osr_ct_take_into_account_srs_coordinate_epoch():

    s = osr.SpatialReference()
    s.SetFromUserInput("EPSG:7844")  # GDA2020
    s.SetAxisMappingStrategy(osr.OAMS_AUTHORITY_COMPLIANT)

    t_2020 = osr.SpatialReference()
    t_2020.SetAxisMappingStrategy(osr.OAMS_AUTHORITY_COMPLIANT)
    t_2020.SetFromUserInput("EPSG:9000")  # ITRF2014
    t_2020.SetCoordinateEpoch(2020)

    # 2020 is the central epoch of the transformation, so no coordinate
    # change is expected
    ct = osr.CoordinateTransformation(s, t_2020)
    x, y, _ = ct.TransformPoint(-30, 150, 0)
    assert x == pytest.approx(-30, abs=1e-10)
    assert y == pytest.approx(150, abs=1e-10)

    t_2030 = osr.SpatialReference()
    t_2030.SetAxisMappingStrategy(osr.OAMS_AUTHORITY_COMPLIANT)
    t_2030.SetFromUserInput("EPSG:9000")  # ITRF2014
    t_2030.SetCoordinateEpoch(2030)

    ct = osr.CoordinateTransformation(s, t_2030)
    x, y, _ = ct.TransformPoint(-30, 150, 0)
    assert x == pytest.approx(-29.9999950478, abs=1e-10)
    assert y == pytest.approx(150.0000022212, abs=1e-10)

    ct = osr.CoordinateTransformation(t_2030, s)
    x, y, _ = ct.TransformPoint(-29.9999950478, 150.0000022212, 0)
    assert x == pytest.approx(-30, abs=1e-10)
    assert y == pytest.approx(150, abs=1e-10)

    # Not properly supported currently
    with gdal.quiet_errors():
        osr.CoordinateTransformation(t_2020, t_2030)


###############################################################################
# Test transformation between CRS that only differ by axis order


def test_osr_ct_only_axis_order_different():

    s_wrong_axis_order = osr.SpatialReference()
    s_wrong_axis_order.SetFromUserInput("""GEOGCS["WGS 84",
    DATUM["WGS_1984",
        SPHEROID["WGS 84",6378137,298.257223563,
            AUTHORITY["EPSG","7030"]],
        AUTHORITY["EPSG","6326"]],
    PRIMEM["Greenwich",0,
        AUTHORITY["EPSG","8901"]],
    UNIT["degree",0.0174532925199433,
        AUTHORITY["EPSG","9122"]],
    AXIS["Longitude",EAST],
    AXIS["Latitude",NORTH]]""")

    t = osr.SpatialReference()
    t.ImportFromEPSG(4326)
    t.SetAxisMappingStrategy(osr.OAMS_AUTHORITY_COMPLIANT)

    ct = osr.CoordinateTransformation(s_wrong_axis_order, t)
    x, y, _ = ct.TransformPoint(2, 49, 0)
    assert x == 49
    assert y == 2


###############################################################################
# Test transformation for a CRS whose definition contradicts the one of the
# authority. NOTE: it is arguable that this is the correct behaviour. One
# could consider that the AUTHORITY would have precedence.


def test_osr_ct_wkt_non_consistent_with_epsg_definition():

    s_wrong_axis_order = osr.SpatialReference()
    s_wrong_axis_order.SetFromUserInput("""GEOGCS["WGS 84",
    DATUM["WGS_1984",
        SPHEROID["WGS 84",6378137,298.257223563,
            AUTHORITY["EPSG","7030"]],
        AUTHORITY["EPSG","6326"]],
    PRIMEM["Greenwich",0,
        AUTHORITY["EPSG","8901"]],
    UNIT["degree",0.0174532925199433,
        AUTHORITY["EPSG","9122"]],
    AXIS["Longitude",EAST],
    AXIS["Latitude",NORTH],
    AUTHORITY["EPSG","4326"]]""")

    t = osr.SpatialReference()
    t.ImportFromEPSG(4326)
    t.SetAxisMappingStrategy(osr.OAMS_AUTHORITY_COMPLIANT)

    ct = osr.CoordinateTransformation(s_wrong_axis_order, t)
    x, y, _ = ct.TransformPoint(2, 49, 0)
    assert x == 49
    assert y == 2


###############################################################################
# Test effect of OGR_CT_PREFER_OFFICIAL_SRS_DEF=NO
# https://github.com/OSGeo/PROJ/issues/2955


@pytest.mark.require_proj(7, 2)
def test_osr_ct_OGR_CT_PREFER_OFFICIAL_SRS_DEF():

    # Not sure about the minimal version, but works as expected with 7.2.1

    wkt = 'PROJCS["OSGB 1936 / British National Grid",GEOGCS["OSGB 1936",DATUM["OSGB_1936",SPHEROID["Airy 1830",6377563.396,299.3249646,AUTHORITY["EPSG","7001"]],TOWGS84[446.448,-125.157,542.06,0.15,0.247,0.842,-20.489],AUTHORITY["EPSG","6277"]],PRIMEM["Greenwich",0,AUTHORITY["EPSG","8901"]],UNIT["degree",0.0174532925199433,AUTHORITY["EPSG","9122"]],AUTHORITY["EPSG","4277"]],PROJECTION["Transverse_Mercator"],PARAMETER["latitude_of_origin",49],PARAMETER["central_meridian",-2],PARAMETER["scale_factor",0.9996012717],PARAMETER["false_easting",400000],PARAMETER["false_northing",-100000],UNIT["metre",1,AUTHORITY["EPSG","9001"]],AXIS["Easting",EAST],AXIS["Northing",NORTH],AUTHORITY["EPSG","27700"]]'
    s = osr.SpatialReference()
    s.SetFromUserInput(wkt)

    t = osr.SpatialReference()
    t.SetAxisMappingStrategy(osr.OAMS_TRADITIONAL_GIS_ORDER)
    t.ImportFromEPSG(4258)  # ETRS 89

    # No datum shift
    ct = osr.CoordinateTransformation(s, t)
    x, y, _ = ct.TransformPoint(826158.063, 2405844.125, 0)
    assert abs(x - 9.873) < 0.001, x
    assert abs(y - 71.127) < 0.001, y

    # Datum shift implied by the TOWGS4 clause
    with gdaltest.config_option("OGR_CT_PREFER_OFFICIAL_SRS_DEF", "NO"):
        ct = osr.CoordinateTransformation(s, t)
        x, y, _ = ct.TransformPoint(826158.063, 2405844.125, 0)
        assert abs(x - 9.867) < 0.001, x
        assert abs(y - 71.125) < 0.001, y


###############################################################################
# Test NAD83(CSRS)v7 change of epoch


@pytest.mark.require_proj(9, 4)
def test_osr_ct_point_motion_operation():

    s = osr.SpatialReference()
    s.ImportFromEPSG(8254)  # NAD83(CSRS)v7 3D
    s.SetCoordinateEpoch(2002)

    t = osr.SpatialReference()
    t.ImportFromEPSG(8254)  # NAD83(CSRS)v7 3D
    t.SetCoordinateEpoch(2010)

    ct = osr.CoordinateTransformation(s, t)
    x, y, z = ct.TransformPoint(60.5, -79.5)
    assert abs(x - 60.49999994) < 1e-8, x
    assert abs(y - -79.49999963) < 1e-8, y
    assert abs(z - 0.060) < 1e-3, z

    t = osr.SpatialReference()
    t.ImportFromEPSG(22717)  # "NAD83(CSRS)v7 / UTM zone 17N"
    t.SetCoordinateEpoch(2010)

    ct = osr.CoordinateTransformation(s, t)
    x, y, z = ct.TransformPoint(60.5, -79.5)
    assert abs(x - 582395.993) < 1e-3, x
    assert abs(y - 6708035.973) < 1e-3, y
    assert abs(z - 0.060) < 1e-3, z


###############################################################################
# Test effect of SetDataAxisToSRSAxisMapping([1,2,-3])


def test_osr_ct_source_z_axis_reversal():

    s = osr.SpatialReference()
    s.ImportFromEPSG(4979)
    s.SetDataAxisToSRSAxisMapping([1, 2, -3])

    t = osr.SpatialReference()
    t.ImportFromEPSG(4979)

    ct = osr.CoordinateTransformation(s, t)
    x, y, z = ct.TransformPoint(1, 2, -3)
    assert x == pytest.approx(1)
    assert y == pytest.approx(2)
    assert z == pytest.approx(3)


###############################################################################
# Test effect of SetDataAxisToSRSAxisMapping([1,2,-3])


def test_osr_ct_target_z_axis_reversal():

    s = osr.SpatialReference()
    s.ImportFromEPSG(4979)

    t = osr.SpatialReference()
    t.ImportFromEPSG(4979)
    t.SetDataAxisToSRSAxisMapping([1, 2, -3])

    ct = osr.CoordinateTransformation(s, t)
    x, y, z = ct.TransformPoint(1, 2, -3)
    assert x == pytest.approx(1)
    assert y == pytest.approx(2)
    assert z == pytest.approx(3)


###############################################################################
# Test effect of SetDataAxisToSRSAxisMapping([-2,1])


def test_osr_ct_source_mapping_minus_two_one():

    s = osr.SpatialReference()
    s.ImportFromEPSG(4326)
    s.SetDataAxisToSRSAxisMapping([-2, 1])

    t = osr.SpatialReference()
    t.ImportFromEPSG(4326)

    ct = osr.CoordinateTransformation(s, t)
    x, y, _ = ct.TransformPoint(10, 20, 0)
    assert x == pytest.approx(-20)
    assert y == pytest.approx(10)


###############################################################################
# Test effect of SetDataAxisToSRSAxisMapping([-2,1])


def test_osr_ct_target_mapping_minus_two_one():

    s = osr.SpatialReference()
    s.ImportFromEPSG(4326)

    t = osr.SpatialReference()
    t.ImportFromEPSG(4326)
    t.SetDataAxisToSRSAxisMapping([-2, 1])

    ct = osr.CoordinateTransformation(s, t)
    x, y, _ = ct.TransformPoint(-20, 10, 0)
    assert x == pytest.approx(10)
    assert y == pytest.approx(20)


###############################################################################
# Test effect of SetDataAxisToSRSAxisMapping([-2,1])


def test_osr_ct_source_and_target_mapping_minus_two_one():

    s = osr.SpatialReference()
    s.ImportFromEPSG(4326)
    s.SetDataAxisToSRSAxisMapping([-2, 1])

    t = osr.SpatialReference()
    t.ImportFromEPSG(4326)
    t.SetDataAxisToSRSAxisMapping([-2, 1])

    ct = osr.CoordinateTransformation(s, t)
    x, y, _ = ct.TransformPoint(10, 20, 0)
    assert x == pytest.approx(10)
    assert y == pytest.approx(20)


###############################################################################
# Test bug fix for https://github.com/OSGeo/gdal/issues/9732


@pytest.mark.require_proj(9, 2)
def test_osr_ct_fix_9732():

    s = osr.SpatialReference()
    s.ImportFromEPSG(6453)

    assert not s.IsDerivedProjected()

    t = osr.SpatialReference()
    t.SetFromUserInput("""DERIVEDPROJCRS["Ground for NAD83(2011) / Idaho West (ftUS)",
    BASEPROJCRS["NAD83(2011) / Idaho West (ftUS)",
        BASEGEOGCRS["NAD83(2011)",
            DATUM["NAD83 (National Spatial Reference System 2011)",ELLIPSOID["GRS 1980",6378137,298.257222101,LENGTHUNIT["metre",1]]],
            PRIMEM["Greenwich",0,ANGLEUNIT["degree",0.0174532925199433]],
            ID["EPSG",6318]],
        CONVERSION["SPCS83 Idaho West zone (US Survey feet)",
            METHOD["Transverse Mercator",ID["EPSG",9807]],
            PARAMETER["Latitude of natural origin",41.6666666666667,ANGLEUNIT["degree",0.0174532925199433],ID["EPSG",8801]],
            PARAMETER["Longitude of natural origin",-115.75,ANGLEUNIT["degree",0.0174532925199433],ID["EPSG",8802]],
            PARAMETER["Scale factor at natural origin",0.999933333,SCALEUNIT["unity",1],ID["EPSG",8805]],
            PARAMETER["False easting",2624666.667,LENGTHUNIT["US survey foot",0.304800609601219],ID["EPSG",8806]],
            PARAMETER["False northing",0,LENGTHUNIT["US survey foot",0.304800609601219],ID["EPSG",8807]]],
        CS[Cartesian,2],
        AXIS["easting (X)",east,ORDER[1],LENGTHUNIT["US survey foot",0.304800609601219]],
        AXIS["northing (Y)",north,ORDER[2],LENGTHUNIT["US survey foot",0.304800609601219]],
        USAGE[SCOPE["Engineering survey, topographic mapping."],AREA["United States (USA) - Idaho - counties of Ada; Adams; Benewah; Boise; Bonner; Boundary; Canyon; Clearwater; Elmore; Gem; Idaho; Kootenai; Latah; Lewis; Nez Perce; Owyhee; Payette; Shoshone; Valley; Washington."],BBOX[41.99,-117.24,49.01,-114.32]],
        ID["EPSG",6453]],
    DERIVINGCONVERSION["Grid to ground",
        METHOD["Similarity transformation",ID["EPSG",9621]],
        PARAMETER["Ordinate 1 of evaluation point in target CRS",1000,LENGTHUNIT["US survey foot",0.304800609601219],ID["EPSG",8621]],
        PARAMETER["Ordinate 2 of evaluation point in target CRS",0,LENGTHUNIT["US survey foot",0.304800609601219],ID["EPSG",8622]],
        PARAMETER["Scale factor for source CRS axes",1,SCALEUNIT["unity",1],ID["EPSG",1061]],
        PARAMETER["Rotation angle of source CRS axes", 0,ANGLEUNIT["degree",0.0],ID["EPSG",8614]]],
    CS[Cartesian,2],
    AXIS["easting (X)",east,
        ORDER[1],
        LENGTHUNIT["US survey foot",0.304800609601219]],
    AXIS["northing (Y)",north,
        ORDER[2],
        LENGTHUNIT["US survey foot",0.304800609601219]]]""")

    assert t.IsDerivedProjected()

    ct = osr.CoordinateTransformation(s, t)
    x, y, _ = ct.TransformPoint(2300000, 2000000, 0)
    assert x == pytest.approx(2301000)
    assert y == pytest.approx(2000000)


def test_osr_ct_one_crs_has_a_esri_epsg_code():

    s = osr.SpatialReference()
    s.ImportFromEPSG(4269)  # NAD83
    s.SetAxisMappingStrategy(osr.OAMS_TRADITIONAL_GIS_ORDER)

    t = osr.SpatialReference()
    t.SetFromUserInput(
        """PROJCS["NAD_1983_StatePlane_California_I_FIPS_0401_Feet",GEOGCS["GCS_North_American_1983",DATUM["North_American_Datum_1983",
SPHEROID["GRS_1980",6378137,298.257222101]],PRIMEM["Greenwich",0],UNIT["Degree",0.017453292519943295]],PROJECTION["Lambert_Conformal_Conic_2SP"],
PARAMETER["False_Easting",6561666.666666666],PARAMETER["False_Northing",1640416.666666667],PARAMETER["Central_Meridian",-122],
PARAMETER["Standard_Parallel_1",40],PARAMETER["Standard_Parallel_2",41.66666666666666],PARAMETER["Latitude_Of_Origin",39.33333333333334],
UNIT["Foot_US",0.30480060960121924],AUTHORITY["EPSG","102641"]]"""
    )

    ct = osr.CoordinateTransformation(s, t)
    x, y, _ = ct.TransformPoint(-122, 39.3333333333333, 0)
    assert x == pytest.approx(6561666.667)
    assert y == pytest.approx(1640416.667)
