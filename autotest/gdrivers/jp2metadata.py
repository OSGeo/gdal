#!/usr/bin/env python
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test JP2 metadata support.
# Author:   Even Rouault < even dot rouault @ mines-paris dot org >
# 
###############################################################################
# Copyright (c) 2013, Even Rouault < even dot rouault @ mines-paris dot org >
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

import os
import sys
from osgeo import gdal

sys.path.append( '../pymod' )

import gdaltest


###############################################################################
# Test bugfix for #5249 (Irrelevant ERDAS GeoTIFF JP2Box read)

def jp2metadata_1():

    ds = gdal.Open('data/erdas_foo.jp2')
    if ds is None:
        return 'skip'

    wkt = ds.GetProjectionRef()
    gt = ds.GetGeoTransform()
    if wkt.find('PROJCS["ETRS89') != 0:
        print(wkt)
        return 'fail'
    expected_gt = (356000.0, 0.5, 0.0, 7596000.0, 0.0, -0.5)
    for i in range(6):
        if abs(gt[i] - expected_gt[i]) > 1e-5:
            print(gt)
            return 'fail'
    return 'success'


gdaltest_list = [
    jp2metadata_1,
    ]


if __name__ == '__main__':

    gdaltest.setup_run( 'jp2metadata' )

    gdaltest.run_tests( gdaltest_list )

    gdaltest.summarize()

