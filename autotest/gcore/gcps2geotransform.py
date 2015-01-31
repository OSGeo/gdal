#!/usr/bin/env python
# -*- coding: utf-8 -*-
###############################################################################
# $Id: misc.py 26369 2013-08-25 19:48:28Z goatbar $
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test the GDALGCPsToGeoTransform() method.
# Author:   Frank Warmerdam <warmerdam@pobox.com>
# 
###############################################################################
# Copyright (c) 2013 Frank Warmerdam
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
import sys

sys.path.append( '../pymod' )

import gdaltest

###############################################################################
# Helper to make gcps
def _list2gcps(src_list):
    gcp_list = []
    for src_tuple in src_list:
        gcp = gdal.GCP()
        gcp.GCPPixel = src_tuple[0]
        gcp.GCPLine = src_tuple[1]
        gcp.GCPX = src_tuple[2]
        gcp.GCPY = src_tuple[3]
        gcp_list.append(gcp)
    return gcp_list


###############################################################################
# Test simple exact case of turning GCPs into a GeoTransform.

def gcps2gt_1():

    gt = gdal.GCPsToGeoTransform(_list2gcps([
                (  0.0,   0.0, 400000, 370000),
                (100.0,   0.0, 410000, 370000),
                (100.0, 200.0, 410000, 368000)
                ]))
    if not gdaltest.geotransform_equals(
        gt, (400000.0, 100.0, 0.0, 370000.0, 0.0, -10.0), 0.000001):
        return 'failure'

    return 'success'

###############################################################################
# Similar but non-exact.

def gcps2gt_2():

    gt = gdal.GCPsToGeoTransform(_list2gcps([
                (  0.0,   0.0, 400000, 370000),
                (100.0,   0.0, 410000, 370000),
                (100.0, 200.0, 410000, 368000),
                (  0.0, 200.0, 400000, 368000.01)
                ]))
    if not gdaltest.geotransform_equals(
        gt, (400000.0, 100.0, 0.0, 370000.0025, -5e-05, -9.999975), 0.000001):
        return 'failure'

    return 'success'

###############################################################################
# bApproxOK false, and no good solution.

def gcps2gt_3():

    approx_ok = 0
    gt = gdal.GCPsToGeoTransform(_list2gcps([
                (  0.0,   0.0, 400000, 370000),
                (100.0,   0.0, 410000, 370000),
                (100.0, 200.0, 410000, 368000),
                (  0.0, 200.0, 400000, 360000)
                ]), approx_ok)
    if gt is not None:
        gdaltest.post_reason('Expected failure when no good solution.')
        return 'failure'

    return 'success'

###############################################################################
# Single point - Should return None.

def gcps2gt_4():

    gt = gdal.GCPsToGeoTransform(_list2gcps([
                (  0.0,   0.0, 400000, 370000),
                ]))
    if gt is not None:
        gdaltest.post_reason('Expected failure for single GCP.')
        return 'failure'

    return 'success'

###############################################################################
# Two points - simple offset and scale, no rotation.

def gcps2gt_5():

    gt = gdal.GCPsToGeoTransform(_list2gcps([
                (  0.0,   0.0, 400000, 370000),
                (100.0, 200.0, 410000, 368000),
                ]))
    if not gdaltest.geotransform_equals(
        gt, (400000.0, 100.0, 0.0, 370000.0, 0.0, -10.0), 0.000001):
        return 'failure'

    return 'success'

###############################################################################
# Special case for four points in a particular order.  Exact result.

def gcps2gt_6():

    gt = gdal.GCPsToGeoTransform(_list2gcps([
                (400000, 370000, 400000, 370000),
                (410000, 370000, 410000, 370000),
                (410000, 368000, 410000, 368000),
                (400000, 368000, 400000, 368000),
                ]))
    if not gdaltest.geotransform_equals(
        gt, (0.0, 1.0, 0.0, 0.0, 0.0, 1.0), 0.000001):
        return 'failure'

    return 'success'

###############################################################################
# Try a case that is hard to do without normalization.

def gcps2gt_7():

    gt = gdal.GCPsToGeoTransform(_list2gcps([
                (400000, 370000, 400000, 370000),
                (410000, 368000, 410000, 368000),
                (410000, 370000, 410000, 370000),
                (400000, 368000, 400000, 368000),
                ]))
    if not gdaltest.geotransform_equals(
        gt, (0.0, 1.0, 0.0, 0.0, 0.0, 1.0), 0.000001):
        return 'failure'

    return 'success'

###############################################################################
# A fairly messy real world case without a easy to predict result.

def gcps2gt_8():

    gt = gdal.GCPsToGeoTransform(_list2gcps([
                (0.01,    0.04, -87.05528672907, 39.22759504228),
                (0.01, 2688.02, -86.97079900719, 39.27075713986),
                (4031.99, 2688.04, -87.05960736744, 39.37569137000),
                (1988.16, 1540.80, -87.055069186699924, 39.304963106777514),
                (1477.41, 2400.83, -87.013419295885001, 39.304705030894979),
                (1466.02, 2376.92, -87.013906298363295, 39.304056190007913),
                ]))
    gt_expected = (-87.056612873288, -2.232795668658e-05, 3.178617809303e-05, 
                    39.227856615716, 2.6091510188921e-05, 1.596921026218e-05)
    if not gdaltest.geotransform_equals(gt, gt_expected, 0.00001):
        return 'failure'

    return 'success'

    
gdaltest_list = [ 
    gcps2gt_1,
    gcps2gt_2,
    gcps2gt_3,
    gcps2gt_4,
    gcps2gt_5,
    gcps2gt_6,
    gcps2gt_7,
    gcps2gt_8,
]

if __name__ == '__main__':

    gdaltest.setup_run( 'gcps2geotransform' )

    gdaltest.run_tests( gdaltest_list )

    gdaltest.summarize()

