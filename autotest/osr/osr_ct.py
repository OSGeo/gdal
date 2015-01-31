#!/usr/bin/env python
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

import sys

sys.path.append( '../pymod' )

from osgeo import gdal
import gdaltest
from osgeo import osr
from osgeo import ogr


###############################################################################
# Verify that we have PROJ.4 available.

def osr_ct_1():


    gdaltest.have_proj4 = 0

    utm_srs = osr.SpatialReference()
    utm_srs.SetUTM( 11 )
    utm_srs.SetWellKnownGeogCS( 'WGS84' )

    ll_srs = osr.SpatialReference()
    ll_srs.SetWellKnownGeogCS( 'WGS84' )

    try:
        gdal.PushErrorHandler( 'CPLQuietErrorHandler' )
        ct = osr.CoordinateTransformation( ll_srs, utm_srs )
        gdal.PopErrorHandler()
        if gdal.GetLastErrorMsg().find('Unable to load PROJ.4') != -1:
            gdaltest.post_reason( 'PROJ.4 missing, transforms not available.' )
            return 'skip'
    except ValueError:
        gdal.PopErrorHandler()
        if gdal.GetLastErrorMsg().find('Unable to load PROJ.4') != -1:
            gdaltest.post_reason( 'PROJ.4 missing, transforms not available.' )
            return 'skip'
        else:
            gdaltest.post_reason( gdal.GetLastErrorMsg() )
            return 'fail'

    if ct is None or ct.this is None:
        gdaltest.post_reason( 'Unable to create simple CoordinateTransformat.')
        return 'fail'

    gdaltest.have_proj4 = 1

    return 'success'

###############################################################################
# Actually perform a simple LL to UTM conversion.

def osr_ct_2():

    if gdaltest.have_proj4 == 0:
        return 'skip'

    utm_srs = osr.SpatialReference()
    utm_srs.SetUTM( 11 )
    utm_srs.SetWellKnownGeogCS( 'WGS84' )

    ll_srs = osr.SpatialReference()
    ll_srs.SetWellKnownGeogCS( 'WGS84' )

    gdaltest.ct = osr.CoordinateTransformation( ll_srs, utm_srs )

    result = gdaltest.ct.TransformPoint( -117.5, 32.0, 0.0 )
    if abs(result[0] - 452772.06) > 0.01 \
       or abs(result[1] - 3540544.89 ) > 0.01 \
       or abs(result[2] - 0.0) > 0.01:
        gdaltest.post_reason( 'Wrong LL to UTM result' )
        return 'fail'
    else:
        return 'success'

###############################################################################
# Transform an OGR geometry ... this is mostly aimed at ensuring that
# the OGRCoordinateTransformation target SRS isn't deleted till the output
# geometry which also uses it is deleted.

def osr_ct_3():

    if gdaltest.have_proj4 == 0:
        return 'skip'

    utm_srs = osr.SpatialReference()
    utm_srs.SetUTM( 11 )
    utm_srs.SetWellKnownGeogCS( 'WGS84' )

    ll_srs = osr.SpatialReference()
    ll_srs.SetWellKnownGeogCS( 'WGS84' )

    ct = osr.CoordinateTransformation( ll_srs, utm_srs )

    pnt = ogr.CreateGeometryFromWkt( 'POINT(-117.5 32.0)', ll_srs )
    result = pnt.Transform( ct )
    if result != 0:
        return 'fail'

    ll_srs = None
    ct = None
    utm_srs = None

    out_srs = pnt.GetSpatialReference().ExportToPrettyWkt()
    if out_srs[0:6] != 'PROJCS':
        gdaltest.post_reason( 'output srs corrupt, ref counting issue?' )
        return 'fail'

    pnt = None

    return 'success'

###############################################################################
# Actually perform a simple LL to UTM conversion.
# Works for both OG and NG bindings

def osr_ct_4():

    if gdaltest.have_proj4 == 0:
        return 'skip'

    utm_srs = osr.SpatialReference()
    utm_srs.SetUTM( 11 )
    utm_srs.SetWellKnownGeogCS( 'WGS84' )

    ll_srs = osr.SpatialReference()
    ll_srs.SetWellKnownGeogCS( 'WGS84' )

    gdaltest.ct = osr.CoordinateTransformation( ll_srs, utm_srs )

    result = gdaltest.ct.TransformPoints(  [ (-117.5, 32.0, 0.0), (-117.5, 32.0) ] )

    for i in range(2):
        if abs(result[i][0] - 452772.06) > 0.01 \
        or abs(result[i][1] - 3540544.89 ) > 0.01 \
        or abs(result[i][2] - 0.0) > 0.01:
            gdaltest.post_reason( 'Wrong LL to UTM result' )
            return 'fail'

    return 'success'

###############################################################################
# Same test, but with any sequence of tuples instead of a tuple of tuple
# New in NG bindings (#3020)

def osr_ct_5():

    if gdaltest.have_proj4 == 0:
        return 'skip'

    utm_srs = osr.SpatialReference()
    utm_srs.SetUTM( 11 )
    utm_srs.SetWellKnownGeogCS( 'WGS84' )

    ll_srs = osr.SpatialReference()
    ll_srs.SetWellKnownGeogCS( 'WGS84' )

    gdaltest.ct = osr.CoordinateTransformation( ll_srs, utm_srs )

    result = gdaltest.ct.TransformPoints(  ( (-117.5, 32.0, 0.0), (-117.5, 32.0) ) )

    for i in range(2):
        if abs(result[i][0] - 452772.06) > 0.01 \
        or abs(result[i][1] - 3540544.89 ) > 0.01 \
        or abs(result[i][2] - 0.0) > 0.01:
            gdaltest.post_reason( 'Wrong LL to UTM result' )
            return 'fail'

    return 'success'

###############################################################################
# Test osr.CreateCoordinateTransformation() method

def osr_ct_6():

    if gdaltest.have_proj4 == 0:
        return 'skip'

    ct = osr.CreateCoordinateTransformation( None, None )
    if ct is not None:
        return 'fail'

    utm_srs = osr.SpatialReference()
    utm_srs.SetUTM( 11 )
    utm_srs.SetWellKnownGeogCS( 'WGS84' )

    ll_srs = osr.SpatialReference()
    ll_srs.SetWellKnownGeogCS( 'WGS84' )

    ct = osr.CreateCoordinateTransformation( ll_srs, utm_srs )
    if ct is None:
        return 'fail'

    result = ct.TransformPoints(  ( (-117.5, 32.0, 0.0), (-117.5, 32.0) ) )

    for i in range(2):
        if abs(result[i][0] - 452772.06) > 0.01 \
        or abs(result[i][1] - 3540544.89 ) > 0.01 \
        or abs(result[i][2] - 0.0) > 0.01:
            gdaltest.post_reason( 'Wrong LL to UTM result' )
            return 'fail'

    return 'success'

###############################################################################
# Actually perform a simple Pseudo Mercator to LL conversion.

def osr_ct_7():

    if gdaltest.have_proj4 == 0:
        return 'skip'

    pm_srs = osr.SpatialReference()
    pm_srs.ImportFromEPSG( 3857 )

    ll_srs = osr.SpatialReference()
    ll_srs.SetWellKnownGeogCS( 'WGS84' )

    gdaltest.ct = osr.CoordinateTransformation( pm_srs, ll_srs )

    (x, y, z) = gdaltest.ct.TransformPoint( 7000000, 7000000, 0 )
    (exp_x, exp_y, exp_z) = (62.8820698884, 53.0918187696, 0.0)
    if (abs(exp_x - x) > 0.00001 or
        abs(exp_y - y) > 0.00001 or
        abs(exp_z - z) > 0.00001):
        gdaltest.post_reason( 'Wrong LL for Pseudo Mercator result' )
        print('Got:      (%f, %f, %f)' % (x, y, z))
        print('Expected: (%f, %f, %f)' % (exp_x, exp_y, exp_z))
        return 'fail'

    pnt = ogr.CreateGeometryFromWkt( 'POINT(%g %g)' % (7000000, 7000000),
                                     pm_srs )
    expected_pnt = ogr.CreateGeometryFromWkt( 'POINT(%.10f %.10f)' % (exp_x, exp_y),
                                     ll_srs )
    result = pnt.Transform( gdaltest.ct )
    if result != 0:
        return 'fail'
    if (abs(expected_pnt.GetX() - pnt.GetX()) > 0.00001 or
        abs(expected_pnt.GetY() - pnt.GetY()) > 0.00001 or
        abs(expected_pnt.GetZ() - pnt.GetZ()) > 0.00001):
        gdaltest.post_reason( 'Failed to transform from Pseudo Mercator to LL')
        print('Got:      %s' % pnt.ExportToWkt())
        print('Expected: %s' % expected_pnt.ExportToWkt())
        return 'fail'

    return 'success'

###############################################################################
# Cleanup

def osr_ct_cleanup():

    if gdaltest.have_proj4 == 0:
        return 'skip'

    gdaltest.ct = None

    return 'success'

gdaltest_list = [
    osr_ct_1,
    osr_ct_2,
    osr_ct_3,
    osr_ct_4,
    osr_ct_5,
    osr_ct_6,
    osr_ct_7,
    osr_ct_cleanup,
    None ]

if __name__ == '__main__':

    gdaltest.setup_run( 'osr_ct' )

    gdaltest.run_tests( gdaltest_list )

    gdaltest.summarize()
