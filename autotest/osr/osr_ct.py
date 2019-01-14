#!/usr/bin/env pytest
# -*- coding: utf-8 -*-
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test coordinate transformations.
# Author:   Frank Warmerdam <warmerdam@pobox.com>
#
###############################################################################
# Copyright (c) 2003, Frank Warmerdam <warmerdam@pobox.com>
# Copyright (c) 2009-2013, Even Rouault <even dot rouault at mines-paris dot org>
# Copyright (c) 2014, Google
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



from osgeo import gdal
from osgeo import osr
from osgeo import ogr
import gdaltest
import pytest


###############################################################################
# Verify that we have PROJ.4 available.

def test_osr_ct_1():

    gdaltest.have_proj4 = 0

    utm_srs = osr.SpatialReference()
    utm_srs.SetUTM(11)
    utm_srs.SetWellKnownGeogCS('WGS84')

    ll_srs = osr.SpatialReference()
    ll_srs.SetWellKnownGeogCS('WGS84')

    try:
        gdal.PushErrorHandler('CPLQuietErrorHandler')
        ct = osr.CoordinateTransformation(ll_srs, utm_srs)
        gdal.PopErrorHandler()
        if gdal.GetLastErrorMsg().find('Unable to load PROJ.4') != -1:
            pytest.skip('PROJ.4 missing, transforms not available.')
    except ValueError:
        gdal.PopErrorHandler()
        if gdal.GetLastErrorMsg().find('Unable to load PROJ.4') != -1:
            pytest.skip('PROJ.4 missing, transforms not available.')
        pytest.fail(gdal.GetLastErrorMsg())

    assert not (ct is None or ct.this is None), \
        'Unable to create simple CoordinateTransformat.'

    gdaltest.have_proj4 = 1

###############################################################################
# Actually perform a simple LL to UTM conversion.


def test_osr_ct_2():

    if gdaltest.have_proj4 == 0:
        pytest.skip()

    utm_srs = osr.SpatialReference()
    utm_srs.SetUTM(11)
    utm_srs.SetWellKnownGeogCS('WGS84')

    ll_srs = osr.SpatialReference()
    ll_srs.SetWellKnownGeogCS('WGS84')

    gdaltest.ct = osr.CoordinateTransformation(ll_srs, utm_srs)

    result = gdaltest.ct.TransformPoint(-117.5, 32.0, 0.0)
    assert abs(result[0] - 452772.06) <= 0.01 and abs(result[1] - 3540544.89) <= 0.01 and abs(result[2] - 0.0) <= 0.01, \
        'Wrong LL to UTM result'

###############################################################################
# Transform an OGR geometry ... this is mostly aimed at ensuring that
# the OGRCoordinateTransformation target SRS isn't deleted till the output
# geometry which also uses it is deleted.


def test_osr_ct_3():

    if gdaltest.have_proj4 == 0:
        pytest.skip()

    utm_srs = osr.SpatialReference()
    utm_srs.SetUTM(11)
    utm_srs.SetWellKnownGeogCS('WGS84')

    ll_srs = osr.SpatialReference()
    ll_srs.SetWellKnownGeogCS('WGS84')

    ct = osr.CoordinateTransformation(ll_srs, utm_srs)

    pnt = ogr.CreateGeometryFromWkt('POINT(-117.5 32.0)', ll_srs)
    result = pnt.Transform(ct)
    assert result == 0

    ll_srs = None
    ct = None
    utm_srs = None

    out_srs = pnt.GetSpatialReference().ExportToPrettyWkt()
    assert out_srs[0:6] == 'PROJCS', 'output srs corrupt, ref counting issue?'

    pnt = None

###############################################################################
# Actually perform a simple LL to UTM conversion.
# Works for both OG and NG bindings


def test_osr_ct_4():

    if gdaltest.have_proj4 == 0:
        pytest.skip()

    utm_srs = osr.SpatialReference()
    utm_srs.SetUTM(11)
    utm_srs.SetWellKnownGeogCS('WGS84')

    ll_srs = osr.SpatialReference()
    ll_srs.SetWellKnownGeogCS('WGS84')

    gdaltest.ct = osr.CoordinateTransformation(ll_srs, utm_srs)

    result = gdaltest.ct.TransformPoints([(-117.5, 32.0, 0.0), (-117.5, 32.0)])

    for i in range(2):
        assert abs(result[i][0] - 452772.06) <= 0.01 and abs(result[i][1] - 3540544.89) <= 0.01 and abs(result[i][2] - 0.0) <= 0.01, \
            'Wrong LL to UTM result'

    
###############################################################################
# Same test, but with any sequence of tuples instead of a tuple of tuple
# New in NG bindings (#3020)


def test_osr_ct_5():

    if gdaltest.have_proj4 == 0:
        pytest.skip()

    utm_srs = osr.SpatialReference()
    utm_srs.SetUTM(11)
    utm_srs.SetWellKnownGeogCS('WGS84')

    ll_srs = osr.SpatialReference()
    ll_srs.SetWellKnownGeogCS('WGS84')

    gdaltest.ct = osr.CoordinateTransformation(ll_srs, utm_srs)

    result = gdaltest.ct.TransformPoints(((-117.5, 32.0, 0.0), (-117.5, 32.0)))

    for i in range(2):
        assert abs(result[i][0] - 452772.06) <= 0.01 and abs(result[i][1] - 3540544.89) <= 0.01 and abs(result[i][2] - 0.0) <= 0.01, \
            'Wrong LL to UTM result'

    
###############################################################################
# Test osr.CreateCoordinateTransformation() method


def test_osr_ct_6():

    if gdaltest.have_proj4 == 0:
        pytest.skip()

    ct = osr.CreateCoordinateTransformation(None, None)
    assert ct is None

    utm_srs = osr.SpatialReference()
    utm_srs.SetUTM(11)
    utm_srs.SetWellKnownGeogCS('WGS84')

    ll_srs = osr.SpatialReference()
    ll_srs.SetWellKnownGeogCS('WGS84')

    ct = osr.CreateCoordinateTransformation(ll_srs, utm_srs)
    assert ct is not None

    result = ct.TransformPoints(((-117.5, 32.0, 0.0), (-117.5, 32.0)))

    for i in range(2):
        assert abs(result[i][0] - 452772.06) <= 0.01 and abs(result[i][1] - 3540544.89) <= 0.01 and abs(result[i][2] - 0.0) <= 0.01, \
            'Wrong LL to UTM result'

    
###############################################################################
# Actually perform a simple Pseudo Mercator to LL conversion.


def test_osr_ct_7():

    if gdaltest.have_proj4 == 0:
        pytest.skip()

    pm_srs = osr.SpatialReference()
    pm_srs.ImportFromEPSG(3857)

    ll_srs = osr.SpatialReference()
    ll_srs.SetWellKnownGeogCS('WGS84')

    gdaltest.ct = osr.CoordinateTransformation(pm_srs, ll_srs)

    (x, y, z) = gdaltest.ct.TransformPoint(7000000, 7000000, 0)
    (exp_x, exp_y, exp_z) = (62.8820698884, 53.0918187696, 0.0)
    if (abs(exp_x - x) > 0.00001 or
        abs(exp_y - y) > 0.00001 or
            abs(exp_z - z) > 0.00001):
        print('Got:      (%f, %f, %f)' % (x, y, z))
        print('Expected: (%f, %f, %f)' % (exp_x, exp_y, exp_z))
        pytest.fail('Wrong LL for Pseudo Mercator result')

    pnt = ogr.CreateGeometryFromWkt('POINT(%g %g)' % (7000000, 7000000),
                                    pm_srs)
    expected_pnt = ogr.CreateGeometryFromWkt('POINT(%.10f %.10f)' % (exp_x, exp_y),
                                             ll_srs)
    result = pnt.Transform(gdaltest.ct)
    assert result == 0
    if (abs(expected_pnt.GetX() - pnt.GetX()) > 0.00001 or
        abs(expected_pnt.GetY() - pnt.GetY()) > 0.00001 or
            abs(expected_pnt.GetZ() - pnt.GetZ()) > 0.00001):
        print('Got:      %s' % pnt.ExportToWkt())
        print('Expected: %s' % expected_pnt.ExportToWkt())
        pytest.fail('Failed to transform from Pseudo Mercator to LL')

    
###############################################################################
# Test WebMercator -> WGS84 optimized transform


def test_osr_ct_8():

    if gdaltest.have_proj4 == 0:
        pytest.skip()

    src_srs = osr.SpatialReference()
    src_srs.ImportFromEPSG(3857)

    dst_srs = osr.SpatialReference()
    dst_srs.SetWellKnownGeogCS('WGS84')

    ct = osr.CoordinateTransformation(src_srs, dst_srs)

    pnts = [(0, 6274861.39400658), (1, 6274861.39400658)]
    result = ct.TransformPoints(pnts)
    expected_result = [(0.0, 49.000000000000007, 0.0), (8.9831528411952125e-06, 49.000000000000007, 0.0)]

    for i in range(2):
        for j in range(3):
            if abs(result[i][j] - expected_result[i][j]) > 1e-10:
                print('Got:      %s' % str(result))
                print('Expected: %s' % str(expected_result))
                pytest.fail('Failed to transform from Pseudo Mercator to LL')

    pnts = [(0, 6274861.39400658), (1 + 0, 1 + 6274861.39400658)]
    result = ct.TransformPoints(pnts)
    expected_result = [(0.0, 49.000000000000007, 0.0), (8.9831528411952125e-06, 49.000005893478189, 0.0)]

    for i in range(2):
        for j in range(3):
            if abs(result[i][j] - expected_result[i][j]) > 1e-10:
                print('Got:      %s' % str(result))
                print('Expected: %s' % str(expected_result))
                pytest.fail('Failed to transform from Pseudo Mercator to LL')


###############################################################################
# Test coordinate transformation where only one CRS has a towgs84 clause (#1156)


def test_osr_ct_towgs84_only_one_side():

    if gdaltest.have_proj4 == 0:
        pytest.skip()

    srs_towgs84 = osr.SpatialReference()
    srs_towgs84.SetFromUserInput("+proj=longlat +ellps=GRS80 +towgs84=100,200,300")

    srs_just_ellps = osr.SpatialReference()
    srs_just_ellps.SetFromUserInput('+proj=longlat +ellps=GRS80')

    ct = osr.CoordinateTransformation(srs_towgs84, srs_just_ellps)
    (x, y, z) = ct.TransformPoint(0, 0, 0)
    assert x == 0
    assert y == 0
    assert z == 0

    ct = osr.CoordinateTransformation(srs_just_ellps, srs_towgs84)
    (x, y, z) = ct.TransformPoint(0, 0, 0)
    assert x == 0
    assert y == 0
    assert z == 0


###############################################################################
# Test coordinate transformation where both side have towgs84/datum clause (#1156)


def test_osr_ct_towgs84_both_side():

    if gdaltest.have_proj4 == 0:
        pytest.skip()

    srs_towgs84 = osr.SpatialReference()
    srs_towgs84.SetFromUserInput("+proj=longlat +ellps=GRS80 +towgs84=100,200,300")

    srs_other_towgs84 = osr.SpatialReference()
    srs_other_towgs84.SetFromUserInput("+proj=longlat +ellps=GRS80 +towgs84=0,0,0")

    ct = osr.CoordinateTransformation(srs_towgs84, srs_other_towgs84)
    (x, y, z) = ct.TransformPoint(0, 0, 0)
    assert x != 0
    assert y != 0
    assert z != 0

    srs_datum_wgs84 = osr.SpatialReference()
    srs_datum_wgs84.SetFromUserInput("+proj=longlat +datum=WGS84")

    ct = osr.CoordinateTransformation(srs_towgs84, srs_datum_wgs84)
    (x, y, z) = ct.TransformPoint(0, 0, 0)
    assert x != 0
    assert y != 0
    assert z != 0

    ct = osr.CoordinateTransformation(srs_datum_wgs84, srs_towgs84)
    (x, y, z) = ct.TransformPoint(0, 0, 0)
    assert x != 0
    assert y != 0
    assert z != 0


###############################################################################
# Cleanup


def test_osr_ct_cleanup():

    if gdaltest.have_proj4 == 0:
        pytest.skip()

    gdaltest.ct = None



