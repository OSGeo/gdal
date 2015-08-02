#!/usr/bin/env python
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test read functionality for OGR IDF driver.
# Author:   Even Rouault <even.rouault at spatialys.com>
# 
###############################################################################
# Copyright (c) 2015, Even Rouault <even.rouault at spatialys.com>
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

import gdaltest
from osgeo import ogr

###############################################################################
# Basic test

def ogr_idf_1():

    ds = ogr.Open('data/test.idf')
    lyr = ds.GetLayer(0)
    f = lyr.GetNextFeature()
    if f['NODE_ID'] != 1 or f['foo'] != 'U' or f.GetGeometryRef().ExportToWkt() != 'POINT (2 49)':
        gdaltest.post_reason('fail')
        f.DumpReadable()
        return 'fail'

    lyr = ds.GetLayer(1)
    f = lyr.GetNextFeature()
    if f.GetGeometryRef().ExportToWkt() != 'LINESTRING (2 49,2.5 49.5,3 50)':
        gdaltest.post_reason('fail')
        f.DumpReadable()
        return 'fail'

    lyr = ds.GetLayer(2)
    f = lyr.GetNextFeature()
    if f.GetGeometryRef().ExportToWkt() != 'POINT (2.5 49.5)':
        gdaltest.post_reason('fail')
        f.DumpReadable()
        return 'fail'

    lyr = ds.GetLayer(3)
    f = lyr.GetNextFeature()
    if f['FOO'] != 1:
        gdaltest.post_reason('fail')
        f.DumpReadable()
        return 'fail'

    return 'success'

###############################################################################
# Run test_ogrsf

def ogr_idf_2():

    import test_cli_utilities
    if test_cli_utilities.get_test_ogrsf_path() is None:
        return 'skip'

    ret = gdaltest.runexternal(test_cli_utilities.get_test_ogrsf_path() + ' -ro data/test.idf')

    if ret.find('INFO') == -1 or ret.find('ERROR') != -1:
        print(ret)
        return 'fail'

    return 'success'

gdaltest_list = [ 
    ogr_idf_1,
    ogr_idf_2 ]


if __name__ == '__main__':

    gdaltest.setup_run( 'ogr_idf' )

    gdaltest.run_tests( gdaltest_list )

    gdaltest.summarize()

